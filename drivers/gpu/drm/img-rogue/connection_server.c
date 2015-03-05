/*************************************************************************/ /*!
@File
@Title          Server side connection management
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@Description    Handles connections coming from the client and the management
                connection based information
@License        Dual MIT/GPLv2

The contents of this file are subject to the MIT license as set out below.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

Alternatively, the contents of this file may be used under the terms of
the GNU General Public License Version 2 ("GPL") in which case the provisions
of GPL are applicable instead of those above.

If you wish to allow use of your version of this file only under the terms of
GPL, and not to allow others to use your version of this file under the terms
of the MIT license, indicate your decision by deleting the provisions above
and replace them with the notice and other provisions required by GPL as set
out in the file called "GPL-COPYING" included in this distribution. If you do
not delete the provisions above, a recipient may use your version of this file
under the terms of either the MIT license or GPL.

This License is also included in this distribution in the file called
"MIT-COPYING".

EXCEPT AS OTHERWISE STATED IN A NEGOTIATED AGREEMENT: (A) THE SOFTWARE IS
PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
PURPOSE AND NONINFRINGEMENT; AND (B) IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/ /**************************************************************************/

#include "handle.h"
#include "pvrsrv.h"
#include "connection_server.h"
#include "osconnection_server.h"
#include "allocmem.h"
#include "pvr_debug.h"
#include "sync_server.h"
#include "process_stats.h"
#include "pdump_km.h"
#include "lists.h"

/* The bridge lock should be held while manipulating this list */
CONNECTION_DATA *g_psDeferConnectionDataList = NULL;
/* Variable used to hold in memory the timeout for the current time slice*/
IMG_UINT64 gui64TimesliceLimit;
/* Counter number of handle data freed during the current time slice */
IMG_UINT32 gui32HandleDataFreeCounter = 0;

static IMPLEMENT_LIST_INSERT(CONNECTION_DATA)
static IMPLEMENT_LIST_REMOVE(CONNECTION_DATA)
static IMPLEMENT_LIST_FOR_EACH_SAFE(CONNECTION_DATA)

/* PID associated with Connection currently being purged by Cleanup thread */
static IMG_PID gCurrentPurgeConnectionPid = 0;

static PVRSRV_ERROR ConnectionDataDestroy(CONNECTION_DATA *psConnection)
{
	PVRSRV_ERROR eError;

	if (psConnection == NULL)
	{
		PVR_DPF((PVR_DBG_ERROR, "ConnectionDestroy: Missing connection!"));
		PVR_ASSERT(0);
		return PVRSRV_ERROR_INVALID_PARAMS;
	}

	/* Close the process statistics */
#if defined(PVRSRV_ENABLE_PROCESS_STATS) && !defined(PVRSRV_DEBUG_LINUX_MEMORY_STATS)
	if (psConnection->hProcessStats != NULL)
	{
		PVRSRVStatsDeregisterProcess(psConnection->hProcessStats);
		psConnection->hProcessStats = NULL;
	}
#endif

	/* Free handle base for this connection */
	if (psConnection->psHandleBase != NULL)
	{
		eError = PVRSRVFreeHandleBase(psConnection->psHandleBase);
		if (eError != PVRSRV_OK)
		{
			if (eError != PVRSRV_ERROR_RETRY)
			{
				PVR_DPF((PVR_DBG_ERROR,
					 "ConnectionDataDestroy: Couldn't free handle base for connection (%d)",
					 eError));
			}

			return eError;
		}

		psConnection->psHandleBase = NULL;
	}

	if (psConnection->psSyncConnectionData != NULL)
	{
		SyncUnregisterConnection(psConnection->psSyncConnectionData);
		psConnection->psSyncConnectionData = NULL;
	}

	if (psConnection->psPDumpConnectionData != NULL)
	{
		PDumpUnregisterConnection(psConnection->psPDumpConnectionData);
		psConnection->psPDumpConnectionData = NULL;
	}

	/* Call environment specific connection data deinit function */
	if (psConnection->hOsPrivateData != NULL)
	{
		eError = OSConnectionPrivateDataDeInit(psConnection->hOsPrivateData);
		if (eError != PVRSRV_OK)
		{
			PVR_DPF((PVR_DBG_ERROR,
				 "PVRSRVConnectionDataDestroy: OSConnectionPrivateDataDeInit failed (%d)",
				 eError));

			return eError;
		}

		psConnection->hOsPrivateData = NULL;
	}

	OSFreeMem(psConnection);

	return PVRSRV_OK;
}

