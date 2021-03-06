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
#include <linux/dma-buf.h>
#include <linux/reservation.h>

#include "mediatek_drm_drv.h"
#include "mediatek_drm_crtc.h"
#include "mediatek_drm_fb.h"
#include "mediatek_drm_gem.h"

#include "mediatek_drm_ddp.h"


void mtk_crtc_finish_page_flip(struct mtk_drm_crtc *mtk_crtc)
{
	struct drm_device *dev = mtk_crtc->base.dev;

#ifndef MEDIATEK_DRM_UPSTREAM
	if (mtk_crtc->fence)
		drm_fence_signal_and_put(&mtk_crtc->fence);
	mtk_crtc->fence = mtk_crtc->pending_fence;
	mtk_crtc->pending_fence = NULL;
#endif /* MEDIATEK_DRM_UPSTREAM */

	drm_send_vblank_event(dev, mtk_crtc->event->pipe, mtk_crtc->event);
	drm_crtc_vblank_put(&mtk_crtc->base);
	mtk_crtc->event = NULL;
}

void mtk_drm_crtc_irq(struct mtk_drm_crtc *mtk_crtc)
{
	struct drm_device *dev = mtk_crtc->base.dev;
	unsigned long flags;

	if (mtk_crtc->pending_ovl_config) {
		mtk_crtc->pending_ovl_config = false;
		ovl_layer_config(&mtk_crtc->base,
			mtk_crtc->pending_ovl_addr,
			mtk_crtc->pending_ovl_format,
			mtk_crtc->pending_enabled);
	}

#ifndef MEDIATEK_DRM_UPSTREAM
	if (mtk_crtc->pending_ovl_cursor_config) {
		mtk_crtc->pending_ovl_cursor_config = false;
		ovl_layer_config_cursor(&mtk_crtc->base,
			mtk_crtc->pending_ovl_cursor_addr,
			mtk_crtc->pending_ovl_cursor_x,
			mtk_crtc->pending_ovl_cursor_y,
			mtk_crtc->pending_ovl_cursor_enabled);
	}
#endif /* MEDIATEK_DRM_UPSTREAM */

	drm_handle_vblank(dev, 0);
	spin_lock_irqsave(&dev->event_lock, flags);
	if (mtk_crtc->pending_needs_vblank) {
		mtk_crtc_finish_page_flip(mtk_crtc);
		mtk_crtc->pending_needs_vblank = false;
	}
	spin_unlock_irqrestore(&dev->event_lock, flags);
}

#ifndef MEDIATEK_DRM_UPSTREAM
static int mtk_drm_crtc_cursor_set(struct drm_crtc *crtc,
	struct drm_file *file_priv,	uint32_t handle,
	uint32_t width,	uint32_t height)
{
	struct drm_device *dev = crtc->dev;
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct drm_gem_object *obj;
	struct mtk_drm_gem_buf *buffer;
	int ret = 0;

	if (!handle) {
		/* turn off cursor */
		obj = NULL;
		buffer = NULL;
		goto finish;
	}

	if ((width != 64) || (height != 64)) {
		DRM_ERROR("bad cursor width or height %dx%d\n", width, height);
		return -EINVAL;
	}

	obj = drm_gem_object_lookup(dev, file_priv, handle);
	if (!obj) {
		DRM_ERROR("not find cursor obj %x crtc %p\n", handle, mtk_crtc);
		return -ENOENT;
	}

	buffer = to_mtk_gem_obj(obj)->buffer;
	if (!buffer) {
		DRM_ERROR("buffer is null\n");
		ret = -EFAULT;
		goto fail;
	}

finish:
	if (mtk_crtc->cursor_obj)
		drm_gem_object_unreference_unlocked(mtk_crtc->cursor_obj);

	mtk_crtc->cursor_w = width;
	mtk_crtc->cursor_h = height;
	mtk_crtc->cursor_obj = obj;

	if (buffer) {
		mtk_crtc->pending_ovl_cursor_addr = buffer->mva_addr;
		mtk_crtc->pending_ovl_cursor_x = mtk_crtc->cursor_x;
		mtk_crtc->pending_ovl_cursor_y = mtk_crtc->cursor_y;
		mtk_crtc->pending_ovl_cursor_enabled = true;
		mtk_crtc->pending_ovl_cursor_config = true;
	} else {
		mtk_crtc->pending_ovl_cursor_addr = 0;
		mtk_crtc->pending_ovl_cursor_x = mtk_crtc->cursor_x;
		mtk_crtc->pending_ovl_cursor_y = mtk_crtc->cursor_y;
		mtk_crtc->pending_ovl_cursor_enabled = false;
		mtk_crtc->pending_ovl_cursor_config = true;
	}

	return 0;
fail:
	drm_gem_object_unreference_unlocked(obj);
	return ret;
}

