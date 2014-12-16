/*************************************************************************/ /*!
@File
@Title          RGX Register configuration
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@Description    RGX Regconfig routines
@License        Strictly Confidential.
*/ /**************************************************************************/

#include "rgxregconfig.h"
#include "pvr_debug.h"
#include "rgxutils.h"
#include "rgxfwutils.h"
#include "device.h"
#include "sync_internal.h"
#include "pdump_km.h"
#include "pvrsrv.h"
PVRSRV_ERROR PVRSRVRGXSetRegConfigPIKM(PVRSRV_DEVICE_NODE	*psDeviceNode,
					IMG_UINT8              ui8RegPowerIsland)
{
#if defined(SUPPORT_USER_REGISTER_CONFIGURATION)
	PVRSRV_ERROR 		eError = PVRSRV_OK;
	PVRSRV_RGXDEV_INFO 	*psDevInfo = psDeviceNode->pvDevice;
	RGX_REG_CONFIG          *psRegCfg = &psDevInfo->sRegCongfig;
	RGXFWIF_PWR_EVT		ePowerIsland = (RGXFWIF_PWR_EVT) ui8RegPowerIsland;


	if (ePowerIsland < psRegCfg->ePowerIslandToPush)
	{
		PVR_DPF((PVR_DBG_ERROR, "PVRSRVRGXSetRegConfigPIKM: Register configuration must be in power island order."));
		return PVRSRV_ERROR_REG_CONFIG_INVALID_PI;
	}

	psRegCfg->ePowerIslandToPush = ePowerIsland;

	return eError;
#else
	PVR_DPF((PVR_DBG_ERROR, "PVRSRVRGXSetRegConfigPIKM: Feature disabled. Compile with SUPPORT_USER_REGISTER_CONFIGURATION"));
	return PVRSRV_ERROR_FEATURE_DISABLED;
#endif
}

PVRSRV_ERROR PVRSRVRGXAddRegConfigKM(PVRSRV_DEVICE_NODE	*psDeviceNode,
					IMG_UINT32		ui32RegAddr,
					IMG_UINT64		ui64RegValue)
{
#if defined(SUPPORT_USER_REGISTER_CONFIGURATION)
	PVRSRV_ERROR 		eError = PVRSRV_OK;
	RGXFWIF_KCCB_CMD 	sRegCfgCmd;
	PVRSRV_RGXDEV_INFO 	*psDevInfo = psDeviceNode->pvDevice;
	RGX_REG_CONFIG          *psRegCfg = &psDevInfo->sRegCongfig;

	if (psRegCfg->bEnabled)
	{
		PVR_DPF((PVR_DBG_ERROR, "PVRSRVRGXSetRegConfigPIKM: Cannot add record whilst register configuration active."));
		return PVRSRV_ERROR_REG_CONFIG_ENABLED;
	}
	if (psRegCfg->ui32NumRegRecords == RGXFWIF_REG_CFG_MAX_SIZE)
	{
		PVR_DPF((PVR_DBG_ERROR, "PVRSRVRGXSetRegConfigPIKM: Register configuration full."));
		return PVRSRV_ERROR_REG_CONFIG_FULL;
	}

	sRegCfgCmd.eCmdType = RGXFWIF_KCCB_CMD_REGCONFIG;
	sRegCfgCmd.uCmdData.sRegConfigData.sRegConfig.ui64Addr = (IMG_UINT64) ui32RegAddr;
	sRegCfgCmd.uCmdData.sRegConfigData.sRegConfig.ui64Value = ui64RegValue;
	sRegCfgCmd.uCmdData.sRegConfigData.eRegConfigPI = psRegCfg->ePowerIslandToPush;
	sRegCfgCmd.uCmdData.sRegConfigData.eCmdType = RGXFWIF_REGCFG_CMD_ADD;

	eError = RGXScheduleCommand(psDeviceNode->pvDevice,
				RGXFWIF_DM_GP,
				&sRegCfgCmd,
				sizeof(sRegCfgCmd),
				IMG_TRUE);
	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR, "PVRSRVRGXAddRegConfigKM: RGXScheduleCommand failed. Error:%u", eError));
		return eError;
	}

	psRegCfg->ui32NumRegRecords++;

	return eError;
#else
	PVR_DPF((PVR_DBG_ERROR, "PVRSRVRGXSetRegConfigPIKM: Feature disabled. Compile with SUPPORT_USER_REGISTER_CONFIGURATION"));
	return PVRSRV_ERROR_FEATURE_DISABLED;
#endif
}

