/*
 * mt_soc_afe_control.h  --  Mediatek AFE controller
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

#ifndef _AUDIO_AFE_CONTROL_H
#define _AUDIO_AFE_CONTROL_H

#include "mt_soc_digital_type.h"
#include <linux/device.h>
#include <linux/io.h>
#include <sound/pcm.h>

/*****************************************************************************
 *                  R E G I S T E R       D E F I N I T I O N
 *****************************************************************************/
#define AUDIO_TOP_CON0          0x0000
#define AUDIO_TOP_CON1          0x0004
#define AUDIO_TOP_CON3          0x000C
#define AFE_DAC_CON0            0x0010
#define AFE_DAC_CON1            0x0014
#define AFE_I2S_CON             0x0018
#define AFE_DAIBT_CON0          0x001c

#define AFE_CONN0               0x0020
#define AFE_CONN1               0x0024
#define AFE_CONN2               0x0028
#define AFE_CONN3               0x002C
#define AFE_CONN4               0x0030

#define AFE_I2S_CON1            0x0034
#define AFE_I2S_CON2            0x0038
#define AFE_MRGIF_CON           0x003C

/* Memory interface */
#define AFE_DL1_BASE            0x0040
#define AFE_DL1_CUR             0x0044
#define AFE_DL1_END             0x0048
#define AFE_DL2_BASE            0x0050
#define AFE_DL2_CUR             0x0054
#define AFE_DL2_END             0x0058
#define AFE_AWB_BASE            0x0070
#define AFE_AWB_END             0x0078
#define AFE_AWB_CUR             0x007C
#define AFE_VUL_BASE            0x0080
#define AFE_VUL_END             0x0088
#define AFE_VUL_CUR             0x008C
#define AFE_DAI_BASE            0x0090
#define AFE_DAI_END             0x0098
#define AFE_DAI_CUR             0x009C

#define AFE_IRQ_CON             0x00A0

/* Memory interface monitor */
#define AFE_MEMIF_MON0          0x00D0
#define AFE_MEMIF_MON1          0x00D4
#define AFE_MEMIF_MON2          0x00D8
#define AFE_MEMIF_MON3          0x00DC
#define AFE_MEMIF_MON4          0x00E0

#define AFE_ADDA_DL_SRC2_CON0   0x0108
#define AFE_ADDA_DL_SRC2_CON1   0x010C
#define AFE_ADDA_UL_SRC_CON0    0x0114
#define AFE_ADDA_UL_SRC_CON1    0x0118
#define AFE_ADDA_TOP_CON0       0x0120
#define AFE_ADDA_UL_DL_CON0     0x0124
#define AFE_ADDA_SRC_DEBUG      0x012C
#define AFE_ADDA_SRC_DEBUG_MON0 0x0130
#define AFE_ADDA_SRC_DEBUG_MON1 0x0134
#define AFE_ADDA_NEWIF_CFG0     0x0138
#define AFE_ADDA_NEWIF_CFG1     0x013C

#define AFE_FOC_CON             0x0170
#define AFE_FOC_CON1            0x0174
#define AFE_FOC_CON2            0x0178
#define AFE_FOC_CON3            0x017C
#define AFE_FOC_CON4            0x0180
#define AFE_FOC_CON5            0x0184
#define AFE_MON_STEP            0x0188

#define FOC_ROM_SIG             0x018C

#define AFE_SIDETONE_DEBUG      0x01D0
#define AFE_SIDETONE_MON        0x01D4
#define AFE_SIDETONE_CON0       0x01E0
#define AFE_SIDETONE_COEFF      0x01E4
#define AFE_SIDETONE_CON1       0x01E8
#define AFE_SIDETONE_GAIN       0x01EC
#define AFE_SGEN_CON0           0x01F0

#define AFE_TOP_CON0            0x0200

#define AFE_ADDA_PREDIS_CON0    0x0260
#define AFE_ADDA_PREDIS_CON1    0x0264

#define AFE_MRGIF_MON0          0x0270
#define AFE_MRGIF_MON1          0x0274
#define AFE_MRGIF_MON2          0x0278

#define AFE_MOD_PCM_BASE        0x0330
#define AFE_MOD_PCM_END         0x0338
#define AFE_MOD_PCM_CUR         0x033C

#define AFE_HDMI_OUT_CON0       0x0370
#define AFE_HDMI_OUT_BASE       0x0374
#define AFE_HDMI_OUT_CUR        0x0378
#define AFE_HDMI_OUT_END        0x037C
#define AFE_SPDIF_OUT_CON0      0x0380
#define AFE_SPDIF_BASE          0x0384
#define AFE_SPDIF_CUR           0x0388
#define AFE_SPDIF_END           0x038C
#define AFE_HDMI_CONN0          0x0390
#define AFE_8CH_I2S_OUT_CON     0x0394

