/*************************************************************************/ /*!
@File
@Title          Server bridge for breakpoint
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@Description    Implements the server side of the bridge for breakpoint
@License        Strictly Confidential.
*/ /**************************************************************************/

#include <stddef.h>
#include <asm/uaccess.h>

#include "img_defs.h"

#include "rgxbreakpoint.h"


#include "common_breakpoint_bridge.h"

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
PVRSRVBridgeRGXSetBreakpoint(IMG_UINT32 ui32BridgeID,
					  PVRSRV_BRIDGE_IN_RGXSETBREAKPOINT *psRGXSetBreakpointIN,
					  PVRSRV_BRIDGE_OUT_RGXSETBREAKPOINT *psRGXSetBreakpointOUT,
					 CONNECTION_DATA *psConnection)
{
	IMG_HANDLE hDevNodeInt = IMG_NULL;
	IMG_HANDLE hPrivDataInt = IMG_NULL;

	PVRSRV_BRIDGE_ASSERT_CMD(ui32BridgeID, PVRSRV_BRIDGE_BREAKPOINT_RGXSETBREAKPOINT);







				{
					/* Look up the address from the handle */
					psRGXSetBreakpointOUT->eError =
						PVRSRVLookupHandle(psConnection->psHandleBase,
											(IMG_VOID **) &hDevNodeInt,
											psRGXSetBreakpointIN->hDevNode,
											PVRSRV_HANDLE_TYPE_DEV_NODE);
					if(psRGXSetBreakpointOUT->eError != PVRSRV_OK)
					{
						goto RGXSetBreakpoint_exit;
					}
				}


				{
					/* Look up the address from the handle */
					psRGXSetBreakpointOUT->eError =
						PVRSRVLookupHandle(psConnection->psHandleBase,
											(IMG_VOID **) &hPrivDataInt,
											psRGXSetBreakpointIN->hPrivData,
											PVRSRV_HANDLE_TYPE_DEV_PRIV_DATA);
					if(psRGXSetBreakpointOUT->eError != PVRSRV_OK)
					{
						goto RGXSetBreakpoint_exit;
					}
				}


	psRGXSetBreakpointOUT->eError =
		PVRSRVRGXSetBreakpointKM(
					hDevNodeInt,
					hPrivDataInt,
					psRGXSetBreakpointIN->eFWDataMaster,
					psRGXSetBreakpointIN->ui32BreakpointAddr,
					psRGXSetBreakpointIN->ui32HandlerAddr,
					psRGXSetBreakpointIN->ui32DM);




RGXSetBreakpoint_exit:

	return 0;
}

static IMG_INT
PVRSRVBridgeRGXClearBreakpoint(IMG_UINT32 ui32BridgeID,
					  PVRSRV_BRIDGE_IN_RGXCLEARBREAKPOINT *psRGXClearBreakpointIN,
					  PVRSRV_BRIDGE_OUT_RGXCLEARBREAKPOINT *psRGXClearBreakpointOUT,
					 CONNECTION_DATA *psConnection)
{
	IMG_HANDLE hDevNodeInt = IMG_NULL;
	IMG_HANDLE hPrivDataInt = IMG_NULL;

	PVRSRV_BRIDGE_ASSERT_CMD(ui32BridgeID, PVRSRV_BRIDGE_BREAKPOINT_RGXCLEARBREAKPOINT);







				{
					/* Look up the address from the handle */
					psRGXClearBreakpointOUT->eError =
						PVRSRVLookupHandle(psConnection->psHandleBase,
											(IMG_VOID **) &hDevNodeInt,
											psRGXClearBreakpointIN->hDevNode,
											PVRSRV_HANDLE_TYPE_DEV_NODE);
					if(psRGXClearBreakpointOUT->eError != PVRSRV_OK)
					{
						goto RGXClearBreakpoint_exit;
					}
				}


				{
					/* Look up the address from the handle */
					psRGXClearBreakpointOUT->eError =
						PVRSRVLookupHandle(psConnection->psHandleBase,
											(IMG_VOID **) &hPrivDataInt,
											psRGXClearBreakpointIN->hPrivData,
											PVRSRV_HANDLE_TYPE_DEV_PRIV_DATA);
					if(psRGXClearBreakpointOUT->eError != PVRSRV_OK)
					{
						goto RGXClearBreakpoint_exit;
					}
				}


	psRGXClearBreakpointOUT->eError =
		PVRSRVRGXClearBreakpointKM(
					hDevNodeInt,
					hPrivDataInt);




RGXClearBreakpoint_exit:

	return 0;
}

