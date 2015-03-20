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
#define pr_fmt(fmt) "imudbg:"fmt

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
#include <linux/io.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>

struct miscdevice mtkiommutestdev;

struct device mtktest = {
	.init_name = "mtkiommutest",
	.archdata = {
		.iommu = NULL,
	}
};

struct mtk_iommu_info *ps_imu_tst;
static int m4u_debug_set(void *data, u64 val)
{
	pr_info("m4u_debug_set:val=%llu\n", val);

	switch (val) {
	case 2: {
		int ret = 0;
		void __iomem *va1;
		dma_addr_t dma_addr1;
		struct mtkmmu_drvdata testport;
		DEFINE_DMA_ATTRS(attrs);

		memset(&testport, 0, sizeof(testport));

		testport.portid = 0;
		mtktest.archdata.iommu = &testport;

		pr_info("set drv data ok\n");

		ret = arm_iommu_attach_device(&mtktest,
					      mtk_iommu_mapping());
		pr_info("attach device ret 0x%x\n", ret);

		dma_set_attr(DMA_ATTR_NO_KERNEL_MAPPING, &attrs);

		va1 = dma_alloc_attrs(&mtktest, 50*4096,
				      &dma_addr1, GFP_KERNEL, &attrs);
		pr_info("get dmaaddr1 0x%pad, va =0x%p\n",
			&dma_addr1, va1);

		va1 = dma_alloc_attrs(&mtktest, 100*4096,
				      &dma_addr1, GFP_KERNEL, &attrs);
		pr_info("get dmaaddr2 0x%pad , va2 =0x%p\n",
			&dma_addr1, va1);
		}
		break;
	/*disp test iommu in mt8135*/
	/*
	case 3:	{
		u32 sz = 1280*800*4;
		u32 iovaovl = 0x1000;
		u32 iovaovllayer3 = 0x600000;
		u32 ovlpa, ovlpalayer3 = 0;
		struct mtk_iommu_info *piommu = ps_imu_tst;
		struct mtkmmu_drvdata testport;

		memset(&testport, 0, sizeof(testport));
		testport.portid = 0;
		mtktest.archdata.iommu = &testport;

		pr_info("set drv data ok\n");

		ovlpa = readl(piommu->dispbase + 0xf80);
		ovlpalayer3 = readl(piommu->dispbase + 0xfa0);

		pr_info("ovl-tst addr %p 0x%x\n",
			piommu->dispbase, ovlpa);

		pr_info("iommu test iova 0x%x-0x%x\n", iovaovl, iovaovllayer3);

		iommu_map(piommu->domain, iovaovl, ovlpa, sz, 0);
		iommu_map(piommu->domain, iovaovllayer3, ovlpalayer3, sz, 0);

		pr_info("iommu map done\n");

		arm_iommu_attach_device(&mtktest,
					mtk_iommu_mapping());

		writel(iovaovl, piommu->dispbase + 0xf80);
		writel(iovaovllayer3, piommu->dispbase + 0xfa0);

		pr_info("disp set ok\n");
		}
		break;
	*/
	/*disp test iommu in mt8135*/
	/*case 4:
	{
		u32 sz = 1280*800*2;
		u32 iovaovl = 0x100000;
		u32 cnt = 0;
		u32 ovlpa=0;
		struct mtk_iommu_info *piommu = ps_imu_tst;
		struct mtkmmu_drvdata testport;

		ovlpa = (u32)readl(piommu->dispbase + 0x80);
		pr_info("ovl-tst addr 0x%p+0x40=0x%p 0x%x\n",
			piommu->dispbase,(piommu->dispbase + 0x80), ovlpa);

		pr_info("iommu test iova 0x%x\n", iovaovl);

		iommu_map(piommu->domain, iovaovl, ovlpa, sz, 0);

		memset(&testport, 0, sizeof(testport));
		testport.portid = 19;
		mtktest.archdata.iommu = &testport;

		arm_iommu_attach_device(&mtktest,
					mtk_iommu_mapping());

		writel(1, piommu->ovl_base+0x24);

		while((readl(piommu->ovl_base+0x24) &0x2) != 0x2) {
			cnt++;
			if(cnt>10000) {
				pr_err("disp path get mutex timeout 0x%x\n",
					readl(piommu->ovl_base+0x24));
				break;
			}
		}
		pr_info("iommu get mutex cnt %d\n", cnt);

		pr_info("iommu set iova new 0x%x\n", iovaovl);

		//set new
		writel(iovaovl, piommu->dispbase + 0x80);
		writel(0, piommu->ovl_base+0x24);
		pr_info("iommu test iova done\n");
	}
	break;
	*/

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

/*static int mtk_imudbg_probe(struct platform_device *pdev)
{
	int ret =0;
	struct device_node *np;
	struct platform_device *pdevt;
	struct of_phandle_args out_args={0};
	int i = 0;

	pr_info("mtk_imudbg_probe\n");

	ret = of_parse_phandle_with_args(pdev->dev.of_node,
					 "iommus", "iommu-cells",
					 0, &out_args);

	pr_info("test ret %d, cnt %d\n", ret,
		out_args.args_count);

	for( i = 0; i< out_args.args_count; i++) {
		pr_info("<%d> %d\n", i, out_args.args[i]);
	}

	return 0;
}

static const struct of_device_id mtk_imudbg_of_ids[] = {
	{ .compatible = "mediatek,mt8173-perisys-iommu",
	},
	{}
};

static struct platform_driver mtk_imudbg_drv = {
	.probe	= mtk_imudbg_probe,
	.driver	= {
		.name = "mtimudbg",
		.of_match_table = mtk_imudbg_of_ids,
	}
};
*/
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
	/*
	ret = platform_driver_register(&mtk_imudbg_drv);
	if (ret) {
		pr_err("imudbg drv fail\n");
		return -ENODEV;
	}*/

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
