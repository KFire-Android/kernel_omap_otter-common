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
#include <linux/pm_runtime.h>

#include <plat/dma.h>
#include <plat/dma-44xx.h>

#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/soc-dpcm.h>

#include "omap-abe-priv.h"
#include "omap-pcm.h"

#define OMAP_ABE_FORMATS (SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S32_LE)

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
	}, {},
},
{
	{
		.name = "Vibra Playback",
		.dma_req = OMAP44XX_DMA_ABE_REQ_6,
		.data_type = OMAP_DMA_DATA_TYPE_S32,
		.sync_mode = OMAP_DMA_SYNC_PACKET,
	}, {},
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
	}, {},
},};


static int modem_get_dai(struct snd_pcm_substream *substream,
	struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct omap_abe *abe = snd_soc_dai_get_drvdata(dai);
	struct snd_soc_pcm_runtime *modem_rtd;

	abe->modem.substream[substream->stream] =
			snd_soc_get_dai_substream(rtd->card,
					OMAP_ABE_BE_MM_EXT1, !substream->stream);

	if (abe->modem.substream[substream->stream] == NULL)
		return -ENODEV;

	modem_rtd = abe->modem.substream[substream->stream]->private_data;
	abe->modem.substream[substream->stream]->runtime = substream->runtime;
	abe->modem.dai = modem_rtd->cpu_dai;

	return 0;
}

