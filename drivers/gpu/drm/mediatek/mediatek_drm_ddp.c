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
#include <linux/pm_runtime.h>

#include "mediatek_drm_crtc.h"
#include "mediatek_drm_ddp.h"


#define DISP_REG_CONFIG_DISP_OVL0_MOUT_EN	0x040
#define DISP_REG_CONFIG_DISP_OVL1_MOUT_EN	0x044
#define DISP_REG_CONFIG_DISP_OD_MOUT_EN		0x048
#define DISP_REG_CONFIG_DISP_GAMMA_MOUT_EN	0x04C
#define DISP_REG_CONFIG_DISP_UFOE_MOUT_EN	0x050
#define DISP_REG_CONFIG_DISP_COLOR0_SEL_IN	0x084
#define DISP_REG_CONFIG_DISP_COLOR1_SEL_IN	0x088
#define DISP_REG_CONFIG_DPI_SEL_IN		0x0AC
#define DISP_REG_CONFIG_DISP_PATH1_SOUT_SEL_IN	0x0C8
#define DISP_REG_CONFIG_MMSYS_CG_CON0		0x100

#define DISP_REG_CONFIG_MUTEX_EN(n)      (0x20 + 0x20 * n)
#define DISP_REG_CONFIG_MUTEX_MOD(n)     (0x2C + 0x20 * n)
#define DISP_REG_CONFIG_MUTEX_SOF(n)     (0x30 + 0x20 * n)

#define DISP_REG_OVL_INTEN                       0x0004
#define DISP_REG_OVL_INTSTA                      0x0008
#define DISP_REG_OVL_EN                          0x000C
#define DISP_REG_OVL_RST                         0x0014
#define DISP_REG_OVL_ROI_SIZE                    0x0020
#define DISP_REG_OVL_ROI_BGCLR                   0x0028
#define DISP_REG_OVL_SRC_CON                     0x002C
#define DISP_REG_OVL_L0_CON                      0x0030
#define DISP_REG_OVL_L0_SRCKEY                   0x0034
#define DISP_REG_OVL_L0_SRC_SIZE                 0x0038
#define DISP_REG_OVL_L0_OFFSET                   0x003C
#define DISP_REG_OVL_L0_PITCH                    0x0044
#define DISP_REG_OVL_L1_CON                      0x0050
#define DISP_REG_OVL_L1_SRCKEY                   0x0054
#define DISP_REG_OVL_L1_SRC_SIZE                 0x0058
#define DISP_REG_OVL_L1_OFFSET                   0x005C
#define DISP_REG_OVL_L1_PITCH                    0x0064
#define DISP_REG_OVL_RDMA0_CTRL                  0x00C0
#define DISP_REG_OVL_RDMA0_MEM_GMC_SETTING       0x00C8
#define DISP_REG_OVL_RDMA1_CTRL                  0x00E0
#define DISP_REG_OVL_RDMA1_MEM_GMC_SETTING       0x00E8
#define DISP_REG_OVL_RDMA1_FIFO_CTRL             0x00F0
#define DISP_REG_OVL_L0_ADDR                     0x0f40
#define DISP_REG_OVL_L1_ADDR                     0x0f60

#define DISP_REG_RDMA_INT_ENABLE          0x0000
#define DISP_REG_RDMA_INT_STATUS          0x0004
#define DISP_REG_RDMA_GLOBAL_CON          0x0010
#define DISP_REG_RDMA_SIZE_CON_0          0x0014
#define DISP_REG_RDMA_SIZE_CON_1          0x0018
#define DISP_REG_RDMA_FIFO_CON            0x0040

#define DISP_OD_EN                              0x000
#define DISP_OD_INTEN                           0x008
#define DISP_OD_INTS                            0x00C
#define DISP_OD_CFG                             0x020
#define DISP_OD_SIZE                            0x030

#define DISP_REG_UFO_START			0x000

#define DISP_COLOR_CFG_MAIN			0x400
#define DISP_COLOR_START			0xC00


enum DISPLAY_PATH {
	PRIMARY_PATH = 0,
	EXTERNAL_PATH = 1,
};

enum RDMA_MODE {
	RDMA_MODE_DIRECT_LINK = 0,
	RDMA_MODE_MEMORY = 1,
};

enum RDMA_OUTPUT_FORMAT {
	RDMA_OUTPUT_FORMAT_ARGB = 0,
	RDMA_OUTPUT_FORMAT_YUV444 = 1,
};

