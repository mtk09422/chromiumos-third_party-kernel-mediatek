/*
 * mtk_afe_connection.c  --  Mediatek AFE connection support
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

#include "mtk_afe_control.h"
#include "mtk_afe_digital_type.h"

/* here define conenction table for input and output */
static const char mtk_afe_conn_tb[MTK_AFE_INTERCONN_NUM_INPUT]
	[MTK_AFE_INTERCONN_NUM_OUTPUT] = {
	/* 0   1   2   3   4   5   6   7   8   9  10  11  12  13  14  15
	    16  17  18  19  20  21  22 */
	{3, 3, 3, 3, 3, 3, -1, 1, 1, 1, 1, -1, -1, 1, 1, 3,
	 3, -1, -1, 3, 3, 1, -1},	/* I00 */
	{3, 3, 3, 3, 3, -1, 3, 1, 1, 1, 1, -1, -1, 1, 1, 3,
	 3, -1, -1, 3, 3, -1, 1},	/* I01 */
	{1, 1, 1, 1, 1, 1, -1, 1, 1, -1, -1, 1, -1, 1, 1, 1,
	 1, -1, -1, -1, -1, -1, -1},	/* I02 */
	{1, 1, 3, 1, 1, 1, -1, 1, -1, 1, -1, -1, -1, 1, 1, 1,
	 1, -1, -1, 1, 1, 1, -1},	/* I03 */
	{1, 1, 3, 1, 1, -1, 1, -1, 1, -1, 1, -1, -1, 1, 1, 1,
	 1, -1, -1, 1, 1, -1, 1},	/* I04 */
	{3, 3, 3, 3, 3, 1, -1, 1, -1, 1, -1, 1, -1, 1, 1, 1,
	 1, -1, -1, 3, 3, 1, -1},	/* I05 */
	{3, 3, 3, 3, 3, -1, 1, -1, 1, -1, 1, -1, 1, 1, 1, 1,
	 1, -1, -1, 3, 3, -1, 1},	/* I06 */
	{3, 3, 3, 3, 3, 1, -1, 1, -1, 1, -1, 1, -1, 1, 1, 1,
	 1, -1, -1, 3, 3, 1, -1},	/* I07 */
	{3, 3, 3, 3, 3, -1, 1, -1, 1, -1, 1, -1, 1, 1, 1, 1,
	 1, -1, -1, 3, 3, -1, 1},	/* I08 */
	{1, 1, 1, 1, 1, 1, -1, -1, -1, 1, -1, -1, 1, 1, 1,
	 1, 1, -1, -1, 1, 1, -1, -1},	/* I09 */
	{3, -1, 3, 3, -1, 1, -1, 1, -1, 1, 1, 1, 1, -1, -1, -1,
	 -1, -1, -1, 1, -1, 1, 1},	/* I10 */
	{-1, 3, 3, -1, 3, -1, 1, -1, 1, 1, 1, 1, 1, -1, -1, -1,
	 -1, -1, -1, -1, 1, 1, 1},	/* I11 */
	{3, -1, 3, 3, -1, 1, -1, 1, -1, 1, 1, 1, 1, -1, -1, -1,
	 -1, -1, -1, 1, -1, 1, 1},	/* I12 */
	{-1, 3, 3, -1, 3, -1, 1, -1, 1, 1, 1, 1, 1, -1, -1, -1,
	 -1, -1, -1, -1, 1, 1, 1},	/* I13 */
	{-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	 -1, -1, -1, -1, -1, -1, -1},	/* I14 */
	{3, 3, 3, 3, 3, 3, -1, -1, -1, 1, -1, -1, -1, 1, 1, 3,
	 1, -1, -1, -1, -1, 1, -1},	/* I15 */
	{3, 3, 3, 3, 3, -1, 3, -1, -1, -1, 1, -1, -1, 1, 1, 1,
	 3, -1, -1, -1, -1, -1, 1},	/* I16 */
	{1, -1, 3, 1, -1, 1, -1, 1, -1, 1, -1, 1, -1, 1, 1, 1,
	 1, -1, -1, 1, 1, 1, -1},	/* I17 */
	{-1, 1, 3, -1, 1, -1, 1, -1, 1, -1, 1, -1, 1, 1, 1, 1,
	 1, -1, -1, 1, 1, -1, 1},	/* I18 */
	{3, -1, 3, 3, 3, 1, -1, 1, -1, 1, -1, 1, -1, 1, 1, 1,
	 1, -1, -1, 1, 1, 1, -1},	/* I19 */
	{-1, 3, 3, 3, 3, -1, 1, -1, 1, -1, 1, -1, 1, 1, 1, 1,
	 1, -1, -1, 1, 1, -1, 1},	/* I20 */
};

