/*
 * mt8135_afe_pcm.c  --  Mediatek ALSA SoC Afe platform driver
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

#include "mt_afe_common.h"
#include "mt_afe_clk.h"
#include "mt_afe_control.h"
#include "mt_afe_digital_type.h"
#include <linux/dma-mapping.h>
#include <linux/module.h>
#include <linux/spinlock.h>
#include <sound/soc.h>
#include <linux/debugfs.h>

/*upstream todo: separate id and pin*/
#define AFE_DL1_ID		MT_AFE_PIN_DL1
#define AFE_AWB_ID		MT_AFE_PIN_AWB
#define AFE_VUL_ID		MT_AFE_PIN_VUL
#define AFE_HDMI_ID		MT_AFE_PIN_HDMI
#define AFE_ROUTING_ID		MT_AFE_PIN_NUM

#define UL_RATE		(SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000 | \
			 SNDRV_PCM_RATE_32000 | SNDRV_PCM_RATE_48000)
#define HDMI_RATES	(SNDRV_PCM_RATE_32000 | SNDRV_PCM_RATE_44100 | \
			 SNDRV_PCM_RATE_48000 | SNDRV_PCM_RATE_88200 | \
			 SNDRV_PCM_RATE_96000 | SNDRV_PCM_RATE_176400 | \
			 SNDRV_PCM_RATE_192000)

static const struct snd_pcm_hardware mt8135_d1_hardware = {
	.info = (SNDRV_PCM_INFO_MMAP | SNDRV_PCM_INFO_INTERLEAVED |
		 SNDRV_PCM_INFO_RESUME | SNDRV_PCM_INFO_MMAP_VALID),
	.buffer_bytes_max = 16*1024,
	.period_bytes_max = 16*1024,
	.periods_min = 1,
	.periods_max = 4096,
	.fifo_size = 0,
};

static const struct snd_pcm_hardware mt8135_ul_hardware = {
	.info = (SNDRV_PCM_INFO_INTERLEAVED | SNDRV_PCM_INFO_MMAP |
		 SNDRV_PCM_INFO_MMAP_VALID),
	.buffer_bytes_max = 16*1024,
	.period_bytes_max = 16*1024,
	.periods_min = 1,
	.periods_max = 4096,
	.fifo_size = 0,
};

static const struct snd_pcm_hardware mt8135_hdmi_hardware = {
	.info = (SNDRV_PCM_INFO_MMAP | SNDRV_PCM_INFO_INTERLEAVED |
		 SNDRV_PCM_INFO_RESUME | SNDRV_PCM_INFO_MMAP_VALID),
	.buffer_bytes_max = 512*1024,
	.period_bytes_max = 512*1024,
	.periods_min = 1,
	.periods_max = 1024,
	.fifo_size = 0,
};

static struct dentry *mt8135_afe_debugfs;

static int mt8135_afe_debug_open(struct inode *inode, struct file *file)
{
	file->private_data = inode->i_private;
	return 0;
}

