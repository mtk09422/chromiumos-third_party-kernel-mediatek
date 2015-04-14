/*
 * Mediatek ALSA SoC Afe platform driver
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

#include <linux/delay.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/pm_runtime.h>
#include <sound/soc.h>
#include <linux/debugfs.h>   /* FIXME: for debug */
#include "mtk-afe-common.h"
#include "mtk-afe-connection.h"
#include <dt-bindings/sound/mtk-afe.h>

/*****************************************************************************
 *                  R E G I S T E R       D E F I N I T I O N
 *****************************************************************************/
#define AUDIO_TOP_CON0		0x0000
#define AUDIO_TOP_CON1		0x0004
#define AFE_DAC_CON0		0x0010
#define AFE_DAC_CON1		0x0014
#define AFE_I2S_CON		0x0018
#define AFE_I2S_CON1		0x0034
#define AFE_I2S_CON2		0x0038
#define AFE_I2S_CON3		0x004c
#define AFE_CONN_24BIT		0x006c

/* Memory interface */
#define AFE_DL1_BASE		0x0040
#define AFE_DL1_CUR		0x0044
#define AFE_DL1_END		0x0048
#define AFE_DL2_BASE		0x0050
#define AFE_DL2_CUR		0x0054
#define AFE_DL2_END		0x0058
#define AFE_AWB_BASE		0x0070
#define AFE_AWB_END		0x0078
#define AFE_AWB_CUR		0x007c
#define AFE_VUL_BASE		0x0080
#define AFE_VUL_END		0x0088
#define AFE_VUL_CUR		0x008c
#define AFE_DAI_BASE		0x0090
#define AFE_DAI_END		0x0098
#define AFE_DAI_CUR		0x009c
#define AFE_MOD_PCM_BASE	0x0330
#define AFE_MOD_PCM_END		0x0338
#define AFE_MOD_PCM_CUR		0x033C
#define AFE_HDMI_OUT_BASE	0x0374
#define AFE_HDMI_OUT_CUR	0x0378
#define AFE_HDMI_OUT_END	0x037c

#define AFE_ADDA_DL_SRC2_CON0	0x0108
#define AFE_ADDA_DL_SRC2_CON1	0x010c
#define AFE_ADDA_UL_SRC_CON0	0x0114
#define AFE_ADDA_UL_SRC_CON1	0x0118
#define AFE_ADDA_TOP_CON0	0x0120
#define AFE_ADDA_UL_DL_CON0	0x0124
#define AFE_ADDA_NEWIF_CFG0     0x0138
#define AFE_ADDA_NEWIF_CFG1     0x013C
#define AFE_ADDA2_TOP_CON0	0x0600

#define AFE_HDMI_OUT_CON0	0x0370
#define AFE_APLL1_TUNER_CFG	0x03f0
#define AFE_APLL2_TUNER_CFG	0x03f4

#define AFE_IRQ_MCU_CON		0x03a0
#define AFE_IRQ_STATUS		0x03a4
#define AFE_IRQ_CLR		0x03a8
#define AFE_IRQ_CNT1		0x03ac
#define AFE_IRQ_CNT2		0x03b0
#define AFE_IRQ_MCU_EN		0x03b4
#define AFE_IRQ_CNT5		0x03bc
#define AFE_IRQ_CNT7		0x03dc

#define AFE_TDM_CON1		0x0548
#define AFE_TDM_CON2		0x054c

#define AFE_BASE_END_OFFSET	8
#define AFE_IRQ_STATUS_BITS	0xff

#define AFE_TDM_CON1_BCK_INV			(1 << 1)
#define AFE_TDM_CON1_LRCK_INV			(1 << 2)
#define AFE_TDM_CON1_1_BCK_DELAY		(1 << 3)
#define AFE_TDM_CON1_MSB_ALIGNED		(1 << 4)
#define AFE_TDM_CON1_WLEN_16BIT			(1 << 8)
#define AFE_TDM_CON1_WLEN_32BIT			(2 << 8)
#define AFE_TDM_CON1_2CH_SDATA			(0 << 10)
#define AFE_TDM_CON1_4CH_SDATA			(1 << 10)
#define AFE_TDM_CON1_8CH_SDATA			(2 << 10)
#define AFE_TDM_CON1_16_BCK_CYCLES		(0 << 12)
#define AFE_TDM_CON1_24_BCK_CYCLES		(1 << 12)
#define AFE_TDM_CON1_32_BCK_CYCLES		(2 << 12)
#define AFE_TDM_CON1_LRCK_TDM_WIDTH(x)		(((x) - 1) << 24)

#define MTK_AFE_I2S_CON_PHASE_SHIFT_FIX		(1 << 31)
#define MTK_AFE_I2S_CON_BCK_INVERT		(1 << 29)
#define MTK_AFE_I2S_CON_FROM_IO_MUX		(1 << 28)
#define MTK_AFE_I2S_CON_LOW_JITTER_CLOCK	(1 << 12)
#define MTK_AFE_I2S_CON_INV_LRCK		(1 << 5)
#define MTK_AFE_I2S_CON_FORMAT_I2S		(1 << 3)
#define MTK_AFE_I2S_CON_SRC_SLAVE		(1 << 2)
#define MTK_AFE_I2S_CON_WLEN_32BITS		(1 << 1)
#define MTK_AFE_I2S_CON_ENABLE			(1 << 0)

#define MTK_AFE_I2S_CON3_LR_SWAP		(1 << 31)
#define MTK_AFE_I2S_CON3_LOW_JITTER_CLOCK	(1 << 12)
#define MTK_AFE_I2S_CON3_RATE(x)		(((x) & 0xf) << 8)
#define MTK_AFE_I2S_CON3_INV_LRCK		(1 << 5)
#define MTK_AFE_I2S_CON3_FORMAT_I2S		(1 << 3)
#define MTK_AFE_I2S_CON3_WLEN_32BITS		(1 << 1)
#define MTK_AFE_I2S_CON3_ENABLE			(1 << 0)

#define MTK_AFE_I2S_CON2_LR_SWAP		(1 << 31)
#define MTK_AFE_I2S_CON2_BCK_INVERT		(1 << 23)
#define MTK_AFE_I2S_CON2_LOW_JITTER_CLOCK	(1 << 12)
#define MTK_AFE_I2S_CON2_RATE(x)		(((x) & 0xf) << 8)
#define MTK_AFE_I2S_CON2_FORMAT_I2S		(1 << 3)
#define MTK_AFE_I2S_CON2_WLEN_32BITS		(1 << 1)
#define MTK_AFE_I2S_CON2_ENABLE			(1 << 0)

#define MTK_AFE_I2S_CON1_LR_SWAP		(1 << 31)
#define MTK_AFE_I2S_CON1_LOW_JITTER_CLOCK	(1 << 12)
#define MTK_AFE_I2S_CON1_RATE(x)		(((x) & 0xf) << 8)
#define MTK_AFE_I2S_CON1_INV_LRCK		(1 << 5)
#define MTK_AFE_I2S_CON1_FORMAT_I2S		(1 << 3)
#define MTK_AFE_I2S_CON1_WLEN_32BITS		(1 << 1)
#define MTK_AFE_I2S_CON1_ENABLE			(1 << 0)

enum mtk_afe_tdm_ch_start {
	MTK_AFE_TDM_CH_START_O30_O31 = 0,
	MTK_AFE_TDM_CH_START_O32_O33,
	MTK_AFE_TDM_CH_START_O34_O35,
	MTK_AFE_TDM_CH_START_O36_O37,
	MTK_AFE_TDM_CH_ZERO,
};

enum mtk_afe_i2s_rate {
	MTK_AFE_I2S_RATE_8K = 0,
	MTK_AFE_I2S_RATE_11K = 1,
	MTK_AFE_I2S_RATE_12K = 2,
	MTK_AFE_I2S_RATE_16K = 4,
	MTK_AFE_I2S_RATE_22K = 5,
	MTK_AFE_I2S_RATE_24K = 6,
	MTK_AFE_I2S_RATE_32K = 8,
	MTK_AFE_I2S_RATE_44K = 9,
	MTK_AFE_I2S_RATE_48K = 10,
	MTK_AFE_I2S_RATE_88K = 11,
	MTK_AFE_I2S_RATE_96K = 12,
	MTK_AFE_I2S_RATE_174K = 13,
	MTK_AFE_I2S_RATE_192K = 14,
};

