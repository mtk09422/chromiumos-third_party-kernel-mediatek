/*
 * Copyright (c) 2014 MediaTek Inc.
 * Author: YongWu <yong.wu@mediatek.com>
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

#include <linux/io.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/mm.h>
#include <linux/iommu.h>
#include <linux/errno.h>
#include <linux/io.h>
#include <linux/of_address.h>

#include "mtk_iommu_platform.h"
#include "mtk_iommu_reg_mt8173.h"

#define MTK_MM_TFID(larbid, portid) ((larbid << 7) | (portid << 2))

static const struct mtk_iommu_port
			mtk_iommu_mt8173_port[MTK_IOMMU_PORT_MAX_NR] = {
	/*port name                   m4uid slaveid larbid portid tfid*/
	/*larb0*/
	{"M4U_PORT_DISP_OVL0",          0,  0,    0,  0, MTK_MM_TFID(0, 0)},
	{"M4U_PORT_DISP_RDMA0",         0,  0,    0,  1, MTK_MM_TFID(0, 1)},
	{"M4U_PORT_DISP_WDMA0",         0,  0,    0,  2, MTK_MM_TFID(0, 2)},
	{"M4U_PORT_DISP_OD_R",          0,  0,    0,  3, MTK_MM_TFID(0, 3)},
	{"M4U_PORT_DISP_OD_W",          0,  0,    0,  4, MTK_MM_TFID(0, 4)},
	{"M4U_PORT_MDP_RDMA0",          0,  0,    0,  5, MTK_MM_TFID(0, 5)},
	{"M4U_PORT_MDP_WDMA",           0,  0,    0,  6, MTK_MM_TFID(0, 6)},
	{"M4U_PORT_MDP_WROT0",          0,  0,    0,  7, MTK_MM_TFID(0, 7)},

	/*larb1*/
	{"M4U_PORT_HW_VDEC_MC_EXT",      0,  0,   1,  0, MTK_MM_TFID(1, 0)},
	{"M4U_PORT_HW_VDEC_PP_EXT",      0,  0,   1,  1, MTK_MM_TFID(1, 1)},
	{"M4U_PORT_HW_VDEC_UFO_EXT",     0,  0,   1,  2, MTK_MM_TFID(1, 2)},
	{"M4U_PORT_HW_VDEC_VLD_EXT",     0,  0,   1,  3, MTK_MM_TFID(1, 3)},
	{"M4U_PORT_HW_VDEC_VLD2_EXT",    0,  0,   1,  4, MTK_MM_TFID(1, 4)},
	{"M4U_PORT_HW_VDEC_AVC_MV_EXT",  0,  0,   1,  5, MTK_MM_TFID(1, 5)},
	{"M4U_PORT_HW_VDEC_PRED_RD_EXT", 0,  0,   1,  6, MTK_MM_TFID(1, 6)},
	{"M4U_PORT_HW_VDEC_PRED_WR_EXT", 0,  0,   1,  7, MTK_MM_TFID(1, 7)},
	{"M4U_PORT_HW_VDEC_PPWRAP_EXT",  0,  0,   1,  8, MTK_MM_TFID(1, 8)},

	/*larb2*/
	{"M4U_PORT_IMGO",                0,  0,    2,  0, MTK_MM_TFID(2, 0)},
	{"M4U_PORT_RRZO",                0,  0,    2,  1, MTK_MM_TFID(2, 1)},
	{"M4U_PORT_AAO",                 0,  0,    2,  2, MTK_MM_TFID(2, 2)},
	{"M4U_PORT_LCSO",                0,  0,    2,  3, MTK_MM_TFID(2, 3)},
	{"M4U_PORT_ESFKO",               0,  0,    2,  4, MTK_MM_TFID(2, 4)},
	{"M4U_PORT_IMGO_D",              0,  0,    2,  5, MTK_MM_TFID(2, 5)},
	{"M4U_PORT_LSCI",                0,  0,    2,  6, MTK_MM_TFID(2, 6)},
	{"M4U_PORT_LSCI_D",              0,  0,    2,  7, MTK_MM_TFID(2, 7)},
	{"M4U_PORT_BPCI",                0,  0,    2,  8, MTK_MM_TFID(2, 8)},
	{"M4U_PORT_BPCI_D",              0,  0,    2,  9, MTK_MM_TFID(2, 9)},
	{"M4U_PORT_UFDI",                0,  0,    2,  10, MTK_MM_TFID(2, 10)},
	{"M4U_PORT_IMGI",                0,  0,    2,  11, MTK_MM_TFID(2, 11)},
	{"M4U_PORT_IMG2O",               0,  0,    2,  12, MTK_MM_TFID(2, 12)},
	{"M4U_PORT_IMG3O",               0,  0,    2,  13, MTK_MM_TFID(2, 13)},
	{"M4U_PORT_VIPI",                0,  0,    2,  14, MTK_MM_TFID(2, 14)},
	{"M4U_PORT_VIP2I",               0,  0,    2,  15, MTK_MM_TFID(2, 15)},
	{"M4U_PORT_VIP3I",               0,  0,    2,  16, MTK_MM_TFID(2, 16)},
	{"M4U_PORT_LCEI",                0,  0,    2,  17, MTK_MM_TFID(2, 17)},
	{"M4U_PORT_RB",                  0,  0,    2,  18, MTK_MM_TFID(2, 18)},
	{"M4U_PORT_RP",                  0,  0,    2,  19, MTK_MM_TFID(2, 19)},
	{"M4U_PORT_WR",                  0,  0,    2,  20, MTK_MM_TFID(2, 20)},

	/*larb3*/
	{"M4U_PORT_VENC_RCPU",            0,  0,   3,  0, MTK_MM_TFID(3, 0)},
	{"M4U_PORT_VENC_REC",             0,  0,   3,  1, MTK_MM_TFID(3, 1)},
	{"M4U_PORT_VENC_BSDMA",           0,  0,   3,  2, MTK_MM_TFID(3, 2)},
	{"M4U_PORT_VENC_SV_COMV",         0,  0,   3,  3, MTK_MM_TFID(3, 3)},
	{"M4U_PORT_VENC_RD_COMV",         0,  0,   3,  4, MTK_MM_TFID(3, 4)},
	{"M4U_PORT_JPGENC_RDMA",          0,  0,   3,  5, MTK_MM_TFID(3, 5)},
	{"M4U_PORT_JPGENC_BSDMA",         0,  0,   3,  6, MTK_MM_TFID(3, 6)},
	{"M4U_PORT_JPGDEC_WDMA",          0,  0,   3,  7, MTK_MM_TFID(3, 7)},
	{"M4U_PORT_JPGDEC_BSDMA",         0,  0,   3,  8, MTK_MM_TFID(3, 8)},
	{"M4U_PORT_VENC_CUR_LUMA",        0,  0,   3,  9, MTK_MM_TFID(3, 9)},
	{"M4U_PORT_VENC_CUR_CHROMA",      0,  0,   3,  10, MTK_MM_TFID(3, 10)},
	{"M4U_PORT_VENC_REF_LUMA",        0,  0,   3,  11, MTK_MM_TFID(3, 11)},
	{"M4U_PORT_VENC_REF_CHROMA",      0,  0,   3,  12, MTK_MM_TFID(3, 12)},
	{"M4U_PORT_VENC_NBM_RDMA",        0,  0,   3,  13, MTK_MM_TFID(3, 13)},
	{"M4U_PORT_VENC_NBM_WDMA",        0,  0,   3,  14, MTK_MM_TFID(3, 14)},

	/*larb4*/
	{"M4U_PORT_DISP_OVL1",             0,  0,   4,  0, MTK_MM_TFID(4, 0)},
	{"M4U_PORT_DISP_RDMA1",            0,  0,   4,  1, MTK_MM_TFID(4, 1)},
	{"M4U_PORT_DISP_RDMA2",            0,  0,   4,  2, MTK_MM_TFID(4, 2)},
	{"M4U_PORT_DISP_WDMA1",            0,  0,   4,  3, MTK_MM_TFID(4, 3)},
	{"M4U_PORT_MDP_RDMA1",             0,  0,   4,  4, MTK_MM_TFID(4, 4)},
	{"M4U_PORT_MDP_WROT1",             0,  0,   4,  5, MTK_MM_TFID(4, 5)},

	/*larb5*/
	{"M4U_PORT_VENC_RCPU_SET2",        0,  0,    5,  0, MTK_MM_TFID(4, 0)},
	{"M4U_PORT_VENC_REC_FRM_SET2",     0,  0,    5,  1, MTK_MM_TFID(5, 1)},
	{"M4U_PORT_VENC_REF_LUMA_SET2",    0,  0,    5,  2, MTK_MM_TFID(5, 2)},
	{"M4U_PORT_VENC_REC_CHROMA_SET2",  0,  0,    5,  3, MTK_MM_TFID(5, 3)},
	{"M4U_PORT_VENC_BSDMA_SET2",       0,  0,    5,  4, MTK_MM_TFID(5, 4)},
	{"M4U_PORT_VENC_CUR_LUMA_SET2",    0,  0,    5,  5, MTK_MM_TFID(5, 5)},
	{"M4U_PORT_VENC_CUR_CHROMA_SET2",  0,  0,    5,  6, MTK_MM_TFID(5, 6)},
	{"M4U_PORT_VENC_RD_COMA_SET2",     0,  0,    5,  7, MTK_MM_TFID(5, 7)},
	{"M4U_PORT_VENC_SV_COMA_SET2",     0,  0,    5,  8, MTK_MM_TFID(5, 8)},

	/*perisys iommu*/
	{"M4U_PORT_RESERVE",               1,  0,    6,  0, 0xff},
	{"M4U_PORT_SPM",                   1,  0,    6,  1, 0x50},
	{"M4U_PORT_MD32",                  1,  0,    6,  2, 0x90},
	{"M4U_PORT_PTP_THERM",             1,  0,    6,  4, 0xd0},
	{"M4U_PORT_PWM",                   1,  0,    6,  5, 0x1},
	{"M4U_PORT_MSDC1",                 1,  0,    6,  6, 0x21},
	{"M4U_PORT_MSDC2",                 1,  0,    6,  7, 0x41},
	{"M4U_PORT_NFI",                   1,  0,    6,  8, 0x8},
	{"M4U_PORT_AUDIO",                 1,  0,    6,  9, 0x48},
	{"M4U_PORT_RESERVED2",             1,  0,    6,  10, 0xfe},
	{"M4U_PORT_HSIC_XHCI",             1,  0,    6,  11, 0x9},

	{"M4U_PORT_HSIC_MAS",              1,  0,    6,  12, 0x11},
	{"M4U_PORT_HSIC_DEV",              1,  0,    6,  13, 0x19},
	{"M4U_PORT_AP_DMA",                1,  0,    6,  14, 0x18},
	{"M4U_PORT_HSIC_DMA",              1,  0,    6,  15, 0xc8},
	{"M4U_PORT_MSDC0",                 1,  0,    6,  16, 0x0},
	{"M4U_PORT_MSDC3",                 1,  0,    6,  17, 0x20},
	{"M4U_PORT_UNKNOWN",               1,  0,    6,  18, 0xf},
};

