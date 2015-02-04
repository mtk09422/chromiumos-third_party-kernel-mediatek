/*
 * Copyright (c) 2015 MediaTek Inc.
 * Author: Chunfeng.Yun <chunfeng.yun@mediatek.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/resource.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/export.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/gpio.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/of_address.h>
#include <linux/usb/otg.h>
#include <linux/usb/of.h>
#include <linux/regulator/consumer.h>
#include <linux/usb/phy.h>

#include "phy-mt65xx-usb.h"


static inline void u3p_writel(void __iomem *base, u32 offset, u32 data)
{
	writel(data, base + offset);
}

static inline u32 u3p_readl(void __iomem *base, u32 offset)
{
	return readl(base + offset);
}

static inline void u3p_setmsk(void __iomem *base, u32 offset, u32 msk)
{
	void __iomem *addr = base + offset;

	writel((readl(addr) | msk), addr);
}

static inline void u3p_clrmsk(void __iomem *base, u32 offset, u32 msk)
{
	void __iomem *addr = base + offset;

	writel((readl(addr) & ~msk), addr);
}

static inline void u3p_setval(void __iomem *base, u32 offset,
	u32 mask, u32 value)
{
	void __iomem *addr = base + offset;
	unsigned int new_value;

	new_value = (readl(addr) & ~mask) | value;
	writel(new_value, addr);
}

static void phy_index_power_on(struct mt65xx_u3phy *u3phy, int index)
{
	void __iomem *sif_base = u3phy->sif_base + U3P_PHY_BASE(index);

	if (!index) {
		/* Set RG_SSUSB_VUSB10_ON as 1 after VUSB10 ready */
		u3p_setmsk(sif_base, U3P_U3_PHYA_REG0, P3A_RG_U3_VUSB10_ON);
		/*[MT6593 only]power domain iso disable */
		u3p_clrmsk(sif_base, U3P_USBPHYACR6, PA6_RG_U2_ISO_EN);
	}
	/*switch to USB function. (system register, force ip into usb mode) */
	u3p_clrmsk(sif_base, U3P_U2PHYDTM0, P2C_FORCE_UART_EN);
	u3p_clrmsk(sif_base, U3P_U2PHYDTM1, P2C_RG_UART_EN);
	if (!index)
		u3p_clrmsk(sif_base, U3P_U2PHYACR4, P2C_U2_GPIO_CTR_MSK);

	/*(force_suspendm=0) (let suspendm=1, enable usb 480MHz pll) */
	u3p_clrmsk(sif_base, U3P_U2PHYDTM0, P2C_FORCE_SUSPENDM);
	u3p_clrmsk(sif_base, U3P_U2PHYDTM0,
			P2C_RG_XCVRSEL | P2C_RG_DATAIN | P2C_DTM0_PART_MASK);

	/*DP/DM BC1.1 path Disable */
	u3p_clrmsk(sif_base, U3P_USBPHYACR6, PA6_RG_U2_BC11_SW_EN);
	/*OTG Enable */
	u3p_setmsk(sif_base, U3P_USBPHYACR6, PA6_RG_U2_OTG_VBUSCMP_EN);
	u3p_setval(sif_base, U3P_U3PHYA_DA_REG0, P3A_RG_XTAL_EXT_EN_U3,
			P3A_RG_XTAL_EXT_EN_U3_VAL(2));
	u3p_setval(sif_base, U3P_U3_PHYA_REG9, P3A_RG_RX_DAC_MUX,
			P3A_RG_RX_DAC_MUX_VAL(4));
	if (!index) {
		u3p_setmsk(sif_base, U3P_XTALCTL3, XC3_RG_U3_XTAL_RX_PWD);
		u3p_setmsk(sif_base, U3P_XTALCTL3, XC3_RG_U3_FRC_XTAL_RX_PWD);
		/*[mt8173]disable Change 100uA current from SSUSB */
		u3p_clrmsk(sif_base, U3P_USBPHYACR5, PA5_RG_U2_HS_100U_U3_EN);
	}
	u3p_setval(sif_base, U3P_U3_PHYA_REG6, P3A_RG_TX_EIDLE_CM,
			P3A_RG_TX_EIDLE_CM_VAL(0xe));
	u3p_setval(sif_base, U3P_PHYD_CDR1, P3D_RG_CDR_BIR_LTD0,
			P3D_RG_CDR_BIR_LTD0_VAL(0xc));
	u3p_setval(sif_base, U3P_PHYD_CDR1, P3D_RG_CDR_BIR_LTD1,
			P3D_RG_CDR_BIR_LTD1_VAL(0x3));

	udelay(800);
	u3p_setmsk(sif_base, U3P_U2PHYDTM1, P2C_RG_VBUSVALID | P2C_RG_AVALID);
	u3p_clrmsk(sif_base, U3P_U2PHYDTM1, P2C_RG_SESSEND);

	/* USB 2.0 slew rate calibration */
	u3p_setval(sif_base, U3P_USBPHYACR5, PA5_RG_U2_HSTX_SRCTRL,
			PA5_RG_U2_HSTX_SRCTRL_VAL(4));

	dev_dbg(u3phy->dev, "%s(%d)\n", __func__, index);
}


