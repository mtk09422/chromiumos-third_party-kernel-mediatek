/*************************************************************************/ /*!
@File
@Title          Implementation of PMR functions for OS managed memory
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@Description    Part of the memory management.  This module is responsible for
                implementing the function callbacks for physical memory borrowed
                from that normally managed by the operating system.
@License        Dual MIT/GPLv2

The contents of this file are subject to the MIT license as set out below.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

Alternatively, the contents of this file may be used under the terms of
the GNU General Public License Version 2 ("GPL") in which case the provisions
of GPL are applicable instead of those above.

If you wish to allow use of your version of this file only under the terms of
GPL, and not to allow others to use your version of this file under the terms
of the MIT license, indicate your decision by deleting the provisions above
and replace them with the notice and other provisions required by GPL as set
out in the file called "GPL-COPYING" included in this distribution. If you do
not delete the provisions above, a recipient may use your version of this file
under the terms of either the MIT license or GPL.

This License is also included in this distribution in the file called
"MIT-COPYING".

EXCEPT AS OTHERWISE STATED IN A NEGOTIATED AGREEMENT: (A) THE SOFTWARE IS
PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
PURPOSE AND NONINFRINGEMENT; AND (B) IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/ /**************************************************************************/

/* include5/ */
#include "img_types.h"
#include "pvr_debug.h"
#include "pvrsrv_error.h"
#include "pvrsrv_memallocflags.h"
#include "rgx_pdump_panics.h"
/* services/server/include/ */
#include "allocmem.h"
#include "osfunc.h"
#include "pdump_physmem.h"
#include "pdump_km.h"
#include "pmr.h"
#include "pmr_impl.h"
#include "devicemem_server_utils.h"

/* ourselves */
#include "physmem_osmem.h"

#if defined(PVRSRV_ENABLE_PROCESS_STATS)
#include "process_stats.h"
#endif

#include <linux/version.h>

#if (LINUX_VERSION_CODE > KERNEL_VERSION(3,0,0))
#include <linux/mm.h>
#define PHYSMEM_SUPPORTS_SHRINKER
#endif

#include <linux/slab.h>
#include <linux/highmem.h>
#include <linux/mm_types.h>
#include <linux/vmalloc.h>
#include <linux/gfp.h>
#include <linux/sched.h>
#include <asm/io.h>
#if defined(CONFIG_X86)
#include <asm/cacheflush.h>
#endif

#if defined(CONFIG_X86)
#define PMR_UNSET_PAGES_STACK_ALLOC 64
#endif

/* Provide SHRINK_STOP definition for kernel older than 3.12 */
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,12,0))
#define SHRINK_STOP (~0UL)
#endif

#if  (LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,0))
static IMG_UINT32 g_uiMaxOrder = PVR_LINUX_PHYSMEM_MAX_ALLOC_ORDER_NUM;
#else
/* split_page not available on older kernels */
#undef PVR_LINUX_PHYSMEM_MAX_ALLOC_ORDER_NUM
#define PVR_LINUX_PHYSMEM_MAX_ALLOC_ORDER_NUM 0
static IMG_UINT32 g_uiMaxOrder = 0;
#endif

struct _PMR_OSPAGEARRAY_DATA_ {
    /*
     * iNumPagesAllocated:
     * Number of pages allocated in this PMR so far.
     * Don't think more 8G memory will be used in one PMR
     */
    IMG_INT32 iNumPagesAllocated;

    /*
     * uiTotalNumPages:
     * Total number of pages supported by this PMR. (Fixed as of now due the fixed Page table array size)
     *  number of "pages" (a.k.a. macro pages, compound pages, higher order pages, etc...)
     */
    IMG_UINT32 uiTotalNumPages;

    /*
     * uiTotalNumPages:
     * Total number of pages supported by this PMR. (Fixed as of now due the fixed Page table array size)
     */
    IMG_UINT32 uiPagesToAlloc;

    /*
      uiLog2PageSize;

      size of each "page" -- this would normally be the same as
      PAGE_SHIFT, but we support the idea that we may allocate pages
      in larger chunks for better contiguity, using order>0 in the
      call to alloc_pages()
    */
    IMG_UINT32 uiLog2DevPageSize;

    /*
      the pages thusly allocated...  N.B.. One entry per compound page,
      where compound pages are used.
    */
    struct page **pagearray;

    /*
      for pdump...
    */
    IMG_BOOL bPDumpMalloced;
    IMG_HANDLE hPDumpAllocInfo;

    /*
      record at alloc time whether poisoning will be required when the
      PMR is freed.
    */
    IMG_BOOL bZero;
    IMG_BOOL bPoisonOnFree;
    IMG_BOOL bPoisonOnAlloc;
    IMG_BOOL bHasOSPages;
    IMG_BOOL bOnDemand;
    IMG_BOOL bUnpinned;
    /*
	 The cache mode of the PMR (required at free time)
	 Boolean used to track if we need to revert the cache attributes
	 of the pages used in this allocation. Depends on OS/architecture.
	*/
    IMG_UINT32 ui32CPUCacheFlags;
	IMG_BOOL bUnsetMemoryType;
};

/***********************************
 * Page pooling for uncached pages *
 ***********************************/
 
static void
_FreeOSPage(IMG_UINT32 ui32CPUCacheFlags,
			IMG_UINT32 uiOrder,
			IMG_BOOL bUnsetMemoryType,
			IMG_BOOL bFreeToOS,
			struct page *psPage);

static PVRSRV_ERROR
_FreeOSPages(struct _PMR_OSPAGEARRAY_DATA_ *psPageArrayData,
									IMG_UINT32 *pai32FreeIndices,
									IMG_UINT32 ui32FreePageCount);

typedef	struct
{
	/* Linkage for page pool LRU list */
	struct list_head sPagePoolItem;

	struct page *psPage;
} LinuxPagePoolEntry;

typedef struct
{
	struct list_head sUnpinPoolItem;
	struct _PMR_OSPAGEARRAY_DATA_ *psPageArrayDataPtr;
} LinuxUnpinEntry;


/* Track what is live */
static IMG_UINT32 g_ui32LiveAllocs = 0;

static IMG_UINT32 g_ui32UnpinPageCount = 0;

static IMG_UINT32 g_ui32PagePoolEntryCount = 0;

#if defined(PVR_LINUX_PHYSMEM_MAX_POOL_PAGES)
static IMG_UINT32 g_ui32PagePoolMaxEntries = PVR_LINUX_PHYSMEM_MAX_POOL_PAGES;
#else
static IMG_UINT32 g_ui32PagePoolMaxEntries = 0;
#endif

/* Global structures we use to manage the page pool */
static DEFINE_MUTEX(g_sPagePoolMutex);

static struct kmem_cache *g_psLinuxPagePoolCache = NULL;

static LIST_HEAD(g_sPagePoolList);
static LIST_HEAD(g_sUncachedPagePoolList);
static LIST_HEAD(g_sUnpinList);

static inline void
_PagePoolLock(void)
{
	mutex_lock(&g_sPagePoolMutex);
}

static inline int
_PagePoolTrylock(void)
{
	return mutex_trylock(&g_sPagePoolMutex);
}

static inline void
_PagePoolUnlock(void)
{
	mutex_unlock(&g_sPagePoolMutex);
}

static PVRSRV_ERROR
_AddUnpinListEntryUnlocked(struct _PMR_OSPAGEARRAY_DATA_ *psOSPageArrayData)
{
	LinuxUnpinEntry *psUnpinEntry;

	psUnpinEntry = OSAllocMem(sizeof(LinuxUnpinEntry));
	if (!psUnpinEntry)
	{
		PVR_DPF((PVR_DBG_ERROR,
				"%s: OSAllocMem failed. Cannot add entry to unpin list.",
				__func__));
		return PVRSRV_ERROR_OUT_OF_MEMORY;
	}

	psUnpinEntry->psPageArrayDataPtr = psOSPageArrayData;

	/* Add into pool that the shrinker can access easily*/
	list_add_tail(&psUnpinEntry->sUnpinPoolItem, &g_sUnpinList);

	g_ui32UnpinPageCount += psOSPageArrayData->iNumPagesAllocated;

	return PVRSRV_OK;
}

static void
_RemoveUnpinListEntryUnlocked(struct _PMR_OSPAGEARRAY_DATA_ *psOSPageArrayData)
{
	LinuxUnpinEntry *psUnpinEntry, *psTempUnpinEntry;

	/* Remove from pool */
	list_for_each_entry_safe(psUnpinEntry,
	                         psTempUnpinEntry,
	                         &g_sUnpinList,
	                         sUnpinPoolItem)
	{
		if (psUnpinEntry->psPageArrayDataPtr == psOSPageArrayData)
		{
			list_del(&psUnpinEntry->sUnpinPoolItem);
			break;
		}
	}

	OSFreeMem(psUnpinEntry);

	g_ui32UnpinPageCount -= psOSPageArrayData->iNumPagesAllocated;
}


static LinuxPagePoolEntry *
_LinuxPagePoolEntryAlloc(void)
{
    return kmem_cache_zalloc(g_psLinuxPagePoolCache, GFP_KERNEL);
}

static inline IMG_BOOL _GetPoolListHead(IMG_UINT32 ui32CPUCacheFlags, struct list_head **ppsPoolHead)
{
	switch(ui32CPUCacheFlags)
	{
		case PVRSRV_MEMALLOCFLAG_CPU_UNCACHED:
/*
	For x86 we need to keep different lists for uncached
	and write-combined as we must always honour the PAT
	setting which cares about this difference.
*/
#if defined(CONFIG_X86)
			*ppsPoolHead = &g_sUncachedPagePoolList;
			break;
#else
			/* Fall-through */
#endif
		case PVRSRV_MEMALLOCFLAG_CPU_WRITE_COMBINE:
			*ppsPoolHead = &g_sPagePoolList;
			break;
		default:
			return IMG_FALSE;
	}
	return IMG_TRUE;
}

static void
_LinuxPagePoolEntryFree(LinuxPagePoolEntry *psPagePoolEntry)
{
	kmem_cache_free(g_psLinuxPagePoolCache, psPagePoolEntry);
}

static inline IMG_BOOL
_AddEntryToPool(struct page *psPage, IMG_UINT32 ui32CPUCacheFlags)
{
	LinuxPagePoolEntry *psEntry;
	struct list_head *psPoolHead = NULL;

	if (!_GetPoolListHead(ui32CPUCacheFlags, &psPoolHead))
	{
		return IMG_FALSE;
	}

	psEntry = _LinuxPagePoolEntryAlloc();
	if (psEntry == NULL)
	{
		return IMG_FALSE;
	}

	psEntry->psPage = psPage;
	_PagePoolLock();
	list_add_tail(&psEntry->sPagePoolItem, psPoolHead);
	g_ui32PagePoolEntryCount++;
	_PagePoolUnlock();

#if defined(PVRSRV_ENABLE_PROCESS_STATS)
	PVRSRVStatsIncrMemAllocPoolStat(PAGE_SIZE);
#endif

	return IMG_TRUE;
}

