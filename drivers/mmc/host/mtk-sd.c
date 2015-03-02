/*
 * Copyright (c) 2014-2015 MediaTek Inc.
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
#define MAX_BD_NUM          1024
#define MAX_BD_PER_GPD      MAX_BD_NUM

#define MSDC_AUTOCMD23          0x02
#define MSDC_AUTOCMD19          0x03

/*--------------------------------------------------------------------------*/
/* Common Definition                                                        */
/*--------------------------------------------------------------------------*/
#define MSDC_FIFO_SZ            128
#define MSDC_FIFO_THD           64

#define MSDC_MS                 0
#define MSDC_SDMMC              1

#define MSDC_MODE_UNKNOWN       0
#define MSDC_MODE_PIO           1
#define MSDC_MODE_DMA_BASIC     2
#define MSDC_MODE_DMA_DESC      3
#define MSDC_MODE_DMA_ENHANCED  4
#define MSDC_MODE_MMC_STREAM    5

#define MSDC_BUS_1BITS          0
#define MSDC_BUS_4BITS          1
#define MSDC_BUS_8BITS          2

#define MSDC_BURST_8B           3
#define MSDC_BURST_16B          4
#define MSDC_BURST_32B          5
#define MSDC_BURST_64B          6

#define MSDC_EMMC_BOOTMODE0     0	/* Pull low CMD mode */
#define MSDC_EMMC_BOOTMODE1     1	/* Reset CMD mode */

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
#define MSDC_CFG         0x0
#define MSDC_IOCON       0x04
#define MSDC_PS          0x08
#define MSDC_INT         0x0c
#define MSDC_INTEN       0x10
#define MSDC_FIFOCS      0x14
#define MSDC_TXDATA      0x18
#define MSDC_RXDATA      0x1c
#define SDC_CFG          0x30
#define SDC_CMD          0x34
#define SDC_ARG          0x38
#define SDC_STS          0x3c
#define SDC_RESP0        0x40
#define SDC_RESP1        0x44
#define SDC_RESP2        0x48
#define SDC_RESP3        0x4c
#define SDC_BLK_NUM      0x50
#define SDC_CSTS         0x58
#define SDC_CSTS_EN      0x5c
#define SDC_DCRC_STS     0x60
#define EMMC_CFG0        0x70
#define EMMC_CFG1        0x74
#define EMMC_STS         0x78
#define EMMC_IOCON       0x7c
#define SDC_ACMD_RESP    0x80
#define SDC_ACMD19_TRG   0x84
#define SDC_ACMD19_STS   0x88
#define MSDC_DMA_SA      0x90
#define MSDC_DMA_CA      0x94
#define MSDC_DMA_CTRL    0x98
#define MSDC_DMA_CFG     0x9c
#define MSDC_DBG_SEL     0xa0
#define MSDC_DBG_OUT     0xa4
#define MSDC_DMA_LEN     0xa8
#define MSDC_PATCH_BIT   0xb0
#define MSDC_PATCH_BIT1  0xb4
#define MSDC_PAD_CTL0    0xe0
#define MSDC_PAD_CTL1    0xe4
#define MSDC_PAD_CTL2    0xe8
#define MSDC_PAD_TUNE    0xec
#define MSDC_DAT_RDDLY0  0xf0
#define MSDC_DAT_RDDLY1  0xf4
#define MSDC_HW_DBG      0xf8
#define MSDC_VERSION     0x100
#define MSDC_ECO_VER     0x104

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
#define MSDC_PS_WP              (0x1  << 31)	/* R  */

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
#define MSDC_FIFOCS_CLR         (0x1  << 31)	/* RW */

/* SDC_CFG mask */
#define SDC_CFG_SDIOINTWKUP     (0x1  << 0)	/* RW */
#define SDC_CFG_INSWKUP         (0x1  << 1)	/* RW */
#define SDC_CFG_BUSWIDTH        (0x3  << 16)	/* RW */
#define SDC_CFG_SDIO            (0x1  << 19)	/* RW */
#define SDC_CFG_SDIOIDE         (0x1  << 20)	/* RW */
#define SDC_CFG_INTATGAP        (0x1  << 21)	/* RW */
#define SDC_CFG_DTOC            (0xff << 24)	/* RW */

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
#define EMMC_CFG1_BOOTACKTMC    (0xfff << 20)	/* RW */

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
#define MSDC_CKGEN_MSDC_DLY_SEL   (0x1f << 10)
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
#define MSDC_PAD_CTL0_CLKRDSEL  (0xff << 24)	/* RW */

