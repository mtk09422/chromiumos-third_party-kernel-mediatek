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
#ifndef MTK_IOMMU_REG_MT8173_H
#define MTK_IOMMU_REG_MT8173_H

#define F_DESC_VALID                0x2
#define F_DESC_SHARE(en)            (en << 10)
#define F_DESC_NONSEC(non_sec)      (non_sec << 11)

#define REG_MMUG_PT_BASE	 0x0

#define REG_MMU_INVLD		 0x20
#define F_MMU_INV_ALL		 0x2
#define F_MMU_INV_RANGE		 0x1

#define REG_MMU_INVLD_SA	 0x24
#define REG_MMU_INVLD_EA         0x28

#define REG_MMU_INVLD_SEC         0x2c
#define F_MMU_INV_SEC_ALL          0x2
#define F_MMU_INV_SEC_RANGE        0x1

#define REG_INVLID_SEL	         0x38
#define F_MMU_INV_EN_L1		 (1 << 0)
#define F_MMU_INV_EN_L2		 (1 << 1)

#define REG_MMU_STANDARD_AXI_MODE   0x48
#define REG_MMU_DCM_DIS             0x50
#define REG_MMU_LEGACY_4KB_MODE     0x60

#define REG_MMU_CTRL_REG                 0x110
#define F_MMU_CTRL_REROUTE_PFQ_TO_MQ_EN  (1<<4)
#define F_MMU_CTRL_TF_PROT_VAL(prot)      (((prot) & 0x3)<<5)
#define F_MMU_CTRL_COHERE_EN          (1<<8)

#define REG_MMU_IVRP_PADDR               0x114
#define F_MMU_IVRP_PA_SET(PA)            (PA>>1)
#define F_MMU_IVRP_4G_DRAM_PA_SET(PA)    ((PA>>1)|(1<<31))

#define REG_MMU_INT_L2_CONTROL      0x120
#define F_INT_L2_CLR_BIT            (1<<12)

#define REG_MMU_INT_MAIN_CONTROL    0x124
#define F_INT_TRANSLATION_FAULT(MMU)           (1<<(0+(((MMU)<<1)|((MMU)<<2))))
#define F_INT_MAIN_MULTI_HIT_FAULT(MMU)        (1<<(1+(((MMU)<<1)|((MMU)<<2))))
#define F_INT_INVALID_PA_FAULT(MMU)            (1<<(2+(((MMU)<<1)|((MMU)<<2))))
#define F_INT_ENTRY_REPLACEMENT_FAULT(MMU)     (1<<(3+(((MMU)<<1)|((MMU)<<2))))
#define F_INT_TLB_MISS_FAULT(MMU)              (1<<(4+(((MMU)<<1)|((MMU)<<2))))
#define F_INT_PFH_FIFO_ERR(MMU)                (1<<(6+(((MMU)<<1)|((MMU)<<2))))

#define REG_MMU_CPE_DONE              0x12C

#define REG_MMU_MAIN_FAULT_ST         0x134

#define REG_MMU_FAULT_VA(mmu)         (0x13c+((mmu)<<3))
#define F_MMU_FAULT_VA_MSK            ((~0x0)<<12)
#define F_MMU_FAULT_VA_WRITE_BIT      (1<<1)
#define F_MMU_FAULT_VA_LAYER_BIT      (1<<0)

#define REG_MMU_INVLD_PA(mmu)         (0x140+((mmu)<<3))
#define REG_MMU_INT_ID(mmu)           (0x150+((mmu)<<2))
#define F_MMU0_INT_ID_TF_MSK          (~0x3)	/* only for MM iommu. */

/*smi larb*/
#define SMI_LARB_MMU_EN                 (0xf00)
#define F_SMI_MMU_EN(port)              (1<<((port)))
#define SMI_LARB_SEC_EN                 (0xf04)
#define F_SMI_SEC_EN(port)              (1<<((port)))

#endif

