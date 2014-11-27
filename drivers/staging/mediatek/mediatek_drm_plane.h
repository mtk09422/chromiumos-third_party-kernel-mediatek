struct mtk_plane {
	struct drm_plane		base;
	bool					enabled;
	/* struct mtk_drm_overlay	overlay; */
};

int mtk_plane_init(struct drm_device *dev, unsigned int nr);

