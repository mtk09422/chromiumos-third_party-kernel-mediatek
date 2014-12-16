/*************************************************************************/ /*!
@File
@Title          Server bridge for syncexport
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@Description    Implements the server side of the bridge for syncexport
@License        Strictly Confidential.
*/ /**************************************************************************/

#include <stddef.h>
#include <asm/uaccess.h>

#include "img_defs.h"

#include "sync_server.h"


#include "common_syncexport_bridge.h"

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


static PVRSRV_ERROR ReleaseExport(IMG_VOID *pvData)
{
	PVR_UNREFERENCED_PARAMETER(pvData);

	return PVRSRV_OK;
}


/* ***************************************************************************
 * Server-side bridge entry points
 */
 
static IMG_INT
PVRSRVBridgeSyncPrimServerExport(IMG_UINT32 ui32BridgeID,
					  PVRSRV_BRIDGE_IN_SYNCPRIMSERVEREXPORT *psSyncPrimServerExportIN,
					  PVRSRV_BRIDGE_OUT_SYNCPRIMSERVEREXPORT *psSyncPrimServerExportOUT,
					 CONNECTION_DATA *psConnection)
{
	SERVER_SYNC_PRIMITIVE * psSyncHandleInt = IMG_NULL;
	SERVER_SYNC_EXPORT * psExportInt = IMG_NULL;
	IMG_HANDLE hExportInt = IMG_NULL;

	PVRSRV_BRIDGE_ASSERT_CMD(ui32BridgeID, PVRSRV_BRIDGE_SYNCEXPORT_SYNCPRIMSERVEREXPORT);







				{
					/* Look up the address from the handle */
					psSyncPrimServerExportOUT->eError =
						PVRSRVLookupHandle(psConnection->psHandleBase,
											(IMG_VOID **) &psSyncHandleInt,
											psSyncPrimServerExportIN->hSyncHandle,
											PVRSRV_HANDLE_TYPE_SERVER_SYNC_PRIMITIVE);
					if(psSyncPrimServerExportOUT->eError != PVRSRV_OK)
					{
						goto SyncPrimServerExport_exit;
					}
				}


	psSyncPrimServerExportOUT->eError =
		PVRSRVSyncPrimServerExportKM(
					psSyncHandleInt,
					&psExportInt);
	/* Exit early if bridged call fails */
	if(psSyncPrimServerExportOUT->eError != PVRSRV_OK)
	{
		goto SyncPrimServerExport_exit;
	}


	/*
	 * For cases where we need a cross process handle we actually allocate two.
	 * 
	 * The first one is a connection specific handle and it gets given the real
	 * release function. This handle does *NOT* get returned to the caller. It's
	 * purpose is to release any leaked resources when we either have a bad or
	 * abnormally terminated client. If we didn't do this then the resource
	 * wouldn't be freed until driver unload. If the resource is freed normally,
	 * this handle can be looked up via the cross process handle and then
	 * released accordingly.
	 * 
	 * The second one is a cross process handle and it gets given a noop release
	 * function. This handle does get returned to the caller.
	 */
	psSyncPrimServerExportOUT->eError = PVRSRVAllocHandle(psConnection->psHandleBase,
							&hExportInt,
							(IMG_VOID *) psExportInt,
							PVRSRV_HANDLE_TYPE_SERVER_SYNC_EXPORT,
							PVRSRV_HANDLE_ALLOC_FLAG_SHARED
							,(PFN_HANDLE_RELEASE)&PVRSRVSyncPrimServerUnexportKM);
	if (psSyncPrimServerExportOUT->eError != PVRSRV_OK)
	{
		goto SyncPrimServerExport_exit;
	}

	psSyncPrimServerExportOUT->eError = PVRSRVAllocHandle(KERNEL_HANDLE_BASE,
							&psSyncPrimServerExportOUT->hExport,
							(IMG_VOID *) psExportInt,
							PVRSRV_HANDLE_TYPE_SERVER_SYNC_EXPORT,
							PVRSRV_HANDLE_ALLOC_FLAG_MULTI,
							(PFN_HANDLE_RELEASE)&ReleaseExport);
	if (psSyncPrimServerExportOUT->eError != PVRSRV_OK)
	{
		goto SyncPrimServerExport_exit;
	}



SyncPrimServerExport_exit:
	if (psSyncPrimServerExportOUT->eError != PVRSRV_OK)
	{
		if (psSyncPrimServerExportOUT->hExport)
		{
			PVRSRV_ERROR eError = PVRSRVReleaseHandle(KERNEL_HANDLE_BASE,
						(IMG_HANDLE) psSyncPrimServerExportOUT->hExport,
						PVRSRV_HANDLE_TYPE_SERVER_SYNC_EXPORT);

			/* Releasing the handle should free/destroy/release the resource. This should never fail... */
			PVR_ASSERT((eError == PVRSRV_OK) || (eError == PVRSRV_ERROR_RETRY));

		}

		if (hExportInt)
		{
			PVRSRV_ERROR eError = PVRSRVReleaseHandle(psConnection->psHandleBase,
						hExportInt,
						PVRSRV_HANDLE_TYPE_SERVER_SYNC_EXPORT);

			/* Releasing the handle should free/destroy/release the resource. This should never fail... */
			PVR_ASSERT((eError == PVRSRV_OK) || (eError == PVRSRV_ERROR_RETRY));

			/* Avoid freeing/destroying/releasing the resource a second time below */
			psExportInt = IMG_NULL;
		}

		if (psExportInt)
		{
			PVRSRVSyncPrimServerUnexportKM(psExportInt);
		}
	}


	return 0;
}

