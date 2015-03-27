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

#include <drm/drmP.h>
#include <drm/drm_panel.h>
#include <drm/drm_crtc_helper.h>
#include <linux/of_gpio.h>

#include "mediatek_drm_output.h"
#include "it6151.h"

#ifndef CONFIG_DRM_MEDIATEK_IT6151
static struct drm_encoder *mtk_drm_connector_best_encoder(
						struct drm_connector *connector)
{
	struct mtk_drm_connector *mtk_conn = to_mtk_connector(connector);
	struct drm_device *dev = connector->dev;
	struct drm_encoder *encoder;
	struct drm_mode_object *obj;

	obj = drm_mode_object_find(dev, mtk_conn->enc_id,
						DRM_MODE_OBJECT_ENCODER);
	if (!obj) {
		DRM_DEBUG_KMS("Unknown ENCODER ID %d\n", mtk_conn->enc_id);
		return NULL;
	}

	encoder = obj_to_encoder(obj);

	return encoder;
}

static const struct drm_display_mode drm_default_modes[] = {
	/* 800x1280@60Hz */
	{ DRM_MODE("800x1280", DRM_MODE_TYPE_DRIVER, 70000,
	800, 810, 811, 821, 0,
	1280, 1312, 1333, 1390, 0, 0) },
};

static int mtk_drm_connector_get_modes(struct drm_connector *connector)
{
	const struct drm_display_mode *ptr = &drm_default_modes[0];
	struct drm_display_mode *mode;
	int count = 0;
	/* struct mtk_drm_manager *manager = mtk_conn->manager;
	void *edid = NULL; */

	mode = drm_mode_duplicate(connector->dev, ptr);
	if (mode) {
		drm_mode_probed_add(connector, mode);
		count++;
	}

	connector->display_info.width_mm = mode->hdisplay;
	connector->display_info.height_mm = mode->vdisplay;

	/*
	 * If the panel provides one or more modes, use them exclusively and
	 * ignore any other means of obtaining a mode.
	 */
	/* if (output->panel) {
		err = output->panel->funcs->get_modes(output->panel);
		if (err > 0)
		return err;
	} */

	/* output->panel = of_drm_find_panel(device->dev.of_node);
	if (output->panel) {
		if (output->connector.dev)
			drm_helper_hpd_irq_event(output->connector.dev);
	} */

	/*
	 * if get_edid() exists then get_edid() callback of hdmi side
	 * is called to get edid data through i2c interface else
	 * get timing from the FIMD driver(display controller).
	 *
	 * P.S. in case of lcd panel, count is always 1 if success
	 * because lcd panel has only one mode.
	 */
	/* if (manager->get_edid) {
	    int ret;
		edid = kzalloc(MAX_EDID, GFP_KERNEL);
		if (!edid) {
			DRM_ERROR("failed to allocate edid\n");
			goto out;
		}

		ret = manager->get_edid(manager->dev, connector,
						edid, MAX_EDID);
		if (ret < 0) {
			DRM_ERROR("failed to get edid data.\n");
			goto out;
		}

		count = drm_add_edid_modes(connector, edid);
		if (!count) {
			DRM_ERROR("Add edid modes failed %d\n", count);
			goto out;
		}

		drm_mode_connector_update_edid_property(connector, edid);
	} else {
		struct drm_display_mode *mode = drm_mode_create(connector->dev);
		struct mtk_drm_panel_info *panel;

		if (manager->get_panel)
			panel = manager->get_panel(manager->dev);
		else {
			drm_mode_destroy(connector->dev, mode);
			return 0;
		}

		convert_to_display_mode(mode, panel);
		connector->display_info.width_mm = mode->width_mm;
		connector->display_info.height_mm = mode->height_mm;

		mode->type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;
		drm_mode_set_name(mode);
		drm_mode_probed_add(connector, mode);

		count = 1;
	}

out:
	kfree(edid); */
	return count;
}

static enum drm_connector_status mtk_drm_connector_detect(
	struct drm_connector *connector, bool force)
{
	enum drm_connector_status status = connector_status_unknown;

	status = connector_status_connected; /* FIXME? */

	return status;
}

static void mtk_drm_connector_destroy(struct drm_connector *connector)
{
	drm_connector_unregister(connector);
	drm_connector_cleanup(connector);
}

