#include <drm/drmP.h>
#include <linux/clk.h>

#include "mediatek_drm_crtc.h"
#include "mediatek_drm_hw.h"

/* These are locked by dev->vbl_lock */
void mtk_enable_vblank(void __iomem *disp_base)
{
	writel(0x4, disp_base + DISP_REG_RDMA_INT_ENABLE);
}

void mtk_disable_vblank(void __iomem *disp_base)
{
	writel(0x0, disp_base + DISP_REG_RDMA_INT_ENABLE);
}

void mtk_clear_vblank(void __iomem *disp_base)
{
	writel(0x0, disp_base + DISP_REG_RDMA_INT_STATUS);
}

static void DispConfigMainPathConnection(void __iomem *disp_base)
{
	/* OVL output to BLS */
	writel(0x2, disp_base + DISP_REG_CONFIG_OVL_MOUT_EN);

	/* BLS input from  OVL */
	writel(0x0, disp_base + DISP_REG_CONFIG_BLS_SEL);

	/* RDMA0 output to DSI */
	writel(0x0, disp_base + DISP_REG_CONFIG_RDMA0_OUT_SEL);
}

static void DispConfigMainPathMutex(void __iomem *mutex_base)
{
	unsigned int ID = 0;

	/* Module: OVL=2, RDMA0=7 */
	writel((1 << 2 | 1 << 7), mutex_base + DISP_REG_CONFIG_MUTEX_MOD(ID));

	/* Clock source from DSI */
	writel(1, mutex_base + DISP_REG_CONFIG_MUTEX_SOF(ID));
}

static int disp_path_get_mutex(void __iomem *mutex_base)
{
	unsigned int ID = 0;
	unsigned int cnt = 0;
	unsigned long reg;

	writel(0x1, mutex_base + DISP_REG_CONFIG_MUTEX_EN(ID));
	writel(0x1, mutex_base + DISP_REG_CONFIG_MUTEX(ID));
	reg = readl(mutex_base + DISP_REG_CONFIG_MUTEX(ID));
	while (!(reg & DISP_INT_MUTEX_BIT_MASK)) {
		cnt++;
		if (cnt > 10000) {
			pr_err("disp_path_get_mutex() timeout!\n");
			break;
		}
		reg = readl(mutex_base + DISP_REG_CONFIG_MUTEX(ID));
	}

	return 0;
}

static void disp_path_release_mutex(void __iomem *mutex_base)
{
	unsigned int ID = 0;

	writel(0x0, mutex_base + DISP_REG_CONFIG_MUTEX(ID));
}

static void OVLStart(void __iomem *disp_base)
{
	writel(0x01, disp_base + DISP_REG_OVL_EN);
	writel(0x0f, disp_base + DISP_REG_OVL_INTEN);
}

static void OVLStop(void __iomem *disp_base)
{
	writel(0x00, disp_base + DISP_REG_OVL_INTEN);
	writel(0x00, disp_base + DISP_REG_OVL_EN);
	writel(0x00, disp_base + DISP_REG_OVL_INTSTA);
}

static void OVLROI(void __iomem *disp_base, unsigned int W, unsigned int H,
	unsigned int bgColor)
{
	writel(H << 16 | W, disp_base + DISP_REG_OVL_ROI_SIZE);
	writel(bgColor, disp_base + DISP_REG_OVL_ROI_BGCLR);
}

static void OVLLayerSwitch(void __iomem *disp_base, unsigned layer, bool en)
{
	unsigned int reg;

	reg = readl(disp_base + DISP_REG_OVL_SRC_CON);

	if (en)
		reg |= (1U<<layer);
	else
		reg &= ~(1U<<layer);

	writel(reg, disp_base + DISP_REG_OVL_SRC_CON);
}

#define RDMA_N_OFST 0x1000
static void RDMAStart(void __iomem *disp_base, unsigned idx)
{
	unsigned int reg;

	BUG_ON(idx >= 2);

	writel(0x4, disp_base + DISP_REG_RDMA_INT_ENABLE + idx * RDMA_N_OFST);
	reg = readl(disp_base + DISP_REG_RDMA_GLOBAL_CON + idx * RDMA_N_OFST);
	reg |= 1;
	writel(reg, disp_base + DISP_REG_RDMA_GLOBAL_CON + idx * RDMA_N_OFST);
}

