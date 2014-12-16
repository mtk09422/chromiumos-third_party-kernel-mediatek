/**************************************************************************/ /*!
@File           physmem_dmabuf.h
@Title          Header for dmabuf PMR factory
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@Description    Part of the memory management. This module is responsible for
                implementing the function callbacks importing Ion allocations
@License        Strictly Confidential.
*/ /***************************************************************************/

#if !defined(_PHYSMEM_DMABUF_H_)
#define _PHYSMEM_DMABUF_H_

#include <linux/dma-buf.h>

#include "img_types.h"
#include "pvrsrv_error.h"
#include "pvrsrv_memallocflags.h"
#include "connection_server.h"

#include "pmr.h"

typedef PVRSRV_ERROR (*PFN_DESTROY_DMABUF_PMR)(PHYS_HEAP *psHeap,
					       struct dma_buf_attachment *psAttachment);

PVRSRV_ERROR
PhysmemCreateNewDmaBufBackedPMR(PHYS_HEAP *psHeap,
				struct dma_buf_attachment *psAttachment,
				PFN_DESTROY_DMABUF_PMR pfnDestroy,
				PVRSRV_MEMALLOCFLAGS_T uiFlags,
				PMR **ppsPMRPtr);

#if defined(SUPPORT_ION)
PVRSRV_ERROR
PhysmemImportDmaBuf(CONNECTION_DATA *psConnection,
					IMG_INT fd,
					PVRSRV_MEMALLOCFLAGS_T uiFlags,
					PMR **ppsPMRPtr,
					IMG_DEVMEM_SIZE_T *puiSize,
					IMG_DEVMEM_ALIGN_T *puiAlign);
#endif

#endif /* !defined(_PHYSMEM_DMABUF_H_) */