static inline void
_RemoveEntryFromPoolUnlocked(LinuxPagePoolEntry *psPagePoolEntry)
{
#if defined(PVRSRV_ENABLE_PROCESS_STATS)
	PVRSRVStatsDecrMemAllocPoolStat(PAGE_SIZE);
#endif

	list_del(&psPagePoolEntry->sPagePoolItem);
	g_ui32PagePoolEntryCount--;
}

static inline struct page *
_RemoveFirstEntryFromPool(IMG_UINT32 ui32CPUCacheFlags)
{
	LinuxPagePoolEntry *psPagePoolEntry;
	struct page *psPage;
	struct list_head *psPoolHead = NULL;

	if (!_GetPoolListHead(ui32CPUCacheFlags, &psPoolHead))
	{
		return NULL;
	}

	_PagePoolLock();
	if (list_empty(psPoolHead))
	{
		_PagePoolUnlock();
		return NULL;
	}

	PVR_ASSERT(g_ui32PagePoolEntryCount > 0);
	psPagePoolEntry = list_first_entry(psPoolHead, LinuxPagePoolEntry, sPagePoolItem);
	_RemoveEntryFromPoolUnlocked(psPagePoolEntry);
	psPage = psPagePoolEntry->psPage;
	_LinuxPagePoolEntryFree(psPagePoolEntry);
	_PagePoolUnlock();

	return psPage;
}

#if defined(PHYSMEM_SUPPORTS_SHRINKER)
static struct shrinker g_sShrinker;

static unsigned long
_CountObjectsInPagePool(struct shrinker *psShrinker, struct shrink_control *psShrinkControl)
{
	int remain;

	PVR_ASSERT(psShrinker == &g_sShrinker);
	(void)psShrinker;
	(void)psShrinkControl;

	/* In order to avoid possible deadlock use mutex_trylock in place of mutex_lock */
	if (_PagePoolTrylock() == 0)
		return 0;
	remain = g_ui32PagePoolEntryCount + g_ui32UnpinPageCount;
	_PagePoolUnlock();

	return remain;
}

static unsigned long
_ScanObjectsInPagePool(struct shrinker *psShrinker, struct shrink_control *psShrinkControl)
{
	unsigned long uNumToScan = psShrinkControl->nr_to_scan;
	unsigned long uSurplus = 0;
	LinuxPagePoolEntry *psPagePoolEntry, *psTempPoolEntry;
	LinuxUnpinEntry *psUnpinEntry, *psTempUnpinEntry;

	PVR_ASSERT(psShrinker == &g_sShrinker);
	(void)psShrinker;

	/* In order to avoid possible deadlock use mutex_trylock in place of mutex_lock */
	if (_PagePoolTrylock() == 0)
		return SHRINK_STOP;
	list_for_each_entry_safe(psPagePoolEntry,
	                         psTempPoolEntry,
	                         &g_sPagePoolList,
	                         sPagePoolItem)
	{
		_RemoveEntryFromPoolUnlocked(psPagePoolEntry);

		/*
		  We don't want to save the cache type and is we need to unset the
		  memory type as it would double the page pool structure and the
		  values are always going to be the same anyway which is why the
		  page is in the pool (well the page could be UNCACHED or
		  WRITE_COMBINE but we don't even need the cache type for freeing
		  back to the OS).
		*/
		_FreeOSPage(PVRSRV_MEMALLOCFLAG_CPU_WRITE_COMBINE,
			    0,
			    IMG_TRUE,
			    IMG_TRUE,
			    psPagePoolEntry->psPage);
		_LinuxPagePoolEntryFree(psPagePoolEntry);

		if (--uNumToScan == 0)
		{
			goto e_exit;
		}
	}

	/*
	  Note:
	  For anything other then x86 this list will be empty but we want to
	  keep differences between compiled code to a minimum and so
	  this isn't wrapped in #if defined(CONFIG_X86)
	*/
	list_for_each_entry_safe(psPagePoolEntry,
	                         psTempPoolEntry,
	                         &g_sUncachedPagePoolList,
	                         sPagePoolItem)
	{
		_RemoveEntryFromPoolUnlocked(psPagePoolEntry);

		/*
		  We don't want to save the cache type and is we need to unset the
		  memory type as it would double the page pool structure and the
		  values are always going to be the same anyway which is why the
		  page is in the pool (well the page could be UNCACHED or
		  WRITE_COMBINE but we don't even need the cache type for freeing
		  back to the OS).
		*/
		_FreeOSPage(PVRSRV_MEMALLOCFLAG_CPU_UNCACHED,
			    0,
			    IMG_TRUE,
			    IMG_TRUE,
			    psPagePoolEntry->psPage);
		_LinuxPagePoolEntryFree(psPagePoolEntry);

		if (--uNumToScan == 0)
		{
			goto e_exit;
		}
	}

	/* Free unpinned memory, starting with LRU entries */
	list_for_each_entry_safe(psUnpinEntry,
	                         psTempUnpinEntry,
	                         &g_sUnpinList,
	                         sUnpinPoolItem)
	{
		struct _PMR_OSPAGEARRAY_DATA_ *psPageArrayDataPtr = psUnpinEntry->psPageArrayDataPtr;
		IMG_UINT32 uiNumPages = (psPageArrayDataPtr->uiTotalNumPages > psPageArrayDataPtr->iNumPagesAllocated)? \
								psPageArrayDataPtr->iNumPagesAllocated:psPageArrayDataPtr->uiTotalNumPages;
		PVRSRV_ERROR eError;

		/* Free associated pages */
		eError = _FreeOSPages(psPageArrayDataPtr,NULL,psPageArrayDataPtr->uiTotalNumPages);
		if (eError != PVRSRV_OK)
		{
			PVR_DPF((PVR_DBG_ERROR,
					"%s: Shrinker is unable to free unpinned pages. Error: %s (%d)",
					 __FUNCTION__,
					 PVRSRVGetErrorStringKM(eError),
					 eError));
			goto e_exit;
		}

		/* Remove item from pool */
		list_del(&psUnpinEntry->sUnpinPoolItem);

		g_ui32UnpinPageCount -= uiNumPages;

		/* Check if there is more to free or if we already surpassed the limit */
		if (uiNumPages < uNumToScan)
		{
			uNumToScan -= uiNumPages;

		}
		else if (uiNumPages > uNumToScan)
		{
			uSurplus += uiNumPages - uNumToScan;
			uNumToScan = 0;
			goto e_exit;
		}
		else
		{
			uNumToScan -= uiNumPages;
			goto e_exit;
		}
	}

e_exit:
	if (list_empty(&g_sPagePoolList) && list_empty(&g_sUncachedPagePoolList))
	{
		PVR_ASSERT(g_ui32PagePoolEntryCount == 0);
	}
	if (list_empty(&g_sUnpinList))
	{
		PVR_ASSERT(g_ui32UnpinPageCount == 0);
	}

#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,12,0))
	{
		/* Returning the number of pages that still reside in the pool */
		int remain;
		remain = g_ui32PagePoolEntryCount + g_ui32UnpinPageCount;
		_PagePoolUnlock();
		return remain;
	}
#else
	/* Returning the  number of pages freed during the scan */
	_PagePoolUnlock();
	return psShrinkControl->nr_to_scan - uNumToScan + uSurplus;
#endif
}

#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,12,0))
static int
_ShrinkPagePool(struct shrinker *psShrinker, struct shrink_control *psShrinkControl)
{
	if (psShrinkControl->nr_to_scan != 0)
	{
		return _ScanObjectsInPagePool(psShrinker, psShrinkControl);
	}
	else
	{
		/* No pages are being reclaimed so just return the page count */
		return _CountObjectsInPagePool(psShrinker, psShrinkControl);
	}
}

static struct shrinker g_sShrinker =
{
	.shrink = _ShrinkPagePool,
	.seeks = DEFAULT_SEEKS
};
#else
static struct shrinker g_sShrinker =
{
	.count_objects = _CountObjectsInPagePool,
	.scan_objects = _ScanObjectsInPagePool,
	.seeks = DEFAULT_SEEKS
};
#endif
#endif /* defined(PHYSMEM_SUPPORTS_SHRINKER) */

static void DisableOOMKiller(void)
{
	/* PF_DUMPCORE is treated by the VM as if the OOM killer was disabled.
	 *
	 * As oom_killer_disable() is an inline, non-exported function, we
	 * can't use it from a modular driver. Furthermore, the OOM killer
	 * API doesn't look thread safe, which `current' is.
	 */
	WARN_ON(current->flags & PF_DUMPCORE);
	current->flags |= PF_DUMPCORE;
}

static void _InitPagePool(void)
{
	IMG_UINT32 ui32Flags = 0;

	_PagePoolLock();
#if defined(DEBUG_LINUX_SLAB_ALLOCATIONS)
	ui32Flags |= SLAB_POISON|SLAB_RED_ZONE;
#endif
	g_psLinuxPagePoolCache = kmem_cache_create("img-pp", sizeof(LinuxPagePoolEntry), 0, ui32Flags, NULL);

#if defined(PHYSMEM_SUPPORTS_SHRINKER)
	/* Only create the shrinker if we created the cache OK */
	if (g_psLinuxPagePoolCache)
	{
		register_shrinker(&g_sShrinker);
	}
#endif
	_PagePoolUnlock();
}

static void _DeinitPagePool(void)
{
	LinuxPagePoolEntry *psPagePoolEntry, *psTempPPEntry;

	_PagePoolLock();
	/* Evict all the pages from the pool */
	list_for_each_entry_safe(psPagePoolEntry,
	                         psTempPPEntry,
	                         &g_sPagePoolList,
	                         sPagePoolItem)
	{
		_RemoveEntryFromPoolUnlocked(psPagePoolEntry);

		/*
			We don't want to save the cache type and is we need to unset the
			memory type as it would double the page pool structure and the
			values are always going to be the same anyway which is why the
			page is in the pool (well the page could be UNCACHED or
			WRITE_COMBINE but we don't even need the cache type for freeing
			back to the OS).
		*/
		_FreeOSPage(PVRSRV_MEMALLOCFLAG_CPU_WRITE_COMBINE,
					0,
					IMG_TRUE,
					IMG_TRUE,
					psPagePoolEntry->psPage);
		_LinuxPagePoolEntryFree(psPagePoolEntry);
	}
	
	/*
		Note:
		For anything other then x86 this will be a no-op but we want to
		keep differences between compiled code to a minimum and so
		this isn't wrapped in #if defined(CONFIG_X86)
	*/
	list_for_each_entry_safe(psPagePoolEntry,
	                         psTempPPEntry,
	                         &g_sUncachedPagePoolList,
	                         sPagePoolItem)
	{
		_RemoveEntryFromPoolUnlocked(psPagePoolEntry);

		_FreeOSPage(PVRSRV_MEMALLOCFLAG_CPU_UNCACHED,
					0,
					IMG_TRUE,
					IMG_TRUE,
					psPagePoolEntry->psPage);
		_LinuxPagePoolEntryFree(psPagePoolEntry);
	}

	PVR_ASSERT(g_ui32PagePoolEntryCount == 0);

	/* Free the page cache */
	kmem_cache_destroy(g_psLinuxPagePoolCache);

#if defined(PHYSMEM_SUPPORTS_SHRINKER)
	unregister_shrinker(&g_sShrinker);
#endif
	_PagePoolUnlock();
}

