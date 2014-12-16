/*****************************************************************************
*  Copyright Statement:
*  --------------------
*  This software is protected by Copyright and the information contained
*  herein is confidential. The software may not be copied and the information
*  contained herein may not be used or disclosed except with the written
*  permission of MediaTek Inc. (C) 2005
*
*  BY OPENING THIS FILE, BUYER HEREBY UNEQUIVOCALLY ACKNOWLEDGES AND AGREES
*  THAT THE SOFTWARE/FIRMWARE AND ITS DOCUMENTATIONS ("MEDIATEK SOFTWARE")
*  RECEIVED FROM MEDIATEK AND/OR ITS REPRESENTATIVES ARE PROVIDED TO BUYER ON
*  AN "AS-IS" BASIS ONLY. MEDIATEK EXPRESSLY DISCLAIMS ANY AND ALL WARRANTIES,
*  EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED WARRANTIES OF
*  MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE OR NONINFRINGEMENT.
*  NEITHER DOES MEDIATEK PROVIDE ANY WARRANTY WHATSOEVER WITH RESPECT TO THE
*  SOFTWARE OF ANY THIRD PARTY WHICH MAY BE USED BY, INCORPORATED IN, OR
*  SUPPLIED WITH THE MEDIATEK SOFTWARE, AND BUYER AGREES TO LOOK ONLY TO SUCH
*  THIRD PARTY FOR ANY WARRANTY CLAIM RELATING THERETO. MEDIATEK SHALL ALSO
*  NOT BE RESPONSIBLE FOR ANY MEDIATEK SOFTWARE RELEASES MADE TO BUYER'S
*  SPECIFICATION OR TO CONFORM TO A PARTICULAR STANDARD OR OPEN FORUM.
*
*  BUYER'S SOLE AND EXCLUSIVE REMEDY AND MEDIATEK'S ENTIRE AND CUMULATIVE
*  LIABILITY WITH RESPECT TO THE MEDIATEK SOFTWARE RELEASED HEREUNDER WILL BE,
*  AT MEDIATEK'S OPTION, TO REVISE OR REPLACE THE MEDIATEK SOFTWARE AT ISSUE,
*  OR REFUND ANY SOFTWARE LICENSE FEES OR SERVICE CHARGE PAID BY BUYER TO
*  MEDIATEK FOR SUCH MEDIATEK SOFTWARE AT ISSUE. 
*
*  THE TRANSACTION CONTEMPLATED HEREUNDER SHALL BE CONSTRUED IN ACCORDANCE
*  WITH THE LAWS OF THE STATE OF CALIFORNIA, USA, EXCLUDING ITS CONFLICT OF
*  LAWS PRINCIPLES.  ANY DISPUTES, CONTROVERSIES OR CLAIMS ARISING THEREOF AND
*  RELATED THERETO SHALL BE SETTLED BY ARBITRATION IN SAN FRANCISCO, CA, UNDER
*  THE RULES OF THE INTERNATIONAL CHAMBER OF COMMERCE (ICC).
*
*****************************************************************************/

/*****************************************************************************
 *
 * Filename:
 * ---------
 *   mtk_mfgdvfs.h
 *
 * Project:
 * --------
 *   MT8135
 *
 * Description:
 * ------------
 *   Mtk RGX GPU dvfs definition and declaration for mtk_mfgdvfs.c file
 *
 * Author:
 * -------
 *   Enzhu Wang
 *
 *============================================================================
 * History and modification
 *1. Initial @ July 9th 2013
 *============================================================================
 ****************************************************************************/
#include "servicesext.h"

#ifndef MTK_MFGDVFS_H
#define MTK_MFGDVFS_H

typedef struct _MTK_DVFS_T
{
	IMG_HANDLE	hMtkDVFSTimer;
	IMG_HANDLE	hMtkDVFSThread;
} MTK_DVFS_T;

/* be called by  mtk_mfgsys.c */
#ifndef MTK_MFG_DVFS 

/* MFG Clock Mux Selection */
#define GPU_CKSQ_MUX_CK    (0)
#define GPU_UNIVPLL1_D4    (1)
#define GPU_SYSPLL_D2      (2)
#define GPU_SYSPLL_D2P5    (3)
#define GPU_SYSPLL_D3      (4)
#define GPU_UNIVPLL_D5     (5)
#define GPU_UNIVPLL1_D2    (6)
#define GPU_MMPLL_D2       (7)
#define GPU_MMPLL_D3       (8)
#define GPU_MMPLL_D4       (9)
#define GPU_MMPLL_D5       (10)
#define GPU_MMPLL_D6       (11)
#define GPU_MMPLL_D7       (12)

