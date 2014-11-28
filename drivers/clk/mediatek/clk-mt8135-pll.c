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

#include <linux/io.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/clkdev.h>

#include "clk-mtk.h"
#include "clk-pll.h"
#include "clk-mt8135-pll.h"

/*
 * clk_pll
 */

#define PLL_BASE_EN	BIT(0)
#define PLL_PWR_ON	BIT(0)
#define PLL_ISO_EN	BIT(1)
#define PLL_PCW_CHG	BIT(31)
#define RST_BAR_MASK	BIT(27)
#define AUDPLL_TUNER_EN	BIT(31)

#define PLL_PREDIV_H		5
#define PLL_PREDIV_L		4
#define PLL_PREDIV_MASK		GENMASK(PLL_PREDIV_H, PLL_PREDIV_L)
#define PLL_VCODIV_L		19
#define PLL_VCODIV_MASK		BIT(19)

static const u32 pll_vcodivsel_map[2] = { 1, 2 };
static const u32 pll_prediv_map[4] = { 1, 2, 4, 4 };
static const u32 pll_posdiv_map[8] = { 1, 2, 4, 8, 16, 16, 16, 16 };
static const u32 pll_fbksel_map[4] = { 1, 2, 4, 4 };

static u32 calc_pll_vco_freq(
		u32 fin,
		u32 pcw,
		u32 vcodivsel,
		u32 prediv,
		u32 pcwfbits)
{
	/* vco = (fin * pcw * vcodivsel / prediv) >> pcwfbits; */
	u64 vco = fin;
	u8 c = 0;

	vco = vco * pcw * vcodivsel;
	do_div(vco, prediv);

	if (vco & GENMASK(pcwfbits - 1, 0))
		c = 1;
	vco >>= pcwfbits;

	if (c)
		vco++;

	return (u32)vco;
}

static u32 freq_limit(u32 freq)
{
	static const u32 freq_max = 2000 * 1000 * 1000;	/* 2000 MHz */
	static const u32 freq_min = 1000 * 1000 * 1000 / 16;	/* 1000 MHz */

	if (freq <= freq_min)
		freq = freq_min + 16;
	else if (freq > freq_max)
		freq = freq_max;

	return freq;
}

static int calc_pll_freq_cfg(
		u32 *pcw,
		u32 *postdiv_idx,
		u32 freq,
		u32 fin,
		int pcwfbits)
{
	static const u64 freq_max = 2000 * 1000 * 1000;	/* 2000 MHz */
	static const u64 freq_min = 1000 * 1000 * 1000;	/* 1000 MHz */
	static const u64 postdiv[] = { 1, 2, 4, 8, 16 };
	u64 n_info;
	u32 idx;

	pr_debug("freq: %u\n", freq);

	/* search suitable postdiv */
	for (idx = 0;
		idx < ARRAY_SIZE(postdiv) && postdiv[idx] * freq <= freq_min;
		idx++)
		;

	if (idx >= ARRAY_SIZE(postdiv))
		return -EINVAL;	/* freq is out of range (too low) */
	else if (postdiv[idx] * freq > freq_max)
		return -EINVAL;	/* freq is out of range (too high) */

	/* n_info = freq * postdiv / 26MHz * 2^pcwfbits */
	n_info = (postdiv[idx] * freq) << pcwfbits;
	do_div(n_info, fin);

	*postdiv_idx = idx;
	*pcw = (u32)n_info;

	return 0;
}

static int clk_pll_is_enabled(struct clk_hw *hw)
{
	struct mtk_clk_pll *pll = to_mtk_clk_pll(hw);

	int r = (readl_relaxed(pll->base_addr) & PLL_BASE_EN) != 0;

	pr_debug("%d: %s\n", r, __clk_get_name(hw->clk));

	return r;
}

