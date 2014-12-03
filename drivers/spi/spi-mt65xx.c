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

#define SPI_CFG0_REG                      0x0000
#define SPI_CFG1_REG                      0x0004
#define SPI_TX_SRC_REG                    0x0008
#define SPI_RX_DST_REG                    0x000c
#define SPI_TX_DATA_REG                   0x0010
#define SPI_RX_DATA_REG                   0x0014
#define SPI_CMD_REG                       0x0018
#define SPI_STATUS0_REG                   0x001c
#define SPI_STATUS1_REG                   0x0020

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

#define IDLE 0
#define INPROGRESS 1
#define PAUSED 2

#define PACKET_SIZE 0x400
#define SPI_4B_ALIGN 0x4
#define SPI_FIFO_SIZE 32
#define INVALID_DMA_ADDRESS 0xffffffff
#define SPI_LOOP_TEST
#define SPI_DMA_TIMEOUT		(msecs_to_jiffies(100))

struct mtk_spi_driver_data {
	struct platform_device *pdev;
	void __iomem *regs;
	int irq;
	int running_status;
	struct clk *clk;
	struct mt_chip_conf *config;
	struct spi_master *master;
	struct spi_transfer *next_xfer;
	spinlock_t lock;
	struct completion xfer_completion;
	struct list_head queue;
};

static void mtk_spi_reg_info(struct mtk_spi_driver_data *msd)
{
	dev_dbg(&msd->pdev->dev, "||********************||\n");
	dev_dbg(&msd->pdev->dev, "cfg0:0x%.8x\n",
		readl(msd->regs + SPI_CFG0_REG));
	dev_dbg(&msd->pdev->dev, "cfg1:0x%.8x\n",
		readl(msd->regs + SPI_CFG1_REG));
	dev_dbg(&msd->pdev->dev, "cmd :0x%.8x\n",
		readl(msd->regs + SPI_CMD_REG));
	dev_dbg(&msd->pdev->dev, "tx_s:0x%.8x\n",
		readl(msd->regs + SPI_TX_SRC_REG));
	dev_dbg(&msd->pdev->dev, "rx_d:0x%.8x\n",
		readl(msd->regs + SPI_RX_DST_REG));
	dev_dbg(&msd->pdev->dev, "st1:0x%.8x\n",
		readl(msd->regs + SPI_STATUS1_REG));
	dev_dbg(&msd->pdev->dev, "||********************||\n");
}

static int is_pause_mode(struct spi_message	*msg)
{
	struct mt_chip_conf *conf;
	conf = (struct mt_chip_conf *)msg->state;
	return conf->pause;
}

static int is_fifo_read(struct spi_message *msg)
{
	struct mt_chip_conf *conf;
	conf = (struct mt_chip_conf *)msg->state;
	return (conf->com_mod == FIFO_TRANSFER) || (conf->com_mod == OTHER1);
}

static inline int is_last_xfer(struct spi_message *msg,
				struct spi_transfer *xfer)
{
	return msg->transfers.prev == &xfer->transfer_list;
}

static int is_interrupt_enable(struct mtk_spi_driver_data *msd)
{
	u32 cmd;
	cmd = readl(msd->regs + SPI_CMD_REG);
	return (cmd >> SPI_CMD_FINISH_IE_OFFSET) & 1;
}

static inline void mtk_set_pause_bit(struct mtk_spi_driver_data *msd)
{
	u32 reg_val;
	reg_val = readl(msd->regs + SPI_CMD_REG);
	reg_val |= 1 << SPI_CMD_PAUSE_EN_OFFSET;
	writel(reg_val, msd->regs + SPI_CMD_REG);
}

static inline void mtk_clear_pause_bit(struct mtk_spi_driver_data *msd)
{
	u32 reg_val;
	reg_val = readl(msd->regs + SPI_CMD_REG);
	reg_val &= ~SPI_CMD_PAUSE_EN_MASK;
	writel(reg_val, msd->regs + SPI_CMD_REG);
}

static inline void mtk_clear_resume_bit(struct mtk_spi_driver_data *msd)
{
	u32 reg_val;
	reg_val = readl(msd->regs + SPI_CMD_REG);
	reg_val &= ~SPI_CMD_RESUME_MASK;
	writel(reg_val, msd->regs + SPI_CMD_REG);
}

