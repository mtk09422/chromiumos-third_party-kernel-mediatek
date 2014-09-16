/*
 * mt_soc_pcm_dl1.c  --  Mediatek ALSA SoC DL1 playback platform driver
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
#include "mt_soc_digital_type.h"
#include <linux/dma-mapping.h>
#include <linux/module.h>
#include <linux/spinlock.h>
#include <sound/soc.h>
#include <linux/sched.h>
#include <linux/cgroup.h>
#include <linux/pid.h>


/*
 *    function implementation
 */
static int mtk_afe_probe(struct platform_device *pdev);
static int mtk_pcm_close(struct snd_pcm_substream *substream);
static int mtk_pcm_dl1_prestart(struct snd_pcm_substream *substream);

/* defaults */
#define MAX_BUFFER_SIZE     (16*1024)
#define MIN_PERIOD_SIZE     64
#define MAX_PERIOD_SIZE     MAX_BUFFER_SIZE
#define USE_FORMATS         (SNDRV_PCM_FMTBIT_U8 | SNDRV_PCM_FMTBIT_S16_LE)
#define USE_RATE        (SNDRV_PCM_RATE_CONTINUOUS | SNDRV_PCM_RATE_8000_48000)
#define USE_RATE_MIN        8000
#define USE_RATE_MAX        48000
#define USE_CHANNELS_MIN    1
#define USE_CHANNELS_MAX    2
#define USE_PERIODS_MIN     512
#define USE_PERIODS_MAX     8192

static const struct snd_pcm_hardware mtk_pcm_hardware = {
	.info = (SNDRV_PCM_INFO_MMAP |
			 SNDRV_PCM_INFO_INTERLEAVED |
			 SNDRV_PCM_INFO_RESUME |
			 SNDRV_PCM_INFO_MMAP_VALID),
	.formats = USE_FORMATS,
	.rates = USE_RATE,
	.rate_min = USE_RATE_MIN,
	.rate_max = USE_RATE_MAX,
	.channels_min = USE_CHANNELS_MIN,
	.channels_max = USE_CHANNELS_MAX,
	.buffer_bytes_max = MAX_BUFFER_SIZE,
	.period_bytes_max = MAX_PERIOD_SIZE,
	.periods_min = 1,
	.periods_max = 4096,
	.fifo_size = 0,
};

static int mtk_pcm_dl1_stop(struct snd_pcm_substream *substream)
{
	pr_debug("%s\n", __func__);

	set_memory_path_enable(MTAUD_BLOCK_MEM_DL1, false);
	set_irq_enable(MTAUD_IRQ_MCU_MODE_IRQ1, false);

	/* clean audio hardware buffer */
	aud_drv_reset_buffer(MTAUD_BLOCK_MEM_DL1);

	return 0;
}

static int mtk_pcm_dl1_post_stop(struct snd_pcm_substream *substream)
{
	pr_debug("%s\n", __func__);

	if (!get_i2s_dac_enable())
		set_i2s_dac_enable(false);

	/* here to turn off digital part */
	set_connection(MTAUD_DISCONNECT, MTAUD_INTERCONNECTION_I05,
		       MTAUD_INTERCONNECTION_O03);
	set_connection(MTAUD_DISCONNECT, MTAUD_INTERCONNECTION_I06,
		       MTAUD_INTERCONNECTION_O04);

	enable_afe(false);
	return 0;
}

static snd_pcm_uframes_t mtk_pcm_pointer(struct snd_pcm_substream *substream)
{
	signed int hw_ptr;

	hw_ptr = aud_drv_update_hw_ptr(MTAUD_BLOCK_MEM_DL1, substream);

	return hw_ptr;
}

static int mtk_pcm_hw_params(struct snd_pcm_substream *substream,
			     struct snd_pcm_hw_params *hw_params)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct snd_dma_buffer *dma_buf = &substream->dma_buffer;
	int ret = 0;

	pr_debug("%s\n", __func__);
	if (unlikely(params_buffer_bytes(hw_params) > afe_sram_size)) {
		pr_warn("%s request size %d > max size %d\n", __func__,
			params_buffer_bytes(hw_params), afe_sram_size);
	}

	/* here to allcoate sram to hardware --------------------------- */
	aud_drv_allocate_dl1_buffer(afe_sram_size);
	substream->runtime->dma_bytes = afe_sram_size;
	substream->runtime->dma_area = (unsigned char *)afe_sram_address;
	substream->runtime->dma_addr = afe_sram_phy_address;
	dma_buf->dev.type = SNDRV_DMA_TYPE_DEV;
	dma_buf->dev.dev = substream->pcm->card->dev;
	dma_buf->private_data = NULL;
	pr_debug("%s dma_bytes = %d dma_area = %p dma_addr = 0x%x\n",
		 __func__, substream->runtime->dma_bytes,
		 substream->runtime->dma_area, substream->runtime->dma_addr);

	pr_debug("%s runtime rate = %d channels = %d substream->pcm->device = %d\n",
		 __func__, runtime->rate, runtime->channels,
		 substream->pcm->device);
	return ret;
}

