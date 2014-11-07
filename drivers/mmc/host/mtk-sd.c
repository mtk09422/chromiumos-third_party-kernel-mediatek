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

#include "mtk-sd.h"

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

/* interrupt control primitives */
static inline void msdc_cmd_ints_enable(struct msdc_host *host)
{
	u32 wints = MSDC_CMD_INTS;

	sdr_set_bits(MSDC_INTEN, wints);
}

static inline void msdc_cmd_ints_disable(struct msdc_host *host)
{
	u32 wints = MSDC_CMD_INTS;

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

static inline void msdc_data_ints_enable(struct msdc_host *host)
{
	u32 wints = msdc_data_ints_mask(host);

	sdr_set_bits(MSDC_INTEN, wints);
}
static inline void msdc_data_ints_disable(struct msdc_host *host)
{
	u32 wints = msdc_data_ints_mask(host);

	sdr_clr_bits(MSDC_INTEN, wints);
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

static void msdc_dma_start(struct msdc_host *host)
{
	msdc_data_ints_enable(host);
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

static inline void msdc_dma_setup(struct msdc_host *host, struct msdc_dma *dma,
		struct mmc_data *data)
{
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
		sdr_get_field(MSDC_CFG, MSDC_CFG_CKMOD, mode);
		/*DDR mode will double the clk cycles for data timeout */
		timeout = mode >= 2 ? timeout * 2 : timeout;
		timeout = timeout > 1 ? timeout - 1 : 0;
		timeout = timeout > 255 ? 255 : timeout;
	}
	sdr_set_field(SDC_CFG, SDC_CFG_DTOC, timeout);
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
		msdc_reset_hw();
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

	sdr_set_field(MSDC_CFG, MSDC_CFG_CKMOD | MSDC_CFG_CKDIV,
			(mode << 8) | (div % 0xff));
	while (!(sdr_read32(MSDC_CFG) & MSDC_CFG_CKSTB))
		cpu_relax();

	host->sclk = sclk;
	host->mclk = hz;
	host->ddr = ddr;
	/* need because clk changed. */
	msdc_set_timeout(host, host->timeout_ns, host->timeout_clks);
	msdc_irq_restore(flags);

	dev_err(host->dev, "sclk: %d, ddr: %d\n", host->sclk, ddr);
}

static void msdc_clksrc_onoff(struct msdc_host *host, u32 on)
{
	u32 div, mode;

	if (on) {
		clk_enable(host->src_clk);
		udelay(10);
		/* enable SD/MMC/SDIO bus clock:
		 * it will be automatically gated when the bus is idle
		 * (set MSDC_CFG_CKPDN bit to have it always on) */
		sdr_set_field(MSDC_CFG, MSDC_CFG_MODE, MSDC_SDMMC);

		sdr_get_field(MSDC_CFG, MSDC_CFG_CKMOD, mode);
		sdr_get_field(MSDC_CFG, MSDC_CFG_CKDIV, div);
		while (!(sdr_read32(MSDC_CFG) & MSDC_CFG_CKSTB))
			cpu_relax();

	} else {
		/* disable SD/MMC/SDIO bus clock */
		sdr_set_field(MSDC_CFG, MSDC_CFG_MODE, MSDC_MS);
		/* turn off SDHC functional clock */
		clk_disable(host->src_clk);
	}
}

static int msdc_gate_clock(struct msdc_host *host)
{
	int ret = 0;
	unsigned long flags;

	spin_lock_irqsave(&host->clk_gate_lock, flags);
	if (host->clk_gate_count > 0)
		host->clk_gate_count--;

	if (host->clk_gate_count == 0) {
		msdc_clksrc_onoff(host, 0);
		dev_dbg(host->dev,
			"%s: clock gated: clk_gate_count=%d\n",
			__func__, host->clk_gate_count);
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
	dev_err(host->dev, "Set power mode(%d)", mode);
	if (host->power_mode == MMC_POWER_OFF && mode != MMC_POWER_OFF) {
		if (host->power_control)
			host->power_control(host, 1);
		else
			dev_err(host->dev,
					"No power control callback.\n");

		mdelay(10);
	} else if (host->power_mode != MMC_POWER_OFF && mode == MMC_POWER_OFF) {

		if (host->power_control)
			host->power_control(host, 0);
		else
			dev_err(host->dev,
					"No power control callback.\n");

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

		msdc_dma_on();

		if (((host->timeout_ns != data->timeout_ns) ||
		    (host->timeout_clks != data->timeout_clks)))
			msdc_set_timeout(host, data->timeout_ns,
					data->timeout_clks);

		sdr_write32(SDC_BLK_NUM, data->blocks);
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

	rsp[0] = sdr_read32(SDC_ACMD_RESP);

	if (events & MSDC_INT_ACMDRDY)
		cmd->error = 0;
	else {
		msdc_reset_hw();
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
	bool active;

	spin_lock_irqsave(&host->lock, flags);
	cancel_delayed_work(&host->req_timeout);
	active = host->mrq != NULL;
	host->mrq = NULL;
	spin_unlock_irqrestore(&host->lock, flags);

	if (active) {
		host->mrq = 0;
		msdc_track_cmd_data(host, mrq->cmd, mrq->data);
		msdc_unprepare_data(host, mrq);
	}
	mmc_request_done(host->mmc, mrq);
}

/* returns true if command is fully handled; returns false otherwise */
static bool msdc_cmd_done(struct msdc_host *host, int events,
			  struct mmc_request *mrq, struct mmc_command *cmd)
{
	bool done = false;
	bool sbc_error;
	bool do_next;

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

		msdc_cmd_ints_disable(host);

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
			msdc_reset_hw();
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

		if (do_next)
			msdc_cmd_next(host, mrq, cmd);
		done = true;
	}
	return done;
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

	if (msdc_txfifocnt() || msdc_rxfifocnt()) {
		dev_err(host->dev, "TX/RX FIFO non-empty before start of IO. Reset\n");
		msdc_reset_hw();
	}

	cmd->error = 0;
	rawcmd = msdc_cmd_prepare_raw_cmd(host, mrq, cmd);
	msdc_set_cmd_timeout(host, cmd);

	dev_dbg(host->dev, "%s: cmd=%d arg=%08X cmd_reg=%08X; has_busy=%d\n",
			__func__, cmd->opcode, cmd->arg, rawcmd,
			host->cmd_rsp == RESP_R1B);

	msdc_cmd_ints_enable(host);
	sdc_send_cmd(rawcmd, cmd->arg);

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
	 * use HW option,  otherwise use SW option */
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
	bool do_next;

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
		msdc_dma_stop(host);

		if ((events & MSDC_INT_XFER_COMPL) && (!stop || !stop->error))
			data->bytes_xfered = data->blocks * data->blksz;
		else {
			msdc_reset_hw();
			msdc_clr_fifo();
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

		if (do_next)
			msdc_data_xfer_next(host, mrq, data);
		done = true;
	}
	return done;
}

static void msdc_set_buswidth(struct msdc_host *host, u32 width)
{
	u32 val = sdr_read32(SDC_CFG);

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

	sdr_write32(SDC_CFG, val);
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

		while (sdc_is_busy())
			cpu_relax();

		/* check if CMD/DATA lines both 0 */
		if ((sdr_read32(MSDC_PS) & ((1 << 24) | (0xF << 16))) == 0) {

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

static void msdc_init_hw(struct msdc_host *host)
{
	/* Configure to MMC/SD mode */
	sdr_set_field(MSDC_CFG, MSDC_CFG_MODE, MSDC_SDMMC);

	/* Reset */
	msdc_reset_hw();

	/* Disable card detection */
	sdr_clr_bits(MSDC_PS, MSDC_PS_CDEN);

	/* Disable and clear all interrupts */
	sdr_clr_bits(MSDC_INTEN, sdr_read32(MSDC_INTEN));
	sdr_write32(MSDC_INT, sdr_read32(MSDC_INT));

	sdr_write32(MSDC_PAD_TUNE, 0x00000000);
	sdr_write32(MSDC_IOCON, 0x00000000);
	sdr_set_field(MSDC_IOCON, MSDC_IOCON_DDLSEL, 1);
	sdr_write32(MSDC_PATCH_BIT0, 0x403C004F);
	sdr_set_field(MSDC_PATCH_BIT0, MSDC_CKGEN_MSDC_DLY_SEL, 1);
	sdr_write32(MSDC_PATCH_BIT1, 0xFFFF0089);
	/* Configure to enable SDIO mode.
	   it's must otherwise sdio cmd5 failed */
	sdr_set_bits(SDC_CFG, SDC_CFG_SDIO);

	/* disable detect SDIO device interupt function */
	sdr_clr_bits(SDC_CFG, SDC_CFG_SDIOIDE);

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

static void msdc_set_host_power_control(struct msdc_host *host)
{
	if (MSDC_EMMC == host->host_func) {
		host->power_control = msdc_emmc_power;
	} else if (MSDC_SD == host->host_func) {
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
	void __iomem *base;
	struct regulator *core_power;
	struct regulator *io_power;
	struct clk *src_clk;
	struct clk *pll_clk = NULL;
	int irq;
	const char *function;
	int ret = mmc_of_parse(mmc);

	if (ret)
		return ret;
	/* iomap register */
	base = of_iomap(pdev->dev.of_node, 0);
	if (!base) {
		dev_info(&pdev->dev, "can't of_iomap for msdc!!\n");
		return -ENOMEM;
	} else {
		dev_info(&pdev->dev, "of_iomap for msdc @ 0x%p\n", base);
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
	dev_info(&pdev->dev, "msdc get irq # %d\n", irq);
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

	host->error = 0;
	host->mclk = 0; /* mclk: the request clock of mmc sub-system */
	/* hclk: clock of clock source to msdc controller */
	host->hclk = clk_get_rate(host->src_clk);
	host->sclk = 0; /* sclk: the really clock after divition */
	host->suspend = 0;
	host->clk_gate_count = 0;
	host->power_mode = MMC_POWER_OFF;
	host->power_control = NULL;
	host->power_switch = NULL;
	msdc_set_host_power_control(host);
	host->timeout_ns = 0;
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
			= dma_alloc_coherent(NULL,
				MAX_GPD_NUM * sizeof(struct mt_gpdma_desc),
				&host->dma.gpd_addr, GFP_KERNEL);
	host->dma.bd
			= dma_alloc_coherent(NULL,
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
	spin_lock_init(&host->clk_gate_lock);
	spin_lock_init(&host->remove_bad_card);

	platform_set_drvdata(pdev, mmc);
	clk_prepare(host->src_clk);

	pm_runtime_enable(host->dev);
	pm_runtime_get_sync(host->dev);
	pm_runtime_set_autosuspend_delay(host->dev, MTK_MMC_AUTOSUSPEND_DELAY);
	pm_runtime_use_autosuspend(host->dev);

	ret = request_threaded_irq((unsigned int) host->irq, msdc_irq, NULL,
		IRQF_TRIGGER_LOW | IRQF_ONESHOT, DRV_NAME, host);
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

/* 4 device share one driver, using "drvdata" to show difference */
static int msdc_drv_remove(struct platform_device *pdev)
{
	struct mmc_host *mmc;
	struct msdc_host *host;

	mmc = platform_get_drvdata(pdev);
	if (!mmc) {
		dev_info(&pdev->dev, "Already free!\n");
		return 0;
	}

	host = mmc_priv(mmc);
	if (!host) {
		dev_info(&pdev->dev, "Already free!\n");
		return 0;
	}

	pm_runtime_get_sync(host->dev);
	dev_err(host->dev, "removed !!!");

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

static int __init mt_msdc_init(void)
{
	int ret;

	/* clkmgr board hardware reset default register value is all 0,
	 * means all cg clk is default ON!
	 * So disable all msdc clk in first entry of probe(),
	 * otherwise, there would be some error
	 * occur during suspend procedure. modifier: jie.wu
	 */

	ret = platform_driver_register(&mt_msdc_driver);
	if (ret) {
		pr_err(DRV_NAME ": Can't register driver");
		return ret;
	}

	return 0;
}

static void __exit mt_msdc_exit(void)
{
	platform_driver_unregister(&mt_msdc_driver);
}

module_init(mt_msdc_init);
module_exit(mt_msdc_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("MediaTek SD/MMC Card Driver");
