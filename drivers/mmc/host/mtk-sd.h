#ifndef __MTK_SD_H
#define __MTK_SD_H

#include <linux/bitops.h>
#include <linux/regulator/consumer.h>
#include <linux/mmc/host.h>
#include <linux/tracepoint.h>
enum {
	MT_PIN_PULL_DEFAULT,
	MT_PIN_PULL_DISABLE,
	MT_PIN_PULL_DOWN,
	MT_PIN_PULL_UP,
	MT_PIN_PULL_ENABLE,
	MT_PIN_PULL_ENABLE_DOWN,
	MT_PIN_PULL_ENABLE_UP,
};

#define MAX_GPD_NUM         (1 + 1)	/* one null gpd */
#define MAX_BD_NUM          (1024)
#define MAX_BD_PER_GPD      (MAX_BD_NUM)
#define HOST_MAX_NUM        (5)

#define SDIO_ERROR_BYPASS
/* ///////////////////////////////////////////////////////////////////////// */


/* #define MSDC_CLKSRC_REG     (0xf100000C) */
#define MSDC_DESENSE_REG	(0xf0007070)

#define MSDC_AUTOCMD12          (0x0001)
#define MSDC_AUTOCMD23          (0x0002)
#define MSDC_AUTOCMD19          (0x0003)

/*--------------------------------------------------------------------------*/
/* Common Macro                                                             */
/*--------------------------------------------------------------------------*/
#define REG_ADDR(x)                 (host->base + OFFSET_##x)

/*--------------------------------------------------------------------------*/
/* Common Definition                                                        */
/*--------------------------------------------------------------------------*/
#define MSDC_FIFO_SZ            (128)
#define MSDC_FIFO_THD           (64)	/* (128) */
#define MSDC_NUM                (4)

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

#define MSDC_BRUST_8B           (3)
#define MSDC_BRUST_16B          (4)
#define MSDC_BRUST_32B          (5)
#define MSDC_BRUST_64B          (6)

#define MSDC_PIN_PULL_NONE      MT_PIN_PULL_DISABLE
#define MSDC_PIN_PULL_DOWN      MT_PIN_PULL_ENABLE_DOWN
#define MSDC_PIN_PULL_UP        MT_PIN_PULL_ENABLE_UP

#define MSDC_AUTOCMD12          (0x0001)
#define MSDC_AUTOCMD23          (0x0002)
#define MSDC_AUTOCMD19          (0x0003)

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
#define MSDC_CFG                REG_ADDR(MSDC_CFG)
#define MSDC_IOCON              REG_ADDR(MSDC_IOCON)
#define MSDC_PS                 REG_ADDR(MSDC_PS)
#define MSDC_INT                REG_ADDR(MSDC_INT)
#define MSDC_INTEN              REG_ADDR(MSDC_INTEN)
#define MSDC_FIFOCS             REG_ADDR(MSDC_FIFOCS)
#define MSDC_TXDATA             REG_ADDR(MSDC_TXDATA)
#define MSDC_RXDATA             REG_ADDR(MSDC_RXDATA)
#define MSDC_PATCH_BIT0         REG_ADDR(MSDC_PATCH_BIT)

/* sdmmc register */
#define SDC_CFG                 REG_ADDR(SDC_CFG)
#define SDC_CMD                 REG_ADDR(SDC_CMD)
#define SDC_ARG                 REG_ADDR(SDC_ARG)
#define SDC_STS                 REG_ADDR(SDC_STS)
#define SDC_RESP0               REG_ADDR(SDC_RESP0)
#define SDC_RESP1               REG_ADDR(SDC_RESP1)
#define SDC_RESP2               REG_ADDR(SDC_RESP2)
#define SDC_RESP3               REG_ADDR(SDC_RESP3)
#define SDC_BLK_NUM             REG_ADDR(SDC_BLK_NUM)
#define SDC_CSTS                REG_ADDR(SDC_CSTS)
#define SDC_CSTS_EN             REG_ADDR(SDC_CSTS_EN)
#define SDC_DCRC_STS            REG_ADDR(SDC_DCRC_STS)

/* emmc register*/
#define EMMC_CFG0               REG_ADDR(EMMC_CFG0)
#define EMMC_CFG1               REG_ADDR(EMMC_CFG1)
#define EMMC_STS                REG_ADDR(EMMC_STS)
#define EMMC_IOCON              REG_ADDR(EMMC_IOCON)

/* auto command register */
#define SDC_ACMD_RESP           REG_ADDR(SDC_ACMD_RESP)
#define SDC_ACMD19_TRG          REG_ADDR(SDC_ACMD19_TRG)
#define SDC_ACMD19_STS          REG_ADDR(SDC_ACMD19_STS)

