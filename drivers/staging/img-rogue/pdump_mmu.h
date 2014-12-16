/**************************************************************************/ /*!
@File
@Title          Common MMU Management
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@Description    Implements basic low level control of MMU.
@License        Strictly Confidential.
*/ /***************************************************************************/

#ifndef SRVKM_PDUMP_MMU_H
#define SRVKM_PDUMP_MMU_H

/* services/server/include/ */
#include "pdump_symbolicaddr.h"
/* include/ */
#include "img_types.h"
#include "pvrsrv_error.h"

#include "mmu_common.h"

/*
	PDUMP MMU attributes
*/
typedef struct _PDUMP_MMU_ATTRIB_DEVICE_
{
    /* Per-Device Pdump attribs */

	/*!< Pdump memory bank name */
	IMG_CHAR				*pszPDumpMemDevName;

	/*!< Pdump register bank name */
	IMG_CHAR				*pszPDumpRegDevName;

} PDUMP_MMU_ATTRIB_DEVICE;

typedef struct _PDUMP_MMU_ATTRIB_CONTEXT_
{
	IMG_UINT32 ui32Dummy;
} PDUMP_MMU_ATTRIB_CONTEXT;

typedef struct _PDUMP_MMU_ATTRIB_HEAP_
{
	/* data page info */
	IMG_UINT32 ui32DataPageMask;
} PDUMP_MMU_ATTRIB_HEAP;

typedef struct _PDUMP_MMU_ATTRIB_
{
    /* FIXME: would these be better as pointers rather than copies? */
    struct _PDUMP_MMU_ATTRIB_DEVICE_ sDevice;
    struct _PDUMP_MMU_ATTRIB_CONTEXT_ sContext;
    struct _PDUMP_MMU_ATTRIB_HEAP_ sHeap;
} PDUMP_MMU_ATTRIB;

#if defined(PDUMP)
    extern PVRSRV_ERROR PDumpMMUMalloc(const IMG_CHAR			*pszPDumpDevName,
                                       MMU_LEVEL				eMMULevel,
                                       IMG_DEV_PHYADDR			*psDevPAddr,
                                       IMG_UINT32				ui32Size,
                                       IMG_UINT32				ui32Align);

    extern PVRSRV_ERROR PDumpMMUFree(const IMG_CHAR				*pszPDumpDevName,
                                     MMU_LEVEL					eMMULevel,
                                     IMG_DEV_PHYADDR			*psDevPAddr);

    extern PVRSRV_ERROR PDumpMMUMalloc2(const IMG_CHAR			*pszPDumpDevName,
                                        const IMG_CHAR			*pszTableType,/* PAGE_CATALOGUE, PAGE_DIRECTORY, PAGE_TABLE */
                                        const IMG_CHAR 			*pszSymbolicAddr,
                                        IMG_UINT32				ui32Size,
                                        IMG_UINT32				ui32Align);

    extern PVRSRV_ERROR PDumpMMUFree2(const IMG_CHAR			*pszPDumpDevName,
                                      const IMG_CHAR			*pszTableType,/* PAGE_CATALOGUE, PAGE_DIRECTORY, PAGE_TABLE */
                                      const IMG_CHAR 			*pszSymbolicAddr);

    extern PVRSRV_ERROR PDumpMMUDumpPxEntries(MMU_LEVEL eMMULevel,
    								   const IMG_CHAR *pszPDumpDevName,
                                       IMG_VOID *pvPxMem,
                                       IMG_DEV_PHYADDR sPxDevPAddr,
                                       IMG_UINT32 uiFirstEntry,
                                       IMG_UINT32 uiNumEntries,
                                       const IMG_CHAR *pszMemspaceName,
                                       const IMG_CHAR *pszSymbolicAddr,
                                       IMG_UINT64 uiSymbolicAddrOffset,
                                       IMG_UINT32 uiBytesPerEntry,
                                       IMG_UINT32 uiLog2Align,
                                       IMG_UINT32 uiAddrShift,
                                       IMG_UINT64 uiAddrMask,
                                       IMG_UINT64 uiPxEProtMask,
                                       IMG_UINT32 ui32Flags);

    extern PVRSRV_ERROR PDumpMMUAllocMMUContext(const IMG_CHAR *pszPDumpMemSpaceName,
                                                IMG_DEV_PHYADDR sPCDevPAddr,
                                                PDUMP_MMU_TYPE eMMUType,
                                                IMG_UINT32 *pui32MMUContextID);

    extern PVRSRV_ERROR PDumpMMUFreeMMUContext(const IMG_CHAR *pszPDumpMemSpaceName,
                                               IMG_UINT32 ui32MMUContextID);

	extern PVRSRV_ERROR PDumpMMUActivateCatalog(const IMG_CHAR *pszPDumpRegSpaceName,
												const IMG_CHAR *pszPDumpRegName,
												IMG_UINT32 uiRegAddr,
												const IMG_CHAR *pszPDumpPCSymbolicName);

	/* FIXME: split to separate file... (debatable whether this is anything to do with MMU) */
extern PVRSRV_ERROR
PDumpMMUSAB(const IMG_CHAR *pszPDumpMemNamespace,
               IMG_UINT32 uiPDumpMMUCtx,
               IMG_DEV_VIRTADDR sDevAddrStart,
               IMG_DEVMEM_SIZE_T uiSize,
               const IMG_CHAR *pszFilename,
               IMG_UINT32 uiFileOffset,
			   IMG_UINT32 ui32PDumpFlags);

	#define PDUMP_MMU_MALLOC_DP(pszPDumpMemDevName, aszSymbolicAddr, ui32Size, ui32Align) \
        PDumpMMUMalloc2(pszPDumpMemDevName, "DATA_PAGE", aszSymbolicAddr, ui32Size, ui32Align)
    #define PDUMP_MMU_FREE_DP(pszPDumpMemDevName, aszSymbolicAddr) \
        PDumpMMUFree2(pszPDumpMemDevName, "DATA_PAGE", aszSymbolicAddr)

    #define PDUMP_MMU_ALLOC_MMUCONTEXT(pszPDumpMemDevName, sPCDevPAddr, eMMUType, puiPDumpCtxID) \
        PDumpMMUAllocMMUContext(pszPDumpMemDevName,                     \
                                sPCDevPAddr,                            \
                                eMMUType,								\
                                puiPDumpCtxID)

    #define PDUMP_MMU_FREE_MMUCONTEXT(pszPDumpMemDevName, uiPDumpCtxID) \
        PDumpMMUFreeMMUContext(pszPDumpMemDevName, uiPDumpCtxID)
#else

	#define PDUMP_MMU_MALLOC_DP(pszPDumpMemDevName, pszDevPAddr, ui32Size, ui32Align) \
        ((IMG_VOID)0)
    #define PDUMP_MMU_FREE_DP(pszPDumpMemDevName, psDevPAddr) \
        ((IMG_VOID)0)
    #define PDUMP_MMU_ALLOC_MMUCONTEXT(pszPDumpMemDevName, sPCDevPAddr, puiPDumpCtxID) \
        ((IMG_VOID)0)
    #define PDUMP_MMU_FREE_MMUCONTEXT(pszPDumpMemDevName, uiPDumpCtxID) \
        ((IMG_VOID)0)

#endif // defined(PDUMP)

#endif
