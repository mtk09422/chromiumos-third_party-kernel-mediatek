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
#include <linux/err.h>
#include <linux/mm.h>
#include <linux/iommu.h>
#include <linux/errno.h>
#include "asm/cacheflush.h"

#include "mtk_iommu_reg_mt8173.h"
#include "mtk_iommu_pagetable.h"
#include "mtk_iommu_platform.h"

static inline void m4u_set_pgd_val(struct imu_pgd_t *pgd, unsigned int val)
{
	imu_pgd_val(*pgd) = val;
}

static inline unsigned int __m4u_get_pgd_attr_16M(unsigned int prot)
{
	unsigned int pgprot;

	pgprot = F_PGD_TYPE_SUPERSECTION;
	pgprot |= F_PGD_NS_BIT_SECTION(1);
	pgprot |= F_PGD_S_BIT;
	pgprot |= (prot & IOMMU_CACHE) ? (F_PGD_C_BIT | F_PGD_B_BIT) : 0;
	return pgprot;
}

static inline unsigned int __m4u_get_pgd_attr_1M(unsigned int prot)
{
	unsigned int pgprot;

	pgprot = F_PGD_TYPE_SECTION;
	pgprot |= F_PGD_NS_BIT_SECTION(1);
	pgprot |= F_PGD_S_BIT;
	pgprot |= (prot & IOMMU_CACHE) ? (F_PGD_C_BIT | F_PGD_B_BIT) : 0;
	return pgprot;
}

static inline unsigned int __m4u_get_pgd_attr_page(unsigned int prot)
{
	unsigned int pgprot;

	pgprot = F_PGD_TYPE_PAGE;
	pgprot |= F_PGD_NS_BIT_PAGE(1);
	return pgprot;
}

static inline unsigned int __m4u_get_pte_attr_64K(unsigned int prot)
{
	unsigned int pgprot;

	pgprot = F_PTE_TYPE_LARGE;
	pgprot |= F_PTE_S_BIT;
	pgprot |= (prot & IOMMU_CACHE) ? (F_PGD_C_BIT | F_PGD_B_BIT) : 0;
	return pgprot;
}

static inline unsigned int __m4u_get_pte_attr_4K(unsigned int prot)
{
	unsigned int pgprot;

	pgprot = F_PTE_TYPE_SMALL;
	pgprot |= F_PTE_S_BIT;
	pgprot |= (prot & IOMMU_CACHE) ? (F_PGD_C_BIT | F_PGD_B_BIT) : 0;
	return pgprot;
}

static inline void m4u_pgtable_flush(void *vastart, void *vaend)
{
	__dma_flush_range(vastart, vaend);
}

