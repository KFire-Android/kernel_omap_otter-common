/*
 * omap-hdmi.c  --  OMAP ALSA SoC DAI driver for HDMI audio
 *
 * Copyright (C) 2009 Texas Instruments
 *
 * Contact: Jorge Candelaria <x0107209@ti.com>
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

#include <plat/control.h>
#include <plat/dma.h>
#include <plat/hdmi_lib.h>
#include "omap-pcm.h"
#include "omap-hdmi.h"

#define OMAP_HDMI_RATES        (SNDRV_PCM_RATE_32000 | \
			SNDRV_PCM_RATE_44100 | SNDRV_PCM_RATE_48000)


/* Support for 16 and 24 bits */
#define OMAP_HDMI_FORMATS (SNDRV_PCM_FMTBIT_S16_LE | \
			SNDRV_PCM_FMTBIT_S24_LE)


struct omap_hdmi_data {
	struct hdmi_notifier notifier;
	struct snd_pcm_substream *substream;
	int active;
};

struct omap_hdmi_data hdmi_data;

static struct omap_pcm_dma_data omap_hdmi_dai_dma_params = {
	.name = "HDMI playback",
	.dma_req = OMAP44XX_DMA_DSS_HDMI_REQ,
	.port_addr = HDMI_WP + HDMI_WP_AUDIO_DATA,
	.sync_mode = OMAP_DMA_SYNC_PACKET,
};

static void hdmi_hpd_notifier(int state, void *data)
{
	struct omap_hdmi_data *hdmi_data = data;
	struct snd_pcm_substream *substream = hdmi_data->substream;

	if (state) {
		hdmi_data->active = 1;
	} else {
		if (substream)
			snd_pcm_stop(substream, SNDRV_PCM_STATE_DISCONNECTED);
		hdmi_data->active = 0;
	}
}

static void hdmi_pwrchange_notifier(int state, void *data)
{
	struct omap_hdmi_data *hdmi_data = data;
	struct snd_pcm_substream *substream = hdmi_data->substream;

	switch (state) {
	case HDMI_EVENT_POWEROFF:
		if (substream) {
			snd_pcm_stop(substream, SNDRV_PCM_STATE_DISCONNECTED);
			hdmi_set_audio_power(0);
		}
		hdmi_data->active = 0;
		break;
	case HDMI_EVENT_POWERON:
		break;
	case HDMI_EVENT_POWERPHYON:
		if (substream) {
			hdmi_w1_wrapper_enable(HDMI_WP);
			hdmi_w1_start_audio_transfer(HDMI_WP);
		}
		break;
	case HDMI_EVENT_POWERPHYOFF:
		if (substream) {
			hdmi_w1_stop_audio_transfer(HDMI_WP);
			hdmi_w1_wrapper_disable(HDMI_WP);
		}
		break;
	default:
		break;
	}
}

static int omap_hdmi_dai_startup(struct snd_pcm_substream *substream,
				  struct snd_soc_dai *dai)
{
	int err = 0;
	if (!hdmi_data.active) {
		printk(KERN_ERR "hdmi device not available\n");
		return -ENODEV;
	}

	hdmi_set_audio_power(1);
	hdmi_data.substream = substream;
	err = hdmi_w1_wrapper_enable(HDMI_WP);
	return err;
}

static void omap_hdmi_dai_shutdown(struct snd_pcm_substream *substream,
				    struct snd_soc_dai *dai)
{
	hdmi_w1_wrapper_disable(HDMI_WP);
	hdmi_data.substream = NULL;

	if (hdmi_data.active)
		hdmi_set_audio_power(0);

#ifdef CONFIG_OMAP_HDMI_AUDIO_WA
	if (hdmi_lib_stop_acr_wa())
		printk(KERN_WARNING "HDMI WA may be in bad state");
#endif
}

