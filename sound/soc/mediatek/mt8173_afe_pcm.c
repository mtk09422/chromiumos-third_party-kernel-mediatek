/*
 * mt8173_afe_pcm.c  --  Mediatek ALSA SoC Afe platform driver
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
#include "mtk_afe_digital_type.h"
#include "mtk_afe_connection.h"
#include <linux/delay.h>
#include <linux/module.h>
#include <sound/soc.h>
#include <linux/debugfs.h>

#define AFE_DL1_ID	0
#define AFE_AWB_ID	1
#define AFE_VUL_ID	2
#define AFE_HDMI_ID	3
#define AFE_DAI_NUM	(AFE_HDMI_ID + 1)

static DEFINE_SPINLOCK(mtk_afe_control_lock);

struct mt8173_afe_dai_id {
	int id;
	int pin;
};

static const struct mt8173_afe_dai_id dai_map[] = {
	{AFE_DL1_ID,	MTK_AFE_PIN_DL1},
	{AFE_AWB_ID,	MTK_AFE_PIN_AWB},
	{AFE_VUL_ID,	MTK_AFE_PIN_VUL},
	{AFE_HDMI_ID,	MTK_AFE_PIN_HDMI},
};

#define MTK_AFE_HDMI_RATES   (SNDRV_PCM_RATE_32000 | SNDRV_PCM_RATE_44100 | \
			      SNDRV_PCM_RATE_48000 | SNDRV_PCM_RATE_88200 | \
			      SNDRV_PCM_RATE_96000 | SNDRV_PCM_RATE_176400 | \
			      SNDRV_PCM_RATE_192000)

/*
 * FIXME:  can we merge these?
 * they are separated because snd_pcm_lib_preallocate_pages_for_all
 * pre-allocate buffer according to buffer_bytes_max,
 * but only HDMI needs larger size for 8ch playback
 */
static const struct snd_pcm_hardware mt8173_afe_hardware = {
	.info = (SNDRV_PCM_INFO_MMAP | SNDRV_PCM_INFO_INTERLEAVED |
		 SNDRV_PCM_INFO_RESUME | SNDRV_PCM_INFO_MMAP_VALID),
	.buffer_bytes_max = 16*1024,
	.period_bytes_max = 8*1024,
	.periods_min = 2,
	.periods_max = 256,
	.fifo_size = 0,
};

static const struct snd_pcm_hardware mt8173_hdmi_hardware = {
	.info = (SNDRV_PCM_INFO_MMAP | SNDRV_PCM_INFO_INTERLEAVED |
		 SNDRV_PCM_INFO_RESUME | SNDRV_PCM_INFO_MMAP_VALID),
	.buffer_bytes_max = 256*1024,
	.period_bytes_max = 128*1024,
	.periods_min = 2,
	.periods_max = 256,
	.fifo_size = 0,
};

/* FIXME: to remove debugfs */
static struct dentry *mt8173_afe_debugfs;

static int mt8173_afe_debug_open(struct inode *inode, struct file *file)
{
	file->private_data = inode->i_private;
	return 0;
}

