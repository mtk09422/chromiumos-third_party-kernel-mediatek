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

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/ioport.h>
#include <linux/irq.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <linux/spinlock.h>

#include <linux/mmc/card.h>
#include <linux/mmc/core.h>
#include <linux/mmc/host.h>
#include <linux/mmc/mmc.h>
#include <linux/mmc/sd.h>
#include <linux/mmc/sdio.h>

#define MAX_GPD_NUM         (1 + 1)	/* one null gpd */
#define MAX_BD_NUM          1024
#define MAX_BD_PER_GPD      MAX_BD_NUM

/*--------------------------------------------------------------------------*/
/* Common Definition                                                        */
/*--------------------------------------------------------------------------*/
#define MSDC_MS                 0
#define MSDC_SDMMC              1

#define MSDC_BUS_1BITS          0
#define MSDC_BUS_4BITS          1
#define MSDC_BUS_8BITS          2

#define MSDC_BURST_8B           3
#define MSDC_BURST_16B          4
#define MSDC_BURST_32B          5
#define MSDC_BURST_64B          6

/*--------------------------------------------------------------------------*/
/* Register Offset                                                          */
/*--------------------------------------------------------------------------*/
#define MSDC_CFG         0x0
#define MSDC_IOCON       0x04
#define MSDC_PS          0x08
#define MSDC_INT         0x0c
#define MSDC_INTEN       0x10
#define MSDC_FIFOCS      0x14
#define SDC_CFG          0x30
#define SDC_CMD          0x34
#define SDC_ARG          0x38
#define SDC_STS          0x3c
#define SDC_RESP0        0x40
#define SDC_RESP1        0x44
#define SDC_RESP2        0x48
#define SDC_RESP3        0x4c
#define SDC_BLK_NUM      0x50
#define SDC_ACMD_RESP    0x80
#define MSDC_DMA_SA      0x90
#define MSDC_DMA_CTRL    0x98
#define MSDC_DMA_CFG     0x9c
#define MSDC_PATCH_BIT   0xb0
#define MSDC_PATCH_BIT1  0xb4
#define MSDC_PAD_TUNE    0xec

/*--------------------------------------------------------------------------*/
/* Register Mask                                                            */
/*--------------------------------------------------------------------------*/

/* MSDC_CFG mask */
#define MSDC_CFG_MODE           (1 << 0)	/* RW */
#define MSDC_CFG_CKPDN          (1 << 1)	/* RW */
#define MSDC_CFG_RST            (1 << 2)	/* RW */
#define MSDC_CFG_PIO            (1 << 3)	/* RW */
#define MSDC_CFG_CKDRVEN        (1 << 4)	/* RW */
#define MSDC_CFG_BV18SDT        (1 << 5)	/* RW */
#define MSDC_CFG_BV18PSS        (1 << 6)	/* R  */
#define MSDC_CFG_CKSTB          (1 << 7)	/* R  */
#define MSDC_CFG_CKDIV          (0xff << 8)	/* RW */
#define MSDC_CFG_CKMOD          (0x3 << 16)	/* RW */

/* MSDC_IOCON mask */
#define MSDC_IOCON_SDR104CKS    (1 << 0)	/* RW */
#define MSDC_IOCON_RSPL         (1 << 1)	/* RW */
#define MSDC_IOCON_DSPL         (1 << 2)	/* RW */
#define MSDC_IOCON_DDLSEL       (1 << 3)	/* RW */
#define MSDC_IOCON_DDR50CKD     (1 << 4)	/* RW */
#define MSDC_IOCON_DSPLSEL      (1 << 5)	/* RW */
#define MSDC_IOCON_W_DSPL       (1 << 8)	/* RW */
#define MSDC_IOCON_D0SPL        (1 << 16)	/* RW */
#define MSDC_IOCON_D1SPL        (1 << 17)	/* RW */
#define MSDC_IOCON_D2SPL        (1 << 18)	/* RW */
#define MSDC_IOCON_D3SPL        (1 << 19)	/* RW */
#define MSDC_IOCON_D4SPL        (1 << 20)	/* RW */
#define MSDC_IOCON_D5SPL        (1 << 21)	/* RW */
#define MSDC_IOCON_D6SPL        (1 << 22)	/* RW */
#define MSDC_IOCON_D7SPL        (1 << 23)	/* RW */
#define MSDC_IOCON_RISCSZ       (0x3 << 24)	/* RW */