/* MSDC_PAD_CTL1 mask */
#define MSDC_PAD_CTL1_CMDDRVN   (0x7  << 0)	/* RW */
#define MSDC_PAD_CTL1_CMDDRVP   (0x7  << 4)	/* RW */
#define MSDC_PAD_CTL1_CMDSR     (0x1  << 8)	/* RW */
#define MSDC_PAD_CTL1_CMDPD     (0x1  << 16)	/* RW */
#define MSDC_PAD_CTL1_CMDPU     (0x1  << 17)	/* RW */
#define MSDC_PAD_CTL1_CMDSMT    (0x1  << 18)	/* RW */
#define MSDC_PAD_CTL1_CMDIES    (0x1  << 19)	/* RW */
#define MSDC_PAD_CTL1_CMDTDSEL  (0xf  << 20)	/* RW */
#define MSDC_PAD_CTL1_CMDRDSEL  (0xff << 24)	/* RW */

/* MSDC_PAD_CTL2 mask */
#define MSDC_PAD_CTL2_DATDRVN   (0x7  << 0)	/* RW */
#define MSDC_PAD_CTL2_DATDRVP   (0x7  << 4)	/* RW */
#define MSDC_PAD_CTL2_DATSR     (0x1  << 8)	/* RW */
#define MSDC_PAD_CTL2_DATPD     (0x1  << 16)	/* RW */
#define MSDC_PAD_CTL2_DATPU     (0x1  << 17)	/* RW */
#define MSDC_PAD_CTL2_DATIES    (0x1  << 19)	/* RW */
#define MSDC_PAD_CTL2_DATSMT    (0x1  << 18)	/* RW */
#define MSDC_PAD_CTL2_DATTDSEL  (0xf  << 20)	/* RW */
#define MSDC_PAD_CTL2_DATRDSEL  (0xff << 24)	/* RW */

/* MSDC_PAD_TUNE mask */
#define MSDC_PAD_TUNE_DATWRDLY  (0x1f << 0)	/* RW */
#define MSDC_PAD_TUNE_DATRRDLY  (0x1f << 8)	/* RW */
#define MSDC_PAD_TUNE_CMDRDLY   (0x1f << 16)	/* RW */
#define MSDC_PAD_TUNE_CMDRRDLY  (0x1f << 22)	/* RW */
#define MSDC_PAD_TUNE_CLKTXDLY  (0x1f << 27)	/* RW */

/* MSDC_DAT_RDDLY0/1 mask */
#define MSDC_DAT_RDDLY0_D3      (0x1f << 0)	/* RW */
#define MSDC_DAT_RDDLY0_D2      (0x1f << 8)	/* RW */
#define MSDC_DAT_RDDLY0_D1      (0x1f << 16)	/* RW */
#define MSDC_DAT_RDDLY0_D0      (0x1f << 24)	/* RW */

#define MSDC_DAT_RDDLY1_D7      (0x1f << 0)	/* RW */
#define MSDC_DAT_RDDLY1_D6      (0x1f << 8)	/* RW */
#define MSDC_DAT_RDDLY1_D5      (0x1f << 16)	/* RW */
#define MSDC_DAT_RDDLY1_D4      (0x1f << 24)	/* RW */

/*--------------------------------------------------------------------------*/
/* Descriptor Structure                                                     */
/*--------------------------------------------------------------------------*/
struct mt_gpdma_desc {
	u32 first_u32;
#define GPDMA_DESC_HWO		BIT(0)
#define GPDMA_DESC_BDP		BIT(1)
#define GPDMA_DESC_CHECKSUM	(0xff << 8) /* bit8 ~ bit15 */
#define GPDMA_DESC_INT		BIT(16)
	u32 next;
	u32 ptr;
	u32 second_u32;
#define GPDMA_DESC_BUFLEN	(0xffff) /* bit0 ~ bit15 */
#define GPDMA_DESC_EXTLEN	(0xff << 16) /* bit16 ~ bit23 */
	u32 arg;
	u32 blknum;
	u32 cmd;
};

struct mt_bdma_desc {
	u32 first_u32;
#define BDMA_DESC_EOL		BIT(0)
#define BDMA_DESC_CHECKSUM	(0xff << 8) /* bit8 ~ bit15 */
#define BDMA_DESC_BLKPAD	BIT(17)
#define BDMA_DESC_DWPAD		BIT(18)
	u32 next;
	u32 ptr;
	u32 second_u32;
#define BDMA_DESC_BUFLEN	(0xffff) /* bit0 ~ bit15 */
};

struct scatterlist_ex {
	u32 cmd;
	u32 arg;
	u32 sglen;
	struct scatterlist *sg;
};

