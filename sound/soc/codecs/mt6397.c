/*
 * mt6397.c  --  MT6397 ALSA SoC codec driver
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

#include "mt6397.h"
#include <linux/module.h>
#include <sound/soc.h>
#include <linux/mfd/mt6397/core.h>
#include <linux/delay.h>
#include <linux/debugfs.h>

static struct mt6397_codec_data_priv *codec_data;
static struct snd_soc_codec *codec_control;
static struct dentry *mt_codec_debugfs;
int aud_ana_clk_cntr;

static uint32_t codec_fs[MTCODEC_DEVICE_INOUT_MAX] = { 44100, 48000 };

static DEFINE_MUTEX(ana_ctrl_mutex);
static DEFINE_MUTEX(auddrv_pmic_mutex);

/* Function implementation */
static void speaker_amp_change(bool enable);
static void audio_amp_change(int channels, bool enable);
static void headset_speaker_amp_change(bool enable);


static void ana_set_codec(struct snd_soc_codec *codec)
{
	codec_control = codec;
}

static void ana_set_reg(uint32_t offset, uint32_t value, uint32_t mask)
{
	snd_soc_update_bits(codec_control, offset, mask, value);
}

static void ana_enable_clk(uint32_t mask, bool enable)
{
	/* set pmic register or analog CONTROL_IFACE_PATH */
	uint32_t val;
	uint32_t reg = enable ? MT6397_TOP_CKPDN_CLR : MT6397_TOP_CKPDN_SET;

	snd_soc_update_bits(codec_control, reg, mask, mask);
	val = snd_soc_read(codec_control, MT6397_TOP_CKPDN);
	if (val < 0)
		pr_err("error ana_enable_clk\n");

	if ((val & mask) != (enable ? 0 : mask))
		pr_err("%s: data mismatch: mask=%04X, val=%04X, enable=%d\n",
		       __func__, mask, val, enable);
}

static uint32_t ana_get_reg(uint32_t offset)
{
	uint32_t ret;

	ret = snd_soc_read(codec_control, offset);
	if (ret < 0)
		pr_err("error ana_get_reg\n");
	return ret;
}

/*****************************************************************************
 * FUNCTION
 *  aud_drv_ana_clk_on / aud_drv_ana_clk_off  upstream todo wait pmic ccf
 *
 * DESCRIPTION
 *  Enable/Disable analog part clock
 *
 *****************************************************************************/
static void aud_drv_ana_clk_on(void)
{
	mutex_lock(&auddrv_pmic_mutex);
	if (aud_ana_clk_cntr == 0) {
		pr_debug("+%s aud_ana_clk_cntr:%d\n", __func__,
			 aud_ana_clk_cntr);
		ana_set_reg(MT6397_TOP_CKCON1,
			0x0010, 0x0010); /*bit4: RG_CLKSQ_EN*/
		ana_enable_clk(0x0003, true);
	}
	aud_ana_clk_cntr++;
	mutex_unlock(&auddrv_pmic_mutex);
	pr_debug("-%s aud_ana_clk_cntr:%d\n", __func__, aud_ana_clk_cntr);
}

static void aud_drv_ana_clk_off(void)
{
	mutex_lock(&auddrv_pmic_mutex);
	aud_ana_clk_cntr--;
	if (aud_ana_clk_cntr == 0) {
		pr_debug("+%s Ana clk(%x)\n", __func__, aud_ana_clk_cntr);
		/* Disable ADC clock */
		/*bit4: RG_CLKSQ_EN*/
		ana_set_reg(MT6397_TOP_CKCON1, 0x0000, 0x0010);
		ana_enable_clk(0x0003, false);


	} else if (aud_ana_clk_cntr < 0) {
		pr_err("!!Aud_ADC_Clk_cntr<0 (%d)\n",  aud_ana_clk_cntr);
		/*AUDIO_ASSERT(true);*/
		aud_ana_clk_cntr = 0;
	}
	mutex_unlock(&auddrv_pmic_mutex);
	pr_debug("-%s , Aud_ADC_Clk_cntr:%d\n", __func__, aud_ana_clk_cntr);
}

uint32_t get_ul_frequency(uint32_t frequency)
{
	uint32_t reg_value = 0;

	pr_debug("AudCodec get_ul_frequency ApplyDLFrequency = %d", frequency);

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
		pr_warn("AudCodec get_ul_frequency with unsupported frequency = %d",
			frequency);
	}
	pr_debug("AudCodec get_ul_frequency reg_value = %d", reg_value);
	return reg_value;
}

static int mt6397_codec_startup(struct snd_pcm_substream *substream,
				struct snd_soc_dai *dai_port)
{
	if (substream->stream == SNDRV_PCM_STREAM_CAPTURE)
		pr_debug("%s SNDRV_PCM_STREAM_CAPTURE\n", __func__);
	else if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		pr_debug("%s SNDRV_PCM_STREAM_PLAYBACK\n", __func__);

	return 0;
}

static int mt6397_codec_prepare(struct snd_pcm_substream *substream,
				struct snd_soc_dai *dai_port)
{
	if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
		pr_debug("AudCodec %s set up SNDRV_PCM_STREAM_CAPTURE rate = %d\n",
			 __func__, substream->runtime->rate);
		codec_fs[MTCODEC_DEVICE_ADC] = substream->runtime->rate;

	} else if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		pr_debug("AudCodec %s set up SNDRV_PCM_STREAM_PLAYBACK rate = %d\n",
			 __func__, substream->runtime->rate);
		codec_fs[MTCODEC_DEVICE_DAC] = substream->runtime->rate;
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

	pr_debug("%s command = %d\n ", __func__, command);
	return 0;
}

static const struct snd_soc_dai_ops mt6397_aif1_dai_ops = {
	.startup = mt6397_codec_startup,
	.prepare = mt6397_codec_prepare,
	.trigger = mt6397_codec_trigger,
};

static struct snd_soc_dai_driver mtk_codec_dai_drvs[] = {
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

uint32_t dl_newif_fs(unsigned int frequency)
{
	uint32_t reg_value = 0;

	pr_debug("AudCodec %s frequency = %d\n", __func__, frequency);
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
		pr_warn("AudCodec %s unexpected frequency = %d\n",
			__func__, frequency);
	}
	return reg_value;
}

static bool get_dl_status(void)
{
	int i = 0;

	for (i = 0; i < MTCODEC_DEVICE_2IN1_SPK; i++) {
		if (codec_data->device_power[i])
			return true;
	}
	return false;
}

static bool get_uplink_status(void)
{
	int i = 0;

	for (i = MTCODEC_DEVICE_IN_ADC1; i < MTCODEC_DEVICE_MAX; i++) {
		if (codec_data->device_power[i])
			return true;
	}
	return false;
}


static void set_mux(enum MTCODEC_DEVICE_TYPE device_type,
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
			pr_warn("AudCodec set_mux: %d %d\n",
				device_type, mux_type);
		}
		/* bits 3,4 */
		ana_set_reg(MT6397_AUDBUF_CFG0, reg_value, 0x000000018);
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
			pr_warn("AudCodec set_mux: %d %d\n",
				device_type, mux_type);
		}
		ana_set_reg(MT6397_AUDBUF_CFG0, reg_value, 0x000001e0);
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
			pr_warn("AudCodec set_mux: %d %d\n",
				device_type, mux_type);
		}
		ana_set_reg(MT6397_AUDBUF_CFG0, reg_value, 0x00001e00);
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
			pr_warn("AudCodec set_mux: %d %d\n",
				device_type, mux_type);
		}
		ana_set_reg(MT6397_AUD_IV_CFG0, reg_value | (reg_value << 8),
			    0x00001c1c);
		break;
	case MTCODEC_DEVICE_IN_PREAMP_L:
		if (mux_type == MTCODEC_MUX_IN_MIC1) {
			reg_value = 1 << 2;
		} else if (mux_type == MTCODEC_MUX_IN_MIC2) {
			reg_value = 2 << 2;
		} else if (mux_type == MTCODEC_MUX_IN_MIC3) {
			reg_value = 3 << 2;
		} else {
			reg_value = 1 << 2;
			pr_warn("AudCodec set_mux: %d %d\n",
				device_type, mux_type);
		}
		ana_set_reg(MT6397_AUDPREAMP_CON0, reg_value, 0x0000001c);
		break;
	case MTCODEC_DEVICE_IN_PREAMP_R:
		if (mux_type == MTCODEC_MUX_IN_MIC1) {
			reg_value = 1 << 5;
		} else if (mux_type == MTCODEC_MUX_IN_MIC2) {
			reg_value = 2 << 5;
		} else if (mux_type == MTCODEC_MUX_IN_MIC3) {
			reg_value = 3 << 5;
		} else {
			reg_value = 1 << 5;
			pr_warn("AudCodec set_mux: %d %d\n",
				device_type, mux_type);
		}
		ana_set_reg(MT6397_AUDPREAMP_CON0, reg_value, 0x000000e0);
		break;
	case MTCODEC_DEVICE_IN_ADC1:
		if (mux_type == MTCODEC_MUX_IN_MIC1) {
			reg_value = 1 << 2;
		} else if (mux_type == MTCODEC_MUX_IN_PREAMP_1) {
			reg_value = 4 << 2;
		} else if (mux_type == MTCODEC_MUX_IN_LEVEL_SHIFT_BUFFER) {
			reg_value = 5 << 2;
		} else {
			reg_value = 1 << 2;
			pr_warn("AudCodec set_mux: %d %d\n",
				device_type, mux_type);
		}
		ana_set_reg(MT6397_AUDADC_CON0, reg_value, 0x0000001c);
		break;
	case MTCODEC_DEVICE_IN_ADC2:
		if (mux_type == MTCODEC_MUX_IN_MIC1) {
			reg_value = 1 << 5;
		} else if (mux_type == MTCODEC_MUX_IN_PREAMP_2) {
			reg_value = 4 << 5;
		} else if (mux_type == MTCODEC_MUX_IN_LEVEL_SHIFT_BUFFER) {
			reg_value = 5 << 5;
		} else {
			reg_value = 1 << 5;
			pr_warn("AudCodec set_mux: %d %d\n",
				device_type, mux_type);
		}
		ana_set_reg(MT6397_AUDADC_CON0, reg_value, 0x000000e0);
		break;
	default:
		break;
	}
}

