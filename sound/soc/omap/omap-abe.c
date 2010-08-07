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

#define NUM_ABE_BACKENDS		11

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

struct abe_backend_dai {
	atomic_t active;	/* active stream direction */
	struct snd_pcm_substream *substream; /* substreams per direction */
	spinlock_t lock;

	/* trigger work */
	struct work_struct work;
	int cmd;
};

struct abe_frontend_dai {
	/* each frontend ALSA PCM can access several backend DAIs */
	int be_enabled[NUM_ABE_BACKENDS];
	struct omap_aess_fe *fe;
};

struct omap_abe_data {
	struct abe_frontend_dai frontend[NUM_ABE_FRONTENDS];
	struct abe_backend_dai be[NUM_ABE_BACKENDS][2];
	int configure;
	atomic_t mcpdm_dl_cnt;  /* McPDM DL port activation counter */
	atomic_t mcpdm_ul_cnt;  /* McPDM UL port activation counter */
	atomic_t mm_dl_cnt;     /* MM DL port activation counter */

	struct clk *clk;

	/* hwmod platform device */
	struct platform_device *pdev;
};

static struct omap_abe_data abe_data = {
	/* PDM UL1 */
	.be[ABE_DAI_PDM_UL][SNDRV_PCM_STREAM_CAPTURE] = {
			.active = ATOMIC_INIT(0),
			.lock = SPIN_LOCK_UNLOCKED,
	},
	/* PDM DL1 */
	.be[ABE_DAI_PDM_DL1][SNDRV_PCM_STREAM_PLAYBACK] = {
			.active = ATOMIC_INIT(0),
			.lock = SPIN_LOCK_UNLOCKED,
	},
	/* PDM DL2 */
	.be[ABE_DAI_PDM_DL2][SNDRV_PCM_STREAM_PLAYBACK] = {
			.active = ATOMIC_INIT(0),
			.lock = SPIN_LOCK_UNLOCKED,
	},
	/* PDM VIB */
	.be[ABE_DAI_PDM_VIB][SNDRV_PCM_STREAM_PLAYBACK] = {
			.active = ATOMIC_INIT(0),
			.lock = SPIN_LOCK_UNLOCKED,
	},
	/* BT VX DL */
	.be[ABE_DAI_BT_VX_DL][SNDRV_PCM_STREAM_PLAYBACK] = {
			.active = ATOMIC_INIT(0),
			.lock = SPIN_LOCK_UNLOCKED,
	},
	/* BT_VX_UL */
	.be[ABE_DAI_BT_VX_UL][SNDRV_PCM_STREAM_CAPTURE] = {
			.active = ATOMIC_INIT(0),
			.lock = SPIN_LOCK_UNLOCKED,
	},
	/* MODEM */
	.be[ABE_DAI_MM_DL][SNDRV_PCM_STREAM_PLAYBACK] = {
			.active = ATOMIC_INIT(0),
			.lock = SPIN_LOCK_UNLOCKED,
	},
	.be[ABE_DAI_MM_UL][SNDRV_PCM_STREAM_CAPTURE] = {
			.active = ATOMIC_INIT(0),
			.lock = SPIN_LOCK_UNLOCKED,
	},
	/* DMIC1 */
	.be[ABE_DAI_PDM_UL][SNDRV_PCM_STREAM_CAPTURE] = {
			.active = ATOMIC_INIT(0),
			.lock = SPIN_LOCK_UNLOCKED,
	},
	/* DMIC2 */
	.be[ABE_DAI_PDM_UL][SNDRV_PCM_STREAM_CAPTURE] = {
			.active = ATOMIC_INIT(0),
			.lock = SPIN_LOCK_UNLOCKED,
	},
	/* DMIC3 */
	.be[ABE_DAI_PDM_UL][SNDRV_PCM_STREAM_CAPTURE] = {
			.active = ATOMIC_INIT(0),
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
//	struct snd_soc_pcm_runtime *rtd = substream->private_data;
//	struct omap_abe_data *priv = snd_soc_dai_get_drvdata(dai);
	abe_data_format_t format;
	abe_dma_t dma_sink;

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
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
			abe_connect_cbpr_dmareq_port(MM_DL_PORT, &format, ABE_CBPR0_IDX,
					&dma_sink);
		else
			abe_connect_cbpr_dmareq_port(MM_UL2_PORT, &format,  ABE_CBPR4_IDX,
					&dma_sink);
        break;
	case ABE_FRONTEND_DAI_MEDIA_CAPTURE:
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
			return -EINVAL;
		else
			abe_connect_cbpr_dmareq_port(MM_UL_PORT, &format,  ABE_CBPR3_IDX,
					&dma_sink);
        break;
	case ABE_FRONTEND_DAI_VOICE:
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
			abe_connect_cbpr_dmareq_port(VX_DL_PORT, &format, ABE_CBPR1_IDX,
					&dma_sink);
		else
			abe_connect_cbpr_dmareq_port(VX_UL_PORT, &format,  ABE_CBPR2_IDX,
					&dma_sink);
        break;
	case ABE_FRONTEND_DAI_TONES:
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
			abe_connect_cbpr_dmareq_port(TONES_DL_PORT, &format, ABE_CBPR5_IDX,
					&dma_sink);
		else
			return -EINVAL;
        break;
	case ABE_FRONTEND_DAI_VIBRA:
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
			abe_connect_cbpr_dmareq_port(VIB_DL_PORT, &format, ABE_CBPR6_IDX,
					&dma_sink);
		else
			return -EINVAL;
        break;
	}

	return 0;
}

static void abe_fe_shutdown(struct snd_pcm_substream *substream,
				struct snd_soc_dai *dai)
{

}

static int omap_abe_get_be(struct abe_frontend_dai *fe, int fe_id)
{
	return 0;
}

/* Frontend --> Backend ALSA PCM OPS */

