/*
 * mt8135_evbp1.c  --  MT8135 EVBP1 ALSA SoC machine driver
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

#include "mt_soc_clk.h"
#include "mt_soc_afe_control.h"
#include <linux/module.h>
#include <sound/soc.h>
#include <linux/debugfs.h>


static struct dentry *mt_sco_audio_debugfs;


static int mt_soc_debug_open(struct inode *inode, struct file *file)
{
	pr_notice("mt_soc_debug_open\n");
	return 0;
}

static ssize_t mt_soc_debug_read(struct file *file, char __user *buf,
				 size_t count, loff_t *pos)
{
	const int size = 4096;
	char buffer[size];
	int n = 0;

	aud_drv_clk_on();

	n += scnprintf(buffer + n, size - n, "AUDIO_TOP_CON0  = 0x%x\n",
		       afe_get_reg(AUDIO_TOP_CON0));
	n += scnprintf(buffer + n, size - n, "AUDIO_TOP_CON1  = 0x%x\n",
		       afe_get_reg(AUDIO_TOP_CON1));
	n += scnprintf(buffer + n, size - n, "AUDIO_TOP_CON3  = 0x%x\n",
		       afe_get_reg(AUDIO_TOP_CON3));
	n += scnprintf(buffer + n, size - n, "AFE_DAC_CON0  = 0x%x\n",
		       afe_get_reg(AFE_DAC_CON0));
	n += scnprintf(buffer + n, size - n, "AFE_DAC_CON1  = 0x%x\n",
		       afe_get_reg(AFE_DAC_CON1));
	n += scnprintf(buffer + n, size - n, "AFE_I2S_CON  = 0x%x\n",
		       afe_get_reg(AFE_I2S_CON));
	n += scnprintf(buffer + n, size - n, "AFE_DAIBT_CON0  = 0x%x\n",
		       afe_get_reg(AFE_DAIBT_CON0));
	n += scnprintf(buffer + n, size - n, "AFE_CONN0  = 0x%x\n",
		       afe_get_reg(AFE_CONN0));
	n += scnprintf(buffer + n, size - n, "AFE_CONN1  = 0x%x\n",
		       afe_get_reg(AFE_CONN1));
	n += scnprintf(buffer + n, size - n, "AFE_CONN2  = 0x%x\n",
		       afe_get_reg(AFE_CONN2));
	n += scnprintf(buffer + n, size - n, "AFE_CONN3  = 0x%x\n",
		       afe_get_reg(AFE_CONN3));
	n += scnprintf(buffer + n, size - n, "AFE_CONN4  = 0x%x\n",
		       afe_get_reg(AFE_CONN4));
	n += scnprintf(buffer + n, size - n, "AFE_I2S_CON1  = 0x%x\n",
		       afe_get_reg(AFE_I2S_CON1));
	n += scnprintf(buffer + n, size - n, "AFE_I2S_CON2  = 0x%x\n",
		       afe_get_reg(AFE_I2S_CON2));
	n += scnprintf(buffer + n, size - n, "AFE_MRGIF_CON  = 0x%x\n",
		       afe_get_reg(AFE_MRGIF_CON));
	n += scnprintf(buffer + n, size - n, "AFE_DL1_BASE  = 0x%x\n",
		       afe_get_reg(AFE_DL1_BASE));
	n += scnprintf(buffer + n, size - n, "AFE_DL1_CUR  = 0x%x\n",
		       afe_get_reg(AFE_DL1_CUR));
	n += scnprintf(buffer + n, size - n, "AFE_DL1_END  = 0x%x\n",
		       afe_get_reg(AFE_DL1_END));
	n += scnprintf(buffer + n, size - n, "AFE_DL2_BASE  = 0x%x\n",
		       afe_get_reg(AFE_DL2_BASE));
	n += scnprintf(buffer + n, size - n, "AFE_DL2_CUR  = 0x%x\n",
		       afe_get_reg(AFE_DL2_CUR));
	n += scnprintf(buffer + n, size - n, "AFE_DL2_END  = 0x%x\n",
		       afe_get_reg(AFE_DL2_END));
	n += scnprintf(buffer + n, size - n, "AFE_AWB_BASE  = 0x%x\n",
		       afe_get_reg(AFE_AWB_BASE));
	n += scnprintf(buffer + n, size - n, "AFE_AWB_END  = 0x%x\n",
		       afe_get_reg(AFE_AWB_END));
	n += scnprintf(buffer + n, size - n, "AFE_AWB_CUR  = 0x%x\n",
		       afe_get_reg(AFE_AWB_CUR));
	n += scnprintf(buffer + n, size - n, "AFE_VUL_BASE  = 0x%x\n",
		       afe_get_reg(AFE_VUL_BASE));
	n += scnprintf(buffer + n, size - n, "AFE_VUL_END  = 0x%x\n",
		       afe_get_reg(AFE_VUL_END));
	n += scnprintf(buffer + n, size - n, "AFE_VUL_CUR  = 0x%x\n",
		       afe_get_reg(AFE_VUL_CUR));
	n += scnprintf(buffer + n, size - n, "AFE_DAI_BASE  = 0x%x\n",
		       afe_get_reg(AFE_DAI_BASE));
	n += scnprintf(buffer + n, size - n, "AFE_DAI_END  = 0x%x\n",
		       afe_get_reg(AFE_DAI_END));
	n += scnprintf(buffer + n, size - n, "AFE_DAI_CUR  = 0x%x\n",
		       afe_get_reg(AFE_DAI_CUR));
	n += scnprintf(buffer + n, size - n, "AFE_IRQ_CON= 0x%x\n",
		       afe_get_reg(AFE_IRQ_CON));
	n += scnprintf(buffer + n, size - n, "MEMIF_MON0 = 0x%x\n",
		       afe_get_reg(AFE_MEMIF_MON0));
	n += scnprintf(buffer + n, size - n, "MEMIF_MON1 = 0x%x\n",
		       afe_get_reg(AFE_MEMIF_MON1));
	n += scnprintf(buffer + n, size - n, "MEMIF_MON2 = 0x%x\n",
		       afe_get_reg(AFE_MEMIF_MON2));
	n += scnprintf(buffer + n, size - n, "MEMIF_MON3 = 0x%x\n",
		       afe_get_reg(AFE_MEMIF_MON3));
	n += scnprintf(buffer + n, size - n, "MEMIF_MON4 = 0x%x\n",
		       afe_get_reg(AFE_MEMIF_MON4));
	n += scnprintf(buffer + n, size - n, "AFE_ADDA_DL_SRC2_CON0  = 0x%x\n",
		       afe_get_reg(AFE_ADDA_DL_SRC2_CON0));
	n += scnprintf(buffer + n, size - n, "AFE_ADDA_DL_SRC2_CON1  = 0x%x\n",
		       afe_get_reg(AFE_ADDA_DL_SRC2_CON1));
	n += scnprintf(buffer + n, size - n, "AFE_ADDA_UL_SRC_CON0  = 0x%x\n",
		       afe_get_reg(AFE_ADDA_UL_SRC_CON0));
	n += scnprintf(buffer + n, size - n, "AFE_ADDA_UL_SRC_CON1  = 0x%x\n",
		       afe_get_reg(AFE_ADDA_UL_SRC_CON1));
	n += scnprintf(buffer + n, size - n, "AFE_ADDA_TOP_CON0  = 0x%x\n",
		       afe_get_reg(AFE_ADDA_TOP_CON0));
	n += scnprintf(buffer + n, size - n, "AFE_ADDA_UL_DL_CON0  = 0x%x\n",
		       afe_get_reg(AFE_ADDA_UL_DL_CON0));
	n += scnprintf(buffer + n, size - n, "AFE_ADDA_SRC_DEBUG  = 0x%x\n",
		       afe_get_reg(AFE_ADDA_SRC_DEBUG));
	n += scnprintf(buffer + n, size - n,
		       "AFE_ADDA_SRC_DEBUG_MON0  = 0x%x\n",
		       afe_get_reg(AFE_ADDA_SRC_DEBUG_MON0));
	n += scnprintf(buffer + n, size - n,
		       "AFE_ADDA_SRC_DEBUG_MON1  = 0x%x\n",
		       afe_get_reg(AFE_ADDA_SRC_DEBUG_MON1));
	n += scnprintf(buffer + n, size - n, "AFE_ADDA_NEWIF_CFG0  = 0x%x\n",
		       afe_get_reg(AFE_ADDA_NEWIF_CFG0));
	n += scnprintf(buffer + n, size - n, "AFE_ADDA_NEWIF_CFG1  = 0x%x\n",
		       afe_get_reg(AFE_ADDA_NEWIF_CFG1));
	n += scnprintf(buffer + n, size - n, "SIDETONE_DEBUG = 0x%x\n",
		       afe_get_reg(AFE_SIDETONE_DEBUG));
	n += scnprintf(buffer + n, size - n, "SIDETONE_MON = 0x%x\n",
		       afe_get_reg(AFE_SIDETONE_MON));
	n += scnprintf(buffer + n, size - n, "SIDETONE_CON0 = 0x%x\n",
		       afe_get_reg(AFE_SIDETONE_CON0));
	n += scnprintf(buffer + n, size - n, "SIDETONE_COEFF = 0x%x\n",
		       afe_get_reg(AFE_SIDETONE_COEFF));
	n += scnprintf(buffer + n, size - n, "SIDETONE_CON1 = 0x%x\n",
		       afe_get_reg(AFE_SIDETONE_CON1));
	n += scnprintf(buffer + n, size - n, "SIDETONE_GAIN = 0x%x\n",
		       afe_get_reg(AFE_SIDETONE_GAIN));
	n += scnprintf(buffer + n, size - n, "SGEN_CON0 = 0x%x\n",
		       afe_get_reg(AFE_SGEN_CON0));
	n += scnprintf(buffer + n, size - n, "AFE_MRGIF_MON0 = 0x%x\n",
		       afe_get_reg(AFE_MRGIF_MON0));
	n += scnprintf(buffer + n, size - n, "AFE_MRGIF_MON1 = 0x%x\n",
		       afe_get_reg(AFE_MRGIF_MON1));
	n += scnprintf(buffer + n, size - n, "AFE_MRGIF_MON2 = 0x%x\n",
		       afe_get_reg(AFE_MRGIF_MON2));
	n += scnprintf(buffer + n, size - n, "AFE_TOP_CON0 = 0x%x\n",
		       afe_get_reg(AFE_TOP_CON0));

	n += scnprintf(buffer + n, size - n, "AFE_ADDA_PREDIS_CON0 = 0x%x\n",
		       afe_get_reg(AFE_ADDA_PREDIS_CON0));
	n += scnprintf(buffer + n, size - n, "AFE_ADDA_PREDIS_CON1 = 0x%x\n",
		       afe_get_reg(AFE_ADDA_PREDIS_CON1));

	n += scnprintf(buffer + n, size - n, "HDMI_OUT_CON0   = 0x%x\n",
		       afe_get_reg(AFE_HDMI_OUT_CON0));
	n += scnprintf(buffer + n, size - n, "HDMI_OUT_BASE   = 0x%x\n",
		       afe_get_reg(AFE_HDMI_OUT_BASE));
	n += scnprintf(buffer + n, size - n, "HDMI_OUT_CUR    = 0x%x\n",
		       afe_get_reg(AFE_HDMI_OUT_CUR));
	n += scnprintf(buffer + n, size - n, "HDMI_OUT_END    = 0x%x\n",
		       afe_get_reg(AFE_HDMI_OUT_END));
	n += scnprintf(buffer + n, size - n, "SPDIF_OUT_CON0  = 0x%x\n",
		       afe_get_reg(AFE_SPDIF_OUT_CON0));
	n += scnprintf(buffer + n, size - n, "SPDIF_BASE      = 0x%x\n",
		       afe_get_reg(AFE_SPDIF_BASE));
	n += scnprintf(buffer + n, size - n, "SPDIF_CUR       = 0x%x\n",
		       afe_get_reg(AFE_SPDIF_CUR));
	n += scnprintf(buffer + n, size - n, "SPDIF_END       = 0x%x\n",
		       afe_get_reg(AFE_SPDIF_END));
	n += scnprintf(buffer + n, size - n, "HDMI_CONN0      = 0x%x\n",
		       afe_get_reg(AFE_HDMI_CONN0));
	n += scnprintf(buffer + n, size - n, "8CH_I2S_OUT_CON = 0x%x\n",
		       afe_get_reg(AFE_8CH_I2S_OUT_CON));
	n += scnprintf(buffer + n, size - n, "IEC_CFG         = 0x%x\n",
		       afe_get_reg(AFE_IEC_CFG));
	n += scnprintf(buffer + n, size - n, "IEC_NSNUM       = 0x%x\n",
		       afe_get_reg(AFE_IEC_NSNUM));
	n += scnprintf(buffer + n, size - n, "IEC_BURST_INFO  = 0x%x\n",
		       afe_get_reg(AFE_IEC_BURST_INFO));
	n += scnprintf(buffer + n, size - n, "IEC_BURST_LEN   = 0x%x\n",
		       afe_get_reg(AFE_IEC_BURST_LEN));
	n += scnprintf(buffer + n, size - n, "IEC_NSADR       = 0x%x\n",
		       afe_get_reg(AFE_IEC_NSADR));
	n += scnprintf(buffer + n, size - n, "IEC_CHL_STAT0   = 0x%x\n",
		       afe_get_reg(AFE_IEC_CHL_STAT0));
	n += scnprintf(buffer + n, size - n, "IEC_CHL_STAT1   = 0x%x\n",
		       afe_get_reg(AFE_IEC_CHL_STAT1));
	n += scnprintf(buffer + n, size - n, "IEC_CHR_STAT0   = 0x%x\n",
		       afe_get_reg(AFE_IEC_CHR_STAT0));
	n += scnprintf(buffer + n, size - n, "IEC_CHR_STAT1   = 0x%x\n",
		       afe_get_reg(AFE_IEC_CHR_STAT1));
	n += scnprintf(buffer + n, size - n, "AFE_IRQ_MCU_CON = 0x%x\n",
				afe_get_reg(AFE_IRQ_MCU_CON));	/* ccc */
	n += scnprintf(buffer + n, size - n, "AFE_IRQ_STATUS = 0x%x\n",
		       afe_get_reg(AFE_IRQ_STATUS));
	n += scnprintf(buffer + n, size - n, "AFE_IRQ_CLR = 0x%x\n",
		       afe_get_reg(AFE_IRQ_CLR));
	n += scnprintf(buffer + n, size - n, "AFE_IRQ_CNT1 = 0x%x\n",
		       afe_get_reg(AFE_IRQ_CNT1));
	n += scnprintf(buffer + n, size - n, "AFE_IRQ_CNT2 = 0x%x\n",
		       afe_get_reg(AFE_IRQ_CNT2));
	n += scnprintf(buffer + n, size - n, "AFE_IRQ_MON2 = 0x%x\n",
		       afe_get_reg(AFE_IRQ_MON2));
	n += scnprintf(buffer + n, size - n, "AFE_IRQ_CNT5 = 0x%x\n",
		       afe_get_reg(AFE_IRQ_CNT5));
	n += scnprintf(buffer + n, size - n, "AFE_IRQ1_CNT_MON = 0x%x\n",
		       afe_get_reg(AFE_IRQ1_CNT_MON));
	n += scnprintf(buffer + n, size - n, "AFE_IRQ2_CNT_MON = 0x%x\n",
		       afe_get_reg(AFE_IRQ2_CNT_MON));
	n += scnprintf(buffer + n, size - n, "AFE_IRQ1_EN_CNT_MON = 0x%x\n",
		       afe_get_reg(AFE_IRQ1_EN_CNT_MON));
	n += scnprintf(buffer + n, size - n, "AFE_IRQ5_EN_CNT_MON = 0x%x\n",
		       afe_get_reg(AFE_IRQ5_EN_CNT_MON));

	n += scnprintf(buffer + n, size - n, "AFE_MEMIF_MINLEN  = 0x%x\n",
		       afe_get_reg(AFE_MEMIF_MINLEN));
	n += scnprintf(buffer + n, size - n, "AFE_MEMIF_MAXLEN  = 0x%x\n",
		       afe_get_reg(AFE_MEMIF_MAXLEN));
	n += scnprintf(buffer + n, size - n, "AFE_MEMIF_PBUF_SIZE  = 0x%x\n",
		       afe_get_reg(AFE_MEMIF_PBUF_SIZE));
	n += scnprintf(buffer + n, size - n, "AFE_GAIN1_CON0  = 0x%x\n",
		       afe_get_reg(AFE_GAIN1_CON0));
	n += scnprintf(buffer + n, size - n, "AFE_GAIN1_CON1  = 0x%x\n",
		       afe_get_reg(AFE_GAIN1_CON1));
	n += scnprintf(buffer + n, size - n, "AFE_GAIN1_CON2  = 0x%x\n",
		       afe_get_reg(AFE_GAIN1_CON2));
	n += scnprintf(buffer + n, size - n, "AFE_GAIN1_CON3  = 0x%x\n",
		       afe_get_reg(AFE_GAIN1_CON3));
	n += scnprintf(buffer + n, size - n, "AFE_GAIN1_CONN  = 0x%x\n",
		       afe_get_reg(AFE_GAIN1_CONN));
	n += scnprintf(buffer + n, size - n, "AFE_GAIN1_CUR  = 0x%x\n",
		       afe_get_reg(AFE_GAIN1_CUR));
	n += scnprintf(buffer + n, size - n, "AFE_GAIN2_CON0  = 0x%x\n",
		       afe_get_reg(AFE_GAIN2_CON0));
	n += scnprintf(buffer + n, size - n, "AFE_GAIN2_CON1  = 0x%x\n",
		       afe_get_reg(AFE_GAIN2_CON1));
	n += scnprintf(buffer + n, size - n, "AFE_GAIN2_CON2  = 0x%x\n",
		       afe_get_reg(AFE_GAIN2_CON2));
	n += scnprintf(buffer + n, size - n, "AFE_GAIN2_CON3  = 0x%x\n",
		       afe_get_reg(AFE_GAIN2_CON3));
	n += scnprintf(buffer + n, size - n, "AFE_GAIN2_CONN  = 0x%x\n",
		       afe_get_reg(AFE_GAIN2_CONN));
	n += scnprintf(buffer + n, size - n, "AFE_GAIN2_CONN2  = 0x%x\n",
		       afe_get_reg(AFE_GAIN2_CONN2));
	n += scnprintf(buffer + n, size - n, "AFE_GAIN2_CUR  = 0x%x\n",
		       afe_get_reg(AFE_GAIN2_CUR));

	aud_drv_clk_off();
	return simple_read_from_buffer(buf, count, pos, buffer, n);
}

