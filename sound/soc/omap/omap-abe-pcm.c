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

#include <linux/delay.h>

#include <linux/omap-dma.h>
#define OMAP44XX_DMA_ABE_REQ_0			101
#define OMAP44XX_DMA_ABE_REQ_1			102
#define OMAP44XX_DMA_ABE_REQ_2			103
#define OMAP44XX_DMA_ABE_REQ_3			104
#define OMAP44XX_DMA_ABE_REQ_4			105
#define OMAP44XX_DMA_ABE_REQ_5			106
#define OMAP44XX_DMA_ABE_REQ_6			107
#define OMAP44XX_DMA_ABE_REQ_7			108

#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/soc-dpcm.h>

#include "omap-abe-priv.h"
#include "omap-pcm.h"

/*
 * Stream DMA parameters
 */
static struct omap_pcm_dma_data omap_abe_dai_dma_params[7][2] = {
	{
		{
			.name = "Media Playback",
			.dma_req = OMAP44XX_DMA_ABE_REQ_0,
			.data_type = 32,
		},
		{
			.name = "Media Capture1",
			.dma_req = OMAP44XX_DMA_ABE_REQ_3,
			.data_type = 32,
		},
	},
	{
		{},
		{
			.name = "Media Capture2",
			.dma_req = OMAP44XX_DMA_ABE_REQ_4,
			.data_type = 32,
		},
	},
	{
		{
			.name = "Voice Playback",
			.dma_req = OMAP44XX_DMA_ABE_REQ_1,
			.data_type = 32,
		},
		{
			.name = "Voice Capture",
			.dma_req = OMAP44XX_DMA_ABE_REQ_2,
			.data_type = 32,
		},
	},
	{
		{
			.name = "Tones Playback",
			.dma_req = OMAP44XX_DMA_ABE_REQ_5,
			.data_type = 32,
		},
		{},
	},
	{
		{
			.name = "Vibra Playback",
			.dma_req = OMAP44XX_DMA_ABE_REQ_6,
			.data_type = 32,
		},
		{},
	},
	{
		{
			.name = "MODEM Playback",
			.dma_req = OMAP44XX_DMA_ABE_REQ_1,
			.data_type = 32,
		},
		{
			.name = "MODEM Capture",
			.dma_req = OMAP44XX_DMA_ABE_REQ_2,
			.data_type = 32,
		},
	},
	{
		{
			.name = "Low Power Playback",
			.dma_req = OMAP44XX_DMA_ABE_REQ_0,
			.data_type = 32,
		},
		{},
	},
};

static int omap_abe_dl1_enabled(struct omap_abe *abe)
{
	/* DL1 path is common for PDM_DL1, BT_VX_DL and MM_EXT_DL */
	return omap_abe_port_is_enabled(abe->aess,
				abe->dai.port[OMAP_ABE_BE_PORT_PDM_DL1]) +
		omap_abe_port_is_enabled(abe->aess,
				abe->dai.port[OMAP_ABE_BE_PORT_BT_VX_DL]) +
		omap_abe_port_is_enabled(abe->aess,
				abe->dai.port[OMAP_ABE_BE_PORT_MM_EXT_DL]);
}

static void mute_be(struct snd_soc_pcm_runtime *be,
		struct snd_soc_dai *dai, int stream)
{
	struct omap_abe *abe = snd_soc_dai_get_drvdata(dai);

	dev_dbg(be->dev, "%s: %s %d\n", __func__, be->cpu_dai->name, stream);

	if (stream == SNDRV_PCM_STREAM_PLAYBACK) {
		switch (be->dai_link->be_id) {
		case OMAP_ABE_DAI_PDM_DL1:
		case OMAP_ABE_DAI_BT_VX:
		case OMAP_ABE_DAI_MM_FM:
			/*
			 * DL1 Mixer->SDT Mixer and DL1 gain are common for
			 * PDM_DL1, BT_VX_DL and MM_EXT_DL, mute those gains
			 * only if the last active BE
			 */
			if (omap_abe_dl1_enabled(abe) == 1) {
				omap_aess_mute_gain(abe->aess, OMAP_AESS_GAIN_DL1_LEFT);
				omap_aess_mute_gain(abe->aess, OMAP_AESS_GAIN_DL1_RIGHT);
				omap_aess_mute_gain(abe->aess, OMAP_AESS_MIXSDT_DL);
			}
			break;
		case OMAP_ABE_DAI_PDM_DL2:
			omap_aess_mute_gain(abe->aess, OMAP_AESS_GAIN_DL2_LEFT);
			omap_aess_mute_gain(abe->aess, OMAP_AESS_GAIN_DL2_RIGHT);
			break;
		case OMAP_ABE_DAI_MODEM:
			break;
		}
	} else {
		switch (be->dai_link->be_id) {
		case OMAP_ABE_DAI_PDM_UL:
			omap_aess_mute_gain(abe->aess, OMAP_AESS_GAIN_AMIC_LEFT);
			omap_aess_mute_gain(abe->aess, OMAP_AESS_GAIN_AMIC_RIGHT);
			break;
		case OMAP_ABE_DAI_BT_VX:
			omap_aess_mute_gain(abe->aess, OMAP_AESS_GAIN_BTUL_LEFT);
			omap_aess_mute_gain(abe->aess, OMAP_AESS_GAIN_BTUL_RIGHT);
			break;
		case OMAP_ABE_DAI_MM_FM:
		case OMAP_ABE_DAI_MODEM:
			break;
		case OMAP_ABE_DAI_DMIC0:
			omap_aess_mute_gain(abe->aess, OMAP_AESS_GAIN_DMIC1_LEFT);
			omap_aess_mute_gain(abe->aess, OMAP_AESS_GAIN_DMIC1_RIGHT);
			break;
		case OMAP_ABE_DAI_DMIC1:
			omap_aess_mute_gain(abe->aess, OMAP_AESS_GAIN_DMIC2_LEFT);
			omap_aess_mute_gain(abe->aess, OMAP_AESS_GAIN_DMIC2_RIGHT);
			break;
		case OMAP_ABE_DAI_DMIC2:
			omap_aess_mute_gain(abe->aess, OMAP_AESS_GAIN_DMIC3_LEFT);
			omap_aess_mute_gain(abe->aess, OMAP_AESS_GAIN_DMIC3_RIGHT);
			break;
		}
	}
}