static int clk_pll_prepare(struct clk_hw *hw)
{
	unsigned long flags = 0;
	struct mtk_clk_pll *pll = to_mtk_clk_pll(hw);
	u32 r;

	pr_debug("%s\n", __clk_get_name(hw->clk));

	mtk_clk_lock(flags);

	r = readl_relaxed(pll->pwr_addr) | PLL_PWR_ON;
	writel_relaxed(r, pll->pwr_addr);
	wmb();	/* sync write before delay */
	udelay(1);

	r = readl_relaxed(pll->pwr_addr) & ~PLL_ISO_EN;
	writel_relaxed(r , pll->pwr_addr);
	wmb();	/* sync write before delay */
	udelay(1);

	r = readl_relaxed(pll->base_addr) | pll->en_mask;
	writel_relaxed(r, pll->base_addr);
	wmb();	/* sync write before delay */
	udelay(20);

	if (pll->flags & HAVE_RST_BAR) {
		r = readl_relaxed(pll->base_addr) | RST_BAR_MASK;
		writel_relaxed(r, pll->base_addr);
	}

	mtk_clk_unlock(flags);

	return 0;
}

static void clk_pll_unprepare(struct clk_hw *hw)
{
	unsigned long flags = 0;
	struct mtk_clk_pll *pll = to_mtk_clk_pll(hw);
	u32 r;

	pr_debug("%s: PLL_AO: %d\n",
		__clk_get_name(hw->clk), !!(pll->flags & PLL_AO));

	if (pll->flags & PLL_AO)
		return;

	mtk_clk_lock(flags);

	if (pll->flags & HAVE_RST_BAR) {
		r = readl_relaxed(pll->base_addr) & ~RST_BAR_MASK;
		writel_relaxed(r, pll->base_addr);
	}

	r = readl_relaxed(pll->base_addr) & ~PLL_BASE_EN;
	writel_relaxed(r, pll->base_addr);

	r = readl_relaxed(pll->pwr_addr) | PLL_ISO_EN;
	writel_relaxed(r, pll->pwr_addr);

	r = readl_relaxed(pll->pwr_addr) & ~PLL_PWR_ON;
	writel_relaxed(r, pll->pwr_addr);

	mtk_clk_unlock(flags);
}

#define SDM_PLL_POSTDIV_H	8
#define SDM_PLL_POSTDIV_L	6
#define SDM_PLL_POSTDIV_MASK	GENMASK(SDM_PLL_POSTDIV_H, SDM_PLL_POSTDIV_L)
#define SDM_PLL_PCW_H		20
#define SDM_PLL_PCW_L		0
#define SDM_PLL_PCW_MASK	GENMASK(SDM_PLL_PCW_H, SDM_PLL_PCW_L)

static unsigned long clk_pll_recalc_rate(
		struct clk_hw *hw,
		unsigned long parent_rate)
{
	struct mtk_clk_pll *pll = to_mtk_clk_pll(hw);

	u32 con0 = readl_relaxed(pll->base_addr);
	u32 con1 = readl_relaxed(pll->base_addr + 4);

	u32 vcodivsel = (con0 & PLL_VCODIV_MASK) >> PLL_VCODIV_L;
	u32 prediv = (con0 & PLL_PREDIV_MASK) >> PLL_PREDIV_L;
	u32 posdiv = (con0 & SDM_PLL_POSTDIV_MASK) >> SDM_PLL_POSTDIV_L;
	u32 pcw = (con1 & SDM_PLL_PCW_MASK) >> SDM_PLL_PCW_L;
	u32 pcwfbits = 14;

	u32 vco_freq;
	unsigned long r;

	parent_rate = parent_rate ? parent_rate : 26000000;
	vcodivsel = pll_vcodivsel_map[vcodivsel];
	prediv = pll_prediv_map[prediv];

	vco_freq = calc_pll_vco_freq(
			parent_rate, pcw, vcodivsel, prediv, pcwfbits);
	r = vco_freq / pll_posdiv_map[posdiv];

	pr_debug("%lu: %s\n", r, __clk_get_name(hw->clk));

	return r;
}

