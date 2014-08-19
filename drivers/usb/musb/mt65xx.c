/*
 * Copyright (c) 2014 MediaTek Inc.
 * Author: Yonglong.Wu <yonglong.wu@mediatek.com>
 *
 * MT65XX USB 2.0 Host mode controller.
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

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/io.h>
#include <linux/i2c.h>
#include "musb_core.h"
#include "mt65xx.h"
#include <linux/dma-mapping.h>
#include <linux/platform_device.h>
#include "musbhsdma.h"
#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/regulator/consumer.h>
#include <linux/usb/usb_phy_generic.h>
#endif
#include <linux/jiffies.h>
#include <linux/delay.h>
#include <asm/io.h>
#include <linux/spinlock.h>
#include <linux/clk.h>

struct musb *mtk_musb = NULL;
static DEFINE_SEMAPHORE(power_clock_lock);
static bool platform_init_first = true;
bool mt65xx_musb_power = false;
static struct delayed_work id_pin_work;

#ifdef CONFIG_OF
static unsigned long usb_phy_base;
/* main clock for usb port0 */
static struct clk *usb_clk_main;
static struct clk *univpll_clk;
static struct regulator *usb_power;
#define VOL_3300	(3300000)
#define U2PHYDTM1  ((usb_phy_base)+0x800 + 0x6c)
static const struct of_device_id apusb_of_ids[] = {
	{.compatible = "mediatek,mt6577-usb0",},
	{},
};
MODULE_DEVICE_TABLE(of, apusb_of_ids);
#endif

static struct timer_list musb_idle_timer;

#define FRA (48)
#define PARA (22)

static DEFINE_SPINLOCK(musb_reg_clock_lock);

static inline u8 USBPHY_READ8(unsigned offset)
{
	return musb_readb((void __iomem *)usb_phy_base + 0x800,
					(unsigned)offset);
}

static inline void USBPHY_WRITE8(unsigned offset, u8 value)
{
	musb_writeb((void __iomem *)usb_phy_base + 0x800, offset, value);
}

static inline void USBPHY_SET8(unsigned offset, u8 mask)
{
	USBPHY_WRITE8(offset, USBPHY_READ8(offset) | mask);
}

static inline void USBPHY_CLR8(unsigned offset, u8 mask)
{
	USBPHY_WRITE8(offset, USBPHY_READ8(offset) & ~mask);
}

static inline u16 USBPHY_READ16(unsigned offset)
{
	return musb_readw((void __iomem *)usb_phy_base + 0x800, offset);
}

static inline void USBPHY_WRITE16(unsigned offset, u16 value)
{
	musb_writew((void __iomem *)usb_phy_base + 0x800, offset, value);
}

static inline void USBPHY_SET16(unsigned offset, u16 mask)
{
	USBPHY_WRITE16(offset, USBPHY_READ16(offset) | mask);
}

static inline void USBPHY_CLR16(unsigned offset, u16 mask)
{
	USBPHY_WRITE16(offset, USBPHY_READ16(offset) & ~mask);
}

static inline u32 USBPHY_READ32(unsigned offset)
{
	return musb_readl((void __iomem *)usb_phy_base + 0x800, offset);
}

static inline void USBPHY_WRITE32(unsigned offset, u32 value)
{
	musb_writel((void __iomem *)usb_phy_base + 0x800, offset, value);
}

static inline void USBPHY_SET32(unsigned offset, u32 mask)
{
	USBPHY_WRITE32(offset, USBPHY_READ32(offset) | mask);
}

static inline void USBPHY_CLR32(unsigned offset, u32 mask)
{
	USBPHY_WRITE32(offset, USBPHY_READ32(offset) & ~mask);
}

bool mt65xx_musb_enable_clock(bool enable)
{
	static int count;
	unsigned long flags;

	spin_lock_irqsave(&musb_reg_clock_lock, flags);

	if (enable && count == 0) {
		clk_prepare(usb_clk_main);
		clk_enable(usb_clk_main);
	} else if (!enable && count == 1) {
		clk_disable(usb_clk_main);
		clk_unprepare(usb_clk_main);
	}

	if (enable)
		count++;
	else
		count = (count == 0) ? 0 : (count - 1);

	spin_unlock_irqrestore(&musb_reg_clock_lock, flags);

	pr_debug("enable(%d), count(%d)\n", enable, count);
	return 1;
}