static void unmute_be(struct snd_soc_pcm_runtime *be,
		struct snd_soc_dai *dai, int stream)
{
	struct omap_abe *abe = snd_soc_dai_get_drvdata(dai);

	dev_dbg(be->dev, "%s: %s %d\n", __func__, be->cpu_dai->name, stream);

	if (stream == SNDRV_PCM_STREAM_PLAYBACK) {
		switch (be->dai_link->be_id) {
		case OMAP_ABE_DAI_PDM_DL1:
		case OMAP_ABE_DAI_BT_VX:
		case OMAP_ABE_DAI_MM_FM:
			/*
			 * DL1 Mixer->SDT Mixer and DL1 gain are common for
			 * PDM_DL1, BT_VX_DL and MM_EXT_DL, unmute when any
			 * of them becomes active
			 */
			omap_aess_unmute_gain(abe->aess, OMAP_AESS_GAIN_DL1_LEFT);
			omap_aess_unmute_gain(abe->aess, OMAP_AESS_GAIN_DL1_RIGHT);
			omap_aess_unmute_gain(abe->aess, OMAP_AESS_MIXSDT_DL);
			break;
		case OMAP_ABE_DAI_PDM_DL2:
			omap_aess_unmute_gain(abe->aess, OMAP_AESS_GAIN_DL2_LEFT);
			omap_aess_unmute_gain(abe->aess, OMAP_AESS_GAIN_DL2_RIGHT);
			break;
		case OMAP_ABE_DAI_MODEM:
			break;
		}
	} else {

		switch (be->dai_link->be_id) {
		case OMAP_ABE_DAI_PDM_UL:
			omap_aess_unmute_gain(abe->aess, OMAP_AESS_GAIN_AMIC_LEFT);
			omap_aess_unmute_gain(abe->aess, OMAP_AESS_GAIN_AMIC_RIGHT);
			break;
		case OMAP_ABE_DAI_BT_VX:
			omap_aess_unmute_gain(abe->aess, OMAP_AESS_GAIN_BTUL_LEFT);
			omap_aess_unmute_gain(abe->aess, OMAP_AESS_GAIN_BTUL_RIGHT);
			break;
		case OMAP_ABE_DAI_MM_FM:
		case OMAP_ABE_DAI_MODEM:
			break;
		case OMAP_ABE_DAI_DMIC0:
			omap_aess_unmute_gain(abe->aess, OMAP_AESS_GAIN_DMIC1_LEFT);
			omap_aess_unmute_gain(abe->aess, OMAP_AESS_GAIN_DMIC1_RIGHT);
			break;
		case OMAP_ABE_DAI_DMIC1:
			omap_aess_unmute_gain(abe->aess, OMAP_AESS_GAIN_DMIC2_LEFT);
			omap_aess_unmute_gain(abe->aess, OMAP_AESS_GAIN_DMIC2_RIGHT);
			break;
		case OMAP_ABE_DAI_DMIC2:
			omap_aess_unmute_gain(abe->aess, OMAP_AESS_GAIN_DMIC3_LEFT);
			omap_aess_unmute_gain(abe->aess, OMAP_AESS_GAIN_DMIC3_RIGHT);
			break;
		}
	}
}

static void enable_be_port(struct snd_soc_pcm_runtime *be,
		struct snd_soc_dai *dai, int stream)
{
	struct omap_abe *abe = snd_soc_dai_get_drvdata(dai);
	struct omap_aess_data_format format;

	dev_dbg(be->dev, "%s: %s %d\n", __func__, be->cpu_dai->name, stream);

	switch (be->dai_link->be_id) {
	case OMAP_ABE_DAI_PDM_DL1:
	case OMAP_ABE_DAI_PDM_DL2:
	case OMAP_ABE_DAI_PDM_UL:
		/* McPDM is a special case, handled by McPDM driver */
		break;
	case OMAP_ABE_DAI_BT_VX:
		if (stream == SNDRV_PCM_STREAM_PLAYBACK) {

			/* port can only be configured if it's not running */
			if (omap_abe_port_is_enabled(abe->aess,
					abe->dai.port[OMAP_ABE_BE_PORT_BT_VX_DL]))
				return;

			/* BT_DL connection to McBSP 1 ports */
			format.f = 8000;
			format.samp_format = OMAP_AESS_FORMAT_STEREO_RSHIFTED_16;
			omap_aess_connect_serial_port(abe->aess, OMAP_ABE_BT_VX_DL_PORT, &format, MCBSP1_TX, NULL);
			omap_abe_port_enable(abe->aess,
				abe->dai.port[OMAP_ABE_BE_PORT_BT_VX_DL]);
		} else {

			/* port can only be configured if it's not running */
			if (omap_abe_port_is_enabled(abe->aess,
					abe->dai.port[OMAP_ABE_BE_PORT_BT_VX_UL]))
				return;

			/* BT_UL connection to McBSP 1 ports */
			format.f = 8000;
			format.samp_format = OMAP_AESS_FORMAT_STEREO_RSHIFTED_16;
			omap_aess_connect_serial_port(abe->aess, OMAP_ABE_BT_VX_UL_PORT, &format, MCBSP1_RX, NULL);
			omap_abe_port_enable(abe->aess,
				abe->dai.port[OMAP_ABE_BE_PORT_BT_VX_UL]);
		}
		break;
	case OMAP_ABE_DAI_MM_FM:
		if (stream == SNDRV_PCM_STREAM_PLAYBACK) {

			/* port can only be configured if it's not running */
			if (omap_abe_port_is_enabled(abe->aess,
					abe->dai.port[OMAP_ABE_BE_PORT_MM_EXT_DL]))
				return;

			/* MM_EXT connection to McBSP 2 ports */
			format.f = 48000;
			format.samp_format = OMAP_AESS_FORMAT_STEREO_RSHIFTED_16;
			omap_aess_connect_serial_port(abe->aess, OMAP_ABE_MM_EXT_OUT_PORT, &format, MCBSP2_TX, NULL);
			omap_abe_port_enable(abe->aess,
				abe->dai.port[OMAP_ABE_BE_PORT_MM_EXT_DL]);
		} else {

			/* port can only be configured if it's not running */
			if (omap_abe_port_is_enabled(abe->aess,
					abe->dai.port[OMAP_ABE_BE_PORT_MM_EXT_UL]))
				return;

			/* MM_EXT connection to McBSP 2 ports */
			format.f = 48000;
			format.samp_format = OMAP_AESS_FORMAT_STEREO_RSHIFTED_16;
			omap_aess_connect_serial_port(abe->aess, OMAP_ABE_MM_EXT_IN_PORT, &format, MCBSP2_RX, NULL);
			omap_abe_port_enable(abe->aess,
				abe->dai.port[OMAP_ABE_BE_PORT_MM_EXT_UL]);
		}
		break;
	case OMAP_ABE_DAI_DMIC0:
		omap_abe_port_enable(abe->aess,
				abe->dai.port[OMAP_ABE_BE_PORT_DMIC0]);
		break;
	case OMAP_ABE_DAI_DMIC1:
		omap_abe_port_enable(abe->aess,
				abe->dai.port[OMAP_ABE_BE_PORT_DMIC1]);
		break;
	case OMAP_ABE_DAI_DMIC2:
		omap_abe_port_enable(abe->aess,
				abe->dai.port[OMAP_ABE_BE_PORT_DMIC2]);
		break;
	}
}