static void phy_index_power_off(struct mt65xx_u3phy *u3phy, int index)
{
	void __iomem *sif_base = u3phy->sif_base + U3P_PHY_BASE(index);


	/*switch to USB function. (system register, force ip into usb mode) */
	u3p_clrmsk(sif_base, U3P_U2PHYDTM0, P2C_FORCE_UART_EN);
	u3p_clrmsk(sif_base, U3P_U2PHYDTM1, P2C_RG_UART_EN);
	if (!index)
		u3p_clrmsk(sif_base, U3P_U2PHYACR4, P2C_U2_GPIO_CTR_MSK);

	u3p_setmsk(sif_base, U3P_U2PHYDTM0, P2C_FORCE_SUSPENDM);
	u3p_setval(sif_base, U3P_U2PHYDTM0,
			P2C_RG_XCVRSEL, P2C_RG_XCVRSEL_VAL(1));
	u3p_setval(sif_base, U3P_U2PHYDTM0,
			P2C_RG_DATAIN, P2C_RG_DATAIN_VAL(0));
	u3p_setmsk(sif_base, U3P_U2PHYDTM0, P2C_DTM0_PART_MASK);
	/*DP/DM BC1.1 path Disable */
	u3p_clrmsk(sif_base, U3P_USBPHYACR6, PA6_RG_U2_BC11_SW_EN);
	/*OTG Disable */
	u3p_clrmsk(sif_base, U3P_USBPHYACR6, PA6_RG_U2_OTG_VBUSCMP_EN);
	if (!index) {
		/*Change 100uA current switch to USB2.0 */
		u3p_clrmsk(sif_base, U3P_USBPHYACR5, PA5_RG_U2_HS_100U_U3_EN);
	}
	udelay(800);

	/*let suspendm=0, set utmi into analog power down */
	u3p_clrmsk(sif_base, U3P_U2PHYDTM0, P2C_RG_SUSPENDM);
	udelay(1);

	u3p_clrmsk(sif_base, U3P_U2PHYDTM1, P2C_RG_VBUSVALID | P2C_RG_AVALID);
	u3p_setmsk(sif_base, U3P_U2PHYDTM1, P2C_RG_SESSEND);
	if (!index) {
		/* Set RG_SSUSB_VUSB10_ON as 1 after VUSB10 ready */
		u3p_clrmsk(sif_base, U3P_U3_PHYA_REG0, P3A_RG_U3_VUSB10_ON);
	}
	dev_dbg(u3phy->dev, "%s(%d)\n", __func__, index);
}


static void u3phy_power_on(struct mt65xx_u3phy *u3phy)
{
	phy_index_power_on(u3phy, 0);
	phy_index_power_on(u3phy, 1);
}

static void u3phy_power_off(struct mt65xx_u3phy *u3phy)
{
	phy_index_power_off(u3phy, 0);
	phy_index_power_off(u3phy, 1);
}


static int wait_for_value(void __iomem *base, int addr,
			int msk, int value, int count)
{
	int i;

	for (i = 0; i < count; i++) {
		if ((u3p_readl(base, addr) & msk) == value)
			return 0;
		udelay(100);
	}

	return -ETIMEDOUT;
}