/* MSDC_PS mask */
#define MSDC_PS_CDEN            (1 << 0)	/* RW */
#define MSDC_PS_CDSTS           (1 << 1)	/* R  */
#define MSDC_PS_CDDEBOUNCE      (0xf << 12)	/* RW */
#define MSDC_PS_DAT             (0xff << 16)	/* R  */
#define MSDC_PS_CMD             (1 << 24)	/* R  */
#define MSDC_PS_WP              (1 << 31)	/* R  */

/* MSDC_INT mask */
#define MSDC_INT_MMCIRQ         (1 << 0)	/* W1C */
#define MSDC_INT_CDSC           (1 << 1)	/* W1C */
#define MSDC_INT_ACMDRDY        (1 << 3)	/* W1C */
#define MSDC_INT_ACMDTMO        (1 << 4)	/* W1C */
#define MSDC_INT_ACMDCRCERR     (1 << 5)	/* W1C */
#define MSDC_INT_DMAQ_EMPTY     (1 << 6)	/* W1C */
#define MSDC_INT_SDIOIRQ        (1 << 7)	/* W1C */
#define MSDC_INT_CMDRDY         (1 << 8)	/* W1C */
#define MSDC_INT_CMDTMO         (1 << 9)	/* W1C */
#define MSDC_INT_RSPCRCERR      (1 << 10)	/* W1C */
#define MSDC_INT_CSTA           (1 << 11)	/* R */
#define MSDC_INT_XFER_COMPL     (1 << 12)	/* W1C */
#define MSDC_INT_DXFER_DONE     (1 << 13)	/* W1C */
#define MSDC_INT_DATTMO         (1 << 14)	/* W1C */
#define MSDC_INT_DATCRCERR      (1 << 15)	/* W1C */
#define MSDC_INT_ACMD19_DONE    (1 << 16)	/* W1C */
#define MSDC_INT_DMA_BDCSERR    (1 << 17)	/* W1C */
#define MSDC_INT_DMA_GPDCSERR   (1 << 18)	/* W1C */
#define MSDC_INT_DMA_PROTECT    (1 << 19)	/* W1C */

/* MSDC_INTEN mask */
#define MSDC_INTEN_MMCIRQ       (1 << 0)	/* RW */
#define MSDC_INTEN_CDSC         (1 << 1)	/* RW */
#define MSDC_INTEN_ACMDRDY      (1 << 3)	/* RW */
#define MSDC_INTEN_ACMDTMO      (1 << 4)	/* RW */
#define MSDC_INTEN_ACMDCRCERR   (1 << 5)	/* RW */
#define MSDC_INTEN_DMAQ_EMPTY   (1 << 6)	/* RW */
#define MSDC_INTEN_SDIOIRQ      (1 << 7)	/* RW */
#define MSDC_INTEN_CMDRDY       (1 << 8)	/* RW */
#define MSDC_INTEN_CMDTMO       (1 << 9)	/* RW */
#define MSDC_INTEN_RSPCRCERR    (1 << 10)	/* RW */
#define MSDC_INTEN_CSTA         (1 << 11)	/* RW */
#define MSDC_INTEN_XFER_COMPL   (1 << 12)	/* RW */
#define MSDC_INTEN_DXFER_DONE   (1 << 13)	/* RW */
#define MSDC_INTEN_DATTMO       (1 << 14)	/* RW */
#define MSDC_INTEN_DATCRCERR    (1 << 15)	/* RW */
#define MSDC_INTEN_ACMD19_DONE  (1 << 16)	/* RW */
#define MSDC_INTEN_DMA_BDCSERR  (1 << 17)	/* RW */
#define MSDC_INTEN_DMA_GPDCSERR (1 << 18)	/* RW */
#define MSDC_INTEN_DMA_PROTECT  (1 << 19)	/* RW */

