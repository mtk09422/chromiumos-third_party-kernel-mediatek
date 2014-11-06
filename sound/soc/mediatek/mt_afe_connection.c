/*
 * mt_afe_connection.c  --  Mediatek AFE connection support
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

#include "mt_afe_control.h"
#include "mt_afe_digital_type.h"


/**
* here define conenction table for input and output
*/
static const char mt_afe_conn_tbl[MT_AFE_INTERCONN_NUM_INPUT]
	[MT_AFE_INTERCONN_NUM_OUTPUT] = {
	/* 0   1   2   3   4   5   6   7   8   9  10  11  12  13  14  15  16
	17  18 */
	{3, 3, 3, 3, 3, 3, -1, -1, -1, -1, -1, -1, -1, -1, -1, 3, 1,
	-1, -1},	/* I00 */
	{3, 3, 3, 3, 3, -1, 3, -1, -1, -1, -1, -1, -1, -1, -1, 1, 3,
	-1, -1},	/* I01 */
	{1, 1, 1, 1, 1, 1, -1, 1, -1, -1, -1, 1, -1, -1, -1, 1, 1,
	1, -1},	/* I02 */
	{1, 1, 1, 1, 1, 1, -1, 1, -1, 1, -1, -1, -1, -1, -1, 1, 1,
	1, -1},	/* I03 */
	{1, 1, 1, 1, 1, -1, 1, -1, 1, -1, 1, -1, -1, -1, -1, 1, 1,
	-1, 1},	/* I04 */
	{3, 3, 3, 3, 3, 1, -1, 0, -1, 0, -1, 0, -1, 1, 1, -1, -1,
	0, -1},	/* I05 */
	{3, 3, 3, 3, 3, -1, 1, -1, 0, -1, 0, -1, 0, 1, 1, -1, -1,
	-1, 0},	/* I06 */
	{3, 3, 3, 3, 3, 1, -1, 0, -1, 0, -1, 0, -1, 1, 1, -1, -1,
	0, -1},	/* I07 */
	{3, 3, 3, 3, 3, -1, 1, -1, 0, -1, 0, -1, 0, 1, 1, -1, -1,
	-1, 0},	/* I08 */
	{1, 1, 1, 1, 1, 1, -1, -1, -1, -1, -1, -1, 1, -1, -1, 1, 1,
	-1, -1},	/* I09 */
	{3, -1, 3, 3, -1, 0, -1, 1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	1, -1},	/* I10 */
	{-1, 3, 3, -1, 3, -1, 0, -1, 1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, 1},	/* I11 */
	{3, -1, 3, 3, -1, 0, -1, 1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	1, -1},	/* I12 */
	{-1, 3, 3, -1, 3, -1, 0, -1, 1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, 1},	/* I13 */
	{1, 1, 1, 1, 1, 1, -1, -1, -1, -1, -1, -1, 1, -1, -1, 1, 1,
	-1, -1},	/* I14 */
	{3, 3, 3, 3, 3, 3, -1, -1, -1, 1, -1, -1, -1, -1, -1, 3, 1,
	-1, -1},	/* I15 */
	{3, 3, 3, 3, 3, -1, 3, -1, -1, -1, 1, -1, -1, -1, -1, 1, 3,
	-1, -1},	/* I16 */
};

