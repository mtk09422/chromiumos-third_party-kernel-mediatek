/*
 * Copyright (c) 2014 MediaTek Inc.
 * Author: Jie Qiu <jie.qiu@mediatek.com>
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
#include <drm/drm_crtc.h>
#include <drm/drm_crtc_helper.h>
#include <linux/i2c.h>
#include <linux/mutex.h>
#include <linux/kernel.h>
#include <linux/component.h>
#include <linux/platform_device.h>
#include <linux/of_platform.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/of_irq.h>
#include <linux/clk.h>
#include <linux/kthread.h>
#include <linux/regmap.h>
#include <linux/debugfs.h>

#include "mediatek_hdmi_display_core.h"
#include "mediatek_hdmi_ctrl.h"
#include "mediatek_hdmi_regs.h"

static void hdmi_encoder_destroy(struct drm_encoder *encoder)
{
	drm_encoder_cleanup(encoder);
}

static void hdmi_encoder_dpms(struct drm_encoder *encoder, int mode)
{
	struct mediatek_hdmi_context *hctx;

	hctx = hdmi_ctx_from_encoder(encoder);
	if (DRM_MODE_DPMS_ON == mode)
		mtk_hdmi_signal_on(hctx);
	else
		mtk_hdmi_signal_off(hctx);
}

static bool hdmi_encoder_mode_fixup(struct drm_encoder *encoder,
			       const struct drm_display_mode *mode,
			       struct drm_display_mode *adjusted_mode)
{
	return true;
}

static void hdmi_encoder_mode_set(struct drm_encoder *encoder,
				  struct drm_display_mode *mode,
				  struct drm_display_mode *adjusted_mode)
{
	struct mediatek_hdmi_context *hdmi_context = NULL;

	hdmi_context = hdmi_ctx_from_encoder(encoder);
	if (!hdmi_context) {
		mtk_hdmi_err("%s failed, invalid hdmi context!\n", __func__);
		return;
	}

	mtk_hdmi_display_set_vid_format(hdmi_context->display_node,
					adjusted_mode);
}

static void hdmi_encoder_prepare(struct drm_encoder *encoder)
{
	/* DRM_MODE_DPMS_OFF? */

	/* drm framework doesn't check NULL. */
}

static void hdmi_encoder_commit(struct drm_encoder *encoder)
{
	/* DRM_MODE_DPMS_ON? */
}

static enum drm_connector_status hdmi_conn_detect(struct drm_connector *conn,
						  bool force)
{
	struct mediatek_hdmi_context *hdmi_context = hdmi_ctx_from_conn(conn);
	int val;

	val = gpio_get_value(hdmi_context->htplg_gpio);
	if (val == 1)
		return connector_status_connected;
	else
		return connector_status_disconnected;
}

static void hdmi_conn_destroy(struct drm_connector *conn)
{
	drm_connector_cleanup(conn);
}

static int hdmi_conn_set_property(struct drm_connector *conn,
				  struct drm_property *property, uint64_t val)
{
	return 0;
}

static int mtk_hdmi_conn_get_modes(struct drm_connector *conn)
{
	struct mediatek_hdmi_context *hdmi_context = hdmi_ctx_from_conn(conn);
	struct edid *edid;

	if (!hdmi_context->ddc_adpt)
		return -ENODEV;

	edid = drm_get_edid(conn, hdmi_context->ddc_adpt);
	if (!edid)
		return -ENODEV;

	drm_mode_connector_update_edid_property(conn, edid);

	return drm_add_edid_modes(conn, edid);
}

static int mtk_hdmi_conn_mode_valid(struct drm_connector *conn,
				    struct drm_display_mode *mode)
{
	mtk_hdmi_info("xres=%d, yres=%d, refresh=%d, intl=%d clock=%d\n",
		mode->hdisplay, mode->vdisplay, mode->vrefresh,
		(mode->flags & DRM_MODE_FLAG_INTERLACE) ? true :
		false, mode->clock * 1000);