static int check_ip_clk_status(struct mt65xx_u3phy *u3phy)
{
	int ret;
	int u3_port_num;
	int u2_port_num;
	u32 xhci_cap;
	void __iomem *sif_base = u3phy->sif_base;

	xhci_cap = u3p_readl(sif_base, U3P_IP_XHCI_CAP);
	u3_port_num = CAP_U3_PORT_NUM(xhci_cap);
	u2_port_num = CAP_U2_PORT_NUM(xhci_cap);

	ret = wait_for_value(sif_base, U3P_IP_PW_STS1, STS1_SYSPLL_STABLE,
			  STS1_SYSPLL_STABLE, 100);
	if (ret) {
		dev_err(u3phy->dev, "sypll is not stable!!!\n");
		goto err;
	}

	ret = wait_for_value(sif_base, U3P_IP_PW_STS1, STS1_REF_RST,
			  STS1_REF_RST, 100);
	if (ret) {
		dev_err(u3phy->dev, "ref_clk is still active!!!\n");
		goto err;
	}

	ret = wait_for_value(sif_base, U3P_IP_PW_STS1, STS1_SYS125_RST,
			   STS1_SYS125_RST, 100);
	if (ret) {
		dev_err(u3phy->dev, "sys125_ck is still active!!!\n");
		goto err;
	}

	if (u3_port_num) {
		ret = wait_for_value(sif_base, U3P_IP_PW_STS1, STS1_U3_MAC_RST,
				   STS1_U3_MAC_RST, 100);
		if (ret) {
			dev_err(u3phy->dev, "mac3_mac_ck is still active!!!\n");
			goto err;
		}
	}

	if (u2_port_num) {
		ret = wait_for_value(sif_base, U3P_IP_PW_STS2, STS2_U2_MAC_RST,
				   STS2_U2_MAC_RST, 100);
		if (ret) {
			dev_err(u3phy->dev, "mac2_sys_ck is still active!!!\n");
			goto err;
		}
	}
	return 0;

err:
	return -EBUSY;
}

static int u3phy_ports_enable(struct mt65xx_u3phy *u3phy)
{
	int i;
	u32 temp;
	int u3_port_num;
	int u2_port_num;
	void __iomem *sif_base = u3phy->sif_base;


	temp = u3p_readl(sif_base, U3P_IP_XHCI_CAP);
	u3_port_num = CAP_U3_PORT_NUM(temp);
	u2_port_num = CAP_U2_PORT_NUM(temp);
	dev_dbg(u3phy->dev, "%s u2p:%d, u3p:%d\n",
		__func__, u2_port_num, u3_port_num);

	/* power on host ip */
	u3p_clrmsk(sif_base, U3P_IP_PW_CTRL1, CTRL1_IP_HOST_PDN);

	/* power on and enable all u3 ports */
	for (i = 0; i < u3_port_num; i++) {
		temp = u3p_readl(sif_base, U3P_U3_CTRL(i));
		temp &= ~(CTRL_U3_PORT_PDN | CTRL_U3_PORT_DIS);
		temp |= CTRL_U3_PORT_HOST_SEL;
		u3p_writel(sif_base, U3P_U3_CTRL(i), temp);
	}

	/* power on and enable all u2 ports */
	for (i = 0; i < u2_port_num; i++) {
		temp = u3p_readl(sif_base, U3P_U2_CTRL(i));
		temp &= ~(CTRL_U2_PORT_PDN | CTRL_U2_PORT_DIS);
		temp |= CTRL_U2_PORT_HOST_SEL;
		u3p_writel(sif_base, U3P_U2_CTRL(i), temp);
	}
	return check_ip_clk_status(u3phy);
}

static inline void u3phy_soft_reset(struct mt65xx_u3phy *u3phy)
{
	/* reset whole ip */
	u3p_setmsk(u3phy->sif_base, U3P_IP_PW_CTRL0, CTRL0_IP_SW_RST);
	u3p_clrmsk(u3phy->sif_base, U3P_IP_PW_CTRL0, CTRL0_IP_SW_RST);
}


static void u3phy_timing_init(struct mt65xx_u3phy *u3phy)
{
	void __iomem *mbase = u3phy->mac_base;
	int u3_port_num;
	u32 temp;

	temp = u3p_readl(u3phy->sif_base, U3P_IP_XHCI_CAP);
	u3_port_num = CAP_U3_PORT_NUM(temp);

	if (u3_port_num) {
		/* set MAC reference clock speed */
		u3p_setval(mbase, U3P_UX_EXIT_LFPS_PARAM,
				RX_UX_EXIT_REF, RX_UX_EXIT_REF_VAL);
		/* set REF_CLK */
		u3p_setval(mbase, U3P_REF_CLK_PARAM,
				REF_CLK_1000NS, REF_CLK_VAL_DEF);
		/* set SYS_CLK */
		u3p_setval(mbase, U3P_TIMING_PULSE_CTRL,
				U3T_CNT_1US, U3T_CNT_1US_VAL);
		/* set LINK_PM_TIMER=3 */
		u3p_setval(mbase, U3P_LINK_PM_TIMER,
				PM_LC_TIMEOUT, PM_LC_TIMEOUT_VAL);
	}
	u3p_setval(mbase, U3P_U2_TIMING_PARAM, U2T_VAL_1US, U2T_VAL_1US_VAL);
}


