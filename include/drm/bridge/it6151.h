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

#ifndef _DRM_BRIDGE_IT6151_H_
#define _DRM_BRIDGE_IT6151_H_

struct drm_device;
struct drm_encoder;
struct i2c_client;
struct device_node;

#if defined(CONFIG_DRM_IT6151)
int it6151_init(struct drm_device *dev, struct drm_encoder *encoder,
	struct i2c_client *client, struct device_node *node);
#else
inline int it6151_init(struct drm_device *dev, struct drm_encoder *encoder,
	struct i2c_client *client, struct device_node *node)
{
	return 0;
}
#endif

#endif
