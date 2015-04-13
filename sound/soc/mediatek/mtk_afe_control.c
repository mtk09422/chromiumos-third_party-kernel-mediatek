/*
 * mtk_afe_control.c  --  Mediatek AFE controller
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

#include "mtk_afe_common.h"
#include "mtk_afe_control.h"
#include "mtk_afe_connection.h"
#include <linux/interrupt.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <sound/pcm.h>

/* FIXME: move to 8173 specific file */
static struct mtk_afe_clk aud_clks[MTK_CLK_NUM] = {
	[MTK_CLK_AFE] = {"aud_afe_clk", true, NULL},
	[MTK_CLK_HDMI] = {"aud_hdmi_clk", false, NULL},
	[MTK_CLK_SPDIF] = {"aud_spdif_clk", false, NULL},
	[MTK_CLK_APLL22M] = {"aud_apll22m_clk", true, NULL},
	[MTK_CLK_APLL24M] = {"aud_apll24m_clk", true, NULL},
	[MTK_CLK_APLL1_TUNER] = {"aud_apll1_tuner_clk", false, NULL},
	[MTK_CLK_APLL2_TUNER] = {"aud_apll2_tuner_clk", false, NULL},
	[MTK_CLK_INFRASYS_AUD] = {"infra_sys_audio_clk", false, NULL},
	[MTK_CLK_TOP_PDN_AUD] = {"top_pdn_audio", false, NULL},
	[MTK_CLK_TOP_PDN_AUD_BUS] = {"top_pdn_aud_intbus", false, NULL},
	[MTK_CLK_TOP_AUD1_SEL] = {"top_audio_1_sel", false, NULL},
	[MTK_CLK_TOP_AUD2_SEL] = {"top_audio_2_sel", false, NULL},
	[MTK_CLK_TOP_APLL1_CK] = {"top_apll1_ck", false, NULL},
	[MTK_CLK_TOP_APLL2_CK] = {"top_apll2_ck", false, NULL},
	[MTK_CLK_SCP_SYS_AUD] = {"scp_sys_audio", false, NULL},
	[MTK_CLK_CLK26M] = {"clk26m", false, NULL}
};

#define MTK_AFE_MEMIF_REGS	7

/*
 * reg_base
 * reg_cur
 * reg_irq_cnt
 * reg_irq_en
 * reg_irq_fs
 * reg_fs
 * reg_ch
 */