#define DMA_FLAG_NONE       0x00000000
#define DMA_FLAG_EN_CHKSUM  0x00000001
#define DMA_FLAG_PAD_BLOCK  0x00000002
#define DMA_FLAG_PAD_DWORD  0x00000004

struct msdc_dma {
	struct scatterlist *sg;	/* I/O scatter list */

	struct mt_gpdma_desc *gpd;		/* pointer to gpd array */
	struct mt_bdma_desc *bd;		/* pointer to bd array */
	dma_addr_t gpd_addr;	/* the physical address of gpd array */
	dma_addr_t bd_addr;	/* the physical address of bd array */
};

struct msdc_host {
	struct device *dev;
	struct mmc_host *mmc;	/* mmc structure */
	int cmd_rsp;

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

	struct pinctrl *pinctrl;
	struct pinctrl_state *pins_default;
	struct pinctrl_state *pins_uhs;
	struct delayed_work req_timeout;
	int irq;		/* host interrupt */

	struct clk *src_clk;	/* msdc source clock */
	u32 mclk;		/* mmc subsystem clock */
	u32 hclk;		/* host clock speed */
	u32 sclk;		/* SD/MS clock speed */
	bool ddr;
};

struct dma_addr {
	u32 start_address;
	u32 size;
	u8 end;
	struct dma_addr *next;
};

static void sdr_set_bits(void __iomem *reg, u32 bs)
{
	u32 val = readl(reg);

	val |= bs;
	writel(val, reg);
}

static void sdr_clr_bits(void __iomem *reg, u32 bs)
{
	u32 val = readl(reg);

	val &= ~(u32)bs;
	writel(val, reg);
}

static void sdr_set_field(void __iomem *reg, u32 field, u32 val)
{
	unsigned int tv = readl(reg);

	tv &= ~field;
	tv |= ((val) << (ffs((unsigned int)field) - 1));
	writel(tv, reg);
}

static void sdr_get_field(void __iomem *reg, u32 field, u32 val)
{
	unsigned int tv = readl(reg);

	val = ((tv & field) >> (ffs((unsigned int)field) - 1));
}

#define REQ_CMD_EIO  (0x1 << 0)
#define REQ_CMD_TMO  (0x1 << 1)
#define REQ_DAT_ERR  (0x1 << 2)
#define REQ_STOP_EIO (0x1 << 3)
#define REQ_STOP_TMO (0x1 << 4)
#define REQ_CMD_BUSY (0x1 << 5)

#define msdc_txfifocnt(host) \
	((readl(host->base + MSDC_FIFOCS) & MSDC_FIFOCS_TXCNT) >> 16)
#define msdc_rxfifocnt(host) \
	((readl(host->base + MSDC_FIFOCS) & MSDC_FIFOCS_RXCNT) >> 0)

#define sdc_is_busy(host)    \
	(readl(host->base + SDC_STS) & SDC_STS_SDCBUSY)
#define sdc_is_cmd_busy(host)  \
	(readl(host->base + SDC_STS) & SDC_STS_CMDBUSY)

#define MSDC_PREPARE_FLAG BIT(0)
#define MSDC_ASYNC_FLAG BIT(1)
#define MSDC_MMAP_FLAG BIT(2)

static void msdc_reset_hw(struct msdc_host *host)
{
	u32 val;

	sdr_set_bits(host->base + MSDC_CFG, MSDC_CFG_RST);
	while (readl(host->base + MSDC_CFG) & MSDC_CFG_RST)
		cpu_relax();

	sdr_set_bits(host->base + MSDC_FIFOCS, MSDC_FIFOCS_CLR);
	while (readl(host->base + MSDC_FIFOCS) & MSDC_FIFOCS_CLR)
		cpu_relax();

	val = readl(host->base + MSDC_INT);
	writel(val, host->base + MSDC_INT);
}

#define HOST_MIN_MCLK       260000
#define HOST_MAX_BLKSZ      2048
#define MSDC_OCR_AVAIL      (MMC_VDD_28_29 | MMC_VDD_29_30 \
		| MMC_VDD_30_31 | MMC_VDD_31_32 | MMC_VDD_32_33)

#define DEFAULT_DTOC       3
#define MTK_MMC_AUTOSUSPEND_DELAY	500
#define CMD_TIMEOUT         (HZ/10 * 5)	/* 100ms x5 */
#define DAT_TIMEOUT         (HZ    * 5)	/* 1000ms x5 */
#define CLK_TIMEOUT         (HZ    * 5)	/* 5s    */
/* a single transaction for WIFI may be 50K */
#define MAX_DMA_CNT         (64 * 1024 - 512)

#define MAX_HW_SGMTS        (MAX_BD_NUM)
#define MAX_PHY_SGMTS       (MAX_BD_NUM)
#define MAX_SGMT_SZ         (MAX_DMA_CNT)
#define MAX_REQ_SZ          (512*1024)