static const struct file_operations mtaudio_debug_ops = {
	.open = mt_soc_debug_open,
	.read = mt_soc_debug_read,
};

/* Digital audio interface glue - connects codec <---> CPU */
static struct snd_soc_dai_link mt_soc_dai_common[] = {
	/* FrontEnd DAI Links */
	{
	 .name = "MultiMedia1",
	 .stream_name = "Playback",
	 .cpu_dai_name = "snd-soc-dummy-dai",
	 .platform_name = "mt-soc-dl1-pcm",
	 .codec_dai_name = "mt-soc-codec-tx-dai",
	 .codec_name = "mt6397-codec",
	 },
};


static const struct snd_kcontrol_new mt_soc_controls[] = {
};


static struct snd_soc_card snd_soc_card_mt = {
	.name = "mt-snd-card",
	.dai_link = mt_soc_dai_common,
	.num_links = ARRAY_SIZE(mt_soc_dai_common),
	.controls = mt_soc_controls,
	.num_controls = ARRAY_SIZE(mt_soc_controls),
};

static int mtk_soc_machine_dev_probe(struct platform_device *pdev)
{
	struct snd_soc_card *card = &snd_soc_card_mt;
	struct device_node *codec_node;
	int ret;

	pr_notice("%s dev name %s\n", __func__, dev_name(&pdev->dev));
	if (pdev->dev.of_node) {
		dev_set_name(&pdev->dev, "%s", "mt8135-evbp1");
		pr_notice("%s set dev name %s\n", __func__,
			  dev_name(&pdev->dev));
	}
	codec_node = of_parse_phandle(pdev->dev.of_node,
				      "mediatek,audio-codec", 0);
	if (!codec_node) {
		dev_err(&pdev->dev,
			"Property 'audio-codec' missing or invalid\n");
	} else {
		mt_soc_dai_common[0].codec_name = NULL;
		mt_soc_dai_common[0].codec_of_node = codec_node;
	}

	card->dev = &pdev->dev;

	init_audio_clk(&pdev->dev);

	ret = snd_soc_register_card(card);
	if (ret) {
		pr_err("%s snd_soc_register_card fail %d\n", __func__, ret);
		return ret;
	}
	mt_sco_audio_debugfs =
	    debugfs_create_file("mtksocaudio", S_IFREG | S_IRUGO, NULL,
				(void *)"mtksocaudio", &mtaudio_debug_ops);

	return 0;
}

static int mtk_soc_machine_dev_remove(struct platform_device *pdev)
{
	struct snd_soc_card *card = platform_get_drvdata(pdev);

	snd_soc_unregister_card(card);
	debugfs_remove(mt_sco_audio_debugfs);
	return 0;
}

static const struct of_device_id mtk_soc_machine_dt_match[] = {
	{ .compatible = "mediatek,mt8135-evbp1", },
	{ }
};
MODULE_DEVICE_TABLE(of, mtk_soc_machine_dt_match);

static struct platform_driver mtk_soc_machine_driver = {
	.driver = {
		   .name = "mt8135-evbp1",
		   .owner = THIS_MODULE,
		   .of_match_table = mtk_soc_machine_dt_match,
#ifdef CONFIG_PM
		   .pm = &snd_soc_pm_ops,
#endif
		   },
	.probe = mtk_soc_machine_dev_probe,
	.remove = mtk_soc_machine_dev_remove,
};

module_platform_driver(mtk_soc_machine_driver);

/* Module information */
MODULE_DESCRIPTION("ALSA SoC driver for mtxxxx");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:mt-snd-card");