#define AFE_IRQ_MCU_CON         0x03A0
#define AFE_IRQ_STATUS          0x03A4
#define AFE_IRQ_CLR             0x03A8
#define AFE_IRQ_CNT1            0x03AC
#define AFE_IRQ_CNT2            0x03B0
#define AFE_IRQ_MON2            0x03B8
#define AFE_IRQ_CNT5            0x03BC
#define AFE_IRQ1_CNT_MON        0x03C0
#define AFE_IRQ2_CNT_MON        0x03C4
#define AFE_IRQ1_EN_CNT_MON     0x03C8
#define AFE_IRQ5_EN_CNT_MON     0x03cc
#define AFE_MEMIF_MINLEN        0x03D0
#define AFE_MEMIF_MAXLEN        0x03D4
#define AFE_MEMIF_PBUF_SIZE     0x03D8

/* AFE GAIN CONTROL REGISTER */
#define AFE_GAIN1_CON0          0x0410
#define AFE_GAIN1_CON1          0x0414
#define AFE_GAIN1_CON2          0x0418
#define AFE_GAIN1_CON3          0x041C
#define AFE_GAIN1_CONN          0x0420
#define AFE_GAIN1_CUR           0x0424
#define AFE_GAIN2_CON0          0x0428
#define AFE_GAIN2_CON1          0x042C
#define AFE_GAIN2_CON2          0x0430
#define AFE_GAIN2_CON3          0x0434
#define AFE_GAIN2_CONN          0x0438
#define AFE_GAIN2_CUR           0x043C
#define AFE_GAIN2_CONN2         0x0440

#define AFE_IEC_CFG             0x0480
#define AFE_IEC_NSNUM           0x0484
#define AFE_IEC_BURST_INFO      0x0488
#define AFE_IEC_BURST_LEN       0x048C
#define AFE_IEC_NSADR           0x0490
#define AFE_IEC_CHL_STAT0       0x04A0
#define AFE_IEC_CHL_STAT1       0x04A4
#define AFE_IEC_CHR_STAT0       0x04A8
#define AFE_IEC_CHR_STAT1       0x04AC

/* here is only fpga needed */
#define FPGA_CFG2               0x04B8
#define FPGA_CFG3               0x04BC
#define FPGA_CFG0               0x04C0
#define FPGA_CFG1               0x04C4
#define FPGA_VERSION            0x04C8
#define FPGA_STC                0x04CC

#define AFE_ASRC_CON0           0x0500
#define AFE_ASRC_CON1           0x0504
#define AFE_ASRC_CON2           0x0508
#define AFE_ASRC_CON3           0x050C
#define AFE_ASRC_CON4           0x0510
#define AFE_ASRC_CON5           0x0514
#define AFE_ASRC_CON6           0x0518
#define AFE_ASRC_CON7           0x051C
#define AFE_ASRC_CON8           0x0520
#define AFE_ASRC_CON9           0x0524
#define AFE_ASRC_CON10          0x0528
#define AFE_ASRC_CON11          0x052C

#define PCM_INTF_CON1           0x0530
#define PCM_INTF_CON2           0x0538
#define PCM2_INTF_CON           0x053C


/**********************************
 *  Detailed Definitions
 **********************************/

/* AFE_TOP_CON0 */
#define PDN_AFE  2
#define PDN_ADC  5
#define PDN_I2S  6
#define APB_W2T 12
#define APB_R2T 13
#define APB_SRC 14
#define PDN_APLL_TUNER 19
#define PDN_HDMI_CK    20
#define PDN_SPDF_CK    21

/* AUDIO_TOP_CON3 */
#define HDMI_SPEAKER_OUT_HDMI_POS 5
#define HDMI_SPEAKER_OUT_HDMI_LEN 1
#define HDMI_BCK_DIV_LEN      6
#define HDMI_BCK_DIV_POS      8
#define HDMI_2CH_SEL_POS      6
#define HDMI_2CH_SEL_LEN      2
#define HDMI_2CH_SEL_SDATA0 0
#define HDMI_2CH_SEL_SDATA1 1
#define HDMI_2CH_SEL_SDATA2 2
#define HDMI_2CH_SEL_SDATA3 3

