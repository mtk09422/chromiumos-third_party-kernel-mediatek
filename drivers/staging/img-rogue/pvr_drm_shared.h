/*************************************************************************/ /*!
@File
@Title          Services DRM definitions shared between kernel and user space.
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@License        Strictly Confidential.
*/ /**************************************************************************/

#if !defined(__PVR_DRM_SHARED_H__)
#define __PVR_DRM_SHARED_H__

#if defined(SUPPORT_DRM)
#include <linux/types.h>

/* 
 * DRM command numbers, relative to DRM_COMMAND_BASE. 
 * These defines must be prefixed with "DRM_".
 */
#define DRM_PVR_SRVKM_CMD			0 /* Used for PVR Services ioctls */
#define DRM_PVR_DBGDRV_CMD			1 /* Debug driver (PDUMP) ioctls */
#define DRM_PVR_UNPRIV_CMD			2 /* PVR driver unprivileged ioctls */
#define DRM_PVR_GEM_CREATE			3
#define DRM_PVR_GEM_TO_IMG_HANDLE		4
#define DRM_PVR_IMG_TO_GEM_HANDLE		5
#define DRM_PVR_GEM_SYNC_GET			6


#if !defined(SUPPORT_KERNEL_SRVINIT)
/* Subcommands of DRM_PVR_UNPRIV_CMD */
#define	DRM_PVR_UNPRIV_CMD_INIT_SUCCESFUL	0 /* PVR Services init succesful */

typedef struct drm_pvr_unpriv_cmd_tag
{
	uint32_t	cmd;
	int32_t		result;
} drm_pvr_unpriv_cmd;
#endif	/* #if !defined(SUPPORT_KERNEL_SRVINIT) */

#define PVR_GEM_USE_SCANOUT	(1 << 0)
#define PVR_GEM_USE_CURSOR	(2 << 0)

typedef	struct drm_pvr_gem_create_tag
{
	/* Input parameters (preserved by the ioctl) */
	uint64_t	size;
	uint32_t	alloc_flags;
	uint32_t	usage_flags;

	/* Output parameters */
	uint32_t	handle;
	uint32_t	pad;
} drm_pvr_gem_create;

typedef	struct drm_pvr_gem_to_img_handle_tag
{	
	/* Input parameters (preserved by the ioctl) */
	uint32_t	gem_handle;
	uint32_t	pad;

	/* Output parameters */
	uint64_t	img_handle;
} drm_pvr_gem_to_img_handle;

typedef	struct drm_pvr_img_to_gem_handle_tag
{	
	/* Input parameters (preserved by the ioctl) */
	uint64_t	img_handle;

	/* Output parameters */
	uint32_t	gem_handle;
	uint32_t	pad;
} drm_pvr_img_to_gem_handle;

typedef struct drm_pvr_gem_sync_get_tag
{
	/* Input parameters (preserved by the ioctl) */
	uint32_t	gem_handle;
	uint32_t	type;

	/* Output parameters */
	uint64_t	sync_handle;
	uint32_t	firmware_addr;
	uint32_t	pad;
} drm_pvr_gem_sync_get;

#endif /* defined(SUPPORT_DRM) */
#endif /* defined(__PVR_DRM_SHARED_H__) */
