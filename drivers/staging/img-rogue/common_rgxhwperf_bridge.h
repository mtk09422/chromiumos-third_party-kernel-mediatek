/*************************************************************************/ /*!
@File
@Title          Common bridge header for rgxhwperf
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@Description    Declares common defines and structures that are used by both
                the client and sever side of the bridge for rgxhwperf
@License        Strictly Confidential.
*/ /**************************************************************************/

#ifndef COMMON_RGXHWPERF_BRIDGE_H
#define COMMON_RGXHWPERF_BRIDGE_H

#include "rgx_bridge.h"
#include "rgx_hwperf_km.h"


#include "pvr_bridge_io.h"

#define PVRSRV_BRIDGE_RGXHWPERF_CMD_FIRST			(PVRSRV_BRIDGE_RGXHWPERF_START)
#define PVRSRV_BRIDGE_RGXHWPERF_RGXCTRLHWPERF			PVRSRV_IOWR(PVRSRV_BRIDGE_RGXHWPERF_CMD_FIRST+0)
#define PVRSRV_BRIDGE_RGXHWPERF_RGXCONFIGENABLEHWPERFCOUNTERS			PVRSRV_IOWR(PVRSRV_BRIDGE_RGXHWPERF_CMD_FIRST+1)
#define PVRSRV_BRIDGE_RGXHWPERF_RGXCTRLHWPERFCOUNTERS			PVRSRV_IOWR(PVRSRV_BRIDGE_RGXHWPERF_CMD_FIRST+2)
#define PVRSRV_BRIDGE_RGXHWPERF_RGXCONFIGCUSTOMCOUNTERS			PVRSRV_IOWR(PVRSRV_BRIDGE_RGXHWPERF_CMD_FIRST+3)
#define PVRSRV_BRIDGE_RGXHWPERF_CMD_LAST			(PVRSRV_BRIDGE_RGXHWPERF_CMD_FIRST+3)


/*******************************************
            RGXCtrlHWPerf          
 *******************************************/

/* Bridge in structure for RGXCtrlHWPerf */
typedef struct PVRSRV_BRIDGE_IN_RGXCTRLHWPERF_TAG
{
	IMG_HANDLE hDevNode;
	IMG_BOOL bToggle;
	IMG_UINT64 ui64Mask;
} __attribute__((packed)) PVRSRV_BRIDGE_IN_RGXCTRLHWPERF;


/* Bridge out structure for RGXCtrlHWPerf */
typedef struct PVRSRV_BRIDGE_OUT_RGXCTRLHWPERF_TAG
{
	PVRSRV_ERROR eError;
} __attribute__((packed)) PVRSRV_BRIDGE_OUT_RGXCTRLHWPERF;

/*******************************************
            RGXConfigEnableHWPerfCounters          
 *******************************************/

/* Bridge in structure for RGXConfigEnableHWPerfCounters */
typedef struct PVRSRV_BRIDGE_IN_RGXCONFIGENABLEHWPERFCOUNTERS_TAG
{
	IMG_HANDLE hDevNode;
	IMG_UINT32 ui32ArrayLen;
	RGX_HWPERF_CONFIG_CNTBLK * psBlockConfigs;
} __attribute__((packed)) PVRSRV_BRIDGE_IN_RGXCONFIGENABLEHWPERFCOUNTERS;


/* Bridge out structure for RGXConfigEnableHWPerfCounters */
typedef struct PVRSRV_BRIDGE_OUT_RGXCONFIGENABLEHWPERFCOUNTERS_TAG
{
	PVRSRV_ERROR eError;
} __attribute__((packed)) PVRSRV_BRIDGE_OUT_RGXCONFIGENABLEHWPERFCOUNTERS;

/*******************************************
            RGXCtrlHWPerfCounters          
 *******************************************/

/* Bridge in structure for RGXCtrlHWPerfCounters */
typedef struct PVRSRV_BRIDGE_IN_RGXCTRLHWPERFCOUNTERS_TAG
{
	IMG_HANDLE hDevNode;
	IMG_BOOL bEnable;
	IMG_UINT32 ui32ArrayLen;
	IMG_UINT16 * pui16BlockIDs;
} __attribute__((packed)) PVRSRV_BRIDGE_IN_RGXCTRLHWPERFCOUNTERS;


/* Bridge out structure for RGXCtrlHWPerfCounters */
typedef struct PVRSRV_BRIDGE_OUT_RGXCTRLHWPERFCOUNTERS_TAG
{
	PVRSRV_ERROR eError;
} __attribute__((packed)) PVRSRV_BRIDGE_OUT_RGXCTRLHWPERFCOUNTERS;

/*******************************************
            RGXConfigCustomCounters          
 *******************************************/

/* Bridge in structure for RGXConfigCustomCounters */
typedef struct PVRSRV_BRIDGE_IN_RGXCONFIGCUSTOMCOUNTERS_TAG
{
	IMG_HANDLE hDevNode;
	IMG_UINT16 ui16CustomBlockID;
	IMG_UINT16 ui16NumCustomCounters;
	IMG_UINT32 * pui32CustomCounterIDs;
} __attribute__((packed)) PVRSRV_BRIDGE_IN_RGXCONFIGCUSTOMCOUNTERS;


/* Bridge out structure for RGXConfigCustomCounters */
typedef struct PVRSRV_BRIDGE_OUT_RGXCONFIGCUSTOMCOUNTERS_TAG
{
	PVRSRV_ERROR eError;
} __attribute__((packed)) PVRSRV_BRIDGE_OUT_RGXCONFIGCUSTOMCOUNTERS;

#endif /* COMMON_RGXHWPERF_BRIDGE_H */