static ssize_t mt8173_afe_debug_read(struct file *file, char __user *buf,
				     size_t count, loff_t *pos)
{
	struct mtk_afe_info *afe_info = file->private_data;
	const int size = 4096;
	char buffer[size];
	int n = 0;

	n = scnprintf(buffer + n, size - n, "AUDIO_TOP_CON0 = 0x%x\n",
		      mtk_afe_get_reg(afe_info, AUDIO_TOP_CON0));
	n += scnprintf(buffer + n, size - n, "AUDIO_TOP_CON1 = 0x%x\n",
		       mtk_afe_get_reg(afe_info, AUDIO_TOP_CON1));
	n += scnprintf(buffer + n, size - n, "AUDIO_TOP_CON2 = 0x%x\n",
		       mtk_afe_get_reg(afe_info, AUDIO_TOP_CON2));
	n += scnprintf(buffer + n, size - n, "AUDIO_TOP_CON3 = 0x%x\n",
		       mtk_afe_get_reg(afe_info, AUDIO_TOP_CON3));
	n += scnprintf(buffer + n, size - n, "AFE_TOP_CON0 = 0x%x\n",
		       mtk_afe_get_reg(afe_info, AFE_TOP_CON0));
	n += scnprintf(buffer + n, size - n, "AFE_DAC_CON0 = 0x%x\n",
		       mtk_afe_get_reg(afe_info, AFE_DAC_CON0));
	n += scnprintf(buffer + n, size - n, "AFE_DAC_CON1 = 0x%x\n",
		       mtk_afe_get_reg(afe_info, AFE_DAC_CON1));
	n += scnprintf(buffer + n, size - n, "AFE_I2S_CON = 0x%x\n",
		       mtk_afe_get_reg(afe_info, AFE_I2S_CON));
	n += scnprintf(buffer + n, size - n, "AFE_I2S_CON1 = 0x%x\n",
		       mtk_afe_get_reg(afe_info, AFE_I2S_CON1));
	n += scnprintf(buffer + n, size - n, "AFE_I2S_CON2 = 0x%x\n",
		       mtk_afe_get_reg(afe_info, AFE_I2S_CON2));
	n += scnprintf(buffer + n, size - n, "AFE_I2S_CON3 = 0x%x\n",
		       mtk_afe_get_reg(afe_info, AFE_I2S_CON3));

	n += scnprintf(buffer + n, size - n, "AFE_CONN0 = 0x%x\n",
		       mtk_afe_get_reg(afe_info, AFE_CONN0));
	n += scnprintf(buffer + n, size - n, "AFE_CONN1 = 0x%x\n",
		       mtk_afe_get_reg(afe_info, AFE_CONN1));
	n += scnprintf(buffer + n, size - n, "AFE_CONN2 = 0x%x\n",
		       mtk_afe_get_reg(afe_info, AFE_CONN2));
	n += scnprintf(buffer + n, size - n, "AFE_CONN3 = 0x%x\n",
		       mtk_afe_get_reg(afe_info, AFE_CONN3));
	n += scnprintf(buffer + n, size - n, "AFE_CONN4 = 0x%x\n",
		       mtk_afe_get_reg(afe_info, AFE_CONN4));
	n += scnprintf(buffer + n, size - n, "AFE_CONN5 = 0x%x\n",
		       mtk_afe_get_reg(afe_info, AFE_CONN5));
	n += scnprintf(buffer + n, size - n, "AFE_CONN6 = 0x%x\n",
		       mtk_afe_get_reg(afe_info, AFE_CONN6));
	n += scnprintf(buffer + n, size - n, "AFE_CONN7 = 0x%x\n",
		       mtk_afe_get_reg(afe_info, AFE_CONN7));
	n += scnprintf(buffer + n, size - n, "AFE_CONN8 = 0x%x\n",
		       mtk_afe_get_reg(afe_info, AFE_CONN8));
	n += scnprintf(buffer + n, size - n, "AFE_CONN9 = 0x%x\n",
		       mtk_afe_get_reg(afe_info, AFE_CONN9));
	n += scnprintf(buffer + n, size - n, "AFE_CONN_24BIT = 0x%x\n",
		       mtk_afe_get_reg(afe_info, AFE_CONN_24BIT));

	n += scnprintf(buffer + n, size - n, "AFE_DAIBT_CON0 = 0x%x\n",
		       mtk_afe_get_reg(afe_info, AFE_DAIBT_CON0));
	n += scnprintf(buffer + n, size - n, "AFE_MRGIF_CON = 0x%x\n",
		       mtk_afe_get_reg(afe_info, AFE_MRGIF_CON));
	n += scnprintf(buffer + n, size - n, "AFE_MRGIF_MON0 = 0x%x\n",
		       mtk_afe_get_reg(afe_info, AFE_MRGIF_MON0));
	n += scnprintf(buffer + n, size - n, "AFE_MRGIF_MON1 = 0x%x\n",
		       mtk_afe_get_reg(afe_info, AFE_MRGIF_MON1));
	n += scnprintf(buffer + n, size - n, "AFE_MRGIF_MON2 = 0x%x\n",
		       mtk_afe_get_reg(afe_info, AFE_MRGIF_MON2));

	n += scnprintf(buffer + n, size - n, "AFE_DL1_BASE = 0x%x\n",
		       mtk_afe_get_reg(afe_info, AFE_DL1_BASE));
	n += scnprintf(buffer + n, size - n, "AFE_DL1_CUR = 0x%x\n",
		       mtk_afe_get_reg(afe_info, AFE_DL1_CUR));
	n += scnprintf(buffer + n, size - n, "AFE_DL1_END = 0x%x\n",
		       mtk_afe_get_reg(afe_info, AFE_DL1_END));
	n += scnprintf(buffer + n, size - n, "AFE_DL2_BASE = 0x%x\n",
		       mtk_afe_get_reg(afe_info, AFE_DL2_BASE));
	n += scnprintf(buffer + n, size - n, "AFE_DL2_CUR = 0x%x\n",
		       mtk_afe_get_reg(afe_info, AFE_DL2_CUR));
	n += scnprintf(buffer + n, size - n, "AFE_DL2_END = 0x%x\n",
		       mtk_afe_get_reg(afe_info, AFE_DL2_END));
	n += scnprintf(buffer + n, size - n, "AFE_AWB_BASE = 0x%x\n",
		       mtk_afe_get_reg(afe_info, AFE_AWB_BASE));
	n += scnprintf(buffer + n, size - n, "AFE_AWB_END = 0x%x\n",
		       mtk_afe_get_reg(afe_info, AFE_AWB_END));
	n += scnprintf(buffer + n, size - n, "AFE_AWB_CUR = 0x%x\n",
		       mtk_afe_get_reg(afe_info, AFE_AWB_CUR));
	n += scnprintf(buffer + n, size - n, "AFE_VUL_BASE = 0x%x\n",
		       mtk_afe_get_reg(afe_info, AFE_VUL_BASE));
	n += scnprintf(buffer + n, size - n, "AFE_VUL_END = 0x%x\n",
		       mtk_afe_get_reg(afe_info, AFE_VUL_END));
	n += scnprintf(buffer + n, size - n, "AFE_VUL_CUR = 0x%x\n",
		       mtk_afe_get_reg(afe_info, AFE_VUL_CUR));
	n += scnprintf(buffer + n, size - n, "AFE_DAI_BASE = 0x%x\n",
		       mtk_afe_get_reg(afe_info, AFE_DAI_BASE));
	n += scnprintf(buffer + n, size - n, "AFE_DAI_END = 0x%x\n",
		       mtk_afe_get_reg(afe_info, AFE_DAI_END));
	n += scnprintf(buffer + n, size - n, "AFE_DAI_CUR = 0x%x\n",
		       mtk_afe_get_reg(afe_info, AFE_DAI_CUR));
	n += scnprintf(buffer + n, size - n, "AFE_DL1_D2_BASE = 0x%x\n",
		       mtk_afe_get_reg(afe_info, AFE_DL1_D2_BASE));
	n += scnprintf(buffer + n, size - n, "AFE_DL1_D2_END = 0x%x\n",
		       mtk_afe_get_reg(afe_info, AFE_DL1_D2_END));
	n += scnprintf(buffer + n, size - n, "AFE_DL1_D2_CUR = 0x%x\n",
		       mtk_afe_get_reg(afe_info, AFE_DL1_D2_CUR));
	n += scnprintf(buffer + n, size - n, "AFE_VUL_D2_BASE = 0x%x\n",
		       mtk_afe_get_reg(afe_info, AFE_VUL_D2_BASE));
	n += scnprintf(buffer + n, size - n, "AFE_VUL_D2_END = 0x%x\n",
		       mtk_afe_get_reg(afe_info, AFE_VUL_D2_END));
	n += scnprintf(buffer + n, size - n, "AFE_VUL_D2_CUR = 0x%x\n",
		       mtk_afe_get_reg(afe_info, AFE_VUL_D2_CUR));

	n += scnprintf(buffer + n, size - n, "AFE_MEMIF_MSB = 0x%x\n",
		       mtk_afe_get_reg(afe_info, AFE_MEMIF_MSB));
	n += scnprintf(buffer + n, size - n, "MEMIF_MON0 = 0x%x\n",
		       mtk_afe_get_reg(afe_info, AFE_MEMIF_MON0));
	n += scnprintf(buffer + n, size - n, "MEMIF_MON1 = 0x%x\n",
		       mtk_afe_get_reg(afe_info, AFE_MEMIF_MON1));
	n += scnprintf(buffer + n, size - n, "MEMIF_MON2 = 0x%x\n",
		       mtk_afe_get_reg(afe_info, AFE_MEMIF_MON2));
	n += scnprintf(buffer + n, size - n, "MEMIF_MON4 = 0x%x\n",
		       mtk_afe_get_reg(afe_info, AFE_MEMIF_MON4));

	n += scnprintf(buffer + n, size - n, "AFE_ADDA_DL_SRC2_CON0 = 0x%x\n",
		       mtk_afe_get_reg(afe_info, AFE_ADDA_DL_SRC2_CON0));
	n += scnprintf(buffer + n, size - n, "AFE_ADDA_DL_SRC2_CON1 = 0x%x\n",
		       mtk_afe_get_reg(afe_info, AFE_ADDA_DL_SRC2_CON1));
	n += scnprintf(buffer + n, size - n, "AFE_ADDA_UL_SRC_CON0 = 0x%x\n",
		       mtk_afe_get_reg(afe_info, AFE_ADDA_UL_SRC_CON0));
	n += scnprintf(buffer + n, size - n, "AFE_ADDA_UL_SRC_CON1 = 0x%x\n",
		       mtk_afe_get_reg(afe_info, AFE_ADDA_UL_SRC_CON1));
	n += scnprintf(buffer + n, size - n, "AFE_ADDA_TOP_CON0 = 0x%x\n",
		       mtk_afe_get_reg(afe_info, AFE_ADDA_TOP_CON0));
	n += scnprintf(buffer + n, size - n, "AFE_ADDA_UL_DL_CON0 = 0x%x\n",
		       mtk_afe_get_reg(afe_info, AFE_ADDA_UL_DL_CON0));
	n += scnprintf(buffer + n, size - n, "AFE_ADDA_SRC_DEBUG = 0x%x\n",
		       mtk_afe_get_reg(afe_info, AFE_ADDA_SRC_DEBUG));
	n += scnprintf(buffer + n, size - n, "AFE_ADDA_SRC_DEBUG_MON0 = 0x%x\n",
		       mtk_afe_get_reg(afe_info, AFE_ADDA_SRC_DEBUG_MON0));
	n += scnprintf(buffer + n, size - n, "AFE_ADDA_SRC_DEBUG_MON1 = 0x%x\n",
		       mtk_afe_get_reg(afe_info, AFE_ADDA_SRC_DEBUG_MON1));
	n += scnprintf(buffer + n, size - n, "AFE_ADDA_NEWIF_CFG0 = 0x%x\n",
		       mtk_afe_get_reg(afe_info, AFE_ADDA_NEWIF_CFG0));
	n += scnprintf(buffer + n, size - n, "AFE_ADDA_NEWIF_CFG1 = 0x%x\n",
		       mtk_afe_get_reg(afe_info, AFE_ADDA_NEWIF_CFG1));
	n += scnprintf(buffer + n, size - n, "AFE_ADDA2_TOP_CON0 = 0x%x\n",
		       mtk_afe_get_reg(afe_info, AFE_ADDA2_TOP_CON0));
	n += scnprintf(buffer + n, size - n, "AFE_ADDA_PREDIS_CON0 = 0x%x\n",
		       mtk_afe_get_reg(afe_info, AFE_ADDA_PREDIS_CON0));
	n += scnprintf(buffer + n, size - n, "AFE_ADDA_PREDIS_CON1 = 0x%x\n",
		       mtk_afe_get_reg(afe_info, AFE_ADDA_PREDIS_CON1));

	n += scnprintf(buffer + n, size - n, "AFE_IRQ_MCU_CON = 0x%x\n",
		       mtk_afe_get_reg(afe_info, AFE_IRQ_MCU_CON));
	n += scnprintf(buffer + n, size - n, "AFE_IRQ_MCU_STATUS = 0x%x\n",
		       mtk_afe_get_reg(afe_info, AFE_IRQ_STATUS));
	n += scnprintf(buffer + n, size - n, "AFE_IRQ_MCU_CLR = 0x%x\n",
		       mtk_afe_get_reg(afe_info, AFE_IRQ_CLR));
	n += scnprintf(buffer + n, size - n, "AFE_IRQ_MCU_CNT1 = 0x%x\n",
		       mtk_afe_get_reg(afe_info, AFE_IRQ_CNT1));
	n += scnprintf(buffer + n, size - n, "AFE_IRQ_MCU_CNT2 = 0x%x\n",
		       mtk_afe_get_reg(afe_info, AFE_IRQ_CNT2));
	n += scnprintf(buffer + n, size - n, "AFE_IRQ_MCU_EN = 0x%x\n",
		       mtk_afe_get_reg(afe_info, AFE_IRQ_MCU_EN));
	n += scnprintf(buffer + n, size - n, "AFE_IRQ_MCU_MON2 = 0x%x\n",
		       mtk_afe_get_reg(afe_info, AFE_IRQ_MON2));
	n += scnprintf(buffer + n, size - n, "AFE_IRQ_MCU_CNT5 = 0x%x\n",
		       mtk_afe_get_reg(afe_info, AFE_IRQ_CNT5));
	n += scnprintf(buffer + n, size - n, "AFE_IRQ1_MCU_CNT_MON = 0x%x\n",
		       mtk_afe_get_reg(afe_info, AFE_IRQ1_CNT_MON));
	n += scnprintf(buffer + n, size - n, "AFE_IRQ2_MCU_CNT_MON = 0x%x\n",
		       mtk_afe_get_reg(afe_info, AFE_IRQ2_CNT_MON));
	n += scnprintf(buffer + n, size - n, "AFE_IRQ1_MCU_EN_CNT_MON = 0x%x\n",
		       mtk_afe_get_reg(afe_info, AFE_IRQ1_EN_CNT_MON));
	n += scnprintf(buffer + n, size - n, "AFE_IRQ5_MCU_CNT_MON = 0x%x\n",
		       mtk_afe_get_reg(afe_info, AFE_IRQ5_EN_CNT_MON));
	n += scnprintf(buffer + n, size - n, "AFE_MEMIF_MAXLEN = 0x%x\n",
		       mtk_afe_get_reg(afe_info, AFE_MEMIF_MAXLEN));
	n += scnprintf(buffer + n, size - n, "AFE_MEMIF_PBUF_SIZE = 0x%x\n",
		       mtk_afe_get_reg(afe_info, AFE_MEMIF_PBUF_SIZE));

	n += scnprintf(buffer + n, size - n, "SIDETONE_DEBUG = 0x%x\n",
		       mtk_afe_get_reg(afe_info, AFE_SIDETONE_DEBUG));
	n += scnprintf(buffer + n, size - n, "SIDETONE_MON = 0x%x\n",
		       mtk_afe_get_reg(afe_info, AFE_SIDETONE_MON));
	n += scnprintf(buffer + n, size - n, "SIDETONE_CON0 = 0x%x\n",
		       mtk_afe_get_reg(afe_info, AFE_SIDETONE_CON0));
	n += scnprintf(buffer + n, size - n, "SIDETONE_COEFF = 0x%x\n",
		       mtk_afe_get_reg(afe_info, AFE_SIDETONE_COEFF));
	n += scnprintf(buffer + n, size - n, "SIDETONE_CON1 = 0x%x\n",
		       mtk_afe_get_reg(afe_info, AFE_SIDETONE_CON1));
	n += scnprintf(buffer + n, size - n, "SIDETONE_GAIN = 0x%x\n",
		       mtk_afe_get_reg(afe_info, AFE_SIDETONE_GAIN));
	n += scnprintf(buffer + n, size - n, "SGEN_CON0 = 0x%x\n",
		       mtk_afe_get_reg(afe_info, AFE_SGEN_CON0));
	n += scnprintf(buffer + n, size - n, "SGEN_CON1 = 0x%x\n",
		       mtk_afe_get_reg(afe_info, AFE_SGEN_CON1));

	n += scnprintf(buffer + n, size - n, "AFE_GAIN1_CON0 = 0x%x\n",
		       mtk_afe_get_reg(afe_info, AFE_GAIN1_CON0));
	n += scnprintf(buffer + n, size - n, "AFE_GAIN1_CON1 = 0x%x\n",
		       mtk_afe_get_reg(afe_info, AFE_GAIN1_CON1));
	n += scnprintf(buffer + n, size - n, "AFE_GAIN1_CON2 = 0x%x\n",
		       mtk_afe_get_reg(afe_info, AFE_GAIN1_CON2));
	n += scnprintf(buffer + n, size - n, "AFE_GAIN1_CON3 = 0x%x\n",
		       mtk_afe_get_reg(afe_info, AFE_GAIN1_CON3));
	n += scnprintf(buffer + n, size - n, "AFE_GAIN1_CONN = 0x%x\n",
		       mtk_afe_get_reg(afe_info, AFE_GAIN1_CONN));
	n += scnprintf(buffer + n, size - n, "AFE_GAIN1_CUR = 0x%x\n",
		       mtk_afe_get_reg(afe_info, AFE_GAIN1_CUR));
	n += scnprintf(buffer + n, size - n, "AFE_GAIN2_CON0 = 0x%x\n",
		       mtk_afe_get_reg(afe_info, AFE_GAIN2_CON0));
	n += scnprintf(buffer + n, size - n, "AFE_GAIN2_CON1 = 0x%x\n",
		       mtk_afe_get_reg(afe_info, AFE_GAIN2_CON1));
	n += scnprintf(buffer + n, size - n, "AFE_GAIN2_CON2 = 0x%x\n",
		       mtk_afe_get_reg(afe_info, AFE_GAIN2_CON2));
	n += scnprintf(buffer + n, size - n, "AFE_GAIN2_CON3 = 0x%x\n",
		       mtk_afe_get_reg(afe_info, AFE_GAIN2_CON3));
	n += scnprintf(buffer + n, size - n, "AFE_GAIN2_CONN = 0x%x\n",
		       mtk_afe_get_reg(afe_info, AFE_GAIN2_CONN));
	n += scnprintf(buffer + n, size - n, "AFE_GAIN2_CUR = 0x%x\n",
		       mtk_afe_get_reg(afe_info, AFE_GAIN2_CUR));
	n += scnprintf(buffer + n, size - n, "AFE_GAIN2_CONN2 = 0x%x\n",
		       mtk_afe_get_reg(afe_info, AFE_GAIN2_CONN2));
	n += scnprintf(buffer + n, size - n, "AFE_GAIN2_CONN3 = 0x%x\n",
		       mtk_afe_get_reg(afe_info, AFE_GAIN2_CONN3));
	n += scnprintf(buffer + n, size - n, "AFE_GAIN1_CONN2 = 0x%x\n",
		       mtk_afe_get_reg(afe_info, AFE_GAIN1_CONN2));
	n += scnprintf(buffer + n, size - n, "AFE_GAIN1_CONN3 = 0x%x\n",
		       mtk_afe_get_reg(afe_info, AFE_GAIN1_CONN3));
	n += scnprintf(buffer + n, size - n, "DCM_CFG = 0x%x\n",
		       mtk_afe_topck_get_reg(afe_info, AUDIO_DCM_CFG));
	n += scnprintf(buffer + n, size - n, "CLK_CFG_4 = 0x%x\n",
		       mtk_afe_topck_get_reg(afe_info, AUDIO_CLK_CFG_4));
	/* HDMI */
	n += scnprintf(buffer + n, size - n, "AUDIO_CLK_CFG_6 = 0x%x\n",
		       mtk_afe_topck_get_reg(afe_info, AUDIO_CLK_CFG_6));
	n += scnprintf(buffer + n, size - n, "AUDIO_CLK_CFG_7 = 0x%x\n",
		       mtk_afe_topck_get_reg(afe_info, AUDIO_CLK_CFG_7));
	n += scnprintf(buffer + n, size - n, "AUDIO_CLK_AUDDIV_0 = 0x%x\n",
		       mtk_afe_topck_get_reg(afe_info, AUDIO_CLK_AUDDIV_0));
	n += scnprintf(buffer + n, size - n, "AUDIO_CLK_AUDDIV_1 = 0x%x\n",
		       mtk_afe_topck_get_reg(afe_info, AUDIO_CLK_AUDDIV_1));
	n += scnprintf(buffer + n, size - n, "AUDIO_CLK_AUDDIV_2 = 0x%x\n",
		       mtk_afe_topck_get_reg(afe_info, AUDIO_CLK_AUDDIV_2));
	n += scnprintf(buffer + n, size - n, "AUDIO_CLK_AUDDIV_3 = 0x%x\n",
		       mtk_afe_topck_get_reg(afe_info, AUDIO_CLK_AUDDIV_3));

	n += scnprintf(buffer + n, size - n, "AP_PLL_CON5 = 0x%x\n",
		       mtk_afe_pll_get_reg(afe_info, AP_PLL_CON5));
	n += scnprintf(buffer + n, size - n, "APLL1_CON0 = 0x%x\n",
		       mtk_afe_pll_get_reg(afe_info, AUDIO_APLL1_CON0));
	n += scnprintf(buffer + n, size - n, "APLL1_CON1 = 0x%x\n",
		       mtk_afe_pll_get_reg(afe_info, AUDIO_APLL1_CON1));
	n += scnprintf(buffer + n, size - n, "APLL1_CON2 = 0x%x\n",
		       mtk_afe_pll_get_reg(afe_info, AUDIO_APLL1_CON2));
	n += scnprintf(buffer + n, size - n, "APLL1_PWR_CON0 = 0x%x\n",
		       mtk_afe_pll_get_reg(afe_info, AUDIO_APLL1_PWR_CON0));
	n += scnprintf(buffer + n, size - n, "APLL2_CON0 = 0x%x\n",
		       mtk_afe_pll_get_reg(afe_info, AUDIO_APLL2_CON0));
	n += scnprintf(buffer + n, size - n, "APLL2_CON1 = 0x%x\n",
		       mtk_afe_pll_get_reg(afe_info, AUDIO_APLL2_CON1));
	n += scnprintf(buffer + n, size - n, "APLL2_CON2 = 0x%x\n",
		       mtk_afe_pll_get_reg(afe_info, AUDIO_APLL2_CON2));
	n += scnprintf(buffer + n, size - n, "APLL2_PWR_CON0 = 0x%x\n",
		       mtk_afe_pll_get_reg(afe_info, AUDIO_APLL2_PWR_CON0));

	return simple_read_from_buffer(buf, count, pos, buffer, n);
}

