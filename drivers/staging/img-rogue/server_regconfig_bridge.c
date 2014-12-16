/*************************************************************************/ /*!
@File
@Title          Server bridge for regconfig
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@Description    Implements the server side of the bridge for regconfig
@License        Strictly Confidential.
*/ /**************************************************************************/

#include <stddef.h>
#include <asm/uaccess.h>

#include "img_defs.h"

#include "rgxregconfig.h"


#include "common_regconfig_bridge.h"

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
PVRSRVBridgeRGXSetRegConfigPI(IMG_UINT32 ui32BridgeID,
					  PVRSRV_BRIDGE_IN_RGXSETREGCONFIGPI *psRGXSetRegConfigPIIN,
					  PVRSRV_BRIDGE_OUT_RGXSETREGCONFIGPI *psRGXSetRegConfigPIOUT,
					 CONNECTION_DATA *psConnection)
{
	IMG_HANDLE hDevNodeInt = IMG_NULL;

	PVRSRV_BRIDGE_ASSERT_CMD(ui32BridgeID, PVRSRV_BRIDGE_REGCONFIG_RGXSETREGCONFIGPI);







				{
					/* Look up the address from the handle */
					psRGXSetRegConfigPIOUT->eError =
						PVRSRVLookupHandle(psConnection->psHandleBase,
											(IMG_VOID **) &hDevNodeInt,
											psRGXSetRegConfigPIIN->hDevNode,
											PVRSRV_HANDLE_TYPE_DEV_NODE);
					if(psRGXSetRegConfigPIOUT->eError != PVRSRV_OK)
					{
						goto RGXSetRegConfigPI_exit;
					}
				}


	psRGXSetRegConfigPIOUT->eError =
		PVRSRVRGXSetRegConfigPIKM(
					hDevNodeInt,
					psRGXSetRegConfigPIIN->ui8RegPowerIsland);




RGXSetRegConfigPI_exit:

	return 0;
}

static IMG_INT
PVRSRVBridgeRGXAddRegconfig(IMG_UINT32 ui32BridgeID,
					  PVRSRV_BRIDGE_IN_RGXADDREGCONFIG *psRGXAddRegconfigIN,
					  PVRSRV_BRIDGE_OUT_RGXADDREGCONFIG *psRGXAddRegconfigOUT,
					 CONNECTION_DATA *psConnection)
{
	IMG_HANDLE hDevNodeInt = IMG_NULL;

	PVRSRV_BRIDGE_ASSERT_CMD(ui32BridgeID, PVRSRV_BRIDGE_REGCONFIG_RGXADDREGCONFIG);







				{
					/* Look up the address from the handle */
					psRGXAddRegconfigOUT->eError =
						PVRSRVLookupHandle(psConnection->psHandleBase,
											(IMG_VOID **) &hDevNodeInt,
											psRGXAddRegconfigIN->hDevNode,
											PVRSRV_HANDLE_TYPE_DEV_NODE);
					if(psRGXAddRegconfigOUT->eError != PVRSRV_OK)
					{
						goto RGXAddRegconfig_exit;
					}
				}


	psRGXAddRegconfigOUT->eError =
		PVRSRVRGXAddRegConfigKM(
					hDevNodeInt,
					psRGXAddRegconfigIN->ui32RegAddr,
					psRGXAddRegconfigIN->ui64RegValue);




RGXAddRegconfig_exit:

	return 0;
}

static IMG_INT
PVRSRVBridgeRGXClearRegConfig(IMG_UINT32 ui32BridgeID,
					  PVRSRV_BRIDGE_IN_RGXCLEARREGCONFIG *psRGXClearRegConfigIN,
					  PVRSRV_BRIDGE_OUT_RGXCLEARREGCONFIG *psRGXClearRegConfigOUT,
					 CONNECTION_DATA *psConnection)
{
	IMG_HANDLE hDevNodeInt = IMG_NULL;

	PVRSRV_BRIDGE_ASSERT_CMD(ui32BridgeID, PVRSRV_BRIDGE_REGCONFIG_RGXCLEARREGCONFIG);







				{
					/* Look up the address from the handle */
					psRGXClearRegConfigOUT->eError =
						PVRSRVLookupHandle(psConnection->psHandleBase,
											(IMG_VOID **) &hDevNodeInt,
											psRGXClearRegConfigIN->hDevNode,
											PVRSRV_HANDLE_TYPE_DEV_NODE);
					if(psRGXClearRegConfigOUT->eError != PVRSRV_OK)
					{
						goto RGXClearRegConfig_exit;
					}
				}


	psRGXClearRegConfigOUT->eError =
		PVRSRVRGXClearRegConfigKM(
					hDevNodeInt);




RGXClearRegConfig_exit:

	return 0;
}

