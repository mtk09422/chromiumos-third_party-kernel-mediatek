/*************************************************************************/ /*!
@File
@Title          RGX time correlation and calibration header file
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@Description    Header for the RGX time correlation and calibration routines
@License        Strictly Confidential.
*/ /**************************************************************************/

#if !defined(__RGXTIMECORR_H__)
#define __RGXTIMECORR_H__

#include "img_types.h"

/*!
******************************************************************************

 @Function    RGXGPUFreqCalibratePrePowerState

 @Description Manage GPU frequency and timer correlation data
              before a power off.

 @Input       hDevHandle : RGX Device Node

 @Return      IMG_VOID

******************************************************************************/
IMG_VOID RGXGPUFreqCalibratePrePowerState(IMG_HANDLE hDevHandle);

/*!
******************************************************************************

 @Function    RGXGPUFreqCalibratePostPowerState

 @Description Manage GPU frequency and timer correlation data
              after a power on.

 @Input       hDevHandle : RGX Device Node

 @Return      IMG_VOID

******************************************************************************/
IMG_VOID RGXGPUFreqCalibratePostPowerState(IMG_HANDLE hDevHandle);

/*!
******************************************************************************

 @Function    RGXGPUFreqCalibratePreClockSpeedChange

 @Description Manage GPU frequency and timer correlation data
              before a DVFS transition.

 @Input       hDevHandle : RGX Device Node

 @Return      IMG_VOID

******************************************************************************/
IMG_VOID RGXGPUFreqCalibratePreClockSpeedChange(IMG_HANDLE hDevHandle);

/*!
******************************************************************************

 @Function    RGXGPUFreqCalibratePostClockSpeedChange

 @Description Manage GPU frequency and timer correlation data
              after a DVFS transition.

 @Input       hDevHandle        : RGX Device Node
 @Input       ui32NewClockSpeed : GPU clock speed after the DVFS transition

 @Return      IMG_UINT32 : Calibrated GPU clock speed after the DVFS transition

******************************************************************************/
IMG_UINT32 RGXGPUFreqCalibratePostClockSpeedChange(IMG_HANDLE hDevHandle, IMG_UINT32 ui32NewClockSpeed);

/*!
******************************************************************************

 @Function    RGXGPUFreqCalibratePeriodic

 @Description Calibrate the GPU clock speed and correlate the timers
              at regular intervals.

 @Input       hDevHandle : RGX Device Node

 @Return      IMG_VOID

******************************************************************************/
IMG_VOID RGXGPUFreqCalibrateCorrelatePeriodic(IMG_HANDLE hDevHandle);

#endif /* __RGXTIMECORR_H__ */