static void RDMAStop(void __iomem *disp_base, unsigned idx)
{
	unsigned int reg;

	BUG_ON(idx >= 2);

	reg = readl(disp_base + DISP_REG_RDMA_GLOBAL_CON + idx * RDMA_N_OFST);
	reg &= ~(1U);
	writel(reg, disp_base + DISP_REG_RDMA_GLOBAL_CON + idx * RDMA_N_OFST);
	writel(0, disp_base + DISP_REG_RDMA_INT_ENABLE + idx * RDMA_N_OFST);
	writel(0, disp_base + DISP_REG_RDMA_INT_STATUS + idx * RDMA_N_OFST);
}

static void RDMAReset(void __iomem *disp_base, unsigned idx)
{
	unsigned int reg;
	unsigned int delay_cnt;

	BUG_ON(idx >= 2);

	reg = readl(disp_base + DISP_REG_RDMA_GLOBAL_CON + idx * RDMA_N_OFST);
	reg |= 0x10;
	writel(reg, disp_base + DISP_REG_RDMA_GLOBAL_CON + idx * RDMA_N_OFST);

	delay_cnt = 0;
	while ((readl(disp_base + DISP_REG_RDMA_GLOBAL_CON + idx * RDMA_N_OFST)
		& 0x700) == 0x100) {

		delay_cnt++;
		if (delay_cnt > 10000) {
			pr_err("RDMAReset timeout\n");
			break;
		}
	}

	reg = readl(disp_base + DISP_REG_RDMA_GLOBAL_CON + idx * RDMA_N_OFST);
	reg &= ~(0x10U);
	writel(reg, disp_base + DISP_REG_RDMA_GLOBAL_CON + idx * RDMA_N_OFST);

	delay_cnt = 0;
	while ((readl(disp_base + DISP_REG_RDMA_GLOBAL_CON + idx * RDMA_N_OFST)
		& 0x700) != 0x100) {

		delay_cnt++;
		if (delay_cnt > 10000) {
			pr_err("RDMAReset timeout\n");
			break;
		}
	}
}

static void RDMAConfigDirectLink(void __iomem *disp_base, unsigned idx,
	unsigned width, unsigned height)
{
	unsigned int reg;
	enum RDMA_MODE mode = RDMA_MODE_DIRECT_LINK;
	enum RDMA_OUTPUT_FORMAT outputFormat = RDMA_OUTPUT_FORMAT_ARGB;

	BUG_ON(idx >= 2);

	/* Config mode */
	reg = readl(disp_base + DISP_REG_RDMA_GLOBAL_CON + idx * RDMA_N_OFST);
	if (mode == RDMA_MODE_DIRECT_LINK)
		reg &= ~(0x2U);
	writel(reg, disp_base + DISP_REG_RDMA_GLOBAL_CON + idx * RDMA_N_OFST);

	/* Config output format */
	reg = readl(disp_base + DISP_REG_RDMA_SIZE_CON_0 + idx * RDMA_N_OFST);
	if (outputFormat == RDMA_OUTPUT_FORMAT_ARGB)
		reg &= ~(0x20000000U);
	else
		reg |= 0x20000000U;
	writel(reg, disp_base + DISP_REG_RDMA_SIZE_CON_0 + idx * RDMA_N_OFST);

	/* Config width */
	reg = readl(disp_base + DISP_REG_RDMA_SIZE_CON_0 + idx * RDMA_N_OFST);
	reg = (reg & ~(0xFFFU)) | (width & 0xFFFU);
	writel(reg, disp_base + DISP_REG_RDMA_SIZE_CON_0 + idx * RDMA_N_OFST);

	/* Config height */
	reg = readl(disp_base + DISP_REG_RDMA_SIZE_CON_1 + idx * RDMA_N_OFST);
	reg = (reg & ~(0xFFFFFU)) | (height & 0xFFFFFU);
	writel(reg, disp_base + DISP_REG_RDMA_SIZE_CON_1 + idx * RDMA_N_OFST);

	/* Config FIFO control */
	reg = 0x80F00008;
	writel(reg, disp_base + DISP_REG_RDMA_FIFO_CON + idx * RDMA_N_OFST);
}

