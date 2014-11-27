/*
 * Copyright (c) 2014 MediaTek Inc.
 * Author: YT SHEN <yt.shen@mediatek.com>
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
#include <drm/drm_crtc.h>
#include <drm/drm_crtc_helper.h>

#include "mediatek_drm_drv.h"
#include "mediatek_drm_crtc.h"
#include "mediatek_drm_fb.h"
#include "mediatek_drm_gem.h"
#include "mediatek_drm_output.h"
#include "mediatek_drm_hw.h"

#include "mediatek_drm_dev_if.h"

#include <../drivers/iommu/mtk_iommu.h>

#define DRIVER_NAME "mediatek-drm"
#define DRIVER_DESC "Mediatek SoC DRM"
#define DRIVER_DATE "20141031"
#define DRIVER_MAJOR 1
#define DRIVER_MINOR 0

static struct drm_mode_config_funcs mediatek_drm_mode_config_funcs = {
	.fb_create = mtk_drm_mode_fb_create,
#ifdef CONFIG_DRM_MEDIATEK_FBDEV
	.output_poll_changed = mtk_drm_mode_output_poll_changed,
#endif
};

static irqreturn_t mtk_drm_irq(int irq, void *data)
{
	struct mtk_drm_crtc *mtk_crtc = data;
	/* struct drm_device *dev = data;
	unsigned long status; */

	mtk_clear_vblank(mtk_crtc->regs);
	mtk_drm_crtc_irq(mtk_crtc);

	return IRQ_HANDLED;
}

static int mtk_drm_kms_init(struct drm_device *dev)
{
	struct mtk_drm_private *priv = get_mtk_drm_private(dev);
	struct mtk_drm_crtc *mtk_crtc;
	struct resource *regs;
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
	regs = platform_get_resource(dev->platformdev, IORESOURCE_MEM, 0);
	mtk_crtc->regs = devm_ioremap_resource(dev->dev, regs);
	if (IS_ERR(mtk_crtc->regs))
		return PTR_ERR(mtk_crtc->regs);

	regs = platform_get_resource(dev->platformdev, IORESOURCE_MEM, 1);
	mtk_crtc->dsi_reg = devm_ioremap_resource(dev->dev, regs);
	if (IS_ERR(mtk_crtc->dsi_reg))
		return PTR_ERR(mtk_crtc->dsi_reg);

	regs = platform_get_resource(dev->platformdev, IORESOURCE_MEM, 2);
	mtk_crtc->mutex_regs = devm_ioremap_resource(dev->dev, regs);
	if (IS_ERR(mtk_crtc->mutex_regs))
		return PTR_ERR(mtk_crtc->mutex_regs);

	regs = platform_get_resource(dev->platformdev, IORESOURCE_MEM, 3);
	mtk_crtc->dsi_ana_reg = devm_ioremap_resource(dev->dev, regs);
	if (IS_ERR(mtk_crtc->dsi_ana_reg))
		return PTR_ERR(mtk_crtc->dsi_ana_reg);

	mtk_dsi_probe(dev);

	/*
	 * We don't use the drm_irq_install() helpers provided by the DRM
	 * core, so we need to set this manually in order to allow the
	 * DRM_IOCTL_WAIT_VBLANK to operate correctly.
	 */
	dev->irq_enabled = true;
	irq = platform_get_irq(dev->platformdev, 0);
	err = devm_request_irq(dev->dev, irq, mtk_drm_irq, 0,
					dev_name(dev->dev),	mtk_crtc);
	if (err < 0) {
		dev_err(dev->dev, "devm_request_irq %d fail %d\n", irq, err);
		/* dev->irq_enabled = false;
		return ret; */
	}
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
	/* mediatek_drm_debugfs_init(mtk_crtc->regs); */

	/* MainDispPathClear(&mtk_crtc->base); */
	DispClockSetup(dev->dev, &(mtk_crtc->disp_clks));

	pr_info("DispClockOn\n");
	DispClockOn(mtk_crtc->disp_clks);

	pr_info("MainDispPathSetup\n");
	MainDispPathSetup(&mtk_crtc->base);

	pr_info("MainDispPathPowerOn\n");
	MainDispPathPowerOn(&mtk_crtc->base);

	return 0;
}

