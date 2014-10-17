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

#include <linux/err.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>
#include <linux/reset-controller.h>
#include <linux/slab.h>

struct infracfg_reset_data {
	void __iomem *membase;
	unsigned int resetbase;
	spinlock_t lock;
	struct reset_controller_dev	rcdev;
};

static int infracfg_reset_assert(struct reset_controller_dev *rcdev,
			      unsigned long id)
{
	void __iomem *addr;
	unsigned int val;
	unsigned long flags;
	struct infracfg_reset_data *data = container_of(rcdev,
						     struct infracfg_reset_data,
						     rcdev);

	spin_lock_irqsave(&data->lock, flags);
	addr = data->membase + data->resetbase + ((id / 32) << 2);
	val = readl(addr);
	val |= 1 << (id % 32);
	writel(val, addr);
	spin_unlock_irqrestore(&data->lock, flags);

	return 0;
}

static int infracfg_reset_deassert(struct reset_controller_dev *rcdev,
				unsigned long id)
{
	void __iomem *addr;
	unsigned int val;
	unsigned long flags;
	struct infracfg_reset_data *data = container_of(rcdev,
						     struct infracfg_reset_data,
						     rcdev);

	spin_lock_irqsave(&data->lock, flags);
	addr = data->membase + data->resetbase + ((id / 32) << 2);
	val = readl(addr);
	val &= ~(1 << (id % 32));
	writel(val, addr);
	spin_unlock_irqrestore(&data->lock, flags);
	return 0;
}

static struct reset_control_ops infracfg_reset_ops = {
	.assert = infracfg_reset_assert,
	.deassert = infracfg_reset_deassert,
};

static int infracfg_reset_probe(struct platform_device *pdev)
{
	struct infracfg_reset_data *data;
	struct resource *res;

	data = devm_kzalloc(&pdev->dev, sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM,
		"reset-infracfg");
	if (!res) {
		dev_err(&pdev->dev, "could not get resource\n");
		return -ENODEV;
	}
	data->membase = devm_ioremap(&pdev->dev, res->start,
		resource_size(res));
	if (!data->membase) {
		dev_err(&pdev->dev, "failed to ioremap infracfg memory\n");
		return -ENOMEM;
	}

	spin_lock_init(&data->lock);
	data->resetbase = 0x30;
	data->rcdev.owner = THIS_MODULE;
	data->rcdev.nr_resets = 64;
	data->rcdev.ops = &infracfg_reset_ops;
	data->rcdev.of_node = pdev->dev.of_node;

	return reset_controller_register(&data->rcdev);
}

static int infracfg_reset_remove(struct platform_device *pdev)
{
	struct infracfg_reset_data *data = platform_get_drvdata(pdev);

	reset_controller_unregister(&data->rcdev);
	return 0;
}

static const struct of_device_id infracfg_reset_dt_ids[] = {
	 { .compatible = "mediatek,mt8135-infracfg", },
	 {},
};
MODULE_DEVICE_TABLE(of, infracfg_reset_dt_ids);

static struct platform_driver infracfg_reset_driver = {
	.probe = infracfg_reset_probe,
	.remove	= infracfg_reset_remove,
	.driver = {
		.name = "reset-infracfg",
		.owner = THIS_MODULE,
		.of_match_table	= infracfg_reset_dt_ids,
	},
};

static int __init infracfg_reset_init(void)
{
	return platform_driver_register(&infracfg_reset_driver);
}
core_initcall(infracfg_reset_init);

static void __exit infracfg_reset_exit(void)
{
	platform_driver_unregister(&infracfg_reset_driver);
}
module_exit(infracfg_reset_exit);

MODULE_AUTHOR("Flora Fu <flora.fu@mediatek.com>");
MODULE_DESCRIPTION("Infrasys Reset Driver");
MODULE_LICENSE("GPL");
