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
#include <linux/clk.h>

#include "mediatek_drm_crtc.h"
#include "mediatek_drm_hw-mt8173.h"

#include "mediatek_drm_drv.h"
#include "mediatek_drm_gem.h"
#include "mediatek_drm_dev_if.h"

/* These are locked by dev->vbl_lock */
void mtk_enable_vblank(void __iomem *disp_base)
{
	writel(0x1, disp_base + DISP_OD_INTEN);
}

void mtk_disable_vblank(void __iomem *disp_base)
{
	writel(0x0, disp_base + DISP_OD_INTEN);
}

void mtk_clear_vblank(void __iomem *disp_base)
{
	writel(0x0, disp_base + DISP_OD_INTS);
}

u32 mtk_get_vblank(void __iomem *disp_base)
{
	return readl(disp_base + DISP_OD_INTS);
}

static void DispConfigMainPathConnection(void __iomem *disp_base)
{
	/* OVL0 output to COLOR0 */
	writel(0x1, disp_base + DISP_REG_CONFIG_DISP_OVL0_MOUT_EN);

	/* OD output to RDMA0 */
	writel(0x1, disp_base + DISP_REG_CONFIG_DISP_OD_MOUT_EN);

	/* UFOE output to DSI0 */
	writel(0x1, disp_base + DISP_REG_CONFIG_DISP_UFOE_MOUT_EN);

	/* COLOR0 input from OVL0 */
	writel(0x1, disp_base + DISP_REG_CONFIG_DISP_COLOR0_SEL_IN);
}

static void DispConfigMainPathMutex(void __iomem *mutex_base)
{
	unsigned int ID = 0;

	/* Module: OVL0=11, RDMA0=13, COLOR0=18, AAL=20, UFOE=22, OD=25 */
	writel((1 << 11 | 1 << 13 | 1 << 18 | 1 << 20 | 1 << 22 | 1 << 25),
		mutex_base + DISP_REG_CONFIG_MUTEX_MOD(ID));

	/* Clock source from DSI0 */
	writel(1, mutex_base + DISP_REG_CONFIG_MUTEX_SOF(ID));
	writel(1, mutex_base + DISP_REG_CONFIG_MUTEX_EN(ID));
}

/* work on OVL0 only now */
static void OVLStart(void __iomem *ovl_base)
{
	writel(0x01, ovl_base + DISP_REG_OVL_EN);
	writel(0x0f, ovl_base + DISP_REG_OVL_INTEN);
}

static void OVLStop(void __iomem *ovl_base)
{
	writel(0x00, ovl_base + DISP_REG_OVL_INTEN);
	writel(0x00, ovl_base + DISP_REG_OVL_EN);
	writel(0x00, ovl_base + DISP_REG_OVL_INTSTA);
}

static void OVLROI(void __iomem *ovl_base, unsigned int W, unsigned int H,
	unsigned int bgColor)
{
	writel(H << 16 | W, ovl_base + DISP_REG_OVL_ROI_SIZE);
	writel(bgColor, ovl_base + DISP_REG_OVL_ROI_BGCLR);
}

void OVLLayerSwitch(void __iomem *ovl_base, unsigned layer, bool en)
{
	u32 reg;

	reg = readl(ovl_base + DISP_REG_OVL_SRC_CON);

	if (en)
		reg |= (1U<<layer);
	else
		reg &= ~(1U<<layer);

	writel(reg, ovl_base + DISP_REG_OVL_SRC_CON);
	writel(0x1, ovl_base + DISP_REG_OVL_RST);
	writel(0x0, ovl_base + DISP_REG_OVL_RST);
}

#define RDMA_N_OFST 0x1000
static void RDMAStart(void __iomem *rdma_base, unsigned idx)
{
	unsigned int reg;

	BUG_ON(idx >= 2);

	writel(0x4, rdma_base + DISP_REG_RDMA_INT_ENABLE + idx * RDMA_N_OFST);
	reg = readl(rdma_base + DISP_REG_RDMA_GLOBAL_CON + idx * RDMA_N_OFST);
	reg |= 1;
	writel(reg, rdma_base + DISP_REG_RDMA_GLOBAL_CON + idx * RDMA_N_OFST);
}

