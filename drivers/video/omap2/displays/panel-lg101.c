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
#include <linux/i2c/lvds-serlink.h>
#include <linux/i2c/lvds-de-serlink.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>

#include <video/omapdss.h>
#include <video/omap-panel-lg101.h>

static const struct omap_video_timings lg101_default_timings = {
	/* 1280 x 800 @ 60 Hz Reduced blanking VESA CVT 0.31M3-R */
	.x_res		= 1280,
	.y_res		= 800,

	.pixel_clock	= 67333,

	.hfp		= 32,
	.hsw		= 48,
	.hbp		= 80,

	.vfp		= 4,
	.vsw		= 3,
	.vbp		= 7,

	.vsync_level	= OMAPDSS_SIG_ACTIVE_LOW,
	.hsync_level	= OMAPDSS_SIG_ACTIVE_LOW,
	.data_pclk_edge = OMAPDSS_DRIVE_SIG_RISING_EDGE,
	.de_level	= OMAPDSS_SIG_ACTIVE_HIGH,
	.sync_pclk_edge = OMAPDSS_DRIVE_SIG_OPPOSITE_EDGES,
};

struct panel_drv_data {
	struct omap_dss_device *dssdev;
	struct mutex lock;
	int p_gpio;
	int dith;
	struct i2c_client *ser_i2c_client;
	struct i2c_client *deser_i2c_client;
};

static int send_i2c_cmd(struct i2c_client *client, int cmd, void *arg)
{
	int status = 0;
	if (client && client->driver && client->driver->command)
		status = client->driver->command(client, cmd, arg);

	return status;
}
static int lg101_power_on(struct omap_dss_device *dssdev)
{
	struct panel_drv_data *ddata = dev_get_drvdata(&dssdev->dev);
	int r;
	u8 data;

	if (dssdev->state == OMAP_DSS_DISPLAY_ACTIVE)
		return 0;

	dev_dbg(&dssdev->dev, "lg101_power_on");

	gpio_set_value_cansleep(ddata->p_gpio, 0); /* S0-0 S1-1 S2-0 A1 -> B2*/

	omapdss_dpi_set_timings(dssdev, &dssdev->panel.timings);
	omapdss_dpi_set_data_lines(dssdev, dssdev->phy.dpi.data_lines);

	r = omapdss_dpi_display_enable(dssdev);
	if (r)
		goto err0;

	r = send_i2c_cmd(ddata->ser_i2c_client, SER_RESET, NULL);
	if (r < 0) {
		dev_err(&dssdev->dev, "failed to reset serializer ...");
		goto err0;
	}
	dev_err(&dssdev->dev, "Serializer Reset done...");

	r = send_i2c_cmd(ddata->ser_i2c_client, SER_CONFIGURE, NULL);
	if (r < 0) {
		dev_err(&dssdev->dev, "configure serializer failed ...");
		goto err0;
	}
	dev_err(&dssdev->dev, "Serializer configuration done...");

	r = send_i2c_cmd(ddata->ser_i2c_client, SER_VALIDATE_PCLK, NULL);
	if (r < 0) {
		dev_err(&dssdev->dev, "PCLK not present ...");
		goto err0;
	}
	dev_err(&dssdev->dev, "PCLK detection done...");

	r = send_i2c_cmd(ddata->ser_i2c_client, SER_GET_I2C_ADDR, NULL);
	if ((r < 0) || (r != ddata->ser_i2c_client->addr)) {
		dev_err(&dssdev->dev, "couldn't read i2c serializer address ...");
		goto err0;
	}
	dev_err(&dssdev->dev, "Serializer i2c addr %x ...", r);

	r = send_i2c_cmd(ddata->ser_i2c_client, SER_VALIDATE_LINK, NULL);
	if (r != 1) {
		dev_err(&dssdev->dev, "link not preset ...");
		r = -ENODEV;
		goto err0;
	}
	dev_err(&dssdev->dev, "Link detected ...");

	r = send_i2c_cmd(ddata->ser_i2c_client, SER_GET_DES_I2C_ADDR,
								(void *)&data);
	if ((r < 0) || (data != ddata->deser_i2c_client->addr)) {
		dev_err(&dssdev->dev, "couldn't read i2c deserializer address ...");
		goto err0;
	}
	dev_err(&dssdev->dev, "Deserilizer i2c addr %x", data);

	r = send_i2c_cmd(ddata->ser_i2c_client, SER_REG_DUMP,
							NULL);
	if (r < 0) {
		dev_err(&dssdev->dev, "couldn't read serializer sts ...");
		goto err0;
	}
	dev_err(&dssdev->dev, "Serializer status dump");

	/* --------- Deserializer -------------- */
	r = send_i2c_cmd(ddata->deser_i2c_client, DSER_RESET, NULL);
	if (r < 0) {
		dev_err(&dssdev->dev, "failed to reset de serializer ...");
		goto err0;
	}
	dev_err(&dssdev->dev, "DeSerializer Reset done...");

	r = send_i2c_cmd(ddata->deser_i2c_client, DSER_EN_BC, NULL);
	if (r < 0) {
		dev_err(&dssdev->dev, "failed to enable back channel ...");
		goto err0;
	}
	dev_err(&dssdev->dev, "DeSerializer back channel enabled ...");

	r = send_i2c_cmd(ddata->deser_i2c_client, DSER_GET_I2C_ADDR, NULL);
	if ((r < 0) || (r != ddata->deser_i2c_client->addr)) {
		dev_err(&dssdev->dev, "couldn't read i2c de serializer address ...");
		goto err0;
	}
	dev_err(&dssdev->dev, "Deserializer i2c addr %x ...", r);

	r = send_i2c_cmd(ddata->deser_i2c_client, DSER_GET_SER_I2C_ADDR,
								(void *)&data);
	if ((r < 0) || (data != ddata->ser_i2c_client->addr)) {
		dev_err(&dssdev->dev, "failed to get serializer id ...");
		goto err0;
	}
	dev_err(&dssdev->dev, "Serilizer i2c addr %x\n", data);

	r = send_i2c_cmd(ddata->deser_i2c_client, DSER_CONFIGURE,
								NULL);
	if (r < 0) {
		dev_err(&dssdev->dev, "failed to configure deserializer...");
		goto err0;
	}
	dev_err(&dssdev->dev, "Deserilizer configuration done ...\n");

	r = send_i2c_cmd(ddata->deser_i2c_client, DSER_REG_DUMP,
							NULL);
	if (r < 0) {
		dev_err(&dssdev->dev, "couldn't read deserializer sts ...");
		goto err0;
	}
	dev_err(&dssdev->dev, "Deserializer status dump");

	return 0;
err0:
	return r;
}

