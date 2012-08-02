/*
 * omap-hdmi.c
 *
 * OMAP ALSA SoC DAI driver for HDMI audio on OMAP4 processors.
 * Copyright (C) 2010-2011 Texas Instruments Incorporated - http://www.ti.com/
 * Authors: Jorge Candelaria <jorge.candelaria@ti.com>
 *          Ricardo Neri <ricardo.neri@ti.com>
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
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/initval.h>
#include <sound/soc.h>
#include <sound/asound.h>
#include <sound/asoundef.h>

#include <plat/omap_hwmod.h>
#include <plat/dma.h>
#include <video/omapdss.h>
#include "omap-pcm.h"
#include "omap-hdmi.h"

#define DRV_NAME "omap-hdmi-audio-dai"

static struct omap_pcm_dma_data omap_hdmi_dai_dma_params = {
	.name = "HDMI playback",
	.sync_mode = OMAP_DMA_SYNC_PACKET,
};

static struct {
	struct omap_dss_device *dssdev;
	struct snd_aes_iec958 iec;
	struct snd_cea_861_aud_if cea;
	struct notifier_block notifier;
	int active;
	struct omap_hwmod *oh;
} hdmi;

static int omap_hdmi_dai_startup(struct snd_pcm_substream *substream,
				  struct snd_soc_dai *dai)
{
	int err;
	/*
	 * Make sure that the period bytes are multiple of the DMA packet size.
	 * Largest packet size we use is 32 32-bit words = 128 bytes
	 */
	err = snd_pcm_hw_constraint_step(substream->runtime, 0,
				 SNDRV_PCM_HW_PARAM_PERIOD_BYTES, 128);
	if (err < 0)
		return err;

	return 0;
}

static int omap_hdmi_dai_hw_params(struct snd_pcm_substream *substream,
				    struct snd_pcm_hw_params *params,
				    struct snd_soc_dai *dai)
{
	int err = 0;
	struct snd_aes_iec958 iec;
	struct snd_cea_861_aud_if cea;

	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_S16_LE:
		omap_hdmi_dai_dma_params.packet_size = 16;
		break;
	case SNDRV_PCM_FORMAT_S24_LE:
		omap_hdmi_dai_dma_params.packet_size = 32;
		break;
	default:
		err = -EINVAL;
	}

	omap_hdmi_dai_dma_params.data_type = OMAP_DMA_DATA_TYPE_S32;

	snd_soc_dai_set_dma_data(dai, substream,
				 &omap_hdmi_dai_dma_params);

	/* Fill IEC 60958 channel status word */
	/* TODO: no need to fill with zeros when using kzalloc */
	iec.status[0] = 0;
	iec.status[1] = 0;
	iec.status[2] = 0;
	iec.status[3] = 0;
	iec.status[4] = 0;
	iec.status[5] = 0;
	/* Fill CEA 861 audio infoframe */
	/* here fill commercial use (0) */
	iec.status[0] &= ~IEC958_AES0_PROFESSIONAL;
	/* here fill lpcm audio (0) */
	iec.status[0] &= ~IEC958_AES0_NONAUDIO;
	/* here fill copyright info */
	iec.status[0] |= IEC958_AES0_CON_NOT_COPYRIGHT;
	/* here fill emphasis info */
	iec.status[0] |= IEC958_AES0_CON_EMPHASIS_NONE;
	/* here fill mode info */
	iec.status[0] |= IEC958_AES1_PRO_MODE_NOTID;

	/* here fill category */
	iec.status[1] = IEC958_AES1_CON_GENERAL;

	/* here fill the source number */
	iec.status[2] |= IEC958_AES2_CON_SOURCE_UNSPEC;
	/* here fill the channel number */
	iec.status[2] |= IEC958_AES2_CON_CHANNEL_UNSPEC;

	/* here fill sampling rate */
	switch (params_rate(params)) {
	case 32000:
		iec.status[3] |= IEC958_AES3_CON_FS_32000;
		break;
	case 44100:
		iec.status[3] |= IEC958_AES3_CON_FS_44100;
		break;
	case 48000:
		iec.status[3] |= IEC958_AES3_CON_FS_48000;
		break;
	case 88200:
		iec.status[3] |= IEC958_AES3_CON_FS_88200;
		break;
	case 96000:
		iec.status[3] |= IEC958_AES3_CON_FS_96000;
		break;
	case 176400:
		iec.status[3] |= IEC958_AES3_CON_FS_176400;
		break;
	case 192000:
		iec.status[3] |= IEC958_AES3_CON_FS_192000;
		break;
	default:
		return -EINVAL;
	}

	/* here fill the clock accuracy */
	iec.status[3] |= IEC958_AES3_CON_CLOCK_1000PPM;

	/* here fill the word length */
	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_S16_LE:
		iec.status[4] |= IEC958_AES4_CON_WORDLEN_20_16;
		iec.status[4] |= 0; /* replace with word max define */
		break;
	case SNDRV_PCM_FORMAT_S24_LE:
		iec.status[4] |= IEC958_AES4_CON_WORDLEN_24_20;
		iec.status[4] |= IEC958_AES4_CON_MAX_WORDLEN_24;
		break;
	default:
		err = -EINVAL;
	}

	/* Fill the CEA 861 audio infoframe. Refer to the specification
	 * for details.
	 * The OMAP HDMI IP requires to use the 8-channel channel code when
	 * transmitting more than two channels.
	 */
	if (params_channels(params) == 2)
		cea.db4_ca = 0x0;
	else
		cea.db4_ca = 0x13;
	cea.db1_ct_cc = (params_channels(params) - 1)
		& CEA861_AUDIO_INFOFRAME_DB1CC;
	cea.db1_ct_cc |= CEA861_AUDIO_INFOFRAME_DB1CT_FROM_STREAM;
	cea.db2_sf_ss = CEA861_AUDIO_INFOFRAME_DB2SF_FROM_STREAM;
	cea.db2_sf_ss |= CEA861_AUDIO_INFOFRAME_DB2SS_FROM_STREAM;
	cea.db3 = 0; /* not used, all zeros */
	cea.db5_dminh_lsv = CEA861_AUDIO_INFOFRAME_DB5_DM_INH_PROHIBITED;
	cea.db5_dminh_lsv |= (0 & CEA861_AUDIO_INFOFRAME_DB5_LSV);

	hdmi.iec = iec;
	hdmi.cea = cea;

	err = hdmi.dssdev->driver->audio_config(hdmi.dssdev, &hdmi.iec,
							&hdmi.cea);

	return err;
}

