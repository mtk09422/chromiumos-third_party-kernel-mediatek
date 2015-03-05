/*
 * Copyright (c) 2014-2015 MediaTek Inc.
 * Author: Yong Wu <yong.wu@mediatek.com>
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

#define pr_fmt(fmt) "m4u:"fmt

#include <linux/io.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/mm.h>
#include <linux/iommu.h>
#include <linux/errno.h>
#include <linux/list.h>
#include <linux/memblock.h>
#include <linux/dma-mapping.h>
#include <linux/dma-iommu.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/of_platform.h>
#include <linux/mtk-smi.h>
#include <asm/cacheflush.h>

#include "mtk_iommu.h"

#define REG_MMUG_PT_BASE	 0x0

#define REG_MMU_INVLD		 0x20
#define F_MMU_INV_ALL		 0x2
#define F_MMU_INV_RANGE		 0x1

#define REG_MMU_INVLD_SA	 0x24
#define REG_MMU_INVLD_EA         0x28

#define REG_MMU_INVLD_SEC         0x2c
#define F_MMU_INV_SEC_ALL          0x2
#define F_MMU_INV_SEC_RANGE        0x1

#define REG_INVLID_SEL	         0x38
#define F_MMU_INV_EN_L1		 BIT(0)
#define F_MMU_INV_EN_L2		 BIT(1)

#define REG_MMU_STANDARD_AXI_MODE   0x48
#define REG_MMU_DCM_DIS             0x50
#define REG_MMU_LEGACY_4KB_MODE     0x60

#define REG_MMU_CTRL_REG                 0x110
#define F_MMU_CTRL_REROUTE_PFQ_TO_MQ_EN  BIT(4)
#define F_MMU_CTRL_TF_PROT_VAL(prot)      (((prot) & 0x3)<<5)
#define F_MMU_CTRL_COHERE_EN             BIT(8)

#define REG_MMU_IVRP_PADDR               0x114
#define F_MMU_IVRP_PA_SET(PA)            (PA>>1)

#define REG_MMU_INT_L2_CONTROL      0x120
#define F_INT_L2_CLR_BIT            BIT(12)

#define REG_MMU_INT_MAIN_CONTROL    0x124
#define F_INT_TRANSLATION_FAULT(MMU)           (1<<(0+(((MMU)<<1)|((MMU)<<2))))
#define F_INT_MAIN_MULTI_HIT_FAULT(MMU)        (1<<(1+(((MMU)<<1)|((MMU)<<2))))
#define F_INT_INVALID_PA_FAULT(MMU)            (1<<(2+(((MMU)<<1)|((MMU)<<2))))
#define F_INT_ENTRY_REPLACEMENT_FAULT(MMU)     (1<<(3+(((MMU)<<1)|((MMU)<<2))))
#define F_INT_TLB_MISS_FAULT(MMU)              (1<<(4+(((MMU)<<1)|((MMU)<<2))))
#define F_INT_PFH_FIFO_ERR(MMU)                (1<<(6+(((MMU)<<1)|((MMU)<<2))))

#define REG_MMU_CPE_DONE              0x12C

#define REG_MMU_MAIN_FAULT_ST         0x134

#define REG_MMU_FAULT_VA(mmu)         (0x13c+((mmu)<<3))
#define F_MMU_FAULT_VA_MSK            ((~0x0)<<12)
#define F_MMU_FAULT_VA_WRITE_BIT       BIT(1)
#define F_MMU_FAULT_VA_LAYER_BIT       BIT(0)

#define REG_MMU_INVLD_PA(mmu)         (0x140+((mmu)<<3))
#define REG_MMU_INT_ID(mmu)           (0x150+((mmu)<<2))
#define F_MMU0_INT_ID_TF_MSK          (~0x3)	/* only for MM iommu. */

#define MTK_TFID(larbid, portid) ((larbid << 7) | (portid << 2))