static unsigned int ovl_fmt_convert(unsigned int fmt)
{
	switch (fmt) {
	case DRM_FORMAT_RGB888:
		return OVL_INFMT_RGB888;
	case DRM_FORMAT_RGB565:
		return OVL_INFMT_RGB565;
	case DRM_FORMAT_ARGB8888:
		return OVL_INFMT_ARGB8888;
	case DRM_FORMAT_XRGB8888:
		return OVL_INFMT_xRGB8888;
	case DRM_FORMAT_YUYV:
		return OVL_INFMT_YUYV;
	case DRM_FORMAT_UYVY:
		return OVL_INFMT_UYVY;
	case DRM_FORMAT_YVYU:
		return OVL_INFMT_YVYU;
	case DRM_FORMAT_VYUY:
		return OVL_INFMT_VYUY;
	default:
		pr_err("drm format %X is not supported\n", fmt);
		return OVL_INFMT_UNKNOWN;
	}
}

#define OVL_N_OFST 0x1000
void OVLLayerConfigCursor(struct drm_crtc *crtc, unsigned int addr,
	int x, int y)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	void __iomem *drm_disp_base = mtk_crtc->regs;
	void __iomem *mutex = mtk_crtc->mutex_regs;
	unsigned int reg;
/*	unsigned int layer = 1; */
	unsigned int width = 64;
	unsigned int src_pitch = 64 * 4;
	bool keyEn = 1;
	unsigned int key = 0xFF000000;	/* color key */
	bool aen = 1;			/* alpha enable */
	unsigned char alpha = 0xFF;
	unsigned int fmt = OVL_INFMT_ARGB8888;

	if (width + x > DISPLAY_WIDTH)
		width = DISPLAY_WIDTH - min(x, DISPLAY_WIDTH);

	disp_path_get_mutex(mutex);
	OVLStop(drm_disp_base);

	writel(0x3, drm_disp_base + DISP_REG_OVL_SRC_CON);

	writel(0x00000001, drm_disp_base + DISP_REG_OVL_RDMA1_CTRL);
	writel(0x40402020, drm_disp_base + DISP_REG_OVL_RDMA1_MEM_GMC_SETTING);
	writel(0x01000000, drm_disp_base + DISP_REG_OVL_RDMA1_FIFO_CTRL);

	reg = keyEn << 30 | fmt << 12 | aen << 8 | alpha;
	writel(reg, drm_disp_base + DISP_REG_OVL_L1_CON);
	writel(key, drm_disp_base + DISP_REG_OVL_L1_SRCKEY);
	writel(64 << 16 | width, drm_disp_base + DISP_REG_OVL_L1_SRC_SIZE);
	writel(y << 16 | x, drm_disp_base + DISP_REG_OVL_L1_OFFSET);
	writel(addr, drm_disp_base + DISP_REG_OVL_L1_ADDR);
	writel(src_pitch & 0xFFFF, drm_disp_base + DISP_REG_OVL_L1_PITCH);

	OVLStart(drm_disp_base);
	disp_path_release_mutex(mutex);
}

void OVLLayerConfig(struct drm_crtc *crtc, unsigned int addr,
	unsigned int format)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	void __iomem *drm_disp_base = mtk_crtc->regs;
	void __iomem *mutex = mtk_crtc->mutex_regs;
	unsigned int reg;