#define OVL_COLOR_BASE 30
enum OVL_INPUT_FORMAT {
	OVL_INFMT_RGB565 = 0,
	OVL_INFMT_RGB888 = 1,
	OVL_INFMT_RGBA8888 = 2,
	OVL_INFMT_ARGB8888 = 3,
	OVL_INFMT_UYVY = 4,
	OVL_INFMT_YUYV = 5,
	OVL_INFMT_UNKNOWN = 16,

	OVL_INFMT_BGR565 = OVL_INFMT_RGB565 + OVL_COLOR_BASE,
	OVL_INFMT_BGR888 = OVL_INFMT_RGB888 + OVL_COLOR_BASE,
	OVL_INFMT_BGRA8888 = OVL_INFMT_RGBA8888 + OVL_COLOR_BASE,
	OVL_INFMT_ABGR8888 = OVL_INFMT_ARGB8888 + OVL_COLOR_BASE,
};

enum {
	MUTEX_MOD_OVL0      = 11,
	MUTEX_MOD_RDMA0     = 13,
	MUTEX_MOD_COLOR0    = 18,
	MUTEX_MOD_AAL       = 20,
	MUTEX_MOD_UFOE      = 22,
	MUTEX_MOD_PWM0      = 23,
	MUTEX_MOD_OD        = 25,
};

enum {
	MUTEX_SOF_DSI0      = 1,
};

enum {
	OVL0_MOUT_EN_COLOR0     = 0x1,
};

enum {
	OD_MOUT_EN_RDMA0        = 0x1,
};

enum {
	UFOE_MOUT_EN_DSI0       = 0x1,
};

enum {
	COLOR0_SEL_IN_OVL0      = 0x1,
};

enum {
	OD_RELAY_MODE           = 0x1,
};

enum {
	UFO_BYPASS              = 0x4,
};

enum {
	COLOR_BYPASS_ALL        = (1UL<<7),
	COLOR_SEQ_SEL           = (1UL<<13),
};

enum {
	OVL_LAYER_SRC_DRAM      = 0,
};


void mtk_enable_vblank(struct drm_crtc *crtc)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);

	if (mtk_crtc->pipe == 0)
		writel(0x1, mtk_crtc->od_regs + DISP_OD_INTEN);
	else
		writel(0x2, mtk_crtc->ovl_regs[1] + DISP_REG_OVL_INTEN);
}

void mtk_disable_vblank(struct drm_crtc *crtc)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);

	if (mtk_crtc->pipe == 0)
		writel(0x0, mtk_crtc->od_regs + DISP_OD_INTEN);
	else
		writel(0x0, mtk_crtc->ovl_regs[1] + DISP_REG_OVL_INTEN);
}

void mtk_clear_vblank(struct drm_crtc *crtc)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);

	if (mtk_crtc->pipe == 0)
		writel(0x0, mtk_crtc->od_regs + DISP_OD_INTS);
	else
		writel(0x0, mtk_crtc->ovl_regs[1] + DISP_REG_OVL_INTSTA);
}

static void disp_config_main_path_connection(void __iomem *disp_base)
{
	writel(OVL0_MOUT_EN_COLOR0,
		disp_base + DISP_REG_CONFIG_DISP_OVL0_MOUT_EN);
	writel(OD_MOUT_EN_RDMA0, disp_base + DISP_REG_CONFIG_DISP_OD_MOUT_EN);
	writel(UFOE_MOUT_EN_DSI0,
		disp_base + DISP_REG_CONFIG_DISP_UFOE_MOUT_EN);
	writel(COLOR0_SEL_IN_OVL0,
		disp_base + DISP_REG_CONFIG_DISP_COLOR0_SEL_IN);
}

static void disp_config_main_path_mutex(void __iomem *mutex_base)
{
	unsigned int id = 0;

	writel((1 << MUTEX_MOD_OVL0 | 1 << MUTEX_MOD_RDMA0 |
		1 << MUTEX_MOD_COLOR0 | 1 << MUTEX_MOD_AAL |
		1 << MUTEX_MOD_UFOE | 1 << MUTEX_MOD_PWM0 |
		1 << MUTEX_MOD_OD),
		mutex_base + DISP_REG_CONFIG_MUTEX_MOD(id));

	writel(MUTEX_SOF_DSI0, mutex_base + DISP_REG_CONFIG_MUTEX_SOF(id));
	writel(1, mutex_base + DISP_REG_CONFIG_MUTEX_EN(id));
}

