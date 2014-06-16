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


#define GPT6_CON_MT65xx 0x10008060

static void __init mediatek_timer_init(void)
{
	static void __iomem *gpt_base;

	if (of_machine_is_compatible("mediatek,mt6589")) {
		/* set cntfreq register which is not done in bootloader */
		asm volatile("mcr p15, 0, %0, c14, c0, 0" : : "r" (13000000));

		/* turn on GPT6 which ungates arch timer clocks */
		gpt_base = ioremap(GPT6_CON_MT65xx, 0x04);
	}

	/* enabel clock and set to free-run */
	if (gpt_base)
		writel(0x31, gpt_base);

	of_clk_init(NULL);
	clocksource_of_init();
};

static const char * const mediatek_board_dt_compat[] = {
	"mediatek,mt6589",
	"mediatek,mt8127",
	NULL,
};

DT_MACHINE_START(MEDIATEK_DT, "Mediatek Cortex-A7 (Device Tree)")
	.dt_compat	= mediatek_board_dt_compat,
	.init_time	= mediatek_timer_init,
MACHINE_END