static void EnableOOMKiller(void)
{
	current->flags &= ~PF_DUMPCORE;
}

static void
_PoisonPages(struct page *page,
             IMG_UINT32 uiOrder,
             const IMG_CHAR *pacPoisonData,
             size_t uiPoisonSize)
{
    void *kvaddr;
    IMG_UINT32 uiSrcByteIndex;
    IMG_UINT32 uiDestByteIndex;
    IMG_UINT32 uiSubPageIndex;
    IMG_CHAR *pcDest;

    uiSrcByteIndex = 0;
    for (uiSubPageIndex = 0; uiSubPageIndex < (1U << uiOrder); uiSubPageIndex++)
    {
        kvaddr = kmap(page + uiSubPageIndex);

        pcDest = kvaddr;

        for(uiDestByteIndex=0; uiDestByteIndex<PAGE_SIZE; uiDestByteIndex++)
        {
            pcDest[uiDestByteIndex] = pacPoisonData[uiSrcByteIndex];
            uiSrcByteIndex++;
            if (uiSrcByteIndex == uiPoisonSize)
            {
                uiSrcByteIndex = 0;
            }
        }
        kunmap(page + uiSubPageIndex);
    }
}

static const IMG_CHAR _AllocPoison[] = "^PoIsOn";
static const IMG_UINT32 _AllocPoisonSize = 7;
static const IMG_CHAR _FreePoison[] = "<DEAD-BEEF>";
static const IMG_UINT32 _FreePoisonSize = 11;

static PVRSRV_ERROR
_AllocOSPageArray(PMR_SIZE_T uiChunkSize,
        IMG_UINT32 ui32NumPhysChunks,
        IMG_UINT32 ui32NumVirtChunks,
        IMG_UINT32 uiLog2DevPageSize,
        IMG_BOOL bZero,
        IMG_BOOL bPoisonOnAlloc,
        IMG_BOOL bPoisonOnFree,
        IMG_BOOL bOnDemand,
        IMG_UINT32 ui32CPUCacheFlags,
		struct _PMR_OSPAGEARRAY_DATA_ **ppsPageArrayDataPtr)
{
    PVRSRV_ERROR eError;
    void *pvData;
    PMR_SIZE_T uiSize = uiChunkSize * ui32NumVirtChunks;
    IMG_UINT32 ui32NumPhysPages=0, ui32NumVirtPages=0, i=0;

    struct page **ppsPageArray;
    struct _PMR_OSPAGEARRAY_DATA_ *psPageArrayData;

    if (uiSize >= 0x1000000000ULL)
    {
        PVR_DPF((PVR_DBG_ERROR,
                 "physmem_osmem_linux.c: Do you really want 64GB of physical memory in one go?  This is likely a bug"));
        eError = PVRSRV_ERROR_INVALID_PARAMS;
        goto e_freed_pvdata;
    }

    PVR_ASSERT(PAGE_SHIFT <= uiLog2DevPageSize);
    if ((uiSize & ((1ULL << uiLog2DevPageSize) - 1)) != 0)
    {
    	PVR_DPF((PVR_DBG_ERROR,
    			"Allocation size "PMR_SIZE_FMTSPEC" is not multiple of page size 2^%u !",
    			uiSize,
			uiLog2DevPageSize));

        eError = PVRSRV_ERROR_PMR_NOT_PAGE_MULTIPLE;
        goto e_freed_pvdata;
    }

   /* Use of cast below is justified by the assertion that follows to
       prove that no significant bits have been truncated */
    ui32NumVirtPages = (IMG_UINT32)(((uiSize-1)>>PAGE_SHIFT) + 1);
    PVR_ASSERT(((PMR_SIZE_T)ui32NumVirtPages << PAGE_SHIFT) == uiSize);

    pvData = OSAllocMem(sizeof(struct _PMR_OSPAGEARRAY_DATA_) +
                     sizeof(struct page *) * ui32NumVirtPages);
    if (pvData == NULL)
    {
        PVR_DPF((PVR_DBG_ERROR,
                 "physmem_osmem_linux.c: OS refused the memory allocation for the table of pages.  Did you ask for too much?"));
        eError = PVRSRV_ERROR_INVALID_PARAMS;
        goto e_freed_pvdata;
    }
    PVR_ASSERT(pvData != NULL);

    ui32NumPhysPages = (IMG_UINT32)((((ui32NumPhysChunks * uiChunkSize)-1)>>PAGE_SHIFT) + 1);
    PVR_ASSERT(((PMR_SIZE_T)ui32NumPhysPages << PAGE_SHIFT) == (ui32NumPhysChunks * uiChunkSize));

    psPageArrayData = pvData;
    ppsPageArray = pvData + sizeof(struct _PMR_OSPAGEARRAY_DATA_);
    psPageArrayData->pagearray = ppsPageArray;
    psPageArrayData->uiLog2DevPageSize = uiLog2DevPageSize;
    psPageArrayData->uiTotalNumPages = ui32NumVirtPages;
    /*No pages are allocated yet */
    psPageArrayData->iNumPagesAllocated = 0;
    psPageArrayData->uiPagesToAlloc = ui32NumPhysPages;
    psPageArrayData->bZero = bZero;
    psPageArrayData->ui32CPUCacheFlags = ui32CPUCacheFlags;
 	psPageArrayData->bPoisonOnAlloc = bPoisonOnAlloc;
 	psPageArrayData->bPoisonOnFree = bPoisonOnFree;
 	psPageArrayData->bHasOSPages = IMG_FALSE;
 	psPageArrayData->bOnDemand = bOnDemand;
 	psPageArrayData->bUnpinned = IMG_FALSE;

    psPageArrayData->bPDumpMalloced = IMG_FALSE;

	psPageArrayData->bUnsetMemoryType = IMG_FALSE;

	*ppsPageArrayDataPtr = psPageArrayData;
    for(i=0;i<ui32NumVirtPages;i++)
    {
    	ppsPageArray[i] = (struct page *)(INVALID_PAGE);
    }

	return PVRSRV_OK;

e_freed_pvdata:
   PVR_ASSERT(eError != PVRSRV_OK);
   return eError;

}

static inline PVRSRV_ERROR
_ApplyOSPagesAttribute(struct page **ppsPage, IMG_UINT32 uiNumPages, IMG_BOOL bFlush,
#if defined (CONFIG_X86)
					   struct page **ppsUnsetPages, IMG_UINT32 uiUnsetPagesIndex,
#endif
					   IMG_UINT32 ui32Order, IMG_UINT32 ui32CPUCacheFlags, unsigned int gfp_flags,
					   IMG_UINT32 *pui32MapTable)
{
	PVRSRV_ERROR eError = PVRSRV_OK;
	
	if (ppsPage != NULL)
	{
#if defined (CONFIG_ARM) || defined(CONFIG_ARM64) || defined (CONFIG_METAG)
		/*  On ARM kernels we can be given pages which still remain in the cache.
			In order to make sure that the data we write through our mappings
			doesn't get over written by later cache evictions we invalidate the
			pages that get given to us.
	
			Note:
			This still seems to be true if we request cold pages, it's just less
			likely to be in the cache. */
		IMG_UINT32 ui32Idx;
		IMG_UINT32 uiPageIndex =0;
		
		if (ui32CPUCacheFlags != PVRSRV_MEMALLOCFLAG_CPU_CACHED)
		{
			for (ui32Idx = 0; ui32Idx < uiNumPages;  ++ui32Idx)
			{
				IMG_CPU_PHYADDR sCPUPhysAddrStart, sCPUPhysAddrEnd;
				void *pvPageVAddr;

		        if(NULL != pui32MapTable)
		        {
					/* The Page Map Table points to the indices */
					uiPageIndex = pui32MapTable[ui32Idx];
		        }else{
		        	uiPageIndex = ui32Idx;
		        }

				pvPageVAddr = kmap(ppsPage[uiPageIndex]);
				sCPUPhysAddrStart.uiAddr = page_to_phys(ppsPage[uiPageIndex]);
				sCPUPhysAddrEnd.uiAddr = sCPUPhysAddrStart.uiAddr + PAGE_SIZE;

				/* If we're zeroing, we need to make sure the cleared memory is pushed out
				   of the cache before the cache lines are invalidated */
				if (bFlush)
				{
					OSFlushCPUCacheRangeKM(pvPageVAddr,
										   pvPageVAddr + PAGE_SIZE,
										   sCPUPhysAddrStart,
										   sCPUPhysAddrEnd);
				}
				else
				{
					OSInvalidateCPUCacheRangeKM(pvPageVAddr,
												pvPageVAddr + PAGE_SIZE,
												sCPUPhysAddrStart,
												sCPUPhysAddrEnd);
				}
				
				kunmap(ppsPage[uiPageIndex]);
			}
		}
#endif
#if defined (CONFIG_X86)
		/*  On X86 if we already have a mapping we need to change the mode of
			current mapping before we map it ourselves	*/
		int ret = IMG_FALSE;
		IMG_UINT32 uiPageIndex, uiPageCount=0;
		PVR_UNREFERENCED_PARAMETER(bFlush);

		switch (ui32CPUCacheFlags)
		{
			case PVRSRV_MEMALLOCFLAG_CPU_UNCACHED:
				ret = set_pages_array_uc(ppsUnsetPages, uiUnsetPagesIndex);
				if (ret)
				{
					eError = PVRSRV_ERROR_UNABLE_TO_SET_CACHE_MODE;
					PVR_DPF((PVR_DBG_ERROR, "Setting Linux page caching mode to UC failed, returned %d", ret));
				}
				break;

			case PVRSRV_MEMALLOCFLAG_CPU_WRITE_COMBINE:
				ret = set_pages_array_wc(ppsUnsetPages, uiUnsetPagesIndex);
				if (ret)
				{
					eError = PVRSRV_ERROR_UNABLE_TO_SET_CACHE_MODE;
					PVR_DPF((PVR_DBG_ERROR, "Setting Linux page caching mode to WC failed, returned %d", ret));
				}
				break;

			case PVRSRV_MEMALLOCFLAG_CPU_CACHED:
				break;

			default:
				break;
		}

		if (ret)
		{
			for(uiPageCount = 0; uiPageCount < uiNumPages; uiPageCount++)
			{
				if(NULL != pui32MapTable)
				{
					/* The Page Map Table points to the indices */
					uiPageIndex = pui32MapTable[uiPageCount];
				 }else{
				   	uiPageIndex = uiPageCount;
				 }
				_FreeOSPage(ui32CPUCacheFlags,
							ui32Order,
							IMG_FALSE,
							IMG_FALSE,
							ppsPage[uiPageIndex]);
			}
		}
#endif
	}
	
	return eError;
}