static void enable_fe_port(struct snd_pcm_substream *substream,
		struct snd_soc_dai *dai, int stream)
{
	struct snd_soc_pcm_runtime *fe = substream->private_data;
	struct omap_abe *abe = snd_soc_dai_get_drvdata(dai);

	dev_dbg(fe->dev, "%s: %s %d\n", __func__, dai->name, stream);

	switch (dai->id) {
	case OMAP_ABE_FRONTEND_DAI_MEDIA:
		if (stream == SNDRV_PCM_STREAM_PLAYBACK)
			omap_abe_port_enable(abe->aess,
					abe->dai.port[OMAP_ABE_FE_PORT_MM_DL1]);
		else
			omap_abe_port_enable(abe->aess,
					abe->dai.port[OMAP_ABE_FE_PORT_MM_UL1]);
		break;
	case OMAP_ABE_FRONTEND_DAI_LP_MEDIA:
		if (stream == SNDRV_PCM_STREAM_PLAYBACK) {
			omap_abe_port_enable(abe->aess,
					abe->dai.port[OMAP_ABE_FE_PORT_MM_DL_LP]);
			abe->dai.port[OMAP_ABE_FE_PORT_MM_DL_LP]->substream = substream;
		}
		break;
	case OMAP_ABE_FRONTEND_DAI_MEDIA_CAPTURE:
		if (stream == SNDRV_PCM_STREAM_CAPTURE)
			omap_abe_port_enable(abe->aess,
					abe->dai.port[OMAP_ABE_FE_PORT_MM_UL2]);
		break;
	case OMAP_ABE_FRONTEND_DAI_MODEM:
	case OMAP_ABE_FRONTEND_DAI_VOICE:
		if (stream == SNDRV_PCM_STREAM_PLAYBACK)
			omap_abe_port_enable(abe->aess,
					abe->dai.port[OMAP_ABE_FE_PORT_VX_DL]);
		else
			omap_abe_port_enable(abe->aess,
					abe->dai.port[OMAP_ABE_FE_PORT_VX_UL]);
		break;
	case OMAP_ABE_FRONTEND_DAI_TONES:
		if (stream == SNDRV_PCM_STREAM_PLAYBACK)
			omap_abe_port_enable(abe->aess,
					abe->dai.port[OMAP_ABE_FE_PORT_TONES]);
		break;
	}
}

static void disable_be_port(struct snd_soc_pcm_runtime *be,
		struct snd_soc_dai *dai, int stream)
{
	struct omap_abe *abe = snd_soc_dai_get_drvdata(dai);

	dev_dbg(be->dev, "%s: %s %d\n", __func__, be->cpu_dai->name, stream);

	switch (be->dai_link->be_id) {
	/* McPDM is a special case, handled by McPDM driver */
	case OMAP_ABE_DAI_PDM_DL1:
	case OMAP_ABE_DAI_PDM_DL2:
	case OMAP_ABE_DAI_PDM_UL:
		break;
	case OMAP_ABE_DAI_BT_VX:
		if (stream == SNDRV_PCM_STREAM_PLAYBACK)
			omap_abe_port_disable(abe->aess,
					abe->dai.port[OMAP_ABE_BE_PORT_BT_VX_DL]);
		else
			omap_abe_port_disable(abe->aess,
					abe->dai.port[OMAP_ABE_BE_PORT_BT_VX_UL]);
		break;
	case OMAP_ABE_DAI_MM_FM:
	case OMAP_ABE_DAI_MODEM:
		if (stream == SNDRV_PCM_STREAM_PLAYBACK)
			omap_abe_port_disable(abe->aess,
					abe->dai.port[OMAP_ABE_BE_PORT_MM_EXT_DL]);
		else
			omap_abe_port_disable(abe->aess,
					abe->dai.port[OMAP_ABE_BE_PORT_MM_EXT_UL]);
		break;
	case OMAP_ABE_DAI_DMIC0:
		omap_abe_port_disable(abe->aess,
				abe->dai.port[OMAP_ABE_BE_PORT_DMIC0]);
		break;
	case OMAP_ABE_DAI_DMIC1:
		omap_abe_port_disable(abe->aess,
				abe->dai.port[OMAP_ABE_BE_PORT_DMIC1]);
		break;
	case OMAP_ABE_DAI_DMIC2:
		omap_abe_port_disable(abe->aess,
				abe->dai.port[OMAP_ABE_BE_PORT_DMIC2]);
		break;
	}
}

static void disable_fe_port(struct snd_pcm_substream *substream,
		struct snd_soc_dai *dai, int stream)
{
	struct snd_soc_pcm_runtime *fe = substream->private_data;
	struct omap_abe *abe = snd_soc_dai_get_drvdata(dai);

	dev_dbg(fe->dev, "%s: %s %d\n", __func__, dai->name, stream);

