#include <linux/version.h>
#include <linux/module.h>
#include <linux/vmalloc.h>
#include <linux/component.h>

#include <drm/drmP.h>
#include <drm/drm_crtc.h>
#include <drm/drm_crtc_helper.h>

#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <pvr_drm_display.h>

#include "mediatek_drm_debugfs.h"

#include "mediatek_drm_drv.h"
#include "mediatek_drm_crtc.h"
#include "mediatek_drm_fb.h"

#include "mediatek_drm_gem.h"
#include "mediatek_drm_output.h"
#include "mediatek_drm_ddp.h"

#include "mediatek_drm_dev_if.h"

#ifdef CONFIG_MTK_IOMMU
#include <asm/dma-iommu.h>
#include <linux/of_platform.h>
#endif

static struct drm_mode_config_funcs mediatek_drm_mode_config_funcs = {
	.fb_create = mtk_drm_mode_fb_create,
#ifdef CONFIG_DRM_MEDIATEK_FBDEV
	.output_poll_changed = mtk_drm_mode_output_poll_changed,
#endif
};

#define MAKESTRING(x)	#x
#define TOSTRING(x)	MAKESTRING(x)

#define DEVNAME	TOSTRING(DISPLAY_CONTROLLER)

static    struct platform_device *mediatek_drm_pdev;

static void display_buffer_destroy(struct kref *kref)
{
	struct pvr_drm_display_buffer *buffer =
		container_of(kref, struct pvr_drm_display_buffer, refcount);

	DRM_DEBUG_DRIVER("display_buffer_destroy %p\n", buffer);
#ifdef CONFIG_MTK_IOMMU
	dma_free_attrs(&mediatek_drm_pdev->dev, buffer->base.size,
		       buffer->base.pages, buffer->base.mva_addr,
		&buffer->base.dma_attrs);
	vunmap(buffer->base.kvaddr);
#else
	kfree(buffer->base.kvaddr);
	kfree(buffer->base.pages);
#endif

	kfree(buffer);
}

static inline void display_buffer_ref(struct pvr_drm_display_buffer *buffer)
{
	kref_get(&buffer->refcount);
}

static inline void display_buffer_unref(struct pvr_drm_display_buffer *buffer)
{
	kref_put(&buffer->refcount, display_buffer_destroy);
}

static irqreturn_t mtk_drm_irq(int irq, void *data)
{
	struct mtk_drm_crtc *mtk_crtc = data;

	mtk_clear_vblank(mtk_crtc->od_regs);
	mtk_drm_crtc_irq(mtk_crtc);

	return IRQ_HANDLED;
}

