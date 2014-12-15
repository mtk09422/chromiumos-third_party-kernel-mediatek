/****************************************************************************
 * Ralink Tech Inc.
 * Taiwan, R.O.C.
 *
 * (c) Copyright 2013, Ralink Technology, Inc.
 *
 * All rights reserved. Ralink's source code is an unpublished work and the
 * use of a copyright notice does not imply otherwise. This source code
 * contains confidential trade secret material of Ralink Tech. Any attemp
 * or participation in deciphering, decoding, reverse engineering or in any
 * way altering the source code is stricitly prohibited, unless the prior
 * written consent of Ralink Technology, Inc. is obtained.
 ***************************************************************************/

/****************************************************************************

	Abstract:

	All related CFG80211 function body.

	History:

***************************************************************************/
#define RTMP_MODULE_OS

#ifdef RT_CFG80211_SUPPORT

#include "rt_config.h"


INT CFG80211_StaPortSecured(RTMP_ADAPTER *pAd, const UCHAR *mac, UINT flag)
{
	MAC_TABLE_ENTRY *pEntry = MacTableLookup(pAd, mac);

	if (!pEntry) {
		DBGPRINT(RT_DEBUG_ERROR, ("Can't find pEntry in %s\n",
					  __func__));
	} else {
		if (flag) {
			pEntry->PrivacyFilter = Ndis802_11PrivFilterAcceptAll;
			pEntry->WpaState = AS_PTKINITDONE;
			pEntry->PortSecured = WPA_802_1X_PORT_SECURED;
			DBGPRINT(RT_DEBUG_TRACE, ("AID:%d, PortSecured\n",
						  pEntry->Aid));
		} else {
			pEntry->PrivacyFilter = Ndis802_11PrivFilter8021xWEP;
			pEntry->PortSecured = WPA_802_1X_PORT_NOT_SECURED;
			DBGPRINT(RT_DEBUG_TRACE, ("AID:%d, PortNotSecured\n",
						  pEntry->Aid));
		}
	}

	return 0;
}

INT CFG80211_ApStaDel(RTMP_ADAPTER *pAd, const UCHAR *mac)
{
	if (mac == NULL) {
#ifdef RT_CFG80211_P2P_CONCURRENT_DEVICE
		/* From WCID=2 */
		if (INFRA_ON(pAd))
			;//P2PMacTableReset(pAd);
		else
#endif /* RT_CFG80211_P2P_CONCURRENT_DEVICE */
			MacTableReset(pAd);
	} else {
		MAC_TABLE_ENTRY *pEntry = MacTableLookup(pAd, mac);
		if (pEntry)
			MlmeDeAuthAction(pAd, pEntry, REASON_NO_LONGER_VALID, FALSE);
		else
			DBGPRINT(RT_DEBUG_ERROR, ("Can't find pEntry in ApStaDel\n"));
	}

	return 0;
}

#endif /* RT_CFG80211_SUPPORT */