/*	unsigned int layer = 0; */
	unsigned int source = 0;	/* from memory */
	unsigned int src_x = 0;	/* ROI x offset */
	unsigned int src_y = 0;	/* ROI y offset */
	unsigned int src_pitch = DISPLAY_WIDTH * 4;
	unsigned int dst_x = 0;	/* ROI x offset */
	unsigned int dst_y = 0;	/* ROI y offset */
	unsigned int dst_w = DISPLAY_WIDTH;	/* ROT width */
	unsigned int dst_h = DISPLAY_HEIGHT;	/* ROI height */
	bool keyEn = 1;
	unsigned int key = 0xFF000000;	/* color key */
	bool aen = 0;			/* alpha enable */
	unsigned char alpha = 0x0;

	unsigned int rgb_swap, bpp;
	unsigned int fmt = ovl_fmt_convert(format);

	if (fmt == OVL_INFMT_BGR888 || fmt == OVL_INFMT_BGR565 ||
		fmt == OVL_INFMT_ABGR8888 || fmt == OVL_INFMT_PABGR8888 ||
		fmt == OVL_INFMT_xBGR8888) {
		fmt -= OVL_COLOR_BASE;
		rgb_swap = 1;
	} else {
		rgb_swap = 0;
	}

	if ((src_x & 1)) {
		switch (fmt) {
		case OVL_INFMT_YUYV:
			fmt = OVL_INFMT_YVYU;
			break;
		case OVL_INFMT_UYVY:
			fmt = OVL_INFMT_VYUY;
			break;
		case OVL_INFMT_YVYU:
			fmt = OVL_INFMT_YUYV;
			break;
		case OVL_INFMT_VYUY:
			fmt = OVL_INFMT_UYVY;
			break;
		}
	}

	switch (fmt) {
	case OVL_INFMT_ARGB8888:
	case OVL_INFMT_PARGB8888:
	case OVL_INFMT_xRGB8888:
		bpp = 4;
		break;
	case OVL_INFMT_RGB888:
	case OVL_INFMT_YUV444:
		bpp = 3;
		break;
	case OVL_INFMT_RGB565:
	case OVL_INFMT_YUYV:
	case OVL_INFMT_UYVY:
	case OVL_INFMT_YVYU:
	case OVL_INFMT_VYUY:
		bpp = 2;
		break;
	default:
		bpp = 1;	/* invalid input format */
	}

	disp_path_get_mutex(mutex);
	OVLStop(drm_disp_base);

	writel(0x3, drm_disp_base + DISP_REG_OVL_SRC_CON);

	writel(0x00000001, drm_disp_base + DISP_REG_OVL_RDMA0_CTRL);
	writel(0x40402020, drm_disp_base + DISP_REG_OVL_RDMA0_MEM_GMC_SETTING);
	writel(0x01000000, drm_disp_base + DISP_REG_OVL_RDMA0_FIFO_CTRL);

	reg = keyEn << 30 | source << 28 | rgb_swap << 25 |
			fmt << 12 | aen << 8 | alpha;
	writel(reg, drm_disp_base + DISP_REG_OVL_L0_CON);
	writel(key, drm_disp_base + DISP_REG_OVL_L0_SRCKEY);
	writel(dst_h << 16 | dst_w, drm_disp_base + DISP_REG_OVL_L0_SRC_SIZE);
	writel(dst_y << 16 | dst_x, drm_disp_base + DISP_REG_OVL_L0_OFFSET);
	reg = addr + src_x * bpp + src_y * src_pitch;
	writel(reg, drm_disp_base + DISP_REG_OVL_L0_ADDR);
	writel(src_pitch & 0xFFFF, drm_disp_base + DISP_REG_OVL_L0_PITCH);

	OVLStart(drm_disp_base);
	disp_path_release_mutex(mutex);
}

void OVLDumpRegister(void __iomem *disp_base)
{
	int i;

	pr_info("OVL Register:\n");
	for (i = 0; i < 0x300; i += 16)
		pr_info("0x%08X: %08X %08X %08X %08X\n", i,
			readl(disp_base + DISPSYS_OVL_BASE + i),
			readl(disp_base + DISPSYS_OVL_BASE + i + 4),
			readl(disp_base + DISPSYS_OVL_BASE + i + 8),
			readl(disp_base + DISPSYS_OVL_BASE + i + 12));
}

