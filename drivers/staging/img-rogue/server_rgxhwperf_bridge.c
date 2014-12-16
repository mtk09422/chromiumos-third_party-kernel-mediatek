/*************************************************************************/ /*!
@File
@Title          Server bridge for rgxhwperf
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@Description    Implements the server side of the bridge for rgxhwperf
@License        Strictly Confidential.
*/ /**************************************************************************/

#include <stddef.h>
#include <asm/uaccess.h>

#include "img_defs.h"

#include "rgxhwperf.h"


#include "common_rgxhwperf_bridge.h"

#include "allocmem.h"
#include "pvr_debug.h"
#include "connection_server.h"
#include "pvr_bridge.h"
#include "rgx_bridge.h"
#include "srvcore.h"
#include "handle.h"

#if defined (SUPPORT_AUTH)
#include "osauth.h"
#endif

#include <linux/slab.h>




/* ***************************************************************************
 * Server-side bridge entry points
 */
 
static IMG_INT
PVRSRVBridgeRGXCtrlHWPerf(IMG_UINT32 ui32BridgeID,
					  PVRSRV_BRIDGE_IN_RGXCTRLHWPERF *psRGXCtrlHWPerfIN,
					  PVRSRV_BRIDGE_OUT_RGXCTRLHWPERF *psRGXCtrlHWPerfOUT,
					 CONNECTION_DATA *psConnection)
{
	IMG_HANDLE hDevNodeInt = IMG_NULL;

	PVRSRV_BRIDGE_ASSERT_CMD(ui32BridgeID, PVRSRV_BRIDGE_RGXHWPERF_RGXCTRLHWPERF);







				{
					/* Look up the address from the handle */
					psRGXCtrlHWPerfOUT->eError =
						PVRSRVLookupHandle(psConnection->psHandleBase,
											(IMG_VOID **) &hDevNodeInt,
											psRGXCtrlHWPerfIN->hDevNode,
											PVRSRV_HANDLE_TYPE_DEV_NODE);
					if(psRGXCtrlHWPerfOUT->eError != PVRSRV_OK)
					{
						goto RGXCtrlHWPerf_exit;
					}
				}


	psRGXCtrlHWPerfOUT->eError =
		PVRSRVRGXCtrlHWPerfKM(
					hDevNodeInt,
					psRGXCtrlHWPerfIN->bToggle,
					psRGXCtrlHWPerfIN->ui64Mask);




RGXCtrlHWPerf_exit:

	return 0;
}

static IMG_INT
PVRSRVBridgeRGXConfigEnableHWPerfCounters(IMG_UINT32 ui32BridgeID,
					  PVRSRV_BRIDGE_IN_RGXCONFIGENABLEHWPERFCOUNTERS *psRGXConfigEnableHWPerfCountersIN,
					  PVRSRV_BRIDGE_OUT_RGXCONFIGENABLEHWPERFCOUNTERS *psRGXConfigEnableHWPerfCountersOUT,
					 CONNECTION_DATA *psConnection)
{
	IMG_HANDLE hDevNodeInt = IMG_NULL;
	RGX_HWPERF_CONFIG_CNTBLK *psBlockConfigsInt = IMG_NULL;

	PVRSRV_BRIDGE_ASSERT_CMD(ui32BridgeID, PVRSRV_BRIDGE_RGXHWPERF_RGXCONFIGENABLEHWPERFCOUNTERS);




	if (psRGXConfigEnableHWPerfCountersIN->ui32ArrayLen != 0)
	{
		psBlockConfigsInt = OSAllocMem(psRGXConfigEnableHWPerfCountersIN->ui32ArrayLen * sizeof(RGX_HWPERF_CONFIG_CNTBLK));
		if (!psBlockConfigsInt)
		{
			psRGXConfigEnableHWPerfCountersOUT->eError = PVRSRV_ERROR_OUT_OF_MEMORY;
	
			goto RGXConfigEnableHWPerfCounters_exit;
		}
	}

			/* Copy the data over */
			if ( !OSAccessOK(PVR_VERIFY_READ, (IMG_VOID*) psRGXConfigEnableHWPerfCountersIN->psBlockConfigs, psRGXConfigEnableHWPerfCountersIN->ui32ArrayLen * sizeof(RGX_HWPERF_CONFIG_CNTBLK))
				|| (OSCopyFromUser(NULL, psBlockConfigsInt, psRGXConfigEnableHWPerfCountersIN->psBlockConfigs,
				psRGXConfigEnableHWPerfCountersIN->ui32ArrayLen * sizeof(RGX_HWPERF_CONFIG_CNTBLK)) != PVRSRV_OK) )
			{
				psRGXConfigEnableHWPerfCountersOUT->eError = PVRSRV_ERROR_INVALID_PARAMS;

				goto RGXConfigEnableHWPerfCounters_exit;
			}



				{
					/* Look up the address from the handle */
					psRGXConfigEnableHWPerfCountersOUT->eError =
						PVRSRVLookupHandle(psConnection->psHandleBase,
											(IMG_VOID **) &hDevNodeInt,
											psRGXConfigEnableHWPerfCountersIN->hDevNode,
											PVRSRV_HANDLE_TYPE_DEV_NODE);
					if(psRGXConfigEnableHWPerfCountersOUT->eError != PVRSRV_OK)
					{
						goto RGXConfigEnableHWPerfCounters_exit;
					}
				}


	psRGXConfigEnableHWPerfCountersOUT->eError =
		PVRSRVRGXConfigEnableHWPerfCountersKM(
					hDevNodeInt,
					psRGXConfigEnableHWPerfCountersIN->ui32ArrayLen,
					psBlockConfigsInt);




RGXConfigEnableHWPerfCounters_exit:
	if (psBlockConfigsInt)
		OSFreeMem(psBlockConfigsInt);

	return 0;
}