static int mtk_drm_crtc_cursor_move(struct drm_crtc *crtc, int x, int y)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_drm_gem_buf *buffer;

	if (!mtk_crtc->cursor_obj) {
		/* DRM_ERROR("mtk_drm_crtc_cursor_move no obj [%p]\n", crtc); */
		return 0;
	}

	buffer = to_mtk_gem_obj(mtk_crtc->cursor_obj)->buffer;

	if (x < 0)
		x = 0;

	if (y < 0)
		y = 0;

	mtk_crtc->cursor_x = x;
	mtk_crtc->cursor_y = y;

	mtk_crtc->pending_ovl_cursor_addr = buffer->mva_addr;
	mtk_crtc->pending_ovl_cursor_x = x;
	mtk_crtc->pending_ovl_cursor_y = y;
	mtk_crtc->pending_ovl_cursor_enabled = true;
	mtk_crtc->pending_ovl_cursor_config = true;

	return 0;
}

static void mtk_drm_crtc_update_cb(struct drm_reservation_cb *rcb, void *params)
{
	struct mtk_drm_crtc *mtk_crtc = params;
	struct drm_device *dev = mtk_crtc->base.dev;
	unsigned long flags;

	mtk_crtc->pending_ovl_addr = mtk_crtc->flip_buffer->mva_addr;
	mtk_crtc->pending_ovl_format =
			mtk_crtc->base.primary->fb->pixel_format;
	mtk_crtc->pending_enabled = true;
	mtk_crtc->pending_ovl_config = true;

	spin_lock_irqsave(&dev->event_lock, flags);
	if (mtk_crtc->event) {
		mtk_crtc->pending_needs_vblank = true;
	} else {
		if (mtk_crtc->fence)
			drm_fence_signal_and_put(&mtk_crtc->fence);
		mtk_crtc->fence = mtk_crtc->pending_fence;
		mtk_crtc->pending_fence = NULL;
	}
	spin_unlock_irqrestore(&dev->event_lock, flags);
}

static int mtk_drm_crtc_update_sync(struct mtk_drm_crtc *mtk_crtc,
				   struct reservation_object *resv)
{
	struct fence *fence;
	int ret;
	struct drm_device *dev = mtk_crtc->base.dev;
	unsigned long flags;

	ww_mutex_lock(&resv->lock, NULL);
	spin_lock_irqsave(&dev->event_lock, flags);
	ret = reservation_object_reserve_shared(resv);
	if (ret < 0) {
		DRM_ERROR("Reserving space for shared fence failed: %d.\n",
			ret);
		goto err_mutex;
	}

	fence = drm_sw_fence_new(mtk_crtc->fence_context,
			atomic_add_return(1, &mtk_crtc->fence_seqno));
	if (IS_ERR(fence)) {
		ret = PTR_ERR(fence);
		DRM_ERROR("Failed to create fence: %d.\n", ret);
		goto err_mutex;
	}
	mtk_crtc->pending_fence = fence;
	drm_reservation_cb_init(&mtk_crtc->rcb, mtk_drm_crtc_update_cb,
		mtk_crtc);
	ret = drm_reservation_cb_add(&mtk_crtc->rcb, resv, false);
	if (ret < 0) {
		DRM_ERROR("Adding reservation to callback failed: %d.\n", ret);
		goto err_fence;
	}
	drm_reservation_cb_done(&mtk_crtc->rcb);
	reservation_object_add_shared_fence(resv, mtk_crtc->pending_fence);
	spin_unlock_irqrestore(&dev->event_lock, flags);
	ww_mutex_unlock(&resv->lock);
	return 0;
err_fence:
	fence_put(mtk_crtc->pending_fence);
	mtk_crtc->pending_fence = NULL;
err_mutex:
	spin_unlock_irqrestore(&dev->event_lock, flags);
	ww_mutex_unlock(&resv->lock);

