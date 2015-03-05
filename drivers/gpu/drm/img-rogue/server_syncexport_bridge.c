/*************************************************************************/ /*!
@File
@Title          Server bridge for syncexport
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@Description    Implements the server side of the bridge for syncexport
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


static PVRSRV_ERROR ReleaseExport(void *pvData)
{
	PVR_UNREFERENCED_PARAMETER(pvData);

	return PVRSRV_OK;
}


/* ***************************************************************************
 * Server-side bridge entry points
 */
 
static IMG_INT
PVRSRVBridgeSyncPrimServerExport(IMG_UINT32 ui32DispatchTableEntry,
					  PVRSRV_BRIDGE_IN_SYNCPRIMSERVEREXPORT *psSyncPrimServerExportIN,
					  PVRSRV_BRIDGE_OUT_SYNCPRIMSERVEREXPORT *psSyncPrimServerExportOUT,
					 CONNECTION_DATA *psConnection)
{
	SERVER_SYNC_PRIMITIVE * psSyncHandleInt = NULL;
	SERVER_SYNC_EXPORT * psExportInt = NULL;
	IMG_HANDLE hExportInt = NULL;







				{
					/* Look up the address from the handle */
					psSyncPrimServerExportOUT->eError =
						PVRSRVLookupHandle(psConnection->psHandleBase,
											(void **) &psSyncHandleInt,
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
							(void *) psExportInt,
							PVRSRV_HANDLE_TYPE_SERVER_SYNC_EXPORT,
							PVRSRV_HANDLE_ALLOC_FLAG_SHARED
							,(PFN_HANDLE_RELEASE)&PVRSRVSyncPrimServerUnexportKM);
	if (psSyncPrimServerExportOUT->eError != PVRSRV_OK)
	{
		goto SyncPrimServerExport_exit;
	}

	psSyncPrimServerExportOUT->eError = PVRSRVAllocHandle(KERNEL_HANDLE_BASE,
							&psSyncPrimServerExportOUT->hExport,
							(void *) psExportInt,
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
			psExportInt = NULL;
		}

		if (psExportInt)
		{
			PVRSRVSyncPrimServerUnexportKM(psExportInt);
		}
	}


	return 0;
}

static IMG_INT
PVRSRVBridgeSyncPrimServerUnexport(IMG_UINT32 ui32DispatchTableEntry,
					  PVRSRV_BRIDGE_IN_SYNCPRIMSERVERUNEXPORT *psSyncPrimServerUnexportIN,
					  PVRSRV_BRIDGE_OUT_SYNCPRIMSERVERUNEXPORT *psSyncPrimServerUnexportOUT,
					 CONNECTION_DATA *psConnection)
{
	SERVER_SYNC_EXPORT * psExportInt = NULL;
	IMG_HANDLE hExportInt = NULL;

	PVR_UNREFERENCED_PARAMETER(psConnection);







	psSyncPrimServerUnexportOUT->eError =
		PVRSRVLookupHandle(KERNEL_HANDLE_BASE,
					(void **) &psExportInt,
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
PVRSRVBridgeSyncPrimServerImport(IMG_UINT32 ui32DispatchTableEntry,
					  PVRSRV_BRIDGE_IN_SYNCPRIMSERVERIMPORT *psSyncPrimServerImportIN,
					  PVRSRV_BRIDGE_OUT_SYNCPRIMSERVERIMPORT *psSyncPrimServerImportOUT,
					 CONNECTION_DATA *psConnection)
{
	SERVER_SYNC_EXPORT * psImportInt = NULL;
	SERVER_SYNC_PRIMITIVE * psSyncHandleInt = NULL;




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
											(void **) &psImportInt,
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
							(void *) psSyncHandleInt,
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


PVRSRV_ERROR InitSYNCEXPORTBridge(void);
PVRSRV_ERROR DeinitSYNCEXPORTBridge(void);

/*
 * Register all SYNCEXPORT functions with services
 */
PVRSRV_ERROR InitSYNCEXPORTBridge(void)
{

	SetDispatchTableEntry(PVRSRV_BRIDGE_SYNCEXPORT, PVRSRV_BRIDGE_SYNCEXPORT_SYNCPRIMSERVEREXPORT, PVRSRVBridgeSyncPrimServerExport,
					NULL, NULL,
					0, 0);

	SetDispatchTableEntry(PVRSRV_BRIDGE_SYNCEXPORT, PVRSRV_BRIDGE_SYNCEXPORT_SYNCPRIMSERVERUNEXPORT, PVRSRVBridgeSyncPrimServerUnexport,
					NULL, NULL,
					0, 0);

	SetDispatchTableEntry(PVRSRV_BRIDGE_SYNCEXPORT, PVRSRV_BRIDGE_SYNCEXPORT_SYNCPRIMSERVERIMPORT, PVRSRVBridgeSyncPrimServerImport,
					NULL, NULL,
					0, 0);


	return PVRSRV_OK;
}

/*
 * Unregister all syncexport functions with services
 */
PVRSRV_ERROR DeinitSYNCEXPORTBridge(void)
{
	return PVRSRV_OK;
}

