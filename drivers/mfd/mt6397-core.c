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

#include <linux/module.h>
#include <linux/mfd/core.h>
#include <linux/mfd/mt6397/core.h>
#include <linux/of_device.h>
#include "mtk-pmic-wrap.h"
#include "mt6397-irq.h"
#include <linux/of_irq.h>

static struct mfd_cell mt6397_devs[] = {
	{
		.name = "mt6397-rtc",
		.id = -1,
	},
	{
		.name = "mt6397-regulator",
		.of_compatible = "mediatek,mt6397-regulator",
	},
	{
		.name = "mt6397-codec",
		.id = -1,
		.of_compatible = "mediatek,mt6397-codec"
	},
	{
		.name = "mt6397-clk",
		.id = -1,
		.of_compatible = "mediatek,mt6397-clk"
	},
};

static int mt6397_probe(struct platform_device *pdev)
{
	u32 ret;
	struct mt6397_chip *mt6397;

	struct pmic_wrapper *wrp = dev_get_drvdata(pdev->dev.parent);

	mt6397 = devm_kzalloc(&pdev->dev,
			sizeof(struct mt6397_chip), GFP_KERNEL);
	if (!mt6397)
		return -ENOMEM;
	mt6397->dev = &pdev->dev;
	mt6397->regmap = wrp->regmap;
	platform_set_drvdata(pdev, mt6397);

	mt6397_irq_init(mt6397);

	ret = mfd_add_devices(mt6397->dev, -1, &mt6397_devs[0],
			ARRAY_SIZE(mt6397_devs), NULL, 0, NULL);
	if (ret < 0)
		dev_err(mt6397->dev, "Failed to mfd_add_devices: %d\n", ret);

	return ret;
}

static int mt6397_remove(struct platform_device *pdev)
{
	struct mt6397_chip *mt6397 = platform_get_drvdata(pdev);

	mfd_remove_devices(mt6397->dev);
	return 0;
}

static const struct of_device_id mt6397_of_match[] = {
	{ .compatible = "mediatek,mt6397" },
	{ }
};
MODULE_DEVICE_TABLE(of, mt6397_of_match);

static struct platform_driver mt6397_driver = {
	.probe	= mt6397_probe,
	.remove = mt6397_remove,
	.driver = {
		.name = "mt6397",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(mt6397_of_match),
	},
};

module_platform_driver(mt6397_driver);

MODULE_AUTHOR("Flora Fu <flora.fu@mediatek.com>");
MODULE_DESCRIPTION("Driver for MediaTek MT6397 PMIC");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:mt6397");