static void RDMAStop(void __iomem *rdma_base, unsigned idx)
{
	unsigned int reg;

	BUG_ON(idx >= 2);

	reg = readl(rdma_base + DISP_REG_RDMA_GLOBAL_CON + idx * RDMA_N_OFST);
	reg &= ~(1U);
	writel(reg, rdma_base + DISP_REG_RDMA_GLOBAL_CON + idx * RDMA_N_OFST);
	writel(0, rdma_base + DISP_REG_RDMA_INT_ENABLE + idx * RDMA_N_OFST);
	writel(0, rdma_base + DISP_REG_RDMA_INT_STATUS + idx * RDMA_N_OFST);
}

static void RDMAReset(void __iomem *rdma_base, unsigned idx)
{
	unsigned int reg;
	unsigned int delay_cnt;

	BUG_ON(idx >= 2);

	reg = readl(rdma_base + DISP_REG_RDMA_GLOBAL_CON + idx * RDMA_N_OFST);
	reg |= 0x10;
	writel(reg, rdma_base + DISP_REG_RDMA_GLOBAL_CON + idx * RDMA_N_OFST);

	delay_cnt = 0;
	while ((readl(rdma_base + DISP_REG_RDMA_GLOBAL_CON + idx * RDMA_N_OFST)
		& 0x700) == 0x100) {

		delay_cnt++;
		if (delay_cnt > 10000) {
			DRM_ERROR("RDMAReset timeout\n");
			break;
		}
	}

	reg = readl(rdma_base + DISP_REG_RDMA_GLOBAL_CON + idx * RDMA_N_OFST);
	reg &= ~(0x10U);
	writel(reg, rdma_base + DISP_REG_RDMA_GLOBAL_CON + idx * RDMA_N_OFST);

	delay_cnt = 0;
	while ((readl(rdma_base + DISP_REG_RDMA_GLOBAL_CON + idx * RDMA_N_OFST)
		& 0x700) != 0x100) {

		delay_cnt++;
		if (delay_cnt > 10000) {
			DRM_ERROR("RDMAReset timeout\n");
			break;
		}
	}
}

static void RDMAConfigDirectLink(void __iomem *rdma_base, unsigned idx,
	unsigned width, unsigned height)
{
	unsigned int reg;
	enum RDMA_MODE mode = RDMA_MODE_DIRECT_LINK;
	enum RDMA_OUTPUT_FORMAT outputFormat = RDMA_OUTPUT_FORMAT_ARGB;

	BUG_ON(idx >= 2);

	/* Config mode */
	reg = readl(rdma_base + DISP_REG_RDMA_GLOBAL_CON + idx * RDMA_N_OFST);
	if (mode == RDMA_MODE_DIRECT_LINK)
		reg &= ~(0x2U);
	writel(reg, rdma_base + DISP_REG_RDMA_GLOBAL_CON + idx * RDMA_N_OFST);

	/* Config output format */
	reg = readl(rdma_base + DISP_REG_RDMA_SIZE_CON_0 + idx * RDMA_N_OFST);
	if (outputFormat == RDMA_OUTPUT_FORMAT_ARGB)
		reg &= ~(0x20000000U);
	else
		reg |= 0x20000000U;
	writel(reg, rdma_base + DISP_REG_RDMA_SIZE_CON_0 + idx * RDMA_N_OFST);

	/* Config width */
	reg = readl(rdma_base + DISP_REG_RDMA_SIZE_CON_0 + idx * RDMA_N_OFST);
	reg = (reg & ~(0xFFFU)) | (width & 0xFFFU);
	writel(reg, rdma_base + DISP_REG_RDMA_SIZE_CON_0 + idx * RDMA_N_OFST);

	/* Config height */
	reg = readl(rdma_base + DISP_REG_RDMA_SIZE_CON_1 + idx * RDMA_N_OFST);
	reg = (reg & ~(0xFFFFFU)) | (height & 0xFFFFFU);
	writel(reg, rdma_base + DISP_REG_RDMA_SIZE_CON_1 + idx * RDMA_N_OFST);

	/* Config FIFO control */
	reg = 0x80F00008;
	writel(reg, rdma_base + DISP_REG_RDMA_FIFO_CON + idx * RDMA_N_OFST);
}

static void ODStart(void __iomem *od_base, unsigned int W, unsigned int H)
{
	writel(W << 16 | H, od_base + DISP_OD_SIZE);
	writel(1, od_base + DISP_OD_CFG); /* RELAY mode */

	writel(1, od_base + DISP_OD_EN);
}

