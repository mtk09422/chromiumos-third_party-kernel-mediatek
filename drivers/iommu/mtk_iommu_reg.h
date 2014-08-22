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

/*common macro definitions*/
#define F_VAL(val, msb, lsb) (((val) & ((1<<(msb-lsb+1))-1))<<lsb)
#define F_MSK(msb, lsb)     F_VAL(0xffffffff, msb, lsb)
#define F_BIT_SET(bit)          (1<<(bit))
#define F_BIT_VAL(val, bit)  ((!!(val))<<(bit))
#define F_MSK_SHIFT(regval, msb, lsb) (((regval)&F_MSK(msb, lsb))>>lsb)

#define F_DESC_VALID                F_VAL(0x2, 1, 0)
#define F_DESC_SHARE(en)            F_BIT_VAL(en, 2)
#define F_DESC_NONSEC(non_sec)      F_BIT_VAL(non_sec, 3)
#define F_DESC_PA_MSK               F_MSK(31, 12)

/*M4U_BASEg*/
#define REG_MMUG_CTRL			(0x0)
#define F_MMUG_CTRL_INV_EN0		(1<<0)
#define F_MMUG_CTRL_INV_EN1		(1<<1)
#define F_MMUG_CTRL_INV_EN2		(1<<2)
#define F_MMUG_CTRL_PRE_LOCK(en)    F_BIT_VAL(en, 3)
#define F_MMUG_CTRL_PRE_EN		(1<<4)

#define REG_MMUG_INVLD			0x4
#define F_MMUG_INV_ALL			0x2
#define F_MMUG_INV_RANGE		0x1

#define REG_MMUG_INVLD_SA			0x8
#define REG_MMUG_INVLD_EA			0xC
#define REG_MMUG_PT_BASE			0x10
#define F_MMUG_PT_VA_MSK			0xffff0000

#define REG_MMUG_L2_SEL				0x18
#define F_MMUG_L2_SEL_FLUSH_EN(en)          F_BIT_VAL(en, 3)
#define F_MMUG_L2_SEL_L2_ULTRA(en)          F_BIT_VAL(en, 2)
#define F_MMUG_L2_SEL_L2_SHARE(en)          F_BIT_VAL(en, 1)
#define F_MMUG_L2_SEL_L2_BUS_SEL(go_emi)    F_BIT_VAL(go_emi, 0)

#define REG_MMUG_DCM               (0x1C)
#define F_MMUG_DCM_ON(on)       F_BIT_VAL(on, 0)

/*M4U_L2_BASE*/
#define REG_L2_GDC_STATE        (0x0)
#define F_L2_GDC_ST_LOCK_ALERT_GET(regval)  F_MSK_SHIFT(regval, 15, 8)
#define F_L2_GDC_ST_LOCK_ALERT_BIT          F_BIT_SET(15)
#define F_L2_GDC_ST_LOCK_ALERT_SET_IDX_GET(regval)  F_MSK_SHIFT(regval, 14, 8)
#define F_L2_GDC_ST_EVENT_GET(regval)       F_MSK_SHIFT(regval, 7, 6)
#define F_L2_GDC_ST_EVENT_MSK               F_MSK(7, 6)
#define F_L2_GDC_ST_EVENT_VAL(val)          F_VAL(val, 7, 6)
#define F_L2_GDC_ST_OP_ST_GET(regval)       F_MSK_SHIFT(regval, 5, 1)
#define F_L2_GDC_ST_BUSY_GET(regval)        F_MSK_SHIFT(regval, 0, 0)