static int mtk_pcm_hw_free(struct snd_pcm_substream *substream)
{
	pr_debug("%s\n", __func__);
	return 0;
}

/* Conventional and unconventional sample rate supported */
static const unsigned int supported_sample_rates[] = {
	8000, 11025, 12000, 16000, 22050, 24000, 32000, 44100, 48000
};

static struct snd_pcm_hw_constraint_list constraints_sample_rates = {
	.count = ARRAY_SIZE(supported_sample_rates),
	.list = supported_sample_rates,
	.mask = 0,
};

static int mtk_pcm_open(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct pcm_afe_info *prtd = NULL;
	int ret = 0;

	pr_debug("%s\n", __func__);

	aud_drv_dl_set_platform_info(MTAUD_BLOCK_MEM_DL1, substream);

	prtd = kzalloc(sizeof(*prtd), GFP_KERNEL);

	if (unlikely(!prtd)) {
		pr_err("%s failed to allocate memory\n", __func__);
		return -ENOMEM;
	}

	prtd->substream = substream;
	prtd->prepared = false;
	runtime->hw = mtk_pcm_hardware;

	memcpy((void *)(&(runtime->hw)), (void *)&mtk_pcm_hardware,
	       sizeof(struct snd_pcm_hardware));

	runtime->private_data = prtd;

	pr_info("%s rates= 0x%x mtk_pcm_hardware= %p hw= %p\n",
		__func__, runtime->hw.rates, &mtk_pcm_hardware, &runtime->hw);

	ret = snd_pcm_hw_constraint_list(runtime, 0, SNDRV_PCM_HW_PARAM_RATE,
					 &constraints_sample_rates);
	if (unlikely(ret < 0))
		pr_err("snd_pcm_hw_constraint_list failed: 0x%x\n", ret);

	ret = snd_pcm_hw_constraint_integer(runtime,
					    SNDRV_PCM_HW_PARAM_PERIODS);
	if (unlikely(ret < 0))
		pr_err("snd_pcm_hw_constraint_integer failed: 0x%x\n", ret);

	/* here open audio clocks */
	aud_drv_clk_on();

	/* print for hw pcm information */
	pr_debug("%s runtime rate = %d channels = %d substream->pcm->device = %d\n",
		 __func__, runtime->rate, runtime->channels,
		 substream->pcm->device);

	runtime->hw.info |= (SNDRV_PCM_INFO_INTERLEAVED |
				SNDRV_PCM_INFO_NONINTERLEAVED |
				SNDRV_PCM_INFO_MMAP_VALID);

	if (unlikely(ret < 0)) {
		pr_err("%s mtk_pcm_close\n", __func__);
		mtk_pcm_close(substream);
		return ret;
	}
	pr_debug("%s return\n", __func__);
	return 0;
}

static int mtk_pcm_close(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct pcm_afe_info *prtd = runtime->private_data;

	pr_info("%s\n", __func__);
	aud_drv_dl_reset_platform_info(MTAUD_BLOCK_MEM_DL1);
	mtk_pcm_dl1_post_stop(substream);

	aud_drv_clk_off();
	kfree(prtd);
	return 0;
}

static int mtk_pcm_prepare(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime_stream = substream->runtime;
	struct pcm_afe_info *prtd = NULL;

	pr_debug("%s rate= %u channels= %u period_size= %lu\n",
		 __func__, runtime_stream->rate, runtime_stream->channels,
		 runtime_stream->period_size);

	/* HW sequence: */
	/* mtk_pcm_dl1_prestart->codec->mtk_pcm_dl1_start */
	prtd = runtime_stream->private_data;
	if (likely(!prtd->prepared)) {
		mtk_pcm_dl1_prestart(substream);
		prtd->prepared = true;
	}
	return 0;
}