static struct mtk_afe_reg memif_regs[] = {
	/* DL1 */
	{ .addr = AFE_DL1_BASE,      .shift = 0,	.mask = 0xffffffff },
	{ .addr = AFE_DL1_CUR,       .shift = 0,	.mask = 0xffffffff },
	{ .addr = AFE_IRQ_CNT1,      .shift = 0,	.mask = 0x3ffff },
	{ .addr = AFE_IRQ_MCU_CON,   .shift = 0,	.mask = 1 },
	{ .addr = AFE_IRQ_MCU_CON,   .shift = 4,	.mask = 0xf0 },
	{ .addr = AFE_DAC_CON1,      .shift = 0,	.mask = 0xf },
	{ .addr = AFE_DAC_CON1,      .shift = 21,	.mask = 1 << 21 },
	/* DL2 */
	{ .addr = AFE_DL2_BASE,      .shift = 0,	.mask = 0xffffffff },
	{ .addr = AFE_DL2_CUR,       .shift = 0,	.mask = 0xffffffff },
	{ .addr = AFE_IRQ_CNT1,      .shift = 0,	.mask = 0x3ffff },
	{ .addr = AFE_IRQ_MCU_CON,   .shift = 0,	.mask = 1 },
	{ .addr = AFE_IRQ_MCU_CON,   .shift = 4,	.mask = 0xf0 },
	{ .addr = AFE_DAC_CON1,      .shift = 4,	.mask = 0xf0 },
	{ .addr = AFE_DAC_CON1,      .shift = 22,	.mask = 1 << 22 },
	/* VUL */
	{ .addr = AFE_VUL_BASE,      .shift = 0,	.mask = 0xffffffff },
	{ .addr = AFE_VUL_CUR,       .shift = 0,	.mask = 0xffffffff },
	{ .addr = AFE_IRQ_CNT2,      .shift = 0,	.mask = 0x3ffff },
	{ .addr = AFE_IRQ_MCU_CON,   .shift = 1,	.mask = 1 << 1},
	{ .addr = AFE_IRQ_MCU_CON,   .shift = 8,	.mask = 0xf00 },
	{ .addr = AFE_DAC_CON1,	     .shift = 16,	.mask = 0xf0000 },
	{ .addr = AFE_DAC_CON1,      .shift = 27,	.mask = 1 << 27 },
	/* DAI */
	{ .addr = AFE_DAI_BASE,      .shift = 0,	.mask = 0xffffffff },
	{ .addr = AFE_DAI_CUR,       .shift = 0,	.mask = 0xffffffff },
	{ .addr = AFE_IRQ_CNT2,      .shift = 0,	.mask = 0x3ffff },
	{ .addr = AFE_IRQ_MCU_CON,   .shift = 1,	.mask = 1 << 1},
	{ .addr = AFE_IRQ_MCU_CON,   .shift = 8,	.mask = 0xf00 },
	{ .addr = AFE_DAC_CON1,      .shift = 20,	.mask = 1 << 20 },
	{ .addr = AFE_MEMIF_MON0,    .shift = 0,	.mask = 0 },
	/* I2S */
	{ .addr = AFE_MEMIF_MON0,    .shift = 0,	.mask = 0 },
	{ .addr = AFE_MEMIF_MON0,    .shift = 0,	.mask = 0 },
	{ .addr = AFE_MEMIF_MON0,    .shift = 0,	.mask = 0 },
	{ .addr = AFE_MEMIF_MON0,    .shift = 0,	.mask = 0 },
	{ .addr = AFE_MEMIF_MON0,    .shift = 0,	.mask = 0 },
	{ .addr = AFE_DAC_CON1,      .shift = 8,	.mask = 0xf00 },
	{ .addr = AFE_MEMIF_MON0,    .shift = 0,	.mask = 0 },
	/* AWB */
	{ .addr = AFE_AWB_BASE,      .shift = 0,	.mask = 0xffffffff },
	{ .addr = AFE_AWB_CUR,       .shift = 0,	.mask = 0xffffffff },
	{ .addr = AFE_IRQ_CNT2,      .shift = 0,	.mask = 0x3ffff },
	{ .addr = AFE_IRQ_MCU_CON,   .shift = 1,	.mask = 1 << 1},
	{ .addr = AFE_IRQ_MCU_CON,   .shift = 8,	.mask = 0xf00 },
	{ .addr = AFE_DAC_CON1,      .shift = 12,	.mask = 0xf000 },
	{ .addr = AFE_DAC_CON1,      .shift = 24,	.mask = 1 << 24 },
	/* MOD_DAI */
	{ .addr = AFE_MOD_PCM_BASE,  .shift = 0,	.mask = 0xffffffff },
	{ .addr = AFE_MOD_PCM_CUR,   .shift = 0,	.mask = 0xffffffff },
	{ .addr = AFE_IRQ_CNT2,      .shift = 0,	.mask = 0x3ffff },
	{ .addr = AFE_IRQ_MCU_CON,   .shift = 1,	.mask = 1 << 1},
	{ .addr = AFE_IRQ_MCU_CON,   .shift = 8,	.mask = 0xf00 },
	{ .addr = AFE_DAC_CON1,      .shift = 30,	.mask = 1 << 30 },
	{ .addr = AFE_MEMIF_MON0,    .shift = 0,	.mask = 0 },
	/* HDMI */
	{ .addr = AFE_HDMI_OUT_BASE, .shift = 0,	.mask = 0xffffffff },
	{ .addr = AFE_HDMI_OUT_CUR,  .shift = 0,	.mask = 0xffffffff },
	{ .addr = AFE_IRQ_CNT5,      .shift = 0,	.mask = 0x3ffff },
	{ .addr = AFE_IRQ_MCU_CON,   .shift = 12,	.mask = 1 << 12},
	{ .addr = AFE_MEMIF_MON0,    .shift = 0,	.mask = 0 },
	{ .addr = AFE_MEMIF_MON0,    .shift = 0,	.mask = 0 },
	{ .addr = AFE_MEMIF_MON0,    .shift = 0,	.mask = 0 },
};