static IMG_INT
PVRSRVBridgeRGXCtrlHWPerfCounters(IMG_UINT32 ui32BridgeID,
					  PVRSRV_BRIDGE_IN_RGXCTRLHWPERFCOUNTERS *psRGXCtrlHWPerfCountersIN,
					  PVRSRV_BRIDGE_OUT_RGXCTRLHWPERFCOUNTERS *psRGXCtrlHWPerfCountersOUT,
					 CONNECTION_DATA *psConnection)
{
	IMG_HANDLE hDevNodeInt = IMG_NULL;
	IMG_UINT16 *ui16BlockIDsInt = IMG_NULL;

	PVRSRV_BRIDGE_ASSERT_CMD(ui32BridgeID, PVRSRV_BRIDGE_RGXHWPERF_RGXCTRLHWPERFCOUNTERS);




	if (psRGXCtrlHWPerfCountersIN->ui32ArrayLen != 0)
	{
		ui16BlockIDsInt = OSAllocMem(psRGXCtrlHWPerfCountersIN->ui32ArrayLen * sizeof(IMG_UINT16));
		if (!ui16BlockIDsInt)
		{
			psRGXCtrlHWPerfCountersOUT->eError = PVRSRV_ERROR_OUT_OF_MEMORY;
	
			goto RGXCtrlHWPerfCounters_exit;
		}
	}

			/* Copy the data over */
			if ( !OSAccessOK(PVR_VERIFY_READ, (IMG_VOID*) psRGXCtrlHWPerfCountersIN->pui16BlockIDs, psRGXCtrlHWPerfCountersIN->ui32ArrayLen * sizeof(IMG_UINT16))
				|| (OSCopyFromUser(NULL, ui16BlockIDsInt, psRGXCtrlHWPerfCountersIN->pui16BlockIDs,
				psRGXCtrlHWPerfCountersIN->ui32ArrayLen * sizeof(IMG_UINT16)) != PVRSRV_OK) )
			{
				psRGXCtrlHWPerfCountersOUT->eError = PVRSRV_ERROR_INVALID_PARAMS;

				goto RGXCtrlHWPerfCounters_exit;
			}



				{
					/* Look up the address from the handle */
					psRGXCtrlHWPerfCountersOUT->eError =
						PVRSRVLookupHandle(psConnection->psHandleBase,
											(IMG_VOID **) &hDevNodeInt,
											psRGXCtrlHWPerfCountersIN->hDevNode,
											PVRSRV_HANDLE_TYPE_DEV_NODE);
					if(psRGXCtrlHWPerfCountersOUT->eError != PVRSRV_OK)
					{
						goto RGXCtrlHWPerfCounters_exit;
					}
				}


	psRGXCtrlHWPerfCountersOUT->eError =
		PVRSRVRGXCtrlHWPerfCountersKM(
					hDevNodeInt,
					psRGXCtrlHWPerfCountersIN->bEnable,
					psRGXCtrlHWPerfCountersIN->ui32ArrayLen,
					ui16BlockIDsInt);




RGXCtrlHWPerfCounters_exit:
	if (ui16BlockIDsInt)
		OSFreeMem(ui16BlockIDsInt);

	return 0;
}

