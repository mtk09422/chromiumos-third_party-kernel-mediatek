/*
 * mt_afe_control.c  --  Mediatek AFE controller
 *
 * Copyright (c) 2014 MediaTek Inc.
 * Author: Koro Chen <koro.chen@mediatek.com>
 *         Hidalgo Huang <hidalgo.huang@mediatek.com>
 *         Ir Lian <ir.lian@mediatek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include "mt_afe_common.h"
#include "mt_afe_clk.h"
#include "mt_afe_control.h"
#include "mt_afe_connection.h"
#include <linux/interrupt.h>
#include <linux/spinlock.h>
#include <linux/dma-mapping.h>
#include <linux/gpio.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/clk.h>

static DEFINE_SPINLOCK(mt_afe_control_lock);

void mt_afe_set_reg(struct mt_afe_info *afe_info, uint32_t offset,
		    uint32_t value, uint32_t mask)
{
	const uint32_t *address = (uint32_t *)((char *)afe_info->base_addr
								+ offset);
	uint32_t val_tmp;

	val_tmp = readl(address);
	val_tmp &= (~mask);
	val_tmp |= (value & mask);
	writel(val_tmp, IOMEM(address));
}

static void mt_afe_dl_interrupt_handler(struct mt_afe_info *afe_info)
{
	/* irq1 ISR handler */
	signed int afe_consumed_bytes;
	signed int hw_memory_index;
	signed int hw_cur_read_idx = 0;
	struct mt_afe_block * const afe_block =
				&(afe_info->memif[MT_AFE_PIN_DL1].block);

	PRINTK_AUDDEEPDRV("%s\n", __func__);
	hw_cur_read_idx = mt_afe_get_reg(afe_info, AFE_DL1_CUR);

	if (hw_cur_read_idx == 0) {
		dev_notice(afe_info->dev, "%s hw_cur_read_idx ==0\n", __func__);
		hw_cur_read_idx = afe_block->phys_buf_addr;
	}

	hw_memory_index = (hw_cur_read_idx - afe_block->phys_buf_addr);


	PRINTK_AUDDEEPDRV("%s ReadIdx=0x%x memidx = 0x%x PhysBufAddr = 0x%x\n",
			  __func__, hw_cur_read_idx, hw_memory_index,
			  afe_block->phys_buf_addr);


	/* get hw consume bytes */
	if (hw_memory_index > afe_block->dma_read_idx) {
		afe_consumed_bytes = hw_memory_index - afe_block->dma_read_idx;
	} else {
		afe_consumed_bytes = afe_block->buffer_size +
				hw_memory_index - afe_block->dma_read_idx;
	}

	if ((afe_consumed_bytes & 0x03) != 0)
		dev_warn(afe_info->dev,
			 "%s DMA address is not aligned 32 bytes\n", __func__);

	afe_block->dma_read_idx += afe_consumed_bytes;
	afe_block->dma_read_idx %= afe_block->buffer_size;

	PRINTK_AUDDEEPDRV("%s before snd_pcm_period_elapsed\n", __func__);
	snd_pcm_period_elapsed(afe_info->sub_stream[MT_AFE_PIN_DL1]);
	PRINTK_AUDDEEPDRV("%s after snd_pcm_period_elapsed\n", __func__);
}

static void mt_afe_handle_mem_context(struct mt_afe_info *afe_info,
				      struct mt_afe_memif *mem_block)
{
	uint32_t hw_cur_read_idx = 0;
	signed int hw_get_bytes = 0;
	struct mt_afe_block *block = NULL;

	if (!mem_block)
		return;

	switch (mem_block->memif_num) {
	case MT_AFE_PIN_VUL:
		hw_cur_read_idx = mt_afe_get_reg(afe_info, AFE_VUL_CUR);
		break;
	case MT_AFE_PIN_DAI:
		hw_cur_read_idx = mt_afe_get_reg(afe_info, AFE_DAI_CUR);
		break;
	case MT_AFE_PIN_AWB:
		hw_cur_read_idx = mt_afe_get_reg(afe_info, AFE_AWB_CUR);
		break;
	case MT_AFE_PIN_MOD_DAI:
		hw_cur_read_idx = mt_afe_get_reg(afe_info, AFE_MOD_PCM_CUR);
		break;
	}

	block = &mem_block->block;

	if (hw_cur_read_idx == 0) {
		dev_warn(afe_info->dev, "%s hw_cur_read_idx = 0\n", __func__);
		return;
	}
	if (block->virt_buf_addr == NULL)
		return;

	/* HW already fill in */
	hw_get_bytes = (hw_cur_read_idx - block->phys_buf_addr) -
			block->write_idx;

	if (hw_get_bytes < 0)
		hw_get_bytes += block->buffer_size;

	PRINTK_AUDDEEPDRV("%s write_idx:%x get_bytes:%x cur_read_idx:%x\n",
			  __func__,
			  block->write_idx, hw_get_bytes, hw_cur_read_idx);

	block->write_idx += hw_get_bytes;
	block->write_idx &= block->buffer_size - 1;

	snd_pcm_period_elapsed(afe_info->sub_stream[mem_block->memif_num]);
}

static void mt_afe_ul_interrupt_handler(struct mt_afe_info *afe_info)
{
	/* irq2 ISR handler */
	const uint32_t afe_dac_con0 = mt_afe_get_reg(afe_info, AFE_DAC_CON0);
	struct mt_afe_memif *mem_block = NULL;

	if (afe_dac_con0 & 0x8) {
		/* handle VUL Context */
		mem_block = &afe_info->memif[MT_AFE_PIN_VUL];
		mt_afe_handle_mem_context(afe_info, mem_block);
	}
	if (afe_dac_con0 & 0x10) {
		/* handle DAI Context */
		mem_block = &afe_info->memif[MT_AFE_PIN_DAI];
		mt_afe_handle_mem_context(afe_info, mem_block);
	}
	if (afe_dac_con0 & 0x40) {
		/* handle AWB Context */
		mem_block = &afe_info->memif[MT_AFE_PIN_AWB];
		mt_afe_handle_mem_context(afe_info, mem_block);
	}
}

/*upstream can we merge with dl1*/
static void mt_afe_hdmi_interrupt_handler(struct mt_afe_info *afe_info)
{
	/* irq1 ISR handler */
	signed int afe_consumed_bytes;
	signed int hw_memory_index;
	signed int hw_cur_read_idx = 0;
	struct mt_afe_block * const afe_block =
				&(afe_info->memif[MT_AFE_PIN_HDMI].block);

	PRINTK_AUDDEEPDRV("%s\n", __func__);
	hw_cur_read_idx = mt_afe_get_reg(afe_info, AFE_HDMI_OUT_CUR);

	if (hw_cur_read_idx == 0) {
		dev_notice(afe_info->dev, "%s hw_cur_read_idx ==0\n", __func__);
		hw_cur_read_idx = afe_block->phys_buf_addr;
	}

	hw_memory_index = (hw_cur_read_idx - afe_block->phys_buf_addr);


	PRINTK_AUDDEEPDRV("%s ReadIdx=0x%x memidx = 0x%x PhysBufAddr = 0x%x\n",
			  __func__, hw_cur_read_idx, hw_memory_index,
			  afe_block->phys_buf_addr);


	/* get hw consume bytes */
	if (hw_memory_index > afe_block->dma_read_idx) {
		afe_consumed_bytes = hw_memory_index - afe_block->dma_read_idx;
	} else {
		afe_consumed_bytes = afe_block->buffer_size +
				hw_memory_index - afe_block->dma_read_idx;
	}

	if ((afe_consumed_bytes & 0x03) != 0)
		dev_warn(afe_info->dev,
			 "%s DMA address is not aligned 32 bytes\n", __func__);

	afe_block->dma_read_idx += afe_consumed_bytes;
	afe_block->dma_read_idx %= afe_block->buffer_size;

	PRINTK_AUDDEEPDRV("%s before snd_pcm_period_elapsed\n", __func__);
	snd_pcm_period_elapsed(afe_info->sub_stream[MT_AFE_PIN_HDMI]);
	PRINTK_AUDDEEPDRV("%s after snd_pcm_period_elapsed\n", __func__);
}

