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
#include <linux/err.h>
#include <linux/mm.h>
#include <linux/iommu.h>
#include <linux/errno.h>
#include "asm/cacheflush.h"

#include "mtk_iommu.h"
#include "mtk_iommu_pagetable.h"

/* 2 level pagetable: pgd -> pte */
#define F_PTE_TYPE_GET(regval)  (regval & 0x3)
#define F_PTE_TYPE_LARGE         BIT(0)
#define F_PTE_TYPE_SMALL         BIT(1)
#define F_PTE_B_BIT              BIT(2)
#define F_PTE_C_BIT              BIT(3)
#define F_PTE_BIT32_BIT          BIT(9)
#define F_PTE_S_BIT              BIT(10)
#define F_PTE_NG_BIT             BIT(11)
#define F_PTE_PA_LARGE_MSK            (~0UL << 16)
#define F_PTE_PA_LARGE_GET(regval)    ((regval >> 16) & 0xffff)
#define F_PTE_PA_SMALL_MSK            (~0UL << 12)
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
		((imu_pgd_val(pgd) & F_PGD_TYPE_SECTION_MSK) ==\
			F_PGD_TYPE_SUPERSECTION))

#define F_PGD_B_BIT                     BIT(2)
#define F_PGD_C_BIT                     BIT(3)
#define F_PGD_BIT32_BIT                 BIT(9)
#define F_PGD_S_BIT                     BIT(16)
#define F_PGD_NG_BIT                    BIT(17)
#define F_PGD_NS_BIT_PAGE(ns)           (ns << 3)
#define F_PGD_NS_BIT_SECTION(ns)        (ns << 19)
#define F_PGD_NS_BIT_SUPERSECTION(ns)   (ns << 19)

#define imu_pgd_index(addr)		((addr) >> IMU_PGDIR_SHIFT)
#define imu_pgd_offset(domain, addr) ((domain)->pgd + imu_pgd_index(addr))

#define imu_pte_index(addr)    (((addr)>>IMU_PAGE_SHIFT)&(IMU_PTRS_PER_PTE - 1))
#define imu_pte_offset_map(pgd, addr) (imu_pte_map(pgd) + imu_pte_index(addr))

#define F_PGD_PA_PAGETABLE_MSK            (~0 << 10)
#define F_PGD_PA_SECTION_MSK              (~0 << 20)
#define F_PGD_PA_SUPERSECTION_MSK         (~0 << 24)

static inline struct imu_pte_t *imu_pte_map(struct imu_pgd_t *pgd)
{
	unsigned int pte_pa = imu_pgd_val(*pgd);

	return (struct imu_pte_t *)(__va(pte_pa
					& F_PGD_PA_PAGETABLE_MSK));
}

static inline struct imu_pgd_t *imu_supersection_start(struct imu_pgd_t *pgd)
{
	return (struct imu_pgd_t *)(round_down((unsigned long)pgd, (16 * 4)));
}

static inline void m4u_set_pgd_val(struct imu_pgd_t *pgd, unsigned int val)
{
	imu_pgd_val(*pgd) = val;
}

static inline unsigned int __m4u_get_pgd_attr(unsigned int prot,
					      bool super, bool imu4gmode)
{
	unsigned int pgprot;

	pgprot = F_PGD_NS_BIT_SECTION(1) | F_PGD_S_BIT;
	pgprot |= super ? F_PGD_TYPE_SUPERSECTION : F_PGD_TYPE_SECTION;
	pgprot |= (prot & IOMMU_CACHE) ? (F_PGD_C_BIT | F_PGD_B_BIT) : 0;
	pgprot |= imu4gmode ? F_PGD_BIT32_BIT : 0;

	return pgprot;
}

static inline unsigned int __m4u_get_pte_attr(unsigned int prot,
					      bool large, bool imu4gmode)
{
	unsigned int pgprot;

	pgprot = F_PTE_S_BIT;
	pgprot |= large ? F_PTE_TYPE_LARGE : F_PTE_TYPE_SMALL;
	pgprot |= (prot & IOMMU_CACHE) ? (F_PGD_C_BIT | F_PGD_B_BIT) : 0;
	pgprot |= imu4gmode ? F_PTE_BIT32_BIT : 0;

	return pgprot;
}

