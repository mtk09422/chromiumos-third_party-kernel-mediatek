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

#include <drm/drmP.h>
#include <drm/drm_gem.h>
#include <drm/drm_crtc_helper.h>

#include <linux/clk.h>
#include <linux/debugfs.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/reset.h>

#include <linux/of_gpio.h>
#include <linux/gpio.h>

#include <linux/gpio/consumer.h>
#include <linux/module.h>
#include <linux/of_platform.h>

#include <linux/regulator/consumer.h>

#include <drm/drm_mipi_dsi.h>
#include <drm/drm_panel.h>

#include <video/mipi_display.h>
#include <video/videomode.h>

#include "mediatek_drm_output.h"
#include "mediatek_drm_drv.h"
#include "mediatek_drm_crtc.h"

#include "mediatek_drm_ddp.h"

#include "mediatek_drm_gem.h"
#include "mediatek_drm_dev_if.h"
#include "mediatek_drm_dsi.h"




#define DSI_VIDEO_FIFO_DEPTH (1920 / 4)
#define DSI_HOST_FIFO_DEPTH 64

struct mtk_dsi {
	struct mtk_output output;
	struct device *dev;

	void __iomem *dsi_reg_base;
	void __iomem *dsi_tx_reg_base;

	struct clk *dsi_disp_clk_cg;
	struct clk *dsi_dsi_clk_cg;
	struct clk *dsi_div2_clk_cg;

	struct clk *dsi0_engine_clk_cg;
	struct clk *dsi0_digital_clk_cg;


	struct drm_minor *minor;

	u32 pll_clk_rate;
	unsigned long mode_flags;
	enum mipi_dsi_pixel_format format;
	unsigned int lanes;
	struct videomode vm;
	struct mipi_dsi_host host;


	bool enabled;

/*  temp test  */
	unsigned int it6151_rst;
	unsigned int bl_en_gpio;
	unsigned int it6151_pwr1v2_gpio;
	unsigned int bl_pwmasr_gpio;
};

static inline struct mtk_dsi *host_to_mtk(struct mipi_dsi_host *host)
{
	return container_of(host, struct mtk_dsi, host);
}

static inline struct mtk_dsi *to_dsi(struct mtk_output *output)
{
	return container_of(output, struct mtk_dsi, output);
}

static inline unsigned long mtk_dsi_readl(struct mtk_dsi *dsi,
	unsigned long reg)
{
	return readl(dsi->dsi_reg_base + (reg << 2));
}

static inline void mtk_dsi_writel(struct mtk_dsi *dsi, unsigned long value,
				    unsigned long reg)
{
	writel(value, dsi->dsi_reg_base + (reg << 2));
}

#if 0
def CONFIG_DRM_MEDIATEK_IT6151
static int mtk_dsi_of_read_u32(const struct device_node *np,
				  const char *propname, u32 *out_value)
{
	int ret = of_property_read_u32(np, propname, out_value);

	if (ret < 0)
		DRM_ERROR("%s: failed to get '%s' property\n", np->full_name,
		       propname);

	return ret;
}
#endif

void DSI_PHY_clk_switch(struct mtk_dsi *dsi, bool on)
{

	u32 tmp_reg;

	if (on) {

		tmp_reg = readl(dsi->dsi_tx_reg_base + MIPITX_DSI_SW_CTRL);
		tmp_reg = (tmp_reg & (~SW_CTRL_EN));
		writel(tmp_reg, dsi->dsi_tx_reg_base + MIPITX_DSI_SW_CTRL);
	} else {

return;
		tmp_reg = readl(dsi->dsi_tx_reg_base + MIPITX_DSI_SW_CTRL_CON0);

		tmp_reg = tmp_reg  | (SW_LNTC_LPTX_PRE_OE | SW_LNTC_LPTX_OE |
			SW_LNTC_HSTX_PRE_OE | SW_LNTC_HSTX_OE |
			SW_LNT0_LPTX_PRE_OE | SW_LNT0_LPTX_OE |
			SW_LNT0_HSTX_PRE_OE | SW_LNT0_HSTX_OE |
			SW_LNT1_LPTX_PRE_OE | SW_LNT1_LPTX_OE |
			SW_LNT1_HSTX_PRE_OE | SW_LNT1_HSTX_OE |
			SW_LNT2_LPTX_PRE_OE | SW_LNT2_LPTX_OE |
			SW_LNT2_HSTX_PRE_OE | SW_LNT2_HSTX_OE);
		writel(tmp_reg, dsi->dsi_tx_reg_base + MIPITX_DSI_SW_CTRL_CON0);



		tmp_reg = readl(dsi->dsi_tx_reg_base + MIPITX_DSI_SW_CTRL);
		tmp_reg = (tmp_reg | SW_CTRL_EN);
		writel(tmp_reg, dsi->dsi_tx_reg_base + MIPITX_DSI_SW_CTRL);


		tmp_reg = readl(dsi->dsi_tx_reg_base + MIPITX_DSI_PLL_CON0);
		tmp_reg = (tmp_reg & (~RG_DSI0_MPPLL_PLL_EN));
		writel(tmp_reg, dsi->dsi_tx_reg_base + MIPITX_DSI_PLL_CON0);


		udelay(100);

		tmp_reg = readl(dsi->dsi_tx_reg_base + MIPITX_DSI_TOP_CON);
		tmp_reg = (tmp_reg & (~(RG_DSI_LNT_HS_BIAS_EN |
			RG_DSI_LNT_IMP_CAL_EN |
			RG_DSI_LNT_TESTMODE_EN)));
		writel(tmp_reg, dsi->dsi_tx_reg_base + MIPITX_DSI_TOP_CON);

		tmp_reg = readl(dsi->dsi_tx_reg_base + MIPITX_DSI0_CLOCK_LANE);
		tmp_reg = tmp_reg & (~RG_DSI0_LNTC_LDOOUT_EN);
		writel(tmp_reg, dsi->dsi_tx_reg_base + MIPITX_DSI0_CLOCK_LANE);

		tmp_reg = readl(dsi->dsi_tx_reg_base + MIPITX_DSI0_DATA_LANE0);
		tmp_reg = tmp_reg & (~RG_DSI0_LNT0_LDOOUT_EN);
		writel(tmp_reg, dsi->dsi_tx_reg_base + MIPITX_DSI0_DATA_LANE0);

		tmp_reg = readl(dsi->dsi_tx_reg_base + MIPITX_DSI0_DATA_LANE1);
		tmp_reg = tmp_reg & (~RG_DSI0_LNT1_LDOOUT_EN);
		writel(tmp_reg, dsi->dsi_tx_reg_base + MIPITX_DSI0_DATA_LANE1);

		tmp_reg = readl(dsi->dsi_tx_reg_base + MIPITX_DSI0_DATA_LANE2);
		tmp_reg = tmp_reg & (~RG_DSI0_LNT2_LDOOUT_EN);
		writel(tmp_reg, dsi->dsi_tx_reg_base + MIPITX_DSI0_DATA_LANE2);

		tmp_reg = readl(dsi->dsi_tx_reg_base + MIPITX_DSI0_DATA_LANE3);
		tmp_reg = tmp_reg & (~RG_DSI0_LNT3_LDOOUT_EN);
		writel(tmp_reg, dsi->dsi_tx_reg_base + MIPITX_DSI0_DATA_LANE3);


		mdelay(1);

		tmp_reg = readl(dsi->dsi_tx_reg_base + MIPITX_DSI0_CON);
		tmp_reg = tmp_reg & (~(RG_DSI0_CKG_LDOOUT_EN |
			RG_DSI0_LDOCORE_EN));
		writel(tmp_reg, dsi->dsi_tx_reg_base + MIPITX_DSI0_CON);

		tmp_reg = readl(dsi->dsi_tx_reg_base + MIPITX_DSI_BG_CON);
		tmp_reg = tmp_reg & (~(RG_DSI_BG_CKEN | RG_DSI_BG_CORE_EN));
		writel(tmp_reg, dsi->dsi_tx_reg_base + MIPITX_DSI_BG_CON);


		mdelay(1);
	}

	if (on) {
		tmp_reg = readl(dsi->dsi_tx_reg_base + MIPITX_DSI_PLL_CON0);
		tmp_reg = (tmp_reg | RG_DSI0_MPPLL_PLL_EN);
		writel(tmp_reg, dsi->dsi_tx_reg_base + MIPITX_DSI_PLL_CON0);
	} else {
		tmp_reg = readl(dsi->dsi_tx_reg_base + MIPITX_DSI_PLL_CON0);
		tmp_reg = (tmp_reg & (~RG_DSI0_MPPLL_PLL_EN));
		writel(tmp_reg, dsi->dsi_tx_reg_base + MIPITX_DSI_PLL_CON0);
	}
}

