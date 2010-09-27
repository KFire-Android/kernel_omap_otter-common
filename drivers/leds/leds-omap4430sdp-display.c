/*
 * OMAP4430 SDP display LED Driver
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
#include <linux/leds-omap4430sdp-display.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/i2c/twl.h>

#define OMAP4430_LED_DEBUG 0

struct display_led_data {
	struct led_classdev pri_display_class_dev;
	struct led_classdev sec_display_class_dev;
	struct omap4430_sdp_disp_led_platform_data *led_pdata;
	struct mutex pri_disp_lock;
	struct mutex sec_disp_lock;
};

#if OMAP4430_LED_DEBUG

#define DBG_LED_PWM2ON		0x03
#define DBG_LED_PWM2OFF		0x04
#define DBG_LED_TOGGLE3		TWL6030_TOGGLE3

struct omap4430_sdp_reg {
	const char *name;
	uint8_t reg;
} omap4430_sdp_regs[] = {
	{ "PWM2ON",	DBG_LED_PWM2ON },
	{ "PWM2OFF",	DBG_LED_PWM2OFF },
	{ "TOGGLE3",	DBG_LED_TOGGLE3 },
};
#endif

static void omap4430_sdp_primary_disp_store(struct led_classdev *led_cdev,
				enum led_brightness value)
{
	struct display_led_data *led_data = container_of(led_cdev,
			struct display_led_data, pri_display_class_dev);
	mutex_lock(&led_data->pri_disp_lock);

	if (led_data->led_pdata->primary_display_set)
		led_data->led_pdata->primary_display_set(value);

	mutex_unlock(&led_data->pri_disp_lock);
}

static void omap4430_sdp_secondary_disp_store(struct led_classdev *led_cdev,
				enum led_brightness value)
{
	struct display_led_data *led_data = container_of(led_cdev,
			struct display_led_data, sec_display_class_dev);

	mutex_lock(&led_data->sec_disp_lock);

	if ((led_data->led_pdata->flags & LEDS_CTRL_AS_TWO_DISPLAYS) &&
		led_data->led_pdata->secondary_display_set)
			led_data->led_pdata->secondary_display_set(value);

	mutex_unlock(&led_data->sec_disp_lock);
}

#if OMAP4430_LED_DEBUG
static ssize_t ld_omap4430_sdp_registers_show(struct device *dev,
			      struct device_attribute *attr, char *buf)
{
	unsigned i, n, reg_count;
	uint8_t value;

	reg_count = sizeof(omap4430_sdp_regs) / sizeof(omap4430_sdp_regs[0]);
	for (i = 0, n = 0; i < reg_count; i++) {
		twl_i2c_read_u8(TWL_MODULE_PWM, &value, omap4430_sdp_regs[i].reg);
		n += scnprintf(buf + n, PAGE_SIZE - n,
			       "%-20s = 0x%02X\n",
			       omap4430_sdp_regs[i].name,
			       value);
	}

	return n;
}

static ssize_t ld_omap4430_sdp_registers_store(struct device *dev,
			       struct device_attribute *attr,
			       const char *buf, size_t count)
{
	unsigned i, reg_count, value;
	int error;
	char name[30];

	if (count >= 30) {
		pr_err("%s:input too long\n", __func__);
		return -1;
	}

	if (sscanf(buf, "%s %x", name, &value) != 2) {
		pr_err("%s:unable to parse input\n", __func__);
		return -1;
	}

	reg_count = sizeof(omap4430_sdp_regs) / sizeof(omap4430_sdp_regs[0]);
	for (i = 0; i < reg_count; i++) {
		if (!strcmp(name, omap4430_sdp_regs[i].name)) {
			if (!strcmp("TOGGLE3", omap4430_sdp_regs[i].name)) {
				error = twl_i2c_write_u8(TWL6030_MODULE_ID1,
					value,
					omap4430_sdp_regs[i].reg);

			} else {
				error = twl_i2c_write_u8(TWL_MODULE_PWM, value,
					omap4430_sdp_regs[i].reg);
			}
			if (error) {
				pr_err("%s:Failed to write register %s\n",
					__func__, name);
				return -1;
			}
			return count;
		}
	}
	pr_err("%s:no such register %s\n", __func__, name);
	return -1;
}

static DEVICE_ATTR(registers, 0644, ld_omap4430_sdp_registers_show,
		ld_omap4430_sdp_registers_store);
#endif

static int omap4430_sdp_display_probe(struct platform_device *pdev)
{
	int ret;
	struct display_led_data *info;

	pr_info("%s:Enter\n", __func__);

	if (pdev->dev.platform_data == NULL) {
		pr_err("%s: platform data required\n", __func__);
		return -ENODEV;
	}

	info = kzalloc(sizeof(struct display_led_data), GFP_KERNEL);
	if (info == NULL) {
		ret = -ENOMEM;
		return ret;
	}

	info->led_pdata = pdev->dev.platform_data;
	platform_set_drvdata(pdev, info);

	info->pri_display_class_dev.name = "lcd-backlight";
	info->pri_display_class_dev.brightness_set = omap4430_sdp_primary_disp_store;
	info->pri_display_class_dev.max_brightness = LED_FULL;
	mutex_init(&info->pri_disp_lock);

	ret = led_classdev_register(&pdev->dev,
				    &info->pri_display_class_dev);
	if (ret < 0) {
		pr_err("%s: Register led class failed\n", __func__);
		kfree(info);
		return ret;
	}

	if (info->led_pdata->flags & LEDS_CTRL_AS_TWO_DISPLAYS) {
		pr_info("%s: Configuring the secondary LED\n", __func__);
		info->sec_display_class_dev.name = "lcd-backlight2";
		info->sec_display_class_dev.brightness_set =
			omap4430_sdp_secondary_disp_store;
		info->sec_display_class_dev.max_brightness = LED_OFF;
		mutex_init(&info->sec_disp_lock);

		ret = led_classdev_register(&pdev->dev,
					    &info->sec_display_class_dev);
		if (ret < 0) {
			pr_err("%s: Register led class failed\n", __func__);
			kfree(info);
			return ret;
		}

		if (info->led_pdata->secondary_display_set)
			info->led_pdata->secondary_display_set(0);
	}

#if OMAP4430_LED_DEBUG
	ret = device_create_file(info->pri_display_class_dev.dev,
			&dev_attr_registers);
	if (ret < 0) {
		pr_err("%s: Could not register registers fd \n",
			__func__);

	}
#endif
	if (info->led_pdata->display_led_init)
		info->led_pdata->display_led_init();

	pr_info("%s:Exit\n", __func__);

	return ret;
}

static int omap4430_sdp_display_remove(struct platform_device *pdev)
{
	struct display_led_data *info = platform_get_drvdata(pdev);
#if OMAP4430_LED_DEBUG
	device_remove_file(info->pri_display_class_dev.dev,
			&dev_attr_registers);
#endif
	led_classdev_unregister(&info->pri_display_class_dev);
	if (info->led_pdata->flags & LEDS_CTRL_AS_TWO_DISPLAYS)
		led_classdev_unregister(&info->sec_display_class_dev);

	return 0;
}

static struct platform_driver omap4430_sdp_display_driver = {
	.probe = omap4430_sdp_display_probe,
	.remove = omap4430_sdp_display_remove,
	.driver = {
		   .name = "display_led",
		   .owner = THIS_MODULE,
		   },
};

static int __init omap4430_sdp_display_init(void)
{
	return platform_driver_register(&omap4430_sdp_display_driver);
}

static void __exit omap4430_sdp_display_exit(void)
{
	platform_driver_unregister(&omap4430_sdp_display_driver);
}

module_init(omap4430_sdp_display_init);
module_exit(omap4430_sdp_display_exit);

MODULE_DESCRIPTION("OMAP4430 SDP Display Lighting driver");
MODULE_AUTHOR("Dan Murphy <dmurphy@ti.com");
MODULE_LICENSE("GPL");
