/*
 * mtk_afe_connection.h  --  Mediatek AFE connection support
 *
 * Copyright (c) 2015 MediaTek Inc.
 * Author: Koro Chen <koro.chen@mediatek.com>
 *             Sascha Hauer <s.hauer@pengutronix.de>
 *             Hidalgo Huang <hidalgo.huang@mediatek.com>
 *             Ir Lian <ir.lian@mediatek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef _MTK_AFE_CONNECTION_H_
#define _MTK_AFE_CONNECTION_H_

struct mtk_afe;

int mtk_afe_interconn_connect(struct mtk_afe *afe_info, unsigned int in,
			      unsigned int out, bool rightshift);
int mtk_afe_interconn_disconnect(struct mtk_afe *afe_info, unsigned int in,
				 unsigned int out);

#endif