static long clk_pll_round_rate(
		struct clk_hw *hw,
		unsigned long rate,
		unsigned long *prate)
{
	int r;
	u32 pcwfbits = 14;
	u32 pcw = 0;
	u32 postdiv = 0;

	pr_debug("%s, rate: %lu\n", __clk_get_name(hw->clk), rate);

	*prate = *prate ? *prate : 26000000;
	rate = freq_limit(rate);
	calc_pll_freq_cfg(&pcw, &postdiv, rate, *prate, pcwfbits);

	r = calc_pll_vco_freq(*prate, pcw, 1, 1, pcwfbits);
	r = r / pll_posdiv_map[postdiv];
	return r;
}

static void clk_pll_set_rate_regs(
		struct clk_hw *hw,
		u32 pcw,
		u32 postdiv_idx)
{
	unsigned long flags = 0;
	struct mtk_clk_pll *pll = to_mtk_clk_pll(hw);
	void __iomem *con0_addr = pll->base_addr;
	void __iomem *con1_addr = pll->base_addr + 4;
	u32 con0;
	u32 con1;
	u32 pll_en;

	mtk_clk_lock(flags);

	con0 = readl_relaxed(con0_addr);
	con1 = readl_relaxed(con1_addr);

	pll_en = con0 & PLL_BASE_EN;

	/* set postdiv */
	con0 &= ~SDM_PLL_POSTDIV_MASK;
	con0 |= postdiv_idx << SDM_PLL_POSTDIV_L;
	writel_relaxed(con0, con0_addr);

	/* set pcw */
	con1 &= ~SDM_PLL_PCW_MASK;
	con1 |= pcw << SDM_PLL_PCW_L;

	if (pll_en)
		con1 |= PLL_PCW_CHG;

	writel_relaxed(con1, con1_addr);

	if (pll_en) {
		wmb();	/* sync write before delay */
		udelay(20);
	}

	mtk_clk_unlock(flags);
}

static int clk_pll_set_rate(
		struct clk_hw *hw,
		unsigned long rate,
		unsigned long parent_rate)
{
	u32 pcwfbits = 14;
	u32 pcw = 0;
	u32 postdiv_idx = 0;
	int r;

	parent_rate = parent_rate ? parent_rate : 26000000;
	r = calc_pll_freq_cfg(&pcw, &postdiv_idx, rate, parent_rate, pcwfbits);

	pr_debug("%s, rate: %lu, pcw: %u, postdiv_idx: %u\n",
		__clk_get_name(hw->clk), rate, pcw, postdiv_idx);

	if (r == 0)
		clk_pll_set_rate_regs(hw, pcw, postdiv_idx);

	return r;
}

const struct clk_ops mtk_clk_pll_ops = {
	.is_enabled	= clk_pll_is_enabled,
	.prepare	= clk_pll_prepare,
	.unprepare	= clk_pll_unprepare,
	.recalc_rate	= clk_pll_recalc_rate,
	.round_rate	= clk_pll_round_rate,
	.set_rate	= clk_pll_set_rate,
};

#define ARM_PLL_POSTDIV_H	26
#define ARM_PLL_POSTDIV_L	24
#define ARM_PLL_POSTDIV_MASK	GENMASK(ARM_PLL_POSTDIV_H, ARM_PLL_POSTDIV_L)
#define ARM_PLL_PCW_H		20
#define ARM_PLL_PCW_L		0
#define ARM_PLL_PCW_MASK	GENMASK(ARM_PLL_PCW_H, ARM_PLL_PCW_L)

