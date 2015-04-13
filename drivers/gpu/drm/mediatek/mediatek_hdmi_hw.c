/*
 * Copyright (c) 2014 MediaTek Inc.
 * Author: Jie Qiu <jie.qiu@mediatek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#include "mediatek_hdmi_hw.h"
#include "mediatek_hdmi_regs.h"
#include "mediatek_hdmi_ctrl.h"

#include <linux/delay.h>
#include <linux/hdmi.h>
#include <linux/io.h>

#define hdmi_grl_read(addr) readl(hdmi_context->grl_regs + addr)
#define hdmi_grl_write(addr, val) writel(val, hdmi_context->grl_regs + addr)
#define hdmi_grl_write_msk(addr, val, msk) \
	hdmi_grl_write((addr), \
	(hdmi_grl_read(addr) & (~(msk))) | ((val) & (msk)))

#define hdmi_pll_read(addr) readl(hdmi_context->pll_regs + addr)
#define hdmi_pll_write(addr, val) writel(val, hdmi_context->pll_regs + addr)
#define hdmi_pll_write_msk(addr, val, msk) \
	hdmi_pll_write((addr), \
	(hdmi_pll_read(addr) & (~(msk))) | ((val) & (msk)))

#define hdmi_sys_read(addr) readl(hdmi_context->sys_regs + addr)
#define hdmi_sys_write(addr, val) writel(val, hdmi_context->sys_regs + addr)
#define hdmi_sys_write_msk(addr, val, msk) \
	hdmi_sys_write((addr), \
	(hdmi_sys_read(addr) & (~(msk))) | ((val) & (msk)))

static u32 internal_read_reg(phys_addr_t addr)
{
	void __iomem *ptr = NULL;
	u32 val;

	ptr = ioremap(addr, 0x4);
	val = readl(ptr);
	iounmap(ptr);
	return val;
}

static void internal_write_reg(phys_addr_t addr, size_t val)
{
	void __iomem *ptr = NULL;

	ptr = ioremap(addr, 0x4);
	writel(val, ptr);
	iounmap(ptr);
}

#define internal_read(addr) internal_read_reg(addr)
#define internal_write(addr, val) internal_write_reg(addr, val)
#define internal_write_msk(addr, val, msk) \
	internal_write((addr), \
	(internal_read(addr) & (~(msk))) | ((val) & (msk)))

static const u8 PREDIV[3][4] = {
	{0x0, 0x0, 0x0, 0x0},	/* 27Mhz */
	{0x1, 0x1, 0x1, 0x1},	/* 74Mhz */
	{0x1, 0x1, 0x1, 0x1}	/* 148Mhz */
};

static const u8 TXDIV[3][4] = {
	{0x3, 0x3, 0x3, 0x2},	/* 27Mhz */
	{0x2, 0x1, 0x1, 0x1},	/* 74Mhz */
	{0x1, 0x0, 0x0, 0x0}	/* 148Mhz */
};

static const u8 FBKSEL[3][4] = {
	{0x1, 0x1, 0x1, 0x1},	/* 27Mhz */
	{0x1, 0x0, 0x1, 0x1},	/* 74Mhz */
	{0x1, 0x0, 0x1, 0x1}	/* 148Mhz */
};

static const u8 FBKDIV[3][4] = {
	{19, 24, 29, 19},	/* 27Mhz */
	{19, 24, 14, 19},	/* 74Mhz */
	{19, 24, 14, 19}	/* 148Mhz */
};

static const u8 DIVEN[3][4] = {
	{0x2, 0x1, 0x1, 0x2},	/* 27Mhz */
	{0x2, 0x2, 0x2, 0x2},	/* 74Mhz */
	{0x2, 0x2, 0x2, 0x2}	/* 148Mhz */
};

static const u8 HTPLLBP[3][4] = {
	{0xc, 0xc, 0x8, 0xc},	/* 27Mhz */
	{0xc, 0xf, 0xf, 0xc},	/* 74Mhz */
	{0xc, 0xf, 0xf, 0xc}	/* 148Mhz */
};

static const u8 HTPLLBC[3][4] = {
	{0x2, 0x3, 0x3, 0x2},	/* 27Mhz */
	{0x2, 0x3, 0x3, 0x2},	/* 74Mhz */
	{0x2, 0x3, 0x3, 0x2}	/* 148Mhz */
};

static const u8 HTPLLBR[3][4] = {
	{0x1, 0x1, 0x0, 0x1},	/* 27Mhz */
	{0x1, 0x2, 0x2, 0x1},	/* 74Mhz */
	{0x1, 0x2, 0x2, 0x1}	/* 148Mhz */
};