static int omap_abe_dl1_enabled(struct omap_abe *abe)
{
	int dl1;

	/* DL1 path is common for PDM_DL1, BT_VX_DL and MM_EXT_DL */
	dl1 = omap_abe_port_is_enabled(abe->aess,
				abe->dai.port[OMAP_ABE_BE_PORT_PDM_DL1]) +
		omap_abe_port_is_enabled(abe->aess,
				abe->dai.port[OMAP_ABE_BE_PORT_BT_VX_DL]) +
		omap_abe_port_is_enabled(abe->aess,
				abe->dai.port[OMAP_ABE_BE_PORT_MM_EXT_DL]);

#if !defined(CONFIG_SND_OMAP_SOC_ABE_DL2)
	dl1 += omap_abe_port_is_enabled(abe->aess,
				abe->dai.port[OMAP_ABE_BE_PORT_PDM_DL2]);
#endif

	return dl1;
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
#if defined(CONFIG_SND_OMAP_SOC_ABE_DL2)
			omap_aess_mute_gain(abe->aess, OMAP_AESS_GAIN_DL2_LEFT);
			omap_aess_mute_gain(abe->aess, OMAP_AESS_GAIN_DL2_RIGHT);
#else
			if (omap_abe_dl1_enabled(abe) == 1) {
				omap_aess_mute_gain(abe->aess, OMAP_AESS_GAIN_DL1_LEFT);
				omap_aess_mute_gain(abe->aess, OMAP_AESS_GAIN_DL1_RIGHT);
				omap_aess_mute_gain(abe->aess, OMAP_AESS_MIXSDT_DL);
			}
#endif
			break;
		case OMAP_ABE_DAI_PDM_VIB:
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
#if defined(CONFIG_SND_OMAP_SOC_ABE_DL2)
			omap_aess_unmute_gain(abe->aess, OMAP_AESS_GAIN_DL2_LEFT);
			omap_aess_unmute_gain(abe->aess, OMAP_AESS_GAIN_DL2_RIGHT);
#else
			omap_aess_unmute_gain(abe->aess, OMAP_AESS_GAIN_DL1_LEFT);
			omap_aess_unmute_gain(abe->aess, OMAP_AESS_GAIN_DL1_RIGHT);
			omap_aess_unmute_gain(abe->aess, OMAP_AESS_MIXSDT_DL);
#endif
			break;
		case OMAP_ABE_DAI_PDM_VIB:
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

/* Connect BE to serial port (McBSP) */
static int be_connect_serial_port(struct snd_soc_pcm_runtime *be,
				  struct snd_soc_dai *dai, int stream)
{
	struct snd_soc_dpcm_runtime *dpcm = &be->dpcm[stream];
	struct snd_pcm_hw_params *params = &dpcm->hw_params;
	struct snd_soc_dai *cpu_dai = be->cpu_dai;
	struct omap_abe *abe = snd_soc_dai_get_drvdata(dai);
	struct omap_aess_data_format format;
	u32 port, mcbsp_id;

	format.f = params_rate(params);

	switch (params_channels(params)) {
	case 1:
		if (params_format(params) == SNDRV_PCM_FORMAT_S16_LE)
			format.samp_format = MONO_RSHIFTED_16;
		else
			format.samp_format = MONO_MSB;
		break;
	case 2:
		if (params_format(params) == SNDRV_PCM_FORMAT_S16_LE)
			format.samp_format = STEREO_RSHIFTED_16;
		else
			format.samp_format = STEREO_MSB;
		break;
	default:
		dev_err(dai->dev, "%d channels not supported\n",
			params_channels(params));
		return -EINVAL;
	}

	switch (be->dai_link->be_id) {
	case OMAP_ABE_DAI_BT_VX:
		if (stream == SNDRV_PCM_STREAM_PLAYBACK)
			port = OMAP_ABE_BT_VX_DL_PORT;
		else
			port = OMAP_ABE_BT_VX_UL_PORT;
		break;
	case OMAP_ABE_DAI_MM_FM:
		if (stream == SNDRV_PCM_STREAM_PLAYBACK)
			port = OMAP_ABE_MM_EXT_OUT_PORT;
		else
			port = OMAP_ABE_MM_EXT_IN_PORT;
		break;
	case OMAP_ABE_DAI_MODEM:
		if (stream == SNDRV_PCM_STREAM_PLAYBACK)
			port = OMAP_ABE_VX_DL_PORT;
		else
			port = OMAP_ABE_VX_UL_PORT;
		/*
		 * Invert stream direction:
		 * - McBSP RX is used for voice call playback direction
		 * - McBSP TX is used for voice call capture direction
		 */
		stream = !stream;
		break;
	default:
		dev_err(dai->dev, "serial port cannot be connected to BE %d\n",
			be->dai_link->be_id);
		return -EINVAL;
	}

	switch (cpu_dai->id) {
	case 1:
		if (stream == SNDRV_PCM_STREAM_PLAYBACK)
			mcbsp_id = MCBSP1_TX;
		else
			mcbsp_id = MCBSP1_RX;
		break;
	case 2:
		if (stream == SNDRV_PCM_STREAM_PLAYBACK)
			mcbsp_id = MCBSP2_TX;
		else
			mcbsp_id = MCBSP2_RX;
		break;
	case 3:
		if (stream == SNDRV_PCM_STREAM_PLAYBACK)
			mcbsp_id = MCBSP3_TX;
		else
			mcbsp_id = MCBSP3_RX;
		break;
	default:
		dev_err(dai->dev, "invalid DAI id %d\n",
			dai->id);
		return -EINVAL;
	}

	omap_aess_connect_serial_port(abe->aess, port, &format, mcbsp_id);

	return 0;
}

static void enable_be_port(struct snd_soc_pcm_runtime *be,
		struct snd_soc_dai *dai, int stream)
{
	struct omap_abe *abe = snd_soc_dai_get_drvdata(dai);

	dev_dbg(be->dev, "%s: %s %d\n", __func__, be->cpu_dai->name, stream);

	switch (be->dai_link->be_id) {
	case OMAP_ABE_DAI_PDM_DL1:
	case OMAP_ABE_DAI_PDM_DL2:
	case OMAP_ABE_DAI_PDM_VIB:
	case OMAP_ABE_DAI_PDM_UL:
		/* McPDM is a special case, handled by McPDM driver */
		break;
	case OMAP_ABE_DAI_BT_VX:
		if (stream == SNDRV_PCM_STREAM_PLAYBACK) {

			/* port can only be configured if it's not running */
			if (omap_abe_port_is_enabled(abe->aess,
					abe->dai.port[OMAP_ABE_BE_PORT_BT_VX_DL]))
				return;

			be_connect_serial_port(be, dai, stream);
			omap_abe_port_enable(abe->aess,
				abe->dai.port[OMAP_ABE_BE_PORT_BT_VX_DL]);
		} else {

			/* port can only be configured if it's not running */
			if (omap_abe_port_is_enabled(abe->aess,
					abe->dai.port[OMAP_ABE_BE_PORT_BT_VX_UL]))
				return;

			be_connect_serial_port(be, dai, stream);
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

			be_connect_serial_port(be, dai, stream);
			omap_abe_port_enable(abe->aess,
				abe->dai.port[OMAP_ABE_BE_PORT_MM_EXT_DL]);
		} else {

			/* port can only be configured if it's not running */
			if (omap_abe_port_is_enabled(abe->aess,
					abe->dai.port[OMAP_ABE_BE_PORT_MM_EXT_UL]))
				return;

			be_connect_serial_port(be, dai, stream);
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
		if (stream == SNDRV_PCM_STREAM_PLAYBACK)
			omap_abe_port_enable(abe->aess,
					abe->dai.port[OMAP_ABE_FE_PORT_MM_DL_LP]);
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
	case OMAP_ABE_FRONTEND_DAI_VIBRA:
		if (stream == SNDRV_PCM_STREAM_PLAYBACK)
			omap_abe_port_enable(abe->aess,
					abe->dai.port[OMAP_ABE_FE_PORT_VIB]);
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
	case OMAP_ABE_DAI_PDM_VIB:
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
	case OMAP_ABE_FRONTEND_DAI_VIBRA:
		if (stream == SNDRV_PCM_STREAM_PLAYBACK)
			omap_abe_port_disable(abe->aess,
					abe->dai.port[OMAP_ABE_FE_PORT_VIB]);
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

static void mute_fe_port_playback(struct snd_soc_pcm_runtime *fe,
		struct snd_soc_dai *dai, int mute)
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
	case OMAP_ABE_FRONTEND_DAI_VIBRA:
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
	struct snd_soc_dpcm_params *dpcm_params;
	struct snd_pcm_substream *be_substream;
	int stream = substream->stream;
	enum snd_soc_dpcm_state state;

	dev_dbg(fe->dev, "%s: %s %d\n", __func__, fe->cpu_dai->name, stream);

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:

		/* mute and enable BE ports */
		list_for_each_entry(dpcm_params, &fe->dpcm[stream].be_clients, list_be) {
			struct snd_soc_pcm_runtime *be = dpcm_params->be;

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
			snd_soc_dpcm_platform_trigger(substream, cmd, fe->platform);
			enable_fe_port(substream, dai, stream);
			/* unmute FE port */
			unmute_fe_port(substream, dai, stream);
		}

		/* Restore ABE GAINS AMIC */
		list_for_each_entry(dpcm_params, &fe->dpcm[stream].be_clients, list_be) {
			struct snd_soc_pcm_runtime *be = dpcm_params->be;

			/* does this trigger() apply to this BE and stream ? */
			if (!snd_soc_dpcm_be_can_update(fe, be, stream))
				continue;

			/* unmute this BE port */
			unmute_be(be, dai, stream);
		}
		break;
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		/* Enable sDMA */
		snd_soc_dpcm_platform_trigger(substream, cmd, fe->platform);
		enable_fe_port(substream, dai, stream);
		break;
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		/* Disable sDMA */
		disable_fe_port(substream, dai, stream);
		snd_soc_dpcm_platform_trigger(substream, cmd, fe->platform);
		break;
	case SNDRV_PCM_TRIGGER_STOP:

		/* does this trigger() apply to the FE ? */
		if (snd_soc_dpcm_fe_can_update(fe, stream)) {
			/* mute FE port */
			mute_fe_port(substream, dai, stream);
			/* Disable sDMA */
			disable_fe_port(substream, dai, stream);
			snd_soc_dpcm_platform_trigger(substream, cmd, fe->platform);
		}

		/* disable BE ports */
		list_for_each_entry(dpcm_params, &fe->dpcm[stream].be_clients, list_be) {
			struct snd_soc_pcm_runtime *be = dpcm_params->be;

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
	struct snd_soc_dpcm_params *dpcm_params;
	struct snd_pcm_substream *be_substream;
	int stream = substream->stream;
	enum snd_soc_dpcm_state state;

	dev_dbg(fe->dev, "%s: %s %d\n", __func__, fe->cpu_dai->name, stream);

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:

		/* mute and enable ports */
		list_for_each_entry(dpcm_params, &fe->dpcm[stream].be_clients, list_be) {
			struct snd_soc_pcm_runtime *be = dpcm_params->be;

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
			snd_soc_dpcm_platform_trigger(substream, cmd, fe->platform);
			enable_fe_port(substream, dai, stream);
		}

		/* unmute FE port (sensitive to runtime udpates) */
		unmute_fe_port(substream, dai, stream);
		break;
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		/* Enable Frontend sDMA  */
		snd_soc_dpcm_platform_trigger(substream, cmd, fe->platform);
		enable_fe_port(substream, dai, stream);

		/* unmute FE port */
		unmute_fe_port(substream, dai, stream);
		break;
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		/* mute FE port */
		mute_fe_port(substream, dai, stream);
		/* disable Frontend sDMA  */
		disable_fe_port(substream, dai, stream);
		snd_soc_dpcm_platform_trigger(substream, cmd, fe->platform);

		break;
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
		/* does this trigger() apply to the FE ? */
		if (snd_soc_dpcm_fe_can_update(fe, stream)) {
			/* mute FE port (sensitive to runtime udpates) */
			mute_fe_port(substream, dai, stream);
			/* disable the transfer */
			disable_fe_port(substream, dai, stream);
			snd_soc_dpcm_platform_trigger(substream, cmd, fe->platform);

		}

		/* disable BE ports */
		list_for_each_entry(dpcm_params, &fe->dpcm[stream].be_clients, list_be) {
			struct snd_soc_pcm_runtime *be = dpcm_params->be;

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
	int ret = 0;

	dev_dbg(dai->dev, "%s: %s\n", __func__, dai->name);
	abe->dai.num_active++;

	omap_abe_pm_runtime_get_sync(abe);

	if (dai->id == OMAP_ABE_FRONTEND_DAI_MODEM) {

		ret = modem_get_dai(substream, dai);
		if (ret < 0) {
			dev_err(dai->dev, "failed to get MODEM DAI\n");
			goto err;
		}
		dev_dbg(abe->modem.dai->dev, "%s: MODEM stream %d\n",
				__func__, substream->stream);

		pm_runtime_get_sync(abe->modem.dai->dev);

		ret = snd_soc_dai_startup(abe->modem.substream[substream->stream],
				abe->modem.dai);
		if (ret < 0) {
			dev_err(abe->modem.dai->dev, "failed to open DAI %d\n", ret);
			pm_runtime_put(abe->modem.dai->dev);
			goto err;
		}
	}

	return 0;

err:
	omap_abe_pm_runtime_put_sync(abe);
	abe->dai.num_active--;
	return ret;
}

static int omap_abe_dai_hw_params(struct snd_pcm_substream *substream,
			struct snd_pcm_hw_params *params,
			struct snd_soc_dai *dai)
{
	struct omap_abe *abe = snd_soc_dai_get_drvdata(dai);
	struct omap_pcm_dma_data *dma_data;
	struct omap_aess_data_format format;
	struct omap_aess_dma dma_sink;
	struct omap_aess_dma dma_params;
	struct snd_pcm_substream *modem_substream;
	struct snd_soc_pcm_runtime *modem_rtd;
	struct snd_pcm_hw_params *modem_params;
	int ret;

	dev_dbg(dai->dev, "%s: %s\n", __func__, dai->name);

	dma_data = &omap_abe_dai_dma_params[dai->id][substream->stream];
	switch (params_channels(params)) {
	case 1:
		if (params_format(params) == SNDRV_PCM_FORMAT_S16_LE)
			format.samp_format = MONO_16_16;
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
	case 6:
		format.samp_format = SIX_MSB;
		break;
	case 7:
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
	case OMAP_ABE_FRONTEND_DAI_MEDIA:
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
			omap_aess_connect_cbpr_dmareq_port(abe->aess, OMAP_ABE_MM_DL_PORT, &format, ABE_CBPR0_IDX,
					&dma_sink);
			omap_aess_read_port_address(abe->aess, OMAP_ABE_MM_DL_PORT, &dma_params);
		} else {
			omap_aess_connect_cbpr_dmareq_port(abe->aess, OMAP_ABE_MM_UL_PORT, &format,  ABE_CBPR3_IDX,
					&dma_sink);
			omap_aess_read_port_address(abe->aess, OMAP_ABE_MM_UL_PORT, &dma_params);
		}
		break;
	case OMAP_ABE_FRONTEND_DAI_LP_MEDIA:
		return 0;
		break;
	case OMAP_ABE_FRONTEND_DAI_MEDIA_CAPTURE:
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
			return -EINVAL;
		else {
			omap_aess_connect_cbpr_dmareq_port(abe->aess, OMAP_ABE_MM_UL2_PORT, &format,  ABE_CBPR4_IDX,
					&dma_sink);
			omap_aess_read_port_address(abe->aess, OMAP_ABE_MM_UL2_PORT, &dma_params);
		}
		break;
	case OMAP_ABE_FRONTEND_DAI_VOICE:
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
			omap_aess_connect_cbpr_dmareq_port(abe->aess, OMAP_ABE_VX_DL_PORT, &format, ABE_CBPR1_IDX,
					&dma_sink);
			omap_aess_read_port_address(abe->aess, OMAP_ABE_VX_DL_PORT, &dma_params);
		} else {
			omap_aess_connect_cbpr_dmareq_port(abe->aess, OMAP_ABE_VX_UL_PORT, &format,  ABE_CBPR2_IDX,
					&dma_sink);
			omap_aess_read_port_address(abe->aess, OMAP_ABE_VX_UL_PORT, &dma_params);
		}
		break;
	case OMAP_ABE_FRONTEND_DAI_TONES:
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
			omap_aess_connect_cbpr_dmareq_port(abe->aess, OMAP_ABE_TONES_DL_PORT, &format, ABE_CBPR5_IDX,
					&dma_sink);
			omap_aess_read_port_address(abe->aess, OMAP_ABE_TONES_DL_PORT, &dma_params);
		} else
			return -EINVAL;
		break;
	case OMAP_ABE_FRONTEND_DAI_VIBRA:
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
			omap_aess_connect_cbpr_dmareq_port(abe->aess, OMAP_ABE_VIB_DL_PORT, &format, ABE_CBPR6_IDX,
					&dma_sink);
			omap_aess_read_port_address(abe->aess, OMAP_ABE_VIB_DL_PORT, &dma_params);
		} else
			return -EINVAL;
		break;
	case OMAP_ABE_FRONTEND_DAI_MODEM:
		modem_substream = abe->modem.substream[substream->stream];
		modem_rtd = modem_substream->private_data;
		modem_params = &modem_rtd->dpcm[substream->stream].hw_params;

		/*
		 * MODEM is special case where data IO is performed by McBSP2
		 * directly onto VX_DL and VX_UL (instead of SDMA).
		 * It's assumed that the FE port is open with the params the
		 * McBSP port will actually use
		 */
		memcpy(modem_params, params, sizeof(struct snd_pcm_hw_params));
		be_connect_serial_port(modem_rtd, dai, substream->stream);

		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
			omap_aess_read_port_address(abe->aess, OMAP_ABE_VX_DL_PORT, &dma_params);
		else
			omap_aess_read_port_address(abe->aess, OMAP_ABE_VX_UL_PORT, &dma_params);
		break;
	default:
		dev_err(dai->dev, "port %d not supported\n", dai->id);
		return -EINVAL;
	}

	/* configure frontend SDMA data */
	dma_data->port_addr = (unsigned long)dma_params.data;
	dma_data->packet_size = dma_params.iter;

	if (dai->id == OMAP_ABE_FRONTEND_DAI_MODEM) {
		/* call hw_params on McBSP with correct DMA data */
		snd_soc_dai_set_dma_data(abe->modem.dai, substream,
					dma_data);

		dev_dbg(abe->modem.dai->dev, "%s: MODEM stream %d\n",
				__func__, substream->stream);

		ret = snd_soc_dai_hw_params(abe->modem.substream[substream->stream],
				params, abe->modem.dai);
		if (ret < 0)
			dev_err(abe->modem.dai->dev, "MODEM hw_params failed\n");
		return ret;
	}

	snd_soc_dai_set_dma_data(dai, substream, dma_data);

	return 0;
}

static int omap_abe_dai_prepare(struct snd_pcm_substream *substream,
				  struct snd_soc_dai *dai)
{
	struct omap_abe *abe = snd_soc_dai_get_drvdata(dai);
	int ret = 0;

	dev_dbg(dai->dev, "%s: %s\n", __func__, dai->name);

	if (dai->id == OMAP_ABE_FRONTEND_DAI_MODEM) {
		ret = snd_soc_dai_prepare(abe->modem.substream[substream->stream],
				abe->modem.dai);

		dev_dbg(abe->modem.dai->dev, "%s: MODEM stream %d\n",
				__func__, substream->stream);

		if (ret < 0) {
			dev_err(abe->modem.dai->dev, "MODEM prepare failed\n");
			return ret;
		}
	}
	return ret;
}

static int omap_abe_dai_trigger(struct snd_pcm_substream *substream,
				  int cmd, struct snd_soc_dai *dai)
{
	struct omap_abe *abe = snd_soc_dai_get_drvdata(dai);
	int ret = 0;

	dev_dbg(dai->dev, "%s: %s cmd %d\n", __func__, dai->name, cmd);

	if (dai->id == OMAP_ABE_FRONTEND_DAI_MODEM) {

		dev_dbg(abe->modem.dai->dev, "%s: MODEM stream %d cmd %d\n",
				__func__, substream->stream, cmd);

		ret = snd_soc_dai_trigger(abe->modem.substream[substream->stream],
				cmd, abe->modem.dai);
		if (ret < 0) {
			dev_err(abe->modem.dai->dev, "MODEM trigger failed\n");
			return ret;
		}
	}

	return ret;
}

static int omap_abe_dai_bespoke_trigger(struct snd_pcm_substream *substream,
				  int cmd, struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *fe = substream->private_data;
	struct omap_abe *abe = snd_soc_dai_get_drvdata(dai);
	int ret = 0;

	dev_dbg(dai->dev, "%s: %s cmd %d\n", __func__, dai->name, cmd);

	if ((dai->id == OMAP_ABE_FRONTEND_DAI_MODEM) &&
			snd_soc_dpcm_fe_can_update(fe, substream->stream)) {

		dev_dbg(abe->modem.dai->dev, "%s: MODEM stream %d cmd %d\n",
				__func__, substream->stream, cmd);

		ret = snd_soc_dai_trigger(abe->modem.substream[substream->stream],
				cmd, abe->modem.dai);
		if (ret < 0) {
			dev_err(abe->modem.dai->dev, "MODEM trigger failed\n");
			return ret;
		}
	}

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		playback_trigger(substream, dai, cmd);
	else
		capture_trigger(substream, dai, cmd);

	return ret;
}

static int omap_abe_dai_hw_free(struct snd_pcm_substream *substream,
				  struct snd_soc_dai *dai)
{
	struct omap_abe *abe = snd_soc_dai_get_drvdata(dai);
	int ret = 0;

	dev_dbg(dai->dev, "%s: %s\n", __func__, dai->name);

	if (dai->id == OMAP_ABE_FRONTEND_DAI_MODEM) {

		dev_dbg(abe->modem.dai->dev, "%s: MODEM stream %d\n",
				__func__, substream->stream);

		ret = snd_soc_dai_hw_free(abe->modem.substream[substream->stream],
				abe->modem.dai);
		if (ret < 0) {
			dev_err(abe->modem.dai->dev, "MODEM hw_free failed\n");
			return ret;
		}
	}
	return ret;
}

static void omap_abe_dai_shutdown(struct snd_pcm_substream *substream,
				struct snd_soc_dai *dai)
{
	struct omap_abe *abe = snd_soc_dai_get_drvdata(dai);
	int err;

	dev_dbg(dai->dev, "%s: %s\n", __func__, dai->name);

	if (dai->id == OMAP_ABE_FRONTEND_DAI_MODEM) {
		dev_dbg(abe->modem.dai->dev, "%s: MODEM stream %d\n",
				__func__, substream->stream);

		snd_soc_dai_shutdown(abe->modem.substream[substream->stream],
				abe->modem.dai);
		pm_runtime_put(abe->modem.dai->dev);

	}

	/* shutdown the ABE if last user */
	if (!abe->active && !omap_aess_check_activity(abe->aess)) {
		omap_aess_set_opp_processing(abe->aess, ABE_OPP25);
		abe->opp.level = 25;
		omap_aess_stop_event_generator(abe->aess);
		udelay(250);
		if (abe->device_scale) {
			err = abe->device_scale(abe->dev, abe->opp.freqs[0]);
			if (err)
				dev_err(abe->dev, "failed to scale to lowest OPP\n");
		}
	}

	omap_abe_pm_runtime_put_sync(abe);

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
	.hw_free	= omap_abe_dai_hw_free,
	.prepare	= omap_abe_dai_prepare,
	.trigger	= omap_abe_dai_trigger,
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
			.rates = SNDRV_PCM_RATE_48000 | SNDRV_PCM_RATE_44100,
			.formats = OMAP_ABE_FORMATS,
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
			.formats = OMAP_ABE_FORMATS,
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
			.formats = OMAP_ABE_FORMATS,
		},
		.capture = {
			.stream_name = "Voice Capture",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000 | SNDRV_PCM_RATE_48000,
			.formats = OMAP_ABE_FORMATS,
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
			.rates = SNDRV_PCM_RATE_48000 | SNDRV_PCM_RATE_44100,
			.formats = OMAP_ABE_FORMATS,
		},
		.ops = &omap_abe_dai_ops,
	},
	{	/* Vibra */
		.name = "Vibra",
		.suspend = omap_abe_dai_suspend,
		.resume = omap_abe_dai_resume,
		.playback = {
			.stream_name = "Vibra Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_CONTINUOUS,
			.formats = OMAP_ABE_FORMATS,
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
			.formats = OMAP_ABE_FORMATS,
		},
		.capture = {
			.stream_name = "Modem Capture",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000,
			.formats = OMAP_ABE_FORMATS,
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
			.formats = OMAP_ABE_FORMATS,
		},
		.ops = &omap_abe_dai_ops,
	},
};