/* FIXME: for debug */
#define AUDIO_APLL1_CON0        0x02A0
#define AUDIO_APLL1_CON1        0x02A4
#define AUDIO_APLL1_CON2        0x02A8
#define AUDIO_APLL1_PWR_CON0    0x02B0
#define AUDIO_APLL2_CON0        0x02B4
#define AUDIO_APLL2_CON1        0x02B8
#define AUDIO_APLL2_CON2        0x02C0
#define AUDIO_APLL2_PWR_CON0    0x02C4
#define AUDIO_CLK_CFG_6		0x00A0
#define AUDIO_CLK_CFG_7		0x00B0

/* FIXME APLL tuner move to CCF */
#define AP_PLL_CON5		0x0014
void mtk_afe_pll_set_reg(struct mtk_afe *afe, uint32_t offset,
			 uint32_t value, uint32_t mask)
{
	uint32_t val;

	val = readl(afe->apmixedsys_base_addr + offset);
	val &= ~mask;
	val |= value & mask;
	writel(val, afe->apmixedsys_base_addr + offset);
}

/* FIXME SPDIF clk move to CCF */
enum mtk_afe_apll_clk_freq {
	MTK_AFE_APLL1_CLK_FREQ = (22579200 * 8),
	MTK_AFE_APLL2_CLK_FREQ = (24576000 * 8),
};

#define AUDIO_CLK_AUDDIV_0	0x0120
#define AUDIO_CLK_AUDDIV_1	0x0124
#define AUDIO_CLK_AUDDIV_2	0x0128
#define AUDIO_CLK_AUDDIV_3	0x012C

void mtk_afe_topck_set_reg(struct mtk_afe *afe, uint32_t offset,
			   uint32_t value, uint32_t mask)
{
	uint32_t val;

	val = readl(afe->topckgen_base_addr + offset);
	val &= ~mask;
	val |= value & mask;
	writel(val, afe->topckgen_base_addr + offset);
}

/* FIXME: for debug */
static inline uint32_t mtk_afe_topck_get_reg(struct mtk_afe *afe,
					     uint32_t offset)
{
	return readl(afe->topckgen_base_addr + offset);
}

/* FIXME: for debug */
static inline uint32_t mtk_afe_pll_get_reg(struct mtk_afe *afe,
					   uint32_t offset)
{
	return readl(afe->apmixedsys_base_addr + offset);
}

static struct dentry *mt8173_afe_debugfs;

static int mt8173_afe_debug_open(struct inode *inode, struct file *file)
{
	file->private_data = inode->i_private;
	return 0;
}

static ssize_t mt8173_afe_debug_read(struct file *file, char __user *buf,
				     size_t count, loff_t *pos)
{
	struct mtk_afe *afe = file->private_data;
	const int size = 4096;
	char buffer[size];
	int n = 0;

	n += scnprintf(buffer + n, size - n, "AUDIO_CLK_CFG_6 = 0x%x\n",
		       mtk_afe_topck_get_reg(afe, AUDIO_CLK_CFG_6));
	n += scnprintf(buffer + n, size - n, "AUDIO_CLK_CFG_7 = 0x%x\n",
		       mtk_afe_topck_get_reg(afe, AUDIO_CLK_CFG_7));
	n += scnprintf(buffer + n, size - n, "AUDIO_CLK_AUDDIV_0 = 0x%x\n",
		       mtk_afe_topck_get_reg(afe, AUDIO_CLK_AUDDIV_0));
	n += scnprintf(buffer + n, size - n, "AUDIO_CLK_AUDDIV_1 = 0x%x\n",
		       mtk_afe_topck_get_reg(afe, AUDIO_CLK_AUDDIV_1));
	n += scnprintf(buffer + n, size - n, "AUDIO_CLK_AUDDIV_2 = 0x%x\n",
		       mtk_afe_topck_get_reg(afe, AUDIO_CLK_AUDDIV_2));
	n += scnprintf(buffer + n, size - n, "AUDIO_CLK_AUDDIV_3 = 0x%x\n",
		       mtk_afe_topck_get_reg(afe, AUDIO_CLK_AUDDIV_3));

	n += scnprintf(buffer + n, size - n, "AP_PLL_CON5 = 0x%x\n",
		       mtk_afe_pll_get_reg(afe, AP_PLL_CON5));
	n += scnprintf(buffer + n, size - n, "APLL1_CON0 = 0x%x\n",
		       mtk_afe_pll_get_reg(afe, AUDIO_APLL1_CON0));
	n += scnprintf(buffer + n, size - n, "APLL1_CON1 = 0x%x\n",
		       mtk_afe_pll_get_reg(afe, AUDIO_APLL1_CON1));
	n += scnprintf(buffer + n, size - n, "APLL1_CON2 = 0x%x\n",
		       mtk_afe_pll_get_reg(afe, AUDIO_APLL1_CON2));
	n += scnprintf(buffer + n, size - n, "APLL1_PWR_CON0 = 0x%x\n",
		       mtk_afe_pll_get_reg(afe, AUDIO_APLL1_PWR_CON0));
	n += scnprintf(buffer + n, size - n, "APLL2_CON0 = 0x%x\n",
		       mtk_afe_pll_get_reg(afe, AUDIO_APLL2_CON0));
	n += scnprintf(buffer + n, size - n, "APLL2_CON1 = 0x%x\n",
		       mtk_afe_pll_get_reg(afe, AUDIO_APLL2_CON1));
	n += scnprintf(buffer + n, size - n, "APLL2_CON2 = 0x%x\n",
		       mtk_afe_pll_get_reg(afe, AUDIO_APLL2_CON2));
	n += scnprintf(buffer + n, size - n, "APLL2_PWR_CON0 = 0x%x\n",
		       mtk_afe_pll_get_reg(afe, AUDIO_APLL2_PWR_CON0));

	return simple_read_from_buffer(buf, count, pos, buffer, n);
}

static const struct file_operations mt8173_afe_debug_ops = {
	.open = mt8173_afe_debug_open,
	.read = mt8173_afe_debug_read,
};

static const struct snd_pcm_hardware mtk_afe_hardware = {
	.info = (SNDRV_PCM_INFO_MMAP | SNDRV_PCM_INFO_INTERLEAVED |
		 SNDRV_PCM_INFO_RESUME | SNDRV_PCM_INFO_MMAP_VALID),
	.buffer_bytes_max = 256 * 1024,
	.period_bytes_min = 512,
	.period_bytes_max = 128 * 1024,
	.periods_min = 2,
	.periods_max = 256,
	.fifo_size = 0,
};

static int mtk_afe_pcm_open(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct mtk_afe *afe = snd_soc_platform_get_drvdata(rtd->platform);
	struct mtk_afe_io *io = &afe->ios[rtd->cpu_dai->id];
	struct mtk_afe_memif *memif;
	int ret;

	if (io->mem[substream->stream] < 0)
		/* memif is not specified */
		return -ENXIO;
	memif = &afe->memif[io->mem[substream->stream]];
	memif->substream = substream;

	snd_soc_set_runtime_hwparams(substream, &mtk_afe_hardware);
	ret = snd_pcm_hw_constraint_integer(runtime,
					    SNDRV_PCM_HW_PARAM_PERIODS);
	if (ret < 0) {
		dev_err(afe->dev, "snd_pcm_hw_constraint_integer failed\n");
		return ret;
	}

	if (!memif->use_sram)
		return 0;

	return snd_pcm_hw_constraint_minmax(runtime,
					    SNDRV_PCM_HW_PARAM_BUFFER_BYTES,
					    mtk_afe_hardware.period_bytes_min,
					    afe->sram_size);
}

static int mtk_afe_pcm_close(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct mtk_afe *afe = snd_soc_platform_get_drvdata(rtd->platform);
	struct mtk_afe_io *io = &afe->ios[rtd->cpu_dai->id];

	afe->memif[io->mem[substream->stream]].substream = NULL;

	return 0;
}

