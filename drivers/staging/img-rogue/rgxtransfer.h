/*************************************************************************/ /*!
@File
@Title          RGX Transfer queue Functionality
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@Description    Header for the RGX Transfer queue Functionality
@License        Strictly Confidential.
*/ /**************************************************************************/

#if !defined(__RGXTRANSFER_H__)
#define __RGXTRANSFER_H__

#include "devicemem.h"
#include "device.h"
#include "rgxdevice.h"
#include "rgxfwutils.h"
#include "rgx_fwif_resetframework.h"
#include "rgxdebug.h"

#include "sync_server.h"
#include "connection_server.h"

typedef struct _RGX_SERVER_TQ_CONTEXT_ RGX_SERVER_TQ_CONTEXT;

/*!
*******************************************************************************

 @Function	PVRSRVRGXCreateTransferContextKM

 @Description
	Server-side implementation of RGXCreateTransferContext

 @Input pvDeviceNode - device node
 
FIXME fill this in

 @Return   PVRSRV_ERROR

******************************************************************************/
IMG_EXPORT
PVRSRV_ERROR PVRSRVRGXCreateTransferContextKM(CONNECTION_DATA			*psConnection,
										   PVRSRV_DEVICE_NODE		*psDeviceNode,
										   IMG_UINT32				ui32Priority,
										   IMG_DEV_VIRTADDR			sMCUFenceAddr,
										   IMG_UINT32				ui32FrameworkCommandSize,
										   IMG_PBYTE				pabyFrameworkCommand,
										   IMG_HANDLE				hMemCtxPrivData,
										   RGX_SERVER_TQ_CONTEXT	**ppsTransferContext);


/*!
*******************************************************************************

 @Function	PVRSRVRGXDestroyTransferContextKM

 @Description
	Server-side implementation of RGXDestroyTransferContext

 @Input psTransferContext - Transfer context

 @Return   PVRSRV_ERROR

******************************************************************************/
IMG_EXPORT
PVRSRV_ERROR PVRSRVRGXDestroyTransferContextKM(RGX_SERVER_TQ_CONTEXT *psTransferContext);

/*!
*******************************************************************************

 @Function	PVRSRVSubmitTransferKM

 @Description
	Schedules one or more 2D or 3D HW commands on the firmware


 @Return   PVRSRV_ERROR

******************************************************************************/
IMG_EXPORT
PVRSRV_ERROR PVRSRVRGXSubmitTransferKM(RGX_SERVER_TQ_CONTEXT	*psTransferContext,
									IMG_UINT32				ui32PrepareCount,
									IMG_UINT32				*paui32ClientFenceCount,
									PRGXFWIF_UFO_ADDR		**papauiClientFenceUFOAddress,
									IMG_UINT32				**papaui32ClientFenceValue,
									IMG_UINT32				*paui32ClientUpdateCount,
									PRGXFWIF_UFO_ADDR		**papauiClientUpdateUFOAddress,
									IMG_UINT32				**papaui32ClientUpdateValue,
									IMG_UINT32				*paui32ServerSyncCount,
									IMG_UINT32				**papaui32ServerSyncFlags,
									SERVER_SYNC_PRIMITIVE	***papapsServerSyncs,
									IMG_UINT32				ui32NumFenceFDs,
									IMG_INT32				*paui32FenceFDs,
									IMG_UINT32				*paui32FWCommandSize,
									IMG_UINT8				**papaui8FWCommand,
									IMG_UINT32				*pui32TQPrepareFlags,
									IMG_UINT32				ui32ExtJobRef,
									IMG_UINT32				ui32IntJobRef);

IMG_EXPORT
PVRSRV_ERROR PVRSRVRGXSetTransferContextPriorityKM(CONNECTION_DATA *psConnection,
												   RGX_SERVER_TQ_CONTEXT *psTransferContext,
												   IMG_UINT32 ui32Priority);

/* Debug - check if transfer context is waiting on a fence */
IMG_VOID CheckForStalledTransferCtxt(PVRSRV_RGXDEV_INFO *psDevInfo,
									 DUMPDEBUG_PRINTF_FUNC *pfnDumpDebugPrintf);

/* Debug/Watchdog - check if client transfer contexts are stalled */
IMG_BOOL CheckForStalledClientTransferCtxt(PVRSRV_RGXDEV_INFO *psDevInfo);

PVRSRV_ERROR PVRSRVRGXKickSyncTransferKM(RGX_SERVER_TQ_CONTEXT	*psTransferContext,
									   IMG_UINT32				ui32ClientFenceCount,
									   PRGXFWIF_UFO_ADDR		*pauiClientFenceUFOAddress,
									   IMG_UINT32				*paui32ClientFenceValue,
									   IMG_UINT32				ui32ClientUpdateCount,
									   PRGXFWIF_UFO_ADDR		*pauiClientUpdateUFOAddress,
									   IMG_UINT32				*paui32ClientUpdateValue,
									   IMG_UINT32				ui32ServerSyncCount,
									   IMG_UINT32				*pui32ServerSyncFlags,
									   SERVER_SYNC_PRIMITIVE	**pasServerSyncs,
									   IMG_UINT32				ui32NumFenceFDs,
									   IMG_INT32				*paui32FenceFDs,
									   IMG_UINT32				ui32TQPrepareFlags);
#endif /* __RGXTRANSFER_H__ */
