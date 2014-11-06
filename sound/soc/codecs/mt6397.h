/*
 * mt6397.h  --  MT6397 ALSA SoC codec driver
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

#ifndef _MTCODEC_TYPE_H
#define _MTCODEC_TYPE_H

#include <linux/types.h>
#include <sound/soc.h>

/*****************************************************************************
 *                  R E G I S T E R       D E F I N I T I O N
 *****************************************************************************/

/* digital pmic register*/
#define MT6397_DIG_AUDIO_BASE              (0x4000)
#define MT6397_AFE_UL_DL_CON0              (MT6397_DIG_AUDIO_BASE + 0x0000)
#define MT6397_AFE_DL_SRC2_CON0_H          (MT6397_DIG_AUDIO_BASE + 0x0002)
#define MT6397_AFE_DL_SRC2_CON0_L          (MT6397_DIG_AUDIO_BASE + 0x0004)
#define MT6397_AFE_DL_SDM_CON0             (MT6397_DIG_AUDIO_BASE + 0x0006)
#define MT6397_AFE_DL_SDM_CON1             (MT6397_DIG_AUDIO_BASE + 0x0008)
#define MT6397_AFE_UL_SRC_CON0_H           (MT6397_DIG_AUDIO_BASE + 0x000A)
#define MT6397_AFE_UL_SRC_CON0_L           (MT6397_DIG_AUDIO_BASE + 0x000C)
#define MT6397_AFE_UL_SRC_CON1_H           (MT6397_DIG_AUDIO_BASE + 0x000E)
#define MT6397_AFE_UL_SRC_CON1_L           (MT6397_DIG_AUDIO_BASE + 0x0010)
#define MT6397_AFE_TOP_CON0                (MT6397_DIG_AUDIO_BASE + 0x0012)
#define MT6397_AUDIO_TOP_CON0              (MT6397_DIG_AUDIO_BASE + 0x0014)
#define MT6397_AFE_DL_SRC_MON0             (MT6397_DIG_AUDIO_BASE + 0x0016)
#define MT6397_AFE_DL_SDM_TEST0            (MT6397_DIG_AUDIO_BASE + 0x0018)
#define MT6397_AFE_MON_DEBUG0              (MT6397_DIG_AUDIO_BASE + 0x001A)
#define MT6397_AFUNC_AUD_CON0              (MT6397_DIG_AUDIO_BASE + 0x001C)
#define MT6397_AFUNC_AUD_CON1              (MT6397_DIG_AUDIO_BASE + 0x001E)
#define MT6397_AFUNC_AUD_CON2              (MT6397_DIG_AUDIO_BASE + 0x0020)
#define MT6397_AFUNC_AUD_CON3              (MT6397_DIG_AUDIO_BASE + 0x0022)
#define MT6397_AFUNC_AUD_CON4              (MT6397_DIG_AUDIO_BASE + 0x0024)
#define MT6397_AFUNC_AUD_MON0              (MT6397_DIG_AUDIO_BASE + 0x0026)
#define MT6397_AFUNC_AUD_MON1              (MT6397_DIG_AUDIO_BASE + 0x0028)
#define MT6397_AUDRC_TUNE_MON0             (MT6397_DIG_AUDIO_BASE + 0x002A)
#define MT6397_AFE_UP8X_FIFO_CFG0          (MT6397_DIG_AUDIO_BASE + 0x002C)
#define MT6397_AFE_UP8X_FIFO_LOG_MON0      (MT6397_DIG_AUDIO_BASE + 0x002E)
#define MT6397_AFE_UP8X_FIFO_LOG_MON1      (MT6397_DIG_AUDIO_BASE + 0x0030)
#define MT6397_AFE_DL_DC_COMP_CFG0         (MT6397_DIG_AUDIO_BASE + 0x0032)
#define MT6397_AFE_DL_DC_COMP_CFG1         (MT6397_DIG_AUDIO_BASE + 0x0034)
#define MT6397_AFE_DL_DC_COMP_CFG2         (MT6397_DIG_AUDIO_BASE + 0x0036)
#define MT6397_AFE_PMIC_NEWIF_CFG0         (MT6397_DIG_AUDIO_BASE + 0x0038)
#define MT6397_AFE_PMIC_NEWIF_CFG1         (MT6397_DIG_AUDIO_BASE + 0x003A)
#define MT6397_AFE_PMIC_NEWIF_CFG2         (MT6397_DIG_AUDIO_BASE + 0x003C)
#define MT6397_AFE_PMIC_NEWIF_CFG3         (MT6397_DIG_AUDIO_BASE + 0x003E)
#define MT6397_AFE_SGEN_CFG0               (MT6397_DIG_AUDIO_BASE + 0x0040)
#define MT6397_AFE_SGEN_CFG1               (MT6397_DIG_AUDIO_BASE + 0x0042)