#define NCTS_BYTES          0x07
static const u8 HDMI_NCTS[7][9][NCTS_BYTES] = {
	{{0x00, 0x00, 0x69, 0x78, 0x00, 0x10, 0x00},
	 {0x00, 0x00, 0xd2, 0xf0, 0x00, 0x10, 0x00},
	 {0x00, 0x03, 0x37, 0xf9, 0x00, 0x2d, 0x80},
	 {0x00, 0x01, 0x22, 0x0a, 0x00, 0x10, 0x00},
	 {0x00, 0x06, 0x6f, 0xf3, 0x00, 0x2d, 0x80},
	 {0x00, 0x02, 0x44, 0x14, 0x00, 0x10, 0x00},
	 {0x00, 0x01, 0xA5, 0xe0, 0x00, 0x10, 0x00},
	 {0x00, 0x06, 0x6F, 0xF3, 0x00, 0x16, 0xC0},
	 {0x00, 0x03, 0x66, 0x1E, 0x00, 0x0C, 0x00}
	 },
	{{0x00, 0x00, 0x75, 0x30, 0x00, 0x18, 0x80},
	 {0x00, 0x00, 0xea, 0x60, 0x00, 0x18, 0x80},
	 {0x00, 0x03, 0x93, 0x87, 0x00, 0x45, 0xac},
	 {0x00, 0x01, 0x42, 0x44, 0x00, 0x18, 0x80},
	 {0x00, 0x03, 0x93, 0x87, 0x00, 0x22, 0xd6},
	 {0x00, 0x02, 0x84, 0x88, 0x00, 0x18, 0x80},
	 {0x00, 0x01, 0xd4, 0xc0, 0x00, 0x18, 0x80},
	 {0x00, 0x03, 0x93, 0x87, 0x00, 0x11, 0x6B},
	 {0x00, 0x03, 0xC6, 0xCC, 0x00, 0x12, 0x60}
	 },
	{{0x00, 0x00, 0x69, 0x78, 0x00, 0x18, 0x00},
	 {0x00, 0x00, 0xd2, 0xf0, 0x00, 0x18, 0x00},
	 {0x00, 0x02, 0x25, 0x51, 0x00, 0x2d, 0x80},
	 {0x00, 0x01, 0x22, 0x0a, 0x00, 0x18, 0x00},
	 {0x00, 0x02, 0x25, 0x51, 0x00, 0x16, 0xc0},
	 {0x00, 0x02, 0x44, 0x14, 0x00, 0x18, 0x00},
	 {0x00, 0x01, 0xA5, 0xe0, 0x00, 0x18, 0x00},
	 {0x00, 0x04, 0x4A, 0xA2, 0x00, 0x16, 0xC0},
	 {0x00, 0x03, 0xC6, 0xCC, 0x00, 0x14, 0x00}
	 },
	{{0x00, 0x00, 0x75, 0x30, 0x00, 0x31, 0x00},
	 {0x00, 0x00, 0xea, 0x60, 0x00, 0x31, 0x00},
	 {0x00, 0x03, 0x93, 0x87, 0x00, 0x8b, 0x58},
	 {0x00, 0x01, 0x42, 0x44, 0x00, 0x31, 0x00},
	 {0x00, 0x03, 0x93, 0x87, 0x00, 0x45, 0xac},
	 {0x00, 0x02, 0x84, 0x88, 0x00, 0x31, 0x00},
	 {0x00, 0x01, 0xd4, 0xc0, 0x00, 0x31, 0x00},
	 {0x00, 0x03, 0x93, 0x87, 0x00, 0x22, 0xD6},
	 {0x00, 0x03, 0xC6, 0xCC, 0x00, 0x24, 0xC0}
	 },
	{{0x00, 0x00, 0x69, 0x78, 0x00, 0x30, 0x00},
	 {0x00, 0x00, 0xd2, 0xf0, 0x00, 0x30, 0x00},
	 {0x00, 0x02, 0x25, 0x51, 0x00, 0x5b, 0x00},
	 {0x00, 0x01, 0x22, 0x0a, 0x00, 0x30, 0x00},
	 {0x00, 0x02, 0x25, 0x51, 0x00, 0x2d, 0x80},
	 {0x00, 0x02, 0x44, 0x14, 0x00, 0x30, 0x00},
	 {0x00, 0x01, 0xA5, 0xe0, 0x00, 0x30, 0x00},
	 {0x00, 0x04, 0x4A, 0xA2, 0x00, 0x2D, 0x80},
	 {0x00, 0x03, 0xC6, 0xCC, 0x00, 0x28, 0x80}
	 },
	{{0x00, 0x00, 0x75, 0x30, 0x00, 0x62, 0x00},
	 {0x00, 0x00, 0xea, 0x60, 0x00, 0x62, 0x00},
	 {0x00, 0x03, 0x93, 0x87, 0x01, 0x16, 0xb0},
	 {0x00, 0x01, 0x42, 0x44, 0x00, 0x62, 0x00},
	 {0x00, 0x03, 0x93, 0x87, 0x00, 0x8b, 0x58},
	 {0x00, 0x02, 0x84, 0x88, 0x00, 0x62, 0x00},
	 {0x00, 0x01, 0xd4, 0xc0, 0x00, 0x62, 0x00},
	 {0x00, 0x03, 0x93, 0x87, 0x00, 0x45, 0xAC},
	 {0x00, 0x03, 0xC6, 0xCC, 0x00, 0x49, 0x80}
	 },
	{{0x00, 0x00, 0x69, 0x78, 0x00, 0x60, 0x00},
	 {0x00, 0x00, 0xd2, 0xf0, 0x00, 0x60, 0x00},
	 {0x00, 0x02, 0x25, 0x51, 0x00, 0xb6, 0x00},
	 {0x00, 0x01, 0x22, 0x0a, 0x00, 0x60, 0x00},
	 {0x00, 0x02, 0x25, 0x51, 0x00, 0x5b, 0x00},
	 {0x00, 0x02, 0x44, 0x14, 0x00, 0x60, 0x00},
	 {0x00, 0x01, 0xA5, 0xe0, 0x00, 0x60, 0x00},
	 {0x00, 0x04, 0x4A, 0xA2, 0x00, 0x5B, 0x00},
	 {0x00, 0x03, 0xC6, 0xCC, 0x00, 0x50, 0x00}
	 }
};

void mtk_hdmi_hw_vid_black(struct mediatek_hdmi_context *hdmi_context,
			   bool black)
{
	if (black)
		hdmi_grl_write_msk(VIDEO_CFG_4, GEN_RGB, VIDEO_SOURCE_SEL);
	else
		hdmi_grl_write_msk(VIDEO_CFG_4, NORMAL_PATH, VIDEO_SOURCE_SEL);
}

void mtk_hdmi_hw_make_reg_writabe(struct mediatek_hdmi_context *hdmi_context,
				  bool enable)
{
	if (enable) {
		hdmi_sys_write_msk(HDMI_SYS_CFG20, HDMI_PCLK_FREE_RUN,
				   HDMI_PCLK_FREE_RUN);
		hdmi_sys_write_msk(HDMI_SYS_CFG1C, HDMI_ON | ANLG_ON,
				   HDMI_ON | ANLG_ON);
	} else {
		hdmi_sys_write_msk(HDMI_SYS_CFG20, 0, HDMI_PCLK_FREE_RUN);
		hdmi_sys_write_msk(HDMI_SYS_CFG1C, 0, HDMI_ON | ANLG_ON);
	}
}

void mtk_hdmi_hw_1p4_verseion_enable(struct mediatek_hdmi_context *hdmi_context,
				     bool enable)
{
	if (enable)
		hdmi_sys_write_msk(HDMI_SYS_CFG20, 0, HDMI2P0_EN);
	else
		hdmi_sys_write_msk(HDMI_SYS_CFG20, HDMI2P0_EN, HDMI2P0_EN);
}

bool mtk_hdmi_hw_is_hpd_high(struct mediatek_hdmi_context *hdmi_context)
{
	u8 status;

	status = hdmi_grl_read(GRL_STATUS);

	return (STATUS_HTPLG & status) == STATUS_HTPLG;
}

void mtk_hdmi_hw_aud_mute(struct mediatek_hdmi_context *hdmi_context, bool mute)
{
	u32 val = 0;

	val = hdmi_grl_read(GRL_AUDIO_CFG);
	if (mute)
		val |= AUDIO_ZERO;
	else
		val &= ~AUDIO_ZERO;
	hdmi_grl_write(GRL_AUDIO_CFG, val);
}

