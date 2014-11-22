/*
* Copyright (c) 2014 MediaTek Inc.
* Author: Ming.Huang <ming.huang@mediatek.com>
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
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/types.h>
#include <linux/miscdevice.h>
#include <linux/watchdog.h>
#include <linux/fs.h>
#include <linux/reboot.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/irq.h>
#include <linux/smp.h>
#include <asm/smp_twd.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/notifier.h>

#define WATCHDOG_NAME          "Mt65xx_Watchdog"

/*WDT_MODE*/
#define MT65XX_WDT_MODE        (0x0000)
#define MT65XX_WDT_MODE_KEY    (0x22000000)
#define MT65XX_WDT_MODE_ENABLE (0x25)

/*WDT_LENGTH
* Key = BitField  4:0 = 0x08
* TimeOut = BitField 15:5
* 1 tick means 512 * T32.768K = 15.6 ms --> 1s = 1000/15.6  = 64 ticks
*/
#define MT65XX_WDT_LENGTH      (0x0004)
#define MT65XX_WDT_LENGTH_KEY  (0x0008)
#define MT65XX_WDT_LENGTH_FIELD_OFFSET  5
#define MT65XX_WDT_LENGTH_SECOND_TO_TICKS  64

/*WDT_RESTART*/
#define MT65XX_WDT_RESTART     (0x0008)
#define MT65XX_WDT_RESTART_KEY (0x1971)

/* in seconds */
#define MT65XX_MIN_TIMEOUT     1
#define MT65XX_MAX_TIMEOUT     31


/**
 * struct mt65xx_wdt: mt65xx wdt device structure
 * @wdt_device: instance of struct watchdog_device
 * @lock: spin lock protecting dev structure and io access
 * @reg_base: base address of wdt
 * @timeout: current programmed timeout
 */
struct mt65xx_wdt {
	struct watchdog_device    wdd;
	struct notifier_block    wdt_notifier;
	void __iomem              *reg_base;
	unsigned int              timeout;
};

static bool nowayout = WATCHDOG_NOWAYOUT;
module_param(nowayout, bool, 0);
MODULE_PARM_DESC(nowayout, "Watchdog cannot be stopped once started (default="
			__MODULE_STRING(WATCHDOG_NOWAYOUT) ")");

static int wdt_enable(struct watchdog_device *wdd)
{
	struct mt65xx_wdt *wdt = watchdog_get_drvdata(wdd);
	unsigned int tmp;

	tmp = MT65XX_WDT_MODE_KEY | MT65XX_WDT_MODE_ENABLE;
	writel(tmp, wdt->reg_base + MT65XX_WDT_MODE);

	return 0;
}

static int wdt_disable(struct watchdog_device *wdd)
{
	struct mt65xx_wdt *wdt = watchdog_get_drvdata(wdd);
	unsigned int tmp;

	tmp = MT65XX_WDT_MODE_KEY;
	writel(tmp, wdt->reg_base + MT65XX_WDT_MODE);

	return 0;
}

static int wdt_ping(struct watchdog_device *wdd)
{
	struct mt65xx_wdt *wdt = watchdog_get_drvdata(wdd);

	writel(MT65XX_WDT_RESTART_KEY, wdt->reg_base+MT65XX_WDT_RESTART);

	return 0;
}

static int wdt_set_timeout(struct watchdog_device *wdd, unsigned int timeout)
{
	unsigned int ticks, write_val;
	struct mt65xx_wdt *wdt = watchdog_get_drvdata(wdd);

	wdd->timeout = timeout;

	ticks = wdd->timeout * MT65XX_WDT_LENGTH_SECOND_TO_TICKS;
	ticks = ticks << MT65XX_WDT_LENGTH_FIELD_OFFSET;
	write_val = ticks | MT65XX_WDT_LENGTH_KEY;

	writel(write_val, wdt->reg_base + MT65XX_WDT_LENGTH);

	return 0;
}

static const struct watchdog_ops wdt_ops = {
	.owner		= THIS_MODULE,
	.start		= wdt_enable,
	.stop		= wdt_disable,
	.ping		= wdt_ping,
	.set_timeout	= wdt_set_timeout,
};

/* Notifier for system down */
static int wdt_notify_sys(struct notifier_block *this, unsigned long code,
			  void *unused)
{
	struct mt65xx_wdt *wdt = container_of(this, struct mt65xx_wdt,
								wdt_notifier);

	if (code == SYS_DOWN || code == SYS_HALT)
		wdt_disable(&wdt->wdd);

	return NOTIFY_DONE;
}

static const struct watchdog_info wdt_info = {
	.options = WDIOF_SETTIMEOUT | WDIOF_MAGICCLOSE | WDIOF_KEEPALIVEPING,
	.identity = WATCHDOG_NAME,
};

static int mt65xx_wdt_probe(struct platform_device *pdev)
{
	struct mt65xx_wdt *wdt;
	struct resource *res;
	int ret;
	int rc;

	wdt = devm_kzalloc(&pdev->dev, sizeof(*wdt), GFP_KERNEL);
	if (NULL == wdt)
		return -ENOMEM;

	/* initialize mt65xx_wdt structure */
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	wdt->reg_base = devm_ioremap_resource(&pdev->dev, res);
	if (NULL == wdt->reg_base) {
		dev_err(&pdev->dev, "wdt->reg_base== NULL\n");
		ret = -ENOMEM;
	}

	wdt->wdd.info = &wdt_info;
	wdt->wdd.ops = &wdt_ops;
	wdt->wdd.timeout = MT65XX_MAX_TIMEOUT;
	wdt->wdd.min_timeout = MT65XX_MIN_TIMEOUT;
	wdt->wdd.max_timeout = MT65XX_MAX_TIMEOUT;

	watchdog_set_nowayout(&wdt->wdd, nowayout);
	watchdog_set_drvdata(&wdt->wdd, wdt);

	ret = watchdog_register_device(&wdt->wdd);
	if (ret)
		dev_err(&pdev->dev, "watchdog_register_device() failed: %d\n",
				ret);

	platform_set_drvdata(pdev, wdt);
	wdt_disable(&wdt->wdd);

	wdt->wdt_notifier.notifier_call = wdt_notify_sys;
	rc = register_reboot_notifier(&wdt->wdt_notifier);
	if (rc)
		dev_err(&pdev->dev, "cannot register reboot notifier (err=%d)\n",
				rc);

	return ret;
}

static int mt65xx_wdt_remove(struct platform_device *pdev)
{
	struct mt65xx_wdt *wdt = platform_get_drvdata(pdev);

	if (wdt) {
		platform_set_drvdata(pdev, NULL);
		iounmap(wdt->reg_base);
		kfree(wdt);
	}
	unregister_reboot_notifier(&wdt->wdt_notifier);
	watchdog_unregister_device(&wdt->wdd);

	return 0;
}

static struct of_device_id mt65xx_wdt_match_data[] = {
	{
		.compatible = "mediatek,mt6577-watchdog",
	}
};

static struct platform_driver mt65xx_wdt_driver = {
	.probe = mt65xx_wdt_probe,
	.remove = mt65xx_wdt_remove,
	.driver = {
		.owner = THIS_MODULE,
		.name = "mt65xx_wdt",
		.of_match_table = mt65xx_wdt_match_data,
	},
};

module_platform_driver(mt65xx_wdt_driver);

MODULE_AUTHOR("Ming Huang <ming.huang@mediatek.com>");
MODULE_DESCRIPTION("mt65xx Watchdog");
MODULE_LICENSE("GPL");