PVRSRV_ERROR PVRSRVConnectionConnect(void **ppvPrivData, void *pvOSData)
{
	CONNECTION_DATA *psConnection;
	PVRSRV_ERROR eError;

	/* Allocate connection data area */
	psConnection = OSAllocZMem(sizeof(*psConnection));
	if (psConnection == NULL)
	{
		PVR_DPF((PVR_DBG_ERROR,
			 "PVRSRVConnectionConnect: Couldn't allocate connection data"));
		return PVRSRV_ERROR_OUT_OF_MEMORY;
	}

	/* Call environment specific connection data init function */
	eError = OSConnectionPrivateDataInit(&psConnection->hOsPrivateData, pvOSData);
	if (eError != PVRSRV_OK)
	{
		 PVR_DPF((PVR_DBG_ERROR,
			  "PVRSRVConnectionConnect: OSConnectionPrivateDataInit failed (%d)",
			  eError));
		goto failure;
	}

	psConnection->pid = OSGetCurrentClientProcessIDKM();

	/* Register this connection with the sync core */
	eError = SyncRegisterConnection(&psConnection->psSyncConnectionData);
	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR,
			 "PVRSRVConnectionConnect: Couldn't register the sync data"));
		goto failure;
	}

	/*
	 * Register this connection with the pdump core. Pass in the sync connection data
	 * as it will be needed later when we only get passed in the PDump connection data.
	 */
	eError = PDumpRegisterConnection(psConnection->psSyncConnectionData,
					 &psConnection->psPDumpConnectionData);
	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR,
			 "PVRSRVConnectionConnect: Couldn't register the PDump data"));
		goto failure;
	}

	/* Allocate handle base for this connection */
	eError = PVRSRVAllocHandleBase(&psConnection->psHandleBase);
	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR,
			 "PVRSRVConnectionConnect: Couldn't allocate handle base for connection (%d)",
			 eError));
		goto failure;
	}

	/* Allocate process statistics */
#if defined(PVRSRV_ENABLE_PROCESS_STATS) && !defined(PVRSRV_DEBUG_LINUX_MEMORY_STATS)
	eError = PVRSRVStatsRegisterProcess(&psConnection->hProcessStats);
	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR,
			 "PVRSRVConnectionConnect: Couldn't register process statistics (%d)",
			 eError));
		goto failure;
	}
#endif

	*ppvPrivData = psConnection;

	return eError;

failure:
	ConnectionDataDestroy(psConnection);

	return eError;
}

void PVRSRVConnectionDisconnect(void *pvDataPtr)
{
	PVRSRV_DATA *psPVRSRVData = PVRSRVGetPVRSRVData();
	CONNECTION_DATA *psConnectionData = pvDataPtr;
	PVRSRV_ERROR eError;

	/* Defer always the release of the connection data */
	List_CONNECTION_DATA_Insert(&g_psDeferConnectionDataList, psConnectionData);

	/* Now signal clean up thread */
	eError = OSEventObjectSignal(psPVRSRVData->hCleanupEventObject);
	PVR_LOG_IF_ERROR(eError, "OSEventObjectSignal");

}

