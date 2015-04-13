/*
 * mt6397.c  --  MT6397 ALSA SoC codec driver
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

#include "mt6397.h"
#include <linux/module.h>
#include <sound/soc.h>
#include <linux/mfd/mt6397/core.h>
#include <linux/delay.h>
#include <linux/debugfs.h>

static struct dentry *mt6397_debugfs;
int mt6397_clk_cntr; /* FIXME todo use new ccf */

static DEFINE_MUTEX(mt6397_ctrl_mutex);
static DEFINE_MUTEX(mt6397_clk_mutex);

/* Function implementation */
static void mt6397_set_reg(struct mt6397_codec_priv *mt6397_data,
			   uint32_t offset, uint32_t value, uint32_t mask)
{
	snd_soc_update_bits(mt6397_data->mt6397_codec, offset, mask, value);
}

static void mt6397_enable_clk(struct mt6397_codec_priv *mt6397_data,
			      uint32_t mask, bool enable)
{
	/* set pmic register or analog CONTROL_IFACE_PATH */
	uint32_t val;
	uint32_t reg = enable ? MT6397_TOP_CKPDN_CLR : MT6397_TOP_CKPDN_SET;

	snd_soc_update_bits(mt6397_data->mt6397_codec, reg, mask, mask);
	val = snd_soc_read(mt6397_data->mt6397_codec, MT6397_TOP_CKPDN);
	if (val < 0)
		dev_err(mt6397_data->mt6397_codec->dev, "error mt6397_enable_clk\n");

	if ((val & mask) != (enable ? 0 : mask))
		dev_err(mt6397_data->mt6397_codec->dev, "%s: data mismatch: mask=%04X, val=%04X, enable=%d\n",
			__func__, mask, val, enable);
}

static uint32_t mt6397_get_reg(struct mt6397_codec_priv *mt6397_data,
			       uint32_t offset)
{
	uint32_t ret;

	ret = snd_soc_read(mt6397_data->mt6397_codec, offset);
	if (ret < 0)
		dev_err(mt6397_data->mt6397_codec->dev, "error mt6397_get_reg\n");
	return ret;
}

static void mt6397_clk_on(struct mt6397_codec_priv *mt6397_data)
{
	mutex_lock(&mt6397_clk_mutex);
	if (mt6397_clk_cntr == 0) {
		dev_dbg(mt6397_data->mt6397_codec->dev,
			"+%s mt6397_clk_cntr:%d\n", __func__, mt6397_clk_cntr);
		/*RG_CLKSQ_EN*/
		mt6397_set_reg(mt6397_data, MT6397_TOP_CKCON1, 0x0010, 0x0010);
		mt6397_enable_clk(mt6397_data, 0x0003, true);
	}
	mt6397_clk_cntr++;
	mutex_unlock(&mt6397_clk_mutex);
	dev_dbg(mt6397_data->mt6397_codec->dev,
		"-%s mt6397_clk_cntr:%d\n", __func__, mt6397_clk_cntr);
}

static void mt6397_clk_off(struct mt6397_codec_priv *mt6397_data)
{
	mutex_lock(&mt6397_clk_mutex);
	mt6397_clk_cntr--;
	if (mt6397_clk_cntr == 0) {
		dev_dbg(mt6397_data->mt6397_codec->dev,
			"+%s Ana clk(%x)\n", __func__, mt6397_clk_cntr);
		/* Disable ADC clock */
		/*RG_CLKSQ_EN*/
		mt6397_set_reg(mt6397_data, MT6397_TOP_CKCON1, 0x0000, 0x0010);
		mt6397_enable_clk(mt6397_data, 0x0003, false);

	} else if (mt6397_clk_cntr < 0) {
		dev_err(mt6397_data->mt6397_codec->dev, "!!Aud_ADC_Clk_cntr<0 (%d)\n",
			mt6397_clk_cntr);
		mt6397_clk_cntr = 0;
	}
	mutex_unlock(&mt6397_clk_mutex);
	dev_dbg(mt6397_data->mt6397_codec->dev,
		"-%s , Aud_ADC_Clk_cntr:%d\n", __func__, mt6397_clk_cntr);
}

uint32_t mt6397_ul_fs(struct mt6397_codec_priv *mt6397_data)
{
	uint32_t reg_value = 0;
	uint32_t frequency = mt6397_data->mt6397_fs[MTCODEC_DEVICE_ADC];

	dev_dbg(mt6397_data->mt6397_codec->dev,
		"AudCodec mt6397_ul_fs ApplyDLFrequency = %d", frequency);

	switch (frequency) {
	case 8000:
		reg_value = 0x0 << 1;
		break;
	case 16000:
		reg_value = 0x5 << 1;
		break;
	case 32000:
		reg_value = 0xa << 1;
		break;
	case 48000:
		reg_value = 0xf << 1;
		break;
	default:
		dev_warn(mt6397_data->mt6397_codec->dev, "AudCodec mt6397_ul_fs with unsupported frequency = %d",
			 frequency);
	}
	dev_dbg(mt6397_data->mt6397_codec->dev,
		"AudCodec mt6397_ul_fs reg_value = %d", reg_value);
	return reg_value;
}

static int mt6397_codec_startup(struct snd_pcm_substream *substream,
				struct snd_soc_dai *dai_port)
{
	return 0;
}

static int mt6397_codec_prepare(struct snd_pcm_substream *substream,
				struct snd_soc_dai *dai_port)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct mt6397_codec_priv *mt6397_data =
		snd_soc_codec_get_drvdata(rtd->codec);

	if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
		dev_dbg(mt6397_data->mt6397_codec->dev,
			"AudCodec %s set up SNDRV_PCM_STREAM_CAPTURE rate = %d\n",
			__func__, substream->runtime->rate);
		mt6397_data->mt6397_fs[MTCODEC_DEVICE_ADC] =
			substream->runtime->rate;

	} else if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		dev_dbg(mt6397_data->mt6397_codec->dev,
			"AudCodec %s set up SNDRV_PCM_STREAM_PLAYBACK rate = %d\n",
			__func__, substream->runtime->rate);
		mt6397_data->mt6397_fs[MTCODEC_DEVICE_DAC] =
			substream->runtime->rate;
	}
	return 0;
}

static int mt6397_codec_trigger(struct snd_pcm_substream *substream,
				int command, struct snd_soc_dai *dai_port)
{
	/* struct snd_pcm_runtime *runtime = substream->runtime; */
	switch (command) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
		break;
	}

	return 0;
}

static const struct snd_soc_dai_ops mt6397_aif1_dai_ops = {
	.startup = mt6397_codec_startup,
	.prepare = mt6397_codec_prepare,
	.trigger = mt6397_codec_trigger,
};

static struct snd_soc_dai_driver mt6397_dai_drvs[] = {
	{
		.name = "mt-soc-codec-tx-dai",
		.ops = &mt6397_aif1_dai_ops,
		.playback = {
			.stream_name = "Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_8000_48000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
		},
	},
	{
		.name = "mt-soc-codec-rx-dai",
		.ops = &mt6397_aif1_dai_ops,
		.capture = {
			.stream_name = "Capture",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_8000_48000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
		},
	},
};

uint32_t mt6397_dl_fs(struct mt6397_codec_priv *mt6397_data)
{
	uint32_t reg_value = 0;
	unsigned int frequency = mt6397_data->mt6397_fs[MTCODEC_DEVICE_DAC];

	dev_dbg(mt6397_data->mt6397_codec->dev,
		"AudCodec %s frequency = %d\n", __func__, frequency);
	switch (frequency) {
	case 8000:
		reg_value = 0;
		break;
	case 11025:
		reg_value = 1;
		break;
	case 12000:
		reg_value = 2;
		break;
	case 16000:
		reg_value = 3;
		break;
	case 22050:
		reg_value = 4;
		break;
	case 24000:
		reg_value = 5;
		break;
	case 32000:
		reg_value = 6;
		break;
	case 44100:
		reg_value = 7;
		break;
	case 48000:
		reg_value = 8;
		break;
	default:
		dev_warn(mt6397_data->mt6397_codec->dev, "AudCodec %s unexpected frequency = %d\n",
			 __func__, frequency);
	}
	return reg_value;
}

static bool mt6397_get_dl_status(struct mt6397_codec_priv *mt6397_data)
{
	int i = 0;

	for (i = 0; i < MTCODEC_DEVICE_2IN1_SPK; i++) {
		if (mt6397_data->power[i])
			return true;
	}
	return false;
}

static bool mt6397_get_adc_status(struct mt6397_codec_priv *mt6397_data)
{
	int i = 0;

	for (i = MTCODEC_DEVICE_ADC1; i < MTCODEC_DEVICE_ADC3; i++) {
		if (mt6397_data->power[i])
			return true;
	}
	return false;
}

static bool mt6397_get_uplink_status(struct mt6397_codec_priv *mt6397_data)
{
	int i = 0;

	for (i = MTCODEC_DEVICE_ADC1; i < MTCODEC_DEVICE_MAX; i++) {
		if (mt6397_data->power[i])
			return true;
	}
	return false;
}

static void mt6397_mux(struct mt6397_codec_priv *mt6397_data,
		       enum MTCODEC_DEVICE_TYPE device_type,
		       enum MTCODEC_MUX_TYPE mux_type)
{
	uint32_t reg_value = 0;

	switch (device_type) {
	case MTCODEC_DEVICE_OUT_EARPIECEL:
	case MTCODEC_DEVICE_OUT_EARPIECER:
		if (mux_type == MTCODEC_MUX_OPEN) {
			reg_value = 0;
		} else if (mux_type == MTCODEC_MUX_MUTE) {
			reg_value = 1 << 3;
		} else if (mux_type == MTCODEC_MUX_VOICE) {
			reg_value = 2 << 3;
		} else {
			reg_value = 2 << 3;
			dev_warn(mt6397_data->mt6397_codec->dev, "AudCodec mt6397_mux: %d %d\n",
				 device_type, mux_type);
		}
		mt6397_set_reg(mt6397_data, MT6397_AUDBUF_CFG0,
			       reg_value, 0x000000018);
		break;
	case MTCODEC_DEVICE_OUT_HSL:
		if (mux_type == MTCODEC_MUX_OPEN) {
			reg_value = 0;
		} else if (mux_type == MTCODEC_MUX_LINEIN_L) {
			reg_value = 1 << 5;
		} else if (mux_type == MTCODEC_MUX_LINEIN_R) {
			reg_value = 2 << 5;
		} else if (mux_type == MTCODEC_MUX_LINEIN_STEREO) {
			reg_value = 3 << 5;
		} else if (mux_type == MTCODEC_MUX_AUDIO) {
			reg_value = 4 << 5;
		} else if (mux_type == MTCODEC_MUX_LINEIN_AUDIO_MONO) {
			reg_value = 5 << 5;
		} else if (mux_type == MTCODEC_MUX_IV_BUFFER) {
			reg_value = 8 << 5;
		} else {
			reg_value = 4 << 5;
			dev_warn(mt6397_data->mt6397_codec->dev, "AudCodec mt6397_mux: %d %d\n",
				 device_type, mux_type);
		}
		mt6397_set_reg(mt6397_data, MT6397_AUDBUF_CFG0,
			       reg_value, 0x000001e0);
		break;
	case MTCODEC_DEVICE_OUT_HSR:
		if (mux_type == MTCODEC_MUX_OPEN) {
			reg_value = 0;
		} else if (mux_type == MTCODEC_MUX_LINEIN_L) {
			reg_value = 1 << 9;
		} else if (mux_type == MTCODEC_MUX_LINEIN_R) {
			reg_value = 2 << 9;
		} else if (mux_type == MTCODEC_MUX_LINEIN_STEREO) {
			reg_value = 3 << 9;
		} else if (mux_type == MTCODEC_MUX_AUDIO) {
			reg_value = 4 << 9;
		} else if (mux_type == MTCODEC_MUX_LINEIN_AUDIO_MONO) {
			reg_value = 5 << 9;
		} else if (mux_type == MTCODEC_MUX_IV_BUFFER) {
			reg_value = 8 << 9;
		} else {
			reg_value = 4 << 9;
			dev_warn(mt6397_data->mt6397_codec->dev, "AudCodec mt6397_mux: %d %d\n",
				 device_type, mux_type);
		}
		mt6397_set_reg(mt6397_data, MT6397_AUDBUF_CFG0,
			       reg_value, 0x00001e00);
		break;
	case MTCODEC_DEVICE_OUT_SPKR:
	case MTCODEC_DEVICE_OUT_SPKL:
	case MTCODEC_DEVICE_OUT_SPK_HS_R:
	case MTCODEC_DEVICE_OUT_SPK_HS_L:
		if (mux_type == MTCODEC_MUX_OPEN) {
			reg_value = 0;
		} else if ((mux_type == MTCODEC_MUX_LINEIN_L) ||
			   (mux_type == MTCODEC_MUX_LINEIN_R)) {
			reg_value = 1 << 2;
		} else if (mux_type == MTCODEC_MUX_LINEIN_STEREO) {
			reg_value = 2 << 2;
		} else if (mux_type == MTCODEC_MUX_OPEN) {
			reg_value = 3 << 2;
		} else if (mux_type == MTCODEC_MUX_AUDIO) {
			reg_value = 4 << 2;
		} else if (mux_type == MTCODEC_MUX_LINEIN_AUDIO_MONO) {
			reg_value = 5 << 2;
		} else if (mux_type == MTCODEC_MUX_LINEIN_AUDIO_STEREO) {
			reg_value = 6 << 2;
		} else {
			reg_value = 4 << 2;
			dev_warn(mt6397_data->mt6397_codec->dev, "AudCodec mt6397_mux: %d %d\n",
				 device_type, mux_type);
		}
		mt6397_set_reg(mt6397_data, MT6397_AUD_IV_CFG0,
			       reg_value | (reg_value << 8), 0x00001c1c);
		break;
	case MTCODEC_DEVICE_PREAMP_L:
		if (mux_type == MTCODEC_MUX_MIC1) {
			reg_value = 1 << 2;
		} else if (mux_type == MTCODEC_MUX_MIC2) {
			reg_value = 2 << 2;
		} else if (mux_type == MTCODEC_MUX_MIC3) {
			reg_value = 3 << 2;
		} else {
			reg_value = 1 << 2;
			dev_warn(mt6397_data->mt6397_codec->dev, "AudCodec mt6397_mux: %d %d\n",
				 device_type, mux_type);
		}
		mt6397_set_reg(mt6397_data, MT6397_AUDPREAMP_CON0,
			       reg_value, 0x0000001c);
		break;
	case MTCODEC_DEVICE_PREAMP_R:
		if (mux_type == MTCODEC_MUX_MIC1) {
			reg_value = 1 << 5;
		} else if (mux_type == MTCODEC_MUX_MIC2) {
			reg_value = 2 << 5;
		} else if (mux_type == MTCODEC_MUX_MIC3) {
			reg_value = 3 << 5;
		} else {
			reg_value = 1 << 5;
			dev_warn(mt6397_data->mt6397_codec->dev, "AudCodec mt6397_mux: %d %d\n",
				 device_type, mux_type);
		}
		mt6397_set_reg(mt6397_data, MT6397_AUDPREAMP_CON0,
			       reg_value, 0x000000e0);
		break;
	case MTCODEC_DEVICE_ADC1:
		if (mux_type == MTCODEC_MUX_MIC1) {
			reg_value = 1 << 2;
		} else if (mux_type == MTCODEC_MUX_PREAMP1) {
			reg_value = 4 << 2;
		} else if (mux_type == MTCODEC_MUX_LEVEL_SHIFT_BUFFER) {
			reg_value = 5 << 2;
		} else {
			reg_value = 1 << 2;
			dev_warn(mt6397_data->mt6397_codec->dev, "AudCodec mt6397_mux: %d %d\n",
				 device_type, mux_type);
		}
		mt6397_set_reg(mt6397_data, MT6397_AUDADC_CON0,
			       reg_value, 0x0000001c);
		break;
	case MTCODEC_DEVICE_ADC2:
		if (mux_type == MTCODEC_MUX_MIC1) {
			reg_value = 1 << 5;
		} else if (mux_type == MTCODEC_MUX_PREAMP2) {
			reg_value = 4 << 5;
		} else if (mux_type == MTCODEC_MUX_LEVEL_SHIFT_BUFFER) {
			reg_value = 5 << 5;
		} else {
			reg_value = 1 << 5;
			dev_warn(mt6397_data->mt6397_codec->dev, "AudCodec mt6397_mux: %d %d\n",
				 device_type, mux_type);
		}
		mt6397_set_reg(mt6397_data, MT6397_AUDADC_CON0,
			       reg_value, 0x000000e0);
		break;
	default:
		break;
	}
}