static void turn_on_dac_power(void)
{
	ana_set_reg(AFE_PMIC_NEWIF_CFG0,
		    dl_newif_fs(codec_fs[MTCODEC_DEVICE_DAC]) << 12, 0xf000);
	ana_set_reg(AFUNC_AUD_CON2, 0x0006, 0xffff);
	ana_set_reg(AFUNC_AUD_CON0, 0xc3a1, 0xffff);
	ana_set_reg(AFUNC_AUD_CON2, 0x0003, 0xffff);
	ana_set_reg(AFUNC_AUD_CON2, 0x000b, 0xffff);
	ana_set_reg(AFE_DL_SDM_CON1, 0x001e, 0xffff);
	ana_set_reg(AFE_DL_SRC2_CON0_H, 0x0300 |
		    dl_newif_fs(codec_fs[MTCODEC_DEVICE_DAC]) << 12, 0x0ffff);
	ana_set_reg(AFE_UL_DL_CON0, 0x007f, 0xffff);
	ana_set_reg(AFE_DL_SRC2_CON0_L, 0x1801, 0xffff);
}

static void turn_off_dac_power(void)
{
	pr_debug("AudCodec turn_off_dac_power\n");

	ana_set_reg(AFE_DL_SRC2_CON0_L, 0x1800, 0xffff);
	if (!get_uplink_status())
		ana_set_reg(AFE_UL_DL_CON0, 0x0000, 0xffff);
}

static void spk_auto_trim_offset(void)
{
	uint32_t wait_for_ready = 0;
	uint32_t reg1 = 0;
	uint32_t chip_version = 0;
	int retyrcount = 50;

	ana_set_reg(AFUNC_AUD_CON2, 0x0080, 0x0080);
	/* enable VA28 , VA 33 VBAT ref , set dc */
	ana_set_reg(MT6397_AUDLDO_CFG0, 0x0D92, 0xffff);
	/* set ACC mode  enable NVREF */
	ana_set_reg(MT6397_AUDNVREGGLB_CFG0, 0x000C, 0xffff);
	/* enable LDO ; fix me , seperate for UL  DL LDO */
	ana_set_reg(MT6397_AUD_NCP0, 0xE000, 0xE000);
	/* RG DEV ck on */
	ana_set_reg(MT6397_NCP_CLKDIV_CON0, 0x102B, 0xffff);
	ana_set_reg(MT6397_NCP_CLKDIV_CON1, 0x0000, 0xffff);	/* NCP on */
	usleep_range(200, 210);
	/* ZCD setting gain step gain and enable */
	ana_set_reg(MT6397_ZCD_CON0, 0x0301, 0xffff);
	/* audio bias adjustment */
	ana_set_reg(MT6397_IBIASDIST_CFG0, 0x0552, 0xffff);
	/* set DUDIV gain ,iv buffer gain */
	ana_set_reg(MT6397_ZCD_CON4, 0x0505, 0xffff);
	/* set IV buffer on */
	ana_set_reg(MT6397_AUD_IV_CFG0, 0x1111, 0xffff);
	usleep_range(100, 110);
	/* reset docoder */
	ana_set_reg(MT6397_AUDCLKGEN_CFG0, 0x0001, 0x0001);
	/* power on DAC */
	ana_set_reg(MT6397_AUDDAC_CON0, 0x000f, 0xffff);
	usleep_range(100, 110);
	set_mux(MTCODEC_DEVICE_OUT_SPKR, MTCODEC_MUX_AUDIO);
	set_mux(MTCODEC_DEVICE_OUT_SPKL, MTCODEC_MUX_AUDIO);
	ana_set_reg(MT6397_AUDBUF_CFG0, 0x0000, 0x0007);	/* set Mux */
	ana_set_reg(AFUNC_AUD_CON2, 0x0000, 0x0080);

	/* disable the software register mode */
	ana_set_reg(MT6397_SPK_CON1, 0, 0x7f00);
	/* disable the software register mode */
	ana_set_reg(MT6397_SPK_CON4, 0, 0x7f00);
	/* Choose new mode for trim (E2 Trim) */
	ana_set_reg(MT6397_SPK_CON9, 0x0018, 0xffff);
	ana_set_reg(MT6397_SPK_CON0, 0x0008, 0xffff);	/* Enable auto trim */
	ana_set_reg(MT6397_SPK_CON3, 0x0008, 0xffff);	/* Enable auto trim R */
	ana_set_reg(MT6397_SPK_CON0, 0x3000, 0xf000);	/* set gain */
	ana_set_reg(MT6397_SPK_CON3, 0x3000, 0xf000);	/* set gain R */
	ana_set_reg(MT6397_SPK_CON9, 0x0100, 0x0f00);	/* set gain L */
	ana_set_reg(MT6397_SPK_CON5, (0x1 << 11), 0x7800);	/* set gain R */
	/* Enable amplifier & auto trim */
	ana_set_reg(MT6397_SPK_CON0, 0x0001, 0x0001);
	/* Enable amplifier & auto trim R */
	ana_set_reg(MT6397_SPK_CON3, 0x0001, 0x0001);

	/* empirical data shows it usually takes 13ms to be ready */
	usleep_range(14000, 15000);

	do {
		wait_for_ready = ana_get_reg(MT6397_SPK_CON1);
		wait_for_ready = ((wait_for_ready & 0x8000) >> 15);

		if (wait_for_ready) {
			wait_for_ready = ana_get_reg(MT6397_SPK_CON4);
			wait_for_ready = ((wait_for_ready & 0x8000) >> 15);
			if (wait_for_ready)
				break;
		}

		pr_debug("spk_auto_trim_offset sleep\n");
		usleep_range(100, 110);
	} while (retyrcount--);

	if (likely(wait_for_ready))
		pr_debug("spk_auto_trim_offset done\n");
	else
		pr_warn("spk_auto_trim_offset fail\n");

	ana_set_reg(MT6397_SPK_CON9, 0x0, 0xffff);
	ana_set_reg(MT6397_SPK_CON5, 0, 0x7800);	/* set gain R */
	ana_set_reg(MT6397_SPK_CON0, 0x0000, 0x0001);
	ana_set_reg(MT6397_SPK_CON3, 0x0000, 0x0001);

	/* get trim offset result */
	pr_debug("GetSPKAutoTrimOffset ");
	ana_set_reg(MT6397_TEST_CON0, 0x0805, 0xffff);
	reg1 = ana_get_reg(MT6397_TEST_OUT_L);
	codec_data->spk_l_trim = ((reg1 >> 0) & 0xf);
	ana_set_reg(MT6397_TEST_CON0, 0x0806, 0xffff);
	reg1 = ana_get_reg(MT6397_TEST_OUT_L);
	codec_data->spk_l_trim |= 0xF;
	codec_data->spk_l_polarity = ((reg1 >> 1) & 0x1);
	ana_set_reg(MT6397_TEST_CON0, 0x080E, 0xffff);
	reg1 = ana_get_reg(MT6397_TEST_OUT_L);
	codec_data->spk_r_trim = ((reg1 >> 0) & 0xf);
	ana_set_reg(MT6397_TEST_CON0, 0x080F, 0xffff);
	reg1 = ana_get_reg(MT6397_TEST_OUT_L);
	codec_data->spk_r_trim |= (((reg1 >> 0) & 0x1) << 4);
	codec_data->spk_r_polarity = ((reg1 >> 1) & 0x1);

	chip_version = ana_get_reg(MT6397_CID);
	if (chip_version == 0x1097 /*upstream todo remove hard code*/) {
		pr_info("PMIC is MT6397 E1, set speaker R trim code to 0\n");
		codec_data->spk_r_trim = 0;
		codec_data->spk_r_polarity = 0;
	}

	pr_debug("mSPKlpolarity = %d mISPKltrim = 0x%x\n",
		 codec_data->spk_l_polarity, codec_data->spk_l_trim);
	pr_debug("mSPKrpolarity = %d mISPKrtrim = 0x%x\n",
		 codec_data->spk_r_polarity, codec_data->spk_r_trim);

	/* turn off speaker after trim */
	ana_set_reg(AFUNC_AUD_CON2, 0x0080, 0x0080);
	ana_set_reg(MT6397_SPK_CON0, 0x0000, 0xffff);
	ana_set_reg(MT6397_SPK_CON3, 0x0000, 0xffff);
	ana_set_reg(MT6397_SPK_CON11, 0x0000, 0xffff);

	/* enable LDO ; fix me , seperate for UL  DL LDO */
	ana_set_reg(MT6397_AUDCLKGEN_CFG0, 0x0000, 0x0001);
	/* RG DEV ck on */
	ana_set_reg(MT6397_AUDDAC_CON0, 0x0000, 0xffff);
	ana_set_reg(MT6397_AUD_IV_CFG0, 0x0000, 0xffff);	/* NCP on */
	/* Audio headset power on */
	ana_set_reg(MT6397_IBIASDIST_CFG0, 0x1552, 0xffff);

	ana_set_reg(MT6397_AUDNVREGGLB_CFG0, 0x0006, 0xffff);
	ana_set_reg(MT6397_NCP_CLKDIV_CON1, 0x0001, 0xffff);	/* fix me */
	ana_set_reg(MT6397_AUD_NCP0, 0x0000, 0x6000);
	ana_set_reg(MT6397_AUDLDO_CFG0, 0x0192, 0xffff);
	ana_set_reg(AFUNC_AUD_CON2, 0x0000, 0x0080);
}