static PVRSRV_ERROR
_AllocOSPage(IMG_UINT32 ui32CPUCacheFlags,
             unsigned int gfp_flags,
             IMG_UINT32 uiOrder,
             struct page **ppsPage,
             IMG_UINT32 uiPageIndex,
             IMG_BOOL *pbPageFromPool,
             IMG_UINT32 *pui32MapTable)
{
	struct page *psPage = NULL;
	IMG_UINT32 ui32Count;
	*pbPageFromPool = IMG_FALSE;

	/* Does the requested page contiguity match the CPU page size? */
	if (uiOrder == 0)
	{
		psPage = _RemoveFirstEntryFromPool(ui32CPUCacheFlags);
		if (psPage != NULL)
		{
			*pbPageFromPool = IMG_TRUE;
			if (gfp_flags & __GFP_ZERO)
			{
				/* The kernel will zero the page for us when we allocate it,
				   but if it comes from the pool then we must do this 
				   ourselves. */
				void *pvPageVAddr = kmap(psPage);
				memset(pvPageVAddr, 0, PAGE_SIZE);
				kunmap(psPage);
			}
		}
	}

	/*  Did we check the page pool and/or was it a page pool miss,
		either the pool was empty or it was for a cached page so we
		must ask the OS  */
	if (!*pbPageFromPool)
	{
		DisableOOMKiller();
		psPage = alloc_pages(gfp_flags, uiOrder);
		EnableOOMKiller();
	}

	if(psPage == NULL)
	{
		return PVRSRV_ERROR_OUT_OF_MEMORY; 
	}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,0))
	if (uiOrder != 0)
	{
		split_page(psPage, uiOrder);
	}
#endif

	for (ui32Count=0; ui32Count < (1 << uiOrder); ui32Count++)
	{	/* We must manually calculate page addresses if asking for more than one */
		IMG_UINT32 ui32Idx = (pui32MapTable) ? pui32MapTable[uiPageIndex + ui32Count] : uiPageIndex + ui32Count;

		ppsPage[ui32Idx] = &psPage[ui32Count];
	}

	return PVRSRV_OK;
}

/* Allocation of OS pages: We may allocate N^order pages at a time for two reasons.
 * Firstly to support device pages which are larger the OS. By asking the OS for 2^Order
 * OS pages at a time we guarantee the device page is contiguous. Secondly for performance
 * where we may ask for N^order pages to reduce the number of calls to alloc_pages, and
 * thus reduce time for huge allocations. Regardless of page order requested, we need to
 * break them down to track _OS pages. The maximum order requested is increased if all
 * max order allocations were successful. If any request fails we reduce the max order.
 * In addition, we must also handle sparse allocations: allocations which provide only a
 * proportion of sparse physical backing within the total virtual range. Currently
 * we only support sparse allocations on device pages that are OS page sized.
 */
static PVRSRV_ERROR
_AllocOSPages(struct _PMR_OSPAGEARRAY_DATA_ *psPageArrayData,
					   IMG_UINT32 ui32CPUCacheFlags,
					   IMG_UINT32 ui32MinOrder,
					   unsigned int gfp_flags,
					   IMG_UINT32 *pui32MapTable)
{
	PVRSRV_ERROR eError;
	IMG_UINT32 uiPageIndex = 0;
	IMG_BOOL bPageFromPool = IMG_FALSE;
	IMG_UINT32 ui32Order;
	IMG_UINT32 ui32NumPageReq;
	IMG_UINT32 ui32GfpFlags;
	IMG_UINT32 ui32HighOrderGfpFlags = ((gfp_flags & ~__GFP_WAIT) | __GFP_NORETRY);
	struct page **ppsPageArray = psPageArrayData->pagearray;
	IMG_BOOL bIncreaseMaxOrder = IMG_TRUE;

#if defined(CONFIG_X86)
	/* On x86 we batch applying cache attributes by storing references to all
	   pages that are not from the page pool */
	struct page *apsUnsetPages[PMR_UNSET_PAGES_STACK_ALLOC];
	struct page **ppsUnsetPages = apsUnsetPages;
	IMG_UINT32 uiUnsetPagesIndex = 0;

	if (psPageArrayData->uiPagesToAlloc > PMR_UNSET_PAGES_STACK_ALLOC)
	{
		ppsUnsetPages = OSAllocMem(sizeof(struct page*) * psPageArrayData->uiPagesToAlloc);
		if (ppsUnsetPages == NULL)
		{
			PVR_DPF((PVR_DBG_ERROR, "%s: Failed alloc_pages metadata allocation", __FUNCTION__));
			return PVRSRV_ERROR_OUT_OF_MEMORY;
		}
	}
#endif

	if (psPageArrayData->uiPagesToAlloc < PVR_LINUX_HIGHORDER_ALLOCATION_THRESHOLD)
	{	/* Small allocations: Ask for one device page at a time */
		ui32Order = ui32MinOrder;
		bIncreaseMaxOrder = IMG_FALSE;
	}
	else
	{	/* Large allocations: Ask for many device pages at a time */
		ui32Order = MAX(g_uiMaxOrder, ui32MinOrder);
	}

	/* Only if asking for more contiguity than we actually need, let it fail */
	ui32GfpFlags = (ui32Order > ui32MinOrder) ? ui32HighOrderGfpFlags : gfp_flags;
	ui32NumPageReq = (1 << ui32Order);

	for (uiPageIndex=0; uiPageIndex < psPageArrayData->uiPagesToAlloc; )
	{
		IMG_UINT32 ui32PageRemain = psPageArrayData->uiPagesToAlloc - uiPageIndex;

		while (ui32NumPageReq > ui32PageRemain)
		{	/* Pages to request is larger than that remaining. Ask for less */
			ui32Order = MAX(ui32Order >> 1,ui32MinOrder);
			ui32NumPageReq = (1 << ui32Order);
			ui32GfpFlags = (ui32Order > ui32MinOrder) ? ui32HighOrderGfpFlags : gfp_flags;
		}

		/* Alloc uiOrder pages at uiPageIndex */
		eError = _AllocOSPage(ui32CPUCacheFlags, ui32GfpFlags, ui32Order,
							  ppsPageArray, uiPageIndex, &bPageFromPool, pui32MapTable);

		if (eError == PVRSRV_OK)
		{
#if defined(CONFIG_X86)
			if (!bPageFromPool)
			{
				IMG_UINT32 uiCount;

				for (uiCount=0; uiCount < ui32NumPageReq; uiCount++)
				{
					IMG_UINT32 ui32Idx = (pui32MapTable) ? pui32MapTable[uiPageIndex+uiCount] : uiPageIndex+uiCount;

					ppsUnsetPages[uiUnsetPagesIndex] = ppsPageArray[ui32Idx];
					uiUnsetPagesIndex++;
				}
			}
#endif
			/* Successful request. Move onto next. */
			uiPageIndex += ui32NumPageReq;
		}
		else
		{
			if (ui32Order > ui32MinOrder)
			{
				/* Last request failed. Let's ask for less next time */
				ui32Order = MAX(ui32Order >> 1,ui32MinOrder);
				bIncreaseMaxOrder = IMG_FALSE;
				ui32NumPageReq = (1 << ui32Order);
				ui32GfpFlags = (ui32Order > ui32MinOrder) ? ui32HighOrderGfpFlags : gfp_flags;
				g_uiMaxOrder = ui32Order;
			}
			else
			{
				/* Failed to alloc pages at required contiguity. Failed allocation */
				PVR_DPF((PVR_DBG_ERROR, "%s: alloc_pages failed to honour request at %d of %d (%s)",
								__FUNCTION__,
								uiPageIndex,
								psPageArrayData->uiPagesToAlloc,
								PVRSRVGetErrorStringKM(eError)));
				goto e_error_oom_exit;
			}
		}
	}

	if (bIncreaseMaxOrder && (ui32Order < PVR_LINUX_PHYSMEM_MAX_ALLOC_ORDER_NUM))
	{	/* All successful allocations on max order. Let's ask for more next time */
		g_uiMaxOrder++;
	}

	/* Do the cache management as required */
	eError = _ApplyOSPagesAttribute (ppsPageArray,
					 psPageArrayData->uiPagesToAlloc,
					 psPageArrayData->bZero,
#if defined(CONFIG_X86)
					 ppsUnsetPages,
					 uiUnsetPagesIndex,
#endif
					 ui32Order,
					 ui32CPUCacheFlags,
					 gfp_flags,
					 pui32MapTable);

	if (eError == PVRSRV_OK)
	{
		goto e_exit;
	}

	PVR_DPF((PVR_DBG_ERROR, "Failed to to set page attributes"));

e_error_oom_exit:
{
	IMG_UINT32 ui32PageToFree;

	for(ui32PageToFree = 0;ui32PageToFree < uiPageIndex; ui32PageToFree++)
	{
		IMG_UINT32 ui32Idx = (pui32MapTable) ? pui32MapTable[ui32PageToFree] : ui32PageToFree;

		_FreeOSPage(ui32CPUCacheFlags,
					0,
					IMG_TRUE,
					IMG_TRUE,
					ppsPageArray[ui32Idx]);
	}
	eError = PVRSRV_ERROR_PMR_FAILED_TO_ALLOC_PAGES;
}

e_exit:
#if defined(CONFIG_X86)
	if (psPageArrayData->uiPagesToAlloc > PMR_UNSET_PAGES_STACK_ALLOC)
	{
		OSFreeMem(ppsUnsetPages);
	}
#endif

	return eError;
}