static void mt6397_turn_on_dac_power(struct mt6397_codec_priv *mt6397_data)
{
	mt6397_set_reg(mt6397_data, MT6397_AFE_PMIC_NEWIF_CFG0,
		       mt6397_dl_fs(mt6397_data) << 12, 0xf000);
	mt6397_set_reg(mt6397_data, MT6397_AFUNC_AUD_CON2, 0x0006, 0xffff);
	mt6397_set_reg(mt6397_data, MT6397_AFUNC_AUD_CON0, 0xc3a1, 0xffff);
	mt6397_set_reg(mt6397_data, MT6397_AFUNC_AUD_CON2, 0x0003, 0xffff);
	mt6397_set_reg(mt6397_data, MT6397_AFUNC_AUD_CON2, 0x000b, 0xffff);
	mt6397_set_reg(mt6397_data, MT6397_AFE_DL_SDM_CON1, 0x001e, 0xffff);
	mt6397_set_reg(mt6397_data, MT6397_AFE_DL_SRC2_CON0_H, 0x0300 |
		     mt6397_dl_fs(mt6397_data) << 12, 0x0ffff);
	mt6397_set_reg(mt6397_data, MT6397_AFE_UL_DL_CON0, 0x007f, 0xffff);
	mt6397_set_reg(mt6397_data, MT6397_AFE_DL_SRC2_CON0_L, 0x1801, 0xffff);
}

static void mt6397_turn_off_dac_power(struct mt6397_codec_priv *mt6397_data)
{
	dev_dbg(mt6397_data->mt6397_codec->dev, "AudCodec mt6397_turn_off_dac_power\n");

	mt6397_set_reg(mt6397_data, MT6397_AFE_DL_SRC2_CON0_L, 0x1800, 0xffff);
	if (!mt6397_get_uplink_status(mt6397_data))
		mt6397_set_reg(mt6397_data, MT6397_AFE_UL_DL_CON0,
			       0x0000, 0xffff);
}

static void mt6397_spk_auto_trim_offset(struct mt6397_codec_priv *mt6397_data)
{
	uint32_t wait_for_ready = 0;
	uint32_t reg1 = 0;
	uint32_t chip_version = 0;
	int retyrcount = 50;

	mt6397_set_reg(mt6397_data, MT6397_AFUNC_AUD_CON2, 0x0080, 0x0080);
	/* enable VA28 , VA 33 VBAT ref , set dc */
	mt6397_set_reg(mt6397_data, MT6397_AUDLDO_CFG0, 0x0D92, 0xffff);
	/* set ACC mode  enable NVREF */
	mt6397_set_reg(mt6397_data, MT6397_AUDNVREGGLB_CFG0, 0x000C, 0xffff);
	/* enable LDO ; fix me , separate for UL  DL LDO */
	mt6397_set_reg(mt6397_data, MT6397_AUD_NCP0, 0xE000, 0xE000);
	/* RG DEV ck on */
	mt6397_set_reg(mt6397_data, MT6397_NCP_CLKDIV_CON0, 0x102B, 0xffff);
	/* NCP on */
	mt6397_set_reg(mt6397_data, MT6397_NCP_CLKDIV_CON1, 0x0000, 0xffff);
	usleep_range(200, 210);
	/* ZCD setting gain step gain and enable */
	mt6397_set_reg(mt6397_data, MT6397_ZCD_CON0, 0x0301, 0xffff);
	/* audio bias adjustment */
	mt6397_set_reg(mt6397_data, MT6397_IBIASDIST_CFG0, 0x0552, 0xffff);
	/* set DUDIV gain ,iv buffer gain */
	mt6397_set_reg(mt6397_data, MT6397_ZCD_CON4, 0x0505, 0xffff);
	/* set IV buffer on */
	mt6397_set_reg(mt6397_data, MT6397_AUD_IV_CFG0, 0x1111, 0xffff);
	usleep_range(100, 110);
	/* reset docoder */
	mt6397_set_reg(mt6397_data, MT6397_AUDCLKGEN_CFG0, 0x0001, 0x0001);
	/* power on DAC */
	mt6397_set_reg(mt6397_data, MT6397_AUDDAC_CON0, 0x000f, 0xffff);
	usleep_range(100, 110);
	mt6397_mux(mt6397_data, MTCODEC_DEVICE_OUT_SPKR, MTCODEC_MUX_AUDIO);
	mt6397_mux(mt6397_data, MTCODEC_DEVICE_OUT_SPKL, MTCODEC_MUX_AUDIO);
	/* set Mux */
	mt6397_set_reg(mt6397_data, MT6397_AUDBUF_CFG0, 0x0000, 0x0007);
	mt6397_set_reg(mt6397_data, MT6397_AFUNC_AUD_CON2, 0x0000, 0x0080);

	/* disable the software register mode */
	mt6397_set_reg(mt6397_data, MT6397_SPK_CON1, 0, 0x7f00);
	/* disable the software register mode */
	mt6397_set_reg(mt6397_data, MT6397_SPK_CON4, 0, 0x7f00);
	/* Choose new mode for trim (E2 Trim) */
	mt6397_set_reg(mt6397_data, MT6397_SPK_CON9, 0x0018, 0xffff);
	/* Enable auto trim */
	mt6397_set_reg(mt6397_data, MT6397_SPK_CON0, 0x0008, 0xffff);
	/* Enable auto trim R */
	mt6397_set_reg(mt6397_data, MT6397_SPK_CON3, 0x0008, 0xffff);
	/* set gain */
	mt6397_set_reg(mt6397_data, MT6397_SPK_CON0, 0x3000, 0xf000);
	/* set gain R */
	mt6397_set_reg(mt6397_data, MT6397_SPK_CON3, 0x3000, 0xf000);
	/* set gain L */
	mt6397_set_reg(mt6397_data, MT6397_SPK_CON9, 0x0100, 0x0f00);
	/* set gain R */
	mt6397_set_reg(mt6397_data, MT6397_SPK_CON5, (0x1 << 11), 0x7800);
	/* Enable amplifier & auto trim */
	mt6397_set_reg(mt6397_data, MT6397_SPK_CON0, 0x0001, 0x0001);
	/* Enable amplifier & auto trim R */
	mt6397_set_reg(mt6397_data, MT6397_SPK_CON3, 0x0001, 0x0001);

	/* empirical data shows it usually takes 13ms to be ready */
	usleep_range(15000, 16000);

	do {
		wait_for_ready = mt6397_get_reg(mt6397_data, MT6397_SPK_CON1);
		wait_for_ready = ((wait_for_ready & 0x8000) >> 15);

		if (wait_for_ready) {
			wait_for_ready = mt6397_get_reg(mt6397_data,
							MT6397_SPK_CON4);
			wait_for_ready = ((wait_for_ready & 0x8000) >> 15);
			if (wait_for_ready)
				break;
		}

		dev_dbg(mt6397_data->mt6397_codec->dev,
			"mt6397_spk_auto_trim_offset sleep\n");
		usleep_range(100, 110);
	} while (retyrcount--);

	if (wait_for_ready)
		dev_dbg(mt6397_data->mt6397_codec->dev,
			"mt6397_spk_auto_trim_offset done\n");
	else
		dev_warn(mt6397_data->mt6397_codec->dev,
			 "mt6397_spk_auto_trim_offset fail\n");

	mt6397_set_reg(mt6397_data, MT6397_SPK_CON9, 0x0, 0xffff);
	/* set gain R */
	mt6397_set_reg(mt6397_data, MT6397_SPK_CON5, 0, 0x7800);
	mt6397_set_reg(mt6397_data, MT6397_SPK_CON0, 0x0000, 0x0001);
	mt6397_set_reg(mt6397_data, MT6397_SPK_CON3, 0x0000, 0x0001);

	/* get trim offset result */
	dev_dbg(mt6397_data->mt6397_codec->dev, "GetSPKAutoTrimOffset ");
	mt6397_set_reg(mt6397_data, MT6397_TEST_CON0, 0x0805, 0xffff);
	reg1 = mt6397_get_reg(mt6397_data, MT6397_TEST_OUT_L);
	mt6397_data->spk_l_trim = ((reg1 >> 0) & 0xf);
	mt6397_set_reg(mt6397_data, MT6397_TEST_CON0, 0x0806, 0xffff);
	reg1 = mt6397_get_reg(mt6397_data, MT6397_TEST_OUT_L);
	mt6397_data->spk_l_trim |= 0xF;
	mt6397_data->spk_l_polarity = ((reg1 >> 1) & 0x1);
	mt6397_set_reg(mt6397_data, MT6397_TEST_CON0, 0x080E, 0xffff);
	reg1 = mt6397_get_reg(mt6397_data, MT6397_TEST_OUT_L);
	mt6397_data->spk_r_trim = ((reg1 >> 0) & 0xf);
	mt6397_set_reg(mt6397_data, MT6397_TEST_CON0, 0x080F, 0xffff);
	reg1 = mt6397_get_reg(mt6397_data, MT6397_TEST_OUT_L);
	mt6397_data->spk_r_trim |= (((reg1 >> 0) & 0x1) << 4);
	mt6397_data->spk_r_polarity = ((reg1 >> 1) & 0x1);

	chip_version = mt6397_get_reg(mt6397_data, MT6397_CID);
	if (chip_version == 0x1097 /* FIXME todo remove hard code */) {
		dev_info(mt6397_data->mt6397_codec->dev, "PMIC is MT6397 E1, set spk R trim code to 0\n");
		mt6397_data->spk_r_trim = 0;
		mt6397_data->spk_r_polarity = 0;
	}

	dev_dbg(mt6397_data->mt6397_codec->dev, "mSPKlpolarity = %d mISPKltrim = 0x%x\n",
		mt6397_data->spk_l_polarity, mt6397_data->spk_l_trim);
	dev_dbg(mt6397_data->mt6397_codec->dev, "mSPKrpolarity = %d mISPKrtrim = 0x%x\n",
		mt6397_data->spk_r_polarity, mt6397_data->spk_r_trim);

	/* turn off speaker after trim */
	mt6397_set_reg(mt6397_data, MT6397_AFUNC_AUD_CON2, 0x0080, 0x0080);
	mt6397_set_reg(mt6397_data, MT6397_SPK_CON0, 0x0000, 0xffff);
	mt6397_set_reg(mt6397_data, MT6397_SPK_CON3, 0x0000, 0xffff);
	mt6397_set_reg(mt6397_data, MT6397_SPK_CON11, 0x0000, 0xffff);

	/* enable LDO ; fix me , separate for UL  DL LDO */
	mt6397_set_reg(mt6397_data, MT6397_AUDCLKGEN_CFG0, 0x0000, 0x0001);
	/* RG DEV ck on */
	mt6397_set_reg(mt6397_data, MT6397_AUDDAC_CON0, 0x0000, 0xffff);
	/* NCP on */
	mt6397_set_reg(mt6397_data, MT6397_AUD_IV_CFG0, 0x0000, 0xffff);
	/* Audio headset power on */
	mt6397_set_reg(mt6397_data, MT6397_IBIASDIST_CFG0, 0x1552, 0xffff);

	mt6397_set_reg(mt6397_data, MT6397_AUDNVREGGLB_CFG0, 0x0006, 0xffff);
	/* fix me */
	mt6397_set_reg(mt6397_data, MT6397_NCP_CLKDIV_CON1, 0x0001, 0xffff);
	mt6397_set_reg(mt6397_data, MT6397_AUD_NCP0, 0x0000, 0x6000);
	mt6397_set_reg(mt6397_data, MT6397_AUDLDO_CFG0, 0x0192, 0xffff);
	mt6397_set_reg(mt6397_data, MT6397_AFUNC_AUD_CON2, 0x0000, 0x0080);
}

static void mt6397_get_trim_offset(struct mt6397_codec_priv *mt6397_data)
{
	uint32_t reg1 = 0, reg2 = 0;
	bool trim_enable = 0;

	/* get to check if trim happen */
	reg1 = mt6397_get_reg(mt6397_data, MT6397_TRIM_ADDRESS1);
	reg2 = mt6397_get_reg(mt6397_data, MT6397_TRIM_ADDRESS2);
	dev_dbg(mt6397_data->mt6397_codec->dev,
		"AudCodec reg1 = 0x%x reg2 = 0x%x\n", reg1, reg2);

	trim_enable = (reg1 >> 11) & 1;
	if (trim_enable == 0) {
		mt6397_data->hp_l_trim = 2;
		mt6397_data->hp_l_fine_trim = 0;
		mt6397_data->hp_r_trim = 2;
		mt6397_data->hp_r_fine_trim = 0;
		mt6397_data->iv_hp_l_trim = 3;
		mt6397_data->iv_hp_l_fine_trim = 0;
		mt6397_data->iv_hp_r_trim = 3;
		mt6397_data->iv_hp_r_fine_trim = 0;
	} else {
		mt6397_data->hp_l_trim = ((reg1 >> 3) & 0xf);
		mt6397_data->hp_r_trim = ((reg1 >> 7) & 0xf);
		mt6397_data->hp_l_fine_trim = ((reg1 >> 12) & 0x3);
		mt6397_data->hp_r_fine_trim = ((reg1 >> 14) & 0x3);
		mt6397_data->iv_hp_l_trim = ((reg2 >> 0) & 0xf);
		mt6397_data->iv_hp_r_trim = ((reg2 >> 4) & 0xf);
		mt6397_data->iv_hp_l_fine_trim = ((reg2 >> 8) & 0x3);
		mt6397_data->iv_hp_r_fine_trim = ((reg2 >> 10) & 0x3);
	}

	dev_dbg(mt6397_data->mt6397_codec->dev, "AudCodec %s trim_enable = %d reg1 = 0x%x reg2 = 0x%x\n",
		__func__, trim_enable, reg1, reg2);
	dev_dbg(mt6397_data->mt6397_codec->dev, "AudCodec %s mHPLtrim = 0x%x mHPLfinetrim = 0x%x mHPRtrim = 0x%x mHPRfinetrim = 0x%x\n",
		__func__, mt6397_data->hp_l_trim, mt6397_data->hp_l_fine_trim,
		 mt6397_data->hp_r_trim, mt6397_data->hp_r_fine_trim);
	dev_dbg(mt6397_data->mt6397_codec->dev, "AudCodec %s mIVHPLtrim = 0x%x mIVHPLfinetrim = 0x%x mIVHPRtrim = 0x%x mIVHPRfinetrim = 0x%x\n",
		__func__, mt6397_data->iv_hp_l_trim,
		mt6397_data->iv_hp_l_fine_trim,
		mt6397_data->iv_hp_r_trim, mt6397_data->iv_hp_r_fine_trim);
}

