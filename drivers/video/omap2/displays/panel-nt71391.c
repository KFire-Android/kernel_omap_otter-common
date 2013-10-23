/*
 * Novatek NT71391 MIPI DSI driver
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

/*commenting this flag since it can cause audio skips
 * during suspend resume*/
#define DEBUG 

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
#include <linux/i2c.h>
#include <video/omapdss.h>
#include <video/omap-panel-generic.h>
#include "../dss/dss.h"

#include "panel-nt71391.h"


#ifdef CONFIG_MACH_OMAP4_BOWSER_SUBTYPE_JEM_FTM
static unsigned char first_suspend = 0;
#else
static unsigned char first_suspend = 1;
#endif

#define GPIO_BACKLIGHT_EN_O2M         25  /* Backlight GPIO */
DECLARE_WAIT_QUEUE_HEAD(panel_fini_queue);

static struct omap_video_timings nt71391_timings = {
	.x_res		= NT71391_WIDTH,
	.y_res		= NT71391_HEIGHT,
	.pixel_clock	= NT71391_PCLK,
	.hfp		= NT71391_HFP,
	.hsw            = NT71391_HSW,
	.hbp            = NT71391_HBP,
	.vfp            = NT71391_VFP,
	.vsw            = NT71391_VSW,
	.vbp            = NT71391_VBP,
};

/* device private data structure */
struct nt71391_data {
	struct mutex lock;

	struct omap_dss_device *dssdev;

	int channel0;
	int channel1;
};

static struct panel_board_data *get_board_data(struct omap_dss_device *dssdev)
{
	return (struct panel_board_data *)dssdev->data;
}

static void nt71391_get_timings(struct omap_dss_device *dssdev,
		struct omap_video_timings *timings)
{
	*timings = dssdev->panel.timings;
}

static void nt71391_set_timings(struct omap_dss_device *dssdev,
		struct omap_video_timings *timings)
{
}

static int nt71391_check_timings(struct omap_dss_device *dssdev,
		struct omap_video_timings *timings)
{
	if (nt71391_timings.x_res != timings->x_res ||
			nt71391_timings.y_res != timings->y_res ||
			nt71391_timings.pixel_clock != timings->pixel_clock ||
			nt71391_timings.hsw != timings->hsw ||
			nt71391_timings.hfp != timings->hfp ||
			nt71391_timings.hbp != timings->hbp ||
			nt71391_timings.vsw != timings->vsw ||
			nt71391_timings.vfp != timings->vfp ||
			nt71391_timings.vbp != timings->vbp)
		return -EINVAL;

	return 0;
}

static void nt71391_get_resolution(struct omap_dss_device *dssdev,
		u16 *xres, u16 *yres)
{
	*xres = nt71391_timings.x_res;
	*yres = nt71391_timings.y_res;
}

static int nt71391_lcd_enable(struct omap_dss_device *dssdev)
{
	struct panel_board_data *board_data = get_board_data(dssdev);

	if (board_data->lcd_en_gpio == -1)
		return -1;

	gpio_set_value(board_data->lcd_en_gpio, 1);
	dev_dbg(&dssdev->dev, "nt71391_lcd_enable\n");

	return 0;
}

static int nt71391_lcd_disable(struct omap_dss_device *dssdev)
{
	struct panel_board_data *board_data = get_board_data(dssdev);

	if (board_data->lcd_en_gpio == -1)
		return -1;

	gpio_set_value(board_data->lcd_en_gpio, 0);
	dev_dbg(&dssdev->dev, "nt71391_lcd_disable\n");

	return 0;
}