/* analog pmic  register*/
#include <linux/mfd/mt6397/registers.h>
#define MT6397_TRIM_ADDRESS1            (MT6397_EFUSE_DOUT_192_207) /* [3:15] */
#define MT6397_TRIM_ADDRESS2            (MT6397_EFUSE_DOUT_208_223) /* [0:11] */


/*****************************************************************************
 *                ENUM DEFINITION
 *****************************************************************************/

enum MTCODEC_VOL_TYPE {
	MTCODEC_VOL_HSOUTL = 0,
	MTCODEC_VOL_HSOUTR,
	MTCODEC_VOL_HPOUTL,
	MTCODEC_VOL_HPOUTR,
	MTCODEC_VOL_SPKL,
	MTCODEC_VOL_SPKR,
	MTCODEC_VOL_SPK_HS_R,
	MTCODEC_VOL_SPK_HS_L,
	MTCODEC_VOL_IV_BUFFER,
	MTCODEC_VOL_LINEOUTL,
	MTCODEC_VOL_LINEOUTR,
	MTCODEC_VOL_LINEINL,
	MTCODEC_VOL_LINEINR,
	MTCODEC_VOL_MICAMPL,
	MTCODEC_VOL_MICAMPR,
	MTCODEC_VOL_LEVELSHIFTL,
	MTCODEC_VOL_LEVELSHIFTR,
	MTCODEC_VOL_TYPE_MAX
};

/* mux seleciotn */
enum MTCODEC_MUX_TYPE {
	MTCODEC_MUX_VOICE = 0,
	MTCODEC_MUX_AUDIO,
	MTCODEC_MUX_IV_BUFFER,
	MTCODEC_MUX_LINEIN_STEREO,
	MTCODEC_MUX_LINEIN_L,
	MTCODEC_MUX_LINEIN_R,
	MTCODEC_MUX_LINEIN_AUDIO_MONO,
	MTCODEC_MUX_LINEIN_AUDIO_STEREO,
	MTCODEC_MUX_MIC1,
	MTCODEC_MUX_MIC2,
	MTCODEC_MUX_MIC3,
	MTCODEC_MUX_MIC4,
	MTCODEC_MUX_LINE_IN,
	MTCODEC_MUX_PREAMP1,
	MTCODEC_MUX_PREAMP2,
	MTCODEC_MUX_LEVEL_SHIFT_BUFFER,
	MTCODEC_MUX_MUTE,
	MTCODEC_MUX_OPEN,
	MTCODEC_MAX_MUX_TYPE
};

