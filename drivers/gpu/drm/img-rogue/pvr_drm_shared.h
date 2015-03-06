/*************************************************************************/ /*!
@File
@Title          PVR DRM definitions shared between kernel and user space.
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@License        Dual MIT/GPLv2

The contents of this file are subject to the MIT license as set out below.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

Alternatively, the contents of this file may be used under the terms of
the GNU General Public License Version 2 ("GPL") in which case the provisions
of GPL are applicable instead of those above.

If you wish to allow use of your version of this file only under the terms of
GPL, and not to allow others to use your version of this file under the terms
of the MIT license, indicate your decision by deleting the provisions above
and replace them with the notice and other provisions required by GPL as set
out in the file called "GPL-COPYING" included in this distribution. If you do
not delete the provisions above, a recipient may use your version of this file
under the terms of either the MIT license or GPL.

This License is also included in this distribution in the file called
"MIT-COPYING".

EXCEPT AS OTHERWISE STATED IN A NEGOTIATED AGREEMENT: (A) THE SOFTWARE IS
PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
PURPOSE AND NONINFRINGEMENT; AND (B) IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/ /**************************************************************************/

#if !defined(__PVR_DRM_SHARED_H__)
#define __PVR_DRM_SHARED_H__

#include <drm/drm.h>

/* Subcommands of DRM_PVR_UNPRIV_CMD */
#define DRM_PVR_UNPRIV_CMD_INIT_SUCCESFUL	0 /* PVR Services init successful */

struct drm_pvr_unpriv_cmd {
	/* Input parameter (preserved by the ioctl) */
	__u32 cmd;

	/* Output parameter */
	__s32 result;
};

#define PVR_GEM_USE_SCANOUT	(1 << 0)
#define PVR_GEM_USE_CURSOR	(1 << 1)

struct drm_pvr_gem_create {
	/* Input parameters (preserved by the ioctl) */
	__u64 size;
	__u32 alloc_flags;
	__u32 usage_flags;

	/* Output parameter */
	__u32 handle;
	__u32 pad;
};

struct drm_pvr_gem_to_img_handle {
	/* Input parameter (preserved by the ioctl) */
	__u32 gem_handle;
	__u32 pad;

	/* Output parameter */
	__u64 img_handle;
};

struct drm_pvr_img_to_gem_handle {
	/* Input parameter (preserved by the ioctl) */
	__u64 img_handle;

	/* Output parameter */
	__u32 gem_handle;
	__u32 pad;
};

struct drm_pvr_gem_sync_get {
	/* Input parameters (preserved by the ioctl) */
	__u32 gem_handle;
	__u32 type;

	/* Output parameters */
	__u64 sync_handle;
	__u32 firmware_addr;
	__u32 pad;
};

/*
 * DRM command numbers, relative to DRM_COMMAND_BASE.
 * These defines must be prefixed with "DRM_".
 */
#define DRM_PVR_SRVKM_CMD		0 /* Used for PVR Services ioctls */
#define DRM_PVR_DBGDRV_CMD		1 /* Debug driver (PDUMP) ioctls */
#define DRM_PVR_UNPRIV_CMD		2 /* PVR driver unprivileged ioctls */
#define DRM_PVR_GEM_CREATE		3
#define DRM_PVR_GEM_TO_IMG_HANDLE	4
#define DRM_PVR_IMG_TO_GEM_HANDLE	5
#define DRM_PVR_GEM_SYNC_GET		6

/* These defines must be prefixed with "DRM_IOCTL_". */
#define DRM_IOCTL_PVR_UNPRIV_CMD		DRM_IOWR(DRM_COMMAND_BASE + DRM_PVR_UNPRIV_CMD, struct drm_pvr_unpriv_cmd)
#define DRM_IOCTL_PVR_GEM_CREATE		DRM_IOWR(DRM_COMMAND_BASE + DRM_PVR_GEM_CREATE, struct drm_pvr_gem_create)
#define DRM_IOCTL_PVR_GEM_TO_IMG_HANDLE	DRM_IOWR(DRM_COMMAND_BASE + DRM_PVR_GEM_TO_IMG_HANDLE, struct drm_pvr_gem_to_img_handle)
#define DRM_IOCTL_PVR_IMG_TO_GEM_HANDLE	DRM_IOWR(DRM_COMMAND_BASE + DRM_PVR_IMG_TO_GEM_HANDLE, struct drm_pvr_img_to_gem_handle)
#define DRM_IOCTL_PVR_GEM_SYNC_GET		DRM_IOWR(DRM_COMMAND_BASE + DRM_PVR_GEM_SYNC_GET, struct drm_pvr_gem_sync_get)

#endif /* defined(__PVR_DRM_SHARED_H__) */
