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
#include <linux/pm_runtime.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/mm.h>
#include <linux/iommu.h>
#include <linux/errno.h>
#include <linux/list.h>
#include <linux/memblock.h>
#include <linux/export.h>
#include <asm/cacheflush.h>
#include <asm/dma-iommu.h>

#include <linux/io.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>

#include "mtk_iommu_platform.h"
#include "mtk_iommu_reg.h"
#include "mtk_iommu.h"

static struct mtk_iommu_info *piommu_info;

#define iova_pteaddr(piommu, iova) (piommu->pgt_baseva+((iova)>>PAGE_SHIFT))

static int m4u_port_2_larb_id(unsigned int port)
{
	int i;

	for (i = piommu_info->iommu_cfg->larb_nr - 1; i >= 0; i--) {
		if (port >= piommu_info->iommu_cfg->m4u_port_in_larbx[i])
			return i;
	}
	return 0;
}

static inline int larb_2_m4u_id(int larb)
{
	return !!piommu_info->larb_port_mapping[larb];
}

static inline int m4u_port_2_m4u_id(unsigned int port)
{
	return larb_2_m4u_id(m4u_port_2_larb_id(port));
}

static inline int m4u_port_2_smi_port(unsigned int port)
{
	int larb = m4u_port_2_larb_id(port);
	int local_port = 0;
	int i;

	for (i = piommu_info->iommu_cfg->larb_nr - 1; i >= 0; i--) {
		if (port >= piommu_info->iommu_cfg->m4u_port_in_larbx[i]) {
			local_port = port -
				piommu_info->iommu_cfg->m4u_port_in_larbx[i];
			break;
		}
	}

	return piommu_info->iommu_cfg->m4u_port_in_larbx[larb] + local_port;
}

static unsigned int larb_port_2_m4u_port(unsigned int larb,
					 unsigned int local_port)
{
	return piommu_info->iommu_cfg->m4u_port_in_larbx[larb] + local_port;
}

static const char *m4u_get_port_name(const struct mtk_iommu_info *piommu,
				     unsigned int portid)
{
	const struct mtk_port *pcurport = NULL;

	if (unlikely(!piommu)) {
		pr_err("iommu info portName L%d\n", __LINE__);
		return "ErrName";
	}

	pcurport = piommu->iommu_cfg->pport + portid;

	if (portid < piommu->iommu_cfg->m4u_port_nr && pcurport) {
		return pcurport->port_name;
	} else {
		pr_err("portid error %d\n", portid);
		return "UNKOWN_PORT";
	}
}

static void __iomem *m4u_get_base(const struct mtk_iommu_info *piommu,
				  unsigned int portid)
{
	return piommu->m4u[m4u_port_2_m4u_id(portid)].m4u_base;
}

static void m4u_clear_intr(void __iomem *m4u_base)
{
	unsigned int temp;

	temp = readl(m4u_base+REG_MMU_INT_CONTROL) | F_INT_CLR_BIT;
	writel(temp, m4u_base+REG_MMU_INT_CONTROL);
}