static void PurgeConnectionData(CONNECTION_DATA *psConnectionData)
{
	PVRSRV_ERROR eError;

	gCurrentPurgeConnectionPid = psConnectionData->pid;

	List_CONNECTION_DATA_Remove(psConnectionData);

	eError = ConnectionDataDestroy(psConnectionData);
	if (eError != PVRSRV_OK)
	{
		if (eError == PVRSRV_ERROR_RETRY)
		{
			PVR_DPF((PVR_DBG_MESSAGE,
				 "PurgeConnectionData: Failed to purge connection data %p "
				 "(deferring destruction)",
				 psConnectionData));

			List_CONNECTION_DATA_Insert(&g_psDeferConnectionDataList, psConnectionData);
		}
	}
	else
	{
		PVR_DPF((PVR_DBG_MESSAGE,
			 "PurgeConnectionData: Connection data %p deferred destruction finished",
			 psConnectionData));
	}

	/* Check if possible resize the global handle base */
	eError = PVRSRVPurgeHandles(KERNEL_HANDLE_BASE);
	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR,
			 "PVRSRVConnectionDisconnect: Purge of global handle pool failed (%d)",
			 eError));
	}

	gCurrentPurgeConnectionPid = 0;

}

IMG_BOOL PVRSRVConnectionPurgeDeferred(IMG_UINT64 ui64TimeSlice)
{
	/* Set the time slice limit time */
	gui64TimesliceLimit = ui64TimeSlice;
	PVR_DPF((PVR_DBG_MESSAGE, "PVRSRVConnectionPurgeDeferred: set new time slice limit to %llu",
	         gui64TimesliceLimit));

	/* Reset the counter for the handle data freed during the current time
	 * slice */
	gui32HandleDataFreeCounter = 0;

	List_CONNECTION_DATA_ForEachSafe(g_psDeferConnectionDataList, PurgeConnectionData);

	PVR_DPF_RETURN_VAL((g_psDeferConnectionDataList != NULL) ? IMG_FALSE : IMG_TRUE);
}

PVRSRV_ERROR PVRSRVConnectionInit(void)
{
	return PVRSRV_OK;
}

PVRSRV_ERROR PVRSRVConnectionDeInit(void)
{
	if (g_psDeferConnectionDataList != NULL)
	{
		/* Useful to know how many connection data items are waiting at this point... */
		CONNECTION_DATA *psConnectionData = g_psDeferConnectionDataList;
		IMG_UINT32 uiDeferredCount = 0;

		while (psConnectionData != NULL)
		{
			psConnectionData = psConnectionData->psNext;
			uiDeferredCount++;
		}

		PVR_DPF((PVR_DBG_WARNING,
			 "PVRSRVConnectionDeInit: %d connection data(s) waiting to be destroyed!",
			 uiDeferredCount));

		/* Attempt to free more resources... */
		LOOP_UNTIL_TIMEOUT(MAX_HW_TIME_US)
		{
			/* Set it with 0 to avoid the release of the global lock during the
			 * deInit phase*/
			(void)PVRSRVConnectionPurgeDeferred(0);

			/* If the driver is not in a okay state then don't try again... */
			if (PVRSRVGetPVRSRVData()->eServicesState != PVRSRV_SERVICES_STATE_OK)
			{
				break;
			}

			/* Break out if we have freed them all... */
			if (g_psDeferConnectionDataList == NULL)
			{
				break;
			}

			OSSleepms((MAX_HW_TIME_US / 1000) / 10);
		} END_LOOP_UNTIL_TIMEOUT();

		/* Once more for luck and then force the issue... */
		(void)PVRSRVConnectionPurgeDeferred(0);

		if (g_psDeferConnectionDataList != NULL)
		{
			psConnectionData = g_psDeferConnectionDataList;
			uiDeferredCount = 0;

			while (psConnectionData != NULL)
			{
				PVR_DPF((PVR_DBG_ERROR,
					 "PVRSRVConnectionDeInit: "
					 "Connection data %p has not been freed!",
					 psConnectionData));

				psConnectionData = psConnectionData->psNext;
				uiDeferredCount++;
			}

			PVR_DPF((PVR_DBG_ERROR,
				 "PVRSRVConnectionDeInit: "
				 "%d connection data(s) still waiting!",
				 uiDeferredCount));

			PVR_ASSERT(0);

			return PVRSRV_ERROR_PROCESSING_BLOCKED;
		}
	}

	return PVRSRV_OK;
}

IMG_PID PVRSRVGetPurgeConnectionPid(void)
{
	return gCurrentPurgeConnectionPid;
}
