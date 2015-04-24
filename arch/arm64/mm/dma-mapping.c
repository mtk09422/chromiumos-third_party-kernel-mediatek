/*
 * SWIOTLB-based DMA API implementation
 *
 * Copyright (C) 2012 ARM Ltd.
 * Author: Catalin Marinas <catalin.marinas@arm.com>
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

#include <linux/gfp.h>
#include <linux/export.h>
#include <linux/slab.h>
#include <linux/genalloc.h>
#include <linux/dma-mapping.h>
#include <linux/dma-contiguous.h>
#include <linux/vmalloc.h>
#include <linux/swiotlb.h>

#include <asm/cacheflush.h>

struct dma_map_ops *dma_ops;
EXPORT_SYMBOL(dma_ops);

static pgprot_t __get_dma_pgprot(struct dma_attrs *attrs, pgprot_t prot,
				 bool coherent)
{
	if (!coherent || dma_get_attr(DMA_ATTR_WRITE_COMBINE, attrs))
		return pgprot_writecombine(prot);
	return prot;
}

static struct gen_pool *atomic_pool;

#define DEFAULT_DMA_COHERENT_POOL_SIZE  SZ_256K
static size_t atomic_pool_size = DEFAULT_DMA_COHERENT_POOL_SIZE;

static int __init early_coherent_pool(char *p)
{
	atomic_pool_size = memparse(p, &p);
	return 0;
}
early_param("coherent_pool", early_coherent_pool);

static void *__alloc_from_pool(size_t size, struct page **ret_page)
{
	unsigned long val;
	void *ptr = NULL;

	if (!atomic_pool) {
		WARN(1, "coherent pool not initialised!\n");
		return NULL;
	}

	val = gen_pool_alloc(atomic_pool, size);
	if (val) {
		phys_addr_t phys = gen_pool_virt_to_phys(atomic_pool, val);

		*ret_page = phys_to_page(phys);
		ptr = (void *)val;
	}

	return ptr;
}

static bool __in_atomic_pool(void *start, size_t size)
{
	return addr_in_gen_pool(atomic_pool, (unsigned long)start, size);
}

static int __free_from_pool(void *start, size_t size)
{
	if (!__in_atomic_pool(start, size))
		return 0;

	gen_pool_free(atomic_pool, (unsigned long)start, size);

	return 1;
}

static void *__dma_alloc_coherent(struct device *dev, size_t size,
				  dma_addr_t *dma_handle, gfp_t flags,
				  struct dma_attrs *attrs)
{
	if (dev == NULL) {
		WARN_ONCE(1, "Use an actual device structure for DMA allocation\n");
		return NULL;
	}

	if (IS_ENABLED(CONFIG_ZONE_DMA) &&
	    dev->coherent_dma_mask <= DMA_BIT_MASK(32))
		flags |= GFP_DMA;
	if (IS_ENABLED(CONFIG_DMA_CMA) && (flags & __GFP_WAIT)) {
		struct page *page;

		size = PAGE_ALIGN(size);
		page = dma_alloc_from_contiguous(dev, size >> PAGE_SHIFT,
							get_order(size));
		if (!page)
			return NULL;

		*dma_handle = phys_to_dma(dev, page_to_phys(page));
		return page_address(page);
	} else {
		return swiotlb_alloc_coherent(dev, size, dma_handle, flags);
	}
}

static void __dma_free_coherent(struct device *dev, size_t size,
				void *vaddr, dma_addr_t dma_handle,
				struct dma_attrs *attrs)
{
	bool freed;
	phys_addr_t paddr = dma_to_phys(dev, dma_handle);

	if (dev == NULL) {
		WARN_ONCE(1, "Use an actual device structure for DMA allocation\n");
		return;
	}

	freed = dma_release_from_contiguous(dev,
					phys_to_page(paddr),
					size >> PAGE_SHIFT);
	if (!freed)
		swiotlb_free_coherent(dev, size, vaddr, dma_handle);
}

static void *__dma_alloc(struct device *dev, size_t size,
			 dma_addr_t *dma_handle, gfp_t flags,
			 struct dma_attrs *attrs)
{
	struct page *page;
	void *ptr, *coherent_ptr;
	bool coherent = is_device_dma_coherent(dev);

	size = PAGE_ALIGN(size);

	if (!coherent && !(flags & __GFP_WAIT)) {
		struct page *page = NULL;
		void *addr = __alloc_from_pool(size, &page);

		if (addr)
			*dma_handle = phys_to_dma(dev, page_to_phys(page));

		return addr;
	}

	ptr = __dma_alloc_coherent(dev, size, dma_handle, flags, attrs);
	if (!ptr)
		goto no_mem;

	/* no need for non-cacheable mapping if coherent */
	if (coherent)
		return ptr;

	/* remove any dirty cache lines on the kernel alias */
	__dma_flush_range(ptr, ptr + size);

	/* create a coherent mapping */
	page = virt_to_page(ptr);
	coherent_ptr = dma_common_contiguous_remap(page, size, VM_USERMAP,
				__get_dma_pgprot(attrs,
					__pgprot(PROT_NORMAL_NC), false),
					NULL);
	if (!coherent_ptr)
		goto no_map;

	return coherent_ptr;

