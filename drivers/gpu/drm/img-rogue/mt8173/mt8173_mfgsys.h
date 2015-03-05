/*
* Copyright (c) 2014 MediaTek Inc.
* Author: Chiawen Lee <chiawen.lee@mediatek.com>
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*/
#include "servicesext.h"
#include "rgxdevice.h"

#ifndef MT8173_MFGSYS_H
#define MT8173_MFGSYS_H

/* control APM is enabled or not  */
#define MTK_PM_SUPPORT 1

/* control RD is enabled or not */
/* RD_POWER_ISLAND only enable with E2 IC, disalbe in E1 IC @2013/9/17 */
/* #define RD_POWER_ISLAND 1 */

/*  unit ms, timeout interval for DVFS detection */
#define MTK_DVFS_SWITCH_INTERVAL  300

/*  need idle device before switching DVFS  */
#define MTK_DVFS_IDLE_DEVICE  0

/* used created thread to handle DVFS switch or not */
#define MTK_DVFS_CREATE_THREAD  0

#define ENABLE_MTK_MFG_DEBUG 0

#if ENABLE_MTK_MFG_DEBUG
#define mtk_mfg_debug(fmt, args...) pr_info("[MFG]" fmt, ##args)
#else
#define mtk_mfg_debug(fmt, args...)
#endif

extern struct regulator *g_vgpu;
extern struct clk *g_mmpll;

/* extern to be used by PVRCore_Init in RGX DDK module.c  */
extern int MTKMFGSystemInit(void);
extern int MTKMFGSystemDeInit(void);

extern void MTKSysSetInitialPowerState(void);
extern void MTKSysRestoreInitialPowerState(void);

/* below register interface in RGX sysconfig.c */
extern PVRSRV_ERROR MTKSysDevPrePowerState(PVRSRV_DEV_POWER_STATE eNew,
					   PVRSRV_DEV_POWER_STATE eCurrent,
					   IMG_BOOL bForced);
extern PVRSRV_ERROR MTKSysDevPostPowerState(PVRSRV_DEV_POWER_STATE eNew,
					    PVRSRV_DEV_POWER_STATE eCurrent,
					    IMG_BOOL bForced);
extern PVRSRV_ERROR MTKSystemPrePowerState(PVRSRV_SYS_POWER_STATE eNew);
extern PVRSRV_ERROR MTKSystemPostPowerState(PVRSRV_SYS_POWER_STATE eNew);

#endif /* MT8173_MFGSYS_H*/
