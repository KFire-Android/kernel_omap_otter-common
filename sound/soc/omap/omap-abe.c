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

#undef DEBUG

#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/workqueue.h>
#include <linux/delay.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/initval.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>

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
 * 1) Get DAI params from HAL and pass onto DAI drivers ?
 *    (are params static ??, i.e. can I just use a table)
 */

#define NUM_ABE_FRONTENDS		7
#define NUM_ABE_BACKENDS		11

/* logical -> physical DAI status */
enum dai_status {
	DAI_STOPPED = 0,
	DAI_STARTED,
};

struct omap_abe_data {
	int be_active[NUM_ABE_BACKENDS][2];
	int trigger_active[NUM_ABE_BACKENDS][2];

	struct clk *clk;
	struct workqueue_struct *workqueue;

	/* hwmod platform device */
	struct platform_device *pdev;

	/* MODEM FE*/
	struct snd_pcm_substream *modem_substream[2];
	struct snd_soc_dai *modem_dai;

	/* reference counting and port status - HAL should really do this */
	enum dai_status dmic_status[3];
	enum dai_status pdm_dl_status[3];
	enum dai_status pdm_ul_status;

};

static struct omap_abe_data abe_data;
static void capture_trigger(struct snd_pcm_substream *substream, int cmd);
static void playback_trigger(struct snd_pcm_substream *substream, int cmd);

/* frontend mutex */
static DEFINE_MUTEX(fe_mutex);

/*
 * Stream DMA parameters
 */
static struct omap_pcm_dma_data omap_abe_dai_dma_params[7][2] = {
{
	{
		.name = "Media Playback",
		.dma_req = OMAP44XX_DMA_ABE_REQ_0,
		.data_type = OMAP_DMA_DATA_TYPE_S32,
		.sync_mode = OMAP_DMA_SYNC_PACKET,
	},
	{
		.name = "Media Capture1",
		.dma_req = OMAP44XX_DMA_ABE_REQ_3,
		.data_type = OMAP_DMA_DATA_TYPE_S32,
		.sync_mode = OMAP_DMA_SYNC_PACKET,
	},
},
{
	{},
	{
		.name = "Media Capture2",
		.dma_req = OMAP44XX_DMA_ABE_REQ_4,
		.data_type = OMAP_DMA_DATA_TYPE_S32,
		.sync_mode = OMAP_DMA_SYNC_PACKET,
	},
},
{
	{
		.name = "Voice Playback",
		.dma_req = OMAP44XX_DMA_ABE_REQ_1,
		.data_type = OMAP_DMA_DATA_TYPE_S32,
		.sync_mode = OMAP_DMA_SYNC_PACKET,
	},
	{
		.name = "Voice Capture",
		.dma_req = OMAP44XX_DMA_ABE_REQ_2,
		.data_type = OMAP_DMA_DATA_TYPE_S32,
		.sync_mode = OMAP_DMA_SYNC_PACKET,
	},
},
{
	{
		.name = "Tones Playback",
		.dma_req = OMAP44XX_DMA_ABE_REQ_5,
		.data_type = OMAP_DMA_DATA_TYPE_S32,
		.sync_mode = OMAP_DMA_SYNC_PACKET,
	},{},
},
{
	{
		.name = "Vibra Playback",
		.dma_req = OMAP44XX_DMA_ABE_REQ_6,
		.data_type = OMAP_DMA_DATA_TYPE_S32,
		.sync_mode = OMAP_DMA_SYNC_PACKET,
	},{},
},
{
	{
		.name = "MODEM Playback",
		.dma_req = OMAP44XX_DMA_ABE_REQ_1,
		.data_type = OMAP_DMA_DATA_TYPE_S32,
		.sync_mode = OMAP_DMA_SYNC_PACKET,
	},
	{
		.name = "MODEM Capture",
		.dma_req = OMAP44XX_DMA_ABE_REQ_2,
		.data_type = OMAP_DMA_DATA_TYPE_S32,
		.sync_mode = OMAP_DMA_SYNC_PACKET,
	},
},
{
	{
		.name = "Low Power Playback",
		.dma_req = OMAP44XX_DMA_ABE_REQ_0,
		.data_type = OMAP_DMA_DATA_TYPE_S32,
		.sync_mode = OMAP_DMA_SYNC_PACKET,
	},{},
},};

/*
 * Caller holds lock for be_active calls.
 */
static inline int be_get_active(struct snd_soc_pcm_runtime *be_rtd, int stream)
{
	return abe_data.be_active[be_rtd->dai_link->be_id][stream];
}

static inline void be_inc_active(struct snd_soc_pcm_runtime *be_rtd, int stream)
{
	abe_data.be_active[be_rtd->dai_link->be_id][stream]++;
}

static inline void be_dec_active(struct snd_soc_pcm_runtime *be_rtd, int stream)
{
	abe_data.be_active[be_rtd->dai_link->be_id][stream]--;
}

/* iff the BE has one user can we start and stop the port */
static inline int be_is_pending(struct snd_soc_pcm_runtime *be_rtd, int stream)
{
	return abe_data.be_active[be_rtd->dai_link->be_id][stream] == 1 ? 1 : 0;
}

static inline int be_is_running(struct snd_soc_pcm_runtime *be_rtd, int stream)
{
	return abe_data.be_active[be_rtd->dai_link->be_id][stream] >= 1 ? 1 : 0;
}

/*
 * Caller holds lock for trigger_active calls.
 */
static inline int trigger_get_active(struct snd_soc_pcm_runtime *be_rtd,
					int stream)
{
	return abe_data.trigger_active[be_rtd->dai_link->be_id][stream];
}

static inline void trigger_inc_active(struct snd_soc_pcm_runtime *be_rtd,
					int stream)
{
	abe_data.trigger_active[be_rtd->dai_link->be_id][stream]++;
}

static inline void trigger_dec_active(struct snd_soc_pcm_runtime *be_rtd,
					int stream)
{
	abe_data.trigger_active[be_rtd->dai_link->be_id][stream]--;
}

/* iff the BE has one user can we start and stop the port */
static inline int trigger_is_pending(struct snd_soc_pcm_runtime *be_rtd,
					int stream)
{
	return abe_data.trigger_active[be_rtd->dai_link->be_id][stream] == 1 ? 1 : 0;
}

/*
 * consolidate our 3 PDM and DMICs.
 */
static inline int pdm_ready(struct omap_abe_data *abe)
{
	int i;

	/* check the 3 DL DAIs */
	for (i = 0; i < 3; i++) {
		if (abe->pdm_dl_status[i] == DAI_STARTED)
			return 0;
	}

	return 1;
}

static inline int dmic_ready(struct omap_abe_data *abe)
{
	int i;

	/* check the 3 DMIC DAIs */
	for (i = 0; i < 3; i++) {
		if (abe->dmic_status[i] == DAI_STARTED)
			return 0;
	}

	return 1;
}

static int modem_get_dai(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_pcm_runtime *modem_rtd;

	abe_data.modem_substream[substream->stream] =
			snd_soc_get_dai_substream(rtd->card,
					OMAP_ABE_BE_MM_EXT1, substream->stream);
	if (abe_data.modem_substream[substream->stream] == NULL)
		return -ENODEV;

	modem_rtd = abe_data.modem_substream[substream->stream]->private_data;
	abe_data.modem_dai = modem_rtd->cpu_dai;
	return 0;
}

