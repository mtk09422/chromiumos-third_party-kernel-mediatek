/*
 * Copyright (c) 2014 MediaTek Inc.
 * Author: Chaotian.Jing <chaotian.jing@mediatek.com>
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

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/dma-mapping.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/ioport.h>
#include <linux/irq.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/spinlock.h>
#include <linux/platform_device.h>
#include <linux/workqueue.h>
#include <linux/gpio.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/of_gpio.h>
#include <linux/bitops.h>
#include <linux/regulator/consumer.h>
#include <linux/pm_runtime.h>
#include <linux/clk.h>
#include <linux/err.h>

#include <linux/mmc/card.h>
#include <linux/mmc/core.h>
#include <linux/mmc/host.h>
#include <linux/mmc/mmc.h>
#include <linux/mmc/sd.h>
#include <linux/mmc/sdio.h>

#define MAX_GPD_NUM         (1 + 1)	/* one null gpd */
#define MAX_BD_NUM          (1024)
#define MAX_BD_PER_GPD      (MAX_BD_NUM)

#define MSDC_AUTOCMD12          (0x0001)
#define MSDC_AUTOCMD23          (0x0002)
#define MSDC_AUTOCMD19          (0x0003)

/*--------------------------------------------------------------------------*/
/* Common Definition                                                        */
/*--------------------------------------------------------------------------*/
#define MSDC_FIFO_SZ            (128)
#define MSDC_FIFO_THD           (64)

#define MSDC_MS                 (0)
#define MSDC_SDMMC              (1)

#define MSDC_MODE_UNKNOWN       (0)
#define MSDC_MODE_PIO           (1)
#define MSDC_MODE_DMA_BASIC     (2)
#define MSDC_MODE_DMA_DESC      (3)
#define MSDC_MODE_DMA_ENHANCED  (4)
#define MSDC_MODE_MMC_STREAM    (5)

#define MSDC_BUS_1BITS          (0)
#define MSDC_BUS_4BITS          (1)
#define MSDC_BUS_8BITS          (2)

#define MSDC_BURST_8B           (3)
#define MSDC_BURST_16B          (4)
#define MSDC_BURST_32B          (5)
#define MSDC_BURST_64B          (6)

#define MSDC_EMMC_BOOTMODE0     (0)	/* Pull low CMD mode */
#define MSDC_EMMC_BOOTMODE1     (1)	/* Reset CMD mode */

enum {
	RESP_NONE = 0,
	RESP_R1 = 1,
	RESP_R2 = 2,
	RESP_R3 = 3,
	RESP_R4 = 4,
	RESP_R5 = 1,
	RESP_R6 = 1,
	RESP_R7 = 1,
	RESP_R1B = 7
};

/*--------------------------------------------------------------------------*/
/* Register Offset                                                          */
/*--------------------------------------------------------------------------*/
#define OFFSET_MSDC_CFG         (0x0)
#define OFFSET_MSDC_IOCON       (0x04)
#define OFFSET_MSDC_PS          (0x08)
#define OFFSET_MSDC_INT         (0x0c)
#define OFFSET_MSDC_INTEN       (0x10)
#define OFFSET_MSDC_FIFOCS      (0x14)
#define OFFSET_MSDC_TXDATA      (0x18)
#define OFFSET_MSDC_RXDATA      (0x1c)
#define OFFSET_SDC_CFG          (0x30)
#define OFFSET_SDC_CMD          (0x34)
#define OFFSET_SDC_ARG          (0x38)
#define OFFSET_SDC_STS          (0x3c)
#define OFFSET_SDC_RESP0        (0x40)
#define OFFSET_SDC_RESP1        (0x44)
#define OFFSET_SDC_RESP2        (0x48)
#define OFFSET_SDC_RESP3        (0x4c)
#define OFFSET_SDC_BLK_NUM      (0x50)
#define OFFSET_SDC_CSTS         (0x58)
#define OFFSET_SDC_CSTS_EN      (0x5c)
#define OFFSET_SDC_DCRC_STS     (0x60)
#define OFFSET_EMMC_CFG0        (0x70)
#define OFFSET_EMMC_CFG1        (0x74)
#define OFFSET_EMMC_STS         (0x78)
#define OFFSET_EMMC_IOCON       (0x7c)
#define OFFSET_SDC_ACMD_RESP    (0x80)
#define OFFSET_SDC_ACMD19_TRG   (0x84)
#define OFFSET_SDC_ACMD19_STS   (0x88)
#define OFFSET_MSDC_DMA_SA      (0x90)
#define OFFSET_MSDC_DMA_CA      (0x94)
#define OFFSET_MSDC_DMA_CTRL    (0x98)
#define OFFSET_MSDC_DMA_CFG     (0x9c)
#define OFFSET_MSDC_DBG_SEL     (0xa0)
#define OFFSET_MSDC_DBG_OUT     (0xa4)
#define OFFSET_MSDC_DMA_LEN     (0xa8)
#define OFFSET_MSDC_PATCH_BIT   (0xb0)
#define OFFSET_MSDC_PATCH_BIT1  (0xb4)
#define OFFSET_MSDC_PAD_CTL0    (0xe0)
#define OFFSET_MSDC_PAD_CTL1    (0xe4)
#define OFFSET_MSDC_PAD_CTL2    (0xe8)
#define OFFSET_MSDC_PAD_TUNE    (0xec)
#define OFFSET_MSDC_DAT_RDDLY0  (0xf0)
#define OFFSET_MSDC_DAT_RDDLY1  (0xf4)
#define OFFSET_MSDC_HW_DBG      (0xf8)
#define OFFSET_MSDC_VERSION     (0x100)
#define OFFSET_MSDC_ECO_VER     (0x104)

/*--------------------------------------------------------------------------*/
/* Register Address                                                         */
/*--------------------------------------------------------------------------*/

/* common register */
#define MSDC_CFG(x)             ((x) + OFFSET_MSDC_CFG)
#define MSDC_IOCON(x)           ((x) + OFFSET_MSDC_IOCON)
#define MSDC_PS(x)              ((x) + OFFSET_MSDC_PS)
#define MSDC_INT(x)             ((x) + OFFSET_MSDC_INT)
#define MSDC_INTEN(x)           ((x) + OFFSET_MSDC_INTEN)
#define MSDC_FIFOCS(x)          ((x) + OFFSET_MSDC_FIFOCS)
#define MSDC_TXDATA(x)          ((x) + OFFSET_MSDC_TXDATA)
#define MSDC_RXDATA(x)          ((x) + OFFSET_MSDC_RXDATA)

/* sdmmc register */
#define SDC_CFG(x)              ((x) + OFFSET_SDC_CFG)
#define SDC_CMD(x)              ((x) + OFFSET_SDC_CMD)
#define SDC_ARG(x)              ((x) + OFFSET_SDC_ARG)
#define SDC_STS(x)              ((x) + OFFSET_SDC_STS)
#define SDC_RESP0(x)            ((x) + OFFSET_SDC_RESP0)
#define SDC_RESP1(x)            ((x) + OFFSET_SDC_RESP1)
#define SDC_RESP2(x)            ((x) + OFFSET_SDC_RESP2)
#define SDC_RESP3(x)            ((x) + OFFSET_SDC_RESP3)
#define SDC_BLK_NUM(x)          ((x) + OFFSET_SDC_BLK_NUM)
#define SDC_CSTS(x)             ((x) + OFFSET_SDC_CSTS)
#define SDC_CSTS_EN(x)          ((x) + OFFSET_SDC_CSTS_EN)
#define SDC_DCRC_STS(x)         ((x) + OFFSET_SDC_DCRC_STS)

/* emmc register*/
#define EMMC_CFG0(x)            ((x) + OFFSET_EMMC_CFG0)
#define EMMC_CFG1(x)            ((x) + OFFSET_EMMC_CFG1)
#define EMMC_STS(x)             ((x) + OFFSET_EMMC_STS)
#define EMMC_IOCON(x)           ((x) + OFFSET_EMMC_IOCON)

/* auto command register */
#define SDC_ACMD_RESP(x)        ((x) + OFFSET_SDC_ACMD_RESP)
#define SDC_ACMD19_TRG(x)       ((x) + OFFSET_SDC_ACMD19_TRG)
#define SDC_ACMD19_STS(x)       ((x) + OFFSET_SDC_ACMD19_STS)

/* dma register */
#define MSDC_DMA_SA(x)          ((x) + OFFSET_MSDC_DMA_SA)
#define MSDC_DMA_CA(x)          ((x) + OFFSET_MSDC_DMA_CA)
#define MSDC_DMA_CTRL(x)        ((x) + OFFSET_MSDC_DMA_CTRL)
#define MSDC_DMA_CFG(x)         ((x) + OFFSET_MSDC_DMA_CFG)
#define MSDC_DMA_LEN(x)         ((x) + OFFSET_MSDC_DMA_LEN)

/* pad ctrl register */
#define MSDC_PAD_CTL0(x)        ((x) + OFFSET_MSDC_PAD_CTL0)
#define MSDC_PAD_CTL1(x)        ((x) + OFFSET_MSDC_PAD_CTL1)
#define MSDC_PAD_CTL2(x)        ((x) + OFFSET_MSDC_PAD_CTL2)

/* data read delay */
#define MSDC_DAT_RDDLY0(x)      ((x) + OFFSET_MSDC_DAT_RDDLY0)
#define MSDC_DAT_RDDLY1(x)      ((x) + OFFSET_MSDC_DAT_RDDLY1)

/* debug register */
#define MSDC_DBG_SEL(x)         ((x) + OFFSET_MSDC_DBG_SEL)
#define MSDC_DBG_OUT(x)         ((x) + OFFSET_MSDC_DBG_OUT)

/* misc register */
#define MSDC_PATCH_BIT0(x)      ((x) + OFFSET_MSDC_PATCH_BIT)
#define MSDC_PATCH_BIT1(x)      ((x) + OFFSET_MSDC_PATCH_BIT1)
#define MSDC_PAD_TUNE(x)        ((x) + OFFSET_MSDC_PAD_TUNE)
#define MSDC_HW_DBG(x)          ((x) + OFFSET_MSDC_HW_DBG)
#define MSDC_VERSION(x)         ((x) + OFFSET_MSDC_VERSION)
#define MSDC_ECO_VER(x)         ((x) + OFFSET_MSDC_ECO_VER)

/*--------------------------------------------------------------------------*/
/* Register Mask                                                            */
/*--------------------------------------------------------------------------*/

/* MSDC_CFG mask */
#define MSDC_CFG_MODE           (0x1  << 0)	/* RW */
#define MSDC_CFG_CKPDN          (0x1  << 1)	/* RW */
#define MSDC_CFG_RST            (0x1  << 2)	/* RW */
#define MSDC_CFG_PIO            (0x1  << 3)	/* RW */
#define MSDC_CFG_CKDRVEN        (0x1  << 4)	/* RW */
#define MSDC_CFG_BV18SDT        (0x1  << 5)	/* RW */
#define MSDC_CFG_BV18PSS        (0x1  << 6)	/* R  */
#define MSDC_CFG_CKSTB          (0x1  << 7)	/* R  */
#define MSDC_CFG_CKDIV          (0xff << 8)	/* RW */
#define MSDC_CFG_CKMOD          (0x3  << 16)	/* RW */