#define REG_L2_GDC_OP           (0x4)
#define F_L2_GDC_BYPASS(en)             F_BIT_VAL(en, 10)
#define F_GET_L2_GDC_PERF_MASK(regval)  F_MSK_SHIFT(regval, 9, 7)
#define F_L2_GDC_PERF_MASK(msk)         F_VAL(msk, 9, 7)
#define GDC_PERF_MASK_HIT_MISS              0
#define GDC_PERF_MASK_RI_RO                 1
#define GDC_PERF_MASK_BUSY_CYCLE            3
#define GDC_PERF_MASK_READ_OUTSTAND_FIFO    3
#define F_L2_GDC_LOCK_ALERT_DIS(dis)    F_BIT_VAL(dis, 6)
#define F_L2_GDC_PERF_EN(en)            F_BIT_VAL(en, 5)
#define F_L2_GDC_SRAM_MODE(en)          F_BIT_VAL(en, 4)
#define F_L2_GDC_LOCK_TH(th)            F_VAL(th, 3, 2)
#define F_L2_GDC_PAUSE_OP(op)           F_VAL(op, 1, 0)
#define GDC_NO_PAUSE            0
#define GDC_READ_PAUSE          1
#define GDC_WRITE_PAUSE         2
#define GDC_READ_WRITE_PAUSE    3

#define REG_L2_GPE_STATUS       (0x18)
#define F_L2_GPE_ST_RANGE_INV_DONE  2
#define F_L2_GPE_ST_PREFETCH_DONE  1

/*m4u 0/1*/
#define REG_MMU_MAIN_TAG_0_31(x)       (0x100+((x)<<2))
#define REG_MMU_MAIN_TAG_32_63(x)      (0x400+(((x)-32)<<2))
#define REG_MMU_MAIN_TAG(x)	(((x) < 32) \
	? REG_MMU_MAIN_TAG_0_31(x) : REG_MMU_MAIN_TAG_32_63(x))
#define F_MAIN_TLB_LOCK_BIT     (1<<11)
#define F_MAIN_TLB_VALID_BIT    (1<<10)
#define F_MAIN_TLB_SQ_EN_BIT    (1<<9)
#define F_MAIN_TLB_SQ_INDEX_MSK (0xf<<5)
#define F_MAIN_TLB_INV_DES_BIT      (1<<4)
#define F_MAIN_TLB_VA_MSK           F_MSK(31, 12)
#define REG_MMU_PFH_TAG_0_31(x)       (0x180+((x)<<2))
#define REG_MMU_PFH_TAG_32_63(x)      (0x480+(((x)-32)<<2))
#define REG_MMU_PFH_TAG(x)	(((x) < 32) \
	? REG_MMU_PFH_TAG_0_31(x) : REG_MMU_PFH_TAG_32_63(x))
#define F_PFH_TAG_VA_MSK            F_MSK(31, 14)
#define F_PFH_TAG_VALID_MSK         F_MSK(13, 10)
#define F_PFH_TAG_VALID(regval)     F_MSK_SHIFT((regval), 13, 10)
#define F_PFH_TAG_VALID_MSK_OF_MVA(mva)	((F_BIT_SET(10)) \
	<< (F_MSK_SHIFT((mva), 13, 10)))
#define F_PFH_TAG_DESC_VALID_MSK         F_MSK(9, 6)
#define F_PFH_TAG_DESC_VALID_MSK_OF_MVA(mva) ((F_BIT_SET(6)) \
	<< (F_MSK_SHIFT(mva, 13, 10)))