static int nt71391_probe(struct omap_dss_device *dssdev)
{
	struct nt71391_data *d2d;
	int r = 0;

	dev_dbg(&dssdev->dev, "nt71391_probe\n");

	dssdev->panel.config = OMAP_DSS_LCD_TFT;
	dssdev->panel.timings = nt71391_timings;
	dssdev->ctrl.pixel_size = 18;

	d2d = kzalloc(sizeof(*d2d), GFP_KERNEL);
	if (!d2d) {
		r = -ENOMEM;
		return r;
	}

	d2d->dssdev = dssdev;

	mutex_init(&d2d->lock);

	dev_set_drvdata(&dssdev->dev, d2d);

	r = omap_dsi_request_vc(dssdev, &d2d->channel0);
	if (r)
		dev_err(&dssdev->dev, "failed to get virtual channel0\n");

	r = omap_dsi_set_vc_id(dssdev, d2d->channel0, 0);
	if (r)
		dev_err(&dssdev->dev, "failed to set VC_ID0\n");

	r = omap_dsi_request_vc(dssdev, &d2d->channel1);
	if (r)
		dev_err(&dssdev->dev, "failed to get virtual channel1\n");

	r = omap_dsi_set_vc_id(dssdev, d2d->channel1, 0);
	if (r)
		dev_err(&dssdev->dev, "failed to set VC_ID1\n");

	dev_dbg(&dssdev->dev, "nt71391_probe done\n");
	printk("nt71391_probe done\n");

	/* do I need an err and kfree(d2d) */
	return r;
}

static void nt71391_remove(struct omap_dss_device *dssdev)
{
	struct nt71391_data *d2d = dev_get_drvdata(&dssdev->dev);

	omap_dsi_release_vc(dssdev, d2d->channel0);
	omap_dsi_release_vc(dssdev, d2d->channel1);

	kfree(d2d);
}

void nt71391_set_clk_153(struct omap_dss_device *dssdev)
{
	struct nt71391_data *d2d = dev_get_drvdata(&dssdev->dev);
	u8 buf[2];
	int r = 0;

	buf[0] = 0xF3;
	buf[1] = 0xA0;

	printk("-------------------nt71391_set_clk_153-------------\n");
	r = dsi_vc_gen_write_nosync_sclk(dssdev, d2d->channel0, buf, 4);
	if(r < 0) {
		printk("Error in sending lock cmd\n");
		goto err;
	}

	mdelay(2);

	buf[0] = 0xac;//ADDR=AC
	buf[1] = 0x2b;//CLK=153

	r = dsi_vc_gen_write_nosync_sclk(dssdev, d2d->channel0, buf, 5);
	if(r < 0) {
		printk("Error in setting clk cmd\n");
		goto err;
	}

	mdelay(2);

	buf[0] = 0;
	buf[1] = 0;
	r = dsi_vc_gen_write_nosync_sclk(dssdev, d2d->channel0, buf, 6);
	if(r < 0) {
		printk("Error in sending unlock cmd\n");
		goto err;
	}

	return;
err:
	printk("Error: fail to set system clock\n");
	return;
}

void nt71391_read_vendor_id(struct omap_dss_device *dssdev)
{
	struct nt71391_data *d2d = dev_get_drvdata(&dssdev->dev);
	u8 reg;
	u8 buf[2];
	u8 vendor_id[3];
	int r = 0;

	buf[0] = 0xF3;
	buf[1] = 0xA0;

	r = dsi_vc_generic_write_nosync(dssdev, d2d->channel0, buf, 2);
	if(r < 0)
	{
		printk("Error in sending lock cmd\n");
		goto err;
	}

	mdelay(1);

	reg = 0x8D;
	r = dsi_vc_generic_read_1(dssdev, d2d->channel0, reg, &vendor_id[0], 2);
	if(r < 0)
	{
		printk("Error in dsi read\n");
		goto err;
	}

	mdelay(1);

	reg = 0x8E;
	r = dsi_vc_generic_read_1(dssdev, d2d->channel0, reg, &vendor_id[1], 2);
	if(r < 0)
	{
		printk("Error in dsi read\n");
		goto err;
	}

	mdelay(1);

	reg = 0x8F;
	r = dsi_vc_generic_read_1(dssdev, d2d->channel0, reg, &vendor_id[2], 2);
	if(r < 0)
	{
		printk("Error in dsi read\n");
		goto err;
	}

	mdelay(1);

	buf[0] = 0;
	buf[1] = 0;
	r = dsi_vc_generic_write_nosync(dssdev, d2d->channel0, buf, 1);
	if(r < 0)
	{
		printk("Error in sending unlock cmd\n");
		goto err;
	}

	printk("Read values %x %x %x\n", vendor_id[0], vendor_id[1], vendor_id[2]);
	return ;
err:
	printk("Error: Could not read vendor id\n");
	return;
}