no_map:
	__dma_free_coherent(dev, size, ptr, *dma_handle, attrs);
no_mem:
	*dma_handle = DMA_ERROR_CODE;
	return NULL;
}

static void __dma_free(struct device *dev, size_t size,
		       void *vaddr, dma_addr_t dma_handle,
		       struct dma_attrs *attrs)
{
	void *swiotlb_addr = phys_to_virt(dma_to_phys(dev, dma_handle));

	if (!is_device_dma_coherent(dev)) {
		if (__free_from_pool(vaddr, size))
			return;
		vunmap(vaddr);
	}
	__dma_free_coherent(dev, size, swiotlb_addr, dma_handle, attrs);
}

static dma_addr_t __swiotlb_map_page(struct device *dev, struct page *page,
				     unsigned long offset, size_t size,
				     enum dma_data_direction dir,
				     struct dma_attrs *attrs)
{
	dma_addr_t dev_addr;

	dev_addr = swiotlb_map_page(dev, page, offset, size, dir, attrs);
	if (!is_device_dma_coherent(dev))
		__dma_map_area(phys_to_virt(dma_to_phys(dev, dev_addr)), size, dir);

	return dev_addr;
}


static void __swiotlb_unmap_page(struct device *dev, dma_addr_t dev_addr,
				 size_t size, enum dma_data_direction dir,
				 struct dma_attrs *attrs)
{
	if (!is_device_dma_coherent(dev))
		__dma_unmap_area(phys_to_virt(dma_to_phys(dev, dev_addr)), size, dir);
	swiotlb_unmap_page(dev, dev_addr, size, dir, attrs);
}

static int __swiotlb_map_sg_attrs(struct device *dev, struct scatterlist *sgl,
				  int nelems, enum dma_data_direction dir,
				  struct dma_attrs *attrs)
{
	struct scatterlist *sg;
	int i, ret;

	ret = swiotlb_map_sg_attrs(dev, sgl, nelems, dir, attrs);
	if (!is_device_dma_coherent(dev))
		for_each_sg(sgl, sg, ret, i)
			__dma_map_area(phys_to_virt(dma_to_phys(dev, sg->dma_address)),
				       sg->length, dir);

	return ret;
}

static void __swiotlb_unmap_sg_attrs(struct device *dev,
				     struct scatterlist *sgl, int nelems,
				     enum dma_data_direction dir,
				     struct dma_attrs *attrs)
{
	struct scatterlist *sg;
	int i;