static unsigned long clk_arm_pll_recalc_rate(
		struct clk_hw *hw,
		unsigned long parent_rate)
{
	struct mtk_clk_pll *pll = to_mtk_clk_pll(hw);

	u32 con0 = readl_relaxed(pll->base_addr);
	u32 con1 = readl_relaxed(pll->base_addr + 4);

	u32 vcodivsel = (con0 & PLL_VCODIV_MASK) >> PLL_VCODIV_L;
	u32 prediv = (con0 & PLL_PREDIV_MASK) >> PLL_PREDIV_L;
	u32 posdiv = (con1 & ARM_PLL_POSTDIV_MASK) >> ARM_PLL_POSTDIV_L;
	u32 pcw = (con1 & ARM_PLL_PCW_MASK) >> ARM_PLL_PCW_L;
	u32 pcwfbits = 14;

	u32 vco_freq;
	unsigned long r;

	parent_rate = parent_rate ? parent_rate : 26000000;
	vcodivsel = pll_vcodivsel_map[vcodivsel];
	prediv = pll_prediv_map[prediv];

	vco_freq = calc_pll_vco_freq(
			parent_rate, pcw, vcodivsel, prediv, pcwfbits);
	r = vco_freq / pll_posdiv_map[posdiv];

	pr_debug("%lu: %s\n", r, __clk_get_name(hw->clk));

	return r;
}

static void clk_arm_pll_set_rate_regs(
		struct clk_hw *hw,
		u32 pcw,
		u32 postdiv_idx)

{
	unsigned long flags = 0;
	struct mtk_clk_pll *pll = to_mtk_clk_pll(hw);
	void __iomem *con0_addr = pll->base_addr;
	void __iomem *con1_addr = pll->base_addr + 4;
	u32 con0;
	u32 con1;
	u32 pll_en;

	mtk_clk_lock(flags);

	con0 = readl_relaxed(con0_addr);
	con1 = readl_relaxed(con1_addr);

	pll_en = con0 & PLL_BASE_EN;

	/* postdiv */
	con1 &= ~ARM_PLL_POSTDIV_MASK;
	con1 |= postdiv_idx << ARM_PLL_POSTDIV_L;

	/* pcw */
	con1 &= ~ARM_PLL_PCW_MASK;
	con1 |= pcw << ARM_PLL_PCW_L;

	if (pll_en)
		con1 |= PLL_PCW_CHG;

	writel_relaxed(con1, con1_addr);

	if (pll_en) {
		wmb();	/* sync write before delay */
		udelay(20);
	}

	mtk_clk_unlock(flags);
}

static int clk_arm_pll_set_rate(
		struct clk_hw *hw,
		unsigned long rate,
		unsigned long parent_rate)
{
	u32 pcwfbits = 14;
	u32 pcw = 0;
	u32 postdiv_idx = 0;
	int r;

	parent_rate = parent_rate ? parent_rate : 26000000;
	r = calc_pll_freq_cfg(&pcw, &postdiv_idx, rate, parent_rate, pcwfbits);

	pr_debug("%s, rate: %lu, pcw: %u, postdiv_idx: %u\n",
		__clk_get_name(hw->clk), rate, pcw, postdiv_idx);

	if (r == 0)
		clk_arm_pll_set_rate_regs(hw, pcw, postdiv_idx);

	return r;
}

const struct clk_ops mtk_clk_arm_pll_ops = {
	.is_enabled	= clk_pll_is_enabled,
	.prepare	= clk_pll_prepare,
	.unprepare	= clk_pll_unprepare,
	.recalc_rate	= clk_arm_pll_recalc_rate,
	.round_rate	= clk_pll_round_rate,
	.set_rate	= clk_arm_pll_set_rate,
};

static int clk_lc_pll_prepare(struct clk_hw *hw)
{
	unsigned long flags = 0;
	struct mtk_clk_pll *pll = to_mtk_clk_pll(hw);
	u32 r;

	pr_debug("%s\n", __clk_get_name(hw->clk));

	mtk_clk_lock(flags);

	r = readl_relaxed(pll->base_addr) | pll->en_mask;
	writel_relaxed(r, pll->base_addr);
	wmb();	/* sync write before delay */
	udelay(20);

	if (pll->flags & HAVE_RST_BAR) {
		r = readl_relaxed(pll->base_addr) | RST_BAR_MASK;
		writel_relaxed(r, pll->base_addr);
	}

	mtk_clk_unlock(flags);

	return 0;
}