static void UFOEStart(void __iomem *ufoe_base)
{
	writel(0x4, ufoe_base + DISPSYS_UFOE_BASE); /* default, BYPASS */
}

static void ColorStart(void __iomem *color_base)
{
	writel(0x2080, color_base + DISP_COLOR_CFG_MAIN); /* default, BYPASS */
	writel(0x1, color_base + DISP_COLOR_START);
}

static unsigned int ovl_fmt_convert(unsigned int fmt)
{
	switch (fmt) {
	case DRM_FORMAT_RGB888:
		return OVL_INFMT_RGB888;
	case DRM_FORMAT_RGB565:
		return OVL_INFMT_RGB565;
	case DRM_FORMAT_ARGB8888:
		return OVL_INFMT_ARGB8888;
	case DRM_FORMAT_RGBA8888:
		return OVL_INFMT_RGBA8888;
	case DRM_FORMAT_BGR888:
		return OVL_INFMT_BGR888;
	case DRM_FORMAT_BGR565:
		return OVL_INFMT_BGR565;
	case DRM_FORMAT_ABGR8888:
		return OVL_INFMT_ABGR8888;
	case DRM_FORMAT_XRGB8888:
	case DRM_FORMAT_BGRA8888:
		return OVL_INFMT_BGRA8888;
	case DRM_FORMAT_YUYV:
		return OVL_INFMT_YUYV;
	case DRM_FORMAT_UYVY:
		return OVL_INFMT_UYVY;
	default:
		DRM_ERROR("drm format %X is not supported\n", fmt);
		return OVL_INFMT_UNKNOWN;
	}
}

#define OVL_N_OFST 0x1000
void OVLLayerConfigCursor(struct drm_crtc *crtc, unsigned int addr,
	int x, int y)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	void __iomem *drm_disp_base = mtk_crtc->regs;
	unsigned int reg;
/*	unsigned int layer = 1; */
	unsigned int width = 64;
	unsigned int src_pitch = 64 * 4;
	bool keyEn = 1;
	unsigned int key = 0xFF000000;	/* color key */
	bool aen = 1;			/* alpha enable */
	unsigned char alpha = 0xFF;
	unsigned int fmt = OVL_INFMT_ARGB8888;

	if (width + x > crtc->mode.hdisplay)
		width = crtc->mode.hdisplay - min(x, crtc->mode.hdisplay);

	OVLStop(drm_disp_base);

	writel(0x1, drm_disp_base + DISP_REG_OVL_RST);
	writel(0x0, drm_disp_base + DISP_REG_OVL_RST);

	writel(0x3, drm_disp_base + DISP_REG_OVL_SRC_CON);

	writel(0x00000001, drm_disp_base + DISP_REG_OVL_RDMA1_CTRL);
	writel(0x40402020, drm_disp_base + DISP_REG_OVL_RDMA1_MEM_GMC_SETTING);
	writel(0x01000000, drm_disp_base + DISP_REG_OVL_RDMA1_FIFO_CTRL);

	reg = keyEn << 30 | fmt << 12 | aen << 8 | alpha;
	writel(reg, drm_disp_base + DISP_REG_OVL_L1_CON);
	writel(key, drm_disp_base + DISP_REG_OVL_L1_SRCKEY);
	writel(64 << 16 | width, drm_disp_base + DISP_REG_OVL_L1_SRC_SIZE);
	writel(y << 16 | x, drm_disp_base + DISP_REG_OVL_L1_OFFSET);
	writel(addr, drm_disp_base + DISP_REG_OVL_L1_ADDR);
	writel(src_pitch & 0xFFFF, drm_disp_base + DISP_REG_OVL_L1_PITCH);

	OVLStart(drm_disp_base);
}

void OVLLayerConfig(struct drm_crtc *crtc, unsigned int addr,
	unsigned int format)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	void __iomem *drm_disp_base = mtk_crtc->ovl_regs;
	unsigned int reg;