static int mtk_drm_kms_init(struct drm_device *dev)
{
	struct mtk_drm_private *priv = get_mtk_drm_private(dev);
	struct device *pdev = get_mtk_drm_device(dev);
	struct mtk_drm_crtc *mtk_crtc;
	struct mtk_drm_crtc *mtk_crtc_ext;
	int irq, nr, err;

	drm_mode_config_init(dev);

	dev->mode_config.min_width = 320;
	dev->mode_config.min_height = 240;

	/*
	 * set max width and height as default value(4096x4096).
	 * this value would be used to check framebuffer size limitation
	 * at drm_mode_addfb().
	 */
	dev->mode_config.max_width = 4096;
	dev->mode_config.max_height = 4096;
	dev->mode_config.funcs = &mediatek_drm_mode_config_funcs;

	for (nr = 0; nr < MAX_CRTC; nr++) {
		err = mtk_drm_crtc_create(dev, nr);
		if (err)
			return err;
			/* goto err_crtc; */
	}

	for (nr = 0; nr < MAX_PLANE; nr++) {
		/* err = mtk_plane_init(dev, nr); */
		if (err)
			return err;
			/* goto err_crtc; */
	}

	mtk_crtc = to_mtk_crtc(priv->crtc[0]);
	mtk_crtc->regs = of_iomap(mediatek_drm_pdev->dev.of_node, 0);
	if (IS_ERR(mtk_crtc->regs))
		return PTR_ERR(mtk_crtc->regs);

	mtk_crtc->ovl_regs[0] = of_iomap(mediatek_drm_pdev->dev.of_node, 1);
	if (IS_ERR(mtk_crtc->ovl_regs[0]))
		return PTR_ERR(mtk_crtc->ovl_regs[0]);

	mtk_crtc->ovl_regs[1] = of_iomap(mediatek_drm_pdev->dev.of_node, 2);
	if (IS_ERR(mtk_crtc->ovl_regs[1]))
		return PTR_ERR(mtk_crtc->ovl_regs[1]);

	mtk_crtc->rdma_regs[0] = of_iomap(mediatek_drm_pdev->dev.of_node, 3);
	if (IS_ERR(mtk_crtc->rdma_regs[0]))
		return PTR_ERR(mtk_crtc->rdma_regs[0]);

	mtk_crtc->rdma_regs[1] = of_iomap(mediatek_drm_pdev->dev.of_node, 4);
	if (IS_ERR(mtk_crtc->rdma_regs[1]))
		return PTR_ERR(mtk_crtc->rdma_regs[1]);

	mtk_crtc->color_regs[0] = of_iomap(mediatek_drm_pdev->dev.of_node, 5);
	if (IS_ERR(mtk_crtc->color_regs[0]))
		return PTR_ERR(mtk_crtc->color_regs[0]);

	mtk_crtc->color_regs[1] = of_iomap(mediatek_drm_pdev->dev.of_node, 6);
	if (IS_ERR(mtk_crtc->color_regs[1]))
		return PTR_ERR(mtk_crtc->color_regs[1]);

	mtk_crtc->aal_regs = of_iomap(mediatek_drm_pdev->dev.of_node, 7);
	if (IS_ERR(mtk_crtc->aal_regs))
		return PTR_ERR(mtk_crtc->aal_regs);

	mtk_crtc->gamma_regs = of_iomap(mediatek_drm_pdev->dev.of_node, 8);
	if (IS_ERR(mtk_crtc->gamma_regs))
		return PTR_ERR(mtk_crtc->gamma_regs);

	mtk_crtc->ufoe_regs = of_iomap(mediatek_drm_pdev->dev.of_node, 9);
	if (IS_ERR(mtk_crtc->ufoe_regs))
		return PTR_ERR(mtk_crtc->ufoe_regs);

	mtk_crtc->dsi_reg = of_iomap(mediatek_drm_pdev->dev.of_node, 10);
	if (IS_ERR(mtk_crtc->dsi_reg))
		return PTR_ERR(mtk_crtc->dsi_reg);

	mtk_crtc->mutex_regs = of_iomap(mediatek_drm_pdev->dev.of_node, 11);
	if (IS_ERR(mtk_crtc->mutex_regs))
		return PTR_ERR(mtk_crtc->mutex_regs);

	mtk_crtc->od_regs = of_iomap(mediatek_drm_pdev->dev.of_node, 12);
	if (IS_ERR(mtk_crtc->od_regs))
		return PTR_ERR(mtk_crtc->od_regs);

	mtk_crtc->dsi_ana_reg = of_iomap(mediatek_drm_pdev->dev.of_node, 13);
	if (IS_ERR(mtk_crtc->dsi_ana_reg))
		return PTR_ERR(mtk_crtc->dsi_ana_reg);

	mtk_crtc_ext = to_mtk_crtc(priv->crtc[1]);
	mtk_crtc_ext->regs = mtk_crtc->regs;
	mtk_crtc_ext->ovl_regs[0] = mtk_crtc->ovl_regs[0];
	mtk_crtc_ext->ovl_regs[1] = mtk_crtc->ovl_regs[1];
	mtk_crtc_ext->rdma_regs[0] = mtk_crtc->rdma_regs[0];
	mtk_crtc_ext->rdma_regs[1] = mtk_crtc->rdma_regs[1];
	mtk_crtc_ext->color_regs[0] = mtk_crtc->color_regs[0];
	mtk_crtc_ext->color_regs[1] = mtk_crtc->color_regs[1];
	mtk_crtc_ext->aal_regs = mtk_crtc->aal_regs;
	mtk_crtc_ext->gamma_regs = mtk_crtc->gamma_regs;
	mtk_crtc_ext->ufoe_regs = mtk_crtc->ufoe_regs;
	mtk_crtc_ext->dsi_reg = mtk_crtc->dsi_reg;
	mtk_crtc_ext->mutex_regs = mtk_crtc->mutex_regs;
	mtk_crtc_ext->od_regs = mtk_crtc->od_regs;
	mtk_crtc_ext->dsi_ana_reg = mtk_crtc->dsi_ana_reg;

	mtk_dsi_probe(dev);

	/*
	 * We don't use the drm_irq_install() helpers provided by the DRM
	 * core, so we need to set this manually in order to allow the
	 * DRM_IOCTL_WAIT_VBLANK to operate correctly.
	 */
	dev->irq_enabled = true;
	irq = platform_get_irq(mediatek_drm_pdev, 0);
	DRM_DEBUG_KMS("DBG_YT mtk_drm_kms_init irq=%d\n", irq);
	err = devm_request_irq(&mediatek_drm_pdev->dev, irq, mtk_drm_irq,
			       IRQF_TRIGGER_LOW,
			       dev_name(&mediatek_drm_pdev->dev), mtk_crtc);

	if (err < 0)
		pr_info("DBG_YT devm_request_irq %d fail %d\n", irq, err);

	err = drm_vblank_init(dev, MAX_CRTC);
	if (err < 0)
		return err;

	mtk_output_init(dev);
	mtk_enable_vblank(mtk_crtc->od_regs);

	drm_kms_helper_poll_init(dev);
	mediatek_drm_debugfs_init(&mtk_crtc->base);

	/* MainDispPathClear(&mtk_crtc->base); */
	DispClockSetup(&mediatek_drm_pdev->dev, &(mtk_crtc->disp_clks));

	DRM_INFO("DispClockOn\n");
	DispClockOn(mtk_crtc->disp_clks);

	DRM_INFO("MainDispPathSetup\n");
	MainDispPathSetup(&mtk_crtc->base);

	DRM_INFO("MainDispPathPowerOn\n");
	MainDispPathPowerOn(&mtk_crtc->base);
	ExtDispPathPowerOn(&mtk_crtc->base);

	DRM_INFO("ExtDispPathSetup\n");
	ExtDispPathSetup(&mtk_crtc->base);

#ifdef CONFIG_MTK_IOMMU
	{
		struct device_node *node;
		struct platform_device *pdev;
		struct dma_iommu_mapping *imu_mapping;

		OVLLayerSwitch(mtk_crtc->ovl_regs, 0, 0, 0);
		OVLLayerSwitch(mtk_crtc->ovl_regs, 0, 1, 0);
		OVLLayerSwitch(mtk_crtc->ovl_regs, 0, 2, 0);
		OVLLayerSwitch(mtk_crtc->ovl_regs, 0, 3, 0);
		DRM_INFO("DBG_YT disable ovl layer\n");

		node = of_parse_phandle(mediatek_drm_pdev->dev.of_node,
					"iommus", 0);
		if (!node)
			return 0;

		pdev = of_find_device_by_node(node);
		if (WARN_ON(!pdev)) {
			of_node_put(node);
			return -EINVAL;
		}

		imu_mapping = pdev->dev.archdata.mapping;

		DRM_INFO("find dev name %s map %p\n", pdev->name, imu_mapping);
		arm_iommu_attach_device(&mediatek_drm_pdev->dev, imu_mapping);
	}
#endif

#ifdef CONFIG_DRM_MEDIATEK_FBDEV
	mtk_fbdev_create(dev);
#endif

	err = component_bind_all(pdev, dev);
	if (err)
		return err;

	return 0;
}