#ifndef MEDIATEK_DRM_UPSTREAM
static void disp_config_ext_path_connection(void __iomem *disp_base)
{
	/* OVL1 output to COLOR1 */
	writel(0x1, disp_base + DISP_REG_CONFIG_DISP_OVL1_MOUT_EN);

	/* GAMME output to RDMA1 */
	writel(0x1, disp_base + DISP_REG_CONFIG_DISP_GAMMA_MOUT_EN);

	/* PATH1 output to DPI */
	writel(0x2, disp_base + DISP_REG_CONFIG_DISP_PATH1_SOUT_SEL_IN);

	/* DPI input from PATH1 */
	writel(0x1, disp_base + DISP_REG_CONFIG_DPI_SEL_IN);

	/* COLOR1 input from OVL1 */
	writel(0x1, disp_base + DISP_REG_CONFIG_DISP_COLOR1_SEL_IN);
}

static void disp_config_ext_path_mutex(void __iomem *mutex_base)
{
	unsigned int ID = 1;

	/* Module: OVL1=12, RDMA1=14, COLOR1=19, GAMMA=21 */
	writel((1 << 12 | 1 << 14 | 1 << 19 | 1 << 21),
		mutex_base + DISP_REG_CONFIG_MUTEX_MOD(ID));

	/* Clock source from DPI0 */
	writel(3, mutex_base + DISP_REG_CONFIG_MUTEX_SOF(ID));
	writel(1, mutex_base + DISP_REG_CONFIG_MUTEX_EN(ID));
}
#endif /* MEDIATEK_DRM_UPSTREAM */

static void ovl_start(void __iomem *ovl_base[2], unsigned idx)
{
	BUG_ON(idx >= 2);

	writel(0x01, ovl_base[idx] + DISP_REG_OVL_EN);
}

static void ovl_stop(void __iomem *ovl_base[2], unsigned idx)
{
	BUG_ON(idx >= 2);

	writel(0x00, ovl_base[idx] + DISP_REG_OVL_EN);
}

static void ovl_roi(void __iomem *ovl_base[2], unsigned idx,
	unsigned int w, unsigned int h,	unsigned int bg_color)
{
	BUG_ON(idx >= 2);

	writel(h << 16 | w, ovl_base[idx] + DISP_REG_OVL_ROI_SIZE);
	writel(bg_color, ovl_base[idx] + DISP_REG_OVL_ROI_BGCLR);
}

void ovl_layer_switch(void __iomem *ovl_base[2], unsigned idx,
	unsigned layer, bool en)
{
	u32 reg;

	BUG_ON(idx >= 2);

	reg = readl(ovl_base[idx] + DISP_REG_OVL_SRC_CON);
	if (en)
		reg |= (1U<<layer);
	else
		reg &= ~(1U<<layer);

	writel(reg, ovl_base[idx] + DISP_REG_OVL_SRC_CON);
	writel(0x1, ovl_base[idx] + DISP_REG_OVL_RST);
	writel(0x0, ovl_base[idx] + DISP_REG_OVL_RST);
}

static void rdma_start(void __iomem *rdma_base[2], unsigned idx)
{
	unsigned int reg;

	BUG_ON(idx >= 2);

	writel(0x4, rdma_base[idx] + DISP_REG_RDMA_INT_ENABLE);
	reg = readl(rdma_base[idx] + DISP_REG_RDMA_GLOBAL_CON);
	reg |= 1;
	writel(reg, rdma_base[idx] + DISP_REG_RDMA_GLOBAL_CON);
}

static void rdma_stop(void __iomem *rdma_base[2], unsigned idx)
{
	unsigned int reg;

	BUG_ON(idx >= 2);

	reg = readl(rdma_base[idx] + DISP_REG_RDMA_GLOBAL_CON);
	reg &= ~(1U);
	writel(reg, rdma_base[idx] + DISP_REG_RDMA_GLOBAL_CON);
	writel(0, rdma_base[idx] + DISP_REG_RDMA_INT_ENABLE);
	writel(0, rdma_base[idx] + DISP_REG_RDMA_INT_STATUS);
}

static void rdma_reset(void __iomem *rdma_base[2], unsigned idx)
{
	unsigned int reg;
	unsigned int delay_cnt;

	BUG_ON(idx >= 2);

	reg = readl(rdma_base[idx] + DISP_REG_RDMA_GLOBAL_CON);
	reg |= 0x10;
	writel(reg, rdma_base[idx] + DISP_REG_RDMA_GLOBAL_CON);

	delay_cnt = 0;
	while ((readl(rdma_base[idx] + DISP_REG_RDMA_GLOBAL_CON)
		& 0x700) == 0x100) {

		delay_cnt++;
		if (delay_cnt > 10000) {
			DRM_ERROR("RDMA[%d] Reset timeout\n", idx);
			break;
		}
	}

	reg = readl(rdma_base[idx] + DISP_REG_RDMA_GLOBAL_CON);
	reg &= ~(0x10U);
	writel(reg, rdma_base[idx] + DISP_REG_RDMA_GLOBAL_CON);

	delay_cnt = 0;
	while ((readl(rdma_base[idx] + DISP_REG_RDMA_GLOBAL_CON)
		& 0x700) != 0x100) {

		delay_cnt++;
		if (delay_cnt > 10000) {
			DRM_ERROR("RDMA[%d] Reset timeout\n", idx);
			break;
		}
	}
}

