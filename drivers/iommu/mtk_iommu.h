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
#ifndef MTK_IOMMU_H
#define MTK_IOMMU_H

#include <asm/dma-iommu.h>

typedef int (*iommu_fault_callback_t) (unsigned int port,
					unsigned int iova, void *data);
struct mtkmmu_drvdata {
	unsigned int portid;
	unsigned int virtuality;
	iommu_fault_callback_t fault_handler;
	void *faultdata;  /*fault handle: data*/
	struct mtkmmu_drvdata *pnext; /*many port can be configed at one time*/
};

struct dma_iommu_mapping *mtk_iommu_mapping(void);

#endif
