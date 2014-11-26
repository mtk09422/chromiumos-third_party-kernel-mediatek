/*
 * mt8135-max98090.c  --  MT8135 MAX98090 ALSA SoC machine driver
 *
 * Copyright (c) 2014 MediaTek Inc.
 * Author: Koro Chen <koro.chen@mediatek.com>
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

#include "mt_afe_clk.h"
#include "mt_afe_control.h"
#include <linux/module.h>
#include <sound/soc.h>
#include <sound/jack.h>
#include <linux/gpio.h>
#include <linux/regulator/consumer.h>
#include "../codecs/max98090.h"

#define MCLK_FOR_MAX98090	12288000
#define MCLK_EN_GPIO		118

static struct snd_soc_jack mt8135_max98090_jack;

static struct snd_soc_jack_pin mt8135_max98090_jack_pins[] = {
	{
		.pin	= "Headphone",
		.mask	= SND_JACK_HEADPHONE,
	},
	{
		.pin	= "Headset Mic",
		.mask	= SND_JACK_MICROPHONE,
	},
};

static int mt8135_max98090_event(struct snd_soc_dapm_widget *w,
				 struct snd_kcontrol *kcontrol, int event)
{
	gpio_set_value(MCLK_EN_GPIO, SND_SOC_DAPM_EVENT_ON(event));
	return 0;
}

static const struct snd_soc_dapm_widget mt8135_max98090_widgets[] = {
	SND_SOC_DAPM_SPK("Speaker", mt8135_max98090_event),
	SND_SOC_DAPM_MIC("Buildin Mic", mt8135_max98090_event),
	SND_SOC_DAPM_HP("Headphone", mt8135_max98090_event),
	SND_SOC_DAPM_MIC("Headset Mic", mt8135_max98090_event),
};

static const struct snd_soc_dapm_route mt8135_max98090_routes[] = {
	{"Speaker", NULL, "SPKL"},
	{"Speaker", NULL, "SPKR"},
	{"DMICL", NULL, "Buildin Mic"},
	{"Headphone", NULL, "HPL"},
	{"Headphone", NULL, "HPR"},
	{"Headset Mic", NULL, "MICBIAS"},
	{"IN34", NULL, "Headset Mic"},
};

static const struct snd_kcontrol_new mt8135_max98090_controls[] = {
	SOC_DAPM_PIN_SWITCH("Speaker"),
	SOC_DAPM_PIN_SWITCH("Buildin Mic"),
	SOC_DAPM_PIN_SWITCH("Headphone"),
	SOC_DAPM_PIN_SWITCH("Headset Mic"),
};

static int mt8135_max98090_init(struct snd_soc_pcm_runtime *runtime)
{
	int ret;
	struct snd_soc_card *card = runtime->card;
	struct snd_soc_codec *codec = runtime->codec;

	ret = snd_soc_dai_set_sysclk(runtime->codec_dai, 0, MCLK_FOR_MAX98090,
				     SND_SOC_CLOCK_IN);
	if (ret) {
		dev_err(card->dev, "Can't set max98090 clock %d\n", ret);
		return ret;
	}

	/*enable jack detection*/
	ret = snd_soc_jack_new(codec, "Headphone", SND_JACK_HEADPHONE,
			       &mt8135_max98090_jack);
	if (ret) {
		dev_err(card->dev, "Can't snd_soc_jack_new %d\n", ret);
		return ret;
	}

	ret = snd_soc_jack_add_pins(&mt8135_max98090_jack,
				    ARRAY_SIZE(mt8135_max98090_jack_pins),
				    mt8135_max98090_jack_pins);
	if (ret) {
		dev_err(card->dev, "Can't snd_soc_jack_add_pins %d\n", ret);
		return ret;
	}

	return max98090_mic_detect(codec, &mt8135_max98090_jack);
}

/* Digital audio interface glue - connects codec <---> CPU */
static struct snd_soc_dai_link mt8135_max98090_dais[] = {
	{
		.name = "Playback",
		.stream_name = "Playback",
		.cpu_dai_name = "DL1",
		.platform_name = "mt8135-afe-pcm",
		.codec_dai_name = "HiFi",
		.codec_name = "max98090.1-0010",
		.init = mt8135_max98090_init,
		.dai_fmt = SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_NB_NF |
			   SND_SOC_DAIFMT_CBS_CFS,

	},
	{
		.name = "Capture",
		.stream_name = "Capture",
		.cpu_dai_name = "AWB",
		.platform_name = "mt8135-afe-pcm",
		.codec_dai_name = "HiFi",
		.codec_name = "max98090.1-0010",
	},
	{
		.name = "HDMI Playback",
		.stream_name = "HDMI Playback",
		.cpu_dai_name = "HDMI",
		.platform_name = "mt8135-afe-pcm",
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
	},
};

static struct snd_soc_card mt8135_max98090_card = {
	.name = "mt8135-max98090",
	.dai_link = mt8135_max98090_dais,
	.num_links = ARRAY_SIZE(mt8135_max98090_dais),
	.controls = mt8135_max98090_controls,
	.num_controls = ARRAY_SIZE(mt8135_max98090_controls),
	.dapm_widgets = mt8135_max98090_widgets,
	.num_dapm_widgets = ARRAY_SIZE(mt8135_max98090_widgets),
	.dapm_routes = mt8135_max98090_routes,
	.num_dapm_routes = ARRAY_SIZE(mt8135_max98090_routes),
};