PVRSRV_ERROR PVRSRVRGXClearRegConfigKM(PVRSRV_DEVICE_NODE	*psDeviceNode)
{
#if defined(SUPPORT_USER_REGISTER_CONFIGURATION)
	PVRSRV_ERROR 		eError = PVRSRV_OK;
	RGXFWIF_KCCB_CMD 	sRegCfgCmd;
	PVRSRV_RGXDEV_INFO 	*psDevInfo = psDeviceNode->pvDevice;
	RGX_REG_CONFIG          *psRegCfg = &psDevInfo->sRegCongfig;

	if (psRegCfg->bEnabled)
	{
		PVR_DPF((PVR_DBG_ERROR, "PVRSRVRGXSetRegConfigPIKM: Attempt to clear register configuration whilst active."));
		return PVRSRV_ERROR_REG_CONFIG_ENABLED;
	}

	sRegCfgCmd.eCmdType = RGXFWIF_KCCB_CMD_REGCONFIG;
	sRegCfgCmd.uCmdData.sRegConfigData.eCmdType = RGXFWIF_REGCFG_CMD_CLEAR;

	eError = RGXScheduleCommand(psDeviceNode->pvDevice,
				RGXFWIF_DM_GP,
				&sRegCfgCmd,
				sizeof(sRegCfgCmd),
				IMG_TRUE);
	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR, "PVRSRVRGXClearRegConfigKM: RGXScheduleCommand failed. Error:%u", eError));
		return eError;
	}

	psRegCfg->ui32NumRegRecords = 0;
	psRegCfg->ePowerIslandToPush = RGXFWIF_PWR_EVT_PWR_ON; /* Default first PI */

	return eError;
#else
	PVR_DPF((PVR_DBG_ERROR, "PVRSRVRGXClearRegConfigKM: Feature disabled. Compile with SUPPORT_USER_REGISTER_CONFIGURATION"));
	return PVRSRV_ERROR_FEATURE_DISABLED;
#endif
}

PVRSRV_ERROR PVRSRVRGXEnableRegConfigKM(PVRSRV_DEVICE_NODE	*psDeviceNode)
{
#if defined(SUPPORT_USER_REGISTER_CONFIGURATION)
	PVRSRV_ERROR 		eError = PVRSRV_OK;
	RGXFWIF_KCCB_CMD 	sRegCfgCmd;
	PVRSRV_RGXDEV_INFO 	*psDevInfo = psDeviceNode->pvDevice;
	RGX_REG_CONFIG          *psRegCfg = &psDevInfo->sRegCongfig;

	sRegCfgCmd.eCmdType = RGXFWIF_KCCB_CMD_REGCONFIG;
	sRegCfgCmd.uCmdData.sRegConfigData.eCmdType = RGXFWIF_REGCFG_CMD_ENABLE;

	eError = RGXScheduleCommand(psDeviceNode->pvDevice,
				RGXFWIF_DM_GP,
				&sRegCfgCmd,
				sizeof(sRegCfgCmd),
				IMG_TRUE);
	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR, "PVRSRVRGXEnableRegConfigKM: RGXScheduleCommand failed. Error:%u", eError));
		return eError;
	}

	psRegCfg->bEnabled = IMG_TRUE;

	return eError;
#else
	PVR_DPF((PVR_DBG_ERROR, "PVRSRVRGXEnableRegConfigKM: Feature disabled. Compile with SUPPORT_USER_REGISTER_CONFIGURATION"));
	return PVRSRV_ERROR_FEATURE_DISABLED;
#endif
}

PVRSRV_ERROR PVRSRVRGXDisableRegConfigKM(PVRSRV_DEVICE_NODE	*psDeviceNode)
{
#if defined(SUPPORT_USER_REGISTER_CONFIGURATION)
	PVRSRV_ERROR 		eError = PVRSRV_OK;
	RGXFWIF_KCCB_CMD 	sRegCfgCmd;
	PVRSRV_RGXDEV_INFO 	*psDevInfo = psDeviceNode->pvDevice;
	RGX_REG_CONFIG          *psRegCfg = &psDevInfo->sRegCongfig;

	sRegCfgCmd.eCmdType = RGXFWIF_KCCB_CMD_REGCONFIG;
	sRegCfgCmd.uCmdData.sRegConfigData.eCmdType = RGXFWIF_REGCFG_CMD_DISABLE;

	eError = RGXScheduleCommand(psDeviceNode->pvDevice,
				RGXFWIF_DM_GP,
				&sRegCfgCmd,
				sizeof(sRegCfgCmd),
				IMG_TRUE);
	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR, "PVRSRVRGXDisableRegConfigKM: RGXScheduleCommand failed. Error:%u", eError));
		return eError;
	}

	psRegCfg->bEnabled = IMG_FALSE;

	return eError;
#else
	PVR_DPF((PVR_DBG_ERROR, "PVRSRVRGXDisableRegConfigKM: Feature disabled. Compile with SUPPORT_USER_REGISTER_CONFIGURATION"));
	return PVRSRV_ERROR_FEATURE_DISABLED;
#endif
}


/******************************************************************************
 End of file (rgxregconfig.c)
******************************************************************************/
