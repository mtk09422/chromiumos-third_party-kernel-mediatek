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

#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/clkdev.h>
#include <linux/delay.h>

#include "clk-mtk.h"

#define CON0_BASE_EN		BIT(0)
#define CON0_PWR_ON		BIT(0)
#define CON0_ISO_EN		BIT(1)
#define CON0_PCW_CHG		BIT(31)

#define AUDPLL_TUNER_EN		BIT(31)

/*
 * MediaTek PLLs are configured through their pcw value. The pcw value describes
 * a divider in the PLL feedback loop which consists of 7 bits for the integer
 * part and the remaining bits (if present) for the fractional part. Also they
 * have a 3 bit power-of-two post divider.
 */

struct mtk_clk_pll {
	struct clk_hw	hw;
	void __iomem	*base_addr;
	void __iomem	*pd_addr;
	void __iomem	*pwr_addr;
	void __iomem	*tuner_addr;
	void __iomem	*pcw_addr;
	const struct mtk_pll_data *data;
};

static inline struct mtk_clk_pll *to_mtk_clk_pll(struct clk_hw *hw)
{
	return container_of(hw, struct mtk_clk_pll, hw);
}

static int mtk_pll_is_prepared(struct clk_hw *hw)
{
	struct mtk_clk_pll *pll = to_mtk_clk_pll(hw);

	return (readl(pll->base_addr) & CON0_BASE_EN) != 0;
}

static unsigned long __mtk_pll_recalc_rate(struct mtk_clk_pll *pll, u32 fin,
		u32 pcw, int postdiv)
{
	int pcwbits = pll->data->pcwbits;
	int pcwfbits;
	u64 vco;
	u8 c = 0;

	/* The fractional part of the PLL divider. */
	pcwfbits = pcwbits > 7 ? pcwbits - 7 : 0;

	vco = (u64)fin * pcw;

	if (pcwfbits && (vco & GENMASK(pcwfbits - 1, 0)))
		c = 1;

	vco >>= pcwfbits;

	if (c)
		++vco;

	return ((unsigned long)vco + postdiv - 1) / postdiv;
}

static void mtk_pll_set_rate_regs(struct clk_hw *hw, u32 pcw,
		int postdiv)
{
	struct mtk_clk_pll *pll = to_mtk_clk_pll(hw);
	u32 con1, pd, val;
	int pll_en;

	/* set postdiv */
	pd = readl(pll->pd_addr);
	pd &= ~(0x7 << pll->data->pd_shift);
	pd |= (ffs(postdiv) - 1) << pll->data->pd_shift;
	writel(pd, pll->pd_addr);

	pll_en = readl(pll->base_addr) & CON0_BASE_EN;

	/* set pcw */
	val = readl(pll->pcw_addr);

	val &= ~GENMASK(pll->data->pcw_shift + pll->data->pcwbits - 1,
			pll->data->pcw_shift);
	val |= pcw << pll->data->pcw_shift;
	writel(val, pll->pcw_addr);

	con1 = readl(pll->base_addr + 4);

	if (pll_en)
		con1 |= CON0_PCW_CHG;

	writel(con1, pll->base_addr + 4);
	if (pll->tuner_addr)
		writel(con1 + 1, pll->tuner_addr);

	if (pll_en)
		usleep_range(20, 1000);
}

/*
 * mtk_pll_calc_values - calculate good values for a given input frequency.
 * @pll:	The pll
 * @pcw:	The pcw value (output)
 * @postdiv:	The post divider (output)
 * @freq:	The desired target frequency
 * @fin:	The input frequency
 *
 */
static void mtk_pll_calc_values(struct mtk_clk_pll *pll, u32 *pcw, u32 *postdiv,
		u32 freq, u32 fin)
{
	unsigned long fmin = 1000 * MHZ;
	u64 _pcw;
	u32 val;

	if (freq > pll->data->fmax)
		freq = pll->data->fmax;

	for (val = 0; val < 4; val++) {
		*postdiv = 1 << val;
		if (freq * *postdiv >= fmin)
			break;
	}

	/* _pcw = freq * postdiv / fin * 2^pcwfbits */
	_pcw = ((u64)freq << val) << (pll->data->pcwbits - 7);
	do_div(_pcw, fin);

	*pcw = (u32)_pcw;
}

static int mtk_pll_set_rate(struct clk_hw *hw, unsigned long rate,
		unsigned long parent_rate)
{
	struct mtk_clk_pll *pll = to_mtk_clk_pll(hw);
	u32 pcw = 0;
	u32 postdiv;

	mtk_pll_calc_values(pll, &pcw, &postdiv, rate, parent_rate);
	mtk_pll_set_rate_regs(hw, pcw, postdiv);

	return 0;
}

