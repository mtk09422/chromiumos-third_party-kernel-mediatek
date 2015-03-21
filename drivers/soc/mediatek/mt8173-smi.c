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
#include <linux/io.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/mm.h>

#define SMI_LARB_MMU_EN                 (0xf00)
#define F_SMI_MMU_EN(port)              (1 << (port))

struct mtk_smi_larb {
	void __iomem *larb_base;
	spinlock_t portlock;
	struct clk *larb_clk[3];/* each larb has 3 clk at most */
};

static const char * const mtk_smi_clk_name[] = {
	"larb_sub0", "larb_sub1", "larb_sub2"
};

static const struct of_device_id mtk_smi_of_ids[] = {
	{ .compatible = "mediatek,mt8173-smi-larb",
	},
	{}
};

int mtk_smi_larb_get(struct device *larbdev)
{
	struct mtk_smi_larb *larbpriv = dev_get_drvdata(larbdev);
	int i, ret = 0;

	for (i = 0; i < 3; i++)
		if (larbpriv->larb_clk[i]) {
			ret = clk_prepare_enable(larbpriv->larb_clk[i]);
			if (ret) {
				dev_err(larbdev,
					"failed to enable larbclk%d:%d\n",
					i, ret);
				break;
			}
		}
	return ret;
}

void mtk_smi_larb_put(struct device *larbdev)
{
	struct mtk_smi_larb *larbpriv = dev_get_drvdata(larbdev);
	int i;

	for (i = 2; i >= 0; i--)
		if (larbpriv->larb_clk[i])
			clk_disable_unprepare(larbpriv->larb_clk[i]);
}

int mtk_smi_config_port(struct device *larbdev,	unsigned int larbportid)
{
	struct mtk_smi_larb *larbpriv = dev_get_drvdata(larbdev);
	unsigned long flags;
	int ret;
	u32 reg;

	ret = mtk_smi_larb_get(larbdev);
	if (ret)
		return ret;

	spin_lock_irqsave(&larbpriv->portlock, flags);
	reg = readl(larbpriv->larb_base + SMI_LARB_MMU_EN);
	reg &= ~F_SMI_MMU_EN(larbportid);
	reg |= F_SMI_MMU_EN(larbportid);
	writel(reg, larbpriv->larb_base + SMI_LARB_MMU_EN);
	spin_unlock_irqrestore(&larbpriv->portlock, flags);

	mtk_smi_larb_put(larbdev);

	return 0;
}

static int mtk_smi_probe(struct platform_device *pdev)
{
	struct mtk_smi_larb *larbpriv;
	struct resource *res;
	struct device *dev = &pdev->dev;
	unsigned int i;

	larbpriv = devm_kzalloc(dev, sizeof(struct mtk_smi_larb), GFP_KERNEL);
	if (!larbpriv)
		return -ENOMEM;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	larbpriv->larb_base = devm_ioremap_resource(dev, res);
	if (IS_ERR(larbpriv->larb_base)) {
		dev_err(dev, "larbbase %p err\n", larbpriv->larb_base);
		return PTR_ERR(larbpriv->larb_base);
	}

	for (i = 0; i < 3; i++) {
		larbpriv->larb_clk[i] = devm_clk_get(dev, mtk_smi_clk_name[i]);

		if (IS_ERR(larbpriv->larb_clk[i])) {
			if (i == 2) {/* some larb may have only 2 clock */
				larbpriv->larb_clk[i] = NULL;
			} else {
				dev_err(dev, "clock-%d err: %p\n", i,
					larbpriv->larb_clk[i]);
				return PTR_ERR(larbpriv->larb_clk[i]);
			}
		}
	}
	spin_lock_init(&larbpriv->portlock);
	dev_set_drvdata(dev, larbpriv);
	return 0;
}

static int mtk_smi_remove(struct platform_device *pdev)
{
	return 0;
}

static struct platform_driver mtk_smi_driver = {
	.probe	= mtk_smi_probe,
	.remove	= mtk_smi_remove,
	.driver	= {
		.name = "mtksmi",
		.of_match_table = mtk_smi_of_ids,
	}
};

static int __init mtk_smi_init(void)
{
	return platform_driver_register(&mtk_smi_driver);
}

subsys_initcall(mtk_smi_init);