/* AFE_DAC_CON0 */
#define AFE_ON      0
#define DL1_ON      1
#define DL2_ON      2
#define VUL_ON      3
#define DAI_ON      4
#define I2S_ON      5
#define AWB_ON      6
#define MOD_PCM_ON  7
#define AFE_ON_RETM  12
#define AFE_DL1_RETM 13
#define AFE_DL2_RETM 14
#define AFE_AWB_RETM 16

/* AFE_DAC_CON1 */
#define DL1_MODE_LEN    4
#define DL1_MODE_POS    0

#define DL2_MODE_LEN    4
#define DL2_MODE_POS    4

#define I2S_MODE_LEN    4
#define I2S_MODE_POS    8

#define AWB_MODE_LEN    4
#define AWB_MODE_POS    12

#define VUL_MODE_LEN    4
#define VUL_MODE_POS    16

#define DAI_MODE_LEN    1
#define DAI_MODE_POS    20

#define DL1_DATA_LEN    1
#define DL1_DATA_POS    21

#define DL2_DATA_LEN    1
#define DL2_DATA_POS    22

#define I2S_DATA_LEN    1
#define I2S_DATA_POS    23

#define AWB_DATA_LEN    1
#define AWB_DATA_POS    24

#define AWB_R_MONO_LEN  1
#define AWB_R_MONO_POS  25

#define VUL_DATA_LEN    1
#define VUL_DATA_POS    27

#define VUL_R_MONO_LEN  1
#define VUL_R_MONO_POS  28

#define DAI_DUP_WR_LEN  1
#define DAI_DUP_WR_POS  29

#define MOD_PCM_MODE_LEN    1
#define MOD_PCM_MODE_POS    30

#define MOD_PCM_DUP_WR_LEN  1
#define MOD_PCM_DUP_WR_POS  31

/* AFE_I2S_CON1 and AFE_I2S_CON2 */
#define AI2S_EN_POS             0
#define AI2S_EN_LEN             1
#define AI2S_WLEN_POS           1
#define AI2S_WLEN_LEN           1
#define AI2S_FMT_POS            3
#define AI2S_FMT_LEN            1
#define AI2S_OUT_MODE_POS       8
#define AI2S_OUT_MODE_LEN       4
#define AI2S_UPDATE_WORD_POS    24
#define AI2S_UPDATE_WORD_LEN    5
#define AI2S_LR_SWAP_POS        31
#define AI2S_LR_SWAP_LEN        1

#define I2S_EN_POS          0
#define I2S_EN_LEN          1
#define I2S_WLEN_POS        1
#define I2S_WLEN_LEN        1
#define I2S_SRC_POS         2
#define I2S_SRC_LEN         1
#define I2S_FMT_POS         3
#define I2S_FMT_LEN         1
#define I2S_DIR_POS         4
#define I2S_DIR_LEN         1
#define I2S_OUT_MODE_POS    8
#define I2S_OUT_MODE_LEN    4

#define FOC_EN_POS  0
#define FOC_EN_LEN  1

/* Modem PCM 1 */
#define PCM_EN_POS          0
#define PCM_EN_LEN          1

#define PCM_FMT_POS         1
#define PCM_FMT_LEN         2

#define PCM_MODE_POS        3
#define PCM_MODE_LEN        1

#define PCM_WLEN_POS        4
#define PCM_WLEN_LEN        1

#define PCM_SLAVE_POS       5
#define PCM_SLAVE_LEN       1

#define PCM_BYP_ASRC_POS    6
#define PCM_BYP_ASRC_LEN    1

#define PCM_BTMODE_POS      7
#define PCM_BTMODE_LEN      1

#define PCM_SYNC_TYPE_POS   8
#define PCM_SYNC_TYPE_LEN   1

#define PCM_SYNC_LEN_POS    9
#define PCM_SYNC_LEN_LEN    5

#define PCM_EXT_MODEM_POS   17
#define PCM_EXT_MODEM_LEN   1

#define PCM_VBT16K_MODE_POS 18
#define PCM_VBT16K_MODE_LEN 1

/* #define PCM_BCKINV_POS      6 */
/* #define PCM_BCKINV_LEN      1 */
/* #define PCM_SYNCINV_POS     7 */
/* #define PCM_SYNCINV_LEN     1 */

#define PCM_SERLOOPBK_POS   28
#define PCM_SERLOOPBK_LEN   1

#define PCM_PARLOOPBK_POS   29
#define PCM_PARLOOPBK_LEN   1

#define PCM_BUFLOOPBK_POS   30
#define PCM_BUFLOOPBK_LEN   1

#define PCM_FIX_VAL_SEL_POS 31
#define PCM_FIX_VAL_SEL_LEN 1