static int mtk_afe_pcm_hw_params(struct snd_pcm_substream *substream,
				 struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct mtk_afe *afe = snd_soc_platform_get_drvdata(rtd->platform);
	struct mtk_afe_io *io = &afe->ios[rtd->cpu_dai->id];
	struct mtk_afe_memif *memif = &afe->memif[io->mem[substream->stream]];
	int ret;

	dev_dbg(afe->dev,
		"%s period = %u,rate= %u, channels=%u\n",
		__func__, params_period_size(params), params_rate(params),
		params_channels(params));

	if (memif->use_sram) {
		struct snd_dma_buffer *dma_buf = &substream->dma_buffer;
		int size = params_buffer_bytes(params);

		memif->buffer_size = size;
		memif->phys_buf_addr = afe->sram_phy_address;

		dma_buf->bytes = size;
		dma_buf->area = (unsigned char *)afe->sram_address;
		dma_buf->addr = afe->sram_phy_address;
		dma_buf->dev.type = SNDRV_DMA_TYPE_DEV;
		dma_buf->dev.dev = substream->pcm->card->dev;
		snd_pcm_set_runtime_buffer(substream, dma_buf);
	} else {
		ret = snd_pcm_lib_malloc_pages(substream,
					       params_buffer_bytes(params));
		if (ret < 0)
			return ret;

		memif->phys_buf_addr = substream->runtime->dma_addr;
		memif->buffer_size = substream->runtime->dma_bytes;
	}
	memif->hw_ptr = 0;

	/* start */
	regmap_write(afe->regmap,
		     memif->data->reg_ofs_base, memif->phys_buf_addr);
	/* end */
	regmap_write(afe->regmap,
		     memif->data->reg_ofs_base + AFE_BASE_END_OFFSET,
		     memif->phys_buf_addr + memif->buffer_size - 1);

	return 0;
}

static int mtk_afe_pcm_hw_free(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct mtk_afe *afe = snd_soc_platform_get_drvdata(rtd->platform);
	struct mtk_afe_io *io = &afe->ios[rtd->cpu_dai->id];
	struct mtk_afe_memif *memif = &afe->memif[io->mem[substream->stream]];

	if (memif->use_sram)
		return 0;

	return snd_pcm_lib_free_pages(substream);
}

static snd_pcm_uframes_t mtk_afe_pcm_pointer
			 (struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct mtk_afe *afe = snd_soc_platform_get_drvdata(rtd->platform);
	struct mtk_afe_io *io = &afe->ios[rtd->cpu_dai->id];
	struct mtk_afe_memif *memif = &afe->memif[io->mem[substream->stream]];

	return bytes_to_frames(substream->runtime, memif->hw_ptr);
}

static const struct snd_pcm_ops mtk_afe_pcm_ops = {
	.open = mtk_afe_pcm_open,
	.close = mtk_afe_pcm_close,
	.ioctl = snd_pcm_lib_ioctl,
	.hw_params = mtk_afe_pcm_hw_params,
	.hw_free = mtk_afe_pcm_hw_free,
	.pointer = mtk_afe_pcm_pointer,
};

static int mtk_afe_pcm_new(struct snd_soc_pcm_runtime *rtd)
{
	size_t size;
	struct snd_card *card = rtd->card->snd_card;
	struct snd_pcm *pcm = rtd->pcm;

	size = mtk_afe_hardware.buffer_bytes_max;

	return snd_pcm_lib_preallocate_pages_for_all(pcm, SNDRV_DMA_TYPE_DEV,
						     card->dev, size, size);
}

static void mtk_afe_pcm_free(struct snd_pcm *pcm)
{
	snd_pcm_lib_preallocate_free_for_all(pcm);
}

static int mtk_afe_pcm_probe(struct snd_soc_platform *platform)
{
	struct mtk_afe *afe = snd_soc_platform_get_drvdata(platform);

	mt8173_afe_debugfs =
		debugfs_create_file("afe_reg", S_IFREG | S_IRUGO,
				    NULL,
				    (void *)afe,
				    &mt8173_afe_debug_ops);
	return 0;
}

static const struct snd_soc_platform_driver mtk_afe_pcm_platform = {
	.ops = &mtk_afe_pcm_ops,
	.probe = mtk_afe_pcm_probe,
	.pcm_new = mtk_afe_pcm_new,
	.pcm_free = mtk_afe_pcm_free,
};

struct mtk_afe_rate {
	int rate;
	int regvalue;
};

static const struct mtk_afe_rate mtk_afe_adda_rates[] = {
	{ .rate = 8000, .regvalue = 0 },
	{ .rate = 11025, .regvalue = 1 },
	{ .rate = 12000, .regvalue = 2 },
	{ .rate = 16000, .regvalue = 3 },
	{ .rate = 22050, .regvalue = 4 },
	{ .rate = 24000, .regvalue = 5 },
	{ .rate = 32000, .regvalue = 6 },
	{ .rate = 44100, .regvalue = 7 },
	{ .rate = 48000, .regvalue = 8 },
};

static int mtk_afe_adda_fs(u32 sample_rate)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(mtk_afe_adda_rates); i++)
		if (mtk_afe_adda_rates[i].rate == sample_rate)
			return mtk_afe_adda_rates[i].regvalue;

	return -EINVAL;
}

static const struct mtk_afe_rate mtk_afe_i2s_rates[] = {
	{ .rate = 8000, .regvalue = MTK_AFE_I2S_RATE_8K },
	{ .rate = 11025, .regvalue = MTK_AFE_I2S_RATE_11K },
	{ .rate = 12000, .regvalue = MTK_AFE_I2S_RATE_12K },
	{ .rate = 16000, .regvalue = MTK_AFE_I2S_RATE_16K },
	{ .rate = 22050, .regvalue = MTK_AFE_I2S_RATE_22K },
	{ .rate = 24000, .regvalue = MTK_AFE_I2S_RATE_24K },
	{ .rate = 32000, .regvalue = MTK_AFE_I2S_RATE_32K },
	{ .rate = 44100, .regvalue = MTK_AFE_I2S_RATE_44K },
	{ .rate = 48000, .regvalue = MTK_AFE_I2S_RATE_48K },
	{ .rate = 88000, .regvalue = MTK_AFE_I2S_RATE_88K },
	{ .rate = 96000, .regvalue = MTK_AFE_I2S_RATE_96K },
	{ .rate = 174000, .regvalue = MTK_AFE_I2S_RATE_174K },
	{ .rate = 192000, .regvalue = MTK_AFE_I2S_RATE_192K },
};

static int mtk_afe_i2s_fs(u32 sample_rate)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(mtk_afe_i2s_rates); i++)
		if (mtk_afe_i2s_rates[i].rate == sample_rate)
			return mtk_afe_i2s_rates[i].regvalue;

	return -EINVAL;
}

static int mtk_afe_set_adda_dac_out(struct mtk_afe *afe, uint32_t rate)
{
	u32 audio_i2s_dac = 0;
	u32 con0, con1;

	/* set dl src2 */
	con0 = (mtk_afe_adda_fs(rate) << 28) | (0x03 << 24) | (0x03 << 11);

	/* 8k or 16k voice mode */
	if (con0 == 0 || con0 == 3)
		con0 |= 0x01 << 5;

	/* SA suggests to apply -0.3db to audio/speech path */
	con0 = con0 | (0x01 << 1);
	con1 = 0xf74f0000;

	/* set and turn off */
	regmap_write(afe->regmap, AFE_ADDA_DL_SRC2_CON0, con0);

	regmap_write(afe->regmap, AFE_ADDA_DL_SRC2_CON1, 0xf74f0000);

	audio_i2s_dac =	MTK_AFE_I2S_CON1_FORMAT_I2S |
		MTK_AFE_I2S_CON1_RATE(mtk_afe_i2s_fs(rate));

	/* set and turn off */
	regmap_write(afe->regmap, AFE_I2S_CON1, audio_i2s_dac);
	return 0;
}

static void mtk_afe_pmic_shutdown(struct mtk_afe *afe,
				  struct snd_pcm_substream *substream)
{
	/* output */
	regmap_update_bits(afe->regmap, AFE_ADDA_DL_SRC2_CON0, 1, 0);
	regmap_update_bits(afe->regmap, AFE_I2S_CON1, 1, 0);

	/* input */
	regmap_update_bits(afe->regmap, AFE_ADDA_UL_SRC_CON0, 1, 0);
	/* disable ADDA */
	regmap_update_bits(afe->regmap, AFE_ADDA_UL_DL_CON0, 1, false);
}

