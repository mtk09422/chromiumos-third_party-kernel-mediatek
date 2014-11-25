/*
 * A fairly generic DMA-API to IOMMU-API glue layer.
 *
 * Copyright (C) 2014 ARM Ltd.
 *
 * based in part on arch/arm/mm/dma-mapping.c:
 * Copyright (C) 2000-2004 Russell King
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#define pr_fmt(fmt)	"%s: " fmt, __func__

#include <linux/dma-contiguous.h>
#include <linux/dma-iommu.h>
#include <linux/iommu.h>
#include <linux/iova.h>

int iommu_dma_init(void)
{
	return iommu_iova_cache_init();
}

struct iommu_dma_domain {
	struct iommu_domain *domain;
	struct iova_domain *iovad;
	struct kref kref;
};

static inline dma_addr_t dev_dma_addr(struct device *dev, dma_addr_t addr)
{
	dma_addr_t offset = (dma_addr_t)dev->dma_pfn_offset << PAGE_SHIFT;

	BUG_ON(addr < offset);
	return addr - offset;
}

static int __dma_direction_to_prot(enum dma_data_direction dir, bool coherent)
{
	int prot = coherent ? IOMMU_CACHE : 0;

	switch (dir) {
	case DMA_BIDIRECTIONAL:
		return prot | IOMMU_READ | IOMMU_WRITE;
	case DMA_TO_DEVICE:
		return prot | IOMMU_READ;
	case DMA_FROM_DEVICE:
		return prot | IOMMU_WRITE;
	default:
		return 0;
	}
}

static struct iova *__alloc_iova(struct device *dev, size_t size, bool coherent)
{
	struct iommu_dma_domain *dom = arch_get_dma_domain(dev);
	struct iova_domain *iovad = dom->iovad;
	unsigned long shift = iova_shift(iovad);
	unsigned long length = iova_align(iovad, size) >> shift;
	unsigned long limit_pfn = iovad->dma_32bit_pfn;
	u64 dma_limit = coherent ? dev->coherent_dma_mask : dma_get_mask(dev);

	limit_pfn = min_t(unsigned long, limit_pfn, dma_limit >> shift);
	/* Alignment should probably come from a domain/device attribute... */
	return alloc_iova(iovad, length, limit_pfn, false);
}

/*
 * Create a mapping in device IO address space for specified pages
 */
dma_addr_t iommu_dma_create_iova_mapping(struct device *dev,
		struct page **pages, size_t size, bool coherent)
{
	struct iommu_dma_domain *dom = arch_get_dma_domain(dev);
	struct iova_domain *iovad = dom->iovad;
	struct iommu_domain *domain = dom->domain;
	struct iova *iova;
	unsigned int count = PAGE_ALIGN(size) >> PAGE_SHIFT;
	dma_addr_t addr_lo, addr_hi;
	int i, prot = __dma_direction_to_prot(DMA_BIDIRECTIONAL, coherent);

	iova = __alloc_iova(dev, size, coherent);
	if (!iova)
		return DMA_ERROR_CODE;

	addr_hi = addr_lo = iova_dma_addr(iovad, iova);
	for (i = 0; i < count; ) {
		unsigned int next_pfn = page_to_pfn(pages[i]) + 1;
		phys_addr_t phys = page_to_phys(pages[i]);
		unsigned int len, j;

		for (j = i+1; j < count; j++, next_pfn++)
			if (page_to_pfn(pages[j]) != next_pfn)
				break;

		len = (j - i) << PAGE_SHIFT;
		if (iommu_map(domain, addr_hi, phys, len, prot))
			goto fail;
		addr_hi += len;
		i = j;
	}
	return dev_dma_addr(dev, addr_lo);
fail:
	iommu_unmap(domain, addr_lo, addr_hi - addr_lo);
	__free_iova(iovad, iova);
	return DMA_ERROR_CODE;
}

int iommu_dma_release_iova_mapping(struct device *dev, dma_addr_t iova,
		size_t size)
{
	struct iommu_dma_domain *dom = arch_get_dma_domain(dev);
	struct iova_domain *iovad = dom->iovad;
	size_t offset = iova_offset(iovad, iova);
	size_t len = iova_align(iovad, size + offset);

	iommu_unmap(dom->domain, iova - offset, len);
	free_iova(iovad, iova_pfn(iovad, iova));
	return 0;
}

