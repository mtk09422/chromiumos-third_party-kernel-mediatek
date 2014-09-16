/*
 * mt_soc_common.h  --  Mediatek audio driver common definitions
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

#ifndef AUDIO_GLOBAL_H
#define AUDIO_GLOBAL_H

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

struct afe_block {
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

struct afe_mem_control {
	struct file *flip;
	struct afe_block block;
	bool running;
	unsigned int mem_if_num;
};

struct pcm_afe_info {
	unsigned long dma_addr;
	struct snd_pcm_substream *substream;
	unsigned int pcm_irq_pos;	/* IRQ position */
	uint32_t samp_rate;
	uint32_t channel_mode;
	uint8_t start;
	uint32_t dsp_cnt;
	uint32_t buf_phys;
	int32_t mmap_flag;
	bool prepared;
	struct afe_mem_control *afe_control;
};

enum IRQ_MCU_TYPE {
	INTERRUPT_IRQ1_MCU = 1,
	INTERRUPT_IRQ2_MCU = 2,
	INTERRUPT_IRQ_MCU_DAI_SET = 4,
	INTERRUPT_IRQ_MCU_DAI_RST = 8,
	INTERRUPT_HDMI_IRQ = 16,
	INTERRUPT_SPDF_IRQ = 32
};

/* below for audio debugging */
#define DEBUG_AUDDRV
#define DEBUG_AFE_REG
#define DEBUG_ANA_REG
#define DEBUG_AUD_CLK
/* #define DEEP_DEBUG_AUDDRV */

#ifdef DEEP_DEBUG_AUDDRV
#define PRINTK_AUDDEEPDRV(format, args...) pr_info(format, ##args)
#else
#define PRINTK_AUDDEEPDRV(format, args...)
#endif

#ifdef DEBUG_AUDDRV
#define PRINTK_AUDDRV(format, args...) pr_info(format, ##args)
#else
#define PRINTK_AUDDRV(format, args...)
#endif

#ifdef DEBUG_AFE_REG
#define PRINTK_AFE_REG(format, args...) pr_info(format, ##args)
#else
#define PRINTK_AFE_REG(format, args...)
#endif

#ifdef DEBUG_ANA_REG
#define PRINTK_ANA_REG(format, args...) pr_info(format, ##args)
#else
#define PRINTK_ANA_REG(format, args...)
#endif

#ifdef DEBUG_AUD_CLK
#define PRINTK_AUD_CLK(format, args...)  pr_info(format, ##args)
#else
#define PRINTK_AUD_CLK(format, args...)
#endif

#define PRINTK_AUD_ERROR(format, args...)  pr_err(format, ##args)

/* if need assert , use AUDIO_ASSERT(true) */
#define AUDIO_ASSERT(value) BUG_ON(false)

#define ENUM_TO_STR(enum) #enum

#endif
