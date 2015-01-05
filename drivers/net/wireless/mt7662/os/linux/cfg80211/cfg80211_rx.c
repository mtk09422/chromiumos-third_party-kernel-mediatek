/*
 ***************************************************************************
 * Ralink Tech Inc.
 * 4F, No. 2 Technology 5th Rd.
 * Science-based Industrial Park
 * Hsin-chu, Taiwan, R.O.C.
 *
 * (c) Copyright 2002-2013, Ralink Technology, Inc.
 *
 * All rights reserved. Ralink's source code is an unpublished work and the
 * use of a copyright notice does not imply otherwise. This source code
 * contains confidential trade secret material of Ralink Tech. Any attemp
 * or participation in deciphering, decoding, reverse engineering or in any
 * way altering the source code is stricitly prohibited, unless the prior
 * written consent of Ralink Technology, Inc. is obtained.
 ***************************************************************************

	Module Name:

	Abstract:

	Revision History:
	Who 		When			What
	--------	----------		----------------------------------------------
*/

#define RTMP_MODULE_OS

#ifdef RT_CFG80211_SUPPORT

#include "rt_config.h"

extern UCHAR CFG_P2POUIBYTE[];

VOID CFG80211_Convert802_3Packet(RTMP_ADAPTER *pAd, RX_BLK *pRxBlk, UCHAR *pHeader802_3)
{
#ifdef CONFIG_AP_SUPPORT
	if (IS_PKT_OPMODE_AP(pRxBlk))
	{
    	RTMP_AP_802_11_REMOVE_LLC_AND_CONVERT_TO_802_3(pRxBlk, pHeader802_3);
	}
	else
#endif /* CONFIG_AP_SUPPORT */
	{
#ifdef CONFIG_STA_SUPPORT
		RTMP_802_11_REMOVE_LLC_AND_CONVERT_TO_802_3(pRxBlk, pHeader802_3);
#endif /*CONFIG_STA_SUPPORT*/
	}
}

VOID CFG80211_Announce802_3Packet(RTMP_ADAPTER *pAd, RX_BLK *pRxBlk, UCHAR FromWhichBSSID)
{
#ifdef CONFIG_AP_SUPPORT
	if (IS_PKT_OPMODE_AP(pRxBlk))
	{
		AP_ANNOUNCE_OR_FORWARD_802_3_PACKET(pAd, pRxBlk->pRxPacket, FromWhichBSSID);
	}
	else
#endif /* CONFIG_AP_SUPPORT */
	{
#ifdef CONFIG_STA_SUPPORT
		ANNOUNCE_OR_FORWARD_802_3_PACKET(pAd, pRxBlk->pRxPacket, FromWhichBSSID);
#endif /*CONFIG_STA_SUPPORT*/
	}

}

