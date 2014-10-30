/*
 ***************************************************************************
 * Ralink Tech Inc.
 * 4F, No. 2 Technology 5th Rd.
 * Science-based Industrial Park
 * Hsin-chu, Taiwan, R.O.C.
 *
 * (c) Copyright 2002-2004, Ralink Technology, Inc.
 *
 * All rights reserved. Ralink's source code is an unpublished work and the
 * use of a copyright notice does not imply otherwise. This source code
 * contains confidential trade secret material of Ralink Tech. Any attemp
 * or participation in deciphering, decoding, reverse engineering or in any
 * way altering the source code is stricitly prohibited, unless the prior
 * written consent of Ralink Technology, Inc. is obtained.
 ***************************************************************************

	Module Name:
	rtmp_init_inf.c

	Abstract:
	Miniport generic portion header file

	Revision History:
	Who         When          What
	--------    ----------    ----------------------------------------------
*/
#include	"rt_config.h"
#ifdef DOT11R_FT_SUPPORT
#include	"ft.h"
#endif /* DOT11R_FT_SUPPORT */


#ifdef HW_COEXISTENCE_SUPPORT
VOID InitHWCoexistence(RTMP_ADAPTER *pAd);
#endif /* HW_COEXISTENCE_SUPPORT */

#ifdef CONFIG_STA_SUPPORT
#ifdef PROFILE_STORE
NDIS_STATUS WriteDatThread(RTMP_ADAPTER *pAd);
#endif /* PROFILE_STORE */
#endif /* CONFIG_STA_SUPPORT */

#ifdef LINUX
#ifdef OS_ABL_FUNC_SUPPORT
/* Utilities provided from NET module */
RTMP_NET_ABL_OPS RtmpDrvNetOps, *pRtmpDrvNetOps = &RtmpDrvNetOps;
RTMP_PCI_CONFIG RtmpPciConfig, *pRtmpPciConfig = &RtmpPciConfig;
RTMP_USB_CONFIG RtmpUsbConfig, *pRtmpUsbConfig = &RtmpUsbConfig;

VOID RtmpDrvOpsInit(
	OUT VOID *pDrvOpsOrg,
	INOUT VOID *pDrvNetOpsOrg,
	IN RTMP_PCI_CONFIG *pPciConfig,
	IN RTMP_USB_CONFIG *pUsbConfig)
{
	RTMP_DRV_ABL_OPS *pDrvOps = (RTMP_DRV_ABL_OPS *)pDrvOpsOrg;
#ifdef RTMP_USB_SUPPORT
	RTMP_NET_ABL_OPS *pDrvNetOps = (RTMP_NET_ABL_OPS *)pDrvNetOpsOrg;
#endif /* RTMP_USB_SUPPORT */


	/* init PCI/USB configuration in different OS */
	if (pPciConfig != NULL)
		RtmpPciConfig = *pPciConfig;

	if (pUsbConfig != NULL)
		RtmpUsbConfig = *pUsbConfig;

	/* init operators provided from us (DRIVER module) */
	pDrvOps->RTMPAllocAdapterBlock = RTMPAllocAdapterBlock;
	pDrvOps->RTMPFreeAdapter = RTMPFreeAdapter;

	pDrvOps->RtmpRaDevCtrlExit = RtmpRaDevCtrlExit;
	pDrvOps->RtmpRaDevCtrlInit = RtmpRaDevCtrlInit;
#ifdef RTMP_MAC_PCI
	pDrvOps->RTMPHandleInterrupt = RTMPHandleInterrupt;
#endif /* RTMP_MAC_PCI */
	pDrvOps->RTMPSendPackets = RTMPSendPackets;
#ifdef P2P_SUPPORT
	pDrvOps->P2P_PacketSend = P2P_PacketSend;
#endif /* P2P_SUPPORT */

	pDrvOps->RTMP_COM_IoctlHandle = RTMP_COM_IoctlHandle;
#ifdef CONFIG_AP_SUPPORT
	pDrvOps->RTMP_AP_IoctlHandle = RTMP_AP_IoctlHandle;
#endif /* CONFIG_AP_SUPPORT */
#ifdef CONFIG_STA_SUPPORT
	pDrvOps->RTMP_STA_IoctlHandle = RTMP_STA_IoctlHandle;
#endif /* CONFIG_STA_SUPPORT */

	pDrvOps->RTMPDrvOpen = RTMPDrvOpen;
	pDrvOps->RTMPDrvClose = RTMPDrvClose;
	pDrvOps->RTMPInfClose = RTMPInfClose;
	pDrvOps->rt28xx_init = rt28xx_init;

	/* init operators provided from us and netif module */
#ifdef RTMP_USB_SUPPORT
	*pRtmpDrvNetOps = *pDrvNetOps;
	pRtmpDrvNetOps->RtmpDrvUsbBulkOutDataPacketComplete = RTUSBBulkOutDataPacketComplete;
	pRtmpDrvNetOps->RtmpDrvUsbBulkOutMLMEPacketComplete = RTUSBBulkOutMLMEPacketComplete;
	pRtmpDrvNetOps->RtmpDrvUsbBulkOutNullFrameComplete = RTUSBBulkOutNullFrameComplete;
/*	pRtmpDrvNetOps->RtmpDrvUsbBulkOutRTSFrameComplete = RTUSBBulkOutRTSFrameComplete;*/
	pRtmpDrvNetOps->RtmpDrvUsbBulkOutPsPollComplete = RTUSBBulkOutPsPollComplete;
	pRtmpDrvNetOps->RtmpDrvUsbBulkRxComplete = RTUSBBulkRxComplete;
	*pDrvNetOps = *pRtmpDrvNetOps;
#endif /* RTMP_USB_SUPPORT */
}

RTMP_BUILD_DRV_OPS_FUNCTION_BODY

#endif /* OS_ABL_FUNC_SUPPORT */
#endif /* LINUX */


int rt28xx_init(VOID *pAdSrc, PSTRING pDefaultMac, PSTRING pHostName)
{
	RTMP_ADAPTER *pAd = (RTMP_ADAPTER *)pAdSrc;
	NDIS_STATUS Status;
#ifdef RTMP_MAC_USB
	UINT index = 0;
#endif /* RTMP_MAC_USB */
	
	if (!pAd)
		return FALSE;

	if (rtmp_asic_top_init(pAd) != TRUE)
		goto err1;

	DBGPRINT(RT_DEBUG_TRACE, ("MAC[Ver:Rev=0x%08x : 0x%08x]\n",
				pAd->MACVersion, pAd->ChipID));

	RT28XXDMADisable(pAd);

	if (mcu_sys_init(pAd) != TRUE)
		goto err1;

#ifdef RTMP_MAC_PCI
	// TODO: shiang-usw, need to check this for RTMP_MAC
	/* Disable interrupts here which is as soon as possible*/
	/* This statement should never be true. We might consider to remove it later*/
	if (RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_INTERRUPT_ACTIVE))
	{
		RTMP_ASIC_INTERRUPT_DISABLE(pAd);
	}
#endif /* RTMP_MAC_PCI */

	/* reset Adapter flags*/
	RTMP_CLEAR_FLAGS(pAd);

#ifdef RELEASE_EXCLUDE
	/* defined(RT5370) || defined(RT5372) || defined(RT5390) || defined(RT5392)

	     driver need backward compatible, so we need change these values dynamicly.
	*/
	/*
	2010/08/12:
		In windows's team, they found that the 5390 could not use the high memory. 
		Becuase the AP will switch to high-memory to update the beacon every 100 ms in the power saving mode and 
		then switch to low memory to update other information likes MCS. But our hardware does lock the high-low
		memory switch bit correctly, they encounter the mass contnet of memory in power-saving testing in WHQL.
		For fixing this bug, the hardware WAPI and Beacom will coexist in low-memory. Now we do not the same 
		modifications, because we expect that we will not meet the same situation. We will use the original codes to
		pass QA test, if it could pass, we will decide to change the beacon buffer as Windows's team.
	*/
#endif // RELEASE_EXCLUDE //
	if (MAX_LEN_OF_MAC_TABLE > MAX_AVAILABLE_CLIENT_WCID(pAd))
	{
		DBGPRINT(RT_DEBUG_ERROR, ("MAX_LEN_OF_MAC_TABLE can not be larger than MAX_AVAILABLE_CLIENT_WCID!!!!\n"));
		goto err1;
	}

#ifdef CONFIG_AP_SUPPORT
	/* Init BssTab & ChannelInfo tabbles for auto channel select.*/
	IF_DEV_CONFIG_OPMODE_ON_AP(pAd)
	{
		AutoChBssTableInit(pAd);
		ChannelInfoInit(pAd);
	}
#endif /* CONFIG_AP_SUPPORT */

#ifdef DOT11_N_SUPPORT
	/* Allocate BA Reordering memory*/
	if (ba_reordering_resource_init(pAd, MAX_REORDERING_MPDU_NUM) != TRUE)		
		goto err1;
#endif /* DOT11_N_SUPPORT */

#ifdef RESOURCE_PRE_ALLOC
	Status = RTMPInitTxRxRingMemory(pAd);
#else
	Status = RTMPAllocTxRxRingMemory(pAd);
#endif /* RESOURCE_PRE_ALLOC */
	if (Status != NDIS_STATUS_SUCCESS)
	{
		DBGPRINT_ERR(("RTMPAllocTxRxMemory failed, Status[=0x%08x]\n", Status));
		goto err2;
	}

#ifdef RTMP_MAC_USB
#ifdef MULTI_URB
	pAd->CommonCfg.bMultipleIRP = TRUE;
#else
	pAd->CommonCfg.bMultipleIRP = FALSE;
#endif
	if (pAd->CommonCfg.bMultipleIRP)
		pAd->CommonCfg.NumOfBulkInIRP = RX_RING_SIZE;
	else
		pAd->CommonCfg.NumOfBulkInIRP = 1;
#endif /* RTMP_MAC_USB */

#ifdef WLAN_SKB_RECYCLE
	skb_queue_head_init(&pAd->rx0_recycle);
#endif /* WLAN_SKB_RECYCLE */

	RTMP_SET_FLAG(pAd, fRTMP_ADAPTER_INTERRUPT_IN_USE);

	/* initialize MLME*/
#ifdef RT6352
	if (IS_RT6352(pAd))
		pAd->bCalibrationDone = FALSE;
#endif /* RT6352 */
	
	Status = RtmpMgmtTaskInit(pAd);
	if (Status != NDIS_STATUS_SUCCESS)
		goto err3;

	Status = MlmeInit(pAd);
	if (Status != NDIS_STATUS_SUCCESS)
	{
		DBGPRINT_ERR(("MlmeInit failed, Status[=0x%08x]\n", Status));
		goto err4;
	}

	/* Initialize pAd->StaCfg, pAd->ApCfg, pAd->CommonCfg to manufacture default*/
	UserCfgInit(pAd);

#ifdef MESH_SUPPORT
	MeshCfgInit(pAd, pHostName);
#endif /* MESH_SUPPORT */

	Status = RtmpNetTaskInit(pAd);
	if (Status != NDIS_STATUS_SUCCESS)
		goto err5;

	CfgInitHook(pAd);

#ifdef BLOCK_NET_IF
	initblockQueueTab(pAd);
#endif /* BLOCK_NET_IF */

	Status = MeasureReqTabInit(pAd);
	if (Status != NDIS_STATUS_SUCCESS)
	{
		DBGPRINT_ERR(("MeasureReqTabInit failed, Status[=0x%08x]\n",Status));
		goto err6;
	}
	Status = TpcReqTabInit(pAd);
	if (Status != NDIS_STATUS_SUCCESS)
	{
		DBGPRINT_ERR(("TpcReqTabInit failed, Status[=0x%08x]\n",Status));
		goto err6;
	}

	/*
		WiFi system operation mode setting base on following partitions:
		1. Parameters from config file
		2. Hardware cap from EEPROM
		3. Chip capabilities in code
	*/
	pAd->RfIcType = RFIC_UNKNOWN;
	Status = RTMPReadParametersHook(pAd);
	if (Status != NDIS_STATUS_SUCCESS)
	{
		DBGPRINT_ERR(("RTMPReadParametersHook failed, Status[=0x%08x]\n",Status));
		goto err6;
	}
	DBGPRINT(RT_DEBUG_OFF, ("1. Phy Mode = %d\n", pAd->CommonCfg.PhyMode));

	/* We should read EEPROM for all cases */
	NICReadEEPROMParameters(pAd, (PSTRING)pDefaultMac);
#ifdef CONFIG_STA_SUPPORT
#ifdef ETH_CONVERT_SUPPORT
	IF_DEV_CONFIG_OPMODE_ON_STA(pAd)
	{
		extern UCHAR ZERO_MAC_ADDR[MAC_ADDR_LEN];

		if ( NdisEqualMemory(&pAd->EthConvert.EthCloneMac[0], &ZERO_MAC_ADDR[0], MAC_ADDR_LEN))
		{
			NdisMoveMemory(&pAd->EthConvert.EthCloneMac[0], &pAd->CurrentAddress[0], MAC_ADDR_LEN);
			DBGPRINT(RT_DEBUG_OFF, ("Read EEPROM, EthCloneMac is %02x:%02x:%02x:%02x:%02x:%02x!\n",
					PRINT_MAC(pAd->EthConvert.EthCloneMac)));
		}
	}