/*	unsigned int layer = 0; */
	unsigned int source = 0;	/* from memory */
	unsigned int src_x = 0;	/* ROI x offset */
	unsigned int src_y = 0;	/* ROI y offset */
	unsigned int src_pitch;
	unsigned int dst_x = 0;	/* ROI x offset */
	unsigned int dst_y = 0;	/* ROI y offset */
	unsigned int dst_w = crtc->mode.hdisplay;	/* ROT width */
	unsigned int dst_h = crtc->mode.vdisplay;	/* ROI height */
	bool keyEn = 1;
	unsigned int key = 0xFF000000;	/* color key */
	bool aen = 0;			/* alpha enable */
	unsigned char alpha = 0x0;

	unsigned int rgb_swap, bpp;
	unsigned int fmt = ovl_fmt_convert(format);

	if (fmt == OVL_INFMT_BGR888 || fmt == OVL_INFMT_BGR565 ||
		fmt == OVL_INFMT_ABGR8888 || fmt == OVL_INFMT_BGRA8888) {
		fmt -= OVL_COLOR_BASE;
		rgb_swap = 1;
	} else {
		rgb_swap = 0;
	}

	switch (fmt) {
	case OVL_INFMT_ARGB8888:
	case OVL_INFMT_RGBA8888:
		bpp = 4;
		break;
	case OVL_INFMT_RGB888:
		bpp = 3;
		break;
	case OVL_INFMT_RGB565:
	case OVL_INFMT_YUYV:
	case OVL_INFMT_UYVY:
		bpp = 2;
		break;
	default:
		bpp = 1;	/* invalid input format */
	}

	src_pitch = crtc->mode.hdisplay * bpp;
	OVLLayerSwitch(mtk_crtc->ovl_regs, 0, 1);

	writel(0x1, drm_disp_base + DISP_REG_OVL_RST);
	writel(0x0, drm_disp_base + DISP_REG_OVL_RST);

	writel(0x3, drm_disp_base + DISP_REG_OVL_SRC_CON);

	writel(0x00000001, drm_disp_base + DISP_REG_OVL_RDMA0_CTRL);
	writel(0x40402020, drm_disp_base + DISP_REG_OVL_RDMA0_MEM_GMC_SETTING);

	reg = keyEn << 30 | source << 28 | rgb_swap << 25 | fmt << 12 |
		aen << 8 | alpha;
	writel(reg, drm_disp_base + DISP_REG_OVL_L0_CON);
	writel(key, drm_disp_base + DISP_REG_OVL_L0_SRCKEY);
	writel(dst_h << 16 | dst_w, drm_disp_base + DISP_REG_OVL_L0_SRC_SIZE);
	writel(dst_y << 16 | dst_x, drm_disp_base + DISP_REG_OVL_L0_OFFSET);
	reg = addr + src_x * bpp + src_y * src_pitch;
	writel(reg, drm_disp_base + DISP_REG_OVL_L0_ADDR);
	writel(src_pitch & 0xFFFF, drm_disp_base + DISP_REG_OVL_L0_PITCH);
}

void OVLDumpRegister(void __iomem *disp_base)
{
	int i;

	DRM_INFO("OVL Register:\n");
	for (i = 0; i < 0x300; i += 16)
		DRM_INFO("0x%08X: %08X %08X %08X %08X\n", i,
			readl(disp_base + DISPSYS_OVL0_BASE + i),
			readl(disp_base + DISPSYS_OVL0_BASE + i + 4),
			readl(disp_base + DISPSYS_OVL0_BASE + i + 8),
			readl(disp_base + DISPSYS_OVL0_BASE + i + 12));
}

void RDMADumpRegister(void __iomem *disp_base)
{
	int i;

	DRM_INFO("RDMA Register:\n");
	for (i = 0; i < 0x100; i += 16)
		DRM_INFO("0x%08X: %08X %08X %08X %08X\n", i,
			readl(disp_base + DISPSYS_RDMA0_BASE + i),
			readl(disp_base + DISPSYS_RDMA0_BASE + i + 4),
			readl(disp_base + DISPSYS_RDMA0_BASE + i + 8),
			readl(disp_base + DISPSYS_RDMA0_BASE + i + 12));
}

void DispConfigDumpRegister(void __iomem *disp_base)
{
	int i;

	DRM_INFO("Disp Config Register:\n");
	for (i = 0; i < 0x180; i += 16)
		DRM_INFO("0x%08X: %08X %08X %08X %08X\n", i,
			readl(disp_base + DISPSYS_CONFIG_BASE + i),
			readl(disp_base + DISPSYS_CONFIG_BASE + i + 4),
			readl(disp_base + DISPSYS_CONFIG_BASE + i + 8),
			readl(disp_base + DISPSYS_CONFIG_BASE + i + 12));
}

