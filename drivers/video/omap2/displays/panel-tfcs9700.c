/*
 * Copyright (C) 2013 Texas Instruments Inc
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/of_i2c.h>
#include <linux/of.h>
#include <linux/regmap.h>

#include <video/omapdss.h>
#include <video/omap-panel-tfcs9700.h>

#define TLC_NAME		"tlc59108"
#define TLC_I2C_ADDR		0x40

#define TLC59108_MODE1		0x00
#define TLC59108_PWM2		0x04
#define TLC59108_LEDOUT0	0x0c
#define TLC59108_LEDOUT1	0x0d

struct tlc_data {
	struct	device *dev;
	struct  regmap *regmap;
	int p_gpio;
};

static const struct omap_video_timings tfc_s9700_default_timings = {
	.x_res		= 800,
	.y_res		= 480,

	.pixel_clock	= 29232,

	.hfp		= 41,
	.hsw		= 49,
	.hbp		= 41,

	.vfp		= 13,
	.vsw		= 4,
	.vbp		= 29,

	.vsync_level	= OMAPDSS_SIG_ACTIVE_LOW,
	.hsync_level	= OMAPDSS_SIG_ACTIVE_LOW,
	.data_pclk_edge	= OMAPDSS_DRIVE_SIG_RISING_EDGE,
	.de_level	= OMAPDSS_SIG_ACTIVE_HIGH,
	.sync_pclk_edge	= OMAPDSS_DRIVE_SIG_OPPOSITE_EDGES,
};

struct panel_drv_data {
	struct omap_dss_device *dssdev;

	struct i2c_client *tlc_client;
	struct mutex lock;

	int dith;
};

static int tlc_init(struct i2c_client *client)
{
	struct tlc_data *data = dev_get_drvdata(&client->dev);
	struct regmap *map = data->regmap;

	/* init the TLC chip */
	regmap_write(map, TLC59108_MODE1, 0x01);

	/*
	 * set LED1(AVDD) to ON state(default), enable LED2 in PWM mode, enable
	 * LED0 to OFF state
	 */
	regmap_write(map, TLC59108_LEDOUT0, 0x21);

	/* set LED2 PWM to full freq */
	regmap_write(map, TLC59108_PWM2, 0xff);

	/* set LED4(UPDN) and LED6(MODE3) to OFF state */
	regmap_write(map, TLC59108_LEDOUT1, 0x11);

	return 0;
}

static int tlc_uninit(struct i2c_client *client)
{
	struct tlc_data *data = dev_get_drvdata(&client->dev);
	struct regmap *map = data->regmap;

	/* clear TLC chip regs */
	regmap_write(map, TLC59108_PWM2, 0x0);
	regmap_write(map, TLC59108_LEDOUT0, 0x0);
	regmap_write(map, TLC59108_LEDOUT1, 0x0);

	regmap_write(map, TLC59108_MODE1, 0x0);

	return 0;
}

static int tfc_s9700_power_on(struct omap_dss_device *dssdev)
{
	struct panel_drv_data *ddata = dev_get_drvdata(&dssdev->dev);
	int r;

	if (dssdev->state == OMAP_DSS_DISPLAY_ACTIVE)
		return 0;

	omapdss_dpi_set_timings(dssdev, &dssdev->panel.timings);
	omapdss_dpi_set_data_lines(dssdev, dssdev->phy.dpi.data_lines);

	r = omapdss_dpi_display_enable(dssdev);
	if (r)
		goto err0;

	tlc_init(ddata->tlc_client);

	return 0;
err0:
	return r;
}

static void tfc_s9700_power_off(struct omap_dss_device *dssdev)
{
	struct panel_drv_data *ddata = dev_get_drvdata(&dssdev->dev);

	if (dssdev->state != OMAP_DSS_DISPLAY_ACTIVE)
		return;

	tlc_uninit(ddata->tlc_client);

	omapdss_dpi_display_disable(dssdev);
}

static int tfc_s9700_probe_of(struct omap_dss_device *dssdev,
		struct panel_drv_data *ddata)
{
	struct device_node *node = dssdev->dev.of_node;
	struct device_node *tlc;
	struct i2c_client *client;
	int r, datalines;

	r = of_property_read_u32(node, "data-lines", &datalines);
	if (r) {
		dev_err(&dssdev->dev, "failed to parse datalines");
		return r;
	}