/* connection bits of certain bits */
static const char mtk_afe_conn_bits_tbl[MTK_AFE_INTERCONN_NUM_INPUT]
	[MTK_AFE_INTERCONN_NUM_OUTPUT] = {
	/* 0   1   2   3   4   5   6   7   8   9  10  11  12  13  14  15
	    16  17  18 19 20 21 22 */
	{0, 16, 0, 16, 0, 16, -1, 2, 5, 8, 12, -1, -1, 2, 15, 16,
	 22, -1, -1, 8, 24, 30, -1},	/* I00 */
	{1, 17, 1, 17, 1, -1, 22, 3, 6, 9, 13, -1, -1, 3, 16, 17,
	 23, -1, -1, 10, 26, -1, 2},	/* I01 */
	{2, 18, 2, 18, 2, 17, -1, 21, 22, -1, -1, 6, -1, 4, 17, -1,
	 24, -1, -1, -1, -1, -1, -1},	/* I02 */
	{3, 19, 3, 19, 3, 18, -1, 26, -1, 0, -1, -1, -1, 5, 18, 19,
	 25, -1, -1, 12, 28, 31, -1},	/* I03 */
	{4, 20, 4, 20, 4, -1, 23, -1, 29, -1, 3, -1, -1, 6, 19, 20,
	 26, -1, -1, 13, 29, -1, 3},	/* I04 */
	{5, 21, 5, 21, 5, 19, -1, 27, -1, 1, -1, 7, -1, 16, 20, 17,
	 26, -1, -1, 14, 31, 8, -1},	/* I05 */
	{6, 22, 6, 22, 6, -1, 24, -1, 30, -1, 4, -1, 9, 17, 21, 18,
	 27, -1, -1, 16, 0, -1, 11},	/* I06 */
	{7, 23, 7, 23, 7, 20, -1, 28, -1, 2, -1, 8, -1, 18, 22, 19,
	 28, -1, -1, 18, 2, 9, -1},	/* I07 */
	{8, 24, 8, 24, 8, -1, 25, -1, 31, -1, 5, -1, 10, 19, 23, 20,
	 29, -1, -1, 20, 4, -1, 12},	/* I08 */
	{9, 25, 9, 25, 9, 21, 27, -1, -1, 10, -1, -1, 11, 7, 20, 21,
	 27, -1, -1, 22, 6, -1, -1},	/* I09 */
	{0, -1, 4, 8, -1, 12, -1, 14, -1, 26, 28, 30, 0, -1, -1, -1,
	 -1, -1, -1, 28, -1, 0, -1},	/* I10 */
	{-1, 2, 6, -1, 10, -1, 13, -1, 15, 27, 29, 31, 1, -1, -1, -1,
	 -1, -1, -1, -1, 29, 31, 1},	/* I11 */
	{0, -1, 4, 8, -1, 12, -1, 14, -1, 8, 10, 12, 14, -1, -1, -1,
	 -1, -1, -1, 2, -1, -1, 6},	/* I12 */
	{-1, 2, 6, -1, 10, -1, 13, -1, 15, 9, 11, 13, 15, -1, -1, -1,
	 -1, -1, -1, -1, 3, 4, 7},	/* I13 */
	{-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	 -1, -1, -1, -1, -1, -1, -1},	/* I14 */
	{13, 18, 23, 28, 1, 6, -1, -1, -1, 10, -1, -1, -1, 9, 22, 29,
	 2, -1, -1, 23, -1, 10, -1},	/* I15 */
	{14, 19, 24, 29, 2, -1, 8, -1, -1, -1, 11, -1, -1, 10, 23, 30,
	 3, -1, -1, -1, -1, -1, 13},	/* I16 */
	{0, 19, 27, 0, 2, 22, 8, 26, -1, 30, 11, 2, -1, 11, 24, 21,
	 30, -1, -1, 22, 6, 0, -1},	/* I17 */
	{14, 3, 28, -1, 1, -1, 24, -1, 28, -1, 0, -1, 4, 12, 25, 22,
	 31, -1, -1, 23, 7, -1, 4},	/* I18 */
	{1, 19, 10, 14, 18, 23, 8, 27, -1, 31, -1, 3, -1, 13, 26, 23,
	 0, -1, -1, 24, 28, 1, -1},	/* I19 */
	{14, 4, 12, 16, 20, -1, 25, -1, 29, -1, 1, -1, 5, 14, 27, 24,
	 1, -1, -1, 25, 29, -1, 5},	/* I20 */
};

