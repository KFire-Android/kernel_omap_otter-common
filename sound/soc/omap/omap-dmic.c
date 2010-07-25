/*
 * omap-dmic.c  --  OMAP ASoC DMIC DAI driver
 *
 * Copyright (C) 2010 Texas Instruments
 *
 * Author: Liam Girdwood <lrg@slimlogic.co.uk>
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
#include <linux/platform_device.h>
#include <linux/wait.h>
#include <linux/interrupt.h>
#include <linux/err.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/irq.h>

#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/initval.h>
#include <sound/soc.h>

#include <plat/control.h>
#include <plat/dma.h>
#include "omap-pcm.h"

struct omap_dmic_data {
	int blah;
};

static int omap_dmic_dai_startup(struct snd_pcm_substream *substream,
				  struct snd_soc_dai *dai)
{
	int err = 0;


	return err;
}

static void omap_dmic_dai_shutdown(struct snd_pcm_substream *substream,
				    struct snd_soc_dai *dai)
{

}

static int omap_dmic_dai_hw_params(struct snd_pcm_substream *substream,
				    struct snd_pcm_hw_params *params,
				    struct snd_soc_dai *dai)
{
//	struct omap_dmic_data *dmic_priv = snd_soc_dai_get_drvdata(dai);


	return 0;
}

static int omap_dmic_dai_hw_free(struct snd_pcm_substream *substream,
				  struct snd_soc_dai *dai)
{
//	struct omap_dmic_data *dmic_priv = snd_soc_dai_get_drvdata(dai);


	return 0;
}

static struct snd_soc_dai_ops omap_dmic_dai_ops = {
	.startup	= omap_dmic_dai_startup,
	.shutdown	= omap_dmic_dai_shutdown,
	.hw_params	= omap_dmic_dai_hw_params,
	.hw_free	= omap_dmic_dai_hw_free,
};

// TODO: get the correct rates etc
#define OMAP_DMIC_RATES	(SNDRV_PCM_RATE_48000 | SNDRV_PCM_RATE_96000)
#define OMAP_DMIC_FORMATS	(SNDRV_PCM_FMTBIT_S16_LE)

static int omap_dmic_dai_probe(struct snd_soc_dai *dai)
{
//	snd_soc_dai_set_drvdata(dai, &dmic_data);
	return 0;
}

static struct snd_soc_dai_driver omap_dmic_dai = {
	.name = "omap-dmic-dai",
	.probe = omap_dmic_dai_probe,
	.capture = {
		.channels_min = 1,
		.channels_max = 2,
		.rates = OMAP_DMIC_RATES,
		.formats = OMAP_DMIC_FORMATS,
	},
	.ops = &omap_dmic_dai_ops,
};

static __devinit int asoc_dmic_probe(struct platform_device *pdev)
{
	return snd_soc_register_dai(&pdev->dev, &omap_dmic_dai);
}

static int __devexit asoc_dmic_remove(struct platform_device *pdev)
{
	snd_soc_unregister_dai(&pdev->dev);
	return 0;
}

static struct platform_driver asoc_dmic_driver = {
	.driver = {
			.name = "omap-dmic-dai",
			.owner = THIS_MODULE,
	},

	.probe = asoc_dmic_probe,
	.remove = __devexit_p(asoc_dmic_remove),
};

static int __init snd_omap_dmic_init(void)
{
	return platform_driver_register(&asoc_dmic_driver);
}
module_init(snd_omap_dmic_init);

static void __exit snd_omap_dmic_exit(void)
{
	platform_driver_unregister(&asoc_dmic_driver);
}
module_exit(snd_omap_dmic_exit);

MODULE_AUTHOR("Liam Girdwood <lrg@slimlogic.co.uk>");
MODULE_DESCRIPTION("OMAP DMIC SoC Interface");
MODULE_LICENSE("GPL");