/* Frontend PCM Operations */

static int abe_fe_startup(struct snd_pcm_substream *substream,
			struct snd_soc_dai *dai)
{
	int ret = 0;

	/* TODO: complete HW  pcm for backends */
#if 0
	snd_pcm_hw_constraint_list(substream->runtime, 0,
				SNDRV_PCM_HW_PARAM_RATE,
				priv->sysclk_constraints);
#endif

	if (dai->id == ABE_FRONTEND_DAI_MODEM) {

		ret = modem_get_dai(substream);
		if (ret < 0) {
			dev_err(dai->dev, "failed to get MODEM DAI\n");
			return ret;
		}
		dev_dbg(abe_data.modem_dai->dev, "%s: MODEM stream %d\n",
				__func__, substream->stream);

		ret = snd_soc_dai_startup(abe_data.modem_substream[substream->stream],
				abe_data.modem_dai);
		if (ret < 0) {
			dev_err(abe_data.modem_dai->dev, "failed to open DAI %d\n", ret);
			return ret;
		}
	}

	return ret;
}

static int abe_fe_hw_params(struct snd_pcm_substream *substream,
			struct snd_pcm_hw_params *params,
			struct snd_soc_dai *dai)
{
	abe_data_format_t format;
	abe_dma_t dma_sink;
	abe_dma_t dma_params;
	int ret;

	switch (params_channels(params)) {
	case 1:
		if (params_format(params) == SNDRV_PCM_FORMAT_S16_LE)
			format.samp_format = MONO_RSHIFTED_16;
		else
			format.samp_format = MONO_MSB;
		break;
	case 2:
		if (params_format(params) == SNDRV_PCM_FORMAT_S16_LE)
			format.samp_format = STEREO_16_16;
		else
			format.samp_format = STEREO_MSB;
		break;
	case 3:
		format.samp_format = THREE_MSB;
		break;
	case 4:
		format.samp_format = FOUR_MSB;
		break;
	case 5:
		format.samp_format = FIVE_MSB;
		break;
	case 6 :
		format.samp_format = SIX_MSB;
		break;
	case 7 :
		format.samp_format = SEVEN_MSB;
		break;
	case 8:
		format.samp_format = EIGHT_MSB;
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
			abe_connect_cbpr_dmareq_port(MM_DL_PORT, &format, ABE_CBPR0_IDX,
					&dma_sink);
			abe_read_port_address(MM_DL_PORT, &dma_params);
		} else {
			abe_connect_cbpr_dmareq_port(MM_UL_PORT, &format,  ABE_CBPR3_IDX,
					&dma_sink);
			abe_read_port_address(MM_UL_PORT, &dma_params);
		}
        break;
	case ABE_FRONTEND_DAI_LP_MEDIA:
		return 0;
        break;
	case ABE_FRONTEND_DAI_MEDIA_CAPTURE:
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
			return -EINVAL;
		else {
			abe_connect_cbpr_dmareq_port(MM_UL2_PORT, &format,  ABE_CBPR4_IDX,
					&dma_sink);
			abe_read_port_address(MM_UL2_PORT, &dma_params);
		}
        break;
	case ABE_FRONTEND_DAI_VOICE:
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
			abe_connect_cbpr_dmareq_port(VX_DL_PORT, &format, ABE_CBPR1_IDX,
					&dma_sink);
			abe_read_port_address(VX_DL_PORT, &dma_params);
		} else {
			abe_connect_cbpr_dmareq_port(VX_UL_PORT, &format,  ABE_CBPR2_IDX,
					&dma_sink);
			abe_read_port_address(VX_UL_PORT, &dma_params);
		}
        break;
	case ABE_FRONTEND_DAI_TONES:
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
			abe_connect_cbpr_dmareq_port(TONES_DL_PORT, &format, ABE_CBPR5_IDX,
					&dma_sink);
			abe_read_port_address(TONES_DL_PORT, &dma_params);
		} else
			return -EINVAL;
        break;
	case ABE_FRONTEND_DAI_VIBRA:
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
			abe_connect_cbpr_dmareq_port(VIB_DL_PORT, &format, ABE_CBPR6_IDX,
					&dma_sink);
			abe_read_port_address(VIB_DL_PORT, &dma_params);
		} else
			return -EINVAL;
		break;
	case ABE_FRONTEND_DAI_MODEM:
		/* MODEM is special case where data IO is performed by McBSP2
		 * directly onto VX_DL and VX_UL (instead of SDMA).
		 */
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
			/* Vx_DL connection to McBSP 2 ports */
			format.samp_format = STEREO_RSHIFTED_16;
			abe_connect_serial_port(VX_DL_PORT, &format, MCBSP2_RX);
			abe_read_port_address(VX_DL_PORT, &dma_params);
		} else {
			/* Vx_UL connection to McBSP 2 ports */
			format.samp_format = STEREO_RSHIFTED_16;
			abe_connect_serial_port(VX_UL_PORT, &format, MCBSP2_TX);
			abe_read_port_address(VX_UL_PORT, &dma_params);
		}
        break;
	}

	/* configure frontend SDMA data */
	omap_abe_dai_dma_params[dai->id][substream->stream].port_addr =
			(unsigned long)dma_params.data;
	omap_abe_dai_dma_params[dai->id][substream->stream].packet_size =
			dma_params.iter;

	if (dai->id == ABE_FRONTEND_DAI_MODEM) {
		/* call hw_params on McBSP with correct DMA data */
		snd_soc_dai_set_dma_data(abe_data.modem_dai, substream,
				&omap_abe_dai_dma_params[dai->id][substream->stream]);

		dev_dbg(abe_data.modem_dai->dev, "%s: MODEM stream %d\n",
				__func__, substream->stream);

		ret = snd_soc_dai_hw_params(abe_data.modem_substream[substream->stream],
				params, abe_data.modem_dai);
		if (ret < 0)
			dev_err(abe_data.modem_dai->dev, "MODEM hw_params failed\n");
		return ret;
	}

	snd_soc_dai_set_dma_data(dai, substream,
				&omap_abe_dai_dma_params[dai->id][substream->stream]);

	return 0;
}

static int abe_fe_prepare(struct snd_pcm_substream *substream,
				  struct snd_soc_dai *dai)
{
	int ret = 0;

	if (dai->id == ABE_FRONTEND_DAI_MODEM) {
		ret = snd_soc_dai_prepare(abe_data.modem_substream[substream->stream],
				abe_data.modem_dai);

		dev_dbg(abe_data.modem_dai->dev, "%s: MODEM stream %d\n",
				__func__, substream->stream);

		if (ret < 0) {
			dev_err(abe_data.modem_dai->dev, "MODEM prepare failed\n");
			return ret;
		}
	}
	return ret;
}

static int abe_fe_trigger(struct snd_pcm_substream *substream,
				  int cmd, struct snd_soc_dai *dai)
{
	int ret = 0;

	if (dai->id == ABE_FRONTEND_DAI_MODEM) {

		dev_dbg(abe_data.modem_dai->dev, "%s: MODEM stream %d cmd %d\n",
				__func__, substream->stream, cmd);

		ret = snd_soc_dai_trigger(abe_data.modem_substream[substream->stream],
				cmd, abe_data.modem_dai);
		if (ret < 0) {
			dev_err(abe_data.modem_dai->dev, "MODEM trigger failed\n");
			return ret;
		}
	}
	return ret;
}