static inline void mtk_spi_disable_dma(struct mtk_spi_driver_data *msd)
{
	u32  cmd;
	cmd = readl(msd->regs + SPI_CMD_REG);
	cmd &= ~SPI_CMD_TX_DMA_MASK;
	cmd &= ~SPI_CMD_RX_DMA_MASK;
	writel(cmd, msd->regs + SPI_CMD_REG);
}

static inline void mtk_spi_dma_transfer(struct mtk_spi_driver_data *msd,
			struct spi_transfer *xfer, u8 mode)
{
	u32  cmd;
	cmd = readl(msd->regs + SPI_CMD_REG);
	/*set up the DMA bus address*/
	if ((mode == DMA_TRANSFER) || (mode == OTHER1)) {
		if ((xfer->tx_buf != NULL)
			|| ((xfer->tx_dma != INVALID_DMA_ADDRESS)
			&& (xfer->tx_dma != 0))) {
			if (xfer->tx_dma & (SPI_4B_ALIGN - 1))
				dev_err(&msd->pdev->dev,
					"Warning!Tx_DMA address should be 4Byte alignment,buf:%p,dma:%x\n",
					xfer->tx_buf, xfer->tx_dma);
			writel(cpu_to_le32(xfer->tx_dma),
						msd->regs + SPI_TX_SRC_REG);
			cmd |= 1 << SPI_CMD_TX_DMA_OFFSET;
		}
	}
	if ((mode == DMA_TRANSFER) || (mode == OTHER2)) {
		if ((xfer->rx_buf != NULL) ||
			((xfer->rx_dma != INVALID_DMA_ADDRESS)
			&& (xfer->rx_dma != 0))) {
			if (xfer->rx_dma & (SPI_4B_ALIGN - 1))
				dev_err(&msd->pdev->dev,
					"Warning!Rx_DMA address should be 4Byte alignment,buf:%p,dma:%x\n",
					xfer->rx_buf, xfer->rx_dma);
			writel(cpu_to_le32(xfer->rx_dma),
						msd->regs + SPI_RX_DST_REG);
			cmd |= 1 << SPI_CMD_RX_DMA_OFFSET;
		}
	}

	writel(cmd, msd->regs + SPI_CMD_REG);
}



static inline int mtk_spi_setup_packet(struct mtk_spi_driver_data *msd,
				struct spi_transfer *xfer)
{
	u32  packet_size, packet_loop, cfg1;

	if (xfer->len < PACKET_SIZE)
		packet_size = xfer->len;
	else
		packet_size = PACKET_SIZE;

	if (xfer->len % packet_size) {
		packet_loop = xfer->len/packet_size + 1;
		dev_err(&msd->pdev->dev,
			"ERROR!!The lens must be a multiple of %d, your len %u\n\n",
			PACKET_SIZE, xfer->len);
		return -EINVAL;
	} else
		packet_loop = (xfer->len)/packet_size;
	dev_info(&msd->pdev->dev,
		"The packet_size:0x%x packet_loop:0x%x\n",
		packet_size, packet_loop);

	cfg1 = readl(msd->regs + SPI_CFG1_REG);
	cfg1 &= ~(SPI_CFG1_PACKET_LENGTH_MASK + SPI_CFG1_PACKET_LOOP_MASK);
	cfg1 |= (packet_size - 1)<<SPI_CFG1_PACKET_LENGTH_OFFSET;
	cfg1 |= (packet_loop - 1)<<SPI_CFG1_PACKET_LOOP_OFFSET;
	writel(cfg1, msd->regs + SPI_CFG1_REG);

	return 0;
}

static inline void mtk_spi_start_transfer(struct mtk_spi_driver_data *msd)
{
	u32 reg_val;
	reg_val = readl(msd->regs + SPI_CMD_REG);
	reg_val |= 1 << SPI_CMD_ACT_OFFSET;
	/*All register must be prepared before setting the start bit [SMP]*/
	mb();
	writel(reg_val, msd->regs + SPI_CMD_REG);
}

static inline void mtk_spi_resume_transfer(struct mtk_spi_driver_data *msd)
{
	u32 reg_val;

	reg_val = readl(msd->regs + SPI_CMD_REG);
	reg_val &= ~SPI_CMD_RESUME_MASK;
	reg_val |= 1 << SPI_CMD_RESUME_OFFSET;
	/*All register must be prepared before setting the start bit [SMP]*/
	mb();
	writel(reg_val, msd->regs + SPI_CMD_REG);
}