	switch (dai->id) {
	case OMAP_ABE_FRONTEND_DAI_MEDIA:
		if (stream == SNDRV_PCM_STREAM_PLAYBACK)
			omap_abe_port_disable(abe->aess,
					abe->dai.port[OMAP_ABE_FE_PORT_MM_DL1]);
		else
			omap_abe_port_disable(abe->aess,
					abe->dai.port[OMAP_ABE_FE_PORT_MM_UL1]);
		break;
	case OMAP_ABE_FRONTEND_DAI_LP_MEDIA:
		if (stream == SNDRV_PCM_STREAM_PLAYBACK)
			omap_abe_port_disable(abe->aess,
					abe->dai.port[OMAP_ABE_FE_PORT_MM_DL_LP]);
		break;
	case OMAP_ABE_FRONTEND_DAI_MEDIA_CAPTURE:
		if (stream == SNDRV_PCM_STREAM_CAPTURE)
			omap_abe_port_disable(abe->aess,
					abe->dai.port[OMAP_ABE_FE_PORT_MM_UL2]);
		break;
	case OMAP_ABE_FRONTEND_DAI_MODEM:
	case OMAP_ABE_FRONTEND_DAI_VOICE:
		if (stream == SNDRV_PCM_STREAM_PLAYBACK)
			omap_abe_port_disable(abe->aess,
					abe->dai.port[OMAP_ABE_FE_PORT_VX_DL]);
		else
			omap_abe_port_disable(abe->aess,
					abe->dai.port[OMAP_ABE_FE_PORT_VX_UL]);
		break;
	case OMAP_ABE_FRONTEND_DAI_TONES:
		if (stream == SNDRV_PCM_STREAM_PLAYBACK)
			omap_abe_port_disable(abe->aess,
					abe->dai.port[OMAP_ABE_FE_PORT_TONES]);
		break;
	}
}

static void mute_fe_port_capture(struct snd_soc_pcm_runtime *fe, struct snd_soc_dai *dai, int mute)
{
	struct omap_abe *abe = snd_soc_dai_get_drvdata(dai);

	dev_dbg(fe->dev, "%s: %s FE %s\n",
			__func__, mute ? "mute" : "unmute",
			fe->dai_link->name);

	switch (fe->cpu_dai->id) {
	case OMAP_ABE_FRONTEND_DAI_MEDIA_CAPTURE:
		if (mute) {
			omap_aess_mute_gain(abe->aess, OMAP_AESS_MIXDL1_MM_UL2);
			omap_aess_mute_gain(abe->aess, OMAP_AESS_MIXDL2_MM_UL2);
		} else {
			omap_aess_unmute_gain(abe->aess, OMAP_AESS_MIXDL1_MM_UL2);
			omap_aess_unmute_gain(abe->aess, OMAP_AESS_MIXDL2_MM_UL2);
		}
		break;
	case OMAP_ABE_FRONTEND_DAI_MODEM:
	case OMAP_ABE_FRONTEND_DAI_VOICE:
		if (mute) {
			omap_aess_mute_gain(abe->aess, OMAP_AESS_MIXSDT_UL);
			omap_aess_mute_gain(abe->aess, OMAP_AESS_MIXAUDUL_UPLINK);
		} else {
			omap_aess_unmute_gain(abe->aess, OMAP_AESS_MIXSDT_UL);
			omap_aess_unmute_gain(abe->aess, OMAP_AESS_MIXAUDUL_UPLINK);
		}
		break;
	case OMAP_ABE_FRONTEND_DAI_MEDIA:
	default:
		break;
	}
}

static void mute_fe_port_playback(struct snd_soc_pcm_runtime *fe, struct snd_soc_dai *dai, int mute)
{

	struct omap_abe *abe = snd_soc_dai_get_drvdata(dai);

	dev_dbg(fe->dev, "%s: %s FE %s\n",
			__func__, mute ? "mute" : "unmute",
			fe->dai_link->name);

	switch (fe->cpu_dai->id) {
	case OMAP_ABE_FRONTEND_DAI_MEDIA:
	case OMAP_ABE_FRONTEND_DAI_LP_MEDIA:
		if (mute) {
			omap_aess_mute_gain(abe->aess, OMAP_AESS_MIXDL1_MM_DL);
			omap_aess_mute_gain(abe->aess, OMAP_AESS_MIXDL2_MM_DL);
		} else {
			omap_aess_unmute_gain(abe->aess, OMAP_AESS_MIXDL1_MM_DL);
			omap_aess_unmute_gain(abe->aess, OMAP_AESS_MIXDL2_MM_DL);
		}
		break;
	case OMAP_ABE_FRONTEND_DAI_VOICE:
	case OMAP_ABE_FRONTEND_DAI_MODEM:
		if (mute) {
			omap_aess_mute_gain(abe->aess, OMAP_AESS_MIXDL1_VX_DL);
			omap_aess_mute_gain(abe->aess, OMAP_AESS_MIXDL2_VX_DL);
		} else {
			omap_aess_unmute_gain(abe->aess, OMAP_AESS_MIXDL1_VX_DL);
			omap_aess_unmute_gain(abe->aess, OMAP_AESS_MIXDL2_VX_DL);
		}
		break;
	case OMAP_ABE_FRONTEND_DAI_TONES:
		if (mute) {
			omap_aess_mute_gain(abe->aess, OMAP_AESS_MIXDL1_TONES);
			omap_aess_mute_gain(abe->aess, OMAP_AESS_MIXDL2_TONES);
		} else {
			omap_aess_unmute_gain(abe->aess, OMAP_AESS_MIXDL1_TONES);
			omap_aess_unmute_gain(abe->aess, OMAP_AESS_MIXDL2_TONES);
		}
		break;
	default:
		break;
	}
}

static void mute_fe_port(struct snd_pcm_substream *substream,
		struct snd_soc_dai *dai, int stream)
{
	struct snd_soc_pcm_runtime *fe = substream->private_data;

	dev_dbg(fe->dev, "%s: %s %d\n", __func__, dai->name, stream);

	if (stream == SNDRV_PCM_STREAM_PLAYBACK)
		mute_fe_port_playback(fe, dai, 1);
	else
		mute_fe_port_capture(fe, dai, 1);
}