static void clk_lc_pll_unprepare(struct clk_hw *hw)
{
	unsigned long flags = 0;
	struct mtk_clk_pll *pll = to_mtk_clk_pll(hw);
	u32 r;

	pr_debug("%s\n", __clk_get_name(hw->clk));

	mtk_clk_lock(flags);

	if (pll->flags & HAVE_RST_BAR) {
		r = readl_relaxed(pll->base_addr) & ~RST_BAR_MASK;
		writel_relaxed(r, pll->base_addr);
	}

	r = readl_relaxed(pll->base_addr) & ~PLL_BASE_EN;
	writel_relaxed(r, pll->base_addr);

	mtk_clk_unlock(flags);
}

#define LC_PLL_FBKSEL_H		21
#define LC_PLL_FBKSEL_L		20
#define LC_PLL_FBKSEL_MASK	GENMASK(LC_PLL_FBKSEL_H, LC_PLL_FBKSEL_L)
#define LC_PLL_POSTDIV_H	8
#define LC_PLL_POSTDIV_L	6
#define LC_PLL_POSTDIV_MASK	GENMASK(LC_PLL_POSTDIV_H, LC_PLL_POSTDIV_L)
#define LC_PLL_FBKDIV_H		15
#define LC_PLL_FBKDIV_L		9
#define LC_PLL_FBKDIV_MASK	GENMASK(LC_PLL_FBKDIV_H, LC_PLL_FBKDIV_L)

static unsigned long clk_lc_pll_recalc_rate(
		struct clk_hw *hw,
		unsigned long parent_rate)
{
	struct mtk_clk_pll *pll = to_mtk_clk_pll(hw);

	u32 con0 = readl_relaxed(pll->base_addr);

	u32 fbksel = (con0 & LC_PLL_FBKSEL_MASK) >> LC_PLL_FBKSEL_L;
	u32 vcodivsel = (con0 & PLL_VCODIV_MASK) >> PLL_VCODIV_L;
	u32 fbkdiv = (con0 & LC_PLL_FBKDIV_MASK) >> LC_PLL_FBKDIV_L;
	u32 prediv = (con0 & PLL_PREDIV_MASK) >> PLL_PREDIV_L;
	u32 posdiv = (con0 & LC_PLL_POSTDIV_MASK) >> LC_PLL_POSTDIV_L;

	u32 vco_freq;
	unsigned long r;

	parent_rate = parent_rate ? parent_rate : 26000000;
	vcodivsel = pll_vcodivsel_map[vcodivsel];
	fbksel = pll_fbksel_map[fbksel];
	prediv = pll_prediv_map[prediv];

	vco_freq = parent_rate * fbkdiv * fbksel * vcodivsel / prediv;
	r = vco_freq / pll_posdiv_map[posdiv];

	pr_debug("%lu: %s\n", r, __clk_get_name(hw->clk));

	return r;
}

static long clk_lc_pll_round_rate(
		struct clk_hw *hw,
		unsigned long rate,
		unsigned long *prate)
{
	int r;
	u32 pcwfbits = 0;
	u32 pcw = 0;
	u32 postdiv = 0;

	pr_debug("%s, rate: %lu\n", __clk_get_name(hw->clk), rate);

	*prate = *prate ? *prate : 26000000;
	rate = freq_limit(rate);
	calc_pll_freq_cfg(&pcw, &postdiv, rate, *prate, pcwfbits);

	r = calc_pll_vco_freq(*prate, pcw, 1, 1, pcwfbits);
	r = r / pll_posdiv_map[postdiv];
	return r;
}

static void clk_lc_pll_set_rate_regs(
		struct clk_hw *hw,
		u32 pcw,
		u32 postdiv_idx)
{
	unsigned long flags = 0;
	struct mtk_clk_pll *pll = to_mtk_clk_pll(hw);
	void __iomem *con0_addr = pll->base_addr;
	u32 con0;
	u32 pll_en;

	mtk_clk_lock(flags);

	con0 = readl_relaxed(con0_addr);

	pll_en = con0 & PLL_BASE_EN;

	/* postdiv */
	con0 &= ~LC_PLL_POSTDIV_MASK;
	con0 |= postdiv_idx << LC_PLL_POSTDIV_L;

	/* fkbdiv */
	con0 &= ~LC_PLL_FBKDIV_MASK;
	con0 |= pcw << LC_PLL_FBKDIV_L;

	writel_relaxed(con0, con0_addr);

	if (pll_en) {
		wmb();	/* sync write before delay */
		udelay(20);
	}
	mtk_clk_unlock(flags);
}