static unsigned long mtk_pll_recalc_rate(struct clk_hw *hw,
		unsigned long parent_rate)
{
	struct mtk_clk_pll *pll = to_mtk_clk_pll(hw);
	u32 postdiv;
	u32 pcw;

	postdiv = (readl(pll->pd_addr) >> pll->data->pd_shift) & 0x7;
	postdiv = 1 << postdiv;

	pcw = readl(pll->pcw_addr) >> pll->data->pcw_shift;
	pcw &= GENMASK(pll->data->pcwbits - 1, 0);

	return __mtk_pll_recalc_rate(pll, parent_rate, pcw, postdiv);
}

static long mtk_pll_round_rate(struct clk_hw *hw, unsigned long rate,
		unsigned long *prate)
{
	struct mtk_clk_pll *pll = to_mtk_clk_pll(hw);
	u32 pcw = 0;
	int postdiv;

	mtk_pll_calc_values(pll, &pcw, &postdiv, rate, *prate);

	return __mtk_pll_recalc_rate(pll, *prate, pcw, postdiv);
}

static int mtk_pll_prepare(struct clk_hw *hw)
{
	struct mtk_clk_pll *pll = to_mtk_clk_pll(hw);
	u32 r;

	r = readl(pll->pwr_addr) | CON0_PWR_ON;
	writel(r, pll->pwr_addr);
	usleep_range(1, 100);

	r = readl(pll->pwr_addr) & ~CON0_ISO_EN;
	writel(r, pll->pwr_addr);
	usleep_range(1, 100);

	r = readl(pll->base_addr) | pll->data->en_mask;
	writel(r, pll->base_addr);

	if (pll->tuner_addr) {
		r = readl(pll->tuner_addr) | AUDPLL_TUNER_EN;
		writel(r, pll->tuner_addr);
	}

	usleep_range(20, 1000);

	if (pll->data->flags & HAVE_RST_BAR) {
		r = readl(pll->base_addr) | pll->data->rst_bar_mask;
		writel(r, pll->base_addr);
	}

	return 0;
}

static void mtk_pll_unprepare(struct clk_hw *hw)
{
	struct mtk_clk_pll *pll = to_mtk_clk_pll(hw);
	u32 r;

	if (pll->data->flags & HAVE_RST_BAR) {
		r = readl(pll->base_addr) & ~pll->data->rst_bar_mask;
		writel(r, pll->base_addr);
	}

	if (pll->tuner_addr) {
		r = readl(pll->tuner_addr) & ~AUDPLL_TUNER_EN;
		writel(r, pll->tuner_addr);
	}

	r = readl(pll->base_addr) & ~CON0_BASE_EN;
	writel(r, pll->base_addr);

	r = readl(pll->pwr_addr) | CON0_ISO_EN;
	writel(r, pll->pwr_addr);

	r = readl(pll->pwr_addr) & ~CON0_PWR_ON;
	writel(r, pll->pwr_addr);
}

static const struct clk_ops mtk_pll_ops = {
	.is_prepared	= mtk_pll_is_prepared,
	.prepare	= mtk_pll_prepare,
	.unprepare	= mtk_pll_unprepare,
	.recalc_rate	= mtk_pll_recalc_rate,
	.round_rate	= mtk_pll_round_rate,
	.set_rate	= mtk_pll_set_rate,
};

static struct clk *mtk_clk_register_pll(const struct mtk_pll_data *data,
		void __iomem *base)
{
	struct mtk_clk_pll *pll;
	struct clk_init_data init;
	struct clk *clk;
	const char *parent_name = "clk26m";

	pr_debug("%s(): name: %s\n", __func__, data->name);

	pll = kzalloc(sizeof(*pll), GFP_KERNEL);
	if (!pll)
		return ERR_PTR(-ENOMEM);

	pll->base_addr = base + data->reg;
	pll->pwr_addr = base + data->pwr_reg;
	pll->pd_addr = base + data->pd_reg;
	pll->pcw_addr = base + data->pcw_reg;
	if (data->tuner_reg)
		pll->tuner_addr = base + data->tuner_reg;
	pll->hw.init = &init;
	pll->data = data;

	init.name = data->name;
	init.ops = &mtk_pll_ops;
	init.parent_names = &parent_name;
	init.num_parents = 1;

	clk = clk_register(NULL, &pll->hw);

	if (IS_ERR(clk))
		kfree(pll);

	return clk;
}

void __init mtk_clk_register_plls(struct device_node *node,
		const struct mtk_pll_data *plls, int num_plls, struct clk_onecell_data *clk_data)
{
	void __iomem *base;
	int r, i;
	struct clk *clk;

	base = of_iomap(node, 0);
	if (!base) {
		pr_err("%s(): ioremap failed\n", __func__);
		return;
	}

	for (i = 0; i < num_plls; i++) {
		const struct mtk_pll_data *pll = &plls[i];

		clk = mtk_clk_register_pll(pll, base);

		if (IS_ERR(clk)) {
			pr_err("Failed to register clk %s: %ld\n",
					pll->name, PTR_ERR(clk));
			continue;
		}

		clk_data->clks[pll->id] = clk;
	}

	r = of_clk_add_provider(node, of_clk_src_onecell_get, clk_data);
	if (r)
		pr_err("%s(): could not register clock provider: %d\n",
			__func__, r);
}
