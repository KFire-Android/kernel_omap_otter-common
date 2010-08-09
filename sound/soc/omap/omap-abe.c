/*
 * omap-abe.c  --  OMAP ALSA SoC DAI driver using Audio Backend
 *
 * Copyright (C) 2010 Texas Instruments
 *
 * Contact: Liam Girdwood <lrg@slimlogic.co.uk>
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

#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/workqueue.h>
#include <linux/spinlock.h>
#include <linux/delay.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/initval.h>
#include <sound/soc.h>

#include <plat/control.h>
#include <plat/dma-44xx.h>
#include <plat/dma.h>
#include "omap-mcpdm.h"
#include "omap-pcm.h"
#include "omap-abe.h"
#include "omap-abe-dsp.h"
#include "abe/abe_main.h"


#define OMAP_ABE_FORMATS	(SNDRV_PCM_FMTBIT_S32_LE)

/*
 * This is the AESS frontend driver. i.e. the part that Tx and Rx PCM
 * data via the OMAP sDMA and ALSA userspace PCMs.
 *
 * TODO:
 *
 * 1) Get DAI params from HAL and pass onto DAI drivers ? (are params static ??, i.e. can I just use a table)
 */

#define NUM_ABE_FRONTENDS		5
#define NUM_ABE_BACKENDS		11

#if 0
/* Frontend DAI Playback / Capture Support Matrix */
static const struct {
	int dir[2];
} fe_stream[] = {
	{{1, 1,},}, /* playback and capture supported on PCM0 - Multimedia*/
	{{0, 1,},}, /* capture only supported on PCM1 - Multimedia capture */
	{{1, 1,},}, /* playback and capture supported on PCM2 - Voice */
	{{1, 0,},}, /* playback only supported on PCM3 - Tones */
	{{1, 0,},}, /* playback only supported on PCM4  - Vibra */
};

/* Backend DAI Playback / Capture Support Matrix */
static const struct {
	const char *id;
	int dir[2];
} be_stream[] = {
	{OMAP_ABE_BE_PDM_UL1, {0, 1,},}, /* capture only supported on PDM UL*/
	{OMAP_ABE_BE_PDM_DL1, {1, 0,},}, /* playback only supported on PDM DL1 */
	{OMAP_ABE_BE_PDM_DL2, {1, 0,},}, /* playback only supported on PDM DL2 */
	{OMAP_ABE_BE_PDM_VIB, {1, 0,},}, /* playback only supported on PDM VIB */
	{OMAP_ABE_BE_BT_VX, {1, 0,},}, /* playback supported on BT VX_DL */
	{OMAP_ABE_BE_BT_VX, {0, 1,},}, /* playback and capture supported on BT VX_UL */
	{OMAP_ABE_BE_MM_EXT0, {1, 0,},}, /* playback supported on MM EXT_DL */
	{OMAP_ABE_BE_MM_EXT0, {0, 1,},}, /* capture supported on MM EXT_UL */
	{OMAP_ABE_BE_DMIC0, {0, 1,},}, /* capture only supported on DMIC0 */
	{OMAP_ABE_BE_DMIC1, {0, 1,},}, /* capture only supported on DMIC1 */
	{OMAP_ABE_BE_DMIC2, {0, 1,},}, /* capture only supported on DMIC2 */
};
#endif

struct abe_frontend_dai {
	spinlock_t lock;

	/* trigger work */
	struct work_struct work;
	int cmd;
	struct snd_pcm_substream *substream;
};

struct omap_abe_data {
	struct abe_frontend_dai frontend[NUM_ABE_FRONTENDS][2];
	int be_active[NUM_ABE_BACKENDS][2];

	int configure;
	atomic_t mcpdm_dl_cnt;  /* McPDM DL port activation counter */
	atomic_t mcpdm_ul_cnt;  /* McPDM UL port activation counter */
	atomic_t mm_dl_cnt;     /* MM DL port activation counter */

	struct clk *clk;

	/* hwmod platform device */
	struct platform_device *pdev;
};

static struct omap_abe_data abe_data = {

	/*ABE_FRONTEND_DAI_MEDIA */
	.frontend[ABE_FRONTEND_DAI_MEDIA][SNDRV_PCM_STREAM_CAPTURE] = {
			.lock = SPIN_LOCK_UNLOCKED,
	},
	.frontend[ABE_FRONTEND_DAI_MEDIA][SNDRV_PCM_STREAM_PLAYBACK] = {
			.lock = SPIN_LOCK_UNLOCKED,
	},

	/* ABE_FRONTEND_DAI_MEDIA_CAPTURE */
	.frontend[ABE_FRONTEND_DAI_MEDIA_CAPTURE][SNDRV_PCM_STREAM_PLAYBACK] = {
			.lock = SPIN_LOCK_UNLOCKED,
	},

	/* ABE_FRONTEND_DAI_VOICE */
	.frontend[ABE_FRONTEND_DAI_VOICE][SNDRV_PCM_STREAM_PLAYBACK] = {
			.lock = SPIN_LOCK_UNLOCKED,
	},
	.frontend[ABE_FRONTEND_DAI_VOICE][SNDRV_PCM_STREAM_PLAYBACK] = {
			.lock = SPIN_LOCK_UNLOCKED,
	},

