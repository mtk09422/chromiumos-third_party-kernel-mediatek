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
 *   mtk_mfgsys.h
 *
 * Project:
 * --------
 *   MT8135
 *
 * Description:
 * ------------
 *   Mtk RGX GPU system definition and declaration for mtk_mfgsys.c and mtk_mfgdvfs.c file
 *
 * Author:
 * -------
 *   Enzhu Wang
 *
 *============================================================================
 * History and modification
 *1. Initial @ April 27th 2013
 *============================================================================
 ****************************************************************************/
#include "servicesext.h"
#include "rgxdevice.h"

#ifndef MTK_MFGSYS_H
#define MTK_MFGSYS_H

/* control APM is enabled or not  */
#define MTK_PM_SUPPORT 1  

/* control RD is enabled or not */
/* RD_POWER_ISLAND only enable with E2 IC, disalbe in E1 IC @2013/9/17 */
//#define RD_POWER_ISLAND 1 

/*  unit ms, timeout interval for DVFS detection */
#define MTK_DVFS_SWITCH_INTERVAL  300 

/*  need idle device before switching DVFS  */
#define MTK_DVFS_IDLE_DEVICE  0       

/* used created thread to handle DVFS switch or not */
#define MTK_DVFS_CREATE_THREAD  0       

#define ENABLE_MTK_MFG_DEBUG 0

#if ENABLE_MTK_MFG_DEBUG
#define mtk_mfg_debug(fmt, args...)     printk("[MFG]" fmt, ##args)
#else
#define mtk_mfg_debug(fmt, args...) 
#endif

typedef enum _FREQ_TABLE_TYPE_
{
    TBL_TYPE_DVFS_NOT_SUPPORT =0x0, /* means use init freq and volt always, not support DVFS */
    TBL_TYPE_DVFS_2_LEVEL =0x1,     /* two level to switch */
    TBL_TYPE_DVFS_3_LEVEL =0x2,     /* three level to switch */
    TBL_TYPE_DVFS_4_LEVEL =0x3,     /* four level to switch */
    TBL_TYPE_DVFS_MAX =0xf,
} FREQ_TABLE_DVFS_TYPE;

//extern to be used by PVRCore_Init in RGX DDK module.c 
extern int MTKMFGSystemInit(void);
extern int MTKMFGSystemDeInit(void);
//extern bool MTKMFGIsE2andAboveVersion(void);

extern void MTKSysSetInitialPowerState(void);

extern void MTKSysRestoreInitialPowerState(void);

/* below register interface in RGX sysconfig.c */
extern PVRSRV_ERROR MTKSysDevPrePowerState(PVRSRV_DEV_POWER_STATE eNewPowerState,
                                         PVRSRV_DEV_POWER_STATE eCurrentPowerState,
									     IMG_BOOL bForced);
extern PVRSRV_ERROR MTKSysDevPostPowerState(PVRSRV_DEV_POWER_STATE eNewPowerState,
                                          PVRSRV_DEV_POWER_STATE eCurrentPowerState,
									      IMG_BOOL bForced);
extern PVRSRV_ERROR MTKSystemPrePowerState(PVRSRV_SYS_POWER_STATE eNewPowerState);

extern PVRSRV_ERROR MTKSystemPostPowerState(PVRSRV_SYS_POWER_STATE eNewPowerState);

#endif // MTK_MFGSYS_H