	if (!is_device_dma_coherent(dev))
		for_each_sg(sgl, sg, nelems, i)
			__dma_unmap_area(phys_to_virt(dma_to_phys(dev, sg->dma_address)),
					 sg->length, dir);
	swiotlb_unmap_sg_attrs(dev, sgl, nelems, dir, attrs);
}

static void __swiotlb_sync_single_for_cpu(struct device *dev,
					  dma_addr_t dev_addr, size_t size,
					  enum dma_data_direction dir)
{
	if (!is_device_dma_coherent(dev))
		__dma_unmap_area(phys_to_virt(dma_to_phys(dev, dev_addr)), size, dir);
	swiotlb_sync_single_for_cpu(dev, dev_addr, size, dir);
}

static void __swiotlb_sync_single_for_device(struct device *dev,
					     dma_addr_t dev_addr, size_t size,
					     enum dma_data_direction dir)
{
	swiotlb_sync_single_for_device(dev, dev_addr, size, dir);
	if (!is_device_dma_coherent(dev))
		__dma_map_area(phys_to_virt(dma_to_phys(dev, dev_addr)), size, dir);
}

static void __swiotlb_sync_sg_for_cpu(struct device *dev,
				      struct scatterlist *sgl, int nelems,
				      enum dma_data_direction dir)
{
	struct scatterlist *sg;
	int i;

	if (!is_device_dma_coherent(dev))
		for_each_sg(sgl, sg, nelems, i)
			__dma_unmap_area(phys_to_virt(dma_to_phys(dev, sg->dma_address)),
					 sg->length, dir);
	swiotlb_sync_sg_for_cpu(dev, sgl, nelems, dir);
}

static void __swiotlb_sync_sg_for_device(struct device *dev,
					 struct scatterlist *sgl, int nelems,
					 enum dma_data_direction dir)
{
	struct scatterlist *sg;
	int i;

	swiotlb_sync_sg_for_device(dev, sgl, nelems, dir);
	if (!is_device_dma_coherent(dev))
		for_each_sg(sgl, sg, nelems, i)
			__dma_map_area(phys_to_virt(dma_to_phys(dev, sg->dma_address)),
				       sg->length, dir);
}

/* vma->vm_page_prot must be set appropriately before calling this function */
static int __dma_common_mmap(struct device *dev, struct vm_area_struct *vma,
			     void *cpu_addr, dma_addr_t dma_addr, size_t size)
{
	int ret = -ENXIO;
	unsigned long nr_vma_pages = (vma->vm_end - vma->vm_start) >>
					PAGE_SHIFT;
	unsigned long nr_pages = PAGE_ALIGN(size) >> PAGE_SHIFT;
	unsigned long pfn = dma_to_phys(dev, dma_addr) >> PAGE_SHIFT;
	unsigned long off = vma->vm_pgoff;

	if (dma_mmap_from_coherent(dev, vma, cpu_addr, size, &ret))
		return ret;

	if (off < nr_pages && nr_vma_pages <= (nr_pages - off)) {
		ret = remap_pfn_range(vma, vma->vm_start,
				      pfn + off,
				      vma->vm_end - vma->vm_start,
				      vma->vm_page_prot);
	}

	return ret;
}

static int __swiotlb_mmap(struct device *dev,
			  struct vm_area_struct *vma,
			  void *cpu_addr, dma_addr_t dma_addr, size_t size,
			  struct dma_attrs *attrs)
{
	vma->vm_page_prot = __get_dma_pgprot(attrs, vma->vm_page_prot,
					     is_device_dma_coherent(dev));
	return __dma_common_mmap(dev, vma, cpu_addr, dma_addr, size);
}