/* MSDC_IOCON mask */
#define MSDC_IOCON_SDR104CKS    (0x1  << 0)	/* RW */
#define MSDC_IOCON_RSPL         (0x1  << 1)	/* RW */
#define MSDC_IOCON_DSPL         (0x1  << 2)	/* RW */
#define MSDC_IOCON_DDLSEL       (0x1  << 3)	/* RW */
#define MSDC_IOCON_DDR50CKD     (0x1  << 4)	/* RW */
#define MSDC_IOCON_DSPLSEL      (0x1  << 5)	/* RW */
#define MSDC_IOCON_W_DSPL       (0x1  << 8)	/* RW */
#define MSDC_IOCON_D0SPL        (0x1  << 16)	/* RW */
#define MSDC_IOCON_D1SPL        (0x1  << 17)	/* RW */
#define MSDC_IOCON_D2SPL        (0x1  << 18)	/* RW */
#define MSDC_IOCON_D3SPL        (0x1  << 19)	/* RW */
#define MSDC_IOCON_D4SPL        (0x1  << 20)	/* RW */
#define MSDC_IOCON_D5SPL        (0x1  << 21)	/* RW */
#define MSDC_IOCON_D6SPL        (0x1  << 22)	/* RW */
#define MSDC_IOCON_D7SPL        (0x1  << 23)	/* RW */
#define MSDC_IOCON_RISCSZ       (0x3  << 24)	/* RW */

/* MSDC_PS mask */
#define MSDC_PS_CDEN            (0x1  << 0)	/* RW */
#define MSDC_PS_CDSTS           (0x1  << 1)	/* R  */
#define MSDC_PS_CDDEBOUNCE      (0xf  << 12)	/* RW */
#define MSDC_PS_DAT             (0xff << 16)	/* R  */
#define MSDC_PS_CMD             (0x1  << 24)	/* R  */
#define MSDC_PS_WP              (0x1UL << 31)	/* R  */

/* MSDC_INT mask */
#define MSDC_INT_MMCIRQ         (0x1  << 0)	/* W1C */
#define MSDC_INT_CDSC           (0x1  << 1)	/* W1C */
#define MSDC_INT_ACMDRDY        (0x1  << 3)	/* W1C */
#define MSDC_INT_ACMDTMO        (0x1  << 4)	/* W1C */
#define MSDC_INT_ACMDCRCERR     (0x1  << 5)	/* W1C */
#define MSDC_INT_DMAQ_EMPTY     (0x1  << 6)	/* W1C */
#define MSDC_INT_SDIOIRQ        (0x1  << 7)	/* W1C */
#define MSDC_INT_CMDRDY         (0x1  << 8)	/* W1C */
#define MSDC_INT_CMDTMO         (0x1  << 9)	/* W1C */
#define MSDC_INT_RSPCRCERR      (0x1  << 10)	/* W1C */
#define MSDC_INT_CSTA           (0x1  << 11)	/* R */
#define MSDC_INT_XFER_COMPL     (0x1  << 12)	/* W1C */
#define MSDC_INT_DXFER_DONE     (0x1  << 13)	/* W1C */
#define MSDC_INT_DATTMO         (0x1  << 14)	/* W1C */
#define MSDC_INT_DATCRCERR      (0x1  << 15)	/* W1C */
#define MSDC_INT_ACMD19_DONE    (0x1  << 16)	/* W1C */
#define MSDC_INT_DMA_BDCSERR    (0x1  << 17)	/* W1C */
#define MSDC_INT_DMA_GPDCSERR   (0x1  << 18)	/* W1C */
#define MSDC_INT_DMA_PROTECT    (0x1  << 19)	/* W1C */

/* MSDC_INTEN mask */
#define MSDC_INTEN_MMCIRQ       (0x1  << 0)	/* RW */
#define MSDC_INTEN_CDSC         (0x1  << 1)	/* RW */
#define MSDC_INTEN_ACMDRDY      (0x1  << 3)	/* RW */
#define MSDC_INTEN_ACMDTMO      (0x1  << 4)	/* RW */
#define MSDC_INTEN_ACMDCRCERR   (0x1  << 5)	/* RW */
#define MSDC_INTEN_DMAQ_EMPTY   (0x1  << 6)	/* RW */
#define MSDC_INTEN_SDIOIRQ      (0x1  << 7)	/* RW */
#define MSDC_INTEN_CMDRDY       (0x1  << 8)	/* RW */
#define MSDC_INTEN_CMDTMO       (0x1  << 9)	/* RW */
#define MSDC_INTEN_RSPCRCERR    (0x1  << 10)	/* RW */
#define MSDC_INTEN_CSTA         (0x1  << 11)	/* RW */
#define MSDC_INTEN_XFER_COMPL   (0x1  << 12)	/* RW */
#define MSDC_INTEN_DXFER_DONE   (0x1  << 13)	/* RW */
#define MSDC_INTEN_DATTMO       (0x1  << 14)	/* RW */
#define MSDC_INTEN_DATCRCERR    (0x1  << 15)	/* RW */
#define MSDC_INTEN_ACMD19_DONE  (0x1  << 16)	/* RW */
#define MSDC_INTEN_DMA_BDCSERR  (0x1  << 17)	/* RW */
#define MSDC_INTEN_DMA_GPDCSERR (0x1  << 18)	/* RW */
#define MSDC_INTEN_DMA_PROTECT  (0x1  << 19)	/* RW */

/* MSDC_FIFOCS mask */
#define MSDC_FIFOCS_RXCNT       (0xff << 0)	/* R */
#define MSDC_FIFOCS_TXCNT       (0xff << 16)	/* R */
#define MSDC_FIFOCS_CLR         (0x1UL << 31)	/* RW */

/* SDC_CFG mask */
#define SDC_CFG_SDIOINTWKUP     (0x1  << 0)	/* RW */
#define SDC_CFG_INSWKUP         (0x1  << 1)	/* RW */
#define SDC_CFG_BUSWIDTH        (0x3  << 16)	/* RW */
#define SDC_CFG_SDIO            (0x1  << 19)	/* RW */
#define SDC_CFG_SDIOIDE         (0x1  << 20)	/* RW */
#define SDC_CFG_INTATGAP        (0x1  << 21)	/* RW */
#define SDC_CFG_DTOC            (0xffUL << 24)	/* RW */

/* SDC_CMD mask */
#define SDC_CMD_OPC             (0x3f << 0)	/* RW */
#define SDC_CMD_BRK             (0x1  << 6)	/* RW */
#define SDC_CMD_RSPTYP          (0x7  << 7)	/* RW */
#define SDC_CMD_DTYP            (0x3  << 11)	/* RW */
#define SDC_CMD_RW              (0x1  << 13)	/* RW */
#define SDC_CMD_STOP            (0x1  << 14)	/* RW */
#define SDC_CMD_GOIRQ           (0x1  << 15)	/* RW */
#define SDC_CMD_BLKLEN          (0xfff << 16)	/* RW */
#define SDC_CMD_AUTOCMD         (0x3  << 28)	/* RW */
#define SDC_CMD_VOLSWTH         (0x1  << 30)	/* RW */

/* SDC_STS mask */
#define SDC_STS_SDCBUSY         (0x1  << 0)	/* RW */
#define SDC_STS_CMDBUSY         (0x1  << 1)	/* RW */
#define SDC_STS_SWR_COMPL       (0x1  << 31)	/* RW */

/* SDC_DCRC_STS mask */
#define SDC_DCRC_STS_NEG        (0xff << 8)	/* RO */
#define SDC_DCRC_STS_POS        (0xff << 0)	/* RO */

/* EMMC_CFG0 mask */
#define EMMC_CFG0_BOOTSTART     (0x1  << 0)	/* W */
#define EMMC_CFG0_BOOTSTOP      (0x1  << 1)	/* W */
#define EMMC_CFG0_BOOTMODE      (0x1  << 2)	/* RW */
#define EMMC_CFG0_BOOTACKDIS    (0x1  << 3)	/* RW */
#define EMMC_CFG0_BOOTWDLY      (0x7  << 12)	/* RW */
#define EMMC_CFG0_BOOTSUPP      (0x1  << 15)	/* RW */

/* EMMC_CFG1 mask */
#define EMMC_CFG1_BOOTDATTMC    (0xfffff << 0)	/* RW */
#define EMMC_CFG1_BOOTACKTMC    (0xfffUL << 20)	/* RW */

/* EMMC_STS mask */
#define EMMC_STS_BOOTCRCERR     (0x1  << 0)	/* W1C */
#define EMMC_STS_BOOTACKERR     (0x1  << 1)	/* W1C */
#define EMMC_STS_BOOTDATTMO     (0x1  << 2)	/* W1C */
#define EMMC_STS_BOOTACKTMO     (0x1  << 3)	/* W1C */
#define EMMC_STS_BOOTUPSTATE    (0x1  << 4)	/* R */
#define EMMC_STS_BOOTACKRCV     (0x1  << 5)	/* W1C */
#define EMMC_STS_BOOTDATRCV     (0x1  << 6)	/* R */

/* EMMC_IOCON mask */
#define EMMC_IOCON_BOOTRST      (0x1  << 0)	/* RW */

/* SDC_ACMD19_TRG mask */
#define SDC_ACMD19_TRG_TUNESEL  (0xf  << 0)	/* RW */

/* MSDC_DMA_CTRL mask */
#define MSDC_DMA_CTRL_START     (0x1  << 0)	/* W */
#define MSDC_DMA_CTRL_STOP      (0x1  << 1)	/* W */
#define MSDC_DMA_CTRL_RESUME    (0x1  << 2)	/* W */
#define MSDC_DMA_CTRL_MODE      (0x1  << 8)	/* RW */
#define MSDC_DMA_CTRL_LASTBUF   (0x1  << 10)	/* RW */
#define MSDC_DMA_CTRL_BRUSTSZ   (0x7  << 12)	/* RW */

/* MSDC_DMA_CFG mask */
#define MSDC_DMA_CFG_STS        (0x1  << 0)	/* R */
#define MSDC_DMA_CFG_DECSEN     (0x1  << 1)	/* RW */
#define MSDC_DMA_CFG_AHBHPROT2  (0x2  << 8)	/* RW */
#define MSDC_DMA_CFG_ACTIVEEN   (0x2  << 12)	/* RW */
#define MSDC_DMA_CFG_CS12B16B   (0x1  << 16)	/* RW */

/* MSDC_PATCH_BIT mask */
#define MSDC_PATCH_BIT_ODDSUPP    (0x1  <<  1)	/* RW */
#define MSDC_INT_DAT_LATCH_CK_SEL (0x7  <<  7)
#define MSDC_CKGEN_MSDC_DLY_SEL   (0x1F << 10)
#define MSDC_PATCH_BIT_IODSSEL    (0x1  << 16)	/* RW */
#define MSDC_PATCH_BIT_IOINTSEL   (0x1  << 17)	/* RW */
#define MSDC_PATCH_BIT_BUSYDLY    (0xf  << 18)	/* RW */
#define MSDC_PATCH_BIT_WDOD       (0xf  << 22)	/* RW */
#define MSDC_PATCH_BIT_IDRTSEL    (0x1  << 26)	/* RW */
#define MSDC_PATCH_BIT_CMDFSEL    (0x1  << 27)	/* RW */
#define MSDC_PATCH_BIT_INTDLSEL   (0x1  << 28)	/* RW */
#define MSDC_PATCH_BIT_SPCPUSH    (0x1  << 29)	/* RW */
#define MSDC_PATCH_BIT_DECRCTMO   (0x1  << 30)	/* RW */

/* MSDC_PATCH_BIT1 mask */
#define MSDC_PATCH_BIT1_WRDAT_CRCS  (0x7 << 0)
#define MSDC_PATCH_BIT1_CMD_RSP     (0x7 << 3)
#define MSDC_PATCH_BIT1_GET_CRC_MARGIN	(0x01 << 7) /* RW */

