#ifndef __MUSB_MTK_MUSB_H__
#define __MUSB_MTK_MUSB_H__

/* #include "wakelock.h" */
#include "musb_io.h"

#define DEVICE_INTTERRUPT 1
#define EINT_CHR_DET_NUM 23

#define ID_PULL_UP 0x0101
#define ID_PHY_RESET 0x3d11

struct mt65xx_glue {
	struct device *dev;
	struct platform_device *musb;
};

extern void mt_usb_set_vbus(struct musb *musb, int is_on);
extern int mt_usb_get_vbus_status(struct musb *musb);
extern int ep_config_from_table_for_host(struct musb *musb);
extern irqreturn_t dma_controller_irq(int irq, void *private_data);

/*
MTK Software reset reg
*/
#define MUSB_SWRST 0x74
#define MUSB_SWRST_PHY_RST         (1<<7)
#define MUSB_SWRST_PHYSIG_GATE_HS  (1<<6)
#define MUSB_SWRST_PHYSIG_GATE_EN  (1<<5)
#define MUSB_SWRST_REDUCE_DLY      (1<<4)
#define MUSB_SWRST_UNDO_SRPFIX     (1<<3)
#define MUSB_SWRST_FRC_VBUSVALID   (1<<2)
#define MUSB_SWRST_SWRST           (1<<1)
#define MUSB_SWRST_DISUSBRESET     (1<<0)

#define USB_L1INTS (0x00a0)	/* USB level 1 interrupt status register */
#define USB_L1INTM (0x00a4)	/* USB level 1 interrupt mask register  */
#define USB_L1INTP (0x00a8)	/* USB level 1 interrupt polarity register  */


#define DMA_INTR_UNMASK_CLR_OFFSET (16)
#define DMA_INTR_UNMASK_SET_OFFSET (24)
#define USB_DMA_REALCOUNT(chan) (0x0280+0x10*(chan))


/* ====================== */
/* USB interrupt register */
/* ====================== */

/* word access */
#define TX_INT_STATUS        (1<<0)
#define RX_INT_STATUS        (1<<1)
#define USBCOM_INT_STATUS    (1<<2)
#define DMA_INT_STATUS       (1<<3)
#define PSR_INT_STATUS       (1<<4)
#define QINT_STATUS          (1<<5)
#define QHIF_INT_STATUS      (1<<6)
#define DPDM_INT_STATUS      (1<<7)
#define VBUSVALID_INT_STATUS (1<<8)
#define IDDIG_INT_STATUS     (1<<9)
#define DRVVBUS_INT_STATUS   (1<<10)

#define VBUSVALID_INT_POL    (1<<8)
#define IDDIG_INT_POL        (1<<9)
#define DRVVBUS_INT_POL      (1<<10)


/* battery sync */
extern void BAT_SetUSBState(int usb_state_value);
extern void wake_up_bat(void);
/* usb phy setting */
extern void usb_phy_recover(void);
extern void usb_phy_savecurrent(void);
extern bool usb_enable_clock(bool enable);
extern void usb_phy_poweron(void);

#endif
