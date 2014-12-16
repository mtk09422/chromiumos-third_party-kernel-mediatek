/*************************************************************************/ /*!
@File
@Title          Server side connection management
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@Description    Handles connections coming from the client and the management
                connection based information
@License        Strictly Confidential.
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
CONNECTION_DATA *g_psDeferConnectionDataList = IMG_NULL;
/* Variable used to hold in memory the timeout for the current time slice*/
IMG_UINT64 gui64TimesliceLimit;
/* Counter number of handle data freed during the current time slice */
IMG_UINT32 gui32HandleDataFreeCounter = 0;

static IMPLEMENT_LIST_INSERT(CONNECTION_DATA)
static IMPLEMENT_LIST_REMOVE(CONNECTION_DATA)
static IMPLEMENT_LIST_FOR_EACH_SAFE(CONNECTION_DATA)


static PVRSRV_ERROR ConnectionDataDestroy(CONNECTION_DATA *psConnection)
{
	PVRSRV_ERROR eError;

	if (psConnection == IMG_NULL)
	{
		PVR_DPF((PVR_DBG_ERROR, "ConnectionDestroy: Missing connection!"));
		PVR_ASSERT(0);
		return PVRSRV_ERROR_INVALID_PARAMS;
	}

	/* Close the process statistics */
#if defined(PVRSRV_ENABLE_PROCESS_STATS)
	if (psConnection->hProcessStats != IMG_NULL)
	{
		PVRSRVStatsDeregisterProcess(psConnection->hProcessStats);
		psConnection->hProcessStats = IMG_NULL;
	}
#endif

	/* Free handle base for this connection */
	if (psConnection->psHandleBase != IMG_NULL)
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

		psConnection->psHandleBase = IMG_NULL;
	}

	if (psConnection->psSyncConnectionData != IMG_NULL)
	{
		SyncUnregisterConnection(psConnection->psSyncConnectionData);
		psConnection->psSyncConnectionData = IMG_NULL;
	}

	if (psConnection->psPDumpConnectionData != IMG_NULL)
	{
		PDumpUnregisterConnection(psConnection->psPDumpConnectionData);
		psConnection->psPDumpConnectionData = IMG_NULL;
	}

	/* Call environment specific connection data deinit function */
	if (psConnection->hOsPrivateData != IMG_NULL)
	{
		eError = OSConnectionPrivateDataDeInit(psConnection->hOsPrivateData);
		if (eError != PVRSRV_OK)
		{
			PVR_DPF((PVR_DBG_ERROR,
				 "PVRSRVConnectionDataDestroy: OSConnectionPrivateDataDeInit failed (%d)",
				 eError));

			return eError;
		}

		psConnection->hOsPrivateData = IMG_NULL;
	}

	OSFreeMem(psConnection);

	return PVRSRV_OK;
}

PVRSRV_ERROR PVRSRVConnectionConnect(IMG_PVOID *ppvPrivData, IMG_PVOID pvOSData)
{
	CONNECTION_DATA *psConnection;
	PVRSRV_ERROR eError;

	/* Allocate connection data area */
	psConnection = OSAllocZMem(sizeof(*psConnection));
	if (psConnection == IMG_NULL)
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
#if defined(PVRSRV_ENABLE_PROCESS_STATS)
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

void PVRSRVConnectionDisconnect(IMG_PVOID pvDataPtr)
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

	PVR_DPF_RETURN_VAL((g_psDeferConnectionDataList != IMG_NULL) ? IMG_FALSE : IMG_TRUE);
}

PVRSRV_ERROR PVRSRVConnectionInit(void)
{
	return PVRSRV_OK;
}

PVRSRV_ERROR PVRSRVConnectionDeInit(void)
{
	if (g_psDeferConnectionDataList != IMG_NULL)
	{
		/* Useful to know how many connection data items are waiting at this point... */
		CONNECTION_DATA *psConnectionData = g_psDeferConnectionDataList;
		IMG_UINT32 uiDeferredCount = 0;

		while (psConnectionData != IMG_NULL)
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
			if (g_psDeferConnectionDataList == IMG_NULL)
			{
				break;
			}

			OSSleepms((MAX_HW_TIME_US / 1000) / 10);
		} END_LOOP_UNTIL_TIMEOUT();

		/* Once more for luck and then force the issue... */
		(void)PVRSRVConnectionPurgeDeferred(0);

		if (g_psDeferConnectionDataList != IMG_NULL)
		{
			psConnectionData = g_psDeferConnectionDataList;
			uiDeferredCount = 0;

			while (psConnectionData != IMG_NULL)
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