static struct dma_map_ops swiotlb_dma_ops = {
	.alloc = __dma_alloc,
	.free = __dma_free,
	.mmap = __swiotlb_mmap,
	.map_page = __swiotlb_map_page,
	.unmap_page = __swiotlb_unmap_page,
	.map_sg = __swiotlb_map_sg_attrs,
	.unmap_sg = __swiotlb_unmap_sg_attrs,
	.sync_single_for_cpu = __swiotlb_sync_single_for_cpu,
	.sync_single_for_device = __swiotlb_sync_single_for_device,
	.sync_sg_for_cpu = __swiotlb_sync_sg_for_cpu,
	.sync_sg_for_device = __swiotlb_sync_sg_for_device,
	.dma_supported = swiotlb_dma_supported,
	.mapping_error = swiotlb_dma_mapping_error,
};

extern int swiotlb_late_init_with_default_size(size_t default_size);

static int __init atomic_pool_init(void)
{
	pgprot_t prot = __pgprot(PROT_NORMAL_NC);
	unsigned long nr_pages = atomic_pool_size >> PAGE_SHIFT;
	struct page *page;
	void *addr;
	unsigned int pool_size_order = get_order(atomic_pool_size);

	if (dev_get_cma_area(NULL))
		page = dma_alloc_from_contiguous(NULL, nr_pages,
							pool_size_order);
	else
		page = alloc_pages(GFP_DMA, pool_size_order);

	if (page) {
		int ret;
		void *page_addr = page_address(page);

		memset(page_addr, 0, atomic_pool_size);
		__dma_flush_range(page_addr, page_addr + atomic_pool_size);

		atomic_pool = gen_pool_create(PAGE_SHIFT, -1);
		if (!atomic_pool)
			goto free_page;

		addr = dma_common_contiguous_remap(page, atomic_pool_size,
					VM_USERMAP, prot, atomic_pool_init);

		if (!addr)
			goto destroy_genpool;

		ret = gen_pool_add_virt(atomic_pool, (unsigned long)addr,
					page_to_phys(page),
					atomic_pool_size, -1);
		if (ret)
			goto remove_mapping;

		gen_pool_set_algo(atomic_pool,
				  gen_pool_first_fit_order_align,
				  (void *)PAGE_SHIFT);

		pr_info("DMA: preallocated %zu KiB pool for atomic allocations\n",
			atomic_pool_size / 1024);
		return 0;
	}
	goto out;

remove_mapping:
	dma_common_free_remap(addr, atomic_pool_size, VM_USERMAP);
destroy_genpool:
	gen_pool_destroy(atomic_pool);
	atomic_pool = NULL;
free_page:
	if (!dma_release_from_contiguous(NULL, page, nr_pages))
		__free_pages(page, pool_size_order);
out:
	pr_err("DMA: failed to allocate %zu KiB pool for atomic coherent allocation\n",
		atomic_pool_size / 1024);
	return -ENOMEM;
}

static int __init swiotlb_late_init(void)
{
	size_t swiotlb_size = min(SZ_64M, MAX_ORDER_NR_PAGES << PAGE_SHIFT);

	dma_ops = &swiotlb_dma_ops;

	return swiotlb_late_init_with_default_size(swiotlb_size);
}

static int __init arm64_dma_init(void)
{
	int ret = 0;

	ret |= swiotlb_late_init();
	ret |= atomic_pool_init();
	ret |= iommu_dma_init();

	return ret;
}
arch_initcall(arm64_dma_init);

#define PREALLOC_DMA_DEBUG_ENTRIES	4096

static int __init dma_debug_do_init(void)
{
	dma_debug_init(PREALLOC_DMA_DEBUG_ENTRIES);
	return 0;
}
fs_initcall(dma_debug_do_init);


#ifdef CONFIG_IOMMU_DMA
#include <linux/iommu.h>

static struct page **__atomic_get_pages(void *addr)
{
	struct page *page;
	phys_addr_t phys;

	phys = gen_pool_virt_to_phys(atomic_pool, (unsigned long)addr);
	page = phys_to_page(phys);

	return (struct page **)page;
}

static struct page **__iommu_get_pages(void *cpu_addr, struct dma_attrs *attrs)
{
	struct vm_struct *area;