struct page **iommu_dma_alloc_buffer(struct device *dev, size_t size,
		gfp_t gfp, struct dma_attrs *attrs,
		void (clear_buffer)(struct page *page, size_t size))
{
	struct page **pages;
	int count = size >> PAGE_SHIFT;
	int array_size = count * sizeof(struct page *);
	int i = 0;

	if (array_size <= PAGE_SIZE)
		pages = kzalloc(array_size, GFP_KERNEL);
	else
		pages = vzalloc(array_size);
	if (!pages)
		return NULL;

	if (dma_get_attr(DMA_ATTR_FORCE_CONTIGUOUS, attrs)) {
		unsigned long order = get_order(size);
		struct page *page;

		page = dma_alloc_from_contiguous(dev, count, order);
		if (!page)
			goto error;

		if (clear_buffer)
			clear_buffer(page, size);

		for (i = 0; i < count; i++)
			pages[i] = page + i;

		return pages;
	}

	/*
	 * IOMMU can map any pages, so himem can also be used here
	 */
	gfp |= __GFP_NOWARN | __GFP_HIGHMEM;

	while (count) {
		int j, order = __fls(count);

		pages[i] = alloc_pages(gfp, order);
		while (!pages[i] && order)
			pages[i] = alloc_pages(gfp, --order);
		if (!pages[i])
			goto error;

		if (order) {
			split_page(pages[i], order);
			j = 1 << order;
			while (--j)
				pages[i + j] = pages[i] + j;
		}

		if (clear_buffer)
			clear_buffer(pages[i], PAGE_SIZE << order);
		i += 1 << order;
		count -= 1 << order;
	}

	return pages;
error:
	while (i--)
		if (pages[i])
			__free_pages(pages[i], 0);
	if (array_size <= PAGE_SIZE)
		kfree(pages);
	else
		vfree(pages);
	return NULL;
}

int iommu_dma_free_buffer(struct device *dev, struct page **pages, size_t size,
		struct dma_attrs *attrs)
{
	int count = size >> PAGE_SHIFT;
	int array_size = count * sizeof(struct page *);
	int i;

	if (dma_get_attr(DMA_ATTR_FORCE_CONTIGUOUS, attrs)) {
		dma_release_from_contiguous(dev, pages[0], count);
	} else {
		for (i = 0; i < count; i++)
			if (pages[i])
				__free_pages(pages[i], 0);
	}

	if (array_size <= PAGE_SIZE)
		kfree(pages);
	else
		vfree(pages);
	return 0;
}

static dma_addr_t __iommu_dma_map_page(struct device *dev, struct page *page,
		unsigned long offset, size_t size, enum dma_data_direction dir,
		bool coherent)
{
	dma_addr_t dma_addr;
	struct iommu_dma_domain *dom = arch_get_dma_domain(dev);
	struct iova_domain *iovad = dom->iovad;
	phys_addr_t phys = page_to_phys(page) + offset;
	size_t iova_off = iova_offset(iovad, phys);
	size_t len = iova_align(iovad, size + iova_off);
	int prot = __dma_direction_to_prot(dir, coherent);
	struct iova *iova = __alloc_iova(dev, len, coherent);

	if (!iova)
		return DMA_ERROR_CODE;

	dma_addr = iova_dma_addr(iovad, iova);
	if (iommu_map(dom->domain, dma_addr, phys - iova_off, len, prot)) {
		__free_iova(iovad, iova);
		return DMA_ERROR_CODE;
	}

	return dev_dma_addr(dev, dma_addr + iova_off);
}

dma_addr_t iommu_dma_map_page(struct device *dev, struct page *page,
		unsigned long offset, size_t size, enum dma_data_direction dir,
		struct dma_attrs *attrs)
{
	return __iommu_dma_map_page(dev, page, offset, size, dir, false);
}

dma_addr_t iommu_dma_coherent_map_page(struct device *dev, struct page *page,
		unsigned long offset, size_t size, enum dma_data_direction dir,
		struct dma_attrs *attrs)
{
	return __iommu_dma_map_page(dev, page, offset, size, dir, true);
}

void iommu_dma_unmap_page(struct device *dev, dma_addr_t handle, size_t size,
		enum dma_data_direction dir, struct dma_attrs *attrs)
{
	struct iommu_dma_domain *dom = arch_get_dma_domain(dev);
	struct iova_domain *iovad = dom->iovad;
	size_t offset = iova_offset(iovad, handle);
	size_t len = iova_align(iovad, size + offset);
	dma_addr_t iova = handle - offset;

	if (!iova)
		return;

	iommu_unmap(dom->domain, iova, len);
	free_iova(iovad, iova_pfn(iovad, iova));
}