void MainDispPathPowerOn(struct drm_crtc *crtc)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);

	OVLStart(mtk_crtc->ovl_regs);
	RDMAStart(mtk_crtc->rdma_regs, 0);
}

void MainDispPathPowerOff(struct drm_crtc *crtc)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);

	OVLStop(mtk_crtc->ovl_regs);
	RDMAStop(mtk_crtc->rdma_regs, 0);
	RDMAReset(mtk_crtc->rdma_regs, 0);

	DispConfigDumpRegister(mtk_crtc->regs);
}

void MainDispPathSetup(struct drm_crtc *crtc)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct device *pdev = get_mtk_drm_device(crtc->dev);

	unsigned int width, height;
	int err;

	err = of_property_read_u32(pdev->of_node, "mediatek,width",
		&width);
	if (err < 0)
		return;

	err = of_property_read_u32(pdev->of_node, "mediatek,height",
		&height);
	if (err < 0)
		return;

	DRM_INFO("DBG_YT MainDispPathSetup %d %d\n", width, height);
	/* Setup OVL */
	OVLROI(mtk_crtc->ovl_regs, width, height, 0x00000000);
	OVLLayerSwitch(mtk_crtc->ovl_regs, 0, 1);

	/* Setup RDMA0 */
	RDMAConfigDirectLink(mtk_crtc->rdma_regs, 0, width, height);

	/* Setup OD */
	ODStart(mtk_crtc->od_regs, width, height);

	/* Setup UFOE */
	UFOEStart(mtk_crtc->ufoe_regs);

	/* Setup Color */
	ColorStart(mtk_crtc->color_regs);

	/* Setup main path connection */
	DispConfigMainPathConnection(mtk_crtc->regs);

	/* Setup main path mutex */
	DispConfigMainPathMutex(mtk_crtc->mutex_regs);
}

void MainDispPathClear(struct drm_crtc *crtc)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	int i;

	DRM_INFO("MainDispPathClear..all\n");
	for (i = 0; i < 0x100; i += 4)
		writel(0, mtk_crtc->regs + i);

	for (i = 0; i < 0x1000; i += 4) /* OVL */
		writel(0, mtk_crtc->ovl_regs + i);

	for (i = 0; i < 0x1000; i += 4) /* RDMA */
		writel(0, mtk_crtc->rdma_regs + i);

	for (i = 0x400; i < 0x1000; i += 4) /* COLOR */
		writel(0, mtk_crtc->color_regs + i);

	for (i = 0; i < 0x1000; i += 4) /* AAL */
		writel(0, mtk_crtc->aal_regs + i);

	for (i = 0; i < 0x1000; i += 4) /* UFOE */
		writel(0, mtk_crtc->ufoe_regs + i);

	for (i = 0; i < 0x1000; i += 4) /* DSI */
		writel(0, mtk_crtc->dsi_reg + i);

	for (i = 0; i < 0x1000; i += 4) /* MUTEX */
		writel(0, mtk_crtc->mutex_regs + i);

	for (i = 0; i < 0x1000; i += 4) /* OD */
		writel(0, mtk_crtc->od_regs + i);
}