static IMG_INT
PVRSRVBridgeRGXEnableRegConfig(IMG_UINT32 ui32BridgeID,
					  PVRSRV_BRIDGE_IN_RGXENABLEREGCONFIG *psRGXEnableRegConfigIN,
					  PVRSRV_BRIDGE_OUT_RGXENABLEREGCONFIG *psRGXEnableRegConfigOUT,
					 CONNECTION_DATA *psConnection)
{
	IMG_HANDLE hDevNodeInt = IMG_NULL;

	PVRSRV_BRIDGE_ASSERT_CMD(ui32BridgeID, PVRSRV_BRIDGE_REGCONFIG_RGXENABLEREGCONFIG);







				{
					/* Look up the address from the handle */
					psRGXEnableRegConfigOUT->eError =
						PVRSRVLookupHandle(psConnection->psHandleBase,
											(IMG_VOID **) &hDevNodeInt,
											psRGXEnableRegConfigIN->hDevNode,
											PVRSRV_HANDLE_TYPE_DEV_NODE);
					if(psRGXEnableRegConfigOUT->eError != PVRSRV_OK)
					{
						goto RGXEnableRegConfig_exit;
					}
				}


	psRGXEnableRegConfigOUT->eError =
		PVRSRVRGXEnableRegConfigKM(
					hDevNodeInt);




RGXEnableRegConfig_exit:

	return 0;
}

static IMG_INT
PVRSRVBridgeRGXDisableRegConfig(IMG_UINT32 ui32BridgeID,
					  PVRSRV_BRIDGE_IN_RGXDISABLEREGCONFIG *psRGXDisableRegConfigIN,
					  PVRSRV_BRIDGE_OUT_RGXDISABLEREGCONFIG *psRGXDisableRegConfigOUT,
					 CONNECTION_DATA *psConnection)
{
	IMG_HANDLE hDevNodeInt = IMG_NULL;

	PVRSRV_BRIDGE_ASSERT_CMD(ui32BridgeID, PVRSRV_BRIDGE_REGCONFIG_RGXDISABLEREGCONFIG);







				{
					/* Look up the address from the handle */
					psRGXDisableRegConfigOUT->eError =
						PVRSRVLookupHandle(psConnection->psHandleBase,
											(IMG_VOID **) &hDevNodeInt,
											psRGXDisableRegConfigIN->hDevNode,
											PVRSRV_HANDLE_TYPE_DEV_NODE);
					if(psRGXDisableRegConfigOUT->eError != PVRSRV_OK)
					{
						goto RGXDisableRegConfig_exit;
					}
				}


	psRGXDisableRegConfigOUT->eError =
		PVRSRVRGXDisableRegConfigKM(
					hDevNodeInt);




RGXDisableRegConfig_exit:

	return 0;
}



/* *************************************************************************** 
 * Server bridge dispatch related glue 
 */


PVRSRV_ERROR InitREGCONFIGBridge(IMG_VOID);
PVRSRV_ERROR DeinitREGCONFIGBridge(IMG_VOID);

/*
 * Register all REGCONFIG functions with services
 */
PVRSRV_ERROR InitREGCONFIGBridge(IMG_VOID)
{

	SetDispatchTableEntry(PVRSRV_BRIDGE_REGCONFIG_RGXSETREGCONFIGPI, PVRSRVBridgeRGXSetRegConfigPI,
					IMG_NULL, IMG_NULL,
					0, 0);

	SetDispatchTableEntry(PVRSRV_BRIDGE_REGCONFIG_RGXADDREGCONFIG, PVRSRVBridgeRGXAddRegconfig,
					IMG_NULL, IMG_NULL,
					0, 0);

	SetDispatchTableEntry(PVRSRV_BRIDGE_REGCONFIG_RGXCLEARREGCONFIG, PVRSRVBridgeRGXClearRegConfig,
					IMG_NULL, IMG_NULL,
					0, 0);

	SetDispatchTableEntry(PVRSRV_BRIDGE_REGCONFIG_RGXENABLEREGCONFIG, PVRSRVBridgeRGXEnableRegConfig,
					IMG_NULL, IMG_NULL,
					0, 0);

	SetDispatchTableEntry(PVRSRV_BRIDGE_REGCONFIG_RGXDISABLEREGCONFIG, PVRSRVBridgeRGXDisableRegConfig,
					IMG_NULL, IMG_NULL,
					0, 0);


	return PVRSRV_OK;
}

/*
 * Unregister all regconfig functions with services
 */
PVRSRV_ERROR DeinitREGCONFIGBridge(IMG_VOID)
{
	return PVRSRV_OK;
}