static void get_trim_offset(void)
{
	uint32_t reg1 = 0, reg2 = 0;
	bool trim_enable = 0;

	/* get to check if trim happen */
	reg1 = ana_get_reg(PMIC_TRIM_ADDRESS1);
	reg2 = ana_get_reg(PMIC_TRIM_ADDRESS2);
	pr_debug("AudCodec reg1 = 0x%x reg2 = 0x%x\n", reg1, reg2);

	trim_enable = (reg1 >> 11) & 1;
	if (trim_enable == 0) {
		codec_data->hp_l_trim = 2;
		codec_data->hp_l_fine_trim = 0;
		codec_data->hp_r_trim = 2;
		codec_data->hp_r_fine_trim = 0;
		codec_data->iv_hp_l_trim = 3;
		codec_data->iv_hp_l_fine_trim = 0;
		codec_data->iv_hp_r_trim = 3;
		codec_data->iv_hp_r_fine_trim = 0;
	} else {
		codec_data->hp_l_trim = ((reg1 >> 3) & 0xf);
		codec_data->hp_r_trim = ((reg1 >> 7) & 0xf);
		codec_data->hp_l_fine_trim = ((reg1 >> 12) & 0x3);
		codec_data->hp_r_fine_trim = ((reg1 >> 14) & 0x3);
		codec_data->iv_hp_l_trim = ((reg2 >> 0) & 0xf);
		codec_data->iv_hp_r_trim = ((reg2 >> 4) & 0xf);
		codec_data->iv_hp_l_fine_trim = ((reg2 >> 8) & 0x3);
		codec_data->iv_hp_r_fine_trim = ((reg2 >> 10) & 0x3);
	}

	pr_debug("AudCodec %s trim_enable = %d reg1 = 0x%x reg2 = 0x%x\n",
		 __func__, trim_enable, reg1, reg2);
	pr_debug("AudCodec %s mHPLtrim = 0x%x mHPLfinetrim = 0x%x mHPRtrim = 0x%x mHPRfinetrim = 0x%x\n",
		 __func__, codec_data->hp_l_trim, codec_data->hp_l_fine_trim,
		 codec_data->hp_r_trim, codec_data->hp_r_fine_trim);
	pr_debug("AudCodec %s mIVHPLtrim = 0x%x mIVHPLfinetrim = 0x%x mIVHPRtrim = 0x%x mIVHPRfinetrim = 0x%x\n",
		 __func__, codec_data->iv_hp_l_trim,
		 codec_data->iv_hp_l_fine_trim,
		 codec_data->iv_hp_r_trim, codec_data->iv_hp_r_fine_trim);
}

static void set_hp_trim_offset(void)
{
	uint32_t AUDBUG_reg = 0;

	pr_debug("AudCodec set_hp_trim_offset");
	AUDBUG_reg |= 1 << 8;	/* enable trim function */
	AUDBUG_reg |= codec_data->hp_r_fine_trim << 11;
	AUDBUG_reg |= codec_data->hp_l_fine_trim << 9;
	AUDBUG_reg |= codec_data->hp_r_trim << 4;
	AUDBUG_reg |= codec_data->hp_l_trim;
	ana_set_reg(MT6397_AUDBUF_CFG3, AUDBUG_reg, 0x1fff);
}

static void set_spk_trim_offset(void)
{
	uint32_t AUDBUG_reg = 0;

	AUDBUG_reg |= 1 << 14;	/* enable trim function */
	AUDBUG_reg |= codec_data->spk_l_polarity << 13;	/* polarity */
	AUDBUG_reg |= codec_data->spk_l_trim << 8;	/* polarity */
	pr_debug("SetSPKlTrimOffset AUDBUG_reg = 0x%x\n", AUDBUG_reg);
	ana_set_reg(MT6397_SPK_CON1, AUDBUG_reg, 0x7f00);
	AUDBUG_reg = 0;
	AUDBUG_reg |= 1 << 14;	/* enable trim function */
	AUDBUG_reg |= codec_data->spk_r_polarity << 13;	/* polarity */
	AUDBUG_reg |= codec_data->spk_r_trim << 8;	/* polarity */
	pr_debug("SetSPKrTrimOffset AUDBUG_reg = 0x%x\n", AUDBUG_reg);
	ana_set_reg(MT6397_SPK_CON4, AUDBUG_reg, 0x7f00);
}

static void set_iv_hp_trim_offset(void)
{
	uint32_t AUDBUG_reg = 0;

	AUDBUG_reg |= 1 << 8;	/* enable trim function */

	if ((codec_data->hp_r_fine_trim == 0) ||
	    (codec_data->hp_l_fine_trim == 0))
		codec_data->iv_hp_r_fine_trim = 0;
	else
		codec_data->iv_hp_r_fine_trim = 2;

	AUDBUG_reg |= codec_data->iv_hp_r_fine_trim << 11;

	if ((codec_data->hp_r_fine_trim == 0) ||
	    (codec_data->hp_l_fine_trim == 0))
		codec_data->iv_hp_l_fine_trim = 0;
	else
		codec_data->iv_hp_l_fine_trim = 2;

	AUDBUG_reg |= codec_data->iv_hp_l_fine_trim << 9;

	AUDBUG_reg |= codec_data->iv_hp_r_trim << 4;
	AUDBUG_reg |= codec_data->iv_hp_l_trim;
	ana_set_reg(MT6397_AUDBUF_CFG3, AUDBUG_reg, 0x1fff);
}

static void audio_amp_change(int channels, bool enable)
{
	if (enable) {
		pr_debug("AudCodec turn on audio_amp_change\n");
		if (!get_dl_status())
			turn_on_dac_power();

		if (!codec_data->device_power[MTCODEC_VOL_HPOUTL] &&
		    !codec_data->device_power[MTCODEC_VOL_HPOUTR]) {
			ana_set_reg(AFUNC_AUD_CON2, 0x0080, 0x0080);
			set_hp_trim_offset();
			/* enable VA28 , VA 33 VBAT ref , set dc */
			ana_set_reg(MT6397_AUDLDO_CFG0, 0x0D92, 0xffff);
			/* set ACC mode      enable NVREF */
			ana_set_reg(MT6397_AUDNVREGGLB_CFG0, 0x000C, 0xffff);
			/* enable LDO ; fix me , seperate for UL  DL LDO */
			ana_set_reg(MT6397_AUD_NCP0, 0xE000, 0xE000);
			/* RG DEV ck on */
			ana_set_reg(MT6397_NCP_CLKDIV_CON0, 0x102b, 0xffff);
			/* NCP on */
			ana_set_reg(MT6397_NCP_CLKDIV_CON1, 0x0000, 0xffff);
			/* msleep(1);// temp remove */
			usleep_range(200, 210);

			ana_set_reg(MT6397_ZCD_CON0, 0x0101, 0xffff);
			ana_set_reg(MT6397_AUDACCDEPOP_CFG0, 0x0030, 0xffff);
			ana_set_reg(MT6397_AUDBUF_CFG0, 0x0008, 0xffff);
			ana_set_reg(MT6397_IBIASDIST_CFG0, 0x0552, 0xffff);
			ana_set_reg(MT6397_ZCD_CON2, 0x0c0c, 0xffff);
			ana_set_reg(MT6397_ZCD_CON3, 0x000F, 0xffff);
			ana_set_reg(MT6397_AUDBUF_CFG1, 0x0900, 0xffff);
			ana_set_reg(MT6397_AUDBUF_CFG2, 0x0082, 0xffff);
			/* msleep(1);// temp remove */

			ana_set_reg(MT6397_AUDBUF_CFG0, 0x0009, 0xffff);
			/* msleep(30); //temp */

			ana_set_reg(MT6397_AUDBUF_CFG1, 0x0940, 0xffff);
			usleep_range(200, 210);
			ana_set_reg(MT6397_AUDBUF_CFG0, 0x000F, 0xffff);
			/* msleep(1);// temp remove */

			ana_set_reg(MT6397_AUDBUF_CFG1, 0x0100, 0xffff);
			usleep_range(100, 110);
			ana_set_reg(MT6397_AUDBUF_CFG2, 0x0022, 0xffff);
			ana_set_reg(MT6397_ZCD_CON2, 0x00c0c, 0xffff);
			usleep_range(100, 110);
			/* msleep(1);// temp remove */

			ana_set_reg(MT6397_AUDCLKGEN_CFG0, 0x0001, 0x0001);
			ana_set_reg(MT6397_AUDDAC_CON0, 0x000F, 0xffff);
			usleep_range(100, 110);
			/* msleep(1);// temp remove */
			ana_set_reg(MT6397_AUDBUF_CFG0, 0x0006, 0x0007);
			set_mux(MTCODEC_DEVICE_OUT_HSR, MTCODEC_MUX_AUDIO);
			set_mux(MTCODEC_DEVICE_OUT_HSL, MTCODEC_MUX_AUDIO);

			ana_set_reg(AFUNC_AUD_CON2, 0x0000, 0x0080);
		}
		pr_debug("AudCodec turn on audio_amp_change done\n");
	} else {
		pr_debug("AudCodec turn off audio_amp_change\n");

		if (!codec_data->device_power[MTCODEC_VOL_HPOUTL] &&
		    !codec_data->device_power[MTCODEC_VOL_HPOUTR]) {
			ana_set_reg(AFUNC_AUD_CON2, 0x0080, 0x0080);
			ana_set_reg(MT6397_ZCD_CON2, 0x0c0c, 0xffff);
			ana_set_reg(MT6397_AUDBUF_CFG0, 0x0000, 0x1fe7);
			/* RG DEV ck off; */
			ana_set_reg(MT6397_IBIASDIST_CFG0, 0x1552, 0xffff);
			/* NCP off */
			ana_set_reg(MT6397_AUDDAC_CON0, 0x0000, 0xffff);
			ana_set_reg(MT6397_AUDCLKGEN_CFG0, 0x0000, 0x0001);

			if (get_uplink_status() == false)
				/* need check */
				ana_set_reg(MT6397_AUDNVREGGLB_CFG0,
					0x0006, 0xffff);

			 /* fix me */
			ana_set_reg(MT6397_NCP_CLKDIV_CON1, 0x0001, 0xffff);
			ana_set_reg(MT6397_AUD_NCP0, 0x0000, 0x6000);

			if (get_uplink_status() == false)
				ana_set_reg(MT6397_AUDLDO_CFG0, 0x0192, 0xffff);

			ana_set_reg(AFUNC_AUD_CON2, 0x0000, 0x0080);
			if (get_dl_status() == false)
				turn_off_dac_power();
		}
		pr_debug("AudCodec turn off audio_amp_change done\n");
	}
}