static void mt65xx_musb_hs_slew_rate_cal(void)
{
	unsigned long data;
	unsigned long x;
	unsigned char value;
	unsigned long start_time, timeout;
	unsigned int timeout_flag = 0;
	/* 4 s1:enable usb ring oscillator. */
	USBPHY_WRITE8(0x15, 0x80);

	/* 4 s2:wait 1us. */
	udelay(1);

	/* 4 s3:enable free run clock */
	USBPHY_WRITE8(0xf00 - 0x800 + 0x11, 0x01);
	/* 4 s4:setting cyclecnt. */
	USBPHY_WRITE8(0xf00 - 0x800 + 0x01, 0x04);

	/* make sure USB PORT1 */
	USBPHY_CLR8(0xf00 - 0x800 + 0x03, 0x0C);
	/* 4 s5:enable frequency meter */
	USBPHY_SET8(0xf00 - 0x800 + 0x03, 0x01);

	/* 4 s6:wait for frequency valid. */
	start_time = jiffies;
	timeout = jiffies + 3 * HZ;

	while (!(USBPHY_READ8(0xf00 - 0x800 + 0x10) & 0x1)) {
		if (time_after(jiffies, timeout)) {
			timeout_flag = 1;
			break;
		}
	}

	/* 4 s7: read result. */
	if (timeout_flag) {
		pr_info("[USBPHY] Slew Rate Calibration: Timeout\n");
		value = 0x4;
	} else {
		data = USBPHY_READ32(0xf00 - 0x800 + 0x0c);
		x = ((1024 * FRA * PARA) / data);
		value = (unsigned char)(x / 1000);
		if ((x - value * 1000) / 100 >= 5)
			value += 1;
	}

	/* 4 s8: disable Frequency and run clock. */
	/* disable frequency meter */
	USBPHY_CLR8(0xf00 - 0x800 + 0x03, 0x01);
	/* disable free run clock */
	USBPHY_CLR8(0xf00 - 0x800 + 0x11, 0x01);
	/* 4 s9: */
	USBPHY_WRITE8(0x15, value << 4);

	/* 4 s10:disable usb ring oscillator. */
	USBPHY_CLR8(0x15, 0x80);
}

static void mt65xx_musb_phy_savecurrent_internal(void)
{
	/* 4 1. swtich to USB function. system register,
	 * force ip into usb mode. */
	USBPHY_CLR8(0x6b, 0x04);
	USBPHY_CLR8(0x6e, 0x01);

	/* 4 2. release force suspendm. */
	USBPHY_CLR8(0x6a, 0x04);
	/* 4 3. RG_DPPULLDOWN./RG_DMPULLDOWN. */
	USBPHY_SET8(0x68, 0xc0);
	/* 4 4. RG_XCVRSEL[1:0] =2'b01. */
	USBPHY_CLR8(0x68, 0x30);

	USBPHY_SET8(0x68, 0x10);
	/* 4 5. RG_TERMSEL = 1'b1 */
	USBPHY_SET8(0x68, 0x04);
	/* 4 6. RG_DATAIN[3:0]=4'b0000 */
	USBPHY_CLR8(0x69, 0x3c);
	/* 4 7.force_dp_pulldown, force_dm_pulldown,
	 * force_xcversel,force_termsel. */
	USBPHY_SET8(0x6a, 0xba);

	/* 4 8.RG_USB20_BC11_SW_EN 1'b0 */
	USBPHY_CLR8(0x1a, 0x80);
	/* 4 9.RG_USB20_OTG_VBUSSCMP_EN 1'b0 */
	USBPHY_CLR8(0x1a, 0x10);
	/* 4 10. delay 800us. */
	udelay(800);
	/* 4 11. rg_usb20_pll_stable = 1 */
	USBPHY_SET8(0x63, 0x02);

/* ALPS00427972, implement the analog register formula */
/* pr_info("%s: USBPHY_READ8(0x05) = 0x%x\n", __func__, USBPHY_READ8(0x05)); */
/* pr_info("%s: USBPHY_READ8(0x07) = 0x%x\n", __func__, USBPHY_READ8(0x07)); */
/* ALPS00427972, implement the analog register formula */

	udelay(1);
	/* 4 12.  force suspendm = 1. */
	USBPHY_SET8(0x6a, 0x04);
	/* 4 13.  wait 1us */
	udelay(1);
}

