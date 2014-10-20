/*
* Copyright (c) 2014 MediaTek Inc.
* Author: tonny.zhang <tonny.zhang@mediatek.com>
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


#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/ioport.h>
#include <linux/errno.h>
#include <linux/spi/spi.h>
#include <linux/workqueue.h>
#include <linux/dma-mapping.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/irqreturn.h>
#include <linux/types.h>
#include <linux/delay.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/spinlock.h>
#include <linux/sched.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include "spi-mt65xx-master.h"

#define SPI_CFG0_REG                      (0x0000)
#define SPI_CFG1_REG                      (0x0004)
#define SPI_TX_SRC_REG                    (0x0008)
#define SPI_RX_DST_REG                    (0x000c)
#define SPI_TX_DATA_REG                   (0x0010)
#define SPI_RX_DATA_REG                   (0x0014)
#define SPI_CMD_REG                       (0x0018)
#define SPI_STATUS0_REG                   (0x001c)
#define SPI_STATUS1_REG                   (0x0020)

#define SPI_CFG0_SCK_HIGH_OFFSET          0
#define SPI_CFG0_SCK_LOW_OFFSET           8
#define SPI_CFG0_CS_HOLD_OFFSET           16
#define SPI_CFG0_CS_SETUP_OFFSET          24

#define SPI_CFG0_SCK_HIGH_MASK            0xff
#define SPI_CFG0_SCK_LOW_MASK             0xff00
#define SPI_CFG0_CS_HOLD_MASK             0xff0000
#define SPI_CFG0_CS_SETUP_MASK            0xff000000

#define SPI_CFG1_CS_IDLE_OFFSET           0
#define SPI_CFG1_PACKET_LOOP_OFFSET       8
#define SPI_CFG1_PACKET_LENGTH_OFFSET     16
#define SPI_CFG1_GET_TICK_DLY_OFFSET      30

#define SPI_CFG1_CS_IDLE_MASK             0xff
#define SPI_CFG1_PACKET_LOOP_MASK         0xff00
#define SPI_CFG1_PACKET_LENGTH_MASK       0x3ff0000
#define SPI_CFG1_GET_TICK_DLY_MASK        0xc0000000

#define SPI_CMD_ACT_OFFSET                0
#define SPI_CMD_RESUME_OFFSET             1
#define SPI_CMD_RST_OFFSET                2
#define SPI_CMD_PAUSE_EN_OFFSET           4
#define SPI_CMD_DEASSERT_OFFSET           5
#define SPI_CMD_CPHA_OFFSET               8
#define SPI_CMD_CPOL_OFFSET               9
#define SPI_CMD_RX_DMA_OFFSET             10
#define SPI_CMD_TX_DMA_OFFSET             11
#define SPI_CMD_TXMSBF_OFFSET             12
#define SPI_CMD_RXMSBF_OFFSET             13
#define SPI_CMD_RX_ENDIAN_OFFSET          14
#define SPI_CMD_TX_ENDIAN_OFFSET          15
#define SPI_CMD_FINISH_IE_OFFSET          16
#define SPI_CMD_PAUSE_IE_OFFSET           17

#define SPI_CMD_ACT_MASK                  0x1
#define SPI_CMD_RESUME_MASK               0x2
#define SPI_CMD_RST_MASK                  0x4
#define SPI_CMD_PAUSE_EN_MASK             0x10
#define SPI_CMD_DEASSERT_MASK             0x20
#define SPI_CMD_CPHA_MASK                 0x100
#define SPI_CMD_CPOL_MASK                 0x200
#define SPI_CMD_RX_DMA_MASK               0x400
#define SPI_CMD_TX_DMA_MASK               0x800
#define SPI_CMD_TXMSBF_MASK               0x1000
#define SPI_CMD_RXMSBF_MASK               0x2000
#define SPI_CMD_RX_ENDIAN_MASK            0x4000
#define SPI_CMD_TX_ENDIAN_MASK            0x8000
#define SPI_CMD_FINISH_IE_MASK            0x10000
#define SPI_CMD_PAUSE_IE_MASK             0x20000

/* Register access macros */
#define spi_readl(port, offset) \
	__raw_readl((port)->regs+(offset))

#define spi_writel(port, offset, value) \
	__raw_writel((value), (port)->regs+(offset))

/*open base log out*/
/* #define SPI_DEBUG */

/* #define SPI_VERBOSE */

#define IDLE 0
#define INPROGRESS 1
#define PAUSED 2

#define PACKET_SIZE 0x400
#define SPI_FIFO_SIZE 32
#define SPI_4B_ALIGN 0x4
#define INVALID_DMA_ADDRESS 0xffffffff

struct mt_spi_t {
	struct platform_device *pdev;
	void __iomem *regs;
	int irq;
	int running;
	struct clk *clk;
	struct mt_chip_conf *config;
	struct spi_master *master;
	struct spi_transfer *cur_transfer;
	struct spi_transfer *next_transfer;
	spinlock_t lock;
	struct list_head	queue;

};