	if (((mode->clock) >= 27000) || ((mode->clock) <= 297000))
		return MODE_OK;

	return MODE_BAD;
}

static struct drm_encoder *mtk_hdmi_conn_best_enc(struct drm_connector *conn)
{
	struct mediatek_hdmi_context *hdmi_context = hdmi_ctx_from_conn(conn);

	return &hdmi_context->encoder;
}

static const struct drm_encoder_funcs mtk_hdmi_encoder_funcs = {
	.destroy = hdmi_encoder_destroy,
};

static const struct drm_encoder_helper_funcs mtk_hdmi_encoder_helper_funcs = {
	.dpms = hdmi_encoder_dpms,
	.mode_fixup = hdmi_encoder_mode_fixup,
	.mode_set = hdmi_encoder_mode_set,
	.prepare = hdmi_encoder_prepare,
	.commit = hdmi_encoder_commit,
};

static const struct drm_connector_funcs mtk_hdmi_connector_funcs = {
	.dpms = drm_helper_connector_dpms,
	.detect = hdmi_conn_detect,
	.fill_modes = drm_helper_probe_single_connector_modes,
	.destroy = hdmi_conn_destroy,
	.set_property = hdmi_conn_set_property,
};

static const struct drm_connector_helper_funcs
	mtk_hdmi_connector_helper_funcs = {
	.get_modes = mtk_hdmi_conn_get_modes,
	.mode_valid = mtk_hdmi_conn_mode_valid,
	.best_encoder = mtk_hdmi_conn_best_enc,
};

static int mtk_hdmi_create_conn_enc(struct mediatek_hdmi_context *hdmi_context)
{
	int ret;

	ret =
	    drm_encoder_init(hdmi_context->drm_dev, &hdmi_context->encoder,
			     &mtk_hdmi_encoder_funcs, DRM_MODE_ENCODER_TMDS);
	if (ret) {
		mtk_hdmi_err("drm_encoder_init failed! ret = %d\n", ret);
		goto errcode;
	}
	drm_encoder_helper_add(&hdmi_context->encoder,
			       &mtk_hdmi_encoder_helper_funcs);

	ret =
	    drm_connector_init(hdmi_context->drm_dev, &hdmi_context->conn,
			       &mtk_hdmi_connector_funcs,
			       DRM_MODE_CONNECTOR_HDMIA);
	if (ret) {
		mtk_hdmi_err("drm_connector_init failed! ret = %d\n", ret);
		goto errcode;
	}
	drm_connector_helper_add(&hdmi_context->conn,
				 &mtk_hdmi_connector_helper_funcs);

	hdmi_context->conn.polled = DRM_CONNECTOR_POLL_HPD;
	hdmi_context->conn.interlace_allowed = true;
	hdmi_context->conn.doublescan_allowed = false;

	ret = drm_connector_register(&hdmi_context->conn);
	if (ret) {
		mtk_hdmi_err("drm_connector_register failed! (%d)\n", ret);
		goto errcode;
	}

	ret =
	    drm_mode_connector_attach_encoder(&hdmi_context->conn,
					      &hdmi_context->encoder);
	if (ret) {
		mtk_hdmi_err("drm_mode_connector_attach_encoder failed! (%d)\n",
			     ret);
		goto errcode;
	}

	hdmi_context->conn.encoder = &hdmi_context->encoder;
	hdmi_context->encoder.possible_crtcs = 0x2;

	mtk_hdmi_info("create encoder and connector success!\n");

	return 0;

errcode:
	return ret;
}

static void mtk_hdmi_destroy_conn_enc(struct mediatek_hdmi_context
				      *hdmi_context)
{
	if (hdmi_context == NULL)
		return;

	drm_encoder_cleanup(&hdmi_context->encoder);
	drm_connector_unregister(&hdmi_context->conn);
	drm_connector_cleanup(&hdmi_context->conn);
}

