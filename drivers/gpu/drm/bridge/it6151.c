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

#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/regulator/consumer.h>

#include <drm/drmP.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_crtc.h>
#include <drm/drm_mipi_dsi.h>
#include <drm/drm_panel.h>



#define MIPI_RX_SW_RST            0x05
#define MIPI_RX_INT_MASK          0x09
#define MIPI_RX_SYS_CFG           0x0c
#define MIPI_RX_MCLK              0x11
#define MIPI_RX_PHY_IF_1          0x19
#define MIPI_RX_PKT_DEC           0x27
#define MIPI_RX_UFO_BLK_HIGH      0x28
#define MIPI_RX_UFO_BLK_LOW       0x29
#define MIPI_RX_UFO_HDE_DELAY     0x2e
#define MIPI_RX_UFO_RESYNC        0x2f
#define MIPI_RX_RESYNC_POL        0x4e
#define MIPI_RX_PCLK_HSCLK        0x80
#define MIPI_RX_AMP_TERM          0x84
#define MIPI_RX_TIMER_INT_CNT     0x92

#define DP_TX_VEN_ID_LOW        0x00
#define DP_TX_VEN_ID_HIGH       0x01
#define DP_TX_DEV_ID_LOW        0x02
#define DP_TX_DEV_ID_HIGH       0x03
#define DP_TX_REV_ID            0x04
#define DP_TX_SW_RST            0x05
#define DP_TX_INT_STA_0         0x06
#define DP_TX_INT_STA_1         0x07
#define DP_TX_INT_STA_2         0x08
#define DP_TX_INT_MASK_0        0x09
#define DP_TX_INT_MASK_1        0x0a
#define DP_TX_INT_MASK_2        0x0b
#define DP_TX_SYS_CFG           0x0c
#define DP_TX_SYS_DBG           0x0f
#define DP_TX_LANE              0x16
#define DP_TX_TRAIN             0x17
#define DP_TX_AUX_CH_FIFO       0x21
#define DP_TX_AUX_CLK           0x22
#define DP_TX_PC_REQ_FIFO       0x23
#define DP_TX_PC_REQ_OFST_0     0x24
#define DP_TX_PC_REQ_OFST_1     0x25
#define DP_TX_PC_REQ_OFST_2     0x26
#define DP_TX_PC_REQ_WD_0       0x27
#define DP_TX_PC_REQ_SEL        0x2b
#define DP_TX_HDP               0x3a
#define DP_TX_LNPWDB            0x5c
#define DP_TX_EQ                0x5f
#define DP_TX_COL_CONV_19       0x76
#define DP_TX_COL_CONV_20       0x77
#define DP_TX_PG_H_DE_END_L     0x7e
#define DP_TX_PG_H_DE_END_H     0x7f
#define DP_TX_PG_H_SYNC_START_L 0x80
#define DP_TX_PG_H_SYNC_START_H 0x81
#define DP_TX_PG_H_SYNC_END_L   0x82
#define DP_TX_PG_H_SYNC_END_H   0x83
#define DP_TX_PG_V_DE_END_L     0x88
#define DP_TX_PG_V_DE_END_H     0x89
#define DP_TX_PG_V_DE_START_L   0x8a
#define DP_TX_IN_VDO_TM_15      0xb5
#define DP_TX_IN_VDO_TM_17      0xb7
#define DP_TX_PSR_CTRL_0        0xc4
#define DP_TX_PSR_CTRL_1        0xc5
#define DP_TX_HDP_IRQ_TM        0xc9
#define DP_TX_AUX_DBG           0xca
#define DP_TX_AUX_MASTER        0xcb
#define DP_TX_PKT_OPT           0xce
#define DP_TX_VDO_FIFO          0xd3
#define DP_TX_VDO_STMP          0xd4
#define DP_TX_PKT               0xe8
#define DP_TX_PKT_AVI_VIC       0xec
#define DP_TX_MIPI_PORT         0xfd

enum {
	MIPI_1_LANE = 0,
	MIPI_2_LANE = 1,
	MIPI_3_LANE = 2,
	MIPI_4_LANE = 3,
};