/* dma register */
#define MSDC_DMA_SA             REG_ADDR(MSDC_DMA_SA)
#define MSDC_DMA_CA             REG_ADDR(MSDC_DMA_CA)
#define MSDC_DMA_CTRL           REG_ADDR(MSDC_DMA_CTRL)
#define MSDC_DMA_CFG            REG_ADDR(MSDC_DMA_CFG)
#define MSDC_DMA_LEN            REG_ADDR(MSDC_DMA_LEN)

/* pad ctrl register */
#define MSDC_PAD_CTL0           REG_ADDR(MSDC_PAD_CTL0)
#define MSDC_PAD_CTL1           REG_ADDR(MSDC_PAD_CTL1)
#define MSDC_PAD_CTL2           REG_ADDR(MSDC_PAD_CTL2)

/* data read delay */
#define MSDC_DAT_RDDLY0         REG_ADDR(MSDC_DAT_RDDLY0)
#define MSDC_DAT_RDDLY1         REG_ADDR(MSDC_DAT_RDDLY1)

/* debug register */
#define MSDC_DBG_SEL            REG_ADDR(MSDC_DBG_SEL)
#define MSDC_DBG_OUT            REG_ADDR(MSDC_DBG_OUT)

/* misc register */
#define MSDC_PATCH_BIT          REG_ADDR(MSDC_PATCH_BIT)
#define MSDC_PATCH_BIT1         REG_ADDR(MSDC_PATCH_BIT1)
#define MSDC_PAD_TUNE           REG_ADDR(MSDC_PAD_TUNE)
#define MSDC_HW_DBG             REG_ADDR(MSDC_HW_DBG)
#define MSDC_VERSION            REG_ADDR(MSDC_VERSION)
#define MSDC_ECO_VER            REG_ADDR(MSDC_ECO_VER)	/* ECO Version */

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
	u32 hwo:1;		/* could be changed by hw */
	u32 bdp:1;
	u32 rsv0:6;
	u32 chksum:8;
	u32 intr:1;
	u32 rsv1:15;
	void *next;
	void *ptr;
	u32 buflen:16;
	u32 extlen:8;
	u32 rsv2:8;
	u32 arg;
	u32 blknum;
	u32 cmd;
};

struct mt_bdma_desc {
	u32 eol:1;
	u32 rsv0:7;
	u32 chksum:8;
	u32 rsv1:1;
	u32 blkpad:1;
	u32 dwpad:1;
	u32 rsv2:13;
	void *next;
	void *ptr;
	u32 buflen:16;
	u32 rsv3:16;
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
	bool disable_next;

	spinlock_t clk_gate_lock;
	/*to solve removing bad card race condition with hot-plug enable */
	spinlock_t remove_bad_card;
	int clk_gate_count;

	void __iomem *base;		/* host base address */