/**
* connection bits of certain bits
*/
static const char mt_afe_conn_bits_tbl[MT_AFE_INTERCONN_NUM_INPUT]
	[MT_AFE_INTERCONN_NUM_OUTPUT] = {
	/* 0   1   2   3   4   5   6   7   8   9  10  11  12  13  14  15  16
	17  18 */
	{0, 16, 0, 16, 0, 16, -1, -1, -1, -1, -1, -1, -1, -1, -1, 16, 22,
	-1, -1},	/* I00 */
	{1, 17, 1, 17, 1, -1, 22, -1, -1, -1, -1, -1, -1, -1, -1, 17, 23,
	-1, -1},	/* I01 */
	{2, 18, 2, 18, 2, 17, -1, 21, -1, -1, -1, 6, -1, -1, -1, 18, 24,
	22, -1},	/* I02 */
	{3, 19, 3, 19, 3, 18, -1, 26, -1, 0, -1, -1, -1, -1, -1, 19, 25,
	13, -1},	/* I03 */
	{4, 20, 4, 20, 4, -1, 23, -1, 29, -1, 3, -1, -1, -1, -1, 20, 26,
	-1, 16},	/* I04 */
	{5, 21, 5, 21, 5, 19, -1, 27, -1, 1, -1, 7, -1, 16, 20, -1, -1,
	14, -1},	/* I05 */
	{6, 22, 6, 22, 6, -1, 24, -1, 30, -1, 4, -1, 9, 17, 21, -1, -1,
	-1, 17},	/* I06 */
	{7, 23, 7, 23, 7, 20, -1, 28, -1, 2, -1, 8, -1, 18, 22, -1, -1,
	15, -1},	/* I07 */
	{8, 24, 8, 24, 8, -1, 25, -1, 31, -1, 5, -1, 10, 19, 23, -1, -1,
	-1, 18},	/* I08 */
	{9, 25, 9, 25, 9, 21, 27, -1, -1, -1, -1, -1, 11, -1, -1, 21, 27,
	-1, -1},	/* I09 */
	{0, -1, 4, 8, -1, 12, -1, 14, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	24, -1},	/* I10 */
	{-1, 2, 6, -1, 10, -1, 13, -1, 15, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, 25},	/* I11 */
	{0, -1, 4, 8, -1, 12, -1, 14, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	6, -1},	/* I12 */
	{-1, 2, 6, -1, 10, -1, 13, -1, 15, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, 7},	/* I13 */
	{12, 17, 22, 27, 0, 5, -1, -1, -1, -1, -1, -1, 12, -1, -1, 28, 1,
	-1, -1},	/* I14 */
	{13, 18, 23, 28, 1, 6, -1, -1, -1, 10, -1, -1, -1, -1, -1, 29, 2,
	-1, -1},	/* I15 */
	{14, 19, 24, 29, 2, -1, 8, -1, -1, -1, 11, -1, -1, -1, -1, 30, 3,
	-1, -1},	/* I16 */
};


/**
* connection shift bits of certain bits
*/
static const char mt_afe_shift_conn_bits[MT_AFE_INTERCONN_NUM_INPUT]
				[MT_AFE_INTERCONN_NUM_OUTPUT] = {
	/* 0   1   2   3   4   5   6    7  8   9  10  11  12  13  14  15  16
	17  18 */
	{10, 26, 10, 26, 10, 19, -1, -1, -1, -1, -1, -1, -1, -1, -1, 31, -1,
	-1, -1},	/* I00 */
	{11, 27, 11, 27, 11, -1, 20, -1, -1, -1, -1, -1, -1, -1, -1, -1, 4,
	-1, -1},	/* I01 */
	{-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1},	/* I02 */
	{-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1},	/* I03 */
	{-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1},	/* I04 */
	{12, 28, 12, 28, 12, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1},	/* I05 */
	{13, 29, 13, 29, 13, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1},	/* I06 */
	{14, 30, 14, 30, 14, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1},	/* I07 */
	{15, 31, 15, 31, 15, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1},	/* I08 */
	{-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1},	/* I09 */
	{1, -1, 5, 9, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1},	/* I10 */
	{-1, 3, 7, -1, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1},	/* I11 */
	{1, -1, 5, 9, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1},	/* I12 */
	{-1, 3, 7, -1, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1},	/* I13 */
	{-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1},	/* I14 */
	{15, 20, 25, 30, 3, 7, -1, -1, -1, -1, -1, -1, -1, -1, -1, 0, -1,
	-1, -1},	/* I15 */
	{16, 21, 26, 31, 4, -1, 9, -1, -1, -1, -1, -1, -1, -1, -1, -1, 5,
	-1, -1},	/* I16 */
};