static int hdmi_audio_notifier_callback(struct notifier_block *nb,
			unsigned long arg, void *ptr)
{
	enum omap_dss_display_state state = arg;
	int err = 0;

	if (state == OMAP_DSS_DISPLAY_ACTIVE) {
		if (hdmi.active)
			hdmi.dssdev->driver->audio_enable(hdmi.dssdev, false);
			err = hdmi.dssdev->driver->audio_config(hdmi.dssdev,
						&hdmi.iec, &hdmi.cea);
			if (hdmi.active) {
				omap_hwmod_set_slave_idlemode(hdmi.oh,
						HWMOD_IDLEMODE_NO);
				hdmi.dssdev->driver->audio_enable(hdmi.dssdev,
									true);
			}
		}
	return err;
}


static int omap_hdmi_dai_prepare(struct snd_pcm_substream *substream,
				struct snd_soc_dai *dai)
{
	return hdmi.dssdev->driver->audio_enable(hdmi.dssdev, true);
}

static int omap_hdmi_dai_trigger(struct snd_pcm_substream *substream, int cmd,
				struct snd_soc_dai *dai)
{
	int err = 0;

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		omap_hwmod_set_slave_idlemode(hdmi.oh,
				HWMOD_IDLEMODE_NO);
		hdmi.dssdev->driver->audio_start(hdmi.dssdev, true);
		hdmi.active = 1;
		break;
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		hdmi.active = 0;
		hdmi.dssdev->driver->audio_start(hdmi.dssdev, false);
		omap_hwmod_set_slave_idlemode(hdmi.oh,
			HWMOD_IDLEMODE_SMART_WKUP);
		break;
	default:
		err = -EINVAL;
	}
	return err;
}

static void omap_hdmi_dai_shutdown(struct snd_pcm_substream *substream,
				struct snd_soc_dai *dai)
{
	hdmi.dssdev->driver->audio_enable(hdmi.dssdev, false);
}

