#include <linux/kernel.h>
#include <linux/blkdev.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/dma-mapping.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/ioport.h>
#include <linux/irq.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/spinlock.h>
#include <linux/platform_device.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/sysfs.h>
#include <linux/timer.h>
#include <linux/vmalloc.h>
#include <linux/workqueue.h>
#include <linux/gpio.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/regulator/consumer.h>
#include <linux/clk.h>
#include <linux/err.h>

#include <linux/mmc/card.h>
#include <linux/mmc/core.h>
#include <linux/mmc/host.h>
#include <linux/mmc/mmc.h>
#include <linux/mmc/sd.h>
#include <linux/mmc/sdio.h>

#include "mtk-sd.h"
#include "mtk-sd-trace.h"

#define VOL_3300	(3300000)
#define VOL_1800	(1800000)
#define CLK_RATE_200MHZ	(200000000UL)
#define DRV_NAME            "mtk-msdc"

#define MMC_CMD23_ARG_SPECIAL (0xFFFF0000)
#define MSDC_CMD_INTS	(MSDC_INTEN_CMDRDY | MSDC_INTEN_RSPCRCERR | \
		MSDC_INTEN_CMDTMO | MSDC_INTEN_ACMDRDY | \
		MSDC_INTEN_ACMDCRCERR | MSDC_INTEN_ACMDTMO)

static struct device_node *gpio_node;
static struct device_node *gpio1_node;
static void __iomem *gpio_base;
static void __iomem *gpio1_base;
/* 50M -> 26/4 = 6.25 MHz */
/* VMCH is for T-card main power.
 * VMC for T-card when no emmc, for eMMC when has emmc.
 * VGP for T-card when has emmc.
 */
static bool msdc_cmd_done(struct msdc_host *host, int events,
			  struct mmc_request *mrq, struct mmc_command *cmd);
static void msdc_cmd_next(struct msdc_host *host,
		struct mmc_request *mrq, struct mmc_command *cmd);
static bool msdc_data_xfer_done(struct msdc_host *host, u32 events,
				struct mmc_request *mrq, struct mmc_data *data);
static void msdc_data_xfer_next(struct msdc_host *host,
		struct mmc_request *mrq, struct mmc_data *data);
static void msdc_set_driving(struct msdc_host *host);

/* interrupt control primitives */
static inline void msdc_cmd_ints_enable(struct msdc_host *host, bool enable)
{
	u32 wints = MSDC_CMD_INTS;

	if (enable)
		sdr_set_bits(MSDC_INTEN, wints);
	else
		sdr_clr_bits(MSDC_INTEN, wints);
}

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

static inline void msdc_data_ints_enable(struct msdc_host *host, bool enable)
{
	u32 wints = msdc_data_ints_mask(host);

	if (enable)
		sdr_set_bits(MSDC_INTEN, wints);
	else
		sdr_clr_bits(MSDC_INTEN, wints);
}

/* PIO primitives */
static inline bool msdc_use_pio_mode(struct msdc_host *host,
		struct mmc_request *mrq)
{
	struct mmc_data *data = mrq->data;
	u32 size = data ? data->blocks*data->blksz : 0;

	return size <= host->hw->max_pio;
}

static void *msdc_vmap_sg(struct scatterlist *sg)
{
	struct page **pages;
	phys_addr_t page_start;
	unsigned int page_count;
	pgprot_t prot;
	unsigned int i;
	void *vaddr;

	phys_addr_t start = (phys_addr_t)sg_phys(sg);
	size_t size = sg_dma_len(sg);

	page_start = start - offset_in_page(start);
	page_count = DIV_ROUND_UP(size + offset_in_page(start), PAGE_SIZE);

	prot = PAGE_KERNEL;

	pages = kmalloc_array(page_count, sizeof(struct page *), GFP_KERNEL);
	if (!pages) {
		pr_err("%s: Failed to allocate array for %u pages\n", __func__,
			page_count);
		return NULL;
	}

	for (i = 0; i < page_count; i++) {
		phys_addr_t addr = page_start + i * PAGE_SIZE;

		pages[i] = pfn_to_page(addr >> PAGE_SHIFT);
	}
	vaddr = vmap(pages, page_count, VM_MAP, prot);
	kfree(pages);

	return vaddr;
}

static int msdc_pio_read(struct msdc_host *host, struct mmc_data *data,
		unsigned long timeout)
{
	struct scatterlist *sg = data->sg;
	u32 num = data->sg_len;
	unsigned long tmo = jiffies + timeout;
	u32 events = 0;
	u32 count;
	u32 left = (u32)-1;


	while (num && time_before(jiffies, tmo)) {
		u32 *ptr = sg_virt(sg);
		u32 *map_ptr = 0;

		left = sg_dma_len(sg);

		if (!ptr) {
			ptr = map_ptr = msdc_vmap_sg(sg);
			BUG_ON(!ptr);
		}

		while (left > 3 && time_before(jiffies, tmo)) {
			u32 ready = msdc_rxfifocnt();

			count = (left > ready ? ready : left) >> 2;
			if (!count)
				continue;
			left -= count << 2;
			while (count--)
				*ptr++ = msdc_fifo_read32();
		}
		{
			u8 *u8ptr = (u8 *)ptr;

			while (left && time_before(jiffies, tmo)) {
				u32 ready = msdc_rxfifocnt();

				count = (left > ready ? ready : left);
				if (!count)
					continue;
				left -= count;
				while (count--)
					*u8ptr++ = msdc_fifo_read8();
			}
		}
		sg = sg_next(sg);
		num--;
		events = sdr_read32(MSDC_INT);
		if (events)
			sdr_write32(MSDC_INT, events);
		events &= MSDC_INT_XFER_COMPL | MSDC_INT_DATTMO
			| MSDC_INT_DATCRCERR;
		if (!events && !left && !num)
			events = MSDC_INT_XFER_COMPL;
		if (map_ptr)
			vunmap(map_ptr);
		if (events)
			break;
	}

	if (!events) {
		dev_err(host->dev, "PIO mode read: size=%d; left=%d; timeout\n",
			data->blksz * data->blocks, left);
		events = MSDC_INT_DATTMO;
	} else if (events & (MSDC_INT_DATTMO | MSDC_INT_DATCRCERR))
		dev_err(host->dev,
			"PIO mode read: size=%d; left=%d; events=%08X\n",
			data->blksz * data->blocks, left, events);

	return events;
}

static u32 msdc_pio_write(struct msdc_host *host, struct mmc_data *data,
		unsigned long timeout)
{
	struct scatterlist *sg = data->sg;
	u32 num = data->sg_len;
	unsigned long tmo = jiffies + timeout;
	u32 events = 0;
	u32 count;

	while (num && time_before(jiffies, tmo)) {
		u32 *ptr = sg_virt(sg);
		u32 *map_ptr = 0;
		u32 left = sg_dma_len(sg);

		if (!ptr) {
			ptr = map_ptr = msdc_vmap_sg(sg);
			BUG_ON(!ptr);
		}

		while (left > 3 && time_before(jiffies, tmo)) {
			u32 free = MSDC_FIFO_SZ - msdc_txfifocnt();

			count = (left > free ? free : left) >> 2;
			if (!count)
				continue;
			left -= count << 2;
			while (count--) {
				msdc_fifo_write32(*ptr);
				ptr++;
			}
		}
		{
			u8 *u8ptr = (u8 *)ptr;

			while (left && time_before(jiffies, tmo)) {
				u32 free = MSDC_FIFO_SZ - msdc_txfifocnt();

				count = (left > free ? free : left);
				if (!count)
					continue;
				left -= count;
				while (count--) {
					msdc_fifo_write8(*u8ptr);
					u8ptr++;
				}
			}
		}
		sg = sg_next(sg);
		num--;
		while (!events && time_before(jiffies, tmo)) {
			events = sdr_read32(MSDC_INT);
			if (events) {
				sdr_write32(MSDC_INT, events);
				events &= MSDC_INT_XFER_COMPL | MSDC_INT_DATTMO
					| MSDC_INT_DATCRCERR;
			} else if (num)
				break;
			else if (!msdc_txfifocnt())
				events = MSDC_INT_XFER_COMPL;
		}
		if (map_ptr)
			vunmap(map_ptr);
		if (events)
			break;
	}

	if (!events) {
		dev_err(host->dev, "PIO mode write: size=%d; timeout\n",
			data->blksz * data->blocks);
		events = MSDC_INT_DATTMO;
	} else if (events & (MSDC_INT_DATTMO | MSDC_INT_DATCRCERR))
		dev_err(host->dev, "PIO mode write: size=%d; events=%08X\n",
			data->blksz * data->blocks, events);

	return events;
}

/* DMA primitives */
static inline bool msdc_use_data_polling(struct msdc_host *host,
		struct mmc_request *mrq)
{
	struct mmc_data *data = mrq->data;
	u32 size = data ? data->blocks*data->blksz : 0;

	return size <= host->hw->max_dma_polling && host->hw->max_poll_time;
}

static inline void msdc_init_gpd_ex(struct mt_gpdma_desc *gpd, u8 extlen,
		u32 cmd, u32 arg, u32 blknum)
{
	gpd->extlen = extlen;
	gpd->cmd    = cmd;
	gpd->arg    = arg;
	gpd->blknum = blknum;
}

static inline void msdc_init_bd(struct mt_bdma_desc *bd, bool blkpad,
		bool dwpad, u32 dptr, u16 dlen)
{
	bd->blkpad = blkpad;
	bd->dwpad  = dwpad;
	bd->ptr    = (void *)dptr;
	bd->buflen = dlen;
}

static void msdc_dma_start(struct msdc_host *host, bool use_irq)
{
	msdc_data_ints_enable(host, use_irq);
	mb(); /* wait for pending IO to finish */
	sdr_set_field(MSDC_DMA_CTRL, MSDC_DMA_CTRL_START, 1);

	dev_dbg(host->dev, "DMA start\n");
}

static void msdc_dma_stop(struct msdc_host *host)
{
	dev_dbg(host->dev, "DMA status: 0x%8X\n", sdr_read32(MSDC_DMA_CFG));

	sdr_set_field(MSDC_DMA_CTRL, MSDC_DMA_CTRL_STOP, 1);
	while (sdr_read32(MSDC_DMA_CFG) & MSDC_DMA_CFG_STS)
		;
	mb(); /* wait for pending IO to finish */

	msdc_data_ints_enable(host, false);

	dev_dbg(host->dev, "DMA stop\n");
}

static int msdc_get_data(u8 *dst, struct mmc_data *data)
{
	int left;
	u8 *ptr;
	struct scatterlist *sg = data->sg;
	int num = data->sg_len;

	while (num) {
		left = sg_dma_len(sg);
		ptr = (u8 *) sg_virt(sg);
		memcpy(dst, ptr, left);
		sg = sg_next(sg);
		dst += left;
		num--;
	}
	return 0;
}

static u8 msdc_dma_calcs(u8 *buf, u32 len)
{
	u32 i, sum = 0;

	for (i = 0; i < len; i++)
		sum += buf[i];
	return 0xFF - (u8) sum;
}

static int msdc_dma_config(struct msdc_host *host, struct msdc_dma *dma)
{
	u32 sglen = dma->sglen;
	/* u32 i, j, num, bdlen, arg, xfersz; */
	u32 j, num, bdlen;
	u32 dma_address, dma_len;
	u8 blkpad, dwpad, chksum;
	struct scatterlist *sg = dma->sg;
	struct mt_gpdma_desc *gpd;
	struct mt_bdma_desc *bd;

	switch (dma->mode) {
	case MSDC_MODE_DMA_BASIC:
		BUG_ON(dma->xfersz > 65535);
		BUG_ON(dma->sglen != 1);
		dma_address = sg_dma_address(sg);
		dma_len = sg_dma_len(sg);
		sdr_write32(MSDC_DMA_SA, dma_address);

		sdr_set_field(MSDC_DMA_CTRL, MSDC_DMA_CTRL_LASTBUF, 1);
		sdr_write32(MSDC_DMA_LEN, dma_len);
		sdr_set_field(MSDC_DMA_CTRL, MSDC_DMA_CTRL_BRUSTSZ,
				dma->burstsz);
		sdr_set_field(MSDC_DMA_CTRL, MSDC_DMA_CTRL_MODE, 0);
		break;
	case MSDC_MODE_DMA_DESC:
		blkpad = (dma->flags & DMA_FLAG_PAD_BLOCK) ? 1 : 0;
		dwpad = (dma->flags & DMA_FLAG_PAD_DWORD) ? 1 : 0;
		chksum = (dma->flags & DMA_FLAG_EN_CHKSUM) ? 1 : 0;

		/* calculate the required number of gpd */
		num = (sglen + MAX_BD_PER_GPD - 1) / MAX_BD_PER_GPD;
		BUG_ON(num != 1);

		gpd = dma->gpd;
		bd = dma->bd;
		bdlen = sglen;

		/* modify gpd */
		/* gpd->intr = 0; */
		gpd->hwo = 1; /* hw will clear it */
		gpd->bdp = 1;
		gpd->chksum = 0; /* need to clear first. */
		gpd->chksum = (chksum ? msdc_dma_calcs((u8 *) gpd, 16) : 0);

		/* modify bd */
		for (j = 0; j < bdlen; j++) {
			dma_address = sg_dma_address(sg);
			dma_len = sg_dma_len(sg);
			msdc_init_bd(&bd[j], blkpad, dwpad, dma_address,
					dma_len);

			if (j == bdlen - 1)
				bd[j].eol = 1; /* the last bd */
			else
				bd[j].eol = 0;
			bd[j].chksum = 0; /* checksume need to clear first */
			bd[j].chksum = (chksum ?
					msdc_dma_calcs((u8 *)(&bd[j]), 16) : 0);
			sg++;
		}

		dma->used_gpd += 2;
		dma->used_bd += bdlen;

		sdr_set_field(MSDC_DMA_CFG, MSDC_DMA_CFG_DECSEN, chksum);
		sdr_set_field(MSDC_DMA_CTRL, MSDC_DMA_CTRL_BRUSTSZ,
				dma->burstsz);
		sdr_set_field(MSDC_DMA_CTRL, MSDC_DMA_CTRL_MODE, 1);

		sdr_write32(MSDC_DMA_SA, (u32) dma->gpd_addr);
		break;

	default:
		break;
	}

	dev_dbg(host->dev, "DMA_CTRL = 0x%x", sdr_read32(MSDC_DMA_CTRL));
	dev_dbg(host->dev, "DMA_CFG  = 0x%x", sdr_read32(MSDC_DMA_CFG));
	dev_dbg(host->dev, "DMA_SA   = 0x%x", sdr_read32(MSDC_DMA_SA));

	return 0;
}