void mtk_hdmi_hw_reset(struct mediatek_hdmi_context *hdmi_context, bool reset)
{
	if (reset) {
		hdmi_sys_write_msk(HDMI_SYS_CFG1C, HDMI_RST, HDMI_RST);
	} else {
		hdmi_sys_write_msk(HDMI_SYS_CFG1C, 0, HDMI_RST);
		hdmi_grl_write_msk(GRL_CFG3, 0, CFG3_CONTROL_PACKET_DELAY);
		hdmi_sys_write_msk(HDMI_SYS_CFG1C, ANLG_ON, ANLG_ON);
	}
}

void mtk_hdmi_hw_enable_notice(struct mediatek_hdmi_context *hdmi_context,
			       bool enable_notice)
{
	u32 val = 0;

	val = hdmi_grl_read(GRL_CFG2);
	if (enable_notice)
		val |= CFG2_NOTICE_EN;
	else
		val &= ~CFG2_NOTICE_EN;
	hdmi_grl_write(GRL_CFG2, val);
}

void mtk_hdmi_hw_write_int_mask(struct mediatek_hdmi_context *hdmi_context,
				u32 int_mask)
{
	hdmi_grl_write(GRL_INT_MASK, int_mask);
}

void mtk_hdmi_hw_enable_dvi_mode(struct mediatek_hdmi_context *hdmi_context,
				 bool enable)
{
	u32 val = 0;

	val = hdmi_grl_read(GRL_CFG1);
	if (enable)
		val |= CFG1_DVI;
	else
		val &= ~CFG1_DVI;
	hdmi_grl_write(GRL_CFG1, val);
}

u32 mtk_hdmi_hw_get_int_type(struct mediatek_hdmi_context *hdmi_context)
{
	return hdmi_grl_read(GRL_INT);
}

void mtk_hdmi_hw_on_off_tmds(struct mediatek_hdmi_context *hdmi_context,
			     bool on)
{
	if (on) {
		hdmi_pll_write_msk(HDMI_CON1, RG_HDMITX_PLL_AUTOK_EN,
				   RG_HDMITX_PLL_AUTOK_EN);
		hdmi_pll_write_msk(HDMI_CON0, RG_HDMITX_PLL_POSDIV,
				   RG_HDMITX_PLL_POSDIV);
		hdmi_pll_write_msk(HDMI_CON3, 0, RG_HDMITX_MHLCK_EN);
		hdmi_pll_write_msk(HDMI_CON1, RG_HDMITX_PLL_BIAS_EN,
				   RG_HDMITX_PLL_BIAS_EN);
		usleep_range(100, 150);
		hdmi_pll_write_msk(HDMI_CON0, RG_HDMITX_PLL_EN,
				   RG_HDMITX_PLL_EN);
		usleep_range(100, 150);
		hdmi_pll_write_msk(HDMI_CON1, RG_HDMITX_PLL_BIAS_LPF_EN,
				   RG_HDMITX_PLL_BIAS_LPF_EN);
		hdmi_pll_write_msk(HDMI_CON1, RG_HDMITX_PLL_TXDIV_EN,
				   RG_HDMITX_PLL_TXDIV_EN);
		hdmi_pll_write_msk(HDMI_CON3, RG_HDMITX_SER_EN,
				   RG_HDMITX_SER_EN);
		hdmi_pll_write_msk(HDMI_CON3, RG_HDMITX_PRD_EN,
				   RG_HDMITX_PRD_EN);
		hdmi_pll_write_msk(HDMI_CON3, RG_HDMITX_DRV_EN,
				   RG_HDMITX_DRV_EN);
		usleep_range(100, 150);
	} else {
		hdmi_pll_write_msk(HDMI_CON3, 0, RG_HDMITX_DRV_EN);
		hdmi_pll_write_msk(HDMI_CON3, 0, RG_HDMITX_PRD_EN);
		hdmi_pll_write_msk(HDMI_CON3, 0, RG_HDMITX_SER_EN);
		hdmi_pll_write_msk(HDMI_CON1, 0, RG_HDMITX_PLL_TXDIV_EN);
		hdmi_pll_write_msk(HDMI_CON1, 0, RG_HDMITX_PLL_BIAS_LPF_EN);
		usleep_range(100, 150);
		hdmi_pll_write_msk(HDMI_CON0, 0, RG_HDMITX_PLL_EN);
		usleep_range(100, 150);
		hdmi_pll_write_msk(HDMI_CON1, 0, RG_HDMITX_PLL_BIAS_EN);
		hdmi_pll_write_msk(HDMI_CON0, 0, RG_HDMITX_PLL_POSDIV);
		hdmi_pll_write_msk(HDMI_CON1, 0, RG_HDMITX_PLL_AUTOK_EN);
		usleep_range(100, 150);
	}
}

void mtk_hdmi_hw_send_info_frame(struct mediatek_hdmi_context *hdmi_context,
				 u8 *buffer, u8 len)
{
	u32 val;
	u32 ctrl_reg = GRL_CTRL;
	int i;
	u8 *frame_data;
	u8 frame_type;
	u8 frame_ver;
	u8 frame_len;
	u8 checksum;
	int ctrl_frame_en = 0;

	frame_type = *buffer;
	buffer += 1;
	frame_ver = *buffer;
	buffer += 1;
	frame_len = *buffer;
	buffer += 1;
	checksum = *buffer;
	buffer += 1;
	frame_data = buffer;

	mtk_hdmi_info
	    ("frame_type:0x%x,frame_ver:0x%x,frame_len:0x%x,checksum:0x%x\n",
	     frame_type, frame_ver, frame_len, checksum);

	if (HDMI_INFOFRAME_TYPE_AVI == frame_type) {
		ctrl_frame_en = CTRL_AVI_EN;
		ctrl_reg = GRL_CTRL;
	} else if (HDMI_INFOFRAME_TYPE_SPD == frame_type) {
		ctrl_frame_en = CTRL_SPD_EN;
		ctrl_reg = GRL_CTRL;
	} else if (HDMI_INFOFRAME_TYPE_AUDIO == frame_type) {
		ctrl_frame_en = CTRL_AUDIO_EN;
		ctrl_reg = GRL_CTRL;
	} else if (HDMI_INFOFRAME_TYPE_VENDOR == frame_type) {
		ctrl_frame_en = VS_EN;
		ctrl_reg = GRL_ACP_ISRC_CTRL;
	}

	hdmi_grl_write_msk(ctrl_reg, 0, ctrl_frame_en);
	hdmi_grl_write(GRL_INFOFRM_TYPE, frame_type);
	hdmi_grl_write(GRL_INFOFRM_VER, frame_ver);
	hdmi_grl_write(GRL_INFOFRM_LNG, frame_len);

	hdmi_grl_write(GRL_IFM_PORT, checksum);
	for (i = 0; i < frame_len; i++)
		hdmi_grl_write(GRL_IFM_PORT, frame_data[i]);

	val = hdmi_grl_read(ctrl_reg);
	val |= ctrl_frame_en;
	hdmi_grl_write(ctrl_reg, val);
}

