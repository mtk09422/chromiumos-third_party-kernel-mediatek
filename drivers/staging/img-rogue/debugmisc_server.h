/*************************************************************************/ /*!
@File
@Title          Debugging and miscellaneous functions server interface
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@Description    Kernel services functions for debugging and other
                miscellaneous functionality.
@License        Strictly Confidential.
*/ /**************************************************************************/

#if ! defined(DEBUGMISC_SERVER_H)
#define DEBUGMISC_SERVER_H

#include <img_defs.h>
#include <pvrsrv_error.h>
#include <device.h>
#include <pmr.h>

IMG_EXPORT PVRSRV_ERROR
PVRSRVDebugMiscSLCSetBypassStateKM(
	PVRSRV_DEVICE_NODE *psDeviceNode,
	IMG_UINT32  uiFlags,
	IMG_BOOL  bSetBypassed);

IMG_EXPORT PVRSRV_ERROR
PVRSRVDebugMiscInitFWImageKM(
	PMR *psFWImgDestPMR,
	PMR *psFWImgSrcPMR,
	IMG_UINT64 ui64FWImgLen,
	PMR *psFWImgSigPMR,
	IMG_UINT64 ui64FWSigLen);

IMG_EXPORT PVRSRV_ERROR
PVRSRVRGXDebugMiscSetFWLogKM(
	PVRSRV_DEVICE_NODE *psDeviceNode,
	IMG_UINT32  ui32RGXFWLogType);

IMG_EXPORT PVRSRV_ERROR
PVRSRVRGXDebugMiscDumpFreelistPageListKM(PVRSRV_DEVICE_NODE *psDeviceNode);

IMG_EXPORT PVRSRV_ERROR
PowMonTestIoctlKM(IMG_UINT32  uiCmd,
				  IMG_UINT32  uiIn1,
				  IMG_UINT32  uiIn2,
				  IMG_UINT32  *puiOut1,
				  IMG_UINT32  *puiOut2);
#endif
