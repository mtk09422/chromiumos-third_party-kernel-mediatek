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

#ifndef __MFD_MT6397_CORE_H__
#define __MFD_MT6397_CORE_H__

extern const struct regmap_config mt_regmap_config;

struct mt6397_chip {
	/* Device */
	struct device	*dev;

	/* Control interface */
	struct regmap	*regmap;
};

struct mt6397_regulator_data {
	int id;
	const char *name;
	struct regulator_init_data *initdata;
	struct device_node *reg_node;
};

struct mt6397_platform_data {
	struct mt6397_regulator_data *regulators;
	unsigned int num_regulators;
};

#endif /* __MFD_MT6397_CORE_H__ */

