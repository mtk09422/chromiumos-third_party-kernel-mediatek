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
#ifndef MTK_IOMMU_REG_H
#define MTK_IOMMU_REG_H

#define F_DESC_VALID                0x2
#define F_DESC_SHARE(en)            (en << 2)
#define F_DESC_NONSEC(non_sec)      (non_sec << 3)

/*M4U_BASEg global base*/
#define REG_MMUG_CTRL			(0x0)
#define F_MMUG_CTRL_INV_EN0		(1 << 0)
#define F_MMUG_CTRL_INV_EN1		(1 << 1)
#define F_MMUG_CTRL_INV_EN2		(1 << 2)
#define F_MMUG_CTRL_PRE_EN		(1 << 4)

#define REG_MMUG_INVLD			0x4
#define F_MMUG_INV_ALL			0x2
#define F_MMUG_INV_RANGE		0x1

#define REG_MMUG_INVLD_SA		0x8
#define REG_MMUG_INVLD_EA		0xC
#define REG_MMUG_PT_BASE		0x10

#define REG_MMUG_L2_SEL				0x18
#define F_MMUG_L2_SEL_FLUSH_EN                  (1 << 3)
#define F_MMUG_L2_SEL_L2_ULTRA                  (1 << 2)
#define F_MMUG_L2_SEL_L2_BUS_SEL_EMI            (1 << 0)

#define REG_MMUG_DCM                    (0x1C)
#define F_MMUG_DCM_ON                   (1 << 0)

/*M4U_L2_BASE*/
#define REG_L2_GDC_STATE                (0x0)
#define F_L2_GDC_ST_LOCK_ALERT_BIT      (1 << 15)
#define F_L2_GDC_ST_EVENT_MSK           0xC0

#define REG_L2_GDC_OP                   (0x4)
#define F_L2_GDC_LOCK_TH(th)            ((th & 0x3) << 2)

#define REG_L2_GPE_STATUS               (0x18)
#define F_L2_GPE_ST_RANGE_INV_DONE       2
#define F_L2_GPE_ST_PREFETCH_DONE        1

/*m4u (0/1)*/
#define F_MAIN_TLB_LOCK_BIT              (1 << 11)
#define F_MAIN_TLB_VALID_BIT             (1 << 10)
#define F_MAIN_TLB_SQ_EN_BIT             (1 << 9)
#define F_MAIN_TLB_SQ_INDEX_MSK          (0xf << 5)
#define F_MAIN_TLB_INV_DES_BIT           (1 << 4)

#define REG_MMU_CTRL_REG                 0x210
#define F_MMU_CTRL_TF_PROT_VAL(prot)     ((prot & 0x3) << 5)
#define F_MMU_CTRL_COHERE_EN             (1 << 8)
#define REG_MMU_IVRP_PADDR               0x214
#define REG_MMU_INT_CONTROL              0x220
#define F_INT_CLR_BIT                    (1 << 12)
#define REG_MMU_FAULT_ST                 0x224
#define F_INT_TRANSLATION_FAULT                 (1 << 0)
#define F_INT_TLB_MULTI_HIT_FAULT               (1 << 1)
#define F_INT_INVALID_PHYSICAL_ADDRESS_FAULT    (1 << 2)
#define F_INT_ENTRY_REPLACEMENT_FAULT           (1 << 3)
#define F_INT_TABLE_WALK_FAULT                  (1 << 4)
#define F_INT_TLB_MISS_FAULT                    (1 << 5)
#define F_INT_PFH_DMA_FIFO_OVERFLOW             (1 << 6)
#define F_INT_MISS_DMA_FIFO_OVERFLOW            (1 << 7)
#define F_INT_PFH_FIFO_ERR                      (1 << 8)
#define F_INT_MISS_FIFO_ERR                     (1 << 9)
#define REG_MMU_FAULT_VA                0x228
#define REG_MMU_INVLD_PA                0x22C
#define REG_MMU_INT_ID                  0x388
#define F_INT_ID_TF_PORT_ID(regval)     ((regval >> 8) & 0xf)
#define F_INT_ID_TF_LARB_ID(regval)     ((regval >> 12) & 0x7)
#define F_INT_ID_MH_PORT_ID(regval)     ((regval >> 24) & 0xf)
#define F_INT_ID_MH_LARB_ID(regval)     ((regval >> 28) & 0x7)

/*SMI_COMMON_AO_BASE*/
#define REG_SMI_SECUR_CON_G3D			(0x05E8)
#define REG_SMI_SECUR_CON(x)			(0x05C0 + ((x) << 2))
#define REG_SMI_SECUR_CON_OF_PORT(port)  REG_SMI_SECUR_CON((port >> 3))
#define F_SMI_SECUR_CON_SECURE(port)	(1 << ((port & 0x7) << 2))
#define F_SMI_SECUR_CON_DOMAIN(port, val) (((val) & 0x3) \
	<< (((port & 0x7) << 2) + 1))
#define F_SMI_SECUR_CON_VIRTUAL(port)	(1 << (((port & 0x7) << 2) + 3))
#define F_SMI_SECUR_CON_MASK(port)	(0xf << (((port & 0x7) << 2)))

#endif