static inline void mt8173_iommu_clear_intr(void __iomem *m4u_base)
{
	u32 temp;

	temp = readl(m4u_base + REG_MMU_INT_L2_CONTROL) | F_INT_L2_CLR_BIT;
	writel(temp, m4u_base + REG_MMU_INT_L2_CONTROL);
}

static inline int mt8173_get_port_by_tfid(const struct mtk_iommu_info *pimu,
					  int m4u_id, int tf_id)
{
	const struct mtk_iommu_cfg *pimucfg = pimu->imucfg;
	int i;
	unsigned int portid = 0;

	if (m4u_id == 0)
		tf_id &= F_MMU0_INT_ID_TF_MSK;

	for (i = 0; i < pimucfg->m4u_port_nr; i++) {
		if ((pimucfg->pport[i].tf_id == tf_id) &&
		    (pimucfg->pport[i].m4u_id == m4u_id)) {
			portid = i;
			break;
		}
	}
	if (i == pimucfg->m4u_port_nr)
		dev_err(pimu->dev, "tf_id find fail, m4uid %d, tfid %d\n",
			m4u_id, tf_id);
	return portid;
}

static int mt8173_iommu_invalidate_tlb(const struct mtk_iommu_info *piommu,
				       unsigned int m4u_id, int isinvall,
				       unsigned int iova_start,
				       unsigned int iova_end)
{
	void __iomem *m4u_base = piommu->m4u[0].m4u_base;
	u32 reg;
	unsigned int cnt = 0;

	if (iova_start >= iova_end)
		isinvall = 1;

	if (!isinvall) {
		iova_start = round_down(iova_start, SZ_4K);
		iova_end = round_up(iova_end, SZ_4K);
	}

	reg = F_MMU_INV_EN_L2 | F_MMU_INV_EN_L1;

	writel(reg, m4u_base + REG_INVLID_SEL);

	if (isinvall) {
		writel(F_MMU_INV_ALL, m4u_base + REG_MMU_INVLD);
	} else {
		writel(iova_start, m4u_base + REG_MMU_INVLD_SA);
		writel(iova_end, m4u_base + REG_MMU_INVLD_EA);
		writel(F_MMU_INV_RANGE, m4u_base + REG_MMU_INVLD);
	}

	if (!isinvall) {
		while (!readl(m4u_base + REG_MMU_CPE_DONE)) {
			if (cnt++ >= 10000) {
				dev_warn(piommu->dev, "invalid don't done\n");
				cnt = 0;
			}
		};
		writel(0, m4u_base + REG_MMU_CPE_DONE);
	}

	return 0;
}

