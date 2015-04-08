/*
 * Copyright (c) 2014 MediaTek Inc.
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

enum mtk_dsi_format {
	MTK_DSI_FORMAT_16P,
	MTK_DSI_FORMAT_18NP,
	MTK_DSI_FORMAT_18P,
	MTK_DSI_FORMAT_24P,
};


#define DSI_START		0x00
#define DSI_INTEN		0x08
#define DSI_INTSTA		0x0C
	#define RD_RDY				(1)
	#define CMD_DONE			(1<<1)
	#define TE_RDY				(1<<2)
	#define VM_DONE				(1<<3)
	#define EXT_TE				(1<<4)
	#define VM_CMD_DONE			(1<<5)
	#define BUSY				(1<<31)

#define	DSI_CON_CTRL		0x10
	#define DSI_RESET		(1)


#define DSI_MODE_CTRL 0x14
	#define  MODE		2
		#define CMD_MODE                  0
		#define SYNC_PULSE_MODE    1
		#define SYNC_EVENT_MODE    2
		#define BURST_MODE              3
	#define  FRM_MODE    (1<<16)
	#define  MIX_MODE     (1<<17)


#define DSI_TXRX_CTRL	0x18
	#define VC_NUM	(2<<0)
	#define LANE_NUM	(0xF<<2)
	#define DIS_EOT		(1<<6)
	#define NULL_EN		(1<<7)
	#define TE_FREERUN		(1<<8)
	#define EXT_TE_EN		(1<<9)
	#define EXT_TE_EDGE		(1<<10)
	#define MAX_RTN_SIZE		(0xF<<12)
	#define HSTX_CKLP_EN		(1<<16)



#define DSI_PSCTRL		0x1C
	#define DSI_PS_WC		0x3FFF
	#define DSI_PS_SEL	 (2<<16)
		#define PACKED_PS_16BIT_RGB565		(0<<16)
		#define LOOSELY_PS_18BIT_RGB666		(1<<16)
		#define PACKED_PS_18BIT_RGB666		(2<<16)
		#define PACKED_PS_24BIT_RGB888		(3<<16)


#define DSI_VSA_NL			0x20
#define DSI_VBP_NL			0x24
#define DSI_VFP_NL			0x28
#define DSI_VACT_NL		0x2C
#define DSI_HSA_WC		0x50
#define DSI_HBP_WC		0x54
#define DSI_HFP_WC		0x58
#define DSI_BLLP_WC		0x5C

#define			DSI_HSTX_CKL_WC	0x64


#define DSI_PHY_LCCON 0x104
	#define LC_HS_TX_EN					(1)
	#define LC_ULPM_EN					(1<<1)
	#define LC_WAKEUP_EN				(1<<2)

#define DSI_PHY_LD0CON 0x108
	#define LD0_HS_TX_EN				(1)
	#define LD0_ULPM_EN					(1<<1)
	#define LD0_WAKEUP_EN				(1<<2)

#define		MIPITX_DSI0_CON			0x00
    #define RG_DSI0_LDOCORE_EN					(1)
    #define RG_DSI0_CKG_LDOOUT_EN				(1<<1)
    #define RG_DSI0_BCLK_SEL					(3<<2)
    #define RG_DSI0_LD_IDX_SEL					(7<<4)
    #define RG_DSI0_PHYCLK_SEL					(2<<8)
    #define RG_DSI0_DSICLK_FREQ_SEL				(1<<10)
    #define RG_DSI0_LPTX_CLMP_EN				(1<<11)



#define		MIPITX_DSI0_CLOCK_LANE	0x04
    #define RG_DSI0_LNTC_LDOOUT_EN				(1)
    #define RG_DSI0_LNTC_CKLANE_EN				(1<<1)
    #define RG_DSI0_LNTC_LPTX_IPLUS1			(1<<2)
    #define RG_DSI0_LNTC_LPTX_IPLUS2			(1<<3)
    #define RG_DSI0_LNTC_LPTX_IMINUS			(1<<4)
    #define RG_DSI0_LNTC_LPCD_IPLUS				(1<<5)
    #define RG_DSI0_LNTC_LPCD_IMLUS				(1<<6)
    #define RG_DSI0_LNTC_RT_CODE				(0xF<<8)



#define		MIPITX_DSI0_DATA_LANE0	0x08
    #define RG_DSI0_LNT0_LDOOUT_EN				(1)
    #define RG_DSI0_LNT0_CKLANE_EN				(1<<1)
    #define RG_DSI0_LNT0_LPTX_IPLUS1			(1<<2)
    #define RG_DSI0_LNT0_LPTX_IPLUS2			(1<<3)
    #define RG_DSI0_LNT0_LPTX_IMINUS			(1<<4)
    #define RG_DSI0_LNT0_LPCD_IPLUS				(1<<5)
    #define RG_DSI0_LNT0_LPCD_IMINUS			(1<<6)
    #define RG_DSI0_LNT0_RT_CODE				(0xF<<8)


#define		MIPITX_DSI0_DATA_LANE1	0x0C
    #define RG_DSI0_LNT1_LDOOUT_EN				(1)
	#define RG_DSI0_LNT1_CKLANE_EN				(1<<1)
	#define RG_DSI0_LNT1_LPTX_IPLUS1			(1<<2)
	#define RG_DSI0_LNT1_LPTX_IPLUS2			(1<<3)
	#define RG_DSI0_LNT1_LPTX_IMINUS			(1<<4)
	#define RG_DSI0_LNT1_LPCD_IPLUS				(1<<5)
	#define RG_DSI0_LNT1_LPCD_IMINUS			(1<<6)
	#define RG_DSI0_LNT1_RT_CODE				(0xF<<8)


#define		MIPITX_DSI0_DATA_LANE2	0x10
    #define RG_DSI0_LNT2_LDOOUT_EN				(1)
	#define RG_DSI0_LNT2_CKLANE_EN				(1<<1)
	#define RG_DSI0_LNT2_LPTX_IPLUS1			(1<<2)
	#define RG_DSI0_LNT2_LPTX_IPLUS2			(1<<3)
	#define RG_DSI0_LNT2_LPTX_IMINUS			(1<<4)
	#define RG_DSI0_LNT2_LPCD_IPLUS				(1<<5)
	#define RG_DSI0_LNT2_LPCD_IMINUS			(1<<6)
	#define RG_DSI0_LNT2_RT_CODE				(0xF<<8)


#define		MIPITX_DSI0_DATA_LANE3	0x14
	#define RG_DSI0_LNT3_LDOOUT_EN				(1)
	#define RG_DSI0_LNT3_CKLANE_EN				(1<<1)
	#define RG_DSI0_LNT3_LPTX_IPLUS1			(1<<2)
	#define RG_DSI0_LNT3_LPTX_IPLUS2			(1<<3)
	#define RG_DSI0_LNT3_LPTX_IMINUS			(1<<4)
	#define RG_DSI0_LNT3_LPCD_IPLUS				(1<<5)
	#define RG_DSI0_LNT3_LPCD_IMINUS			(1<<6)
	#define RG_DSI0_LNT3_RT_CODE				(0xF<<8)



#define		MIPITX_DSI_TOP_CON	0x40
	#define RG_DSI_LNT_INTR_EN			(1)
	#define RG_DSI_LNT_HS_BIAS_EN			(1<<1)
	#define RG_DSI_LNT_IMP_CAL_EN			(1<<2)
	#define RG_DSI_LNT_TESTMODE_EN		(1<<3)
	#define RG_DSI_LNT_IMP_CAL_CODE		(0xF<<4)
	#define RG_DSI_LNT_AIO_SEL				(7<<8)
	#define RG_DSI_PAD_TIE_LOW_EN			(1<<11)
	#define RG_DSI_DEBUG_INPUT_EN			(1<<12)
	#define RG_DSI_PRESERVE			       (7<<13)

#define		MIPITX_DSI_BG_CON		0x44
	#define RG_DSI_BG_CORE_EN		1
	#define RG_DSI_BG_CKEN			(1<<1)
	#define RG_DSI_BG_DIV			(0x3<<2)
	#define RG_DSI_BG_FAST_CHARGE		(1<<4)
	#define RG_DSI_V12_SEL			 (7<<5)
	#define RG_DSI_V10_SEL			 (7<<8)
	#define RG_DSI_V072_SEL			 (7<<11)
	#define RG_DSI_V04_SEL			 (7<<14)
	#define RG_DSI_V032_SEL			 (7<<17)
	#define RG_DSI_V02_SEL			 (7<<20)
	#define rsv_23					 (1<<)
	#define RG_DSI_BG_R1_TRIM		 (0xF<<24)
	#define RG_DSI_BG_R2_TRIM		 (0xF<<28)


#define		MIPITX_DSI_PLL_CON0	0x50
    #define RG_DSI0_MPPLL_PLL_EN			(1<<0)
    #define RG_DSI0_MPPLL_PREDIV			(3<<1)
    #define RG_DSI0_MPPLL_TXDIV0			(3<<3)
    #define RG_DSI0_MPPLL_TXDIV1			(3<<5)
    #define RG_DSI0_MPPLL_POSDIV		(7<<7)
    #define RG_DSI0_MPPLL_MONVC_EN		(1<<10)
    #define RG_DSI0_MPPLL_MONREF_EN		(1<<11)
	#define RG_DSI0_MPPLL_VOD_EN		(1<<12)

#define		MIPITX_DSI_PLL_PWR		0x68
	#define RG_DSI_MPPLL_SDM_PWR_ON			(1<<0)
	#define RG_DSI_MPPLL_SDM_ISO_EN			(1<<1)
	#define RG_DSI_MPPLL_SDM_PWR_ACK		(1<<8)

#define	DSI_PHY_TIMECON0			0x0110
	#define LPX			(0xFF<<0)
	#define HS_PRPR		(0xFF<<8)
	#define HS_ZERO		(0xFF<<16)
	#define HS_TRAIL		(0xFF<<24)

#define	DSI_PHY_TIMECON1			0x0114
	#define TA_GO			(0xFF<<0)
	#define TA_SURE			(0xFF<<8)
	#define TA_GET			(0xFF<<16)
	#define DA_HS_EXIT		(0xFF<<24)


#define	DSI_PHY_TIMECON2			0x0118
	#define CONT_DET		(0xFF<<0)
	#define CLK_ZERO		(0xFF<<16)
	#define CLK_TRAIL		(0xFF<<24)


#define	DSI_PHY_TIMECON3			0x011C
	#define CLK_HS_PRPR		(0xFF<<0)
	#define CLK_HS_POST		(0xFF<<8)
	#define CLK_HS_EXIT		(0xFF<<16)




#define		MIPITX_DSI_PLL_CON1	0x54
    #define RG_DSI0_MPPLL_SDM_FRA_EN			(1)
    #define RG_DSI0_MPPLL_SDM_SSC_PH_INIT			(1<<1)
    #define RG_DSI0_MPPLL_SDM_SSC_EN			(1<<2)
    #define RG_DSI0_MPPLL_SDM_SSC_PRD			(0xFFFF<<16)



#define		MIPITX_DSI_PLL_CON2	0x58
#define		MIPITX_DSI_PLL_CON3	0x5C
#define		MIPITX_DSI_PLL_TOP	0x60
    #define RG_MPPLL_TST_EN			(1)
    #define RG_MPPLL_TSTCK_EN		(1<<1)
    #define RG_MPPLL_TSTSEL			(3<<2)
    #define RG_MPPLL_PRESERVE		(0xFF<<16)

#define		MIPITX_DSI_RGS			0x64
#define		MIPITX_DSI_GPIO_EN	0x68
#define		MIPITX_DSI_GPIO_OUT	0x6c

#define		MIPITX_DSI_SW_CTRL	0x80
	#define SW_CTRL_EN		(1<<0)

#define		MIPITX_DSI_SW_CTRL_CON0	0x84
    #define SW_LNTC_LPTX_PRE_OE		(1<<0)
    #define SW_LNTC_LPTX_OE			(1<<1)
    #define SW_LNTC_LPTX_P			(1<<2)
    #define SW_LNTC_LPTX_N			(1<<3)
    #define SW_LNTC_HSTX_PRE_OE		(1<<4)
    #define SW_LNTC_HSTX_OE			(1<<5)
    #define SW_LNTC_HSTX_ZEROCLK	(1<<6)
    #define SW_LNT0_LPTX_PRE_OE		(1<<7)
    #define SW_LNT0_LPTX_OE			(1<<8)
    #define SW_LNT0_LPTX_P			(1<<9)
    #define SW_LNT0_LPTX_N			(1<<10)
    #define SW_LNT0_HSTX_PRE_OE		(1<<11)
    #define SW_LNT0_HSTX_OE			(1<<12)
    #define SW_LNT0_LPRX_EN			(1<<13)
    #define SW_LNT1_LPTX_PRE_OE		(1<<14)
    #define SW_LNT1_LPTX_OE			(1<<15)
    #define SW_LNT1_LPTX_P			(1<<16)
    #define SW_LNT1_LPTX_N			(1<<17)
    #define SW_LNT1_HSTX_PRE_OE		(1<<18)
    #define SW_LNT1_HSTX_OE			(1<<19)
    #define SW_LNT2_LPTX_PRE_OE		(1<<20)
    #define SW_LNT2_LPTX_OE			(1<<21)
    #define SW_LNT2_LPTX_P			(1<<22)
    #define SW_LNT2_LPTX_N			(1<<23)
    #define SW_LNT2_HSTX_PRE_OE		(1<<24)
    #define SW_LNT2_HSTX_OE			(1<<25)


#define		MIPITX_DSI_SW_CTRL_CON1	0x88

#define		MIPITX_DSI0_MPPLL_SDM_CON1	0x90
	#define RG_DSI0_MPPLL_ISO_EN		(1)
	#define RG_BY_ISO_DLY		        (1<<1)
	#define RG_DSI0_MPPLL_PWR_ON		(1<<2)
	#define RG_BY_PWR_DLY	            (1<<3)
	#define RSV_ATPG_CON               (1<<4)
	#define DSI0_MPPLL_PWR_ACK         (1<<24)


#define		MIPITX_DSI0_MPPLL_SDM_CON2	0x94



struct mtk_dsi {
	struct drm_device *drm_dev;
	struct drm_encoder encoder;
	struct drm_connector conn;
	struct drm_panel *panel;
	struct mipi_dsi_host host;
	struct regulator *disp_supplies;

	void __iomem *dsi_reg_base;
	void __iomem *dsi_tx_reg_base;

	struct clk *dsi_disp_clk_cg;
	struct clk *dsi_dsi_clk_cg;
	struct clk *dsi_div2_clk_cg;

	struct clk *dsi0_engine_clk_cg;
	struct clk *dsi0_digital_clk_cg;

	u32 pll_clk_rate;

	unsigned long mode_flags;
	enum mipi_dsi_pixel_format format;
	unsigned int lanes;
	struct videomode vm;
	bool enabled;
};


static inline struct mtk_dsi *host_to_mtk(struct mipi_dsi_host *host)
{
	return container_of(host, struct mtk_dsi, host);
}

static inline struct mtk_dsi *encoder_to_dsi(struct drm_encoder *e)
{
	return container_of(e, struct mtk_dsi, encoder);
}

#define connector_to_dsi(c) container_of(c, struct mtk_dsi, conn)



