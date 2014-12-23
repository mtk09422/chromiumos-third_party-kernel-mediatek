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

#include "clk-mtk.h"
#include "clk-pll.h"
#include "clk-gate.h"
#include "clk-mt8135-pll.h"

#include <dt-bindings/clock/mt8135-clk.h>

/*
 * platform clocks
 */

/* ROOT */
#define clk_null		"clk_null"
#define clk26m			"clk26m"
#define rtc32k			"rtc32k"

#define dsi0_lntc_dsiclk	"dsi0_lntc_dsi"
#define hdmitx_clkdig_cts	"hdmitx_dig_cts"
#define clkph_mck		"clkph_mck"
#define cpum_tck_in		"cpum_tck_in"

/* PLL */
#define armpll1			"armpll1"
#define armpll2			"armpll2"
#define mainpll			"mainpll"
#define univpll			"univpll"
#define mmpll			"mmpll"
#define msdcpll			"msdcpll"
#define tvdpll			"tvdpll"
#define lvdspll			"lvdspll"
#define audpll			"audpll"
#define vdecpll			"vdecpll"

#define mainpll_806m		"mainpll_806m"
#define mainpll_537p3m		"mainpll_537p3m"
#define mainpll_322p4m		"mainpll_322p4m"
#define mainpll_230p3m		"mainpll_230p3m"

#define univpll_624m		"univpll_624m"
#define univpll_416m		"univpll_416m"
#define univpll_249p6m		"univpll_249p6m"
#define univpll_178p3m		"univpll_178p3m"
#define univpll_48m		"univpll_48m"

/* DIV */
#define mmpll_d2		"mmpll_d2"
#define mmpll_d3		"mmpll_d3"
#define mmpll_d5		"mmpll_d5"
#define mmpll_d7		"mmpll_d7"
#define mmpll_d4		"mmpll_d4"
#define mmpll_d6		"mmpll_d6"

#define syspll_d2		"syspll_d2"
#define syspll_d4		"syspll_d4"
#define syspll_d6		"syspll_d6"
#define syspll_d8		"syspll_d8"
#define syspll_d10		"syspll_d10"
#define syspll_d12		"syspll_d12"
#define syspll_d16		"syspll_d16"
#define syspll_d24		"syspll_d24"
#define syspll_d3		"syspll_d3"
#define syspll_d2p5		"syspll_d2p5"
#define syspll_d5		"syspll_d5"
#define syspll_d3p5		"syspll_d3p5"

#define univpll1_d2		"univpll1_d2"
#define univpll1_d4		"univpll1_d4"
#define univpll1_d6		"univpll1_d6"
#define univpll1_d8		"univpll1_d8"
#define univpll1_d10		"univpll1_d10"

#define univpll2_d2		"univpll2_d2"
#define univpll2_d4		"univpll2_d4"
#define univpll2_d6		"univpll2_d6"
#define univpll2_d8		"univpll2_d8"

#define univpll_d3		"univpll_d3"
#define univpll_d5		"univpll_d5"
#define univpll_d7		"univpll_d7"
#define univpll_d10		"univpll_d10"
#define univpll_d26		"univpll_d26"

#define apll_ck			"apll"
#define apll_d4			"apll_d4"
#define apll_d8			"apll_d8"
#define apll_d16		"apll_d16"
#define apll_d24		"apll_d24"

#define lvdspll_d2		"lvdspll_d2"
#define lvdspll_d4		"lvdspll_d4"
#define lvdspll_d8		"lvdspll_d8"

#define lvdstx_clkdig_cts	"lvdstx_dig_cts"
#define vpll_dpix_ck		"vpll_dpix_ck"
#define tvhdmi_h_ck		"tvhdmi_h_ck"
#define hdmitx_clkdig_d2	"hdmitx_dig_d2"
#define hdmitx_clkdig_d3	"hdmitx_dig_d3"
#define tvhdmi_d2		"tvhdmi_d2"
#define tvhdmi_d4		"tvhdmi_d4"
#define mempll_mck_d4		"mempll_mck_d4"

/* TOP */
#define axi_sel			"axi_sel"
#define smi_sel			"smi_sel"
#define mfg_sel			"mfg_sel"
#define irda_sel		"irda_sel"
#define cam_sel			"cam_sel"
#define aud_intbus_sel		"aud_intbus_sel"
#define jpg_sel			"jpg_sel"
#define disp_sel		"disp_sel"
#define msdc30_1_sel		"msdc30_1_sel"
#define msdc30_2_sel		"msdc30_2_sel"
#define msdc30_3_sel		"msdc30_3_sel"
#define msdc30_4_sel		"msdc30_4_sel"
#define usb20_sel		"usb20_sel"
#define venc_sel		"venc_sel"
#define spi_sel			"spi_sel"
#define uart_sel		"uart_sel"
#define mem_sel			"mem_sel"
#define camtg_sel		"camtg_sel"
#define audio_sel		"audio_sel"
#define fix_sel			"fix_sel"
#define vdec_sel		"vdec_sel"
#define ddrphycfg_sel		"ddrphycfg_sel"
#define dpilvds_sel		"dpilvds_sel"
#define pmicspi_sel		"pmicspi_sel"
#define msdc30_0_sel		"msdc30_0_sel"
#define smi_mfg_as_sel		"smi_mfg_as_sel"
#define gcpu_sel		"gcpu_sel"
#define dpi1_sel		"dpi1_sel"
#define cci_sel			"cci_sel"
#define apll_sel		"apll_sel"
#define hdmipll_sel		"hdmipll_sel"

/* PERI0 */
#define i2c5_ck			"i2c5_ck"
#define i2c4_ck			"i2c4_ck"
#define i2c3_ck			"i2c3_ck"
#define i2c2_ck			"i2c2_ck"
#define i2c1_ck			"i2c1_ck"
#define i2c0_ck			"i2c0_ck"
#define uart3_ck		"uart3_ck"
#define uart2_ck		"uart2_ck"
#define uart1_ck		"uart1_ck"
#define uart0_ck		"uart0_ck"
#define irda_ck			"irda_ck"
#define nli_ck			"nli_ck"
#define md_hif_ck		"md_hif_ck"
#define ap_hif_ck		"ap_hif_ck"
#define msdc30_3_ck		"msdc30_3_ck"
#define msdc30_2_ck		"msdc30_2_ck"
#define msdc30_1_ck		"msdc30_1_ck"
#define msdc20_2_ck		"msdc20_2_ck"
#define msdc20_1_ck		"msdc20_1_ck"
#define ap_dma_ck		"ap_dma_ck"
#define usb1_ck			"usb1_ck"
#define usb0_ck			"usb0_ck"
#define pwm_ck			"pwm_ck"
#define pwm7_ck			"pwm7_ck"
#define pwm6_ck			"pwm6_ck"
#define pwm5_ck			"pwm5_ck"
#define pwm4_ck			"pwm4_ck"
#define pwm3_ck			"pwm3_ck"
#define pwm2_ck			"pwm2_ck"
#define pwm1_ck			"pwm1_ck"
#define therm_ck		"therm_ck"
#define nfi_ck			"nfi_ck"

/* PERI1 */
#define usbslv_ck		"usbslv_ck"
#define usb1_mcu_ck		"usb1_mcu_ck"
#define usb0_mcu_ck		"usb0_mcu_ck"
#define gcpu_ck			"gcpu_ck"
#define fhctl_ck		"fhctl_ck"
#define spi1_ck			"spi1_ck"
#define auxadc_ck		"auxadc_ck"
#define peri_pwrap_ck		"peri_pwrap_ck"
#define i2c6_ck			"i2c6_ck"