static PVRSRV_ERROR
_OSAllocRamBackedPages(struct _PMR_OSPAGEARRAY_DATA_ **ppsPageArrayDataPtr, IMG_UINT32 *pui32MapTable)
{
	PVRSRV_ERROR eError;
	IMG_UINT32 uiOrder;
	IMG_UINT32 uiPageIndex;
	IMG_UINT32 ui32CPUCacheFlags;
	struct _PMR_OSPAGEARRAY_DATA_ *psPageArrayData = *ppsPageArrayDataPtr;
	struct page **ppsPageArray = psPageArrayData->pagearray;
	unsigned int gfp_flags;

    PVR_ASSERT(NULL != psPageArrayData);
    PVR_ASSERT(0 <= psPageArrayData->iNumPagesAllocated);

    PVR_ASSERT(NULL != ppsPageArray);
    if(psPageArrayData->uiTotalNumPages < \
    			(psPageArrayData->iNumPagesAllocated + psPageArrayData->uiPagesToAlloc))
    {
    	return PVRSRV_ERROR_PMR_BAD_MAPPINGTABLE_SIZE;
    }

	uiOrder = psPageArrayData->uiLog2DevPageSize - PAGE_SHIFT;
	ui32CPUCacheFlags = psPageArrayData->ui32CPUCacheFlags;

	gfp_flags = GFP_USER | __GFP_NOWARN | __GFP_NOMEMALLOC;

#if defined(CONFIG_X86_64)
	gfp_flags |= __GFP_DMA32;
#else
#if !defined(CONFIG_X86_PAE)
	gfp_flags |= __GFP_HIGHMEM;
#endif
#endif

	if (psPageArrayData->bZero)
	{
		gfp_flags |= __GFP_ZERO;
	}

	/*
		Unset memory type is set to true as although in the "normal" case
		(where we free the page back to the pool) we don't want to unset
		it, we _must_ unset it in the case where the page pool was full
		and thus we have to give the page back to the OS.
	*/
	if (ui32CPUCacheFlags == PVRSRV_MEMALLOCFLAG_CPU_UNCACHED
	    ||ui32CPUCacheFlags == PVRSRV_MEMALLOCFLAG_CPU_WRITE_COMBINE)
	{
		psPageArrayData->bUnsetMemoryType = IMG_TRUE;
	}
	else
	{
		psPageArrayData->bUnsetMemoryType = IMG_FALSE;
	}

	eError = _AllocOSPages(psPageArrayData, ui32CPUCacheFlags, uiOrder, gfp_flags, pui32MapTable);

	if (eError == PVRSRV_OK)
	{
		for (uiPageIndex = 0; uiPageIndex < psPageArrayData->uiPagesToAlloc; uiPageIndex++)
		{
			/* Can't ask us to zero it and poison it */
			PVR_ASSERT(!psPageArrayData->bZero || !psPageArrayData->bPoisonOnAlloc);

			if (psPageArrayData->bPoisonOnAlloc)
			{
				_PoisonPages(ppsPageArray[uiPageIndex],
							 0,
							 _AllocPoison,
							 _AllocPoisonSize);
			}
			
#if defined(PVRSRV_ENABLE_PROCESS_STATS)
#if !defined(PVRSRV_ENABLE_MEMORY_STATS)
			/* Allocation is done a page at a time */
			PVRSRVStatsIncrMemAllocStat(PVRSRV_MEM_ALLOC_TYPE_ALLOC_UMA_PAGES, PAGE_SIZE);
#else
			{
				IMG_CPU_PHYADDR sCPUPhysAddr;
	
				sCPUPhysAddr.uiAddr = page_to_phys(ppsPageArray[uiPageIndex]);
				PVRSRVStatsAddMemAllocRecord(PVRSRV_MEM_ALLOC_TYPE_ALLOC_UMA_PAGES,
											 NULL,
											 sCPUPhysAddr,
											 PAGE_SIZE,
											 NULL);
			}
#endif
#endif
		}

		psPageArrayData->iNumPagesAllocated += psPageArrayData->uiPagesToAlloc;
		psPageArrayData->uiPagesToAlloc = 0;

		if(psPageArrayData->iNumPagesAllocated)
		{
			/* OS Pages have been allocated */
			psPageArrayData->bHasOSPages = IMG_TRUE;
		}

		PVR_DPF((PVR_DBG_MESSAGE, "physmem_osmem_linux.c: allocated OS memory for PMR @0x%p", psPageArrayData));

		return PVRSRV_OK;
	}
	return eError;
}


/*
	Note:
	We must _only_ check bUnsetMemoryType in the case where we need to free
	the page back to the OS since we may have to revert the cache properties
	of the page to the default as given by the OS when it was allocated.
*/
static void
_FreeOSPage(IMG_UINT32 ui32CPUCacheFlags,
            IMG_UINT32 uiOrder,
            IMG_BOOL bUnsetMemoryType,
            IMG_BOOL bFreeToOS,
            struct page *psPage)
{
	IMG_BOOL bAddedToPool = IMG_FALSE;
#if defined (CONFIG_X86)
	void *pvPageVAddr;
#else
	PVR_UNREFERENCED_PARAMETER(bUnsetMemoryType);
#endif

	/* Only zero order pages can be managed in the pool */
	if ((uiOrder == 0) && (!bFreeToOS))
	{
		_PagePoolLock();
		bAddedToPool = g_ui32PagePoolEntryCount < g_ui32PagePoolMaxEntries;
		_PagePoolUnlock();

		if (bAddedToPool)
		{
			if (!_AddEntryToPool(psPage, ui32CPUCacheFlags))
			{
				bAddedToPool = IMG_FALSE;
			}
		}
	}

	if (!bAddedToPool)
	{
#if defined(CONFIG_X86)
		pvPageVAddr = page_address(psPage);
		if (pvPageVAddr && bUnsetMemoryType == IMG_TRUE)
		{
			int ret;

			ret = set_memory_wb((unsigned long)pvPageVAddr, 1);
			if (ret)
			{
				PVR_DPF((PVR_DBG_ERROR, "%s: Failed to reset page attribute", __FUNCTION__));
			}
		}
#endif
		__free_pages(psPage, 0);
	}
}

static PVRSRV_ERROR
_FreeOSPagesArray(struct _PMR_OSPAGEARRAY_DATA_ *psPageArrayData)
{
	PVR_DPF((PVR_DBG_MESSAGE, "physmem_osmem_linux.c: freed OS memory for PMR @0x%p", psPageArrayData));

	OSFreeMem(psPageArrayData);

	return PVRSRV_OK;
}

static PVRSRV_ERROR
_FreeOSPages(struct _PMR_OSPAGEARRAY_DATA_ *psPageArrayData,
					IMG_UINT32 *pai32FreeIndices,
					IMG_UINT32 ui32FreePageCount)
{
	PVRSRV_ERROR eError;
	IMG_UINT32 uiNumPages;
	IMG_UINT32 uiOrder;
	IMG_UINT32 uiPageIndex,ui32Loop = 0;
	struct page **ppsPageArray;
	IMG_BOOL bAddedToPool = IMG_FALSE;

#if defined (CONFIG_X86)
	struct page *apsUnsetPages[PMR_UNSET_PAGES_STACK_ALLOC];
	struct page **ppsUnsetPages = apsUnsetPages;
	IMG_UINT32 uiUnsetPagesIndex = 0;

	if (psPageArrayData->uiTotalNumPages > PMR_UNSET_PAGES_STACK_ALLOC)
	{
		ppsUnsetPages = OSAllocMem(sizeof(struct page*) * psPageArrayData->uiTotalNumPages);
		if (ppsUnsetPages == NULL)
		{
			PVR_DPF((PVR_DBG_ERROR, "%s: Failed free_pages metadata allocation", __FUNCTION__));
			return PVRSRV_ERROR_OUT_OF_MEMORY;
			goto e_exit;
		}
	}
#endif

	PVR_ASSERT(psPageArrayData->bHasOSPages);

	ppsPageArray = psPageArrayData->pagearray;
	if(NULL == pai32FreeIndices)
	{
		uiNumPages = psPageArrayData->uiTotalNumPages;
	}else{
		uiNumPages = ui32FreePageCount;
	}
	uiOrder = psPageArrayData->uiLog2DevPageSize - PAGE_SHIFT;

	for (ui32Loop = 0;
			ui32Loop < uiNumPages;
			ui32Loop++)
	{
		if(NULL == pai32FreeIndices)
		{
			uiPageIndex = ui32Loop;
		}else{
			uiPageIndex = pai32FreeIndices[ui32Loop];
		}

		if(INVALID_PAGE != ppsPageArray[uiPageIndex])
		{

			if (psPageArrayData->bPoisonOnFree)
			{
				_PoisonPages(ppsPageArray[uiPageIndex],
							 uiOrder,
							 _FreePoison,
							 _FreePoisonSize);
			}

	#if defined(PVRSRV_ENABLE_PROCESS_STATS)
	#if !defined(PVRSRV_ENABLE_MEMORY_STATS)
			/* Allocation is done a page at a time */
			PVRSRVStatsDecrMemAllocStat(PVRSRV_MEM_ALLOC_TYPE_ALLOC_UMA_PAGES, PAGE_SIZE);
	#else
			{
				IMG_CPU_PHYADDR sCPUPhysAddr;

				sCPUPhysAddr.uiAddr = page_to_phys(ppsPageArray[uiPageIndex]);
				PVRSRVStatsRemoveMemAllocRecord(PVRSRV_MEM_ALLOC_TYPE_ALLOC_UMA_PAGES, sCPUPhysAddr.uiAddr);
			}
	#endif
	#endif
			/* Only zero order pages can be managed in the pool 
			 * Also unpinned pages should go to OS directly */
					if (uiOrder == 0  && !psPageArrayData->bUnpinned)
					{
						_PagePoolLock();
						bAddedToPool = g_ui32PagePoolEntryCount < g_ui32PagePoolMaxEntries;
						_PagePoolUnlock();

						if (bAddedToPool)
						{
							if (!_AddEntryToPool(ppsPageArray[uiPageIndex], psPageArrayData->ui32CPUCacheFlags))
							{
								bAddedToPool = IMG_FALSE;
							}
						}
					}

					if (!bAddedToPool)
					{
			#if defined(CONFIG_X86)
						if (psPageArrayData->bUnsetMemoryType == IMG_TRUE)
						{
							/* Keeping track of the pages for which the caching needs to change */
							ppsUnsetPages[uiUnsetPagesIndex] = ppsPageArray[uiPageIndex];
							uiUnsetPagesIndex++;
						}
						else
			#endif
						{
							__free_pages(ppsPageArray[uiPageIndex], uiOrder);
						}
					}
			ppsPageArray[uiPageIndex] = (struct page *)INVALID_PAGE;

			psPageArrayData->iNumPagesAllocated--;
			PVR_ASSERT(0 <= psPageArrayData->iNumPagesAllocated);
		}else{
			PVR_ASSERT(0 <= psPageArrayData->iNumPagesAllocated);
		}
	}

#if defined(CONFIG_X86)
	if (uiUnsetPagesIndex != 0)
	{
		int ret;
		ret = set_pages_array_wb(ppsUnsetPages, uiUnsetPagesIndex);

		if (ret)
		{
			PVR_DPF((PVR_DBG_ERROR, "%s: Failed to reset page attributes", __FUNCTION__));
		}

		for (uiPageIndex = 0;
		     uiPageIndex < uiUnsetPagesIndex;
		     uiPageIndex++)
		{
			_FreeOSPage(0,
			            uiOrder,
			            IMG_FALSE,
			            IMG_TRUE,
			            ppsUnsetPages[uiPageIndex]);
		}
	}

	if (ppsUnsetPages != apsUnsetPages)
	{
		OSFreeMem(ppsUnsetPages);
	}
#endif

	eError = PVRSRV_OK;

    PVR_ASSERT(psPageArrayData->iNumPagesAllocated <= psPageArrayData->uiTotalNumPages);

    if(0 == psPageArrayData->iNumPagesAllocated)
    {
    	psPageArrayData->bHasOSPages = IMG_FALSE;
    }

#if defined(CONFIG_X86)
e_exit:
#endif
	return eError;
}

/*
 *
 * Implementation of callback functions
 *
 */

/* destructor func is called after last reference disappears, but
   before PMR itself is freed. */