#if 0
void DSI_PHY_clk_setting(struct mtk_dsi *dsi)
{
	u32 reg, txdiv = 0, pcw = 0;
	unsigned int data_Rate = dsi->pll_clk_rate*2;



	reg = readl(dsi->dsi_tx_reg_base + MIPITX_DSI_TOP_CON);
	reg = (reg & (~RG_DSI_LNT_IMP_CAL_CODE)) | (8<<4);
	reg = (reg & (~RG_DSI_LNT_HS_BIAS_EN)) | (1<<1);
	writel(reg, dsi->dsi_tx_reg_base + MIPITX_DSI_TOP_CON);

	reg = readl(dsi->dsi_tx_reg_base + MIPITX_DSI_BG_CON);
	reg = (reg & (~RG_DSI_V032_SEL)) | (4<<17);
	reg = (reg & (~RG_DSI_V04_SEL)) | (4<<14);
	reg = (reg & (~RG_DSI_V072_SEL)) | (4<<11);
	reg = (reg & (~RG_DSI_V10_SEL)) | (4<<8);
	reg = (reg & (~RG_DSI_V12_SEL)) | (4<<5);
	reg = (reg & (~RG_DSI_BG_CKEN)) | (1<<1);
	reg = (reg & (~RG_DSI_BG_CORE_EN)) | (1);
	writel(reg, dsi->dsi_tx_reg_base + MIPITX_DSI_BG_CON);
	mdelay(10);

	reg = readl(dsi->dsi_tx_reg_base + MIPITX_DSI0_CON);
	reg = (reg & (~RG_DSI0_CKG_LDOOUT_EN)) | (1<<1);
	reg = (reg & (~RG_DSI0_LDOCORE_EN)) | (1);
	writel(reg, dsi->dsi_tx_reg_base + MIPITX_DSI0_CON);

	reg = readl(dsi->dsi_tx_reg_base + MIPITX_DSI0_MPPLL_SDM_CON1);
	reg = (reg & (~RG_DSI0_MPPLL_PWR_ON)) | (1<<2);
	reg = (reg & (~RG_DSI0_MPPLL_ISO_EN)) | (1);
	writel(reg, dsi->dsi_tx_reg_base + MIPITX_DSI0_MPPLL_SDM_CON1);
	mdelay(1);

	reg = (reg & (~RG_DSI0_MPPLL_ISO_EN));
	writel(reg, dsi->dsi_tx_reg_base + MIPITX_DSI0_MPPLL_SDM_CON1);

	if (0 != data_Rate) {
		if (data_Rate > 1250)
			udelay(1);
		else if (data_Rate >= 500)
			txdiv = 1;
		else if (data_Rate >= 250)
			txdiv = 2;
		else if (data_Rate >= 125)
			txdiv = 4;
		else if (data_Rate > 62)
			txdiv = 8;
		else if (data_Rate >= 50)
			txdiv = 16;
		else
			udelay(1);

	}

		reg = readl(dsi->dsi_tx_reg_base + MIPITX_DSI_PLL_CON0);

	switch (txdiv) {
	case 1:
		reg = (reg & (~RG_DSI0_MPPLL_TXDIV0)) | (0<<4);
		reg = (reg & (~RG_DSI0_MPPLL_TXDIV1)) | (0<<6);

		break;
	case 2:
		reg = (reg & (~RG_DSI0_MPPLL_TXDIV0)) | (1<<4);
		reg = (reg & (~RG_DSI0_MPPLL_TXDIV1)) | (0<<6);
		break;
	case 4:
		reg = (reg & (~RG_DSI0_MPPLL_TXDIV0)) | (2<<4);
		reg = (reg & (~RG_DSI0_MPPLL_TXDIV1)) | (0<<6);
		break;
	case 8:
		reg = (reg & (~RG_DSI0_MPPLL_TXDIV0)) | (2<<4);
		reg = (reg & (~RG_DSI0_MPPLL_TXDIV1)) | (1<<6);
		break;
	case 16:
		reg = (reg & (~RG_DSI0_MPPLL_TXDIV0)) | (2<<4);
		reg = (reg & (~RG_DSI0_MPPLL_TXDIV1)) | (2<<6);
		break;

	default:
		break;
	}

	writel(reg, dsi->dsi_tx_reg_base + MIPITX_DSI_PLL_CON0);

	reg = readl(dsi->dsi_tx_reg_base + MIPITX_DSI_PLL_CON1);
	reg = (reg & (~RG_DSI0_MPPLL_SDM_SSC_EN));
	reg = (reg & (~RG_DSI0_MPPLL_SDM_SSC_PH_INIT))  |  (1<<1);
	/*reg = (reg & (~RG_DSI0_MPPLL_SDM_FRA_EN)) |  (1<<0);*/
	reg = (reg & (~RG_DSI0_MPPLL_SDM_FRA_EN));
	writel(reg, dsi->dsi_tx_reg_base + MIPITX_DSI_PLL_CON1);


	reg = readl(dsi->dsi_tx_reg_base + MIPITX_DSI_PLL_CON0);
	reg = (reg & (~RG_DSI0_MPPLL_VOD_EN))  |  (1<<0);
	writel(reg, dsi->dsi_tx_reg_base + MIPITX_DSI_PLL_CON0);

	pcw = data_Rate * txdiv / 13;

/*	reg = ((pcw & 0x7F)<<24) + (((256*(data_Rate*txdiv%13)/13) & 0xFF)<<16)
		+ (((256*(256*(data_Rate*txdiv%13)%13)/13) & 0xFF)<<8)
		+((256*(256*(256*(data_Rate*txdiv%13)%13)%13)/13) & 0xFF);*/

	reg = ((0x3E & 0x7F)<<24);
	writel(reg, dsi->dsi_tx_reg_base + MIPITX_DSI_PLL_CON2);


	reg = readl(dsi->dsi_tx_reg_base + MIPITX_DSI_PLL_CON0);
	reg = (reg & (~RG_DSI0_MPPLL_PREDIV));
	reg = (reg & (~RG_DSI0_MPPLL_POSDIV));
	writel(reg, dsi->dsi_tx_reg_base + MIPITX_DSI_PLL_CON0);


	reg = readl(dsi->dsi_tx_reg_base + MIPITX_DSI0_MPPLL_SDM_CON1);
	reg = (reg & (~RG_DSI0_MPPLL_ISO_EN));
	writel(reg, dsi->dsi_tx_reg_base + MIPITX_DSI0_MPPLL_SDM_CON1);





	reg = readl(dsi->dsi_tx_reg_base + MIPITX_DSI0_CLOCK_LANE);
	reg = (reg & (~RG_DSI0_LNTC_RT_CODE)) | (8<<8);
	reg = (reg & (~RG_DSI0_LNTC_LDOOUT_EN)) | (1);
	writel(reg, dsi->dsi_tx_reg_base + MIPITX_DSI0_CLOCK_LANE);

	reg = readl(dsi->dsi_tx_reg_base + MIPITX_DSI0_DATA_LANE0);
	reg = (reg & (~RG_DSI0_LNT0_RT_CODE)) | (8<<7);
	reg = (reg & (~RG_DSI0_LNT0_LDOOUT_EN)) | (1);
	writel(reg, dsi->dsi_tx_reg_base + MIPITX_DSI0_DATA_LANE0);

	reg = readl(dsi->dsi_tx_reg_base + MIPITX_DSI0_DATA_LANE1);
	reg = (reg & (~RG_DSI0_LNT1_RT_CODE)) | (8<<5);
	reg = (reg & (~RG_DSI0_LNT1_LDOOUT_EN)) | (1);
	writel(reg, dsi->dsi_tx_reg_base + MIPITX_DSI0_DATA_LANE1);

	reg = readl(dsi->dsi_tx_reg_base + MIPITX_DSI0_DATA_LANE2);
	reg = (reg & (~RG_DSI0_LNT2_RT_CODE)) | (8<<5);
	reg = (reg & (~RG_DSI0_LNT2_LDOOUT_EN)) | (1);
	writel(reg, dsi->dsi_tx_reg_base + MIPITX_DSI0_DATA_LANE2);

	reg = readl(dsi->dsi_tx_reg_base + MIPITX_DSI0_DATA_LANE3);
	reg = (reg & (~RG_DSI0_LNT3_RT_CODE)) | (8<<5);
	reg = (reg & (~RG_DSI0_LNT3_LDOOUT_EN)) | (1);
	writel(reg, dsi->dsi_tx_reg_base + MIPITX_DSI0_DATA_LANE3);

	reg = (reg & (~RG_DSI0_MPPLL_PLL_EN));


	udelay(1000);


	reg = readl(dsi->dsi_tx_reg_base + MIPITX_DSI_PLL_CON0);
	reg = (reg & (~RG_DSI0_MPPLL_PLL_EN)) | (1<<1);
	writel(reg, dsi->dsi_tx_reg_base + MIPITX_DSI_PLL_CON0);
	mdelay(20);


	reg = readl(dsi->dsi_tx_reg_base + MIPITX_DSI_PLL_TOP);
	reg = (reg & (~RG_MPPLL_PRESERVE)) | (3<<16);
	writel(reg, dsi->dsi_tx_reg_base + MIPITX_DSI_PLL_TOP);

}

