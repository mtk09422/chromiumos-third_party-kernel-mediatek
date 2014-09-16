/*
 * mt_soc_afe_control.c  --  Mediatek AFE controller
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

#include "mt_soc_common.h"
#include "mt_soc_clk.h"
#include "mt_soc_afe_control.h"
#include "mt_soc_afe_connection.h"
#include <linux/interrupt.h>
#include <linux/spinlock.h>
#include <linux/dma-mapping.h>
#include <linux/gpio.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/clk.h>


/*
 *    global variable control
 */
/* address for ioremap audio hardware register */
void *afe_base_address;
void *afe_sram_address;
uint32_t afe_sram_phy_address;
uint32_t afe_sram_size;

/* static  variable */
static struct aud_digital_i2s *audio_adc_i2s;
static const bool audio_adc_i2s_status;

static struct aud_irq_mcu_mode *audio_mcu_mode[MTAUD_IRQ_MCU_MODE_NUM]
	= { NULL };
static struct aud_memif_attribute *audio_memif[MTAUD_BLOCK_NUM] = { NULL };

static bool audio_init;

static struct afe_mem_control afe_dl1_control_context;
struct afe_mem_control afe_vul_control_context;
struct afe_mem_control afe_dai_control_context;

static struct snd_pcm_substream *sub_stream[MTAUD_MEMIF_NUM] = { NULL };

/* mutex lock */
static DEFINE_MUTEX(afe_control_mutex);
static DEFINE_SPINLOCK(afe_control_lock);

/*
 *    function implementation
 */
static irqreturn_t aud_drv_irq_handler(int irq, void *dev_id);
static void aud_drv_dl_interrupt_handler(void);

void afe_set_reg(uint32_t offset, uint32_t value, uint32_t mask)
{
	const uint32_t *address = (uint32_t *)((char *)afe_base_address
								+ offset);
	uint32_t val_tmp;

	val_tmp = readl(address);
	val_tmp &= (~mask);
	val_tmp |= (value & mask);
	writel(val_tmp, IOMEM(address));
}


int do_platform_init(struct device *dev)
{
	struct resource res;
	unsigned int irq_id;
	int ret = 0;

	if (!dev->of_node) {
		dev_err(dev, "No platform data supplied\n");
		return -EINVAL;
	}

	/*get IRQ*/
	irq_id = irq_of_parse_and_map(dev->of_node, 0);
	if (!irq_id) {
		dev_err(dev, "no irq for node %s\n", dev->of_node->name);
		return -ENXIO;
	}
	ret = request_irq(irq_id, aud_drv_irq_handler,
			  IRQF_TRIGGER_LOW, "Afe_ISR_Handle", dev);
	if (ret) {
		dev_err(dev, "could not request_irq\n");
		return ret;
	}

	/*get SRAM base*/
	ret = of_address_to_resource(dev->of_node, 0, &res);
	if (ret) {
		dev_err(dev, "could not get SRAM base\n");
		return ret;
	}
	afe_sram_phy_address = res.start;
	afe_sram_size = resource_size(&res);
	afe_sram_address = ioremap_nocache(res.start, afe_sram_size);

	/*get REG base*/
	ret = of_address_to_resource(dev->of_node, 1, &res);
	if (ret) {
		dev_err(dev, "could not get REG base\n");
		return ret;
	}
	afe_base_address = ioremap_nocache(res.start, resource_size(&res));

	pr_info("AudDrv IRQ = %d BASE = 0x%p SRAM = 0x%p size = %d\n",
		irq_id, afe_base_address, afe_sram_address, afe_sram_size);

	init_afe_control();
	return ret;
}