static IMG_INT
PVRSRVBridgeSyncPrimServerUnexport(IMG_UINT32 ui32BridgeID,
					  PVRSRV_BRIDGE_IN_SYNCPRIMSERVERUNEXPORT *psSyncPrimServerUnexportIN,
					  PVRSRV_BRIDGE_OUT_SYNCPRIMSERVERUNEXPORT *psSyncPrimServerUnexportOUT,
					 CONNECTION_DATA *psConnection)
{
	SERVER_SYNC_EXPORT * psExportInt = IMG_NULL;
	IMG_HANDLE hExportInt = IMG_NULL;

	PVRSRV_BRIDGE_ASSERT_CMD(ui32BridgeID, PVRSRV_BRIDGE_SYNCEXPORT_SYNCPRIMSERVERUNEXPORT);

	PVR_UNREFERENCED_PARAMETER(psConnection);







	psSyncPrimServerUnexportOUT->eError =
		PVRSRVLookupHandle(KERNEL_HANDLE_BASE,
					(IMG_VOID **) &psExportInt,
					(IMG_HANDLE) psSyncPrimServerUnexportIN->hExport,
					PVRSRV_HANDLE_TYPE_SERVER_SYNC_EXPORT);
	PVR_ASSERT(psSyncPrimServerUnexportOUT->eError == PVRSRV_OK);

	/*
	 * Find the connection specific handle that represents the same data
	 * as the cross process handle as releasing it will actually call the
	 * data's real release function (see the function where the cross
	 * process handle is allocated for more details).
	 */
	psSyncPrimServerUnexportOUT->eError =
		PVRSRVFindHandle(psConnection->psHandleBase,
					&hExportInt,
					psExportInt,
					PVRSRV_HANDLE_TYPE_SERVER_SYNC_EXPORT);
	PVR_ASSERT(psSyncPrimServerUnexportOUT->eError == PVRSRV_OK);

	psSyncPrimServerUnexportOUT->eError =
		PVRSRVReleaseHandle(psConnection->psHandleBase,
					hExportInt,
					PVRSRV_HANDLE_TYPE_SERVER_SYNC_EXPORT);
	PVR_ASSERT((psSyncPrimServerUnexportOUT->eError == PVRSRV_OK) || (psSyncPrimServerUnexportOUT->eError == PVRSRV_ERROR_RETRY));

	psSyncPrimServerUnexportOUT->eError =
		PVRSRVReleaseHandle(KERNEL_HANDLE_BASE,
					(IMG_HANDLE) psSyncPrimServerUnexportIN->hExport,
					PVRSRV_HANDLE_TYPE_SERVER_SYNC_EXPORT);
	if ((psSyncPrimServerUnexportOUT->eError != PVRSRV_OK) && (psSyncPrimServerUnexportOUT->eError != PVRSRV_ERROR_RETRY))
	{
		PVR_ASSERT(0);
		goto SyncPrimServerUnexport_exit;
	}



SyncPrimServerUnexport_exit:

	return 0;
}