static int mt8135_max98090_dev_probe(struct platform_device *pdev)
{
	struct snd_soc_card *card = &mt8135_max98090_card;
	struct device_node *codec_node;
	struct regulator *reg_vgp1, *reg_vgp4;
	unsigned int volt;
	int ret, i;

	if (pdev->dev.of_node) {
		dev_set_name(&pdev->dev, "%s", "mt8135-max98090");
		dev_notice(&pdev->dev, "%s set dev name %s\n", __func__,
			   dev_name(&pdev->dev));
	}
	codec_node = of_parse_phandle(pdev->dev.of_node,
				      "mediatek,audio-codec", 0);
	if (!codec_node) {
		dev_err(&pdev->dev,
			"Property 'audio-codec' missing or invalid\n");
	} else {
		for (i = 0; i < card->num_links; i++) {
			if (mt8135_max98090_dais[i].codec_name &&
			    !strcmp(mt8135_max98090_dais[i].codec_name,
				    "snd-soc-dummy"))
				continue;
			mt8135_max98090_dais[i].codec_name = NULL;
			mt8135_max98090_dais[i].codec_of_node = codec_node;
		}
	}
	ret = gpio_request_one(MCLK_EN_GPIO, GPIOF_OUT_INIT_LOW, "mclk_en");
	if (ret) {
		dev_err(&pdev->dev, "%s gpio_request_one fail %d\n",
			__func__, ret);
		return ret;
	}
	/*enable power for MAX98090*/
	reg_vgp1 = devm_regulator_get(&pdev->dev, "reg-vgp1");
	if (IS_ERR(reg_vgp1)) {
		ret = PTR_ERR(reg_vgp1);
		dev_err(&pdev->dev, "failed to get reg_vgp1\n");
		return ret;
	}
	/* set voltage to 1.2V */
	volt = regulator_get_voltage(reg_vgp1);
	ret = regulator_set_voltage(reg_vgp1, 1220000, 1220000);
	if (ret != 0) {
		dev_err(&pdev->dev,
			"Failed to set reg_vgp1 voltage: %d\n", ret);
		return ret;
	}
	dev_info(&pdev->dev, "reg_vgp1 voltage = %d->%d\n",
		 volt, regulator_get_voltage(reg_vgp1));

	ret = regulator_enable(reg_vgp1);
	if (ret != 0) {
		dev_err(&pdev->dev, "Failed to enable reg_vgp1: %d\n", ret);
		return ret;
	}

	reg_vgp4 = devm_regulator_get(&pdev->dev, "reg-vgp4");
	if (IS_ERR(reg_vgp4)) {
		ret = PTR_ERR(reg_vgp4);
		dev_err(&pdev->dev, "failed to get reg_vgp4\n");
		return ret;
	}
	/* set voltage to 1.8V */
	volt = regulator_get_voltage(reg_vgp4);
	ret = regulator_set_voltage(reg_vgp4, 1800000, 1800000);
	if (ret != 0) {
		dev_err(&pdev->dev,
			"Failed to set reg_vgp4 voltage: %d\n", ret);
		return ret;
	}
	dev_info(&pdev->dev, "reg_vgp4 voltage = %d->%d\n",
		 volt, regulator_get_voltage(reg_vgp4));

	ret = regulator_enable(reg_vgp4);
	if (ret != 0) {
		dev_err(&pdev->dev, "Failed to enable reg_vgp4: %d\n", ret);
		return ret;
	}


	card->dev = &pdev->dev;

	ret = snd_soc_register_card(card);
	if (ret) {
		dev_err(&pdev->dev, "%s snd_soc_register_card fail %d\n",
			__func__, ret);
		return ret;
	}
	return 0;
}

static int mt8135_max98090_dev_remove(struct platform_device *pdev)
{
	struct snd_soc_card *card = platform_get_drvdata(pdev);

	gpio_free(MCLK_EN_GPIO);
	snd_soc_unregister_card(card);
	return 0;
}

static const struct of_device_id mt8135_max98090_dt_match[] = {
	{ .compatible = "mediatek,mt8135-max98090", },
	{ }
};

static struct platform_driver mt8135_max98090_driver = {
	.driver = {
		   .name = "mt8135-max98090",
		   .owner = THIS_MODULE,
		   .of_match_table = mt8135_max98090_dt_match,
#ifdef CONFIG_PM
		   .pm = &snd_soc_pm_ops,
#endif
		   },
	.probe = mt8135_max98090_dev_probe,
	.remove = mt8135_max98090_dev_remove,
};

module_platform_driver(mt8135_max98090_driver);

/* Module information */
MODULE_DESCRIPTION("MT8135 MAX98090 ALSA SoC machine driver");
MODULE_AUTHOR("Koro Chen <koro.chen@mediatek.com>");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:mt8135-max98090");
MODULE_DEVICE_TABLE(of, mt8135_max98090_dt_match);