static const struct file_operations mt8173_afe_debug_ops = {
	.open = mt8173_afe_debug_open,
	.read = mt8173_afe_debug_read,
};

static int mt8173_afe_set_buf_reg(struct mtk_afe_info *afe_info,
				  struct mtk_afe_memif *memif)
{
	/* start */
	regmap_write(afe_info->regmap,
		     memif->reg_base.addr, memif->block.phys_buf_addr);
	/* end */
	regmap_write(afe_info->regmap,
		     memif->reg_base.addr + AFE_BASE_END_OFFSET,
		     memif->block.phys_buf_addr + memif->block.buffer_size - 1);
	return 0;
}

static int mt8173_afe_set_sram_buf(struct mtk_afe_info *afe_info,
				   struct snd_pcm_substream *substream,
				   struct mtk_afe_memif *memif, size_t size)
{
	struct mtk_afe_block *block = &memif->block;
	struct snd_dma_buffer *dma_buf = &substream->dma_buffer;

	if (size > afe_info->afe_sram_size)
		return -EINVAL;

	/* allocate memory */
	block->buffer_size = size;
	block->phys_buf_addr = afe_info->afe_sram_phy_address;
	block->virt_buf_addr = (unsigned char *)afe_info->afe_sram_address;
	block->write_idx = 0;
	block->dma_read_idx = 0;

	/* set sram address top hardware */
	mt8173_afe_set_buf_reg(afe_info, memif);

	dma_buf->bytes = size;
	dma_buf->area = (unsigned char *)afe_info->afe_sram_address;
	dma_buf->addr = afe_info->afe_sram_phy_address;
	dma_buf->dev.type = SNDRV_DMA_TYPE_DEV;
	dma_buf->dev.dev = substream->pcm->card->dev;
	dma_buf->private_data = NULL;
	snd_pcm_set_runtime_buffer(substream, dma_buf);

	return 0;
}

static int mt8173_afe_set_buf(struct mtk_afe_info *afe_info,
			      struct snd_pcm_substream *substream,
			      struct mtk_afe_memif *memif)
{
	struct snd_pcm_runtime * const runtime = substream->runtime;
	struct mtk_afe_block *block = &memif->block;

	block->phys_buf_addr = runtime->dma_addr;
	block->virt_buf_addr = runtime->dma_area;
	block->buffer_size = runtime->dma_bytes;
	block->write_idx = 0;
	block->dma_read_idx = 0;

	mt8173_afe_set_buf_reg(afe_info, memif);

	return 0;
}

static bool mt8173_afe_get_memif_enable(struct mtk_afe_info *afe_info)
{
	int i;

	for (i = 0; i < MTK_AFE_PIN_NUM; i++) {
		if ((afe_info->pin_status[i]) == true)
			return true;
	}
	return false;
}

static bool mt8173_afe_get_ul_memif_enable(struct mtk_afe_info *afe_info)
{
	int i;

	for (i = MTK_AFE_PIN_VUL; i <= MTK_AFE_PIN_MOD_DAI; i++) {
		if ((afe_info->pin_status[i]) == true)
			return true;
	}
	return false;
}

static void mt8173_afe_enable_afe(struct mtk_afe_info *afe_info, bool enable)
{
	unsigned long flags;
	bool mem_enable = mt8173_afe_get_memif_enable(afe_info);

	spin_lock_irqsave(&mtk_afe_control_lock, flags);

	if (!enable && !mem_enable)
		regmap_update_bits(afe_info->regmap, AFE_DAC_CON0, 1, 0);
	else if (enable && mem_enable)
		regmap_update_bits(afe_info->regmap, AFE_DAC_CON0, 1, 1);

	spin_unlock_irqrestore(&mtk_afe_control_lock, flags);
}

static uint32_t mt8173_afe_i2s_fs(uint32_t sample_rate)
{
	switch (sample_rate) {
	case 8000:
		return MTK_AFE_I2S_RATE_8K;
	case 11025:
		return MTK_AFE_I2S_RATE_11K;
	case 12000:
		return MTK_AFE_I2S_RATE_12K;
	case 16000:
		return MTK_AFE_I2S_RATE_16K;
	case 22050:
		return MTK_AFE_I2S_RATE_22K;
	case 24000:
		return MTK_AFE_I2S_RATE_24K;
	case 32000:
		return MTK_AFE_I2S_RATE_32K;
	case 44100:
		return MTK_AFE_I2S_RATE_44K;
	case 48000:
		return MTK_AFE_I2S_RATE_48K;
	case 88000:
		return MTK_AFE_I2S_RATE_88K;
	case 96000:
		return MTK_AFE_I2S_RATE_96K;
	case 174000:
		return MTK_AFE_I2S_RATE_174K;
	case 192000:
		return MTK_AFE_I2S_RATE_192K;
	default:
		break;
	}
	return MTK_AFE_I2S_RATE_44K;
}

static int mt8173_afe_set_sample_rate(struct mtk_afe_info *afe_info,
				      uint32_t pin, uint32_t sample_rate)
{
	uint32_t fs = mt8173_afe_i2s_fs(sample_rate);
	unsigned int reg = afe_info->memif[pin].reg_fs.addr;
	unsigned int shift = afe_info->memif[pin].reg_fs.shift;
	unsigned int mask = afe_info->memif[pin].reg_fs.mask;

	if (pin != MTK_AFE_PIN_DAI && pin != MTK_AFE_PIN_MOD_DAI) {
		regmap_update_bits(afe_info->regmap, reg, mask, fs << shift);
	} else {
		if (fs == MTK_AFE_I2S_RATE_8K)
			regmap_update_bits(afe_info->regmap,
					   reg, mask, 0 << shift);
		else if (fs == MTK_AFE_I2S_RATE_16K)
			regmap_update_bits(afe_info->regmap,
					   reg, mask, 1 << shift);
		else
			return -EINVAL;
	}
	return 0;
}

static void mt8173_afe_set_i2s_adc(struct mtk_afe_info *afe_info,
				   uint32_t samplerate)
{
	uint32_t sampling_rate = mt8173_afe_i2s_fs(samplerate);
	uint32_t voice_mode = 0;

	/* Use Int ADC */
	regmap_update_bits(afe_info->regmap, AFE_ADDA_TOP_CON0, 1, 0);

	if (sampling_rate == MTK_AFE_I2S_RATE_8K)
		voice_mode = 0;
	else if (sampling_rate == MTK_AFE_I2S_RATE_16K)
		voice_mode = 1;
	else if (sampling_rate == MTK_AFE_I2S_RATE_32K)
		voice_mode = 2;
	else if (sampling_rate == MTK_AFE_I2S_RATE_48K)
		voice_mode = 3;

	regmap_update_bits(afe_info->regmap, AFE_ADDA_UL_SRC_CON0,
			   0x001e0000, ((voice_mode << 2) | voice_mode) << 17);
	regmap_update_bits(afe_info->regmap, AFE_ADDA_NEWIF_CFG1,
			   0x00000c00, ((voice_mode < 3) ? 1 : 3) << 10);
}

static void mt8173_afe_set_i2s_adc_enable(struct mtk_afe_info *afe_info,
					  bool enable)
{
	regmap_update_bits(afe_info->regmap,
			   AFE_ADDA_UL_SRC_CON0, 1, enable ? 1 : 0);
	afe_info->pin_status[MTK_AFE_PIN_I2S_ADC] = enable;
	if (enable)
		regmap_update_bits(afe_info->regmap, AFE_ADDA_UL_DL_CON0, 1, 1);
	else if (!afe_info->pin_status[MTK_AFE_PIN_I2S_DAC] &&
		 !afe_info->pin_status[MTK_AFE_PIN_I2S_ADC])
		regmap_update_bits(afe_info->regmap, AFE_ADDA_UL_DL_CON0, 1, 0);
}

static void mt8173_afe_set_dl_src2(struct mtk_afe_info *afe_info,
				   uint32_t sample_rate)
{
	uint32_t con0, con1;

	if (sample_rate == 44100)
		con0 = 7;
	else if (sample_rate == 8000)
		con0 = 0;
	else if (sample_rate == 11025)
		con0 = 1;
	else if (sample_rate == 12000)
		con0 = 2;
	else if (sample_rate == 16000)
		con0 = 3;
	else if (sample_rate == 22050)
		con0 = 4;
	else if (sample_rate == 24000)
		con0 = 5;
	else if (sample_rate == 32000)
		con0 = 6;
	else if (sample_rate == 48000)
		con0 = 8;
	else
		con0 = 7;	/* Default 44100 */

	con0 = (con0 << 28) | (0x03 << 24) | (0x03 << 11);

	/* 8k or 16k voice mode */
	if (con0 == 0 || con0 == 3)
		con0 |= 0x01 << 5;

	/* SA suggest apply -0.3db to audio/speech path */
	con0 = con0 | (0x01 << 1);
	con1 = 0xf74f0000;

	regmap_write(afe_info->regmap, AFE_ADDA_DL_SRC2_CON0, con0);
	regmap_write(afe_info->regmap, AFE_ADDA_DL_SRC2_CON1, con1);
}

static int mt8173_afe_set_i2s_dac_out(struct mtk_afe_info *afe_info,
				      uint32_t sample_rate)
{
	uint32_t audio_i2s_dac = 0;

	mt8173_afe_set_dl_src2(afe_info, sample_rate);

	audio_i2s_dac |= MTK_AFE_LR_SWAP_NO << 31;
	audio_i2s_dac |= MTK_AFE_NORMAL_CLOCK << 12;
	audio_i2s_dac |= mt8173_afe_i2s_fs(sample_rate) << 8;
	audio_i2s_dac |= MTK_AFE_INV_LRCK_NO << 5;
	audio_i2s_dac |= MTK_AFE_I2S_FORMAT_I2S << 3;
	audio_i2s_dac |= MTK_AFE_I2S_WLEN_16BITS << 1;
	regmap_write(afe_info->regmap, AFE_I2S_CON1, audio_i2s_dac);
	return 0;
}

static int mt8173_afe_set_memif_enable(struct mtk_afe_info *afe_info,
				       uint32_t pin, bool enable)
{
	unsigned long flags;

	if (pin >= MTK_AFE_PIN_NUM)
		return -EINVAL;

	spin_lock_irqsave(&mtk_afe_control_lock, flags);
	afe_info->pin_status[pin] = enable;
	if (pin <= MTK_AFE_PIN_MOD_DAI)
		regmap_update_bits(afe_info->regmap,
				   AFE_DAC_CON0,
				   1 << (pin + 1), enable << (pin + 1));
	spin_unlock_irqrestore(&mtk_afe_control_lock, flags);
	return 0;
}

