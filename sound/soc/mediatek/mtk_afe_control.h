/*
 * mtk_afe_control.h  --  Mediatek AFE controller
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

#ifndef _MTK_AFE_CONTROL_H
#define _MTK_AFE_CONTROL_H

#include "mtk_afe_common.h"
#include <linux/io.h>
#include <linux/platform_device.h>

/*****************************************************************************
 *                  R E G I S T E R       D E F I N I T I O N
 *****************************************************************************/
#define AUDIO_TOP_CON0          0x0000
#define AUDIO_TOP_CON1          0x0004
#define AUDIO_TOP_CON2          0x0008
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
#define AFE_CONN5               0x005C
#define AFE_CONN6               0x00BC
#define AFE_CONN7               0x0460
#define AFE_CONN8               0x0464
#define AFE_CONN9               0x0468

#define AFE_I2S_CON1            0x0034
#define AFE_I2S_CON2            0x0038
#define AFE_MRGIF_CON           0x003C

/* Memory interface */
#define AFE_DL1_BASE            0x0040
#define AFE_DL1_CUR             0x0044
#define AFE_DL1_END             0x0048
#define AFE_DL1_D2_BASE         0x0340
#define AFE_DL1_D2_CUR          0x0344
#define AFE_DL1_D2_END          0x0348
#define AFE_VUL_D2_BASE         0x0350
#define AFE_VUL_D2_END          0x0358
#define AFE_VUL_D2_CUR          0x035C
#define AFE_I2S_CON3		0x004C
#define AFE_DL2_BASE            0x0050
#define AFE_DL2_CUR             0x0054
#define AFE_DL2_END             0x0058
#define AFE_CONN_24BIT		0x006C
#define AFE_AWB_BASE            0x0070
#define AFE_AWB_END             0x0078
#define AFE_AWB_CUR             0x007C
#define AFE_VUL_BASE            0x0080
#define AFE_VUL_END             0x0088
#define AFE_VUL_CUR             0x008C
#define AFE_DAI_BASE            0x0090
#define AFE_DAI_END             0x0098
#define AFE_DAI_CUR             0x009C

/* Memory interface monitor */
#define AFE_MEMIF_MSB           0x00CC
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

#define AFE_SIDETONE_DEBUG      0x01D0
#define AFE_SIDETONE_MON        0x01D4
#define AFE_SIDETONE_CON0       0x01E0
#define AFE_SIDETONE_COEFF      0x01E4
#define AFE_SIDETONE_CON1       0x01E8
#define AFE_SIDETONE_GAIN       0x01EC
#define AFE_SGEN_CON0           0x01F0
#define AFE_SGEN_CON1           0x01F4
#define AFE_TOP_CON0            0x0200

#define AFE_ADDA_PREDIS_CON0    0x0260
#define AFE_ADDA_PREDIS_CON1    0x0264

#define AFE_MRGIF_MON0          0x0270
#define AFE_MRGIF_MON1          0x0274
#define AFE_MRGIF_MON2          0x0278

#define AFE_MOD_PCM_BASE        0x0330
#define AFE_MOD_PCM_END         0x0338
#define AFE_MOD_PCM_CUR         0x033C

#define AFE_SPDIF2_OUT_CON0     0x0360
#define AFE_SPDIF2_BASE         0x0364
#define AFE_SPDIF2_CUR          0x0368
#define AFE_SPDIF2_END          0x036C
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
#define AFE_HDMI_CONN1		0x0398

#define AFE_IRQ_MCU_CON         0x03A0
#define AFE_IRQ_STATUS          0x03A4
#define AFE_IRQ_CLR             0x03A8
#define AFE_IRQ_CNT1            0x03AC
#define AFE_IRQ_CNT2            0x03B0
#define AFE_IRQ_MCU_EN          0x03B4
#define AFE_IRQ_MON2            0x03B8
#define AFE_IRQ_CNT5            0x03BC
#define AFE_IRQ1_CNT_MON        0x03C0
#define AFE_IRQ2_CNT_MON        0x03C4
#define AFE_IRQ1_EN_CNT_MON     0x03C8
#define AFE_IRQ5_EN_CNT_MON     0x03CC
#define AFE_MEMIF_MAXLEN        0x03D4
#define AFE_MEMIF_PBUF_SIZE     0x03D8
#define AFE_IRQ_MCU_CNT7        0x03DC
#define AFE_MEMIF_PBUF2_SIZE    0x03EC
#define AFE_APLL1_TUNER_CFG	0x03F0
#define AFE_APLL2_TUNER_CFG	0x03F4

/* AFE GAIN CONTROL REGISTER */
#define AFE_GAIN1_CON0         0x0410
#define AFE_GAIN1_CON1         0x0414
#define AFE_GAIN1_CON2         0x0418
#define AFE_GAIN1_CON3         0x041C
#define AFE_GAIN1_CONN         0x0420
#define AFE_GAIN1_CUR          0x0424
#define AFE_GAIN2_CON0         0x0428
#define AFE_GAIN2_CON1         0x042C
#define AFE_GAIN2_CON2         0x0430
#define AFE_GAIN2_CON3         0x0434
#define AFE_GAIN2_CONN         0x0438
#define AFE_GAIN2_CUR          0x043C
#define AFE_GAIN2_CONN2        0x0440
#define AFE_GAIN2_CONN3        0x0444
#define AFE_GAIN1_CONN2        0x0448
#define AFE_GAIN1_CONN3        0x044C