/* connection shift bits of certain bits */
static const char mtk_afe_shift_conn_bits[MTK_AFE_INTERCONN_NUM_INPUT]
				[MTK_AFE_INTERCONN_NUM_OUTPUT] = {
	/* 0   1   2   3   4   5   6    7  8   9  10  11  12  13  14  15
	    16  17  18  19  20  21  22 */
	{10, 26, 10, 26, 10, 19, -1, -1, -1, -1, -1, -1, -1, -1, -1, 31,
	 25, -1, -1, 9, 25, -1, -1},	/* I00 */
	{11, 27, 11, 27, 11, -1, 20, -1, -1, -1, -1, -1, -1, -1, -1, 16,
	 4, -1, -1, 11, 27, -1, -1},	/* I01 */
	{-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	 -1, -1, -1, -1, -1, -1, -1},	/* I02 */
	{-1, -1, 25, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	 -1, -1, -1, -1, -1, -1, -1},	/* I03 */
	{-1, -1, 26, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	 -1, -1, -1, -1, -1, -1, -1},	/* I04 */
	{12, 28, 12, 28, 12, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	 -1, -1, -1, 15, 30, -1, -1},	/* I05 */
	{13, 29, 13, 29, 13, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	 -1, -1, -1, 17, 1, -1, -1},	/* I06 */
	{14, 30, 14, 30, 14, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	 -1, -1, -1, 19, 3, -1, -1},	/* I07 */
	{15, 31, 15, 31, 15, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	 -1, -1, -1, 21, 5, -1, -1},	/* I08 */
	{-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	 -1, -1, -1, -1, -1, -1, -1},	/* I09 */
	{1, -1, 5, 9, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	 -1, -1, -1, -1, -1, -1, -1},	/* I10 */
	{-1, 3, 7, -1, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	 -1, -1, -1, -1, -1, -1, -1},	/* I11 */
	{1, -1, 5, 9, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	 -1, -1, -1, -1, -1, -1, -1},	/* I12 */
	{-1, 3, 7, -1, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	 -1, -1, -1, -1, -1, -1, -1},	/* I13 */
	{-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	 -1, -1, -1, -1, -1, -1, -1},	/* I14 */
	{15, 20, 25, 30, 3, 7, -1, -1, -1, 10, -1, -1, -1, -1, -1,
	 0, 2, -1, -1, -1, -1, -1, -1},	/* I15 */
	{16, 21, 26, 31, 4, -1, 9, -1, -1, -1, 11, -1, -1, -1, -1, 30,
	 5, -1, -1, -1, -1, -1, -1},	/* I16 */
	{14, 19, 30, 29, 2, -1, 8, -1, -1, -1, 11, -1, -1, -1, -1, 30,
	 3, -1, -1, -1, -1, -1, -1},	/* I17 */
	{14, 19, 31, 29, 2, -1, 8, -1, -1, -1, 11, -1, -1, -1, -1, 30,
	 3, -1, -1, -1, -1, -1, -1},	/* I18 */
	{2, 19, 11, 16, 19, -1, 8, -1, -1, -1, 11, -1, -1, -1, -1, 30,
	 3, -1, -1, -1, -1, -1, -1},	/* I19 */
	{14, 5, 13, 17, 21, -1, 8, -1, -1, -1, 11, -1, -1, -1, -1, 30,
	 3, -1, -1, -1, -1, -1, -1},	/* I20 */
};

