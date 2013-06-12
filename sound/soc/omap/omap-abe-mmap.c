/*
 * omap-abe.c  --  OMAP ALSA SoC DAI driver using Audio Backend
 *
 * Copyright (C) 2010 Texas Instruments
 *
 * Contact: Liam Girdwood <lrg@ti.com>
 *          Misael Lopez Cruz <misael.lopez@ti.com>
 *          Sebastien Guiriec <s-guiriec@ti.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 */

#include <linux/pm_runtime.h>


#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>

#include "omap-abe-priv.h"
#include "omap-pcm.h"

int abe_pm_save_context(struct omap_abe *abe);
int abe_pm_restore_context(struct omap_abe *abe);
int abe_opp_recalc_level(struct omap_abe *abe);
int abe_opp_set_level(struct omap_abe *abe, int opp);

/* Ping pong buffer DMEM offset - we should read this from future FWs */
#define OMAP_ABE_DMEM_BASE_OFFSET_PING_PONG	0x4000

static const struct snd_pcm_hardware omap_abe_hardware = {
	.info			= SNDRV_PCM_INFO_MMAP |
				  SNDRV_PCM_INFO_MMAP_VALID |
				  SNDRV_PCM_INFO_INTERLEAVED |
				  SNDRV_PCM_INFO_BLOCK_TRANSFER |
				  SNDRV_PCM_INFO_PAUSE |
				  SNDRV_PCM_INFO_RESUME,
	.formats		= SNDRV_PCM_FMTBIT_S16_LE |
				  SNDRV_PCM_FMTBIT_S32_LE,
	.period_bytes_min	= 4 * 1024,
	.period_bytes_max	= 24 * 1024,
	.periods_min		= 4,
	.periods_max		= 4,
	.buffer_bytes_max	= 24 * 1024 * 2,
};

/*
 * omap_aess_irq_data
 *
 * IRQ FIFO content declaration
 *	APS interrupts : IRQ_FIFO[31:28] = IRQtag_APS,
 *		IRQ_FIFO[27:16] = APS_IRQs, IRQ_FIFO[15:0] = loopCounter
 *	SEQ interrupts : IRQ_FIFO[31:28] OMAP_ABE_IRQTAG_COUNT,
 *		IRQ_FIFO[27:16] = Count_IRQs, IRQ_FIFO[15:0] = loopCounter
 *	Ping-Pong Interrupts : IRQ_FIFO[31:28] = OMAP_ABE_IRQTAG_PP,
 *		IRQ_FIFO[27:16] = PP_MCU_IRQ, IRQ_FIFO[15:0] = loopCounter
 */
struct omap_aess_irq_data {
	unsigned int counter:16;
	unsigned int data:12;
	unsigned int tag:4;
};

#define OMAP_ABE_IRQTAG_COUNT	0x000c
#define OMAP_ABE_IRQTAG_PP	0x000d
#define OMAP_ABE_IRQ_FIFO_MASK	((OMAP_ABE_D_MCUIRQFIFO_SIZE >> 2) - 1)

static void abe_irq_pingpong_subroutine(struct snd_pcm_substream *substream, struct omap_abe *abe)
{

	u32 dst, n_bytes;

	omap_aess_read_next_ping_pong_buffer(abe->aess, OMAP_ABE_MM_DL_PORT, &dst, &n_bytes);
	omap_aess_set_ping_pong_buffer(abe->aess, OMAP_ABE_MM_DL_PORT, n_bytes);

	/* 1st IRQ does not signal completed period */
	if (abe->mmap.first_irq) {
		abe->mmap.first_irq = 0;
	} else {
		if (substream)
			snd_pcm_period_elapsed(substream);
	}
}


