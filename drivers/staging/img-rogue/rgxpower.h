/*************************************************************************/ /*!
@File
@Title          RGX power header file
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@Description    Header for the RGX power
@License        Strictly Confidential.
*/ /**************************************************************************/

#if !defined(__RGXPOWER_H__)
#define __RGXPOWER_H__

#include "pvrsrv_error.h"
#include "img_types.h"
#include "servicesext.h"


/*!
******************************************************************************

 @Function	RGXPrePowerState

 @Description

 does necessary preparation before power state transition

 @Input	   hDevHandle : RGX Device Node
 @Input	   eNewPowerState : New power state
 @Input	   eCurrentPowerState : Current power state

 @Return   PVRSRV_ERROR :

******************************************************************************/
PVRSRV_ERROR RGXPrePowerState(IMG_HANDLE				hDevHandle, 
							  PVRSRV_DEV_POWER_STATE	eNewPowerState, 
							  PVRSRV_DEV_POWER_STATE	eCurrentPowerState,
							  IMG_BOOL					bForced);

/*!
******************************************************************************

 @Function	RGXPostPowerState

 @Description

 does necessary preparation after power state transition

 @Input	   hDevHandle : RGX Device Node
 @Input	   eNewPowerState : New power state
 @Input	   eCurrentPowerState : Current power state

 @Return   PVRSRV_ERROR :

******************************************************************************/
PVRSRV_ERROR RGXPostPowerState(IMG_HANDLE				hDevHandle, 
							   PVRSRV_DEV_POWER_STATE	eNewPowerState, 
							   PVRSRV_DEV_POWER_STATE	eCurrentPowerState,
							  IMG_BOOL					bForced);


/*!
******************************************************************************

 @Function	RGXPreClockSpeedChange

 @Description

	Does processing required before an RGX clock speed change.

 @Input	   hDevHandle : RGX Device Node
 @Input	   bIdleDevice : Whether the firmware needs to be idled
 @Input	   eCurrentPowerState : Power state of the device

 @Return   PVRSRV_ERROR :

******************************************************************************/
PVRSRV_ERROR RGXPreClockSpeedChange(IMG_HANDLE				hDevHandle,
									PVRSRV_DEV_POWER_STATE	eCurrentPowerState);

/*!
******************************************************************************

 @Function	RGXPostClockSpeedChange

 @Description

	Does processing required after an RGX clock speed change.

 @Input	   hDevHandle : RGX Device Node
 @Input	   bIdleDevice : Whether the firmware had been idled previously
 @Input	   eCurrentPowerState : Power state of the device

 @Return   PVRSRV_ERROR :

******************************************************************************/
PVRSRV_ERROR RGXPostClockSpeedChange(IMG_HANDLE				hDevHandle,
									 PVRSRV_DEV_POWER_STATE	eCurrentPowerState);


/*!
******************************************************************************

 @Function	RGXDustCountChange

 @Description Change of number of DUSTs

 @Input	   hDevHandle : RGX Device Node
 @Input	   ui32NumberOfDusts : Number of DUSTs to make transition to

 @Return   PVRSRV_ERROR :

******************************************************************************/
PVRSRV_ERROR RGXDustCountChange(IMG_HANDLE				hDevHandle,
								IMG_UINT32				ui32NumberOfDusts);

/*!
******************************************************************************

 @Function	RGXAPMLatencyChange

 @Description

	Changes the wait duration used before firmware indicates IDLE.
	Reducing this value will cause the firmware to shut off faster and
	more often but may increase bubbles in GPU scheduling due to the added
	power management activity. If bPersistent is NOT set, APM latency will
	return back to system default on power up.

 @Input	   hDevHandle : RGX Device Node
 @Input	   ui32ActivePMLatencyms : Number of milliseconds to wait
 @Input	   bPersistent : Set to ensure new value is not reset

 @Return   PVRSRV_ERROR :

******************************************************************************/
PVRSRV_ERROR RGXAPMLatencyChange(IMG_HANDLE				hDevHandle,
				IMG_UINT32				ui32ActivePMLatencyms,
				IMG_BOOL				bActivePMLatencyPersistant);

/*!
******************************************************************************

 @Function	RGXActivePowerRequest

 @Description Initiate a handshake with the FW to power off the GPU

 @Input	   hDevHandle : RGX Device Node

 @Return   PVRSRV_ERROR :

******************************************************************************/
PVRSRV_ERROR RGXActivePowerRequest(IMG_HANDLE hDevHandle);

/*!
******************************************************************************

 @Function	RGXForcedIdleRequest

 @Description Initiate a handshake with the FW to idle the GPU

 @Input	   hDevHandle : RGX Device Node

 @Return   PVRSRV_ERROR :

******************************************************************************/
PVRSRV_ERROR RGXForcedIdleRequest(IMG_HANDLE hDevHandle, IMG_BOOL bDeviceOffPermitted);

/*!
******************************************************************************

 @Function	RGXCancelForcedIdleRequest

 @Description Send a request to cancel idle to the firmware.

 @Input	   hDevHandle : RGX Device Node

 @Return   PVRSRV_ERROR :

******************************************************************************/
PVRSRV_ERROR RGXCancelForcedIdleRequest(IMG_HANDLE hDevHandle);

#endif /* __RGXPOWER_H__ */
