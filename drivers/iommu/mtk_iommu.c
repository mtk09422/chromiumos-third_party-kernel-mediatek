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

#include <linux/of_address.h>
#include <linux/of_irq.h>

#include "mtk_iommu_platform.h"
#include "mtk_iommu_reg_mt8173.h"

struct mtk_iommu_user {
	struct list_head	list;
	unsigned int		portid;
};

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
	struct iommu_domain *domain = dev_id;
	struct mtk_iommu_domain *mtkdomain = domain->priv;
	struct mtk_iommu_info *piommu = mtkdomain->piommuinfo;

	if (irq == piommu->m4u[0].irq) {
		m4u_index = 0;
	} else if (irq == piommu->m4u[1].irq) {
		m4u_index = 1;
	} else {
		dev_err(piommu->dev, "irq number:%d\n", irq);
		goto isr_err;
	}
	report_iommu_fault(domain, piommu->dev, 0, m4u_index);

isr_err:
	return IRQ_HANDLED;
}

static struct device *pimudev;

static int mt8173_iommu_domain_init(struct iommu_domain *domain)
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
		/*align pagetable size here*/
		pr_err("pagetable not aligned pa 0x%pad-0x%p align 0x%x\n",
		       &priv->pgd_pa, priv->pgd, M4U_PGD_SIZE);
		goto err_pgtable;
	}

	memset(priv->pgd, 0, M4U_PGD_SIZE);

	spin_lock_init(&priv->pgtlock);
	spin_lock_init(&priv->portlock);
	domain->priv = priv;

	domain->geometry.aperture_start = 0;
	domain->geometry.aperture_end   = ~0UL;
	domain->geometry.force_aperture = true;

	return 0;

err_pgtable:
	if (priv->pgd)
		dma_free_coherent(pimudev, M4U_PGD_SIZE, priv->pgd,
				  priv->pgd_pa);
	kfree(priv);
	return -ENOMEM;
}

static void mt8173_iommu_domain_destroy(struct iommu_domain *domain)
{
	struct mtk_iommu_domain *priv = domain->priv;

	dma_free_coherent(priv->piommuinfo->dev, M4U_PGD_SIZE,
			  priv->pgd, priv->pgd_pa);
	kfree(domain->priv);
	domain->priv = NULL;
}