static inline void m4u_pgtable_flush(void *vastart, void *vaend)
{
	/*
	 * this function is not acceptable, we will use dma_map_single
	 * or use dma_pool_create for the level2 pagetable.
	 */
	__dma_flush_range(vastart, vaend);
}

/* @return   0 -- pte is allocated
 *     1 -- pte is not allocated, because it's allocated by others
 *     <0 -- error
 */
static int m4u_alloc_pte(struct mtk_iommu_domain *domain, struct imu_pgd_t *pgd,
			 unsigned int pgprot)
{
	void *pte_new_va;
	phys_addr_t pte_new;
	struct kmem_cache *pte_kmem = domain->piommuinfo->m4u_pte_kmem;
	struct device *dev = domain->piommuinfo->dev;
	unsigned int ret;

	pte_new_va = kmem_cache_zalloc(pte_kmem, GFP_KERNEL);
	if (unlikely(!pte_new_va)) {
		dev_err(dev, "%s:fail, no memory\n", __func__);
		return -ENOMEM;
	}
	pte_new = __virt_to_phys(pte_new_va);

	/* check pte alignment -- must 1K align */
	if (unlikely(pte_new & (IMU_BYTES_PER_PTE - 1))) {
		dev_err(dev, "%s:fail, not align pa=0x%pa, va=0x%p\n",
			__func__, &pte_new, pte_new_va);
		kmem_cache_free(pte_kmem, (void *)pte_new_va);
		return -ENOMEM;
	}

	/* because someone else may have allocated for this pgd first */
	if (likely(!imu_pgd_val(*pgd))) {
		m4u_set_pgd_val(pgd, (unsigned int)(pte_new) | pgprot);
		dev_dbg(dev, "%s:pgd:0x%p,pte_va:0x%p,pte_pa:%pa,value:0x%x\n",
			__func__, pgd, pte_new_va,
			&pte_new, (unsigned int)(pte_new) | pgprot);
		ret = 0;
	} else {
		/* allocated by other thread */
		dev_dbg(dev, "m4u pte allocated by others: pgd=0x%p\n", pgd);
		kmem_cache_free(pte_kmem, (void *)pte_new_va);
		ret = 1;
	}
	return ret;
}

static int m4u_free_pte(struct mtk_iommu_domain *domain, struct imu_pgd_t *pgd)
{
	struct imu_pte_t *pte_old;
	struct kmem_cache *pte_kmem = domain->piommuinfo->m4u_pte_kmem;

	pte_old = imu_pte_map(pgd);
	m4u_set_pgd_val(pgd, 0);

	kmem_cache_free(pte_kmem, pte_old);

	return 0;
}

static int m4u_map_page(struct mtk_iommu_domain *m4u_domain, unsigned int iova,
			phys_addr_t pa, unsigned int prot, bool largepage)
{
	int ret;
	struct imu_pgd_t *pgd;
	struct imu_pte_t *pte;
	unsigned int pte_new, pgprot;
	unsigned int padscpt;
	struct device *dev = m4u_domain->piommuinfo->dev;
	unsigned int mask = largepage ?
				F_PTE_PA_LARGE_MSK : F_PTE_PA_SMALL_MSK;
	unsigned int i, ptenum = largepage ? 16 : 1;
	bool imu4gmode = (pa > 0xffffffffL) ? true : false;

	if ((iova & (~mask)) != ((unsigned int)pa & (~mask))) {
		dev_err(dev, "error to mk_pte: iova=0x%x, pa=0x%pa, type=%s\n",
			iova, &pa, largepage ? "large page" : "small page");
		return -EINVAL;
	}

	iova &= mask;
	padscpt = (unsigned int)pa & mask;

	pgprot = F_PGD_TYPE_PAGE | F_PGD_NS_BIT_PAGE(1);
	pgd = imu_pgd_offset(m4u_domain, iova);
	if (!imu_pgd_val(*pgd)) {
		ret = m4u_alloc_pte(m4u_domain, pgd, pgprot);
		if (ret < 0)
			return ret;
		else if (ret > 0)
			pte_new = 0;
		else
			pte_new = 1;
	} else {
		if ((imu_pgd_val(*pgd) & (~F_PGD_PA_PAGETABLE_MSK)) != pgprot) {
			dev_err(dev, "%s: iova=0x%x, pgd=0x%x, pgprot=0x%x\n",
				__func__, iova, imu_pgd_val(*pgd), pgprot);
			return -1;
		}
		pte_new = 0;
	}

	pgprot = __m4u_get_pte_attr(prot, largepage, imu4gmode);
	pte = imu_pte_offset_map(pgd, iova);

	dev_dbg(dev, "%s:iova:0x%x,pte:0x%p(0x%p+0x%x),pa:%pa,value:0x%x-%s\n",
		__func__, iova, &imu_pte_val(*pte), imu_pte_map(pgd),
		imu_pte_index(iova), &pa, padscpt | pgprot,
		largepage ? "large page" : "small page");

	for (i = 0; i < ptenum; i++) {
		if (imu_pte_val(pte[i])) {
			dev_err(dev, "%s: pte=0x%x, i=%d\n", __func__,
				imu_pte_val(pte[i]), i);
			goto err_out;
		}
		imu_pte_val(pte[i]) = padscpt | pgprot;
	}

	m4u_pgtable_flush(pte, pte + ptenum);

	return 0;

 err_out:
	for (i--; i >= 0; i--)
		imu_pte_val(pte[i]) = 0;
	return -EEXIST;
}