void mt65xx_musb_phy_savecurrent(void)
{

	mt65xx_musb_phy_savecurrent_internal();
	/* 4 14. turn off internal 48Mhz PLL. */
	/* mt65xx_musb_enable_clock(false); */
	/* pr_info("usb save current success\n"); */
}

void mt65xx_usb_phy_recover(void)
{
	/* 4 1. turn on USB reference clock. */
	mt65xx_musb_enable_clock(true);
	/* 4 2. wait 50 usec. */
	udelay(50);

	/* clean PUPD_BIST_EN */
	/* PUPD_BIST_EN = 1'b0 */
	/* PMIC will use it to detect charger type */
	USBPHY_CLR8(0x1d, 0x10);

	/* 4 3. force_uart_en = 1'b0 */
	USBPHY_CLR8(0x6b, 0x04);
	/* 4 4. RG_UART_EN = 1'b0 */
	USBPHY_CLR8(0x6e, 0x1);
	/* 4 5. force_uart_en = 1'b0 */
	USBPHY_CLR8(0x6a, 0x04);

	/* 4 6. RG_DPPULLDOWN = 1'b0 */
	USBPHY_CLR8(0x68, 0x40);
	/* 4 7. RG_DMPULLDOWN = 1'b0 */
	USBPHY_CLR8(0x68, 0x80);
	/* 4 8. RG_XCVRSEL = 2'b00 */
	USBPHY_CLR8(0x68, 0x30);
	/* 4 9. RG_TERMSEL = 1'b0 */
	USBPHY_CLR8(0x68, 0x04);
	/* 4 10. RG_DATAIN[3:0] = 4'b0000 */
	USBPHY_CLR8(0x69, 0x3c);

	/* 4 11. force_dp_pulldown = 1b'0 */
	USBPHY_CLR8(0x6a, 0x10);
	/* 4 12. force_dm_pulldown = 1b'0 */
	USBPHY_CLR8(0x6a, 0x20);
	/* 4 13. force_xcversel = 1b'0 */
	USBPHY_CLR8(0x6a, 0x08);
	/* 4 14. force_termsel = 1b'0 */
	USBPHY_CLR8(0x6a, 0x02);
	/* 4 15. force_datain = 1b'0 */
	USBPHY_CLR8(0x6a, 0x80);

	/* 4 16. RG_USB20_BC11_SW_EN 1'b0 */
	USBPHY_CLR8(0x1a, 0x80);
	/* 4 17. RG_USB20_OTG_VBUSSCMP_EN 1'b1 */
	USBPHY_SET8(0x1a, 0x10);

	/* 4 18. wait 800 usec. */
	udelay(800);

	mt65xx_musb_hs_slew_rate_cal();
	/* optimized USB electrical characteristic for slimport Anx3618 */
	USBPHY_SET8(0x05, 0x77);
	pr_info("usb recovery success\n");

	/* receiver level adjustment  */
	USBPHY_CLR8(0x18, 0x0f);
	USBPHY_SET8(0x18, 0x03);
}

static void mt65xx_musb_set_vbus(struct musb *musb, int is_on)
{
	pr_info("mt65xx_usb20_vbus++,is_on=%d\r\n", is_on);
	if (is_on) {
		/* power on VBUS, implement later... */
		/*tbl_charger_otg_vbus((work_busy(id_pin_work.work) << 8)
		   | 1); */
	} else {
		/* power off VBUS, implement later... */
		/*tbl_charger_otg_vbus((work_busy(id_pin_work.work) << 8)
		   | 0); */
	}
}

static int mt65xx_musb_get_vbus_status(struct musb *musb)
{
	int ret = 0;

	if ((musb_readb(musb->mregs, MUSB_DEVCTL) & MUSB_DEVCTL_VBUS)
	    != MUSB_DEVCTL_VBUS) {
		ret = 1;
	} else {
		pr_info("VBUS error, devctl=%x\n",
			musb_readb(musb->mregs, MUSB_DEVCTL));
	}
	pr_info("vbus ready = %d\n", ret);
	return ret;
}