static IMG_INT
PVRSRVBridgeRGXEnableBreakpoint(IMG_UINT32 ui32BridgeID,
					  PVRSRV_BRIDGE_IN_RGXENABLEBREAKPOINT *psRGXEnableBreakpointIN,
					  PVRSRV_BRIDGE_OUT_RGXENABLEBREAKPOINT *psRGXEnableBreakpointOUT,
					 CONNECTION_DATA *psConnection)
{
	IMG_HANDLE hDevNodeInt = IMG_NULL;
	IMG_HANDLE hPrivDataInt = IMG_NULL;

	PVRSRV_BRIDGE_ASSERT_CMD(ui32BridgeID, PVRSRV_BRIDGE_BREAKPOINT_RGXENABLEBREAKPOINT);







				{
					/* Look up the address from the handle */
					psRGXEnableBreakpointOUT->eError =
						PVRSRVLookupHandle(psConnection->psHandleBase,
											(IMG_VOID **) &hDevNodeInt,
											psRGXEnableBreakpointIN->hDevNode,
											PVRSRV_HANDLE_TYPE_DEV_NODE);
					if(psRGXEnableBreakpointOUT->eError != PVRSRV_OK)
					{
						goto RGXEnableBreakpoint_exit;
					}
				}


				{
					/* Look up the address from the handle */
					psRGXEnableBreakpointOUT->eError =
						PVRSRVLookupHandle(psConnection->psHandleBase,
											(IMG_VOID **) &hPrivDataInt,
											psRGXEnableBreakpointIN->hPrivData,
											PVRSRV_HANDLE_TYPE_DEV_PRIV_DATA);
					if(psRGXEnableBreakpointOUT->eError != PVRSRV_OK)
					{
						goto RGXEnableBreakpoint_exit;
					}
				}


	psRGXEnableBreakpointOUT->eError =
		PVRSRVRGXEnableBreakpointKM(
					hDevNodeInt,
					hPrivDataInt);




RGXEnableBreakpoint_exit:

	return 0;
}

static IMG_INT
PVRSRVBridgeRGXDisableBreakpoint(IMG_UINT32 ui32BridgeID,
					  PVRSRV_BRIDGE_IN_RGXDISABLEBREAKPOINT *psRGXDisableBreakpointIN,
					  PVRSRV_BRIDGE_OUT_RGXDISABLEBREAKPOINT *psRGXDisableBreakpointOUT,
					 CONNECTION_DATA *psConnection)
{
	IMG_HANDLE hDevNodeInt = IMG_NULL;
	IMG_HANDLE hPrivDataInt = IMG_NULL;

	PVRSRV_BRIDGE_ASSERT_CMD(ui32BridgeID, PVRSRV_BRIDGE_BREAKPOINT_RGXDISABLEBREAKPOINT);







				{
					/* Look up the address from the handle */
					psRGXDisableBreakpointOUT->eError =
						PVRSRVLookupHandle(psConnection->psHandleBase,
											(IMG_VOID **) &hDevNodeInt,
											psRGXDisableBreakpointIN->hDevNode,
											PVRSRV_HANDLE_TYPE_DEV_NODE);
					if(psRGXDisableBreakpointOUT->eError != PVRSRV_OK)
					{
						goto RGXDisableBreakpoint_exit;
					}
				}


				{
					/* Look up the address from the handle */
					psRGXDisableBreakpointOUT->eError =
						PVRSRVLookupHandle(psConnection->psHandleBase,
											(IMG_VOID **) &hPrivDataInt,
											psRGXDisableBreakpointIN->hPrivData,
											PVRSRV_HANDLE_TYPE_DEV_PRIV_DATA);
					if(psRGXDisableBreakpointOUT->eError != PVRSRV_OK)
					{
						goto RGXDisableBreakpoint_exit;
					}
				}


	psRGXDisableBreakpointOUT->eError =
		PVRSRVRGXDisableBreakpointKM(
					hDevNodeInt,
					hPrivDataInt);




RGXDisableBreakpoint_exit:

	return 0;
}

