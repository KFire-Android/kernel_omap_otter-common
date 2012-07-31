/*
 * LG-4591 panel support
 *
 * Copyright 2011 Texas Instruments, Inc.
 * Author: Archit Taneja <archit@ti.com>
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
#include <linux/debugfs.h>
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

#include <video/omapdss.h>
#include <video/mipi_display.h>
#include <video/omap-panel-lg4591.h>

#include "panel-lg4591.h"

static struct omap_video_timings lg4591_timings = {
	.x_res		= 720,
	.y_res		= 1280,
	.pixel_clock	= 73940,
	.hfp		= 154,
	.hsw		= 4,
	.hbp		= 62,
	.vfp		= 84,
	.vsw		= 1,
	.vbp		= 15,
};

static const struct omap_dss_dsi_videomode_data vm_data = {
	.hsa			= 1,
	.hfp			= 115,
	.hbp			= 47,
	.vsa			= 1,
	.vfp			= 84,
	.vbp			= 15,

	.vp_de_pol		= true,
	.vp_vsync_pol		= true,
	.vp_hsync_pol		= false,
	.vp_hsync_end		= false,
	.vp_vsync_end		= false,

	.blanking_mode		= 0,
	.hsa_blanking_mode	= 1,
	.hfp_blanking_mode	= 1,
	.hbp_blanking_mode	= 1,

	.ddr_clk_always_on	= true,

	.window_sync		= 4,
};

struct lg4591_data {
	struct mutex lock;

	struct omap_dss_device *dssdev;
	struct backlight_device *bldev;
	struct dentry *debug_dir;
	bool enabled;
	u8 rotate;
	bool mirror;
	int bl;

	int config_channel;
	int pixel_channel;

	struct omap_video_timings *timings;

	struct panel_lg4591_data *pdata;
};

struct lg4591_reg {
	/* Address and register value */
	u8 data[10];
	int len;
};

static int lg4591_config(struct omap_dss_device *dssdev);

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
	{ { BLCTL, 0x82, 0x24, 0x01, 0x11}, 5 },
};

static struct lg4591_reg display_on[] = {
	{ { MIPI_DCS_SET_DISPLAY_ON, }, 1},
};

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

/* dummy functions */

static int lg4591_rotate(struct omap_dss_device *dssdev, u8 rotate)
{
	return 0;
}

static u8 lg4591_get_rotate(struct omap_dss_device *dssdev)
{
	return 0;
}

static int lg4591_mirror(struct omap_dss_device *dssdev, bool enable)
{
	return 0;
}

static bool lg4591_get_mirror(struct omap_dss_device *dssdev)
{
	return false;
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
	struct lg4591_data *lg_d = dev_get_drvdata(&dssdev->dev);

	if (lg_d->rotate == 0 || lg_d->rotate == 2) {
		*xres = dssdev->panel.timings.x_res;
		*yres = dssdev->panel.timings.y_res;
	} else {
		*yres = dssdev->panel.timings.x_res;
		*xres = dssdev->panel.timings.y_res;
	}
}

static int lg4591_enable_te(struct omap_dss_device *dssdev, bool enable)
{
	return 0;
}

