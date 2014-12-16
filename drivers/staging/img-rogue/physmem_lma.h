/**************************************************************************/ /*!
@File
@Title          Header for local card memory allocator
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@Description    Part of the memory management. This module is responsible for
                implementing the function callbacks for local card memory.
@License        Strictly Confidential.
*/ /***************************************************************************/

#ifndef _SRVSRV_PHYSMEM_LMA_H_
#define _SRVSRV_PHYSMEM_LMA_H_

/* include/ */
#include "img_types.h"
#include "pvrsrv_error.h"
#include "pvrsrv_memallocflags.h"

/* services/server/include/ */
#include "pmr.h"
#include "pmr_impl.h"

/*
 * PhysmemNewLocalRamBackedPMR
 *
 * This function will create a PMR using the local card memory and is OS
 * agnostic.
 */
PVRSRV_ERROR
PhysmemNewLocalRamBackedPMR(PVRSRV_DEVICE_NODE *psDevNode,
							IMG_DEVMEM_SIZE_T uiSize,
							IMG_DEVMEM_SIZE_T uiChunkSize,
							IMG_UINT32 ui32NumPhysChunks,
							IMG_UINT32 ui32NumVirtChunks,
							IMG_BOOL *pabMappingTable,
							IMG_UINT32 uiLog2PageSize,
							PVRSRV_MEMALLOCFLAGS_T uiFlags,
							PMR **ppsPMRPtr);

#if defined(SUPPORT_GPUVIRT_VALIDATION)
/*
 * Define some helper list functions for the virtualization validation code
 */

IMG_VOID	InsertPidOSidsCoupling(IMG_PID pId, IMG_UINT32 ui32OSid, IMG_UINT32 ui32OSidReg);
IMG_VOID	RetrieveOSidsfromPidList(IMG_PID pId, IMG_UINT32 *pui32OSid, IMG_UINT32 *pui32OSidReg);
IMG_VOID	RemovePidOSidCoupling(IMG_PID pId);
#endif

#endif /* #ifndef _SRVSRV_PHYSMEM_LMA_H_ */
