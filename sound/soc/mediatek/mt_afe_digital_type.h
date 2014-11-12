/*
 * mt_afe_digital_type.h  --  Mediatek audio SOC digital module definitions
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

#ifndef _MT_AFE_DIGITAL_TYPE_H
#define _MT_AFE_DIGITAL_TYPE_H

#include <linux/types.h>

/*****************************************************************************
 *                ENUM DEFINITION
 *****************************************************************************/

enum mt_afe_pin {
	/* memory interfrace */
	MT_AFE_PIN_DL1 = 0,
	MT_AFE_PIN_DL2,
	MT_AFE_PIN_VUL,
	MT_AFE_PIN_DAI,
	MT_AFE_PIN_I2S,	/* currently no use */
	MT_AFE_PIN_AWB,
	MT_AFE_PIN_MOD_DAI,
	MT_AFE_PIN_HDMI,
	/* connection to int main modem */
	MT_AFE_PIN_MOD_PCM1,
	/* connection to extrt/int modem */
	MT_AFE_PIN_MOD_PCM2,
	/* 1st I2S for DAC and ADC */
	MT_AFE_PIN_I2S_DAC,
	MT_AFE_PIN_I2S_ADC,
	/* 2nd I2S */
	MT_AFE_PIN_I2S_O2,
	MT_AFE_PIN_I2S_I2,
	/* HW gain contorl */
	MT_AFE_PIN_HW_GAIN1,
	MT_AFE_PIN_HW_GAIN2,
	/* megrge interface */
	MT_AFE_PIN_MRG_O,
	MT_AFE_PIN_MRG_I,
	MT_AFE_PIN_DAIBT,
	MT_AFE_PIN_NUM,
	MT_AFE_MEMIF_NUM =
		MT_AFE_PIN_HDMI + 1
};

enum mt_afe_memif_dir {
	MT_AFE_MEMIF_DIR_O,
	MT_AFE_MEMIF_DIR_I
};

enum mt_afe_interconn_input {
	MT_AFE_INTERCONN_I00,
	MT_AFE_INTERCONN_I01,
	MT_AFE_INTERCONN_I02,
	MT_AFE_INTERCONN_I03,
	MT_AFE_INTERCONN_I04,
	MT_AFE_INTERCONN_I05,
	MT_AFE_INTERCONN_I06,
	MT_AFE_INTERCONN_I07,
	MT_AFE_INTERCONN_I08,
	MT_AFE_INTERCONN_I09,
	MT_AFE_INTERCONN_I10,
	MT_AFE_INTERCONN_I11,
	MT_AFE_INTERCONN_I12,
	MT_AFE_INTERCONN_I13,
	MT_AFE_INTERCONN_I14,
	MT_AFE_INTERCONN_I15,
	MT_AFE_INTERCONN_I16,
	MT_AFE_INTERCONN_NUM_INPUT
};

enum mt_afe_interconn_output {
	MT_AFE_INTERCONN_O00,
	MT_AFE_INTERCONN_O01,
	MT_AFE_INTERCONN_O02,
	MT_AFE_INTERCONN_O03,
	MT_AFE_INTERCONN_O04,
	MT_AFE_INTERCONN_O05,
	MT_AFE_INTERCONN_O06,
	MT_AFE_INTERCONN_O07,
	MT_AFE_INTERCONN_O08,
	MT_AFE_INTERCONN_O09,
	MT_AFE_INTERCONN_O10,
	MT_AFE_INTERCONN_O11,
	MT_AFE_INTERCONN_O12,
	MT_AFE_INTERCONN_O13,
	MT_AFE_INTERCONN_O14,
	MT_AFE_INTERCONN_O15,
	MT_AFE_INTERCONN_O16,
	MT_AFE_INTERCONN_O17,
	MT_AFE_INTERCONN_O18,
	MT_AFE_INTERCONN_NUM_OUTPUT
};

enum mt_afe_hdmi_interconn_input {
	MT_AFE_INTERCONN_I20 = 20,
	MT_AFE_INTERCONN_I21,
	MT_AFE_INTERCONN_I22,
	MT_AFE_INTERCONN_I23,
	MT_AFE_INTERCONN_I24,
	MT_AFE_INTERCONN_I25,
	MT_AFE_INTERCONN_I26,
	MT_AFE_INTERCONN_I27,
	MT_AFE_HDMI_CONN_INPUT_BASE = MT_AFE_INTERCONN_I20,
	MT_AFE_HDMI_CONN_INPUT_MAX = MT_AFE_INTERCONN_I27,
	MT_AFE_NUM_HDMI_INPUT = (MT_AFE_HDMI_CONN_INPUT_MAX -
				 MT_AFE_HDMI_CONN_INPUT_BASE + 1)
};

enum mt_afe_hdmi_interconn_output {
	MT_AFE_INTERCONN_O20 = 20,
	MT_AFE_INTERCONN_O21,
	MT_AFE_INTERCONN_O22,
	MT_AFE_INTERCONN_O23,
	MT_AFE_INTERCONN_O24,
	MT_AFE_INTERCONN_O25,
	MT_AFE_INTERCONN_O26,
	MT_AFE_INTERCONN_O27,
	MT_AFE_INTERCONN_O28,
	MT_AFE_INTERCONN_O29,
	MT_AFE_HDMI_CONN_OUTPUT_BASE = MT_AFE_INTERCONN_O20,
	MT_AFE_HDMI_CONN_OUTPUT_MAX = MT_AFE_INTERCONN_O29,
	MT_AFE_NUM_HDMI_OUTPUT = (MT_AFE_HDMI_CONN_OUTPUT_MAX -
				  MT_AFE_HDMI_CONN_OUTPUT_BASE + 1)
};


