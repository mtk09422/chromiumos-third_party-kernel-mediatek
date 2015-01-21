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
#include <linux/of_address.h>

#include "mtk_iommu_platform.h"
#include "mtk_iommu_reg_mt8135.h"

#define MTK_TFID(larbid, portid) ((larbid << 12) | (portid << 8))

static const struct mtk_iommu_port
			mtk_iommu_mt8135_port[] = {
	/*port name                   m4uid slaveid larbid portid tfid*/
	/*larb0*/
	{"M4U_PORT_VENC_REF_LUMA",        0,   0,    0,    0, MTK_TFID(6, 0)},
	{"M4U_PORT_VENC_REF_CHROMA",      0,   0,    0,    1, MTK_TFID(6, 1)},
	{"M4U_PORT_VENC_REC_FRM",         0,   0,    0,    2, MTK_TFID(6, 2)},
	{"M4U_PORT_VENC_RCPU",            0,   0,    0,    3, MTK_TFID(6, 3)},
	{"M4U_PORT_VENC_BSDMA",           0,   0,    0,    4, MTK_TFID(6, 4)},
	{"M4U_PORT_VENC_CUR_LUMA",        0,   0,    0,    5, MTK_TFID(6, 5)},
	{"M4U_PORT_VENC_CUR_CHROMA",      0,   0,    0,    6, MTK_TFID(6, 6)},
	{"M4U_PORT_VENC_RD_COMV",         0,   0,    0,    7, MTK_TFID(6, 7)},
	{"M4U_PORT_VENC_SV_COMV",         0,   0,    0,    8, MTK_TFID(6, 8)},

	/*larb1*/
	{"M4U_PORT_HW_VDEC_MC_EXT",      0,    0,    1,    0, MTK_TFID(5, 0)},
	{"M4U_PORT_HW_VDEC_PP_EXT",      0,    0,    1,    1, MTK_TFID(5, 1)},
	{"M4U_PORT_HW_VDEC_AVC_MV_EXT",  0,    0,    1,    2, MTK_TFID(5, 2)},
	{"M4U_PORT_HW_VDEC_PRED_RD_EXT", 0,    0,    1,    3, MTK_TFID(5, 3)},
	{"M4U_PORT_HW_VDEC_PRED_WR_EXT", 0,    0,    1,    4, MTK_TFID(5, 4)},
	{"M4U_PORT_HW_VDEC_VLD_EXT",     0,    0,    1,    5, MTK_TFID(5, 5)},
	{"M4U_PORT_HW_VDEC_VLD2_EXT",    0,    0,    1,    6, MTK_TFID(5, 6)},

	/*larb2*/
	{"M4U_PORT_ROT_EXT",             0,    0,   2,    0,   MTK_TFID(4, 0)},
	{"M4U_PORT_OVL_CH0",             0,    0,   2,    1,   MTK_TFID(4, 1)},
	{"M4U_PORT_OVL_CH1",             0,    0,   2,    2,   MTK_TFID(4, 2)},
	{"M4U_PORT_OVL_CH2",             0,    0,   2,    3,   MTK_TFID(4, 3)},
	{"M4U_PORT_OVL_CH3",             0,    0,   2,    4,   MTK_TFID(4, 4)},
	{"M4U_PORT_WDMA0",               0,    0,   2,    5,   MTK_TFID(4, 5)},
	{"M4U_PORT_WDMA1",               0,    0,   2,    6,   MTK_TFID(4, 6)},
	{"M4U_PORT_RDMA0",               0,    0,   2,    7,   MTK_TFID(4, 7)},
	{"M4U_PORT_RDMA1",               0,    0,   2,    8,   MTK_TFID(4, 8)},
	{"M4U_PORT_CMDQ",                0,    0,   2,    9,   MTK_TFID(4, 9)},
	{"M4U_PORT_DBI",                 0,    0,   2,    10,  MTK_TFID(4, 10)},
	{"M4U_PORT_G2D",                 0,    0,   2,    11,  MTK_TFID(4, 11)},

	/*larb3*/
	{"M4U_PORT_JPGDEC_WDMA",         0,    0,   3,    0,   MTK_TFID(3, 0)},
	{"M4U_PORT_JPGENC_RDMA",         0,    0,   3,    1,   MTK_TFID(3, 1)},
	{"M4U_PORT_IMGI",                0,    0,   3,    2,   MTK_TFID(3, 2)},
	{"M4U_PORT_VIPI",                0,    0,   3,    3,   MTK_TFID(3, 3)},
	{"M4U_PORT_VIP2I",               0,    0,   3,    4,   MTK_TFID(3, 4)},
	{"M4U_PORT_DISPO",               0,    0,   3,    5,   MTK_TFID(3, 5)},
	{"M4U_PORT_DISPCO",              0,    0,   3,    6,   MTK_TFID(3, 6)},
	{"M4U_PORT_DISPVO",              0,    0,   3,    7,   MTK_TFID(3, 7)},
	{"M4U_PORT_VIDO",                0,    0,   3,    8,   MTK_TFID(3, 8)},
	{"M4U_PORT_VIDCO",               0,    0,   3,    9,   MTK_TFID(3, 9)},
	{"M4U_PORT_DIDVO",               0,    0,   3,    10,  MTK_TFID(3, 10)},
	{"M4U_PORT_GDMA_SMI_WR",         0,    0,   3,    12,  MTK_TFID(3, 11)},
	{"M4U_PORT_JPGDEC_BSDMA",        0,    0,   3,    13,  MTK_TFID(3, 12)},
	{"M4U_PORT_JPGENC_BSDMA",        0,    0,   3,    14,  MTK_TFID(3, 13)},

	/*larb4*/
	{"M4U_PORT_GDMA_SMI_RD",         0,    0,   4,    0,   MTK_TFID(2, 0)},
	{"M4U_PORT_IMGCI",               0,    0,   4,    1,   MTK_TFID(2, 1)},
	{"M4U_PORT_IMGO",                0,    0,   4,    2,   MTK_TFID(2, 2)},
	{"M4U_PORT_IMG2O",               0,    0,   4,    3,   MTK_TFID(2, 3)},
	{"M4U_PORT_LSCI",                0,    0,   4,    4,   MTK_TFID(2, 4)},
	{"M4U_PORT_FLKI",                0,    0,   4,    5,   MTK_TFID(2, 5)},
	{"M4U_PORT_LCEI",                0,    0,   4,    6,   MTK_TFID(2, 6)},
	{"M4U_PORT_LCSO",                0,    0,   4,    7,   MTK_TFID(2, 7)},
	{"M4U_PORT_ESFKO",               0,    0,   4,    8,   MTK_TFID(2, 8)},
	{"M4U_PORT_AAO",                 0,    0,   4,    9,   MTK_TFID(2, 9)},

	/*larb5*/
	{"M4U_PORT_AUDIO",               0,    0,   5,    0,   MTK_TFID(1, 0)},

	/*larb6*/
	{"M4U_PORT_GCPU",                0,    0,   6,    0,   MTK_TFID(0, 0)},
};