static PVRSRV_ERROR
PMRFinalizeOSMem(PMR_IMPL_PRIVDATA pvPriv
                 //struct _PMR_OSPAGEARRAY_DATA_ *psOSPageArrayData
                 )
{
    PVRSRV_ERROR eError;
    struct _PMR_OSPAGEARRAY_DATA_ *psOSPageArrayData;

    psOSPageArrayData = pvPriv;

    /* Conditionally do the PDump free, because if CreatePMR failed we
       won't have done the PDump MALLOC.  */
    if (psOSPageArrayData->bPDumpMalloced)
    {
        PDumpPMRFree(psOSPageArrayData->hPDumpAllocInfo);
    }

	if (psOSPageArrayData->bUnpinned == IMG_TRUE)
	{
		_PagePoolLock();
		if (psOSPageArrayData->bHasOSPages)
		{
			_RemoveUnpinListEntryUnlocked(psOSPageArrayData);
		}
		_PagePoolUnlock();
	}


	/*  We can't free pages until now. */
	if (psOSPageArrayData->bHasOSPages)
	{
		eError = _FreeOSPages(psOSPageArrayData,NULL, 0);
		PVR_ASSERT (eError == PVRSRV_OK); /* can we do better? */
    }

    eError = _FreeOSPagesArray(psOSPageArrayData);
    PVR_ASSERT (eError == PVRSRV_OK); /* can we do better? */

    return PVRSRV_OK;
}

/* callback function for locking the system physical page addresses.
   This function must be called before the lookup address func. */
static PVRSRV_ERROR
PMRLockSysPhysAddressesOSMem(PMR_IMPL_PRIVDATA pvPriv,
                             // struct _PMR_OSPAGEARRAY_DATA_ *psOSPageArrayData,
                             IMG_UINT32 uiLog2DevPageSize)
{
    PVRSRV_ERROR eError;
    struct _PMR_OSPAGEARRAY_DATA_ *psOSPageArrayData;

    psOSPageArrayData = pvPriv;

	/* Try and create the page pool if required */
	if ((g_ui32PagePoolMaxEntries > 0) && (g_psLinuxPagePoolCache == NULL))
	{
		_InitPagePool();
	}

	g_ui32LiveAllocs++;

    if (psOSPageArrayData->bOnDemand)
    {
		/* Allocate Memory for deferred allocation */
    	eError = _OSAllocRamBackedPages(&psOSPageArrayData,NULL);
    	if (eError != PVRSRV_OK)
    	{
    		return eError;
    	}
    }

    /* Physical page addresses are already locked down in this
       implementation, so there is no need to acquire physical
       addresses.  We do need to verify that the physical contiguity
       requested by the caller (i.e. page size of the device they
       intend to map this memory into) is compatible with (i.e. not of
       coarser granularity than) our already known physicial
       contiguity of the pages */
    if (uiLog2DevPageSize > psOSPageArrayData->uiLog2DevPageSize)
    {
        /* or NOT_MAPPABLE_TO_THIS_PAGE_SIZE ? */
        eError = PVRSRV_ERROR_PMR_INCOMPATIBLE_CONTIGUITY;
        return eError;
    }

    eError = PVRSRV_OK;
    return eError;

}

static PVRSRV_ERROR
PMRUnlockSysPhysAddressesOSMem(PMR_IMPL_PRIVDATA pvPriv
                               //struct _PMR_OSPAGEARRAY_DATA_ *psOSPageArrayData
                               )
{
    /* Just drops the refcount. */

    PVRSRV_ERROR eError = PVRSRV_OK;
    struct _PMR_OSPAGEARRAY_DATA_ *psOSPageArrayData;

    psOSPageArrayData = pvPriv;


    if (psOSPageArrayData->bOnDemand)
    {
		/* Free Memory for deferred allocation */
    	eError = _FreeOSPages(psOSPageArrayData,NULL,0);
    	if (eError != PVRSRV_OK)
    	{
    		return eError;
    	}
    }

    PVR_ASSERT (eError == PVRSRV_OK);
    g_ui32LiveAllocs--;
    /* Destroy the page pool if required */
    if ((g_ui32PagePoolMaxEntries > 0) && (g_psLinuxPagePoolCache != NULL) && (g_ui32LiveAllocs == 0))
    {
        _DeinitPagePool();
    }
    return eError;
}

/* N.B.  It is assumed that PMRLockSysPhysAddressesOSMem() is called _before_ this function! */
static PVRSRV_ERROR
PMRSysPhysAddrOSMem(PMR_IMPL_PRIVDATA pvPriv,
                    IMG_UINT32 ui32NumOfPages,
                    IMG_DEVMEM_OFFSET_T *puiOffset,
					IMG_BOOL *pbValid,
                    IMG_DEV_PHYADDR *psDevPAddr)
{
    const struct _PMR_OSPAGEARRAY_DATA_ *psOSPageArrayData = pvPriv;
    struct page **ppsPageArray = psOSPageArrayData->pagearray;
	IMG_UINT32 uiPageSize = 1U << psOSPageArrayData->uiLog2DevPageSize;
	IMG_UINT32 ui32Order = psOSPageArrayData->uiLog2DevPageSize - PAGE_SHIFT;
	IMG_UINT32 uiInPageOffset;
    IMG_UINT32 uiPageIndex;
    IMG_UINT32 idx;

    for (idx=0; idx < ui32NumOfPages; idx++)
    {
		if (pbValid[idx])
		{
			uiPageIndex = puiOffset[idx] >> psOSPageArrayData->uiLog2DevPageSize;
			uiInPageOffset = puiOffset[idx] - ((IMG_DEVMEM_OFFSET_T)uiPageIndex << psOSPageArrayData->uiLog2DevPageSize);

			PVR_ASSERT(uiPageIndex < psOSPageArrayData->uiTotalNumPages);
			PVR_ASSERT(uiInPageOffset < uiPageSize);

			psDevPAddr[idx].uiAddr = page_to_phys(ppsPageArray[uiPageIndex * (1 << ui32Order)]) + uiInPageOffset;
		}
    }

    return PVRSRV_OK;
}

typedef struct _PMR_OSPAGEARRAY_KERNMAP_DATA_ {
	void *pvBase;
	IMG_UINT32 ui32PageCount;
} PMR_OSPAGEARRAY_KERNMAP_DATA;

static PVRSRV_ERROR
PMRAcquireKernelMappingDataOSMem(PMR_IMPL_PRIVDATA pvPriv,
                                 size_t uiOffset,
                                 size_t uiSize,
                                 void **ppvKernelAddressOut,
                                 IMG_HANDLE *phHandleOut,
                                 PMR_FLAGS_T ulFlags)
{
    PVRSRV_ERROR eError;
    struct _PMR_OSPAGEARRAY_DATA_ *psOSPageArrayData;
    void *pvAddress;
    pgprot_t prot = PAGE_KERNEL;
    IMG_UINT32 ui32CPUCacheFlags;
    IMG_UINT32 ui32PageOffset;
    size_t uiMapOffset;
    IMG_UINT32 ui32PageCount;
    PMR_OSPAGEARRAY_KERNMAP_DATA *psData;

    psOSPageArrayData = pvPriv;
	ui32CPUCacheFlags = DevmemCPUCacheMode(ulFlags);

	/*
		Zero offset and size as a special meaning which means map in the
		whole of the PMR, this is due to fact that the places that call
		this callback might not have access to be able to determine the
		physical size
	*/
	if ((uiOffset == 0) && (uiSize == 0))
	{
		ui32PageOffset = 0;
		uiMapOffset = 0;
		ui32PageCount = psOSPageArrayData->iNumPagesAllocated;
	}
	else
	{
		size_t uiEndoffset;

		ui32PageOffset = uiOffset >> PAGE_SHIFT;
		uiMapOffset = uiOffset - (ui32PageOffset << PAGE_SHIFT);
		uiEndoffset = uiOffset + uiSize - 1;
		// Add one as we want the count, not the offset
		ui32PageCount = (uiEndoffset >> PAGE_SHIFT) + 1;
		ui32PageCount -= ui32PageOffset;
	}

	switch (ui32CPUCacheFlags)
	{
		case PVRSRV_MEMALLOCFLAG_CPU_UNCACHED:
				prot = pgprot_noncached(prot);
				break;

		case PVRSRV_MEMALLOCFLAG_CPU_WRITE_COMBINE:
				prot = pgprot_writecombine(prot);
				break;

		case PVRSRV_MEMALLOCFLAG_CPU_CACHED:
				break;

		default:
				eError = PVRSRV_ERROR_INVALID_PARAMS;
				goto e0;
	}
	
	psData = OSAllocMem(sizeof(PMR_OSPAGEARRAY_KERNMAP_DATA));
	if (psData == NULL)
	{
		eError = PVRSRV_ERROR_OUT_OF_MEMORY;
		goto e0;
	}
	
#if defined(PVRSRV_USE_VMAP_FOR_KERNEL_MAPPINGS)
	pvAddress = vmap(&psOSPageArrayData->pagearray[ui32PageOffset],
	                 ui32PageCount,
	                 VM_READ | VM_WRITE,
	                 prot);
#else
	pvAddress = vm_map_ram(&psOSPageArrayData->pagearray[ui32PageOffset],
						   ui32PageCount,
						   -1,
						   prot);
#endif
	if (pvAddress == NULL)
	{
		eError = PVRSRV_ERROR_OUT_OF_MEMORY;
		goto e1;
	}

    *ppvKernelAddressOut = pvAddress + uiMapOffset;
    psData->pvBase = pvAddress;
    psData->ui32PageCount = ui32PageCount;
    *phHandleOut = psData;

    return PVRSRV_OK;

    /*
      error exit paths follow
    */
 e1:
    OSFreeMem(psData);
 e0:
    PVR_ASSERT(eError != PVRSRV_OK);
    return eError;
}
static void PMRReleaseKernelMappingDataOSMem(PMR_IMPL_PRIVDATA pvPriv,
                                                 IMG_HANDLE hHandle)
{
    struct _PMR_OSPAGEARRAY_DATA_ *psOSPageArrayData;
    PMR_OSPAGEARRAY_KERNMAP_DATA *psData;

    psOSPageArrayData = pvPriv;
    psData = hHandle;
#if defined(PVRSRV_USE_VMAP_FOR_KERNEL_MAPPINGS)
    vunmap(psData->pvBase);
#else
    vm_unmap_ram(psData->pvBase, psData->ui32PageCount);
#endif
    OSFreeMem(psData);
}

static
PVRSRV_ERROR PMRUnpinOSMem(PMR_IMPL_PRIVDATA pPriv)
{

#if defined(PHYSMEM_SUPPORTS_SHRINKER)

	struct _PMR_OSPAGEARRAY_DATA_ *psOSPageArrayData = pPriv;
	PVRSRV_ERROR eError = PVRSRV_OK;

	/* Sanity check */
	PVR_ASSERT(psOSPageArrayData->bUnpinned == IMG_FALSE);

	/* Lock down the pool and add the array to the unpin list */
	_PagePoolLock();

	eError = _AddUnpinListEntryUnlocked(psOSPageArrayData);

	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR,
		         "%s: Not able to add allocation to unpinned list (%d).",
		         __FUNCTION__,
		         eError));

		goto e_exit;
	}

	psOSPageArrayData->bUnpinned = IMG_TRUE;