static int mtk_afe_pmic_prepare(struct mtk_afe *afe,
				struct snd_pcm_substream *substream)
{
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		mtk_afe_set_adda_dac_out(afe, substream->runtime->rate);
		regmap_update_bits(afe->regmap, AFE_ADDA_DL_SRC2_CON0, 1, 1);
		regmap_update_bits(afe->regmap, AFE_I2S_CON1, 1, 1);

	} else {
		u32 rate = mtk_afe_i2s_fs(substream->runtime->rate);
		u32 voice_mode = 0;

		/* Use Int ADC */
		regmap_update_bits(afe->regmap, AFE_ADDA_TOP_CON0, 1, 0);

		if (rate == MTK_AFE_I2S_RATE_8K)
			voice_mode = 0;
		else if (rate == MTK_AFE_I2S_RATE_16K)
			voice_mode = 1;
		else if (rate == MTK_AFE_I2S_RATE_32K)
			voice_mode = 2;
		else if (rate == MTK_AFE_I2S_RATE_48K)
			voice_mode = 3;

		/* set and turn off */
		regmap_write(afe->regmap, AFE_ADDA_UL_SRC_CON0,
			     ((voice_mode << 2) | voice_mode) << 17);

		regmap_update_bits(afe->regmap, AFE_ADDA_NEWIF_CFG1,
				   0xc00, ((voice_mode < 3) ? 1 : 3) << 10);

		regmap_update_bits(afe->regmap, AFE_ADDA_UL_SRC_CON0, 1, 1);
	}
	/* enable ADDA */
	regmap_update_bits(afe->regmap, AFE_ADDA_UL_DL_CON0, 1, true);
	return 0;
}

static int mtk_afe_set_2nd_i2s(struct mtk_afe *afe, bool doublew,
			       uint32_t rate, bool bck_inv, bool slave)
{
	uint32_t val;
	unsigned int fs = mtk_afe_i2s_fs(rate);

	regmap_update_bits(afe->regmap, AFE_DAC_CON1, 0xf00, fs << 8);

	val = MTK_AFE_I2S_CON_PHASE_SHIFT_FIX |
	      MTK_AFE_I2S_CON_FROM_IO_MUX |
	      MTK_AFE_I2S_CON_LOW_JITTER_CLOCK |
	      MTK_AFE_I2S_CON_FORMAT_I2S;

	if (bck_inv)
		val |= MTK_AFE_I2S_CON_BCK_INVERT;
	if (slave)
		val |= MTK_AFE_I2S_CON_SRC_SLAVE;
	if (doublew)
		val |= MTK_AFE_I2S_CON_WLEN_32BITS;

	regmap_update_bits(afe->regmap,
			   AFE_I2S_CON, 0xfffffffe, val);

	/* set output */
	val = MTK_AFE_I2S_CON3_LOW_JITTER_CLOCK |
	      MTK_AFE_I2S_CON3_RATE(fs) |
	      MTK_AFE_I2S_CON3_FORMAT_I2S;

	if (doublew)
		val |= MTK_AFE_I2S_CON3_WLEN_32BITS;

	regmap_update_bits(afe->regmap,
			   AFE_I2S_CON3, 0xfffffffe, val);
	return 0;
}

static int mtk_afe_set_i2s(struct mtk_afe *afe, uint32_t rate)
{
	uint32_t val;
	unsigned int fs = mtk_afe_i2s_fs(rate);

	/* from external ADC */
	regmap_update_bits(afe->regmap, AFE_ADDA2_TOP_CON0, 0x1, 0x1);

	/* set input */
	val = MTK_AFE_I2S_CON2_LOW_JITTER_CLOCK |
	      MTK_AFE_I2S_CON2_RATE(fs) |
	      MTK_AFE_I2S_CON2_FORMAT_I2S;

	regmap_update_bits(afe->regmap,
			   AFE_I2S_CON2, 0xfffffffe, val);

	/* set output */
	val = MTK_AFE_I2S_CON1_LOW_JITTER_CLOCK |
	      MTK_AFE_I2S_CON1_RATE(fs) |
	      MTK_AFE_I2S_CON1_FORMAT_I2S;

	regmap_update_bits(afe->regmap,
			   AFE_I2S_CON1, 0xfffffffe, val);
	return 0;
}

static void mtk_afe_set_2nd_i2s_enable(struct mtk_afe *afe,
				       uint32_t rate, bool enable)
{
	int val;

	regmap_read(afe->regmap, AFE_I2S_CON, &val);
	if (!!(val & MTK_AFE_I2S_CON_ENABLE) == enable)
		return; /* must skip soft reset */

	/* 2nd I2S soft reset begin */
	regmap_update_bits(afe->regmap, AUDIO_TOP_CON1, 2, 2);

	/* input */
	regmap_update_bits(afe->regmap, AFE_I2S_CON, 1, enable);

	/* output */
	regmap_update_bits(afe->regmap, AFE_I2S_CON3, 1, enable);

	/* 2nd I2S soft reset end */
	udelay(1);
	regmap_update_bits(afe->regmap, AUDIO_TOP_CON1, 2, 0);
}

static void mtk_afe_set_i2s_enable(struct mtk_afe *afe,
				   uint32_t rate, bool enable)
{
	int val;

	regmap_read(afe->regmap, AFE_I2S_CON2, &val);
	if (!!(val & MTK_AFE_I2S_CON2_ENABLE) == enable)
		return; /* must skip soft reset */

	/* I2S soft reset begin */
	regmap_update_bits(afe->regmap, AUDIO_TOP_CON1, 4, 4);

	/* input */
	regmap_update_bits(afe->regmap, AFE_I2S_CON2, 1, enable);

	/* output */
	regmap_update_bits(afe->regmap, AFE_I2S_CON1, 1, enable);

	/* I2S soft reset end */
	udelay(1);
	regmap_update_bits(afe->regmap, AUDIO_TOP_CON1, 4, 0);
}

static void mtk_afe_i2s_shutdown(struct mtk_afe *afe,
				 struct snd_pcm_substream *substream)
{
	mtk_afe_set_i2s_enable(afe, substream->runtime->rate, false);
}

static int mtk_afe_i2s_prepare(struct mtk_afe *afe,
			       struct snd_pcm_substream *substream)
{
	/* config I2S */
	mtk_afe_set_i2s(afe, substream->runtime->rate);
	mtk_afe_set_i2s_enable(afe, substream->runtime->rate, true);

	return 0;
}

static void mtk_afe_i2s2_shutdown(struct mtk_afe *afe,
				  struct snd_pcm_substream *substream)
{
	mtk_afe_set_2nd_i2s_enable(afe, substream->runtime->rate, false);
}

static int mtk_afe_i2s2_prepare(struct mtk_afe *afe,
				struct snd_pcm_substream *substream)
{
	/* config I2S */
	mtk_afe_set_2nd_i2s(afe, false, substream->runtime->rate, false, false);
	mtk_afe_set_2nd_i2s_enable(afe, substream->runtime->rate, true);
	return 0;
}

static int mtk_afe_hdmi_start(struct mtk_afe *afe,
			      struct snd_pcm_substream *substream)
{
	/* FIXME SPDIF clk move to CCF */
	unsigned int rate = substream->runtime->rate;
	unsigned int mclk_div;

	if (rate % 11025 == 0) {
		mtk_afe_topck_set_reg(afe, AUDIO_CLK_AUDDIV_0, 0 << 9, 1 << 9);
		mclk_div = (MTK_AFE_APLL1_CLK_FREQ / 128 / rate) - 1;
	} else {
		mtk_afe_topck_set_reg(afe, AUDIO_CLK_AUDDIV_0, 1 << 9, 1 << 9);
		mclk_div = (MTK_AFE_APLL2_CLK_FREQ / 128 / rate) - 1;
	}
	mtk_afe_topck_set_reg(afe, AUDIO_CLK_AUDDIV_3,
			      mclk_div << 24, 0xff000000);
	mtk_afe_topck_set_reg(afe, AUDIO_CLK_AUDDIV_3, 0 << 14, 1 << 14);

	regmap_update_bits(afe->regmap, AUDIO_TOP_CON0,
			   (1 << 20) | (1 << 21), 0);

	/* enable Out control */
	regmap_update_bits(afe->regmap, AFE_HDMI_OUT_CON0, 1, 1);

	/* enable tdm */
	regmap_update_bits(afe->regmap, AFE_TDM_CON1, 1, 1);

	return 0;
}

static void mtk_afe_hdmi_pause(struct mtk_afe *afe,
			       struct snd_pcm_substream *substream)
{
	/* disable tdm */
	regmap_update_bits(afe->regmap, AFE_TDM_CON1, 1, 0);

	/* disable Out control */
	regmap_update_bits(afe->regmap, AFE_HDMI_OUT_CON0, 1, 0);

