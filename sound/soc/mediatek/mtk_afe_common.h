/*
 * mtk_afe_common.h  --  Mediatek audio driver common definitions
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

#ifndef _MTK_AFE_COMMON_H_
#define _MTK_AFE_COMMON_H_

#include "mtk_afe_digital_type.h"
#include <linux/clk.h>
#include <linux/regmap.h>

struct mtk_afe_block {
	unsigned int phys_buf_addr;
	unsigned char *virt_buf_addr;

	int buffer_size;
	int write_idx;		/* Previous Write Index. */
	int dma_read_idx;	/* Previous DMA Read Index. */
};

struct mtk_afe_reg {
	uint32_t addr;
	unsigned int shift;
	unsigned int mask;
};

struct mtk_afe_memif {
	struct mtk_afe_block block;
	bool use_sram;
	bool prepared;
	struct snd_pcm_substream *sub_stream;
	unsigned int irq_num;
	struct mtk_afe_reg reg_base;
	struct mtk_afe_reg reg_cur;
	struct mtk_afe_reg reg_irq_cnt;
	struct mtk_afe_reg reg_irq_en;
	struct mtk_afe_reg reg_irq_fs;
	struct mtk_afe_reg reg_fs;
	struct mtk_afe_reg reg_ch;
};

struct mtk_afe_clk {
	const char *name;
	const bool default_on;
	struct clk *clk;
};

struct mtk_afe_info {
	/* i2s/2nd i2s/mtkif */
	uint32_t codec_if;

	/* address for ioremap audio hardware register */
	void __iomem *base_addr;
	void __iomem *afe_sram_address;
	void __iomem *apmixedsys_base_addr;
	void __iomem *topckgen_base_addr;
	uint32_t afe_sram_phy_address;
	uint32_t afe_sram_size;
	struct device *dev;
	struct regmap *regmap;

	/* AFE HW status */
	/* FIXME todo make these array pointer? */
	bool irq_status[MTK_AFE_IRQ_MODE_NUM];
	bool pin_status[MTK_AFE_PIN_NUM];
	struct mtk_afe_memif memif[MTK_AFE_MEMIF_NUM];

	/* clock */
	struct mtk_afe_clk clock[MTK_CLK_NUM];

	/* hdmi loopback */
	unsigned int hdmi_loop_type;
	unsigned int hdmi_fs;
};
#endif
