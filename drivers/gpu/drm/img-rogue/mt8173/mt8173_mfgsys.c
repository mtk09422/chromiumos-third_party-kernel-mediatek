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
#include <linux/pm_runtime.h>

#include "pvrsrv_error.h"

#include "mt8173_mfgsys.h"
#include "mt8173_mfgdvfs.h"

struct regulator *g_vgpu;
struct clk *g_mmpll;

static struct clk *g_mfgclk_mem_in_sel;
static struct clk *g_mfgclk_axi_in_sel;
static struct clk *g_mfgclk_axi;
static struct clk *g_mfgclk_mem;
static struct clk *g_mfgclk_g3d;
static struct clk *g_mfgclk_26m;

static void __iomem *gbRegBase;
struct platform_device *mtkBackupPVRLDMDev = NULL;

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

	pm_runtime_get_sync(&mtkBackupPVRLDMDev->dev);

	clk_prepare(g_mfgclk_mem_in_sel);
	clk_enable(g_mfgclk_mem_in_sel);

	clk_prepare(g_mfgclk_axi_in_sel);
	clk_enable(g_mfgclk_axi_in_sel);

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
	clk_disable(g_mfgclk_26m);
	clk_unprepare(g_mfgclk_26m);

	clk_disable(g_mfgclk_g3d);
	clk_unprepare(g_mfgclk_g3d);

	clk_disable(g_mfgclk_mem);
	clk_unprepare(g_mfgclk_mem);

	clk_disable(g_mfgclk_axi);
	clk_unprepare(g_mfgclk_axi);

	clk_disable(g_mfgclk_axi_in_sel);
	clk_unprepare(g_mfgclk_axi_in_sel);

	clk_disable(g_mfgclk_mem_in_sel);
	clk_unprepare(g_mfgclk_mem_in_sel);

	pm_runtime_put_sync(&mtkBackupPVRLDMDev->dev);
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
	DRV_WriteReg32(gbRegBase + 0x24, 0x00000000);
	DRV_WriteReg32(gbRegBase + 0x28, 0x00000000);
	DRV_WriteReg32(gbRegBase + 0xe0, 0x00000000);
	DRV_WriteReg32(gbRegBase + 0xe4, 0x00000000);
	DRV_WriteReg32(gbRegBase + 0xe8, 0x00000000);
	DRV_WriteReg32(gbRegBase + 0xec, 0x00000000);
	DRV_WriteReg32(gbRegBase + 0xa0, 0x00000000);

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
	int volt, enable, err;

	gbRegBase = of_iomap(pdev->dev.of_node, 1);
	if (!gbRegBase) {
		mtk_mfg_debug("Unable to ioremap registers pdev %p\n", pdev);
		return -ENOMEM;
	}

	g_mfgclk_mem_in_sel = devm_clk_get(&pdev->dev, "mfg_mem_in_sel");
	if (IS_ERR(g_mfgclk_mem_in_sel)) {
		err = PTR_ERR(g_mfgclk_mem_in_sel);
		goto err_iounmap_reg_base;
	}

	g_mfgclk_axi_in_sel = devm_clk_get(&pdev->dev, "mfg_axi_in_sel");
	if (IS_ERR(g_mfgclk_axi_in_sel)) {
		err = PTR_ERR(g_mfgclk_axi_in_sel);
		goto err_clk_put_mem_in_sel;
	}

	g_mfgclk_axi = devm_clk_get(&pdev->dev, "mfg_axi");
	if (IS_ERR(g_mfgclk_axi)) {
		err = PTR_ERR(g_mfgclk_axi);
		goto err_clk_put_axi_in_sel;
	}

	g_mfgclk_mem = devm_clk_get(&pdev->dev, "mfg_mem");
	if (IS_ERR(g_mfgclk_mem)) {
		err = PTR_ERR(g_mfgclk_mem);
		goto err_clk_put_mfgclk_axi;
	}

	g_mfgclk_g3d = devm_clk_get(&pdev->dev, "mfg_g3d");
	if (IS_ERR(g_mfgclk_g3d)) {
		err = PTR_ERR(g_mfgclk_g3d);
		goto err_clk_put_mfgclk_mem;
	}

	g_mfgclk_26m = devm_clk_get(&pdev->dev, "mfg_26m");
	if (IS_ERR(g_mfgclk_26m)) {
		err = PTR_ERR(g_mfgclk_26m);
		goto err_clk_put_mfgclk_g3d;
	}

	g_mmpll = devm_clk_get(&pdev->dev, "mmpll_clk");
	if (IS_ERR(g_mmpll)) {
		err = PTR_ERR(g_mmpll);
		goto err_clk_put_mfgclk_26m;
	}

	g_vgpu = devm_regulator_get(&pdev->dev, "mfgsys-power");
	if (IS_ERR(g_vgpu)) {
		err = PTR_ERR(g_vgpu);
		goto err_clk_put_mmpll;
	}

	mtkBackupPVRLDMDev = pdev;

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

	pm_runtime_enable(&pdev->dev);

	if (!bMfgInit)
		MTKMFGSystemInit();

	return 0;

err_clk_put_mmpll:
	devm_clk_put(&pdev->dev, g_mmpll);
err_clk_put_mfgclk_26m:
	devm_clk_put(&pdev->dev, g_mfgclk_26m);
err_clk_put_mfgclk_g3d:
	devm_clk_put(&pdev->dev, g_mfgclk_g3d);
err_clk_put_mfgclk_mem:
	devm_clk_put(&pdev->dev, g_mfgclk_mem);
err_clk_put_mfgclk_axi:
	devm_clk_put(&pdev->dev, g_mfgclk_axi);
err_clk_put_axi_in_sel:
	devm_clk_put(&pdev->dev, g_mfgclk_axi_in_sel);
err_clk_put_mem_in_sel:
	devm_clk_put(&pdev->dev, g_mfgclk_mem_in_sel);
err_iounmap_reg_base:
	iounmap(gbRegBase);
	return err;
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