static ssize_t mt8135_afe_debug_read(struct file *file, char __user *buf,
				     size_t count, loff_t *pos)
{
	struct mt_afe_info *afe_info = file->private_data;
	const int size = 4096;
	char buffer[size];
	int n = 0;

	mt_afe_clk_on(afe_info);

	n += scnprintf(buffer + n, size - n, "AUDIO_TOP_CON0  = 0x%x\n",
		       mt_afe_get_reg(afe_info, AUDIO_TOP_CON0));
	n += scnprintf(buffer + n, size - n, "AUDIO_TOP_CON1  = 0x%x\n",
		       mt_afe_get_reg(afe_info, AUDIO_TOP_CON1));
	n += scnprintf(buffer + n, size - n, "AUDIO_TOP_CON3  = 0x%x\n",
		       mt_afe_get_reg(afe_info, AUDIO_TOP_CON3));
	n += scnprintf(buffer + n, size - n, "AFE_DAC_CON0  = 0x%x\n",
		       mt_afe_get_reg(afe_info, AFE_DAC_CON0));
	n += scnprintf(buffer + n, size - n, "AFE_DAC_CON1  = 0x%x\n",
		       mt_afe_get_reg(afe_info, AFE_DAC_CON1));
	n += scnprintf(buffer + n, size - n, "AFE_I2S_CON  = 0x%x\n",
		       mt_afe_get_reg(afe_info, AFE_I2S_CON));
	n += scnprintf(buffer + n, size - n, "AFE_DAIBT_CON0  = 0x%x\n",
		       mt_afe_get_reg(afe_info, AFE_DAIBT_CON0));
	n += scnprintf(buffer + n, size - n, "AFE_CONN0  = 0x%x\n",
		       mt_afe_get_reg(afe_info, AFE_CONN0));
	n += scnprintf(buffer + n, size - n, "AFE_CONN1  = 0x%x\n",
		       mt_afe_get_reg(afe_info, AFE_CONN1));
	n += scnprintf(buffer + n, size - n, "AFE_CONN2  = 0x%x\n",
		       mt_afe_get_reg(afe_info, AFE_CONN2));
	n += scnprintf(buffer + n, size - n, "AFE_CONN3  = 0x%x\n",
		       mt_afe_get_reg(afe_info, AFE_CONN3));
	n += scnprintf(buffer + n, size - n, "AFE_CONN4  = 0x%x\n",
		       mt_afe_get_reg(afe_info, AFE_CONN4));
	n += scnprintf(buffer + n, size - n, "AFE_I2S_CON1  = 0x%x\n",
		       mt_afe_get_reg(afe_info, AFE_I2S_CON1));
	n += scnprintf(buffer + n, size - n, "AFE_I2S_CON2  = 0x%x\n",
		       mt_afe_get_reg(afe_info, AFE_I2S_CON2));
	n += scnprintf(buffer + n, size - n, "AFE_MRGIF_CON  = 0x%x\n",
		       mt_afe_get_reg(afe_info, AFE_MRGIF_CON));
	n += scnprintf(buffer + n, size - n, "AFE_DL1_BASE  = 0x%x\n",
		       mt_afe_get_reg(afe_info, AFE_DL1_BASE));
	n += scnprintf(buffer + n, size - n, "AFE_DL1_CUR  = 0x%x\n",
		       mt_afe_get_reg(afe_info, AFE_DL1_CUR));
	n += scnprintf(buffer + n, size - n, "AFE_DL1_END  = 0x%x\n",
		       mt_afe_get_reg(afe_info, AFE_DL1_END));
	n += scnprintf(buffer + n, size - n, "AFE_DL2_BASE  = 0x%x\n",
		       mt_afe_get_reg(afe_info, AFE_DL2_BASE));
	n += scnprintf(buffer + n, size - n, "AFE_DL2_CUR  = 0x%x\n",
		       mt_afe_get_reg(afe_info, AFE_DL2_CUR));
	n += scnprintf(buffer + n, size - n, "AFE_DL2_END  = 0x%x\n",
		       mt_afe_get_reg(afe_info, AFE_DL2_END));
	n += scnprintf(buffer + n, size - n, "AFE_AWB_BASE  = 0x%x\n",
		       mt_afe_get_reg(afe_info, AFE_AWB_BASE));
	n += scnprintf(buffer + n, size - n, "AFE_AWB_END  = 0x%x\n",
		       mt_afe_get_reg(afe_info, AFE_AWB_END));
	n += scnprintf(buffer + n, size - n, "AFE_AWB_CUR  = 0x%x\n",
		       mt_afe_get_reg(afe_info, AFE_AWB_CUR));
	n += scnprintf(buffer + n, size - n, "AFE_VUL_BASE  = 0x%x\n",
		       mt_afe_get_reg(afe_info, AFE_VUL_BASE));
	n += scnprintf(buffer + n, size - n, "AFE_VUL_END  = 0x%x\n",
		       mt_afe_get_reg(afe_info, AFE_VUL_END));
	n += scnprintf(buffer + n, size - n, "AFE_VUL_CUR  = 0x%x\n",
		       mt_afe_get_reg(afe_info, AFE_VUL_CUR));
	n += scnprintf(buffer + n, size - n, "AFE_DAI_BASE  = 0x%x\n",
		       mt_afe_get_reg(afe_info, AFE_DAI_BASE));
	n += scnprintf(buffer + n, size - n, "AFE_DAI_END  = 0x%x\n",
		       mt_afe_get_reg(afe_info, AFE_DAI_END));
	n += scnprintf(buffer + n, size - n, "AFE_DAI_CUR  = 0x%x\n",
		       mt_afe_get_reg(afe_info, AFE_DAI_CUR));
	n += scnprintf(buffer + n, size - n, "AFE_IRQ_CON= 0x%x\n",
		       mt_afe_get_reg(afe_info, AFE_IRQ_CON));
	n += scnprintf(buffer + n, size - n, "MEMIF_MON0 = 0x%x\n",
		       mt_afe_get_reg(afe_info, AFE_MEMIF_MON0));
	n += scnprintf(buffer + n, size - n, "MEMIF_MON1 = 0x%x\n",
		       mt_afe_get_reg(afe_info, AFE_MEMIF_MON1));
	n += scnprintf(buffer + n, size - n, "MEMIF_MON2 = 0x%x\n",
		       mt_afe_get_reg(afe_info, AFE_MEMIF_MON2));
	n += scnprintf(buffer + n, size - n, "MEMIF_MON3 = 0x%x\n",
		       mt_afe_get_reg(afe_info, AFE_MEMIF_MON3));
	n += scnprintf(buffer + n, size - n, "MEMIF_MON4 = 0x%x\n",
		       mt_afe_get_reg(afe_info, AFE_MEMIF_MON4));
	n += scnprintf(buffer + n, size - n, "AFE_ADDA_DL_SRC2_CON0  = 0x%x\n",
		       mt_afe_get_reg(afe_info, AFE_ADDA_DL_SRC2_CON0));
	n += scnprintf(buffer + n, size - n, "AFE_ADDA_DL_SRC2_CON1  = 0x%x\n",
		       mt_afe_get_reg(afe_info, AFE_ADDA_DL_SRC2_CON1));
	n += scnprintf(buffer + n, size - n, "AFE_ADDA_UL_SRC_CON0 = 0x%x\n",
		       mt_afe_get_reg(afe_info, AFE_ADDA_UL_SRC_CON0));
	n += scnprintf(buffer + n, size - n, "AFE_ADDA_UL_SRC_CON1 = 0x%x\n",
		       mt_afe_get_reg(afe_info, AFE_ADDA_UL_SRC_CON1));
	n += scnprintf(buffer + n, size - n, "AFE_ADDA_TOP_CON0 = 0x%x\n",
		       mt_afe_get_reg(afe_info, AFE_ADDA_TOP_CON0));
	n += scnprintf(buffer + n, size - n, "AFE_ADDA_UL_DL_CON0 = 0x%x\n",
		       mt_afe_get_reg(afe_info, AFE_ADDA_UL_DL_CON0));
	n += scnprintf(buffer + n, size - n, "AFE_ADDA_SRC_DEBUG = 0x%x\n",
		       mt_afe_get_reg(afe_info, AFE_ADDA_SRC_DEBUG));
	n += scnprintf(buffer + n, size - n, "AFE_ADDA_SRC_DEBUG_MON0 = 0x%x\n",
		       mt_afe_get_reg(afe_info, AFE_ADDA_SRC_DEBUG_MON0));
	n += scnprintf(buffer + n, size - n, "AFE_ADDA_SRC_DEBUG_MON1 = 0x%x\n",
		       mt_afe_get_reg(afe_info, AFE_ADDA_SRC_DEBUG_MON1));
	n += scnprintf(buffer + n, size - n, "AFE_ADDA_NEWIF_CFG0 = 0x%x\n",
		       mt_afe_get_reg(afe_info, AFE_ADDA_NEWIF_CFG0));
	n += scnprintf(buffer + n, size - n, "AFE_ADDA_NEWIF_CFG1 = 0x%x\n",
		       mt_afe_get_reg(afe_info, AFE_ADDA_NEWIF_CFG1));
	n += scnprintf(buffer + n, size - n, "SIDETONE_DEBUG = 0x%x\n",
		       mt_afe_get_reg(afe_info, AFE_SIDETONE_DEBUG));
	n += scnprintf(buffer + n, size - n, "SIDETONE_MON = 0x%x\n",
		       mt_afe_get_reg(afe_info, AFE_SIDETONE_MON));
	n += scnprintf(buffer + n, size - n, "SIDETONE_CON0 = 0x%x\n",
		       mt_afe_get_reg(afe_info, AFE_SIDETONE_CON0));
	n += scnprintf(buffer + n, size - n, "SIDETONE_COEFF = 0x%x\n",
		       mt_afe_get_reg(afe_info, AFE_SIDETONE_COEFF));
	n += scnprintf(buffer + n, size - n, "SIDETONE_CON1 = 0x%x\n",
		       mt_afe_get_reg(afe_info, AFE_SIDETONE_CON1));
	n += scnprintf(buffer + n, size - n, "SIDETONE_GAIN = 0x%x\n",
		       mt_afe_get_reg(afe_info, AFE_SIDETONE_GAIN));
	n += scnprintf(buffer + n, size - n, "SGEN_CON0 = 0x%x\n",
		       mt_afe_get_reg(afe_info, AFE_SGEN_CON0));
	n += scnprintf(buffer + n, size - n, "AFE_MRGIF_MON0 = 0x%x\n",
		       mt_afe_get_reg(afe_info, AFE_MRGIF_MON0));
	n += scnprintf(buffer + n, size - n, "AFE_MRGIF_MON1 = 0x%x\n",
		       mt_afe_get_reg(afe_info, AFE_MRGIF_MON1));
	n += scnprintf(buffer + n, size - n, "AFE_MRGIF_MON2 = 0x%x\n",
		       mt_afe_get_reg(afe_info, AFE_MRGIF_MON2));
	n += scnprintf(buffer + n, size - n, "AFE_TOP_CON0 = 0x%x\n",
		       mt_afe_get_reg(afe_info, AFE_TOP_CON0));

	n += scnprintf(buffer + n, size - n, "AFE_ADDA_PREDIS_CON0 = 0x%x\n",
		       mt_afe_get_reg(afe_info, AFE_ADDA_PREDIS_CON0));
	n += scnprintf(buffer + n, size - n, "AFE_ADDA_PREDIS_CON1 = 0x%x\n",
		       mt_afe_get_reg(afe_info, AFE_ADDA_PREDIS_CON1));

	n += scnprintf(buffer + n, size - n, "HDMI_OUT_CON0   = 0x%x\n",
		       mt_afe_get_reg(afe_info, AFE_HDMI_OUT_CON0));
	n += scnprintf(buffer + n, size - n, "HDMI_OUT_BASE   = 0x%x\n",
		       mt_afe_get_reg(afe_info, AFE_HDMI_OUT_BASE));
	n += scnprintf(buffer + n, size - n, "HDMI_OUT_CUR    = 0x%x\n",
		       mt_afe_get_reg(afe_info, AFE_HDMI_OUT_CUR));
	n += scnprintf(buffer + n, size - n, "HDMI_OUT_END    = 0x%x\n",
		       mt_afe_get_reg(afe_info, AFE_HDMI_OUT_END));
	n += scnprintf(buffer + n, size - n, "SPDIF_OUT_CON0  = 0x%x\n",
		       mt_afe_get_reg(afe_info, AFE_SPDIF_OUT_CON0));
	n += scnprintf(buffer + n, size - n, "SPDIF_BASE      = 0x%x\n",
		       mt_afe_get_reg(afe_info, AFE_SPDIF_BASE));
	n += scnprintf(buffer + n, size - n, "SPDIF_CUR       = 0x%x\n",
		       mt_afe_get_reg(afe_info, AFE_SPDIF_CUR));
	n += scnprintf(buffer + n, size - n, "SPDIF_END       = 0x%x\n",
		       mt_afe_get_reg(afe_info, AFE_SPDIF_END));
	n += scnprintf(buffer + n, size - n, "HDMI_CONN0      = 0x%x\n",
		       mt_afe_get_reg(afe_info, AFE_HDMI_CONN0));
	n += scnprintf(buffer + n, size - n, "8CH_I2S_OUT_CON = 0x%x\n",
		       mt_afe_get_reg(afe_info, AFE_8CH_I2S_OUT_CON));
	n += scnprintf(buffer + n, size - n, "IEC_CFG         = 0x%x\n",
		       mt_afe_get_reg(afe_info, AFE_IEC_CFG));
	n += scnprintf(buffer + n, size - n, "IEC_NSNUM       = 0x%x\n",
		       mt_afe_get_reg(afe_info, AFE_IEC_NSNUM));
	n += scnprintf(buffer + n, size - n, "IEC_BURST_INFO  = 0x%x\n",
		       mt_afe_get_reg(afe_info, AFE_IEC_BURST_INFO));
	n += scnprintf(buffer + n, size - n, "IEC_BURST_LEN   = 0x%x\n",
		       mt_afe_get_reg(afe_info, AFE_IEC_BURST_LEN));
	n += scnprintf(buffer + n, size - n, "IEC_NSADR       = 0x%x\n",
		       mt_afe_get_reg(afe_info, AFE_IEC_NSADR));
	n += scnprintf(buffer + n, size - n, "IEC_CHL_STAT0   = 0x%x\n",
		       mt_afe_get_reg(afe_info, AFE_IEC_CHL_STAT0));
	n += scnprintf(buffer + n, size - n, "IEC_CHL_STAT1   = 0x%x\n",
		       mt_afe_get_reg(afe_info, AFE_IEC_CHL_STAT1));
	n += scnprintf(buffer + n, size - n, "IEC_CHR_STAT0   = 0x%x\n",
		       mt_afe_get_reg(afe_info, AFE_IEC_CHR_STAT0));
	n += scnprintf(buffer + n, size - n, "IEC_CHR_STAT1   = 0x%x\n",
		       mt_afe_get_reg(afe_info, AFE_IEC_CHR_STAT1));
	n += scnprintf(buffer + n, size - n, "AFE_IRQ_MCU_CON = 0x%x\n",
		       mt_afe_get_reg(afe_info, AFE_IRQ_MCU_CON));
	n += scnprintf(buffer + n, size - n, "AFE_IRQ_STATUS = 0x%x\n",
		       mt_afe_get_reg(afe_info, AFE_IRQ_STATUS));
	n += scnprintf(buffer + n, size - n, "AFE_IRQ_CLR = 0x%x\n",
		       mt_afe_get_reg(afe_info, AFE_IRQ_CLR));
	n += scnprintf(buffer + n, size - n, "AFE_IRQ_CNT1 = 0x%x\n",
		       mt_afe_get_reg(afe_info, AFE_IRQ_CNT1));
	n += scnprintf(buffer + n, size - n, "AFE_IRQ_CNT2 = 0x%x\n",
		       mt_afe_get_reg(afe_info, AFE_IRQ_CNT2));
	n += scnprintf(buffer + n, size - n, "AFE_IRQ_MON2 = 0x%x\n",
		       mt_afe_get_reg(afe_info, AFE_IRQ_MON2));
	n += scnprintf(buffer + n, size - n, "AFE_IRQ_CNT5 = 0x%x\n",
		       mt_afe_get_reg(afe_info, AFE_IRQ_CNT5));
	n += scnprintf(buffer + n, size - n, "AFE_IRQ1_CNT_MON = 0x%x\n",
		       mt_afe_get_reg(afe_info, AFE_IRQ1_CNT_MON));
	n += scnprintf(buffer + n, size - n, "AFE_IRQ2_CNT_MON = 0x%x\n",
		       mt_afe_get_reg(afe_info, AFE_IRQ2_CNT_MON));
	n += scnprintf(buffer + n, size - n, "AFE_IRQ1_EN_CNT_MON = 0x%x\n",
		       mt_afe_get_reg(afe_info, AFE_IRQ1_EN_CNT_MON));
	n += scnprintf(buffer + n, size - n, "AFE_IRQ5_EN_CNT_MON = 0x%x\n",
		       mt_afe_get_reg(afe_info, AFE_IRQ5_EN_CNT_MON));

	n += scnprintf(buffer + n, size - n, "AFE_MEMIF_MINLEN  = 0x%x\n",
		       mt_afe_get_reg(afe_info, AFE_MEMIF_MINLEN));
	n += scnprintf(buffer + n, size - n, "AFE_MEMIF_MAXLEN  = 0x%x\n",
		       mt_afe_get_reg(afe_info, AFE_MEMIF_MAXLEN));
	n += scnprintf(buffer + n, size - n, "AFE_MEMIF_PBUF_SIZE  = 0x%x\n",
		       mt_afe_get_reg(afe_info, AFE_MEMIF_PBUF_SIZE));
	n += scnprintf(buffer + n, size - n, "AFE_GAIN1_CON0  = 0x%x\n",
		       mt_afe_get_reg(afe_info, AFE_GAIN1_CON0));
	n += scnprintf(buffer + n, size - n, "AFE_GAIN1_CON1  = 0x%x\n",
		       mt_afe_get_reg(afe_info, AFE_GAIN1_CON1));
	n += scnprintf(buffer + n, size - n, "AFE_GAIN1_CON2  = 0x%x\n",
		       mt_afe_get_reg(afe_info, AFE_GAIN1_CON2));
	n += scnprintf(buffer + n, size - n, "AFE_GAIN1_CON3  = 0x%x\n",
		       mt_afe_get_reg(afe_info, AFE_GAIN1_CON3));
	n += scnprintf(buffer + n, size - n, "AFE_GAIN1_CONN  = 0x%x\n",
		       mt_afe_get_reg(afe_info, AFE_GAIN1_CONN));
	n += scnprintf(buffer + n, size - n, "AFE_GAIN1_CUR  = 0x%x\n",
		       mt_afe_get_reg(afe_info, AFE_GAIN1_CUR));
	n += scnprintf(buffer + n, size - n, "AFE_GAIN2_CON0  = 0x%x\n",
		       mt_afe_get_reg(afe_info, AFE_GAIN2_CON0));
	n += scnprintf(buffer + n, size - n, "AFE_GAIN2_CON1  = 0x%x\n",
		       mt_afe_get_reg(afe_info, AFE_GAIN2_CON1));
	n += scnprintf(buffer + n, size - n, "AFE_GAIN2_CON2  = 0x%x\n",
		       mt_afe_get_reg(afe_info, AFE_GAIN2_CON2));
	n += scnprintf(buffer + n, size - n, "AFE_GAIN2_CON3  = 0x%x\n",
		       mt_afe_get_reg(afe_info, AFE_GAIN2_CON3));
	n += scnprintf(buffer + n, size - n, "AFE_GAIN2_CONN  = 0x%x\n",
		       mt_afe_get_reg(afe_info, AFE_GAIN2_CONN));
	n += scnprintf(buffer + n, size - n, "AFE_GAIN2_CONN2  = 0x%x\n",
		       mt_afe_get_reg(afe_info, AFE_GAIN2_CONN2));
	n += scnprintf(buffer + n, size - n, "AFE_GAIN2_CUR  = 0x%x\n",
		       mt_afe_get_reg(afe_info, AFE_GAIN2_CUR));
	mt_afe_clk_off(afe_info);

	return simple_read_from_buffer(buf, count, pos, buffer, n);
}

