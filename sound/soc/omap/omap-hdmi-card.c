/*
 * omap-hdmi-card.c
 *
 * OMAP ALSA SoC machine driver for TI OMAP HDMI
 * Copyright (C) 2011 Texas Instruments Incorporated - http://www.ti.com/
 * Author: Ricardo Neri <ricardo.neri@ti.com>
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

#include <linux/module.h>
#include <linux/of.h>
#include <sound/pcm.h>
#include <sound/soc.h>
#include <asm/mach-types.h>
#include <video/omapdss.h>

#define DRV_NAME "omap-hdmi-audio-card"

struct hdmi_card_data {
	struct platform_device *codec_pdev;
};

static struct snd_soc_dai_link omap_hdmi_dai = {
	.name = "HDMI",
	.stream_name = "HDMI",
	.cpu_dai_name = "omap-hdmi-audio",
	.platform_name = "omap-pcm-audio",
	.codec_name = "hdmi-audio-codec",
	.codec_dai_name = "omap-hdmi-hifi",
};

static struct snd_soc_card snd_soc_omap_hdmi = {
	.name = "OMAPHDMI",
	.owner = THIS_MODULE,
	.dai_link = &omap_hdmi_dai,
	.num_links = 1,
};

static int omap_hdmi_probe(struct platform_device *pdev)
{
	struct snd_soc_card *card = &snd_soc_omap_hdmi;
	struct device_node *node = pdev->dev.of_node;
	struct hdmi_card_data *card_data;
	int ret;

	card->dev = &pdev->dev;

	card_data = devm_kzalloc(&pdev->dev, sizeof(*card_data), GFP_KERNEL);
	if (!card_data)
		return -ENOMEM;

	card_data->codec_pdev = ERR_PTR(-EINVAL);

	/* for DT boot */
	if (node) {
		struct device_node *dev_node;

		if (snd_soc_of_parse_card_name(card, "ti,model")) {
			dev_err(&pdev->dev, "Card name is not provided\n");
			return -ENODEV;
		}

		dev_node = of_parse_phandle(node, "ti,hdmi_audio", 0);
		if (!dev_node) {
			dev_err(&pdev->dev, "hdmi node is not provided\n");
			return -EINVAL;
		}

		dev_node = of_parse_phandle(node, "ti,level_shifter", 0);
		if (!dev_node) {
			dev_err(&pdev->dev, "level shifter node is not provided\n");
			return -EINVAL;
		}
		card_data->codec_pdev = platform_device_register_simple("hdmi-audio-codec",
							    -1, NULL, 0);
		if (IS_ERR(card_data->codec_pdev)) {
			dev_err(&pdev->dev,
				"Cannot instantiate hdmi-audio-codec\n");
			return PTR_ERR(card_data->codec_pdev);
		}
	}

	snd_soc_card_set_drvdata(card, card_data);
	ret = snd_soc_register_card(card);
	if (ret) {
		dev_err(&pdev->dev, "snd_soc_register_card failed (%d)\n", ret);
		card->dev = NULL;
		goto err_register_card;
	}

	return 0;

err_register_card:
	if (!IS_ERR(card_data->codec_pdev))
		platform_device_unregister(card_data->codec_pdev);
	return ret;
}

static int omap_hdmi_remove(struct platform_device *pdev)
{
	struct snd_soc_card *card = platform_get_drvdata(pdev);
	struct hdmi_card_data *card_data;

	card_data = snd_soc_card_get_drvdata(card);
	if (!IS_ERR(card_data->codec_pdev))
		platform_device_unregister(card_data->codec_pdev);
	snd_soc_unregister_card(card);
	card->dev = NULL;
	return 0;
}

static const struct of_device_id omap_hdmi_of_match[] = {
	{.compatible = "ti,omap-hdmi-tpd12s015-audio", },
	{ },
};
MODULE_DEVICE_TABLE(of, omap_hdmi_of_match);

static struct platform_driver omap_hdmi_driver = {
	.driver = {
		.name = DRV_NAME,
		.owner = THIS_MODULE,
		.of_match_table = omap_hdmi_of_match,
	},
	.probe = omap_hdmi_probe,
	.remove = omap_hdmi_remove,
};

module_platform_driver(omap_hdmi_driver);

MODULE_AUTHOR("Ricardo Neri <ricardo.neri@ti.com>");
MODULE_DESCRIPTION("OMAP HDMI machine ASoC driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:" DRV_NAME);