	/* FIXME SPDIF clk move to CCF */
	mtk_afe_topck_set_reg(afe, AUDIO_CLK_AUDDIV_3, 1 << 14, 1 << 14);

	regmap_update_bits(afe->regmap, AUDIO_TOP_CON0,
			   (1 << 20) | (1 << 21), (1 << 20) | (1 << 21));
}

static int mtk_afe_hdmi_prepare(struct mtk_afe *afe,
				struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	u32 val;

	val = AFE_TDM_CON1_BCK_INV |
		AFE_TDM_CON1_1_BCK_DELAY |
		AFE_TDM_CON1_MSB_ALIGNED | /* I2S mode */
		AFE_TDM_CON1_WLEN_32BIT |
		AFE_TDM_CON1_32_BCK_CYCLES |
		AFE_TDM_CON1_LRCK_TDM_WIDTH(32);

	regmap_update_bits(afe->regmap, AFE_TDM_CON1, ~1, val);

	/* set tdm2 config */
	switch (runtime->channels) {
	case 7:
	case 8:
		val = MTK_AFE_TDM_CH_START_O30_O31;
		val |= (MTK_AFE_TDM_CH_START_O32_O33 << 4);
		val |= (MTK_AFE_TDM_CH_START_O34_O35 << 8);
		val |= (MTK_AFE_TDM_CH_START_O36_O37 << 12);
		break;
	case 5:
	case 6:
		val = MTK_AFE_TDM_CH_START_O30_O31;
		val |= (MTK_AFE_TDM_CH_START_O32_O33 << 4);
		val |= (MTK_AFE_TDM_CH_START_O34_O35 << 8);
		val |= (MTK_AFE_TDM_CH_ZERO << 12);
		break;
	case 3:
	case 4:
		val = MTK_AFE_TDM_CH_START_O30_O31;
		val |= (MTK_AFE_TDM_CH_START_O32_O33 << 4);
		val |= (MTK_AFE_TDM_CH_ZERO << 8);
		val |= (MTK_AFE_TDM_CH_ZERO << 12);
		break;
	case 1:
	case 2:
		val = MTK_AFE_TDM_CH_START_O30_O31;
		val |= (MTK_AFE_TDM_CH_ZERO << 4);
		val |= (MTK_AFE_TDM_CH_ZERO << 8);
		val |= (MTK_AFE_TDM_CH_ZERO << 12);
		break;
	default:
		val = 0;
	}
	regmap_update_bits(afe->regmap, AFE_TDM_CON2,
			   0x0000ffff, val);

	regmap_update_bits(afe->regmap, AFE_HDMI_OUT_CON0,
			   0x000000f0, runtime->channels << 4);
	return 0;
}

static int mtk_set_connections(struct mtk_afe *afe, struct mtk_afe_io *io)
{
	int i, ret;

	for (i = 0; i < io->num_connections; i++) {
		ret = mtk_afe_interconn_connect(afe, io->connections[i * 2],
						io->connections[i * 2 + 1],
						0);
		if (ret) {
			dev_err(afe->dev, "Cannot create connection %d <-> %d\n",
				io->connections[i * 2],
				io->connections[i * 2 + 1]);
			return ret;
		}
	}

	return 0;
}

static int mtk_afe_dais_startup(struct snd_pcm_substream *substream,
				struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct mtk_afe *afe = snd_soc_platform_get_drvdata(rtd->platform);
	struct mtk_afe_io *io = &afe->ios[rtd->cpu_dai->id];
	int ret;

	dev_dbg(afe->dev, "%s\n", __func__);

	if (dai->active)
		return 0;

	ret = mtk_set_connections(afe, io);
	if (ret)
		return ret;

	dev_dbg(afe->dev, "enabling io %s\n", io->data->name);
	if (io->m_ck) {
		ret = clk_prepare_enable(io->m_ck);
		if (ret) {
			dev_err(afe->dev, "Failed to enable m_ck\n");
			return ret;
		}
		regmap_update_bits(afe->regmap, AUDIO_TOP_CON0,
				   (1 << 8) | (1 << 9), 0);

		/* FIXME APLL tuner move to CCF */
		afe->apll_enable++;
		if (afe->apll_enable == 1) {
			regmap_update_bits(afe->regmap, AUDIO_TOP_CON0,
					   (1 << 18) | (1 << 19), 0);
			regmap_update_bits(afe->regmap,
					   AFE_APLL1_TUNER_CFG,
					   0xfff7, 0x8033);
			regmap_update_bits(afe->regmap,
					   AFE_APLL2_TUNER_CFG,
					   0xfff7, 0x8035);
			mtk_afe_pll_set_reg(afe, AP_PLL_CON5, 0x3, 0x3);
		}
	}

	if (io->b_ck) {
		ret = clk_prepare_enable(io->b_ck);
		if (ret) {
			dev_err(afe->dev, "Failed to enable b_ck\n");
			return ret;
		}
	}

	return 0;
}

static void mtk_afe_dais_shutdown(struct snd_pcm_substream *substream,
				  struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct mtk_afe *afe = snd_soc_platform_get_drvdata(rtd->platform);
	struct mtk_afe_io *io = &afe->ios[rtd->cpu_dai->id];

	dev_dbg(afe->dev, "%s\n", __func__);

	if (dai->active)
		return;

	if (io->data->shutdown)
		io->data->shutdown(afe, substream);

	if (io->m_ck) {
		regmap_update_bits(afe->regmap, AUDIO_TOP_CON0,
				   (1 << 8) | (1 << 9),
				   (1 << 8) | (1 << 9));
		clk_disable_unprepare(io->m_ck);

		/* FIXME APLL tuner move to CCF */
		afe->apll_enable--;
		if (afe->apll_enable == 0) {
			regmap_update_bits(afe->regmap, AUDIO_TOP_CON0,
					   (1 << 18) | (1 << 19),
					   (1 << 18) | (1 << 19));
			regmap_update_bits(afe->regmap,
					   AFE_APLL1_TUNER_CFG, 0x1, 0);
			regmap_update_bits(afe->regmap,
					   AFE_APLL2_TUNER_CFG, 0x1, 0);
			mtk_afe_pll_set_reg(afe, AP_PLL_CON5, 0x0, 0x3);
		}
		if (afe->apll_enable < 0)
			afe->apll_enable = 0;
	}
	if (io->b_ck)
		clk_disable_unprepare(io->b_ck);
	/* disable AFE */
	regmap_update_bits(afe->regmap, AFE_DAC_CON0, 1, 0);
}

static int mtk_afe_dais_hw_params(struct snd_pcm_substream *substream,
				  struct snd_pcm_hw_params *params,
				  struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct mtk_afe *afe = snd_soc_platform_get_drvdata(rtd->platform);
	struct mtk_afe_io *io = &afe->ios[rtd->cpu_dai->id];
	struct mtk_afe_memif *memif = &afe->memif[io->mem[substream->stream]];
	unsigned int mono;
	int sample_rate = params_rate(params);
	int fs_shift = memif->data->fs_shift;
	u32 val;

	/* set channel */
	if (memif->data->mono_shift >= 0) {
		mono = (params_channels(params) == 1) ? 1 : 0;
		regmap_update_bits(afe->regmap, AFE_DAC_CON1,
				   1 << memif->data->mono_shift,
				   mono << memif->data->mono_shift);
	}

	/* set rate */
	if (fs_shift < 0)
		return 0;
	if (memif->data->id == MTK_AFE_MEMIF_DAI ||
	    memif->data->id == MTK_AFE_MEMIF_MOD_DAI) {
		if (sample_rate == 8000)
			val = 0;
		else if (sample_rate == 16000)
			val = 1;
		else if (sample_rate == 32000)
			val = 2;
		else
			return -EINVAL;
		if (memif->data->id == MTK_AFE_MEMIF_DAI)
			regmap_update_bits(afe->regmap, AFE_DAC_CON0,
					   0x3 << fs_shift, val << fs_shift);
		else
			regmap_update_bits(afe->regmap, AFE_DAC_CON1,
					   0x3 << fs_shift, val << fs_shift);

	} else {
		regmap_update_bits(afe->regmap, AFE_DAC_CON1,
				   0xf << fs_shift,
				   mtk_afe_i2s_fs(sample_rate) << fs_shift);
	}

	return 0;
}