static unsigned int memif_irq[MTK_AFE_MEMIF_NUM] = {
	MTK_AFE_IRQ_MODE_1,
	MTK_AFE_IRQ_MODE_1,
	MTK_AFE_IRQ_MODE_2,
	MTK_AFE_IRQ_MODE_2,
	MTK_AFE_IRQ_MODE_2,
	MTK_AFE_IRQ_MODE_2,
	MTK_AFE_IRQ_MODE_2,
	MTK_AFE_IRQ_MODE_5,
};

static bool mtk_afe_readable_reg(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case AFE_IRQ_CLR:
		return false;
	default:
		return true;
	}
}

static bool mtk_afe_volatile_reg(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case AFE_DL1_CUR:
	case AFE_DL2_CUR:
	case AFE_VUL_CUR:
	case AFE_DAI_CUR:
	case AFE_AWB_CUR:
	case AFE_MOD_PCM_CUR:
	case AFE_HDMI_OUT_CUR:
	case AFE_IRQ_STATUS:
	case AFE_IRQ_CLR:
	case AFE_ASRC_CON0:
		return true;
	default:
		return false;
	}
}

static const struct regmap_config mtk_afe_regmap_config = {
	.reg_bits = 32,
	.reg_stride = 4,
	.val_bits = 32,
	.max_register = AFE_ADDA2_TOP_CON0,
	.readable_reg = mtk_afe_readable_reg,
	.volatile_reg = mtk_afe_volatile_reg,
	.cache_type = REGCACHE_RBTREE,
};

/* FIXME can we remove this */
void mtk_afe_pll_set_reg(struct mtk_afe_info *afe_info, uint32_t offset,
			 uint32_t value, uint32_t mask)
{
	uint32_t val;

	val = readl(afe_info->apmixedsys_base_addr + offset);
	val &= ~mask;
	val |= value & mask;
	writel(val, afe_info->apmixedsys_base_addr + offset);
}

/* FIXME can we remove this */
void mtk_afe_topck_set_reg(struct mtk_afe_info *afe_info, uint32_t offset,
			   uint32_t value, uint32_t mask)
{
	uint32_t val;

	val = readl(afe_info->topckgen_base_addr + offset);
	val &= ~mask;
	val |= value & mask;
	writel(val, afe_info->topckgen_base_addr + offset);
}

/* irq1 ISR handler */
static void mtk_afe_dl_interrupt_handler(struct mtk_afe_info *afe_info,
					 unsigned int pin)
{
	int afe_consumed_bytes, hw_memory_index;
	unsigned int hw_cur_read_idx = 0;
	struct mtk_afe_block * const block = &afe_info->memif[pin].block;

	regmap_read(afe_info->regmap, afe_info->memif[pin].reg_cur.addr,
		    &hw_cur_read_idx);

	if (hw_cur_read_idx == 0) {
		dev_notice(afe_info->dev, "%s hw_cur_read_idx ==0\n", __func__);
		hw_cur_read_idx = block->phys_buf_addr;
	}

	hw_memory_index = hw_cur_read_idx - block->phys_buf_addr;

	/* get hw consume bytes */
	if (hw_memory_index > block->dma_read_idx)
		afe_consumed_bytes = hw_memory_index - block->dma_read_idx;
	else
		afe_consumed_bytes = block->buffer_size +
				hw_memory_index - block->dma_read_idx;

	block->dma_read_idx += afe_consumed_bytes;
	block->dma_read_idx %= block->buffer_size;

	snd_pcm_period_elapsed(afe_info->memif[pin].sub_stream);
}

static void __mtk_afe_ul_interrupt_handler(struct mtk_afe_info *afe_info,
					   unsigned int pin)
{
	int hw_get_bytes;
	struct mtk_afe_block *block = &afe_info->memif[pin].block;
	uint32_t hw_ptr = 0;

	regmap_read(afe_info->regmap, afe_info->memif[pin].reg_cur.addr,
		    &hw_ptr);

	if (hw_ptr == 0) {
		dev_err(afe_info->dev, "%s hw_ptr = 0\n", __func__);
		return;
	}
	if (block->virt_buf_addr == NULL)
		return;

	/* HW already fill in */
	hw_get_bytes = hw_ptr - block->phys_buf_addr - block->write_idx;

	if (hw_get_bytes < 0)
		hw_get_bytes += block->buffer_size;

	block->write_idx += hw_get_bytes;
	block->write_idx &= block->buffer_size - 1;

	snd_pcm_period_elapsed(afe_info->memif[pin].sub_stream);
}