static int lg4591_hw_reset(struct omap_dss_device *dssdev)
{
	struct lg4591_data *lg_d = dev_get_drvdata(&dssdev->dev);

	gpio_set_value(lg_d->pdata->reset_gpio, 1);
	msleep(10);
	gpio_set_value(lg_d->pdata->reset_gpio, 0);
	msleep(15);
	gpio_set_value(lg_d->pdata->reset_gpio, 1);
	msleep(10);

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

static int lg4591_probe(struct omap_dss_device *dssdev)
{
	int ret = 0;
	struct backlight_properties props = {
		.brightness = 255,
		.max_brightness = 255,
		.type = BACKLIGHT_RAW,
	};
	struct lg4591_data *lg_d = NULL;

	dev_dbg(&dssdev->dev, "lg4591_probe\n");

	if (dssdev->data == NULL) {
		dev_err(&dssdev->dev, "no platform data!\n");
		return -EINVAL;
	}

	dssdev->panel.config = OMAP_DSS_LCD_TFT;
	dssdev->panel.timings = lg4591_timings;
	dssdev->panel.dsi_vm_data = vm_data;
	dssdev->panel.dsi_pix_fmt = OMAP_DSS_DSI_FMT_RGB888;

	lg_d = kzalloc(sizeof(*lg_d), GFP_KERNEL);
	if (!lg_d)
		return -ENOMEM;

	lg_d->dssdev = dssdev;
	lg_d->pdata = dssdev->data;

	lg_d->bl = props.brightness;

	if (!lg_d->pdata) {
		dev_err(&dssdev->dev, "Invalid platform data\n");
		ret = -EINVAL;
		goto err;
	}

	mutex_init(&lg_d->lock);

	dev_set_drvdata(&dssdev->dev, lg_d);

	/* Register DSI backlight control */
	lg_d->bldev = backlight_device_register("lg4591", &dssdev->dev, dssdev,
		&lg4591_backlight_ops, &props);
	if (IS_ERR(lg_d->bldev)) {
		ret = PTR_ERR(lg_d->bldev);
		goto err_backlight_device_register;
	}

	lg_d->debug_dir = debugfs_create_dir("lg4591", NULL);
	if (!lg_d->debug_dir) {
		dev_err(&dssdev->dev, "failed to create debug dir\n");
		goto err_debugfs;
	}

	ret = omap_dsi_request_vc(dssdev, &lg_d->pixel_channel);
	if (ret) {
		dev_err(&dssdev->dev, "failed to request pixel update "
			"channel\n");
		goto err_debugfs;
	}

	ret = omap_dsi_set_vc_id(dssdev, lg_d->pixel_channel, 0);
	if (ret) {
		dev_err(&dssdev->dev, "failed to set VC_ID for pixel data "
			"virtual channel\n");
		goto err_pixel_vc;
	}

	ret = omap_dsi_request_vc(dssdev, &lg_d->config_channel);
	if (ret) {
		dev_err(&dssdev->dev, "failed to request config channel\n");
		goto err_pixel_vc;
	}

	ret = omap_dsi_set_vc_id(dssdev, lg_d->config_channel, 0);
	if (ret) {
		dev_err(&dssdev->dev, "failed to set VC_ID for config"
			" virtual channel\n");
		goto err_config_vc;
	}

	dev_dbg(&dssdev->dev, "lg4591_probe\n");

	return ret;
err_config_vc:
	omap_dsi_release_vc(dssdev, lg_d->config_channel);
err_pixel_vc:
	omap_dsi_release_vc(dssdev, lg_d->pixel_channel);
err_debugfs:
	backlight_device_unregister(lg_d->bldev);
err_backlight_device_register:
	mutex_destroy(&lg_d->lock);
	gpio_free(lg_d->pdata->reset_gpio);
err:
	kfree(lg_d);

	return ret;
}

static void lg4591_remove(struct omap_dss_device *dssdev)
{
	struct lg4591_data *lg_d = dev_get_drvdata(&dssdev->dev);
	debugfs_remove_recursive(lg_d->debug_dir);
	backlight_device_unregister(lg_d->bldev);
	mutex_destroy(&lg_d->lock);
	gpio_free(lg_d->pdata->reset_gpio);
	kfree(lg_d);
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

	msleep(10);

	r = lg4591_write_sequence(dssdev, init_seq2, ARRAY_SIZE(init_seq2));
	if (r)
		return r;

	msleep(10);

	r = lg4591_write_sequence(dssdev, init_seq3, ARRAY_SIZE(init_seq3));
	if (r)
		return r;

	msleep(10);

	r = lg4591_write_sequence(dssdev, sleep_out, ARRAY_SIZE(sleep_out));
	if (r)
		return r;

	msleep(10);

	r = lg4591_write_sequence(dssdev, init_seq4, ARRAY_SIZE(init_seq4));
	if (r)
		return r;

	msleep(10);

	r = lg4591_write_sequence(dssdev, display_on, ARRAY_SIZE(display_on));
	if (r)
		return r;

	msleep(10);

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

	/* At power on the first vsync has not been received yet */
	dssdev->first_vsync = false;

	if (lg_d->pdata->set_power)
		lg_d->pdata->set_power(true);

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

	gpio_set_value(lg_d->pdata->reset_gpio, 0);

	msleep(20);

	lg_d->enabled = 0;
	dsi_disable_video_output(dssdev, lg_d->pixel_channel);
	omapdss_dsi_display_disable(dssdev, false, false);

	if (lg_d->pdata->set_power)
		lg_d->pdata->set_power(false);

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

static int lg4591_resume(struct omap_dss_device *dssdev)
{
	dev_dbg(&dssdev->dev, "resume\n");

	if (dssdev->state != OMAP_DSS_DISPLAY_SUSPENDED)
		return -EINVAL;

	return lg4591_start(dssdev);
}

static int lg4591_suspend(struct omap_dss_device *dssdev)
{
	dev_dbg(&dssdev->dev, "suspend\n");

	if (dssdev->state != OMAP_DSS_DISPLAY_ACTIVE)
		return -EINVAL;

	lg4591_stop(dssdev);

	dssdev->state = OMAP_DSS_DISPLAY_SUSPENDED;

	return 0;
}

static int lg4591_sync(struct omap_dss_device *dssdev)
{
	return 0;
}

static struct omap_dss_driver lg4591_driver = {
	.probe = lg4591_probe,
	.remove = lg4591_remove,

	.enable = lg4591_enable,
	.disable = lg4591_disable,
	.suspend = lg4591_suspend,
	.resume = lg4591_resume,

	.sync = lg4591_sync,

	.get_resolution = lg4591_get_resolution,
	.get_recommended_bpp = omapdss_default_get_recommended_bpp,

	/* dummy entry start */
	.enable_te = lg4591_enable_te,
	.set_rotate = lg4591_rotate,
	.get_rotate = lg4591_get_rotate,
	.set_mirror = lg4591_mirror,
	.get_mirror = lg4591_get_mirror,
	/* dummy entry end */

	.get_timings = lg4591_get_timings,
	.set_timings = lg4591_set_timings,
	.check_timings = lg4591_check_timings,

	.driver = {
		.name = "lg4591",
		.owner = THIS_MODULE,
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