#ifdef CONFIG_MTK_IOMMU_DEBUG
int mtk_iommu_dump_info(void)
{
	int i;
	const struct mtk_iommu_info *piommu = piommu_info;
	const struct mtk_port *pcurport;

	if (unlikely(!piommu)) {
		pr_err("iommu info dump L%d err\n", __LINE__);
		return -EFAULT;
	}

	pr_info("m4u g base 0x%p, m4u0 0x%p m4u1 0x%p L2 0x%p\n",
		piommu->m4u_g_base, piommu->m4u[0].m4u_base,
		piommu->m4u[1].m4u_base, piommu->m4u_L2_base);

	pr_info("smi AO 0x%p Ext 0x%p\n",
		piommu->smi_common_ao_base,
		piommu->smi_common_ext_base);
	pr_info("mtk irq %d %d\n",
		piommu->m4u[0].irq, piommu->m4u[1].irq);

	pr_info("larbport %d %d %d %d %d\n",
		piommu->larb_port_mapping[0],
		piommu->larb_port_mapping[1],
		piommu->larb_port_mapping[2],
		piommu->larb_port_mapping[3],
		piommu->larb_port_mapping[4]
		);
	pr_info("iova size 0x%x\n", piommu->iova_size);

	pr_info("m4u nr %d larb nr %d larbnrreal %d\n",
		piommu->iommu_cfg->m4u_nr,
		piommu->iommu_cfg->larb_nr,
		piommu->iommu_cfg->larb_nr_real);
	pr_info("m4u tf larb id %d\n", piommu->iommu_cfg->m4u_tf_larbid);
	pr_info("main tlb nr %d, pfh %d, seqnr %d\n",
		piommu->iommu_cfg->main_tlb_nr,
		piommu->iommu_cfg->pfn_tlb_nr,
		piommu->iommu_cfg->m4u_seq_nr);

	pr_info("larbport %d %d %d %d %d %d %d\n",
		piommu->iommu_cfg->m4u_port_in_larbx[0],
		piommu->iommu_cfg->m4u_port_in_larbx[1],
		piommu->iommu_cfg->m4u_port_in_larbx[2],
		piommu->iommu_cfg->m4u_port_in_larbx[3],
		piommu->iommu_cfg->m4u_port_in_larbx[4],
		piommu->iommu_cfg->m4u_port_in_larbx[5],
		piommu->iommu_cfg->m4u_port_in_larbx[6]
		);
	pr_info("special port: gpu %d\n",
		piommu->iommu_cfg->m4u_gpu_port);
	pr_info("max portid %d\n", piommu->iommu_cfg->m4u_port_nr);

	for (i = 0; i < piommu->iommu_cfg->m4u_port_nr; i++) {
		pcurport = piommu->iommu_cfg->pport + i;
		pr_info("port larb%d portid%02d %30s\n",
			pcurport->larbid, pcurport->portid,
			pcurport->port_name);
	}

	return 0;
}
#endif

static int m4u_invalid_tlb(unsigned int m4u_id,
			   int isinvall, unsigned int iova_start,
			   unsigned int iova_end)
{
	unsigned int reg = 0;
	void __iomem *m4u_base_g = piommu_info->m4u_g_base;
	void __iomem *m4u_base_L2 = piommu_info->m4u_L2_base;

	if (!isinvall && (iova_end < iova_start)) {
		pr_info("invalid tlb fail: s=0x%x,e=0x%x\n", iova_start,
			iova_end);
		isinvall = 1;
	}

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
	} else {
		writel(iova_start & (~0xfff), m4u_base_g + REG_MMUG_INVLD_SA);
		writel(iova_end & (~0xfff), m4u_base_g + REG_MMUG_INVLD_EA);
		writel(F_MMUG_INV_RANGE, m4u_base_g + REG_MMUG_INVLD);
	}

	if (isinvall) {
		while (!(readl(m4u_base_L2 + REG_L2_GDC_STATE)
			       & F_L2_GDC_ST_EVENT_MSK))
			;
		reg = readl(m4u_base_L2+REG_L2_GDC_STATE);
		reg = reg & (~F_L2_GDC_ST_EVENT_MSK);
		writel(reg, m4u_base_L2+REG_L2_GDC_STATE);
	} else {
		while (!(readl(m4u_base_L2+REG_L2_GPE_STATUS)
			       & F_L2_GPE_ST_RANGE_INV_DONE))
			;
		reg = readl(m4u_base_L2+REG_L2_GPE_STATUS);
		reg = reg & (~F_L2_GPE_ST_RANGE_INV_DONE);
		writel(reg, m4u_base_L2+REG_L2_GPE_STATUS);
	}

	return 0;
}