/* INFRA */
#define pmic_wrap_ck		"pmic_wrap_ck"
#define pmicspi_ck		"pmicspi_ck"
#define ccif1_ap_ctrl		"ccif1_ap_ctrl"
#define ccif0_ap_ctrl		"ccif0_ap_ctrl"
#define kp_ck			"kp_ck"
#define cpum_ck			"cpum_ck"
#define m4u_ck			"m4u_ck"
#define mfgaxi_ck		"mfgaxi_ck"
#define devapc_ck		"devapc_ck"
#define audio_ck		"audio_ck"
#define mfg_bus_ck		"mfg_bus_ck"
#define smi_ck			"smi_ck"
#define dbgclk_ck		"dbgclk_ck"

/* MFG */
#define baxi_ck			"baxi_ck"
#define bmem_ck			"bmem_ck"
#define bg3d_ck			"bg3d_ck"
#define b26m_ck			"b26m_ck"

/* ISP (IMG) */
#define fpc_ck			"fpc_ck"
#define jpgenc_jpg_ck		"jpgenc_jpg_ck"
#define jpgenc_smi_ck		"jpgenc_smi_ck"
#define jpgdec_jpg_ck		"jpgdec_jpg_ck"
#define jpgdec_smi_ck		"jpgdec_smi_ck"
#define sen_cam_ck		"sen_cam_ck"
#define sen_tg_ck		"sen_tg_ck"
#define cam_cam_ck		"cam_cam_ck"
#define cam_smi_ck		"cam_smi_ck"
#define comm_smi_ck		"comm_smi_ck"
#define larb4_smi_ck		"larb4_smi_ck"
#define larb3_smi_ck		"larb3_smi_ck"

/* VENC */
#define venc_ck			"venc_ck"

/* VDEC */
#define vdec_ck			"vdec_ck"
#define vdec_larb_ck		"vdec_larb_ck"

/* DISP0 */
#define smi_larb2_ck		"smi_larb2_ck"
#define rot_disp_ck		"rot_disp_ck"
#define rot_smi_ck		"rot_smi_ck"
#define scl_disp_ck		"scl_disp_ck"
#define ovl_disp_ck		"ovl_disp_ck"
#define ovl_smi_ck		"ovl_smi_ck"
#define color_disp_ck		"color_disp_ck"
#define tdshp_disp_ck		"tdshp_disp_ck"
#define bls_disp_ck		"bls_disp_ck"
#define wdma0_disp_ck		"wdma0_disp_ck"
#define wdma0_smi_ck		"wdma0_smi_ck"
#define wdma1_disp_ck		"wdma1_disp_ck"
#define wdma1_smi_ck		"wdma1_smi_ck"
#define rdma0_disp_ck		"rdma0_disp_ck"
#define rdma0_smi_ck		"rdma0_smi_ck"
#define rdma0_output_ck		"rdma0_output_ck"
#define rdma1_disp_ck		"rdma1_disp_ck"
#define rdma1_smi_ck		"rdma1_smi_ck"
#define rdma1_output_ck		"rdma1_output_ck"
#define gamma_disp_ck		"gamma_disp_ck"
#define gamma_pixel_ck		"gamma_pixel_ck"
#define cmdq_disp_ck		"cmdq_disp_ck"
#define cmdq_smi_ck		"cmdq_smi_ck"
#define g2d_disp_ck		"g2d_disp_ck"
#define g2d_smi_ck		"g2d_smi_ck"

/* DISP1 */
#define dsi_disp_ck		"dsi_disp_ck"
#define dsi_dsi_ck		"dsi_dsi_ck"
#define dsi_div2_dsi_ck		"dsi_div2_dsi_ck"
#define dpi1_ck			"dpi1_ck"
#define lvds_disp_ck		"lvds_disp_ck"
#define lvds_cts_ck		"lvds_cts_ck"
#define hdmi_disp_ck		"hdmi_disp_ck"
#define hdmi_pll_ck		"hdmi_pll_ck"
#define hdmi_audio_ck		"hdmi_audio_ck"
#define hdmi_spdif_ck		"hdmi_spdif_ck"
#define mutex_26m_ck		"mutex_26m_ck"
#define ufo_disp_ck		"ufo_disp_ck"

/* AUDIO */
#define afe_ck			"afe_ck"
#define i2s_ck			"i2s_ck"
#define apll_tuner_ck		"apll_tuner_ck"
#define hdmi_ck			"hdmi_ck"
#define spdf_ck			"spdf_ck"

struct mtk_fixed_factor {
	int id;
	const char *name;
	const char *parent_name;
	int mult;
	int div;
};

#define FACTOR(_id, _name, _parent, _mult, _div) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.mult = _mult,				\
		.div = _div,				\
	}

static void __init init_factors(struct mtk_fixed_factor *clks, int num,
		struct clk_onecell_data *clk_data)
{
	int i;
	struct clk *clk;

	for (i = 0; i < num; i++) {
		struct mtk_fixed_factor *ff = &clks[i];

		clk = clk_register_fixed_factor(NULL, ff->name, ff->parent_name,
				0, ff->mult, ff->div);

		if (IS_ERR(clk)) {
			pr_err("Failed to register clk %s: %ld\n",
					ff->name, PTR_ERR(clk));
			continue;
		}

		if (clk_data)
			clk_data->clks[ff->id] = clk;

		pr_debug("factor %3d: %s\n", i, ff->name);
	}
}

static struct mtk_fixed_factor root_clk_alias[] __initdata = {
	FACTOR(TOP_DSI0_LNTC_DSICLK, dsi0_lntc_dsiclk, clk_null, 1, 1),
	FACTOR(TOP_HDMITX_CLKDIG_CTS, hdmitx_clkdig_cts, clk_null, 1, 1),
	FACTOR(TOP_CLKPH_MCK, clkph_mck, clk_null, 1, 1),
	FACTOR(TOP_CPUM_TCK_IN, cpum_tck_in, clk_null, 1, 1),
};

static void __init init_clk_root_alias(struct clk_onecell_data *clk_data)
{
	init_factors(root_clk_alias, ARRAY_SIZE(root_clk_alias), clk_data);
}

static struct mtk_fixed_factor top_divs[] __initdata = {
	FACTOR(TOP_MAINPLL_806M, mainpll_806m, mainpll, 1, 2),
	FACTOR(TOP_MAINPLL_537P3M, mainpll_537p3m, mainpll, 1, 3),
	FACTOR(TOP_MAINPLL_322P4M, mainpll_322p4m, mainpll, 1, 5),
	FACTOR(TOP_MAINPLL_230P3M, mainpll_230p3m, mainpll, 1, 7),

	FACTOR(TOP_UNIVPLL_624M, univpll_624m, univpll, 1, 2),
	FACTOR(TOP_UNIVPLL_416M, univpll_416m, univpll, 1, 3),
	FACTOR(TOP_UNIVPLL_249P6M, univpll_249p6m, univpll, 1, 5),
	FACTOR(TOP_UNIVPLL_178P3M, univpll_178p3m, univpll, 1, 7),
	FACTOR(TOP_UNIVPLL_48M, univpll_48m, univpll, 1, 26),

	FACTOR(TOP_MMPLL_D2, mmpll_d2, mmpll, 1, 2),
	FACTOR(TOP_MMPLL_D3, mmpll_d3, mmpll, 1, 3),
	FACTOR(TOP_MMPLL_D5, mmpll_d5, mmpll, 1, 5),
	FACTOR(TOP_MMPLL_D7, mmpll_d7, mmpll, 1, 7),
	FACTOR(TOP_MMPLL_D4, mmpll_d4, mmpll_d2, 1, 2),
	FACTOR(TOP_MMPLL_D6, mmpll_d6, mmpll_d3, 1, 2),

	FACTOR(TOP_SYSPLL_D2, syspll_d2, mainpll_806m, 1, 1),
	FACTOR(TOP_SYSPLL_D4, syspll_d4, mainpll_806m, 1, 2),
	FACTOR(TOP_SYSPLL_D6, syspll_d6, mainpll_806m, 1, 3),
	FACTOR(TOP_SYSPLL_D8, syspll_d8, mainpll_806m, 1, 4),
	FACTOR(TOP_SYSPLL_D10, syspll_d10, mainpll_806m, 1, 5),
	FACTOR(TOP_SYSPLL_D12, syspll_d12, mainpll_806m, 1, 6),
	FACTOR(TOP_SYSPLL_D16, syspll_d16, mainpll_806m, 1, 8),
	FACTOR(TOP_SYSPLL_D24, syspll_d24, mainpll_806m, 1, 12),

