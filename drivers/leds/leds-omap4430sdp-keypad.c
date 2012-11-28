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
#include <linux/module.h>
#include <linux/slab.h>

#include <linux/i2c/twl.h>
#include <plat/led.h>

struct keypad_led_data {
	struct led_classdev keypad_led_class_dev;
    struct timer_list timer;
    struct work_struct work;
    int delay_on;
    int delay_off;
    int blink_step;
    int brightness;
};

#define KP_LED_PWM1ON		0x00
#define KP_LED_PWM1OFF		0x01
#define KP_LED_TOGGLE3		0x92

struct keypad_led_data *g_green_led_data;

void omap4430_green_led_set(struct led_classdev *led_cdev,
				enum led_brightness value)
{
	u8 brightness = 0;

	flush_work(&g_green_led_data->work);
	del_timer_sync(&g_green_led_data->timer);

	if (value > 1) {
		if (value == LED_FULL)
			brightness = 0x7f;
		else
			brightness = (~(value/2)) & 0x7f;

		twl_i2c_write_u8(TWL_MODULE_PWM, brightness, KP_LED_PWM1ON);
		twl_i2c_write_u8(TWL6030_MODULE_ID1, 0x06, KP_LED_TOGGLE3);
	} else {
		twl_i2c_write_u8(TWL6030_MODULE_ID1, 0x01, KP_LED_TOGGLE3);
		twl_i2c_write_u8(TWL6030_MODULE_ID1, 0x07, KP_LED_TOGGLE3);
	}

}
EXPORT_SYMBOL(omap4430_green_led_set);

static void omap4430_keypad_led_work(struct work_struct *work)
{
	struct keypad_led_data *led_data = container_of(work, struct keypad_led_data, work);
	u8 brightness = 0;

	if (led_data->brightness == 0)
		led_data->blink_step = 1;
	else if (led_data->brightness == 255)
		led_data->blink_step = -1;

	led_data->brightness += led_data->blink_step;

	if (led_data->blink_step == 1) {
		if (led_data->brightness > 1) {
			if (led_data->brightness == LED_FULL)
				brightness = 0x7f;
			else
				brightness = (~(led_data->brightness/2)) & 0x7f;
			twl_i2c_write_u8(TWL_MODULE_PWM, brightness, KP_LED_PWM1ON);
			twl_i2c_write_u8(TWL6030_MODULE_ID1, 0x06, KP_LED_TOGGLE3);
		} else {
			twl_i2c_write_u8(TWL6030_MODULE_ID1, 0x01, KP_LED_TOGGLE3);
			twl_i2c_write_u8(TWL6030_MODULE_ID1, 0x07, KP_LED_TOGGLE3);
		}
		mod_timer(&led_data->timer, jiffies + msecs_to_jiffies(led_data->delay_on));
	} else {
		if (led_data->brightness > 1) {
			if (led_data->brightness == LED_FULL)
				brightness = 0x7f;
			else
				brightness = (~(led_data->brightness/2)) & 0x7f;
			twl_i2c_write_u8(TWL_MODULE_PWM, brightness, KP_LED_PWM1ON);
			twl_i2c_write_u8(TWL6030_MODULE_ID1, 0x06, KP_LED_TOGGLE3);
		} else {
			twl_i2c_write_u8(TWL6030_MODULE_ID1, 0x01, KP_LED_TOGGLE3);
			twl_i2c_write_u8(TWL6030_MODULE_ID1, 0x07, KP_LED_TOGGLE3);
		}
		mod_timer(&led_data->timer, jiffies + msecs_to_jiffies(led_data->delay_off));
	}
}

static void omap4430_keypad_led_timer(unsigned long data)
{
	struct keypad_led_data *led_data = (struct keypad_led_data *) data;

	schedule_work(&led_data->work);
}

int omap4430_green_led_set_blink(struct led_classdev *led_cdev, 
        unsigned long *delay_on, unsigned long *delay_off)
{
	if (*delay_on == 0 || *delay_off == 0)
		return -1;

	g_green_led_data->delay_on = *delay_on;
	g_green_led_data->delay_off = *delay_off;
	g_green_led_data->brightness = 0;
	omap4430_green_led_set(&g_green_led_data->keypad_led_class_dev, g_green_led_data->brightness);
	mod_timer(&g_green_led_data->timer, jiffies + msecs_to_jiffies(g_green_led_data->delay_on));

	return 0;
}
EXPORT_SYMBOL(omap4430_green_led_set_blink);

static int omap4430_keypad_led_probe(struct platform_device *pdev)
{
	int ret;
	struct keypad_led_data *info;

	info = kzalloc(sizeof(struct keypad_led_data), GFP_KERNEL);
	if (info == NULL) {
		ret = -ENOMEM;
		return ret;
	}
	g_green_led_data = info;

	platform_set_drvdata(pdev, info);

	info->keypad_led_class_dev.name = "led-green";
	info->keypad_led_class_dev.brightness_set =
			omap4430_green_led_set;
	info->keypad_led_class_dev.max_brightness = LED_FULL;
	info->keypad_led_class_dev.brightness = LED_OFF;
	info->keypad_led_class_dev.blink_set = omap4430_green_led_set_blink;
	init_timer(&info->timer);
	info->timer.function = omap4430_keypad_led_timer;
	info->timer.data = (unsigned long) info;
	INIT_WORK(&info->work, omap4430_keypad_led_work);

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
	//twl_i2c_write_u8(TWL6030_MODULE_ID1, 0x06, KP_LED_TOGGLE3);
	twl_i2c_write_u8(TWL6030_MODULE_ID1, 0x01, KP_LED_TOGGLE3);
	twl_i2c_write_u8(TWL6030_MODULE_ID1, 0x07, KP_LED_TOGGLE3);

	return ret;
}

static int omap4430_keypad_led_remove(struct platform_device *pdev)
{
	struct keypad_led_data *info = platform_get_drvdata(pdev);
	led_classdev_unregister(&info->keypad_led_class_dev);
	flush_work(&info->work);
	del_timer_sync(&info->timer);
	return 0;
}

static int omap4430_keypad_led_suspend(struct platform_device *pdev, pm_message_t state)
{
	printk("!!!!!!!%s!!!!!!!!!!\n",__func__);
	struct keypad_led_data *info = platform_get_drvdata(pdev);
	omap4430_green_led_set(&info->keypad_led_class_dev,LED_OFF);
	return 0;
}

static int omap4430_keypad_led_resume(struct platform_device *pdev)
{
	struct keypad_led_data *info = platform_get_drvdata(pdev);
	omap4430_green_led_set(&info->keypad_led_class_dev,LED_OFF);
	printk("!!!!!!!%s!!!!!!!!!!\n",__func__);
	return 0;
}

static struct platform_driver omap4430_keypad_led_driver = {
	.probe = omap4430_keypad_led_probe,
	.remove = omap4430_keypad_led_remove,
	.suspend	= omap4430_keypad_led_suspend,
	.resume		= omap4430_keypad_led_resume,
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

subsys_initcall(omap4430_keypad_led_init);
module_exit(omap4430_keypad_led_exit);

MODULE_DESCRIPTION("OMAP4430 SDP Keypad Lighting driver");
MODULE_AUTHOR("Dan Murphy <dmurphy@ti.com");
MODULE_LICENSE("GPL");
