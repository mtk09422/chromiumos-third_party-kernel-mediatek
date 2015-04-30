/*
 * mt8173-rt5650-rt5676.c  --  MT8173 machine driver with RT5650/5676 codecs
 *
 * Copyright (c) 2015 MediaTek Inc.
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

#include <linux/module.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/regulator/consumer.h>
#include <sound/soc.h>
#include <sound/jack.h>
#include "../codecs/rt5645.h"
#include "../codecs/rt5677.h"

#define MCLK_FOR_CODECS		12288000

#define GPIO_RT5650_MCLK_EN	(135 + 16)
#define GPIO_RT5676_MCLK_EN	(135 + 17)

static int mt8173_rt5650_rt5676_event(struct snd_soc_dapm_widget *w,
				      struct snd_kcontrol *kcontrol, int event)
{
/* wait pmic gpio ready */
/*
	gpio_set_value(GPIO_RT5650_MCLK_EN, SND_SOC_DAPM_EVENT_ON(event));
	gpio_set_value(GPIO_RT5676_MCLK_EN, SND_SOC_DAPM_EVENT_ON(event));
*/
	return 0;
}

static const struct snd_soc_dapm_widget mt8173_rt5650_rt5676_widgets[] = {
	SND_SOC_DAPM_SPK("Speaker", mt8173_rt5650_rt5676_event),
	SND_SOC_DAPM_MIC("Int Mic", mt8173_rt5650_rt5676_event),
	SND_SOC_DAPM_HP("Headphone", mt8173_rt5650_rt5676_event),
	SND_SOC_DAPM_MIC("Headset Mic", mt8173_rt5650_rt5676_event),
};

static const struct snd_soc_dapm_route mt8173_rt5650_rt5676_routes[] = {
	{"Speaker", NULL, "SPOL"},
	{"Speaker", NULL, "SPOR"},
	{"Speaker", NULL, "Sub AIF2TX"}, /* IF2 ADC to 5650  */
	{"Sub DMIC L1", NULL, "Int Mic"}, /* DMIC from 5676 */
	{"Sub DMIC R1", NULL, "Int Mic"},
	{"Headphone", NULL, "HPOL"},
	{"Headphone", NULL, "HPOR"},
	{"Headphone", NULL, "Sub AIF2TX"}, /* IF2 ADC to 5650  */
	{"Headset Mic", NULL, "micbias1"},
	{"Headset Mic", NULL, "micbias2"},
	{"IN1P", NULL, "Headset Mic"},
	{"IN1N", NULL, "Headset Mic"},
	{"Sub AIF2RX", NULL, "Headset Mic"}, /* IF2 DAC from 5650  */
};

static const struct snd_kcontrol_new mt8173_rt5650_rt5676_controls[] = {
	SOC_DAPM_PIN_SWITCH("Speaker"),
	SOC_DAPM_PIN_SWITCH("Int Mic"),
	SOC_DAPM_PIN_SWITCH("Headphone"),
	SOC_DAPM_PIN_SWITCH("Headset Mic"),
};

static int mt8173_rt5650_rt5676_hw_params(struct snd_pcm_substream *substream,
					  struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	int i, ret;

	for (i = 0; i < rtd->num_codecs; i++) {
		struct snd_soc_dai *codec_dai = rtd->codec_dais[i];

		/* pll from mclk 12.288M */
		ret = snd_soc_dai_set_pll(codec_dai, 0, 0, MCLK_FOR_CODECS,
					  params_rate(params) * 512);
		if (ret)
			return ret;

		/* sysclk from pll */
		ret = snd_soc_dai_set_sysclk(codec_dai, 1,
					     params_rate(params) * 512,
					     SND_SOC_CLOCK_IN);
		if (ret)
			return ret;
	}
	return 0;
}

static struct snd_soc_ops mt8173_rt5650_rt5676_ops = {
	.hw_params = mt8173_rt5650_rt5676_hw_params,
};

static struct snd_soc_jack mt8173_rt5650_rt5676_hp_jack;
static struct snd_soc_jack mt8173_rt5650_rt5676_mic_jack;