static int omap_abe_dai_startup(struct snd_pcm_substream *substream,
				  struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_card *card = rtd->card;
	struct abe_frontend_dai *fe = &abe_data.frontend[dai->id];
	struct omap_aess_fe *aess_fe;
	int ret = 0, i, started = 0;

	printk("%s: frontend %s %d\n", __func__, rtd->dai_link->name, dai->id);



	aess_fe = omap_aess_get_be_status(dai->id);
	if (aess_fe == NULL) {
		dev_err(rtd->cpu_dai->dev, "could not get valid frontend\n");
		return -EINVAL;
	}

	/* only startup backends that are either sinks or sources to this frontend DAI */
	for (i = 0; i < NUM_ABE_BACKENDS; i++) {
		struct abe_backend_dai *be =
			&abe_data.be[i][substream->stream];

		/* make sure we only open backend DAIs that support
		 * the same stream direction */
		if (!fe_stream[dai->id].dir[substream->stream] ||
				!be_stream[i].dir[substream->stream])
			continue;

		/* we dont need to open backend DAI if it's not enabled */
		if (!fe->be_enabled[i])
			continue;

		/* we dont need to open backend DAI if it's already active */
		if (atomic_read(&be->active))
			continue;

		spin_lock(&be->lock);

		/* enable backend DAI for this stream direction */
		be->substream = snd_soc_get_dai_substream(card, be_stream[i].id,
			substream->stream);
		if (be->substream == NULL) {
			spin_unlock(&be->lock);
			printk(KERN_ERR "%s: could not get backend"
				" substream %s\n", __func__, be_stream[i].id);
			ret = -ENODEV;
			goto unwind;
		}
		spin_unlock(&be->lock);

		ret = snd_soc_pcm_open(be->substream);
		if (ret < 0)
			goto unwind;

		atomic_inc(&be->active);
		started++;
	}

	/* did we start any valid DAIs */
	if (!started) {
		printk("%s: no backends started\n", __func__);
	//	return -EINVAL;
	}

	/* start the frontend */
	ret = abe_fe_startup(substream, dai);
	if (ret < 0) {
		printk("%s: failed to start FE %d\n", __func__, ret);
		goto unwind;
	}

	printk("%s: frontend finished %s %d\n", __func__, rtd->dai_link->name, dai->id);
	return 0;

unwind:
	/* disable any enabled and non active backends */
	for (--i; i >= 0; i--) {
		struct abe_backend_dai *be =
			&abe_data.be[i][substream->stream];

		if (fe->be_enabled[i] && atomic_read(&be->active))
			snd_soc_pcm_close(be->substream);
	}

	return ret;
}

static void omap_abe_dai_shutdown(struct snd_pcm_substream *substream,
				    struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct abe_frontend_dai *fe = &abe_data.frontend[dai->id];
	int i;

	printk("%s: frontend %s \n", __func__, rtd->dai_link->name);
	/* only shutdown backends that are either sinks or sources to this frontend DAI */
	for (i = 0; i < NUM_ABE_BACKENDS; i++) {
		struct abe_backend_dai *be =
			&abe_data.be[i][substream->stream];

		/* make sure we only close backend DAIs that support
		 * the same stream direction */
		if (!fe_stream[dai->id].dir[substream->stream] ||
				!be_stream[i].dir[substream->stream])
			continue;

		/* we dont need to close backend DAI if it's not enabled */
		if (!fe->be_enabled[i])
			continue;

		/* we dont need to close backend DAI if it's already inactive */
		if (!atomic_read(&be->active)) {
			continue;
		}

		/* close backend stream if inactive */
		if (atomic_sub_return(1, &be->active) == 0) {
			snd_soc_pcm_close(be->substream);

			spin_lock(&be->lock);
			be->substream = NULL;
			spin_unlock(&be->lock);
		}

	}
	/* now shutdown the frontend */
	abe_fe_shutdown(substream, dai);
}