static int audio_ampl_get(struct snd_kcontrol *kcontrol,
			  struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("AudCodec %s() = %d\n", __func__,
		 codec_data->device_power[MTCODEC_VOL_HPOUTL]);
	ucontrol->value.integer.value[0] =
		codec_data->device_power[MTCODEC_VOL_HPOUTL];
	return 0;
}

static int audio_ampl_set(struct snd_kcontrol *kcontrol,
			  struct snd_ctl_elem_value *ucontrol)
{
	/* struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol); */
	mutex_lock(&ana_ctrl_mutex);

	pr_debug("AudCodec %s() gain = %d\n ", __func__,
		 (int)ucontrol->value.integer.value[0]);
	if (ucontrol->value.integer.value[0]) {
		aud_drv_ana_clk_on();
		audio_amp_change(MTCODEC_CHANNELS_LEFT1, true);
		codec_data->device_power[MTCODEC_VOL_HPOUTL] =
			ucontrol->value.integer.value[0];
	} else {
		codec_data->device_power[MTCODEC_VOL_HPOUTL] =
			ucontrol->value.integer.value[0];
		audio_amp_change(MTCODEC_CHANNELS_LEFT1, false);
		aud_drv_ana_clk_off();
	}
	mutex_unlock(&ana_ctrl_mutex);
	return 0;
}

static void speaker_amp_change(bool enable)
{
	if (enable) {
		pr_debug("AudCodec turn on speaker_amp_change\n");
		if (get_dl_status() == false)
			turn_on_dac_power();

		/* enable SPK related CLK //Luke */
		ana_enable_clk(0x0604, true);
		ana_set_reg(AFUNC_AUD_CON2, 0x0080, 0x0080);
		set_spk_trim_offset();
		/* enable VA28 , VA 33 VBAT ref , set dc */
		ana_set_reg(MT6397_AUDLDO_CFG0, 0x0D92, 0xffff);
		/* set ACC mode  enable NVREF */
		ana_set_reg(MT6397_AUDNVREGGLB_CFG0, 0x000C, 0xffff);
		/* enable LDO ; fix me , seperate for UL  DL LDO */
		ana_set_reg(MT6397_AUD_NCP0, 0xE000, 0xE000);
		/* RG DEV ck on */
		ana_set_reg(MT6397_NCP_CLKDIV_CON0, 0x102B, 0xffff);
		/* NCP on */
		ana_set_reg(MT6397_NCP_CLKDIV_CON1, 0x0000, 0xffff);
		usleep_range(200, 210);
		/* ZCD setting gain step gain and enable */
		ana_set_reg(MT6397_ZCD_CON0, 0x0301, 0xffff);
		/* audio bias adjustment */
		ana_set_reg(MT6397_IBIASDIST_CFG0, 0x0552, 0xffff);
		/* set DUDIV gain ,iv buffer gain */
		ana_set_reg(MT6397_ZCD_CON4, 0x0505, 0xffff);
		/* set IV buffer on */
		ana_set_reg(MT6397_AUD_IV_CFG0, 0x1111, 0xffff);
		usleep_range(100, 110);
		/* reset docoder */
		ana_set_reg(MT6397_AUDCLKGEN_CFG0, 0x0001, 0x0001);
		/* power on DAC */
		ana_set_reg(MT6397_AUDDAC_CON0, 0x000f, 0xffff);
		usleep_range(100, 110);
		set_mux(MTCODEC_DEVICE_OUT_SPKR, MTCODEC_MUX_AUDIO);
		set_mux(MTCODEC_DEVICE_OUT_SPKL, MTCODEC_MUX_AUDIO);
		/* set Mux */
		ana_set_reg(MT6397_AUDBUF_CFG0, 0x0000, 0x0007);
		ana_set_reg(AFUNC_AUD_CON2, 0x0000, 0x0080);
		/* set gain L */
		ana_set_reg(MT6397_SPK_CON9, 0x0100, 0x0f00);
		/* set gain R */
		ana_set_reg(MT6397_SPK_CON5, (0x1 << 11), 0x7800);

		/*Class D amp*/
		if (codec_data->spk_channel_sel == 0) {
			/* Stereo */
			ana_set_reg(MT6397_SPK_CON0, 0x3009, 0xffff);
			ana_set_reg(MT6397_SPK_CON3, 0x3009, 0xffff);
			ana_set_reg(MT6397_SPK_CON2, 0x0014, 0xffff);
			ana_set_reg(MT6397_SPK_CON5, 0x0014, 0x07ff);
		} else if (codec_data->spk_channel_sel == 1) {
			/* MonoLeft */
			ana_set_reg(MT6397_SPK_CON0, 0x3009, 0xffff);
			ana_set_reg(MT6397_SPK_CON2, 0x0014, 0xffff);
		} else if (codec_data->spk_channel_sel == 2) {
			/* MonoRight */
			ana_set_reg(MT6397_SPK_CON3, 0x3009, 0xffff);
			ana_set_reg(MT6397_SPK_CON5, 0x0014, 0x07ff);
		} else {
			/*AUDIO_ASSERT(true);*/
		}

		if (codec_data->spk_channel_sel == 0) {
			/* Stereo SPK gain setting*/
			ana_set_reg(MT6397_SPK_CON9, 0x0800, 0xffff);
			ana_set_reg(MT6397_SPK_CON5, (0x8 << 11), 0x7800);
		} else if (codec_data->spk_channel_sel == 1) {
			/* MonoLeft SPK gain setting*/
			ana_set_reg(MT6397_SPK_CON9, 0x0800, 0xffff);
		} else if (codec_data->spk_channel_sel == 2) {
			/* MonoRight SPK gain setting*/
			ana_set_reg(MT6397_SPK_CON5, (0x8 << 11), 0x7800);
		} else {
			/*AUDIO_ASSERT(true);*/
		}
		/* spk output stage enabke and enable */
		ana_set_reg(MT6397_SPK_CON11, 0x0f00, 0xffff);
		usleep_range(4000, 5000);
		pr_debug("AudCodec turn on speaker_amp_change done\n");

	} else {
		pr_debug("AudCodec turn off speaker_amp_change\n");
		ana_set_reg(AFUNC_AUD_CON2, 0x0080, 0x0080);
		ana_set_reg(MT6397_SPK_CON0, 0x0000, 0xffff);
		ana_set_reg(MT6397_SPK_CON3, 0x0000, 0xffff);
		ana_set_reg(MT6397_SPK_CON11, 0x0000, 0xffff);
		/* enable LDO ; fix me , seperate for UL  DL LDO */
		ana_set_reg(MT6397_AUDCLKGEN_CFG0, 0x0000, 0x0001);
		/* RG DEV ck on */
		ana_set_reg(MT6397_AUDDAC_CON0, 0x0000, 0xffff);
		/* NCP on */
		ana_set_reg(MT6397_AUD_IV_CFG0, 0x0000, 0xffff);
		/* Audio headset power on */
		ana_set_reg(MT6397_IBIASDIST_CFG0, 0x1552, 0xffff);
		/* ana_set_reg(AUDBUF_CFG1, 0x0000, 0x0100); */
		if (get_uplink_status() == false)
			ana_set_reg(MT6397_AUDNVREGGLB_CFG0, 0x0006, 0xffff);

		/* fix me */
		ana_set_reg(MT6397_NCP_CLKDIV_CON1, 0x0001, 0xffff);
		ana_set_reg(MT6397_AUD_NCP0, 0x0000, 0x6000);
		if (get_uplink_status() == false)
			ana_set_reg(MT6397_AUDLDO_CFG0, 0x0192, 0xffff);

		ana_set_reg(AFUNC_AUD_CON2, 0x0000, 0x0080);
		/* disable SPK related CLK        //Luke */
		ana_enable_clk(0x0604, false);
		if (get_dl_status() == false)
			turn_off_dac_power();

		/* temp solution, set ZCD_CON0 to 0x101 for pop noise */
		ana_set_reg(MT6397_ZCD_CON0, 0x0101, 0xffff);
		pr_debug("AudCodec turn off speaker_amp_change done\n");
	}
}

