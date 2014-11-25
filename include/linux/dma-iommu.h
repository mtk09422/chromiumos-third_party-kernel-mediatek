/*
 * Copyright (C) 2014 ARM Ltd.
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
#ifndef __DMA_IOMMU_H
#define __DMA_IOMMU_H

#ifdef __KERNEL__

#include <linux/dma-mapping.h>

#ifdef CONFIG_IOMMU_DMA

int iommu_dma_init(void);


struct iommu_dma_domain *iommu_dma_create_domain(const struct iommu_ops *ops,
		dma_addr_t base, size_t size);
void iommu_dma_release_domain(struct iommu_dma_domain *dma_domain);

struct iommu_domain *iommu_dma_raw_domain(struct iommu_dma_domain *dma_domain);

int iommu_dma_attach_device(struct device *dev, struct iommu_dma_domain *domain);
void iommu_dma_detach_device(struct device *dev);

/*
 * Implementation of these is left to arch code - it can associate domains
 * with devices however it likes, provided the lookup is efficient.
 */
static inline struct iommu_dma_domain *arch_get_dma_domain(struct device *dev);
static inline void arch_set_dma_domain(struct device *dev,
		struct iommu_dma_domain *dma_domain);


dma_addr_t iommu_dma_create_iova_mapping(struct device *dev,
		struct page **pages, size_t size, bool coherent);
int iommu_dma_release_iova_mapping(struct device *dev, dma_addr_t iova,
		size_t size);

struct page **iommu_dma_alloc_buffer(struct device *dev, size_t size,
		gfp_t gfp, struct dma_attrs *attrs,
		void (clear_buffer)(struct page *page, size_t size));
int iommu_dma_free_buffer(struct device *dev, struct page **pages, size_t size,
		struct dma_attrs *attrs);

dma_addr_t iommu_dma_map_page(struct device *dev, struct page *page,
		unsigned long offset, size_t size, enum dma_data_direction dir,
		struct dma_attrs *attrs);
dma_addr_t iommu_dma_coherent_map_page(struct device *dev, struct page *page,
		unsigned long offset, size_t size, enum dma_data_direction dir,
		struct dma_attrs *attrs);
void iommu_dma_unmap_page(struct device *dev, dma_addr_t handle, size_t size,
		enum dma_data_direction dir, struct dma_attrs *attrs);

int iommu_dma_map_sg(struct device *dev, struct scatterlist *sg, int nents,
		enum dma_data_direction dir, struct dma_attrs *attrs);
int iommu_dma_coherent_map_sg(struct device *dev, struct scatterlist *sg,
		int nents, enum dma_data_direction dir,
		struct dma_attrs *attrs);
void iommu_dma_unmap_sg(struct device *dev, struct scatterlist *sgl, int nents,
		enum dma_data_direction dir, struct dma_attrs *attrs);

int iommu_dma_supported(struct device *dev, u64 mask);
int iommu_dma_mapping_error(struct device *dev, dma_addr_t dma_addr);

static inline struct iommu_dma_domain *arch_get_dma_domain(struct device *dev)
{
	return NULL;
}

static inline void arch_set_dma_domain(struct device *dev,
		struct iommu_dma_domain *dma_domain)
{ }

#else

static inline int iommu_dma_init(void)
{
	return 0;
}

#endif  /* CONFIG_IOMMU_DMA */

#endif	/* __KERNEL__ */
#endif	/* __DMA_IOMMU_H */