	FACTOR(TOP_SYSPLL_D3, syspll_d3, mainpll_537p3m, 1, 1),

	FACTOR(TOP_SYSPLL_D2P5, syspll_d2p5, mainpll_322p4m, 2, 1),
	FACTOR(TOP_SYSPLL_D5, syspll_d5, mainpll_322p4m, 1, 1),

	FACTOR(TOP_SYSPLL_D3P5, syspll_d3p5, mainpll_230p3m, 2, 1),

	FACTOR(TOP_UNIVPLL1_D2, univpll1_d2, univpll_624m, 1, 2),
	FACTOR(TOP_UNIVPLL1_D4, univpll1_d4, univpll_624m, 1, 4),
	FACTOR(TOP_UNIVPLL1_D6, univpll1_d6, univpll_624m, 1, 6),
	FACTOR(TOP_UNIVPLL1_D8, univpll1_d8, univpll_624m, 1, 8),
	FACTOR(TOP_UNIVPLL1_D10, univpll1_d10, univpll_624m, 1, 10),

	FACTOR(TOP_UNIVPLL2_D2, univpll2_d2, univpll_416m, 1, 2),
	FACTOR(TOP_UNIVPLL2_D4, univpll2_d4, univpll_416m, 1, 4),
	FACTOR(TOP_UNIVPLL2_D6, univpll2_d6, univpll_416m, 1, 6),
	FACTOR(TOP_UNIVPLL2_D8, univpll2_d8, univpll_416m, 1, 8),

	FACTOR(TOP_UNIVPLL_D3, univpll_d3, univpll_416m, 1, 1),
	FACTOR(TOP_UNIVPLL_D5, univpll_d5, univpll_249p6m, 1, 1),
	FACTOR(TOP_UNIVPLL_D7, univpll_d7, univpll_178p3m, 1, 1),
	FACTOR(TOP_UNIVPLL_D10, univpll_d10, univpll_249p6m, 1, 5),
	FACTOR(TOP_UNIVPLL_D26, univpll_d26, univpll_48m, 1, 1),

	FACTOR(TOP_APLL_CK, apll_ck, audpll, 1, 1),
	FACTOR(TOP_APLL_D4, apll_d4, audpll, 1, 4),
	FACTOR(TOP_APLL_D8, apll_d8, audpll, 1, 8),
	FACTOR(TOP_APLL_D16, apll_d16, audpll, 1, 16),
	FACTOR(TOP_APLL_D24, apll_d24, audpll, 1, 24),

	FACTOR(TOP_LVDSPLL_D2, lvdspll_d2, lvdspll, 1, 2),
	FACTOR(TOP_LVDSPLL_D4, lvdspll_d4, lvdspll, 1, 4),
	FACTOR(TOP_LVDSPLL_D8, lvdspll_d8, lvdspll, 1, 8),

	FACTOR(TOP_LVDSTX_CLKDIG_CT, lvdstx_clkdig_cts, lvdspll, 1, 1),
	FACTOR(TOP_VPLL_DPIX_CK, vpll_dpix_ck, lvdspll, 1, 1),

	FACTOR(TOP_TVHDMI_H_CK, tvhdmi_h_ck, tvdpll, 1, 1),

	FACTOR(TOP_HDMITX_CLKDIG_D2, hdmitx_clkdig_d2, hdmitx_clkdig_cts, 1, 2),
	FACTOR(TOP_HDMITX_CLKDIG_D3, hdmitx_clkdig_d3, hdmitx_clkdig_cts, 1, 3),

	FACTOR(TOP_TVHDMI_D2, tvhdmi_d2, tvhdmi_h_ck, 1, 2),
	FACTOR(TOP_TVHDMI_D4, tvhdmi_d4, tvhdmi_h_ck, 1, 4),

	FACTOR(TOP_MEMPLL_MCK_D4, mempll_mck_d4, clkph_mck, 1, 4),
};

static void __init init_clk_top_div(struct clk_onecell_data *clk_data)
{
	init_factors(top_divs, ARRAY_SIZE(top_divs), clk_data);
}

static const char *axi_parents[] __initconst = {
		clk26m,
		syspll_d3,
		syspll_d4,
		syspll_d6,
		univpll_d5,
		univpll2_d2,
		syspll_d3p5};

static const char *smi_parents[] __initconst = {
		clk26m,
		clkph_mck,
		syspll_d2p5,
		syspll_d3,
		syspll_d8,
		univpll_d5,
		univpll1_d2,
		univpll1_d6,
		mmpll_d3,
		mmpll_d4,
		mmpll_d5,
		mmpll_d6,
		mmpll_d7,
		vdecpll,
		lvdspll};

static const char *mfg_parents[] __initconst = {
		clk26m,
		univpll1_d4,
		syspll_d2,
		syspll_d2p5,
		syspll_d3,
		univpll_d5,
		univpll1_d2,
		mmpll_d2,
		mmpll_d3,
		mmpll_d4,
		mmpll_d5,
		mmpll_d6,
		mmpll_d7};

static const char *irda_parents[] __initconst = {
		clk26m,
		univpll2_d8,
		univpll1_d6};

static const char *cam_parents[] __initconst = {
		clk26m,
		syspll_d3,
		syspll_d3p5,
		syspll_d4,
		univpll_d5,
		univpll2_d2,
		univpll_d7,
		univpll1_d4};

static const char *aud_intbus_parents[] __initconst = {
		clk26m,
		syspll_d6,
		univpll_d10};

static const char *jpg_parents[] __initconst = {
		clk26m,
		syspll_d5,
		syspll_d4,
		syspll_d3,
		univpll_d7,
		univpll2_d2,
		univpll_d5};

static const char *disp_parents[] __initconst = {
		clk26m,
		syspll_d3p5,
		syspll_d3,
		univpll2_d2,
		univpll_d5,
		univpll1_d2,
		lvdspll,
		vdecpll};

static const char *msdc30_parents[] __initconst = {
		clk26m,
		syspll_d6,
		syspll_d5,
		univpll1_d4,
		univpll2_d4,
		msdcpll};

static const char *usb20_parents[] __initconst = {
		clk26m,
		univpll2_d6,
		univpll1_d10};

static const char *venc_parents[] __initconst = {
		clk26m,
		syspll_d3,
		syspll_d8,
		univpll_d5,
		univpll1_d6,
		mmpll_d4,
		mmpll_d5,
		mmpll_d6};

static const char *spi_parents[] __initconst = {
		clk26m,
		syspll_d6,
		syspll_d8,
		syspll_d10,
		univpll1_d6,
		univpll1_d8};

static const char *uart_parents[] __initconst = {
		clk26m,
		univpll2_d8};

static const char *mem_parents[] __initconst = {
		clk26m,
		clkph_mck};

static const char *camtg_parents[] __initconst = {
		clk26m,
		univpll_d26,
		univpll1_d6,
		syspll_d16,
		syspll_d8};

static const char *audio_parents[] __initconst = {
		clk26m,
		syspll_d24};

static const char *fix_parents[] __initconst = {
		rtc32k,
		clk26m,
		univpll_d5,
		univpll_d7,
		univpll1_d2,
		univpll1_d4,
		univpll1_d6,
		univpll1_d8};

static const char *vdec_parents[] __initconst = {
		clk26m,
		vdecpll,
		clkph_mck,
		syspll_d2p5,
		syspll_d3,
		syspll_d3p5,
		syspll_d4,
		syspll_d5,
		syspll_d6,
		syspll_d8,
		univpll1_d2,
		univpll2_d2,
		univpll_d7,
		univpll_d10,
		univpll2_d4,
		lvdspll};

