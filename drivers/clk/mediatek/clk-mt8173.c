/*
 * Copyright (c) 2014 MediaTek Inc.
 * Author: James Liao <jamesjj.liao@mediatek.com>
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

#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/slab.h>
#include <linux/mfd/syscon.h>

#include "clk-mtk.h"
#include "clk-gate.h"
#include "clk-cpumux.h"
#include "clk-mt8173-pg.h"

#include <dt-bindings/clock/mt8173-clk.h>

static DEFINE_SPINLOCK(lock);

static const struct mtk_fixed_factor root_clk_alias[] __initconst = {
	FACTOR(TOP_CLKPH_MCK_O, "clkph_mck_o", "clk_null", 1, 1),
	FACTOR(TOP_DPI_CK, "dpi_ck", "clk_null", 1, 1),
	FACTOR(TOP_USB_SYSPLL_125M, "usb_syspll_125m", "clk_null", 1, 1),
	FACTOR(TOP_HDMITX_DIG_CTS, "hdmitx_dig_cts", "clk_null", 1, 1),
};

static const struct mtk_fixed_factor top_divs[] __initconst = {
	FACTOR(TOP_ARMCA7PLL_754M, "armca7pll_754m", "armca7pll", 1, 2),
	FACTOR(TOP_ARMCA7PLL_502M, "armca7pll_502m", "armca7pll", 1, 3),

	FACTOR(TOP_MAIN_H546M, "main_h546m", "mainpll", 1, 2),
	FACTOR(TOP_MAIN_H364M, "main_h364m", "mainpll", 1, 3),
	FACTOR(TOP_MAIN_H218P4M, "main_h218p4m", "mainpll", 1, 5),
	FACTOR(TOP_MAIN_H156M, "main_h156m", "mainpll", 1, 7),

	FACTOR(TOP_TVDPLL_445P5M, "tvdpll_445p5m", "tvdpll", 1, 4),
	FACTOR(TOP_TVDPLL_594M, "tvdpll_594m", "tvdpll", 1, 3),

	FACTOR(TOP_UNIV_624M, "univ_624m", "univpll", 1, 2),
	FACTOR(TOP_UNIV_416M, "univ_416m", "univpll", 1, 3),
	FACTOR(TOP_UNIV_249P6M, "univ_249p6m", "univpll", 1, 5),
	FACTOR(TOP_UNIV_178P3M, "univ_178p3m", "univpll", 1, 7),
	FACTOR(TOP_UNIV_48M, "univ_48m", "univpll", 1, 26),

	FACTOR(TOP_CLKRTC_EXT, "clkrtc_ext", "clk32k", 1, 1),
	FACTOR(TOP_CLKRTC_INT, "clkrtc_int", "clk26m", 1, 793),
	FACTOR(TOP_FPC_CK, "fpc_ck", "clk26m", 1, 1),

	FACTOR(TOP_HDMITXPLL_D2, "hdmitxpll_d2", "hdmitx_dig_cts", 1, 2),
	FACTOR(TOP_HDMITXPLL_D3, "hdmitxpll_d3", "hdmitx_dig_cts", 1, 3),

	FACTOR(TOP_ARMCA7PLL_D2, "armca7pll_d2", "armca7pll_754m", 1, 1),
	FACTOR(TOP_ARMCA7PLL_D3, "armca7pll_d3", "armca7pll_502m", 1, 1),

	FACTOR(TOP_APLL1_CK, "apll1_ck", "apll1", 1, 1),
	FACTOR(TOP_APLL2_CK, "apll2_ck", "apll2", 1, 1),

	FACTOR(TOP_DMPLL_CK, "dmpll_ck", "clkph_mck_o", 1, 1),
	FACTOR(TOP_DMPLL_D2, "dmpll_d2", "clkph_mck_o", 1, 2),
	FACTOR(TOP_DMPLL_D4, "dmpll_d4", "clkph_mck_o", 1, 4),
	FACTOR(TOP_DMPLL_D8, "dmpll_d8", "clkph_mck_o", 1, 8),
	FACTOR(TOP_DMPLL_D16, "dmpll_d16", "clkph_mck_o", 1, 16),

	FACTOR(TOP_LVDSPLL_D2, "lvdspll_d2", "lvdspll", 1, 2),
	FACTOR(TOP_LVDSPLL_D4, "lvdspll_d4", "lvdspll", 1, 4),
	FACTOR(TOP_LVDSPLL_D8, "lvdspll_d8", "lvdspll", 1, 8),

	FACTOR(TOP_MMPLL_CK, "mmpll_ck", "mmpll", 1, 1),
	FACTOR(TOP_MMPLL_D2, "mmpll_d2", "mmpll", 1, 2),

	FACTOR(TOP_MSDCPLL_CK, "msdcpll_ck", "msdcpll", 1, 1),
	FACTOR(TOP_MSDCPLL_D2, "msdcpll_d2", "msdcpll", 1, 2),
	FACTOR(TOP_MSDCPLL_D4, "msdcpll_d4", "msdcpll", 1, 4),
	FACTOR(TOP_MSDCPLL2_CK, "msdcpll2_ck", "msdcpll2", 1, 1),
	FACTOR(TOP_MSDCPLL2_D2, "msdcpll2_d2", "msdcpll2", 1, 2),
	FACTOR(TOP_MSDCPLL2_D4, "msdcpll2_d4", "msdcpll2", 1, 4),

	FACTOR(TOP_SYSPLL_D2, "syspll_d2", "main_h546m", 1, 1),
	FACTOR(TOP_SYSPLL1_D2, "syspll1_d2", "main_h546m", 1, 2),
	FACTOR(TOP_SYSPLL1_D4, "syspll1_d4", "main_h546m", 1, 4),
	FACTOR(TOP_SYSPLL1_D8, "syspll1_d8", "main_h546m", 1, 8),
	FACTOR(TOP_SYSPLL1_D16, "syspll1_d16", "main_h546m", 1, 16),
	FACTOR(TOP_SYSPLL_D3, "syspll_d3", "main_h364m", 1, 1),
	FACTOR(TOP_SYSPLL2_D2, "syspll2_d2", "main_h364m", 1, 2),
	FACTOR(TOP_SYSPLL2_D4, "syspll2_d4", "main_h364m", 1, 4),
	FACTOR(TOP_SYSPLL_D5, "syspll_d5", "main_h218p4m", 1, 1),
	FACTOR(TOP_SYSPLL3_D2, "syspll3_d2", "main_h218p4m", 1, 2),
	FACTOR(TOP_SYSPLL3_D4, "syspll3_d4", "main_h218p4m", 1, 4),
	FACTOR(TOP_SYSPLL_D7, "syspll_d7", "main_h156m", 1, 1),
	FACTOR(TOP_SYSPLL4_D2, "syspll4_d2", "main_h156m", 1, 2),
	FACTOR(TOP_SYSPLL4_D4, "syspll4_d4", "main_h156m", 1, 4),

	FACTOR(TOP_TVDPLL_CK, "tvdpll_ck", "tvdpll_594m", 1, 1),
	FACTOR(TOP_TVDPLL_D2, "tvdpll_d2", "tvdpll_594m", 1, 2),
	FACTOR(TOP_TVDPLL_D4, "tvdpll_d4", "tvdpll_594m", 1, 4),
	FACTOR(TOP_TVDPLL_D8, "tvdpll_d8", "tvdpll_594m", 1, 8),
	FACTOR(TOP_TVDPLL_D16, "tvdpll_d16", "tvdpll_594m", 1, 16),

	FACTOR(TOP_UNIVPLL_D2, "univpll_d2", "univ_624m", 1, 1),
	FACTOR(TOP_UNIVPLL1_D2, "univpll1_d2", "univ_624m", 1, 2),
	FACTOR(TOP_UNIVPLL1_D4, "univpll1_d4", "univ_624m", 1, 4),
	FACTOR(TOP_UNIVPLL1_D8, "univpll1_d8", "univ_624m", 1, 8),
	FACTOR(TOP_UNIVPLL_D3, "univpll_d3", "univ_416m", 1, 1),
	FACTOR(TOP_UNIVPLL2_D2, "univpll2_d2", "univ_416m", 1, 2),
	FACTOR(TOP_UNIVPLL2_D4, "univpll2_d4", "univ_416m", 1, 4),
	FACTOR(TOP_UNIVPLL2_D8, "univpll2_d8", "univ_416m", 1, 8),
	FACTOR(TOP_UNIVPLL_D5, "univpll_d5", "univ_249p6m", 1, 1),
	FACTOR(TOP_UNIVPLL3_D2, "univpll3_d2", "univ_249p6m", 1, 2),
	FACTOR(TOP_UNIVPLL3_D4, "univpll3_d4", "univ_249p6m", 1, 4),
	FACTOR(TOP_UNIVPLL3_D8, "univpll3_d8", "univ_249p6m", 1, 8),
	FACTOR(TOP_UNIVPLL_D7, "univpll_d7", "univ_178p3m", 1, 1),
	FACTOR(TOP_UNIVPLL_D26, "univpll_d26", "univ_48m", 1, 1),
	FACTOR(TOP_UNIVPLL_D52, "univpll_d52", "univ_48m", 1, 2),

	FACTOR(TOP_VCODECPLL_CK, "vcodecpll_ck", "vcodecpll", 1, 3),
	FACTOR(TOP_VCODECPLL_370P5, "vcodecpll_370p5", "vcodecpll", 1, 4),

	FACTOR(TOP_VENCPLL_CK, "vencpll_ck", "vencpll", 1, 1),
	FACTOR(TOP_VENCPLL_D2, "vencpll_d2", "vencpll", 1, 2),
	FACTOR(TOP_VENCPLL_D4, "vencpll_d4", "vencpll", 1, 4),
};

static const char * const axi_parents[] __initconst = {
	"clk26m",
	"syspll1_d2",
	"syspll_d5",
	"syspll1_d4",
	"univpll_d5",
	"univpll2_d2",
	"dmpll_d2",
	"dmpll_d4"
};

static const char * const mem_parents[] __initconst = {
	"clk26m",
	"dmpll_ck"
};

static const char * const ddrphycfg_parents[] __initconst = {
	"clk26m",
	"syspll1_d8"
};

static const char * const mm_parents[] __initconst = {
	"clk26m",
	"vencpll_d2",
	"main_h364m",
	"syspll1_d2",
	"syspll_d5",
	"syspll1_d4",
	"univpll1_d2",
	"univpll2_d2",
	"dmpll_d2"
};

static const char * const pwm_parents[] __initconst = {
	"clk26m",
	"univpll2_d4",
	"univpll3_d2",
	"univpll1_d4"
};

static const char * const vdec_parents[] __initconst = {
	"clk26m",
	"vcodecpll_ck",
	"tvdpll_445p5m",
	"univpll_d3",
	"vencpll_d2",
	"syspll_d3",
	"univpll1_d2",
	"mmpll_d2",
	"dmpll_d2",
	"dmpll_d4"
};

static const char * const venc_parents[] __initconst = {
	"clk26m",
	"vcodecpll_ck",
	"tvdpll_445p5m",
	"univpll_d3",
	"vencpll_d2",
	"syspll_d3",
	"univpll1_d2",
	"univpll2_d2",
	"dmpll_d2",
	"dmpll_d4"
};

static const char * const mfg_parents[] __initconst = {
	"clk26m",
	"mmpll_ck",
	"dmpll_ck",
	"clk26m",
	"clk26m",
	"clk26m",
	"clk26m",
	"clk26m",
	"clk26m",
	"syspll_d3",
	"syspll1_d2",
	"syspll_d5",
	"univpll_d3",
	"univpll1_d2",
	"univpll_d5",
	"univpll2_d2"
};

static const char * const camtg_parents[] __initconst = {
	"clk26m",
	"univpll_d26",
	"univpll2_d2",
	"syspll3_d2",
	"syspll3_d4",
	"univpll1_d4"
};

static const char * const uart_parents[] __initconst = {
	"clk26m",
	"univpll2_d8"
};

static const char * const spi_parents[] __initconst = {
	"clk26m",
	"syspll3_d2",
	"syspll1_d4",
	"syspll4_d2",
	"univpll3_d2",
	"univpll2_d4",
	"univpll1_d8"
};

static const char * const usb20_parents[] __initconst = {
	"clk26m",
	"univpll1_d8",
	"univpll3_d4"
};

static const char * const usb30_parents[] __initconst = {
	"clk26m",
	"univpll3_d2",
	"usb_syspll_125m",
	"univpll2_d4"
};

static const char * const msdc50_0_h_parents[] __initconst = {
	"clk26m",
	"syspll1_d2",
	"syspll2_d2",
	"syspll4_d2",
	"univpll_d5",
	"univpll1_d4"
};

static const char * const msdc50_0_parents[] __initconst = {
	"clk26m",
	"msdcpll_ck",
	"msdcpll_d2",
	"univpll1_d4",
	"syspll2_d2",
	"syspll_d7",
	"msdcpll_d4",
	"vencpll_d4",
	"tvdpll_ck",
	"univpll_d2",
	"univpll1_d2",
	"mmpll_ck",
	"msdcpll2_ck",
	"msdcpll2_d2",
	"msdcpll2_d4"
};

static const char * const msdc30_1_parents[] __initconst = {
	"clk26m",
	"univpll2_d2",
	"msdcpll_d4",
	"univpll1_d4",
	"syspll2_d2",
	"syspll_d7",
	"univpll_d7",
	"vencpll_d4"
};

static const char * const msdc30_2_parents[] __initconst = {
	"clk26m",
	"univpll2_d2",
	"msdcpll_d4",
	"univpll1_d4",
	"syspll2_d2",
	"syspll_d7",
	"univpll_d7",
	"vencpll_d2"
};

static const char * const msdc30_3_parents[] __initconst = {
	"clk26m",
	"msdcpll2_ck",
	"msdcpll2_d2",
	"univpll2_d2",
	"msdcpll2_d4",
	"msdcpll_d4",
	"univpll1_d4",
	"syspll2_d2",
	"syspll_d7",
	"univpll_d7",
	"vencpll_d4",
	"msdcpll_ck",
	"msdcpll_d2",
	"msdcpll_d4"
};

static const char * const audio_parents[] __initconst = {
	"clk26m",
	"syspll3_d4",
	"syspll4_d4",
	"syspll1_d16"
};

static const char * const aud_intbus_parents[] __initconst = {
	"clk26m",
	"syspll1_d4",
	"syspll4_d2",
	"univpll3_d2",
	"univpll2_d8",
	"dmpll_d4",
	"dmpll_d8"
};

static const char * const pmicspi_parents[] __initconst = {
	"clk26m",
	"syspll1_d8",
	"syspll3_d4",
	"syspll1_d16",
	"univpll3_d4",
	"univpll_d26",
	"dmpll_d8",
	"dmpll_d16"
};

static const char * const scp_parents[] __initconst = {
	"clk26m",
	"syspll1_d2",
	"univpll_d5",
	"syspll_d5",
	"dmpll_d2",
	"dmpll_d4"
};

static const char * const atb_parents[] __initconst = {
	"clk26m",
	"syspll1_d2",
	"univpll_d5",
	"dmpll_d2"
};

static const char * const venc_lt_parents[] __initconst = {
	"clk26m",
	"univpll_d3",
	"vcodecpll_ck",
	"tvdpll_445p5m",
	"vencpll_d2",
	"syspll_d3",
	"univpll1_d2",
	"univpll2_d2",
	"syspll1_d2",
	"univpll_d5",
	"vcodecpll_370p5",
	"dmpll_ck"
};

static const char * const dpi0_parents[] __initconst = {
	"clk26m",
	"tvdpll_d2",
	"tvdpll_d4",
	"clk26m",
	"clk26m",
	"tvdpll_d8",
	"tvdpll_d16"
};

static const char * const irda_parents[] __initconst = {
	"clk26m",
	"univpll2_d4",
	"syspll2_d4"
};

static const char * const cci400_parents[] __initconst = {
	"clk26m",
	"vencpll_ck",
	"armca7pll_754m",
	"armca7pll_502m",
	"univpll_d2",
	"syspll_d2",
	"msdcpll_ck",
	"dmpll_ck"
};

static const char * const aud_1_parents[] __initconst = {
	"clk26m",
	"apll1_ck",
	"univpll2_d4",
	"univpll2_d8"
};

static const char * const aud_2_parents[] __initconst = {
	"clk26m",
	"apll2_ck",
	"univpll2_d4",
	"univpll2_d8"
};

static const char * const mem_mfg_in_parents[] __initconst = {
	"clk26m",
	"mmpll_ck",
	"dmpll_ck",
	"clk26m"
};

static const char * const axi_mfg_in_parents[] __initconst = {
	"clk26m",
	"axi_sel",
	"dmpll_d2"
};

static const char * const scam_parents[] __initconst = {
	"clk26m",
	"syspll3_d2",
	"univpll2_d4",
	"dmpll_d4"
};

static const char * const spinfi_ifr_parents[] __initconst = {
	"clk26m",
	"univpll2_d8",
	"univpll3_d4",
	"syspll4_d2",
	"univpll2_d4",
	"univpll3_d2",
	"syspll1_d4",
	"univpll1_d4"
};

static const char * const hdmi_parents[] __initconst = {
	"clk26m",
	"hdmitx_dig_cts",
	"hdmitxpll_d2",
	"hdmitxpll_d3"
};

static const char * const dpilvds_parents[] __initconst = {
	"clk26m",
	"lvdspll",
	"lvdspll_d2",
	"lvdspll_d4",
	"lvdspll_d8",
	"fpc_ck"
};

static const char * const msdc50_2_h_parents[] __initconst = {
	"clk26m",
	"syspll1_d2",
	"syspll2_d2",
	"syspll4_d2",
	"univpll_d5",
	"univpll1_d4"
};

static const char * const hdcp_parents[] __initconst = {
	"clk26m",
	"syspll4_d2",
	"syspll3_d4",
	"univpll2_d4"
};

static const char * const hdcp_24m_parents[] __initconst = {
	"clk26m",
	"univpll_d26",
	"univpll_d52",
	"univpll2_d8"
};

static const char * const rtc_parents[] __initconst = {
	"clkrtc_int",
	"clkrtc_ext",
	"clk26m",
	"univpll3_d8"
};

static const char * const i2s0_m_ck_parents[] __initconst = {
	"apll1_div1",
	"apll2_div1"
};

static const char * const i2s1_m_ck_parents[] __initconst = {
	"apll1_div2",
	"apll2_div2"
};

static const char * const i2s2_m_ck_parents[] __initconst = {
	"apll1_div3",
	"apll2_div3"
};

static const char * const i2s3_m_ck_parents[] __initconst = {
	"apll1_div4",
	"apll2_div4"
};

static const char * const i2s3_b_ck_parents[] __initconst = {
	"apll1_div5",
	"apll2_div5"
};

static const char * const ca53_parents[] __initconst = {
	"clk26m",
	"armca7pll",
	"mainpll",
	"univpll"
};

static const char * const ca57_parents[] __initconst = {
	"clk26m",
	"armca15pll",
	"mainpll",
	"univpll"
};

static const struct mtk_composite top_muxes[] __initconst = {
	/* CLK_CFG_0 */
	MUX(TOP_AXI_SEL, "axi_sel", axi_parents, 0x0040, 0, 3),
	MUX(TOP_MEM_SEL, "mem_sel", mem_parents, 0x0040, 8, 1),
	MUX_GATE(TOP_DDRPHYCFG_SEL, "ddrphycfg_sel", ddrphycfg_parents, 0x0040, 16, 1, 23),
	MUX_GATE(TOP_MM_SEL, "mm_sel", mm_parents, 0x0040, 24, 4, 31),
	/* CLK_CFG_1 */
	MUX_GATE(TOP_PWM_SEL, "pwm_sel", pwm_parents, 0x0050, 0, 2, 7),
	MUX_GATE(TOP_VDEC_SEL, "vdec_sel", vdec_parents, 0x0050, 8, 4, 15),
	MUX_GATE(TOP_VENC_SEL, "venc_sel", venc_parents, 0x0050, 16, 4, 23),
	MUX_GATE(TOP_MFG_SEL, "mfg_sel", mfg_parents, 0x0050, 24, 4, 31),
	/* CLK_CFG_2 */
	MUX_GATE(TOP_CAMTG_SEL, "camtg_sel", camtg_parents, 0x0060, 0, 3, 7),
	MUX_GATE(TOP_UART_SEL, "uart_sel", uart_parents, 0x0060, 8, 1, 15),
	MUX_GATE(TOP_SPI_SEL, "spi_sel", spi_parents, 0x0060, 16, 3, 23),
	MUX_GATE(TOP_USB20_SEL, "usb20_sel", usb20_parents, 0x0060, 24, 2, 31),
	/* CLK_CFG_3 */
	MUX_GATE(TOP_USB30_SEL, "usb30_sel", usb30_parents, 0x0070, 0, 2, 7),
	MUX_GATE(TOP_MSDC50_0_H_SEL, "msdc50_0_h_sel", msdc50_0_h_parents, 0x0070, 8, 3, 15),
	MUX_GATE(TOP_MSDC50_0_SEL, "msdc50_0_sel", msdc50_0_parents, 0x0070, 16, 4, 23),
	MUX_GATE(TOP_MSDC30_1_SEL, "msdc30_1_sel", msdc30_1_parents, 0x0070, 24, 3, 31),
	/* CLK_CFG_4 */
	MUX_GATE(TOP_MSDC30_2_SEL, "msdc30_2_sel", msdc30_2_parents, 0x0080, 0, 3, 7),
	MUX_GATE(TOP_MSDC30_3_SEL, "msdc30_3_sel", msdc30_3_parents, 0x0080, 8, 4, 15),
	MUX_GATE(TOP_AUDIO_SEL, "audio_sel", audio_parents, 0x0080, 16, 2, 23),
	MUX_GATE(TOP_AUD_INTBUS_SEL, "aud_intbus_sel", aud_intbus_parents, 0x0080, 24, 3, 31),
	/* CLK_CFG_5 */
	MUX_GATE(TOP_PMICSPI_SEL, "pmicspi_sel", pmicspi_parents, 0x0090, 0, 3, 7 /* 7:5 */),
	MUX_GATE(TOP_SCP_SEL, "scp_sel", scp_parents, 0x0090, 8, 3, 15),
	MUX_GATE(TOP_ATB_SEL, "atb_sel", atb_parents, 0x0090, 16, 2, 23),
	MUX_GATE(TOP_VENC_LT_SEL, "venclt_sel", venc_lt_parents, 0x0090, 24, 4, 31),
	/* CLK_CFG_6 */
	MUX_GATE(TOP_DPI0_SEL, "dpi0_sel", dpi0_parents, 0x00a0, 0, 3, 7),
	MUX_GATE(TOP_IRDA_SEL, "irda_sel", irda_parents, 0x00a0, 8, 2, 15),
	MUX_GATE(TOP_CCI400_SEL, "cci400_sel", cci400_parents, 0x00a0, 16, 3, 23),
	MUX_GATE(TOP_AUD_1_SEL, "aud_1_sel", aud_1_parents, 0x00a0, 24, 2, 31),
	/* CLK_CFG_7 */
	MUX_GATE(TOP_AUD_2_SEL, "aud_2_sel", aud_2_parents, 0x00b0, 0, 2, 7),
	MUX_GATE(TOP_MEM_MFG_IN_SEL, "mem_mfg_in_sel", mem_mfg_in_parents, 0x00b0, 8, 2, 15),
	MUX_GATE(TOP_AXI_MFG_IN_SEL, "axi_mfg_in_sel", axi_mfg_in_parents, 0x00b0, 16, 2, 23),
	MUX_GATE(TOP_SCAM_SEL, "scam_sel", scam_parents, 0x00b0, 24, 2, 31),
	/* CLK_CFG_12 */
	MUX_GATE(TOP_SPINFI_IFR_SEL, "spinfi_ifr_sel", spinfi_ifr_parents, 0x00c0, 0, 3, 7),
	MUX_GATE(TOP_HDMI_SEL, "hdmi_sel", hdmi_parents, 0x00c0, 8, 2, 15),
	MUX_GATE(TOP_DPILVDS_SEL, "dpilvds_sel", dpilvds_parents, 0x00c0, 24, 3, 31),
	/* CLK_CFG_13 */
	MUX_GATE(TOP_MSDC50_2_H_SEL, "msdc50_2_h_sel", msdc50_2_h_parents, 0x00d0, 0, 3, 7),
	MUX_GATE(TOP_HDCP_SEL, "hdcp_sel", hdcp_parents, 0x00d0, 8, 2, 15),
	MUX_GATE(TOP_HDCP_24M_SEL, "hdcp_24m_sel", hdcp_24m_parents, 0x00d0, 16, 2, 23),
	MUX(TOP_RTC_SEL, "rtc_sel", rtc_parents, 0x00d0, 24, 2),

	DIV_GATE(TOP_APLL1_DIV0, "apll1_div0", "aud_1_sel", 0x12c, 8, 0x120, 4, 24),
	DIV_GATE(TOP_APLL1_DIV1, "apll1_div1", "aud_1_sel", 0x12c, 9, 0x124, 8, 0),
	DIV_GATE(TOP_APLL1_DIV2, "apll1_div2", "aud_1_sel", 0x12c, 10, 0x124, 8, 8),
	DIV_GATE(TOP_APLL1_DIV3, "apll1_div3", "aud_1_sel", 0x12c, 11, 0x124, 8, 16),
	DIV_GATE(TOP_APLL1_DIV4, "apll1_div4", "aud_1_sel", 0x12c, 12, 0x124, 8, 24),
	DIV_GATE(TOP_APLL1_DIV5, "apll1_div5", "aud_1_sel", 0x12c, 13, 0x12c, 4, 0),

	DIV_GATE(TOP_APLL2_DIV0, "apll2_div0", "aud_2_sel", 0x12c, 16, 0x120, 4, 28),
	DIV_GATE(TOP_APLL2_DIV1, "apll2_div1", "aud_2_sel", 0x12c, 17, 0x128, 8, 0),
	DIV_GATE(TOP_APLL2_DIV2, "apll2_div2", "aud_2_sel", 0x12c, 18, 0x128, 8, 8),
	DIV_GATE(TOP_APLL2_DIV3, "apll2_div3", "aud_2_sel", 0x12c, 19, 0x128, 8, 16),
	DIV_GATE(TOP_APLL2_DIV4, "apll2_div4", "aud_2_sel", 0x12c, 20, 0x128, 8, 24),
	DIV_GATE(TOP_APLL2_DIV5, "apll2_div5", "aud_2_sel", 0x12c, 21, 0x12c, 4, 4),

	MUX(TOP_I2S0_M_CK_SEL, "i2s0_m_ck_sel", i2s0_m_ck_parents, 0x120, 4, 1),
	MUX(TOP_I2S1_M_CK_SEL, "i2s1_m_ck_sel", i2s1_m_ck_parents, 0x120, 5, 1),
	MUX(TOP_I2S2_M_CK_SEL, "i2s2_m_ck_sel", i2s2_m_ck_parents, 0x120, 6, 1),
	MUX(TOP_I2S3_M_CK_SEL, "i2s3_m_ck_sel", i2s3_m_ck_parents, 0x120, 7, 1),
	MUX(TOP_I2S3_B_CK_SEL, "i2s3_b_ck_sel", i2s3_b_ck_parents, 0x120, 8, 1),
};