bool init_afe_control(void)
{
	int i = 0;

	/* first time to init , reg init. */
	mutex_lock(&afe_control_mutex);
	if (!audio_init) {
		audio_init = true;
		audio_adc_i2s = kzalloc(sizeof(*audio_adc_i2s), GFP_KERNEL);

		for (i = 0; i < MTAUD_IRQ_MCU_MODE_NUM; i++)
			audio_mcu_mode[i] = kzalloc(
				sizeof(*audio_mcu_mode[0]), GFP_KERNEL);

		for (i = 0; i < MTAUD_BLOCK_NUM; i++)
			audio_memif[i] = kzalloc(
				sizeof(*audio_memif[0]), GFP_KERNEL);

		afe_dl1_control_context.mem_if_num = MTAUD_BLOCK_MEM_DL1;
		afe_vul_control_context.mem_if_num = MTAUD_BLOCK_MEM_VUL;
		afe_dai_control_context.mem_if_num = MTAUD_BLOCK_MEM_DAI;
	}
	mutex_unlock(&afe_control_mutex);
	return true;
}

bool reset_afe_control(void)
{
	int i;

	pr_info("reset_afe_control\n");

	mutex_lock(&afe_control_mutex);
	audio_init = false;

	for (i = 0; i < MTAUD_IRQ_MCU_MODE_NUM; i++)
		memset((void *)(audio_mcu_mode[i]), 0,
		       sizeof(struct aud_irq_mcu_mode));

	for (i = 0; i < MTAUD_BLOCK_NUM; i++)
		memset((void *)(audio_memif[i]), 0,
		       sizeof(struct aud_memif_attribute));

	mutex_unlock(&afe_control_mutex);
	return true;
}

/*****************************************************************************
 * FUNCTION
 *  aud_drv_irq_handler / AudDrv_magic_tasklet
 *
 * DESCRIPTION
 *  IRQ handler
 *
 *****************************************************************************
 */
irqreturn_t aud_drv_irq_handler(int irq, void *dev_id)
{
	const uint32_t reg_value = (afe_get_reg(AFE_IRQ_STATUS) &
						IRQ_STATUS_BIT);

	PRINTK_AUDDEEPDRV("aud_drv_irq_handler AFE_IRQ_STATUS = %x\n",
			  reg_value);

	if (reg_value & INTERRUPT_IRQ1_MCU)
		aud_drv_dl_interrupt_handler();

	/* here is error handle , for interrupt is trigger but not status ,
		clear all interrupt with bit 6 */
	if (unlikely(reg_value == 0)) {
		pr_info("%s reg_value == 0\n", __func__);
		/*upstream todo  cannot clk_prepare/clk_unprepare in IRQ*/
		aud_drv_clk_on();
		afe_set_reg(AFE_IRQ_CLR, 1 << 6, 0xff);
		afe_set_reg(AFE_IRQ_CLR, 1, 0xff);
		afe_set_reg(AFE_IRQ_CLR, 1 << 1, 0xff);
		afe_set_reg(AFE_IRQ_CLR, 1 << 2, 0xff);
		afe_set_reg(AFE_IRQ_CLR, 1 << 3, 0xff);
		afe_set_reg(AFE_IRQ_CLR, 1 << 4, 0xff);
		afe_set_reg(AFE_IRQ_CLR, 1 << 5, 0xff);

		aud_drv_clk_off();
		goto aud_drv_irq_handler_exit;
	}

	/* clear irq */
	afe_set_reg(AFE_IRQ_CLR, reg_value, 0xff);

aud_drv_irq_handler_exit:
	return IRQ_HANDLED;
}

void aud_drv_dl_set_platform_info(enum MTAUD_BLOCK digital_block,
				  struct snd_pcm_substream *substream)
{
	if (likely((int)digital_block < ARRAY_SIZE(sub_stream)))
		sub_stream[digital_block] = substream;
	else
		pr_err("%s unexpected digital_block = %d\n", __func__,
		       digital_block);
}

void aud_drv_dl_reset_platform_info(enum MTAUD_BLOCK digital_block)
{
	if (likely((int)digital_block < ARRAY_SIZE(sub_stream)))
		sub_stream[digital_block] = NULL;
	else
		pr_err("%s unexpected digital_block = %d\n", __func__,
		       digital_block);
}

