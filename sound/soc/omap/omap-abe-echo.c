/*
 * omap-echo.c  --  OMAP ASoC ECHO DAI driver
 *
 * Copyright (C) 2012 Texas Instruments
 *
 * Author: Misael Lopez Cruz <misael.lopez@ti.com>
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

#include <linux/module.h>
#include <linux/platform_device.h>

#include <sound/soc.h>

static struct snd_soc_dai_driver omap_echo_dai[] = {
{
	.name = "omap-abe-echo-dai",
	.capture = {
		.channels_min = 1,
		.channels_max = 2,
		.rates = SNDRV_PCM_RATE_48000,
		.formats = SNDRV_PCM_FMTBIT_S32_LE,
	},
},
};

static __devinit int asoc_echo_probe(struct platform_device *pdev)
{
	return snd_soc_register_dais(&pdev->dev, omap_echo_dai,
				     ARRAY_SIZE(omap_echo_dai));
}

static int __devexit asoc_echo_remove(struct platform_device *pdev)
{
	snd_soc_unregister_dai(&pdev->dev);
	return 0;
}

static struct platform_driver asoc_echo_driver = {
	.driver = {
		.name = "omap-abe-echo-dai",
		.owner = THIS_MODULE,
	},
	.probe = asoc_echo_probe,
	.remove = __devexit_p(asoc_echo_remove),
};

static int __init snd_omap_echo_init(void)
{
	return platform_driver_register(&asoc_echo_driver);
}
module_init(snd_omap_echo_init);

static void __exit snd_omap_echo_exit(void)
{
	platform_driver_unregister(&asoc_echo_driver);
}
module_exit(snd_omap_echo_exit);

MODULE_ALIAS("platform:omap-abe-echo");
MODULE_AUTHOR("Misael Lopez Cruz <misael.lopez@ti.com>");
MODULE_DESCRIPTION("OMAP Echo SoC Interface");
MODULE_LICENSE("GPL");
