/*************************************************************************/ /*!
@File
@Title          Server bridge for debugmisc
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@Description    Implements the server side of the bridge for debugmisc
@License        Strictly Confidential.
*/ /**************************************************************************/

#include <stddef.h>
#include <asm/uaccess.h>

#include "img_defs.h"

#include "devicemem_server.h"
#include "debugmisc_server.h"
#include "pmr.h"
#include "physmem_osmem.h"


#include "common_debugmisc_bridge.h"

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
PVRSRVBridgeDebugMiscSLCSetBypassState(IMG_UINT32 ui32BridgeID,
					  PVRSRV_BRIDGE_IN_DEBUGMISCSLCSETBYPASSSTATE *psDebugMiscSLCSetBypassStateIN,
					  PVRSRV_BRIDGE_OUT_DEBUGMISCSLCSETBYPASSSTATE *psDebugMiscSLCSetBypassStateOUT,
					 CONNECTION_DATA *psConnection)
{
	IMG_HANDLE hDevNodeInt = IMG_NULL;

	PVRSRV_BRIDGE_ASSERT_CMD(ui32BridgeID, PVRSRV_BRIDGE_DEBUGMISC_DEBUGMISCSLCSETBYPASSSTATE);







				{
					/* Look up the address from the handle */
					psDebugMiscSLCSetBypassStateOUT->eError =
						PVRSRVLookupHandle(psConnection->psHandleBase,
											(IMG_VOID **) &hDevNodeInt,
											psDebugMiscSLCSetBypassStateIN->hDevNode,
											PVRSRV_HANDLE_TYPE_DEV_NODE);
					if(psDebugMiscSLCSetBypassStateOUT->eError != PVRSRV_OK)
					{
						goto DebugMiscSLCSetBypassState_exit;
					}
				}


	psDebugMiscSLCSetBypassStateOUT->eError =
		PVRSRVDebugMiscSLCSetBypassStateKM(
					hDevNodeInt,
					psDebugMiscSLCSetBypassStateIN->ui32Flags,
					psDebugMiscSLCSetBypassStateIN->bIsBypassed);




DebugMiscSLCSetBypassState_exit:

	return 0;
}

static IMG_INT
PVRSRVBridgeRGXDebugMiscSetFWLog(IMG_UINT32 ui32BridgeID,
					  PVRSRV_BRIDGE_IN_RGXDEBUGMISCSETFWLOG *psRGXDebugMiscSetFWLogIN,
					  PVRSRV_BRIDGE_OUT_RGXDEBUGMISCSETFWLOG *psRGXDebugMiscSetFWLogOUT,
					 CONNECTION_DATA *psConnection)
{
	IMG_HANDLE hDevNodeInt = IMG_NULL;

	PVRSRV_BRIDGE_ASSERT_CMD(ui32BridgeID, PVRSRV_BRIDGE_DEBUGMISC_RGXDEBUGMISCSETFWLOG);







				{
					/* Look up the address from the handle */
					psRGXDebugMiscSetFWLogOUT->eError =
						PVRSRVLookupHandle(psConnection->psHandleBase,
											(IMG_VOID **) &hDevNodeInt,
											psRGXDebugMiscSetFWLogIN->hDevNode,
											PVRSRV_HANDLE_TYPE_DEV_NODE);
					if(psRGXDebugMiscSetFWLogOUT->eError != PVRSRV_OK)
					{
						goto RGXDebugMiscSetFWLog_exit;
					}
				}


	psRGXDebugMiscSetFWLogOUT->eError =
		PVRSRVRGXDebugMiscSetFWLogKM(
					hDevNodeInt,
					psRGXDebugMiscSetFWLogIN->ui32RGXFWLogType);




RGXDebugMiscSetFWLog_exit:

	return 0;
}

static IMG_INT
PVRSRVBridgeRGXDebugMiscDumpFreelistPageList(IMG_UINT32 ui32BridgeID,
					  PVRSRV_BRIDGE_IN_RGXDEBUGMISCDUMPFREELISTPAGELIST *psRGXDebugMiscDumpFreelistPageListIN,
					  PVRSRV_BRIDGE_OUT_RGXDEBUGMISCDUMPFREELISTPAGELIST *psRGXDebugMiscDumpFreelistPageListOUT,
					 CONNECTION_DATA *psConnection)
{
	IMG_HANDLE hDevNodeInt = IMG_NULL;

	PVRSRV_BRIDGE_ASSERT_CMD(ui32BridgeID, PVRSRV_BRIDGE_DEBUGMISC_RGXDEBUGMISCDUMPFREELISTPAGELIST);







				{
					/* Look up the address from the handle */
					psRGXDebugMiscDumpFreelistPageListOUT->eError =
						PVRSRVLookupHandle(psConnection->psHandleBase,
											(IMG_VOID **) &hDevNodeInt,
											psRGXDebugMiscDumpFreelistPageListIN->hDevNode,
											PVRSRV_HANDLE_TYPE_DEV_NODE);
					if(psRGXDebugMiscDumpFreelistPageListOUT->eError != PVRSRV_OK)
					{
						goto RGXDebugMiscDumpFreelistPageList_exit;
					}
				}


	psRGXDebugMiscDumpFreelistPageListOUT->eError =
		PVRSRVRGXDebugMiscDumpFreelistPageListKM(
					hDevNodeInt);




RGXDebugMiscDumpFreelistPageList_exit:

	return 0;
}