/* connection of register */
static const short mtk_afe_conn_reg[MTK_AFE_INTERCONN_NUM_INPUT]
	[MTK_AFE_INTERCONN_NUM_OUTPUT] = {
	/* I00 */
	{0x20, 0x20, 0x24, 0x24, 0x28, 0x28, -1, 0x5c, -1, 0x5c, 0x5c, -1,
	 -1, 0x448, 0x448, 0x438, 0x438, 0x5c, 0x5c, 0x464, 0x464, -1, -1},
	/* I01 */
	{0x20, 0x20, 0x24, 0x24, 0x28, -1, 0x28, 0x5c, 0x5c, 0x5c, 0x5c,
	 -1, -1, 0x448, 0x448, 0x438, 0x438, 0x5c, 0x5c, 0x464, 0x464, -1},
	/* I02 */
	{-1, 0x20, 0x24, 0x24, -1, -1, 0x30, 0x30, -1, -1, -1, 0x2c,
	 -1, 0x448, 0x448, -1, -1, 0x30, 0x30, -1, -1, -1, -1},
	/* I03 */
	{0x20, 0x20, 0x24, 0x24, 0x28, 0x28, -1, 0x28, -1, 0x2C, -1, -1,
	 -1, 0x448, 0x448, 0x438, 0x438, 0x30, -1, 0x464, 0x464, 0x5c, -1},
	/* I04 */
	{0x20, 0x20, 0x24, 0x24, 0x28, -1, 0x28, -1, 0x28, -1, 0x2C, -1,
	 -1, 0x448, 0x448, 0x438, 0x438, -1, 0x30, 0x464, 0x464, -1, 0xbc},
	/* I05 */
	{0x20, 0x20, 0x24, 0x24, 0x28, 0x28, -1, 0x28, -1, 0x2C, -1, -1,
	 -1, 0x420, 0x420, -1, -1, 0x30, -1, 0x464, 0x464, -1, -1},
	/* I06 */
	{0x20, 0x20, 0x24, 0x24, 0x28, -1, 0x28, -1, 0x28, -1, 0x2C, -1,
	 0x2C, 0x420, 0x420, -1, -1, -1, 0x30, 0x464, -1, -1, -1},
	/* I07 */
	{0x20, 0x20, 0x24, 0x24, 0x28, 0x28, -1, 0x28, -1, 0x2C, -1, -1,
	 -1, 0x420, 0x420, -1, -1, 0x30, -1, 0x464, -1, -1, -1},
	/* I08 */
	{0x20, 0x20, 0x24, 0x24, 0x28, -1, 0x28, -1, 0x28, -1, 0x2C, -1,
	 0x2C, 0x420, 0x420, -1, -1, -1, 0x30, 0x464, -1, -1, -1},
	/* I09 */
	{0x20, 0x20, 0x24, 0x24, 0x28, 0x28, -1, -1, -1, 0x5c, -1, -1,
	 0x2C, 0x448, 0x448, 0x438, 0x438, 0x5c, 0x5c, 0x5c, 0x5c, -1, -1},
	/* I10 */
	{0x420, -1, -1, 0x420, -1, 0x420, -1, 0x420, -1, -1, -1, -1,
	 0x448, -1, -1, -1, -1, 0x420, -1, 0x448, -1, 0x448, 0x44c},
	/* I11 */
	{-1, 0x420, -1, -1, 0x420, -1, 0x420, -1, 0x420, -1, -1, -1,
	 0x448, -1, -1, -1, -1, -1, 0x420, -1, 0x448, 0x448, 0x44c},
	/* I12 */
	{0x438, 0x438, -1, 0x438, -1, 0x438, -1, 0x438, -1, -1, -1, -1,
	 -1, -1, -1, -1, -1, 0x440, -1, 0x444, -1, 0x444, 0x444},
	/* I13 */
	{-1, 0x438, -1, -1, 0x438, -1, 0x438, -1, 0x438, -1, -1, -1,
	 -1, -1, -1, -1, -1, -1, 0x440, -1, 0x444, 0x444, 0x444},
	/* I14 */
	{0x2C, 0x2C, 0x2C, 0x2C, 0x30, 0x30, -1, 0x5c, 0x5c, 0x5c, -1,
	 -1, 0x30, 0x448, 0x448, 0x438, 0x440, -1, -1, 0x5c, 0x5c, -1, -1},
	/* I15 */
	{0x2C, 0x2C, 0x2C, 0x2C, 0x30, 0x30, -1, -1, -1, 0x30, -1, -1,
	 -1, 0x448, 0x448, 0x438, 0x440, -1, -1, -1, -1, -1, -1},
	/* I16 */
	{0x2C, 0x2C, 0x2C, 0x2C, 0x30, -1, 0x30, -1, -1, -1, 0x30, -1,
	 -1, 0x448, 0x448, 0x438, 0x440, -1, -1, -1, -1, -1, -1},
	/* I17 */
	{0x460, 0x2C, 0x2C, 0x5c, 0x30, 0x460, 0x30, 0x460, -1, 0x460, 0x30,
	 0x464, -1, 0x448, 0x448, 0x438, 0x440, 0x5c, -1, 0x464, -1, 0xbc, -1},
	/* I18 */
	{0x2C, 0x460, 0x2C, 0x2C, 0x5c, -1, 0x460, -1, 0x460, -1, 0x464,
	 -1, 0x464, 0x448, 0x448, 0x438, 0x440, -1, 0x5c, 0x464, -1, -1, 0xbc},
	/* I19 */
	{0x460, 0x2C, 0x460, 0x460, 0x460, 0x460, 0x30, 0x460, -1, 0x460,
	 0x30, 0x464, -1, 0x448, 0x448, 0x438, 0x444, 0x464, -1, 0x5c, 0x5c,
	 0xbc, -1},
	/* I20 */
	{0x2C, 0x460, 0x460, 0x460, 0x460, -1, 0x460, -1, 0x460, -1,
	 0x464, -1, 0x464, 0x448, 0x448, 0x438, 0x444, -1, 0x464, 0x5c, 0x5c,
	 -1, 0xbc},
};