#endif /* ETH_CONVERT_SUPPORT */
#endif /* CONFIG_STA_SUPPORT */
	DBGPRINT(RT_DEBUG_OFF, ("2. Phy Mode = %d\n", pAd->CommonCfg.PhyMode));

	/* After operation mode is finialized, init the AP or STA mode */
#ifdef CONFIG_AP_SUPPORT
	IF_DEV_CONFIG_OPMODE_ON_AP(pAd)
	{
		APInitialize(pAd);

#ifdef DOT11K_RRM_SUPPORT
                RRM_CfgInit(pAd);
#endif /* DOT11K_RRM_SUPPORT */
	}
#endif /* CONFIG_AP_SUPPORT */

#ifdef CONFIG_STA_SUPPORT
	IF_DEV_CONFIG_OPMODE_ON_STA(pAd) {
		STAInitialize(pAd);

#ifdef CREDENTIAL_STORE
		RecoverConnectInfo(pAd);
#endif /* CREDENTIAL_STORE */
	}
#endif /* CONFIG_STA_SUPPORT */


#ifdef DOT11_N_SUPPORT
   	/*Init Ba Capability parameters.*/
	pAd->CommonCfg.DesiredHtPhy.MpduDensity = (UCHAR)pAd->CommonCfg.BACapability.field.MpduDensity;
	pAd->CommonCfg.DesiredHtPhy.AmsduEnable = (USHORT)pAd->CommonCfg.BACapability.field.AmsduEnable;
	pAd->CommonCfg.DesiredHtPhy.AmsduSize = (USHORT)pAd->CommonCfg.BACapability.field.AmsduSize;
	pAd->CommonCfg.DesiredHtPhy.MimoPs = (USHORT)pAd->CommonCfg.BACapability.field.MMPSmode;
	/* UPdata to HT IE*/
	pAd->CommonCfg.HtCapability.HtCapInfo.MimoPs = (USHORT)pAd->CommonCfg.BACapability.field.MMPSmode;
	pAd->CommonCfg.HtCapability.HtCapInfo.AMsduSize = (USHORT)pAd->CommonCfg.BACapability.field.AmsduSize;
	pAd->CommonCfg.HtCapability.HtCapParm.MpduDensity = (UCHAR)pAd->CommonCfg.BACapability.field.MpduDensity;
#endif /* DOT11_N_SUPPORT */

	/* after reading Registry, we now know if in AP mode or STA mode */
	DBGPRINT(RT_DEBUG_OFF, ("3. Phy Mode = %d\n", pAd->CommonCfg.PhyMode));


	/*
		All settle down, now it's time to init asic related parameters
	*/
	/* Init the hardware, we need to init asic before read registry, otherwise mac register will be reset */
	Status = NICInitializeAdapter(pAd, TRUE);
	if (Status != NDIS_STATUS_SUCCESS)
	{
		DBGPRINT_ERR(("NICInitializeAdapter failed, Status[=0x%08x]\n", Status));
		if (Status != NDIS_STATUS_SUCCESS)
		goto err6;
	}

#ifdef CONFIG_FPGA_MODE
#ifdef CAPTURE_MODE
	cap_mode_init(pAd);
#endif /* CAPTURE_MODE */
#endif /* CONFIG_FPGA_MODE */

	NICInitAsicFromEEPROM(pAd);

#ifdef LED_CONTROL_SUPPORT
	/* Send LED Setting to MCU */
	RTMPInitLEDMode(pAd);	
#endif /* LED_CONTROL_SUPPORT */


	/*
		Do necessary calibration after ASIC initialized
		this's chip variant and may different for different chips
	*/
#ifdef RT6352
	if (IS_RT6352(pAd))
	{
		RtmpKickOutHwNullFrame(pAd, TRUE, FALSE);

#if defined(RT6352_EP_SUPPORT) || defined(RT6352_EL_SUPPORT)
		{
			ULONG SysRegValue;

			RTMP_SYS_IO_READ32(0xb0000060, &SysRegValue);
			if ((SysRegValue & 0x100000) == 0x0)
			{
				SysRegValue |= 0x100000;
				RTMP_SYS_IO_WRITE32(0xb0000060, SysRegValue);
				DBGPRINT(RT_DEBUG_ERROR,("Change as GPIO Mode(0x%x)\n", SysRegValue));
			}
		}
#endif /* defined(RT6352_EP_SUPPORT) || defined(RT6352_EL_SUPPORT) */

		/* Do R-Calibration */
		R_Calibration(pAd);

#ifdef RTMP_TEMPERATURE_CALIBRATION
		/* Temperature Init */
		RT6352_Temperature_Init(pAd);		
		RT6352_TemperatureCalibration(pAd);
#endif /* RTMP_TEMPERATURE_CALIBRATION */

#ifdef RTMP_TEMPERATURE_COMPENSATION
		/*
			read out tempature reference value (0x80 ~ 0x7F)
			TssiPlusBoundaryG [7] [6] [5] [4] [3] [2] [1] [0] (smaller) +
			TssiMinusBoundaryG[0] [1] [2] [3] [4] [5] [6] [7] (larger)
		*/
		RT6352_EEPROM_TSSI_24G_READ(pAd);
		/* 
			pAd->TssiCalibratedOffset: 
			reference temperature(e2p[D1h])
		*/				
		/* adjust the boundary table by pAd->TssiCalibratedOffset */
		RT6352_TssiTableAdjust(pAd);

		/* ATE temperature(e2p[77h]) */
		RT6352_TssiMpAdjust(pAd);

		DBGPRINT(RT_DEBUG_OFF,("E2PROM: G Tssi[-7 .. +7] = %d %d %d %d %d %d %d - %d - %d %d %d %d %d %d %d, offset=%d, tuning=%d\n",
			pAd->TssiMinusBoundaryG[7], pAd->TssiMinusBoundaryG[6], pAd->TssiMinusBoundaryG[5],
			pAd->TssiMinusBoundaryG[4], pAd->TssiMinusBoundaryG[3], pAd->TssiMinusBoundaryG[2], pAd->TssiMinusBoundaryG[1],
			pAd->TssiRefG,
			pAd->TssiPlusBoundaryG[1], pAd->TssiPlusBoundaryG[2], pAd->TssiPlusBoundaryG[3], pAd->TssiPlusBoundaryG[4],
			pAd->TssiPlusBoundaryG[5], pAd->TssiPlusBoundaryG[6], pAd->TssiPlusBoundaryG[7],
			pAd->TssiCalibratedOffset, pAd->bAutoTxAgcG));
#endif /* RTMP_TEMPERATURE_COMPENSATION */

		AsicSwitchChannel(pAd, pAd->CommonCfg.Channel, TRUE);
		AsicLockChannel(pAd, pAd->CommonCfg.Channel);

		/* RF Self TX DC Calibration */
		RF_SELF_TXDC_CAL(pAd);

		/* Rx DCOC Calibration */
		RxDCOC_Calibration(pAd);

		/* BandWidth Filter Calibration */
		BW_Filter_Calibration(pAd,TRUE);
		BW_Filter_Calibration(pAd,FALSE);

		/* Do LOFT and IQ Calibration */
		LOFT_IQ_Calibration(pAd);

		/* DPD_Calibration */
#ifdef RT6352_EP_SUPPORT
		if (pAd->bExtPA == FALSE)
#endif /* RT6352_EP_SUPPORT */
		{
			DoDPDCalibration(pAd);
			pAd->DoDPDCurrTemperature = 0x7FFFFFFF;
		}

		/* Rx DCOC Calibration */
		RxDCOC_Calibration(pAd);

		/* Do RXIQ Calibration */
		RXIQ_Calibration(pAd);

#if defined(RT6352_EP_SUPPORT) || defined(RT6352_EL_SUPPORT)
		RT6352_Init_ExtPA_ExtLNA(pAd, FALSE);
#endif /* defined(RT6352_EP_SUPPORT) || defined(RT6352_EL_SUPPORT) */
	}
#endif /* RT6352 */

#ifdef RALINK_ATE
	if (ATEInit(pAd) != NDIS_STATUS_SUCCESS)
	{
		DBGPRINT(RT_DEBUG_ERROR, ("%s(): ATE initialization failed !\n", __FUNCTION__));
		goto err6;
	}
#endif /* RALINK_ATE */

#ifdef RTMP_FREQ_CALIBRATION_SUPPORT
#ifdef CONFIG_STA_SUPPORT
	/* Initialize the frequency calibration*/
	if (pAd->chipCap.FreqCalibrationSupport)
		FrequencyCalibration(pAd);	
#endif /* CONFIG_STA_SUPPORT */
#endif /* RTMP_FREQ_CALIBRATION_SUPPORT */

#ifdef RTMP_INTERNAL_TX_ALC
	/* Initialize the desired TSSI table*/
	RTMP_CHIP_ASIC_TSSI_TABLE_INIT(pAd);
#endif /* RTMP_INTERNAL_TX_ALC */

#ifdef RT6352
	if (IS_RT6352(pAd))
		InitRfPaModeTable(pAd);
#endif /* RT6352 */

#ifdef RT8592
	// TODO: shiang-6590, actually, this operation shall be move to bbp_init
	if (IS_RT8592(pAd))
		bw_filter_cal(pAd);
#endif /* RT8592 */

#ifdef RTMP_TEMPERATURE_COMPENSATION
	/* Temperature compensation, initialize the lookup table */
	DBGPRINT(RT_DEBUG_OFF, ("bAutoTxAgcG = %d\n", pAd->bAutoTxAgcG));

	if (pAd->chipCap.bTempCompTxALC && pAd->bAutoTxAgcG)
		InitLookupTable(pAd);
#endif /* RTMP_TEMPERATURE_COMPENSATION */

#ifdef RTMP_FREQ_CALIBRATION_SUPPORT
#ifdef CONFIG_STA_SUPPORT
	if (pAd->chipCap.FreqCalibrationSupport)
		InitFrequencyCalibration(pAd);
#endif /* CONFIG_STA_SUPPORT */
#endif /* RTMP_FREQ_CALIBRATION_SUPPORT */

	/* Set PHY to appropriate mode and will update the ChannelListNum in this function */
	RTMPSetPhyMode(pAd, pAd->CommonCfg.PhyMode);
	if (pAd->ChannelListNum == 0)
	{
		DBGPRINT(RT_DEBUG_ERROR, ("Wrong configuration. No valid channel found. Check \"ContryCode\" and \"ChannelGeography\" setting.\n"));
		goto err6;
	}

#ifdef DOT11_N_SUPPORT
	DBGPRINT(RT_DEBUG_OFF, ("MCS Set = %02x %02x %02x %02x %02x\n", pAd->CommonCfg.HtCapability.MCSSet[0],
           pAd->CommonCfg.HtCapability.MCSSet[1], pAd->CommonCfg.HtCapability.MCSSet[2],
           pAd->CommonCfg.HtCapability.MCSSet[3], pAd->CommonCfg.HtCapability.MCSSet[4]));
#endif /* DOT11_N_SUPPORT */

#ifdef WIN_NDIS
	/* Patch cardbus controller if EEPROM said so. */
	if (pAd->bTest1 == FALSE)
		RTMPPatchCardBus(pAd);
#endif /* WIN_NDIS */

#ifdef CONFIG_AP_SUPPORT
	IF_DEV_CONFIG_OPMODE_ON_AP(pAd)
	{
#ifdef AP_QLOAD_SUPPORT
		QBSS_LoadInit(pAd);
#endif /* AP_QLOAD_SUPPORT */
	}
#endif /* CONFIG_AP_SUPPORT */

	/* APInitialize(pAd);*/

#ifdef IKANOS_VX_1X0
	VR_IKANOS_FP_Init(pAd->ApCfg.BssidNum, pAd->PermanentAddress);
#endif /* IKANOS_VX_1X0 */