irqreturn_t mt_afe_irq_handler(int irq, void *dev_id)
{
	struct mt_afe_info *afe_info = dev_id;
	const uint32_t reg_value = (mt_afe_get_reg(afe_info, AFE_IRQ_STATUS) &
				    IRQ_STATUS_BIT);

	PRINTK_AUDDEEPDRV("mt_afe_irq_handler AFE_IRQ_STATUS = %x\n",
			  reg_value);

	if (reg_value & MT_AFE_IRQ1_MCU)
		mt_afe_dl_interrupt_handler(afe_info);

	if (reg_value & MT_AFE_IRQ2_MCU)
		mt_afe_ul_interrupt_handler(afe_info);

	if (reg_value & MT_AFE_HDMI_IRQ)
		mt_afe_hdmi_interrupt_handler(afe_info);

	/* here is error handle , for interrupt is trigger but not status ,
		clear all interrupt with bit 6 */
	if (reg_value == 0) {
		dev_info(afe_info->dev, "%s reg_value == 0\n", __func__);
		mt_afe_clk_on(afe_info);
		mt_afe_set_reg(afe_info, AFE_IRQ_CLR, 1 << 6, 0xff);
		mt_afe_set_reg(afe_info, AFE_IRQ_CLR, 1, 0xff);
		mt_afe_set_reg(afe_info, AFE_IRQ_CLR, 1 << 1, 0xff);
		mt_afe_set_reg(afe_info, AFE_IRQ_CLR, 1 << 2, 0xff);
		mt_afe_set_reg(afe_info, AFE_IRQ_CLR, 1 << 3, 0xff);
		mt_afe_set_reg(afe_info, AFE_IRQ_CLR, 1 << 4, 0xff);
		mt_afe_set_reg(afe_info, AFE_IRQ_CLR, 1 << 5, 0xff);

		mt_afe_clk_off(afe_info);
		goto mt_afe_irq_handler_exit;
	}

	/* clear irq */
	mt_afe_set_reg(afe_info, AFE_IRQ_CLR, reg_value, 0xff);

mt_afe_irq_handler_exit:
	return IRQ_HANDLED;
}

int mt_afe_platform_init(struct platform_device *pdev)
{
	struct resource *res;
	struct device *dev = &pdev->dev;
	struct mt_afe_info *afe_info = NULL;
	unsigned int irq_id;
	int ret = 0;

	if (!dev->of_node) {
		dev_err(dev, "could not get device tree\n");
		return -EINVAL;
	}

	afe_info = devm_kzalloc(&pdev->dev, sizeof(*afe_info), GFP_KERNEL);
	if (!afe_info) {
		dev_err(afe_info->dev, "%s failed to allocate memory\n",
			__func__);
		return -ENOMEM;
	}
	afe_info->dev = dev;
	platform_set_drvdata(pdev, afe_info);

	/*get IRQ*/
	irq_id = platform_get_irq(pdev, 0);
	if (!irq_id) {
		dev_err(dev, "no irq for node %s\n", dev->of_node->name);
		return -ENXIO;
	}
	ret = devm_request_irq(dev, irq_id, mt_afe_irq_handler,
			       IRQF_TRIGGER_LOW, "Afe_ISR_Handle",
			       (void *)afe_info);
	if (ret) {
		dev_err(dev, "could not request_irq\n");
		return ret;
	}

	/*get SRAM base*/
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(&pdev->dev, "could not get SRAM base\n");
		return -ENXIO;
	}
	afe_info->afe_sram_phy_address = res->start;
	afe_info->afe_sram_size = resource_size(res);

	/*upstream to check: is non-cache guaranteed by devm_ioremap_resource*/
	afe_info->afe_sram_address = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(afe_info->afe_sram_address))
		return PTR_ERR(afe_info->afe_sram_address);

	/*get REG base*/
	res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	if (!res) {
		dev_err(&pdev->dev, "could not get REG base\n");
		return -ENXIO;
	}

	/*upstream to check: is non-cache guaranteed by devm_ioremap_resource*/
	afe_info->base_addr = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(afe_info->base_addr))
		return PTR_ERR(afe_info->base_addr);

	dev_info(afe_info->dev, "AudDrv IRQ = %d BASE = 0x%p SRAM = 0x%p size = %d\n",
		 irq_id, afe_info->base_addr, afe_info->afe_sram_address,
		 afe_info->afe_sram_size);

	ret = mt_afe_init_audio_clk(afe_info, dev);
	if (ret) {
		dev_err(afe_info->dev, "%s mt_afe_init_audio_clk fail %d\n",
			__func__, ret);
		return ret;
	}

	/* for PDN_AFE default 0, disable it. */
	/*mt_afe_clk_off(afe_info);  upstream why need?*/

	return ret;
}

int mt_afe_reset_buffer(struct mt_afe_info *afe_info, enum mt_afe_pin pin)
{
	struct mt_afe_block *afe_block;

	switch (pin) {
	case MT_AFE_PIN_DL1:
		afe_block = &(afe_info->memif[MT_AFE_PIN_DL1].block);
		break;
	case MT_AFE_PIN_VUL:
		afe_block = &(afe_info->memif[MT_AFE_PIN_VUL].block);
		break;
	case MT_AFE_PIN_DAI:
		afe_block = &(afe_info->memif[MT_AFE_PIN_DAI].block);
		break;
	default:
		return -EINVAL;
	}
	memset(afe_block->virt_buf_addr, 0, afe_block->buffer_size);
	afe_block->dma_read_idx = 0;
	afe_block->write_idx = 0;
	afe_block->data_remained = 0;
	return 0;
}


int mt_afe_update_hw_ptr(struct mt_afe_info *afe_info, enum mt_afe_pin pin,
			 struct snd_pcm_substream *substream)
{
	struct mt_afe_block *afe_block;
	int rc = 0;

	afe_block = &(afe_info->memif[pin].block);

	switch (pin) {
	case MT_AFE_PIN_DL1:
	case MT_AFE_PIN_HDMI:
		rc = bytes_to_frames(substream->runtime,
				     afe_block->dma_read_idx);
		break;
	case MT_AFE_PIN_VUL:
	case MT_AFE_PIN_DAI:
	case MT_AFE_PIN_AWB:
		rc = bytes_to_frames(substream->runtime, afe_block->write_idx);
	default:
		break;
	}
	return rc;
}

int mt_afe_allocate_dl1_buffer(struct mt_afe_info *afe_info,
			       struct snd_pcm_substream *substream)
{
	uint32_t phy_addr = 0;

	struct mt_afe_block *block = &afe_info->memif[MT_AFE_PIN_DL1].block;
	struct snd_dma_buffer *dma_buf = &substream->dma_buffer;

	afe_info->memif[MT_AFE_PIN_DL1].memif_num = MT_AFE_PIN_DL1;
	block->buffer_size = afe_info->afe_sram_size;

	dev_dbg(afe_info->dev, "AudDrv %s length = %d\n",
		__func__, afe_info->afe_sram_size);
	if (afe_info->afe_sram_size > MT_AFE_DL1_MAX_BUFFER_LENGTH)
		return -EINVAL;

	/* allocate memory */
	if (block->phys_buf_addr == 0) {
		/* todo there should be a sram manager to
			allocate memory for lowpower.powervr_device*/
		phy_addr = afe_info->afe_sram_phy_address;
		block->phys_buf_addr = phy_addr;

		dev_info(afe_info->dev, "AudDrv %s length = %d\n",
			 __func__, afe_info->afe_sram_size);
		block->virt_buf_addr =
			(unsigned char *)afe_info->afe_sram_address;
	}

	dev_dbg(afe_info->dev, "AudDrv %s pucVirtBufAddr = %p\n", __func__,
		block->virt_buf_addr);

	/* check 32 bytes align */
	if ((block->phys_buf_addr & 0x1f) != 0)
		dev_warn(afe_info->dev, "[Auddrv] %s is not aligned (0x%x)\n",
			 __func__, block->phys_buf_addr);

	block->sample_num_mask = 0x001f;	/* 32 byte align */
	block->write_idx = 0;
	block->dma_read_idx = 0;
	block->data_remained = 0;
	block->fsync_flag = false;
	block->reset_flag = true;

	/* set sram address top hardware */
	mt_afe_set_reg(afe_info, AFE_DL1_BASE, block->phys_buf_addr,
		       0xFFFFFFFF);
	mt_afe_set_reg(afe_info, AFE_DL1_END,
		       block->phys_buf_addr + (afe_info->afe_sram_size - 1),
		       0xFFFFFFFF);

	substream->runtime->dma_bytes = afe_info->afe_sram_size;
	substream->runtime->dma_area =
		(unsigned char *)afe_info->afe_sram_address;
	substream->runtime->dma_addr = afe_info->afe_sram_phy_address;
	dma_buf->dev.type = SNDRV_DMA_TYPE_DEV;
	dma_buf->dev.dev = substream->pcm->card->dev;
	dma_buf->private_data = NULL;