e_exit:
	_PagePoolUnlock();
	return eError;

#else
	PVR_UNREFERENCED_PARAMETER(pPriv);
	return PVRSRV_OK;
#endif
}

static
PVRSRV_ERROR PMRPinOSMem(PMR_IMPL_PRIVDATA pPriv,
							PMR_MAPPING_TABLE *psMappingTable)
{
#if defined(PHYSMEM_SUPPORTS_SHRINKER)

	PVRSRV_ERROR eError;
	struct _PMR_OSPAGEARRAY_DATA_ *psOSPageArrayData = pPriv;
	IMG_UINT32  *pui32MapTable = NULL;
	IMG_UINT32 i,j=0, ui32Temp=0;

	/* Sanity check */
	PVR_ASSERT(psOSPageArrayData->bUnpinned == IMG_TRUE);

	_PagePoolLock();

	/* If there are still pages in the array remove entries from the pool */
	if (psOSPageArrayData->bHasOSPages)
	{
		_RemoveUnpinListEntryUnlocked(psOSPageArrayData);
		psOSPageArrayData->bUnpinned = IMG_FALSE;

		_PagePoolUnlock();

		eError = PVRSRV_OK;
		goto e_exit_mapalloc_failure;
	}

	psOSPageArrayData->bUnpinned = IMG_FALSE;
	_PagePoolUnlock();

	/* If pages were reclaimed we allocate new ones and
	 * return PVRSRV_ERROR_PMR_NEW_MEMORY  */
	if(1 == psMappingTable->ui32NumVirtChunks){
		psOSPageArrayData->uiPagesToAlloc = psOSPageArrayData->uiTotalNumPages;
		eError = _OSAllocRamBackedPages(&psOSPageArrayData, NULL);
	}else
	{
		pui32MapTable = (IMG_UINT32 *)OSAllocMem(sizeof(IMG_UINT32) * psMappingTable->ui32NumPhysChunks);
		if(NULL == pui32MapTable)
		{
			eError = PVRSRV_ERROR_PMR_FAILED_TO_ALLOC_PAGES;
			PVR_DPF((PVR_DBG_ERROR,
					 "%s: Not able to Alloc Map Table.",
					 __FUNCTION__));
			goto e_exit_mapalloc_failure;
		}

		for (i = 0,j=0; i < psMappingTable->ui32NumVirtChunks; i++)
		{
			ui32Temp = psMappingTable->aui32Translation[i];
			if (TRANSLATION_INVALID != ui32Temp)
			{
				pui32MapTable[j++] = ui32Temp;
			}
		}

		psOSPageArrayData->uiPagesToAlloc = psMappingTable->ui32NumPhysChunks;
		eError = _OSAllocRamBackedPages(&psOSPageArrayData, pui32MapTable);
	}

	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR,
				 "%s: Not able to get new pages for unpinned allocation.",
				 __FUNCTION__));

		eError = PVRSRV_ERROR_PMR_FAILED_TO_ALLOC_PAGES;
		goto e_exit;
	}

	PVR_DPF((PVR_DBG_MESSAGE,
			 "%s: Allocating new pages for unpinned allocation. "
			 "Old content is lost!",
			 __FUNCTION__));

	eError = PVRSRV_ERROR_PMR_NEW_MEMORY;

e_exit:
	OSFreeMem(pui32MapTable);
e_exit_mapalloc_failure:
	return eError;

#else
	PVR_UNREFERENCED_PARAMETER(pPriv);
	return PVRSRV_OK;
#endif
}

/*************************************************************************/ /*!
@Function       PMRChangeSparseMemOSMem
@Description    This function Changes the sparse mapping by allocating & freeing
				of pages. It does also change the GPU and CPU maps accordingly
@Return         PVRSRV_ERROR failure code
*/ /**************************************************************************/
static PVRSRV_ERROR
PMRChangeSparseMemOSMem(PMR_IMPL_PRIVDATA pPriv,
		const PMR *psPMR,
		IMG_UINT32 ui32AllocPageCount,
		IMG_UINT32 *pai32AllocIndices,
		IMG_UINT32 ui32FreePageCount,
		IMG_UINT32 *pai32FreeIndices,
		IMG_UINT32	uiFlags,
		IMG_UINT32	*pui32Status)
{
	IMG_UINT32 ui32AdtnlAllocPages=0, ui32AdtnlFreePages=0,ui32CommonRequstCount=0,ui32Loop=0;
	struct _PMR_OSPAGEARRAY_DATA_ *psPMRPageArrayData = (struct _PMR_OSPAGEARRAY_DATA_ *)pPriv;
	struct page *page;
	PVRSRV_ERROR eError = ~PVRSRV_OK;
	IMG_UINT32	ui32Index = 0,uiAllocpgidx,uiFreepgidx;
	IMG_UINT32 ui32Order =  psPMRPageArrayData->uiLog2DevPageSize - PAGE_SHIFT;

	/*Fetch the Page table array represented by the PMR */
	struct page **psPageArray = psPMRPageArrayData->pagearray;
	PMR_MAPPING_TABLE *psPMRMapTable = PMR_GetMappigTable(psPMR);

	if(SPARSE_RESIZE_BOTH == (uiFlags & SPARSE_RESIZE_BOTH))
	{
		ui32CommonRequstCount = (ui32AllocPageCount > ui32FreePageCount)?ui32FreePageCount:ui32AllocPageCount;
	}
	if(SPARSE_RESIZE_ALLOC == (uiFlags & SPARSE_RESIZE_ALLOC))
	{
		ui32AdtnlAllocPages = ui32AllocPageCount - ui32CommonRequstCount;
	}
	if(SPARSE_RESIZE_FREE == (uiFlags & SPARSE_RESIZE_FREE))
	{
		ui32AdtnlFreePages = ui32FreePageCount - ui32CommonRequstCount;
	}
	if(0 == (ui32CommonRequstCount || ui32AdtnlAllocPages || ui32AdtnlFreePages))
	{
		eError = PVRSRV_ERROR_INVALID_PARAMS;
		return eError;
	}

	/*The incoming request is classified in to two operations alloc & free pages, independent of each other
	 * These operations can be combined with two mapping operations as well which are GPU & CPU space mapping
	 * From the alloc and free page requests, net pages to be allocated or freed is computed.
	 * And hence the order of operations are done in the following steps.
	 * 1. Allocate net pages
	 * 2. Move the free pages from free request to common alloc requests.
	 * 3. Free net pages
	 * */

	{
		/*Alloc parameters are validated at the time of allocation
		 * and any error will be handled then
		 * */
		/*Validate the free parameters*/
		if(ui32FreePageCount && (NULL != pai32FreeIndices))
		{
			for(ui32Loop=0; ui32Loop<ui32FreePageCount; ui32Loop++)
			{
				uiFreepgidx = pai32FreeIndices[ui32Loop];
				if((uiFreepgidx > psPMRPageArrayData->uiTotalNumPages))
				{
					eError = PVRSRV_ERROR_DEVICEMEM_OUT_OF_RANGE;
					goto SparseMemChangeFailed;
				}
				if(INVALID_PAGE == psPageArray[uiFreepgidx])
				{
					eError = PVRSRV_ERROR_INVALID_PARAMS;
					goto SparseMemChangeFailed;
				}
			}
		}

		/*The following block of code verifies any issues with common alloc page indices */
		for(ui32Loop=ui32AdtnlAllocPages; ui32Loop<ui32AllocPageCount; ui32Loop++)
		{
			uiAllocpgidx = pai32AllocIndices[ui32Loop];
			if((uiAllocpgidx > psPMRPageArrayData->uiTotalNumPages))
			{
				eError = PVRSRV_ERROR_DEVICEMEM_OUT_OF_RANGE;
				goto SparseMemChangeFailed;
			}
			if(SPARSE_REMAP_MEM != (uiFlags & SPARSE_REMAP_MEM))
			{
				if((INVALID_PAGE !=  psPageArray[uiAllocpgidx]) || \
								(TRANSLATION_INVALID != psPMRMapTable->aui32Translation[uiAllocpgidx]))
				{
					eError = PVRSRV_ERROR_INVALID_PARAMS;
					goto SparseMemChangeFailed;
				}
			}else{
				if(((INVALID_PAGE ==  psPageArray[uiAllocpgidx]) || \
						(TRANSLATION_INVALID == psPMRMapTable->aui32Translation[uiAllocpgidx])))
				{
					eError = PVRSRV_ERROR_INVALID_PARAMS;
					goto SparseMemChangeFailed;
				}
			}
		}

		ui32Loop = 0;
		if(0 != ui32AdtnlAllocPages)
		{

				/*Alloc pages*/

				/*Tell how many pages to allocate */
				psPMRPageArrayData->uiPagesToAlloc = ui32AdtnlAllocPages;

				eError = _OSAllocRamBackedPages(&psPMRPageArrayData,pai32AllocIndices);
				if(PVRSRV_OK != eError)
				{
					PVR_DPF((PVR_DBG_MESSAGE, "%s: New Addtl Allocation of pages failed", __FUNCTION__));
					goto SparseMemChangeFailed;

				}
				/*Mark the corresponding pages of translation table as valid */
				for(ui32Loop=0;ui32Loop<ui32AdtnlAllocPages;ui32Loop++)
				{
					psPMRMapTable->aui32Translation[pai32AllocIndices[ui32Loop]] = pai32AllocIndices[ui32Loop];
				}
		}

		/*Move the corresponding free pages to alloc request */
		ui32Index = ui32Loop;
		for(ui32Loop=0; ui32Loop<ui32CommonRequstCount; ui32Loop++,ui32Index++)
		{
			uiAllocpgidx = pai32AllocIndices[ui32Index];
			uiFreepgidx =  pai32FreeIndices[ui32Loop];
			page = psPageArray[uiAllocpgidx];
			psPageArray[uiAllocpgidx] = psPageArray[uiFreepgidx];
			/*is remap mem used in real world scenario? should it be turned to a debug feature ?
			 * The condition check need to be out of loop, will be done at later point though after some analysis */
			if(SPARSE_REMAP_MEM != (uiFlags & SPARSE_REMAP_MEM))
			{
				psPMRMapTable->aui32Translation[uiFreepgidx] = TRANSLATION_INVALID;
				psPMRMapTable->aui32Translation[uiAllocpgidx] = uiAllocpgidx;
				psPageArray[uiFreepgidx] = (struct page *)INVALID_PAGE;
			}else{
				psPageArray[uiFreepgidx] = page;
				psPMRMapTable->aui32Translation[uiFreepgidx] = uiFreepgidx;
				psPMRMapTable->aui32Translation[uiAllocpgidx] = uiAllocpgidx;
#if 0//def PDUMP
				PDUMP_PANIC(RGX, SPARSEMEM_SWAP, "Request to swap alloc & free pages not supported ");
#endif
			}

			/*Be sure to honour the attributes associated with the allocation
			 * such as zeroing, poisoning etc
			 * */
			if (psPMRPageArrayData->bPoisonOnAlloc)
	        {
	            _PoisonPages(psPageArray[uiAllocpgidx],
	            				ui32Order,
	            				_AllocPoison,
	            				_AllocPoisonSize);
	        }else{
	        	if (psPMRPageArrayData->bZero)
	        	{
	        		char a = 0;
	        		_PoisonPages(psPageArray[uiAllocpgidx],
	        						ui32Order,
	        		                &a,
	        		                1);
	        	}
	        }
		}

		/*Free the additional free pages */
		if(0 != ui32AdtnlFreePages)
		{
			eError = _FreeOSPages(psPMRPageArrayData, &pai32FreeIndices[ui32Loop], ui32AdtnlFreePages);
			while(ui32Loop < ui32FreePageCount)
			{
				psPMRMapTable->aui32Translation[pai32FreeIndices[ui32Loop]] = TRANSLATION_INVALID;
				ui32Loop++;
			}
		}

	}

	eError = PVRSRV_OK;
SparseMemChangeFailed:
		return eError;
}