/* MFG Mem clock selection */
#define GPU_MEM_CKSQ_MUX_CK  (0)
#define GPU_MEM_TOP_SMI      (1)
#define GPU_MEM_TOP_MFG      (2)
#define GPU_MEM_TOP_MEM      (3)


#define GPU_FMFG    (0)
#define GPU_FMEM    (1)		/* GPU internal mem clock */

#define MAX_SAFEVOLT			1150000 /* 1.15V */

#define GPU_POWER_VGPU_1_15V   (0x48)
#define GPU_POWER_VGPU_1_10V   (0x40)
#define GPU_POWER_VGPU_1_05V   (0x38)
#define GPU_POWER_VGPU_1_025V  (0x34)
#define GPU_POWER_VGPU_1_00V   (0x30)
#define GPU_POWER_VGPU_0_95V   (0x28)
#define GPU_POWER_VGPU_0_90V   (0x20)

#define GPU_POWER_VGPU_1_15V_VOL   (1150)
#define GPU_POWER_VGPU_1_10V_VOL   (1100)
#define GPU_POWER_VGPU_1_05V_VOL   (1050)
#define GPU_POWER_VGPU_1_025V_VOL  (1025)
#define GPU_POWER_VGPU_1_00V_VOL   (1000)
#define GPU_POWER_VGPU_0_95V_VOL   (950)
#define GPU_POWER_VGPU_0_90V_VOL   (900)

#define GPU_DVFS_F1     (395000)	/* KHz  GPU_MMPLL_D3 @1185 */
#define GPU_DVFS_F2     (296250)	/* KHz  GPU_MMPLL_D4 */
#define GPU_DVFS_F3     (257833)	/* KHz  GPU_MMPLL_D3 @773.5 */
#define GPU_DVFS_F4     (249600)	/* KHz  GPU_UNIVPLL_D5 */
#define GPU_DVFS_F5     (237000)	/* KHz  GPU_MMPLL_D5 @1185 */
#define GPU_DVFS_F6     (197500)	/* KHz  GPU_MMPLL_D6 @1185 */
#define GPU_DVFS_F7     (169286)	/* KHz  GPU_MMPLL_D7 @1185 */
#define GPU_DVFS_F8     (156000)	/* KHz  GPU_UNIVPLL1_D4 */

#define GPU_MMPLL_D3_CLOCK       (GPU_DVFS_F1)
#define GPU_MMPLL_D4_CLOCK       (GPU_DVFS_F2)
#define GPU_MMPLL_773_D3_CLOCK   (GPU_DVFS_F3)
#define GPU_UNIVPLL_D5_CLOCK     (GPU_DVFS_F4)
#define GPU_MMPLL_D5_CLOCK       (GPU_DVFS_F5)
#define GPU_MMPLL_D6_CLOCK       (GPU_DVFS_F6)
#define GPU_MMPLL_D7_CLOCK       (GPU_DVFS_F7)
#define GPU_UNIVPLL1_D4_CLOCK    (GPU_DVFS_F8)

#define GPU_BONDING_000  GPU_DVFS_F1
#define GPU_BONDING_001  (286000)
#define GPU_BONDING_010  (357000)
#define GPU_BONDING_011  (400000)
#define GPU_BONDING_100  (455000)
#define GPU_BONDING_101  (476000)
#define GPU_BONDING_110  (476000)
#define GPU_BONDING_111  (476000)

#define GPU_DVFS_OVER_F450     (450000)	/* (1350000) */
#define GPU_DVFS_OVER_F475     (475000)	/* (1425000) */
#define GPU_DVFS_OVER_F500     (500000)	/* (1500000) */
#define GPU_DVFS_OVER_F550     (550000)	/* (1650000) */

#define GPU_MMPLL_1650_D3_CLOCK    (GPU_DVFS_OVER_F550)
#define GPU_MMPLL_1500_D3_CLOCK    (GPU_DVFS_OVER_F500)
#define GPU_MMPLL_1425_D3_CLOCK    (GPU_DVFS_OVER_F475)
#define GPU_MMPLL_1350_D3_CLOCK    (GPU_DVFS_OVER_F450)

#define MMPLL   0
#define UNIVPLL 1

extern IMG_INT32 MTKInitFreqInfo(struct platform_device *dev);
extern void MTKDeInitFreqInfo(struct platform_device *dev);
#else
extern IMG_INT32 MTKSetFreqInfo(FREQ_TABLE_DVFS_TYPE TblType);
#endif
extern PVRSRV_ERROR MtkDVFSInit(IMG_VOID);
extern PVRSRV_ERROR MtkDVFSDeInit(IMG_VOID);

#endif // MTK_MFGDVFS_H