/* MSDC_FIFOCS mask */
#define MSDC_FIFOCS_RXCNT       (0xff << 0)	/* R */
#define MSDC_FIFOCS_TXCNT       (0xff << 16)	/* R */
#define MSDC_FIFOCS_CLR         (1 << 31)	/* RW */

/* SDC_CFG mask */
#define SDC_CFG_SDIOINTWKUP     (1 << 0)	/* RW */
#define SDC_CFG_INSWKUP         (1 << 1)	/* RW */
#define SDC_CFG_BUSWIDTH        (0x3 << 16)	/* RW */
#define SDC_CFG_SDIO            (1 << 19)	/* RW */
#define SDC_CFG_SDIOIDE         (1 << 20)	/* RW */
#define SDC_CFG_INTATGAP        (1 << 21)	/* RW */
#define SDC_CFG_DTOC            (0xff << 24)	/* RW */

/* SDC_STS mask */
#define SDC_STS_SDCBUSY         (1 << 0)	/* RW */
#define SDC_STS_CMDBUSY         (1 << 1)	/* RW */
#define SDC_STS_SWR_COMPL       (1 << 31)	/* RW */

/* MSDC_DMA_CTRL mask */
#define MSDC_DMA_CTRL_START     (1 << 0)	/* W */
#define MSDC_DMA_CTRL_STOP      (1 << 1)	/* W */
#define MSDC_DMA_CTRL_RESUME    (1 << 2)	/* W */
#define MSDC_DMA_CTRL_MODE      (1 << 8)	/* RW */
#define MSDC_DMA_CTRL_LASTBUF   (1 << 10)	/* RW */
#define MSDC_DMA_CTRL_BRUSTSZ   (0x7 << 12)	/* RW */

/* MSDC_DMA_CFG mask */
#define MSDC_DMA_CFG_STS        (1 << 0)	/* R */
#define MSDC_DMA_CFG_DECSEN     (1 << 1)	/* RW */
#define MSDC_DMA_CFG_AHBHPROT2  (0x2 << 8)	/* RW */
#define MSDC_DMA_CFG_ACTIVEEN   (0x2 << 12)	/* RW */
#define MSDC_DMA_CFG_CS12B16B   (1 << 16)	/* RW */

/* MSDC_PATCH_BIT mask */
#define MSDC_PATCH_BIT_ODDSUPP    (1 <<  1)	/* RW */
#define MSDC_INT_DAT_LATCH_CK_SEL (0x7 <<  7)
#define MSDC_CKGEN_MSDC_DLY_SEL   (0x1f << 10)
#define MSDC_PATCH_BIT_IODSSEL    (1 << 16)	/* RW */
#define MSDC_PATCH_BIT_IOINTSEL   (1 << 17)	/* RW */
#define MSDC_PATCH_BIT_BUSYDLY    (0xf << 18)	/* RW */
#define MSDC_PATCH_BIT_WDOD       (0xf << 22)	/* RW */
#define MSDC_PATCH_BIT_IDRTSEL    (1 << 26)	/* RW */
#define MSDC_PATCH_BIT_CMDFSEL    (1 << 27)	/* RW */
#define MSDC_PATCH_BIT_INTDLSEL   (1 << 28)	/* RW */
#define MSDC_PATCH_BIT_SPCPUSH    (1 << 29)	/* RW */
#define MSDC_PATCH_BIT_DECRCTMO   (1 << 30)	/* RW */

#define REQ_CMD_EIO  (1 << 0)
#define REQ_CMD_TMO  (1 << 1)
#define REQ_DAT_ERR  (1 << 2)
#define REQ_STOP_EIO (1 << 3)
#define REQ_STOP_TMO (1 << 4)
#define REQ_CMD_BUSY (1 << 5)

#define MSDC_PREPARE_FLAG (1 << 0)
#define MSDC_ASYNC_FLAG (1 << 1)
#define MSDC_MMAP_FLAG (1 << 2)

#define HOST_MAX_BLKSZ      2048
#define MSDC_OCR_AVAIL      (MMC_VDD_28_29 | MMC_VDD_29_30 \
		| MMC_VDD_30_31 | MMC_VDD_31_32 | MMC_VDD_32_33)

