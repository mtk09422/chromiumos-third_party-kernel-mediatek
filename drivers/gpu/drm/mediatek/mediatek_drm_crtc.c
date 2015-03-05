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

#include "mediatek_drm_dev_if.h"

void mtk_crtc_finish_page_flip(struct mtk_drm_crtc *mtk_crtc)
{
	struct drm_device *dev = mtk_crtc->base.dev;
	unsigned long flags;

	if (!mtk_crtc->event)
		return;

	spin_lock_irqsave(&dev->event_lock, flags);
	drm_send_vblank_event(dev, mtk_crtc->event->pipe, mtk_crtc->event);
	drm_crtc_vblank_put(&mtk_crtc->base);
	mtk_crtc->event = NULL;
	spin_unlock_irqrestore(&dev->event_lock, flags);
}

#ifdef PVRDRM
static int checkvsync;
#endif
static int irq_count1;
static int irq_count2;
static int irq_count3;
void mtk_drm_crtc_irq(struct mtk_drm_crtc *mtk_crtc)
{
	struct drm_device *dev = mtk_crtc->base.dev;
#ifdef PVRDRM
	unsigned long flags;
#endif

	irq_count1++;
#ifdef PVRDRM
	if (checkvsync)
		DRM_ERROR("mtk_drm_crtc_irq [%p] [%d] [%d]!\n",
			mtk_crtc, mtk_crtc->flip_status, irq_count1);

	spin_lock_irqsave(&dev->event_lock, flags);
	if (mtk_crtc->flip_status == MTK_DRM_CRTC_FLIP_STATUS_DONE) {
		if (mtk_crtc->flip_sync_op) {
			pvr_drm_gem_sync_op_complete(dev, mtk_crtc->flip_sync_op);
			mtk_crtc->flip_sync_op = NULL;
		}
		mtk_crtc->flip_status = MTK_DRM_CRTC_FLIP_STATUS_NONE;
		checkvsync = 0;
	}
	spin_unlock_irqrestore(&dev->event_lock, flags);
#endif
	irq_count2++;
	drm_handle_vblank(dev, 0);
	if (mtk_crtc->event)
		mtk_crtc_finish_page_flip(mtk_crtc);
	irq_count3++;
}

#ifdef PVRDRM
static void mtk_sync_op_complete_cb(struct drm_gem_object *bo,
				    void *data,
				    struct pvr_drm_sync_op *sync_op)
{
	struct drm_crtc *crtc = (struct drm_crtc *)data;

	pvr_drm_gem_sync_op_complete(crtc->dev, sync_op);
}

static void mtk_flip_cb(struct drm_gem_object *bo,
			void *data,
			struct pvr_drm_sync_op *sync_op)
{
	struct drm_crtc *crtc = (struct drm_crtc *)data;
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_drm_gem_buf *mtk_gem_buf = get_mtk_gem_buffer(bo);
	struct drm_framebuffer *fb = crtc->primary->fb;
	unsigned long flags;

	spin_lock_irqsave(&crtc->dev->event_lock, flags);
	WARN_ON(mtk_crtc->flip_status != MTK_DRM_CRTC_FLIP_STATUS_PENDING);
	mtk_crtc->flip_status = MTK_DRM_CRTC_FLIP_STATUS_DONE;

	WARN_ON(mtk_crtc->flip_sync_op);
	mtk_crtc->flip_sync_op = sync_op;

	/*
	 * the values related to a buffer of the drm framebuffer
	 * to be applied should be set at here. because these values
	 * first, are set to shadow registers and then to
	 * real registers at vsync front porch period.
	 */
	if (fb)
		OVLLayerConfig(&mtk_crtc->base, mtk_gem_buf->mva_addr,
			fb->pixel_format);
	else
		DRM_ERROR("mtk_flip_cb not set hw (NO fb)\n");