static int abe_fe_hw_free(struct snd_pcm_substream *substream,
				  struct snd_soc_dai *dai)
{
	int ret = 0;

	if (dai->id == ABE_FRONTEND_DAI_MODEM) {

		dev_dbg(abe_data.modem_dai->dev, "%s: MODEM stream %d\n",
				__func__, substream->stream);

		ret = snd_soc_dai_hw_free(abe_data.modem_substream[substream->stream],
				abe_data.modem_dai);
		if (ret < 0) {
			dev_err(abe_data.modem_dai->dev, "MODEM hw_free failed\n");
			return ret;
		}
	}
	return ret;
}

static void abe_fe_shutdown(struct snd_pcm_substream *substream,
				struct snd_soc_dai *dai)
{

	if (dai->id == ABE_FRONTEND_DAI_MODEM) {
		dev_dbg(abe_data.modem_dai->dev, "%s: MODEM stream %d\n",
				__func__, substream->stream);

		snd_soc_dai_shutdown(abe_data.modem_substream[substream->stream],
				abe_data.modem_dai);
	}
	//TODO: Do we need to do reset this stuff i.e. :-
	//abe_connect_cbpr_dmareq_port(VIB_DL_PORT, &format, ABE_CBPR6_IDX,
	//				&dma_sink);
}

static void abe_be_dapm(struct snd_soc_pcm_runtime *rtd,
		int id, int stream, int cmd)
{
	dev_dbg(&rtd->dev, "%s: id %d stream %d cmd %d\n",
			__func__, id, stream, cmd);

	switch (id) {
	case ABE_FRONTEND_DAI_MEDIA:
	case ABE_FRONTEND_DAI_LP_MEDIA:
		if (stream == SNDRV_PCM_STREAM_PLAYBACK)
			snd_soc_dapm_stream_event(rtd, stream, "Multimedia Playback",cmd);
		else
			snd_soc_dapm_stream_event(rtd, stream, "Multimedia Capture1",cmd);
		break;
	case ABE_FRONTEND_DAI_MEDIA_CAPTURE:
		snd_soc_dapm_stream_event(rtd, stream, "Multimedia Capture2",cmd);
		break;
	case ABE_FRONTEND_DAI_VOICE:
		if (stream == SNDRV_PCM_STREAM_PLAYBACK)
			snd_soc_dapm_stream_event(rtd, stream, "Voice Playback",cmd);
		else
			snd_soc_dapm_stream_event(rtd, stream, "Voice Capture",cmd);
		break;
	case ABE_FRONTEND_DAI_TONES:
		if (stream == SNDRV_PCM_STREAM_PLAYBACK)
			snd_soc_dapm_stream_event(rtd, stream, "Tones Playback",cmd);
		break;
	case ABE_FRONTEND_DAI_VIBRA:
		if (stream == SNDRV_PCM_STREAM_PLAYBACK)
			snd_soc_dapm_stream_event(rtd, stream, "Vibra Playback",cmd);
		break;
	case ABE_FRONTEND_DAI_MODEM:
		if (stream == SNDRV_PCM_STREAM_PLAYBACK)
			snd_soc_dapm_stream_event(rtd, stream, "MODEM Playback",cmd);
		else
			snd_soc_dapm_stream_event(rtd, stream, "MODEM Capture",cmd);
		break;
	}
}

/* Frontend --> Backend ALSA PCM OPS */

static int omap_abe_dai_startup(struct snd_pcm_substream *substream,
				  struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	int stream = substream->stream, ret = 0, i;

	dev_dbg(dai->dev, "%s: frontend %s %d\n", __func__,
			rtd->dai_link->name, dai->id);

	mutex_lock(&fe_mutex);

	/* only startup BE DAIs that are either sinks or sources to this FE DAI */
	for (i = 0; i < rtd->num_be[stream]; i++) {
		struct snd_soc_pcm_runtime *be_rtd = rtd->be_rtd[i][stream];
		struct snd_pcm_substream *be_substream =
			be_rtd->pcm->streams[stream].substream;

		if (be_substream == NULL)
			continue;

		/* update BE ref counts */
		be_inc_active(rtd->be_rtd[i][stream], stream);

		if (be_is_pending(rtd->be_rtd[i][stream], stream)) {
			be_rtd->current_fe = dai->id;
			ret = snd_soc_pcm_open(be_substream);
			if (ret < 0)
				goto unwind;
			dev_dbg(&rtd->dev, "%s: open %s:%d at %d act %d\n", __func__,
				rtd->be_rtd[i][stream]->dai_link->name,
				rtd->be_rtd[i][stream]->dai_link->be_id, i,
				abe_data.be_active[rtd->be_rtd[i][stream]->dai_link->be_id][stream]);
		}
	}

	/* start the ABE frontend */
	ret = abe_fe_startup(substream, dai);
	if (ret < 0) {
		dev_err(dai->dev,"%s: failed to start FE %d\n", __func__, ret);
		goto unwind;
	}

	dev_dbg(dai->dev,"%s: frontend finished %s %d\n", __func__,
			rtd->dai_link->name, dai->id);
	mutex_unlock(&fe_mutex);
	return 0;

unwind:
	/* disable any enabled and non active backends */
	for (--i; i >= 0; i--) {
		struct snd_soc_pcm_runtime *be_rtd = rtd->be_rtd[i][stream];
		struct snd_pcm_substream *be_substream =
			be_rtd->pcm->streams[stream].substream;

		if (be_substream == NULL)
			continue;

		if (be_is_pending(rtd->be_rtd[i][stream], stream)) {
			be_rtd->current_fe = dai->id;
			snd_soc_pcm_close(be_substream);

			dev_dbg(&rtd->dev, "%s: open-err-close %s:%d at %d act %d\n", __func__,
				rtd->be_rtd[i][stream]->dai_link->name,
				rtd->be_rtd[i][stream]->dai_link->be_id, i,
				abe_data.be_active[rtd->be_rtd[i][stream]->dai_link->be_id][stream]);
		}

		be_dec_active(rtd->be_rtd[i][stream], stream);
	}

	mutex_unlock(&fe_mutex);
	return ret;
}

static void omap_abe_dai_shutdown(struct snd_pcm_substream *substream,
				    struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	int stream = substream->stream, i;

	dev_dbg(dai->dev,"%s: frontend %s \n", __func__, rtd->dai_link->name);
	/* only shutdown backends that are either sinks or sources to this frontend DAI */

	mutex_lock(&fe_mutex);

	for (i = 0; i < rtd->num_be[stream]; i++) {
		struct snd_soc_pcm_runtime *be_rtd = rtd->be_rtd[i][stream];
		struct snd_pcm_substream *be_substream =
			be_rtd->pcm->streams[stream].substream;

		if (be_substream == NULL)
			continue;

		/* close backend stream if inactive */
		if (be_is_pending(rtd->be_rtd[i][stream], stream)) {
			be_rtd->current_fe = dai->id;
			snd_soc_pcm_close(be_substream);

			dev_dbg(&rtd->dev,"%s: close %s:%d at %d act %d\n", __func__,
				rtd->be_rtd[i][stream]->dai_link->name,
				rtd->be_rtd[i][stream]->dai_link->be_id, i,
				abe_data.be_active[rtd->be_rtd[i][stream]->dai_link->be_id][stream]);
		}

		be_dec_active(rtd->be_rtd[i][stream], stream);
	}

	/* now shutdown the frontend */
	abe_fe_shutdown(substream, dai);

	abe_be_dapm(rtd, dai->id, stream, SND_SOC_DAPM_STREAM_STOP);
	dev_dbg(dai->dev,"%s: frontend %s completed !\n", __func__, rtd->dai_link->name);
	mutex_unlock(&fe_mutex);
}