enum {
	RGB_24b     = 0x3E,
	RGB_30b     = 0x0D,
	RGB_36b     = 0x1D,
	RGB_18b_P   = 0x1E,
	RGB_18b_L   = 0x2E,
	YCbCr_16b   = 0x2C,
	YCbCr_20b   = 0x0C,
	YCbCr_24b   = 0x1C,
};

enum {
	B_HBR   = 0,
	B_LBR   = 1,
};

enum {
	B_1_LANE    = 0,
	B_2_LANE    = 1,
	B_4_LANE    = 3,
};

enum {
	B_SSC_DISABLE   = 0,
	B_SSC_ENABLE    = 1,
};

struct it6151_driver_data {
	u8 training_bitrate;
	u8 dptx_ssc_setting;
	u8 mp_mclk_inv;
	u8 mp_continuous_clk;
	u8 mp_lane_deskew;
	u8 mp_pclk_div;
	u8 mp_lane_swap;
	u8 mp_pn_swap;

	u8 dp_pn_swap;
	u8 dp_aux_pn_swap;
	u8 dp_lane_swap;
	u8 int_mask;
	u8 mipi_int_mask;
	u8 timer_cnt;

	u16 panel_width;
	u8 vic;
	u8 mp_hpol;
	u8 mp_vpol;
	u8 mipi_lane_count;
	u8 dptx_lane_count;
	u8 en_ufo;
	u8 mipi_packed_fmt;
	u8 mp_h_resync;
	u8 mp_v_resync;
};

struct it6151_bridge {
	struct drm_connector connector;
	struct i2c_client *client;
	struct drm_encoder *encoder;
	struct it6151_driver_data *driver_data;
	int gpio_rst_n;
	u16 rx_reg;
	u16 tx_reg;
	bool enabled;
};

static int it6151_regr(struct i2c_client *client, u16 i2c_addr,
	u8 reg, u8 *value)
{
	int r;
	u8 tx_data[] = {
		reg,
	};
	u8 rx_data[1];
	struct i2c_msg msgs[] = {
		{
			.addr = i2c_addr,
			.flags = 0,
			.buf = tx_data,
			.len = ARRAY_SIZE(tx_data),
		},
		{
			.addr = i2c_addr,
			.flags = I2C_M_RD,
			.buf = rx_data,
			.len = ARRAY_SIZE(rx_data),
		 },
	};

	r = i2c_transfer(client->adapter, msgs, ARRAY_SIZE(msgs));
	if (r < 0) {
		dev_err(&client->dev, "%s: reg 0x%02x error %d\n", __func__,
			reg, r);
		return r;
	}

	if (r < ARRAY_SIZE(msgs)) {
		dev_err(&client->dev, "%s: reg 0x%02x msgs %d\n", __func__,
			reg, r);
		return -EAGAIN;
	}

	*value = rx_data[0];

	dev_dbg(&client->dev, "%s: reg 0x%02x value 0x%02x\n", __func__,
		reg, *value);

	return 0;
}

static int it6151_regw(struct i2c_client *client, u16 i2c_addr,
	u8 reg, u8 value)
{
	int r;
	u8 tx_data[] = {
		reg,
		value,
	};
	struct i2c_msg msgs[] = {
		{
			.addr = i2c_addr,
			.flags = 0,
			.buf = tx_data,
			.len = ARRAY_SIZE(tx_data),
		},
	};

	r = i2c_transfer(client->adapter, msgs, ARRAY_SIZE(msgs));
	if (r < 0) {
		dev_err(&client->dev, "%s: reg 0x%02x val 0x%02x error %d\n",
			__func__, reg, value, r);
		return r;
	}

	dev_dbg(&client->dev, "%s: reg 0x%02x val 0x%02x\n",
			__func__, reg, value);

	return 0;
}

static const struct drm_display_mode it6151_drm_default_modes[] = {
	/* 1368x768@60Hz */
	{ DRM_MODE("1368x768", DRM_MODE_TYPE_DRIVER, 72070,
		1368, 1368 + 58, 1368 + 58 + 58, 1368 + 58 + 58 + 58, 0,
		768, 768 + 4, 768 + 4 + 4, 768 + 4 + 4 + 4, 0, 0) },
};

