/*************************************************************************/ /*!
@File
@Title          Services cache management header
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@Description    Defines for cache management which are visible internally only.
@License        Strictly Confidential.
*/ /**************************************************************************/

#ifndef _CACHE_INTERNAL_H_
#define _CACHE_INTERNAL_H_
#include "img_types.h"
#include "pvrsrv_devmem.h"
#include "cache_external.h"

typedef struct _CACHE_BATCH_OP_ENTRY_
{
	IMG_UINT32			ui32PMREntryIndex;
	PVRSRV_CACHE_OP  	eCacheOp;
	IMG_DEVMEM_SIZE_T	uiSize;
    IMG_DEVMEM_OFFSET_T uiOffset;
} CACHE_BATCH_OP_ENTRY;

#endif	/* _CACHE_INTERNAL_H_ */