static void rdma_config_direct_link(void __iomem *rdma_base[2], unsigned idx,
	unsigned width, unsigned height)
{
	unsigned int reg;
	enum RDMA_MODE mode = RDMA_MODE_DIRECT_LINK;
	enum RDMA_OUTPUT_FORMAT output_format = RDMA_OUTPUT_FORMAT_ARGB;

	BUG_ON(idx >= 2);

	reg = readl(rdma_base[idx] + DISP_REG_RDMA_GLOBAL_CON);
	if (mode == RDMA_MODE_DIRECT_LINK)
		reg &= ~(0x2U);
	writel(reg, rdma_base[idx] + DISP_REG_RDMA_GLOBAL_CON);

	reg = readl(rdma_base[idx] + DISP_REG_RDMA_SIZE_CON_0);
	if (output_format == RDMA_OUTPUT_FORMAT_ARGB)
		reg &= ~(0x20000000U);
	else
		reg |= 0x20000000U;
	writel(reg, rdma_base[idx] + DISP_REG_RDMA_SIZE_CON_0);

	reg = readl(rdma_base[idx] + DISP_REG_RDMA_SIZE_CON_0);
	reg = (reg & ~(0xFFFU)) | (width & 0xFFFU);
	writel(reg, rdma_base[idx] + DISP_REG_RDMA_SIZE_CON_0);

	reg = readl(rdma_base[idx] + DISP_REG_RDMA_SIZE_CON_1);
	reg = (reg & ~(0xFFFFFU)) | (height & 0xFFFFFU);
	writel(reg, rdma_base[idx] + DISP_REG_RDMA_SIZE_CON_1);

	writel(0x80F00008, rdma_base[idx] + DISP_REG_RDMA_FIFO_CON);
}

static void od_start(void __iomem *od_base, unsigned int w, unsigned int h)
{
	writel(w << 16 | h, od_base + DISP_OD_SIZE);
	writel(OD_RELAY_MODE, od_base + DISP_OD_CFG);
	writel(1, od_base + DISP_OD_EN);
}

static void ufoe_start(void __iomem *ufoe_base)
{
	writel(UFO_BYPASS, ufoe_base + DISP_REG_UFO_START);
}

static void color_start(void __iomem *color_base[2], unsigned idx)
{
	writel(COLOR_BYPASS_ALL | COLOR_SEQ_SEL,
		color_base[idx] + DISP_COLOR_CFG_MAIN);
	writel(0x1, color_base[idx] + DISP_COLOR_START);
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
		return OVL_INFMT_UNKNOWN;
	}
}

#ifndef MEDIATEK_DRM_UPSTREAM
void ovl_layer_config_cursor(struct drm_crtc *crtc, unsigned int addr,
	int x, int y, bool enabled)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	void __iomem *drm_disp_base;
	unsigned int reg;
/*	unsigned int layer = 1; */
	unsigned int width = 64;
	unsigned int height = 64;
	unsigned int src_pitch = 64 * 4;
	bool keyEn = 0;
	bool aen = 1;			/* alpha enable */
	unsigned char alpha = 0xFF;
	unsigned int fmt = OVL_INFMT_RGBA8888;
	unsigned int src_con, new_set;

	BUG_ON(mtk_crtc->pipe >= 2);
	drm_disp_base = mtk_crtc->ovl_regs[mtk_crtc->pipe];

	if (width + x > crtc->mode.hdisplay)
		width = crtc->mode.hdisplay - min(x, crtc->mode.hdisplay);

	if (height + y > crtc->mode.vdisplay)
		height = crtc->mode.vdisplay - min(y, crtc->mode.vdisplay);

	src_con = readl(drm_disp_base + DISP_REG_OVL_SRC_CON);
	if (enabled == true)
		new_set = src_con | 0x2;
	else
		new_set = src_con & ~(0x2);

	if (new_set != src_con) {
		writel(new_set, drm_disp_base + DISP_REG_OVL_SRC_CON);
		writel(0x00000001, drm_disp_base + DISP_REG_OVL_RDMA1_CTRL);
		writel(0x40402020, drm_disp_base + DISP_REG_OVL_RDMA1_MEM_GMC_SETTING);

		reg = keyEn << 30 | fmt << 12 | aen << 8 | alpha;
		writel(reg, drm_disp_base + DISP_REG_OVL_L1_CON);
		writel(src_pitch & 0xFFFF, drm_disp_base + DISP_REG_OVL_L1_PITCH);
	}
	writel(height << 16 | width, drm_disp_base + DISP_REG_OVL_L1_SRC_SIZE);
	writel(y << 16 | x, drm_disp_base + DISP_REG_OVL_L1_OFFSET);
	writel(addr, drm_disp_base + DISP_REG_OVL_L1_ADDR);
}
#endif /* MEDIATEK_DRM_UPSTREAM */

