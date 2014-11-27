#include <drm/drmP.h>
#include <drm/drm_crtc_helper.h>

#include "mediatek_drm_drv.h"
#include "mediatek_drm_crtc.h"
#include "mediatek_drm_fb.h"
#include "mediatek_drm_gem.h"
#include "mediatek_drm_hw.h"

#include "mediatek_drm_dev_if.h"

/* #define VSYNC_TIMER */

#ifdef VSYNC_TIMER
static DEFINE_SPINLOCK(timer_lock);
struct mtk_drm_crtc *timer_mtk_crtc = NULL;

static struct hrtimer psudo_vsync_timer;
static int vsync_interval = 16*1000;
static int vsyncount;

static enum hrtimer_restart psudo_vsync(struct hrtimer *timer)
{
	unsigned long flags;

	/* if((vsyncount & 0xff) == 0) */
		pr_info("karen add A: psudo_vsync vsyncount = %d\n", vsyncount);
	vsyncount++;

	mtk_clear_vblank(timer_mtk_crtc->regs);
	mtk_drm_crtc_irq(timer_mtk_crtc);

	spin_lock_irqsave(&timer_lock, flags);
	timer_mtk_crtc = NULL;
	spin_unlock_irqrestore(&timer_lock, flags);

	return HRTIMER_NORESTART;
}