static struct it6151_driver_data it6151_driver_data_1368x768 = {
	.training_bitrate = B_HBR,
	.dptx_ssc_setting = B_SSC_ENABLE,
	.mp_mclk_inv = 1,
	.mp_continuous_clk = 1,
	.mp_lane_deskew = 1,
	.mp_pclk_div = 2,
	.mp_lane_swap = 0,
	.mp_pn_swap = 0,

	.dp_pn_swap = 0,
	.dp_aux_pn_swap = 0,
	.dp_lane_swap = 0,
	.int_mask = 3,
	.mipi_int_mask = 0,
	.timer_cnt = 0xa,

	.panel_width = 1368,
	.vic = 0,
	.mp_hpol = 0,
	.mp_vpol = 1,
	.mipi_lane_count = MIPI_4_LANE,
	.dptx_lane_count = B_1_LANE,
	.en_ufo = 0,
	.mipi_packed_fmt = RGB_24b,
	.mp_h_resync = 1,
	.mp_v_resync = 0,
};

static struct it6151_driver_data *it6151_get_driver_data(void)
{
	return &it6151_driver_data_1368x768;
}

static int it6151_check_valid_id(struct it6151_bridge *ite_bridge)
{
	struct i2c_client *client = ite_bridge->client;
	u16 tx_reg = ite_bridge->tx_reg;
	u8 ven_id_low, ven_id_high, dev_id_low, dev_id_high;
	int retry_cnt = 0;

	do {
		it6151_regr(client, tx_reg, DP_TX_VEN_ID_LOW, &ven_id_low);
		if (ven_id_low != 0x54)
			DRM_ERROR("ven_id_low = 0x%x\n", ven_id_low);
	} while ((retry_cnt++ < 10) && (ven_id_low != 0x54));

	it6151_regr(client, tx_reg, DP_TX_VEN_ID_HIGH, &ven_id_high);
	it6151_regr(client, tx_reg, DP_TX_DEV_ID_LOW, &dev_id_low);
	it6151_regr(client, tx_reg, DP_TX_DEV_ID_HIGH, &dev_id_high);

	if ((ven_id_low == 0x54) && (ven_id_high == 0x49) &&
		(dev_id_low == 0x51) && (dev_id_high == 0x61))
		return 1;

	return 0;
}

static void it6151_mipirx_init(struct it6151_bridge *ite_bridge)
{
	struct i2c_client *client = ite_bridge->client;
	struct it6151_driver_data *drv_data = ite_bridge->driver_data;
	u16 rx_reg = ite_bridge->rx_reg;
	u16 tx_reg = ite_bridge->tx_reg;
	unsigned char rev_id;

	it6151_regr(client, tx_reg, DP_TX_REV_ID, &rev_id);

	it6151_regw(client, rx_reg, MIPI_RX_SW_RST, 0x00);
	it6151_regw(client, rx_reg, MIPI_RX_SYS_CFG,
		(drv_data->mp_lane_swap << 7) | (drv_data->mp_pn_swap << 6) |
		(drv_data->mipi_lane_count << 4) | drv_data->en_ufo);
	it6151_regw(client, rx_reg, MIPI_RX_MCLK, drv_data->mp_mclk_inv);

	if (rev_id == 0xA1)
		it6151_regw(client, rx_reg,
			MIPI_RX_PHY_IF_1, drv_data->mp_lane_deskew);
	else
		it6151_regw(client, rx_reg,
			MIPI_RX_PHY_IF_1,
			(drv_data->mp_continuous_clk << 1) |
			drv_data->mp_lane_deskew);

	it6151_regw(client, rx_reg, MIPI_RX_PKT_DEC, drv_data->mipi_packed_fmt);
	it6151_regw(client, rx_reg, MIPI_RX_UFO_BLK_HIGH,
		((drv_data->panel_width/4-1)>>2)&0xC0);
	it6151_regw(client, rx_reg, MIPI_RX_UFO_BLK_LOW,
		(drv_data->panel_width/4-1)&0xFF);
	it6151_regw(client, rx_reg, MIPI_RX_UFO_HDE_DELAY, 0x34);
	it6151_regw(client, rx_reg, MIPI_RX_UFO_RESYNC, 0x01);
	it6151_regw(client, rx_reg, MIPI_RX_RESYNC_POL,
		(drv_data->mp_v_resync<<3)|(drv_data->mp_h_resync<<2)|
		(drv_data->mp_vpol<<1)|(drv_data->mp_hpol));
	it6151_regw(client, rx_reg, MIPI_RX_PCLK_HSCLK,
		(drv_data->en_ufo<<5)|drv_data->mp_pclk_div);
	it6151_regw(client, rx_reg, MIPI_RX_AMP_TERM, 0x8f);
	it6151_regw(client, rx_reg, MIPI_RX_INT_MASK, drv_data->mipi_int_mask);
	it6151_regw(client, rx_reg, MIPI_RX_TIMER_INT_CNT, drv_data->timer_cnt);
}