/* shift connection of register */
static const short mtk_afe_shift_conn_reg[MTK_AFE_INTERCONN_NUM_INPUT]
				[MTK_AFE_INTERCONN_NUM_OUTPUT] = {
	{0x20, 0x20, 0x24, 0x24, 0x28, 0x30, -1, -1, -1, -1, -1, -1, -1,
	 -1, -1, 0x438, -1, -1, -1, 0x464, 0x464, -1, -1},	/* I00 */
	{0x20, 0x20, 0x24, 0x24, 0x28, -1, 0x30, -1, -1, -1, -1, -1, -1,
	 -1, -1, -1, 0x440, -1, -1, 0x464, 0x464, -1, -1},	/* I01 */
	{-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	 -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},	/* I02 */
	{-1, -1, 0x30, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	 -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},	/* I03 */
	{-1, -1, 0x30, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	 -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},	/* I04 */
	{0x20, 0x20, 0x24, 0x24, 0x28, -1, -1, -1, -1, -1, -1, 0x2C, -1,
	 -1, -1, -1, -1, -1, -1, 0x464, 0x464, -1, -1},	/* I05 */
	{0x20, 0x20, 0x24, 0x24, 0x28, -1, -1, -1, -1, -1, -1, -1, -1,
	 -1, -1, -1, -1, -1, -1, 0x464, -1, -1, -1},	/* I06 */
	{0x20, 0x20, 0x24, 0x24, 0x28, -1, -1, -1, -1, -1, -1, 0x2C, -1,
	 -1, -1, -1, -1, -1, -1, 0x464, -1, -1, -1},	/* I07 */
	{0x20, 0x20, 0x24, 0x24, 0x28, -1, -1, -1, -1, -1, -1, -1, -1,
	 -1, -1, -1, -1, -1, -1, 0x464, -1, -1, -1},	/* I08 */
	{-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	 -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},	/* I09 */
	{0x420, -1, -1, 0x420, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	 -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},	/* I10 */
	{0x420, 0x420, -1, -1, 0x420, -1, -1, -1, -1, -1, -1, -1, -1,
	 -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},	/* I11 */
	{0x438, 0x438, -1, 0x438, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	 -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},	/* I12 */
	{-1, 0x438, -1, -1, 0x438, -1, -1, -1, -1, -1, -1, -1, -1,
	 -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},	/* I13 */
	{-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	 -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},	/* I14 */
	{0x2C, 0x2C, -1, 0x2C, 0x30, 0x30, -1, -1, -1, -1, -1, -1, -1,
	 -1, -1, 0x440, -1, -1, -1, -1, -1, -1, -1},	/* I15 */
	{0x2C, 0x2C, -1, 0x2C, 0x30, -1, 0x30, -1, -1, -1, -1, -1, -1,
	 -1, -1, -1, 0x440, -1, -1, -1, -1, -1, -1},	/* I16 */
	{0x2C, 0x2C, 0xbc, 0x2C, 0x30, -1, 0x30, -1, -1, -1, -1, -1, -1,
	 -1, -1, -1, 0x440, -1, -1, -1, -1, -1, -1},	/* I17 */
	{0x2C, 0x2C, 0xbc, 0x2C, 0x30, -1, 0x30, -1, -1, -1, -1, -1, -1,
	 -1, -1, -1, 0x440, -1, -1, -1, -1, -1, -1},	/* I18 */
	{0x460, 0x2C, 0x460, 0x460, 0x460, -1, 0x30, -1, -1, -1, -1, -1, -1,
	 -1, -1, -1, 0x440, -1, -1, -1, -1, -1, -1},	/* I19 */
	{0x2C, 0x460, 0x460, 0x460, 0x460, -1, 0x30, -1, -1, -1, -1, -1, -1,
	 -1, -1, -1, 0x440, -1, -1, -1, -1, -1, -1},	/* I20 */
};

