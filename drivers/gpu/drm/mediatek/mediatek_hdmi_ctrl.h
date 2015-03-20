
#ifndef _MEDIATEK_HDMI_CTRL_H
#define _MEDIATEK_HDMI_CTRL_H

#include <linux/types.h>
#include <linux/bug.h>
#include <linux/kernel.h>
#include <linux/hdmi.h>
#include <linux/clk.h>
#include <drm/drm_crtc.h>
#include <linux/regmap.h>
#include "mediatek_hdmi_display_core.h"

enum hdmi_aud_input_type {
	HDMI_AUD_INPUT_I2S = 0,
	HDMI_AUD_INPUT_SPDIF,
};

enum hdmi_aud_i2s_fmt {
	HDMI_I2S_MODE_RJT_24BIT = 0,
	HDMI_I2S_MODE_RJT_16BIT,
	HDMI_I2S_MODE_LJT_24BIT,
	HDMI_I2S_MODE_LJT_16BIT,
	HDMI_I2S_MODE_I2S_24BIT,
	HDMI_I2S_MODE_I2S_16BIT
};

enum hdmi_aud_mclk {
	HDMI_AUD_MCLK_128FS,
	HDMI_AUD_MCLK_192FS,
	HDMI_AUD_MCLK_256FS,
	HDMI_AUD_MCLK_384FS,
	HDMI_AUD_MCLK_512FS,
	HDMI_AUD_MCLK_768FS,
	HDMI_AUD_MCLK_1152FS,
};

enum hdmi_aud_iec_frame_rate {
	HDMI_IEC_32K = 0,
	HDMI_IEC_96K,
	HDMI_IEC_192K,
	HDMI_IEC_768K,
	HDMI_IEC_44K,
	HDMI_IEC_88K,
	HDMI_IEC_176K,
	HDMI_IEC_705K,
	HDMI_IEC_16K,
	HDMI_IEC_22K,
	HDMI_IEC_24K,
	HDMI_IEC_48K,
};

enum hdmi_aud_channel_type {
	HDMI_AUD_CHAN_TYPE_1_0 = 0,
	HDMI_AUD_CHAN_TYPE_1_1,
	HDMI_AUD_CHAN_TYPE_2_0,
	HDMI_AUD_CHAN_TYPE_2_1,
	HDMI_AUD_CHAN_TYPE_3_0,
	HDMI_AUD_CHAN_TYPE_3_1,
	HDMI_AUD_CHAN_TYPE_4_0,
	HDMI_AUD_CHAN_TYPE_4_1,
	HDMI_AUD_CHAN_TYPE_5_0,
	HDMI_AUD_CHAN_TYPE_5_1,
	HDMI_AUD_CHAN_TYPE_6_0,
	HDMI_AUD_CHAN_TYPE_6_1,
	HDMI_AUD_CHAN_TYPE_7_0,
	HDMI_AUD_CHAN_TYPE_7_1,
	HDMI_AUD_CHAN_TYPE_3_0_LRS,
	HDMI_AUD_CHAN_TYPE_3_1_LRS,
	HDMI_AUD_CHAN_TYPE_4_0_CLRS,
	HDMI_AUD_CHAN_TYPE_4_1_CLRS,
	HDMI_AUD_CHAN_TYPE_6_1_CS,
	HDMI_AUD_CHAN_TYPE_6_1_CH,
	HDMI_AUD_CHAN_TYPE_6_1_OH,
	HDMI_AUD_CHAN_TYPE_6_1_CHR,
	HDMI_AUD_CHAN_TYPE_7_1_LH_RH,
	HDMI_AUD_CHAN_TYPE_7_1_LSR_RSR,
	HDMI_AUD_CHAN_TYPE_7_1_LC_RC,
	HDMI_AUD_CHAN_TYPE_7_1_LW_RW,
	HDMI_AUD_CHAN_TYPE_7_1_LSD_RSD,
	HDMI_AUD_CHAN_TYPE_7_1_LSS_RSS,
	HDMI_AUD_CHAN_TYPE_7_1_LHS_RHS,
	HDMI_AUD_CHAN_TYPE_7_1_CS_CH,
	HDMI_AUD_CHAN_TYPE_7_1_CS_OH,
	HDMI_AUD_CHAN_TYPE_7_1_CS_CHR,
	HDMI_AUD_CHAN_TYPE_7_1_CH_OH,
	HDMI_AUD_CHAN_TYPE_7_1_CH_CHR,
	HDMI_AUD_CHAN_TYPE_7_1_OH_CHR,
	HDMI_AUD_CHAN_TYPE_7_1_LSS_RSS_LSR_RSR,
	HDMI_AUD_CHAN_TYPE_6_0_CS,
	HDMI_AUD_CHAN_TYPE_6_0_CH,
	HDMI_AUD_CHAN_TYPE_6_0_OH,
	HDMI_AUD_CHAN_TYPE_6_0_CHR,
	HDMI_AUD_CHAN_TYPE_7_0_LH_RH,
	HDMI_AUD_CHAN_TYPE_7_0_LSR_RSR,
	HDMI_AUD_CHAN_TYPE_7_0_LC_RC,
	HDMI_AUD_CHAN_TYPE_7_0_LW_RW,
	HDMI_AUD_CHAN_TYPE_7_0_LSD_RSD,
	HDMI_AUD_CHAN_TYPE_7_0_LSS_RSS,
	HDMI_AUD_CHAN_TYPE_7_0_LHS_RHS,
	HDMI_AUD_CHAN_TYPE_7_0_CS_CH,
	HDMI_AUD_CHAN_TYPE_7_0_CS_OH,
	HDMI_AUD_CHAN_TYPE_7_0_CS_CHR,
	HDMI_AUD_CHAN_TYPE_7_0_CH_OH,
	HDMI_AUD_CHAN_TYPE_7_0_CH_CHR,
	HDMI_AUD_CHAN_TYPE_7_0_OH_CHR,
	HDMI_AUD_CHAN_TYPE_7_0_LSS_RSS_LSR_RSR,
	HDMI_AUD_CHAN_TYPE_8_0_LH_RH_CS,
	HDMI_AUD_CHAN_TYPE_UNKNOWN = 0xFF
};