static int omap_abe_dai_hw_params(struct snd_pcm_substream *substream,
				    struct snd_pcm_hw_params *params,
				    struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	int stream = substream->stream, i, ret = 0;
	struct snd_pcm_hw_params *params_fe;

	params_fe = kzalloc(sizeof(struct snd_pcm_hw_params), GFP_KERNEL);
	if (!params_fe) {
		WARN_ON(1);
		return -ENOMEM;
	}

	dev_dbg(dai->dev, "%s: frontend %s\n", __func__, rtd->dai_link->name);

	memcpy(params_fe, params, sizeof(struct snd_pcm_hw_params));

	mutex_lock(&fe_mutex);

	for (i = 0; i < rtd->num_be[stream]; i++) {
		struct snd_soc_pcm_runtime *be_rtd = rtd->be_rtd[i][stream];
		struct snd_pcm_substream *be_substream =
			be_rtd->pcm->streams[stream].substream;

		if (be_substream == NULL)
			continue;

		if (be_is_running(rtd->be_rtd[i][stream], stream)) {
			be_rtd->current_fe = dai->id;
			if (be_rtd->dai_link->be_hw_params_fixup)
				ret = be_rtd->dai_link->be_hw_params_fixup(be_rtd, params);
			if (ret < 0) {
				dev_err(&rtd->dev, "%s: backend hw_params fixup failed %d\n",
										__func__, ret);
				goto out;
			}

			ret = snd_soc_pcm_hw_params(be_substream, params);
			if (ret < 0) {
				dev_err(&rtd->dev, "%s: backend hw_params failed %d\n",
						__func__, ret);
				goto out;
			}
		}
	}

	/* call hw_params on the frontend */
	memcpy(params, params_fe, sizeof(struct snd_pcm_hw_params));
	ret = abe_fe_hw_params(substream, params, dai);
	if (ret < 0)
		dev_err(dai->dev,"%s: frontend hw_params failed\n", __func__);

out:
	mutex_unlock(&fe_mutex);
	kfree(params_fe);
	return ret;
}

static int omap_abe_dai_trigger(struct snd_pcm_substream *substream,
				  int cmd, struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	int ret;

	dev_dbg(dai->dev,"%s: frontend %s \n", __func__, rtd->dai_link->name);

	/* call prepare on the frontend. TODO: do we handle MODEM as sequence */
	ret = abe_fe_trigger(substream, cmd, dai);
	if (ret < 0) {
		dev_err(dai->dev,"%s: frontend trigger failed\n", __func__);
		return ret;
	}

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		playback_trigger(substream, cmd);
	else
		capture_trigger(substream, cmd);

	return 0;
}

static int omap_abe_dai_prepare(struct snd_pcm_substream *substream,
				  struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	int stream = substream->stream, ret = 0, i;

	dev_dbg(dai->dev,"%s: frontend %s \n", __func__, rtd->dai_link->name);
	/* only prepare backends that are either sinks or sources to
	 * this frontend DAI */

	mutex_lock(&fe_mutex);

	for (i = 0; i < rtd->num_be[stream]; i++) {
		struct snd_soc_pcm_runtime *be_rtd = rtd->be_rtd[i][stream];
		struct snd_pcm_substream *be_substream =
			be_rtd->pcm->streams[stream].substream;

		if (be_substream == NULL)
			continue;

		/* prepare backend stream */
		if (be_is_running(rtd->be_rtd[i][stream], stream)) {
			be_rtd->current_fe = dai->id;
			snd_soc_pcm_prepare(be_substream);
		}
	}

	/* call prepare on the frontend */
	ret = abe_fe_prepare(substream, dai);
	if (ret < 0)
		dev_err(dai->dev,"%s: frontend prepare failed\n", __func__);

	abe_be_dapm(rtd, dai->id, stream, SND_SOC_DAPM_STREAM_START);

	mutex_unlock(&fe_mutex);
	return ret;
}

