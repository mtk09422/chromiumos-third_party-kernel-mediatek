/*
 * Copyright (c) 2014 MediaTek Inc.
 * Author: Flora.Fu <flora.fu@mediatek.com>
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

static const struct clk_ops mt6397_clkpdn_ops = {
	.prepare = mt6397_clkpdn_prepare,
	.unprepare = mt6397_clkpdn_unprepare,
	.is_prepared = mt6397_clkpdn_is_prepared,
};

#define GATES(id, _name, _reg, _mask)		\
[id] = {					\
	.hw.init = &(struct clk_init_data){	\
		.name = #_name,			\
		.ops = &mt6397_clkpdn_ops,	\
		.flags = CLK_IGNORE_UNUSED,	\
	},					\
	.enable_reg = _reg,			\
	.enable_mask = _mask,			\
}

static struct mt6397_clkpdn mt6397_clks[] = {
	GATES(MT6397_TOPCKPDN_AUD_26M, "top-aud26m",
		MT6397_TOP_CKPDN, BIT(0)),
	GATES(MT6397_TOPCKPDN_AUD_13M, "top-aud13m",
		MT6397_TOP_CKPDN, BIT(1)),
	GATES(MT6397_TOPCKPDN_SPK_CK, "top-spk",
		MT6397_TOP_CKPDN, BIT(2)),
	GATES(MT6397_TOPCKPDN_PWMOC_CK, "top-pwmoc",
		MT6397_TOP_CKPDN, BIT(3)),
	GATES(MT6397_TOPCKPDN_EFUSE_CK, "top-efuse",
		MT6397_TOP_CKPDN, BIT(4)),
	GATES(MT6397_TOPCKPDN_FGADC_CK, "top-fgadc",
		MT6397_TOP_CKPDN, BIT(5)),
	GATES(MT6397_TOPCKPDN_FGADC_ANA_CK, "top-ana",
		MT6397_TOP_CKPDN, BIT(6)),
	GATES(MT6397_TOPCKPDN_BST_DRV_1M_CK, "top-bstdrv1m",
		MT6397_TOP_CKPDN, BIT(7)),
	GATES(MT6397_TOPCKPDN_RTC_MCLK, "top-rtc-mclk",
		MT6397_TOP_CKPDN, BIT(8)),
	GATES(MT6397_TOPCKPDN_SPK_PWM_DIV, "top-spkpwm",
		MT6397_TOP_CKPDN, BIT(9)),
	GATES(MT6397_TOPCKPDN_SPK_DIV, "top-spkdiv",
		MT6397_TOP_CKPDN, BIT(10)),
	GATES(MT6397_TOPCKPDN_SMPS_CK_DIV2, "top-smpsdiv2",
		MT6397_TOP_CKPDN, BIT(11)),
	GATES(MT6397_TOPCKPDN_SMPS_CK_DIV, "top-smpsdiv",
		MT6397_TOP_CKPDN, BIT(12)),
	GATES(MT6397_TOPCKPDN_AUXADC_CK, "top-auxadc",
		MT6397_TOP_CKPDN, BIT(13)),
	GATES(MT6397_TOPCKPDN_ACCDET_CK, "top-accdet",
		MT6397_TOP_CKPDN, BIT(14)),
	GATES(MT6397_TOPCKPDN_STRUP_6M, "top-strup6m",
		MT6397_TOP_CKPDN, BIT(15)),
	GATES(MT6397_TOPCKPDN2_RTC32K_1V8, "top2-rtc32k1v8",
		MT6397_TOP_CKPDN2, BIT(0)),
	GATES(MT6397_TOPCKPDN2_FQMTR, "top2-fqmtr",
		MT6397_TOP_CKPDN2, BIT(1)),
	GATES(MT6397_TOPCKPDN2_STRUP_75K_CK, "top2-starup75k",
		MT6397_TOP_CKPDN2, BIT(2)),
	GATES(MT6397_TOPCKPDN2_RTC_32K_CK, "top2-rtc32k",
		MT6397_TOP_CKPDN2, BIT(3)),
	GATES(MT6397_TOPCKPDN2_PCHR_32K_CK, "top2-pchr32k",
		MT6397_TOP_CKPDN2, BIT(4)),
	GATES(MT6397_TOPCKPDN2_LDOSTB_1M_CK, "top2-ldostb1m",
		MT6397_TOP_CKPDN2, BIT(5)),
	GATES(MT6397_TOPCKPDN2_INTRP_CK, "top2-intr",
		MT6397_TOP_CKPDN2, BIT(6)),
	GATES(MT6397_TOPCKPDN2_DRV_32K_CK, "top2-drv32k",
		MT6397_TOP_CKPDN2, BIT(7)),
	GATES(MT6397_TOPCKPDN2_CHR1M_CK, "top2-chr1m",
		MT6397_TOP_CKPDN2, BIT(8)),
	GATES(MT6397_TOPCKPDN2_BUCK_CK, "top2-buck",
		MT6397_TOP_CKPDN2, BIT(9)),
	GATES(MT6397_TOPCKPDN2_BUCK_ANA_CK, "top2-buckana",
		MT6397_TOP_CKPDN2, BIT(10)),
	GATES(MT6397_TOPCKPDN2_BUCK32K, "top2-buck32k",
		MT6397_TOP_CKPDN2, BIT(11)),
	GATES(MT6397_TOPCKPDN2_BUCK_1M_CK, "top2-buck1m",
		MT6397_TOP_CKPDN2, BIT(12)),
	GATES(MT6397_TOPCKPDN2_STRUP_32K_CK, "top2-starup32k",
		MT6397_TOP_CKPDN2, BIT(13)),
	GATES(MT6397_TOPCKPDN2_RTC_75K_CK, "top2-rtc75k",
		MT6397_TOP_CKPDN2, BIT(14)),
	GATES(MT6397_TOPCKPDN2_RSV_15, "top2-rsv",
		MT6397_TOP_CKPDN2, BIT(15)),
	GATES(MT6397_TOPCKPDN3_AUXADC_DIV_CK, "top3-auxadcdiv",
		MT6397_TOP_CKPDN3, BIT(0)),
	GATES(MT6397_TOPCKPDN3_RTC_75K_DIV4_CK, "top3-rtc75kdiv4",
		MT6397_TOP_CKPDN3, BIT(1)),
	GATES(MT6397_WRPCKPDN_I2C0, "wrap-i2c0",
		MT6397_WRP_CKPDN, BIT(0)),
	GATES(MT6397_WRPCKPDN_I2C1, "wrap-i2c1",
		MT6397_WRP_CKPDN, BIT(1)),
	GATES(MT6397_WRPCKPDN_I2C2, "wrap-i2c2",
		MT6397_WRP_CKPDN, BIT(2)),
	GATES(MT6397_WRPCKPDN_PWM, "wrap-pwm",
		MT6397_WRP_CKPDN, BIT(3)),
	GATES(MT6397_WRPCKPDN_KP, "wrap-kp",
		MT6397_WRP_CKPDN, BIT(4)),
	GATES(MT6397_WRPCKPDN_EINT, "wrap-eint",
		MT6397_WRP_CKPDN, BIT(5)),
	GATES(MT6397_WRPCKPDN_32K, "wrap-32k",
		MT6397_WRP_CKPDN, BIT(6)),
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
	struct regmap *regmap;
	size_t num_clks;
	struct mt6397_chip *mt6397 = dev_get_drvdata(pdev->dev.parent);
	struct clk_onecell_data *onecell;

	regmap = mt6397->regmap;
	num_clks = ARRAY_SIZE(mt6397_clks);

	clk = devm_kzalloc(dev, sizeof(struct clk *) * num_clks, GFP_KERNEL);
	if (!clk)
		return -ENOMEM;

	onecell = devm_kzalloc(dev, sizeof(*onecell), GFP_KERNEL);
	if (!onecell)
		return -ENOMEM;

	onecell->clk_num = num_clks;
	for (i = 0; i < num_clks; i++) {
		mt6397_clks[i].regmap = mt6397->regmap;
		clk[i] = devm_clk_register(dev, &mt6397_clks[i].hw);
		if (IS_ERR(clk))
			return PTR_ERR(clk);
	}
	onecell->clks = clk;
	ret = of_clk_add_provider(dev->of_node,
				  of_clk_src_onecell_get, onecell);

	return ret;
}

static int mt6397_clk_remove(struct platform_device *pdev)
{
	of_clk_del_provider(pdev->dev.of_node);

	return 0;
}

static struct platform_driver mt6397_clk_driver = {
	.probe		= mt6397_clk_probe,
	.remove		= mt6397_clk_remove,
	.driver		= {
		.name	= "mt6397-clk",
		.owner	= THIS_MODULE,
		.of_match_table = mt6397_clk_match_table,
	},
};

module_platform_driver(mt6397_clk_driver);

MODULE_AUTHOR("Flora Fu <flora.fu@mediatek.com>");
MODULE_DESCRIPTION("Clock Driver for MediaTek MT6397 PMIC");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:mt6397-clock");
