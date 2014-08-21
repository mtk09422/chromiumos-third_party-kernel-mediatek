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

#include <linux/export.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/mfd/core.h>
#include <linux/err.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/regmap.h>
#include <linux/mfd/mt6397/core.h>
#include <linux/mfd/mt6397/registers.h>
#include "mt8135_pmic_wrap.h"

static struct mfd_cell mt6397_devs[] = {
	{  .name = "mt6397-pmic", -1},
	{  .name = "mt6397-rtc", -1},
	{  .name = "mt6397-regulator", -1},

};

static int mt6397_probe(struct platform_device *pdev)
{
	u32 ret = 0;
	u32 reg_value = 0;

	struct mt6397_chip *mt6397 = NULL;
	struct mt6397_platform_data *pdata = NULL;

	mt6397 = devm_kzalloc(&pdev->dev,
			sizeof(struct mt6397_chip), GFP_KERNEL);
	if (!mt6397)
		return -ENOMEM;

	mt6397->dev = &pdev->dev;

	pdata = devm_kzalloc(&pdev->dev,
			sizeof(struct mt6397_platform_data), GFP_KERNEL);
	if (!pdata) {
		devm_kfree(&pdev->dev, mt6397);
		return -ENOMEM;
	}

	pdev->dev.platform_data = pdata;

	/* init mt_regmap_config */
	mt6397->regmap = devm_regmap_init(&pdev->dev, NULL,
			pdev->dev.parent, &mt_regmap_config);
	if (IS_ERR(mt6397->regmap)) {
		ret = PTR_ERR(mt6397->regmap);
		dev_err(mt6397->dev, "Failed to allocate register map: %d\n",
			ret);
		goto err;
	}
	platform_set_drvdata(pdev, mt6397);

	/* Read PMIC chip revision */
	if (regmap_read(mt6397->regmap, 0x100, &reg_value) < 0) {
		dev_err(mt6397->dev, "Failed to read Chip IDa, ret=%d\n", ret);
		ret = -ENODEV;
		goto err;
	} else{
		dev_info(mt6397->dev, "Chip ID = 0x%x\n", reg_value);
	}

	/* add mfd devices */
	ret = mfd_add_devices(mt6397->dev, -1, &mt6397_devs[0],
			ARRAY_SIZE(mt6397_devs), NULL, 0, NULL);
	if (ret < 0)
		goto err_mfd;
	return ret;
err_mfd:
	mfd_remove_devices(mt6397->dev);
err:
	devm_kfree(&pdev->dev, pdata);
	devm_kfree(&pdev->dev, mt6397);
	return ret;
}

static int mt6397_remove(struct platform_device *pdev)
{
	struct mt6397_chip *mt6397 = platform_get_drvdata(pdev);
	struct mt6397_platform_data *pdata = dev_get_platdata(mt6397->dev);

	mfd_remove_devices(mt6397->dev);
	devm_kfree(&pdev->dev, pdata);
	devm_kfree(&pdev->dev, mt6397);
	return 0;
}

static int mt6397_suspend(struct platform_device *dev, pm_message_t state)
{
	return 0;
}

static int mt6397_resume(struct platform_device *dev)
{
	return 0;
}

static const struct of_device_id mt6397_of_match[] = {
	{ .compatible = "mediatek,mt6397" },
	{ }
};

static struct platform_driver mt6397_driver = {
	.probe	= mt6397_probe,
	.remove = mt6397_remove,
	.suspend = mt6397_suspend,
	.resume = mt6397_resume,
	.driver = {
		.name = "mt6397",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(mt6397_of_match),
	},
};

static int __init mt6397_init(void)
{
	int ret;

	ret = platform_driver_register(&mt6397_driver);
	if (ret) {
		pr_err("****[mt6397_init] Unable to register driver(%d)\n",
			ret);
		return ret;
	}
	return 0;
}
arch_initcall(mt6397_init);

static void __init mt6397_exit(void)
{
	platform_driver_unregister(&mt6397_driver);
	pr_info("****[mt6397_exit] : DONE !!\n");
}
module_exit(mt6397_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Flora Fu <flora.fu@mediatek.com>");
MODULE_DESCRIPTION("Driver for MediaTek MT6397 PMIC");
MODULE_ALIAS("platform:mt6397");