/* MSDC_PAD_CTL0 mask */
#define MSDC_PAD_CTL0_CLKDRVN   (0x7  << 0)	/* RW */
#define MSDC_PAD_CTL0_CLKDRVP   (0x7  << 4)	/* RW */
#define MSDC_PAD_CTL0_CLKSR     (0x1  << 8)	/* RW */
#define MSDC_PAD_CTL0_CLKPD     (0x1  << 16)	/* RW */
#define MSDC_PAD_CTL0_CLKPU     (0x1  << 17)	/* RW */
#define MSDC_PAD_CTL0_CLKSMT    (0x1  << 18)	/* RW */
#define MSDC_PAD_CTL0_CLKIES    (0x1  << 19)	/* RW */
#define MSDC_PAD_CTL0_CLKTDSEL  (0xf  << 20)	/* RW */
#define MSDC_PAD_CTL0_CLKRDSEL  (0xffUL << 24)	/* RW */

/* MSDC_PAD_CTL1 mask */
#define MSDC_PAD_CTL1_CMDDRVN   (0x7  << 0)	/* RW */
#define MSDC_PAD_CTL1_CMDDRVP   (0x7  << 4)	/* RW */
#define MSDC_PAD_CTL1_CMDSR     (0x1  << 8)	/* RW */
#define MSDC_PAD_CTL1_CMDPD     (0x1  << 16)	/* RW */
#define MSDC_PAD_CTL1_CMDPU     (0x1  << 17)	/* RW */
#define MSDC_PAD_CTL1_CMDSMT    (0x1  << 18)	/* RW */
#define MSDC_PAD_CTL1_CMDIES    (0x1  << 19)	/* RW */
#define MSDC_PAD_CTL1_CMDTDSEL  (0xf  << 20)	/* RW */
#define MSDC_PAD_CTL1_CMDRDSEL  (0xffUL << 24)	/* RW */

/* MSDC_PAD_CTL2 mask */
#define MSDC_PAD_CTL2_DATDRVN   (0x7  << 0)	/* RW */
#define MSDC_PAD_CTL2_DATDRVP   (0x7  << 4)	/* RW */
#define MSDC_PAD_CTL2_DATSR     (0x1  << 8)	/* RW */
#define MSDC_PAD_CTL2_DATPD     (0x1  << 16)	/* RW */
#define MSDC_PAD_CTL2_DATPU     (0x1  << 17)	/* RW */
#define MSDC_PAD_CTL2_DATIES    (0x1  << 19)	/* RW */
#define MSDC_PAD_CTL2_DATSMT    (0x1  << 18)	/* RW */
#define MSDC_PAD_CTL2_DATTDSEL  (0xf  << 20)	/* RW */
#define MSDC_PAD_CTL2_DATRDSEL  (0xffUL << 24)	/* RW */

/* MSDC_PAD_TUNE mask */
#define MSDC_PAD_TUNE_DATWRDLY  (0x1F << 0)	/* RW */
#define MSDC_PAD_TUNE_DATRRDLY  (0x1F << 8)	/* RW */
#define MSDC_PAD_TUNE_CMDRDLY   (0x1F << 16)	/* RW */
#define MSDC_PAD_TUNE_CMDRRDLY  (0x1FUL << 22)	/* RW */
#define MSDC_PAD_TUNE_CLKTXDLY  (0x1FUL << 27)	/* RW */

/* MSDC_DAT_RDDLY0/1 mask */
#define MSDC_DAT_RDDLY0_D3      (0x1F << 0)	/* RW */
#define MSDC_DAT_RDDLY0_D2      (0x1F << 8)	/* RW */
#define MSDC_DAT_RDDLY0_D1      (0x1F << 16)	/* RW */
#define MSDC_DAT_RDDLY0_D0      (0x1FUL << 24)	/* RW */

#define MSDC_DAT_RDDLY1_D7      (0x1F << 0)	/* RW */
#define MSDC_DAT_RDDLY1_D6      (0x1F << 8)	/* RW */
#define MSDC_DAT_RDDLY1_D5      (0x1F << 16)	/* RW */
#define MSDC_DAT_RDDLY1_D4      (0x1FUL << 24)	/* RW */

#define CARD_READY_FOR_DATA             (1<<8)
#define CARD_CURRENT_STATE(x)           ((x&0x00001E00)>>9)

/*--------------------------------------------------------------------------*/
/* Descriptor Structure                                                     */
/*--------------------------------------------------------------------------*/
struct mt_gpdma_desc {
	u32 first_u32;
#define GPDMA_DESC_HWO		BIT(0)
#define GPDMA_DESC_BDP		BIT(1)
#define GPDMA_DESC_RSV0		(0x3F << 2) /* bit2 ~ bit7 */
#define GPDMA_DESC_CHECKSUM	(0xFF << 8) /* bit8 ~ bit15 */
#define GPDMA_DESC_INT		BIT(16)
#define GPDMA_DESC_RSV1		(0x7FFF << 17) /* bit17 ~ bit31 */
	u32 next;
	u32 ptr;
	u32 second_u32;
#define GPDMA_DESC_BUFLEN	(0xFFFF) /* bit0 ~ bit15 */
#define GPDMA_DESC_EXTLEN	(0xFF << 16) /* bit16 ~ bit23 */
#define GPDMA_DESC_RSV2		(0xFF << 24) /* bit24 ~ bit31 */
	u32 arg;
	u32 blknum;
	u32 cmd;
};

struct mt_bdma_desc {
	u32 first_u32;
#define BDMA_DESC_EOL		BIT(0)
#define BDMA_DESC_RSV0		(0x7F << 1) /* bit1 ~ bit7 */
#define BDMA_DESC_CHECKSUM	(0xFF << 8) /* bit8 ~ bit15 */
#define BDMA_DESC_RSV1		BIT(16)
#define BDMA_DESC_BLKPAD	BIT(17)
#define BDMA_DESC_DWPAD		BIT(18)
#define BDMA_DESC_RSV2		(0x1FFF << 19) /* bit19 ~ bit31 */
	u32 next;
	u32 ptr;
	u32 second_u32;
#define BDMA_DESC_BUFLEN	(0xFFFF) /* bit0 ~ bit15 */
#define BDMA_DESC_RSV3		(0xFF << 16)
};

struct scatterlist_ex {
	u32 cmd;
	u32 arg;
	u32 sglen;
	struct scatterlist *sg;
};

#define DMA_FLAG_NONE       (0x00000000)
#define DMA_FLAG_EN_CHKSUM  (0x00000001)
#define DMA_FLAG_PAD_BLOCK  (0x00000002)
#define DMA_FLAG_PAD_DWORD  (0x00000004)

struct msdc_dma {
	u32 flags;		/* flags */
	u32 xfersz;		/* xfer size in bytes */
	u32 sglen;		/* size of scatter list */
	u32 blklen;		/* block size */
	struct scatterlist *sg;	/* I/O scatter list */
	struct scatterlist_ex *esg;	/* extended I/O scatter list */
	u8 mode;		/* dma mode        */
	u8 burstsz;		/* burst size      */
	u8 intr;		/* dma done interrupt */
	u8 padding;		/* padding */
	u32 cmd;		/* enhanced mode command */
	u32 arg;		/* enhanced mode arg */
	u32 rsp;		/* enhanced mode command response */
	u32 autorsp;		/* auto command response */

	struct mt_gpdma_desc *gpd;		/* pointer to gpd array */
	struct mt_bdma_desc *bd;		/* pointer to bd array */
	dma_addr_t gpd_addr;	/* the physical address of gpd array */
	dma_addr_t bd_addr;	/* the physical address of bd array */
	u32 used_gpd;		/* the number of used gpd elements */
	u32 used_bd;		/* the number of used bd elements */
};

struct last_cmd {
	u8	cmd;
	u8	type;
	u32	arg;
};

enum host_function {
	MSDC_EMMC = 0,
	MSDC_SD = 1,
	MSDC_SDIO = 2
};

struct msdc_host {
	struct device *dev;
	enum host_function host_func;
	struct mmc_host *mmc;	/* mmc structure */
	int cmd_rsp;
	struct last_cmd last_cmd;

	struct regulator *core_power;
	struct regulator *io_power;
	spinlock_t lock;
	struct mmc_request *mrq;
	struct mmc_command *cmd;
	struct mmc_data *data;
	int error;

	void __iomem *base;		/* host base address */

	struct msdc_dma dma;	/* dma channel */

	u32 timeout_ns;		/* data timeout ns */
	u32 timeout_clks;	/* data timeout clks */

	struct msdc_pinctrl *pin_ctl;
	struct pinctrl *pinctrl;
	struct pinctrl_state *pins_default;
	struct pinctrl_state *pins_uhs;
	struct delayed_work req_timeout;
	int irq;		/* host interrupt */

	struct clk *src_clk;	/* msdc source clock */
	u32 mclk;		/* mmc subsystem clock */
	u32 hclk;		/* host clock speed */
	u32 sclk;		/* SD/MS clock speed */
	/* u8 core_power; */		/* core power */
	u8 power_mode;		/* host power mode */
	u8 suspend;		/* host suspended ? */
	u8 app_cmd;		/* for app command */
	u32 app_cmd_arg;
	u8 autocmd;
	bool ddr;
	struct dma_addr *latest_dma_address;
	u32 io_power_state;
	u32 core_power_state;

	void (*power_control)(struct msdc_host *host, u32 on);

	void (*power_switch)(struct msdc_host *host, u32 on);
};

struct dma_addr {
	u32 start_address;
	u32 size;
	u8 end;
	struct dma_addr *next;
};

#define sdr_read8(reg)           readb(reg)
#define sdr_read16(reg)          readw(reg)
#define sdr_read32(reg)          readl(reg)
#define sdr_write8(reg, val)	writeb(val, reg)
#define sdr_write16(reg, val)	writew(val, reg)
#define sdr_write32(reg, val)	writel(val, reg)
static void sdr_set_bits(void __iomem *reg, u32 bs)
{
	u32 val = sdr_read32(reg);

	val |= bs;
	sdr_write32(reg, val);
}

static void sdr_clr_bits(void __iomem *reg, u32 bs)
{
	u32 val = sdr_read32(reg);

	val &= ~(u32)bs;
	sdr_write32(reg, val);
}

static void sdr_set_field(void __iomem *reg, u32 field, u32 val)
{
	unsigned int tv = sdr_read32(reg);

	tv &= ~field;
	tv |= ((val) << (ffs((unsigned int)field) - 1));
	sdr_write32(reg, tv);
}

static void sdr_get_field(void __iomem *reg, u32 field, u32 val)
{
	unsigned int tv = sdr_read32(reg);

	val = ((tv & field) >> (ffs((unsigned int)field) - 1));
}

#define REQ_CMD_EIO  (0x1 << 0)
#define REQ_CMD_TMO  (0x1 << 1)
#define REQ_DAT_ERR  (0x1 << 2)
#define REQ_STOP_EIO (0x1 << 3)
#define REQ_STOP_TMO (0x1 << 4)
#define REQ_CMD_BUSY (0x1 << 5)

#define msdc_txfifocnt(host) \
	((sdr_read32(MSDC_FIFOCS(host->base)) & MSDC_FIFOCS_TXCNT) >> 16)
#define msdc_rxfifocnt(host) \
	((sdr_read32(MSDC_FIFOCS(host->base)) & MSDC_FIFOCS_RXCNT) >> 0)

#define msdc_dma_on(host)       sdr_clr_bits(MSDC_CFG(host->base), MSDC_CFG_PIO)
#define msdc_dma_off(host)      sdr_set_bits(MSDC_CFG(host->base), MSDC_CFG_PIO)

