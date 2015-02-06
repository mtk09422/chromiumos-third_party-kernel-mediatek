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
#include <linux/clk.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/regulator/consumer.h>

#include <linux/io.h>


#include "mt8173_mfgsys.h"
#include "mt8173_mfgdvfs.h"

#include "pvrsrv_error.h"
#include "allocmem.h"
#include "osfunc.h"
#include "pvrsrv.h"
#include "power.h"


/* MT8173 GPU Frequency Table */
static struct mt_gpufreq_table_info mt_gpufreqs[] = {
	GPUOP_FREQ_TBL(GPU_DVFS_FREQ1, GPU_DVFS_VOLT1, 0),
	GPUOP_FREQ_TBL(GPU_DVFS_FREQ2, GPU_DVFS_VOLT1, 1),
	GPUOP_FREQ_TBL(GPU_DVFS_FREQ3, GPU_DVFS_VOLT2, 2),
	GPUOP_FREQ_TBL(GPU_DVFS_FREQ4, GPU_DVFS_VOLT2, 3),
	GPUOP_FREQ_TBL(GPU_DVFS_FREQ5, GPU_DVFS_VOLT2, 4),
	GPUOP_FREQ_TBL(GPU_DVFS_FREQ6, GPU_DVFS_VOLT2, 5),
};

static unsigned int g_cur_gpu_freq = GPU_DVFS_FREQ3;
static unsigned int g_cur_gpu_volt = GPU_DVFS_VOLT2;
static unsigned int g_cur_gpu_idx  = 0xFF;

/*
static bool         mt_gpufreq_fixed_freq_volt_state = false;
static unsigned int mt_gpufreq_fixed_frequency;
static unsigned int mt_gpufreq_fixed_voltage;
*/

static unsigned int g_gpufreq_volt_enable_state;
static DEFINE_MUTEX(mt_gpufreq_lock);


/* TODO : ADD DVFS in drivers\devfreq\mediatek for gpu vol and freq settting*/
static void _mtk_dfs_mmpll(unsigned int freq_new)
{
	unsigned int err;
	/* mmpll setting is on mt_freghopping.c  */
	mtk_mfg_debug("_mtk_dfs_mmpll set freq_new = %d\n", freq_new);

	err = clk_set_rate(g_mmpll, freq_new * 1000);
	if (err)
		mtk_mfg_debug("_mtk_dfs_mmpll err %d\n", err);

	udelay(100);
	mtk_mfg_debug("_mtk_dfs_mmpll freq = %ld\n", clk_get_rate(g_mmpll));
}

static void _mtk_gpu_clock_switch(unsigned int freq_new)
{
	if (freq_new == g_cur_gpu_freq)
		return;

	switch (freq_new) {
	case GPU_DVFS_FREQ1:
	case GPU_DVFS_FREQ2:
	case GPU_DVFS_FREQ3:
	case GPU_DVFS_FREQ4:
	case GPU_DVFS_FREQ5:
	case GPU_DVFS_FREQ6:
		_mtk_dfs_mmpll(freq_new*4);
		break;

	default:
		mtk_mfg_debug("GPU_DVFS: new freq not valid: %d\n", freq_new);
		break;
	}

	g_cur_gpu_freq = freq_new;

	mtk_mfg_debug("_mtk_gpu_clock_switch, freq_new = %d\n", freq_new);
}

static void _mtk_gpu_volt_switch(unsigned int volt_old, unsigned int volt_new)
{
	int ret;

	if (volt_new == g_cur_gpu_volt)
		return;
	ret = regulator_set_voltage(g_vgpu, GPU_VOLT_TO_EXTBUCK_VAL(volt_new),
				    GPU_VOLT_TO_EXTBUCK_MAXVAL(volt_new));
	if (ret != 0) {
		mtk_mfg_debug("_mtk_gpu_volt_switch: fail to set 0x%X (%d -> %d)\n",
			      ret, volt_old, volt_new);
		return;
	}

	udelay(GPU_DVFS_VOLT_SETTLE_TIME(volt_old, volt_new));

	g_cur_gpu_volt = volt_new;

	mtk_mfg_debug("_mtk_gpu_volt_switch, volt_new = %d\n", volt_new);
}


static void _mtk_gpufreq_set_initial(void)
{
	int index = GPU_DVFS_INIT_LVL, volt , freq;

	volt = regulator_get_voltage(g_vgpu);
	freq = clk_get_rate(g_mmpll);
	mtk_mfg_debug("Before _mtk_gpufreq_set_initial[freq, volt] = [%d, %d]\n",
		      volt, freq);

	mutex_lock(&mt_gpufreq_lock);

	g_cur_gpu_freq = freq / 1000;
	g_cur_gpu_volt = volt / 1000;

	_mtk_gpu_volt_switch(volt / 1000, mt_gpufreqs[index].gpufreq_volt);
	_mtk_gpu_clock_switch(mt_gpufreqs[index].gpufreq_khz);
	g_cur_gpu_idx = mt_gpufreqs[index].gpufreq_idx;

	mutex_unlock(&mt_gpufreq_lock);

	volt = regulator_get_voltage(g_vgpu);
	freq = clk_get_rate(g_mmpll);
	mtk_mfg_debug("After _mtk_gpufreq_set_initial[freq, volt]=[%d, %d]\n",
		      volt, freq);

	pr_info("[MFG] Set GPU Initial Freq. : freq = %d, volt = %d\n",
		g_cur_gpu_freq, g_cur_gpu_volt);
}


IMG_INT32 MTKInitFreqInfo(struct platform_device *dev)
{
	int ret;

	if (dev == NULL)
		return PVRSRV_ERROR_INVALID_PARAMS;

	ret = regulator_enable(g_vgpu);
	if (ret != 0) {
		pr_err("GPU_DVFS: Failed to enable reg-vgpu: %d\n", ret);
		return ret;
	}
	g_gpufreq_volt_enable_state = 1;
	_mtk_gpufreq_set_initial();
	return PVRSRV_OK;
}

void MTKDeInitFreqInfo(struct platform_device *dev)
{
	if (dev == NULL)
		return;
	mtk_mfg_debug("MTKDeInitFreqInfo\n");
}