#ifdef RELEASE_EXCLUDE
	/*
		Recorder: Sample Lin
		Co-worker: Albert Yang, Alex Lin, Albert Liu, Shawn Hsiao
		Date: 2009/12/29

		1. Motivation: Large power consumption problem in RT2870 only.

		2. Problem: When we insert/remove rt2870 station and then insert
			rt2870 ap driver, the current will reach 7xx mA, but the maximum
			legal current is 500mA in USB device.

		3. Phenomenon:
			We found 8051 firmware will update 0x1328 to default value 0x0f50f,
			i.e. enable PA and LNA for 2.4G and 5GHz.

			But if AP is located in 2.4GHz, no need to enable PA/LNA for 5GHz;
			if AP is located in 5GHz, no need to enable PA/LNA for 2.4GHz.

		4. Analysis:
			(1) Our driver will change 0x1328 only when channel switch to
				correct value 0x0f50a (2.4GHz) or 0x0f505 (5GHz);
				But the value will be changed to the wrong value 0x0f50f
				after interface up.
			(2) The we found if we issue the MCU command 0x30 in radio off
				function of station driver, the MAC 0x1328 will be changed
				automatically in AP driver.
			(3) Albert Liu said that 8051 only update MAC 0x1328 when it
				receives 0x31 MCU command.
			(4) If we do not upload 8051 firmware in AP driver after station
				driver is removed, no the problem occur.
				So we think that when 8051 is on work, if we upload 8051
				firmware, any unpredicted result will occur.

		5. Solution:
			(1) From Alex:
				Use new firmware and send 0x31 MCU command before channel
				switch. The new firmware will save 0x1328 value when you issue
				0x30 MCU sleep command and restore 0x1328 value when you issue
				0x31 MCU wake up command.
			(2) From Albert Liu: (final solution, no need to patch firmware)
				Or reset MCU before 8051 firmware upload and 8051 will reset
				anything in AP or STA driver for USB device.
				The solution can not be used in PCI/PCIe device because
				USB device has a ROM kept a simple 8051 firmware but no ROM
				on PCI/PCIe device:

				a. When USB up, it will run the 8051 on ROM;
				b. When we issue a reset MCU command, USB will translate
					to the 8051 on ROM if we have ever run 8051 before;
				c. And we upload firmware;
				d. Issue a vendor request 0x01, 0x8 to run the 8051 on RAM;

				So we must reset 8051 before firmware upload.
				But the side effect is we often receive CRC-ERROR packets.
			(3) Modify EEPROM 0x2e1 = 0x80 to disable 8051 recover 0x1328
				value when 1st beacon is sent.
				Because the code in ROM (not RAM) of USB device will check
				the bit.
				If our customer can not accept to modify EEPROM, we will use
				solution 1.
	*/
#endif /* RELEASE_EXCLUDE */
#ifdef RTMP_MAC_USB
	if (pAd->chipCap.MCUType == M8051) {
		AsicSendCommandToMcu(pAd, 0x31, 0xff, 0x00, 0x02, FALSE);
		RtmpusecDelay(10000);
	}
#endif /* RTMP_MAC_USB */

#ifdef RALINK_ATE
#ifdef RT5592
#ifdef RTMP_RF_RW_SUPPORT
	/* both for RT5572 and RT5592 */
	if (IS_RT5592(pAd))
	{
		RT55x2ATEFilterCalibration(pAd);
	}
#endif /* RTMP_RF_RW_SUPPORT */
#endif /* RT5592 */
#endif /* RALINK_ATE */


#ifdef RTMP_INTERNAL_TX_ALC
#ifdef RT6352
	if (IS_RT6352(pAd) && (pAd->TxPowerCtrl.bInternalTxALC == TRUE))
	{
		RT635xTssiDcCalibration(pAd);
	}
#endif /* RT6352 */
#endif /* RTMP_INTERNAL_TX_ALC */

	/*
		Some modules init must be called before APStartUp().
		Or APStartUp() will make up beacon content and call
		other modules API to get some information to fill.
	*/
#ifdef WMM_ACM_SUPPORT
#ifdef CONFIG_AP_SUPPORT
	if (ACMR_IS_AP_MODE(pAd))
	{
		ACMP_Init(pAd,
				pAd->CommonCfg.APEdcaParm.bACM[0],
				pAd->CommonCfg.APEdcaParm.bACM[1],
				pAd->CommonCfg.APEdcaParm.bACM[2],
				pAd->CommonCfg.APEdcaParm.bACM[3], 0);
	}
#endif /* CONFIG_AP_SUPPORT */
#ifdef CONFIG_STA_SUPPORT
	if (ACMR_IS_STA_MODE(pAd))
	{
		ACMP_Init(pAd, 0, 0, 0, 0, 0);
	}
#endif /* CONFIG_STA_SUPPORT */
#endif /* WMM_ACM_SUPPORT */

#ifdef RELEASE_EXCLUDE
	/*
		8051 firmware require the signal during booting time.
		2008/11/28:KH marked the following codes to patch Frequency offset bug

		AsicSendCommandToMcu(pAd, 0x72, 0xFF, 0x00, 0x00, FALSE);
	*/
#endif /* RELEASE_EXCLUDE */

#ifdef SMART_ANTENNA
	RtmpSAInit(pAd);
#endif /* SMART_ANTENNA */

	if (pAd && (Status != NDIS_STATUS_SUCCESS))
	{
		if (RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_INTERRUPT_IN_USE))
		{
			RTMP_CLEAR_FLAG(pAd, fRTMP_ADAPTER_INTERRUPT_IN_USE);
		}
	}
	else if (pAd)
	{
		/* Microsoft HCT require driver send a disconnect event after driver initialization.*/
		OPSTATUS_CLEAR_FLAG(pAd, fOP_STATUS_MEDIA_STATE_CONNECTED);
		OPSTATUS_CLEAR_FLAG(pAd, fOP_AP_STATUS_MEDIA_STATE_CONNECTED);

		DBGPRINT(RT_DEBUG_TRACE, ("NDIS_STATUS_MEDIA_DISCONNECT Event B!\n"));

#ifdef CONFIG_AP_SUPPORT
		IF_DEV_CONFIG_OPMODE_ON_AP(pAd)
		{
			if (pAd->ApCfg.bAutoChannelAtBootup || (pAd->CommonCfg.Channel == 0))
			{
				/* Enable Interrupt first due to we need to scan channel to receive beacons.*/
#ifdef RTMP_MAC_PCI
				RTMP_IRQ_ENABLE(pAd);
#endif /* RTMP_MAC_PCI */
#ifdef RTMP_MAC_USB
				RTMP_CLEAR_FLAG(pAd, fRTMP_ADAPTER_RESET_IN_PROGRESS);
				RTMP_CLEAR_FLAG(pAd, fRTMP_ADAPTER_REMOVE_IN_PROGRESS);
				
				/* Support multiple BulkIn IRP,*/
				/* the value on pAd->CommonCfg.NumOfBulkInIRP may be large than 1.*/
#ifdef MULTI_URB				
				for(index=0; index<SUBMIT_URB_NUM; index++)
				{					
					DoBulkIn(pAd);
					DBGPRINT(RT_DEBUG_TRACE, ("DoBulkIn!\n" ));
				}
#else
				for(index=0; index<pAd->CommonCfg.NumOfBulkInIRP; index++)
				{
					RTUSBBulkReceive(pAd);
					DBGPRINT(RT_DEBUG_TRACE, ("RTUSBBulkReceive!\n" ));
				}
#endif
#endif /* RTMP_MAC_USB */

				/* Now Enable RxTx*/
				RTMPEnableRxTx(pAd);
#ifdef MT762x
				// TODO: shiang-usw, check why MT76x2 don't need to set this flag here!
				if (!IS_MT762x(pAd))
#endif /* MT762x */
					RTMP_SET_FLAG(pAd, fRTMP_ADAPTER_START_UP);

				/* Let BBP register at 20MHz to do scan */
				bbp_set_bw(pAd, BW_20);

				/* Now we can receive the beacon and do the listen beacon*/
				/* use default BW to select channel*/
				pAd->CommonCfg.Channel = AP_AUTO_CH_SEL(pAd, pAd->ApCfg.AutoChannelAlg);
				pAd->ApCfg.bAutoChannelAtBootup = FALSE;
			}

#ifdef DOT11_N_SUPPORT
			/* If WMODE_CAP_N(phymode) and BW=40 check extension channel, after select channel  */
			N_ChannelCheck(pAd);

#ifdef DOT11N_DRAFT3
        		/* 
         			We only do this Overlapping BSS Scan when system up, for the 
				other situation of channel changing, we depends on station's 
				report to adjust ourself.
			*/
			if (pAd->CommonCfg.bForty_Mhz_Intolerant == TRUE)
			{
				DBGPRINT(RT_DEBUG_TRACE, ("Disable 20/40 BSSCoex Channel Scan(BssCoex=%d, 40MHzIntolerant=%d)\n", 
											pAd->CommonCfg.bBssCoexEnable, 
											pAd->CommonCfg.bForty_Mhz_Intolerant));
			}
			else if(pAd->CommonCfg.bBssCoexEnable == TRUE)
			{	
				DBGPRINT(RT_DEBUG_TRACE, ("Enable 20/40 BSSCoex Channel Scan(BssCoex=%d)\n", 
							pAd->CommonCfg.bBssCoexEnable));
				APOverlappingBSSScan(pAd);
			}

			RTMP_11N_D3_TimerInit(pAd);
/*			RTMPInitTimer(pAd, &pAd->CommonCfg.Bss2040CoexistTimer, GET_TIMER_FUNCTION(Bss2040CoexistTimeOut), pAd, FALSE);*/
#endif /* DOT11N_DRAFT3 */
#endif /* DOT11_N_SUPPORT */

#ifdef RELEASE_EXCLUDE
			/*
			3090, 3090A and 3390 all support hadware tone radar function. But the soluation of those are different.
			3090 is the old one.
			*/
#endif /* RELEASE_EXCLUDE */
#ifdef RT3090
#ifdef CARRIER_DETECTION_SUPPORT
			if (IS_RT3090A(pAd)||IS_RT3390(pAd))
				pAd->chipCap.carrier_func=TONE_RADAR_V2;
			else
				pAd->chipCap.carrier_func=TONE_RADAR_V1;
#endif /* CARRIER_DETECTION_SUPPORT */
#endif /* RT3090 */

			APStartUp(pAd);
			DBGPRINT(RT_DEBUG_OFF, ("Main bssid = %02x:%02x:%02x:%02x:%02x:%02x\n", 
						PRINT_MAC(pAd->ApCfg.MBSSID[BSS0].wdev.bssid)));

#ifdef DYNAMIC_VGA_SUPPORT
#ifdef MT76x2
			if (IS_MT76x2(pAd)) {
				mt76x2_reinit_agc_gain(pAd, pAd->hw_cfg.cent_ch);
				mt76x2_reinit_hi_lna_gain(pAd, pAd->hw_cfg.cent_ch);
				mt76x2_get_agc_gain(pAd, TRUE);
			}
#endif /* MT76x2 */
#endif /* DYNAMIC_VGA_SUPPORT */
		}
#endif /* CONFIG_AP_SUPPORT */

#ifdef RT6352
		if (IS_RT6352(pAd))
		{
#ifdef DYNAMIC_VGA_SUPPORT
			if (pAd->CommonCfg.lna_vga_ctl.bDyncVgaEnable == TRUE)
		{
				rt6352_dynamic_vga_enable(pAd);
		}
#endif /* DYNAMIC_VGA_SUPPORT */

			pAd->bCalibrationDone = TRUE;
		}
#endif /* RT6352 */

#ifdef RTMP_MAC_USB
		RTMP_CLEAR_FLAG(pAd, fRTMP_ADAPTER_RESET_IN_PROGRESS);
		RTMP_CLEAR_FLAG(pAd, fRTMP_ADAPTER_REMOVE_IN_PROGRESS);

		/* Support multiple BulkIn IRP,*/
		/* the value on pAd->CommonCfg.NumOfBulkInIRP may be large than 1.*/		
#ifdef MULTI_URB				
		for(index=0; index<SUBMIT_URB_NUM; index++)
		{					
			DoBulkIn(pAd);
			DBGPRINT(RT_DEBUG_TRACE, ("DoBulkIn!\n" ));
		}
#else
		for (index=0; index<pAd->CommonCfg.NumOfBulkInIRP; index++)
		{
			RTUSBBulkReceive(pAd);
			DBGPRINT(RT_DEBUG_TRACE, ("RTUSBBulkReceive!\n" ));
		}
#endif
#endif /* RTMP_MAC_USB */
	}

	/* Set up the Mac address*/
#ifdef CONFIG_AP_SUPPORT
#ifndef P2P_APCLI_SUPPORT
	RtmpOSNetDevAddrSet(pAd->OpMode, pAd->net_dev, &pAd->CurrentAddress[0], NULL);
#endif /* P2P_APCLI_SUPPORT */
#endif /* CONFIG_AP_SUPPORT */
#ifdef CONFIG_STA_SUPPORT
	NdisMoveMemory(&pAd->StaCfg.wdev.if_addr[0], &pAd->CurrentAddress[0], MAC_ADDR_LEN);
	RtmpOSNetDevAddrSet(pAd->OpMode, pAd->net_dev, &pAd->CurrentAddress[0], (PUCHAR)(pAd->StaCfg.dev_name));
	NdisMoveMemory(&pAd->StaCfg.wdev.if_addr[0], &pAd->CurrentAddress[0], MAC_ADDR_LEN);
#endif /* CONFIG_STA_SUPPORT */

#ifdef UAPSD_SUPPORT
        UAPSD_Init(pAd);
#endif /* UAPSD_SUPPORT */

	/* assign function pointers*/
#ifdef MAT_SUPPORT
	/* init function pointers, used in OS_ABL */
	RTMP_MATOpsInit(pAd);
#endif /* MAT_SUPPORT */

