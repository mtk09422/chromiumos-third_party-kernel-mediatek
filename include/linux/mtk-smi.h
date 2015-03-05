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
#ifndef MTK_IOMMU_SMI_H
#define MTK_IOMMU_SMI_H
#include <linux/platform_device.h>

/*
 * Enable iommu for each port, it is only for iommu.
 *
 * Returns 0 if successfully, others if failed.
 */
int mtk_smi_config_port(struct platform_device *pdev,
			unsigned int larbportid);

/*
 * The multimedia module should call the two function below
 * which help open/close the clock of the larb.
 * so the client dtsi should add the larb like "larb = <&larb0>"
 * to get platform_device.
 *
 * mtk_smi_larb_get should be called before the multimedia h/w work.
 * mtk_smi_larb_put should be called after h/w done.
 *
 * Returns 0 if successfully, others if failed.
 */
int mtk_smi_larb_get(struct platform_device *plarbdev);
void mtk_smi_larb_put(struct platform_device *plarbdev);

#endif