static const struct mtk_composite cpu_muxes[] __initconst = {
	MUX(INFRA_CA53SEL, "infra_ca53_sel", ca53_parents, 0x0000, 0, 2),
	MUX(INFRA_CA57SEL, "infra_ca57_sel", ca57_parents, 0x0000, 2, 2),
};

static void __init mtk_init_clk_topckgen(void __iomem *top_base,
		struct clk_onecell_data *clk_data)
{
	int i;
	struct clk *clk;

	for (i = 0; i < ARRAY_SIZE(top_muxes); i++) {
		const struct mtk_composite *mux = &top_muxes[i];

		clk = mtk_clk_register_composite(mux, top_base, &lock);

		if (IS_ERR(clk)) {
			pr_err("Failed to register clk %s: %ld\n",
					mux->name, PTR_ERR(clk));
			continue;
		}

		if (clk_data)
			clk_data->clks[mux->id] = clk;
	}
}

static const struct mtk_gate_regs infra_cg_regs = {
	.set_ofs = 0x0040,
	.clr_ofs = 0x0044,
	.sta_ofs = 0x0048,
};

#define GATE_ICG(_id, _name, _parent, _shift) {	\
		.id = _id,					\
		.name = _name,					\
		.parent_name = _parent,				\
		.regs = &infra_cg_regs,				\
		.shift = _shift,				\
		.ops = &mtk_clk_gate_ops_setclr,		\
	}