#define CMD_TIMEOUT         (HZ/10 * 5)	/* 100ms x5 */
#define DAT_TIMEOUT         (HZ    * 5)	/* 1000ms x5 */

/*--------------------------------------------------------------------------*/
/* Descriptor Structure                                                     */
/*--------------------------------------------------------------------------*/
struct mt_gpdma_desc {
	u32 first_u32;
#define GPDMA_DESC_HWO		(1 << 0)
#define GPDMA_DESC_BDP		(1 << 1)
#define GPDMA_DESC_CHECKSUM	(0xff << 8) /* bit8 ~ bit15 */
#define GPDMA_DESC_INT		(1 << 16)
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
#define BDMA_DESC_EOL		(1 << 0)
#define BDMA_DESC_CHECKSUM	(0xff << 8) /* bit8 ~ bit15 */
#define BDMA_DESC_BLKPAD	(1 << 17)
#define BDMA_DESC_DWPAD		(1 << 18)
	u32 next;
	u32 ptr;
	u32 second_u32;
#define BDMA_DESC_BUFLEN	(0xffff) /* bit0 ~ bit15 */
};

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

static void sdr_get_field(void __iomem *reg, u32 field, u32 *val)
{
	unsigned int tv = readl(reg);

	*val = ((tv & field) >> (ffs((unsigned int)field) - 1));
}

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
		sdr_get_field(host->base + MSDC_CFG, MSDC_CFG_CKMOD, &mode);
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

static inline u32 msdc_cmd_find_resp(struct msdc_host *host,
		struct mmc_request *mrq, struct mmc_command *cmd)
{
	u32 resp;

	switch (mmc_resp_type(cmd)) {
		/* Actually, R1, R5, R6, R7 are the same */
	case MMC_RSP_R1:
		resp = 1;
		break;
	case MMC_RSP_R1B:
		resp = 7;
		break;
	case MMC_RSP_R2:
		resp = 2;
		break;
	case MMC_RSP_R3:
		resp = 3;
		break;
	case MMC_RSP_NONE:
	default:
		resp = 0;
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
					!(mrq->sbc->arg & 0xFFFF0000))
				rawcmd |= 2 << 28; /* AutoCMD23 */
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

	sdr_clr_bits(host->base + MSDC_INTEN, MSDC_INTEN_CMDRDY |
			MSDC_INTEN_RSPCRCERR | MSDC_INTEN_CMDTMO |
			MSDC_INTEN_ACMDRDY | MSDC_INTEN_ACMDCRCERR |
			MSDC_INTEN_ACMDTMO);
	writel(cmd->arg, host->base + SDC_ARG);

	switch (host->cmd_rsp) {
	case 0:
		break;
	case 2:
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

	while ((readl(host->base + SDC_STS) & SDC_STS_CMDBUSY)
			&& time_before(jiffies, tmo))
		continue;

	if (readl(host->base + SDC_STS) & SDC_STS_CMDBUSY) {
		dev_err(host->dev, "CMD bus busy detected\n");
		host->error |= REQ_CMD_BUSY;
		msdc_cmd_done(host, MSDC_INT_CMDTMO, mrq, cmd);
		return false;
	}

	if (mmc_resp_type(cmd) == MMC_RSP_R1B || cmd->data) {
		/* R1B or with data, should check SDCBUSY */
		while (readl(host->base + SDC_STS) & SDC_STS_SDCBUSY)
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
	} else {
		host->cmd = cmd;
	}
	spin_unlock_irqrestore(&host->lock, flags);

	if (!msdc_cmd_is_ready(host, mrq, cmd))
		return;

	if ((readl(host->base + MSDC_FIFOCS) & MSDC_FIFOCS_TXCNT) >> 16
		|| (readl(host->base + MSDC_FIFOCS) & MSDC_FIFOCS_RXCNT)) {
		dev_err(host->dev, "TX/RX FIFO non-empty before start of IO. Reset\n");
		msdc_reset_hw(host);
	}

	cmd->error = 0;
	rawcmd = msdc_cmd_prepare_raw_cmd(host, mrq, cmd);
	mod_delayed_work(system_wq, &host->req_timeout, DAT_TIMEOUT);

	sdr_set_bits(host->base + MSDC_INTEN, MSDC_INTEN_CMDRDY |
			MSDC_INTEN_RSPCRCERR | MSDC_INTEN_CMDTMO |
			MSDC_INTEN_ACMDRDY | MSDC_INTEN_ACMDCRCERR |
			MSDC_INTEN_ACMDTMO);
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
	}
	spin_unlock_irqrestore(&host->lock, flags);