	return 0;
}

static bool mt_afe_get_memif_enable(struct mt_afe_info *afe_info)
{
	int i;

	for (i = 0; i < MT_AFE_PIN_NUM; i++) {
		if ((afe_info->afe_pin[i].state) == true)
			return true;
	}
	return false;
}

static bool mt_afe_get_ul_memif_enable(struct mt_afe_info *afe_info)
{
	int i;

	for (i = MT_AFE_PIN_VUL; i <= MT_AFE_PIN_MOD_DAI; i++) {
		if ((afe_info->afe_pin[i].state) == true)
			return true;
	}
	return false;
}

void mt_afe_enable_afe(struct mt_afe_info *afe_info, bool enable)
{
	unsigned long flags;
	bool mem_enable;

	spin_lock_irqsave(&mt_afe_control_lock, flags);
	mem_enable = mt_afe_get_memif_enable(afe_info);

	if (!enable && !mem_enable)
		mt_afe_set_reg(afe_info, AFE_DAC_CON0, 0x0, 0x1);
	else if (enable && mem_enable)
		mt_afe_set_reg(afe_info, AFE_DAC_CON0, 0x1, 0x1);

	spin_unlock_irqrestore(&mt_afe_control_lock, flags);
}

static uint32_t mt_afe_i2s_fs(uint32_t sample_rate)
{
	switch (sample_rate) {
	case 8000:
		return MT_AFE_I2S_RATE_8K;
	case 11025:
		return MT_AFE_I2S_RATE_11K;
	case 12000:
		return MT_AFE_I2S_RATE_12K;
	case 16000:
		return MT_AFE_I2S_RATE_16K;
	case 22050:
		return MT_AFE_I2S_RATE_22K;
	case 24000:
		return MT_AFE_I2S_RATE_24K;
	case 32000:
		return MT_AFE_I2S_RATE_32K;
	case 44100:
		return MT_AFE_I2S_RATE_44K;
	case 48000:
		return MT_AFE_I2S_RATE_48K;
	default:
		break;
	}
	return MT_AFE_I2S_RATE_44K;
}

int mt_afe_set_sample_rate(struct mt_afe_info *afe_info,
			   uint32_t aud_block, uint32_t sample_rate)
{
	dev_dbg(afe_info->dev, "%s aud_block = %d aud_block = %d\n", __func__,
		aud_block, aud_block);
	sample_rate = mt_afe_i2s_fs(sample_rate);

	switch (aud_block) {
	case MT_AFE_PIN_DL1:
		mt_afe_set_reg(afe_info, AFE_DAC_CON1,
			       sample_rate, 0x0000000f);
		break;
	case MT_AFE_PIN_DL2:
		mt_afe_set_reg(afe_info, AFE_DAC_CON1,
			       sample_rate << 4, 0x000000f0);
		break;
	case MT_AFE_PIN_I2S:
		mt_afe_set_reg(afe_info, AFE_DAC_CON1,
			       sample_rate << 8, 0x00000f00);
		break;
	case MT_AFE_PIN_AWB:
		mt_afe_set_reg(afe_info, AFE_DAC_CON1,
			       sample_rate << 12, 0x0000f000);
		break;
	case MT_AFE_PIN_VUL:
		mt_afe_set_reg(afe_info, AFE_DAC_CON1,
			       sample_rate << 16, 0x000f0000);
		break;
	case MT_AFE_PIN_DAI:
		if (sample_rate == MT_AFE_I2S_RATE_8K)
			mt_afe_set_reg(afe_info, AFE_DAC_CON1,
				       0 << 20, 1 << 20);
		else if (sample_rate == MT_AFE_I2S_RATE_16K)
			mt_afe_set_reg(afe_info, AFE_DAC_CON1,
				       1 << 20, 1 << 20);
		else
			return -EINVAL;
		break;
	case MT_AFE_PIN_MOD_DAI:
		if (sample_rate == MT_AFE_I2S_RATE_8K)
			mt_afe_set_reg(afe_info, AFE_DAC_CON1,
				       0 << 30, 1 << 30);
		else if (sample_rate == MT_AFE_I2S_RATE_16K)
			mt_afe_set_reg(afe_info, AFE_DAC_CON1,
				       1 << 30, 1 << 30);
		else
			return -EINVAL;
		break;
	}
	return 0;
}

int mt_afe_set_ch(struct mt_afe_info *afe_info,
		  uint32_t memory_interface, uint32_t channel)
{
	switch (memory_interface) {
	case MT_AFE_PIN_AWB:
		mt_afe_set_reg(afe_info, AFE_DAC_CON1, channel << 24, 1 << 24);
		break;
	case MT_AFE_PIN_VUL:
		mt_afe_set_reg(afe_info, AFE_DAC_CON1, channel << 27, 1 << 27);
		break;
	default:
		dev_info(afe_info->dev, "mt_afe_set_ch  memory_interface = %d, channel = %d\n",
			 memory_interface, channel);
		return -EINVAL;
	}
	return 0;
}

void mt_afe_set_2nd_i2s(struct mt_afe_info *afe_info, uint32_t samplerate)
{
	uint32_t audio_i2s = 0;

	/* set 2nd samplerate to AFE_ADC_CON1 */
	mt_afe_set_sample_rate(afe_info, MT_AFE_PIN_I2S, samplerate);
	audio_i2s |= (MT_AFE_INV_LRCK_NO << 5);
	audio_i2s |= (MT_AFE_I2S_DIR_IN << 4); /* set to in for in+out*/
	audio_i2s |= (MT_AFE_I2S_FORMAT_I2S << 3);
	audio_i2s |= (MT_AFE_I2S_SRC_MASTER << 2);
	audio_i2s |= (MT_AFE_I2S_WLEN_16BITS << 1);
	mt_afe_set_reg(afe_info, AFE_I2S_CON, audio_i2s, 0xFFFFFFFE);
}

void mt_afe_set_i2s_adc(struct mt_afe_info *afe_info,
			struct mt_afe_i2s *digtal_i2s)
{
	uint32_t sampling_rate = mt_afe_i2s_fs(digtal_i2s->i2s_sample_rate);
	uint32_t voice_mode = 0;

	mt_afe_set_reg(afe_info, AFE_ADDA_TOP_CON0, 0, 0x1); /* Use Int ADC */
	if (sampling_rate == MT_AFE_I2S_RATE_8K)
		voice_mode = 0;
	else if (sampling_rate == MT_AFE_I2S_RATE_16K)
		voice_mode = 1;
	else if (sampling_rate == MT_AFE_I2S_RATE_32K)
		voice_mode = 2;
	else if (sampling_rate == MT_AFE_I2S_RATE_48K)
		voice_mode = 3;

	mt_afe_set_reg(afe_info, AFE_ADDA_UL_SRC_CON0,
		       ((voice_mode << 2) | voice_mode) << 17,
		       0x001E0000);
	mt_afe_set_reg(afe_info, AFE_ADDA_NEWIF_CFG1,
		       ((voice_mode < 3) ? 1 : 3) << 10, 0x00000C00);
}
void mt_afe_set_i2s_ext_adc(struct mt_afe_info *afe_info,
			    struct mt_afe_i2s *digtal_i2s)
{
	uint32_t audio_i2s_adc = 0;

	mt_afe_set_reg(afe_info, AFE_ADDA_TOP_CON0, 1, 0x1); /* Using Ext ADC */
	audio_i2s_adc |= (digtal_i2s->lr_swap << 31);
	audio_i2s_adc |= (digtal_i2s->buffer_update_word << 24);
	audio_i2s_adc |= (digtal_i2s->inv_lrck << 23);
	audio_i2s_adc |= (digtal_i2s->fpga_bit_test << 22);
	audio_i2s_adc |= (digtal_i2s->fpga_bit << 21);
	audio_i2s_adc |= (digtal_i2s->loopback << 20);
	audio_i2s_adc |= (mt_afe_i2s_fs(digtal_i2s->i2s_sample_rate) << 8);
	audio_i2s_adc |= (digtal_i2s->i2s_fmt << 3);
	audio_i2s_adc |= (digtal_i2s->i2s_wlen << 1);
	dev_info(afe_info->dev, "%s audio_i2s_adc = 0x%x",
		 __func__, audio_i2s_adc);
	mt_afe_set_reg(afe_info, AFE_I2S_CON2, audio_i2s_adc, 0xFFFFFFFF);
}