static int clk_lc_pll_set_rate(
		struct clk_hw *hw,
		unsigned long rate,
		unsigned long parent_rate)
{
	u32 pcwfbits = 0;
	u32 pcw = 0;
	u32 postdiv_idx = 0;
	int r;

	parent_rate = parent_rate ? parent_rate : 26000000;
	r = calc_pll_freq_cfg(&pcw, &postdiv_idx, rate, parent_rate, pcwfbits);

	pr_debug("%s, rate: %lu, pcw: %u, postdiv_idx: %u\n",
		__clk_get_name(hw->clk), rate, pcw, postdiv_idx);

	if (r == 0)
		clk_lc_pll_set_rate_regs(hw, pcw, postdiv_idx);

	return r;
}

const struct clk_ops mtk_clk_lc_pll_ops = {
	.is_enabled	= clk_pll_is_enabled,
	.prepare	= clk_lc_pll_prepare,
	.unprepare	= clk_lc_pll_unprepare,
	.recalc_rate	= clk_lc_pll_recalc_rate,
	.round_rate	= clk_lc_pll_round_rate,
	.set_rate	= clk_lc_pll_set_rate,
};

static int clk_aud_pll_prepare(struct clk_hw *hw)
{
	unsigned long flags = 0;
	struct mtk_clk_pll *pll = to_mtk_clk_pll(hw);
	u32 r;

	pr_debug("%s\n", __clk_get_name(hw->clk));

	mtk_clk_lock(flags);

	r = readl_relaxed(pll->pwr_addr) | PLL_PWR_ON;
	writel_relaxed(r, pll->pwr_addr);
	wmb();	/* sync write before delay */
	udelay(1);

	r = readl_relaxed(pll->pwr_addr) & ~PLL_ISO_EN;
	writel_relaxed(r, pll->pwr_addr);
	wmb();	/* sync write before delay */
	udelay(1);

	r = readl_relaxed(pll->base_addr) | pll->en_mask;
	writel_relaxed(r, pll->base_addr);

	r = readl_relaxed(pll->base_addr + 16) | AUDPLL_TUNER_EN;
	writel_relaxed(r, pll->base_addr + 16);
	wmb();	/* sync write before delay */
	udelay(20);

	if (pll->flags & HAVE_RST_BAR) {
		r = readl_relaxed(pll->base_addr) | RST_BAR_MASK;
		writel_relaxed(r, pll->base_addr);
	}

	mtk_clk_unlock(flags);

	return 0;
}

static void clk_aud_pll_unprepare(struct clk_hw *hw)
{
	unsigned long flags = 0;
	struct mtk_clk_pll *pll = to_mtk_clk_pll(hw);
	u32 r;

	pr_debug("%s\n", __clk_get_name(hw->clk));

	mtk_clk_lock(flags);

	if (pll->flags & HAVE_RST_BAR) {
		r = readl_relaxed(pll->base_addr) & ~RST_BAR_MASK;
		writel_relaxed(r, pll->base_addr);
	}
	r = readl_relaxed(pll->base_addr + 16) & ~AUDPLL_TUNER_EN;
	writel_relaxed(r, pll->base_addr + 16);

	r = readl_relaxed(pll->base_addr) & ~PLL_BASE_EN;
	writel_relaxed(r, pll->base_addr);

	r = readl_relaxed(pll->pwr_addr) | PLL_ISO_EN;
	writel_relaxed(r, pll->pwr_addr);

	r = readl_relaxed(pll->pwr_addr) & ~PLL_PWR_ON;
	writel_relaxed(r, pll->pwr_addr);

	mtk_clk_unlock(flags);
}