static void mtk_drm_kms_deinit(struct drm_device *dev)
{
	drm_kms_helper_poll_fini(dev);

#ifdef CONFIG_DRM_MEDIATEK_FBDEV
	mtk_fbdev_destroy(dev);
#endif

	drm_vblank_cleanup(dev);
	drm_mode_config_cleanup(dev);
}

int dc_drmmtk_init(struct drm_device *dev, void **display_priv_out)
{
	struct mtk_display_device *dev_private;

	if (!dev || !display_priv_out)
		return -EINVAL;

	dev_private = kzalloc(sizeof(*dev_private), GFP_KERNEL);
	if (!dev_private) {
		DRM_ERROR("failed to allocate private\n");
		return -ENOMEM;
	}

	dev_private->dev = dev;
	dev_private->mediatek_dev = &mediatek_drm_pdev->dev;
	*display_priv_out = dev_private;

	return 0;
}
EXPORT_SYMBOL(dc_drmmtk_init);

int dc_drmmtk_configure(void *display_priv)
{
	struct mtk_display_device *display_dev = display_priv;
	int ret;

	if (!display_dev)
		return -EINVAL;

	DRM_DEBUG_KMS("dc_drmmtk_configure pdev=%p, display_priv=%p\n",
		      mediatek_drm_pdev, display_priv);

	ret = mtk_drm_kms_init(display_dev->dev);

	if (ret)
		DRM_DEBUG_KMS("dc_drmmtk_configure fail\n");

	return ret;
}
EXPORT_SYMBOL(dc_drmmtk_configure);

