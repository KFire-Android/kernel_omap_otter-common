/*
 * LG-4591 panel driver
 *
 * Copyright 2011 Texas Instruments, Inc.
 * Author: Archit Taneja <archit@ti.com>
 * Author: Tomi Valkeinen <tomi.valkeinen@ti.com>
 *
 * based on d2l panel driver by Jerry Alexander <x0135174@ti.com>
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
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/jiffies.h>
#include <linux/sched.h>
#include <linux/seq_file.h>
#include <linux/backlight.h>
#include <linux/fb.h>
#include <linux/interrupt.h>
#include <linux/gpio.h>
#include <linux/slab.h>
#include <linux/regulator/consumer.h>
#include <linux/mutex.h>
#include <linux/i2c.h>
#include <linux/of_gpio.h>

#include <video/omapdss.h>
#include <video/mipi_display.h>

#define DISPLAY_CTRL1	0xb5
#define DISPLAY_CTRL2	0xb6
#define OSCSET		0xc0
#define PWRCTL1		0xc1
#define PWRCTL2		0xc2
#define PWRCTL3		0xc3
#define PWRCTL4		0xc4
#define RGAMMAP		0xd0
#define RGAMMAN		0xd1
#define GGAMMAP		0xd2
#define GGAMMAN		0xd3
#define BGAMMAP		0xd4
#define BGAMMAN		0xd5
#define DSI_CFG		0xe0
#define OTP2		0xf9

#define WRDISBV		0x51
#define RDDISBV		0x52
#define WRCTRLD		0x53
#define RDCTRLD		0x54

static struct omap_video_timings lg4591_timings = {
	.x_res		= 720,
	.y_res		= 1280,
	.pixel_clock	= 80000,
	.hfp		= 1,
	.hsw		= 1,
	.hbp		= 306,
	.vfp		= 1,
	.vsw		= 1,
	.vbp		= 15,
};

static struct omap_dss_dsi_videomode_timings vm_data = {
	.hsa			= 1,
	.hfp			= 1,
	.hbp			= 227,
	.vsa			= 1,
	.vfp			= 1,
	.vbp			= 15,

	.blanking_mode		= 0,
	.hsa_blanking_mode	= 1,
	.hfp_blanking_mode	= 1,
	.hbp_blanking_mode	= 1,

	.ddr_clk_always_on	= true,

	.window_sync		= 4,
};

struct lg4591_data {
	struct mutex lock;

	int	reset_gpio;
	struct omap_dsi_pin_config pin_config;

	struct omap_dss_device *dssdev;
	struct backlight_device *bldev;
	bool enabled;
	int bl;

	int config_channel;
	int pixel_channel;

	struct omap_video_timings *timings;
};

struct lg4591_reg {
	/* Address and register value */
	u8 data[10];
	int len;
};

static struct lg4591_reg init_seq1[] = {
	{ { DSI_CFG, 0x43, 0x00, 0x80, 0x00, 0x00, }, 6, },
	{ { DISPLAY_CTRL1, 0x29, 0x20, 0x40, 0x00, 0x00 }, 6, },
	{ { DISPLAY_CTRL2, 0x01, 0x14, 0x0f, 0x16, 0x13 }, 6, },
	{ { RGAMMAP, 0x10, 0x31, 0x67, 0x34, 0x18, 0x06, 0x60, 0x32, 0x02 },
		10, },
	{ { RGAMMAN, 0x10, 0x14, 0x64, 0x34, 0x00, 0x05, 0x60, 0x33, 0x04 },
		10, },
	{ { GGAMMAP, 0x10, 0x31, 0x67, 0x34, 0x18, 0x06, 0x60, 0x32, 0x02 },
		10, },
	{ { GGAMMAN, 0x10, 0x14, 0x64, 0x34, 0x00, 0x05, 0x60, 0x33, 0x04 },
		10, },
	{ { BGAMMAP, 0x10, 0x31, 0x67, 0x34, 0x18, 0x06, 0x60, 0x32, 0x02 },
		10, },
	{ { BGAMMAN, 0x10, 0x14, 0x64, 0x34, 0x00, 0x05, 0x60, 0x33, 0x04 },
		10, },
	{ { OSCSET, 0x01, 0x04, }, 3 },
	{ { PWRCTL3, 0x00, 0x09, 0x10, 0x12, 0x00, 0x66, 0x00, 0x32, 0x00 },
		10, },
	{ { PWRCTL4, 0x22, 0x24, 0x18, 0x18, 0x47 }, 6, },
	{ { OTP2, 0x00, }, 2 },
	{ { PWRCTL1, 0x00, }, 2 },
	{ { PWRCTL2, 0x02, }, 2 },
};