static struct mediatek_hdmi_display_ops hdmi_display_ops = {
	.init = mtk_hdmi_output_init,
	.set_format = mtk_hdmi_output_set_display_mode,
};

static int mtk_hdmi_bind(struct device *dev, struct device *master, void *data)
{
	int ret;
	struct device_node *prenode;
	struct mediatek_hdmi_context *hdmi_context = NULL;
	struct meidatek_hdmi_display_node *display_node = NULL;

	hdmi_context = platform_get_drvdata(to_platform_device(dev));
	if (!hdmi_context) {
		mtk_hdmi_err(" platform_get_drvdata failed!\n");
		ret = -EFAULT;
		goto errcode;
	}

	prenode = of_parse_phandle(dev->of_node, "prenode", 0);
	if (!prenode) {
		mtk_hdmi_err("find prenode node failed!\n");
		ret = -EFAULT;
		goto errcode;
	}

	display_node = mtk_hdmi_display_find_node(prenode);
	if (!display_node) {
		mtk_hdmi_err("find display node failed!\n");
		ret = -EFAULT;
		goto errcode;
	}

	if (mtk_hdmi_display_add_pre_node
	    (hdmi_context->display_node, display_node)) {
		mtk_hdmi_err("add display node failed!\n");
		ret = -EFAULT;
		goto errcode;
	}

	if (!mtk_hdmi_display_init(hdmi_context->display_node))
		mtk_hdmi_err("init display failed!\n");

	hdmi_context->drm_dev = data;
	mtk_hdmi_info(" hdmi_context = %p, data = %p , display_node = %p\n",
		      hdmi_context, data, display_node);

	ret = mtk_hdmi_create_conn_enc(hdmi_context);
	return ret;

errcode:
	return ret;
}

static void mtk_hdmi_unbind(struct device *dev,
			    struct device *master, void *data)
{
	struct mediatek_hdmi_context *hdmi_context = NULL;

	hdmi_context = platform_get_drvdata(to_platform_device(dev));
	mtk_hdmi_destroy_conn_enc(hdmi_context);

	hdmi_context->drm_dev = NULL;
}

static const struct component_ops mediatek_hdmi_component_ops = {
	.bind = mtk_hdmi_bind,
	.unbind = mtk_hdmi_unbind,
};

static void __iomem *mtk_hdmi_resource_ioremap(struct platform_device *pdev,
					       u32 index)
{
	struct resource *mem;

	mem = platform_get_resource(pdev, IORESOURCE_MEM, index);
	if (!mem) {
		mtk_hdmi_err("get memory source fail!\n");
		return ERR_PTR(-EFAULT);
	}

	mtk_hdmi_info("index :%d , physical adr: 0x%llx, end: 0x%llx\n", index,
		      mem->start, mem->end);

	return devm_ioremap_resource(&pdev->dev, mem);
}