/*************************************************************************/ /*!
@Function       PMRChangeSparseMemCPUMapOSMem
@Description    This function Changes CPU maps accordingly
@Return         PVRSRV_ERROR failure code
*/ /**************************************************************************/
static
PVRSRV_ERROR PMRChangeSparseMemCPUMapOSMem(PMR_IMPL_PRIVDATA pPriv,
								const PMR *psPMR,
								IMG_UINT64 sCpuVAddrBase,
								IMG_UINT32	ui32AllocPageCount,
								IMG_UINT32	*pai32AllocIndices,
								IMG_UINT32	ui32FreePageCount,
								IMG_UINT32	*pai32FreeIndices,
								IMG_UINT32	*pui32Status)
{
	struct page **psPageArray;
	struct _PMR_OSPAGEARRAY_DATA_ *psPMRPageArrayData = (struct _PMR_OSPAGEARRAY_DATA_ *)pPriv;
	PVRSRV_ERROR eError = ~PVRSRV_OK;

	psPageArray = psPMRPageArrayData->pagearray;
	return OSChangeSparseMemCPUAddrMap((void **)psPageArray,
											sCpuVAddrBase,
											0,
											ui32AllocPageCount,
											pai32AllocIndices,
											ui32FreePageCount,
											pai32FreeIndices,
											pui32Status,
											IMG_FALSE);
	return eError;
}

static PMR_IMPL_FUNCTAB _sPMROSPFuncTab = {
    .pfnLockPhysAddresses = &PMRLockSysPhysAddressesOSMem,
    .pfnUnlockPhysAddresses = &PMRUnlockSysPhysAddressesOSMem,
    .pfnDevPhysAddr = &PMRSysPhysAddrOSMem,
    .pfnAcquireKernelMappingData = &PMRAcquireKernelMappingDataOSMem,
    .pfnReleaseKernelMappingData = &PMRReleaseKernelMappingDataOSMem,
    .pfnReadBytes = NULL,
    .pfnWriteBytes = NULL,
    .pfnUnpinMem = &PMRUnpinOSMem,
    .pfnPinMem = &PMRPinOSMem,
    .pfnChangeSparseMem = &PMRChangeSparseMemOSMem,
    .pfnChangeSparseMemCPUMap = &PMRChangeSparseMemCPUMapOSMem,
    .pfnFinalize = &PMRFinalizeOSMem,
};

static PVRSRV_ERROR
_NewOSAllocPagesPMR(PVRSRV_DEVICE_NODE *psDevNode,
                    IMG_DEVMEM_SIZE_T uiSize,
					IMG_DEVMEM_SIZE_T uiChunkSize,
					IMG_UINT32 ui32NumPhysChunks,
					IMG_UINT32 ui32NumVirtChunks,
					IMG_UINT32 *pui32MappingTable,
                    IMG_UINT32 uiLog2DevPageSize,
                    PVRSRV_MEMALLOCFLAGS_T uiFlags,
                    PMR **ppsPMRPtr)
{
    PVRSRV_ERROR eError;
    PVRSRV_ERROR eError2;
    PMR *psPMR;
    struct _PMR_OSPAGEARRAY_DATA_ *psPrivData;
    IMG_HANDLE hPDumpAllocInfo = NULL;
    PMR_FLAGS_T uiPMRFlags;
	PHYS_HEAP *psPhysHeap;
    IMG_BOOL bZero;
    IMG_BOOL bPoisonOnAlloc;
    IMG_BOOL bPoisonOnFree;
    IMG_BOOL bOnDemand = ((uiFlags & PVRSRV_MEMALLOCFLAG_NO_OSPAGES_ON_ALLOC) > 0);
	IMG_BOOL bCpuLocal = ((uiFlags & PVRSRV_MEMALLOCFLAG_CPU_LOCAL) > 0);
	IMG_UINT32 ui32CPUCacheFlags = (IMG_UINT32) DevmemCPUCacheMode(uiFlags);

    if (uiFlags & PVRSRV_MEMALLOCFLAG_ZERO_ON_ALLOC)
    {
        bZero = IMG_TRUE;
    }
    else
    {
        bZero = IMG_FALSE;
    }

    if (uiFlags & PVRSRV_MEMALLOCFLAG_POISON_ON_ALLOC)
    {
        bPoisonOnAlloc = IMG_TRUE;
    }
    else
    {
        bPoisonOnAlloc = IMG_FALSE;
    }

    if (uiFlags & PVRSRV_MEMALLOCFLAG_POISON_ON_FREE)
    {
        bPoisonOnFree = IMG_TRUE;
    }
    else
    {
        bPoisonOnFree = IMG_FALSE;
    }

    if ((uiFlags & PVRSRV_MEMALLOCFLAG_ZERO_ON_ALLOC) &&
        (uiFlags & PVRSRV_MEMALLOCFLAG_POISON_ON_ALLOC))
    {
        /* Zero on Alloc and Poison on Alloc are mutually exclusive */
        eError = PVRSRV_ERROR_INVALID_PARAMS;
        goto errorOnParam;
    }

	/* Silently round up alignment/pagesize if request was less that
	   PAGE_SHIFT, because it would never be harmful for memory to be
	   _more_ contiguous that was desired */
	uiLog2DevPageSize = PAGE_SHIFT > uiLog2DevPageSize
		? PAGE_SHIFT
		: uiLog2DevPageSize;

	/* Create Array structure that hold the physical pages */
	eError = _AllocOSPageArray(uiChunkSize,
						   ui32NumPhysChunks,
						   ui32NumVirtChunks,
						   uiLog2DevPageSize,
						   bZero,
						   bPoisonOnAlloc,
						   bPoisonOnFree,
						   bOnDemand,
						   ui32CPUCacheFlags,
						   &psPrivData);
	if (eError != PVRSRV_OK)
	{
		goto errorOnAllocPageArray;
	}

	if (!bOnDemand)
	{
		if(ui32NumPhysChunks == ui32NumVirtChunks)
		{
			/* Allocate the physical pages */
			eError = _OSAllocRamBackedPages(&psPrivData,NULL);

		}else{
			if(0 != ui32NumPhysChunks)
			{
				/* Allocate the physical pages */
				eError = _OSAllocRamBackedPages(&psPrivData,pui32MappingTable);
			}
		}
		if (eError != PVRSRV_OK)
		{
			goto errorOnAllocPages;
		}
	}

    /* In this instance, we simply pass flags straight through.

       Generically, uiFlags can include things that control the PMR
       factory, but we don't need any such thing (at the time of
       writing!), and our caller specifies all PMR flags so we don't
       need to meddle with what was given to us.
    */
    uiPMRFlags = (PMR_FLAGS_T)(uiFlags & PVRSRV_MEMALLOCFLAGS_PMRFLAGSMASK);
    /* check no significant bits were lost in cast due to different
       bit widths for flags */
    PVR_ASSERT(uiPMRFlags == (uiFlags & PVRSRV_MEMALLOCFLAGS_PMRFLAGSMASK));

    if (bOnDemand)
    {
    	PDUMPCOMMENT("Deferred Allocation PMR (UMA)");
    }
    if (bCpuLocal)
    {
    	PDUMPCOMMENT("CPU_LOCAL allocation requested");
		psPhysHeap = psDevNode->apsPhysHeap[PVRSRV_DEVICE_PHYS_HEAP_CPU_LOCAL];
    }
	else
	{
		psPhysHeap = psDevNode->apsPhysHeap[PVRSRV_DEVICE_PHYS_HEAP_GPU_LOCAL];
	}

    eError = PMRCreatePMR(psPhysHeap,
                          uiSize,
                          uiChunkSize,
                          ui32NumPhysChunks,
                          ui32NumVirtChunks,
                          pui32MappingTable,
                          uiLog2DevPageSize,
                          uiPMRFlags,
                          "PMROSAP",
                          &_sPMROSPFuncTab,
                          psPrivData,
                          &psPMR,
                          &hPDumpAllocInfo,
    					  IMG_FALSE);
    if (eError != PVRSRV_OK)
    {
        goto errorOnCreate;
    }


	psPrivData->hPDumpAllocInfo = hPDumpAllocInfo;
	psPrivData->bPDumpMalloced = IMG_TRUE;

    *ppsPMRPtr = psPMR;
    return PVRSRV_OK;

errorOnCreate:
	if (!bOnDemand)
	{
		eError2 = _FreeOSPages(psPrivData,NULL, 0);
		PVR_ASSERT(eError2 == PVRSRV_OK);
	}

errorOnAllocPages:
	eError2 = _FreeOSPagesArray(psPrivData);
	PVR_ASSERT(eError2 == PVRSRV_OK);

errorOnAllocPageArray:
errorOnParam:
    PVR_ASSERT(eError != PVRSRV_OK);
    return eError;
}

PVRSRV_ERROR
PhysmemNewOSRamBackedPMR(PVRSRV_DEVICE_NODE *psDevNode,
                         IMG_DEVMEM_SIZE_T uiSize,
						 IMG_DEVMEM_SIZE_T uiChunkSize,
						 IMG_UINT32 ui32NumPhysChunks,
						 IMG_UINT32 ui32NumVirtChunks,
						 IMG_UINT32 *pui32MappingTable,
                         IMG_UINT32 uiLog2PageSize,
                         PVRSRV_MEMALLOCFLAGS_T uiFlags,
                         PMR **ppsPMRPtr)
{
    return _NewOSAllocPagesPMR(psDevNode,
                               uiSize,
                               uiChunkSize,
                               ui32NumPhysChunks,
                               ui32NumVirtChunks,
                               pui32MappingTable,
                               uiLog2PageSize,
                               uiFlags,
                               ppsPMRPtr);
}