	if (__in_atomic_pool(cpu_addr, PAGE_SIZE))
		return __atomic_get_pages(cpu_addr);

	area = find_vm_area(cpu_addr);
	if (!area)
		return NULL;

	return area->pages;
}

static void *__iommu_alloc_atomic(struct device *dev, size_t size,
				  dma_addr_t *handle, bool coherent)
{
	struct page *page;
	void *addr;

	addr = __alloc_from_pool(size, &page);
	if (!addr)
		return NULL;

	*handle = iommu_dma_create_iova_mapping(dev, &page, size, coherent);
	if (*handle == DMA_ERROR_CODE) {
		__free_from_pool(addr, size);
		return NULL;
	}
	return addr;
}

static void __iommu_free_atomic(struct device *dev, void *cpu_addr,
				dma_addr_t handle, size_t size)
{
	iommu_dma_release_iova_mapping(dev, handle, size);
	__free_from_pool(cpu_addr, size);
}

static void __dma_clear_buffer(struct page *page, size_t size)
{
	void *ptr = page_address(page);

	memset(ptr, 0, size);
	__dma_flush_range(ptr, ptr + size);
}

static void *__iommu_alloc_attrs(struct device *dev, size_t size,
				 dma_addr_t *handle, gfp_t gfp,
				 struct dma_attrs *attrs)
{
	bool coherent = is_device_dma_coherent(dev);
	pgprot_t prot = coherent ? __pgprot(PROT_NORMAL) :
				   __pgprot(PROT_NORMAL_NC);
	struct page **pages;
	void *addr = NULL;

	*handle = DMA_ERROR_CODE;
	size = PAGE_ALIGN(size);

	if (!(gfp & __GFP_WAIT))
		return __iommu_alloc_atomic(dev, size, handle, coherent);
	/*
	 * FIXME: This isn't even true any more!
	 *
	 * Following is a work-around (a.k.a. hack) to prevent pages
	 * with __GFP_COMP being passed to split_page() which cannot
	 * handle them.  The real problem is that this flag probably
	 * should be 0 on ARM as it is not supported on this
	 * platform; see CONFIG_HUGETLBFS.
	 */
	gfp &= ~(__GFP_COMP);

	pages = iommu_dma_alloc_buffer(dev, size, gfp, attrs,
			__dma_clear_buffer);
	if (!pages)
		return NULL;

	*handle = iommu_dma_create_iova_mapping(dev, pages, size, coherent);
	if (*handle == DMA_ERROR_CODE)
		goto err_mapping;

	addr = dma_common_pages_remap(pages, size, VM_USERMAP,
				      __get_dma_pgprot(attrs, prot, coherent),
				      __builtin_return_address(0));
	if (addr)
		return addr;

	iommu_dma_release_iova_mapping(dev, *handle, size);
err_mapping:
	iommu_dma_free_buffer(dev, pages, size, attrs);
	return NULL;
}

static void __iommu_free_attrs(struct device *dev, size_t size, void *cpu_addr,
			       dma_addr_t handle, struct dma_attrs *attrs)
{
	struct page **pages;

	size = PAGE_ALIGN(size);
	if (__in_atomic_pool(cpu_addr, size)) {
		__iommu_free_atomic(dev, cpu_addr, handle, size);
		return;
	}

	pages = __iommu_get_pages(cpu_addr, attrs);
	if (!pages) {
		WARN(1, "trying to free invalid coherent area: %p\n", cpu_addr);
		return;
	}

	dma_common_free_remap(cpu_addr, size, VM_USERMAP);

	iommu_dma_release_iova_mapping(dev, handle, size);
	iommu_dma_free_buffer(dev, pages, size, attrs);
}

