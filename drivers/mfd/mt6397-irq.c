/*
 * Copyright (c) 2014 MediaTek Inc.
 * Author: Dongdong Cheng <dongdong.cheng@mediatek.com>
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
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/sched.h>
#include <linux/mfd/mt6397/core.h>
#include <linux/regmap.h>
#include <linux/irqdomain.h>
#include <linux/device.h>
#include "mt6397-irq.h"
#include <linux/mfd/mt6397/registers.h>
#include <linux/platform_device.h>
#include <linux/of_irq.h>

static const u32 mt6397_mask_reg[] = {
	[GRP_INT_STATUS0] = MT6397_INT_CON0,
	[GRP_INT_STATUS1] = MT6397_INT_CON1,
};

struct mt6397_irq_data {
	int mask;
	enum PMIC_INT_STATUS_GRP group;
};

#define DECLARE_IRQ(idx, _group, _mask)		\
	[(idx)] = { .group = (_group), .mask = (_mask) }

static const struct mt6397_irq_data mt6397_irqs[] = {
	DECLARE_IRQ(RG_INT_STATUS_SPKL_AB, GRP_INT_STATUS0, 1 << 0),
	DECLARE_IRQ(RG_INT_STATUS_SPKR_AB, GRP_INT_STATUS0, 1 << 1),
	DECLARE_IRQ(RG_INT_STATUS_SPKL, GRP_INT_STATUS0, 1 << 2),
	DECLARE_IRQ(RG_INT_STATUS_SPKR, GRP_INT_STATUS0, 1 << 3),
	DECLARE_IRQ(RG_INT_STATUS_BAT_L, GRP_INT_STATUS0, 1 << 4),
	DECLARE_IRQ(RG_INT_STATUS_BAT_H, GRP_INT_STATUS0, 1 << 5),
	DECLARE_IRQ(RG_INT_STATUS_FG_BAT_L, GRP_INT_STATUS0, 1 << 6),
	DECLARE_IRQ(RG_INT_STATUS_FG_BAT_H, GRP_INT_STATUS0, 1 << 7),
	DECLARE_IRQ(RG_INT_STATUS_WATCHDOG, GRP_INT_STATUS0, 1 << 8),
	DECLARE_IRQ(RG_INT_STATUS_PWRKEY, GRP_INT_STATUS0, 1 << 9),
	DECLARE_IRQ(RG_INT_STATUS_THR_L, GRP_INT_STATUS0, 1 << 10),
	DECLARE_IRQ(RG_INT_STATUS_THR_H, GRP_INT_STATUS0, 1 << 11),
	DECLARE_IRQ(RG_INT_STATUS_VBATON_UNDET,	GRP_INT_STATUS0, 1 << 12),
	DECLARE_IRQ(RG_INT_STATUS_BVALID_DET, GRP_INT_STATUS0, 1 << 13),
	DECLARE_IRQ(RG_INT_STATUS_CHRDET, GRP_INT_STATUS0, 1 << 14),
	DECLARE_IRQ(RG_INT_STATUS_OV, GRP_INT_STATUS0, 1 << 15),

	DECLARE_IRQ(RG_INT_STATUS_LDO, GRP_INT_STATUS1, 1 << 0),
	DECLARE_IRQ(RG_INT_STATUS_HOMEKEY, GRP_INT_STATUS1, 1 << 1),
	DECLARE_IRQ(RG_INT_STATUS_ACCDET, GRP_INT_STATUS1, 1 << 2),
	DECLARE_IRQ(RG_INT_STATUS_AUDIO, GRP_INT_STATUS1, 1 << 3),
	DECLARE_IRQ(RG_INT_STATUS_RTC, GRP_INT_STATUS1, 1 << 4),
	DECLARE_IRQ(RG_INT_STATUS_PWRKEY_RSTB, GRP_INT_STATUS1, 1 << 5),
	DECLARE_IRQ(RG_INT_STATUS_HDMI_SIFM, GRP_INT_STATUS1, 1 << 6),
	DECLARE_IRQ(RG_INT_STATUS_HDMI_CEC, GRP_INT_STATUS1, 1 << 7),
	DECLARE_IRQ(RG_INT_STATUS_VCA15, GRP_INT_STATUS1, 1 << 8),
	DECLARE_IRQ(RG_INT_STATUS_VSRMCA15, GRP_INT_STATUS1, 1 << 9),
	DECLARE_IRQ(RG_INT_STATUS_VCORE, GRP_INT_STATUS1, 1 << 10),
	DECLARE_IRQ(RG_INT_STATUS_VGPU, GRP_INT_STATUS1, 1 << 11),
	DECLARE_IRQ(RG_INT_STATUS_VIO18, GRP_INT_STATUS1, 1 << 12),
	DECLARE_IRQ(RG_INT_STATUS_VPCA7, GRP_INT_STATUS1, 1 << 13),
	DECLARE_IRQ(RG_INT_STATUS_VSRMCA7, GRP_INT_STATUS1, 1 << 14),
	DECLARE_IRQ(RG_INT_STATUS_VDRM, GRP_INT_STATUS1, 1 << 15),
};

static void mt6397_irq_lock(struct irq_data *data)
{
	struct mt6397_chip *mt6397 = irq_get_chip_data(data->irq);

	mutex_lock(&mt6397->irqlock);
}

static void mt6397_irq_sync_unlock(struct irq_data *data)
{
	struct mt6397_chip *mt6397 = irq_get_chip_data(data->irq);
	int i;

	for (i = 0; i < MT6397_IRQ_GROUP_NR; i++) {
		mt6397->irq_masks_cache[i] = mt6397->irq_masks_cur[i];
		regmap_write(mt6397->regmap,
			mt6397_mask_reg[i], mt6397->irq_masks_cur[i]);
	}

	mutex_unlock(&mt6397->irqlock);
}

static void mt6397_irq_mask(struct irq_data *data)
{
	struct mt6397_chip *mt6397 = irq_get_chip_data(data->irq);
	const struct mt6397_irq_data *irq_data = &mt6397_irqs[data->hwirq];

	mt6397->irq_masks_cur[irq_data->group] |= irq_data->mask;
}

static void mt6397_irq_unmask(struct irq_data *data)
{
	struct mt6397_chip *mt6397 = irq_get_chip_data(data->irq);
	const struct mt6397_irq_data *irq_data = &mt6397_irqs[data->hwirq];

	mt6397->irq_masks_cur[irq_data->group] &= ~irq_data->mask;
}

static struct irq_chip mt6397_irq_chip = {
	.name = "mt6397-irq",
	.irq_bus_lock = mt6397_irq_lock,
	.irq_bus_sync_unlock = mt6397_irq_sync_unlock,
	.irq_mask = mt6397_irq_mask,
	.irq_unmask = mt6397_irq_unmask,
};

static irqreturn_t mt6397_irq_thread(int irq, void *data)
{
	struct mt6397_chip *mt6397 = data;
	int irq_reg[MT6397_IRQ_GROUP_NR] = {};
	int i, cur_irq;
	struct irq_data *d = irq_get_irq_data(irq);
	struct irq_chip *chip = irq_data_get_irq_chip(d);

	if (regmap_read(mt6397->regmap,
		MT6397_INT_STATUS0, &irq_reg[GRP_INT_STATUS0]) < 0) {
		dev_err(mt6397->dev, "Failed to read INT_STATUS0\n");
		return IRQ_NONE;
	}

	if (regmap_read(mt6397->regmap,
		MT6397_INT_STATUS1, &irq_reg[GRP_INT_STATUS1]) < 0) {
		dev_err(mt6397->dev, "Failed to read INT_STATUS1\n");
		return IRQ_NONE;
	}
	/* Apply masking */
	for (i = 0; i < MT6397_IRQ_GROUP_NR; i++)
		irq_reg[i] &= ~mt6397->irq_masks_cur[i];

	/* Report */
	for (i = 0; i < MT6397_IRQ_NR; i++) {
		if (irq_reg[mt6397_irqs[i].group] & mt6397_irqs[i].mask) {
			cur_irq = irq_find_mapping(mt6397->irq_domain, i);
			if (cur_irq)
				handle_nested_irq(cur_irq);
		}
	}

	if (regmap_write(mt6397->regmap, MT6397_INT_STATUS0, 0xffff) < 0) {
		dev_err(mt6397->dev, "Failed to clear all INT_STATUS0\n");
		return -ENODEV;
	}

	if (regmap_write(mt6397->regmap, MT6397_INT_STATUS1, 0xffff) < 0) {
		dev_err(mt6397->dev, "Failed to clear all INT_STATUS1\n");
		return -ENODEV;
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

int mt6397_irq_init(struct mt6397_chip *mt6397)
{
	struct irq_domain *domain;
	int ret;

	struct platform_device *pdev = to_platform_device(mt6397->dev);
	struct device_node *node = pdev->dev.of_node;

	mutex_init(&mt6397->irqlock);
	mt6397->irq = irq_of_parse_and_map(node, 0);

	/* Mask individual interrupt sources */
	mt6397->irq_masks_cur[GRP_INT_STATUS0] = 0x4200;
	mt6397->irq_masks_cache[GRP_INT_STATUS0] = 0x4200;
	mt6397->irq_masks_cur[GRP_INT_STATUS1] = 0x0010;
	mt6397->irq_masks_cache[GRP_INT_STATUS1] = 0x0010;
	regmap_write(mt6397->regmap, mt6397_mask_reg[GRP_INT_STATUS0], 0x4200);
	regmap_write(mt6397->regmap, mt6397_mask_reg[GRP_INT_STATUS1], 0x0010);

	domain = irq_domain_add_linear(mt6397->dev->of_node, MT6397_IRQ_NR,
		&mt6397_irq_domain_ops, mt6397);

	if (!domain) {
		dev_err(mt6397->dev, "could not create irq domain\n");
		return -ENOMEM;
	}

	mt6397->irq_domain = domain;
	ret = devm_request_threaded_irq(mt6397->dev, mt6397->irq, NULL,
		mt6397_irq_thread, IRQF_ONESHOT, "mt6397-pmic", mt6397);
	if (ret) {
		dev_err(mt6397->dev, "%s: failed to register irq=%d (%d); err: %d\n",
			__func__, mt6397->irq, mt6397->irq, ret);
	}

	return 0;
}