static int omap_abe_dai_hw_free(struct snd_pcm_substream *substream,
				  struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	int stream = substream->stream, ret = 0, i;

	dev_dbg(dai->dev,"%s: frontend %s \n", __func__, rtd->dai_link->name);

	mutex_lock(&fe_mutex);

	/* call hw_free on the frontend */
	ret = abe_fe_hw_free(substream, dai);
	if (ret < 0)
		dev_err(dai->dev,"%s: frontend hw_free failed\n", __func__);

	/* only hw_params backends that are either sinks or sources
	 * to this frontend DAI */
	for (i = 0; i < rtd->num_be[stream]; i++) {
		struct snd_soc_pcm_runtime *be_rtd = rtd->be_rtd[i][stream];
		struct snd_pcm_substream *be_substream =
			be_rtd->pcm->streams[stream].substream;

		if (be_substream == NULL)
			continue;

		if (be_is_running(rtd->be_rtd[i][stream], stream)) {
			be_rtd->current_fe = dai->id;
			snd_soc_pcm_hw_free(be_substream);
		}
	}

	mutex_unlock(&fe_mutex);
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
static void mute_be_capture(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_pcm_runtime *be_rtd;
	int i;

	for (i = 0; i < rtd->num_be[SNDRV_PCM_STREAM_CAPTURE]; i++) {
		be_rtd = rtd->be_rtd[i][SNDRV_PCM_STREAM_CAPTURE];

		dev_dbg(&rtd->dev, "%s: be ID=%d\n", __func__, be_rtd->dai_link->be_id);

		switch (be_rtd->dai_link->be_id) {
		case OMAP_ABE_DAI_PDM_UL:
			abe_mute_gain(GAINS_AMIC, GAIN_LEFT_OFFSET);
			abe_mute_gain(GAINS_AMIC, GAIN_RIGHT_OFFSET);
			break;
		case OMAP_ABE_DAI_BT_VX:
		case OMAP_ABE_DAI_MM_FM:
		case OMAP_ABE_DAI_MODEM:
			break;
		case OMAP_ABE_DAI_DMIC0:
			abe_mute_gain(GAINS_DMIC1, GAIN_LEFT_OFFSET);
			abe_mute_gain(GAINS_DMIC1, GAIN_RIGHT_OFFSET);
			break;
		case OMAP_ABE_DAI_DMIC1:
			abe_mute_gain(GAINS_DMIC2, GAIN_LEFT_OFFSET);
			abe_mute_gain(GAINS_DMIC2, GAIN_RIGHT_OFFSET);
			break;
		case OMAP_ABE_DAI_DMIC2:
			abe_mute_gain(GAINS_DMIC3, GAIN_LEFT_OFFSET);
			abe_mute_gain(GAINS_DMIC3, GAIN_RIGHT_OFFSET);
			break;
		}
	}
}

// TODO: finish this and restore correct volume levels
static void unmute_be_capture(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_pcm_runtime *be_rtd;
	int i;

	for (i = 0; i < rtd->num_be[SNDRV_PCM_STREAM_CAPTURE]; i++) {
		be_rtd = rtd->be_rtd[i][SNDRV_PCM_STREAM_CAPTURE];

		dev_dbg(&rtd->dev, "%s: be ID=%d\n", __func__, be_rtd->dai_link->be_id);

		switch (be_rtd->dai_link->be_id) {
		case OMAP_ABE_DAI_PDM_UL:
			abe_unmute_gain(GAINS_AMIC, GAIN_LEFT_OFFSET);
			abe_unmute_gain(GAINS_AMIC, GAIN_RIGHT_OFFSET);
			break;
		case OMAP_ABE_DAI_BT_VX:
		case OMAP_ABE_DAI_MM_FM:
		case OMAP_ABE_DAI_MODEM:
			break;
		case OMAP_ABE_DAI_DMIC0:
			abe_unmute_gain(GAINS_DMIC1, GAIN_LEFT_OFFSET);
			abe_unmute_gain(GAINS_DMIC1, GAIN_RIGHT_OFFSET);
			break;
		case OMAP_ABE_DAI_DMIC1:
			abe_unmute_gain(GAINS_DMIC2, GAIN_LEFT_OFFSET);
			abe_unmute_gain(GAINS_DMIC2, GAIN_RIGHT_OFFSET);
			break;
		case OMAP_ABE_DAI_DMIC2:
			abe_unmute_gain(GAINS_DMIC3, GAIN_LEFT_OFFSET);
			abe_unmute_gain(GAINS_DMIC3, GAIN_RIGHT_OFFSET);
			break;
		}
	}
}

static void mute_be_playback(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_pcm_runtime *be_rtd;
	int i;

	for (i = 0; i < rtd->num_be[SNDRV_PCM_STREAM_PLAYBACK]; i++) {
		be_rtd = rtd->be_rtd[i][SNDRV_PCM_STREAM_PLAYBACK];

		dev_dbg(&rtd->dev, "%s: be ID=%d\n", __func__, be_rtd->dai_link->be_id);

		switch (be_rtd->dai_link->be_id) {
		case OMAP_ABE_DAI_PDM_DL1:
			abe_write_gain(GAINS_DL1, MUTE_GAIN, RAMP_5MS,
				GAIN_LEFT_OFFSET);
			abe_write_gain(GAINS_DL1, MUTE_GAIN, RAMP_5MS,
				GAIN_RIGHT_OFFSET);
			break;
		case OMAP_ABE_DAI_PDM_DL2:
			abe_write_gain(GAINS_DL2, MUTE_GAIN, RAMP_5MS,
				GAIN_LEFT_OFFSET);
			abe_write_gain(GAINS_DL2, MUTE_GAIN, RAMP_5MS,
				GAIN_RIGHT_OFFSET);
			break;
		case OMAP_ABE_DAI_PDM_VIB:
		case OMAP_ABE_DAI_BT_VX:
		case OMAP_ABE_DAI_MM_FM:
		case OMAP_ABE_DAI_MODEM:
			break;
		}
	}
}

static void unmute_be_playback(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_pcm_runtime *be_rtd;
	int i;

	for (i = 0; i < rtd->num_be[SNDRV_PCM_STREAM_PLAYBACK]; i++) {
		be_rtd = rtd->be_rtd[i][SNDRV_PCM_STREAM_PLAYBACK];

		dev_dbg(&rtd->dev, "%s: be ID=%d\n", __func__, be_rtd->dai_link->be_id);

		switch (be_rtd->dai_link->be_id) {
		case OMAP_ABE_DAI_PDM_DL1:
			abe_write_gain(GAINS_DL1, GAIN_0dB, RAMP_5MS,
				GAIN_LEFT_OFFSET);
			abe_write_gain(GAINS_DL1, GAIN_0dB, RAMP_5MS,
				GAIN_RIGHT_OFFSET);
			break;
		case OMAP_ABE_DAI_PDM_DL2:
			abe_write_gain(GAINS_DL2, GAIN_0dB, RAMP_5MS,
				GAIN_LEFT_OFFSET);
			abe_write_gain(GAINS_DL2, GAIN_0dB, RAMP_5MS,
				GAIN_RIGHT_OFFSET);
			break;
		case OMAP_ABE_DAI_PDM_VIB:
		case OMAP_ABE_DAI_BT_VX:
		case OMAP_ABE_DAI_MM_FM:
		case OMAP_ABE_DAI_MODEM:
			break;
		}
	}
}

static inline void abe_dai_enable_data_transfer(int port)
{
	pr_debug("%s : port %d\n", __func__, port);
	if (port != PDM_DL_PORT)
		abe_enable_data_transfer(port);
}

static void inc_be_ports(struct snd_pcm_substream *substream, int stream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_pcm_runtime *be_rtd;
	int i;

	for (i = 0; i < rtd->num_be[stream]; i++) {
		be_rtd = rtd->be_rtd[i][stream];

		if (be_is_running(be_rtd, stream))
			/* update TRIGGER ref counts */
			trigger_inc_active(be_rtd, stream);

	}
}

static void dec_be_ports(struct snd_pcm_substream *substream, int stream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_pcm_runtime *be_rtd;
	int i;

	for (i = 0; i < rtd->num_be[stream]; i++) {
		be_rtd = rtd->be_rtd[i][stream];

		if (be_is_running(be_rtd, stream))
			/* update TRIGGER ref counts */
			trigger_dec_active(be_rtd, stream);

	}
}

static void enable_be_ports(struct snd_pcm_substream *substream, int stream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_pcm_runtime *be_rtd;
	abe_data_format_t format;
	int i;

	for (i = 0; i < rtd->num_be[stream]; i++) {
		be_rtd = rtd->be_rtd[i][stream];

		dev_dbg(&rtd->dev, "%s: be ID=%d stream %d active %d\n",
				__func__, be_rtd->dai_link->be_id, stream,
				be_get_active(be_rtd, stream));

		switch (be_rtd->dai_link->be_id) {
		case OMAP_ABE_DAI_PDM_DL1:
			if (trigger_is_pending(be_rtd, stream) &&
			    pdm_ready(&abe_data))
				abe_dai_enable_data_transfer(PDM_DL_PORT);
			abe_data.pdm_dl_status[0] = DAI_STARTED;
			break;
		case OMAP_ABE_DAI_PDM_DL2:
			if (trigger_is_pending(be_rtd, stream) &&
			    pdm_ready(&abe_data))
				abe_dai_enable_data_transfer(PDM_DL_PORT);
			abe_data.pdm_dl_status[1] = DAI_STARTED;
			break;
		case OMAP_ABE_DAI_PDM_VIB:
			if (trigger_is_pending(be_rtd, stream) &&
			    pdm_ready(&abe_data))
				abe_dai_enable_data_transfer(PDM_DL_PORT);
			abe_data.pdm_dl_status[2] = DAI_STARTED;
			break;
		case OMAP_ABE_DAI_PDM_UL:
			if (trigger_is_pending(be_rtd, stream))
				abe_dai_enable_data_transfer(PDM_UL_PORT);
			abe_data.pdm_ul_status = DAI_STARTED;
			break;
		case OMAP_ABE_DAI_BT_VX:
			if (trigger_is_pending(be_rtd, stream)) {
				if (stream == SNDRV_PCM_STREAM_PLAYBACK) {
					/* BT_DL connection to McBSP 1 ports */
					format.f = 8000;
					format.samp_format = MONO_RSHIFTED_16;
					abe_connect_serial_port(BT_VX_DL_PORT,
								&format, MCBSP1_TX);
					abe_dai_enable_data_transfer(BT_VX_DL_PORT);
				} else {
					/* BT_UL connection to McBSP 1 ports */
					format.f = 8000;
					format.samp_format = MONO_RSHIFTED_16;
					abe_connect_serial_port(BT_VX_UL_PORT,
								&format, MCBSP1_RX);
					abe_dai_enable_data_transfer(BT_VX_UL_PORT);
				}
			}
			break;
		case OMAP_ABE_DAI_MM_FM:
			if (trigger_is_pending(be_rtd, stream)) {
				if (stream == SNDRV_PCM_STREAM_PLAYBACK) {
					/* MM_EXT connection to McBSP 2 ports */
					format.f = 48000;
					format.samp_format = STEREO_RSHIFTED_16;
					abe_connect_serial_port(MM_EXT_OUT_PORT,
								&format, MCBSP2_TX);
					abe_dai_enable_data_transfer(MM_EXT_OUT_PORT);
				} else {
					/* MM_EXT connection to McBSP 2 ports */
					format.f = 48000;
					format.samp_format = STEREO_RSHIFTED_16;
					abe_connect_serial_port(MM_EXT_IN_PORT,
								&format, MCBSP2_RX);
					abe_dai_enable_data_transfer(MM_EXT_IN_PORT);
				}
			}
			break;
		case OMAP_ABE_DAI_DMIC0:
			if (trigger_is_pending(be_rtd, stream) &&
			    dmic_ready(&abe_data))
				abe_dai_enable_data_transfer(DMIC_PORT);
			abe_data.dmic_status[0] = DAI_STARTED;
			break;
		case OMAP_ABE_DAI_DMIC1:
			if (trigger_is_pending(be_rtd, stream) &&
			    dmic_ready(&abe_data))
				abe_dai_enable_data_transfer(DMIC_PORT1);
			abe_data.dmic_status[1] = DAI_STARTED;
			break;
		case OMAP_ABE_DAI_DMIC2:
			if (trigger_is_pending(be_rtd, stream) &&
			    dmic_ready(&abe_data))
				abe_dai_enable_data_transfer(DMIC_PORT2);
			abe_data.dmic_status[2] = DAI_STARTED;
			break;
		}
	}
}

static void enable_fe_ports(struct snd_pcm_substream *substream, int stream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;

	dev_dbg(&rtd->dev, "%s: fe ID=%d stream %d\n",
			__func__, rtd->cpu_dai->id, stream);

	switch(rtd->cpu_dai->id) {
	case ABE_FRONTEND_DAI_MEDIA:
		if (stream == SNDRV_PCM_STREAM_PLAYBACK)
			abe_enable_data_transfer(MM_DL_PORT);
		else
			abe_enable_data_transfer(MM_UL_PORT);
		break;
	case ABE_FRONTEND_DAI_LP_MEDIA:
		abe_enable_data_transfer(MM_DL_PORT);
		break;
	case ABE_FRONTEND_DAI_MEDIA_CAPTURE:
		if (stream == SNDRV_PCM_STREAM_CAPTURE)
			abe_enable_data_transfer(MM_UL2_PORT);
		break;
	case ABE_FRONTEND_DAI_MODEM:
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

static inline void abe_dai_disable_data_transfer(int port)
{
	pr_debug("%s : port %d\n", __func__, port);
	if (port != PDM_DL_PORT)
		abe_disable_data_transfer(port);
}

static void disable_be_ports(struct snd_pcm_substream *substream, int stream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_pcm_runtime *be_rtd;
	int i;

	for (i = 0; i < rtd->num_be[stream]; i++) {

		be_rtd = rtd->be_rtd[i][stream];

		dev_dbg(&rtd->dev, "%s: be ID=%d stream %d active %d\n",
				__func__, be_rtd->dai_link->be_id, stream,
				be_get_active(be_rtd, stream));

		// TODO: main port active logic will be moved into HAL
		switch (be_rtd->dai_link->be_id) {
		case OMAP_ABE_DAI_PDM_DL1:
			abe_data.pdm_dl_status[0] = DAI_STOPPED;
			if (trigger_is_pending(be_rtd, stream) &&
			    pdm_ready(&abe_data))
				abe_dai_disable_data_transfer(PDM_DL_PORT);
			break;
		case OMAP_ABE_DAI_PDM_DL2:
			abe_data.pdm_dl_status[1] = DAI_STOPPED;
			if (trigger_is_pending(be_rtd, stream) &&
			    pdm_ready(&abe_data))
				abe_dai_disable_data_transfer(PDM_DL_PORT);
			break;
		case OMAP_ABE_DAI_PDM_VIB:
			abe_data.pdm_dl_status[2] = DAI_STOPPED;
			if (trigger_is_pending(be_rtd, stream) &&
			    pdm_ready(&abe_data))
				abe_dai_disable_data_transfer(PDM_VIB_PORT);
			break;
		case OMAP_ABE_DAI_PDM_UL:
			abe_data.pdm_ul_status = DAI_STOPPED;
			if (trigger_is_pending(be_rtd, stream))
				abe_dai_disable_data_transfer(PDM_UL_PORT);
			break;
		case OMAP_ABE_DAI_BT_VX:
			if (trigger_is_pending(be_rtd, stream)) {
				if (stream == SNDRV_PCM_STREAM_PLAYBACK)
					abe_dai_disable_data_transfer(BT_VX_DL_PORT);
				else
					abe_dai_disable_data_transfer(BT_VX_UL_PORT);
			}
			break;
		case OMAP_ABE_DAI_MM_FM:
		case OMAP_ABE_DAI_MODEM:
			if (trigger_is_pending(be_rtd, stream)) {
				if (stream == SNDRV_PCM_STREAM_PLAYBACK)
					abe_dai_disable_data_transfer(MM_EXT_OUT_PORT);
				else
					abe_dai_disable_data_transfer(MM_EXT_IN_PORT);
			}
		case OMAP_ABE_DAI_DMIC0:
			abe_data.dmic_status[0] = DAI_STOPPED;
			if (trigger_is_pending(be_rtd, stream) &&
			    dmic_ready(&abe_data))
				abe_dai_disable_data_transfer(DMIC_PORT);
			break;
		case OMAP_ABE_DAI_DMIC1:
			abe_data.dmic_status[1] = DAI_STOPPED;
			if (trigger_is_pending(be_rtd, stream) &&
			    dmic_ready(&abe_data))
				abe_dai_disable_data_transfer(DMIC_PORT1);
			break;
		case OMAP_ABE_DAI_DMIC2:
			abe_data.dmic_status[2] = DAI_STOPPED;
			if (trigger_is_pending(be_rtd, stream) &&
			    dmic_ready(&abe_data))
				abe_dai_disable_data_transfer(DMIC_PORT2);
			break;
		}

	}
}

static void disable_fe_ports(struct snd_pcm_substream *substream, int stream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;

	dev_dbg(&rtd->dev, "%s: fe ID=%d stream %d\n",
			__func__, rtd->cpu_dai->id, stream);

	switch(rtd->cpu_dai->id) {
	case ABE_FRONTEND_DAI_MEDIA:
		if (stream == SNDRV_PCM_STREAM_PLAYBACK)
			abe_disable_data_transfer(MM_DL_PORT);
		else
			abe_disable_data_transfer(MM_UL_PORT);
		break;
	case ABE_FRONTEND_DAI_LP_MEDIA:
		abe_disable_data_transfer(MM_DL_PORT);
		break;
	case ABE_FRONTEND_DAI_MEDIA_CAPTURE:
		if (stream == SNDRV_PCM_STREAM_CAPTURE)
			abe_disable_data_transfer(MM_UL2_PORT);
		break;
	case ABE_FRONTEND_DAI_MODEM:
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

/* TODO: finish this */
static void mute_fe_port(struct snd_pcm_substream *substream, int stream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;

	dev_dbg(&rtd->dev, "%s: fe ID=%d stream %d\n",
			__func__, rtd->cpu_dai->id, stream);

	switch(rtd->cpu_dai->id) {
	case ABE_FRONTEND_DAI_MEDIA:
	case ABE_FRONTEND_DAI_LP_MEDIA:
		if (abe_data.be_active[OMAP_ABE_DAI_PDM_DL2][SNDRV_PCM_STREAM_PLAYBACK]) {
			abe_mute_gain(MIXDL2, MIX_DL2_INPUT_MM_DL);
		}
		if (abe_data.be_active[OMAP_ABE_DAI_PDM_DL1][SNDRV_PCM_STREAM_PLAYBACK]) {
			abe_mute_gain(MIXDL1, MIX_DL1_INPUT_MM_DL);
		}
		break;
	case ABE_FRONTEND_DAI_MEDIA_CAPTURE:
		break;
	case ABE_FRONTEND_DAI_VOICE:
		if (abe_data.be_active[OMAP_ABE_DAI_PDM_DL2][SNDRV_PCM_STREAM_PLAYBACK]) {
			abe_mute_gain(MIXDL2, MIX_DL2_INPUT_VX_DL);
		}
		if (abe_data.be_active[OMAP_ABE_DAI_PDM_DL1][SNDRV_PCM_STREAM_PLAYBACK]) {
			abe_mute_gain(MIXDL1, MIX_DL1_INPUT_VX_DL);
		}
		break;
	case ABE_FRONTEND_DAI_TONES:
		if (abe_data.be_active[OMAP_ABE_DAI_PDM_DL2][SNDRV_PCM_STREAM_PLAYBACK]) {
			abe_mute_gain(MIXDL2, MIX_DL2_INPUT_TONES);
		}
		if (abe_data.be_active[OMAP_ABE_DAI_PDM_DL1][SNDRV_PCM_STREAM_PLAYBACK]) {
			abe_mute_gain(MIXDL1, MIX_DL1_INPUT_TONES);
		}
		break;
	case ABE_FRONTEND_DAI_VIBRA:

		break;
	}
}

/* TODO: finish this */
static void unmute_fe_port(struct snd_pcm_substream *substream, int stream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;

	dev_dbg(&rtd->dev, "%s: fe ID=%d stream %d\n",
			__func__, rtd->cpu_dai->id, stream);

	switch(rtd->cpu_dai->id) {
	case ABE_FRONTEND_DAI_MEDIA:
	case ABE_FRONTEND_DAI_LP_MEDIA:
		if (abe_data.be_active[OMAP_ABE_DAI_PDM_DL2][SNDRV_PCM_STREAM_PLAYBACK]) {
			abe_unmute_gain(MIXDL2, MIX_DL2_INPUT_MM_DL);
		}
		if (abe_data.be_active[OMAP_ABE_DAI_PDM_DL1][SNDRV_PCM_STREAM_PLAYBACK]) {
			abe_unmute_gain(MIXDL1, MIX_DL1_INPUT_MM_DL);
		}
		break;
	case ABE_FRONTEND_DAI_MEDIA_CAPTURE:
		break;
	case ABE_FRONTEND_DAI_VOICE:
		if (abe_data.be_active[OMAP_ABE_DAI_PDM_DL2][SNDRV_PCM_STREAM_PLAYBACK]) {
			abe_unmute_gain(MIXDL2, MIX_DL2_INPUT_VX_DL);
		}
		if (abe_data.be_active[OMAP_ABE_DAI_PDM_DL1][SNDRV_PCM_STREAM_PLAYBACK]) {
			abe_unmute_gain(MIXDL1, MIX_DL1_INPUT_VX_DL);
		}
		break;
	case ABE_FRONTEND_DAI_TONES:
		if (abe_data.be_active[OMAP_ABE_DAI_PDM_DL2][SNDRV_PCM_STREAM_PLAYBACK]) {
			abe_unmute_gain(MIXDL2, MIX_DL2_INPUT_TONES);
		}
		if (abe_data.be_active[OMAP_ABE_DAI_PDM_DL1][SNDRV_PCM_STREAM_PLAYBACK]) {
			abe_unmute_gain(MIXDL1, MIX_DL1_INPUT_TONES);
		}
		break;
	case ABE_FRONTEND_DAI_VIBRA:

		break;
	}
}

static void trigger_be_ports(struct snd_pcm_substream *substream, int cmd)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_pcm_runtime *be_rtd;
	int i, stream = substream->stream;

	for (i = 0; i < rtd->num_be[stream]; i++) {
		be_rtd = rtd->be_rtd[i][stream];

		dev_dbg(&rtd->dev, "%s: be ID=%d cmd %d act %d\n",
				__func__, be_rtd->dai_link->be_id, cmd,
				abe_data.be_active[be_rtd->dai_link->be_id][stream]);

		switch (cmd) {
		case SNDRV_PCM_TRIGGER_START:
		case SNDRV_PCM_TRIGGER_STOP:
			if (be_is_running(be_rtd, stream) &&
			    trigger_is_pending(be_rtd, stream))
					snd_soc_dai_trigger(substream, cmd,
							be_rtd->cpu_dai);
			break;
		default:
			if (be_is_running(be_rtd, stream))
				snd_soc_dai_trigger(substream, cmd,
							be_rtd->cpu_dai);
			break;
		}
	}
}

static void capture_trigger(struct snd_pcm_substream *substream, int cmd)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;

	dev_dbg(&rtd->dev, "%s-start: fe ID=%d stream %d\n",
			__func__, rtd->cpu_dai->id, substream->stream);

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
		/*   ALL BE GAINS set to mute */
		mute_be_capture(substream);

		/* Enable ALL ABE BE ports */
		inc_be_ports(substream, SNDRV_PCM_STREAM_CAPTURE);
		enable_be_ports(substream, SNDRV_PCM_STREAM_CAPTURE);

		/* DAI work must be started/stopped at least 250us after ABE */
		udelay(250);

		/*  trigger ALL BE ports */
		trigger_be_ports(substream, cmd);

		/* Enable Frontend sDMA  */
		enable_fe_ports(substream, SNDRV_PCM_STREAM_CAPTURE);

		/* Restore ABE GAINS AMIC */
		unmute_be_capture(substream);
		break;
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		/* Enable sDMA / Enable ABE MM_UL2 */
		enable_fe_ports(substream, SNDRV_PCM_STREAM_CAPTURE);
		break;
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		/* Disable sDMA / Enable ABE MM_UL2 */
		disable_fe_ports(substream, SNDRV_PCM_STREAM_CAPTURE);
		break;
	case SNDRV_PCM_TRIGGER_STOP:
		/* Disable sDMA */
		disable_fe_ports(substream, SNDRV_PCM_STREAM_CAPTURE);

		disable_be_ports(substream, SNDRV_PCM_STREAM_CAPTURE);

		/* DAI work must be started/stopped at least 250us after ABE */
		udelay(250);

		/*  trigger ALL BE ports */
		trigger_be_ports(substream, cmd);
		dec_be_ports(substream, SNDRV_PCM_STREAM_CAPTURE);
		break;
	default:
		break;
	}

	dev_dbg(&rtd->dev, "%s - finish: fe ID=%d stream %d\n",
			__func__, rtd->cpu_dai->id, substream->stream);
}

static void playback_trigger(struct snd_pcm_substream *substream, int cmd)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;

	dev_dbg(&rtd->dev, "%s-start: fe ID=%d stream %d\n",
			__func__, rtd->cpu_dai->id, substream->stream);

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
		/* mute splayback */
		mute_be_playback(substream);

		/* enabled BE ports */
		inc_be_ports(substream, SNDRV_PCM_STREAM_PLAYBACK);
		enable_be_ports(substream, SNDRV_PCM_STREAM_PLAYBACK);

		/* DAI work must be started/stopped at least 250us after ABE */
		udelay(250);

		/*  trigger ALL BE ports */
		trigger_be_ports(substream, cmd);

		/* TODO: unmute DL1 */
		unmute_be_playback(substream);

		/* Enable Frontend sDMA  */
		enable_fe_ports(substream, SNDRV_PCM_STREAM_PLAYBACK);

		/* unmute ABE_MM_DL */
		unmute_fe_port(substream, SNDRV_PCM_STREAM_PLAYBACK);
		break;
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		/* Enable Frontend sDMA  */
		enable_fe_ports(substream, SNDRV_PCM_STREAM_PLAYBACK);

		/* unmute ABE_MM_DL */
		unmute_fe_port(substream, SNDRV_PCM_STREAM_PLAYBACK);
		break;
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		/* Enable Frontend sDMA  */
		disable_fe_ports(substream, SNDRV_PCM_STREAM_PLAYBACK);

		/* mute ABE_MM_DL */
		mute_fe_port(substream, SNDRV_PCM_STREAM_PLAYBACK);
		break;
	case SNDRV_PCM_TRIGGER_STOP:
		/* disable the transfer */
		disable_fe_ports(substream, SNDRV_PCM_STREAM_PLAYBACK);

		/* mute ABE_MM_DL */
		mute_fe_port(substream, SNDRV_PCM_STREAM_PLAYBACK);

		/* TODO :mute Phoenix */

		/* disable our backends */
		disable_be_ports(substream, SNDRV_PCM_STREAM_PLAYBACK);

		/* DAI work must be started/stopped at least 250us after ABE */
		udelay(250);

		/*  trigger ALL BE ports */
		trigger_be_ports(substream, cmd);
		dec_be_ports(substream, SNDRV_PCM_STREAM_PLAYBACK);
		break;
	default:
		break;
	}

	dev_dbg(&rtd->dev, "%s - finish: fe ID=%d stream %d\n",
			__func__, rtd->cpu_dai->id, substream->stream);
}

