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

#ifndef _MEDIATEK_DRM_DEBUGFS_H_
#define _MEDIATEK_DRM_DEBUGFS_H_

void mediatek_drm_debugfs_init(struct drm_crtc *crtc);
void mediatek_drm_debugfs_deinit(void);

#endif
