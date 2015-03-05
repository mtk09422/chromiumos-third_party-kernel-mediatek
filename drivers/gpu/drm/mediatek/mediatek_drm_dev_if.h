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

#ifndef _MEDIATEK_DRM_DEV_IF_H_
#define _MEDIATEK_DRM_DEV_IF_H_

#ifdef PVRDRM

#include <pvr_drm_display.h>

struct mtk_display_device {
	struct mtk_drm_private	dev_private;
	struct device		*mediatek_dev;
	struct drm_device	*dev;
};

struct pvr_drm_display_buffer {
	struct mtk_drm_gem_buf	base;
	struct kref		refcount;
	struct drm_device	*dev;
};

static inline struct device *get_mtk_drm_device(struct drm_device *dev)
{
	struct mtk_display_device *display_device;

	display_device = (struct mtk_display_device *)
		pvr_drm_get_display_device(dev);

	if (display_device)
		return display_device->mediatek_dev;

	return NULL;
}


static inline struct mtk_drm_private *get_mtk_drm_private(
	struct drm_device *dev)
{
	return (struct mtk_drm_private *)pvr_drm_get_display_device(dev);
}

static inline struct mtk_drm_gem_buf *get_mtk_gem_buffer(
	struct drm_gem_object *bo)
{
	struct pvr_drm_display_buffer *pBuffer = pvr_drm_gem_buffer(bo);

	if (pBuffer)
		return &pBuffer->base;

	return NULL;
}
#else
#define get_mtk_drm_private(dev)	((struct mtk_drm_private *) \
	dev->dev_private)
#define get_mtk_gem_buffer(gemobj)	(to_mtk_gem_obj(gemobj)->buffer)
#define get_mtk_drm_device(drm)		(((struct drm_device *)drm)->dev)
#endif

#endif