static int mtk_drm_load(struct drm_device *dev, unsigned long flags)
{
	struct mtk_drm_private *priv;

	priv = devm_kzalloc(dev->dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	dev->dev_private = priv;
	platform_set_drvdata(dev->platformdev, dev);

	return mtk_drm_kms_init(dev);
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

static int mtk_drm_unload(struct drm_device *dev)
{
	mtk_drm_kms_deinit(dev);
	dev->dev_private = NULL;

	return 0;
}

static int mtk_drm_open(struct drm_device *drm, struct drm_file *filp)
{
	return 0;
}

static void mediatek_drm_preclose(struct drm_device *drm, struct drm_file *file)
{
}

static void mediatek_drm_lastclose(struct drm_device *drm)
{
}

static int mtk_drm_enable_vblank(struct drm_device *drm, int pipe)
{
	return 0;
}

static void mtk_drm_disable_vblank(struct drm_device *drm, int pipe)
{
}

static const struct drm_ioctl_desc mediatek_drm_ioctls[] = {
	DRM_IOCTL_DEF_DRV(MTK_GEM_CREATE, mtk_drm_ioctl_gem_create,
		DRM_UNLOCKED|DRM_AUTH),
	DRM_IOCTL_DEF_DRV(MTK_GEM_MMAP, mtk_drm_ioctl_gem_mmap,
		DRM_UNLOCKED|DRM_AUTH),
};

static const struct vm_operations_struct mediatek_drm_gem_vm_ops = {
	.open = drm_gem_vm_open,
	.close = drm_gem_vm_close,
};

static const struct file_operations mediatek_drm_fops = {
	.owner = THIS_MODULE,
	.open = drm_open,
	.release = drm_release,
	.unlocked_ioctl = drm_ioctl,
	.mmap = mtk_drm_gem_mmap,
	.poll = drm_poll,
	.read = drm_read,
#ifdef CONFIG_COMPAT
	.compat_ioctl = drm_compat_ioctl,
#endif
};

static struct drm_driver mediatek_drm_driver = {
	.driver_features = DRIVER_MODESET | DRIVER_GEM | DRIVER_PRIME,
	.load = mtk_drm_load,
	.unload = mtk_drm_unload,
	.open = mtk_drm_open,
	.preclose = mediatek_drm_preclose,
	.lastclose = mediatek_drm_lastclose,

	.get_vblank_counter = drm_vblank_count,
	.enable_vblank = mtk_drm_enable_vblank,
	.disable_vblank = mtk_drm_disable_vblank,

	.gem_free_object = mtk_drm_gem_free_object,
	.gem_vm_ops = &mediatek_drm_gem_vm_ops,
	.dumb_create = mtk_drm_gem_dumb_create,
	.dumb_map_offset = mtk_drm_gem_dumb_map_offset,
	.dumb_destroy = drm_gem_dumb_destroy,

	.prime_handle_to_fd = drm_gem_prime_handle_to_fd,
	.prime_fd_to_handle = drm_gem_prime_fd_to_handle,
	.gem_prime_export = drm_gem_prime_export, /* FIXME? */
	.gem_prime_import = drm_gem_prime_import, /* FIXME? */

	.ioctls = mediatek_drm_ioctls,
	.num_ioctls = ARRAY_SIZE(mediatek_drm_ioctls),
	.fops = &mediatek_drm_fops,

	.name = DRIVER_NAME,
	.desc = DRIVER_DESC,
	.date = DRIVER_DATE,
	.major = DRIVER_MAJOR,
	.minor = DRIVER_MINOR,
};

static int mtk_drm_probe(struct platform_device *pdev)
{
	return drm_platform_init(&mediatek_drm_driver, pdev);
}

static int mtk_drm_remove(struct platform_device *pdev)
{
	drm_put_dev(platform_get_drvdata(pdev));

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
		/*.pm     = &mtk_pm_ops, */
	},
	/* .id_table = mtk_drm_platform_ids, */
};

static int mediatek_drm_init(void)
{
	int err;

#ifdef CONFIG_DRM_MEDIATEK_IT6151
	pr_info("DBG_YT it6151mipirx_i2c_driver\n");
	err = i2c_add_driver(&it6151mipirx_i2c_driver);
	if (err < 0)
		return err;

	pr_info("DBG_YT it6151dptx_i2c_driver\n");
	err = i2c_add_driver(&it6151dptx_i2c_driver);
	if (err < 0)
		return err;
#endif

#ifdef CONFIG_DRM_MEDIATEK_DSI
	pr_info("DBG_YT mtk_dsi_driver\n");
	err = platform_driver_register(&mtk_dsi_driver);
	if (err < 0)
		return err;
#endif

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

static void mediatek_drm_exit(void)
{
	platform_driver_unregister(&mediatek_drm_platform_driver);
	/* platform_driver_unregister(&mtk_dsi_driver); */
}

module_init(mediatek_drm_init);
module_exit(mediatek_drm_exit);

MODULE_AUTHOR("YT SHEN <yt.shen@mediatek.com>");
MODULE_DESCRIPTION("Mediatek SoC DRM driver");
MODULE_LICENSE("GPL");