	/* BT_VX_UL */
	.frontend[ABE_FRONTEND_DAI_TONES][SNDRV_PCM_STREAM_CAPTURE] = {
			.lock = SPIN_LOCK_UNLOCKED,
	},

	/* MODEM */
	.frontend[ABE_FRONTEND_DAI_VIBRA][SNDRV_PCM_STREAM_PLAYBACK] = {
			.lock = SPIN_LOCK_UNLOCKED,
	},

	.mcpdm_dl_cnt = ATOMIC_INIT(0),
	.mcpdm_ul_cnt = ATOMIC_INIT(0),
	.mm_dl_cnt = ATOMIC_INIT(0),
	.configure = 0,
};

/* frontend mutex */
static DEFINE_MUTEX(fe_mutex);

/*
 * Stream DMA parameters
 * FIXME: make this array[5][2] and put in priv data, otherwise we clobber it
 * upon opening new FEs.
 */
static struct omap_pcm_dma_data omap_abe_dai_dma_params[] = {
	{
		.name = "Audio Playback",
		.dma_req = OMAP44XX_DMA_ABE_REQ_0,
		.data_type = OMAP_DMA_DATA_TYPE_S32,
		.sync_mode = OMAP_DMA_SYNC_PACKET,
	},
	{
		.name = "Audio Capture",
		.dma_req = OMAP44XX_DMA_ABE_REQ_2,
		.data_type = OMAP_DMA_DATA_TYPE_S32,
		.sync_mode = OMAP_DMA_SYNC_PACKET,
	},
};

/* Frontend PCM Operations */

static int abe_fe_startup(struct snd_pcm_substream *substream,
			struct snd_soc_dai *dai)
{
	//struct snd_soc_pcm_runtime *rtd = substream->private_data;
	//struct snd_soc_codec *codec = rtd->codec;
	struct omap_abe_data *priv = snd_soc_dai_get_drvdata(dai);
	//struct twl4030_codec_data *pdata = codec->dev->platform_data;
	//struct platform_device *pdev = priv->pdev;

	/* TODO: complete HW  pcm for backends */
#if 0
	snd_pcm_hw_constraint_list(substream->runtime, 0,
				SNDRV_PCM_HW_PARAM_RATE,
				priv->sysclk_constraints);
#endif

	if (!priv->configure++) {

		//if (pdata->device_enable)
		//	pdata->device_enable(pdev);

		// TODO: replace with 6 Mux config in userspace
		abe_set_router_configuration(UPROUTE, UPROUTE_CONFIG_AMIC,
			(abe_router_t *)abe_router_ul_table_preset[UPROUTE_CONFIG_AMIC]);

		// TODO: move to UCM control
		abe_write_gain(GAINS_DL1, GAIN_M6dB,  RAMP_0MS, GAIN_LEFT_OFFSET);
		abe_write_gain(GAINS_DL1, GAIN_M6dB,  RAMP_0MS, GAIN_RIGHT_OFFSET);
		abe_write_gain(GAINS_DL2, GAIN_M6dB,  RAMP_0MS, GAIN_LEFT_OFFSET);
		abe_write_gain(GAINS_DL2, GAIN_M6dB,  RAMP_0MS, GAIN_RIGHT_OFFSET);

		abe_write_gain(GAINS_AMIC, GAIN_M6dB, RAMP_0MS, GAIN_LEFT_OFFSET);
		abe_write_gain(GAINS_AMIC, GAIN_M6dB, RAMP_0MS, GAIN_RIGHT_OFFSET);

		abe_write_gain(GAINS_SPLIT, GAIN_M6dB, RAMP_0MS, GAIN_LEFT_OFFSET);
		abe_write_gain(GAINS_SPLIT, GAIN_M6dB, RAMP_0MS, GAIN_RIGHT_OFFSET);
	}

	return 0;
}

static int abe_fe_hw_params(struct snd_pcm_substream *substream,
			struct snd_pcm_hw_params *params,
			struct snd_soc_dai *dai)
{
	abe_data_format_t format;
	abe_dma_t dma_sink;
	abe_dma_t dma_params;
	int dma_req;

	switch (params_channels(params)) {
	case 1:
		format.samp_format = MONO_MSB;
		break;
	case 2:
		format.samp_format = STEREO_MSB;
		break;
	default:
		dev_err(dai->dev, "%d channels not supported",
			params_channels(params));
		return -EINVAL;
	}

	format.f = params_rate(params);