	spin_unlock_irqrestore(&crtc->dev->event_lock, flags);
}
#endif

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

	buffer = get_mtk_gem_buffer(obj);
	if (!buffer) {
		DRM_ERROR("buffer is null\n");
		ret = -EFAULT;
		goto fail;
	}

finish:
	if (mtk_crtc->cursor_obj)
		drm_gem_object_unreference(mtk_crtc->cursor_obj);

	mtk_crtc->cursor_w = width;
	mtk_crtc->cursor_h = height;
	mtk_crtc->cursor_obj = obj;

	if (buffer) /* need else: to furn off cursor */
		OVLLayerConfigCursor(&mtk_crtc->base, buffer->mva_addr,
			mtk_crtc->cursor_x, mtk_crtc->cursor_y);

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

	buffer = get_mtk_gem_buffer(mtk_crtc->cursor_obj);

	if (x < 0)
		x = 0;

	if (y < 0)
		y = 0;

	mtk_crtc->cursor_x = x;
	mtk_crtc->cursor_y = y;

	OVLLayerConfigCursor(&mtk_crtc->base, buffer->mva_addr, x, y);

	return 0;
}

static int mtk_drm_crtc_page_flip(struct drm_crtc *crtc,
				     struct drm_framebuffer *fb,
				     struct drm_pending_vblank_event *event,
				     uint32_t page_flip_flags)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_drm_fb *mtk_fb = to_mtk_fb(fb);
#ifndef PVRDRM
	struct mtk_drm_gem_buf *buffer = get_mtk_gem_buffer(mtk_fb->gem_obj[0]);
#endif
	struct drm_device *dev = crtc->dev;
	struct drm_framebuffer *old_fb = crtc->primary->fb;
	unsigned long flags;
	int ret;
	static int busy_count;

#ifdef PVRDRM
	if (mtk_crtc->flip_status != MTK_DRM_CRTC_FLIP_STATUS_NONE) {
		busy_count++;
		if (busy_count == 3000) {
			DRM_ERROR("flip_status wrong [%p][%d]!\n",
				mtk_crtc, mtk_crtc->flip_status);
			DRM_ERROR("isr [%d][%d][%d]!\n",
				irq_count1, irq_count2, irq_count3);
			checkvsync = 1;

			if (mtk_get_vblank(mtk_crtc->od_regs) & 0x1) { /* OD */
				mtk_clear_vblank(mtk_crtc->od_regs);
				DRM_ERROR("workaround %d\n",
					mtk_get_vblank(mtk_crtc->od_regs));
			}
			busy_count = 0;
		}
		return -EBUSY;
	}
#endif
	busy_count = 0;

	if (mtk_crtc->event)
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

#ifdef PVRDRM
	spin_lock_irqsave(&dev->event_lock, flags);
	mtk_crtc->flip_status = MTK_DRM_CRTC_FLIP_STATUS_PENDING;
	spin_unlock_irqrestore(&dev->event_lock, flags);

	ret = pvr_drm_gem_sync_op_take(mtk_fb->gem_obj[0],
				       mtk_sync_op_complete_cb,
				       (void *)crtc);
	if (ret) {
		DRM_ERROR("pvr_drm_gem_sync_op_take on new framebuffer failed, please check\n");
		spin_lock_irqsave(&dev->event_lock, flags);
		mtk_crtc->flip_status = MTK_DRM_CRTC_FLIP_STATUS_NONE;
		spin_unlock_irqrestore(&dev->event_lock, flags);
		crtc->primary->fb = old_fb;
	}

	ret = pvr_drm_gem_sync_op_take(to_mtk_fb(old_fb)->gem_obj[0],
				       mtk_flip_cb,
				       (void *)crtc);
	if (ret) {
		DRM_ERROR("pvr_drm_gem_sync_op_take on old framebuffer failed, please check\n");
		spin_lock_irqsave(&dev->event_lock, flags);
		mtk_crtc->flip_status = MTK_DRM_CRTC_FLIP_STATUS_NONE;
		spin_unlock_irqrestore(&dev->event_lock, flags);
		crtc->primary->fb = old_fb;
	}