#ifdef SPI_VERBOSE

static void spi_dump_reg(struct mt_spi_t *ms)
{
	dev_dbg(&ms->pdev->dev, "||*****************************************||\n");
	dev_dbg(&ms->pdev->dev, "cfg0:0x%.8x\n", spi_readl(ms, SPI_CFG0_REG));
	dev_dbg(&ms->pdev->dev, "cfg1:0x%.8x\n", spi_readl(ms, SPI_CFG1_REG));
	dev_dbg(&ms->pdev->dev, "cmd:0x%.8x\n", spi_readl(ms, SPI_CMD_REG));
	dev_dbg(&ms->pdev->dev, "tx_s:0x%.8x\n", spi_readl(ms, SPI_TX_SRC_REG));
	dev_dbg(&ms->pdev->dev, "rx_d:0x%.8x\n", spi_readl(ms, SPI_RX_DST_REG));
	dev_dbg(&ms->pdev->dev, "st1:0x%.8x\n", spi_readl(ms, SPI_STATUS1_REG));
	dev_dbg(&ms->pdev->dev, "||*****************************************||\n");
}

#else
static void spi_dump_reg(struct mt_spi_t *ms) {}
#endif

static inline int is_pause_mode(struct spi_message *msg)
{
	struct mt_chip_conf *conf;
	conf = (struct mt_chip_conf *)msg->state;
	return conf->pause;
}

static inline int is_fifo_read(struct spi_message *msg)
{
	struct mt_chip_conf *conf;
	conf = (struct mt_chip_conf *)msg->state;
	return (conf->com_mod == FIFO_TRANSFER) || (conf->com_mod == OTHER1);
}

static inline int is_interrupt_enable(struct mt_spi_t *ms)
{
	u32 cmd;
	cmd = spi_readl(ms, SPI_CMD_REG);
	return (cmd >> SPI_CMD_FINISH_IE_OFFSET) & 1;
}

static inline void set_pause_bit(struct mt_spi_t *ms)
{
	u32 reg_val;
	reg_val = spi_readl(ms, SPI_CMD_REG);
	reg_val |= 1 << SPI_CMD_PAUSE_EN_OFFSET;
	spi_writel(ms, SPI_CMD_REG, reg_val);
}

static inline void clear_pause_bit(struct mt_spi_t *ms)
{
	u32 reg_val;
	reg_val = spi_readl(ms, SPI_CMD_REG);
	reg_val &= ~SPI_CMD_PAUSE_EN_MASK;
	spi_writel(ms, SPI_CMD_REG, reg_val);
}

static inline void clear_resume_bit(struct mt_spi_t *ms)
{
	u32 reg_val;
	reg_val = spi_readl(ms, SPI_CMD_REG);
	reg_val &= ~SPI_CMD_RESUME_MASK;
	spi_writel(ms, SPI_CMD_REG, reg_val);
}

static inline void spi_disable_dma(struct mt_spi_t *ms)
{
	u32  cmd;
	cmd = spi_readl(ms, SPI_CMD_REG);
	cmd &= ~SPI_CMD_TX_DMA_MASK;
	cmd &= ~SPI_CMD_RX_DMA_MASK;
	spi_writel(ms, SPI_CMD_REG, cmd);
}

static inline void spi_enable_dma(struct mt_spi_t *ms, u8 mode)
{
	u32  cmd;
	cmd = spi_readl(ms, SPI_CMD_REG);
	/*set up the DMA bus address*/
	if ((mode == DMA_TRANSFER) || (mode == OTHER1)) {
		if ((ms->cur_transfer->tx_buf != NULL) ||
			((ms->cur_transfer->tx_dma != INVALID_DMA_ADDRESS)
				&& (ms->cur_transfer->tx_dma != 0))) {
			if (ms->cur_transfer->tx_dma & (SPI_4B_ALIGN - 1)) {
				dev_err(&ms->pdev->dev,
					"Tx_DMA address should be 4Byte alignment,buf:%p,dma:%x\n",
					ms->cur_transfer->tx_buf,
					ms->cur_transfer->tx_dma);
			}
			spi_writel(ms, SPI_TX_SRC_REG,
				cpu_to_le32(ms->cur_transfer->tx_dma));
			cmd |= 1 << SPI_CMD_TX_DMA_OFFSET;
		}
	}
	if ((mode == DMA_TRANSFER) || (mode == OTHER2)) {
		if ((ms->cur_transfer->rx_buf != NULL) ||
			((ms->cur_transfer->rx_dma != INVALID_DMA_ADDRESS)
				&& (ms->cur_transfer->rx_dma != 0))) {
			if (ms->cur_transfer->rx_dma & (SPI_4B_ALIGN - 1)) {

				dev_err(&ms->pdev->dev,
					"Rx_DMA address should be 4Byte alignment,buf:%p,dma:%x\n",
					ms->cur_transfer->rx_buf,
					ms->cur_transfer->rx_dma);
			}
			spi_writel(ms, SPI_RX_DST_REG,
				cpu_to_le32(ms->cur_transfer->rx_dma));
			cmd |= 1 << SPI_CMD_RX_DMA_OFFSET;
		}
	}
	spi_writel(ms, SPI_CMD_REG, cmd);
}

