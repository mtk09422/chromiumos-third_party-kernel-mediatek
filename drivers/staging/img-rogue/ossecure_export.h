/**************************************************************************/ /*!
@File
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@License        Strictly Confidential.
*/ /***************************************************************************/

#include "img_types.h"
#include "pvrsrv_error.h"
#include "connection_server.h"

PVRSRV_ERROR OSSecureExport(CONNECTION_DATA *psConnection,
							IMG_PVOID pvData,
							IMG_SECURE_TYPE *phSecure,
							CONNECTION_DATA **ppsSecureConnection);
							
PVRSRV_ERROR OSSecureImport(IMG_SECURE_TYPE hSecure, IMG_PVOID *ppvData);

