/*
* Copyright (c) 2015 MediaTek Inc.
* Author: Leilk Liu <leilk.liu@mediatek.com>
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
#include <linux/sched.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/kernel.h>
#include <linux/spi/spi_bitbang.h>
#include <linux/gpio.h>
#include <linux/module.h>
#include <linux/of_gpio.h>

#define SPI_CFG0_REG                      0x0000
#define SPI_CFG1_REG                      0x0004
#define SPI_TX_SRC_REG                    0x0008
#define SPI_RX_DST_REG                    0x000c
#define SPI_CMD_REG                       0x0018
#define SPI_STATUS0_REG                   0x001c
#define SPI_PAD_SEL_REG                   0x0024

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

#define SPI_PAD1_MASK					  0x1

#define COMPAT_MT8135			(0x1 << 0)
#define COMPAT_MT8173			(0x1 << 1)

#define IDLE 0
#define INPROGRESS 1
#define PAUSED 2

#define PACKET_SIZE 1024

struct mtk_chip_config {
	u32 setuptime;
	u32 holdtime;
	u32 high_time;
	u32 low_time;
	u32 cs_idletime;
	u32 tx_mlsb;
	u32 rx_mlsb;
	u32 tx_endian;
	u32 rx_endian;
	u32 pause;
	u32 finish_intr;
	u32 deassert;
	u32 tckdly;
};

struct mtk_spi_ddata {
	struct spi_bitbang bitbang;
	void __iomem *base;
	u32 irq;
	u32 state;
	u32 platform_compat;
	struct clk *clk;

	const u8 *tx_buf;
	u8 *rx_buf;
	u32 tx_len, rx_len;
	struct completion done;
};

/*
 * A piece of default chip info unless the platform
 * supplies it.
 */
static const struct mtk_chip_config mtk_default_chip_info = {
	.setuptime = 10,
	.holdtime = 12,
	.high_time = 6,
	.low_time = 6,
	.cs_idletime = 12,
	.rx_mlsb = 1,
	.tx_mlsb = 1,
	.tx_endian = 0,
	.rx_endian = 0,
	.pause = 0,
	.finish_intr = 1,
	.deassert = 0,
	.tckdly = 0,
};

static const struct of_device_id mtk_spi_of_match[] = {
	{ .compatible = "mediatek,mt8135-spi", .data = (void *)COMPAT_MT8135},
	{ .compatible = "mediatek,mt8173-spi", .data = (void *)COMPAT_MT8173},
	{}
};
MODULE_DEVICE_TABLE(of, mtk_spi_of_match);

static void mtk_spi_reset(struct mtk_spi_ddata *mdata)
{
	u32 reg_val;

	/*set the software reset bit in SPI_CMD_REG.*/
	reg_val = readl(mdata->base + SPI_CMD_REG);
	reg_val &= ~SPI_CMD_RST_MASK;
	reg_val |= 1 << SPI_CMD_RST_OFFSET;
	writel(reg_val, mdata->base + SPI_CMD_REG);
	reg_val = readl(mdata->base + SPI_CMD_REG);
	reg_val &= ~SPI_CMD_RST_MASK;
	writel(reg_val, mdata->base + SPI_CMD_REG);
}

static void mtk_set_pause_bit(struct mtk_spi_ddata *mdata)
{
	u32 reg_val;

	reg_val = readl(mdata->base + SPI_CMD_REG);
	reg_val |= 1 << SPI_CMD_PAUSE_EN_OFFSET;
	reg_val |= 1 << SPI_CMD_PAUSE_IE_OFFSET;
	writel(reg_val, mdata->base + SPI_CMD_REG);
}

static void mtk_clear_pause_bit(struct mtk_spi_ddata *mdata)
{
	u32 reg_val;

	reg_val = readl(mdata->base + SPI_CMD_REG);
	reg_val &= ~SPI_CMD_PAUSE_EN_MASK;
	writel(reg_val, mdata->base + SPI_CMD_REG);
}

static int mtk_spi_config(struct mtk_spi_ddata *mdata,
					struct mtk_chip_config *chip_config)
{
	u32 reg_val;

