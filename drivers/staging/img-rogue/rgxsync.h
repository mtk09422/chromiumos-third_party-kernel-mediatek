/*************************************************************************/ /*!
@File
@Title          RGX sync kick functionality
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@Description    Header for the RGX sync kick functionality
@License        Strictly Confidential.
*/ /**************************************************************************/

#if !defined(__RGXSYNC__)
#define __RGXSYNC_H__

#include "devicemem.h"
#include "device.h"
#include "rgxfwutils.h"
#include "rgx_fwif_resetframework.h"
#include "rgxdebug.h"

#include "sync_server.h"
#include "sync_internal.h"
#include "connection_server.h"

/*!
*******************************************************************************
 @Function	RGXKickSyncKM

 @Description Send a sync kick command to the FW
	
 @Return   PVRSRV_ERROR
******************************************************************************/
PVRSRV_ERROR RGXKickSyncKM(PVRSRV_DEVICE_NODE        *psDeviceNode,
						   RGX_SERVER_COMMON_CONTEXT *psServerCommonContext,
						   RGXFWIF_DM                eDM,
						   IMG_CHAR                  *pszCommandName,
						   IMG_UINT32                ui32ClientFenceCount,
						   PRGXFWIF_UFO_ADDR         *pauiClientFenceUFOAddress,
						   IMG_UINT32                *paui32ClientFenceValue,
						   IMG_UINT32                ui32ClientUpdateCount,
						   PRGXFWIF_UFO_ADDR         *pauiClientUpdateUFOAddress,
						   IMG_UINT32                *paui32ClientUpdateValue,
						   IMG_UINT32                ui32ServerSyncPrims,
						   IMG_UINT32                *paui32ServerSyncFlags,
						   SERVER_SYNC_PRIMITIVE     **pasServerSyncs,
						   IMG_BOOL                  bPDumpContinuous);

#endif /* __RGXSYNC_H__ */