static void msdc_dma_setup(struct msdc_host *host, struct msdc_dma *dma,
		struct mmc_data *data)
{
	BUG_ON(!data || data->sg_len > MAX_BD_NUM);

	dma->sg = data->sg;
	dma->flags = DMA_FLAG_EN_CHKSUM; /* DMA_FLAG_NONE; */
	dma->sglen = data->sg_len;
	dma->xfersz = data->blocks * data->blksz;
	dma->burstsz = MSDC_BRUST_64B;

	if (dma->sglen == 1 && sg_dma_len(dma->sg) <= MAX_DMA_CNT)
		dma->mode = MSDC_MODE_DMA_BASIC;
	else
		dma->mode = MSDC_MODE_DMA_DESC;

	dev_dbg(host->dev, "DMA mode<%d> sglen<%d> xfersz<%d>",
			dma->mode, dma->sglen, dma->xfersz);

	msdc_dma_config(host, dma);
}

static void msdc_prepare_data(struct msdc_host *host, struct mmc_request *mrq)
{
	struct mmc_data *data = mrq->data;

	if (!data || msdc_use_pio_mode(host, mrq))
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

	if (!data  || msdc_use_pio_mode(host, mrq)
			|| (data->host_cookie & MSDC_ASYNC_FLAG))
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
		sdr_get_field(MSDC_CFG, MSDC_CFG_CKMOD, mode);
		/*DDR mode will double the clk cycles for data timeout */
		timeout = mode >= 2 ? timeout * 2 : timeout;
		timeout = timeout > 1 ? timeout - 1 : 0;
		timeout = timeout > 255 ? 255 : timeout;
	}
	sdr_set_field(SDC_CFG, SDC_CFG_DTOC, timeout);
}

static int msdc_clk_stable(struct msdc_host *host, u32 mode, u32 div)
{
	int retry = 0;
	int cnt = 1000;
	int retry_cnt = 1;

	do {
		retry = 3;
		sdr_set_field(MSDC_CFG, MSDC_CFG_CKMOD | MSDC_CFG_CKDIV,
				(mode << 8) | ((div + retry_cnt) % 0xff));
		/* sdr_set_field(MSDC_CFG, MSDC_CFG_CKMOD, mode); */
		msdc_retry(!(sdr_read32(MSDC_CFG) & MSDC_CFG_CKSTB),
				retry, cnt, host->id);
		if (retry == 0) {
			dev_err(host->dev,
				"host->onclock(%d) failed; retry again\n",
				host->core_clkon);
			clk_disable(host->src_clk);
			clk_enable(host->src_clk);
		}
		retry = 3;
		sdr_set_field(MSDC_CFG, MSDC_CFG_CKDIV, div);
		msdc_retry(!(sdr_read32(MSDC_CFG) & MSDC_CFG_CKSTB),
				retry, cnt, host->id);
		msdc_reset_hw(host->id);
		if (retry_cnt == 2)
			break;
		retry_cnt += 1;
	} while (!retry);
	return 0;
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
		if (is_card_sdio(host) || (host->hw->flags & MSDC_SDIO_IRQ)) {
			host->saved_para.hz = hz;
#ifdef SDIO_ERROR_BYPASS
			host->sdio_error = 0;
#endif
		}
		host->mclk = 0;
		msdc_reset_hw(host->id);
		return;
	}

	msdc_irq_save(flags);

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

	msdc_clk_stable(host, mode, div);

	host->sclk = sclk;
	host->mclk = hz;
	host->ddr = ddr;
	/* need because clk changed. */
	msdc_set_timeout(host, host->timeout_ns, host->timeout_clks);

	/* printk(KERN_ERR "================"); */
	if (hz > 100000000) {
		/* EMMC/SD/SDIO use the same clock source MSDC_PLL
		 * with default clk freq is 208MHz,
		 * To meet the eMMC spec, clk freq must less than 200MHz,i
		 * so adjust it to 196.625MHz,
		 * SD/SDIO is also run at 196.625MHz
		 * in SDR104 mode(it should be 208MHz)
		 */
		/* pll_set_freq(MSDCPLL, 196625); */

		msdc_set_driving(host);
	}

	msdc_irq_restore(flags);
	dev_dbg(host->dev, "sclk: %d\n", host->sclk);
}

static void msdc_clksrc_onoff(struct msdc_host *host, u32 on)
{
	u32 div, mode;

	if (on) {
		if (!host->core_clkon) {
			/* turn on SDHC functional clock */
			if (clk_enable(host->src_clk)) {
				pr_err("msdc%d on clock failed, retry once\n",
						host->id);
				clk_disable(host->src_clk);
				clk_enable(host->src_clk);
			}
			host->core_clkon = 1;
			udelay(10);

			/* enable SD/MMC/SDIO bus clock:
			 * it will be automatically gated when the bus is idle
			 * (set MSDC_CFG_CKPDN bit to have it always on) */
			sdr_set_field(MSDC_CFG, MSDC_CFG_MODE, MSDC_SDMMC);

			sdr_get_field(MSDC_CFG, MSDC_CFG_CKMOD, mode);
			sdr_get_field(MSDC_CFG, MSDC_CFG_CKDIV, div);
			msdc_clk_stable(host, mode, div);

		}
	} else {
		if (host->core_clkon) {
			/* disable SD/MMC/SDIO bus clock */
			sdr_set_field(MSDC_CFG, MSDC_CFG_MODE, MSDC_MS);

			/* turn off SDHC functional clock */
			clk_disable(host->src_clk);
			host->core_clkon = 0;
		}
	}
}

static int msdc_gate_clock(struct msdc_host *host, int delay)
{
	int ret = 0;
	unsigned long flags;

	spin_lock_irqsave(&host->clk_gate_lock, flags);
	if (host->clk_gate_count > 0)
		host->clk_gate_count--;
	if (delay) {
		mod_timer(&host->timer, jiffies + CLK_TIMEOUT);
		dev_dbg(host->dev, "%s: clk_gate_count=%d; start delay timer\n",
			__func__, host->clk_gate_count);
	} else if (host->clk_gate_count == 0) {
		del_timer(&host->timer);
		msdc_clksrc_onoff(host, 0);
		dev_dbg(host->dev,
			"%s: clock gated: clk_gate_count=%d, delay=%d\n",
			__func__, host->clk_gate_count, delay);
	} else
		ret = -EBUSY;
	spin_unlock_irqrestore(&host->clk_gate_lock, flags);
	return ret;
}

static void msdc_ungate_clock(struct msdc_host *host)
{
	unsigned long flags;

	spin_lock_irqsave(&host->clk_gate_lock, flags);
	host->clk_gate_count++;
	dev_dbg(host->dev, "%s: clk_gate_count=%d\n",
			__func__, host->clk_gate_count);
	if (host->clk_gate_count == 1)
		msdc_clksrc_onoff(host, 1);
	spin_unlock_irqrestore(&host->clk_gate_lock, flags);
}

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


static void msdc_set_rdtdsel(struct msdc_host *host, bool sd_18)
{
	switch (host->id) {
	case 0:
		sdr_set_field(MSDC0_RDSEL_BASE, MSDC0_RDSEL, 0x0);
		sdr_set_field(MSDC0_TDSEL_BASE, MSDC0_TDSEL, 0x0);
		break;
	case 1:
		if (sd_18) {
			sdr_set_field(MSDC1_RDSEL_BASE, MSDC1_RDSEL, 0x0);
			sdr_set_field(MSDC1_TDSEL_BASE, MSDC1_TDSEL, 0x0);
		} else {
			sdr_set_field(MSDC1_RDSEL_BASE, MSDC1_RDSEL, 0x0);
			sdr_set_field(MSDC1_TDSEL_BASE, MSDC1_TDSEL, 0x5);
		}
		break;
	case 2:
		if (sd_18) {
			sdr_set_field(MSDC2_RDSEL_BASE, MSDC2_RDSEL, 0x0);
			sdr_set_field(MSDC2_TDSEL_BASE, MSDC2_TDSEL, 0x0);
		} else {
			sdr_set_field(MSDC2_RDSEL_BASE, MSDC2_RDSEL, 0x0);
			sdr_set_field(MSDC2_TDSEL_BASE, MSDC2_TDSEL, 0x5);
		}
		break;
	case 3:
		sdr_set_field(MSDC3_RDSEL_BASE, MSDC3_RDSEL, 0x0);
		sdr_set_field(MSDC3_TDSEL_BASE, MSDC3_TDSEL, 0x0);
		break;
	case 4:
		sdr_set_field(MSDC4_RDSEL_BASE, MSDC4_RDSEL, 0x0);
		sdr_set_field(MSDC4_TDSEL_BASE, MSDC4_TDSEL, 0x0);
		break;
	default:
		break;
	}
}

static struct msdc_pinctrl *msdc_lookup_pinctl(struct msdc_host *host)
{
	int i;
	struct msdc_pinctrl *pin_ctl;
	struct mmc_ios *ios = &host->mmc->ios;

	for (i = 0; i < ARRAY_SIZE(host->hw->pin_ctl); ++i) {
		pin_ctl = &host->hw->pin_ctl[i];
		if (pin_ctl->timing && !(pin_ctl->timing & (1 << ios->timing)))
			continue;
		if (pin_ctl->voltage &&
			!(pin_ctl->voltage & (1 << ios->signal_voltage)))
			continue;
		break;
	}

	host->pin_ctl = pin_ctl;

	dev_err(host->dev, "%s: idx=%d; clock=%d; timing=%d; voltage=%d\n",
			__func__, i, ios->clock, ios->timing,
			ios->signal_voltage);

	return host->pin_ctl;
}

static void msdc_set_driving(struct msdc_host *host)
{
	struct msdc_pinctrl *pin_ctl = msdc_lookup_pinctl(host);

	switch (host->id) {
	case 0:
		sdr_set_field(MSDC0_DAT_DRVING_BASE,
				MSDC0_DAT_DRVING, pin_ctl->dat_drv);
		sdr_set_field(MSDC0_CMD_DRVING_BASE,
				MSDC0_CMD_DRVING, pin_ctl->cmd_drv);
		sdr_set_field(MSDC0_CLK_DRVING_BASE,
				MSDC0_CLK_DRVING, pin_ctl->clk_drv);
		break;
	case 1:
		sdr_set_field(MSDC1_DAT_DRVING_BASE,
				MSDC1_DAT_DRVING, pin_ctl->dat_drv);
		sdr_set_field(MSDC1_CMD_DRVING_BASE,
				MSDC1_CMD_DRVING, pin_ctl->cmd_drv);
		sdr_set_field(MSDC1_CLK_DRVING_BASE,
				MSDC1_CLK_DRVING, pin_ctl->clk_drv);
		break;
	case 2:
		sdr_set_field(MSDC2_DAT_DRVING_BASE,
				MSDC2_DAT_DRVING, pin_ctl->dat_drv);
		sdr_set_field(MSDC2_CMD_DRVING_BASE,
				MSDC2_CMD_DRVING, pin_ctl->cmd_drv);
		sdr_set_field(MSDC2_CLK_DRVING_BASE,
				MSDC2_CLK_DRVING, pin_ctl->clk_drv);
		break;
	case 3:
		sdr_set_field(MSDC3_DAT_DRVING_BASE,
				MSDC3_DAT_DRVING, pin_ctl->dat_drv);
		sdr_set_field(MSDC3_CMD_DRVING_BASE,
				MSDC3_CMD_DRVING, pin_ctl->cmd_drv);
		sdr_set_field(MSDC3_CLK_DRVING_BASE,
				MSDC3_CLK_DRVING, pin_ctl->clk_drv);
		break;
	case 4:
		sdr_set_field(MSDC4_DAT_DRVING_BASE,
				MSDC4_DAT_DRVING, pin_ctl->dat_drv);
		sdr_set_field(MSDC4_CMD_DRVING_BASE,
				MSDC4_CMD_DRVING, pin_ctl->cmd_drv);
		sdr_set_field(MSDC4_CLK_DRVING_BASE,
				MSDC4_CLK_DRVING, pin_ctl->clk_drv);
		break;
	}
}

static void msdc_set_smt(struct msdc_host *host, int set_smt)
{
	switch (host->id) {
	case 0:
		sdr_set_field_discrete(MSDC0_SMT_BASE, MSDC0_SMT_CLK, set_smt);
		sdr_set_field_discrete(MSDC0_SMT_BASE, MSDC0_SMT_CMD, set_smt);
		sdr_set_field_discrete(MSDC0_SMT_BASE, MSDC0_SMT_DAT, set_smt);
		break;
	case 1:
		sdr_set_field_discrete(MSDC1_SMT_BASE, MSDC1_SMT_CLK, set_smt);
		sdr_set_field_discrete(MSDC1_SMT_BASE, MSDC1_SMT_CMD, set_smt);
		sdr_set_field_discrete(MSDC1_SMT_BASE, MSDC1_SMT_DAT, set_smt);
		break;
	case 2:
		sdr_set_field_discrete(MSDC2_SMT_BASE1, MSDC2_SMT_CLK, set_smt);
		sdr_set_field_discrete(MSDC2_SMT_BASE1, MSDC2_SMT_CMD, set_smt);
		sdr_set_field_discrete(MSDC2_SMT_BASE1,
				MSDC2_SMT_DAT1_0, set_smt);
		sdr_set_field_discrete(MSDC2_SMT_BASE2,
				MSDC2_SMT_DAT2_3, set_smt);
		break;
	case 3:
		sdr_set_field_discrete(MSDC3_SMT_BASE, MSDC3_SMT_CLK, set_smt);
		sdr_set_field_discrete(MSDC3_SMT_BASE, MSDC3_SMT_CMD, set_smt);
		sdr_set_field_discrete(MSDC3_SMT_BASE, MSDC3_SMT_DAT, set_smt);
		break;
	case 4:
		sdr_set_field_discrete(MSDC4_SMT_BASE, MSDC4_SMT_CLK, set_smt);
		sdr_set_field_discrete(MSDC4_SMT_BASE, MSDC4_SMT_CMD, set_smt);
		sdr_set_field_discrete(MSDC4_SMT_BASE, MSDC4_SMT_DAT, set_smt);
		break;
	default:
		break;
	}
}