static const struct file_operations mt8135_afe_debug_ops = {
	.open = mt8135_afe_debug_open,
	.read = mt8135_afe_debug_read,
};

static void mt8135_afe_hdmi_set_connection(struct mt_afe_info *afe_info,
					   unsigned int conn_state,
					   unsigned int channels)
{
	/* O20~O27: L/R/LS/RS/C/LFE/CH7/CH8 */
	switch (channels) {
	case 8:
		mt_afe_set_connection(afe_info, conn_state,
				      MT_AFE_INTERCONN_I26,
				      MT_AFE_INTERCONN_O26);
		mt_afe_set_connection(afe_info, conn_state,
				      MT_AFE_INTERCONN_I27,
				      MT_AFE_INTERCONN_O27);
		/*fall through*/
	case 6:
		mt_afe_set_connection(afe_info, conn_state,
				      MT_AFE_INTERCONN_I24,
				      MT_AFE_INTERCONN_O22);
		mt_afe_set_connection(afe_info, conn_state,
				      MT_AFE_INTERCONN_I25,
				      MT_AFE_INTERCONN_O23);
		/*fall through*/
	case 4:
		mt_afe_set_connection(afe_info, conn_state,
				      MT_AFE_INTERCONN_I22,
				      MT_AFE_INTERCONN_O24);
		mt_afe_set_connection(afe_info, conn_state,
				      MT_AFE_INTERCONN_I23,
				      MT_AFE_INTERCONN_O25);
		/*fall through*/
	case 2:
		mt_afe_set_connection(afe_info, conn_state,
				      MT_AFE_INTERCONN_I20,
				      MT_AFE_INTERCONN_O20);
		mt_afe_set_connection(afe_info, conn_state,
				      MT_AFE_INTERCONN_I21,
				      MT_AFE_INTERCONN_O21);
		break;
	case 1:
		mt_afe_set_connection(afe_info, conn_state,
				      MT_AFE_INTERCONN_I20,
				      MT_AFE_INTERCONN_O20);
		break;
	default:
		dev_warn(afe_info->dev,
			 "%s unsupported channels %u\n", __func__, channels);
		break;
	}
}

