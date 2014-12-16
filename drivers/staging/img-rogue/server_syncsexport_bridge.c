/*************************************************************************/ /*!
@File
@Title          Server bridge for syncsexport
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@Description    Implements the server side of the bridge for syncsexport
@License        Strictly Confidential.
*/ /**************************************************************************/

#include <stddef.h>
#include <asm/uaccess.h>

#include "img_defs.h"

#include "sync_server.h"


#include "common_syncsexport_bridge.h"

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
PVRSRVBridgeSyncPrimServerSecureExport(IMG_UINT32 ui32BridgeID,
					  PVRSRV_BRIDGE_IN_SYNCPRIMSERVERSECUREEXPORT *psSyncPrimServerSecureExportIN,
					  PVRSRV_BRIDGE_OUT_SYNCPRIMSERVERSECUREEXPORT *psSyncPrimServerSecureExportOUT,
					 CONNECTION_DATA *psConnection)
{
	SERVER_SYNC_PRIMITIVE * psSyncHandleInt = IMG_NULL;
	SERVER_SYNC_EXPORT * psExportInt = IMG_NULL;
	IMG_HANDLE hExportInt = IMG_NULL;
	CONNECTION_DATA *psSecureConnection;

	PVRSRV_BRIDGE_ASSERT_CMD(ui32BridgeID, PVRSRV_BRIDGE_SYNCSEXPORT_SYNCPRIMSERVERSECUREEXPORT);







				{
					/* Look up the address from the handle */
					psSyncPrimServerSecureExportOUT->eError =
						PVRSRVLookupHandle(psConnection->psHandleBase,
											(IMG_VOID **) &psSyncHandleInt,
											psSyncPrimServerSecureExportIN->hSyncHandle,
											PVRSRV_HANDLE_TYPE_SERVER_SYNC_PRIMITIVE);
					if(psSyncPrimServerSecureExportOUT->eError != PVRSRV_OK)
					{
						goto SyncPrimServerSecureExport_exit;
					}
				}


	psSyncPrimServerSecureExportOUT->eError =
		PVRSRVSyncPrimServerSecureExportKM(psConnection,
					psSyncHandleInt,
					&psSyncPrimServerSecureExportOUT->Export,
					&psExportInt, &psSecureConnection);
	/* Exit early if bridged call fails */
	if(psSyncPrimServerSecureExportOUT->eError != PVRSRV_OK)
	{
		goto SyncPrimServerSecureExport_exit;
	}


	psSyncPrimServerSecureExportOUT->eError = PVRSRVAllocHandle(psSecureConnection->psHandleBase,
							&hExportInt,
							(IMG_VOID *) psExportInt,
							PVRSRV_HANDLE_TYPE_SERVER_SYNC_EXPORT,
							PVRSRV_HANDLE_ALLOC_FLAG_SHARED
							,(PFN_HANDLE_RELEASE)&PVRSRVSyncPrimServerSecureUnexportKM);
	if (psSyncPrimServerSecureExportOUT->eError != PVRSRV_OK)
	{
		goto SyncPrimServerSecureExport_exit;
	}




SyncPrimServerSecureExport_exit:
	if (psSyncPrimServerSecureExportOUT->eError != PVRSRV_OK)
	{
		if (psExportInt)
		{
			PVRSRVSyncPrimServerSecureUnexportKM(psExportInt);
		}
	}


	return 0;
}

static IMG_INT
PVRSRVBridgeSyncPrimServerSecureUnexport(IMG_UINT32 ui32BridgeID,
					  PVRSRV_BRIDGE_IN_SYNCPRIMSERVERSECUREUNEXPORT *psSyncPrimServerSecureUnexportIN,
					  PVRSRV_BRIDGE_OUT_SYNCPRIMSERVERSECUREUNEXPORT *psSyncPrimServerSecureUnexportOUT,
					 CONNECTION_DATA *psConnection)
{

	PVRSRV_BRIDGE_ASSERT_CMD(ui32BridgeID, PVRSRV_BRIDGE_SYNCSEXPORT_SYNCPRIMSERVERSECUREUNEXPORT);









	psSyncPrimServerSecureUnexportOUT->eError =
		PVRSRVReleaseHandle(psConnection->psHandleBase,
					(IMG_HANDLE) psSyncPrimServerSecureUnexportIN->hExport,
					PVRSRV_HANDLE_TYPE_SERVER_SYNC_EXPORT);
	if ((psSyncPrimServerSecureUnexportOUT->eError != PVRSRV_OK) && (psSyncPrimServerSecureUnexportOUT->eError != PVRSRV_ERROR_RETRY))
	{
		PVR_ASSERT(0);
		goto SyncPrimServerSecureUnexport_exit;
	}



SyncPrimServerSecureUnexport_exit:

	return 0;
}

