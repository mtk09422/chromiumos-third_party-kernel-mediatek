/*************************************************************************/ /*!
@File
@Title          Server bridge for smm
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@Description    Implements the server side of the bridge for smm
@License        Strictly Confidential.
*/ /**************************************************************************/

#include <stddef.h>
#include <asm/uaccess.h>

#include "img_defs.h"

#include "pmr.h"
#include "secure_export.h"


#include "common_smm_bridge.h"

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
PVRSRVBridgePMRSecureExportPMR(IMG_UINT32 ui32BridgeID,
					  PVRSRV_BRIDGE_IN_PMRSECUREEXPORTPMR *psPMRSecureExportPMRIN,
					  PVRSRV_BRIDGE_OUT_PMRSECUREEXPORTPMR *psPMRSecureExportPMROUT,
					 CONNECTION_DATA *psConnection)
{
	PMR * psPMRInt = IMG_NULL;
	PMR * psPMROutInt = IMG_NULL;
	IMG_HANDLE hPMROutInt = IMG_NULL;
	CONNECTION_DATA *psSecureConnection;

	PVRSRV_BRIDGE_ASSERT_CMD(ui32BridgeID, PVRSRV_BRIDGE_SMM_PMRSECUREEXPORTPMR);





	PMRLock();


				{
					/* Look up the address from the handle */
					psPMRSecureExportPMROUT->eError =
						PVRSRVLookupHandle(psConnection->psHandleBase,
											(IMG_VOID **) &psPMRInt,
											psPMRSecureExportPMRIN->hPMR,
											PVRSRV_HANDLE_TYPE_PHYSMEM_PMR);
					if(psPMRSecureExportPMROUT->eError != PVRSRV_OK)
					{
						PMRUnlock();
						goto PMRSecureExportPMR_exit;
					}
				}


	psPMRSecureExportPMROUT->eError =
		PMRSecureExportPMR(psConnection,
					psPMRInt,
					&psPMRSecureExportPMROUT->Export,
					&psPMROutInt, &psSecureConnection);
	/* Exit early if bridged call fails */
	if(psPMRSecureExportPMROUT->eError != PVRSRV_OK)
	{
		PMRUnlock();
		goto PMRSecureExportPMR_exit;
	}
	PMRUnlock();


	psPMRSecureExportPMROUT->eError = PVRSRVAllocHandle(psSecureConnection->psHandleBase,
							&hPMROutInt,
							(IMG_VOID *) psPMROutInt,
							PVRSRV_HANDLE_TYPE_PHYSMEM_PMR_SECURE_EXPORT,
							PVRSRV_HANDLE_ALLOC_FLAG_SHARED
							,(PFN_HANDLE_RELEASE)&PMRSecureUnexportPMR);
	if (psPMRSecureExportPMROUT->eError != PVRSRV_OK)
	{
		goto PMRSecureExportPMR_exit;
	}




PMRSecureExportPMR_exit:
	if (psPMRSecureExportPMROUT->eError != PVRSRV_OK)
	{
		if (psPMROutInt)
		{
			PMRSecureUnexportPMR(psPMROutInt);
		}
	}


	return 0;
}

static IMG_INT
PVRSRVBridgePMRSecureUnexportPMR(IMG_UINT32 ui32BridgeID,
					  PVRSRV_BRIDGE_IN_PMRSECUREUNEXPORTPMR *psPMRSecureUnexportPMRIN,
					  PVRSRV_BRIDGE_OUT_PMRSECUREUNEXPORTPMR *psPMRSecureUnexportPMROUT,
					 CONNECTION_DATA *psConnection)
{

	PVRSRV_BRIDGE_ASSERT_CMD(ui32BridgeID, PVRSRV_BRIDGE_SMM_PMRSECUREUNEXPORTPMR);





	PMRLock();




	psPMRSecureUnexportPMROUT->eError =
		PVRSRVReleaseHandle(psConnection->psHandleBase,
					(IMG_HANDLE) psPMRSecureUnexportPMRIN->hPMR,
					PVRSRV_HANDLE_TYPE_PHYSMEM_PMR_SECURE_EXPORT);
	if ((psPMRSecureUnexportPMROUT->eError != PVRSRV_OK) && (psPMRSecureUnexportPMROUT->eError != PVRSRV_ERROR_RETRY))
	{
		PVR_ASSERT(0);
		PMRUnlock();
		goto PMRSecureUnexportPMR_exit;
	}

	PMRUnlock();


PMRSecureUnexportPMR_exit:

	return 0;
}