static int omap_hdmi_dai_trigger(struct snd_pcm_substream *substream, int cmd,
				  struct snd_soc_dai *dai)
{
	int err = 0;

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		if (cmd == SNDRV_PCM_TRIGGER_RESUME)
			hdmi_set_audio_power(1);
		err = hdmi_w1_start_audio_transfer(HDMI_WP);
		break;

	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		err = hdmi_w1_stop_audio_transfer(HDMI_WP);
		if (err)
			return err;
		if (cmd == SNDRV_PCM_TRIGGER_SUSPEND)
			hdmi_set_audio_power(0);
		break;
	default:
		err = -EINVAL;
	}

	return err;
}

static int omap_hdmi_dai_hw_params(struct snd_pcm_substream *substream,
				    struct snd_pcm_hw_params *params,
				    struct snd_soc_dai *dai)
{
	int err = 0;

	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_S16_LE:
		err = hdmi_configure_audio_sample_size(HDMI_SAMPLE_16BITS);
		break;
	case SNDRV_PCM_FORMAT_S24_LE:
		err = hdmi_configure_audio_sample_size(HDMI_SAMPLE_24BITS);
		break;
	default:
		return -EINVAL;
	}

	if (err)
		return err;

	err = hdmi_configure_audio_sample_freq(params_rate(params));
	if (err)
		return err;

	omap_hdmi_dai_dma_params.data_type = OMAP_DMA_DATA_TYPE_S32;
	omap_hdmi_dai_dma_params.packet_size = 0x20;

	snd_soc_dai_set_dma_data(dai, substream,
				 &omap_hdmi_dai_dma_params);

#ifdef CONFIG_OMAP_HDMI_AUDIO_WA
	err = hdmi_lib_start_acr_wa();
	if (err)
		printk(KERN_ERR "Failed to start ACR workaround[%d]]\n", err);
#endif


	return err;
}

static struct snd_soc_dai_ops omap_hdmi_dai_ops = {
	.startup	= omap_hdmi_dai_startup,
	.shutdown	= omap_hdmi_dai_shutdown,
	.trigger	= omap_hdmi_dai_trigger,
	.hw_params	= omap_hdmi_dai_hw_params,
};

static struct snd_soc_dai_driver omap_hdmi_dai = {
	.playback = {
		.channels_min = 2,
		/* currently we support only stereo HDMI */
		.channels_max = 2,
		.rates = OMAP_HDMI_RATES,
		.formats = OMAP_HDMI_FORMATS,
	},
	.ops = &omap_hdmi_dai_ops,
};

static __devinit int omap_hdmi_probe(struct platform_device *pdev)
{
	struct hdmi_notifier *notifier = &hdmi_data.notifier;

	notifier->hpd_notifier = hdmi_hpd_notifier;
	notifier->pwrchange_notifier = hdmi_pwrchange_notifier;
	notifier->private_data = &hdmi_data;
	hdmi_lib_init();
	hdmi_add_notifier(notifier);

	return snd_soc_register_dai(&pdev->dev, &omap_hdmi_dai);
}

static int __devexit omap_hdmi_remove(struct platform_device *pdev)
{
	struct hdmi_notifier *notifier = &hdmi_data.notifier;

	notifier->private_data = NULL;
	hdmi_remove_notifier(notifier);
	snd_soc_unregister_dai(&pdev->dev);

	return 0;
}

static struct platform_driver hdmi_dai_driver = {
	.driver = {
		.name = "hdmi-dai",
		.owner = THIS_MODULE,
	},
	.probe = omap_hdmi_probe,
	.remove = __devexit_p(omap_hdmi_remove),
};

static int __init hdmi_dai_init(void)
{
	return platform_driver_register(&hdmi_dai_driver);
}
module_init(hdmi_dai_init);

static void __exit hdmi_dai_exit(void)
{
	platform_driver_unregister(&hdmi_dai_driver);
}
module_exit(hdmi_dai_exit);

MODULE_AUTHOR("Jorge Candelaria <x0107209@ti.com");
MODULE_DESCRIPTION("OMAP HDMI SoC Interface");
MODULE_LICENSE("GPL");