enum mt_afe_conn_state {
	MT_AFE_DISCONN = 0x0,
	MT_AFE_CONN = 0x1,
	MT_AFE_SHIFT = 0x2
};

enum mt_afe_irq_mode {
	MT_AFE_IRQ_MODE_1 = 0,
	MT_AFE_IRQ_MODE_2,
	MT_AFE_IRQ_MODE_3,
	MT_AFE_IRQ_MODE_5,
	MT_AFE_IRQ_MODE_NUM
};

enum mt_afe_lr_swap {
	MT_AFE_LR_SWAP_NO = 0,
	MT_AFE_LR_SWAP_YES = 1
};

enum mt_afe_inv_lrck {
	MT_AFE_INV_LRCK_NO = 0,
	MT_AFE_INV_LRCK_YES = 1
};

enum mt_afe_i2s_dir {
	MT_AFE_I2S_DIR_OUT = 0,
	MT_AFE_I2S_DIR_IN = 1
};

enum mt_afe_i2s_format {
	MT_AFE_I2S_FORMAT_EIAJ = 0,
	MT_AFE_I2S_FORMAT_I2S = 1
};

enum mt_afe_i2s_src {
	MT_AFE_I2S_SRC_MASTER = 0,
	MT_AFE_I2S_SRC_SLAVE = 1
};

enum mt_afe_i2s_wlen {
	MT_AFE_I2S_WLEN_16BITS = 0,
	MT_AFE_I2S_WLEN_32BITS = 1
};

enum mt_afe_i2s_rate {
	MT_AFE_I2S_RATE_8K = 0,
	MT_AFE_I2S_RATE_11K = 1,
	MT_AFE_I2S_RATE_12K = 2,
	MT_AFE_I2S_RATE_16K = 4,
	MT_AFE_I2S_RATE_22K = 5,
	MT_AFE_I2S_RATE_24K = 6,
	MT_AFE_I2S_RATE_32K = 8,
	MT_AFE_I2S_RATE_44K = 9,
	MT_AFE_I2S_RATE_48K = 10
};

enum mt_afe_hdmi_input_bits {
	MT_AFE_HDMI_INPUT_16BIT = 0,
	MT_AFE_HDMI_INPUT_32BIT
};

enum mt_afe_hdmi_i2s_wlen {
	MT_AFE_HDMI_I2S_8BIT = 0,
	MT_AFE_HDMI_I2S_16BIT,
	MT_AFE_HDMI_I2S_24BIT,
	MT_AFE_HDMI_I2S_32BIT
};

enum mt_afe_hdmi_i2s_delay {
	MT_AFE_HDMI_I2S_NOT_DELAY = 0,
	MT_AFE_HDMI_I2S_FIRST_BIT_1T_DELAY
};

enum mt_afe_hdmi_i2s_lrck_inv {
	MT_AFE_HDMI_I2S_LRCK_NOT_INVERSE = 0,
	MT_AFE_HDMI_I2S_LRCK_INVERSE
};

enum mt_afe_hdmi_i2s_bclk_inv {
	MT_AFE_HDMI_I2S_BCLK_NOT_INVERSE = 0,
	MT_AFE_HDMI_I2S_BCLK_INVERSE
};

struct mt_afe_i2s {
	bool lr_swap;
	bool i2s_slave;
	uint32_t i2s_sample_rate;
	bool inv_lrck;
	bool i2s_fmt;
	bool i2s_wlen;
	bool l2s_en;
	bool i2s_in_pad_sel;

	/* her for ADC usage , DAC will not use this */
	int buffer_update_word;
	bool loopback;
	bool fpga_bit;
	bool fpga_bit_test;
};

/* class for irq mode and counter. */
struct mt_afe_irq {
	unsigned int status;	/* on,off */
	unsigned int irq_mcu_counter;
	unsigned int sample_rate;
};

struct mt_afe_pin_attribute {
	int format;
	int direction;
	unsigned int sample_rate;
	unsigned int channels;
	unsigned int buffer_size;
	unsigned int interrupt_sample;
	unsigned int memory_interface_type;
	unsigned int clock_inverse;
	unsigned int mono_sel;
	unsigned int dup_write;
	bool state;
	void *privatedata;
};

struct mt_afe_hdmi_clock {
	int fs;
	int apll_sel;
	int bck_div;
};

enum {
	MT_AFE_LOOPBACK_NONE = 0,
	MT_AFE_LOOPBACK_AMIC_SPK,
	MT_AFE_LOOPBACK_AMIC_HP,
	MT_AFE_LOOPBACK_DMIC_SPK,
	MT_AFE_LOOPBACK_DMIC_HP,
	MT_AFE_LOOPBACK_HSMIC_SPK,
	MT_AFE_LOOPBACK_HSMIC_HP,
};

#endif
