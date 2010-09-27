/*
 * Phoenix Keypad LED Driver for the OMAP4430 SDP
 *
 * Copyright (C) 2010 Texas Instruments
 *
 * Author: Dan Murphy <DMurphy@ti.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#include <linux/gpio.h>
#include <linux/leds.h>
#include <linux/platform_device.h>
#include <linux/slab.h>

#include <linux/i2c/twl.h>

struct keypad_led_data {
	struct led_classdev keypad_led_class_dev;
};

#define KP_LED_PWM1ON		0x00
#define KP_LED_PWM1OFF		0x01
#define KP_LED_TOGGLE3		TWL6030_TOGGLE3

static void omap4430_keypad_led_store(struct led_classdev *led_cdev,
				enum led_brightness value)
{
	u8 brightness = 0;

	if (value > 1) {
		if (value == LED_FULL)
			brightness = 0x7f;
		else
			brightness = (~(value/2)) & 0x7f;

		twl_i2c_write_u8(TWL_MODULE_PWM, brightness, KP_LED_PWM1ON);
		twl_i2c_write_u8(TWL6030_MODULE_ID1, 0x36, TWL6030_TOGGLE3);
	} else {
		twl_i2c_write_u8(TWL6030_MODULE_ID1, 0x31, TWL6030_TOGGLE3);
		twl_i2c_write_u8(TWL6030_MODULE_ID1, 0x37, TWL6030_TOGGLE3);
	}

}

static int omap4430_keypad_led_probe(struct platform_device *pdev)
{
	int ret;
	struct keypad_led_data *info;

	pr_info("%s:Enter\n", __func__);

	info = kzalloc(sizeof(struct keypad_led_data), GFP_KERNEL);
	if (info == NULL) {
		ret = -ENOMEM;
		return ret;
	}

	platform_set_drvdata(pdev, info);

	info->keypad_led_class_dev.name = "keyboard-backlight";
	info->keypad_led_class_dev.brightness_set =
			omap4430_keypad_led_store;
	info->keypad_led_class_dev.max_brightness = LED_FULL;

	ret = led_classdev_register(&pdev->dev,
				    &info->keypad_led_class_dev);
	if (ret < 0) {
		pr_err("%s: Register led class failed\n", __func__);
		kfree(info);
		return ret;
	}

	/*TO DO pass in these values from the board file */
	twl_i2c_write_u8(TWL_MODULE_PWM, 0xFF, KP_LED_PWM1ON);
	twl_i2c_write_u8(TWL_MODULE_PWM, 0x7f, KP_LED_PWM1OFF);
	twl_i2c_write_u8(TWL6030_MODULE_ID1, 0x06, TWL6030_TOGGLE3);

	pr_info("%s:Exit\n", __func__);

	return ret;
}

static int omap4430_keypad_led_remove(struct platform_device *pdev)
{
	struct keypad_led_data *info = platform_get_drvdata(pdev);
	led_classdev_unregister(&info->keypad_led_class_dev);
	return 0;
}

static struct platform_driver omap4430_keypad_led_driver = {
	.probe = omap4430_keypad_led_probe,
	.remove = omap4430_keypad_led_remove,
	.driver = {
		   .name = "keypad_led",
		   .owner = THIS_MODULE,
		   },
};

static int __init omap4430_keypad_led_init(void)
{
	return platform_driver_register(&omap4430_keypad_led_driver);
}

static void __exit omap4430_keypad_led_exit(void)
{
	platform_driver_unregister(&omap4430_keypad_led_driver);
}

module_init(omap4430_keypad_led_init);
module_exit(omap4430_keypad_led_exit);

MODULE_DESCRIPTION("OMAP4430 SDP Keypad Lighting driver");
MODULE_AUTHOR("Dan Murphy <dmurphy@ti.com");
MODULE_LICENSE("GPL");
