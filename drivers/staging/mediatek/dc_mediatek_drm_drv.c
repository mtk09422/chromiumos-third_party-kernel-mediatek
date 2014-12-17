#include <linux/version.h>
#include <linux/module.h>
#include <linux/vmalloc.h>

#include <drm/drmP.h>
#include <drm/drm_crtc.h>
#include <drm/drm_crtc_helper.h>

#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#ifdef CONFIG_MTK_IOMMU_MT8135
#include <../drivers/iommu/mtk_iommu.h>
#endif
#include <pvr_drm_display.h>

/*#include "mediatek_drm_debugfs.h"*/

#include "mediatek_drm_drv.h"
#include "mediatek_drm_crtc.h"
#include "mediatek_drm_fb.h"

#include "mediatek_drm_gem.h"
#include "mediatek_drm_output.h"
#include "mediatek_drm_hw.h"

#include "mediatek_drm_dev_if.h"

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
#ifdef CONFIG_MTK_IOMMU_MT8135
	dma_free_attrs(buffer->dev->dev, buffer->base.size, buffer->base.pages,
		buffer->base.mva_addr, &buffer->base.dma_attrs);
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

	mtk_clear_vblank(mtk_crtc->regs);
	mtk_drm_crtc_irq(mtk_crtc);

	return IRQ_HANDLED;
}

