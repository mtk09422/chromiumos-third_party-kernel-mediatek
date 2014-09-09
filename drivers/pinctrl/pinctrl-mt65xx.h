/*
* Copyright (c) 2014 MediaTek Inc.
* Author: Hongzhou.Yang <hongzhou.yang@mediatek.com>
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

#ifndef __PINCTRL_MTK_H
#define __PINCTRL_MTK_H

#include <linux/pinctrl/pinctrl.h>
#include <linux/spinlock.h>
#define MT_EDGE_SENSITIVE           0
#define MT_LEVEL_SENSITIVE          1
#define EINT_DBNC_SET_DBNC_BITS    (4)
#define EINT_DBNC_SET_EN_BITS      (0)
#define EINT_DBNC_SET_RST_BITS     (1)
#define EINT_DBNC_EN_BIT           (0x1)
#define EINT_DBNC_RST_BIT          (0x1)
#define EINT_DBNC_SET_EN           (0x1)

struct mt_eint_offsets {
	const char *name;
	unsigned int  stat;
	unsigned int  ack;
	unsigned int  mask;
	unsigned int  mask_set;
	unsigned int  mask_clr;
	unsigned int  sens;
	unsigned int  sens_set;
	unsigned int  sens_clr;
	unsigned int  pol;
	unsigned int  pol_set;
	unsigned int  pol_clr;
	unsigned int  dom_en;
	unsigned int  dbnc_ctrl;
	unsigned int  dbnc_set;
	unsigned int  dbnc_clr;
	u8  port_mask;
	u8  ports;
};

struct mt_desc_function {
	const char *name;
	unsigned char muxval;
	unsigned char irqnum;
};

struct mt_desc_pin {
	struct pinctrl_pin_desc	pin;
	const char *pad;
	const char *chip;
	struct mt_desc_function	*functions;
};

struct mt_pinctrl_desc {
	const struct mt_desc_pin	*pins;
	int				npins;
	int				ap_num;
	int				db_cnt;
};

#define MT_PIN(_pin, _pad, _chip, ...)				\
	{							\
		.pin = _pin,					\
		.pad = _pad,					\
		.chip = _chip,					\
		.functions = (struct mt_desc_function[]){	\
			__VA_ARGS__, { } },			\
	}

#define MT_FUNCTION(_val, _name)				\
	{							\
		.name = _name,					\
		.muxval = _val,					\
		.irqnum = 255,					\
	}

#define MT_FUNCTION_IRQ(_val, _name, _irq)			\
	{							\
		.name = _name,					\
		.muxval = _val,					\
		.irqnum = _irq,					\
	}

struct mt_pinctrl_function {
	const char	*name;
	const char	**groups;
	unsigned	ngroups;
};

struct mt_pinctrl_group {
	const char	*name;
	const char	*pad;
	unsigned long	config;
	unsigned	pin;
};

struct mt_pinctrl {
	void __iomem            *membase1;
	void __iomem            *membase2;
	struct mt_pinctrl_desc  *desc;
	struct device           *dev;
	struct gpio_chip	*chip;
	spinlock_t              lock;
	struct mt_pinctrl_group	*groups;
	unsigned			ngroups;
	struct mt_pinctrl_function	*functions;
	unsigned			nfunctions;
	struct pinctrl_dev      *pctl_dev;
	struct gpio_offsets     *offset;
	void __iomem		*eint_reg_base;
	struct irq_domain	*domain;
};

struct gpio_offsets {
	unsigned int dir_offset;
	unsigned int ies_offset;
	unsigned int pullen_offset;
	unsigned int pullsel_offset;
	unsigned int drv_offset;
	unsigned int invser_offset;
	unsigned int dout_offset;
	unsigned int din_offset;
	unsigned int pinmux_offset;
	unsigned short type1_start;
	unsigned short type1_end;
	struct mt_eint_offsets eint_offsets;
};

#endif /* __PINCTRL_MT65XX_H */
