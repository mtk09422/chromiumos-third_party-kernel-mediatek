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
#include <dt-bindings/clock/mt8135-clk.h>

#include "clk-mtk.h"
#include "clk-gate.h"

static DEFINE_SPINLOCK(lock);

static const struct mtk_fixed_factor root_clk_alias[] __initconst = {
	FACTOR(TOP_DSI0_LNTC_DSICLK, "dsi0_lntc_dsiclk", "clk_null", 1, 1),
	FACTOR(TOP_HDMITX_CLKDIG_CTS, "hdmitx_clkdig_cts", "clk_null", 1, 1),
	FACTOR(TOP_CLKPH_MCK, "clkph_mck", "clk_null", 1, 1),
	FACTOR(TOP_CPUM_TCK_IN, "cpum_tck_in", "clk_null", 1, 1),
};

static const struct mtk_fixed_factor top_divs[] __initconst = {
	FACTOR(TOP_MAINPLL_806M, "mainpll_806m", "mainpll", 1, 2),
	FACTOR(TOP_MAINPLL_537P3M, "mainpll_537p3m", "mainpll", 1, 3),
	FACTOR(TOP_MAINPLL_322P4M, "mainpll_322p4m", "mainpll", 1, 5),
	FACTOR(TOP_MAINPLL_230P3M, "mainpll_230p3m", "mainpll", 1, 7),

	FACTOR(TOP_UNIVPLL_624M, "univpll_624m", "univpll", 1, 2),
	FACTOR(TOP_UNIVPLL_416M, "univpll_416m", "univpll", 1, 3),
	FACTOR(TOP_UNIVPLL_249P6M, "univpll_249p6m", "univpll", 1, 5),
	FACTOR(TOP_UNIVPLL_178P3M, "univpll_178p3m", "univpll", 1, 7),
	FACTOR(TOP_UNIVPLL_48M, "univpll_48m", "univpll", 1, 26),

	FACTOR(TOP_MMPLL_D2, "mmpll_d2", "mmpll", 1, 2),
	FACTOR(TOP_MMPLL_D3, "mmpll_d3", "mmpll", 1, 3),
	FACTOR(TOP_MMPLL_D5, "mmpll_d5", "mmpll", 1, 5),
	FACTOR(TOP_MMPLL_D7, "mmpll_d7", "mmpll", 1, 7),
	FACTOR(TOP_MMPLL_D4, "mmpll_d4", "mmpll_d2", 1, 2),
	FACTOR(TOP_MMPLL_D6, "mmpll_d6", "mmpll_d3", 1, 2),

	FACTOR(TOP_SYSPLL_D2, "syspll_d2", "mainpll_806m", 1, 1),
	FACTOR(TOP_SYSPLL_D4, "syspll_d4", "mainpll_806m", 1, 2),
	FACTOR(TOP_SYSPLL_D6, "syspll_d6", "mainpll_806m", 1, 3),
	FACTOR(TOP_SYSPLL_D8, "syspll_d8", "mainpll_806m", 1, 4),
	FACTOR(TOP_SYSPLL_D10, "syspll_d10", "mainpll_806m", 1, 5),
	FACTOR(TOP_SYSPLL_D12, "syspll_d12", "mainpll_806m", 1, 6),
	FACTOR(TOP_SYSPLL_D16, "syspll_d16", "mainpll_806m", 1, 8),
	FACTOR(TOP_SYSPLL_D24, "syspll_d24", "mainpll_806m", 1, 12),

	FACTOR(TOP_SYSPLL_D3, "syspll_d3", "mainpll_537p3m", 1, 1),

	FACTOR(TOP_SYSPLL_D2P5, "syspll_d2p5", "mainpll_322p4m", 2, 1),
	FACTOR(TOP_SYSPLL_D5, "syspll_d5", "mainpll_322p4m", 1, 1),

	FACTOR(TOP_SYSPLL_D3P5, "syspll_d3p5", "mainpll_230p3m", 2, 1),

	FACTOR(TOP_UNIVPLL1_D2, "univpll1_d2", "univpll_624m", 1, 2),
	FACTOR(TOP_UNIVPLL1_D4, "univpll1_d4", "univpll_624m", 1, 4),
	FACTOR(TOP_UNIVPLL1_D6, "univpll1_d6", "univpll_624m", 1, 6),
	FACTOR(TOP_UNIVPLL1_D8, "univpll1_d8", "univpll_624m", 1, 8),
	FACTOR(TOP_UNIVPLL1_D10, "univpll1_d10", "univpll_624m", 1, 10),

	FACTOR(TOP_UNIVPLL2_D2, "univpll2_d2", "univpll_416m", 1, 2),
	FACTOR(TOP_UNIVPLL2_D4, "univpll2_d4", "univpll_416m", 1, 4),
	FACTOR(TOP_UNIVPLL2_D6, "univpll2_d6", "univpll_416m", 1, 6),
	FACTOR(TOP_UNIVPLL2_D8, "univpll2_d8", "univpll_416m", 1, 8),

	FACTOR(TOP_UNIVPLL_D3, "univpll_d3", "univpll_416m", 1, 1),
	FACTOR(TOP_UNIVPLL_D5, "univpll_d5", "univpll_249p6m", 1, 1),
	FACTOR(TOP_UNIVPLL_D7, "univpll_d7", "univpll_178p3m", 1, 1),
	FACTOR(TOP_UNIVPLL_D10, "univpll_d10", "univpll_249p6m", 1, 5),
	FACTOR(TOP_UNIVPLL_D26, "univpll_d26", "univpll_48m", 1, 1),

	FACTOR(TOP_APLL_CK, "apll_ck", "audpll", 1, 1),
	FACTOR(TOP_APLL_D4, "apll_d4", "audpll", 1, 4),
	FACTOR(TOP_APLL_D8, "apll_d8", "audpll", 1, 8),
	FACTOR(TOP_APLL_D16, "apll_d16", "audpll", 1, 16),
	FACTOR(TOP_APLL_D24, "apll_d24", "audpll", 1, 24),

	FACTOR(TOP_LVDSPLL_D2, "lvdspll_d2", "lvdspll", 1, 2),
	FACTOR(TOP_LVDSPLL_D4, "lvdspll_d4", "lvdspll", 1, 4),
	FACTOR(TOP_LVDSPLL_D8, "lvdspll_d8", "lvdspll", 1, 8),

	FACTOR(TOP_LVDSTX_CLKDIG_CT, "lvdstx_clkdig_cts", "lvdspll", 1, 1),
	FACTOR(TOP_VPLL_DPIX_CK, "vpll_dpix_ck", "lvdspll", 1, 1),

	FACTOR(TOP_TVHDMI_H_CK, "tvhdmi_h_ck", "tvdpll", 1, 1),

	FACTOR(TOP_HDMITX_CLKDIG_D2, "hdmitx_clkdig_d2", "hdmitx_clkdig_cts", 1, 2),
	FACTOR(TOP_HDMITX_CLKDIG_D3, "hdmitx_clkdig_d3", "hdmitx_clkdig_cts", 1, 3),

	FACTOR(TOP_TVHDMI_D2, "tvhdmi_d2", "tvhdmi_h_ck", 1, 2),
	FACTOR(TOP_TVHDMI_D4, "tvhdmi_d4", "tvhdmi_h_ck", 1, 4),

	FACTOR(TOP_MEMPLL_MCK_D4, "mempll_mck_d4", "clkph_mck", 1, 4),
};