static int omap_abe_dai_hw_params(struct snd_pcm_substream *substream,
				    struct snd_pcm_hw_params *params,
				    struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct abe_frontend_dai *fe = &abe_data.frontend[dai->id];
	int i, stream = substream->stream, dma_req, ret;
	abe_data_format_t format;
	abe_dma_t dma_params;

	printk("%s: frontend %s \n", __func__, rtd->dai_link->name);
	/* get abe dma data */
	switch (dai->id) {
	case ABE_FRONTEND_DAI_MEDIA:
		if (stream == SNDRV_PCM_STREAM_CAPTURE) {
			abe_read_port_address(MM_UL2_PORT, &dma_params);
			dma_req = OMAP44XX_DMA_ABE_REQ_4;
		} else {
			abe_read_port_address(MM_DL_PORT, &dma_params);
			dma_req = OMAP44XX_DMA_ABE_REQ_0;
		}
		break;
	case ABE_FRONTEND_DAI_TONES:
		if (stream == SNDRV_PCM_STREAM_CAPTURE) {
			return -EINVAL;
		} else {
			abe_read_port_address(TONES_DL_PORT, &dma_params);
			dma_req = OMAP44XX_DMA_ABE_REQ_5;
		}
		break;
	case ABE_FRONTEND_DAI_VOICE:
		if (stream == SNDRV_PCM_STREAM_CAPTURE) {
			abe_read_port_address(VX_UL_PORT, &dma_params);
			dma_req = OMAP44XX_DMA_ABE_REQ_2;
		} else {
			abe_read_port_address(VX_DL_PORT, &dma_params);
			dma_req = OMAP44XX_DMA_ABE_REQ_1;
		}
		break;
	case ABE_FRONTEND_DAI_MEDIA_CAPTURE:
		if (stream == SNDRV_PCM_STREAM_CAPTURE) {
			abe_read_port_address(MM_UL_PORT, &dma_params);
			dma_req = OMAP44XX_DMA_ABE_REQ_3;
		} else
			return -EINVAL;
		break;
	case ABE_FRONTEND_DAI_VIBRA:
		if (stream == SNDRV_PCM_STREAM_CAPTURE) {
			return -EINVAL;
		} else {
			abe_read_port_address(VIB_DL_PORT, &dma_params);
                        dma_req = OMAP44XX_DMA_ABE_REQ_6;
		}
		break;
	default:
			printk(KERN_ERR "%s: invalid frontend %d\n",__func__, dai->id);
		return -EINVAL;
	}

	/* configure frontend SDMA data */
	omap_abe_dai_dma_params[stream].dma_req = dma_req;
	omap_abe_dai_dma_params[stream].port_addr =
					(unsigned long)dma_params.data;
	omap_abe_dai_dma_params[stream].packet_size = dma_params.iter;
	snd_soc_dai_set_dma_data(dai, substream,
		&omap_abe_dai_dma_params[stream]);

	// TODO: generate our own hw params for backend DAI
	/* only hw_params backends that are either sinks or
	 * sources to this frontend DAI */
	for (i = 0; i < NUM_ABE_BACKENDS; i++) {
		struct abe_backend_dai *be =
			&abe_data.be[i][substream->stream];

		/* make sure we only hw_params backend DAIs that support
		 * the same stream direction */
		if (!fe_stream[dai->id].dir[substream->stream] ||
				!be_stream[i].dir[substream->stream])

		/* we dont need to call backend DAI if it's not enabled */
		if (!fe->be_enabled[i])
			continue;

		/* we dont need to hw_param backend DAI if it's inactive */
		if (!atomic_read(&be->active))
			continue;

		/* perform any BE config */
		switch (i) {
		case ABE_DAI_PDM_UL:
		case ABE_DAI_PDM_DL1:
		case ABE_DAI_PDM_DL2:
		case ABE_DAI_PDM_VIB:
			break;
		case ABE_DAI_BT_VX_UL:
		case ABE_DAI_BT_VX_DL:
		// TODO create params for McBSP DAI -
		//ALSO what about other paths ?????? with different source and sinks
			/* 16bit, 8kHz, stereo for BT  */
			format.f = 8000;
			format.samp_format = MONO_MSB;
			if (stream == SNDRV_PCM_STREAM_CAPTURE)
				abe_connect_serial_port(VX_UL_PORT, &format, MCBSP1_RX);
			else
				abe_connect_serial_port(VX_DL_PORT, &format, MCBSP1_TX);
			break;
		case ABE_DAI_MM_UL:
		case ABE_DAI_MM_DL:
		// TODO create params for McBSP DAI
			/* 16bit, 48kHz, stereo for FM  */
			format.f = 48000;
			format.samp_format = STEREO_RSHIFTED_16;
			if (stream == SNDRV_PCM_STREAM_CAPTURE)
				abe_connect_serial_port(MM_UL_PORT, &format, MCBSP1_RX);
			else
				abe_connect_serial_port(MM_DL_PORT, &format, MCBSP1_TX);
			// TODO create params for McBSP DAI
			break;
#if 0 // TODO:
		case ABE_DAI_MM_EXT1:
		// TODO create params for McBSP DAI
			/* 16bit, 8kHz, stereo for MODEM  */
			format.f = 8000;
			format.samp_format = MONO_MSB;
			if (stream == SNDRV_PCM_STREAM_CAPTURE)
				abe_connect_serial_port(VX_UL_PORT, &format, MCBSP2_RX);
			else
				abe_connect_serial_port(VX_DL_PORT, &format, MCBSP2_TX);
			break;
#endif
		case ABE_DAI_DMIC0:
		case ABE_DAI_DMIC1:
		case ABE_DAI_DMIC2:
			break;
		};

		ret = snd_soc_pcm_hw_params(be->substream, params);
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
	struct abe_frontend_dai *fe = &abe_data.frontend[dai->id];
	int i, ret = 0;

	printk("%s: frontend %s \n", __func__, rtd->dai_link->name);

	/* only trigger backends that are either sinks or sources to
	 * this frontend DAI */
	for (i = 0; i < NUM_ABE_BACKENDS; i++) {
		struct abe_backend_dai *be =
			&abe_data.be[i][substream->stream];

		/* make sure we only hw_params backend DAIs that support
		 * the same stream direction */
		if (!fe_stream[dai->id].dir[substream->stream] ||
				!be_stream[i].dir[substream->stream])
			continue;

		/* we dont need to call backend DAI if it's not enabled */
		if (!fe->be_enabled[i])
			continue;

		/* we dont need to hw_param backend DAI if it's inactive */
		if (!atomic_read(&be->active))
			continue;

		spin_lock(&be->lock);
		be->cmd = cmd;
		spin_unlock(&be->lock);

		/* perform the backend trigger work */
		schedule_work(&be->work);
	}
	return ret;
}

static int omap_abe_dai_prepare(struct snd_pcm_substream *substream,
				  struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct abe_frontend_dai *fe = &abe_data.frontend[dai->id];
	int ret = 0, i;

	printk("%s: frontend %s \n", __func__, rtd->dai_link->name);
	/* only prepare backends that are either sinks or sources to
	 * this frontend DAI */
	for (i = 0; i < NUM_ABE_BACKENDS; i++) {
		struct abe_backend_dai *be =
			&abe_data.be[i][substream->stream];

		/* make sure we only hw_params backend DAIs that support
		 * the same stream direction */
		if (!fe_stream[dai->id].dir[substream->stream] ||
				!be_stream[i].dir[substream->stream])
			continue;

		/* we dont need to call backend DAI if it's not enabled */
		if (!fe->be_enabled[i])
			continue;

		/* we dont need to hw_param backend DAI if it's inactive */
		if (!atomic_read(&be->active))
			continue;

		snd_soc_pcm_prepare(be->substream);
	}

	return ret;
}

static int omap_abe_dai_hw_free(struct snd_pcm_substream *substream,
				  struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct abe_frontend_dai *fe = &abe_data.frontend[dai->id];
	int ret = 0, i;

	printk("%s: frontend %s \n", __func__, rtd->dai_link->name);
	/* only hw_params backends that are either sinks or sources
	 * to this frontend DAI */
	for (i = 0; i < NUM_ABE_BACKENDS; i++) {
		struct abe_backend_dai *be =
			&abe_data.be[i][substream->stream];

		/* make sure we only hw_params backend DAIs that support
		 * the same stream direction */
		if (!fe_stream[dai->id].dir[substream->stream] ||
				!be_stream[i].dir[substream->stream])
			continue;

		/* we dont need to call backend DAI if it's not enabled */
		if (!fe->be_enabled[i])
			continue;

		/* we dont need to hw_param backend DAI if it's inactive */
		if (!atomic_read(&be->active))
			continue;

		snd_soc_pcm_hw_free(be->substream);
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

/* work for ABE PDM-UL backend */
static void capture_work_pdm_ul(struct work_struct *work)
{
	struct abe_backend_dai *be =
			container_of(work, struct abe_backend_dai, work);
	struct snd_soc_pcm_runtime *rtd = be->substream->private_data;

	spin_lock(&be->lock);

	switch (be->cmd) {
	case SNDRV_PCM_TRIGGER_START:
		/* ABE GAINS_AMIC set to mute */
		abe_write_mixer(MIXAUDUL, MUTE_GAIN, RAMP_0MS,
				MIX_AUDUL_INPUT_UPLINK);

               if (!atomic_cmpxchg(&abe_data.mcpdm_ul_cnt, 0, 1)) {
			/* Enable ABE_PDM_UL port */
			abe_enable_data_transfer(PDM_UL_PORT);
			/* DAI work must be started/stopped at least 250us after ABE */
			udelay(250);

			/* start OMAP McPDML IP */
			snd_soc_dai_trigger(be->substream, be->cmd, rtd->cpu_dai);

			/* TODO: enable PDM clock */

		} else {
			atomic_inc(&abe_data.mcpdm_ul_cnt);
		}

		/* Enable sDMA / Enable ABE MM_UL2 */
		abe_enable_data_transfer(MM_UL2_PORT);

		/* Restore ABE GAINS AMIC */
		abe_write_mixer(MIXAUDUL, GAIN_M6dB, RAMP_0MS,
				MIX_AUDUL_INPUT_UPLINK);
		break;
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		/* Enable sDMA / Enable ABE MM_UL2 */
		abe_enable_data_transfer(MM_UL2_PORT);
		break;
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		/* Disable sDMA / Enable ABE MM_UL2 */
		abe_disable_data_transfer(MM_UL2_PORT);
		break;
	case SNDRV_PCM_TRIGGER_STOP:
		/* Disable sDMA / Enable ABE MM_UL2 */
		abe_disable_data_transfer(MM_UL2_PORT);

		if (atomic_dec_and_test(&abe_data.mcpdm_ul_cnt)) {
			/* Disable ABE_PDM_UL port */
			abe_disable_data_transfer(PDM_UL_PORT);

			/* DAI work must be started/stopped at least 250us after ABE */
			udelay(250);

			/* stop OMAP McPDML IP */
			snd_soc_dai_trigger(be->substream, be->cmd, rtd->cpu_dai);

			/* TODO: shutdown PDM clock */

		}
		break;
	default:
		break;
	}

	spin_unlock(&be->lock);
}

/* work for ABE PDM-DL1 backend */
static void playback_work_pdm_dl1(struct work_struct *work)
{
	struct abe_backend_dai *be =
			container_of(work, struct abe_backend_dai, work);
	struct snd_soc_pcm_runtime *rtd = be->substream->private_data;

	spin_lock(&be->lock);

	switch (be->cmd) {
	case SNDRV_PCM_TRIGGER_START:
		if (!atomic_cmpxchg(&abe_data.mcpdm_dl_cnt, 0, 1)) {
			/* start ABE PDM DL1 */
			abe_enable_data_transfer(PDM_DL_PORT);

			/* DAI work must be started/stopped at least 250us after ABE */
			udelay(250);

			/* start OMAP McPDML IP */
			snd_soc_dai_trigger(be->substream, be->cmd, rtd->cpu_dai);
		} else {
			atomic_inc(&abe_data.mcpdm_dl_cnt);
		}

		/* TODO: unmute DL1 */

		/* enable the transfer */
		if (!atomic_cmpxchg(&abe_data.mm_dl_cnt, 0, 1))
			abe_enable_data_transfer(MM_DL_PORT);
		else
			atomic_inc(&abe_data.mm_dl_cnt);

		/* unmute ABE_MM_DL */
		abe_write_mixer(MIXDL1, GAIN_M6dB, RAMP_0MS, MIX_DL1_INPUT_MM_DL);
		break;
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		/* enable the transfer */
		abe_enable_data_transfer(MM_DL_PORT);

		/* unmute ABE_MM_DL */
		abe_write_mixer(MIXDL1, GAIN_M6dB, RAMP_0MS, MIX_DL1_INPUT_MM_DL);
		break;
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		/* disable the transfer */
		abe_disable_data_transfer(MM_DL_PORT);

		/* mute ABE_MM_DL */
		abe_write_mixer(MIXDL1, MUTE_GAIN, RAMP_0MS, MIX_DL1_INPUT_MM_DL);
		break;
	case SNDRV_PCM_TRIGGER_STOP:
		/* disable the transfer */
		if (atomic_dec_and_test(&abe_data.mm_dl_cnt))
			abe_disable_data_transfer(MM_DL_PORT);

		/* mute ABE_MM_DL */
		abe_write_mixer(MIXDL1, MUTE_GAIN, RAMP_0MS, MIX_DL1_INPUT_MM_DL);

		/* TODO :mute Phoenix */

		if (atomic_dec_and_test(&abe_data.mcpdm_dl_cnt)) {
			/* stop ABE PDM DL1 */
			abe_disable_data_transfer(PDM_DL_PORT);

			/* DAI work must be started/stopped at least 250us after ABE */
			udelay(250);

			/* stop OMAP McPDML IP */
			snd_soc_dai_trigger(be->substream, be->cmd, rtd->cpu_dai);
		}
		break;
	default:
		break;
	}

	spin_unlock(&be->lock);
}

/* work for ABE PDM-DL2 backend */ //TODO: check ports here for DL2
static void playback_work_pdm_dl2(struct work_struct *work)
{
	struct abe_backend_dai *be =
			container_of(work, struct abe_backend_dai, work);
	struct snd_soc_pcm_runtime *rtd = be->substream->private_data;

	spin_lock(&be->lock);

	switch (be->cmd) {
	case SNDRV_PCM_TRIGGER_START:
		if (!atomic_cmpxchg(&abe_data.mcpdm_dl_cnt, 0, 1)) {
			/* start ABE PDM DL1 */
			abe_enable_data_transfer(PDM_DL_PORT);

			/* DAI work must be started/stopped at least 250us after ABE */
			udelay(250);

			/* start OMAP McPDML IP */
			snd_soc_dai_trigger(be->substream, be->cmd, rtd->cpu_dai);
		} else {
			atomic_inc(&abe_data.mcpdm_dl_cnt);
		}

		/* TODO: unmute DL1 */

		/* enable the transfer */
		if (!atomic_cmpxchg(&abe_data.mm_dl_cnt, 0, 1))
			abe_enable_data_transfer(MM_DL_PORT);
		else
			atomic_inc(&abe_data.mm_dl_cnt);

		/* unmute ABE_MM_DL */
		abe_write_mixer(MIXDL2, GAIN_M6dB, RAMP_0MS, MIX_DL2_INPUT_MM_DL);
		break;
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		/* enable the transfer */
		abe_enable_data_transfer(MM_DL_PORT);

		/* unmute ABE_MM_DL */
		abe_write_mixer(MIXDL2, GAIN_M6dB, RAMP_0MS, MIX_DL2_INPUT_MM_DL);
		break;
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		/* disable the transfer */
		abe_disable_data_transfer(MM_DL_PORT);

		/* mute ABE_MM_DL */
		abe_write_mixer(MIXDL2, MUTE_GAIN, RAMP_0MS, MIX_DL2_INPUT_MM_DL);
		break;
	case SNDRV_PCM_TRIGGER_STOP:
		/* disable the transfer */
		if (atomic_dec_and_test(&abe_data.mm_dl_cnt))
			abe_disable_data_transfer(MM_DL_PORT);

		/* mute ABE_MM_DL */
		abe_write_mixer(MIXDL2, MUTE_GAIN, RAMP_0MS, MIX_DL2_INPUT_MM_DL);

		/* TODO :mute Phoenix */

		if (atomic_dec_and_test(&abe_data.mcpdm_dl_cnt)) {
			/* stop ABE PDM DL1 */
			abe_disable_data_transfer(PDM_DL_PORT);

			/* DAI work must be started/stopped at least 250us after ABE */
			udelay(250);

			/* stop OMAP McPDML IP */
			snd_soc_dai_trigger(be->substream, be->cmd, rtd->cpu_dai);
		}
		break;
	default:
		break;
	}

	spin_unlock(&be->lock);
}

/* work for ABE PDM-VIB backend */
static void playback_work_pdm_vib(struct work_struct *work)
{
	struct abe_backend_dai *be =
			container_of(work, struct abe_backend_dai, work);
	struct snd_soc_pcm_runtime *rtd = be->substream->private_data;

	spin_lock(&be->lock);

	switch (be->cmd) {
	case SNDRV_PCM_TRIGGER_START:
		if (!atomic_cmpxchg(&abe_data.mcpdm_dl_cnt, 0, 1)) {
			/* start ABE PDM DL1 */
			abe_enable_data_transfer(PDM_DL_PORT);

			/* DAI work must be started/stopped at least 250us after ABE */
			udelay(250);

			/* start OMAP McPDML IP */
			snd_soc_dai_trigger(be->substream, be->cmd, rtd->cpu_dai);
		} else {
			atomic_inc(&abe_data.mcpdm_dl_cnt);
		}

		/* enable the transfer */
		abe_enable_data_transfer(VIB_DL_PORT);
		break;
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		/* enable the transfer */
		abe_enable_data_transfer(VIB_DL_PORT);
		break;
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		/* disable the transfer */
		abe_disable_data_transfer(VIB_DL_PORT);
		break;
	case SNDRV_PCM_TRIGGER_STOP:
		/* disable the transfer */
		abe_disable_data_transfer(VIB_DL_PORT);

		if (atomic_dec_and_test(&abe_data.mcpdm_dl_cnt)) {
			/* stop ABE PDM DL1 */
			abe_disable_data_transfer(PDM_DL_PORT);

			/* DAI work must be started/stopped at least 250us after ABE */
			udelay(250);

			/* stop OMAP McPDML IP */
			snd_soc_dai_trigger(be->substream, be->cmd, rtd->cpu_dai);
		}
		break;
	default:
		break;
	}

	spin_unlock(&be->lock);
}

/* work for ABE VX backend */
static void playback_work_vx(struct work_struct *work)
{
	struct abe_backend_dai *be =
			container_of(work, struct abe_backend_dai, work);
	struct snd_soc_pcm_runtime *rtd = be->substream->private_data;

	spin_lock(&be->lock);

	switch (be->cmd) {
	case SNDRV_PCM_TRIGGER_START:
		/* start ABE VX */
		abe_enable_data_transfer(BT_VX_DL_PORT);

		/* DAI work must be started/stopped at least 250us after ABE */
		udelay(250);

		/* start OMAP McBSP IP */
		snd_soc_dai_trigger(be->substream, be->cmd, rtd->cpu_dai);

		/* TODO: unmute DL1 */

		/* enable the transfer */
		abe_enable_data_transfer(VX_DL_PORT);

		/* unmute ABE_MM_DL */
		abe_write_mixer(MIXDL1, GAIN_M6dB, RAMP_0MS, MIX_DL1_INPUT_VX_DL);
		break;
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		/* enable the transfer */
		abe_enable_data_transfer(VX_DL_PORT);

		/* unmute ABE_MM_DL */
		abe_write_mixer(MIXDL1, GAIN_M6dB, RAMP_0MS, MIX_DL1_INPUT_VX_DL);
		break;
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		/* disable the transfer */
		abe_disable_data_transfer(VX_DL_PORT);

		/* mute ABE_MM_DL */
		abe_write_mixer(MIXDL1, MUTE_GAIN, RAMP_0MS, MIX_DL1_INPUT_VX_DL);
		break;
	case SNDRV_PCM_TRIGGER_STOP:
		/* disable the transfer */
		abe_disable_data_transfer(VX_DL_PORT);

		/* mute ABE_MM_DL */
		abe_write_mixer(MIXDL1, MUTE_GAIN, RAMP_0MS, MIX_DL1_INPUT_VX_DL);

		/* TODO :mute Phoenix */

		/* stop ABE PDM DL1 */
		abe_disable_data_transfer(BT_VX_DL_PORT);

		/* DAI work must be started/stopped at least 250us after ABE */
		udelay(250);

		/* stop OMAP McPDML IP */
		snd_soc_dai_trigger(be->substream, be->cmd, rtd->cpu_dai);
		break;
	default:
		break;
	}

	spin_unlock(&be->lock);
}

static void capture_work_vx(struct work_struct *work)
{
	struct abe_backend_dai *be =
			container_of(work, struct abe_backend_dai, work);
	struct snd_soc_pcm_runtime *rtd = be->substream->private_data;

	spin_lock(&be->lock);

	switch (be->cmd) {
	case SNDRV_PCM_TRIGGER_START:
		/* ABE GAINS_AMIC set to mute */
	//	abe_write_mixer(MIXAUDUL, MUTE_GAIN, RAMP_0MS,
	//			MIX_AUDUL_INPUT_UPLINK);

		/*Enable ABE_PDM_UL port */
		abe_enable_data_transfer(BT_VX_UL_PORT);

		/* Enable sDMA / Enable ABE MM_UL2 */
		abe_enable_data_transfer(VX_DL_PORT);

		/* DAI work must be started/stopped at least 250us after ABE */
		udelay(250);

		/* start OMAP McPDML IP */
		snd_soc_dai_trigger(be->substream, be->cmd, rtd->cpu_dai);

		/* TODO: enable PDM clock */

		/* Restore ABE GAINS AMIC */
	//	abe_write_mixer(MIXAUDUL, GAIN_M6dB, RAMP_0MS,
	//			MIX_AUDUL_INPUT_UPLINK);
		break;
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		/* Enable sDMA / Enable ABE MM_UL2 */
		abe_enable_data_transfer(VX_DL_PORT);
		break;
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		/* Disable sDMA / Enable ABE MM_UL2 */
		abe_disable_data_transfer(VX_DL_PORT);
		break;
	case SNDRV_PCM_TRIGGER_STOP:
		/* Disable sDMA / Enable ABE MM_UL2 */
		abe_disable_data_transfer(VX_DL_PORT);

		/*Disable ABE_PDM_UL port */
		abe_disable_data_transfer(BT_VX_UL_PORT);

		/* DAI work must be started/stopped at least 250us after ABE */
		udelay(250);

		/* stop OMAP McPDML IP */
		snd_soc_dai_trigger(be->substream, be->cmd, rtd->cpu_dai);

		/* TODO: shutdown PDM clock */
		break;
	default:
		break;
	}

	spin_unlock(&be->lock);
}

/* work for ABE MM-ext backend */
static void playback_work_mm(struct work_struct *work)
{
	struct abe_backend_dai *be =
			container_of(work, struct abe_backend_dai, work);
	struct snd_soc_pcm_runtime *rtd = be->substream->private_data;

	spin_lock(&be->lock);

	switch (be->cmd) {
	case SNDRV_PCM_TRIGGER_START:
		/* start ABE PDM DL1 */
		abe_enable_data_transfer(MM_EXT_OUT_PORT);

		/* DAI work must be started/stopped at least 250us after ABE */
		udelay(250);

		/* start OMAP McBSP IP */
		snd_soc_dai_trigger(be->substream, be->cmd, rtd->cpu_dai);

		/* TODO: unmute DL1 */

		/* enable the transfer */
		if (!atomic_cmpxchg(&abe_data.mm_dl_cnt, 0, 1))
			abe_enable_data_transfer(MM_DL_PORT);
		else
			atomic_inc(&abe_data.mm_dl_cnt);

		/* unmute ABE_MM_DL */
		abe_write_mixer(MIXDL1, GAIN_M6dB, RAMP_0MS, MIX_DL1_INPUT_VX_DL);
		break;
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		/* enable the transfer */
		abe_enable_data_transfer(MM_DL_PORT);

		/* unmute ABE_MM_DL */
		abe_write_mixer(MIXDL1, GAIN_M6dB, RAMP_0MS, MIX_DL1_INPUT_VX_DL);
		break;
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		/* disable the transfer */
		abe_disable_data_transfer(MM_DL_PORT);

		/* mute ABE_MM_DL */
		abe_write_mixer(MIXDL1, MUTE_GAIN, RAMP_0MS, MIX_DL1_INPUT_VX_DL);
		break;
	case SNDRV_PCM_TRIGGER_STOP:
		/* disable the transfer */
		if (atomic_dec_and_test(&abe_data.mm_dl_cnt))
			abe_disable_data_transfer(MM_DL_PORT);

		/* mute ABE_MM_DL */
		abe_write_mixer(MIXDL1, MUTE_GAIN, RAMP_0MS, MIX_DL1_INPUT_VX_DL);

		/* TODO :mute Phoenix */

		/* stop ABE PDM DL1 */
		abe_disable_data_transfer(MM_EXT_OUT_PORT);

		/* DAI work must be started/stopped at least 250us after ABE */
		udelay(250);

		/* stop OMAP McPDML IP */
		snd_soc_dai_trigger(be->substream, be->cmd, rtd->cpu_dai);
		break;
	default:
		break;
	}

	spin_unlock(&be->lock);
}

static void capture_work_mm(struct work_struct *work)
{
	struct abe_backend_dai *be =
			container_of(work, struct abe_backend_dai, work);
	struct snd_soc_pcm_runtime *rtd = be->substream->private_data;

	spin_lock(&be->lock);

	switch (be->cmd) {
	case SNDRV_PCM_TRIGGER_START:
		/* ABE GAINS_AMIC set to mute */
		abe_write_mixer(MIXAUDUL, MUTE_GAIN, RAMP_0MS,
				MIX_AUDUL_INPUT_UPLINK);

		/*Enable ABE_PDM_UL port */
		abe_enable_data_transfer(MM_EXT_IN_PORT);

		/* Enable sDMA / Enable ABE MM_UL2 */
		abe_enable_data_transfer(MM_UL_PORT);

		/* DAI work must be started/stopped at least 250us after ABE */
		udelay(250);

		/* start OMAP McPDML IP */
		snd_soc_dai_trigger(be->substream, be->cmd, rtd->cpu_dai);

		/* TODO: enable PDM clock */

		/* Restore ABE GAINS AMIC */
		abe_write_mixer(MIXAUDUL, GAIN_M6dB, RAMP_0MS,
				MIX_AUDUL_INPUT_UPLINK);
		break;
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		/* Enable sDMA / Enable ABE MM_UL2 */
		abe_enable_data_transfer(MM_UL_PORT);
		break;
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		/* Disable sDMA / Enable ABE MM_UL2 */
		abe_disable_data_transfer(MM_UL_PORT);
		break;
	case SNDRV_PCM_TRIGGER_STOP:
		/* Disable sDMA / Enable ABE MM_UL2 */
		abe_disable_data_transfer(MM_UL_PORT);

		/*Disable ABE_PDM_UL port */
		abe_disable_data_transfer(MM_EXT_IN_PORT);

		/* DAI work must be started/stopped at least 250us after ABE */
		udelay(250);

		/* stop OMAP McPDML IP */
		snd_soc_dai_trigger(be->substream, be->cmd, rtd->cpu_dai);

		/* TODO: shutdown PDM clock */
		break;
	default:
		break;
	}

	spin_unlock(&be->lock);
}

/* work for ABE DMIC1 backend */
static void capture_work_dmic0(struct work_struct *work)
{
	struct abe_backend_dai *be =
			container_of(work, struct abe_backend_dai, work);
	struct snd_soc_pcm_runtime *rtd = be->substream->private_data;

	spin_lock(&be->lock);

	switch (be->cmd) {
	case SNDRV_PCM_TRIGGER_START:
		/* ABE GAINS_AMIC set to mute */
		abe_write_mixer(MIXAUDUL, MUTE_GAIN, RAMP_0MS,
				MIX_AUDUL_INPUT_UPLINK);

		/*Enable ABE_PDM_UL port */
		abe_enable_data_transfer(DMIC_PORT);

		/* Enable sDMA / Enable ABE MM_UL2 */
		abe_enable_data_transfer(MM_UL_PORT);

		/* DAI work must be started/stopped at least 250us after ABE */
		udelay(250);

		/* start OMAP McPDML IP */
		snd_soc_dai_trigger(be->substream, be->cmd, rtd->cpu_dai);

		/* TODO: enable PDM clock */

		/* Restore ABE GAINS AMIC */
		abe_write_mixer(MIXAUDUL, GAIN_M6dB, RAMP_0MS,
				MIX_AUDUL_INPUT_UPLINK);
		break;
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		/* Enable sDMA / Enable ABE MM_UL2 */
		abe_enable_data_transfer(MM_UL_PORT);
		break;
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		/* Disable sDMA / Enable ABE MM_UL2 */
		abe_disable_data_transfer(MM_UL_PORT);
		break;
	case SNDRV_PCM_TRIGGER_STOP:
		/* Disable sDMA / Enable ABE MM_UL2 */
		abe_disable_data_transfer(MM_UL_PORT);

		/*Disable ABE_PDM_UL port */
		abe_disable_data_transfer(DMIC_PORT);

		/* DAI work must be started/stopped at least 250us after ABE */
		udelay(250);

		/* stop OMAP McPDML IP */
		snd_soc_dai_trigger(be->substream, be->cmd, rtd->cpu_dai);

		/* TODO: shutdown PDM clock */
		break;
	default:
		break;
	}

	spin_unlock(&be->lock);
}

/* work for ABE DMIC2 backend */
static void capture_work_dmic1(struct work_struct *work)
{
	struct abe_backend_dai *be =
			container_of(work, struct abe_backend_dai, work);
	struct snd_soc_pcm_runtime *rtd = be->substream->private_data;

	spin_lock(&be->lock);

	switch (be->cmd) {
	case SNDRV_PCM_TRIGGER_START:
		/* ABE GAINS_AMIC set to mute */
		abe_write_mixer(MIXAUDUL, MUTE_GAIN, RAMP_0MS,
				MIX_AUDUL_INPUT_UPLINK);

               if (!atomic_cmpxchg(&abe_data.mcpdm_ul_cnt, 0, 1)) {
			/* Enable ABE_PDM_UL port */
			abe_enable_data_transfer(PDM_UL_PORT);
			/* DAI work must be started/stopped at least 250us after ABE */
			udelay(250);

			/* start OMAP McPDML IP */
			snd_soc_dai_trigger(be->substream, be->cmd, rtd->cpu_dai);

			/* TODO: enable PDM clock */

		} else {
			atomic_inc(&abe_data.mcpdm_ul_cnt);
		}

		/* Enable sDMA / Enable ABE MM_UL2 */
		abe_enable_data_transfer(MM_UL2_PORT);

		/* Restore ABE GAINS AMIC */
		abe_write_mixer(MIXAUDUL, GAIN_M6dB, RAMP_0MS,
				MIX_AUDUL_INPUT_UPLINK);
		break;
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		/* Enable sDMA / Enable ABE MM_UL2 */
		abe_enable_data_transfer(MM_UL2_PORT);
		break;
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		/* Disable sDMA / Enable ABE MM_UL2 */
		abe_disable_data_transfer(MM_UL2_PORT);
		break;
	case SNDRV_PCM_TRIGGER_STOP:
		/* Disable sDMA / Enable ABE MM_UL2 */
		abe_disable_data_transfer(MM_UL2_PORT);

		if (atomic_dec_and_test(&abe_data.mcpdm_ul_cnt)) {
			/* Disable ABE_PDM_UL port */
			abe_disable_data_transfer(PDM_UL_PORT);

			/* DAI work must be started/stopped at least 250us after ABE */
			udelay(250);

			/* stop OMAP McPDML IP */
			snd_soc_dai_trigger(be->substream, be->cmd, rtd->cpu_dai);

			/* TODO: shutdown PDM clock */

		}
		break;
	default:
		break;
	}

	spin_unlock(&be->lock);
}

/* work for ABE DMIC3 backend */
static void capture_work_dmic2(struct work_struct *work)
{
	struct abe_backend_dai *be =
			container_of(work, struct abe_backend_dai, work);
	struct snd_soc_pcm_runtime *rtd = be->substream->private_data;

	spin_lock(&be->lock);

	switch (be->cmd) {
	case SNDRV_PCM_TRIGGER_START:
		/* ABE GAINS_AMIC set to mute */
		abe_write_mixer(MIXAUDUL, MUTE_GAIN, RAMP_0MS,
				MIX_AUDUL_INPUT_UPLINK);

               if (!atomic_cmpxchg(&abe_data.mcpdm_ul_cnt, 0, 1)) {
			/* Enable ABE_PDM_UL port */
			abe_enable_data_transfer(PDM_UL_PORT);
			/* DAI work must be started/stopped at least 250us after ABE */
			udelay(250);

			/* start OMAP McPDML IP */
			snd_soc_dai_trigger(be->substream, be->cmd, rtd->cpu_dai);

			/* TODO: enable PDM clock */

		} else {
			atomic_inc(&abe_data.mcpdm_ul_cnt);
		}

		/* Enable sDMA / Enable ABE MM_UL2 */
		abe_enable_data_transfer(MM_UL2_PORT);

		/* Restore ABE GAINS AMIC */
		abe_write_mixer(MIXAUDUL, GAIN_M6dB, RAMP_0MS,
				MIX_AUDUL_INPUT_UPLINK);
		break;
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		/* Enable sDMA / Enable ABE MM_UL2 */
		abe_enable_data_transfer(MM_UL2_PORT);
		break;
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		/* Disable sDMA / Enable ABE MM_UL2 */
		abe_disable_data_transfer(MM_UL2_PORT);
		break;
	case SNDRV_PCM_TRIGGER_STOP:
		/* Disable sDMA / Enable ABE MM_UL2 */
		abe_disable_data_transfer(MM_UL2_PORT);

		if (atomic_dec_and_test(&abe_data.mcpdm_ul_cnt)) {
			/* Disable ABE_PDM_UL port */
			abe_disable_data_transfer(PDM_UL_PORT);

			/* DAI work must be started/stopped at least 250us after ABE */
			udelay(250);

			/* stop OMAP McPDML IP */
			snd_soc_dai_trigger(be->substream, be->cmd, rtd->cpu_dai);

			/* TODO: shutdown PDM clock */

		}
		break;
	default:
		break;
	}

	spin_unlock(&be->lock);
}

static int omap_abe_dai_probe(struct snd_soc_dai *dai)
{

	snd_soc_dai_set_drvdata(dai, &abe_data);

	/* MCPDM UL */
	INIT_WORK(&abe_data.be[ABE_DAI_PDM_UL][SNDRV_PCM_STREAM_CAPTURE].work,
		capture_work_pdm_ul);

	/* MCPDM DL 1 */
	INIT_WORK(&abe_data.be[ABE_DAI_PDM_DL1][SNDRV_PCM_STREAM_PLAYBACK].work,
		playback_work_pdm_dl1);

	/* MCPDM DL 2 */
	INIT_WORK(&abe_data.be[ABE_DAI_PDM_DL2][SNDRV_PCM_STREAM_PLAYBACK].work,
		playback_work_pdm_dl2);

	/* MCPDM VIB */
	INIT_WORK(&abe_data.be[ABE_DAI_PDM_VIB][SNDRV_PCM_STREAM_PLAYBACK].work,
		playback_work_pdm_vib);

	/* MCBSP Voice */
	INIT_WORK(&abe_data.be[ABE_DAI_BT_VX_DL][SNDRV_PCM_STREAM_PLAYBACK].work,
		playback_work_vx);
	INIT_WORK(&abe_data.be[ABE_DAI_BT_VX_UL][SNDRV_PCM_STREAM_CAPTURE].work,
		capture_work_vx);

	/* MCBSP Media */
	INIT_WORK(&abe_data.be[ABE_DAI_MM_DL][SNDRV_PCM_STREAM_PLAYBACK].work,
		playback_work_mm);
	INIT_WORK(&abe_data.be[ABE_DAI_MM_UL][SNDRV_PCM_STREAM_CAPTURE].work,
		capture_work_mm);

	/* DMIC 1,2,3 */
	INIT_WORK(&abe_data.be[ABE_DAI_DMIC0][SNDRV_PCM_STREAM_CAPTURE].work,
		capture_work_dmic0);
	INIT_WORK(&abe_data.be[ABE_DAI_DMIC1][SNDRV_PCM_STREAM_CAPTURE].work,
		capture_work_dmic1);
	INIT_WORK(&abe_data.be[ABE_DAI_DMIC2][SNDRV_PCM_STREAM_CAPTURE].work,
		capture_work_dmic2);

	return 0;
}

static struct snd_soc_dai_driver omap_abe_dai[] = {
	{	/* Multimedia Playback and Capture */
		.name = "Media",
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
		.name = "Media Capture",
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