/**
* connection of register
*/
static const short mt_afe_conn_reg[MT_AFE_INTERCONN_NUM_INPUT]
	[MT_AFE_INTERCONN_NUM_OUTPUT] = {
	/* 0   1   2    3   4   5      6      7      8      9     10     11
	12      13   14     15     16     17     18 */
	{0x20, 0x20, 0x24, 0x24, 0x28, 0x28, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, 0x438, 0x438, -1, -1}, /* I00 */
	{0x20, 0x20, 0x24, 0x24, 0x28, -1, 0x28, -1, -1, -1, -1, -1,
	-1, -1, -1, 0x438, 0x438, -1, -1}, /* I01 */
	{0x20, 0x20, 0x24, 0x24, 0x28, 0x28, -1, 0x30, -1, -1, -1, 0x2C,
	-1, -1, -1, 0x438, 0x438, 0x30, -1}, /* I02 */
	{0x20, 0x20, 0x24, 0x24, 0x28, 0x28, -1, 0x28, -1, 0x2C, -1, -1,
	-1, -1, -1, 0x438, 0x438, 0x30, -1}, /* I03 */
	{0x20, 0x20, 0x24, 0x24, 0x28, -1, 0x28, -1, 0x28, -1, 0x2C, -1,
	-1, -1, -1, 0x438, 0x438, -1, 0x30}, /* I04 */
	{0x20, 0x20, 0x24, 0x24, 0x28, 0x28, -1, 0x28, -1, 0x2C, -1, 0x2C,
	-1, 0x420, 0x420, -1, -1, 0x30, -1},	/* I05 */
	{0x20, 0x20, 0x24, 0x24, 0x28, -1, 0x28, -1, 0x28, -1, 0x2C, -1,
	0x2C, 0x420, 0x420, -1, -1, -1, 0x30},	/* I06 */
	{0x20, 0x20, 0x24, 0x24, 0x28, 0x28, -1, 0x28, -1, 0x2C, -1, 0x2C,
	-1, 0x420, 0x420, -1, -1, 0x30, -1},	/* I07 */
	{0x20, 0x20, 0x24, 0x24, 0x28, -1, 0x28, -1, 0x28, -1, 0x2C, -1,
	0x2C, 0x420, 0x420, -1, -1, -1, 0x30},	/* I08 */
	{0x20, 0x20, 0x24, 0x24, 0x28, 0x28, -1, -1, -1, -1, -1, -1,
	0x2C, -1, -1, 0x438, 0x438, -1, -1}, /* I09 */
	{0x420, -1, 0x420, 0x420, -1, 0x420, -1, 0x420, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, 0x420, -1}, /* I10 */
	{-1, 0x420, 0x420, -1, 0x420, -1, 0x420, -1, 0x420, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, 0x420}, /* I11 */
	{0x438, -1, 0x438, 0x438, -1, 0x438, -1, 0x438, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, 0x440, -1}, /* I12 */
	{-1, 0x438, 0x438, -1, 0x438, -1, 0x438, -1, 0x438, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, 0x440}, /* I13 */
	{0x2C, 0x2C, 0x2C, 0x2C, 0x30, 0x30, -1, -1, -1, -1, -1, -1,
	0x30, -1, -1, 0x438, 0x440, -1, -1}, /* I14 */
	{0x2C, 0x2C, 0x2C, 0x2C, 0x30, 0x30, -1, -1, -1, 0x30, -1, -1,
	-1, -1, -1, 0x438, 0x440, -1, -1}, /* I15 */
	{0x2C, 0x2C, 0x2C, 0x2C, 0x30, -1, 0x30, -1, -1, -1, 0x30, -1,
	-1, -1, -1, 0x438, 0x440, -1, -1}, /* I16 */
};


/**
* shift connection of register
*/
static const short mt_afe_shift_conn_reg[MT_AFE_INTERCONN_NUM_INPUT]
				[MT_AFE_INTERCONN_NUM_OUTPUT] = {
	/* 0   1   2    3    4    5    6    7    8    9    10    11    12    13
		14    15    16    17    18 */
	{0x20, 0x20, 0x24, 0x24, 0x28, 0x30, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, 0x438, -1, -1, -1}, /* I00 */
	{0x20, 0x20, 0x24, 0x24, 0x28, -1, 0x30, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, 0x440, -1, -1}, /* I01 */
	{-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1},	/* I02 */
	{-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1},	/* I03 */
	{-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1},	/* I04 */
	{0x20, 0x20, 0x24, 0x24, 0x28, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1},	/* I05 */
	{0x20, 0x20, 0x24, 0x24, 0x28, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1},	/* I06 */
	{0x20, 0x20, 0x24, 0x24, 0x28, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1},	/* I07 */
	{0x20, 0x20, 0x24, 0x24, 0x28, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1},	/* I08 */
	{-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1},	/* I09 */
	{0x420, -1, 0x420, 0x420, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1},	/* I10 */
	{0x420, 0x420, 0x420, -1, 0x420, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1},	/* I11 */
	{0x438, -1, 0x438, 0x438, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1},	/* I12 */
	{-1, 0x438, 0x438, -1, 0x438, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1},	/* I13 */
	{-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1},	/* I14 */
	{0x2C, 0x2C, 0x2C, 0x2C, 0x30, 0x30, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, 0x440, -1, -1, -1},	/* I15 */
	{0x2C, 0x2C, 0x2C, 0x2C, 0x30, -1, 0x30, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, 0x440, -1, -1},	/* I16 */
};