static void lg101_power_off(struct omap_dss_device *dssdev)
{
	struct panel_drv_data *ddata = dev_get_drvdata(&dssdev->dev);

	if (dssdev->state != OMAP_DSS_DISPLAY_ACTIVE)
		return;
	dev_dbg(&dssdev->dev, "lg101_power_off");

	gpio_set_value_cansleep(ddata->p_gpio, 1); /* S0-1 S1-0 S2-0 A1 -> B1*/

	omapdss_dpi_display_disable(dssdev);
}

static int lg101_probe_of(struct omap_dss_device *dssdev,
		struct panel_drv_data *ddata)
{
	struct device_node *node = dssdev->dev.of_node;
	int r, datalines, gpio, gpio_count;

	gpio_count = of_gpio_count(node);
	/* DRA-7 board has 1 selection gpio */
	if (gpio_count != 1) {
		dev_err(&dssdev->dev, "wrong number of GPIOs %d\n", gpio_count);
		return -EINVAL;
	}
	dev_dbg(&dssdev->dev, "gpio count %d\n", gpio_count);

	/* Get the GPIO for GPMC vs VID3 selection */
	gpio = of_get_gpio(node, 0);
	if (gpio_is_valid(gpio)) {
		ddata->p_gpio = gpio;
	} else {
		dev_err(&dssdev->dev, "fail to parse vid3 gpio %d\n", gpio);
		return -EINVAL;
	}
	dev_dbg(&dssdev->dev, "gpio number %d\n", gpio);

	r = of_property_read_u32(node, "data-lines", &datalines);
	if (r) {
		dev_err(&dssdev->dev, "failed to parse datalines");
		return r;
	}
	dssdev->phy.dpi.data_lines = datalines;

	/* make sure the child nodes are probed */
	r = of_platform_populate(dssdev->dev.of_node, NULL, NULL, &dssdev->dev);
	if (r < 0) {
		dev_err(&dssdev->dev, "failed to populate child devices");
		return r;
	}
	dev_err(&dssdev->dev, "child device populate");

	/* Get I2c node of serializer */
	node = of_parse_phandle(dssdev->dev.of_node, "serializer", 0);
	if (node)
		ddata->ser_i2c_client = of_find_i2c_device_by_node(node);
	else
		return -EINVAL;
	dev_err(&dssdev->dev, "Serial I2C ID %x", ddata->ser_i2c_client->addr);

