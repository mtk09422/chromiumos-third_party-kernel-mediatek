/*
 * Copyright (c) 2014 MediaTek Inc.
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

#include <drm/drmP.h>
#include <drm/drm_gem.h>

#include "mediatek_drm_drv.h"
#include "mediatek_drm_gem.h"

struct mtk_drm_gem_obj *mtk_drm_gem_create(struct drm_device *dev,
				unsigned int flags,	unsigned long size)
{
	struct mtk_drm_gem_obj *mtk_gem;
	struct mtk_drm_gem_buf *mtk_buf;
	int ret;

	if (!size) {
		DRM_ERROR("invalid size.\n");
		ret = -EINVAL;
		goto err;
	}

	mtk_gem = kzalloc(sizeof(*mtk_gem), GFP_KERNEL);
	if (!mtk_gem) {
		ret = -ENOMEM;
		goto err;
	}

	mtk_buf = kzalloc(sizeof(*mtk_buf), GFP_KERNEL);
	if (!mtk_buf) {
		ret = -ENOMEM;
		goto err_buf;
	}
	mtk_gem->buffer = mtk_buf;

#ifndef CONFIG_MTK_IOMMU
	if (flags) {
		dev_err(dev->dev, "iommu is not supported!! %d\n", flags);
		flags = 0;
	}
#endif

	if (flags == 0) {
		size = round_up(size, PAGE_SIZE);
		mtk_buf->kvaddr = kzalloc(size, GFP_KERNEL);
		if (!mtk_buf->kvaddr) {
			ret = -ENOMEM;
			goto err_size;
		}

		mtk_buf->paddr = virt_to_phys(mtk_buf->kvaddr);
		mtk_buf->size = size;
		mtk_buf->mva_addr = mtk_buf->paddr;
	}
#ifdef CONFIG_MTK_IOMMU
	else if (flags == 2) {
		struct page **pages;
		struct sg_table *sgt;
		struct scatterlist *sgl;
		int i, npages;

		size = round_up(size, PAGE_SIZE);
		mtk_buf->kvaddr = kzalloc(size, GFP_KERNEL);
		if (!mtk_buf->kvaddr) {
			ret = -ENOMEM;
			goto err_NEW;
		}

		npages = size >> PAGE_SHIFT;
		pages = kmalloc(npages * sizeof(*pages), GFP_KERNEL);
		if (!pages) {
			ret = -ENOMEM;
			goto err_NEW;
		}
		mtk_buf->pages = pages;

		sgt = kzalloc(sizeof(*sgt), GFP_KERNEL);
		if (!sgt) {
			ret = -ENOMEM;
			goto err_NEW;
		}
		mtk_buf->sgt = sgt;

		ret = sg_alloc_table(sgt, npages, GFP_KERNEL);
		if (ret)
			goto err_NEW;

		for (i = 0; i < npages; i++)
			pages[i] = virt_to_page(mtk_buf->kvaddr + i*PAGE_SIZE);

		for_each_sg(sgt->sgl, sgl, npages, i) {
			sg_set_page(sgl, pages[i], PAGE_SIZE, 0);
		}

		if (dma_map_sg(dev->dev, sgt->sgl, sgt->nents,
				DMA_BIDIRECTIONAL) == 0) {
			DRM_INFO("DBG_YT dma_map_sg failed\n");
			goto err_NEW;
		}

		mtk_buf->paddr = virt_to_phys(mtk_buf->kvaddr);
		mtk_buf->size = size;
		mtk_buf->mva_addr = sg_dma_address(sgt->sgl);
	} else if (flags == 1) { /* use iommu to */
		struct page **pages;
		struct sg_table *sgt;
		struct scatterlist *sgl;
		int i, npages;

		size = PAGE_ALIGN(size) + PAGE_SIZE*32;
		npages = size >> PAGE_SHIFT;
		pages = kmalloc(npages * sizeof(*pages), GFP_KERNEL);
		if (!pages) {
			ret = -ENOMEM;
			goto err_NEW;
		}
		mtk_buf->pages = pages;

		for (i = 0; i < npages; i++) {
			pages[i] = alloc_page(GFP_KERNEL);
			if (IS_ERR(pages[i])) {
				ret = PTR_ERR(pages[i]);
				goto err_NEW;
			}
		}

		mtk_buf->kvaddr = vmap(pages, npages, 0, PAGE_KERNEL);
		if (!mtk_buf->kvaddr) {
			DRM_DEBUG_KMS("fail vmap\n");
			ret = -EIO;
			goto err_NEW;
		}

		sgt = kzalloc(sizeof(*sgt), GFP_KERNEL);
		if (!sgt) {
			ret = -ENOMEM;
			goto err_NEW;
		}
		mtk_buf->sgt = sgt;

		ret = sg_alloc_table(sgt, npages, GFP_KERNEL);
		if (ret)
			goto err_NEW;

		for_each_sg(sgt->sgl, sgl, npages, i) {
			sg_set_page(sgl, pages[i], PAGE_SIZE, 0);
		}

		npages = dma_map_sg(dev->dev, sgt->sgl, sgt->nents,
			DMA_BIDIRECTIONAL);
		if (!npages) {
			DRM_INFO("DBG_YT dma_map_sg failed\n");
			goto err_NEW;
		}

		mtk_buf->paddr = 0;
		mtk_buf->size = size;
		mtk_buf->mva_addr = sg_dma_address(sgt->sgl);
	} else if (flags == 3) {
		init_dma_attrs(&mtk_buf->dma_attrs);

		size = PAGE_ALIGN(size);
		mtk_buf->kvaddr = dma_alloc_attrs(dev->dev, size,
			&mtk_buf->mva_addr,	GFP_KERNEL,
			&mtk_buf->dma_attrs);

		mtk_buf->paddr = 0;
		mtk_buf->size = size;
	}