static int mt8173_iommu_parse_dt(struct platform_device *pdev,
				 struct mtk_iommu_info *piommu)
{
	struct device *piommudev = &pdev->dev;
	struct device_node *ofnode;
	struct resource *res;
	unsigned int mtk_iommu_cell = 0;
	unsigned int i;

	ofnode = piommudev->of_node;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	piommu->m4u_g_base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(piommu->m4u_g_base)) {
		dev_err(piommudev, "m4u_g_base 0x%p err\n", piommu->m4u_g_base);
		goto iommu_dts_err;
	}

	piommu->m4u[0].m4u_base = piommu->m4u_g_base;

	for (i = 0; i < piommu->imucfg->larb_nr; i++) {
		res = platform_get_resource(pdev, IORESOURCE_MEM, 1 + i);
		piommu->larb_base[i] =
				devm_ioremap_resource(&pdev->dev, res);
		if (IS_ERR(piommu->larb_base[i])) {
			dev_err(piommudev, "larb base[%d]-0x%p err\n",
				i, piommu->larb_base[i]);
			goto iommu_dts_err;
		}
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 7);

	piommu->m4u[0].irq = platform_get_irq(pdev, 0);
	if (piommu->m4u[0].irq < 0) {/*m4u1 irq err is allowed*/
		dev_err(piommudev, "iommu irq err %d %d\n",
			piommu->m4u[0].irq, piommu->m4u[1].irq);
		goto iommu_dts_err;
	}

	piommu->m4u_infra_clk = devm_clk_get(piommudev, "infra_m4u");
	if (IS_ERR(piommu->m4u_infra_clk)) {
		dev_err(piommudev, "clk err 0x%p\n", piommu->m4u_infra_clk);
		goto iommu_dts_err;
	}

	of_property_read_u32(ofnode, "iommu-cells", &mtk_iommu_cell);
	if (mtk_iommu_cell != 1) {
		dev_err(piommudev, "mtk-cell fail:%d\n", mtk_iommu_cell);
		goto iommu_dts_err;
	}

	return 0;

