/*
 * LG LD089WU1 MIPI DSI driver
 *
 * Copyright (C) Texas Instruments
 * Author: Subbaraman Narayanamurthy <x0164410@ti.com>
 *
 * Based on original version from Jerry Alexander <x0135174@ti.com>
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
#include <linux/backlight.h>
#include <linux/fb.h>
#include <linux/interrupt.h>
#include <linux/gpio.h>
#include <linux/slab.h>
#include <linux/mutex.h>

#include <video/omapdss.h>

static struct omap_video_timings ld089wu1_timings;

/* device private data structure */
struct ld089wu1_data {
	struct mutex lock;

	struct omap_dss_device *dssdev;

	int channel0;
	int channel1;
};

static void ld089wu1_get_timings(struct omap_dss_device *dssdev,
		struct omap_video_timings *timings)
{
	*timings = dssdev->panel.timings;
}

static void ld089wu1_set_timings(struct omap_dss_device *dssdev,
		struct omap_video_timings *timings)
{
	dev_info(&dssdev->dev, "set_timings() not implemented\n");
}

static int ld089wu1_check_timings(struct omap_dss_device *dssdev,
		struct omap_video_timings *timings)
{
	if (ld089wu1_timings.x_res != timings->x_res ||
			ld089wu1_timings.y_res != timings->y_res ||
			ld089wu1_timings.pixel_clock != timings->pixel_clock ||
			ld089wu1_timings.hsw != timings->hsw ||
			ld089wu1_timings.hfp != timings->hfp ||
			ld089wu1_timings.hbp != timings->hbp ||
			ld089wu1_timings.vsw != timings->vsw ||
			ld089wu1_timings.vfp != timings->vfp ||
			ld089wu1_timings.vbp != timings->vbp)
		return -EINVAL;

	return 0;
}

static void ld089wu1_get_resolution(struct omap_dss_device *dssdev,
		u16 *xres, u16 *yres)
{
	*xres = ld089wu1_timings.x_res;
	*yres = ld089wu1_timings.y_res;
}

static int ld089wu1_hw_reset(struct omap_dss_device *dssdev)
{
	if (dssdev == NULL || dssdev->reset_gpio == -1)
		return -1;

	gpio_set_value(dssdev->reset_gpio, 1);
	udelay(100);
	/* reset the panel */
	gpio_set_value(dssdev->reset_gpio, 0);
	/* assert reset */
	udelay(100);
	gpio_set_value(dssdev->reset_gpio, 1);

	/* wait after releasing reset */
	msleep(100);

	return 0;
}

static int ld089wu1_hw_enable(struct omap_dss_device *dssdev, bool enable)
{
	if (dssdev == NULL || dssdev->reset_gpio == -1)
		return -1;

	if (enable) {
		gpio_set_value(dssdev->reset_gpio, 1);
		dev_dbg(&dssdev->dev, "ld089wu1_hw_enable\n");
	} else {
		gpio_set_value(dssdev->reset_gpio, 0);
		dev_dbg(&dssdev->dev, "ld089wu1_hw_disable\n");
	}

	return 0;
}

static int ld089wu1_probe(struct omap_dss_device *dssdev)
{
	struct ld089wu1_data *d2d;
	int r = 0;

	dev_dbg(&dssdev->dev, "ld089wu1_probe\n");

	dssdev->panel.config = OMAP_DSS_LCD_TFT;
	ld089wu1_timings = dssdev->panel.timings;

	d2d = kzalloc(sizeof(*d2d), GFP_KERNEL);
	if (!d2d) {
		r = -ENOMEM;
		return r;
	}

	d2d->dssdev = dssdev;

	mutex_init(&d2d->lock);

	dev_set_drvdata(&dssdev->dev, d2d);

	r = omap_dsi_request_vc(dssdev, &d2d->channel0);
	if (r) {
		dev_err(&dssdev->dev, "failed to get virtual channel0\n");
		goto err;
	}

	r = omap_dsi_set_vc_id(dssdev, d2d->channel0, 0);
	if (r) {
		dev_err(&dssdev->dev, "failed to set VC_ID0\n");
		goto err;
	}

	r = omap_dsi_request_vc(dssdev, &d2d->channel1);
	if (r) {
		dev_err(&dssdev->dev, "failed to get virtual channel1\n");
		goto err;
	}

	r = omap_dsi_set_vc_id(dssdev, d2d->channel1, 0);
	if (r) {
		dev_err(&dssdev->dev, "failed to set VC_ID1\n");
		goto err;
	}

	dev_dbg(&dssdev->dev, "ld089wu1_probe done\n");
	return 0;

err:
	kfree(d2d);
	return r;
}

static void ld089wu1_remove(struct omap_dss_device *dssdev)
{
	struct ld089wu1_data *d2d = dev_get_drvdata(&dssdev->dev);

	omap_dsi_release_vc(dssdev, d2d->channel0);
	omap_dsi_release_vc(dssdev, d2d->channel1);

	kfree(d2d);
}