static IMG_INT
PVRSRVBridgeSyncPrimServerImport(IMG_UINT32 ui32BridgeID,
					  PVRSRV_BRIDGE_IN_SYNCPRIMSERVERIMPORT *psSyncPrimServerImportIN,
					  PVRSRV_BRIDGE_OUT_SYNCPRIMSERVERIMPORT *psSyncPrimServerImportOUT,
					 CONNECTION_DATA *psConnection)
{
	SERVER_SYNC_EXPORT * psImportInt = IMG_NULL;
	SERVER_SYNC_PRIMITIVE * psSyncHandleInt = IMG_NULL;

	PVRSRV_BRIDGE_ASSERT_CMD(ui32BridgeID, PVRSRV_BRIDGE_SYNCEXPORT_SYNCPRIMSERVERIMPORT);




#if defined (SUPPORT_AUTH)
	psSyncPrimServerImportOUT->eError = OSCheckAuthentication(psConnection, 1);
	if (psSyncPrimServerImportOUT->eError != PVRSRV_OK)
	{
		goto SyncPrimServerImport_exit;
	}
#endif



				{
					/* Look up the address from the handle */
					psSyncPrimServerImportOUT->eError =
						PVRSRVLookupHandle(KERNEL_HANDLE_BASE,
											(IMG_VOID **) &psImportInt,
											psSyncPrimServerImportIN->hImport,
											PVRSRV_HANDLE_TYPE_SERVER_SYNC_EXPORT);
					if(psSyncPrimServerImportOUT->eError != PVRSRV_OK)
					{
						goto SyncPrimServerImport_exit;
					}
				}


	psSyncPrimServerImportOUT->eError =
		PVRSRVSyncPrimServerImportKM(
					psImportInt,
					&psSyncHandleInt,
					&psSyncPrimServerImportOUT->ui32SyncPrimVAddr);
	/* Exit early if bridged call fails */
	if(psSyncPrimServerImportOUT->eError != PVRSRV_OK)
	{
		goto SyncPrimServerImport_exit;
	}


	psSyncPrimServerImportOUT->eError = PVRSRVAllocHandle(psConnection->psHandleBase,
							&psSyncPrimServerImportOUT->hSyncHandle,
							(IMG_VOID *) psSyncHandleInt,
							PVRSRV_HANDLE_TYPE_SERVER_SYNC_PRIMITIVE,
							PVRSRV_HANDLE_ALLOC_FLAG_MULTI
							,(PFN_HANDLE_RELEASE)&PVRSRVServerSyncFreeKM);
	if (psSyncPrimServerImportOUT->eError != PVRSRV_OK)
	{
		goto SyncPrimServerImport_exit;
	}




SyncPrimServerImport_exit:
	if (psSyncPrimServerImportOUT->eError != PVRSRV_OK)
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


PVRSRV_ERROR InitSYNCEXPORTBridge(IMG_VOID);
PVRSRV_ERROR DeinitSYNCEXPORTBridge(IMG_VOID);

/*
 * Register all SYNCEXPORT functions with services
 */
PVRSRV_ERROR InitSYNCEXPORTBridge(IMG_VOID)
{

	SetDispatchTableEntry(PVRSRV_BRIDGE_SYNCEXPORT_SYNCPRIMSERVEREXPORT, PVRSRVBridgeSyncPrimServerExport,
					IMG_NULL, IMG_NULL,
					0, 0);

	SetDispatchTableEntry(PVRSRV_BRIDGE_SYNCEXPORT_SYNCPRIMSERVERUNEXPORT, PVRSRVBridgeSyncPrimServerUnexport,
					IMG_NULL, IMG_NULL,
					0, 0);

	SetDispatchTableEntry(PVRSRV_BRIDGE_SYNCEXPORT_SYNCPRIMSERVERIMPORT, PVRSRVBridgeSyncPrimServerImport,
					IMG_NULL, IMG_NULL,
					0, 0);


	return PVRSRV_OK;
}

/*
 * Unregister all syncexport functions with services
 */
PVRSRV_ERROR DeinitSYNCEXPORTBridge(IMG_VOID)
{
	return PVRSRV_OK;
}

