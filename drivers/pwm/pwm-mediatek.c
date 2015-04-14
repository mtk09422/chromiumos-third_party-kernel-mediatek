/*
 * Mediatek pulse-width-modulation controller driver.
 * Copyright (c) 2015 MediaTek Inc.
 * Author: YH Huang <yh.huang@mediatek.com>
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

#include <linux/clk.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/pwm.h>
#include <linux/platform_device.h>
#include <linux/slab.h>

#define DISP_PWM_EN_OFF			(0x0)
#define PWM_ENABLE_SHIFT		(0x0)
#define PWM_ENABLE_MASK			(0x1 << PWM_ENABLE_SHIFT)

#define DISP_PWM_COMMIT_OFF		(0x08)
#define PWM_COMMIT_SHIFT		(0x0)
#define PWM_COMMIT_MASK			(0x1 << PWM_COMMIT_SHIFT)

#define DISP_PWM_CON_0_OFF		(0x10)
#define PWM_CLKDIV_SHIFT		(0x10)
#define PWM_CLKDIV_MASK			(0x3ff << PWM_CLKDIV_SHIFT)
#define PWM_CLKDIV_MAX			(0x000003ff)

#define DISP_PWM_CON_1_OFF		(0x14)
#define PWM_PERIOD_SHIFT		(0x0)
#define PWM_PERIOD_MASK			(0xfff << PWM_PERIOD_SHIFT)
#define PWM_PERIOD_MAX			(0x00000fff)

#define PWM_HIGH_WIDTH_SHIFT		(0x10)
#define PWM_HIGH_WIDTH_MASK		(0x1fff << PWM_HIGH_WIDTH_SHIFT)

#define NUM_PWM 1

/* Shift log2(PWM_PERIOD_MAX + 1) as divisor */
#define PERIOD_BIT_SHIFT 12

struct mtk_pwm_chip {
	struct pwm_chip	chip;
	struct device	*dev;
	struct clk	*clk_pwm_sel;
	struct clk	*clk_disp_pwm0mm;
	struct clk	*clk_disp_pwm026m;
	void __iomem	*mmio_base;
};

static void mtk_pwm_setting(void __iomem *address, u32 value, u32 mask)
{
	u32 val;

	val = readl(address);
	val &= ~mask;
	val |= value;
	writel(val, address);
}

static int mtk_pwm_config(struct pwm_chip *chip, struct pwm_device *pwm,
			  int duty_ns, int period_ns)
{
	struct mtk_pwm_chip *mpc;
	u64 div, rate;
	u32 clk_div, period, high_width, rem;

	/*
	 * Find period, high_width and clk_div to suit duty_ns and period_ns.
	 * Calculate proper div value to keep period value in the bound.
	 *
	 * period_ns = 10^9 * (clk_div + 1) * (period +1) / PWM_CLK_RATE
	 * duty_ns = 10^9 * (clk_div + 1) * (high_width + 1) / PWM_CLK_RATE
	 *
	 * period = (PWM_CLK_RATE * period_ns) / (10^9 * (clk_div + 1)) - 1
	 * high_width = (PWM_CLK_RATE * duty_ns) / (10^9 * (clk_div + 1)) - 1
	 */
	mpc = container_of(chip, struct mtk_pwm_chip, chip);
	rate = clk_get_rate(mpc->clk_disp_pwm026m);
	clk_div = div_u64_rem(rate * period_ns, NSEC_PER_SEC, &rem) >>
				PERIOD_BIT_SHIFT;
	if (clk_div > PWM_CLKDIV_MAX)
		return -EINVAL;

	div = clk_div + 1;
	period = div64_u64(rate * period_ns, NSEC_PER_SEC * div);
	if (period > 0)
		period--;
	high_width = div64_u64(rate * duty_ns, NSEC_PER_SEC * div);
	if (high_width > 0)
		high_width--;

	mtk_pwm_setting(mpc->mmio_base + DISP_PWM_CON_0_OFF,
			clk_div << PWM_CLKDIV_SHIFT, PWM_CLKDIV_MASK);
	mtk_pwm_setting(mpc->mmio_base + DISP_PWM_CON_1_OFF,
			(period << PWM_PERIOD_SHIFT) |
			(high_width << PWM_HIGH_WIDTH_SHIFT),
			PWM_PERIOD_MASK | PWM_HIGH_WIDTH_MASK);

	mtk_pwm_setting(mpc->mmio_base + DISP_PWM_COMMIT_OFF,
			1 << PWM_COMMIT_SHIFT, PWM_COMMIT_MASK);
	mtk_pwm_setting(mpc->mmio_base + DISP_PWM_COMMIT_OFF,
			0 << PWM_COMMIT_SHIFT, PWM_COMMIT_MASK);

	return 0;
}