static void mt6397_set_hp_trim_offset(struct mt6397_codec_priv *mt6397_data)
{
	uint32_t AUDBUG_reg = 0;

	dev_dbg(mt6397_data->mt6397_codec->dev, "AudCodec mt6397_set_hp_trim_offset");
	AUDBUG_reg |= 1 << 8;	/* enable trim function */
	AUDBUG_reg |= mt6397_data->hp_r_fine_trim << 11;
	AUDBUG_reg |= mt6397_data->hp_l_fine_trim << 9;
	AUDBUG_reg |= mt6397_data->hp_r_trim << 4;
	AUDBUG_reg |= mt6397_data->hp_l_trim;
	mt6397_set_reg(mt6397_data, MT6397_AUDBUF_CFG3, AUDBUG_reg, 0x1fff);
}

static void mt6397_set_spk_trim_offset(struct mt6397_codec_priv *mt6397_data)
{
	uint32_t AUDBUG_reg = 0;

	AUDBUG_reg |= 1 << 14;	/* enable trim function */
	AUDBUG_reg |= mt6397_data->spk_l_polarity << 13;	/* polarity */
	AUDBUG_reg |= mt6397_data->spk_l_trim << 8;	/* polarity */
	dev_dbg(mt6397_data->mt6397_codec->dev,
		"SetSPKlTrimOffset AUDBUG_reg = 0x%x\n", AUDBUG_reg);
	mt6397_set_reg(mt6397_data, MT6397_SPK_CON1, AUDBUG_reg, 0x7f00);
	AUDBUG_reg = 0;
	AUDBUG_reg |= 1 << 14;	/* enable trim function */
	AUDBUG_reg |= mt6397_data->spk_r_polarity << 13;	/* polarity */
	AUDBUG_reg |= mt6397_data->spk_r_trim << 8;	/* polarity */
	dev_dbg(mt6397_data->mt6397_codec->dev,
		"SetSPKrTrimOffset AUDBUG_reg = 0x%x\n", AUDBUG_reg);
	mt6397_set_reg(mt6397_data, MT6397_SPK_CON4, AUDBUG_reg, 0x7f00);
}

static void mt6397_set_iv_hp_trim_offset(struct mt6397_codec_priv *mt6397_data)
{
	uint32_t AUDBUG_reg = 0;

	AUDBUG_reg |= 1 << 8;	/* enable trim function */

	if ((mt6397_data->hp_r_fine_trim == 0) ||
	    (mt6397_data->hp_l_fine_trim == 0))
		mt6397_data->iv_hp_r_fine_trim = 0;
	else
		mt6397_data->iv_hp_r_fine_trim = 2;

	AUDBUG_reg |= mt6397_data->iv_hp_r_fine_trim << 11;

	if ((mt6397_data->hp_r_fine_trim == 0) ||
	    (mt6397_data->hp_l_fine_trim == 0))
		mt6397_data->iv_hp_l_fine_trim = 0;
	else
		mt6397_data->iv_hp_l_fine_trim = 2;

	AUDBUG_reg |= mt6397_data->iv_hp_l_fine_trim << 9;

	AUDBUG_reg |= mt6397_data->iv_hp_r_trim << 4;
	AUDBUG_reg |= mt6397_data->iv_hp_l_trim;
	mt6397_set_reg(mt6397_data, MT6397_AUDBUF_CFG3, AUDBUG_reg, 0x1fff);
}

static void mt6397_hs_amp_on(struct mt6397_codec_priv *mt6397_data)
{
	dev_dbg(mt6397_data->mt6397_codec->dev, "mt6397_hs_amp_on\n");
	if (!mt6397_get_dl_status(mt6397_data))
		mt6397_turn_on_dac_power(mt6397_data);

	if (!mt6397_data->power[MTCODEC_VOL_HPOUTL] &&
	    !mt6397_data->power[MTCODEC_VOL_HPOUTR]) {
		mt6397_set_reg(mt6397_data, MT6397_AFUNC_AUD_CON2,
			       0x0080, 0x0080);
		mt6397_set_hp_trim_offset(mt6397_data);
		/* enable VA28 , VA 33 VBAT ref , set dc */
		mt6397_set_reg(mt6397_data, MT6397_AUDLDO_CFG0, 0x0D92, 0xffff);
		/* set ACC mode      enable NVREF */
		mt6397_set_reg(mt6397_data, MT6397_AUDNVREGGLB_CFG0,
			       0x000C, 0xffff);
		/* enable LDO ; fix me , separate for UL  DL LDO */
		mt6397_set_reg(mt6397_data, MT6397_AUD_NCP0, 0xE000, 0xE000);
		/* RG DEV ck on */
		mt6397_set_reg(mt6397_data, MT6397_NCP_CLKDIV_CON0,
			       0x102b, 0xffff);
		/* NCP on */
		mt6397_set_reg(mt6397_data, MT6397_NCP_CLKDIV_CON1,
			       0x0000, 0xffff);
		/* msleep(1);// temp remove */
		usleep_range(200, 210);

		mt6397_set_reg(mt6397_data, MT6397_ZCD_CON0, 0x0101, 0xffff);
		mt6397_set_reg(mt6397_data, MT6397_AUDACCDEPOP_CFG0,
			       0x0030, 0xffff);
		mt6397_set_reg(mt6397_data, MT6397_AUDBUF_CFG0, 0x0008, 0xffff);
		mt6397_set_reg(mt6397_data, MT6397_IBIASDIST_CFG0,
			       0x0552, 0xffff);
		mt6397_set_reg(mt6397_data, MT6397_ZCD_CON2, 0x0c0c, 0xffff);
		mt6397_set_reg(mt6397_data, MT6397_ZCD_CON3, 0x000F, 0xffff);
		mt6397_set_reg(mt6397_data, MT6397_AUDBUF_CFG1, 0x0900, 0xffff);
		mt6397_set_reg(mt6397_data, MT6397_AUDBUF_CFG2, 0x0082, 0xffff);
		/* msleep(1);// temp remove */

		mt6397_set_reg(mt6397_data, MT6397_AUDBUF_CFG0, 0x0009, 0xffff);
		/* msleep(30); //temp */

		mt6397_set_reg(mt6397_data, MT6397_AUDBUF_CFG1, 0x0940, 0xffff);
		usleep_range(200, 210);
		mt6397_set_reg(mt6397_data, MT6397_AUDBUF_CFG0, 0x000F, 0xffff);
		/* msleep(1);// temp remove */

		mt6397_set_reg(mt6397_data, MT6397_AUDBUF_CFG1, 0x0100, 0xffff);
		usleep_range(100, 110);
		mt6397_set_reg(mt6397_data, MT6397_AUDBUF_CFG2, 0x0022, 0xffff);
		mt6397_set_reg(mt6397_data, MT6397_ZCD_CON2, 0x00c0c, 0xffff);
		usleep_range(100, 110);
		/* msleep(1);// temp remove */

		mt6397_set_reg(mt6397_data, MT6397_AUDCLKGEN_CFG0,
			       0x0001, 0x0001);
		mt6397_set_reg(mt6397_data, MT6397_AUDDAC_CON0, 0x000F, 0xffff);
		usleep_range(100, 110);
		/* msleep(1);// temp remove */
		mt6397_set_reg(mt6397_data, MT6397_AUDBUF_CFG0, 0x0006, 0x0007);
		mt6397_mux(mt6397_data, MTCODEC_DEVICE_OUT_HSR,
			   MTCODEC_MUX_AUDIO);
		mt6397_mux(mt6397_data, MTCODEC_DEVICE_OUT_HSL,
			   MTCODEC_MUX_AUDIO);

		mt6397_set_reg(mt6397_data, MT6397_AFUNC_AUD_CON2,
			       0x0000, 0x0080);
	}
	dev_dbg(mt6397_data->mt6397_codec->dev, "mt6397_hs_amp_on done\n");
}

static void mt6397_hs_amp_off(struct mt6397_codec_priv *mt6397_data)
{
	dev_dbg(mt6397_data->mt6397_codec->dev, "mt6397_hs_amp_off\n");

	if (!mt6397_data->power[MTCODEC_VOL_HPOUTL] &&
	    !mt6397_data->power[MTCODEC_VOL_HPOUTR]) {
		mt6397_set_reg(mt6397_data, MT6397_AFUNC_AUD_CON2,
			       0x0080, 0x0080);
		mt6397_set_reg(mt6397_data, MT6397_ZCD_CON2, 0x0c0c, 0xffff);
		mt6397_set_reg(mt6397_data, MT6397_AUDBUF_CFG0, 0x0000, 0x1fe7);
		/* RG DEV ck off; */
		mt6397_set_reg(mt6397_data, MT6397_IBIASDIST_CFG0,
			       0x1552, 0xffff);
		/* NCP off */
		mt6397_set_reg(mt6397_data, MT6397_AUDDAC_CON0, 0x0000, 0xffff);
		mt6397_set_reg(mt6397_data, MT6397_AUDCLKGEN_CFG0,
			       0x0000, 0x0001);

		if (mt6397_get_uplink_status(mt6397_data) == false)
			/* need check */
			mt6397_set_reg(mt6397_data, MT6397_AUDNVREGGLB_CFG0,
				       0x0006, 0xffff);

		/* fix me */
		mt6397_set_reg(mt6397_data, MT6397_NCP_CLKDIV_CON1,
			       0x0001, 0xffff);
		mt6397_set_reg(mt6397_data, MT6397_AUD_NCP0, 0x0000, 0x6000);

		if (mt6397_get_uplink_status(mt6397_data) == false)
			mt6397_set_reg(mt6397_data, MT6397_AUDLDO_CFG0,
				       0x0192, 0xffff);

		mt6397_set_reg(mt6397_data, MT6397_AFUNC_AUD_CON2,
			       0x0000, 0x0080);
		if (mt6397_get_dl_status(mt6397_data) == false)
			mt6397_turn_off_dac_power(mt6397_data);
	}
	dev_dbg(mt6397_data->mt6397_codec->dev, "mt6397_hs_amp_off done\n");
}

static int mt6397_hs_ampl_get(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct mt6397_codec_priv *mt6397_data =
		snd_soc_codec_get_drvdata(codec);

	dev_dbg(mt6397_data->mt6397_codec->dev,
		"AudCodec %s() = %d\n", __func__,
		mt6397_data->power[MTCODEC_VOL_HPOUTL]);
	ucontrol->value.integer.value[0] =
		mt6397_data->power[MTCODEC_VOL_HPOUTL];
	return 0;
}

static int mt6397_hs_ampl_set(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct mt6397_codec_priv *mt6397_data =
		snd_soc_codec_get_drvdata(codec);

	mutex_lock(&mt6397_ctrl_mutex);

	dev_dbg(mt6397_data->mt6397_codec->dev,
		"AudCodec %s() gain = %d\n ", __func__,
		(int)ucontrol->value.integer.value[0]);
	if (ucontrol->value.integer.value[0]) {
		mt6397_clk_on(mt6397_data);
		mt6397_hs_amp_on(mt6397_data);
		mt6397_data->power[MTCODEC_VOL_HPOUTL] =
			ucontrol->value.integer.value[0];
	} else {
		mt6397_data->power[MTCODEC_VOL_HPOUTL] =
			ucontrol->value.integer.value[0];
		mt6397_hs_amp_off(mt6397_data);
		mt6397_clk_off(mt6397_data);
	}
	mutex_unlock(&mt6397_ctrl_mutex);
	return 0;
}

static void mt6397_spk_amp_on(struct mt6397_codec_priv *mt6397_data)
{
	dev_dbg(mt6397_data->mt6397_codec->dev, "mt6397_spk_amp_on\n");
	if (mt6397_get_dl_status(mt6397_data) == false)
		mt6397_turn_on_dac_power(mt6397_data);

	/* enable SPK related CLK //Luke */
	mt6397_enable_clk(mt6397_data, 0x0604, true);
	mt6397_set_reg(mt6397_data, MT6397_AFUNC_AUD_CON2, 0x0080, 0x0080);
	mt6397_set_spk_trim_offset(mt6397_data);
	/* enable VA28 , VA 33 VBAT ref , set dc */
	mt6397_set_reg(mt6397_data, MT6397_AUDLDO_CFG0, 0x0D92, 0xffff);
	/* set ACC mode  enable NVREF */
	mt6397_set_reg(mt6397_data, MT6397_AUDNVREGGLB_CFG0, 0x000C, 0xffff);
	/* enable LDO ; fix me , separate for UL  DL LDO */
	mt6397_set_reg(mt6397_data, MT6397_AUD_NCP0, 0xE000, 0xE000);
	/* RG DEV ck on */
	mt6397_set_reg(mt6397_data, MT6397_NCP_CLKDIV_CON0, 0x102B, 0xffff);
	/* NCP on */
	mt6397_set_reg(mt6397_data, MT6397_NCP_CLKDIV_CON1, 0x0000, 0xffff);
	usleep_range(200, 210);
	/* ZCD setting gain step gain and enable */
	mt6397_set_reg(mt6397_data, MT6397_ZCD_CON0, 0x0301, 0xffff);
	/* audio bias adjustment */
	mt6397_set_reg(mt6397_data, MT6397_IBIASDIST_CFG0, 0x0552, 0xffff);
	/* set DUDIV gain ,iv buffer gain */
	mt6397_set_reg(mt6397_data, MT6397_ZCD_CON4, 0x0505, 0xffff);
	/* set IV buffer on */
	mt6397_set_reg(mt6397_data, MT6397_AUD_IV_CFG0, 0x1111, 0xffff);
	usleep_range(100, 110);
	/* reset docoder */
	mt6397_set_reg(mt6397_data, MT6397_AUDCLKGEN_CFG0, 0x0001, 0x0001);
	/* power on DAC */
	mt6397_set_reg(mt6397_data, MT6397_AUDDAC_CON0, 0x000f, 0xffff);
	usleep_range(100, 110);
	mt6397_mux(mt6397_data, MTCODEC_DEVICE_OUT_SPKR, MTCODEC_MUX_AUDIO);
	mt6397_mux(mt6397_data, MTCODEC_DEVICE_OUT_SPKL, MTCODEC_MUX_AUDIO);
	/* set Mux */
	mt6397_set_reg(mt6397_data, MT6397_AUDBUF_CFG0, 0x0000, 0x0007);
	mt6397_set_reg(mt6397_data, MT6397_AFUNC_AUD_CON2, 0x0000, 0x0080);
	/* set gain L */
	mt6397_set_reg(mt6397_data, MT6397_SPK_CON9, 0x0100, 0x0f00);
	/* set gain R */
	mt6397_set_reg(mt6397_data, MT6397_SPK_CON5, (0x1 << 11), 0x7800);

	/*Class D amp*/
	if (mt6397_data->spk_channel_sel == 0) {
		/* Stereo */
		mt6397_set_reg(mt6397_data, MT6397_SPK_CON0, 0x3009, 0xffff);
		mt6397_set_reg(mt6397_data, MT6397_SPK_CON3, 0x3009, 0xffff);
		mt6397_set_reg(mt6397_data, MT6397_SPK_CON2, 0x0014, 0xffff);
		mt6397_set_reg(mt6397_data, MT6397_SPK_CON5, 0x0014, 0x07ff);
	} else if (mt6397_data->spk_channel_sel == 1) {
		/* MonoLeft */
		mt6397_set_reg(mt6397_data, MT6397_SPK_CON0, 0x3009, 0xffff);
		mt6397_set_reg(mt6397_data, MT6397_SPK_CON2, 0x0014, 0xffff);
	} else if (mt6397_data->spk_channel_sel == 2) {
		/* MonoRight */
		mt6397_set_reg(mt6397_data, MT6397_SPK_CON3, 0x3009, 0xffff);
		mt6397_set_reg(mt6397_data, MT6397_SPK_CON5, 0x0014, 0x07ff);
		}

	if (mt6397_data->spk_channel_sel == 0) {
		/* Stereo SPK gain setting*/
		mt6397_set_reg(mt6397_data, MT6397_SPK_CON9, 0x0800, 0xffff);
		mt6397_set_reg(mt6397_data, MT6397_SPK_CON5,
			       (0x8 << 11), 0x7800);
	} else if (mt6397_data->spk_channel_sel == 1) {
		/* MonoLeft SPK gain setting*/
		mt6397_set_reg(mt6397_data, MT6397_SPK_CON9, 0x0800, 0xffff);
	} else if (mt6397_data->spk_channel_sel == 2) {
		/* MonoRight SPK gain setting*/
		mt6397_set_reg(mt6397_data, MT6397_SPK_CON5,
			       (0x8 << 11), 0x7800);
		}
	/* spk output stage enabke and enable */
	mt6397_set_reg(mt6397_data, MT6397_SPK_CON11, 0x0f00, 0xffff);
	usleep_range(4000, 5000);
	dev_dbg(mt6397_data->mt6397_codec->dev, "mt6397_spk_amp_on done\n");
}