void ovl_layer_config(struct drm_crtc *crtc, unsigned int addr,
	unsigned int format, bool enabled)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	void __iomem *drm_disp_base;
	unsigned int reg;
	unsigned int src_pitch;
	unsigned int dst_x = 0;
	unsigned int dst_y = 0;
	unsigned int dst_w = crtc->mode.hdisplay;
	unsigned int dst_h = crtc->mode.vdisplay;
	bool color_key_en = 1;
	unsigned int color_key = 0xFF000000;
	bool alpha_en = 0;
	unsigned char alpha = 0x0;
	unsigned int src_con, new_set;

	unsigned int rgb_swap, bpp;
	unsigned int fmt = ovl_fmt_convert(format);

	BUG_ON(mtk_crtc->pipe >= 2);
	drm_disp_base = mtk_crtc->ovl_regs[mtk_crtc->pipe];

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
		bpp = 1;
	}

	if (crtc->primary->fb && crtc->primary->fb->pitches[0])
		src_pitch = crtc->primary->fb->pitches[0];
	else
		src_pitch = crtc->mode.hdisplay * bpp;

	src_con = readl(drm_disp_base + DISP_REG_OVL_SRC_CON);
	if (enabled == true)
		new_set = src_con | 0x1;
	else
		new_set = src_con & ~(0x1);

	writel(0x1, drm_disp_base + DISP_REG_OVL_RST);
	writel(0x0, drm_disp_base + DISP_REG_OVL_RST);

	writel(new_set, drm_disp_base + DISP_REG_OVL_SRC_CON);

	writel(0x00000001, drm_disp_base + DISP_REG_OVL_RDMA0_CTRL);
	writel(0x40402020, drm_disp_base + DISP_REG_OVL_RDMA0_MEM_GMC_SETTING);

	reg = color_key_en << 30 | OVL_LAYER_SRC_DRAM << 28 |
		rgb_swap << 25 | fmt << 12 | alpha_en << 8 | alpha;
	writel(reg, drm_disp_base + DISP_REG_OVL_L0_CON);
	writel(color_key, drm_disp_base + DISP_REG_OVL_L0_SRCKEY);
	writel(dst_h << 16 | dst_w, drm_disp_base + DISP_REG_OVL_L0_SRC_SIZE);
	writel(dst_y << 16 | dst_x, drm_disp_base + DISP_REG_OVL_L0_OFFSET);
	writel(addr, drm_disp_base + DISP_REG_OVL_L0_ADDR);
	writel(src_pitch & 0xFFFF, drm_disp_base + DISP_REG_OVL_L0_PITCH);
}

void main_disp_path_power_on(struct drm_crtc *crtc)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);

	ovl_start(mtk_crtc->ovl_regs, mtk_crtc->pipe);
	rdma_start(mtk_crtc->rdma_regs, mtk_crtc->pipe);
}

void main_disp_path_power_off(struct drm_crtc *crtc)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);

	ovl_stop(mtk_crtc->ovl_regs, mtk_crtc->pipe);
	rdma_stop(mtk_crtc->rdma_regs, mtk_crtc->pipe);
	rdma_reset(mtk_crtc->rdma_regs, mtk_crtc->pipe);
}

