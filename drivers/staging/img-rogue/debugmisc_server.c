/*************************************************************************/ /*!
@File
@Title          Debugging and miscellaneous functions server implementation
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@Description    Kernel services functions for debugging and other
                miscellaneous functionality.
@License        Strictly Confidential.
*/ /**************************************************************************/

#include "pvrsrv.h"
#include "pvr_debug.h"
#include "debugmisc_server.h"
#include "rgxfwutils.h"
#include "rgxta3d.h"
#include "pdump_km.h"
#include "mmu_common.h"
#include "devicemem_server.h"
#include "osfunc.h"

IMG_EXPORT PVRSRV_ERROR
PVRSRVDebugMiscSLCSetBypassStateKM(
	PVRSRV_DEVICE_NODE *psDeviceNode,
	IMG_UINT32  uiFlags,
	IMG_BOOL bSetBypassed)
{
	RGXFWIF_KCCB_CMD  sSLCBPCtlCmd;
	PVRSRV_ERROR  eError = PVRSRV_OK;

	sSLCBPCtlCmd.eCmdType = RGXFWIF_KCCB_CMD_SLCBPCTL;
	sSLCBPCtlCmd.uCmdData.sSLCBPCtlData.bSetBypassed = bSetBypassed;
	sSLCBPCtlCmd.uCmdData.sSLCBPCtlData.uiFlags = uiFlags;

	eError = RGXScheduleCommand(psDeviceNode->pvDevice,
	                            RGXFWIF_DM_GP,
	                            &sSLCBPCtlCmd,
	                            sizeof(sSLCBPCtlCmd),
	                            IMG_TRUE);
	if(eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR, "PVRSRVDebugMiscSLCSetEnableStateKM: RGXScheduleCommandfailed. Error:%u", eError));
	}
	else
	{
		/* Wait for the SLC flush to complete */
		eError = RGXWaitForFWOp(psDeviceNode->pvDevice, RGXFWIF_DM_GP, psDeviceNode->psSyncPrim, IMG_TRUE);
		if (eError != PVRSRV_OK)
		{
			PVR_DPF((PVR_DBG_ERROR,"PVRSRVDebugMiscSLCSetEnableStateKM: Waiting for value aborted with error (%u)", eError));
		}
	}

	return PVRSRV_OK;
}

IMG_EXPORT PVRSRV_ERROR
PVRSRVRGXDebugMiscSetFWLogKM(
	PVRSRV_DEVICE_NODE *psDeviceNode,
	IMG_UINT32  ui32RGXFWLogType)
{
	PVRSRV_RGXDEV_INFO* psDevInfo = psDeviceNode->pvDevice;

	/* check log type is valid */
	if (ui32RGXFWLogType & ~RGXFWIF_LOG_TYPE_MASK)
	{
		return PVRSRV_ERROR_INVALID_PARAMS;
	}

	/* set the new log type */
	psDevInfo->psRGXFWIfTraceBuf->ui32LogType = ui32RGXFWLogType;

	return PVRSRV_OK;

}

static IMG_BOOL
_RGXDumpFreeListPageList(PDLLIST_NODE psNode, IMG_PVOID pvCallbackData)
{
	RGX_FREELIST *psFreeList = IMG_CONTAINER_OF(psNode, RGX_FREELIST, sNode);

	RGXDumpFreeListPageList(psFreeList);

	return IMG_TRUE;
}

IMG_EXPORT PVRSRV_ERROR
PVRSRVRGXDebugMiscDumpFreelistPageListKM(
	PVRSRV_DEVICE_NODE *psDeviceNode)
{
	PVRSRV_RGXDEV_INFO* psDevInfo = psDeviceNode->pvDevice;

	if (dllist_is_empty(&psDevInfo->sFreeListHead))
	{
		return PVRSRV_OK;
	}

	PVR_LOG(("---------------[ Begin Freelist Page List Dump ]------------------"));

	OSLockAcquire(psDevInfo->hLockFreeList);
	dllist_foreach_node(&psDevInfo->sFreeListHead, _RGXDumpFreeListPageList, IMG_NULL);
	OSLockRelease(psDevInfo->hLockFreeList);

	PVR_LOG(("----------------[ End Freelist Page List Dump ]-------------------"));

	return PVRSRV_OK;

}
