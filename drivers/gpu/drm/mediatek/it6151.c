
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


struct it6151_bridge {
	struct drm_connector connector;
	struct i2c_client *mipirx_client;
	struct i2c_client *dptx_client;
	struct drm_encoder *encoder;
	struct drm_bridge *bridge;
	struct regulator_bulk_data *supplies;
	u32 gpio_pwr12_en_n;
	u32 gpio_rst_n;
	bool enabled;
};

/*
//#define PANEL_RESOLUTION_1280x800_NOUFO
//#define PANEL_RESOLUTION_2048x1536_NOUFO_18B
//#define PANEL_RESOLUTION_2048x1536
//#define PANEL_RESOLUTION_2048x1536_NOUFO // FOR INTEL Platform
// #define PANEL_RESOLUTION_1920x1200p60RB
//#define PANEL_RESOLUTION_1920x1080p60
*/

#define PANEL_RESULUTION_1536x2048

#define MIPI_4_LANE		(3)
#define MIPI_3_LANE		(2)
#define MIPI_2_LANE		(1)
#define MIPI_1_LANE		(0)

#define RGB_24b         (0x3E)
#define RGB_30b         (0x0D)
#define RGB_36b         (0x1D)
#define RGB_18b_P       (0x1E)
#define RGB_18b_L       (0x2E)
#define YCbCr_16b       (0x2C)
#define YCbCr_20b       (0x0C)
#define YCbCr_24b       (0x1C)

#define B_DPTXIN_6Bpp   (0)
#define B_DPTXIN_8Bpp   (1)
#define B_DPTXIN_10Bpp  (2)
#define B_DPTXIN_12Bpp  (3)

#define B_LBR			(1)
#define B_HBR			(0)

#define B_4_LANE		(3)
#define B_2_LANE		(1)
#define B_1_LANE		(0)

#define B_SSC_ENABLE	(1)
#define B_SSC_DISABLE	(0)

#define TRAINING_BITRATE	(B_HBR)
#define DPTX_SSC_SETTING	(B_SSC_ENABLE)/*(B_SSC_DISABLE)*/
#define HIGH_PCLK			(1)
#define MP_MCLK_INV			(1)
#define MP_CONTINUOUS_CLK	(1)
#define MP_LANE_DESKEW		(1)
#define MP_PCLK_DIV			(2)
#define MP_LANE_SWAP		(0)
#define MP_PN_SWAP			(0)

#define DP_PN_SWAP			(0)
#define DP_AUX_PN_SWAP		(0)
#define DP_LANE_SWAP		(0)
/* (0) our convert board need to LANE SWAP for data lane*/
#define FRAME_RESYNC		(0)
#define LVDS_LANE_SWAP		(0)
#define LVDS_PN_SWAP		(0)
#define LVDS_DC_BALANCE		(0)

#define LVDS_6BIT		(0) /* '0' for 8 bit, '1' for 6 bit */
#define VESA_MAP		(1) /* '0' for JEIDA , '1' for VESA MAP */

#define INT_MASK			(3)
#define MIPI_INT_MASK		(0)
#define TIMER_CNT			(0x0A)

#ifdef PANEL_RESOLUTION_1280x800_NOUFO
#define PANEL_WIDTH 1280
#define VIC 0
#define MP_HPOL 0
#define MP_VPOL 1
#define DPTX_LANE_COUNT  B_2_LANE
#define MIPI_LANE_COUNT  MIPI_4_LANE
#define EN_UFO 0
#define MIPI_PACKED_FMT		RGB_24b
#define MP_H_RESYNC			1
#define MP_V_RESYNC			0
#endif

#ifdef PANEL_RESOLUTION_1920x1080p60
#define PANEL_WIDTH 1920
#define VIC 0x10
#define MP_HPOL 1
#define MP_VPOL 1
#define DPTX_LANE_COUNT  B_2_LANE
#define MIPI_LANE_COUNT  MIPI_4_LANE
#define EN_UFO 0
#define MIPI_PACKED_FMT		RGB_24b
#define MP_H_RESYNC			1
#define MP_V_RESYNC			0
#endif

