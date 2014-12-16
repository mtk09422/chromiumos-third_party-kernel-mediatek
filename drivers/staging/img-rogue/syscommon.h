/*************************************************************************/ /*!
@File
@Title          Common System APIs and structures
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@Description    This header provides common system-specific declarations and macros
                that are supported by all systems
@License        Strictly Confidential.
*/ /**************************************************************************/

#ifndef _SYSCOMMON_H
#define _SYSCOMMON_H

#include "osfunc.h"

#if defined(NO_HARDWARE) && defined(__linux__) && defined(__KERNEL__)
#include <asm/io.h>
#endif

#if defined (__cplusplus)
extern "C" {
#endif
#include "pvrsrv.h"

PVRSRV_ERROR SysCreateConfigData(PVRSRV_SYSTEM_CONFIG **ppsSysConfig, void *hDevice);
IMG_VOID SysDestroyConfigData(PVRSRV_SYSTEM_CONFIG *psSysConfig);
PVRSRV_ERROR SysAcquireSystemData(IMG_HANDLE hSysData);
PVRSRV_ERROR SysReleaseSystemData(IMG_HANDLE hSysData);
PVRSRV_ERROR SysDebugInfo(PVRSRV_SYSTEM_CONFIG *psSysConfig, DUMPDEBUG_PRINTF_FUNC *pfnDumpDebugPrintf);

#if defined(SUPPORT_GPUVIRT_VALIDATION)
#include "services.h"
IMG_VOID SysSetOSidRegisters(IMG_UINT32 aui32OSidMin[GPUVIRT_VALIDATION_NUM_OS][GPUVIRT_VALIDATION_NUM_REGIONS], IMG_UINT32 aui32OSidMax[GPUVIRT_VALIDATION_NUM_OS][GPUVIRT_VALIDATION_NUM_REGIONS]);
IMG_VOID SysPrintAndResetFaultStatusRegister(void);
#endif

#if defined(SUPPORT_SYSTEM_INTERRUPT_HANDLING)
PVRSRV_ERROR SysInstallDeviceLISR(IMG_UINT32 ui32IRQ,
				  IMG_CHAR *pszName,
				  PFN_LISR pfnLISR,
				  IMG_PVOID pvData,
				  IMG_HANDLE *phLISRData);

PVRSRV_ERROR SysUninstallDeviceLISR(IMG_HANDLE hLISRData);

#if defined(SUPPORT_DRM)
IMG_BOOL SystemISRHandler(IMG_VOID *pvData);
#endif
#endif /* defined(SUPPORT_SYSTEM_INTERRUPT_HANDLING) */


/*
 * SysReadHWReg and SysWriteHWReg differ from OSReadHWReg and OSWriteHWReg
 * in that they are always intended for use with real hardware, even on
 * NO_HARDWARE systems.
 */
#if !(defined(NO_HARDWARE) && defined(__linux__) && defined(__KERNEL__))
#define	SysReadHWReg(p, o) OSReadHWReg(p, o)
#define SysWriteHWReg(p, o, v) OSWriteHWReg(p, o, v)
#else	/* !(defined(NO_HARDWARE) && defined(__linux__)) */
/*!
******************************************************************************

 @Function	SysReadHWReg

 @Description

 register read function

 @input pvLinRegBaseAddr :	lin addr of register block base

 @input ui32Offset :

 @Return   register value

******************************************************************************/
static inline IMG_UINT32 SysReadHWReg(IMG_PVOID pvLinRegBaseAddr, IMG_UINT32 ui32Offset)
{
	return (IMG_UINT32) readl(pvLinRegBaseAddr + ui32Offset);
}

/*!
******************************************************************************

 @Function	SysWriteHWReg

 @Description

 register write function

 @input pvLinRegBaseAddr :	lin addr of register block base

 @input ui32Offset :

 @input ui32Value :

 @Return   none

******************************************************************************/
static inline IMG_VOID SysWriteHWReg(IMG_PVOID pvLinRegBaseAddr, IMG_UINT32 ui32Offset, IMG_UINT32 ui32Value)
{
	writel(ui32Value, pvLinRegBaseAddr + ui32Offset);
}
#endif	/* !(defined(NO_HARDWARE) && defined(__linux__)) */

/*!
******************************************************************************

 @Function		SysCheckMemAllocSize

 @Description	Function to apply memory budgeting policies

 @input			psDevNode

 @input			uiChunkSize

 @input			ui32NumPhysChunks

 @Return		PVRSRV_ERROR

******************************************************************************/
FORCE_INLINE PVRSRV_ERROR SysCheckMemAllocSize(struct _PVRSRV_DEVICE_NODE_ *psDevNode,
												IMG_UINT64 ui64MemSize)
{
	PVR_UNREFERENCED_PARAMETER(psDevNode);
	PVR_UNREFERENCED_PARAMETER(ui64MemSize);

	return PVRSRV_OK;
}

#if defined(__cplusplus)
}
#endif

#endif

/*****************************************************************************
 End of file (syscommon.h)
*****************************************************************************/
