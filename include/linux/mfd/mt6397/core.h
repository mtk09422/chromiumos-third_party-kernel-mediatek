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

#ifndef __MFD_MT6397_CORE_H__
#define __MFD_MT6397_CORE_H__

struct mt6397_chip {
	struct device *dev;
	struct regmap *regmap;
};

#endif /* __MFD_MT6397_CORE_H__ */