	if (mrq->data)
		msdc_prepare_data(host, mrq);

	/* if SBC is required, we have HW option and SW option.
	 * if HW option is enabled, and SBC does not have "special" flags,
	 * use HW option,  otherwise use SW option
	 */
	if (mrq->sbc && (!mmc_card_mmc(mmc->card) ||
	    (mrq->sbc->arg & 0xFFFF0000)))
		msdc_start_command(host, mrq, mrq->sbc);
	else
		msdc_start_command(host, mrq, mrq->cmd);
}

static void msdc_pre_req(struct mmc_host *mmc, struct mmc_request *mrq,
		bool is_first_req)
{
	struct msdc_host *host = mmc_priv(mmc);
	struct mmc_data *data = mrq->data;

	if (!data)
		return;

	msdc_prepare_data(host, mrq);
	data->host_cookie |= MSDC_ASYNC_FLAG;
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

static int msdc_ops_switch_volt(struct mmc_host *mmc, struct mmc_ios *ios)
{
	struct msdc_host *host = mmc_priv(mmc);
	int min_uv, max_uv;
	int ret = 0;

	if (!IS_ERR(mmc->supply.vqmmc)) {

		if (ios->signal_voltage == MMC_SIGNAL_VOLTAGE_330) {
			min_uv = 3300000;
			max_uv = 3300000;
		} else if (ios->signal_voltage == MMC_SIGNAL_VOLTAGE_180) {
			min_uv = 1800000;
			max_uv = 1800000;
		} else {
			dev_err(host->dev, "Unsupported signal voltage!\n");
			return -EINVAL;
		}

		ret = regulator_set_voltage(mmc->supply.vqmmc, min_uv, max_uv);
		if (ret) {
			dev_err(host->dev,
					"Regulator set error %d: %d - %d\n",
					ret, min_uv, max_uv);
		}
	}

	return ret;
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

	/* disable detect SDIO device interrupt function */
	sdr_clr_bits(host->base + SDC_CFG, SDC_CFG_SDIOIDE);

	/* Configure to default data timeout */
	sdr_set_field(host->base + SDC_CFG, SDC_CFG_DTOC, 3);
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
	int ret;
	u32 ddr = 0;

	if (ios->timing == MMC_TIMING_UHS_DDR50)
		ddr = 1;

	msdc_set_buswidth(host, ios->bus_width);

	/* Suspend/Resume will do power off/on */
	switch (ios->power_mode) {
	case MMC_POWER_UP:
		msdc_init_hw(host);
		if (!IS_ERR(mmc->supply.vmmc)) {
			ret = mmc_regulator_set_ocr(mmc, mmc->supply.vmmc,
					ios->vdd);
			if (ret) {
				dev_err(host->dev, "Failed to set vmmc power!\n");
				return;
			}
		}
		break;
	case MMC_POWER_ON:
		if (!IS_ERR(mmc->supply.vqmmc)) {
			ret = regulator_enable(mmc->supply.vqmmc);
			if (ret)
				dev_err(host->dev, "Failed to set vqmmc power!\n");
		}
		break;
	case MMC_POWER_OFF:
		if (!IS_ERR(mmc->supply.vmmc))
			mmc_regulator_set_ocr(mmc, mmc->supply.vmmc, 0);

		if (!IS_ERR(mmc->supply.vqmmc))
			regulator_disable(mmc->supply.vqmmc);
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
	.post_req = msdc_post_req,
	.pre_req = msdc_pre_req,
	.request = msdc_ops_request,
	.set_ios = msdc_ops_set_ios,
	.start_signal_voltage_switch = msdc_ops_switch_volt,
};

static int msdc_drv_probe(struct platform_device *pdev)
{
	struct mmc_host *mmc;
	struct msdc_host *host;
	struct resource *res;
	int ret;

	if (!pdev->dev.of_node) {
		dev_err(&pdev->dev, "No DT found\n");
		return -EINVAL;
	}
	/* Allocate MMC host for this device */
	mmc = mmc_alloc_host(sizeof(struct msdc_host), &pdev->dev);
	if (!mmc)
		return -ENOMEM;

	host = mmc_priv(mmc);
	ret = mmc_of_parse(mmc);
	if (ret)
		goto host_free;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	host->base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(host->base)) {
		ret = PTR_ERR(host->base);
		goto host_free;
	}

	ret = mmc_regulator_get_supply(mmc);
	if (ret == -EPROBE_DEFER)
		goto host_free;

	host->src_clk = devm_clk_get(&pdev->dev, "source");
	if (IS_ERR(host->src_clk)) {
		dev_err(&pdev->dev,
				"Invalid source clock from the device tree!\n");
		ret = PTR_ERR(host->src_clk);
		goto host_free;
	}

	host->irq = platform_get_irq(pdev, 0);
	if (host->irq < 0) {
		ret = -EINVAL;
		goto host_free;
	}

	host->pinctrl = devm_pinctrl_get(&pdev->dev);
	if (IS_ERR(host->pinctrl)) {
		ret = PTR_ERR(host->pinctrl);
		dev_err(&pdev->dev, "Cannot find pinctrl!\n");
		goto host_free;
	}

	host->pins_default = pinctrl_lookup_state(host->pinctrl, "default");
	if (IS_ERR(host->pins_default)) {
		ret = PTR_ERR(host->pins_default);
		dev_err(&pdev->dev, "Cannot find pinctrl default!\n");
		goto host_free;
	}

	host->pins_uhs = pinctrl_lookup_state(host->pinctrl, "state_uhs");
	if (IS_ERR(host->pins_uhs)) {
		ret = PTR_ERR(host->pins_uhs);
		dev_err(&pdev->dev, "Cannot find pinctrl uhs!\n");
		goto host_free;
	}

	host->dev = &pdev->dev;
	host->mmc = mmc;
	/* Set host parameters to mmc */
	mmc->ops = &mt_msdc_ops;
	mmc->f_min = 260000;
	mmc->ocr_avail = MMC_VDD_30_31 | MMC_VDD_31_32 | MMC_VDD_32_33;

	mmc->caps |= MMC_CAP_ERASE | MMC_CAP_CMD23;
	/* MMC core transfer sizes tunable parameters */
	mmc->max_segs = MAX_BD_NUM;
	mmc->max_seg_size = 64 * 1024;
	mmc->max_blk_size = 2048;
	mmc->max_req_size = 512 * 1024;
	mmc->max_blk_count = mmc->max_req_size;

	host->hclk = clk_get_rate(host->src_clk);
	host->timeout_clks = 3 * 1048576;
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

	ret = devm_request_irq(&pdev->dev, (unsigned int) host->irq, msdc_irq,
		IRQF_TRIGGER_LOW | IRQF_ONESHOT, pdev->name, host);
	if (ret)
		goto release;

	ret = mmc_add_host(mmc);
	if (ret)
		goto release;

	return 0;

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

	platform_set_drvdata(pdev, NULL);
	mmc_remove_host(host->mmc);
	msdc_deinit_hw(host);

	dma_free_coherent(&pdev->dev,
			MAX_GPD_NUM * sizeof(struct mt_gpdma_desc),
			host->dma.gpd, host->dma.gpd_addr);
	dma_free_coherent(&pdev->dev, MAX_BD_NUM * sizeof(struct mt_bdma_desc),
			host->dma.bd, host->dma.bd_addr);

	mmc_free_host(host->mmc);

	return 0;
}

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
	},
};

module_platform_driver(mt_msdc_driver);
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("MediaTek SD/MMC Card Driver");