static int __iommu_get_sgtable(struct device *dev, struct sg_table *sgt,
				 void *cpu_addr, dma_addr_t dma_addr,
				 size_t size, struct dma_attrs *attrs)
{
	unsigned int count = PAGE_ALIGN(size) >> PAGE_SHIFT;
	struct page **pages = __iommu_get_pages(cpu_addr, attrs);

	if (!pages)
		return -ENXIO;

	return sg_alloc_table_from_pages(sgt, pages, count, 0, size,
					 GFP_KERNEL);
}

static int __iommu_mmap_attrs(struct device *dev, struct vm_area_struct *vma,
			      void *cpu_addr, dma_addr_t dma_addr, size_t size,
			      struct dma_attrs *attrs)
{
	unsigned long uaddr = vma->vm_start;
	unsigned long usize = vma->vm_end - vma->vm_start;
	struct page **pages = __iommu_get_pages(cpu_addr, attrs);
	int ret;

	if (!pages)
		return -ENXIO;

	vma->vm_page_prot = __get_dma_pgprot(attrs, vma->vm_page_prot,
					     is_device_dma_coherent(dev));

	if (dma_mmap_from_coherent(dev, vma, cpu_addr, size, &ret))
		return ret;

	do {
		ret = vm_insert_page(vma, uaddr, *pages++);
		if (ret) {
			pr_err("Remapping memory failed: %d\n", ret);
			return ret;
		}
		uaddr += PAGE_SIZE;
		usize -= PAGE_SIZE;
	} while (usize > 0);

	return 0;
}

static void __iommu_sync_single_for_cpu(struct device *dev,
					dma_addr_t dev_addr, size_t size,
					enum dma_data_direction dir)
{
	if (!is_device_dma_coherent(dev) && (dev_addr != DMA_ERROR_CODE)) {
		struct iommu_dma_domain *dma_domain = arch_get_dma_domain(dev);
		struct iommu_domain *domain = iommu_dma_raw_domain(dma_domain);
		phys_addr_t phys = iommu_iova_to_phys(domain, dev_addr);

		__dma_unmap_area(phys_to_virt(phys), size, dir);
	}
}

static void __iommu_sync_single_for_device(struct device *dev,
					   dma_addr_t dev_addr, size_t size,
					   enum dma_data_direction dir)
{
	if (!is_device_dma_coherent(dev) && (dev_addr != DMA_ERROR_CODE)) {
		struct iommu_dma_domain *dma_domain = arch_get_dma_domain(dev);
		struct iommu_domain *domain = iommu_dma_raw_domain(dma_domain);
		phys_addr_t phys = iommu_iova_to_phys(domain, dev_addr);

		__dma_map_area(phys_to_virt(phys), size, dir);
	}
}

static dma_addr_t __iommu_map_page(struct device *dev, struct page *page,
				   unsigned long offset, size_t size,
				   enum dma_data_direction dir,
				   struct dma_attrs *attrs)
{
	dma_addr_t dev_addr;

	if (is_device_dma_coherent(dev))
		return iommu_dma_coherent_map_page(dev, page, offset, size,
				dir, attrs);

	dev_addr = iommu_dma_map_page(dev, page, offset, size, dir, attrs);

	if (!dma_get_attr(DMA_ATTR_SKIP_CPU_SYNC, attrs))
		__iommu_sync_single_for_device(dev, dev_addr, size, dir);

	return dev_addr;
}

static void __iommu_unmap_page(struct device *dev, dma_addr_t dev_addr,
			       size_t size, enum dma_data_direction dir,
			       struct dma_attrs *attrs)
{
	if (!dma_get_attr(DMA_ATTR_SKIP_CPU_SYNC, attrs))
		__iommu_sync_single_for_cpu(dev, dev_addr, size, dir);

	iommu_dma_unmap_page(dev, dev_addr, size, dir, attrs);
}

static void __iommu_sync_sg_for_cpu(struct device *dev,
				    struct scatterlist *sgl, int nelems,
				    enum dma_data_direction dir)
{
	struct scatterlist *sg;
	int i;

	if (is_device_dma_coherent(dev))
		return;

