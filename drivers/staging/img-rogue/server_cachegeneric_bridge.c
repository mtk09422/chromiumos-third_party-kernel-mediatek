/*************************************************************************/ /*!
@File
@Title          Server bridge for cachegeneric
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@Description    Implements the server side of the bridge for cachegeneric
@License        Strictly Confidential.
*/ /**************************************************************************/

#include <stddef.h>
#include <asm/uaccess.h>

#include "img_defs.h"

#include "cache_generic.h"


#include "common_cachegeneric_bridge.h"

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
PVRSRVBridgeCacheOpQueue(IMG_UINT32 ui32BridgeID,
					  PVRSRV_BRIDGE_IN_CACHEOPQUEUE *psCacheOpQueueIN,
					  PVRSRV_BRIDGE_OUT_CACHEOPQUEUE *psCacheOpQueueOUT,
					 CONNECTION_DATA *psConnection)
{

	PVRSRV_BRIDGE_ASSERT_CMD(ui32BridgeID, PVRSRV_BRIDGE_CACHEGENERIC_CACHEOPQUEUE);

	PVR_UNREFERENCED_PARAMETER(psConnection);






	psCacheOpQueueOUT->eError =
		CacheOpQueue(
					psCacheOpQueueIN->iuCacheOp);





	return 0;
}



/* *************************************************************************** 
 * Server bridge dispatch related glue 
 */


PVRSRV_ERROR InitCACHEGENERICBridge(IMG_VOID);
PVRSRV_ERROR DeinitCACHEGENERICBridge(IMG_VOID);

/*
 * Register all CACHEGENERIC functions with services
 */
PVRSRV_ERROR InitCACHEGENERICBridge(IMG_VOID)
{

	SetDispatchTableEntry(PVRSRV_BRIDGE_CACHEGENERIC_CACHEOPQUEUE, PVRSRVBridgeCacheOpQueue,
					IMG_NULL, IMG_NULL,
					0, 0);


	return PVRSRV_OK;
}

/*
 * Unregister all cachegeneric functions with services
 */
PVRSRV_ERROR DeinitCACHEGENERICBridge(IMG_VOID)
{
	return PVRSRV_OK;
}