static const struct drm_connector_funcs mtk_drm_conn_funcs = {
	.dpms	= drm_helper_connector_dpms,
	.detect	= mtk_drm_connector_detect,
	.fill_modes	= drm_helper_probe_single_connector_modes,
	/* .set_property	= mtk_drm_connector_set_property, */
	.destroy	= mtk_drm_connector_destroy,
};

static const struct drm_connector_helper_funcs mtk_drm_conn_helper_funcs = {
	.get_modes	= mtk_drm_connector_get_modes,
	/* .mode_valid	= , // FIXME: optional? */
	.best_encoder	= mtk_drm_connector_best_encoder,
};
#endif

static void mtk_drm_encoder_destroy(struct drm_encoder *encoder)
{
	drm_encoder_cleanup(encoder);
}

static void mtk_drm_encoder_dpms(struct drm_encoder *encoder, int mode)
{
/*	struct mtk_output *output = encoder_to_output(&encoder);
	struct drm_panel *panel = output->panel;

	DRM_INFO("DBG_YT mtk_drm_encoder_dpms mode = %d\n", mode);
	if (mode != DRM_MODE_DPMS_ON) {
		drm_panel_disable(panel);
		mtk_output_disable(output);
	} else {
		mtk_output_enable(output);
		drm_panel_enable(panel);
	}
*/
}

static bool mtk_drm_encoder_mode_fixup(struct drm_encoder *encoder,
			       const struct drm_display_mode *mode,
			       struct drm_display_mode *adjusted_mode)
{
	return true;
}

static void mtk_drm_encoder_prepare(struct drm_encoder *encoder)
{
	/* DRM_MODE_DPMS_OFF? */

	/* drm framework doesn't check NULL. */
}

static void mtk_drm_encoder_mode_set(struct drm_encoder *encoder,
	struct drm_display_mode *mode, struct drm_display_mode *adjusted)
{
}

static void mtk_drm_encoder_commit(struct drm_encoder *encoder)
{
	/* DRM_MODE_DPMS_ON? */
}

static const struct drm_encoder_funcs mtk_drm_encoder_funcs = {
	.destroy	= mtk_drm_encoder_destroy,
};

static const struct drm_encoder_helper_funcs mtk_drm_encoder_helpers_funcs = {
	.dpms = mtk_drm_encoder_dpms,
	.mode_fixup = mtk_drm_encoder_mode_fixup,
	.prepare = mtk_drm_encoder_prepare,
	.mode_set = mtk_drm_encoder_mode_set,
	.commit = mtk_drm_encoder_commit,
};

#ifdef CONFIG_DRM_MEDIATEK_IT6151
struct bridge_init {
	struct i2c_client *mipirx_client;
	struct i2c_client *dptx_client;
	struct device_node *node_mipirx;
	struct device_node *node_dptx;
};

static bool find_bridge(const char *mipirx, const char *dptx,
	struct bridge_init *bridge)
{
	pr_info("DBG_jitao find_bridge\n");

	/* bridge->mipirx_client = NULL; */
	bridge->node_mipirx = of_find_compatible_node(NULL, NULL, mipirx);

	pr_info("DBG_jitao find_bridge 1\n");

	if (!bridge->node_mipirx)
		return false;

	pr_info("DBG_jitao find_bridge 2 of-%s\n",
		of_node_full_name(bridge->node_mipirx));

	/* bridge->mipirx_client = it6151_mipirx;
	//of_find_i2c_device_by_node(bridge->node_mipirx);
	if (!bridge->mipirx_client)
		return false; */

	pr_info("DBG_jitao find_bridge 3\n");

	/* bridge->dptx_client = NULL; */
	bridge->node_dptx = of_find_compatible_node(NULL, NULL, dptx);
	if (!bridge->node_dptx)
		return false;

	pr_info("DBG_jitao find_bridge 4\n");

	/* bridge->dptx_client = it6151_dptx;
	//of_find_i2c_device_by_node(bridge->node_dptx);
	if (!bridge->dptx_client)
		return false; */

	pr_info("DBG_jitao find_bridge 5\n");

	return true;
}

