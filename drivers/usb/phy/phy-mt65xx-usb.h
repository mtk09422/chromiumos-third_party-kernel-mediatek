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

#ifndef __MT65XX_U3PHY_H
#define __MT65XX_U3PHY_H

#include <linux/clk.h>
#include <linux/usb/phy.h>

/* Register definitions */

#define AP_PLL_CON0		0x00
#define CON0_RG_LTECLKSQ_EN	(0x1 << 0)
#define CON0_RG_LTECLKSQ_LPF_EN	(0x1 << 1)

#define AP_PLL_CON1		0x04
#define AP_PLL_CON2		0x08
#define CON2_DA_REF2USB_TX_EN	(0x1 << 0)
#define CON2_DA_REF2USB_TX_LPF_EN	(0x1 << 1)
#define CON2_DA_REF2USB_TX_OUT_EN	(0x1 << 2)
#define CON2_DA_REF2USB_TX_MASK \
	(CON2_DA_REF2USB_TX_EN | CON2_DA_REF2USB_TX_LPF_EN | \
	CON2_DA_REF2USB_TX_OUT_EN)


/*
 * 0x1127_0000 for MAC register
 * relative to MAC base address
 */
#define SSUSB_USB3_MAC_CSR_BASE	(0x1400)
#define SSUSB_USB3_SYS_CSR_BASE	(0x1400)
#define SSUSB_USB2_CSR_BASE		(0x2400)

/*
 * 0x1128_0000 for sifslv register in Infra
 * relative to USB3_SIF_BASE base address
 */
#define SSUSB_SIFSLV_IPPC_BASE		(0x700)

/*
 * 0x1129_0000 for sifslv2 register in top_ao
 *  relative to USB3_SIF2_BASE base address
 */
#define SSUSB_SIFSLV_U2PHY_COM_BASE	(0x10800)
#define SSUSB_SIFSLV_U3PHYD_BASE	(0x10900)
#define SSUSB_SIFSLV_U2FREQ_BASE	(0x10f00)
#define SSUSB_USB30_PHYA_SIV_B_BASE		(0x10b00)
#define SSUSB_SIFSLV_U3PHYA_DA_BASE		(0x10c00)
#define SSUSB_SIFSLV_SPLLC		(0x10000)

/*port1 refs. +0x800(refer to port0)*/
#define U3P_PORT_OFFSET (0x800)	/*based on port0 */
#define U3P_PHY_BASE(index) ((U3P_PORT_OFFSET) * (index))

#define U3P_IP_PW_CTRL0	(SSUSB_SIFSLV_IPPC_BASE+0x0000)
#define CTRL0_IP_SW_RST			(0x1<<0)

#define U3P_IP_PW_CTRL1	(SSUSB_SIFSLV_IPPC_BASE+0x0004)
#define CTRL1_IP_HOST_PDN		(0x1<<0)

#define U3P_IP_PW_STS1		(SSUSB_SIFSLV_IPPC_BASE+0x0010)
#define STS1_U3_MAC_RST		(0x1 << 16)
#define STS1_SYS125_RST		(0x1 << 10)
#define STS1_REF_RST		(0x1 << 8)
#define STS1_SYSPLL_STABLE	(0x1 << 0)

#define U3P_IP_PW_STS2	(SSUSB_SIFSLV_IPPC_BASE+0x0014)
#define STS2_U2_MAC_RST	(0x1 << 0)

#define U3P_IP_XHCI_CAP	(SSUSB_SIFSLV_IPPC_BASE + 0x24)
#define CAP_U3_PORT_NUM(p)	((p) & 0xff)
#define CAP_U2_PORT_NUM(p)	(((p) >> 8) & 0xff)

#define U3P_U3_CTRL_0P	(SSUSB_SIFSLV_IPPC_BASE+0x0030)
#define CTRL_U3_PORT_HOST_SEL	(0x1<<2)
#define CTRL_U3_PORT_PDN		(0x1<<1)
#define CTRL_U3_PORT_DIS		(0x1<<0)

#define U3P_U2_CTRL_0P	(SSUSB_SIFSLV_IPPC_BASE+0x0050)
#define CTRL_U2_PORT_HOST_SEL	(0x1<<2)
#define CTRL_U2_PORT_PDN		(0x1<<1)
#define CTRL_U2_PORT_DIS		(0x1<<0)

#define U3P_U3_CTRL(p)	(U3P_U3_CTRL_0P + ((p) * 0x08))
#define U3P_U2_CTRL(p)	(U3P_U2_CTRL_0P + ((p) * 0x08))

#define U3P_USBPHYACR5      (SSUSB_SIFSLV_U2PHY_COM_BASE+0x0014)
#define PA5_RG_U2_HSTX_SRCTRL			(0x7<<12)
#define PA5_RG_U2_HSTX_SRCTRL_VAL(x)	((0x7 & (x)) << 12)
#define PA5_RG_U2_HS_100U_U3_EN			(0x1<<11)

#define U3P_USBPHYACR6      (SSUSB_SIFSLV_U2PHY_COM_BASE+0x0018)
#define PA6_RG_U2_ISO_EN			(0x1<<31)
#define PA6_RG_U2_BC11_SW_EN		(0x1<<23)
#define PA6_RG_U2_OTG_VBUSCMP_EN	(0x1<<20)

#define U3P_U2PHYACR4       (SSUSB_SIFSLV_U2PHY_COM_BASE+0x0020)
#define P2C_RG_USB20_GPIO_CTL	(0x1<<9)
#define P2C_USB20_GPIO_MODE		(0x1<<8)
#define P2C_U2_GPIO_CTR_MSK  (P2C_RG_USB20_GPIO_CTL | P2C_USB20_GPIO_MODE)