static void mt6397_spk_amp_off(struct mt6397_codec_priv *mt6397_data)
{
	dev_dbg(mt6397_data->mt6397_codec->dev, "mt6397_spk_amp_off\n");
	mt6397_set_reg(mt6397_data, MT6397_AFUNC_AUD_CON2, 0x0080, 0x0080);
	mt6397_set_reg(mt6397_data, MT6397_SPK_CON0, 0x0000, 0xffff);
	mt6397_set_reg(mt6397_data, MT6397_SPK_CON3, 0x0000, 0xffff);
	mt6397_set_reg(mt6397_data, MT6397_SPK_CON11, 0x0000, 0xffff);
	/* enable LDO ; fix me , separate for UL  DL LDO */
	mt6397_set_reg(mt6397_data, MT6397_AUDCLKGEN_CFG0, 0x0000, 0x0001);
	/* RG DEV ck on */
	mt6397_set_reg(mt6397_data, MT6397_AUDDAC_CON0, 0x0000, 0xffff);
	/* NCP on */
	mt6397_set_reg(mt6397_data, MT6397_AUD_IV_CFG0, 0x0000, 0xffff);
	/* Audio headset power on */
	mt6397_set_reg(mt6397_data, MT6397_IBIASDIST_CFG0, 0x1552, 0xffff);
	/* mt6397_set_reg(mt6397_data, MT6397_AUDBUF_CFG1, 0x0000, 0x0100); */
	if (mt6397_get_uplink_status(mt6397_data) == false)
		mt6397_set_reg(mt6397_data, MT6397_AUDNVREGGLB_CFG0,
			       0x0006, 0xffff);

	/* fix me */
	mt6397_set_reg(mt6397_data, MT6397_NCP_CLKDIV_CON1, 0x0001, 0xffff);
	mt6397_set_reg(mt6397_data, MT6397_AUD_NCP0, 0x0000, 0x6000);
	if (mt6397_get_uplink_status(mt6397_data) == false)
		mt6397_set_reg(mt6397_data, MT6397_AUDLDO_CFG0, 0x0192, 0xffff);

	mt6397_set_reg(mt6397_data, MT6397_AFUNC_AUD_CON2, 0x0000, 0x0080);
	/* disable SPK related CLK        //Luke */
	mt6397_enable_clk(mt6397_data, 0x0604, false);
	if (mt6397_get_dl_status(mt6397_data) == false)
		mt6397_turn_off_dac_power(mt6397_data);

	/* temp solution, set ZCD_CON0 to 0x101 for pop noise */
	mt6397_set_reg(mt6397_data, MT6397_ZCD_CON0, 0x0101, 0xffff);
	dev_dbg(mt6397_data->mt6397_codec->dev, "mt6397_spk_amp_off done\n");
}

static int mt6397_spk_amp_get(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct mt6397_codec_priv *mt6397_data =
		snd_soc_codec_get_drvdata(codec);

	dev_dbg(mt6397_data->mt6397_codec->dev, "AudCodec %s()=%d\n", __func__,
		mt6397_data->power[MTCODEC_VOL_SPKL]);
	ucontrol->value.integer.value[0] =
		mt6397_data->power[MTCODEC_VOL_SPKL];
	return 0;
}

static int mt6397_spk_amp_set(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct mt6397_codec_priv *mt6397_data =
		snd_soc_codec_get_drvdata(codec);

	dev_dbg(mt6397_data->mt6397_codec->dev,
		"AudCodec %s() gain = %d\n ", __func__,
		(int)ucontrol->value.integer.value[0]);
	if (ucontrol->value.integer.value[0]) {
		mt6397_clk_on(mt6397_data);
		mt6397_spk_amp_on(mt6397_data);
		mt6397_data->power[MTCODEC_VOL_SPKL] =
			ucontrol->value.integer.value[0];
	} else {
		mt6397_data->power[MTCODEC_VOL_SPKL] =
			ucontrol->value.integer.value[0];
		mt6397_spk_amp_off(mt6397_data);
		mt6397_clk_off(mt6397_data);
	}
	return 0;
}

static void mt6397_hs_spk_amp_on(struct mt6397_codec_priv *mt6397_data)
{
	dev_dbg(mt6397_data->mt6397_codec->dev, "mt6397_hs_spk_amp_on\n");
	if (!mt6397_get_dl_status(mt6397_data))
		mt6397_turn_on_dac_power(mt6397_data);

	/* enable SPK related CLK */
	mt6397_enable_clk(mt6397_data, 0x0604, true);
	mt6397_set_reg(mt6397_data, MT6397_AFUNC_AUD_CON2, 0x0080, 0x0080);
	mt6397_set_hp_trim_offset(mt6397_data);
	mt6397_set_iv_hp_trim_offset(mt6397_data);
	mt6397_set_spk_trim_offset(mt6397_data);

	/* enable VA28 , VA 33 VBAT ref , set dc */
	mt6397_set_reg(mt6397_data, MT6397_AUDLDO_CFG0, 0x0D92, 0xffff);
	/* set ACC mode  enable NVREF */
	mt6397_set_reg(mt6397_data, MT6397_AUDNVREGGLB_CFG0, 0x000C, 0xffff);
	/* enable LDO ; fix me , separate for UL  DL LDO */
	mt6397_set_reg(mt6397_data, MT6397_AUD_NCP0, 0xE000, 0xE000);
	/* RG DEV ck on */
	mt6397_set_reg(mt6397_data, MT6397_NCP_CLKDIV_CON0, 0x102B, 0xffff);
	/* NCP on */
	mt6397_set_reg(mt6397_data, MT6397_NCP_CLKDIV_CON1, 0x0000, 0xffff);
	usleep_range(200, 210);

	/* ZCD setting gain step gain and enable */
	mt6397_set_reg(mt6397_data, MT6397_ZCD_CON0, 0x0301, 0xffff);
	/* select charge current ; fix me */
	mt6397_set_reg(mt6397_data, MT6397_AUDACCDEPOP_CFG0, 0x0030, 0xffff);
	/* set voice playback with headset */
	mt6397_set_reg(mt6397_data, MT6397_AUDBUF_CFG0, 0x0008, 0xffff);
	/* audio bias adjustment */
	mt6397_set_reg(mt6397_data, MT6397_IBIASDIST_CFG0, 0x0552, 0xffff);
	/* HP PGA gain */
	mt6397_set_reg(mt6397_data, MT6397_ZCD_CON2, 0x0C0C, 0xffff);
	/* HP PGA gain */
	mt6397_set_reg(mt6397_data, MT6397_ZCD_CON3, 0x000F, 0xffff);
	/* HP enhance */
	mt6397_set_reg(mt6397_data, MT6397_AUDBUF_CFG1, 0x0900, 0xffff);
	/* HS enahnce */
	mt6397_set_reg(mt6397_data, MT6397_AUDBUF_CFG2, 0x0082, 0xffff);
	mt6397_set_reg(mt6397_data, MT6397_AUDBUF_CFG0, 0x0009, 0xffff);
	/* HP vcm short */
	mt6397_set_reg(mt6397_data, MT6397_AUDBUF_CFG1, 0x0940, 0xffff);
	usleep_range(200, 210);
	/* HP power on */
	mt6397_set_reg(mt6397_data, MT6397_AUDBUF_CFG0, 0x000F, 0xffff);
	/* HP vcm not short */
	mt6397_set_reg(mt6397_data, MT6397_AUDBUF_CFG1, 0x0100, 0xffff);
	usleep_range(100, 110);
	/* HS VCM not short */
	mt6397_set_reg(mt6397_data, MT6397_AUDBUF_CFG2, 0x0022, 0xffff);

	/* HP PGA gain */
	mt6397_set_reg(mt6397_data, MT6397_ZCD_CON2, 0x0808, 0xffff);
	usleep_range(100, 110);
	/* HP PGA gain */
	mt6397_set_reg(mt6397_data, MT6397_ZCD_CON4, 0x0505, 0xffff);

	/* set IV buffer on */
	mt6397_set_reg(mt6397_data, MT6397_AUD_IV_CFG0, 0x1111, 0xffff);
	usleep_range(100, 110);
	/* reset docoder */
	mt6397_set_reg(mt6397_data, MT6397_AUDCLKGEN_CFG0, 0x0001, 0x0001);
	/* power on DAC */
	mt6397_set_reg(mt6397_data, MT6397_AUDDAC_CON0, 0x000F, 0xffff);
	usleep_range(100, 110);
	mt6397_mux(mt6397_data, MTCODEC_DEVICE_OUT_SPKR, MTCODEC_MUX_AUDIO);
	mt6397_mux(mt6397_data, MTCODEC_DEVICE_OUT_SPKL, MTCODEC_MUX_AUDIO);
	/* set headhpone mux */
	mt6397_set_reg(mt6397_data, MT6397_AUDBUF_CFG0, 0x1106, 0x1106);
	/* set gain L */
	mt6397_set_reg(mt6397_data, MT6397_SPK_CON9, 0x0100, 0x0f00);
	/* set gain R */
	mt6397_set_reg(mt6397_data, MT6397_SPK_CON5, (0x1 << 11), 0x7800);

	/*speaker gain setting, trim enable, spk enable, class D*/
	mt6397_set_reg(mt6397_data, MT6397_SPK_CON0, 0x3009, 0xffff);
	mt6397_set_reg(mt6397_data, MT6397_SPK_CON3, 0x3009, 0xffff);
	mt6397_set_reg(mt6397_data, MT6397_SPK_CON2, 0x0014, 0xffff);
	mt6397_set_reg(mt6397_data, MT6397_SPK_CON5, 0x0014, 0x07ff);

	/* SPK gain setting */
	mt6397_set_reg(mt6397_data, MT6397_SPK_CON9, 0x0400, 0xffff);
	/* SPK-R gain setting */
	mt6397_set_reg(mt6397_data, MT6397_SPK_CON5, (0x4 << 11), 0x7800);
	/* spk output stage enabke and enableAudioClockPortDST */
	mt6397_set_reg(mt6397_data, MT6397_SPK_CON11, 0x0f00, 0xffff);
	mt6397_set_reg(mt6397_data, MT6397_AFUNC_AUD_CON2, 0x0000, 0x0080);
	usleep_range(4000, 5000);

	dev_dbg(mt6397_data->mt6397_codec->dev, "mt6397_hs_spk_amp_on done\n");
}

static void mt6397_hs_spk_amp_off(struct mt6397_codec_priv *mt6397_data)
{
	dev_dbg(mt6397_data->mt6397_codec->dev, "mt6397_hs_spk_amp_off\n");
	mt6397_set_reg(mt6397_data, MT6397_AFUNC_AUD_CON2, 0x0080, 0x0080);
	mt6397_set_reg(mt6397_data, MT6397_SPK_CON0, 0x0000, 0xffff);
	mt6397_set_reg(mt6397_data, MT6397_SPK_CON3, 0x0000, 0xffff);
	mt6397_set_reg(mt6397_data, MT6397_SPK_CON11, 0x0000, 0xffff);
	mt6397_set_reg(mt6397_data, MT6397_ZCD_CON2, 0x0C0C, 0x0f0f);

	mt6397_set_reg(mt6397_data, MT6397_AUDBUF_CFG0, 0x0000, 0x0007);
	mt6397_set_reg(mt6397_data, MT6397_AUDBUF_CFG0, 0x0000, 0x1fe0);
	mt6397_set_reg(mt6397_data, MT6397_IBIASDIST_CFG0, 0x1552, 0xffff);
	mt6397_set_reg(mt6397_data, MT6397_AUDDAC_CON0, 0x0000, 0xffff);
	mt6397_set_reg(mt6397_data, MT6397_AUDCLKGEN_CFG0, 0x0000, 0x0001);
	mt6397_set_reg(mt6397_data, MT6397_AUD_IV_CFG0, 0x0010, 0xffff);
	mt6397_set_reg(mt6397_data, MT6397_AUDBUF_CFG1, 0x0000, 0x0100);
	mt6397_set_reg(mt6397_data, MT6397_AUDBUF_CFG2, 0x0000, 0x0080);

	if (!mt6397_get_uplink_status(mt6397_data))
		mt6397_set_reg(mt6397_data, MT6397_AUDNVREGGLB_CFG0,
			       0x0006, 0xffff);

	mt6397_set_reg(mt6397_data, MT6397_NCP_CLKDIV_CON1, 0x0001, 0xffff);
	mt6397_set_reg(mt6397_data, MT6397_AUD_NCP0, 0x0000, 0x6000);
	if (!mt6397_get_uplink_status(mt6397_data))
		mt6397_set_reg(mt6397_data, MT6397_AUDLDO_CFG0, 0x0192, 0xffff);

	mt6397_set_reg(mt6397_data, MT6397_AFUNC_AUD_CON2, 0x0000, 0x0080);
	/* disable SPK related CLK   //Luke */
	mt6397_enable_clk(mt6397_data, 0x0604, false);
	if (!mt6397_get_dl_status(mt6397_data))
		mt6397_turn_off_dac_power(mt6397_data);
	/* ZCD setting gain step gain and enable */
	mt6397_set_reg(mt6397_data, MT6397_ZCD_CON0, 0x0101, 0xffff);
	dev_dbg(mt6397_data->mt6397_codec->dev, "mt6397_hs_spk_amp_off done\n");
}