	/* set the timing */
	reg_val = readl(mdata->base + SPI_CFG0_REG);
	reg_val &= ~(SPI_CFG0_SCK_HIGH_MASK | SPI_CFG0_SCK_LOW_MASK);
	reg_val &= ~(SPI_CFG0_CS_HOLD_MASK | SPI_CFG0_CS_SETUP_MASK);
	reg_val |= ((chip_config->high_time - 1) << SPI_CFG0_SCK_HIGH_OFFSET);
	reg_val |= ((chip_config->low_time - 1) << SPI_CFG0_SCK_LOW_OFFSET);
	reg_val |= ((chip_config->holdtime - 1) << SPI_CFG0_CS_HOLD_OFFSET);
	reg_val |= ((chip_config->setuptime - 1) << SPI_CFG0_CS_SETUP_OFFSET);
	writel(reg_val, mdata->base + SPI_CFG0_REG);

	reg_val = readl(mdata->base + SPI_CFG1_REG);
	reg_val &= ~SPI_CFG1_CS_IDLE_MASK;
	reg_val |= ((chip_config->cs_idletime - 1) << SPI_CFG1_CS_IDLE_OFFSET);
	reg_val &= ~SPI_CFG1_GET_TICK_DLY_MASK;
	reg_val |= ((chip_config->tckdly) << SPI_CFG1_GET_TICK_DLY_OFFSET);
	writel(reg_val, mdata->base + SPI_CFG1_REG);

	/* set the mlsbx and mlsbtx */
	reg_val = readl(mdata->base + SPI_CMD_REG);
	reg_val &= ~(SPI_CMD_TX_ENDIAN_MASK | SPI_CMD_RX_ENDIAN_MASK);
	reg_val &= ~(SPI_CMD_TXMSBF_MASK | SPI_CMD_RXMSBF_MASK);
	reg_val |= (chip_config->tx_mlsb << SPI_CMD_TXMSBF_OFFSET);
	reg_val |= (chip_config->rx_mlsb << SPI_CMD_RXMSBF_OFFSET);
	reg_val |= (chip_config->tx_endian << SPI_CMD_TX_ENDIAN_OFFSET);
	reg_val |= (chip_config->rx_endian << SPI_CMD_RX_ENDIAN_OFFSET);
	writel(reg_val, mdata->base + SPI_CMD_REG);

	/* set finish and pause interrupt always enable */
	reg_val = readl(mdata->base + SPI_CMD_REG);
	reg_val &= ~SPI_CMD_FINISH_IE_MASK;
	reg_val |= (chip_config->finish_intr << SPI_CMD_FINISH_IE_OFFSET);
	writel(reg_val, mdata->base + SPI_CMD_REG);

	reg_val = readl(mdata->base + SPI_CMD_REG);
	reg_val |= 1 << SPI_CMD_TX_DMA_OFFSET;
	reg_val |= 1 << SPI_CMD_RX_DMA_OFFSET;
	writel(reg_val, mdata->base + SPI_CMD_REG);

	/* set deassert mode */
	reg_val = readl(mdata->base + SPI_CMD_REG);
	reg_val &= ~SPI_CMD_DEASSERT_MASK;
	reg_val |= (chip_config->deassert << SPI_CMD_DEASSERT_OFFSET);
	writel(reg_val, mdata->base + SPI_CMD_REG);

	/* pad select */
	if (mdata->platform_compat & COMPAT_MT8173)
		writel(SPI_PAD1_MASK, mdata->base + SPI_PAD_SEL_REG);

	return 0;
}