static const struct mtk_gate infra_clks[] __initconst = {
	GATE_ICG(INFRA_DBGCLK, "infra_dbgclk", "axi_sel", 0),
	GATE_ICG(INFRA_SMI, "infra_smi", "mm_sel", 1),
	GATE_ICG(INFRA_AUDIO, "infra_audio", "aud_intbus_sel", 5),
	GATE_ICG(INFRA_GCE, "infra_gce", "axi_sel", 6),
	GATE_ICG(INFRA_L2C_SRAM, "infra_l2c_sram", "axi_sel", 7),
	GATE_ICG(INFRA_M4U, "infra_m4u", "mem_sel", 8),
	GATE_ICG(INFRA_CPUM, "infra_cpum", "clk_null", 15),
	GATE_ICG(INFRA_KP, "infra_kp", "axi_sel", 16),
	GATE_ICG(INFRA_CEC, "infra_cec", "clk26m", 18),
	GATE_ICG(INFRA_PMICSPI, "infra_pmicspi", "pmicspi_sel", 22),
	GATE_ICG(INFRA_PMICWRAP, "infra_pmicwrap", "axi_sel", 23),
};

static const struct mtk_gate_regs peri0_cg_regs = {
	.set_ofs = 0x0008,
	.clr_ofs = 0x0010,
	.sta_ofs = 0x0018,
};