#ifdef RTMP_RBUS_SUPPORT
	if (pAd->infType == RTMP_DEV_INF_RBUS)
	{
#ifdef VIDEO_TURBINE_SUPPORT
		VideoTurbineDynamicTune(pAd);
#endif /* VIDEO_TURBINE_SUPPORT */

#ifdef RT3XXX_ANTENNA_DIVERSITY_SUPPORT
		RT3XXX_AntDiversity_Init(pAd);
#endif /* RT3XXX_ANTENNA_DIVERSITY_SUPPORT */
	}
#endif /* RTMP_RBUS_SUPPORT */

#ifdef CONFIG_AP_SUPPORT
	IF_DEV_CONFIG_OPMODE_ON_AP(pAd)
	{
#ifdef MAT_SUPPORT
		MATEngineInit(pAd);
#endif /* MAT_SUPPORT */

#ifdef CLIENT_WDS
		CliWds_ProxyTabInit(pAd);
#endif /* CLIENT_WDS */
	}
#endif /* CONFIG_AP_SUPPORT */

#ifdef CONFIG_STA_SUPPORT
	IF_DEV_CONFIG_OPMODE_ON_STA(pAd)
	{
#ifdef DOT11Z_TDLS_SUPPORT
		TDLS_Table_Init(pAd);
#endif /* DOT11Z_TDLS_SUPPORT */

#ifdef WPA_SUPPLICANT_SUPPORT
#ifndef NATIVE_WPA_SUPPLICANT_SUPPORT
		/* send wireless event to wpa_supplicant for infroming interface up.*/
		RtmpOSWrielessEventSend(pAd->net_dev, RT_WLAN_EVENT_CUSTOM, RT_INTERFACE_UP, NULL, NULL, 0);
#endif /* NATIVE_WPA_SUPPLICANT_SUPPORT */
#endif /* WPA_SUPPLICANT_SUPPORT */

#ifdef ETH_CONVERT_SUPPORT
		MATEngineInit(pAd);
#endif /* ETH_CONVERT_SUPPORT */
	}
#endif /* CONFIG_STA_SUPPORT */

	/* auto-fall back settings */
#ifdef RT6352
	if (IS_RT6352(pAd))
		RTMP_IO_WRITE32(pAd, HT_FBK_CFG1, 0xedcba980);
#endif /* RT6352 */
#ifdef DOT11N_SS3_SUPPORT
	if (pAd->CommonCfg.TxStream >= 3)
	{
		RTMP_IO_WRITE32(pAd, TX_FBK_CFG_3S_0, 0x12111008);
		RTMP_IO_WRITE32(pAd, TX_FBK_CFG_3S_1, 0x16151413);
	}
#endif /* DOT11N_SS3_SUPPORT */

#ifdef STREAM_MODE_SUPPORT
	RtmpStreamModeInit(pAd);
#endif /* STREAM_MODE_SUPPORT */

#if defined(RT2883) || defined(RT3883)
	if (IS_RT2883(pAd) || IS_RT3883(pAd))
	{
		UINT8 BBPValue = 0;
		BBP_IO_READ8_BY_REG_ID(pAd, BBP_R65, &BBPValue);
		
		if (pAd->CommonCfg.FineAGC)
			BBPValue |= 0x40; /* turn on fine AGC*/
		else
			BBPValue &= ~0x40; /* turn off fine AGC*/
		BBP_IO_WRITE8_BY_REG_ID(pAd, BBP_R65, BBPValue);
	}
#endif /* defined(RT2883) || defined(RT3883) */

#ifdef DOT11_N_SUPPORT
#ifdef TXBF_SUPPORT
#ifndef MT76x2
	if (pAd->CommonCfg.ITxBfTimeout)
	{
		RTMP_BBP_IO_WRITE8_BY_REG_ID(pAd, BBP_R179, 0x02);
		RTMP_BBP_IO_WRITE8_BY_REG_ID(pAd, BBP_R180, 0);
		RTMP_BBP_IO_WRITE8_BY_REG_ID(pAd, BBP_R182, pAd->CommonCfg.ITxBfTimeout & 0xFF);
		RTMP_BBP_IO_WRITE8_BY_REG_ID(pAd, BBP_R180, 1);
		RTMP_BBP_IO_WRITE8_BY_REG_ID(pAd, BBP_R182, (pAd->CommonCfg.ITxBfTimeout>>8) & 0xFF);
	}
#endif	

	if (pAd->CommonCfg.ETxBfTimeout)
	{
		RTMP_IO_WRITE32(pAd, TX_TXBF_CFG_3, pAd->CommonCfg.ETxBfTimeout);
	}
#endif /* TXBF_SUPPORT */
#endif /* DOT11_N_SUPPORT */

#ifdef RT305x
	RTMP_CHIP_SPECIFIC(pAd, RT305x_INITIALIZATION, NULL, 0);
#endif /* RT305x */

#ifdef SIGMATEL_SDK
	SYControlInit(pAd);
#endif /* SIGMATEL_SDK 	*/

#ifdef RT3290
	if (IS_RT3290(pAd))
	{
		WLAN_FUN_CTRL_STRUC WlanFunCtrl = {.word = 0};
		RTMP_MAC_PWRSV_EN(pAd, TRUE, TRUE);
		//
		// Too much time for reading efuse(enter/exit L1), and our device will hang up
		// Enable L1
		//
		RTMP_IO_READ32(pAd, WLAN_FUN_CTRL, &WlanFunCtrl.word);
		if (WlanFunCtrl.field.WLAN_EN == TRUE)
		{
			WlanFunCtrl.field.PCIE_APP0_CLK_REQ = FALSE;
			RTMP_IO_WRITE32(pAd, WLAN_FUN_CTRL, WlanFunCtrl.word);
		}
	}
#endif /* RT3290 */

	DBGPRINT_S(Status, ("<==== rt28xx_init, Status=%x\n", Status));

	return TRUE;

err6:
#ifdef SMART_ANTENNA
	RtmpSAExit(pAd);
#endif /* SMART_ANTENNA */

#ifdef IGMP_SNOOP_SUPPORT
	MultiCastFilterTableReset(&pAd->pMulticastFilterTable);
#endif /* IGMP_SNOOP_SUPPORT */

	MeasureReqTabExit(pAd);
	TpcReqTabExit(pAd);
err5:	
	RtmpNetTaskExit(pAd);
	UserCfgExit(pAd);
err4:	
	MlmeHalt(pAd);
	RTMP_AllTimerListRelease(pAd);
err3:	
	RtmpMgmtTaskExit(pAd);
#ifdef RTMP_TIMER_TASK_SUPPORT
	NdisFreeSpinLock(&pAd->TimerQLock);
#endif /* RTMP_TIMER_TASK_SUPPORT */
err2:
#ifdef RESOURCE_PRE_ALLOC
	RTMPResetTxRxRingMemory(pAd);
#else
	RTMPFreeTxRxRingMemory(pAd);
#endif /* RESOURCE_PRE_ALLOC */

err1:

	MCU_CTRL_EXIT(pAd);

#ifdef CONFIG_AP_SUPPORT
	IF_DEV_CONFIG_OPMODE_ON_AP(pAd) {
		/* Free BssTab & ChannelInfo tabbles.*/
		AutoChBssTableDestroy(pAd);
		ChannelInfoDestroy(pAd);
	}
#endif /* CONFIG_AP_SUPPORT */

#ifdef RT3290
	if (IS_RT3290(pAd))
		RTMPEnableWlan(pAd, FALSE, FALSE);
#endif /* RT3290 */

#ifdef DOT11_N_SUPPORT
	if(pAd->mpdu_blk_pool.mem)
		os_free_mem(pAd, pAd->mpdu_blk_pool.mem); /* free BA pool*/
#endif /* DOT11_N_SUPPORT */

#ifdef INF_AMAZON_SE
err0:
#endif /* INF_AMAZON_SE */
#ifdef ST
err0:
#endif /* ST */

	DBGPRINT(RT_DEBUG_ERROR, ("!!! rt28xx init fail !!!\n"));
	return FALSE;
}