static inline void mtk_spi_reset(struct mtk_spi_driver_data *msd)
{
	u32 reg_val;
	/*set the software reset bit in SPI_CMD_REG.*/
	reg_val = readl(msd->regs + SPI_CMD_REG);
	reg_val &= ~SPI_CMD_RST_MASK;
	reg_val |= 1 << SPI_CMD_RST_OFFSET;
	writel(reg_val, msd->regs + SPI_CMD_REG);
	reg_val = readl(msd->regs + SPI_CMD_REG);
	reg_val &= ~SPI_CMD_RST_MASK;
	writel(reg_val, msd->regs + SPI_CMD_REG);
}

static int mtk_spi_config(struct mtk_spi_driver_data *msd,
					struct mt_chip_conf *chip_config)
{
	u32 reg_val;
	/*set the timing*/
	reg_val = readl(msd->regs + SPI_CFG0_REG);
	reg_val &= ~(SPI_CFG0_SCK_HIGH_MASK | SPI_CFG0_SCK_LOW_MASK);
	reg_val &= ~(SPI_CFG0_CS_HOLD_MASK | SPI_CFG0_CS_SETUP_MASK);
	reg_val |= ((chip_config->high_time-1) << SPI_CFG0_SCK_HIGH_OFFSET);
	reg_val |= ((chip_config->low_time-1) << SPI_CFG0_SCK_LOW_OFFSET);
	reg_val |= ((chip_config->holdtime-1) << SPI_CFG0_CS_HOLD_OFFSET);
	reg_val |= ((chip_config->setuptime-1) << SPI_CFG0_CS_SETUP_OFFSET);
	writel(reg_val, msd->regs + SPI_CFG0_REG);

	reg_val = readl(msd->regs + SPI_CFG1_REG);
	reg_val &= ~SPI_CFG1_CS_IDLE_MASK;
	reg_val |= ((chip_config->cs_idletime-1) << SPI_CFG1_CS_IDLE_OFFSET);

	reg_val &= ~SPI_CFG1_GET_TICK_DLY_MASK;
	reg_val |= ((chip_config->tckdly) << SPI_CFG1_GET_TICK_DLY_OFFSET);
	writel(reg_val, msd->regs + SPI_CFG1_REG);

	/*set the mlsbx and mlsbtx*/
	reg_val = readl(msd->regs + SPI_CMD_REG);
	reg_val &= ~(SPI_CMD_TX_ENDIAN_MASK | SPI_CMD_RX_ENDIAN_MASK);
	reg_val &= ~(SPI_CMD_TXMSBF_MASK | SPI_CMD_RXMSBF_MASK);
	reg_val &= ~(SPI_CMD_CPHA_MASK | SPI_CMD_CPOL_MASK);
	reg_val |= (chip_config->tx_mlsb << SPI_CMD_TXMSBF_OFFSET);
	reg_val |= (chip_config->rx_mlsb << SPI_CMD_RXMSBF_OFFSET);
	reg_val |= (chip_config->tx_endian << SPI_CMD_TX_ENDIAN_OFFSET);
	reg_val |= (chip_config->rx_endian << SPI_CMD_RX_ENDIAN_OFFSET);
	reg_val |= (chip_config->cpha << SPI_CMD_CPHA_OFFSET);
	reg_val |= (chip_config->cpol << SPI_CMD_CPOL_OFFSET);
	writel(reg_val, msd->regs + SPI_CMD_REG);

	/*set pause mode*/
	reg_val = readl(msd->regs + SPI_CMD_REG);
	reg_val &= ~SPI_CMD_PAUSE_EN_MASK;
	reg_val &= ~SPI_CMD_PAUSE_IE_MASK;
	reg_val |= (chip_config->pause << SPI_CMD_PAUSE_IE_OFFSET);
	writel(reg_val, msd->regs + SPI_CMD_REG);

	/*set finish interrupt always enable*/
	reg_val = readl(msd->regs + SPI_CMD_REG);
	reg_val &= ~SPI_CMD_FINISH_IE_MASK;
	reg_val |= (1 << SPI_CMD_FINISH_IE_OFFSET);

