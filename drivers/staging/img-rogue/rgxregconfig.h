/*************************************************************************/ /*!
@File
@Title          RGX register configuration functionality
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@Description    Header for the RGX register configuration functionality
@License        Strictly Confidential.
*/ /**************************************************************************/

#if !defined(__RGXREGCONFIG_H__)
#define __RGXREGCONFIG_H__

#include "pvr_debug.h"
#include "rgxutils.h"
#include "rgxfwutils.h"
#include "rgx_fwif_km.h"

/*!
*******************************************************************************
 @Function	PVRSRVRGXSetRegConfigPIKM

 @Description
	Server-side implementation of RGXSetRegConfig

 @Input psDeviceNode - RGX Device node
 @Input ui8RegPowerIsland - Reg configuration

 @Return   PVRSRV_ERROR
******************************************************************************/
PVRSRV_ERROR PVRSRVRGXSetRegConfigPIKM(PVRSRV_DEVICE_NODE	*psDeviceNode,
					IMG_UINT8       ui8RegPowerIsland);
/*!
*******************************************************************************
 @Function	PVRSRVRGXSetRegConfigKM

 @Description
	Server-side implementation of RGXSetRegConfig

 @Input psDeviceNode - RGX Device node
 @Input ui64RegAddr - Register address
 @Input ui64RegValue - Reg value

 @Return   PVRSRV_ERROR
******************************************************************************/

PVRSRV_ERROR PVRSRVRGXAddRegConfigKM(PVRSRV_DEVICE_NODE	*psDeviceNode,
					IMG_UINT32	ui64RegAddr,
					IMG_UINT64	ui64RegValue);

/*!
*******************************************************************************
 @Function	PVRSRVRGXClearRegConfigKM

 @Description
	Server-side implementation of RGXClearRegConfig

 @Input psDeviceNode - RGX Device node

 @Return   PVRSRV_ERROR
******************************************************************************/
PVRSRV_ERROR PVRSRVRGXClearRegConfigKM(PVRSRV_DEVICE_NODE	*psDeviceNode);

/*!
*******************************************************************************
 @Function	PVRSRVRGXEnableRegConfigKM

 @Description
	Server-side implementation of RGXEnableRegConfig

 @Input psDeviceNode - RGX Device node

 @Return   PVRSRV_ERROR
******************************************************************************/
PVRSRV_ERROR PVRSRVRGXEnableRegConfigKM(PVRSRV_DEVICE_NODE	*psDeviceNode);

/*!
*******************************************************************************
 @Function	PVRSRVRGXDisableRegConfigKM

 @Description
	Server-side implementation of RGXDisableRegConfig

 @Input psDeviceNode - RGX Device node

 @Return   PVRSRV_ERROR
******************************************************************************/
PVRSRV_ERROR PVRSRVRGXDisableRegConfigKM(PVRSRV_DEVICE_NODE	*psDeviceNode);

#endif /* __RGXREGCONFIG_H__ */