static int mt8135_afe_set_ul_buf(struct mt_afe_info *afe_info,
				 struct snd_pcm_substream *substream,
				 struct snd_pcm_hw_params *hw_params,
				  int id)
{
	struct snd_pcm_runtime * const runtime = substream->runtime;
	struct mt_afe_block * const block =
		&afe_info->memif[id].block;

	afe_info->memif[id].memif_num = id;

	block->phys_buf_addr = runtime->dma_addr;
	block->virt_buf_addr = runtime->dma_area;
	block->buffer_size = runtime->dma_bytes;
	block->sample_num_mask = 0x001f;	/* 32 byte align */
	block->write_idx = 0;
	block->dma_read_idx = 0;
	block->data_remained = 0;
	block->fsync_flag = false;
	block->reset_flag = true;
	dev_dbg(afe_info->dev,
		"%s dma_bytes = %d dma_area = %p dma_addr = 0x%x\n",
		__func__, block->buffer_size, block->virt_buf_addr,
		block->phys_buf_addr);
	/* set sram address top hardware */
	switch (id) {
	case AFE_VUL_ID:
		mt_afe_set_reg(afe_info, AFE_VUL_BASE,
			       block->phys_buf_addr, 0xFFFFFFFF);
		mt_afe_set_reg(afe_info, AFE_VUL_END,
			       block->phys_buf_addr + (block->buffer_size - 1),
			       0xFFFFFFFF);
		break;
	case AFE_AWB_ID:
		mt_afe_set_reg(afe_info, AFE_AWB_BASE,
			       block->phys_buf_addr, 0xFFFFFFFF);
		mt_afe_set_reg(afe_info, AFE_AWB_END,
			       block->phys_buf_addr + (block->buffer_size - 1),
			       0xFFFFFFFF);
		break;
	default:
		dev_err(afe_info->dev, "error: invalid DAI ID %d\n", id);
		return -EINVAL;
	};
	return 0;
}

static int mt8135_afe_pcm_ul_stop(struct mt_afe_info *afe_info,
				  struct snd_pcm_substream *substream,
				  int id)
{
	struct mt_afe_block *vul_block =
		&(afe_info->memif[id].block);

	dev_dbg(afe_info->dev, "%s\n", __func__);

	switch (id) {
	case AFE_VUL_ID:
		mt_afe_set_i2s_adc_enable(afe_info, false);
		break;
	case AFE_AWB_ID:
		mt_afe_set_2nd_i2s_enable(afe_info, MT_AFE_I2S_DIR_IN, false);
		break;
	default:
		dev_err(afe_info->dev, "error: invalid DAI ID %d\n", id);
		return -EINVAL;
	};

	mt_afe_set_memory_path_enable(afe_info, id, false);

	/* here to set interrupt */
	mt_afe_set_irq_enable(afe_info, MT_AFE_IRQ_MODE_2, false);

	/* here to turn off digital part */
	switch (id) {
	case AFE_VUL_ID:
		mt_afe_set_connection(afe_info, MT_AFE_DISCONN,
				      MT_AFE_INTERCONN_I03,
				      MT_AFE_INTERCONN_O09);
		mt_afe_set_connection(afe_info, MT_AFE_DISCONN,
				      MT_AFE_INTERCONN_I04,
				      MT_AFE_INTERCONN_O10);
		break;
	case AFE_AWB_ID:
		mt_afe_set_connection(afe_info, MT_AFE_DISCONN,
				      MT_AFE_INTERCONN_I00,
				      MT_AFE_INTERCONN_O05);
		mt_afe_set_connection(afe_info, MT_AFE_DISCONN,
				      MT_AFE_INTERCONN_I01,
				      MT_AFE_INTERCONN_O06);
		break;
	default:
		dev_err(afe_info->dev, "error: invalid DAI ID %d\n", id);
		return -EINVAL;
	};


	mt_afe_enable_afe(afe_info, false);

	vul_block->dma_read_idx = 0;
	vul_block->write_idx = 0;
	vul_block->data_remained = 0;
	return 0;
}

static int mt8135_afe_pcm_dl1_post_stop(struct mt_afe_info *afe_info,
					struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;

	dev_dbg(afe_info->dev, "%s\n", __func__);

	/*upstream fixme*/
	if (!strcmp(rtd->codec_dai->name, "mt-soc-codec-tx-dai")) {
		if (!mt_afe_get_i2s_dac_enable(afe_info))
			mt_afe_set_i2s_dac_enable(afe_info, false);

		/* here to turn off digital part */
		mt_afe_set_connection(afe_info, MT_AFE_DISCONN,
				      MT_AFE_INTERCONN_I05,
				      MT_AFE_INTERCONN_O03);
		mt_afe_set_connection(afe_info, MT_AFE_DISCONN,
				      MT_AFE_INTERCONN_I06,
				      MT_AFE_INTERCONN_O04);
	} else {
		mt_afe_set_2nd_i2s_enable(afe_info, MT_AFE_I2S_DIR_OUT, false);

		/* here to turn off digital part */
		mt_afe_set_connection(afe_info, MT_AFE_DISCONN,
				      MT_AFE_INTERCONN_I05,
				      MT_AFE_INTERCONN_O00);
		mt_afe_set_connection(afe_info, MT_AFE_DISCONN,
				      MT_AFE_INTERCONN_I06,
				      MT_AFE_INTERCONN_O01);
	}
	mt_afe_enable_afe(afe_info, false);
	return 0;
}