static struct lg4591_reg init_seq2[] = {
	{ { PWRCTL2, 0x06, }, 2 },
};

static struct lg4591_reg init_seq3[] = {
	{ { PWRCTL2, 0x4E, }, 2 },
};

static struct lg4591_reg sleep_out[] = {
	{ { MIPI_DCS_EXIT_SLEEP_MODE, }, 1},
};

static struct lg4591_reg init_seq4[] = {
	{ { OTP2, 0x80}, 2 },
};

static struct lg4591_reg brightness_ctrl[] = {
	{ { WRCTRLD, 0x24}, 2 },
};

static struct lg4591_reg display_on[] = {
	{ { MIPI_DCS_SET_DISPLAY_ON, }, 1},
};

static int lg4591_write(struct omap_dss_device *dssdev, u8 *buf, int len)
{
	struct lg4591_data *lg_d = dev_get_drvdata(&dssdev->dev);
	int r;

	r = dsi_vc_dcs_write_nosync(dssdev, lg_d->config_channel, buf, len);
	if (r)
		dev_err(&dssdev->dev, "write cmd/reg(%x) failed: %d\n",
				buf[0], r);

	return r;
}

static int lg4591_write_sequence(struct omap_dss_device *dssdev,
		struct lg4591_reg *seq, int len)
{
	int r, i;

	for (i = 0; i < len; i++) {
		r = lg4591_write(dssdev, seq[i].data, seq[i].len);
		if (r) {
			dev_err(&dssdev->dev, "sequence failed: %d\n", i);
			return -EINVAL;
		}

		/* TODO: Figure out why this is needed for OMAP5 */
		msleep(1);
	}

	return 0;
}

static void lg4591_get_timings(struct omap_dss_device *dssdev,
		struct omap_video_timings *timings)
{
	*timings = dssdev->panel.timings;
}

static void lg4591_set_timings(struct omap_dss_device *dssdev,
		struct omap_video_timings *timings)
{
	dssdev->panel.timings.x_res = timings->x_res;
	dssdev->panel.timings.y_res = timings->y_res;
	dssdev->panel.timings.pixel_clock = timings->pixel_clock;
	dssdev->panel.timings.hsw = timings->hsw;
	dssdev->panel.timings.hfp = timings->hfp;
	dssdev->panel.timings.hbp = timings->hbp;
	dssdev->panel.timings.vsw = timings->vsw;
	dssdev->panel.timings.vfp = timings->vfp;
	dssdev->panel.timings.vbp = timings->vbp;
}

static int lg4591_check_timings(struct omap_dss_device *dssdev,
		struct omap_video_timings *timings)
{
	return 0;
}

static void lg4591_get_resolution(struct omap_dss_device *dssdev,
		u16 *xres, u16 *yres)
{
	*xres = dssdev->panel.timings.x_res;
	*yres = dssdev->panel.timings.y_res;
}

static int lg4591_hw_reset(struct omap_dss_device *dssdev)
{
	struct lg4591_data *lg_d = dev_get_drvdata(&dssdev->dev);

	gpio_set_value(lg_d->reset_gpio, 1);
	msleep(20);
	gpio_set_value(lg_d->reset_gpio, 0);
	msleep(25);
	gpio_set_value(lg_d->reset_gpio, 1);
	msleep(20);

	return 0;
}

static int lg4591_update_brightness(struct omap_dss_device *dssdev, int level)
{
	int r;
	u8 buf[2];

	buf[0] = WRDISBV;
	buf[1] = level;

	r = lg4591_write(dssdev, buf, sizeof(buf));
	if (r)
		return r;

	return 0;
}

