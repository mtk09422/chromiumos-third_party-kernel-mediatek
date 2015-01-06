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
#ifndef MTK_IOMMU_PLATFORM_H
#define MTK_IOMMU_PLATFORM_H

#include <linux/io.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/mm.h>

#include "mtk_iommu.h"

enum {
	M4U_ID_0 = 0,
	M4U_ID_1,
	M4U_ID_ALL
};

#define MTK_IOMMU_PGT_SZ  SZ_1M /*pagetable size*/

#define MTK_PROTECT_PA_ALIGN  128

#define MTK_IOMMU_LARB_MAX_NR 8
#define MTK_IOMMU_PORT_MAX_NR 100
struct mtk_iommu_port {
	const char *port_name;
	unsigned int m4u_id:2;
	unsigned int m4u_slave:2;/*main tlb index in mm iommu*/
	unsigned int larb_id:4;
	unsigned int port_id:8;/*port id in larb*/
	unsigned int tf_id:16;/*translation fault id*/
};

struct mtk_iommu {
	void __iomem *m4u_base;
	unsigned int  irq;
};

struct mtk_iommu_info {
	void __iomem *m4u_g_base;
	void __iomem *m4u_L2_base;
	void __iomem *smi_common_ao_base;
	void __iomem *larb_base[MTK_IOMMU_LARB_MAX_NR];
	struct mtk_iommu m4u[M4U_ID_ALL];
	struct clk *m4u_infra_clk;
	struct clk *smi_infra_clk;
	/*struct clk *smi_clk[SMI_CLK_LARB_MAX];*/
	unsigned int iova_base; /*iova start addr*/
	unsigned int iova_size;
	dma_addr_t   pgt_basepa;
	void __iomem *protect_va; /*dirty data*/
	struct device *dev;
	struct iommu_domain *domain;
	const struct mtk_iommu_cfg *imucfg;
	struct mtkmmu_drvdata portcfg[MTK_IOMMU_PORT_MAX_NR];
};

struct mtk_iommu_domain {
	unsigned int *pgtableva; /*pagetable base addr*/
	dma_addr_t pgtablepa;
	spinlock_t pgtlock;	/* lock for modifying page table*/
	spinlock_t portlock;    /* lock for config port*/
	struct mtk_iommu_info *piommuinfo;
};

/*hw config*/
struct mtk_iommu_cfg {
	unsigned int m4u_nr;
	unsigned int m4u0_offset;
	unsigned int m4u1_offset;
	unsigned int L2_offset;
	unsigned int larb_nr;
	unsigned int larb_nr_real;
	unsigned int m4u_port_in_larbx[MTK_IOMMU_LARB_MAX_NR];
	unsigned int m4u_gpu_port;
	unsigned int m4u_port_nr;
	const struct mtk_iommu_port *pport;

	/*hw function*/
	int (*dt_parse)(struct platform_device *pdev,
			struct mtk_iommu_info *piommu);
	int (*hw_init)(const struct mtk_iommu_info *piommu);
	int (*map)(struct mtk_iommu_domain *mtkdomain, unsigned int iova,
		   phys_addr_t paddr, size_t size);
	size_t (*unmap)(struct mtk_iommu_domain *mtkdomain, unsigned int iova,
			size_t size);
	phys_addr_t (*iova_to_phys)(struct mtk_iommu_domain *mtkdomain,
				    unsigned int iova);
	int (*config_port)(struct mtk_iommu_info *piommu, int portid);
	int (*faulthandler)(struct iommu_domain *imudomain,
			    struct device *dev, unsigned long iova,
			    int m4u_index, void *pimu);
};

extern const struct mtk_iommu_cfg mtk_iommu_mt8135_cfg;
extern const struct mtk_iommu_cfg mtk_iommu_mt8173_cfg;

const char *mtk_iommu_get_port_name(const struct mtk_iommu_info *piommu,
				    unsigned int portid);

#endif