static const struct mtk_gate_regs peri1_cg_regs = {
	.set_ofs = 0x000c,
	.clr_ofs = 0x0014,
	.sta_ofs = 0x001c,
};

#define GATE_PERI0(_id, _name, _parent, _shift) {	\
		.id = _id,					\
		.name = _name,					\
		.parent_name = _parent,				\
		.regs = &peri0_cg_regs,				\
		.shift = _shift,				\
		.ops = &mtk_clk_gate_ops_setclr,		\
	}

#define GATE_PERI1(_id, _name, _parent, _shift) {	\
		.id = _id,					\
		.name = _name,					\
		.parent_name = _parent,				\
		.regs = &peri1_cg_regs,				\
		.shift = _shift,				\
		.ops = &mtk_clk_gate_ops_setclr,		\
	}

static const struct mtk_gate peri_clks[] __initconst = {
	/* PERI0 */
	GATE_PERI0(PERI_NFI, "peri_nfi", "axi_sel", 0),
	GATE_PERI0(PERI_THERM, "peri_therm", "axi_sel", 1),
	GATE_PERI0(PERI_PWM1, "peri_pwm1", "axi_sel", 2),
	GATE_PERI0(PERI_PWM2, "peri_pwm2", "axi_sel", 3),
	GATE_PERI0(PERI_PWM3, "peri_pwm3", "axi_sel", 4),
	GATE_PERI0(PERI_PWM4, "peri_pwm4", "axi_sel", 5),
	GATE_PERI0(PERI_PWM5, "peri_pwm5", "axi_sel", 6),
	GATE_PERI0(PERI_PWM6, "peri_pwm6", "axi_sel", 7),
	GATE_PERI0(PERI_PWM7, "peri_pwm7", "axi_sel", 8),
	GATE_PERI0(PERI_PWM, "peri_pwm", "axi_sel", 9),
	GATE_PERI0(PERI_USB0, "peri_usb0", "usb20_sel", 10),
	GATE_PERI0(PERI_USB1, "peri_usb1", "usb20_sel", 11),
	GATE_PERI0(PERI_AP_DMA, "peri_ap_dma", "axi_sel", 12),
	GATE_PERI0(PERI_MSDC30_0, "peri_msdc30_0", "msdc50_0_sel", 13),
	GATE_PERI0(PERI_MSDC30_1, "peri_msdc30_1", "msdc30_1_sel", 14),
	GATE_PERI0(PERI_MSDC30_2, "peri_msdc30_2", "msdc30_2_sel", 15),
	GATE_PERI0(PERI_MSDC30_3, "peri_msdc30_3", "msdc30_3_sel", 16),
	GATE_PERI0(PERI_NLI_ARB, "peri_nli_arb", "axi_sel", 17),
	GATE_PERI0(PERI_IRDA, "peri_irda", "irda_sel", 18),
	GATE_PERI0(PERI_UART0, "peri_uart0", "uart_sel", 19),
	GATE_PERI0(PERI_UART1, "peri_uart1", "uart_sel", 20),
	GATE_PERI0(PERI_UART2, "peri_uart2", "uart_sel", 21),
	GATE_PERI0(PERI_UART3, "peri_uart3", "uart_sel", 22),
	GATE_PERI0(PERI_I2C0, "peri_i2c0", "axi_sel", 23),
	GATE_PERI0(PERI_I2C1, "peri_i2c1", "axi_sel", 24),
	GATE_PERI0(PERI_I2C2, "peri_i2c2", "axi_sel", 25),
	GATE_PERI0(PERI_I2C3, "peri_i2c3", "axi_sel", 26),
	GATE_PERI0(PERI_I2C4, "peri_i2c4", "axi_sel", 27),
	GATE_PERI0(PERI_AUXADC, "peri_auxadc", "clk26m", 28),
	GATE_PERI0(PERI_SPI0, "peri_spi0", "spi_sel", 29),
	GATE_PERI0(PERI_I2C5, "peri_i2c5", "axi_sel", 30),
	GATE_PERI0(PERI_NFIECC, "peri_nfiecc", "axi_sel", 31),
	/* PERI1 */
	GATE_PERI1(PERI_SPI, "peri_spi", "spi_sel", 0),
	GATE_PERI1(PERI_IRRX, "peri_irrx", "spi_sel", 1),
	GATE_PERI1(PERI_I2C6, "peri_i2c6", "axi_sel", 2),
};