static inline int m4u_hw_config_port(struct mtk_iommu_info *piommu,
				     int portid, int vir, int sec)
{
	unsigned int sec_con_val;
	void __iomem *m4u_base;
	void __iomem *m4u_ao_common;

	m4u_base = m4u_get_base(piommu, portid);
	m4u_ao_common = piommu->smi_common_ao_base;

	pr_debug("m4u_hwconfig_port, port=%s (%d), Vir=%d, Sec=%d\n",
		 m4u_get_port_name(piommu, portid), portid, vir, sec);

	if (portid > piommu->iommu_cfg->m4u_port_nr) {
		pr_err("config port id err %d(%d), 0x%p\n", portid,
		       piommu->iommu_cfg->m4u_port_nr, m4u_base);
		return -EINVAL;
	}

	sec_con_val = 0;
	if (vir)
		sec_con_val |= F_SMI_SECUR_CON_VIRTUAL(portid);

	if (sec)
		sec_con_val |= F_SMI_SECUR_CON_SECURE(portid);

	sec_con_val |= F_SMI_SECUR_CON_DOMAIN(portid, 3);

	if (portid != piommu->iommu_cfg->m4u_gpu_port) {
		unsigned int regval;

		regval = readl(m4u_ao_common +
			REG_SMI_SECUR_CON_OF_PORT(portid));
		regval = (regval&(~F_SMI_SECUR_CON_MASK(portid)))|sec_con_val;
		writel(regval, m4u_ao_common +
		       REG_SMI_SECUR_CON_OF_PORT(portid));
	} else
		writel(sec_con_val,
		       m4u_ao_common + REG_SMI_SECUR_CON_G3D);
	return 0;
}

static int m4u_config_port(struct mtk_iommu_domain *priv,
			   struct mtkmmu_drvdata *data)
{
	struct mtkmmu_drvdata *pcurportcfg;
	struct mtk_iommu_info *piommu = priv->piommuinfo;

	if (unlikely(data->portid > piommu->iommu_cfg->m4u_port_nr)) {
		pr_err("iommu info err %d L%d\n", data->portid, __LINE__);
		return -EINVAL;
	}

	pcurportcfg = &piommu->portcfg[data->portid];

	if (pcurportcfg->virtuality == data->virtuality &&
	    pcurportcfg->portid == data->portid) {
		pr_debug("cfg port %s %d is same,skip\n",
			 m4u_get_port_name(piommu, data->portid),
			 data->virtuality);
	} else {
		pcurportcfg->virtuality = !!data->virtuality;
		pcurportcfg->portid = data->portid;
		pcurportcfg->fault_handler = data->fault_handler;
		pcurportcfg->faultdata = data->faultdata;

		m4u_hw_config_port(piommu, pcurportcfg->portid,
				   pcurportcfg->virtuality, 0);
	}
	return 0;
}