static int mt6397_hs_spk_amp_get(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct mt6397_codec_priv *mt6397_data =
		snd_soc_codec_get_drvdata(codec);

	dev_dbg(mt6397_data->mt6397_codec->dev, "AudCodec %s()=%d\n", __func__,
		mt6397_data->power[MTCODEC_VOL_SPK_HS_L]);
	ucontrol->value.integer.value[0] =
		mt6397_data->power[MTCODEC_VOL_SPK_HS_L];
	return 0;
}

static int mt6397_hs_spk_amp_set(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct mt6397_codec_priv *mt6397_data =
		snd_soc_codec_get_drvdata(codec);

	dev_dbg(mt6397_data->mt6397_codec->dev,
		"AudCodec %s() gain = %d\n ", __func__,
		(int)ucontrol->value.integer.value[0]);
	if (ucontrol->value.integer.value[0]) {
		mt6397_clk_on(mt6397_data);
		mt6397_hs_spk_amp_on(mt6397_data);
		mt6397_data->power[MTCODEC_VOL_SPK_HS_L] =
			ucontrol->value.integer.value[0];
	} else {
		mt6397_data->power[MTCODEC_VOL_SPK_HS_L] =
			ucontrol->value.integer.value[0];
		mt6397_hs_spk_amp_off(mt6397_data);
		mt6397_clk_off(mt6397_data);
	}
	return 0;
}

static const char *const mt6397_amp_function[] = { "Off", "On" };

static const char *const mt6397_dac_pga_hs_gain[] = {
	"8Db", "7Db", "6Db", "5Db", "4Db", "3Db", "2Db", "1Db", "0Db", "-1Db",
	"-2Db", "-3Db", "-4Db", "RES1", "RES2", "-40Db"
};

static const char *const mt6397_dac_pga_handset_gain[] = {
	"-21Db", "-19Db", "-17Db", "-15Db", "-13Db", "-11Db", "-9Db", "-7Db",
	"-5Db", "-3Db", "-1Db", "1Db", "3Db", "5Db", "7Db", "9Db"
};				/* todo: 6397's setting */

static const char *const mt6397_dac_pga_spk_gain[] = {
	"Mute", "0Db", "4Db", "5Db", "6Db", "7Db", "8Db", "9Db", "10Db",
	"11Db", "12Db", "13Db", "14Db", "15Db", "16Db", "17Db"
};

static const char *const mt6397_voice_mux_function[] = { "Voice", "Speaker" };
static const char *const mt6397_spk_selection_function[] = { "Stereo",
						"MonoLeft", "MonoRight" };

static int mt6397_spk_pga_l_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct mt6397_codec_priv *mt6397_data =
		snd_soc_codec_get_drvdata(codec);

	dev_dbg(mt6397_data->mt6397_codec->dev, "AudCodec %s = %d\n", __func__,
		mt6397_data->volume[MTCODEC_VOL_SPKL]);
	ucontrol->value.integer.value[0] =
		mt6397_data->volume[MTCODEC_VOL_SPKL];
	return 0;
}

static int mt6397_spk_pga_l_set(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct mt6397_codec_priv *mt6397_data =
		snd_soc_codec_get_drvdata(codec);
	int index = 0;

	dev_dbg(mt6397_data->mt6397_codec->dev, "AudCodec %s()\n", __func__);

	if (ucontrol->value.enumerated.item[0] >
	    ARRAY_SIZE(mt6397_dac_pga_spk_gain)) {
		dev_err(mt6397_data->mt6397_codec->dev, "AudCodec return -EINVAL\n");
		return -EINVAL;
	}

	index = ucontrol->value.integer.value[0];
	mt6397_set_reg(mt6397_data, MT6397_SPK_CON9, index << 8, 0x00000f00);
	mt6397_data->volume[MTCODEC_VOL_SPKL] =
		ucontrol->value.integer.value[0];
	return 0;
}

static int mt6397_spk_pga_r_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct mt6397_codec_priv *mt6397_data =
		snd_soc_codec_get_drvdata(codec);

	dev_dbg(mt6397_data->mt6397_codec->dev, "AudCodec %s = %d\n", __func__,
		mt6397_data->volume[MTCODEC_VOL_SPKR]);
	ucontrol->value.integer.value[0] =
		mt6397_data->volume[MTCODEC_VOL_SPKR];
	return 0;
}

static int mt6397_spk_pga_r_set(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct mt6397_codec_priv *mt6397_data =
		snd_soc_codec_get_drvdata(codec);
	int index = 0;

	dev_dbg(mt6397_data->mt6397_codec->dev, "AudCodec %s()\n", __func__);

	if (ucontrol->value.enumerated.item[0] >
	    ARRAY_SIZE(mt6397_dac_pga_spk_gain)) {
		dev_err(mt6397_data->mt6397_codec->dev, "AudCodec return -EINVAL\n");
		return -EINVAL;
	}

	index = ucontrol->value.integer.value[0];
	mt6397_set_reg(mt6397_data, MT6397_SPK_CON5, index << 11, 0x00007800);
	mt6397_data->volume[MTCODEC_VOL_SPKR] =
		ucontrol->value.integer.value[0];
	return 0;
}

static int mt6397_spk_channel_set(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct mt6397_codec_priv *mt6397_data =
		snd_soc_codec_get_drvdata(codec);

	dev_dbg(mt6397_data->mt6397_codec->dev, "AudCodec %s()\n", __func__);
	mt6397_data->spk_channel_sel =
		ucontrol->value.integer.value[0];
	return 0;
}

static int mt6397_spk_channel_get(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct mt6397_codec_priv *mt6397_data =
		snd_soc_codec_get_drvdata(codec);

	dev_dbg(mt6397_data->mt6397_codec->dev, "AudCodec %s = %d\n", __func__,
		mt6397_data->spk_channel_sel);
	ucontrol->value.integer.value[0] =
		mt6397_data->spk_channel_sel;
	return 0;
}

static int mt6397_hs_pga_l_get(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct mt6397_codec_priv *mt6397_data =
		snd_soc_codec_get_drvdata(codec);

	dev_dbg(mt6397_data->mt6397_codec->dev,
		"AudCodec %s() = %d\n", __func__,
		mt6397_data->volume[MTCODEC_VOL_HPOUTL]);
	ucontrol->value.integer.value[0] =
		mt6397_data->volume[MTCODEC_VOL_HPOUTL];
	return 0;
}

static int mt6397_hs_pga_l_set(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct mt6397_codec_priv *mt6397_data =
		snd_soc_codec_get_drvdata(codec);
	int index = 0;

	dev_dbg(mt6397_data->mt6397_codec->dev, "AudCodec %s()\n", __func__);
	if (ucontrol->value.enumerated.item[0] >
		ARRAY_SIZE(mt6397_dac_pga_hs_gain)) {
		dev_err(mt6397_data->mt6397_codec->dev, "AudCodec return -EINVAL\n");
		return -EINVAL;
	}
	index = ucontrol->value.integer.value[0];
	mt6397_set_reg(mt6397_data, MT6397_ZCD_CON2, index, 0x0000000F);
	mt6397_data->volume[MTCODEC_VOL_HPOUTL] =
		ucontrol->value.integer.value[0];
	return 0;
}

static int mt6397_hs_pga_r_get(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct mt6397_codec_priv *mt6397_data =
		snd_soc_codec_get_drvdata(codec);

	dev_dbg(mt6397_data->mt6397_codec->dev,
		"AudCodec %s() = %d\n", __func__,
		mt6397_data->volume[MTCODEC_VOL_HPOUTR]);
	ucontrol->value.integer.value[0] =
		mt6397_data->volume[MTCODEC_VOL_HPOUTR];
	return 0;
}

static int mt6397_hs_pga_r_set(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct mt6397_codec_priv *mt6397_data =
		snd_soc_codec_get_drvdata(codec);
	int index = 0;

	dev_dbg(mt6397_data->mt6397_codec->dev, "AudCodec %s()\n", __func__);

	if (ucontrol->value.enumerated.item[0] >
			ARRAY_SIZE(mt6397_dac_pga_hs_gain)) {
		dev_err(mt6397_data->mt6397_codec->dev, "AudCodec return -EINVAL\n");
		return -EINVAL;
	}
	index = ucontrol->value.integer.value[0];
	mt6397_set_reg(mt6397_data, MT6397_ZCD_CON2, index << 8, 0x000000F00);
	mt6397_data->volume[MTCODEC_VOL_HPOUTR] =
		ucontrol->value.integer.value[0];
	return 0;
}

static const struct soc_enum mt6397_amp_enum[] = {
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(mt6397_amp_function),
			    mt6397_amp_function),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(mt6397_amp_function),
			    mt6397_amp_function),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(mt6397_amp_function),
			    mt6397_amp_function),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(mt6397_amp_function),
			    mt6397_amp_function),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(mt6397_amp_function),
			    mt6397_amp_function),
	/* here comes pga gain setting */
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(mt6397_dac_pga_hs_gain),
			    mt6397_dac_pga_hs_gain),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(mt6397_dac_pga_hs_gain),
			    mt6397_dac_pga_hs_gain),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(mt6397_dac_pga_handset_gain),
			    mt6397_dac_pga_handset_gain),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(mt6397_dac_pga_spk_gain),
			    mt6397_dac_pga_spk_gain),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(mt6397_dac_pga_spk_gain),
			    mt6397_dac_pga_spk_gain),
	/* Mux Function */
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(mt6397_voice_mux_function),
			    mt6397_voice_mux_function),
	/* Configurations */
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(mt6397_spk_selection_function),
			    mt6397_spk_selection_function),
};

static const struct snd_kcontrol_new mt6397_snd_controls[] = {
	SOC_ENUM_EXT("Audio_Amp_Switch", mt6397_amp_enum[1],
		     mt6397_hs_ampl_get, mt6397_hs_ampl_set),
	SOC_ENUM_EXT("Speaker_Amp_Switch", mt6397_amp_enum[3],
		     mt6397_spk_amp_get, mt6397_spk_amp_set),
	SOC_ENUM_EXT("Headset_Speaker_Amp_Switch", mt6397_amp_enum[4],
		     mt6397_hs_spk_amp_get,
		     mt6397_hs_spk_amp_set),

	SOC_ENUM_EXT("Headset_PGAL_GAIN", mt6397_amp_enum[5],
		     mt6397_hs_pga_l_get, mt6397_hs_pga_l_set),
	SOC_ENUM_EXT("Headset_PGAR_GAIN", mt6397_amp_enum[6],
		     mt6397_hs_pga_r_get, mt6397_hs_pga_r_set),
	SOC_ENUM_EXT("Speaker_PGAL_GAIN", mt6397_amp_enum[8],
		     mt6397_spk_pga_l_get, mt6397_spk_pga_l_set),
	SOC_ENUM_EXT("Speaker_PGAR_GAIN", mt6397_amp_enum[9],
		     mt6397_spk_pga_r_get, mt6397_spk_pga_r_set),

	SOC_ENUM_EXT("Speaker_Channel_Select", mt6397_amp_enum[11],
		     mt6397_spk_channel_get,
		     mt6397_spk_channel_set),

};

static void mt6397_dmic_power_on(struct mt6397_codec_priv *mt6397_data)
{
	/* pmic digital part */
	mt6397_set_reg(mt6397_data, MT6397_AUDCLKGEN_CFG0, 0x0000, 0x0002);
	mt6397_set_reg(mt6397_data, MT6397_AFE_UL_SRC_CON0_L, 0x0000, 0xffff);

	mt6397_enable_clk(mt6397_data, 0x0003, true);
	mt6397_set_reg(mt6397_data, MT6397_AUDIO_TOP_CON0, 0x0000, 0xffff);
	mt6397_set_reg(mt6397_data, MT6397_AFE_UL_SRC_CON0_H,
		       mt6397_ul_fs(mt6397_data), 0xffff);
	mt6397_set_reg(mt6397_data, MT6397_AFE_UL_DL_CON0, 0x007f, 0xffff);
	mt6397_set_reg(mt6397_data, MT6397_AFE_UL_SRC_CON0_L, 0x0023, 0xffff);

	/* AudioMachineDevice */
	mt6397_set_reg(mt6397_data, MT6397_AUDNVREGGLB_CFG0, 0x000c, 0xffff);
	mt6397_set_reg(mt6397_data, MT6397_AUDDIGMI_CON0, 0x0181, 0xffff);
}

static void mt6397_dmic_power_off(struct mt6397_codec_priv *mt6397_data)
{
	/* AudioMachineDevice */
	mt6397_set_reg(mt6397_data, MT6397_AUDDIGMI_CON0, 0x0080, 0xffff);
	/* pmic digital part */
	mt6397_set_reg(mt6397_data, MT6397_AFE_UL_SRC_CON0_H, 0x0000, 0xffff);
	mt6397_set_reg(mt6397_data, MT6397_AFE_UL_SRC_CON0_L, 0x0000, 0xffff);
	if (mt6397_get_dl_status(mt6397_data) == false)
		mt6397_set_reg(mt6397_data, MT6397_AFE_UL_DL_CON0,
			       0x0000, 0xffff);
}

static void mt6397_adc_power_on(struct mt6397_codec_priv *mt6397_data)
{
	if (!mt6397_get_adc_status(mt6397_data)) {
		/* pmic digital part */
		mt6397_set_reg(mt6397_data, MT6397_AUDCLKGEN_CFG0,
			       0x0000, 0x0002);
		mt6397_set_reg(mt6397_data, MT6397_AFE_UL_SRC_CON0_L,
			       0x0000, 0xffff);
		mt6397_set_reg(mt6397_data, MT6397_AUDCLKGEN_CFG0,
			       0x0002, 0x0002);
		mt6397_enable_clk(mt6397_data, 0x0003, true);
		mt6397_set_reg(mt6397_data, MT6397_AUDIO_TOP_CON0,
			       0x0000, 0xffff);
		mt6397_set_reg(mt6397_data, MT6397_AFE_UL_SRC_CON0_H,
			       mt6397_ul_fs(mt6397_data), 0xffff);
		mt6397_set_reg(mt6397_data, MT6397_AFE_UL_DL_CON0,
			       0x007f, 0xffff);
		mt6397_set_reg(mt6397_data, MT6397_AFE_UL_SRC_CON0_L,
			       0x0001, 0xffff);

		/* pmic analog part */
		mt6397_set_reg(mt6397_data, MT6397_AUDNVREGGLB_CFG0,
			       0x000c, 0xffff);
		mt6397_set_reg(mt6397_data, MT6397_AUDLDO_CFG0, 0x0D92, 0xffff);
		mt6397_set_reg(mt6397_data, MT6397_AUD_NCP0, 0x9000, 0x9000);

		mt6397_mux(mt6397_data, MTCODEC_DEVICE_ADC1,
			   MTCODEC_MUX_PREAMP1);
		mt6397_mux(mt6397_data, MTCODEC_DEVICE_ADC2,
			   MTCODEC_MUX_PREAMP2);

		/* open power */
		mt6397_set_reg(mt6397_data, MT6397_AUDPREAMP_CON0,
			       0x0003, 0x0003);
		mt6397_set_reg(mt6397_data, MT6397_AUDADC_CON0, 0x0093, 0xffff);
		mt6397_set_reg(mt6397_data, MT6397_NCP_CLKDIV_CON0,
			       0x102B, 0x102B);
		mt6397_set_reg(mt6397_data, MT6397_NCP_CLKDIV_CON1,
			       0x0000, 0xffff);
		mt6397_set_reg(mt6397_data, MT6397_AUDDIGMI_CON0,
			       0x0180, 0x0180);
		mt6397_set_reg(mt6397_data, MT6397_AUDPREAMPGAIN_CON0,
			       0x0033, 0x0033);
	}
	/* TODO: Please Luke check how to separate MTCODEC_DEVICE_ADC1 and
	   MTCODEC_DEVICE_ADC2 */
}

