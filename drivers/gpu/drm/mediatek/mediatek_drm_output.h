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

#ifndef _MEDIATEK_DRM_OUTPUT_H_
#define _MEDIATEK_DRM_OUTPUT_H_

#include <linux/clk.h>

enum mtk_output_type {
	MTK_OUTPUT_RGB,
	MTK_OUTPUT_HDMI,
	MTK_OUTPUT_DSI,
	MTK_OUTPUT_EDP,
};

struct mtk_drm_connector {
	struct drm_connector	base;
	uint32_t		enc_id;
};

#define to_mtk_connector(x)	container_of(x, struct mtk_drm_connector, base)

struct mtk_output {
	struct device_node *of_node;
	struct device *dev;
/*
	const struct tegra_output_ops *ops;

	struct i2c_adapter *ddc;
	const struct edid *edid;
	unsigned int hpd_irq;
	int hpd_gpio;	*/
	const struct mtk_output_ops *ops;

	unsigned int type;

	struct drm_panel *panel;

	struct drm_encoder *encoder;
	struct drm_connector *connector;
};

struct mtk_output_ops {
	int (*enable)(struct mtk_output *output);
	int (*disable)(struct mtk_output *output);
	int (*setup_clock)(struct mtk_output *output, struct clk *clk,
			   unsigned long pclk, unsigned int *div);
	int (*check_mode)(struct mtk_output *output,
			  struct drm_display_mode *mode,
			  enum drm_mode_status *status);
	enum drm_connector_status (*detect)(struct mtk_output *output);
};

extern struct mtk_output_ops dsi_ops;

static inline struct mtk_output *encoder_to_output(struct drm_encoder **e)
{
	return (struct mtk_output *)container_of(e, struct mtk_output, encoder);
}

static inline struct mtk_output *connector_to_output(
	struct drm_connector **c)
{
	return (struct mtk_output *)container_of(c, struct mtk_output,
		connector);
}

int mtk_output_init(struct drm_device *dev);

static inline int mtk_output_enable(struct mtk_output *output)
{
	if (output && output->ops && output->ops->enable)
		return output->ops->enable(output);

	return output ? -ENOSYS : -EINVAL;
}

static inline int mtk_output_disable(struct mtk_output *output)
{
	if (output && output->ops && output->ops->disable)
		return output->ops->disable(output);

	return output ? -ENOSYS : -EINVAL;
}

int mtk_output_probe(struct mtk_output *output);

#endif