static int mtk_afe_dais_prepare(struct snd_pcm_substream *substream,
				struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_pcm_runtime * const runtime = substream->runtime;
	struct mtk_afe *afe = snd_soc_platform_get_drvdata(rtd->platform);
	struct mtk_afe_io *io = &afe->ios[rtd->cpu_dai->id];
	int ret;

	dev_dbg(afe->dev, "%s: %d\n", __func__, substream->runtime->rate);

	if (io->m_ck) {
		ret = clk_set_rate(io->m_ck, runtime->rate * io->mck_mul);
		if (ret) {
			dev_err(afe->dev, "Failed to set m_ck rate\n");
			return ret;
		}
	}

	if (io->b_ck) {
		ret = clk_set_rate(io->b_ck, runtime->rate *
				   runtime->channels * 32);
		if (ret) {
			dev_err(afe->dev, "Failed to set b_ck rate\n");
			return ret;
		}
	}

	if (io->data->prepare)
		ret = io->data->prepare(afe, substream);

	/* enable AFE */
	regmap_update_bits(afe->regmap, AFE_DAC_CON0, 1, 1);
	return 0;
}

static int mtk_afe_dais_trigger(struct snd_pcm_substream *substream, int cmd,
				struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_pcm_runtime * const runtime = substream->runtime;
	struct mtk_afe *afe = snd_soc_platform_get_drvdata(rtd->platform);
	struct mtk_afe_io *io = &afe->ios[rtd->cpu_dai->id];
	struct mtk_afe_memif *memif = &afe->memif[io->mem[substream->stream]];
	unsigned int counter = runtime->period_size;
	int ret;
	int enable_shift = memif->data->enable_shift;

	dev_info(afe->dev, "%s %s cmd=%d\n", __func__, memif->data->name, cmd);

	if (!memif->irqdata) {
		dev_err(afe->dev, "IRQ for memif %d is not described\n",
			rtd->cpu_dai->id);
		return -ENXIO;
	}

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
		if (enable_shift >= 0)
			regmap_update_bits(afe->regmap, AFE_DAC_CON0,
					   1 << enable_shift, 0xffffffff);

		/* set irq counter */
		regmap_update_bits(afe->regmap,
				   memif->irqdata->reg_cnt,
				   0x3ffff << memif->irqdata->cnt_shift,
				   counter << memif->irqdata->cnt_shift);

		/* set irq fs */
		if (memif->irqdata->fs_shift >= 0)
			regmap_update_bits(afe->regmap,
					   AFE_IRQ_MCU_CON,
					   0xf << memif->irqdata->fs_shift,
					   mtk_afe_i2s_fs(runtime->rate) <<
					       memif->irqdata->fs_shift);
		/* enable interrupt */
		regmap_update_bits(afe->regmap, AFE_IRQ_MCU_CON,
				   1 << memif->irqdata->en_shift,
				   1 << memif->irqdata->en_shift);

		if (io->data->start) {
			ret = io->data->start(afe, substream);
			if (ret)
				return ret;
		}
		return 0;
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
		if (io->data->pause)
			io->data->pause(afe, substream);

		if (enable_shift >= 0)
			regmap_update_bits(afe->regmap, AFE_DAC_CON0,
					   1 << enable_shift, 0);
		/* disable interrupt */
		regmap_update_bits(afe->regmap, AFE_IRQ_MCU_CON,
				   1 << memif->irqdata->en_shift,
				   0 << memif->irqdata->en_shift);
		/* and clear pending IRQ */
		regmap_write(afe->regmap, AFE_IRQ_CLR,
			     1 << memif->irqdata->clr_shift);
		memif->hw_ptr = 0;
		return 0;
	default:
		return -EINVAL;
	}
}

static const struct snd_soc_dai_ops mtk_afe_dai_ops = {
	.startup	= mtk_afe_dais_startup,
	.shutdown	= mtk_afe_dais_shutdown,
	.hw_params	= mtk_afe_dais_hw_params,
	.prepare	= mtk_afe_dais_prepare,
	.trigger	= mtk_afe_dais_trigger,
};

static const struct mtk_afe_io_data mtk_afe_io_data[MTK_AFE_IO_NUM] = {
	[MTK_AFE_IO_PMIC] = {
		.num = MTK_AFE_IO_PMIC,
		.name = "pmic",
		.shutdown = mtk_afe_pmic_shutdown,
		.prepare = mtk_afe_pmic_prepare,
	},
	[MTK_AFE_IO_I2S] = {
		.num = MTK_AFE_IO_I2S,
		.name = "i2s",
		.shutdown = mtk_afe_i2s_shutdown,
		.prepare = mtk_afe_i2s_prepare,
	},
	[MTK_AFE_IO_2ND_I2S] = {
		.num = MTK_AFE_IO_2ND_I2S,
		.name = "i2s2",
		.shutdown = mtk_afe_i2s2_shutdown,
		.prepare = mtk_afe_i2s2_prepare,
	},
	[MTK_AFE_IO_HDMI] = {
		.num = MTK_AFE_IO_HDMI,
		.name = "hdmi",
		.prepare = mtk_afe_hdmi_prepare,
		.start = mtk_afe_hdmi_start,
		.pause = mtk_afe_hdmi_pause,
	},
};

static struct snd_soc_dai_driver mtk_afe_pcm_dais[] = {
	{
		.name  = "I2S",
		.id  = MTK_AFE_IO_I2S,
		.playback = {
			.stream_name = "I2S Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_8000_48000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
		},
		.capture = {
			.stream_name = "I2S Capture",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_8000_48000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
		},
		.ops = &mtk_afe_dai_ops,
		.symmetric_rates = 1,
	}, {
		.name = "2nd I2S",
		.id  = MTK_AFE_IO_2ND_I2S,
		.playback = {
			.stream_name = "2nd I2S Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_8000_48000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
		},
		.capture = {
			.stream_name = "2nd I2S Capture",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_8000_48000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
		},
		.ops = &mtk_afe_dai_ops,
		.symmetric_rates = 1,
	}, {
		.name = "HDMI",
		.id  = MTK_AFE_IO_HDMI,
		.playback = {
			.stream_name = "HDMI Playback",
			.channels_min = 2,
			.channels_max = 8,
			.rates = SNDRV_PCM_RATE_32000 | SNDRV_PCM_RATE_44100 |
				SNDRV_PCM_RATE_48000 | SNDRV_PCM_RATE_88200 |
				SNDRV_PCM_RATE_96000 | SNDRV_PCM_RATE_176400 |
				SNDRV_PCM_RATE_192000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
		},
		.ops = &mtk_afe_dai_ops,
	}, {
		.name = "PMIC",
		.id  = MTK_AFE_IO_PMIC,
		.playback = {
			.stream_name = "PMIC Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_8000_48000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
		},
		.capture = {
			.stream_name = "PMIC Capture",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_8000_48000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
		},
		.ops = &mtk_afe_dai_ops,
	},
};

static const struct snd_soc_component_driver mtk_afe_pcm_dai_component = {
	.name		= "mtk-afe-pcm-dai",
};

static const char *aud_clks[MTK_CLK_NUM] = {
	[MTK_CLK_INFRASYS_AUD] = "infra_sys_audio_clk",
	[MTK_CLK_TOP_PDN_AUD] = "top_pdn_audio",
	[MTK_CLK_TOP_PDN_AUD_BUS] = "top_pdn_aud_intbus",
	[MTK_CLK_I2S0_M] =  "i2s0_m",
	[MTK_CLK_I2S1_M] =  "i2s1_m",
	[MTK_CLK_I2S2_M] =  "i2s2_m",
	[MTK_CLK_I2S3_M] =  "i2s3_m",
	[MTK_CLK_I2S3_B] =  "i2s3_b",
	[MTK_CLK_BCK0] =  "bck0",
	[MTK_CLK_BCK1] =  "bck1",
};

