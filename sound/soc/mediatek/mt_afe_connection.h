/*
 * mt_afe_connection.h  --  Mediatek AFE connection support
 *
 * Copyright (c) 2014 MediaTek Inc.
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

#ifndef _MT_AFE_CONNECTION_H_
#define _MT_AFE_CONNECTION_H_

#include "mt_afe_common.h"
/*****************************************************************************
 *                E X T E R N A L   R E F E R E N C E
 *****************************************************************************/
int mt_afe_set_connection_state(struct mt_afe_info *afe_info,
				uint32_t conn_state,
				uint32_t in, uint32_t out);

#endif
