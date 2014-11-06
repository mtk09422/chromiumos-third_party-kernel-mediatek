/*
 * mt_afe_clk.c  --  Mediatek audio clock controller
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

#include "mt_afe_common.h"
#include "mt_afe_clk.h"
#include "mt_afe_control.h"
#include <linux/spinlock.h>
#include <linux/delay.h>

int mt_afe_init_audio_clk(struct mt_afe_info *afe_info, void *dev)
{
	afe_info->mt_afe_infra_audio_clk = devm_clk_get(dev, "infra_audio_clk");
	if (IS_ERR(afe_info->mt_afe_infra_audio_clk))
		return PTR_ERR(afe_info->mt_afe_infra_audio_clk);

	afe_info->mt_afe_clk = devm_clk_get(dev, "afe_clk");
	if (IS_ERR(afe_info->mt_afe_clk))
		return PTR_ERR(afe_info->mt_afe_clk);

	afe_info->mt_afe_hdmi_clk = devm_clk_get(dev, "hdmi_clk");
	if (IS_ERR(afe_info->mt_afe_hdmi_clk))
		return PTR_ERR(afe_info->mt_afe_hdmi_clk);

	afe_info->mt_afe_top_apll_clk = devm_clk_get(dev, "top_apll_clk");
	if (IS_ERR(afe_info->mt_afe_top_apll_clk))
		return PTR_ERR(afe_info->mt_afe_top_apll_clk);

	afe_info->mt_afe_apll_d16 = devm_clk_get(dev, "apll_d16");
	if (IS_ERR(afe_info->mt_afe_apll_d16))
		return PTR_ERR(afe_info->mt_afe_apll_d16);

	afe_info->mt_afe_apll_d24 = devm_clk_get(dev, "apll_d24");
	if (IS_ERR(afe_info->mt_afe_apll_d24))
		return PTR_ERR(afe_info->mt_afe_apll_d24);

	afe_info->mt_afe_apll_d4 = devm_clk_get(dev, "apll_d4");
	if (IS_ERR(afe_info->mt_afe_apll_d4))
		return PTR_ERR(afe_info->mt_afe_apll_d4);

	afe_info->mt_afe_apll_d8 = devm_clk_get(dev, "apll_d8");
	if (IS_ERR(afe_info->mt_afe_apll_d8))
		return PTR_ERR(afe_info->mt_afe_apll_d8);

	afe_info->mt_afe_audpll = devm_clk_get(dev, "audpll");
	if (IS_ERR(afe_info->mt_afe_audpll))
		return PTR_ERR(afe_info->mt_afe_audpll);

	afe_info->mt_afe_apll_tuner_clk = devm_clk_get(dev, "apll_tuner_clk");
	if (IS_ERR(afe_info->mt_afe_apll_tuner_clk))
		return PTR_ERR(afe_info->mt_afe_apll_tuner_clk);

	if (clk_prepare(afe_info->mt_afe_infra_audio_clk))
		dev_err(afe_info->dev, "%s infra_audio_clk fail\n", __func__);

	if (clk_prepare(afe_info->mt_afe_clk))
		dev_err(afe_info->dev, "%s afe_clk fail\n", __func__);

	if (clk_prepare(afe_info->mt_afe_hdmi_clk))
		dev_err(afe_info->dev, "%s hdmi_clk fail\n", __func__);

	if (clk_prepare(afe_info->mt_afe_top_apll_clk))
		dev_err(afe_info->dev, "%s top_apll_clk fail\n", __func__);

	if (clk_prepare(afe_info->mt_afe_apll_d16))
		dev_err(afe_info->dev, "%s apll_d16 fail\n", __func__);

	if (clk_prepare(afe_info->mt_afe_apll_d24))
		dev_err(afe_info->dev, "%s apll_d24 fail\n", __func__);

	if (clk_prepare(afe_info->mt_afe_apll_d4))
		dev_err(afe_info->dev, "%s apll_d4 fail\n", __func__);

	if (clk_prepare(afe_info->mt_afe_apll_d8))
		dev_err(afe_info->dev, "%s apll_d8 fail\n", __func__);

	if (clk_prepare(afe_info->mt_afe_audpll))
		dev_err(afe_info->dev, "%s audpll fail\n", __func__);

	if (clk_prepare(afe_info->mt_afe_apll_tuner_clk))
		dev_err(afe_info->dev, "%s apll_tuner_clk fail\n", __func__);


	/* Use APB 3.0 */
	mt_afe_set_reg(afe_info, AUDIO_TOP_CON0, 0x00004000, 0x00004000);
	return 0;
}