void RDMADumpRegister(void __iomem *disp_base)
{
	int i;

	pr_info("RDMA Register:\n");
	for (i = 0; i < 0x100; i += 16)
		pr_info("0x%08X: %08X %08X %08X %08X\n", i,
			readl(disp_base + DISPSYS_RDMA0_BASE + i),
			readl(disp_base + DISPSYS_RDMA0_BASE + i + 4),
			readl(disp_base + DISPSYS_RDMA0_BASE + i + 8),
			readl(disp_base + DISPSYS_RDMA0_BASE + i + 12));
}

void BLSDumpRegister(void __iomem *disp_base)
{
	int i;

	pr_info("BLS Register:\n");
	for (i = 0; i < 0x100; i += 16)
		pr_info("0x%08X: %08X %08X %08X %08X\n", i,
			readl(disp_base + DISPSYS_BLS_BASE + i),
			readl(disp_base + DISPSYS_BLS_BASE + i + 4),
			readl(disp_base + DISPSYS_BLS_BASE + i + 8),
			readl(disp_base + DISPSYS_BLS_BASE + i + 12));
}

void DispConfigDumpRegister(void __iomem *disp_base)
{
	int i;

	pr_info("Disp Config Register:\n");
	for (i = 0; i < 0x180; i += 16)
		pr_info("0x%08X: %08X %08X %08X %08X\n", i,
			readl(disp_base + DISPSYS_CONFIG_BASE + i),
			readl(disp_base + DISPSYS_CONFIG_BASE + i + 4),
			readl(disp_base + DISPSYS_CONFIG_BASE + i + 8),
			readl(disp_base + DISPSYS_CONFIG_BASE + i + 12));
}

void MutexDumpRegister(void __iomem *mutex_base)
{
	int i;

	pr_info("Mutex Register:\n");
	for (i = 0; i < 0x100; i += 16)
		pr_info("0x%08X: %08X %08X %08X %08X\n", i,
			readl(mutex_base + DISPSYS_MUTEX_BASE + i),
			readl(mutex_base + DISPSYS_MUTEX_BASE + i + 4),
			readl(mutex_base + DISPSYS_MUTEX_BASE + i + 8),
			readl(mutex_base + DISPSYS_MUTEX_BASE + i + 12));
}

void MainDispPathPowerOn(struct drm_crtc *crtc)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	void __iomem *drm_disp_base = mtk_crtc->regs;
	void __iomem *mutex = mtk_crtc->mutex_regs;

	disp_path_get_mutex(mutex);

	OVLStart(drm_disp_base);
	RDMAStart(drm_disp_base, 0);

	disp_path_release_mutex(mutex);
}

void MainDispPathPowerOff(struct drm_crtc *crtc)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	void __iomem *drm_disp_base = mtk_crtc->regs;
	void __iomem *mutex = mtk_crtc->mutex_regs;

	disp_path_get_mutex(mutex);

	OVLStop(drm_disp_base);
	RDMAStop(drm_disp_base, 0);
	RDMAReset(drm_disp_base, 0);

	disp_path_release_mutex(mutex);

	DispConfigDumpRegister(drm_disp_base);
}

void MainDispPathSetup(struct drm_crtc *crtc)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	void __iomem *drm_disp_base = mtk_crtc->regs;
	void __iomem *mutex = mtk_crtc->mutex_regs;
	unsigned int width = DISPLAY_WIDTH;
	unsigned int height = DISPLAY_HEIGHT;

	/* Setup OVL */
	OVLROI(drm_disp_base, width, height, 0x00000000);
	OVLLayerSwitch(drm_disp_base, 0, 1);

	/* Setup RDMA0 */
	RDMAConfigDirectLink(drm_disp_base, 0, width, height);

	/* Setup main path connection */
	DispConfigMainPathConnection(drm_disp_base);

	/* Setup main path mutex */
	DispConfigMainPathMutex(mutex);
}