static int mt8173_rt5650_rt5676_init(struct snd_soc_pcm_runtime *runtime)
{
	struct snd_soc_card *card = runtime->card;
	struct snd_soc_codec *codec = runtime->codec_dais[0]->codec;
	struct snd_soc_codec *codec_sub = runtime->codec_dais[1]->codec;
	int ret;

	rt5645_sel_asrc_clk_src(codec,
				RT5645_DA_STEREO_FILTER |
				RT5645_AD_STEREO_FILTER,
				RT5645_CLK_SEL_I2S1_ASRC);
	rt5677_sel_asrc_clk_src(codec_sub,
				RT5677_DA_STEREO_FILTER |
				RT5677_AD_STEREO1_FILTER,
				RT5677_CLK_SEL_I2S1_ASRC);
	rt5677_sel_asrc_clk_src(codec_sub,
				RT5677_AD_STEREO2_FILTER |
				RT5677_I2S2_SOURCE,
				RT5677_CLK_SEL_I2S2_ASRC);

	/* enable jack detection */
	ret = snd_soc_jack_new(codec, "Headphone Jack", SND_JACK_HEADPHONE,
			       &mt8173_rt5650_rt5676_hp_jack);
	if (ret) {
		dev_err(card->dev, "Can't new HP Jack %d\n", ret);
		return ret;
	}
	ret = snd_soc_jack_new(codec, "Mic Jack", SND_JACK_MICROPHONE,
			       &mt8173_rt5650_rt5676_mic_jack);
	if (ret) {
		dev_err(card->dev, "Can't new Mic Jack %d\n", ret);
		return ret;
	}

	return rt5645_set_jack_detect(codec, &mt8173_rt5650_rt5676_hp_jack,
				      &mt8173_rt5650_rt5676_mic_jack);
}

static struct snd_soc_dai_link_component mt8173_rt5650_rt5676_codecs[] = {
	{
		.dai_name = "rt5645-aif1",
	},
	{
		.dai_name = "rt5677-aif1",
	},
};