static struct mtk_gate_regs mfg_cg_regs = {
	.set_ofs = 0x0004,
	.clr_ofs = 0x0008,
	.sta_ofs = 0x0000,
};

#define GATE_MFG(_id, _name, _parent, _shift) {	\
		.id = _id,					\
		.name = _name,					\
		.parent_name = _parent,				\
		.regs = &mfg_cg_regs,				\
		.shift = _shift,				\
		.ops = &mtk_clk_gate_ops_setclr,			\
	}

static struct mtk_gate mfg_clks[] __initdata = {
	GATE_MFG(MFG_AXI, "mfg_axi", "axi_sel", 0),
	GATE_MFG(MFG_MEM, "mfg_mem", "mem_sel", 1),
	GATE_MFG(MFG_G3D, "mfg_g3d", "mfg_sel", 2),
	GATE_MFG(MFG_26M, "mfg_26m", "clk26m", 3),
};

static struct mtk_gate_regs img_cg_regs = {
	.set_ofs = 0x0004,
	.clr_ofs = 0x0008,
	.sta_ofs = 0x0000,
};

#define GATE_IMG(_id, _name, _parent, _shift) {	\
		.id = _id,					\
		.name = _name,					\
		.parent_name = _parent,				\
		.regs = &img_cg_regs,				\
		.shift = _shift,				\
		.ops = &mtk_clk_gate_ops_setclr,			\
	}

static struct mtk_gate img_clks[] __initdata = {
	GATE_IMG(IMG_LARB2_SMI, "img_larb2_smi", "mm_sel", 0),
	GATE_IMG(IMG_CAM_SMI, "img_cam_smi", "mm_sel", 5),
	GATE_IMG(IMG_CAM_CAM, "img_cam_cam", "mm_sel", 6),
	GATE_IMG(IMG_SEN_TG, "img_sen_tg", "camtg_sel", 7),
	GATE_IMG(IMG_SEN_CAM, "img_sen_cam", "mm_sel", 8),
	GATE_IMG(IMG_CAM_SV, "img_cam_sv", "mm_sel", 9),
	GATE_IMG(IMG_FD, "img_fd", "mm_sel", 11),
};

static struct mtk_gate_regs mm0_cg_regs = {
	.set_ofs = 0x0104,
	.clr_ofs = 0x0108,
	.sta_ofs = 0x0100,
};

static struct mtk_gate_regs mm1_cg_regs = {
	.set_ofs = 0x0114,
	.clr_ofs = 0x0118,
	.sta_ofs = 0x0110,
};

#define GATE_MM0(_id, _name, _parent, _shift) {	\
		.id = _id,					\
		.name = _name,					\
		.parent_name = _parent,				\
		.regs = &mm0_cg_regs,				\
		.shift = _shift,				\
		.ops = &mtk_clk_gate_ops_setclr,			\
	}

#define GATE_MM1(_id, _name, _parent, _shift) {	\
		.id = _id,					\
		.name = _name,					\
		.parent_name = _parent,				\
		.regs = &mm1_cg_regs,				\
		.shift = _shift,				\
		.ops = &mtk_clk_gate_ops_setclr,			\
	}