/* device power */
enum MTCODEC_DEVICE_TYPE {
	MTCODEC_DEVICE_OUT_EARPIECER = 0,
	MTCODEC_DEVICE_OUT_EARPIECEL = 1,
	MTCODEC_DEVICE_OUT_HSR = 2,
	MTCODEC_DEVICE_OUT_HSL = 3,
	MTCODEC_DEVICE_OUT_SPKR = 4,
	MTCODEC_DEVICE_OUT_SPKL = 5,
	MTCODEC_DEVICE_OUT_SPK_HS_R = 6,
	MTCODEC_DEVICE_OUT_SPK_HS_L = 7,
	MTCODEC_DEVICE_OUT_LINEOUTR = 8,
	MTCODEC_DEVICE_OUT_LINEOUTL = 9,
	MTCODEC_DEVICE_2IN1_SPK = 10,
	/* DEVICE_IN_LINEINR = 11, */
	/* DEVICE_IN_LINEINL = 12, */
	MTCODEC_DEVICE_ADC1 = 13,
	MTCODEC_DEVICE_ADC2 = 14,
	MTCODEC_DEVICE_ADC3 = 15,
	MTCODEC_DEVICE_PREAMP_L = 16,
	MTCODEC_DEVICE_PREAMP_R = 17,
	MTCODEC_DEVICE_DMIC = 18,
	MTCODEC_DEVICE_MAX
};

enum MTCODEC_DEVICE_SAMPLERATE_TYPE {
	MTCODEC_DEVICE_DAC,
	MTCODEC_DEVICE_ADC,
	MTCODEC_DEVICE_INOUT_MAX
};

enum MTCODEC_DEVICE_TYPE_SETTING {
	MTCODEC_DEVICE_PLATFORM_MACHINE,
	MTCODEC_DEVICE_PLATFORM,
	MTCODEC_DEVICE_MACHINE,
	MTCODEC_DEVICE_TYPE_SETTING_MAX
};

enum MTCODEC_AUDIOANALOG_TYPE {
	MTCODEC_AUDIOANALOG_DEVICE,
	MTCODEC_AUDIOANALOG_VOLUME,
	MTCODEC_AUDIOANALOG_MUX
};

enum MTCODEC_AUDIOANALOGZCD_TYPE {
	MTCODEC_AUDIOANALOGZCD_HEADPHONE = 1,
	MTCODEC_AUDIOANALOGZCD_HANDSET = 2,
	MTCODEC_AUDIOANALOGZCD_IVBUFFER = 3,
};

enum MTCODEC_SPEAKER_CLASS {
	MTCODEC_CLASS_AB = 0,
	MTCODEC_CLASS_D,
};

enum MTCODEC_CHANNELS {
	MTCODEC_CHANNELS_LEFT1 = 0,
	MTCODEC_CHANNELS_RIGHT1,
};

enum MTCODEC_LOOPBACK {
	MTCODEC_LOOP_NONE = 0,
	MTCODEC_LOOP_AMIC_SPK,
	MTCODEC_LOOP_AMIC_HP,
	MTCODEC_LOOP_DMIC_SPK,
	MTCODEC_LOOP_DMIC_HP,
	MTCODEC_LOOP_HS_SPK,
	MTCODEC_LOOP_HS_HP,
};

struct mt6397_codec_priv {
	int volume[MTCODEC_VOL_TYPE_MAX];
	int mux[MTCODEC_MAX_MUX_TYPE];
	int power[MTCODEC_DEVICE_MAX];
	int spk_channel_sel;
	uint8_t hp_l_trim;
	uint8_t hp_l_fine_trim;
	uint8_t hp_r_trim;
	uint8_t hp_r_fine_trim;
	uint8_t iv_hp_l_trim;
	uint8_t iv_hp_l_fine_trim;
	uint8_t iv_hp_r_trim;
	uint8_t iv_hp_r_fine_trim;
	uint8_t spk_l_polarity;
	uint8_t spk_l_trim;
	uint8_t spk_r_polarity;
	uint8_t spk_r_trim;
	uint32_t codec_loopback_type;
	struct snd_soc_codec *mt6397_codec;
	uint32_t mt6397_fs[MTCODEC_DEVICE_INOUT_MAX];

};

#define ENUM_TO_STR(enum) #enum

#endif