#define sdc_is_busy(host)    \
	(sdr_read32(SDC_STS(host->base)) & SDC_STS_SDCBUSY)
#define sdc_is_cmd_busy(host)  \
	(sdr_read32(SDC_STS(host->base)) & SDC_STS_CMDBUSY)

#define sdc_send_cmd(host, cmd, arg) \
	do { \
		sdr_write32(SDC_ARG(host->base), (arg)); \
		sdr_write32(SDC_CMD(host->base), (cmd)); \
	} while (0)

#define MSDC_PREPARE_FLAG BIT(0)
#define MSDC_ASYNC_FLAG BIT(1)
#define MSDC_MMAP_FLAG BIT(2)

static void msdc_reset_hw(struct msdc_host *host)
{
	u32 val;

	sdr_set_bits(MSDC_CFG(host->base), MSDC_CFG_RST);
	while (sdr_read32(MSDC_CFG(host->base)) & MSDC_CFG_RST)
		cpu_relax();

	sdr_set_bits(MSDC_FIFOCS(host->base), MSDC_FIFOCS_CLR);
	while (sdr_read32(MSDC_FIFOCS(host->base)) & MSDC_FIFOCS_CLR)
		cpu_relax();

	val = sdr_read32(MSDC_INT(host->base));
	sdr_write32(MSDC_INT(host->base), val);
}

#define msdc_irq_save(host, val) \
	do { \
		val = sdr_read32(MSDC_INTEN(host->base)); \
		sdr_clr_bits(MSDC_INTEN(host->base), val); \
	} while (0)

#define msdc_irq_restore(host, val) \
	sdr_set_bits(MSDC_INTEN(host->base), val)

#define HOST_MIN_MCLK       (260000)

#define HOST_MAX_BLKSZ      (2048)

#define MSDC_OCR_AVAIL      (MMC_VDD_28_29 | MMC_VDD_29_30 \
		| MMC_VDD_30_31 | MMC_VDD_31_32 | MMC_VDD_32_33)
/* #define MSDC_OCR_AVAIL      (MMC_VDD_32_33 | MMC_VDD_33_34) */

#define MSDC1_IRQ_SEL       (1 << 9)
#define PDN_REG             (0xF1000010)

#define DEFAULT_DEBOUNCE    (8)	/* 8 cycles */
/* data timeout counter. 65536x40(75/77) /1048576 * 3(83/85) sclk. */
#define DEFAULT_DTOC        (3)

#define MTK_MMC_AUTOSUSPEND_DELAY	(500)
#define CMD_TIMEOUT         (HZ/10 * 5)	/* 100ms x5 */
#define DAT_TIMEOUT         (HZ    * 5)	/* 1000ms x5 */
#define CLK_TIMEOUT         (HZ    * 5)	/* 5s    */
#define POLLING_BUSY		(HZ	   * 3)
/* a single transaction for WIFI may be 50K */
#define MAX_DMA_CNT         (64 * 1024 - 512)

#define MAX_HW_SGMTS        (MAX_BD_NUM)
#define MAX_PHY_SGMTS       (MAX_BD_NUM)
#define MAX_SGMT_SZ         (MAX_DMA_CNT)
#define MAX_REQ_SZ          (512*1024)

#define CMD_TUNE_UHS_MAX_TIME	(2*32*8*8)
#define CMD_TUNE_HS_MAX_TIME	(2*32)

#define READ_TUNE_UHS_CLKMOD1_MAX_TIME	(2*32*32*8)
#define READ_TUNE_UHS_MAX_TIME	(2*32*32)
#define READ_TUNE_HS_MAX_TIME	(2*32)

#define WRITE_TUNE_HS_MAX_TIME	(2*32*32)
#define WRITE_TUNE_UHS_MAX_TIME (2*32*32*8)

#define MSDC_LOWER_FREQ
#define MSDC_MAX_FREQ_DIV   (2)	/* 200 / (4 * 2) */
#define MSDC_MAX_TIMEOUT_RETRY	(1)
#define	MSDC_MAX_POWER_CYCLE	(3)

#define MSDC_CD_PIN_EN      (1 << 0)	/* card detection pin is wired   */
#define MSDC_WP_PIN_EN      (1 << 1)	/* write protection pin is wired */
#define MSDC_RST_PIN_EN     (1 << 2)	/* emmc reset pin is wired       */
#define MSDC_SDIO_IRQ       (1 << 3)	/* use internal sdio irq (bus)   */
#define MSDC_EXT_SDIO_IRQ   (1 << 4)	/* use external sdio irq         */
#define MSDC_REMOVABLE      (1 << 5)	/* removable slot                */
#define MSDC_SYS_SUSPEND    (1 << 6)	/* suspended by system           */
#define MSDC_HIGHSPEED      (1 << 7)	/* high-speed mode support       */
#define MSDC_UHS1           (1 << 8)	/* uhs-1 mode support            */
#define MSDC_DDR            (1 << 9)	/* ddr mode support              */
#define MSDC_INTERNAL_CLK   (1 << 11)	/* Force Internal clock */

#define MSDC_SMPL_RISING    (0)
#define MSDC_SMPL_FALLING   (1)

#define MSDC_CMD_PIN        (0)
#define MSDC_DAT_PIN        (1)
#define MSDC_CD_PIN         (2)
#define MSDC_WP_PIN         (3)
#define MSDC_RST_PIN        (4)

#define VOL_3300	(3300000)
#define VOL_1800	(1800000)
#define CLK_RATE_200MHZ	(200000000UL)
#define DRV_NAME            "mtk-msdc"

#define MMC_CMD23_ARG_SPECIAL (0xFFFF0000)
#define MSDC_CMD_INTS	(MSDC_INTEN_CMDRDY | MSDC_INTEN_RSPCRCERR | \
		MSDC_INTEN_CMDTMO | MSDC_INTEN_ACMDRDY | \
		MSDC_INTEN_ACMDCRCERR | MSDC_INTEN_ACMDTMO)

static void msdc_cmd_next(struct msdc_host *host,
		struct mmc_request *mrq, struct mmc_command *cmd);

static inline u32 msdc_data_ints_mask(struct msdc_host *host)
{
	u32 wints =
		/* data xfer */
		MSDC_INTEN_XFER_COMPL |
		MSDC_INTEN_DATTMO |
		MSDC_INTEN_DATCRCERR |
		/* DMA events */
		MSDC_INTEN_DMA_BDCSERR |
		MSDC_INTEN_DMA_GPDCSERR |
		MSDC_INTEN_DMA_PROTECT;

	/* auto-stop */
	if (host->autocmd == MSDC_AUTOCMD12)
		wints |=
			MSDC_INT_ACMDCRCERR |
			MSDC_INT_ACMDTMO |
			MSDC_INT_ACMDRDY;

	return wints;
}

static inline void msdc_data_ints_enable(struct msdc_host *host)
{
	u32 wints = msdc_data_ints_mask(host);

	sdr_set_bits(MSDC_INTEN(host->base), wints);
}
static inline void msdc_data_ints_disable(struct msdc_host *host)
{
	u32 wints = msdc_data_ints_mask(host);

	sdr_clr_bits(MSDC_INTEN(host->base), wints);
}

static void msdc_dma_start(struct msdc_host *host)
{
	msdc_data_ints_enable(host);
	mb(); /* wait for pending IO to finish */
	sdr_set_field(MSDC_DMA_CTRL(host->base), MSDC_DMA_CTRL_START, 1);

	dev_dbg(host->dev, "DMA start\n");
}

static void msdc_dma_stop(struct msdc_host *host)
{
	dev_dbg(host->dev, "DMA status: 0x%8X\n",
			sdr_read32(MSDC_DMA_CFG(host->base)));

	sdr_set_field(MSDC_DMA_CTRL(host->base), MSDC_DMA_CTRL_STOP, 1);
	while (sdr_read32(MSDC_DMA_CFG(host->base)) & MSDC_DMA_CFG_STS)
		;
	mb(); /* wait for pending IO to finish */

	msdc_data_ints_disable(host);

	dev_dbg(host->dev, "DMA stop\n");
}

static u8 msdc_dma_calcs(u8 *buf, u32 len)
{
	u32 i, sum = 0;

	for (i = 0; i < len; i++)
		sum += buf[i];
	return 0xFF - (u8) sum;
}

static inline void msdc_dma_setup(struct msdc_host *host, struct msdc_dma *dma,
		struct mmc_data *data)
{
	u32 sglen;
	u32 j, num, bdlen;
	u32 dma_address, dma_len;
	u8 blkpad, dwpad, chksum;
	struct scatterlist *sg;
	struct mt_gpdma_desc *gpd;
	struct mt_bdma_desc *bd;

	dma->sg = data->sg;
	dma->flags = DMA_FLAG_EN_CHKSUM; /* DMA_FLAG_NONE; */
	dma->sglen = data->sg_len;
	dma->xfersz = data->blocks * data->blksz;
	dma->burstsz = MSDC_BURST_64B;
	dma->mode = MSDC_MODE_DMA_DESC;
	sglen = dma->sglen;
	sg = dma->sg;

	dev_dbg(host->dev, "DMA mode<%d> sglen<%d> xfersz<%d>",
			dma->mode, dma->sglen, dma->xfersz);

	switch (dma->mode) {
	case MSDC_MODE_DMA_DESC:
		blkpad = 0;
		dwpad = 0;
		chksum = (dma->flags & DMA_FLAG_EN_CHKSUM) ? 1 : 0;

		/* calculate the required number of gpd */
		num = (sglen + MAX_BD_PER_GPD - 1) / MAX_BD_PER_GPD;

		gpd = dma->gpd;
		bd = dma->bd;
		bdlen = sglen;

		/* modify gpd */
		gpd->first_u32 |= GPDMA_DESC_HWO;
		gpd->first_u32 |= GPDMA_DESC_BDP;
		/* need to clear first. use these bits to calc checksum */
		gpd->first_u32 &= ~GPDMA_DESC_CHECKSUM;
		gpd->first_u32 |= (chksum ? msdc_dma_calcs((u8 *) gpd, 16) : 0)
			<< 8;

		/* modify bd */
		for (j = 0; j < bdlen; j++) {
			dma_address = sg_dma_address(sg);
			dma_len = sg_dma_len(sg);

			/* init bd */
			bd[j].first_u32 &= ~BDMA_DESC_BLKPAD;
			bd[j].first_u32 &= ~BDMA_DESC_DWPAD;
			bd[j].ptr = (u32)dma_address;
			bd[j].second_u32 &= ~BDMA_DESC_BUFLEN;
			bd[j].second_u32 |= (dma_len & BDMA_DESC_BUFLEN);

			if (j == bdlen - 1) /* the last bd */
				bd[j].first_u32 |= BDMA_DESC_EOL;
			else
				bd[j].first_u32 &= ~BDMA_DESC_EOL;

			/* checksume need to clear first */
			bd[j].first_u32 &= ~BDMA_DESC_CHECKSUM;
			bd[j].first_u32 |= (chksum ?
				msdc_dma_calcs((u8 *)(&bd[j]), 16) : 0) << 8;
			sg++;
		}

		dma->used_gpd += 2;
		dma->used_bd += bdlen;

		sdr_set_field(MSDC_DMA_CFG(host->base), MSDC_DMA_CFG_DECSEN,
				chksum);
		sdr_set_field(MSDC_DMA_CTRL(host->base), MSDC_DMA_CTRL_BRUSTSZ,
				dma->burstsz);
		sdr_set_field(MSDC_DMA_CTRL(host->base), MSDC_DMA_CTRL_MODE, 1);

		sdr_write32(MSDC_DMA_SA(host->base), (u32) dma->gpd_addr);
		break;

	default:
		break;
	}
}

