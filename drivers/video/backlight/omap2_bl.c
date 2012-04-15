/*
 * Backlight driver for OMAP2 based boards.
 *
 * Copyright (c) 2006 Andrzej Zaborowski  <balrog@zabor.org>
 *
 * This package is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This package is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this package; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/fb.h>
#include <linux/backlight.h>
#include <linux/slab.h>
#include <linux/leds.h>

#include <mach/hardware.h>
#include <plat/board.h>
#include <plat/mux.h>
#include <plat/dmtimer.h>

#define OMAPBL_MAX_INTENSITY		0xff

struct omap_backlight {
	int powermode;
	int current_intensity;

	struct omap_dm_timer *intensity_timer;
	struct device *dev;
	struct omap_backlight_config *pdata;
};

static void inline omapbl_send_intensity(struct omap_backlight *bl, int intensity)
{
	pr_info(">> omap2_bl :: omapbl_send_intensity(intensity == %d)\n", intensity);
	if (intensity == LED_FULL) {
		omap_dm_timer_set_pwm(bl->intensity_timer, 1, 1, OMAP_TIMER_TRIGGER_OVERFLOW_AND_COMPARE);
		omap_dm_timer_set_match(bl->intensity_timer, 1, 0xffffff00);
	} else {
		omap_dm_timer_set_pwm(bl->intensity_timer, 0, 1, OMAP_TIMER_TRIGGER_OVERFLOW_AND_COMPARE);
		omap_dm_timer_set_match(bl->intensity_timer, 1, (0xffffff00) | intensity);
	}
}

static void omapbl_blank(struct omap_backlight *bl, int mode)
{
	switch (mode) {
	case FB_BLANK_NORMAL:
	case FB_BLANK_VSYNC_SUSPEND:
	case FB_BLANK_HSYNC_SUSPEND:
	case FB_BLANK_POWERDOWN:
		pr_info(">> omap2_bl :: omapbl_blank(OFF, mode == %d)\n", mode);
		/* Everything off */
		omap_dm_timer_set_pwm(bl->intensity_timer, 0, 1,
				      OMAP_TIMER_TRIGGER_OVERFLOW_AND_COMPARE);
		omap_dm_timer_stop(bl->intensity_timer);
		omap_dm_timer_disable(bl->intensity_timer);

		if (bl->pdata->set_power)
			bl->pdata->set_power(bl->dev, mode);

		// omapbl_send_intensity(0);
		break;

	case FB_BLANK_UNBLANK:
		pr_info(">> omap2_bl :: omapbl_blank(ON, mode == %d, bl->current_intensity == %d)\n", mode, bl->current_intensity);
		/* Select clock */
		omap_dm_timer_enable(bl->intensity_timer);
		omap_dm_timer_set_source(bl->intensity_timer, OMAP_TIMER_SRC_SYS_CLK);
		omap_dm_timer_set_prescaler(bl->intensity_timer, 0);

		if (bl->pdata->set_power)
			bl->pdata->set_power(bl->dev, mode);

		omap_dm_timer_set_load(bl->intensity_timer, 1, 0xffffff00);
		omap_dm_timer_start(bl->intensity_timer);
		omapbl_send_intensity(bl, bl->current_intensity);
		break;
	}
}

#ifdef CONFIG_PM
static int omapbl_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct backlight_device *dev = platform_get_drvdata(pdev);
	struct omap_backlight *bl = dev_get_drvdata(&dev->dev);

	omapbl_blank(bl, FB_BLANK_POWERDOWN);
	return 0;
}

static int omapbl_resume(struct platform_device *pdev)
{
	struct backlight_device *dev = platform_get_drvdata(pdev);
	struct omap_backlight *bl = dev_get_drvdata(&dev->dev);

	omapbl_blank(bl, bl->powermode);
	return 0;
}
#else
#define omapbl_suspend	NULL
#define omapbl_resume	NULL
#endif

static int omapbl_set_power(struct backlight_device *dev, int state)
{
	struct omap_backlight *bl = dev_get_drvdata(&dev->dev);

	omapbl_blank(bl, state);
	bl->powermode = state;

	return 0;
}