static IMG_INT
PVRSRVBridgeRGXOverallocateBPRegisters(IMG_UINT32 ui32BridgeID,
					  PVRSRV_BRIDGE_IN_RGXOVERALLOCATEBPREGISTERS *psRGXOverallocateBPRegistersIN,
					  PVRSRV_BRIDGE_OUT_RGXOVERALLOCATEBPREGISTERS *psRGXOverallocateBPRegistersOUT,
					 CONNECTION_DATA *psConnection)
{
	IMG_HANDLE hDevNodeInt = IMG_NULL;

	PVRSRV_BRIDGE_ASSERT_CMD(ui32BridgeID, PVRSRV_BRIDGE_BREAKPOINT_RGXOVERALLOCATEBPREGISTERS);







				{
					/* Look up the address from the handle */
					psRGXOverallocateBPRegistersOUT->eError =
						PVRSRVLookupHandle(psConnection->psHandleBase,
											(IMG_VOID **) &hDevNodeInt,
											psRGXOverallocateBPRegistersIN->hDevNode,
											PVRSRV_HANDLE_TYPE_DEV_NODE);
					if(psRGXOverallocateBPRegistersOUT->eError != PVRSRV_OK)
					{
						goto RGXOverallocateBPRegisters_exit;
					}
				}


	psRGXOverallocateBPRegistersOUT->eError =
		PVRSRVRGXOverallocateBPRegistersKM(
					hDevNodeInt,
					psRGXOverallocateBPRegistersIN->ui32TempRegs,
					psRGXOverallocateBPRegistersIN->ui32SharedRegs);




RGXOverallocateBPRegisters_exit:

	return 0;
}



/* *************************************************************************** 
 * Server bridge dispatch related glue 
 */


PVRSRV_ERROR InitBREAKPOINTBridge(IMG_VOID);
PVRSRV_ERROR DeinitBREAKPOINTBridge(IMG_VOID);

/*
 * Register all BREAKPOINT functions with services
 */
PVRSRV_ERROR InitBREAKPOINTBridge(IMG_VOID)
{

	SetDispatchTableEntry(PVRSRV_BRIDGE_BREAKPOINT_RGXSETBREAKPOINT, PVRSRVBridgeRGXSetBreakpoint,
					IMG_NULL, IMG_NULL,
					0, 0);

	SetDispatchTableEntry(PVRSRV_BRIDGE_BREAKPOINT_RGXCLEARBREAKPOINT, PVRSRVBridgeRGXClearBreakpoint,
					IMG_NULL, IMG_NULL,
					0, 0);

	SetDispatchTableEntry(PVRSRV_BRIDGE_BREAKPOINT_RGXENABLEBREAKPOINT, PVRSRVBridgeRGXEnableBreakpoint,
					IMG_NULL, IMG_NULL,
					0, 0);

	SetDispatchTableEntry(PVRSRV_BRIDGE_BREAKPOINT_RGXDISABLEBREAKPOINT, PVRSRVBridgeRGXDisableBreakpoint,
					IMG_NULL, IMG_NULL,
					0, 0);

	SetDispatchTableEntry(PVRSRV_BRIDGE_BREAKPOINT_RGXOVERALLOCATEBPREGISTERS, PVRSRVBridgeRGXOverallocateBPRegisters,
					IMG_NULL, IMG_NULL,
					0, 0);


	return PVRSRV_OK;
}

/*
 * Unregister all breakpoint functions with services
 */
PVRSRV_ERROR DeinitBREAKPOINTBridge(IMG_VOID)
{
	return PVRSRV_OK;
}