static inline int spi_setup_packet(struct mt_spi_t *ms)
{
	u32  packet_size, packet_loop, cfg1;

	if (ms->cur_transfer->len < PACKET_SIZE)
		packet_size = ms->cur_transfer->len;
	else
		packet_size = PACKET_SIZE;

	if (ms->cur_transfer->len % packet_size) {
		packet_loop = ms->cur_transfer->len/packet_size + 1;
		/*parameter not correct,
		there will be more data transfer,
		notice user to change*/
		dev_err(&ms->pdev->dev,
			"ERROR!!The lens must be a multiple of %d, your len %u\n\n",
			PACKET_SIZE, ms->cur_transfer->len);
		return -EINVAL;
	} else
		packet_loop = (ms->cur_transfer->len)/packet_size;

	dev_dbg(&ms->pdev->dev, "The packet_size:0x%x packet_loop:0x%x\n",
		packet_size, packet_loop);

	cfg1 = spi_readl(ms, SPI_CFG1_REG);
	cfg1 &= ~(SPI_CFG1_PACKET_LENGTH_MASK + SPI_CFG1_PACKET_LOOP_MASK);
	cfg1 |= (packet_size - 1)<<SPI_CFG1_PACKET_LENGTH_OFFSET;
	cfg1 |= (packet_loop - 1)<<SPI_CFG1_PACKET_LOOP_OFFSET;
	spi_writel(ms, SPI_CFG1_REG, cfg1);

	return 0;
}

static inline void spi_start_transfer(struct mt_spi_t *ms)
{
	u32 reg_val;
	reg_val = spi_readl(ms, SPI_CMD_REG);
	reg_val |= 1 << SPI_CMD_ACT_OFFSET;
	spi_writel(ms, SPI_CMD_REG, reg_val);
}

static inline void spi_resume_transfer(struct mt_spi_t *ms)
{
	u32 reg_val;
	reg_val = spi_readl(ms, SPI_CMD_REG);
	reg_val &= ~SPI_CMD_RESUME_MASK;
	reg_val |= 1 << SPI_CMD_RESUME_OFFSET;
	spi_writel(ms,  SPI_CMD_REG, reg_val);
}

static inline void reset_spi(struct mt_spi_t *ms)
{
	u32 reg_val;
	/*set the software reset bit in SPI_CMD_REG.*/
	reg_val = spi_readl(ms, SPI_CMD_REG);
	reg_val &= ~SPI_CMD_RST_MASK;
	reg_val |= 1 << SPI_CMD_RST_OFFSET;
	spi_writel(ms, SPI_CMD_REG, reg_val);
	reg_val = spi_readl(ms, SPI_CMD_REG);
	reg_val &= ~SPI_CMD_RST_MASK;
	spi_writel(ms, SPI_CMD_REG, reg_val);
}

static inline int is_last_xfer(struct spi_message *msg,
	struct spi_transfer *xfer)
{
	return msg->transfers.prev == &xfer->transfer_list;
}

static inline int transfer_dma_mapping(struct mt_spi_t *ms, u8 mode,
	struct spi_transfer *xfer)
{
	struct device *dev = &ms->pdev->dev;
	xfer->tx_dma = xfer->rx_dma = INVALID_DMA_ADDRESS;

	if ((mode == DMA_TRANSFER) || (mode == OTHER1)) {
		if (xfer->tx_buf) {
			xfer->tx_dma = dma_map_single(dev, (void *)xfer->tx_buf,
				xfer->len,  DMA_TO_DEVICE);
			if (dma_mapping_error(dev, xfer->tx_dma)) {
				dev_err(&ms->pdev->dev, "dma mapping tx_buf error.\n");
				return -ENOMEM;
			}
		}

	}
	if ((mode == DMA_TRANSFER) || (mode == OTHER2)) {
		if (xfer->rx_buf) {
			xfer->rx_dma = dma_map_single(dev, xfer->rx_buf,
				xfer->len, DMA_FROM_DEVICE);
			if (dma_mapping_error(dev, xfer->rx_dma)) {
				if (xfer->tx_buf)
					dma_unmap_single(dev, xfer->tx_dma,
						xfer->len, DMA_TO_DEVICE);
				dev_err(&ms->pdev->dev, "dma mapping rx_buf error.\n");
				return -ENOMEM;
			}
		}
	}

	return 0;
}

static inline void transfer_dma_unmapping(struct mt_spi_t *ms,
	struct spi_transfer *xfer)
{
	struct device *dev =  &ms->pdev->dev;