#define AUD_PLL_POSTDIV_H	8
#define AUD_PLL_POSTDIV_L	6
#define AUD_PLL_POSTDIV_MASK	GENMASK(AUD_PLL_POSTDIV_H, AUD_PLL_POSTDIV_L)
#define AUD_PLL_PCW_H		30
#define AUD_PLL_PCW_L		0
#define AUD_PLL_PCW_MASK	GENMASK(AUD_PLL_PCW_H, AUD_PLL_PCW_L)

static unsigned long clk_aud_pll_recalc_rate(
		struct clk_hw *hw,
		unsigned long parent_rate)
{
	struct mtk_clk_pll *pll = to_mtk_clk_pll(hw);

	u32 con0 = readl_relaxed(pll->base_addr);
	u32 con1 = readl_relaxed(pll->base_addr + 4);

	u32 vcodivsel = (con0 & PLL_VCODIV_MASK) >> PLL_VCODIV_L;
	u32 prediv = (con0 & PLL_PREDIV_MASK) >> PLL_PREDIV_L;
	u32 posdiv = (con0 & AUD_PLL_POSTDIV_MASK) >> AUD_PLL_POSTDIV_L;
	u32 pcw = (con1 & AUD_PLL_PCW_MASK) >> AUD_PLL_PCW_L;
	u32 pcwfbits = 24;

	u32 vco_freq;
	unsigned long r;

	parent_rate = parent_rate ? parent_rate : 26000000;
	vcodivsel = pll_vcodivsel_map[vcodivsel];
	prediv = pll_prediv_map[prediv];

	vco_freq = calc_pll_vco_freq(
			parent_rate, pcw, vcodivsel, prediv, pcwfbits);
	r = vco_freq / pll_posdiv_map[posdiv];

	pr_debug("%lu: %s\n", r, __clk_get_name(hw->clk));

	return r;
}

static long clk_aud_pll_round_rate(
		struct clk_hw *hw,
		unsigned long rate,
		unsigned long *prate)
{
	int r;
	u32 pcwfbits = 24;
	u32 pcw = 0;
	u32 postdiv = 0;

	pr_debug("%s, rate: %lu\n", __clk_get_name(hw->clk), rate);

	*prate = *prate ? *prate : 26000000;
	rate = freq_limit(rate);
	calc_pll_freq_cfg(&pcw, &postdiv, rate, *prate, pcwfbits);

	r = calc_pll_vco_freq(*prate, pcw, 1, 1, pcwfbits);
	r = r / pll_posdiv_map[postdiv];
	return r;
}

static void clk_aud_pll_set_rate_regs(
		struct clk_hw *hw,
		u32 pcw,
		u32 postdiv_idx)
{
	unsigned long flags = 0;
	struct mtk_clk_pll *pll = to_mtk_clk_pll(hw);
	void __iomem *con0_addr = pll->base_addr;
	void __iomem *con1_addr = pll->base_addr + 4;
	void __iomem *con4_addr = pll->base_addr + 16;
	u32 con0;
	u32 con1;
	u32 pll_en;

	mtk_clk_lock(flags);

	con0 = readl_relaxed(con0_addr);
	con1 = readl_relaxed(con1_addr);

	pll_en = con0 & PLL_BASE_EN;

	/* set postdiv */
	con0 &= ~AUD_PLL_POSTDIV_MASK;
	con0 |= postdiv_idx << AUD_PLL_POSTDIV_L;
	writel_relaxed(con0, con0_addr);

	/* set pcw */
	con1 &= ~AUD_PLL_PCW_MASK;
	con1 |= pcw << AUD_PLL_PCW_L;

	if (pll_en)
		con1 |= PLL_PCW_CHG;

	writel_relaxed(con1, con1_addr);
	writel_relaxed(con1 + 1, con4_addr);
	/* AUDPLL_CON4[30:0] (AUDPLL_TUNER_N_INFO) = (pcw + 1) */

	if (pll_en) {
		wmb();	/* sync write before delay */
		udelay(20);
	}

	mtk_clk_unlock(flags);
}