static void msdc_prepare_data(struct msdc_host *host, struct mmc_request *mrq)
{
	struct mmc_data *data = mrq->data;

	if (!data)
		return;

	if (!(data->host_cookie & MSDC_PREPARE_FLAG)) {
		bool read = (data->flags & MMC_DATA_READ) != 0;

		data->host_cookie |= MSDC_PREPARE_FLAG;
		dma_map_sg(host->dev, data->sg, data->sg_len,
			   read ? DMA_FROM_DEVICE : DMA_TO_DEVICE);
	}
}

static void msdc_unprepare_data(struct msdc_host *host, struct mmc_request *mrq)
{
	struct mmc_data *data = mrq->data;

	if (!data  || (data->host_cookie & MSDC_ASYNC_FLAG))
		return;

	if ((data->host_cookie & MSDC_PREPARE_FLAG)) {
		bool read = (data->flags & MMC_DATA_READ) != 0;

		dma_unmap_sg(host->dev, data->sg, data->sg_len,
			     read ? DMA_FROM_DEVICE : DMA_TO_DEVICE);
		data->host_cookie &= ~MSDC_PREPARE_FLAG;
	}
}

/* clock control primitives */
static void msdc_set_timeout(struct msdc_host *host, u32 ns, u32 clks)
{
	u32 timeout, clk_ns;
	u32 mode = 0;

	host->timeout_ns = ns;
	host->timeout_clks = clks;
	if (host->sclk == 0) {
		timeout = 0;
	} else {
		clk_ns  = 1000000000UL / host->sclk;
		timeout = (ns + clk_ns - 1) / clk_ns + clks;
		/* in 1048576 sclk cycle unit */
		timeout = (timeout + (1 << 20) - 1) >> 20;
		sdr_get_field(MSDC_CFG(host->base), MSDC_CFG_CKMOD, mode);
		/*DDR mode will double the clk cycles for data timeout */
		timeout = mode >= 2 ? timeout * 2 : timeout;
		timeout = timeout > 1 ? timeout - 1 : 0;
		timeout = timeout > 255 ? 255 : timeout;
	}
	sdr_set_field(SDC_CFG(host->base), SDC_CFG_DTOC, timeout);
}

static void msdc_set_mclk(struct msdc_host *host, int ddr, u32 hz)
{
	u32 mode;
	u32 flags;
	u32 div;
	u32 sclk;
	u32 hclk = host->hclk;

	if (!hz) {
		dev_err(host->dev, "set mclk to 0\n");
		host->mclk = 0;
		msdc_reset_hw(host);
		return;
	}

	msdc_irq_save(host, flags);

	if (ddr) { /* may need to modify later */
		mode = 0x2; /* ddr mode and use divisor */
		if (hz >= (hclk >> 2)) {
			div = 0; /* mean div = 1/4 */
			sclk = hclk >> 2; /* sclk = clk / 4 */
		} else {
			div = (hclk + ((hz << 2) - 1)) / (hz << 2);
			sclk = (hclk >> 2) / div;
			div = (div >> 1);
		}
	} else if (hz >= hclk) {
		mode = 0x1; /* no divisor */
		div = 0;
		sclk = hclk;
	} else {
		mode = 0x0; /* use divisor */
		if (hz >= (hclk >> 1)) {
			div = 0; /* mean div = 1/2 */
			sclk = hclk >> 1; /* sclk = clk / 2 */
		} else {
			div = (hclk + ((hz << 2) - 1)) / (hz << 2);
			sclk = (hclk >> 2) / div;
		}
	}

	sdr_set_field(MSDC_CFG(host->base), MSDC_CFG_CKMOD | MSDC_CFG_CKDIV,
			(mode << 8) | (div % 0xff));
	while (!(sdr_read32(MSDC_CFG(host->base)) & MSDC_CFG_CKSTB))
		cpu_relax();

	host->sclk = sclk;
	host->mclk = hz;
	host->ddr = ddr;
	/* need because clk changed. */
	msdc_set_timeout(host, host->timeout_ns, host->timeout_clks);
	msdc_irq_restore(host, flags);

	dev_dbg(host->dev, "sclk: %d, ddr: %d\n", host->sclk, ddr);
}

#ifdef CONFIG_PM_RUNTIME
static void msdc_clksrc_on(struct msdc_host *host)
{
	u32 div, mode;

	clk_enable(host->src_clk);
	udelay(10);
	/* enable SD/MMC/SDIO bus clock:
	 * it will be automatically gated when the bus is idle
	 * (set MSDC_CFG_CKPDN bit to have it always on)
	 */
	sdr_set_field(MSDC_CFG(host->base), MSDC_CFG_MODE, MSDC_SDMMC);

	sdr_get_field(MSDC_CFG(host->base), MSDC_CFG_CKMOD, mode);
	sdr_get_field(MSDC_CFG(host->base), MSDC_CFG_CKDIV, div);
	while (!(sdr_read32(MSDC_CFG(host->base)) & MSDC_CFG_CKSTB))
		cpu_relax();
}

static void msdc_clksrc_off(struct msdc_host *host)
{
	/* disable SD/MMC/SDIO bus clock */
	sdr_set_field(MSDC_CFG(host->base), MSDC_CFG_MODE, MSDC_MS);
	/* turn off SDHC functional clock */
	clk_disable(host->src_clk);
}

static int msdc_gate_clock(struct msdc_host *host)
{
	int ret = 0;
	unsigned long flags;

	spin_lock_irqsave(&host->lock, flags);
	msdc_clksrc_off(host);
	spin_unlock_irqrestore(&host->lock, flags);
	return ret;
}

static void msdc_ungate_clock(struct msdc_host *host)
{
	unsigned long flags;

	spin_lock_irqsave(&host->lock, flags);
	msdc_clksrc_on(host);
	spin_unlock_irqrestore(&host->lock, flags);
}
#endif

static u32 msdc_ldo_power(struct regulator *reg_name, u32 on, u32 volt_value)
{
	int ret = 0;

	if (on)	 {
		ret = regulator_set_voltage(reg_name, volt_value, volt_value);
		if (ret) {
			pr_err("Failed to set regulator!");
			return ret;
		}
		ret = regulator_enable(reg_name);
		if (ret) {
			pr_err("Failed to enable regulator!");
			return ret;
		}
	} else {
		ret = regulator_disable(reg_name);
		if (ret) {
			pr_err("Failed to disable regulator!");
			return ret;
		}
	}

	return ret;
}

static void msdc_emmc_power(struct msdc_host *host, u32 on)
{
	msdc_ldo_power(host->core_power, on, VOL_3300);
}

static void msdc_sd_power(struct msdc_host *host, u32 on)
{
	msdc_ldo_power(host->io_power, on, VOL_3300);
	msdc_ldo_power(host->core_power, on, VOL_3300);
}

static void msdc_sd_power_switch(struct msdc_host *host, u32 on)
{
	msdc_ldo_power(host->io_power, on, VOL_1800);
}

static void msdc_set_power_mode(struct msdc_host *host, u8 mode)
{
	dev_dbg(host->dev, "Set power mode(%d)", mode);
	if (host->power_mode == MMC_POWER_OFF && mode != MMC_POWER_OFF) {
		if (host->power_control)
			host->power_control(host, 1);

		mdelay(10);
	} else if (host->power_mode != MMC_POWER_OFF && mode == MMC_POWER_OFF) {

		if (host->power_control)
			host->power_control(host, 0);

		mdelay(10);
	}
	host->power_mode = mode;
}

static inline u32 msdc_cmd_find_resp(struct msdc_host *host,
		struct mmc_request *mrq, struct mmc_command *cmd)
{
	u32 resp;

	switch (mmc_resp_type(cmd)) {
		/* Actually, R1, R5, R6, R7 are the same */
	case MMC_RSP_R1:
		resp = RESP_R1;
		break;
	case MMC_RSP_R1B:
		resp = RESP_R1B;
		break;
	case MMC_RSP_R2:
		resp = RESP_R2;
		break;
	case MMC_RSP_R3:
		resp = RESP_R3;
		break;
	case MMC_RSP_NONE:
	default:
		resp = RESP_NONE;
		break;
	}

	return resp;
}

static inline u32 msdc_cmd_prepare_raw_cmd(struct msdc_host *host,
		struct mmc_request *mrq, struct mmc_command *cmd)
{
	/* rawcmd :
	 * vol_swt << 30 | auto_cmd << 28 | blklen << 16 | go_irq << 15 |
	 * stop << 14 | rw << 13 | dtype << 11 | rsptyp << 7 | brk << 6 | opcode
	 */
	u32 opcode = cmd->opcode;
	u32 resp = msdc_cmd_find_resp(host, mrq, cmd);
	u32 rawcmd = (opcode & 0x3F) | ((resp & 7) << 7);

	host->cmd_rsp = resp;

	if ((opcode == SD_IO_RW_DIRECT && cmd->flags == (unsigned int) -1) ||
		opcode == MMC_STOP_TRANSMISSION)
		rawcmd |= (1 << 14);
	else if (opcode == SD_SWITCH_VOLTAGE)
		rawcmd |= (1 << 30);
	else if ((opcode == SD_APP_SEND_SCR) ||
		  (opcode == SD_APP_SEND_NUM_WR_BLKS) ||
		  (opcode == SD_SWITCH &&
		  (mmc_cmd_type(cmd) == MMC_CMD_ADTC)) ||
		  (opcode == SD_APP_SD_STATUS &&
		  (mmc_cmd_type(cmd) == MMC_CMD_ADTC)) ||
		  (opcode == MMC_SEND_EXT_CSD &&
		  (mmc_cmd_type(cmd) == MMC_CMD_ADTC)))
		rawcmd |= (1 << 11);

	if (cmd->data) {
		struct mmc_data *data = cmd->data;

		if (mmc_op_multi(opcode)) {
			if (host->autocmd == MSDC_AUTOCMD12 ||
			    (host->autocmd == MSDC_AUTOCMD23 &&
			    mrq->sbc &&
			    !(mrq->sbc->arg & MMC_CMD23_ARG_SPECIAL)))
				rawcmd |= (host->autocmd & 3) << 28;
		}

		rawcmd |= ((data->blksz & 0xFFF) << 16);
		if (data->flags & MMC_DATA_WRITE)
			rawcmd |= (1 << 13);
		if (data->blocks > 1)
			rawcmd |= (2 << 11);
		else
			rawcmd |= (1 << 11);

		msdc_dma_on(host);

		if (((host->timeout_ns != data->timeout_ns) ||
		    (host->timeout_clks != data->timeout_clks))) {
			msdc_set_timeout(host, data->timeout_ns,
					data->timeout_clks);
		}

		sdr_write32(SDC_BLK_NUM(host->base), data->blocks);
	}
	return rawcmd;
}

static inline void msdc_set_cmd_timeout(struct msdc_host *host,
		struct mmc_command *cmd)
{
	mod_delayed_work(system_wq, &host->req_timeout, DAT_TIMEOUT);
}

static inline void msdc_set_data_timeout(struct msdc_host *host,
		struct mmc_command *cmd, struct mmc_data *data)
{
	mod_delayed_work(system_wq, &host->req_timeout, DAT_TIMEOUT);
}


static void msdc_start_data(struct msdc_host *host, struct mmc_request *mrq,
			    struct mmc_command *cmd, struct mmc_data *data)
{
	bool read;
	unsigned long flags;