	if ((xfer->tx_dma != INVALID_DMA_ADDRESS) && (xfer->tx_dma != 0)) {
		dma_unmap_single(dev, xfer->tx_dma, xfer->len, DMA_TO_DEVICE);
		dev_alert(dev, "tx_dma:0x%x,unmapping\n", xfer->tx_dma);
		xfer->tx_dma = INVALID_DMA_ADDRESS;
	}

	if ((xfer->rx_dma != INVALID_DMA_ADDRESS) && (xfer->rx_dma != 0)) {
		dma_unmap_single(dev, xfer->rx_dma, xfer->len, DMA_FROM_DEVICE);
		dev_alert(dev, "rx_dma:0x%x,unmapping\n", xfer->rx_dma);
		xfer->rx_dma = INVALID_DMA_ADDRESS;
	}
}

static void mt_spi_msg_done(struct mt_spi_t *ms,
	struct spi_message *msg, int status);
static void mt_spi_next_message(struct mt_spi_t *ms);
static int	mt_do_spi_setup(struct mt_spi_t *ms,
	struct mt_chip_conf *chip_config);

static inline int mt_spi_next_xfer(struct mt_spi_t *ms,
	struct spi_message *msg)
{
	struct spi_transfer	*xfer;
	struct mt_chip_conf *chip_config = (struct mt_chip_conf *)msg->state;
	u8 mode, cnt, i;
	int ret = 0;

	if (unlikely(!ms)) {
		dev_err(&msg->spi->dev, "master wrapper is invalid\n");
		ret = -EINVAL;
		goto fail;
	}
	if (unlikely(!msg)) {
		dev_err(&msg->spi->dev, "msg is invalid\n");
		ret = -EINVAL;
		goto fail;
	}
	if (unlikely(!msg->state)) {
		dev_err(&msg->spi->dev, "msg config is invalid\n");
		ret = -EINVAL;
		goto fail;
	}
	if (unlikely(!is_interrupt_enable(ms))) {
		dev_err(&msg->spi->dev, "interrupt is disable\n");
		ret = -EINVAL;
		goto fail;
	}

	mode = chip_config->com_mod;
	xfer = ms->cur_transfer;
	dev_dbg(&ms->pdev->dev,
		"start xfer 0x%p, mode %d, len %u\n", xfer, mode, xfer->len);
	if ((mode == FIFO_TRANSFER) || (mode == OTHER1) || (mode == OTHER2)) {
		if (xfer->len > SPI_FIFO_SIZE) {
			ret = -EINVAL;
			dev_err(&msg->spi->dev, "xfer len is invalid over fifo size\n");
			goto fail;
		}
	}
	if (is_last_xfer(msg, xfer)) {
		dev_dbg(&ms->pdev->dev, "The last xfer.\n");
		ms->next_transfer = NULL;
		clear_pause_bit(ms);
	} else{
		dev_dbg(&ms->pdev->dev, "Not the last xfer.\n");
		ms->next_transfer = list_entry(xfer->transfer_list.next,
				struct spi_transfer, transfer_list);
	}
	/*Disable DMA*/
	spi_disable_dma(ms);
	ret = spi_setup_packet(ms);
	if (ret < 0)
		goto fail;
	/*Using FIFO to send data*/
	if ((mode == FIFO_TRANSFER) || (mode == OTHER2)) {
		cnt = (xfer->len%4)?(xfer->len/4 + 1):(xfer->len/4);
		for (i = 0; i < cnt; i++) {
			spi_writel(ms, SPI_TX_DATA_REG,
				*((u32 *)xfer->tx_buf + i));
			dev_alert(&msg->spi->dev, "tx_buf data is:%x\n",
				*((u32 *)xfer->tx_buf + i));
			dev_alert(&msg->spi->dev, "tx_buf addr is:%p\n",
				(u32 *)xfer->tx_buf + i);
		}
	}
	/*Using DMA to send data*/
	if ((mode == DMA_TRANSFER) || (mode == OTHER1) || (mode == OTHER2))
		spi_enable_dma(ms, mode);

	if (ms->running == PAUSED) {
		dev_dbg(&ms->pdev->dev, "pause status to resume.\n");
		spi_resume_transfer(ms);
	} else if (ms->running == IDLE) {
		dev_dbg(&ms->pdev->dev, "The xfer start\n");
		/*if there is only one transfer,
		pause bit should not be set.*/
		if (is_pause_mode(msg) && !is_last_xfer(msg, xfer)) {
			dev_dbg(&ms->pdev->dev, "set pause mode.\n");
			set_pause_bit(ms);
		}
		spi_start_transfer(ms);
	} else{
		dev_err(&msg->spi->dev, "Wrong status\n");
		ret = -1;
		goto fail;
	}
	ms->running = INPROGRESS;
	/*Exit pause mode*/
	if (is_pause_mode(msg) && is_last_xfer(msg, xfer))
		clear_resume_bit(ms);