static int mt8173_afe_set_i2s_dac_enable(struct mtk_afe_info *afe_info,
					 bool enable)
{
	afe_info->pin_status[MTK_AFE_PIN_I2S_DAC] = enable;

	regmap_update_bits(afe_info->regmap, AFE_ADDA_DL_SRC2_CON0, 1, enable);
	regmap_update_bits(afe_info->regmap, AFE_I2S_CON1, 1, enable);

	if (enable || (!enable && !afe_info->pin_status[MTK_AFE_PIN_I2S_ADC]))
		regmap_update_bits(afe_info->regmap,
				   AFE_ADDA_UL_DL_CON0, 1, enable);
	return 0;
}

static int mt8173_afe_set_irq_enable(struct mtk_afe_info *afe_info,
				     struct mtk_afe_memif *memif, bool enable)
{
	unsigned int irq = memif->irq_num;
	unsigned int val;

	if (irq == MTK_AFE_IRQ_MODE_2 && !enable &&
	    mt8173_afe_get_ul_memif_enable(afe_info)) {
		/* IRQ2 is in used */
		regmap_read(afe_info->regmap, AFE_DAC_CON0, &val);
		dev_info(afe_info->dev,
			 "skip disable IRQ2, AFE_DAC_CON0 = 0x%x\n", val);
		return 0;
	}
	regmap_update_bits(afe_info->regmap, memif->reg_irq_en.addr,
			   memif->reg_irq_en.mask,
			   enable << memif->reg_irq_en.shift);
	afe_info->irq_status[irq] = enable;
	return 0;
}

static int mt8173_turn_on_apll22m_clock(struct mtk_afe_info *afe_info)
{
	int ret = clk_prepare_enable(afe_info->clock[MTK_CLK_TOP_AUD1_SEL].clk);

	if (ret) {
		dev_err(afe_info->dev, "%s clk_prepare_enable %s fail %d\n",
			__func__, afe_info->clock[MTK_CLK_TOP_AUD1_SEL].name,
			ret);
		return ret;
	}
	ret = clk_set_parent(afe_info->clock[MTK_CLK_TOP_AUD1_SEL].clk,
			     afe_info->clock[MTK_CLK_TOP_APLL1_CK].clk);
	if (ret) {
		dev_err(afe_info->dev, "%s clk_set_parent %s-%s fail %d\n",
			__func__, afe_info->clock[MTK_CLK_TOP_AUD1_SEL].name,
			afe_info->clock[MTK_CLK_TOP_APLL1_CK].name, ret);
		return ret;
	}

	ret = clk_prepare_enable(afe_info->clock[MTK_CLK_INFRASYS_AUD].clk);
	if (ret) {
		dev_err(afe_info->dev, "%s clk_prepare_enable %s fail %d\n",
			__func__, afe_info->clock[MTK_CLK_INFRASYS_AUD].name,
			ret);
		return ret;
	}

	ret = clk_prepare_enable(afe_info->clock[MTK_CLK_APLL22M].clk);
	if (ret)
		dev_err(afe_info->dev, "%s clk_prepare_enable %s fail %d\n",
			__func__, afe_info->clock[MTK_CLK_APLL22M].name, ret);
	return ret;
}

static int mt8173_turn_off_apll22m_clock(struct mtk_afe_info *afe_info)
{
	int ret;

	clk_disable_unprepare(afe_info->clock[MTK_CLK_APLL22M].clk);
	clk_disable_unprepare(afe_info->clock[MTK_CLK_INFRASYS_AUD].clk);

	ret = clk_set_parent(afe_info->clock[MTK_CLK_TOP_AUD1_SEL].clk,
			     afe_info->clock[MTK_CLK_CLK26M].clk);
	if (ret) {
		dev_err(afe_info->dev, "%s clk_set_parent %s-%s fail %d\n",
			__func__, afe_info->clock[MTK_CLK_TOP_AUD1_SEL].name,
			afe_info->clock[MTK_CLK_CLK26M].name, ret);
		return ret;
	}

	clk_disable_unprepare(afe_info->clock[MTK_CLK_TOP_AUD1_SEL].clk);
	return ret;
}

static int mt8173_turn_on_apll24m_clock(struct mtk_afe_info *afe_info)
{
	int ret = clk_prepare_enable(afe_info->clock[MTK_CLK_TOP_AUD2_SEL].clk);

	if (ret) {
		dev_err(afe_info->dev, "%s clk_prepare_enable %s fail %d\n",
			__func__, afe_info->clock[MTK_CLK_TOP_AUD2_SEL].name,
			ret);
		return ret;
	}

	ret = clk_set_parent(afe_info->clock[MTK_CLK_TOP_AUD2_SEL].clk,
			     afe_info->clock[MTK_CLK_TOP_APLL2_CK].clk);
	if (ret) {
		dev_err(afe_info->dev, "%s clk_set_parent %s-%s fail %d\n",
			__func__, afe_info->clock[MTK_CLK_TOP_AUD2_SEL].name,
			afe_info->clock[MTK_CLK_TOP_APLL2_CK].name, ret);
		return ret;
	}

	ret = clk_prepare_enable(afe_info->clock[MTK_CLK_INFRASYS_AUD].clk);
	if (ret) {
		dev_err(afe_info->dev, "%s clk_prepare_enable %s fail %d\n",
			__func__, afe_info->clock[MTK_CLK_INFRASYS_AUD].name,
			ret);
		return ret;
	}

	ret = clk_prepare_enable(afe_info->clock[MTK_CLK_APLL24M].clk);
	if (ret)
		dev_err(afe_info->dev, "%s clk_prepare_enable %s fail %d\n",
			__func__, afe_info->clock[MTK_CLK_APLL24M].name, ret);
	return ret;
}

static int mt8173_turn_off_apll24m_clock(struct mtk_afe_info *afe_info)
{
	int ret;

	clk_disable_unprepare(afe_info->clock[MTK_CLK_APLL24M].clk);
	clk_disable_unprepare(afe_info->clock[MTK_CLK_INFRASYS_AUD].clk);

	ret = clk_set_parent(afe_info->clock[MTK_CLK_TOP_AUD2_SEL].clk,
			     afe_info->clock[MTK_CLK_CLK26M].clk);
	if (ret) {
		dev_err(afe_info->dev, "%s clk_set_parent %s-%s fail %d\n",
			__func__, afe_info->clock[MTK_CLK_TOP_AUD2_SEL].name,
		       afe_info->clock[MTK_CLK_CLK26M].name, ret);
		return ret;
	}

	clk_disable_unprepare(afe_info->clock[MTK_CLK_TOP_AUD2_SEL].clk);
	return ret;
}

static int mt8173_turn_on_apll1_tuner_clock(struct mtk_afe_info *afe_info)
{
	int ret = clk_prepare_enable(afe_info->clock[MTK_CLK_INFRASYS_AUD].clk);

	if (ret) {
		dev_err(afe_info->dev, "%s clk_prepare_enable %s fail %d\n",
			__func__, afe_info->clock[MTK_CLK_INFRASYS_AUD].name,
			ret);
		return ret;
	}

	ret = clk_prepare_enable(afe_info->clock[MTK_CLK_APLL1_TUNER].clk);
	if (ret) {
		dev_err(afe_info->dev, "%s clk_prepare_enable %s fail %d\n",
			__func__, afe_info->clock[MTK_CLK_APLL1_TUNER].name,
			ret);
		return ret;
	}

	regmap_update_bits(afe_info->regmap, AFE_APLL1_TUNER_CFG,
			   0x0000FFF7, 0x00008033);
	mtk_afe_pll_set_reg(afe_info, AP_PLL_CON5, 1 << 0, 1 << 0);
	return ret;
}

static int mt8173_turn_off_apll1_tuner_clock(struct mtk_afe_info *afe_info)
{
	clk_disable_unprepare(afe_info->clock[MTK_CLK_APLL1_TUNER].clk);
	clk_disable_unprepare(afe_info->clock[MTK_CLK_INFRASYS_AUD].clk);
	mtk_afe_pll_set_reg(afe_info, AP_PLL_CON5, 0 << 0, 1 << 0);
	return 0;
}

static int mt8173_turn_on_apll2_tuner_clock(struct mtk_afe_info *afe_info)
{
	int ret = clk_prepare_enable(afe_info->clock[MTK_CLK_INFRASYS_AUD].clk);

	if (ret) {
		dev_err(afe_info->dev, "%s clk_prepare_enable %s fail %d\n",
			__func__, afe_info->clock[MTK_CLK_INFRASYS_AUD].name,
			ret);
		return ret;
	}

	ret = clk_prepare_enable(afe_info->clock[MTK_CLK_APLL2_TUNER].clk);
	if (ret) {
		dev_err(afe_info->dev, "%s clk_prepare_enable %s fail %d\n",
			__func__, afe_info->clock[MTK_CLK_APLL2_TUNER].name,
			ret);
		return ret;
	}

	regmap_update_bits(afe_info->regmap, AFE_APLL2_TUNER_CFG,
			   0x0000FFF7, 0x00008035);
	mtk_afe_pll_set_reg(afe_info, AP_PLL_CON5, 1 << 1, 1 << 1);
	return ret;
}

static int mt8173_turn_off_apll2_tuner_clock(struct mtk_afe_info *afe_info)
{
	clk_disable_unprepare(afe_info->clock[MTK_CLK_APLL2_TUNER].clk);
	clk_disable_unprepare(afe_info->clock[MTK_CLK_INFRASYS_AUD].clk);
	mtk_afe_pll_set_reg(afe_info, AP_PLL_CON5, 0 << 1, 1 << 1);
	return 0;
}

static uint32_t mt8173_get_apll_by_sample_rate(uint32_t rate)
{
	if (rate == 176400 || rate == 88200 || rate == 44100 ||
	    rate == 22050 || rate == 11025)
		return MTK_AFE_APLL1;
	else
		return MTK_AFE_APLL2;
}

static int mt8173_afe_enable_apll(struct mtk_afe_info *afe_info,
				  uint32_t rate, bool enable)
{
	int ret;

	if (MTK_AFE_APLL1 == (mt8173_get_apll_by_sample_rate(rate))) {
		if (enable)
			ret = mt8173_turn_on_apll22m_clock(afe_info);
		else
			ret = mt8173_turn_off_apll22m_clock(afe_info);
	} else {
		if (enable)
			ret = mt8173_turn_on_apll24m_clock(afe_info);
		else
			ret = mt8173_turn_off_apll24m_clock(afe_info);
	}
	return ret;
}

static int mt8173_afe_enable_apll_tuner(struct mtk_afe_info *afe_info,
					uint32_t rate, bool enable)
{
	int ret;

	if (MTK_AFE_APLL1 == (mt8173_get_apll_by_sample_rate(rate))) {
		if (enable)
			ret = mt8173_turn_on_apll1_tuner_clock(afe_info);
		else
			ret = mt8173_turn_off_apll1_tuner_clock(afe_info);
	} else {
		if (enable)
			ret = mt8173_turn_on_apll2_tuner_clock(afe_info);
		else
			ret = mt8173_turn_off_apll2_tuner_clock(afe_info);
	}
	return ret;
}