static struct mtk_gate mm_clks[] __initdata = {
	/* MM0 */
	GATE_MM0(MM_SMI_COMMON, "mm_smi_common", "mm_sel", 0),
	GATE_MM0(MM_SMI_LARB0, "mm_smi_larb0", "mm_sel", 1),
	GATE_MM0(MM_CAM_MDP, "mm_cam_mdp", "mm_sel", 2),
	GATE_MM0(MM_MDP_RDMA0, "mm_mdp_rdma0", "mm_sel", 3),
	GATE_MM0(MM_MDP_RDMA1, "mm_mdp_rdma1", "mm_sel", 4),
	GATE_MM0(MM_MDP_RSZ0, "mm_mdp_rsz0", "mm_sel", 5),
	GATE_MM0(MM_MDP_RSZ1, "mm_mdp_rsz1", "mm_sel", 6),
	GATE_MM0(MM_MDP_RSZ2, "mm_mdp_rsz2", "mm_sel", 7),
	GATE_MM0(MM_MDP_TDSHP0, "mm_mdp_tdshp0", "mm_sel", 8),
	GATE_MM0(MM_MDP_TDSHP1, "mm_mdp_tdshp1", "mm_sel", 9),
	GATE_MM0(MM_MDP_WDMA, "mm_mdp_wdma", "mm_sel", 11),
	GATE_MM0(MM_MDP_WROT0, "mm_mdp_wrot0", "mm_sel", 12),
	GATE_MM0(MM_MDP_WROT1, "mm_mdp_wrot1", "mm_sel", 13),
	GATE_MM0(MM_FAKE_ENG, "mm_fake_eng", "mm_sel", 14),
	GATE_MM0(MM_MUTEX_32K, "mm_mutex_32k", "rtc_sel", 15),
	GATE_MM0(MM_DISP_OVL0, "mm_disp_ovl0", "mm_sel", 16),
	GATE_MM0(MM_DISP_OVL1, "mm_disp_ovl1", "mm_sel", 17),
	GATE_MM0(MM_DISP_RDMA0, "mm_disp_rdma0", "mm_sel", 18),
	GATE_MM0(MM_DISP_RDMA1, "mm_disp_rdma1", "mm_sel", 19),
	GATE_MM0(MM_DISP_RDMA2, "mm_disp_rdma2", "mm_sel", 20),
	GATE_MM0(MM_DISP_WDMA0, "mm_disp_wdma0", "mm_sel", 21),
	GATE_MM0(MM_DISP_WDMA1, "mm_disp_wdma1", "mm_sel", 22),
	GATE_MM0(MM_DISP_COLOR0, "mm_disp_color0", "mm_sel", 23),
	GATE_MM0(MM_DISP_COLOR1, "mm_disp_color1", "mm_sel", 24),
	GATE_MM0(MM_DISP_AAL, "mm_disp_aal", "mm_sel", 25),
	GATE_MM0(MM_DISP_GAMMA, "mm_disp_gamma", "mm_sel", 26),
	GATE_MM0(MM_DISP_UFOE, "mm_disp_ufoe", "mm_sel", 27),
	GATE_MM0(MM_DISP_SPLIT0, "mm_disp_split0", "mm_sel", 28),
	GATE_MM0(MM_DISP_SPLIT1, "mm_disp_split1", "mm_sel", 29),
	GATE_MM0(MM_DISP_MERGE, "mm_disp_merge", "mm_sel", 30),
	GATE_MM0(MM_DISP_OD, "mm_disp_od", "mm_sel", 31),
	/* MM1 */
	GATE_MM1(MM_DISP_PWM0MM, "mm_disp_pwm0mm", "mm_sel", 0),
	GATE_MM1(MM_DISP_PWM026M, "mm_disp_pwm026m", "clk26m", 1),
	GATE_MM1(MM_DISP_PWM1MM, "mm_disp_pwm1mm", "mm_sel", 2),
	GATE_MM1(MM_DISP_PWM126M, "mm_disp_pwm126m", "clk26m", 3),
	GATE_MM1(MM_DSI0_ENGINE, "mm_dsi0_engine", "mm_sel", 4),
	GATE_MM1(MM_DSI0_DIGITAL, "mm_dsi0_digital", "clk_null", 5),
	GATE_MM1(MM_DSI1_ENGINE, "mm_dsi1_engine", "mm_sel", 6),
	GATE_MM1(MM_DSI1_DIGITAL, "mm_dsi1_digital", "clk_null", 7),
	GATE_MM1(MM_DPI_PIXEL, "mm_dpi_pixel", "dpi0_sel", 8),
	GATE_MM1(MM_DPI_ENGINE, "mm_dpi_engine", "mm_sel", 9),
	GATE_MM1(MM_DPI1_PIXEL, "mm_dpi1_pixel", "clk_null", 10),
	GATE_MM1(MM_DPI1_ENGINE, "mm_dpi1_engine", "mm_sel", 11),
	GATE_MM1(MM_HDMI_PIXEL, "mm_hdmi_pixel", "dpi0_sel", 12),
	GATE_MM1(MM_HDMI_PLLCK, "mm_hdmi_pllck", "hdmi_sel", 13),
	GATE_MM1(MM_HDMI_AUDIO, "mm_hdmi_audio", "apll1", 14),
	GATE_MM1(MM_HDMI_SPDIF, "mm_hdmi_spdif", "apll2", 15),
	GATE_MM1(MM_LVDS_PIXEL, "mm_lvds_pixel", "clk_null", 16),
	GATE_MM1(MM_LVDS_CTS, "mm_lvds_cts", "clk_null", 17),
	GATE_MM1(MM_SMI_LARB4, "mm_smi_larb4", "mm_sel", 18),
	GATE_MM1(MM_HDMI_HDCP, "mm_hdmi_hdcp", "hdcp_sel", 19),
	GATE_MM1(MM_HDMI_HDCP24M, "mm_hdmi_hdcp24m", "hdcp_24m_sel", 20),
};

static struct mtk_gate_regs vdec0_cg_regs = {
	.set_ofs = 0x0000,
	.clr_ofs = 0x0004,
	.sta_ofs = 0x0000,
};

static struct mtk_gate_regs vdec1_cg_regs = {
	.set_ofs = 0x0008,
	.clr_ofs = 0x000c,
	.sta_ofs = 0x0008,
};

#define GATE_VDEC0(_id, _name, _parent, _shift) {	\
		.id = _id,					\
		.name = _name,					\
		.parent_name = _parent,				\
		.regs = &vdec0_cg_regs,				\
		.shift = _shift,				\
		.ops = &mtk_clk_gate_ops_setclr_inv,		\
	}

#define GATE_VDEC1(_id, _name, _parent, _shift) {	\
		.id = _id,					\
		.name = _name,					\
		.parent_name = _parent,				\
		.regs = &vdec1_cg_regs,				\
		.shift = _shift,				\
		.ops = &mtk_clk_gate_ops_setclr_inv,		\
	}

static struct mtk_gate vdec_clks[] __initdata = {
	GATE_VDEC0(VDEC_CKEN, "vdec_cken", "vdec_sel", 0),
	GATE_VDEC1(VDEC_LARB_CKEN, "vdec_larb_cken", "mm_sel", 0),
};

static struct mtk_gate_regs venc_cg_regs = {
	.set_ofs = 0x0004,
	.clr_ofs = 0x0008,
	.sta_ofs = 0x0000,
};

#define GATE_VENC(_id, _name, _parent, _shift) {	\
		.id = _id,					\
		.name = _name,					\
		.parent_name = _parent,				\
		.regs = &venc_cg_regs,				\
		.shift = _shift,				\
		.ops = &mtk_clk_gate_ops_setclr_inv,		\
	}

static struct mtk_gate venc_clks[] __initdata = {
	GATE_VENC(VENC_CKE0, "venc_cke0", "mm_sel", 0),
	GATE_VENC(VENC_CKE1, "venc_cke1", "venc_sel", 4),
	GATE_VENC(VENC_CKE2, "venc_cke2", "venc_sel", 8),
	GATE_VENC(VENC_CKE3, "venc_cke3", "venc_sel", 12),
};

static struct mtk_gate_regs venclt_cg_regs = {
	.set_ofs = 0x0004,
	.clr_ofs = 0x0008,
	.sta_ofs = 0x0000,
};

#define GATE_VENCLT(_id, _name, _parent, _shift) {	\
		.id = _id,					\
		.name = _name,					\
		.parent_name = _parent,				\
		.regs = &venclt_cg_regs,			\
		.shift = _shift,				\
		.ops = &mtk_clk_gate_ops_setclr_inv,		\
	}

static struct mtk_gate venclt_clks[] __initdata = {
	GATE_VENCLT(VENCLT_CKE0, "venclt_cke0", "mm_sel", 0),
	GATE_VENCLT(VENCLT_CKE1, "venclt_cke1", "venclt_sel", 4),
};

static struct mtk_gate_regs aud_cg_regs = {
	.set_ofs = 0x0000,
	.clr_ofs = 0x0000,
	.sta_ofs = 0x0000,
};

#define GATE_AUDIO(_id, _name, _parent, _shift, _ops) {	\
		.id = _id,					\
		.name = _name,					\
		.parent_name = _parent,				\
		.regs = &aud_cg_regs,				\
		.shift = _shift,				\
		.ops = _ops,		\
	}

