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

#define MAX_FB_OBJ	4
#define FBDEV_BPP	16

/*
 * mtk specific framebuffer structure.
 *
 * @fb: drm framebuffer object.
 * @mtk_gem_obj: array of mtk specific gem object containing a gem object.
 */
struct mtk_drm_fb {
	struct drm_framebuffer	base;
	struct drm_gem_object	*gem_obj[MAX_FB_OBJ]; /* FIXME? mtk_gem_obj? */
};

#define to_mtk_fb(x) container_of(x, struct mtk_drm_fb, base)

void mtk_drm_mode_output_poll_changed(struct drm_device *dev);
struct drm_framebuffer *mtk_drm_mode_fb_create(struct drm_device *dev,
					       struct drm_file *file,
					       struct drm_mode_fb_cmd2 *cmd);

int mtk_fbdev_create(struct drm_device *dev);
void mtk_fbdev_destroy(struct drm_device *dev);