void main_disp_path_setup(struct drm_crtc *crtc)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct device_node *node;

	unsigned int width, height;
	int err;

	node = of_find_compatible_node(NULL, NULL, "mediatek,mt8173-dsi");

	err = of_property_read_u32(node, "mediatek,width", &width);
	if (err < 0)
		return;

	err = of_property_read_u32(node, "mediatek,height", &height);
	if (err < 0)
		return;

	width = ((width + 3)>>2)<<2;
	DRM_INFO("DBG_YT MainDispPathSetup %d %d\n", width, height);

	ovl_roi(mtk_crtc->ovl_regs, PRIMARY_PATH, width, height, 0x00000000);
	ovl_layer_switch(mtk_crtc->ovl_regs, PRIMARY_PATH, 0, 1);
	rdma_config_direct_link(mtk_crtc->rdma_regs,
		PRIMARY_PATH, width, height);
	od_start(mtk_crtc->od_regs, width, height);
	ufoe_start(mtk_crtc->ufoe_regs);
	color_start(mtk_crtc->color_regs, PRIMARY_PATH);
	disp_config_main_path_connection(mtk_crtc->regs);
	disp_config_main_path_mutex(mtk_crtc->mutex_regs);
}

#ifndef MEDIATEK_DRM_UPSTREAM
void ext_disp_path_power_on(struct drm_crtc *crtc)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);

	ovl_start(mtk_crtc->ovl_regs, EXTERNAL_PATH);
	rdma_start(mtk_crtc->rdma_regs, EXTERNAL_PATH);
}

void ext_disp_path_power_off(struct drm_crtc *crtc)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);

	ovl_stop(mtk_crtc->ovl_regs, EXTERNAL_PATH);
	rdma_stop(mtk_crtc->rdma_regs, EXTERNAL_PATH);
	rdma_reset(mtk_crtc->rdma_regs, EXTERNAL_PATH);
}

void ext_disp_path_setup(struct drm_crtc *crtc)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	unsigned int width, height;

	width = 1920;
	height = 1080;
	DRM_INFO("DBG_YT ext_disp_path_setup %d %d\n", width, height);

	/* Setup OVL1 */
	ovl_roi(mtk_crtc->ovl_regs, EXTERNAL_PATH, width, height, 0x00000000);
	ovl_layer_switch(mtk_crtc->ovl_regs, EXTERNAL_PATH, 0, 1);

	/* Setup RDMA1 */
	rdma_config_direct_link(mtk_crtc->rdma_regs,
		EXTERNAL_PATH, width, height);

	/* Setup Color1 */
	color_start(mtk_crtc->color_regs, EXTERNAL_PATH);

	/* Setup main path connection */
	disp_config_ext_path_connection(mtk_crtc->regs);

	/* Setup main path mutex */
	disp_config_ext_path_mutex(mtk_crtc->mutex_regs);
}
#endif /* MEDIATEK_DRM_UPSTREAM */

void disp_clock_setup(struct device *dev, struct MTK_DISP_CLKS **pdisp_clks)
{
	*pdisp_clks = vmalloc(sizeof(struct MTK_DISP_CLKS));

	pm_runtime_enable(dev);
	(*pdisp_clks)->dev = dev;

	(*pdisp_clks)->mutex_disp_ck = devm_clk_get(dev, "mutex_disp_ck");
	if (IS_ERR((*pdisp_clks)->mutex_disp_ck))
		DRM_ERROR("disp_clock_setup: Get mutex_disp_ck fail.\n");

	(*pdisp_clks)->ovl0_disp_ck = devm_clk_get(dev, "ovl0_disp_ck");
	if (IS_ERR((*pdisp_clks)->ovl0_disp_ck))
		DRM_ERROR("disp_clock_setup: Get ovl0_disp_ck fail.\n");

#ifndef MEDIATEK_DRM_UPSTREAM
	(*pdisp_clks)->ovl1_disp_ck = devm_clk_get(dev, "ovl1_disp_ck");
	if (IS_ERR((*pdisp_clks)->ovl1_disp_ck))
		DRM_ERROR("DispClockSetup: Get ovl1_disp_ck fail.\n");
#endif /* MEDIATEK_DRM_UPSTREAM */

	(*pdisp_clks)->rdma0_disp_ck = devm_clk_get(dev, "rdma0_disp_ck");
	if (IS_ERR((*pdisp_clks)->rdma0_disp_ck))
		DRM_ERROR("disp_clock_setup: Get rdma0_disp_ck.\n");

#ifndef MEDIATEK_DRM_UPSTREAM
	(*pdisp_clks)->rdma1_disp_ck = devm_clk_get(dev, "rdma1_disp_ck");
	if (IS_ERR((*pdisp_clks)->rdma1_disp_ck))
		DRM_ERROR("DispClockSetup: Get rdma1_disp_ck.\n");
#endif /* MEDIATEK_DRM_UPSTREAM */

	(*pdisp_clks)->color0_disp_ck = devm_clk_get(dev, "color0_disp_ck");
	if (IS_ERR((*pdisp_clks)->color0_disp_ck))
		DRM_ERROR("disp_clock_setup: Get color0_disp_ck fail.\n");

#ifndef MEDIATEK_DRM_UPSTREAM
	(*pdisp_clks)->color1_disp_ck = devm_clk_get(dev, "color1_disp_ck");
	if (IS_ERR((*pdisp_clks)->color1_disp_ck))
		DRM_ERROR("DispClockSetup: Get color1_disp_ck fail.\n");
#endif /* MEDIATEK_DRM_UPSTREAM */

	(*pdisp_clks)->aal_disp_ck = devm_clk_get(dev, "aal_disp_ck");
	if (IS_ERR((*pdisp_clks)->aal_disp_ck))
		DRM_ERROR("disp_clock_setup: Get aal_disp_ck fail.\n");

#ifndef MEDIATEK_DRM_UPSTREAM
	(*pdisp_clks)->gamma_disp_ck = devm_clk_get(dev, "gamma_disp_ck");
	if (IS_ERR((*pdisp_clks)->gamma_disp_ck))
		DRM_ERROR("DispClockSetup: Get gamma_disp_ck fail.\n");
#endif /* MEDIATEK_DRM_UPSTREAM */

	(*pdisp_clks)->ufoe_disp_ck = devm_clk_get(dev, "ufoe_disp_ck");
	if (IS_ERR((*pdisp_clks)->ufoe_disp_ck))
		DRM_ERROR("disp_clock_setup: Get ufoe_disp_ck fail.\n");

	(*pdisp_clks)->od_disp_ck = devm_clk_get(dev, "od_disp_ck");
	if (IS_ERR((*pdisp_clks)->od_disp_ck))
		DRM_ERROR("disp_clock_setup: Get od_disp_ck fail.\n");

}

