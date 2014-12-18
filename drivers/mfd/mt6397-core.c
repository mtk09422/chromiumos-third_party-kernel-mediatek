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

#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/regmap.h>
#include <linux/mfd/core.h>
#include <linux/mfd/mt6397/core.h>
#include <linux/mfd/mt6397/registers.h>

static const struct mfd_cell mt6397_devs[] = {
	{
		.name = "mt6397-rtc",
		.of_compatible = "mediatek,mt6397-rtc",
	},
	{
		.name = "mt6397-regulator",
		.of_compatible = "mediatek,mt6397-regulator",
	},
	{
		.name = "mt6397-codec",
		.of_compatible = "mediatek,mt6397-codec",
	},
	{	.name = "mt6397-clk",
		.of_compatible = "mediatek,mt6397-clk",
	},
};

static const u16 mt6397_status_reg[] = {
	[GRP_INT_STATUS0] = MT6397_INT_STATUS0,
	[GRP_INT_STATUS1] = MT6397_INT_STATUS1,
};

static const u16 mt6397_mask_reg[] = {
	[GRP_INT_CON0] = MT6397_INT_CON0,
	[GRP_INT_CON1] = MT6397_INT_CON1,
};

struct mt6397_irq_data {
	u16 mask;
	enum PMIC_INT_STATUS_GRP group;
};

#define DECLARE_IRQ(idx, _group, _mask)		\
	[(idx)] = { .group = (_group), .mask = (_mask) }

static const struct mt6397_irq_data mt6397_irqs[] = {
	DECLARE_IRQ(RG_INT_STATUS_SPKL_AB, GRP_INT_CON0, 1 << 0),
	DECLARE_IRQ(RG_INT_STATUS_SPKR_AB, GRP_INT_CON0, 1 << 1),
	DECLARE_IRQ(RG_INT_STATUS_SPKL, GRP_INT_CON0, 1 << 2),
	DECLARE_IRQ(RG_INT_STATUS_SPKR, GRP_INT_CON0, 1 << 3),
	DECLARE_IRQ(RG_INT_STATUS_BAT_L, GRP_INT_CON0, 1 << 4),
	DECLARE_IRQ(RG_INT_STATUS_BAT_H, GRP_INT_CON0, 1 << 5),
	DECLARE_IRQ(RG_INT_STATUS_FG_BAT_L, GRP_INT_CON0, 1 << 6),
	DECLARE_IRQ(RG_INT_STATUS_FG_BAT_H, GRP_INT_CON0, 1 << 7),
	DECLARE_IRQ(RG_INT_STATUS_WATCHDOG, GRP_INT_CON0, 1 << 8),
	DECLARE_IRQ(RG_INT_STATUS_PWRKEY, GRP_INT_CON0, 1 << 9),
	DECLARE_IRQ(RG_INT_STATUS_THR_L, GRP_INT_CON0, 1 << 10),
	DECLARE_IRQ(RG_INT_STATUS_THR_H, GRP_INT_CON0, 1 << 11),
	DECLARE_IRQ(RG_INT_STATUS_VBATON_UNDET,	GRP_INT_CON0, 1 << 12),
	DECLARE_IRQ(RG_INT_STATUS_BVALID_DET, GRP_INT_CON0, 1 << 13),
	DECLARE_IRQ(RG_INT_STATUS_CHRDET, GRP_INT_CON0, 1 << 14),
	DECLARE_IRQ(RG_INT_STATUS_OV, GRP_INT_CON0, 1 << 15),

	DECLARE_IRQ(RG_INT_STATUS_LDO, GRP_INT_CON1, 1 << 0),
	DECLARE_IRQ(RG_INT_STATUS_HOMEKEY, GRP_INT_CON1, 1 << 1),
	DECLARE_IRQ(RG_INT_STATUS_ACCDET, GRP_INT_CON1, 1 << 2),
	DECLARE_IRQ(RG_INT_STATUS_AUDIO, GRP_INT_CON1, 1 << 3),
	DECLARE_IRQ(RG_INT_STATUS_RTC, GRP_INT_CON1, 1 << 4),
	DECLARE_IRQ(RG_INT_STATUS_PWRKEY_RSTB, GRP_INT_CON1, 1 << 5),
	DECLARE_IRQ(RG_INT_STATUS_HDMI_SIFM, GRP_INT_CON1, 1 << 6),
	DECLARE_IRQ(RG_INT_STATUS_HDMI_CEC, GRP_INT_CON1, 1 << 7),
	DECLARE_IRQ(RG_INT_STATUS_VCA15, GRP_INT_CON1, 1 << 8),
	DECLARE_IRQ(RG_INT_STATUS_VSRMCA15, GRP_INT_CON1, 1 << 9),
	DECLARE_IRQ(RG_INT_STATUS_VCORE, GRP_INT_CON1, 1 << 10),
	DECLARE_IRQ(RG_INT_STATUS_VGPU, GRP_INT_CON1, 1 << 11),
	DECLARE_IRQ(RG_INT_STATUS_VIO18, GRP_INT_CON1, 1 << 12),
	DECLARE_IRQ(RG_INT_STATUS_VPCA7, GRP_INT_CON1, 1 << 13),
	DECLARE_IRQ(RG_INT_STATUS_VSRMCA7, GRP_INT_CON1, 1 << 14),
	DECLARE_IRQ(RG_INT_STATUS_VDRM, GRP_INT_CON1, 1 << 15),
};

