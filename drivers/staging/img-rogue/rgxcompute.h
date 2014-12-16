/*************************************************************************/ /*!
@File
@Title          RGX compute functionality
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@Description    Header for the RGX compute functionality
@License        Strictly Confidential.
*/ /**************************************************************************/

#if !defined(__RGXCOMPUTE_H__)
#define __RGXCOMPUTE_H__

#include "devicemem.h"
#include "device.h"
#include "rgxfwutils.h"
#include "rgx_fwif_resetframework.h"
#include "rgxdebug.h"

#include "sync_server.h"
#include "sync_internal.h"
#include "connection_server.h"


typedef struct _RGX_SERVER_COMPUTE_CONTEXT_ RGX_SERVER_COMPUTE_CONTEXT;

/*!
*******************************************************************************
 @Function	PVRSRVRGXCreateComputeContextKM

 @Description
	

 @Input pvDeviceNode 
 @Input psCmpCCBMemDesc - 
 @Input psCmpCCBCtlMemDesc - 
 @Output ppsFWComputeContextMemDesc - 

 @Return   PVRSRV_ERROR
******************************************************************************/
IMG_EXPORT
PVRSRV_ERROR PVRSRVRGXCreateComputeContextKM(CONNECTION_DATA			*psConnection,
											 PVRSRV_DEVICE_NODE			*psDeviceNode,
											 IMG_UINT32					ui32Priority,
											 IMG_DEV_VIRTADDR			sMCUFenceAddr,
											 IMG_UINT32					ui32FrameworkRegisterSize,
											 IMG_PBYTE					pbyFrameworkRegisters,
											 IMG_HANDLE					hMemCtxPrivData,
											 RGX_SERVER_COMPUTE_CONTEXT	**ppsComputeContext);

/*! 
*******************************************************************************
 @Function	PVRSRVRGXDestroyComputeContextKM

 @Description
	Server-side implementation of RGXDestroyComputeContext

 @Input psCleanupData - 

 @Return   PVRSRV_ERROR
******************************************************************************/
IMG_EXPORT
PVRSRV_ERROR PVRSRVRGXDestroyComputeContextKM(RGX_SERVER_COMPUTE_CONTEXT *psComputeContext);


/*!
*******************************************************************************
 @Function	PVRSRVRGXKickCDMKM

 @Description
	Server-side implementation of RGXKickCDM

 @Input psDeviceNode - RGX Device node
 @Input psFWComputeContextMemDesc - Mem desc for firmware compute context
 @Input ui32cCCBWoffUpdate - New fw Woff for the client CDM CCB

 @Return   PVRSRV_ERROR
******************************************************************************/
IMG_EXPORT
PVRSRV_ERROR PVRSRVRGXKickCDMKM(RGX_SERVER_COMPUTE_CONTEXT	*psComputeContext,
								IMG_UINT32					ui32ClientFenceCount,
								PRGXFWIF_UFO_ADDR			*pauiClientFenceUFOAddress,
								IMG_UINT32					*paui32ClientFenceValue,
								IMG_UINT32					ui32ClientUpdateCount,
								PRGXFWIF_UFO_ADDR			*pauiClientUpdateUFOAddress,
								IMG_UINT32					*paui32ClientUpdateValue,
								IMG_UINT32					ui32ServerSyncPrims,
								IMG_UINT32					*paui32ServerSyncFlags,
								SERVER_SYNC_PRIMITIVE 		**pasServerSyncs,
								IMG_UINT32					ui32CmdSize,
								IMG_PBYTE					pui8DMCmd,
								IMG_BOOL					bPDumpContinuous,
								IMG_UINT32					ui32ExtJobRef,
								IMG_UINT32					ui32IntJobRef);

/*!
*******************************************************************************
 @Function	PVRSRVRGXFlushComputeDataKM

 @Description
	Server-side implementation of RGXFlushComputeData

 @Input psComputeContext - Compute context to flush

 @Return   PVRSRV_ERROR
******************************************************************************/
IMG_EXPORT
PVRSRV_ERROR PVRSRVRGXFlushComputeDataKM(RGX_SERVER_COMPUTE_CONTEXT *psComputeContext);

PVRSRV_ERROR PVRSRVRGXSetComputeContextPriorityKM(CONNECTION_DATA *psConnection,
												  RGX_SERVER_COMPUTE_CONTEXT *psComputeContext,
												  IMG_UINT32 ui32Priority);

/* Debug - check if compute context is waiting on a fence */
IMG_VOID CheckForStalledComputeCtxt(PVRSRV_RGXDEV_INFO *psDevInfo,
									DUMPDEBUG_PRINTF_FUNC *pfnDumpDebugPrintf);

/* Debug/Watchdog - check if client compute contexts are stalled */
IMG_BOOL CheckForStalledClientComputeCtxt(PVRSRV_RGXDEV_INFO *psDevInfo);

/*!
*******************************************************************************
 @Function	PVRSRVRGXKickSyncCDMKM

 @Description
	Sending a sync kick command though this CDM context

 @Return   PVRSRV_ERROR
******************************************************************************/
IMG_EXPORT PVRSRV_ERROR 
PVRSRVRGXKickSyncCDMKM(RGX_SERVER_COMPUTE_CONTEXT  *psComputeContext,
                       IMG_UINT32                  ui32ClientFenceCount,
                       PRGXFWIF_UFO_ADDR           *pauiClientFenceUFOAddress,
                       IMG_UINT32                  *paui32ClientFenceValue,
                       IMG_UINT32                  ui32ClientUpdateCount,
                       PRGXFWIF_UFO_ADDR           *pauiClientUpdateUFOAddress,
                       IMG_UINT32                  *paui32ClientUpdateValue,
                       IMG_UINT32                  ui32ServerSyncPrims,
                       IMG_UINT32                  *paui32ServerSyncFlags,
                       SERVER_SYNC_PRIMITIVE       **pasServerSyncs,
					   IMG_UINT32				   ui32NumFenceFDs,
					   IMG_INT32				   *paui32FenceFDs,
                       IMG_BOOL                    bPDumpContinuous);

#endif /* __RGXCOMPUTE_H__ */