#define VOL_3300	3300000
#define VOL_1800	1800000
#define CLK_RATE_200MHZ	200000000UL

#define CMD23_ARG_SPECIAL 0xFFFF0000
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

	return wints;
}

static u8 msdc_dma_calcs(u8 *buf, u32 len)
{
	u32 i, sum = 0;

	for (i = 0; i < len; i++)
		sum += buf[i];
	return 0xff - (u8) sum;
}

static inline void msdc_dma_setup(struct msdc_host *host, struct msdc_dma *dma,
		struct mmc_data *data)
{
	u32 sglen, j;
	u32 dma_address, dma_len;
	struct scatterlist *sg;
	struct mt_gpdma_desc *gpd;
	struct mt_bdma_desc *bd;

	sglen = data->sg_len;
	sg = data->sg;

	gpd = dma->gpd;
	bd = dma->bd;

	/* modify gpd */
	gpd->first_u32 |= GPDMA_DESC_HWO;
	gpd->first_u32 |= GPDMA_DESC_BDP;
	/* need to clear first. use these bits to calc checksum */
	gpd->first_u32 &= ~GPDMA_DESC_CHECKSUM;
	gpd->first_u32 |= msdc_dma_calcs((u8 *) gpd, 16) << 8;

	/* modify bd */
	for (j = 0; j < sglen; j++) {
		dma_address = sg_dma_address(sg);
		dma_len = sg_dma_len(sg);

		/* init bd */
		bd[j].first_u32 &= ~BDMA_DESC_BLKPAD;
		bd[j].first_u32 &= ~BDMA_DESC_DWPAD;
		bd[j].ptr = (u32)dma_address;
		bd[j].second_u32 &= ~BDMA_DESC_BUFLEN;
		bd[j].second_u32 |= (dma_len & BDMA_DESC_BUFLEN);

		if (j == sglen - 1) /* the last bd */
			bd[j].first_u32 |= BDMA_DESC_EOL;
		else
			bd[j].first_u32 &= ~BDMA_DESC_EOL;

		/* checksume need to clear first */
		bd[j].first_u32 &= ~BDMA_DESC_CHECKSUM;
		bd[j].first_u32 |= msdc_dma_calcs((u8 *)(&bd[j]), 16) << 8;
		sg++;
	}

	sdr_set_field(host->base + MSDC_DMA_CFG, MSDC_DMA_CFG_DECSEN, 1);
	sdr_set_field(host->base + MSDC_DMA_CTRL, MSDC_DMA_CTRL_BRUSTSZ,
			MSDC_BURST_64B);
	sdr_set_field(host->base + MSDC_DMA_CTRL, MSDC_DMA_CTRL_MODE, 1);

	writel((u32)dma->gpd_addr, host->base + MSDC_DMA_SA);

}

static void msdc_prepare_data(struct msdc_host *host, struct mmc_request *mrq)
{
	struct mmc_data *data = mrq->data;

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

	if (data->host_cookie & MSDC_ASYNC_FLAG)
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
		sdr_get_field(host->base + MSDC_CFG, MSDC_CFG_CKMOD, mode);
		/*DDR mode will double the clk cycles for data timeout */
		timeout = mode >= 2 ? timeout * 2 : timeout;
		timeout = timeout > 1 ? timeout - 1 : 0;
		timeout = timeout > 255 ? 255 : timeout;
	}
	sdr_set_field(host->base + SDC_CFG, SDC_CFG_DTOC, timeout);
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

	flags = readl(host->base + MSDC_INTEN);
	sdr_clr_bits(host->base + MSDC_INTEN, flags);

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

	sdr_set_field(host->base + MSDC_CFG, MSDC_CFG_CKMOD | MSDC_CFG_CKDIV,
			(mode << 8) | (div % 0xff));
	while (!(readl(host->base + MSDC_CFG) & MSDC_CFG_CKSTB))
		cpu_relax();

	host->sclk = sclk;
	host->mclk = hz;
	host->ddr = ddr;
	/* need because clk changed. */
	msdc_set_timeout(host, host->timeout_ns, host->timeout_clks);
	sdr_set_bits(host->base + MSDC_INTEN, flags);

	dev_dbg(host->dev, "sclk: %d, ddr: %d\n", host->sclk, ddr);
}

#ifdef CONFIG_PM
static int msdc_gate_clock(struct msdc_host *host)
{
	int ret = 0;
	unsigned long flags;

	spin_lock_irqsave(&host->lock, flags);
	/* disable SD/MMC/SDIO bus clock */
	sdr_set_field(host->base + MSDC_CFG, MSDC_CFG_MODE, MSDC_MS);
	/* turn off SDHC functional clock */
	clk_disable(host->src_clk);
	spin_unlock_irqrestore(&host->lock, flags);
	return ret;
}