static IMG_INT
PVRSRVBridgePhysmemImportSecBuf(IMG_UINT32 ui32BridgeID,
					  PVRSRV_BRIDGE_IN_PHYSMEMIMPORTSECBUF *psPhysmemImportSecBufIN,
					  PVRSRV_BRIDGE_OUT_PHYSMEMIMPORTSECBUF *psPhysmemImportSecBufOUT,
					 CONNECTION_DATA *psConnection)
{
	IMG_HANDLE hDevNodeInt = IMG_NULL;
	PMR * psPMRPtrInt = IMG_NULL;

	PVRSRV_BRIDGE_ASSERT_CMD(ui32BridgeID, PVRSRV_BRIDGE_DEBUGMISC_PHYSMEMIMPORTSECBUF);







				{
					/* Look up the address from the handle */
					psPhysmemImportSecBufOUT->eError =
						PVRSRVLookupHandle(psConnection->psHandleBase,
											(IMG_VOID **) &hDevNodeInt,
											psPhysmemImportSecBufIN->hDevNode,
											PVRSRV_HANDLE_TYPE_DEV_NODE);
					if(psPhysmemImportSecBufOUT->eError != PVRSRV_OK)
					{
						goto PhysmemImportSecBuf_exit;
					}
				}


	psPhysmemImportSecBufOUT->eError =
		PhysmemNewTDSecureBufPMR(
					hDevNodeInt,
					psPhysmemImportSecBufIN->uiSize,
					psPhysmemImportSecBufIN->ui32Log2PageSize,
					psPhysmemImportSecBufIN->uiFlags,
					&psPMRPtrInt);
	/* Exit early if bridged call fails */
	if(psPhysmemImportSecBufOUT->eError != PVRSRV_OK)
	{
		goto PhysmemImportSecBuf_exit;
	}


	psPhysmemImportSecBufOUT->eError = PVRSRVAllocHandle(psConnection->psHandleBase,
							&psPhysmemImportSecBufOUT->hPMRPtr,
							(IMG_VOID *) psPMRPtrInt,
							PVRSRV_HANDLE_TYPE_PHYSMEM_PMR,
							PVRSRV_HANDLE_ALLOC_FLAG_MULTI
							,(PFN_HANDLE_RELEASE)&PMRUnrefPMR);
	if (psPhysmemImportSecBufOUT->eError != PVRSRV_OK)
	{
		goto PhysmemImportSecBuf_exit;
	}




PhysmemImportSecBuf_exit:
	if (psPhysmemImportSecBufOUT->eError != PVRSRV_OK)
	{
		if (psPMRPtrInt)
		{
			PMRUnrefPMR(psPMRPtrInt);
		}
	}


	return 0;
}

static IMG_INT
PVRSRVBridgePowMonTestIoctl(IMG_UINT32 ui32BridgeID,
					  PVRSRV_BRIDGE_IN_POWMONTESTIOCTL *psPowMonTestIoctlIN,
					  PVRSRV_BRIDGE_OUT_POWMONTESTIOCTL *psPowMonTestIoctlOUT,
					 CONNECTION_DATA *psConnection)
{

	PVRSRV_BRIDGE_ASSERT_CMD(ui32BridgeID, PVRSRV_BRIDGE_DEBUGMISC_POWMONTESTIOCTL);

	PVR_UNREFERENCED_PARAMETER(psConnection);






	psPowMonTestIoctlOUT->eError =
		PowMonTestIoctlKM(
					psPowMonTestIoctlIN->ui32Cmd,
					psPowMonTestIoctlIN->ui32In1,
					psPowMonTestIoctlIN->ui32In2,
					&psPowMonTestIoctlOUT->ui32Out1,
					&psPowMonTestIoctlOUT->ui32Out2);





	return 0;
}



/* *************************************************************************** 
 * Server bridge dispatch related glue 
 */


PVRSRV_ERROR InitDEBUGMISCBridge(IMG_VOID);
PVRSRV_ERROR DeinitDEBUGMISCBridge(IMG_VOID);

/*
 * Register all DEBUGMISC functions with services
 */
PVRSRV_ERROR InitDEBUGMISCBridge(IMG_VOID)
{

	SetDispatchTableEntry(PVRSRV_BRIDGE_DEBUGMISC_DEBUGMISCSLCSETBYPASSSTATE, PVRSRVBridgeDebugMiscSLCSetBypassState,
					IMG_NULL, IMG_NULL,
					0, 0);

	SetDispatchTableEntry(PVRSRV_BRIDGE_DEBUGMISC_RGXDEBUGMISCSETFWLOG, PVRSRVBridgeRGXDebugMiscSetFWLog,
					IMG_NULL, IMG_NULL,
					0, 0);

	SetDispatchTableEntry(PVRSRV_BRIDGE_DEBUGMISC_RGXDEBUGMISCDUMPFREELISTPAGELIST, PVRSRVBridgeRGXDebugMiscDumpFreelistPageList,
					IMG_NULL, IMG_NULL,
					0, 0);

	SetDispatchTableEntry(PVRSRV_BRIDGE_DEBUGMISC_PHYSMEMIMPORTSECBUF, PVRSRVBridgePhysmemImportSecBuf,
					IMG_NULL, IMG_NULL,
					0, 0);

	SetDispatchTableEntry(PVRSRV_BRIDGE_DEBUGMISC_POWMONTESTIOCTL, PVRSRVBridgePowMonTestIoctl,
					IMG_NULL, IMG_NULL,
					0, 0);


	return PVRSRV_OK;
}

/*
 * Unregister all debugmisc functions with services
 */
PVRSRV_ERROR DeinitDEBUGMISCBridge(IMG_VOID)
{
	return PVRSRV_OK;
}

