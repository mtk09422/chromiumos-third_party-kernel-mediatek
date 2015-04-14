/*
 * mtk_afe_common.h  --  Mediatek audio driver common definitions
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

#ifndef _MTK_AFE_COMMON_H_
#define _MTK_AFE_COMMON_H_

#include <linux/clk.h>
#include <linux/regmap.h>
#include <dt-bindings/sound/mtk-afe.h>

enum {
	MTK_CLK_INFRASYS_AUD,
	MTK_CLK_TOP_PDN_AUD,
	MTK_CLK_TOP_PDN_AUD_BUS,
	MTK_CLK_I2S0_M,
	MTK_CLK_I2S1_M,
	MTK_CLK_I2S2_M,
	MTK_CLK_I2S3_M,
	MTK_CLK_I2S3_B,
	MTK_CLK_BCK0,
	MTK_CLK_BCK1,
	MTK_CLK_NUM
};

struct mtk_afe;
struct snd_pcm_substream;

struct mtk_afe_io_data {
	int num;
	const char *name;
	int (*startup)(struct mtk_afe *, struct snd_pcm_substream *);
	void (*shutdown)(struct mtk_afe *, struct snd_pcm_substream *);
	int (*prepare)(struct mtk_afe *, struct snd_pcm_substream *);
	int (*start)(struct mtk_afe *, struct snd_pcm_substream *);
	void (*pause)(struct mtk_afe *, struct snd_pcm_substream *);
};

struct mtk_afe_io {
	const struct mtk_afe_io_data *data;
	struct clk *m_ck;
	struct clk *b_ck;
	u32 mck_mul;
	u32 *connections;
	int num_connections;
	int mem[2]; /* playback and capture */
};

struct mtk_afe_irq_data {
	int reg_cnt;
	int cnt_shift;
	int en_shift;
	int fs_shift;
	int clr_shift;
};

struct mtk_afe_memif_data {
	int id;
	const char *name;
	int reg_ofs_base;
	int reg_ofs_cur;
	int fs_shift;
	int mono_shift;
	int enable_shift;
};

struct mtk_afe_memif {
	unsigned int phys_buf_addr;
	int buffer_size;
	unsigned int hw_ptr;		/* Previous IRQ's HW ptr */
	bool use_sram;
	struct snd_pcm_substream *substream;
	const struct mtk_afe_memif_data *data;
	const struct mtk_afe_irq_data *irqdata;
};

struct mtk_afe {
	/* address for ioremap audio hardware register */
	void __iomem *base_addr;
	void __iomem *sram_address;
	u32 sram_phy_address;
	u32 sram_size;
	struct device *dev;
	struct regmap *regmap;

	struct mtk_afe_memif memif[MTK_AFE_MEMIF_NUM];
	struct mtk_afe_io ios[MTK_AFE_IO_NUM];

	struct clk *clocks[MTK_CLK_NUM];
};
#endif
