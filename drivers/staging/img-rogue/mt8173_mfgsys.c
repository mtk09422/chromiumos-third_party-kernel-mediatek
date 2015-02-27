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
#include <linux/delay.h>
#include <linux/mutex.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/regulator/consumer.h>

#include "pvrsrv_error.h"

#include "mt8173_mfgsys.h"
#include "mt8173_mfgdvfs.h"

struct regulator *g_vgpu;
struct clk *g_mmpll;

static struct clk *g_mfgclk_sys;
static struct clk *g_mfgclk_axi;
static struct clk *g_mfgclk_mem;
static struct clk *g_mfgclk_g3d;
static struct clk *g_mfgclk_26m;

static void __iomem *gbRegBase;
struct platform_device *mtkBackupPVRLDMDev = NULL;

static bool bGetClock;
static bool bMfgInit;


#define DRV_WriteReg32(ptr, data) \
	writel((u32)data, ptr)

#define DRV_Reg32(ptr) \
	(readl(ptr))

static int MtkEnableMfgClock(void)
{
	mtk_mfg_debug("MtkEnableMfgClock[%d] Begin... clks %p %p %p %p\n",
		      bGetClock, g_mfgclk_axi, g_mfgclk_mem,
		      g_mfgclk_g3d, g_mfgclk_26m);

	if (bGetClock == false)
		return PVRSRV_OK;

	clk_prepare(g_mfgclk_sys);
	clk_enable(g_mfgclk_sys);

	clk_prepare(g_mfgclk_axi);
	clk_enable(g_mfgclk_axi);

	clk_prepare(g_mfgclk_mem);
	clk_enable(g_mfgclk_mem);

	clk_prepare(g_mfgclk_g3d);
	clk_enable(g_mfgclk_g3d);

	clk_prepare(g_mfgclk_26m);
	clk_enable(g_mfgclk_26m);

	mtk_mfg_debug("MtkEnableMfgClock  end...\n");
	return PVRSRV_OK;
}

static int MtkDisableMfgClock(void)
{
	mtk_mfg_debug("MtkDisableMfgClock[%d]\n", bGetClock);
	return PVRSRV_OK;
	if (bGetClock == false)
		return PVRSRV_OK;

	clk_disable(g_mfgclk_26m);
	clk_unprepare(g_mfgclk_26m);

	clk_disable(g_mfgclk_g3d);
	clk_unprepare(g_mfgclk_g3d);

	clk_disable(g_mfgclk_mem);
	clk_unprepare(g_mfgclk_mem);

	clk_disable(g_mfgclk_axi);
	clk_unprepare(g_mfgclk_axi);

	clk_disable(g_mfgclk_sys);
	clk_unprepare(g_mfgclk_sys);

	return PVRSRV_OK;
}

static int MTKEnableHWAPM(void)
{
	mtk_mfg_debug("MTKEnableHWAPM...\n");
	DRV_WriteReg32(gbRegBase + 0x24, 0x003c3d4d);
	DRV_WriteReg32(gbRegBase + 0x28, 0x4d45440b);
	DRV_WriteReg32(gbRegBase + 0xe0, 0x7a710184);
	DRV_WriteReg32(gbRegBase + 0xe4, 0x835f6856);
	DRV_WriteReg32(gbRegBase + 0xe8, 0x002b0234);
	DRV_WriteReg32(gbRegBase + 0xec, 0x80000000);
	DRV_WriteReg32(gbRegBase + 0xa0, 0x08000000);
	return PVRSRV_OK;
}


static int MTKDisableHWAPM(void)
{
	mtk_mfg_debug("MTKDisableHWAPM...\n");
	return PVRSRV_OK;
}

int MTKMFGSystemInit(void)
{
	mtk_mfg_debug("[MFG]MTKMFGSystemInit Begin\n");

	bMfgInit = true;

#ifndef MTK_MFG_DVFS
	MTKInitFreqInfo(mtkBackupPVRLDMDev);
#endif

	/* MTKEnableHWAPM(); */

	mtk_mfg_debug("MTKMFGSystemInit End\n");
	return PVRSRV_OK;
}

int MTKMFGSystemDeInit(void)
{
#ifndef MTK_MFG_DVFS
	MTKDeInitFreqInfo(mtkBackupPVRLDMDev);
#endif
	return PVRSRV_OK;
}

static DEFINE_MUTEX(g_DevPreMutex);
static DEFINE_MUTEX(g_DevPostMutex);