void mt_afe_set_i2s_adc_enable(struct mt_afe_info *afe_info, bool enable)
{
	mt_afe_set_reg(afe_info, AFE_ADDA_UL_SRC_CON0, enable ? 1 : 0, 0x01);
	afe_info->afe_pin[MT_AFE_PIN_I2S_ADC].state = enable;
	if (enable) {
		mt_afe_set_reg(afe_info, AFE_ADDA_UL_DL_CON0, 0x0001, 0x0001);
	} else if
	(!afe_info->afe_pin[MT_AFE_PIN_I2S_DAC].state &&
	 !afe_info->afe_pin[MT_AFE_PIN_I2S_ADC].state) {
		mt_afe_set_reg(afe_info, AFE_ADDA_UL_DL_CON0, 0x0000, 0x0001);
	}
}

void mt_afe_set_2nd_i2s_enable(struct mt_afe_info *afe_info,
			       enum mt_afe_i2s_dir dir_in, bool enable)
{
	if (afe_info->afe_pin[MT_AFE_PIN_I2S_O2 + dir_in].state == enable)
		return;
	afe_info->afe_pin[MT_AFE_PIN_I2S_O2 + dir_in].state = enable;

	/*if another is enabled already?*/
	if (!afe_info->afe_pin[MT_AFE_PIN_I2S_O2 + (dir_in ? 0 : 1)].state)
		mt_afe_set_reg(afe_info, AFE_I2S_CON, enable, 0x1);
}


void mt_afe_clean_pre_distortion(struct mt_afe_info *afe_info)
{
	dev_dbg(afe_info->dev, "%s\n", __func__);
	mt_afe_set_reg(afe_info, AFE_ADDA_PREDIS_CON0, 0, 0xFFFFFFFF);
	mt_afe_set_reg(afe_info, AFE_ADDA_PREDIS_CON1, 0, 0xFFFFFFFF);
}

void mt_afe_set_dl_src2(struct mt_afe_info *afe_info, uint32_t sample_rate)
{
	uint32_t afe_adda_dl_src2_con0, afe_adda_dl_src2_con1;

	if (sample_rate == 44100)
		afe_adda_dl_src2_con0 = 7;
	else if (sample_rate == 8000)
		afe_adda_dl_src2_con0 = 0;
	else if (sample_rate == 11025)
		afe_adda_dl_src2_con0 = 1;
	else if (sample_rate == 12000)
		afe_adda_dl_src2_con0 = 2;
	else if (sample_rate == 16000)
		afe_adda_dl_src2_con0 = 3;
	else if (sample_rate == 22050)
		afe_adda_dl_src2_con0 = 4;
	else if (sample_rate == 24000)
		afe_adda_dl_src2_con0 = 5;
	else if (sample_rate == 32000)
		afe_adda_dl_src2_con0 = 6;
	else if (sample_rate == 48000)
		afe_adda_dl_src2_con0 = 8;
	else
		afe_adda_dl_src2_con0 = 7;	/* Default 44100 */

	/* 8k or 16k voice mode */
	if (afe_adda_dl_src2_con0 == 0 || afe_adda_dl_src2_con0 == 3) {
		afe_adda_dl_src2_con0 =
		    (afe_adda_dl_src2_con0 << 28) | (0x03 << 24) |
		    (0x03 << 11) | (0x01 << 5);
	} else {
		afe_adda_dl_src2_con0 = (afe_adda_dl_src2_con0 << 28) |
						(0x03 << 24) | (0x03 << 11);
	}
	/* SA suggest apply -0.3db to audio/speech path*/
	afe_adda_dl_src2_con0 = afe_adda_dl_src2_con0 | (0x01 << 1);
	afe_adda_dl_src2_con1 = 0xf74f0000;

	mt_afe_set_reg(afe_info, AFE_ADDA_DL_SRC2_CON0,
		       afe_adda_dl_src2_con0, 0xFFFFFFFF);
	mt_afe_set_reg(afe_info, AFE_ADDA_DL_SRC2_CON1,
		       afe_adda_dl_src2_con1, 0xFFFFFFFF);
}

void mt_afe_set_i2s_dac_out(struct mt_afe_info *afe_info,
			    uint32_t sample_rate)
{
	uint32_t audio_i2s_dac = 0;

	dev_dbg(afe_info->dev, "mt_afe_set_i2s_dac_out\n");
	mt_afe_clean_pre_distortion(afe_info);
	mt_afe_set_dl_src2(afe_info, sample_rate);

	audio_i2s_dac |= (MT_AFE_LR_SWAP_NO << 31);
	audio_i2s_dac |= (mt_afe_i2s_fs(sample_rate) << 8);
	audio_i2s_dac |= (MT_AFE_INV_LRCK_NO << 5);
	audio_i2s_dac |= (MT_AFE_I2S_FORMAT_I2S << 3);
	audio_i2s_dac |= (MT_AFE_I2S_WLEN_16BITS << 1);
	mt_afe_set_reg(afe_info, AFE_I2S_CON1, audio_i2s_dac, 0xFFFFFFFF);
}

void mt_afe_set_hdmi_path_enable(struct mt_afe_info *afe_info, bool enable)
{
	afe_info->afe_pin[MT_AFE_PIN_HDMI].state = enable;
}

int mt_afe_set_memory_path_enable(struct mt_afe_info *afe_info,
				  uint32_t aud_block, bool enable)
{
	if (aud_block > MT_AFE_PIN_MOD_DAI)
		return -EINVAL;

	if (enable) {
		afe_info->afe_pin[aud_block].state = true;
		mt_afe_set_reg(afe_info, AFE_DAC_CON0,
			       enable << (aud_block + 1),
			    1 << (aud_block + 1));
	} else {
		mt_afe_set_reg(afe_info, AFE_DAC_CON0,
			       enable << (aud_block + 1),
			    1 << (aud_block + 1));
		afe_info->afe_pin[aud_block].state = false;
	}
	return 0;
}

void mt_afe_set_i2s_dac_enable(struct mt_afe_info *afe_info, bool enable)
{
	afe_info->afe_pin[MT_AFE_PIN_I2S_DAC].state = enable;

	if (enable) {
		mt_afe_set_reg(afe_info, AFE_ADDA_DL_SRC2_CON0, enable, 0x01);
		mt_afe_set_reg(afe_info, AFE_I2S_CON1, enable, 0x1);
		mt_afe_set_reg(afe_info, AFE_ADDA_UL_DL_CON0, enable, 0x0001);
	} else {
		mt_afe_set_reg(afe_info, AFE_ADDA_DL_SRC2_CON0, enable, 0x01);
		mt_afe_set_reg(afe_info, AFE_I2S_CON1, enable, 0x1);

		if (!afe_info->afe_pin[MT_AFE_PIN_I2S_DAC].state &&
		    !afe_info->afe_pin[MT_AFE_PIN_I2S_ADC].state) {
			mt_afe_set_reg(afe_info, AFE_ADDA_UL_DL_CON0,
				       enable, 0x0001);
		}
	}
}

bool mt_afe_get_i2s_dac_enable(struct mt_afe_info *afe_info)
{
	return afe_info->afe_pin[MT_AFE_PIN_I2S_DAC].state;
}

int mt_afe_set_connection(struct mt_afe_info *afe_info,
			  uint32_t conn_state, uint32_t input,
			  uint32_t output)
{
	return mt_afe_set_connection_state(afe_info, conn_state,
					   input, output);
}

void mt_afe_set_irq_enable(struct mt_afe_info *afe_info, uint32_t irq,
			   bool enable)
{
	dev_dbg(afe_info->dev, "%s(), irq = %d, enable = %d\n",
		__func__, irq, enable);

	switch (irq) {
	case MT_AFE_IRQ_MODE_2:
		if (!enable && mt_afe_get_ul_memif_enable(afe_info)) {
			/* IRQ2 is in used */
			dev_info(afe_info->dev, "skip disable IRQ2, AFE_DAC_CON0 = 0x%x\n",
				 mt_afe_get_reg(afe_info, AFE_DAC_CON0));
			break;
		}
	case MT_AFE_IRQ_MODE_1:
	case MT_AFE_IRQ_MODE_3:
		mt_afe_set_reg(afe_info, AFE_IRQ_MCU_CON,
			       (enable << irq), (1 << irq));
		afe_info->audio_mcu_mode[irq].status = enable;
		break;
	case MT_AFE_IRQ_MODE_5:
		mt_afe_set_reg(afe_info, AFE_IRQ_MCU_CON,
			       (enable << 12), (1 << 12));
		afe_info->audio_mcu_mode[irq].status = enable;
		break;
	default:
		dev_warn(afe_info->dev, "%s unexpected irq = %d\n",
			 __func__, irq);
		break;
	}
}