static uint32_t mt8173_afe_set_clk_mclk(struct mtk_afe_info *afe_info,
					uint32_t clock_type, uint32_t rate)
{
	uint32_t apll_type = mt8173_get_apll_by_sample_rate(rate);
	uint32_t apll_clock;
	uint32_t mclk_div = 0;

	if (apll_type == MTK_AFE_APLL1)
		apll_clock = MTK_AFE_APLL1_CLK_FREQ;
	else
		apll_clock = MTK_AFE_APLL2_CLK_FREQ;

	/* set up mclk mux select / ck div */
	switch (clock_type) {
	case MTK_AFE_ENGEN:
		mclk_div = 7;
		if (apll_type == MTK_AFE_APLL1)
			mtk_afe_topck_set_reg(afe_info, AUDIO_CLK_AUDDIV_0,
					      mclk_div << 24, 0x0f000000);
		else
			mtk_afe_topck_set_reg(afe_info, AUDIO_CLK_AUDDIV_0,
					      mclk_div << 28, 0xf0000000);
		break;
	case MTK_AFE_I2S0:
		mclk_div = (apll_clock / 256 / rate) - 1;
		if (apll_type == MTK_AFE_APLL1) {
			mtk_afe_topck_set_reg(afe_info, AUDIO_CLK_AUDDIV_0,
					      0 << 4, 1 << 4);
			mtk_afe_topck_set_reg(afe_info, AUDIO_CLK_AUDDIV_1,
					      mclk_div, 0x000000ff);
		} else {
			mtk_afe_topck_set_reg(afe_info, AUDIO_CLK_AUDDIV_0,
					      1 << 4, 1 << 4);
			mtk_afe_topck_set_reg(afe_info, AUDIO_CLK_AUDDIV_2,
					      mclk_div, 0x000000ff);
		}
		break;
	case MTK_AFE_I2S1:
		mclk_div = (apll_clock / 256 / rate) - 1;
		if (apll_type == MTK_AFE_APLL1) {
			mtk_afe_topck_set_reg(afe_info, AUDIO_CLK_AUDDIV_0,
					      0 << 5, 1 << 5);
			mtk_afe_topck_set_reg(afe_info, AUDIO_CLK_AUDDIV_1,
					      mclk_div << 8, 0x0000ff00);
		} else {
			mtk_afe_topck_set_reg(afe_info, AUDIO_CLK_AUDDIV_0,
					      1 << 5, 1 << 5);
			mtk_afe_topck_set_reg(afe_info, AUDIO_CLK_AUDDIV_2,
					      mclk_div << 8, 0x0000ff00);
		}
		break;
	case MTK_AFE_I2S2:
		mclk_div = (apll_clock / 256 / rate) - 1;
		if (apll_type == MTK_AFE_APLL1) {
			mtk_afe_topck_set_reg(afe_info, AUDIO_CLK_AUDDIV_0,
					      0 << 6, 1 << 6);
			mtk_afe_topck_set_reg(afe_info, AUDIO_CLK_AUDDIV_1,
					      mclk_div << 16, 0x00ff0000);
		} else {
			mtk_afe_topck_set_reg(afe_info, AUDIO_CLK_AUDDIV_0,
					      1 << 6, 1 << 6);
			mtk_afe_topck_set_reg(afe_info, AUDIO_CLK_AUDDIV_2,
					      mclk_div << 16, 0x00ff0000);
		}
		break;
	case MTK_AFE_I2S3:
		mclk_div = (apll_clock / 256 / rate) - 1;
		if (apll_type == MTK_AFE_APLL1) {
			mtk_afe_topck_set_reg(afe_info, AUDIO_CLK_AUDDIV_0,
					      0 << 7, 1 << 7);
			mtk_afe_topck_set_reg(afe_info, AUDIO_CLK_AUDDIV_1,
					      mclk_div << 24, 0xff000000);
		} else {
			mtk_afe_topck_set_reg(afe_info, AUDIO_CLK_AUDDIV_0,
					      1 << 7, 1 << 7);
			mtk_afe_topck_set_reg(afe_info, AUDIO_CLK_AUDDIV_2,
					      mclk_div << 24, 0xff000000);
		}
		break;
	case MTK_AFE_SPDIF:
		mclk_div = (apll_clock / 128 / rate) - 1;
		if (apll_type == MTK_AFE_APLL1) {
			mtk_afe_topck_set_reg(afe_info, AUDIO_CLK_AUDDIV_0,
					      0 << 9, 1 << 9);
			mtk_afe_topck_set_reg(afe_info, AUDIO_CLK_AUDDIV_3,
					      mclk_div << 24, 0xff000000);
		} else {
			mtk_afe_topck_set_reg(afe_info, AUDIO_CLK_AUDDIV_0,
					      1 << 9, 1 << 9);
			mtk_afe_topck_set_reg(afe_info, AUDIO_CLK_AUDDIV_3,
					      mclk_div << 24, 0xff000000);
		}
		break;
	case MTK_AFE_SPDIF2:
		mclk_div = (apll_clock / 128 / rate) - 1;
		if (apll_type == MTK_AFE_APLL1) {
			mtk_afe_topck_set_reg(afe_info, AUDIO_CLK_AUDDIV_4,
					      0 << 8, 1 << 8);
			mtk_afe_topck_set_reg(afe_info, AUDIO_CLK_AUDDIV_4,
					      mclk_div, 0x000000ff);
		} else {
			mtk_afe_topck_set_reg(afe_info, AUDIO_CLK_AUDDIV_4,
					      1 << 8, 1 << 8);
			mtk_afe_topck_set_reg(afe_info, AUDIO_CLK_AUDDIV_4,
					      mclk_div, 0x000000ff);
		}
		break;
	default:
		break;
	}

	return mclk_div;
}

static void mt8173_afe_set_clk_i2s3_bclk(struct mtk_afe_info *afe_info,
					 uint32_t mck_div, uint32_t rate,
					 uint32_t ch, uint32_t bits)
{
	uint32_t apll_type = mt8173_get_apll_by_sample_rate(rate);
	uint32_t bck = rate * ch * bits;
	uint32_t bck_div;

	if (apll_type == MTK_AFE_APLL1) {
		mtk_afe_topck_set_reg(afe_info, AUDIO_CLK_AUDDIV_0,
				      0 << 8, 1 << 8);
		bck_div = ((MTK_AFE_APLL1_CLK_FREQ / (mck_div + 1)) / bck) - 1;
		mtk_afe_topck_set_reg(afe_info, AUDIO_CLK_AUDDIV_3,
				      bck_div, 0x0000000f);
	} else {
		mtk_afe_topck_set_reg(afe_info, AUDIO_CLK_AUDDIV_0,
				      1 << 8, 1 << 8);
		bck_div = ((MTK_AFE_APLL2_CLK_FREQ / (mck_div + 1)) / bck) - 1;
		mtk_afe_topck_set_reg(afe_info, AUDIO_CLK_AUDDIV_3,
				      bck_div << 4, 0x000000f0);
	}
}

static void mt8173_enable_i2s_div_pwr(struct mtk_afe_info *afe_info,
				      uint32_t diveder_name, bool enable)
{
	if (enable)
		mtk_afe_topck_set_reg(afe_info, AUDIO_CLK_AUDDIV_3,
				      0 << diveder_name, 1 << diveder_name);
	else
		mtk_afe_topck_set_reg(afe_info, AUDIO_CLK_AUDDIV_3,
				      1 << diveder_name, 1 << diveder_name);
}

static void mt8173_afe_enable_apll_div_pwr(struct mtk_afe_info *afe_info,
					   uint32_t clock_type,
					   uint32_t sample_rate, bool enable)
{
	uint32_t apll_type = mt8173_get_apll_by_sample_rate(sample_rate);
	static int apll_clk_div_pwr_cnt[MTK_AFE_APLL_CLOCK_TYPE_NUM] = { 0 };

	if (enable) {
		apll_clk_div_pwr_cnt[clock_type]++;
		if (apll_clk_div_pwr_cnt[clock_type] > 1)
			return;
	} else {
		apll_clk_div_pwr_cnt[clock_type]--;
		if (apll_clk_div_pwr_cnt[clock_type] > 0) {
			return;
		} else if (apll_clk_div_pwr_cnt[clock_type] < 0) {
			dev_warn(afe_info->dev, "%s unexpected count(%u,%d)\n",
				 __func__, clock_type,
				 apll_clk_div_pwr_cnt[clock_type]);
			apll_clk_div_pwr_cnt[clock_type] = 0;
			return;
		}
	}

	switch (clock_type) {
	case MTK_AFE_ENGEN:
		if (apll_type == MTK_AFE_APLL1)
			mt8173_enable_i2s_div_pwr(afe_info,
						  MTK_AFE_APLL1_DIV0, enable);
		else
			mt8173_enable_i2s_div_pwr(afe_info,
						  MTK_AFE_APLL2_DIV0, enable);
		break;
	case MTK_AFE_I2S0:
		if (apll_type == MTK_AFE_APLL1)
			mt8173_enable_i2s_div_pwr(afe_info,
						  MTK_AFE_APLL1_DIV1, enable);
		else
			mt8173_enable_i2s_div_pwr(afe_info,
						  MTK_AFE_APLL2_DIV1, enable);
		break;
	case MTK_AFE_I2S1:
		if (apll_type == MTK_AFE_APLL1)
			mt8173_enable_i2s_div_pwr(afe_info,
						  MTK_AFE_APLL1_DIV2, enable);
		else
			mt8173_enable_i2s_div_pwr(afe_info,
						  MTK_AFE_APLL2_DIV2, enable);
		break;
	case MTK_AFE_I2S2:
		if (apll_type == MTK_AFE_APLL1)
			mt8173_enable_i2s_div_pwr(afe_info,
						  MTK_AFE_APLL1_DIV3, enable);
		else
			mt8173_enable_i2s_div_pwr(afe_info,
						  MTK_AFE_APLL2_DIV3, enable);
		break;
	case MTK_AFE_I2S3:
		if (apll_type == MTK_AFE_APLL1)
			mt8173_enable_i2s_div_pwr(afe_info,
						  MTK_AFE_APLL1_DIV4, enable);
		else
			mt8173_enable_i2s_div_pwr(afe_info,
						  MTK_AFE_APLL2_DIV4, enable);
		break;
	case MTK_AFE_I2S3_BCK:
		if (apll_type == MTK_AFE_APLL1)
			mt8173_enable_i2s_div_pwr(afe_info,
						  MTK_AFE_APLL1_DIV5, enable);
		else
			mt8173_enable_i2s_div_pwr(afe_info,
						  MTK_AFE_APLL2_DIV5, enable);
		break;
	case MTK_AFE_SPDIF:
		mt8173_enable_i2s_div_pwr(afe_info, MTK_AFE_SPDIF_DIV, enable);
		break;
	case MTK_AFE_SPDIF2:
		mt8173_enable_i2s_div_pwr(afe_info, MTK_AFE_SPDIF2_DIV, enable);
		break;
	default:
		break;
	}
}

static void mt8173_afe_set_hdmi_tdm2_config(struct mtk_afe_info *afe_info,
					    unsigned int channels)
{
	unsigned int register_value = 0;

	switch (channels) {
	case 7:
	case 8:
		register_value |= MTK_AFE_TDM_CH_START_O30_O31;
		register_value |= (MTK_AFE_TDM_CH_START_O32_O33 << 4);
		register_value |= (MTK_AFE_TDM_CH_START_O34_O35 << 8);
		register_value |= (MTK_AFE_TDM_CH_START_O36_O37 << 12);
		break;
	case 5:
	case 6:
		register_value |= MTK_AFE_TDM_CH_START_O30_O31;
		register_value |= (MTK_AFE_TDM_CH_START_O32_O33 << 4);
		register_value |= (MTK_AFE_TDM_CH_START_O34_O35 << 8);
		register_value |= (MTK_AFE_TDM_CH_ZERO << 12);
		break;
	case 3:
	case 4:
		register_value |= MTK_AFE_TDM_CH_START_O30_O31;
		register_value |= (MTK_AFE_TDM_CH_START_O32_O33 << 4);
		register_value |= (MTK_AFE_TDM_CH_ZERO << 8);
		register_value |= (MTK_AFE_TDM_CH_ZERO << 12);
		break;
	case 1:
	case 2:
		register_value |= MTK_AFE_TDM_CH_START_O30_O31;
		register_value |= (MTK_AFE_TDM_CH_ZERO << 4);
		register_value |= (MTK_AFE_TDM_CH_ZERO << 8);
		register_value |= (MTK_AFE_TDM_CH_ZERO << 12);
		break;
	default:
		return;
	}

	regmap_update_bits(afe_info->regmap, AFE_TDM_CON2,
			   0x0000FFFF, register_value);
}