irqreturn_t abe_irq_handler(int irq, void *dev_id)
{
	struct omap_abe *abe = dev_id;
	struct omap_aess_addr addr;
	struct omap_aess *aess = abe->aess;
	struct omap_aess_irq_data IRQ_data;
	u32 abe_irq_dbg_write_ptr, i, cmem_src, sm_cm;

	pm_runtime_get_sync(abe->dev);
	omap_aess_clear_irq(abe->aess);

	/* extract the write pointer index from CMEM memory (INITPTR format) */
	/* CMEM address of the write pointer in bytes */
	cmem_src = aess->fw_info->label_id[OMAP_AESS_BUFFER_MCU_IRQ_FIFO_PTR_ID] << 2;
	omap_abe_mem_read(aess, OMAP_ABE_CMEM, cmem_src,
			  &sm_cm, sizeof(abe_irq_dbg_write_ptr));
	/* AESS left-pointer index located on MSBs */
	abe_irq_dbg_write_ptr = sm_cm >> 16;
	abe_irq_dbg_write_ptr &= 0xFF;
	/* loop on the IRQ FIFO content */
	for (i = 0; i < OMAP_ABE_D_MCUIRQFIFO_SIZE; i++) {
		/* stop when the FIFO is empty */
		if (abe_irq_dbg_write_ptr == aess->irq_dbg_read_ptr)
			break;
		/* read the IRQ/DBG FIFO */
		memcpy(&addr, &aess->fw_info->map[OMAP_AESS_DMEM_MCUIRQFIFO_ID],
		       sizeof(struct omap_aess_addr));
		addr.offset += (aess->irq_dbg_read_ptr << 2);
		addr.bytes = sizeof(IRQ_data);
		omap_aess_mem_read(aess, addr, (u32 *)&IRQ_data);
		aess->irq_dbg_read_ptr = (aess->irq_dbg_read_ptr + 1) & OMAP_ABE_IRQ_FIFO_MASK;
		/* select the source of the interrupt */
		switch (IRQ_data.tag) {
		case OMAP_ABE_IRQTAG_PP:
			/* first IRQ doesn't represent a buffer transference completion */
			if (aess->pp_first_irq)
				aess->pp_first_irq = 0;
			else
				aess->pp_buf_id = (aess->pp_buf_id + 1) & 0x03;

			abe_irq_pingpong_subroutine(abe->dai.port[OMAP_ABE_FE_PORT_MM_DL_LP]->substream,
						    abe);

			break;
		case OMAP_ABE_IRQTAG_COUNT:
			/*omap_aess_monitoring(aess);*/
			break;
		default:
			break;
		}

	}

	pm_runtime_put_sync_suspend(abe->dev);
	return IRQ_HANDLED;
}

static int omap_abe_hwrule_size_step(struct snd_pcm_hw_params *params,
					struct snd_pcm_hw_rule *rule)
{
	unsigned int rate = params_rate(params);

	/* 44.1kHz has the same iteration number as 48kHz */
	rate = (rate == 44100) ? 48000 : rate;

	/* ABE requires chunks of 250us worth of data */
	return snd_interval_step(hw_param_interval(params, rule->var), 0,
				 rate / 4000);
}

static int aess_open(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_platform *platform = rtd->platform;
	struct omap_abe *abe = snd_soc_platform_get_drvdata(platform);
	struct snd_soc_dai *dai = rtd->cpu_dai;
	int ret = 0;

	mutex_lock(&abe->mutex);

	dev_dbg(dai->dev, "%s: %s\n", __func__, dai->name);

	switch (dai->id) {
	case OMAP_ABE_FRONTEND_DAI_MODEM:
		break;
	case OMAP_ABE_FRONTEND_DAI_LP_MEDIA:
		snd_soc_set_runtime_hwparams(substream, &omap_abe_hardware);
		ret = snd_pcm_hw_constraint_step(substream->runtime, 0,
					 SNDRV_PCM_HW_PARAM_BUFFER_BYTES, 1024);
		break;
	default:
		/*
		 * Period and buffer size must be aligned with the Audio Engine
		 * processing loop which is 250 us long
		 */
		ret = snd_pcm_hw_rule_add(substream->runtime, 0,
					SNDRV_PCM_HW_PARAM_PERIOD_SIZE,
					omap_abe_hwrule_size_step,
					NULL,
					SNDRV_PCM_HW_PARAM_PERIOD_SIZE, -1);
		ret = snd_pcm_hw_rule_add(substream->runtime, 0,
					SNDRV_PCM_HW_PARAM_BUFFER_SIZE,
					omap_abe_hwrule_size_step,
					NULL,
					SNDRV_PCM_HW_PARAM_BUFFER_SIZE, -1);
		break;
	}

	if (ret < 0) {
		dev_err(abe->dev, "failed to set period constraints for DAI %d\n",
			dai->id);
		goto out;
	}

	pm_runtime_get_sync(abe->dev);

	if (!abe->active++) {
		abe->opp.level = 0;
		abe_pm_restore_context(abe);
		abe_opp_set_level(abe, 100);
		omap_aess_wakeup(abe->aess);
	}

out:
	mutex_unlock(&abe->mutex);
	return ret;
}