static const char * const axi_parents[] __initconst = {
	"clk26m",
	"syspll_d3",
	"syspll_d4",
	"syspll_d6",
	"univpll_d5",
	"univpll2_d2",
	"syspll_d3p5"
};

static const char * const smi_parents[] __initconst = {
	"clk26m",
	"clkph_mck",
	"syspll_d2p5",
	"syspll_d3",
	"syspll_d8",
	"univpll_d5",
	"univpll1_d2",
	"univpll1_d6",
	"mmpll_d3",
	"mmpll_d4",
	"mmpll_d5",
	"mmpll_d6",
	"mmpll_d7",
	"vdecpll",
	"lvdspll"
};

static const char * const mfg_parents[] __initconst = {
	"clk26m",
	"univpll1_d4",
	"syspll_d2",
	"syspll_d2p5",
	"syspll_d3",
	"univpll_d5",
	"univpll1_d2",
	"mmpll_d2",
	"mmpll_d3",
	"mmpll_d4",
	"mmpll_d5",
	"mmpll_d6",
	"mmpll_d7"
};

static const char * const irda_parents[] __initconst = {
	"clk26m",
	"univpll2_d8",
	"univpll1_d6"
};

static const char * const cam_parents[] __initconst = {
	"clk26m",
	"syspll_d3",
	"syspll_d3p5",
	"syspll_d4",
	"univpll_d5",
	"univpll2_d2",
	"univpll_d7",
	"univpll1_d4"
};