static int mt8173_afe_set_i2s_asrc_config(struct mtk_afe_info *afe_info,
					  unsigned int sample_rate)
{
	switch (sample_rate) {
	case 44100:
		regmap_write(afe_info->regmap, AFE_ASRC_CON13, 0x0);
		regmap_write(afe_info->regmap, AFE_ASRC_CON14, 0x1B9000);
		regmap_write(afe_info->regmap, AFE_ASRC_CON15, 0x1B9000);
		regmap_write(afe_info->regmap, AFE_ASRC_CON16, 0x3F5987);
		regmap_write(afe_info->regmap, AFE_ASRC_CON17, 0x1FBD);
		regmap_write(afe_info->regmap, AFE_ASRC_CON20, 0x9C00);
		regmap_write(afe_info->regmap, AFE_ASRC_CON21, 0x8B00);
		regmap_write(afe_info->regmap, AFE_ASRC_CON0, 0x71);
		break;
	case 48000:
		regmap_write(afe_info->regmap, AFE_ASRC_CON13, 0x0);
		regmap_write(afe_info->regmap, AFE_ASRC_CON14, 0x1E0000);
		regmap_write(afe_info->regmap, AFE_ASRC_CON15, 0x1E0000);
		regmap_write(afe_info->regmap, AFE_ASRC_CON16, 0x3F5987);
		regmap_write(afe_info->regmap, AFE_ASRC_CON17, 0x1FBD);
		regmap_write(afe_info->regmap, AFE_ASRC_CON20, 0x8F00);
		regmap_write(afe_info->regmap, AFE_ASRC_CON21, 0x7F00);
		regmap_write(afe_info->regmap, AFE_ASRC_CON0, 0x71);
		break;
	case 32000:
		regmap_write(afe_info->regmap, AFE_ASRC_CON13, 0x0);
		regmap_write(afe_info->regmap, AFE_ASRC_CON14, 0x140000);
		regmap_write(afe_info->regmap, AFE_ASRC_CON15, 0x140000);
		regmap_write(afe_info->regmap, AFE_ASRC_CON16, 0x3F5987);
		regmap_write(afe_info->regmap, AFE_ASRC_CON17, 0x1FBD);
		regmap_write(afe_info->regmap, AFE_ASRC_CON20, 0xD800);
		regmap_write(afe_info->regmap, AFE_ASRC_CON21, 0xBD00);
		regmap_write(afe_info->regmap, AFE_ASRC_CON0, 0x71);
		break;
	default:
		dev_err(afe_info->dev, "%s sample rate %u not handled\n",
			__func__, sample_rate);
		return -EINVAL;
	}
	return 0;
}

static int mt8173_afe_set_2nd_i2s(struct mtk_afe_info *afe_info, uint32_t wlen,
				  uint32_t rate, uint32_t bck_inv,
				  uint32_t mode)
{
	uint32_t audio_i2s = 0;

	if (afe_info->pin_status[MTK_AFE_PIN_I2S_O2] ||
	    afe_info->pin_status[MTK_AFE_PIN_I2S_I2])
		return 0;

	/* set 2nd rate to AFE_ADC_CON1 */
	mt8173_afe_set_sample_rate(afe_info, MTK_AFE_PIN_I2S, rate);
	audio_i2s |= 1 << 31;	/* enable phase_shift_fix for better quality */
	audio_i2s |= bck_inv << 29;
	audio_i2s |= MTK_AFE_FROM_IO_MUX << 28;
	audio_i2s |= MTK_AFE_LOW_JITTER_CLOCK << 12;
	audio_i2s |= MTK_AFE_INV_LRCK_NO << 5;
	audio_i2s |= MTK_AFE_I2S_FORMAT_I2S << 3;
	audio_i2s |= mode << 2;
	audio_i2s |= wlen << 1;
	regmap_update_bits(afe_info->regmap,
			   AFE_I2S_CON, 0xfffffffe, audio_i2s);

	/* set output */
	audio_i2s = 0;
	audio_i2s |= (MTK_AFE_LR_SWAP_NO << 31);
	audio_i2s |= (MTK_AFE_LOW_JITTER_CLOCK << 12);
	audio_i2s |= (mt8173_afe_i2s_fs(rate) << 8);
	audio_i2s |= (MTK_AFE_INV_LRCK_NO << 5);
	audio_i2s |= (MTK_AFE_I2S_FORMAT_I2S << 3);
	audio_i2s |= (wlen << 1);
	regmap_update_bits(afe_info->regmap,
			   AFE_I2S_CON3, 0xfffffffe, audio_i2s);
	return 0;
}

static void mt8173_afe_set_2nd_i2s_enable(struct mtk_afe_info *afe_info,
					  enum mtk_afe_i2s_dir dir_in,
					  uint32_t rate, bool enable)
{
	if (afe_info->pin_status[MTK_AFE_PIN_I2S_O2 + dir_in] == enable)
		return;
	afe_info->pin_status[MTK_AFE_PIN_I2S_O2 + dir_in] = enable;

	/* if another is enabled already? */
	if (afe_info->pin_status[MTK_AFE_PIN_I2S_O2 + (dir_in ? 0 : 1)])
		return;

	/* low jitter I2S clock */
	if (enable) {
		mt8173_afe_enable_apll(afe_info, rate, enable);
		mt8173_afe_enable_apll_tuner(afe_info, rate, enable);
		mt8173_afe_set_clk_mclk(afe_info, MTK_AFE_I2S0, rate);
		mt8173_afe_set_clk_mclk(afe_info, MTK_AFE_ENGEN, rate);
		mt8173_afe_enable_apll_div_pwr(afe_info, MTK_AFE_I2S0, rate,
					       enable);
		mt8173_afe_enable_apll_div_pwr(afe_info, MTK_AFE_ENGEN, rate,
					       enable);
	}

	/* 2nd I2S i2s soft reset begin */
	regmap_update_bits(afe_info->regmap, AUDIO_TOP_CON1, 2, 2);

	/* input */
	regmap_update_bits(afe_info->regmap, AFE_I2S_CON, 1, enable);

	/* output */
	regmap_update_bits(afe_info->regmap, AFE_I2S_CON3, 1, enable);

	/* 2nd I2S i2s soft reset end */
	udelay(1);
	regmap_update_bits(afe_info->regmap, AUDIO_TOP_CON1, 2, 0);

	/* low jitter I2S clock */
	if (!enable) {
		mt8173_afe_enable_apll_div_pwr(afe_info, MTK_AFE_I2S0, rate,
					       enable);
		mt8173_afe_enable_apll_div_pwr(afe_info, MTK_AFE_ENGEN, rate,
					       enable);
		mt8173_afe_enable_apll_tuner(afe_info, rate, enable);
		mt8173_afe_enable_apll(afe_info, rate, enable);
	}
}

static int mt8173_hdmi_loopback_get(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *comp = snd_kcontrol_chip(kcontrol);
	struct mtk_afe_info *afe_info = snd_soc_component_get_drvdata(comp);

	ucontrol->value.integer.value[0] = afe_info->hdmi_loop_type;
	return 0;
}

static int mt8173_hdmi_loopback_set(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_platform *platform = snd_kcontrol_chip(kcontrol);
	struct mtk_afe_info *afe_info = snd_soc_platform_get_drvdata(platform);
	unsigned int type = ucontrol->value.integer.value[0];

	if (afe_info->hdmi_loop_type == type)
		return 0;

	if (afe_info->hdmi_loop_type != MTK_AFE_HDMI_LOOP_NONE) {
		/* FIXME to test in real HDMI */
		mt8173_afe_set_2nd_i2s_enable(afe_info, MTK_AFE_I2S_DIR_IN,
					      afe_info->hdmi_fs, false);
		/* disable tdm_i2s_loopback */
		regmap_update_bits(afe_info->regmap, AFE_TDM_CON2,
				   1 << 20, 0 << 20);

		if (afe_info->codec_if != MTK_AFE_IF_2ND_I2S) {
			mtk_afe_interconn_disconnect(afe_info,
						     MTK_AFE_INTERCONN_I00,
						     MTK_AFE_INTERCONN_O03);
			mtk_afe_interconn_disconnect(afe_info,
						     MTK_AFE_INTERCONN_I01,
						     MTK_AFE_INTERCONN_O04);
			mt8173_afe_set_i2s_dac_enable(afe_info, false);
		} else {
			mtk_afe_interconn_disconnect(afe_info,
						     MTK_AFE_INTERCONN_I00,
						     MTK_AFE_INTERCONN_O00);
			mtk_afe_interconn_disconnect(afe_info,
						     MTK_AFE_INTERCONN_I01,
						     MTK_AFE_INTERCONN_O01);
			mt8173_afe_set_2nd_i2s_enable(afe_info,
						      MTK_AFE_I2S_DIR_OUT,
						      afe_info->hdmi_fs, false);
		}
	}

	afe_info->hdmi_loop_type = type;
	if (type == MTK_AFE_HDMI_LOOP_NONE)
		return 0;

	if (afe_info->codec_if == MTK_AFE_IF_MTK) {
		mtk_afe_interconn_connect(afe_info,
					  MTK_AFE_INTERCONN_I00,
					  MTK_AFE_INTERCONN_O03);
		mtk_afe_interconn_connect(afe_info,
					  MTK_AFE_INTERCONN_I01,
					  MTK_AFE_INTERCONN_O04);
		mt8173_afe_set_i2s_asrc_config(afe_info, afe_info->hdmi_fs);
		mt8173_afe_set_2nd_i2s(afe_info, MTK_AFE_I2S_WLEN_32BITS,
				       afe_info->hdmi_fs, MTK_AFE_BCK_INV_YES,
				       MTK_AFE_I2S_SRC_SLAVE);

	} else {
		mtk_afe_interconn_connect(afe_info,
					  MTK_AFE_INTERCONN_I00,
					  MTK_AFE_INTERCONN_O00);
		mtk_afe_interconn_connect(afe_info,
					  MTK_AFE_INTERCONN_I01,
					  MTK_AFE_INTERCONN_O01);
		/* FIXME: to check master */
		mt8173_afe_set_2nd_i2s(afe_info, MTK_AFE_I2S_WLEN_32BITS,
				       afe_info->hdmi_fs, MTK_AFE_BCK_INV_YES,
				       MTK_AFE_I2S_SRC_MASTER);
	}

	mt8173_afe_set_2nd_i2s_enable(afe_info, MTK_AFE_I2S_DIR_IN,
				      afe_info->hdmi_fs, true);

	regmap_update_bits(afe_info->regmap, AFE_TDM_CON2, 1 << 21,
			   (type - MTK_AFE_HDMI_LOOP_SDAT0) << 21);

	/* enable tdm_i2s_loopback */
	regmap_update_bits(afe_info->regmap, AFE_TDM_CON2, 1 << 20, 1 << 20);

	/* enable sound output to codec */
	if (afe_info->codec_if != MTK_AFE_IF_2ND_I2S) {
		mt8173_afe_set_i2s_dac_out(afe_info, afe_info->hdmi_fs);
		mt8173_afe_set_i2s_dac_enable(afe_info, true);
	} else {
		mt8173_afe_set_2nd_i2s_enable(afe_info, MTK_AFE_I2S_DIR_OUT,
					      afe_info->hdmi_fs, true);
	}
	return 0;
}

static const char * const mt8173_hdmi_loopback_func[] = {"off", "sda0", "sda1",
							 "sda2", "sda3"};
static const struct soc_enum mt8173__pcm_hdmi_control_enum[] = {
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(mt8173_hdmi_loopback_func),
			    mt8173_hdmi_loopback_func),
};

static const struct snd_kcontrol_new mt8173__pcm_hdmi_controls[] = {
	SOC_ENUM_EXT("HDMI_Loopback_Select", mt8173__pcm_hdmi_control_enum[0],
		     mt8173_hdmi_loopback_get,
		     mt8173_hdmi_loopback_set),
};

static int mt8173_afe_pcm_probe(struct snd_soc_platform *platform)
{
	struct mt_afe_info *afe_info = snd_soc_platform_get_drvdata(platform);

	snd_soc_add_platform_controls(platform, mt8173__pcm_hdmi_controls,
				      ARRAY_SIZE(mt8173__pcm_hdmi_controls));
	mt8173_afe_debugfs =
		debugfs_create_file("afe_reg", S_IFREG | S_IRUGO,
				    NULL,
				    (void *)afe_info,
				    &mt8173_afe_debug_ops);
	return 0;
}

static int mt8173_afe_pcm_remove(struct snd_soc_platform *platform)
{
	debugfs_remove(mt8173_afe_debugfs);
	return 0;
}

/* mt8173_afe_pcm_ops */
static int mt8173_afe_pcm_close(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct mtk_afe_info *afe_info =
		snd_soc_platform_get_drvdata(rtd->platform);

	afe_info->memif[dai_map[rtd->cpu_dai->id].pin].sub_stream = NULL;

	/* FIXME todo: enable deep idle for dram case */
	return 0;
}

static snd_pcm_uframes_t mt8173_afe_pcm_pointer
			 (struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct mtk_afe_info *afe_info =
		snd_soc_platform_get_drvdata(rtd->platform);
	uint32_t pin = dai_map[rtd->cpu_dai->id].pin;
	struct mtk_afe_block *block = &afe_info->memif[pin].block;

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		return bytes_to_frames(substream->runtime, block->dma_read_idx);
	else
		return bytes_to_frames(substream->runtime, block->write_idx);
}