static int mtk_pcm_dl1_prestart(struct snd_pcm_substream *substream)
{
	/* here start digital part */
	set_connection(MTAUD_CONNECT, MTAUD_INTERCONNECTION_I05,
		       MTAUD_INTERCONNECTION_O03);
	set_connection(MTAUD_CONNECT, MTAUD_INTERCONNECTION_I06,
		       MTAUD_INTERCONNECTION_O04);

	/* start I2S DAC out */
	set_i2s_dac_out(substream->runtime->rate);
	set_memory_path_enable(MTAUD_BLOCK_I2S_OUT_DAC, true);
	set_i2s_dac_enable(true);

	enable_afe(true);
	return 0;
}

static int mtk_pcm_dl1_start(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime * const runtime = substream->runtime;
	struct timespec curr_tstamp;

	pr_debug("%s period = %lu,runtime->rate= %u, runtime->channels=%u\n",
		 __func__, runtime->period_size, runtime->rate,
		 runtime->channels);

	/* set dl1 sample ratelimit_state */
	set_sample_rate(MTAUD_BLOCK_MEM_DL1, runtime->rate);
	set_channels(MTAUD_BLOCK_MEM_DL1, runtime->channels);

	/* here to set interrupt */
	set_irq_mcu_counter(MTAUD_IRQ_MCU_MODE_IRQ1,
			    runtime->period_size);
	set_irq_mcu_sample_rate(MTAUD_IRQ_MCU_MODE_IRQ1,
				runtime->rate);

	set_memory_path_enable(MTAUD_BLOCK_MEM_DL1, true);
	set_irq_enable(MTAUD_IRQ_MCU_MODE_IRQ1, true);
	snd_pcm_gettime(substream->runtime, (struct timespec *)&curr_tstamp);

	pr_debug("%s curr_tstamp %ld %ld\n", __func__,
		 curr_tstamp.tv_sec, curr_tstamp.tv_nsec);

	return 0;
}

static int mtk_pcm_trigger(struct snd_pcm_substream *substream, int cmd)
{
	pr_info("%s cmd=%d\n", __func__, cmd);

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
		return mtk_pcm_dl1_start(substream);
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
		return mtk_pcm_dl1_stop(substream);
	}
	return -EINVAL;
}

#define mtk_afe_suspend	NULL
#define mtk_afe_resume	NULL


static struct snd_pcm_ops mtk_afe_ops = {
	.open = mtk_pcm_open,
	.close = mtk_pcm_close,
	.ioctl = snd_pcm_lib_ioctl,
	.hw_params = mtk_pcm_hw_params,
	.hw_free = mtk_pcm_hw_free,
	.prepare = mtk_pcm_prepare,
	.trigger = mtk_pcm_trigger,
	.pointer = mtk_pcm_pointer,
};

static struct snd_soc_platform_driver mtk_soc_platform = {
	.ops = &mtk_afe_ops,
	.suspend = mtk_afe_suspend,
	.resume = mtk_afe_resume,
};

static int mtk_afe_probe(struct platform_device *pdev)
{
	int ret = 0;

	pr_notice("AudDrv %s: dev name %s\n", __func__, dev_name(&pdev->dev));

	if (pdev->dev.of_node) {
		dev_set_name(&pdev->dev, "%s", "mt-soc-dl1-pcm");
		pr_notice("%s set dev name %s\n", __func__,
			  dev_name(&pdev->dev));
	}

	/* for PDN_AFE default 0, disable it. */
	aud_drv_clk_off();

	ret = do_platform_init(&pdev->dev);
	if (ret)
		return ret;

	return snd_soc_register_platform(&pdev->dev, &mtk_soc_platform);
}

static int mtk_afe_remove(struct platform_device *pdev)
{
	pr_debug("%s\n", __func__);
	snd_soc_unregister_platform(&pdev->dev);
	return 0;
}

static const struct of_device_id mtk_dl1_dt_match[] = {
	{ .compatible = "mediatek,mt-soc-dl1-pcm", },
	{ }
};
MODULE_DEVICE_TABLE(of, mtk_dl1_dt_match);

static struct platform_driver mtk_afe_driver = {
	.driver = {
		   .name = "mt-soc-dl1-pcm",
		   .owner = THIS_MODULE,
		   .of_match_table = mtk_dl1_dt_match,
		   },
	.probe = mtk_afe_probe,
	.remove = mtk_afe_remove,
};

module_platform_driver(mtk_afe_driver);

MODULE_DESCRIPTION("AFE PCM module platform driver");
MODULE_LICENSE("GPL");