	return 0;

fail:
	list_for_each_entry(xfer, &msg->transfers, transfer_list) {
		if ((!msg->is_dma_mapped))
			transfer_dma_unmapping(ms, xfer);
	}
	ms->running = IDLE;
	mt_spi_msg_done(ms, msg, ret);

	return ret;
}

static void mt_spi_msg_done(struct mt_spi_t *ms,
				struct spi_message *msg, int status)
{

	list_del(&msg->queue);
	msg->status = status;

	dev_dbg(&ms->pdev->dev, "msg:%p complete(%d): %u bytes transferred\n",
		msg, status, msg->actual_length);

	spi_disable_dma(ms);
	msg->complete(msg->context);
	ms->running			= IDLE;
	ms->cur_transfer	= NULL;
	ms->next_transfer	= NULL;
	/* continue if needed */
	if (list_empty(&ms->queue)) {
		dev_dbg(&ms->pdev->dev, "All msg is completion.\n\n");
		clk_disable(ms->clk);
	} else
		mt_spi_next_message(ms);
}

static void mt_spi_next_message(struct mt_spi_t *ms)
{
	struct spi_message	*msg;
	struct mt_chip_conf *chip_config;

	msg = list_entry(ms->queue.next, struct spi_message, queue);
	chip_config = (struct mt_chip_conf *)msg->state;
	dev_dbg(&ms->pdev->dev, "start transfer message:0x%p\n", msg);
	ms->cur_transfer = list_entry(msg->transfers.next,
		struct spi_transfer, transfer_list);
	/*spi and transfer reset*/
	reset_spi(ms);
	mt_do_spi_setup(ms, chip_config);
	mt_spi_next_xfer(ms, msg);

}

static int mt_spi_transfer(struct spi_device *spidev,
						struct spi_message *msg)
{
	struct spi_master *master;
	struct mt_spi_t *ms;
	struct spi_transfer *xfer;
	struct mt_chip_conf *chip_config;
	unsigned long flags;
	master = spidev->master;
	ms = spi_master_get_devdata(master);

	dev_dbg(&ms->pdev->dev, "enter,start add msg:0x%p\n", msg);

	if (unlikely(!msg)) {
		dev_err(&spidev->dev, "msg is NULL pointer.\n");
		msg->status = -EINVAL;
		goto out;
	}
	if (unlikely(list_empty(&msg->transfers))) {
		dev_err(&spidev->dev, "the message is NULL.\n");
		msg->status = -EINVAL;
		msg->actual_length = 0;
		goto out;
	}

	/*if device don't config chip, set default */
	if (master->setup(spidev)) {
		dev_err(&spidev->dev, "set up error.\n");
		msg->status = -EINVAL;
		msg->actual_length = 0;
		goto out;
	}

	chip_config	= (struct mt_chip_conf *)spidev->controller_data;
	msg->state	= chip_config;

	list_for_each_entry(xfer, &msg->transfers, transfer_list) {
		if (!((xfer->tx_buf || xfer->rx_buf) && xfer->len)) {
			dev_err(&spidev->dev,
				"missing tx %p or rx %p buf, len%d\n",
				xfer->tx_buf, xfer->rx_buf, xfer->len);
			msg->status = -EINVAL;
			goto out;
		}
		/*
		 * DMA map early, for performance (empties dcache ASAP) and
		 * better fault reporting.
		 *
		 * NOTE that if dma_unmap_single() ever starts to do work on
		 * platforms supported by this driver, we would need to clean
		 * up mappings for previously-mapped transfers.
		 */
		if ((!msg->is_dma_mapped)) {
			if (transfer_dma_mapping(ms,
				chip_config->com_mod, xfer) < 0)
				return -ENOMEM;
		}
	}
#ifdef SPI_VERBOSE
	list_for_each_entry(xfer, &msg->transfers, transfer_list) {
		dev_alert(&spidev->dev,
			"xfer %p: len %04u tx %p/%08x rx %p/%08x\n",
			xfer, xfer->len,
			xfer->tx_buf, xfer->tx_dma,
			xfer->rx_buf, xfer->rx_dma);
	}
#endif
	msg->status = -EINPROGRESS;
	msg->actual_length = 0;

	spin_lock_irqsave(&ms->lock, flags);
	list_add_tail(&msg->queue, &ms->queue);
	dev_dbg(&ms->pdev->dev, "add msg %p to queue\n", msg);
	if (!ms->cur_transfer) {
		clk_enable(ms->clk);
		mt_spi_next_message(ms);
	}
	spin_unlock_irqrestore(&ms->lock, flags);