	struct msdc_dma dma;	/* dma channel */
	u32 dma_addr;		/* dma transfer address */
	u32 dma_left_size;	/* dma transfer left size */
	u32 dma_xfer_size;	/* dma transfer size in bytes */

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
	/* driver will get a EINT(Level sensitive)
	   when boot up phone with card insert */
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
#define sdr_set_bits(reg, bs) \
	do { \
		u32 val = sdr_read32(reg); \
		val |= bs; \
		sdr_write32(reg, val); \
	} while (0)
#define sdr_clr_bits(reg, bs) \
	do { \
		u32 val = sdr_read32(reg); \
		val &= ~((u32)(bs)); \
		sdr_write32(reg, val); \
	} while (0)
#define sdr_set_field(reg, field, val) \
	do {	\
		unsigned int tv = sdr_read32(reg);	\
		tv &= ~(field); \
		tv |= ((val) << (ffs((unsigned int)field) - 1)); \
		sdr_write32(reg, tv); \
	} while (0)
#define sdr_get_field(reg, field, val) \
	do {	\
		unsigned int tv = sdr_read32(reg);	\
		val = ((tv & (field)) >> (ffs((unsigned int)field) - 1)); \
	} while (0)
#define sdr_set_field_discrete(reg, field, val) \
	do {	\
		unsigned int tv = sdr_read32(reg); \
		tv = (val == 1) ? (tv|(field)) : (tv & ~(field));\
		sdr_write32(reg, tv); \
	} while (0)
#define sdr_get_field_discrete(reg, field, val) \
	do {	\
		unsigned int tv = sdr_read32(reg); \
		val = tv & (field); \
		val = (val == field) ? 1 : 0;\
	} while (0)

#define REQ_CMD_EIO  (0x1 << 0)
#define REQ_CMD_TMO  (0x1 << 1)
#define REQ_DAT_ERR  (0x1 << 2)
#define REQ_STOP_EIO (0x1 << 3)
#define REQ_STOP_TMO (0x1 << 4)
#define REQ_CMD_BUSY (0x1 << 5)

#define MAX_CSD_CAPACITY (4096 * 512)

#define msdc_txfifocnt()   ((sdr_read32(MSDC_FIFOCS) & MSDC_FIFOCS_TXCNT) >> 16)
#define msdc_rxfifocnt()   ((sdr_read32(MSDC_FIFOCS) & MSDC_FIFOCS_RXCNT) >> 0)
#define msdc_fifo_write32(v)   sdr_write32(MSDC_TXDATA, (v))
#define msdc_fifo_write8(v)    sdr_write8(MSDC_TXDATA, (v))
#define msdc_fifo_read32()   sdr_read32(MSDC_RXDATA)
#define msdc_fifo_read8()    sdr_read8(MSDC_RXDATA)

#define msdc_dma_on()        sdr_clr_bits(MSDC_CFG, MSDC_CFG_PIO)
#define msdc_dma_off()       sdr_set_bits(MSDC_CFG, MSDC_CFG_PIO)

#define sdc_is_busy()          (sdr_read32(SDC_STS) & SDC_STS_SDCBUSY)
#define sdc_is_cmd_busy()      (sdr_read32(SDC_STS) & SDC_STS_CMDBUSY)

#define sdc_send_cmd(cmd, arg) \
	do { \
		sdr_write32(SDC_ARG, (arg)); \
		sdr_write32(SDC_CMD, (cmd)); \
	} while (0)

#define MSDC_PREPARE_FLAG BIT(0)
#define MSDC_ASYNC_FLAG BIT(1)
#define MSDC_MMAP_FLAG BIT(2)

#define msdc_reset() \
	do { \
		sdr_set_bits(MSDC_CFG, MSDC_CFG_RST); \
		while (sdr_read32(MSDC_CFG) & MSDC_CFG_RST) \
			cpu_relax(); \
	} while (0)

#define msdc_clr_int() \
	do { \
		u32 val = sdr_read32(MSDC_INT); \
		sdr_write32(MSDC_INT, val); \
	} while (0)

#define msdc_clr_fifo() \
	do { \
		sdr_set_bits(MSDC_FIFOCS, MSDC_FIFOCS_CLR); \
		while (sdr_read32(MSDC_FIFOCS) & MSDC_FIFOCS_CLR) \
			cpu_relax(); \
	} while (0)

#define msdc_reset_hw(id) \
	do { \
		msdc_reset(); \
		msdc_clr_fifo(); \
		msdc_clr_int(); \
	} while (0)

#define msdc_irq_save(val) \
	do { \
		val = sdr_read32(MSDC_INTEN); \
		sdr_clr_bits(MSDC_INTEN, val); \
	} while (0)

#define msdc_irq_restore(val) \
	sdr_set_bits(MSDC_INTEN, val)

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

#define MSDC_DEV_ATTR(name, fmt, val, fmt_type)				\
static ssize_t msdc_attr_##name##_show(struct device *dev,	\
		struct device_attribute *attr, char *buf)	\
{									\
	struct mmc_host *mmc = dev_get_drvdata(dev);			\
	struct msdc_host *host = mmc_priv(mmc);				\
	return sprintf(buf, fmt "\n", (fmt_type)val);			\
}									\
static ssize_t msdc_attr_##name##_store(struct device *dev,		\
	struct device_attribute *attr, const char *buf, size_t count)	\
{									\
	fmt_type tmp;							\
	struct mmc_host *mmc = dev_get_drvdata(dev);			\
	struct msdc_host *host = mmc_priv(mmc);				\
	int n = sscanf(buf, fmt, &tmp);					\
	val = (typeof(val))tmp;						\
	return n ? count : -EINVAL;					\
}									\
static DEVICE_ATTR(name, S_IRUGO | S_IWUSR | S_IWGRP, \
		msdc_attr_##name##_show, msdc_attr_##name##_store)

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

#define MSDC_SDCARD_FLAG  (MSDC_SYS_SUSPEND | MSDC_REMOVABLE | MSDC_HIGHSPEED)
/* Please enable/disable SD card MSDC_CD_PIN_EN for customer request */
#define MSDC_SDIO_FLAG    (MSDC_EXT_SDIO_IRQ | MSDC_HIGHSPEED|MSDC_REMOVABLE)
#define MSDC_EMMC_FLAG    (MSDC_SYS_SUSPEND | MSDC_HIGHSPEED)

#define MSDC_BOOT_EN (1)
#define MSDC_CD_HIGH (1)
#define MSDC_CD_LOW  (0)

#endif /* __MTK_SD_H */