static int lg4591_set_brightness(struct backlight_device *bd)
{
	struct omap_dss_device *dssdev = dev_get_drvdata(&bd->dev);
	struct lg4591_data *lg_d = dev_get_drvdata(&dssdev->dev);
	int bl = bd->props.brightness;
	int r = 0;

	if (bl == lg_d->bl)
		return 0;

	mutex_lock(&lg_d->lock);

	if (dssdev->state == OMAP_DSS_DISPLAY_ACTIVE) {
		dsi_bus_lock(dssdev);

		r = lg4591_update_brightness(dssdev, bl);
		if (!r)
			lg_d->bl = bl;

		dsi_bus_unlock(dssdev);
	}

	mutex_unlock(&lg_d->lock);

	return r;
}

static int lg4591_get_brightness(struct backlight_device *bd)
{
	return bd->props.brightness;
}

static const struct backlight_ops lg4591_backlight_ops  = {
	.get_brightness = lg4591_get_brightness,
	.update_status = lg4591_set_brightness,
};

static int lg4591_probe_of(struct omap_dss_device *dssdev,
		struct lg4591_data *lg_d)
{
	struct device_node *node = dssdev->dev.of_node;
	struct property *prop;
	u32 lane_arr[10];
	int len, i, num_pins, r;

	r = of_get_gpio(node, 0);
	if (!gpio_is_valid(r)) {
		dev_err(&dssdev->dev, "failed to parse reset gpio\n");
		return r;
	}
	lg_d->reset_gpio = r;

	prop = of_find_property(node, "lanes", &len);
	if (prop == NULL) {
		dev_err(&dssdev->dev, "failed to find lane data\n");
		return -EINVAL;
	}

	num_pins = len / sizeof(u32);

	if (num_pins < 4 || num_pins % 2 != 0
			|| num_pins > ARRAY_SIZE(lane_arr)) {
		dev_err(&dssdev->dev, "bad number of lanes\n");
		return -EINVAL;
	}

	r = of_property_read_u32_array(node, "lanes", lane_arr, num_pins);
	if (r) {
		dev_err(&dssdev->dev, "failed to read lane data\n");
		return r;
	}

	lg_d->pin_config.num_pins = num_pins;
	for (i = 0; i < num_pins; ++i)
		lg_d->pin_config.pins[i] = (int)lane_arr[i];

	return 0;
}

static int lg4591_probe(struct omap_dss_device *dssdev)
{
	struct device_node *node = dssdev->dev.of_node;
	struct backlight_properties props;
	struct lg4591_data *lg_d;
	int r;

	dev_dbg(&dssdev->dev, "lg4591_probe\n");

	if (node == NULL) {
		dev_err(&dssdev->dev, "no device tree data!\n");
		return -EINVAL;
	}

	lg_d = devm_kzalloc(&dssdev->dev, sizeof(*lg_d), GFP_KERNEL);
	if (!lg_d)
		return -ENOMEM;

	dev_set_drvdata(&dssdev->dev, lg_d);
	lg_d->dssdev = dssdev;

	r = lg4591_probe_of(dssdev, lg_d);
	if (r)
		return r;

	dssdev->caps = 0;
	dssdev->panel.timings = lg4591_timings;
	dssdev->panel.dsi_pix_fmt = OMAP_DSS_DSI_FMT_RGB888;

	mutex_init(&lg_d->lock);

	r = devm_gpio_request_one(&dssdev->dev, lg_d->reset_gpio,
			GPIOF_DIR_OUT, "lcd reset");
	if (r) {
		dev_err(&dssdev->dev, "failed to request reset gpio\n");
		return r;
	}

	/* Register DSI backlight control */
	memset(&props, 0, sizeof(struct backlight_properties));

	props.type = BACKLIGHT_RAW;
	props.max_brightness = 255;
	props.brightness = 255;

	lg_d->bl = props.brightness;

	lg_d->bldev = backlight_device_register("lg4591", &dssdev->dev, dssdev,
			&lg4591_backlight_ops, &props);
	if (IS_ERR(lg_d->bldev))
		return PTR_ERR(lg_d->bldev);

	r = omap_dsi_request_vc(dssdev, &lg_d->pixel_channel);
	if (r) {
		dev_err(&dssdev->dev,
				"failed to request pixel update channel\n");
		goto err_req_pix_vc;
	}

	r = omap_dsi_set_vc_id(dssdev, lg_d->pixel_channel, 0);
	if (r) {
		dev_err(&dssdev->dev,
				"failed to set VC_ID for pixel data virtual channel\n");
		goto err_set_pix_vc_id;
	}

	r = omap_dsi_request_vc(dssdev, &lg_d->config_channel);
	if (r) {
		dev_err(&dssdev->dev, "failed to request config channel\n");
		goto err_req_cfg_vc;
	}

	r = omap_dsi_set_vc_id(dssdev, lg_d->config_channel, 0);
	if (r) {
		dev_err(&dssdev->dev,
				"failed to set VC_ID for config virtual channel\n");
		goto err_set_cfg_vc_id;
	}

	return 0;

err_set_cfg_vc_id:
	omap_dsi_release_vc(dssdev, lg_d->config_channel);
err_req_cfg_vc:
err_set_pix_vc_id:
	omap_dsi_release_vc(dssdev, lg_d->pixel_channel);
err_req_pix_vc:
	backlight_device_unregister(lg_d->bldev);

	return r;
}

