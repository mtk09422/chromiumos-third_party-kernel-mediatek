/*
 * mt8173_evb.c  --  mt8173 evb ALSA SoC machine driver
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

#include <linux/module.h>
#include <sound/soc.h>

/* Digital audio interface glue - connects codec <---> CPU */
static struct snd_soc_dai_link mt8173_evb_dai_common[] = {
	{
		.name = "Playback",
		.stream_name = "Playback",
		.cpu_dai_name = "DL1",
		.platform_name = "11220000.mt8173-afe-pcm",
		.codec_dai_name = "mt-soc-codec-tx-dai",
		.codec_name = "mt6397-codec",
	},
	{
		.name = "Capture",
		.stream_name = "Capture",
		.cpu_dai_name = "VUL",
		.platform_name = "11220000.mt8173-afe-pcm",
		.codec_dai_name = "mt-soc-codec-rx-dai",
		.codec_name = "mt6397-codec",
	},
	{
		.name = "HDMI Playback",
		.stream_name = "HDMI Playback",
		.cpu_dai_name = "HDMI",
		.platform_name = "11220000.mt8173-afe-pcm",
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
	},
};

static struct snd_soc_card mt8173_evb_card = {
	.name = "mt-snd-card",
	.dai_link = mt8173_evb_dai_common,
	.num_links = ARRAY_SIZE(mt8173_evb_dai_common),
};

static int mt8173_evb_dev_probe(struct platform_device *pdev)
{
	struct snd_soc_card *card = &mt8173_evb_card;
	struct device_node *codec_node;
	int ret, i;

	codec_node = of_parse_phandle(pdev->dev.of_node,
				      "mediatek,audio-codec", 0);
	if (!codec_node) {
		dev_err(&pdev->dev,
			"Property 'audio-codec' missing or invalid\n");
	} else {
		for (i = 0; i < card->num_links; i++) {
			if (mt8173_evb_dai_common[i].codec_name &&
			    !strcmp(mt8173_evb_dai_common[i].codec_name,
				    "snd-soc-dummy"))
				continue;
			mt8173_evb_dai_common[i].codec_name = NULL;
			mt8173_evb_dai_common[i].codec_of_node = codec_node;
		}
	}

	card->dev = &pdev->dev;

	ret = snd_soc_register_card(card);
	if (ret)
		dev_err(&pdev->dev, "%s snd_soc_register_card fail %d\n",
			__func__, ret);
	return ret;
}

static int mt8173_evb_dev_remove(struct platform_device *pdev)
{
	struct snd_soc_card *card = platform_get_drvdata(pdev);

	snd_soc_unregister_card(card);
	return 0;
}

static const struct of_device_id mt8173_evb_dt_match[] = {
	{ .compatible = "mediatek,mt8173-evb", },
	{ }
};

static struct platform_driver mt8173_evb_driver = {
	.driver = {
		   .name = "mt8173-evb",
		   .owner = THIS_MODULE,
		   .of_match_table = mt8173_evb_dt_match,
#ifdef CONFIG_PM
		   .pm = &snd_soc_pm_ops,
#endif
	},
	.probe = mt8173_evb_dev_probe,
	.remove = mt8173_evb_dev_remove,
};

module_platform_driver(mt8173_evb_driver);

/* Module information */
MODULE_DESCRIPTION("MT8173 evb ALSA SoC machine driver");
MODULE_AUTHOR("Koro Chen <koro.chen@mediatek.com>");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:mt-snd-card");
MODULE_DEVICE_TABLE(of, mt8173_evb_dt_match);