static void msdc_pin_pud(struct msdc_host *host, int mode)
{
	switch (host->id) {
	case 0:
		sdr_set_field_discrete(MSDC0_R1_BASE, MSDC0_R1_CMD, mode);
		sdr_set_field_discrete(MSDC0_PUPD_BASE, MSDC0_PUPD_CMD, mode);
		sdr_set_field_discrete(MSDC0_R1_BASE, MSDC0_R1_DAT, mode);
		sdr_set_field_discrete(MSDC0_PUPD_BASE, MSDC0_PUPD_DAT, mode);
		break;
	case 1:
		sdr_set_field_discrete(MSDC1_PUPD_POLARITY_BASE,
				MSDC1_PUPD_CMD, mode);
		sdr_set_field_discrete(MSDC1_PUPD_ENABLE_BASE,
				MSDC1_PUPD_CMD, 1);
		sdr_set_field_discrete(MSDC1_PUPD_POLARITY_BASE,
				MSDC1_PUPD_DAT, mode);
		sdr_set_field_discrete(MSDC1_PUPD_ENABLE_BASE,
				MSDC1_PUPD_DAT, 1);
		break;
	case 2:
		sdr_set_field_discrete(MSDC2_PUPD_POLARITY_BASE2,
				MSDC2_PUPD_CMD, mode);
		sdr_set_field_discrete(MSDC2_PUPD_ENABLE_BASE2,
				MSDC2_PUPD_CMD, 1);
		sdr_set_field_discrete(MSDC2_PUPD_POLARITY_BASE1,
				MSDC2_PUPD_DAT1_0, mode);
		sdr_set_field_discrete(MSDC2_PUPD_POLARITY_BASE2,
				MSDC2_PUPD_DAT2_3, mode);
		sdr_set_field_discrete(MSDC2_PUPD_ENABLE_BASE1,
				MSDC2_PUPD_DAT1_0, 1);
		sdr_set_field_discrete(MSDC2_PUPD_ENABLE_BASE2,
				MSDC2_PUPD_DAT2_3, 1);
		break;
	case 3:
		sdr_set_field_discrete(MSDC3_R1_BASE, MSDC3_R1_CMD, mode);
		sdr_set_field_discrete(MSDC3_R1_BASE, MSDC3_R1_DAT, mode);
		sdr_set_field_discrete(MSDC3_PUPD_BASE, MSDC3_PUPD_CMD, mode);
		sdr_set_field_discrete(MSDC3_PUPD_BASE, MSDC3_PUPD_DAT, mode);
		break;
	case 4:
		sdr_set_field_discrete(MSDC4_R1_BASE1, MSDC4_R1_DAT0_7, mode);
		sdr_set_field_discrete(MSDC4_R1_BASE2,
				MSDC4_R1_DAT3 | MSDC4_R1_CMD, mode);
		sdr_set_field_discrete(MSDC4_PUPD_BASE1,
				MSDC4_PUPD_DAT0_7, mode);
		sdr_set_field_discrete(MSDC4_PUPD_BASE2,
				MSDC4_PUPD_DAT3 | MSDC4_PUPD_CMD, mode);
		break;
	default:
		break;
	}
}

static void msdc_pin_pnul(struct msdc_host *host, int mode)
{
	switch (host->id) {
	case 1:
		sdr_set_field_discrete(MSDC1_PUPD_ENABLE_BASE,
				MSDC1_PUPD_CMD, mode);
		sdr_set_field_discrete(MSDC1_PUPD_ENABLE_BASE,
				MSDC1_PUPD_DAT, mode);
		break;
	case 2:
		sdr_set_field_discrete(MSDC2_PUPD_ENABLE_BASE1,
				MSDC2_PUPD_CMD, mode);
		sdr_set_field_discrete(MSDC2_PUPD_ENABLE_BASE1,
				MSDC2_PUPD_DAT1_0, mode);
		sdr_set_field_discrete(MSDC2_PUPD_ENABLE_BASE2,
				MSDC2_PUPD_DAT2_3, mode);
		break;
	default:
		break;
	}
}

static void msdc_pin_config(struct msdc_host *host, int mode)
{
	struct msdc_hw *hw = host->hw;
	int pull = (mode == MSDC_PIN_PULL_UP) ? MSDC_PIN_PULL_UP
			: MSDC_PIN_PULL_DOWN;

	/* Config WP pin */
	if (hw->flags & MSDC_WP_PIN_EN) {
		if (hw->config_gpio_pin)
			hw->config_gpio_pin(MSDC_WP_PIN, pull);
	}

	switch (mode) {
	case MSDC_PIN_PULL_UP:
		msdc_pin_pud(host, 1);
		break;
	case MSDC_PIN_PULL_DOWN:
		msdc_pin_pud(host, 0);
		break;
	case MSDC_PIN_PULL_NONE:
	default:
		msdc_pin_pnul(host, 0);
		break;
	}

	dev_dbg(host->dev, "Pins mode(%d), down(%d), up(%d)",
			mode, MSDC_PIN_PULL_DOWN,
			MSDC_PIN_PULL_UP);
}

static void msdc_pin_reset(struct msdc_host *host, int mode)
{
	struct msdc_hw *hw = (struct msdc_hw *) host->hw;
	int pull = (mode == MSDC_PIN_PULL_UP) ? MSDC_PIN_PULL_UP
			: MSDC_PIN_PULL_DOWN;

	/* Config reset pin */
	if (hw->flags & MSDC_RST_PIN_EN) {
		if (hw->config_gpio_pin) /* NULL */
			hw->config_gpio_pin(MSDC_RST_PIN, pull);

		if (mode == MSDC_PIN_PULL_UP)
			sdr_clr_bits(EMMC_IOCON, EMMC_IOCON_BOOTRST);
		else
			sdr_set_bits(EMMC_IOCON, EMMC_IOCON_BOOTRST);
	}
}

/*
 register as callback function of WIFI(combo_sdio_register_pm) .
 can called by msdc_drv_suspend/resume too.
 */
static void msdc_save_emmc_setting(struct msdc_host *host)
{
	host->saved_para.ddr = host->ddr;
	host->saved_para.hz = host->mclk;
	host->saved_para.sdc_cfg = sdr_read32(SDC_CFG);
	sdr_get_field(MSDC_IOCON, MSDC_IOCON_RSPL, host->hw->cmd_edge);
	sdr_get_field(MSDC_IOCON, MSDC_IOCON_DSPL, host->hw->rdata_edge);
	sdr_get_field(MSDC_IOCON, MSDC_IOCON_W_DSPL, host->hw->wdata_edge);
	host->saved_para.pad_tune = sdr_read32(MSDC_PAD_TUNE);
	host->saved_para.ddly0 = sdr_read32(MSDC_DAT_RDDLY0);
	host->saved_para.ddly1 = sdr_read32(MSDC_DAT_RDDLY1);
	sdr_get_field(MSDC_PATCH_BIT0, MSDC_CKGEN_MSDC_DLY_SEL,
		host->saved_para.ckgen_msdc_dly_sel);
	sdr_get_field(MSDC_PATCH_BIT1, MSDC_PATCH_BIT1_CMD_RSP,
			host->saved_para.cmd_resp_ta_cntr);
	sdr_get_field(MSDC_PATCH_BIT1, MSDC_PATCH_BIT1_WRDAT_CRCS,
			host->saved_para.wrdat_crc_ta_cntr);
}

static void msdc_restore_emmc_setting(struct msdc_host *host)
{
	msdc_set_mclk(host, host->ddr, host->mclk);
	sdr_write32(SDC_CFG, host->saved_para.sdc_cfg);
	sdr_set_field(MSDC_IOCON, MSDC_IOCON_RSPL, host->hw->cmd_edge);
	sdr_set_field(MSDC_IOCON, MSDC_IOCON_DSPL, host->hw->rdata_edge);
	sdr_set_field(MSDC_IOCON, MSDC_IOCON_W_DSPL, host->hw->wdata_edge);
	sdr_write32(MSDC_PAD_TUNE, host->saved_para.pad_tune);
	sdr_write32(MSDC_DAT_RDDLY0, host->saved_para.ddly0);
	sdr_write32(MSDC_DAT_RDDLY1, host->saved_para.ddly1);
	sdr_set_field(MSDC_PATCH_BIT0, MSDC_CKGEN_MSDC_DLY_SEL,
		host->saved_para.ckgen_msdc_dly_sel);
	sdr_set_field(MSDC_PATCH_BIT1, MSDC_PATCH_BIT1_WRDAT_CRCS,
			host->saved_para.wrdat_crc_ta_cntr);
	sdr_set_field(MSDC_PATCH_BIT1, MSDC_PATCH_BIT1_CMD_RSP,
			host->saved_para.cmd_resp_ta_cntr);
	sdr_set_field(MSDC_PATCH_BIT1, MSDC_PATCH_BIT1_GET_CRC_MARGIN, 1);
}

static void msdc_emmc_power(struct msdc_host *host, u32 on)
{
	if (on) {
		msdc_reset_hw(host->id);
		msdc_pin_reset(host, MSDC_PIN_PULL_UP);
		msdc_pin_config(host, MSDC_PIN_PULL_UP);
	} else
		msdc_save_emmc_setting(host);
	switch (host->id) {
	case 0:
		msdc_set_smt(host, 1);
		msdc_ldo_power(host->core_power, on, VOL_3300);
		break;
	case 4:
		msdc_set_smt(host, 1);
		msdc_ldo_power(host->core_power, on, VOL_3300);
		break;
	default:
		break;
	}
	if (!on) {
		msdc_pin_config(host, MSDC_PIN_PULL_DOWN);
		msdc_pin_reset(host, MSDC_PIN_PULL_DOWN);
	} else {
		mdelay(10);
		msdc_restore_emmc_setting(host);
	}
}

static void msdc_sd_power(struct msdc_host *host, u32 on)
{
	switch (host->id) {
	case 1:
	case 2:
		msdc_set_smt(host, 1);
		msdc_set_driving(host);
		msdc_set_rdtdsel(host, 0);
		msdc_ldo_power(host->io_power, on, VOL_3300);
		msdc_ldo_power(host->core_power, on, VOL_3300);
		break;
	default:
		break;
	}
}

static void msdc_sd_power_switch(struct msdc_host *host, u32 on)
{
	switch (host->id) {
	case 1:
	case 2:
		msdc_ldo_power(host->io_power, on, VOL_1800);
		msdc_set_smt(host, 1);
		msdc_set_rdtdsel(host, 1);
		msdc_set_driving(host); /* 1 */
		break;
	default:
		break;
	}
}

static void msdc_sdio_power(struct msdc_host *host, u32 on)
{
	switch (host->id) {
	case 2:
		/* Bus & device keeps 2.8v */
		/* msdc_ldo_power(on, MT65XX_POWER_LDO_VIO28,
		   VOL_2800, &host->io_power_state); */
		host->core_power_state = host->io_power_state;
		break;
	default: /* if host->id is 3, it uses default 1.8v setting,
		    which always turns on */
		break;
	}
}

static void msdc_sdio_irq_enable(struct msdc_host *host, bool enable)
{
	struct msdc_hw *hw = host->hw;
	int sdio_irq = gpio_to_irq(hw->sdio.irq.pin);

	if (enable)
		enable_irq(sdio_irq);
	else  if (in_interrupt())
		disable_irq_nosync(sdio_irq);
	else
		disable_irq(sdio_irq);

	trace_msdc_sdio_irq_enable(host, enable);
}

static void msdc_sdio_polling_enable(struct msdc_host *host, bool enable)
{
	struct mmc_host *mmc = host->mmc;

	if (enable && (mmc->caps & MMC_CAP_SDIO_IRQ)) {
		dev_err(host->dev, "%s: switching to SDIO polling mode\n",
				__func__);
		mmc->caps &= ~MMC_CAP_SDIO_IRQ;
		msdc_sdio_irq_enable(host, false);
		mmc_signal_sdio_irq(mmc);
	}
	if (!enable && !(mmc->caps & MMC_CAP_SDIO_IRQ)) {
		dev_err(host->dev, "%s: switching to SDIO IRQ mode\n",
				__func__);
		mmc->caps |= MMC_CAP_SDIO_IRQ;
		msdc_sdio_irq_enable(host, true);
		mmc_signal_sdio_irq(mmc);
	}
}

static irqreturn_t msdc_eirq_sdio(int irq, void *data)
{
	struct msdc_host *host = (struct msdc_host *) data;

	trace_msdc_sdio_irq(host, 0);
	mmc_signal_sdio_irq(host->mmc);

	return IRQ_HANDLED;
}

/* msdc_eirq_cd will not be used!  We not using EINT for card detection. */
static void msdc_eirq_cd(void *data)
{
	struct msdc_host *host = (struct msdc_host *) data;

	dev_dbg(host->dev, "CD EINT");

	tasklet_hi_schedule(&host->card_tasklet);
}

/* detect cd interrupt */

static void msdc_tasklet_card(unsigned long arg)
{
	struct msdc_host *host = (struct msdc_host *) arg;
	struct msdc_hw *hw = host->hw;
	u32 inserted;

	if (hw->get_cd_status) { /* NULL */
		inserted = hw->get_cd_status();
	} else {
		/* status = sdr_read32(MSDC_PS); */
		if (hw->cd_level)
			inserted = (host->sd_cd_polarity == 0) ? 1 : 0;
		else
			inserted = (host->sd_cd_polarity == 0) ? 0 : 1;
	}
	dev_info(host->dev, "card found<%s>",
			inserted ? "inserted" : "removed");

	host->card_inserted = inserted;
	host->mmc->f_max = HOST_MAX_MCLK;
	host->hw->cmd_edge = 0; /* new card tuning from 0 */
	host->hw->rdata_edge = 0;
	host->hw->wdata_edge = 0;

	dev_err(host->dev, "host->suspend(%d)", host->suspend);
	if (!host->suspend && host->sd_cd_insert_work) {
		if (inserted)
			mmc_detect_change(host->mmc, msecs_to_jiffies(200));
		else
			mmc_detect_change(host->mmc, msecs_to_jiffies(10));
	}
	dev_err(host->dev, "insert_workqueue(%d)", host->sd_cd_insert_work);
}

static void msdc_set_power_mode(struct msdc_host *host, u8 mode)
{
	dev_err(host->dev, "Set power mode(%d)", mode);
	if (host->power_mode == MMC_POWER_OFF && mode != MMC_POWER_OFF) {
		msdc_pin_reset(host, MSDC_PIN_PULL_UP);
		msdc_pin_config(host, MSDC_PIN_PULL_UP);

		if (host->power_control)
			host->power_control(host, 1);
		else
			dev_err(host->dev,
					"No power control callback.\n");

		mdelay(10);
	} else if (host->power_mode != MMC_POWER_OFF && mode == MMC_POWER_OFF) {

		if (is_card_sdio(host)) {
			msdc_pin_config(host, MSDC_PIN_PULL_UP);
		} else {

			if (host->power_control)
				host->power_control(host, 0);
			else
				dev_err(host->dev,
						"No power control callback.\n");

			msdc_pin_config(host, MSDC_PIN_PULL_DOWN);
		}
		mdelay(10);
		msdc_pin_reset(host, MSDC_PIN_PULL_DOWN);
	}
	host->power_mode = mode;
}