static int clk_aud_pll_set_rate(
		struct clk_hw *hw,
		unsigned long rate,
		unsigned long parent_rate)
{
	u32 pcwfbits = 24;
	u32 pcw = 0;
	u32 postdiv_idx = 0;
	int r;

	parent_rate = parent_rate ? parent_rate : 26000000;
	r = calc_pll_freq_cfg(&pcw, &postdiv_idx, rate, parent_rate, pcwfbits);

	pr_debug("%s, rate: %lu, pcw: %u, postdiv_idx: %u\n",
		__clk_get_name(hw->clk), rate, pcw, postdiv_idx);

	if (r == 0)
		clk_aud_pll_set_rate_regs(hw, pcw, postdiv_idx);

	return r;
}

const struct clk_ops mtk_clk_aud_pll_ops = {
	.is_enabled	= clk_pll_is_enabled,
	.prepare	= clk_aud_pll_prepare,
	.unprepare	= clk_aud_pll_unprepare,
	.recalc_rate	= clk_aud_pll_recalc_rate,
	.round_rate	= clk_aud_pll_round_rate,
	.set_rate	= clk_aud_pll_set_rate,
};

#define TVD_PLL_POSTDIV_H	8
#define TVD_PLL_POSTDIV_L	6
#define TVD_PLL_POSTDIV_MASK	GENMASK(TVD_PLL_POSTDIV_H, TVD_PLL_POSTDIV_L)
#define TVD_PLL_PCW_H		30
#define TVD_PLL_PCW_L		0
#define TVD_PLL_PCW_MASK	GENMASK(TVD_PLL_PCW_H, TVD_PLL_PCW_L)

static void clk_tvd_pll_set_rate_regs(
		struct clk_hw *hw,
		u32 pcw,
		u32 postdiv_idx)
{
	unsigned long flags = 0;
	struct mtk_clk_pll *pll = to_mtk_clk_pll(hw);
	void __iomem *con0_addr = pll->base_addr;
	void __iomem *con1_addr = pll->base_addr + 4;
	u32 con0;
	u32 con1;
	u32 pll_en;

	mtk_clk_lock(flags);

	con0 = readl_relaxed(con0_addr);
	con1 = readl_relaxed(con1_addr);

	pll_en = con0 & PLL_BASE_EN;

	/* set postdiv */
	con0 &= ~TVD_PLL_POSTDIV_MASK;
	con0 |= postdiv_idx << TVD_PLL_POSTDIV_L;
	writel_relaxed(con0, con0_addr);

	/* set pcw */
	con1 &= ~TVD_PLL_PCW_MASK;
	con1 |= pcw << TVD_PLL_PCW_L;

	if (pll_en)
		con1 |= PLL_PCW_CHG;

	writel_relaxed(con1, con1_addr);

	if (pll_en) {
		wmb();	/* sync write before delay */
		udelay(20);
	}

	mtk_clk_unlock(flags);
}

static int clk_tvd_pll_set_rate(
		struct clk_hw *hw,
		unsigned long rate,
		unsigned long parent_rate)
{
	u32 pcwfbits = 24;
	u32 pcw = 0;
	u32 postdiv_idx = 0;
	int r;

	parent_rate = parent_rate ? parent_rate : 26000000;
	r = calc_pll_freq_cfg(&pcw, &postdiv_idx, rate, parent_rate, pcwfbits);

	pr_debug("%s, rate: %lu, pcw: %u, postdiv_idx: %u\n",
		__clk_get_name(hw->clk), rate, pcw, postdiv_idx);

	if (r == 0)
		clk_tvd_pll_set_rate_regs(hw, pcw, postdiv_idx);

	return r;
}

const struct clk_ops mtk_clk_tvd_pll_ops = {
	.is_enabled	= clk_pll_is_enabled,
	.prepare	= clk_pll_prepare,
	.unprepare	= clk_pll_unprepare,
	.recalc_rate	= clk_aud_pll_recalc_rate,
	.round_rate	= clk_aud_pll_round_rate,
	.set_rate	= clk_tvd_pll_set_rate,
};