static int finalise_sg(struct device *dev, struct scatterlist *sg, int nents,
		dma_addr_t dma_addr)
{
	struct scatterlist *s, *seg = sg;
	unsigned long seg_mask = dma_get_seg_boundary(dev);
	unsigned int max_len = dma_get_max_seg_size(dev);
	unsigned int seg_len = 0, seg_dma = 0;
	int i, count = 1;

	for_each_sg(sg, s, nents, i) {
		/* Un-swizzling the fields here, hence the naming mismatch */
		unsigned int s_offset = sg_dma_address(s);
		unsigned int s_length = sg_dma_len(s);
		unsigned int s_dma_len = s->length;

		s->offset = s_offset;
		s->length = s_length;
		sg_dma_address(s) = DMA_ERROR_CODE;
		sg_dma_len(s) = 0;

		if (seg_len && (seg_dma + seg_len == dma_addr + s_offset) &&
		    (seg_len + s_dma_len <= max_len) &&
		    ((seg_dma & seg_mask) <= seg_mask - (seg_len + s_length))
		   ) {
			sg_dma_len(seg) += s_dma_len;
		} else {
			if (seg_len) {
				seg = sg_next(seg);
				count++;
			}
			sg_dma_len(seg) = s_dma_len;
			sg_dma_address(seg) = dma_addr + s_offset;

			seg_len = s_offset;
			seg_dma = dma_addr + s_offset;
		}
		seg_len += s_length;
		dma_addr += s_dma_len;
	}
	return count;
}

static void invalidate_sg(struct scatterlist *sg, int nents)
{
	struct scatterlist *s;
	int i;

	for_each_sg(sg, s, nents, i) {
		if (sg_dma_address(s) != DMA_ERROR_CODE)
			s->offset = sg_dma_address(s);
		if (sg_dma_len(s))
			s->length = sg_dma_len(s);
		sg_dma_address(s) = DMA_ERROR_CODE;
		sg_dma_len(s) = 0;
	}
}

static int __iommu_dma_map_sg(struct device *dev, struct scatterlist *sg,
		int nents, enum dma_data_direction dir, struct dma_attrs *attrs,
		bool coherent)
{
	struct iommu_dma_domain *dom = arch_get_dma_domain(dev);
	struct iova_domain *iovad = dom->iovad;
	struct iova *iova;
	struct scatterlist *s;
	dma_addr_t dma_addr;
	size_t iova_len = 0;
	int i, prot = __dma_direction_to_prot(dir, coherent);

	/*
	 * Work out how much IOVA space we need, and align the segments to
	 * IOVA granules for the IOMMU driver to handle. With some clever
	 * trickery we can modify the list in a reversible manner.
	 */
	for_each_sg(sg, s, nents, i) {
		size_t s_offset = iova_offset(iovad, s->offset);
		size_t s_length = s->length;

		sg_dma_address(s) = s->offset;
		sg_dma_len(s) = s_length;
		s->offset -= s_offset;
		s_length = iova_align(iovad, s_length + s_offset);
		s->length = s_length;

		iova_len += s_length;
	}

	iova = __alloc_iova(dev, iova_len, coherent);
	if (!iova)
		goto out_restore_sg;

	/*
	 * We'll leave any physical concatenation to the IOMMU driver's
	 * implementation - it knows better than we do.
	 */
	dma_addr = iova_dma_addr(iovad, iova);
	if (iommu_map_sg(dom->domain, dma_addr, sg, nents, prot) < iova_len)
		goto out_free_iova;

	return finalise_sg(dev, sg, nents, dev_dma_addr(dev, dma_addr));

out_free_iova:
	__free_iova(iovad, iova);
out_restore_sg:
	invalidate_sg(sg, nents);
	return 0;
}

int iommu_dma_map_sg(struct device *dev, struct scatterlist *sg, int nents,
		enum dma_data_direction dir, struct dma_attrs *attrs)
{
	return __iommu_dma_map_sg(dev, sg, nents, dir, attrs, false);
}

int iommu_dma_coherent_map_sg(struct device *dev, struct scatterlist *sg,
		int nents, enum dma_data_direction dir, struct dma_attrs *attrs)
{
	return __iommu_dma_map_sg(dev, sg, nents, dir, attrs, true);
}

void iommu_dma_unmap_sg(struct device *dev, struct scatterlist *sg, int nents,
		enum dma_data_direction dir, struct dma_attrs *attrs)
{
	struct iommu_dma_domain *dom = arch_get_dma_domain(dev);
	struct iova_domain *iovad = dom->iovad;
	struct scatterlist *s;
	int i;
	dma_addr_t iova = sg_dma_address(sg) & ~iova_mask(iovad);
	size_t len = 0;

