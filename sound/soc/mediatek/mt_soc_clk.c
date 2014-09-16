/*
 * mt_soc_clk.c  --  Mediatek audio clock controller
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

#include "mt_soc_common.h"
#include "mt_soc_clk.h"
#include "mt_soc_afe_control.h"
#include <linux/spinlock.h>
#include <linux/delay.h>

struct clk *infra_audio_clk;
struct clk *afe_clk;

void init_audio_clk(void *dev)
{
	infra_audio_clk = devm_clk_get(dev, "infra_audio_clk");
	BUG_ON(IS_ERR(infra_audio_clk));

	afe_clk = devm_clk_get(dev, "afe_clk");
	BUG_ON(IS_ERR(afe_clk));

	/* Use APB 3.0 */
	afe_set_reg(AUDIO_TOP_CON0, 0x00004000, 0x00004000);
}