static int mtk_spi_setup_transfer(struct spi_device *spi, struct spi_transfer *t)
{
	u32 reg_val;
	struct spi_master *master = spi->master;
	struct mtk_spi_ddata *mdata = spi_master_get_devdata(master);
	struct spi_message *m = master->cur_msg;
	struct mtk_chip_config *chip_config;

	u8 cpha	= spi->mode & SPI_CPHA ? 1 : 0;
	u8 cpol = spi->mode & SPI_CPOL ? 1 : 0;

	reg_val = readl(mdata->base + SPI_CMD_REG);
	reg_val &= ~(SPI_CMD_CPHA_MASK | SPI_CMD_CPOL_MASK);
	reg_val |= (cpha << SPI_CMD_CPHA_OFFSET);
	reg_val |= (cpol << SPI_CMD_CPOL_OFFSET);
	writel(reg_val, mdata->base + SPI_CMD_REG);

	if (t->cs_change) {
		if (!(list_is_last(&t->transfer_list, &m->transfers)))
			mdata->state = IDLE;
	} else {
		mdata->state = IDLE;
		mtk_spi_reset(mdata);
	}

	chip_config = (struct mtk_chip_config *)spi->controller_data;
	if (!chip_config) {
		chip_config = (void *)&mtk_default_chip_info;
		spi->controller_data = chip_config;
		mdata->state = IDLE;
	}

	mtk_spi_config(mdata, chip_config);

	return 0;
}

static void mtk_spi_chipselect(struct spi_device *spi, int is_on)
{
	struct mtk_spi_ddata *mdata = spi_master_get_devdata(spi->master);

	switch (is_on) {
	case BITBANG_CS_ACTIVE:
		mtk_set_pause_bit(mdata);
		break;
	case BITBANG_CS_INACTIVE:
		mtk_clear_pause_bit(mdata);
		break;
	}
}

static void mtk_spi_start_transfer(struct mtk_spi_ddata *mdata)
{
	u32 reg_val;

	reg_val = readl(mdata->base + SPI_CMD_REG);
	reg_val |= 1 << SPI_CMD_ACT_OFFSET;
	writel(reg_val, mdata->base + SPI_CMD_REG);
}

static void mtk_spi_resume_transfer(struct mtk_spi_ddata *mdata)
{
	u32 reg_val;

	reg_val = readl(mdata->base + SPI_CMD_REG);
	reg_val &= ~SPI_CMD_RESUME_MASK;
	reg_val |= 1 << SPI_CMD_RESUME_OFFSET;
	writel(reg_val, mdata->base + SPI_CMD_REG);
}

static int mtk_spi_setup_packet(struct mtk_spi_ddata *mdata,
				struct spi_transfer *xfer)
{
	struct device *dev = &mdata->bitbang.master->dev;
	u32 packet_size, packet_loop, reg_val;

	packet_size = min_t(unsigned, xfer->len, PACKET_SIZE);
	if (xfer->len % packet_size) {
		dev_err(dev, "ERROR!The lens must be a multiple of %d, your len %d\n",
				PACKET_SIZE, xfer->len);
		return -EINVAL;
	}

	packet_loop = xfer->len / packet_size;

	reg_val = readl(mdata->base + SPI_CFG1_REG);
	reg_val &= ~(SPI_CFG1_PACKET_LENGTH_MASK + SPI_CFG1_PACKET_LOOP_MASK);
	reg_val |= (packet_size - 1) << SPI_CFG1_PACKET_LENGTH_OFFSET;
	reg_val |= (packet_loop - 1) << SPI_CFG1_PACKET_LOOP_OFFSET;
	writel(reg_val, mdata->base + SPI_CFG1_REG);

	return 0;
}

