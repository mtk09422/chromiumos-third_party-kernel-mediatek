
#ifndef _DRM_BRIDGE_IT6151_H_
#define _DRM_BRIDGE_IT6151_H_

struct drm_device;
struct drm_encoder;
struct i2c_client;
struct device_node;

#if 1/*defined(CONFIG_DRM_IT6151) || defined(CONFIG_DRM_IT6151_MODULE)*/

void it6151_pre_enable(struct drm_bridge *bridge);

int it6151_init(struct drm_device *dev, struct drm_encoder *encoder,
		struct i2c_client *mipirx_client,
		struct i2c_client *dptx_client, struct device_node *node);
#else

static inline int it6151_init(struct drm_device *dev,
	struct drm_encoder *encoder, struct i2c_client *mipirx_client,
	struct i2c_client *dptx_client, struct device_node *node)
{
	return 0;
}

#endif

#endif

