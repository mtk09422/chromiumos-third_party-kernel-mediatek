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

#ifndef _MEDIATEK_DRM_DRV_H_
#define _MEDIATEK_DRM_DRV_H_

#define MAX_CRTC	2
#define MAX_PLANE	4

extern struct platform_driver mtk_dsi_driver;
extern struct i2c_driver it6151mipirx_i2c_driver;
extern struct i2c_driver it6151dptx_i2c_driver;

struct mtk_drm_private {
	struct drm_fb_helper *fb_helper;

	/*
	 * created crtc object would be contained at this array and
	 * this array is used to be aware of which crtc did it request vblank.
	 */
	struct drm_crtc *crtc[MAX_CRTC];
};

/**
 * User-desired buffer creation information structure.
 *
 * @handle: returned a handle to created gem object.
 *	- this handle will be set by gem module of kernel side.
 * @flags: user request for setting memory type or cache attributes.
 * @size: user-desired memory allocation size.
 *	- this size value would be page-aligned internally.
 */
struct drm_mtk_gem_create {
	unsigned int handle;
	unsigned int flags;
	uint64_t size;
};

/**
 * A structure for getting buffer offset.
 *
 * @handle: a pointer to gem object created.
 * @pad: just padding to be 64-bit aligned.
 * @offset: relatived offset value of the memory region allocated.
 *	- this value should be set by user.
 */
struct drm_mtk_gem_map_off {
	unsigned int handle;
	unsigned int pad;
	uint64_t offset;
};

/**
 * A structure for mapping buffer.
 *
 * @handle: a handle to gem object created.
 * @size: memory size to be mapped.
 * @mapped: having user virtual address mmaped.
 *	- this variable would be filled by mtk gem module
 *	of kernel side with user virtual address which is allocated
 *	by do_mmap().
 */
struct drm_mtk_gem_mmap {
	unsigned int handle;
	unsigned int size;
	uint64_t mapped;
};

#define DRM_MTK_GEM_CREATE		0x00
#define DRM_MTK_GEM_MAP_OFFSET	0x01
#define DRM_MTK_GEM_MMAP		0x02
/* Reserved 0x03 ~ 0x05 for mtk specific gem ioctl */
#define DRM_MTK_PLANE_SET_ZPOS	0x06
#define DRM_MTK_VIDI_CONNECTION	0x07

#define DRM_IOCTL_MTK_GEM_CREATE		DRM_IOWR(DRM_COMMAND_BASE + \
		DRM_MTK_GEM_CREATE, struct drm_mtk_gem_create)

#define DRM_IOCTL_MTK_GEM_MAP_OFFSET	DRM_IOWR(DRM_COMMAND_BASE + \
		DRM_MTK_GEM_MAP_OFFSET, struct drm_mtk_gem_map_off)

#define DRM_IOCTL_MTK_GEM_MMAP	DRM_IOWR(DRM_COMMAND_BASE + \
		DRM_MTK_GEM_MMAP, struct drm_mtk_gem_mmap)

#define DRM_IOCTL_MTK_PLANE_SET_ZPOS	DRM_IOWR(DRM_COMMAND_BASE + \
		DRM_MTK_PLANE_SET_ZPOS, struct drm_mtk_plane_set_zpos)

#define DRM_IOCTL_MTK_VIDI_CONNECTION	DRM_IOWR(DRM_COMMAND_BASE + \
		DRM_MTK_VIDI_CONNECTION, struct drm_mtk_vidi_connection)

#define MTK_DRM_IOCTL_DRV(name)\
extern int mtk_drm_ioctl_##name(struct drm_device *, void *, struct drm_file *)

int mtk_dsi_probe(struct drm_device *dev);

MTK_DRM_IOCTL_DRV(gem_create);
MTK_DRM_IOCTL_DRV(gem_map_offset);
MTK_DRM_IOCTL_DRV(gem_mmap);

#endif