static void msdc_ungate_clock(struct msdc_host *host)
{
	unsigned long flags;

	spin_lock_irqsave(&host->lock, flags);
	clk_enable(host->src_clk);
	/* enable SD/MMC/SDIO bus clock:
	 * it will be automatically gated when the bus is idle
	 * (set MSDC_CFG_CKPDN bit to have it always on)
	 */
	sdr_set_field(host->base + MSDC_CFG, MSDC_CFG_MODE, MSDC_SDMMC);
	while (!(readl(host->base + MSDC_CFG) & MSDC_CFG_CKSTB))
		cpu_relax();
	spin_unlock_irqrestore(&host->lock, flags);
}
#endif

static void msdc_set_power_mode(struct msdc_host *host, u8 mode)
{
	int ret;

	dev_dbg(host->dev, "Set power mode(%d)", mode);
	if (mode != MMC_POWER_OFF) {
		regulator_set_voltage(host->core_power, VOL_3300, VOL_3300);
		ret = regulator_enable(host->core_power);
		if (ret)
			dev_err(host->dev, "Failed to set core power!\n");

		if (host->io_power) {
			regulator_set_voltage(host->io_power, VOL_3300,
					VOL_3300);
			ret = regulator_enable(host->io_power);
			if (ret)
				dev_err(host->dev, "Failed to set io power!\n");
		}
	} else {
		regulator_disable(host->core_power);
		if (host->io_power)
			regulator_disable(host->io_power);
	}

	msleep(10);
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
	u32 rawcmd = (opcode & 0x3f) | ((resp & 7) << 7);

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
			if (mmc_card_mmc(host->mmc->card) && mrq->sbc &&
					!(mrq->sbc->arg & CMD23_ARG_SPECIAL))
				rawcmd |= (MSDC_AUTOCMD23 & 3) << 28;
		}

		rawcmd |= ((data->blksz & 0xFFF) << 16);
		if (data->flags & MMC_DATA_WRITE)
			rawcmd |= (1 << 13);
		if (data->blocks > 1)
			rawcmd |= (2 << 11);
		else
			rawcmd |= (1 << 11);
		/* Always use dma mode */
		sdr_clr_bits(host->base + MSDC_CFG, MSDC_CFG_PIO);

		if (((host->timeout_ns != data->timeout_ns) ||
				(host->timeout_clks != data->timeout_clks))) {
			msdc_set_timeout(host, data->timeout_ns,
					data->timeout_clks);
		}

		writel(data->blocks, host->base + SDC_BLK_NUM);
	}
	return rawcmd;
}

static void msdc_start_data(struct msdc_host *host, struct mmc_request *mrq,
			    struct mmc_command *cmd, struct mmc_data *data)
{
	bool read;
	unsigned long flags;
	u32 wints;

	spin_lock_irqsave(&host->lock, flags);
	if (!host->data) {
		host->data = data;
	} else {
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

	mod_delayed_work(system_wq, &host->req_timeout, DAT_TIMEOUT);
	msdc_dma_setup(host, &host->dma, data);
	wints = msdc_data_ints_mask(host);
	sdr_set_bits(host->base + MSDC_INTEN, wints);
	mb(); /* wait for pending IO to finish */
	sdr_set_field(host->base + MSDC_DMA_CTRL, MSDC_DMA_CTRL_START, 1);
	dev_dbg(host->dev, "DMA start\n");
	dev_dbg(host->dev, "%s: cmd=%d DMA data: %d blocks; read=%d\n",
			__func__, cmd->opcode, data->blocks, read);
}

static int msdc_auto_cmd_done(struct msdc_host *host, int events,
		struct mmc_command *cmd)
{
	u32 *rsp = cmd->resp;

	rsp[0] = readl(host->base + SDC_ACMD_RESP);

