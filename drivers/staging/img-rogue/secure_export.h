/**************************************************************************/ /*!
@File
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@License        Strictly Confidential.
*/ /***************************************************************************/

#include "img_types.h"
#include "pvrsrv_error.h"
#include "pmr.h"
#include "connection_server.h"

typedef struct _SECURE_CLEANUP_DATA_ {
	PMR *psPMR;
} SECURE_CLEANUP_DATA;

PVRSRV_ERROR PMRSecureExportPMR(CONNECTION_DATA *psConnection,
								PMR *psPMR,
								IMG_SECURE_TYPE *phSecure,
								PMR **ppsPMR,
								CONNECTION_DATA **ppsSecureConnection);

PVRSRV_ERROR PMRSecureUnexportPMR(PMR *psPMR);

PVRSRV_ERROR PMRSecureImportPMR(IMG_SECURE_TYPE hSecure,
								PMR **ppsPMR,
								IMG_DEVMEM_SIZE_T *puiSize,
								IMG_DEVMEM_ALIGN_T *puiAlign);

PVRSRV_ERROR PMRSecureUnimportPMR(PMR *psPMR);