int mt_afe_rm_audio_clk(struct mt_afe_info *afe_info)
{
	clk_unprepare(afe_info->mt_afe_infra_audio_clk);
	clk_unprepare(afe_info->mt_afe_clk);
	clk_unprepare(afe_info->mt_afe_hdmi_clk);
	clk_unprepare(afe_info->mt_afe_top_apll_clk);
	clk_unprepare(afe_info->mt_afe_apll_d16);
	clk_unprepare(afe_info->mt_afe_apll_d24);
	clk_unprepare(afe_info->mt_afe_apll_d4);
	clk_unprepare(afe_info->mt_afe_apll_d8);
	clk_unprepare(afe_info->mt_afe_audpll);
	clk_unprepare(afe_info->mt_afe_apll_tuner_clk);
	return 0;
}

void mt_afe_set_hdmi_clk_src(struct mt_afe_info *afe_info,
			     unsigned int fs, int apll_clk_sel)
{
	/* TBD */
	unsigned int apll_sdm_pcw = AUDPLL_SDM_PCW_98M;	/* apll tuner = sdm+1 */
	unsigned int ck_apll = 0;
	unsigned int hdmi_blk_div;
	unsigned int bit_width = 3;	/* default = 32 bits */

	hdmi_blk_div = (128 / ((bit_width + 1) * 8 * 2) / 2) - 1;
	if ((hdmi_blk_div < 0) || (hdmi_blk_div > 63))
		dev_info(afe_info->dev, "hdmi_blk_div is out of range.\n");

	ck_apll = apll_clk_sel;

	if ((fs == 44100) || (fs == 88200) || (fs == 176400))
		apll_sdm_pcw = AUDPLL_SDM_PCW_90M;

	mt_top_apll_clk_on(afe_info); /*turn on APLL clk for clk_set_parent*/
	switch (apll_clk_sel) {
	case APLL_D4:
		clk_set_parent(afe_info->mt_afe_top_apll_clk,
			       afe_info->mt_afe_apll_d4);
		break;
	case APLL_D8:
		clk_set_parent(afe_info->mt_afe_top_apll_clk,
			       afe_info->mt_afe_apll_d8);
		break;
	case APLL_D24:
		clk_set_parent(afe_info->mt_afe_top_apll_clk,
			       afe_info->mt_afe_apll_d24);
		break;
	case APLL_D16:
	default:		/* default 48k */
		/* APLL_DIV : 2048/128=16 */
		clk_set_parent(afe_info->mt_afe_top_apll_clk,
			       afe_info->mt_afe_apll_d16);
		break;
	}
	mt_top_apll_clk_off(afe_info); /*turn off APLL clk for clk_set_parent*/

	/* Set APLL source clock SDM PCW info */
	clk_set_rate(afe_info->mt_afe_audpll, apll_sdm_pcw);
	/* Set HDMI BCK DIV */
	mt_afe_set_reg(afe_info, AUDIO_TOP_CON3,
		       hdmi_blk_div << HDMI_BCK_DIV_POS,
		       ((0x1 << HDMI_BCK_DIV_LEN) - 1) << HDMI_BCK_DIV_POS);
}