	/* Get I2c node of dserializer */
	node = of_parse_phandle(dssdev->dev.of_node, "dserializer", 0);
	if (node)
		ddata->deser_i2c_client = of_find_i2c_device_by_node(node);
	else
		return -EINVAL;
	dev_err(&dssdev->dev, "DeSerial I2C ID %x",
					ddata->deser_i2c_client->addr);

	return 0;
}

static int lg101_probe(struct omap_dss_device *dssdev)
{
	struct panel_drv_data *ddata;
	int r;

	dev_err(&dssdev->dev, "lg101_probe probe\n");

	ddata = devm_kzalloc(&dssdev->dev, sizeof(*ddata), GFP_KERNEL);
	if (!ddata)
		return -ENOMEM;

	dssdev->panel.timings = lg101_default_timings;

	ddata->dssdev = dssdev;
	mutex_init(&ddata->lock);

	if (dssdev->dev.of_node) {
		r = lg101_probe_of(dssdev, ddata);
		if (r)
			return r;
	} else if (dssdev->data) {
		struct lg101_platform_data *pdata = dssdev->data;
		dssdev->phy.dpi.data_lines = pdata->datalines;
		ddata->p_gpio = pdata->p_gpio;
	} else {
		return -EINVAL;
	}

	if (dssdev->phy.dpi.data_lines == 24)
		ddata->dith = 0;
	else if (dssdev->phy.dpi.data_lines == 18)
		ddata->dith = 1;
	else
		return -EINVAL;

	r = gpio_request_one(ddata->p_gpio, GPIOF_OUT_INIT_HIGH, "vid3_sel");
	if (r)
		return r;

	dev_set_drvdata(&dssdev->dev, ddata);

	dev_err(&dssdev->dev, "lg101_probe probe sucessful..\n");

	return 0;
}

static void __exit lg101_remove(struct omap_dss_device *dssdev)
{
	struct panel_drv_data *ddata = dev_get_drvdata(&dssdev->dev);

	mutex_lock(&ddata->lock);

	gpio_free(ddata->p_gpio);

	dev_set_drvdata(&dssdev->dev, NULL);

	mutex_unlock(&ddata->lock);

	kfree(ddata);
}

static int lg101_enable(struct omap_dss_device *dssdev)
{
	struct panel_drv_data *ddata = dev_get_drvdata(&dssdev->dev);
	int r;

	mutex_lock(&ddata->lock);

	r = lg101_power_on(dssdev);
	if (r == 0)
		dssdev->state = OMAP_DSS_DISPLAY_ACTIVE;

	mutex_unlock(&ddata->lock);

	return r;
}

static void lg101_disable(struct omap_dss_device *dssdev)
{
	struct panel_drv_data *ddata = dev_get_drvdata(&dssdev->dev);

	mutex_lock(&ddata->lock);

	lg101_power_off(dssdev);

	dssdev->state = OMAP_DSS_DISPLAY_DISABLED;

	mutex_unlock(&ddata->lock);
}

static void lg101_set_timings(struct omap_dss_device *dssdev,
		struct omap_video_timings *timings)
{
	struct panel_drv_data *ddata = dev_get_drvdata(&dssdev->dev);

	mutex_lock(&ddata->lock);
	omapdss_dpi_set_timings(dssdev, timings);
	dssdev->panel.timings = *timings;
	mutex_unlock(&ddata->lock);
}

static void lg101_get_timings(struct omap_dss_device *dssdev,
		struct omap_video_timings *timings)
{
	struct panel_drv_data *ddata = dev_get_drvdata(&dssdev->dev);

	mutex_lock(&ddata->lock);
	*timings = dssdev->panel.timings;
	mutex_unlock(&ddata->lock);
}

static int lg101_check_timings(struct omap_dss_device *dssdev,
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
static const struct of_device_id lg101_of_match[] = {
	{
		.compatible = "ti,lg101",
	},
	{},
};

MODULE_DEVICE_TABLE(of, lg101_of_match);
#else
#define lg101_of_match NULL
#endif

static struct omap_dss_driver lg101_driver = {
	.probe		= lg101_probe,
	.remove		= __exit_p(lg101_remove),

	.enable		= lg101_enable,
	.disable	= lg101_disable,

	.set_timings	= lg101_set_timings,
	.get_timings	= lg101_get_timings,
	.check_timings	= lg101_check_timings,

	.driver         = {
		.name   = "lg101",
		.owner  = THIS_MODULE,
		.of_match_table = lg101_of_match,
	},
};

static int __init lg101_init(void)
{
	return omap_dss_register_driver(&lg101_driver);
}

static void __exit lg101_exit(void)
{
	omap_dss_unregister_driver(&lg101_driver);
}

module_init(lg101_init);
module_exit(lg101_exit);
MODULE_LICENSE("GPL");