	for_each_sg(sgl, sg, nelems, i)
		__dma_unmap_area(sg_virt(sg), sg->length, dir);
}

static void __iommu_sync_sg_for_device(struct device *dev,
				       struct scatterlist *sgl, int nelems,
				       enum dma_data_direction dir)
{
	struct scatterlist *sg;
	int i;

	if (is_device_dma_coherent(dev))
		return;

	for_each_sg(sgl, sg, nelems, i)
		__dma_map_area(sg_virt(sg), sg->length, dir);
}

static int __iommu_map_sg_attrs(struct device *dev, struct scatterlist *sgl,
				int nelems, enum dma_data_direction dir,
				struct dma_attrs *attrs)
{
	if (is_device_dma_coherent(dev))
		return iommu_dma_coherent_map_sg(dev, sgl, nelems, dir, attrs);

	if (!dma_get_attr(DMA_ATTR_SKIP_CPU_SYNC, attrs))
		__iommu_sync_sg_for_device(dev, sgl, nelems, dir);

	return iommu_dma_map_sg(dev, sgl, nelems, dir, attrs);
}

static void __iommu_unmap_sg_attrs(struct device *dev,
				   struct scatterlist *sgl, int nelems,
				   enum dma_data_direction dir,
				   struct dma_attrs *attrs)
{
	if (!dma_get_attr(DMA_ATTR_SKIP_CPU_SYNC, attrs))
		__iommu_sync_sg_for_cpu(dev, sgl, nelems, dir);

	iommu_dma_unmap_sg(dev, sgl, nelems, dir, attrs);
}

static struct dma_map_ops iommu_dma_ops = {
	.alloc = __iommu_alloc_attrs,
	.free = __iommu_free_attrs,
	.mmap = __iommu_mmap_attrs,
	.get_sgtable = __iommu_get_sgtable,
	.map_page = __iommu_map_page,
	.unmap_page = __iommu_unmap_page,
	.map_sg = __iommu_map_sg_attrs,
	.unmap_sg = __iommu_unmap_sg_attrs,
	.sync_single_for_cpu = __iommu_sync_single_for_cpu,
	.sync_single_for_device = __iommu_sync_single_for_device,
	.sync_sg_for_cpu = __iommu_sync_sg_for_cpu,
	.sync_sg_for_device = __iommu_sync_sg_for_device,
	.dma_supported = iommu_dma_supported,
	.mapping_error = iommu_dma_mapping_error,
};

static void __iommu_setup_dma_ops(struct device *dev, u64 dma_base, u64 size,
				  const struct iommu_ops *ops)
{
	struct iommu_dma_domain *dma_domain;

	if (!ops)
		return;

	dma_domain = iommu_dma_create_domain(ops, dma_base, size);
	if (!dma_domain) {
		pr_warn("Failed to create %llu-byte IOMMU mapping for device %s\n",
				size, dev_name(dev));
		return;
	}

	if (iommu_dma_attach_device(dev, dma_domain))
		pr_warn("Failed to attach device %s to IOMMU mapping\n",
				dev_name(dev));
	else
		dev->archdata.dma_ops = &iommu_dma_ops;

	/* drop the initial mapping refcount */
	iommu_dma_release_domain(dma_domain);
}

void arch_teardown_dma_ops(struct device *dev)
{
	if (dev->archdata.dma_domain) {
		iommu_dma_detach_device(dev);
		dev->archdata.dma_ops = NULL;
	}
}

#else

static void __iommu_setup_dma_ops(struct device *dev, u64 dma_base, u64 size,
				  struct iommu_ops *iommu)
{ }

#endif  /* CONFIG_IOMMU_DMA */

void arch_setup_dma_ops(struct device *dev, u64 dma_base, u64 size,
			struct iommu_ops *iommu, bool coherent)
{
	dev->archdata.dma_coherent = coherent;
	__iommu_setup_dma_ops(dev, dma_base, size, iommu);
}
