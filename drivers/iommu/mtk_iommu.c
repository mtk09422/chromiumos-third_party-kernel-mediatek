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
#include <asm/cacheflush.h>
#include <linux/dma-mapping.h>
#include <asm/dma-iommu.h>

#include <linux/io.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>

#include "mtk_iommu_platform.h"
#include "mtk_iommu_reg_mt8173.h"
#include "mtk_iommu.h"

const char *mtk_iommu_get_port_name(const struct mtk_iommu_info *piommu,
				    unsigned int portid)
{
	const struct mtk_iommu_port *pcurport = NULL;

	pcurport = piommu->imucfg->pport + portid;

	if (portid < piommu->imucfg->m4u_port_nr && pcurport)
		return pcurport->port_name;
	else
		return "UNKNOWN_PORT";
}

static irqreturn_t mtk_iommu_isr(int irq, void *dev_id)
{
	unsigned int m4u_index;
	struct mtk_iommu_info *piommu = (struct mtk_iommu_info *)dev_id;

	if (irq == piommu->m4u[0].irq) {
		m4u_index = 0;
	} else if (irq == piommu->m4u[1].irq) {
		m4u_index = 1;
	} else {
		dev_err(piommu->dev, "irq number:%d\n", irq);
		goto isr_err;
	}
	report_iommu_fault(piommu->domain, piommu->dev, 0, m4u_index);

isr_err:
	return IRQ_HANDLED;
}

static struct device *pimudev;