void dc_drmmtk_cleanup(void *display_priv)
{
	struct mtk_display_device *display_dev = display_priv;
	struct drm_device *dev = display_dev->dev;

	if (display_dev) {
		mtk_drm_kms_deinit(dev);
		kfree(display_dev);
	}
}
EXPORT_SYMBOL(dc_drmmtk_cleanup);

int dc_drmmtk_buffer_alloc(void *display_priv,
			   size_t size,
			   struct pvr_drm_display_buffer **buffer_out)
{
	struct pvr_drm_display_buffer *buffer;
	struct mtk_display_device *display_dev = display_priv;
	struct page **pages;
	void *kvaddr;
#ifndef CONFIG_MTK_IOMMU
	uint32_t i;
#endif
	uint32_t npages;
	int err;

	if (!display_priv || size == 0 || !buffer_out) {
		DRM_DEBUG_KMS("fail1 (%d)\n", (int)size);
		return -EINVAL;
	}

	buffer = kzalloc(sizeof(*buffer), GFP_KERNEL);
	if (!buffer)
		return -ENOMEM;

	size = PAGE_ALIGN(size);
	npages = size >> PAGE_SHIFT;

#ifdef CONFIG_MTK_IOMMU
	init_dma_attrs(&buffer->base.dma_attrs);

	dma_set_attr(DMA_ATTR_NO_KERNEL_MAPPING, &buffer->base.dma_attrs);

	pages = dma_alloc_attrs(&mediatek_drm_pdev->dev, size,
				(dma_addr_t *)&buffer->base.mva_addr, GFP_KERNEL,
				&buffer->base.dma_attrs);
	if (!pages) {
		err = -ENOMEM;
		goto err_free_buffer;
	}
	buffer->base.paddr = 0;

/* test!!!
	if(1)
	{
		int i;
		pr_info( "test print page info begin\n");

		for (i = 0; i < npages; i++) {
			phys_addr_t paddr = page_to_phys(pages[i]);
		}

		pr_info( "test print page info end\n");
	} */

	kvaddr = vmap(pages, npages, 0, PAGE_KERNEL);
	if (!kvaddr) {
		err = -ENOMEM;
		goto err_free_pages;
	}

/*	for (i = 0; i < npages; i++) {
		phys_addr_t paddr = dma_to_phys(display_dev->dev->dev,
			buffer->base.mva_addr+i*PAGE_SIZE);
		pages[i] = phys_to_page(paddr);
	} */

#else
	pages = kmalloc_array(npages, sizeof(*pages), GFP_KERNEL);
	if (!pages) {
		err = -ENOMEM;
		goto err_free_buffer;
	}

	kvaddr = kzalloc(size, GFP_KERNEL);
	if (!kvaddr) {
		err = -ENOMEM;
		goto err_free_pages;
	}

	buffer->base.mva_addr = virt_to_phys(kvaddr);
	for (i = 0; i < npages; i++)
		pages[i] = phys_to_page(buffer->base.mva_addr + i*PAGE_SIZE);

#endif

	DRM_DEBUG_KMS("buffer->base.mva_addr = 0x%x, kva = %p, size = %d\n",
		      buffer->base.mva_addr, kvaddr, (int)size);