void mtk_hdmi_hw_send_aud_packet(struct mediatek_hdmi_context *hdmi_context,
				 bool enable)
{
	u32 val = 0;

	val = hdmi_grl_read(GRL_SHIFT_R2);
	if (enable)
		val &= ~AUDIO_PACKET_OFF;
	else
		val |= AUDIO_PACKET_OFF;
	hdmi_grl_write(GRL_SHIFT_R2, val);
}

void mtk_hdmi_hw_set_pll(struct mediatek_hdmi_context *hdmi_context,
			 enum mtk_hdmi_hw_ref_clk ref_clk,
			 enum HDMI_DISPLAY_COLOR_DEPTH depth)
{
	unsigned int v4valueclk = 0;
	unsigned int v4valued2 = 0;
	unsigned int v4valued1 = 0;
	unsigned int v4valued0 = 0;

	hdmi_pll_write_msk(HDMI_CON0,
			   ((PREDIV[ref_clk][depth]) << PREDIV_SHIFT),
			   RG_HDMITX_PLL_PREDIV);
	hdmi_pll_write_msk(HDMI_CON0, RG_HDMITX_PLL_POSDIV,
			   RG_HDMITX_PLL_POSDIV);
	hdmi_pll_write_msk(HDMI_CON0, (0x1 << PLL_IC_SHIFT), RG_HDMITX_PLL_IC);
	hdmi_pll_write_msk(HDMI_CON0, (0x1 << PLL_IR_SHIFT), RG_HDMITX_PLL_IR);
	hdmi_pll_write_msk(HDMI_CON1,
			   ((TXDIV[ref_clk][depth]) << PLL_TXDIV_SHIFT),
			   RG_HDMITX_PLL_TXDIV);
	hdmi_pll_write_msk(HDMI_CON0,
			   ((FBKSEL[ref_clk][depth]) << PLL_FBKSEL_SHIFT),
			   RG_HDMITX_PLL_FBKSEL);
	hdmi_pll_write_msk(HDMI_CON0,
			   ((FBKDIV[ref_clk][depth]) << PLL_FBKDIV_SHIFT),
			   RG_HDMITX_PLL_FBKDIV);
	hdmi_pll_write_msk(HDMI_CON1,
			   ((DIVEN[ref_clk][depth]) << PLL_DIVEN_SHIFT),
			   RG_HDMITX_PLL_DIVEN);
	hdmi_pll_write_msk(HDMI_CON0,
			   ((HTPLLBP[ref_clk][depth]) << PLL_BP_SHIFT),
			   RG_HDMITX_PLL_BP);
	hdmi_pll_write_msk(HDMI_CON0,
			   ((HTPLLBC[ref_clk][depth]) << PLL_BC_SHIFT),
			   RG_HDMITX_PLL_BC);
	hdmi_pll_write_msk(HDMI_CON0,
			   ((HTPLLBR[ref_clk][depth]) << PLL_BR_SHIFT),
			   RG_HDMITX_PLL_BR);

	v4valueclk = internal_read(0x10206000 + 0x4c8);
	v4valueclk = v4valueclk >> 24;
	v4valued2 = internal_read(0x10206000 + 0x4c8);
	v4valued2 = (v4valued2 & 0x00ff0000) >> 16;
	v4valued1 = internal_read(0x10206000 + 0x530);
	v4valued1 = (v4valued1 & 0x00000fc0) >> 6;
	v4valued0 = internal_read(0x10206000 + 0x530);
	v4valued0 = v4valued0 & 0x3f;

	if ((v4valueclk == 0) || (v4valued2 == 0) ||
	    (v4valued1 == 0) || (v4valued0 == 0)) {
		v4valueclk = 0x30;
		v4valued2 = 0x30;
		v4valued1 = 0x30;
		v4valued0 = 0x30;
	}