static bool mt65xx_musb_is_host(void)
{
	u8 devctl = 0;

	pr_info("will mask PMIC charger detection\n");
	/* bat_detect_set_usb_host_mode(true); */
	musb_platform_enable(mtk_musb);
	devctl = musb_readb(mtk_musb->mregs, MUSB_DEVCTL);
	pr_info("devctl = %x before end session\n", devctl);
	/* this will cause A-device change back
	 * to B-device after A-cable plug out */
	devctl &= ~MUSB_DEVCTL_SESSION;
	musb_writeb(mtk_musb->mregs, MUSB_DEVCTL, devctl);
	msleep(20);

	devctl = musb_readb(mtk_musb->mregs, MUSB_DEVCTL);
	pr_info("devctl = %x before set session\n", devctl);

	devctl |= MUSB_DEVCTL_SESSION;
	musb_writeb(mtk_musb->mregs, MUSB_DEVCTL, devctl);
	msleep(55);
	devctl = musb_readb(mtk_musb->mregs, MUSB_DEVCTL);
	pr_info("devclt = %x\n", devctl);

	if (devctl & MUSB_DEVCTL_BDEVICE) {
		pr_info("will unmask PMIC charger detection\n");
		/* bat_detect_set_usb_host_mode(false); */
		return false;
	} else {
		return true;
	}
}

static void mt65xx_musb_switch_int_to_device(struct musb *musb)
{
	musb_writel(musb->mregs, USB_L1INTP, 0);
	musb_writel(musb->mregs, USB_L1INTM,
		IDDIG_INT_STATUS | musb_readl(musb->mregs, USB_L1INTM));

	pr_info("mt65xx_musb_switch_int_to_device is done\n");
}

static void mt65xx_musb_switch_int_to_host(struct musb *musb)
{
	musb_writel(musb->mregs, USB_L1INTP, IDDIG_INT_STATUS);
	musb_writel(musb->mregs, USB_L1INTM,
		IDDIG_INT_STATUS | musb_readl(musb->mregs, USB_L1INTM));
	pr_info("mt65xx_musb_switch_int_to_host is done\n");
}

static void mt65xx_musb_id_pin_work(struct work_struct *data)
{
	u8 devctl = 0;

	pr_info("work start, is_host=%d\n", mtk_musb->is_host);
	mtk_musb->is_host = mt65xx_musb_is_host();
	pr_info("musb is as %s\n", mtk_musb->is_host ? "host" : "device");

	if (mtk_musb->is_host) {
		/* setup fifo for host mode
		 * merge into musb_core.c */
		/* ep_config_from_table_for_host(mtk_musb); */
		musb_platform_set_vbus(mtk_musb, 1);
		musb_start(mtk_musb);
		/* wait VBUS ready */
		msleep(100);
		devctl = musb_readb(mtk_musb->mregs, MUSB_DEVCTL);
		musb_writeb(mtk_musb->mregs, MUSB_DEVCTL,
					(devctl&(~MUSB_DEVCTL_SESSION)));
		/* USB MAC OFF*/
		/* VBUSVALID=0, AVALID=0, BVALID=0, SESSEND=1, IDDIG=X */
		USBPHY_SET8(0x6c, 0x10);
		USBPHY_CLR8(0x6c, 0x2e);
		USBPHY_SET8(0x6d, 0x3e);
		pr_info("force PHY to idle, 0x6d=%x, 0x6c=%x\n",
				USBPHY_READ8(0x6d), USBPHY_READ8(0x6c));
		msleep(20);
		devctl = musb_readb(mtk_musb->mregs, MUSB_DEVCTL);
		musb_writeb(mtk_musb->mregs, MUSB_DEVCTL,
					(devctl | MUSB_DEVCTL_SESSION));
		USBPHY_CLR8(0x6c, 0x10);
		USBPHY_SET8(0x6c, 0x2c);
		USBPHY_SET8(0x6d, 0x3e);
		pr_info("force PHY to host mode, 0x6d=%x, 0x6c=%x\n",
				USBPHY_READ8(0x6d), USBPHY_READ8(0x6c));
		MUSB_HST_MODE(mtk_musb);
		mt65xx_musb_switch_int_to_device(mtk_musb);
	} else {
		pr_info("devctl is %x\n",
				musb_readb(mtk_musb->mregs, MUSB_DEVCTL));
		musb_writeb(mtk_musb->mregs, MUSB_DEVCTL, 0);
		musb_platform_set_vbus(mtk_musb, 0);

		/* USB MAC OFF*/
		/* VBUSVALID=0, AVALID=0, BVALID=0, SESSEND=1, IDDIG=X */
		USBPHY_SET8(0x6c, 0x10);
		USBPHY_CLR8(0x6c, 0x2e);
		USBPHY_SET8(0x6d, 0x3e);
		pr_info("force PHY to idle, 0x6d=%x, 0x6c=%x\n",
				USBPHY_READ8(0x6d), USBPHY_READ8(0x6c));
		musb_stop(mtk_musb);
		/* ALPS00849138 */
		mtk_musb->xceiv->state = OTG_STATE_B_IDLE;
		MUSB_DEV_MODE(mtk_musb);
		mt65xx_musb_switch_int_to_host(mtk_musb);
	}
	pr_info("work end, is_host=%d\n", mtk_musb->is_host);
}

