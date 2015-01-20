/*
 * mtk_afe_connection.h  --  Mediatek AFE connection support
 *
 * Copyright (c) 2015 MediaTek Inc.
 * Author: Koro Chen <koro.chen@mediatek.com>
 *         Hidalgo Huang <hidalgo.huang@mediatek.com>
 *         Ir Lian <ir.lian@mediatek.com>
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

#include "mtk_afe_common.h"

int mtk_afe_interconn_connect(struct mtk_afe_info *afe_info,
			      uint32_t in, uint32_t out);
int mtk_afe_interconn_connect_shift(struct mtk_afe_info *afe_info,
				    uint32_t in, uint32_t out);
int mtk_afe_interconn_disconnect(struct mtk_afe_info *afe_info,
				 uint32_t in, uint32_t out);

#endif