	return 0;

out:
	return -1;

}
static irqreturn_t mt_spi_interrupt(int irq, void *dev_id)
{
	struct mt_spi_t *ms = (struct mt_spi_t *)dev_id;
	struct spi_message	*msg;
	struct spi_transfer	*xfer;
	struct mt_chip_conf *chip_config;

	unsigned long flags;
	u32 reg_val, cnt;
	u8 mode, i;

	spin_lock_irqsave(&ms->lock, flags);
	xfer = ms->cur_transfer;
	msg = list_entry(ms->queue.next, struct spi_message, queue);

	/*Clear interrupt status first by reading the register*/
	reg_val = spi_readl(ms, SPI_STATUS0_REG);
	dev_dbg(&ms->pdev->dev,
		"xfer:0x%p interrupt status:%x\n", xfer, reg_val & 0x3);

	if (unlikely(!msg)) {
		dev_dbg(&ms->pdev->dev, "msg in interrupt %d is NULL pointer.\n",
			reg_val & 0x3);
		goto out;
	}
	if (unlikely(!xfer)) {
		dev_dbg(&ms->pdev->dev, "xfer in interrupt %d is NULL pointer.\n",
			reg_val & 0x3);
		goto out;
	}

	chip_config = (struct mt_chip_conf *)msg->state;
	mode = chip_config->com_mod;

	if ((reg_val & 0x03) == 0)
		goto out;

	if (!msg->is_dma_mapped)
		transfer_dma_unmapping(ms, ms->cur_transfer);

	if (is_pause_mode(msg)) {
		if (ms->running == INPROGRESS)
			ms->running = PAUSED;
		else
			dev_err(&msg->spi->dev, "Wrong spi status.\n");
	} else
		ms->running = IDLE;

	if (is_fifo_read(msg) && xfer->rx_buf) {
		cnt = (xfer->len%4)?(xfer->len/4 + 1):(xfer->len/4);
		for (i = 0; i < cnt; i++) {
			reg_val = spi_readl(ms, SPI_RX_DATA_REG);
			dev_alert(&msg->spi->dev,
				"SPI_RX_DATA_REG:0x%x", reg_val);
			*((u32 *)xfer->rx_buf + i) = reg_val;
		}
	}

	msg->actual_length += xfer->len;

	if (is_last_xfer(msg, xfer)) {
		mt_spi_msg_done(ms, msg, 0);
	} else{
		ms->cur_transfer = ms->next_transfer;
		mt_spi_next_xfer(ms, msg);

	}

	spin_unlock_irqrestore(&ms->lock, flags);
	return IRQ_HANDLED;
out:
	spin_unlock_irqrestore(&ms->lock, flags);
	dev_dbg(&ms->pdev->dev, "return IRQ_NONE.\n");
	return IRQ_NONE;
}


/* Write chip configuration to HW register */
static int mt_do_spi_setup(struct mt_spi_t *ms,
	struct mt_chip_conf *chip_config)
{
	u32 reg_val;

	/*set the timing*/
	reg_val = spi_readl(ms, SPI_CFG0_REG);
	reg_val &= ~(SPI_CFG0_SCK_HIGH_MASK | SPI_CFG0_SCK_LOW_MASK);
	reg_val &= ~(SPI_CFG0_CS_HOLD_MASK | SPI_CFG0_CS_SETUP_MASK);
	reg_val |= ((chip_config->high_time-1) << SPI_CFG0_SCK_HIGH_OFFSET);
	reg_val |= ((chip_config->low_time-1) << SPI_CFG0_SCK_LOW_OFFSET);
	reg_val |= ((chip_config->holdtime-1) << SPI_CFG0_CS_HOLD_OFFSET);
	reg_val |= ((chip_config->setuptime-1) << SPI_CFG0_CS_SETUP_OFFSET);
	spi_writel(ms, SPI_CFG0_REG, reg_val);

	reg_val = spi_readl(ms, SPI_CFG1_REG);
	reg_val &= ~(SPI_CFG1_CS_IDLE_MASK);
	reg_val |= ((chip_config->cs_idletime-1) << SPI_CFG1_CS_IDLE_OFFSET);

	reg_val &= ~(SPI_CFG1_GET_TICK_DLY_MASK);
	reg_val |= ((chip_config->tckdly) << SPI_CFG1_GET_TICK_DLY_OFFSET);
	spi_writel(ms, SPI_CFG1_REG, reg_val);

	/*set the mlsbx and mlsbtx*/
	reg_val = spi_readl(ms, SPI_CMD_REG);
	reg_val &= ~(SPI_CMD_TX_ENDIAN_MASK | SPI_CMD_RX_ENDIAN_MASK);
	reg_val &= ~(SPI_CMD_TXMSBF_MASK | SPI_CMD_RXMSBF_MASK);
	reg_val &= ~(SPI_CMD_CPHA_MASK | SPI_CMD_CPOL_MASK);
	reg_val |= (chip_config->tx_mlsb << SPI_CMD_TXMSBF_OFFSET);
	reg_val |= (chip_config->rx_mlsb << SPI_CMD_RXMSBF_OFFSET);
	reg_val |= (chip_config->tx_endian << SPI_CMD_TX_ENDIAN_OFFSET);
	reg_val |= (chip_config->rx_endian << SPI_CMD_RX_ENDIAN_OFFSET);
	reg_val |= (chip_config->cpha << SPI_CMD_CPHA_OFFSET);
	reg_val |= (chip_config->cpol << SPI_CMD_CPOL_OFFSET);
	spi_writel(ms, SPI_CMD_REG, reg_val);