void disp_clock_on(struct MTK_DISP_CLKS *disp_clks)
{
	int ret;

	/* disp_mtcmos */
	ret = pm_runtime_get_sync(disp_clks->dev);
	if (ret < 0)
		DRM_ERROR("failed to get_sync(%d)\n", ret);

	ret = clk_prepare(disp_clks->mutex_disp_ck);
	if (ret != 0)
		DRM_ERROR("clk_prepare(disp_clks->mutex_disp_ck) error!\n");

	ret = clk_enable(disp_clks->mutex_disp_ck);
	if (ret != 0)
		DRM_ERROR("clk_enable(disp_clks->mutex_disp_ck) error!\n");

	ret = clk_prepare(disp_clks->ovl0_disp_ck);
	if (ret != 0)
		DRM_ERROR("clk_prepare(disp_clks->ovl0_disp_ck) error!\n");

	ret = clk_enable(disp_clks->ovl0_disp_ck);
	if (ret != 0)
		DRM_ERROR("clk_enable(disp_clks->ovl0_disp_ck) error!\n");

#ifndef MEDIATEK_DRM_UPSTREAM
	/* ovl1_disp_ck */
	ret = clk_prepare(disp_clks->ovl1_disp_ck);
	if (ret != 0)
		DRM_ERROR("clk_prepare(disp_clks->ovl1_disp_ck) error!\n");

	ret = clk_enable(disp_clks->ovl1_disp_ck);
	if (ret != 0)
		DRM_ERROR("clk_enable(disp_clks->ovl1_disp_ck) error!\n");
#endif /* MEDIATEK_DRM_UPSTREAM */

	ret = clk_prepare(disp_clks->rdma0_disp_ck);
	if (ret != 0)
		DRM_ERROR("clk_prepare(disp_clks->rdma0_disp_ck) error!\n");

	ret = clk_enable(disp_clks->rdma0_disp_ck);
	if (ret != 0)
		DRM_ERROR("clk_enable(disp_clks->rdma0_disp_ck) error!\n");

#ifndef MEDIATEK_DRM_UPSTREAM
	/* rdma1_disp_ck */
	ret = clk_prepare(disp_clks->rdma1_disp_ck);
	if (ret != 0)
		DRM_ERROR("clk_prepare(disp_clks->rdma1_disp_ck) error!\n");

	ret = clk_enable(disp_clks->rdma1_disp_ck);
	if (ret != 0)
		DRM_ERROR("clk_enable(disp_clks->rdma1_disp_ck) error!\n");
#endif /* MEDIATEK_DRM_UPSTREAM */

	ret = clk_prepare(disp_clks->color0_disp_ck);
	if (ret != 0)
		DRM_ERROR("clk_prepare(disp_clks->color0_disp_ck) error!\n");

	ret = clk_enable(disp_clks->color0_disp_ck);
	if (ret != 0)
		DRM_ERROR("clk_enable(disp_clks->color0_disp_ck) error!\n");

#ifndef MEDIATEK_DRM_UPSTREAM
	/* color1_disp_ck */
	ret = clk_prepare(disp_clks->color1_disp_ck);
	if (ret != 0)
		DRM_ERROR("clk_prepare(disp_clks->color1_disp_ck) error!\n");

	ret = clk_enable(disp_clks->color1_disp_ck);
	if (ret != 0)
		DRM_ERROR("clk_enable(disp_clks->color1_disp_ck) error!\n");
#endif /* MEDIATEK_DRM_UPSTREAM */

	ret = clk_prepare(disp_clks->aal_disp_ck);
	if (ret != 0)
		DRM_ERROR("clk_prepare(disp_clks->aal_disp_ck) error!\n");

	ret = clk_enable(disp_clks->aal_disp_ck);
	if (ret != 0)
		DRM_ERROR("clk_enable(disp_clks->aal_disp_ck) error!\n");

#ifndef MEDIATEK_DRM_UPSTREAM
	/* gamma_disp_ck */
	ret = clk_prepare(disp_clks->gamma_disp_ck);
	if (ret != 0)
		DRM_ERROR("clk_prepare(disp_clks->gamma_disp_ck) error!\n");

	ret = clk_enable(disp_clks->gamma_disp_ck);
	if (ret != 0)
		DRM_ERROR("clk_enable(disp_clks->gamma_disp_ck) error!\n");
#endif /* MEDIATEK_DRM_UPSTREAM */

	ret = clk_prepare(disp_clks->ufoe_disp_ck);
	if (ret != 0)
		DRM_ERROR("clk_prepare(disp_clks->ufoe_disp_ck) error!\n");

	ret = clk_enable(disp_clks->ufoe_disp_ck);
	if (ret != 0)
		DRM_ERROR("clk_enable(disp_clks->ufoe_disp_ck) error!\n");

	ret = clk_prepare(disp_clks->od_disp_ck);
	if (ret != 0)
		DRM_ERROR("clk_prepare(disp_clks->od_disp_ck) error!\n");

	ret = clk_enable(disp_clks->od_disp_ck);
	if (ret != 0)
		DRM_ERROR("clk_enable(disp_clks->od_disp_ck) error!\n");

}