static int mtk_iommu_domain_init(struct iommu_domain *domain)
{
	struct mtk_iommu_domain *priv;

	priv = kzalloc(sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	priv->pgtableva = dma_alloc_coherent(pimudev, MTK_IOMMU_PGT_SZ,
					     &priv->pgtablepa, GFP_KERNEL);
	if (!priv->pgtableva) {
		pr_err("dma_alloc_coherent pagetable fail\n");
		goto err_pgtable;
	}

	if (!IS_ALIGNED(priv->pgtablepa, MTK_IOMMU_PGT_SZ)) {
		/*align pagetable size here*/
		pr_err("pagetable not aligned pa 0x%pad-0x%p align 0x%x\n",
		       &priv->pgtablepa, priv->pgtableva, MTK_IOMMU_PGT_SZ);
		goto err_pgtable;
	}

	memset(priv->pgtableva, 0, MTK_IOMMU_PGT_SZ);

	spin_lock_init(&priv->pgtlock);
	spin_lock_init(&priv->portlock);
	domain->priv = priv;

	return 0;

err_pgtable:
	if (priv->pgtableva)
		dma_free_coherent(pimudev, MTK_IOMMU_PGT_SZ, priv->pgtableva,
				  priv->pgtablepa);

	kfree(priv);
	return -ENOMEM;
}

static void mtk_iommu_domain_destroy(struct iommu_domain *domain)
{
	struct mtk_iommu_domain *priv = domain->priv;

	dma_free_coherent(priv->piommuinfo->dev, MTK_IOMMU_PGT_SZ,
			  priv->pgtableva, priv->pgtablepa);
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
	struct mtk_iommu_info *piommu = priv->piommuinfo;

	spin_lock_irqsave(&priv->portlock, flags);
	while (data && data->portid < piommu->imucfg->m4u_port_nr) {
		piommu->portcfg[data->portid].portid =
			data->portid;
		piommu->portcfg[data->portid].fault_handler =
			data->fault_handler;
		piommu->portcfg[data->portid].faultdata =
			data->faultdata;
		piommu->imucfg->config_port(piommu, data->portid);
		data = data->pnext;
	}
	spin_unlock_irqrestore(&priv->portlock, flags);

	return 0;
}

static void mtk_iommu_detach_device(struct iommu_domain *domain,
				    struct device *dev)
{
}

static int mtk_iommu_add_device(struct device *dev)
{
	/*pr_info("add_device %s\n", dev->init_name);*/
	return 0;
}

static int mtk_iommu_map(struct iommu_domain *domain, unsigned long iova,
			 phys_addr_t paddr, size_t size, int prot)
{
	struct mtk_iommu_domain *priv = domain->priv;
	unsigned long flags;

	if (!PAGE_ALIGNED(paddr)) {
		dev_err(priv->piommuinfo->dev,
			"iommu map pa not aligned 0x%pa\n", &paddr);
		return -EINVAL;
	}

	dev_dbg(priv->piommuinfo->dev,
		"iommu map iova 0x%lx pa 0x%pa sz 0x%zx\n",
		iova, &paddr, size);

	spin_lock_irqsave(&priv->pgtlock, flags);
	priv->piommuinfo->imucfg->map(priv, (unsigned int)iova, paddr, size);
	spin_unlock_irqrestore(&priv->pgtlock, flags);

	return 0;
}

static size_t mtk_iommu_unmap(struct iommu_domain *domain,
			      unsigned long iova, size_t size)
{
	struct mtk_iommu_domain *priv = domain->priv;
	unsigned long flags;

	spin_lock_irqsave(&priv->pgtlock, flags);
	priv->piommuinfo->imucfg->unmap(priv, (unsigned int)iova, size);
	spin_unlock_irqrestore(&priv->pgtlock, flags);

	return 0;
}

static phys_addr_t mtk_iommu_iova_to_phys(struct iommu_domain *domain,
					  dma_addr_t iova)
{
	struct mtk_iommu_domain *priv = domain->priv;
	unsigned long flags;
	phys_addr_t phys;

	spin_lock_irqsave(&priv->pgtlock, flags);
	phys = priv->piommuinfo->imucfg->iova_to_phys(priv,
					       (unsigned int)iova);
	spin_unlock_irqrestore(&priv->pgtlock, flags);
	return phys;
}

static struct iommu_ops mtk_iommu_ops = {
	.domain_init = mtk_iommu_domain_init,
	.domain_destroy = mtk_iommu_domain_destroy,
	.attach_dev = mtk_iommu_attach_device,
	.detach_dev = mtk_iommu_detach_device,
	.add_device = mtk_iommu_add_device,
	.map = mtk_iommu_map,
	.unmap = mtk_iommu_unmap,
	.iova_to_phys = mtk_iommu_iova_to_phys,
	.pgsize_bitmap = PAGE_SIZE,
};

/* mtkiommu mapping table */
static struct dma_iommu_mapping *mtk_mapping;

struct dma_iommu_mapping *mtk_iommu_mapping(void)
{
	return mtk_mapping;
}
EXPORT_SYMBOL(mtk_iommu_mapping);

static const struct of_device_id mtk_iommu_of_ids[] = {
	{ .compatible = "mediatek,mt8135-iommu",
	  .data = &mtk_iommu_mt8135_cfg,
	},
	{ .compatible = "mediatek,mt8173-iommu",
	  .data = &mtk_iommu_mt8173_cfg,
	},
	{}
};

static int mtk_iommu_probe(struct platform_device *pdev)
{
	int ret, i;
	struct mtk_iommu_info *piommu;
	struct iommu_domain *domain;
	struct mtk_iommu_domain *mtk_domain;
	const struct of_device_id *of_id;

	pimudev = &pdev->dev;

	piommu = devm_kzalloc(&pdev->dev, sizeof(struct mtk_iommu_info),
			      GFP_KERNEL);
	if (!piommu)
		return -ENOMEM;

	of_id = of_match_node(mtk_iommu_of_ids, pdev->dev.of_node);
	if (!of_id)
		return -ENODEV;

	piommu->protect_va = devm_kmalloc(&pdev->dev, MTK_PROTECT_PA_ALIGN*2,
					  GFP_KERNEL);
	if (!piommu->protect_va)
		goto protect_err;
	memset(piommu->protect_va, 0x55, MTK_PROTECT_PA_ALIGN*2);

	piommu->imucfg = (const struct mtk_iommu_cfg *)of_id->data;
	piommu->dev = &pdev->dev;

	ret = piommu->imucfg->dt_parse(pdev, piommu);
	if (ret) {
		dev_err(piommu->dev, "iommu dt parse fail\n");
		goto dt_err;
	}

	piommu->iova_base = 0;
	piommu->iova_size = MTK_IOMMU_PGT_SZ * SZ_1K;

	/*init dma mapping*/
	mtk_mapping = arm_iommu_create_mapping(&platform_bus_type,
					       piommu->iova_base,
					       piommu->iova_size);
	if (IS_ERR(mtk_mapping))
		goto dt_err;
	domain = mtk_mapping->domain;
	mtk_domain = domain->priv;
	mtk_domain->piommuinfo = piommu;

	piommu->pgt_basepa = mtk_domain->pgtablepa;
	piommu->domain = domain;

	ret = piommu->imucfg->hw_init(piommu);
	if (ret < 0)
		goto dt_err;

	for (i = 0; i < piommu->imucfg->m4u_nr; i++) {
		if (devm_request_irq(piommu->dev, piommu->m4u[i].irq,
				     mtk_iommu_isr, IRQF_TRIGGER_NONE,
				     "mtkiommu", (void *)piommu)) {
			dev_err(piommu->dev, "IRQ request %d failed\n",
				piommu->m4u[i].irq);
			goto dt_err;
		}
	}

	iommu_set_fault_handler(domain, piommu->imucfg->faulthandler, piommu);

	dev_set_drvdata(piommu->dev, piommu);
	dev_info(piommu->dev, "probe suc\n");

	return 0;

dt_err:
	kfree(piommu->protect_va);
protect_err:
	kfree(piommu);
	dev_err(piommu->dev, "iommu probe err\n");
	piommu = NULL;
	return 0;
}

static int mtk_iommu_remove(struct platform_device *pdev)
{
	struct mtk_iommu_info *piommu =	dev_get_drvdata(&pdev->dev);

	dev_info(piommu->dev, "iommu_remove\n");

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
	int ret = 0;

	ret = bus_set_iommu(&platform_bus_type, &mtk_iommu_ops);
	if (ret) {
		pr_err("iommu bus setting fail 0x%x-0x%x\n", ret, -ENODEV);
		return -ENODEV;
	}

	ret = platform_driver_register(&mtk_iommu_driver);
	if (ret) {
		pr_err("iommu drv reg fail\n");
		bus_set_iommu(&platform_bus_type, NULL);
		return -ENODEV;
	}

	return ret;
}

subsys_initcall(mtk_iommu_init);