/**
* connection state of register
*/
static char mt_afe_reg_state[MT_AFE_INTERCONN_NUM_INPUT]
	[MT_AFE_INTERCONN_NUM_OUTPUT] = {
	/* 0  1  2  3  4  5  6  7  8  9  10 11 12 13 14 15 16 17 18 */
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, /* I00 */
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, /* I01 */
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, /* I02 */
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, /* I03 */
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, /* I04 */
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, /* I05 */
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, /* I06 */
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, /* I07 */
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, /* I08 */
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, /* I09 */
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, /* I10 */
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, /* I11 */
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, /* I12 */
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, /* I13 */
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, /* I14 */
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, /* I15 */
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0} /* I16 */
};

/**
* value of connection register to set for certain input
*/
const uint32_t mt_afe_hdmi_conn_value[MT_AFE_NUM_HDMI_INPUT] = {
	/* I_20 I_21 I_22 I_23 I_24 I_25 I_26 I_27 */
	0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7
};

/**
* mask of connection register to set for certain output
*/
const uint32_t mt_afe_hdmi_conn_mask[MT_AFE_NUM_HDMI_OUTPUT] = {
	/* O_20  O_21  O_22  O_23  O_24  O_25  O_26  O_27  O_28  O_29 */
	0x7, 0x7, 0x7, 0x7, 0x7, 0x7, 0x7, 0x7, 0x7, 0x7
};

/**
* shift bits of connection register to set for certain output
*/
const char mt_afe_hdmi_conn_shift_bits[MT_AFE_NUM_HDMI_OUTPUT] = {
	/* O_20 O_21 O_22 O_23 O_24 O_25 O_26 O_27 O_28 O_29 */
	0, 3, 6, 9, 12, 15, 18, 21, 24, 27
};

/**
* connection state of HDMI
*/
char mt_afe_hdmi_conn_state[MT_AFE_NUM_HDMI_INPUT][MT_AFE_NUM_HDMI_OUTPUT] = {
	/* O_20 O_21 O_22 O_23 O_24 O_25 O_26 O_27 O_28 O_29 */
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0},	/* I_20 */
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0},	/* I_21 */
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0},	/* I_22 */
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0},	/* I_23 */
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0},	/* I_24 */
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0},	/* I_25 */
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0},	/* I_26 */
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0}	/* I_27 */
};




static bool mt_afe_check_conn(struct mt_afe_info *afe_info,
			      short regaddr, char bits)
{
	if (regaddr <= 0 || bits < 0) {
		dev_notice(afe_info->dev,
			   "regaddr = %x bits = %d\n", regaddr, bits);
		return false;
	}
	return true;
}

static int mt_afe_set_hdmi_conn(struct mt_afe_info *afe_info,
				uint32_t conn_state, uint32_t in, uint32_t out)
{
	uint32_t input_idx;
	uint32_t out_idx;
	uint32_t reg_value, reg_mask, reg_shift;

	/* check if connection request is valid */
	if (in < MT_AFE_HDMI_CONN_INPUT_BASE ||
	    in > MT_AFE_HDMI_CONN_INPUT_MAX ||
	    out < MT_AFE_HDMI_CONN_OUTPUT_BASE ||
	    out > MT_AFE_HDMI_CONN_OUTPUT_MAX) {
		return -EINVAL;
	}

	input_idx = in - MT_AFE_HDMI_CONN_INPUT_BASE;
	out_idx = out - MT_AFE_HDMI_CONN_OUTPUT_BASE;

	/* do connection */
	switch (conn_state) {
	case MT_AFE_CONN:
		reg_value = mt_afe_hdmi_conn_value[input_idx];
		reg_mask = mt_afe_hdmi_conn_mask[out_idx];
		reg_shift = mt_afe_hdmi_conn_shift_bits[out_idx];

		mt_afe_set_reg(afe_info, AFE_HDMI_CONN0, reg_value << reg_shift,
			       reg_mask << reg_shift);
		mt_afe_hdmi_conn_state[input_idx][out_idx] = MT_AFE_CONN;
		break;
	case MT_AFE_DISCONN:
		reg_value = mt_afe_hdmi_conn_value[MT_AFE_INTERCONN_I27 -
					 MT_AFE_HDMI_CONN_INPUT_BASE];
		reg_mask = mt_afe_hdmi_conn_mask[out_idx];
		reg_shift = mt_afe_hdmi_conn_shift_bits[out_idx];

		mt_afe_set_reg(afe_info, AFE_HDMI_CONN0, reg_value << reg_shift,
			       reg_mask << reg_shift);
		mt_afe_hdmi_conn_state[input_idx][out_idx] = MT_AFE_DISCONN;
		break;
	default:
		break;
	}

	return true;
}