static const char *ddrphycfg_parents[] __initconst = {
		clk26m,
		axi_sel,
		syspll_d12};

static const char *dpilvds_parents[] __initconst = {
		clk26m,
		lvdspll,
		lvdspll_d2,
		lvdspll_d4,
		lvdspll_d8};

static const char *pmicspi_parents[] __initconst = {
		clk26m,
		univpll2_d6,
		syspll_d8,
		syspll_d10,
		univpll1_d10,
		mempll_mck_d4,
		univpll_d26,
		syspll_d24};

static const char *smi_mfg_as_parents[] __initconst = {
		clk26m,
		smi_sel,
		mfg_sel,
		mem_sel};

static const char *gcpu_parents[] __initconst = {
		clk26m,
		syspll_d4,
		univpll_d7,
		syspll_d5,
		syspll_d6};

static const char *dpi1_parents[] __initconst = {
		clk26m,
		tvhdmi_h_ck,
		tvhdmi_d2,
		tvhdmi_d4};

static const char *cci_parents[] __initconst = {
		clk26m,
		mainpll_537p3m,
		univpll_d3,
		syspll_d2p5,
		syspll_d3,
		syspll_d5};

static const char *apll_parents[] __initconst = {
		clk26m,
		apll_ck,
		apll_d4,
		apll_d8,
		apll_d16,
		apll_d24};

static const char *hdmipll_parents[] __initconst = {
		clk26m,
		hdmitx_clkdig_cts,
		hdmitx_clkdig_d2,
		hdmitx_clkdig_d3};

struct mtk_mux {
	int id;
	const char *name;
	u32 reg;
	int shift;
	int width;
	int gate;
	const char **parent_names;
	int num_parents;
};

#define MUX(_id, _name, _parents, _reg, _shift, _width, _gate) {	\
		.id = _id,						\
		.name = _name,						\
		.reg = _reg,						\
		.shift = _shift,					\
		.width = _width,					\
		.gate = _gate,						\
		.parent_names = (const char **)_parents,		\
		.num_parents = ARRAY_SIZE(_parents),			\
	}

static struct mtk_mux top_muxes[] __initdata = {
	/* CLK_CFG_0 */
	MUX(TOP_AXI_SEL, axi_sel, axi_parents,
		0x0140, 0, 3, INVALID_MUX_GATE_BIT),
	MUX(TOP_SMI_SEL, smi_sel, smi_parents, 0x0140, 8, 4, 15),
	MUX(TOP_MFG_SEL, mfg_sel, mfg_parents, 0x0140, 16, 4, 23),
	MUX(TOP_IRDA_SEL, irda_sel, irda_parents, 0x0140, 24, 2, 31),
	/* CLK_CFG_1 */
	MUX(TOP_CAM_SEL, cam_sel, cam_parents, 0x0144, 0, 3, 7),
	MUX(TOP_AUD_INTBUS_SEL, aud_intbus_sel, aud_intbus_parents,
		0x0144, 8, 2, 15),
	MUX(TOP_JPG_SEL, jpg_sel, jpg_parents, 0x0144, 16, 3, 23),
	MUX(TOP_DISP_SEL, disp_sel, disp_parents, 0x0144, 24, 3, 31),
	/* CLK_CFG_2 */
	MUX(TOP_MSDC30_1_SEL, msdc30_1_sel, msdc30_parents, 0x0148, 0, 3, 7),
	MUX(TOP_MSDC30_2_SEL, msdc30_2_sel, msdc30_parents, 0x0148, 8, 3, 15),
	MUX(TOP_MSDC30_3_SEL, msdc30_3_sel, msdc30_parents, 0x0148, 16, 3, 23),
	MUX(TOP_MSDC30_4_SEL, msdc30_4_sel, msdc30_parents, 0x0148, 24, 3, 31),
	/* CLK_CFG_3 */
	MUX(TOP_USB20_SEL, usb20_sel, usb20_parents, 0x014c, 0, 2, 7),
	/* CLK_CFG_4 */
	MUX(TOP_VENC_SEL, venc_sel, venc_parents, 0x0150, 8, 3, 15),
	MUX(TOP_SPI_SEL, spi_sel, spi_parents, 0x0150, 16, 3, 23),
	MUX(TOP_UART_SEL, uart_sel, uart_parents, 0x0150, 24, 2, 31),
	/* CLK_CFG_6 */
	MUX(TOP_MEM_SEL, mem_sel, mem_parents, 0x0158, 0, 2, 7),
	MUX(TOP_CAMTG_SEL, camtg_sel, camtg_parents, 0x0158, 8, 3, 15),
	MUX(TOP_AUDIO_SEL, audio_sel, audio_parents, 0x0158, 24, 2, 31),
	/* CLK_CFG_7 */
	MUX(TOP_FIX_SEL, fix_sel, fix_parents, 0x015c, 0, 3, 7),
	MUX(TOP_VDEC_SEL, vdec_sel, vdec_parents, 0x015c, 8, 4, 15),
	MUX(TOP_DDRPHYCFG_SEL, ddrphycfg_sel, ddrphycfg_parents,
		0x015c, 16, 2, 23),
	MUX(TOP_DPILVDS_SEL, dpilvds_sel, dpilvds_parents, 0x015c, 24, 3, 31),
	/* CLK_CFG_8 */
	MUX(TOP_PMICSPI_SEL, pmicspi_sel, pmicspi_parents, 0x0164, 0, 3, 7),
	MUX(TOP_MSDC30_0_SEL, msdc30_0_sel, msdc30_parents, 0x0164, 8, 3, 15),
	MUX(TOP_SMI_MFG_AS_SEL, smi_mfg_as_sel, smi_mfg_as_parents,
		0x0164, 16, 2, 23),
	MUX(TOP_GCPU_SEL, gcpu_sel, gcpu_parents, 0x0164, 24, 3, 31),
	/* CLK_CFG_9 */
	MUX(TOP_DPI1_SEL, dpi1_sel, dpi1_parents, 0x0168, 0, 2, 7),
	MUX(TOP_CCI_SEL, cci_sel, cci_parents, 0x0168, 8, 3, 15),
	MUX(TOP_APLL_SEL, apll_sel, apll_parents, 0x0168, 16, 3, 23),
	MUX(TOP_HDMIPLL_SEL, hdmipll_sel, hdmipll_parents, 0x0168, 24, 2, 31),
};

static void __init init_clk_topckgen(void __iomem *top_base,
		struct clk_onecell_data *clk_data)
{
	int i;
	struct clk *clk;

	for (i = 0; i < ARRAY_SIZE(top_muxes); i++) {
		struct mtk_mux *mux = &top_muxes[i];

		clk = mtk_clk_register_mux(mux->name,
			mux->parent_names, mux->num_parents,
			top_base + mux->reg, mux->shift, mux->width, mux->gate);

		if (IS_ERR(clk)) {
			pr_err("Failed to register clk %s: %ld\n",
					mux->name, PTR_ERR(clk));
			continue;
		}

		if (clk_data)
			clk_data->clks[mux->id] = clk;

		pr_debug("mux %3d: %s\n", i, mux->name);
	}
}

struct mtk_pll {
	int id;
	const char *name;
	const char *parent_name;
	u32 reg;
	u32 pwr_reg;
	u32 en_mask;
	unsigned int flags;
	const struct clk_ops *ops;
};

#define PLL(_id, _name, _parent, _reg, _pwr_reg, _en_mask, _flags, _ops) { \
		.id = _id,						\
		.name = _name,						\
		.parent_name = _parent,					\
		.reg = _reg,						\
		.pwr_reg = _pwr_reg,					\
		.en_mask = _en_mask,					\
		.flags = _flags,					\
		.ops = _ops,						\
	}