	writel(reg_val, msd->regs + SPI_CMD_REG);

	/*set the communication of mode*/
	reg_val = readl(msd->regs + SPI_CMD_REG);
	reg_val &= ~SPI_CMD_TX_DMA_MASK;
	reg_val &= ~SPI_CMD_RX_DMA_MASK;
	writel(reg_val, msd->regs + SPI_CMD_REG);

	/*set deassert mode*/
	reg_val = readl(msd->regs + SPI_CMD_REG);
	reg_val &= ~SPI_CMD_DEASSERT_MASK;
	reg_val |= (chip_config->deassert << SPI_CMD_DEASSERT_OFFSET);
	writel(reg_val, msd->regs + SPI_CMD_REG);

	return 0;
}

static int mtk_transfer_dma_mapping(struct mtk_spi_driver_data *msd, u8 mode,
	struct spi_transfer *xfer)
{
	struct device *dev = &msd->pdev->dev;
	xfer->tx_dma = xfer->rx_dma = INVALID_DMA_ADDRESS;

	if ((mode == DMA_TRANSFER) || (mode == OTHER1)) {
		if (xfer->tx_buf) {
			xfer->tx_dma = dma_map_single(dev, (void *)xfer->tx_buf,
				xfer->len, DMA_TO_DEVICE);

			if (dma_mapping_error(dev, xfer->tx_dma)) {
				dev_err(&msd->pdev->dev, "dma mapping tx_buf error.\n");
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

				dev_err(&msd->pdev->dev, "dma mapping rx_buf error.\n");
				return -ENOMEM;
			}
		}
	}

	return 0;
}

static void mtk_transfer_dma_unmapping(struct mtk_spi_driver_data *msd,
	struct spi_transfer *xfer)
{
	struct device *dev =  &msd->pdev->dev;

	if ((xfer->tx_dma != INVALID_DMA_ADDRESS) && (xfer->tx_dma != 0)) {
		dma_unmap_single(dev, xfer->tx_dma, xfer->len, DMA_TO_DEVICE);
		dev_dbg(dev, "tx_dma:0x%x,unmapping\n", xfer->tx_dma);
		xfer->tx_dma = INVALID_DMA_ADDRESS;
	}
	if ((xfer->rx_dma != INVALID_DMA_ADDRESS) && (xfer->rx_dma != 0)) {
		dma_unmap_single(dev, xfer->rx_dma, xfer->len, DMA_FROM_DEVICE);
		dev_dbg(dev, "rx_dma:0x%x,unmapping\n", xfer->rx_dma);
		xfer->rx_dma = INVALID_DMA_ADDRESS;
	}
}


static int mtk_spi_transfer_one_message(struct spi_master *master,
						struct spi_message *msg)
{
	struct mtk_spi_driver_data *msd;
	struct spi_transfer *xfer;
	struct mt_chip_conf *chip_config;
	u8  mode, cnt, i;
	int ret = 0;
	u32 reg_val;
	struct spi_device *spidev = msg->spi;
	void __iomem *regs;

	msd = spi_master_get_devdata(master);
	regs = msd->regs;

	dev_info(&msd->pdev->dev, "enter,start add msg:0x%p\n", msg);
	/*if device don't config chip, set default */
	if (master->setup(spidev)) {
		dev_err(&spidev->dev, "set up error.\n");
		msg->status = -EINVAL;
		msg->actual_length = 0;
		goto fail;
	}

	chip_config	= (struct mt_chip_conf *)spidev->controller_data;
	msg->state	= chip_config;