static int mt8135_afe_pcm_close(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct mt_afe_info *afe_info =
		snd_soc_platform_get_drvdata(rtd->platform);

	dev_dbg(afe_info->dev, "%s\n", __func__);
	afe_info->sub_stream[rtd->cpu_dai->id] = NULL;
	switch (rtd->cpu_dai->id) {
	case AFE_DL1_ID:
		mt8135_afe_pcm_dl1_post_stop(afe_info, substream);
		break;
	case AFE_VUL_ID:
	case AFE_AWB_ID:
		mt8135_afe_pcm_ul_stop(afe_info, substream, rtd->cpu_dai->id);
		/*SetDeepIdleEnableForAfe(true); upstream todo*/
		break;
	case AFE_HDMI_ID:
		/*SetDeepIdleEnableForHdmi(true); upstream todo*/
		return 0;
	default:
		dev_err(rtd->dev, "error: invalid DAI ID %d\n",
			rtd->cpu_dai->id);
		return -EINVAL;
	};
	mt_afe_clk_off(afe_info);
	return 0;
}

static int mt8135_afe_pcm_dl1_prestart(struct mt_afe_info *afe_info,
				       struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;

	/*upstream fixme*/
	if (!strcmp(rtd->codec_dai->name, "mt-soc-codec-tx-dai")) {
		pr_info("%s = dac\n", __func__);
		/* here start digital part */
		mt_afe_set_connection(afe_info, MT_AFE_CONN,
				      MT_AFE_INTERCONN_I05,
				      MT_AFE_INTERCONN_O03);
		mt_afe_set_connection(afe_info, MT_AFE_CONN,
				      MT_AFE_INTERCONN_I06,
				      MT_AFE_INTERCONN_O04);

		/* start I2S DAC out */
		mt_afe_set_i2s_dac_out(afe_info, substream->runtime->rate);
		mt_afe_set_memory_path_enable(afe_info, MT_AFE_PIN_I2S_DAC,
					      true);
		mt_afe_set_i2s_dac_enable(afe_info, true);
	} else {
		pr_info("%s = i2s\n", __func__);
		/* here start digital part */
		mt_afe_set_connection(afe_info, MT_AFE_CONN,
				      MT_AFE_INTERCONN_I05,
				      MT_AFE_INTERCONN_O00);
		mt_afe_set_connection(afe_info, MT_AFE_CONN,
				      MT_AFE_INTERCONN_I06,
				      MT_AFE_INTERCONN_O01);

		/* start 2nd I2S out */
		mt_afe_set_2nd_i2s(afe_info, substream->runtime->rate);
		mt_afe_set_2nd_i2s_enable(afe_info, MT_AFE_I2S_DIR_OUT, true);
	}
	mt_afe_enable_afe(afe_info, true);
	return 0;
}


static int mt8135_afe_pcm_stop(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct mt_afe_info *afe_info =
		snd_soc_platform_get_drvdata(rtd->platform);
	struct mt_afe_block *block = NULL;

	dev_dbg(afe_info->dev, "%s\n", __func__);
	switch (rtd->cpu_dai->id) {
	case AFE_DL1_ID:
		mt_afe_set_memory_path_enable(afe_info, MT_AFE_PIN_DL1, false);
		mt_afe_set_irq_enable(afe_info, MT_AFE_IRQ_MODE_1, false);

		/* clean audio hardware buffer */
		return mt_afe_reset_buffer(afe_info, MT_AFE_PIN_DL1);
	case AFE_VUL_ID:
	case AFE_AWB_ID:
		return mt8135_afe_pcm_ul_stop(afe_info, substream,
					      rtd->cpu_dai->id);

	case AFE_HDMI_ID:
		mt8135_afe_hdmi_set_connection(afe_info, MT_AFE_DISCONN,
					       runtime->channels);

		mt_afe_set_hdmi_i2s_enable(afe_info, false);

		mt_afe_set_hdmi_out_control_enable(afe_info, false);

		mt_afe_set_irq_enable(afe_info, MT_AFE_IRQ_MODE_5, false);

		mt_afe_set_hdmi_path_enable(afe_info, false);

		mt_afe_enable_afe(afe_info, false);

		/* clean audio hardware buffer */
		block = &(afe_info->memif[MT_AFE_PIN_HDMI].block);
		memset(block->virt_buf_addr, 0, block->buffer_size);
		block->dma_read_idx = 0;
		block->write_idx = 0;
		block->data_remained = 0;

		mt_hdmi_clk_off(afe_info);
		mt_apll_tuner_clk_off(afe_info);
		mt_afe_clk_off(afe_info);
		return 0;
	default:
		dev_err(rtd->dev, "error: invalid DAI ID %d\n",
			rtd->cpu_dai->id);
		return -EINVAL;
	}
}

static snd_pcm_uframes_t mt8135_afe_pcm_pointer
	(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct mt_afe_info *afe_info =
		snd_soc_platform_get_drvdata(rtd->platform);

	return mt_afe_update_hw_ptr(afe_info, rtd->cpu_dai->id, substream);
}

static int mt8135_afe_pcm_hw_params(struct snd_pcm_substream *substream,
				    struct snd_pcm_hw_params *hw_params)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_dma_buffer *dma_buf = &substream->dma_buffer;
	struct mt_afe_block *block = NULL;
	struct mt_afe_info *afe_info =
		snd_soc_platform_get_drvdata(rtd->platform);
	int ret = 0;

	dev_dbg(afe_info->dev, "%s\n", __func__);
	switch (rtd->cpu_dai->id) {
	case AFE_DL1_ID:
		if (params_buffer_bytes(hw_params) > afe_info->afe_sram_size) {
			dev_warn(afe_info->dev,
				 "%s request size %d > max size %d\n", __func__,
				 params_buffer_bytes(hw_params),
				 afe_info->afe_sram_size);
		}

		/* here to allcoate sram to hardware */
		mt_afe_allocate_dl1_buffer(afe_info, substream);
		dev_dbg(afe_info->dev,
			"%s dma_bytes = %d dma_area = %p dma_addr = 0x%x\n",
			__func__, substream->runtime->dma_bytes,
			substream->runtime->dma_area,
			substream->runtime->dma_addr);

		dev_dbg(afe_info->dev,
			"%s rate = %d channels = %d device = %d\n",
			__func__, runtime->rate, runtime->channels,
			substream->pcm->device);
		break;
	case AFE_VUL_ID:
	case AFE_AWB_ID:
		ret = snd_pcm_lib_malloc_pages(substream,
					       params_buffer_bytes(hw_params));

		if (ret >= 0)
			mt8135_afe_set_ul_buf(afe_info, substream, hw_params,
					      rtd->cpu_dai->id);
		else
			dev_err(afe_info->dev,
				"%s snd_pcm_lib_malloc_pages fail %d\n",
				__func__, ret);

		dev_dbg(afe_info->dev,
			"%s dma_bytes = %d dma_area = %p dma_addr = 0x%x\n",
			__func__, runtime->dma_bytes,
			runtime->dma_area, runtime->dma_addr);
		break;

	case AFE_HDMI_ID:
		block = &(afe_info->memif[MT_AFE_PIN_HDMI].block);
		memset(block, 0, sizeof(struct mt_afe_block));

		dma_buf->dev.type = SNDRV_DMA_TYPE_DEV;
		dma_buf->dev.dev = substream->pcm->card->dev;

		ret = snd_pcm_lib_malloc_pages(substream,
					       params_buffer_bytes(hw_params));

		if (ret >= 0) {
			block->virt_buf_addr = runtime->dma_area;
			block->phys_buf_addr = runtime->dma_addr;
			block->buffer_size = runtime->dma_bytes;
		} else {
			dev_err(afe_info->dev,
				"%s snd_pcm_lib_malloc_pages fail %d\n",
				__func__, ret);
		}

		dev_info(afe_info->dev,
			 "%s dma_bytes = %d dma_area = %p dma_addr = 0x%x\n",
			 __func__, runtime->dma_bytes, runtime->dma_area,
			 runtime->dma_addr);
		break;


	default:
		dev_err(rtd->dev, "error: invalid DAI ID %d\n",
			rtd->cpu_dai->id);
		return -EINVAL;
	};
	return ret;
}