static int psudo_vsync_timer_init(void)
{
	ktime_t ktime = ktime_set(0, vsync_interval * 1000);

	hrtimer_init(&psudo_vsync_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	psudo_vsync_timer.function = &psudo_vsync;
	hrtimer_start(&psudo_vsync_timer, ktime, HRTIMER_MODE_REL);

	return 0;
}

#if 0
static void psudo_vsync_timer_exit(void)
{
	int ret;

	ret = hrtimer_cancel(&psudo_vsync_timer);
	if (ret)
		pr_info("The timer was still in use...\n");
}
#endif
#endif

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

void mtk_drm_crtc_irq(struct mtk_drm_crtc *mtk_crtc)
{
	struct drm_device *dev = mtk_crtc->base.dev;
/*	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	void __iomem *base = dcrtc->base; */

#ifdef PVRDRM
	if (mtk_crtc->flip_status == MTK_DRM_CRTC_FLIP_STATUS_DONE) {
		if (mtk_crtc->flip_data) {
			pvr_drm_flip_done(dev, mtk_crtc->flip_data);
			mtk_crtc->flip_data = NULL;
		}
	}
#endif

	if (mtk_crtc->event) {
		drm_handle_vblank(dev, mtk_crtc->event->pipe);
		mtk_crtc_finish_page_flip(mtk_crtc);
	}
}

#ifdef PVRDRM
static void mtk_flip_cb(struct drm_gem_object *bo,
			void *data,
			struct pvr_drm_flip_data *flip_data)
{
	struct drm_crtc *crtc = (struct drm_crtc *)data;
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_drm_gem_buf *mtk_gem_buf = get_mtk_gem_buffer(bo);
	struct drm_framebuffer *fb = crtc->primary->fb;
	unsigned long flags;

	spin_lock_irqsave(&crtc->dev->event_lock, flags);
	WARN_ON(mtk_crtc->flip_status != MTK_DRM_CRTC_FLIP_STATUS_PENDING);
	mtk_crtc->flip_status = MTK_DRM_CRTC_FLIP_STATUS_DONE;

	WARN_ON(mtk_crtc->flip_data);
	mtk_crtc->flip_data = flip_data;
	spin_unlock_irqrestore(&crtc->dev->event_lock, flags);

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

#ifdef VSYNC_TIMER
	if (timer_mtk_crtc == NULL) {
		spin_lock_irqsave(&timer_lock, flags);
		timer_mtk_crtc = mtk_crtc;
		spin_unlock_irqrestore(&timer_lock, flags);
		psudo_vsync_timer_init();
	} else {
		DRM_ERROR("timer_mtk_crtc not null (%p)\n", mtk_crtc);
	}
#endif
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
		DRM_ERROR("mtk_drm_crtc_cursor_move no cursor obj[%p]\n", crtc);
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

	if (mtk_crtc->event)
		return -EBUSY;

	if (fb->width != crtc->mode.hdisplay ||
		fb->height != crtc->mode.vdisplay) {
		DRM_ERROR("mtk_drm_crtc_page_flip width/height not match !!\n");
		return -EINVAL;
	}

	spin_lock_irqsave(&dev->event_lock, flags);
	mtk_crtc->event = event;
	spin_unlock_irqrestore(&dev->event_lock, flags);

	if (event) {
		ret = drm_crtc_vblank_get(crtc);
		if (ret) {
			DRM_DEBUG("failed to acquire vblank events\n");
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
	mtk_crtc->flip_status = MTK_DRM_CRTC_FLIP_STATUS_PENDING;
	ret = pvr_drm_flip_schedule(mtk_fb->gem_obj[0],
				    mtk_flip_cb,
				    (void *)crtc);
	if (ret) {
		DRM_ERROR("pvr_drm_flip_schedule fail, please check\n");
		crtc->primary->fb = old_fb;
	}
#else
	OVLLayerConfig(&mtk_crtc->base, buffer->mva_addr, fb->pixel_format);

	if (ret) { /* FIXME: will fail? */
		crtc->primary->fb = old_fb;
		drm_crtc_vblank_put(crtc);
	}
#ifdef VSYNC_TIMER
	if (timer_mtk_crtc == NULL) {
		spin_lock_irqsave(&timer_lock, flags);
		timer_mtk_crtc = mtk_crtc;
		spin_unlock_irqrestore(&timer_lock, flags);
		psudo_vsync_timer_init();
	} else {
		DRM_ERROR("timer_mtk_crtc not null (%p)\n", mtk_crtc);
	}
#endif
#endif

	return ret;
}

static void mtk_drm_crtc_destroy(struct drm_crtc *crtc)
{
	struct mtk_drm_private *priv = get_mtk_drm_private(crtc->dev);
	unsigned int nr;

	for (nr = 0; nr < MAX_CRTC; nr++)
		priv->crtc[nr] = NULL;

	drm_crtc_cleanup(crtc);
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
#if 0
	if (mtk_crtc->dpms != DRM_MODE_DPMS_ON) {
		int mode = DRM_MODE_DPMS_ON;

		/*
		 * enable hardware(power on) to all encoders hdmi connected
		 * to current crtc.
		 */
		mtk_drm_crtc_dpms(crtc, mode);
		/*
		 * enable dma to all encoders connected to current crtc and
		 * lcd panel.
		 */
		mtk_drm_fn_encoder(crtc, &mode,
					mtk_drm_encoder_dpms_from_crtc);
	}

	mtk_drm_fn_encoder(crtc, &mtk_crtc->pipe,
			mtk_drm_encoder_crtc_commit);
#endif
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
	buffer = get_mtk_gem_buffer(mtk_fb->gem_obj[0]);

	pr_info("DBG_YT mtk_drm_crtc_mode_set %X\n", buffer->mva_addr);
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

static void mtk_drm_crtc_disable(struct drm_crtc *crtc)
{
	/* disable crtc when not in use - more explicit than dpms off */
	/* struct drm_device *drm = crtc->dev;
	struct drm_plane *plane;

	drm_for_each_legacy_plane(plane, &drm->mode_config.plane_list) {
		if (plane->crtc == crtc) {
			plane->crtc = NULL;

			if (plane->fb) {
				drm_framebuffer_unreference(plane->fb);
				plane->fb = NULL;
			}
		}
	}

	drm_vblank_off(drm, dc->pipe); */
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

	/* mtk_crtc->dpms = DRM_MODE_DPMS_OFF;
	mtk_crtc->overlay.zpos = DEFAULT_ZPOS; */
	priv->crtc[nr] = &mtk_crtc->base;

	drm_crtc_init(dev, &mtk_crtc->base, &mediatek_crtc_funcs);
	drm_crtc_helper_add(&mtk_crtc->base, &mediatek_crtc_helper_funcs);

	return 0;
}