static int mtk_drm_kms_init(struct drm_device *dev)
{
	struct mtk_drm_private *priv = get_mtk_drm_private(dev);
	struct mtk_drm_crtc *mtk_crtc;
	int irq, nr, err;

	drm_mode_config_init(dev);

	dev->mode_config.min_width = 0;
	dev->mode_config.min_height = 0;

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

	mtk_crtc->dsi_reg = of_iomap(mediatek_drm_pdev->dev.of_node, 1);
	if (IS_ERR(mtk_crtc->dsi_reg))
		return PTR_ERR(mtk_crtc->dsi_reg);

	mtk_crtc->mutex_regs = of_iomap(mediatek_drm_pdev->dev.of_node, 2);
	if (IS_ERR(mtk_crtc->mutex_regs))
		return PTR_ERR(mtk_crtc->mutex_regs);

	mtk_crtc->dsi_ana_reg = of_iomap(mediatek_drm_pdev->dev.of_node, 3);
	if (IS_ERR(mtk_crtc->dsi_ana_reg))
		return PTR_ERR(mtk_crtc->dsi_ana_reg);


	mtk_dsi_probe(dev);


	/*
	 * We don't use the drm_irq_install() helpers provided by the DRM
	 * core, so we need to set this manually in order to allow the
	 * DRM_IOCTL_WAIT_VBLANK to operate correctly.
	 */
	dev->irq_enabled = true;
	irq = platform_get_irq(mediatek_drm_pdev, 0);
	pr_info("DBG_YT mtk_drm_kms_init irq=%d\n", irq);
	err = devm_request_irq(&mediatek_drm_pdev->dev, irq, mtk_drm_irq,
		IRQF_SHARED, dev_name(&mediatek_drm_pdev->dev), mtk_crtc);

	if (err < 0)
		pr_info("DBG_YT devm_request_irq %d fail %d\n", irq, err);

	err = drm_vblank_init(dev, MAX_CRTC);
	if (err < 0)
		return err;

	mtk_output_init(dev);
	mtk_enable_vblank(mtk_crtc->regs);

#ifdef CONFIG_MTK_IOMMU_MT8135
	{
		struct mtkmmu_drvdata mmuport[4];

		/* M4U_PORT_OVL_CH0, M4U_LARB2_PORTn(1) */
		/* M4U_PORT_OVL_CH1, M4U_LARB2_PORTn(2) */
		/* M4U_PORT_OVL_CH2, M4U_LARB2_PORTn(3) */
		/* M4U_PORT_OVL_CH3, M4U_LARB2_PORTn(4) */
		mmuport[0].portid = 17;
		mmuport[0].virtuality = 1;
		mmuport[0].pnext = &mmuport[1];
		mmuport[1].portid = 18;
		mmuport[1].virtuality = 1;
#if 1
		mmuport[1].pnext = NULL; /* &mmuport[2]; */
#else
		mmuport[1].pnext = &mmuport[2];
#endif
		mmuport[2].portid = 19;
		mmuport[2].virtuality = 1;
		mmuport[2].pnext = &mmuport[3];
		mmuport[3].portid = 20;
		mmuport[3].virtuality = 1;
		mmuport[3].pnext = NULL;
		dev->dev->archdata.iommu = &mmuport[0];

		pr_info("DBG_YT attach_device M4U_PORT_OVL\n");
		arm_iommu_attach_device(dev->dev, mtk_iommu_mapping());
	}
#endif

#ifdef CONFIG_DRM_MEDIATEK_FBDEV
	mtk_fbdev_create(dev);
#endif

	drm_kms_helper_poll_init(dev);
	DispClockSetup(&mediatek_drm_pdev->dev, &(mtk_crtc->disp_clks));

	/*
	pr_info("DBG_YT iomem regs = %X", (unsigned int)mtk_crtc->regs);
	mediatek_drm_debugfs_init(mtk_crtc->regs);
	*/
	pr_info("DispClockOn\n");
	DispClockOn(mtk_crtc->disp_clks);

	pr_info("MainDispPathSetup\n");
	MainDispPathSetup(&mtk_crtc->base);

	pr_info("MainDispPathPowerOn\n");
	MainDispPathPowerOn(&mtk_crtc->base);

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

	pr_info("dc_drmmtk_configure mediatek_drm_pdev=%p, display_priv=%p\n",
		mediatek_drm_pdev, display_priv);

	ret = mtk_drm_kms_init(display_dev->dev);

	if (ret)
		pr_info("dc_drmmtk_configure fail\n");

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
#ifndef CONFIG_MTK_IOMMU_MT8135
	uint32_t i;
#endif
	uint32_t npages;
	int err;

	DRM_DEBUG_DRIVER("dc_drmmtk_buffer_alloc\n");

	if (!display_priv || size == 0 || !buffer_out) {
		DRM_DEBUG_KMS("fail1 (%d)\n", size);
		return -EINVAL;
	}

	buffer = kzalloc(sizeof(*buffer), GFP_KERNEL);
	if (!buffer)
		return -ENOMEM;

	size = PAGE_ALIGN(size);
	npages = size >> PAGE_SHIFT;

#ifdef CONFIG_MTK_IOMMU_MT8135
	init_dma_attrs(&buffer->base.dma_attrs);

	dma_set_attr(DMA_ATTR_NO_KERNEL_MAPPING, &buffer->base.dma_attrs);

	pages = dma_alloc_attrs(display_dev->dev->dev, size,
		&buffer->base.mva_addr,	GFP_KERNEL,
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
	pages = kmalloc(npages * sizeof(*pages), GFP_KERNEL);
	if (!pages) {
		err = -ENOMEM;
		goto err_free_buffer;
	}

	kvaddr = kzalloc(size, GFP_KERNEL);
	if (!kvaddr) {
		pr_info("fail kvaddr\n");
		err = -ENOMEM;
		goto err_free_pages;
	}

	buffer->base.mva_addr = virt_to_phys(kvaddr);
	for (i = 0; i < npages; i++)
		pages[i] = phys_to_page(buffer->base.mva_addr + i*PAGE_SIZE);

#endif

	DRM_DEBUG_DRIVER("buffer->base.mva_addr = 0x%x, kva = %p, size = %d\n",
		buffer->base.mva_addr, kvaddr, size);

	buffer->dev = display_dev->dev;
	buffer->base.pages = pages;
	buffer->base.size = size;
	buffer->base.kvaddr = kvaddr;

	kref_init(&buffer->refcount);

	*buffer_out = buffer;

	return 0;

err_free_pages:
#ifdef CONFIG_MTK_IOMMU_MT8135
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


	addr_array = kmalloc(npages * sizeof(*addr_array), GFP_KERNEL);
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
	if (!display_priv)
		return -EINVAL;

	if (crtc >= MAX_CRTC || crtc < 0) {
		pr_err(DEVNAME " - %s: invalid crtc (%d)\n", __func__, crtc);
		return -EINVAL;
	}

	return 0;

}
EXPORT_SYMBOL(dc_drmmtk_enable_vblank);

int dc_drmmtk_disable_vblank(void *display_priv, int crtc)
{
	if (!display_priv)
		return -EINVAL;

	if (crtc >= MAX_CRTC || crtc < 0) {
		pr_err(DEVNAME " - %s: invalid crtc (%d)\n", __func__, crtc);
		return -EINVAL;
	}

	return 0;

}
EXPORT_SYMBOL(dc_drmmtk_disable_vblank);


static int mtk_drm_probe(struct platform_device *pdev)
{
	pr_info("DBG_YT mtk_drm_probe(%p)\n", pdev);

	if (!pdev->dev.of_node) {
		pr_info("No DT found\n");
		return -EINVAL;
	}

	mediatek_drm_pdev = pdev;

	return 0;
}

static int mtk_drm_remove(struct platform_device *pdev)
{
	return 0;
}

static const struct of_device_id mediatek_drm_of_ids[] = {
	{ .compatible = "mediatek,mt8135-drm", },
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

	pr_info("DBG_YT it6151mipirx_i2c_driver\n");
	err = i2c_add_driver(&it6151mipirx_i2c_driver);
	if (err < 0)
		return err;

	pr_info("DBG_YT it6151dptx_i2c_driver\n");
	err = i2c_add_driver(&it6151dptx_i2c_driver);
	if (err < 0)
		return err;


	pr_info("DBG_YT mediatek_drm_init\n");
	err = platform_driver_register(&mediatek_drm_platform_driver);
	if (err < 0)
		return err;

#if 0
	err = platform_driver_register(&mediatek_hdmi_driver);
	if (err < 0)
		goto unregister_sor;
#endif

	return 0;

#if 0
unregister_hdmi:
	platform_driver_register(&mediatek_drm_driver);
	return err;
#endif
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