VOID RTMPDrvOpen(VOID *pAdSrc)
{
	RTMP_ADAPTER *pAd = (RTMP_ADAPTER *)pAdSrc;

	RTMP_CLEAR_PSFLAG(pAd, fRTMP_PS_MCU_SLEEP);
#ifdef CONFIG_STA_SUPPORT
#ifdef DOT11R_FT_SUPPORT
	IF_DEV_CONFIG_OPMODE_ON_STA(pAd)
	{
		FT_RIC_Init(pAd);
	}
#endif /* DOT11R_FT_SUPPORT */
#endif /* CONFIG_STA_SUPPORT */

#ifdef RTMP_MAC
	// TODO: shiang-usw, check this for RMTP_MAC
	if (pAd->chipCap.hif_type == HIF_RTMP) {
#ifdef RTMP_MAC_PCI
		/* Enable Interrupt*/
		RTMP_IRQ_ENABLE(pAd);
#endif /* RTMP_MAC_PCI */

		/* Now Enable RxTx*/
		RTMPEnableRxTx(pAd);
	}
#endif /* RTMP_MAC */

	RTMP_SET_FLAG(pAd, fRTMP_ADAPTER_START_UP);

#ifdef MT76XX_BTCOEX_SUPPORT
		// Init BT Coexistence
	if (IS_MT76XXBTCOMBO(pAd))
		InitBTCoexistence(pAd);
#endif /*MT76XX_BTCOEX_SUPPORT*/

#ifdef MT76x0
	if (IS_MT76x0(pAd))
	{
		/* Select Q2 to receive command response */
		AndesFunSetOP(pAd, Q_SELECT, pAd->chipCap.CmdRspRxRing);

#ifdef CONFIG_AP_SUPPORT
		IF_DEV_CONFIG_OPMODE_ON_AP(pAd)
		{
			AsicDisableSync(pAd);
			MT76x0_Calibration(pAd, pAd->hw_cfg.cent_ch, TRUE, TRUE);
			if (pAd->Dot11_H.RDMode != RD_SILENCE_MODE)
				AsicEnableBssSync(pAd);
		}
#endif /* CONFIG_AP_SUPPORT */
	}
#endif /* MT76x0 */

#if 0
	/* 
	 * debugging helper
	 * 		show the size of main table in Adapter structure
	 *		MacTab  -- 185K
	 *		BATable -- 137K
	 * 		Total 	-- 385K  !!!!! (5/26/2006)
	 */
	printk("sizeof(pAd->MacTab) = %ld\n", sizeof(pAd->MacTab));
	printk("sizeof(pAd->AccessControlList) = %ld\n", sizeof(pAd->AccessControlList));
	printk("sizeof(pAd->ApCfg) = %ld\n", sizeof(pAd->ApCfg));
	printk("sizeof(pAd->BATable) = %ld\n", sizeof(pAd->BATable));	
	BUG();
#endif 

#ifdef CONFIG_STA_SUPPORT
#ifdef PCIE_PS_SUPPORT
	IF_DEV_CONFIG_OPMODE_ON_STA(pAd)
        RTMPInitPCIeLinkCtrlValue(pAd);
#endif /* PCIE_PS_SUPPORT */

#ifdef STA_EASY_CONFIG_SETUP
	IF_DEV_CONFIG_OPMODE_ON_STA(pAd)
	{
		pAd->Mlme.AutoProvisionMachine.CurrState = INFRA_AUTO_PROVISION;
		MlmeEnqueue(pAd, AUTO_PROVISION_STATE_MACHINE, MT2_CONN_AP, 0, NULL, 0);
	    RTMP_MLME_HANDLER(pAd);
	}
#endif /* STA_EASY_CONFIG_SETUP */
#endif /* CONFIG_STA_SUPPORT */

#ifdef CONFIG_AP_SUPPORT
#ifdef BG_FT_SUPPORT
	BG_FTPH_Init();
#endif /* BG_FT_SUPPORT */
#endif /* CONFIG_AP_SUPPORT */

#ifdef HW_COEXISTENCE_SUPPORT
	InitHWCoexistence(pAd);
#endif /* HW_COEXISTENCE_SUPPORT */

//+++Add by shiang for debug
	DBGPRINT(RT_DEBUG_OFF, ("%s(1):Check if PDMA is idle!\n", __FUNCTION__));
	AsicWaitPDMAIdle(pAd, 5, 10);
//---Add by shiang for debug

#ifdef CONFIG_STA_SUPPORT
	/*
		To reduce connection time, 
		do auto reconnect here instead of waiting STAMlmePeriodicExec to do auto reconnect.
	*/
	if (pAd->OpMode == OPMODE_STA)
		MlmeAutoReconnectLastSSID(pAd);
#endif /* CONFIG_STA_SUPPORT */
//+++Add by shiang for debug
	DBGPRINT(RT_DEBUG_OFF, ("%s(2):Check if PDMA is idle!\n", __FUNCTION__));
	AsicWaitPDMAIdle(pAd, 5, 10);
//---Add by shiang for debug
#ifdef HW_COEXISTENCE_SUPPORT
	InitHWCoexistence(pAd);
#endif /* HW_COEXISTENCE_SUPPORT */

#ifdef CONFIG_STA_SUPPORT
#ifdef DOT11W_PMF_SUPPORT
	if (pAd->OpMode == OPMODE_STA)
	{
		pAd->StaCfg.PmfCfg.MFPC = FALSE;
		pAd->StaCfg.PmfCfg.MFPR = FALSE;
		pAd->StaCfg.PmfCfg.PMFSHA256 = FALSE;
		if ((pAd->StaCfg.wdev.AuthMode == Ndis802_11AuthModeWPA2 || pAd->StaCfg.wdev.AuthMode == Ndis802_11AuthModeWPA2PSK)
			&& (pAd->StaCfg.wdev.WepStatus == Ndis802_11AESEnable))
		{
			pAd->StaCfg.PmfCfg.PMFSHA256 = pAd->StaCfg.PmfCfg.Desired_PMFSHA256;
			if (pAd->StaCfg.PmfCfg.Desired_MFPC)
			{
				pAd->StaCfg.PmfCfg.MFPC = TRUE;
				pAd->StaCfg.PmfCfg.MFPR = pAd->StaCfg.PmfCfg.Desired_MFPR;

				if (pAd->StaCfg.PmfCfg.MFPR)
					pAd->StaCfg.PmfCfg.PMFSHA256 = TRUE;
			}
		} else if (pAd->StaCfg.PmfCfg.Desired_MFPC) {
			DBGPRINT(RT_DEBUG_ERROR, ("[PMF]%s:: Security is not WPA2/WPA2PSK AES\n", __FUNCTION__));
		}

		DBGPRINT(RT_DEBUG_ERROR, ("[PMF]%s:: MFPC=%d, MFPR=%d, SHA256=%d\n",
					__FUNCTION__, pAd->StaCfg.PmfCfg.MFPC, pAd->StaCfg.PmfCfg.MFPR,
					pAd->StaCfg.PmfCfg.PMFSHA256));
	}
#endif /* DOT11W_PMF_SUPPORT */
#endif /* CONFIG_STA_SUPPORT */

#ifdef WSC_INCLUDED
#ifdef CONFIG_AP_SUPPORT
	if ((pAd->OpMode == OPMODE_AP)
#ifdef P2P_SUPPORT
		/* P2P will use ApCfg.MBSSID and ApCfg.ApCliTab also. */
		|| TRUE
#endif /* P2P_SUPPORT */
		)
	{
		INT index;
		for (index = 0; index < pAd->ApCfg.BssidNum; index++)
		{
#ifdef HOSTAPD_SUPPORT
			if (pAd->ApCfg.MBSSID[index].Hostapd == Hostapd_EXT)
			{
				DBGPRINT(RT_DEBUG_TRACE, ("WPS is control by hostapd now.\n"));
			}
			else
#endif /*HOSTAPD_SUPPORT*/
			{
				PWSC_CTRL pWscControl;
				UCHAR zeros16[16]= {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
				
				pWscControl = &pAd->ApCfg.MBSSID[index].WscControl;
				DBGPRINT(RT_DEBUG_TRACE, ("Generate UUID for apidx(%d)\n", index));
				if (NdisEqualMemory(&pWscControl->Wsc_Uuid_E[0], zeros16, UUID_LEN_HEX))
					WscGenerateUUID(pAd, &pWscControl->Wsc_Uuid_E[0], &pWscControl->Wsc_Uuid_Str[0], index, FALSE);
				WscInit(pAd, FALSE, index);
			}
		}

#ifdef APCLI_SUPPORT
		for(index = 0; index < MAX_APCLI_NUM; index++)
		{
			PWSC_CTRL pWpsCtrl = &pAd->ApCfg.ApCliTab[index].WscControl;
			
			pWpsCtrl->pAd = pAd;        
			NdisZeroMemory(pWpsCtrl->EntryAddr, MAC_ADDR_LEN);
#ifdef WSC_V2_SUPPORT
			pWpsCtrl->WscConfigMethods= 0x238C;
#else			
			pWpsCtrl->WscConfigMethods= 0x018C;
#endif /*WSC_V2_SUPPORT*/
			RTMP_AP_IoctlHandle(pAd, NULL, CMD_RTPRIV_IOCTL_WSC_INIT, 0, (VOID *)&pAd->ApCfg.ApCliTab[index], index);
		}
#endif /* APCLI_SUPPORT */
	}
#endif /* CONFIG_AP_SUPPORT */

#ifdef CONFIG_STA_SUPPORT
	IF_DEV_CONFIG_OPMODE_ON_STA(pAd)
	{
		PWSC_CTRL pWscControl = &pAd->StaCfg.WscControl;
		
		WscGenerateUUID(pAd, &pWscControl->Wsc_Uuid_E[0], &pWscControl->Wsc_Uuid_Str[0], 0, FALSE);
		WscInit(pAd, FALSE, BSS0);
#ifdef WSC_V2_SUPPORT
		WscInitRegistrarPair(pAd, &pAd->StaCfg.WscControl, BSS0);
#endif /* WSC_V2_SUPPORT */
#if ((defined(WOW_SUPPORT) && defined(RTMP_MAC_USB)) || defined(NEW_WOW_SUPPORT)) && defined(WOW_IFDOWN_SUPPORT)
	if (pAd->WOW_Cfg.bEnable == TRUE)
		ASIC_WOW_DISABLE(pAd);
#endif /* ((defined(WOW_SUPPORT) && defined(RTMP_MAC_USB)) || defined(NEW_WOW_SUPPORT)) && defined(WOW_IFDOWN_SUPPORT) */

	}
#endif /* CONFIG_STA_SUPPORT */

	/* WSC hardware push button function 0811 */
	WSC_HDR_BTN_Init(pAd);
#endif /* WSC_INCLUDED */
#ifdef MT76XX_BTCOEX_SUPPORT
	//SendAndesWLANStatus(pAd,WLAN_Device_ON,0);
	if (IS_MT76XXBTCOMBO(pAd))
		MLMEHook(pAd, WLAN_Device_ON, 0);
#endif /*MT76XX_BTCOEX_SUPPORT*/

#ifdef CONFIG_AP_SUPPORT
#ifdef MULTI_CLIENT_SUPPORT
	pAd->CommonCfg.txRetryCfg = 0;

	{
		UINT32	TxRtyCfg;

		RTMP_IO_READ32(pAd, TX_RTY_CFG, &TxRtyCfg);
		pAd->CommonCfg.txRetryCfg = TxRtyCfg;
	}
#endif /* MULTI_CLIENT_SUPPORT */
#endif /* CONFIG_AP_SUPPORT */

#if defined(RT_CFG80211_P2P_SUPPORT) && defined(SUPPORT_ACS_ALL_CHANNEL_RANK)
    if (pAd->ApCfg.bAutoChannelAtBootup && pAd->ApCfg.bAutoChannelScaned == 0)
    {
#ifdef RTMP_MAC_PCI
        /* Enable Interrupt first due to we need to scan channel to receive beacons.*/
        RTMP_IRQ_ENABLE(pAd);
#endif /* RTMP_MAC_PCI */
#ifdef RTMP_MAC_USB
        RTMP_CLEAR_FLAG(pAd, fRTMP_ADAPTER_RESET_IN_PROGRESS);
        RTMP_CLEAR_FLAG(pAd, fRTMP_ADAPTER_REMOVE_IN_PROGRESS);


        /* Support multiple BulkIn IRP,*/
        /* the value on pAd->CommonCfg.NumOfBulkInIRP may be large than 1.*/
        {
            UCHAR index;
#ifdef MULTI_URB				
            for(index=0; index<SUBMIT_URB_NUM; index++)
            {                   
                DoBulkIn(pAd);
                DBGPRINT(RT_DEBUG_TRACE, ("DoBulkIn!\n" ));
            }
#else /* !MULTI_URB */
            for(index=0; index<pAd->CommonCfg.NumOfBulkInIRP; index++)
            {
                RTUSBBulkReceive(pAd);
                DBGPRINT(RT_DEBUG_TRACE, ("RTUSBBulkReceive!\n" ));
            }
#endif /* MULTI_URB */
        }
#endif /* RTMP_MAC_USB */
        /* Now Enable RxTx*/
        RTMPEnableRxTx(pAd);
        //RTMP_SET_FLAG(pAd, fRTMP_ADAPTER_START_UP);
        
#ifdef MT762x
        // TODO: shiang-usw, check why MT76x2 don't need to set this flag here!
        if (!IS_MT762x(pAd))
#endif /* MT762x */
        RTMP_SET_FLAG(pAd, fRTMP_ADAPTER_START_UP);
        /* Let BBP register at 20MHz to do scan */
        bbp_set_bw(pAd, BW_20);
        DBGPRINT(RT_DEBUG_ERROR, ("SYNC - BBP R4 to 20MHz.l\n"));
        AP_AUTO_CH_SEL(pAd, pAd->ApCfg.AutoChannelAlg);
        pAd->ApCfg.bAutoChannelScaned = 1;
    }
#endif /* SUPPORT_ACS_ALL_CHANNEL_RANK */

#ifdef CONFIG_MULTI_CHANNEL
{
#if 1 //yiwei force turn on andes debug 
	UINT32 tmp;
	//iwpriv ra0 mac 730 = 13
	RTMP_IO_WRITE32(pAd, 0x730, 13);
	RTMP_IO_READ32(pAd, 0x730, &tmp);
	DBGPRINT(RT_DEBUG_ERROR, ("$$$$$ mac 730=%u\n",tmp));
#endif
}
#endif /* CONFIG_MULTI_CHANNEL */

#ifdef ED_MONITOR
{
	BOOLEAN bEdcca = FALSE;

	bEdcca = GetEDCCASupport(pAd);

	if (bEdcca)
	{
		ed_monitor_init(pAd);
	}
}
#endif /* ED_MONITOR */

}


VOID RTMPDrvClose(VOID *pAdSrc, VOID *net_dev)
{
	RTMP_ADAPTER *pAd = (RTMP_ADAPTER *)pAdSrc;
	UINT32 i = 0;
	UINT32 MacReg1, MacReg2;

#ifdef LED_CONTROL_SUPPORT
	RTMPExitLEDMode(pAd);
#endif // LED_CONTROL_SUPPORT

#ifdef RT_CFG80211_SUPPORT
#ifdef CONFIG_AP_SUPPORT
		if (pAd->cfg80211_ctrl.isCfgInApMode == RT_CMD_80211_IFTYPE_AP && RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_START_UP))
		{
			CFG80211DRV_DisableApInterface(pAd);
			pAd->cfg80211_ctrl.isCfgInApMode = RT_CMD_80211_IFTYPE_STATION;			
		}
#endif /* CONFIG_AP_SUPPORT */	
#endif/*RT_CFG80211_SUPPORT*/

#ifdef BB_SOC
	 if (RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_RADIO_OFF))
	 {
	 	DBGPRINT(RT_DEBUG_TRACE, ("Radio_ON first....\n"));
    		MlmeRadioOn(pAd);
	 }
#endif /* BB_SOC */

#ifdef CONFIG_STA_SUPPORT
#ifdef CREDENTIAL_STORE
		if (pAd->IndicateMediaState == NdisMediaStateConnected)
			StoreConnectInfo(pAd);
		else
		{
			RTMP_SEM_LOCK(&pAd->StaCtIf.Lock);
			pAd->StaCtIf.Changeable = FALSE;
			RTMP_SEM_UNLOCK(&pAd->StaCtIf.Lock);
		}
#endif /* CREDENTIAL_STORE */
#endif /* CONFIG_STA_SUPPORT */

#ifdef CONFIG_AP_SUPPORT
#ifdef BG_FT_SUPPORT
	BG_FTPH_Remove();
#endif /* BG_FT_SUPPORT */
#endif /* CONFIG_AP_SUPPORT */

#ifdef RTMP_RBUS_SUPPORT
#ifdef RT3XXX_ANTENNA_DIVERSITY_SUPPORT
	if (pAd->infType == RTMP_DEV_INF_RBUS)
	RT3XXX_AntDiversity_Fini(pAd);
#endif /* RT3XXX_ANTENNA_DIVERSITY_SUPPORT */
#endif /* RTMP_RBUS_SUPPORT */

#ifdef CONFIG_STA_SUPPORT
	IF_DEV_CONFIG_OPMODE_ON_STA(pAd)
	{
#ifdef PCIE_PS_SUPPORT
		RTMPPCIeLinkCtrlValueRestore(pAd, RESTORE_CLOSE);
#endif /* PCIE_PS_SUPPORT */

		/* If dirver doesn't wake up firmware here,*/
		/* NICLoadFirmware will hang forever when interface is up again.*/
		if (OPSTATUS_TEST_FLAG(pAd, fOP_STATUS_DOZE))
        {      
		    AsicForceWakeup(pAd, TRUE);
        }

#ifdef RTMP_MAC_USB
		RTMP_SET_FLAG(pAd, fRTMP_ADAPTER_REMOVE_IN_PROGRESS);
#endif /* RTMP_MAC_USB */

#ifdef RTMP_MAC_PCI
		pAd->bPCIclkOff = FALSE;    
#endif /* RTMP_MAC_PCI */
	}