#define AFE_IEC_CFG             0x0480
#define AFE_IEC_NSNUM           0x0484
#define AFE_IEC_BURST_INFO      0x0488
#define AFE_IEC_BURST_LEN       0x048C
#define AFE_IEC_NSADR           0x0490
#define AFE_IEC_CHL_STAT0       0x04A0
#define AFE_IEC_CHL_STAT1       0x04A4
#define AFE_IEC_CHR_STAT0       0x04A8
#define AFE_IEC_CHR_STAT1       0x04AC

#define AFE_IEC2_CFG            0x04B0
#define AFE_IEC2_NSNUM          0x04B4
#define AFE_IEC2_BURST_INFO     0x04B8
#define AFE_IEC2_BURST_LEN      0x04BC
#define AFE_IEC2_NSADR          0x04C0
#define AFE_IEC2_CHL_STAT0      0x04D0
#define AFE_IEC2_CHL_STAT1      0x04D4
#define AFE_IEC2_CHR_STAT0      0x04D8
#define AFE_IEC2_CHR_STAT1      0x04DC

#define AFE_ASRC_CON0		0x500
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

#define AFE_TDM_CON1		0x548
#define AFE_TDM_CON2		0x54C

#define AFE_ASRC_CON13		0x550
#define AFE_ASRC_CON14		0x554
#define AFE_ASRC_CON15		0x558
#define AFE_ASRC_CON16		0x55C
#define AFE_ASRC_CON17		0x560
#define AFE_ASRC_CON18		0x564
#define AFE_ASRC_CON19		0x568
#define AFE_ASRC_CON20		0x56C
#define AFE_ASRC_CON21		0x570

#define AFE_ADDA2_TOP_CON0      0x0600

/**********************************
 *  Detailed Definitions
 **********************************/

/* AUDIO_TOP_CON3 */
#define HDMI_BCK_DIV_LEN	6
#define HDMI_BCK_DIV_POS	8

/* AFE_IRQ_STATUS */
#define AFE_IRQ1_BIT		(1<<0)
#define AFE_IRQ2_BIT		(1<<1)
#define AFE_HDMI_IRQ_BIT	(1<<4)
#define AFE_IRQ_STATUS_BITS	0x3F

/* others FIXME to remove */
/* APMIXEDSYS */
#define AP_PLL_CON5		0x0014

/* FIXME: only for debug begin */
#define AUDIO_APLL1_CON0        0x02A0
#define AUDIO_APLL1_CON1        0x02A4
#define AUDIO_APLL1_CON2        0x02A8
#define AUDIO_APLL1_PWR_CON0    0x02B0
#define AUDIO_APLL2_CON0        0x02B4
#define AUDIO_APLL2_CON1        0x02B8
#define AUDIO_APLL2_CON2        0x02C0
#define AUDIO_APLL2_PWR_CON0    0x02C4
/* FIXME: only for debug end */

/* CKSYS_TOP */

/* FIXME: only for debug begin */
#define AUDIO_DCM_CFG           0x0004
#define AUDIO_CLK_CFG_4		0x0080
#define AUDIO_CLK_CFG_6		0x00A0
#define AUDIO_CLK_CFG_7		0x00B0
/* FIXME: only for debug end */

#define AUDIO_CLK_AUDDIV_0	0x0120
#define AUDIO_CLK_AUDDIV_1	0x0124
#define AUDIO_CLK_AUDDIV_2	0x0128
#define AUDIO_CLK_AUDDIV_3	0x012C
#define AUDIO_CLK_AUDDIV_4	0x0134

#define AFE_BASE_END_OFFSET	8

void mtk_afe_pll_set_reg(struct mtk_afe_info *afe_info, uint32_t offset,
			 uint32_t value, uint32_t mask);
void mtk_afe_topck_set_reg(struct mtk_afe_info *afe_info, uint32_t offset,
			   uint32_t value, uint32_t mask);
static inline uint32_t mtk_afe_get_reg(struct mtk_afe_info *afe_info,
				       uint32_t offset)
{
	return readl(afe_info->base_addr + offset);
}

/* FIXME: only debug use */
static inline uint32_t mtk_afe_topck_get_reg(struct mtk_afe_info *afe_info,
					     uint32_t offset)
{
	return readl(afe_info->topckgen_base_addr + offset);
}

/* FIXME: only debug use */
static inline uint32_t mtk_afe_pll_get_reg(struct mtk_afe_info *afe_info,
					   uint32_t offset)
{
	return readl(afe_info->apmixedsys_base_addr + offset);
}

int mtk_afe_platform_init(struct platform_device *pdev);
int mtk_afe_clk_on(struct mtk_afe_info *afe_info);
int mtk_afe_clk_off(struct mtk_afe_info *afe_info);
#endif