iommu_dts_err:
	dev_err(piommudev, "mt parse dt fail\n");
	return -EINVAL;
}

static int mt8173_iommu_hw_init(const struct mtk_iommu_info *piommu)
{
	u32 i = 0;
	u32 regval, protectpa;
	int ret = 0;
	void __iomem *gm4ubaseaddr = piommu->m4u[i].m4u_base;

	ret = clk_prepare_enable(piommu->m4u_infra_clk);
	if (ret) {
		dev_err(piommu->dev, "m4u clk enable error\n");
		return -ENODEV;
	}

	writel(piommu->pgt_basepa, piommu->m4u_g_base + REG_MMUG_PT_BASE);

	/*legacy mode*/
	writel(1, piommu->m4u_g_base + REG_MMU_LEGACY_4KB_MODE);

	protectpa = (unsigned int)virt_to_phys(piommu->protect_va);
	protectpa = ALIGN(protectpa, MTK_PROTECT_PA_ALIGN);

	regval = F_MMU_CTRL_REROUTE_PFQ_TO_MQ_EN |
		F_MMU_CTRL_TF_PROT_VAL(2) |
		F_MMU_CTRL_COHERE_EN;
	writel(regval, gm4ubaseaddr + REG_MMU_CTRL_REG);

	writel(0x6f, gm4ubaseaddr + REG_MMU_INT_L2_CONTROL);
	writel(0xffffffff, gm4ubaseaddr + REG_MMU_INT_MAIN_CONTROL);

	/* protect memory,HW will write here while translation fault */
	writel(F_MMU_IVRP_PA_SET(protectpa), gm4ubaseaddr + REG_MMU_IVRP_PADDR);

	writel(0, piommu->m4u_g_base + REG_MMU_DCM_DIS);

	/* 3 non-standard AXI mode */
	writel(0, gm4ubaseaddr + REG_MMU_STANDARD_AXI_MODE);

	return 0;
}