	switch (dai->id) {
	case ABE_FRONTEND_DAI_MEDIA:
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
			abe_read_port_address(MM_DL_PORT, &dma_params);
			dma_req = OMAP44XX_DMA_ABE_REQ_0;
			abe_connect_cbpr_dmareq_port(MM_DL_PORT, &format, ABE_CBPR0_IDX,
					&dma_sink);
		} else {
			abe_read_port_address(MM_UL2_PORT, &dma_params);
			dma_req = OMAP44XX_DMA_ABE_REQ_4;
			abe_connect_cbpr_dmareq_port(MM_UL2_PORT, &format,  ABE_CBPR4_IDX,
					&dma_sink);
		}
        break;
	case ABE_FRONTEND_DAI_MEDIA_CAPTURE:
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
			return -EINVAL;
		else {
			abe_read_port_address(MM_UL_PORT, &dma_params);
			dma_req = OMAP44XX_DMA_ABE_REQ_3;
			abe_connect_cbpr_dmareq_port(MM_UL_PORT, &format,  ABE_CBPR3_IDX,
					&dma_sink);
		}
        break;
	case ABE_FRONTEND_DAI_VOICE:
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
			abe_read_port_address(VX_DL_PORT, &dma_params);
			dma_req = OMAP44XX_DMA_ABE_REQ_1;
			abe_connect_cbpr_dmareq_port(VX_DL_PORT, &format, ABE_CBPR1_IDX,
					&dma_sink);
		} else {
			abe_read_port_address(VX_UL_PORT, &dma_params);
			dma_req = OMAP44XX_DMA_ABE_REQ_2;
			abe_connect_cbpr_dmareq_port(VX_UL_PORT, &format,  ABE_CBPR2_IDX,
					&dma_sink);
		}
        break;
	case ABE_FRONTEND_DAI_TONES:
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
			abe_read_port_address(TONES_DL_PORT, &dma_params);
			dma_req = OMAP44XX_DMA_ABE_REQ_5;
			abe_connect_cbpr_dmareq_port(TONES_DL_PORT, &format, ABE_CBPR5_IDX,
					&dma_sink);
		} else
			return -EINVAL;
        break;
	case ABE_FRONTEND_DAI_VIBRA:
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
			abe_read_port_address(VIB_DL_PORT, &dma_params);
			dma_req = OMAP44XX_DMA_ABE_REQ_6;
			abe_connect_cbpr_dmareq_port(VIB_DL_PORT, &format, ABE_CBPR6_IDX,
					&dma_sink);
		} else
			return -EINVAL;
        break;
	}

	/* configure frontend SDMA data */
	omap_abe_dai_dma_params[substream->stream].dma_req = dma_req;
	omap_abe_dai_dma_params[substream->stream].port_addr =
					(unsigned long)dma_params.data;
	omap_abe_dai_dma_params[substream->stream].packet_size = dma_params.iter;
	snd_soc_dai_set_dma_data(dai, substream,
		&omap_abe_dai_dma_params[substream->stream]);

	return 0;
}

static void abe_fe_shutdown(struct snd_pcm_substream *substream,
				struct snd_soc_dai *dai)
{
	//TODO: Do we need to do reset this stuff :-
	//abe_connect_cbpr_dmareq_port(VIB_DL_PORT, &format, ABE_CBPR6_IDX,
	//				&dma_sink);
}


/* Frontend --> Backend ALSA PCM OPS */

static int omap_abe_dai_startup(struct snd_pcm_substream *substream,
				  struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct abe_frontend_dai *fe = &abe_data.frontend[dai->id][substream->stream];
	int ret = 0, i;

	printk("%s: frontend %s %d\n", __func__, rtd->dai_link->name, dai->id);

	spin_lock(&fe->lock);

	/* only startup backends that are either sinks or sources to this frontend DAI */
	for (i = 0; i < rtd->num_be; i++) {
		struct snd_pcm_substream *be_substream =
				rtd->be_rtd[i]->pcm->streams[substream->stream].substream;

		if (be_substream == NULL)
			goto unwind;

		ret = snd_soc_pcm_open(be_substream);
		if (ret < 0)
			goto unwind;

		abe_data.be_active[rtd->be_rtd[i]->dai_link->be_id][substream->stream]++;
	}

	/* start the ABE frontend */
	ret = abe_fe_startup(substream, dai);
	if (ret < 0) {
		printk("%s: failed to start FE %d\n", __func__, ret);
		goto unwind;
	}

	spin_unlock(&fe->lock);
	printk("%s: frontend finished %s %d\n", __func__, rtd->dai_link->name, dai->id);
	return 0;

unwind:
	/* disable any enabled and non active backends */
	for (--i; i >= 0; i--) {
			snd_soc_pcm_close(rtd->be_rtd[i]->pcm->streams[substream->stream].substream);
			abe_data.be_active[rtd->be_rtd[i]->dai_link->be_id][substream->stream]--;
	}
	spin_unlock(&fe->lock);
	return ret;
}

static void omap_abe_dai_shutdown(struct snd_pcm_substream *substream,
				    struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct abe_frontend_dai *fe = &abe_data.frontend[dai->id][substream->stream];
	int i;

	printk("%s: frontend %s \n", __func__, rtd->dai_link->name);
	/* only shutdown backends that are either sinks or sources to this frontend DAI */

	spin_lock(&fe->lock);
	for (i = 0; i < rtd->num_be; i++) {
		/* close backend stream if inactive */
		snd_soc_pcm_close(rtd->be_rtd[i]->pcm->streams[substream->stream].substream);
		abe_data.be_active[rtd->be_rtd[i]->dai_link->be_id][substream->stream]--;
	}
	spin_unlock(&fe->lock);

	/* now shutdown the frontend */
	abe_fe_shutdown(substream, dai);
}