static int mt8135_iommu_domain_init(struct iommu_domain *domain)
{
	struct mtk_iommu_domain *priv;

	priv = kzalloc(sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	priv->pgd = dma_alloc_coherent(pimudev, MTK_PGT_SIZE, &priv->pgd_pa,
				       GFP_KERNEL);
	if (!priv->pgd) {
		pr_err("dma_alloc_coherent pagetable fail\n");
		goto err_mem;
	}

	if (!IS_ALIGNED(priv->pgd_pa, MTK_PGT_SIZE)) {
		/*align pagetable size here*/
		pr_err("pagetable not aligned pa 0x%pad-0x%p\n",
		       &priv->pgd_pa, priv->pgd);
		goto err_pgtable;
	}

	memset(priv->pgd, 0, MTK_PGT_SIZE);

	spin_lock_init(&priv->pgtlock);
	spin_lock_init(&priv->portlock);
	domain->priv = priv;

	domain->geometry.aperture_start = 0;
	domain->geometry.aperture_end   = ~0UL;
	domain->geometry.force_aperture = true;

	return 0;
err_pgtable:
	dma_free_coherent(pimudev, MTK_PGT_SIZE, priv->pgd,
			  priv->pgd_pa);
err_mem:
	kfree(priv);
	return -ENOMEM;
}

static void mt8135_iommu_domain_destroy(struct iommu_domain *domain)
{
	struct mtk_iommu_domain *priv = domain->priv;

	dma_free_coherent(priv->piommuinfo->dev, MTK_PGT_SIZE,
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
	struct mtk_iommu_user *imu_user = dev->archdata.iommu;
	struct mtk_iommu_user *cur;

	if (!imu_user)
		return 0;

	spin_lock_irqsave(&priv->portlock, flags);
	list_for_each_entry(cur, &imu_user->list, list) {
		if (cur->portid <= piommu->imucfg->m4u_port_nr)
			piommu->imucfg->config_port(piommu, cur->portid);
		else
			dev_err(piommu->dev, "port id %d err\n", cur->portid);
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
	unsigned int i = 0;
	struct platform_device *pdev = to_platform_device(dev);
	struct of_phandle_args out_args = {0};
	struct mtk_iommu_user *imu_owner, *imu_owner_head = NULL, *next;

	while (!of_parse_phandle_with_args(pdev->dev.of_node, "iommus",
					   "iommu-cells", i, &out_args)) {
		if (0 == i) { /*list head*/
			imu_owner_head = kzalloc(sizeof(*imu_owner_head),
						 GFP_KERNEL);
			if (!imu_owner_head)
				return -ENOMEM;

			INIT_LIST_HEAD(&imu_owner_head->list);
			dev->archdata.iommu = imu_owner_head;
		}

		imu_owner = kzalloc(sizeof(*imu_owner), GFP_KERNEL);
		if (!imu_owner)
			goto dev_mem_err;

		if (1 == out_args.args_count) {
			imu_owner->portid = out_args.args[0];
			dev_dbg(dev, "iommu add portid%d", imu_owner->portid);
		} else {
			dev_err(dev, "iommu format error,arg number = %d\n",
				out_args.args_count);
		}

		list_add(&imu_owner->list, &imu_owner_head->list);
		imu_owner_head = imu_owner;
		i++;
	}

	return 0;

dev_mem_err:
	imu_owner_head = dev->archdata.iommu;
	list_for_each_entry_safe(imu_owner, next, &imu_owner_head->list, list)
		kfree(imu_owner);
	kfree(imu_owner_head);

	return -ENOMEM;
}

static void mtk_iommu_remove_device(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct mtk_iommu_user *imu_owner, *next, *imu_owner_head = NULL;

	imu_owner_head = dev->archdata.iommu;
	if (imu_owner_head) {
		dev_dbg(dev, "remove_device %s\n", pdev->name);

		list_for_each_entry_safe(imu_owner, next, &imu_owner_head->list,
					 list)
			kfree(imu_owner);

		kfree(imu_owner_head);
	}
}

static int mtk_iommu_map(struct iommu_domain *domain, unsigned long iova,
			 phys_addr_t paddr, size_t size, int prot)
{
	struct mtk_iommu_domain *priv = domain->priv;
	unsigned long flags;

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
	size_t sz;

	spin_lock_irqsave(&priv->pgtlock, flags);
	sz = priv->piommuinfo->imucfg->unmap(priv, (unsigned int)iova, size);
	spin_unlock_irqrestore(&priv->pgtlock, flags);

	return sz;
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

static struct iommu_ops mt8173_iommu_ops = {
	.domain_init = mt8173_iommu_domain_init,
	.domain_destroy = mt8173_iommu_domain_destroy,
	.attach_dev = mtk_iommu_attach_device,
	.detach_dev = mtk_iommu_detach_device,
	.add_device = mtk_iommu_add_device,
	.remove_device = mtk_iommu_remove_device,
	.map = mtk_iommu_map,
	.unmap = mtk_iommu_unmap,
	.iova_to_phys = mtk_iommu_iova_to_phys,
	.pgsize_bitmap = SZ_4K | SZ_64K | SZ_1M | SZ_16M,
};

static struct iommu_ops mt8135_iommu_ops = {
	.domain_init = mt8135_iommu_domain_init,
	.domain_destroy = mt8135_iommu_domain_destroy,
	.attach_dev = mtk_iommu_attach_device,
	.detach_dev = mtk_iommu_detach_device,
	.add_device = mtk_iommu_add_device,
	.remove_device = mtk_iommu_remove_device,
	.map = mtk_iommu_map,
	.unmap = mtk_iommu_unmap,
	.iova_to_phys = mtk_iommu_iova_to_phys,
	.pgsize_bitmap = SZ_4K,
};

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
	struct dma_iommu_mapping *imu_mapping;
	const struct of_device_id *of_id;

	pimudev = &pdev->dev;

	piommu = devm_kzalloc(&pdev->dev, sizeof(struct mtk_iommu_info),
			      GFP_KERNEL);
	if (!piommu)
		return -ENOMEM;

	piommu->dev = &pdev->dev;

	of_id = of_match_node(mtk_iommu_of_ids, pdev->dev.of_node);
	if (!of_id)
		return -ENODEV;

	piommu->protect_va = devm_kmalloc(&pdev->dev, MTK_PROTECT_PA_ALIGN*2,
					  GFP_KERNEL);
	if (!piommu->protect_va)
		goto protect_err;
	memset(piommu->protect_va, 0x55, MTK_PROTECT_PA_ALIGN*2);

	piommu->imucfg = (const struct mtk_iommu_cfg *)of_id->data;

	ret = piommu->imucfg->dt_parse(pdev, piommu);
	if (ret) {
		dev_err(piommu->dev, "iommu dt parse fail\n");
		goto dt_err;
	}
	ret = piommu->imucfg->pte_init(piommu);
	if (ret) {
		dev_err(piommu->dev, "pte cachemem alloc fail\n");
		goto dt_err;
	}

	/*init dma mapping*/
	imu_mapping = arm_iommu_create_mapping(&platform_bus_type,
					       piommu->imucfg->iova_base,
					       piommu->imucfg->iova_size);
	if (IS_ERR(imu_mapping))
		goto pte_err;
	domain = imu_mapping->domain;
	mtk_domain = domain->priv;
	mtk_domain->piommuinfo = piommu;

	ret = piommu->imucfg->hw_init(mtk_domain);
	if (ret < 0)
		goto hw_err;

	for (i = 0; i < piommu->imucfg->m4u_nr; i++) {
		if (devm_request_irq(piommu->dev, piommu->m4u[i].irq,
				     mtk_iommu_isr, IRQF_TRIGGER_NONE,
				     "mtkiommu", (void *)domain)) {
			dev_err(piommu->dev, "IRQ request %d failed\n",
				piommu->m4u[i].irq);
			goto hw_err;
		}
	}

	arm_iommu_attach_device(piommu->dev, imu_mapping);

	iommu_set_fault_handler(domain, piommu->imucfg->faulthandler, piommu);

	dev_set_drvdata(piommu->dev, piommu);
	dev_info(piommu->dev, "probe suc\n");

	return 0;
hw_err:
	arm_iommu_release_mapping(imu_mapping);
pte_err:
	if (piommu->imucfg->pte_uninit)
		piommu->imucfg->pte_uninit(piommu);
dt_err:
	kfree(piommu->protect_va);
protect_err:
	dev_err(piommu->dev, "probe error\n");
	return 0;
}

static int mtk_iommu_remove(struct platform_device *pdev)
{
	struct mtk_iommu_info *piommu =	dev_get_drvdata(&pdev->dev);

	dev_info(piommu->dev, "iommu_remove");

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

	ret = bus_set_iommu(&platform_bus_type,
			    1 ? &mt8173_iommu_ops : &mt8135_iommu_ops);
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