int mt_afe_set_irq_fs(struct mt_afe_info *afe_info,
		      uint32_t irqmode, uint32_t sample_rate)
{
	switch (irqmode) {
	case MT_AFE_IRQ_MODE_1:
		mt_afe_set_reg(afe_info, AFE_IRQ_MCU_CON,
			       (mt_afe_i2s_fs(sample_rate) << 4), 0x000000f0);
		break;
	case MT_AFE_IRQ_MODE_2:
		mt_afe_set_reg(afe_info, AFE_IRQ_MCU_CON,
			       (mt_afe_i2s_fs(sample_rate) << 8), 0x00000f00);
		break;
	case MT_AFE_IRQ_MODE_5:
		break;
	case MT_AFE_IRQ_MODE_3:
	default:
		return -EINVAL;
	}
	return 0;
}

int mt_afe_set_irq_counter(struct mt_afe_info *afe_info,
			   uint32_t irqmode, uint32_t counter)
{
	switch (irqmode) {
	case MT_AFE_IRQ_MODE_1:
		mt_afe_set_reg(afe_info, AFE_IRQ_CNT1, counter, 0xFFFFFFFF);
		break;
	case MT_AFE_IRQ_MODE_2:
		mt_afe_set_reg(afe_info, AFE_IRQ_CNT2, counter, 0xFFFFFFFF);
		break;
	case MT_AFE_IRQ_MODE_5:
		mt_afe_set_reg(afe_info, AFE_IRQ_CNT5, counter, 0xFFFFFFFF);
		break;
	case MT_AFE_IRQ_MODE_3:
	default:
		return -EINVAL;
	}
	return 0;
}

const struct mt_afe_hdmi_clock hdmi_clock[] = {
	{32000, APLL_D24, 0},	/* 32k */
	{44100, APLL_D16, 0},	/* 44.1k */
	{48000, APLL_D16, 0},	/* 48k */
	{88200, APLL_D8, 0},	/* 88.2k */
	{96000, APLL_D8, 0},	/* 96k */
	{176400, APLL_D4, 0},	/* 176.4k */
	{192000, APLL_D4, 0}	/* 192k */
};

static unsigned int mt_afe_hdmi_fs(unsigned int sample_rate)
{
	switch (sample_rate) {
	case 32000:
		return 0;
	case 44100:
		return 1;
	case 48000:
		return 2;
	case 88200:
		return 3;
	case 96000:
		return 4;
	case 176400:
		return 5;
	case 192000:
		return 6;
	}
	return 0;
}

void mt_afe_set_hdmi_out_control(struct mt_afe_info *afe_info,
				 unsigned int channels)
{
	unsigned int register_value = 0;

	register_value |= (channels << 4);
	register_value |= (MT_AFE_HDMI_INPUT_16BIT << 1);
	mt_afe_set_reg(afe_info, AFE_HDMI_OUT_CON0, register_value, 0xFFFFFFFF);
}

void mt_afe_set_hdmi_out_control_enable(struct mt_afe_info *afe_info,
					bool enable)
{
	unsigned int register_value = 0;

	if (enable)
		register_value |= 1;

	mt_afe_set_reg(afe_info, AFE_HDMI_OUT_CON0, register_value, 0x1);
}

void mt_afe_set_hdmi_i2s(struct mt_afe_info *afe_info)
{
	unsigned int register_value = 0;

	register_value |= (MT_AFE_HDMI_I2S_32BIT << 4);
	register_value |= (MT_AFE_HDMI_I2S_FIRST_BIT_1T_DELAY << 3);
	register_value |= (MT_AFE_HDMI_I2S_LRCK_NOT_INVERSE << 2);
	register_value |= (MT_AFE_HDMI_I2S_BCLK_INVERSE << 1);
	mt_afe_set_reg(afe_info, AFE_8CH_I2S_OUT_CON, register_value,
		       0xFFFFFFFF);
}

void mt_afe_set_hdmi_i2s_enable(struct mt_afe_info *afe_info, bool enable)
{
	unsigned int register_value = 0;

	if (enable)
		register_value |= 1;

	mt_afe_set_reg(afe_info, AFE_8CH_I2S_OUT_CON, register_value, 0x1);
}

void mt_afe_set_hdmi_clock_source(struct mt_afe_info *afe_info,
				  unsigned int sample_rate)
{
	unsigned int sample_rate_index = mt_afe_hdmi_fs(sample_rate);

	mt_afe_set_hdmi_clk_src(afe_info,
				hdmi_clock[sample_rate_index].fs,
				hdmi_clock[sample_rate_index].apll_sel);
}

void mt_afe_set_hdmi_bck_div(struct mt_afe_info *afe_info,
			     unsigned int sample_rate)
{
	unsigned int register_value = 0;

	register_value |=
	    (hdmi_clock[mt_afe_hdmi_fs(sample_rate)].bck_div) << 8;
	mt_afe_set_reg(afe_info, AUDIO_TOP_CON3, register_value, 0x3F00);
}