static int mt8173_iommu_fault_handler(struct iommu_domain *imudomain,
				      struct device *dev, unsigned long iova,
				      int m4u_index, void *pimu)
{
	void __iomem *m4u_base;
	u32 int_state, regval;
	int m4u_slave_id = 0;
	unsigned int layer, write, m4u_port;
	unsigned int fault_mva, fault_pa;
	struct mtk_iommu_info *piommu = pimu;
	struct mtk_iommu_domain *mtkdomain = imudomain->priv;
	unsigned int *ptestart;

	m4u_base = piommu->m4u[m4u_index].m4u_base;
	int_state = readl(m4u_base + REG_MMU_MAIN_FAULT_ST);

	/* read error info from registers */
	fault_mva = readl(m4u_base + REG_MMU_FAULT_VA(m4u_slave_id));
	layer = !!(fault_mva & F_MMU_FAULT_VA_LAYER_BIT);
	write = !!(fault_mva & F_MMU_FAULT_VA_WRITE_BIT);
	fault_mva &= F_MMU_FAULT_VA_MSK;
	fault_pa = readl(m4u_base + REG_MMU_INVLD_PA(m4u_slave_id));
	regval = readl(m4u_base + REG_MMU_INT_ID(m4u_slave_id));
	m4u_port = mt8173_get_port_by_tfid(piommu, m4u_index, regval);

	if (int_state & F_INT_TRANSLATION_FAULT(m4u_slave_id)) {
		dev_err_ratelimited(dev,
				    "fault:port=%s iova=0x%x pa=0x%x layer=%d wr=%d\n",
				    mtk_iommu_get_port_name(piommu, m4u_port),
				    fault_mva, fault_pa, layer, write);

		if (fault_mva < piommu->iova_size &&
		    fault_mva > piommu->iova_base) {
			ptestart = mtkdomain->pgtableva
					+ (fault_mva >> PAGE_SHIFT);
			dev_err_ratelimited(dev,
					    "pagetable @ 0x%x: 0x%x,0x%x,0x%x\n",
					    fault_mva, ptestart[-1],
					    ptestart[0], ptestart[1]);
		}

		/* call user's callback to dump user registers */
		if (piommu->portcfg[m4u_port].fault_handler)
			piommu->portcfg[m4u_port].fault_handler(
				m4u_port, fault_mva,
				piommu->portcfg[m4u_port].faultdata);
	}

	if (int_state & F_INT_MAIN_MULTI_HIT_FAULT(m4u_slave_id))
		dev_err_ratelimited(dev, "multi-hit!port=%s iova=0x%x\n",
				    mtk_iommu_get_port_name(piommu, m4u_port),
				    fault_mva);

	if (int_state & F_INT_INVALID_PA_FAULT(m4u_slave_id)) {
		if (!(int_state & F_INT_TRANSLATION_FAULT(m4u_slave_id)))
			dev_err_ratelimited(dev, "invalid pa!port=%s iova=0x%x\n",
					    mtk_iommu_get_port_name(piommu,
								    m4u_port),
					    fault_mva);
	}
	if (int_state & F_INT_ENTRY_REPLACEMENT_FAULT(m4u_slave_id))
		dev_err_ratelimited(dev, "replace-fault!port=%s iova=0x%x\n",
				    mtk_iommu_get_port_name(piommu, m4u_port),
				    fault_mva);

	if (int_state & F_INT_TLB_MISS_FAULT(m4u_slave_id))
		dev_err_ratelimited(dev, "tlb miss-fault!port=%s iova=0x%x\n",
				    mtk_iommu_get_port_name(piommu, m4u_port),
				    fault_mva);

	mt8173_iommu_invalidate_tlb(piommu, 0, 1, 0, 0);

	mt8173_iommu_clear_intr(m4u_base);

	return 0;
}

