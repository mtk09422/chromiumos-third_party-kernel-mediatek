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

#include "mediatek_drm_ddp.h"
#ifndef MEDIATEK_DRM_UPSTREAM
#include "drm_sync_helper.h"
#endif /* MEDIATEK_DRM_UPSTREAM */


#define MAX_FB_BUFFER	4
#define DEFAULT_ZPOS	-1

/*
 * MediaTek specific crtc structure.
 *
 * @base: crtc object.
 * @pipe: a crtc index created at load() with a new crtc object creation
 *	and the crtc object would be set to private->crtc array
 *	to get a crtc object corresponding to this pipe from private->crtc
 *	array when irq interrupt occurred. the reason of using this pipe is that
 *	drm framework doesn't support multiple irq yet.
 *	we can refer to the crtc to current hardware interrupt occurred through
 *	this pipe value.
 */
struct mtk_drm_crtc {
	struct drm_crtc			base;
	void __iomem			*regs;
	void __iomem			*ovl_regs[2];
	void __iomem			*rdma_regs[2];
	void __iomem			*color_regs[2];
	void __iomem			*aal_regs;
	void __iomem			*gamma_regs;
	void __iomem			*ufoe_regs;
	void __iomem			*dsi_reg;
	void __iomem			*mutex_regs;
	void __iomem			*od_regs;
	void __iomem			*dsi_ana_reg;
	struct MTK_DISP_REGS		*disp_regs;
	struct MTK_DISP_CLKS		*disp_clks;
#ifndef MEDIATEK_DRM_UPSTREAM
	unsigned int			cursor_x, cursor_y;
	unsigned int			cursor_w, cursor_h;
	struct drm_gem_object		*cursor_obj;
#endif /* MEDIATEK_DRM_UPSTREAM */
	unsigned int			pipe;
	struct drm_pending_vblank_event	*event;
	struct mtk_drm_gem_buf *flip_buffer;
#ifndef MEDIATEK_DRM_UPSTREAM

	unsigned fence_context;
	atomic_t fence_seqno;
	struct fence *fence;
	struct drm_reservation_cb rcb;
	struct fence *pending_fence;
	bool pending_needs_vblank;
#endif /* MEDIATEK_DRM_UPSTREAM */
};

#define to_mtk_crtc(x) container_of(x, struct mtk_drm_crtc, base)

int mtk_drm_crtc_create(struct drm_device *dev, unsigned int nr);
void mtk_drm_crtc_irq(struct mtk_drm_crtc *mtk_crtc);

