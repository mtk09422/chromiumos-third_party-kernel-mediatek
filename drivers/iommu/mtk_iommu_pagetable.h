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
#ifndef MTK_IOMMU_PAGETABLE_H
#define MTK_IOMMU_PAGETABLE_H

#define MMU_SMALL_PAGE_SIZE     (SZ_4K)
#define MMU_LARGE_PAGE_SIZE     (SZ_64K)
#define MMU_SECTION_SIZE        (SZ_1M)
#define MMU_SUPERSECTION_SIZE   (SZ_16M)

#define IMU_PGDIR_SHIFT   20
#define IMU_PAGE_SHIFT    12
#define IMU_PTRS_PER_PGD  4096
#define IMU_PTRS_PER_PTE  256
#define IMU_BYTES_PER_PTE (IMU_PTRS_PER_PTE*sizeof(unsigned int))

#define MMU_PT_TYPE_SUPERSECTION    (1<<4)
#define MMU_PT_TYPE_SECTION         (1<<3)
#define MMU_PT_TYPE_LARGE_PAGE      (1<<2)
#define MMU_PT_TYPE_SMALL_PAGE      (1<<1)

struct imu_pte_t {
	unsigned int imu_pte;
};

struct imu_pgd_t {
	unsigned int imu_pgd;
};

#define imu_pte_val(x)      ((x).imu_pte)
#define imu_pgd_val(x)      ((x).imu_pgd)

#define __imu_pte(x)    ((imu_pte_t){(x)})
#define __imu_pgd(x)    ((imu_pgd_t){(x)})

#define imu_pte_none(pte)   (!imu_pte_val(pte))
#define imu_pte_type(pte)   (imu_pte_val(pte) & 0x3)

#define imu_pgd_index(addr)		((addr) >> IMU_PGDIR_SHIFT)
#define imu_pgd_offset(domain, addr) ((domain)->pgd + imu_pgd_index(addr))

#define imu_pte_index(addr)    (((addr)>>IMU_PAGE_SHIFT)&(IMU_PTRS_PER_PTE - 1))
#define imu_pte_offset_map(pgd, addr) (imu_pte_map(pgd) + imu_pte_index(addr))

/* 2 level pagetable: pgd -> pte */
#define F_PTE_TYPE_GET(regval)  (regval & 0x3)
#define F_PTE_TYPE_LARGE        (0x1)
#define F_PTE_TYPE_SMALL        (0x2)
#define F_PTE_B_BIT             (1 << 2)
#define F_PTE_C_BIT             (1 << 3)
#define F_PTE_BIT32_BIT           (1 << 9)
#define F_PTE_S_BIT               (1 << 10)
#define F_PTE_NG_BIT              (1 << 11)
#define F_PTE_PA_LARGE_MSK            (~0 << 16)
#define F_PTE_PA_LARGE_GET(regval)    ((regval >> 16) & 0xffff)
#define F_PTE_PA_SMALL_MSK            (~0 << 12)
#define F_PTE_PA_SMALL_GET(regval)    ((regval >> 12) & (~0))
#define F_PTE_TYPE_IS_LARGE_PAGE(pte) ((imu_pte_val(pte) & 0x3) == \
					F_PTE_TYPE_LARGE)
#define F_PTE_TYPE_IS_SMALL_PAGE(pte) ((imu_pte_val(pte) & 0x3) == \
					F_PTE_TYPE_SMALL)

#define F_PGD_TYPE_PAGE         (0x1)
#define F_PGD_TYPE_PAGE_MSK     (0x3)
#define F_PGD_TYPE_SECTION      (0x2)
#define F_PGD_TYPE_SUPERSECTION   (0x2 | (1 << 18))
#define F_PGD_TYPE_SECTION_MSK    (0x3 | (1 << 18))
#define F_PGD_TYPE_IS_PAGE(pgd)   ((imu_pgd_val(pgd)&3) == 1)
#define F_PGD_TYPE_IS_SECTION(pgd) \
	(F_PGD_TYPE_IS_PAGE(pgd) ? 0 : \
		((imu_pgd_val(pgd) & F_PGD_TYPE_SECTION_MSK) == \
			F_PGD_TYPE_SECTION))
#define F_PGD_TYPE_IS_SUPERSECTION(pgd) \
	(F_PGD_TYPE_IS_PAGE(pgd) ? 0 : \
		((imu_pgd_val(pgd)&F_PGD_TYPE_SECTION_MSK) ==\
			F_PGD_TYPE_SUPERSECTION))

#define F_PGD_B_BIT                     (1 << 2)
#define F_PGD_C_BIT                     (1 << 3)
#define F_PGD_BIT32_BIT                 (1 << 9)
#define F_PGD_S_BIT                     (1 << 16)
#define F_PGD_NG_BIT                    (1 << 17)
#define F_PGD_NS_BIT_PAGE(ns)           (ns << 3)
#define F_PGD_NS_BIT_SECTION(ns)        (ns << 19)
#define F_PGD_NS_BIT_SUPERSECTION(ns)   (ns << 19)

#define F_PGD_PA_PAGETABLE_MSK            (~0 << 10)
#define F_PGD_PA_SECTION_MSK              (~0 << 20)
#define F_PGD_PA_SUPERSECTION_MSK         (~0 << 24)

struct m4u_pte_info_t {
	struct imu_pgd_t *pgd;
	struct imu_pte_t *pte;
	unsigned int iova;
	phys_addr_t pa;
	unsigned int size;
	int valid;
};

static inline struct imu_pte_t *imu_pte_map(struct imu_pgd_t *pgd)
{
	return (struct imu_pte_t *)__va(imu_pgd_val(*pgd) &
					F_PGD_PA_PAGETABLE_MSK);
}

static inline int imu_pte_unmap(struct imu_pte_t *pte)
{
	return 0;
}

static inline struct imu_pgd_t *imu_supersection_start(struct imu_pgd_t *pgd)
{
	return (struct imu_pgd_t *)(round_down((unsigned long)pgd, (16 * 4)));
}

static inline struct imu_pte_t *imu_largepage_start(struct imu_pte_t *pte)
{
	return (struct imu_pte_t *)(round_down((unsigned long)pte, (16 * 4)));
}

static inline unsigned int m4u_calc_next_iova(unsigned int addr,
					      unsigned int end,
					      unsigned int size)
{
	unsigned int __boundary = (addr + size) & (~(size - 1));

	/*if addr is 0xfffc0000, sz is 1M, then __boundary is 0*/
	__boundary = (__boundary) ? __boundary : end;

	return min(__boundary, end);
}

#endif