/* BT PCM */
#define DAIBT_EN_POS   0
#define DAIBT_EN_LEN   1
#define BTPCM_EN_POS   1
#define BTPCM_EN_LEN   1
#define BTPCM_SYNC_POS   2
#define BTPCM_SYNC_LEN   1
#define DAIBT_DATARDY_POS   3
#define DAIBT_DATARDY_LEN   1
#define BTPCM_LENGTH_POS   4
#define BTPCM_LENGTH_LEN   3
#define DAIBT_MODE_POS   9
#define DAIBT_MODE_LEN   1

/* AFE_IRQ_CON */
#define IRQ1_ON         0
#define IRQ2_ON         1
#define IRQ3_ON         2
#define IRQ4_ON         3
#define IRQ1_FS         4
#define IRQ2_FS         8
#define IRQ5_ON         12
#define IRQ6_ON         13
#define IRQ_SETTING_BIT 0x3007

/* AFE_IRQ_STATUS */
#define IRQ1_ON_BIT     (1<<0)
#define IRQ2_ON_BIT     (1<<1)
#define IRQ3_ON_BIT     (1<<2)
#define IRQ4_ON_BIT     (1<<3)
#define IRQ5_ON_BIT     (1<<4)
#define IRQ6_ON_BIT     (1<<5)
#define IRQ_STATUS_BIT  0x3F

/* AFE_IRQ_CLR */
#define IRQ1_CLR (1<<0)
#define IRQ2_CLR (1<<1)
#define IRQ3_CLR (1<<2)
#define IRQ4_CLR (1<<3)
#define IRQ_CLR  (1<<4)

#define IRQ1_MISS_CLR (1<<8)
#define IRQ2_MISS_CLR (1<<9)
#define IRQ3_MISS_CLR (1<<10)
#define IRQ4_MISS_CLR (1<<11)
#define IRQ5_MISS_CLR (1<<12)
#define IRQ6_MISS_CLR (1<<13)

/* AFE_IRQ_MCU_MON2 */
#define IRQ1_MISS_BIT       (1<<8)
#define IRQ2_MISS_BIT       (1<<9)
#define IRQ3_MISS_BIT       (1<<10)
#define IRQ4_MISS_BIT       (1<<11)
#define IRQ5_MISS_BIT       (1<<12)
#define IRQ6_MISS_BIT       (1<<13)
#define IRQ_MISS_STATUS_BIT 0x3F00

/* AUDIO_TOP_CON3 */
#define HDMI_OUT_SPEAKER_BIT    4
#define SPEAKER_OUT_HDMI        5
#define HDMI_2CH_SEL_POS        6
#define HDMI_2CH_SEL_LEN        2

/* AFE_SIDETONE_DEBUG */
#define STF_SRC_SEL     16
#define STF_I5I6_SEL    19

/* AFE_SIDETONE_CON0 */
#define STF_COEFF_VAL       0
#define STF_COEFF_ADDRESS   16
#define STF_CH_SEL          23
#define STF_COEFF_W_ENABLE  24
#define STF_W_ENABLE        25
#define STF_COEFF_BIT       0x0000FFFF

/* AFE_SIDETONE_CON1 */
#define STF_TAP_NUM 0
#define STF_ON      8
#define STF_BYPASS  31

/* AFE_SGEN_CON0 */
#define SINE_TONE_FREQ_DIV_CH1  0
#define SINE_TONE_AMP_DIV_CH1   5
#define SINE_TONE_MODE_CH1      8
#define SINE_TONE_FREQ_DIV_CH2  12
#define SINE_TONE_AMP_DIV_CH2   17
#define SINE_TONE_MODE_CH2      20
#define SINE_TONE_MUTE_CH1      24
#define SINE_TONE_MUTE_CH2      25
#define SINE_TONE_ENABLE        26
#define SINE_TONE_LOOPBACK_MOD  28

/* FPGA_CFG0 */
#define MCLK_MUX2_POS    26
#define MCLK_MUX2_LEN    1
#define MCLK_MUX1_POS    25
#define MCLK_MUX1_LEN    1
#define MCLK_MUX0_POS    24
#define MCLK_MUX0_LEN    1
#define SOFT_RST_POS   16
#define SOFT_RST_LEN   8
#define HOP26M_SEL_POS   12
#define HOP26M_SEL_LEN   2

/* FPGA_CFG1 */
#define CODEC_SEL_POS   0
#define DAC_SEL_POS     4
#define ADC_SEL_POS     8


/* #define AUDPLL_CON0   (APMIXED_BASE+0x02E8) */
#define AUDPLL_CON1   (APMIXED_BASE+0x02EC)
#define AUDPLL_CON4   (APMIXED_BASE+0x02F8)