	if ((ref_clk == HDMI_REF_CLK_148MHZ) &&
	    (depth != HDMI_DEEP_COLOR_24BITS)) {
		hdmi_pll_write_msk(HDMI_CON3, RG_HDMITX_PRD_IMP_EN,
				   RG_HDMITX_PRD_IMP_EN);
		hdmi_pll_write_msk(HDMI_CON4, (0x6 << PRD_IBIAS_CLK_SHIFT),
				   RG_HDMITX_PRD_IBIAS_CLK);
		hdmi_pll_write_msk(HDMI_CON4, (0x6 << PRD_IBIAS_D2_SHIFT),
				   RG_HDMITX_PRD_IBIAS_D2);
		hdmi_pll_write_msk(HDMI_CON4, (0x6 << PRD_IBIAS_D1_SHIFT),
				   RG_HDMITX_PRD_IBIAS_D1);
		hdmi_pll_write_msk(HDMI_CON4, (0x6 << PRD_IBIAS_D0_SHIFT),
				   RG_HDMITX_PRD_IBIAS_D0);

		hdmi_pll_write_msk(HDMI_CON3, (0xf << DRV_IMP_EN_SHIFT),
				   RG_HDMITX_DRV_IMP_EN);
		hdmi_pll_write_msk(HDMI_CON6, (v4valueclk << DRV_IMP_CLK_SHIFT),
				   RG_HDMITX_DRV_IMP_CLK);
		hdmi_pll_write_msk(HDMI_CON6, (v4valued2 << DRV_IMP_D2_SHIFT),
				   RG_HDMITX_DRV_IMP_D2);
		hdmi_pll_write_msk(HDMI_CON6, (v4valued1 << DRV_IMP_D1_SHIFT),
				   RG_HDMITX_DRV_IMP_D1);
		hdmi_pll_write_msk(HDMI_CON6, (v4valued0 << DRV_IMP_D0_SHIFT),
				   RG_HDMITX_DRV_IMP_D0);

		hdmi_pll_write_msk(HDMI_CON5, (0x1c << DRV_IBIAS_CLK_SHIFT),
				   RG_HDMITX_DRV_IBIAS_CLK);
		hdmi_pll_write_msk(HDMI_CON5, (0x1c << DRV_IBIAS_D2_SHIFT),
				   RG_HDMITX_DRV_IBIAS_D2);
		hdmi_pll_write_msk(HDMI_CON5, (0x1c << DRV_IBIAS_D1_SHIFT),
				   RG_HDMITX_DRV_IBIAS_D1);
		hdmi_pll_write_msk(HDMI_CON5, (0x1c << DRV_IBIAS_D0_SHIFT),
				   RG_HDMITX_DRV_IBIAS_D0);
	} else {
		hdmi_pll_write_msk(HDMI_CON3, 0, RG_HDMITX_PRD_IMP_EN);
		hdmi_pll_write_msk(HDMI_CON4, (0x3 << PRD_IBIAS_CLK_SHIFT),
				   RG_HDMITX_PRD_IBIAS_CLK);
		hdmi_pll_write_msk(HDMI_CON4, (0x3 << PRD_IBIAS_D2_SHIFT),
				   RG_HDMITX_PRD_IBIAS_D2);
		hdmi_pll_write_msk(HDMI_CON4, (0x3 << PRD_IBIAS_D1_SHIFT),
				   RG_HDMITX_PRD_IBIAS_D1);
		hdmi_pll_write_msk(HDMI_CON4, (0x3 << PRD_IBIAS_D0_SHIFT),
				   RG_HDMITX_PRD_IBIAS_D0);

		hdmi_pll_write_msk(HDMI_CON3, (0x0 << DRV_IMP_EN_SHIFT),
				   RG_HDMITX_DRV_IMP_EN);
		hdmi_pll_write_msk(HDMI_CON6, (v4valueclk << DRV_IMP_CLK_SHIFT),
				   RG_HDMITX_DRV_IMP_CLK);
		hdmi_pll_write_msk(HDMI_CON6, (v4valued2 << DRV_IMP_D2_SHIFT),
				   RG_HDMITX_DRV_IMP_D2);
		hdmi_pll_write_msk(HDMI_CON6, (v4valued1 << DRV_IMP_D1_SHIFT),
				   RG_HDMITX_DRV_IMP_D1);
		hdmi_pll_write_msk(HDMI_CON6, (v4valued0 << DRV_IMP_D0_SHIFT),
				   RG_HDMITX_DRV_IMP_D0);

		hdmi_pll_write_msk(HDMI_CON5, (0xa << DRV_IBIAS_CLK_SHIFT),
				   RG_HDMITX_DRV_IBIAS_CLK);
		hdmi_pll_write_msk(HDMI_CON5, (0xa << DRV_IBIAS_D2_SHIFT),
				   RG_HDMITX_DRV_IBIAS_D2);
		hdmi_pll_write_msk(HDMI_CON5, (0xa << DRV_IBIAS_D1_SHIFT),
				   RG_HDMITX_DRV_IBIAS_D1);
		hdmi_pll_write_msk(HDMI_CON5, (0xa << DRV_IBIAS_D0_SHIFT),
				   RG_HDMITX_DRV_IBIAS_D0);
	}
}

bool mtk_hdmi_hw_config_sys(struct mediatek_hdmi_context *hdmi_context,
			    enum mtk_hdmi_hw_ref_clk ref_clk)
{
	if (clk_set_parent
	    (hdmi_context->hdmitx_clk_mux, hdmi_context->hdmitx_clk_div1)) {
		mtk_hdmi_err("set hdmitx_clk_mux parent failed!\n");
		return false;
	}

	if (HDMI_REF_CLK_148MHZ == ref_clk) {
		if (clk_set_parent
		    (hdmi_context->dpi_clk_mux, hdmi_context->dpi_clk_div4)) {
			mtk_hdmi_err("set dpi_clk_mux parent failed!\n");
			return false;
		}
	} else if (HDMI_REF_CLK_74MHZ == ref_clk) {
		if (clk_set_parent
		    (hdmi_context->dpi_clk_mux, hdmi_context->dpi_clk_div4)) {
			mtk_hdmi_err("set dpi_clk_mux parent failed!\n");
			return false;
		}
	} else if (HDMI_REF_CLK_27MHZ == ref_clk) {
		if (clk_set_parent
		    (hdmi_context->dpi_clk_mux, hdmi_context->dpi_clk_div8)) {
			mtk_hdmi_err("set dpi_clk_mux parent failed!\n");
			return false;
		}
	} else {
		mtk_hdmi_err("invalid argument!\n");
		return false;
	}

	if (clk_prepare_enable(hdmi_context->hdmi_dpi_clk_gate)) {
		mtk_hdmi_err("enable dpi_clk clk failed!\n");
		return false;
	}

	if (clk_prepare_enable(hdmi_context->hdmi_dpi_eng_clk_gate)) {
		mtk_hdmi_err("enable dpi_eng_clk clk failed!\n");
		return false;
	}

	if (clk_prepare_enable(hdmi_context->hdmi_id_clk_gate)) {
		mtk_hdmi_err("enable id_clk clk failed!\n");
		return false;
	}

	if (clk_prepare_enable(hdmi_context->hdmi_pll_clk_gate)) {
		mtk_hdmi_err("enable pll_clk clk failed!\n");
		return false;
	}

	if (hdmi_context->aud_input_type == HDMI_AUD_INPUT_SPDIF) {
		clk_disable_unprepare(hdmi_context->hdmi_aud_bclk_gate);
		if (clk_prepare_enable(hdmi_context->hdmi_aud_spdif_gate)) {
			mtk_hdmi_err
			    ("enable hdmi_aud_spdif_gate clk failed!\n");
			return false;
		}
	} else {
		if (clk_prepare_enable(hdmi_context->hdmi_aud_bclk_gate)) {
			mtk_hdmi_err("enable hdmi_aud_bclk_gate clk failed!\n");
			return false;
		}
		if (clk_prepare_enable(hdmi_context->hdmi_aud_spdif_gate)) {
			mtk_hdmi_err
			    ("enable hdmi_aud_spdif_gate clk failed!\n");
			return false;
		}
	}

	hdmi_sys_write_msk(HDMI_SYS_CFG20, 0, HDMI_OUT_FIFO_EN | MHL_MODE_ON);
	mdelay(2);
	hdmi_sys_write_msk(HDMI_SYS_CFG20, HDMI_OUT_FIFO_EN,
			   HDMI_OUT_FIFO_EN | MHL_MODE_ON);

	internal_write_msk(0x10209040, 0x3 << 16, 0x7 << 16);

	return true;
}

void mtk_hdmi_hw_set_deep_color_mode(struct mediatek_hdmi_context *hdmi_context,
				     enum HDMI_DISPLAY_COLOR_DEPTH depth)
{
	u32 val = 0;