static irqreturn_t MTK_M4U_isr(int irq, void *dev_id)
{
	void __iomem *m4u_base;
	unsigned int m4u_index;
	unsigned int intrsrc, faultmva, port_regval;
	int portid, larbid;
	struct mtk_iommu_info *piommu_info = (struct mtk_iommu_info *)dev_id;

	if (unlikely(!piommu_info)) {
		pr_err("iommu info err irq L%d\n", __LINE__);
		return IRQ_HANDLED;
	}

	m4u_index = irq - piommu_info->m4u[0].irq;
	if (m4u_index > M4U_ID_ALL) {
		pr_err("m4u irq Error %d\n", m4u_index);
		return IRQ_HANDLED;
	}

	m4u_base = piommu_info->m4u[m4u_index].m4u_base;

	intrsrc = readl(m4u_base+REG_MMU_FAULT_ST) & 0xff;
	faultmva = readl(m4u_base+REG_MMU_FAULT_VA);
	port_regval = readl(m4u_base+REG_MMU_INT_ID);

	if (0 == intrsrc) {
		pr_err("<m4u>warning:MTK_M4U_isr, larbID=%d but REG_MMU_FAULT_ST=0x0\n",
		       m4u_index);
		m4u_clear_intr(m4u_base);
		return IRQ_HANDLED;
	}

	if (intrsrc&F_INT_TRANSLATION_FAULT) {
		unsigned int m4u_port = 0;

		portid = F_INT_ID_TF_PORT_ID(port_regval);
		larbid = piommu_info->iommu_cfg->m4u_tf_larbid
				- F_INT_ID_TF_LARB_ID(port_regval);

		m4u_port = larb_port_2_m4u_port(larbid, portid);

		pr_err("<m4u>translation fault: larb=%d,port=%s,iova=0x%x\n",
		       larbid, m4u_get_port_name(piommu_info, m4u_port),
		       faultmva);

		if (faultmva < piommu_info->iova_size &&
		    faultmva > piommu_info->iova_base) {
			unsigned int *ptestart;

			ptestart = iova_pteaddr(piommu_info, faultmva);
			pr_err("<m4u>pagetable @ 0x%x: 0x%x,0x%x,0x%x\n",
			       faultmva,
			       ptestart[-1], ptestart[0], ptestart[1]);
		}

		 /*user fault handle*/
		 if (piommu_info->portcfg[m4u_port].fault_handler)
			piommu_info->portcfg[m4u_port].fault_handler(
				m4u_port, faultmva,
				piommu_info->portcfg[m4u_port].faultdata);
	}

	if (intrsrc & F_INT_TLB_MULTI_HIT_FAULT)
		pr_err("<m4u>multi-hit error!\n");

	if (intrsrc & F_INT_INVALID_PHYSICAL_ADDRESS_FAULT) {
		if (!(intrsrc & F_INT_TRANSLATION_FAULT)) {
			if (faultmva < piommu_info->iova_size &&
			    faultmva > piommu_info->iova_base) {
				unsigned int *ptestart;

				ptestart = iova_pteaddr(piommu_info, faultmva);
				pr_err("<m4u>pagetable @ 0x%x: 0x%x,0x%x,0x%x\n",
				       faultmva, ptestart[-1],
				       ptestart[0], ptestart[1]);
			}

			pr_err("<m4u>invalid PA:0x%x->0x%x\n", faultmva,
			       readl(m4u_base+REG_MMU_INVLD_PA));
		} else {
			pr_err("<m4u>invalid PA:0x%x->0x%x\n", faultmva,
			       readl(m4u_base+REG_MMU_INVLD_PA));
		}
	}
	if (intrsrc & F_INT_ENTRY_REPLACEMENT_FAULT)
		pr_err("<m4u>error: Entry replacement fault!No free TLB");

	if (intrsrc & F_INT_TABLE_WALK_FAULT)
		pr_err("<m4u>error:Table walk fault! pageTable start addr\n");

	if (intrsrc & F_INT_TLB_MISS_FAULT)
		pr_err("<m4u>error:TLB miss fault!\n");

	if (intrsrc & F_INT_PFH_DMA_FIFO_OVERFLOW)
		pr_err("<m4u>error:Prefetch DMA fifo overflow fault!\n");

	m4u_invalid_tlb(m4u_index, 1, 0, 0);

	m4u_clear_intr(m4u_base);

	return IRQ_HANDLED;
}

