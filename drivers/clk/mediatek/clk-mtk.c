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
#include <linux/delay.h>
#include <linux/clkdev.h>

#include "clk-mtk.h"


#if CLK_DEBUG
	#define TRACE(fmt, args...) \
		pr_warn("[clk-mtk] %s():%d: " fmt, \
			__func__, __LINE__, ##args)
#else
	#define TRACE(fmt, args...)
#endif

#define PR_ERR(fmt, args...) pr_err("%s(): " fmt, __func__, ##args)

#define BITMASK(bits)		(BIT((1 ? bits) + 1) - BIT((0 ? bits)))
#define BITS(bits, val)		(BITMASK(bits) & (val << (0 ? bits)))

#define clk_readl(addr)		readl(addr)
#define clk_writel(addr, val)	do { writel(val, addr); dsb(sy); } while (0)
#define clk_setl(addr, mask)	clk_writel(addr, clk_readl(addr) | (mask))
#define clk_clrl(addr, mask)	clk_writel(addr, clk_readl(addr) & ~(mask))


/*
 * clk_fixed_rate
 */


static unsigned long fixed_clk_recalc_rate(
		struct clk_hw *hw,
		unsigned long parent_rate)
{
	TRACE("%s, parent_rate: %lu\n", __clk_get_name(hw->clk), parent_rate);
	return to_clk_fixed_rate(hw)->fixed_rate;
}


static const struct clk_ops mt_clk_fixed_rate_ops = {
	.recalc_rate	= fixed_clk_recalc_rate,
};


struct clk *mt_clk_register_fixed_rate(
		const char *name,
		const char *parent_name,
		unsigned long fixed_rate)
{
	struct mt_clk_fixed_rate *fixed;
	struct clk *clk;
	struct clk_init_data init;

	TRACE("name: %s, fixed_rate: %lu\n", name, fixed_rate);

	fixed = kzalloc(sizeof(*fixed), GFP_KERNEL);
	if (!fixed)
		return ERR_PTR(-ENOMEM);

	init.name = name;
	init.ops = &mt_clk_fixed_rate_ops;
	init.flags = CLK_IS_ROOT;
	init.parent_names = parent_name ? &parent_name : NULL;
	init.num_parents = parent_name ? 1 : 0;

	fixed->fixed_rate = fixed_rate;
	fixed->hw.init = &init;

	clk = clk_register(NULL, &fixed->hw);

	if (IS_ERR(clk))
		kfree(fixed);

	return clk;
}


/*
 * clk_gate
 */


int cg_prepare(struct clk_hw *hw)
{
	struct mt_clk_gate *cg = to_clk_gate(hw);

	TRACE("%s, bit: %u, pre_clk: %s\n", __clk_get_name(hw->clk), cg->bit,
		cg->pre_clk ? __clk_get_name(cg->pre_clk) : "");

	if (cg->pre_clk)
		return clk_prepare_enable(cg->pre_clk);
	else
		return 0;
}


void cg_unprepare(struct clk_hw *hw)
{
	struct mt_clk_gate *cg = to_clk_gate(hw);

	TRACE("%s, bit: %u, pre_clk: %s\n", __clk_get_name(hw->clk), cg->bit,
		cg->pre_clk ? __clk_get_name(cg->pre_clk) : "");

	if (cg->pre_clk)
		clk_disable_unprepare(cg->pre_clk);
}


static void cg_set_mask(struct mt_clk_gate *cg, uint32_t mask)
{
	if (cg->flags & CLK_GATE_NO_SETCLR_REG)
		clk_setl(cg->regs->sta_addr, mask);
	else
		clk_writel(cg->regs->set_addr, mask);
}


static void cg_clr_mask(struct mt_clk_gate *cg, uint32_t mask)
{
	if (cg->flags & CLK_GATE_NO_SETCLR_REG)
		clk_clrl(cg->regs->sta_addr, mask);
	else
		clk_writel(cg->regs->clr_addr, mask);
}


static int cg_enable(struct clk_hw *hw)
{
	struct mt_clk_gate *cg = to_clk_gate(hw);
	uint32_t mask = BIT(cg->bit);

	TRACE("%s, bit: %u\n", __clk_get_name(hw->clk), cg->bit);

	if (cg->before_enable)
		cg->before_enable(cg);

	if (cg->pre_clk_is_enabled && !cg->pre_clk_is_enabled(cg))
		return 0;

	if (cg->flags & CLK_GATE_INVERSE)
		cg_set_mask(cg, mask);
	else
		cg_clr_mask(cg, mask);

#if DUMMY_REG_TEST
	if (cg->flags & CLK_GATE_INVERSE)
		clk_setl(cg->regs->sta_addr, mask);
	else
		clk_clrl(cg->regs->sta_addr, mask);
#endif /* DUMMY_REG_TEST */

	return 0;
}


static void cg_disable(struct clk_hw *hw)
{
	struct mt_clk_gate *cg = to_clk_gate(hw);
	uint32_t mask = BIT(cg->bit);

	TRACE("%s, bit: %u\n", __clk_get_name(hw->clk), cg->bit);

	if (cg->pre_clk_is_enabled && !cg->pre_clk_is_enabled(cg))
		return;

	if (cg->flags & CLK_GATE_INVERSE)
		cg_clr_mask(cg, mask);
	else
		cg_set_mask(cg, mask);

#if DUMMY_REG_TEST
	if (cg->flags & CLK_GATE_INVERSE)
		clk_clrl(cg->regs->sta_addr, mask);
	else
		clk_setl(cg->regs->sta_addr, mask);
#endif /* DUMMY_REG_TEST */

	if (cg->after_disable)
		cg->after_disable(cg);
}


static int cg_is_enabled(struct clk_hw *hw)
{
	struct mt_clk_gate *cg = to_clk_gate(hw);

	if (cg->pre_clk_is_enabled && !cg->pre_clk_is_enabled(cg))
		return 0;
	else {
		uint32_t mask = BIT(cg->bit);
		uint32_t val = mask & clk_readl(cg->regs->sta_addr);

		int r = (cg->flags & CLK_GATE_INVERSE) ?
				(val != 0) : (val == 0);
		TRACE("%d, %s, bit[%d]\n",
			r, __clk_get_name(hw->clk), (int)cg->bit);
		return r;
	}
}


static const struct clk_ops mt_clk_gate_ops = {
	.prepare	= cg_prepare,
	.unprepare	= cg_unprepare,
	.is_enabled	= cg_is_enabled,
	.enable		= cg_enable,
	.disable	= cg_disable,
};


struct clk *mt_clk_register_gate(
		const char *name,
		const char *parent_name,
		const char *pre_clk_name,
		struct mt_clk_regs *regs,
		uint8_t bit,
		uint32_t flags,
		mt_clk_gate_cb before_enable,
		mt_clk_gate_cb after_disable,
		mt_clk_gate_bool_cb pre_clk_is_enabled)
{
	struct mt_clk_gate *cg;
	struct clk *clk;
	struct clk_init_data init;

	TRACE("name: %s, bit: %d\n", name, (int)bit);

	cg = kzalloc(sizeof(*cg), GFP_KERNEL);
	if (!cg)
		return ERR_PTR(-ENOMEM);

	init.name = name;
	init.flags = CLK_IGNORE_UNUSED;
	init.parent_names = parent_name ? &parent_name : NULL;
	init.num_parents = parent_name ? 1 : 0;
	init.ops = &mt_clk_gate_ops;

	if (pre_clk_name) {
		cg->pre_clk = clk_get(NULL, pre_clk_name);
		if (!cg->pre_clk) {
			kfree(cg);
			return ERR_PTR(-EINVAL);
		}
	}

	if (flags & CLK_GATE_INVERSE)
		regs->en_mask |= BIT(bit);
	else
		regs->cg_mask |= BIT(bit);

	cg->regs = regs;
	cg->bit = bit;
	cg->flags = flags;
	cg->before_enable = before_enable;
	cg->after_disable = after_disable;
	cg->pre_clk_is_enabled = pre_clk_is_enabled;

	cg->hw.init = &init;

	clk = clk_register(NULL, &cg->hw);
	if (IS_ERR(clk))
		kfree(cg);

	return clk;
}


/*
 * clk_mux
 */


static uint8_t mux_get_parent(struct clk_hw *hw)
{
	struct mt_clk_mux *mux = to_mt_clk_mux(hw);
	int num_parents = __clk_get_num_parents(hw->clk);
	uint32_t val;

	val = clk_readl(mux->base_addr) >> mux->shift;
	val &= mux->mask;

	TRACE("%s, val: %d\n", __clk_get_name(hw->clk), val);

	if (val >= num_parents)
		return -EINVAL;

	return val;
}


static int mux_set_parent(struct clk_hw *hw, uint8_t index)
{
	struct mt_clk_mux *mux = to_mt_clk_mux(hw);
	uint32_t val;

	WARN_ON(hw->clk && !(__clk_get_enable_count(hw->clk) &&
		__clk_is_enabled(hw->clk)));

	val = clk_readl(mux->base_addr);
	val &= ~(mux->mask << mux->shift);
	val |= index << mux->shift;
	clk_writel(mux->base_addr, val);

	TRACE("%s, index: %d\n", __clk_get_name(hw->clk), (int)index);

	return 0;
}


static int mux_enable(struct clk_hw *hw)
{
	struct mt_clk_mux *mux = to_mt_clk_mux(hw);

	TRACE("%s, bit: %u\n", __clk_get_name(hw->clk), mux->gate_bit);

	if (mux->gate_bit <= MAX_MUX_GATE_BIT) {
		uint32_t mask = BIT(mux->gate_bit);
		clk_clrl(mux->base_addr, mask);
	}

	return 0;
}


static void mux_disable(struct clk_hw *hw)
{
	struct mt_clk_mux *mux = to_mt_clk_mux(hw);

	TRACE("%s, bit: %u\n", __clk_get_name(hw->clk), mux->gate_bit);

	if (mux->gate_bit <= MAX_MUX_GATE_BIT) {
		uint32_t mask = BIT(mux->gate_bit);
		clk_setl(mux->base_addr, mask);
	}
}


static int mux_is_enabled(struct clk_hw *hw)
{
	struct mt_clk_mux *mux = to_mt_clk_mux(hw);
	uint32_t mask = BIT(mux->gate_bit);

	int r = ((mask & clk_readl(mux->base_addr)) == 0);
	TRACE("%d, %s, bit[%d]\n",
		r, __clk_get_name(hw->clk), (int)mux->gate_bit);

	return r;
}


static const struct clk_ops mt_clk_mux_ops = {
	.get_parent	= mux_get_parent,
	.set_parent	= mux_set_parent,
	.is_enabled	= mux_is_enabled,
	.enable		= mux_enable,
	.disable	= mux_disable,
};


struct clk *mt_clk_register_mux_table(
		const char *name,
		const char **parent_names,
		uint8_t num_parents,
		void __iomem *base_addr,
		uint8_t shift,
		uint32_t mask,
		uint8_t gate_bit)
{
	struct mt_clk_mux *mux;
	struct clk *clk;
	struct clk_init_data init;

	TRACE("name: %s, num_parents: %d, gate_bit: %d\n",
		name, (int)num_parents, (int)gate_bit);

	mux = kzalloc(sizeof(struct mt_clk_mux), GFP_KERNEL);
	if (!mux)
		return ERR_PTR(-ENOMEM);

	init.name = name;
	init.ops = &mt_clk_mux_ops;
	init.flags = CLK_IGNORE_UNUSED;
	init.parent_names = parent_names;
	init.num_parents = num_parents;

	mux->base_addr = base_addr;
	mux->shift = shift;
	mux->mask = mask;
	mux->gate_bit = gate_bit;
	mux->hw.init = &init;

	clk = clk_register(NULL, &mux->hw);

	if (IS_ERR(clk))
		kfree(mux);

	return clk;
}


struct clk *mt_clk_register_mux(
		const char *name,
		const char **parent_names,
		uint8_t num_parents,
		void __iomem *base_addr,
		uint8_t shift,
		uint8_t width,
		uint8_t gate_bit)
{
	uint32_t mask = BIT(width) - 1;

	TRACE("name: %s, num_parents: %d, gate_bit: %d\n",
		name, (int)num_parents, (int)gate_bit);

	return mt_clk_register_mux_table(name, parent_names, num_parents,
		base_addr, shift, mask, gate_bit);
}


/*
 * clk_fixed_factor
 */


static unsigned long factor_recalc_rate(
		struct clk_hw *hw,
		unsigned long parent_rate)
{
	struct mt_clk_fixed_factor *fix = to_mt_clk_fixed_factor(hw);
	unsigned long long int rate;

	TRACE("%s, parent_rate: %lu\n", __clk_get_name(hw->clk), parent_rate);

	rate = (unsigned long long int)parent_rate * fix->mult;
	do_div(rate, fix->div);
	return (unsigned long)rate;
}


static long factor_round_rate(
		struct clk_hw *hw,
		unsigned long rate,
		unsigned long *prate)
{
	struct mt_clk_fixed_factor *fix = to_mt_clk_fixed_factor(hw);

	TRACE("%s, rate: %lu\n", __clk_get_name(hw->clk), rate);

	if (__clk_get_flags(hw->clk) & CLK_SET_RATE_PARENT) {
		unsigned long best_parent;

		best_parent = (rate / fix->mult) * fix->div;
		*prate = __clk_round_rate(__clk_get_parent(hw->clk),
				best_parent);
	}

	return (*prate / fix->div) * fix->mult;
}


static int factor_set_rate(
		struct clk_hw *hw,
		unsigned long rate,
		unsigned long parent_rate)
{
	TRACE("%s, rate: %lu\n", __clk_get_name(hw->clk), rate);
	return 0;
}


static const struct clk_ops mt_clk_fixed_factor_ops = {
	.round_rate	= factor_round_rate,
	.set_rate	= factor_set_rate,
	.recalc_rate	= factor_recalc_rate,
};


struct clk *mt_clk_register_fixed_factor(
		const char *name,
		const char *parent_name,
		uint32_t mult,
		uint32_t div)
{
	struct mt_clk_fixed_factor *fix;
	struct clk_init_data init;
	struct clk *clk;

	TRACE("name: %s, %u/%u\n", name, mult, div);

	fix = kzalloc(sizeof(*fix), GFP_KERNEL);
	if (!fix)
		return ERR_PTR(-ENOMEM);

	fix->mult = mult;
	fix->div = div;
	fix->hw.init = &init;

	init.name = name;
	init.ops = &mt_clk_fixed_factor_ops;
	init.flags = 0;
	init.parent_names = &parent_name;
	init.num_parents = 1;

	clk = clk_register(NULL, &fix->hw);

	if (IS_ERR(clk))
		kfree(fix);

	return clk;
}


/*
 * clk_pll
 */


#define PLL_BASE_EN	BIT(0)
#define PLL_PWR_ON	BIT(0)
#define PLL_ISO_EN	BIT(1)
#define RST_BAR_MASK	BITMASK(27 : 27)

static const uint32_t pll_vcodivsel_map[2] = { 1, 2 };
static const uint32_t pll_prediv_map[4] = { 1, 2, 4, 4 };
static const uint32_t pll_posdiv_map[8] = { 1, 2, 4, 8, 16, 16, 16, 16 };
static const uint32_t pll_fbksel_map[4] = { 1, 2, 4, 4 };


static uint32_t calc_pll_vco_freq(
		uint32_t fin,
		uint32_t pcw,
		uint32_t vcodivsel,
		uint32_t prediv,
		uint32_t pcwfbits)
{
	/* vco = (fin * pcw * vcodivsel / prediv) >> pcwfbits; */
	uint64_t vco = fin;
	vco = vco * pcw * vcodivsel;
	do_div(vco, prediv);
	vco >>= pcwfbits;
	return (uint32_t)vco;
}


static int calc_pll_freq_cfg(
		uint32_t *pcw,
		uint32_t *postdiv_idx,
		uint32_t freq,
		uint32_t fin,
		int pcwfbits)
{
	static const uint64_t freq_max = 2000 * 1000 * 1000;	/* 2000 MHz */
	static const uint64_t freq_min = 1000 * 1000 * 1000;	/* 1000 MHz */
	static const uint64_t postdiv[] = { 1, 2, 4, 8, 16 };
	uint64_t n_info;
	uint32_t idx;

	TRACE("freq: %u\n", freq);

	/* search suitable postdiv */
	for (idx = 0;
		idx < ARRAY_SIZE(postdiv) && postdiv[idx] * freq <= freq_min;
		idx++)
		;

	if (idx >= ARRAY_SIZE(postdiv))
		return -2;	/* freq is out of range (too low) */
	else if (postdiv[idx] * freq > freq_max)
		return -3;	/* freq is out of range (too high) */

	/* n_info = freq * postdiv / 26MHz * 2^pcwfbits */
	n_info = (postdiv[idx] * freq) << pcwfbits;
	do_div(n_info, fin);

	*postdiv_idx = idx;
	*pcw = (uint32_t)n_info;

	return 0;
}


static int clk_pll_is_enabled(struct clk_hw *hw)
{
	struct mt_clk_pll *pll = to_mt_clk_pll(hw);

	int r = (clk_readl(pll->base_addr) & PLL_BASE_EN) != 0;
	TRACE("%d: %s\n", r, __clk_get_name(hw->clk));
	return r;
}


static int clk_pll_enable(struct clk_hw *hw)
{
	struct mt_clk_pll *pll = to_mt_clk_pll(hw);

	TRACE("%s\n", __clk_get_name(hw->clk));
	clk_setl(pll->pwr_addr, PLL_PWR_ON);
	udelay(1);
	clk_clrl(pll->pwr_addr, PLL_ISO_EN);
	udelay(1);

	clk_setl(pll->base_addr, pll->en_mask);
	udelay(20);

	if (pll->flags & HAVE_RST_BAR)
		clk_setl(pll->base_addr, RST_BAR_MASK);

	return 0;
}


static void clk_pll_disable(struct clk_hw *hw)
{
	struct mt_clk_pll *pll = to_mt_clk_pll(hw);

	TRACE("%s: PLL_AO: %d\n",
		__clk_get_name(hw->clk), !!(pll->flags & PLL_AO));

	if (pll->flags & PLL_AO)
		return;

	if (pll->flags & HAVE_RST_BAR)
		clk_clrl(pll->base_addr, RST_BAR_MASK);

	clk_clrl(pll->base_addr, PLL_BASE_EN);

	clk_setl(pll->pwr_addr, PLL_ISO_EN);
	clk_clrl(pll->pwr_addr, PLL_PWR_ON);
}


static unsigned long clk_pll_recalc_rate(
		struct clk_hw *hw,
		unsigned long parent_rate)
{
	struct mt_clk_pll *pll = to_mt_clk_pll(hw);

	uint32_t con0 = clk_readl(pll->base_addr);
	uint32_t con1 = clk_readl((uint8_t *)pll->base_addr + 4);

	uint32_t vcodivsel = (con0 & BIT(19)) >> 19;		/* bit[19] */
	uint32_t prediv = (con0 & BITMASK(5 : 4)) >> 4;		/* bit[5:4] */
	uint32_t posdiv = (con0 & BITMASK(8 : 6)) >> 6;		/* bit[8:6] */
	uint32_t pcw = con1 & BITMASK(20 : 0);			/* bit[20:0] */
	uint32_t pcwfbits = 14;

	uint32_t vco_freq;
	unsigned long r;

	parent_rate = parent_rate ? parent_rate : 26000000;
	vcodivsel = pll_vcodivsel_map[vcodivsel];
	prediv = pll_prediv_map[prediv];

	vco_freq = calc_pll_vco_freq(
			parent_rate, pcw, vcodivsel, prediv, pcwfbits);
	r = vco_freq / pll_posdiv_map[posdiv];
	TRACE("%lu: %s\n", r, __clk_get_name(hw->clk));
	return r;
}


static long clk_pll_round_rate(
		struct clk_hw *hw,
		unsigned long rate,
		unsigned long *prate)
{
	TRACE("%s, rate: %lu\n", __clk_get_name(hw->clk), rate);
	return rate;
}


static int clk_pll_set_rate(
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
	TRACE("%s, rate: %lu, pcw: %u, postdiv_idx: %u\n",
		__clk_get_name(hw->clk), rate, pcw, postdiv_idx);

	if (r == 0) {
		struct mt_clk_pll *pll = to_mt_clk_pll(hw);

		void __iomem *con0_addr = pll->base_addr;
		void __iomem *con1_addr = (uint8_t *)pll->base_addr + 4;
		uint32_t con0 = clk_readl(con0_addr);	/* PLL_CON0 */
		uint32_t con1 = clk_readl(con1_addr);	/* PLL_CON1 */
		uint32_t pll_en;

		/* set postdiv */
		clk_writel(con0_addr,
			(con0 & ~BITMASK(8 : 6)) | BITS(8 : 6, postdiv_idx));

		/* set pcw */
		pll_en = con0 & PLL_BASE_EN;	/* PLL_CON0[0] */

		con1 &= ~BITMASK(20 : 0);
		con1 |= pcw & BITMASK(20 : 0);	/* PLL_CON1[20:0] = pcw */

		if (pll_en)
			con1 |= BIT(31);	/* PLL_CON1[31] = 1 */

		clk_writel(con1_addr, con1);

		if (pll_en)
			udelay(20);
	}

	return r;
}


const struct clk_ops mt_clk_pll_ops = {
	.is_enabled	= clk_pll_is_enabled,
	.enable		= clk_pll_enable,
	.disable	= clk_pll_disable,
	.recalc_rate	= clk_pll_recalc_rate,
	.round_rate	= clk_pll_round_rate,
	.set_rate	= clk_pll_set_rate,
};


static unsigned long clk_arm_pll_recalc_rate(
		struct clk_hw *hw,
		unsigned long parent_rate)
{
	struct mt_clk_pll *pll = to_mt_clk_pll(hw);

	uint32_t con0 = clk_readl(pll->base_addr);
	uint32_t con1 = clk_readl((uint8_t *)pll->base_addr + 4);

	uint32_t vcodivsel = (con0 & BIT(19)) >> 19;		/* bit[19] */
	uint32_t prediv = (con0 & BITMASK(5 : 4)) >> 4;		/* bit[5:4] */
	uint32_t posdiv = (con1 & BITMASK(26 : 24)) >> 24;	/* bit[26:24] */
	uint32_t pcw = con1 & BITMASK(20 : 0);			/* bit[20:0] */
	uint32_t pcwfbits = 14;

	uint32_t vco_freq;
	unsigned long r;

	parent_rate = parent_rate ? parent_rate : 26000000;
	vcodivsel = pll_vcodivsel_map[vcodivsel];
	prediv = pll_prediv_map[prediv];

	vco_freq = calc_pll_vco_freq(
			parent_rate, pcw, vcodivsel, prediv, pcwfbits);
	r = vco_freq / pll_posdiv_map[posdiv];
	TRACE("%lu: %s\n", r, __clk_get_name(hw->clk));
	return r;
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
	TRACE("%s, rate: %lu, pcw: %u, postdiv_idx: %u\n",
		__clk_get_name(hw->clk), rate, pcw, postdiv_idx);

	if (r == 0) {
		struct mt_clk_pll *pll = to_mt_clk_pll(hw);

		void __iomem *con0_addr = pll->base_addr;
		void __iomem *con1_addr = (uint8_t *)pll->base_addr + 4;
		uint32_t con0 = clk_readl(con0_addr);	/* PLL_CON0 */
		uint32_t con1 = clk_readl(con1_addr);	/* PLL_CON1 */
		uint32_t pll_en;

		/* postdiv */
		con1 &= ~BITMASK(26 : 24);
		con1 |= BITS(26 : 24, postdiv_idx);
			/* PLL_CON1[26:24] = postdiv_idx */

		/* pcw */
		con1 &= ~BITMASK(20 : 0);
		con1 |= pcw & BITMASK(20 : 0);	/* PLL_CON1[20:0] = pcw */

		pll_en = con0 & PLL_BASE_EN;	/* PLL_CON0[0] */
		if (pll_en)
			con1 |= BIT(31);	/* PLL_CON1[31] = 1 */

		clk_writel(con1_addr, con1);

		if (pll_en)
			udelay(20);
	}

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


static int clk_lc_pll_enable(struct clk_hw *hw)
{
	struct mt_clk_pll *pll = to_mt_clk_pll(hw);

	TRACE("%s\n", __clk_get_name(hw->clk));

	clk_setl(pll->base_addr, pll->en_mask);
	udelay(20);

	if (pll->flags & HAVE_RST_BAR)
		clk_setl(pll->base_addr, RST_BAR_MASK);

	return 0;
}


static void clk_lc_pll_disable(struct clk_hw *hw)
{
	struct mt_clk_pll *pll = to_mt_clk_pll(hw);

	TRACE("%s\n", __clk_get_name(hw->clk));

	if (pll->flags & HAVE_RST_BAR)
		clk_clrl(pll->base_addr, RST_BAR_MASK);

	clk_clrl(pll->base_addr, PLL_BASE_EN);
}


static unsigned long clk_lc_pll_recalc_rate(
		struct clk_hw *hw,
		unsigned long parent_rate)
{
	struct mt_clk_pll *pll = to_mt_clk_pll(hw);

	uint32_t con0 = clk_readl(pll->base_addr);

	uint32_t fbksel = (con0 & BITMASK(21 : 20)) >> 20;	/* bit[21:20] */
	uint32_t vcodivsel = (con0 & BIT(19)) >> 19;		/* bit[19] */
	uint32_t fbkdiv = (con0 & BITMASK(15 : 9)) >> 9;	/* bit[15:9] */
	uint32_t prediv = (con0 & BITMASK(5 : 4)) >> 4;		/* bit[5:4] */
	uint32_t posdiv = (con0 & BITMASK(8 : 6)) >> 6;		/* bit[8:6] */

	uint32_t vco_freq;
	unsigned long r;

	parent_rate = parent_rate ? parent_rate : 26000000;
	vcodivsel = pll_vcodivsel_map[vcodivsel];
	fbksel = pll_fbksel_map[fbksel];
	prediv = pll_prediv_map[prediv];

	vco_freq = parent_rate * fbkdiv * fbksel * vcodivsel / prediv;
	r = vco_freq / pll_posdiv_map[posdiv];
	TRACE("%lu: %s\n", r, __clk_get_name(hw->clk));
	return r;
}


static int clk_lc_pll_set_rate(
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
	TRACE("%s, rate: %lu, pcw: %u, postdiv_idx: %u\n",
		__clk_get_name(hw->clk), rate, pcw, postdiv_idx);

	if (r == 0) {
		struct mt_clk_pll *pll = to_mt_clk_pll(hw);

		void __iomem *con0_addr = pll->base_addr;
		uint32_t con0 = clk_readl(con0_addr);	/* PLL_CON0 */
		uint32_t pll_en;

		/* postdiv */
		con0 &= ~BITMASK(8 : 6);
		con0 |= BITS(8 : 6, postdiv_idx);
			/* PLL_CON0[8:6] = postdiv_idx */

		/* pcw */
		con0 &= ~BITMASK(15 : 9);
		con0 |= BITS(15 : 9, pcw);	/* PLL_CON0[15:9] = pcw */

		clk_writel(con0_addr, con0);

		pll_en = con0 & PLL_BASE_EN;	/* PLL_CON0[0] */
		if (pll_en)
			udelay(20);
	}

	return r;
}


const struct clk_ops mt_clk_lc_pll_ops = {
	.is_enabled	= clk_pll_is_enabled,
	.enable		= clk_lc_pll_enable,
	.disable	= clk_lc_pll_disable,
	.recalc_rate	= clk_lc_pll_recalc_rate,
	.round_rate	= clk_pll_round_rate,
	.set_rate	= clk_lc_pll_set_rate,
};


static int clk_aud_pll_enable(struct clk_hw *hw)
{
	struct mt_clk_pll *pll = to_mt_clk_pll(hw);

	TRACE("%s\n", __clk_get_name(hw->clk));

	clk_setl(pll->pwr_addr, PLL_PWR_ON);
	udelay(1);
	clk_clrl(pll->pwr_addr, PLL_ISO_EN);
	udelay(1);

	clk_setl(pll->base_addr, pll->en_mask);
	clk_setl(pll->base_addr + 16, BIT(31));
		/* AUDPLL_CON4[31] (AUDPLL_TUNER_EN) = 1 */
	udelay(20);

	if (pll->flags & HAVE_RST_BAR)
		clk_setl(pll->base_addr, RST_BAR_MASK);

	return 0;
}


static void clk_aud_pll_disable(struct clk_hw *hw)
{
	struct mt_clk_pll *pll = to_mt_clk_pll(hw);

	TRACE("%s\n", __clk_get_name(hw->clk));

	if (pll->flags & HAVE_RST_BAR)
		clk_clrl(pll->base_addr, RST_BAR_MASK);

	/* AUDPLL_CON4[31] (AUDPLL_TUNER_EN) = 0 */
	clk_clrl(pll->base_addr + 16, BIT(31));
	clk_clrl(pll->base_addr, PLL_BASE_EN);

	clk_setl(pll->pwr_addr, PLL_ISO_EN);
	clk_clrl(pll->pwr_addr, PLL_PWR_ON);
}


static unsigned long clk_aud_pll_recalc_rate(
		struct clk_hw *hw,
		unsigned long parent_rate)
{
	struct mt_clk_pll *pll = to_mt_clk_pll(hw);

	uint32_t con0 = clk_readl(pll->base_addr);
	uint32_t con1 = clk_readl((uint8_t *)pll->base_addr + 4);

	uint32_t vcodivsel = (con0 & BIT(19)) >> 19;		/* bit[19] */
	uint32_t prediv = (con0 & BITMASK(5 : 4)) >> 4;		/* bit[5:4] */
	uint32_t posdiv = (con0 & BITMASK(8 : 6)) >> 6;		/* bit[8:6] */
	uint32_t pcw = con1 & BITMASK(30 : 0);			/* bit[30:0] */
	uint32_t pcwfbits = 24;

	uint32_t vco_freq;
	unsigned long r;

	parent_rate = parent_rate ? parent_rate : 26000000;
	vcodivsel = pll_vcodivsel_map[vcodivsel];
	prediv = pll_prediv_map[prediv];

	vco_freq = calc_pll_vco_freq(
			parent_rate, pcw, vcodivsel, prediv, pcwfbits);
	r = vco_freq / pll_posdiv_map[posdiv];
	TRACE("%lu: %s\n", r, __clk_get_name(hw->clk));
	return r;
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
	TRACE("%s, rate: %lu, pcw: %u, postdiv_idx: %u\n",
		__clk_get_name(hw->clk), rate, pcw, postdiv_idx);

	if (r == 0) {
		struct mt_clk_pll *pll = to_mt_clk_pll(hw);

		void __iomem *con0_addr = pll->base_addr;
		void __iomem *con1_addr = (uint8_t *)pll->base_addr + 4;
		void __iomem *con4_addr = (uint8_t *)pll->base_addr + 16;
		uint32_t con0 = clk_readl(con0_addr);	/* PLL_CON0 */
		uint32_t con1 = clk_readl(con1_addr);	/* PLL_CON1 */
		uint32_t pll_en;

		/* set postdiv */
		clk_writel(con0_addr,
			(con0 & ~BITMASK(8 : 6)) | BITS(8 : 6, postdiv_idx));

		/* set pcw */
		pll_en = con0 & PLL_BASE_EN;	/* PLL_CON0[0] */

		con1 &= ~BITMASK(30 : 0);
		con1 |= BITS(30 : 0, pcw);	/* PLL_CON1[30:0] = pcw */

		if (pll_en)
			con1 |= BIT(31);	/* PLL_CON1[31] = 1 */

		/* AUDPLL_CON4[30:0] (AUDPLL_TUNER_N_INFO) = (pcw + 1) */
		clk_writel(con4_addr, con1 + 1);
		clk_writel(con1_addr, con1);

		if (pll_en)
			udelay(20);
	}

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


static int clk_tvd_pll_set_rate(
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
	TRACE("%s, rate: %lu, pcw: %u, postdiv_idx: %u\n",
		__clk_get_name(hw->clk), rate, pcw, postdiv_idx);

	if (r == 0) {
		struct mt_clk_pll *pll = to_mt_clk_pll(hw);

		void __iomem *con0_addr = pll->base_addr;
		void __iomem *con1_addr = (uint8_t *)pll->base_addr + 4;
		uint32_t con0 = clk_readl(con0_addr);	/* PLL_CON0 */
		uint32_t con1 = clk_readl(con1_addr);	/* PLL_CON1 */
		uint32_t pll_en;

		/* set postdiv */
		clk_writel(con0_addr,
			(con0 & ~BITMASK(8 : 6)) | BITS(8 : 6, postdiv_idx));

		/* set pcw */
		pll_en = con0 & PLL_BASE_EN;	/* PLL_CON0[0] */

		con1 &= ~BITMASK(30 : 0);
		con1 |= BITS(30 : 0, pcw);	/* PLL_CON1[30:0] = pcw */

		if (pll_en)
			con1 |= BIT(31);	/* PLL_CON1[31] = 1 */

		clk_writel(con1_addr, con1);

		if (pll_en)
			udelay(20);
	}

	return r;
}


const struct clk_ops mt_clk_tvd_pll_ops = {
	.is_enabled	= clk_pll_is_enabled,
	.enable		= clk_pll_enable,
	.disable	= clk_pll_disable,
	.recalc_rate	= clk_aud_pll_recalc_rate,
	.round_rate	= clk_pll_round_rate,
	.set_rate	= clk_tvd_pll_set_rate,
};


struct clk *mt_clk_register_pll(
		const char *name,
		const char *parent_name,
		uint32_t *base_addr,
		uint32_t *pwr_addr,
		uint32_t en_mask,
		uint32_t flags,
		const struct clk_ops *ops)
{
	struct mt_clk_pll *pll;
	struct clk_init_data init;
	struct clk *clk;

	TRACE("name: %s\n", name);

	pll = kzalloc(sizeof(*pll), GFP_KERNEL);
	if (!pll)
		return ERR_PTR(-ENOMEM);

	pll->base_addr = base_addr;
	pll->pwr_addr = pwr_addr;
	pll->en_mask = en_mask;
	pll->flags = flags;
	pll->hw.init = &init;

	init.name = name;
	init.ops = ops;
	init.flags = CLK_IGNORE_UNUSED;
	init.parent_names = &parent_name;
	init.num_parents = 1;

	clk = clk_register(NULL, &pll->hw);

	if (IS_ERR(clk))
		kfree(pll);

	return clk;
}


/*
 * device tree support
 */


static void __init mt_clk_fixed_rate_init(struct device_node *node, void *data)
{
	u32 rate;
	struct clk *clk;

	if (of_property_read_u32(node, "clock-frequency", &rate)) {
		PR_ERR("missing clock-frequency\n");
		return;
	}

	TRACE("fixed_rate: %s, %u\n", node->name, rate);

	clk = mt_clk_register_fixed_rate(node->name, NULL, rate);
	if (!IS_ERR(clk)) {
		of_clk_add_provider(node, of_clk_src_simple_get, clk);
		clk_register_clkdev(clk, node->name, NULL);
	}
}
CLK_OF_DECLARE(
	mtk_fixed_rate,
	"mediatek,clk-fixed_rate",
	mt_clk_fixed_rate_init);


static void __init mt_clk_pll_init(
		struct device_node *node,
		uint32_t en_mask,
		uint32_t flags,
		const struct clk_ops *ops)
{
	void __iomem *reg;
	void __iomem *pwr_reg;
	const char *parent_name;
	struct clk *clk;

	reg = of_iomap(node, 0);
	pwr_reg = of_iomap(node, 1);
	if (!reg || !pwr_reg) {
		PR_ERR("missing reg\n");
		return;
	}

	parent_name = of_clk_get_parent_name(node, 0);

	TRACE("pll: %s, reg: 0x%08x, pwr_reg: 0x%08x\n",
		node->name, (uint32_t)reg, (uint32_t)pwr_reg);

	clk = mt_clk_register_pll(node->name, parent_name,
		reg, pwr_reg, en_mask, flags, ops);

	if (!IS_ERR(clk)) {
		of_clk_add_provider(node, of_clk_src_simple_get, clk);
		clk_register_clkdev(clk, node->name, NULL);
	}
}


static void __init mt_clk_pll_arm_init(struct device_node *node, void *data)
{
	mt_clk_pll_init(node, 0x80000001,
		HAVE_PLL_HP, &mt_clk_arm_pll_ops);
}
CLK_OF_DECLARE(mtk_pll_arm, "mediatek,clk-pll-arm", mt_clk_pll_arm_init);


static void __init mt_clk_pll_main_init(struct device_node *node, void *data)
{
	mt_clk_pll_init(node, 0xF0000001,
		HAVE_PLL_HP | HAVE_RST_BAR | PLL_AO, &mt_clk_pll_ops);
}
CLK_OF_DECLARE(mtk_pll_main, "mediatek,clk-pll-main", mt_clk_pll_main_init);


static void __init mt_clk_pll_univ_init(struct device_node *node, void *data)
{
	mt_clk_pll_init(node, 0xF3000001,
		HAVE_RST_BAR | HAVE_FIX_FRQ | PLL_AO, &mt_clk_lc_pll_ops);
}
CLK_OF_DECLARE(mtk_pll_univ, "mediatek,clk-pll-univ", mt_clk_pll_univ_init);


static void __init mt_clk_pll_mm_init(struct device_node *node, void *data)
{
	mt_clk_pll_init(node, 0xF0000001,
		HAVE_PLL_HP | HAVE_RST_BAR, &mt_clk_pll_ops);
}
CLK_OF_DECLARE(mtk_pll_mm, "mediatek,clk-pll-mm", mt_clk_pll_mm_init);


static void __init mt_clk_pll_msdc_init(struct device_node *node, void *data)
{
	mt_clk_pll_init(node, 0x80000001,
		HAVE_PLL_HP, &mt_clk_pll_ops);
}
CLK_OF_DECLARE(mtk_pll_msdc, "mediatek,clk-pll-msdc", mt_clk_pll_msdc_init);


static void __init mt_clk_pll_tvd_init(struct device_node *node, void *data)
{
	mt_clk_pll_init(node, 0x80000001,
		HAVE_PLL_HP, &mt_clk_tvd_pll_ops);
}
CLK_OF_DECLARE(mtk_pll_tvd, "mediatek,clk-pll-tvd", mt_clk_pll_tvd_init);


static void __init mt_clk_pll_lvds_init(struct device_node *node, void *data)
{
	mt_clk_pll_init(node, 0x80000001,
		HAVE_PLL_HP, &mt_clk_pll_ops);
}
CLK_OF_DECLARE(mtk_pll_lvds, "mediatek,clk-pll-lvds", mt_clk_pll_lvds_init);


static void __init mt_clk_pll_aud_init(struct device_node *node, void *data)
{
	mt_clk_pll_init(node, 0x80000001,
		0, &mt_clk_aud_pll_ops);
}
CLK_OF_DECLARE(mtk_pll_aud, "mediatek,clk-pll-aud", mt_clk_pll_aud_init);


static void __init mt_clk_pll_vdec_init(struct device_node *node, void *data)
{
	mt_clk_pll_init(node, 0x80000001,
		HAVE_PLL_HP, &mt_clk_pll_ops);
}
CLK_OF_DECLARE(mtk_pll_vdec, "mediatek,clk-pll-vdec", mt_clk_pll_vdec_init);


static void __init mt_clk_fixed_factor_init(
		struct device_node *node, void *data)
{
	u32 mult, div;
	const char *parent_name;
	struct clk *clk;

	if (of_property_read_u32(node, "clock-mult", &mult)) {
		PR_ERR("missing clock-mult\n");
		return;
	}

	if (of_property_read_u32(node, "clock-div", &div)) {
		PR_ERR("missing clock-div\n");
		return;
	}

	parent_name = of_clk_get_parent_name(node, 0);
	if (!parent_name) {
		PR_ERR("missing clocks\n");
		return;
	}

	TRACE("div: %s = %s * %u / %u\n", node->name, parent_name, mult, div);

	clk = mt_clk_register_fixed_factor(node->name, parent_name, mult, div);
	if (!IS_ERR(clk)) {
		of_clk_add_provider(node, of_clk_src_simple_get, clk);
		clk_register_clkdev(clk, node->name, NULL);
	}
}
CLK_OF_DECLARE(
	mtk_fixed_factor,
	"mediatek,clk-fixed_factor",
	mt_clk_fixed_factor_init);


static void __init mt_clk_mux_init(struct device_node *node, void *data)
{
	struct device_node *np;
	struct device_node *clocks;
	void __iomem *reg;
	u32 shift, width, gate_bit;
	int parent_count;
	const char **parent_names;
	const char *parent_name;
	struct clk *clk;
	int i;

	reg = of_iomap(node, 0);
	if (!reg) {
		PR_ERR("missing reg\n");
		return;
	}

	clocks = of_get_child_by_name(node, "clocks");
	if (!clocks) {
		PR_ERR("missing clocks\n");
		return;
	}

	for_each_child_of_node(clocks, np) {
		if (of_property_read_u32(np, "bit-shift", &shift)) {
			PR_ERR("missing bit-shift\n");
			continue;
		}

		if (of_property_read_u32(np, "bit-width", &width)) {
			PR_ERR("missing bit-width\n");
			continue;
		}

		if (of_property_read_u32(np, "gate-bit", &gate_bit))
			gate_bit = INVALID_MUX_GATE_BIT;

		TRACE(
			"mux: %s, reg: 0x%08x, shift: %u, width: %u, gate_bit: %u\n",
			np->name, (uint32_t)reg, shift, width, gate_bit);

		parent_count = of_count_phandle_with_args(np,
				"clocks", "#clock-cells");
		parent_names = kzalloc(sizeof(const char *) * parent_count,
				GFP_KERNEL);

		if (!parent_names)
			continue;

		for (i = 0; i < parent_count; i++) {
			parent_name = of_clk_get_parent_name(np, i);
			TRACE("parent of (%s, %d/%d): %s\n",
				np->name, i, parent_count, parent_name);
			parent_names[i] = parent_name;
		}

		clk = mt_clk_register_mux(np->name, parent_names, parent_count,
			reg, shift, width, gate_bit);

		kfree(parent_names);

		if (!IS_ERR(clk)) {
			of_clk_add_provider(np, of_clk_src_simple_get, clk);
			clk_register_clkdev(clk, np->name, NULL);
		}
	}
}
CLK_OF_DECLARE(mtk_mux, "mediatek,clk-mux", mt_clk_mux_init);


static void __init mt_clk_common_gate_init(
		struct device_node *node, uint32_t flags)
{
	struct device_node *np;
	struct device_node *clocks;
	void __iomem *reg_sta;
	void __iomem *reg_clr;
	void __iomem *reg_set;
	const char *parent_name;
	u32 shift;
	struct mt_clk_regs *regs;
	struct clk *clk;

	reg_sta = of_iomap(node, 0);
	reg_clr = of_iomap(node, 1);
	reg_set = of_iomap(node, 2);

	if (!reg_sta || !reg_clr || !reg_set) {
		PR_ERR("missing reg\n");
		return;
	}

	clocks = of_get_child_by_name(node, "clocks");
	if (!clocks) {
		PR_ERR("missing clocks\n");
		return;
	}

	TRACE(
		"sys: %s, reg_sta: 0x%08x, reg_clr: 0x%08x, reg_set: 0x%08x, flags: 0x%x\n",
		node->name, (uint32_t)reg_sta, (uint32_t)reg_clr,
		(uint32_t)reg_set, flags);

	regs = kzalloc(sizeof(struct mt_clk_regs), GFP_KERNEL);
	if (!regs)
		return;

	regs->sta_addr = reg_sta;
	regs->set_addr = reg_set;
	regs->clr_addr = reg_clr;

	for_each_child_of_node(clocks, np) {
		if (of_property_read_u32(np, "bit-shift", &shift)) {
			PR_ERR("missing bit-shift\n");
			continue;
		}

		parent_name = of_clk_get_parent_name(np, 0);

		TRACE("gate: %s, shift: %u, parent: %s\n",
			np->name, shift, parent_name);

		clk = mt_clk_register_gate(np->name, parent_name, NULL,
			regs, shift, flags, NULL, NULL, NULL);

		if (!IS_ERR(clk)) {
			of_clk_add_provider(np, of_clk_src_simple_get, clk);
			clk_register_clkdev(clk, np->name, NULL);
		}
	}

}


static void __init mt_clk_gate_init(struct device_node *node, void *data)
{
	mt_clk_common_gate_init(node, 0);
}
CLK_OF_DECLARE(mtk_gate, "mediatek,clk-gate", mt_clk_gate_init);


static void __init mt_clk_gate_inv_init(struct device_node *node, void *data)
{
	mt_clk_common_gate_init(node, CLK_GATE_INVERSE);
}
CLK_OF_DECLARE(mtk_gate_inv, "mediatek,clk-gate-inv", mt_clk_gate_inv_init);


static void __init mt_clk_gate_audio_init(struct device_node *node, void *data)
{
	struct device_node *np;
	struct device_node *clocks;
	void __iomem *reg;
	const char *parent_name;
	const char *pre_clk_name;
	u32 shift;
	struct mt_clk_regs *regs;
	struct clk *clk;

	reg = of_iomap(node, 0);

	if (!reg) {
		PR_ERR("missing reg\n");
		return;
	}

	clocks = of_get_child_by_name(node, "clocks");
	if (!clocks) {
		PR_ERR("missing clocks\n");
		return;
	}

	TRACE("sys: %s, reg: 0x%08x, flags: CLK_GATE_NO_SETCLR_REG\n",
		node->name, (uint32_t)reg);

	regs = kzalloc(sizeof(struct mt_clk_regs), GFP_KERNEL);
	if (!regs)
		return;

	regs->sta_addr = reg;
	regs->set_addr = reg;
	regs->clr_addr = reg;

	for_each_child_of_node(clocks, np) {
		if (of_property_read_u32(np, "bit-shift", &shift)) {
			PR_ERR("missing gate-bit\n");
			continue;
		}

		parent_name = of_clk_get_parent_name(np, 0);
		pre_clk_name = of_clk_get_parent_name(np, 1);

		TRACE("gate: %s, shift: %u, parent: %s, pre_clk: %s\n",
			np->name, shift, parent_name, pre_clk_name);

		clk = mt_clk_register_gate(np->name, parent_name, pre_clk_name,
			regs, shift, CLK_GATE_NO_SETCLR_REG, NULL, NULL, NULL);

		if (!IS_ERR(clk)) {
			of_clk_add_provider(np, of_clk_src_simple_get, clk);
			clk_register_clkdev(clk, np->name, NULL);
		}
	}

}
CLK_OF_DECLARE(
	mtk_gate_audio,
	"mediatek,clk-gate-audio",
	mt_clk_gate_audio_init);