	tlc = of_parse_phandle(node, "tlc", 0);
	if (!tlc) {
		dev_err(&dssdev->dev, "could not find tlc device\n");
		return -EINVAL;
	}

	client = of_find_i2c_device_by_node(tlc);
	if (!client) {
		dev_err(&dssdev->dev, "failed to find tlc i2c client device\n");
		return -EINVAL;
	}

	ddata->tlc_client = client;

	dssdev->phy.dpi.data_lines = datalines;

	return 0;
}

static struct i2c_board_info tlc_i2c_board_info = {
	I2C_BOARD_INFO(TLC_NAME, TLC_I2C_ADDR),
};

static int tfc_s9700_probe(struct omap_dss_device *dssdev)
{
	struct panel_drv_data *ddata;
	int r;

	ddata = devm_kzalloc(&dssdev->dev, sizeof(*ddata), GFP_KERNEL);
	if (!ddata)
		return -ENOMEM;

	dssdev->panel.timings = tfc_s9700_default_timings;

	ddata->dssdev = dssdev;
	mutex_init(&ddata->lock);

	if (dssdev->dev.of_node) {
		r = tfc_s9700_probe_of(dssdev, ddata);
		if (r)
			return r;
	} else if (dssdev->data) {
		struct tfc_s9700_platform_data *pdata = dssdev->data;
		struct i2c_client *client;
		struct i2c_adapter *adapter;
		int tlc_adapter_id;

		dssdev->phy.dpi.data_lines = pdata->datalines;

		tlc_adapter_id = pdata->tlc_adapter_id;

		adapter = i2c_get_adapter(tlc_adapter_id);
		if (!adapter) {
			dev_err(&dssdev->dev, "can't get i2c adapter\n");
			return -ENODEV;
		}

		client = i2c_new_device(adapter, &tlc_i2c_board_info);
		if (!client) {
			dev_err(&dssdev->dev, "can't add i2c device\n");
			return -ENODEV;
		}

		ddata->tlc_client = client;
	} else {
		return -EINVAL;
	}


	if (dssdev->phy.dpi.data_lines == 24)
		ddata->dith = 0;
	else if (dssdev->phy.dpi.data_lines == 18)
		ddata->dith = 1;
	else
		return -EINVAL;

	dev_set_drvdata(&dssdev->dev, ddata);

	return 0;
}

static void __exit tfc_s9700_remove(struct omap_dss_device *dssdev)
{
	struct panel_drv_data *ddata = dev_get_drvdata(&dssdev->dev);

	mutex_lock(&ddata->lock);

	dev_set_drvdata(&dssdev->dev, NULL);

	mutex_unlock(&ddata->lock);
}

static int tfc_s9700_enable(struct omap_dss_device *dssdev)
{
	struct panel_drv_data *ddata = dev_get_drvdata(&dssdev->dev);
	int r;

	mutex_lock(&ddata->lock);

	r = tfc_s9700_power_on(dssdev);
	if (r == 0)
		dssdev->state = OMAP_DSS_DISPLAY_ACTIVE;

	mutex_unlock(&ddata->lock);

	return r;
}

static void tfc_s9700_disable(struct omap_dss_device *dssdev)
{
	struct panel_drv_data *ddata = dev_get_drvdata(&dssdev->dev);

	mutex_lock(&ddata->lock);

	tfc_s9700_power_off(dssdev);

	dssdev->state = OMAP_DSS_DISPLAY_DISABLED;

	mutex_unlock(&ddata->lock);
}

static void tfc_s9700_set_timings(struct omap_dss_device *dssdev,
		struct omap_video_timings *timings)
{
	struct panel_drv_data *ddata = dev_get_drvdata(&dssdev->dev);

	mutex_lock(&ddata->lock);
	omapdss_dpi_set_timings(dssdev, timings);
	dssdev->panel.timings = *timings;
	mutex_unlock(&ddata->lock);
}

static void tfc_s9700_get_timings(struct omap_dss_device *dssdev,
		struct omap_video_timings *timings)
{
	struct panel_drv_data *ddata = dev_get_drvdata(&dssdev->dev);

	mutex_lock(&ddata->lock);
	*timings = dssdev->panel.timings;
	mutex_unlock(&ddata->lock);
}

static int tfc_s9700_check_timings(struct omap_dss_device *dssdev,
		struct omap_video_timings *timings)
{
	struct panel_drv_data *ddata = dev_get_drvdata(&dssdev->dev);
	int r;