/*Turn on/off ADA_SSUSB_XTAL_CK 26MHz*/
static void u3_xtal_clock_enable(struct mt65xx_u3phy *u3phy)
{
	u3p_setmsk(u3phy->ap_pll, AP_PLL_CON0, CON0_RG_LTECLKSQ_EN);
	udelay(100); /* wait for PLL stable */

	u3p_setmsk(u3phy->ap_pll, AP_PLL_CON0, CON0_RG_LTECLKSQ_LPF_EN);
	u3p_setmsk(u3phy->ap_pll, AP_PLL_CON2, CON2_DA_REF2USB_TX_EN);
	udelay(100); /* wait for PLL stable */

	u3p_setmsk(u3phy->ap_pll, AP_PLL_CON2, CON2_DA_REF2USB_TX_LPF_EN);
	u3p_setmsk(u3phy->ap_pll, AP_PLL_CON2, CON2_DA_REF2USB_TX_OUT_EN);

}

static inline void u3_xtal_clock_disable(struct mt65xx_u3phy *u3phy)
{
	u3p_clrmsk(u3phy->ap_pll, AP_PLL_CON2, CON2_DA_REF2USB_TX_MASK);
}


static int get_u3phy_clks(struct mt65xx_u3phy *u3phy)
{
	struct clk *tmp;

	tmp = devm_clk_get(u3phy->dev, "scp_sys_usb");
	if (IS_ERR(tmp)) {
		dev_err(u3phy->dev, "error to get scp-sys-usb\n");
		return PTR_ERR(tmp);
	}
	u3phy->scp_sys = tmp;

	tmp = devm_clk_get(u3phy->dev, "peri_usb0");
	if (IS_ERR(tmp)) {
		dev_err(u3phy->dev, "error to get peri_usb0\n");
		return PTR_ERR(tmp);
	}
	u3phy->peri_usb0 = tmp;

	tmp = devm_clk_get(u3phy->dev, "peri_usb1");
	if (IS_ERR(tmp)) {
		dev_err(u3phy->dev, "error to get peri_usb1\n");
		return PTR_ERR(tmp);
	}
	u3phy->peri_usb0 = tmp;
	dev_dbg(u3phy->dev, "mu3d get clks ok\n");

	return 0;
}


static int u3phy_clks_enable(struct mt65xx_u3phy *u3phy)
{
	int ret;

	u3_xtal_clock_enable(u3phy);

	ret = clk_prepare_enable(u3phy->scp_sys);
	if (ret) {
		dev_err(u3phy->dev, "failed to enable scp-sys-clk\n");
		goto scp_sys_err;
	}
	ret = clk_prepare_enable(u3phy->peri_usb0);
	if (ret) {
		dev_err(u3phy->dev, "failed to enable peri-usb0\n");
		goto clken_usb0_err;
	}
	ret = clk_prepare_enable(u3phy->peri_usb0);
	if (ret) {
		dev_err(u3phy->dev, "failed to enable peri-usb0\n");
		goto clken_usb1_err;
	}

	udelay(50);

	return 0;

clken_usb1_err:
	clk_disable_unprepare(u3phy->peri_usb0);

clken_usb0_err:
	clk_disable_unprepare(u3phy->scp_sys);

scp_sys_err:
	return -EINVAL;
}

static void u3phy_clks_disable(struct mt65xx_u3phy *u3phy)
{
	clk_disable_unprepare(u3phy->peri_usb1);
	clk_disable_unprepare(u3phy->peri_usb0);
	clk_disable_unprepare(u3phy->scp_sys);
	u3_xtal_clock_disable(u3phy);
}


