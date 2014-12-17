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

#include <linux/mfd/syscon.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/reset-controller.h>

struct mtk_reset_data {
	struct regmap *regmap;
	u32 resetbase;
	u32 num_regs;
	struct reset_controller_dev rcdev;
};

static int mtk_reset_assert(struct reset_controller_dev *rcdev,
			      unsigned long id)
{
	struct regmap *regmap;
	u32 addr;
	u32 mask;
	struct mtk_reset_data *data = container_of(rcdev,
						     struct mtk_reset_data,
						     rcdev);
	regmap = data->regmap;
	addr = data->resetbase + ((id / 32) << 2);
	mask = BIT(id % 32);

	return regmap_update_bits(regmap, addr, mask, mask);
}

static int mtk_reset_deassert(struct reset_controller_dev *rcdev,
				unsigned long id)
{
	struct regmap *regmap;
	u32 addr;
	u32 mask;
	struct mtk_reset_data *data = container_of(rcdev,
						     struct mtk_reset_data,
						     rcdev);

	regmap = data->regmap;
	addr = data->resetbase + ((id / 32) << 2);
	mask = BIT(id % 32);

	return regmap_update_bits(regmap, addr, mask, ~mask);
}

static struct reset_control_ops mtk_reset_ops = {
	.assert = mtk_reset_assert,
	.deassert = mtk_reset_deassert,
};

static int mtk_reset_probe(struct platform_device *pdev)
{
	struct mtk_reset_data *data;
	struct device_node *np = pdev->dev.of_node;
	struct device_node *syscon_np;
	u32 reg[2];
	int ret;

	data = devm_kzalloc(&pdev->dev, sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	syscon_np = of_get_parent(np);
	data->regmap = syscon_node_to_regmap(syscon_np);
	of_node_put(syscon_np);
	if (IS_ERR(data->regmap)) {
		dev_err(&pdev->dev, "couldn't get syscon-reset regmap\n");
		return PTR_ERR(data->regmap);
	}
	ret = of_property_read_u32_array(np, "reg", reg, 2);
	if (ret) {
		dev_err(&pdev->dev, "couldn't read reset base from syscon!\n");
		return -EINVAL;
	}

	data->resetbase = reg[0];
	data->num_regs = reg[1] >> 2;
	data->rcdev.owner = THIS_MODULE;
	data->rcdev.nr_resets = data->num_regs * 32;
	data->rcdev.ops = &mtk_reset_ops;
	data->rcdev.of_node = pdev->dev.of_node;

	return reset_controller_register(&data->rcdev);
}

static int mtk_reset_remove(struct platform_device *pdev)
{
	struct mtk_reset_data *data = platform_get_drvdata(pdev);

	reset_controller_unregister(&data->rcdev);

	return 0;
}

static const struct of_device_id mtk_reset_dt_ids[] = {
	{ .compatible = "mediatek,reset", },
	{},
};
MODULE_DEVICE_TABLE(of, mtk_reset_dt_ids);

static struct platform_driver mtk_reset_driver = {
	.probe = mtk_reset_probe,
	.remove = mtk_reset_remove,
	.driver = {
		.name = "mtk-reset",
		.of_match_table = mtk_reset_dt_ids,
	},
};

module_platform_driver(mtk_reset_driver);

MODULE_AUTHOR("Flora Fu <flora.fu@mediatek.com>");
MODULE_DESCRIPTION("MediaTek SoC Generic Reset Controller");
MODULE_LICENSE("GPL");