static struct mtk_pll plls[] __initdata = {
	PLL(APMIXED_ARMPLL1, armpll1, clk26m, 0x0200, 0x0218,
		0x80000001, HAVE_PLL_HP, &mtk_clk_arm_pll_ops),
	PLL(APMIXED_ARMPLL2, armpll2, clk26m, 0x02cc, 0x02e4,
		0x80000001, HAVE_PLL_HP, &mtk_clk_arm_pll_ops),
	PLL(APMIXED_MAINPLL, mainpll, clk26m, 0x021c, 0x0234,
		0xf0000001, HAVE_PLL_HP | HAVE_RST_BAR | PLL_AO,
		&mtk_clk_pll_ops),
	PLL(APMIXED_UNIVPLL, univpll, clk26m, 0x0238, 0x0250,
		0xf3000001, HAVE_RST_BAR | HAVE_FIX_FRQ | PLL_AO,
		&mtk_clk_lc_pll_ops),
	PLL(APMIXED_MMPLL, mmpll, clk26m, 0x0254, 0x026c,
		0xf0000001, HAVE_PLL_HP | HAVE_RST_BAR, &mtk_clk_pll_ops),
	PLL(APMIXED_MSDCPLL, msdcpll, clk26m, 0x0278, 0x0290,
		0x80000001, HAVE_PLL_HP, &mtk_clk_pll_ops),
	PLL(APMIXED_TVDPLL, tvdpll, clk26m, 0x0294, 0x02ac,
		0x80000001, HAVE_PLL_HP, &mtk_clk_tvd_pll_ops),
	PLL(APMIXED_LVDSPLL, lvdspll, clk26m, 0x02b0, 0x02c8,
		0x80000001, HAVE_PLL_HP, &mtk_clk_pll_ops),
	PLL(APMIXED_AUDPLL, audpll, clk26m, 0x02e8, 0x0300,
		0x80000001, 0, &mtk_clk_aud_pll_ops),
	PLL(APMIXED_VDECPLL, vdecpll, clk26m, 0x0304, 0x031c,
		0x80000001, HAVE_PLL_HP, &mtk_clk_pll_ops),
};

static void __init init_clk_apmixedsys(void __iomem *apmixed_base,
		struct clk_onecell_data *clk_data)
{
	int i;
	struct clk *clk;

	for (i = 0; i < ARRAY_SIZE(plls); i++) {
		struct mtk_pll *pll = &plls[i];

		clk = mtk_clk_register_pll(pll->name, pll->parent_name,
				apmixed_base + pll->reg,
				apmixed_base + pll->pwr_reg,
				pll->en_mask, pll->flags, pll->ops);

		if (IS_ERR(clk)) {
			pr_err("Failed to register clk %s: %ld\n",
					pll->name, PTR_ERR(clk));
			continue;
		}

		if (clk_data)
			clk_data->clks[pll->id] = clk;

		pr_debug("pll %3d: %s\n", i, pll->name);
	}
}

struct mtk_gate_regs {
	u32 sta_ofs;
	u32 clr_ofs;
	u32 set_ofs;
};

struct mtk_gate {
	int id;
	const char *name;
	const char *parent_name;
	struct mtk_gate_regs *regs;
	int shift;
	u32 flags;
};

#define GATE(_id, _name, _parent, _regs, _shift, _flags) {	\
		.id = _id,					\
		.name = _name,					\
		.parent_name = _parent,				\
		.regs = &_regs,					\
		.shift = _shift,				\
		.flags = _flags,				\
	}

static void __init init_clk_gates(
		void __iomem *reg_base,
		struct mtk_gate *clks, int num,
		struct clk_onecell_data *clk_data)
{
	int i;
	struct clk *clk;

	for (i = 0; i < num; i++) {
		struct mtk_gate *gate = &clks[i];

		clk = mtk_clk_register_gate(gate->name, gate->parent_name,
				reg_base + gate->regs->set_ofs,
				reg_base + gate->regs->clr_ofs,
				reg_base + gate->regs->sta_ofs,
				gate->shift, gate->flags);

		if (IS_ERR(clk)) {
			pr_err("Failed to register clk %s: %ld\n",
					gate->name, PTR_ERR(clk));
			continue;
		}

		if (clk_data)
			clk_data->clks[gate->id] = clk;

		pr_debug("gate %3d: %s\n", i, gate->name);
	}
}

static struct mtk_gate_regs infra_cg_regs = {
	.set_ofs = 0x0040,
	.clr_ofs = 0x0044,
	.sta_ofs = 0x0048,
};

static struct mtk_gate infra_clks[] __initdata = {
	GATE(INFRA_PMIC_WRAP_CK, pmic_wrap_ck, axi_sel, infra_cg_regs, 23, 0),
	GATE(INFRA_PMICSPI_CK, pmicspi_ck, pmicspi_sel, infra_cg_regs, 22, 0),
	GATE(INFRA_CCIF1_AP_CTRL, ccif1_ap_ctrl, axi_sel, infra_cg_regs, 21, 0),
	GATE(INFRA_CCIF0_AP_CTRL, ccif0_ap_ctrl, axi_sel, infra_cg_regs, 20, 0),
	GATE(INFRA_KP_CK, kp_ck, axi_sel, infra_cg_regs, 16, 0),
	GATE(INFRA_CPUM_CK, cpum_ck, cpum_tck_in, infra_cg_regs, 15, 0),
	GATE(INFRA_M4U_CK, m4u_ck, mem_sel, infra_cg_regs, 8, 0),
	GATE(INFRA_MFGAXI_CK, mfgaxi_ck, axi_sel, infra_cg_regs, 7, 0),
	GATE(INFRA_DEVAPC_CK, devapc_ck, axi_sel, infra_cg_regs, 6,
		CLK_GATE_INVERSE),
	GATE(INFRA_AUDIO_CK, audio_ck, aud_intbus_sel, infra_cg_regs, 5, 0),
	GATE(INFRA_MFG_BUS_CK, mfg_bus_ck, axi_sel, infra_cg_regs, 2, 0),
	GATE(INFRA_SMI_CK, smi_ck, smi_sel, infra_cg_regs, 1, 0),
	GATE(INFRA_DBGCLK_CK, dbgclk_ck, axi_sel, infra_cg_regs, 0, 0),
};

static void __init init_clk_infrasys(void __iomem *infrasys_base,
		struct clk_onecell_data *clk_data)
{
	pr_debug("init infrasys gates:\n");
	init_clk_gates(infrasys_base, infra_clks, ARRAY_SIZE(infra_clks),
		clk_data);
}

static struct mtk_gate_regs peri0_cg_regs = {
	.set_ofs = 0x0008,
	.clr_ofs = 0x0010,
	.sta_ofs = 0x0018,
};

static struct mtk_gate_regs peri1_cg_regs = {
	.set_ofs = 0x000c,
	.clr_ofs = 0x0014,
	.sta_ofs = 0x001c,
};