static int mtk_spi_txrx_bufs(struct spi_device *spi, struct spi_transfer *xfer)
{
	struct spi_master *master = spi->master;
	struct mtk_spi_ddata *mdata = spi_master_get_devdata(master);
	struct device *dev = &mdata->bitbang.master->dev;
	int cmd, ret;

	/* mtk spi hw tx/rx have 4bytes aligned restriction,
	  * so kmalloc tx/rx buffer to workaround here.
	  */
	mdata->tx_buf = mdata->rx_buf = NULL;
	if (xfer->tx_buf) {
		mdata->tx_buf = kmalloc(xfer->len, GFP_KERNEL);
		if (!mdata->tx_buf) {
			dev_err(dev, "malloc tx_buf failed.\n");
			ret = -ENOMEM;
			goto err_free;
		}
		memcpy((void *)mdata->tx_buf, xfer->tx_buf, xfer->len);
	}
	if (xfer->rx_buf) {
		mdata->rx_buf = kmalloc(xfer->len, GFP_KERNEL);
		if (!mdata->rx_buf) {
			dev_err(dev, "malloc rx_buf failed.\n");
			ret = -ENOMEM;
			goto err_free;
		}
	}

	reinit_completion(&mdata->done);

	xfer->tx_dma = xfer->rx_dma = DMA_ERROR_CODE;
	if (xfer->tx_buf) {
		xfer->tx_dma = dma_map_single(dev, (void *)mdata->tx_buf,
					      xfer->len, DMA_TO_DEVICE);
		if (dma_mapping_error(dev, xfer->tx_dma)) {
			dev_err(dev, "dma mapping tx_buf error.\n");
			ret = -ENOMEM;
			goto err_free;
		}
	}
	if (xfer->rx_buf) {
		xfer->rx_dma = dma_map_single(dev, mdata->rx_buf,
					      xfer->len, DMA_FROM_DEVICE);
		if (dma_mapping_error(dev, xfer->rx_dma)) {
			if (xfer->tx_buf)
				dma_unmap_single(dev, xfer->tx_dma,
						 xfer->len, DMA_TO_DEVICE);
			dev_err(dev, "dma mapping rx_buf error.\n");
			ret = -ENOMEM;
			goto err_free;
		}
	}

	ret = mtk_spi_setup_packet(mdata, xfer);
	if (ret != 0)
		goto err_free;

	/* workaround mt8173 HW issue, so enable TX dma for RX */
	cmd = readl(mdata->base + SPI_CMD_REG);
	if (xfer->tx_buf || (mdata->platform_compat & COMPAT_MT8173))
		cmd |= 1 << SPI_CMD_TX_DMA_OFFSET;
	if (xfer->rx_buf)
		cmd |= 1 << SPI_CMD_RX_DMA_OFFSET;
	writel(cmd, mdata->base + SPI_CMD_REG);

	/* set up the DMA bus address */
	if (xfer->tx_dma != DMA_ERROR_CODE)
		writel(cpu_to_le32(xfer->tx_dma), mdata->base + SPI_TX_SRC_REG);
	if (xfer->rx_dma != DMA_ERROR_CODE)
		writel(cpu_to_le32(xfer->rx_dma), mdata->base + SPI_RX_DST_REG);

	if (mdata->state == IDLE)
		mtk_spi_start_transfer(mdata);
	else if (mdata->state == PAUSED)
		mtk_spi_resume_transfer(mdata);
	else
		mdata->state = INPROGRESS;

	wait_for_completion(&mdata->done);

	if (xfer->tx_dma != DMA_ERROR_CODE) {
		dma_unmap_single(dev, xfer->tx_dma, xfer->len, DMA_TO_DEVICE);
		xfer->tx_dma = DMA_ERROR_CODE;
	}
	if (xfer->rx_dma != DMA_ERROR_CODE) {
		dma_unmap_single(dev, xfer->rx_dma, xfer->len, DMA_FROM_DEVICE);
		xfer->rx_dma = DMA_ERROR_CODE;
	}

	/* spi disable dma */
	cmd = readl(mdata->base + SPI_CMD_REG);
	cmd &= ~SPI_CMD_TX_DMA_MASK;
	cmd &= ~SPI_CMD_RX_DMA_MASK;
	writel(cmd, mdata->base + SPI_CMD_REG);

	if (xfer->rx_buf)
		memcpy(xfer->rx_buf, mdata->rx_buf, xfer->len);

	ret = xfer->len;

err_free:
	kfree(mdata->tx_buf);
	kfree(mdata->rx_buf);
	return ret;
}

static irqreturn_t mtk_spi_interrupt(int irq, void *dev_id)
{
	struct mtk_spi_ddata *mdata = dev_id;
	u32 reg_val;

	reg_val = readl(mdata->base + SPI_STATUS0_REG);
	if (reg_val & 0x2)
		mdata->state = PAUSED;
	else
		mdata->state = IDLE;
	complete(&mdata->done);

	return IRQ_HANDLED;
}