enum hdmi_aud_channel_swap_type {
	HDMI_AUD_SWAP_LR,
	HDMI_AUD_SWAP_LFE_CC,
	HDMI_AUD_SWAP_LSRS,
	HDMI_AUD_SWAP_RLS_RRS,
	HDMI_AUD_SWAP_LR_STATUS,
};

struct mediatek_hdmi_context {
	struct drm_device *drm_dev;
	struct drm_encoder encoder;
	struct drm_connector conn;
	struct i2c_adapter *ddc_adpt;
	struct task_struct *kthread;
	struct clk *tvd_clk;
	struct clk *dpi_clk_mux;
	struct clk *dpi_clk_div2;
	struct clk *dpi_clk_div4;
	struct clk *dpi_clk_div8;
	struct clk *hdmitx_clk_mux;
	struct clk *hdmitx_clk_div1;
	struct clk *hdmitx_clk_div2;
	struct clk *hdmitx_clk_div3;
	struct clk *hdmi_id_clk_gate;
	struct clk *hdmi_pll_clk_gate;
	struct clk *hdmi_dpi_clk_gate;
	struct clk *hdmi_aud_bclk_gate;
	struct clk *hdmi_aud_spdif_gate;
	#if defined(CONFIG_DEBUG_FS)
	struct dentry *debugfs;
	#endif
	struct meidatek_hdmi_display_node *display_node;
	int en_5v_gpio;
	int htplg_gpio;
	int flt_n_5v_gpio;
	int flt_n_5v_irq;
	int hpd;
	void __iomem *sys_regs;
	void __iomem *grl_regs;
	void __iomem *pll_regs;
	void __iomem *cec_regs;
	bool init;
	enum HDMI_DISPLAY_COLOR_DEPTH depth;
	enum hdmi_colorspace csp;
	bool aud_mute;
	bool vid_black;
	bool output;
	enum hdmi_audio_coding_type aud_codec;
	enum hdmi_audio_sample_frequency aud_hdmi_fs;
	enum hdmi_audio_sample_size aud_sampe_size;
	enum hdmi_aud_input_type aud_input_type;
	enum hdmi_aud_i2s_fmt aud_i2s_fmt;
	enum hdmi_aud_mclk aud_mclk;
	enum hdmi_aud_iec_frame_rate iec_frame_fs;
	enum hdmi_aud_channel_type aud_input_chan_type;
	u8 hdmi_l_channel_state[6];
	u8 hdmi_r_channel_state[6];
	struct mutex hdmi_mutex;/*to do */
};

#define hdmi_ctx_from_encoder(e)	\
	container_of(e, struct mediatek_hdmi_context, encoder)
#define hdmi_ctx_from_conn(e)	\
	container_of(e, struct mediatek_hdmi_context, conn)
bool mtk_hdmi_output_init(void *private_data);
bool mtk_hdmi_hpd_high(struct mediatek_hdmi_context *hdmi_context);
bool mtk_hdmi_signal_on(struct mediatek_hdmi_context *hdmi_context);
bool mtk_hdmi_signal_off(struct mediatek_hdmi_context *hdmi_context);
bool mtk_hdmi_output_set_display_mode(struct drm_display_mode *display_mode,
				      void *private_data);

int mtk_drm_hdmi_debugfs_init(struct mediatek_hdmi_context *hdmi_context);
void mtk_drm_hdmi_debugfs_exit(struct mediatek_hdmi_context *hdmi_context);



#endif /*  */