static const char * const aud_intbus_parents[] __initconst = {
	"clk26m",
	"syspll_d6",
	"univpll_d10"
};

static const char * const jpg_parents[] __initconst = {
	"clk26m",
	"syspll_d5",
	"syspll_d4",
	"syspll_d3",
	"univpll_d7",
	"univpll2_d2",
	"univpll_d5"
};

static const char * const disp_parents[] __initconst = {
	"clk26m",
	"syspll_d3p5",
	"syspll_d3",
	"univpll2_d2",
	"univpll_d5",
	"univpll1_d2",
	"lvdspll",
	"vdecpll"
};

static const char * const msdc30_parents[] __initconst = {
	"clk26m",
	"syspll_d6",
	"syspll_d5",
	"univpll1_d4",
	"univpll2_d4",
	"msdcpll"
};

static const char * const usb20_parents[] __initconst = {
	"clk26m",
	"univpll2_d6",
	"univpll1_d10"
};

static const char * const venc_parents[] __initconst = {
	"clk26m",
	"syspll_d3",
	"syspll_d8",
	"univpll_d5",
	"univpll1_d6",
	"mmpll_d4",
	"mmpll_d5",
	"mmpll_d6"
};

static const char * const spi_parents[] __initconst = {
	"clk26m",
	"syspll_d6",
	"syspll_d8",
	"syspll_d10",
	"univpll1_d6",
	"univpll1_d8"
};

static const char * const uart_parents[] __initconst = {
	"clk26m",
	"univpll2_d8"
};

static const char * const mem_parents[] __initconst = {
	"clk26m",
	"clkph_mck"
};

static const char * const camtg_parents[] __initconst = {
	"clk26m",
	"univpll_d26",
	"univpll1_d6",
	"syspll_d16",
	"syspll_d8"
};

static const char * const audio_parents[] __initconst = {
	"clk26m",
	"syspll_d24"
};

static const char * const fix_parents[] __initconst = {
	"rtc32k",
	"clk26m",
	"univpll_d5",
	"univpll_d7",
	"univpll1_d2",
	"univpll1_d4",
	"univpll1_d6",
	"univpll1_d8"
};

static const char * const vdec_parents[] __initconst = {
	"clk26m",
	"vdecpll",
	"clkph_mck",
	"syspll_d2p5",
	"syspll_d3",
	"syspll_d3p5",
	"syspll_d4",
	"syspll_d5",
	"syspll_d6",
	"syspll_d8",
	"univpll1_d2",
	"univpll2_d2",
	"univpll_d7",
	"univpll_d10",
	"univpll2_d4",
	"lvdspll"
};

static const char * const ddrphycfg_parents[] __initconst = {
	"clk26m",
	"axi_sel",
	"syspll_d12"
};

static const char * const dpilvds_parents[] __initconst = {
	"clk26m",
	"lvdspll",
	"lvdspll_d2",
	"lvdspll_d4",
	"lvdspll_d8"
};

static const char * const pmicspi_parents[] __initconst = {
	"clk26m",
	"univpll2_d6",
	"syspll_d8",
	"syspll_d10",
	"univpll1_d10",
	"mempll_mck_d4",
	"univpll_d26",
	"syspll_d24"
};