#ifdef PANEL_RESOLUTION_1920x1200p60RB
#define PANEL_WIDTH 1920
#define VIC 0
#define MP_HPOL 1
#define MP_VPOL 0
#define DPTX_LANE_COUNT  B_2_LANE
#define MIPI_LANE_COUNT  MIPI_4_LANE
#define EN_UFO 0
#define MIPI_PACKED_FMT		RGB_24b
#define MP_H_RESYNC			1
#define MP_V_RESYNC			0
#endif

#ifdef PANEL_RESOLUTION_2048x1536
#define PANEL_WIDTH 2048
#define VIC 0
#define MP_HPOL 0
#define MP_VPOL 1
#define MIPI_LANE_COUNT  MIPI_4_LANE
#define DPTX_LANE_COUNT  B_4_LANE
#define EN_UFO 1
#define MIPI_PACKED_FMT		RGB_24b
#define MP_H_RESYNC			0
#define MP_V_RESYNC			0
#endif

#ifdef PANEL_RESOLUTION_2048x1536_NOUFO
#define PANEL_WIDTH 2048
#define VIC 0
#define MP_HPOL 0
#define MP_VPOL 1
#define MIPI_LANE_COUNT  MIPI_4_LANE
#define DPTX_LANE_COUNT  B_4_LANE
#define EN_UFO 0
#define MIPI_PACKED_FMT		RGB_24b
#define MP_H_RESYNC			1
#define MP_V_RESYNC			0
#endif

#ifdef PANEL_RESOLUTION_2048x1536_NOUFO_18B
#define PANEL_WIDTH 2048
#define VIC 0
#define MP_HPOL 0
#define MP_VPOL 1
#define MIPI_LANE_COUNT  MIPI_4_LANE
#define DPTX_LANE_COUNT  B_4_LANE
#define EN_UFO 0
#define MIPI_PACKED_FMT		RGB_18b_P
#define MP_H_RESYNC			1
#define MP_V_RESYNC			0
#endif

#ifdef PANEL_RESULUTION_1536x2048
#define PANEL_WIDTH 1368
#define VIC 0
#define MP_HPOL 0
#define MP_VPOL 1
#define MIPI_LANE_COUNT  MIPI_4_LANE
#define DPTX_LANE_COUNT  B_1_LANE/*8B_1_LANE*/
#define EN_UFO 0
#define MIPI_PACKED_FMT		RGB_24b
#define MP_H_RESYNC			1
#define MP_V_RESYNC			0
#define EN_VLC					0
#define VLC_CFG_H			0  /*bit7~4*/
#define VLC_CFG_L			0     /*//0xB        //bit3~0*/
#endif

#define DP_I2C_ADDR 0x5C
#define MIPI_I2C_ADDR 0x6C

struct i2c_client *it6151_mipirx;
struct i2c_client *it6151_dptx;

static u8 it6151_reg_i2c_read_byte(struct it6151_bridge *ite_bridge,
	u8 dev_address, u8 addr, u8 *dataBuffer)
{
	int ret = 0;

if (dev_address == ite_bridge->mipirx_client->addr) {
		ret = i2c_master_send(ite_bridge->mipirx_client, &addr, 1);

		if (ret <= 0) {
			DRM_ERROR("Failed to send i2c command, ret=%d\n", ret);
			return ret;
		}

		ret = i2c_master_recv(ite_bridge->mipirx_client, dataBuffer, 1);
		if (ret <= 0) {
			DRM_ERROR("Failed to recv i2c data, ret=%d\n", ret);
			return ret;
		}
	} else if (dev_address == ite_bridge->dptx_client->addr) {
		ret = i2c_master_send(ite_bridge->dptx_client, &addr, 1);
		if (ret <= 0) {
			DRM_ERROR("Failed to send i2c command, ret=%d\n", ret);
			return ret;
		}

		ret = i2c_master_recv(ite_bridge->dptx_client, dataBuffer, 1);
		if (ret <= 0) {
			DRM_ERROR("Failed to recv i2c data, ret=%d\n", ret);
			return ret;
		}
	}
	return ret;
}
void it6151_reg_i2c_write_byte(struct it6151_bridge *ite_bridge,
	u8 dev_address, u8 reg_address, u8 reg_val)
{
	u8 write_data[2];

	write_data[0] = reg_address;
	write_data[1] = reg_val;

	if (dev_address == ite_bridge->mipirx_client->addr) {
		i2c_master_send(ite_bridge->mipirx_client,
			write_data, ARRAY_SIZE(write_data));
	} else if (dev_address == ite_bridge->dptx_client->addr) {
		i2c_master_send(ite_bridge->dptx_client,
			write_data, ARRAY_SIZE(write_data));
	}
}