static IMG_INT
PVRSRVBridgePMRSecureImportPMR(IMG_UINT32 ui32BridgeID,
					  PVRSRV_BRIDGE_IN_PMRSECUREIMPORTPMR *psPMRSecureImportPMRIN,
					  PVRSRV_BRIDGE_OUT_PMRSECUREIMPORTPMR *psPMRSecureImportPMROUT,
					 CONNECTION_DATA *psConnection)
{
	PMR * psPMRInt = IMG_NULL;

	PVRSRV_BRIDGE_ASSERT_CMD(ui32BridgeID, PVRSRV_BRIDGE_SMM_PMRSECUREIMPORTPMR);





	PMRLock();


	psPMRSecureImportPMROUT->eError =
		PMRSecureImportPMR(
					psPMRSecureImportPMRIN->Export,
					&psPMRInt,
					&psPMRSecureImportPMROUT->uiSize,
					&psPMRSecureImportPMROUT->sAlign);
	/* Exit early if bridged call fails */
	if(psPMRSecureImportPMROUT->eError != PVRSRV_OK)
	{
		PMRUnlock();
		goto PMRSecureImportPMR_exit;
	}
	PMRUnlock();


	psPMRSecureImportPMROUT->eError = PVRSRVAllocHandle(psConnection->psHandleBase,
							&psPMRSecureImportPMROUT->hPMR,
							(IMG_VOID *) psPMRInt,
							PVRSRV_HANDLE_TYPE_PHYSMEM_PMR,
							PVRSRV_HANDLE_ALLOC_FLAG_MULTI
							,(PFN_HANDLE_RELEASE)&PMRUnrefPMR);
	if (psPMRSecureImportPMROUT->eError != PVRSRV_OK)
	{
		goto PMRSecureImportPMR_exit;
	}




PMRSecureImportPMR_exit:
	if (psPMRSecureImportPMROUT->eError != PVRSRV_OK)
	{
		if (psPMRInt)
		{
			PMRUnrefPMR(psPMRInt);
		}
	}


	return 0;
}



/* *************************************************************************** 
 * Server bridge dispatch related glue 
 */


PVRSRV_ERROR InitSMMBridge(IMG_VOID);
PVRSRV_ERROR DeinitSMMBridge(IMG_VOID);

/*
 * Register all SMM functions with services
 */
PVRSRV_ERROR InitSMMBridge(IMG_VOID)
{

	SetDispatchTableEntry(PVRSRV_BRIDGE_SMM_PMRSECUREEXPORTPMR, PVRSRVBridgePMRSecureExportPMR,
					IMG_NULL, IMG_NULL,
					0, 0);

	SetDispatchTableEntry(PVRSRV_BRIDGE_SMM_PMRSECUREUNEXPORTPMR, PVRSRVBridgePMRSecureUnexportPMR,
					IMG_NULL, IMG_NULL,
					0, 0);

	SetDispatchTableEntry(PVRSRV_BRIDGE_SMM_PMRSECUREIMPORTPMR, PVRSRVBridgePMRSecureImportPMR,
					IMG_NULL, IMG_NULL,
					0, 0);


	return PVRSRV_OK;
}

/*
 * Unregister all smm functions with services
 */
PVRSRV_ERROR DeinitSMMBridge(IMG_VOID)
{
	return PVRSRV_OK;
}