/* apmixed sys AUDPLL_CON4 */
#define AUDPLL_SDM_PCW_98M       98304000 /*0x3C7EA932*/
#define AUDPLL_SDM_PCW_90M       90316800 /*0x37945EA6*/
/*#define AUDPLL_TUNER_N_98M       0x3C7EA933	48k, 98.304M , sdm_pcw+1 */
/*#define AUDPLL_TUNER_N_90M       0x37945EA7	44.1k, 90.3168M, sdm_pcw+1 */

/* AUDPLL_CON0 */
#define AUDPLL_EN_POS            0
#define AUDPLL_EN_LEN            1
#define AUDPLL_PREDIV_POS        4
#define AUDPLL_PREDIV_LEN        2
#define AUDPLL_POSDIV_POS        6
#define AUDPLL_POSDIV_LEN        3
/* AUDPLL_CON1 */
#define AUDPLL_SDM_PCW_POS       0
#define AUDPLL_SDM_PCW_LEN      31
#define AUDPLL_SDM_PCW_CHG_POS  31
#define AUDPLL_SDM_PCW_CHG_LEN   1
/* AUDPLL_CON4 */
#define AUDPLL_TUNER_N_INFO_POS  0
#define AUDPLL_TUNER_N_INFO_LEN 31
#define AUDPLL_TUNER_N_INFO_MASK 0x7FFFFFFF
#define AUDPLL_TUNER_EN_POS     31
#define AUDPLL_TUNER_EN_LEN      1
#define AUDPLL_TUNER_EN_MASK     0x80000000

/* #define CLK_CFG_9       (TOPCKEGN_BASE+0x0168) */
#define CLK_APLL_SEL_POS     16
#define CLK_APLL_SEL_LEN     3
#define CLKSQ_MUX_CK     0
#define AD_APLL_CK       1
#define APLL_D4          2
#define APLL_D8          3
#define APLL_D16         4
#define APLL_D24        5
#define PDN_APLL_POS     23
#define PDN_APLL_LEN      1

/*****************************************************************************
 *                E X T E R N A L   R E F E R E N C E S
 *****************************************************************************/
extern void *afe_base_address;
extern void *afe_sram_address;
extern uint32_t afe_sram_phy_address;
extern uint32_t afe_sram_size;
extern struct afe_mem_control afe_vul_control_context;
extern struct afe_mem_control afe_dai_control_context;

/*****************************************************************************
 *                          C O N S T A N T S
 *****************************************************************************/
#define AUDDRV_DL1_MAX_BUFFER_LENGTH (0x4000)
#define MASK_ALL          (0xFFFFFFFF)


void afe_set_reg(uint32_t offset, uint32_t value, uint32_t mask);
static inline uint32_t afe_get_reg(uint32_t offset)
{
	const uint32_t *address = (uint32_t *)((char *)afe_base_address
					       + offset);
	return readl(address);
}
int do_platform_init(struct device *dev);
bool init_afe_control(void);
bool reset_afe_control(void);
bool set_sample_rate(uint32_t aud_block, uint32_t sample_rate);
bool set_channels(uint32_t memory_interface, uint32_t channel);

bool set_irq_mcu_counter(uint32_t irqmode, uint32_t counter);
bool set_irq_enable(uint32_t irqmode, bool enable);
bool set_irq_mcu_sample_rate(uint32_t irqmode, uint32_t sample_rate);
bool get_irq_status(uint32_t irqmode, struct aud_irq_mcu_mode *mcumode);

bool set_connection(uint32_t connection_state, uint32_t input, uint32_t output);
bool set_memory_path_enable(uint32_t aud_block, bool enable);
bool set_i2s_dac_enable(bool enable);
bool get_i2s_dac_enable(void);
void enable_afe(bool enable);
bool set_i2s_adc_in(struct aud_digital_i2s *digtal_i2s);
bool set_i2s_adc_enable(bool enable);
bool set_i2s_dac_out(uint32_t sample_rate);
void clean_pre_distortion(void);

void aud_drv_dl_set_platform_info(enum MTAUD_BLOCK digital_block,
				  struct snd_pcm_substream *substream);
void aud_drv_dl_reset_platform_info(enum MTAUD_BLOCK digital_block);
void aud_drv_reset_buffer(enum MTAUD_BLOCK digital_block);
int aud_drv_allocate_dl1_buffer(uint32_t afe_buf_length);
int aud_drv_update_hw_ptr(enum MTAUD_BLOCK digital_block,
			  struct snd_pcm_substream *substream);


#endif