	switch (depth) {
	case HDMI_DEEP_COLOR_24BITS:
		val = COLOR_8BIT_MODE;
		break;
	case HDMI_DEEP_COLOR_30BITS:
		val = COLOR_10BIT_MODE;
		break;
	case HDMI_DEEP_COLOR_36BITS:
		val = COLOR_12BIT_MODE;
		break;
	case HDMI_DEEP_COLOR_48BITS:
		val = COLOR_16BIT_MODE;
		break;
	default:
		val = COLOR_8BIT_MODE;
		break;
	}

	if (val == COLOR_8BIT_MODE) {
		hdmi_sys_write_msk(HDMI_SYS_CFG20, val,
				   DEEP_COLOR_MODE_MASK | DEEP_COLOR_EN);
	} else {
		hdmi_sys_write_msk(HDMI_SYS_CFG20, val | DEEP_COLOR_EN,
				   DEEP_COLOR_MODE_MASK | DEEP_COLOR_EN);
	}
}

void mtk_hdmi_hw_send_AV_MUTE(struct mediatek_hdmi_context *hdmi_context)
{
	u32 val;

	val = hdmi_grl_read(GRL_CFG4);
	val &= ~CTRL_AVMUTE;
	hdmi_grl_write(GRL_CFG4, val);

	mdelay(2);

	val |= CTRL_AVMUTE;
	hdmi_grl_write(GRL_CFG4, val);
}

void mtk_hdmi_hw_send_AV_UNMUTE(struct mediatek_hdmi_context *hdmi_context)
{
	u32 val;

	val = hdmi_grl_read(GRL_CFG4);
	val |= CFG4_AV_UNMUTE_EN;
	val &= ~CFG4_AV_UNMUTE_SET;
	hdmi_grl_write(GRL_CFG4, val);

	mdelay(2);

	val &= ~CFG4_AV_UNMUTE_EN;
	val |= CFG4_AV_UNMUTE_SET;
	hdmi_grl_write(GRL_CFG4, val);
}

void mtk_hdmi_hw_ncts_enable(struct mediatek_hdmi_context *hdmi_context,
			     bool on)
{
	u32 val;

	val = hdmi_grl_read(GRL_CTS_CTRL);
	if (on)
		val &= ~CTS_CTRL_SOFT;
	else
		val |= CTS_CTRL_SOFT;
	hdmi_grl_write(GRL_CTS_CTRL, val);
}

void mtk_hdmi_hw_ncts_auto_write_enable(struct mediatek_hdmi_context
					*hdmi_context, bool enable)
{
	u32 val = 0;

	val = hdmi_grl_read(GRL_DIVN);
	if (enable)
		val |= NCTS_WRI_ANYTIME;
	else
		val &= ~NCTS_WRI_ANYTIME;
	hdmi_grl_write(GRL_DIVN, val);
}

void mtk_hdmi_hw_msic_setting(struct mediatek_hdmi_context *hdmi_context,
			      struct drm_display_mode *mode)
{
	hdmi_grl_write_msk(GRL_CFG4, 0, CFG_MHL_MODE);

	if (mode->flags & DRM_MODE_FLAG_INTERLACE &&
	    mode->clock == 74250 &&
	    mode->vdisplay == 1080)
		hdmi_grl_write_msk(GRL_CFG2, 0, MHL_DE_SEL);
	else
		hdmi_grl_write_msk(GRL_CFG2, MHL_DE_SEL, MHL_DE_SEL);
}

void mtk_hdmi_hw_aud_set_channel_swap(struct mediatek_hdmi_context
				      *hdmi_context,
				      enum hdmi_aud_channel_swap_type swap)
{
	u8 swap_bit;

	switch (swap) {
	case HDMI_AUD_SWAP_LR:
		swap_bit = LR_SWAP;
		break;
	case HDMI_AUD_SWAP_LFE_CC:
		swap_bit = LFE_CC_SWAP;
		break;
	case HDMI_AUD_SWAP_LSRS:
		swap_bit = LSRS_SWAP;
		break;
	case HDMI_AUD_SWAP_RLS_RRS:
		swap_bit = RLS_RRS_SWAP;
		break;
	case HDMI_AUD_SWAP_LR_STATUS:
		swap_bit = LR_STATUS_SWAP;
		break;
	default:
		swap_bit = LFE_CC_SWAP;
		break;
	}
	hdmi_grl_write_msk(GRL_CH_SWAP, swap_bit, 0xff);
}

void mtk_hdmi_hw_aud_raw_data_enable(struct mediatek_hdmi_context *hdmi_context,
				     bool enable)
{
	u32 val = 0;

	val = hdmi_grl_read(GRL_MIX_CTRL);
	if (enable)
		val |= MIX_CTRL_FLAT;
	else
		val &= ~MIX_CTRL_FLAT;

	hdmi_grl_write(GRL_MIX_CTRL, val);
}

void mtk_hdmi_hw_aud_set_bit_num(struct mediatek_hdmi_context *hdmi_context,
				 enum hdmi_audio_sample_size bit_num)
{
	u32 val = 0;

	if (bit_num == HDMI_AUDIO_SAMPLE_SIZE_16)
		val = AOUT_16BIT;
	else if (bit_num == HDMI_AUDIO_SAMPLE_SIZE_20)
		val = AOUT_20BIT;
	else if (bit_num == HDMI_AUDIO_SAMPLE_SIZE_24)
		val = AOUT_24BIT;

	hdmi_grl_write_msk(GRL_AOUT_BNUM_SEL, val, 0x03);
}

void mtk_hdmi_hw_aud_set_i2s_fmt(struct mediatek_hdmi_context *hdmi_context,
				 enum hdmi_aud_i2s_fmt i2s_fmt)
{
	u32 val = 0;

	val = hdmi_grl_read(GRL_CFG0);
	val &= ~0x33;

	switch (i2s_fmt) {
	case HDMI_I2S_MODE_RJT_24BIT:
		val |= (CFG0_I2S_MODE_RTJ | CFG0_I2S_MODE_24BIT);
		break;
	case HDMI_I2S_MODE_RJT_16BIT:
		val |= (CFG0_I2S_MODE_RTJ | CFG0_I2S_MODE_16BIT);
		break;
	case HDMI_I2S_MODE_LJT_24BIT:
		val |= (CFG0_I2S_MODE_LTJ | CFG0_I2S_MODE_24BIT);
		break;
	case HDMI_I2S_MODE_LJT_16BIT:
		val |= (CFG0_I2S_MODE_LTJ | CFG0_I2S_MODE_16BIT);
		break;
	case HDMI_I2S_MODE_I2S_24BIT:
		val |= (CFG0_I2S_MODE_I2S | CFG0_I2S_MODE_24BIT);
		break;
	case HDMI_I2S_MODE_I2S_16BIT:
		val |= (CFG0_I2S_MODE_I2S | CFG0_I2S_MODE_16BIT);
		break;
	default:
		break;
	}
	hdmi_grl_write(GRL_CFG0, val);
}

