/**************************************************************************/ /*!
@File
@Title          Server side internal synchronisation interface
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@Description    Describes the server side internal synchronisation functions
@License        Strictly Confidential.
*/ /***************************************************************************/

#ifndef _SYNC_SERVER_INTERNAL_H_
#define _SYNC_SERVER_INTERNAL_H_

#include "img_types.h"

IMG_VOID
ServerSyncRef(SERVER_SYNC_PRIMITIVE *psSync);

IMG_VOID
ServerSyncUnref(SERVER_SYNC_PRIMITIVE *psSync);

#endif	/*_SYNC_SERVER_INTERNAL_H_ */