static struct mtk_gate peri_clks[] __initdata = {
	/* PERI0 */
	GATE(PERI_I2C5_CK, i2c5_ck, axi_sel, peri0_cg_regs, 31, 0),
	GATE(PERI_I2C4_CK, i2c4_ck, axi_sel, peri0_cg_regs, 30, 0),
	GATE(PERI_I2C3_CK, i2c3_ck, axi_sel, peri0_cg_regs, 29, 0),
	GATE(PERI_I2C2_CK, i2c2_ck, axi_sel, peri0_cg_regs, 28, 0),
	GATE(PERI_I2C1_CK, i2c1_ck, axi_sel, peri0_cg_regs, 27, 0),
	GATE(PERI_I2C0_CK, i2c0_ck, axi_sel, peri0_cg_regs, 26, 0),
	GATE(PERI_UART3_CK, uart3_ck, axi_sel, peri0_cg_regs, 25, 0),
	GATE(PERI_UART2_CK, uart2_ck, axi_sel, peri0_cg_regs, 24, 0),
	GATE(PERI_UART1_CK, uart1_ck, axi_sel, peri0_cg_regs, 23, 0),
	GATE(PERI_UART0_CK, uart0_ck, axi_sel, peri0_cg_regs, 22, 0),
	GATE(PERI_IRDA_CK, irda_ck, irda_sel, peri0_cg_regs, 21, 0),
	GATE(PERI_NLI_CK, nli_ck, axi_sel, peri0_cg_regs, 20, 0),
	GATE(PERI_MD_HIF_CK, md_hif_ck, axi_sel, peri0_cg_regs, 19, 0),
	GATE(PERI_AP_HIF_CK, ap_hif_ck, axi_sel, peri0_cg_regs, 18, 0),
	GATE(PERI_MSDC30_3_CK, msdc30_3_ck, msdc30_4_sel, peri0_cg_regs, 17, 0),
	GATE(PERI_MSDC30_2_CK, msdc30_2_ck, msdc30_3_sel, peri0_cg_regs, 16, 0),
	GATE(PERI_MSDC30_1_CK, msdc30_1_ck, msdc30_2_sel, peri0_cg_regs, 15, 0),
	GATE(PERI_MSDC20_2_CK, msdc20_2_ck, msdc30_1_sel, peri0_cg_regs, 14, 0),
	GATE(PERI_MSDC20_1_CK, msdc20_1_ck, msdc30_0_sel, peri0_cg_regs, 13, 0),
	GATE(PERI_AP_DMA_CK, ap_dma_ck, axi_sel, peri0_cg_regs, 12, 0),
	GATE(PERI_USB1_CK, usb1_ck, usb20_sel, peri0_cg_regs, 11, 0),
	GATE(PERI_USB0_CK, usb0_ck, usb20_sel, peri0_cg_regs, 10, 0),
	GATE(PERI_PWM_CK, pwm_ck, axi_sel, peri0_cg_regs, 9, 0),
	GATE(PERI_PWM7_CK, pwm7_ck, axi_sel, peri0_cg_regs, 8, 0),
	GATE(PERI_PWM6_CK, pwm6_ck, axi_sel, peri0_cg_regs, 7, 0),
	GATE(PERI_PWM5_CK, pwm5_ck, axi_sel, peri0_cg_regs, 6, 0),
	GATE(PERI_PWM4_CK, pwm4_ck, axi_sel, peri0_cg_regs, 5, 0),
	GATE(PERI_PWM3_CK, pwm3_ck, axi_sel, peri0_cg_regs, 4, 0),
	GATE(PERI_PWM2_CK, pwm2_ck, axi_sel, peri0_cg_regs, 3, 0),
	GATE(PERI_PWM1_CK, pwm1_ck, axi_sel, peri0_cg_regs, 2, 0),
	GATE(PERI_THERM_CK, therm_ck, axi_sel, peri0_cg_regs, 1, 0),
	GATE(PERI_NFI_CK, nfi_ck, axi_sel, peri0_cg_regs, 0, 0),
	/* PERI1 */
	GATE(PERI_USBSLV_CK, usbslv_ck, axi_sel, peri1_cg_regs, 8, 0),
	GATE(PERI_USB1_MCU_CK, usb1_mcu_ck, axi_sel, peri1_cg_regs, 7, 0),
	GATE(PERI_USB0_MCU_CK, usb0_mcu_ck, axi_sel, peri1_cg_regs, 6, 0),
	GATE(PERI_GCPU_CK, gcpu_ck, gcpu_sel, peri1_cg_regs, 5, 0),
	GATE(PERI_FHCTL_CK, fhctl_ck, clk26m, peri1_cg_regs, 4, 0),
	GATE(PERI_SPI1_CK, spi1_ck, spi_sel, peri1_cg_regs, 3, 0),
	GATE(PERI_AUXADC_CK, auxadc_ck, clk26m, peri1_cg_regs, 2, 0),
	GATE(PERI_PERI_PWRAP_CK, peri_pwrap_ck, axi_sel, peri1_cg_regs, 1, 0),
	GATE(PERI_I2C6_CK, i2c6_ck, axi_sel, peri1_cg_regs, 0, 0),
};

static void __init init_clk_perisys(void __iomem *perisys_base,
		struct clk_onecell_data *clk_data)
{
	pr_debug("init perisys gates:\n");
	init_clk_gates(perisys_base, peri_clks, ARRAY_SIZE(peri_clks),
		clk_data);
}

static struct mtk_gate_regs mfg_cg_regs = {
	.set_ofs = 0x0004,
	.clr_ofs = 0x0008,
	.sta_ofs = 0x0000,
};

static struct mtk_gate mfg_clks[] __initdata = {
	GATE(MFG_BAXI_CK, baxi_ck, axi_sel, mfg_cg_regs, 0, 0),
	GATE(MFG_BMEM_CK, bmem_ck, smi_mfg_as_sel, mfg_cg_regs, 1, 0),
	GATE(MFG_BG3D_CK, bg3d_ck, mfg_sel, mfg_cg_regs, 2, 0),
	GATE(MFG_B26M_CK, b26m_ck, clk26m, mfg_cg_regs, 3, 0),
};

static void __init init_clk_mfgsys(void __iomem *mfgsys_base,
		struct clk_onecell_data *clk_data)
{
	pr_debug("init mfgsys gates:\n");
	init_clk_gates(mfgsys_base, mfg_clks, ARRAY_SIZE(mfg_clks), clk_data);
}

static struct mtk_gate_regs img_cg_regs = {
	.set_ofs = 0x0004,
	.clr_ofs = 0x0008,
	.sta_ofs = 0x0000,
};

static struct mtk_gate img_clks[] __initdata = {
	GATE(IMG_FPC_CK, fpc_ck, smi_sel, img_cg_regs, 13, 0),
	GATE(IMG_JPGENC_JPG_CK, jpgenc_jpg_ck, jpg_sel, img_cg_regs, 12, 0),
	GATE(IMG_JPGENC_SMI_CK, jpgenc_smi_ck, smi_sel, img_cg_regs, 11, 0),
	GATE(IMG_JPGDEC_JPG_CK, jpgdec_jpg_ck, jpg_sel, img_cg_regs, 10, 0),
	GATE(IMG_JPGDEC_SMI_CK, jpgdec_smi_ck, smi_sel, img_cg_regs, 9, 0),
	GATE(IMG_SEN_CAM_CK, sen_cam_ck, cam_sel, img_cg_regs, 8, 0),
	GATE(IMG_SEN_TG_CK, sen_tg_ck, camtg_sel, img_cg_regs, 7, 0),
	GATE(IMG_CAM_CAM_CK, cam_cam_ck, cam_sel, img_cg_regs, 6, 0),
	GATE(IMG_CAM_SMI_CK, cam_smi_ck, smi_sel, img_cg_regs, 5, 0),
	GATE(IMG_COMM_SMI_CK, comm_smi_ck, smi_sel, img_cg_regs, 4, 0),
	GATE(IMG_LARB4_SMI_CK, larb4_smi_ck, smi_sel, img_cg_regs, 2, 0),
	GATE(IMG_LARB3_SMI_CK, larb3_smi_ck, smi_sel, img_cg_regs, 0, 0),
};

static void __init init_clk_imgsys(void __iomem *imgsys_base,
		struct clk_onecell_data *clk_data)
{
	pr_debug("init imgsys gates:\n");
	init_clk_gates(imgsys_base, img_clks, ARRAY_SIZE(img_clks), clk_data);
}

static struct mtk_gate_regs venc_cg_regs = {
	.set_ofs = 0x0004,
	.clr_ofs = 0x0008,
	.sta_ofs = 0x0000,
};

static struct mtk_gate venc_clks[] __initdata = {
	GATE(VENC_VENC_CK, venc_ck, venc_sel, venc_cg_regs, 0,
		CLK_GATE_INVERSE),
};

static void __init init_clk_vencsys(void __iomem *vencsys_base,
		struct clk_onecell_data *clk_data)
{
	pr_debug("init vencsys gates:\n");
	init_clk_gates(vencsys_base, venc_clks, ARRAY_SIZE(venc_clks),
		clk_data);
}

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