static int omap_abe_dai_hw_params(struct snd_pcm_substream *substream,
				    struct snd_pcm_hw_params *params,
				    struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	int i, ret;

	printk("%s: frontend %s \n", __func__, rtd->dai_link->name);

	// TODO: generate our own hw params for backend DAI
	/* only hw_params backends that are either sinks or
	 * sources to this frontend DAI */
	for (i = 0; i < rtd->num_be; i++) {

		ret = snd_soc_pcm_hw_params(rtd->be_rtd[i]->pcm->streams[substream->stream].substream,
			params);
		if (ret < 0) {
			printk(KERN_ERR "%s: be hw_params failed %d\n", __func__, ret);
			return ret;
		}
	}

	/* call hw_params on the frontend */
	ret = abe_fe_hw_params(substream, params, dai);
	if (ret < 0)
		printk(KERN_ERR "%s: frontend hw_params failed\n", __func__);

	return ret;
}

static int omap_abe_dai_trigger(struct snd_pcm_substream *substream,
				  int cmd, struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct abe_frontend_dai *fe = &abe_data.frontend[dai->id][substream->stream];
	int i, ret = 0;

	printk("%s: frontend %s \n", __func__, rtd->dai_link->name);

	/* only trigger backends that are either sinks or sources to
	 * this frontend DAI */
	for (i = 0; i < rtd->num_be; i++) {

		spin_lock(&fe->lock);
		fe->cmd = cmd;
		fe->substream = substream;
		spin_unlock(&fe->lock);

		/* perform the backend trigger work */
		schedule_work(&fe->work);
	}
	return ret;
}

static int omap_abe_dai_prepare(struct snd_pcm_substream *substream,
				  struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	int ret = 0, i;

	printk("%s: frontend %s \n", __func__, rtd->dai_link->name);
	/* only prepare backends that are either sinks or sources to
	 * this frontend DAI */

	for (i = 0; i < rtd->num_be; i++) {
		/* close backend stream if inactive */
		snd_soc_pcm_prepare(rtd->be_rtd[i]->pcm->streams[substream->stream].substream);
	}

	return ret;
}

static int omap_abe_dai_hw_free(struct snd_pcm_substream *substream,
				  struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	int ret = 0, i;

	printk("%s: frontend %s \n", __func__, rtd->dai_link->name);
	/* only hw_params backends that are either sinks or sources
	 * to this frontend DAI */
	for (i = 0; i < rtd->num_be; i++) {
		snd_soc_pcm_hw_free(rtd->be_rtd[i]->pcm->streams[substream->stream].substream);
	}

	return ret;
}

/* ABE Frontend PCM OPS */
static struct snd_soc_dai_ops omap_abe_dai_ops = {
	.startup	= omap_abe_dai_startup,
	.shutdown	= omap_abe_dai_shutdown,
	.hw_params	= omap_abe_dai_hw_params,
	.prepare	= omap_abe_dai_prepare,
	.hw_free	= omap_abe_dai_hw_free,
	.trigger	= omap_abe_dai_trigger,
};

// TODO: finish this
static void mute_be_capture(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_soc_pcm_runtime *be_rtd;
	int i;

	for (i = 0; i < rtd->num_be; i++) {
		be_rtd = rtd->be_rtd[i];

		dev_dbg(&rtd->dev, "%s: be ID=%d\n", __func__, be_rtd->dai_link->be_id);

		switch (be_rtd->dai_link->be_id) {
		case OMAP_ABE_DAI_PDM_UL:
//			abe_write_mixer(MIXAUDUL, MUTE_GAIN, RAMP_0MS,
//				MIX_AUDUL_INPUT_UPLINK);
			break;
		case OMAP_ABE_DAI_BT_VX:
		case OMAP_ABE_DAI_MM_FM:
		case OMAP_ABE_DAI_MODEM:
		case OMAP_ABE_DAI_DMIC0:
		case OMAP_ABE_DAI_DMIC1:
		case OMAP_ABE_DAI_DMIC2:
			break;
		}
	}
}

// TODO: finish this and restore correct volume levels
static void unmute_be_capture(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_soc_pcm_runtime *be_rtd;
	int i;

	for (i = 0; i < rtd->num_be; i++) {
		be_rtd = rtd->be_rtd[i];

		dev_dbg(&rtd->dev, "%s: be ID=%d\n", __func__, be_rtd->dai_link->be_id);

		switch (be_rtd->dai_link->be_id) {
		case OMAP_ABE_DAI_PDM_UL:
//			abe_write_mixer(MIXAUDUL, GAIN_M6dB, RAMP_0MS,
//				MIX_AUDUL_INPUT_UPLINK);
			break;
		case OMAP_ABE_DAI_BT_VX:
		case OMAP_ABE_DAI_MM_FM:
		case OMAP_ABE_DAI_MODEM:
		case OMAP_ABE_DAI_DMIC0:
		case OMAP_ABE_DAI_DMIC1:
		case OMAP_ABE_DAI_DMIC2:
			break;
		}
	}
}

// TODO: finish this - restore correct values
static void mute_be_playback(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_soc_pcm_runtime *be_rtd;
	int i;

	for (i = 0; i < rtd->num_be; i++) {
		be_rtd = rtd->be_rtd[i];

		dev_dbg(&rtd->dev, "%s: be ID=%d\n", __func__, be_rtd->dai_link->be_id);

		switch (be_rtd->dai_link->be_id) {
		case OMAP_ABE_DAI_PDM_DL1:
			//abe_write_mixer(MIXDL2, MUTE_GAIN, RAMP_0MS, MIX_DL2_INPUT_MM_DL);
			break;
		case OMAP_ABE_DAI_PDM_DL2:
			//abe_write_mixer(MIXDL2, MUTE_GAIN, RAMP_0MS, MIX_DL2_INPUT_MM_DL);
			break;
		case OMAP_ABE_DAI_PDM_VIB:
		case OMAP_ABE_DAI_BT_VX:
		case OMAP_ABE_DAI_MM_FM:
		case OMAP_ABE_DAI_MODEM:
			break;
		}
	}
}