	spin_lock_irqsave(&host->lock, flags);
	if (!host->data)
		host->data = data;
	else {
		dev_err(host->dev, "%s: CMD%d [%d blocks] is active",
				__func__, host->mrq->cmd->opcode,
				host->data->blocks);
		dev_err(host->dev, "but new CMD%d [%d blocks] started\n",
			cmd->opcode, data->blocks);
		BUG();
	}
	spin_unlock_irqrestore(&host->lock, flags);
	read = (data->flags & MMC_DATA_READ) != 0;
	data->error = 0;
	if (data->stop)
		data->stop->error = 0;

	msdc_set_data_timeout(host, cmd, data);
	msdc_dma_setup(host, &host->dma, data);
	msdc_dma_start(host);
	dev_dbg(host->dev, "%s: cmd=%d DMA data: %d blocks; read=%d\n",
			__func__, cmd->opcode, data->blocks, read);
}

static int msdc_auto_cmd_done(struct msdc_host *host, int events,
		struct mmc_command *cmd)
{
	u32 *rsp = &cmd->resp[0];

	rsp[0] = sdr_read32(SDC_ACMD_RESP(host->base));

	if (events & MSDC_INT_ACMDRDY)
		cmd->error = 0;
	else {
		msdc_reset_hw(host);
		if (events & MSDC_INT_ACMDCRCERR) {
			cmd->error = (unsigned int) -EILSEQ;
			host->error |= REQ_STOP_EIO;
		} else if (events & MSDC_INT_ACMDTMO) {
			cmd->error = (unsigned int) -ETIMEDOUT;
			host->error |= REQ_STOP_TMO;
		}
		dev_err(host->dev,
			"%s: AUTO_CMD%d arg=%08X; rsp %08X; cmd_error=%d\n",
			__func__, cmd->opcode, cmd->arg, rsp[0],
			(int)cmd->error);
	}
	return (int)(cmd->error);
}

static void msdc_track_cmd_data(struct msdc_host *host,
				struct mmc_command *cmd, struct mmc_data *data)
{
	if (host->error)
		dev_err(host->dev, "%s: cmd=%d arg=%08X; host->error=0x%08X\n",
			__func__, cmd->opcode, cmd->arg, host->error);
}

static void msdc_request_done(struct msdc_host *host, struct mmc_request *mrq)
{
	unsigned long flags;

	spin_lock_irqsave(&host->lock, flags);
	cancel_delayed_work(&host->req_timeout);
	host->mrq = NULL;
	spin_unlock_irqrestore(&host->lock, flags);

	msdc_track_cmd_data(host, mrq->cmd, mrq->data);
	msdc_unprepare_data(host, mrq);
	mmc_request_done(host->mmc, mrq);
}

/* returns true if command is fully handled; returns false otherwise */
static bool msdc_cmd_done(struct msdc_host *host, int events,
			  struct mmc_request *mrq, struct mmc_command *cmd)
{
	bool done = false;
	bool sbc_error;

	if (mrq->sbc && cmd == mrq->cmd &&
	    (events & (MSDC_INT_ACMDRDY | MSDC_INT_ACMDCRCERR
		       | MSDC_INT_ACMDTMO)))
		msdc_auto_cmd_done(host, events, mrq->sbc);

	sbc_error = mrq->sbc && mrq->sbc->error;

	if (sbc_error || (events & (MSDC_INT_CMDRDY
					| MSDC_INT_RSPCRCERR
					| MSDC_INT_CMDTMO))) {
		unsigned long flags;
		u32 *rsp = &cmd->resp[0];

		spin_lock_irqsave(&host->lock, flags);
		done = !host->cmd;
		host->cmd = NULL;
		spin_unlock_irqrestore(&host->lock, flags);

		if (done)
			return true;

		sdr_clr_bits(MSDC_INTEN(host->base), MSDC_CMD_INTS);

		switch (host->cmd_rsp) {
		case RESP_NONE:
			break;
		case RESP_R2:
			rsp[0] = sdr_read32(SDC_RESP3(host->base));
			rsp[1] = sdr_read32(SDC_RESP2(host->base));
			rsp[2] = sdr_read32(SDC_RESP1(host->base));
			rsp[3] = sdr_read32(SDC_RESP0(host->base));
			break;
		default: /* Response types 1, 3, 4, 5, 6, 7(1b) */
			rsp[0] = sdr_read32(SDC_RESP0(host->base));
			break;
		}
		if (!sbc_error && !(events & MSDC_INT_CMDRDY)) {
			msdc_reset_hw(host);
			if (events & MSDC_INT_RSPCRCERR) {
				cmd->error = (unsigned int) -EILSEQ;
				host->error |= REQ_CMD_EIO;
			} else if (events & MSDC_INT_CMDTMO) {
				cmd->error = (unsigned int) -ETIMEDOUT;
				host->error |= REQ_CMD_TMO;
			}
		}
		if (cmd->error)
			dev_err(host->dev,
				"%s: cmd=%d arg=%08X; rsp %08X; cmd_error=%d\n",
				__func__, cmd->opcode, cmd->arg, rsp[0],
				(int)cmd->error);
		if (!sbc_error) {
			host->last_cmd.cmd = cmd->opcode;
			host->last_cmd.type = host->cmd_rsp;
			host->last_cmd.arg = cmd->arg;
		}

		msdc_cmd_next(host, mrq, cmd);
		done = true;
	}
	return done;
}

static inline bool msdc_cmd_is_ready(struct msdc_host *host,
		struct mmc_request *mrq, struct mmc_command *cmd)
{
	unsigned long tmo = jiffies + msecs_to_jiffies(20);

	while (sdc_is_cmd_busy(host) && time_before(jiffies, tmo))
		continue;

	if (sdc_is_cmd_busy(host)) {
		dev_err(host->dev,
			"CMD bus busy detected: last cmd=%d; arg=%08X\n",
			host->last_cmd.cmd, host->last_cmd.arg);
		host->error |= REQ_CMD_BUSY;
		msdc_cmd_done(host, MSDC_INT_CMDTMO, mrq, cmd);
		return false;
	}

	if (mmc_resp_type(cmd) == MMC_RSP_R1B || cmd->data) {
		/* R1B or with data, should check SDCBUSY */
		while (sdc_is_busy(host))
			cpu_relax();

	}
	return true;
}

static void msdc_start_command(struct msdc_host *host,
		struct mmc_request *mrq, struct mmc_command *cmd)
{
	u32 rawcmd;
	unsigned long flags;

	spin_lock_irqsave(&host->lock, flags);
	if (!host->cmd)
		host->cmd = cmd;
	else {
		dev_err(host->dev, "%s: CMD%d is active, but new CMD%d is sent\n",
				__func__, host->cmd->opcode, cmd->opcode);
		BUG();
	}
	spin_unlock_irqrestore(&host->lock, flags);

	if (!msdc_cmd_is_ready(host, mrq, cmd))
		return;

	if (msdc_txfifocnt(host) || msdc_rxfifocnt(host)) {
		dev_err(host->dev, "TX/RX FIFO non-empty before start of IO. Reset\n");
		msdc_reset_hw(host);
	}

	cmd->error = 0;
	rawcmd = msdc_cmd_prepare_raw_cmd(host, mrq, cmd);
	msdc_set_cmd_timeout(host, cmd);

	dev_dbg(host->dev, "%s: cmd=%d arg=%08X cmd_reg=%08X; has_busy=%d\n",
			__func__, cmd->opcode, cmd->arg, rawcmd,
			host->cmd_rsp == RESP_R1B);

	sdr_set_bits(MSDC_INTEN(host->base), MSDC_CMD_INTS);
	sdc_send_cmd(host, rawcmd, cmd->arg);

}

static void msdc_cmd_next(struct msdc_host *host,
		struct mmc_request *mrq, struct mmc_command *cmd)
{
	if (cmd->error || (mrq->sbc && mrq->sbc->error))
		msdc_request_done(host, mrq);
	else if (cmd == mrq->sbc)
		msdc_start_command(host, mrq, mrq->cmd);
	else if (!cmd->data)
		msdc_request_done(host, mrq);
	else
		msdc_start_data(host, mrq, cmd, cmd->data);
}


static void msdc_ops_request(struct mmc_host *mmc, struct mmc_request *mrq)
{
	unsigned long flags;
	struct msdc_host *host = mmc_priv(mmc);

	host->error = 0;

	spin_lock_irqsave(&host->lock, flags);
	if (!host->mrq)
		host->mrq = mrq;
	else {
		dev_err(host->dev,
			"%s: req [CMD%d] is active, new req [CMD%d] is sent\n",
			__func__, host->mrq->cmd->opcode, mrq->cmd->opcode);
		BUG();
	}
	spin_unlock_irqrestore(&host->lock, flags);

	msdc_prepare_data(host, mrq);

	/* if SBC is required, we have HW option and SW option.
	 * if HW option is enabled, and SBC does not have "special" flags,
	 * use HW option,  otherwise use SW option
	 */
	if (mrq->sbc && (host->autocmd != MSDC_AUTOCMD23 ||
	    (mrq->sbc->arg & MMC_CMD23_ARG_SPECIAL)))
		msdc_start_command(host, mrq, mrq->sbc);
	else
		msdc_start_command(host, mrq, mrq->cmd);
}

static inline void msdc_dma_clear(struct msdc_host *host)
{
	host->dma.used_bd = 0;
	host->dma.used_gpd = 0;
}

static void msdc_pre_req(struct mmc_host *mmc, struct mmc_request *mrq,
		bool is_first_req)
{
	struct msdc_host *host = mmc_priv(mmc);
	struct mmc_command *cmd = mrq->cmd;
	struct mmc_data *data = mrq->data;

	if (!data)
		return;

	data->host_cookie = 0;
	if (cmd->opcode == MMC_READ_SINGLE_BLOCK ||
	    cmd->opcode	== MMC_READ_MULTIPLE_BLOCK ||
	    cmd->opcode == MMC_WRITE_BLOCK ||
	    cmd->opcode == MMC_WRITE_MULTIPLE_BLOCK) {
		msdc_prepare_data(host, mrq);
		data->host_cookie |= MSDC_ASYNC_FLAG;
	}
}

static void msdc_post_req(struct mmc_host *mmc, struct mmc_request *mrq,
		int err)
{
	struct msdc_host *host = mmc_priv(mmc);
	struct mmc_data *data;

	data = mrq->data;
	if (!data)
		return;
	if (data->host_cookie) {
		data->host_cookie &= ~MSDC_ASYNC_FLAG;
		msdc_unprepare_data(host, mrq);
	}
}

static void msdc_data_xfer_next(struct msdc_host *host,
				struct mmc_request *mrq, struct mmc_data *data)
{
	if (mmc_op_multi(mrq->cmd->opcode) && mrq->stop && !mrq->stop->error &&
	    (!data->bytes_xfered
	     || (!mrq->sbc && host->autocmd != MSDC_AUTOCMD12)))
		msdc_start_command(host, mrq, mrq->stop);
	else
		msdc_request_done(host, mrq);
}

static bool msdc_data_xfer_done(struct msdc_host *host, u32 events,
				struct mmc_request *mrq, struct mmc_data *data)
{
	struct mmc_command *stop = data->stop;
	unsigned long flags;
	bool done;

	bool check_stop = (stop && host->autocmd == MSDC_AUTOCMD12 &&
	    (events & (MSDC_INT_ACMDRDY
		       | MSDC_INT_ACMDCRCERR | MSDC_INT_ACMDTMO)));
	bool check_data = (events &
	    (MSDC_INT_XFER_COMPL | MSDC_INT_DATCRCERR | MSDC_INT_DATTMO
	     | MSDC_INT_DMA_BDCSERR | MSDC_INT_DMA_GPDCSERR
	     | MSDC_INT_DMA_PROTECT));