static void lg4591_remove(struct omap_dss_device *dssdev)
{
	struct lg4591_data *lg_d = dev_get_drvdata(&dssdev->dev);

	omap_dsi_release_vc(dssdev, lg_d->config_channel);
	omap_dsi_release_vc(dssdev, lg_d->pixel_channel);

	backlight_device_unregister(lg_d->bldev);
}

/**
 * lg4591_config - Configure lg4591
 *
 * Initial configuration for lg4591 configuration registers
 */
static int lg4591_config(struct omap_dss_device *dssdev)
{
	struct lg4591_data *lg_d = dev_get_drvdata(&dssdev->dev);
	int r;

	r = lg4591_write_sequence(dssdev, init_seq1, ARRAY_SIZE(init_seq1));
	if (r)
		return r;

	msleep(20);

	r = lg4591_write_sequence(dssdev, init_seq2, ARRAY_SIZE(init_seq2));
	if (r)
		return r;

	msleep(20);

	r = lg4591_write_sequence(dssdev, init_seq3, ARRAY_SIZE(init_seq3));
	if (r)
		return r;

	msleep(20);

	r = lg4591_write_sequence(dssdev, sleep_out, ARRAY_SIZE(sleep_out));
	if (r)
		return r;

	msleep(20);

	r = lg4591_write_sequence(dssdev, init_seq4, ARRAY_SIZE(init_seq4));
	if (r)
		return r;

	msleep(20);

	r = lg4591_write_sequence(dssdev, display_on, ARRAY_SIZE(display_on));
	if (r)
		return r;

	msleep(20);

	r = lg4591_write_sequence(dssdev, brightness_ctrl,
			ARRAY_SIZE(brightness_ctrl));
	if (r)
		return r;

	r = dsi_vc_set_max_rx_packet_size(dssdev, lg_d->config_channel, 0xff);
	if (r) {
		dev_err(&dssdev->dev, "can't set max rx packet size\n");
		return r;
	}

	r = lg4591_update_brightness(dssdev, lg_d->bl);
	if (r)
		return r;

	return 0;
}

static int lg4591_power_on(struct omap_dss_device *dssdev)
{
	struct lg4591_data *lg_d = dev_get_drvdata(&dssdev->dev);
	int r;

	r = omapdss_dsi_configure_pins(dssdev, &lg_d->pin_config);
	if (r) {
		dev_err(&dssdev->dev, "failed to configure DSI pins\n");
		goto err0;
	}

	omapdss_dsi_set_pixel_format(dssdev, OMAP_DSS_DSI_FMT_RGB888);
	omapdss_dsi_set_timings(dssdev, &lg4591_timings);
	omapdss_dsi_set_videomode_timings(dssdev, &vm_data);
	omapdss_dsi_set_operation_mode(dssdev, OMAP_DSS_DSI_VIDEO_MODE);

	r = omapdss_dsi_set_clocks(dssdev, 240000000, 10000000);
	if (r) {
		dev_err(&dssdev->dev, "failed to set HS and LP clocks\n");
		goto err0;
	}

	r = omapdss_dsi_display_enable(dssdev);
	if (r) {
		dev_err(&dssdev->dev, "failed to enable DSI\n");
		goto err0;
	}

	lg4591_hw_reset(dssdev);

	omapdss_dsi_vc_enable_hs(dssdev, lg_d->pixel_channel, true);

	/* XXX */
	msleep(100);

	r = lg4591_config(dssdev);
	if (r) {
		dev_err(&dssdev->dev, "failed to configure panel\n");
		goto err;
	}

	dsi_enable_video_output(dssdev, lg_d->pixel_channel);

	lg_d->enabled = true;

	return r;
err:
	dev_err(&dssdev->dev, "error while enabling panel, issuing HW reset\n");

	lg4591_hw_reset(dssdev);

	omapdss_dsi_display_disable(dssdev, false, false);
err0:
	return r;
}