static int get_u3phy_p1_vbus_gpio(struct mt65xx_u3phy *u3phy)
{
	struct device_node *dn = u3phy->dev->of_node;
	u32 flags;

	/*
	* not all platforms have such gpio, so it's not a error
	* if it does not exists.
	*/
	u3phy->p1_gpio_num = of_get_named_gpio_flags(dn, "p1-gpio", 0, &flags);
	if (u3phy->p1_gpio_num < 0) {
		dev_warn(u3phy->dev, "can't get p1-gpio-num, ignore it.\n");
		return 0;
	}
	u3phy->p1_gpio_active_low = !!(flags & OF_GPIO_ACTIVE_LOW);

	dev_dbg(u3phy->dev, "%s() vbus-gpio:%d.\n",
		__func__, u3phy->p1_gpio_num);

	return 0;
}

static inline void vbus_gpio_pullup(struct mt65xx_u3phy *u3phy)
{
	if (u3phy->p1_gpio_num >= 0)
		gpio_set_value(u3phy->p1_gpio_num, !u3phy->p1_gpio_active_low);
}

static inline void vbus_gpio_pulldown(struct mt65xx_u3phy *u3phy)
{
	if (u3phy->p1_gpio_num >= 0)
		gpio_set_value(u3phy->p1_gpio_num, u3phy->p1_gpio_active_low);
}


static int mt65xx_u3phy_init(struct usb_phy *phy)
{
	struct mt65xx_u3phy *u3phy;
	int ret;

	u3phy = container_of(phy, struct mt65xx_u3phy, phy);
	dev_dbg(u3phy->dev, "%s(is_on:%d)+\n", __func__, u3phy->is_on);
	if (0 == u3phy->is_on) {
		vbus_gpio_pullup(u3phy);
		ret = regulator_enable(u3phy->vusb33);
		if (ret) {
			dev_err(u3phy->dev, "failed to enable vusb33\n");
			goto reg_err;
		}

		ret = u3phy_clks_enable(u3phy);
		if (ret) {
			dev_err(u3phy->dev, "failed to enable clks\n");
			goto clks_err;
		}

		u3phy_soft_reset(u3phy);
		ret = u3phy_ports_enable(u3phy);
		if (ret) {
			dev_err(u3phy->dev, "failed to enable ports\n");
			goto port_err;
		}
		u3phy_timing_init(u3phy);
		u3phy_power_on(u3phy);
	}
	u3phy->is_on++;

	return 0;

port_err:
	u3phy_clks_disable(u3phy);
clks_err:
	regulator_disable(u3phy->vusb33);
reg_err:
	vbus_gpio_pulldown(u3phy);
	return ret;
}


static void mt65xx_u3phy_shutdown(struct usb_phy *phy)
{
	struct mt65xx_u3phy *u3phy;

	u3phy = container_of(phy, struct mt65xx_u3phy, phy);
	dev_dbg(u3phy->dev, "%s(is_on:%d)+\n", __func__, u3phy->is_on);

	u3phy->is_on--;
	if (0 == u3phy->is_on) {
		u3phy_power_off(u3phy);
		u3phy_clks_disable(u3phy);
		regulator_disable(u3phy->vusb33);
		vbus_gpio_pulldown(u3phy);
	}
}

static inline int u3phy_suspend(struct mt65xx_u3phy *u3phy)
{
	mt65xx_u3phy_shutdown(&u3phy->phy);
	return 0;
}

static inline int u3phy_resume(struct mt65xx_u3phy *u3phy)
{
	return mt65xx_u3phy_init(&u3phy->phy);
}

static int	mt65xx_u3phy_suspend(struct usb_phy *x, int suspend)
{
	struct mt65xx_u3phy *u3phy = container_of(x, struct mt65xx_u3phy, phy);

	if (suspend)
		return u3phy_suspend(u3phy);
	else
		return u3phy_resume(u3phy);
}


static void __iomem *get_ap_pll_regs_base(struct device *dev, char *phandle)
{
	struct device_node *dn = dev->of_node;
	struct device_node *pll_dn;

	pll_dn = of_parse_phandle(dn, phandle, 0);
	if (!pll_dn) {
		dev_err(dev, "failed to get %s phandle in %s node\n", phandle,
			dev->of_node->full_name);
		return NULL;
	}

	return of_iomap(pll_dn, 0);
}

static int u3phy_bind_host(struct device *dev, const char *phandle)
{
	struct device_node *dn = dev->of_node;
	struct device_node *hcd_dn;
	struct platform_device *hcd_dev;


	hcd_dn = of_parse_phandle(dn, phandle, 0);
	if (!hcd_dn) {
		dev_dbg(dev, "failed to get %s phandle in %s node\n", phandle,
			dev->of_node->full_name);
		return -ENODEV;
	}

	hcd_dev = of_find_device_by_node(hcd_dn);
	if (!hcd_dev) {
		dev_err(dev, "failed to get control device %s\n", phandle);
		return -EINVAL;
	}
	usb_bind_phy(dev_name(&hcd_dev->dev), 0, dev_name(dev));
	dev_dbg(dev, "host: %s, phy: %s\n",
		dev_name(&hcd_dev->dev), dev_name(dev));

	return 0;
}