/* connection state of register */
static char mtk_afe_reg_sta[MTK_AFE_INTERCONN_NUM_INPUT]
			   [MTK_AFE_INTERCONN_NUM_OUTPUT];

/* value of connection register to set for certain input */
static const uint32_t mtk_afe_hdmi_conn_value[MTK_AFE_NUM_HDMI_INPUT] = {
	/* I_30 I_31 I_32 I_33 I_34 I_35 I_36 I_37 */
	0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7
};

/* mask of connection register to set for certain output */
static const uint32_t mtk_afe_hdmi_conn_mask[MTK_AFE_NUM_HDMI_OUTPUT] = {
	/* O_30 O_31 O_32 O_33 O_34 O_35 O_36 O_37 O_38 O_39 O_40 O_41 */
	0x7, 0x7, 0x7, 0x7, 0x7, 0x7, 0x7, 0x7, 0x7, 0x7, 0x7, 0x7
};

/* shift bits of connection register to set for certain output */
static const char mtk_afe_hdmi_conn_shift_bits[MTK_AFE_NUM_HDMI_OUTPUT] = {
	/* O_30 O_31 O_32 O_33 O_34 O_35 O_36 O_37 O_38 O_39 O_40 O_41 */
	0, 3, 6, 9, 12, 15, 18, 21, 24, 27, 0, 3
};