void MainDispPathClear(struct drm_crtc *crtc)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	void __iomem *drm_disp_base = mtk_crtc->regs;
	void __iomem *mutex = mtk_crtc->mutex_regs;
	int i;

	pr_info("MainDispPathClear..all\n");
	for (i = 0; i < 0x100; i += 4)
		writel(0, drm_disp_base + i);

	for (i = 0x1000; i < 0x9000; i += 4) /* from ROT to BLS */
		writel(0, drm_disp_base + i);

	for (i = 0; i < 0x2000; i += 4) /* MUTEX & CMDQ */
		writel(0, mutex + i);
}

void DispClockSetup(struct device *dev, struct MTK_DISP_CLKS **pdisp_clks)
{
	*pdisp_clks = vmalloc(sizeof(struct MTK_DISP_CLKS));

	(*pdisp_clks)->smi_larb2_ck = devm_clk_get(dev, "smi_larb2_ck");
	if (IS_ERR((*pdisp_clks)->smi_larb2_ck))
		pr_err("DispClockSetup: Get smi_larb2_ck fail.\n");

	(*pdisp_clks)->ovl_disp_ck = devm_clk_get(dev, "ovl_disp_ck");
	if (IS_ERR((*pdisp_clks)->ovl_disp_ck))
		pr_err("DispClockSetup: Get ovl_disp_ck fail.\n");

	(*pdisp_clks)->ovl_smi_ck = devm_clk_get(dev, "ovl_smi_ck");
	if (IS_ERR((*pdisp_clks)->ovl_smi_ck))
		pr_err("DispClockSetup: Get ovl_smi_ck fail.\n");

	(*pdisp_clks)->bls_disp_ck = devm_clk_get(dev, "bls_disp_ck");
	if (IS_ERR((*pdisp_clks)->bls_disp_ck))
		pr_err("DispClockSetup: Get bls_disp_ck fail.\n");

	(*pdisp_clks)->rdma0_disp_ck = devm_clk_get(dev, "rdma0_disp_ck");
	if (IS_ERR((*pdisp_clks)->rdma0_disp_ck))
		pr_err("DispClockSetup: Get rdma0_disp_ck fail.\n");

	(*pdisp_clks)->rdma0_smi_ck = devm_clk_get(dev, "rdma0_smi_ck");
	if (IS_ERR((*pdisp_clks)->rdma0_smi_ck))
		pr_err("DispClockSetup: Get rdma0_smi_ckfail.\n");

	(*pdisp_clks)->rdma0_output_ck = devm_clk_get(dev, "rdma0_output_ck");
	if (IS_ERR((*pdisp_clks)->rdma0_output_ck))
		pr_err("DispClockSetup: Get rdma0_output_ck fail.\n");

	(*pdisp_clks)->mutex_disp_ck = devm_clk_get(dev, "mutex_disp_ck");
	if (IS_ERR((*pdisp_clks)->mutex_disp_ck))
		pr_err("DispClockSetup: Get mutex_disp_ck fail.\n");
}