	/*
	 * The scatterlist segments are mapped into contiguous IOVA space,
	 * so just add up the total length and unmap it in one go.
	 */
	for_each_sg(sg, s, nents, i)
		len += sg_dma_len(s);

	iommu_unmap(dom->domain, iova, len);
	free_iova(iovad, iova_pfn(iovad, iova));
}

struct iommu_dma_domain *iommu_dma_create_domain(const struct iommu_ops *ops,
		dma_addr_t base, size_t size)
{
	struct iommu_dma_domain *dom;
	struct iommu_domain *domain;
	struct iova_domain *iovad;
	struct iommu_domain_geometry *dg;
	unsigned long order, base_pfn, end_pfn;

	dom = kzalloc(sizeof(*dom), GFP_KERNEL);
	if (!dom)
		return NULL;

	/*
	 * HACK: We'd like to ask the relevant IOMMU in ops for a suitable
	 * domain, but until that happens, bypass the bus nonsense and create
	 * one directly for this specific device/IOMMU combination...
	 */
	domain = kzalloc(sizeof(*domain), GFP_KERNEL);

	if (!domain)
		goto out_free_dma_domain;
	domain->ops = ops;

	if (ops->domain_init(domain))
		goto out_free_iommu_domain;
	/*
	 * ...and do the bare minimum to sanity-check that the domain allows
	 * at least some access to the device...
	 */
	dg = &domain->geometry;
	if (!(base <= dg->aperture_end && base + size > dg->aperture_start)) {
		pr_warn("specified DMA range outside IOMMU capability\n");
		goto out_free_iommu_domain;
	}
	/* ...then finally give it a kicking to make sure it fits */
	dg->aperture_start = max(base, dg->aperture_start);
	dg->aperture_end = min(base + size - 1, dg->aperture_end);
	/*
	 * Note that this almost certainly breaks the case where multiple
	 * devices with different DMA capabilities need to share a domain,
	 * but we don't have the necessary information to handle that here
	 * anyway - "proper" group and domain allocation needs to involve
	 * the IOMMU driver and a complete view of the bus.
	 */

	iovad = kzalloc(sizeof(*iovad), GFP_KERNEL);
	if (!iovad)
		goto out_free_iommu_domain;

	/* Use the smallest supported page size for IOVA granularity */
	order = __ffs(ops->pgsize_bitmap);
	base_pfn = max(dg->aperture_start >> order, (dma_addr_t)1);
	end_pfn = dg->aperture_end >> order;
	init_iova_domain(iovad, 1UL << order, base_pfn, end_pfn);

	dom->domain = domain;
	dom->iovad = iovad;
	kref_init(&dom->kref);
	return dom;

out_free_iommu_domain:
	kfree(domain);
out_free_dma_domain:
	kfree(dom);
	return NULL;
}

static void iommu_dma_free_domain(struct kref *kref)
{
	struct iommu_dma_domain *dom;

	dom = container_of(kref, struct iommu_dma_domain, kref);
	put_iova_domain(dom->iovad);
	iommu_domain_free(dom->domain);
	kfree(dom);
}

void iommu_dma_release_domain(struct iommu_dma_domain *dom)
{
	kref_put(&dom->kref, iommu_dma_free_domain);
}

struct iommu_domain *iommu_dma_raw_domain(struct iommu_dma_domain *dom)
{
	return dom ? dom->domain : NULL;
}

int iommu_dma_attach_device(struct device *dev, struct iommu_dma_domain *dom)
{
	int ret;

	kref_get(&dom->kref);
	ret = iommu_attach_device(dom->domain, dev);
	if (ret)
		iommu_dma_release_domain(dom);
	else
		arch_set_dma_domain(dev, dom);
	return ret;
}

void iommu_dma_detach_device(struct device *dev)
{
	struct iommu_dma_domain *dom = arch_get_dma_domain(dev);

	arch_set_dma_domain(dev, NULL);
	iommu_detach_device(dom->domain, dev);
	iommu_dma_release_domain(dom);
}

int iommu_dma_supported(struct device *dev, u64 mask)
{
	/*
	 * 'Special' IOMMUs which don't have the same addressing capability
	 * as the CPU will have to wait until we have some way to query that
	 * before they'll be able to use this framework.
	 */
	return 1;
}

int iommu_dma_mapping_error(struct device *dev, dma_addr_t dma_addr)
{
	return dma_addr == DMA_ERROR_CODE;
}