/* connection reg of HDMI */
static const uint32_t hdmi_connection_reg[MTK_AFE_NUM_HDMI_OUTPUT] = {
	/* O_30 O_31 O_32 O_33 */
	AFE_HDMI_CONN0, AFE_HDMI_CONN0, AFE_HDMI_CONN0, AFE_HDMI_CONN0,
	/* O_34 O_35 O_36 O_37 */
	AFE_HDMI_CONN0, AFE_HDMI_CONN0, AFE_HDMI_CONN0, AFE_HDMI_CONN0,
	/* O_38 O_39 O_40 O_41 */
	AFE_HDMI_CONN0, AFE_HDMI_CONN0, AFE_HDMI_CONN1, AFE_HDMI_CONN1
};

static bool mtk_afe_interconn_check(struct mtk_afe_info *afe_info,
				    short regaddr, char bits)
{
	if (regaddr <= 0 || bits < 0) {
		dev_notice(afe_info->dev,
			   "regaddr = %x bits = %d\n", regaddr, bits);
		return false;
	}
	return true;
}

static int mtk_afe_interconn_hdmi(struct mtk_afe_info *afe_info,
				  uint32_t in, uint32_t out, bool conn)
{
	uint32_t input_idx;
	uint32_t out_idx;
	uint32_t reg_val, reg_mask, reg_shift, reg_conn;

	/* check if connection request is valid */
	if (in < MTK_AFE_HDMI_CONN_INPUT_BASE ||
	    in > MTK_AFE_HDMI_CONN_INPUT_MAX ||
	    out < MTK_AFE_HDMI_CONN_OUTPUT_BASE ||
	    out > MTK_AFE_HDMI_CONN_OUTPUT_MAX) {
		return -EINVAL;
	}

	input_idx = in - MTK_AFE_HDMI_CONN_INPUT_BASE;
	out_idx = out - MTK_AFE_HDMI_CONN_OUTPUT_BASE;

	/* do connection */
	if (conn)
		reg_val = mtk_afe_hdmi_conn_value[input_idx];
	else
		reg_val = mtk_afe_hdmi_conn_value[MTK_AFE_NUM_HDMI_INPUT - 1];

	reg_mask = mtk_afe_hdmi_conn_mask[out_idx];
	reg_shift = mtk_afe_hdmi_conn_shift_bits[out_idx];
	reg_conn = hdmi_connection_reg[out_idx];

	regmap_update_bits(afe_info->regmap, reg_conn,
			   reg_mask << reg_shift, reg_val << reg_shift);
	return 0;
}

static int mtk_afe_interconn_valid(struct mtk_afe_info *afe_info,
				   uint32_t in, uint32_t out)
{
	if (in >= MTK_AFE_INTERCONN_NUM_INPUT ||
	    out >= MTK_AFE_INTERCONN_NUM_OUTPUT) {
		dev_err(afe_info->dev,
			"out of bound mpConnectionTable[%d][%d]\n", in, out);
		return -EINVAL;
	}

	if (mtk_afe_conn_tb[in][out] < 0 || mtk_afe_conn_tb[in][out] == 0) {
		dev_err(afe_info->dev,
			"No connection mtk_afe_conn_tb[%d][%d] = %d\n",
			in, out, mtk_afe_conn_tb[in][out]);
		return -EINVAL;
	}
	return 0;
}