static void mt6397_irq_lock(struct irq_data *data)
{
	struct mt6397_chip *mt6397 = irq_get_chip_data(data->irq);

	mutex_lock(&mt6397->irqlock);
}

static void mt6397_irq_sync_unlock(struct irq_data *data)
{
	int i;
	struct mt6397_chip *mt6397 = irq_get_chip_data(data->irq);

	for (i = 0; i < MT6397_IRQ_GROUP_NR; i++) {
		regmap_write(mt6397->regmap,
			mt6397_mask_reg[i], mt6397->irq_masks_cur[i]);
	}
	mutex_unlock(&mt6397->irqlock);
}

static void mt6397_irq_mask(struct irq_data *data)
{
	struct mt6397_chip *mt6397 = irq_get_chip_data(data->irq);
	const struct mt6397_irq_data *irq_data = &mt6397_irqs[data->hwirq];

	mt6397->irq_masks_cur[irq_data->group] &= ~irq_data->mask;
	regmap_write(mt6397->regmap, mt6397_mask_reg[irq_data->group],
		mt6397->irq_masks_cur[irq_data->group]);
}

static void mt6397_irq_unmask(struct irq_data *data)
{
	struct mt6397_chip *mt6397 = irq_get_chip_data(data->irq);
	const struct mt6397_irq_data *irq_data = &mt6397_irqs[data->hwirq];

	mt6397->irq_masks_cur[irq_data->group] |= irq_data->mask;
	regmap_write(mt6397->regmap, mt6397_mask_reg[irq_data->group],
		mt6397->irq_masks_cur[irq_data->group]);
}

static void mt6397_irq_ack(struct irq_data *data)
{
	struct mt6397_chip *mt6397 = irq_get_chip_data(data->irq);
	const struct mt6397_irq_data *irq_data = &mt6397_irqs[data->hwirq];

	regmap_write(mt6397->regmap,
			mt6397_status_reg[irq_data->group], irq_data->mask);
}

static struct irq_chip mt6397_irq_chip = {
	.name = "mt6397-irq",
	.irq_bus_lock = mt6397_irq_lock,
	.irq_bus_sync_unlock = mt6397_irq_sync_unlock,
	.irq_mask = mt6397_irq_mask,
	.irq_unmask = mt6397_irq_unmask,
	.irq_ack = mt6397_irq_ack,
};

