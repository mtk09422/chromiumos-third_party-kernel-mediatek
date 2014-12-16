/*************************************************************************/ /*!
@File
@Title          memory allocation header
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@Description    Memory-Allocation API definitions
@License        Strictly Confidential.
*/ /**************************************************************************/

#ifndef __ALLOCMEM_H__
#define __ALLOCMEM_H__

#include "img_types.h"

#if defined (__cplusplus)
extern "C" {
#endif

IMG_PVOID OSAllocMem(IMG_UINT32 ui32Size);

IMG_PVOID OSAllocMemstatMem(IMG_UINT32 ui32Size);

IMG_PVOID OSAllocZMem(IMG_UINT32 ui32Size);

IMG_VOID OSFreeMem(IMG_PVOID pvCpuVAddr);

IMG_VOID OSFreeMemstatMem(IMG_PVOID pvCpuVAddr);

#define OSFREEMEM(_ptr) do \
	{ OSFreeMem((_ptr)); \
		(_ptr) = (IMG_VOID*)0; \
		MSC_SUPPRESS_4127\
	} while (0)


#if defined (__cplusplus)
}
#endif

#endif /* __ALLOCMEM_H__ */

/******************************************************************************
 End of file (allocmem.h)
******************************************************************************/