static void unmute_fe_port(struct snd_pcm_substream *substream,
		struct snd_soc_dai *dai, int stream)
{
	struct snd_soc_pcm_runtime *fe = substream->private_data;

	dev_dbg(fe->dev, "%s: %s %d\n", __func__, dai->name, stream);

	if (stream == SNDRV_PCM_STREAM_PLAYBACK)
		mute_fe_port_playback(fe, dai, 0);
	else
		mute_fe_port_capture(fe, dai, 0);
}

static void capture_trigger(struct snd_pcm_substream *substream,
		struct snd_soc_dai *dai, int cmd)
{
	struct snd_soc_pcm_runtime *fe = substream->private_data;
	struct snd_soc_dpcm *dpcm;
	struct snd_pcm_substream *be_substream;
	int stream = substream->stream;
	enum snd_soc_dpcm_state state;

	dev_dbg(fe->dev, "%s: %s %d\n", __func__, fe->cpu_dai->name, stream);

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:

		/* mute and enable BE ports */
		list_for_each_entry(dpcm, &fe->dpcm[stream].be_clients, list_be) {
			struct snd_soc_pcm_runtime *be = dpcm->be;

			/* does this trigger() apply to this BE and stream ? */
			if (!snd_soc_dpcm_be_can_update(fe, be, stream))
				continue;

			/* we only need to start BEs that are prepared or stopped */
			state = snd_soc_dpcm_be_get_state(be, stream);
			if ((state != SND_SOC_DPCM_STATE_PREPARE) &&
			    (state != SND_SOC_DPCM_STATE_STOP))
				continue;

			be_substream = snd_soc_dpcm_get_substream(be, stream);

			/* mute the BE port */
			mute_be(be, dai, stream);

			/* enable the BE port */
			enable_be_port(be, dai, stream);

			/* DAI work must be started/stopped at least 250us after ABE */
			udelay(250);

			/* trigger the BE port */
			snd_soc_dai_trigger(be_substream, cmd, be->cpu_dai);

			snd_soc_dpcm_be_set_state(be, stream, SND_SOC_DPCM_STATE_START);
		}

		/* does this trigger() apply to the FE ? */
		if (snd_soc_dpcm_fe_can_update(fe, stream)) {
			/* Enable Frontend sDMA  */
			snd_soc_platform_trigger(substream, cmd, fe->platform);

			enable_fe_port(substream, dai, stream);
			/* unmute FE port */
			unmute_fe_port(substream, dai, stream);
		}

		/* Restore ABE GAINS AMIC */
		list_for_each_entry(dpcm, &fe->dpcm[stream].be_clients, list_be) {
			struct snd_soc_pcm_runtime *be = dpcm->be;

			/* does this trigger() apply to this BE and stream ? */
			if (!snd_soc_dpcm_be_can_update(fe, be, stream))
				continue;

			/* unmute this BE port */
			unmute_be(be, dai, stream);
		}
		break;
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		/* Enable sDMA */
		snd_soc_platform_trigger(substream, cmd, fe->platform);
		enable_fe_port(substream, dai, stream);
		break;
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		/* Disable sDMA */
		disable_fe_port(substream, dai, stream);
		snd_soc_platform_trigger(substream, cmd, fe->platform);
		break;
	case SNDRV_PCM_TRIGGER_STOP:

		/* does this trigger() apply to the FE ? */
		if (snd_soc_dpcm_fe_can_update(fe, stream)) {
			/* mute FE port */
			mute_fe_port(substream, dai, stream);
			/* Disable sDMA */
			disable_fe_port(substream, dai, stream);
			snd_soc_platform_trigger(substream, cmd, fe->platform);
		}

		/* disable BE ports */
		list_for_each_entry(dpcm, &fe->dpcm[stream].be_clients, list_be) {
			struct snd_soc_pcm_runtime *be = dpcm->be;

			/* does this trigger() apply to this BE and stream ? */
			if (!snd_soc_dpcm_be_can_update(fe, be, stream))
				continue;

			/* we dont need to stop BEs that are already stopped */
			state = snd_soc_dpcm_be_get_state(be, stream);
			if (state != SND_SOC_DPCM_STATE_START)
				continue;

			/* only stop if last running user */
			if (!snd_soc_dpcm_can_be_free_stop(fe, be, stream))
				continue;

			be_substream = snd_soc_dpcm_get_substream(be, stream);

			/* mute the BE port */
			mute_be(be, dai, stream);

			/* disable the BE port */
			disable_be_port(be, dai, stream);

			/* DAI work must be started/stopped at least 250us after ABE */
			udelay(250);

			/* trigger BE port */
			snd_soc_dai_trigger(be_substream, cmd, be->cpu_dai);

			snd_soc_dpcm_be_set_state(be, stream, SND_SOC_DPCM_STATE_STOP);
		}
		break;
	default:
		break;
	}
}

static void playback_trigger(struct snd_pcm_substream *substream,
		struct snd_soc_dai *dai, int cmd)
{
	struct snd_soc_pcm_runtime *fe = substream->private_data;
	struct snd_soc_dpcm *dpcm;
	struct snd_pcm_substream *be_substream;
	int stream = substream->stream;
	enum snd_soc_dpcm_state state;

	dev_dbg(fe->dev, "%s: %s %d\n", __func__, fe->cpu_dai->name, stream);

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:

		/* mute and enable ports */
		list_for_each_entry(dpcm, &fe->dpcm[stream].be_clients, list_be) {
			struct snd_soc_pcm_runtime *be = dpcm->be;

			/* does this trigger() apply to the FE ? */
			if (!snd_soc_dpcm_be_can_update(fe, be, stream))
				continue;

			/* we only need to start BEs that are prepared or stopped */
			state = snd_soc_dpcm_be_get_state(be, stream);
			if ((state != SND_SOC_DPCM_STATE_PREPARE) &&
			    (state != SND_SOC_DPCM_STATE_STOP))
				continue;

			be_substream = snd_soc_dpcm_get_substream(be, stream);

			/* mute BE port */
			mute_be(be, dai, stream);

			/* enabled BE port */
			enable_be_port(be, dai, stream);

			/* DAI work must be started/stopped at least 250us after ABE */
			udelay(250);

			/* trigger BE port */
			snd_soc_dai_trigger(be_substream, cmd, be->cpu_dai);

			/* unmute the BE port */
			unmute_be(be, dai, stream);

			snd_soc_dpcm_be_set_state(be, stream, SND_SOC_DPCM_STATE_START);
		}