static int mtk_pwm_enable(struct pwm_chip *chip, struct pwm_device *pwm)
{
	struct mtk_pwm_chip *mpc;

	mpc = container_of(chip, struct mtk_pwm_chip, chip);
	mtk_pwm_setting(mpc->mmio_base + DISP_PWM_EN_OFF,
			1 << PWM_ENABLE_SHIFT, PWM_ENABLE_MASK);

	return 0;
}

static void mtk_pwm_disable(struct pwm_chip *chip, struct pwm_device *pwm)
{
	struct mtk_pwm_chip *mpc;

	mpc = container_of(chip, struct mtk_pwm_chip, chip);
	mtk_pwm_setting(mpc->mmio_base + DISP_PWM_EN_OFF,
			0 << PWM_ENABLE_SHIFT, PWM_ENABLE_MASK);
}

static const struct pwm_ops mtk_pwm_ops = {
	.config = mtk_pwm_config,
	.enable = mtk_pwm_enable,
	.disable = mtk_pwm_disable,
	.owner = THIS_MODULE,
};

static int mtk_pwm_probe(struct platform_device *pdev)
{
	struct mtk_pwm_chip *pwm;
	struct resource *r;
	int ret;

	pwm = devm_kzalloc(&pdev->dev, sizeof(*pwm), GFP_KERNEL);
	if (!pwm)
		return -ENOMEM;

	pwm->dev = &pdev->dev;

	r = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	pwm->mmio_base = devm_ioremap_resource(&pdev->dev, r);
	if (IS_ERR(pwm->mmio_base))
		return PTR_ERR(pwm->mmio_base);

	pwm->clk_pwm_sel = devm_clk_get(&pdev->dev, "pwm_sel");
	if (IS_ERR(pwm->clk_pwm_sel))
		return PTR_ERR(pwm->clk_pwm_sel);
	pwm->clk_disp_pwm0mm = devm_clk_get(&pdev->dev, "mm_disp_pwm0mm");
	if (IS_ERR(pwm->clk_disp_pwm0mm))
		return PTR_ERR(pwm->clk_disp_pwm0mm);
	pwm->clk_disp_pwm026m = devm_clk_get(&pdev->dev, "mm_disp_pwm026m");
	if (IS_ERR(pwm->clk_disp_pwm026m))
		return PTR_ERR(pwm->clk_disp_pwm026m);

	ret = clk_prepare_enable(pwm->clk_pwm_sel);
	if (ret < 0)
		return ret;
	ret = clk_prepare_enable(pwm->clk_disp_pwm0mm);
	if (ret < 0) {
		clk_disable_unprepare(pwm->clk_pwm_sel);
		return ret;
	}
	ret = clk_prepare_enable(pwm->clk_disp_pwm026m);
	if (ret < 0) {
		clk_disable_unprepare(pwm->clk_pwm_sel);
		clk_disable_unprepare(pwm->clk_disp_pwm0mm);
		return ret;
	}

	platform_set_drvdata(pdev, pwm);

	pwm->chip.dev = &pdev->dev;
	pwm->chip.ops = &mtk_pwm_ops;
	pwm->chip.base = -1;
	pwm->chip.npwm = NUM_PWM;

	ret = pwmchip_add(&pwm->chip);
	if (ret < 0) {
		dev_err(&pdev->dev, "pwmchip_add() failed: %d\n", ret);
		return ret;
	}

	return 0;
}

static int mtk_pwm_remove(struct platform_device *pdev)
{
	struct mtk_pwm_chip *pc = platform_get_drvdata(pdev);

	if (WARN_ON(!pc))
		return -ENODEV;

	clk_disable_unprepare(pc->clk_pwm_sel);
	clk_disable_unprepare(pc->clk_disp_pwm0mm);
	clk_disable_unprepare(pc->clk_disp_pwm026m);

	return pwmchip_remove(&pc->chip);
}

static const struct of_device_id mtk_pwm_of_match[] = {
	{ .compatible = "mediatek,mt8173-disp-pwm" },
	{ }
};

MODULE_DEVICE_TABLE(of, mtk_pwm_of_match);

static struct platform_driver mtk_pwm_driver = {
	.driver = {
		.name = "mediatek-pwm",
		.owner = THIS_MODULE,
		.of_match_table = mtk_pwm_of_match,
	},
	.probe = mtk_pwm_probe,
	.remove = mtk_pwm_remove,
};

module_platform_driver(mtk_pwm_driver);

MODULE_AUTHOR("YH Huang <yh.huang@mediatek.com>");
MODULE_DESCRIPTION("MediaTek SoC PWM driver");
MODULE_LICENSE("GPL v2");
