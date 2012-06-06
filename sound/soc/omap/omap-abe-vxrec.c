/*
 * omap-vxrec.c  --  OMAP ASoC VXREC DAI driver
 *
 * Copyright (C) 2011 Texas Instruments
 *
 * Author: Francois Mazard <f-mazard@ti.com>
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

static struct snd_soc_dai_driver omap_vxrec_dai[] = {
{
	.name = "omap-abe-vxrec-dai",
	.capture = {
		.channels_min = 1,
		.channels_max = 2,
		.rates = SNDRV_PCM_RATE_48000,
		.formats = SNDRV_PCM_FMTBIT_S32_LE,
	},
},
};

static __devinit int asoc_vxrec_probe(struct platform_device *pdev)
{
	int ret;

	ret = snd_soc_register_dais(&pdev->dev, omap_vxrec_dai,
				    ARRAY_SIZE(omap_vxrec_dai));

	return ret;
}

static int __devexit asoc_vxrec_remove(struct platform_device *pdev)
{
	snd_soc_unregister_dai(&pdev->dev);

	return 0;
}

static struct platform_driver asoc_vxrec_driver = {
	.driver = {
		.name = "omap-abe-vxrec-dai",
		.owner = THIS_MODULE,
	},
	.probe = asoc_vxrec_probe,
	.remove = __devexit_p(asoc_vxrec_remove),
};

static int __init snd_omap_vxrec_init(void)
{
	return platform_driver_register(&asoc_vxrec_driver);
}
module_init(snd_omap_vxrec_init);

static void __exit snd_omap_vxrec_exit(void)
{
	platform_driver_unregister(&asoc_vxrec_driver);
}
module_exit(snd_omap_vxrec_exit);

MODULE_AUTHOR("Francois Mazard <f-mazard@ti.com>");
MODULE_DESCRIPTION("OMAP VxREC SoC Interface");
MODULE_LICENSE("GPL");