#endif


void DSI_PHY_clk_setting(struct mtk_dsi *dsi)
{

	unsigned int data_Rate = dsi->pll_clk_rate * 2;
	unsigned int txdiv = 0;
	unsigned int txdiv0 = 0;
	unsigned int txdiv1 = 0;
	unsigned int pcw = 0;
	u32 reg;
	u32 temp;


	/* step 2 */
	reg = readl(dsi->dsi_tx_reg_base + MIPITX_DSI_BG_CON);
	reg = (reg & (~RG_DSI_V032_SEL)) | (4<<17);
	reg = (reg & (~RG_DSI_V04_SEL)) | (4<<14);
	reg = (reg & (~RG_DSI_V072_SEL)) | (4<<11);
	reg = (reg & (~RG_DSI_V10_SEL)) | (4<<8);
	reg = (reg & (~RG_DSI_V12_SEL)) | (4<<5);
	reg = (reg & (~RG_DSI_BG_CKEN)) | (1<<1);
	reg = (reg & (~RG_DSI_BG_CORE_EN)) | (1);
	writel(reg, dsi->dsi_tx_reg_base + MIPITX_DSI_BG_CON);

	/* step 3 */
	udelay(1000);

	/* step 4 */
	reg = readl(dsi->dsi_tx_reg_base + MIPITX_DSI_TOP_CON);
	reg = (reg & (~RG_DSI_LNT_IMP_CAL_CODE)) | (8<<4);
	reg = (reg & (~RG_DSI_LNT_HS_BIAS_EN)) | (1<<1);
	writel(reg, dsi->dsi_tx_reg_base + MIPITX_DSI_TOP_CON);

	/* step 5 */
	reg = readl(dsi->dsi_tx_reg_base + MIPITX_DSI0_CON);
	reg = (reg & (~RG_DSI0_CKG_LDOOUT_EN)) | (1<<1);
	reg = (reg & (~RG_DSI0_LDOCORE_EN)) | (1);
	writel(reg, dsi->dsi_tx_reg_base + MIPITX_DSI0_CON);

	/* step 6 */
	reg = readl(dsi->dsi_tx_reg_base + MIPITX_DSI_PLL_PWR);
	reg = (reg & (~RG_DSI_MPPLL_SDM_PWR_ON)) | (1<<0);
	reg = (reg & (~RG_DSI_MPPLL_SDM_ISO_EN));
	writel(reg, dsi->dsi_tx_reg_base + MIPITX_DSI_PLL_PWR);

	reg = readl(dsi->dsi_tx_reg_base + MIPITX_DSI_PLL_CON0);
	reg = (reg & (~RG_DSI0_MPPLL_PLL_EN));
	writel(reg, dsi->dsi_tx_reg_base + MIPITX_DSI_PLL_CON0);


	udelay(1000);

	if (data_Rate > 1250) {
		txdiv = 1;
		txdiv0 = 0;
		txdiv1 = 0;
	} else if (data_Rate >= 500) {
		txdiv = 1;
		txdiv0 = 0;
		txdiv1 = 0;
	} else if (data_Rate >= 250) {
		txdiv = 2;
		txdiv0 = 1;
		txdiv1 = 0;
	} else if (data_Rate >= 125) {
		txdiv = 4;
		txdiv0 = 2;
		txdiv1 = 0;
	} else if (data_Rate > 62) {
		txdiv = 8;
		txdiv0 = 2;
		txdiv1 = 1;
	} else if (data_Rate >= 50) {
		txdiv = 16;
		txdiv0 = 2;
		txdiv1 = 2;
	} else {
	}

	/* step 8 */
	reg = readl(dsi->dsi_tx_reg_base + MIPITX_DSI_PLL_CON0);

	switch (txdiv) {
	case 1:
		reg = (reg & (~RG_DSI0_MPPLL_TXDIV0)) | (0<<3);
		reg = (reg & (~RG_DSI0_MPPLL_TXDIV1)) | (0<<5);

		break;
	case 2:
		reg = (reg & (~RG_DSI0_MPPLL_TXDIV0)) | (1<<3);
		reg = (reg & (~RG_DSI0_MPPLL_TXDIV1)) | (0<<5);
		break;
	case 4:
		reg = (reg & (~RG_DSI0_MPPLL_TXDIV0)) | (2<<3);
		reg = (reg & (~RG_DSI0_MPPLL_TXDIV1)) | (0<<5);
		break;
	case 8:
		reg = (reg & (~RG_DSI0_MPPLL_TXDIV0)) | (2<<3);
		reg = (reg & (~RG_DSI0_MPPLL_TXDIV1)) | (1<<5);
		break;
	case 16:
		reg = (reg & (~RG_DSI0_MPPLL_TXDIV0)) | (2<<3);
		reg = (reg & (~RG_DSI0_MPPLL_TXDIV1)) | (2<<5);
		break;

	default:
		break;
	}
	reg = (reg & (~RG_DSI0_MPPLL_PREDIV));
	writel(reg, dsi->dsi_tx_reg_base + MIPITX_DSI_PLL_CON0);

	/* step 10 */
	/* PLL PCW config */
	/*
	   PCW bit 24~30 = floor(pcw)
	   PCW bit 16~23 = (pcw - floor(pcw))*256
	   PCW bit 8~15 = (pcw*256 - floor(pcw)*256)*256
	   PCW bit 8~15 = (pcw*256*256 - floor(pcw)*256*256)*256
	 */
	/* pcw = data_Rate*4*txdiv/(26*2);//Post DIV =4, so need data_Rate*4 */
	pcw = data_Rate * txdiv / 13;
	temp = data_Rate * txdiv % 13;
	reg = ((pcw & 0x7F)<<24) + (((256 * temp / 13) & 0xFF)<<16)
			+ (((256 * (256 * temp % 13)/13) & 0xFF)<<8)
			+ ((256 * (256 * (256 * temp % 13) % 13) / 13) & 0xFF);
	writel(reg, dsi->dsi_tx_reg_base + MIPITX_DSI_PLL_CON2);


	reg = readl(dsi->dsi_tx_reg_base + MIPITX_DSI_PLL_CON1);
	reg = (reg & (~RG_DSI0_MPPLL_SDM_FRA_EN)) | (1<<0);
	writel(reg, dsi->dsi_tx_reg_base + MIPITX_DSI_PLL_CON1);


	/* step 11 */
	reg = readl(dsi->dsi_tx_reg_base + MIPITX_DSI0_CLOCK_LANE);
	reg = (reg & (~RG_DSI0_LNTC_LDOOUT_EN)) | (1<<0);
	writel(reg, dsi->dsi_tx_reg_base + MIPITX_DSI0_CLOCK_LANE);

	/* step 12 */
		reg = readl(dsi->dsi_tx_reg_base + MIPITX_DSI0_DATA_LANE0);
		reg = (reg & (~RG_DSI0_LNT0_LDOOUT_EN)) | (1<<0);
		writel(reg, dsi->dsi_tx_reg_base + MIPITX_DSI0_DATA_LANE0);

	/* step 13 */
		reg = readl(dsi->dsi_tx_reg_base + MIPITX_DSI0_DATA_LANE1);
		reg = (reg & (~RG_DSI0_LNT1_LDOOUT_EN)) | (1<<0);
		writel(reg, dsi->dsi_tx_reg_base + MIPITX_DSI0_DATA_LANE1);

	/* step 14 */
		reg = readl(dsi->dsi_tx_reg_base + MIPITX_DSI0_DATA_LANE2);
		reg = (reg & (~RG_DSI0_LNT2_LDOOUT_EN)) | (1<<0);
		writel(reg, dsi->dsi_tx_reg_base + MIPITX_DSI0_DATA_LANE2);

	/* step 15 */
		reg = readl(dsi->dsi_tx_reg_base + MIPITX_DSI0_DATA_LANE3);
		reg = (reg & (~RG_DSI0_LNT3_LDOOUT_EN)) | (1<<0);
		writel(reg, dsi->dsi_tx_reg_base + MIPITX_DSI0_DATA_LANE3);

	/* step 16 */
	reg = readl(dsi->dsi_tx_reg_base + MIPITX_DSI_PLL_CON0);
	reg = (reg & (~RG_DSI0_MPPLL_PLL_EN)) | (1<<0);
	writel(reg, dsi->dsi_tx_reg_base + MIPITX_DSI_PLL_CON0);

	/* step 17 */
	udelay(1000);
	reg = readl(dsi->dsi_tx_reg_base + MIPITX_DSI_PLL_CON1);
	reg = (reg & (~RG_DSI0_MPPLL_SDM_SSC_EN));
	writel(reg, dsi->dsi_tx_reg_base + MIPITX_DSI_PLL_CON1);

	/* step 18 */
	reg = readl(dsi->dsi_tx_reg_base + MIPITX_DSI_TOP_CON);
	reg = (reg & (~RG_DSI_PAD_TIE_LOW_EN));
	writel(reg, dsi->dsi_tx_reg_base + MIPITX_DSI_TOP_CON);

}



