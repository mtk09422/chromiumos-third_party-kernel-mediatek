/*
 * Copyright (c) 2014 MediaTek Inc.
 * Author: Flora Fu <flora.fu@mediatek.com>
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

#include <linux/clk-provider.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mfd/mt6397/core.h>
#include <linux/mfd/mt6397/registers.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <dt-bindings/clock/mediatek,mt6397-clks.h>

struct mt6397_clkpdn {
	struct clk_hw hw;
	struct regmap *regmap;
	u32 enable_reg;
	u32 enable_mask;
	u32 div_reg;
	u32 div_mask;
	u32 div_base;
	u32 div_factor;
	u32 mux_reg;
	u32 mux_mask;
};

#define to_mt6397_clkpdn(_hw) container_of(_hw, struct mt6397_clkpdn, hw)

static int mt6397_clkpdn_prepare(struct clk_hw *hw)
{
	struct mt6397_clkpdn *pdn = to_mt6397_clkpdn(hw);

	return regmap_update_bits(pdn->regmap, pdn->enable_reg,
		pdn->enable_mask, ~pdn->enable_mask);
}

static void mt6397_clkpdn_unprepare(struct clk_hw *hw)
{
	struct mt6397_clkpdn *pdn = to_mt6397_clkpdn(hw);

	regmap_update_bits(pdn->regmap, pdn->enable_reg,
		pdn->enable_mask, pdn->enable_mask);
}

static int mt6397_clkpdn_is_prepared(struct clk_hw *hw)
{
	struct mt6397_clkpdn *pdn = to_mt6397_clkpdn(hw);
	u32 val;

	regmap_read(pdn->regmap, pdn->enable_reg, &val);

	return (val & pdn->enable_mask) == 0;
}

static unsigned long mt6397_clk_recalc_rate(struct clk_hw *hw,
						  unsigned long parent_rate)
{
	struct mt6397_clkpdn *pdn = to_mt6397_clkpdn(hw);
	u32 div, val;

	if (pdn->div_reg == 0) {
		div = 1;
	} else {
		regmap_read(pdn->regmap, pdn->div_reg, &val);
		val &= pdn->div_mask;
		val >>= ffs(pdn->div_mask) - 1;

		if (val)
			div = (1 << val) * pdn->div_base * pdn->div_factor;
		else
			div = pdn->div_base;
	}

	return parent_rate / div;
}

static u8 mt6397_clk_get_parent(struct clk_hw *hw)
{
	struct mt6397_clkpdn *clk = to_mt6397_clkpdn(hw);
	int num_parents = __clk_get_num_parents(hw->clk);
	u32 val;

	if (num_parents == 1)
		return 0;

	regmap_read(clk->regmap, clk->mux_reg, &val);
	val &= clk->mux_mask;

	if (val >= num_parents)
		return -EINVAL;

	return val;
}

static int mt6397_clk_set_parent(struct clk_hw *hw, u8 index)
{
	struct mt6397_clkpdn *clk = to_mt6397_clkpdn(hw);
	u32 idx;

	idx = index;
	idx <<= ffs(clk->mux_mask) - 1;

	return regmap_update_bits(clk->regmap, clk->mux_reg, clk->mux_mask,
			idx);
}

static const struct clk_ops mt6397_clkpdn_ops = {
	.prepare = mt6397_clkpdn_prepare,
	.unprepare = mt6397_clkpdn_unprepare,
	.is_prepared = mt6397_clkpdn_is_prepared,
};

static const struct clk_ops mt6397_clkcomposite_ops = {
	.prepare = mt6397_clkpdn_prepare,
	.unprepare = mt6397_clkpdn_unprepare,
	.is_prepared = mt6397_clkpdn_is_prepared,
	.recalc_rate = mt6397_clk_recalc_rate,
	.get_parent = mt6397_clk_get_parent,
	.set_parent = mt6397_clk_set_parent,
};

enum {
	AUD26M,	AUD26M_DIV64, AUD13M,
	SMPS24M, SMPS12M, SMPS6M, SMPS3M, SMPS2M, SMPS1M,
	PMU75K, PMU12M,
	FG32K,
	RTC32K,
	CHR1M,
};

static const char const *clk_names[] = {
	[AUD26M] = "aud26m",
	[AUD26M_DIV64] = "aud26m_div64",
	[AUD13M] = "aud13m",
	[SMPS24M] = "smps24m",
	[SMPS12M] = "smps12m",
	[SMPS6M] = "smps6m",
	[SMPS3M] = "smps3m",
	[SMPS2M] = "smps2m",
	[SMPS1M] = "smps1m",
	[PMU75K] = "pmu75k",
	[PMU12M] = "pmu12m",
	[FG32K] = "fg32k",
	[RTC32K] = "rtc32k",
	[CHR1M] = "chr1m",
};

static const char const *mt6397_accdet_parents[] = {
	"rtc32k",
	"pmu12m",
	"top-accdet",
	"aud26m",
};

static const char const *mt6397_fgadc_ana_parents[] = {
	"rtc32k",
	"aud26m_div64",
};

static const char const *mt6397_fgmtr_parents[] = {
	"rtc32k",
	"aud26m",
};

#define GATES(id, _name, _reg, _mask, _parent_names)	\
[id] = {						\
	.hw.init = &(struct clk_init_data){		\
		.name = #_name,				\
		.ops = &mt6397_clkpdn_ops,		\
		.flags = CLK_IGNORE_UNUSED,		\
		.parent_names = _parent_names,		\
		.num_parents = 1,			\
	},						\
	.enable_reg = _reg,				\
	.enable_mask = _mask,				\
}

#define COMPOSITE(id, _name, _reg, _mask, _parent_names, _num_parents,	\
	_div_reg, _div_mask, _base, _factor, _mux_reg, _mux_mask)	\
[id] = {								\
	.hw.init = &(struct clk_init_data){				\
		.name = #_name,						\
		.ops = &mt6397_clkcomposite_ops,			\
		.flags = CLK_IGNORE_UNUSED | CLK_GET_RATE_NOCACHE,	\
		.parent_names = _parent_names,				\
		.num_parents = _num_parents,				\
	},								\
	.enable_reg = _reg,						\
	.enable_mask = _mask,						\
	.div_reg = _div_reg,						\
	.div_mask = _div_mask,						\
	.div_base = _base,						\
	.div_factor = _factor,						\
	.mux_reg = _mux_reg,						\
	.mux_mask = _mux_mask,						\
}

static struct mt6397_clkpdn mt6397_clks[MT6397_CLK_MAX] = {
	GATES(MT6397_TOPCKPDN_AUD_26M, "top-aud26m",
		MT6397_TOP_CKPDN, BIT(0), &clk_names[AUD26M]),
	GATES(MT6397_TOPCKPDN_AUD_13M, "top-aud13m",
		MT6397_TOP_CKPDN, BIT(1), &clk_names[AUD13M]),
	GATES(MT6397_TOPCKPDN_SPK_CK, "top-spk",
		MT6397_TOP_CKPDN, BIT(2), &clk_names[SMPS1M]),
	GATES(MT6397_TOPCKPDN_PWMOC_CK, "top-pwmoc",
		MT6397_TOP_CKPDN, BIT(3), &clk_names[SMPS2M]),
	GATES(MT6397_TOPCKPDN_EFUSE_CK, "top-efuse",
		MT6397_TOP_CKPDN, BIT(4), &clk_names[PMU75K]),
	GATES(MT6397_TOPCKPDN_FGADC_CK, "top-fgadc",
		MT6397_TOP_CKPDN, BIT(5), &clk_names[FG32K]),
	COMPOSITE(MT6397_TOPCKPDN_FGADC_ANA_CK, "top-fgadc-ana",
		MT6397_TOP_CKPDN, BIT(6), mt6397_fgadc_ana_parents, 2,
		0, 0, 0, 0, MT6397_TOP_CKCON1, BIT(12)),
	GATES(MT6397_TOPCKPDN_BST_DRV_1M_CK, "top-bstdrv1m",
		MT6397_TOP_CKPDN, BIT(7), &clk_names[SMPS1M]),
	COMPOSITE(MT6397_TOPCKPDN_RTC_MCLK, "top-rtc-mclk",
		MT6397_TOP_CKPDN, BIT(8), &clk_names[SMPS24M], 1,
		MT6397_TOP_CKCON2, 0xC000, 1, 1, 0, 0),
	COMPOSITE(MT6397_TOPCKPDN_SPK_PWM_DIV, "top-spkpwm",
		MT6397_TOP_CKPDN, BIT(9), &clk_names[SMPS1M], 1,
		MT6397_TOP_CKCON2, 0x0018, 1, 8, 0, 0),
	COMPOSITE(MT6397_TOPCKPDN_SPK_DIV, "top-spkdiv",
		MT6397_TOP_CKPDN, BIT(10), &clk_names[SMPS1M], 1,
		MT6397_TOP_CKCON2, 0x0060, 1, 1, 0, 0),
	GATES(MT6397_TOPCKPDN_SMPS_CK_DIV2, "top-smpsdiv2",
		MT6397_TOP_CKPDN, BIT(11), &clk_names[SMPS12M]),
	GATES(MT6397_TOPCKPDN_SMPS_CK_DIV, "top-smpsdiv",
		MT6397_TOP_CKPDN, BIT(12), &clk_names[SMPS24M]),
	COMPOSITE(MT6397_TOPCKPDN_AUXADC_CK, "top-auxadc",
		MT6397_TOP_CKPDN, BIT(13), &clk_names[PMU12M], 1,
		MT6397_TOP_CKCON2, 0x0003, 6, 1, 0, 0),
	COMPOSITE(MT6397_TOPCKPDN_ACCDET_CK, "top-accdet",
		MT6397_TOP_CKPDN, BIT(14), mt6397_accdet_parents, 4,
		0, 0, 0, 0, MT6397_TOP_CKCON1, 0x6000),
	GATES(MT6397_TOPCKPDN_STRUP_6M, "top-strup6m",
		MT6397_TOP_CKPDN, BIT(15), &clk_names[SMPS6M]),
	GATES(MT6397_TOPCKPDN2_RTC32K_1V8, "top2-rtc32k1v8",
		MT6397_TOP_CKPDN2, BIT(0), &clk_names[RTC32K]),
	COMPOSITE(MT6397_TOPCKPDN2_FQMTR, "top2-fqmtr",
		MT6397_TOP_CKPDN2, BIT(1), mt6397_fgmtr_parents, 2,
		0, 0, 0, 0, MT6397_TOP_CKCON1, BIT(15)),
	GATES(MT6397_TOPCKPDN2_STRUP_75K_CK, "top2-starup75k",
		MT6397_TOP_CKPDN2, BIT(2), &clk_names[PMU75K]),
	GATES(MT6397_TOPCKPDN2_RTC_32K_CK, "top2-rtc32k",
		MT6397_TOP_CKPDN2, BIT(3), &clk_names[RTC32K]),
	GATES(MT6397_TOPCKPDN2_PCHR_32K_CK, "top2-pchr32k",
		MT6397_TOP_CKPDN2, BIT(4), &clk_names[RTC32K]),
	GATES(MT6397_TOPCKPDN2_LDOSTB_1M_CK, "top2-ldostb1m",
		MT6397_TOP_CKPDN2, BIT(5), &clk_names[SMPS1M]),
	GATES(MT6397_TOPCKPDN2_INTRP_CK, "top2-intr",
		MT6397_TOP_CKPDN2, BIT(6), &clk_names[PMU75K]),
	GATES(MT6397_TOPCKPDN2_DRV_32K_CK, "top2-drv32k",
		MT6397_TOP_CKPDN2, BIT(7), &clk_names[SMPS1M]),
	GATES(MT6397_TOPCKPDN2_CHR1M_CK, "top2-chr1m",
		MT6397_TOP_CKPDN2, BIT(8), &clk_names[CHR1M]),
	GATES(MT6397_TOPCKPDN2_BUCK_CK, "top2-buck",
		MT6397_TOP_CKPDN2, BIT(9), &clk_names[PMU12M]),
	GATES(MT6397_TOPCKPDN2_BUCK_ANA_CK, "top2-buckana",
		MT6397_TOP_CKPDN2, BIT(10), &clk_names[SMPS2M]),
	GATES(MT6397_TOPCKPDN2_BUCK32K, "top2-buck32k",
		MT6397_TOP_CKPDN2, BIT(11), &clk_names[RTC32K]),
	GATES(MT6397_TOPCKPDN2_BUCK_1M_CK, "top2-buck1m",
		MT6397_TOP_CKPDN2, BIT(12), &clk_names[SMPS1M]),
	GATES(MT6397_TOPCKPDN2_STRUP_32K_CK, "top2-starup32k",
		MT6397_TOP_CKPDN2, BIT(13), &clk_names[RTC32K]),
	GATES(MT6397_TOPCKPDN2_RTC_75K_CK, "top2-rtc75k",
		MT6397_TOP_CKPDN2, BIT(14), &clk_names[PMU75K]),
	GATES(MT6397_TOPCKPDN2_RSV_15, "top2-rsv",
		MT6397_TOP_CKPDN2, BIT(15), &clk_names[SMPS24M]),
	GATES(MT6397_TOPCKPDN3_AUXADC_DIV_CK, "top3-auxadcdiv",
		MT6397_TOP_CKPDN3, BIT(0), &clk_names[PMU12M]),
	GATES(MT6397_TOPCKPDN3_RTC_75K_DIV4_CK, "top3-rtc75kdiv4",
		MT6397_TOP_CKPDN3, BIT(1), &clk_names[PMU75K]),
	COMPOSITE(MT6397_WRPCKPDN_I2C0, "wrap-i2c0",
		MT6397_WRP_CKPDN, BIT(0), &clk_names[SMPS24M], 1,
		MT6397_TOP_CKCON2, 0xC000, 1, 1, 0, 0),
	COMPOSITE(MT6397_WRPCKPDN_I2C1, "wrap-i2c1",
		MT6397_WRP_CKPDN, BIT(1), &clk_names[SMPS24M], 1,
		MT6397_TOP_CKCON2, 0xC000, 1, 1, 0, 0),
	COMPOSITE(MT6397_WRPCKPDN_I2C2, "wrap-i2c2",
		MT6397_WRP_CKPDN, BIT(2), &clk_names[SMPS24M], 1,
		MT6397_TOP_CKCON2, 0xC000, 1, 1, 0, 0),
	COMPOSITE(MT6397_WRPCKPDN_PWM, "wrap-pwm",
		MT6397_WRP_CKPDN, BIT(3), &clk_names[SMPS24M], 1,
		MT6397_TOP_CKCON2, 0xC000, 1, 1, 0, 0),
	COMPOSITE(MT6397_WRPCKPDN_KP, "wrap-kp",
		MT6397_WRP_CKPDN, BIT(4), &clk_names[SMPS24M], 1,
		MT6397_TOP_CKCON2, 0xC000, 1, 1, 0, 0),
	COMPOSITE(MT6397_WRPCKPDN_EINT, "wrap-eint",
		MT6397_WRP_CKPDN, BIT(5), &clk_names[SMPS24M], 1,
		MT6397_TOP_CKCON2, 0xC000, 1, 1, 0, 0),
	GATES(MT6397_WRPCKPDN_32K, "wrap-32k",
		MT6397_WRP_CKPDN, BIT(6), &clk_names[RTC32K]),
	COMPOSITE(MT6397_WRPCKPDN_WRP, "wrap-wrp",
		MT6397_WRP_CKPDN, BIT(7), &clk_names[SMPS24M], 1,
		MT6397_TOP_CKCON2, 0xC000, 1, 1, 0, 0),
};

static const struct of_device_id mt6397_clk_match_table[] = {
	{ .compatible = "mediatek,mt6397-clk" },
	{ }
};
MODULE_DEVICE_TABLE(of, mt6397_clk_match_table);

static int mt6397_clk_probe(struct platform_device *pdev)
{
	int i, ret;
	struct device *dev = &pdev->dev;
	struct clk **clk;
	size_t num_clks;
	struct mt6397_chip *mt6397 = dev_get_drvdata(pdev->dev.parent);
	struct clk_onecell_data *onecell;

	num_clks = ARRAY_SIZE(mt6397_clks);
	clk = devm_kzalloc(dev, sizeof(struct clk *) * num_clks, GFP_KERNEL);
	if (!clk)
		return -ENOMEM;

	onecell = devm_kzalloc(dev, sizeof(*onecell), GFP_KERNEL);
	if (!onecell)
		return -ENOMEM;

	clk[MT6397_CLK_AUD26M] = clk_register_fixed_rate(&pdev->dev,
			"aud26m", NULL, CLK_IS_ROOT, 26000000);

	clk[MT6397_CLK_AUD13M] = clk_register_fixed_factor(&pdev->dev,
			"aud13m", "aud26m", CLK_SET_RATE_PARENT, 1, 2);

	clk[MT6397_CLK_AUD26M_DIV64] = clk_register_fixed_factor(&pdev->dev,
			"aud26m_div64", "aud26m", CLK_SET_RATE_PARENT, 1, 64);

	clk[MT6397_CLK_SMPS24M] = clk_register_fixed_rate(&pdev->dev,
			"smps24m", NULL, CLK_IS_ROOT, 24000000);

	clk[MT6397_CLK_SMPS12M] = clk_register_fixed_factor(&pdev->dev,
			"smps12m", "smps24m", CLK_SET_RATE_PARENT, 1, 2);

	clk[MT6397_CLK_SMPS6M] = clk_register_fixed_factor(&pdev->dev,
			"smps6m", "smps24m", CLK_SET_RATE_PARENT, 1, 4);

	clk[MT6397_CLK_SMPS3M] = clk_register_fixed_factor(&pdev->dev,
			"smps3m", "smps24m", CLK_SET_RATE_PARENT, 1, 8);

	clk[MT6397_CLK_SMPS2M] = clk_register_fixed_factor(&pdev->dev,
			"smps2m", "smps24m", CLK_SET_RATE_PARENT, 1, 12);

	clk[MT6397_CLK_SMPS1M] = clk_register_fixed_factor(&pdev->dev,
			"smps1m", "smps24m", CLK_SET_RATE_PARENT, 1, 24);

	clk[MT6397_CLK_PMU75K] = clk_register_fixed_rate(&pdev->dev,
			"pmu75k", NULL, CLK_IS_ROOT, 75000);

	clk[MT6397_CLK_PMU12M] = clk_register_fixed_rate(&pdev->dev,
			"pmu12m", NULL, CLK_IS_ROOT, 12000000);

	clk[MT6397_CLK_FG32K] = clk_register_fixed_rate(&pdev->dev,
			"fg32k", NULL, CLK_IS_ROOT, 32000);

	clk[MT6397_CLK_RTC32K] = clk_register_fixed_rate(&pdev->dev,
			"rtc32k", NULL, CLK_IS_ROOT, 32000);

	clk[MT6397_CLK_CHR1M] = clk_register_fixed_rate(&pdev->dev,
			"chr1m", NULL, CLK_IS_ROOT, 1000000);

	for (i = MT6397_TOPCKPDN_AUD_26M; i < num_clks; i++) {
		mt6397_clks[i].regmap = mt6397->regmap;
		clk[i] = devm_clk_register(dev, &mt6397_clks[i].hw);
		if (IS_ERR(clk))
			return PTR_ERR(clk);
	}

	onecell->clk_num = num_clks;
	onecell->clks = clk;
	ret = of_clk_add_provider(dev->of_node,
				  of_clk_src_onecell_get, onecell);
	if (ret) {
		dev_err(dev, "unable to add clk provider\n");
		return ret;
	}

	return 0;
}

static int mt6397_clk_remove(struct platform_device *pdev)
{
	of_clk_del_provider(pdev->dev.of_node);

	return 0;
}

static struct platform_driver mt6397_clk_driver = {
	.probe = mt6397_clk_probe,
	.remove = mt6397_clk_remove,
	.driver = {
		.name = "mt6397-clk",
		.of_match_table = mt6397_clk_match_table,
	},
};

module_platform_driver(mt6397_clk_driver);

MODULE_AUTHOR("Flora Fu <flora.fu@mediatek.com>");
MODULE_DESCRIPTION("Clock Driver for MediaTek MT6397 PMIC");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:mt6397-clock");