#endif /* CONFIG_STA_SUPPORT */

#if ((defined(WOW_SUPPORT) && defined(RTMP_MAC_USB)) || defined(NEW_WOW_SUPPORT)) && defined(WOW_IFDOWN_SUPPORT)
	if (pAd->WOW_Cfg.bEnable == FALSE)
#endif /* ((defined(WOW_SUPPORT) && defined(RTMP_MAC_USB)) || defined(NEW_WOW_SUPPORT)) && defined(WOW_IFDOWN_SUPPORT) */
	{
		RTMP_CLEAR_FLAG(pAd, fRTMP_ADAPTER_MCU_SEND_IN_BAND_CMD);
		RTMP_SET_FLAG(pAd, fRTMP_ADAPTER_HALT_IN_PROGRESS);
	}

#ifdef EXT_BUILD_CHANNEL_LIST
	if (pAd->CommonCfg.pChDesp != NULL)
		os_free_mem(NULL, pAd->CommonCfg.pChDesp);
	pAd->CommonCfg.pChDesp = NULL;
	pAd->CommonCfg.DfsType = MAX_RD_REGION;
	pAd->CommonCfg.bCountryFlag = 0;
#endif /* EXT_BUILD_CHANNEL_LIST */
	pAd->CommonCfg.bCountryFlag = FALSE;

#ifdef WMM_ACM_SUPPORT
	/* must call first */
	ACMP_Release(pAd);
#endif /* WMM_ACM_SUPPORT */

#ifdef MESH_SUPPORT
	/* close all mesh link before the interface go down.*/
	if (MESH_ON(pAd))
		MeshDown(pAd, TRUE);
#endif /* MESH_SUPPORT */

#ifdef WDS_SUPPORT
	WdsDown(pAd);
#endif /* WDS_SUPPORT */

	for (i = 0 ; i < NUM_OF_TX_RING; i++)
	{
		while (pAd->DeQueueRunning[i] == TRUE)
		{
			DBGPRINT(RT_DEBUG_TRACE, ("Waiting for TxQueue[%d] done..........\n", i));
			RtmpusecDelay(1000);
		}
	}
	
#ifdef CONFIG_AP_SUPPORT
	IF_DEV_CONFIG_OPMODE_ON_AP(pAd)
	{
		BOOLEAN Cancelled = FALSE;
#ifdef RTMP_MAC_USB
		RTMPCancelTimer(&pAd->CommonCfg.BeaconUpdateTimer, &Cancelled);
#endif /* RTMP_MAC_USB */

#ifdef DOT11N_DRAFT3
		if (pAd->CommonCfg.Bss2040CoexistFlag & BSS_2040_COEXIST_TIMER_FIRED)
		{
			RTMPCancelTimer(&pAd->CommonCfg.Bss2040CoexistTimer, &Cancelled);
			pAd->CommonCfg.Bss2040CoexistFlag  = 0;
		}
#endif /* DOT11N_DRAFT3 */

		/* PeriodicTimer already been canceled by MlmeHalt() API.*/
		/*RTMPCancelTimer(&pAd->PeriodicTimer,	&Cancelled);*/
	}
#endif /* CONFIG_AP_SUPPORT */

	/* Stop Mlme state machine*/
	MlmeHalt(pAd);
	
	/* Close net tasklets*/
	RtmpNetTaskExit(pAd);


#ifdef CONFIG_STA_SUPPORT
	IF_DEV_CONFIG_OPMODE_ON_STA(pAd)
	{
#if ((defined(WOW_SUPPORT) && defined(RTMP_MAC_USB)) || defined(NEW_WOW_SUPPORT)) && defined(WOW_IFDOWN_SUPPORT)
		if (pAd->WOW_Cfg.bEnable == FALSE)
#endif /* ((defined(WOW_SUPPORT) && defined(RTMP_MAC_USB))) && defined(WOW_IFDOWN_SUPPORT) */
		MacTableReset(pAd);
#ifdef MAT_SUPPORT
		MATEngineExit(pAd);
#endif /* MAT_SUPPORT */
#if ((defined(WOW_SUPPORT) && defined(RTMP_MAC_USB)) || defined(NEW_WOW_SUPPORT)) && defined(WOW_IFDOWN_SUPPORT)
		if (pAd->WOW_Cfg.bEnable == TRUE)
		{
			RTMP_IO_READ32(pAd, 0x80, &MacReg1);
			RTMP_IO_READ32(pAd, BCN_TIME_CFG, &MacReg2);
			DBGPRINT(RT_DEBUG_TRACE, ("asic wow enable, 0x80 (0x%x), BCN_TIME_CFG (0x%x)\n", MacReg1, MacReg2));
			ASIC_WOW_ENABLE(pAd);
		}
		else
#endif /* ((defined(WOW_SUPPORT) && defined(RTMP_MAC_USB)) || defined(NEW_WOW_SUPPORT)) && defined(WOW_IFDOWN_SUPPORT) */
			MlmeRadioOff(pAd);
	}
#endif /* CONFIG_STA_SUPPORT */

#ifdef CONFIG_AP_SUPPORT
	IF_DEV_CONFIG_OPMODE_ON_AP(pAd)
	{
#ifdef MAT_SUPPORT
		MATEngineExit(pAd);
#endif /* MAT_SUPPORT */

#ifdef CLIENT_WDS
		CliWds_ProxyTabDestory(pAd);
#endif /* CLIENT_WDS */
		/* Shutdown Access Point function, release all related resources */
		APShutdown(pAd);

/*#ifdef AUTO_CH_SELECT_ENHANCE*/
		/* Free BssTab & ChannelInfo tabbles.*/
/*		AutoChBssTableDestroy(pAd); */
/*		ChannelInfoDestroy(pAd); */
/*#endif  AUTO_CH_SELECT_ENHANCE */
	}
#endif /* CONFIG_AP_SUPPORT */

	MeasureReqTabExit(pAd);
	TpcReqTabExit(pAd);

#if 0
#ifdef LED_CONTROL_SUPPORT
	RTMPExitLEDMode(pAd);
#endif // LED_CONTROL_SUPPORT
#endif /* 0 */

#if 0 /* Move to WscThreadExit */
#ifdef WSC_INCLUDED
#ifdef CONFIG_AP_SUPPORT
	IF_DEV_CONFIG_OPMODE_ON_AP(pAd)
	{
		INT ap_idx;
		for (ap_idx = 0; ap_idx < pAd->ApCfg.BssidNum; ap_idx++)
			WscStop(pAd, FALSE, &pAd->ApCfg.MBSSID[ap_idx].WscControl);
#ifdef APCLI_SUPPORT
		WscStop(pAd, TRUE, &pAd->ApCfg.ApCliTab[BSS0].WscControl);
#endif /* APCLI_SUPPORT */
	}
#endif /* CONFIG_AP_SUPPORT */
#ifdef CONFIG_STA_SUPPORT
	IF_DEV_CONFIG_OPMODE_ON_STA(pAd)
		WscStop(pAd,
#ifdef CONFIG_AP_SUPPORT
				FALSE,
#endif /* CONFIG_AP_SUPPORT */
				&pAd->StaCfg.WscControl);
#endif /* CONFIG_STA_SUPPORT */

#ifdef OLD_DH_KEY
#ifdef CONFIG_AP_SUPPORT
	IF_DEV_CONFIG_OPMODE_ON_AP(pAd)
	    	WSC_VFREE_KEY_MEM(pAd->ApCfg.MBSSID[0].WscControl.pPubKeyMem, pAd->ApCfg.MBSSID[0].WscControl.pSecKeyMem);
#endif /* CONFIG_AP_SUPPORT */
#ifdef CONFIG_STA_SUPPORT
	IF_DEV_CONFIG_OPMODE_ON_STA(pAd)
		WSC_VFREE_KEY_MEM(pAd->StaCfg.WscControl.pPubKeyMem, pAd->StaCfg.WscControl.pSecKeyMem);
#endif /* CONFIG_STA_SUPPORT */
#endif /* OLD_DH_KEY */

	/* WSC hardware push button function 0811 */
	WSC_HDR_BTN_Stop(pAd);
#endif /* WSC_INCLUDED */
#endif

	/* Close kernel threads*/
	RtmpMgmtTaskExit(pAd);
	
#ifdef CONFIG_AP_SUPPORT
	IF_DEV_CONFIG_OPMODE_ON_AP(pAd)
	{
		/* must after RtmpMgmtTaskExit(); Or pAd->pChannelInfo will be NULL */
		/* Free BssTab & ChannelInfo tabbles.*/
		AutoChBssTableDestroy(pAd);
		ChannelInfoDestroy(pAd);
	}
#endif /* CONFIG_AP_SUPPORT */

#ifdef RTMP_MAC_PCI
	{
#if defined(MT76x0) || defined(MT76x2)
		if (IS_MT76x0(pAd) || IS_MT76x2(pAd)) {
			DISABLE_TX_RX(pAd, RTMP_HALT);
		}
		else
#endif /* MT76x0 */
		{
			if (RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_INTERRUPT_ACTIVE))
			{
				DISABLE_TX_RX(pAd, RTMP_HALT);
				RTMP_ASIC_INTERRUPT_DISABLE(pAd);
			}
		}

		/* Receive packets to clear DMA index after disable interrupt. */
		/*RTMPHandleRxDoneInterrupt(pAd);*/
		/* put to radio off to save power when driver unload.  After radiooff, can't write /read register.  So need to finish all */
		/* register access before Radio off.*/

#ifdef RTMP_PCI_SUPPORT
		if (pAd->infType == RTMP_DEV_INF_PCI || pAd->infType == RTMP_DEV_INF_PCIE)
		{
			BOOLEAN brc = TRUE;

#if defined(MT76x0) || defined(MT76x2)
			// TODO: shiang, how about RadioOff for 65xx??
			if (!(IS_MT76x0(pAd) || IS_MT76x2(pAd)))
#endif /* defined(MT76x0) || defined(MT76x2) */
			brc = RT28xxPciAsicRadioOff(pAd, RTMP_HALT, 0);

/*In  solution 3 of 3090F, the bPCIclkOff will be set to TRUE after calling RT28xxPciAsicRadioOff*/
#ifdef PCIE_PS_SUPPORT
			pAd->bPCIclkOff = FALSE;
#endif /* PCIE_PS_SUPPORT */

			if (brc==FALSE)
			{
				DBGPRINT(RT_DEBUG_ERROR,("%s call RT28xxPciAsicRadioOff fail !!\n", __FUNCTION__)); 
			}
		}
#endif /* RTMP_PCI_SUPPORT */
	}

#if 0
	if (RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_INTERRUPT_ACTIVE))
	{
		RTMP_ASIC_INTERRUPT_DISABLE(pAd);
	}

	/* Disable Rx, register value supposed will remain after reset*/
	NICIssueReset(pAd);
#endif
#endif /* RTMP_MAC_PCI */

	/* Free IRQ*/
	if (RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_INTERRUPT_IN_USE))
	{
#ifdef RTMP_MAC_PCI
		/* Deregister interrupt function*/
		RTMP_OS_IRQ_RELEASE(pAd, net_dev);
#endif /* RTMP_MAC_PCI */
		RTMP_CLEAR_FLAG(pAd, fRTMP_ADAPTER_INTERRUPT_IN_USE);
	}

#ifdef SINGLE_SKU_V2
	{
		CH_POWER *ch, *ch_temp;
		DlListForEachSafe(ch, ch_temp, &pAd->SingleSkuPwrList, CH_POWER, List)
		{
			DlListDel(&ch->List);
#ifdef MT7601
			if (IS_MT7601(pAd)) {
				os_free_mem(NULL, ch->Channel);
			}
#endif /* MT7601 */
			os_free_mem(NULL, ch);
		}
	}
#endif /* SINGLE_SKU_V2 */

	/* Free Ring or USB buffers*/
#ifdef RESOURCE_PRE_ALLOC
	RTMPResetTxRxRingMemory(pAd);
#else
	/* Free Ring or USB buffers*/
	RTMPFreeTxRxRingMemory(pAd);
#endif /* RESOURCE_PRE_ALLOC */

	RTMP_CLEAR_FLAG(pAd, fRTMP_ADAPTER_HALT_IN_PROGRESS);

#ifdef WLAN_SKB_RECYCLE
	skb_queue_purge(&pAd->rx0_recycle);
#endif /* WLAN_SKB_RECYCLE */

#ifdef DOT11_N_SUPPORT
	/* Free BA reorder resource*/
	ba_reordering_resource_release(pAd);
#endif /* DOT11_N_SUPPORT */

	UserCfgExit(pAd); /* must after ba_reordering_resource_release */

#ifdef CONFIG_STA_SUPPORT
#ifdef DOT11R_FT_SUPPORT
	IF_DEV_CONFIG_OPMODE_ON_STA(pAd)
	{
		FT_RIC_Release(pAd);
	}