int mtk_afe_interconn_connect(struct mtk_afe_info *afe_info,
			      uint32_t in, uint32_t out)
{
	int conn_bits = 0;
	int conn_reg = 0;
	int ret;

	if (in >= MTK_AFE_HDMI_CONN_INPUT_BASE ||
	    out >= MTK_AFE_HDMI_CONN_OUTPUT_BASE)
		return mtk_afe_interconn_hdmi(afe_info, in, out, true);

	ret = mtk_afe_interconn_valid(afe_info, in, out);
	if (ret)
		return ret;

	conn_bits = mtk_afe_conn_bits_tbl[in][out];
	conn_reg = mtk_afe_conn_reg[in][out];
	if (mtk_afe_interconn_check(afe_info, conn_reg, conn_bits)) {
		regmap_update_bits(afe_info->regmap, conn_reg,
				   1 << conn_bits, 1 << conn_bits);
		mtk_afe_reg_sta[in][out] |= MTK_AFE_CONN;
	}
	return 0;
}

int mtk_afe_interconn_connect_shift(struct mtk_afe_info *afe_info,
				    uint32_t in, uint32_t out)
{
	int conn_bits = 0;
	int conn_reg = 0;
	int ret;

	ret = mtk_afe_interconn_valid(afe_info, in, out);
	if (ret)
		return ret;

	if ((mtk_afe_conn_tb[in][out] & MTK_AFE_SHFT) != MTK_AFE_SHFT) {
		dev_err(afe_info->dev, "don't support shift opeartion");
		return -EINVAL;
	}
	conn_bits = mtk_afe_shift_conn_bits[in][out];
	conn_reg = mtk_afe_shift_conn_reg[in][out];
	if (mtk_afe_interconn_check(afe_info, conn_reg, conn_bits)) {
		regmap_update_bits(afe_info->regmap, conn_reg,
				   1 << conn_bits, 1 << conn_bits);
		mtk_afe_reg_sta[in][out] |= MTK_AFE_SHFT;
	}
	return 0;
}

int mtk_afe_interconn_disconnect(struct mtk_afe_info *afe_info,
				 uint32_t in, uint32_t out)
{
	int conn_bits = 0;
	int conn_reg = 0;
	int ret;

	if (in >= MTK_AFE_HDMI_CONN_INPUT_BASE ||
	    out >= MTK_AFE_HDMI_CONN_OUTPUT_BASE)
		return mtk_afe_interconn_hdmi(afe_info, in, out, false);

	ret = mtk_afe_interconn_valid(afe_info, in, out);
	if (ret)
		return ret;

	if ((mtk_afe_reg_sta[in][out] & MTK_AFE_CONN) == MTK_AFE_CONN) {
		/* here to disconnect connect bits */
		conn_bits = mtk_afe_conn_bits_tbl[in][out];
		conn_reg = mtk_afe_conn_reg[in][out];
		if (mtk_afe_interconn_check(afe_info, conn_reg, conn_bits)) {
			regmap_update_bits(afe_info->regmap, conn_reg,
					   1 << conn_bits, 0 << conn_bits);
			mtk_afe_reg_sta[in][out] &= ~MTK_AFE_CONN;
		}
	}
	if ((mtk_afe_reg_sta[in][out] & MTK_AFE_SHFT) == MTK_AFE_SHFT) {
		/* here to disconnect connect shift bits */
		conn_bits = mtk_afe_shift_conn_bits[in][out];
		conn_reg = mtk_afe_shift_conn_reg[in][out];
		if (mtk_afe_interconn_check(afe_info, conn_reg, conn_bits)) {
			regmap_update_bits(afe_info->regmap, conn_reg,
					   1 << conn_bits, 0 << conn_bits);
			mtk_afe_reg_sta[in][out] &= ~MTK_AFE_SHFT;
		}
	}
	return 0;
}