static int mtk_drm_attach_lcm_bridge(struct drm_device *dev,
	struct drm_encoder *encoder)
{
	struct bridge_init *bridge;
	int ret;

	pr_info("DBG_jitao mtk_drm_attach_lcm_bridge\n");

	bridge = kzalloc(sizeof(*bridge), GFP_KERNEL);

	if (find_bridge("ite,it6151mipirx", "ite,it6151dptx", bridge)) {
		ret = it6151_init(dev, encoder, bridge->mipirx_client,
			bridge->dptx_client, bridge->node_mipirx);
		if (ret != 0)
			return 1;
	}

	return 0;
}
#endif

int mtk_output_init(struct drm_device *dev)
{
	int conn_type, enc_type;
	struct mtk_output *out;
	struct drm_encoder *enc;
	struct mtk_drm_connector *mtk_conn;
	struct drm_connector *conn;
	int ret;

	out = devm_kzalloc(dev->dev, sizeof(*out), GFP_KERNEL);
	if (!out)
		return -ENOMEM;

	enc = devm_kzalloc(dev->dev, sizeof(*enc), GFP_KERNEL);
	if (!enc)
		return -ENOMEM;
	out->encoder = enc;

	mtk_conn = devm_kzalloc(dev->dev, sizeof(*mtk_conn), GFP_KERNEL);
	if (!mtk_conn)
		return -ENOMEM;
	conn = &mtk_conn->base;
	out->connector = conn;

	DRM_INFO("DBG_YT mtk_output_init1\n");
	/* only for bringup */
	out->type = MTK_OUTPUT_DSI;
	out->ops = &dsi_ops;

	switch (out->type) {
	case MTK_OUTPUT_RGB:
		conn_type = DRM_MODE_CONNECTOR_LVDS;
		enc_type = DRM_MODE_ENCODER_LVDS;
		break;

	case MTK_OUTPUT_HDMI:
		conn_type = DRM_MODE_CONNECTOR_HDMIA;
		enc_type = DRM_MODE_ENCODER_TMDS;
		break;

	case MTK_OUTPUT_DSI:
		conn_type = DRM_MODE_CONNECTOR_DSI;
		enc_type = DRM_MODE_ENCODER_DSI;
		break;

	case MTK_OUTPUT_EDP:
		conn_type = DRM_MODE_CONNECTOR_eDP;
		enc_type = DRM_MODE_ENCODER_TMDS;
		break;
	default:
		conn_type = DRM_MODE_CONNECTOR_Unknown;
		enc_type = DRM_MODE_ENCODER_NONE;
		break;
	}

	DRM_INFO("DBG_YT mtk_output_init2\n");

	drm_encoder_init(dev, out->encoder, &mtk_drm_encoder_funcs, enc_type);
	drm_encoder_helper_add(out->encoder, &mtk_drm_encoder_helpers_funcs);

#ifndef CONFIG_DRM_MEDIATEK_IT6151
	drm_connector_init(dev, conn, &mtk_drm_conn_funcs, conn_type);
	drm_connector_helper_add(conn, &mtk_drm_conn_helper_funcs);
	mtk_conn->enc_id = enc->base.id;
#else
	ret = mtk_drm_attach_lcm_bridge(dev, out->encoder);
	if (ret)
		return ret;
#endif

	DRM_INFO("DBG_YT mtk_output_init3\n");

	/* output->connector.dpms = DRM_MODE_DPMS_OFF; */

	if (out->panel)
		drm_panel_attach(out->panel, out->connector);

#ifndef CONFIG_DRM_MEDIATEK_IT6151
	drm_mode_connector_attach_encoder(conn, out->encoder);
	drm_connector_register(conn);
#endif

	out->encoder->possible_crtcs = 0x1;

	return ret;
}

int mtk_output_probe(struct mtk_output *output)
{
	struct device_node  *panel;

	DRM_INFO("DBG_jitao mtk_output_probe\n");

	if (!output->of_node)
		output->of_node = output->dev->of_node;

	panel = of_parse_phandle(output->of_node, "mediatek,panel", 0);
	if (panel) {
		output->panel = of_drm_find_panel(panel);
		if (!output->panel)
			return -EPROBE_DEFER;

		of_node_put(panel);
	}

	return 0;
}