static void msdc_ops_enable_sdio_irq(struct mmc_host *mmc, int enable)
{
	struct msdc_host *host = mmc_priv(mmc);

	if (enable && test_and_clear_bit(SDIO_IRQ_FLAG, &host->sdio_flags))
		msdc_sdio_irq_enable(host, true);
	if (!enable && !test_and_set_bit(SDIO_IRQ_FLAG, &host->sdio_flags))
		msdc_sdio_irq_enable(host, false);
}

static void msdc_sdio_power_enable(void *data, bool power_enable)
{
	struct msdc_host *host = (struct msdc_host *) data;
	struct msdc_hw *hw = host->hw;

	msdc_sdio_irq_enable(host, false);

	if (power_enable) {
		/* mt_pin_set_pull(hw->sdio.irq.pin, MT_PIN_PULL_DISABLE); */
		/* mt_pin_set_mode_eint(hw->sdio.irq.pin); */
	}

	msdc_ungate_clock(host);

	if (!power_enable && !host->suspend) {
		host->suspend = 1;

		/* AP:
		 * core functionality here is mmc_stop_host()
		 * which simply invokes mmc_power_off().
		 * MTK solution needs that power up/down code does not
		 * send any SDIO commands => normal suspend/resume can't
		 * be used here */
		/* host->mmc->pm_flags |= MMC_PM_IGNORE_PM_NOTIFY; */
		mmc_remove_host(host->mmc);
		if (hw->power_state) {
			irq_set_irq_wake(gpio_to_irq(hw->sdio.irq.pin), false);
			msdc_sdio_irq_enable(host, false);
		}
	}
	if (power_enable && host->suspend) {
		host->suspend = 0;
		host->sdio_tune_flag = 0;
		if (hw->sdio.enable_tune) {
			u32 cur_rxdly0; /* ,cur_rxdly1; */

			sdr_set_field(MSDC_IOCON, MSDC_IOCON_DDLSEL, 1);
			/* Latch edge */
			hw->cmd_edge = hw->sdio.iocon.rspl;
			hw->rdata_edge = hw->sdio.iocon.dspl;
			hw->wdata_edge = hw->sdio.iocon.w_dspl;

			sdr_set_field(MSDC_IOCON, MSDC_IOCON_RSPL,
					hw->sdio.iocon.rspl);
			sdr_set_field(MSDC_IOCON, MSDC_IOCON_DSPL,
					hw->sdio.iocon.dspl);
			sdr_set_field(MSDC_IOCON, MSDC_IOCON_W_DSPL,
					hw->sdio.iocon.w_dspl);
			/* CMD delay */
			sdr_set_field(MSDC_PAD_TUNE, MSDC_PAD_TUNE_CMDRDLY,
					hw->sdio.pad_tune.rdly);
			sdr_set_field(MSDC_PAD_TUNE, MSDC_PAD_TUNE_CMDRRDLY,
					hw->sdio.pad_tune.rrdly);
			sdr_set_field(MSDC_PAD_TUNE, MSDC_PAD_TUNE_DATWRDLY,
					hw->sdio.pad_tune.wrdly);
			sdr_set_field(MSDC_PAD_TUNE, MSDC_PAD_TUNE_DATRRDLY,
					(u32)hw->sdio.dat_rd.dly0[0]);
			cur_rxdly0 = (hw->sdio.dat_rd.dly0[0] << 24) |
				(hw->sdio.dat_rd.dly0[1] << 16) |
				(hw->sdio.dat_rd.dly0[2] << 8) |
				(hw->sdio.dat_rd.dly0[3] << 0);

			sdr_write32(MSDC_DAT_RDDLY0, cur_rxdly0);
			host->saved_para.pad_tune = sdr_read32(MSDC_PAD_TUNE);
			host->saved_para.ddly0 = sdr_read32(MSDC_DAT_RDDLY0);
		}

		/* AP:
		 * core functionality here is mmc_start_host()
		 * which simply invokes mmc_power_on().
		 * MTK solution needs that power up/down code does not
		 * send any SDIO commands => normal suspend/resume can't
		 * be used here */
		/* host->mmc->pm_flags |= MMC_PM_IGNORE_PM_NOTIFY; */
		mmc_add_host(host->mmc);
		irq_set_irq_wake(gpio_to_irq(hw->sdio.irq.pin), true);
		msdc_sdio_irq_enable(host, true);
	}

#ifdef SDIO_ERROR_BYPASS
	if (is_card_sdio(host))
		host->sdio_error = 0;
#endif

	/* gate clock at the last step when suspend. */
	if (host->suspend)
		msdc_gate_clock(host, 0);
	else
		msdc_gate_clock(host, 1);

	if (!power_enable) {
		/*pr_info("[mt6620] set WIFI_EINT input pull down\n"); */
		/* mt_pin_set_mode_gpio(hw->sdio.irq.pin); */
		/* gpio_direction_input(hw->sdio.irq.pin); */
		/* mt_pin_set_pull(hw->sdio.irq.pin, MT_PIN_PULL_ENABLE_UP); */
	}

	hw->power_state = power_enable;
	msdc_sdio_irq_enable(host, true);
}

/*
 * This whole offset calc needs to go away immediately.
 * It exists to
 */
#define MBR_START_ADDRESS_BYTE			(16*1024*1024)

/* returns total size of MMC partitions 1..7 */
static u32 msdc_get_other_capacity(struct msdc_host *host)
{
	int i;
	const u8 *ext_csd = host->ext_csd;
	/* 2 Boot partitions + 1 RPMB partition */
	u32 device_other_capacity =
		(u32) ext_csd[EXT_CSD_BOOT_MULT] * 2 * 128 * 1024 +
		(u32) ext_csd[EXT_CSD_RPMB_MULT] * 128 * 1024;

	/* 4 GP partitions */
	for (i = 0; i < 4; ++i) {
		device_other_capacity +=
			((u32)ext_csd[EXT_CSD_GP_SIZE_MULT + 2 + 3 * i]
				<< 16) +
			((u32)ext_csd[EXT_CSD_GP_SIZE_MULT + 1 + 3 * i]
				<< 8) +
			(u32)ext_csd[EXT_CSD_GP_SIZE_MULT + 0 + 3 * i];
	}
	return device_other_capacity;
}