static void mt6397_adc_power_off(struct mt6397_codec_priv *mt6397_data)
{
	if (!mt6397_get_adc_status(mt6397_data)) {
		/* LDO off */
		mt6397_set_reg(mt6397_data, MT6397_AUDPREAMP_CON0,
			       0x0000, 0x0003);
		/* RD_CLK off */
		mt6397_set_reg(mt6397_data, MT6397_AUDADC_CON0, 0x00B4, 0xffff);
		/* NCP off */
		mt6397_set_reg(mt6397_data, MT6397_AUDDIGMI_CON0,
			       0x0080, 0xffff);
		/* turn iogg LDO */
		mt6397_set_reg(mt6397_data, MT6397_AUD_NCP0, 0x0000, 0x1000);
		/* Power Off LSB */
		mt6397_set_reg(mt6397_data, MT6397_AUDLSBUF_CON0,
			       0x0000, 0x0003);

		if (mt6397_get_dl_status(mt6397_data) == false)
			mt6397_set_reg(mt6397_data, MT6397_AUDNVREGGLB_CFG0,
				       0x0004, 0xffff);

		if (mt6397_get_dl_status(mt6397_data) == false)
			mt6397_set_reg(mt6397_data, MT6397_AUDLDO_CFG0,
				       0x0192, 0xffff);
		mt6397_set_reg(mt6397_data, MT6397_AUDCLKGEN_CFG0,
			       0x0000, 0x0002);
		/* pmic digital part */

		mt6397_set_reg(mt6397_data, MT6397_AFE_UL_SRC_CON0_L,
			       0x0000, 0xffff);
		if (!mt6397_get_dl_status(mt6397_data))
			mt6397_set_reg(mt6397_data, MT6397_AFE_UL_DL_CON0,
				       0x0000, 0xffff);
	}
}

static int mt6397_loopback_get(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct mt6397_codec_priv *mt6397_data =
		snd_soc_codec_get_drvdata(codec);

	ucontrol->value.integer.value[0] = mt6397_data->codec_loopback_type;
	return 0;
}

static int mt6397_loopback_set(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct mt6397_codec_priv *mt6397_data =
		snd_soc_codec_get_drvdata(codec);
	uint32_t previous_loopback_type = mt6397_data->codec_loopback_type;

	dev_dbg(mt6397_data->mt6397_codec->dev, "%s %ld\n", __func__,
		ucontrol->value.integer.value[0]);

	if (previous_loopback_type == ucontrol->value.integer.value[0]) {
		dev_dbg(mt6397_data->mt6397_codec->dev,
			"%s dummy operation for %u", __func__,
			mt6397_data->codec_loopback_type);
		return 0;
	}

	if (previous_loopback_type != MTCODEC_LOOP_NONE) {
		/* disable uplink */
		if (previous_loopback_type == MTCODEC_LOOP_AMIC_SPK ||
		    previous_loopback_type == MTCODEC_LOOP_AMIC_HP ||
		    previous_loopback_type == MTCODEC_LOOP_HS_SPK ||
		    previous_loopback_type == MTCODEC_LOOP_HS_HP) {
			if (mt6397_data->power[MTCODEC_DEVICE_ADC1]) {
				mt6397_data->power[MTCODEC_DEVICE_ADC1] = 0;
				mt6397_adc_power_off(mt6397_data);
			}

			if (mt6397_data->power[MTCODEC_DEVICE_ADC2]) {
				mt6397_data->power[MTCODEC_DEVICE_ADC2] = 0;
				mt6397_adc_power_off(mt6397_data);
			}
			mt6397_data->mux[MTCODEC_MUX_PREAMP1] = 0;
			mt6397_data->mux[MTCODEC_MUX_PREAMP2] = 0;
		} else if (previous_loopback_type == MTCODEC_LOOP_DMIC_SPK ||
			   previous_loopback_type == MTCODEC_LOOP_DMIC_HP) {
			if (mt6397_data->power[MTCODEC_DEVICE_DMIC]) {
				mt6397_data->power[MTCODEC_DEVICE_DMIC] = 0;
				mt6397_dmic_power_off(mt6397_data);
			}
		}
		/* disable downlink */
		if (previous_loopback_type == MTCODEC_LOOP_AMIC_SPK ||
		    previous_loopback_type == MTCODEC_LOOP_HS_SPK ||
		    previous_loopback_type == MTCODEC_LOOP_DMIC_SPK) {
			if (mt6397_data->power[MTCODEC_VOL_SPKL]) {
				mt6397_data->power[MTCODEC_VOL_SPKL] = 0;
				mt6397_spk_amp_off(mt6397_data);
			}
			mt6397_clk_off(mt6397_data);
		} else if (previous_loopback_type == MTCODEC_LOOP_AMIC_HP ||
			   previous_loopback_type == MTCODEC_LOOP_DMIC_HP ||
			   previous_loopback_type == MTCODEC_LOOP_HS_HP) {
			if (mt6397_data->power[MTCODEC_VOL_HPOUTL]) {
				mt6397_data->power[MTCODEC_VOL_HPOUTL] = 0;
				mt6397_hs_amp_off(mt6397_data);
			}
			mt6397_clk_off(mt6397_data);
		}
	}
	/* enable uplink */
	if (ucontrol->value.integer.value[0] == MTCODEC_LOOP_AMIC_SPK ||
	    ucontrol->value.integer.value[0] == MTCODEC_LOOP_AMIC_HP ||
	    ucontrol->value.integer.value[0] == MTCODEC_LOOP_HS_SPK ||
	    ucontrol->value.integer.value[0] == MTCODEC_LOOP_HS_HP) {
		mt6397_clk_on(mt6397_data);
		mt6397_data->mt6397_fs[MTCODEC_DEVICE_DAC] = 48000;
		mt6397_data->mt6397_fs[MTCODEC_DEVICE_ADC] = 48000;

		if (!mt6397_data->power[MTCODEC_DEVICE_ADC1]) {
			mt6397_adc_power_on(mt6397_data);
			mt6397_data->power[MTCODEC_DEVICE_ADC1] = 1;
		}

		if (!mt6397_data->power[MTCODEC_DEVICE_ADC2]) {
			mt6397_adc_power_on(mt6397_data);
			mt6397_data->power[MTCODEC_DEVICE_ADC2] = 1;
		}
		/* mux selection */
		if (ucontrol->value.integer.value[0] == MTCODEC_LOOP_HS_SPK ||
		    ucontrol->value.integer.value[0] == MTCODEC_LOOP_HS_HP) {
			mt6397_mux(mt6397_data, MTCODEC_DEVICE_PREAMP_L,
				   MTCODEC_MUX_MIC2);
			mt6397_data->mux[MTCODEC_MUX_PREAMP1] = 2;
		} else {
			mt6397_mux(mt6397_data, MTCODEC_DEVICE_PREAMP_L,
				   MTCODEC_MUX_MIC1);
			mt6397_data->mux[MTCODEC_MUX_PREAMP1] = 1;
		}
	} else if (ucontrol->value.integer.value[0] == MTCODEC_LOOP_DMIC_SPK ||
		   ucontrol->value.integer.value[0] == MTCODEC_LOOP_DMIC_HP) {
		mt6397_clk_on(mt6397_data);
		mt6397_data->mt6397_fs[MTCODEC_DEVICE_DAC] = 32000;
		mt6397_data->mt6397_fs[MTCODEC_DEVICE_ADC] = 32000;

		if (!mt6397_data->power[MTCODEC_DEVICE_DMIC]) {
			mt6397_dmic_power_on(mt6397_data);
			mt6397_data->power[MTCODEC_DEVICE_DMIC] = 1;
		}
	}
	/* enable downlink */
	if (ucontrol->value.integer.value[0] == MTCODEC_LOOP_AMIC_SPK ||
	    ucontrol->value.integer.value[0] == MTCODEC_LOOP_HS_SPK ||
	    ucontrol->value.integer.value[0] == MTCODEC_LOOP_DMIC_SPK) {
		if (!mt6397_data->power[MTCODEC_VOL_SPKL]) {
			mt6397_spk_amp_on(mt6397_data);
			mt6397_data->power[MTCODEC_VOL_SPKL] = 1;
		}
	} else if (ucontrol->value.integer.value[0] == MTCODEC_LOOP_AMIC_HP ||
		   ucontrol->value.integer.value[0] == MTCODEC_LOOP_DMIC_HP ||
		   ucontrol->value.integer.value[0] == MTCODEC_LOOP_HS_HP) {
		if (!mt6397_data->power[MTCODEC_VOL_HPOUTL]) {
			mt6397_hs_amp_on(mt6397_data);
			mt6397_data->power[MTCODEC_VOL_HPOUTL] = 1;
		}
	}

	mt6397_data->codec_loopback_type = ucontrol->value.integer.value[0];
	return 0;
}

static const char *const mt6397_loopback_function[] = {
	ENUM_TO_STR(MTCODEC_LOOP_NONE),
	ENUM_TO_STR(MTCODEC_LOOP_AMIC_SPK),
	ENUM_TO_STR(MTCODEC_LOOP_AMIC_HP),
	ENUM_TO_STR(MTCODEC_LOOP_DMIC_SPK),
	ENUM_TO_STR(MTCODEC_LOOP_DMIC_HP),
	ENUM_TO_STR(MTCODEC_LOOP_HS_SPK),
	ENUM_TO_STR(MTCODEC_LOOP_HS_HP),
};

static const struct soc_enum mt6397_factory_enum[] = {
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(mt6397_loopback_function),
			    mt6397_loopback_function),
};

static const struct snd_kcontrol_new mt6397_factory_controls[] = {
	SOC_ENUM_EXT("Codec_Loopback_Select", mt6397_factory_enum[0],
		     mt6397_loopback_get, mt6397_loopback_set),
};

/* here start uplink power function */
static const char *const mt6397_adc_function[] = { "Off", "On" };
static const char *const mt6397_dmic_function[] = { "Off", "On" };

static const char *const mt6397_preamp_mux_function[] = { "OPEN", "AIN1",
							  "AIN2", "AIN3" };

static const char *const mt6397_adc_ul_pag_gain[] = { "2Db", "8Db", "14Db",
						      "20Db", "26Db", "32Db" };

static const struct soc_enum mt6397_ul_euum[] = {
	SOC_ENUM_SINGLE_EXT(2, mt6397_adc_function),
	SOC_ENUM_SINGLE_EXT(2, mt6397_adc_function),
	SOC_ENUM_SINGLE_EXT(4, mt6397_preamp_mux_function),
	SOC_ENUM_SINGLE_EXT(4, mt6397_preamp_mux_function),
	SOC_ENUM_SINGLE_EXT(6, mt6397_adc_ul_pag_gain),
	SOC_ENUM_SINGLE_EXT(6, mt6397_adc_ul_pag_gain),
	SOC_ENUM_SINGLE_EXT(2, mt6397_dmic_function),
};

static int mt6397_dmic_get(struct snd_kcontrol *kcontrol,
			   struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct mt6397_codec_priv *mt6397_data =
		snd_soc_codec_get_drvdata(codec);

	dev_dbg(mt6397_data->mt6397_codec->dev, "AudCodec mt6397_dmic_get = %d\n",
		mt6397_data->power[MTCODEC_DEVICE_DMIC]);
	ucontrol->value.integer.value[0] =
	    mt6397_data->power[MTCODEC_DEVICE_DMIC];
	return 0;
}

static int mt6397_dmic_set(struct snd_kcontrol *kcontrol,
			   struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct mt6397_codec_priv *mt6397_data =
		snd_soc_codec_get_drvdata(codec);

	dev_dbg(mt6397_data->mt6397_codec->dev, "AudCodec %s()\n", __func__);
	if (ucontrol->value.integer.value[0]) {
		mt6397_clk_on(mt6397_data);
		mt6397_dmic_power_on(mt6397_data);
		mt6397_data->power[MTCODEC_DEVICE_DMIC] =
			ucontrol->value.integer.value[0];
	} else {
		mt6397_data->power[MTCODEC_DEVICE_DMIC] =
			ucontrol->value.integer.value[0];
		mt6397_dmic_power_off(mt6397_data);
		mt6397_clk_off(mt6397_data);
	}
	return 0;
}

static int mt6397_adc1_get(struct snd_kcontrol *kcontrol,
			   struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct mt6397_codec_priv *mt6397_data =
		snd_soc_codec_get_drvdata(codec);

	dev_dbg(mt6397_data->mt6397_codec->dev,
		"AudCodec %s() = %d\n", __func__,
		mt6397_data->power[MTCODEC_DEVICE_ADC1]);
	ucontrol->value.integer.value[0] =
		mt6397_data->power[MTCODEC_DEVICE_ADC1];
	return 0;
}

static int mt6397_adc1_set(struct snd_kcontrol *kcontrol,
			   struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct mt6397_codec_priv *mt6397_data =
		snd_soc_codec_get_drvdata(codec);

	dev_dbg(mt6397_data->mt6397_codec->dev, "AudCodec %s()\n", __func__);
	if (ucontrol->value.integer.value[0]) {
		mt6397_clk_on(mt6397_data);
		mt6397_adc_power_on(mt6397_data);
		mt6397_data->power[MTCODEC_DEVICE_ADC1] =
			ucontrol->value.integer.value[0];
	} else {
		mt6397_data->power[MTCODEC_DEVICE_ADC1] =
			ucontrol->value.integer.value[0];
		mt6397_adc_power_off(mt6397_data);
		mt6397_clk_off(mt6397_data);
	}
	return 0;
}

static int mt6397_adc2_get(struct snd_kcontrol *kcontrol,
			   struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct mt6397_codec_priv *mt6397_data =
		snd_soc_codec_get_drvdata(codec);

	dev_dbg(mt6397_data->mt6397_codec->dev,
		"AudCodec %s() = %d\n", __func__,
		mt6397_data->power[MTCODEC_DEVICE_ADC2]);
	ucontrol->value.integer.value[0] =
		mt6397_data->power[MTCODEC_DEVICE_ADC2];
	return 0;
}

static int mt6397_adc2_set(struct snd_kcontrol *kcontrol,
			   struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct mt6397_codec_priv *mt6397_data =
		snd_soc_codec_get_drvdata(codec);

	dev_dbg(mt6397_data->mt6397_codec->dev, "AudCodec %s()\n", __func__);
	if (ucontrol->value.integer.value[0]) {
		mt6397_clk_on(mt6397_data);
		mt6397_adc_power_on(mt6397_data);
		mt6397_data->power[MTCODEC_DEVICE_ADC2] =
			ucontrol->value.integer.value[0];
	} else {
		mt6397_data->power[MTCODEC_DEVICE_ADC2] =
			ucontrol->value.integer.value[0];
		mt6397_adc_power_off(mt6397_data);
		mt6397_clk_off(mt6397_data);
	}
	return 0;
}