static int mt8135_afe_pcm_hw_free(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct mt_afe_info *afe_info =
		snd_soc_platform_get_drvdata(rtd->platform);

	dev_dbg(afe_info->dev, "%s\n", __func__);

	switch (rtd->cpu_dai->id) {
	case AFE_DL1_ID:
		return 0;
	case AFE_VUL_ID:
	case AFE_AWB_ID:
		return snd_pcm_lib_free_pages(substream);
	case AFE_HDMI_ID:
		memset(&afe_info->memif[MT_AFE_PIN_HDMI], 0,
		       sizeof(struct mt_afe_memif));
		return snd_pcm_lib_free_pages(substream);
	default:
		dev_err(rtd->dev, "error: invalid DAI ID %d\n",
			rtd->cpu_dai->id);
		return -EINVAL;
	}
}

/* Conventional and unconventional sample rate supported */
static const unsigned int mt8135_d1_sample_rates[] = {
	8000, 11025, 12000, 16000, 22050, 24000, 32000, 44100, 48000
};

static struct snd_pcm_hw_constraint_list mt8135_d1_constraint_rates = {
	.count = ARRAY_SIZE(mt8135_d1_sample_rates),
	.list = mt8135_d1_sample_rates,
	.mask = 0,
};

/* Conventional and unconventional sample rate supported */
static unsigned int mt8135_vul_sample_rates[] = {
	8000, 16000, 32000, 48000
};

static struct snd_pcm_hw_constraint_list mt8135_ul_constraint_rates = {
	.count = ARRAY_SIZE(mt8135_vul_sample_rates),
	.list = mt8135_vul_sample_rates,
};

static int mt8135_afe_pcm_open(struct snd_pcm_substream *substream)
{
	int ret;
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct mt_afe_info *afe_info =
		snd_soc_platform_get_drvdata(rtd->platform);

	dev_dbg(afe_info->dev, "%s\n", __func__);

	afe_info->sub_stream[rtd->cpu_dai->id] = substream;
	switch (rtd->cpu_dai->id) {
	case AFE_DL1_ID:
		afe_info->prepared = false;
		snd_soc_set_runtime_hwparams(substream, &mt8135_d1_hardware);

		dev_info(afe_info->dev,
			 "%s rates= 0x%x mt8135_d1_hardware= %p hw= %p\n",
			 __func__, runtime->hw.rates,
			 &mt8135_d1_hardware, &runtime->hw);

		ret = snd_pcm_hw_constraint_list(runtime, 0,
						 SNDRV_PCM_HW_PARAM_RATE,
						 &mt8135_d1_constraint_rates);
		if (ret < 0)
			dev_err(afe_info->dev,
				"snd_pcm_hw_constraint_list failed: 0x%x\n",
				ret);

		ret = snd_pcm_hw_constraint_integer(runtime,
						    SNDRV_PCM_HW_PARAM_PERIODS);
		if (ret < 0)
			dev_err(afe_info->dev,
				"snd_pcm_hw_constraint_integer failed: 0x%x\n",
				ret);

		/* here open audio clocks */
		mt_afe_clk_on(afe_info);

		/* print for hw pcm information */
		dev_dbg(afe_info->dev,
			"%s rate = %d channels = %d device = %d\n",
			__func__, runtime->rate, runtime->channels,
			substream->pcm->device);

		runtime->hw.info |= (SNDRV_PCM_INFO_INTERLEAVED |
					SNDRV_PCM_INFO_NONINTERLEAVED |
					SNDRV_PCM_INFO_MMAP_VALID);

		if (ret < 0) {
			dev_err(afe_info->dev, "%s mt8135_afe_pcm_close\n",
				__func__);
			mt8135_afe_pcm_close(substream);
			return ret;
		}
		break;
	case AFE_VUL_ID:
	case AFE_AWB_ID:
		snd_soc_set_runtime_hwparams(substream, &mt8135_ul_hardware);

		dev_dbg(afe_info->dev,
			"%s rates = 0x%x mt8135_ul_hardware = %p\n ",
			__func__, runtime->hw.rates, &mt8135_ul_hardware);

		ret = snd_pcm_hw_constraint_list(runtime, 0,
						 SNDRV_PCM_HW_PARAM_RATE,
						 &mt8135_ul_constraint_rates);
		if (ret < 0)
			dev_err(afe_info->dev,
				"%s snd_pcm_hw_constraint_list failed %d\n",
				__func__, ret);

		ret = snd_pcm_hw_constraint_integer(runtime,
						    SNDRV_PCM_HW_PARAM_PERIODS);

		if (ret < 0)
			dev_err(afe_info->dev,
				"%s snd_pcm_hw_constraint_integer failed %d\n",
				__func__, ret);

		/* here open audio clocks */
		mt_afe_clk_on(afe_info);

		/* print for hw pcm information */
		dev_info(afe_info->dev,
			 "%s runtime rate = %u channels = %u device = %d\n",
			 __func__, runtime->rate, runtime->channels,
			 substream->pcm->device);

		if (substream->pcm->device & 1) {
			runtime->hw.info |= SNDRV_PCM_INFO_INTERLEAVED;
			runtime->hw.info |= SNDRV_PCM_INFO_NONINTERLEAVED;
		}
		if (substream->pcm->device & 2) {
			runtime->hw.info |= SNDRV_PCM_INFO_INTERLEAVED;
			runtime->hw.info |= SNDRV_PCM_INFO_NONINTERLEAVED;
		}

		if (ret < 0) {
			dev_err(afe_info->dev,
				"%s mt8135_afe_pcm_close\n", __func__);
			mt8135_afe_pcm_close(substream);
			return ret;
		}
		/*SetDeepIdleEnableForAfe(false); upstream todo*/
		break;
	case AFE_HDMI_ID:
		snd_soc_set_runtime_hwparams(substream, &mt8135_hdmi_hardware);

		memset(&afe_info->memif[MT_AFE_PIN_HDMI], 0,
		       sizeof(struct mt_afe_memif));
		/* Ensure that buffer size is a multiple of period size */
		ret = snd_pcm_hw_constraint_integer(runtime,
						    SNDRV_PCM_HW_PARAM_PERIODS);
		if (ret < 0) {
			dev_err(afe_info->dev,
				"%s snd_pcm_hw_constraint_integer fail %d\n",
				__func__, ret);
			return ret;
		}

		dev_info(afe_info->dev,
			 "%s rate = %d channels = %d device = %d\n",
			  __func__, runtime->rate, runtime->channels,
			 substream->pcm->device);

		/*SetDeepIdleEnableForHdmi(false); upstream todo*/
		break;
	default:
		dev_err(rtd->dev, "error: invalid DAI ID %d\n",
			rtd->cpu_dai->id);
		return -EINVAL;
	};


	dev_dbg(afe_info->dev, "%s return\n", __func__);
	return 0;
}