static int mtk_hdmi_dt_parse_pdata(struct mediatek_hdmi_context *hdmi_context,
				   struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	struct device_node *i2c_np = NULL;

	hdmi_context->flt_n_5v_gpio = of_get_gpio(np, 0);
	if (hdmi_context->flt_n_5v_gpio < 0) {
		mtk_hdmi_err
		    ("hdmi_context->flt_n_5v_gpio = %d\n",
		     hdmi_context->flt_n_5v_gpio);
		goto errcode;
	}

	hdmi_context->en_5v_gpio = of_get_gpio(np, 1);
	if (hdmi_context->en_5v_gpio < 0) {
		mtk_hdmi_err
		    ("get en_5v_gpio failed! hdmi_context->en_5v_gpio = %d\n",
		     hdmi_context->en_5v_gpio);
		goto errcode;
	}

	hdmi_context->htplg_gpio = of_get_gpio(np, 2);
	if (hdmi_context->htplg_gpio < 0) {
		mtk_hdmi_err
		    ("get htplg_gpio failed! hdmi_context->htplg_gpio = %d\n",
		     hdmi_context->htplg_gpio);
		goto errcode;
	}

	hdmi_context->tvd_clk = of_clk_get_by_name(np, "tvdpll-clk");
	if (IS_ERR(hdmi_context->tvd_clk)) {
		mtk_hdmi_err(" get tvd_clk failed : %p ,\n",
			     hdmi_context->tvd_clk);
		goto errcode;
	}
	hdmi_context->dpi_clk_mux = of_clk_get_by_name(np, "dpi-clk-mux");
	if (IS_ERR(hdmi_context->dpi_clk_mux)) {
		mtk_hdmi_err(" get dpi_clk_mux failed : %p ,\n",
			     hdmi_context->dpi_clk_mux);
		goto errcode;
	}

	hdmi_context->dpi_clk_div2 = of_clk_get_by_name(np, "dpi-clk-div2");
	if (IS_ERR(hdmi_context->dpi_clk_div2)) {
		mtk_hdmi_err(" get dpi_clk_div1 failed : %p ,\n",
			     hdmi_context->dpi_clk_div2);
		goto errcode;
	}

	hdmi_context->dpi_clk_div4 = of_clk_get_by_name(np, "dpi-clk-div4");
	if (IS_ERR(hdmi_context->dpi_clk_div4)) {
		mtk_hdmi_err(" get dpi_clk_div2 failed : %p ,\n",
			     hdmi_context->dpi_clk_div4);
		goto errcode;
	}

	hdmi_context->dpi_clk_div8 = of_clk_get_by_name(np, "dpi-clk-div8");
	if (IS_ERR(hdmi_context->dpi_clk_div8)) {
		mtk_hdmi_err(" get dpi_clk_div4 failed : %p ,\n",
			     hdmi_context->dpi_clk_div8);
		goto errcode;
	}

	hdmi_context->hdmitx_clk_mux = of_clk_get_by_name(np, "hdmitx-clk-mux");
	if (IS_ERR(hdmi_context->hdmitx_clk_mux)) {
		mtk_hdmi_err(" get hdmitx_clk_mux failed : %p ,\n",
			     hdmi_context->hdmitx_clk_mux);
		goto errcode;
	}

	hdmi_context->hdmitx_clk_div1 =
	    of_clk_get_by_name(np, "hdmitx-clk-div1");
	if (IS_ERR(hdmi_context->hdmitx_clk_div1)) {
		mtk_hdmi_err(" get hdmitx_clk_div1 failed : %p ,\n",
			     hdmi_context->hdmitx_clk_div1);
		goto errcode;
	}

	hdmi_context->hdmitx_clk_div2 =
	    of_clk_get_by_name(np, "hdmitx-clk-div2");
	if (IS_ERR(hdmi_context->hdmitx_clk_div2)) {
		mtk_hdmi_err(" get hdmitx_clk_div2 failed : %p ,\n",
			     hdmi_context->hdmitx_clk_div2);
		goto errcode;
	}

	hdmi_context->hdmitx_clk_div3 =
	    of_clk_get_by_name(np, "hdmitx-clk-div3");
	if (IS_ERR(hdmi_context->hdmitx_clk_div3)) {
		mtk_hdmi_err(" get hdmitx_clk_div3 failed : %p ,\n",
			     hdmi_context->hdmitx_clk_div3);
		goto errcode;
	}

	hdmi_context->hdmi_id_clk_gate =
	    of_clk_get_by_name(np, "hdmi-id-clk-gate");
	if (IS_ERR(hdmi_context->hdmi_id_clk_gate)) {
		mtk_hdmi_err(" get hdmi_id_clk_gate failed : %p ,\n",
			     hdmi_context->hdmi_id_clk_gate);
		goto errcode;
	}

	hdmi_context->hdmi_pll_clk_gate =
	    of_clk_get_by_name(np, "hdmi-pll-clk-gate");
	if (IS_ERR(hdmi_context->hdmi_pll_clk_gate)) {
		mtk_hdmi_err(" get hdmi_pll_clk_gate failed : %p ,\n",
			     hdmi_context->hdmi_pll_clk_gate);
		goto errcode;
	}

	hdmi_context->hdmi_dpi_clk_gate =
	    of_clk_get_by_name(np, "dpi-clk-gate");
	if (IS_ERR(hdmi_context->hdmi_dpi_clk_gate)) {
		mtk_hdmi_err(" get hdmi_dpi_clk_gate failed : %p ,\n",
			     hdmi_context->hdmi_dpi_clk_gate);
		goto errcode;
	}

	hdmi_context->hdmi_dpi_eng_clk_gate =
	    of_clk_get_by_name(np, "dpi-eng-clk-gate");
	if (IS_ERR(hdmi_context->hdmi_dpi_eng_clk_gate)) {
		mtk_hdmi_err(" get hdmi_dpi_eng_clk_gate failed : %p ,\n",
			     hdmi_context->hdmi_dpi_eng_clk_gate);
		goto errcode;
	}

	hdmi_context->hdmi_aud_bclk_gate =
	    of_clk_get_by_name(np, "aud-bclk-gate");
	if (IS_ERR(hdmi_context->hdmi_aud_bclk_gate)) {
		mtk_hdmi_err(" get hdmi_dpi_clk_gate failed : %p ,\n",
			     hdmi_context->hdmi_aud_bclk_gate);
		goto errcode;
	}

	hdmi_context->hdmi_aud_spdif_gate =
	    of_clk_get_by_name(np, "aud-spdif-gate");
	if (IS_ERR(hdmi_context->hdmi_aud_spdif_gate)) {
		mtk_hdmi_err(" get hdmi_dpi_clk_gate failed : %p ,\n",
			     hdmi_context->hdmi_aud_spdif_gate);
		goto errcode;
	}

	hdmi_context->sys_regs = mtk_hdmi_resource_ioremap(pdev, 0);
	if (IS_ERR(hdmi_context->sys_regs)) {
		mtk_hdmi_err(" mem resource ioremap failed\n");
		goto errcode;
	}

	hdmi_context->pll_regs = mtk_hdmi_resource_ioremap(pdev, 1);
	if (IS_ERR(hdmi_context->sys_regs)) {
		mtk_hdmi_err(" mem resource ioremap failed!\n");
		goto errcode;
	}

	hdmi_context->grl_regs = mtk_hdmi_resource_ioremap(pdev, 2);
	if (IS_ERR(hdmi_context->sys_regs)) {
		mtk_hdmi_err(" mem resource ioremap failed!\n");
		goto errcode;
	}

	i2c_np = of_parse_phandle(np, "i2cnode", 0);
	if (!i2c_np) {
		mtk_hdmi_err("find i2c node failed!\n");
		goto errcode;
	}

	hdmi_context->ddc_adpt = of_find_i2c_adapter_by_node(i2c_np);
	if (!hdmi_context->ddc_adpt) {
		mtk_hdmi_err("Failed to get ddc i2c adapter by node\n");
		goto errcode;
	}

	mtk_hdmi_info("hdmi_context->ddc_adpt :0x%p\n", hdmi_context->ddc_adpt);
	mtk_hdmi_info("hdmi_context->en_5v_gpio :%d\n",
		      hdmi_context->en_5v_gpio);
	mtk_hdmi_info("hdmi_context->flt_n_5v_gpio :%d\n",
		      hdmi_context->flt_n_5v_gpio);
	mtk_hdmi_info("hdmi_context->htplg_gpio :%d\n",
		      hdmi_context->htplg_gpio);
	mtk_hdmi_info("hdmi_context->tvd_clk :0x%p\n", hdmi_context->tvd_clk);
	mtk_hdmi_info("hdmi_context->dpi_clk_mux :0x%p\n",
		      hdmi_context->dpi_clk_mux);
	mtk_hdmi_info("hdmi_context->dpi_clk_div2 :0x%p\n",
		      hdmi_context->dpi_clk_div2);
	mtk_hdmi_info("hdmi_context->dpi_clk_div4 :0x%p\n",
		      hdmi_context->dpi_clk_div4);
	mtk_hdmi_info("hdmi_context->dpi_clk_div8 :0x%p\n",
		      hdmi_context->dpi_clk_div8);
	mtk_hdmi_info("hdmi_context->hdmitx_clk_mux :0x%p\n",
		      hdmi_context->hdmitx_clk_mux);
	mtk_hdmi_info("hdmi_context->hdmitx_clk_div1 :0x%p\n",
		      hdmi_context->hdmitx_clk_div1);
	mtk_hdmi_info("hdmi_context->hdmitx_clk_div2 :0x%p\n",
		      hdmi_context->hdmitx_clk_div2);
	mtk_hdmi_info("hdmi_context->hdmitx_clk_div3 :0x%p\n",
		      hdmi_context->hdmitx_clk_div3);
	mtk_hdmi_info("hdmi_context->hdmi_id_clk_gate :0x%p\n",
		      hdmi_context->hdmi_id_clk_gate);
	mtk_hdmi_info("hdmi_context->hdmi_pll_clk_gate :0x%p\n",
		      hdmi_context->hdmi_pll_clk_gate);
	mtk_hdmi_info("hdmi_context->hdmi_dpi_clk_gate :0x%p\n",
		      hdmi_context->hdmi_dpi_clk_gate);
	mtk_hdmi_info("hdmi_context->hdmi_dpi_eng_clk_gate :0x%p\n",
		      hdmi_context->hdmi_dpi_eng_clk_gate);
	mtk_hdmi_info("hdmi_context :0x%p\n", hdmi_context);

	return 0;

errcode:
	return -ENXIO;
}