static void it6151_dptx_init(struct it6151_bridge *ite_bridge)
{
	struct i2c_client *client = ite_bridge->client;
	struct it6151_driver_data *drv_data = ite_bridge->driver_data;
	u16 tx_reg = ite_bridge->tx_reg;

	it6151_regw(client, tx_reg, DP_TX_SW_RST, 0x29);
	it6151_regw(client, tx_reg, DP_TX_SW_RST, 0x00);
	it6151_regw(client, tx_reg, DP_TX_INT_MASK_0, drv_data->int_mask);
	it6151_regw(client, tx_reg, DP_TX_INT_MASK_1, 0x00);
	it6151_regw(client, tx_reg, DP_TX_INT_MASK_2, 0x00);
	it6151_regw(client, tx_reg, DP_TX_PSR_CTRL_1, 0xc1);
	it6151_regw(client, tx_reg, DP_TX_IN_VDO_TM_15, 0x00);
	it6151_regw(client, tx_reg, DP_TX_IN_VDO_TM_17, 0x80);
	it6151_regw(client, tx_reg, DP_TX_PSR_CTRL_0, 0xF0);
	it6151_regw(client, tx_reg, DP_TX_INT_STA_0, 0xFF);
	it6151_regw(client, tx_reg, DP_TX_INT_STA_1, 0xFF);
	it6151_regw(client, tx_reg, DP_TX_INT_STA_2, 0xFF);
	it6151_regw(client, tx_reg, DP_TX_SW_RST, 0x00);
	it6151_regw(client, tx_reg, DP_TX_SYS_CFG, 0x08);
	it6151_regw(client, tx_reg, DP_TX_AUX_CH_FIFO, 0x05);
	it6151_regw(client, tx_reg, DP_TX_HDP, 0x04);
	it6151_regw(client, tx_reg, DP_TX_EQ, 0x06);
	it6151_regw(client, tx_reg, DP_TX_HDP_IRQ_TM, 0xf5);
	it6151_regw(client, tx_reg, DP_TX_AUX_DBG, 0x4c);
	it6151_regw(client, tx_reg, DP_TX_AUX_MASTER, 0x37);
	it6151_regw(client, tx_reg, DP_TX_PKT_OPT, 0x80);
	it6151_regw(client, tx_reg, DP_TX_VDO_FIFO, 0x03);
	it6151_regw(client, tx_reg, DP_TX_VDO_STMP, 0x60);
	it6151_regw(client, tx_reg, DP_TX_PKT, 0x11);
	it6151_regw(client, tx_reg, DP_TX_PKT_AVI_VIC, drv_data->vic);
	mdelay(5);
	it6151_regw(client, tx_reg, DP_TX_PC_REQ_FIFO, 0x42);
	it6151_regw(client, tx_reg, DP_TX_PC_REQ_OFST_0, 0x07);
	it6151_regw(client, tx_reg, DP_TX_PC_REQ_OFST_1, 0x01);
	it6151_regw(client, tx_reg, DP_TX_PC_REQ_OFST_2, 0x00);
	it6151_regw(client, tx_reg, DP_TX_PC_REQ_WD_0, 0x10);
	it6151_regw(client, tx_reg, DP_TX_PC_REQ_SEL, 0x05);
	it6151_regw(client, tx_reg, DP_TX_PC_REQ_FIFO, 0x40);
	it6151_regw(client, tx_reg, DP_TX_AUX_CLK,
		(drv_data->dp_aux_pn_swap<<3)|(drv_data->dp_pn_swap<<2)|0x03);
	it6151_regw(client, tx_reg, DP_TX_LANE,
		(drv_data->dptx_ssc_setting<<4)|(drv_data->dp_lane_swap<<3)|
		(drv_data->dptx_lane_count<<1)|drv_data->training_bitrate);
	it6151_regw(client, tx_reg, DP_TX_SYS_DBG, 0x01);
	it6151_regw(client, tx_reg, DP_TX_COL_CONV_19, 0xa7);
	it6151_regw(client, tx_reg, DP_TX_COL_CONV_20, 0xaf);
	it6151_regw(client, tx_reg, DP_TX_PG_H_DE_END_L, 0x8f);
	it6151_regw(client, tx_reg, DP_TX_PG_H_DE_END_H, 0x07);
	it6151_regw(client, tx_reg, DP_TX_PG_H_SYNC_START_L, 0xef);
	it6151_regw(client, tx_reg, DP_TX_PG_H_SYNC_START_H, 0x5f);
	it6151_regw(client, tx_reg, DP_TX_PG_H_SYNC_END_L, 0xef);
	it6151_regw(client, tx_reg, DP_TX_PG_H_SYNC_END_H, 0x07);
	it6151_regw(client, tx_reg, DP_TX_PG_V_DE_END_L, 0x38);
	it6151_regw(client, tx_reg, DP_TX_PG_V_DE_END_H, 0x1f);
	it6151_regw(client, tx_reg, DP_TX_PG_V_DE_START_L, 0x48);
	it6151_regw(client, tx_reg, DP_TX_SYS_DBG, 0x00);
	it6151_regw(client, tx_reg, DP_TX_LNPWDB, 0xf3);
	it6151_regw(client, tx_reg, DP_TX_TRAIN, 0x04);
	it6151_regw(client, tx_reg, DP_TX_TRAIN, 0x01);
	mdelay(5);
}