static const char * const smi_mfg_as_parents[] __initconst = {
	"clk26m",
	"smi_sel",
	"mfg_sel",
	"mem_sel"
};

static const char * const gcpu_parents[] __initconst = {
	"clk26m",
	"syspll_d4",
	"univpll_d7",
	"syspll_d5",
	"syspll_d6"
};

static const char * const dpi1_parents[] __initconst = {
	"clk26m",
	"tvhdmi_h_ck",
	"tvhdmi_d2",
	"tvhdmi_d4"
};

static const char * const cci_parents[] __initconst = {
	"clk26m",
	"mainpll_537p3m",
	"univpll_d3",
	"syspll_d2p5",
	"syspll_d3",
	"syspll_d5"
};

static const char * const apll_parents[] __initconst = {
	"clk26m",
	"apll_ck",
	"apll_d4",
	"apll_d8",
	"apll_d16",
	"apll_d24"
};

static const char * const hdmipll_parents[] __initconst = {
	"clk26m",
	"hdmitx_clkdig_cts",
	"hdmitx_clkdig_d2",
	"hdmitx_clkdig_d3"
};

static const struct mtk_composite top_muxes[] __initconst = {
	/* CLK_CFG_0 */
	MUX_GATE(TOP_AXI_SEL, "axi_sel", axi_parents,
		0x0140, 0, 3, INVALID_MUX_GATE_BIT),
	MUX_GATE(TOP_SMI_SEL, "smi_sel", smi_parents, 0x0140, 8, 4, 15),
	MUX_GATE(TOP_MFG_SEL, "mfg_sel", mfg_parents, 0x0140, 16, 4, 23),
	MUX_GATE(TOP_IRDA_SEL, "irda_sel", irda_parents, 0x0140, 24, 2, 31),
	/* CLK_CFG_1 */
	MUX_GATE(TOP_CAM_SEL, "cam_sel", cam_parents, 0x0144, 0, 3, 7),
	MUX_GATE(TOP_AUD_INTBUS_SEL, "aud_intbus_sel", aud_intbus_parents,
		0x0144, 8, 2, 15),
	MUX_GATE(TOP_JPG_SEL, "jpg_sel", jpg_parents, 0x0144, 16, 3, 23),
	MUX_GATE(TOP_DISP_SEL, "disp_sel", disp_parents, 0x0144, 24, 3, 31),
	/* CLK_CFG_2 */
	MUX_GATE(TOP_MSDC30_1_SEL, "msdc30_1_sel", msdc30_parents, 0x0148, 0, 3, 7),
	MUX_GATE(TOP_MSDC30_2_SEL, "msdc30_2_sel", msdc30_parents, 0x0148, 8, 3, 15),
	MUX_GATE(TOP_MSDC30_3_SEL, "msdc30_3_sel", msdc30_parents, 0x0148, 16, 3, 23),
	MUX_GATE(TOP_MSDC30_4_SEL, "msdc30_4_sel", msdc30_parents, 0x0148, 24, 3, 31),
	/* CLK_CFG_3 */
	MUX_GATE(TOP_USB20_SEL, "usb20_sel", usb20_parents, 0x014c, 0, 2, 7),
	/* CLK_CFG_4 */
	MUX_GATE(TOP_VENC_SEL, "venc_sel", venc_parents, 0x0150, 8, 3, 15),
	MUX_GATE(TOP_SPI_SEL, "spi_sel", spi_parents, 0x0150, 16, 3, 23),
	MUX_GATE(TOP_UART_SEL, "uart_sel", uart_parents, 0x0150, 24, 2, 31),
	/* CLK_CFG_6 */
	MUX_GATE(TOP_MEM_SEL, "mem_sel", mem_parents, 0x0158, 0, 2, 7),
	MUX_GATE(TOP_CAMTG_SEL, "camtg_sel", camtg_parents, 0x0158, 8, 3, 15),
	MUX_GATE(TOP_AUDIO_SEL, "audio_sel", audio_parents, 0x0158, 24, 2, 31),
	/* CLK_CFG_7 */
	MUX_GATE(TOP_FIX_SEL, "fix_sel", fix_parents, 0x015c, 0, 3, 7),
	MUX_GATE(TOP_VDEC_SEL, "vdec_sel", vdec_parents, 0x015c, 8, 4, 15),
	MUX_GATE(TOP_DDRPHYCFG_SEL, "ddrphycfg_sel", ddrphycfg_parents,
		0x015c, 16, 2, 23),
	MUX_GATE(TOP_DPILVDS_SEL, "dpilvds_sel", dpilvds_parents, 0x015c, 24, 3, 31),
	/* CLK_CFG_8 */
	MUX_GATE(TOP_PMICSPI_SEL, "pmicspi_sel", pmicspi_parents, 0x0164, 0, 3, 7),
	MUX_GATE(TOP_MSDC30_0_SEL, "msdc30_0_sel", msdc30_parents, 0x0164, 8, 3, 15),
	MUX_GATE(TOP_SMI_MFG_AS_SEL, "smi_mfg_as_sel", smi_mfg_as_parents,
		0x0164, 16, 2, 23),
	MUX_GATE(TOP_GCPU_SEL, "gcpu_sel", gcpu_parents, 0x0164, 24, 3, 31),
	/* CLK_CFG_9 */
	MUX_GATE(TOP_DPI1_SEL, "dpi1_sel", dpi1_parents, 0x0168, 0, 2, 7),
	MUX_GATE(TOP_CCI_SEL, "cci_sel", cci_parents, 0x0168, 8, 3, 15),
	MUX_GATE(TOP_APLL_SEL, "apll_sel", apll_parents, 0x0168, 16, 3, 23),
	MUX_GATE(TOP_HDMIPLL_SEL, "hdmipll_sel", hdmipll_parents, 0x0168, 24, 2, 31),
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
	GATE_ICG(INFRA_PMIC_WRAP_CK, "pmic_wrap_ck", "axi_sel", 23),
	GATE_ICG(INFRA_PMICSPI_CK, "pmicspi_ck", "pmicspi_sel", 22),
	GATE_ICG(INFRA_CCIF1_AP_CTRL, "ccif1_ap_ctrl", "axi_sel", 21),
	GATE_ICG(INFRA_CCIF0_AP_CTRL, "ccif0_ap_ctrl", "axi_sel", 20),
	GATE_ICG(INFRA_KP_CK, "kp_ck", "axi_sel", 16),
	GATE_ICG(INFRA_CPUM_CK, "cpum_ck", "cpum_tck_in", 15),
	GATE_ICG(INFRA_M4U_CK, "m4u_ck", "mem_sel", 8),
	GATE_ICG(INFRA_MFGAXI_CK, "mfgaxi_ck", "axi_sel", 7),
	GATE_ICG(INFRA_DEVAPC_CK, "devapc_ck", "axi_sel", 6),
	GATE_ICG(INFRA_AUDIO_CK, "audio_ck", "aud_intbus_sel", 5),
	GATE_ICG(INFRA_MFG_BUS_CK, "mfg_bus_ck", "axi_sel", 2),
	GATE_ICG(INFRA_SMI_CK, "smi_ck", "smi_sel", 1),
	GATE_ICG(INFRA_DBGCLK_CK, "dbgclk_ck", "axi_sel", 0),
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
	GATE_PERI0(PERI_I2C5_CK, "i2c5_ck", "axi_sel", 31),
	GATE_PERI0(PERI_I2C4_CK, "i2c4_ck", "axi_sel", 30),
	GATE_PERI0(PERI_I2C3_CK, "i2c3_ck", "axi_sel", 29),
	GATE_PERI0(PERI_I2C2_CK, "i2c2_ck", "axi_sel", 28),
	GATE_PERI0(PERI_I2C1_CK, "i2c1_ck", "axi_sel", 27),
	GATE_PERI0(PERI_I2C0_CK, "i2c0_ck", "axi_sel", 26),
	GATE_PERI0(PERI_UART3_CK, "uart3_ck", "axi_sel", 25),
	GATE_PERI0(PERI_UART2_CK, "uart2_ck", "axi_sel", 24),
	GATE_PERI0(PERI_UART1_CK, "uart1_ck", "axi_sel", 23),
	GATE_PERI0(PERI_UART0_CK, "uart0_ck", "axi_sel", 22),
	GATE_PERI0(PERI_IRDA_CK, "irda_ck", "irda_sel", 21),
	GATE_PERI0(PERI_NLI_CK, "nli_ck", "axi_sel", 20),
	GATE_PERI0(PERI_MD_HIF_CK, "md_hif_ck", "axi_sel", 19),
	GATE_PERI0(PERI_AP_HIF_CK, "ap_hif_ck", "axi_sel", 18),
	GATE_PERI0(PERI_MSDC30_3_CK, "msdc30_3_ck", "msdc30_4_sel", 17),
	GATE_PERI0(PERI_MSDC30_2_CK, "msdc30_2_ck", "msdc30_3_sel", 16),
	GATE_PERI0(PERI_MSDC30_1_CK, "msdc30_1_ck", "msdc30_2_sel", 15),
	GATE_PERI0(PERI_MSDC20_2_CK, "msdc20_2_ck", "msdc30_1_sel", 14),
	GATE_PERI0(PERI_MSDC20_1_CK, "msdc20_1_ck", "msdc30_0_sel", 13),
	GATE_PERI0(PERI_AP_DMA_CK, "ap_dma_ck", "axi_sel", 12),
	GATE_PERI0(PERI_USB1_CK, "usb1_ck", "usb20_sel", 11),
	GATE_PERI0(PERI_USB0_CK, "usb0_ck", "usb20_sel", 10),
	GATE_PERI0(PERI_PWM_CK, "pwm_ck", "axi_sel", 9),
	GATE_PERI0(PERI_PWM7_CK, "pwm7_ck", "axi_sel", 8),
	GATE_PERI0(PERI_PWM6_CK, "pwm6_ck", "axi_sel", 7),
	GATE_PERI0(PERI_PWM5_CK, "pwm5_ck", "axi_sel", 6),
	GATE_PERI0(PERI_PWM4_CK, "pwm4_ck", "axi_sel", 5),
	GATE_PERI0(PERI_PWM3_CK, "pwm3_ck", "axi_sel", 4),
	GATE_PERI0(PERI_PWM2_CK, "pwm2_ck", "axi_sel", 3),
	GATE_PERI0(PERI_PWM1_CK, "pwm1_ck", "axi_sel", 2),
	GATE_PERI0(PERI_THERM_CK, "therm_ck", "axi_sel", 1),
	GATE_PERI0(PERI_NFI_CK, "nfi_ck", "axi_sel", 0),
	/* PERI1 */
	GATE_PERI1(PERI_USBSLV_CK, "usbslv_ck", "axi_sel", 8),
	GATE_PERI1(PERI_USB1_MCU_CK, "usb1_mcu_ck", "axi_sel", 7),
	GATE_PERI1(PERI_USB0_MCU_CK, "usb0_mcu_ck", "axi_sel", 6),
	GATE_PERI1(PERI_GCPU_CK, "gcpu_ck", "gcpu_sel", 5),
	GATE_PERI1(PERI_FHCTL_CK, "fhctl_ck", "clk26m", 4),
	GATE_PERI1(PERI_SPI1_CK, "spi1_ck", "spi_sel", 3),
	GATE_PERI1(PERI_AUXADC_CK, "auxadc_ck", "clk26m", 2),
	GATE_PERI1(PERI_PERI_PWRAP_CK, "peri_pwrap_ck", "axi_sel", 1),
	GATE_PERI1(PERI_I2C6_CK, "i2c6_ck", "axi_sel", 0),
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
CLK_OF_DECLARE(mtk_topckgen, "mediatek,mt8135-topckgen", mtk_topckgen_init);

static void __init mtk_infrasys_init(struct device_node *node)
{
	struct clk_onecell_data *clk_data;
	int r;

	clk_data = mtk_alloc_clk_data(INFRA_NR_CLK);

	mtk_clk_register_gates(node, infra_clks, ARRAY_SIZE(infra_clks),
						clk_data);

	r = of_clk_add_provider(node, of_clk_src_onecell_get, clk_data);
	if (r)
		pr_err("%s(): could not register clock provider: %d\n",
			__func__, r);

	mtk_register_reset_controller(node, 2, 0x30);
}
CLK_OF_DECLARE(mtk_infrasys, "mediatek,mt8135-infracfg", mtk_infrasys_init);

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
CLK_OF_DECLARE(mtk_pericfg, "mediatek,mt8135-pericfg", mtk_pericfg_init);

#define MT8135_PLL_FMAX		(2000 * MHZ)
#define CON0_MT8135_RST_BAR	BIT(27)

#define PLL(_id, _name, _reg, _pwr_reg, _en_mask, _flags, _pcwbits, _pd_reg, _pd_shift, _tuner_reg, _pcw_reg, _pcw_shift) { \
		.id = _id,						\
		.name = _name,						\
		.reg = _reg,						\
		.pwr_reg = _pwr_reg,					\
		.en_mask = _en_mask,					\
		.flags = _flags,					\
		.rst_bar_mask = CON0_MT8135_RST_BAR,			\
		.fmax = MT8135_PLL_FMAX,				\
		.pcwbits = _pcwbits,					\
		.pd_reg = _pd_reg,					\
		.pd_shift = _pd_shift,					\
		.tuner_reg = _tuner_reg,				\
		.pcw_reg = _pcw_reg,					\
		.pcw_shift = _pcw_shift,				\
	}