/* irq2 ISR handler */
static void mtk_afe_ul_interrupt_handler(struct mtk_afe_info *afe_info)
{
	unsigned int afe_dac_con0 = 0;

	regmap_read(afe_info->regmap, AFE_DAC_CON0, &afe_dac_con0);

	if (afe_dac_con0 & 0x8)
		/* handle VUL Context */
		__mtk_afe_ul_interrupt_handler(afe_info, MTK_AFE_PIN_VUL);
	if (afe_dac_con0 & 0x10)
		/* handle DAI Context */
		__mtk_afe_ul_interrupt_handler(afe_info, MTK_AFE_PIN_DAI);
	if (afe_dac_con0 & 0x40)
		/* handle AWB Context */
		__mtk_afe_ul_interrupt_handler(afe_info, MTK_AFE_PIN_AWB);
}

static irqreturn_t mtk_afe_irq_handler(int irq, void *dev_id)
{
	struct mtk_afe_info *afe_info = dev_id;
	unsigned int reg_value = 0;

	regmap_read(afe_info->regmap, AFE_IRQ_STATUS, &reg_value);

	if (reg_value & AFE_IRQ1_BIT)
		mtk_afe_dl_interrupt_handler(afe_info, MTK_AFE_PIN_DL1);

	if (reg_value & AFE_IRQ2_BIT)
		mtk_afe_ul_interrupt_handler(afe_info);

	if (reg_value & AFE_HDMI_IRQ_BIT)
		mtk_afe_dl_interrupt_handler(afe_info, MTK_AFE_PIN_HDMI);

	if (reg_value == 0)
		/* error: interrupt is trigger but no status */
		dev_err(afe_info->dev, "%s reg_value == 0\n", __func__);

	/* clear irq */
	regmap_write(afe_info->regmap, AFE_IRQ_CLR, reg_value);

	return IRQ_HANDLED;
}

static int mtk_afe_init_audio_clk(struct mtk_afe_info *afe_info)
{
	size_t i;

	memcpy(afe_info->clock, aud_clks,
	       sizeof(*aud_clks) * ARRAY_SIZE(aud_clks));
	for (i = 0; i < ARRAY_SIZE(aud_clks); i++) {
		afe_info->clock[i].clk = devm_clk_get(afe_info->dev,
						      aud_clks[i].name);
		if (IS_ERR(afe_info->clock[i].clk)) {
			dev_err(afe_info->dev, "%s devm_clk_get %s fail\n",
				__func__, aud_clks[i].name);
			return PTR_ERR(afe_info->clock[i].clk);
		}
	}
	return 0;
}

static int mtk_afe_memif_init(struct mtk_afe_info *afe_info)
{
	size_t i;

	for (i = 0; i < MTK_AFE_MEMIF_NUM; i++) {
		memcpy(&afe_info->memif[i].reg_base,
		       memif_regs + MTK_AFE_MEMIF_REGS * i,
		       sizeof(struct mtk_afe_reg) * MTK_AFE_MEMIF_REGS);
		afe_info->memif[i].irq_num = memif_irq[i];
	}
	return 0;
}