static irqreturn_t mt6397_irq_thread(int irq, void *data)
{
	struct mt6397_chip *mt6397 = data;
	int irq_reg[MT6397_IRQ_GROUP_NR] = {};
	int i, cur_irq, ret;

	for (i = 0; i < MT6397_IRQ_GROUP_NR; i++) {
		ret = regmap_read(mt6397->regmap,
			mt6397_status_reg[i], &irq_reg[i]);
		if (ret > 0) {
			dev_err(mt6397->dev,
				"fail to read interrupt status [0x%x]\n",
				mt6397_status_reg[i]);
			return IRQ_NONE;
		}
	}

	for (i = 0; i < MT6397_IRQ_NR; i++) {
		if (irq_reg[mt6397_irqs[i].group] & mt6397_irqs[i].mask) {
			cur_irq = irq_find_mapping(mt6397->irq_domain, i);
			if (cur_irq)
				handle_nested_irq(cur_irq);

			/* wrtie 1 to status bit to clear the event.  */
			regmap_write(mt6397->regmap,
				mt6397_status_reg[mt6397_irqs[i].group],
				mt6397_irqs[i].mask);
		}
	}

	return IRQ_HANDLED;
}

static int mt6397_irq_domain_map(struct irq_domain *d, unsigned int irq,
					irq_hw_number_t hw)
{
	struct mt6397_chip *mt6397 = d->host_data;

	irq_set_chip_data(irq, mt6397);
	irq_set_chip_and_handler(irq, &mt6397_irq_chip, handle_level_irq);
	irq_set_nested_thread(irq, 1);
	set_irq_flags(irq, IRQF_VALID);
	return 0;
}

static struct irq_domain_ops mt6397_irq_domain_ops = {
	.map = mt6397_irq_domain_map,
};

static int mt6397_irq_init(struct mt6397_chip *mt6397)
{
	int ret, i;

	mutex_init(&mt6397->irqlock);

	/* Mask all interrupt sources */
	for (i = 0; i < MT6397_IRQ_GROUP_NR; i++)
		regmap_write(mt6397->regmap, mt6397_mask_reg[i], 0x0000);

	mt6397->irq_domain = irq_domain_add_linear(mt6397->dev->of_node,
		MT6397_IRQ_NR, &mt6397_irq_domain_ops, mt6397);
	if (!mt6397->irq_domain) {
		dev_err(mt6397->dev, "could not create irq domain\n");
		return -ENOMEM;
	}

	ret = devm_request_threaded_irq(mt6397->dev, mt6397->irq, NULL,
		mt6397_irq_thread, IRQF_ONESHOT, "mt6397-pmic", mt6397);
	if (ret) {
		dev_err(mt6397->dev, "failed to register irq=%d; err: %d\n",
			mt6397->irq, ret);
	}

	return 0;
}

static int mt6397_probe(struct platform_device *pdev)
{
	int ret;
	struct mt6397_chip *mt6397;

	mt6397 = devm_kzalloc(&pdev->dev, sizeof(*mt6397), GFP_KERNEL);
	if (!mt6397)
		return -ENOMEM;

	mt6397->dev = &pdev->dev;
	/*
	 * mt6397 MFD is child device of soc pmic wrapper.
	 * Regmap is set from its parent.
	 */
	mt6397->regmap = dev_get_platdata(&pdev->dev);
	if (!mt6397->regmap)
		return -ENODEV;

	mt6397->irq = platform_get_irq(pdev, 0);
	if (mt6397->irq < 0) {
		dev_err(&pdev->dev, "fail to get irq: %d\n", mt6397->irq);
		return mt6397->irq;
	}

	platform_set_drvdata(pdev, mt6397);

	mt6397_irq_init(mt6397);

	ret = mfd_add_devices(&pdev->dev, -1, mt6397_devs,
			ARRAY_SIZE(mt6397_devs), NULL, 0, NULL);
	if (ret)
		dev_err(&pdev->dev, "failed to add child devices: %d\n", ret);

	return ret;
}

static int mt6397_remove(struct platform_device *pdev)
{
	mfd_remove_devices(&pdev->dev);

	return 0;
}

static const struct of_device_id mt6397_of_match[] = {
	{ .compatible = "mediatek,mt6397" },
	{ }
};
MODULE_DEVICE_TABLE(of, mt6397_of_match);

static struct platform_driver mt6397_driver = {
	.probe = mt6397_probe,
	.remove = mt6397_remove,
	.driver = {
		.name = "mt6397",
		.of_match_table = of_match_ptr(mt6397_of_match),
	},
};

module_platform_driver(mt6397_driver);

MODULE_AUTHOR("Flora Fu <flora.fu@mediatek.com>");
MODULE_DESCRIPTION("Driver for MediaTek MT6397 PMIC");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:mt6397");