static int mt6397_preamp1_get(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct mt6397_codec_priv *mt6397_data =
		snd_soc_codec_get_drvdata(codec);

	dev_dbg(mt6397_data->mt6397_codec->dev,
		"AudCodec %s() mt6397_data->mux[MTCODEC_MUX_PREAMP1] = %d\n",
		__func__, mt6397_data->mux[MTCODEC_MUX_PREAMP1]);
	ucontrol->value.integer.value[0] =
		mt6397_data->mux[MTCODEC_MUX_PREAMP1];
	return 0;
}

static int mt6397_preamp1_set(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct mt6397_codec_priv *mt6397_data =
		snd_soc_codec_get_drvdata(codec);

	dev_dbg(mt6397_data->mt6397_codec->dev, "AudCodec %s()\n", __func__);

	if (ucontrol->value.enumerated.item[0] >
	    ARRAY_SIZE(mt6397_preamp_mux_function)) {
		dev_err(mt6397_data->mt6397_codec->dev, "AudCodec return -EINVAL\n");
		return -EINVAL;
	}

	if (ucontrol->value.integer.value[0] == 1) {
		mt6397_mux(mt6397_data, MTCODEC_DEVICE_PREAMP_L,
			   MTCODEC_MUX_MIC1);
	} else if (ucontrol->value.integer.value[0] == 2) {
		mt6397_mux(mt6397_data, MTCODEC_DEVICE_PREAMP_L,
			   MTCODEC_MUX_MIC2);
	} else if (ucontrol->value.integer.value[0] == 3) {
		mt6397_mux(mt6397_data, MTCODEC_DEVICE_PREAMP_L,
			   MTCODEC_MUX_MIC3);
	} else {
		dev_warn(mt6397_data->mt6397_codec->dev,
			 "AudCodec mt6397_mux warning");
	}

	dev_dbg(mt6397_data->mt6397_codec->dev,
		"AudCodec %s() done\n", __func__);
	mt6397_data->mux[MTCODEC_MUX_PREAMP1] =
		ucontrol->value.integer.value[0];
	return 0;
}

static int mt6397_preamp2_get(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct mt6397_codec_priv *mt6397_data =
		snd_soc_codec_get_drvdata(codec);

	dev_dbg(mt6397_data->mt6397_codec->dev, "AudCodec %s() %d\n", __func__,
		mt6397_data->mux[MTCODEC_MUX_PREAMP2]);
	ucontrol->value.integer.value[0] =
		mt6397_data->mux[MTCODEC_MUX_PREAMP2];
	return 0;
}

static int mt6397_preamp2_set(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct mt6397_codec_priv *mt6397_data =
		snd_soc_codec_get_drvdata(codec);

	dev_dbg(mt6397_data->mt6397_codec->dev, "AudCodec %s()\n", __func__);

	if (ucontrol->value.enumerated.item[0] >
	    ARRAY_SIZE(mt6397_preamp_mux_function)) {
		dev_err(mt6397_data->mt6397_codec->dev, "AudCodec return -EINVAL\n");
		return -EINVAL;
	}
	if (ucontrol->value.integer.value[0] == 1) {
		mt6397_mux(mt6397_data, MTCODEC_DEVICE_PREAMP_R,
			   MTCODEC_MUX_MIC1);
	} else if (ucontrol->value.integer.value[0] == 2) {
		mt6397_mux(mt6397_data, MTCODEC_DEVICE_PREAMP_R,
			   MTCODEC_MUX_MIC2);
	} else if (ucontrol->value.integer.value[0] == 3) {
		mt6397_mux(mt6397_data, MTCODEC_DEVICE_PREAMP_R,
			   MTCODEC_MUX_MIC3);
	}
	dev_dbg(mt6397_data->mt6397_codec->dev,
		"AudCodec %s() done\n", __func__);
	mt6397_data->mux[MTCODEC_MUX_PREAMP2] =
		ucontrol->value.integer.value[0];
	return 0;
}

static int mt6397_pga1_get(struct snd_kcontrol *kcontrol,
			   struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct mt6397_codec_priv *mt6397_data =
		snd_soc_codec_get_drvdata(codec);

	dev_dbg(mt6397_data->mt6397_codec->dev,
		"AudCodec %s() = %d\n", __func__,
		mt6397_data->volume[MTCODEC_VOL_MICAMPL]);
	ucontrol->value.integer.value[0] =
		mt6397_data->volume[MTCODEC_VOL_MICAMPL];
	return 0;
}

static int mt6397_pga1_set(struct snd_kcontrol *kcontrol,
			   struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct mt6397_codec_priv *mt6397_data =
		snd_soc_codec_get_drvdata(codec);
	int index = 0;

	dev_dbg(mt6397_data->mt6397_codec->dev, "AudCodec %s()\n", __func__);
	if (ucontrol->value.enumerated.item[0] >
	    ARRAY_SIZE(mt6397_adc_ul_pag_gain)) {
		dev_err(mt6397_data->mt6397_codec->dev, "AudCodec return -EINVAL\n");
		return -EINVAL;
	}
	index = ucontrol->value.integer.value[0];
	mt6397_set_reg(mt6397_data, MT6397_AUDPREAMPGAIN_CON0,
		       index << 0, 0x00000007);
	mt6397_data->volume[MTCODEC_VOL_MICAMPL] =
		ucontrol->value.integer.value[0];
	return 0;
}

static int mt6397_pga2_get(struct snd_kcontrol *kcontrol,
			   struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct mt6397_codec_priv *mt6397_data =
		snd_soc_codec_get_drvdata(codec);

	dev_dbg(mt6397_data->mt6397_codec->dev,
		"AudCodec %s() = %d\n", __func__,
		mt6397_data->volume[MTCODEC_VOL_MICAMPR]);
	ucontrol->value.integer.value[0] =
		mt6397_data->volume[MTCODEC_VOL_MICAMPR];
	return 0;
}

static int mt6397_pga2_set(struct snd_kcontrol *kcontrol,
			   struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct mt6397_codec_priv *mt6397_data =
		snd_soc_codec_get_drvdata(codec);
	int index = 0;

	dev_dbg(mt6397_data->mt6397_codec->dev, "AudCodec %s()\n", __func__);

	if (ucontrol->value.enumerated.item[0] >
	    ARRAY_SIZE(mt6397_adc_ul_pag_gain)) {
		dev_err(mt6397_data->mt6397_codec->dev, "AudCodec return -EINVAL\n");
		return -EINVAL;
	}
	index = ucontrol->value.integer.value[0];
	mt6397_set_reg(mt6397_data, MT6397_AUDPREAMPGAIN_CON0,
		       index << 4, 0x00000070);
	mt6397_data->volume[MTCODEC_VOL_MICAMPR] =
		ucontrol->value.integer.value[0];
	return 0;
}

static const struct snd_kcontrol_new mt6397_ul_controls[] = {
	SOC_ENUM_EXT("Audio_ADC_1_Switch", mt6397_ul_euum[0],
		     mt6397_adc1_get, mt6397_adc1_set),
	SOC_ENUM_EXT("Audio_ADC_2_Switch", mt6397_ul_euum[1],
		     mt6397_adc2_get, mt6397_adc2_set),
	SOC_ENUM_EXT("Audio_Preamp1_Switch", mt6397_ul_euum[2],
		     mt6397_preamp1_get, mt6397_preamp1_set),
	SOC_ENUM_EXT("Audio_Preamp2_Switch", mt6397_ul_euum[3],
		     mt6397_preamp2_get, mt6397_preamp2_set),
	SOC_ENUM_EXT("Audio_PGA1_Setting", mt6397_ul_euum[4],
		     mt6397_pga1_get, mt6397_pga1_set),
	SOC_ENUM_EXT("Audio_PGA2_Setting", mt6397_ul_euum[5],
		     mt6397_pga2_get, mt6397_pga2_set),
	SOC_ENUM_EXT("Audio_Digital_Mic_Switch", mt6397_ul_euum[6],
		     mt6397_dmic_get, mt6397_dmic_set),
};

static void mt6397_codec_init_reg(struct mt6397_codec_priv *mt6397_data)
{
	dev_dbg(mt6397_data->mt6397_codec->dev, "AudCodec mt6397_codec_init_reg\n");

	/* power_init */
	/*RG_CLKSQ_EN*/
	mt6397_set_reg(mt6397_data, MT6397_TOP_CKCON1, 0x0000, 0x0010);
	mt6397_set_reg(mt6397_data, MT6397_AFUNC_AUD_CON2, 0x0080, 0x0080);
	mt6397_set_reg(mt6397_data, MT6397_ZCD_CON2, 0x0c0c, 0xffff);
	mt6397_set_reg(mt6397_data, MT6397_AUDBUF_CFG0, 0x0000, 0x1fe7);
	/* RG DEV ck off; */
	mt6397_set_reg(mt6397_data, MT6397_IBIASDIST_CFG0, 0x1552, 0xffff);
	/* NCP off */
	mt6397_set_reg(mt6397_data, MT6397_AUDDAC_CON0, 0x0000, 0xffff);
	mt6397_set_reg(mt6397_data, MT6397_AUDCLKGEN_CFG0, 0x0000, 0x0001);
	/* need check */
	mt6397_set_reg(mt6397_data, MT6397_AUDNVREGGLB_CFG0, 0x0006, 0xffff);
	/* fix me */
	mt6397_set_reg(mt6397_data, MT6397_NCP_CLKDIV_CON1, 0x0001, 0xffff);
	mt6397_set_reg(mt6397_data, MT6397_AUD_NCP0, 0x0000, 0x6000);
	mt6397_set_reg(mt6397_data, MT6397_AUDLDO_CFG0, 0x0192, 0xffff);
	mt6397_set_reg(mt6397_data, MT6397_AFUNC_AUD_CON2, 0x0000, 0x0080);
	/* gain step gain and enable */
	mt6397_set_reg(mt6397_data, MT6397_ZCD_CON0, 0x0101, 0xffff);
}

static int mt6397_debug_open(struct inode *inode, struct file *file)
{
	file->private_data = inode->i_private;
	return 0;
}