static int speaker_amp_get(struct snd_kcontrol *kcontrol,
			   struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("AudCodec %s()=%d\n", __func__,
		 codec_data->device_power[MTCODEC_VOL_SPKL]);
	ucontrol->value.integer.value[0] =
		codec_data->device_power[MTCODEC_VOL_SPKL];
	return 0;
}

static int speaker_amp_set(struct snd_kcontrol *kcontrol,
			   struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("AudCodec %s() gain = %d\n ", __func__,
		 (int)ucontrol->value.integer.value[0]);
	if (ucontrol->value.integer.value[0]) {
		aud_drv_ana_clk_on();
		speaker_amp_change(true);
		codec_data->device_power[MTCODEC_VOL_SPKL] =
			ucontrol->value.integer.value[0];
	} else {
		codec_data->device_power[MTCODEC_VOL_SPKL] =
			ucontrol->value.integer.value[0];
		speaker_amp_change(false);
		aud_drv_ana_clk_off();
	}
	return 0;
}

static void headset_speaker_amp_change(bool enable)
{
	if (enable) {
		pr_debug("AudCodec turn on headset_speaker_amp_change\n");
		if (!get_dl_status())
			turn_on_dac_power();

		/* enable SPK related CLK        //Luke */
		ana_enable_clk(0x0604, true);
		ana_set_reg(AFUNC_AUD_CON2, 0x0080, 0x0080);
		set_hp_trim_offset();
		set_iv_hp_trim_offset();
		set_spk_trim_offset();

		/* enable VA28 , VA 33 VBAT ref , set dc */
		ana_set_reg(MT6397_AUDLDO_CFG0, 0x0D92, 0xffff);
		/* set ACC mode  enable NVREF */
		ana_set_reg(MT6397_AUDNVREGGLB_CFG0, 0x000C, 0xffff);
		/* enable LDO ; fix me , seperate for UL  DL LDO */
		ana_set_reg(MT6397_AUD_NCP0, 0xE000, 0xE000);
		/* RG DEV ck on */
		ana_set_reg(MT6397_NCP_CLKDIV_CON0, 0x102B, 0xffff);
		/* NCP on */
		ana_set_reg(MT6397_NCP_CLKDIV_CON1, 0x0000, 0xffff);
		usleep_range(200, 210);

		/* ZCD setting gain step gain and enable */
		ana_set_reg(MT6397_ZCD_CON0, 0x0301, 0xffff);
		/* select charge current ; fix me */
		ana_set_reg(MT6397_AUDACCDEPOP_CFG0, 0x0030, 0xffff);
		/* set voice playback with headset */
		ana_set_reg(MT6397_AUDBUF_CFG0, 0x0008, 0xffff);
		/* audio bias adjustment */
		ana_set_reg(MT6397_IBIASDIST_CFG0, 0x0552, 0xffff);
		/* HP PGA gain */
		ana_set_reg(MT6397_ZCD_CON2, 0x0C0C, 0xffff);
		/* HP PGA gain */
		ana_set_reg(MT6397_ZCD_CON3, 0x000F, 0xffff);
		/* HP enhance */
		ana_set_reg(MT6397_AUDBUF_CFG1, 0x0900, 0xffff);
		/* HS enahnce */
		ana_set_reg(MT6397_AUDBUF_CFG2, 0x0082, 0xffff);
		ana_set_reg(MT6397_AUDBUF_CFG0, 0x0009, 0xffff);
		/* HP vcm short */
		ana_set_reg(MT6397_AUDBUF_CFG1, 0x0940, 0xffff);
		usleep_range(200, 210);
		/* HP power on */
		ana_set_reg(MT6397_AUDBUF_CFG0, 0x000F, 0xffff);
		/* HP vcm not short */
		ana_set_reg(MT6397_AUDBUF_CFG1, 0x0100, 0xffff);
		usleep_range(100, 110);
		/* HS VCM not short */
		ana_set_reg(MT6397_AUDBUF_CFG2, 0x0022, 0xffff);

		/* HP PGA gain */
		ana_set_reg(MT6397_ZCD_CON2, 0x0808, 0xffff);
		usleep_range(100, 110);
		/* HP PGA gain */
		ana_set_reg(MT6397_ZCD_CON4, 0x0505, 0xffff);

		/* set IV buffer on */
		ana_set_reg(MT6397_AUD_IV_CFG0, 0x1111, 0xffff);
		usleep_range(100, 110);
		/* reset docoder */
		ana_set_reg(MT6397_AUDCLKGEN_CFG0, 0x0001, 0x0001);
		/* power on DAC */
		ana_set_reg(MT6397_AUDDAC_CON0, 0x000F, 0xffff);
		usleep_range(100, 110);
		set_mux(MTCODEC_DEVICE_OUT_SPKR, MTCODEC_MUX_AUDIO);
		set_mux(MTCODEC_DEVICE_OUT_SPKL, MTCODEC_MUX_AUDIO);
		/* set headhpone mux */
		ana_set_reg(MT6397_AUDBUF_CFG0, 0x1106, 0x1106);

		ana_set_reg(MT6397_SPK_CON9, 0x0100, 0x0f00);	/* set gain L */
		/* set gain R */
		ana_set_reg(MT6397_SPK_CON5, (0x1 << 11), 0x7800);

		/*speaker gain setting, trim enable, spk enable, class D*/
		ana_set_reg(MT6397_SPK_CON0, 0x3009, 0xffff);
		ana_set_reg(MT6397_SPK_CON3, 0x3009, 0xffff);
		ana_set_reg(MT6397_SPK_CON2, 0x0014, 0xffff);
		ana_set_reg(MT6397_SPK_CON5, 0x0014, 0x07ff);

		/* SPK gain setting */
		ana_set_reg(MT6397_SPK_CON9, 0x0400, 0xffff);
		/* SPK-R gain setting */
		ana_set_reg(MT6397_SPK_CON5, (0x4 << 11), 0x7800);
		/* spk output stage enabke and enableAudioClockPortDST */
		ana_set_reg(MT6397_SPK_CON11, 0x0f00, 0xffff);
		ana_set_reg(AFUNC_AUD_CON2, 0x0000, 0x0080);
		usleep_range(4000, 5000);

		pr_debug("AudCodec turn on headset_speaker_amp_change done\n");

	} else {
		pr_debug("AudCodec turn off headset_speaker_amp_change\n");
		ana_set_reg(AFUNC_AUD_CON2, 0x0080, 0x0080);
		ana_set_reg(MT6397_SPK_CON0, 0x0000, 0xffff);
		ana_set_reg(MT6397_SPK_CON3, 0x0000, 0xffff);
		ana_set_reg(MT6397_SPK_CON11, 0x0000, 0xffff);
		ana_set_reg(MT6397_ZCD_CON2, 0x0C0C, 0x0f0f);

		ana_set_reg(MT6397_AUDBUF_CFG0, 0x0000, 0x0007);
		ana_set_reg(MT6397_AUDBUF_CFG0, 0x0000, 0x1fe0);
		ana_set_reg(MT6397_IBIASDIST_CFG0, 0x1552, 0xffff);
		ana_set_reg(MT6397_AUDDAC_CON0, 0x0000, 0xffff);
		ana_set_reg(MT6397_AUDCLKGEN_CFG0, 0x0000, 0x0001);
		ana_set_reg(MT6397_AUD_IV_CFG0, 0x0010, 0xffff);
		ana_set_reg(MT6397_AUDBUF_CFG1, 0x0000, 0x0100);
		ana_set_reg(MT6397_AUDBUF_CFG2, 0x0000, 0x0080);

		if (!get_uplink_status())
			ana_set_reg(MT6397_AUDNVREGGLB_CFG0, 0x0006, 0xffff);

		ana_set_reg(MT6397_NCP_CLKDIV_CON1, 0x0001, 0xffff);
		ana_set_reg(MT6397_AUD_NCP0, 0x0000, 0x6000);
		if (!get_uplink_status())
			ana_set_reg(MT6397_AUDLDO_CFG0, 0x0192, 0xffff);

		ana_set_reg(AFUNC_AUD_CON2, 0x0000, 0x0080);
		/* disable SPK related CLK   //Luke */
		ana_enable_clk(0x0604, false);
		if (!get_dl_status())
			turn_off_dac_power();
		pr_debug("AudCodec turn off headset_speaker_amp_change done\n");
		/* ZCD setting gain step gain and enable */
		ana_set_reg(MT6397_ZCD_CON0, 0x0101, 0xffff);
	}
}