static struct of_device_id mt65xx_u3phy_id_table[] = {
	{ .compatible = "mediatek,mt8173-u3phy",},
	{ },
};
MODULE_DEVICE_TABLE(of, mt65xx_u3phy_id_table);


static int mt65xx_u3phy_probe(struct platform_device *pdev)
{
	struct device_node *dn = pdev->dev.of_node;
	struct mt65xx_u3phy *u3phy = NULL;
	int retval = -ENOMEM;

	u3phy = devm_kzalloc(&pdev->dev, sizeof(*u3phy), GFP_KERNEL);
	if (!u3phy)
		goto err;

	u3phy->is_on = 0;
	u3phy->dev = &pdev->dev;

	u3phy->mac_base = of_iomap(dn, 0);
	if (!u3phy->mac_base) {
		dev_err(&pdev->dev, "failed to remap mac regs\n");
		goto err;
	}

	u3phy->sif_base = of_iomap(dn, 1);
	if (!u3phy->sif_base) {
		dev_err(&pdev->dev, "failed to remap sif regs\n");
		goto sif_err;
	}

	u3phy->ap_pll = get_ap_pll_regs_base(&pdev->dev, "usb3_ref_clk");
	if (!u3phy->ap_pll) {
		dev_err(&pdev->dev, "failed to remap ap pll regs\n");
		goto pll_err;
	}

	u3phy->vusb33 = regulator_get(u3phy->dev, "reg-vusb33");
	if (IS_ERR_OR_NULL(u3phy->vusb33)) {
		dev_err(&pdev->dev, "fail to get vusb33\n");
		retval = PTR_ERR(u3phy->vusb33);
		goto vusb33_err;
	}
	retval = get_u3phy_p1_vbus_gpio(u3phy);
	if (retval)
		goto vbus_err;

	retval = get_u3phy_clks(u3phy);
	if (retval) {
		dev_err(&pdev->dev, "failed to get clks\n");
		goto vbus_err;
	}

	u3phy->phy.dev = u3phy->dev;
	u3phy->phy.label = "mt65xx-usb3phy";
	u3phy->phy.type = USB_PHY_TYPE_USB3;
	u3phy->phy.init = mt65xx_u3phy_init;
	u3phy->phy.shutdown = mt65xx_u3phy_shutdown;
	u3phy->phy.set_suspend = mt65xx_u3phy_suspend;

	platform_set_drvdata(pdev, u3phy);
	retval = u3phy_bind_host(&pdev->dev, "usb-host");
	if (retval) {
		dev_err(&pdev->dev, "failed to bind host & phy\n");
		goto vbus_err;
	}

	retval = usb_add_phy_dev(&u3phy->phy);
	if (retval) {
		dev_err(&pdev->dev, "failed to add phy\n");
		goto vbus_err;
	}

	dev_info(&pdev->dev, "probe done!\n");

	return 0;

vbus_err:
	regulator_put(u3phy->vusb33);
vusb33_err:
	iounmap(u3phy->ap_pll);
pll_err:
	iounmap(u3phy->sif_base);
sif_err:
	iounmap(u3phy->mac_base);
err:
	return retval;
}

static int mt65xx_u3phy_remove(struct platform_device *pdev)
{
	struct mt65xx_u3phy *u3phy = platform_get_drvdata(pdev);

	iounmap(u3phy->mac_base);
	iounmap(u3phy->sif_base);
	iounmap(u3phy->ap_pll);
	regulator_put(u3phy->vusb33);
	usb_remove_phy(&u3phy->phy);

	return 0;
}

static struct platform_driver mt65xx_u3phy_driver = {
	.probe		= mt65xx_u3phy_probe,
	.remove		= mt65xx_u3phy_remove,
	.driver		= {
		.name	= "mt65xx-u3phy",
		.owner	= THIS_MODULE,
		.of_match_table = mt65xx_u3phy_id_table,
	},
};

module_platform_driver(mt65xx_u3phy_driver);

MODULE_DESCRIPTION("Mt65xx USB PHY driver");
MODULE_LICENSE("GPL v2");