static ssize_t mt6397_debug_read(struct file *file, char __user *buf,
				 size_t count, loff_t *pos)
{
	struct mt6397_codec_priv *mt6397_data = file->private_data;
	const int size = 4096;
	char buffer[size];
	int n = 0;

	/* FIXME check no need enable aud clk? */
	n += scnprintf(buffer + n, size - n,
		       "======PMIC digital registers====\n");
	n += scnprintf(buffer + n, size - n, "UL_DL_CON0 = 0x%x\n",
		       mt6397_get_reg(mt6397_data, MT6397_AFE_UL_DL_CON0));
	n += scnprintf(buffer + n, size - n, "DL_SRC2_CON0_H = 0x%x\n",
		       mt6397_get_reg(mt6397_data, MT6397_AFE_DL_SRC2_CON0_H));
	n += scnprintf(buffer + n, size - n, "DL_SRC2_CON0_L = 0x%x\n",
		       mt6397_get_reg(mt6397_data, MT6397_AFE_DL_SRC2_CON0_L));
	n += scnprintf(buffer + n, size - n, "DL_SDM_CON0 = 0x%x\n",
		       mt6397_get_reg(mt6397_data, MT6397_AFE_DL_SDM_CON0));
	n += scnprintf(buffer + n, size - n, "DL_SDM_CON1 = 0x%x\n",
		       mt6397_get_reg(mt6397_data, MT6397_AFE_DL_SDM_CON1));
	n += scnprintf(buffer + n, size - n, "UL_SRC_CON0_H = 0x%x\n",
		       mt6397_get_reg(mt6397_data, MT6397_AFE_UL_SRC_CON0_H));
	n += scnprintf(buffer + n, size - n, "UL_SRC_CON0_L = 0x%x\n",
		       mt6397_get_reg(mt6397_data, MT6397_AFE_UL_SRC_CON0_L));
	n += scnprintf(buffer + n, size - n, "UL_SRC_CON1_H = 0x%x\n",
		       mt6397_get_reg(mt6397_data, MT6397_AFE_UL_SRC_CON1_H));
	n += scnprintf(buffer + n, size - n, "UL_SRC_CON1_L = 0x%x\n",
		       mt6397_get_reg(mt6397_data, MT6397_AFE_UL_SRC_CON1_L));
	n += scnprintf(buffer + n, size - n, "AFE_TOP_CON0 = 0x%x\n",
		       mt6397_get_reg(mt6397_data, MT6397_AFE_TOP_CON0));
	n += scnprintf(buffer + n, size - n, "AFUNC_AUD_CON0 = 0x%x\n",
		       mt6397_get_reg(mt6397_data, MT6397_AFUNC_AUD_CON0));
	n += scnprintf(buffer + n, size - n, "AFUNC_AUD_CON1 = 0x%x\n",
		       mt6397_get_reg(mt6397_data, MT6397_AFUNC_AUD_CON1));
	n += scnprintf(buffer + n, size - n, "AFUNC_AUD_CON2 = 0x%x\n",
		       mt6397_get_reg(mt6397_data, MT6397_AFUNC_AUD_CON2));
	n += scnprintf(buffer + n, size - n, "AFUNC_AUD_CON3 = 0x%x\n",
		       mt6397_get_reg(mt6397_data, MT6397_AFUNC_AUD_CON3));
	n += scnprintf(buffer + n, size - n, "AFUNC_AUD_CON4 = 0x%x\n",
		       mt6397_get_reg(mt6397_data, MT6397_AFUNC_AUD_CON4));
	n += scnprintf(buffer + n, size - n, "AFUNC_AUD_MON0 = 0x%x\n",
		       mt6397_get_reg(mt6397_data, MT6397_AFUNC_AUD_MON0));
	n += scnprintf(buffer + n, size - n, "AFUNC_AUD_MON1 = 0x%x\n",
		       mt6397_get_reg(mt6397_data, MT6397_AFUNC_AUD_MON1));
	n += scnprintf(buffer + n, size - n, "AUDRC_TUNE_MON0 = 0x%x\n",
		       mt6397_get_reg(mt6397_data, MT6397_AUDRC_TUNE_MON0));
	n += scnprintf(buffer + n, size - n, "AFE_UP8X_FIFO_CFG0 = 0x%x\n",
		       mt6397_get_reg(mt6397_data, MT6397_AFE_UP8X_FIFO_CFG0));
	n += scnprintf(buffer + n, size - n, "AFE_UP8X_FIFO_LOG_MON0 = 0x%x\n",
		       mt6397_get_reg(mt6397_data,
				      MT6397_AFE_UP8X_FIFO_LOG_MON0));
	n += scnprintf(buffer + n, size - n, "AFE_UP8X_FIFO_LOG_MON1 = 0x%x\n",
		       mt6397_get_reg(mt6397_data,
				      MT6397_AFE_UP8X_FIFO_LOG_MON1));
	n += scnprintf(buffer + n, size - n, "AFE_DL_DC_COMP_CFG0 = 0x%x\n",
		       mt6397_get_reg(mt6397_data, MT6397_AFE_DL_DC_COMP_CFG0));
	n += scnprintf(buffer + n, size - n, "AFE_DL_DC_COMP_CFG1 = 0x%x\n",
		       mt6397_get_reg(mt6397_data, MT6397_AFE_DL_DC_COMP_CFG1));
	n += scnprintf(buffer + n, size - n, "AFE_DL_DC_COMP_CFG2 = 0x%x\n",
		       mt6397_get_reg(mt6397_data, MT6397_AFE_DL_DC_COMP_CFG2));
	n += scnprintf(buffer + n, size - n, "AFE_PMIC_NEWIF_CFG0 = 0x%x\n",
		       mt6397_get_reg(mt6397_data, MT6397_AFE_PMIC_NEWIF_CFG0));
	n += scnprintf(buffer + n, size - n, "AFE_PMIC_NEWIF_CFG1 = 0x%x\n",
		       mt6397_get_reg(mt6397_data, MT6397_AFE_PMIC_NEWIF_CFG1));
	n += scnprintf(buffer + n, size - n, "AFE_PMIC_NEWIF_CFG2 = 0x%x\n",
		       mt6397_get_reg(mt6397_data, MT6397_AFE_PMIC_NEWIF_CFG2));
	n += scnprintf(buffer + n, size - n, "AFE_PMIC_NEWIF_CFG3 = 0x%x\n",
		       mt6397_get_reg(mt6397_data, MT6397_AFE_PMIC_NEWIF_CFG3));
	n += scnprintf(buffer + n, size - n, "AFE_SGEN_CFG0 = 0x%x\n",
		       mt6397_get_reg(mt6397_data, MT6397_AFE_SGEN_CFG0));
	n += scnprintf(buffer + n, size - n, "AFE_SGEN_CFG1 = 0x%x\n",
		       mt6397_get_reg(mt6397_data, MT6397_AFE_SGEN_CFG1));
	n += scnprintf(buffer + n, size - n,
		       "======PMIC analog registers====\n");
	n += scnprintf(buffer + n, size - n, "TOP_CKPDN = 0x%x\n",
		       mt6397_get_reg(mt6397_data, MT6397_TOP_CKPDN));
	n += scnprintf(buffer + n, size - n, "TOP_CKPDN2 = 0x%x\n",
		       mt6397_get_reg(mt6397_data, MT6397_TOP_CKPDN2));
	n += scnprintf(buffer + n, size - n, "TOP_CKCON1 = 0x%x\n",
		       mt6397_get_reg(mt6397_data, MT6397_TOP_CKCON1));
	n += scnprintf(buffer + n, size - n, "SPK_CON0 = 0x%x\n",
		       mt6397_get_reg(mt6397_data, MT6397_SPK_CON0));
	n += scnprintf(buffer + n, size - n, "SPK_CON1 = 0x%x\n",
		       mt6397_get_reg(mt6397_data, MT6397_SPK_CON1));
	n += scnprintf(buffer + n, size - n, "SPK_CON2 = 0x%x\n",
		       mt6397_get_reg(mt6397_data, MT6397_SPK_CON2));
	n += scnprintf(buffer + n, size - n, "SPK_CON3 = 0x%x\n",
		       mt6397_get_reg(mt6397_data, MT6397_SPK_CON3));
	n += scnprintf(buffer + n, size - n, "SPK_CON4 = 0x%x\n",
		       mt6397_get_reg(mt6397_data, MT6397_SPK_CON4));
	n += scnprintf(buffer + n, size - n, "SPK_CON5 = 0x%x\n",
		       mt6397_get_reg(mt6397_data, MT6397_SPK_CON5));
	n += scnprintf(buffer + n, size - n, "SPK_CON6 = 0x%x\n",
		       mt6397_get_reg(mt6397_data, MT6397_SPK_CON6));
	n += scnprintf(buffer + n, size - n, "SPK_CON7 = 0x%x\n",
		       mt6397_get_reg(mt6397_data, MT6397_SPK_CON7));
	n += scnprintf(buffer + n, size - n, "SPK_CON8 = 0x%x\n",
		       mt6397_get_reg(mt6397_data, MT6397_SPK_CON8));
	n += scnprintf(buffer + n, size - n, "SPK_CON9 = 0x%x\n",
		       mt6397_get_reg(mt6397_data, MT6397_SPK_CON9));
	n += scnprintf(buffer + n, size - n, "SPK_CON10 = 0x%x\n",
		       mt6397_get_reg(mt6397_data, MT6397_SPK_CON10));
	n += scnprintf(buffer + n, size - n, "SPK_CON11 = 0x%x\n",
		       mt6397_get_reg(mt6397_data, MT6397_SPK_CON11));
	n += scnprintf(buffer + n, size - n, "AUDDAC_CON0 = 0x%x\n",
		       mt6397_get_reg(mt6397_data, MT6397_AUDDAC_CON0));
	n += scnprintf(buffer + n, size - n, "AUDBUF_CFG0 = 0x%x\n",
		       mt6397_get_reg(mt6397_data, MT6397_AUDBUF_CFG0));
	n += scnprintf(buffer + n, size - n, "AUDBUF_CFG1 = 0x%x\n",
		       mt6397_get_reg(mt6397_data, MT6397_AUDBUF_CFG1));
	n += scnprintf(buffer + n, size - n, "AUDBUF_CFG2 = 0x%x\n",
		       mt6397_get_reg(mt6397_data, MT6397_AUDBUF_CFG2));
	n += scnprintf(buffer + n, size - n, "AUDBUF_CFG3 = 0x%x\n",
		       mt6397_get_reg(mt6397_data, MT6397_AUDBUF_CFG3));
	n += scnprintf(buffer + n, size - n, "AUDBUF_CFG4 = 0x%x\n",
		       mt6397_get_reg(mt6397_data, MT6397_AUDBUF_CFG4));
	n += scnprintf(buffer + n, size - n, "IBIASDIST_CFG0 = 0x%x\n",
		       mt6397_get_reg(mt6397_data, MT6397_IBIASDIST_CFG0));
	n += scnprintf(buffer + n, size - n, "AUDACCDEPOP_CFG0 = 0x%x\n",
		       mt6397_get_reg(mt6397_data, MT6397_AUDACCDEPOP_CFG0));
	n += scnprintf(buffer + n, size - n, "AUD_IV_CFG0 = 0x%x\n",
		       mt6397_get_reg(mt6397_data, MT6397_AUD_IV_CFG0));
	n += scnprintf(buffer + n, size - n, "AUDCLKGEN_CFG0 = 0x%x\n",
		       mt6397_get_reg(mt6397_data, MT6397_AUDCLKGEN_CFG0));
	n += scnprintf(buffer + n, size - n, "AUDLDO_CFG0 = 0x%x\n",
		       mt6397_get_reg(mt6397_data, MT6397_AUDLDO_CFG0));
	n += scnprintf(buffer + n, size - n, "AUDLDO_CFG1 = 0x%x\n",
		       mt6397_get_reg(mt6397_data, MT6397_AUDLDO_CFG1));
	n += scnprintf(buffer + n, size - n, "AUDNVREGGLB_CFG0 = 0x%x\n",
		       mt6397_get_reg(mt6397_data, MT6397_AUDNVREGGLB_CFG0));
	n += scnprintf(buffer + n, size - n, "AUD_NCP0 = 0x%x\n",
		       mt6397_get_reg(mt6397_data, MT6397_AUD_NCP0));
	n += scnprintf(buffer + n, size - n, "AUDPREAMP_CON0 = 0x%x\n",
		       mt6397_get_reg(mt6397_data, MT6397_AUDPREAMP_CON0));
	n += scnprintf(buffer + n, size - n, "AUDADC_CON0 = 0x%x\n",
		       mt6397_get_reg(mt6397_data, MT6397_AUDADC_CON0));
	n += scnprintf(buffer + n, size - n, "AUDADC_CON1 = 0x%x\n",
		       mt6397_get_reg(mt6397_data, MT6397_AUDADC_CON1));
	n += scnprintf(buffer + n, size - n, "AUDADC_CON2 = 0x%x\n",
		       mt6397_get_reg(mt6397_data, MT6397_AUDADC_CON2));
	n += scnprintf(buffer + n, size - n, "AUDADC_CON3 = 0x%x\n",
		       mt6397_get_reg(mt6397_data, MT6397_AUDADC_CON3));
	n += scnprintf(buffer + n, size - n, "AUDADC_CON4 = 0x%x\n",
		       mt6397_get_reg(mt6397_data, MT6397_AUDADC_CON4));
	n += scnprintf(buffer + n, size - n, "AUDADC_CON5 = 0x%x\n",
		       mt6397_get_reg(mt6397_data, MT6397_AUDADC_CON5));
	n += scnprintf(buffer + n, size - n, "AUDADC_CON6 = 0x%x\n",
		       mt6397_get_reg(mt6397_data, MT6397_AUDADC_CON6));
	n += scnprintf(buffer + n, size - n, "AUDDIGMI_CON0 = 0x%x\n",
		       mt6397_get_reg(mt6397_data, MT6397_AUDDIGMI_CON0));
	n += scnprintf(buffer + n, size - n, "AUDLSBUF_CON0 = 0x%x\n",
		       mt6397_get_reg(mt6397_data, MT6397_AUDLSBUF_CON0));
	n += scnprintf(buffer + n, size - n, "AUDLSBUF_CON1 = 0x%x\n",
		       mt6397_get_reg(mt6397_data, MT6397_AUDLSBUF_CON1));
	n += scnprintf(buffer + n, size - n, "AUDENCSPARE_CON0 = 0x%x\n",
		       mt6397_get_reg(mt6397_data, MT6397_AUDENCSPARE_CON0));
	n += scnprintf(buffer + n, size - n, "AUDENCCLKSQ_CON0 = 0x%x\n",
		       mt6397_get_reg(mt6397_data, MT6397_AUDENCCLKSQ_CON0));
	n += scnprintf(buffer + n, size - n, "AUDPREAMPGAIN_CON0 = 0x%x\n",
		       mt6397_get_reg(mt6397_data, MT6397_AUDPREAMPGAIN_CON0));
	n += scnprintf(buffer + n, size - n, "ZCD_CON0 = 0x%x\n",
		       mt6397_get_reg(mt6397_data, MT6397_ZCD_CON0));
	n += scnprintf(buffer + n, size - n, "ZCD_CON1 = 0x%x\n",
		       mt6397_get_reg(mt6397_data, MT6397_ZCD_CON1));
	n += scnprintf(buffer + n, size - n, "ZCD_CON2 = 0x%x\n",
		       mt6397_get_reg(mt6397_data, MT6397_ZCD_CON2));
	n += scnprintf(buffer + n, size - n, "ZCD_CON3 = 0x%x\n",
		       mt6397_get_reg(mt6397_data, MT6397_ZCD_CON3));
	n += scnprintf(buffer + n, size - n, "ZCD_CON4 = 0x%x\n",
		       mt6397_get_reg(mt6397_data, MT6397_ZCD_CON4));
	n += scnprintf(buffer + n, size - n, "ZCD_CON5 = 0x%x\n",
		       mt6397_get_reg(mt6397_data, MT6397_ZCD_CON5));
	n += scnprintf(buffer + n, size - n, "NCP_CLKDIV_CON0 = 0x%x\n",
		       mt6397_get_reg(mt6397_data, MT6397_NCP_CLKDIV_CON0));
	n += scnprintf(buffer + n, size - n, "NCP_CLKDIV_CON1 = 0x%x\n",
		       mt6397_get_reg(mt6397_data, MT6397_NCP_CLKDIV_CON1));
	dev_notice(mt6397_data->mt6397_codec->dev,
		   "mt6397_debug_read len = %d\n", n);

	return simple_read_from_buffer(buf, count, pos, buffer, n);
}

static const struct file_operations mt6397_debug_ops = {
	.open = mt6397_debug_open,
	.read = mt6397_debug_read,
};

static int mt6397_codec_probe(struct snd_soc_codec *codec)
{
	struct mt6397_codec_priv *mt6397_data = NULL;

	dev_info(codec->dev, "%s()\n", __func__);

	/* add codec controls */
	snd_soc_add_codec_controls(codec, mt6397_snd_controls,
				   ARRAY_SIZE(mt6397_snd_controls));
	snd_soc_add_codec_controls(codec, mt6397_ul_controls,
				   ARRAY_SIZE(mt6397_ul_controls));
	snd_soc_add_codec_controls(codec, mt6397_factory_controls,
				   ARRAY_SIZE(mt6397_factory_controls));

	/* here to set  private data */
	mt6397_data = devm_kzalloc(codec->dev, sizeof(*mt6397_data),
				   GFP_KERNEL);
	if (!mt6397_data)
		return -ENOMEM;
	mt6397_data->mt6397_codec = codec;
	mt6397_data->mt6397_fs[MTCODEC_DEVICE_DAC] = 48000;
	mt6397_data->mt6397_fs[MTCODEC_DEVICE_ADC] = 48000;

	snd_soc_codec_set_drvdata(codec, mt6397_data);

	mt6397_codec_init_reg(mt6397_data);

	/* TRIM FUNCTION */
	/* Get HP Trim Offset */
	mt6397_get_trim_offset(mt6397_data);
	mt6397_spk_auto_trim_offset(mt6397_data);

	mt6397_debugfs = debugfs_create_file("mt6397reg", S_IFREG | S_IRUGO,
					     codec->component.debugfs_root,
					     (void *)mt6397_data,
					     &mt6397_debug_ops);
	return 0;
}

static int mt6397_codec_remove(struct snd_soc_codec *codec)
{
	return 0;
}

static struct regmap *mt6397_codec_get_regmap(struct device *dev)
{
	struct mt6397_chip *mt6397;

	mt6397 = dev_get_drvdata(dev->parent);

	return mt6397->regmap;
}

static struct snd_soc_codec_driver mt6397_soc_codec_driver = {
	.probe = mt6397_codec_probe,
	.remove = mt6397_codec_remove,
	.get_regmap = mt6397_codec_get_regmap,

};

static int mt6397_codec_dev_probe(struct platform_device *pdev)
{
	if (pdev->dev.of_node) {
		dev_set_name(&pdev->dev, "%s", "mt6397-codec");
		dev_notice(&pdev->dev, "%s set dev name %s\n", __func__,
			   dev_name(&pdev->dev));
	}
	return snd_soc_register_codec(&pdev->dev, &mt6397_soc_codec_driver,
			mt6397_dai_drvs, ARRAY_SIZE(mt6397_dai_drvs));
}

static int mt6397_codec_dev_remove(struct platform_device *pdev)
{
	dev_dbg(&pdev->dev, "AudCodec %s:\n", __func__);
	snd_soc_unregister_codec(&pdev->dev);
	debugfs_remove(mt6397_debugfs);
	return 0;
}

static const struct of_device_id mt6397_dt_match[] = {
	{ .compatible = "mediatek,mt6397-codec", },
	{ }
};

static struct platform_driver mt6397_codec_driver = {
	.driver = {
		.name = "mt6397-codec",
		.owner = THIS_MODULE,
		.of_match_table = mt6397_dt_match,
	},
	.probe = mt6397_codec_dev_probe,
	.remove = mt6397_codec_dev_remove,
};

module_platform_driver(mt6397_codec_driver);

/* Module information */
MODULE_DESCRIPTION("MT6397 ALSA SoC codec driver");
MODULE_AUTHOR("Koro Chen <koro.chen@mediatek.com>");
MODULE_LICENSE("GPL v2");
MODULE_DEVICE_TABLE(of, mt6397_dt_match);