void DispClockOn(struct MTK_DISP_CLKS *disp_clks)
{
	int ret;

	/* smi_larb2_ck */
	ret = clk_prepare(disp_clks->smi_larb2_ck);
	if (ret != 0)
		pr_err("DispClockOn: clk_prepare(disp_clks->smi_larb2_ck) error!\n");

	ret = clk_enable(disp_clks->smi_larb2_ck);
	if (ret != 0)
		pr_err("DispClockOn: clk_enable(disp_clks->smi_larb2_ck) error!\n");

	/* ovl_disp_ck */
	ret = clk_prepare(disp_clks->ovl_disp_ck);
	if (ret != 0)
		pr_err("DispClockOn: clk_prepare(disp_clks->ovl_disp_ck) error!\n");

	ret = clk_enable(disp_clks->ovl_disp_ck);
	if (ret != 0)
		pr_err("DispClockOn: clk_enable(disp_clks->ovl_disp_ck) error!\n");

	/* ovl_smi_ck */
	ret = clk_prepare(disp_clks->ovl_smi_ck);
	if (ret != 0)
		pr_err("DispClockOn: clk_prepare(disp_clks->ovl_smi_ck) error!\n");

	ret = clk_enable(disp_clks->ovl_smi_ck);
	if (ret != 0)
		pr_err("DispClockOn: clk_enable(disp_clks->ovl_smi_ck) error!\n");

	/* bls_disp_ck */
	ret = clk_prepare(disp_clks->bls_disp_ck);
	if (ret != 0)
		pr_err("DispClockOn: clk_prepare(disp_clks->bls_disp_ck) error!\n");

	ret = clk_enable(disp_clks->bls_disp_ck);
	if (ret != 0)
		pr_err("DispClockOn: clk_enable(disp_clks->bls_disp_ck) error!\n");

	/* rdma0_disp_ck */
	ret = clk_prepare(disp_clks->rdma0_disp_ck);
	if (ret != 0)
		pr_err("DispClockOn: clk_prepare(disp_clks->rdma0_disp_ck) error!\n");

	ret = clk_enable(disp_clks->rdma0_disp_ck);
	if (ret != 0)
		pr_err("DispClockOn: clk_enable(disp_clks->rdma0_disp_ck) error!\n");

	/* rdma0_smi_ck */
	ret = clk_prepare(disp_clks->rdma0_smi_ck);
	if (ret != 0)
		pr_err("DispClockOn: clk_prepare(disp_clks->rdma0_smi_ck) error!\n");

	ret = clk_enable(disp_clks->rdma0_smi_ck);
	if (ret != 0)
		pr_err("DispClockOn: clk_enable(disp_clks->rdma0_smi_ck) error!\n");

	/* rdma0_output_ck */
	ret = clk_prepare(disp_clks->rdma0_output_ck);
	if (ret != 0)
		pr_err("DispClockOn: clk_prepare(disp_clks->rdma0_output_ck) error!\n");

	ret = clk_enable(disp_clks->rdma0_output_ck);
	if (ret != 0)
		pr_err("DispClockOn: clk_enable(disp_clks->rdma0_output_ck) error!\n");

	/* mutex_disp_ck */
	ret = clk_prepare(disp_clks->mutex_disp_ck);
	if (ret != 0)
		pr_err("DispClockOn: clk_prepare(disp_clks->mutex_disp_ck) error!\n");

	ret = clk_enable(disp_clks->mutex_disp_ck);
	if (ret != 0)
		pr_err("DispClockOn: clk_enable(disp_clks->mutex_disp_ck) error!\n");
}

void DispClockOff(struct MTK_DISP_CLKS *disp_clks)
{
	/* mutex_disp_ck */
	clk_disable(disp_clks->mutex_disp_ck);
	clk_unprepare(disp_clks->mutex_disp_ck);

	/* ovl_disp_ck */
	clk_disable(disp_clks->ovl_disp_ck);
	clk_unprepare(disp_clks->ovl_disp_ck);

	/* ovl_smi_ck */
	clk_disable(disp_clks->ovl_smi_ck);
	clk_unprepare(disp_clks->ovl_smi_ck);

	/* bls_disp_ck */
	clk_disable(disp_clks->bls_disp_ck);
	clk_unprepare(disp_clks->bls_disp_ck);

	/* rdma0_disp_ck */
	clk_disable(disp_clks->rdma0_disp_ck);
	clk_unprepare(disp_clks->rdma0_disp_ck);

	/* rdma0_smi_ck */
	clk_disable(disp_clks->rdma0_smi_ck);
	clk_unprepare(disp_clks->rdma0_smi_ck);

	/* rdma0_output_ck */
	clk_disable(disp_clks->rdma0_output_ck);
	clk_unprepare(disp_clks->rdma0_output_ck);

	/* smi_larb2_ck */
	clk_disable(disp_clks->smi_larb2_ck);
	clk_unprepare(disp_clks->smi_larb2_ck);
}

void DispClockClear(void __iomem *drm_disp_base)
{
	unsigned int reg;

	/* ovl_disp_ck */
	reg = readl(drm_disp_base + DISP_REG_CONFIG_CG_CON0);
	reg |= (0xE131U);
	writel(reg, drm_disp_base + DISP_REG_CONFIG_CG_CON0);
}