#else
	OVLLayerConfig(&mtk_crtc->base, buffer->mva_addr, fb->pixel_format);

	if (ret) { /* FIXME: will fail? */
		crtc->primary->fb = old_fb;
		drm_crtc_vblank_put(crtc);
	}
#endif

	spin_lock_irqsave(&dev->event_lock, flags);
	mtk_crtc->event = event;
	spin_unlock_irqrestore(&dev->event_lock, flags);

	return ret;
}

static void mtk_drm_crtc_destroy(struct drm_crtc *crtc)
{
#ifdef PVRDRM
	struct drm_device *dev = crtc->dev;
	unsigned long flags;
#endif
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);

	destroy_workqueue(mtk_crtc->wq);

	/* for (nr = 0; nr < MAX_CRTC; nr++)
		priv->crtc[nr] = NULL; */

	drm_crtc_cleanup(crtc);
#ifdef PVRDRM
	spin_lock_irqsave(&dev->event_lock, flags);
	if (mtk_crtc->flip_sync_op) {
		pvr_drm_gem_sync_op_complete(dev, mtk_crtc->flip_sync_op);
		mtk_crtc->flip_sync_op = NULL;
	}
	mtk_crtc->flip_status = MTK_DRM_CRTC_FLIP_STATUS_NONE;
	spin_unlock_irqrestore(&dev->event_lock, flags);
#endif
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

	if (!mtk_fb) {
		DRM_INFO("mtk_drm_crtc_mode_set: No mtk_fb.\n");
		return 0;
	}

	buffer = get_mtk_gem_buffer(mtk_fb->gem_obj[0]);

	DRM_INFO("DBG_YT mtk_drm_crtc_mode_set %X\n", buffer->mva_addr);
	OVLLayerConfig(&mtk_crtc->base, buffer->mva_addr, fb->pixel_format);
	/*
	 * copy the mode data adjusted by mode_fixup() into crtc->mode
	 * so that hardware can be seet to proper mode.
	 */
	memcpy(&crtc->mode, adjusted_mode, sizeof(*adjusted_mode));

	/* Take a reference to the new fb as we're using it */
	drm_framebuffer_reference(crtc->primary->fb);

	return 0;
}

static const char *mtk_drm_crtc_fence_get_driver_name(struct fence *fence)
{
	return "mtk_drm_fence";
}

static const char *mtk_drm_crtc_fence_get_timeline_name(struct fence *fence)
{
	return "mtk_drm_timeline";
}

static bool mtk_drm_crtc_fence_enable_signaling(struct fence *fence)
{
	set_bit(FENCE_FLAG_ENABLE_SIGNAL_BIT, &fence->flags);

	return 0;
}

static const struct fence_ops mtk_drm_crtc_fence_ops = {
	.get_driver_name = mtk_drm_crtc_fence_get_driver_name,
	.get_timeline_name = mtk_drm_crtc_fence_get_timeline_name,
	.enable_signaling = mtk_drm_crtc_fence_enable_signaling,
	.wait = fence_default_wait,
};

enum {
	MTK_DRM_CRTC_WORK_MODE_SET = 1,
	MTK_DRM_CRTC_WORK_PAGE_FLIP,
};

struct mtk_drm_crtc_work {
	struct work_struct work;
	int work_type;
	struct dma_buf *dmabuf;
	struct fence *write_fence;
	struct fence *read_fence;
	struct drm_crtc *crtc;
	struct drm_display_mode *mode;
	struct drm_display_mode *adjusted_mode;
	int x;
	int y;
	struct drm_framebuffer *old_fb;
	struct drm_framebuffer *new_fb;
	struct drm_pending_vblank_event *event;
	uint32_t page_flip_flags;
};