static int ld089wu1_power_on(struct omap_dss_device *dssdev)
{
	struct ld089wu1_data *d2d = dev_get_drvdata(&dssdev->dev);
	int r;

	/* At power on the first vsync has not been received yet */
	dssdev->first_vsync = false;

	dev_dbg(&dssdev->dev, "power_on\n");

	if (dssdev->platform_enable)
		dssdev->platform_enable(dssdev);

	r = omapdss_dsi_display_enable(dssdev);
	if (r) {
		dev_err(&dssdev->dev, "failed to enable DSI\n");
		goto err_disp_enable;
	}

	/* reset ld089wu1 */
	ld089wu1_hw_reset(dssdev);

	/* do extra job to match kozio registers (???) */
	dsi_videomode_panel_preinit(dssdev);

	omapdss_dsi_vc_enable_hs(dssdev, d2d->channel0, true);
	omapdss_dsi_vc_enable_hs(dssdev, d2d->channel1, true);

	/* 0x0e - 16bit
	 * 0x1e - packed 18bit
	 * 0x2e - unpacked 18bit
	 * 0x3e - 24bit
	 */
	switch (dssdev->ctrl.pixel_size) {
	case 18:
		dsi_video_mode_enable(dssdev, 0x1e);
		break;
	case 24:
		dsi_video_mode_enable(dssdev, 0x3e);
		break;
	default:
		dev_warn(&dssdev->dev, "not expected pixel size: %d\n",
					dssdev->ctrl.pixel_size);
	}

	dev_dbg(&dssdev->dev, "power_on done\n");

	return r;

err_disp_enable:
	if (dssdev->platform_disable)
		dssdev->platform_disable(dssdev);

	return r;
}

static void ld089wu1_power_off(struct omap_dss_device *dssdev)
{
	int r;
	dsi_video_mode_disable(dssdev);

	omapdss_dsi_display_disable(dssdev, false, false);

	if (dssdev->platform_disable)
		dssdev->platform_disable(dssdev);

	/* disable lcd */
	r = ld089wu1_hw_enable(dssdev, false);
	if (r)
		dev_err(&dssdev->dev, "failed to disable LCD\n");
}

static void ld089wu1_disable(struct omap_dss_device *dssdev)
{
	struct ld089wu1_data *d2d = dev_get_drvdata(&dssdev->dev);

	dev_dbg(&dssdev->dev, "disable\n");

	if (dssdev->state == OMAP_DSS_DISPLAY_ACTIVE) {
		mutex_lock(&d2d->lock);
		dsi_bus_lock(dssdev);

		ld089wu1_power_off(dssdev);

		dsi_bus_unlock(dssdev);
		mutex_unlock(&d2d->lock);
	}

	dssdev->state = OMAP_DSS_DISPLAY_DISABLED;
}

static int ld089wu1_enable(struct omap_dss_device *dssdev)
{
	struct ld089wu1_data *d2d = dev_get_drvdata(&dssdev->dev);
	int r = 0;

	dev_dbg(&dssdev->dev, "enable\n");

	if (dssdev->state != OMAP_DSS_DISPLAY_DISABLED)
		return -EINVAL;

	mutex_lock(&d2d->lock);
	dsi_bus_lock(dssdev);

	r = ld089wu1_power_on(dssdev);

	dsi_bus_unlock(dssdev);

	if (r) {
		dev_err(&dssdev->dev, "enable failed\n");
		dssdev->state = OMAP_DSS_DISPLAY_DISABLED;
	} else {
		dssdev->state = OMAP_DSS_DISPLAY_ACTIVE;
	}

	mutex_unlock(&d2d->lock);

	return r;
}

static int ld089wu1_suspend(struct omap_dss_device *dssdev)
{
	dev_dbg(&dssdev->dev, "suspend\n");
	ld089wu1_disable(dssdev);
	return 0;
}

static int ld089wu1_resume(struct omap_dss_device *dssdev)
{
	int r = 0;

	dev_dbg(&dssdev->dev, "resume\n");
	r = ld089wu1_enable(dssdev);
	if (r)
		dev_err(&dssdev->dev, "resume failed\n");
	return r;
}

static struct omap_dss_driver ld089wu1_driver = {
	.probe		= ld089wu1_probe,
	.remove		= ld089wu1_remove,

	.enable		= ld089wu1_enable,
	.disable	= ld089wu1_disable,
	.suspend	= ld089wu1_suspend,
	.resume		= ld089wu1_resume,

	.get_resolution	= ld089wu1_get_resolution,
	.get_recommended_bpp = omapdss_default_get_recommended_bpp,

	.get_timings	= ld089wu1_get_timings,
	.set_timings	= ld089wu1_set_timings,
	.check_timings	= ld089wu1_check_timings,

	.driver         = {
		.name   = "ld089wu1",
		.owner  = THIS_MODULE,
	},
};

static int __init ld089wu1_init(void)
{
	int r;

	r = omap_dss_register_driver(&ld089wu1_driver);
	if (r < 0) {
		printk(KERN_ERR "ld089wu1 driver registration failed\n");
		return r;
	}

	return 0;
}

static void __exit ld089wu1_exit(void)
{
	omap_dss_unregister_driver(&ld089wu1_driver);
}

module_init(ld089wu1_init);
module_exit(ld089wu1_exit);

MODULE_AUTHOR("Subbaraman Narayanamurthy <x0164410@ti.com>");
MODULE_DESCRIPTION("LG LD089WU1 MIPI DSI Driver");
MODULE_LICENSE("GPL");