static int mtk_hdmi_thread(void *data)
{
	struct mediatek_hdmi_context *hdmi_context = data;
	int val;

	while (!kthread_should_stop()) {
		msleep_interruptible(200);
		val = gpio_get_value(hdmi_context->htplg_gpio);
		if (hdmi_context->hpd != val) {
			mtk_hdmi_info
			    ("hotplug event! hctx hpd=%d,val=%d\n",
			     hdmi_context->hpd, val);
			hdmi_context->hpd = val;
			if (hdmi_context->drm_dev)
				drm_helper_hpd_irq_event(hdmi_context->drm_dev);
		}
	}

	return 0;
}

static irqreturn_t hdmi_flt_n_5v_irq_thread(int irq, void *arg)
{
	mtk_hdmi_err("detected 5v pin error status\n");
	return IRQ_HANDLED;
}

static int mtk_drm_hdmi_probe(struct platform_device *pdev)
{
	int ret;
	struct mediatek_hdmi_context *hctx = NULL;

	hctx = kzalloc(sizeof(*hctx), GFP_KERNEL);
	if (!hctx) {
		ret = -ENOMEM;
		goto errcode;
	}

	ret = mtk_hdmi_dt_parse_pdata(hctx, pdev);
	if (ret) {
		mtk_hdmi_err("mtk_hdmi_dt_parse_pdata failed!!\n");
		goto errcode;
	}

	ret = gpio_direction_output(hctx->en_5v_gpio, 1);
	if (ret) {
		mtk_hdmi_err("enable 5V failed! ret = %d\n", ret);
		goto errcode;
	}

	ret = gpio_direction_input(hctx->flt_n_5v_gpio);
	if (ret) {
		mtk_hdmi_err("config en_5v_gpio failed! ret = %d\n", ret);
		goto errcode;
	}

	ret = gpio_direction_input(hctx->htplg_gpio);
	if (ret) {
		mtk_hdmi_err("config htplg_gpio failed! ret = %d\n", ret);
		goto errcode;
	}

	hctx->flt_n_5v_irq = gpio_to_irq(hctx->flt_n_5v_gpio);
	if (hctx->flt_n_5v_irq < 0) {
		mtk_hdmi_err("hdmi_context->flt_n_5v_irq = %d\n",
			     hctx->flt_n_5v_irq);
		goto errcode;
	}

	ret = devm_request_threaded_irq(&pdev->dev, hctx->flt_n_5v_irq,
					NULL, hdmi_flt_n_5v_irq_thread,
					IRQF_TRIGGER_RISING |
					IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
					"hdmi flt_n_5v", hctx);
	if (ret) {
		mtk_hdmi_err("failed to register hdmi flt_n_5v interrupt\n");
		goto errcode;
	}

	hctx->kthread = kthread_run(mtk_hdmi_thread, hctx, "mtk hdmi thread");
	if (IS_ERR(hctx->kthread)) {
		mtk_hdmi_err("create kthread failed !\n");
		ret = PTR_ERR(hctx->kthread);
		goto errcode;
	}

	hctx->display_node = mtk_hdmi_display_create_node(&hdmi_display_ops,
							  pdev->dev.of_node,
							  hctx);
	if (IS_ERR(hctx->display_node)) {
		mtk_hdmi_err("mtk_hdmi_display_create_node failed!\n");
		ret = PTR_ERR(hctx->display_node);
		goto errcode;
	}

	platform_set_drvdata(pdev, hctx);

	mutex_init(&hctx->hdmi_mutex);

	ret = component_add(&pdev->dev, &mediatek_hdmi_component_ops);
	if (ret) {
		mtk_hdmi_err("component_add failed !\n");
		goto errcode;
	}

	#if defined(CONFIG_DEBUG_FS)
	mtk_drm_hdmi_debugfs_init(hctx);
	#endif

	mtk_hdmi_info("hdmi_context->sys_regs : 0x%p\n", hctx->sys_regs);
	mtk_hdmi_info("hdmi_context->pll_regs : 0x%p\n", hctx->pll_regs);
	mtk_hdmi_info("hdmi_context->grl_regs : 0x%p\n", hctx->grl_regs);
	mtk_hdmi_info("hdmi_context->display_node : 0x%p\n",
		      hctx->display_node);
	return 0;

errcode:
	component_del(&pdev->dev, &mediatek_hdmi_component_ops);

	if (hctx != NULL)
		kfree(hctx);

	return ret;
}