#ifdef DRM_USING_DMA_FENCE
static void mtk_drm_crtc_work_handler(struct work_struct *work_)
{
	struct mtk_drm_crtc_work *work =
		(struct mtk_drm_crtc_work *) work_;

	if (work->dmabuf) {
		struct fence *write_fence = work->write_fence;

		if (write_fence) {
			signed long ret;

			DRM_INFO("write_fence wait %p\n", write_fence);
			ret = fence_wait_timeout(write_fence, true, 1000);
			if (ret == 0)
				DRM_ERROR("Wait write_fence timeout\n");
			else if (ret < 0)
				DRM_ERROR("Wait write_fence error, ret = %lx\n",
					ret);
		}
	}

	switch (work->work_type) {
	case MTK_DRM_CRTC_WORK_MODE_SET:
		mtk_drm_crtc_mode_set(work->crtc, work->mode,
			work->adjusted_mode, work->x, work->y, work->old_fb);
		break;
	case MTK_DRM_CRTC_WORK_PAGE_FLIP:
		mtk_drm_crtc_page_flip(work->crtc, work->new_fb,
			work->event, work->page_flip_flags);
		break;
	}

	fence_signal(work->read_fence);

	if (work->dmabuf)
		dma_buf_put(work->dmabuf);

	kfree(work);
}

static int mtk_drm_crtc_queue_work(int work_type,
	struct drm_crtc *crtc,
	struct drm_display_mode *mode,
	struct drm_display_mode *adjusted_mode,
	int x, int y, struct drm_framebuffer *old_fb,
	struct drm_framebuffer *new_fb,
	struct drm_pending_vblank_event *event,
	uint32_t page_flip_flags)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_drm_fb *mtk_fb = to_mtk_fb(new_fb);
	struct mtk_drm_crtc_work *work;
	struct dma_buf *dmabuf = mtk_fb->gem_obj[0]->dma_buf;
	struct fence *write_fence = NULL;
	struct fence *read_fence = NULL;
	spinlock_t *read_fence_lock = NULL;
	unsigned read_fence_context;
	static unsigned read_fence_seqno;

	DRM_INFO("mtk_drm_crtc_queue_work type = %d\n", work_type);

	work = (struct mtk_drm_crtc_work *)
		kzalloc(sizeof(struct mtk_drm_crtc_work), GFP_KERNEL);
	if (!work) {
		DRM_ERROR("mtk_drm_crtc_queue_work: ");
		DRM_ERROR("could not allocate render_work_t\n");
		goto no_memory;
	}
	INIT_WORK((struct work_struct *)work,
		mtk_drm_crtc_work_handler);

	if (dmabuf) {
		get_dma_buf(dmabuf);
		write_fence = reservation_object_get_excl(dmabuf->resv);
	}

	read_fence = kzalloc(sizeof(struct fence), GFP_KERNEL);
	if (!read_fence) {
		DRM_ERROR("mtk_drm_crtc_queue_work: ");
		DRM_ERROR("could not allocate struct fence\n");
		goto no_memory;
	}
	read_fence_lock = kzalloc(sizeof(spinlock_t), GFP_KERNEL);
	if (!read_fence_lock) {
		DRM_ERROR("mtk_drm_crtc_queue_work: ");
		DRM_ERROR("could not allocate spinlock_t\n");
		goto no_memory;
	}
	spin_lock_init(read_fence_lock);
	read_fence_context = fence_context_alloc(1);
	fence_init(read_fence, &mtk_drm_crtc_fence_ops,
		read_fence_lock, read_fence_context, ++read_fence_seqno);
	read_fence->ops->enable_signaling(read_fence);

	reservation_object_reserve_shared(dmabuf->resv);
	reservation_object_add_shared_fence(dmabuf->resv, read_fence);

	work->work_type = work_type;
	work->dmabuf = dmabuf;
	work->write_fence = write_fence;
	work->read_fence = read_fence;
	work->crtc = crtc;
	work->mode = mode;
	work->adjusted_mode = adjusted_mode;
	work->x = x;
	work->y = y;
	work->old_fb = old_fb;
	work->new_fb = new_fb;
	work->event = event;
	work->page_flip_flags = page_flip_flags;

	queue_work(mtk_crtc->wq, (struct work_struct *)work);

	if (!write_fence) {
		signed long ret;

		ret = fence_wait_timeout(read_fence, true, 1000);
		if (ret == 0)
			DRM_ERROR("Wait read_fence timeout\n");
		else if (ret < 0)
			DRM_ERROR("Wait read_fence error, ret = %lx\n", ret);
	}

	goto out;