	return ret;
}
#endif /* MEDIATEK_DRM_UPSTREAM */

static int mtk_drm_crtc_page_flip(struct drm_crtc *crtc,
				     struct drm_framebuffer *fb,
				     struct drm_pending_vblank_event *event,
				     uint32_t page_flip_flags)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_drm_fb *mtk_fb = to_mtk_fb(fb);
	struct drm_device *dev = crtc->dev;
	unsigned long flags;
	bool busy;
	int ret;

	spin_lock_irqsave(&dev->event_lock, flags);
	busy = !!mtk_crtc->event;
	if (!busy)
		mtk_crtc->event = event;
	spin_unlock_irqrestore(&dev->event_lock, flags);
	if (busy)
		return -EBUSY;

	if (fb->width != crtc->mode.hdisplay ||
		fb->height != crtc->mode.vdisplay) {
		DRM_ERROR("mtk_drm_crtc_page_flip width/height not match !!\n");
		return -EINVAL;
	}


	if (event) {
		ret = drm_crtc_vblank_get(crtc);
		if (ret) {
			DRM_ERROR("failed to acquire vblank events\n");
			return ret;
		}
	}

	/*
	 * the values related to a buffer of the drm framebuffer
	 * to be applied should be set at here. because these values
	 * first, are set to shadow registers and then to
	 * real registers at vsync front porch period.
	 */
	crtc->primary->fb = fb;
	mtk_crtc->flip_buffer = to_mtk_gem_obj(mtk_fb->gem_obj[0])->buffer;

#ifndef MEDIATEK_DRM_UPSTREAM
	if (mtk_fb->gem_obj[0]->dma_buf && mtk_fb->gem_obj[0]->dma_buf->resv) {
		mtk_drm_crtc_update_sync(mtk_crtc,
			mtk_fb->gem_obj[0]->dma_buf->resv);
	} else
#endif /* MEDIATEK_DRM_UPSTREAM */
	{
		mtk_crtc->pending_ovl_addr = mtk_crtc->flip_buffer->mva_addr;
		mtk_crtc->pending_ovl_format =
				mtk_crtc->base.primary->fb->pixel_format;
		mtk_crtc->pending_enabled = true;
		mtk_crtc->pending_ovl_config = true;

		spin_lock_irqsave(&dev->event_lock, flags);
		if (mtk_crtc->event)
			mtk_crtc->pending_needs_vblank = true;
		spin_unlock_irqrestore(&dev->event_lock, flags);
	}

	return ret;
}

static void mtk_drm_crtc_destroy(struct drm_crtc *crtc)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);

#ifndef MEDIATEK_DRM_UPSTREAM
	struct drm_device *dev = crtc->dev;
	unsigned long flags;

	spin_lock_irqsave(&dev->event_lock, flags);
	if (mtk_crtc->fence)
		drm_fence_signal_and_put(&mtk_crtc->fence);

	if (mtk_crtc->pending_fence)
		drm_fence_signal_and_put(&mtk_crtc->pending_fence);
	spin_unlock_irqrestore(&dev->event_lock, flags);
#endif /* MEDIATEK_DRM_UPSTREAM */
	drm_crtc_cleanup(crtc);
	kfree(mtk_crtc);
}

static void mtk_drm_crtc_prepare(struct drm_crtc *crtc)
{
	/* drm framework doesn't check NULL. */
}

static void mtk_drm_crtc_commit(struct drm_crtc *crtc)
{
	/*
	 * when set_crtc is requested from user or at booting time,
	 * crtc->commit would be called without dpms call so if dpms is
	 * no power on then crtc->dpms should be called
	 * with DRM_MODE_DPMS_ON for the hardware power to be on.
	 */
}

