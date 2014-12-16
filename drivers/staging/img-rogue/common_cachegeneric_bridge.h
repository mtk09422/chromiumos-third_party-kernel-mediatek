/*************************************************************************/ /*!
@File
@Title          Common bridge header for cachegeneric
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@Description    Declares common defines and structures that are used by both
                the client and sever side of the bridge for cachegeneric
@License        Strictly Confidential.
*/ /**************************************************************************/

#ifndef COMMON_CACHEGENERIC_BRIDGE_H
#define COMMON_CACHEGENERIC_BRIDGE_H

#include "cache_external.h"


#include "pvr_bridge_io.h"

#define PVRSRV_BRIDGE_CACHEGENERIC_CMD_FIRST			(PVRSRV_BRIDGE_CACHEGENERIC_START)
#define PVRSRV_BRIDGE_CACHEGENERIC_CACHEOPQUEUE			PVRSRV_IOWR(PVRSRV_BRIDGE_CACHEGENERIC_CMD_FIRST+0)
#define PVRSRV_BRIDGE_CACHEGENERIC_CMD_LAST			(PVRSRV_BRIDGE_CACHEGENERIC_CMD_FIRST+0)


/*******************************************
            CacheOpQueue          
 *******************************************/

/* Bridge in structure for CacheOpQueue */
typedef struct PVRSRV_BRIDGE_IN_CACHEOPQUEUE_TAG
{
	PVRSRV_CACHE_OP iuCacheOp;
} __attribute__((packed)) PVRSRV_BRIDGE_IN_CACHEOPQUEUE;


/* Bridge out structure for CacheOpQueue */
typedef struct PVRSRV_BRIDGE_OUT_CACHEOPQUEUE_TAG
{
	PVRSRV_ERROR eError;
} __attribute__((packed)) PVRSRV_BRIDGE_OUT_CACHEOPQUEUE;

#endif /* COMMON_CACHEGENERIC_BRIDGE_H */