// TODO: finish this and restore correct volume levels
static void unmute_be_playback(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_soc_pcm_runtime *be_rtd;
	int i;

	for (i = 0; i < rtd->num_be; i++) {
		be_rtd = rtd->be_rtd[i];

		dev_dbg(&rtd->dev, "%s: be ID=%d\n", __func__, be_rtd->dai_link->be_id);

		switch (be_rtd->dai_link->be_id) {
		case OMAP_ABE_DAI_PDM_DL1:
		case OMAP_ABE_DAI_PDM_DL2:
		case OMAP_ABE_DAI_PDM_VIB:
		case OMAP_ABE_DAI_BT_VX:
		case OMAP_ABE_DAI_MM_FM:
		case OMAP_ABE_DAI_MODEM:
			break;
		}
	}
}

static void enable_be_ports(struct snd_soc_pcm_runtime *rtd,  int stream)
{
	struct snd_soc_pcm_runtime *be_rtd;
	int i;

	for (i = 0; i < rtd->num_be; i++) {
		be_rtd = rtd->be_rtd[i];

		dev_dbg(&rtd->dev, "%s: be ID=%d stream %d active %d\n",
				__func__, be_rtd->dai_link->be_id, stream,
				abe_data.be_active[be_rtd->dai_link->be_id][stream]);

		switch (be_rtd->dai_link->be_id) {
		case OMAP_ABE_DAI_PDM_DL1:
			if (!atomic_cmpxchg(&abe_data.mcpdm_dl_cnt, 0, 1))
				abe_enable_data_transfer(PDM_DL_PORT);
			else
				atomic_inc(&abe_data.mcpdm_dl_cnt);
			break;
		case OMAP_ABE_DAI_PDM_DL2:
			if (!atomic_cmpxchg(&abe_data.mcpdm_dl_cnt, 0, 1))
				abe_enable_data_transfer(PDM_DL2_PORT);
			else
				atomic_inc(&abe_data.mcpdm_dl_cnt);
			break;
		case OMAP_ABE_DAI_PDM_VIB:
			if (!atomic_cmpxchg(&abe_data.mcpdm_dl_cnt, 0, 1))
				abe_enable_data_transfer(PDM_VIB_PORT);
			else
				atomic_inc(&abe_data.mcpdm_dl_cnt);
			break;
		case OMAP_ABE_DAI_PDM_UL:
			if (!atomic_cmpxchg(&abe_data.mcpdm_ul_cnt, 0, 1))
				abe_enable_data_transfer(PDM_UL_PORT);
			else
				atomic_inc(&abe_data.mcpdm_ul_cnt);
			break;
		case OMAP_ABE_DAI_BT_VX:
			if (stream == SNDRV_PCM_STREAM_PLAYBACK)
				abe_enable_data_transfer(BT_VX_DL_PORT);
			else
				abe_enable_data_transfer(BT_VX_UL_PORT);
			break;
		case OMAP_ABE_DAI_MM_FM:
		case OMAP_ABE_DAI_MODEM:
			if (stream == SNDRV_PCM_STREAM_PLAYBACK)
				abe_enable_data_transfer(MM_EXT_OUT_PORT);
			else
				abe_enable_data_transfer(MM_EXT_IN_PORT);
			break;
		case OMAP_ABE_DAI_DMIC0:
			if (stream == SNDRV_PCM_STREAM_PLAYBACK)
				abe_enable_data_transfer(DMIC_PORT);
			break;
		case OMAP_ABE_DAI_DMIC1:
			if (stream == SNDRV_PCM_STREAM_PLAYBACK)
				abe_enable_data_transfer(DMIC_PORT1);
			break;
		case OMAP_ABE_DAI_DMIC2:
			if (stream == SNDRV_PCM_STREAM_PLAYBACK)
				abe_enable_data_transfer(DMIC_PORT2);
			break;
		}
	}
}

static void enable_fe_ports(struct snd_soc_pcm_runtime *rtd, int stream)
{
	dev_dbg(&rtd->dev, "%s: fe ID=%d stream %d\n",
			__func__, rtd->cpu_dai->id, stream);

	switch(rtd->cpu_dai->id) {
	case ABE_FRONTEND_DAI_MEDIA:
		if (stream == SNDRV_PCM_STREAM_PLAYBACK)
			abe_enable_data_transfer(MM_DL_PORT);
		else
			abe_enable_data_transfer(MM_UL_PORT);
		break;
	case ABE_FRONTEND_DAI_MEDIA_CAPTURE:
		if (stream == SNDRV_PCM_STREAM_CAPTURE)
			abe_enable_data_transfer(MM_UL2_PORT);
		break;
	case ABE_FRONTEND_DAI_VOICE:
		if (stream == SNDRV_PCM_STREAM_PLAYBACK)
			abe_enable_data_transfer(VX_DL_PORT);
		else
			abe_enable_data_transfer(VX_UL_PORT);
		break;
	case ABE_FRONTEND_DAI_TONES:
		if (stream == SNDRV_PCM_STREAM_PLAYBACK)
			abe_enable_data_transfer(TONES_DL_PORT);
		break;
	case ABE_FRONTEND_DAI_VIBRA:
		if (stream == SNDRV_PCM_STREAM_PLAYBACK)
			abe_enable_data_transfer(VIB_DL_PORT);
		break;
	}
}