		/* does this trigger() apply to the FE ? */
		if (snd_soc_dpcm_fe_can_update(fe, stream)) {

			/* Enable Frontend sDMA  */
			snd_soc_platform_trigger(substream, cmd, fe->platform);
			enable_fe_port(substream, dai, stream);
		}

		/* unmute FE port (sensitive to runtime udpates) */
		unmute_fe_port(substream, dai, stream);
		break;
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		/* Enable Frontend sDMA  */
		snd_soc_platform_trigger(substream, cmd, fe->platform);
		enable_fe_port(substream, dai, stream);

		/* unmute FE port */
		unmute_fe_port(substream, dai, stream);
		break;
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		/* mute FE port */
		mute_fe_port(substream, dai, stream);
		/* disable Frontend sDMA  */
		disable_fe_port(substream, dai, stream);
		snd_soc_platform_trigger(substream, cmd, fe->platform);

		break;
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
		/* does this trigger() apply to the FE ? */
		if (snd_soc_dpcm_fe_can_update(fe, stream)) {
			/* mute FE port (sensitive to runtime udpates) */
			mute_fe_port(substream, dai, stream);
			/* disable the transfer */
			disable_fe_port(substream, dai, stream);
			snd_soc_platform_trigger(substream, cmd, fe->platform);

		}

		/* disable BE ports */
		list_for_each_entry(dpcm, &fe->dpcm[stream].be_clients, list_be) {
			struct snd_soc_pcm_runtime *be = dpcm->be;

			/* does this trigger() apply to this BE and stream ? */
			if (!snd_soc_dpcm_be_can_update(fe, be, stream))
				continue;

			/* we dont need to stop BEs that are already stopped */
			state = snd_soc_dpcm_be_get_state(be, stream);
			if (state != SND_SOC_DPCM_STATE_START)
				continue;

			/* only stop if last running user */
			if (!snd_soc_dpcm_can_be_free_stop(fe, be, stream))
				continue;

			be_substream = snd_soc_dpcm_get_substream(be, stream);

			/* mute the BE port */
			mute_be(be, dai, stream);

			/* disable the BE */
			disable_be_port(be, dai, stream);

			/* DAI work must be started/stopped at least 250us after ABE */
			udelay(250);

			/*  trigger the BE port */
			snd_soc_dai_trigger(be_substream, cmd, be->cpu_dai);

			snd_soc_dpcm_be_set_state(be, stream, SND_SOC_DPCM_STATE_STOP);
		}
		break;
	default:
		break;
	}
}

static int omap_abe_dai_startup(struct snd_pcm_substream *substream,
			struct snd_soc_dai *dai)
{
	struct omap_abe *abe = snd_soc_dai_get_drvdata(dai);
	struct omap_pcm_dma_data *dma_data;

	dev_dbg(dai->dev, "%s: %s\n", __func__, dai->name);
	abe->dai.num_active++;

	dma_data = &omap_abe_dai_dma_params[dai->id][substream->stream];
	snd_soc_dai_set_dma_data(dai, substream, dma_data);

	return 0;
}

static int omap_abe_dai_hw_params(struct snd_pcm_substream *substream,
			struct snd_pcm_hw_params *params,
			struct snd_soc_dai *dai)
{
	struct omap_abe *abe = snd_soc_dai_get_drvdata(dai);
	struct omap_pcm_dma_data *dma_data;
	struct omap_aess_data_format format;
	struct omap_aess_dma dma_params;

	dev_dbg(dai->dev, "%s: %s\n", __func__, dai->name);

	dma_data = snd_soc_dai_get_dma_data(dai, substream);
	switch (params_channels(params)) {
	case 1:
		if (params_format(params) == SNDRV_PCM_FORMAT_S16_LE)
			format.samp_format = OMAP_AESS_FORMAT_MONO_16_16;
		else
			format.samp_format = OMAP_AESS_FORMAT_MONO_MSB;
		break;
	case 2:
		if (params_format(params) == SNDRV_PCM_FORMAT_S16_LE)
			format.samp_format = OMAP_AESS_FORMAT_STEREO_16_16;
		else
			format.samp_format = OMAP_AESS_FORMAT_STEREO_MSB;
		break;
	case 3:
		format.samp_format = OMAP_AESS_FORMAT_THREE_MSB;
		break;
	case 4:
		format.samp_format = OMAP_AESS_FORMAT_FOUR_MSB;
		break;
	case 5:
		format.samp_format = OMAP_AESS_FORMAT_FIVE_MSB;
		break;
	case 6:
		format.samp_format = OMAP_AESS_FORMAT_SIX_MSB;
		break;
	case 7:
		format.samp_format = OMAP_AESS_FORMAT_SEVEN_MSB;
		break;
	case 8:
		format.samp_format = OMAP_AESS_FORMAT_EIGHT_MSB;
		break;
	default:
		dev_err(dai->dev, "%d channels not supported",
			params_channels(params));
		return -EINVAL;
	}

	format.f = params_rate(params);