static int aess_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct snd_soc_platform *platform = rtd->platform;
	struct omap_abe *abe = snd_soc_platform_get_drvdata(platform);
	struct snd_soc_dai *dai = rtd->cpu_dai;
	struct omap_aess_data_format format;
	size_t period_size;
	u32 dst;
	int ret = 0;

	mutex_lock(&abe->mutex);

	dev_dbg(dai->dev, "%s: %s\n", __func__, dai->name);

	if (dai->id != OMAP_ABE_FRONTEND_DAI_LP_MEDIA)
		goto out;

	format.f = params_rate(params);
	if (params_format(params) == SNDRV_PCM_FORMAT_S32_LE)
		format.samp_format = OMAP_AESS_FORMAT_STEREO_MSB;
	else
		format.samp_format = OMAP_AESS_FORMAT_STEREO_16_16;

	period_size = params_period_bytes(params);

	/* Connect a Ping-Pong cache-flush protocol to MM_DL port */
	omap_aess_connect_irq_ping_pong_port(abe->aess, OMAP_ABE_MM_DL_PORT, &format,
				0,
				period_size, &dst,
				PING_PONG_WITH_MCU_IRQ);

	/* Memory mapping for hw params */
	runtime->dma_area  = abe->io_base[0] + dst;
	runtime->dma_addr  = 0;
	runtime->dma_bytes = period_size * 4;

	/* Need to set the first buffer in order to get interrupt */
	omap_aess_set_ping_pong_buffer(abe->aess, OMAP_ABE_MM_DL_PORT, period_size);
	abe->mmap.first_irq = 1;

out:
	mutex_unlock(&abe->mutex);
	return ret;
}

static int aess_prepare(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_platform *platform = rtd->platform;
	struct omap_abe *abe = snd_soc_platform_get_drvdata(platform);
	struct snd_soc_dai *dai = rtd->cpu_dai;

	dev_dbg(dai->dev, "%s: %s\n", __func__, dai->name);

	mutex_lock(&abe->mutex);
	/*
	 * TODO: do we need to set OPP here ? e.g.
	 * Startup() -> OPP100
	 * aess_prepare() -> OPP0 (as widgets are not updated until after)
	 * stream_event() -> OPP25/50/100 (correct value based on widgets)
	 * abe_opp_recalc_level(abe);
	 */
	mutex_unlock(&abe->mutex);
	return 0;
}

static int aess_close(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_platform *platform = rtd->platform;
	struct omap_abe *abe = snd_soc_platform_get_drvdata(platform);
	struct snd_soc_dai *dai = rtd->cpu_dai;

	dev_dbg(dai->dev, "%s: %s\n", __func__, dai->name);

	mutex_lock(&abe->mutex);

	if (dai->id != OMAP_ABE_FRONTEND_DAI_LP_MEDIA) {
	}

	if (!--abe->active) {
		omap_aess_disable_irq(abe->aess);
		abe_pm_save_context(abe);
		omap_abe_pm_shutdown(platform);
	} else {
		/* Only scale OPP level
		 * if ABE is still active */
		abe_opp_recalc_level(abe);
	}
	pm_runtime_put_sync(abe->dev);

	mutex_unlock(&abe->mutex);
	return 0;
}

static int aess_mmap(struct snd_pcm_substream *substream,
	struct vm_area_struct *vma)
{
	struct snd_soc_pcm_runtime  *rtd = substream->private_data;
	struct snd_soc_dai *dai = rtd->cpu_dai;
	int offset, size, err;

	if (dai->id != OMAP_ABE_FRONTEND_DAI_LP_MEDIA)
		return -EINVAL;

	vma->vm_flags |= VM_IO;
	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
	size = vma->vm_end - vma->vm_start;
	offset = vma->vm_pgoff << PAGE_SHIFT;

	err = io_remap_pfn_range(vma, vma->vm_start,
			(ABE_DEFAULT_BASE_ADDRESS_L4 + ABE_DMEM_BASE_OFFSET_MPU +
			OMAP_ABE_DMEM_BASE_OFFSET_PING_PONG + offset) >> PAGE_SHIFT,
			size, vma->vm_page_prot);
	if (err)
		return -EAGAIN;

	return 0;
}

static snd_pcm_uframes_t aess_pointer(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_platform *platform = rtd->platform;
	struct omap_abe *abe = snd_soc_platform_get_drvdata(platform);
	snd_pcm_uframes_t offset = 0;
	u32 pingpong;

	if (!abe->mmap.first_irq) {
		omap_aess_read_offset_from_ping_buffer(abe->aess, OMAP_ABE_MM_DL_PORT, &pingpong);
		offset = (snd_pcm_uframes_t)pingpong;
	}

	return offset;
}

struct snd_pcm_ops omap_aess_pcm_ops = {
	.open           = aess_open,
	.hw_params	= aess_hw_params,
	.prepare	= aess_prepare,
	.close	        = aess_close,
	.pointer	= aess_pointer,
	.mmap		= aess_mmap,
};