static struct mtk_gate audio_clks[] __initdata = {
	GATE_AUDIO(AUD_AFE, "aud_afe", "audio_sel",
		2, &mtk_clk_gate_ops_no_setclr),
	GATE_AUDIO(AUD_I2S, "aud_i2s", "clk_null",
		6, &mtk_clk_gate_ops_no_setclr),
	GATE_AUDIO(AUD_22M, "aud_22m", "apll1_ck",
		8, &mtk_clk_gate_ops_no_setclr),
	GATE_AUDIO(AUD_24M, "aud_24m", "apll2_ck",
		9, &mtk_clk_gate_ops_no_setclr),
	GATE_AUDIO(AUD_SPDF2, "aud_spdf2", "clk_null",
		11, &mtk_clk_gate_ops_no_setclr),
	GATE_AUDIO(AUD_APLL2_TNR, "aud_apll2_tnr", "apll2_ck",
		18, &mtk_clk_gate_ops_no_setclr),
	GATE_AUDIO(AUD_APLL_TNR, "aud_apll_tnr", "apll1_ck",
		19, &mtk_clk_gate_ops_no_setclr),
	GATE_AUDIO(AUD_HDMI, "aud_hdmi", "clk_null",
		20, &mtk_clk_gate_ops_no_setclr),
	GATE_AUDIO(AUD_SPDF, "aud_spdf", "clk_null",
		21, &mtk_clk_gate_ops_no_setclr),
	GATE_AUDIO(AUD_ADDA3, "aud_adda3", "audio_sel",
		22, &mtk_clk_gate_ops_no_setclr),
	GATE_AUDIO(AUD_ADDA2, "aud_adda2", "audio_sel",
		23, &mtk_clk_gate_ops_no_setclr),
	GATE_AUDIO(AUD_ADC, "aud_adc", "audio_sel",
		24, &mtk_clk_gate_ops_no_setclr),
	GATE_AUDIO(AUD_DAC, "aud_dac", "audio_sel",
		25, &mtk_clk_gate_ops_no_setclr),
	GATE_AUDIO(AUD_DAC_PREDIS, "aud_dac_predis",
		"audio_sel", 26, &mtk_clk_gate_ops_no_setclr),
	GATE_AUDIO(AUD_TML, "aud_tml", "audio_sel",
		27, &mtk_clk_gate_ops_no_setclr),
	GATE_AUDIO(AUD_AHB_IDLE_EX, "aud_ahb_idle_ex", "axi_sel",
		29, &mtk_clk_gate_ops_no_setclr_inv),
	GATE_AUDIO(AUD_AHB_IDLE_IN, "aud_ahb_idle_in", "axi_sel",
		30, &mtk_clk_gate_ops_no_setclr_inv),
};

static const char* const mt8173_critical_topckgen_clocks[] __initconst = {
	"mem_sel",
	"ddrphycfg_sel",
	"cci400_sel",
	"rtc_sel",
	"axi_mfg_in_sel",
	"usb30_sel",
	"mem_mfg_in_sel",
};

static const char* const mt8173_critical_infra_clocks[] __initconst = {
	"infra_pmicwrap",
	"infra_pmicspi",
};

static const char* const mt8173_critical_peri_clocks[] __initconst = {
	"peri_uart0",
};

static void __init mtk_topckgen_init(struct device_node *node)
{
	struct clk_onecell_data *clk_data;
	void __iomem *base;
	int r;

	base = of_iomap(node, 0);
	if (!base) {
		pr_err("%s(): ioremap failed\n", __func__);
		return;
	}

	clk_data = mtk_alloc_clk_data(TOP_NR_CLK);

	mtk_clk_register_factors(root_clk_alias, ARRAY_SIZE(root_clk_alias), clk_data);
	mtk_clk_register_factors(top_divs, ARRAY_SIZE(top_divs), clk_data);
	mtk_init_clk_topckgen(base, clk_data);

	r = of_clk_add_provider(node, of_clk_src_onecell_get, clk_data);
	if (r)
		pr_err("%s(): could not register clock provider: %d\n",
			__func__, r);
}
CLK_OF_DECLARE(mtk_topckgen, "mediatek,mt8173-topckgen", mtk_topckgen_init);

static void __init mtk_infrasys_init(struct device_node *node)
{
	struct clk_onecell_data *clk_data;
	int r;

	clk_data = mtk_alloc_clk_data(INFRA_NR_CLK);

	mtk_clk_register_gates(node, infra_clks, ARRAY_SIZE(infra_clks),
						clk_data);

	mtk_clk_register_cpumuxes(node, cpu_muxes, ARRAY_SIZE(cpu_muxes),
						clk_data);

	r = of_clk_add_provider(node, of_clk_src_onecell_get, clk_data);
	if (r)
		pr_err("%s(): could not register clock provider: %d\n",
			__func__, r);

	mtk_register_reset_controller(node, 2, 0x30);
}
CLK_OF_DECLARE(mtk_infrasys, "mediatek,mt8173-infracfg", mtk_infrasys_init);

static void __init mtk_pericfg_init(struct device_node *node)
{
	struct clk_onecell_data *clk_data;
	int r;

	clk_data = mtk_alloc_clk_data(PERI_NR_CLK);

	mtk_clk_register_gates(node, peri_clks, ARRAY_SIZE(peri_clks),
						clk_data);

	r = of_clk_add_provider(node, of_clk_src_onecell_get, clk_data);
	if (r)
		pr_err("%s(): could not register clock provider: %d\n",
			__func__, r);

	mtk_register_reset_controller(node, 2, 0);
}
CLK_OF_DECLARE(mtk_pericfg, "mediatek,mt8173-pericfg", mtk_pericfg_init);

#define MT8173_PLL_FMAX		(3000UL * MHZ)

#define CON0_MT8173_RST_BAR	BIT(24)

#define PLL(_id, _name, _reg, _pwr_reg, _en_mask, _flags, _pcwbits, _pd_reg, _pd_shift, \
			_tuner_reg, _pcw_reg, _pcw_shift) { \
		.id = _id,						\
		.name = _name,						\
		.reg = _reg,						\
		.pwr_reg = _pwr_reg,					\
		.en_mask = _en_mask,					\
		.flags = _flags,					\
		.rst_bar_mask = CON0_MT8173_RST_BAR,			\
		.fmax = MT8173_PLL_FMAX,				\
		.pcwbits = _pcwbits,					\
		.pd_reg = _pd_reg,					\
		.pd_shift = _pd_shift,					\
		.tuner_reg = _tuner_reg,				\
		.pcw_reg = _pcw_reg,					\
		.pcw_shift = _pcw_shift,				\
	}

static const struct mtk_pll_data plls[] = {
	PLL(APMIXED_ARMCA15PLL, "armca15pll", 0x200, 0x20c, 0x00000001, 0, 21, 0x204, 24, 0x0, 0x204, 0),
	PLL(APMIXED_ARMCA7PLL, "armca7pll", 0x210, 0x21c, 0x00000001, 0, 21, 0x214, 24, 0x0, 0x214, 0),
	PLL(APMIXED_MAINPLL, "mainpll", 0x220, 0x22c, 0xf0000101, HAVE_RST_BAR, 21, 0x220, 4, 0x0, 0x224, 0),
	PLL(APMIXED_UNIVPLL, "univpll", 0x230, 0x23c, 0xfe000001, HAVE_RST_BAR, 7, 0x230, 4, 0x0, 0x234, 14),
	PLL(APMIXED_MMPLL, "mmpll", 0x240, 0x24c, 0x00000001, 0, 21, 0x244, 24, 0x0, 0x244, 0),
	PLL(APMIXED_MSDCPLL, "msdcpll", 0x250, 0x25c, 0x00000001, 0, 21, 0x250, 4, 0x0, 0x254, 0),
	PLL(APMIXED_VENCPLL, "vencpll", 0x260, 0x26c, 0x00000001, 0, 21, 0x260, 4, 0x0, 0x264, 0),
	PLL(APMIXED_TVDPLL, "tvdpll", 0x270, 0x27c, 0x00000001, 0, 21, 0x270, 4, 0x0, 0x274, 0),
	PLL(APMIXED_MPLL, "mpll", 0x280, 0x28c, 0x00000001, 0, 21, 0x280, 4, 0x0, 0x284, 0),
	PLL(APMIXED_VCODECPLL, "vcodecpll", 0x290, 0x29c, 0x00000001, 0, 21, 0x290, 4, 0x0, 0x294, 0),
	PLL(APMIXED_APLL1, "apll1", 0x2a0, 0x2b0, 0x00000001, 0, 31, 0x2a0, 4, 0x2a4, 0x2a4, 0),
	PLL(APMIXED_APLL2, "apll2", 0x2b4, 0x2c4, 0x00000001, 0, 31, 0x2b4, 4, 0x2b8, 0x2b8, 0),
	PLL(APMIXED_LVDSPLL, "lvdspll", 0x2d0, 0x2dc, 0x00000001, 0, 21, 0x2d0, 4, 0x0, 0x2d4, 0),
	PLL(APMIXED_MSDCPLL2, "msdcpll2", 0x2f0, 0x2fc, 0x00000001, 0, 21, 0x2f0, 4, 0x0, 0x2f4, 0),
};