no_memory:
	if (work)
		kfree((void *)work);

	if (read_fence)
		kfree((void *)read_fence);

	if (read_fence_lock)
		kfree((void *)read_fence_lock);

	return -ENOMEM;
out:
	return 0;
}

static int mtk_drm_crtc_page_flip_fence(struct drm_crtc *crtc,
	struct drm_framebuffer *fb,
	struct drm_pending_vblank_event *event,
	uint32_t page_flip_flags)
{
	return mtk_drm_crtc_queue_work(MTK_DRM_CRTC_WORK_PAGE_FLIP,
	    crtc, NULL, NULL, 0, 0, NULL, fb, event, page_flip_flags);
}

static int mtk_drm_crtc_mode_set_fence(struct drm_crtc *crtc,
	struct drm_display_mode *mode,
	struct drm_display_mode *adjusted_mode,
	int x, int y, struct drm_framebuffer *old_fb)
{
	return mtk_drm_crtc_queue_work(MTK_DRM_CRTC_WORK_MODE_SET,
		crtc, mode, adjusted_mode, x, y, old_fb,
		crtc->primary->fb, NULL, 0);
}
#endif

static void mtk_drm_crtc_disable(struct drm_crtc *crtc)
{
}

static struct drm_crtc_funcs mediatek_crtc_funcs = {
	.cursor_set		= mtk_drm_crtc_cursor_set,
	.cursor_move	= mtk_drm_crtc_cursor_move,
	/* .gamma_set	= mtk_drm_gamma_set, // FIXME: optional? */
	.set_config		= drm_crtc_helper_set_config,
	.page_flip		= mtk_drm_crtc_page_flip,
	.destroy		= mtk_drm_crtc_destroy,
};

static struct drm_crtc_helper_funcs mediatek_crtc_helper_funcs = {
	/* .dpms		= mtk_drm_crtc_dpms, */
	.prepare	= mtk_drm_crtc_prepare,
	.commit		= mtk_drm_crtc_commit,
	.mode_fixup	= mtk_drm_crtc_mode_fixup,
	.mode_set	= mtk_drm_crtc_mode_set,
	/* .mode_set_base = mtk_drm_crtc_mode_set_base, // FIXME: optional?
	.load_lut = mtk_drm_crtc_load_lut, */
	.disable	= mtk_drm_crtc_disable,
};

int mtk_drm_crtc_create(struct drm_device *dev, unsigned int nr)
{
	struct mtk_drm_private *priv = get_mtk_drm_private(dev);
	struct mtk_drm_crtc *mtk_crtc;

	mtk_crtc = devm_kzalloc(dev->dev, sizeof(*mtk_crtc), GFP_KERNEL);
	if (!mtk_crtc) {
		DRM_ERROR("failed to allocate mtk crtc\n");
		return -ENOMEM;
	}

	mtk_crtc->wq =
		alloc_workqueue("mtk_drm_crtc_wq",
		WQ_HIGHPRI | WQ_UNBOUND | WQ_MEM_RECLAIM,
		1);
	if (!mtk_crtc->wq) {
		DRM_ERROR("mtk_drm_crtc_create: ");
		DRM_ERROR("failed to create mtk drm crtc work queue\n");
		return -1;
	}

	priv->crtc[nr] = &mtk_crtc->base;

	drm_crtc_init(dev, &mtk_crtc->base, &mediatek_crtc_funcs);
	drm_crtc_helper_add(&mtk_crtc->base, &mediatek_crtc_helper_funcs);

	return 0;
}