void mtk_hdmi_hw_aud_set_high_bitrate(struct mediatek_hdmi_context
				      *hdmi_context, bool enable)
{
	u32 val = 0;

	if (enable) {
		val = hdmi_grl_read(GRL_AOUT_BNUM_SEL);
		val |= HIGH_BIT_RATE_PACKET_ALIGN;
		hdmi_grl_write(GRL_AOUT_BNUM_SEL, val);

		val = hdmi_grl_read(GRL_AUDIO_CFG);
		val |= HIGH_BIT_RATE;
		hdmi_grl_write(GRL_AUDIO_CFG, val);
	} else {
		val = hdmi_grl_read(GRL_AOUT_BNUM_SEL);
		val &= ~HIGH_BIT_RATE_PACKET_ALIGN;
		hdmi_grl_write(GRL_AOUT_BNUM_SEL, val);

		val = hdmi_grl_read(GRL_AUDIO_CFG);
		val &= ~HIGH_BIT_RATE;
		hdmi_grl_write(GRL_AUDIO_CFG, val);
	}
}

void mtk_hdmi_phy_aud_dst_normal_double_enable(struct mediatek_hdmi_context
					       *hdmi_context, bool enable)
{
	u32 val = 0;

	if (enable) {
		val = hdmi_grl_read(GRL_AUDIO_CFG);
		val |= DST_NORMAL_DOUBLE;
		hdmi_grl_write(GRL_AUDIO_CFG, val);
	} else {
		val = hdmi_grl_read(GRL_AUDIO_CFG);
		val &= ~DST_NORMAL_DOUBLE;
		hdmi_grl_write(GRL_AUDIO_CFG, val);
	}
}

void mtk_hdmi_hw_aud_dst_enable(struct mediatek_hdmi_context *hdmi_context,
				bool enable)
{
	u32 val = 0;

	if (enable) {
		val = hdmi_grl_read(GRL_AUDIO_CFG);
		val |= SACD_DST;
		hdmi_grl_write(GRL_AUDIO_CFG, val);
	} else {
		val = hdmi_grl_read(GRL_AUDIO_CFG);
		val &= ~SACD_DST;
		hdmi_grl_write(GRL_AUDIO_CFG, val);
	}
}

void mtk_hdmi_hw_aud_dsd_enable(struct mediatek_hdmi_context *hdmi_context,
				bool enable)
{
	u32 val = 0;

	if (!enable) {
		val = hdmi_grl_read(GRL_AUDIO_CFG);
		val &= ~SACD_SEL;
		hdmi_grl_write(GRL_AUDIO_CFG, val);
	}
}

void mtk_hdmi_hw_aud_set_i2s_chan_num(struct mediatek_hdmi_context
				      *hdmi_context,
				      enum hdmi_aud_channel_type channel_type,
				      u8 channel_count)
{
	u8 val_1, val_2, val_3, val_4;

	if (channel_count == 2) {
		val_1 = 0x04;
		val_2 = 0x50;
	} else if (channel_count == 3 || channel_count == 4) {
		if (channel_count == 4 &&
		    (channel_type == HDMI_AUD_CHAN_TYPE_3_0_LRS ||
		    channel_type == HDMI_AUD_CHAN_TYPE_4_0)) {
			val_1 = 0x14;
		} else {
			val_1 = 0x0c;
		}
		val_2 = 0x50;
	} else if (channel_count == 6 || channel_count == 5) {
		if (channel_count == 6 &&
		    channel_type != HDMI_AUD_CHAN_TYPE_5_1 &&
		    channel_type != HDMI_AUD_CHAN_TYPE_4_1_CLRS) {
			val_1 = 0x3c;
			val_2 = 0x50;
		} else {
			val_1 = 0x1c;
			val_2 = 0x50;
		}
	} else if (channel_count == 8 || channel_count == 7) {
		val_1 = 0x3c;
		val_2 = 0x50;
	} else {
		val_1 = 0x04;
		val_2 = 0x50;
	}

	val_3 = 0xc6;
	val_4 = 0xfa;

	hdmi_grl_write(GRL_CH_SW0, val_2);
	hdmi_grl_write(GRL_CH_SW1, val_3);
	hdmi_grl_write(GRL_CH_SW2, val_4);
	hdmi_grl_write(GRL_I2S_UV, val_1);
}

void mtk_hdmi_hw_aud_set_input_type(struct mediatek_hdmi_context *hdmi_context,
				    enum hdmi_aud_input_type input_type)
{
	u32 val = 0;

	val = hdmi_grl_read(GRL_CFG1);
	if (input_type == HDMI_AUD_INPUT_I2S &&
	    (val & CFG1_SPDIF) == CFG1_SPDIF) {
		val &= ~CFG1_SPDIF;
	} else if (input_type == HDMI_AUD_INPUT_SPDIF &&
		(val & CFG1_SPDIF) == 0) {
		val |= CFG1_SPDIF;
	}
	hdmi_grl_write(GRL_CFG1, val);
}