static const struct drm_display_mode it6151_drm_default_modes[] = {
	/* 1368x768@60Hz */
	{ DRM_MODE("1368x768", DRM_MODE_TYPE_DRIVER, 72070,
	1368, 1368 + 58, 1368 + 58 + 58, 1368 + 58 + 58 + 58, 0,
	768, 768 + 4, 768 + 4 + 4, 768 + 4 + 4 + 4, 0, 0) },
};

static int it6151_get_modes(struct drm_connector *connector)
{
	const struct drm_display_mode *ptr = &it6151_drm_default_modes[0];
	struct drm_display_mode *mode;
	int count = 0;

	pr_info("%s !\n", __func__);

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

struct drm_encoder *it6151_best_encoder(struct drm_connector *connector)
{
	struct it6151_bridge *ite_bridge;

	ite_bridge = container_of(connector, struct it6151_bridge, connector);
	return ite_bridge->encoder;
}

void IT6151_DPTX_init(struct it6151_bridge *ite_bridge)
{

	it6151_reg_i2c_write_byte(ite_bridge, DP_I2C_ADDR, 0x05, 0x29);
	it6151_reg_i2c_write_byte(ite_bridge, DP_I2C_ADDR, 0x05, 0x00);
	it6151_reg_i2c_write_byte(ite_bridge, DP_I2C_ADDR, 0x09, INT_MASK);
	/* Enable HPD_IRQ,HPD_CHG,VIDSTABLE*/
	it6151_reg_i2c_write_byte(ite_bridge, DP_I2C_ADDR, 0x0A, 0x00);
	it6151_reg_i2c_write_byte(ite_bridge, DP_I2C_ADDR, 0x0B, 0x00);
	it6151_reg_i2c_write_byte(ite_bridge, DP_I2C_ADDR, 0xC5, 0xC1);
	it6151_reg_i2c_write_byte(ite_bridge, DP_I2C_ADDR, 0xB5, 0x00);
	it6151_reg_i2c_write_byte(ite_bridge, DP_I2C_ADDR, 0xB7, 0x80);
	it6151_reg_i2c_write_byte(ite_bridge, DP_I2C_ADDR, 0xC4, 0xF0);
	it6151_reg_i2c_write_byte(ite_bridge, DP_I2C_ADDR, 0x06, 0xFF);
	it6151_reg_i2c_write_byte(ite_bridge, DP_I2C_ADDR, 0x07, 0xFF);
	it6151_reg_i2c_write_byte(ite_bridge, DP_I2C_ADDR, 0x08, 0xFF);
	it6151_reg_i2c_write_byte(ite_bridge, DP_I2C_ADDR, 0x05, 0x00);
	it6151_reg_i2c_write_byte(ite_bridge, DP_I2C_ADDR, 0x0c, 0x08);
	it6151_reg_i2c_write_byte(ite_bridge, DP_I2C_ADDR, 0x21, 0x05);
	it6151_reg_i2c_write_byte(ite_bridge, DP_I2C_ADDR, 0x3a, 0x04);
	it6151_reg_i2c_write_byte(ite_bridge, DP_I2C_ADDR, 0x5f, 0x06);
	/*    {DP_I2C_ADDR,0xb5,0xFF,0x80},*/
	it6151_reg_i2c_write_byte(ite_bridge, DP_I2C_ADDR, 0xc9, 0xf5);
	it6151_reg_i2c_write_byte(ite_bridge, DP_I2C_ADDR, 0xca, 0x4c);
	it6151_reg_i2c_write_byte(ite_bridge, DP_I2C_ADDR, 0xcb, 0x37);
	it6151_reg_i2c_write_byte(ite_bridge, DP_I2C_ADDR, 0xce, 0x80);
	it6151_reg_i2c_write_byte(ite_bridge, DP_I2C_ADDR, 0xd3, 0x03);
	it6151_reg_i2c_write_byte(ite_bridge, DP_I2C_ADDR, 0xd4, 0x60);
	it6151_reg_i2c_write_byte(ite_bridge, DP_I2C_ADDR, 0xe8, 0x11);
	it6151_reg_i2c_write_byte(ite_bridge, DP_I2C_ADDR, 0xec, VIC);
	mdelay(5);
	it6151_reg_i2c_write_byte(ite_bridge, DP_I2C_ADDR, 0x23, 0x42);
	it6151_reg_i2c_write_byte(ite_bridge, DP_I2C_ADDR, 0x24, 0x07);
	it6151_reg_i2c_write_byte(ite_bridge, DP_I2C_ADDR, 0x25, 0x01);
	it6151_reg_i2c_write_byte(ite_bridge, DP_I2C_ADDR, 0x26, 0x00);
	it6151_reg_i2c_write_byte(ite_bridge, DP_I2C_ADDR, 0x27, 0x10);
	it6151_reg_i2c_write_byte(ite_bridge, DP_I2C_ADDR, 0x2B, 0x05);
	it6151_reg_i2c_write_byte(ite_bridge, DP_I2C_ADDR, 0x23, 0x40);
	it6151_reg_i2c_write_byte(ite_bridge, DP_I2C_ADDR, 0x22,
		(DP_AUX_PN_SWAP<<3)|(DP_PN_SWAP<<2)|0x03);
	it6151_reg_i2c_write_byte(ite_bridge, DP_I2C_ADDR, 0x16,
		(DPTX_SSC_SETTING<<4)|(DP_LANE_SWAP<<3)|(DPTX_LANE_COUNT<<1)|
		TRAINING_BITRATE);
	it6151_reg_i2c_write_byte(ite_bridge, DP_I2C_ADDR, 0x0f, 0x01);
	it6151_reg_i2c_write_byte(ite_bridge, DP_I2C_ADDR, 0x76, 0xa7);
	it6151_reg_i2c_write_byte(ite_bridge, DP_I2C_ADDR, 0x77, 0xaf);
	it6151_reg_i2c_write_byte(ite_bridge, DP_I2C_ADDR, 0x7e, 0x8f);
	it6151_reg_i2c_write_byte(ite_bridge, DP_I2C_ADDR, 0x7f, 0x07);
	it6151_reg_i2c_write_byte(ite_bridge, DP_I2C_ADDR, 0x80, 0xef);
	it6151_reg_i2c_write_byte(ite_bridge, DP_I2C_ADDR, 0x81, 0x5f);
	it6151_reg_i2c_write_byte(ite_bridge, DP_I2C_ADDR, 0x82, 0xef);
	it6151_reg_i2c_write_byte(ite_bridge, DP_I2C_ADDR, 0x83, 0x07);
	it6151_reg_i2c_write_byte(ite_bridge, DP_I2C_ADDR, 0x88, 0x38);
	it6151_reg_i2c_write_byte(ite_bridge, DP_I2C_ADDR, 0x89, 0x1f);
	it6151_reg_i2c_write_byte(ite_bridge, DP_I2C_ADDR, 0x8a, 0x48);
	it6151_reg_i2c_write_byte(ite_bridge, DP_I2C_ADDR, 0x0f, 0x00);
	it6151_reg_i2c_write_byte(ite_bridge, DP_I2C_ADDR, 0x5c, 0xf3);
	it6151_reg_i2c_write_byte(ite_bridge, DP_I2C_ADDR, 0x17, 0x04);
	it6151_reg_i2c_write_byte(ite_bridge, DP_I2C_ADDR, 0x17, 0x01);
	mdelay(5);
}


int	it6151_bdg_enable(struct it6151_bridge *ite_bridge)
{
	unsigned char VenID[2], DevID[2], RevID;
	unsigned char reg_off;

	reg_off = 0x00;
	it6151_reg_i2c_read_byte(ite_bridge, DP_I2C_ADDR, reg_off, &VenID[0]);
	reg_off = 0x01;
	it6151_reg_i2c_read_byte(ite_bridge, DP_I2C_ADDR, reg_off, &VenID[1]);
	reg_off = 0x02;
	it6151_reg_i2c_read_byte(ite_bridge, DP_I2C_ADDR, reg_off, &DevID[0]);
	reg_off = 0x03;
	it6151_reg_i2c_read_byte(ite_bridge, DP_I2C_ADDR, reg_off, &DevID[1]);
	reg_off = 0x04;
	it6151_reg_i2c_read_byte(ite_bridge, DP_I2C_ADDR, reg_off, &RevID);

	if (VenID[0] == 0x54 && VenID[1] == 0x49 &&
	DevID[0] == 0x51 && DevID[1] == 0x61) {
		it6151_reg_i2c_write_byte(ite_bridge, DP_I2C_ADDR, 0x05,
			0x04);/* DP SW Reset*/
		it6151_reg_i2c_write_byte(ite_bridge, DP_I2C_ADDR, 0xfd,
			(MIPI_I2C_ADDR<<1)|1);
		it6151_reg_i2c_write_byte(ite_bridge, MIPI_I2C_ADDR, 0x05,
			0x00);
		it6151_reg_i2c_write_byte(ite_bridge, MIPI_I2C_ADDR, 0x0c,
			(MP_LANE_SWAP<<7)|(MP_PN_SWAP<<6)|(MIPI_LANE_COUNT<<4)|
			EN_UFO);
		it6151_reg_i2c_write_byte(ite_bridge,
			MIPI_I2C_ADDR, 0x11, MP_MCLK_INV);

		if (RevID == 0xA1) {
			it6151_reg_i2c_write_byte(ite_bridge, MIPI_I2C_ADDR,
			0x19, MP_LANE_DESKEW);
		} else {
			it6151_reg_i2c_write_byte(ite_bridge, MIPI_I2C_ADDR,
			0x19, (MP_CONTINUOUS_CLK << 1) | MP_LANE_DESKEW);
		}
	/*it6151_reg_i2c_write_byte(MIPI_I2C_ADDR,0x19,0x01);*/
	it6151_reg_i2c_write_byte(ite_bridge, MIPI_I2C_ADDR, 0x27,
		MIPI_PACKED_FMT);
	it6151_reg_i2c_write_byte(ite_bridge, MIPI_I2C_ADDR, 0x28,
		((PANEL_WIDTH/4-1)>>2)&0xC0);
	it6151_reg_i2c_write_byte(ite_bridge, MIPI_I2C_ADDR, 0x29,
		(PANEL_WIDTH/4-1)&0xFF);
	it6151_reg_i2c_write_byte(ite_bridge, MIPI_I2C_ADDR, 0x2e, 0x34);
	it6151_reg_i2c_write_byte(ite_bridge, MIPI_I2C_ADDR, 0x2f, 0x01);
	it6151_reg_i2c_write_byte(ite_bridge, MIPI_I2C_ADDR, 0x4e,
		(MP_V_RESYNC<<3)|(MP_H_RESYNC<<2)|(MP_VPOL<<1)|(MP_HPOL));
	it6151_reg_i2c_write_byte(ite_bridge, MIPI_I2C_ADDR, 0x80,
		(EN_UFO<<5)|MP_PCLK_DIV);
	it6151_reg_i2c_write_byte(ite_bridge, MIPI_I2C_ADDR, 0x84, 0x8f);
	it6151_reg_i2c_write_byte(ite_bridge, MIPI_I2C_ADDR, 0x09,
		MIPI_INT_MASK);
	it6151_reg_i2c_write_byte(ite_bridge, MIPI_I2C_ADDR, 0x92, TIMER_CNT);
	IT6151_DPTX_init(ite_bridge);


	return 0;
	}
	return -1;
}

void it6151_pre_enable(struct drm_bridge *bridge)
{
	struct it6151_bridge *ite_bridge = bridge->driver_private;
	int ret;


	pr_info("%s !\n", __func__);

	if (ite_bridge->enabled)
		return;

	if (ite_bridge->gpio_pwr12_en_n) {
		ret = gpio_direction_output(ite_bridge->gpio_pwr12_en_n, 1);
		if (ret < 0) {
			DRM_ERROR("set it6151-pwr-gpio failed (%d)\n", ret);
			return;
		}
	}


	if (ite_bridge->gpio_rst_n) {
		ret = gpio_direction_output(ite_bridge->gpio_rst_n, 1);
		if (ret < 0) {
			DRM_ERROR("set it6151-reset-gpio failed (%d)\n", ret);
			return;
		}
	}

	ret = it6151_bdg_enable(ite_bridge);


	ite_bridge->enabled = true;
}
static void it6151_enable(struct drm_bridge *bridge)
{

}
static void it6151_disable(struct drm_bridge *bridge)
{
	struct it6151_bridge *ite_bridge = bridge->driver_private;
	int ret;

	pr_info("%s !\n", __func__);

	return;

	if (!ite_bridge->enabled)
		return;

	ite_bridge->enabled = false;

	if (ite_bridge->gpio_pwr12_en_n) {
		ret = gpio_direction_output(ite_bridge->gpio_pwr12_en_n, 0);
		if (ret < 0) {
			DRM_ERROR("set it6151-pwr-gpio failed (%d)\n", ret);
			return;
		}
	}

	if (ite_bridge->gpio_rst_n) {
		ret = gpio_direction_output(ite_bridge->gpio_rst_n, 0);
		if (ret < 0) {
			DRM_ERROR("set it6151-reset-gpio failed (%d)\n", ret);
			return;
		}
	}
}

static void it6151_post_disable(struct drm_bridge *bridge)
{
}
enum drm_connector_status it6151_detect(struct drm_connector *connector,
		bool force)
{
	return connector_status_connected;
}
void it6151_connector_destroy(struct drm_connector *connector)
{
	drm_connector_cleanup(connector);
}
struct drm_connector_funcs it6151_connector_funcs = {
	.dpms = drm_helper_connector_dpms,
	.fill_modes = drm_helper_probe_single_connector_modes,
	.detect = it6151_detect,
	.destroy = it6151_connector_destroy,
};
struct drm_connector_helper_funcs it6151_connector_helper_funcs = {
	.get_modes = it6151_get_modes,
	.mode_valid = it6151_mode_valid,
	.best_encoder = it6151_best_encoder,
};
struct drm_bridge_funcs it6151_bridge_funcs = {
	.pre_enable = it6151_pre_enable,
	.enable = it6151_enable,
	.disable = it6151_disable,
	.post_disable = it6151_post_disable,
	/*.destroy = it6151_bridge_destroy,*/
};

int it6151_init(struct drm_device *dev, struct drm_encoder *encoder,
		struct i2c_client *mipirx_client,
		struct i2c_client *dptx_client, struct device_node *node)
{
	int ret;
	struct drm_bridge *bridge;
	struct it6151_bridge *ite_bridge;

	pr_info("%s !\n", __func__);

	bridge = devm_kzalloc(dev->dev, sizeof(*bridge), GFP_KERNEL);
	if (!bridge)
		return -ENOMEM;


	ite_bridge = devm_kzalloc(dev->dev, sizeof(*ite_bridge), GFP_KERNEL);
	if (!ite_bridge)
		return -ENOMEM;


/*
	ite_bridge->supplies = devm_regulator_get(dev->dev, "disp-bdg-supply");
	if (IS_ERR(ite_bridge->supplies)) {
		printk( "cannot get vgp2\n");
		return -ENOMEM;
	}

	ret = regulator_set_voltage(ite_bridge->supplies, 1800000, 1800000);
	if (ret != 0)	{
		printk("lcm failed to set lcm_vgp voltage: %d\n", ret);
		return ret;
	}

	ret = regulator_enable(ite_bridge->supplies);
	if (ret != 0)
	{
		printk("Failed to enable lcm_vgp: %d\n", ret);
		return ret;
	}
*/

	ite_bridge->mipirx_client = mipirx_client;
	ite_bridge->dptx_client = dptx_client;

	ite_bridge->encoder = encoder;
	ite_bridge->bridge = bridge;

	ite_bridge->gpio_pwr12_en_n = of_get_named_gpio(node, "bdgpwr-gpio", 0);
	ite_bridge->gpio_rst_n = of_get_named_gpio(node, "bdgrst-gpio", 0);

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

	ite_bridge->connector.dpms = DRM_MODE_DPMS_OFF;
	ite_bridge->connector.encoder = encoder;


	drm_mode_connector_attach_encoder(&ite_bridge->connector, encoder);

	/*it6151_pre_enable(bridge);*/


	return 0;

err:

	if (gpio_is_valid(ite_bridge->gpio_pwr12_en_n))
		gpio_free(ite_bridge->gpio_pwr12_en_n);

	if (gpio_is_valid(ite_bridge->gpio_rst_n))
		gpio_free(ite_bridge->gpio_rst_n);

	return ret;
}


static int it6151mipirx_i2c_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{

	int ret = 0;
	int err = 0;

	it6151_mipirx = kmalloc(sizeof(struct i2c_client), GFP_KERNEL);
	if (!it6151_mipirx) {
		err = -ENOMEM;
		goto exit;
	}
	memset(it6151_mipirx, 0, sizeof(struct i2c_client));
	it6151_mipirx = client;

	return ret;

exit:
	return err;

}


static int it6151dptx_i2c_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{

	int ret = 0;
	int err = 0;

	it6151_dptx = kmalloc(sizeof(struct i2c_client), GFP_KERNEL);
	if (!it6151_dptx) {
		err = -ENOMEM;
		goto exit;
	}
	memset(it6151_dptx, 0, sizeof(struct i2c_client));
	it6151_dptx = client;

	return ret;

exit:
	return err;

}

static int it6151mipirx_remove(struct i2c_client *client)
{
	return 0;
}


static int it6151dptx_remove(struct i2c_client *client)
{
	return 0;
}

static const struct of_device_id it6151mipirx_of_match[] = {
	{ .compatible = "ite,it6151mipirx"},
	{ },
};

static const struct i2c_device_id it6151_mipirx_id[] = {
	{ "ite,it6151mipirx", 0 }
};

static const struct i2c_device_id it6151_dptx_id[] = {
	{ "ite,it6151dptx", 0 }
};

struct i2c_driver it6151mipirx_i2c_driver = {
	.driver = {
		.name = "it6151_mipirx",
		.owner = THIS_MODULE,
		.of_match_table = it6151mipirx_of_match,
	},
	.probe = it6151mipirx_i2c_probe,
	.remove = it6151mipirx_remove,
	.id_table = it6151_mipirx_id,
};



static const struct of_device_id it6151dptx_of_match[] = {
	{ .compatible = "ite,it6151dptx"},
	{ },
};


struct i2c_driver it6151dptx_i2c_driver = {
	.driver = {
		.name = "it6151_dptx",
		.owner = THIS_MODULE,
		.of_match_table = it6151dptx_of_match,
	},
	.probe = it6151dptx_i2c_probe,
	.remove = it6151dptx_remove,
	.id_table = it6151_dptx_id,
};

/*
static int  it6151_i2c_init(void)
{

	if(i2c_add_driver(&it6151mipirx_i2c_driver)!=0)
	{
		printk("[it6151_i2c_init] it6151mipirx_i2c_driver failed.\n");
	}
	else
	{
		printk("[it6151_i2c_init] it6151mipirx_i2c_driver success.\n");
	}

	if(i2c_add_driver(&it6151dptx_i2c_driver)!=0)
	{
		printk("[it6151_i2c_init] it6151dptx_i2c_driver failed.\n");
	}
	else
	{
		printk("[it6151_i2c_init] it6151dptx_i2c_driver success.\n");
	}

return 0;
}
*/