	buffer->base.pages = pages;
	buffer->base.size = size;
	buffer->base.kvaddr = kvaddr;

	kref_init(&buffer->refcount);

	*buffer_out = buffer;

	return 0;

err_free_pages:
#ifdef CONFIG_MTK_IOMMU
	dma_free_attrs(display_dev->dev->dev, size, pages,
		       buffer->base.mva_addr, &buffer->base.dma_attrs);
#else
	kfree(pages);
#endif

err_free_buffer:
	kfree(buffer);

	return err;
}
EXPORT_SYMBOL(dc_drmmtk_buffer_alloc);

int dc_drmmtk_buffer_free(struct pvr_drm_display_buffer *buffer)
{
	if (!buffer)
		return -EINVAL;

	display_buffer_unref(buffer);

	return 0;
}
EXPORT_SYMBOL(dc_drmmtk_buffer_free);

uint64_t *dc_drmmtk_buffer_acquire(struct pvr_drm_display_buffer *buffer)
{
	uint64_t *addr_array;
	uint32_t npages, i;
	phys_addr_t addr;

	DRM_DEBUG_DRIVER("dc_drmmtk_buffer_acquire\n");

	if (!buffer)
		return ERR_PTR(-EINVAL);

	display_buffer_ref(buffer);

	WARN_ON(buffer->base.size != PAGE_ALIGN(buffer->base.size));
	npages = buffer->base.size >> PAGE_SHIFT;

	addr_array = kmalloc_array(npages, sizeof(*addr_array), GFP_KERNEL);
	if (!addr_array) {
		display_buffer_unref(buffer);
		return ERR_PTR(-ENOMEM);
	}

	for (i = 0; i < npages; i++) {
		addr = page_to_phys(buffer->base.pages[i]);
		addr_array[i] = (uint64_t)(uintptr_t)addr;
	}

	return addr_array;
}
EXPORT_SYMBOL(dc_drmmtk_buffer_acquire);

int dc_drmmtk_buffer_release(struct pvr_drm_display_buffer *buffer,
			     uint64_t *dev_paddr_array)
{
	if (!buffer || !dev_paddr_array)
		return -EINVAL;

	display_buffer_unref(buffer);

	kfree(dev_paddr_array);

	return 0;
}
EXPORT_SYMBOL(dc_drmmtk_buffer_release);

void *dc_drmmtk_buffer_vmap(struct pvr_drm_display_buffer *buffer)
{
	uint32_t npages;

	DRM_DEBUG_DRIVER("dc_drmmtk_buffer_vmap\n");

	if (!buffer)
		return NULL;

	display_buffer_ref(buffer);

	WARN_ON(buffer->base.size != PAGE_ALIGN(buffer->base.size));

	npages = buffer->base.size >> PAGE_SHIFT;
	return vmap(buffer->base.pages, npages, 0, PAGE_KERNEL);
}
EXPORT_SYMBOL(dc_drmmtk_buffer_vmap);

void dc_drmmtk_buffer_vunmap(struct pvr_drm_display_buffer *buffer, void *vaddr)
{
	if (buffer) {
		vunmap(vaddr);
		display_buffer_unref(buffer);
	}
}
EXPORT_SYMBOL(dc_drmmtk_buffer_vunmap);

u32 dc_drmmtk_get_vblank_counter(void *display_priv, int crtc)
{
	struct mtk_display_device *display_dev = display_priv;

	if (!display_dev)
		return 0;

	return drm_vblank_count(display_dev->dev, crtc);
}
EXPORT_SYMBOL(dc_drmmtk_get_vblank_counter);

int dc_drmmtk_enable_vblank(void *display_priv, int crtc)
{
	struct mtk_drm_private *priv;
	struct mtk_drm_crtc *mtk_crtc;
	struct mtk_display_device *display_dev = display_priv;

	if (!display_dev)
		return -EINVAL;

	if (crtc >= MAX_CRTC || crtc < 0) {
		pr_err(DEVNAME " - %s: invalid crtc (%d)\n", __func__, crtc);
		return -EINVAL;
	}

	priv = get_mtk_drm_private(display_dev->dev);
	mtk_crtc = to_mtk_crtc(priv->crtc[crtc]);

	if (mtk_crtc)
		mtk_enable_vblank(mtk_crtc->od_regs);

	return 0;
}
EXPORT_SYMBOL(dc_drmmtk_enable_vblank);