static IMG_INT
PVRSRVBridgeRGXConfigCustomCounters(IMG_UINT32 ui32BridgeID,
					  PVRSRV_BRIDGE_IN_RGXCONFIGCUSTOMCOUNTERS *psRGXConfigCustomCountersIN,
					  PVRSRV_BRIDGE_OUT_RGXCONFIGCUSTOMCOUNTERS *psRGXConfigCustomCountersOUT,
					 CONNECTION_DATA *psConnection)
{
	IMG_HANDLE hDevNodeInt = IMG_NULL;
	IMG_UINT32 *ui32CustomCounterIDsInt = IMG_NULL;

	PVRSRV_BRIDGE_ASSERT_CMD(ui32BridgeID, PVRSRV_BRIDGE_RGXHWPERF_RGXCONFIGCUSTOMCOUNTERS);




	if (psRGXConfigCustomCountersIN->ui16NumCustomCounters != 0)
	{
		ui32CustomCounterIDsInt = OSAllocMem(psRGXConfigCustomCountersIN->ui16NumCustomCounters * sizeof(IMG_UINT32));
		if (!ui32CustomCounterIDsInt)
		{
			psRGXConfigCustomCountersOUT->eError = PVRSRV_ERROR_OUT_OF_MEMORY;
	
			goto RGXConfigCustomCounters_exit;
		}
	}

			/* Copy the data over */
			if ( !OSAccessOK(PVR_VERIFY_READ, (IMG_VOID*) psRGXConfigCustomCountersIN->pui32CustomCounterIDs, psRGXConfigCustomCountersIN->ui16NumCustomCounters * sizeof(IMG_UINT32))
				|| (OSCopyFromUser(NULL, ui32CustomCounterIDsInt, psRGXConfigCustomCountersIN->pui32CustomCounterIDs,
				psRGXConfigCustomCountersIN->ui16NumCustomCounters * sizeof(IMG_UINT32)) != PVRSRV_OK) )
			{
				psRGXConfigCustomCountersOUT->eError = PVRSRV_ERROR_INVALID_PARAMS;

				goto RGXConfigCustomCounters_exit;
			}



				{
					/* Look up the address from the handle */
					psRGXConfigCustomCountersOUT->eError =
						PVRSRVLookupHandle(psConnection->psHandleBase,
											(IMG_VOID **) &hDevNodeInt,
											psRGXConfigCustomCountersIN->hDevNode,
											PVRSRV_HANDLE_TYPE_DEV_NODE);
					if(psRGXConfigCustomCountersOUT->eError != PVRSRV_OK)
					{
						goto RGXConfigCustomCounters_exit;
					}
				}


	psRGXConfigCustomCountersOUT->eError =
		PVRSRVRGXConfigCustomCountersKM(
					hDevNodeInt,
					psRGXConfigCustomCountersIN->ui16CustomBlockID,
					psRGXConfigCustomCountersIN->ui16NumCustomCounters,
					ui32CustomCounterIDsInt);




RGXConfigCustomCounters_exit:
	if (ui32CustomCounterIDsInt)
		OSFreeMem(ui32CustomCounterIDsInt);

	return 0;
}



/* *************************************************************************** 
 * Server bridge dispatch related glue 
 */


PVRSRV_ERROR InitRGXHWPERFBridge(IMG_VOID);
PVRSRV_ERROR DeinitRGXHWPERFBridge(IMG_VOID);

/*
 * Register all RGXHWPERF functions with services
 */
PVRSRV_ERROR InitRGXHWPERFBridge(IMG_VOID)
{

	SetDispatchTableEntry(PVRSRV_BRIDGE_RGXHWPERF_RGXCTRLHWPERF, PVRSRVBridgeRGXCtrlHWPerf,
					IMG_NULL, IMG_NULL,
					0, 0);

	SetDispatchTableEntry(PVRSRV_BRIDGE_RGXHWPERF_RGXCONFIGENABLEHWPERFCOUNTERS, PVRSRVBridgeRGXConfigEnableHWPerfCounters,
					IMG_NULL, IMG_NULL,
					0, 0);

	SetDispatchTableEntry(PVRSRV_BRIDGE_RGXHWPERF_RGXCTRLHWPERFCOUNTERS, PVRSRVBridgeRGXCtrlHWPerfCounters,
					IMG_NULL, IMG_NULL,
					0, 0);

	SetDispatchTableEntry(PVRSRV_BRIDGE_RGXHWPERF_RGXCONFIGCUSTOMCOUNTERS, PVRSRVBridgeRGXConfigCustomCounters,
					IMG_NULL, IMG_NULL,
					0, 0);


	return PVRSRV_OK;
}

/*
 * Unregister all rgxhwperf functions with services
 */
PVRSRV_ERROR DeinitRGXHWPERFBridge(IMG_VOID)
{
	return PVRSRV_OK;
}