int mt_afe_set_connection_state(struct mt_afe_info *afe_info,
				uint32_t conn_state,
				uint32_t in, uint32_t out)
{
	int connection_bits = 0;
	int connect_reg = 0;

	dev_dbg(afe_info->dev, "%s conn_state = %d in = %d out = %d\n",
		__func__, conn_state, in, out);


	if (in >= MT_AFE_HDMI_CONN_INPUT_BASE ||
	    out >= MT_AFE_HDMI_CONN_OUTPUT_BASE)
		return mt_afe_set_hdmi_conn(afe_info, conn_state, in, out);

	if (in >= MT_AFE_INTERCONN_NUM_INPUT ||
	    out >= MT_AFE_INTERCONN_NUM_OUTPUT) {
		dev_warn(afe_info->dev,
			 "out of bound mpConnectionTable[%d][%d]\n", in, out);
		return -EINVAL;
	}

	if (mt_afe_conn_tbl[in][out] < 0 || mt_afe_conn_tbl[in][out] == 0) {
		dev_warn(afe_info->dev,
			 "No connection mt_afe_conn_tbl[%d][%d] = %d\n",
			 in, out, mt_afe_conn_tbl[in][out]);
		return -EINVAL;
	}


	switch (conn_state) {
	case MT_AFE_DISCONN:
		if ((mt_afe_reg_state[in][out] & MT_AFE_CONN) == MT_AFE_CONN) {
			/* here to disconnect connect bits */
			connection_bits = mt_afe_conn_bits_tbl[in][out];
			connect_reg = mt_afe_conn_reg[in][out];
			if (mt_afe_check_conn(afe_info, connect_reg,
					      connection_bits)) {
				mt_afe_set_reg(afe_info, connect_reg,
					       0 << connection_bits,
					       1 << connection_bits);
				mt_afe_reg_state[in][out] &= ~(MT_AFE_CONN);
			}
		}
		if ((mt_afe_reg_state[in][out] & MT_AFE_SHIFT)
		    == MT_AFE_SHIFT) {
			/* here to disconnect connect shift bits */
			connection_bits = mt_afe_shift_conn_bits[in][out];
			connect_reg = mt_afe_shift_conn_reg[in][out];
			if (mt_afe_check_conn(afe_info, connect_reg,
					      connection_bits)) {
				mt_afe_set_reg(afe_info, connect_reg,
					       0 << connection_bits,
					       1 << connection_bits);
				mt_afe_reg_state[in][out] &= ~(MT_AFE_SHIFT);
			}
		}
		break;
	case MT_AFE_CONN:
		/* here to disconnect connect shift bits */
		connection_bits = mt_afe_conn_bits_tbl[in][out];
		connect_reg = mt_afe_conn_reg[in][out];
		if (mt_afe_check_conn(afe_info, connect_reg, connection_bits)) {
			mt_afe_set_reg(afe_info, connect_reg,
				       1 << connection_bits,
				       1 << connection_bits);
			mt_afe_reg_state[in][out] |= MT_AFE_CONN;
		}
		break;
	case MT_AFE_SHIFT:
		if ((mt_afe_conn_tbl[in][out] & MT_AFE_SHIFT) != MT_AFE_SHIFT) {
			dev_warn(afe_info->dev, "don't support shift opeartion");
			break;
		}
		connection_bits = mt_afe_shift_conn_bits[in][out];
		connect_reg = mt_afe_shift_conn_reg[in][out];
		if (mt_afe_check_conn(afe_info, connect_reg, connection_bits)) {
			mt_afe_set_reg(afe_info, connect_reg,
				       1 << connection_bits,
				       1 << connection_bits);
			mt_afe_reg_state[in][out] |= MT_AFE_SHIFT;
		}
		break;
	default:
		dev_warn(afe_info->dev, "%s no this state conn_state = %d\n",
			 __func__, conn_state);
		break;
	}
	return 0;
}

