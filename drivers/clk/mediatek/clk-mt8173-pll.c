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
#include "clk-mt8173-pll.h"

#ifndef GENMASK
#define GENMASK(h, l)	(((U32_C(1) << ((h) - (l) + 1)) - 1) << (l))
#endif

#define clk_readl(addr)		readl(addr)
#define clk_writel(val, addr)	\
	do { writel(val, addr); wmb(); } while (0)	/* sync_write */
#define clk_setl(mask, addr)	clk_writel(clk_readl(addr) | (mask), addr)
#define clk_clrl(mask, addr)	clk_writel(clk_readl(addr) & ~(mask), addr)

/*
 * clk_pll
 */

#define PLL_BASE_EN	BIT(0)
#define PLL_PWR_ON	BIT(0)
#define PLL_ISO_EN	BIT(1)
#define PLL_PCW_CHG	BIT(31)
#define RST_BAR_MASK	BIT(24)
#define AUDPLL_TUNER_EN	BIT(31)

static const uint32_t pll_posdiv_map[8] = { 1, 2, 4, 8, 16, 16, 16, 16 };

static uint32_t calc_pll_vco_freq(
		uint32_t fin,
		uint32_t pcw,
		uint32_t vcodivsel,
		uint32_t prediv,
		uint32_t pcwfbits)
{
	/* vco = (fin * pcw * vcodivsel / prediv) >> pcwfbits; */
	uint64_t vco = fin;
	uint8_t c;

	vco = vco * pcw * vcodivsel;
	do_div(vco, prediv);

	if (vco & GENMASK(pcwfbits - 1, 0))
		c = 1;

	vco >>= pcwfbits;

	if (c)
		++vco;

	return (uint32_t)vco;
}