static void mt65xx_musb_iddig_int(struct musb *musb)
{
	u32 usb_l1_ploy = musb_readl(musb->mregs, USB_L1INTP);

	pr_info("id pin interrupt assert,polarity=0x%x\n", usb_l1_ploy);
	if (usb_l1_ploy & IDDIG_INT_STATUS)
		usb_l1_ploy &= (~IDDIG_INT_STATUS);
	else
		usb_l1_ploy |= IDDIG_INT_STATUS;

	musb_writel(musb->mregs, USB_L1INTP, usb_l1_ploy);
	musb_writel(musb->mregs, USB_L1INTM,
		    (~IDDIG_INT_STATUS) & musb_readl(musb->mregs, USB_L1INTM));

	schedule_delayed_work(&id_pin_work, HZ / 100);
	pr_info("id pin interrupt assert\n");
}

static void mt65xx_otg_int_init(void)
{
	u32 phy_id_pull = 0;

	phy_id_pull = __raw_readl((void __iomem *)U2PHYDTM1);
	phy_id_pull |= ID_PULL_UP;
	__raw_writel(phy_id_pull, (void __iomem *)U2PHYDTM1);

	musb_writel(mtk_musb->mregs, USB_L1INTM,
		    IDDIG_INT_STATUS | musb_readl(mtk_musb->mregs, USB_L1INTM));
}

static void mt65xx_musb_otg_init(struct musb *musb)
{
	struct musb_hdrc_platform_data *plat;
	struct mt_usb_board_data *bdata;

	plat = dev_get_platdata(musb->controller);
	bdata = plat->board_data;
	/* init idpin interrupt */
	mt65xx_otg_int_init();
	INIT_DELAYED_WORK(&id_pin_work, mt65xx_musb_id_pin_work);
}

static void musb_do_idle(unsigned long _musb)
{
	struct musb *musb = (void *)_musb;
	unsigned long flags;
	u8 devctl;

	if (musb->is_active) {
		pr_info("%d active, igonre do_idle\n", musb->xceiv->state);
		return;
	}
	spin_lock_irqsave(&musb->lock, flags);

	switch (musb->xceiv->state) {
	case OTG_STATE_B_PERIPHERAL:
	case OTG_STATE_A_WAIT_BCON:

		devctl = musb_readb(musb->mregs, MUSB_DEVCTL);
		if (devctl & MUSB_DEVCTL_BDEVICE) {
			musb->xceiv->state = OTG_STATE_B_IDLE;
			MUSB_DEV_MODE(musb);
		} else {
			musb->xceiv->state = OTG_STATE_A_IDLE;
			MUSB_HST_MODE(musb);
		}
		break;
	case OTG_STATE_A_HOST:
		devctl = musb_readb(musb->mregs, MUSB_DEVCTL);
		if (devctl & MUSB_DEVCTL_BDEVICE)
			musb->xceiv->state = OTG_STATE_B_IDLE;
		else
			musb->xceiv->state = OTG_STATE_A_WAIT_BCON;
		break;
	default:
		break;
	}
	spin_unlock_irqrestore(&musb->lock, flags);

	pr_info("otg_state %d\n", musb->xceiv->state);
}

static void mt65xx_musb_try_idle(struct musb *musb, unsigned long timeout)
{
	unsigned long default_timeout = jiffies + msecs_to_jiffies(3);
	static unsigned long last_timer;

	if (timeout == 0)
		timeout = default_timeout;

	/* Never idle if active, or when VBUS timeout is not set as host */
	if (musb->is_active || ((musb->a_wait_bcon == 0) &&
			(musb->xceiv->state == OTG_STATE_A_WAIT_BCON))) {
		pr_info("%d active, deleting timer\n", musb->xceiv->state);
		del_timer_sync(&musb_idle_timer);
		last_timer = jiffies;
		return;
	}

	if (time_after(last_timer, timeout)) {
		if (!timer_pending(&musb_idle_timer))
			last_timer = timeout;
		else {
			pr_info("Longer idle timer already pending, ignoring\n");
			return;
		}
	}
	last_timer = timeout;

	pr_info("%d inactive, for idle timer for %lu ms\n", musb->xceiv->state,
		(unsigned long)jiffies_to_msecs(timeout - jiffies));
	mod_timer(&musb_idle_timer, timeout);
}

