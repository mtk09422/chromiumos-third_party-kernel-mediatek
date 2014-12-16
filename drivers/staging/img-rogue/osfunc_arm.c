/*************************************************************************/ /*!
@File
@Title          arm specific OS functions
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@Description    OS functions who's implementation are processor specific
@License        Strictly Confidential.
*/ /**************************************************************************/
#include <linux/version.h>
#include <linux/dma-mapping.h>
//#include <asm/system.h>
#include <asm/cacheflush.h>

#include "pvrsrv_error.h"
#include "img_types.h"
#include "osfunc.h"
#include "pvr_debug.h"

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,27))
#define ON_EACH_CPU(func, info, wait) on_each_cpu(func, info, wait)
#else
#define ON_EACH_CPU(func, info, wait) on_each_cpu(func, info, 0, wait)
#endif

static void per_cpu_cache_flush(void *arg)
{
	PVR_UNREFERENCED_PARAMETER(arg);
	flush_cache_all();
}

void OSCPUOperation(PVRSRV_CACHE_OP uiCacheOp)
{
	switch(uiCacheOp)
	{
		/* Fall-through */
		case PVRSRV_CACHE_OP_CLEAN:
					/* No full (inner) cache clean op */
					ON_EACH_CPU(per_cpu_cache_flush, NULL, 1);
#if defined(CONFIG_OUTER_CACHE)
					outer_clean_range(0, ULONG_MAX);
#endif
					break;

		case PVRSRV_CACHE_OP_FLUSH:
					ON_EACH_CPU(per_cpu_cache_flush, NULL, 1);
#if defined(CONFIG_OUTER_CACHE) && \
	(LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,37))
					/* To use the "deferred flush" (not clean) DDK feature you need a kernel
					 * implementation of outer_flush_all() for ARM CPUs with an outer cache
					 * controller (e.g. PL310, common with Cortex A9 and later).
					 *
					 * Reference DDKs don't require this functionality, as they will only
					 * clean the cache, never flush (clean+invalidate) it.
					 */
					outer_flush_all();
#endif
					break;

		case PVRSRV_CACHE_OP_NONE:
					break;

		default:
					PVR_DPF((PVR_DBG_ERROR,
					"%s: Invalid cache operation type %d",
					__FUNCTION__, uiCacheOp));
					PVR_ASSERT(0);
					break;
	}
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,34))
static inline size_t pvr_dmac_range_len(const void *pvStart, const void *pvEnd)
{
	return (size_t)((char *)pvEnd - (char *)pvStart);
}
#endif

void OSFlushCPUCacheRangeKM(IMG_PVOID pvVirtStart,
							IMG_PVOID pvVirtEnd,
							IMG_CPU_PHYADDR sCPUPhysStart,
							IMG_CPU_PHYADDR sCPUPhysEnd)
{
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,7,0))
	arm_dma_ops.sync_single_for_device(NULL, sCPUPhysStart.uiAddr, sCPUPhysEnd.uiAddr - sCPUPhysStart.uiAddr, DMA_TO_DEVICE);
	arm_dma_ops.sync_single_for_cpu(NULL, sCPUPhysStart.uiAddr, sCPUPhysEnd.uiAddr - sCPUPhysStart.uiAddr, DMA_FROM_DEVICE);
#else	/* (LINUX_VERSION_CODE >= KERNEL_VERSION(3,7,0)) */
	/* Inner cache */
	dmac_flush_range(pvVirtStart, pvVirtEnd);

	/* Outer cache */
	outer_flush_range(sCPUPhysStart.uiAddr, sCPUPhysEnd.uiAddr);
#endif	/* (LINUX_VERSION_CODE >= KERNEL_VERSION(3,7,0)) */
}

void OSCleanCPUCacheRangeKM(IMG_PVOID pvVirtStart,
							IMG_PVOID pvVirtEnd,
							IMG_CPU_PHYADDR sCPUPhysStart,
							IMG_CPU_PHYADDR sCPUPhysEnd)
{
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,7,0))
	arm_dma_ops.sync_single_for_device(NULL, sCPUPhysStart.uiAddr, sCPUPhysEnd.uiAddr - sCPUPhysStart.uiAddr, DMA_TO_DEVICE);
#else	/* (LINUX_VERSION_CODE >= KERNEL_VERSION(3,7,0)) */
	/* Inner cache */
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,34))
	dmac_clean_range(pvVirtStart, pvVirtEnd);
#else
	dmac_map_area(pvVirtStart, pvr_dmac_range_len(pvVirtStart, pvVirtEnd), DMA_TO_DEVICE);
#endif

	/* Outer cache */
	outer_clean_range(sCPUPhysStart.uiAddr, sCPUPhysEnd.uiAddr);
#endif	/* (LINUX_VERSION_CODE >= KERNEL_VERSION(3,7,0)) */
}

void OSInvalidateCPUCacheRangeKM(IMG_PVOID pvVirtStart,
								 IMG_PVOID pvVirtEnd,
								 IMG_CPU_PHYADDR sCPUPhysStart,
								 IMG_CPU_PHYADDR sCPUPhysEnd)
{
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,7,0))
	arm_dma_ops.sync_single_for_cpu(NULL, sCPUPhysStart.uiAddr, sCPUPhysEnd.uiAddr - sCPUPhysStart.uiAddr, DMA_FROM_DEVICE);
#else	/* (LINUX_VERSION_CODE >= KERNEL_VERSION(3,7,0)) */
#if defined(PVR_LINUX_DONT_USE_RANGE_BASED_INVALIDATE)
	OSCleanCPUCacheRangeKM(pvVirtStart, pvVirtEnd, sCPUPhysStart, sCPUPhysEnd);
#else
	/* Inner cache */
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,34))
	dmac_inv_range(pvVirtStart, pvVirtEnd);
#else
	dmac_map_area(pvVirtStart, pvr_dmac_range_len(pvVirtStart, pvVirtEnd), DMA_FROM_DEVICE);
#endif

	/* Outer cache */
	outer_inv_range(sCPUPhysStart.uiAddr, sCPUPhysEnd.uiAddr);
#endif
#endif	/* (LINUX_VERSION_CODE >= KERNEL_VERSION(3,7,0)) */
}