int mtk_afe_platform_init(struct platform_device *pdev)
{
	struct resource *res;
	struct device *dev = &pdev->dev;
	struct mtk_afe_info *afe_info = NULL;
	struct device_node *np;
	unsigned int irq_id, i;
	unsigned int register_value = 0;
	const char *str;
	int ret;
	struct {
		char *name;
		unsigned int val;
	} of_memif_table[] = {
		{ "dl1",	MTK_AFE_PIN_DL1},
		{ "dl2",	MTK_AFE_PIN_DL2},
		{ "vul",	MTK_AFE_PIN_VUL},
		{ "dai",	MTK_AFE_PIN_DAI},
		{ "awb",	MTK_AFE_PIN_AWB},
		{ "mod_dai",	MTK_AFE_PIN_MOD_DAI},
		{ "hdmi",	MTK_AFE_PIN_HDMI},
	};

	if (!dev->of_node) {
		dev_err(dev, "could not get device tree\n");
		return -EINVAL;
	}

	afe_info = devm_kzalloc(&pdev->dev, sizeof(*afe_info), GFP_KERNEL);
	if (!afe_info)
		return -ENOMEM;

	afe_info->dev = dev;
	platform_set_drvdata(pdev, afe_info);

	irq_id = platform_get_irq(pdev, 0);
	if (!irq_id) {
		dev_err(dev, "no irq for np %s\n", dev->of_node->name);
		return -ENXIO;
	}
	ret = devm_request_irq(dev, irq_id, mtk_afe_irq_handler,
			       IRQF_TRIGGER_LOW, "Afe_ISR_Handle",
			       (void *)afe_info);
	if (ret) {
		dev_err(dev, "could not request_irq\n");
		return ret;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	afe_info->base_addr = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(afe_info->base_addr))
		return PTR_ERR(afe_info->base_addr);
	afe_info->regmap = devm_regmap_init_mmio(dev, afe_info->base_addr,
		&mtk_afe_regmap_config);
	if (IS_ERR(afe_info->regmap))
		return PTR_ERR(afe_info->regmap);

	/* get SRAM base */
	res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	afe_info->afe_sram_address = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(afe_info->afe_sram_address))
		goto end_of_sram;
	afe_info->afe_sram_phy_address = res->start;
	afe_info->afe_sram_size = resource_size(res);

	/* which memif use SRAM */
	ret = of_property_read_string(dev->of_node, "afe-sram", &str);
	if (ret == 0) {
		for (i = 0; i < ARRAY_SIZE(of_memif_table); i++) {
			if (!strcasecmp(str, of_memif_table[i].name)) {
				afe_info->memif[of_memif_table[i].val].use_sram
					= true;
				break;
			}
		}
	} else {
		/* default let dl1 use sram */
		afe_info->memif[MTK_AFE_PIN_DL1].use_sram = true;
	}