int dc_drmmtk_disable_vblank(void *display_priv, int crtc)
{
	struct mtk_drm_private *priv;
	struct mtk_drm_crtc *mtk_crtc;
	struct mtk_display_device *display_dev = display_priv;

	if (!display_dev)
		return -EINVAL;

	if (crtc >= MAX_CRTC || crtc < 0) {
		pr_err(DEVNAME " - %s: invalid crtc (%d)\n", __func__, crtc);
		return -EINVAL;
	}

	priv = get_mtk_drm_private(display_dev->dev);
	mtk_crtc = to_mtk_crtc(priv->crtc[crtc]);

	if (mtk_crtc)
		mtk_disable_vblank(mtk_crtc->od_regs);
	return 0;
}
EXPORT_SYMBOL(dc_drmmtk_disable_vblank);

static int compare_of(struct device *dev, void *data)
{
	return dev->of_node == data;
}

static int mtk_drm_add_components(struct device *master, struct master *m)
{
	struct device_node *np = master->of_node;
	unsigned i;
	int ret;

	for (i = 0; ; i++) {
		struct device_node *node;

		node = of_parse_phandle(np, "connectors", i);
		if (!node)
			break;

		ret = component_master_add_child(m, compare_of, node);
		of_node_put(node);
		if (ret)
			return ret;
	}

	return 0;
}

static int mtk_drm_bind(struct device *dev)
{
	return 0; /* drm_platform_init(&mediatek_drm_driver, to_platform_device(dev)); */
}

static void mtk_drm_unbind(struct device *dev)
{
	drm_put_dev(platform_get_drvdata(to_platform_device(dev)));
}

static const struct component_master_ops mtk_drm_ops = {
	.add_components	= mtk_drm_add_components,
	.bind		= mtk_drm_bind,
	.unbind		= mtk_drm_unbind,
};

static int mtk_drm_probe(struct platform_device *pdev)
{
	DRM_DEBUG_KMS("mtk_drm_probe(%p)\n", pdev);

	if (!pdev->dev.of_node) {
		pr_info("No DT found\n");
		return -EINVAL;
	}

	mediatek_drm_pdev = pdev;
	component_master_add(&pdev->dev, &mtk_drm_ops);

	return 0;
}

static int mtk_drm_remove(struct platform_device *pdev)
{
	return 0;
}

static const struct of_device_id mediatek_drm_of_ids[] = {
	{ .compatible = "mediatek,mt8173-drm", },
	{ }
};

static struct platform_driver mediatek_drm_platform_driver = {
	.probe	= mtk_drm_probe,
	.remove	= mtk_drm_remove,
	.driver	= {
		.owner	= THIS_MODULE,
		.name	= "mediatek-drm",
		.of_match_table = mediatek_drm_of_ids,
	},
};

static int __init mediatek_drm_init(void)
{
	int err;

#ifdef CONFIG_DRM_MEDIATEK_IT6151
	DRM_INFO("DBG_YT it6151mipirx_i2c_driver\n");
	err = i2c_add_driver(&it6151mipirx_i2c_driver);
	if (err < 0)
		return err;

	DRM_INFO("DBG_YT it6151dptx_i2c_driver\n");
	err = i2c_add_driver(&it6151dptx_i2c_driver);
	if (err < 0)
		return err;
#endif

	DRM_INFO("mediatek_drm_init(DC)\n");
	err = platform_driver_register(&mediatek_drm_platform_driver);
	if (err < 0)
		return err;

	return 0;
}

static void __exit mediatek_drm_exit(void)
{
	DRM_DEBUG_DRIVER("%s\n", __FILE__);

	platform_driver_unregister(&mediatek_drm_platform_driver);
}

module_init(mediatek_drm_init);
module_exit(mediatek_drm_exit);

MODULE_AUTHOR("Mediatek Inc.");
MODULE_LICENSE("GPL");
