/*
 * mt_soc_clk.h  --  Mediatek audio clock controller
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

#ifndef _AUDDRV_CLK_H_
#define _AUDDRV_CLK_H_

#include <linux/clk.h>

/*****************************************************************************
 *                E X T E R N A L   R E F E R E N C E S
 *****************************************************************************/
extern struct clk *infra_audio_clk;
extern struct clk *afe_clk;

/*****************************************************************************
 *                         INLINE FUNCTION
 *****************************************************************************/
static inline void aud_drv_clk_on(void)
{
	if (clk_prepare_enable(infra_audio_clk))
		pr_err("%s enable_clock MT_CG_INFRA_AUDIO_PDN fail\n",
		       __func__);

	if (clk_prepare_enable(afe_clk))
		pr_err("%s enable_clock MT_CG_AUDIO_PDN_AFE fail\n",
		       __func__);
}

static inline void aud_drv_clk_off(void)
{
	clk_disable_unprepare(afe_clk);
	clk_disable_unprepare(infra_audio_clk);
}

void init_audio_clk(void *dev);
#endif
