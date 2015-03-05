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
#ifndef MTK_IOMMU_PLATFORM_H
#define MTK_IOMMU_PLATFORM_H

#include <linux/io.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/mm.h>
#include <linux/platform_device.h>

#include "mtk_iommu_pagetable.h"

#define M4U_PGD_SIZE       SZ_16K   /* pagetable size,mt8173 */

#define MTK_PROTECT_PA_ALIGN  128

#define MTK_IOMMU_LARB_MAX_NR 8
#define MTK_IOMMU_PORT_MAX_NR 100

struct mtk_iommu_port {
	const char *port_name;
	unsigned int m4u_id:2;
	unsigned int m4u_slave:2;/* main tlb index in mm iommu */
	unsigned int larb_id:4;
	unsigned int port_id:8;/* port id in larb */
	unsigned int tf_id:16; /* translation fault id */
};

struct mtk_iommu_cfg {
	unsigned int larb_nr;
	unsigned int m4u_port_nr;
	const struct mtk_iommu_port *pport;
};

struct mtk_iommu_info {
	void __iomem *m4u_base;
	unsigned int  irq;
	struct platform_device *larbpdev[MTK_IOMMU_LARB_MAX_NR];
	struct clk *m4u_infra_clk;
	void __iomem *protect_va;
	struct device *dev;
	struct kmem_cache *m4u_pte_kmem;
	const struct mtk_iommu_cfg *imucfg;
};

struct mtk_iommu_domain {
	struct imu_pgd_t *pgd;
	dma_addr_t pgd_pa;
	spinlock_t pgtlock;	/* lock for modifying page table */
	spinlock_t portlock;    /* lock for config port */
	struct mtk_iommu_info *piommuinfo;
};

int m4u_map(struct mtk_iommu_domain *m4u_domain, unsigned int iova,
	    phys_addr_t paddr, unsigned int size, unsigned int prot);
int m4u_unmap(struct mtk_iommu_domain *domain, unsigned int iova,
	      unsigned int size);
int m4u_get_pte_info(const struct mtk_iommu_domain *domain,
		     unsigned int iova, struct m4u_pte_info_t *pte_info);

#endif