	spin_lock_irqsave(&host->lock, flags);
	done = !host->data;
	if (check_data)
		host->data = NULL;
	spin_unlock_irqrestore(&host->lock, flags);

	if (done)
		return true;

	if (check_stop)
		msdc_auto_cmd_done(host, events, stop);

	/* AP:
	 * stop->error != 0 means auto CMD12 failed. In this case,
	 * none of data transfer completion statuses will be set.
	 * We report data transfer failure in this case.
	 */
	if (check_data || (stop && stop->error)) {
		msdc_dma_stop(host);

		if ((events & MSDC_INT_XFER_COMPL) && (!stop || !stop->error))
			data->bytes_xfered = data->blocks * data->blksz;
		else {
			dev_err(host->dev, "interrupt events: %x\n", events);
			msdc_reset_hw(host);
			host->error |= REQ_DAT_ERR;
			data->bytes_xfered = 0;

			if (events & MSDC_INT_DATTMO)
				data->error = (unsigned int) -ETIMEDOUT;

			dev_err(host->dev, "%s: cmd=%d; blocks=%d",
				__func__, mrq->cmd->opcode, data->blocks);
			dev_err(host->dev, "data_error=%d xfer_size=%d\n",
					(int)data->error, data->bytes_xfered);
		}
		msdc_dma_clear(host);

		msdc_data_xfer_next(host, mrq, data);
		done = true;
	}
	return done;
}

static void msdc_set_buswidth(struct msdc_host *host, u32 width)
{
	u32 val = sdr_read32(SDC_CFG(host->base));

	val &= ~SDC_CFG_BUSWIDTH;

	switch (width) {
	default:
	case MMC_BUS_WIDTH_1:
		val |= (MSDC_BUS_1BITS << 16);
		break;
	case MMC_BUS_WIDTH_4:
		val |= (MSDC_BUS_4BITS << 16);
		break;
	case MMC_BUS_WIDTH_8:
		val |= (MSDC_BUS_8BITS << 16);
		break;
	}

	sdr_write32(SDC_CFG(host->base), val);
	dev_dbg(host->dev, "Bus Width = %d", width);
}

int msdc_ops_switch_volt(struct mmc_host *mmc, struct mmc_ios *ios)
{
	struct msdc_host *host = mmc_priv(mmc);
	int err = 0;
	u32 status;
	u32 sclk = host->sclk;

	if ((host->host_func != MSDC_EMMC) && (ios->signal_voltage
			!= MMC_SIGNAL_VOLTAGE_330)) {
		/* make sure SDC is not busy (TBC) */
		err = -EPERM;

		while (sdc_is_busy(host))
			cpu_relax();

		/* check if CMD/DATA lines both 0 */
		if ((sdr_read32(MSDC_PS(host->base))
					& ((1 << 24) | (0xF << 16))) == 0) {

			if (ios->signal_voltage == MMC_SIGNAL_VOLTAGE_180) {

				if (host->power_switch)
					host->power_switch(host, 1);
				else
					dev_err(host->dev,
							"No power switch callback");
			}
			/* wait at least 5ms for 1.8v signal switching */
			mdelay(10);

			/* config clock to 10~12MHz mode
			   for volt switch detection by host. */

			msdc_set_mclk(host, 0, 12000000);

			mdelay(5);

			/* start to detect volt change
			   by providing 1.8v signal to card */
			sdr_set_bits(MSDC_CFG(host->base), MSDC_CFG_BV18SDT);

			/* wait at max. 1ms */
			mdelay(1);

			while ((status = sdr_read32(MSDC_CFG(host->base)))
					& MSDC_CFG_BV18SDT)
				cpu_relax();

			if (status & MSDC_CFG_BV18PSS)
				err = 0;
			/* config clock back to init clk freq. */
			msdc_set_mclk(host, 0, sclk);
		}
	}

	return err;
}

static void msdc_request_timeout(struct work_struct *work)
{
	struct msdc_host *host = container_of(work, struct msdc_host,
			req_timeout.work);
	unsigned long flags;
	struct mmc_request *mrq;
	struct mmc_command *cmd;
	struct mmc_data *data;

	spin_lock_irqsave(&host->lock, flags);
	mrq = ACCESS_ONCE(host->mrq);
	cmd = ACCESS_ONCE(host->cmd);
	data = ACCESS_ONCE(host->data);
	spin_unlock_irqrestore(&host->lock, flags);

	/* simulate HW timeout status */
	dev_err(host->dev, "%s: aborting cmd/data/mrq\n", __func__);
	if (mrq) {
		dev_err(host->dev, "%s: aborting mrq=%p cmd=%d\n", __func__,
			mrq, mrq->cmd->opcode);
		if (cmd) {
			dev_err(host->dev, "%s: aborting cmd=%d\n",
					__func__, cmd->opcode);
			msdc_cmd_done(host, MSDC_INT_CMDTMO, mrq, cmd);
		} else if (data) {
			dev_err(host->dev, "%s: abort data: cmd%d; %d blocks\n",
				__func__, mrq->cmd->opcode, data->blocks);
			msdc_data_xfer_done(host, MSDC_INT_DATTMO, mrq, data);
		}
	}
}

static int msdc_ops_enable(struct mmc_host *mmc)
{
	struct msdc_host *host = mmc_priv(mmc);

	pm_runtime_get_sync(host->dev);

	return 0;
}

static int msdc_ops_disable(struct mmc_host *mmc)
{
	struct msdc_host *host = mmc_priv(mmc);

	pm_runtime_mark_last_busy(host->dev);
	pm_runtime_put_autosuspend(host->dev);

	return 0;
}

static irqreturn_t msdc_irq(int irq, void *dev_id)
{
	struct msdc_host *host = (struct msdc_host *) dev_id;

	while (true) {
		unsigned long flags;
		struct mmc_request *mrq;
		struct mmc_command *cmd;
		struct mmc_data *data;
		u32 events, event_mask;

		spin_lock_irqsave(&host->lock, flags);
		events = sdr_read32(MSDC_INT(host->base));
		event_mask = sdr_read32(MSDC_INTEN(host->base));
		/* clear interrupts */
		sdr_write32(MSDC_INT(host->base), events);

		mrq = ACCESS_ONCE(host->mrq);
		cmd = ACCESS_ONCE(host->cmd);
		data = ACCESS_ONCE(host->data);
		spin_unlock_irqrestore(&host->lock, flags);

		if (!(events & event_mask))
			break;

		if (!mrq) {
			dev_err(host->dev,
				"%s: MRQ=NULL; events=%08X; event_mask=%08X\n",
				__func__, events, event_mask);
			WARN_ON(1);
			break;
		}

		dev_dbg(host->dev, "%s: events=%08X\n", __func__, events);

		if (cmd)
			msdc_cmd_done(host, events, mrq, cmd);
		else if (data)
			msdc_data_xfer_done(host, events, mrq, data);
	}

	return IRQ_HANDLED;
}

static void msdc_init_hw(struct msdc_host *host)
{
	u32 val;
	/* Configure to MMC/SD mode */
	sdr_set_field(MSDC_CFG(host->base), MSDC_CFG_MODE, MSDC_SDMMC);

	/* Reset */
	msdc_reset_hw(host);

	/* Disable card detection */
	sdr_clr_bits(MSDC_PS(host->base), MSDC_PS_CDEN);

	/* Disable and clear all interrupts */
	val = sdr_read32(MSDC_INTEN(host->base));
	sdr_clr_bits(MSDC_INTEN(host->base), val);
	sdr_write32(MSDC_INT(host->base), sdr_read32(MSDC_INT(host->base)));

	sdr_write32(MSDC_PAD_TUNE(host->base), 0x00000000);
	sdr_write32(MSDC_IOCON(host->base), 0x00000000);
	sdr_set_field(MSDC_IOCON(host->base), MSDC_IOCON_DDLSEL, 1);
	sdr_write32(MSDC_PATCH_BIT0(host->base), 0x403C004F);
	sdr_set_field(MSDC_PATCH_BIT0(host->base), MSDC_CKGEN_MSDC_DLY_SEL, 1);
	sdr_write32(MSDC_PATCH_BIT1(host->base), 0xFFFF0089);
	/* Configure to enable SDIO mode.
	   it's must otherwise sdio cmd5 failed */
	sdr_set_bits(SDC_CFG(host->base), SDC_CFG_SDIO);

	/* disable detect SDIO device interupt function */
	sdr_clr_bits(SDC_CFG(host->base), SDC_CFG_SDIOIDE);

	/* Configure to default data timeout */
	sdr_set_field(SDC_CFG(host->base), SDC_CFG_DTOC, DEFAULT_DTOC);
	msdc_set_buswidth(host, MMC_BUS_WIDTH_1);

	dev_dbg(host->dev, "init hardware done!");
}

/* called by msdc_drv_remove */
static void msdc_deinit_hw(struct msdc_host *host)
{

	/* Disable and clear all interrupts */
	u32 val = sdr_read32(MSDC_INTEN(host->base));

	sdr_clr_bits(MSDC_INTEN(host->base), val);
	val = sdr_read32(MSDC_INT(host->base));
	sdr_write32(MSDC_INT(host->base), val);

	clk_disable(host->src_clk);
	clk_unprepare(host->src_clk);
	msdc_set_power_mode(host, MMC_POWER_OFF); /* make sure power down */
}

/* init gpd and bd list in msdc_drv_probe */
static void msdc_init_gpd_bd(struct msdc_host *host, struct msdc_dma *dma)
{
	struct mt_gpdma_desc *gpd = dma->gpd;
	struct mt_bdma_desc *bd = dma->bd;
	struct mt_bdma_desc *ptr, *prev;

	/* we just support one gpd */
	int bdlen = MAX_BD_PER_GPD;

	/* init the 2 gpd */
	memset(gpd, 0, sizeof(struct mt_gpdma_desc) * 2);
	gpd->next = ((u32)dma->gpd_addr
			+ sizeof(struct mt_gpdma_desc));

	gpd->first_u32 |= GPDMA_DESC_BDP; /* hwo, cs, bd pointer */
	gpd->ptr = (u32)dma->bd_addr; /* physical address */

	memset(bd, 0, sizeof(struct mt_bdma_desc) * bdlen);
	ptr = bd + bdlen - 1;

	while (ptr != bd) {
		prev = ptr - 1;
		prev->next = (u32)(dma->bd_addr
				+ sizeof(struct mt_bdma_desc) * (ptr - bd));
		ptr = prev;
	}
}

static void msdc_init_dma_latest_address(struct msdc_host *host)
{
	struct dma_addr *ptr, *prev;

	host->latest_dma_address =
		kzalloc(sizeof(struct dma_addr) * MAX_BD_PER_GPD, GFP_KERNEL);
	ptr = host->latest_dma_address + MAX_BD_PER_GPD - 1;
	while (ptr != host->latest_dma_address) {
		prev = ptr - 1;
		prev->next = (void *) (host->latest_dma_address +
			sizeof(struct dma_addr)
			* (ptr - host->latest_dma_address));
		ptr = prev;
	}
}

static void msdc_set_host_power_control(struct msdc_host *host)
{
	if (MSDC_EMMC == host->host_func) {
		host->power_control = msdc_emmc_power;
	} else {
		host->power_control = msdc_sd_power;
		host->power_switch = msdc_sd_power_switch;
	}
}

static int timing_is_uhs(unsigned char timing)
{
	if (timing != MMC_TIMING_LEGACY &&
		timing != MMC_TIMING_MMC_HS &&
		timing != MMC_TIMING_SD_HS)
		return 1;
	return 0;
}