static const struct mtk_afe_irq_data irq_data[MTK_AFE_IRQ_NUM] = {
	[MTK_AFE_IRQ_1] = {
		.reg_cnt = AFE_IRQ_CNT1,
		.cnt_shift = 0,
		.en_shift = 0,
		.fs_shift = 4,
		.clr_shift = 0,
	},
	[MTK_AFE_IRQ_2] = {
		.reg_cnt = AFE_IRQ_CNT2,
		.cnt_shift = 0,
		.en_shift = 1,
		.fs_shift = 8,
		.clr_shift = 1,
	},
	[MTK_AFE_IRQ_3] = {
		.reg_cnt = AFE_IRQ_CNT1,
		.cnt_shift = 20,
		.en_shift = 2,
		.fs_shift = 16,
		.clr_shift = 2,
	},
	[MTK_AFE_IRQ_4] = {
		.reg_cnt = AFE_IRQ_CNT2,
		.cnt_shift = 20,
		.en_shift = 3,
		.fs_shift = 20,
		.clr_shift = 3,
	},
	[MTK_AFE_IRQ_5] = {
		.reg_cnt = AFE_IRQ_CNT5,
		.cnt_shift = 0,
		.en_shift = 12,
		.fs_shift = -1,
		.clr_shift = 4,
	},
	[MTK_AFE_IRQ_7] = {
		.reg_cnt = AFE_IRQ_CNT7,
		.cnt_shift = 0,
		.en_shift = 14,
		.fs_shift = 24,
		.clr_shift = 6,
	},
};

static const struct mtk_afe_memif_data memif_data[MTK_AFE_MEMIF_NUM] = {
	{
		.name = "DL1",
		.id = MTK_AFE_MEMIF_DL1,
		.reg_ofs_base = AFE_DL1_BASE,
		.reg_ofs_cur = AFE_DL1_CUR,
		.fs_shift = 0,
		.mono_shift = 21,
		.enable_shift = 1,
	}, {
		.name = "DL2",
		.id = MTK_AFE_MEMIF_DL2,
		.reg_ofs_base = AFE_DL2_BASE,
		.reg_ofs_cur = AFE_DL2_CUR,
		.fs_shift = 4,
		.mono_shift = 22,
		.enable_shift = 2,
	}, {
		.name = "VUL",
		.id = MTK_AFE_MEMIF_VUL,
		.reg_ofs_base = AFE_VUL_BASE,
		.reg_ofs_cur = AFE_VUL_CUR,
		.fs_shift = 16,
		.mono_shift = 27,
		.enable_shift = 3,
	}, {
		.name = "DAI",
		.id = MTK_AFE_MEMIF_DAI,
		.reg_ofs_base = AFE_DAI_BASE,
		.reg_ofs_cur = AFE_DAI_CUR,
		.fs_shift = 24,
		.mono_shift = -1,
		.enable_shift = 4,
	}, {
		.name = "AWB",
		.id = MTK_AFE_MEMIF_AWB,
		.reg_ofs_base = AFE_AWB_BASE,
		.reg_ofs_cur = AFE_AWB_CUR,
		.fs_shift = 12,
		.mono_shift = 24,
		.enable_shift = 6,
	}, {
		.name = "MOD_DAI",
		.id = MTK_AFE_MEMIF_MOD_DAI,
		.reg_ofs_base = AFE_MOD_PCM_BASE,
		.reg_ofs_cur = AFE_MOD_PCM_CUR,
		.fs_shift = 30,
		.mono_shift = 30,
		.enable_shift = 7,
	}, {
		.name = "HDMI",
		.id = MTK_AFE_MEMIF_HDMI,
		.reg_ofs_base = AFE_HDMI_OUT_BASE,
		.reg_ofs_cur = AFE_HDMI_OUT_CUR,
		.fs_shift = -1,
		.mono_shift = -1,
		.enable_shift = -1,
	},
};

static const struct regmap_config mtk_afe_regmap_config = {
	.reg_bits = 32,
	.reg_stride = 4,
	.val_bits = 32,
	.max_register = AFE_ADDA2_TOP_CON0,
	.cache_type = REGCACHE_NONE,
};

static irqreturn_t mtk_afe_irq_handler(int irq, void *dev_id)
{
	struct mtk_afe *afe = dev_id;
	unsigned int reg_value, hw_ptr;
	int i, ret;

	ret = regmap_read(afe->regmap, AFE_IRQ_STATUS, &reg_value);
	if (ret) {
		dev_err(afe->dev, "%s irq status err\n", __func__);
		reg_value = AFE_IRQ_STATUS_BITS;
		goto err_irq;
	}

	for (i = 0; i < MTK_AFE_MEMIF_NUM; i++) {
		struct mtk_afe_memif *memif = &afe->memif[i];

		if (!memif->irqdata)
			continue;
		if (!(reg_value & (1 << memif->irqdata->clr_shift)))
			continue;

		ret = regmap_read(afe->regmap, memif->data->reg_ofs_cur,
				  &hw_ptr);
		if (ret || hw_ptr == 0) {
			dev_err(afe->dev, "%s hw_ptr err\n", __func__);
			hw_ptr = memif->phys_buf_addr;
		}
		memif->hw_ptr = hw_ptr - memif->phys_buf_addr;
		snd_pcm_period_elapsed(memif->substream);
	}

err_irq:
	/* clear irq */
	regmap_write(afe->regmap, AFE_IRQ_CLR, reg_value & AFE_IRQ_STATUS_BITS);

	return IRQ_HANDLED;
}

static int mtk_afe_runtime_suspend(struct device *dev)
{
	struct mtk_afe *afe = dev_get_drvdata(dev);

	regmap_update_bits(afe->regmap, AUDIO_TOP_CON0, 1 << 2, 1 << 2);

	clk_disable_unprepare(afe->clocks[MTK_CLK_BCK0]);
	clk_disable_unprepare(afe->clocks[MTK_CLK_BCK1]);
	clk_disable_unprepare(afe->clocks[MTK_CLK_TOP_PDN_AUD]);
	clk_disable_unprepare(afe->clocks[MTK_CLK_TOP_PDN_AUD_BUS]);
	clk_disable_unprepare(afe->clocks[MTK_CLK_INFRASYS_AUD]);
	return 0;
}

static int mtk_afe_runtime_resume(struct device *dev)
{
	struct mtk_afe *afe = dev_get_drvdata(dev);
	int ret;

	ret = clk_prepare_enable(afe->clocks[MTK_CLK_INFRASYS_AUD]);
	if (ret)
		return ret;

	ret = clk_prepare_enable(afe->clocks[MTK_CLK_TOP_PDN_AUD_BUS]);
	if (ret)
		goto err_infra;

	ret = clk_prepare_enable(afe->clocks[MTK_CLK_TOP_PDN_AUD]);
	if (ret)
		goto err_top_aud_bus;

	ret = clk_prepare_enable(afe->clocks[MTK_CLK_BCK0]);
	if (ret)
		goto err_top_aud;
	ret = clk_prepare_enable(afe->clocks[MTK_CLK_BCK1]);
	if (ret)
		goto err_bck0;

	/* enable AFE clk */
	regmap_update_bits(afe->regmap, AUDIO_TOP_CON0, 1 << 2, 0);

	/* set O3/O4 16bits */
	regmap_update_bits(afe->regmap, AFE_CONN_24BIT, (1 << 3) | (1 << 4), 0);

	/* unmask all IRQs */
	regmap_update_bits(afe->regmap, AFE_IRQ_MCU_EN, 0xff, 0xff);
	return 0;

err_bck0:
	clk_disable_unprepare(afe->clocks[MTK_CLK_BCK0]);
err_top_aud:
	clk_disable_unprepare(afe->clocks[MTK_CLK_TOP_PDN_AUD]);
err_top_aud_bus:
	clk_disable_unprepare(afe->clocks[MTK_CLK_TOP_PDN_AUD_BUS]);
err_infra:
	clk_disable_unprepare(afe->clocks[MTK_CLK_INFRASYS_AUD]);
	return ret;
}

static int mtk_afe_init_audio_clk(struct mtk_afe *afe)
{
	size_t i;

	for (i = 0; i < ARRAY_SIZE(aud_clks); i++) {
		afe->clocks[i] = devm_clk_get(afe->dev, aud_clks[i]);
		if (IS_ERR(afe->clocks[i])) {
			dev_err(afe->dev, "%s devm_clk_get %s fail\n",
				__func__, aud_clks[i]);
			return PTR_ERR(afe->clocks[i]);
		}
	}
	afe->ios[MTK_AFE_IO_2ND_I2S].m_ck = afe->clocks[MTK_CLK_I2S0_M];
	afe->ios[MTK_AFE_IO_I2S].m_ck = afe->clocks[MTK_CLK_I2S1_M];
	afe->ios[MTK_AFE_IO_HDMI].m_ck = afe->clocks[MTK_CLK_I2S3_M];
	afe->ios[MTK_AFE_IO_HDMI].b_ck = afe->clocks[MTK_CLK_I2S3_B];
	afe->ios[MTK_AFE_IO_2ND_I2S].mck_mul = 256;
	afe->ios[MTK_AFE_IO_I2S].mck_mul = 256;
	afe->ios[MTK_AFE_IO_HDMI].mck_mul = 128;

	clk_set_rate(afe->clocks[MTK_CLK_BCK0], 22579200);
	clk_set_rate(afe->clocks[MTK_CLK_BCK1], 24576000);
	return 0;
}