static int mt8173_iommu_map(struct mtk_iommu_domain *mtkdomain,
			    unsigned int iova, phys_addr_t paddr, size_t size)
{
	unsigned int i;
	unsigned int page_num = DIV_ROUND_UP(size, PAGE_SIZE);
	u32 *pgt_base_iova;
	u32 pabase = (u32)paddr;

	/*spinlock in upper function*/
	pgt_base_iova = mtkdomain->pgtableva + (iova >> PAGE_SHIFT);
	for (i = 0; i < page_num; i++) {
		pgt_base_iova[i] =
		    pabase | F_DESC_VALID | F_DESC_NONSEC(1) | F_DESC_SHARE(0);
		pabase += PAGE_SIZE;
	}
	mt8173_iommu_invalidate_tlb(mtkdomain->piommuinfo, M4U_ID_ALL, 0,
				    iova, iova + size - 1);
	return 0;
}

static size_t mt8173_iommu_unmap(struct mtk_iommu_domain *mtkdomain,
				 unsigned int iova, size_t size)
{
	unsigned int page_num = DIV_ROUND_UP(size, PAGE_SIZE);
	unsigned int *pgt_base_iova;

	/*spinlock in upper function*/
	pgt_base_iova = mtkdomain->pgtableva + (iova >> PAGE_SHIFT);
	memset(pgt_base_iova, 0x0, page_num * sizeof(int));

	mt8173_iommu_invalidate_tlb(mtkdomain->piommuinfo, M4U_ID_ALL, 0,
				    iova, iova + size - 1);
	return 0;
}

static phys_addr_t mt8173_iommu_iova_to_phys(struct mtk_iommu_domain *mtkdomain,
					     unsigned int iova)
{
	u32 phys;

	phys = *(mtkdomain->pgtableva + (iova >> PAGE_SHIFT));
	return (phys_addr_t)phys;
}

static int mt8173_iommu_config_port(struct mtk_iommu_info *piommu,
				    int portid)
{
	int larb, larb_port;
	u32 reg;
	void __iomem *larb_base;

	larb = piommu->imucfg->pport[portid].larb_id;
	larb_port = piommu->imucfg->pport[portid].port_id;
	larb_base = piommu->larb_base[larb];

	reg = readl(larb_base + SMI_LARB_MMU_EN);
	reg = reg & (~F_SMI_MMU_EN(larb_port));
	reg = reg | F_SMI_MMU_EN(larb_port);/*vir*/
	writel(reg, larb_base + SMI_LARB_MMU_EN);

	if (reg != readl(larb_base + SMI_LARB_MMU_EN)) {
		dev_err(piommu->dev, "cfg port %s fail\n",
			mtk_iommu_get_port_name(piommu, portid));
	}

	return 0;
}

const struct mtk_iommu_cfg mtk_iommu_mt8173_cfg = {
	.m4u_nr = 1,
	.larb_nr = 6,
	.m4u_port_in_larbx = {0, 8, 17, 38, 43, 49, 58},
	.m4u_port_nr = 85,
	.pport = mtk_iommu_mt8173_port,
	.dt_parse = mt8173_iommu_parse_dt,
	.hw_init = mt8173_iommu_hw_init,
	.map = mt8173_iommu_map,
	.unmap = mt8173_iommu_unmap,
	.iova_to_phys = mt8173_iommu_iova_to_phys,
	.config_port = mt8173_iommu_config_port,
	.faulthandler = mt8173_iommu_fault_handler,
};
