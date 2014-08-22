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

#include "mtk_iommu.h"

#define M4U_DEVNAME     "mtkiommu"

#define MTK_PT_ALIGN_BASE    ((256*1024)-1)
#define MTK_PROTECT_PA_ALIGN  128

enum {
	M4U_ID_0 = 0,
	M4U_ID_1,
	M4U_ID_ALL
};

#define MTK_IOMMU_LARB_MAX_NR 8
#define MTK_IOMMU_PORT_MAX_NR 60
struct mtk_port {
	const char *port_name;
	unsigned int larbid;
	unsigned int portid;
};

/*hw config*/
struct mtk_iommu_cfg {
	unsigned int m4u_nr;
	unsigned int m4u0_offset;
	unsigned int m4u1_offset;
	unsigned int L2_offset;
	unsigned int larb_nr;
	unsigned int larb_nr_real;
	unsigned int m4u_tf_larbid;
	unsigned int m4u_port_in_larbx[MTK_IOMMU_LARB_MAX_NR];
	unsigned int m4u_gpu_port;
	unsigned int main_tlb_nr;
	unsigned int pfn_tlb_nr;
	unsigned int m4u_seq_nr;
	unsigned int m4u_port_nr;
	const struct mtk_port *pport;
};

struct mtk_iommu {
	void __iomem *m4u_base;
	unsigned int  irq;
};

struct mtk_iommu_info {
	void __iomem *m4u_g_base;
	void __iomem *m4u_L2_base;
	void __iomem *smi_common_ao_base;
	void __iomem *smi_common_ext_base;
	struct mtk_iommu m4u[M4U_ID_ALL];
	struct clk *m4u_infra_clk;
	struct clk *smi_infra_clk;
	unsigned int larb_port_mapping[MTK_IOMMU_LARB_MAX_NR];
	unsigned int iova_base;/*start addr*/
	unsigned int iova_size;
	unsigned int pgt_size;
	unsigned int *pgt_baseva;
	unsigned int *protect_va;
	const struct mtk_iommu_cfg *iommu_cfg;
	struct mtkmmu_drvdata portcfg[MTK_IOMMU_PORT_MAX_NR];
};

struct mtk_iommu_domain {
	unsigned int *pgtableva;
	unsigned int *pgtablepa;
	spinlock_t pgtlock;	/* lock for modifying page table*/
	spinlock_t portlock;
	struct mtk_iommu_info *piommuinfo;
};

const struct mtk_iommu_cfg *mtk_iommu_getcfg(void);

int mtk_iommu_dump_info(void);

#endif