static const struct mtk_pll_data plls[] = {
	PLL(APMIXED_ARMPLL1, "armpll1", 0x200, 0x218, 0x80000001, 0, 21, 0x204, 24, 0x0, 0x204, 0),
	PLL(APMIXED_ARMPLL2, "armpll2", 0x2cc, 0x2e4, 0x80000001, 0, 21, 0x2d0, 24, 0x0, 0x2d0, 0),
	PLL(APMIXED_MAINPLL, "mainpll", 0x21c, 0x234, 0xf0000001, HAVE_RST_BAR, 21, 0x21c, 6, 0x0, 0x220, 0),
	PLL(APMIXED_UNIVPLL, "univpll", 0x238, 0x250, 0xf3000001, HAVE_RST_BAR, 7, 0x238, 6, 0x0, 0x238, 9),
	PLL(APMIXED_MMPLL, "mmpll", 0x254, 0x26c, 0xf0000001, HAVE_RST_BAR, 21, 0x254, 6, 0x0, 0x258, 0),
	PLL(APMIXED_MSDCPLL, "msdcpll", 0x278, 0x290, 0x80000001, 0, 21, 0x278, 6, 0x0, 0x27c, 0),
	PLL(APMIXED_TVDPLL, "tvdpll", 0x294, 0x2ac, 0x80000001, 0, 31, 0x294, 6, 0x0, 0x298, 0),
	PLL(APMIXED_LVDSPLL, "lvdspll", 0x2b0, 0x2c8,	0x80000001, 0, 21, 0x2b0, 6, 0x0, 0x2b4, 0),
	PLL(APMIXED_AUDPLL, "audpll", 0x2e8, 0x300, 0x80000001, 0, 31, 0x2e8, 6, 0x2f8, 0x2ec, 0),
	PLL(APMIXED_VDECPLL, "vdecpll", 0x304, 0x31c,	0x80000001, 0, 21, 0x2b0, 6, 0x0, 0x308, 0),
};

static void __init mtk_apmixedsys_init(struct device_node *node)
{
	struct clk_onecell_data *clk_data;

	clk_data = mtk_alloc_clk_data(ARRAY_SIZE(plls));
	if (!clk_data)
		return;

	mtk_clk_register_plls(node, plls, ARRAY_SIZE(plls), clk_data);
}
CLK_OF_DECLARE(mtk_apmixedsys, "mediatek,mt8135-apmixedsys",
		mtk_apmixedsys_init);
