/*
 * MFD driver for twl6040 codec submodule
 *
 * Author:      Jorge Eduardo Candelaria <jorge.candelaria@ti.com>
 *
 * Copyright:   (C) 20010 Texas Instruments, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
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
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/i2c/twl.h>
#include <linux/mfd/core.h>
#include <linux/mfd/twl6040-codec.h>

#define TWL6040_CODEC_CELLS	2

static struct platform_device *twl6040_codec_dev;

struct twl6040_codec_resource {
	int request_count;
	u8 reg;
	u8 mask;
};

struct twl6040_codec {
	unsigned int audio_mclk;
	struct mutex mutex;
	struct twl6040_codec_resource resource[TWL6040_CODEC_RES_MAX];
	struct mfd_cell cells[TWL6040_CODEC_CELLS];
};

static int twl6040_codec_set_resource(enum twl6040_codec_res id, int enable)
{
	struct twl6040_codec *codec = platform_get_drvdata(twl6040_codec_dev);
	u8 val = 0;

	twl_i2c_read_u8(TWL4030_MODULE_AUDIO_VOICE, &val,
			codec->resource[id].reg);

	if (enable)
		val |= codec->resource[id].mask;
	else
		val &= ~codec->resource[id].mask;

	twl_i2c_write_u8(TWL4030_MODULE_AUDIO_VOICE,
				val, codec->resource[id].reg);

	return val;
}

static inline int twl6040_codec_get_resource(enum twl6040_codec_res id)
{
	struct twl6040_codec *codec = platform_get_drvdata(twl6040_codec_dev);
	u8 val = 0;

	twl_i2c_read_u8(TWL4030_MODULE_AUDIO_VOICE, &val,
			codec->resource[id].reg);

	return val;
}

int twl6040_codec_enable_resource(enum twl6040_codec_res id)
{
	struct twl6040_codec *codec = platform_get_drvdata(twl6040_codec_dev);
	int val;

	if (id >= TWL6040_CODEC_RES_MAX) {
		dev_err(&twl6040_codec_dev->dev,
				"Invalid resource ID (%u)\n", id);
		return -EINVAL;
	}

	mutex_lock(&codec->mutex);
	if (!codec->resource[id].request_count)
		/* Resource was disabled, enable it */
		val = twl6040_codec_set_resource(id, 1);
	else
		val = twl6040_codec_get_resource(id);

	codec->resource[id].request_count++;
	mutex_unlock(&codec->mutex);

	return val;
}
EXPORT_SYMBOL_GPL(twl6040_codec_enable_resource);

int twl6040_codec_disable_resource(unsigned id)
{
	struct twl6040_codec *codec = platform_get_drvdata(twl6040_codec_dev);
	int val;

	if (id >= TWL6040_CODEC_RES_MAX) {
		dev_err(&twl6040_codec_dev->dev,
				"Invalid resource ID (%u)\n", id);
		return -EINVAL;
	}

	mutex_lock(&codec->mutex);
	if (!codec->resource[id].request_count) {
		dev_err(&twl6040_codec_dev->dev,
			"Resource has been disabled already (%u)\n", id);
		mutex_unlock(&codec->mutex);
		return -EPERM;
	}
	codec->resource[id].request_count--;

	if (!codec->resource[id].request_count)
		/* Resource can be disabled now */
		val = twl6040_codec_set_resource(id, 0);
	else
		val = twl6040_codec_get_resource(id);

	mutex_unlock(&codec->mutex);

	return val;
}
EXPORT_SYMBOL_GPL(twl6040_codec_disable_resource);

static int __devinit twl6040_codec_probe(struct platform_device *pdev)
{
	struct twl6040_codec *codec;
	struct twl4030_codec_data *pdata = pdev->dev.platform_data;
	struct mfd_cell *cell = NULL;
	int ret, childs = 0;

	if(!pdata) {
		dev_err(&pdev->dev, "Platform data is missing\n");
		return -EINVAL;
	}

	codec = kzalloc(sizeof(struct twl6040_codec), GFP_KERNEL);
	if (!codec)
		return -ENOMEM;

	platform_set_drvdata(pdev, codec);

	twl6040_codec_dev = pdev;
	mutex_init(&codec->mutex);
	codec->audio_mclk = pdata->audio_mclk;

	if (pdata->audio) {
		cell = &codec->cells[childs];
		cell->name = "twl6040-codec";
		cell->platform_data = pdata->audio;
		cell->data_size = sizeof(*pdata->audio);
		childs++;
	}

	if (pdata->vibra) {
		cell = &codec->cells[childs];
		cell->name = "twl6040-vibra";
		cell->platform_data = pdata->vibra;
		cell->data_size = sizeof(*pdata->vibra);
		childs++;
	}

	if (childs) {
		ret = mfd_add_devices(&pdev->dev, pdev->id, codec->cells,
				      childs, NULL, 0);
	} else {
		dev_err(&pdev->dev, "No platform data found for childs\n");
		ret = -ENODEV;
	}

	if (!ret)
		return 0;

	platform_set_drvdata(pdev, NULL);
	kfree(codec);
	twl6040_codec_dev = NULL;
	return ret;
}

static int __devexit twl6040_codec_remove(struct platform_device *pdev)
{
	struct twl6040_codec *codec = platform_get_drvdata(pdev);

	mfd_remove_devices(&pdev->dev);
	platform_set_drvdata(pdev, NULL);
	kfree(codec);
	twl6040_codec_dev = NULL;

	return 0;
}

MODULE_ALIAS("platform:twl6040_audio");

static struct platform_driver twl6040_codec_driver = {
	.probe		= twl6040_codec_probe,
	.remove		= __devexit_p(twl6040_codec_remove),
	.driver		= {
		.owner	= THIS_MODULE,
		.name	= "twl6040_audio",
	},
};

static int __devinit twl6040_codec_init(void)
{
	return platform_driver_register(&twl6040_codec_driver);
}
module_init(twl6040_codec_init);

static void __devexit twl6040_codec_exit(void)
{
	platform_driver_unregister(&twl6040_codec_driver);
}

module_exit(twl6040_codec_exit);

MODULE_AUTHOR("Jorge Eduardo Candelari <jorge.candelaria@ti.com>");
MODULE_LICENSE("GPL");