static int it6151_bdg_enable(struct it6151_bridge *ite_bridge)
{
	struct i2c_client *client = ite_bridge->client;
	u16 tx_reg = ite_bridge->tx_reg;

	if (it6151_check_valid_id(ite_bridge)) {
		it6151_regw(client, tx_reg, DP_TX_SW_RST, 0x04);
		it6151_regw(client, tx_reg, DP_TX_MIPI_PORT,
			(ite_bridge->rx_reg<<1)|1);

		it6151_mipirx_init(ite_bridge);
		it6151_dptx_init(ite_bridge);

		return 0;
	}

	return -1;
}

static void it6151_pre_enable(struct drm_bridge *bridge)
{
	/* drm framework doesn't check NULL. */
}

static void it6151_enable(struct drm_bridge *bridge)
{
	struct it6151_bridge *ite_bridge = bridge->driver_private;

	gpio_direction_output(ite_bridge->gpio_rst_n, 0);
	udelay(15);
	gpio_direction_output(ite_bridge->gpio_rst_n, 1);

	it6151_bdg_enable(ite_bridge);

	ite_bridge->enabled = true;
}

static void it6151_disable(struct drm_bridge *bridge)
{
	struct it6151_bridge *ite_bridge = bridge->driver_private;

	if (!ite_bridge->enabled)
		return;

	ite_bridge->enabled = false;

	if (gpio_is_valid(ite_bridge->gpio_rst_n))
		gpio_set_value(ite_bridge->gpio_rst_n, 1);
}

static void it6151_post_disable(struct drm_bridge *bridge)
{
	/* drm framework doesn't check NULL. */
}

static struct drm_bridge_funcs it6151_bridge_funcs = {
	.pre_enable = it6151_pre_enable,
	.enable = it6151_enable,
	.disable = it6151_disable,
	.post_disable = it6151_post_disable,
};