#endif /* DOT11R_FT_SUPPORT */
#endif /* CONFIG_STA_SUPPORT */

	RTMP_CLEAR_FLAG(pAd, fRTMP_ADAPTER_START_UP);

/*+++Modify by woody to solve the bulk fail+++*/
#ifdef CONFIG_STA_SUPPORT
	IF_DEV_CONFIG_OPMODE_ON_STA(pAd)
	{
#ifdef RT35xx
		if (IS_RT3572(pAd))
		{
			RT30xxWriteRFRegister(pAd, RF_R08, 0x00);
			AsicSendCommandToMcu(pAd, 0x30, 0xff, 0xff, 0x02, FALSE);
		}
#endif /* RT35xx */
#ifdef DOT11Z_TDLS_SUPPORT
		TDLS_Table_Destory(pAd);
#ifdef TDLS_AUTOLINK_SUPPORT
		TDLS_ClearEntryList(&pAd->StaCfg.TdlsInfo.TdlsDiscovPeerList);
		NdisFreeSpinLock(&pAd->StaCfg.TdlsInfo.TdlsDiscovPeerListSemLock);
		TDLS_ClearEntryList(&pAd->StaCfg.TdlsInfo.TdlsBlackList);
		NdisFreeSpinLock(&pAd->StaCfg.TdlsInfo.TdlsBlackListSemLock);
#endif /* TDLS_AUTOLINK_SUPPORT */
#endif /* DOT11Z_TDLS_SUPPORT */
	}
#endif /* CONFIG_STA_SUPPORT */

	/* clear MAC table */
	/* TODO: do not clear spin lock, such as fLastChangeAccordingMfbLock */
	NdisZeroMemory(&pAd->MacTab, sizeof(MAC_TABLE));

	RTMP_IO_READ32(pAd, 0x80, &MacReg1);
	RTMP_IO_READ32(pAd, 0x1114, &MacReg2);
	DBGPRINT(RT_DEBUG_TRACE, ("close 0x80 (0x%x), BCN_TIME_CFG (0x%x)\n", MacReg1, MacReg2));

	/* release all timers */
	RtmpusecDelay(2000);
	RTMP_AllTimerListRelease(pAd);

#ifdef ED_MONITOR
	if (pAd->ed_chk)
	{
		DBGPRINT(RT_DEBUG_ERROR, ("@@@ %s: go to ed_monitor_exit()!!\n", __FUNCTION__));	
		ed_monitor_exit(pAd);
	}
#endif /* ED_MONITOR */

#ifdef RTMP_TIMER_TASK_SUPPORT
	NdisFreeSpinLock(&pAd->TimerQLock);
#endif /* RTMP_TIMER_TASK_SUPPORT */

#ifdef CONFIG_FPGA_MODE
#ifdef CAPTURE_MODE
	cap_mode_deinit(pAd);
#endif /* CAPTURE_MODE */
#endif /* CONFIG_FPGA_MODE */

}


VOID RTMPInfClose(VOID *pAdSrc)
{
	RTMP_ADAPTER *pAd = (RTMP_ADAPTER *)pAdSrc;

#ifdef CONFIG_AP_SUPPORT
	pAd->ApCfg.MBSSID[MAIN_MBSSID].bBcnSntReq = FALSE;

	IF_DEV_CONFIG_OPMODE_ON_AP(pAd)
	{
		/* kick out all STAs behind the bss.*/
		MbssKickOutStas(pAd, MAIN_MBSSID, REASON_DISASSOC_INACTIVE);
	}

	APMakeAllBssBeacon(pAd);
	APUpdateAllBeaconFrame(pAd);
#endif /* CONFIG_AP_SUPPORT */

#ifdef MESH_SUPPORT
	MeshMakeBeacon(pAd, MESH_BEACON_IDX(pAd));
	MeshUpdateBeaconFrame(pAd, MESH_BEACON_IDX(pAd));
#endif /* MESH_SUPPORT */


#ifdef CONFIG_STA_SUPPORT
	IF_DEV_CONFIG_OPMODE_ON_STA(pAd)
	{
#ifdef PROFILE_STORE
		WriteDatThread(pAd);
		RtmpusecDelay(1000);
#endif /* PROFILE_STORE */
#ifdef QOS_DLS_SUPPORT
		/* send DLS-TEAR_DOWN message, */
		if (pAd->CommonCfg.bDLSCapable)
		{
			UCHAR i;

			/* tear down local dls table entry*/
			for (i=0; i<MAX_NUM_OF_INIT_DLS_ENTRY; i++)
			{
				if (pAd->StaCfg.DLSEntry[i].Valid && (pAd->StaCfg.DLSEntry[i].Status == DLS_FINISH))
				{
					RTMPSendDLSTearDownFrame(pAd, pAd->StaCfg.DLSEntry[i].MacAddr);
					pAd->StaCfg.DLSEntry[i].Status	= DLS_NONE;
					pAd->StaCfg.DLSEntry[i].Valid	= FALSE;
				}
			}

			/* tear down peer dls table entry*/
			for (i=MAX_NUM_OF_INIT_DLS_ENTRY; i<MAX_NUM_OF_DLS_ENTRY; i++)
			{
				if (pAd->StaCfg.DLSEntry[i].Valid && (pAd->StaCfg.DLSEntry[i].Status == DLS_FINISH))
				{
					RTMPSendDLSTearDownFrame(pAd, pAd->StaCfg.DLSEntry[i].MacAddr);
					pAd->StaCfg.DLSEntry[i].Status = DLS_NONE;
					pAd->StaCfg.DLSEntry[i].Valid	= FALSE;
				}
			}
			RTMP_MLME_HANDLER(pAd);
		}
#endif /* QOS_DLS_SUPPORT */

		if (INFRA_ON(pAd) &&
#if ((defined(WOW_SUPPORT) && defined(RTMP_MAC_USB)) || defined(NEW_WOW_SUPPORT)) && defined(WOW_IFDOWN_SUPPORT)
	/* In WOW state, can't issue disassociation reqeust */
			pAd->WOW_Cfg.bEnable == FALSE &&
#endif /* ((defined(WOW_SUPPORT) && defined(RTMP_MAC_USB)) || defined(NEW_WOW_SUPPORT)) && defined(WOW_IFDOWN_SUPPORT) */
			(!RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_NIC_NOT_EXIST)))
		{
			MLME_DISASSOC_REQ_STRUCT	DisReq;
			MLME_QUEUE_ELEM *MsgElem;
    
			os_alloc_mem(NULL, (UCHAR **)&MsgElem, sizeof(MLME_QUEUE_ELEM));
			if (MsgElem)
			{
			COPY_MAC_ADDR(DisReq.Addr, pAd->CommonCfg.Bssid);
			DisReq.Reason =  REASON_DEAUTH_STA_LEAVING;

			MsgElem->Machine = ASSOC_STATE_MACHINE;
			MsgElem->MsgType = MT2_MLME_DISASSOC_REQ;
			MsgElem->MsgLen = sizeof(MLME_DISASSOC_REQ_STRUCT);
			NdisMoveMemory(MsgElem->Msg, &DisReq, sizeof(MLME_DISASSOC_REQ_STRUCT));

			/* Prevent to connect AP again in STAMlmePeriodicExec*/
			pAd->MlmeAux.AutoReconnectSsidLen= 32;
			NdisZeroMemory(pAd->MlmeAux.AutoReconnectSsid, pAd->MlmeAux.AutoReconnectSsidLen);

			pAd->Mlme.CntlMachine.CurrState = CNTL_WAIT_OID_DISASSOC;
			MlmeDisassocReqAction(pAd, MsgElem);
			os_free_mem(NULL, MsgElem);
			}
			
			RtmpusecDelay(1000);
		}

#ifdef WPA_SUPPLICANT_SUPPORT
#ifndef NATIVE_WPA_SUPPLICANT_SUPPORT
		/* send wireless event to wpa_supplicant for infroming interface down.*/
		RtmpOSWrielessEventSend(pAd->net_dev, RT_WLAN_EVENT_CUSTOM, RT_INTERFACE_DOWN, NULL, NULL, 0);
#endif /* NATIVE_WPA_SUPPLICANT_SUPPORT */

		if (pAd->StaCfg.wpa_supplicant_info.pWpsProbeReqIe)
		{
			os_free_mem(NULL, pAd->StaCfg.wpa_supplicant_info.pWpsProbeReqIe);
			pAd->StaCfg.wpa_supplicant_info.pWpsProbeReqIe = NULL;
			pAd->StaCfg.wpa_supplicant_info.WpsProbeReqIeLen = 0;
		}

		if (pAd->StaCfg.wpa_supplicant_info.pWpaAssocIe)
		{
			os_free_mem(NULL, pAd->StaCfg.wpa_supplicant_info.pWpaAssocIe);
			pAd->StaCfg.wpa_supplicant_info.pWpaAssocIe = NULL;
			pAd->StaCfg.wpa_supplicant_info.WpaAssocIeLen = 0;
		}
#endif /* WPA_SUPPLICANT_SUPPORT */


	}
#endif /* CONFIG_STA_SUPPORT */
}


#ifdef HW_COEXISTENCE_SUPPORT
VOID InitHWCoexistence(
	IN PRTMP_ADAPTER pAd)
{
	UINT32 GPIO = 0;
	US_CYC_CNT_STRUC USCycCnt;
#ifdef RT5390
	UCHAR BBPR3=0;
	ULONG Value = 0;
#endif /* RT5390 */
	
	if (pAd == NULL)
	{
		return;
	}

	DBGPRINT(RT_DEBUG_TRACE,("In InitHWCoexistence ...\n"));	
#ifdef RTMP_PCI_SUPPORT
#ifdef RT5390
	if (IS_RT5390BC8(pAd))
	{
		UCHAR BbpReg = 0;
		
		RTMP_BBP_IO_READ8_BY_REG_ID(pAd, BBP_R152, &BbpReg);
		BbpReg = ((BbpReg & ~0x80) | (0x80)); /* Main antenna */
		RTMP_BBP_IO_WRITE8_BY_REG_ID(pAd, BBP_R152, BbpReg);
	
		DBGPRINT(RT_DEBUG_TRACE, ("%s: 5390BC8: Select the main antenna\n", __FUNCTION__));
	}

	if (IS_RT5390BC8(pAd))
	{
		RTMP_BBP_IO_WRITE8_BY_REG_ID(pAd, BBP_R75, 0x52); /* Rx high power VGA offset for LNA offset */
		RTMP_BBP_IO_WRITE8_BY_REG_ID(pAd, BBP_R76, 0x34); /* Rx medium power VGA offset for LNA offset */
	}
#endif /* RT5390 */
#endif /* RTMP_PCI_SUPPORT */
	
#ifdef RELEASE_EXCLUDE
	/*
		Enable BT-Coexistence.
		Update GPIO for Bluetooth
		GPIO#0: Bluetooth priority
		GPIO#1: Wireless active
		GPIO#2: Bluetooth active
	*/
#endif /* RELEASE_EXCLUDE */
	if (
#ifdef BT_COEXISTENCE_SUPPORT
		((pAd->bMiscOn == TRUE) && 
		(pAd->NicConfig2.field.CoexBit == TRUE)) || 
#endif /* BT_COEXISTENCE_SUPPORT */
		(pAd->bWiMaxCoexistenceOn == TRUE))
	{
		DBGPRINT(RT_DEBUG_TRACE,("Coexistence Initialize Hardware\n"));
		
		RTMP_IO_READ32(pAd, US_CYC_CNT, (UINT32 *)&USCycCnt.word);
#ifdef BT_COEXISTENCE_SUPPORT
		if (IS_ENABLE_WIFI_ACTIVE_PULL_LOW_BY_FORCE(pAd))
#ifdef RELEASE_EXCLUDE
			/* Wireless active disable, gpio1 always pull low */
#endif /* RELEASE_EXCLUDE */
			USCycCnt.field.MiscModeEn = 0;
		else
#endif /* BT_COEXISTENCE_SUPPORT */
#ifdef RELEASE_EXCLUDE
		/* Wireless active enable, gpio1 pull high when Tx */
#endif /* RELEASE_EXCLUDE */
		USCycCnt.field.MiscModeEn = 1;
		RTMP_IO_WRITE32(pAd, US_CYC_CNT, USCycCnt.word);

		/* Set bit 0 & 1 to zero to use GPIO 0 & 1 */
#ifdef RT5390
		if (IS_RT5390BC8(pAd))
		{
			/* RT5390BC8 needs to set bit [15:8] to 0 at MAC[10D4] */
			RTMP_IO_READ32(pAd, 0x10d4, &Value);
			if ((Value & 0xff00) != 0)
			{
				Value &= (~0xff00);
				RTMP_IO_WRITE32(pAd, 0x10d4, Value);
			}
		}
#endif /* RT5390 */

		RTMP_IO_READ32(pAd, GPIO_SWITCH, &GPIO);
		GPIO = (GPIO & 0xfffffffc);	
		RTMP_IO_WRITE32(pAd, GPIO_SWITCH, GPIO);
#if defined(RT5390) || defined(RT3572)
			
		/* Default switch to direct mode. */ 
		
	 	if ((IS_RT5390(pAd) || IS_RT3572(pAd)))
		{
	 		RTMP_IO_READ32(pAd, GPIO_CTRL_CFG, &Value);
			Value &= ~(0x0808);
			Value |= 0x08;
			RTMP_IO_WRITE32(pAd, GPIO_CTRL_CFG, Value);

			if (IS_RT5390(pAd))
			{
				/*
				   RT5390 Use GPIO6 and GPIO3 to control antenna diversity
				   Also make sure GPIO_SWITCH(Function) MAC 0x05DC Bit[6] been enabled.
				   Here we use GPIO6 instead of EESK.
				*/
				Value &= ~(0x4040);
			}
			else
			{
				RTMP_IO_READ32(pAd, E2PROM_CSR, &Value);
				Value &= ~(EESK);
				RTMP_IO_WRITE32(pAd, E2PROM_CSR, Value);
			}
		}		
#endif /* defined(RT5390) || defined(RT5372) */

#ifdef BT_COEXISTENCE_SUPPORT
		if (IS_ENABLE_WIFI_ACTIVE_PULL_LOW_BY_FORCE(pAd))
		{
			ULONG Value = 0;
			
			RTMP_IO_READ32( pAd, GPIO_CTRL_CFG, &Value);
			Value &= ~(0x0202);
			Value |= 0x0;
			RTMP_IO_WRITE32( pAd, GPIO_CTRL_CFG, Value);
		}
#endif /* BT_COEXISTENCE_SUPPORT */

#ifdef RTMP_USB_SUPPORT
#ifdef RELEASE_EXCLUDE
		/*
			Firmware patch : 
			enable firmware to poll GPIO0 to enable/disable MAC TX.
			arg0 (disable/enable firmware to poll GPIO0): 
				0x00 => disable; 0x01: enable
			arg1 (BT Mode):
				0x00 => 2-wire solution
						firmware polling gpio0 to enable/disable MAC tx, 
						and accumulate BT priority counter at the same time.
				0x01 => 3-wire solution
						firmware polling gpio0 to enable/disable MAC tx, 
						and pulling gpio2 to accumulate BT active counter.
				0x02 => patch for Wireless Tx blocking when BT power off
						firmware polling gpio0, no disable MAC tx, 
						only accumulate BT priority counter at the same time.
		*/
#endif /* RELEASE_EXCLUDE */
#ifdef BT_COEXISTENCE_SUPPORT
		if(pAd->bCoexMethod == TRUE)
		{
#ifdef RELEASE_EXCLUDE
			/*8070=>3 wire*/
#endif /* RELEASE_EXCLUDE */
			AsicSendCommandToMcu(pAd, MCU_SET_ACTIVE_POLLING, 0xff , 0x01, 0x01, FALSE);
		}
		else
#endif /* BT_COEXISTENCE_SUPPORT */
		{
#ifdef RELEASE_EXCLUDE
			/*3070,3870 =>2wire*/
#endif /* RELEASE_EXCLUDE */
			AsicSendCommandToMcu(pAd, MCU_SET_ACTIVE_POLLING, 0xff , 0x01, 0x00, FALSE);
		}
#endif /* RTMP_USB_SUPPORT */
		
#ifdef BT_COEXISTENCE_SUPPORT
		MiscInit(pAd);
#endif /* BT_COEXISTENCE_SUPPORT */

		pAd->bHWCoexistenceInit = TRUE;
		
		DBGPRINT(RT_DEBUG_TRACE,("Hardware Coexistence Initialized\n"));			
	}
}
#endif /* HW_COEXISTENCE_SUPPORT */