static int m4u_map_section(struct mtk_iommu_domain *m4u_domain,
			   unsigned int iova, phys_addr_t pa,
			   unsigned int prot, bool supersection)
{
	int i;
	struct imu_pgd_t *pgd;
	unsigned int pgprot;
	unsigned int padscpt;
	struct device *dev = m4u_domain->piommuinfo->dev;
	unsigned int mask = supersection ?
			F_PGD_PA_SUPERSECTION_MSK : F_PGD_PA_SECTION_MSK;
	unsigned int pgdnum = supersection ? 16 : 1;
	bool imu4gmode = (pa > 0xffffffffL) ? true : false;

	if ((iova & (~mask)) != ((unsigned int)pa & (~mask))) {
		dev_err(dev, "error to mk_pte: iova=0x%x, pa=0x%pa,type=%s\n",
			iova, &pa, supersection ? "supersection" : "section");
		return -EINVAL;
	}

	iova &= mask;
	padscpt = (unsigned int)pa & mask;

	pgprot = __m4u_get_pgd_attr(prot, supersection, imu4gmode);
	pgd = imu_pgd_offset(m4u_domain, iova);

	dev_dbg(dev, "%s:iova:0x%x,pgd:0x%p(0x%p+0x%x),pa:%pa,value:0x%x-%s\n",
		__func__, iova, pgd, (m4u_domain)->pgd, imu_pgd_index(iova),
		&pa, padscpt | pgprot,
		supersection ? "supersection" : "section");

	for (i = 0; i < pgdnum; i++) {
		if (unlikely(imu_pgd_val(*pgd))) {
			dev_err(dev, "%s:iova=0x%x, pgd=0x%x, i=%d\n", __func__,
				iova, imu_pgd_val(*pgd), i);
			goto err_out;
		}
		m4u_set_pgd_val(pgd, padscpt | pgprot);
		pgd++;
	}
	return 0;

 err_out:
	for (pgd--; i > 0; i--) {
		m4u_set_pgd_val(pgd, 0);
		pgd--;
	}
	return -EEXIST;
}

int m4u_map(struct mtk_iommu_domain *m4u_domain, unsigned int iova,
	    phys_addr_t paddr, unsigned int size, unsigned int prot)
{
	if (size == SZ_4K) {/* most case */
		return m4u_map_page(m4u_domain, iova, paddr, prot, false);
	} else if (size == SZ_64K) {
		return m4u_map_page(m4u_domain, iova, paddr, prot, true);
	} else if (size == SZ_1M) {
		return m4u_map_section(m4u_domain, iova, paddr, prot, false);
	} else if (size == SZ_16M) {
		return m4u_map_section(m4u_domain, iova, paddr, prot, true);
	} else {
		return -EINVAL;
	}
}

static int m4u_check_free_pte(struct mtk_iommu_domain *domain,
			      struct imu_pgd_t *pgd)
{
	struct imu_pte_t *pte;
	int i;

	pte = imu_pte_map(pgd);
	for (i = 0; i < IMU_PTRS_PER_PTE; i++, pte++) {
		if (imu_pte_val(*pte) != 0)
			return 1;
	}

	m4u_free_pte(domain, pgd);
	return 0;
}