static int nt71391_power_on(struct omap_dss_device *dssdev)
{
	struct nt71391_data *d2d = dev_get_drvdata(&dssdev->dev);
	int r;
	u8 dcs_cmd = 0x32;

	r = dss_set_dispc_clk(192000000);
	if (r)
		dev_err(&dssdev->dev, "Failed to set dispc clock rate\n");


	/* At power on the first vsync has not been received yet */
	dssdev->first_vsync = false;
	dev_dbg(&dssdev->dev, "power_on\n");

	if (dssdev->platform_enable)
		dssdev->platform_enable(dssdev);

	if(!dssdev->skip_init) {
		if(first_suspend)
		{
			/* Turn off the panel voltage first and wait for sometime */
			nt71391_lcd_disable(dssdev);
			msleep(100);
		}
		/* enable lcd */
		r = nt71391_lcd_enable(dssdev);
		if(r) {
			dev_err(&dssdev->dev, "failed to enable LCD\n");
			goto err_write_init;
		}
		/* Wait till VCC gets applied */
		msleep(20);
	}

	r = omapdss_dsi_display_enable(dssdev);
	if (r) {
		dev_err(&dssdev->dev, "failed to enable DSI\n");
		goto err_disp_enable;
	}

	/* first_suspend flag is to ignore suspend one time during init process
	we use this flag to re-init the TCON once by disable/enable panel
	voltage. From the next time suspend will help doing this */
	msleep(120);
	nt71391_set_clk_153(dssdev);
	if(!dssdev->skip_init) {
	/* Wait till TCON gets ready */
	//msleep(120);

	/* do extra job to match kozio registers (???) */
	dsi_videomode_panel_preinit(dssdev);

	/* Read vendor id */
#if defined (READ_VENDOR_ID)
	nt71391_read_vendor_id(dssdev);
#endif

	/* Send turn on peripheral command to NT TCON */
	dsi_vc_dcs_write(dssdev, d2d->channel0, &dcs_cmd, 1);

	omapdss_dsi_vc_enable_hs(dssdev, d2d->channel0, true);
	omapdss_dsi_vc_enable_hs(dssdev, d2d->channel1, true);

	/* 0x0e - 16bit
	* 0x1e - packed 18bit
	* 0x2e - unpacked 18bit
	* 0x3e - 24bit
	*/
	dsi_enable_video_output(dssdev, d2d->channel0);
	}
	else
		r = dss_mgr_enable(dssdev->manager);

	if(dssdev->skip_init)
		dssdev->skip_init = false;

	dev_dbg(&dssdev->dev, "power_on done\n");

	return r;

err_write_init:
	omapdss_dsi_display_disable(dssdev, false, false);
err_disp_enable:
	if (dssdev->platform_disable)
		dssdev->platform_disable(dssdev);

	return r;
}

static void nt71391_power_off(struct omap_dss_device *dssdev)
{
	int r;
	struct nt71391_data *d2d = dev_get_drvdata(&dssdev->dev);

	/* 0x0e - 16bit
	* 0x1e - packed 18bit
	* 0x2e - unpacked 18bit
	* 0x3e - 24bit
	*/

	dsi_disable_video_output(dssdev, d2d->channel0);
	dsi_disable_video_output(dssdev, d2d->channel1);

	omapdss_dsi_display_disable(dssdev, false, false);

	if (dssdev->platform_disable)
		dssdev->platform_disable(dssdev);

	/* disable lcd */
	r = nt71391_lcd_disable(dssdev);
	if(r) {
		dev_err(&dssdev->dev, "failed to disable LCD\n");
	}
}

static void nt71391_disable(struct omap_dss_device *dssdev)
{
	struct nt71391_data *d2d = dev_get_drvdata(&dssdev->dev);

	dev_dbg(&dssdev->dev, "disable\n");

	wait_event_timeout(panel_fini_queue, gpio_get_value(GPIO_BACKLIGHT_EN_O2M)==0 , msecs_to_jiffies(100) );

	/*Additional discharge time for PWM to complete the operation*/
        msleep(200);

	if (dssdev->state == OMAP_DSS_DISPLAY_ACTIVE) {
		mutex_lock(&d2d->lock);
		dsi_bus_lock(dssdev);

		nt71391_power_off(dssdev);

		dsi_bus_unlock(dssdev);
		mutex_unlock(&d2d->lock);
	}

	dssdev->state = OMAP_DSS_DISPLAY_DISABLED;
}