ACTION_FRAME_TYPE CFG80211_CheckActionFrameType(
        IN  RTMP_ADAPTER 								 *pAd,
		IN	PUCHAR										 preStr,
		IN	PUCHAR										 pData,
		IN	UINT32                              		 length)
{
	ACTION_FRAME_TYPE actionFrame = NONE_ACTION_FRAME_TYPE;
	struct ieee80211_mgmt *mgmt;
	mgmt = (struct ieee80211_mgmt *)pData;
	if (ieee80211_is_mgmt(mgmt->frame_control))
	{
		if (ieee80211_is_probe_resp(mgmt->frame_control))
		{
			DBGPRINT(RT_DEBUG_INFO, ("CFG80211_PKT: %s ProbeRsp Frame %d\n", preStr, pAd->LatchRfRegs.Channel));
	        if (!mgmt->u.probe_resp.timestamp)
    		{
            		struct timeval tv;
            		do_gettimeofday(&tv);
            		mgmt->u.probe_resp.timestamp = ((UINT64) tv.tv_sec * 1000000) + tv.tv_usec;
    		}
		}
		else if (ieee80211_is_disassoc(mgmt->frame_control))
		{
			DBGPRINT(RT_DEBUG_ERROR, ("CFG80211_PKT: %s DISASSOC Frame\n", preStr));
		}
		else if (ieee80211_is_deauth(mgmt->frame_control))
		{
			DBGPRINT(RT_DEBUG_ERROR, ("CFG80211_PKT: %s Deauth Frame\n", preStr));
		}
		else if (ieee80211_is_action(mgmt->frame_control))
		{
			PP2P_PUBLIC_FRAME pFrame = (PP2P_PUBLIC_FRAME)pData;
			if ((pFrame->p80211Header.FC.SubType == SUBTYPE_ACTION) &&
			    (pFrame->Category == CATEGORY_PUBLIC) &&
			    (pFrame->Action == ACTION_WIFI_DIRECT))
			{
				actionFrame = PUBLIC_ACTION_FRAME_TYPE;
				switch (pFrame->Subtype)
				{
					case GO_NEGOCIATION_REQ:
						DBGPRINT(RT_DEBUG_ERROR, ("CFG80211_PKT: (%02X:%02X:%02X:%02X:%02X:%02X) -> (%02X:%02X:%02X:%02X:%02X:%02X) %s GO_NEGOCIACTION_REQ %d\n", 
									PRINT_MAC(mgmt->sa), PRINT_MAC(mgmt->da) ,preStr, pAd->LatchRfRegs.Channel));
						break;

					case GO_NEGOCIATION_RSP:
						DBGPRINT(RT_DEBUG_ERROR, ("CFG80211_PKT: (%02X:%02X:%02X:%02X:%02X:%02X) -> (%02X:%02X:%02X:%02X:%02X:%02X) %s GO_NEGOCIACTION_RSP %d\n", 
									PRINT_MAC(mgmt->sa), PRINT_MAC(mgmt->da) ,preStr, pAd->LatchRfRegs.Channel));
						break;

					case GO_NEGOCIATION_CONFIRM:
						DBGPRINT(RT_DEBUG_ERROR, ("CFG80211_PKT: (%02X:%02X:%02X:%02X:%02X:%02X) -> (%02X:%02X:%02X:%02X:%02X:%02X) %s GO_NEGOCIACTION_CONFIRM %d\n", 
									PRINT_MAC(mgmt->sa), PRINT_MAC(mgmt->da) ,preStr,  pAd->LatchRfRegs.Channel));
						break;

					case P2P_PROVISION_REQ:
						DBGPRINT(RT_DEBUG_ERROR, ("CFG80211_PKT: (%02X:%02X:%02X:%02X:%02X:%02X) -> (%02X:%02X:%02X:%02X:%02X:%02X) %s P2P_PROVISION_REQ %d\n", 
									PRINT_MAC(mgmt->sa), PRINT_MAC(mgmt->da) ,preStr, pAd->LatchRfRegs.Channel));
						break;

					case P2P_PROVISION_RSP:
						DBGPRINT(RT_DEBUG_ERROR, ("CFG80211_PKT: (%02X:%02X:%02X:%02X:%02X:%02X) -> (%02X:%02X:%02X:%02X:%02X:%02X) %s P2P_PROVISION_RSP %d\n", 
									PRINT_MAC(mgmt->sa), PRINT_MAC(mgmt->da) ,preStr, pAd->LatchRfRegs.Channel));
						break;

					case P2P_INVITE_REQ:
						DBGPRINT(RT_DEBUG_ERROR, ("CFG80211_PKT: (%02X:%02X:%02X:%02X:%02X:%02X) -> (%02X:%02X:%02X:%02X:%02X:%02X) %s P2P_INVITE_REQ %d\n", 
									PRINT_MAC(mgmt->sa), PRINT_MAC(mgmt->da) ,preStr, pAd->LatchRfRegs.Channel));
						break;

					case P2P_INVITE_RSP:
						DBGPRINT(RT_DEBUG_ERROR, ("CFG80211_PKT: (%02X:%02X:%02X:%02X:%02X:%02X) -> (%02X:%02X:%02X:%02X:%02X:%02X) %s P2P_INVITE_RSP %d\n", 
									PRINT_MAC(mgmt->sa), PRINT_MAC(mgmt->da) ,preStr, pAd->LatchRfRegs.Channel));
						break;
					case P2P_DEV_DIS_REQ:
                        DBGPRINT(RT_DEBUG_ERROR, ("CFG80211_PKT: (%02X:%02X:%02X:%02X:%02X:%02X) -> (%02X:%02X:%02X:%02X:%02X:%02X) %s P2P_DEV_DIS_REQ %d\n",
                                                PRINT_MAC(mgmt->sa), PRINT_MAC(mgmt->da) ,preStr, pAd->LatchRfRegs.Channel));
						break;
					case P2P_DEV_DIS_RSP:
                        DBGPRINT(RT_DEBUG_ERROR, ("CFG80211_PKT: (%02X:%02X:%02X:%02X:%02X:%02X) -> (%02X:%02X:%02X:%02X:%02X:%02X) %s P2P_DEV_DIS_RSP %d\n",
                                                PRINT_MAC(mgmt->sa), PRINT_MAC(mgmt->da) ,preStr, pAd->LatchRfRegs.Channel));
                        break;
				}
			}
			 else if ((pFrame->p80211Header.FC.SubType == SUBTYPE_ACTION) &&
					  (pFrame->Category == CATEGORY_PUBLIC) &&
					   ((pFrame->Action == ACTION_GAS_INITIAL_REQ) 	 ||
						(pFrame->Action == ACTION_GAS_INITIAL_RSP)	 ||
						(pFrame->Action == ACTION_GAS_COMEBACK_REQ ) ||
						(pFrame->Action == ACTION_GAS_COMEBACK_RSP)))
			{
				actionFrame = PUBLIC_ACTION_FRAME_TYPE;
			}
			else if	(pFrame->Category == CATEGORY_VENDOR_SPECIFIC_WFD)
			{
			    PP2P_ACTION_FRAME pP2PActionFrame = (PP2P_ACTION_FRAME) pData;
                if (RTMPEqualMemory(&pP2PActionFrame->Octet[2], CFG_P2POUIBYTE, 4))
                {
    				actionFrame = P2P_ACTION_FRAME_TYPE;
    				switch (pP2PActionFrame->Subtype)
				{
                    case P2PACT_NOA:
                            DBGPRINT(RT_DEBUG_ERROR, ("CFG80211_PKT: (%02X:%02X:%02X:%02X:%02X:%02X) -> (%02X:%02X:%02X:%02X:%02X:%02X) %s P2PACT_NOA %d\n",
                                                      PRINT_MAC(mgmt->sa), PRINT_MAC(mgmt->da), preStr, pAd->LatchRfRegs.Channel));
						break;
					case P2PACT_PERSENCE_REQ:
                            DBGPRINT(RT_DEBUG_ERROR, ("CFG80211_PKT: (%02X:%02X:%02X:%02X:%02X:%02X) -> (%02X:%02X:%02X:%02X:%02X:%02X) %s P2PACT_PERSENCE_REQ %d\n",
                                                      PRINT_MAC(mgmt->sa), PRINT_MAC(mgmt->da), preStr, pAd->LatchRfRegs.Channel));
						break;
					case P2PACT_PERSENCE_RSP:
                            DBGPRINT(RT_DEBUG_ERROR, ("CFG80211_PKT: (%02X:%02X:%02X:%02X:%02X:%02X) -> (%02X:%02X:%02X:%02X:%02X:%02X) %s P2PACT_PERSENCE_RSP %d\n",
                                                      PRINT_MAC(mgmt->sa), PRINT_MAC(mgmt->da), preStr, pAd->LatchRfRegs.Channel));
						break;
					case P2PACT_GO_DISCOVER_REQ:
                            DBGPRINT(RT_DEBUG_ERROR, ("CFG80211_PKT: (%02X:%02X:%02X:%02X:%02X:%02X) -> (%02X:%02X:%02X:%02X:%02X:%02X) %s P2PACT_GO_DISCOVER_REQ %d\n",
                                                      PRINT_MAC(mgmt->sa), PRINT_MAC(mgmt->da), preStr, pAd->LatchRfRegs.Channel));
						break;
				}
			}
			}
			else
			{
				DBGPRINT(RT_DEBUG_INFO, ("CFG80211_PKT: %s ACTION Frame with Channel%d\n", preStr, pAd->LatchRfRegs.Channel));
			}
		}
		else
		{
			DBGPRINT(RT_DEBUG_ERROR, ("CFG80211_PKT: %s UNKOWN MGMT FRAME TYPE\n", preStr));
		}
	}
	else
	{
		DBGPRINT(RT_DEBUG_ERROR, ("CFG80211_PKT: %s UNKOWN FRAME TYPE\n", preStr));
	}

	return actionFrame;
}
#ifdef CFG_TDLS_SUPPORT
BOOLEAN CFG80211_HandleTdlsDiscoverRespFrame(RTMP_ADAPTER *pAd, RX_BLK *pRxBlk, UCHAR OpMode)
{
	RXWI_STRUC *pRxWI = pRxBlk->pRxWI;
	PHEADER_802_11 pHeader = pRxBlk->pHeader;
	PNET_DEV pNetDev = NULL;
	UINT32 freq;
	//PNDIS_PACKET pRxPacket = pRxBlk->pRxPacket;
	PP2P_PUBLIC_FRAME pFrame = (PP2P_PUBLIC_FRAME)pHeader;


	if ((pFrame->p80211Header.FC.SubType == SUBTYPE_ACTION) &&
		(pFrame->Category == CATEGORY_PUBLIC) &&
		(pFrame->Action == ACTION_TDLS_DISCOVERY_RSP))
	{
		DBGPRINT(RT_DEBUG_TRACE, ("%s : GOT ACTION_TDLS_DISCOVERY_RSP ACTION: 0x%x \n",__FUNCTION__,pFrame->Action));
		MAP_CHANNEL_ID_TO_KHZ(pAd->LatchRfRegs.Channel, freq);
		freq /= 1000;
		CFG80211OS_RxMgmt(pAd->net_dev, freq, (PUCHAR)pHeader, pRxWI->RXWI_N.MPDUtotalByteCnt);
		return TRUE;
	}

	/*
	report TDLS DiscoverRespFrame to supplicant , but it does nothing ,
	events.c line 2432
	*/

	return FALSE;


}
#endif /* CFG_TDLS_SUPPORT */
BOOLEAN CFG80211_HandleP2pMgmtFrame(RTMP_ADAPTER *pAd, RX_BLK *pRxBlk, UCHAR OpMode)
{
	RXWI_STRUC *pRxWI = pRxBlk->pRxWI;
	PHEADER_802_11 pHeader = pRxBlk->pHeader;
	PCFG80211_CTRL pCfg80211_ctrl = &pAd->cfg80211_ctrl;
	UINT32 freq;

	if ((pHeader->FC.SubType == SUBTYPE_PROBE_REQ) ||
	 	 ((pHeader->FC.SubType == SUBTYPE_ACTION) &&
	 	   CFG80211_CheckActionFrameType(pAd, "RX", (PUCHAR)pHeader, pRxWI->RXWI_N.MPDUtotalByteCnt)))
		{
			MAP_CHANNEL_ID_TO_KHZ(RTMP_GetPrimaryCh(pAd, pAd->LatchRfRegs.Channel), freq);
			freq /= 1000;

#ifdef RT_CFG80211_P2P_CONCURRENT_DEVICE
			/* Check the P2P_GO exist in the VIF List */
			if (pCfg80211_ctrl->Cfg80211VifDevSet.vifDevList.size > 0)
			{
				PNET_DEV pNetDev = NULL;
				BOOLEAN res = FALSE;

				if ((pNetDev = RTMP_CFG80211_FindVifEntry_ByType(pAd, RT_CMD_80211_IFTYPE_P2P_GO)) != NULL)
				{
					DBGPRINT(RT_DEBUG_INFO, ("VIF STA GO RtmpOsCFG80211RxMgmt OK!! TYPE = %d, freq = %d, %02x:%02x:%02x:%02x:%02x:%02x\n",
									  pHeader->FC.SubType, freq, PRINT_MAC(pHeader->Addr2)));

					if (pHeader->FC.SubType == SUBTYPE_ACTION)
					{
						if(CFG80211_CheckActionFrameType(pAd, "CHECK", (PUCHAR)pHeader, pRxWI->RXWI_N.MPDUtotalByteCnt) == P2P_ACTION_FRAME_TYPE)
							res = CFG80211OS_RxMgmt(pNetDev, freq, (PUCHAR)pHeader, pRxWI->RXWI_N.MPDUtotalByteCnt);
						else
						res = CFG80211OS_RxMgmt(CFG80211_GetEventDevice(pAd), freq, (PUCHAR)pHeader, pRxWI->RXWI_N.MPDUtotalByteCnt);

						if (res != TRUE)
							DBGPRINT(RT_DEBUG_ERROR,(" SUBTYPE_ACTION CFG80211OS_RxMgmt failed! (%02x:%02x:%02x:%02x:%02x:%02x)\n", PRINT_MAC(pHeader->Addr2)));
					}
					else if (pHeader->FC.SubType == SUBTYPE_PROBE_REQ)
						res = CFG80211OS_RxMgmt(pNetDev, freq, (PUCHAR)pHeader, pRxWI->RXWI_N.MPDUtotalByteCnt);
					if (res != TRUE)
						DBGPRINT(RT_DEBUG_INFO,("CFG80211OS_RxMgmt failed! (%02x:%02x:%02x:%02x:%02x:%02x)\n", PRINT_MAC(pHeader->Addr2)));

					if (OpMode == OPMODE_AP)
						return TRUE;
				}
			}
#endif /* RT_CFG80211_P2P_CONCURRENT_DEVICE */

			if ( ((pHeader->FC.SubType == SUBTYPE_PROBE_REQ) &&
                 (pCfg80211_ctrl->cfg80211MainDev.Cfg80211RegisterProbeReqFrame == TRUE) ) ||
			     ((pHeader->FC.SubType == SUBTYPE_ACTION)  /*&& ( pAd->Cfg80211RegisterActionFrame == TRUE)*/ ))
			{
				DBGPRINT(RT_DEBUG_INFO,("MAIN STA RtmpOsCFG80211RxMgmt OK!! TYPE = %d, freq = %d, %02x:%02x:%02x:%02x:%02x:%02x\n",
										pHeader->FC.SubType, freq, PRINT_MAC(pHeader->Addr2)));
				if (CFG80211_GetEventDevice(pAd) == NULL) //Fix crash problem when hostapd boot up.
				{
					DBGPRINT(RT_DEBUG_ERROR,("Not Ready for p2p0 netdevice\n")); 
					return FALSE;
				}
				else 
					{
						if (RTMP_CFG80211_HOSTAPD_ON(pAd) )
							CFG80211OS_RxMgmt(pAd->net_dev, freq,  (PUCHAR)pHeader, pRxWI->RXWI_N.MPDUtotalByteCnt);
						else
							CFG80211OS_RxMgmt(CFG80211_GetEventDevice(pAd), freq, (PUCHAR)pHeader, pRxWI->RXWI_N.MPDUtotalByteCnt);
					}

				if (OpMode == OPMODE_AP)
						return TRUE;
			}
		}

	return FALSE;
}


#endif /* RT_CFG80211_SUPPORT */