void mt_afe_recover_reg(struct mt_afe_info *afe_info)
{
	uint32_t *reg = afe_info->reg_backup;

	dev_notice(afe_info->dev, "+%s\n", __func__);

	mt_afe_clk_on(afe_info);

	mt_afe_set_reg(afe_info, AUDIO_TOP_CON3, *reg++, 0xFFFFFFFF);
	mt_afe_set_reg(afe_info, AFE_DAC_CON0, *reg++, 0xFFFFFFFF);
	mt_afe_set_reg(afe_info, AFE_DAC_CON1, *reg++, 0xFFFFFFFF);
	mt_afe_set_reg(afe_info, AFE_I2S_CON, *reg++, 0xFFFFFFFF);
	mt_afe_set_reg(afe_info, AFE_DAIBT_CON0, *reg++, 0xFFFFFFFF);
	mt_afe_set_reg(afe_info, AFE_CONN0, *reg++, 0xFFFFFFFF);
	mt_afe_set_reg(afe_info, AFE_CONN1, *reg++, 0xFFFFFFFF);
	mt_afe_set_reg(afe_info, AFE_CONN2, *reg++, 0xFFFFFFFF);
	mt_afe_set_reg(afe_info, AFE_CONN3, *reg++, 0xFFFFFFFF);
	mt_afe_set_reg(afe_info, AFE_CONN4, *reg++, 0xFFFFFFFF);
	mt_afe_set_reg(afe_info, AFE_I2S_CON1, *reg++, 0xFFFFFFFF);
	mt_afe_set_reg(afe_info, AFE_I2S_CON2, *reg++, 0xFFFFFFFF);
	mt_afe_set_reg(afe_info, AFE_MRGIF_CON, *reg++, 0xFFFFFFFF);
	mt_afe_set_reg(afe_info, AFE_DL1_BASE, *reg++, 0xFFFFFFFF);
	mt_afe_set_reg(afe_info, AFE_DL1_CUR, *reg++, 0xFFFFFFFF);
	mt_afe_set_reg(afe_info, AFE_DL1_END, *reg++, 0xFFFFFFFF);
	mt_afe_set_reg(afe_info, AFE_DL2_BASE, *reg++, 0xFFFFFFFF);
	mt_afe_set_reg(afe_info, AFE_DL2_CUR, *reg++, 0xFFFFFFFF);
	mt_afe_set_reg(afe_info, AFE_DL2_END, *reg++, 0xFFFFFFFF);
	mt_afe_set_reg(afe_info, AFE_AWB_BASE, *reg++, 0xFFFFFFFF);
	mt_afe_set_reg(afe_info, AFE_AWB_CUR, *reg++, 0xFFFFFFFF);
	mt_afe_set_reg(afe_info, AFE_AWB_END, *reg++, 0xFFFFFFFF);
	mt_afe_set_reg(afe_info, AFE_VUL_BASE, *reg++, 0xFFFFFFFF);
	mt_afe_set_reg(afe_info, AFE_VUL_CUR, *reg++, 0xFFFFFFFF);
	mt_afe_set_reg(afe_info, AFE_VUL_END, *reg++, 0xFFFFFFFF);
	mt_afe_set_reg(afe_info, AFE_DAI_BASE, *reg++, 0xFFFFFFFF);
	mt_afe_set_reg(afe_info, AFE_DAI_CUR, *reg++, 0xFFFFFFFF);
	mt_afe_set_reg(afe_info, AFE_DAI_END, *reg++, 0xFFFFFFFF);
	mt_afe_set_reg(afe_info, AFE_IRQ_CON, *reg++, 0xFFFFFFFF);
	mt_afe_set_reg(afe_info, AFE_MEMIF_MON0, *reg++, 0xFFFFFFFF);
	mt_afe_set_reg(afe_info, AFE_MEMIF_MON1, *reg++, 0xFFFFFFFF);
	mt_afe_set_reg(afe_info, AFE_MEMIF_MON2, *reg++, 0xFFFFFFFF);
	mt_afe_set_reg(afe_info, AFE_MEMIF_MON3, *reg++, 0xFFFFFFFF);
	mt_afe_set_reg(afe_info, AFE_MEMIF_MON4, *reg++, 0xFFFFFFFF);
	mt_afe_set_reg(afe_info, AFE_ADDA_DL_SRC2_CON0, *reg++, 0xFFFFFFFF);
	mt_afe_set_reg(afe_info, AFE_ADDA_DL_SRC2_CON1, *reg++, 0xFFFFFFFF);
	mt_afe_set_reg(afe_info, AFE_ADDA_UL_SRC_CON0, *reg++, 0xFFFFFFFF);
	mt_afe_set_reg(afe_info, AFE_ADDA_UL_SRC_CON1, *reg++, 0xFFFFFFFF);
	mt_afe_set_reg(afe_info, AFE_ADDA_TOP_CON0, *reg++, 0xFFFFFFFF);
	mt_afe_set_reg(afe_info, AFE_ADDA_UL_DL_CON0, *reg++, 0xFFFFFFFF);
	mt_afe_set_reg(afe_info, AFE_ADDA_NEWIF_CFG0, *reg++, 0xFFFFFFFF);
	mt_afe_set_reg(afe_info, AFE_ADDA_NEWIF_CFG1, *reg++, 0xFFFFFFFF);
	mt_afe_set_reg(afe_info, AFE_FOC_CON, *reg++, 0xFFFFFFFF);
	mt_afe_set_reg(afe_info, AFE_FOC_CON1, *reg++, 0xFFFFFFFF);
	mt_afe_set_reg(afe_info, AFE_FOC_CON2, *reg++, 0xFFFFFFFF);
	mt_afe_set_reg(afe_info, AFE_FOC_CON3, *reg++, 0xFFFFFFFF);
	mt_afe_set_reg(afe_info, AFE_FOC_CON4, *reg++, 0xFFFFFFFF);
	mt_afe_set_reg(afe_info, AFE_FOC_CON5, *reg++, 0xFFFFFFFF);
	mt_afe_set_reg(afe_info, AFE_MON_STEP, *reg++, 0xFFFFFFFF);
	mt_afe_set_reg(afe_info, AFE_SIDETONE_DEBUG, *reg++, 0xFFFFFFFF);
	mt_afe_set_reg(afe_info, AFE_SIDETONE_MON, *reg++, 0xFFFFFFFF);
	mt_afe_set_reg(afe_info, AFE_SIDETONE_CON0, *reg++, 0xFFFFFFFF);
	mt_afe_set_reg(afe_info, AFE_SIDETONE_COEFF, *reg++, 0xFFFFFFFF);
	mt_afe_set_reg(afe_info, AFE_SIDETONE_CON1, *reg++, 0xFFFFFFFF);
	mt_afe_set_reg(afe_info, AFE_SIDETONE_GAIN, *reg++, 0xFFFFFFFF);
	mt_afe_set_reg(afe_info, AFE_SGEN_CON0, *reg++, 0xFFFFFFFF);
	mt_afe_set_reg(afe_info, AFE_ADDA_PREDIS_CON0, *reg++, 0xFFFFFFFF);
	mt_afe_set_reg(afe_info, AFE_ADDA_PREDIS_CON1, *reg++, 0xFFFFFFFF);
	mt_afe_set_reg(afe_info, AFE_MRGIF_MON0, *reg++, 0xFFFFFFFF);
	mt_afe_set_reg(afe_info, AFE_MRGIF_MON1, *reg++, 0xFFFFFFFF);
	mt_afe_set_reg(afe_info, AFE_MRGIF_MON2, *reg++, 0xFFFFFFFF);
	mt_afe_set_reg(afe_info, AFE_MOD_PCM_BASE, *reg++, 0xFFFFFFFF);
	mt_afe_set_reg(afe_info, AFE_MOD_PCM_END, *reg++, 0xFFFFFFFF);
	mt_afe_set_reg(afe_info, AFE_MOD_PCM_CUR, *reg++, 0xFFFFFFFF);
	mt_afe_set_reg(afe_info, AFE_HDMI_OUT_CON0, *reg++, 0xFFFFFFFF);
	mt_afe_set_reg(afe_info, AFE_HDMI_OUT_BASE, *reg++, 0xFFFFFFFF);
	mt_afe_set_reg(afe_info, AFE_HDMI_OUT_CUR, *reg++, 0xFFFFFFFF);
	mt_afe_set_reg(afe_info, AFE_HDMI_OUT_END, *reg++, 0xFFFFFFFF);
	mt_afe_set_reg(afe_info, AFE_SPDIF_OUT_CON0, *reg++, 0xFFFFFFFF);
	mt_afe_set_reg(afe_info, AFE_SPDIF_BASE, *reg++, 0xFFFFFFFF);
	mt_afe_set_reg(afe_info, AFE_SPDIF_CUR, *reg++, 0xFFFFFFFF);
	mt_afe_set_reg(afe_info, AFE_SPDIF_END, *reg++, 0xFFFFFFFF);
	mt_afe_set_reg(afe_info, AFE_HDMI_CONN0, *reg++, 0xFFFFFFFF);
	mt_afe_set_reg(afe_info, AFE_8CH_I2S_OUT_CON, *reg++, 0xFFFFFFFF);
	mt_afe_set_reg(afe_info, AFE_IEC_CFG, *reg++, 0xFFFFFFFF);
	mt_afe_set_reg(afe_info, AFE_IEC_NSNUM, *reg++, 0xFFFFFFFF);
	mt_afe_set_reg(afe_info, AFE_IEC_BURST_INFO, *reg++, 0xFFFFFFFF);
	mt_afe_set_reg(afe_info, AFE_IEC_BURST_LEN, *reg++, 0xFFFFFFFF);
	mt_afe_set_reg(afe_info, AFE_IEC_NSADR, *reg++, 0xFFFFFFFF);
	mt_afe_set_reg(afe_info, AFE_IEC_CHL_STAT0, *reg++, 0xFFFFFFFF);
	mt_afe_set_reg(afe_info, AFE_IEC_CHL_STAT1, *reg++, 0xFFFFFFFF);
	mt_afe_set_reg(afe_info, AFE_IEC_CHR_STAT0, *reg++, 0xFFFFFFFF);
	mt_afe_set_reg(afe_info, AFE_IEC_CHR_STAT1, *reg++, 0xFFFFFFFF);
	mt_afe_set_reg(afe_info, AFE_IRQ_MCU_CON, *reg++, 0xFFFFFFFF);
	mt_afe_set_reg(afe_info, AFE_IRQ_STATUS, *reg++, 0xFFFFFFFF);
	mt_afe_set_reg(afe_info, AFE_IRQ_CLR, *reg++, 0xFFFFFFFF);
	mt_afe_set_reg(afe_info, AFE_IRQ_CNT1, *reg++, 0xFFFFFFFF);
	mt_afe_set_reg(afe_info, AFE_IRQ_CNT2, *reg++, 0xFFFFFFFF);
	mt_afe_set_reg(afe_info, AFE_IRQ_MON2, *reg++, 0xFFFFFFFF);
	mt_afe_set_reg(afe_info, AFE_IRQ_CNT5, *reg++, 0xFFFFFFFF);
	mt_afe_set_reg(afe_info, AFE_IRQ1_CNT_MON, *reg++, 0xFFFFFFFF);
	mt_afe_set_reg(afe_info, AFE_IRQ2_CNT_MON, *reg++, 0xFFFFFFFF);
	mt_afe_set_reg(afe_info, AFE_IRQ1_EN_CNT_MON, *reg++, 0xFFFFFFFF);
	mt_afe_set_reg(afe_info, AFE_IRQ5_EN_CNT_MON, *reg++, 0xFFFFFFFF);
	mt_afe_set_reg(afe_info, AFE_MEMIF_MINLEN, *reg++, 0xFFFFFFFF);
	mt_afe_set_reg(afe_info, AFE_MEMIF_MAXLEN, *reg++, 0xFFFFFFFF);
	mt_afe_set_reg(afe_info, AFE_MEMIF_PBUF_SIZE, *reg++, 0xFFFFFFFF);
	mt_afe_set_reg(afe_info, AFE_GAIN1_CON0, *reg++, 0xFFFFFFFF);
	mt_afe_set_reg(afe_info, AFE_GAIN1_CON1, *reg++, 0xFFFFFFFF);
	mt_afe_set_reg(afe_info, AFE_GAIN1_CON2, *reg++, 0xFFFFFFFF);
	mt_afe_set_reg(afe_info, AFE_GAIN1_CON3, *reg++, 0xFFFFFFFF);
	mt_afe_set_reg(afe_info, AFE_GAIN1_CONN, *reg++, 0xFFFFFFFF);
	mt_afe_set_reg(afe_info, AFE_GAIN1_CUR, *reg++, 0xFFFFFFFF);
	mt_afe_set_reg(afe_info, AFE_GAIN2_CON0, *reg++, 0xFFFFFFFF);
	mt_afe_set_reg(afe_info, AFE_GAIN2_CON1, *reg++, 0xFFFFFFFF);
	mt_afe_set_reg(afe_info, AFE_GAIN2_CON2, *reg++, 0xFFFFFFFF);
	mt_afe_set_reg(afe_info, AFE_GAIN2_CON3, *reg++, 0xFFFFFFFF);
	mt_afe_set_reg(afe_info, AFE_GAIN2_CONN, *reg++, 0xFFFFFFFF);
	mt_afe_set_reg(afe_info, AFE_ASRC_CON0, *reg++, 0xFFFFFFFF);
	mt_afe_set_reg(afe_info, AFE_ASRC_CON1, *reg++, 0xFFFFFFFF);
	mt_afe_set_reg(afe_info, AFE_ASRC_CON2, *reg++, 0xFFFFFFFF);
	mt_afe_set_reg(afe_info, AFE_ASRC_CON3, *reg++, 0xFFFFFFFF);
	mt_afe_set_reg(afe_info, AFE_ASRC_CON4, *reg++, 0xFFFFFFFF);
	mt_afe_set_reg(afe_info, AFE_ASRC_CON6, *reg++, 0xFFFFFFFF);
	mt_afe_set_reg(afe_info, AFE_ASRC_CON7, *reg++, 0xFFFFFFFF);
	mt_afe_set_reg(afe_info, AFE_ASRC_CON8, *reg++, 0xFFFFFFFF);
	mt_afe_set_reg(afe_info, AFE_ASRC_CON9, *reg++, 0xFFFFFFFF);
	mt_afe_set_reg(afe_info, AFE_ASRC_CON10, *reg++, 0xFFFFFFFF);
	mt_afe_set_reg(afe_info, AFE_ASRC_CON11, *reg++, 0xFFFFFFFF);
	mt_afe_set_reg(afe_info, PCM_INTF_CON1, *reg++, 0xFFFFFFFF);
	mt_afe_set_reg(afe_info, PCM_INTF_CON2, *reg++, 0xFFFFFFFF);
	mt_afe_set_reg(afe_info, PCM2_INTF_CON, *reg++, 0xFFFFFFFF);

	mt_afe_clk_off(afe_info);
	dev_notice(afe_info->dev, "-%s\n", __func__);
}

