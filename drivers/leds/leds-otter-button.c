/*
 * Kindle Fire Button LED driver (based on OMAP4430 SDP display LED Driver)
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

#include <linux/module.h>
#include <linux/gpio.h>
#include <linux/leds.h>
#include <linux/leds-otter-button.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/i2c/twl.h>
#include <plat/led.h>

struct button_led_data {
	struct led_classdev cdev;
	struct otter_button_led_platform_data *pdata;
	struct mutex led_lock;
	struct timer_list timer;
	struct work_struct work;
	int delay_on;
	int delay_off;
	int blink_step;
	int brightness;
};

void otter_button_led_brightness_set(struct led_classdev *led_cdev, enum led_brightness value)
{
	struct button_led_data *led_data = container_of(led_cdev, struct button_led_data, cdev);

	mutex_lock(&led_data->led_lock);
	flush_work(&led_data->work);
	del_timer_sync(&led_data->timer);
	if (led_data->pdata->led_set_brightness)
		led_data->pdata->led_set_brightness(value);
	mutex_unlock(&led_data->led_lock);
}

static void otter_button_led_work(struct work_struct *work)
{
	struct button_led_data *led_data = container_of(work, struct button_led_data, work);

	if (led_data->brightness == 0)
		led_data->blink_step = 1;
	else if (led_data->brightness == 255)
		led_data->blink_step = -1;

	led_data->brightness += led_data->blink_step;

	if (led_data->blink_step == 1) {
		led_data->pdata->led_set_brightness(led_data->brightness);
		mod_timer(&led_data->timer, jiffies + msecs_to_jiffies(led_data->delay_on));
	} else {
		led_data->pdata->led_set_brightness(led_data->brightness);
		mod_timer(&led_data->timer, jiffies + msecs_to_jiffies(led_data->delay_off));
	}
}

static void otter_button_led_timer(unsigned long data)
{
	struct button_led_data *led_data = (struct button_led_data *)data;
	schedule_work(&led_data->work);
}

int otter_button_led_blink_set(struct led_classdev *led_cdev, unsigned long *delay_on, unsigned long *delay_off)
{
	struct button_led_data *led_data = container_of(led_cdev, struct button_led_data, cdev);

	if (*delay_on == 0 || *delay_off == 0)
		return -1;

	led_data->delay_on = *delay_on;
	led_data->delay_off = *delay_off;
	led_data->brightness = 0;
	led_data->pdata->led_set_brightness(led_data->brightness);
	mod_timer(&led_data->timer, jiffies + msecs_to_jiffies(led_data->delay_on));

	return 0;
}

static int otter_button_led_probe(struct platform_device *pdev)
{
	int ret;
	struct button_led_data *info;

	if (pdev->dev.platform_data == NULL) {
		pr_err("%s: platform data required\n", __func__);
		return -ENODEV;
	}

	info = kzalloc(sizeof(struct button_led_data), GFP_KERNEL);
	if (info == NULL) {
		ret = -ENOMEM;
		return ret;
	}
	info->pdata = pdev->dev.platform_data;
	dev_set_drvdata(&pdev->dev, info);

	info->cdev.name = info->pdata->name;
	info->cdev.brightness_set = otter_button_led_brightness_set;
	info->cdev.blink_set = otter_button_led_blink_set;
	info->cdev.max_brightness = LED_FULL;
	info->cdev.brightness = LED_OFF;
	mutex_init(&info->led_lock);
	init_timer(&info->timer);
	info->timer.function = otter_button_led_timer;
	info->timer.data = (unsigned long) info;
	INIT_WORK(&info->work, otter_button_led_work);

	ret = led_classdev_register(&pdev->dev, &info->cdev);
	if (ret < 0) {
		pr_err("%s: Register led class failed\n", __func__);
		kfree(info);
		mutex_destroy(&info->led_lock);
		return ret;
	}

	if (info->pdata->led_init)
		info->pdata->led_init();

	return ret;
}

static int otter_button_led_remove(struct platform_device *pdev)
{
	struct button_led_data *info = platform_get_drvdata(pdev);
	led_classdev_unregister(&info->cdev);
	mutex_destroy(&info->led_lock);
	flush_work(&info->work);
	del_timer_sync(&info->timer);
	kfree(info);
	return 0;
}

static int otter_button_led_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct button_led_data *info = platform_get_drvdata(pdev);
	otter_button_led_brightness_set(&info->cdev, LED_OFF);
	return 0;
}

static int otter_button_led_resume(struct platform_device *pdev)
{
	struct button_led_data *info = platform_get_drvdata(pdev);
	otter_button_led_brightness_set(&info->cdev, LED_OFF);
	return 0;
}

static struct platform_driver otter_button_led_driver = {
	.probe		= otter_button_led_probe,
	.remove		= otter_button_led_remove,
	.suspend	= otter_button_led_suspend,
	.resume		= otter_button_led_resume,
	.driver = {
		.name = "button_led",
		.owner = THIS_MODULE,
	},
};

static int __init otter_button_led_init(void)
{
	return platform_driver_register(&otter_button_led_driver);
}

static void __exit otter_button_led_exit(void)
{
	platform_driver_unregister(&otter_button_led_driver);
}

subsys_initcall(otter_button_led_init);
module_exit(otter_button_led_exit);

MODULE_DESCRIPTION("Kindle Fire Power Button LED driver");
MODULE_AUTHOR("Hashcode <hashcode0f@gmail.com> / Dan Murphy <dmurphy@ti.com>");
MODULE_LICENSE("GPL");