static int mtk_drm_hdmi_remove(struct platform_device *pdev)
{
	struct mediatek_hdmi_context *hdmi_context = NULL;

	hdmi_context = platform_get_drvdata(pdev);

	component_del(&pdev->dev, &mediatek_hdmi_component_ops);
	platform_set_drvdata(pdev, NULL);

	if (hdmi_context != NULL)
		kfree(hdmi_context);

	return 0;
}

static const struct of_device_id mediatek_drm_hdmi_of_ids[] = {
	{.compatible = "mediatek,mt8173-drm-hdmi",},
	{}
};

struct platform_driver mediatek_hdmi_platform_driver = {
	.probe = mtk_drm_hdmi_probe,
	.remove = mtk_drm_hdmi_remove,
	.driver = {
		   .owner = THIS_MODULE,
		   .name = "mediatek-drm-hdmi",
		   .of_match_table = mediatek_drm_hdmi_of_ids,
		   },
};

int mtk_hdmitx_init(void)
{
	int ret;

	ret = platform_driver_register(&mediatek_hdmi_ddc_driver);
	if (ret < 0)
		mtk_hdmi_err("register hdmiddc platform driver failed!");

	ret = platform_driver_register(&mediate_hdmi_dpi_driver);
	if (ret < 0)
		mtk_hdmi_err("register hdmidpi platform driver failed!");

	ret = platform_driver_register(&mediatek_hdmi_platform_driver);
	if (ret < 0)
		mtk_hdmi_err("register hdmitx platform driver failed!");

	return 0;
}

module_init(mtk_hdmitx_init);

MODULE_AUTHOR("Jie Qiu <jie.qiu@mediatek.com>");
MODULE_DESCRIPTION("MediaTek HDMI Driver");
MODULE_LICENSE("GPL");