static const struct mtk_iommu_port mtk_iommu_mt8173_port[] = {
	/* port name                m4uid slaveid larbid portid tfid */
	/* larb0 */
	{"M4U_PORT_DISP_OVL0",          0,  0,    0,  0, MTK_TFID(0, 0)},
	{"M4U_PORT_DISP_RDMA0",         0,  0,    0,  1, MTK_TFID(0, 1)},
	{"M4U_PORT_DISP_WDMA0",         0,  0,    0,  2, MTK_TFID(0, 2)},
	{"M4U_PORT_DISP_OD_R",          0,  0,    0,  3, MTK_TFID(0, 3)},
	{"M4U_PORT_DISP_OD_W",          0,  0,    0,  4, MTK_TFID(0, 4)},
	{"M4U_PORT_MDP_RDMA0",          0,  0,    0,  5, MTK_TFID(0, 5)},
	{"M4U_PORT_MDP_WDMA",           0,  0,    0,  6, MTK_TFID(0, 6)},
	{"M4U_PORT_MDP_WROT0",          0,  0,    0,  7, MTK_TFID(0, 7)},

	/* larb1 */
	{"M4U_PORT_HW_VDEC_MC_EXT",      0,  0,   1,  0, MTK_TFID(1, 0)},
	{"M4U_PORT_HW_VDEC_PP_EXT",      0,  0,   1,  1, MTK_TFID(1, 1)},
	{"M4U_PORT_HW_VDEC_UFO_EXT",     0,  0,   1,  2, MTK_TFID(1, 2)},
	{"M4U_PORT_HW_VDEC_VLD_EXT",     0,  0,   1,  3, MTK_TFID(1, 3)},
	{"M4U_PORT_HW_VDEC_VLD2_EXT",    0,  0,   1,  4, MTK_TFID(1, 4)},
	{"M4U_PORT_HW_VDEC_AVC_MV_EXT",  0,  0,   1,  5, MTK_TFID(1, 5)},
	{"M4U_PORT_HW_VDEC_PRED_RD_EXT", 0,  0,   1,  6, MTK_TFID(1, 6)},
	{"M4U_PORT_HW_VDEC_PRED_WR_EXT", 0,  0,   1,  7, MTK_TFID(1, 7)},
	{"M4U_PORT_HW_VDEC_PPWRAP_EXT",  0,  0,   1,  8, MTK_TFID(1, 8)},

	/* larb2 */
	{"M4U_PORT_IMGO",                0,  0,    2,  0, MTK_TFID(2, 0)},
	{"M4U_PORT_RRZO",                0,  0,    2,  1, MTK_TFID(2, 1)},
	{"M4U_PORT_AAO",                 0,  0,    2,  2, MTK_TFID(2, 2)},
	{"M4U_PORT_LCSO",                0,  0,    2,  3, MTK_TFID(2, 3)},
	{"M4U_PORT_ESFKO",               0,  0,    2,  4, MTK_TFID(2, 4)},
	{"M4U_PORT_IMGO_D",              0,  0,    2,  5, MTK_TFID(2, 5)},
	{"M4U_PORT_LSCI",                0,  0,    2,  6, MTK_TFID(2, 6)},
	{"M4U_PORT_LSCI_D",              0,  0,    2,  7, MTK_TFID(2, 7)},
	{"M4U_PORT_BPCI",                0,  0,    2,  8, MTK_TFID(2, 8)},
	{"M4U_PORT_BPCI_D",              0,  0,    2,  9, MTK_TFID(2, 9)},
	{"M4U_PORT_UFDI",                0,  0,    2,  10, MTK_TFID(2, 10)},
	{"M4U_PORT_IMGI",                0,  0,    2,  11, MTK_TFID(2, 11)},
	{"M4U_PORT_IMG2O",               0,  0,    2,  12, MTK_TFID(2, 12)},
	{"M4U_PORT_IMG3O",               0,  0,    2,  13, MTK_TFID(2, 13)},
	{"M4U_PORT_VIPI",                0,  0,    2,  14, MTK_TFID(2, 14)},
	{"M4U_PORT_VIP2I",               0,  0,    2,  15, MTK_TFID(2, 15)},
	{"M4U_PORT_VIP3I",               0,  0,    2,  16, MTK_TFID(2, 16)},
	{"M4U_PORT_LCEI",                0,  0,    2,  17, MTK_TFID(2, 17)},
	{"M4U_PORT_RB",                  0,  0,    2,  18, MTK_TFID(2, 18)},
	{"M4U_PORT_RP",                  0,  0,    2,  19, MTK_TFID(2, 19)},
	{"M4U_PORT_WR",                  0,  0,    2,  20, MTK_TFID(2, 20)},

	/* larb3 */
	{"M4U_PORT_VENC_RCPU",            0,  0,   3,  0, MTK_TFID(3, 0)},
	{"M4U_PORT_VENC_REC",             0,  0,   3,  1, MTK_TFID(3, 1)},
	{"M4U_PORT_VENC_BSDMA",           0,  0,   3,  2, MTK_TFID(3, 2)},
	{"M4U_PORT_VENC_SV_COMV",         0,  0,   3,  3, MTK_TFID(3, 3)},
	{"M4U_PORT_VENC_RD_COMV",         0,  0,   3,  4, MTK_TFID(3, 4)},
	{"M4U_PORT_JPGENC_RDMA",          0,  0,   3,  5, MTK_TFID(3, 5)},
	{"M4U_PORT_JPGENC_BSDMA",         0,  0,   3,  6, MTK_TFID(3, 6)},
	{"M4U_PORT_JPGDEC_WDMA",          0,  0,   3,  7, MTK_TFID(3, 7)},
	{"M4U_PORT_JPGDEC_BSDMA",         0,  0,   3,  8, MTK_TFID(3, 8)},
	{"M4U_PORT_VENC_CUR_LUMA",        0,  0,   3,  9, MTK_TFID(3, 9)},
	{"M4U_PORT_VENC_CUR_CHROMA",      0,  0,   3,  10, MTK_TFID(3, 10)},
	{"M4U_PORT_VENC_REF_LUMA",        0,  0,   3,  11, MTK_TFID(3, 11)},
	{"M4U_PORT_VENC_REF_CHROMA",      0,  0,   3,  12, MTK_TFID(3, 12)},
	{"M4U_PORT_VENC_NBM_RDMA",        0,  0,   3,  13, MTK_TFID(3, 13)},
	{"M4U_PORT_VENC_NBM_WDMA",        0,  0,   3,  14, MTK_TFID(3, 14)},

	/* larb4 */
	{"M4U_PORT_DISP_OVL1",             0,  0,   4,  0, MTK_TFID(4, 0)},
	{"M4U_PORT_DISP_RDMA1",            0,  0,   4,  1, MTK_TFID(4, 1)},
	{"M4U_PORT_DISP_RDMA2",            0,  0,   4,  2, MTK_TFID(4, 2)},
	{"M4U_PORT_DISP_WDMA1",            0,  0,   4,  3, MTK_TFID(4, 3)},
	{"M4U_PORT_MDP_RDMA1",             0,  0,   4,  4, MTK_TFID(4, 4)},
	{"M4U_PORT_MDP_WROT1",             0,  0,   4,  5, MTK_TFID(4, 5)},

	/* larb5 */
	{"M4U_PORT_VENC_RCPU_SET2",        0,  0,    5,  0, MTK_TFID(5, 0)},
	{"M4U_PORT_VENC_REC_FRM_SET2",     0,  0,    5,  1, MTK_TFID(5, 1)},
	{"M4U_PORT_VENC_REF_LUMA_SET2",    0,  0,    5,  2, MTK_TFID(5, 2)},
	{"M4U_PORT_VENC_REC_CHROMA_SET2",  0,  0,    5,  3, MTK_TFID(5, 3)},
	{"M4U_PORT_VENC_BSDMA_SET2",       0,  0,    5,  4, MTK_TFID(5, 4)},
	{"M4U_PORT_VENC_CUR_LUMA_SET2",    0,  0,    5,  5, MTK_TFID(5, 5)},
	{"M4U_PORT_VENC_CUR_CHROMA_SET2",  0,  0,    5,  6, MTK_TFID(5, 6)},
	{"M4U_PORT_VENC_RD_COMA_SET2",     0,  0,    5,  7, MTK_TFID(5, 7)},
	{"M4U_PORT_VENC_SV_COMA_SET2",     0,  0,    5,  8, MTK_TFID(5, 8)},

	/* perisys iommu */
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

static const struct mtk_iommu_cfg mtk_iommu_mt8173_cfg = {
	.larb_nr = 6,
	.m4u_port_nr = ARRAY_SIZE(mtk_iommu_mt8173_port),
	.pport = mtk_iommu_mt8173_port,
};

static const char *mtk_iommu_get_port_name(const struct mtk_iommu_info *piommu,
					   unsigned int portid)
{
	const struct mtk_iommu_port *pcurport = NULL;

	pcurport = piommu->imucfg->pport + portid;
	if (portid < piommu->imucfg->m4u_port_nr && pcurport)
		return pcurport->port_name;
	else
		return "UNKNOWN_PORT";
}

static int mtk_iommu_get_port_by_tfid(const struct mtk_iommu_info *pimu,
				      int tf_id)
{
	const struct mtk_iommu_cfg *pimucfg = pimu->imucfg;
	int i;
	unsigned int portid = pimucfg->m4u_port_nr;

	for (i = 0; i < pimucfg->m4u_port_nr; i++) {
		if (pimucfg->pport[i].tf_id == tf_id) {
			portid = i;
			break;
		}
	}
	if (i == pimucfg->m4u_port_nr)
		dev_err(pimu->dev, "tf_id find fail, tfid %d\n", tf_id);
	return portid;
}

static irqreturn_t mtk_iommu_isr(int irq, void *dev_id)
{
	struct iommu_domain *domain = dev_id;
	struct mtk_iommu_domain *mtkdomain = domain->priv;
	struct mtk_iommu_info *piommu = mtkdomain->piommuinfo;

	if (irq == piommu->irq)
		report_iommu_fault(domain, piommu->dev, 0, 0);
	else
		dev_err(piommu->dev, "irq number:%d\n", irq);

	return IRQ_HANDLED;
}

static inline void mtk_iommu_clear_intr(void __iomem *m4u_base)
{
	u32 val;

	val = readl(m4u_base + REG_MMU_INT_L2_CONTROL);
	val |= F_INT_L2_CLR_BIT;
	writel(val, m4u_base + REG_MMU_INT_L2_CONTROL);
}

static int mtk_iommu_invalidate_tlb(const struct mtk_iommu_info *piommu,
				    int isinvall, unsigned int iova_start,
				    unsigned int iova_end)
{
	void __iomem *m4u_base = piommu->m4u_base;
	u32 val;
	u64 start, end;

	start = sched_clock();

	if (!isinvall) {
		iova_start = round_down(iova_start, SZ_4K);
		iova_end = round_up(iova_end, SZ_4K);
	}

	val = F_MMU_INV_EN_L2 | F_MMU_INV_EN_L1;

	writel(val, m4u_base + REG_INVLID_SEL);

	if (isinvall) {
		writel(F_MMU_INV_ALL, m4u_base + REG_MMU_INVLD);
	} else {
		writel(iova_start, m4u_base + REG_MMU_INVLD_SA);
		writel(iova_end, m4u_base + REG_MMU_INVLD_EA);
		writel(F_MMU_INV_RANGE, m4u_base + REG_MMU_INVLD);

		while (!readl(m4u_base + REG_MMU_CPE_DONE)) {
			end = sched_clock();
			if (end - start >= 100000000ULL) {
				dev_warn(piommu->dev, "invalid don't done\n");
				writel(F_MMU_INV_ALL, m4u_base + REG_MMU_INVLD);
			}
		};
		writel(0, m4u_base + REG_MMU_CPE_DONE);
	}

	return 0;
}

static int mtk_iommu_fault_handler(struct iommu_domain *imudomain,
				   struct device *dev, unsigned long iova,
				   int m4uindex, void *pimu)
{
	void __iomem *m4u_base;
	u32 int_state, regval;
	int m4u_slave_id = 0;
	unsigned int layer, write, m4u_port;
	unsigned int fault_mva, fault_pa;
	struct mtk_iommu_info *piommu = pimu;
	struct mtk_iommu_domain *mtkdomain = imudomain->priv;

	m4u_base = piommu->m4u_base;
	int_state = readl(m4u_base + REG_MMU_MAIN_FAULT_ST);

	/* read error info from registers */
	fault_mva = readl(m4u_base + REG_MMU_FAULT_VA(m4u_slave_id));
	layer = !!(fault_mva & F_MMU_FAULT_VA_LAYER_BIT);
	write = !!(fault_mva & F_MMU_FAULT_VA_WRITE_BIT);
	fault_mva &= F_MMU_FAULT_VA_MSK;
	fault_pa = readl(m4u_base + REG_MMU_INVLD_PA(m4u_slave_id));
	regval = readl(m4u_base + REG_MMU_INT_ID(m4u_slave_id));
	regval &= F_MMU0_INT_ID_TF_MSK;
	m4u_port = mtk_iommu_get_port_by_tfid(piommu, regval);

	if (int_state & F_INT_TRANSLATION_FAULT(m4u_slave_id)) {
		struct m4u_pte_info_t pte;
		unsigned long flags;

		spin_lock_irqsave(&mtkdomain->pgtlock, flags);
		m4u_get_pte_info(mtkdomain, fault_mva, &pte);
		spin_unlock_irqrestore(&mtkdomain->pgtlock, flags);

		if (pte.size == MMU_SMALL_PAGE_SIZE ||
		    pte.size == MMU_LARGE_PAGE_SIZE) {
			dev_err_ratelimited(
				dev,
				"fault:port=%s iova=0x%x pa=0x%x layer=%d %s;"
				"pgd(0x%x)->pte(0x%x)->pa(%pad)sz(0x%x)Valid(%d)\n",
				mtk_iommu_get_port_name(piommu, m4u_port),
				fault_mva, fault_pa, layer,
				write ? "write" : "read",
				imu_pgd_val(*pte.pgd), imu_pte_val(*pte.pte),
				&pte.pa, pte.size, pte.valid);
		} else {
			dev_err_ratelimited(
				dev,
				"fault:port=%s iova=0x%x pa=0x%x layer=%d %s;"
				"pgd(0x%x)->pa(%pad)sz(0x%x)Valid(%d)\n",
				mtk_iommu_get_port_name(piommu, m4u_port),
				fault_mva, fault_pa, layer,
				write ? "write" : "read",
				imu_pgd_val(*pte.pgd),
				&pte.pa, pte.size, pte.valid);
		}
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

	mtk_iommu_invalidate_tlb(piommu, 1, 0, 0);

	mtk_iommu_clear_intr(m4u_base);

	return 0;
}

static int mtk_iommu_parse_dt(struct platform_device *pdev,
			      struct mtk_iommu_info *piommu)
{
	struct device *piommudev = &pdev->dev;
	struct device_node *ofnode;
	struct resource *res;
	unsigned int mtk_iommu_cell = 0;
	unsigned int i;

	ofnode = piommudev->of_node;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	piommu->m4u_base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(piommu->m4u_base)) {
		dev_err(piommudev, "m4u_base %p err\n", piommu->m4u_base);
		goto iommu_dts_err;
	}

	piommu->irq = platform_get_irq(pdev, 0);
	if (piommu->irq < 0) {
		dev_err(piommudev, "irq err %d\n", piommu->irq);
		goto iommu_dts_err;
	}

	piommu->m4u_infra_clk = devm_clk_get(piommudev, "infra_m4u");
	if (IS_ERR(piommu->m4u_infra_clk)) {
		dev_err(piommudev, "clk err %p\n", piommu->m4u_infra_clk);
		goto iommu_dts_err;
	}

	of_property_read_u32(ofnode, "#iommu-cells", &mtk_iommu_cell);
	if (mtk_iommu_cell != 1) {
		dev_err(piommudev, "iommu-cell fail:%d\n", mtk_iommu_cell);
		goto iommu_dts_err;
	}

	for (i = 0; i < piommu->imucfg->larb_nr; i++) {
		struct device_node *larbnode;

		larbnode = of_parse_phandle(ofnode, "larb", i);
		piommu->larbpdev[i] = of_find_device_by_node(larbnode);
		of_node_put(larbnode);
		if (!piommu->larbpdev[i]) {
			dev_err(piommudev, "larb pdev fail@larb%d\n", i);
			goto iommu_dts_err;
		}
	}

	return 0;

iommu_dts_err:
	return -EINVAL;
}

static int mtk_iommu_hw_init(const struct mtk_iommu_domain *mtkdomain)
{
	struct mtk_iommu_info *piommu = mtkdomain->piommuinfo;
	void __iomem *gm4ubaseaddr = piommu->m4u_base;
	phys_addr_t protectpa;
	u32 regval, protectreg;
	int ret = 0;

	ret = clk_prepare_enable(piommu->m4u_infra_clk);
	if (ret) {
		dev_err(piommu->dev, "m4u clk enable error\n");
		return -ENODEV;
	}

	writel((u32)mtkdomain->pgd_pa, gm4ubaseaddr + REG_MMUG_PT_BASE);

	regval = F_MMU_CTRL_REROUTE_PFQ_TO_MQ_EN |
		F_MMU_CTRL_TF_PROT_VAL(2) |
		F_MMU_CTRL_COHERE_EN;
	writel(regval, gm4ubaseaddr + REG_MMU_CTRL_REG);

	writel(0x6f, gm4ubaseaddr + REG_MMU_INT_L2_CONTROL);
	writel(0xffffffff, gm4ubaseaddr + REG_MMU_INT_MAIN_CONTROL);

	/* protect memory,HW will write here while translation fault */
	protectpa = __virt_to_phys(piommu->protect_va);
	protectpa = ALIGN(protectpa, MTK_PROTECT_PA_ALIGN);
	protectreg = (u32)F_MMU_IVRP_PA_SET(protectpa);
	writel(protectreg, gm4ubaseaddr + REG_MMU_IVRP_PADDR);

	writel(0, gm4ubaseaddr + REG_MMU_DCM_DIS);
	writel(0, gm4ubaseaddr + REG_MMU_STANDARD_AXI_MODE);

	return 0;
}

static inline void mtk_iommu_config_port(struct mtk_iommu_info *piommu,
					 int portid)
{
	int larb, larb_port;

	larb = piommu->imucfg->pport[portid].larb_id;
	larb_port = piommu->imucfg->pport[portid].port_id;

	mtk_smi_config_port(piommu->larbpdev[larb], larb_port);
}

/*
 * pimudev is a global var for dma_alloc_coherent.
 * It is not accepatable, we will delete it if "domain_alloc" is enabled
 */
static struct device *pimudev;

static int mtk_iommu_domain_init(struct iommu_domain *domain)
{
	struct mtk_iommu_domain *priv;

	priv = kzalloc(sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	priv->pgd = dma_alloc_coherent(pimudev, M4U_PGD_SIZE, &priv->pgd_pa,
				       GFP_KERNEL);
	if (!priv->pgd) {
		pr_err("dma_alloc_coherent pagetable fail\n");
		goto err_pgtable;
	}

	if (!IS_ALIGNED(priv->pgd_pa, M4U_PGD_SIZE)) {
		pr_err("pagetable not aligned pa 0x%pad-0x%p align 0x%x\n",
		       &priv->pgd_pa, priv->pgd, M4U_PGD_SIZE);
		goto err_pgtable;
	}

	memset(priv->pgd, 0, M4U_PGD_SIZE);

	spin_lock_init(&priv->pgtlock);
	spin_lock_init(&priv->portlock);
	domain->priv = priv;

	domain->geometry.aperture_start = 0;
	domain->geometry.aperture_end   = (unsigned int)~0;
	domain->geometry.force_aperture = true;

	return 0;

err_pgtable:
	if (priv->pgd)
		dma_free_coherent(pimudev, M4U_PGD_SIZE, priv->pgd,
				  priv->pgd_pa);
	kfree(priv);
	return -ENOMEM;
}

static void mtk_iommu_domain_destroy(struct iommu_domain *domain)
{
	struct mtk_iommu_domain *priv = domain->priv;

	dma_free_coherent(priv->piommuinfo->dev, M4U_PGD_SIZE,
			  priv->pgd, priv->pgd_pa);
	kfree(domain->priv);
	domain->priv = NULL;
}

static int mtk_iommu_attach_device(struct iommu_domain *domain,
				   struct device *dev)
{
	unsigned long flags;
	struct mtk_iommu_domain *priv = domain->priv;
	struct mtk_iommu_info *piommu = priv->piommuinfo;
	struct of_phandle_args out_args = {0};
	struct device *imudev;
	unsigned int i = 0;

	if (!piommu)
		goto imudev;
	else
		imudev = piommu->dev;

	spin_lock_irqsave(&priv->portlock, flags);

	while (!of_parse_phandle_with_args(dev->of_node, "iommus",
					   "#iommu-cells", i, &out_args)) {
		if (1 == out_args.args_count) {
			unsigned int portid = out_args.args[0];

			dev_dbg(dev, "iommu add port:%d\n", portid);

			mtk_iommu_config_port(piommu, portid);

			if (i == 0)
				dev->archdata.dma_ops =
					piommu->dev->archdata.dma_ops;
		}
		i++;
	}

	spin_unlock_irqrestore(&priv->portlock, flags);

imudev:
	return 0;
}

static void mtk_iommu_detach_device(struct iommu_domain *domain,
				    struct device *dev)
{
}

static int mtk_iommu_map(struct iommu_domain *domain, unsigned long iova,
			 phys_addr_t paddr, size_t size, int prot)
{
	struct mtk_iommu_domain *priv = domain->priv;
	unsigned long flags;
	int ret;

	spin_lock_irqsave(&priv->pgtlock, flags);
	ret = m4u_map(priv, (unsigned int)iova, paddr, size, prot);
	mtk_iommu_invalidate_tlb(priv->piommuinfo, 0,
				 iova, iova + size - 1);
	spin_unlock_irqrestore(&priv->pgtlock, flags);

	return ret;
}

static size_t mtk_iommu_unmap(struct iommu_domain *domain,
			      unsigned long iova, size_t size)
{
	struct mtk_iommu_domain *priv = domain->priv;
	unsigned long flags;

	spin_lock_irqsave(&priv->pgtlock, flags);
	m4u_unmap(priv, (unsigned int)iova, size);
	mtk_iommu_invalidate_tlb(priv->piommuinfo, 0,
				 iova, iova + size - 1);
	spin_unlock_irqrestore(&priv->pgtlock, flags);

	return size;
}

static phys_addr_t mtk_iommu_iova_to_phys(struct iommu_domain *domain,
					  dma_addr_t iova)
{
	struct mtk_iommu_domain *priv = domain->priv;
	unsigned long flags;
	struct m4u_pte_info_t pte;

	spin_lock_irqsave(&priv->pgtlock, flags);
	m4u_get_pte_info(priv, (unsigned int)iova, &pte);
	spin_unlock_irqrestore(&priv->pgtlock, flags);

	return pte.pa;
}

static struct iommu_ops mtk_iommu_ops = {
	.domain_init = mtk_iommu_domain_init,
	.domain_destroy = mtk_iommu_domain_destroy,
	.attach_dev = mtk_iommu_attach_device,
	.detach_dev = mtk_iommu_detach_device,
	.map = mtk_iommu_map,
	.unmap = mtk_iommu_unmap,
	.map_sg = default_iommu_map_sg,
	.iova_to_phys = mtk_iommu_iova_to_phys,
	.pgsize_bitmap = SZ_4K | SZ_64K | SZ_1M | SZ_16M,
};

static const struct of_device_id mtk_iommu_of_ids[] = {
	{ .compatible = "mediatek,mt8173-iommu",
	  .data = &mtk_iommu_mt8173_cfg,
	},
	{}
};

static int mtk_iommu_probe(struct platform_device *pdev)
{
	int ret;
	struct iommu_domain *domain;
	struct mtk_iommu_domain *mtk_domain;
	struct mtk_iommu_info *piommu;
	struct iommu_dma_domain  *dom;
	const struct of_device_id *of_id;

	piommu = devm_kzalloc(&pdev->dev, sizeof(struct mtk_iommu_info),
			      GFP_KERNEL);
	if (!piommu)
		return -ENOMEM;

	pimudev = &pdev->dev;
	piommu->dev = &pdev->dev;

	of_id = of_match_node(mtk_iommu_of_ids, pdev->dev.of_node);
	if (!of_id)
		return -ENODEV;

	piommu->protect_va = devm_kmalloc(piommu->dev, MTK_PROTECT_PA_ALIGN*2,
					  GFP_KERNEL);
	if (!piommu->protect_va)
		goto protect_err;
	memset(piommu->protect_va, 0x55, MTK_PROTECT_PA_ALIGN*2);

	piommu->imucfg = (const struct mtk_iommu_cfg *)of_id->data;

	ret = mtk_iommu_parse_dt(pdev, piommu);
	if (ret) {
		dev_err(piommu->dev, "iommu dt parse fail\n");
		goto protect_err;
	}

	/* alloc memcache for level-2 pgt */
	piommu->m4u_pte_kmem = kmem_cache_create("m4u_pte", IMU_BYTES_PER_PTE,
						 IMU_BYTES_PER_PTE, 0, NULL);

	if (IS_ERR_OR_NULL(piommu->m4u_pte_kmem)) {
		dev_err(piommu->dev, "pte cached create fail %p\n",
			piommu->m4u_pte_kmem);
		goto protect_err;
	}

	arch_setup_dma_ops(piommu->dev, 0, (1ULL<<32) - 1, &mtk_iommu_ops, 0);

	dom = get_dma_domain(piommu->dev);
	domain = iommu_dma_raw_domain(dom);

	mtk_domain = domain->priv;
	mtk_domain->piommuinfo = piommu;

	if (!domain)
		goto pte_err;

	ret = mtk_iommu_hw_init(mtk_domain);
	if (ret < 0)
		goto hw_err;

	if (devm_request_irq(piommu->dev, piommu->irq,
			     mtk_iommu_isr, IRQF_TRIGGER_NONE,
			     "mtkiommu", (void *)domain)) {
		dev_err(piommu->dev, "IRQ request %d failed\n",
			piommu->irq);
		goto hw_err;
	}

	iommu_set_fault_handler(domain, mtk_iommu_fault_handler, piommu);

	dev_set_drvdata(piommu->dev, piommu);

	return 0;
hw_err:
	arch_teardown_dma_ops(piommu->dev);
pte_err:
	kmem_cache_destroy(piommu->m4u_pte_kmem);
protect_err:
	dev_err(piommu->dev, "probe error\n");
	return 0;
}

static int mtk_iommu_remove(struct platform_device *pdev)
{
	struct mtk_iommu_info *piommu =	dev_get_drvdata(&pdev->dev);

	arch_teardown_dma_ops(piommu->dev);
	kmem_cache_destroy(piommu->m4u_pte_kmem);

	return 0;
}

static struct platform_driver mtk_iommu_driver = {
	.probe	= mtk_iommu_probe,
	.remove	= mtk_iommu_remove,
	.driver	= {
		.name = "mtkiommu",
		.of_match_table = mtk_iommu_of_ids,
	}
};

static int __init mtk_iommu_init(void)
{
	return platform_driver_register(&mtk_iommu_driver);
}

subsys_initcall(mtk_iommu_init);

