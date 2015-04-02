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

#ifndef _MEDIATEK_DRM_DDP_H_
#define _MEDIATEK_DRM_DDP_H_

struct MTK_DISP_REGS {
	void __iomem *regs;
	void __iomem *ovl_regs;
	void __iomem *rdma_regs;
	void __iomem *aal_regs;
	void __iomem *mutex_regs;
	void __iomem *dsi_reg;
	void __iomem *dsi_ana_reg;
};

struct MTK_DISP_CLKS {
	struct clk *disp_mtcmos;
	struct clk *mutex_disp_ck;
	struct clk *ovl0_disp_ck;
	struct clk *ovl1_disp_ck;
	struct clk *rdma0_disp_ck;
	struct clk *rdma1_disp_ck;
	struct clk *color0_disp_ck;
	struct clk *color1_disp_ck;
	struct clk *aal_disp_ck;
	struct clk *gamma_disp_ck;
	struct clk *ufoe_disp_ck;
	struct clk *od_disp_ck;
};

void mtk_enable_vblank(void __iomem *drm_disp_base);
void mtk_disable_vblank(void __iomem *drm_disp_base);
void mtk_clear_vblank(void __iomem *drm_disp_base);
void ovl_layer_config(struct drm_crtc *crtc, unsigned int addr,
	unsigned int format, bool enabled);
#ifndef MEDIATEK_DRM_UPSTREAM
void ovl_layer_config_cursor(struct drm_crtc *crtc, unsigned int addr,
	int x, int y);
#endif /* MEDIATEK_DRM_UPSTREAM */

void main_disp_path_power_on(struct drm_crtc *crtc);
void main_disp_path_power_off(struct drm_crtc *crtc);
void main_disp_path_setup(struct drm_crtc *crtc);

#ifndef MEDIATEK_DRM_UPSTREAM
void ext_disp_path_power_on(struct drm_crtc *crtc);
void ext_disp_path_power_off(struct drm_crtc *crtc);
void ext_disp_path_setup(struct drm_crtc *crtc);
#endif /* MEDIATEK_DRM_UPSTREAM */

void disp_clock_setup(struct device *dev, struct MTK_DISP_CLKS	**pdisp_clks);
void disp_clock_on(struct MTK_DISP_CLKS *disp_clks);
void disp_clock_off(struct MTK_DISP_CLKS *disp_clks);

void ovl_layer_switch(void __iomem *ovl_base[2], unsigned idx,
	unsigned layer, bool en);

#endif