static int headset_speaker_amp_get(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("AudCodec %s()=%d\n", __func__,
		 codec_data->device_power[MTCODEC_VOL_SPK_HS_L]);
	ucontrol->value.integer.value[0] =
		codec_data->device_power[MTCODEC_VOL_SPK_HS_L];
	return 0;
}

static int headset_speaker_amp_set(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("AudCodec %s() gain = %d\n ", __func__,
		 (int)ucontrol->value.integer.value[0]);
	if (ucontrol->value.integer.value[0]) {
		aud_drv_ana_clk_on();
		headset_speaker_amp_change(true);
		codec_data->device_power[MTCODEC_VOL_SPK_HS_L] =
			ucontrol->value.integer.value[0];
	} else {
		codec_data->device_power[MTCODEC_VOL_SPK_HS_L] =
			ucontrol->value.integer.value[0];
		headset_speaker_amp_change(false);
		aud_drv_ana_clk_off();
	}
	return 0;
}

static const char *const amp_function[] = { "Off", "On" };

static const char *const dac_dl_pga_headset_gain[] = {
	"8Db", "7Db", "6Db", "5Db", "4Db", "3Db", "2Db", "1Db", "0Db", "-1Db",
	"-2Db", "-3Db", "-4Db", "RES1", "RES2", "-40Db"
};

static const char *const dac_dl_pga_handset_gain[] = {
	"-21Db", "-19Db", "-17Db", "-15Db", "-13Db", "-11Db", "-9Db", "-7Db",
	"-5Db", "-3Db", "-1Db", "1Db", "3Db", "5Db", "7Db", "9Db"
};				/* todo: 6397's setting */

static const char *const dac_dl_pga_speaker_gain[] = {
	"Mute", "0Db", "4Db", "5Db", "6Db", "7Db", "8Db", "9Db", "10Db",
	"11Db", "12Db", "13Db", "14Db", "15Db", "16Db", "17Db"
};

static const char *const voice_mux_function[] = { "Voice", "Speaker" };
static const char *const speaker_selection_function[] = { "Stereo",
						"MonoLeft", "MonoRight" };

static int speaker_pga_l_get(struct snd_kcontrol *kcontrol,
			     struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("AudCodec %s = %d\n", __func__,
		 codec_data->volume[MTCODEC_VOL_SPKL]);
	ucontrol->value.integer.value[0] =
		codec_data->volume[MTCODEC_VOL_SPKL];
	return 0;
}

static int speaker_pga_l_set(struct snd_kcontrol *kcontrol,
			     struct snd_ctl_elem_value *ucontrol)
{
	int index = 0;

	pr_debug("AudCodec %s()\n", __func__);

	if (ucontrol->value.enumerated.item[0] >
	    ARRAY_SIZE(dac_dl_pga_speaker_gain)) {
		pr_err("AudCodec return -EINVAL\n");
		return -EINVAL;
	}

	index = ucontrol->value.integer.value[0];
	ana_set_reg(MT6397_SPK_CON9, index << 8, 0x00000f00);
	codec_data->volume[MTCODEC_VOL_SPKL] =
		ucontrol->value.integer.value[0];
	return 0;
}

static int speaker_pga_r_get(struct snd_kcontrol *kcontrol,
			     struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("AudCodec %s = %d\n", __func__,
		 codec_data->volume[MTCODEC_VOL_SPKR]);
	ucontrol->value.integer.value[0] =
		codec_data->volume[MTCODEC_VOL_SPKR];
	return 0;
}

static int speaker_pga_r_set(struct snd_kcontrol *kcontrol,
			     struct snd_ctl_elem_value *ucontrol)
{
	int index = 0;

	pr_debug("AudCodec %s()\n", __func__);

	if (ucontrol->value.enumerated.item[0] >
	    ARRAY_SIZE(dac_dl_pga_speaker_gain)) {
		pr_err("AudCodec return -EINVAL\n");
		return -EINVAL;
	}

	index = ucontrol->value.integer.value[0];
	ana_set_reg(MT6397_SPK_CON5, index << 11, 0x00007800);
	codec_data->volume[MTCODEC_VOL_SPKR] =
		ucontrol->value.integer.value[0];
	return 0;
}

static int speaker_channel_set(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("AudCodec %s()\n", __func__);
	codec_data->spk_channel_sel =
		ucontrol->value.integer.value[0];
	return 0;
}

static int speaker_channel_get(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("AudCodec %s = %d\n", __func__,
		 codec_data->spk_channel_sel);
	ucontrol->value.integer.value[0] =
		codec_data->spk_channel_sel;
	return 0;
}

static int headset_pga_l_get(struct snd_kcontrol *kcontrol,
			     struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("AudCodec %s() = %d\n", __func__,
		 codec_data->volume[MTCODEC_VOL_HPOUTL]);
	ucontrol->value.integer.value[0] =
		codec_data->volume[MTCODEC_VOL_HPOUTL];
	return 0;
}

static int headset_pga_l_set(struct snd_kcontrol *kcontrol,
			     struct snd_ctl_elem_value *ucontrol)
{
	int index = 0;

	pr_debug("AudCodec %s()\n", __func__);
	if (ucontrol->value.enumerated.item[0] >
		ARRAY_SIZE(dac_dl_pga_headset_gain)) {
		pr_err("AudCodec return -EINVAL\n");
		return -EINVAL;
	}
	index = ucontrol->value.integer.value[0];
	ana_set_reg(MT6397_ZCD_CON2, index, 0x0000000F);
	codec_data->volume[MTCODEC_VOL_HPOUTL] =
		ucontrol->value.integer.value[0];
	return 0;
}

static int headset_pga_r_get(struct snd_kcontrol *kcontrol,
			     struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("AudCodec %s() = %d\n", __func__,
		 codec_data->volume[MTCODEC_VOL_HPOUTR]);
	ucontrol->value.integer.value[0] =
		codec_data->volume[MTCODEC_VOL_HPOUTR];
	return 0;
}

static int headset_pga_r_set(struct snd_kcontrol *kcontrol,
			     struct snd_ctl_elem_value *ucontrol)
{
	int index = 0;

	pr_debug("AudCodec %s()\n", __func__);

	if (ucontrol->value.enumerated.item[0] >
			ARRAY_SIZE(dac_dl_pga_headset_gain)) {
		pr_err("AudCodec return -EINVAL\n");
		return -EINVAL;
	}
	index = ucontrol->value.integer.value[0];
	ana_set_reg(MT6397_ZCD_CON2, index << 8, 0x000000F00);
	codec_data->volume[MTCODEC_VOL_HPOUTR] =
		ucontrol->value.integer.value[0];
	return 0;
}

static const struct soc_enum audio_amp_enum[] = {
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(amp_function), amp_function),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(amp_function), amp_function),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(amp_function), amp_function),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(amp_function), amp_function),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(amp_function), amp_function),
	/* here comes pga gain setting */
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(dac_dl_pga_headset_gain),
			    dac_dl_pga_headset_gain),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(dac_dl_pga_headset_gain),
			    dac_dl_pga_headset_gain),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(dac_dl_pga_handset_gain),
			    dac_dl_pga_handset_gain),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(dac_dl_pga_speaker_gain),
			    dac_dl_pga_speaker_gain),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(dac_dl_pga_speaker_gain),
			    dac_dl_pga_speaker_gain),
	/* Mux Function */
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(voice_mux_function), voice_mux_function),
	/* Configurations */
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(speaker_selection_function),
			    speaker_selection_function),
};

static const struct snd_kcontrol_new mt6397_snd_controls[] = {
	SOC_ENUM_EXT("Audio_Amp_Switch", audio_amp_enum[1],
		     audio_ampl_get, audio_ampl_set),
	SOC_ENUM_EXT("Speaker_Amp_Switch", audio_amp_enum[3],
		     speaker_amp_get, speaker_amp_set),
	SOC_ENUM_EXT("Headset_Speaker_Amp_Switch", audio_amp_enum[4],
		     headset_speaker_amp_get,
		     headset_speaker_amp_set),

	SOC_ENUM_EXT("Headset_PGAL_GAIN", audio_amp_enum[5],
		     headset_pga_l_get, headset_pga_l_set),
	SOC_ENUM_EXT("Headset_PGAR_GAIN", audio_amp_enum[6],
		     headset_pga_r_get, headset_pga_r_set),
	SOC_ENUM_EXT("Speaker_PGAL_GAIN", audio_amp_enum[8],
		     speaker_pga_l_get, speaker_pga_l_set),
	SOC_ENUM_EXT("Speaker_PGAR_GAIN", audio_amp_enum[9],
		     speaker_pga_r_get, speaker_pga_r_set),

	SOC_ENUM_EXT("Speaker_Channel_Select", audio_amp_enum[11],
		     speaker_channel_get,
		     speaker_channel_set),

};

