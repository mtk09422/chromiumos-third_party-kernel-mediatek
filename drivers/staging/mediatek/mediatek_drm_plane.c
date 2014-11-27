#include <drm/drm_crtc.h>

#include "mediatek_drm_drv.h"
#include "mediatek_drm_plane.h"

static const uint32_t formats[] = {
	DRM_FORMAT_XRGB8888,
	DRM_FORMAT_ARGB8888,
	DRM_FORMAT_NV12,
	DRM_FORMAT_NV12MT,
};

static struct drm_plane_funcs mtk_plane_funcs = {
/*	.update_plane	= mtk_update_plane,
	.disable_plane	= mtk_disable_plane,
	.destroy	= mtk_plane_destroy, */
};

int mtk_plane_init(struct drm_device *dev, unsigned int nr)
{
/*	struct drm_device	*drmdev  = get_drm_device(dev); */
	struct mtk_plane    *mtk_plane;
	uint32_t possible_crtcs;

	mtk_plane = kzalloc(sizeof(*mtk_plane), GFP_KERNEL);
	if (!mtk_plane)
		return -ENOMEM;

	/* all CRTCs are available */
	possible_crtcs = (1 << MAX_CRTC) - 1;

	return drm_plane_init(dev, &mtk_plane->base, possible_crtcs,
			&mtk_plane_funcs, formats, ARRAY_SIZE(formats), 0);
}