end_of_sram:
	dev_info(dev, "IRQ = %d BASE = 0x%p SRAM = 0x%p size = %d\n",
		 irq_id, afe_info->base_addr, afe_info->afe_sram_address,
		 afe_info->afe_sram_size);

	/* FIXME can we remove this TOPCKGEN register base */
	np = of_find_compatible_node(NULL, NULL, "mediatek,mt8173-topckgen");
	afe_info->topckgen_base_addr = of_iomap(np, 0);
	if (!afe_info->topckgen_base_addr) {
		dev_err(dev, "cannot get topckgen_base_addr\n");
		return -EADDRNOTAVAIL;
	}

	/* FIXME can we remove this APMIXEDSYS register base */
	np = of_find_compatible_node(NULL, NULL, "mediatek,mt8173-apmixedsys");
	afe_info->apmixedsys_base_addr = of_iomap(np, 0);
	if (!afe_info->apmixedsys_base_addr) {
		dev_err(dev, "cannot get apmixedsys_base_addr\n");
		return -EADDRNOTAVAIL;
	}

	/* initial audio related clock */
	ret = mtk_afe_init_audio_clk(afe_info);
	if (ret) {
		dev_err(dev, "%s mtk_afe_init_audio_clk fail %d\n",
			__func__, ret);
		return ret;
	}
	/* initial memif registers */
	mtk_afe_memif_init(afe_info);

	/* initial some default settings here */
	/* turn on MTCMOS audio for registers access and audiosys clocks */
	ret = clk_prepare_enable(afe_info->clock[MTK_CLK_SCP_SYS_AUD].clk);
	if (ret)
		dev_err(dev, "%s clk_prepare_enable %s fail %d\n", __func__,
			afe_info->clock[MTK_CLK_SCP_SYS_AUD].name, ret);

	/* turn on afe clock for registers access */
	mtk_afe_clk_on(afe_info);

	/* power off default on clock */
	for (i = 0; i < MTK_CLK_INFRASYS_AUD; i++) {
		if (!afe_info->clock[i].default_on)
			continue;
		ret = clk_prepare_enable(afe_info->clock[i].clk);
		if (ret) {
			dev_err(afe_info->dev,
				"%s clk_prepare_enable %s fail %d\n", __func__,
				afe_info->clock[i].name, ret);
			return ret;
		} else {
			clk_disable_unprepare(afe_info->clock[i].clk);
		}
	}

	/* HDMI */
	mtk_afe_interconn_connect(afe_info,
				  MTK_AFE_INTERCONN_I36,
				  MTK_AFE_INTERCONN_O36);
	mtk_afe_interconn_connect(afe_info,
				  MTK_AFE_INTERCONN_I37,
				  MTK_AFE_INTERCONN_O37);
	mtk_afe_interconn_connect(afe_info,
				  MTK_AFE_INTERCONN_I34,
				  MTK_AFE_INTERCONN_O32);
	mtk_afe_interconn_connect(afe_info,
				  MTK_AFE_INTERCONN_I35,
				  MTK_AFE_INTERCONN_O33);
	mtk_afe_interconn_connect(afe_info,
				  MTK_AFE_INTERCONN_I32,
				  MTK_AFE_INTERCONN_O34);
	mtk_afe_interconn_connect(afe_info,
				  MTK_AFE_INTERCONN_I33,
				  MTK_AFE_INTERCONN_O35);
	mtk_afe_interconn_connect(afe_info,
				  MTK_AFE_INTERCONN_I30,
				  MTK_AFE_INTERCONN_O30);
	mtk_afe_interconn_connect(afe_info,
				  MTK_AFE_INTERCONN_I31,
				  MTK_AFE_INTERCONN_O31);

	/* HDMI TDM */
	register_value |= MTK_AFE_TDM_BCK_INV << 1;
	register_value |= MTK_AFE_TDM_LRCK_NOT_INV << 2;
	register_value |= MTK_AFE_TDM_1_BCK_DELAY << 3;
	register_value |= MTK_AFE_TDM_MSB_ALIGNED << 4; /* I2S mode */
	register_value |= MTK_AFE_TDM_2CH_SDATA << 10;
	register_value |= MTK_AFE_TDM_WLEN_32BIT << 8;
	register_value |= MTK_AFE_TDM_32_BCK_CYCLES << 12;

	/* LRCK TDM WIDTH */
	register_value |= ((MTK_AFE_TDM_WLEN_32BIT << 4) - 1) << 24;
	regmap_update_bits(afe_info->regmap, AFE_TDM_CON1, 0xFFFFFFFE,
			   register_value);

	return ret;
}

int mtk_afe_clk_on(struct mtk_afe_info *afe_info)
{
	int ret;

	ret = clk_prepare_enable(afe_info->clock[MTK_CLK_INFRASYS_AUD].clk);
	if (ret) {
		dev_err(afe_info->dev, "%s afe_infra fail\n", __func__);
		return ret;
	}
	ret = clk_prepare_enable(afe_info->clock[MTK_CLK_TOP_PDN_AUD_BUS].clk);
	if (ret) {
		dev_err(afe_info->dev, "%s intbus fail\n", __func__);
		return ret;
	}
	ret = clk_prepare_enable(afe_info->clock[MTK_CLK_TOP_PDN_AUD].clk);
	if (ret) {
		dev_err(afe_info->dev, "%s top pdn fail\n", __func__);
		return ret;
	}
	ret = clk_prepare_enable(afe_info->clock[MTK_CLK_AFE].clk);
	if (ret)
		dev_err(afe_info->dev, "%s afe fail\n", __func__);
	return ret;
}

int mtk_afe_clk_off(struct mtk_afe_info *afe_info)
{
	clk_disable_unprepare(afe_info->clock[MTK_CLK_AFE].clk);
	clk_disable_unprepare(afe_info->clock[MTK_CLK_TOP_PDN_AUD].clk);
	clk_disable_unprepare(afe_info->clock[MTK_CLK_TOP_PDN_AUD_BUS].clk);
	clk_disable_unprepare(afe_info->clock[MTK_CLK_INFRASYS_AUD].clk);
	return 0;
}