int MTKMFGGetClocks(struct platform_device *pdev)
{
	int volt, enable;

	gbRegBase = of_iomap(pdev->dev.of_node, 1);
	if (!gbRegBase)
		mtk_mfg_debug("Unable to ioremap registers pdev %p\n", pdev);

	g_mfgclk_sys = devm_clk_get(&pdev->dev, "scp_sys_mfg");
	g_mfgclk_axi = devm_clk_get(&pdev->dev, "mfg_axi");
	g_mfgclk_mem = devm_clk_get(&pdev->dev, "mfg_mem");
	g_mfgclk_g3d = devm_clk_get(&pdev->dev, "mfg_g3d");
	g_mfgclk_26m = devm_clk_get(&pdev->dev, "mfg_26m");

	g_mmpll = devm_clk_get(&pdev->dev, "mmpll_clk");
	g_vgpu = devm_regulator_get(&pdev->dev, "mfgsys-power");
	bGetClock = true;
	mtkBackupPVRLDMDev = pdev;

	if (IS_ERR(g_mfgclk_axi) || IS_ERR(g_mfgclk_mem) ||
	    IS_ERR(g_mfgclk_g3d) || IS_ERR(g_mfgclk_26m) || IS_ERR(g_mmpll))
		bGetClock = false;

	mtk_mfg_debug("MTKMFGGetClocks[%d] clocks %p %p %p %p\n",
		      bGetClock, g_mfgclk_axi, g_mfgclk_mem,
		      g_mfgclk_g3d, g_mfgclk_26m);
	volt = regulator_get_voltage(g_vgpu);
	enable = regulator_is_enabled(g_vgpu);

	mtk_mfg_debug("g_vgpu = %p, volt = %d, enable = %d\n",
		      g_vgpu, volt, enable);

	if (enable == 0) {
		enable = regulator_enable(g_vgpu);
		if (enable != 0)
			mtk_mfg_debug("failed to enable regulator %d\n",
				      regulator_is_enabled(g_vgpu));
	}


	if (bGetClock && !bMfgInit)
		MTKMFGSystemInit();

	return (int)bGetClock;
}

static PVRSRV_DEV_POWER_STATE g_eCurrPowerState = PVRSRV_DEV_POWER_STATE_ON;
static PVRSRV_DEV_POWER_STATE g_eNewPowerState = PVRSRV_DEV_POWER_STATE_DEFAULT;

void MTKSysSetInitialPowerState(void)
{
	mtk_mfg_debug("MTKSysSetInitialPowerState ---\n");
}

void MTKSysRestoreInitialPowerState(void)
{
	mtk_mfg_debug("MTKSysRestoreInitialPowerState ---\n");
}

PVRSRV_ERROR MTKSysDevPrePowerState(PVRSRV_DEV_POWER_STATE eNewPowerState,
				    PVRSRV_DEV_POWER_STATE eCurrentPowerState,
				    IMG_BOOL bForced)
{
	mtk_mfg_debug("MTKSysDevPrePowerState (%d->%d), bForced = %d\n",
		      eCurrentPowerState, eNewPowerState, bForced);

	mutex_lock(&g_DevPreMutex);

	if (PVRSRV_DEV_POWER_STATE_OFF == eNewPowerState
	    && PVRSRV_DEV_POWER_STATE_ON == eCurrentPowerState)	{
		MTKDisableHWAPM();
		MtkDisableMfgClock();
	} else if (PVRSRV_DEV_POWER_STATE_ON == eNewPowerState
		   && PVRSRV_DEV_POWER_STATE_OFF == eCurrentPowerState) {
		MtkEnableMfgClock();
		MTKEnableHWAPM();
	} else {
		mtk_mfg_debug("MTKSysDevPrePowerState do nothing!\n");
	}

	g_eCurrPowerState = eCurrentPowerState;
	g_eNewPowerState = eNewPowerState;

	mutex_unlock(&g_DevPreMutex);
	return PVRSRV_OK;
}

PVRSRV_ERROR MTKSysDevPostPowerState(PVRSRV_DEV_POWER_STATE eNewPowerState,
				     PVRSRV_DEV_POWER_STATE eCurrentPowerState,
				     IMG_BOOL bForced)
{
	/* Post power sequence move to PrePowerState */
	return PVRSRV_OK;

	mtk_mfg_debug("MTKSysDevPostPowerState (%d->%d)\n",
		      eCurrentPowerState, eNewPowerState);

	mutex_lock(&g_DevPostMutex);

	if (PVRSRV_DEV_POWER_STATE_ON == eNewPowerState
	   && PVRSRV_DEV_POWER_STATE_OFF == eCurrentPowerState) {
		MtkEnableMfgClock();
	} else if (PVRSRV_DEV_POWER_STATE_OFF == eNewPowerState
		   && PVRSRV_DEV_POWER_STATE_ON == eCurrentPowerState) {
		MtkDisableMfgClock();
	} else {
		mtk_mfg_debug("MTKSysDevPostPowerState do nothing!\n");
	}

	mutex_unlock(&g_DevPostMutex);

	return PVRSRV_OK;
}

PVRSRV_ERROR MTKSystemPrePowerState(PVRSRV_SYS_POWER_STATE eNewPowerState)
{
	mtk_mfg_debug("MTKSystemPrePowerState eNewPowerState %d\n",
		      eNewPowerState);
	return PVRSRV_OK;
}

PVRSRV_ERROR MTKSystemPostPowerState(PVRSRV_SYS_POWER_STATE eNewPowerState)
{
	mtk_mfg_debug("MTKSystemPostPowerState eNewPowerState %d\n",
		      eNewPowerState);
	return PVRSRV_OK;
}