static void __init mtk_apmixedsys_init(struct device_node *node)
{
	struct clk_onecell_data *clk_data;

	clk_data = mtk_alloc_clk_data(ARRAY_SIZE(plls));
	if (!clk_data)
		return;

	mtk_clk_register_plls(node, plls, ARRAY_SIZE(plls), clk_data);
}
CLK_OF_DECLARE(mtk_apmixedsys, "mediatek,mt8173-apmixedsys",
		mtk_apmixedsys_init);

static void __init mtk_mfgsys_init(struct device_node *node)
{
	struct clk_onecell_data *clk_data;
	void __iomem *base;
	int r;

	base = of_iomap(node, 0);
	if (!base) {
		pr_err("%s(): ioremap failed\n", __func__);
		return;
	}

	clk_data = mtk_alloc_clk_data(MFG_NR_CLK);

	mtk_clk_register_gates(node, mfg_clks, ARRAY_SIZE(mfg_clks),
						clk_data);

	r = of_clk_add_provider(node, of_clk_src_onecell_get, clk_data);
	if (r)
		pr_err("%s(): could not register clock provider: %d\n",
			__func__, r);
}
CLK_OF_DECLARE(mtk_mfgsys, "mediatek,mt8173-mfgsys", mtk_mfgsys_init);

static void __init mtk_imgsys_init(struct device_node *node)
{
	struct clk_onecell_data *clk_data;
	void __iomem *base;
	int r;

	base = of_iomap(node, 0);
	if (!base) {
		pr_err("%s(): ioremap failed\n", __func__);
		return;
	}

	clk_data = mtk_alloc_clk_data(IMG_NR_CLK);

	mtk_clk_register_gates(node, img_clks, ARRAY_SIZE(img_clks),
						clk_data);

	r = of_clk_add_provider(node, of_clk_src_onecell_get, clk_data);

	if (r)
		pr_err("%s(): could not register clock provider: %d\n",
			__func__, r);
}
CLK_OF_DECLARE(mtk_imgsys, "mediatek,mt8173-imgsys", mtk_imgsys_init);

static void __init mtk_mmsys_init(struct device_node *node)
{
	struct clk_onecell_data *clk_data;
	void __iomem *base;
	int r;

	base = of_iomap(node, 0);
	if (!base) {
		pr_err("%s(): ioremap failed\n", __func__);
		return;
	}

	clk_data = mtk_alloc_clk_data(MM_NR_CLK);

	mtk_clk_register_gates(node, mm_clks, ARRAY_SIZE(mm_clks),
						clk_data);

	r = of_clk_add_provider(node, of_clk_src_onecell_get, clk_data);
	if (r)
		pr_err("%s(): could not register clock provider: %d\n",
			__func__, r);
}
CLK_OF_DECLARE(mtk_mmsys, "mediatek,mt8173-mmsys", mtk_mmsys_init);

static void __init mtk_vdecsys_init(struct device_node *node)
{
	struct clk_onecell_data *clk_data;
	void __iomem *base;
	int r;

	base = of_iomap(node, 0);
	if (!base) {
		pr_err("%s(): ioremap failed\n", __func__);
		return;
	}

	clk_data = mtk_alloc_clk_data(VDEC_NR_CLK);

	mtk_clk_register_gates(node, vdec_clks, ARRAY_SIZE(vdec_clks),
						clk_data);

	r = of_clk_add_provider(node, of_clk_src_onecell_get, clk_data);
	if (r)
		pr_err("%s(): could not register clock provider: %d\n",
			__func__, r);
}
CLK_OF_DECLARE(mtk_vdecsys, "mediatek,mt8173-vdecsys", mtk_vdecsys_init);

static void __init mtk_vencsys_init(struct device_node *node)
{
	struct clk_onecell_data *clk_data;
	void __iomem *base;
	int r;

	base = of_iomap(node, 0);
	if (!base) {
		pr_err("%s(): ioremap failed\n", __func__);
		return;
	}

	clk_data = mtk_alloc_clk_data(VENC_NR_CLK);

	mtk_clk_register_gates(node, venc_clks, ARRAY_SIZE(venc_clks),
						clk_data);

	r = of_clk_add_provider(node, of_clk_src_onecell_get, clk_data);
	if (r)
		pr_err("%s(): could not register clock provider: %d\n",
			__func__, r);
}
CLK_OF_DECLARE(mtk_vencsys, "mediatek,mt8173-vencsys", mtk_vencsys_init);

static void __init mtk_vencltsys_init(struct device_node *node)
{
	struct clk_onecell_data *clk_data;
	void __iomem *base;
	int r;

	base = of_iomap(node, 0);
	if (!base) {
		pr_err("%s(): ioremap failed\n", __func__);
		return;
	}

	clk_data = mtk_alloc_clk_data(VENCLT_NR_CLK);

	mtk_clk_register_gates(node, venclt_clks, ARRAY_SIZE(venclt_clks),
						clk_data);

	r = of_clk_add_provider(node, of_clk_src_onecell_get, clk_data);
	if (r)
		pr_err("%s(): could not register clock provider: %d\n",
			__func__, r);
}
CLK_OF_DECLARE(mtk_vencltsys, "mediatek,mt8173-vencltsys", mtk_vencltsys_init);

static void __init mtk_audiosys_init(struct device_node *node)
{
	struct clk_onecell_data *clk_data;
	void __iomem *base;
	int r;

	base = of_iomap(node, 0);
	if (!base) {
		pr_err("%s(): ioremap failed\n", __func__);
		return;
	}

	clk_data = mtk_alloc_clk_data(AUD_NR_CLK);

	mtk_clk_register_gates(node, audio_clks, ARRAY_SIZE(audio_clks),
						clk_data);

	r = of_clk_add_provider(node, of_clk_src_onecell_get, clk_data);
	if (r)
		pr_err("%s(): could not register clock provider: %d\n",
			__func__, r);
}
CLK_OF_DECLARE(mtk_audiosys, "mediatek,mt8173-audiosys", mtk_audiosys_init);

static void __init mt_scpsys_init(struct device_node *node)
{
	struct clk_onecell_data *clk_data;
	void __iomem *infracfg_reg;
	void __iomem *spm_reg;
	int r;

	infracfg_reg = of_iomap(node, 0);
	spm_reg = of_iomap(node, 1);

	if (!infracfg_reg || !spm_reg) {
		pr_err("clk-pg-mt8173: missing reg\n");
		return;
	}

	clk_data = mtk_alloc_clk_data(SCP_NR_CLK);

	init_clk_scpsys(infracfg_reg, spm_reg, clk_data, &lock);

	r = of_clk_add_provider(node, of_clk_src_onecell_get, clk_data);
	if (r)
		pr_err("%s(): could not register clock provider: %d\n",
			__func__, r);
}
CLK_OF_DECLARE(mtk_pg_regs, "mediatek,mt8173-scpsys", mt_scpsys_init);

static int __init clk_critical_init(void)
{
	init_clk_protect_critical(mt8173_critical_topckgen_clocks,
			ARRAY_SIZE(mt8173_critical_topckgen_clocks));

	init_clk_protect_critical(mt8173_critical_infra_clocks,
			ARRAY_SIZE(mt8173_critical_infra_clocks));

	init_clk_protect_critical(mt8173_critical_peri_clocks,
			ARRAY_SIZE(mt8173_critical_peri_clocks));

	return 0;
}

module_init(clk_critical_init);