void DispClockSetup(struct device *dev, struct MTK_DISP_CLKS **pdisp_clks)
{
	*pdisp_clks = vmalloc(sizeof(struct MTK_DISP_CLKS));

	(*pdisp_clks)->disp_mtcmos = devm_clk_get(dev, "disp_mtcmos");
	if (IS_ERR((*pdisp_clks)->disp_mtcmos))
		DRM_ERROR("DispClockSetup: Get disp_mtcmos fail.\n");

	(*pdisp_clks)->smi_common_ck = devm_clk_get(dev, "smi_common_ck");
	if (IS_ERR((*pdisp_clks)->smi_common_ck))
		DRM_ERROR("DispClockSetup: Get smi_common_ck fail.\n");

	(*pdisp_clks)->smi_larb0_ck = devm_clk_get(dev, "smi_larb0_ck");
	if (IS_ERR((*pdisp_clks)->smi_larb0_ck))
		DRM_ERROR("DispClockSetup: Get smi_larb0_ck fail.\n");

	(*pdisp_clks)->smi_larb4_ck = devm_clk_get(dev, "smi_larb4_ck");
	if (IS_ERR((*pdisp_clks)->smi_larb4_ck))
		DRM_ERROR("DispClockSetup: Get smi_larb4_ck fail.\n");

	(*pdisp_clks)->mutex_disp_ck = devm_clk_get(dev, "mutex_disp_ck");
	if (IS_ERR((*pdisp_clks)->mutex_disp_ck))
		DRM_ERROR("DispClockSetup: Get mutex_disp_ck fail.\n");

	(*pdisp_clks)->ovl0_disp_ck = devm_clk_get(dev, "ovl0_disp_ck");
	if (IS_ERR((*pdisp_clks)->ovl0_disp_ck))
		DRM_ERROR("DispClockSetup: Get ovl0_disp_ck fail.\n");

	(*pdisp_clks)->rdma0_disp_ck = devm_clk_get(dev, "rdma0_disp_ck");
	if (IS_ERR((*pdisp_clks)->rdma0_disp_ck))
		DRM_ERROR("DispClockSetup: Get rdma0_disp_ck.\n");

	(*pdisp_clks)->color0_disp_ck = devm_clk_get(dev, "color0_disp_ck");
	if (IS_ERR((*pdisp_clks)->color0_disp_ck))
		DRM_ERROR("DispClockSetup: Get color0_disp_ck fail.\n");

	(*pdisp_clks)->aal_disp_ck = devm_clk_get(dev, "aal_disp_ck");
	if (IS_ERR((*pdisp_clks)->aal_disp_ck))
		DRM_ERROR("DispClockSetup: Get aal_disp_ck fail.\n");

	(*pdisp_clks)->ufoe_disp_ck = devm_clk_get(dev, "ufoe_disp_ck");
	if (IS_ERR((*pdisp_clks)->ufoe_disp_ck))
		DRM_ERROR("DispClockSetup: Get ufoe_disp_ck fail.\n");

	(*pdisp_clks)->od_disp_ck = devm_clk_get(dev, "od_disp_ck");
	if (IS_ERR((*pdisp_clks)->od_disp_ck))
		DRM_ERROR("DispClockSetup: Get od_disp_ck fail.\n");

}

void DispClockOn(struct MTK_DISP_CLKS *disp_clks)
{
	int ret;

	/* disp_mtcmos */
	ret = clk_prepare(disp_clks->disp_mtcmos);
	if (ret != 0)
		DRM_ERROR("clk_prepare(disp_clks->disp_mtcmos) error!\n");

	ret = clk_enable(disp_clks->disp_mtcmos);
	if (ret != 0)
		DRM_ERROR("clk_enable(disp_clks->disp_mtcmos) error!\n");

	/* smi_common_ck */
	ret = clk_prepare(disp_clks->smi_common_ck);
	if (ret != 0)
		DRM_ERROR("clk_prepare(disp_clks->smi_common_ck) error!\n");

	ret = clk_enable(disp_clks->smi_common_ck);
	if (ret != 0)
		DRM_ERROR("clk_enable(disp_clks->smi_common_ck) error!\n");

	/* smi_larb0_ck */
	ret = clk_prepare(disp_clks->smi_larb0_ck);
	if (ret != 0)
		DRM_ERROR("clk_prepare(disp_clks->smi_larb0_ck) error!\n");

	ret = clk_enable(disp_clks->smi_larb0_ck);
	if (ret != 0)
		DRM_ERROR("clk_enable(disp_clks->smi_larb0_ck) error!\n");

	/* smi_larb4_ck */
	ret = clk_prepare(disp_clks->smi_larb4_ck);
	if (ret != 0)
		DRM_ERROR("clk_prepare(disp_clks->smi_larb4_ck) error!\n");

	ret = clk_enable(disp_clks->smi_larb4_ck);
	if (ret != 0)
		DRM_ERROR("clk_enable(disp_clks->smi_larb4_ck) error!\n");

	/* mutex_disp_ck */
	ret = clk_prepare(disp_clks->mutex_disp_ck);
	if (ret != 0)
		DRM_ERROR("clk_prepare(disp_clks->mutex_disp_ck) error!\n");

	ret = clk_enable(disp_clks->mutex_disp_ck);
	if (ret != 0)
		DRM_ERROR("clk_enable(disp_clks->mutex_disp_ck) error!\n");

	/* ovl0_disp_ck */
	ret = clk_prepare(disp_clks->ovl0_disp_ck);
	if (ret != 0)
		DRM_ERROR("clk_prepare(disp_clks->ovl0_disp_ck) error!\n");

	ret = clk_enable(disp_clks->ovl0_disp_ck);
	if (ret != 0)
		DRM_ERROR("clk_enable(disp_clks->ovl0_disp_ck) error!\n");

	/* rdma0_disp_ck */
	ret = clk_prepare(disp_clks->rdma0_disp_ck);
	if (ret != 0)
		DRM_ERROR("clk_prepare(disp_clks->rdma0_disp_ck) error!\n");

	ret = clk_enable(disp_clks->rdma0_disp_ck);
	if (ret != 0)
		DRM_ERROR("clk_enable(disp_clks->rdma0_disp_ck) error!\n");

	/* color0_disp_ck */
	ret = clk_prepare(disp_clks->color0_disp_ck);
	if (ret != 0)
		DRM_ERROR("clk_prepare(disp_clks->color0_disp_ck) error!\n");

	ret = clk_enable(disp_clks->color0_disp_ck);
	if (ret != 0)
		DRM_ERROR("clk_enable(disp_clks->color0_disp_ck) error!\n");

	/* aal_disp_ck */
	ret = clk_prepare(disp_clks->aal_disp_ck);
	if (ret != 0)
		DRM_ERROR("clk_prepare(disp_clks->aal_disp_ck) error!\n");

	ret = clk_enable(disp_clks->aal_disp_ck);
	if (ret != 0)
		DRM_ERROR("clk_enable(disp_clks->aal_disp_ck) error!\n");

	/* ufoe_disp_ck */
	ret = clk_prepare(disp_clks->ufoe_disp_ck);
	if (ret != 0)
		DRM_ERROR("clk_prepare(disp_clks->ufoe_disp_ck) error!\n");

	ret = clk_enable(disp_clks->ufoe_disp_ck);
	if (ret != 0)
		DRM_ERROR("clk_enable(disp_clks->ufoe_disp_ck) error!\n");

	/* od_disp_ck */
	ret = clk_prepare(disp_clks->od_disp_ck);
	if (ret != 0)
		DRM_ERROR("clk_prepare(disp_clks->od_disp_ck) error!\n");

	ret = clk_enable(disp_clks->od_disp_ck);
	if (ret != 0)
		DRM_ERROR("clk_enable(disp_clks->od_disp_ck) error!\n");

}