static int omapbl_update_status(struct backlight_device *dev)
{
	struct omap_backlight *bl = dev_get_drvdata(&dev->dev);

	/* FIXME-HASH: temporary hack to set max brightness [by intersectRaven] */
	if (dev->props.brightness)
		dev->props.brightness = 255;

	pr_info(">> omap2_bl :: omapbl_update_status(brightness == %d, fb_blank == %d)\n", dev->props.brightness, dev->props.fb_blank);
	if (bl->current_intensity != dev->props.brightness) {
		bl->current_intensity = dev->props.brightness;
		if (bl->powermode == FB_BLANK_UNBLANK)
			omapbl_send_intensity(bl, bl->current_intensity);
	}

	if (dev->props.fb_blank != bl->powermode)
		omapbl_set_power(dev, dev->props.fb_blank);

	return 0;
}

static int omapbl_get_intensity(struct backlight_device *dev)
{
	struct omap_backlight *bl = dev_get_drvdata(&dev->dev);
	pr_info(">> omap2_bl :: omapbl_get_intensity(brightness == %d\n", bl->current_intensity);
	return bl->current_intensity;
}

static const struct backlight_ops omapbl_ops = {
	.get_brightness = omapbl_get_intensity,
	.update_status  = omapbl_update_status,
};

static int omapbl_probe(struct platform_device *pdev)
{
	struct backlight_properties props;
	struct backlight_device *dev;
	struct omap_backlight *bl;
	struct omap_backlight_config *pdata = pdev->dev.platform_data;

	if (!pdata)
		return -ENXIO;

	bl = kzalloc(sizeof(struct omap_backlight), GFP_KERNEL);
	if (unlikely(!bl))
		return -ENOMEM;

	memset(&props, 0, sizeof(struct backlight_properties));
	props.type = BACKLIGHT_RAW;
	props.max_brightness = OMAPBL_MAX_INTENSITY;
	dev = backlight_device_register("omap-bl", &pdev->dev, bl, &omapbl_ops, &props);
	if (IS_ERR(dev)) {
		kfree(bl);
		return PTR_ERR(dev);
	}

	bl->powermode = FB_BLANK_POWERDOWN;
	bl->current_intensity = 0;

	bl->pdata = pdata;
	bl->dev = &pdev->dev;

	/* get related dm timers */
	bl->intensity_timer = omap_dm_timer_request_specific(10); // pdata->intensity_timer
	if (bl->intensity_timer == NULL) {
		dev_err(&pdev->dev, "failed to request intensity pwm timer\n");
		kfree(bl);
		return -ENODEV;
	}

	platform_set_drvdata(pdev, dev);

	dev->props.fb_blank = FB_BLANK_UNBLANK;
	dev->props.brightness = pdata->default_intensity;
	omapbl_update_status(dev);

	printk(KERN_INFO "OMAP LCD backlight initialised\n");

	return 0;
}

static int omapbl_remove(struct platform_device *pdev)
{
	struct backlight_device *dev = platform_get_drvdata(pdev);
	struct omap_backlight *bl = dev_get_drvdata(&dev->dev);

	omapbl_blank(bl, FB_BLANK_POWERDOWN);
	backlight_device_unregister(dev);
	omap_dm_timer_free(bl->intensity_timer);
	kfree(bl);

	return 0;
}

static struct platform_driver omapbl_driver = {
	.probe		= omapbl_probe,
	.remove		= omapbl_remove,
	.suspend	= omapbl_suspend,
	.resume		= omapbl_resume,
	.driver		= {
		.name	= "omap-bl",
	},
};

static int __init omapbl_init(void)
{
	return platform_driver_register(&omapbl_driver);
}

static void __exit omapbl_exit(void)
{
	platform_driver_unregister(&omapbl_driver);
}

module_init(omapbl_init);
module_exit(omapbl_exit);

MODULE_AUTHOR("Andrzej Zaborowski <balrog@zabor.org>");
MODULE_DESCRIPTION("OMAP LCD Backlight driver");
MODULE_LICENSE("GPL");