#endif
	else {
		ret = -EINVAL;
		goto err_size;
	}
	mtk_gem->flags = flags;

	DRM_INFO("kvaddr = %X mva_addr = %X\n",
		(unsigned int)mtk_buf->kvaddr, mtk_buf->mva_addr);
	ret = drm_gem_object_init(dev, &mtk_gem->base, size);
	if (ret)
		goto err_init;

	return mtk_gem;

err_NEW:
err_init:
	kfree(mtk_buf->kvaddr);
err_size:
	kfree(mtk_buf);
err_buf:
	kfree(mtk_gem);
err:
	return ERR_PTR(ret);
}

void mtk_drm_gem_free_object(struct drm_gem_object *gem)
{
	struct mtk_drm_gem_obj *mtk_gem = to_mtk_gem_obj(gem);

	DRM_DEBUG_KMS("handle count = %d\n", gem->handle_count);

	drm_gem_free_mmap_offset(gem);

	/* release file pointer to gem object. */
	drm_gem_object_release(gem);

	kfree(mtk_gem);
}

int mtk_drm_gem_dumb_create(struct drm_file *file_priv,
			       struct drm_device *dev,
			       struct drm_mode_create_dumb *args)

{
	struct mtk_drm_gem_obj *mtk_gem;
	unsigned int min_pitch = args->width * ((args->bpp + 7) / 8);
	int ret;

	args->pitch = min_pitch;
	args->size = args->pitch * args->height;

	dev_dbg(dev->dev, "DBG_YT mtk_drm_gem_dumb_create\n");
	mtk_gem = mtk_drm_gem_create(dev, 3, args->size);
	if (IS_ERR(mtk_gem))
		return PTR_ERR(mtk_gem);

	/*
	 * allocate a id of idr table where the obj is registered
	 * and handle has the id what user can see.
	 */
	ret = drm_gem_handle_create(file_priv, &mtk_gem->base, &args->handle);
	if (ret)
		return ret;

	/* drop reference from allocate - handle holds it now. */
	drm_gem_object_unreference_unlocked(&mtk_gem->base);

	return 0;
}

int mtk_drm_gem_dumb_map_offset(struct drm_file *file_priv,
				   struct drm_device *dev, uint32_t handle,
				   uint64_t *offset)
{
	struct drm_gem_object *obj;
	int ret = 0;

	dev_dbg(dev->dev, "DBG_YT mtk_drm_gem_dumb_map_offset\n");
	mutex_lock(&dev->struct_mutex);

	/*
	 * get offset of memory allocated for drm framebuffer.
	 * - this callback would be called by user application
	 *	with DRM_IOCTL_MODE_MAP_DUMB command.
	 */

	obj = drm_gem_object_lookup(dev, file_priv, handle);
	if (!obj) {
		DRM_ERROR("failed to lookup gem object.\n");
		ret = -EINVAL;
		goto unlock;
	}

	ret = drm_gem_create_mmap_offset(obj);
	if (ret)
		goto out;

	*offset = drm_vma_node_offset_addr(&obj->vma_node);
	DRM_DEBUG_KMS("offset = 0x%lx\n", (unsigned long)*offset);

out:
	drm_gem_object_unreference(obj);
unlock:
	mutex_unlock(&dev->struct_mutex);
	return ret;
}

int mtk_drm_gem_mmap(struct file *filp, struct vm_area_struct *vma)
{
	struct mtk_drm_gem_obj *mtk_gem;
	struct drm_gem_object *gem;
	int ret;

	/* set vm_area_struct. */
	ret = drm_gem_mmap(filp, vma);
	if (ret) {
		DRM_ERROR("failed to mmap.\n");
		return ret;
	}

	gem = vma->vm_private_data;
	mtk_gem = to_mtk_gem_obj(gem);

	if (mtk_gem->flags == 0 || mtk_gem->flags == 2) {
		/*
		 * get page frame number to physical memory to be mapped
		 * to user space.
		 */
		ret = remap_pfn_range(vma, vma->vm_start,
			mtk_gem->buffer->paddr >> PAGE_SHIFT,
			vma->vm_end - vma->vm_start, vma->vm_page_prot);
	} else if (mtk_gem->flags == 1) {
		struct mtk_drm_gem_buf *buffer = mtk_gem->buffer;
		unsigned long usize, uaddr;
		int i = 0;

		uaddr = vma->vm_start;
		usize = vma->vm_end - vma->vm_start;

		if (!buffer->pages)
			return -EINVAL;

		vma->vm_flags |= VM_MIXEDMAP;

		do {
			ret = vm_insert_page(vma, uaddr, buffer->pages[i++]);
			if (ret) {
				DRM_ERROR("failed to remap %d\n", ret);
				return ret;
			}

			uaddr += PAGE_SIZE;
			usize -= PAGE_SIZE;
		} while (usize > 0);
	} else {
		struct drm_file *file_priv = filp->private_data;
		struct drm_device *dev = file_priv->minor->dev;
		struct mtk_drm_gem_buf *buffer = mtk_gem->buffer;

		vma->vm_flags |= VM_MIXEDMAP;

		ret = dma_mmap_attrs(dev->dev, vma, buffer->kvaddr,
			buffer->mva_addr, buffer->size, &buffer->dma_attrs);
		if (ret) {
			DRM_ERROR("failed to remap dma %d\n", ret);
			return ret;
		}
	}

	if (ret)
		drm_gem_vm_close(vma);

	return ret;
}