void DispClockOff(struct MTK_DISP_CLKS *disp_clks)
{
	/* disp_mtcmos */
	clk_disable(disp_clks->disp_mtcmos);
	clk_unprepare(disp_clks->disp_mtcmos);

	/* smi_common_ck */
	clk_disable(disp_clks->smi_common_ck);
	clk_unprepare(disp_clks->smi_common_ck);

	/* smi_larb0_ck */
	clk_disable(disp_clks->smi_larb0_ck);
	clk_unprepare(disp_clks->smi_larb0_ck);

	/* smi_larb4_ck */
	clk_disable(disp_clks->smi_larb4_ck);
	clk_unprepare(disp_clks->smi_larb4_ck);

	/* mutex_disp_ck */
	clk_disable(disp_clks->mutex_disp_ck);
	clk_unprepare(disp_clks->mutex_disp_ck);

	/* ovl0_disp_ck */
	clk_disable(disp_clks->ovl0_disp_ck);
	clk_unprepare(disp_clks->ovl0_disp_ck);

	/* rdma0_disp_ck */
	clk_disable(disp_clks->rdma0_disp_ck);
	clk_unprepare(disp_clks->rdma0_disp_ck);

	/* color0_disp_ck */
	clk_disable(disp_clks->color0_disp_ck);
	clk_unprepare(disp_clks->color0_disp_ck);

	/* aal_disp_ck */
	clk_disable(disp_clks->aal_disp_ck);
	clk_unprepare(disp_clks->aal_disp_ck);

	/* ufoe_disp_ck */
	clk_disable(disp_clks->ufoe_disp_ck);
	clk_unprepare(disp_clks->ufoe_disp_ck);

	/* od_disp_ck */
	clk_disable(disp_clks->od_disp_ck);
	clk_unprepare(disp_clks->od_disp_ck);
}

void DispClockClear(void __iomem *drm_disp_base)
{
	unsigned int reg;

	/* ovl_disp_ck */
	reg = readl(drm_disp_base + DISP_REG_CONFIG_MMSYS_CG_CON0);
	/* reg |= (0xE131U); FIXME */
	writel(reg, drm_disp_base + DISP_REG_CONFIG_MMSYS_CG_CON0);
}

