/*
* Copyright (c) 2014 MediaTek Inc.
* Author: Tianping.Fang <tianping.fang@mediatek.com>
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

#include <linux/delay.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/regmap.h>
#include <linux/rtc.h>
#include <linux/irqdomain.h>
#include <linux/spinlock.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/of_address.h>
#include <linux/kernel.h>
#include <linux/io.h>
#include <linux/mfd/mt6397/core.h>
#include <linux/mfd/mt6397/registers.h>

#define RTC_BASE		(0xe000)
#define RTC_BBPU		(0x0000)
#define RTC_WRTGR		(0x003c)
#define RTC_IRQ_EN		(0x0004)
#define RTC_IRQ_STA		(0x0002)

#define RTC_BBPU_CBUSY		(1U << 6)
#define RTC_BBPU_KEY		(0x43 << 8)
#define RTC_BBPU_AUTO		(1U << 3)
#define RTC_IRQ_STA_AL		(1U << 0)
#define RTC_IRQ_STA_LP		(1U << 3)

#define RTC_TC_SEC		(0x000a)
#define RTC_TC_MIN		(0x000c)
#define RTC_TC_HOU		(0x000e)
#define RTC_TC_DOM		(0x0010)
#define RTC_TC_MTH		(0x0014)
#define RTC_TC_YEA		(0x0016)
#define RTC_AL_SEC		(0x0018)
#define RTC_AL_MIN		(0x001a)

#define RTC_IRQ_EN_AL		(1U << 0)
#define RTC_IRQ_EN_ONESHOT	(1U << 2)
#define RTC_IRQ_EN_LP		(1U << 3)
#define RTC_IRQ_EN_ONESHOT_AL	(RTC_IRQ_EN_ONESHOT | RTC_IRQ_EN_AL)

#define RTC_TC_MIN_MASK		(0x003f)
#define RTC_TC_SEC_MASK		(0x003f)
#define RTC_TC_HOU_MASK		(0x001f)
#define RTC_TC_DOM_MASK		(0x001f)
#define RTC_TC_MTH_MASK		(0x000f)
#define RTC_TC_YEA_MASK		(0x007f)

#define RTC_AL_SEC_MASK		(0x003f)
#define RTC_AL_MIN_MASK		(0x003f)
#define RTC_AL_MASK_DOW		(1U << 4)

#define RTC_AL_HOU		(0x001c)
#define RTC_NEW_SPARE_FG_MASK	(0xff00)
#define RTC_NEW_SPARE_FG_SHIFT	8
#define RTC_AL_HOU_MASK		(0x001f)

#define RTC_AL_DOM		(0x001e)
#define RTC_NEW_SPARE1		(0xff00)
#define RTC_AL_DOM_MASK		(0x001f)
#define RTC_AL_MASK		(0x0008)

#define RTC_AL_MTH		(0x0022)
#define RTC_NEW_SPARE3		(0xff00)
#define RTC_AL_MTH_MASK		(0x000f)

#define RTC_AL_YEA		(0x0024)
#define RTC_AL_YEA_MASK		(0x007f)

#define RTC_PDN1		(0x002c)
#define RTC_PDN1_PWRON_TIME	(1U << 7)

#define RTC_PDN2			(0x002e)
#define RTC_PDN2_PWRON_MTH_MASK		(0x000f)
#define RTC_PDN2_PWRON_MTH_SHIFT	0
#define RTC_PDN2_PWRON_ALARM		(1U << 4)
#define RTC_PDN2_UART_MASK		(0x0060)
#define RTC_PDN2_UART_SHIFT		5
#define RTC_PDN2_PWRON_YEA_MASK		(0x7f00)
#define RTC_PDN2_PWRON_YEA_SHIFT	8
#define RTC_PDN2_PWRON_LOGO		(1U << 15)

#define RTC_SPAR0			(0x0030)
#define RTC_SPAR0_PWRON_SEC_MASK	(0x003f)
#define RTC_SPAR0_PWRON_SEC_SHIFT	0

#define RTC_SPAR1			(0x0032)
#define RTC_SPAR1_PWRON_MIN_MASK	(0x003f)
#define RTC_SPAR1_PWRON_MIN_SHIFT	0
#define RTC_SPAR1_PWRON_HOU_MASK	(0x07c0)
#define RTC_SPAR1_PWRON_HOU_SHIFT	6
#define RTC_SPAR1_PWRON_DOM_MASK	(0xf800)
#define RTC_SPAR1_PWRON_DOM_SHIFT	11

#define RTC_MIN_YEAR		1968
#define RTC_BASE_YEAR		1900
#define RTC_NUM_YEARS		128
#define RTC_MIN_YEAR_OFFSET	(RTC_MIN_YEAR - RTC_BASE_YEAR)
#define RTC_RELPWR_WHEN_XRST	1

struct mt6397_rtc {
	struct device		*dev;
	struct rtc_device	*rtc_dev;
	struct mutex		lock;
	int irq;
};
static struct mt6397_chip	*mt6397;

static u16 rtc_read(u32 offset)
{
	u32 rdata = 0;
	u32 addr = (u32)(RTC_BASE + offset);

	regmap_read(mt6397->regmap, addr, &rdata);
	return (u16)rdata;
}

static void rtc_write(u32 offset, u32 data)
{
	u32 addr;

	addr = (u32)(RTC_BASE + offset);
	regmap_write(mt6397->regmap, addr, data);
}

static inline void rtc_busy_wait(void)
{
	while (rtc_read(RTC_BBPU) & RTC_BBPU_CBUSY)
		continue;
}

static void rtc_write_trigger(void)
{
	rtc_write(RTC_WRTGR, 1);
	rtc_busy_wait();
}

static void _rtc_set_lp_irq(void)
{
	u16 irqen;

	irqen = rtc_read(RTC_IRQ_EN) | RTC_IRQ_EN_LP;
	rtc_write(RTC_IRQ_EN, irqen);
	rtc_write_trigger();
}

static void _rtc_get_tick(struct rtc_time *tm)
{
	tm->tm_sec = rtc_read(RTC_TC_SEC);
	tm->tm_min = rtc_read(RTC_TC_MIN);
	tm->tm_hour = rtc_read(RTC_TC_HOU);
	tm->tm_mday = rtc_read(RTC_TC_DOM);
	tm->tm_mon = rtc_read(RTC_TC_MTH);
	tm->tm_year = rtc_read(RTC_TC_YEA);
}

static void _mtk_rtc_read_tick_time(struct rtc_time *tm)
{
	_rtc_get_tick(tm);
	while (rtc_read(RTC_TC_SEC) < tm->tm_sec)
		_rtc_get_tick(tm);
}

static void _mtk_rtc_set_tick_time(struct rtc_time *tm)
{
	rtc_write(RTC_TC_YEA, tm->tm_year);
	rtc_write(RTC_TC_MTH, tm->tm_mon);
	rtc_write(RTC_TC_DOM, tm->tm_mday);
	rtc_write(RTC_TC_HOU, tm->tm_hour);
	rtc_write(RTC_TC_MIN, tm->tm_min);
	rtc_write(RTC_TC_SEC, tm->tm_sec);
	rtc_write_trigger();
}

static void _mtk_rtc_read_alarm_time(struct rtc_time *tm,
					struct rtc_wkalrm *alm)
{
	u16 irqen, pdn2;

	irqen = rtc_read(RTC_IRQ_EN);
	tm->tm_sec  = rtc_read(RTC_AL_SEC);
	tm->tm_min  = rtc_read(RTC_AL_MIN);
	tm->tm_hour = rtc_read(RTC_AL_HOU) & RTC_AL_HOU_MASK;
	tm->tm_mday = rtc_read(RTC_AL_DOM) & RTC_AL_DOM_MASK;
	tm->tm_mon  = rtc_read(RTC_AL_MTH) & RTC_AL_MTH_MASK;
	tm->tm_year = rtc_read(RTC_AL_YEA);
	pdn2 = rtc_read(RTC_PDN2);
	alm->enabled = !!(irqen & RTC_IRQ_EN_AL);
	alm->pending = !!(pdn2 & RTC_PDN2_PWRON_ALARM);
}

static void _mtk_rtc_set_alarm_time(struct rtc_time *tm)
{
	u16 irqen;

	rtc_write(RTC_AL_YEA, tm->tm_year);
	rtc_write(RTC_AL_MTH, (rtc_read(RTC_AL_MTH) &
			(RTC_NEW_SPARE3))|tm->tm_mon);
	rtc_write(RTC_AL_DOM, (rtc_read(RTC_AL_DOM) &
			(RTC_NEW_SPARE1))|tm->tm_mday);
	rtc_write(RTC_AL_HOU, (rtc_read(RTC_AL_HOU) &
			(RTC_NEW_SPARE_FG_MASK))|tm->tm_hour);
	rtc_write(RTC_AL_MIN, tm->tm_min);
	rtc_write(RTC_AL_SEC, tm->tm_sec);
	rtc_write(RTC_AL_MASK, RTC_AL_MASK_DOW);
	rtc_write_trigger();
	irqen = rtc_read(RTC_IRQ_EN) | RTC_IRQ_EN_ONESHOT_AL;
	rtc_write(RTC_IRQ_EN, irqen);
	rtc_write_trigger();

}

static irqreturn_t rtc_irq_handler_thread(int irq, void *data)
{
	struct mt6397_rtc *rtc = data;
	u16 irqsta;

	mutex_lock(&rtc->lock);
	irqsta = rtc_read(RTC_IRQ_STA);
	mutex_unlock(&rtc->lock);
	if (unlikely(!(irqsta & RTC_IRQ_STA_AL))) {
		if (irqsta & RTC_IRQ_STA_LP) {
			dev_err(rtc->dev, "32K was stop!!!\n");
			return IRQ_HANDLED;
		}
	}
	rtc_update_irq(rtc->rtc_dev, 1, RTC_IRQF | RTC_AF);
	return IRQ_HANDLED;
}

static int mtk_rtc_read_time(struct device *dev, struct rtc_time *tm)
{
	unsigned long time;
	struct mt6397_rtc *rtc = dev_get_drvdata(dev);

	mutex_lock(&rtc->lock);
	_mtk_rtc_read_tick_time(tm);
	mutex_unlock(&rtc->lock);

	tm->tm_year += RTC_MIN_YEAR_OFFSET;
	tm->tm_mon--;
	rtc_tm_to_time(tm, &time);

	tm->tm_wday = (time/86400 + 4) % 7;

	return 0;
}

static int mtk_rtc_set_time(struct device *dev, struct rtc_time *tm)
{
	unsigned long time;
	struct mt6397_rtc *rtc = dev_get_drvdata(dev);

	rtc_tm_to_time(tm, &time);
	if (time > (unsigned long)LONG_MAX)
		return -EINVAL;

	tm->tm_year -= RTC_MIN_YEAR_OFFSET;
	tm->tm_mon++;
	mutex_lock(&rtc->lock);
	_mtk_rtc_set_tick_time(tm);
	mutex_unlock(&rtc->lock);

	return 0;
}

static int mtk_rtc_read_alarm(struct device *dev, struct rtc_wkalrm *alm)
{
	struct rtc_time *tm = &alm->time;
	struct mt6397_rtc *rtc = dev_get_drvdata(dev);

	mutex_lock(&rtc->lock);
	_mtk_rtc_read_alarm_time(tm, alm);
	mutex_unlock(&rtc->lock);

	tm->tm_year += RTC_MIN_YEAR_OFFSET;
	tm->tm_mon--;

	return 0;
}

static int mtk_rtc_set_alarm(struct device *dev, struct rtc_wkalrm *alm)
{
	unsigned long time;
	struct rtc_time *tm = &alm->time;
	struct mt6397_rtc *rtc = dev_get_drvdata(dev);

	rtc_tm_to_time(tm, &time);
	if (time > (unsigned long)LONG_MAX)
		return -EINVAL;

	tm->tm_year -= RTC_MIN_YEAR_OFFSET;
	tm->tm_mon++;

	mutex_lock(&rtc->lock);
	if (alm->enabled)
		_mtk_rtc_set_alarm_time(tm);
	mutex_unlock(&rtc->lock);

	return 0;
}

static struct rtc_class_ops mtk_rtc_ops = {
	.read_time  = mtk_rtc_read_time,
	.set_time   = mtk_rtc_set_time,
	.read_alarm = mtk_rtc_read_alarm,
	.set_alarm  = mtk_rtc_set_alarm,
};

static int mtk_rtc_probe(struct platform_device *pdev)
{
	struct mt6397_chip *mt6397_chip = dev_get_drvdata(pdev->dev.parent);
	struct mt6397_rtc *rtc;
	int ret = 0;

	rtc = kzalloc(sizeof(struct mt6397_rtc), GFP_KERNEL);
	if (!rtc)
		return -ENOMEM;

	rtc->dev = &pdev->dev;
	mutex_init(&rtc->lock);

	platform_set_drvdata(pdev, rtc);
	mt6397 = dev_get_drvdata(pdev->dev.parent);

	rtc->rtc_dev = rtc_device_register("mt6397-rtc", &pdev->dev,
				&mtk_rtc_ops, THIS_MODULE);
	if (IS_ERR(rtc)) {
		dev_err(&pdev->dev, "register rtc device failed\n");
		goto out_rtc;
	}
	if (!mt6397_chip->irq_domain)
		dev_warn(&pdev->dev, "irq domain is NULL\n");

	rtc->irq = irq_create_mapping(mt6397_chip->irq_domain,
				RG_INT_STATUS_RTC);
	if (!rtc->irq)
		dev_warn(&pdev->dev, "Failed to map alarm IRQ\n");

	ret = devm_request_threaded_irq(&pdev->dev, rtc->irq, NULL,
			rtc_irq_handler_thread,
			IRQF_TRIGGER_HIGH | IRQF_ONESHOT,
			"mt6397-rtc", rtc);
	if (ret < 0)
		dev_err(&pdev->dev, "Failed to request alarm IRQ: %d: %d\n",
			rtc->irq, ret);

	device_init_wakeup(&pdev->dev, 1);
	_rtc_set_lp_irq();

	return 0;

out_rtc:
	platform_set_drvdata(pdev, NULL);
	kfree(rtc);
	return ret;

}

static int mtk_rtc_remove(struct platform_device *pdev)
{
	struct mt6397_rtc *rtc = platform_get_drvdata(pdev);

	if (rtc) {
		rtc_device_unregister(rtc->rtc_dev);
		kfree(rtc);
	}
	return 0;
}

static const struct platform_device_id mt6397_rtc_id[] = {
	{
		.name = "mt6397-rtc",
		.driver_data = 0,
	},
	{ },
};

static struct platform_driver mtk_rtc_pdrv = {
	.driver = {
		.name = "mt6397-rtc",
		.owner = THIS_MODULE,
	},
	.probe	= mtk_rtc_probe,
	.remove = mtk_rtc_remove,
	.id_table = mt6397_rtc_id,
};

static int __init mt6397_rtc_init(void)
{
	return platform_driver_register(&mtk_rtc_pdrv);
}

static void __exit mt6397_rtc_exit(void)
{
	platform_driver_unregister(&mtk_rtc_pdrv);
}

subsys_initcall(mt6397_rtc_init);
module_exit(mt6397_rtc_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Tianping Fang <tianping.fang@mediatek.com>");
MODULE_DESCRIPTION("RTC Driver for MediaTek MT6397 PMIC");
MODULE_ALIAS("platform:mt6397-rtc");