static struct mtk_gate vdec_clks[] __initdata = {
	GATE(VDEC_VDEC_CK, vdec_ck, vdec_sel, vdec0_cg_regs, 0,
		CLK_GATE_INVERSE),
	GATE(VDEC_VDEC_LARB_CK, vdec_larb_ck, smi_sel, vdec1_cg_regs, 0,
		CLK_GATE_INVERSE),
};

static void __init init_clk_vdecsys(void __iomem *vdecsys_base,
		struct clk_onecell_data *clk_data)
{
	pr_debug("init vdecsys gates:\n");
	init_clk_gates(vdecsys_base, vdec_clks, ARRAY_SIZE(vdec_clks),
		clk_data);
}

static struct mtk_gate_regs disp0_cg_regs = {
	.set_ofs = 0x0104,
	.clr_ofs = 0x0108,
	.sta_ofs = 0x0100,
};

static struct mtk_gate_regs disp1_cg_regs = {
	.set_ofs = 0x0114,
	.clr_ofs = 0x0118,
	.sta_ofs = 0x0110,
};

static struct mtk_gate disp_clks[] __initdata = {
	/* DISP0 */
	GATE(DISP_SMI_LARB2_CK, smi_larb2_ck, smi_sel, disp0_cg_regs, 0, 0),
	GATE(DISP_ROT_DISP_CK, rot_disp_ck, disp_sel, disp0_cg_regs, 1, 0),
	GATE(DISP_ROT_SMI_CK, rot_smi_ck, smi_sel, disp0_cg_regs, 2, 0),
	GATE(DISP_SCL_DISP_CK, scl_disp_ck, disp_sel, disp0_cg_regs, 3, 0),
	GATE(DISP_OVL_DISP_CK, ovl_disp_ck, disp_sel, disp0_cg_regs, 4, 0),
	GATE(DISP_OVL_SMI_CK, ovl_smi_ck, smi_sel, disp0_cg_regs, 5, 0),
	GATE(DISP_COLOR_DISP_CK, color_disp_ck, disp_sel, disp0_cg_regs, 6, 0),
	GATE(DISP_TDSHP_DISP_CK, tdshp_disp_ck, disp_sel, disp0_cg_regs, 7, 0),
	GATE(DISP_BLS_DISP_CK, bls_disp_ck, disp_sel, disp0_cg_regs, 8, 0),
	GATE(DISP_WDMA0_DISP_CK, wdma0_disp_ck, disp_sel, disp0_cg_regs, 9, 0),
	GATE(DISP_WDMA0_SMI_CK, wdma0_smi_ck, smi_sel, disp0_cg_regs, 10, 0),
	GATE(DISP_WDMA1_DISP_CK, wdma1_disp_ck, disp_sel, disp0_cg_regs, 11, 0),
	GATE(DISP_WDMA1_SMI_CK, wdma1_smi_ck, smi_sel, disp0_cg_regs, 12, 0),
	GATE(DISP_RDMA0_DISP_CK, rdma0_disp_ck, disp_sel, disp0_cg_regs, 13, 0),
	GATE(DISP_RDMA0_SMI_CK, rdma0_smi_ck, clk_null, disp0_cg_regs, 14, 0),
	GATE(DISP_RDMA0_OUTPUT_CK, rdma0_output_ck, clk_null,
		disp0_cg_regs, 15, 0),
	GATE(DISP_RDMA1_DISP_CK, rdma1_disp_ck, disp_sel, disp0_cg_regs, 16, 0),
	GATE(DISP_RDMA1_SMI_CK, rdma1_smi_ck, clk_null, disp0_cg_regs, 17, 0),
	GATE(DISP_RDMA1_OUTPUT_CK, rdma1_output_ck, clk_null,
		disp0_cg_regs, 18, 0),
	GATE(DISP_GAMMA_DISP_CK, gamma_disp_ck, disp_sel, disp0_cg_regs, 19, 0),
	GATE(DISP_GAMMA_PIXEL_CK, gamma_pixel_ck, clk_null,
		disp0_cg_regs, 20, 0),
	GATE(DISP_CMDQ_DISP_CK, cmdq_disp_ck, disp_sel, disp0_cg_regs, 21, 0),
	GATE(DISP_CMDQ_SMI_CK, cmdq_smi_ck, smi_sel, disp0_cg_regs, 22, 0),
	GATE(DISP_G2D_DISP_CK, g2d_disp_ck, disp_sel, disp0_cg_regs, 23, 0),
	GATE(DISP_G2D_SMI_CK, g2d_smi_ck, smi_sel, disp0_cg_regs, 24, 0),
	/* DISP1 */
	GATE(DISP_DSI_DISP_CK, dsi_disp_ck, disp_sel, disp1_cg_regs, 3, 0),
	GATE(DISP_DSI_DSI_CK, dsi_dsi_ck, dsi0_lntc_dsiclk,
		disp1_cg_regs, 4, 0),
	GATE(DISP_DSI_DIV2_DSI_CK, dsi_div2_dsi_ck, dsi0_lntc_dsiclk,
		disp1_cg_regs, 5, 0),
	GATE(DISP_DPI1_CK, dpi1_ck, dpi1_sel, disp1_cg_regs, 7, 0),
	GATE(DISP_LVDS_DISP_CK, lvds_disp_ck, vpll_dpix_ck,
		disp1_cg_regs, 10, 0),
	GATE(DISP_LVDS_CTS_CK, lvds_cts_ck, lvdstx_clkdig_cts,
		disp1_cg_regs, 11, 0),
	GATE(DISP_HDMI_DISP_CK, hdmi_disp_ck, dpi1_sel, disp1_cg_regs, 12, 0),
	GATE(DISP_HDMI_PLL_CK, hdmi_pll_ck, hdmipll_sel, disp1_cg_regs, 13, 0),
	GATE(DISP_HDMI_AUDIO_CK, hdmi_audio_ck, apll_sel, disp1_cg_regs, 14, 0),
	GATE(DISP_HDMI_SPDIF_CK, hdmi_spdif_ck, apll_sel, disp1_cg_regs, 15, 0),
	GATE(DISP_MUTEX_26M_CK, mutex_26m_ck, clk26m, disp1_cg_regs, 18, 0),
	GATE(DISP_UFO_DISP_CK, ufo_disp_ck, disp_sel, disp1_cg_regs, 19, 0),
};

static void __init init_clk_dispsys(void __iomem *dispsys_base,
		struct clk_onecell_data *clk_data)
{
	pr_debug("init dispsys gates:\n");
	init_clk_gates(dispsys_base, disp_clks, ARRAY_SIZE(disp_clks),
		clk_data);
}

static struct mtk_gate_regs audio_cg_regs = {
	.set_ofs = 0x0000,
	.clr_ofs = 0x0000,
	.sta_ofs = 0x0000,
};

static struct mtk_gate audio_clks[] __initdata = {
	GATE(AUDIO_AFE_CK, afe_ck, audio_sel,
		audio_cg_regs, 2, CLK_GATE_NO_SETCLR_REG),
	GATE(AUDIO_I2S_CK, i2s_ck, clk_null,
		audio_cg_regs, 6, CLK_GATE_NO_SETCLR_REG),
	GATE(AUDIO_APLL_TUNER_CK, apll_tuner_ck, apll_sel,
		audio_cg_regs, 19, CLK_GATE_NO_SETCLR_REG),
	GATE(AUDIO_HDMI_CK, hdmi_ck, apll_sel,
		audio_cg_regs, 20, CLK_GATE_NO_SETCLR_REG),
	GATE(AUDIO_SPDF_CK, spdf_ck, apll_sel,
		audio_cg_regs, 21, CLK_GATE_NO_SETCLR_REG),
};

static void __init init_clk_audiosys(void __iomem *audiosys_base,
		struct clk_onecell_data *clk_data)
{
	pr_debug("init audiosys gates:\n");
	init_clk_gates(audiosys_base, audio_clks, ARRAY_SIZE(audio_clks),
		clk_data);
}

/*
 * device tree support
 */