void DSI_PHY_TIMCONFIG(struct mtk_dsi *dsi)
{
	u32 timcon0 = 0;
	u32 timcon1 = 0;
	u32 timcon2 = 0;
	u32 timcon3 = 0;
	unsigned int lane_no = dsi->lanes;


	unsigned int cycle_time;
	unsigned int ui;
	unsigned int hs_trail_m, hs_trail_n;




	ui = 1000/(250 * 2) + 0x01;
	cycle_time = 8000/(250 * 2) + 0x01;

	#define NS_TO_CYCLE(n, c)    ((n) / c + (((n) % c) ? 1 : 0))

	hs_trail_m = lane_no;
	hs_trail_n =  NS_TO_CYCLE(((lane_no * 4 * ui) + 60), cycle_time);

	/*timcon0 = (timcon0 & (~HS_TRAIL)) | (((hs_trail_m > hs_trail_n) ?
	hs_trail_m : hs_trail_n) + 0x0a)<<24;*/
	timcon0 = (timcon0 & (~HS_TRAIL)) | (8<<24);

	/*timcon0 = (timcon0 & (~HS_PRPR)) | (NS_TO_CYCLE((60 + 5 * ui),
	cycle_time)<<8);*/
	timcon0 = (timcon0 & (~HS_PRPR)) | 0x6<<8;

	if ((timcon0 & HS_PRPR) == 0)
		timcon0 = (timcon0 & (~HS_PRPR)) | 1<<8;

	/*timcon0 = (timcon0 & (~HS_ZERO)) | (NS_TO_CYCLE((0xC8 + 0x0a * ui -
	((timcon0 & HS_PRPR)>>8) * cycle_time), cycle_time)<<16);*/
	timcon0 =  (timcon0 & (~HS_ZERO)) | 0xA<<16;
	/*timcon0 =  (timcon0 & (~LPX)) | NS_TO_CYCLE(65, cycle_time);*/
	timcon0 =  (timcon0 & (~LPX)) | 5;

	if ((timcon0 & LPX) == 0)
		timcon0 =  (timcon0 & (~LPX)) | 1;

	timcon1 = (timcon1 & (~TA_GET)) | (5 * (timcon0 & LPX)<<16);

	timcon1 = (timcon1 & (~TA_SURE)) | ((3 * (timcon0 & LPX) / 2) << 8);
	timcon1 =  (timcon1 & (~TA_GO)) | (4 * (timcon0 & LPX));

	/*timcon1 = (timcon1 & (~DA_HS_EXIT)) | ((2 * (timcon0 & LPX))<<24);*/
	timcon1 = (timcon1 & (~DA_HS_EXIT)) | (7<<24);

	timcon2 = (timcon2 & (~CLK_TRAIL)) | ((NS_TO_CYCLE(0x64, cycle_time) +
		0x0a)<<24);

	if (((timcon2 & CLK_TRAIL)>>24) < 2)
		timcon2 = (timcon2 & (~CLK_TRAIL)) | (2<<24);


	timcon2 = (timcon2 & (~CONT_DET));
	timcon3 =  (timcon3 & (~CLK_HS_PRPR)) | NS_TO_CYCLE(0x40, cycle_time);
	if ((timcon3 & CLK_HS_PRPR) == 0)
		timcon3 = (timcon3 & (~CLK_HS_PRPR)) | 1;

	timcon2 = (timcon2 & (~CLK_ZERO)) |
		(NS_TO_CYCLE(0x190 - (timcon3 & CLK_HS_PRPR) * cycle_time,
		cycle_time)<<16);


	timcon3 =  (timcon3 & (~CLK_HS_EXIT)) | ((2 * (timcon0 & LPX))<<16);

	timcon3 =  (timcon3 & (~CLK_HS_POST)) | (NS_TO_CYCLE((80 + 52 * ui),
		cycle_time)<<8);

	writel(timcon0, dsi->dsi_reg_base + DSI_PHY_TIMECON0);
	writel(timcon1, dsi->dsi_reg_base + DSI_PHY_TIMECON1);
	writel(timcon2, dsi->dsi_reg_base + DSI_PHY_TIMECON2);
	writel(timcon3, dsi->dsi_reg_base + DSI_PHY_TIMECON3);

}


