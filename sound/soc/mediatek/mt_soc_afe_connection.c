/*
 * mt_soc_afe_connection.c  --  Mediatek AFE connection support
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

#include "mt_soc_afe_control.h"
#include "mt_soc_digital_type.h"


/**
* here define conenction table for input and output
*/
static const char connection_table[MTAUD_INTERCONNECTION_NUM_INPUT]
	[MTAUD_INTERCONNECTION_NUM_OUTPUT] = {
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
static const char connection_bits_table[MTAUD_INTERCONNECTION_NUM_INPUT]
	[MTAUD_INTERCONNECTION_NUM_OUTPUT] = {
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
static const char shift_connection_bits[MTAUD_INTERCONNECTION_NUM_INPUT]
				[MTAUD_INTERCONNECTION_NUM_OUTPUT] = {
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
static const short connection_reg[MTAUD_INTERCONNECTION_NUM_INPUT]
	[MTAUD_INTERCONNECTION_NUM_OUTPUT] = {
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
static const short shift_connection_reg[MTAUD_INTERCONNECTION_NUM_INPUT]
				[MTAUD_INTERCONNECTION_NUM_OUTPUT] = {
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
static char reg_state[MTAUD_INTERCONNECTION_NUM_INPUT]
	[MTAUD_INTERCONNECTION_NUM_OUTPUT] = {
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


static bool check_bits_and_reg(short regaddr, char bits)
{
	if (regaddr <= 0 || bits < 0) {
		pr_notice("regaddr = %x bits = %d\n", regaddr, bits);
		return false;
	}
	return true;
}

bool set_connection_state(uint32_t connection_state, uint32_t in, uint32_t out)
{
	int connection_bits = 0;
	int connect_reg = 0;

	pr_debug("%s connection_state = %d in = %d out = %d\n",
		 __func__, connection_state, in, out);

	if (unlikely(in >= MTAUD_INTERCONNECTION_NUM_INPUT ||
		     out >= MTAUD_INTERCONNECTION_NUM_OUTPUT)) {
		pr_warn("out of bound mpConnectionTable[%d][%d]\n", in, out);
		return false;
	}

	if (unlikely(connection_table[in][out] < 0 ||
		     connection_table[in][out] == 0)) {
		pr_warn("No connection connection_table[%d][%d] = %d\n",
			in, out, connection_table[in][out]);
		return true;
	}


	switch (connection_state) {
	case MTAUD_DISCONNECT:
		if ((reg_state[in][out] & MTAUD_CONNECT) == MTAUD_CONNECT) {
			/* here to disconnect connect bits */
			connection_bits = connection_bits_table[in][out];
			connect_reg = connection_reg[in][out];
			if (check_bits_and_reg(connect_reg, connection_bits)) {
				afe_set_reg(connect_reg, 0 << connection_bits,
					    1 << connection_bits);
				reg_state[in][out] &= ~(MTAUD_CONNECT);
			}
		}
		if ((reg_state[in][out] & MTAUD_SHIFT) == MTAUD_SHIFT) {
			/* here to disconnect connect shift bits */
			connection_bits = shift_connection_bits[in][out];
			connect_reg = shift_connection_reg[in][out];
			if (check_bits_and_reg(connect_reg, connection_bits)) {
				afe_set_reg(connect_reg, 0 << connection_bits,
					    1 << connection_bits);
				reg_state[in][out] &= ~(MTAUD_SHIFT);
			}
		}
		break;
	case MTAUD_CONNECT:
		/* here to disconnect connect shift bits */
		connection_bits = connection_bits_table[in][out];
		connect_reg = connection_reg[in][out];
		if (check_bits_and_reg(connect_reg, connection_bits)) {
			afe_set_reg(connect_reg, 1 << connection_bits,
				    1 << connection_bits);
			reg_state[in][out] |= MTAUD_CONNECT;
		}
		break;
	case MTAUD_SHIFT:
		if ((connection_table[in][out] & MTAUD_SHIFT) != MTAUD_SHIFT) {
			pr_warn("donn't support shift opeartion");
			break;
		}
		connection_bits = shift_connection_bits[in][out];
		connect_reg = shift_connection_reg[in][out];
		if (check_bits_and_reg(connect_reg, connection_bits)) {
			afe_set_reg(connect_reg, 1 << connection_bits,
				    1 << connection_bits);
			reg_state[in][out] |= MTAUD_SHIFT;
		}
		break;
	default:
		pr_warn("%s no this state connection_state = %d\n",
			__func__, connection_state);
		break;
	}
	return true;
}