static IMG_INT
PVRSRVBridgeSyncPrimServerSecureImport(IMG_UINT32 ui32BridgeID,
					  PVRSRV_BRIDGE_IN_SYNCPRIMSERVERSECUREIMPORT *psSyncPrimServerSecureImportIN,
					  PVRSRV_BRIDGE_OUT_SYNCPRIMSERVERSECUREIMPORT *psSyncPrimServerSecureImportOUT,
					 CONNECTION_DATA *psConnection)
{
	SERVER_SYNC_PRIMITIVE * psSyncHandleInt = IMG_NULL;

	PVRSRV_BRIDGE_ASSERT_CMD(ui32BridgeID, PVRSRV_BRIDGE_SYNCSEXPORT_SYNCPRIMSERVERSECUREIMPORT);







	psSyncPrimServerSecureImportOUT->eError =
		PVRSRVSyncPrimServerSecureImportKM(
					psSyncPrimServerSecureImportIN->Export,
					&psSyncHandleInt,
					&psSyncPrimServerSecureImportOUT->ui32SyncPrimVAddr);
	/* Exit early if bridged call fails */
	if(psSyncPrimServerSecureImportOUT->eError != PVRSRV_OK)
	{
		goto SyncPrimServerSecureImport_exit;
	}


	psSyncPrimServerSecureImportOUT->eError = PVRSRVAllocHandle(psConnection->psHandleBase,
							&psSyncPrimServerSecureImportOUT->hSyncHandle,
							(IMG_VOID *) psSyncHandleInt,
							PVRSRV_HANDLE_TYPE_SERVER_SYNC_PRIMITIVE,
							PVRSRV_HANDLE_ALLOC_FLAG_MULTI
							,(PFN_HANDLE_RELEASE)&PVRSRVServerSyncFreeKM);
	if (psSyncPrimServerSecureImportOUT->eError != PVRSRV_OK)
	{
		goto SyncPrimServerSecureImport_exit;
	}




SyncPrimServerSecureImport_exit:
	if (psSyncPrimServerSecureImportOUT->eError != PVRSRV_OK)
	{
		if (psSyncHandleInt)
		{
			PVRSRVServerSyncFreeKM(psSyncHandleInt);
		}
	}


	return 0;
}



/* *************************************************************************** 
 * Server bridge dispatch related glue 
 */


PVRSRV_ERROR InitSYNCSEXPORTBridge(IMG_VOID);
PVRSRV_ERROR DeinitSYNCSEXPORTBridge(IMG_VOID);

/*
 * Register all SYNCSEXPORT functions with services
 */
PVRSRV_ERROR InitSYNCSEXPORTBridge(IMG_VOID)
{

	SetDispatchTableEntry(PVRSRV_BRIDGE_SYNCSEXPORT_SYNCPRIMSERVERSECUREEXPORT, PVRSRVBridgeSyncPrimServerSecureExport,
					IMG_NULL, IMG_NULL,
					0, 0);

	SetDispatchTableEntry(PVRSRV_BRIDGE_SYNCSEXPORT_SYNCPRIMSERVERSECUREUNEXPORT, PVRSRVBridgeSyncPrimServerSecureUnexport,
					IMG_NULL, IMG_NULL,
					0, 0);

	SetDispatchTableEntry(PVRSRV_BRIDGE_SYNCSEXPORT_SYNCPRIMSERVERSECUREIMPORT, PVRSRVBridgeSyncPrimServerSecureImport,
					IMG_NULL, IMG_NULL,
					0, 0);


	return PVRSRV_OK;
}

/*
 * Unregister all syncsexport functions with services
 */
PVRSRV_ERROR DeinitSYNCSEXPORTBridge(IMG_VOID)
{
	return PVRSRV_OK;
}

