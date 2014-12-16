/*************************************************************************/ /*!
@File
@Title          RGX HW Performance header file
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@Description    Header for the RGX HWPerf functions
@License        Strictly Confidential.
*/ /**************************************************************************/

#ifndef RGXHWPERF_H_
#define RGXHWPERF_H_
  
#include "img_types.h"
#include "img_defs.h"
#include "pvrsrv_error.h"

#include "device.h"
#include "rgxdevice.h"
#include "rgx_hwperf_km.h"


/******************************************************************************
 * RGX HW Performance Data Transport Routines
 *****************************************************************************/

PVRSRV_ERROR RGXHWPerfDataStoreCB(PVRSRV_DEVICE_NODE* psDevInfo);

PVRSRV_ERROR RGXHWPerfInit(PVRSRV_DEVICE_NODE *psRgxDevInfo, IMG_BOOL bEnable);
IMG_VOID RGXHWPerfDeinit(void);


/******************************************************************************
 * RGX HW Performance Profiling API(s)
 *****************************************************************************/

PVRSRV_ERROR PVRSRVRGXCtrlHWPerfKM(
		PVRSRV_DEVICE_NODE*	psDeviceNode,
		IMG_BOOL			bToggle,
		IMG_UINT64 			ui64Mask);


PVRSRV_ERROR PVRSRVRGXConfigEnableHWPerfCountersKM(
		PVRSRV_DEVICE_NODE* 		psDeviceNode,
		IMG_UINT32 					ui32ArrayLen,
		RGX_HWPERF_CONFIG_CNTBLK* 	psBlockConfigs);

PVRSRV_ERROR PVRSRVRGXCtrlHWPerfCountersKM(
		PVRSRV_DEVICE_NODE*		psDeviceNode,
		IMG_BOOL			bEnable,
	    IMG_UINT32 			ui32ArrayLen,
	    IMG_UINT16*			psBlockIDs);

PVRSRV_ERROR PVRSRVRGXConfigCustomCountersKM(
		PVRSRV_DEVICE_NODE*     psDeviceNode,
		IMG_UINT16              ui16CustomBlockID,
		IMG_UINT16              ui16NumCustomCounters,
		IMG_UINT32*             pui32CustomCounterIDs);

/******************************************************************************
 * RGX HW Performance To FTrace Profiling API(s)
 *****************************************************************************/

#if defined(SUPPORT_GPUTRACE_EVENTS)

PVRSRV_ERROR RGXHWPerfFTraceGPUInit(PVRSRV_RGXDEV_INFO *psDevInfo);
IMG_VOID RGXHWPerfFTraceGPUDeInit(PVRSRV_RGXDEV_INFO *psDevInfo);

IMG_VOID RGXHWPerfFTraceGPUEnqueueEvent(PVRSRV_RGXDEV_INFO *psDevInfo,
		IMG_UINT32 ui32ExternalJobRef, IMG_UINT32 ui32InternalJobRef,
		const IMG_CHAR* pszJobType);

IMG_VOID RGXHWPerfFTraceGPUEventsEnabledSet(IMG_BOOL bNewValue);
IMG_BOOL RGXHWPerfFTraceGPUEventsEnabled(IMG_VOID);

IMG_VOID RGXHWPerfFTraceGPUThread(IMG_PVOID pvData);

#endif


#endif /* RGXHWPERF_H_ */