	/*set pause mode*/
	reg_val = spi_readl(ms, SPI_CMD_REG);
	reg_val &= ~SPI_CMD_PAUSE_EN_MASK;
	reg_val &= ~SPI_CMD_PAUSE_IE_MASK;
	reg_val |= (chip_config->pause << SPI_CMD_PAUSE_IE_OFFSET);
	spi_writel(ms, SPI_CMD_REG, reg_val);

	/*set finish interrupt always enable*/
	reg_val = spi_readl(ms, SPI_CMD_REG);
	reg_val &= ~SPI_CMD_FINISH_IE_MASK;
	reg_val |= (1 << SPI_CMD_FINISH_IE_OFFSET);

	spi_writel(ms, SPI_CMD_REG, reg_val);

	/*set the communication of mode*/
	reg_val = spi_readl(ms, SPI_CMD_REG);
	reg_val &= ~SPI_CMD_TX_DMA_MASK;
	reg_val &= ~SPI_CMD_RX_DMA_MASK;
	spi_writel(ms, SPI_CMD_REG, reg_val);

	/*set deassert mode*/
	reg_val = spi_readl(ms, SPI_CMD_REG);
	reg_val &= ~SPI_CMD_DEASSERT_MASK;
	reg_val |= (chip_config->deassert << SPI_CMD_DEASSERT_OFFSET);
	spi_writel(ms, SPI_CMD_REG, reg_val);

	return 0;
}

static int mt_spi_setup(struct spi_device *spidev)
{
	struct spi_master *master;
	struct mt_spi_t *ms;
	struct mt_chip_conf *chip_config = NULL;

	master = spidev->master;
	ms = spi_master_get_devdata(master);

	if (!spidev) {
		dev_err(&spidev->dev, "spi device %s: error.\n",
			dev_name(&spidev->dev));
	}
	if (spidev->chip_select >= master->num_chipselect) {
		dev_err(&spidev->dev,
			"master's chipselect number wrong.\n");
		return -EINVAL;
	}

	chip_config = (struct mt_chip_conf *)spidev->controller_data;
	if (!chip_config) {
		chip_config =  kzalloc(sizeof(struct mt_chip_conf), GFP_KERNEL);
		if (!chip_config) {
			dev_err(&spidev->dev,
				" spidev %s: can not get enough memory.\n",
				dev_name(&spidev->dev));
			return -ENOMEM;
		}
		dev_dbg(&ms->pdev->dev, "device %s: set default at chip's runtime state\n",
			dev_name(&spidev->dev));

		chip_config->setuptime = 3;
		chip_config->holdtime = 3;
		chip_config->high_time = 10;
		chip_config->low_time = 10;
		chip_config->cs_idletime = 2;
		chip_config->cpol = 0;
		chip_config->cpha = 1;
		chip_config->rx_mlsb = 1;
		chip_config->tx_mlsb = 1;
		chip_config->tx_endian = 0;
		chip_config->rx_endian = 0;
		chip_config->com_mod = DMA_TRANSFER;
		chip_config->pause = 0;
		chip_config->finish_intr = 1;
		chip_config->deassert = 0;
		chip_config->tckdly = 0;

		spidev->controller_data = chip_config;
	}

	dev_alert(&spidev->dev, "set up chip config,mode:%d\n",
		chip_config->com_mod);

	/*check chip configuration valid*/

	ms->config = chip_config;
	if (!((chip_config->pause == PAUSE_MODE_ENABLE) ||
		(chip_config->pause == PAUSE_MODE_DISABLE)) ||
		!((chip_config->cpol == SPI_CPOL_0) ||
			(chip_config->cpol == SPI_CPOL_1)) ||
		!((chip_config->cpha == SPI_CPHA_0) ||
			(chip_config->cpha == SPI_CPHA_1)) ||
		!((chip_config->tx_mlsb == SPI_LSB) ||
			(chip_config->tx_mlsb == SPI_MSB)) ||
		!((chip_config->com_mod == FIFO_TRANSFER) ||
			(chip_config->com_mod == DMA_TRANSFER) ||
		 (chip_config->com_mod == OTHER1) ||
		 (chip_config->com_mod == OTHER2))) {
		return -EINVAL;
	}
	return 0;
}

static void mt_spi_cleanup(struct spi_device *spidev)
{
	struct spi_master *master;
	struct mt_spi_t *ms;

	master = spidev->master;
	ms = spi_master_get_devdata(master);

	dev_dbg(&ms->pdev->dev, "Calling mt_spi_cleanup.\n");

	spidev->controller_data = NULL;
	spidev->master = NULL;

	return;
}