static int nt71391_enable(struct omap_dss_device *dssdev)
{
	struct nt71391_data *d2d = dev_get_drvdata(&dssdev->dev);
	int r = 0;

	dev_dbg(&dssdev->dev, "enable\n");

	if (dssdev->state != OMAP_DSS_DISPLAY_DISABLED)
		return -EINVAL;

	mutex_lock(&d2d->lock);
	dsi_bus_lock(dssdev);

	r = nt71391_power_on(dssdev);

	dsi_bus_unlock(dssdev);

	if (r) {
		dev_dbg(&dssdev->dev, "enable failed\n");
		dssdev->state = OMAP_DSS_DISPLAY_DISABLED;
	} else {
		dssdev->state = OMAP_DSS_DISPLAY_ACTIVE;
	}

	mutex_unlock(&d2d->lock);
	return r;
}

static int nt71391_suspend(struct omap_dss_device *dssdev)
{
	struct nt71391_data *nd = dev_get_drvdata(&dssdev->dev);
	int r = 0;

	if(first_suspend)
	{
		dev_dbg(&dssdev->dev, "not suspending now\n");
		first_suspend = 0;
		return 0;
	}

	wait_event_timeout(panel_fini_queue, gpio_get_value(GPIO_BACKLIGHT_EN_O2M)==0 , msecs_to_jiffies(100) );

	/*Additional discharge time for PWM to complete the operation*/
        msleep(200);

	dev_dbg(&dssdev->dev, "suspend\n");

	mutex_lock(&nd->lock);

	if (dssdev->state != OMAP_DSS_DISPLAY_ACTIVE) {
		r = -EINVAL;
		goto err;
	}

	dsi_bus_lock(dssdev);

	nt71391_power_off(dssdev);

	dsi_bus_unlock(dssdev);

	dssdev->state = OMAP_DSS_DISPLAY_SUSPENDED;

	mutex_unlock(&nd->lock);

return 0;
err:
	mutex_unlock(&nd->lock);
	return r;
}

static int nt71391_resume(struct omap_dss_device *dssdev)
{
	struct nt71391_data *nd = dev_get_drvdata(&dssdev->dev);
	int r = 0;

	if(first_suspend)
	{
		dev_dbg(&dssdev->dev, "not resuming now\n");
		first_suspend = 0;
		return 0;
	}

	dev_dbg(&dssdev->dev, "resume\n");
	mutex_lock(&nd->lock);

	if (dssdev->state != OMAP_DSS_DISPLAY_SUSPENDED) {
		r = -EINVAL;
		goto err;
	}

	dsi_bus_lock(dssdev);

	r = nt71391_power_on(dssdev);

	dsi_bus_unlock(dssdev);

	if (r) {
		dssdev->state = OMAP_DSS_DISPLAY_DISABLED;
	} else {
		dssdev->state = OMAP_DSS_DISPLAY_ACTIVE;
	}

	mutex_unlock(&nd->lock);
	return r;
err:
	mutex_unlock(&nd->lock);
	return r;
}

static struct omap_dss_driver nt71391_driver = {
	.probe		= nt71391_probe,
	.remove		= nt71391_remove,

	.enable		= nt71391_enable,
	.disable	= nt71391_disable,
	.suspend	= nt71391_suspend,
	.resume		= nt71391_resume,

	.get_resolution	= nt71391_get_resolution,
	.get_recommended_bpp = omapdss_default_get_recommended_bpp,

	.get_timings	= nt71391_get_timings,
	.set_timings	= nt71391_set_timings,
	.check_timings	= nt71391_check_timings,

	.driver         = {
		.name   = "nt71391",
		.owner  = THIS_MODULE,
	},
};

static int __init nt71391_init(void)
{
	int r;

	r = omap_dss_register_driver(&nt71391_driver);
	if (r < 0) {
		printk("nt71391 driver registration failed\n");
		return r;
	}

	return 0;
}

static void __exit nt71391_exit(void)
{
	omap_dss_unregister_driver(&nt71391_driver);
}

module_init(nt71391_init);
module_exit(nt71391_exit);

MODULE_AUTHOR("Subbaraman Narayanamurthy <x0164410@ti.com>");
MODULE_DESCRIPTION("NT71391 MIPI DSI Driver");
MODULE_LICENSE("GPL");