static int omap_abe_dai_probe(struct snd_soc_dai *dai)
{
	snd_soc_dai_set_drvdata(dai, &abe_data);

	abe_data.workqueue = create_singlethread_workqueue("omap-abe");
	if (abe_data.workqueue == NULL)
		return -ENOMEM;
	return 0;
}

static int omap_abe_dai_remove(struct snd_soc_dai *dai)
{
	destroy_workqueue(abe_data.workqueue);
	return 0;
}

static struct snd_soc_dai_driver omap_abe_dai[] = {
	{	/* Multimedia Playback and Capture */
		.name = "MultiMedia1",
		.probe = omap_abe_dai_probe,
		.remove = omap_abe_dai_remove,
		.playback = {
			.stream_name = "MultiMedia1 Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_48000,
			.formats = OMAP_ABE_FORMATS,
		},
		.capture = {
			.stream_name = "MultiMedia1 Capture",
			.channels_min = 2,
			.channels_max = 8,
			.rates = SNDRV_PCM_RATE_48000,
			.formats = OMAP_ABE_FORMATS,
		},
		.ops = &omap_abe_dai_ops,
	},
	{	/* Multimedia Capture */
		.name = "MultiMedia2",
		.capture = {
			.stream_name = "MultiMedia2 Capture",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_48000,
			.formats = OMAP_ABE_FORMATS,
		},
		.ops = &omap_abe_dai_ops,
	},
	{	/* Voice Playback and Capture */
		.name = "Voice",
		.playback = {
			.stream_name = "Voice Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000,
			.formats = OMAP_ABE_FORMATS,
		},
		.capture = {
			.stream_name = "Voice Capture",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000,
			.formats = OMAP_ABE_FORMATS,
		},
		.ops = &omap_abe_dai_ops,
	},
	{	/* Tones Playback */
		.name = "Tones",
		.playback = {
			.stream_name = "Tones Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_48000,
			.formats = OMAP_ABE_FORMATS,
		},
		.ops = &omap_abe_dai_ops,
	},
	{	/* Vibra */
		.name = "Vibra",
		.playback = {
			.stream_name = "Vibra Playback",
			.channels_min = 2,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_CONTINUOUS,
			.formats = OMAP_ABE_FORMATS,
		},
		.ops = &omap_abe_dai_ops,
	},
	{	/* MODEM Voice Playback and Capture */
		.name = "MODEM",
		.playback = {
			.stream_name = "Voice Playback",
			.channels_min = 1,
			.channels_max = 1,
			.rates = SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000,
			.formats = OMAP_ABE_FORMATS | SNDRV_PCM_FMTBIT_S16_LE,
		},
		.capture = {
			.stream_name = "Voice Capture",
			.channels_min = 1,
			.channels_max = 1,
			.rates = SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000,
			.formats = OMAP_ABE_FORMATS | SNDRV_PCM_FMTBIT_S16_LE,
		},
		.ops = &omap_abe_dai_ops,
	},
	{	/* Low Power HiFi Playback */
		.name = "MultiMedia1 LP",
		.playback = {
			.stream_name = "MultiMedia1 LP Playback",
			.channels_min = 2,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_44100 | SNDRV_PCM_RATE_48000,
			.formats = OMAP_ABE_FORMATS | SNDRV_PCM_FMTBIT_S16_LE,
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