	list_for_each_entry(xfer, &msg->transfers, transfer_list) {
		 if (!((xfer->tx_buf || xfer->rx_buf) && xfer->len)) {
			dev_err(&spidev->dev, "tx %p or rx %p buf, len%d\n",
				xfer->tx_buf, xfer->rx_buf, xfer->len);
			 msg->status = -EINVAL;
			 goto fail;
		 }

		if ((!msg->is_dma_mapped)) {
			if (mtk_transfer_dma_mapping(msd,
						chip_config->com_mod, xfer) < 0)
				return -ENOMEM;
		}

		mtk_spi_reset(msd);
		mode = chip_config->com_mod;
		mtk_spi_config(msd, chip_config);
		cnt = (xfer->len%4)?(xfer->len/4 + 1):(xfer->len/4);

		dev_info(&msd->pdev->dev, "Start xfer 0x%p, mode %d, len %u\n",
			xfer, mode, xfer->len);
		if ((mode == FIFO_TRANSFER) || (mode == OTHER1)
			|| (mode == OTHER2)) {
			if (xfer->len > SPI_FIFO_SIZE) {
				dev_err(&msg->spi->dev, "xfer len is invalid over fifo size\n");
				ret = -EINVAL;
				goto fail;
			}
		}

		if (is_last_xfer(msg, xfer)) {
			dev_info(&msd->pdev->dev, "The last xfer.\n");
			msd->next_xfer = NULL;
			mtk_clear_pause_bit(msd);
		} else {
			dev_info(&msd->pdev->dev, "Not the last xfer.\n");
			msd->next_xfer = list_entry(xfer->transfer_list.next,
					struct spi_transfer, transfer_list);
		}
		mtk_spi_disable_dma(msd);
		mtk_spi_setup_packet(msd, xfer);

		reinit_completion(&msd->xfer_completion);

		/*Using FIFO to send data*/
		if (mode == FIFO_TRANSFER || (mode == OTHER2)) {
			cnt = (xfer->len%4)?(xfer->len/4 + 1):(xfer->len/4);
			for (i = 0; i < cnt; i++) {
				writel(*((u32 *)xfer->tx_buf + i),
					regs + SPI_TX_DATA_REG);
				dev_dbg(&msg->spi->dev, "tx_buf data is:%x\n",
					*((u32 *)xfer->tx_buf + i));
				dev_dbg(&msg->spi->dev, "tx_buf addr is:%p\n",
					(u32 *)xfer->tx_buf + i);
			}
		}
		/*Using DMA to send data*/
		if ((mode == DMA_TRANSFER) || (mode == OTHER1)
			|| (mode == OTHER2))
			mtk_spi_dma_transfer(msd, xfer, mode);

		if (msd->running_status == PAUSED) {
			dev_info(&msd->pdev->dev, "Pause status to resume.\n");
			mtk_spi_resume_transfer(msd);
		} else if (msd->running_status == IDLE) {
			dev_info(&msd->pdev->dev, "The xfer start\n");
			if (is_pause_mode(msg) && !is_last_xfer(msg, xfer)) {
				dev_info(&msd->pdev->dev, "Set pause mode.\n");
				mtk_set_pause_bit(msd);
			}
			mtk_spi_start_transfer(msd);
		} else {
			msd->running_status = INPROGRESS;
			/*Exit pause mode*/
			if (is_pause_mode(msg) && is_last_xfer(msg, xfer))
				mtk_clear_resume_bit(msd);
		}

#ifdef SPI_LOOP_TEST
		mtk_spi_reg_info(msd);
		cnt = (xfer->len%4)?(xfer->len/4 + 1):(xfer->len/4);
		if (is_interrupt_enable(msd)) {
			if (is_fifo_read(msg) && xfer->rx_buf) {
				for (i = 0; i < cnt; i++) {
					/*get the data from rx*/
					reg_val = readl(regs + SPI_RX_DATA_REG);
					dev_info(&msg->spi->dev,
					"SPI_RX_DATA_REG:0x%x", reg_val);
					*((u32 *)xfer->rx_buf + i) = reg_val;
				}
			}
		}
#endif
		wait_for_completion_timeout(&msd->xfer_completion,
			SPI_DMA_TIMEOUT);
		msg->actual_length += xfer->len;
		dev_info(&msd->pdev->dev,
			"msg->actual_length is %d\n", msg->actual_length);
	}

	msg->status = msd->running_status;
	spi_finalize_current_message(spidev->master);

	return 0;

fail:
	list_for_each_entry(xfer, &msg->transfers, transfer_list) {
		if ((!msg->is_dma_mapped))
			mtk_transfer_dma_unmapping(msd, xfer);
	}
	msg->status = msd->running_status;
	spi_finalize_current_message(spidev->master);

	return -1;
}

static irqreturn_t mtk_spi_interrupt(int irq, void *dev_id)
{
	struct mtk_spi_driver_data *msd = (struct mtk_spi_driver_data *)dev_id;
	unsigned long flags;
	u32 reg_val;

	spin_lock_irqsave(&msd->lock, flags);
	reg_val = readl(msd->regs + SPI_STATUS0_REG);
	dev_info(&msd->pdev->dev, "interrupt status:%x\n", reg_val & 0x3);
	complete(&msd->xfer_completion);
	spin_unlock_irqrestore(&msd->lock, flags);
	return IRQ_HANDLED;
}


