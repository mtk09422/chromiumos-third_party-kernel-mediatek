/*
 * mt_afe_clk.h  --  Mediatek audio clock controller
 *
 * Copyright (c) 2014 MediaTek Inc.
 * Author: Koro Chen <koro.chen@mediatek.com>
 *         Hidalgo Huang <hidalgo.huang@mediatek.com>
 *         Ir Lian <ir.lian@mediatek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef _MT_AFE_CLK_H_
#define _MT_AFE_CLK_H_

#include "mt_afe_common.h"
#include <linux/clk.h>

/*****************************************************************************
 *                         INLINE FUNCTION
 *****************************************************************************/
static inline void mt_afe_clk_on(struct mt_afe_info *afe_info)
{
	if (clk_enable(afe_info->mt_afe_infra_audio_clk))
		dev_err(afe_info->dev, "%s afe_infra fail\n", __func__);

	if (clk_enable(afe_info->mt_afe_clk))
		dev_err(afe_info->dev, "%s afe fail\n", __func__);
}

static inline void mt_afe_clk_off(struct mt_afe_info *afe_info)
{
	clk_disable(afe_info->mt_afe_clk);
	clk_disable(afe_info->mt_afe_infra_audio_clk);
}

static inline void mt_hdmi_clk_on(struct mt_afe_info *afe_info)
{
	if (clk_enable(afe_info->mt_afe_infra_audio_clk))
		dev_err(afe_info->dev, "%s afe_infra fail\n", __func__);

	if (clk_enable(afe_info->mt_afe_hdmi_clk))
		dev_err(afe_info->dev, "%s hdmi fail\n", __func__);
}

static inline void mt_hdmi_clk_off(struct mt_afe_info *afe_info)
{
	clk_disable(afe_info->mt_afe_hdmi_clk);
	clk_disable(afe_info->mt_afe_infra_audio_clk);
}

static inline void mt_top_apll_clk_on(struct mt_afe_info *afe_info)
{
	if (clk_enable(afe_info->mt_afe_top_apll_clk))
		dev_err(afe_info->dev, "%s fail\n", __func__);
}

static inline void mt_top_apll_clk_off(struct mt_afe_info *afe_info)
{
	clk_disable(afe_info->mt_afe_top_apll_clk);
}

static inline void mt_apll_tuner_clk_on(struct mt_afe_info *afe_info)
{
	if (clk_enable(afe_info->mt_afe_infra_audio_clk))
		dev_err(afe_info->dev, "%s afe_infra fail\n", __func__);
	if (clk_enable(afe_info->mt_afe_apll_tuner_clk))
		dev_err(afe_info->dev, "%s fail\n", __func__);
}

static inline void mt_apll_tuner_clk_off(struct mt_afe_info *afe_info)
{
	clk_disable(afe_info->mt_afe_apll_tuner_clk);
	clk_disable(afe_info->mt_afe_infra_audio_clk);
}

static inline void mt_suspend_clk_on(struct mt_afe_info *afe_info)
{
	if (clk_enable(afe_info->mt_afe_infra_audio_clk))
		dev_err(afe_info->dev, "%s afe_infra fail\n", __func__);
	if (clk_enable(afe_info->mt_afe_clk))
		dev_err(afe_info->dev, "%s afe fail\n", __func__);
	if (clk_enable(afe_info->mt_afe_hdmi_clk))
		dev_err(afe_info->dev, "%s hdmi fail\n", __func__);
	if (clk_enable(afe_info->mt_afe_apll_tuner_clk))
		dev_err(afe_info->dev, "%s fail\n", __func__);
}

static inline void mt_suspend_clk_off(struct mt_afe_info *afe_info)
{
	clk_disable(afe_info->mt_afe_apll_tuner_clk);
	clk_disable(afe_info->mt_afe_hdmi_clk);
	clk_disable(afe_info->mt_afe_clk);
	clk_disable(afe_info->mt_afe_infra_audio_clk);
}


int mt_afe_init_audio_clk(struct mt_afe_info *afe_info, void *dev);
int mt_afe_rm_audio_clk(struct mt_afe_info *afe_info);
void mt_afe_set_hdmi_clk_src(struct mt_afe_info *afe_info,
			     unsigned int fs, int apll_clk_sel);
#endif
