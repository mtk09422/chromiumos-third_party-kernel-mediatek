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