static int mt8135_afe_pcm_prepare(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime_stream = substream->runtime;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct mt_afe_info *afe_info =
		snd_soc_platform_get_drvdata(rtd->platform);

	dev_dbg(afe_info->dev, "%s rate= %u channels= %u period_size= %lu\n",
		__func__, runtime_stream->rate, runtime_stream->channels,
		 runtime_stream->period_size);
	switch (rtd->cpu_dai->id) {
	case AFE_DL1_ID:
		/* HW sequence: */
		/* mt8135_afe_pcm_dl1_prestart->codec->mt8135_afe_pcm_start */
		if (!afe_info->prepared) {
			mt8135_afe_pcm_dl1_prestart(afe_info, substream);
			afe_info->prepared = true;
		}
		break;
	case AFE_VUL_ID:
	case AFE_AWB_ID:
	case AFE_HDMI_ID:
		break;
	default:
		dev_err(rtd->dev, "error: invalid DAI ID %d\n",
			rtd->cpu_dai->id);
		return -EINVAL;
	};
	return 0;
}

static int mt8135_afe_pcm_start(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime * const runtime = substream->runtime;
	struct timespec curr_tstamp;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct mt_afe_info *afe_info =
		snd_soc_platform_get_drvdata(rtd->platform);
	struct mt_afe_i2s i2s_setting;

	dev_dbg(afe_info->dev, "%s period = %lu,runtime->rate= %u, runtime->channels=%u\n",
		__func__, runtime->period_size, runtime->rate,
		runtime->channels);
	switch (rtd->cpu_dai->id) {
	case AFE_DL1_ID:

		/* set dl1 sample rate */
		mt_afe_set_sample_rate(afe_info, MT_AFE_PIN_DL1, runtime->rate);

		/* here to set interrupt */
		mt_afe_set_irq_counter(afe_info, MT_AFE_IRQ_MODE_1,
				       runtime->period_size);
		mt_afe_set_irq_fs(afe_info, MT_AFE_IRQ_MODE_1,
				  runtime->rate);

		mt_afe_set_memory_path_enable(afe_info, MT_AFE_PIN_DL1, true);
		mt_afe_set_irq_enable(afe_info, MT_AFE_IRQ_MODE_1, true);
		snd_pcm_gettime(substream->runtime,
				(struct timespec *)&curr_tstamp);

		dev_dbg(afe_info->dev, "%s curr_tstamp %ld %ld\n", __func__,
			curr_tstamp.tv_sec, curr_tstamp.tv_nsec);
		break;
	case AFE_VUL_ID:
	case AFE_AWB_ID:
		if (rtd->cpu_dai->id == AFE_VUL_ID) {
			/*config ADC I2S*/
			memset(&i2s_setting, 0, sizeof(i2s_setting));
			i2s_setting.lr_swap = MT_AFE_LR_SWAP_NO;
			i2s_setting.buffer_update_word = 8;
			i2s_setting.fpga_bit_test = 0;
			i2s_setting.fpga_bit = 0;
			i2s_setting.loopback = 0;
			i2s_setting.inv_lrck = MT_AFE_INV_LRCK_NO;
			i2s_setting.i2s_fmt = MT_AFE_I2S_FORMAT_I2S;
			i2s_setting.i2s_wlen = MT_AFE_I2S_WLEN_16BITS;
			i2s_setting.i2s_sample_rate = substream->runtime->rate;

			mt_afe_set_i2s_adc(afe_info, &i2s_setting);
			mt_afe_set_i2s_adc_enable(afe_info, true);
		} else {
			/*config 2nd I2S*/
			mt_afe_set_2nd_i2s(afe_info, runtime->rate);
			mt_afe_set_2nd_i2s_enable(afe_info, MT_AFE_I2S_DIR_IN,
						  true);
		}
		mt_afe_set_sample_rate(afe_info, rtd->cpu_dai->id,
				       substream->runtime->rate);
		mt_afe_set_ch(afe_info, rtd->cpu_dai->id,
			      ((substream->runtime->channels == 2) ? 0 : 1));
		mt_afe_set_memory_path_enable(afe_info, rtd->cpu_dai->id, true);

		/* here to set interrupt */
		if (!afe_info->audio_mcu_mode[MT_AFE_IRQ_MODE_2].status) {
			mt_afe_set_irq_counter(afe_info, MT_AFE_IRQ_MODE_2,
					       substream->runtime->period_size);
			mt_afe_set_irq_fs(afe_info, MT_AFE_IRQ_MODE_2,
					  substream->runtime->rate);
			mt_afe_set_irq_enable(afe_info,
					      MT_AFE_IRQ_MODE_2, true);
			snd_pcm_gettime(substream->runtime,
					(struct timespec *)&curr_tstamp);
			dev_dbg(afe_info->dev, "%s curr_tstamp %ld %ld\n",
				__func__,
				curr_tstamp.tv_sec, curr_tstamp.tv_nsec);

		} else {
			dev_dbg(afe_info->dev,
				"%s IRQ2 enabled , use original irq2 mode\n",
				__func__);
		}
		/* here to turn off digital part */
		if (rtd->cpu_dai->id == AFE_VUL_ID) {
			mt_afe_set_connection(afe_info, MT_AFE_CONN,
					      MT_AFE_INTERCONN_I03,
					      MT_AFE_INTERCONN_O09);
			mt_afe_set_connection(afe_info, MT_AFE_CONN,
					      MT_AFE_INTERCONN_I04,
					      MT_AFE_INTERCONN_O10);
		} else {
			mt_afe_set_connection(afe_info, MT_AFE_CONN,
					      MT_AFE_INTERCONN_I00,
					      MT_AFE_INTERCONN_O05);
			mt_afe_set_connection(afe_info, MT_AFE_CONN,
					      MT_AFE_INTERCONN_I01,
					      MT_AFE_INTERCONN_O06);
		}
		mt_afe_enable_afe(afe_info, true);
		break;
	case AFE_HDMI_ID:
		dev_info(afe_info->dev, "%s period_size = %lu\n",
			 __func__, runtime->period_size);

		/* enable audio clock*/
		mt_afe_clk_on(afe_info);
		mt_afe_set_hdmi_clock_source(afe_info, runtime->rate);
		mt_apll_tuner_clk_on(afe_info);
		mt_hdmi_clk_on(afe_info);

		mt_afe_set_reg(afe_info, AFE_HDMI_OUT_BASE,
			       runtime->dma_addr, 0xFFFFFFFF);
		mt_afe_set_reg(afe_info, AFE_HDMI_OUT_END,
			       runtime->dma_addr + (runtime->dma_bytes - 1),
			       0xFFFFFFFF);

		/* here to set interrupt */
		mt_afe_set_irq_counter(afe_info, MT_AFE_IRQ_MODE_5,
				       runtime->period_size);
		mt_afe_set_irq_enable(afe_info, MT_AFE_IRQ_MODE_5, true);

		/* interconnection */
		mt8135_afe_hdmi_set_connection(afe_info, MT_AFE_CONN,
					       runtime->channels);

		/* HDMI Out control */
		mt_afe_set_hdmi_out_control(afe_info, runtime->channels);
		mt_afe_set_hdmi_out_control_enable(afe_info, true);

		/* HDMI I2S */
		mt_afe_set_hdmi_i2s(afe_info);
		mt_afe_set_hdmi_bck_div(afe_info, runtime->rate);
		mt_afe_set_hdmi_i2s_enable(afe_info, true);

		mt_afe_set_hdmi_path_enable(afe_info, true);
		mt_afe_enable_afe(afe_info, true);
		break;
	default:
		dev_err(rtd->dev, "error: invalid DAI ID %d\n",
			rtd->cpu_dai->id);
		return -EINVAL;
	};
	return 0;
}