static void disable_be_ports(struct snd_soc_pcm_runtime *rtd, int stream)
{
	struct snd_soc_pcm_runtime *be_rtd;
	int i;

	for (i = 0; i < rtd->num_be; i++) {
		be_rtd = rtd->be_rtd[i];

		dev_dbg(&rtd->dev, "%s: be ID=%d stream %d active %d\n",
				__func__, be_rtd->dai_link->be_id, stream,
				abe_data.be_active[be_rtd->dai_link->be_id][stream]);

		switch (be_rtd->dai_link->be_id) {
		case OMAP_ABE_DAI_PDM_DL1:
			if (atomic_dec_and_test(&abe_data.mcpdm_dl_cnt))
				abe_disable_data_transfer(PDM_DL_PORT);
			break;
		case OMAP_ABE_DAI_PDM_DL2:
			if (atomic_dec_and_test(&abe_data.mcpdm_dl_cnt))
				abe_disable_data_transfer(PDM_DL2_PORT);
			break;
		case OMAP_ABE_DAI_PDM_VIB:
			if (atomic_dec_and_test(&abe_data.mcpdm_dl_cnt))
				abe_disable_data_transfer(PDM_VIB_PORT);
			break;
		case OMAP_ABE_DAI_PDM_UL:
			if (atomic_dec_and_test(&abe_data.mcpdm_ul_cnt))
				abe_disable_data_transfer(PDM_UL_PORT);
			break;
		case OMAP_ABE_DAI_BT_VX:
			if (stream == SNDRV_PCM_STREAM_PLAYBACK)
				abe_disable_data_transfer(BT_VX_DL_PORT);
			else
				abe_disable_data_transfer(BT_VX_UL_PORT);
			break;
		case OMAP_ABE_DAI_MM_FM:
		case OMAP_ABE_DAI_MODEM:
			if (stream == SNDRV_PCM_STREAM_PLAYBACK)
				abe_disable_data_transfer(MM_EXT_OUT_PORT);
			else
				abe_disable_data_transfer(MM_EXT_IN_PORT);
		case OMAP_ABE_DAI_DMIC0:
			if (stream == SNDRV_PCM_STREAM_PLAYBACK)
				abe_disable_data_transfer(DMIC_PORT);
			break;
		case OMAP_ABE_DAI_DMIC1:
			if (stream == SNDRV_PCM_STREAM_PLAYBACK)
				abe_disable_data_transfer(DMIC_PORT1);
			break;
		case OMAP_ABE_DAI_DMIC2:
			if (stream == SNDRV_PCM_STREAM_PLAYBACK)
				abe_disable_data_transfer(DMIC_PORT2);
			break;
		}
	}
}

static void disable_fe_ports(struct snd_soc_pcm_runtime *rtd, int stream)
{
	dev_dbg(&rtd->dev, "%s: fe ID=%d stream %d\n",
			__func__, rtd->cpu_dai->id, stream);

	switch(rtd->cpu_dai->id) {
	case ABE_FRONTEND_DAI_MEDIA:
		if (stream == SNDRV_PCM_STREAM_PLAYBACK)
			abe_disable_data_transfer(MM_DL_PORT);
		else
			abe_disable_data_transfer(MM_UL_PORT);
		break;
	case ABE_FRONTEND_DAI_MEDIA_CAPTURE:
		if (stream == SNDRV_PCM_STREAM_CAPTURE)
			abe_disable_data_transfer(MM_UL2_PORT);
		break;
	case ABE_FRONTEND_DAI_VOICE:
		if (stream == SNDRV_PCM_STREAM_PLAYBACK)
			abe_disable_data_transfer(VX_DL_PORT);
		else
			abe_disable_data_transfer(VX_UL_PORT);
		break;
	case ABE_FRONTEND_DAI_TONES:
		if (stream == SNDRV_PCM_STREAM_PLAYBACK)
			abe_disable_data_transfer(TONES_DL_PORT);
		break;
	case ABE_FRONTEND_DAI_VIBRA:
		if (stream == SNDRV_PCM_STREAM_PLAYBACK)
			abe_disable_data_transfer(VIB_DL_PORT);
		break;
	}
}

// TODO: finish this
static void mute_fe_port(struct snd_soc_pcm_runtime *rtd, int stream)
{
	dev_dbg(&rtd->dev, "%s: fe ID=%d stream %d\n",
			__func__, rtd->cpu_dai->id, stream);

	switch(rtd->cpu_dai->id) {
	case ABE_FRONTEND_DAI_MEDIA:
	case ABE_FRONTEND_DAI_MEDIA_CAPTURE:
	//	if (stream == SNDRV_PCM_STREAM_CAPTURE)
		break;
	case ABE_FRONTEND_DAI_VOICE:
	case ABE_FRONTEND_DAI_TONES:
	case ABE_FRONTEND_DAI_VIBRA:

		break;
	}
}

