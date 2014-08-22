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
#include <linux/err.h>
#include <linux/mm.h>
#include <linux/iommu.h>
#include <linux/mman.h>
#include <linux/module.h>
#include <linux/miscdevice.h>
#include <linux/dma-mapping.h>
#include <linux/proc_fs.h>
#include <linux/debugfs.h>
#include <asm/dma-iommu.h>

#include "mtk_iommu.h"
#include "mtk_iommu_platform.h"

struct miscdevice mtkiommutestdev;

struct device mtktest = {
	.init_name = "mtkiommutest",
	.archdata = {
		.iommu = NULL,
	}
};

static int m4u_debug_set(void *data, u64 val)
{
	pr_info("m4u_debug_set:val=%llu\n", val);

	switch (val) {
	case 1:
		mtk_iommu_dump_info();
		break;
	case 2: {  /* alloc dma */
			dma_addr_t dma_addr1, dma_addr2, dma_addr3;
			struct mtkmmu_drvdata testport;
			unsigned int va1, va2, va3;

			DEFINE_DMA_ATTRS(attrs);
			memset(&testport, 0, sizeof(testport));

			testport.portid = 3;
			testport.virtuality = 1;

			mtktest.archdata.iommu = &testport;

			pr_info("set drv data ok\n");

			arm_iommu_attach_device(&mtktest,
						mtk_iommu_mapping());

			dma_set_attr(DMA_ATTR_NO_KERNEL_MAPPING, &attrs);

			va1 = (u32)dma_alloc_attrs(&mtktest, 50*4096,
						   &dma_addr1, GFP_KERNEL,
						   &attrs);
			pr_info("get dmaaddr1 0x%x , va =0x%x\n",
				dma_addr1, va1);

			va3 = (u32)dma_alloc_attrs(&mtktest, 100*4096,
						   &dma_addr3, GFP_KERNEL,
						   &attrs);
			pr_info("get dmaaddr3 0x%x , va =0x%x\n",
				dma_addr3, va3);

			pr_info("free dma2 100page va 0x%x\n", va3);
			dma_free_attrs(&mtktest, 100*4096, (void *)va3,
				       dma_addr2, &attrs);

			va2 = (u32)dma_alloc_attrs(&mtktest, 50*4096,
						   &dma_addr2, GFP_KERNEL,
						   &attrs);
			pr_info("get dma 50 again 0x%x , va =0x%x\n",
				dma_addr2, va2);

			pr_info("free dma1 Fst50page va 0x%x\n", va1);
			dma_free_attrs(&mtktest, 50*4096, (void *)va1,
				       dma_addr1, &attrs);

			va2 = (u32)dma_alloc_attrs(&mtktest, 10*4096,
						   &dma_addr2, GFP_KERNEL,
						   &attrs);
			pr_info("get dma new 10 0x%x , va =0x%x\n",
				dma_addr2, va2);
	    }
	    break;
	case 3: {
			struct mtkmmu_drvdata testport;

			memset(&testport, 0, sizeof(testport));
			testport.portid = 10;
			testport.virtuality = 1;

			mtktest.archdata.iommu = &testport;

			arm_iommu_attach_device(&mtktest, mtk_iommu_mapping());
			arm_iommu_attach_device(&mtktest, mtk_iommu_mapping());

			testport.virtuality = 0;
			arm_iommu_attach_device(&mtktest, mtk_iommu_mapping());
			arm_iommu_attach_device(&mtktest, mtk_iommu_mapping());
		}
		break;

	default:
		pr_err("m4u_debug_set error,val=%llu\n", val);
	}
	return 0;
}

static int m4u_debug_get(void *data, u64 *val)
{
	*val = 0;
	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(m4u_debug_fops, m4u_debug_get,
			m4u_debug_set, "%llu\n");

static int __init mtk_iommu_debug_init(void)
{
	struct dentry *dbg;
	int ret = 0;

	dbg = debugfs_create_file("iommutst", 0644,
				  NULL, NULL, &m4u_debug_fops);
	if (IS_ERR(dbg))
		pr_err("create iommutst debug fs err\n");

	ret = device_register(&mtktest);
	if (ret)
		pr_err("create device error\n");

	pr_debug("mtk_iommu_debug create device 0x%x\n", ret);

	return 0;
}
module_init(mtk_iommu_debug_init)

static void __exit mtk_iommu_debugfs_exit(void)
{
}
module_exit(mtk_iommu_debugfs_exit)

MODULE_DESCRIPTION("mtk iommu: debugfs interface");
MODULE_AUTHOR("YongWu<yong.wu@mediatek.com>");
MODULE_LICENSE("GPL v2");