static bool mtk_drm_crtc_mode_fixup(struct drm_crtc *crtc,
			const struct drm_display_mode *mode,
			struct drm_display_mode *adjusted_mode)
{
	/* drm framework doesn't check NULL */
	return true;
}

static int mtk_drm_crtc_mode_set(struct drm_crtc *crtc,
		struct drm_display_mode *mode,
		struct drm_display_mode *adjusted_mode,
		int x, int y, struct drm_framebuffer *old_fb)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct drm_framebuffer *fb;
	struct mtk_drm_fb *mtk_fb;
	struct mtk_drm_gem_buf *buffer;

	fb = crtc->primary->fb;
	mtk_fb = to_mtk_fb(fb);

	buffer = to_mtk_gem_obj(mtk_fb->gem_obj[0])->buffer;

#ifndef MEDIATEK_DRM_UPSTREAM
	DRM_INFO("DBG_YT mtk_drm_crtc_mode_set[%d] %X\n", mtk_crtc->pipe, buffer->mva_addr);
#endif /* MEDIATEK_DRM_UPSTREAM */
	ovl_layer_config(&mtk_crtc->base, buffer->mva_addr, fb->pixel_format, true);
	/*
	 * copy the mode data adjusted by mode_fixup() into crtc->mode
	 * so that hardware can be seet to proper mode.
	 */
	memcpy(&crtc->mode, adjusted_mode, sizeof(*adjusted_mode));

	/* Take a reference to the new fb as we're using it */
	drm_framebuffer_reference(crtc->primary->fb);

	return 0;
}

static void mtk_drm_crtc_disable(struct drm_crtc *crtc)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);

	DRM_INFO("mtk_drm_crtc_disable %d\n", crtc->base.id);

	ovl_layer_config(crtc, 0, 0, false);
	ovl_layer_config_cursor(crtc, 0, mtk_crtc->cursor_x, mtk_crtc->cursor_y, false);
}

static struct drm_crtc_funcs mediatek_crtc_funcs = {
#ifndef MEDIATEK_DRM_UPSTREAM
	.cursor_set		= mtk_drm_crtc_cursor_set,
	.cursor_move	= mtk_drm_crtc_cursor_move,
#endif /* MEDIATEK_DRM_UPSTREAM */
	.set_config		= drm_crtc_helper_set_config,
	.page_flip		= mtk_drm_crtc_page_flip,
	.destroy		= mtk_drm_crtc_destroy,
};

static struct drm_crtc_helper_funcs mediatek_crtc_helper_funcs = {
	.prepare	= mtk_drm_crtc_prepare,
	.commit		= mtk_drm_crtc_commit,
	.mode_fixup	= mtk_drm_crtc_mode_fixup,
	.mode_set	= mtk_drm_crtc_mode_set,
	.disable	= mtk_drm_crtc_disable,
};

int mtk_drm_crtc_create(struct drm_device *dev, unsigned int nr)
{
	struct mtk_drm_private *priv =
		(struct mtk_drm_private *)dev->dev_private;
	struct mtk_drm_crtc *mtk_crtc;

	mtk_crtc = devm_kzalloc(dev->dev, sizeof(*mtk_crtc), GFP_KERNEL);
	if (!mtk_crtc) {
		DRM_ERROR("failed to allocate mtk crtc\n");
		return -ENOMEM;
	}

	priv->crtc[nr] = &mtk_crtc->base;
	mtk_crtc->pipe = nr;

	drm_crtc_init(dev, &mtk_crtc->base, &mediatek_crtc_funcs);
	drm_crtc_helper_add(&mtk_crtc->base, &mediatek_crtc_helper_funcs);

#ifndef MEDIATEK_DRM_UPSTREAM
	mtk_crtc->fence_context = fence_context_alloc(1);
	atomic_set(&mtk_crtc->fence_seqno, 0);
#endif /* MEDIATEK_DRM_UPSTREAM */

	return 0;
}