void mtk_dsi_reset(struct mtk_dsi *dsi)
{

	writel(3, dsi->dsi_reg_base + DSI_CON_CTRL);
	writel(2, dsi->dsi_reg_base + DSI_CON_CTRL);

}


static int mtk_dsi_poweron(struct mtk_dsi *dsi)
{
	int ret;


		DSI_PHY_clk_setting(dsi);


	ret = clk_prepare_enable(dsi->dsi0_engine_clk_cg);
	if (ret < 0) {
		dev_err(dsi->dev, "can't enable dsi0_engine_clk_cg %d\n", ret);
		goto err_dsi0_engine_clk_cg;
	}

	ret = clk_prepare_enable(dsi->dsi0_digital_clk_cg);
	if (ret < 0) {
		dev_err(dsi->dev, "can't enable dsi0_digital_clk_cg %d\n", ret);
		goto err_dsi0_digital_clk_cg;
	}

	mtk_dsi_reset((dsi));


	DSI_PHY_TIMCONFIG(dsi);


	return 0;

err_dsi0_engine_clk_cg:
	clk_disable_unprepare(dsi->dsi0_engine_clk_cg);
err_dsi0_digital_clk_cg:
	clk_disable_unprepare(dsi->dsi0_digital_clk_cg);

	return ret;
}


void DSI_clk_ULP_mode(struct mtk_dsi *dsi, bool enter)
{
	u32 tmp_reg1;

	tmp_reg1 = readl(dsi->dsi_reg_base + DSI_PHY_LCCON);

	if (enter) {

		tmp_reg1 = tmp_reg1 & (~LC_HS_TX_EN);
		writel(tmp_reg1, dsi->dsi_reg_base + DSI_PHY_LCCON);
		udelay(100);
		tmp_reg1 = tmp_reg1 & (~LC_ULPM_EN);
		writel(tmp_reg1, dsi->dsi_reg_base + DSI_PHY_LCCON);
		udelay(100);

	} else {
		tmp_reg1 = tmp_reg1 & (~LC_ULPM_EN);
		writel(tmp_reg1, dsi->dsi_reg_base + DSI_PHY_LCCON);
		udelay(100);
		tmp_reg1 = tmp_reg1 | LC_WAKEUP_EN;
		writel(tmp_reg1, dsi->dsi_reg_base + DSI_PHY_LCCON);
		udelay(100);
		tmp_reg1 = tmp_reg1 & (~LC_WAKEUP_EN);
		writel(tmp_reg1, dsi->dsi_reg_base + DSI_PHY_LCCON);
		udelay(100);

	}
}


void DSI_lane0_ULP_mode(struct mtk_dsi *dsi, bool enter)
{
	u32 tmp_reg1;

	tmp_reg1 = readl(dsi->dsi_reg_base + DSI_PHY_LD0CON);

	if (enter) {

		tmp_reg1 = tmp_reg1 & (~LD0_HS_TX_EN);
		writel(tmp_reg1, dsi->dsi_reg_base + DSI_PHY_LD0CON);
		udelay(100);
		tmp_reg1 = tmp_reg1 & (~LD0_ULPM_EN);
		writel(tmp_reg1, dsi->dsi_reg_base + DSI_PHY_LD0CON);
		udelay(100);

	} else {
		tmp_reg1 = tmp_reg1 & (~LD0_ULPM_EN);
		writel(tmp_reg1, dsi->dsi_reg_base + DSI_PHY_LD0CON);
		udelay(100);
		tmp_reg1 = tmp_reg1 | LD0_WAKEUP_EN;
		writel(tmp_reg1, dsi->dsi_reg_base + DSI_PHY_LD0CON);
		udelay(100);
		tmp_reg1 = tmp_reg1 & (~LD0_WAKEUP_EN);
		writel(tmp_reg1, dsi->dsi_reg_base + DSI_PHY_LD0CON);
		udelay(100);

	}
}

