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

static struct platform_device *twl6040_codec_dev;

int twl6040_reg_read(struct twl6040_codec *twl6040, unsigned int reg)
{
	int ret;
	u8 val;

	mutex_lock(&twl6040->io_mutex);
	ret = twl_i2c_read_u8(TWL_MODULE_AUDIO_VOICE, &val, reg);
	if (ret < 0) {
		mutex_unlock(&twl6040->io_mutex);
		return ret;
	}
	mutex_unlock(&twl6040->io_mutex);

	return val;
}
EXPORT_SYMBOL(twl6040_reg_read);

int twl6040_reg_write(struct twl6040_codec *twl6040, unsigned int reg, u8 val)
{
	int ret;

	mutex_lock(&twl6040->io_mutex);
	ret = twl_i2c_write_u8(TWL_MODULE_AUDIO_VOICE, val, reg);
	mutex_unlock(&twl6040->io_mutex);

	return ret;
}
EXPORT_SYMBOL(twl6040_reg_write);

static int twl6040_codec_set_resource(enum twl6040_codec_res id, int enable)
{
	struct twl6040_codec *twl6040 = platform_get_drvdata(twl6040_codec_dev);
	u8 val = 0;

	val = twl6040_reg_read(twl6040, twl6040->resource[id].reg);

	if (enable)
		val |= twl6040->resource[id].mask;
	else
		val &= ~twl6040->resource[id].mask;

	twl6040_reg_write(twl6040, twl6040->resource[id].reg, val);

	return val;
}

static inline int twl6040_codec_get_resource(enum twl6040_codec_res id)
{
	struct twl6040_codec *twl6040 = platform_get_drvdata(twl6040_codec_dev);

	return twl6040_reg_read(twl6040, twl6040->resource[id].reg);
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
	struct twl6040_codec *twl6040;
	struct twl4030_codec_data *pdata = pdev->dev.platform_data;
	struct mfd_cell *cell = NULL;
	int ret, children = 0;

	if(!pdata) {
		dev_err(&pdev->dev, "Platform data is missing\n");
		return -EINVAL;
	}

	twl6040 = kzalloc(sizeof(struct twl6040_codec), GFP_KERNEL);
	if (!twl6040)
		return -ENOMEM;

	platform_set_drvdata(pdev, twl6040);

	twl6040_codec_dev = pdev;
	mutex_init(&twl6040->mutex);
	mutex_init(&twl6040->io_mutex);
	twl6040->audio_mclk = pdata->audio_mclk;

	if (pdata->audio) {
		cell = &twl6040->cells[children];
		cell->name = "twl6040-codec";
		cell->platform_data = pdata->audio;
		cell->data_size = sizeof(*pdata->audio);
		children++;
	}

	if (pdata->vibra) {
		cell = &twl6040->cells[children];
		cell->name = "vib-twl6040";
		cell->platform_data = pdata->vibra;
		cell->data_size = sizeof(*pdata->vibra);
		children++;
	}

	if (children) {
		ret = mfd_add_devices(&pdev->dev, pdev->id, twl6040->cells,
				      children, NULL, 0);
	} else {
		dev_err(&pdev->dev, "No platform data found for children\n");
		ret = -ENODEV;
	}

	if (!ret)
		return 0;

	platform_set_drvdata(pdev, NULL);
	kfree(twl6040);
	twl6040_codec_dev = NULL;
	return ret;
}

static int __devexit twl6040_codec_remove(struct platform_device *pdev)
{
	struct twl6040_codec *twl6040 = platform_get_drvdata(pdev);

	mfd_remove_devices(&pdev->dev);
	platform_set_drvdata(pdev, NULL);
	kfree(twl6040);
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