static int m4u_hw_init(struct mtk_iommu_info *piommu_info)
{
	unsigned int i = 0;
	unsigned regval;
	unsigned int protectpa;
	int ret = 0;

	ret = clk_prepare_enable(piommu_info->m4u_infra_clk);
	if (ret) {
		pr_err("m4u clk enable error\n");
		return -ENODEV;
	}
	ret = clk_prepare_enable(piommu_info->smi_infra_clk);
	if (ret) {
		clk_disable_unprepare(piommu_info->m4u_infra_clk);
		pr_err("smi clk enable error\n");
		return -ENODEV;
	}

	/* SMI registers bus sel*/
	regval = F_SMI_BUS_SEL_larb0(larb_2_m4u_id(0))
	    | F_SMI_BUS_SEL_larb1(larb_2_m4u_id(1))
	    | F_SMI_BUS_SEL_larb2(larb_2_m4u_id(2))
	    | F_SMI_BUS_SEL_larb3(larb_2_m4u_id(3))
	    | F_SMI_BUS_SEL_larb4(larb_2_m4u_id(4));

	writel(regval, piommu_info->smi_common_ext_base + REG_SMI_BUS_SEL);
	pr_debug("hw init bus:0x%x == 0x%x\n", readl(
		 piommu_info->smi_common_ext_base + REG_SMI_BUS_SEL), regval);

	for (i = 0; i < piommu_info->iommu_cfg->larb_nr; i++) {
		writel(0x66666666,
		       piommu_info->smi_common_ao_base+REG_SMI_SECUR_CON(i));
	}

	/* m4u global registers */
	regval = F_MMUG_L2_SEL_FLUSH_EN(1)
		| F_MMUG_L2_SEL_L2_ULTRA(1)
		| F_MMUG_L2_SEL_L2_SHARE(0)
		| F_MMUG_L2_SEL_L2_BUS_SEL(1);
	writel(regval, piommu_info->m4u_g_base + REG_MMUG_L2_SEL);

	writel(F_MMUG_DCM_ON(1), piommu_info->m4u_g_base + REG_MMUG_DCM);

	/* L2 registers */
	regval = F_L2_GDC_BYPASS(0)
		| F_L2_GDC_PERF_MASK(GDC_PERF_MASK_HIT_MISS)
		| F_L2_GDC_LOCK_ALERT_DIS(0)
		| F_L2_GDC_LOCK_TH(3)
		| F_L2_GDC_PAUSE_OP(GDC_NO_PAUSE);
	writel(regval, piommu_info->m4u_L2_base + REG_L2_GDC_OP);

	/* m4u registers */
	for (i = 0; i < piommu_info->iommu_cfg->m4u_nr; i++) {
		unsigned int irq = piommu_info->m4u[i].irq;
		void __iomem *gm4ubaseaddr = piommu_info->m4u[i].m4u_base;

		protectpa = (unsigned int)virt_to_phys(piommu_info->protect_va);
		protectpa = (protectpa + MTK_PROTECT_PA_ALIGN - 1)
			    & (MTK_PROTECT_PA_ALIGN - 1);

		regval = F_MMU_CTRL_PFH_DIS(0)
			    | F_MMU_CTRL_TLB_WALK_DIS(0)
			    | F_MMU_CTRL_MONITOR_EN(0)
			    | F_MMU_CTRL_MONITOR_CLR(0)
			    | F_MMU_CTRL_PFH_RT_RPL_MODE(0)
			    | F_MMU_CTRL_TF_PROT_VAL(2)
			    | F_MMU_CTRL_COHERE_EN(1);

		writel(regval, gm4ubaseaddr + REG_MMU_CTRL_REG);

		/* enable interrupt control except "Same VA-to-PA test" */
		writel(0x7F, gm4ubaseaddr + REG_MMU_INT_CONTROL);

		/*protect memory,hw will write here while translation fault*/
		writel(protectpa, gm4ubaseaddr + REG_MMU_IVRP_PADDR);

		/*enable irq*/
		if (request_irq(irq, MTK_M4U_isr, IRQF_TRIGGER_NONE,
				M4U_DEVNAME, (void *)piommu_info)) {
			pr_err("request IRQ %d line failed\n", irq);
			goto clk_err;
		}

		pr_info("m4u hw init OK: %d\n", i);
	}
	return 0;

clk_err:
	if (i == 1)
		free_irq(piommu_info->m4u[0].irq, piommu_info);
	clk_disable_unprepare(piommu_info->m4u_infra_clk);
	clk_disable_unprepare(piommu_info->smi_infra_clk);
	return -ENODEV;
}