bool DSI_clk_HS_state(struct mtk_dsi *dsi)
{

	u32 tmp_reg1;

	tmp_reg1 = readl(dsi->dsi_reg_base + DSI_PHY_LCCON);

	return ((tmp_reg1 & LC_HS_TX_EN) == 1) ? true : false;
}


void DSI_clk_HS_mode(struct mtk_dsi *dsi, bool enter)
{
	u32 tmp_reg1;

	tmp_reg1 = readl(dsi->dsi_reg_base + DSI_PHY_LCCON);

	if (enter && !DSI_clk_HS_state(dsi)) {
		tmp_reg1 = tmp_reg1 | LC_HS_TX_EN;
		writel(tmp_reg1, dsi->dsi_reg_base + DSI_PHY_LCCON);
	} else if (!enter && DSI_clk_HS_state(dsi)) {
		tmp_reg1 = tmp_reg1 & (~LC_HS_TX_EN);
		writel(tmp_reg1, dsi->dsi_reg_base + DSI_PHY_LCCON);
	}
}


void  DSI_SetMode(struct mtk_dsi *dsi)
{
	u32 tmp_reg1;

	tmp_reg1 = 0;

	if (dsi->mode_flags & MIPI_DSI_MODE_VIDEO) {

		tmp_reg1 = SYNC_PULSE_MODE;

		if (dsi->mode_flags & MIPI_DSI_MODE_VIDEO_BURST)
			tmp_reg1 = BURST_MODE;

		if (dsi->mode_flags & MIPI_DSI_MODE_VIDEO_SYNC_PULSE)
			tmp_reg1 = SYNC_PULSE_MODE;

	}

	writel(tmp_reg1, dsi->dsi_reg_base + DSI_MODE_CTRL);
}


void DSI_PS_Control(struct mtk_dsi *dsi)
{
	struct videomode *vm = &dsi->vm;
	u32 dsiTmpBufBpp, ps_wc;
	u32 tmp_reg;
	u32 tmp_hstx_cklp_wc;

	tmp_reg = 0;

	if (dsi->format == MIPI_DSI_FMT_RGB565)
		dsiTmpBufBpp = 2;
	else
		dsiTmpBufBpp = 3;

	ps_wc = vm->vactive * dsiTmpBufBpp;

	tmp_reg = ps_wc;

	switch (dsi->format) {
	case MIPI_DSI_FMT_RGB888:
		tmp_reg |= PACKED_PS_24BIT_RGB888;
		break;
	case MIPI_DSI_FMT_RGB666:
		tmp_reg |= PACKED_PS_18BIT_RGB666;
		break;
	case MIPI_DSI_FMT_RGB666_PACKED:
		tmp_reg |= LOOSELY_PS_18BIT_RGB666;
		break;
	case MIPI_DSI_FMT_RGB565:
		tmp_reg |= PACKED_PS_16BIT_RGB565;
		break;
		}

	tmp_hstx_cklp_wc = ps_wc;

	writel(vm->vactive, dsi->dsi_reg_base + DSI_VACT_NL);
	writel(tmp_reg, dsi->dsi_reg_base + DSI_PSCTRL);
	writel(tmp_hstx_cklp_wc, dsi->dsi_reg_base + DSI_HSTX_CKL_WC);
}

void dsi_rxtx_control(struct mtk_dsi *dsi)
{
	u32 tmp_reg = 0;

	switch (dsi->lanes) {
	case 1:
		tmp_reg = 1<<2;
		break;
	case 2:
		tmp_reg = 3<<2;
		break;
	case 3:
		tmp_reg = 7<<2;
		break;
	case 4:
		tmp_reg = 0xF<<2;
		break;
	default:
		tmp_reg = 0xF<<2;
		break;
	}

	writel(tmp_reg, dsi->dsi_reg_base + DSI_TXRX_CTRL);
}


void dsi_ps_control(struct mtk_dsi *dsi)
{
	unsigned int dsiTmpBufBpp;
	u32 tmp_reg1 = 0;

	switch (dsi->format) {
	case MIPI_DSI_FMT_RGB888:
		tmp_reg1 = PACKED_PS_24BIT_RGB888;
		dsiTmpBufBpp = 3;
		break;
	case MIPI_DSI_FMT_RGB666:
		tmp_reg1 = LOOSELY_PS_18BIT_RGB666;
		dsiTmpBufBpp = 3;
		break;
	case MIPI_DSI_FMT_RGB666_PACKED:
		tmp_reg1 = PACKED_PS_18BIT_RGB666;
		dsiTmpBufBpp = 3;
		break;
	case MIPI_DSI_FMT_RGB565:
		tmp_reg1 = PACKED_PS_16BIT_RGB565;
		dsiTmpBufBpp = 2;
		break;
	default:
		tmp_reg1 = PACKED_PS_24BIT_RGB888;
		dsiTmpBufBpp = 3;
		break;
	}

	tmp_reg1 = tmp_reg1 + ((dsi->vm.hactive * dsiTmpBufBpp) & DSI_PS_WC);

	writel(tmp_reg1, dsi->dsi_reg_base + DSI_PSCTRL);

}


void DSI_Config_VDO_Timing(struct mtk_dsi *dsi)
{
	unsigned int horizontal_sync_active_byte;
	unsigned int horizontal_backporch_byte;
	unsigned int horizontal_frontporch_byte;
	unsigned int dsiTmpBufBpp;


	struct videomode *vm = &dsi->vm;

	if (dsi->format == MIPI_DSI_FMT_RGB565)
		dsiTmpBufBpp = 2;
	else
		dsiTmpBufBpp = 3;

	writel(vm->vsync_len, dsi->dsi_reg_base + DSI_VSA_NL);
	writel(vm->vback_porch, dsi->dsi_reg_base + DSI_VBP_NL);
	writel(vm->vfront_porch, dsi->dsi_reg_base + DSI_VFP_NL);
	writel(vm->vactive, dsi->dsi_reg_base + DSI_VACT_NL);

	if (dsi->mode_flags & MIPI_DSI_MODE_VIDEO_SYNC_PULSE) {
		horizontal_sync_active_byte = (vm->hsync_len * dsiTmpBufBpp -
			10);
		horizontal_backporch_byte = (vm->hback_porch * dsiTmpBufBpp -
			10);
	} else {
		horizontal_sync_active_byte = (vm->hsync_len * dsiTmpBufBpp -
			10);
		horizontal_backporch_byte = ((vm->hback_porch + vm->hsync_len) *
			dsiTmpBufBpp - 10);
	}

	horizontal_frontporch_byte = (vm->vfront_porch * dsiTmpBufBpp - 12);


	writel(vm->hsync_len, dsi->dsi_reg_base + DSI_HSA_WC);
	writel(vm->hback_porch, dsi->dsi_reg_base + DSI_HBP_WC);
	writel(vm->hfront_porch, dsi->dsi_reg_base + DSI_HFP_WC);

	dsi_ps_control(dsi);

}



void mtk_dsi_start(struct mtk_dsi *dsi)
{

	writel(0, dsi->dsi_reg_base + DSI_START);
	writel(1, dsi->dsi_reg_base + DSI_START);

}