#define U3P_U2PHYDTM0       (SSUSB_SIFSLV_U2PHY_COM_BASE+0x0068)
#define P2C_FORCE_UART_EN		(0x1<<26)
#define P2C_FORCE_DATAIN		(0x1<<23)
#define P2C_FORCE_DM_PULLDOWN	(0x1<<21)
#define P2C_FORCE_DP_PULLDOWN	(0x1<<20)
#define P2C_FORCE_XCVRSEL		(0x1<<19)
#define P2C_FORCE_SUSPENDM		(0x1<<18)
#define P2C_FORCE_TERMSEL		(0x1<<17)
#define P2C_RG_DATAIN			(0xf<<10)
#define P2C_RG_DATAIN_VAL(x)	((0xf & (x)) << 10)
#define P2C_RG_DMPULLDOWN		(0x1<<7)
#define P2C_RG_DPPULLDOWN		(0x1<<6)
#define P2C_RG_XCVRSEL			(0x3<<4)
#define P2C_RG_XCVRSEL_VAL(x)	((0x3 & (x)) << 4)
#define P2C_RG_SUSPENDM			(0x1<<3)
#define P2C_RG_TERMSEL			(0x1<<2)
#define P2C_DTM0_PART_MASK \
		(P2C_FORCE_DATAIN | P2C_FORCE_DM_PULLDOWN | \
		P2C_FORCE_DP_PULLDOWN | P2C_FORCE_XCVRSEL | \
		P2C_FORCE_TERMSEL | P2C_RG_DMPULLDOWN | \
		P2C_RG_TERMSEL)

#define U3P_U2PHYDTM1       (SSUSB_SIFSLV_U2PHY_COM_BASE+0x006C)
#define P2C_RG_UART_EN		(0x1<<16)
#define P2C_RG_VBUSVALID	(0x1<<5)
#define P2C_RG_SESSEND		(0x1<<4)
#define P2C_RG_AVALID		(0x1<<2)

#define U3P_U3_PHYA_REG0	(SSUSB_USB30_PHYA_SIV_B_BASE+0x0000)
#define P3A_RG_U3_VUSB10_ON			(1<<5)

#define U3P_U3_PHYA_REG6	(SSUSB_USB30_PHYA_SIV_B_BASE+0x0018)
#define P3A_RG_TX_EIDLE_CM			(0xf<<28)
#define P3A_RG_TX_EIDLE_CM_VAL(x)	((0xf & (x)) << 28)

#define U3P_U3_PHYA_REG9	(SSUSB_USB30_PHYA_SIV_B_BASE+0x0024)
#define P3A_RG_RX_DAC_MUX			(0x1f<<1)
#define P3A_RG_RX_DAC_MUX_VAL(x)	((0x1f & (x)) << 1)

#define U3P_U3PHYA_DA_REG0	(SSUSB_SIFSLV_U3PHYA_DA_BASE + 0x0)
#define P3A_RG_XTAL_EXT_EN_U3			(0x3<<10)
#define P3A_RG_XTAL_EXT_EN_U3_VAL(x)	((0x3 & (x)) << 10)

#define U3P_PHYD_CDR1		(SSUSB_SIFSLV_U3PHYD_BASE+0x5c)
#define P3D_RG_CDR_BIR_LTD1				(0x1f<<24)
#define P3D_RG_CDR_BIR_LTD1_VAL(x)		((0x1f & (x)) << 24)
#define P3D_RG_CDR_BIR_LTD0				(0x1f<<8)
#define P3D_RG_CDR_BIR_LTD0_VAL(x)		((0x1f & (x)) << 8)

#define U3P_XTALCTL3		(SSUSB_SIFSLV_SPLLC + 0x18)
#define XC3_RG_U3_XTAL_RX_PWD			(0x1<<9)
#define XC3_RG_U3_FRC_XTAL_RX_PWD		(0x1<<8)

#define U3P_UX_EXIT_LFPS_PARAM	(SSUSB_USB3_MAC_CSR_BASE+0x00A0)
#define RX_UX_EXIT_REF		(0xff<<8)
#define RX_UX_EXIT_REF_VAL	(0x3 << 8)

#define U3P_REF_CLK_PARAM	(SSUSB_USB3_MAC_CSR_BASE+0x00B0)
#define REF_CLK_1000NS		(0xff << 0)
#define REF_CLK_VAL_DEF		(0xa << 0)

#define U3P_LINK_PM_TIMER			(SSUSB_USB3_SYS_CSR_BASE+0x0208)
#define PM_LC_TIMEOUT			(0xf<<0)
#define PM_LC_TIMEOUT_VAL		(0x3 << 0)

#define U3P_TIMING_PULSE_CTRL	(SSUSB_USB3_SYS_CSR_BASE+0x02B4)
#define U3T_CNT_1US			(0xff << 0)
#define U3T_CNT_1US_VAL	(0x3f << 0)	/* 62.5MHz: 63 */

#define U3P_U2_TIMING_PARAM		(SSUSB_USB2_CSR_BASE+0x0040)
#define U2T_VAL_1US			(0xff<<0)
#define U2T_VAL_1US_VAL	(0x3f << 0)	/* 62.5MHz: 63 */


struct mt65xx_u3phy {
	struct usb_phy phy;
	struct device *dev;
	struct regulator *vusb33;
	void __iomem *mac_base;	/* only device-mac regs, exclude xhci's */
	void __iomem *sif_base;	/* include sif & sif2 */
	void __iomem *ap_pll;
	struct clk *scp_sys;
	struct clk *peri_usb0;
	struct clk *peri_usb1;
	int p1_exist;
	int phy_num;
	int p1_gpio_num;
	int p1_gpio_active_low;
	int is_on;
};

#endif				/* __MT65XX_U3PHY_H */