void disp_clock_off(struct MTK_DISP_CLKS *disp_clks)
{
	/* disp_mtcmos */
	pm_runtime_put_sync(disp_clks->dev);

	clk_disable(disp_clks->mutex_disp_ck);
	clk_unprepare(disp_clks->mutex_disp_ck);

	clk_disable(disp_clks->ovl0_disp_ck);
	clk_unprepare(disp_clks->ovl0_disp_ck);

#ifndef MEDIATEK_DRM_UPSTREAM
	/* ovl1_disp_ck */
	clk_disable(disp_clks->ovl1_disp_ck);
	clk_unprepare(disp_clks->ovl1_disp_ck);
#endif /* MEDIATEK_DRM_UPSTREAM */

	clk_disable(disp_clks->rdma0_disp_ck);
	clk_unprepare(disp_clks->rdma0_disp_ck);

#ifndef MEDIATEK_DRM_UPSTREAM
	/* rdma1_disp_ck */
	clk_disable(disp_clks->rdma1_disp_ck);
	clk_unprepare(disp_clks->rdma1_disp_ck);
#endif /* MEDIATEK_DRM_UPSTREAM */

	clk_disable(disp_clks->color0_disp_ck);
	clk_unprepare(disp_clks->color0_disp_ck);

#ifndef MEDIATEK_DRM_UPSTREAM
	/* color1_disp_ck */
	clk_disable(disp_clks->color1_disp_ck);
	clk_unprepare(disp_clks->color1_disp_ck);
#endif /* MEDIATEK_DRM_UPSTREAM */

	clk_disable(disp_clks->aal_disp_ck);
	clk_unprepare(disp_clks->aal_disp_ck);

#ifndef MEDIATEK_DRM_UPSTREAM
	/* gamma_disp_ck */
	clk_disable(disp_clks->gamma_disp_ck);
	clk_unprepare(disp_clks->gamma_disp_ck);
#endif /* MEDIATEK_DRM_UPSTREAM */

	clk_disable(disp_clks->ufoe_disp_ck);
	clk_unprepare(disp_clks->ufoe_disp_ck);

	clk_disable(disp_clks->od_disp_ck);
	clk_unprepare(disp_clks->od_disp_ck);
}