static int it6151_get_modes(struct drm_connector *connector)
{
	const struct drm_display_mode *ptr = &it6151_drm_default_modes[0];
	struct drm_display_mode *mode;
	int count = 0;

	mode = drm_mode_duplicate(connector->dev, ptr);
	if (mode) {
		drm_mode_probed_add(connector, mode);
		count++;
	}

	connector->display_info.width_mm = mode->hdisplay;
	connector->display_info.height_mm = mode->vdisplay;

	return count;
}

static int it6151_mode_valid(struct drm_connector *connector,
		struct drm_display_mode *mode)
{
	return MODE_OK;
}

static struct drm_encoder *it6151_best_encoder(struct drm_connector *connector)
{
	struct it6151_bridge *ite_bridge;

	ite_bridge = container_of(connector, struct it6151_bridge, connector);
	return ite_bridge->encoder;
}

static struct drm_connector_helper_funcs it6151_connector_helper_funcs = {
	.get_modes = it6151_get_modes,
	.mode_valid = it6151_mode_valid,
	.best_encoder = it6151_best_encoder,
};

static enum drm_connector_status it6151_detect(struct drm_connector *connector,
		bool force)
{
	return connector_status_connected;
}

static void it6151_connector_destroy(struct drm_connector *connector)
{
	drm_connector_cleanup(connector);
}

static struct drm_connector_funcs it6151_connector_funcs = {
	.dpms = drm_helper_connector_dpms,
	.fill_modes = drm_helper_probe_single_connector_modes,
	.detect = it6151_detect,
	.destroy = it6151_connector_destroy,
};

int it6151_init(struct drm_device *dev, struct drm_encoder *encoder,
	struct i2c_client *client, struct device_node *node)
{
	int ret;
	struct drm_bridge *bridge;
	struct it6151_bridge *ite_bridge;
	u32 rx_reg, tx_reg;

	bridge = devm_kzalloc(dev->dev, sizeof(*bridge), GFP_KERNEL);
	if (!bridge)
		return -ENOMEM;

	ite_bridge = devm_kzalloc(dev->dev, sizeof(*ite_bridge), GFP_KERNEL);
	if (!ite_bridge)
		return -ENOMEM;

	ite_bridge->encoder = encoder;
	ite_bridge->client = client;
	ite_bridge->driver_data = it6151_get_driver_data();

	ite_bridge->gpio_rst_n = of_get_named_gpio(node, "reset-gpio", 0);
	if (gpio_is_valid(ite_bridge->gpio_rst_n)) {
		ret = gpio_request_one(ite_bridge->gpio_rst_n,
			GPIOF_OUT_INIT_LOW, "mtk_rst");
		if (ret)
			return ret;
	}

	ret = of_property_read_u32(node, "reg",	&tx_reg);
	if (ret) {
		DRM_ERROR("Can't read reg value\n");
		goto err;
	}
	ite_bridge->tx_reg = tx_reg;

	ret = of_property_read_u32(node, "rxreg", &rx_reg);
	if (ret) {
		DRM_ERROR("Can't read rxreg value\n");
		goto err;
	}
	ite_bridge->rx_reg = rx_reg;

	bridge->funcs = &it6151_bridge_funcs;
	ret = drm_bridge_attach(dev, bridge);
	if (ret)
		goto err;

	bridge->driver_private = ite_bridge;
	encoder->bridge = bridge;

	ret = drm_connector_init(dev, &ite_bridge->connector,
		&it6151_connector_funcs, DRM_MODE_CONNECTOR_eDP);
	if (ret)
		goto err;

	drm_connector_helper_add(&ite_bridge->connector,
		&it6151_connector_helper_funcs);
	drm_connector_register(&ite_bridge->connector);
	drm_mode_connector_attach_encoder(&ite_bridge->connector, encoder);

	return 0;

err:
	if (gpio_is_valid(ite_bridge->gpio_rst_n))
		gpio_free(ite_bridge->gpio_rst_n);

	return ret;
}