static void mt65xx_musb_enable(struct musb *musb)
{
	unsigned long flags;

	if (true == mt65xx_musb_power)
		return;

	flags = musb_readl(musb->mregs, USB_L1INTM);

	/* mask ID pin, so "open clock" and "set flag" won't be interrupted.
	 * ISR may call clock_disable. */
	musb_writel(musb->mregs, USB_L1INTM, (~IDDIG_INT_STATUS) & flags);
	if (platform_init_first) {
		pr_info("usb init first\n\r");
		musb->is_host = true;
		mtk_musb = musb;
	}

	if (!mt65xx_musb_power) {
		if (down_interruptible(&power_clock_lock))
			pr_err("USB20" "%s: busy\n", __func__);

		clk_prepare(univpll_clk);
		clk_enable(univpll_clk);
		pr_info("enable UPLL before connect\n");
		mdelay(10);

		mt65xx_usb_phy_recover();

		mt65xx_musb_power = true;

		up(&power_clock_lock);
	}

	musb_writel(mtk_musb->mregs, USB_L1INTM, flags);
}

static void mt65xx_musb_disable(struct musb *musb)
{
	pr_info("%s, %d\n", __func__, mt65xx_musb_power);

	if (false == mt65xx_musb_power)
		return;

	if (platform_init_first) {
		pr_info("usb init first\n\r");
		musb->is_host = false;
		platform_init_first = false;
	}

	if (mt65xx_musb_power) {
		if (down_interruptible(&power_clock_lock))
			pr_err("USB20" "%s: busy\n", __func__);

		mt65xx_musb_phy_savecurrent();
		mt65xx_musb_power = false;
		clk_disable(univpll_clk);
		clk_unprepare(univpll_clk);
		pr_info("disable UPLL before disconnect\n");

		up(&power_clock_lock);
	}
}

void mt_usb_connect(void)
{
	pr_info("[MUSB] USB is ready for connect\n");
	pr_info("is_host %d power %d\n", mtk_musb->is_host, mt65xx_musb_power);
	if (!mtk_musb || mtk_musb->is_host || mt65xx_musb_power)
		return;

	musb_start(mtk_musb);
	pr_info("[MUSB] USB connect\n");
}

void mt_usb_disconnect(void)
{
	pr_info("[MUSB] USB is ready for disconnect\n");

	if (!mtk_musb || mtk_musb->is_host || !mt65xx_musb_power)
		return;

	musb_stop(mtk_musb);

	pr_info("[MUSB] USB disconnect\n");
}

/*-------------------------------------------------------------------------*/
static irqreturn_t generic_interrupt(int irq, void *__hci)
{
	unsigned long flags;
	irqreturn_t retval = IRQ_NONE;
	struct musb *musb = __hci;

	spin_lock_irqsave(&musb->lock, flags);

	/* musb_read_clear_generic_interrupt */
	musb->int_usb = musb_readb(musb->mregs, MUSB_INTRUSB) &
		musb_readb(musb->mregs, MUSB_INTRUSBE);
	musb->int_tx = musb_readw(musb->mregs, MUSB_INTRTX)
		& musb_readw(musb->mregs, MUSB_INTRTXE);
	musb->int_rx = musb_readw(musb->mregs, MUSB_INTRRX)
		& musb_readw(musb->mregs, MUSB_INTRRXE);
	/* memory barrier ensure excute order */
	mb();
	musb_writew(musb->mregs, MUSB_INTRRX, musb->int_rx);
	musb_writew(musb->mregs, MUSB_INTRTX, musb->int_tx);
	musb_writeb(musb->mregs, MUSB_INTRUSB, musb->int_usb);
	/* musb_read_clear_generic_interrupt */

	if (musb->int_usb || musb->int_tx || musb->int_rx)
		retval = musb_interrupt(musb);

	spin_unlock_irqrestore(&musb->lock, flags);

	return retval;
}