static void mt6397_codec_init_reg(struct snd_soc_codec *codec)
{
	pr_debug("AudCodec mt6397_codec_init_reg\n");

	/* power_init */
	ana_set_reg(MT6397_TOP_CKCON1, 0x0000, 0x0010); /*bit4: RG_CLKSQ_EN*/
	ana_set_reg(AFUNC_AUD_CON2, 0x0080, 0x0080);
	ana_set_reg(MT6397_ZCD_CON2, 0x0c0c, 0xffff);
	ana_set_reg(MT6397_AUDBUF_CFG0, 0x0000, 0x1fe7);
	/* RG DEV ck off; */
	ana_set_reg(MT6397_IBIASDIST_CFG0, 0x1552, 0xffff);
	ana_set_reg(MT6397_AUDDAC_CON0, 0x0000, 0xffff);	/* NCP off */
	ana_set_reg(MT6397_AUDCLKGEN_CFG0, 0x0000, 0x0001);
	ana_set_reg(MT6397_AUDNVREGGLB_CFG0, 0x0006, 0xffff);	/* need check */
	ana_set_reg(MT6397_NCP_CLKDIV_CON1, 0x0001, 0xffff);	/* fix me */
	ana_set_reg(MT6397_AUD_NCP0, 0x0000, 0x6000);
	ana_set_reg(MT6397_AUDLDO_CFG0, 0x0192, 0xffff);
	ana_set_reg(AFUNC_AUD_CON2, 0x0000, 0x0080);
	/* gain step gain and enable */
	ana_set_reg(MT6397_ZCD_CON0, 0x0101, 0xffff);
}

static int mt6397_codec_probe(struct snd_soc_codec *codec)
{
	pr_info("%s()\n", __func__);

	ana_set_codec(codec);

	mt6397_codec_init_reg(codec);

	/* add codec controls */
	snd_soc_add_codec_controls(codec, mt6397_snd_controls,
				   ARRAY_SIZE(mt6397_snd_controls));

	/* here to set  private data */
	codec_data = kzalloc(sizeof(*codec_data), GFP_KERNEL);

	if (unlikely(!codec_data)) {
		pr_err("Failed to allocate private data\n");
		return -ENOMEM;
	}

	snd_soc_codec_set_drvdata(codec, codec_data);


	/* TRIM FUNCTION */
	/* Get HP Trim Offset */
	get_trim_offset();
	spk_auto_trim_offset();
	return 0;
}

static int mt6397_codec_remove(struct snd_soc_codec *codec)
{
	pr_info("%s()\n", __func__);
	return 0;
}

static struct regmap *mt6397_codec_get_regmap(struct device *dev)
{
	struct mt6397_chip *mt6397;

	mt6397 = dev_get_drvdata(dev->parent);

	return mt6397->regmap;
}

static int mt_codec_debug_open(struct inode *inode, struct file *file)
{
	pr_notice("mt_soc_debug_open\n");
	return 0;
}