// TODO: finish this
static void unmute_fe_port(struct snd_soc_pcm_runtime *rtd, int stream)
{
	dev_dbg(&rtd->dev, "%s: fe ID=%d stream %d\n",
			__func__, rtd->cpu_dai->id, stream);

	switch(rtd->cpu_dai->id) {
	case ABE_FRONTEND_DAI_MEDIA:
	case ABE_FRONTEND_DAI_MEDIA_CAPTURE:
	//	if (stream == SNDRV_PCM_STREAM_CAPTURE)
		break;
	case ABE_FRONTEND_DAI_VOICE:
	case ABE_FRONTEND_DAI_TONES:
	case ABE_FRONTEND_DAI_VIBRA:

		break;
	}
}

static void trigger_be_ports(struct abe_frontend_dai *fe,
		struct snd_soc_pcm_runtime *rtd)
{
	struct snd_soc_pcm_runtime *be_rtd;
	int i;

	for (i = 0; i < rtd->num_be; i++) {
		be_rtd = rtd->be_rtd[i];
		dev_dbg(&rtd->dev, "%s: be ID=%d cmd %d\n",
				__func__, be_rtd->dai_link->be_id, fe->cmd);
		snd_soc_dai_trigger(fe->substream, fe->cmd, be_rtd->cpu_dai);
	}
}

/* work for ABE capture frontend and backend ABE synchronisation */
static void capture_work(struct work_struct *work)
{
	struct abe_frontend_dai *fe =
			container_of(work, struct abe_frontend_dai, work);
	struct snd_soc_pcm_runtime *rtd = fe->substream->private_data;

	dev_dbg(&rtd->dev, "%s-start: fe ID=%d stream %d\n",
			__func__, rtd->cpu_dai->id,fe->substream->stream);

	spin_lock(&fe->lock);

	switch (fe->cmd) {
	case SNDRV_PCM_TRIGGER_START:
		/*   ALL BE GAINS set to mute */
		mute_be_capture(rtd);

		/* Enable ALL ABE BE ports */
		enable_be_ports(rtd, SNDRV_PCM_STREAM_CAPTURE);

		/* DAI work must be started/stopped at least 250us after ABE */
		udelay(250);

		/*  trigger ALL BE ports */
		trigger_be_ports(fe, rtd);

		/* Enable Frontend sDMA  */
		enable_fe_ports(rtd, SNDRV_PCM_STREAM_CAPTURE);

		/* Restore ABE GAINS AMIC */
		unmute_be_capture(rtd);
		break;
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		/* Enable sDMA / Enable ABE MM_UL2 */
		enable_fe_ports(rtd, SNDRV_PCM_STREAM_CAPTURE);
		break;
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		/* Disable sDMA / Enable ABE MM_UL2 */
		disable_fe_ports(rtd, SNDRV_PCM_STREAM_CAPTURE);
		break;
	case SNDRV_PCM_TRIGGER_STOP:
		/* Disable sDMA */
		disable_fe_ports(rtd, SNDRV_PCM_STREAM_CAPTURE);

		disable_be_ports(rtd, SNDRV_PCM_STREAM_CAPTURE);

		/* DAI work must be started/stopped at least 250us after ABE */
		udelay(250);

		/*  trigger ALL BE ports */
		trigger_be_ports(fe, rtd);

		/* TODO: shutdown PDM clock */
		break;
	default:
		break;
	}

	spin_unlock(&fe->lock);

	dev_dbg(&rtd->dev, "%s - finish: fe ID=%d stream %d\n",
			__func__, rtd->cpu_dai->id,fe->substream->stream);
}


/* work for ABE playback frontend and backend ABE synchronisation */
static void playback_work(struct work_struct *work)
{
	struct abe_frontend_dai *fe =
			container_of(work, struct abe_frontend_dai, work);
	struct snd_soc_pcm_runtime *rtd = fe->substream->private_data;

	dev_dbg(&rtd->dev, "%s-start: fe ID=%d stream %d\n",
			__func__, rtd->cpu_dai->id,fe->substream->stream);

	spin_lock(&fe->lock);

	switch (fe->cmd) {
	case SNDRV_PCM_TRIGGER_START:
		/* mute splayback */
		mute_be_playback(rtd);

		/* enabled BE ports */
		enable_be_ports(rtd, SNDRV_PCM_STREAM_PLAYBACK);

		/* DAI work must be started/stopped at least 250us after ABE */
		udelay(250);

		/*  trigger ALL BE ports */
		trigger_be_ports(fe, rtd);

		/* TODO: unmute DL1 */
		unmute_be_playback(rtd);

		/* Enable Frontend sDMA  */
		enable_fe_ports(rtd, SNDRV_PCM_STREAM_PLAYBACK);

		/* unmute ABE_MM_DL */
		unmute_fe_port(rtd, SNDRV_PCM_STREAM_PLAYBACK);
		break;
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		/* Enable Frontend sDMA  */
		enable_fe_ports(rtd, SNDRV_PCM_STREAM_PLAYBACK);

		/* unmute ABE_MM_DL */
		unmute_fe_port(rtd, SNDRV_PCM_STREAM_PLAYBACK);
		break;
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		/* Enable Frontend sDMA  */
		disable_fe_ports(rtd, SNDRV_PCM_STREAM_PLAYBACK);

		/* mute ABE_MM_DL */
		mute_fe_port(rtd, SNDRV_PCM_STREAM_PLAYBACK);
		break;
	case SNDRV_PCM_TRIGGER_STOP:
		/* disable the transfer */
		disable_fe_ports(rtd, SNDRV_PCM_STREAM_PLAYBACK);

		/* mute ABE_MM_DL */
		mute_fe_port(rtd, SNDRV_PCM_STREAM_PLAYBACK);

		/* TODO :mute Phoenix */

		/* disable our backends */
		disable_be_ports(rtd, SNDRV_PCM_STREAM_PLAYBACK);

		/* DAI work must be started/stopped at least 250us after ABE */
		udelay(250);

		/*  trigger ALL BE ports */
		trigger_be_ports(fe, rtd);
		break;
	default:
		break;
	}

	spin_unlock(&fe->lock);

	dev_dbg(&rtd->dev, "%s - finish: fe ID=%d stream %d\n",
			__func__, rtd->cpu_dai->id,fe->substream->stream);
}