/* Digital audio interface glue - connects codec <---> CPU */
static struct snd_soc_dai_link mt8173_rt5650_rt5676_dais[] = {
	{
		.name = "rt5650_rt5676",
		.stream_name = "rt5650_rt5676 PCM",
		.cpu_dai_name = "I2S",
		.platform_name = "11220000.mt8173-afe-pcm",
		.codecs = mt8173_rt5650_rt5676_codecs,
		.num_codecs = 2,
		.init = mt8173_rt5650_rt5676_init,
		.dai_fmt = SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_NB_NF |
			   SND_SOC_DAIFMT_CBS_CFS,
		.ops = &mt8173_rt5650_rt5676_ops,
		.ignore_pmdown_time = 1,
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

static struct snd_soc_codec_conf mt8173_rt5650_rt5676_codec_conf[] = {
	{
		.name_prefix = "Sub",
	},
};

static struct snd_soc_card mt8173_rt5650_rt5676_card = {
	.name = "mtk-rt5650-rt5676",
	.dai_link = mt8173_rt5650_rt5676_dais,
	.num_links = ARRAY_SIZE(mt8173_rt5650_rt5676_dais),
	.codec_conf = mt8173_rt5650_rt5676_codec_conf,
	.num_configs = ARRAY_SIZE(mt8173_rt5650_rt5676_codec_conf),
	.controls = mt8173_rt5650_rt5676_controls,
	.num_controls = ARRAY_SIZE(mt8173_rt5650_rt5676_controls),
	.dapm_widgets = mt8173_rt5650_rt5676_widgets,
	.num_dapm_widgets = ARRAY_SIZE(mt8173_rt5650_rt5676_widgets),
	.dapm_routes = mt8173_rt5650_rt5676_routes,
	.num_dapm_routes = ARRAY_SIZE(mt8173_rt5650_rt5676_routes),
};

static int mt8173_rt5650_rt5676_dev_probe(struct platform_device *pdev)
{
	struct snd_soc_card *card = &mt8173_rt5650_rt5676_card;
	struct regulator *reg_vgp1;
	int ret;

	mt8173_rt5650_rt5676_codecs[0].of_node =
		of_parse_phandle(pdev->dev.of_node, "mediatek,audio-codec", 0);
	if (!mt8173_rt5650_rt5676_codecs[0].of_node) {
		dev_err(&pdev->dev,
			"Property 'audio-codec' missing or invalid\n");
		return -EINVAL;
	}
	mt8173_rt5650_rt5676_codecs[1].of_node =
		of_parse_phandle(pdev->dev.of_node, "mediatek,audio-codec", 1);
	if (!mt8173_rt5650_rt5676_codecs[1].of_node) {
		dev_err(&pdev->dev,
			"Property 'audio-codec' missing or invalid\n");
		return -EINVAL;
	}
	mt8173_rt5650_rt5676_codec_conf[0].of_node =
		mt8173_rt5650_rt5676_codecs[1].of_node;

/* wait pmic gpio ready */
/*
	ret = devm_gpio_request_one(&pdev->dev,
				    GPIO_RT5650_MCLK_EN,
				    GPIOF_OUT_INIT_LOW, "mclk_en_5650");
	if (ret) {
		dev_err(&pdev->dev, "%s mclk_en_5650 req fail %d\n",
			__func__, ret);
		return ret;
	}
	ret = devm_gpio_request_one(&pdev->dev,
				    GPIO_RT5676_MCLK_EN,
				    GPIOF_OUT_INIT_LOW, "mclk_en_5676");
	if (ret) {
		dev_err(&pdev->dev, "%s mclk_en_5676 req fail %d\n",
			__func__, ret);
		return ret;
	}
*/
	/* set 5650 codec AVDD/DACREF/CPVDD/DBVDD voltage */
	reg_vgp1 = devm_regulator_get(&pdev->dev, "reg-vgp1");
	if (IS_ERR(reg_vgp1)) {
		dev_err(&pdev->dev, "failed to get reg-vgp1\n");
		return PTR_ERR(reg_vgp1);
	}
	ret = regulator_set_voltage(reg_vgp1, 1800000, 1800000);
	if (ret != 0) {
		dev_err(&pdev->dev, "Failed to set reg-vgp1: %d\n", ret);
		return ret;
	}
	dev_info(&pdev->dev, "reg-vgp1 = %d uv\n",
		 regulator_get_voltage(reg_vgp1));

	ret = regulator_enable(reg_vgp1);
	if (ret != 0) {
		dev_err(&pdev->dev, "Failed to enable reg-vgp1: %d\n", ret);
		return ret;
	}

	card->dev = &pdev->dev;
	platform_set_drvdata(pdev, card);

	ret = snd_soc_register_card(card);
	if (ret)
		dev_err(&pdev->dev, "%s snd_soc_register_card fail %d\n",
			__func__, ret);
	return ret;
}

static int mt8173_rt5650_rt5676_dev_remove(struct platform_device *pdev)
{
	struct snd_soc_card *card = platform_get_drvdata(pdev);

	snd_soc_unregister_card(card);
	return 0;
}

static const struct of_device_id mt8173_rt5650_rt5676_dt_match[] = {
	{ .compatible = "mediatek,mt8173-rt5650-rt5676", },
	{ }
};

static struct platform_driver mt8173_rt5650_rt5676_driver = {
	.driver = {
		   .name = "mtk-rt5650-rt5676",
		   .owner = THIS_MODULE,
		   .of_match_table = mt8173_rt5650_rt5676_dt_match,
#ifdef CONFIG_PM
		   .pm = &snd_soc_pm_ops,
#endif
	},
	.probe = mt8173_rt5650_rt5676_dev_probe,
	.remove = mt8173_rt5650_rt5676_dev_remove,
};

module_platform_driver(mt8173_rt5650_rt5676_driver);

/* Module information */
MODULE_DESCRIPTION("MT8173 RT5650 and RT5676 SoC machine driver");
MODULE_AUTHOR("Koro Chen <koro.chen@mediatek.com>");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:mtk-rt5650-rt5676");
MODULE_DEVICE_TABLE(of, mt8173_rt5650_rt5676_dt_match);

