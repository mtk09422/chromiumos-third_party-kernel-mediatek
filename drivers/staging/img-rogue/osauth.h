/**************************************************************************/ /*!
@File
@Title          OS Authentication header
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@Description    Defines the interface between the OS and the bridge to
                authenticate a function called from the client
@License        Strictly Confidential.
*/ /***************************************************************************/

#ifndef __OSAUTH_H__
#define __OSAUTH_H__

#include "img_types.h"
#include "pvrsrv_error.h"
#include "connection_server.h"

PVRSRV_ERROR OSCheckAuthentication(CONNECTION_DATA *psConnectionData, IMG_UINT32 ui32Level);

#endif