void mtk_hdmi_hw_aud_set_channel_status(struct mediatek_hdmi_context
					*hdmi_context, u8 *l_chan_status,
					u8 *r_chan_staus,
					enum hdmi_audio_sample_frequency
					aud_hdmi_fs)
{
	u8 l_status[5];
	u8 r_status[5];
	u8 val = 0;

	l_status[0] = l_chan_status[0];
	l_status[1] = l_chan_status[1];
	l_status[2] = l_chan_status[2];
	r_status[0] = r_chan_staus[0];
	r_status[1] = r_chan_staus[1];
	r_status[2] = r_chan_staus[2];

	l_status[0] &= ~0x02;
	r_status[0] &= ~0x02;

	val = l_chan_status[3] & 0xf0;
	switch (aud_hdmi_fs) {
	case HDMI_AUDIO_SAMPLE_FREQUENCY_32000:
		val |= 0x03;
		break;
	case HDMI_AUDIO_SAMPLE_FREQUENCY_44100:
		break;
	case HDMI_AUDIO_SAMPLE_FREQUENCY_88200:
		val |= 0x08;
		break;
	case HDMI_AUDIO_SAMPLE_FREQUENCY_96000:
		val |= 0x0a;
		break;
	case HDMI_AUDIO_SAMPLE_FREQUENCY_48000:
		val |= 0x02;
		break;
	default:
		val |= 0x02;
		break;
	}
	l_status[3] = val;
	r_status[3] = val;

	val = l_chan_status[4];
	val |= ((~(l_status[3] & 0x0f)) << 4);
	l_status[4] = val;
	r_status[4] = val;

	val = l_status[0];
	hdmi_grl_write(GRL_I2S_C_STA0, val);
	hdmi_grl_write(GRL_L_STATUS_0, val);

	val = r_status[0];
	hdmi_grl_write(GRL_R_STATUS_0, val);

	val = l_status[1];
	hdmi_grl_write(GRL_I2S_C_STA1, val);
	hdmi_grl_write(GRL_L_STATUS_1, val);

	val = r_status[1];
	hdmi_grl_write(GRL_R_STATUS_1, val);

	val = l_status[2];
	hdmi_grl_write(GRL_I2S_C_STA2, val);
	hdmi_grl_write(GRL_L_STATUS_2, val);

	val = r_status[2];
	hdmi_grl_write(GRL_R_STATUS_2, val);

	val = l_status[3];
	hdmi_grl_write(GRL_I2S_C_STA3, val);
	hdmi_grl_write(GRL_L_STATUS_3, val);

	val = r_status[3];
	hdmi_grl_write(GRL_R_STATUS_3, val);

	val = l_status[4];
	hdmi_grl_write(GRL_I2S_C_STA4, val);
	hdmi_grl_write(GRL_L_STATUS_4, val);

	val = r_status[4];
	hdmi_grl_write(GRL_R_STATUS_4, val);

	for (val = 0; val < 19; val++) {
		hdmi_grl_write(GRL_L_STATUS_5 + val * 4, 0);
		hdmi_grl_write(GRL_R_STATUS_5 + val * 4, 0);
	}
}

void mtk_hdmi_hw_aud_src_reenable(struct mediatek_hdmi_context *hdmi_context)
{
	u32 val;

	val = hdmi_grl_read(GRL_MIX_CTRL);
	if (val & MIX_CTRL_SRC_EN) {
		val &= ~MIX_CTRL_SRC_EN;
		hdmi_grl_write(GRL_MIX_CTRL, val);
		usleep_range(255, 512);
		val |= MIX_CTRL_SRC_EN;
		hdmi_grl_write(GRL_MIX_CTRL, val);
	}
}

void mtk_hdmi_hw_aud_src_off(struct mediatek_hdmi_context *hdmi_context)
{
	u32 val;

	val = hdmi_grl_read(GRL_MIX_CTRL);
	val &= ~MIX_CTRL_SRC_EN;
	hdmi_grl_write(GRL_MIX_CTRL, val);
	hdmi_grl_write(GRL_SHIFT_L1, 0x00);
}

void mtk_hdmi_hw_aud_set_mclk(struct mediatek_hdmi_context *hdmi_context,
			      enum hdmi_aud_mclk mclk)
{
	u32 val;

	val = hdmi_grl_read(GRL_CFG5);
	val &= CFG5_CD_RATIO_MASK;

	switch (mclk) {
	case HDMI_AUD_MCLK_128FS:
		val |= CFG5_FS128;
		break;
	case HDMI_AUD_MCLK_256FS:
		val |= CFG5_FS256;
		break;
	case HDMI_AUD_MCLK_384FS:
		val |= CFG5_FS384;
		break;
	case HDMI_AUD_MCLK_512FS:
		val |= CFG5_FS512;
		break;
	case HDMI_AUD_MCLK_768FS:
		val |= CFG5_FS768;
		break;
	default:
		val |= CFG5_FS256;
		break;
	}
	hdmi_grl_write(GRL_CFG5, val);
}

void mtk_hdmi_hw_aud_aclk_inv_enable(struct mediatek_hdmi_context *hdmi_context,
				     bool enable)
{
	u32 val;

	val = hdmi_grl_read(GRL_CFG2);
	if (enable)
		val |= 0x80;
	else
		val &= ~0x80;
	hdmi_grl_write(GRL_CFG2, val);
}

static void do_hdmi_hw_aud_set_ncts(struct mediatek_hdmi_context *hdmi_context,
				    enum HDMI_DISPLAY_COLOR_DEPTH depth,
				    enum hdmi_audio_sample_frequency freq,
				    int pix)
{
	unsigned char val[NCTS_BYTES];
	unsigned int temp;
	int i = 0;

	hdmi_grl_write(GRL_NCTS, 0);
	hdmi_grl_write(GRL_NCTS, 0);
	hdmi_grl_write(GRL_NCTS, 0);
	memset(val, 0, sizeof(val));

	if (depth == HDMI_DEEP_COLOR_24BITS) {
		for (i = 0; i < NCTS_BYTES; i++) {
			if ((freq < 8) && (pix < 9))
				val[i] = HDMI_NCTS[freq - 1][pix][i];
		}
		temp = (val[0] << 24) | (val[1] << 16) |
			(val[2] << 8) | (val[3]);	/* CTS */
	} else {
		for (i = 0; i < NCTS_BYTES; i++) {
			if ((freq < 7) && (pix < 9))
				val[i] = HDMI_NCTS[freq - 1][pix][i];
		}

		temp =
		    (val[0] << 24) | (val[1] << 16) | (val[2] << 8) | (val[3]);

		if (depth == HDMI_DEEP_COLOR_30BITS)
			temp = (temp >> 2) * 5;
		else if (depth == HDMI_DEEP_COLOR_36BITS)
			temp = (temp >> 1) * 3;
		else if (depth == HDMI_DEEP_COLOR_48BITS)
			temp = (temp << 1);

		val[0] = (temp >> 24) & 0xff;
		val[1] = (temp >> 16) & 0xff;
		val[2] = (temp >> 8) & 0xff;
		val[3] = (temp) & 0xff;
	}

	for (i = 0; i < NCTS_BYTES; i++)
		hdmi_grl_write(GRL_NCTS, val[i]);
}

void mtk_hdmi_hw_aud_set_ncts(struct mediatek_hdmi_context *hdmi_context,
			      enum HDMI_DISPLAY_COLOR_DEPTH depth,
			      enum hdmi_audio_sample_frequency freq, int clock)
{
	int pix = 0;

	switch (clock) {
	case 27000:
		pix = 0;
		break;
	case 74175:
		pix = 2;
		break;
	case 74250:
		pix = 3;
		break;
	case 148350:
		pix = 4;
		break;
	case 148500:
		pix = 5;
		break;
	default:
		pix = 0;
		break;
	}

	hdmi_grl_write_msk(DUMMY_304, AUDIO_I2S_NCTS_SEL_64,
			   AUDIO_I2S_NCTS_SEL);
	do_hdmi_hw_aud_set_ncts(hdmi_context, depth, freq, pix);
}
