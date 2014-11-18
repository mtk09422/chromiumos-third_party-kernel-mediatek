/*
 * Device Tree support for Mediatek SoCs
 *
 * Copyright (c) 2014 MundoReader S.L.
 * Author: Matthias Brugger <matthias.bgg@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#include <linux/init.h>
#include <asm/mach/arch.h>
#include <linux/of.h>
#include <linux/clk-provider.h>
#include <linux/clocksource.h>
#include <linux/of_address.h>
#include <linux/io.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/irqchip.h>
#include <linux/irqchip/arm-gic.h>


#define GPT6_CON_MT65xx 0x10008060
#define GIC_HW_IRQ_BASE  32
#define INT_POL_INDEX(a)   ((a) - GIC_HW_IRQ_BASE)

static void __iomem *int_pol_base;

static void __init mediatek_timer_init(void)
{
	static void __iomem *gpt_base;

	/* turn on GPT6 which ungates arch timer clocks */
	if (of_machine_is_compatible("mediatek,mt6589") ||
			of_machine_is_compatible("mediatek,mt8135"))
		gpt_base = ioremap(GPT6_CON_MT65xx, 0x04);

	/* enabel clock and set to free-run */
	if (gpt_base)
		writel(0x31, gpt_base);

	of_clk_init(NULL);
	clocksource_of_init();
};


static int mtk_int_pol_set_type(struct irq_data *d, unsigned int type)
{
	unsigned int irq = d->hwirq;
	u32 offset, reg_index, value;

	offset = INT_POL_INDEX(irq) & 0x1F;
	reg_index = INT_POL_INDEX(irq) >> 5;

	/* This arch extension was called with irq_controller_lock held,
	   so the read-modify-write will be atomic */
	value = readl(int_pol_base + reg_index * 4);
	if (type == IRQ_TYPE_LEVEL_LOW || type == IRQ_TYPE_EDGE_FALLING)
		value |= (1 << offset);
	else
		value &= ~(1 << offset);
	writel(value, int_pol_base + reg_index * 4);

	return 0;
}

static void __init mediatek_init_irq(void)
{
	struct device_node *node;

	node = of_find_compatible_node(NULL, NULL, "mediatek,mt6577-intpol");
	int_pol_base = of_io_request_and_map(node, 0, "intpol");
	if (!int_pol_base) {
		pr_warn("Can't get resource\n");
		return;
	}

	gic_arch_extn.irq_set_type = mtk_int_pol_set_type;

	irqchip_init();
}

static const char * const mediatek_board_dt_compat[] = {
	"mediatek,mt6589",
	"mediatek,mt8127",
	"mediatek,mt8135",
	NULL,
};

DT_MACHINE_START(MEDIATEK_DT, "Mediatek Cortex-A7 (Device Tree)")
	.dt_compat	= mediatek_board_dt_compat,
	.init_time	= mediatek_timer_init,
	.init_irq	= mediatek_init_irq,
MACHINE_END
