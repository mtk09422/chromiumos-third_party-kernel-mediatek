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

struct imu_pte_t {
	unsigned int imu_pte;
};

struct imu_pgd_t {
	unsigned int imu_pgd;
};

#define imu_pte_val(x)      ((x).imu_pte)
#define imu_pgd_val(x)      ((x).imu_pgd)

struct m4u_pte_info_t {
	struct imu_pgd_t *pgd;
	struct imu_pte_t *pte;
	unsigned int iova;
	phys_addr_t pa;
	unsigned int size;
	int valid;
};

#endif