static int omap_abe_dai_probe(struct snd_soc_dai *dai)
{
	snd_soc_dai_set_drvdata(dai, &abe_data);

	/* MM */
	INIT_WORK(&abe_data.frontend[ABE_FRONTEND_DAI_MEDIA][SNDRV_PCM_STREAM_CAPTURE].work,
		capture_work);
	INIT_WORK(&abe_data.frontend[ABE_FRONTEND_DAI_MEDIA][SNDRV_PCM_STREAM_PLAYBACK].work,
		playback_work);

	/* MM UL */
	INIT_WORK(&abe_data.frontend[ABE_FRONTEND_DAI_MEDIA_CAPTURE][SNDRV_PCM_STREAM_CAPTURE].work,
		capture_work);

	/* VX */
	INIT_WORK(&abe_data.frontend[ABE_FRONTEND_DAI_VOICE][SNDRV_PCM_STREAM_CAPTURE].work,
		capture_work);
	INIT_WORK(&abe_data.frontend[ABE_FRONTEND_DAI_VOICE][SNDRV_PCM_STREAM_PLAYBACK].work,
		playback_work);

	/* TONES */
	INIT_WORK(&abe_data.frontend[ABE_FRONTEND_DAI_TONES][SNDRV_PCM_STREAM_PLAYBACK].work,
		playback_work);

	/* VIB */
	INIT_WORK(&abe_data.frontend[ABE_FRONTEND_DAI_VIBRA][SNDRV_PCM_STREAM_PLAYBACK].work,
		playback_work);

	return 0;
}

static struct snd_soc_dai_driver omap_abe_dai[] = {
	{	/* Multimedia Playback and Capture */
		.name = "MultiMedia1",
		.probe = omap_abe_dai_probe,
		.playback = {
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_44100 | SNDRV_PCM_RATE_48000,
			.formats = OMAP_ABE_FORMATS,
		},
		.capture = {
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_48000,
			.formats = OMAP_ABE_FORMATS,
		},
		.ops = &omap_abe_dai_ops,
	},
	{	/* Multimedia Capture */
		.name = "MultiMedia2",
		.capture = {
			.channels_min = 2,
			.channels_max = 8,
			.rates = SNDRV_PCM_RATE_48000,
			.formats = OMAP_ABE_FORMATS,
		},
		.ops = &omap_abe_dai_ops,
	},
	{	/* Voice Playback and Capture */
		.name = "Voice",
		.playback = {
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000,
			.formats = OMAP_ABE_FORMATS,
		},
		.capture = {
			.channels_min = 2,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000,
			.formats = OMAP_ABE_FORMATS,
		},
		.ops = &omap_abe_dai_ops,
	},
	{	/* Tones Playback */
		.name = "Tones",
		.playback = {
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_44100 | SNDRV_PCM_RATE_48000,
			.formats = OMAP_ABE_FORMATS,
		},
		.ops = &omap_abe_dai_ops,
	},
	{	/* Vibra */
		.name = "Vibra",
		.playback = {
			.channels_min = 2,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_CONTINUOUS,
			.formats = OMAP_ABE_FORMATS,
		},
		.ops = &omap_abe_dai_ops,
	},
};

static int __devinit omap_abe_probe(struct platform_device *pdev)
{
	return snd_soc_register_dais(&pdev->dev, omap_abe_dai,
			ARRAY_SIZE(omap_abe_dai));
}

static int __devexit omap_abe_remove(struct platform_device *pdev)
{
	snd_soc_unregister_dais(&pdev->dev, ARRAY_SIZE(omap_abe_dai));
	return 0;
}

static struct platform_driver omap_abe_driver = {
	.driver = {
		.name = "omap-abe-dai",
		.owner = THIS_MODULE,
	},
	.probe = omap_abe_probe,
	.remove = __devexit_p(omap_abe_remove),
};

static int __init omap_abe_init(void)
{
	return platform_driver_register(&omap_abe_driver);
}
module_init(omap_abe_init);

static void __exit omap_abe_exit(void)
{
	platform_driver_unregister(&omap_abe_driver);
}
module_exit(omap_abe_exit);

MODULE_AUTHOR("Liam Girdwood <lrg@slimlogic.co.uk>");
MODULE_DESCRIPTION("OMAP ABE SoC Interface");
MODULE_LICENSE("GPL");