static int mt8135_afe_pcm_trigger(struct snd_pcm_substream *substream, int cmd)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct mt_afe_info *afe_info =
		snd_soc_platform_get_drvdata(rtd->platform);

	dev_info(afe_info->dev, "%s cmd=%d\n", __func__, cmd);

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
		return mt8135_afe_pcm_start(substream);
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
		return mt8135_afe_pcm_stop(substream);
	}
	return -EINVAL;
}

static int mt8135_afe_pcm_probe(struct snd_soc_platform *platform)
{
	struct mt_afe_info *afe_info = snd_soc_platform_get_drvdata(platform);

	mt8135_afe_debugfs =
		debugfs_create_file("afe_reg", S_IFREG | S_IRUGO,
				    platform->debugfs_platform_root,
				    (void *)afe_info,
				    &mt8135_afe_debug_ops);
	return 0;
}

static int mt8135_afe_pcm_remove(struct snd_soc_platform *platform)
{
	debugfs_remove(mt8135_afe_debugfs);
	return 0;
}

static int mt8135_afe_pcm_new(struct snd_soc_pcm_runtime *rtd)
{
	size_t size;
	struct snd_card *card = rtd->card->snd_card;
	struct snd_pcm *pcm = rtd->pcm;
	struct mt_afe_info *afe_info =
	snd_soc_platform_get_drvdata(rtd->platform);

	dev_dbg(afe_info->dev, "mt8135_afe_pcm_new %p\n", pcm);
	switch (rtd->cpu_dai->id) {
	case AFE_DL1_ID:
	case AFE_HDMI_ID:
		return 0;
	case AFE_VUL_ID:
	case AFE_AWB_ID:
		size = mt8135_ul_hardware.buffer_bytes_max;
		return snd_pcm_lib_preallocate_pages_for_all(pcm,
							    SNDRV_DMA_TYPE_DEV,
							    card->dev,
							    size,
							    size);
		return 0;
	default:
		dev_err(rtd->dev, "error: invalid DAI ID %d\n",
			rtd->cpu_dai->id);
		return -EINVAL;
	};
}

static void mt8135_afe_pcm_free(struct snd_pcm *pcm)
{
	snd_pcm_lib_preallocate_free_for_all(pcm);
}

#ifdef CONFIG_PM
static int mt8135_afe_suspend(struct snd_soc_dai *dai)
{
	struct mt_afe_info *afe_info = snd_soc_dai_get_drvdata(dai);

	/*upstream to check*/
	if (afe_info)
		mt_afe_suspend(afe_info);
	return 0;
}

static int mt8135_afe_resume(struct snd_soc_dai *dai)
{
	struct mt_afe_info *afe_info = snd_soc_dai_get_drvdata(dai);

	/*upstream to check*/
	if (afe_info)
		mt_afe_resume(afe_info);
	return 0;
}
#else
#define mt8135_afe_suspend	NULL
#define mt8135_afe_resume	NULL
#endif

static struct snd_pcm_ops mt8135_afe_pcm_ops = {
	.open = mt8135_afe_pcm_open,
	.close = mt8135_afe_pcm_close,
	.ioctl = snd_pcm_lib_ioctl,
	.hw_params = mt8135_afe_pcm_hw_params,
	.hw_free = mt8135_afe_pcm_hw_free,
	.prepare = mt8135_afe_pcm_prepare,
	.trigger = mt8135_afe_pcm_trigger,
	.pointer = mt8135_afe_pcm_pointer,
};

static struct snd_soc_dai_driver mt8135_afe_pcm_dais[] = {
	{
		.name  = "DL1",
		.id  = AFE_DL1_ID,
		.playback = {
			.stream_name = "Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_8000_48000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
		},
	},
	{
		.name  = "AWB",
		.id  = AFE_AWB_ID,
		.capture = {
			.stream_name = "Capture",
			.channels_min = 1,
			.channels_max = 2,
			.rates = UL_RATE,
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
		},
	},
	{
		.name  = "VUL",
		.id  = AFE_VUL_ID,
		.capture = {
			.stream_name = "Capture",
			.channels_min = 1,
			.channels_max = 2,
			.rates = UL_RATE,
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
		},
	},
	{
		.name = "HDMI",
		.id  = AFE_HDMI_ID,
		.playback = {
			.stream_name = "HDMI Playback",
			.channels_min = 2,
			.channels_max = 8,
			.rates = HDMI_RATES,
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
		},
	},
	{
		.name  = "Routing",
		.id  = AFE_ROUTING_ID,
	},
};

static struct snd_soc_platform_driver mt8135_afe_pcm_platform = {
	.ops = &mt8135_afe_pcm_ops,
	.probe = mt8135_afe_pcm_probe,
	.remove	= mt8135_afe_pcm_remove,
	.pcm_new = mt8135_afe_pcm_new,
	.pcm_free = mt8135_afe_pcm_free,
	.suspend = mt8135_afe_suspend,
	.resume = mt8135_afe_resume,
};

static const struct snd_soc_component_driver mt8135_afe_pcm_dai_component = {
	.name		= "mt8135-afe-pcm-dai",
};

static int mt8135_afe_pcm_dev_probe(struct platform_device *pdev)
{
	int ret = 0;

	if (pdev->dev.of_node)
		dev_set_name(&pdev->dev, "%s", "mt8135-afe-pcm");

	ret = mt_afe_platform_init(pdev);
	if (ret)
		return ret;

	ret = snd_soc_register_platform(&pdev->dev, &mt8135_afe_pcm_platform);
	if (ret)
		return ret;

	return snd_soc_register_component(&pdev->dev,
					  &mt8135_afe_pcm_dai_component,
					  mt8135_afe_pcm_dais,
					  ARRAY_SIZE(mt8135_afe_pcm_dais));
}

static int mt8135_afe_pcm_dev_remove(struct platform_device *pdev)
{
	struct mt_afe_info *afe_info = platform_get_drvdata(pdev);

	dev_dbg(&pdev->dev, "%s\n", __func__);
	mt_afe_rm_audio_clk(afe_info);

	snd_soc_unregister_platform(&pdev->dev);
	snd_soc_unregister_component(&pdev->dev);
	return 0;
}

static const struct of_device_id mt8135_afe_pcm_dt_match[] = {
	{ .compatible = "mediatek,mt8135-afe-pcm", },
	{ }
};

static struct platform_driver mt8135_afe_pcm_driver = {
	.driver = {
		   .name = "mt8135-afe-pcm",
		   .owner = THIS_MODULE,
		   .of_match_table = mt8135_afe_pcm_dt_match,
		   },
	.probe = mt8135_afe_pcm_dev_probe,
	.remove = mt8135_afe_pcm_dev_remove,
};

module_platform_driver(mt8135_afe_pcm_driver);

MODULE_DESCRIPTION("Mediatek ALSA SoC Afe platform driver");
MODULE_AUTHOR("Koro Chen <koro.chen@mediatek.com>");
MODULE_LICENSE("GPL v2");
MODULE_DEVICE_TABLE(of, mt8135_afe_pcm_dt_match);