PNET_DEV RtmpPhyNetDevMainCreate(VOID *pAdSrc)
{
	RTMP_ADAPTER *pAd = (RTMP_ADAPTER *)pAdSrc;
	PNET_DEV pDevNew;
	UINT32 MC_RowID = 0, IoctlIF = 0;
	char *dev_name;

#ifdef MULTIPLE_CARD_SUPPORT
	MC_RowID = pAd->MC_RowID;
#endif /* MULTIPLE_CARD_SUPPORT */
#ifdef HOSTAPD_SUPPORT
	IoctlIF = pAd->IoctlIF;
#endif /* HOSTAPD_SUPPORT */

	dev_name = get_dev_name_prefix(pAd, INT_MAIN);
	pDevNew = RtmpOSNetDevCreate((INT32)MC_RowID, (UINT32 *)&IoctlIF,
					INT_MAIN, 0, sizeof(struct mt_dev_priv), dev_name);

#ifdef HOSTAPD_SUPPORT
	pAd->IoctlIF = IoctlIF;
#endif /* HOSTAPD_SUPPORT */

	return pDevNew;
}


#ifdef CONFIG_STA_SUPPORT
#ifdef PROFILE_STORE
static void WriteConfToDatFile(RTMP_ADAPTER *pAd)
{
	char	*cfgData = 0, *offset = 0;
	PSTRING fileName = NULL, pTempStr = NULL;
	RTMP_OS_FD file_r, file_w;
	RTMP_OS_FS_INFO osFSInfo;
	LONG rv, fileLen = 0;


	DBGPRINT(RT_DEBUG_TRACE, ("-----> WriteConfToDatFile\n"));

#ifdef RTMP_RBUS_SUPPORT
	if (pAd->infType == RTMP_DEV_INF_RBUS)
		fileName = STA_PROFILE_PATH_RBUS;
	else
#endif /* RTMP_RBUS_SUPPORT */
		fileName = STA_PROFILE_PATH;

	RtmpOSFSInfoChange(&osFSInfo, TRUE);

	file_r = RtmpOSFileOpen(fileName, O_RDONLY, 0);
	if (IS_FILE_OPEN_ERR(file_r)) 
	{
		DBGPRINT(RT_DEBUG_TRACE, ("-->1) %s: Error opening file %s\n", __FUNCTION__, fileName));
		return;
	}
	else 
	{
		char tempStr[64] = {0};
		while((rv = RtmpOSFileRead(file_r, tempStr, 64)) > 0)
		{
			fileLen += rv;
		}
		os_alloc_mem(NULL, (UCHAR **)&cfgData, fileLen);
		if (cfgData == NULL)
		{
			RtmpOSFileClose(file_r);
			DBGPRINT(RT_DEBUG_TRACE, ("CfgData mem alloc fail. (fileLen = %ld)\n", fileLen));
			goto out;
		}
		NdisZeroMemory(cfgData, fileLen);
		RtmpOSFileSeek(file_r, 0);
		rv = RtmpOSFileRead(file_r, (PSTRING)cfgData, fileLen);
		RtmpOSFileClose(file_r);
		if (rv != fileLen)
		{
			DBGPRINT(RT_DEBUG_TRACE, ("CfgData mem alloc fail, fileLen = %ld\n", fileLen));
			goto ReadErr;
		}
	}

	file_w = RtmpOSFileOpen(fileName, O_WRONLY|O_TRUNC, 0);
	if (IS_FILE_OPEN_ERR(file_w)) 
	{
		goto WriteFileOpenErr;
	}
	else 
	{
		offset = (PCHAR) rtstrstr((PSTRING) cfgData, "Default\n");
		offset += strlen("Default\n");
		RtmpOSFileWrite(file_w, (PSTRING)cfgData, (int)(offset-cfgData));
		os_alloc_mem(NULL, (UCHAR **)&pTempStr, 512);
		if (!pTempStr)
		{
			DBGPRINT(RT_DEBUG_TRACE, ("pTempStr mem alloc fail. (512)\n"));
			RtmpOSFileClose(file_w);
			goto WriteErr;
		}
			
		for (;;)
		{
			int i = 0;
			PSTRING ptr;

			NdisZeroMemory(pTempStr, 512);
			ptr = (PSTRING) offset;
			while(*ptr && *ptr != '\n')
			{
				pTempStr[i++] = *ptr++;
			}
			pTempStr[i] = 0x00;
			if ((size_t)(offset - cfgData) < fileLen)
			{
				offset += strlen(pTempStr) + 1;
				if (strncmp(pTempStr, "SSID=", strlen("SSID=")) == 0)
				{
					NdisZeroMemory(pTempStr, 512);
					NdisMoveMemory(pTempStr, "SSID=", strlen("SSID="));
					NdisMoveMemory(pTempStr + 5, pAd->CommonCfg.Ssid, pAd->CommonCfg.SsidLen);
				}
				else if (strncmp(pTempStr, "AuthMode=", strlen("AuthMode=")) == 0)
				{
					NdisZeroMemory(pTempStr, 512);
					if (pAd->StaCfg.AuthMode == Ndis802_11AuthModeOpen)
						sprintf(pTempStr, "AuthMode=OPEN");
					else if (pAd->StaCfg.AuthMode == Ndis802_11AuthModeShared)
						sprintf(pTempStr, "AuthMode=SHARED");
					else if (pAd->StaCfg.AuthMode == Ndis802_11AuthModeAutoSwitch)
						sprintf(pTempStr, "AuthMode=WEPAUTO");
					else if (pAd->StaCfg.AuthMode == Ndis802_11AuthModeWPAPSK)
						sprintf(pTempStr, "AuthMode=WPAPSK");
					else if (pAd->StaCfg.AuthMode == Ndis802_11AuthModeWPA2PSK)
						sprintf(pTempStr, "AuthMode=WPA2PSK");
					else if (pAd->StaCfg.AuthMode == Ndis802_11AuthModeWPA)
						sprintf(pTempStr, "AuthMode=WPA");
					else if (pAd->StaCfg.AuthMode == Ndis802_11AuthModeWPA2)
						sprintf(pTempStr, "AuthMode=WPA2");
					else if (pAd->StaCfg.AuthMode == Ndis802_11AuthModeWPANone)
						sprintf(pTempStr, "AuthMode=WPANONE");
				}
				else if (strncmp(pTempStr, "EncrypType=", strlen("EncrypType=")) == 0)
				{
					NdisZeroMemory(pTempStr, 512);
					if (pAd->StaCfg.WepStatus == Ndis802_11WEPDisabled)
						sprintf(pTempStr, "EncrypType=NONE");
					else if (pAd->StaCfg.WepStatus == Ndis802_11WEPEnabled)
						sprintf(pTempStr, "EncrypType=WEP");
					else if (pAd->StaCfg.WepStatus == Ndis802_11TKIPEnable)
						sprintf(pTempStr, "EncrypType=TKIP");
					else if (pAd->StaCfg.WepStatus == Ndis802_11AESEnable)
						sprintf(pTempStr, "EncrypType=AES");
				}
				RtmpOSFileWrite(file_w, pTempStr, strlen(pTempStr));
				RtmpOSFileWrite(file_w, "\n", 1);
			}
			else
			{
				break;
			}
		}
		RtmpOSFileClose(file_w);
	}

WriteErr:   
	if (pTempStr)
		os_free_mem(NULL, pTempStr);
ReadErr:
WriteFileOpenErr:    
	if (cfgData)
		os_free_mem(NULL, cfgData);
out:
	RtmpOSFSInfoChange(&osFSInfo, FALSE);


	DBGPRINT(RT_DEBUG_TRACE, ("<----- WriteConfToDatFile\n"));
	return;
}


INT write_dat_file_thread (
    IN ULONG Context)
{
	RTMP_OS_TASK *pTask;
	RTMP_ADAPTER *pAd;
	//int 	Status = 0;

	pTask = (RTMP_OS_TASK *)Context;

	if (pTask == NULL)
	{
		DBGPRINT(RT_DEBUG_TRACE, ("%s: pTask is NULL\n", __FUNCTION__));
		return 0;
	}
	
	pAd = (PRTMP_ADAPTER)RTMP_OS_TASK_DATA_GET(pTask);

	if (pAd == NULL)
	{
		DBGPRINT(RT_DEBUG_TRACE, ("%s: pAd is NULL\n", __FUNCTION__));
		return 0;
	}

	RtmpOSTaskCustomize(pTask);

	/* Update ssid, auth mode and encr type to DAT file */
	WriteConfToDatFile(pAd);
	
		RtmpOSTaskNotifyToExit(pTask);
	
	return 0;
}

NDIS_STATUS WriteDatThread(
	IN  RTMP_ADAPTER *pAd)
{
	NDIS_STATUS status = NDIS_STATUS_FAILURE;
	RTMP_OS_TASK *pTask;

	if (pAd->bWriteDat == FALSE)
		return 0;

	DBGPRINT(RT_DEBUG_TRACE, ("-->WriteDatThreadInit()\n"));

	pTask = &pAd->WriteDatTask;

	RTMP_OS_TASK_INIT(pTask, "RtmpWriteDatTask", pAd);
	status = RtmpOSTaskAttach(pTask, write_dat_file_thread, (ULONG)&pAd->WriteDatTask);
	DBGPRINT(RT_DEBUG_TRACE, ("<--WriteDatThreadInit(), status=%d!\n", status));

	return status;
}
#endif /* PROFILE_STORE */
#endif /* CONFIG_STA_SUPPORT */