#define F_PFH_TAG_DESC_VALID(regval)     F_MSK_SHIFT((regval), 9, 6)
#define F_PFH_TAG_REQUEST_BY_PFH    F_BIT_SET(5)
#define REG_MMU_CTRL_REG         0x210
#define F_MMU_CTRL_PFH_DIS(dis)         F_BIT_VAL(dis, 0)
#define F_MMU_CTRL_TLB_WALK_DIS(dis)    F_BIT_VAL(dis, 1)
#define F_MMU_CTRL_MONITOR_EN(en)       F_BIT_VAL(en, 2)
#define F_MMU_CTRL_MONITOR_CLR(clr)       F_BIT_VAL(clr, 3)
#define F_MMU_CTRL_PFH_RT_RPL_MODE(mod)   F_BIT_VAL(mod, 4)
#define F_MMU_CTRL_TF_PROT_VAL(prot)    F_VAL(prot, 6, 5)
#define F_MMU_CTRL_TF_PROT_MSK           F_MSK(6, 5)
#define F_MMU_CTRL_INT_HANG_en(en)       F_BIT_VAL(en, 7)
#define F_MMU_CTRL_COHERE_EN(en)        F_BIT_VAL(en, 8)
#define REG_MMU_IVRP_PADDR       0x214
#define REG_MMU_INT_CONTROL      0x220
#define F_INT_CLR_BIT (1<<12)
#define REG_MMU_FAULT_ST         0x224
#define F_INT_TRANSLATION_FAULT                 F_BIT_SET(0)
#define F_INT_TLB_MULTI_HIT_FAULT               F_BIT_SET(1)
#define F_INT_INVALID_PHYSICAL_ADDRESS_FAULT    F_BIT_SET(2)
#define F_INT_ENTRY_REPLACEMENT_FAULT           F_BIT_SET(3)
#define F_INT_TABLE_WALK_FAULT                  F_BIT_SET(4)
#define F_INT_TLB_MISS_FAULT                    F_BIT_SET(5)
#define F_INT_PFH_DMA_FIFO_OVERFLOW             F_BIT_SET(6)
#define F_INT_MISS_DMA_FIFO_OVERFLOW            F_BIT_SET(7)
#define F_INT_PFH_FIFO_ERR                      F_BIT_SET(8)
#define F_INT_MISS_FIFO_ERR                     F_BIT_SET(9)
#define REG_MMU_FAULT_VA         0x228
#define REG_MMU_INVLD_PA         0x22C
#define REG_MMU_INT_ID              0x388
#define F_INT_ID_TF_PORT_ID(regval)     F_MSK_SHIFT(regval, 11, 8)
#define F_INT_ID_TF_LARB_ID(regval)     F_MSK_SHIFT(regval, 14, 12)
#define F_INT_ID_MH_PORT_ID(regval)     F_MSK_SHIFT(regval, 27, 24)
#define F_INT_ID_MH_LARB_ID(regval)     F_MSK_SHIFT(regval, 30, 28)

/*SMI_COMMON_AO_BASE*/
#define REG_SMI_SECUR_CON_G3D			(0x05E8)
#define REG_SMI_SECUR_CON(x)			(0x05C0+((x)<<2))
#define REG_SMI_SECUR_CON_OF_PORT(port)	REG_SMI_SECUR_CON(\
		((m4u_port_2_smi_port(port)) >> 3))
#define F_SMI_SECUR_CON_SECURE(port)	((1)\
		<<(((m4u_port_2_smi_port(port))&0x7)<<2))
#define F_SMI_SECUR_CON_DOMAIN(port, val) (((val)&0x3)\
		<<((((m4u_port_2_smi_port(port))&0x7)<<2)+1))
#define F_SMI_SECUR_CON_VIRTUAL(port)	((1)<<\
		((((m4u_port_2_smi_port(port))&0x7)<<2)+3))
#define F_SMI_SECUR_CON_MASK(port)	((0xf)<<\
		((((m4u_port_2_smi_port(port))&0x7)<<2)))

/*SMI_COMMON_EXT_BASE*/
#define REG_SMI_BUS_SEL	                (0x220)
#define F_SMI_BUS_SEL_larb0(mmu_idx)     F_VAL(mmu_idx, 1, 0)
#define F_SMI_BUS_SEL_larb1(mmu_idx)     F_VAL(mmu_idx, 3, 2)
#define F_SMI_BUS_SEL_larb2(mmu_idx)     F_VAL(mmu_idx, 5, 4)
#define F_SMI_BUS_SEL_larb3(mmu_idx)     F_VAL(mmu_idx, 7, 6)
#define F_SMI_BUS_SEL_larb4(mmu_idx)     F_VAL(mmu_idx, 9, 8)
#define F_SMI_BUS_SEL_larb5(mmu_idx)     F_VAL(mmu_idx, 11, 10)
#define F_SMI_BUS_SEL_larb6(mmu_idx)     F_VAL(mmu_idx, 13, 12)

/*larb item base*/
#define SMI_SHARE_EN         (0x210)
#define F_SMI_SHARE_EN(port)     F_BIT_SET(m4u_port_2_larb_port(port))
#define SMI_ROUTE_SEL     (0x220)
#define F_SMI_ROUTE_SEL_EMI(port)    F_BIT_SET(m4u_port_2_larb_port(port))


#endif