static void lg4591_power_off(struct omap_dss_device *dssdev)
{
	struct lg4591_data *lg_d = dev_get_drvdata(&dssdev->dev);

	gpio_set_value(lg_d->reset_gpio, 0);

	msleep(20);

	lg_d->enabled = 0;
	dsi_disable_video_output(dssdev, lg_d->pixel_channel);
	omapdss_dsi_display_disable(dssdev, false, false);
}

static int lg4591_start(struct omap_dss_device *dssdev)
{
	struct lg4591_data *lg_d = dev_get_drvdata(&dssdev->dev);
	int r = 0;

	mutex_lock(&lg_d->lock);

	dsi_bus_lock(dssdev);

	r = lg4591_power_on(dssdev);

	dsi_bus_unlock(dssdev);

	if (r)
		dev_err(&dssdev->dev, "enable failed\n");
	else
		dssdev->state = OMAP_DSS_DISPLAY_ACTIVE;

	mutex_unlock(&lg_d->lock);

	return r;
}

static void lg4591_stop(struct omap_dss_device *dssdev)
{
	struct lg4591_data *lg_d = dev_get_drvdata(&dssdev->dev);

	mutex_lock(&lg_d->lock);

	dsi_bus_lock(dssdev);

	lg4591_power_off(dssdev);

	dsi_bus_unlock(dssdev);

	mutex_unlock(&lg_d->lock);
}

static void lg4591_disable(struct omap_dss_device *dssdev)
{
	dev_dbg(&dssdev->dev, "disable\n");

	if (dssdev->state == OMAP_DSS_DISPLAY_ACTIVE)
		lg4591_stop(dssdev);

	dssdev->state = OMAP_DSS_DISPLAY_DISABLED;
}

static int lg4591_enable(struct omap_dss_device *dssdev)
{
	dev_dbg(&dssdev->dev, "enable\n");

	if (dssdev->state != OMAP_DSS_DISPLAY_DISABLED)
		return -EINVAL;

	return lg4591_start(dssdev);
}

#if defined(CONFIG_OF)
static const struct of_device_id lg4591_of_match[] = {
	{
		.compatible = "ti,lg4591",
	},
	{},
};

MODULE_DEVICE_TABLE(of, lg4591_of_match);
#else
#define dss_of_match NULL
#endif

static struct omap_dss_driver lg4591_driver = {
	.probe = lg4591_probe,
	.remove = lg4591_remove,

	.enable = lg4591_enable,
	.disable = lg4591_disable,

	.get_resolution = lg4591_get_resolution,
	.get_recommended_bpp = omapdss_default_get_recommended_bpp,

	.get_timings = lg4591_get_timings,
	.set_timings = lg4591_set_timings,
	.check_timings = lg4591_check_timings,

	.driver = {
		.name = "lg4591",
		.owner = THIS_MODULE,
		.of_match_table = lg4591_of_match,
	},
};

static int __init lg4591_init(void)
{
	omap_dss_register_driver(&lg4591_driver);
	return 0;
}

static void __exit lg4591_exit(void)
{
	omap_dss_unregister_driver(&lg4591_driver);
}

module_init(lg4591_init);
module_exit(lg4591_exit);

MODULE_AUTHOR("Tomi Valkeinen <tomi.valkeinen@ti.com>");
MODULE_DESCRIPTION("LG4591 driver");
MODULE_LICENSE("GPL");
