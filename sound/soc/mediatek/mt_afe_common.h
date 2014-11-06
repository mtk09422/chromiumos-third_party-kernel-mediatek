/*
 * mt_afe_common.h  --  Mediatek audio driver common definitions
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

#ifndef _MT_AFE_COMMON_H_
#define _MT_AFE_COMMON_H_

#include "mt_afe_digital_type.h"
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/delay.h>
#include <linux/vmalloc.h>
#include <linux/wait.h>
#include <linux/spinlock.h>
#include <linux/sched.h>
#include <linux/jiffies.h>
#include <linux/string.h>
#include <linux/mutex.h>
#include <linux/time.h>
#include <linux/timer.h>
#include <linux/clk.h>
#include <sound/pcm.h>

#define MT_AFE_REG_NUM 200

struct mt_afe_block {
	unsigned int phys_buf_addr;
	unsigned char *virt_buf_addr;

	signed int buffer_size;
	signed int data_remained;
	unsigned int sample_num_mask;	/* sample number mask */
	unsigned int samples_per_int; /* number of samples to play before int*/
	signed int write_idx;	/* Previous Write Index. */
	signed int dma_read_idx;	/* Previous DMA Read Index. */
	unsigned int fsync_flag;
	unsigned int reset_flag;
};

struct mt_afe_memif {
	struct file *flip;
	struct mt_afe_block block;
	bool running;
	unsigned int memif_num;
};

struct mt_afe_info {
	unsigned long dma_addr;
	unsigned int pcm_irq_pos;	/* IRQ position */
	uint32_t samp_rate;
	uint32_t channel_mode;
	uint8_t start;
	uint32_t dsp_cnt;
	uint32_t buf_phys;
	int32_t mmap_flag;
	bool prepared;
	bool suspended;
	uint32_t reg_backup[MT_AFE_REG_NUM];
	/* address for ioremap audio hardware register */
	void *base_addr;
	void *afe_sram_address;
	uint32_t afe_sram_phy_address;
	uint32_t afe_sram_size;
	struct device *dev;

	struct mt_afe_irq audio_mcu_mode[MT_AFE_IRQ_MODE_NUM];
	struct mt_afe_pin_attribute afe_pin[MT_AFE_PIN_NUM];
	struct snd_pcm_substream *sub_stream[MT_AFE_MEMIF_NUM];
	struct mt_afe_memif memif[MT_AFE_MEMIF_NUM];

	/*clock*/
	struct clk *mt_afe_infra_audio_clk;
	struct clk *mt_afe_clk;
	struct clk *mt_afe_hdmi_clk;
	struct clk *mt_afe_top_apll_clk;
	struct clk *mt_afe_apll_d16;
	struct clk *mt_afe_apll_d24;
	struct clk *mt_afe_apll_d4;
	struct clk *mt_afe_apll_d8;
	struct clk *mt_afe_audpll;
	struct clk *mt_afe_apll_tuner_clk;
};

enum MT_AFE_IRQ_MCU_TYPE {
	MT_AFE_IRQ1_MCU = 1,
	MT_AFE_IRQ2_MCU = 2,
	MT_AFE_IRQ_MCU_DAI_SET = 4,
	MT_AFE_IRQ_MCU_DAI_RST = 8,
	MT_AFE_HDMI_IRQ = 16,
	MT_AFE_SPDF_IRQ = 32
};

/* below for audio debugging */
/* #define DEEP_DEBUG_AUDDRV */

#ifdef DEEP_DEBUG_AUDDRV
#define PRINTK_AUDDEEPDRV(format, args...) pr_info(format, ##args)
#else
#define PRINTK_AUDDEEPDRV(format, args...)
#endif

#endif
