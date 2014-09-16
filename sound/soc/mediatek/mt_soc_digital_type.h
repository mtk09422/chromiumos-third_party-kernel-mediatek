/*
 * mt_soc_digital_type.h  --  Mediatek audio SOC digital module definitions
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

#ifndef _AUDIO_DIGITAL_TYPE_H
#define _AUDIO_DIGITAL_TYPE_H

#include <linux/types.h>

/*****************************************************************************
 *                ENUM DEFINITION
 *****************************************************************************/

enum MTAUD_BLOCK {
	/* memmory interfrace */
	MTAUD_BLOCK_MEM_DL1 = 0,
	MTAUD_BLOCK_MEM_DL2,
	MTAUD_BLOCK_MEM_VUL,
	MTAUD_BLOCK_MEM_DAI,
	MTAUD_BLOCK_MEM_I2S,	/* currently no use */
	MTAUD_BLOCK_MEM_AWB,
	MTAUD_BLOCK_MEM_MOD_DAI,
	/* connection to int main modem */
	MTAUD_BLOCK_MODEM_PCM_1_O,
	/* connection to extrt/int modem */
	MTAUD_BLOCK_MODEM_PCM_2_O,
	/* 1st I2S for DAC and ADC */
	MTAUD_BLOCK_I2S_OUT_DAC,
	MTAUD_BLOCK_I2S_IN_ADC,
	/* 2nd I2S */
	MTAUD_BLOCK_I2S_OUT_2,
	MTAUD_BLOCK_I2S_IN_2,
	/* HW gain contorl */
	MTAUD_BLOCK_HW_GAIN1,
	MTAUD_BLOCK_HW_GAIN2,
	/* megrge interface */
	MTAUD_BLOCK_MRG_I2S_OUT,
	MTAUD_BLOCK_MRG_I2S_IN,
	MTAUD_BLOCK_DAI_BT,
	MTAUD_BLOCK_HDMI,
	MTAUD_BLOCK_NUM,
	MTAUD_MEMIF_NUM =
		MTAUD_BLOCK_MEM_MOD_DAI + 1
};

enum MTAUD_MEMIF_DIRECTION {
	MTAUD_MEMIF_DIRECTION_OUTPUT,
	MTAUD_MEMIF_DIRECTION_INPUT
};

enum MTAUD_INTERCONNECTIONINPUT {
	MTAUD_INTERCONNECTION_I00,
	MTAUD_INTERCONNECTION_I01,
	MTAUD_INTERCONNECTION_I02,
	MTAUD_INTERCONNECTION_I03,
	MTAUD_INTERCONNECTION_I04,
	MTAUD_INTERCONNECTION_I05,
	MTAUD_INTERCONNECTION_I06,
	MTAUD_INTERCONNECTION_I07,
	MTAUD_INTERCONNECTION_I08,
	MTAUD_INTERCONNECTION_I09,
	MTAUD_INTERCONNECTION_I10,
	MTAUD_INTERCONNECTION_I11,
	MTAUD_INTERCONNECTION_I12,
	MTAUD_INTERCONNECTION_I13,
	MTAUD_INTERCONNECTION_I14,
	MTAUD_INTERCONNECTION_I15,
	MTAUD_INTERCONNECTION_I16,
	MTAUD_INTERCONNECTION_NUM_INPUT
};

enum MTAUD_INTERCONNECTIONOUTPUT {
	MTAUD_INTERCONNECTION_O00,
	MTAUD_INTERCONNECTION_O01,
	MTAUD_INTERCONNECTION_O02,
	MTAUD_INTERCONNECTION_O03,
	MTAUD_INTERCONNECTION_O04,
	MTAUD_INTERCONNECTION_O05,
	MTAUD_INTERCONNECTION_O06,
	MTAUD_INTERCONNECTION_O07,
	MTAUD_INTERCONNECTION_O08,
	MTAUD_INTERCONNECTION_O09,
	MTAUD_INTERCONNECTION_O10,
	MTAUD_INTERCONNECTION_O11,
	MTAUD_INTERCONNECTION_O12,
	MTAUD_INTERCONNECTION_O13,
	MTAUD_INTERCONNECTION_O14,
	MTAUD_INTERCONNECTION_O15,
	MTAUD_INTERCONNECTION_O16,
	MTAUD_INTERCONNECTION_O17,
	MTAUD_INTERCONNECTION_O18,
	MTAUD_INTERCONNECTION_NUM_OUTPUT
};

enum MTAUD_CONN_STATE {
	MTAUD_DISCONNECT = 0x0,
	MTAUD_CONNECT = 0x1,
	MTAUD_SHIFT = 0x2
};

enum MTAUD_IRQ_MCU_MODE {
	MTAUD_IRQ_MCU_MODE_IRQ1 = 0,
	MTAUD_IRQ_MCU_MODE_IRQ2,
	MTAUD_IRQ_MCU_MODE_IRQ3,
	MTAUD_IRQ_MCU_MODE_IRQ4,
	MTAUD_IRQ_MCU_MODE_NUM
};

enum MTAUD_LR_SWAP {
	MTAUD_LR_SWAP_NO = 0,
	MTAUD_LR_SWAP_YES = 1
};

enum MTAUD_INV_LRCK {
	MTAUD_INV_LRCK_NO = 0,
	MTAUD_INV_LRCK_YES = 1
};

enum MTAUD_I2S_FORMAT {
	MTAUD_I2S_FORMAT_EIAJ = 0,
	MTAUD_I2S_FORMAT_I2S = 1
};

enum MTAUD_I2S_SRC {
	MTAUD_I2S_SRC_MASTER = 0,
	MTAUD_I2S_SRC_SLAVE = 1
};

enum MTAUD_I2S_WLEN {
	MTAUD_I2S_WLEN_16BITS = 0,
	MTAUD_I2S_WLEN_32BITS = 1
};

enum MTAUD_I2S_SAMPLERATE {
	MTAUD_I2S_SAMPLERATE_8K = 0,
	MTAUD_I2S_SAMPLERATE_11K = 1,
	MTAUD_I2S_SAMPLERATE_12K = 2,
	MTAUD_I2S_SAMPLERATE_16K = 4,
	MTAUD_I2S_SAMPLERATE_22K = 5,
	MTAUD_I2S_SAMPLERATE_24K = 6,
	MTAUD_I2S_SAMPLERATE_32K = 8,
	MTAUD_I2S_SAMPLERATE_44K = 9,
	MTAUD_I2S_SAMPLERATE_48K = 10
};

struct aud_digital_i2s {
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
struct aud_irq_mcu_mode {
	unsigned int status;	/* on,off */
	unsigned int irq_mcu_counter;
	unsigned int sample_rate;
};

struct aud_memif_attribute {
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
	unsigned int state;
	void *privatedata;
};

enum {
	AP_LOOPBACK_NONE = 0,
	AP_LOOPBACK_AMIC_TO_SPK,
	AP_LOOPBACK_AMIC_TO_HP,
	AP_LOOPBACK_DMIC_TO_SPK,
	AP_LOOPBACK_DMIC_TO_HP,
	AP_LOOPBACK_HEADSET_MIC_TO_SPK,
	AP_LOOPBACK_HEADSET_MIC_TO_HP,
};

#endif