static unsigned long mtk_get_device_prop(struct platform_device *pdev)
{
	const struct of_device_id *match;

	match = of_match_node(mtk_spi_of_match, pdev->dev.of_node);
	return (unsigned long)match->data;
}

static int mtk_spi_probe(struct platform_device *pdev)
{
	struct spi_master *master;
	struct mtk_spi_ddata *mdata;
	struct resource *res;
	int	ret;

	master = spi_alloc_master(&pdev->dev, sizeof(struct mtk_spi_ddata));
	if (!master) {
		dev_err(&pdev->dev, "failed to alloc spi master\n");
		return -ENOMEM;
	}

	platform_set_drvdata(pdev, master);

	master->dev.of_node = pdev->dev.of_node;
	master->bus_num = pdev->id;
	master->num_chipselect = 1;
	master->mode_bits = SPI_CPOL | SPI_CPHA;

	mdata = spi_master_get_devdata(master);

	mdata->bitbang.master = master;
	mdata->bitbang.chipselect = mtk_spi_chipselect;
	mdata->bitbang.setup_transfer = mtk_spi_setup_transfer;
	mdata->bitbang.txrx_bufs = mtk_spi_txrx_bufs;
	mdata->platform_compat = mtk_get_device_prop(pdev);

	init_completion(&mdata->done);

	mdata->clk = devm_clk_get(&pdev->dev, "main");
	if (IS_ERR(mdata->clk)) {
		ret = PTR_ERR(mdata->clk);
		dev_err(&pdev->dev, "failed to get clock: %d\n", ret);
		goto err;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		ret = -ENODEV;
		dev_err(&pdev->dev, "failed to determine base address\n");
		goto err;
	}

	mdata->base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(mdata->base)) {
		ret = PTR_ERR(mdata->base);
		goto err;
	}

	ret = platform_get_irq(pdev, 0);
	if (ret < 0) {
		dev_err(&pdev->dev, "failed to get irq (%d)\n", ret);
		goto err;
	}

	mdata->irq = ret;

	if (!pdev->dev.dma_mask)
		pdev->dev.dma_mask = &pdev->dev.coherent_dma_mask;

	mdata->bitbang.master->dev.dma_mask = pdev->dev.dma_mask;

	ret = clk_prepare_enable(mdata->clk);
	if (ret < 0) {
		dev_err(&pdev->dev, "failed to enable clock (%d)\n", ret);
		goto err;
	}

	ret = devm_request_irq(&pdev->dev, mdata->irq, mtk_spi_interrupt,
		IRQF_TRIGGER_NONE, dev_name(&pdev->dev), mdata);
	if (ret) {
		dev_err(&pdev->dev, "failed to register irq (%d)\n", ret);
		goto err_disable_clk;
	}

	ret = spi_bitbang_start(&mdata->bitbang);
	if (ret) {
		dev_err(&pdev->dev, "spi_bitbang_start failed (%d)\n", ret);
err_disable_clk:
		clk_disable_unprepare(mdata->clk);
err:
		spi_master_put(master);
	}

	return ret;
}

static int mtk_spi_remove(struct platform_device *pdev)
{
	struct spi_master *master = platform_get_drvdata(pdev);
	struct mtk_spi_ddata *mdata = spi_master_get_devdata(master);

	spi_bitbang_stop(&mdata->bitbang);
	mtk_spi_reset(mdata);
	clk_disable_unprepare(mdata->clk);
	spi_master_put(master);

	return 0;
}

struct platform_driver mtk_spi_driver = {
	.driver = {
		.name = "mtk-spi",
		.of_match_table = mtk_spi_of_match,
	},
	.probe = mtk_spi_probe,
	.remove = mtk_spi_remove,
};

module_platform_driver(mtk_spi_driver);

MODULE_DESCRIPTION("MTK SPI Controller driver");
MODULE_AUTHOR("Leilk Liu <leilk.liu@mediatek.com>");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform: mtk_spi");