static int mt8173_afe_pcm_hw_params(struct snd_pcm_substream *substream,
				    struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct mtk_afe_info *afe_info =
		snd_soc_platform_get_drvdata(rtd->platform);
	uint32_t pin = dai_map[rtd->cpu_dai->id].pin;
	struct mtk_afe_memif *memif = &afe_info->memif[pin];
	int ret;

	dev_dbg(afe_info->dev,
		"%s period = %u,rate= %u, channels=%u\n",
		__func__, params_period_size(params), params_rate(params),
		params_channels(params));

	if (memif->use_sram) {
		/* here to allcoate sram to hardware */
		ret = mt8173_afe_set_sram_buf(afe_info, substream, memif,
					      params_buffer_bytes(params));
	} else {
		ret = snd_pcm_lib_malloc_pages(substream,
					       params_buffer_bytes(params));

		if (ret >= 0)
			mt8173_afe_set_buf(afe_info, substream, memif);
		else
			dev_err(afe_info->dev,
				"%s snd_pcm_lib_malloc_pages fail %d\n",
				__func__, ret);
	}
	return ret;
}

static int mt8173_afe_pcm_hw_free(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct mtk_afe_info *afe_info =
		snd_soc_platform_get_drvdata(rtd->platform);

	if (afe_info->memif[dai_map[rtd->cpu_dai->id].pin].use_sram)
		return 0;
	return snd_pcm_lib_free_pages(substream);
}

static int mt8173_afe_pcm_open(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct mtk_afe_info *afe_info =
		snd_soc_platform_get_drvdata(rtd->platform);
	unsigned int pin = dai_map[rtd->cpu_dai->id].pin;

	if (rtd->cpu_dai->id > AFE_DAI_NUM) {
		dev_err(rtd->dev, "%s invalid DAI ID %d\n", __func__,
			rtd->cpu_dai->id);
		return -EINVAL;
	}

	afe_info->memif[pin].sub_stream = substream;
	afe_info->memif[pin].prepared = false;

	/* FIXME todo: disable deep idle for dram case */

	return 0;
}

static int mt8173_afe_pcm_new(struct snd_soc_pcm_runtime *rtd)
{
	size_t size;
	struct snd_card *card = rtd->card->snd_card;
	struct snd_pcm *pcm = rtd->pcm;
	struct mtk_afe_info *afe_info =
		snd_soc_platform_get_drvdata(rtd->platform);

	if (rtd->cpu_dai->id > AFE_DAI_NUM) {
		dev_err(rtd->dev, "%s invalid DAI ID %d\n", __func__,
			rtd->cpu_dai->id);
		return -EINVAL;
	}

	if (afe_info->memif[dai_map[rtd->cpu_dai->id].pin].use_sram)
		return 0;

	if (rtd->cpu_dai->id != AFE_HDMI_ID)
		size = mt8173_afe_hardware.buffer_bytes_max;
	else
		size = mt8173_hdmi_hardware.buffer_bytes_max;

	return snd_pcm_lib_preallocate_pages_for_all(pcm, SNDRV_DMA_TYPE_DEV,
						     card->dev, size, size);
}

static void mt8173_afe_pcm_free(struct snd_pcm *pcm)
{
	snd_pcm_lib_preallocate_free_for_all(pcm);
}

static struct snd_pcm_ops mt8173_afe_pcm_ops = {
	.open = mt8173_afe_pcm_open,
	.close = mt8173_afe_pcm_close,
	.ioctl = snd_pcm_lib_ioctl,
	.hw_params = mt8173_afe_pcm_hw_params,
	.hw_free = mt8173_afe_pcm_hw_free,
	.pointer = mt8173_afe_pcm_pointer,
};

static struct snd_soc_platform_driver mt8173_afe_pcm_platform = {
	.ops = &mt8173_afe_pcm_ops,
	.probe = mt8173_afe_pcm_probe,
	.remove	= mt8173_afe_pcm_remove,
	.pcm_new = mt8173_afe_pcm_new,
	.pcm_free = mt8173_afe_pcm_free,
};

/* mt8173_afe_dl1_ops */
static int mt8173_afe_dais_startup(struct snd_pcm_substream *substream,
				   struct snd_soc_dai *dai)
{
	int ret;
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct mtk_afe_info *afe_info =
		snd_soc_platform_get_drvdata(rtd->platform);

	snd_soc_set_runtime_hwparams(substream, &mt8173_afe_hardware);
	ret = snd_pcm_hw_constraint_integer(runtime,
					    SNDRV_PCM_HW_PARAM_PERIODS);
	if (ret < 0)
		dev_err(afe_info->dev,
			"snd_pcm_hw_constraint_integer failed: 0x%x\n",
			ret);
	return ret;
}

static void mt8173_afe_dl1_shutdown(struct snd_pcm_substream *substream,
				    struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct mtk_afe_info *afe_info =
		snd_soc_platform_get_drvdata(rtd->platform);

	if (afe_info->codec_if != MTK_AFE_IF_2ND_I2S)
		mt8173_afe_set_i2s_dac_enable(afe_info, false);
	else
		mt8173_afe_set_2nd_i2s_enable(afe_info, MTK_AFE_I2S_DIR_OUT,
					      substream->runtime->rate, false);
	mt8173_afe_enable_afe(afe_info, false);
}

static int mt8173_afe_dl1_prepare(struct snd_pcm_substream *substream,
				  struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct mtk_afe_info *afe_info =
		snd_soc_platform_get_drvdata(rtd->platform);

	if (afe_info->memif[MTK_AFE_PIN_DL1].prepared)
		return 0;
	afe_info->memif[MTK_AFE_PIN_DL1].prepared = true;

	/*
	 * HW sequence:
	 * DAI/AFE enable (prestart)-> codec -> memif enable (start)
	 */
	if (afe_info->codec_if != MTK_AFE_IF_2ND_I2S) {
		/* start I2S DAC out */
		mt8173_afe_set_i2s_dac_out(afe_info, substream->runtime->rate);
		mt8173_afe_set_i2s_dac_enable(afe_info, true);
	} else {
		/* start 2nd I2S out */
		mt8173_afe_set_2nd_i2s(afe_info, MTK_AFE_I2S_WLEN_16BITS,
				       substream->runtime->rate,
				       MTK_AFE_BCK_INV_NO,
				       MTK_AFE_I2S_SRC_MASTER);
		mt8173_afe_set_2nd_i2s_enable(afe_info, MTK_AFE_I2S_DIR_OUT,
					      substream->runtime->rate, true);
	}
	mt8173_afe_enable_afe(afe_info, true);
	return 0;
}

static int mt8173_afe_dais_trigger(struct snd_pcm_substream *substream, int cmd,
				   struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_pcm_runtime * const runtime = substream->runtime;
	struct timespec curr_tstamp;
	struct mtk_afe_info *afe_info =
		snd_soc_platform_get_drvdata(rtd->platform);
	uint32_t pin = dai_map[rtd->cpu_dai->id].pin;
	struct mtk_afe_memif *memif = &afe_info->memif[pin];
	unsigned int irq = memif->irq_num;
	struct mtk_afe_block *afe_block = &memif->block;
	unsigned int rate = mt8173_afe_i2s_fs(runtime->rate);
	unsigned int counter = runtime->period_size;

	dev_info(afe_info->dev, "%s cmd=%d\n", __func__, cmd);

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
		/* enable memif */
		mt8173_afe_set_memif_enable(afe_info, pin, true);
		/* enable interrupt */
		if (!afe_info->irq_status[irq]) {
			/* set irq counter */
			regmap_update_bits(afe_info->regmap,
					   memif->reg_irq_cnt.addr,
					   memif->reg_irq_cnt.mask,
					   counter << memif->reg_irq_cnt.shift);

			/* set irq fs */
			regmap_update_bits(afe_info->regmap,
					   memif->reg_irq_fs.addr,
					   memif->reg_irq_fs.mask,
					   rate << memif->reg_irq_fs.shift);

			mt8173_afe_set_irq_enable(afe_info, memif, true);

		} else {
			dev_dbg(afe_info->dev,
				"%s IRQ enabled , use original irq mode\n",
				__func__);
		}
		snd_pcm_gettime(substream->runtime,
				(struct timespec *)&curr_tstamp);
		dev_dbg(afe_info->dev, "%s curr_tstamp %ld %ld\n",
			__func__, curr_tstamp.tv_sec, curr_tstamp.tv_nsec);
		return 0;
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
		mt8173_afe_set_memif_enable(afe_info, pin, false);
		mt8173_afe_set_irq_enable(afe_info, memif, false);

		/* FIXME skip memset, to check */
		afe_block->dma_read_idx = 0;
		afe_block->write_idx = 0;
		return 0;
	default:
		return -EINVAL;
	}
}

static int mt8173_afe_dais_hw_params(struct snd_pcm_substream *substream,
				     struct snd_pcm_hw_params *params,
				     struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct mtk_afe_info *afe_info =
		snd_soc_platform_get_drvdata(rtd->platform);
	uint32_t pin = dai_map[rtd->cpu_dai->id].pin;
	struct mtk_afe_memif *memif = &afe_info->memif[pin];
	unsigned int mono = (params_channels(params) == 2) ? 0 : 1;

	mt8173_afe_set_sample_rate(afe_info, pin, params_rate(params));
	/* set channel */
	regmap_update_bits(afe_info->regmap, memif->reg_ch.addr,
			   memif->reg_ch.mask, mono << memif->reg_ch.shift);

	return 0;
}

static int mt8173_afe_pcm_set_route(struct mtk_afe_info *afe_info)
{
	/* FIXME: hardcode use 2ndI2S */
	bool use_2nd_i2s = true;

	if (!use_2nd_i2s) {
		afe_info->codec_if = MTK_AFE_IF_MTK;
		/* setup interconnections */
		mtk_afe_interconn_connect(afe_info,
					  MTK_AFE_INTERCONN_I05,
					  MTK_AFE_INTERCONN_O03);
		mtk_afe_interconn_connect(afe_info,
					  MTK_AFE_INTERCONN_I06,
					  MTK_AFE_INTERCONN_O04);
		mtk_afe_interconn_connect(afe_info,
					  MTK_AFE_INTERCONN_I03,
					  MTK_AFE_INTERCONN_O09);
		mtk_afe_interconn_connect(afe_info,
					  MTK_AFE_INTERCONN_I04,
					  MTK_AFE_INTERCONN_O10);
		/* set O3/O4 16bits */
		regmap_update_bits(afe_info->regmap, AFE_CONN_24BIT,
				   1 << MTK_AFE_INTERCONN_O03,
				   MTK_AFE_O16BIT << MTK_AFE_INTERCONN_O03);
		regmap_update_bits(afe_info->regmap, AFE_CONN_24BIT,
				   1 << MTK_AFE_INTERCONN_O04,
				   MTK_AFE_O16BIT << MTK_AFE_INTERCONN_O04);
	} else {
		/* I2S: 8135 use 2ndI2S */
		afe_info->codec_if = MTK_AFE_IF_2ND_I2S;

		/* setup interconnections */
		mtk_afe_interconn_connect(afe_info,
					  MTK_AFE_INTERCONN_I05,
					  MTK_AFE_INTERCONN_O00);
		mtk_afe_interconn_connect(afe_info,
					  MTK_AFE_INTERCONN_I06,
					  MTK_AFE_INTERCONN_O01);
		mtk_afe_interconn_connect(afe_info,
					  MTK_AFE_INTERCONN_I00,
					  MTK_AFE_INTERCONN_O05);
		mtk_afe_interconn_connect(afe_info,
					  MTK_AFE_INTERCONN_I01,
					  MTK_AFE_INTERCONN_O06);
	}
	return 0;
}

static struct snd_soc_dai_ops mt8173_afe_dl1_ops = {
	.startup	= mt8173_afe_dais_startup,
	.shutdown	= mt8173_afe_dl1_shutdown,
	.prepare	= mt8173_afe_dl1_prepare,
	.trigger	= mt8173_afe_dais_trigger,
	.hw_params	= mt8173_afe_dais_hw_params,
};

/* mt8173_afe_vul_ops */
static void mt8173_afe_vul_shutdown(struct snd_pcm_substream *substream,
				    struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct mtk_afe_info *afe_info =
		snd_soc_platform_get_drvdata(rtd->platform);

	mt8173_afe_set_i2s_adc_enable(afe_info, false);
	mt8173_afe_enable_afe(afe_info, false);
}