/*cache sync*/
static int m4u_clean_pte(struct mtk_iommu_domain *domain, unsigned int iova,
			 unsigned int size)
{
	struct imu_pgd_t *pgd;
	unsigned int end_plus_1 = iova + size;

	while (iova < end_plus_1) {
		pgd = imu_pgd_offset(domain, iova);

		if (F_PGD_TYPE_IS_PAGE(*pgd)) {
			struct imu_pte_t *pte, *pte_end;
			unsigned int next_iova, sync_entry_nr;

			pte = imu_pte_offset_map(pgd, iova);
			if (!pte) {
				/* invalid pte: goto next pgd entry */
				iova = m4u_calc_next_iova(iova, end_plus_1,
							  MMU_SECTION_SIZE);
				continue;
			}

			next_iova = m4u_calc_next_iova(iova, end_plus_1,
						       MMU_SECTION_SIZE);
			sync_entry_nr = (next_iova-iova) / MMU_SMALL_PAGE_SIZE;
			pte_end = pte + sync_entry_nr;

			/* do cache sync for [pte, pte_end) */
			m4u_pgtable_flush(pte, pte_end);

			imu_pte_unmap(pte);
			iova = next_iova;

		} else if (F_PGD_TYPE_IS_SUPERSECTION(*pgd)) {
			/* for superseciton: don't need to sync. */
			iova = m4u_calc_next_iova(iova, end_plus_1,
						  MMU_SUPERSECTION_SIZE);
		} else {
			/* for section/invalid: don't need to sync */
			iova = m4u_calc_next_iova(iova, end_plus_1,
						  MMU_SECTION_SIZE);
		}
	}

	return 0;
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
	struct kmem_cache *pte_kmem = domain->piommuinfo->imu_pte_kmem;
	struct device *dev = domain->piommuinfo->dev;
	unsigned int ret;

	pte_new_va = kmem_cache_zalloc(pte_kmem, GFP_KERNEL);
	if (unlikely(!pte_new_va)) {
		dev_err(dev, "%s:fail, no memory\n", __func__);
		return -ENOMEM;
	}
	pte_new = __pa(pte_new_va);

	/* check pte alignment -- must 1K align */
	if (unlikely(pte_new & (IMU_BYTES_PER_PTE - 1))) {
		dev_err(dev, "%s:fail, not align pa=0x%pa, va=0x%p\n",
			__func__, &pte_new, pte_new_va);
		kmem_cache_free(pte_kmem, (void *)pte_new_va);
		return -ENOMEM;
	}

	/* lock and check again */
	/* because someone else may have allocated for this pgd first */
	if (likely(!imu_pgd_val(*pgd))) {
		m4u_set_pgd_val(pgd, (unsigned int)(pte_new) | pgprot);
		dev_dbg(dev, "%s:pgd:0x%p,pte_va:0x%p,pte_pa:0x%pa,value:0x%x\n",
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

int m4u_free_pte(struct mtk_iommu_domain *domain, struct imu_pgd_t *pgd)
{
	struct imu_pte_t *pte_old;
	struct kmem_cache *pte_kmem = domain->piommuinfo->imu_pte_kmem;

	pte_old = imu_pte_map(pgd);
	m4u_set_pgd_val(pgd, 0);

	kmem_cache_free(pte_kmem, pte_old);

	return 0;
}

static int m4u_map_4K(struct mtk_iommu_domain *m4u_domain, unsigned int iova,
		      phys_addr_t pa, unsigned int prot)
{
	int ret, pte_new;
	struct imu_pgd_t *pgd;
	struct imu_pte_t *pte;
	unsigned int pgprot;
	unsigned int padscpt;
	struct device *dev = m4u_domain->piommuinfo->dev;

	if ((iova & (~F_PTE_PA_SMALL_MSK)) !=
	    ((unsigned int)pa & (~F_PTE_PA_SMALL_MSK))) {
		dev_err(dev, "error to mk_pte: iova=0x%x, pa=0x%pa, type=%s\n",
			iova, &pa, "small page");
		return -EINVAL;
	}

	iova &= F_PTE_PA_SMALL_MSK;
	if (pa > 0xffffffffL)
		padscpt = (unsigned int)pa &
				(F_PTE_PA_SMALL_MSK | F_PTE_BIT32_BIT);
	else
		padscpt = (unsigned int)pa & F_PTE_PA_SMALL_MSK;

	pgprot = __m4u_get_pgd_attr_page(prot);
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
		if (unlikely((imu_pgd_val(*pgd) & (~F_PGD_PA_PAGETABLE_MSK)) !=
		    pgprot)) {
			dev_err(dev, "%s: iova=0x%x, pgd=0x%x, pgprot=0x%x\n",
				__func__, iova, imu_pgd_val(*pgd), pgprot);
			return -1;
		}
		pte_new = 0;
	}

	pgprot = __m4u_get_pte_attr_4K(prot);
	pte = imu_pte_offset_map(pgd, iova);

	if (unlikely(imu_pte_val(*pte))) {
		dev_err(dev, "%s: pte=0x%x\n", __func__, imu_pte_val(*pte));
		goto err_out;
	}

	imu_pte_val(*pte) = padscpt | pgprot;

	dev_dbg(dev, "%s:iova:0x%x,pte:0x%p(0x%p + 0x%x),pa:0x%pa,value: 0x%x\n",
		__func__, iova, &imu_pte_val(*pte), imu_pte_map(pgd),
		imu_pte_index(iova), &pa, padscpt | imu_pte_val(*pte));

	imu_pte_unmap(pte);
	return 0;

err_out:
	imu_pte_unmap(pte);
	if (pte_new)
		m4u_free_pte(m4u_domain, pgd);
	return -1;
}

static int m4u_map_64K(struct mtk_iommu_domain *m4u_domain, unsigned int iova,
		       phys_addr_t pa, unsigned int prot)
{
	int ret, i;
	struct imu_pgd_t *pgd;
	struct imu_pte_t *pte;
	unsigned int pte_new, pgprot;
	unsigned int padscpt;
	struct device *dev = m4u_domain->piommuinfo->dev;

	if ((iova & (~F_PTE_PA_LARGE_MSK)) !=
	    ((unsigned int)pa & (~F_PTE_PA_LARGE_MSK))) {
		dev_err(dev, "error to mk_pte: iova=0x%x, pa=0x%pa, type=%s\n",
			iova, &pa, "large page");
		return -EINVAL;
	}

	iova &= F_PTE_PA_LARGE_MSK;
	if (pa > 0xffffffffL)
		padscpt = (unsigned int)pa &
					(F_PTE_PA_SMALL_MSK | F_PTE_BIT32_BIT);
	else
		padscpt = (unsigned int)pa & F_PTE_PA_LARGE_MSK;

	pgprot = __m4u_get_pgd_attr_page(prot);
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
		if (unlikely((imu_pgd_val(*pgd) & (~F_PGD_PA_PAGETABLE_MSK)) !=
		    pgprot)) {
			dev_err(dev, "%s: iova=0x%x, pgd=0x%x, pgprot=0x%x\n",
				__func__, iova, imu_pgd_val(*pgd), pgprot);
			return -1;
		}
		pte_new = 0;
	}

	pgprot = __m4u_get_pte_attr_64K(prot);
	pte = imu_pte_offset_map(pgd, iova);

	dev_dbg(dev, "%s:iova:0x%x,pte:0x%p(0x%p+0x%x),pa:0x%pa,value:0x%x\n",
		__func__, iova, &imu_pte_val(*pte), imu_pte_map(pgd),
		imu_pte_index(iova), &pa, padscpt | pgprot);

	for (i = 0; i < 16; i++) {
		if (unlikely(imu_pte_val(pte[i]))) {
			dev_err(dev, "%s: pte=0x%x, i=%d\n", __func__,
				imu_pte_val(pte[i]), i);
			goto err_out;
		}
		imu_pte_val(pte[i]) = padscpt | pgprot;
	}
	imu_pte_unmap(pte);

	return 0;

 err_out:
	for (i--; i >= 0; i--)
		imu_pte_val(pte[i]) = 0;
	imu_pte_unmap(pte);

	if (pte_new)
		m4u_free_pte(m4u_domain, pgd);
	return -1;
}

static int m4u_map_1M(struct mtk_iommu_domain *m4u_domain, unsigned int iova,
		      phys_addr_t pa, unsigned int prot)
{
	struct imu_pgd_t *pgd;
	unsigned int pgprot;
	unsigned int padscpt;
	struct device *dev = m4u_domain->piommuinfo->dev;

	if ((iova & (~F_PGD_PA_SECTION_MSK)) !=
	    ((unsigned int)pa & (~F_PGD_PA_SECTION_MSK))) {
		dev_err(dev, "error to mk_pte: iova=0x%x, pa=0x%pa, type=%s\n",
			iova, &pa, "section");
		return -EINVAL;
	}

	iova &= F_PGD_PA_SECTION_MSK;
	if (pa > 0xffffffffL)
		padscpt = (unsigned int)pa &
				(F_PTE_PA_SMALL_MSK | F_PGD_BIT32_BIT);
	else
		padscpt = (unsigned int)pa & F_PGD_PA_SECTION_MSK;

	pgprot = __m4u_get_pgd_attr_1M(prot);
	pgd = imu_pgd_offset(m4u_domain, iova);

	if (unlikely(imu_pgd_val(*pgd))) {
		dev_err(dev, "%s: iova=0x%x, pgd=0x%x\n", __func__,
			iova, imu_pgd_val(*pgd));
		return -1;
	}

	m4u_set_pgd_val(pgd, padscpt | pgprot);

	dev_dbg(dev, "%s:iova:0x%x,pgd:0x%p(0x%p+0x%x),pa:0x%pa,value:0x%x\n",
		__func__, iova, pgd, (m4u_domain)->pgd, imu_pgd_index(iova),
		&pa, padscpt | pgprot);

	return 0;
}

static int m4u_map_16M(struct mtk_iommu_domain *m4u_domain, unsigned int iova,
		       phys_addr_t pa, unsigned int prot)
{
	int i;
	struct imu_pgd_t *pgd;
	unsigned int pgprot;
	unsigned int padscpt;
	struct device *dev = m4u_domain->piommuinfo->dev;

	if ((iova & (~F_PGD_PA_SUPERSECTION_MSK)) !=
	    ((unsigned int)pa & (~F_PGD_PA_SUPERSECTION_MSK))) {
		dev_err(dev, "error to mk_pte: iova=0x%x, pa=0x%pa,type=%s\n",
			iova, &pa, "supersection");
		return -EINVAL;
	}

	iova &= F_PGD_PA_SUPERSECTION_MSK;
	if (pa > 0xffffffffL)
		padscpt = (unsigned int)pa &
				(F_PTE_PA_SMALL_MSK | F_PGD_BIT32_BIT);
	else
		padscpt = (unsigned int)pa & F_PGD_PA_SUPERSECTION_MSK;

	pgprot = __m4u_get_pgd_attr_16M(prot);
	pgd = imu_pgd_offset(m4u_domain, iova);

	dev_dbg(dev, "%s:iova:0x%x,pgd:0x%p(0x%p+0x%x),pa:0x%pa,value:0x%x\n",
		__func__, iova, pgd, (m4u_domain)->pgd, imu_pgd_index(iova),
		&pa, padscpt | pgprot);

	for (i = 0; i < 16; i++) {
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
	return -1;
}

int m4u_map(struct mtk_iommu_domain *m4u_domain, unsigned int iova,
	    phys_addr_t paddr, unsigned int size, unsigned int prot)
{
	int ret;

	if (size == SZ_4K) {/*most case*/
		ret = m4u_map_4K(m4u_domain, iova, paddr, prot);
	} else if (size == SZ_1M) {
		ret = m4u_map_1M(m4u_domain, iova, paddr, prot);
	} else if (size == SZ_64K) {
		ret = m4u_map_64K(m4u_domain, iova, paddr, prot);
	} else if (size == SZ_16M) {
		ret = m4u_map_16M(m4u_domain, iova, paddr, prot);
	} else  {
		dev_err(m4u_domain->piommuinfo->dev, "%s: fail size=0x%x\n",
			__func__, size);
		return -1;
	}

	m4u_clean_pte(m4u_domain, iova, size);

	return ret;
}

static int m4u_check_free_pte(struct mtk_iommu_domain *domain,
			      struct imu_pgd_t *pgd)
{
	struct imu_pte_t *pte;
	int i;

	pte = imu_pte_map(pgd);
	for (i = 0; i < IMU_PTRS_PER_PTE; i++, pte++) {
		if (imu_pte_val(*pte) != 0)
			break;
	}

	if (i == IMU_PTRS_PER_PTE) {
		m4u_free_pte(domain, pgd);
		m4u_set_pgd_val(pgd, 0);
		return 0;
	} else {
		return 1;
	}
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
				m4u_clean_pte(domain, iova,
					      num_to_clean << PAGE_SHIFT);

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
				(~(MMU_SUPERSECTION_SIZE - 1));	/* must align */
		} else {
			iova += MMU_SECTION_SIZE;
		}
	} while (iova < end_plus_1 && iova);

	return 0;
}

/* domain->pgtlock should be held */
int m4u_get_pte_info(const struct mtk_iommu_domain *domain, unsigned int iova,
		     struct m4u_pte_info_t *pte_info)
{
	struct imu_pgd_t *pgd;
	struct imu_pte_t *pte = NULL;
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

