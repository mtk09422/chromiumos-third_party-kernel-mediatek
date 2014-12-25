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

#include "mediatek_drm_hw-mt8173.h"

#ifdef PVRDRM
#include <pvr_drm_display.h>
extern int irq_num;

enum mtk_drm_crtc_flip_status {
	MTK_DRM_CRTC_FLIP_STATUS_NONE = 0,
	MTK_DRM_CRTC_FLIP_STATUS_PENDING,
	MTK_DRM_CRTC_FLIP_STATUS_DONE,
};
#endif

#define MAX_FB_BUFFER	4
#define DEFAULT_ZPOS	-1

/*
 * MediaTek drm common overlay structure.
 *
 * @fb_x: offset x on a framebuffer to be displayed.
 *	- the unit is screen coordinates.
 * @fb_y: offset y on a framebuffer to be displayed.
 *	- the unit is screen coordinates.
 * @fb_width: width of a framebuffer.
 * @fb_height: height of a framebuffer.
 * @crtc_x: offset x on hardware screen.
 * @crtc_y: offset y on hardware screen.
 * @crtc_width: window width to be displayed (hardware screen).
 * @crtc_height: window height to be displayed (hardware screen).
 * @mode_width: width of screen mode.
 * @mode_height: height of screen mode.
 * @refresh: refresh rate.
 * @scan_flag: interlace or progressive way.
 *	(it could be DRM_MODE_FLAG_*)
 * @bpp: pixel size.(in bit)
 * @pixel_format: fourcc pixel format of this overlay
 * @dma_addr: array of bus(accessed by dma) address to the memory region
 *	      allocated for a overlay.
 * @vaddr: array of virtual memory addresss to this overlay.
 * @zpos: order of overlay layer(z position).
 * @default_win: a window to be enabled.
 * @color_key: color key on or off.
 * @index_color: if using color key feature then this value would be used
 *			as index color.
 * @local_path: in case of lcd type, local path mode on or off.
 * @transparency: transparency on or off.
 * @activated: activated or not.
 *
 * this structure is common to mtk SoC and its contents would be copied
 * to hardware specific overlay info.
 */
struct mtk_drm_overlay {
	unsigned int fb_x;
	unsigned int fb_y;
	unsigned int fb_width;
	unsigned int fb_height;
	unsigned int crtc_x;
	unsigned int crtc_y;
	unsigned int crtc_width;
	unsigned int crtc_height;
	unsigned int mode_width;
	unsigned int mode_height;
	unsigned int refresh;
	unsigned int scan_flag;
	unsigned int bpp;
	unsigned int pitch;
	uint32_t pixel_format;
	dma_addr_t dma_addr[MAX_FB_BUFFER];
	void __iomem *vaddr[MAX_FB_BUFFER];
	int zpos;

	bool default_win;
	bool color_key;
	unsigned int index_color;
	bool local_path;
	bool transparency;
	bool activated;

	void *mtk_gem_buf[MAX_FB_BUFFER];
};

/*
 * MediaTek specific crtc structure.
 *
 * @drm_crtc: crtc object.
 * @overlay: contain information common to display controller and hdmi and
 *	contents of this overlay object would be copied to sub driver size.
 * @pipe: a crtc index created at load() with a new crtc object creation
 *	and the crtc object would be set to private->crtc array
 *	to get a crtc object corresponding to this pipe from private->crtc
 *	array when irq interrupt occured. the reason of using this pipe is that
 *	drm framework doesn't support multiple irq yet.
 *	we can refer to the crtc to current hardware interrupt occured through
 *	this pipe value.
 * @dpms: store the crtc dpms value
 */
struct mtk_drm_crtc {
	struct drm_crtc			base;
	void __iomem			*regs;
	void __iomem			*ovl_regs;
	void __iomem			*rdma_regs;
	void __iomem			*color_regs;
	void __iomem			*aal_regs;
	void __iomem			*ufoe_regs;
	void __iomem			*dsi_reg;
	void __iomem			*mutex_regs;
	void __iomem			*od_regs;
	void __iomem			*debug_regs;
	void __iomem			*dsi_ana_reg;
	struct MTK_DISP_REGS		*disp_regs;
	struct MTK_DISP_CLKS		*disp_clks;
	unsigned int			cursor_x, cursor_y;
	unsigned int			cursor_w, cursor_h;
	struct drm_gem_object		*cursor_obj;
	/* unsigned int			pipe;
	struct mtk_drm_overlay	overlay;
	unsigned int			dpms;
	add count for EGL_CHROMIUM_get_sync_values
	uint64_t				scb; */
	struct drm_pending_vblank_event	*event;
#ifdef PVRDRM
	enum mtk_drm_crtc_flip_status	flip_status;
	struct pvr_drm_flip_data	*flip_data;
	struct pvr_drm_flip_data	*wdma_data;
#endif
};

#define to_mtk_crtc(x) container_of(x, struct mtk_drm_crtc, base)

int mtk_drm_crtc_create(struct drm_device *dev, unsigned int nr);
void mtk_drm_crtc_irq(struct mtk_drm_crtc *mtk_crtc);