	if (events & MSDC_INT_ACMDRDY) {
		cmd->error = 0;
	} else {
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
		dev_dbg(host->dev, "%s: cmd=%d arg=%08X; host->error=0x%08X\n",
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
	if (mrq->data)
		msdc_unprepare_data(host, mrq);
	mmc_request_done(host->mmc, mrq);
}

/* returns true if command is fully handled; returns false otherwise */
static bool msdc_cmd_done(struct msdc_host *host, int events,
			  struct mmc_request *mrq, struct mmc_command *cmd)
{
	bool done = false;
	bool sbc_error;
	unsigned long flags;
	u32 *rsp = cmd->resp;

	if (mrq->sbc && cmd == mrq->cmd &&
			(events & (MSDC_INT_ACMDRDY | MSDC_INT_ACMDCRCERR
				   | MSDC_INT_ACMDTMO)))
		msdc_auto_cmd_done(host, events, mrq->sbc);

	sbc_error = mrq->sbc && mrq->sbc->error;

	if (!sbc_error && !(events & (MSDC_INT_CMDRDY
					| MSDC_INT_RSPCRCERR
					| MSDC_INT_CMDTMO)))
		return done;

	spin_lock_irqsave(&host->lock, flags);
	done = !host->cmd;
	host->cmd = NULL;
	spin_unlock_irqrestore(&host->lock, flags);

	if (done)
		return true;

	sdr_clr_bits(host->base + MSDC_INTEN, MSDC_CMD_INTS);

	switch (host->cmd_rsp) {
	case RESP_NONE:
		break;
	case RESP_R2:
		rsp[0] = readl(host->base + SDC_RESP3);
		rsp[1] = readl(host->base + SDC_RESP2);
		rsp[2] = readl(host->base + SDC_RESP1);
		rsp[3] = readl(host->base + SDC_RESP0);
		break;
	default: /* Response types 1, 3, 4, 5, 6, 7(1b) */
		rsp[0] = readl(host->base + SDC_RESP0);
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
		dev_dbg(host->dev,
				"%s: cmd=%d arg=%08X; rsp %08X; cmd_error=%d\n",
				__func__, cmd->opcode, cmd->arg, rsp[0],
				(int)cmd->error);

	msdc_cmd_next(host, mrq, cmd);
	done = true;
	return done;
}

static inline bool msdc_cmd_is_ready(struct msdc_host *host,
		struct mmc_request *mrq, struct mmc_command *cmd)
{
	unsigned long tmo = jiffies + msecs_to_jiffies(20);

	while (sdc_is_cmd_busy(host) && time_before(jiffies, tmo))
		continue;

	if (sdc_is_cmd_busy(host)) {
		dev_err(host->dev, "CMD bus busy detected\n");
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
	if (host->cmd) {
		dev_err(host->dev, "%s: CMD%d is active, but new CMD%d is sent\n",
				__func__, host->cmd->opcode, cmd->opcode);
		BUG();
	} else {
		host->cmd = cmd;
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
	mod_delayed_work(system_wq, &host->req_timeout, DAT_TIMEOUT);

	sdr_set_bits(host->base + MSDC_INTEN, MSDC_CMD_INTS);
	writel(cmd->arg, host->base + SDC_ARG);
	writel(rawcmd, host->base + SDC_CMD);
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
	if (!host->mrq) {
		host->mrq = mrq;
	} else {
		dev_err(host->dev,
			"%s: req [CMD%d] is active, new req [CMD%d] is sent\n",
			__func__, host->mrq->cmd->opcode, mrq->cmd->opcode);
		BUG();
	}
	spin_unlock_irqrestore(&host->lock, flags);

	if (mrq->data)
		msdc_prepare_data(host, mrq);

	/* if SBC is required, we have HW option and SW option.
	 * if HW option is enabled, and SBC does not have "special" flags,
	 * use HW option,  otherwise use SW option
	 */
	if (mrq->sbc && (!mmc_card_mmc(mmc->card) ||
	    (mrq->sbc->arg & CMD23_ARG_SPECIAL)))
		msdc_start_command(host, mrq, mrq->sbc);
	else
		msdc_start_command(host, mrq, mrq->cmd);
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
			(!data->bytes_xfered || !mrq->sbc))
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
	u32 wints;

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

	if (check_data || (stop && stop->error)) {
		dev_dbg(host->dev, "DMA status: 0x%8X\n",
				readl(host->base + MSDC_DMA_CFG));
		sdr_set_field(host->base + MSDC_DMA_CTRL, MSDC_DMA_CTRL_STOP,
				1);
		while (readl(host->base + MSDC_DMA_CFG) & MSDC_DMA_CFG_STS)
			;
		mb(); /* wait for pending IO to finish */
		wints = msdc_data_ints_mask(host);
		sdr_clr_bits(host->base + MSDC_INTEN, wints);
		dev_dbg(host->dev, "DMA stop\n");

		if ((events & MSDC_INT_XFER_COMPL) && (!stop || !stop->error)) {
			data->bytes_xfered = data->blocks * data->blksz;
		} else {
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

		msdc_data_xfer_next(host, mrq, data);
		done = true;
	}
	return done;
}

static void msdc_set_buswidth(struct msdc_host *host, u32 width)
{
	u32 val = readl(host->base + SDC_CFG);

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

	writel(val, host->base + SDC_CFG);
	dev_dbg(host->dev, "Bus Width = %d", width);
}

int msdc_ops_switch_volt(struct mmc_host *mmc, struct mmc_ios *ios)
{
	struct msdc_host *host = mmc_priv(mmc);
	int err = 0;
	u32 status;
	u32 sclk = host->sclk;

	if (ios->signal_voltage == MMC_SIGNAL_VOLTAGE_330)
		return 0;

	/* make sure SDC is not busy (TBC) */
	err = -EPERM;

	while (sdc_is_busy(host))
		cpu_relax();

	/* check if CMD/DATA lines both 0 */
	if ((readl(host->base + MSDC_PS)
				& ((1 << 24) | (0xf << 16))) != 0)
		return err;

	if (ios->signal_voltage == MMC_SIGNAL_VOLTAGE_180) {
		if (host->io_power) {
			regulator_set_voltage(host->io_power, VOL_1800,
					VOL_1800);
			/* wait at least 5ms for 1.8v signal switching */
			msleep(5);
		} else {
			dev_dbg(host->dev,
					"Do not support power switch!\n");
			return err;
		}
	}

	/* config clock to 10~12MHz mode for volt switch detection by host. */
	msdc_set_mclk(host, 0, 12000000);

	msleep(5);

	/* start to detect volt change by providing 1.8v signal to card */
	sdr_set_bits(host->base + MSDC_CFG, MSDC_CFG_BV18SDT);

	/* wait at max. 1ms */
	mdelay(1);

	while ((status = readl(host->base + MSDC_CFG))
			& MSDC_CFG_BV18SDT)
		cpu_relax();

	if (status & MSDC_CFG_BV18PSS)
		err = 0;
	/* config clock back to init clk freq. */
	msdc_set_mclk(host, 0, sclk);

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
		events = readl(host->base + MSDC_INT);
		event_mask = readl(host->base + MSDC_INTEN);
		/* clear interrupts */
		writel(events, host->base + MSDC_INT);

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
	sdr_set_field(host->base + MSDC_CFG, MSDC_CFG_MODE, MSDC_SDMMC);

	/* Reset */
	msdc_reset_hw(host);

	/* Disable card detection */
	sdr_clr_bits(host->base + MSDC_PS, MSDC_PS_CDEN);

	/* Disable and clear all interrupts */
	writel(0, host->base + MSDC_INTEN);
	val = readl(host->base + MSDC_INT);
	writel(val, host->base + MSDC_INT);

	writel(0, host->base + MSDC_PAD_TUNE);
	writel(0, host->base + MSDC_IOCON);
	sdr_set_field(host->base + MSDC_IOCON, MSDC_IOCON_DDLSEL, 1);
	writel(0x403c004f, host->base + MSDC_PATCH_BIT);
	sdr_set_field(host->base + MSDC_PATCH_BIT, MSDC_CKGEN_MSDC_DLY_SEL, 1);
	writel(0xffff0089, host->base + MSDC_PATCH_BIT1);
	/* Configure to enable SDIO mode.
	   it's must otherwise sdio cmd5 failed */
	sdr_set_bits(host->base + SDC_CFG, SDC_CFG_SDIO);

	/* disable detect SDIO device interupt function */
	sdr_clr_bits(host->base + SDC_CFG, SDC_CFG_SDIOIDE);

	/* Configure to default data timeout */
	sdr_set_field(host->base + SDC_CFG, SDC_CFG_DTOC, DEFAULT_DTOC);
	msdc_set_buswidth(host, MMC_BUS_WIDTH_1);

	dev_dbg(host->dev, "init hardware done!");
}

static void msdc_deinit_hw(struct msdc_host *host)
{
	u32 val;

	/* Disable and clear all interrupts */
	writel(0, host->base + MSDC_INTEN);

	val = readl(host->base + MSDC_INT);
	writel(val, host->base + MSDC_INT);

	clk_disable(host->src_clk);
	clk_unprepare(host->src_clk);
	msdc_set_power_mode(host, MMC_POWER_OFF); /* make sure power down */
}

/* init gpd and bd list in msdc_drv_probe */
static void msdc_init_gpd_bd(struct msdc_host *host, struct msdc_dma *dma)
{
	struct mt_gpdma_desc *gpd = dma->gpd;
	struct mt_bdma_desc *bd = dma->bd;
	int i;

	memset(gpd, 0, sizeof(struct mt_gpdma_desc) * MAX_GPD_NUM);
	gpd->next = (u32)dma->gpd_addr + sizeof(struct mt_gpdma_desc);

	gpd->first_u32 |= GPDMA_DESC_BDP; /* hwo, cs, bd pointer */
	gpd->ptr = (u32)dma->bd_addr; /* physical address */

	memset(bd, 0, sizeof(struct mt_bdma_desc) * MAX_BD_PER_GPD);

	for (i = 0; i < (MAX_BD_PER_GPD - 1); i++)
		bd[i].next = (u32)dma->bd_addr + sizeof(*bd) * (i + 1);
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
	int irq;
	int ret;

	ret = mmc_of_parse(mmc);
	if (ret)
		return ret;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(base))
		return PTR_ERR(base);

	core_power = devm_regulator_get(&pdev->dev, "core-power");
	if (IS_ERR(core_power)) {
		dev_dbg(&pdev->dev,
			"Cannot get core power from the device tree!\n");
		return PTR_ERR(core_power);
	}

	io_power = devm_regulator_get_optional(&pdev->dev, "io-power");
	if (IS_ERR(io_power)) {
		io_power = NULL;
		dev_dbg(&pdev->dev,
			"Cannot get io power from the device tree!\n");
	}

	src_clk = devm_clk_get(&pdev->dev, "source");
	if (IS_ERR(src_clk)) {
		dev_err(&pdev->dev,
				"Invalid source clock from the device tree!\n");
		return PTR_ERR(src_clk);
	}

	irq = platform_get_irq(pdev, 0);
	if (irq < 0)
		return -EINVAL;

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

	mmc->caps |= MMC_CAP_ERASE | MMC_CAP_CMD23;
	/* MMC core transfer sizes tunable parameters */
	mmc->max_segs = MAX_HW_SGMTS;
	mmc->max_seg_size = MAX_SGMT_SZ;
	mmc->max_blk_size = HOST_MAX_BLKSZ;
	mmc->max_req_size = MAX_REQ_SZ;
	mmc->max_blk_count = mmc->max_req_size;

	host->hclk = clk_get_rate(host->src_clk);
	host->timeout_clks = DEFAULT_DTOC * 1048576;
	host->dma.gpd = dma_alloc_coherent(&pdev->dev,
				MAX_GPD_NUM * sizeof(struct mt_gpdma_desc),
				&host->dma.gpd_addr, GFP_KERNEL);
	host->dma.bd = dma_alloc_coherent(&pdev->dev,
				MAX_BD_NUM * sizeof(struct mt_bdma_desc),
				&host->dma.bd_addr, GFP_KERNEL);
	if ((!host->dma.gpd) || (!host->dma.bd)) {
		ret = -ENOMEM;
		goto release_mem;
	}
	msdc_init_gpd_bd(host, &host->dma);
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
		goto end;

	pm_runtime_mark_last_busy(host->dev);
	pm_runtime_put_autosuspend(host->dev);
	return 0;

end:
	pm_runtime_put_sync(host->dev);
	pm_runtime_disable(host->dev);
release:
	platform_set_drvdata(pdev, NULL);
	msdc_deinit_hw(host);
release_mem:
	if (host->dma.gpd)
		dma_free_coherent(&pdev->dev,
			MAX_GPD_NUM * sizeof(struct mt_gpdma_desc),
			host->dma.gpd, host->dma.gpd_addr);
	if (host->dma.bd)
		dma_free_coherent(&pdev->dev,
			MAX_BD_NUM * sizeof(struct mt_bdma_desc),
			host->dma.bd, host->dma.bd_addr);

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

	pm_runtime_put_sync(host->dev);
	pm_runtime_disable(host->dev);
	dma_free_coherent(&pdev->dev,
			MAX_GPD_NUM * sizeof(struct mt_gpdma_desc),
			host->dma.gpd, host->dma.gpd_addr);
	dma_free_coherent(&pdev->dev, MAX_BD_NUM * sizeof(struct mt_bdma_desc),
			host->dma.bd, host->dma.bd_addr);

	mmc_free_host(host->mmc);

	return 0;
}

#ifdef CONFIG_PM
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
#endif

static const struct dev_pm_ops msdc_dev_pm_ops = {
	SET_RUNTIME_PM_OPS(msdc_runtime_suspend, msdc_runtime_resume, NULL)
};

static const struct of_device_id msdc_of_ids[] = {
	{   .compatible = "mediatek,mt8135-mmc", },
	{}
};

static struct platform_driver mt_msdc_driver = {
	.probe = msdc_drv_probe,
	.remove = msdc_drv_remove,
	.driver = {
		.name = "mtk-msdc",
		.of_match_table = msdc_of_ids,
		.pm = &msdc_dev_pm_ops,
	},
};

module_platform_driver(mt_msdc_driver);
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("MediaTek SD/MMC Card Driver");
