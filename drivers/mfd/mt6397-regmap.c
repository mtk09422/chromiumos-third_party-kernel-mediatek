/*
* Copyright (c) 2014 MediaTek Inc.
* Author: Dongdong.Cheng <dongdong.cheng@mediatek.com>
*
*This file is for MT6397 register map
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
#include "mt8135_pmic_wrap.h"

static int pwrap_reg_read(void *context, unsigned int reg, unsigned int *val)
{
	u32 ret;

	ret = pwrap_read(reg, val);
	return ret;
}

static int pwrap_reg_write(void *context, unsigned int reg, unsigned int val)
{
	u32 ret;

	ret = pwrap_write(reg, val);
	return ret;
}

const struct regmap_config mt_regmap_config = {
	.reg_bits = 16,
	.val_bits = 16,
	.reg_read = pwrap_reg_read,
	.reg_write = pwrap_reg_write
};

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Dongdong Cheng <dongdong.cheng@mediatek.com>");
MODULE_DESCRIPTION("Driver for MediaTek MT6397 PMIC Register Map");
MODULE_ALIAS("platform:mt6397");