int m4u_unmap(struct mtk_iommu_domain *domain, unsigned int iova,
	      unsigned int size)
{
	struct imu_pgd_t *pgd;
	int i, ret;
	unsigned long end_plus_1 = (unsigned long)iova + size;

	do {
		pgd = imu_pgd_offset(domain, iova);

		if (F_PGD_TYPE_IS_PAGE(*pgd)) {
			struct imu_pte_t *pte;
			unsigned int pte_offset;
			unsigned int num_to_clean;

			pte_offset = imu_pte_index(iova);
			num_to_clean =
			    min((unsigned int)((end_plus_1 - iova) / PAGE_SIZE),
				(unsigned int)(IMU_PTRS_PER_PTE - pte_offset));

			pte = imu_pte_offset_map(pgd, iova);

			memset(pte, 0, num_to_clean << 2);

			ret = m4u_check_free_pte(domain, pgd);
			if (ret == 1)/* pte is not freed, need to flush pte */
				m4u_pgtable_flush(pte, pte + num_to_clean);

			iova += num_to_clean << PAGE_SHIFT;
		} else if (F_PGD_TYPE_IS_SECTION(*pgd)) {
			m4u_set_pgd_val(pgd, 0);
			iova += MMU_SECTION_SIZE;
		} else if (F_PGD_TYPE_IS_SUPERSECTION(*pgd)) {
			struct imu_pgd_t *start = imu_supersection_start(pgd);

			if (unlikely(start != pgd))
				dev_err(domain->piommuinfo->dev,
					"%s:supper not align,iova=0x%x,pgd=0x%x\n",
					__func__, iova, imu_pgd_val(*pgd));

			for (i = 0; i < 16; i++)
				m4u_set_pgd_val((start+i), 0);

			iova = (iova + MMU_SUPERSECTION_SIZE) &
				(~(MMU_SUPERSECTION_SIZE - 1));
		} else {
			iova += MMU_SECTION_SIZE;
		}
	} while (iova < end_plus_1 && iova);

	return 0;
}

int m4u_get_pte_info(const struct mtk_iommu_domain *domain, unsigned int iova,
		     struct m4u_pte_info_t *pte_info)
{
	struct imu_pgd_t *pgd;
	struct imu_pte_t *pte;
	unsigned int pa = 0;
	unsigned int size;
	int valid = 1;

	pgd = imu_pgd_offset(domain, iova);

	if (F_PGD_TYPE_IS_PAGE(*pgd)) {
		pte = imu_pte_offset_map(pgd, iova);
		if (F_PTE_TYPE_GET(imu_pte_val(*pte)) == F_PTE_TYPE_LARGE) {
			pa = imu_pte_val(*pte) & F_PTE_PA_LARGE_MSK;
			pa |= iova & (~F_PTE_PA_LARGE_MSK);
			size = MMU_LARGE_PAGE_SIZE;
		} else if (F_PTE_TYPE_GET(imu_pte_val(*pte))
			   == F_PTE_TYPE_SMALL) {
			pa = imu_pte_val(*pte) & F_PTE_PA_SMALL_MSK;
			pa |= iova & (~F_PTE_PA_SMALL_MSK);
			size = MMU_SMALL_PAGE_SIZE;
		} else {
			valid = 0;
			size = MMU_SMALL_PAGE_SIZE;
		}
	} else {
		pte = NULL;
		if (F_PGD_TYPE_IS_SECTION(*pgd)) {
			pa = imu_pgd_val(*pgd) & F_PGD_PA_SECTION_MSK;
			pa |= iova & (~F_PGD_PA_SECTION_MSK);
			size = MMU_SECTION_SIZE;
		} else if (F_PGD_TYPE_IS_SUPERSECTION(*pgd)) {
			pa = imu_pgd_val(*pgd) & F_PGD_PA_SUPERSECTION_MSK;
			pa |= iova & (~F_PGD_PA_SUPERSECTION_MSK);
			size = MMU_SUPERSECTION_SIZE;
		} else {
			valid = 0;
			size = MMU_SECTION_SIZE;
		}
	}

	pte_info->pgd = pgd;
	pte_info->pte = pte;
	pte_info->iova = iova;
	pte_info->pa = pa;
	pte_info->size = size;
	pte_info->valid = valid;
	return 0;
}