static void mtk_dsi_poweroff(struct mtk_dsi *dsi)
{

	return;
	clk_disable_unprepare(dsi->dsi0_engine_clk_cg);
	clk_disable_unprepare(dsi->dsi0_digital_clk_cg);

	usleep_range(10000, 20000);

	DSI_PHY_clk_switch(dsi, 0);
}


int mtk_output_dsi_enable(struct mtk_output *output)
{
	int ret;
	struct mtk_dsi *dsi = to_dsi(output);

	if (dsi->enabled == true)
		return 0;

	ret = mtk_dsi_poweron(dsi);
	if (ret < 0)
		return ret;


	dsi_rxtx_control(dsi);

	DSI_clk_ULP_mode(dsi, 0);
	DSI_lane0_ULP_mode(dsi, 0);
	DSI_clk_HS_mode(dsi, 0);
	DSI_SetMode(dsi);


/*
	ret = drm_panel_enable(dsi->output.panel);
	if (ret < 0) {
		mtk_dsi_poweroff(dsi);
		return ret;
	}
*/

	DSI_PS_Control(dsi);
	DSI_Config_VDO_Timing(dsi);

	DSI_SetMode(dsi);
	DSI_clk_HS_mode(dsi, 1);


	mtk_dsi_start(dsi);

	dsi->enabled = true;


	return 0;
}



int mtk_output_dsi_enable_temp(struct mtk_dsi *dsi)
{
	int ret;

	if (dsi->enabled == true)
		return 0;

	ret = mtk_dsi_poweron(dsi);
	if (ret < 0)
		return ret;

	dsi_rxtx_control(dsi);


	DSI_clk_ULP_mode(dsi, 0);

	DSI_lane0_ULP_mode(dsi, 0);

	DSI_clk_HS_mode(dsi, 0);


	DSI_SetMode(dsi);


/*
	ret = drm_panel_enable(dsi->output.panel);
	if (ret < 0) {
		mtk_dsi_poweroff(dsi);
		return ret;
	}
*/

	DSI_PS_Control(dsi);


	DSI_Config_VDO_Timing(dsi);



	DSI_SetMode(dsi);
	DSI_clk_HS_mode(dsi, 1);



	mtk_dsi_start(dsi);


	dsi->enabled = true;


	return 0;
}


int mtk_output_dsi_disable(struct mtk_dsi *dsi)
{
	return 0;
	if (dsi->enabled == false)
		return 0;

	/*drm_panel_disable(dsi->panel);*/
	DSI_lane0_ULP_mode(dsi, 1);
	DSI_clk_ULP_mode(dsi, 1);
	mtk_dsi_poweroff(dsi);
	DSI_PHY_clk_switch(dsi, 0);



	dsi->enabled = false;
	return 0;
}



struct mtk_output_ops dsi_ops = {
	.enable = mtk_output_dsi_enable,
/*	.disable = mtk_output_dsi_disable,*/
};


static int mtk_dsi_host_attach(struct mipi_dsi_host *host,
				 struct mipi_dsi_device *device)
{
	struct mtk_dsi *dsi = host_to_mtk(host);
	struct mtk_output *output = &dsi->output;

	dsi->mode_flags = device->mode_flags;
	dsi->format = device->format;
	dsi->lanes = device->lanes;

	output->panel = of_drm_find_panel(device->dev.of_node);
	if (output->panel) {
		if (output->connector->dev)
			drm_helper_hpd_irq_event(output->connector->dev);
	}

	return 0;
}

static int mtk_dsi_host_detach(struct mipi_dsi_host *host,
				 struct mipi_dsi_device *device)
{
	struct mtk_dsi *dsi = host_to_mtk(host);
	struct mtk_output *output = &dsi->output;

	if (output->panel && &device->dev == output->panel->dev) {
		if (output->connector->dev)
			drm_helper_hpd_irq_event(output->connector->dev);

		output->panel = NULL;
	}

	return 0;
}

static const struct mipi_dsi_host_ops mtk_dsi_host_ops = {
	.attach = mtk_dsi_host_attach,
	.detach = mtk_dsi_host_detach,
};