static void msdc_ops_set_ios(struct mmc_host *mmc, struct mmc_ios *ios)
{
	struct msdc_host *host = mmc_priv(mmc);
	u32 ddr = 0;

	if (ios->timing == MMC_TIMING_UHS_DDR50)
		ddr = 1;

	msdc_set_buswidth(host, ios->bus_width);

	/* Suspend/Resume will do power off/on */
	switch (ios->power_mode) {
	case MMC_POWER_OFF:
	case MMC_POWER_UP:
		msdc_init_hw(host);
		msdc_set_power_mode(host, ios->power_mode);
		break;
	case MMC_POWER_ON:
		host->power_mode = MMC_POWER_ON;
		break;
	default:
		break;
	}

	/* Apply different pinctrl settings for different timing */
	if (timing_is_uhs(ios->timing))
		pinctrl_select_state(host->pinctrl, host->pins_uhs);
	else
		pinctrl_select_state(host->pinctrl, host->pins_default);

	if (host->mclk != ios->clock || host->ddr != ddr)
		msdc_set_mclk(host, ddr, ios->clock);
}

static struct mmc_host_ops mt_msdc_ops = {
	.enable = msdc_ops_enable,
	.disable = msdc_ops_disable,
	.post_req = msdc_post_req,
	.pre_req = msdc_pre_req,
	.request = msdc_ops_request,
	.set_ios = msdc_ops_set_ios,
	.start_signal_voltage_switch = msdc_ops_switch_volt,
};

static int msdc_of_parse(struct platform_device *pdev, struct mmc_host *mmc)
{
	struct msdc_host *host = mmc_priv(mmc);
	struct resource *res;
	void __iomem *base;
	struct regulator *core_power;
	struct regulator *io_power;
	struct clk *src_clk;
	static struct clk *pll_clk;
	int irq;
	const char *function;
	int ret = mmc_of_parse(mmc);

	if (ret)
		return ret;
	/* iomap register */
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	base = devm_ioremap_resource(&pdev->dev, res);
	if (!base) {
		dev_info(&pdev->dev, "can't of_iomap for msdc!!\n");
		return -ENOMEM;
	}
	core_power = devm_regulator_get(&pdev->dev, "core-power");
	io_power = devm_regulator_get(&pdev->dev, "io-power");
	if (!core_power || !io_power) {
		dev_err(&pdev->dev,
			"Cannot get core/io power from the device tree!\n");
		return -EINVAL;
	}

	src_clk = devm_clk_get(&pdev->dev, "src_clk");
	if (IS_ERR(src_clk)) {
		dev_err(&pdev->dev,
				"Invalid source clock from the device tree!\n");
		return -EINVAL;
	}
	if (!pll_clk) {
		pll_clk = devm_clk_get(&pdev->dev, "pll_clk");
		if (!IS_ERR(pll_clk)) {
			ret = clk_set_rate(pll_clk, CLK_RATE_200MHZ);
			if (ret) {
				dev_err(&pdev->dev,
					"%s: clk_set_rate returned %d\n",
					__func__, ret);
				return ret;
			} else {
				dev_dbg(&pdev->dev, "host clock: %ld\n",
						clk_get_rate(src_clk));
			}
		}
	}

	irq = irq_of_parse_and_map(pdev->dev.of_node, 0);
	if (irq < 0)
		return -EINVAL;

	of_property_read_string(pdev->dev.of_node, "func", &function);
	if (!strcmp("EMMC", function))
		host->host_func = MSDC_EMMC;
	else if (!strcmp("SDCARD", function))
		host->host_func = MSDC_SD;
	else if (!strcmp("SDIO", function))
		host->host_func = MSDC_SDIO;
	else {
		dev_err(&pdev->dev, "Un-recognized function: %s\n", function);
		ret = -EINVAL;
		goto err;
	}

	host->pinctrl = devm_pinctrl_get(&pdev->dev);
	if (IS_ERR(host->pinctrl)) {
		ret = PTR_ERR(host->pinctrl);
		dev_err(&pdev->dev, "Cannot find pinctrl!\n");
		goto err;
	}

	host->pins_default = pinctrl_lookup_state(host->pinctrl, "default");
	if (IS_ERR(host->pins_default)) {
		ret = PTR_ERR(host->pins_default);
		dev_err(&pdev->dev, "Cannot find pinctrl default!\n");
		goto err;
	}

	host->pins_uhs = pinctrl_lookup_state(host->pinctrl, "state_uhs");
	if (IS_ERR(host->pins_uhs)) {
		ret = PTR_ERR(host->pins_uhs);
		dev_err(&pdev->dev, "Cannot find pinctrl uhs!\n");
		goto err;
	}

	host->irq = irq;
	host->base = base;
	host->src_clk = src_clk;
	host->core_power = core_power;
	host->io_power = io_power;

	return 0;
err:
	return ret;
}

static int msdc_drv_probe(struct platform_device *pdev)
{
	struct mmc_host *mmc;
	struct msdc_host *host;
	int ret;

	if (!pdev->dev.of_node) {
		dev_err(&pdev->dev, "No DT found\n");
		return -EINVAL;
	}
	/* Allocate MMC host for this device */
	mmc = mmc_alloc_host(sizeof(struct msdc_host), &pdev->dev);
	if (!mmc)
		return -ENOMEM;

	ret = msdc_of_parse(pdev, mmc);
	if (ret)
		goto host_free;

	host = mmc_priv(mmc);
	host->dev = &pdev->dev;
	host->mmc = mmc;
	/* Set host parameters to mmc */
	mmc->ops = &mt_msdc_ops;
	mmc->f_min = HOST_MIN_MCLK;
	mmc->ocr_avail = MSDC_OCR_AVAIL;

	mmc->caps |= MMC_CAP_ERASE;
	/* MMC core transfer sizes tunable parameters */
	mmc->max_segs = MAX_HW_SGMTS;
	mmc->max_seg_size = MAX_SGMT_SZ;
	mmc->max_blk_size = HOST_MAX_BLKSZ;
	mmc->max_req_size = MAX_REQ_SZ;
	mmc->max_blk_count = mmc->max_req_size;

	/* All members of host had beed set to 0 by mmc_alloc_host */
	host->hclk = clk_get_rate(host->src_clk);
	msdc_set_host_power_control(host);
	host->timeout_clks = DEFAULT_DTOC * 1048576;
	if (host->host_func == MSDC_EMMC) {
		host->autocmd = MSDC_AUTOCMD23;
		mmc->caps |= MMC_CAP_CMD23;
	} else
		host->autocmd = 0;
	host->mrq = NULL;

	host->dma.used_gpd = 0;
	host->dma.used_bd = 0;
	msdc_init_dma_latest_address(host);

	/* using dma_alloc_coherent *//* todo: using 1, for all 4 slots */
	host->dma.gpd
			= dma_alloc_coherent(&pdev->dev,
				MAX_GPD_NUM * sizeof(struct mt_gpdma_desc),
				&host->dma.gpd_addr, GFP_KERNEL);
	host->dma.bd
			= dma_alloc_coherent(&pdev->dev,
				MAX_BD_NUM * sizeof(struct mt_bdma_desc),
				&host->dma.bd_addr, GFP_KERNEL);
	if ((!host->dma.gpd) || (!host->dma.bd)) {
		ret = -ENOMEM;
		goto release_mem;
	}
	msdc_init_gpd_bd(host, &host->dma);
	host->ddr = 0;
	INIT_DELAYED_WORK(&host->req_timeout, msdc_request_timeout);
	spin_lock_init(&host->lock);

	platform_set_drvdata(pdev, mmc);
	clk_prepare(host->src_clk);

	pm_runtime_enable(host->dev);
	pm_runtime_get_sync(host->dev);
	pm_runtime_set_autosuspend_delay(host->dev, MTK_MMC_AUTOSUSPEND_DELAY);
	pm_runtime_use_autosuspend(host->dev);

	ret = devm_request_irq(&pdev->dev, (unsigned int) host->irq, msdc_irq,
		IRQF_TRIGGER_LOW | IRQF_ONESHOT, pdev->name, host);
	if (ret)
		goto release;

	ret = mmc_add_host(mmc);
	if (ret)
		goto free_irq;

	pm_runtime_mark_last_busy(host->dev);
	pm_runtime_put_autosuspend(host->dev);
	return 0;

free_irq:
	free_irq(host->irq, host);
	pm_runtime_put_sync(host->dev);
	pm_runtime_disable(host->dev);
release:
	platform_set_drvdata(pdev, NULL);
	msdc_deinit_hw(host);
release_mem:
	if (host->dma.gpd)
		dma_free_coherent(NULL,
			MAX_GPD_NUM * sizeof(struct mt_gpdma_desc),
			host->dma.gpd, host->dma.gpd_addr);
	if (host->dma.bd)
		dma_free_coherent(NULL,
			MAX_BD_NUM * sizeof(struct mt_bdma_desc),
			host->dma.bd, host->dma.bd_addr);

	kfree(host->latest_dma_address);
host_free:
	mmc_free_host(mmc);

	return ret;
}

static int msdc_drv_remove(struct platform_device *pdev)
{
	struct mmc_host *mmc;
	struct msdc_host *host;

	mmc = platform_get_drvdata(pdev);
	host = mmc_priv(mmc);

	pm_runtime_get_sync(host->dev);

	platform_set_drvdata(pdev, NULL);
	mmc_remove_host(host->mmc);
	msdc_deinit_hw(host);

	free_irq(host->irq, host);

	pm_runtime_put_sync(host->dev);
	pm_runtime_disable(host->dev);
	dma_free_coherent(NULL, MAX_GPD_NUM * sizeof(struct mt_gpdma_desc),
			host->dma.gpd, host->dma.gpd_addr);
	dma_free_coherent(NULL, MAX_BD_NUM * sizeof(struct mt_bdma_desc),
			host->dma.bd, host->dma.bd_addr);

	mmc_free_host(host->mmc);

	return 0;
}

#ifdef CONFIG_PM_RUNTIME
static int msdc_runtime_suspend(struct device *dev)
{
	int ret = 0;
	struct mmc_host *mmc = dev_get_drvdata(dev);
	struct msdc_host *host = mmc_priv(mmc);

	ret = msdc_gate_clock(host);
	return ret;
}

static int msdc_runtime_resume(struct device *dev)
{
	struct mmc_host *mmc = dev_get_drvdata(dev);
	struct msdc_host *host = mmc_priv(mmc);

	msdc_ungate_clock(host);
	return 0;
}
#else
#define msdc_runtime_suspend	NULL
#define msdc_runtime_resume	NULL
#endif

#ifdef CONFIG_PM
static int msdc_drv_suspend(struct device *dev)
{
	return 0;
}

static int msdc_drv_resume(struct device *dev)
{
	return 0;
}
#else
#define msdc_drv_suspend	NULL
#define msdc_drv_resume		NULL
#endif

static const struct of_device_id msdc_of_ids[] = {
	{   .compatible = "mediatek,mmc", },
};

static struct platform_driver mt_msdc_driver = {
	.probe = msdc_drv_probe,
	.remove = msdc_drv_remove,
	.driver = {
		.name = DRV_NAME,
		.owner = THIS_MODULE,
		.of_match_table = msdc_of_ids,
		.pm = &(const struct dev_pm_ops){
			.suspend = msdc_drv_suspend,
			.resume = msdc_drv_resume,
			.runtime_suspend = msdc_runtime_suspend,
			.runtime_resume = msdc_runtime_resume,
		},
	},
};

module_platform_driver(mt_msdc_driver);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("MediaTek SD/MMC Card Driver");