static int __init mt_spi_probe(struct platform_device *pdev)
{
	int	ret = 0;
	int	irq;
	struct clk *clk;
	struct spi_master *master;
	struct resource *res;
	struct mt_spi_t *ms;
	void __iomem *spi_base;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(&pdev->dev, "get resource res NULL.\n");
		return -ENXIO;
	}

	spi_base = devm_ioremap_resource(&pdev->dev, res);
	if (!spi_base) {
		dev_err(&pdev->dev, "could not get the io memory.\n");
		return -ENODEV;
	}

	clk = devm_clk_get(&pdev->dev, "spi1_ck");
	if (IS_ERR(clk)) {
		dev_err(&pdev->dev, "could not find clk: %ld\n", PTR_ERR(clk));
		return PTR_ERR(clk);
	}

	irq = platform_get_irq(pdev, 0);
	if (irq  < 0) {
		dev_err(&pdev->dev, "platform_get_irq error. get invalid irq\n");
		return irq;
	}

	if (of_property_read_u32(pdev->dev.of_node, "cell-index",
				 &pdev->id)) {
		dev_err(&pdev->dev, "SPI get cell-index failed\n");
		return -ENODEV;
	}

	if (!pdev->dev.dma_mask)
		pdev->dev.dma_mask = &pdev->dev.coherent_dma_mask;

	pr_info("SPI reg: 0x%p  irq: %d id: %d\n", spi_base, irq, pdev->id);

	master = spi_alloc_master(&pdev->dev, sizeof(struct mt_spi_t));
	if (!master) {
		dev_err(&pdev->dev, " device %s: alloc spi master fail.\n",
			dev_name(&pdev->dev));
		goto out;
	}
	/*hardware can only connect 1 slave.
	if you want to multple, using gpio CS*/
	master->num_chipselect = 2;
	master->mode_bits = (SPI_CPOL | SPI_CPHA);
	master->bus_num = pdev->id;
	master->setup = mt_spi_setup;
	master->transfer = mt_spi_transfer;
	master->cleanup = mt_spi_cleanup;
	master->dev.of_node = pdev->dev.of_node;
	platform_set_drvdata(pdev, master);

	ms = spi_master_get_devdata(master);
	ms->regs = spi_base;
	ms->pdev = pdev;
	ms->irq	= irq;
	ms->clk = clk;
	ms->running = IDLE;
	ms->cur_transfer = NULL;
	ms->next_transfer = NULL;

	spin_lock_init(&ms->lock);
	INIT_LIST_HEAD(&ms->queue);

	dev_alert(&pdev->dev, "Controller at 0x%p (irq %d)\n", ms->regs, irq);

	ret = request_irq(irq, mt_spi_interrupt, IRQF_TRIGGER_NONE,
			  dev_name(&pdev->dev), ms);

	if (ret) {
		dev_err(&pdev->dev, "registering interrupt handler fails.\n");
		goto out;
	}

	spi_master_set_devdata(master, ms);
	reset_spi(ms);
	clk_prepare_enable(clk);

	ret = spi_register_master(master);
	if (ret) {
		dev_err(&pdev->dev, "spi_register_master fails.\n");
		goto out_free;
	} else {
		dev_dbg(&ms->pdev->dev, "spi register master success.\n");
		spi_dump_reg(ms);
		return 0;
	}
out_free:
	free_irq(irq, ms);
out:
	spi_master_put(master);
	return ret;
}

static int __exit mt_spi_remove(struct platform_device *pdev)
{
	struct mt_spi_t *ms;
	struct spi_message *msg;
	struct spi_master *master = platform_get_drvdata(pdev);
	if (!master) {
		dev_err(&pdev->dev,
			"master %s: is invalid.\n",
			dev_name(&pdev->dev));
		return -EINVAL;
	}
	ms = spi_master_get_devdata(master);

	list_for_each_entry(msg, &ms->queue, queue) {
		msg->status = -ESHUTDOWN;
		msg->complete(msg->context);
	}
	ms->cur_transfer = NULL;
	ms->running = IDLE;

	reset_spi(ms);
	free_irq(ms->irq, master);
	spi_unregister_master(master);
	return 0;
}

static const struct of_device_id mt_spi_of_match[] = {
	{ .compatible = "mediatek,mt6577-spi", },
	{},
};

MODULE_DEVICE_TABLE(of, mt_spi_of_match);

struct platform_driver mt_spi_driver = {
	.driver = {
		.name = "mt-spi",
		.owner = THIS_MODULE,
		.of_match_table = mt_spi_of_match,
	},
	.probe = mt_spi_probe,
	.remove = mt_spi_remove,
};

static int __init mt_spi_init(void)
{
	int ret;

	ret = platform_driver_register(&mt_spi_driver);
	return ret;
}

static void __exit mt_spi_exit(void)
{
	platform_driver_unregister(&mt_spi_driver);
}

module_init(mt_spi_init);
module_exit(mt_spi_exit);

MODULE_DESCRIPTION("mt SPI Controller driver");
MODULE_AUTHOR("Tonny Zhang <tonny.zhang@mediatek.com>");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform: mt_spi");