static irqreturn_t mt65xx_musb_interrupt(int irq, void *dev_id)
{
	irqreturn_t tmp_status;
	irqreturn_t status = IRQ_NONE;
	struct musb *musb = (struct musb *)dev_id;
	u32 usb_l1_ints;

	usb_l1_ints = musb_readl(musb->mregs, USB_L1INTS) &
			musb_readl(musb->mregs, USB_L1INTM);
	pr_debug("usb interrupt assert %x %x  %x %x %x\n", usb_l1_ints,
		musb_readl(musb->mregs, USB_L1INTM),
		musb_readb(musb->mregs, MUSB_INTRUSBE),
		musb_readw(musb->mregs, MUSB_INTRTX),
			musb_readw(musb->mregs, MUSB_INTRTXE));

	if ((usb_l1_ints & TX_INT_STATUS) || (usb_l1_ints & RX_INT_STATUS)
		|| (usb_l1_ints & USBCOM_INT_STATUS)) {
		tmp_status = generic_interrupt(irq, musb);
		if (tmp_status != IRQ_NONE)
			status = tmp_status;
	}

	if (usb_l1_ints & DMA_INT_STATUS) {
		tmp_status = dma_controller_irq(irq, musb->dma_controller);
		if (tmp_status != IRQ_NONE)
			status = tmp_status;
	}
	if (usb_l1_ints & IDDIG_INT_STATUS) {
		mt65xx_musb_iddig_int(musb);
		status = IRQ_HANDLED;
	}

	return status;

}

static int mt65xx_musb_init(struct musb *musb)
{
	#ifdef CONFIG_OF
		int ret = 0;
	#endif
	musb->xceiv = usb_get_phy(USB_PHY_TYPE_USB2);
	musb->dyn_fifo = true;
	musb->is_host = false;
	#ifdef CONFIG_OF
	ret = regulator_set_voltage(usb_power, VOL_3300, VOL_3300);
	if (ret) {
		pr_err("Failed to set regulator, return value:%d!\n", ret);
		return ret;
	}
	ret = regulator_enable(usb_power);
	if (ret) {
		pr_err("Failed to enable usb regulator, ret:%d!\n", ret);
		return ret;
	}
    #endif
	mt65xx_musb_enable(musb);

	musb->isr = mt65xx_musb_interrupt;
	musb_writel(musb->mregs, MUSB_HSDMA_INTR,
				0xff | (0xff << DMA_INTR_UNMASK_SET_OFFSET));
	pr_info("musb platform init %x\n",
			musb_readl(musb->mregs, MUSB_HSDMA_INTR));
	musb_writel(musb->mregs, USB_L1INTM, TX_INT_STATUS | RX_INT_STATUS |
			USBCOM_INT_STATUS | DMA_INT_STATUS);
	setup_timer(&musb_idle_timer, musb_do_idle, (unsigned long)musb);

	mt65xx_musb_otg_init(musb);

	return 0;
}

static int mt65xx_musb_exit(struct musb *musb)
{
	del_timer_sync(&musb_idle_timer);
	return 0;
}

static const struct musb_platform_ops mt65xx_musb_ops = {
	.init = mt65xx_musb_init,
	.exit = mt65xx_musb_exit,
	/*.set_mode     = mt65xx_musb_set_mode, */
	.try_idle = mt65xx_musb_try_idle,
	.enable = mt65xx_musb_enable,
	.disable = mt65xx_musb_disable,
	.set_vbus = mt65xx_musb_set_vbus,
	.vbus_status = mt65xx_musb_get_vbus_status
};

static u64 mt65xx_musb_dmamask = DMA_BIT_MASK(32);