void aud_drv_reset_buffer(enum MTAUD_BLOCK digital_block)
{
	struct afe_block *afe_block;

	switch (digital_block) {
	case MTAUD_BLOCK_MEM_DL1:
		afe_block = &(afe_dl1_control_context.block);
		break;
	case MTAUD_BLOCK_MEM_VUL:
		afe_block = &(afe_vul_control_context.block);
		break;
	case MTAUD_BLOCK_MEM_DAI:
		afe_block = &(afe_dai_control_context.block);
		break;
	default:
		AUDIO_ASSERT(true);
		return;
	}

	memset(afe_block->virt_buf_addr, 0, afe_block->buffer_size);
	afe_block->dma_read_idx = 0;
	afe_block->write_idx = 0;
	afe_block->data_remained = 0;
}


int aud_drv_update_hw_ptr(enum MTAUD_BLOCK digital_block,
			  struct snd_pcm_substream *substream)
{
	struct afe_block *afe_block;
	int rc = 0;

	switch (digital_block) {
	case MTAUD_BLOCK_MEM_DL1:
		afe_block = &(afe_dl1_control_context.block);
		rc = bytes_to_frames(substream->runtime,
				     afe_block->dma_read_idx);
		break;
	case MTAUD_BLOCK_MEM_VUL:
		afe_block = &(afe_vul_control_context.block);
		rc = bytes_to_frames(substream->runtime, afe_block->write_idx);
		break;
	case MTAUD_BLOCK_MEM_DAI:
		afe_block = &(afe_dai_control_context.block);
		rc = bytes_to_frames(substream->runtime, afe_block->write_idx);
		break;
	default:
		AUDIO_ASSERT(true);
		break;
	}

	return rc;
}

/*****************************************************************************
 * FUNCTION
 *  aud_drv_allocate_dl1_buffer / AudDrv_Free_DL1_Buffer
 *
 * DESCRIPTION
 *  allocate DL1 Buffer
 *

******************************************************************************/
int aud_drv_allocate_dl1_buffer(uint32_t afe_buf_length)
{
	uint32_t phy_addr = 0;

	struct afe_block *block = &afe_dl1_control_context.block;

	block->buffer_size = afe_buf_length;

	pr_debug("AudDrv %s length = %d\n", __func__, afe_buf_length);
	if (unlikely(afe_buf_length > AUDDRV_DL1_MAX_BUFFER_LENGTH))
		return -1;

	/* allocate memory */
	if (block->phys_buf_addr == 0) {
		/* todo there should be a sram manager to
			allocate memory for lowpower.powervr_device*/
		phy_addr = afe_sram_phy_address;
		block->phys_buf_addr = phy_addr;

		pr_info("AudDrv %s length = %d\n", __func__, afe_buf_length);
		block->virt_buf_addr = (unsigned char *)afe_sram_address;
	}

	pr_debug("AudDrv %s pucVirtBufAddr = %p\n", __func__,
		 block->virt_buf_addr);

	/* check 32 bytes align */
	if (unlikely((block->phys_buf_addr & 0x1f) != 0))
		pr_warn("[Auddrv] %s is not aligned (0x%x)\n", __func__,
			block->phys_buf_addr);

	block->sample_num_mask = 0x001f;	/* 32 byte align */
	block->write_idx = 0;
	block->dma_read_idx = 0;
	block->data_remained = 0;
	block->fsync_flag = false;
	block->reset_flag = true;

	/* set sram address top hardware */
	afe_set_reg(AFE_DL1_BASE, block->phys_buf_addr, 0xffffffff);
	afe_set_reg(AFE_DL1_END,
		    block->phys_buf_addr + (afe_buf_length - 1), 0xffffffff);

	return 0;
}