static ssize_t mt_codec_debug_read(struct file *file, char __user *buf,
				   size_t count, loff_t *pos)
{
	const int size = 4096;
	char buffer[size];
	int n = 0;
	/*upstream check no need enable aud clk?*/
	n += scnprintf(buffer + n, size - n,
		       "======PMIC digital registers====\n");
	n += scnprintf(buffer + n, size - n, "UL_DL_CON0 = 0x%x\n",
		       ana_get_reg(AFE_UL_DL_CON0));
	n += scnprintf(buffer + n, size - n, "DL_SRC2_CON0_H = 0x%x\n",
		       ana_get_reg(AFE_DL_SRC2_CON0_H));
	n += scnprintf(buffer + n, size - n, "DL_SRC2_CON0_L = 0x%x\n",
		       ana_get_reg(AFE_DL_SRC2_CON0_L));
	n += scnprintf(buffer + n, size - n, "DL_SDM_CON0 = 0x%x\n",
		       ana_get_reg(AFE_DL_SDM_CON0));
	n += scnprintf(buffer + n, size - n, "DL_SDM_CON1 = 0x%x\n",
		       ana_get_reg(AFE_DL_SDM_CON1));
	n += scnprintf(buffer + n, size - n, "UL_SRC_CON0_H = 0x%x\n",
		       ana_get_reg(AFE_UL_SRC_CON0_H));
	n += scnprintf(buffer + n, size - n, "UL_SRC_CON0_L = 0x%x\n",
		       ana_get_reg(AFE_UL_SRC_CON0_L));
	n += scnprintf(buffer + n, size - n, "UL_SRC_CON1_H = 0x%x\n",
		       ana_get_reg(AFE_UL_SRC_CON1_H));
	n += scnprintf(buffer + n, size - n, "UL_SRC_CON1_L = 0x%x\n",
		       ana_get_reg(AFE_UL_SRC_CON1_L));
	n += scnprintf(buffer + n, size - n, "AFE_TOP_CON0 = 0x%x\n",
		       ana_get_reg(ANA_AFE_TOP_CON0));
	n += scnprintf(buffer + n, size - n, "AFUNC_AUD_CON0 = 0x%x\n",
		       ana_get_reg(AFUNC_AUD_CON0));
	n += scnprintf(buffer + n, size - n, "AFUNC_AUD_CON1 = 0x%x\n",
		       ana_get_reg(AFUNC_AUD_CON1));
	n += scnprintf(buffer + n, size - n, "AFUNC_AUD_CON2 = 0x%x\n",
		       ana_get_reg(AFUNC_AUD_CON2));
	n += scnprintf(buffer + n, size - n, "AFUNC_AUD_CON3 = 0x%x\n",
		       ana_get_reg(AFUNC_AUD_CON3));
	n += scnprintf(buffer + n, size - n, "AFUNC_AUD_CON4 = 0x%x\n",
		       ana_get_reg(AFUNC_AUD_CON4));
	n += scnprintf(buffer + n, size - n, "AFUNC_AUD_MON0 = 0x%x\n",
		       ana_get_reg(AFUNC_AUD_MON0));
	n += scnprintf(buffer + n, size - n, "AFUNC_AUD_MON1 = 0x%x\n",
		       ana_get_reg(AFUNC_AUD_MON1));
	n += scnprintf(buffer + n, size - n, "AUDRC_TUNE_MON0 = 0x%x\n",
		       ana_get_reg(AUDRC_TUNE_MON0));
	n += scnprintf(buffer + n, size - n, "AFE_UP8X_FIFO_CFG0 = 0x%x\n",
		       ana_get_reg(AFE_UP8X_FIFO_CFG0));
	n += scnprintf(buffer + n, size - n, "AFE_UP8X_FIFO_LOG_MON0 = 0x%x\n",
		       ana_get_reg(AFE_UP8X_FIFO_LOG_MON0));
	n += scnprintf(buffer + n, size - n, "AFE_UP8X_FIFO_LOG_MON1 = 0x%x\n",
		       ana_get_reg(AFE_UP8X_FIFO_LOG_MON1));
	n += scnprintf(buffer + n, size - n, "AFE_DL_DC_COMP_CFG0 = 0x%x\n",
		       ana_get_reg(AFE_DL_DC_COMP_CFG0));
	n += scnprintf(buffer + n, size - n, "AFE_DL_DC_COMP_CFG1 = 0x%x\n",
		       ana_get_reg(AFE_DL_DC_COMP_CFG1));
	n += scnprintf(buffer + n, size - n, "AFE_DL_DC_COMP_CFG2 = 0x%x\n",
		       ana_get_reg(AFE_DL_DC_COMP_CFG2));
	n += scnprintf(buffer + n, size - n, "AFE_PMIC_NEWIF_CFG0 = 0x%x\n",
		       ana_get_reg(AFE_PMIC_NEWIF_CFG0));
	n += scnprintf(buffer + n, size - n, "AFE_PMIC_NEWIF_CFG1 = 0x%x\n",
		       ana_get_reg(AFE_PMIC_NEWIF_CFG1));
	n += scnprintf(buffer + n, size - n, "AFE_PMIC_NEWIF_CFG2 = 0x%x\n",
		       ana_get_reg(AFE_PMIC_NEWIF_CFG2));
	n += scnprintf(buffer + n, size - n, "AFE_PMIC_NEWIF_CFG3 = 0x%x\n",
		       ana_get_reg(AFE_PMIC_NEWIF_CFG3));
	n += scnprintf(buffer + n, size - n, "AFE_SGEN_CFG0 = 0x%x\n",
		       ana_get_reg(AFE_SGEN_CFG0));
	n += scnprintf(buffer + n, size - n, "AFE_SGEN_CFG1 = 0x%x\n",
		       ana_get_reg(AFE_SGEN_CFG1));
	n += scnprintf(buffer + n, size - n,
		       "======PMIC analog registers====\n");
	n += scnprintf(buffer + n, size - n, "TOP_CKPDN = 0x%x\n",
		       ana_get_reg(MT6397_TOP_CKPDN));
	n += scnprintf(buffer + n, size - n, "TOP_CKPDN2 = 0x%x\n",
		       ana_get_reg(MT6397_TOP_CKPDN2));
	n += scnprintf(buffer + n, size - n, "TOP_CKCON1 = 0x%x\n",
		       ana_get_reg(MT6397_TOP_CKCON1));
	n += scnprintf(buffer + n, size - n, "SPK_CON0 = 0x%x\n",
		       ana_get_reg(MT6397_SPK_CON0));
	n += scnprintf(buffer + n, size - n, "SPK_CON1 = 0x%x\n",
		       ana_get_reg(MT6397_SPK_CON1));
	n += scnprintf(buffer + n, size - n, "SPK_CON2 = 0x%x\n",
		       ana_get_reg(MT6397_SPK_CON2));
	n += scnprintf(buffer + n, size - n, "SPK_CON3 = 0x%x\n",
		       ana_get_reg(MT6397_SPK_CON3));
	n += scnprintf(buffer + n, size - n, "SPK_CON4 = 0x%x\n",
		       ana_get_reg(MT6397_SPK_CON4));
	n += scnprintf(buffer + n, size - n, "SPK_CON5 = 0x%x\n",
		       ana_get_reg(MT6397_SPK_CON5));
	n += scnprintf(buffer + n, size - n, "SPK_CON6 = 0x%x\n",
		       ana_get_reg(MT6397_SPK_CON6));
	n += scnprintf(buffer + n, size - n, "SPK_CON7 = 0x%x\n",
		       ana_get_reg(MT6397_SPK_CON7));
	n += scnprintf(buffer + n, size - n, "SPK_CON8 = 0x%x\n",
		       ana_get_reg(MT6397_SPK_CON8));
	n += scnprintf(buffer + n, size - n, "SPK_CON9 = 0x%x\n",
		       ana_get_reg(MT6397_SPK_CON9));
	n += scnprintf(buffer + n, size - n, "SPK_CON10 = 0x%x\n",
		       ana_get_reg(MT6397_SPK_CON10));
	n += scnprintf(buffer + n, size - n, "SPK_CON11 = 0x%x\n",
		       ana_get_reg(MT6397_SPK_CON11));
	n += scnprintf(buffer + n, size - n, "AUDDAC_CON0 = 0x%x\n",
		       ana_get_reg(MT6397_AUDDAC_CON0));
	n += scnprintf(buffer + n, size - n, "AUDBUF_CFG0 = 0x%x\n",
		       ana_get_reg(MT6397_AUDBUF_CFG0));
	n += scnprintf(buffer + n, size - n, "AUDBUF_CFG1 = 0x%x\n",
		       ana_get_reg(MT6397_AUDBUF_CFG1));
	n += scnprintf(buffer + n, size - n, "AUDBUF_CFG2 = 0x%x\n",
		       ana_get_reg(MT6397_AUDBUF_CFG2));
	n += scnprintf(buffer + n, size - n, "AUDBUF_CFG3 = 0x%x\n",
		       ana_get_reg(MT6397_AUDBUF_CFG3));
	n += scnprintf(buffer + n, size - n, "AUDBUF_CFG4 = 0x%x\n",
		       ana_get_reg(MT6397_AUDBUF_CFG4));
	n += scnprintf(buffer + n, size - n, "IBIASDIST_CFG0 = 0x%x\n",
		       ana_get_reg(MT6397_IBIASDIST_CFG0));
	n += scnprintf(buffer + n, size - n, "AUDACCDEPOP_CFG0 = 0x%x\n",
		       ana_get_reg(MT6397_AUDACCDEPOP_CFG0));
	n += scnprintf(buffer + n, size - n, "AUD_IV_CFG0 = 0x%x\n",
		       ana_get_reg(MT6397_AUD_IV_CFG0));
	n += scnprintf(buffer + n, size - n, "AUDCLKGEN_CFG0 = 0x%x\n",
		       ana_get_reg(MT6397_AUDCLKGEN_CFG0));
	n += scnprintf(buffer + n, size - n, "AUDLDO_CFG0 = 0x%x\n",
		       ana_get_reg(MT6397_AUDLDO_CFG0));
	n += scnprintf(buffer + n, size - n, "AUDLDO_CFG1 = 0x%x\n",
		       ana_get_reg(MT6397_AUDLDO_CFG1));
	n += scnprintf(buffer + n, size - n, "AUDNVREGGLB_CFG0 = 0x%x\n",
		       ana_get_reg(MT6397_AUDNVREGGLB_CFG0));
	n += scnprintf(buffer + n, size - n, "AUD_NCP0 = 0x%x\n",
		       ana_get_reg(MT6397_AUD_NCP0));
	n += scnprintf(buffer + n, size - n, "AUDPREAMP_CON0 = 0x%x\n",
		       ana_get_reg(MT6397_AUDPREAMP_CON0));
	n += scnprintf(buffer + n, size - n, "AUDADC_CON0 = 0x%x\n",
		       ana_get_reg(MT6397_AUDADC_CON0));
	n += scnprintf(buffer + n, size - n, "AUDADC_CON1 = 0x%x\n",
		       ana_get_reg(MT6397_AUDADC_CON1));
	n += scnprintf(buffer + n, size - n, "AUDADC_CON2 = 0x%x\n",
		       ana_get_reg(MT6397_AUDADC_CON2));
	n += scnprintf(buffer + n, size - n, "AUDADC_CON3 = 0x%x\n",
		       ana_get_reg(MT6397_AUDADC_CON3));
	n += scnprintf(buffer + n, size - n, "AUDADC_CON4 = 0x%x\n",
		       ana_get_reg(MT6397_AUDADC_CON4));
	n += scnprintf(buffer + n, size - n, "AUDADC_CON5 = 0x%x\n",
		       ana_get_reg(MT6397_AUDADC_CON5));
	n += scnprintf(buffer + n, size - n, "AUDADC_CON6 = 0x%x\n",
		       ana_get_reg(MT6397_AUDADC_CON6));
	n += scnprintf(buffer + n, size - n, "AUDDIGMI_CON0 = 0x%x\n",
		       ana_get_reg(MT6397_AUDDIGMI_CON0));
	n += scnprintf(buffer + n, size - n, "AUDLSBUF_CON0 = 0x%x\n",
		       ana_get_reg(MT6397_AUDLSBUF_CON0));
	n += scnprintf(buffer + n, size - n, "AUDLSBUF_CON1 = 0x%x\n",
		       ana_get_reg(MT6397_AUDLSBUF_CON1));
	n += scnprintf(buffer + n, size - n, "AUDENCSPARE_CON0 = 0x%x\n",
		       ana_get_reg(MT6397_AUDENCSPARE_CON0));
	n += scnprintf(buffer + n, size - n, "AUDENCCLKSQ_CON0 = 0x%x\n",
		       ana_get_reg(MT6397_AUDENCCLKSQ_CON0));
	n += scnprintf(buffer + n, size - n, "AUDPREAMPGAIN_CON0 = 0x%x\n",
		       ana_get_reg(MT6397_AUDPREAMPGAIN_CON0));
	n += scnprintf(buffer + n, size - n, "ZCD_CON0 = 0x%x\n",
		       ana_get_reg(MT6397_ZCD_CON0));
	n += scnprintf(buffer + n, size - n, "ZCD_CON1 = 0x%x\n",
		       ana_get_reg(MT6397_ZCD_CON1));
	n += scnprintf(buffer + n, size - n, "ZCD_CON2 = 0x%x\n",
		       ana_get_reg(MT6397_ZCD_CON2));
	n += scnprintf(buffer + n, size - n, "ZCD_CON3 = 0x%x\n",
		       ana_get_reg(MT6397_ZCD_CON3));
	n += scnprintf(buffer + n, size - n, "ZCD_CON4 = 0x%x\n",
		       ana_get_reg(MT6397_ZCD_CON4));
	n += scnprintf(buffer + n, size - n, "ZCD_CON5 = 0x%x\n",
		       ana_get_reg(MT6397_ZCD_CON5));
	n += scnprintf(buffer + n, size - n, "NCP_CLKDIV_CON0 = 0x%x\n",
		       ana_get_reg(MT6397_NCP_CLKDIV_CON0));
	n += scnprintf(buffer + n, size - n, "NCP_CLKDIV_CON1 = 0x%x\n",
		       ana_get_reg(MT6397_NCP_CLKDIV_CON1));
	pr_notice("mt_soc_debug_read len = %d\n", n);

	return simple_read_from_buffer(buf, count, pos, buffer, n);
}



static const struct file_operations mt6397_debug_ops = {
	.open = mt_codec_debug_open,
	.read = mt_codec_debug_read,
};

static struct snd_soc_codec_driver soc_mtk_codec = {
	.probe = mt6397_codec_probe,
	.remove = mt6397_codec_remove,
	.get_regmap = mt6397_codec_get_regmap,

};

static int mtk_mt6397_codec_dev_probe(struct platform_device *pdev)
{
	pr_notice("%s dev name %s\n", __func__, dev_name(&pdev->dev));
	if (pdev->dev.of_node) {
		dev_set_name(&pdev->dev, "%s", "mt6397-codec");
		pr_notice("%s set dev name %s\n", __func__,
			  dev_name(&pdev->dev));
	}

	mt_codec_debugfs =
	    debugfs_create_file("mt6397reg", S_IFREG | S_IRUGO, NULL,
				(void *)"mt6397reg", &mt6397_debug_ops);

	return snd_soc_register_codec(&pdev->dev, &soc_mtk_codec,
			mtk_codec_dai_drvs, ARRAY_SIZE(mtk_codec_dai_drvs));
}

static int mtk_mt6397_codec_dev_remove(struct platform_device *pdev)
{
	pr_debug("AudCodec %s:\n", __func__);
	snd_soc_unregister_codec(&pdev->dev);
	debugfs_remove(mt_codec_debugfs);
	return 0;
}

static const struct of_device_id mtk_pmic_codec_dt_match[] = {
	{ .compatible = "mediatek,mt6397-codec", },
	{ }
};
MODULE_DEVICE_TABLE(of, mtk_pmic_codec_dt_match);

static struct platform_driver mtk_pmic_codec_driver = {
	.driver = {
		.name = "mt6397-codec",
		.owner = THIS_MODULE,
		.of_match_table = mtk_pmic_codec_dt_match,
	},
	.probe = mtk_mt6397_codec_dev_probe,
	.remove = mtk_mt6397_codec_dev_remove,
};

module_platform_driver(mtk_pmic_codec_driver);

/* Module information */
MODULE_DESCRIPTION("MTK 6397 codec driver");
MODULE_LICENSE("GPL v2");