static int calc_pll_freq_cfg(
		uint32_t *pcw,
		uint32_t *postdiv_idx,
		uint32_t freq,
		uint32_t fin,
		int pcwfbits)
{
	static const uint64_t freq_max = 3000LU * 1000 * 1000;	/* 3000 MHz */
	static const uint64_t freq_min = 1000 * 1000 * 1000;	/* 1000 MHz */
	static const uint64_t postdiv[] = { 1, 2, 4, 8, 16 };
	uint64_t n_info;
	uint32_t idx;

	pr_debug("freq: %u\n", freq);

	/* search suitable postdiv */
	for (idx = *postdiv_idx;
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
	*pcw = (uint32_t)n_info;

	return 0;
}

static int clk_pll_is_enabled(struct clk_hw *hw)
{
	struct mtk_clk_pll *pll = to_mtk_clk_pll(hw);

	int r = (clk_readl(pll->base_addr) & PLL_BASE_EN) != 0;

	pr_debug("%d: %s\n", r, __clk_get_name(hw->clk));

	return r;
}

static int clk_pll_enable(struct clk_hw *hw)
{
	unsigned long flags = 0;
	struct mtk_clk_pll *pll = to_mtk_clk_pll(hw);

	pr_debug("%s\n", __clk_get_name(hw->clk));

	mtk_clk_lock(flags);

	clk_setl(PLL_PWR_ON, pll->pwr_addr);
	udelay(1);
	clk_clrl(PLL_ISO_EN, pll->pwr_addr);
	udelay(1);

	clk_setl(pll->en_mask, pll->base_addr);
	udelay(20);

	if (pll->flags & HAVE_RST_BAR)
		clk_setl(RST_BAR_MASK, pll->base_addr);

	mtk_clk_unlock(flags);

	return 0;
}

static void clk_pll_disable(struct clk_hw *hw)
{
	unsigned long flags = 0;
	struct mtk_clk_pll *pll = to_mtk_clk_pll(hw);

	pr_debug("%s: PLL_AO: %d\n",
		__clk_get_name(hw->clk), !!(pll->flags & PLL_AO));

	if (pll->flags & PLL_AO)
		return;

	mtk_clk_lock(flags);

	if (pll->flags & HAVE_RST_BAR)
		clk_clrl(RST_BAR_MASK, pll->base_addr);

	clk_clrl(PLL_BASE_EN, pll->base_addr);

	clk_setl(PLL_ISO_EN, pll->pwr_addr);
	clk_clrl(PLL_PWR_ON, pll->pwr_addr);

	mtk_clk_unlock(flags);
}

static long clk_pll_round_rate(
		struct clk_hw *hw,
		unsigned long rate,
		unsigned long *prate)
{
	pr_debug("%s, rate: %lu\n", __clk_get_name(hw->clk), rate);

	return rate;
}

#define SDM_PLL_POSTDIV_H	6
#define SDM_PLL_POSTDIV_L	4
#define SDM_PLL_POSTDIV_MASK	GENMASK(SDM_PLL_POSTDIV_H, SDM_PLL_POSTDIV_L)
#define SDM_PLL_PCW_H		20
#define SDM_PLL_PCW_L		0
#define SDM_PLL_PCW_MASK	GENMASK(SDM_PLL_PCW_H, SDM_PLL_PCW_L)

static unsigned long clk_sdm_pll_recalc_rate(
		struct clk_hw *hw,
		unsigned long parent_rate)
{
	struct mtk_clk_pll *pll = to_mtk_clk_pll(hw);

	uint32_t con0 = clk_readl(pll->base_addr);
	uint32_t con1 = clk_readl(pll->base_addr + 4);

	uint32_t posdiv = (con0 & SDM_PLL_POSTDIV_MASK) >> SDM_PLL_POSTDIV_L;
	uint32_t pcw = (con1 & SDM_PLL_PCW_MASK) >> SDM_PLL_PCW_L;
	uint32_t pcwfbits = 14;

	uint32_t vco_freq;
	unsigned long r;

	parent_rate = parent_rate ? parent_rate : 26000000;

	vco_freq = calc_pll_vco_freq(
			parent_rate, pcw, 1, 1, pcwfbits);
	r = vco_freq / pll_posdiv_map[posdiv];

	pr_debug("%lu: %s\n", r, __clk_get_name(hw->clk));

	return r;
}

static void clk_sdm_pll_set_rate_regs(
		struct clk_hw *hw,
		uint32_t pcw,
		uint32_t postdiv_idx)
{
	unsigned long flags = 0;
	struct mtk_clk_pll *pll = to_mtk_clk_pll(hw);
	void __iomem *con0_addr = pll->base_addr;
	void __iomem *con1_addr = pll->base_addr + 4;
	uint32_t con0;
	uint32_t con1;
	uint32_t pll_en;

	mtk_clk_lock(flags);

	con0 = clk_readl(con0_addr);
	con1 = clk_readl(con1_addr);

	pll_en = con0 & PLL_BASE_EN;

	/* set postdiv */
	con0 &= ~SDM_PLL_POSTDIV_MASK;
	con0 |= postdiv_idx << SDM_PLL_POSTDIV_L;
	clk_writel(con0, con0_addr);

	/* set pcw */
	con1 &= ~SDM_PLL_PCW_MASK;
	con1 |= pcw << SDM_PLL_PCW_L;

	if (pll_en)
		con1 |= PLL_PCW_CHG;

	clk_writel(con1, con1_addr);

	if (pll_en)
		udelay(20);

	mtk_clk_unlock(flags);
}

static int clk_sdm_pll_set_rate(
		struct clk_hw *hw,
		unsigned long rate,
		unsigned long parent_rate)
{
	uint32_t pcwfbits = 14;
	uint32_t pcw = 0;
	uint32_t postdiv_idx = 0;
	int r;

	parent_rate = parent_rate ? parent_rate : 26000000;
	r = calc_pll_freq_cfg(&pcw, &postdiv_idx, rate, parent_rate, pcwfbits);

	pr_debug("%s, rate: %lu, pcw: %u, postdiv_idx: %u\n",
		__clk_get_name(hw->clk), rate, pcw, postdiv_idx);

	if (r == 0)
		clk_sdm_pll_set_rate_regs(hw, pcw, postdiv_idx);

	return r;
}

const struct clk_ops mt_clk_sdm_pll_ops = {
	.is_enabled	= clk_pll_is_enabled,
	.enable		= clk_pll_enable,
	.disable	= clk_pll_disable,
	.recalc_rate	= clk_sdm_pll_recalc_rate,
	.round_rate	= clk_pll_round_rate,
	.set_rate	= clk_sdm_pll_set_rate,
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

	uint32_t con1 = clk_readl(pll->base_addr + 4);

	uint32_t posdiv = (con1 & ARM_PLL_POSTDIV_MASK) >> ARM_PLL_POSTDIV_L;
	uint32_t pcw = (con1 & ARM_PLL_PCW_MASK) >> ARM_PLL_PCW_L;
	uint32_t pcwfbits = 14;

	uint32_t vco_freq;
	unsigned long r;

	parent_rate = parent_rate ? parent_rate : 26000000;

	vco_freq = calc_pll_vco_freq(
			parent_rate, pcw, 1, 1, pcwfbits);
	r = vco_freq / pll_posdiv_map[posdiv];

	pr_debug("%lu: %s\n", r, __clk_get_name(hw->clk));

	return r;
}

static void clk_arm_pll_set_rate_regs(
		struct clk_hw *hw,
		uint32_t pcw,
		uint32_t postdiv_idx)

{
	unsigned long flags = 0;
	struct mtk_clk_pll *pll = to_mtk_clk_pll(hw);
	void __iomem *con0_addr = pll->base_addr;
	void __iomem *con1_addr = pll->base_addr + 4;
	uint32_t con0;
	uint32_t con1;
	uint32_t pll_en;

	mtk_clk_lock(flags);

	con0 = clk_readl(con0_addr);
	con1 = clk_readl(con1_addr);

	pll_en = con0 & PLL_BASE_EN;

	/* postdiv */
	con1 &= ~ARM_PLL_POSTDIV_MASK;
	con1 |= postdiv_idx << ARM_PLL_POSTDIV_L;

	/* pcw */
	con1 &= ~ARM_PLL_PCW_MASK;
	con1 |= pcw << ARM_PLL_PCW_L;

	if (pll_en)
		con1 |= PLL_PCW_CHG;

	clk_writel(con1, con1_addr);

	if (pll_en)
		udelay(20);

	mtk_clk_unlock(flags);
}

static int clk_arm_pll_set_rate(
		struct clk_hw *hw,
		unsigned long rate,
		unsigned long parent_rate)
{
	uint32_t pcwfbits = 14;
	uint32_t pcw = 0;
	uint32_t postdiv_idx = 0;
	int r;

	parent_rate = parent_rate ? parent_rate : 26000000;
	r = calc_pll_freq_cfg(&pcw, &postdiv_idx, rate, parent_rate, pcwfbits);

	pr_debug("%s, rate: %lu, pcw: %u, postdiv_idx: %u\n",
		__clk_get_name(hw->clk), rate, pcw, postdiv_idx);

	if (r == 0)
		clk_arm_pll_set_rate_regs(hw, pcw, postdiv_idx);

	return r;
}

const struct clk_ops mt_clk_arm_pll_ops = {
	.is_enabled	= clk_pll_is_enabled,
	.enable		= clk_pll_enable,
	.disable	= clk_pll_disable,
	.recalc_rate	= clk_arm_pll_recalc_rate,
	.round_rate	= clk_pll_round_rate,
	.set_rate	= clk_arm_pll_set_rate,
};

static int clk_mm_pll_set_rate(
		struct clk_hw *hw,
		unsigned long rate,
		unsigned long parent_rate)
{
	uint32_t pcwfbits = 14;
	uint32_t pcw = 0;
	uint32_t postdiv_idx = 0;
	int r;

	if (rate <= 702000000)
		postdiv_idx = 2;

	parent_rate = parent_rate ? parent_rate : 26000000;
	r = calc_pll_freq_cfg(&pcw, &postdiv_idx, rate, parent_rate, pcwfbits);

	pr_debug("%s, rate: %lu, pcw: %u, postdiv_idx: %u\n",
		__clk_get_name(hw->clk), rate, pcw, postdiv_idx);

	if (r == 0)
		clk_arm_pll_set_rate_regs(hw, pcw, postdiv_idx);

	return r;
}

const struct clk_ops mt_clk_mm_pll_ops = {
	.is_enabled	= clk_pll_is_enabled,
	.enable		= clk_pll_enable,
	.disable	= clk_pll_disable,
	.recalc_rate	= clk_arm_pll_recalc_rate,
	.round_rate	= clk_pll_round_rate,
	.set_rate	= clk_mm_pll_set_rate,
};

static int clk_univ_pll_enable(struct clk_hw *hw)
{
	unsigned long flags = 0;
	struct mtk_clk_pll *pll = to_mtk_clk_pll(hw);

	pr_debug("%s\n", __clk_get_name(hw->clk));

	mtk_clk_lock(flags);

	clk_setl(pll->en_mask, pll->base_addr);
	udelay(20);

	if (pll->flags & HAVE_RST_BAR)
		clk_setl(RST_BAR_MASK, pll->base_addr);

	mtk_clk_unlock(flags);

	return 0;
}

static void clk_univ_pll_disable(struct clk_hw *hw)
{
	unsigned long flags = 0;
	struct mtk_clk_pll *pll = to_mtk_clk_pll(hw);

	pr_debug("%s: PLL_AO: %d\n",
		__clk_get_name(hw->clk), !!(pll->flags & PLL_AO));

	if (pll->flags & PLL_AO)
		return;

	mtk_clk_lock(flags);

	if (pll->flags & HAVE_RST_BAR)
		clk_clrl(RST_BAR_MASK, pll->base_addr);

	clk_clrl(PLL_BASE_EN, pll->base_addr);

	mtk_clk_unlock(flags);
}

#define UNIV_PLL_POSTDIV_H	6
#define UNIV_PLL_POSTDIV_L	4
#define UNIV_PLL_POSTDIV_MASK	GENMASK(UNIV_PLL_POSTDIV_H, UNIV_PLL_POSTDIV_L)
#define UNIV_PLL_FBKDIV_H	20
#define UNIV_PLL_FBKDIV_L	14
#define UNIV_PLL_FBKDIV_MASK	GENMASK(UNIV_PLL_FBKDIV_H, UNIV_PLL_FBKDIV_L)

static unsigned long clk_univ_pll_recalc_rate(
		struct clk_hw *hw,
		unsigned long parent_rate)
{
	struct mtk_clk_pll *pll = to_mtk_clk_pll(hw);

	uint32_t con0 = clk_readl(pll->base_addr);
	uint32_t con1 = clk_readl(pll->base_addr + 4);

	uint32_t fbkdiv = (con1 & UNIV_PLL_FBKDIV_MASK) >> UNIV_PLL_FBKDIV_L;
	uint32_t posdiv = (con0 & UNIV_PLL_POSTDIV_MASK) >> UNIV_PLL_POSTDIV_L;

	uint32_t vco_freq;
	unsigned long r;

	parent_rate = parent_rate ? parent_rate : 26000000;

	vco_freq = parent_rate * fbkdiv;
	r = vco_freq / pll_posdiv_map[posdiv];

	pr_debug("%lu: %s\n", r, __clk_get_name(hw->clk));

	return r;
}

static void clk_univ_pll_set_rate_regs(
		struct clk_hw *hw,
		uint32_t pcw,
		uint32_t postdiv_idx)
{
	unsigned long flags = 0;
	struct mtk_clk_pll *pll = to_mtk_clk_pll(hw);
	void __iomem *con0_addr = pll->base_addr;
	uint32_t con0;
	uint32_t pll_en;

	mtk_clk_lock(flags);

	con0 = clk_readl(con0_addr);

	pll_en = con0 & PLL_BASE_EN;

	/* postdiv */
	con0 &= ~UNIV_PLL_POSTDIV_MASK;
	con0 |= postdiv_idx << UNIV_PLL_POSTDIV_L;

	/* fkbdiv */
	con0 &= ~UNIV_PLL_FBKDIV_MASK;
	con0 |= pcw << UNIV_PLL_FBKDIV_L;

	clk_writel(con0, con0_addr);

	if (pll_en)
		udelay(20);

	mtk_clk_unlock(flags);
}

static int clk_univ_pll_set_rate(
		struct clk_hw *hw,
		unsigned long rate,
		unsigned long parent_rate)
{
	uint32_t pcwfbits = 0;
	uint32_t pcw = 0;
	uint32_t postdiv_idx = 0;
	int r;

	parent_rate = parent_rate ? parent_rate : 26000000;
	r = calc_pll_freq_cfg(&pcw, &postdiv_idx, rate, parent_rate, pcwfbits);

	pr_debug("%s, rate: %lu, pcw: %u, postdiv_idx: %u\n",
		__clk_get_name(hw->clk), rate, pcw, postdiv_idx);

	if (r == 0)
		clk_univ_pll_set_rate_regs(hw, pcw, postdiv_idx);

	return r;
}

const struct clk_ops mt_clk_univ_pll_ops = {
	.is_enabled	= clk_pll_is_enabled,
	.enable		= clk_univ_pll_enable,
	.disable	= clk_univ_pll_disable,
	.recalc_rate	= clk_univ_pll_recalc_rate,
	.round_rate	= clk_pll_round_rate,
	.set_rate	= clk_univ_pll_set_rate,
};

static int clk_aud_pll_enable(struct clk_hw *hw)
{
	unsigned long flags = 0;
	struct mtk_clk_pll *pll = to_mtk_clk_pll(hw);

	pr_debug("%s\n", __clk_get_name(hw->clk));

	mtk_clk_lock(flags);

	clk_setl(PLL_PWR_ON, pll->pwr_addr);
	udelay(1);
	clk_clrl(PLL_ISO_EN, pll->pwr_addr);
	udelay(1);

	clk_setl(pll->en_mask, pll->base_addr);
	clk_setl(AUDPLL_TUNER_EN, pll->base_addr + 8);
	udelay(20);

	if (pll->flags & HAVE_RST_BAR)
		clk_setl(RST_BAR_MASK, pll->base_addr);

	mtk_clk_unlock(flags);

	return 0;
}

static void clk_aud_pll_disable(struct clk_hw *hw)
{
	unsigned long flags = 0;
	struct mtk_clk_pll *pll = to_mtk_clk_pll(hw);

	pr_debug("%s: PLL_AO: %d\n",
		__clk_get_name(hw->clk), !!(pll->flags & PLL_AO));

	if (pll->flags & PLL_AO)
		return;

	mtk_clk_lock(flags);

	if (pll->flags & HAVE_RST_BAR)
		clk_clrl(RST_BAR_MASK, pll->base_addr);

	clk_clrl(AUDPLL_TUNER_EN, pll->base_addr + 8);
	clk_clrl(PLL_BASE_EN, pll->base_addr);

	clk_setl(PLL_ISO_EN, pll->pwr_addr);
	clk_clrl(PLL_PWR_ON, pll->pwr_addr);

	mtk_clk_unlock(flags);
}

#define AUD_PLL_POSTDIV_H	6
#define AUD_PLL_POSTDIV_L	4
#define AUD_PLL_POSTDIV_MASK	GENMASK(AUD_PLL_POSTDIV_H, AUD_PLL_POSTDIV_L)
#define AUD_PLL_PCW_H		30
#define AUD_PLL_PCW_L		0
#define AUD_PLL_PCW_MASK	GENMASK(AUD_PLL_PCW_H, AUD_PLL_PCW_L)

static unsigned long clk_aud_pll_recalc_rate(
		struct clk_hw *hw,
		unsigned long parent_rate)
{
	struct mtk_clk_pll *pll = to_mtk_clk_pll(hw);

	uint32_t con0 = clk_readl(pll->base_addr);
	uint32_t con1 = clk_readl(pll->base_addr + 4);

	uint32_t posdiv = (con0 & AUD_PLL_POSTDIV_MASK) >> AUD_PLL_POSTDIV_L;
	uint32_t pcw = (con1 & AUD_PLL_PCW_MASK) >> AUD_PLL_PCW_L;
	uint32_t pcwfbits = 24;

	uint32_t vco_freq;
	unsigned long r;

	parent_rate = parent_rate ? parent_rate : 26000000;

	vco_freq = calc_pll_vco_freq(
			parent_rate, pcw, 1, 1, pcwfbits);
	r = vco_freq / pll_posdiv_map[posdiv];

	pr_debug("%lu: %s\n", r, __clk_get_name(hw->clk));

	return r;
}

static void clk_aud_pll_set_rate_regs(
		struct clk_hw *hw,
		uint32_t pcw,
		uint32_t postdiv_idx)
{
	unsigned long flags = 0;
	struct mtk_clk_pll *pll = to_mtk_clk_pll(hw);
	void __iomem *con0_addr = pll->base_addr;
	void __iomem *con1_addr = pll->base_addr + 4;
	void __iomem *con2_addr = pll->base_addr + 8;
	uint32_t con0;
	uint32_t con1;
	uint32_t pll_en;

	mtk_clk_lock(flags);

	con0 = clk_readl(con0_addr);
	con1 = clk_readl(con1_addr);

	pll_en = con0 & PLL_BASE_EN;

	/* set postdiv */
	con0 &= ~AUD_PLL_POSTDIV_MASK;
	con0 |= postdiv_idx << AUD_PLL_POSTDIV_L;
	clk_writel(con0, con0_addr);

	/* set pcw */
	con1 &= ~AUD_PLL_PCW_MASK;
	con1 |= pcw << AUD_PLL_PCW_L;

	if (pll_en)
		con1 |= PLL_PCW_CHG;

	clk_writel(con1, con1_addr);
	clk_writel(con1 + 1, con2_addr);
		/* AUDPLL_CON4[30:0] (AUDPLL_TUNER_N_INFO) = (pcw + 1) */

	if (pll_en)
		udelay(20);

	mtk_clk_unlock(flags);
}

static int clk_aud_pll_set_rate(
		struct clk_hw *hw,
		unsigned long rate,
		unsigned long parent_rate)
{
	uint32_t pcwfbits = 24;
	uint32_t pcw = 0;
	uint32_t postdiv_idx = 0;
	int r;

	parent_rate = parent_rate ? parent_rate : 26000000;
	r = calc_pll_freq_cfg(&pcw, &postdiv_idx, rate, parent_rate, pcwfbits);

	pr_debug("%s, rate: %lu, pcw: %u, postdiv_idx: %u\n",
		__clk_get_name(hw->clk), rate, pcw, postdiv_idx);

	if (r == 0)
		clk_aud_pll_set_rate_regs(hw, pcw, postdiv_idx);

	return r;
}

const struct clk_ops mt_clk_aud_pll_ops = {
	.is_enabled	= clk_pll_is_enabled,
	.enable		= clk_aud_pll_enable,
	.disable	= clk_aud_pll_disable,
	.recalc_rate	= clk_aud_pll_recalc_rate,
	.round_rate	= clk_pll_round_rate,
	.set_rate	= clk_aud_pll_set_rate,
};