static int mtk_afe_parse_memif(struct mtk_afe *afe, struct device_node *np,
			       const char *propname, int *mem)
{
	struct mtk_afe_memif *memif;
	int ret;
	u32 val;
	static bool use_sram;
	*mem = -1;
	ret = of_property_read_u32_index(np, propname, 0, &val);
	if (ret)
		/* to be check later since we may have only play or capture */
		return 0;
	if (val > MTK_AFE_MEMIF_NUM) {
		dev_err(afe->dev, "invalid memif %d\n", val);
		return -EINVAL;
	}
	*mem = val;
	memif = &afe->memif[val];

	ret = of_property_read_u32_index(np, propname, 1, &val);
	if (ret)
		return ret;
	if (val >= MTK_AFE_IRQ_NUM) {
		dev_err(afe->dev, "invalid mem irq %d\n", val);
		return -EINVAL;
	}
	memif->irqdata = &irq_data[val];

	ret = of_property_read_u32_index(np, propname, 2, &val);
	if (ret)
		return ret;
	if (val && use_sram)
		return -EINVAL; /* only one memif can use SRAM */
	memif->use_sram = val ? true : false;
	use_sram = memif->use_sram;
	return 0;
}

static int mtk_afe_init_subnode(struct mtk_afe *afe, struct device_node *np)
{
	struct mtk_afe_io *io;
	u32 val;
	int ret, n, i;

	ret = of_property_read_u32(np, "io", &val);
	if (ret)
		return ret;
	if (val >= MTK_AFE_IO_NUM) {
		dev_err(afe->dev, "invalid io %d\n", val);
		return -EINVAL;
	}
	io = &afe->ios[val];

	n = of_property_count_u32_elems(np, "connections");
	if (n <= 0)
		return n;
	io->connections = devm_kzalloc(afe->dev, n * sizeof(u32),
			GFP_KERNEL);
	if (!io->connections)
		return -ENOMEM;
	io->num_connections = n / 2;
	ret = of_property_read_u32_array(np, "connections", io->connections,
					 n);
	if (ret)
		return ret;
	for (i = 0; i < io->num_connections; i++) {
		dev_dbg(afe->dev, "connection %d: %d -> %d\n", i,
			io->connections[i * 2],
			io->connections[i * 2 + 1]);
	}

	ret = mtk_afe_parse_memif(afe, np, "mem-interface-playback", io->mem);
	if (ret)
		return ret;
	ret = mtk_afe_parse_memif(afe, np, "mem-interface-capture",
				  io->mem + 1);
	if (ret)
		return ret;
	if (io->mem[0] < 0 && io->mem[1] < 0) {
		dev_err(afe->dev, "no mem interface given\n");
		return -EINVAL;
	}
	return 0;
}

static int mtk_afe_pcm_dev_probe(struct platform_device *pdev)
{
	int ret, i;
	unsigned int irq_id;
	struct mtk_afe *afe;
	struct device_node *dainode, *np;
	struct resource *res;

	afe = devm_kzalloc(&pdev->dev, sizeof(*afe), GFP_KERNEL);
	if (!afe)
		return -ENOMEM;

	afe->dev = &pdev->dev;

	irq_id = platform_get_irq(pdev, 0);
	if (!irq_id) {
		dev_err(afe->dev, "np %s no irq\n", afe->dev->of_node->name);
		return -ENXIO;
	}
	ret = devm_request_irq(afe->dev, irq_id, mtk_afe_irq_handler,
			       0, "Afe_ISR_Handle", (void *)afe);
	if (ret) {
		dev_err(afe->dev, "could not request_irq\n");
		return ret;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	afe->base_addr = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(afe->base_addr))
		return PTR_ERR(afe->base_addr);

	afe->regmap = devm_regmap_init_mmio(&pdev->dev, afe->base_addr,
		&mtk_afe_regmap_config);
	if (IS_ERR(afe->regmap))
		return PTR_ERR(afe->regmap);

	/* get SRAM base */
	res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	afe->sram_address = devm_ioremap_resource(&pdev->dev, res);
	if (!IS_ERR(afe->sram_address)) {
		afe->sram_phy_address = res->start;
		afe->sram_size = resource_size(res);
	}

	/* FIXME SPDIF clk move to CCF */
	np = of_find_compatible_node(NULL, NULL, "mediatek,mt8173-topckgen");
	afe->topckgen_base_addr = of_iomap(np, 0);
	if (!afe->topckgen_base_addr) {
		dev_err(afe->dev, "cannot get topckgen_base_addr\n");
		return -EADDRNOTAVAIL;
	}

	/* FIXME APLL tuner move to CCF */
	np = of_find_compatible_node(NULL, NULL, "mediatek,mt8173-apmixedsys");
	afe->apmixedsys_base_addr = of_iomap(np, 0);
	if (!afe->apmixedsys_base_addr) {
		dev_err(afe->dev, "cannot get apmixedsys_base_addr\n");
		return -EADDRNOTAVAIL;
	}

	/* initial audio related clock */
	ret = mtk_afe_init_audio_clk(afe);
	if (ret) {
		dev_err(afe->dev, "mtk_afe_init_audio_clk fail\n");
		return ret;
	}

	for (i = 0; i < MTK_AFE_MEMIF_NUM; i++)
		afe->memif[i].data = &memif_data[i];

	for (i = 0; i < MTK_AFE_IO_NUM; i++)
		afe->ios[i].data = &mtk_afe_io_data[i];

	for_each_child_of_node(pdev->dev.of_node, dainode) {
		ret = mtk_afe_init_subnode(afe, dainode);
		if (ret)
			return ret;
	}

	platform_set_drvdata(pdev, afe);

	pm_runtime_enable(&pdev->dev);
	if (!pm_runtime_enabled(&pdev->dev)) {
		ret = mtk_afe_runtime_resume(&pdev->dev);
		if (ret)
			goto err_pm_disable;
	}

	ret = snd_soc_register_platform(&pdev->dev, &mtk_afe_pcm_platform);
	if (ret)
		goto err_pm_disable;

	ret = snd_soc_register_component(&pdev->dev,
					 &mtk_afe_pcm_dai_component,
					 mtk_afe_pcm_dais,
					 ARRAY_SIZE(mtk_afe_pcm_dais));
	if (ret)
		goto err_platform;

	dev_info(&pdev->dev, "MTK AFE driver initialized.\n");
	return 0;

err_platform:
	snd_soc_unregister_platform(&pdev->dev);
err_pm_disable:
	pm_runtime_disable(&pdev->dev);
	return ret;
}

static int mtk_afe_pcm_dev_remove(struct platform_device *pdev)
{
	pm_runtime_disable(&pdev->dev);
	snd_soc_unregister_component(&pdev->dev);
	snd_soc_unregister_platform(&pdev->dev);
	return 0;
}

static const struct of_device_id mtk_afe_pcm_dt_match[] = {
	{ .compatible = "mediatek,mt8173-afe-pcm", },
	{ }
};

static const struct dev_pm_ops mtk_afe_pm_ops = {
	SET_RUNTIME_PM_OPS(mtk_afe_runtime_suspend, mtk_afe_runtime_resume,
			   NULL)
};

static struct platform_driver mtk_afe_pcm_driver = {
	.driver = {
		   .name = "mtk-afe-pcm",
		   .owner = THIS_MODULE,
		   .of_match_table = mtk_afe_pcm_dt_match,
		   .pm = &mtk_afe_pm_ops,
	},
	.probe = mtk_afe_pcm_dev_probe,
	.remove = mtk_afe_pcm_dev_remove,
};

module_platform_driver(mtk_afe_pcm_driver);

MODULE_DESCRIPTION("Mediatek ALSA SoC Afe platform driver");
MODULE_AUTHOR("Koro Chen <koro.chen@mediatek.com>");
MODULE_LICENSE("GPL v2");
MODULE_DEVICE_TABLE(of, mtk_afe_pcm_dt_match);
