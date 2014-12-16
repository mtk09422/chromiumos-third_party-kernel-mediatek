/*************************************************************************/ /*!
@File
@Title          Host memory management implementation for Linux
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@License        Strictly Confidential.
*/ /**************************************************************************/

#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/mm.h>
#include <linux/string.h>

#include "img_defs.h"
#include "allocmem.h"
#if defined(PVRSRV_ENABLE_PROCESS_STATS)
#include "process_stats.h"
#endif

IMG_INTERNAL IMG_PVOID OSAllocMem(IMG_UINT32 ui32Size)
{
	IMG_PVOID pvRet = IMG_NULL;

	if (ui32Size > PVR_LINUX_KMALLOC_ALLOCATION_THRESHOLD)
	{
		pvRet = vmalloc(ui32Size);
	}
	if (pvRet == IMG_NULL)
	{
		pvRet = kmalloc(ui32Size, GFP_KERNEL);
	}

#if defined(PVRSRV_ENABLE_PROCESS_STATS)

	if (pvRet != IMG_NULL)
	{

		if (!is_vmalloc_addr(pvRet))
		{
#if !defined(PVRSRV_ENABLE_MEMORY_STATS)
			PVRSRVStatsIncrMemAllocStat(PVRSRV_MEM_ALLOC_TYPE_KMALLOC, ksize(pvRet));
#else
			{
				IMG_CPU_PHYADDR sCpuPAddr;
				sCpuPAddr.uiAddr = 0;

				PVRSRVStatsAddMemAllocRecord(PVRSRV_MEM_ALLOC_TYPE_KMALLOC,
				                             pvRet,
				                             sCpuPAddr,
				                             ksize(pvRet),
				                             IMG_NULL);
			}
#endif
		}
		else
		{
#if !defined(PVRSRV_ENABLE_MEMORY_STATS)
			PVRSRVStatsIncrMemAllocStatAndTrack(PVRSRV_MEM_ALLOC_TYPE_VMALLOC,
											   ((ui32Size + PAGE_SIZE -1) & ~(PAGE_SIZE-1)),
											   (IMG_UINT64)(IMG_UINTPTR_T) pvRet);
#else
			{
				IMG_CPU_PHYADDR sCpuPAddr;
				sCpuPAddr.uiAddr = 0;

				PVRSRVStatsAddMemAllocRecord(PVRSRV_MEM_ALLOC_TYPE_VMALLOC,
											 pvRet,
											 sCpuPAddr,
											 ((ui32Size + PAGE_SIZE -1) & ~(PAGE_SIZE-1)),
											 IMG_NULL);
			}
#endif
		}

	}
#endif
	return pvRet;
}


IMG_INTERNAL IMG_PVOID OSAllocMemstatMem(IMG_UINT32 ui32Size)
{
	IMG_PVOID pvRet = kmalloc(ui32Size, GFP_KERNEL);

	return pvRet;
}

IMG_INTERNAL IMG_PVOID OSAllocZMem(IMG_UINT32 ui32Size)
{
	IMG_PVOID pvRet = IMG_NULL;

	if (ui32Size > PVR_LINUX_KMALLOC_ALLOCATION_THRESHOLD)
	{
		pvRet = vzalloc(ui32Size);
	}
	if (pvRet == IMG_NULL)
	{
		pvRet = kzalloc(ui32Size, GFP_KERNEL);
	}

#if defined(PVRSRV_ENABLE_PROCESS_STATS)

	if (pvRet != IMG_NULL)
	{

		if (!is_vmalloc_addr(pvRet))
		{
#if !defined(PVRSRV_ENABLE_MEMORY_STATS)
			PVRSRVStatsIncrMemAllocStat(PVRSRV_MEM_ALLOC_TYPE_KMALLOC, ksize(pvRet));
#else
			{
				IMG_CPU_PHYADDR sCpuPAddr;
				sCpuPAddr.uiAddr = 0;

				PVRSRVStatsAddMemAllocRecord(PVRSRV_MEM_ALLOC_TYPE_KMALLOC,
				                             pvRet,
				                             sCpuPAddr,
				                             ksize(pvRet),
				                             IMG_NULL);
			}
#endif
		}
		else
		{
#if !defined(PVRSRV_ENABLE_MEMORY_STATS)
			PVRSRVStatsIncrMemAllocStatAndTrack(PVRSRV_MEM_ALLOC_TYPE_VMALLOC,
											   ((ui32Size + PAGE_SIZE -1) & ~(PAGE_SIZE-1)),
											   (IMG_UINT64)(IMG_UINTPTR_T) pvRet);
#else
			{
				IMG_CPU_PHYADDR sCpuPAddr;
				sCpuPAddr.uiAddr = 0;

				PVRSRVStatsAddMemAllocRecord(PVRSRV_MEM_ALLOC_TYPE_VMALLOC,
											 pvRet,
											 sCpuPAddr,
											 ((ui32Size + PAGE_SIZE -1) & ~(PAGE_SIZE-1)),
											 IMG_NULL);
			}
#endif
		}

	}
#endif
	return pvRet;
}

IMG_INTERNAL void OSFreeMem(IMG_PVOID pvMem)
{

	if ( !is_vmalloc_addr(pvMem) )
	{
#if defined(PVRSRV_ENABLE_PROCESS_STATS)
		if (pvMem != IMG_NULL)
		{
#if !defined(PVRSRV_ENABLE_MEMORY_STATS)
			PVRSRVStatsDecrMemAllocStat(PVRSRV_MEM_ALLOC_TYPE_KMALLOC, ksize(pvMem));
#else
			PVRSRVStatsRemoveMemAllocRecord(PVRSRV_MEM_ALLOC_TYPE_KMALLOC,
			                               (IMG_UINT64)(IMG_UINTPTR_T) pvMem);
#endif
		}
#endif
		kfree(pvMem);
	}
	else
	{
#if defined(PVRSRV_ENABLE_PROCESS_STATS)
		if (pvMem != IMG_NULL)
		{
#if !defined(PVRSRV_ENABLE_MEMORY_STATS)
			PVRSRVStatsDecrMemAllocStatAndUntrack(PVRSRV_MEM_ALLOC_TYPE_VMALLOC,
			                                     (IMG_UINT64)(IMG_UINTPTR_T) pvMem);
#else
			PVRSRVStatsRemoveMemAllocRecord(PVRSRV_MEM_ALLOC_TYPE_VMALLOC,
			                               (IMG_UINT64)(IMG_UINTPTR_T) pvMem);
#endif
		}
#endif
		vfree(pvMem);
	}
}

IMG_INTERNAL void OSFreeMemstatMem(IMG_PVOID pvMem)
{
	kfree(pvMem);
}