static int mtk_iommu_domain_init(struct iommu_domain *domain)
{
	struct mtk_iommu_domain *priv;

	if (unlikely(!piommu_info))
		return -EFAULT;

	priv = kzalloc(sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;
	priv->pgtableva =
	    dma_alloc_coherent(NULL, piommu_info->pgt_size,
			       (dma_addr_t *)&priv->pgtablepa, GFP_KERNEL);
	if (!priv->pgtableva) {
		pr_err("dma_alloc_coherent error!dma memory not available\n");
		goto err_pgtable;
	}

	pr_debug("mtk_iommu_domain_init pgt va 0x%p pa0x%p\n",
		 priv->pgtableva, priv->pgtablepa);
	if (((unsigned int)priv->pgtablepa & (piommu_info->pgt_size-1)) != 0) {
		pr_err("pgt not align 0x%p-0x%p align 0x%x\n", priv->pgtablepa,
		       priv->pgtableva, piommu_info->pgt_size);
		goto err_pgtable;
	}

	piommu_info->protect_va = kmalloc(
			MTK_PROTECT_PA_ALIGN*2, GFP_KERNEL);
	if (!piommu_info->protect_va)
		goto err_pgtable;

	memset(piommu_info->protect_va, 0x55, MTK_PROTECT_PA_ALIGN*2);

	memset(priv->pgtableva, 0, piommu_info->pgt_size);
	spin_lock_init(&priv->pgtlock);
	spin_lock_init(&priv->portlock);

	writel((unsigned int)priv->pgtablepa,
	       piommu_info->m4u_g_base + REG_MMUG_PT_BASE);

	domain->priv = priv;
	piommu_info->pgt_baseva = priv->pgtableva;

	priv->piommuinfo = piommu_info;

	/*confirm pgt base*/
	if ((unsigned int)priv->pgtablepa
	    != readl(piommu_info->m4u_g_base + REG_MMUG_PT_BASE))
		pr_err("mtk iommu pgt base err 0x%x !=reg:0x%x\n",
		       (unsigned int)priv->pgtablepa,
		       readl(piommu_info->m4u_g_base + REG_MMUG_PT_BASE));

	return 0;

err_pgtable:
	if (priv->pgtableva)
		dma_free_coherent(NULL, piommu_info->pgt_size, priv->pgtableva,
				  (dma_addr_t) priv->pgtablepa);
	kfree(priv);
	return -ENOMEM;
}

static void mtk_iommu_domain_destroy(struct iommu_domain *domain)
{
	struct mtk_iommu_domain *priv = domain->priv;

	if (unlikely(!piommu_info)) {
		pr_err("iommu domain destory null err\n");
		return;
	}

	dma_free_coherent(NULL, piommu_info->pgt_size, priv->pgtableva,
			  (dma_addr_t) priv->pgtablepa);
	kfree(domain->priv);
	domain->priv = NULL;
}

static int mtk_iommu_attach_device(struct iommu_domain *domain,
				   struct device *dev)
{
	unsigned long flags;
	struct mtkmmu_drvdata *data =
				(struct mtkmmu_drvdata *)dev->archdata.iommu;
	struct mtk_iommu_domain *priv = domain->priv;

	if (unlikely(!priv)) {
		pr_err("cfg port domain null err\n");
		return -EINVAL;
	}

	spin_lock_irqsave(&priv->pgtlock, flags);
	while (data && data->portid < MTK_IOMMU_PORT_MAX_NR) {
		m4u_config_port(priv, data);
		data = data->pnext;
	}
	spin_lock_irqsave(&priv->pgtlock, flags);

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
	unsigned int i;
	unsigned int page_num = DIV_ROUND_UP(size, PAGE_SIZE);
	unsigned int *pgt_base_iova;
	unsigned int pabase = (unsigned int)paddr;
	unsigned int iovabase = (unsigned int)iova;
	unsigned long flags;

	BUG_ON(priv->pgtableva == NULL);

	if (unlikely(paddr & (PAGE_SIZE-1))) {
		pr_err("mtk map pa not align 0x%x\n", paddr);
		return -EINVAL;
	}

	pr_debug("iommu map iova 0x%x pa 0x%x offset %d, sz 0x%x, pgnr:%d\n",
		 iovabase, pabase, iovabase >> PAGE_SHIFT, size, page_num);

	spin_lock_irqsave(&priv->pgtlock, flags);
	pgt_base_iova = priv->pgtableva + (iovabase >> PAGE_SHIFT);
	for (i = 0; i < page_num; i++) {
		pgt_base_iova[i] =
		    pabase | F_DESC_VALID | F_DESC_NONSEC(1) | F_DESC_SHARE(0);
		pabase += PAGE_SIZE;
	}
	m4u_invalid_tlb(M4U_ID_ALL, 0,
			iovabase, iovabase + size - 1);
	spin_unlock_irqrestore(&priv->pgtlock, flags);
	return 0;
}

static size_t mtk_iommu_unmap(struct iommu_domain *domain,
			      unsigned long iova, size_t size)
{
	struct mtk_iommu_domain *priv = domain->priv;
	unsigned int page_num = DIV_ROUND_UP(size, PAGE_SIZE);
	unsigned int *pgt_base_iova;
	unsigned int iovabase = (unsigned int)iova;
	unsigned long flags;

	BUG_ON(priv->pgtableva == NULL);

	spin_lock_irqsave(&priv->pgtlock, flags);
	pgt_base_iova = priv->pgtableva + (iovabase >> PAGE_SHIFT);
	memset(pgt_base_iova, 0x0, page_num*sizeof(int));

	m4u_invalid_tlb(M4U_ID_ALL, 0,
			iovabase, iovabase + size - 1);
	spin_unlock_irqrestore(&priv->pgtlock, flags);
	return 0;
}

static phys_addr_t mtk_iommu_iova_to_phys(struct iommu_domain *domain,
					  dma_addr_t iova)
{
	struct mtk_iommu_domain *priv = domain->priv;
	unsigned long flags;
	unsigned int phys;
	unsigned int iovabase = (unsigned int)iova;

	spin_lock_irqsave(&priv->pgtlock, flags);
	phys = *(priv->pgtableva + (iovabase >> PAGE_SHIFT));
	spin_unlock_irqrestore(&priv->pgtlock, flags);
	return (phys_addr_t) phys;
}

static struct iommu_ops mtk_iommu_ops = {
	.domain_init = &mtk_iommu_domain_init,
	.domain_destroy = &mtk_iommu_domain_destroy,
	.attach_dev = &mtk_iommu_attach_device,
	.detach_dev = &mtk_iommu_detach_device,
	.map = &mtk_iommu_map,
	.unmap = &mtk_iommu_unmap,
	.iova_to_phys = &mtk_iommu_iova_to_phys,
	.pgsize_bitmap = PAGE_SIZE,
};

static struct dma_iommu_mapping *mtk_mapping;

struct dma_iommu_mapping *mtk_iommu_mapping(void)
{
	return mtk_mapping;
}

static int mtk_iommu_parse_dt(struct device *piommudev,
			      struct mtk_iommu_info *piommu_info)
{
	int ret;
	struct device_node *ofnode;

	if (!piommudev || !piommu_info) {
		pr_err("mtk iommu parst dt null\n");
		return -EINVAL;
	}
	ofnode = piommudev->of_node;
	if (!ofnode) {
		pr_err("mtk iommu parst dt node null\n");
		return -EINVAL;
	}

	piommu_info->m4u_g_base = of_iomap(ofnode, 0);
	piommu_info->smi_common_ao_base = of_iomap(ofnode, 1);
	piommu_info->smi_common_ext_base = of_iomap(ofnode, 2);

	if (IS_ERR(piommu_info->m4u_g_base) ||
	    IS_ERR(piommu_info->smi_common_ao_base) ||
	    IS_ERR(piommu_info->smi_common_ext_base)) {
		pr_err("m4u base err g0x%p AO 0x%p ext 0x%p\n",
		       piommu_info->m4u_g_base,
		       piommu_info->smi_common_ao_base,
		       piommu_info->smi_common_ext_base);
		goto iommu_dts_err;
	}

	piommu_info->m4u_L2_base = piommu_info->m4u_g_base
		+ piommu_info->iommu_cfg->L2_offset;
	piommu_info->m4u[0].m4u_base = piommu_info->m4u_g_base
		+ piommu_info->iommu_cfg->m4u0_offset;
	piommu_info->m4u[1].m4u_base = piommu_info->m4u_g_base
		+ piommu_info->iommu_cfg->m4u1_offset;

	piommu_info->m4u[0].irq = irq_of_parse_and_map(ofnode, 0);
	piommu_info->m4u[1].irq = irq_of_parse_and_map(ofnode, 1);
	if (piommu_info->m4u[0].irq < 0) {/*m4u 1 irq err is allowed*/
		pr_err("mtk irq err %d %d\n",
		       piommu_info->m4u[0].irq, piommu_info->m4u[1].irq);
		goto iommu_dts_err;
	}

	piommu_info->m4u_infra_clk = devm_clk_get(piommudev, "m4uclk");
	piommu_info->smi_infra_clk = devm_clk_get(piommudev, "smiclk");
	if (IS_ERR(piommu_info->m4u_infra_clk) ||
	    IS_ERR(piommu_info->smi_infra_clk)) {
		pr_err("m4u clk err 0x%p, smi 0x%p",
		       piommu_info->m4u_infra_clk, piommu_info->smi_infra_clk);
		goto iommu_dts_err;
	}

	ret = of_property_read_u32_array(ofnode, "larb-port-mapping",
					 piommu_info->larb_port_mapping,
					 piommu_info->iommu_cfg->larb_nr);
	if (ret < 0) {
		pr_err("larb_port_mapping err 0x%x\n", ret);
		goto iommu_dts_err;
	}

	ret = of_property_read_u32(ofnode, "iova-size",
				   &piommu_info->iova_size);
	if (ret < 0) {
		pr_info("iova sz dts err,set default 1G\n");
		piommu_info->iova_size = 0x40000000;
	}
	piommu_info->iova_base = 0;

	piommu_info->pgt_size = piommu_info->iova_size/1024;
	if (piommu_info->pgt_size & MTK_PT_ALIGN_BASE ||
	    piommu_info->iova_size & MTK_PT_ALIGN_BASE) {
		pr_err("iova sz 0x%x pgtsz 0x%x err,align 256M at least\n",
		       piommu_info->pgt_size, piommu_info->iova_size);
		goto iommu_dts_err;
	}

	pr_info("iommu dt parse ok\n");
	return 0;
iommu_dts_err:
	/*unremap will do in probe*/
	return -EINVAL;
}

static int m4u_probe(struct platform_device *pdev)
{
	int ret = 0;

	if (piommu_info) {
		pr_err("m4u init already ok,err\n");
		return -EFAULT;
	}

	piommu_info = devm_kzalloc(&pdev->dev, sizeof(struct mtk_iommu_info),
				   GFP_KERNEL);
	if (!piommu_info)
		return -ENOMEM;

	piommu_info->iommu_cfg = mtk_iommu_getcfg();

	ret = mtk_iommu_parse_dt(&pdev->dev, piommu_info);
	if (ret) {
		pr_err("iommu dt parse fail\n");
		goto dt_err;
	}

	/*init dma mapping*/
	mtk_mapping = arm_iommu_create_mapping(&platform_bus_type,
					       piommu_info->iova_base,
					       piommu_info->iova_size);
	if (IS_ERR(mtk_mapping))
		goto dt_err;

	pr_info("mtk iommu mapping 0x%p,iovaSz 0x%x\n",
		mtk_mapping, piommu_info->iova_size);

	ret = m4u_hw_init(piommu_info);
	if (ret < 0)
		goto dt_err;

	return 0;

dt_err:
	pr_err("iommu probe err\n");
	if (piommu_info->m4u_g_base)
		iounmap(piommu_info->m4u_g_base);
	if (piommu_info->smi_common_ao_base)
		iounmap(piommu_info->smi_common_ao_base);
	if (piommu_info->smi_common_ext_base)
		iounmap(piommu_info->smi_common_ext_base);

	kfree(piommu_info);
	piommu_info = NULL;

	return 0;
}

static int m4u_remove(struct platform_device *pdev)
{
	pr_info("iommu_remove()\n");

	if (unlikely(!piommu_info)) {
		pr_err("iommu remove null err\n");
		return -EFAULT;
	}

	if (piommu_info->m4u_g_base)
		iounmap(piommu_info->m4u_g_base);
	if (piommu_info->smi_common_ao_base)
		iounmap(piommu_info->smi_common_ao_base);
	if (piommu_info->smi_common_ext_base)
		iounmap(piommu_info->smi_common_ext_base);

	free_irq(piommu_info->m4u[0].irq, piommu_info);

	if (piommu_info->iommu_cfg->m4u_nr > 1)
		free_irq(piommu_info->m4u[1].irq, piommu_info);

	return 0;
}

#if defined(CONFIG_MTK_IOMMU_MT8135)
static const struct of_device_id iommu_of_ids[] = {
	{ .compatible = "mediatek,mt8135-iommu", },
	{}
};
#elif defined(CONFIG_MTK_IOMMU_MT8127)
static const struct of_device_id iommu_of_ids[] = {
	{ .compatible = "mediatek,mt8127-iommu", },
	{}
};
#endif
static struct platform_driver mtk_iommu_driver = {
	.probe	= m4u_probe,
	.remove	= m4u_remove,
	.driver	= {
		.name = M4U_DEVNAME,
		.owner = THIS_MODULE,
		.of_match_table = iommu_of_ids,
	}
};

static int __init mtk_iommu_init(void)
{
	int ret = 0;

	ret = bus_set_iommu(&platform_bus_type, &mtk_iommu_ops);
	if (ret) {
		pr_err("reg mtk-iommu fail\n");
		return -ENODEV;
	}

	ret = platform_driver_register(&mtk_iommu_driver);
	if (ret) {
		pr_err("mtkiommu dev error\n");
		bus_set_iommu(&platform_bus_type, NULL);
		return -ENODEV;
	}

	return ret;
}

subsys_initcall(mtk_iommu_init);