static struct clk_onecell_data *alloc_clk_data(unsigned int clk_num)
{
	int i;
	struct clk_onecell_data *clk_data;

	clk_data = kzalloc(sizeof(clk_data), GFP_KERNEL);
	if (!clk_data)
		return NULL;

	clk_data->clks = kcalloc(clk_num, sizeof(struct clk *), GFP_KERNEL);
	if (!clk_data->clks) {
		kfree(clk_data);
		return NULL;
	}

	clk_data->clk_num = clk_num;

	for (i = 0; i < clk_num; ++i)
		clk_data->clks[i] = ERR_PTR(-ENOENT);

	return clk_data;
}

static void __init mtk_topckgen_init(struct device_node *node)
{
	struct clk_onecell_data *clk_data;
	void __iomem *base;
	int r;

	pr_debug("%s: %s\n", __func__, node->name);

	base = of_iomap(node, 0);
	if (!base) {
		pr_err("ioremap topckgen failed\n");
		return;
	}

	clk_data = alloc_clk_data(TOP_NR_CLK);

	init_clk_root_alias(clk_data);
	init_clk_top_div(clk_data);
	init_clk_topckgen(base, clk_data);

	r = of_clk_add_provider(node, of_clk_src_onecell_get, clk_data);
	if (r)
		pr_err("could not register clock provide\n");
}
CLK_OF_DECLARE(mtk_topckgen, "mediatek,mt8135-topckgen", mtk_topckgen_init);

static void __init mtk_apmixedsys_init(struct device_node *node)
{
	struct clk_onecell_data *clk_data;
	void __iomem *base;
	int r;

	pr_debug("%s: %s\n", __func__, node->name);

	base = of_iomap(node, 0);
	if (!base) {
		pr_err("ioremap apmixedsys failed\n");
		return;
	}

	clk_data = alloc_clk_data(APMIXED_NR_CLK);

	init_clk_apmixedsys(base, clk_data);

	r = of_clk_add_provider(node, of_clk_src_onecell_get, clk_data);
	if (r)
		pr_err("could not register clock provide\n");
}
CLK_OF_DECLARE(mtk_apmixedsys, "mediatek,mt8135-apmixedsys",
		mtk_apmixedsys_init);

static void __init mtk_infrasys_init(struct device_node *node)
{
	struct clk_onecell_data *clk_data;
	void __iomem *base;
	int r;

	pr_debug("%s: %s\n", __func__, node->name);

	base = of_iomap(node, 0);
	if (!base) {
		pr_err("ioremap infrasys failed\n");
		return;
	}

	clk_data = alloc_clk_data(INFRA_NR_CLK);

	init_clk_infrasys(base, clk_data);

	r = of_clk_add_provider(node, of_clk_src_onecell_get, clk_data);
	if (r)
		pr_err("could not register clock provide\n");
}
CLK_OF_DECLARE(mtk_infrasys, "mediatek,mt8135-infracfg", mtk_infrasys_init);

static void __init mtk_perisys_init(struct device_node *node)
{
	struct clk_onecell_data *clk_data;
	void __iomem *base;
	int r;

	pr_debug("%s: %s\n", __func__, node->name);

	base = of_iomap(node, 0);
	if (!base) {
		pr_err("ioremap infrasys failed\n");
		return;
	}

	clk_data = alloc_clk_data(PERI_NR_CLK);

	init_clk_perisys(base, clk_data);

	r = of_clk_add_provider(node, of_clk_src_onecell_get, clk_data);
	if (r)
		pr_err("could not register clock provide\n");
}
CLK_OF_DECLARE(mtk_perisys, "mediatek,mt8135-pericfg", mtk_perisys_init);

static void __init mtk_mfgsys_init(struct device_node *node)
{
	struct clk_onecell_data *clk_data;
	void __iomem *base;
	int r;

	pr_debug("%s: %s\n", __func__, node->name);

	base = of_iomap(node, 0);
	if (!base) {
		pr_err("ioremap mfgsys failed\n");
		return;
	}

	clk_data = alloc_clk_data(MFG_NR_CLK);

	init_clk_mfgsys(base, clk_data);

	r = of_clk_add_provider(node, of_clk_src_onecell_get, clk_data);
	if (r)
		pr_err("could not register clock provide\n");
}
CLK_OF_DECLARE(mtk_mfgsys, "mediatek,mt8135-mfgsys", mtk_mfgsys_init);

static void __init mtk_imgsys_init(struct device_node *node)
{
	struct clk_onecell_data *clk_data;
	void __iomem *base;
	int r;

	pr_debug("%s: %s\n", __func__, node->name);

	base = of_iomap(node, 0);
	if (!base) {
		pr_err("ioremap imgsys failed\n");
		return;
	}

	clk_data = alloc_clk_data(IMG_NR_CLK);

	init_clk_imgsys(base, clk_data);

	r = of_clk_add_provider(node, of_clk_src_onecell_get, clk_data);
	if (r)
		pr_err("could not register clock provide\n");
}
CLK_OF_DECLARE(mtk_imgsys, "mediatek,mt8135-imgsys", mtk_imgsys_init);

static void __init mtk_vencsys_init(struct device_node *node)
{
	struct clk_onecell_data *clk_data;
	void __iomem *base;
	int r;

	pr_debug("%s: %s\n", __func__, node->name);

	base = of_iomap(node, 0);
	if (!base) {
		pr_err("ioremap vencsys failed\n");
		return;
	}

	clk_data = alloc_clk_data(VENC_NR_CLK);

	init_clk_vencsys(base, clk_data);

	r = of_clk_add_provider(node, of_clk_src_onecell_get, clk_data);
	if (r)
		pr_err("could not register clock provide\n");
}
CLK_OF_DECLARE(mtk_vencsys, "mediatek,mt8135-vencsys", mtk_vencsys_init);

static void __init mtk_vdecsys_init(struct device_node *node)
{
	struct clk_onecell_data *clk_data;
	void __iomem *base;
	int r;

	pr_debug("%s: %s\n", __func__, node->name);

	base = of_iomap(node, 0);
	if (!base) {
		pr_err("ioremap vdecsys failed\n");
		return;
	}

	clk_data = alloc_clk_data(VDEC_NR_CLK);

	init_clk_vdecsys(base, clk_data);

	r = of_clk_add_provider(node, of_clk_src_onecell_get, clk_data);
	if (r)
		pr_err("could not register clock provide\n");
}
CLK_OF_DECLARE(mtk_vdecsys, "mediatek,mt8135-vdecsys", mtk_vdecsys_init);

static void __init mtk_dispsys_init(struct device_node *node)
{
	struct clk_onecell_data *clk_data;
	void __iomem *base;
	int r;

	pr_debug("%s: %s\n", __func__, node->name);

	base = of_iomap(node, 0);
	if (!base) {
		pr_err("ioremap dispsys failed\n");
		return;
	}

	clk_data = alloc_clk_data(DISP_NR_CLK);

	init_clk_dispsys(base, clk_data);

	r = of_clk_add_provider(node, of_clk_src_onecell_get, clk_data);
	if (r)
		pr_err("could not register clock provide\n");
}
CLK_OF_DECLARE(mtk_dispsys, "mediatek,mt8135-dispsys", mtk_dispsys_init);

static void __init mtk_audiosys_init(struct device_node *node)
{
	struct clk_onecell_data *clk_data;
	void __iomem *base;
	int r;

	pr_debug("%s: %s\n", __func__, node->name);

	base = of_iomap(node, 0);
	if (!base) {
		pr_err("ioremap audiosys failed\n");
		return;
	}

	clk_data = alloc_clk_data(AUDIO_NR_CLK);

	init_clk_audiosys(base, clk_data);

	r = of_clk_add_provider(node, of_clk_src_onecell_get, clk_data);
	if (r)
		pr_err("could not register clock provide\n");
}
CLK_OF_DECLARE(mtk_audiosys, "mediatek,mt8135-audiosys", mtk_audiosys_init);
