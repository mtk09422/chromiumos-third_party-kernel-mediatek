/**************************************************************************/ /*!
@File
@Title		PMR implementation of OS derived physical memory
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@Description	Part of the memory management.  This module is
                responsible for the an implementation of the "PMR"
                abstraction.  This interface is for the
                PhysmemNewOSRamBackedPMR() "PMR Factory" which is
                responsible for claiming chunks of memory (in
                particular physically contiguous quanta) from the
                Operating System.

                As such, this interface will be implemented on a
                Per-OS basis, in the "env" directory for that system.
                A dummy implementation is available in
                physmem_osmem_dummy.c for operating systems that
                cannot, or do not wish to, offer this functionality.
@License        Strictly Confidential.
*/ /***************************************************************************/
#ifndef _SRVSRV_PHYSMEM_OSMEM_H_
#define _SRVSRV_PHYSMEM_OSMEM_H_

/* include/ */
#include "img_types.h"
#include "pvrsrv_error.h"
#include "pvrsrv_memallocflags.h"

/* services/server/include/ */
#include "pmr.h"
#include "pmr_impl.h"

/*
 * PhysmemNewOSRamBackedPMR
 *
 * To be overridden on a per-OS basis.
 *
 * This function will create a PMR using the default "OS supplied" physical pages
 * method, assuming such is available on a particular operating system.  (If not,
 * PVRSRV_ERROR_NOT_SUPPORTED should be returned)
 */
extern PVRSRV_ERROR
PhysmemNewOSRamBackedPMR(PVRSRV_DEVICE_NODE *psDevNode,
                         IMG_DEVMEM_SIZE_T uiSize,
						 IMG_DEVMEM_SIZE_T uiChunkSize,
						 IMG_UINT32 ui32NumPhysChunks,
						 IMG_UINT32 ui32NumVirtChunks,
						 IMG_BOOL *pabMappingTable,
                         IMG_UINT32 uiLog2PageSize,
                         PVRSRV_MEMALLOCFLAGS_T uiFlags,
                         PMR **ppsPMROut);

/*
 * PhysmemNewTDMetaCodePMR
 *
 * This function is used as part of the facility to provide secure META firmware
 * memory. A default implementation is provided which must be replaced by the SoC
 * implementor.
 *
 * Calling this function will return a PMR for a memory allocation made in "secure
 * META code memory". It will only be writable by a hypervisor, and when the feature
 * is enabled on the SoC, the META will only be able to perform instruction reads from
 * memory that is secured that way.
 */
PVRSRV_ERROR
PhysmemNewTDMetaCodePMR(PVRSRV_DEVICE_NODE *psDevNode,
                        IMG_DEVMEM_SIZE_T uiSize,
                        IMG_UINT32 uiLog2PageSize,
                        PVRSRV_MEMALLOCFLAGS_T uiFlags,
                        PMR **ppsPMRPtr);

PVRSRV_ERROR
PhysmemNewTDSecureBufPMR(PVRSRV_DEVICE_NODE *psDevNode,
                         IMG_DEVMEM_SIZE_T uiSize,
                         IMG_UINT32 uiLog2PageSize,
                         PVRSRV_MEMALLOCFLAGS_T uiFlags,
                         PMR **ppsPMRPtr);


#endif /* #ifndef _SRVSRV_PHYSMEM_OSMEM_H_ */