static const struct snd_soc_dai_ops omap_hdmi_dai_ops = {
	.startup	= omap_hdmi_dai_startup,
	.hw_params	= omap_hdmi_dai_hw_params,
	.prepare	= omap_hdmi_dai_prepare,
	.trigger	= omap_hdmi_dai_trigger,
	.shutdown	= omap_hdmi_dai_shutdown,
};

static struct snd_soc_dai_driver omap_hdmi_dai = {
	.playback = {
		.channels_min = 2,
	},
	.ops = &omap_hdmi_dai_ops,
};

static __devinit int omap_hdmi_probe(struct platform_device *pdev)
{
	int ret;
	struct resource *hdmi_rsrc;
	struct omap_dss_device *dssdev = NULL;
	bool hdmi_dev_found = false;

	hdmi_rsrc = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!hdmi_rsrc) {
		dev_err(&pdev->dev, "Cannot obtain IORESOURCE_MEM HDMI\n");
		return -ENODEV;
	}

	omap_hdmi_dai_dma_params.port_addr =  hdmi_rsrc->start
		+ OMAP_HDMI_AUDIO_DMA_PORT;

	hdmi_rsrc = platform_get_resource(pdev, IORESOURCE_DMA, 0);
	if (!hdmi_rsrc) {
		dev_err(&pdev->dev, "Cannot obtain IORESOURCE_DMA HDMI\n");
		return -ENODEV;
	}

	hdmi.oh = omap_hwmod_lookup("dss_hdmi");
	if (!hdmi.oh) {
		dev_err(&pdev->dev, "can't find omap_hwmod for hdmi\n");
		return -ENODEV;
	}

	omap_hdmi_dai_dma_params.dma_req =  hdmi_rsrc->start;

	/*
	 * Find an HDMI device. In the future, registers all the HDMI devices
	 * it finds and create a PCM for each.
	 */
	for_each_dss_dev(dssdev) {
		omap_dss_get_device(dssdev);

		if (!dssdev->driver) {
			omap_dss_put_device(dssdev);
			continue;
		}

		if (dssdev->type == OMAP_DISPLAY_TYPE_HDMI) {
			hdmi_dev_found = true;
			break;
		}
	}

	if (!hdmi_dev_found) {
		dev_err(&pdev->dev, "no driver for HDMI display found");
		return -ENODEV;
	}

	hdmi.dssdev = dssdev;

	/* the supported rates and sample format depend on the cpu */
	if (cpu_is_omap44xx()) {
		omap_hdmi_dai.playback.rates = OMAP4_HDMI_RATES;
		omap_hdmi_dai.playback.formats = OMAP4_HDMI_FORMATS;
		omap_hdmi_dai.playback.channels_max	= 8;
	} else { /* OMAP5 ES1.0 */
		omap_hdmi_dai.playback.rates = OMAP5_HDMI_RATES;
		omap_hdmi_dai.playback.formats = OMAP5_HDMI_FORMATS;
		omap_hdmi_dai.playback.channels_max	= 2;
	}
	ret = snd_soc_register_dai(&pdev->dev, &omap_hdmi_dai);

	hdmi.notifier.notifier_call = hdmi_audio_notifier_callback;
	blocking_notifier_chain_register(&hdmi.dssdev->state_notifiers,
							&hdmi.notifier);

	return ret;
}

static int __devexit omap_hdmi_remove(struct platform_device *pdev)
{
	omap_dss_put_device(hdmi.dssdev);
	blocking_notifier_chain_unregister(&hdmi.dssdev->state_notifiers,
							&hdmi.notifier);

	snd_soc_unregister_dai(&pdev->dev);
	return 0;
}

static struct platform_driver hdmi_dai_driver = {
	.driver = {
		.name = DRV_NAME,
		.owner = THIS_MODULE,
	},
	.probe = omap_hdmi_probe,
	.remove = __devexit_p(omap_hdmi_remove),
};

module_platform_driver(hdmi_dai_driver);

MODULE_AUTHOR("Jorge Candelaria <jorge.candelaria@ti.com>");
MODULE_AUTHOR("Ricardo Neri <ricardo.neri@ti.com>");
MODULE_DESCRIPTION("OMAP HDMI SoC Interface");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:" DRV_NAME);
