/*************************************************************************/ /*!
@File
@Title          RGX debug header file
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@Description    Header for the RGX debugging functions
@License        Strictly Confidential.
*/ /**************************************************************************/

#if !defined(__RGXDEBUG_H__)
#define __RGXDEBUG_H__

#include "pvrsrv_error.h"
#include "img_types.h"
#include "device.h"
#include "pvrsrv.h"
#include "rgxdevice.h"


/*!
*******************************************************************************

 @Function	RGXPanic

 @Description

 Called when an unrecoverable situation is detected. Dumps RGX debug
 information and tells the OS to panic.

 @Input psDevInfo - RGX device info

 @Return IMG_VOID

******************************************************************************/
IMG_VOID RGXPanic(PVRSRV_RGXDEV_INFO	*psDevInfo);

/*!
*******************************************************************************

 @Function	RGXDumpDebugInfo

 @Description

 Dump useful debugging info. Dumps lesser information than PVRSRVDebugRequest.
 Does not dump debugging information for all requester types.(SysDebug, ServerSync info)

 @Input pfnDumpDebugPrintf  - Optional replacement print function
 @Input psDevInfo	        - RGX device info

 @Return   IMG_VOID

******************************************************************************/
IMG_VOID RGXDumpDebugInfo(DUMPDEBUG_PRINTF_FUNC *pfnDumpDebugPrintf,
                          PVRSRV_RGXDEV_INFO	*psDevInfo);

/*!
*******************************************************************************

 @Function	RGXDebugRequestProcess

 @Description

 This function will print out the debug for the specificed level of
 verbosity

 @Input pfnDumpDebugPrintf  - Optional replacement print function
 @Input psDevInfo	        - RGX device info
 @Input ui32VerbLevel       - Verbosity level

 @Return   IMG_VOID

******************************************************************************/
IMG_VOID RGXDebugRequestProcess(DUMPDEBUG_PRINTF_FUNC *pfnDumpDebugPrintf,
                                PVRSRV_RGXDEV_INFO	*psDevInfo,
                                IMG_UINT32			ui32VerbLevel);


#if defined(PVRSRV_ENABLE_FW_TRACE_DEBUGFS)
/*!
*******************************************************************************

 @Function	RGXDumpFirmwareTrace

 @Description Dumps the decoded version of the firmware trace buffer.

 Dump useful debugging info

 @Input pfnDumpDebugPrintf  - Optional replacement print function
 @Input psDevInfo	        - RGX device info

 @Return   IMG_VOID

******************************************************************************/
IMG_VOID RGXDumpFirmwareTrace(DUMPDEBUG_PRINTF_FUNC *pfnDumpDebugPrintf,
                              PVRSRV_RGXDEV_INFO	*psDevInfo);
#endif


/*!
*******************************************************************************

 @Function	RGXQueryDMState

 @Description

 Query DM state

 @Input  psDevInfo        - RGX device info
 @Input  eDM              - DM number for which to return status
 @Output peState          - RGXFWIF_DM_STATE
 @Output psComCtxDevVAddr - If DM is locked-up, Firmware address of Firmware Common Context, otherwise IMG_NULL

 @Return PVRSRV_ERROR
******************************************************************************/
PVRSRV_ERROR RGXQueryDMState(PVRSRV_RGXDEV_INFO *psDevInfo, RGXFWIF_DM eDM, RGXFWIF_DM_STATE *peState, RGXFWIF_DEV_VIRTADDR *psComCtxDevVAddr);

/*!
*******************************************************************************

 @Function	RGXReadWithSP

 @Description

 Reads data from a memory location (FW memory map) using the META Slave Port

 @Input  ui32FWAddr - 32 bit FW address

 @Return IMG_UINT32
******************************************************************************/
IMG_UINT32 RGXReadWithSP(IMG_UINT32 ui32FWAddr);

#endif /* __RGXDEBUG_H__ */