static void aud_drv_dl_interrupt_handler(void)
{
	/* irq1 ISR handler */
	signed int afe_consumed_bytes;
	signed int hw_memory_index;
	signed int hw_cur_read_idx = 0;
	struct afe_block * const afe_block =
				&(afe_dl1_control_context.block);

	PRINTK_AUDDEEPDRV("%s\n", __func__);
	hw_cur_read_idx = afe_get_reg(AFE_DL1_CUR);

	if (hw_cur_read_idx == 0) {
		pr_notice("%s hw_cur_read_idx ==0\n", __func__);
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

	if (unlikely((afe_consumed_bytes & 0x03) != 0))
		pr_warn("%s DMA address is not aligned 32 bytes\n", __func__);

	afe_block->dma_read_idx += afe_consumed_bytes;
	afe_block->dma_read_idx %= afe_block->buffer_size;

	PRINTK_AUDDEEPDRV("%s before snd_pcm_period_elapsed\n", __func__);
	snd_pcm_period_elapsed(sub_stream[MTAUD_BLOCK_MEM_DL1]);
	PRINTK_AUDDEEPDRV("%s after snd_pcm_period_elapsed\n", __func__);
}

static bool check_memif_enable(void)
{
	int i;

	for (i = 0; i < MTAUD_BLOCK_NUM; i++) {
		if ((audio_memif[i]->state) == true)
			return true;
	}
	return false;
}

static bool check_uplink_memif_enable(void)
{
	int i;

	for (i = MTAUD_BLOCK_MEM_VUL; i < MTAUD_MEMIF_NUM; i++) {
		if ((audio_memif[i]->state) == true)
			return true;
	}
	return false;
}

/*****************************************************************************
 * FUNCTION
 *  enable_afe
 *
 * DESCRIPTION
 * enable_afe
 *
 *****************************************************************************
 */
void enable_afe(bool enable)
{
	unsigned long flags;
	bool mem_enable;

	spin_lock_irqsave(&afe_control_lock, flags);
	mem_enable = check_memif_enable();

	if (!enable && !mem_enable)
		afe_set_reg(AFE_DAC_CON0, 0x0, 0x1);
	else if (enable && mem_enable)
		afe_set_reg(AFE_DAC_CON0, 0x1, 0x1);

	spin_unlock_irqrestore(&afe_control_lock, flags);
}

static uint32_t i2s_fs(uint32_t sample_rate)
{
	switch (sample_rate) {
	case 8000:
		return MTAUD_I2S_SAMPLERATE_8K;
	case 11025:
		return MTAUD_I2S_SAMPLERATE_11K;
	case 12000:
		return MTAUD_I2S_SAMPLERATE_12K;
	case 16000:
		return MTAUD_I2S_SAMPLERATE_16K;
	case 22050:
		return MTAUD_I2S_SAMPLERATE_22K;
	case 24000:
		return MTAUD_I2S_SAMPLERATE_24K;
	case 32000:
		return MTAUD_I2S_SAMPLERATE_32K;
	case 44100:
		return MTAUD_I2S_SAMPLERATE_44K;
	case 48000:
		return MTAUD_I2S_SAMPLERATE_48K;
	default:
		break;
	}
	return MTAUD_I2S_SAMPLERATE_44K;
}

bool set_sample_rate(uint32_t aud_block, uint32_t sample_rate)
{
	pr_debug("%s aud_block = %d aud_block = %d\n", __func__,
		 aud_block, aud_block);
	sample_rate = i2s_fs(sample_rate);

	switch (aud_block) {
	case MTAUD_BLOCK_MEM_DL1:
		afe_set_reg(AFE_DAC_CON1, sample_rate, 0x0000000f);
		break;
	case MTAUD_BLOCK_MEM_DL2:
		afe_set_reg(AFE_DAC_CON1, sample_rate << 4, 0x000000f0);
		break;
	case MTAUD_BLOCK_MEM_I2S:
		afe_set_reg(AFE_DAC_CON1, sample_rate << 8, 0x00000f00);
		break;
	case MTAUD_BLOCK_MEM_AWB:
		afe_set_reg(AFE_DAC_CON1, sample_rate << 12, 0x0000f000);
		break;
	case MTAUD_BLOCK_MEM_VUL:
		afe_set_reg(AFE_DAC_CON1, sample_rate << 16, 0x000f0000);
		break;
	case MTAUD_BLOCK_MEM_DAI:
		if (sample_rate == MTAUD_I2S_SAMPLERATE_8K)
			afe_set_reg(AFE_DAC_CON1, 0 << 20, 1 << 20);
		else if (sample_rate == MTAUD_I2S_SAMPLERATE_16K)
			afe_set_reg(AFE_DAC_CON1, 1 << 20, 1 << 20);
		else
			return false;
		break;
	case MTAUD_BLOCK_MEM_MOD_DAI:
		if (sample_rate == MTAUD_I2S_SAMPLERATE_8K)
			afe_set_reg(AFE_DAC_CON1, 0 << 30, 1 << 30);
		else if (sample_rate == MTAUD_I2S_SAMPLERATE_16K)
			afe_set_reg(AFE_DAC_CON1, 1 << 30, 1 << 30);
		else
			return false;
		break;
	}
	return true;
}

bool set_channels(uint32_t memory_interface, uint32_t channel)
{
	switch (memory_interface) {
	case MTAUD_BLOCK_MEM_AWB:
		afe_set_reg(AFE_DAC_CON1, channel << 24, 1 << 24);
		break;
	case MTAUD_BLOCK_MEM_VUL:
		afe_set_reg(AFE_DAC_CON1, channel << 27, 1 << 27);
		break;
	default:
		pr_info("set_channels  memory_interface = %d, channel = %d\n",
			memory_interface, channel);
		return false;
	}
	return true;
}

bool set_i2s_adc_in(struct aud_digital_i2s *digtal_i2s)
{
	memcpy((void *)audio_adc_i2s, (void *)digtal_i2s,
	       sizeof(struct aud_digital_i2s));

	if (!audio_adc_i2s_status) {
		uint32_t sampling_rate = i2s_fs(audio_adc_i2s->i2s_sample_rate);
		uint32_t voice_mode = 0;

		afe_set_reg(AFE_ADDA_TOP_CON0, 0, 0x1);	/* Using Internal ADC */
		if (sampling_rate == MTAUD_I2S_SAMPLERATE_8K)
			voice_mode = 0;
		else if (sampling_rate == MTAUD_I2S_SAMPLERATE_16K)
			voice_mode = 1;
		else if (sampling_rate == MTAUD_I2S_SAMPLERATE_32K)
			voice_mode = 2;
		else if (sampling_rate == MTAUD_I2S_SAMPLERATE_48K)
			voice_mode = 3;

		afe_set_reg(AFE_ADDA_UL_SRC_CON0,
			    ((voice_mode << 2) | voice_mode) << 17,
			    0x001E0000);
		afe_set_reg(AFE_ADDA_NEWIF_CFG1,
			    ((voice_mode < 3) ? 1 : 3) << 10, 0x00000C00);

	} else {
		uint32_t audio_i2s_adc = 0;

		afe_set_reg(AFE_ADDA_TOP_CON0, 1, 0x1);	/* Using External ADC */
		audio_i2s_adc |= (audio_adc_i2s->lr_swap << 31);
		audio_i2s_adc |= (audio_adc_i2s->buffer_update_word << 24);
		audio_i2s_adc |= (audio_adc_i2s->inv_lrck << 23);
		audio_i2s_adc |= (audio_adc_i2s->fpga_bit_test << 22);
		audio_i2s_adc |= (audio_adc_i2s->fpga_bit << 21);
		audio_i2s_adc |= (audio_adc_i2s->loopback << 20);
		audio_i2s_adc |= (i2s_fs(audio_adc_i2s->i2s_sample_rate) << 8);
		audio_i2s_adc |= (audio_adc_i2s->i2s_fmt << 3);
		audio_i2s_adc |= (audio_adc_i2s->i2s_wlen << 1);
		pr_info("%s audio_i2s_adc = 0x%x", __func__, audio_i2s_adc);
		afe_set_reg(AFE_I2S_CON2, audio_i2s_adc, MASK_ALL);
	}
	return true;
}

bool set_i2s_adc_enable(bool enable)
{
	afe_set_reg(AFE_ADDA_UL_SRC_CON0, enable ? 1 : 0, 0x01);
	audio_memif[MTAUD_BLOCK_I2S_IN_ADC]->state = enable;
	if (enable) {
		afe_set_reg(AFE_ADDA_UL_DL_CON0, 0x0001, 0x0001);
	} else if
	(!audio_memif[MTAUD_BLOCK_I2S_OUT_DAC]->state &&
	 !audio_memif[MTAUD_BLOCK_I2S_IN_ADC]->state) {
		afe_set_reg(AFE_ADDA_UL_DL_CON0, 0x0000, 0x0001);
	}
	return true;
}

void clean_pre_distortion(void)
{
	pr_debug("%s\n", __func__);
	afe_set_reg(AFE_ADDA_PREDIS_CON0, 0, MASK_ALL);
	afe_set_reg(AFE_ADDA_PREDIS_CON1, 0, MASK_ALL);
}

bool set_dl_src2(uint32_t sample_rate)
{
	uint32_t afe_adda_dl_src2_con0, afe_adda_dl_src2_con1;

	if (likely(sample_rate == 44100))
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

	afe_set_reg(AFE_ADDA_DL_SRC2_CON0, afe_adda_dl_src2_con0, MASK_ALL);
	afe_set_reg(AFE_ADDA_DL_SRC2_CON1, afe_adda_dl_src2_con1, MASK_ALL);
	return true;
}

bool set_i2s_dac_out(uint32_t sample_rate)
{
	uint32_t audio_i2s_dac = 0;

	pr_debug("set_i2s_dac_out\n");
	clean_pre_distortion();
	set_dl_src2(sample_rate);

	audio_i2s_dac |= (MTAUD_LR_SWAP_NO << 31);
	audio_i2s_dac |= (i2s_fs(sample_rate) << 8);
	audio_i2s_dac |= (MTAUD_INV_LRCK_NO << 5);
	audio_i2s_dac |= (MTAUD_I2S_FORMAT_I2S << 3);
	audio_i2s_dac |= (MTAUD_I2S_WLEN_16BITS << 1);
	afe_set_reg(AFE_I2S_CON1, audio_i2s_dac, MASK_ALL);
	return true;
}

bool set_memory_path_enable(uint32_t aud_block, bool enable)
{
	if (unlikely(aud_block >= MTAUD_MEMIF_NUM))
		return true;

	if (enable) {
		audio_memif[aud_block]->state = true;
		afe_set_reg(AFE_DAC_CON0, enable << (aud_block + 1),
			    1 << (aud_block + 1));
	} else {
		afe_set_reg(AFE_DAC_CON0, enable << (aud_block + 1),
			    1 << (aud_block + 1));
		audio_memif[aud_block]->state = false;
	}
	return true;
}

bool set_i2s_dac_enable(bool enable)
{
	audio_memif[MTAUD_BLOCK_I2S_OUT_DAC]->state = enable;

	if (enable) {
		afe_set_reg(AFE_ADDA_DL_SRC2_CON0, enable, 0x01);
		afe_set_reg(AFE_I2S_CON1, enable, 0x1);
		afe_set_reg(AFE_ADDA_UL_DL_CON0, enable, 0x0001);
	} else {
		afe_set_reg(AFE_ADDA_DL_SRC2_CON0, enable, 0x01);
		afe_set_reg(AFE_I2S_CON1, enable, 0x1);

		if (!audio_memif[MTAUD_BLOCK_I2S_OUT_DAC]->state &&
		    !audio_memif[MTAUD_BLOCK_I2S_IN_ADC]->state) {
			afe_set_reg(AFE_ADDA_UL_DL_CON0, enable, 0x0001);
		}
	}
	return true;
}

bool get_i2s_dac_enable(void)
{
	return audio_memif[MTAUD_BLOCK_I2S_OUT_DAC]->state;
}

bool set_connection(uint32_t connection_state, uint32_t input, uint32_t output)
{
	return set_connection_state(connection_state, input, output);
}

bool set_irq_enable(uint32_t irqmode, bool enable)
{
	pr_debug("%s(), irqmode = %d, enable = %d\n", __func__,
		 irqmode, enable);

	switch (irqmode) {
	case MTAUD_IRQ_MCU_MODE_IRQ2:
		if (unlikely(!enable && check_uplink_memif_enable())) {
			/* IRQ2 is in used */
			pr_info("skip disable IRQ2, AFE_DAC_CON0 = 0x%x\n",
				afe_get_reg(AFE_DAC_CON0));
			break;
		}
	case MTAUD_IRQ_MCU_MODE_IRQ1:
	case MTAUD_IRQ_MCU_MODE_IRQ3:
		afe_set_reg(AFE_IRQ_MCU_CON,
			    (enable << irqmode), (1 << irqmode));
		audio_mcu_mode[irqmode]->status = enable;
		break;
	case MTAUD_IRQ_MCU_MODE_IRQ4:
		afe_set_reg(AFE_IRQ_MCU_CON,
			    (enable << 12), (1 << 12));
		audio_mcu_mode[irqmode]->status = enable;
		break;
	default:
		pr_warn("%s unexpected irqmode = %d\n", __func__, irqmode);
		break;
	}
	return true;
}

bool set_irq_mcu_sample_rate(uint32_t irqmode, uint32_t sample_rate)
{
	switch (irqmode) {
	case MTAUD_IRQ_MCU_MODE_IRQ1:
		afe_set_reg(AFE_IRQ_MCU_CON,
			    (i2s_fs(sample_rate) << 4), 0x000000f0);
		break;
	case MTAUD_IRQ_MCU_MODE_IRQ2:
		afe_set_reg(AFE_IRQ_MCU_CON,
			    (i2s_fs(sample_rate) << 8), 0x00000f00);
		break;
	case MTAUD_IRQ_MCU_MODE_IRQ4:
		break;
	case MTAUD_IRQ_MCU_MODE_IRQ3:
	default:
		return false;
	}
	return true;
}

bool set_irq_mcu_counter(uint32_t irqmode, uint32_t counter)
{
	switch (irqmode) {
	case MTAUD_IRQ_MCU_MODE_IRQ1:
		afe_set_reg(AFE_IRQ_CNT1, counter, 0xffffffff);
		break;
	case MTAUD_IRQ_MCU_MODE_IRQ2:
		afe_set_reg(AFE_IRQ_CNT2, counter, 0xffffffff);
		break;
	case MTAUD_IRQ_MCU_MODE_IRQ4:
		afe_set_reg(AFE_IRQ_CNT5, counter, 0xffffffff);
		break;
	case MTAUD_IRQ_MCU_MODE_IRQ3:
	default:
		return false;
	}
	return true;
}

bool get_irq_status(uint32_t irqmode, struct aud_irq_mcu_mode *mcumode)
{
	switch (irqmode) {
	case MTAUD_IRQ_MCU_MODE_IRQ1:
	case MTAUD_IRQ_MCU_MODE_IRQ2:
	case MTAUD_IRQ_MCU_MODE_IRQ3:
	case MTAUD_IRQ_MCU_MODE_IRQ4:
		memcpy((void *)mcumode, (const void *)audio_mcu_mode[irqmode],
		       sizeof(struct aud_irq_mcu_mode));
		break;
	default:
		return false;
	}
	return true;
}



