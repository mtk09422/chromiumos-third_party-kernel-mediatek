/*************************************************************************/ /*!
@File
@Title          CPU generic cache management
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@Description    Implements server side code for CPU cache management in a
                CPU agnostic manner.
@License        Strictly Confidential.
*/ /**************************************************************************/
#include "cache_generic.h"
#include "cache_internal.h"
#include "device.h"
#include "pvr_debug.h"
#include "pvrsrv.h"
#include "osfunc.h"
#include "pmr.h"

PVRSRV_ERROR CacheOpQueue(PVRSRV_CACHE_OP uiCacheOp)
{
	PVRSRV_DATA *psData = PVRSRVGetPVRSRVData();

	psData->uiCacheOp = SetCacheOp(psData->uiCacheOp, uiCacheOp);
	return PVRSRV_OK;
}
