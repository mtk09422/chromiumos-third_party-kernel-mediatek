/*
 * Copyright (c) 2014 MediaTek Inc.
 * Author: James Liao <jamesjj.liao@mediatek.com>
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

#ifndef __DRV_CLK_MT8135_PLL_H
#define __DRV_CLK_MT8135_PLL_H

/*
 * This is a private header file. DO NOT include it except clk-*.c.
 */

extern const struct clk_ops mtk_clk_pll_ops;
extern const struct clk_ops mtk_clk_arm_pll_ops;
extern const struct clk_ops mtk_clk_lc_pll_ops;
extern const struct clk_ops mtk_clk_aud_pll_ops;
extern const struct clk_ops mtk_clk_tvd_pll_ops;

#endif /* __DRV_CLK_MT8135_PLL_H */