void mt_afe_store_reg(struct mt_afe_info *afe_info)
{
	uint32_t *reg_backup = afe_info->reg_backup;

	dev_notice(afe_info->dev, "+%s\n", __func__);

	mt_afe_clk_on(afe_info);

	*reg_backup++ = mt_afe_get_reg(afe_info, AUDIO_TOP_CON3);
	*reg_backup++ = mt_afe_get_reg(afe_info, AFE_DAC_CON0);
	*reg_backup++ = mt_afe_get_reg(afe_info, AFE_DAC_CON1);
	*reg_backup++ = mt_afe_get_reg(afe_info, AFE_I2S_CON);
	*reg_backup++ = mt_afe_get_reg(afe_info, AFE_DAIBT_CON0);
	*reg_backup++ = mt_afe_get_reg(afe_info, AFE_CONN0);
	*reg_backup++ = mt_afe_get_reg(afe_info, AFE_CONN1);
	*reg_backup++ = mt_afe_get_reg(afe_info, AFE_CONN2);
	*reg_backup++ = mt_afe_get_reg(afe_info, AFE_CONN3);
	*reg_backup++ = mt_afe_get_reg(afe_info, AFE_CONN4);
	*reg_backup++ = mt_afe_get_reg(afe_info, AFE_I2S_CON1);
	*reg_backup++ = mt_afe_get_reg(afe_info, AFE_I2S_CON2);
	*reg_backup++ = mt_afe_get_reg(afe_info, AFE_MRGIF_CON);
	*reg_backup++ = mt_afe_get_reg(afe_info, AFE_DL1_BASE);
	*reg_backup++ = mt_afe_get_reg(afe_info, AFE_DL1_CUR);
	*reg_backup++ = mt_afe_get_reg(afe_info, AFE_DL1_END);
	*reg_backup++ = mt_afe_get_reg(afe_info, AFE_DL2_BASE);
	*reg_backup++ = mt_afe_get_reg(afe_info, AFE_DL2_CUR);
	*reg_backup++ = mt_afe_get_reg(afe_info, AFE_DL2_END);
	*reg_backup++ = mt_afe_get_reg(afe_info, AFE_AWB_BASE);
	*reg_backup++ = mt_afe_get_reg(afe_info, AFE_AWB_CUR);
	*reg_backup++ = mt_afe_get_reg(afe_info, AFE_AWB_END);
	*reg_backup++ = mt_afe_get_reg(afe_info, AFE_VUL_BASE);
	*reg_backup++ = mt_afe_get_reg(afe_info, AFE_VUL_CUR);
	*reg_backup++ = mt_afe_get_reg(afe_info, AFE_VUL_END);
	*reg_backup++ = mt_afe_get_reg(afe_info, AFE_DAI_BASE);
	*reg_backup++ = mt_afe_get_reg(afe_info, AFE_DAI_CUR);
	*reg_backup++ = mt_afe_get_reg(afe_info, AFE_DAI_END);
	*reg_backup++ = mt_afe_get_reg(afe_info, AFE_IRQ_CON);
	*reg_backup++ = mt_afe_get_reg(afe_info, AFE_MEMIF_MON0);
	*reg_backup++ = mt_afe_get_reg(afe_info, AFE_MEMIF_MON1);
	*reg_backup++ = mt_afe_get_reg(afe_info, AFE_MEMIF_MON2);
	*reg_backup++ = mt_afe_get_reg(afe_info, AFE_MEMIF_MON3);
	*reg_backup++ = mt_afe_get_reg(afe_info, AFE_MEMIF_MON4);
	*reg_backup++ = mt_afe_get_reg(afe_info, AFE_ADDA_DL_SRC2_CON0);
	*reg_backup++ = mt_afe_get_reg(afe_info, AFE_ADDA_DL_SRC2_CON1);
	*reg_backup++ = mt_afe_get_reg(afe_info, AFE_ADDA_UL_SRC_CON0);
	*reg_backup++ = mt_afe_get_reg(afe_info, AFE_ADDA_UL_SRC_CON1);
	*reg_backup++ = mt_afe_get_reg(afe_info, AFE_ADDA_TOP_CON0);
	*reg_backup++ = mt_afe_get_reg(afe_info, AFE_ADDA_UL_DL_CON0);
	*reg_backup++ = mt_afe_get_reg(afe_info, AFE_ADDA_NEWIF_CFG0);
	*reg_backup++ = mt_afe_get_reg(afe_info, AFE_ADDA_NEWIF_CFG1);
	*reg_backup++ = mt_afe_get_reg(afe_info, AFE_FOC_CON);
	*reg_backup++ = mt_afe_get_reg(afe_info, AFE_FOC_CON1);
	*reg_backup++ = mt_afe_get_reg(afe_info, AFE_FOC_CON2);
	*reg_backup++ = mt_afe_get_reg(afe_info, AFE_FOC_CON3);
	*reg_backup++ = mt_afe_get_reg(afe_info, AFE_FOC_CON4);
	*reg_backup++ = mt_afe_get_reg(afe_info, AFE_FOC_CON5);
	*reg_backup++ = mt_afe_get_reg(afe_info, AFE_MON_STEP);
	*reg_backup++ = mt_afe_get_reg(afe_info, AFE_SIDETONE_DEBUG);
	*reg_backup++ = mt_afe_get_reg(afe_info, AFE_SIDETONE_MON);
	*reg_backup++ = mt_afe_get_reg(afe_info, AFE_SIDETONE_CON0);
	*reg_backup++ = mt_afe_get_reg(afe_info, AFE_SIDETONE_COEFF);
	*reg_backup++ = mt_afe_get_reg(afe_info, AFE_SIDETONE_CON1);
	*reg_backup++ = mt_afe_get_reg(afe_info, AFE_SIDETONE_GAIN);
	*reg_backup++ = mt_afe_get_reg(afe_info, AFE_SGEN_CON0);
	*reg_backup++ = mt_afe_get_reg(afe_info, AFE_ADDA_PREDIS_CON0);
	*reg_backup++ = mt_afe_get_reg(afe_info, AFE_ADDA_PREDIS_CON1);
	*reg_backup++ = mt_afe_get_reg(afe_info, AFE_MRGIF_MON0);
	*reg_backup++ = mt_afe_get_reg(afe_info, AFE_MRGIF_MON1);
	*reg_backup++ = mt_afe_get_reg(afe_info, AFE_MRGIF_MON2);
	*reg_backup++ = mt_afe_get_reg(afe_info, AFE_MOD_PCM_BASE);
	*reg_backup++ = mt_afe_get_reg(afe_info, AFE_MOD_PCM_END);
	*reg_backup++ = mt_afe_get_reg(afe_info, AFE_MOD_PCM_CUR);
	*reg_backup++ = mt_afe_get_reg(afe_info, AFE_HDMI_OUT_CON0);
	*reg_backup++ = mt_afe_get_reg(afe_info, AFE_HDMI_OUT_BASE);
	*reg_backup++ = mt_afe_get_reg(afe_info, AFE_HDMI_OUT_CUR);
	*reg_backup++ = mt_afe_get_reg(afe_info, AFE_HDMI_OUT_END);
	*reg_backup++ = mt_afe_get_reg(afe_info, AFE_SPDIF_OUT_CON0);
	*reg_backup++ = mt_afe_get_reg(afe_info, AFE_SPDIF_BASE);
	*reg_backup++ = mt_afe_get_reg(afe_info, AFE_SPDIF_CUR);
	*reg_backup++ = mt_afe_get_reg(afe_info, AFE_SPDIF_END);
	*reg_backup++ = mt_afe_get_reg(afe_info, AFE_HDMI_CONN0);
	*reg_backup++ = mt_afe_get_reg(afe_info, AFE_8CH_I2S_OUT_CON);
	*reg_backup++ = mt_afe_get_reg(afe_info, AFE_IEC_CFG);
	*reg_backup++ = mt_afe_get_reg(afe_info, AFE_IEC_NSNUM);
	*reg_backup++ = mt_afe_get_reg(afe_info, AFE_IEC_BURST_INFO);
	*reg_backup++ = mt_afe_get_reg(afe_info, AFE_IEC_BURST_LEN);
	*reg_backup++ = mt_afe_get_reg(afe_info, AFE_IEC_NSADR);
	*reg_backup++ = mt_afe_get_reg(afe_info, AFE_IEC_CHL_STAT0);
	*reg_backup++ = mt_afe_get_reg(afe_info, AFE_IEC_CHL_STAT1);
	*reg_backup++ = mt_afe_get_reg(afe_info, AFE_IEC_CHR_STAT0);
	*reg_backup++ = mt_afe_get_reg(afe_info, AFE_IEC_CHR_STAT1);
	*reg_backup++ = mt_afe_get_reg(afe_info, AFE_IRQ_MCU_CON);
	*reg_backup++ = mt_afe_get_reg(afe_info, AFE_IRQ_STATUS);
	*reg_backup++ = mt_afe_get_reg(afe_info, AFE_IRQ_CLR);
	*reg_backup++ = mt_afe_get_reg(afe_info, AFE_IRQ_CNT1);
	*reg_backup++ = mt_afe_get_reg(afe_info, AFE_IRQ_CNT2);
	*reg_backup++ = mt_afe_get_reg(afe_info, AFE_IRQ_MON2);
	*reg_backup++ = mt_afe_get_reg(afe_info, AFE_IRQ_CNT5);
	*reg_backup++ = mt_afe_get_reg(afe_info, AFE_IRQ1_CNT_MON);
	*reg_backup++ = mt_afe_get_reg(afe_info, AFE_IRQ2_CNT_MON);
	*reg_backup++ = mt_afe_get_reg(afe_info, AFE_IRQ1_EN_CNT_MON);
	*reg_backup++ = mt_afe_get_reg(afe_info, AFE_IRQ5_EN_CNT_MON);
	*reg_backup++ = mt_afe_get_reg(afe_info, AFE_MEMIF_MINLEN);
	*reg_backup++ = mt_afe_get_reg(afe_info, AFE_MEMIF_MAXLEN);
	*reg_backup++ = mt_afe_get_reg(afe_info, AFE_MEMIF_PBUF_SIZE);
	*reg_backup++ = mt_afe_get_reg(afe_info, AFE_GAIN1_CON0);
	*reg_backup++ = mt_afe_get_reg(afe_info, AFE_GAIN1_CON1);
	*reg_backup++ = mt_afe_get_reg(afe_info, AFE_GAIN1_CON2);
	*reg_backup++ = mt_afe_get_reg(afe_info, AFE_GAIN1_CON3);
	*reg_backup++ = mt_afe_get_reg(afe_info, AFE_GAIN1_CONN);
	*reg_backup++ = mt_afe_get_reg(afe_info, AFE_GAIN1_CUR);
	*reg_backup++ = mt_afe_get_reg(afe_info, AFE_GAIN2_CON0);
	*reg_backup++ = mt_afe_get_reg(afe_info, AFE_GAIN2_CON1);
	*reg_backup++ = mt_afe_get_reg(afe_info, AFE_GAIN2_CON2);
	*reg_backup++ = mt_afe_get_reg(afe_info, AFE_GAIN2_CON3);
	*reg_backup++ = mt_afe_get_reg(afe_info, AFE_GAIN2_CONN);
	*reg_backup++ = mt_afe_get_reg(afe_info, AFE_ASRC_CON0);
	*reg_backup++ = mt_afe_get_reg(afe_info, AFE_ASRC_CON1);
	*reg_backup++ = mt_afe_get_reg(afe_info, AFE_ASRC_CON2);
	*reg_backup++ = mt_afe_get_reg(afe_info, AFE_ASRC_CON3);
	*reg_backup++ = mt_afe_get_reg(afe_info, AFE_ASRC_CON4);
	*reg_backup++ = mt_afe_get_reg(afe_info, AFE_ASRC_CON6);
	*reg_backup++ = mt_afe_get_reg(afe_info, AFE_ASRC_CON7);
	*reg_backup++ = mt_afe_get_reg(afe_info, AFE_ASRC_CON8);
	*reg_backup++ = mt_afe_get_reg(afe_info, AFE_ASRC_CON9);
	*reg_backup++ = mt_afe_get_reg(afe_info, AFE_ASRC_CON10);
	*reg_backup++ = mt_afe_get_reg(afe_info, AFE_ASRC_CON11);
	*reg_backup++ = mt_afe_get_reg(afe_info, PCM_INTF_CON1);
	*reg_backup++ = mt_afe_get_reg(afe_info, PCM_INTF_CON2);
	*reg_backup++ = mt_afe_get_reg(afe_info, PCM2_INTF_CON);

	mt_afe_clk_off(afe_info);
	dev_notice(afe_info->dev, "-%s\n", __func__);
}



void mt_afe_suspend(struct mt_afe_info *afe_info)
{
	if (!afe_info->suspended) {
		mt_afe_store_reg(afe_info);
		mt_suspend_clk_off(afe_info);
		afe_info->suspended = true;
	}
}

void mt_afe_resume(struct mt_afe_info *afe_info)
{
	if (afe_info->suspended) {
		mt_suspend_clk_on(afe_info);
		mt_afe_recover_reg(afe_info);
		afe_info->suspended = false;
		mt_afe_reset_buffer(afe_info, MT_AFE_PIN_DL1);
	}
}