static inline int mt8135_get_port_by_tfid(const struct mtk_iommu_info *pimu,
					  int tf_id)
{
	const struct mtk_iommu_cfg *pimucfg = pimu->imucfg;
	int i;
	unsigned int portid = 0;

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

static inline void mt8135_iommu_clear_intr(void __iomem *m4u_base)
{
	u32 temp;

	temp = readl(m4u_base + REG_MMU_INT_CONTROL) | F_INT_CLR_BIT;
	writel(temp, m4u_base + REG_MMU_INT_CONTROL);
}

static inline int mt8135_iommu_config_port(struct mtk_iommu_info *piommu,
					   int portid)
{
	void __iomem *m4u_ao_common;
	u32 sec_con_val, regval;

	m4u_ao_common = piommu->smi_common_ao_base;

	dev_dbg(piommu->dev, "config_port, port=%s\n",
		mtk_iommu_get_port_name(piommu, portid));

	if (portid > piommu->imucfg->m4u_port_nr) {
		dev_err(piommu->dev, "config port id err %d(%d)\n",
			portid, piommu->imucfg->m4u_port_nr);
		return -EINVAL;
	}

	sec_con_val = F_SMI_SECUR_CON_VIRTUAL(portid);
	sec_con_val |= F_SMI_SECUR_CON_DOMAIN(portid, 3);

	if (portid != piommu->imucfg->m4u_gpu_port) {
		regval = readl(m4u_ao_common +
			       REG_SMI_SECUR_CON_OF_PORT(portid));
		regval = (regval
			  &(~F_SMI_SECUR_CON_MASK(portid)))
			 | sec_con_val;
		writel(regval,
		       m4u_ao_common + REG_SMI_SECUR_CON_OF_PORT(portid));
	} else {
		writel(sec_con_val,
		       m4u_ao_common + REG_SMI_SECUR_CON_G3D);
	}
	return 0;
}

static int mt8135_iommu_invalidate_tlb(const struct mtk_iommu_info *piommu,
				       unsigned int m4u_id, int isinvall,
				       unsigned int iova_start,
				       unsigned int iova_end)
{
	u32 reg = 0;
	unsigned int cnt = 0;
	void __iomem *m4u_base_g = piommu->m4u_g_base;
	void __iomem *m4u_base_L2 = piommu->m4u_L2_base;

	if (!isinvall && (iova_end < iova_start))
		isinvall = 1;

	reg = F_MMUG_CTRL_INV_EN2;
	if (m4u_id == M4U_ID_0) {
		reg |= F_MMUG_CTRL_INV_EN0;
	} else if (m4u_id == M4U_ID_1) {
		reg |= F_MMUG_CTRL_INV_EN1;
	} else {
		reg |= F_MMUG_CTRL_INV_EN0;
		reg |= F_MMUG_CTRL_INV_EN1;
	}
	writel(reg, m4u_base_g + REG_MMUG_CTRL);

	if (isinvall) {
		writel(F_MMUG_INV_ALL, m4u_base_g + REG_MMUG_INVLD);

		while (!(readl(m4u_base_L2 + REG_L2_GDC_STATE)
			       & F_L2_GDC_ST_EVENT_MSK)) {
			if (cnt++ >= 10000) {
				dev_warn(piommu->dev, "invalid all don't done\n");
				cnt = 0;
			}
		}
		reg = readl(m4u_base_L2 + REG_L2_GDC_STATE);
		reg = reg & (~F_L2_GDC_ST_EVENT_MSK);
		writel(reg, m4u_base_L2 + REG_L2_GDC_STATE);
	} else {
		writel(iova_start & (~0xfff), m4u_base_g + REG_MMUG_INVLD_SA);
		writel(iova_end & (~0xfff), m4u_base_g + REG_MMUG_INVLD_EA);
		writel(F_MMUG_INV_RANGE, m4u_base_g + REG_MMUG_INVLD);

		while (!(readl(m4u_base_L2 + REG_L2_GPE_STATUS)
			       & F_L2_GPE_ST_RANGE_INV_DONE)) {
			if (cnt++ >= 10000) {
				dev_warn(piommu->dev, "invalid don't done\n");
				cnt = 0;
			}
		}
		reg = readl(m4u_base_L2 + REG_L2_GPE_STATUS);
		reg = reg & (~F_L2_GPE_ST_RANGE_INV_DONE);
		writel(reg, m4u_base_L2 + REG_L2_GPE_STATUS);
	}
	return 0;
}

static int mt8135_iommu_fault_handler(struct iommu_domain *imudomain,
				      struct device *dev, unsigned long iova,
				      int m4u_index, void *pimu)
{
	void __iomem *m4u_base;
	unsigned int *ptestart;
	unsigned int faultmva;
	u32 intrsrc, port_regval;
	struct mtk_iommu_info *piommu = pimu;
	struct mtk_iommu_domain *mtkdomain = imudomain->priv;

	m4u_base = piommu->m4u[m4u_index].m4u_base;
	intrsrc = readl(m4u_base + REG_MMU_FAULT_ST) & 0xff;
	port_regval = readl(m4u_base + REG_MMU_INT_ID);
	faultmva = readl(m4u_base + REG_MMU_FAULT_VA);

	if (0 == intrsrc) {
		dev_err_ratelimited(dev, "m4uid=%d REG_MMU_FAULT_ST=0x0 fail\n",
				    m4u_index);
		goto imufault;
	}

	if (intrsrc & F_INT_TRANSLATION_FAULT) {
		unsigned int m4u_port = 0;

		m4u_port = mt8135_get_port_by_tfid(piommu, port_regval);

		dev_err_ratelimited(dev, "fault:port=%s(%d),iova=0x%x\n",
				    mtk_iommu_get_port_name(piommu, m4u_port),
				    m4u_port, faultmva);

		if (faultmva < piommu->imucfg->iova_size &&
		    faultmva > piommu->imucfg->iova_base) {
			ptestart = (unsigned int *)(mtkdomain->pgd
						+ (faultmva >> PAGE_SHIFT));
			dev_err_ratelimited(dev, "pgt@0x%x:0x%x,0x%x,0x%x\n",
					    faultmva, ptestart[-1],
					    ptestart[0], ptestart[1]);
		}
	}

	if (intrsrc & F_INT_INVALID_PHYSICAL_ADDRESS_FAULT) {
		if (!(intrsrc & F_INT_TRANSLATION_FAULT)) {
			if (faultmva < piommu->imucfg->iova_size &&
			    faultmva > piommu->imucfg->iova_base) {
				ptestart = (unsigned int *)(mtkdomain->pgd
						+ (faultmva >> PAGE_SHIFT));
				dev_err_ratelimited(
					dev,
					"pagetable @ 0x%x: 0x%x,0x%x,0x%x\n",
					faultmva,
					ptestart[-1], ptestart[0], ptestart[1]);
			}
		}
		dev_err_ratelimited(dev, "invalid PA:0x%x->0x%x\n", faultmva,
				    readl(m4u_base + REG_MMU_INVLD_PA));
	}

	if (intrsrc & F_INT_TLB_MULTI_HIT_FAULT)
		dev_err_ratelimited(dev, "multi-hit!\n");

	if (intrsrc & F_INT_ENTRY_REPLACEMENT_FAULT)
		dev_err_ratelimited(dev, "Entry replacement fault!");

	if (intrsrc & F_INT_TABLE_WALK_FAULT)
		dev_err_ratelimited(dev, "Table walk fault!\n");

	if (intrsrc & F_INT_TLB_MISS_FAULT)
		dev_err_ratelimited(dev, "TLB miss fault!\n");

	if (intrsrc & F_INT_PFH_DMA_FIFO_OVERFLOW)
		dev_err_ratelimited(dev, "Prefetch DMA fifo overflow fault!\n");

	mt8135_iommu_invalidate_tlb(piommu, m4u_index, 1, 0, 0);

imufault:
	mt8135_iommu_clear_intr(m4u_base);

	return 0;
}

static int mt8135_iommu_pte_init(struct mtk_iommu_info *piommu)
{
	return 0;
}

static int mt8135_iommu_parse_dt(struct platform_device *pdev,
				 struct mtk_iommu_info *piommu)
{
	struct device *piommudev = &pdev->dev;
	struct device_node *ofnode;
	struct resource *res;
	unsigned int mtk_iommu_cell = 0;

	ofnode = piommudev->of_node;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	piommu->m4u_g_base = devm_ioremap_resource(&pdev->dev, res);
	res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	piommu->smi_common_ao_base = devm_ioremap_resource(&pdev->dev, res);

	if (IS_ERR(piommu->m4u_g_base) ||
	    IS_ERR(piommu->smi_common_ao_base)) {
		dev_err(piommudev, "iommu base err g-0x%p AO-0x%p\n",
			piommu->m4u_g_base,
			piommu->smi_common_ao_base);
		goto iommu_dts_err;
	}

	piommu->m4u_L2_base = piommu->m4u_g_base + piommu->imucfg->L2_offset;
	piommu->m4u[0].m4u_base = piommu->m4u_g_base
				+ piommu->imucfg->m4u0_offset;
	piommu->m4u[1].m4u_base = piommu->m4u_g_base
				+ piommu->imucfg->m4u1_offset;

	piommu->m4u[0].irq = platform_get_irq(pdev, 0);
	piommu->m4u[1].irq = platform_get_irq(pdev, 1);
	if (piommu->m4u[0].irq < 0) {/*m4u1 irq err is allowed*/
		dev_err(piommudev, "iommu irq err %d %d\n",
			piommu->m4u[0].irq, piommu->m4u[1].irq);
		goto iommu_dts_err;
	}

	piommu->m4u_infra_clk = devm_clk_get(piommudev, "m4u");
	piommu->smi_infra_clk = devm_clk_get(piommudev, "smi");
	if (IS_ERR(piommu->m4u_infra_clk) ||
	    IS_ERR(piommu->smi_infra_clk)) {
		dev_err(piommudev, "clk err 0x%p, smi 0x%p\n",
			piommu->m4u_infra_clk, piommu->smi_infra_clk);
		goto iommu_dts_err;
	}

	of_property_read_u32(ofnode, "iommu-cells", &mtk_iommu_cell);
	if (mtk_iommu_cell != 1) {
		dev_err(piommudev, "mtk-cell fail:%d\n", mtk_iommu_cell);
		goto iommu_dts_err;
	}

	return 0;

iommu_dts_err:
	return -EINVAL;
}

static int mt8135_iommu_hw_init(const struct mtk_iommu_domain *mtkdomain)
{
	struct mtk_iommu_info *piommu = mtkdomain->piommuinfo;
	unsigned int i = 0;
	u32 regval, protectpa;
	int ret = 0;

	ret = clk_prepare_enable(piommu->m4u_infra_clk);
	if (ret) {
		dev_err(piommu->dev, "m4u clk enable error\n");
		return -ENODEV;
	}
	ret = clk_prepare_enable(piommu->smi_infra_clk);
	if (ret) {
		clk_disable_unprepare(piommu->m4u_infra_clk);
		dev_err(piommu->dev, "smi clk enable error\n");
		return -ENODEV;
	}

	for (i = 0; i < piommu->imucfg->larb_nr; i++) {
		writel(0x66666666,
		       piommu->smi_common_ao_base+REG_SMI_SECUR_CON(i));
	}

	regval = F_MMUG_L2_SEL_FLUSH_EN |
		F_MMUG_L2_SEL_L2_ULTRA |
		F_MMUG_L2_SEL_L2_BUS_SEL_EMI;
	writel(regval, piommu->m4u_g_base + REG_MMUG_L2_SEL);

	writel(F_MMUG_DCM_ON, piommu->m4u_g_base + REG_MMUG_DCM);

	writel(mtkdomain->pgd_pa, piommu->m4u_g_base + REG_MMUG_PT_BASE);

	regval = F_L2_GDC_LOCK_TH(3);
	writel(regval, piommu->m4u_L2_base + REG_L2_GDC_OP);

	for (i = 0; i < piommu->imucfg->m4u_nr; i++) {
		void __iomem *gm4ubaseaddr = piommu->m4u[i].m4u_base;

		protectpa = (unsigned int)virt_to_phys(piommu->protect_va);
		protectpa = ALIGN(protectpa, MTK_PROTECT_PA_ALIGN);

		regval = F_MMU_CTRL_TF_PROT_VAL(2) |
			 F_MMU_CTRL_COHERE_EN;
		writel(regval, gm4ubaseaddr + REG_MMU_CTRL_REG);

		/* enable interrupt control except "Same VA-to-PA test" */
		writel(0x7F, gm4ubaseaddr + REG_MMU_INT_CONTROL);

		/* protect memory,hw will write here while translation fault */
		writel(protectpa, gm4ubaseaddr + REG_MMU_IVRP_PADDR);
	}
	return 0;
}

static int mt8135_iommu_map(struct mtk_iommu_domain *mtkdomain,
			    unsigned int iova, phys_addr_t paddr, size_t size)
{
	unsigned int i;
	unsigned int page_num = DIV_ROUND_UP(size, PAGE_SIZE);
	unsigned int *pgt_base_iova;
	unsigned int pabase = (unsigned int)paddr;

	/*spinlock in upper function*/
	pgt_base_iova = (unsigned int *)(mtkdomain->pgd + (iova >> PAGE_SHIFT));
	for (i = 0; i < page_num; i++) {
		pgt_base_iova[i] =
		    pabase | F_DESC_VALID | F_DESC_NONSEC(1) | F_DESC_SHARE(0);
		pabase += PAGE_SIZE;
	}
	mt8135_iommu_invalidate_tlb(mtkdomain->piommuinfo, M4U_ID_ALL, 0,
				    iova, iova + size - 1);
	return 0;
}

static size_t mt8135_iommu_unmap(struct mtk_iommu_domain *mtkdomain,
				 unsigned int iova, size_t size)
{
	unsigned int page_num = DIV_ROUND_UP(size, PAGE_SIZE);
	unsigned int *pgt_base_iova;

	/*spinlock in upper function*/
	pgt_base_iova = (unsigned int *)(mtkdomain->pgd + (iova >> PAGE_SHIFT));
	memset(pgt_base_iova, 0x0, page_num * sizeof(int));

	mt8135_iommu_invalidate_tlb(mtkdomain->piommuinfo, M4U_ID_ALL, 0,
				    iova, iova + size - 1);
	return 0;
}

static phys_addr_t mt8135_iommu_iova_to_phys(struct mtk_iommu_domain *mtkdomain,
					     unsigned int iova)
{
	unsigned int phys;

	phys = *(unsigned int *)(mtkdomain->pgd + (iova >> PAGE_SHIFT));
	return (phys_addr_t)phys;
}

const struct mtk_iommu_cfg mtk_iommu_mt8135_cfg = {
	.m4u_nr = 2,
	.m4u0_offset = 0x200,
	.m4u1_offset = 0x800,
	.L2_offset = 0x100,
	.larb_nr = 5,
	.m4u_gpu_port = 53,
	.m4u_port_nr = sizeof(mtk_iommu_mt8135_port)/
				sizeof(struct mtk_iommu_port),
	.iova_base = 0,
	.iova_size = MTK_PGT_SIZE * SZ_1K,
	.pport = mtk_iommu_mt8135_port,
	.pte_init = mt8135_iommu_pte_init,
	.dt_parse = mt8135_iommu_parse_dt,
	.hw_init = mt8135_iommu_hw_init,
	.map = mt8135_iommu_map,
	.unmap = mt8135_iommu_unmap,
	.iova_to_phys = mt8135_iommu_iova_to_phys,
	.config_port = mt8135_iommu_config_port,
	.faulthandler = mt8135_iommu_fault_handler,
};
