/*************************************************************************/ /*!
@File
@Title          Server bridge for timerquery
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@Description    Implements the server side of the bridge for timerquery
@License        Strictly Confidential.
*/ /**************************************************************************/

#include <stddef.h>
#include <asm/uaccess.h>

#include "img_defs.h"

#include "rgxtimerquery.h"


#include "common_timerquery_bridge.h"

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
PVRSRVBridgeRGXBeginTimerQuery(IMG_UINT32 ui32BridgeID,
					  PVRSRV_BRIDGE_IN_RGXBEGINTIMERQUERY *psRGXBeginTimerQueryIN,
					  PVRSRV_BRIDGE_OUT_RGXBEGINTIMERQUERY *psRGXBeginTimerQueryOUT,
					 CONNECTION_DATA *psConnection)
{
	IMG_HANDLE hDevNodeInt = IMG_NULL;

	PVRSRV_BRIDGE_ASSERT_CMD(ui32BridgeID, PVRSRV_BRIDGE_TIMERQUERY_RGXBEGINTIMERQUERY);







				{
					/* Look up the address from the handle */
					psRGXBeginTimerQueryOUT->eError =
						PVRSRVLookupHandle(psConnection->psHandleBase,
											(IMG_VOID **) &hDevNodeInt,
											psRGXBeginTimerQueryIN->hDevNode,
											PVRSRV_HANDLE_TYPE_DEV_NODE);
					if(psRGXBeginTimerQueryOUT->eError != PVRSRV_OK)
					{
						goto RGXBeginTimerQuery_exit;
					}
				}


	psRGXBeginTimerQueryOUT->eError =
		PVRSRVRGXBeginTimerQueryKM(
					hDevNodeInt,
					psRGXBeginTimerQueryIN->ui32QueryId);




RGXBeginTimerQuery_exit:

	return 0;
}

static IMG_INT
PVRSRVBridgeRGXEndTimerQuery(IMG_UINT32 ui32BridgeID,
					  PVRSRV_BRIDGE_IN_RGXENDTIMERQUERY *psRGXEndTimerQueryIN,
					  PVRSRV_BRIDGE_OUT_RGXENDTIMERQUERY *psRGXEndTimerQueryOUT,
					 CONNECTION_DATA *psConnection)
{
	IMG_HANDLE hDevNodeInt = IMG_NULL;

	PVRSRV_BRIDGE_ASSERT_CMD(ui32BridgeID, PVRSRV_BRIDGE_TIMERQUERY_RGXENDTIMERQUERY);







				{
					/* Look up the address from the handle */
					psRGXEndTimerQueryOUT->eError =
						PVRSRVLookupHandle(psConnection->psHandleBase,
											(IMG_VOID **) &hDevNodeInt,
											psRGXEndTimerQueryIN->hDevNode,
											PVRSRV_HANDLE_TYPE_DEV_NODE);
					if(psRGXEndTimerQueryOUT->eError != PVRSRV_OK)
					{
						goto RGXEndTimerQuery_exit;
					}
				}


	psRGXEndTimerQueryOUT->eError =
		PVRSRVRGXEndTimerQueryKM(
					hDevNodeInt);




RGXEndTimerQuery_exit:

	return 0;
}

static IMG_INT
PVRSRVBridgeRGXQueryTimer(IMG_UINT32 ui32BridgeID,
					  PVRSRV_BRIDGE_IN_RGXQUERYTIMER *psRGXQueryTimerIN,
					  PVRSRV_BRIDGE_OUT_RGXQUERYTIMER *psRGXQueryTimerOUT,
					 CONNECTION_DATA *psConnection)
{
	IMG_HANDLE hDevNodeInt = IMG_NULL;

	PVRSRV_BRIDGE_ASSERT_CMD(ui32BridgeID, PVRSRV_BRIDGE_TIMERQUERY_RGXQUERYTIMER);







				{
					/* Look up the address from the handle */
					psRGXQueryTimerOUT->eError =
						PVRSRVLookupHandle(psConnection->psHandleBase,
											(IMG_VOID **) &hDevNodeInt,
											psRGXQueryTimerIN->hDevNode,
											PVRSRV_HANDLE_TYPE_DEV_NODE);
					if(psRGXQueryTimerOUT->eError != PVRSRV_OK)
					{
						goto RGXQueryTimer_exit;
					}
				}


	psRGXQueryTimerOUT->eError =
		PVRSRVRGXQueryTimerKM(
					hDevNodeInt,
					psRGXQueryTimerIN->ui32QueryId,
					&psRGXQueryTimerOUT->ui64StartTime,
					&psRGXQueryTimerOUT->ui64EndTime);




RGXQueryTimer_exit:

	return 0;
}

static IMG_INT
PVRSRVBridgeRGXCurrentTime(IMG_UINT32 ui32BridgeID,
					  PVRSRV_BRIDGE_IN_RGXCURRENTTIME *psRGXCurrentTimeIN,
					  PVRSRV_BRIDGE_OUT_RGXCURRENTTIME *psRGXCurrentTimeOUT,
					 CONNECTION_DATA *psConnection)
{
	IMG_HANDLE hDevNodeInt = IMG_NULL;

	PVRSRV_BRIDGE_ASSERT_CMD(ui32BridgeID, PVRSRV_BRIDGE_TIMERQUERY_RGXCURRENTTIME);







				{
					/* Look up the address from the handle */
					psRGXCurrentTimeOUT->eError =
						PVRSRVLookupHandle(psConnection->psHandleBase,
											(IMG_VOID **) &hDevNodeInt,
											psRGXCurrentTimeIN->hDevNode,
											PVRSRV_HANDLE_TYPE_DEV_NODE);
					if(psRGXCurrentTimeOUT->eError != PVRSRV_OK)
					{
						goto RGXCurrentTime_exit;
					}
				}


	psRGXCurrentTimeOUT->eError =
		PVRSRVRGXCurrentTime(
					hDevNodeInt,
					&psRGXCurrentTimeOUT->ui64Time);




RGXCurrentTime_exit:

	return 0;
}



/* *************************************************************************** 
 * Server bridge dispatch related glue 
 */


PVRSRV_ERROR InitTIMERQUERYBridge(IMG_VOID);
PVRSRV_ERROR DeinitTIMERQUERYBridge(IMG_VOID);

/*
 * Register all TIMERQUERY functions with services
 */
PVRSRV_ERROR InitTIMERQUERYBridge(IMG_VOID)
{

	SetDispatchTableEntry(PVRSRV_BRIDGE_TIMERQUERY_RGXBEGINTIMERQUERY, PVRSRVBridgeRGXBeginTimerQuery,
					IMG_NULL, IMG_NULL,
					0, 0);

	SetDispatchTableEntry(PVRSRV_BRIDGE_TIMERQUERY_RGXENDTIMERQUERY, PVRSRVBridgeRGXEndTimerQuery,
					IMG_NULL, IMG_NULL,
					0, 0);

	SetDispatchTableEntry(PVRSRV_BRIDGE_TIMERQUERY_RGXQUERYTIMER, PVRSRVBridgeRGXQueryTimer,
					IMG_NULL, IMG_NULL,
					0, 0);

	SetDispatchTableEntry(PVRSRV_BRIDGE_TIMERQUERY_RGXCURRENTTIME, PVRSRVBridgeRGXCurrentTime,
					IMG_NULL, IMG_NULL,
					0, 0);


	return PVRSRV_OK;
}

/*
 * Unregister all timerquery functions with services
 */
PVRSRV_ERROR DeinitTIMERQUERYBridge(IMG_VOID)
{
	return PVRSRV_OK;
}