static int mtk_spi_setup(struct spi_device *spidev)
{
	struct spi_master *master;
	struct mtk_spi_driver_data *msd;
	struct mt_chip_conf *chip_config = NULL;

	master = spidev->master;
	msd = spi_master_get_devdata(master);

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
		chip_config = kzalloc(sizeof(struct mt_chip_conf), GFP_KERNEL);
		if (!chip_config) {
			dev_err(&spidev->dev,
				" spidev %s: can not get enough memory.\n",
				dev_name(&spidev->dev));
			return -ENOMEM;
		}
		dev_info(&msd->pdev->dev, "device %s: set default at chip's runtime state\n",
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

	dev_dbg(&spidev->dev, "set up chip config,mode:%d\n",
				chip_config->com_mod);

	/*check chip configuration valid*/
	msd->config = chip_config;
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
	struct mtk_spi_driver_data *msd;

	master = spidev->master;
	msd = spi_master_get_devdata(master);

	dev_info(&msd->pdev->dev, "Calling mt_spi_cleanup.\n");

	spidev->controller_data = NULL;
	spidev->master = NULL;

}

static int mt_spi_probe(struct platform_device *pdev)
{
	int	ret = 0;
	int	irq;
	struct clk *clk;
	struct spi_master *master;
	struct mtk_spi_driver_data *msd;
	void __iomem *spi_base;

	spi_base = of_io_request_and_map(pdev->dev.of_node, 0,
					(char *)pdev->name);
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

	master = spi_alloc_master(&pdev->dev,
					sizeof(struct mtk_spi_driver_data));
	if (!master) {
		dev_err(&pdev->dev, "device %s: alloc spi master fail.\n",
						dev_name(&pdev->dev));
		goto out;
	}
	/*Hardware can only connect 1 slave.
	If you want to multpile, using gpio CS*/
	master->num_chipselect = 1;
	master->mode_bits = (SPI_CPOL | SPI_CPHA);
	master->bus_num = pdev->id;
	master->dev.of_node = pdev->dev.of_node;
	master->setup = mtk_spi_setup;
	master->transfer_one_message = mtk_spi_transfer_one_message;
	master->cleanup = mt_spi_cleanup;

	platform_set_drvdata(pdev, master);

	msd = spi_master_get_devdata(master);
	msd->regs = spi_base;
	msd->pdev = pdev;
	msd->irq = irq;
	msd->clk = clk;
	msd->running_status = IDLE;
	msd->next_xfer = NULL;

	spin_lock_init(&msd->lock);
	init_completion(&msd->xfer_completion);
	dev_dbg(&pdev->dev, "Controller at 0x%p (irq %d)\n", msd->regs, irq);
	ret = devm_request_irq(&pdev->dev, irq, mtk_spi_interrupt,
		IRQF_TRIGGER_NONE, dev_name(&pdev->dev), msd);

	if (ret) {
		dev_err(&pdev->dev, "registering interrupt handler fails.\n");
		goto out;
	}

	spi_master_set_devdata(master, msd);
	mtk_spi_reset(msd);
	clk_prepare_enable(clk);

	ret = spi_register_master(master);
	if (ret) {
		dev_err(&pdev->dev, "spi_register_master fails.\n");
		goto out_free;
	}

out_free:
	free_irq(irq, msd);
out:
	spi_master_put(master);
	return ret;
}

static int __exit mt_spi_remove(struct platform_device *pdev)
{
	struct mtk_spi_driver_data *msd;
	struct spi_message *msg;
	struct spi_master *master = platform_get_drvdata(pdev);
	if (!master) {
		dev_err(&pdev->dev,
			"master %s: is invalid.\n",
			dev_name(&pdev->dev));
		return -EINVAL;
	}
	msd = spi_master_get_devdata(master);

	list_for_each_entry(msg, &msd->queue, queue) {
		msg->status = -ESHUTDOWN;
		msg->complete(msg->context);
	}

	msd->running_status = IDLE;

	mtk_spi_reset(msd);
	free_irq(msd->irq, master);
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