static int mt8173_afe_vul_prepare(struct snd_pcm_substream *substream,
				  struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct mtk_afe_info *afe_info =
		snd_soc_platform_get_drvdata(rtd->platform);

	if (afe_info->memif[MTK_AFE_PIN_VUL].prepared)
		return 0;
	afe_info->memif[MTK_AFE_PIN_VUL].prepared = true;

	/* config ADC I2S */
	mt8173_afe_set_i2s_adc(afe_info, substream->runtime->rate);
	mt8173_afe_set_i2s_adc_enable(afe_info, true);

	mt8173_afe_enable_afe(afe_info, true);
	return 0;
}

static struct snd_soc_dai_ops mt8173_afe_vul_ops = {
	.startup	= mt8173_afe_dais_startup,
	.shutdown	= mt8173_afe_vul_shutdown,
	.prepare	= mt8173_afe_vul_prepare,
	.trigger	= mt8173_afe_dais_trigger,
	.hw_params	= mt8173_afe_dais_hw_params,
};

/* mt8173_afe_awb_ops */
static void mt8173_afe_awb_shutdown(struct snd_pcm_substream *substream,
				    struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct mtk_afe_info *afe_info =
		snd_soc_platform_get_drvdata(rtd->platform);

	mt8173_afe_set_2nd_i2s_enable(afe_info, MTK_AFE_I2S_DIR_IN,
				      substream->runtime->rate, false);
	mt8173_afe_enable_afe(afe_info, false);
}

static int mt8173_afe_awb_prepare(struct snd_pcm_substream *substream,
				  struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct mtk_afe_info *afe_info =
		snd_soc_platform_get_drvdata(rtd->platform);

	if (afe_info->memif[MTK_AFE_PIN_AWB].prepared)
		return 0;
	afe_info->memif[MTK_AFE_PIN_AWB].prepared = true;

	/* config 2nd I2S */
	mt8173_afe_set_2nd_i2s(afe_info, MTK_AFE_I2S_WLEN_16BITS,
			       substream->runtime->rate, MTK_AFE_BCK_INV_NO,
			       MTK_AFE_I2S_SRC_MASTER);
	mt8173_afe_set_2nd_i2s_enable(afe_info, MTK_AFE_I2S_DIR_IN,
				      substream->runtime->rate, true);

	mt8173_afe_enable_afe(afe_info, true);
	return 0;
}

static struct snd_soc_dai_ops mt8173_afe_awb_ops = {
	.startup	= mt8173_afe_dais_startup,
	.shutdown	= mt8173_afe_awb_shutdown,
	.prepare	= mt8173_afe_awb_prepare,
	.trigger	= mt8173_afe_dais_trigger,
	.hw_params	= mt8173_afe_dais_hw_params,
};

/* mt8173_afe_hdmi_ops */
static int mt8173_afe_hdmi_startup(struct snd_pcm_substream *substream,
				   struct snd_soc_dai *dai)
{
	int ret;
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct mtk_afe_info *afe_info =
		snd_soc_platform_get_drvdata(rtd->platform);

	snd_soc_set_runtime_hwparams(substream, &mt8173_hdmi_hardware);

	/* Ensure that buffer size is a multiple of period size */
	ret = snd_pcm_hw_constraint_integer(runtime,
					    SNDRV_PCM_HW_PARAM_PERIODS);
	if (ret < 0)
		dev_err(afe_info->dev,
			"%s snd_pcm_hw_constraint_integer fail %d\n",
			__func__, ret);
	return ret;
}

static void mt8173_afe_hdmi_shutdown(struct snd_pcm_substream *substream,
				     struct snd_soc_dai *dai)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct mtk_afe_info *afe_info =
		snd_soc_platform_get_drvdata(rtd->platform);

	if (afe_info->memif[MTK_AFE_PIN_HDMI].prepared) {
		clk_disable_unprepare(afe_info->clock[MTK_CLK_SPDIF].clk);
		mt8173_afe_enable_apll_div_pwr(afe_info, MTK_AFE_SPDIF,
					       runtime->rate, false);

		clk_disable_unprepare(afe_info->clock[MTK_CLK_HDMI].clk);
		mt8173_afe_enable_apll_div_pwr(afe_info, MTK_AFE_I2S3_BCK,
					       runtime->rate, false);
		mt8173_afe_enable_apll_div_pwr(afe_info, MTK_AFE_I2S3,
					       runtime->rate, false);

		mt8173_afe_enable_apll_tuner(afe_info, runtime->rate, false);
		mt8173_afe_enable_apll(afe_info, runtime->rate, false);
		afe_info->memif[MTK_AFE_PIN_HDMI].prepared = false;
	}
}

static int mt8173_afe_hdmi_prepare(struct snd_pcm_substream *substream,
				   struct snd_soc_dai *dai)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct mtk_afe_info *afe_info =
		snd_soc_platform_get_drvdata(rtd->platform);
	int ret;
	uint32_t mck_div = mt8173_afe_set_clk_mclk(afe_info, MTK_AFE_I2S3,
						   runtime->rate);

	if (afe_info->memif[MTK_AFE_PIN_HDMI].prepared)
		return 0;
	afe_info->memif[MTK_AFE_PIN_HDMI].prepared = true;

	/* setup HDMI clocks */
	mt8173_afe_enable_apll(afe_info, runtime->rate, true);
	mt8173_afe_enable_apll_tuner(afe_info, runtime->rate, true);
	mt8173_afe_set_clk_i2s3_bclk(afe_info, mck_div, runtime->rate, 2, 32);
	mt8173_afe_enable_apll_div_pwr(afe_info, MTK_AFE_I2S3,
				       runtime->rate, true);
	mt8173_afe_enable_apll_div_pwr(afe_info, MTK_AFE_I2S3_BCK,
				       runtime->rate, true);

	/* hdmi clk */
	ret = clk_prepare_enable(afe_info->clock[MTK_CLK_HDMI].clk);
	if (ret) {
		dev_err(afe_info->dev, "%s clk_prepare_enable %s fail %d\n",
			__func__, afe_info->clock[MTK_CLK_HDMI].name, ret);
		return ret;
	}
	mt8173_afe_set_clk_mclk(afe_info, MTK_AFE_SPDIF, runtime->rate);
	mt8173_afe_enable_apll_div_pwr(afe_info, MTK_AFE_SPDIF,
				       runtime->rate, true);

	/* SPDIF clk */
	ret = clk_prepare_enable(afe_info->clock[MTK_CLK_SPDIF].clk);
	if (ret)
		dev_err(afe_info->dev, "%s clk_prepare_enable %s fail %d\n",
			__func__, afe_info->clock[MTK_CLK_SPDIF].name, ret);

	/* save sampling rate for loopback */
	afe_info->hdmi_fs = runtime->rate;

	return ret;
}

static int mt8173_afe_hdmi_trigger(struct snd_pcm_substream *substream, int cmd,
				   struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_pcm_runtime * const runtime = substream->runtime;
	struct mtk_afe_info *afe_info =
		snd_soc_platform_get_drvdata(rtd->platform);
	uint32_t pin = dai_map[rtd->cpu_dai->id].pin;
	struct mtk_afe_memif *memif = &afe_info->memif[pin];
	struct mtk_afe_block *afe_block = &memif->block;
	unsigned int counter = runtime->period_size;

	dev_info(afe_info->dev, "%s cmd=%d\n", __func__, cmd);

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
		/* set irq counter */
		regmap_update_bits(afe_info->regmap,
				   memif->reg_irq_cnt.addr,
				   memif->reg_irq_cnt.mask,
				   counter << memif->reg_irq_cnt.shift);

		mt8173_afe_set_irq_enable(afe_info, memif, true);

		/* set hdmi channel */
		mt8173_afe_set_hdmi_tdm2_config(afe_info, runtime->channels);
		regmap_update_bits(afe_info->regmap, AFE_HDMI_OUT_CON0,
				   0x000000F0, runtime->channels << 4);

		/* enable Out control */
		regmap_update_bits(afe_info->regmap, AFE_HDMI_OUT_CON0,
				   1, true);

		/* enable memif */
		mt8173_afe_set_memif_enable(afe_info, MTK_AFE_PIN_HDMI, true);
		mt8173_afe_enable_afe(afe_info, true);

		/* enable tdm */
		regmap_update_bits(afe_info->regmap, AFE_TDM_CON1, 1, true);
		return 0;
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:

		/* disable tdm */
		regmap_update_bits(afe_info->regmap, AFE_TDM_CON1, 1, false);

		/* disable Out control */
		regmap_update_bits(afe_info->regmap, AFE_HDMI_OUT_CON0,
				   1, false);

		mt8173_afe_set_irq_enable(afe_info, memif, false);

		mt8173_afe_set_memif_enable(afe_info, MTK_AFE_PIN_HDMI, false);
		mt8173_afe_enable_afe(afe_info, false);

		afe_block->dma_read_idx = 0;
		afe_block->write_idx = 0;
		return 0;
	}
	return -EINVAL;
}

static struct snd_soc_dai_ops mt8173_afe_hdmi_ops = {
	.startup	= mt8173_afe_hdmi_startup,
	.shutdown	= mt8173_afe_hdmi_shutdown,
	.prepare	= mt8173_afe_hdmi_prepare,
	.trigger	= mt8173_afe_hdmi_trigger,
};

static struct snd_soc_dai_driver mt8173_afe_pcm_dais[] = {
	{
		.name  = "DL1",
		.id  = AFE_DL1_ID,
		.playback = {
			.stream_name = "DL1 Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_8000_48000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
		},
		.ops = &mt8173_afe_dl1_ops,
	},
	{
		.name  = "AWB",
		.id  = AFE_AWB_ID,
		.capture = {
			.stream_name = "AWB Capture",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_8000_48000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
		},
		.ops = &mt8173_afe_awb_ops,
	},
	{
		.name  = "VUL",
		.id  = AFE_VUL_ID,
		.capture = {
			.stream_name = "VUL Capture",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_8000_48000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
		},
		.ops = &mt8173_afe_vul_ops,
	},
	{
		.name = "HDMI",
		.id  = AFE_HDMI_ID,
		.playback = {
			.stream_name = "HDMI Playback",
			.channels_min = 2,
			.channels_max = 8,
			.rates = MTK_AFE_HDMI_RATES,
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
		},
		.ops = &mt8173_afe_hdmi_ops,
	},
};

static const struct snd_soc_component_driver mt8173_afe_pcm_dai_component = {
	.name		= "mt8173-afe-pcm-dai",
};

static int mt8173_afe_pcm_dev_probe(struct platform_device *pdev)
{
	int ret;
	struct mtk_afe_info *afe_info;
	uint32_t i;

	ret = mtk_afe_platform_init(pdev);
	if (ret)
		return ret;

	/* set audio route */
	afe_info = platform_get_drvdata(pdev);
	mt8173_afe_pcm_set_route(afe_info);

	/* power down all dividers */
	for (i = MTK_AFE_APLL1_DIV0; i < MTK_AFE_APLL_DIV_COUNT; i++)
		mt8173_enable_i2s_div_pwr(afe_info, i, false);

	ret = snd_soc_register_platform(&pdev->dev, &mt8173_afe_pcm_platform);
	if (ret)
		return ret;

	return snd_soc_register_component(&pdev->dev,
					  &mt8173_afe_pcm_dai_component,
					  mt8173_afe_pcm_dais,
					  ARRAY_SIZE(mt8173_afe_pcm_dais));
}

static int mt8173_afe_pcm_dev_remove(struct platform_device *pdev)
{
	struct mtk_afe_info *afe_info = platform_get_drvdata(pdev);

	/* close afe clock */
	mtk_afe_clk_off(afe_info);
	snd_soc_unregister_platform(&pdev->dev);
	snd_soc_unregister_component(&pdev->dev);
	return 0;
}

static const struct of_device_id mt8173_afe_pcm_dt_match[] = {
	{ .compatible = "mediatek,mt8173-afe-pcm", },
	{ }
};

static struct platform_driver mt8173_afe_pcm_driver = {
	.driver = {
		   .name = "mt8173-afe-pcm",
		   .owner = THIS_MODULE,
		   .of_match_table = mt8173_afe_pcm_dt_match,
	},
	.probe = mt8173_afe_pcm_dev_probe,
	.remove = mt8173_afe_pcm_dev_remove,
};

module_platform_driver(mt8173_afe_pcm_driver);

MODULE_DESCRIPTION("Mediatek ALSA SoC Afe platform driver");
MODULE_AUTHOR("Koro Chen <koro.chen@mediatek.com>");
MODULE_LICENSE("GPL v2");
MODULE_DEVICE_TABLE(of, mt8173_afe_pcm_dt_match);