static void msdc_cal_offset(struct msdc_host *host)
{
	if (host->offset_valid)
		return;

	host->offset = MBR_START_ADDRESS_BYTE - msdc_get_other_capacity(host);

	if (host->mmc->card->csd.capacity  == MAX_CSD_CAPACITY)
		host->offset >>= 9;

	dev_err(host->dev, "%s: csd_capacity=0x%08X; offset=0x%08X\n",
		__func__, host->mmc->card->csd.capacity, host->offset);

	BUG_ON(host->offset < 0);

	host->offset_valid = true;
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

static inline u32 msdc_get_irq_status(struct msdc_host *host, u32 mask)
{
	u32 status = sdr_read32(MSDC_INT);

	if (status)
		sdr_write32(MSDC_INT, status);
	return status & mask;
}

static inline bool msdc_cmd_is_ready(struct msdc_host *host,
		struct mmc_request *mrq, struct mmc_command *cmd)
{
	unsigned long tmo = jiffies + msecs_to_jiffies(20);

	while (sdc_is_cmd_busy() && time_before(jiffies, tmo))
		continue;

	if (sdc_is_cmd_busy()) {
		dev_err(host->dev,
			"CMD bus busy detected: last cmd=%d; arg=%08X\n",
			host->last_cmd.cmd, host->last_cmd.arg);
		host->error |= REQ_CMD_BUSY;
		msdc_cmd_done(host, MSDC_INT_CMDTMO, mrq, cmd);
		return false;
	}
	return true;
}

static inline u32 msdc_cmd_find_resp(struct msdc_host *host,
		struct mmc_request *mrq, struct mmc_command *cmd)
{
	u32 resp;
	u32 opcode = cmd->opcode;

	/* Protocol layer does not provide response type, but our hardware needs
	 * to know exact type, not just size
	 */
	if (opcode == MMC_SEND_OP_COND || opcode == SD_APP_OP_COND)
		resp = RESP_R3;
	else if (opcode == MMC_SET_RELATIVE_ADDR
			|| opcode == SD_SEND_RELATIVE_ADDR)
		resp = (mmc_cmd_type(cmd) == MMC_CMD_BCR) ? RESP_R6 : RESP_R1;
	else if (opcode == MMC_FAST_IO)
		resp = RESP_R4;
	else if (opcode == MMC_GO_IRQ_STATE)
		resp = RESP_R5;
	else if (opcode == MMC_SELECT_CARD)
		resp = cmd->arg ? RESP_R1B : RESP_NONE;
	else if (opcode == MMC_ERASE)
		resp = RESP_R1B;
	else if (opcode == SD_IO_RW_DIRECT || opcode == SD_IO_RW_EXTENDED)
		resp = RESP_R1; /* SDIO workaround. */
	else if (opcode == SD_SEND_IF_COND
			&& (mmc_cmd_type(cmd) == MMC_CMD_BCR))
		resp = RESP_R1;
	else {
		switch (mmc_resp_type(cmd)) {
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

		BUG_ON(data->blksz > HOST_MAX_BLKSZ);

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

		if (msdc_use_pio_mode(host, mrq))
			msdc_dma_off();
		else
			msdc_dma_on();

		if (((host->timeout_ns != data->timeout_ns) ||
		    (host->timeout_clks != data->timeout_clks)))
			msdc_set_timeout(host, data->timeout_ns,
					data->timeout_clks);

		sdr_write32(SDC_BLK_NUM, data->blocks);
	}
	return rawcmd;
}

static inline u32 msdc_cmd_prepare_raw_arg(struct msdc_host *host,
		struct mmc_request *mrq, struct mmc_command *cmd)
{
	u32 rawarg = cmd->arg;

	if (host->hw->host_function == MSDC_EMMC &&
		host->hw->boot == MSDC_BOOT_EN &&
		(cmd->opcode == MMC_READ_SINGLE_BLOCK ||
		cmd->opcode == MMC_READ_MULTIPLE_BLOCK ||
		cmd->opcode == MMC_WRITE_BLOCK ||
		cmd->opcode == MMC_WRITE_MULTIPLE_BLOCK ||
		cmd->opcode == MMC_ERASE_GROUP_START ||
		cmd->opcode == MMC_ERASE_GROUP_END) &&
		(host->partition_access == 0)) {

		if (!host->offset_valid) {
			if (cmd->arg) {
				dev_err(host->dev,
					"%s: first access: cmd=%08X arg=%08X\n",
					__func__, cmd->opcode, cmd->arg);
				WARN_ON(1);
			}
			msdc_cal_offset(host);
		}

		rawarg += host->offset;
		if (cmd->opcode == MMC_ERASE_GROUP_START)
			host->erase_start = rawarg;
		if (cmd->opcode == MMC_ERASE_GROUP_END)
			host->erase_end = rawarg;
	}
	if (cmd->opcode == MMC_ERASE &&
	    (cmd->arg == MMC_SECURE_ERASE_ARG ||
	     cmd->arg == MMC_ERASE_ARG) &&
	     host->mmc->card &&
	     host->hw->host_function == MSDC_EMMC &&
	     host->hw->boot == MSDC_BOOT_EN &&
	     !mmc_erase_group_aligned(host->mmc->card,
		     host->erase_start, host->erase_end)) {
		if (cmd->arg == MMC_SECURE_ERASE_ARG &&
		    mmc_can_secure_erase_trim(host->mmc->card))
			rawarg = MMC_SECURE_TRIM1_ARG;
		else if (cmd->arg == MMC_ERASE_ARG ||
		    (cmd->arg == MMC_SECURE_ERASE_ARG &&
		    !mmc_can_secure_erase_trim(host->mmc->card)))
			rawarg = MMC_TRIM_ARG;
	}
	return rawarg;
}

static inline int msdc_cmd_do_poll(struct msdc_host *host,
		struct mmc_request *mrq, struct mmc_command *cmd)
{
	unsigned long long start, now, end_busy, end_poll;
	u32 wait_usec;
	u32 intsts;
	bool done = false;

	preempt_disable();
	start = sched_clock();
	end_poll = start + host->hw->max_poll_time*NSEC_PER_USEC;
	end_busy = start + host->hw->max_busy_time*NSEC_PER_USEC;
	if (host->hw->max_poll_time < host->hw->max_busy_time)
		end_poll = end_busy;
	wait_usec = host->hw->poll_sleep_base;
	host->disable_next = true;
	for (now = start; time_before_eq64(now, end_busy) && !done;
			now = sched_clock()) {
		intsts = msdc_get_irq_status(host, MSDC_CMD_INTS);
		done = intsts && msdc_cmd_done(host, intsts, mrq, cmd);
	}
	host->disable_next = false;
	preempt_enable_no_resched();
	if (done) {
		msdc_cmd_next(host, mrq, cmd);
		return 0;
	}
	trace_msdc_command_start(host->id, cmd->opcode, cmd->arg, 1);
	do {
		intsts = msdc_get_irq_status(host, MSDC_CMD_INTS);
		done = intsts && msdc_cmd_done(host, intsts, mrq, cmd);
		if (done)
			return 0;
		now = sched_clock();
		if (wait_usec) {
			usleep_range(wait_usec, 2 * wait_usec);
			wait_usec <<= 1;
		} else
			udelay(1);
	} while (time_before_eq64(now, end_poll));

	trace_msdc_command_start(host->id, cmd->opcode, cmd->arg, 2);

	now = sched_clock();
	dev_warn(host->dev, "%s: SW poll timeout: %lld ns\n",
			__func__, now - start);

	return -ETIMEDOUT;
}

static void msdc_start_command(struct msdc_host *host,
		struct mmc_request *mrq, struct mmc_command *cmd)
{
	u32 rawcmd;
	u32 rawarg;
	bool use_irq;
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

	if (msdc_txfifocnt() || msdc_rxfifocnt()) {
		dev_err(host->dev, "TX/RX FIFO non-empty before start of IO. Reset\n");
		msdc_reset_hw(host->id);
	}

	cmd->error = 0;
	rawcmd = msdc_cmd_prepare_raw_cmd(host, mrq, cmd);
	rawarg = msdc_cmd_prepare_raw_arg(host, mrq, cmd);
	msdc_set_cmd_timeout(host, cmd);
	trace_msdc_command_start(host->id, cmd->opcode, cmd->arg, 0);

	dev_dbg(host->dev, "%s: cmd=%d arg=%08X cmd_reg=%08X; has_busy=%d\n",
		__func__, cmd->opcode, cmd->arg, rawcmd,
		host->cmd_rsp == RESP_R1B);

	use_irq =
		!host->hw->max_poll_time ||
		!host->hw->max_busy_time ||
		host->mclk < 26000000;

	msdc_cmd_ints_enable(host, use_irq);
	sdc_send_cmd(rawcmd, rawarg);

	if (!use_irq) {
		if (msdc_cmd_do_poll(host, mrq, cmd) < 0) {
			dev_warn(host->dev,
				"%s: cmd=%d arg=%08X;SW poll timeout\n",
				__func__, cmd->opcode, cmd->arg);

			/* fall back to IRQ mode */
			local_irq_save(flags);
			msdc_cmd_ints_enable(host, true);
			local_irq_restore(flags);
		}
	}
}

static void msdc_start_data(struct msdc_host *host, struct mmc_request *mrq,
			    struct mmc_command *cmd, struct mmc_data *data)
{
	bool read;
	unsigned long flags;

	BUG_ON(!cmd || !data);
	BUG_ON(!host->mrq || host->cmd || host->data);
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

	host->latest_operation_type =
		read ? OPER_TYPE_READ : OPER_TYPE_WRITE;

	msdc_set_data_timeout(host, cmd, data);
	if (msdc_use_pio_mode(host, mrq)) {
		/*
		 *  Guard timer should be >= 10ms.
		 * It is enough, 'cause expected transaction duration
		 * is much less than 1ms.
		 * If some transactions take longer than 10ms,
		 * we better have failure here,
		 * and update the logic to route such transactions
		 * to DMA+IRQ path.
		 */
		unsigned long timeout = msecs_to_jiffies(20);
		u32 events;

		preempt_disable();
		events = read ?
			msdc_pio_read(host, data, timeout) :
			msdc_pio_write(host, data, timeout);
		preempt_enable_no_resched();

		msdc_data_xfer_done(host, events, mrq, data);
	} else {
		bool use_irq = !msdc_use_data_polling(host, mrq);

		msdc_dma_setup(host, &host->dma, data);
		msdc_dma_start(host, use_irq);
		dev_dbg(host->dev, "%s: cmd=%d DMA data: %d blocks; read=%d\n",
			__func__, cmd->opcode, data->blocks, read);
		if (!use_irq) {
			unsigned long flags;
			u32 data_ints = msdc_data_ints_mask(host);
			u64 now;
			u64 start = sched_clock();
			u64 busy_end = start +
				((u64)host->hw->max_busy_time)*NSEC_PER_USEC;
			u64 poll_end = start +
				((u64)host->hw->max_poll_time)*NSEC_PER_USEC;
			int wait_us = host->hw->poll_sleep_base;
			bool done = false;

			/* SDIO data transfer should not
			 * take more than 1..2 ms, but SDIO spec requires
			 * us to wait worst case 1000 ms */

			preempt_disable();
			host->disable_next = true;
			for (now = start; time_before64(now, busy_end) && !done;
					now = sched_clock()) {
				u32 ints = msdc_get_irq_status(host, data_ints);

				done = ints && msdc_data_xfer_done(host,
						ints, mrq, data);
			}
			host->disable_next = false;
			preempt_enable_no_resched();
			if (done) {
				msdc_data_xfer_next(host, mrq, data);
				return;
			}
			for (; time_before64(now, poll_end);
					now = sched_clock()) {
				u32 ints = msdc_get_irq_status(host, data_ints);

				done = ints && msdc_data_xfer_done(host, ints,
						mrq, data);
				if (done)
					return;
				if (wait_us) {
					usleep_range(wait_us, wait_us*2);
					wait_us *= 2;
				} else
					udelay(1);
			}

			now = sched_clock();
			dev_warn(host->dev,
				"%s: SW poll data timeout: %lld ns; CMD%d\n",
					__func__, now - start, cmd->opcode);

			local_irq_save(flags);
			msdc_data_ints_enable(host, true);
			local_irq_restore(flags);
		}
	}
}

static void msdc_validate_rsp(struct msdc_host *host, struct mmc_command *cmd)
{
	u32 status = 0;
	u32 mask = is_card_sdio(host) ? 0xCF00 : 0xFFFF0000;

	if (host->cmd_rsp != RESP_NONE && host->cmd_rsp != RESP_R2)
		status = cmd->resp[0];
	if ((status & mask) && !cmd->error)
		dev_err(host->dev,
			"%s: CMD is OK, but card responded with status=%08X\n",
			__func__, status);
}

static int msdc_auto_cmd_done(struct msdc_host *host, int events,
		struct mmc_command *cmd)
{
	u32 *rsp = &cmd->resp[0];

	rsp[0] = sdr_read32(SDC_ACMD_RESP);
	msdc_validate_rsp(host, cmd);

	if (events & MSDC_INT_ACMDRDY)
		cmd->error = 0;
	else {
		msdc_reset_hw(host->id);
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
	trace_msdc_command_rsp(host->id, cmd->opcode, cmd->error, cmd->resp[0]);
	return (int)(cmd->error);
}

static void msdc_track_cmd_data(struct msdc_host *host,
				struct mmc_command *cmd, struct mmc_data *data)
{
	/* AP: track current MMC partition: this is necessary
	 * to manage host->offset it will go away, once offset-related
	 * logic is removed from preloader/lk */
	if (host->hw->host_function == MSDC_EMMC &&
		host->hw->boot == MSDC_BOOT_EN &&
		cmd->opcode == MMC_SWITCH &&
		(((cmd->arg >> 16) & 0xFF) == EXT_CSD_PART_CONFIG))
		host->partition_access = (cmd->arg >> 8)
			& EXT_CSD_PART_CONFIG_ACC_MASK;
	if (data && cmd->opcode == MMC_SEND_EXT_CSD &&
		((host->hw->flags & MSDC_REMOVABLE) ||
		!host->offset_valid)) {
		msdc_get_data(host->ext_csd, data);
		host->offset_valid = false;
	}

	if (host->error)
		dev_err(host->dev, "%s: cmd=%d arg=%08X; host->error=0x%08X\n",
			__func__, cmd->opcode, cmd->arg, host->error);
}

static void msdc_request_done(struct msdc_host *host, struct mmc_request *mrq)
{
	unsigned long flags;
	bool active;

	spin_lock_irqsave(&host->lock, flags);
	cancel_delayed_work(&host->req_timeout);
	active = host->mrq != NULL;
	host->mrq = NULL;
	spin_unlock_irqrestore(&host->lock, flags);

	if (active) {
		host->mrq = 0;
		host->request_ts = jiffies;
		msdc_track_cmd_data(host, mrq->cmd, mrq->data);
		msdc_gate_clock(host, 1);
		msdc_unprepare_data(host, mrq);
	}
	mmc_request_done(host->mmc, mrq);
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

/* returns true if command is fully handled; returns false otherwise */
static bool msdc_cmd_done(struct msdc_host *host, int events,
			  struct mmc_request *mrq, struct mmc_command *cmd)
{
	bool done = false;
	bool sbc_error;
	bool do_next;

	BUG_ON(!mrq || !cmd);

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
		do_next = !host->disable_next;
		spin_unlock_irqrestore(&host->lock, flags);

		if (done)
			return true;

		msdc_cmd_ints_enable(host, false);

		switch (host->cmd_rsp) {
		case RESP_NONE:
			break;
		case RESP_R2:
			rsp[0] = sdr_read32(SDC_RESP3);
			rsp[1] = sdr_read32(SDC_RESP2);
			rsp[2] = sdr_read32(SDC_RESP1);
			rsp[3] = sdr_read32(SDC_RESP0);
			break;
		default: /* Response types 1, 3, 4, 5, 6, 7(1b) */
			rsp[0] = sdr_read32(SDC_RESP0);
			break;
		}
		if (!sbc_error && !(events & MSDC_INT_CMDRDY)) {
			msdc_reset_hw(host->id);
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
			trace_msdc_command_rsp(host->id, cmd->opcode,
					cmd->error, cmd->resp[0]);
			host->last_cmd.cmd = cmd->opcode;
			host->last_cmd.type = host->cmd_rsp;
			host->last_cmd.arg = cmd->arg;
			msdc_validate_rsp(host, cmd);
		}

		if (do_next)
			msdc_cmd_next(host, mrq, cmd);
		done = true;
	}
	return done;
}

static void msdc_restore_info(struct msdc_host *host)
{
	int retry = 3;

	mb(); /* synchronize memory view with other CPUs */
	msdc_reset_hw(host->id);
	host->saved_para.msdc_cfg = host->saved_para.msdc_cfg & ~BIT(5);
	dev_err(host->dev, "%s: host->saved_para.msdc_cfg=0x%08X\n",
			__func__, host->saved_para.msdc_cfg);
	sdr_write32(MSDC_CFG, host->saved_para.msdc_cfg);

	while (retry--) {
		msdc_set_mclk(host, host->saved_para.ddr, host->saved_para.hz);
		if (sdr_read32(MSDC_CFG) != host->saved_para.msdc_cfg) {
			dev_err(host->dev, "set_mclk is unstable");
			dev_err(host->dev,
				"cur_cfg=%x, save_cfg=%x, cur_hz=%d, save_hz=%d"
			, sdr_read32(MSDC_CFG), host->saved_para.msdc_cfg,
				host->mclk, host->saved_para.hz);
		} else
			break;
	}

	sdr_set_field(MSDC_PATCH_BIT0, MSDC_INT_DAT_LATCH_CK_SEL,
		host->saved_para.int_dat_latch_ck_sel); /* for SDIO 3.0 */
	sdr_set_field(MSDC_PATCH_BIT0, MSDC_CKGEN_MSDC_DLY_SEL,
		host->saved_para.ckgen_msdc_dly_sel); /* for SDIO 3.0 */
	sdr_set_field(MSDC_PATCH_BIT1, MSDC_PATCH_BIT1_CMD_RSP,
		host->saved_para.cmd_resp_ta_cntr); /* for SDIO 3.0 */
	sdr_set_field(MSDC_PATCH_BIT1, MSDC_PATCH_BIT1_WRDAT_CRCS,
		host->saved_para.wrdat_crc_ta_cntr); /* for SDIO 3.0 */
	sdr_write32(MSDC_DAT_RDDLY0, host->saved_para.ddly0);
	sdr_write32(MSDC_PAD_TUNE, host->saved_para.pad_tune);
	sdr_write32(SDC_CFG, host->saved_para.sdc_cfg);
	sdr_write32(MSDC_IOCON, host->saved_para.iocon);
}

static void msdc_do_request(struct msdc_host *host, struct mmc_request *mrq)
{
	unsigned long flags;
	/* request sanity check */
	BUG_ON(!mrq || !mrq->cmd);
	/* device state sanity check */
	BUG_ON(host->mrq || host->cmd || host->data);

	if (!is_card_present(host) || host->power_mode == MMC_POWER_OFF) {
		dev_err(host->dev, "cmd<%d> arg<0x%x> card<%d> power<%d>",
				mrq->cmd->opcode, mrq->cmd->arg,
				is_card_present(host), host->power_mode);
		mrq->cmd->error = (unsigned int) -ENOMEDIUM;

		msdc_request_done(host, mrq);
		return;
	}

	host->tune = 0;
	host->error = 0;
	host->power_cycle_enable = 1;

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

	msdc_ungate_clock(host);
	msdc_prepare_data(host, mrq);

	/* if SBC is required, we have HW option and SW option.
	 * if HW option is enabled, and SBC does not have "special" flags,
	 * use HW option,  otherwise use SW option */
	if (mrq->sbc && (host->autocmd != MSDC_AUTOCMD23 ||
	    (mrq->sbc->arg & MMC_CMD23_ARG_SPECIAL)))
		msdc_start_command(host, mrq, mrq->sbc);
	else
		msdc_start_command(host, mrq, mrq->cmd);
}

static void msdc_dma_clear(struct msdc_host *host)
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

	BUG_ON(!cmd);
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

#ifdef TUNE_FLOW_TEST
static void msdc_reset_para(struct msdc_host *host)
{
	u32 dsmpl, rsmpl;

	/* because we have a card, which must work at dsmpl<0> and rsmpl<0> */

	sdr_get_field(MSDC_IOCON, MSDC_IOCON_DSPL, dsmpl);
	sdr_get_field(MSDC_IOCON, MSDC_IOCON_RSPL, rsmpl);

	if (dsmpl == 0) {
		sdr_set_field(MSDC_IOCON, MSDC_IOCON_DSPL, 1);
		dev_err(host->dev, "set dspl<0>");
		sdr_set_field(MSDC_PAD_TUNE, MSDC_PAD_TUNE_CMDRDLY, 0);
	}

	if (rsmpl == 0) {
		sdr_set_field(MSDC_IOCON, MSDC_IOCON_RSPL, 1);
		dev_err(host->dev, "set rspl<0>");
		sdr_write32(MSDC_DAT_RDDLY0, 0);
		sdr_set_field(MSDC_PAD_TUNE, MSDC_PAD_TUNE_DATWRDLY, 0);
	}
}
#endif
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
	struct mmc_command *cmd = mrq->cmd;
	struct mmc_command *stop = data->stop;
	unsigned long flags;
	bool done;
	bool do_next;

	bool check_stop = (stop && host->autocmd == MSDC_AUTOCMD12 &&
	    (events & (MSDC_INT_ACMDRDY
		       | MSDC_INT_ACMDCRCERR | MSDC_INT_ACMDTMO)));
	bool check_data = (events &
	    (MSDC_INT_XFER_COMPL | MSDC_INT_DATCRCERR | MSDC_INT_DATTMO
	     | MSDC_INT_DMA_BDCSERR | MSDC_INT_DMA_GPDCSERR
	     | MSDC_INT_DMA_PROTECT));

	BUG_ON(!host || !mrq || !data || !cmd);

	spin_lock_irqsave(&host->lock, flags);
	done = !host->data;
	if (check_data)
		host->data = NULL;
	do_next = !host->disable_next;
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
		if (!msdc_use_pio_mode(host, mrq))
			msdc_dma_stop(host);

		if ((events & MSDC_INT_XFER_COMPL) && (!stop || !stop->error))
			data->bytes_xfered = data->blocks * data->blksz;
		else {
			msdc_reset_hw(host->id);
			msdc_clr_fifo(host->id);
			host->error |= REQ_DAT_ERR;
			data->bytes_xfered = 0;

			if (events & MSDC_INT_DATTMO)
				data->error = (unsigned int) -ETIMEDOUT;

			dev_err(host->dev, "%s: cmd=%d; blocks=%d",
				__func__, mrq->cmd->opcode, data->blocks);
			dev_err(host->dev, "data_error=%d xfer_size=%d\n",
					(int)data->error, data->bytes_xfered);
		}
		trace_msdc_data_xfer(host, data->flags, host->error,
				data->blksz * data->blocks);
		msdc_dma_clear(host);

		if (do_next)
			msdc_data_xfer_next(host, mrq, data);
		done = true;
	}
	return done;
}

static void msdc_ops_request(struct mmc_host *mmc, struct mmc_request *mrq)
{
	struct msdc_host *host = mmc_priv(mmc);

	BUG_ON(mmc == NULL);
	BUG_ON(mrq == NULL);

	if (host->shutdown) {
		mrq->cmd->error = -ENOMEDIUM;
		pr_notice("%s: cmd=%d not allowed [shutdown]\n",
			__func__, mrq->cmd->opcode);
		msdc_request_done(host, mrq);
	} else
		msdc_do_request(host, mrq);
}

static void msdc_set_buswidth(struct msdc_host *host, u32 width)
{
	u32 val = sdr_read32(SDC_CFG);

	val &= ~SDC_CFG_BUSWIDTH;

	switch (width) {
	default:
	case MMC_BUS_WIDTH_1:
		width = 1;
		val |= (MSDC_BUS_1BITS << 16);
		break;
	case MMC_BUS_WIDTH_4:
		val |= (MSDC_BUS_4BITS << 16);
		break;
	case MMC_BUS_WIDTH_8:
		val |= (MSDC_BUS_8BITS << 16);
		break;
	}

	sdr_write32(SDC_CFG, val);
	dev_dbg(host->dev, "Bus Width = %d", width);
}

static void msdc_ops_set_ios(struct mmc_host *mmc, struct mmc_ios *ios)
{
	struct msdc_host *host = mmc_priv(mmc);
	struct msdc_hw *hw = host->hw;
	u32 ddr = 0;


	if (ios->timing == MMC_TIMING_UHS_DDR50)
		ddr = 1;
	msdc_ungate_clock(host);

	msdc_set_buswidth(host, ios->bus_width);

	/* Power control ??? */
	switch (ios->power_mode) {
	case MMC_POWER_OFF:
	case MMC_POWER_UP:
		msdc_set_driving(host);
		msdc_set_power_mode(host, ios->power_mode);
		break;
	case MMC_POWER_ON:
		host->power_mode = MMC_POWER_ON;
		break;
	default:
		break;
	}

	if (host->mclk != ios->clock || host->ddr != ddr) {
		/* not change when clock Freq. not changed ddr need set clock */
		if (ios->clock >= 25000000) {
			if (ios->clock > 100000000)
				msdc_set_driving(host);
			sdr_set_field(MSDC_IOCON, MSDC_IOCON_RSPL,
					hw->cmd_edge);
			sdr_set_field(MSDC_IOCON, MSDC_IOCON_DSPL,
					hw->rdata_edge);
			sdr_set_field(MSDC_IOCON, MSDC_IOCON_W_DSPL,
					hw->wdata_edge);
			sdr_write32(MSDC_PAD_TUNE, host->saved_para.pad_tune);
			sdr_write32(MSDC_DAT_RDDLY0, host->saved_para.ddly0);
			sdr_write32(MSDC_DAT_RDDLY1, host->saved_para.ddly1);
			sdr_set_field(MSDC_PATCH_BIT0, MSDC_CKGEN_MSDC_DLY_SEL,
				host->hw->ckgen_msdc_dly_sel);
			sdr_set_field(MSDC_PATCH_BIT1,
					MSDC_PATCH_BIT1_WRDAT_CRCS,
					host->saved_para.wrdat_crc_ta_cntr);
			sdr_set_field(MSDC_PATCH_BIT1, MSDC_PATCH_BIT1_CMD_RSP,
					host->saved_para.cmd_resp_ta_cntr);
			sdr_set_field(MSDC_PATCH_BIT1,
					MSDC_PATCH_BIT1_GET_CRC_MARGIN, 1);

		}

		if (!ios->clock) {
			if (ios->power_mode == MMC_POWER_OFF) {
				sdr_get_field(MSDC_IOCON, MSDC_IOCON_RSPL,
						hw->cmd_edge);
				sdr_get_field(MSDC_IOCON, MSDC_IOCON_DSPL,
						hw->rdata_edge);
				sdr_get_field(MSDC_IOCON, MSDC_IOCON_W_DSPL,
						hw->wdata_edge);
				host->saved_para.pad_tune =
					sdr_read32(MSDC_PAD_TUNE);
				host->saved_para.ddly0 =
					sdr_read32(MSDC_DAT_RDDLY0);
				host->saved_para.ddly1 =
					sdr_read32(MSDC_DAT_RDDLY1);
				sdr_get_field(MSDC_PATCH_BIT1,
					MSDC_PATCH_BIT1_CMD_RSP,
					host->saved_para.cmd_resp_ta_cntr);
				sdr_get_field(MSDC_PATCH_BIT1,
					MSDC_PATCH_BIT1_WRDAT_CRCS,
					host->saved_para.wrdat_crc_ta_cntr);
			}
		}
		msdc_set_mclk(host, ddr, ios->clock);
	}
	msdc_gate_clock(host, 1);
}

static int msdc_ops_get_cd(struct mmc_host *mmc)
{
	struct msdc_host *host = mmc_priv(mmc);
	int inserted;

	if (is_card_sdio(host)) {
		host->card_inserted	= !host->suspend;
		goto end;
	}

	/* for emmc, MSDC_REMOVABLE not set, always return 1 */
	if (!(host->hw->flags & MSDC_REMOVABLE)) {
		host->card_inserted = 1;
		goto end;
	}
	if (host->hw->flags & MSDC_CD_PIN_EN) {
		if (host->hw->cd_level)
			host->card_inserted =
				(host->sd_cd_polarity == 0) ? 1 : 0;
		else
			host->card_inserted =
				(host->sd_cd_polarity == 0) ? 0 : 1;
	} else
		host->card_inserted = 1;
	dev_dbg(host->dev, "Card insert<%d> Block bad card<%d>",
			host->card_inserted, host->block_bad_card);
	if (host->hw->host_function == MSDC_SD && host->block_bad_card)
		host->card_inserted = 0;
end:
	inserted = host->card_inserted;
	return inserted;
}

int msdc_ops_switch_volt(struct mmc_host *mmc, struct mmc_ios *ios)
{
	struct msdc_host *host = mmc_priv(mmc);
	int err = 0;
	u32 timeout = 100;
	u32 retry = 10;
	u32 status;
	u32 sclk = host->sclk;

	if ((host->hw->host_function != MSDC_EMMC) && (ios->signal_voltage
			!= MMC_SIGNAL_VOLTAGE_330)) {
		/* make sure SDC is not busy (TBC) */
		/* WAIT_COND(!SDC_IS_BUSY(), timeout, timeout); */
		err = (unsigned int) -EIO;
		msdc_retry(sdc_is_busy(), retry, timeout, host->id);
		if (timeout == 0 && retry == 0) {
			err = (unsigned int) -ETIMEDOUT;
			goto out;
		}

		/* pull up disabled in CMD and DAT[3:0] to allow
		   card drives them to low */

		/* check if CMD/DATA lines both 0 */
		if ((sdr_read32(MSDC_PS) & ((1 << 24) | (0xF << 16))) == 0) {

			/* pull up disabled in CMD and DAT[3:0] */
			msdc_pin_config(host, MSDC_PIN_PULL_NONE);

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

			/* pull up enabled in CMD and DAT[3:0] */
			msdc_pin_config(host, MSDC_PIN_PULL_UP);
			mdelay(5);

			/* start to detect volt change
			   by providing 1.8v signal to card */
			sdr_set_bits(MSDC_CFG, MSDC_CFG_BV18SDT);

			/* wait at max. 1ms */
			mdelay(1);

			while ((status = sdr_read32(MSDC_CFG))
					& MSDC_CFG_BV18SDT)
				cpu_relax();

			if (status & MSDC_CFG_BV18PSS)
				err = 0;
			/* config clock back to init clk freq. */
			msdc_set_mclk(host, 0, sclk);
		}
	}
out:
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

static struct mmc_host_ops mt_msdc_ops = {
	.post_req = msdc_post_req,
	.pre_req = msdc_pre_req,
	.request = msdc_ops_request,
	.set_ios = msdc_ops_set_ios,
	.get_cd = msdc_ops_get_cd,
	.enable_sdio_irq = msdc_ops_enable_sdio_irq,
	.start_signal_voltage_switch = msdc_ops_switch_volt,
};

static irqreturn_t msdc_cd_irq(int irq, void *data)
{
	struct msdc_host *host = (struct msdc_host *)data;
	int got_bad_card = 0;
	unsigned long flags;

	spin_lock_irqsave(&host->remove_bad_card, flags);
	if (host->hw->cd_level ^ host->sd_cd_polarity)
		got_bad_card = host->block_bad_card;
	host->sd_cd_polarity = !host->sd_cd_polarity;
	spin_unlock_irqrestore(&host->remove_bad_card, flags);
	host->block_bad_card = 0;
	irq_set_irq_type(host->cd_irq, host->sd_cd_polarity
			? IRQF_TRIGGER_HIGH : IRQF_TRIGGER_LOW);
	if (got_bad_card == 0)
		tasklet_hi_schedule(&host->card_tasklet);
	dev_err(host->dev, "SD card %s",
			(host->hw->cd_level ^ host->sd_cd_polarity)
			? "insert" : "remove");
	return IRQ_HANDLED;
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
		events = sdr_read32(MSDC_INT);
		event_mask = sdr_read32(MSDC_INTEN);
		sdr_write32(MSDC_INT, events); /* clear interrupts */

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

static void msdc_init_cd_irq(struct msdc_host *host)
{
	struct msdc_hw *hw = host->hw;
	char cd_pin_name[] = "MSDCx_INSI";

	sdr_clr_bits(MSDC_PS, MSDC_PS_CDEN);
	sdr_clr_bits(MSDC_INTEN, MSDC_INTEN_CDSC);
	sdr_clr_bits(SDC_CFG, SDC_CFG_INSWKUP);

	cd_pin_name[4] = host->id + '0';
	/*host->cd_pin = mt_pin_find_by_pin_name(0, cd_pin_name, NULL); */
	if (host->cd_pin < 0)
		return;

	host->cd_irq = gpio_to_irq(host->cd_pin);
	if ((MSDC_SD == hw->host_function) &&
	    (hw->flags & MSDC_CD_PIN_EN)) {
		int ret;
		u32 trig_flag;
		/* mt_pin_set_mode_main(host->cd_pin); */
		/* mt_pin_set_pull(host->cd_pin, MT_PIN_PULL_ENABLE_UP); */
		trig_flag = hw->cd_level ? IRQF_TRIGGER_HIGH : IRQF_TRIGGER_LOW;
		host->sd_cd_polarity = hw->cd_level;

		ret = request_irq(host->cd_irq, msdc_cd_irq, trig_flag,
				"msdc_cd_irq", host);
		dev_err(host->dev, "SD card detection irq: ret=%d\n", ret);
	}
}

/* called by msdc_drv_probe */
static void msdc_init_hw(struct msdc_host *host)
{
	struct msdc_hw *hw = host->hw;
	u32 cur_rxdly0, cur_rxdly1;

	/* Power on */
	msdc_pin_reset(host, MSDC_PIN_PULL_UP);
	clk_prepare(host->src_clk);
	clk_enable(host->src_clk);
	host->core_clkon = 1;

	/* Configure to MMC/SD mode */
	sdr_set_field(MSDC_CFG, MSDC_CFG_MODE, MSDC_SDMMC);

	/* Reset */
	msdc_reset_hw(host->id);

	/* Disable card detection */
	sdr_clr_bits(MSDC_PS, MSDC_PS_CDEN);

	/* Disable and clear all interrupts */
	sdr_clr_bits(MSDC_INTEN, sdr_read32(MSDC_INTEN));
	sdr_write32(MSDC_INT, sdr_read32(MSDC_INT));

	sdr_write32(MSDC_PAD_TUNE, 0x00000000);
	sdr_set_field(MSDC_PAD_TUNE, MSDC_PAD_TUNE_DATWRDLY, hw->datwrddly);
	sdr_set_field(MSDC_PAD_TUNE, MSDC_PAD_TUNE_CMDRRDLY, hw->cmdrrddly);
	sdr_set_field(MSDC_PAD_TUNE, MSDC_PAD_TUNE_CMDRDLY, hw->cmdrddly);
	sdr_write32(MSDC_IOCON, 0x00000000);
	sdr_set_field(MSDC_IOCON, MSDC_IOCON_DDLSEL, 1);
	cur_rxdly0 = ((hw->dat0rddly & 0x1F) << 24)
			| ((hw->dat1rddly & 0x1F) << 16)
			| ((hw->dat2rddly & 0x1F) << 8)
			| ((hw->dat3rddly & 0x1F) << 0);
	cur_rxdly1 = ((hw->dat4rddly & 0x1F) << 24)
			| ((hw->dat5rddly & 0x1F) << 16)
			| ((hw->dat6rddly & 0x1F) << 8)
			| ((hw->dat7rddly & 0x1F) << 0);
	sdr_write32(MSDC_DAT_RDDLY0, cur_rxdly0);
	sdr_write32(MSDC_DAT_RDDLY1, cur_rxdly1);
	sdr_write32(MSDC_PATCH_BIT0, 0x403C004F);
	sdr_set_field(MSDC_PATCH_BIT0, MSDC_CKGEN_MSDC_DLY_SEL,
		host->hw->ckgen_msdc_dly_sel);
	sdr_write32(MSDC_PATCH_BIT1, 0xFFFF0089);
	/* sdr_set_field(MSDC_PATCH_BIT1, MSDC_PATCH_BIT1_WRDAT_CRCS, 1); */
	/* sdr_set_field(MSDC_PATCH_BIT1, MSDC_PATCH_BIT1_CMD_RSP,    1); */
	host->saved_para.pad_tune = sdr_read32(MSDC_PAD_TUNE);
	host->saved_para.ddly0 = sdr_read32(MSDC_DAT_RDDLY0);
	host->saved_para.ddly1 = sdr_read32(MSDC_DAT_RDDLY1);
	sdr_get_field(MSDC_PATCH_BIT1, MSDC_PATCH_BIT1_CMD_RSP,
		host->saved_para.cmd_resp_ta_cntr);
	sdr_get_field(MSDC_PATCH_BIT1, MSDC_PATCH_BIT1_WRDAT_CRCS,
		host->saved_para.wrdat_crc_ta_cntr);

	/* Configure to enable SDIO mode.
	   it's must otherwise sdio cmd5 failed */
	sdr_set_bits(SDC_CFG, SDC_CFG_SDIO);

	/* disable detect SDIO device interupt function */
	sdr_clr_bits(SDC_CFG, SDC_CFG_SDIOIDE);

	/* eneable SMT for glitch filter */
	msdc_set_smt(host, 1);
	/* set clk, cmd, dat pad driving */
	msdc_set_driving(host);

	/* Configure to default data timeout */
	sdr_set_field(SDC_CFG, SDC_CFG_DTOC, DEFAULT_DTOC);
	msdc_set_buswidth(host, MMC_BUS_WIDTH_1);

	dev_dbg(host->dev, "init hardware done!");
}

/* called by msdc_drv_remove */
static void msdc_deinit_hw(struct msdc_host *host)
{

	/* Disable and clear all interrupts */
	sdr_clr_bits(MSDC_INTEN, sdr_read32(MSDC_INTEN));
	sdr_write32(MSDC_INT, sdr_read32(MSDC_INT));

	clk_disable(host->src_clk);
	clk_unprepare(host->src_clk);
	/* Disable card detection */
	if (host->cd_irq)
		free_irq(host->cd_irq, host);
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
	gpd->next = (void *) ((u32) dma->gpd_addr
			+ sizeof(struct mt_gpdma_desc));

	gpd->bdp = 1; /* hwo, cs, bd pointer */
	gpd->ptr = (void *) dma->bd_addr; /* physical address */

	memset(bd, 0, sizeof(struct mt_bdma_desc) * bdlen);
	ptr = bd + bdlen - 1;

	while (ptr != bd) {
		prev = ptr - 1;
		prev->next = (void *) (dma->bd_addr
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

static void msdc_timer_pm(unsigned long data)
{
	struct msdc_host *host = (struct msdc_host *) data;
	unsigned long flags;

	spin_lock_irqsave(&host->clk_gate_lock, flags);
	if (!host->clk_gate_count) {
		msdc_clksrc_onoff(host, 0);
		dev_dbg(host->dev,
			"%s: delay expired, disable clock, clk_gate_count=%d\n",
			__func__, host->clk_gate_count);
	} else
		dev_dbg(host->dev, "%s: delay expired, clk_gate_count=%d\n",
			__func__, host->clk_gate_count);
	spin_unlock_irqrestore(&host->clk_gate_lock, flags);
}

static void msdc_set_host_power_control(struct msdc_host *host)
{
	switch (host->id) {
	case 0:
		if (MSDC_EMMC == host->hw->host_function) {
			host->power_control = msdc_emmc_power;
		} else {
			dev_err(host->dev,
					"Host function definition error.");
			BUG();
		}
		break;
	case 1:
	case 2:
		if (MSDC_SD == host->hw->host_function) {
			host->power_control = msdc_sd_power;
			host->power_switch = msdc_sd_power_switch;
		} else {
			dev_err(host->dev,
					"Host function definition error.");
			BUG();
		}
		break;
	case 3:
		if (MSDC_SDIO == host->hw->host_function)
			host->power_control = msdc_sdio_power;
		else {
			dev_err(host->dev,
					"Host function definition error.");
			BUG();
		}
		break;
	case 4:
		if (MSDC_EMMC == host->hw->host_function) {
			host->power_control = msdc_emmc_power;
		} else {
			dev_err(host->dev,
					"Host function definition error.");
			BUG();
		}
		break;
	default:
		break;
	}
}

static ssize_t msdc_attr_sdio_polling_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct mmc_host *mmc = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n", !(mmc->caps & MMC_CAP_SDIO_IRQ));
}

static ssize_t msdc_attr_sdio_polling_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct mmc_host *mmc = dev_get_drvdata(dev);
	struct msdc_host *host = mmc_priv(mmc);

	if (count)
		msdc_sdio_polling_enable(host, buf[0] != '0');
	return count;
}

static DEVICE_ATTR(sdio_polling, S_IRUGO | S_IWUSR | S_IWGRP,
		   msdc_attr_sdio_polling_show, msdc_attr_sdio_polling_store);

MSDC_DEV_ATTR(dat_drv, "%d", host->pin_ctl->dat_drv, int);
MSDC_DEV_ATTR(cmd_drv, "%d", host->pin_ctl->cmd_drv, int);
MSDC_DEV_ATTR(clk_drv, "%d", host->pin_ctl->clk_drv, int);
MSDC_DEV_ATTR(max_pio, "%d", host->hw->max_pio, int);
MSDC_DEV_ATTR(max_dma_polling, "%d", host->hw->max_dma_polling, int);
MSDC_DEV_ATTR(max_busy_time, "%d", host->hw->max_busy_time, int);
MSDC_DEV_ATTR(max_poll_time, "%d", host->hw->max_poll_time, int);
MSDC_DEV_ATTR(poll_sleep_base, "%d", host->hw->poll_sleep_base, int);

static struct device_attribute *msdc_attrs[] = {
	&dev_attr_dat_drv,
	&dev_attr_cmd_drv,
	&dev_attr_clk_drv,
	&dev_attr_max_pio,
	&dev_attr_max_dma_polling,
	&dev_attr_max_busy_time,
	&dev_attr_max_poll_time,
	&dev_attr_poll_sleep_base,
	NULL,
};

static struct device_attribute *msdc_sdio_attrs[] = {
	&dev_attr_sdio_polling,
	NULL,
};

static void msdc_add_device_attrs(struct msdc_host *host,
		struct device_attribute *attrs[])
{
	int i, ret;

	if (!attrs)
		return;

	for (i = 0; attrs[i]; ++i) {
		ret = device_create_file(host->dev, attrs[i]);
		if (ret)
			dev_err(host->dev,
				"failed to register attribute: %s; err=%d\n",
				attrs[i]->attr.name, ret);
	}
}

struct msdc_hw msdc0_hw = {
	.cmd_edge = MSDC_SMPL_RISING,
	.rdata_edge = MSDC_SMPL_RISING,
	.wdata_edge = MSDC_SMPL_RISING,
	.ckgen_msdc_dly_sel = 1,
	/* time in usec */
	.max_poll_time = 1000,
	.max_busy_time = 10,

	.pin_ctl = {
		{
			.timing = BIT(MMC_TIMING_MMC_HS200)
				| BIT(MMC_TIMING_UHS_DDR50)
				| BIT(MMC_TIMING_UHS_SDR104),
			.clk_drv = 2,
			.cmd_drv = 2,
			.dat_drv = 1,
		},
		{
			.clk_drv = 4,
			.cmd_drv = 4,
			.dat_drv = 4,
		},
	},
	.data_pins = 8,
	.data_offset = 0,
	.flags = MSDC_EMMC_FLAG,
	.dat0rddly = 0,
	.dat1rddly = 0,
	.dat2rddly = 0,
	.dat3rddly = 0,
	.dat4rddly = 0,
	.dat5rddly = 0,
	.dat6rddly = 0,
	.dat7rddly = 0,
	.datwrddly = 0,
	.cmdrrddly = 0,
	.cmdrddly = 0,
	.host_function = MSDC_EMMC,
	.boot = MSDC_BOOT_EN,
	.shutdown_delay_ms = 1600,
};

struct msdc_hw msdc1_hw = {
	.cmd_edge       = MSDC_SMPL_RISING,
	.rdata_edge     = MSDC_SMPL_RISING,
	.wdata_edge     = MSDC_SMPL_RISING,
	.ckgen_msdc_dly_sel = 1,
	/* time in usec */
	.max_poll_time = 1000,
	.max_busy_time = 10,
	.pin_ctl = {
		{
			.timing = BIT(MMC_TIMING_UHS_DDR50)
				| BIT(MMC_TIMING_UHS_SDR104),
			.clk_drv = 3,
			.cmd_drv = 3,
			.dat_drv = 3,
		},
		{
			.clk_drv = 5,
			.cmd_drv = 3,
			.dat_drv = 3,
		},
	},
	.data_pins      = 4,
	.data_offset    = 0,
	.flags		  = MSDC_SYS_SUSPEND | MSDC_WP_PIN_EN | MSDC_HIGHSPEED,
	.dat0rddly		= 0,
	.dat1rddly		= 0,
	.dat2rddly		= 0,
	.dat3rddly		= 0,
	.dat4rddly		= 0,
	.dat5rddly		= 0,
	.dat6rddly		= 0,
	.dat7rddly		= 0,
	.datwrddly		= 0,
	.cmdrrddly		= 0,
	.cmdrddly		= 0,
	.host_function	= MSDC_SD,
	.boot			= 0,
	.cd_level		= MSDC_CD_LOW,
};
static int msdc_drv_probe(struct platform_device *pdev)
{
	struct mmc_host *mmc;
	struct msdc_host *host;
	struct msdc_hw *hw;
	void __iomem *base;
	struct regulator *core_power;
	struct regulator *io_power;
	struct clk *src_clk;
	struct clk *pll_clk = NULL;
	int ret, irq;

	pr_info("of msdc DT probe %s!\n", pdev->dev.of_node->name);
	/* iomap register */
	base = of_iomap(pdev->dev.of_node, 0);
	if (!base) {
		pr_info("can't of_iomap for msdc!!\n");
		return -ENOMEM;
	} else {
		pr_info("of_iomap for msdc @ 0x%p\n", base);
	}

	core_power = devm_regulator_get(&pdev->dev, "core-power");
	io_power = devm_regulator_get(&pdev->dev, "io-power");
	if (!core_power || !io_power) {
		dev_err(&pdev->dev,
			"Cannot get core/io power from the device tree!\n");
		return -EINVAL;
	}

	src_clk = devm_clk_get(&pdev->dev, "src_clk");
	BUG_ON(IS_ERR(src_clk));
	if (!pll_clk) {
		pll_clk = devm_clk_get(&pdev->dev, "pll_clk");
		BUG_ON(IS_ERR(pll_clk));
		ret = clk_set_rate(pll_clk, CLK_RATE_200MHZ);
		if (ret) {
			dev_err(&pdev->dev, "%s: clk_set_rate returned %d\n",
					__func__, ret);
			return ret;
		} else {
			dev_dbg(&pdev->dev, "host clock: %ld\n",
					clk_get_rate(src_clk));
		}
	}

	if (gpio_node == NULL) {
		gpio_node = of_find_compatible_node(NULL, NULL,
				"mediatek,GPIO");
		gpio_base = of_iomap(gpio_node, 0);
		pr_info("of_iomap for gpio base @ 0x%p\n", gpio_base);
	}
	if (gpio1_node == NULL) {
		gpio1_node = of_find_compatible_node(NULL, NULL,
				"mediatek,GPIO1");
		gpio1_base = of_iomap(gpio1_node, 0);
		pr_info("of_iomap for gpio1 base @ 0x%p\n", gpio1_base);
	}

	/* get irq #  */
	irq = irq_of_parse_and_map(pdev->dev.of_node, 0);
	pr_info("msdc get irq # %d\n", irq);
	BUG_ON(irq < 0);

	if (strcmp(pdev->dev.of_node->name, "mmc0") == 0) {
		hw = &msdc0_hw;
		pdev->id = 0;
		pr_info("platform_data hw:0x%p, is msdc0_hw\n", hw);
	}
	if (strcmp(pdev->dev.of_node->name, "mmc1") == 0) {
		hw = &msdc1_hw;
		pdev->id = 1;
		pr_info("platform_data hw:0x%p, is msdc1_hw\n", hw);
	}
#if defined(CFG_DEV_MSDC2)
	if (strcmp(pdev->dev.of_node->name, "mmc2") == 0) {
		hw = &msdc2_hw;
		pdev->id = 2;
		pr_info("platform_data hw:0x%p, is msdc2_hw\n", hw);
	}
#endif
#if defined(CFG_DEV_MSDC3)
	if (strcmp(pdev->dev.of_node->name, "MSDC3") == 0) {
		hw = &msdc3_hw;
		pdev->id = 3;
		pr_info("platform_data hw:0x%p, is msdc3_hw\n", hw);
	}
#endif

	if (hw == NULL)
		return -ENODEV;

	/* Allocate MMC host for this device */
	mmc = mmc_alloc_host(sizeof(struct msdc_host), &pdev->dev);
	if (!mmc)
		return -ENOMEM;

	BUG_ON((!hw) || (irq < 0));

	/* Set host parameters to mmc */
	mmc->ops = &mt_msdc_ops;
	mmc->f_min = HOST_MIN_MCLK;
	mmc->f_max = HOST_MAX_MCLK;
	mmc->ocr_avail = MSDC_OCR_AVAIL;

	mmc->caps = 0;

	/* For sd card: MSDC_SYS_SUSPEND | MSDC_WP_PIN_EN
	 * | MSDC_CD_PIN_EN | MSDC_REMOVABLE | MSDC_HIGHSPEED,
	 * For sdio   : MSDC_EXT_SDIO_IRQ | MSDC_HIGHSPEED */
	if (hw->flags & MSDC_HIGHSPEED)
		mmc->caps |= MMC_CAP_MMC_HIGHSPEED | MMC_CAP_SD_HIGHSPEED;
	if (hw->data_pins == 4)
		mmc->caps |= MMC_CAP_4_BIT_DATA;
	else if (hw->data_pins == 8)
		mmc->caps |= MMC_CAP_8_BIT_DATA | MMC_CAP_4_BIT_DATA;
	if ((hw->flags & MSDC_SDIO_IRQ) || (hw->flags & MSDC_EXT_SDIO_IRQ))
		mmc->caps |= MMC_CAP_SDIO_IRQ; /* yes for sdio */
	if (hw->flags & MSDC_UHS1) {
		mmc->caps |= MMC_CAP_UHS_SDR12 | MMC_CAP_UHS_SDR25
			| MMC_CAP_UHS_SDR50
			| MMC_CAP_UHS_SDR104;
		mmc->caps2 |= MMC_CAP2_HS200_1_8V_SDR;
	}
	if (hw->flags & MSDC_DDR)
		mmc->caps |= MMC_CAP_UHS_DDR50 | MMC_CAP_1_8V_DDR;
	if (!(hw->flags & MSDC_REMOVABLE))
		mmc->caps |= MMC_CAP_NONREMOVABLE;
	mmc->caps |= MMC_CAP_ERASE;
	/* MMC core transfer sizes tunable parameters */
	mmc->max_segs = MAX_HW_SGMTS;
	mmc->max_seg_size = MAX_SGMT_SZ;
	mmc->max_blk_size = HOST_MAX_BLKSZ;
	mmc->max_req_size = MAX_REQ_SZ;
	mmc->max_blk_count = mmc->max_req_size;

	host = mmc_priv(mmc);
	host->dev = &pdev->dev;
	host->core_power = core_power;
	host->io_power = io_power;
	host->hw = hw;
	host->mmc = mmc;
	host->id = pdev->id;
	host->error = 0;
	host->irq = irq;
	host->base = base;
	host->src_clk = src_clk;
	host->mclk = 0; /* mclk: the request clock of mmc sub-system */
	/* hclk: clock of clock source to msdc controller */
	host->hclk = clk_get_rate(host->src_clk);
	host->sclk = 0; /* sclk: the really clock after divition */
	host->suspend = 0;
	host->core_clkon = 0;
	host->card_clkon = 0;
	host->clk_gate_count = 0;
	host->power_mode = MMC_POWER_OFF;
	host->power_control = NULL;
	host->power_switch = NULL;
	if (mmc->caps & MMC_CAP_NONREMOVABLE)
		host->card_inserted = 1;
#ifdef SDIO_ERROR_BYPASS
	host->sdio_error = 0;
#endif
	msdc_set_host_power_control(host);
	if ((host->hw->host_function == MSDC_SD) && (host->hw->flags
			& MSDC_CD_PIN_EN)) {
		/* work around : hot-plug project SD card LDO
		 * always on if no SD card insert */
		msdc_sd_power(host, 1);
		msdc_sd_power(host, 0);
	}
	host->timeout_ns = 0;
	host->timeout_clks = DEFAULT_DTOC * 1048576;
	if (host->hw->host_function == MSDC_EMMC) {
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
			= dma_alloc_coherent(NULL,
				MAX_GPD_NUM * sizeof(struct mt_gpdma_desc),
				&host->dma.gpd_addr, GFP_KERNEL);
	host->dma.bd
			= dma_alloc_coherent(NULL,
				MAX_BD_NUM * sizeof(struct mt_bdma_desc),
				&host->dma.bd_addr, GFP_KERNEL);
	BUG_ON((!host->dma.gpd) || (!host->dma.bd));
	msdc_init_gpd_bd(host, &host->dma);
	host->host_mode = mmc->caps;
	/*for emmc */
	host->read_time_tune = 0;
	host->write_time_tune = 0;
	host->rwcmd_time_tune = 0;
	host->power_cycle = 0;
	host->power_cycle_enable = 1;
	host->sw_timeout = 0;
	host->tune = 0;
	host->ddr = 0;
	host->sd_cd_insert_work = 0;
	host->block_bad_card = 0;
	host->saved_para.suspend_flag = 0;
	host->latest_operation_type = OPER_TYPE_NUM;
	tasklet_init(&host->card_tasklet, msdc_tasklet_card, (ulong) host);
	INIT_DELAYED_WORK(&host->req_timeout, msdc_request_timeout);
	spin_lock_init(&host->lock);
	spin_lock_init(&host->clk_gate_lock);
	spin_lock_init(&host->remove_bad_card);

	init_timer(&host->timer);
	host->timer.function = msdc_timer_pm;
	host->timer.data = (unsigned long) host;

	msdc_init_hw(host);

	ret = request_threaded_irq((unsigned int) irq, msdc_irq, NULL,
		IRQF_TRIGGER_LOW | IRQF_ONESHOT, DRV_NAME, host);
	if (ret)
		goto release;

	if (hw->flags & MSDC_CD_PIN_EN) { /* not set for sdio */
		if (hw->request_cd_eirq) {
			/* msdc_eirq_cd will not be used! */
			hw->request_cd_eirq(msdc_eirq_cd, (void *) host);
		}
	}

	if (is_card_sdio(host)) {
		int irq = gpio_to_irq(hw->sdio.irq.pin);
		/* mt_pin_set_pull(hw->sdio.irq.pin, MT_PIN_PULL_ENABLE_UP); */
		/* mt_pin_set_mode_gpio(hw->sdio.irq.pin); */
		/* gpio_direction_input(hw->sdio.irq.pin); */
		irq_set_status_flags(irq, IRQ_NOAUTOEN);
		if (request_threaded_irq(irq, msdc_eirq_sdio, NULL,
			hw->sdio.irq.flags, dev_name(host->dev), host))
			dev_err(host->dev, "%s: failed to request irq: err=%d\n",
				__func__, ret);

		hw->sdio.power_enable = msdc_sdio_power_enable;
		hw->sdio.power_data = host;
		if (hw->flags & MSDC_SYS_SUSPEND) /* will not set for WIFI */
			dev_err(host->dev,
				"MSDC_SYS_SUSPEND, IRQ pin are both set\n");
		/* mmc->pm_flags |= MMC_PM_IGNORE_PM_NOTIFY; */
		/* pm not controlled by system but by client. */
	}

	platform_set_drvdata(pdev, mmc);

	/* Config card detection pin and enable interrupts */
	if (hw->flags & MSDC_CD_PIN_EN) /* set for card */
		msdc_init_cd_irq(host);

	msdc_add_device_attrs(host, msdc_attrs);
	if (is_card_sdio(host))
		msdc_add_device_attrs(host, msdc_sdio_attrs);

	ret = mmc_add_host(mmc);
	if (ret)
		goto free_irq;
	if (hw->flags & MSDC_CD_PIN_EN)
		host->sd_cd_insert_work = 1;


	return 0;

free_irq:
	free_irq(irq, host);
release:
	platform_set_drvdata(pdev, NULL);
	msdc_deinit_hw(host);
	tasklet_kill(&host->card_tasklet);
	mmc_free_host(mmc);

	return ret;
}

/* 4 device share one driver, using "drvdata" to show difference */
static int msdc_drv_remove(struct platform_device *pdev)
{
	struct mmc_host *mmc;
	struct msdc_host *host;

	mmc = platform_get_drvdata(pdev);
	BUG_ON(!mmc);

	host = mmc_priv(mmc);
	BUG_ON(!host);

	dev_err(host->dev, "removed !!!");

	platform_set_drvdata(pdev, NULL);
	mmc_remove_host(host->mmc);
	msdc_deinit_hw(host);

	tasklet_kill(&host->card_tasklet);
	free_irq(host->irq, host);

	dma_free_coherent(NULL, MAX_GPD_NUM * sizeof(struct mt_gpdma_desc),
			host->dma.gpd, host->dma.gpd_addr);
	dma_free_coherent(NULL, MAX_BD_NUM * sizeof(struct mt_bdma_desc),
			host->dma.bd, host->dma.bd_addr);

	mmc_free_host(host->mmc);

	return 0;
}

static int msdc_sdio_suspend(struct msdc_host *host)
{
	int ret = 0;

	dev_err(host->dev, "%s: enter\n", __func__);
	msdc_sdio_irq_enable(host, false);
	mmc_claim_host(host->mmc);
	host->saved_para.hz = host->mclk;
	if (host->saved_para.hz) {
		msdc_ungate_clock(host);
		sdr_get_field(MSDC_CFG, MSDC_CFG_CKMOD, host->saved_para.mode);
		sdr_get_field(MSDC_CFG, MSDC_CFG_CKDIV, host->saved_para.div);
		sdr_get_field(MSDC_PATCH_BIT0, MSDC_CKGEN_MSDC_DLY_SEL,
			host->saved_para.ckgen_msdc_dly_sel);
		host->saved_para.msdc_cfg = sdr_read32(MSDC_CFG);
		host->saved_para.ddly0 = sdr_read32(MSDC_DAT_RDDLY0);
		host->saved_para.pad_tune = sdr_read32(MSDC_PAD_TUNE);
		host->saved_para.sdc_cfg = sdr_read32(SDC_CFG);
		host->saved_para.iocon = sdr_read32(MSDC_IOCON);
		host->saved_para.ddr = host->ddr;
		ret = msdc_gate_clock(host, 0);
		if (ret) {
			dev_err(host->dev, "%s: err=%d\n", __func__, ret);
			mmc_release_host(host->mmc);
			msdc_sdio_irq_enable(host, true);
		} else
			host->saved_para.suspend_flag = 1;
	}
	dev_err(host->dev, "%s: done; err=%d\n", __func__, ret);
	return ret;
}

static void msdc_sdio_resume(struct msdc_host *host)
{
	msdc_ungate_clock(host);
	mb(); /* synchronize memory view with other CPUs */
	if (host->saved_para.hz) {
		if (host->saved_para.suspend_flag) {
			dev_err(host->dev,
				"%s: resume cur_cfg=%08X, save_cfg=%08X",
				 __func__, sdr_read32(MSDC_CFG),
				 host->saved_para.msdc_cfg);
			dev_err(host->dev, "cur_hz=%d, save_hz=%d\n",
					host->mclk, host->saved_para.hz);
			host->saved_para.suspend_flag = 0;
			msdc_restore_info(host);
		} else if (host->saved_para.msdc_cfg &&
			sdr_read32(MSDC_CFG) != host->saved_para.msdc_cfg) {
			/* Notice it turns off clock,
			   it possible needs to restore the register */
			dev_err(host->dev,
				"%s: update cur_cfg=%08X, save_cfg=%08X",
				 __func__, sdr_read32(MSDC_CFG),
				 host->saved_para.msdc_cfg);
			dev_err(host->dev, "cur_hz=%d, save_hz=%d\n",
					host->mclk, host->saved_para.hz);
			msdc_restore_info(host);
		}
	}
	msdc_gate_clock(host, 1);
	mmc_release_host(host->mmc);
	msdc_sdio_irq_enable(host, true);
}

static int msdc_drv_suspend(struct device *dev)
{
	int ret = 0;
	struct mmc_host *mmc = dev_get_drvdata(dev);
	struct msdc_host *host = mmc_priv(mmc);

	if (host->hw->flags & MSDC_SYS_SUSPEND) {
		/* MMC/SD is using system suspend */
		int ret_clk;

		msdc_ungate_clock(host);
		/* ret = mmc_suspend_host(host->mmc); */
		ret_clk = msdc_gate_clock(host, 0);
		if (!ret)
			ret = ret_clk;
		if (ret_clk)
			dev_err(host->dev, "%s: err=%d\n", __func__, ret);
	} else {
		/* SDIO suspend */
		if (!is_card_sdio(host))
			return 0;
		ret = msdc_sdio_suspend(host);
	}
	return ret;
}

static int msdc_drv_resume(struct device *dev)
{
	int ret = 0;
	struct mmc_host *mmc = dev_get_drvdata(dev);
	struct msdc_host *host = mmc_priv(mmc);

	msdc_ungate_clock(host);
	if (host->hw->flags & MSDC_SYS_SUSPEND)
		/* ret = mmc_resume_host(host->mmc); */
		ret = 0;
	else if (is_card_sdio(host))
		msdc_sdio_resume(host);
	msdc_gate_clock(host, 1);
	return ret;
}

static void msdc_drv_shutdown(struct platform_device *pdev)
{
	struct mmc_host *mmc = platform_get_drvdata(pdev);
	struct msdc_host *host = mmc_priv(mmc);
	int delay;

	dev_err(host->dev, "%s: enter\n", __func__);

	mmc_claim_host(host->mmc);
	if (is_card_sdio(host))
		msdc_sdio_power_enable(host, false);

	host->shutdown = true;

	if (!host->hw->shutdown_delay_ms || !mmc->card)
		goto done;

	mmc_card_set_removed(mmc->card);
	delay = host->hw->shutdown_delay_ms -
		jiffies_to_msecs(jiffies - host->request_ts);
	dev_err(host->dev, "%s: delay=%d ms\n", __func__, delay);
	if (delay > 0)
		/* add extra 20ms because of uncertainty of msleep API */
		msleep(delay+20);
done:
	mmc_release_host(host->mmc);
	dev_err(host->dev, "%s: done\n", __func__);
}

static const struct of_device_id msdc_of_ids[] = {
	{   .compatible = "mediatek,mmc", },
};

static struct platform_driver mt_msdc_driver = {
	.probe = msdc_drv_probe,
	.remove = msdc_drv_remove,
	.shutdown = msdc_drv_shutdown,
	.driver = {
		.name = DRV_NAME,
		.owner = THIS_MODULE,
		.of_match_table = msdc_of_ids,
		.pm = &(const struct dev_pm_ops){
			.suspend = msdc_drv_suspend,
			.resume = msdc_drv_resume,
		},
	},
};

static int __init mt_msdc_init(void)
{
	int ret;

	/* clkmgr board hardware reset default register value is all 0,
	 * means all cg clk is default ON!
	 * So disable all msdc clk in first entry of probe(),
	 * otherwise, there would be some error
	 * occur during suspend procedure. modifier: jie.wu
	 */
	/*
	pr_err("%s: disable all msdc clocks\n", __func__);
	if (clock_is_on(PERI_MSDC0_PDN))
		disable_clock(PERI_MSDC0_PDN, "SD");
	if (clock_is_on(PERI_MSDC1_PDN))
		disable_clock(PERI_MSDC1_PDN, "SD");
	if (clock_is_on(PERI_MSDC2_PDN))
		disable_clock(PERI_MSDC2_PDN, "SD");
	if (clock_is_on(PERI_MSDC3_PDN))
		disable_clock(PERI_MSDC3_PDN, "SD");
	if (clock_is_on(PERI_MSDC4_PDN))
		disable_clock(PERI_MSDC4_PDN, "SD");
	*/
	ret = platform_driver_register(&mt_msdc_driver);
	if (ret) {
		pr_err(DRV_NAME ": Can't register driver");
		return ret;
	}

	pr_info(DRV_NAME ": MediaTek MSDC Driver\n");

	return 0;
}

static void __exit mt_msdc_exit(void)
{
	platform_driver_unregister(&mt_msdc_driver);
}

#define CREATE_TRACE_POINTS
#include "mtk-sd-trace.h"

module_init(mt_msdc_init);
module_exit(mt_msdc_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("MediaTek SD/MMC Card Driver");