	mutex_lock(&ddata->lock);
	r = dpi_check_timings(dssdev, timings);
	mutex_unlock(&ddata->lock);

	return r;
}

#if defined(CONFIG_OF)
static const struct of_device_id tfc_s9700_of_match[] = {
	{
		.compatible = "ti,tfc_s9700",
	},
	{},
};

MODULE_DEVICE_TABLE(of, tfc_s9700_of_match);
#else
#define dss_of_match NULL
#endif

static struct omap_dss_driver tfc_s9700_driver = {
	.probe		= tfc_s9700_probe,
	.remove		= __exit_p(tfc_s9700_remove),

	.enable		= tfc_s9700_enable,
	.disable	= tfc_s9700_disable,

	.set_timings	= tfc_s9700_set_timings,
	.get_timings	= tfc_s9700_get_timings,
	.check_timings	= tfc_s9700_check_timings,

	.driver         = {
		.name   = "tfc_s9700",
		.owner  = THIS_MODULE,
		.of_match_table = tfc_s9700_of_match,
	},
};

struct regmap_config tlc59108_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,
};

static int tlc59108_i2c_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	int r;
	struct regmap *regmap;
	struct tlc_data *data;
	struct device *dev = &client->dev;
	struct device_node *node = dev->of_node;
	unsigned int val;
	int gpio;

	regmap = devm_regmap_init_i2c(client, &tlc59108_regmap_config);
	if (IS_ERR(regmap)) {
		r = PTR_ERR(regmap);
		dev_err(&client->dev, "Failed to init regmap: %d\n", r);
		return r;
	}

	data = devm_kzalloc(dev, sizeof(struct tlc_data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	gpio = of_get_gpio(node, 0);

	if (gpio_is_valid(gpio)) {
		data->p_gpio = gpio;
	} else if (gpio == -ENOENT) {
		data->p_gpio = -1;
	} else {
		dev_err(dev, "failed to parse Power gpio\n");
		return gpio;
	}

	if (gpio_is_valid(data->p_gpio)) {
		r = devm_gpio_request_one(dev, data->p_gpio, GPIOF_OUT_INIT_LOW,
				"tfc_s9700 p_gpio");
		if (r) {
			dev_err(dev, "Failed to request Power GPIO %d\n",
				data->p_gpio);
			return r;
		}
	}

	dev_set_drvdata(dev, data);
	data->dev = dev;
	data->regmap = regmap;

	msleep(10);

	/* Try to read a TLC register to verify if i2c works */
	r = regmap_read(data->regmap, TLC59108_MODE1, &val);
	if (r < 0) {
		dev_err(dev, "Failed to set MODE1: %d\n", r);
		return r;
	}

	dev_info(dev, "Successfully initialized %s\n", TLC_NAME);

	return 0;
}

static int tlc59108_i2c_remove(struct i2c_client *client)
{
	struct tlc_data *data = dev_get_drvdata(&client->dev);

	if (gpio_is_valid(data->p_gpio))
		gpio_set_value_cansleep(data->p_gpio, 1);

	return 0;
}

static const struct i2c_device_id tlc59108_id[] = {
	{ TLC_NAME, 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, tlc59108_id);

static const struct of_device_id tlc59108_of_match[] = {
	{ .compatible = "ti,tlc59108", },
	{ },
};
MODULE_DEVICE_TABLE(of, tlc59108_of_match);

static struct i2c_driver tlc59108_i2c_driver = {
	.driver = {
		.owner	= THIS_MODULE,
		.name	= TLC_NAME,
		.of_match_table = tlc59108_of_match,
	},
	.id_table	= tlc59108_id,
	.probe		= tlc59108_i2c_probe,
	.remove		= tlc59108_i2c_remove,
};

static int __init tfc_s9700_init(void)
{
	int r;

	r = i2c_add_driver(&tlc59108_i2c_driver);
	if (r) {
		printk(KERN_WARNING "tlc59108 driver" \
			" registration failed\n");
		return r;
	}

	r = omap_dss_register_driver(&tfc_s9700_driver);
	if (r)
		i2c_del_driver(&tlc59108_i2c_driver);

	return r;
}

static void __exit tfc_s9700_exit(void)
{
	omap_dss_unregister_driver(&tfc_s9700_driver);
}

module_init(tfc_s9700_init);
module_exit(tfc_s9700_exit);
MODULE_LICENSE("GPL");