static int mt65xx_probe(struct platform_device *pdev)
{
	struct musb_hdrc_platform_data *pdata = pdev->dev.platform_data;
	struct platform_device *musb;
	struct mt65xx_glue *glue;
	#ifdef CONFIG_OF
	struct musb_hdrc_config *config;
	struct device_node *np = pdev->dev.of_node;
	#endif
	int ret = -ENOMEM;

	glue = kzalloc(sizeof(*glue), GFP_KERNEL);
	if (!glue) {
		dev_err(&pdev->dev, "failed to allocate glue context\n");
		goto err0;
	}

	musb = platform_device_alloc("musb-hdrc", PLATFORM_DEVID_AUTO);
	if (!musb) {
		dev_err(&pdev->dev, "failed to allocate musb device\n");
		goto err1;
	}
#ifdef CONFIG_OF
	usb_phy_base = (unsigned long) of_iomap(pdev->dev.of_node, 1);
	usb_clk_main = devm_clk_get(&pdev->dev, "usb0_clk_main");
	if (IS_ERR(usb_clk_main)) {
		dev_err(&pdev->dev, "cannot get usb0 main clock");
		ret = -ENOMEM;
		goto err1;
	}
	univpll_clk = devm_clk_get(&pdev->dev, "univpll_clk");
	if (IS_ERR(univpll_clk)) {
		dev_err(&pdev->dev, "cannot get univpll clock");
		ret = -ENOMEM;
		goto err2;
	}
	usb_power = devm_regulator_get(&pdev->dev, "usb-power");
	if (!usb_power) {
		pr_err("Cannot get usb power from the device tree!\n");
		return -EINVAL;
	}
	pdata = devm_kzalloc(&pdev->dev, sizeof(*pdata), GFP_KERNEL);
	pr_info("usb_phy_base:0x%lx\n", usb_phy_base);
	if (!pdata) {
		dev_err(&pdev->dev, "failed to allocate musb platfrom data\n");
		goto err3;
	}
	config = devm_kzalloc(&pdev->dev, sizeof(*config), GFP_KERNEL);
	if (!config) {
		dev_err(&pdev->dev, "failed to allocate musb hdrc config\n");
		goto err3;
	}
	of_property_read_u32(np, "mode", (u32 *) &pdata->mode);
	/* of_property_read_u32(np, "dma_channels", (u32 *)
	 * &config->dma_channels); */
	of_property_read_u32(np, "num_eps", (u32 *) &config->num_eps);
	pr_info("config->num_eps:%d\n", config->num_eps);
	config->multipoint = of_property_read_bool(np, "multipoint");
	pr_info("config->mulipoint:%d\n", config->multipoint);
	of_property_read_u32(np, "ram_bits", (u32 *)&config->ram_bits);
	pdata->config = config;
#endif
	musb->dev.parent = &pdev->dev;
	musb->dev.dma_mask = &mt65xx_musb_dmamask;
	musb->dev.coherent_dma_mask = mt65xx_musb_dmamask;
#ifdef CONFIG_OF
	pdev->dev.dma_mask = &mt65xx_musb_dmamask;
	pdev->dev.coherent_dma_mask = mt65xx_musb_dmamask;
#endif

	glue->dev = &pdev->dev;
	glue->musb = musb;

	pdata->platform_ops = &mt65xx_musb_ops;
	usb_phy_generic_register();
	platform_set_drvdata(pdev, glue);

	ret = platform_device_add_resources(musb, pdev->resource,
					pdev->num_resources);
	if (ret) {
		dev_err(&pdev->dev, "failed to add resources\n");
		goto err3;
	}

	ret = platform_device_add_data(musb, pdata, sizeof(*pdata));
	if (ret) {
		dev_err(&pdev->dev, "failed to add platform_data\n");
		goto err3;
	}

	ret = platform_device_add(musb);

	if (ret) {
		dev_err(&pdev->dev, "failed to register musb device\n");
		goto err3;
	}
#ifdef CONFIG_OF
	pr_info("USB probe done!\n");
#endif

	return 0;

err3:
	platform_device_put(musb);
	devm_clk_put(&pdev->dev, univpll_clk);
err2:
	devm_clk_put(&pdev->dev, usb_clk_main);
err1:
	kfree(glue);
err0:
	return ret;
}

static int mt65xx_remove(struct platform_device *pdev)
{
	struct mt65xx_glue *glue = platform_get_drvdata(pdev);

	platform_device_unregister(glue->musb);
	kfree(glue);
	return 0;
}

static struct platform_driver mt65xx_driver = {
	.remove = mt65xx_remove,
	.probe = mt65xx_probe,
	.driver = {
		.name = "musb-mt65xx",
#ifdef CONFIG_OF
		.of_match_table = apusb_of_ids,
#endif
		},
};

static int __init mt65xx_init(void)
{
	return platform_driver_register(&mt65xx_driver);
}

subsys_initcall(mt65xx_init);

static void __exit mt65xx_exit(void)
{
	platform_driver_unregister(&mt65xx_driver);
}

module_exit(mt65xx_exit)
