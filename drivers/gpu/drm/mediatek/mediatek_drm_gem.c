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

#include "mediatek_drm_gem.h"
#include "drm/mediatek_drm.h"


struct mtk_drm_gem_obj *mtk_drm_gem_init(struct drm_device *dev,
						      unsigned long size)
{
	struct mtk_drm_gem_obj *mtk_gem_obj;
	struct drm_gem_object *obj;
	int ret;

	mtk_gem_obj = kzalloc(sizeof(*mtk_gem_obj), GFP_KERNEL);
	if (!mtk_gem_obj)
		return NULL;

	mtk_gem_obj->size = size;
	obj = &mtk_gem_obj->base;

	ret = drm_gem_object_init(dev, obj, size);
	if (ret < 0) {
		DRM_ERROR("failed to initialize gem object\n");
		kfree(mtk_gem_obj);
		return NULL;
	}

	DRM_DEBUG_KMS("created file object = 0x%p\n", obj->filp);

	return mtk_gem_obj;
}

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
	} else {
		struct page **pages;
		int npages, size_pages;
		int offset, index;

		size = PAGE_ALIGN(size);
		npages = size >> PAGE_SHIFT;
		size_pages = npages * sizeof(*pages);
		pages = kmalloc(size_pages, GFP_KERNEL);
		if (!pages) {
			ret = -ENOMEM;
			goto err_size;
		}
		mtk_buf->pages = pages;

		init_dma_attrs(&mtk_buf->dma_attrs);

		mtk_buf->kvaddr = dma_alloc_attrs(dev->dev, size,
			(dma_addr_t *)&mtk_buf->mva_addr, GFP_KERNEL,
			&mtk_buf->dma_attrs);

		mtk_buf->paddr = 0;
		mtk_buf->size = size;

		for (offset = 0, index = 0;
			offset < size; offset += PAGE_SIZE, index++)
			mtk_buf->pages[index] =
				vmalloc_to_page(mtk_buf->kvaddr + offset);

		mtk_buf->sgt = drm_prime_pages_to_sg(mtk_buf->pages, npages);
	}
	mtk_gem->flags = flags;

	DRM_INFO("kvaddr = %p mva_addr = %X\n",
		mtk_buf->kvaddr, mtk_buf->mva_addr);
	ret = drm_gem_object_init(dev, &mtk_gem->base, size);
	if (ret)
		goto err_mem;

	return mtk_gem;

err_mem:
	if (mtk_buf->paddr)
		kfree(mtk_buf->kvaddr);
	else
		dma_free_attrs(dev->dev, size, mtk_buf->kvaddr,
			mtk_buf->mva_addr, &mtk_buf->dma_attrs);
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

	if (mtk_gem->flags == 0)
		kfree(mtk_gem->buffer->kvaddr);
	else
		dma_free_attrs(gem->dev->dev, mtk_gem->buffer->size,
			mtk_gem->buffer->kvaddr, mtk_gem->buffer->mva_addr,
			&mtk_gem->buffer->dma_attrs);

	kfree(mtk_gem->buffer);
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

	if (mtk_gem->flags == 0) {
		/*
		 * get page frame number to physical memory to be mapped
		 * to user space.
		 */
		ret = remap_pfn_range(vma, vma->vm_start,
			mtk_gem->buffer->paddr >> PAGE_SHIFT,
			vma->vm_end - vma->vm_start, vma->vm_page_prot);
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

int mediatek_gem_map_offset_ioctl(struct drm_device *drm, void *data,
				  struct drm_file *file_priv)
{
	struct drm_mtk_gem_map_off *args = data;

	return mtk_drm_gem_dumb_map_offset(file_priv, drm, args->handle,
						&args->offset);
}

int mediatek_gem_create_ioctl(struct drm_device *dev, void *data,
			      struct drm_file *file_priv)
{
	struct mtk_drm_gem_obj *mtk_gem;
	struct drm_mtk_gem_create *args = data;
	int ret;

	dev_dbg(dev->dev, "DBG_YT mediatek_gem_create_ioctl\n");
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