int mtk_dsi_probe(struct drm_device *dev)
{
	struct mtk_dsi *dsi;
	int err;
	struct drm_bridge *bridge;
	/*int ret;
	struct resource *regs;
	struct device_node *node;*/
	struct mtk_drm_private *priv = get_mtk_drm_private(dev);
	struct device *pdev = get_mtk_drm_device(dev);
	struct mtk_drm_crtc *mtk_crtc;
/*	u32 reg,i; */

	mtk_crtc = to_mtk_crtc(priv->crtc[0]);

	bridge = devm_kzalloc(dev->dev, sizeof(*bridge), GFP_KERNEL);
	if (!bridge)
		return -ENOMEM;



	dsi = devm_kzalloc(dev->dev, sizeof(*dsi), GFP_KERNEL);
	if (!dsi)
		return -ENOMEM;

	/*dsi->output.dev = dsi->dev = dev;

	err = mtk_output_probe(&dsi->output);
	if (err < 0)
		return err;*/

	/*
	 * Assume these values by default. When a DSI peripheral driver
	 * attaches to the DSI host, the parameters will be taken from
	 * the attached device.
	 */
	dsi->mode_flags = MIPI_DSI_MODE_VIDEO;
	dsi->format = MIPI_DSI_FMT_RGB888;
	dsi->lanes = 4;
	dsi->pll_clk_rate = 200;


#if 0
	/* #ifdef CONFIG_DRM_MEDIATEK_IT6151 */
	err = mtk_dsi_of_read_u32(pdev->of_node,
		"mediatek,mipi-tx-burst-freq", &dsi->pll_clk_rate);
	if (err < 0)
		return err;
#endif

	dsi->dsi0_engine_clk_cg = devm_clk_get(pdev, "dsi0_engine_disp_ck");
	if (IS_ERR(dsi->dsi0_engine_clk_cg)) {
		dev_err(pdev, "cannot get dsi0_engine_clk_cg\n");
		return PTR_ERR(dsi->dsi0_engine_clk_cg);
	}

	err = clk_prepare_enable(dsi->dsi0_engine_clk_cg);
	if (err < 0) {
		dev_err(pdev, "cannot enable dsi0_engine_clk_cg\n");
		return err;
	}


	dsi->dsi0_digital_clk_cg = devm_clk_get(pdev, "dsi0_digital_disp_ck");
	if (IS_ERR(dsi->dsi0_digital_clk_cg)) {
		dev_err(pdev, "cannot get dsi0_digital_clk_cg\n");
		return PTR_ERR(dsi->dsi0_digital_clk_cg);
	}

	err = clk_prepare_enable(dsi->dsi0_digital_clk_cg);
	if (err < 0) {
		dev_err(pdev, "cannot enable dsi0_digital_clk_cg\n");
		return err;
	}



#if 0
	err = mtk_dsi_setup_clocks(dsi);
	if (err < 0) {
		dev_err(pdev, "cannot setup clocks\n");
		return err;
	}
#endif
	/*regs = platform_get_resource(pdev, IORESOURCE_MEM, 0);*/
	dsi->dsi_reg_base = mtk_crtc->dsi_reg;
	/*if (IS_ERR(dsi->dsi_reg_base))
		return PTR_ERR(dsi->dsi_reg_base);



	regs = platform_get_resource(dev->platformdev, IORESOURCE_MEM, 1);*/
	dsi->dsi_tx_reg_base = mtk_crtc->dsi_ana_reg;
	/*if (IS_ERR(dsi->dsi_tx_reg_base))
	return PTR_ERR(dsi->dsi_tx_reg_base);*/


/*
	dsi->host.ops = &mtk_dsi_host_ops;
	dsi->host.dev = &dev;


	err = mipi_dsi_host_register(&dsi->host);
	if (err < 0) {
		dev_err(&dev->platformdev,
			"failed to register DSI host: %d\n", err);
		return err;
	}
*/
	/* only for bringup */
	dsi->output.type = MTK_OUTPUT_DSI;
	dsi->output.ops = &dsi_ops;


	/*platform_set_drvdata(dev->platformdev, dsi);*/



/*     for test       */

dsi->vm.pixelclock = 76000;
dsi->vm.hactive   = 1368;
dsi->vm.hback_porch = 100;
dsi->vm.hfront_porch = 106;
dsi->vm.hsync_len = 26;

dsi->vm.vactive   = 768;
dsi->vm.vback_porch = 10;
dsi->vm.vfront_porch = 10;
dsi->vm.vsync_len = 12;

#ifdef CONFIG_DRM_MEDIATEK_IT6151

dsi->it6151_rst = 94;

/*


dsi->bl_en_gpio = 23;
dsi->it6151_pwr1v2_gpio = 93;



gpio_request(dsi->bl_en_gpio, "bl-en-gpio");
gpio_direction_output(dsi->bl_en_gpio, 1);

gpio_request(dsi->it6151_pwr1v2_gpio, "it6151pwr12-gpio");
gpio_direction_output(dsi->it6151_pwr1v2_gpio, 1);
*/

gpio_request(dsi->it6151_rst, "it6151reset-gpio");
gpio_direction_output(dsi->it6151_rst, 0);
udelay(15);
gpio_direction_output(dsi->it6151_rst, 1);



/*
dsi->bl_en_gpio = devm_gpiod_get(pdev, "bl-en-gpio");
if (IS_ERR(dsi->bl_en_gpio)) {
	err = PTR_ERR(dsi->bl_en_gpio);
	if (err != -ENOENT) {
		printk("dsi->bl_en_gpio GPIO: %d\n", err);
		return err;
	}

	dsi->bl_en_gpio = NULL;
} else {
	err = gpiod_direction_output(dsi->bl_en_gpio, 0);
	if (err < 0) {
		printk( "failed to setup GPIO: %d\n", err);
		return err;
	}

	printk( "failed11 to setup GPIO: %d\n", err);
}


dsi->it6151_pwr1v2_gpio = devm_gpiod_get(pdev, "bl-en-gpio");
if (IS_ERR(dsi->it6151_pwr1v2_gpio)) {
	err = PTR_ERR(dsi->it6151_pwr1v2_gpio);
	if (err != -ENOENT) {
		printk("dsi->it6151_pwr1v2_gpio GPIO: %d\n", err);
		return err;
	}

	dsi->bl_en_gpio = NULL;
} else {
	err = gpiod_direction_output(dsi->bl_en_gpio, 0);
	if (err < 0) {
		printk( "failed to it6151_pwr1v2_gpio GPIO: %d\n", err);
		return err;
	}

	printk( "failed11 to it6151_pwr1v2_gpio GPIO: %d\n", err);
}



dsi->bl_pwmasr_gpio = devm_gpiod_get(pdev, "bl-en-gpio");
if (IS_ERR(dsi->bl_pwmasr_gpio)) {
	err = PTR_ERR(dsi->bl_pwmasr_gpio);
	if (err != -ENOENT) {
		printk("dsi->bl_pwmasr_gpio GPIO: %d\n", err);
		return err;
	}
	printk( "failed1 to bl_pwmasr_gpio GPIO: %d\n", err);

	dsi->bl_pwmasr_gpio = NULL;
} else {
	err = gpiod_direction_output(dsi->bl_pwmasr_gpio, 0);
	if (err < 0) {
		printk( "failed to bl_pwmasr_gpio GPIO: %d\n", err);
		return err;
	}

	printk( "failed11 to bl_pwmasr_gpio GPIO: %d\n", err);
}




node = of_find_node_by_name(NULL,"bl-en-gpio");
*/
/*
dsi->bl_en_gpio = of_get_named_gpio(node, "bl-en-gpio", 0);
if (gpio_is_valid(dsi->bl_en_gpio)) {
	ret = gpio_request_one(dsi->bl_en_gpio, GPIOF_OUT_INIT_HIGH,
		"PANEL_BL_EN");
	if (ret) {
		printk("Request bl_en-gpio failed (%d)\n", ret);
		return ret;
	}
}
else
	printk("DBG_jitao bl_en_gpio error 1\n");


dsi->it6151_pwr1v2_gpio = of_get_named_gpio(node, "it6151pwr12-gpio", 0);
if (gpio_is_valid(dsi->it6151_pwr1v2_gpio)) {
	ret = gpio_request_one(dsi->it6151_pwr1v2_gpio, GPIOF_OUT_INIT_HIGH,
		"it6151pwr12-gpio");
	if (ret) {
		printk("Request it6151pwr12--gpio failed (%d)\n", ret);
		return ret;
	}
}
else
	printk("DBG_jitao it6151_pwr1v2_gpio error 1\n");

dsi->bl_pwmasr_gpio = of_get_named_gpio(node, "bl-pwmas-gpio", 0);
if (gpio_is_valid(dsi->bl_pwmasr_gpio)) {
	ret = gpio_request_one(dsi->bl_pwmasr_gpio,	GPIOF_OUT_INIT_HIGH,
		"bl-pwmas-gpio");
	if (ret) {
		printk("Request bl_pwm as--gpio failed (%d)\n", ret);
		return ret;
	}
}
else
	printk("DBG_jitao bl_pwmasr_gpio error 1\n");

*/


mtk_output_dsi_enable_temp(dsi);

it6151_pre_enable_temp();


#endif

	/*mtk_output_init(pdev, &dsi->output);*/

/*     for test end */
	return 0;
}

/*
static int mtk_dsi_remove(struct platform_device *pdev)
{
	struct mtk_dsi *dsi = platform_get_drvdata(pdev);


	mipi_dsi_host_unregister(&dsi->host);

	mtk_output_dsi_disable(dsi);

	return 0;
}
*/

static const struct of_device_id mtk_dsi_of_match[] = {
	{ .compatible = "mediatek,mt8135-dsi" },
	{ },
};
/*
struct platform_driver mtk_dsi_driver = {
	.driver = {
		.name = "mtk-dsi",
		.of_match_table = mtk_dsi_of_match,
	},
	.probe = mtk_dsi_probe,
	.remove = mtk_dsi_remove,
};
*/