	switch (dai->id) {
	case OMAP_ABE_FRONTEND_DAI_MEDIA:
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
			omap_aess_connect_cbpr_dmareq_port(abe->aess,
						OMAP_ABE_MM_DL_PORT, &format,
						ABE_CBPR0_IDX, &dma_params);
		else
			omap_aess_connect_cbpr_dmareq_port(abe->aess,
						OMAP_ABE_MM_UL_PORT, &format,
						ABE_CBPR3_IDX, &dma_params);
		break;
	case OMAP_ABE_FRONTEND_DAI_LP_MEDIA:
		return 0;
		break;
	case OMAP_ABE_FRONTEND_DAI_MEDIA_CAPTURE:
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
			return -EINVAL;
		else
			omap_aess_connect_cbpr_dmareq_port(abe->aess,
						OMAP_ABE_MM_UL2_PORT, &format,
						ABE_CBPR4_IDX, &dma_params);
		break;
	case OMAP_ABE_FRONTEND_DAI_VOICE:
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
			omap_aess_connect_cbpr_dmareq_port(abe->aess,
						OMAP_ABE_VX_DL_PORT, &format,
						ABE_CBPR1_IDX, &dma_params);
		else
			omap_aess_connect_cbpr_dmareq_port(abe->aess,
						OMAP_ABE_VX_UL_PORT, &format,
						ABE_CBPR2_IDX, &dma_params);
		break;
	case OMAP_ABE_FRONTEND_DAI_TONES:
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
			omap_aess_connect_cbpr_dmareq_port(abe->aess,
						OMAP_ABE_TONES_DL_PORT, &format,
						ABE_CBPR5_IDX, &dma_params);
		else
			return -EINVAL;
		break;
	case OMAP_ABE_FRONTEND_DAI_MODEM:

		/* MODEM is special case where data IO is performed by McBSP2
		 * directly onto VX_DL and VX_UL (instead of SDMA).
		 */
		format.samp_format = OMAP_AESS_FORMAT_STEREO_RSHIFTED_16;

		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
			omap_aess_connect_serial_port(abe->aess,
						OMAP_ABE_VX_DL_PORT, &format,
						MCBSP2_RX, &dma_params);
		else
			omap_aess_connect_serial_port(abe->aess,
						OMAP_ABE_VX_UL_PORT, &format,
						MCBSP2_TX, &dma_params);
		break;
	default:
		dev_err(dai->dev, "port %d not supported\n", dai->id);
		return -EINVAL;
	}

	/* configure frontend SDMA data */
	dma_data->port_addr = (unsigned long)dma_params.data;
	dma_data->packet_size = dma_params.iter;

	return 0;
}


static int omap_abe_dai_bespoke_trigger(struct snd_pcm_substream *substream,
				  int cmd, struct snd_soc_dai *dai)
{
	dev_dbg(dai->dev, "%s: %s cmd %d\n", __func__, dai->name, cmd);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		playback_trigger(substream, dai, cmd);
	else
		capture_trigger(substream, dai, cmd);

	return 0;
}


static void omap_abe_dai_shutdown(struct snd_pcm_substream *substream,
				struct snd_soc_dai *dai)
{
	struct omap_abe *abe = snd_soc_dai_get_drvdata(dai);
	int err;

	dev_dbg(dai->dev, "%s: %s\n", __func__, dai->name);

	/* shutdown the ABE if last user */
	if (!abe->active && !omap_aess_check_activity(abe->aess)) {
		omap_aess_set_opp_processing(abe->aess, ABE_OPP25);
		abe->opp.level = 25;
		omap_aess_stop_event_generator(abe->aess);
		udelay(250);
		if (abe->device_scale) {
			err = abe->device_scale(abe->dev, abe->dev, abe->opp.freqs[0]);
			if (err)
				dev_err(abe->dev, "failed to scale to lowest OPP\n");
		}
	}

	abe->dai.num_active--;
}

#ifdef CONFIG_PM
static int omap_abe_dai_suspend(struct snd_soc_dai *dai)
{
	struct omap_abe *abe = snd_soc_dai_get_drvdata(dai);

	dev_dbg(dai->dev, "%s: %s active %d\n",
		__func__, dai->name, dai->active);

	if (!dai->active)
		return 0;

	if (++abe->dai.num_suspended < abe->dai.num_active)
		return 0;

	omap_aess_mute_gain(abe->aess, OMAP_AESS_MIXSDT_UL);
	omap_aess_mute_gain(abe->aess, OMAP_AESS_MIXSDT_DL);
	omap_aess_mute_gain(abe->aess, OMAP_AESS_MIXAUDUL_MM_DL);
	omap_aess_mute_gain(abe->aess, OMAP_AESS_MIXAUDUL_TONES);
	omap_aess_mute_gain(abe->aess, OMAP_AESS_MIXAUDUL_UPLINK);
	omap_aess_mute_gain(abe->aess, OMAP_AESS_MIXAUDUL_VX_DL);
	omap_aess_mute_gain(abe->aess, OMAP_AESS_MIXVXREC_TONES);
	omap_aess_mute_gain(abe->aess, OMAP_AESS_MIXVXREC_VX_DL);
	omap_aess_mute_gain(abe->aess, OMAP_AESS_MIXVXREC_MM_DL);
	omap_aess_mute_gain(abe->aess, OMAP_AESS_MIXVXREC_VX_UL);
	omap_aess_mute_gain(abe->aess, OMAP_AESS_MIXDL1_MM_DL);
	omap_aess_mute_gain(abe->aess, OMAP_AESS_MIXDL1_MM_UL2);
	omap_aess_mute_gain(abe->aess, OMAP_AESS_MIXDL1_VX_DL);
	omap_aess_mute_gain(abe->aess, OMAP_AESS_MIXDL1_TONES);
	omap_aess_mute_gain(abe->aess, OMAP_AESS_MIXDL2_MM_DL);
	omap_aess_mute_gain(abe->aess, OMAP_AESS_MIXDL2_MM_UL2);
	omap_aess_mute_gain(abe->aess, OMAP_AESS_MIXDL2_VX_DL);
	omap_aess_mute_gain(abe->aess, OMAP_AESS_MIXDL2_TONES);
	omap_aess_mute_gain(abe->aess, OMAP_AESS_MIXECHO_DL1);
	omap_aess_mute_gain(abe->aess, OMAP_AESS_MIXECHO_DL2);

	return 0;
}

static int omap_abe_dai_resume(struct snd_soc_dai *dai)
{
	struct omap_abe *abe = snd_soc_dai_get_drvdata(dai);

	dev_dbg(dai->dev, "%s: %s active %d\n",
		__func__, dai->name, dai->active);

	if (!dai->active)
		return 0;

	if (abe->dai.num_suspended-- < abe->dai.num_active)
		return 0;

	omap_aess_unmute_gain(abe->aess, OMAP_AESS_MIXSDT_UL);
	omap_aess_unmute_gain(abe->aess, OMAP_AESS_MIXSDT_DL);
	omap_aess_unmute_gain(abe->aess, OMAP_AESS_MIXAUDUL_MM_DL);
	omap_aess_unmute_gain(abe->aess, OMAP_AESS_MIXAUDUL_TONES);
	omap_aess_unmute_gain(abe->aess, OMAP_AESS_MIXAUDUL_UPLINK);
	omap_aess_unmute_gain(abe->aess, OMAP_AESS_MIXAUDUL_VX_DL);
	omap_aess_unmute_gain(abe->aess, OMAP_AESS_MIXVXREC_TONES);
	omap_aess_unmute_gain(abe->aess, OMAP_AESS_MIXVXREC_VX_DL);
	omap_aess_unmute_gain(abe->aess, OMAP_AESS_MIXVXREC_MM_DL);
	omap_aess_unmute_gain(abe->aess, OMAP_AESS_MIXVXREC_VX_UL);
	omap_aess_unmute_gain(abe->aess, OMAP_AESS_MIXDL1_MM_DL);
	omap_aess_unmute_gain(abe->aess, OMAP_AESS_MIXDL1_MM_UL2);
	omap_aess_unmute_gain(abe->aess, OMAP_AESS_MIXDL1_VX_DL);
	omap_aess_unmute_gain(abe->aess, OMAP_AESS_MIXDL1_TONES);
	omap_aess_unmute_gain(abe->aess, OMAP_AESS_MIXDL2_MM_DL);
	omap_aess_unmute_gain(abe->aess, OMAP_AESS_MIXDL2_MM_UL2);
	omap_aess_unmute_gain(abe->aess, OMAP_AESS_MIXDL2_VX_DL);
	omap_aess_unmute_gain(abe->aess, OMAP_AESS_MIXDL2_TONES);
	omap_aess_unmute_gain(abe->aess, OMAP_AESS_MIXECHO_DL1);
	omap_aess_unmute_gain(abe->aess, OMAP_AESS_MIXECHO_DL2);

	return 0;
}
#else
#define omap_abe_dai_suspend	NULL
#define omap_abe_dai_resume		NULL
#endif

static int omap_abe_dai_probe(struct snd_soc_dai *dai)
{
	struct omap_abe *abe = snd_soc_dai_get_drvdata(dai);
	int i;

	abe->aess = omap_abe_port_mgr_get();
	if (!abe->aess)
		goto err;

	for (i = 0; i <= OMAP_ABE_MAX_PORT_ID; i++) {

		abe->dai.port[i] = omap_abe_port_open(abe->aess, i);
		if (abe->dai.port[i] == NULL)
			goto err_port;
	}

	return 0;

err_port:
	for (--i; i >= 0; i--)
		omap_abe_port_close(abe->aess, abe->dai.port[i]);
	omap_abe_port_mgr_put(abe->aess);
err:
	return -ENODEV;
}

static int omap_abe_dai_remove(struct snd_soc_dai *dai)
{
	struct omap_abe *abe = snd_soc_dai_get_drvdata(dai);
	int i;

	for (i = 0; i <= OMAP_ABE_MAX_PORT_ID; i++)
		omap_abe_port_close(abe->aess, abe->dai.port[i]);

	omap_abe_port_mgr_put(abe->aess);
	return 0;
}

static struct snd_soc_dai_ops omap_abe_dai_ops = {
	.startup	= omap_abe_dai_startup,
	.shutdown	= omap_abe_dai_shutdown,
	.hw_params	= omap_abe_dai_hw_params,
	.bespoke_trigger = omap_abe_dai_bespoke_trigger,
};

struct snd_soc_dai_driver omap_abe_dai[] = {
	{	/* Multimedia Playback and Capture */
		.name = "MultiMedia1",
		.probe = omap_abe_dai_probe,
		.remove = omap_abe_dai_remove,
		.suspend = omap_abe_dai_suspend,
		.resume = omap_abe_dai_resume,
		.playback = {
			.stream_name = "MM1 Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_44100 | SNDRV_PCM_RATE_48000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S32_LE,
		},
		.capture = {
			.stream_name = "MM1 Capture",
			.channels_min = 1,
			.channels_max = 6,
			.rates = SNDRV_PCM_RATE_48000,
			.formats = SNDRV_PCM_FMTBIT_S32_LE,
		},
		.ops = &omap_abe_dai_ops,
	},
	{	/* Multimedia Capture */
		.name = "MultiMedia2",
		.suspend = omap_abe_dai_suspend,
		.resume = omap_abe_dai_resume,
		.capture = {
			.stream_name = "MM2 Capture",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_48000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S32_LE,
		},
		.ops = &omap_abe_dai_ops,
	},
	{	/* Voice Playback and Capture */
		.name = "Voice",
		.suspend = omap_abe_dai_suspend,
		.resume = omap_abe_dai_resume,
		.playback = {
			.stream_name = "Voice Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000 | SNDRV_PCM_RATE_48000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S32_LE,
		},
		.capture = {
			.stream_name = "Voice Capture",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000 | SNDRV_PCM_RATE_48000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S32_LE,
		},
		.ops = &omap_abe_dai_ops,
	},
	{	/* Tones Playback */
		.name = "Tones",
		.suspend = omap_abe_dai_suspend,
		.resume = omap_abe_dai_resume,
		.playback = {
			.stream_name = "Tones Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_44100 | SNDRV_PCM_RATE_48000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S32_LE,
		},
		.ops = &omap_abe_dai_ops,
	},
	{	/* MODEM Voice Playback and Capture */
		.name = "MODEM",
		.suspend = omap_abe_dai_suspend,
		.resume = omap_abe_dai_resume,
		.playback = {
			.stream_name = "Modem Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S32_LE,
		},
		.capture = {
			.stream_name = "Modem Capture",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S32_LE,
		},
		.ops = &omap_abe_dai_ops,
	},
	{	/* Low Power HiFi Playback */
		.name = "MultiMedia1 LP",
		.suspend = omap_abe_dai_suspend,
		.resume = omap_abe_dai_resume,
		.playback = {
			.stream_name = "MMLP Playback",
			.channels_min = 2,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_44100 | SNDRV_PCM_RATE_48000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S32_LE,
		},
		.ops = &omap_abe_dai_ops,
	},
};
