/*
 * pmic_thermal.c
 *
 * Copyright (C) Amazon Technologies Inc. All rights reserved.
 * Donald Chan (hoiho@lab126.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/module.h>
#include <linux/hwmon.h>
#include <linux/hwmon-sysfs.h>
#include <linux/platform_device.h>
#include <linux/sysfs.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/i2c/twl.h>
#include <linux/i2c/twl6030-gpadc.h>

#include <mach/omap4-common.h>

#include <plat/control.h>

/* NTC mapping table for battery */
static int temp_table_battery[] = {
	/* -20 to -11 C */
	852, 850, 847, 844, 841, 838, 834, 831, 827, 822,
	/* -10 to -1 C */
	818, 814, 809, 805, 799, 793, 789, 783, 778, 770,
	/* 0 to 9 C */
	765, 757, 752, 744, 737, 730, 723, 714, 707, 698,
	/* 10 to 19 C */
	690, 681, 672, 664, 655, 645, 636, 626, 616, 608,
	/* 20 to 29 C */
	598, 588, 577, 567, 557, 547, 536, 526, 516, 505,
	/* 30 to 39 C */
	495, 484, 474, 464, 454, 444, 433, 423, 413, 403,
	/* 40 to 49 C */
	393, 383, 374, 364, 356, 346, 337, 328, 320, 311,
	/* 50 to 59 C */
	302, 294, 285, 278, 269, 262, 255, 248, 240, 233,
	/* 60 to 69 C */
	226, 220, 213, 207, 202, 194, 189, 183, 179, 173,
	/* 70 to 79 C */
	167, 163, 157, 153, 148, 144, 140, 135, 131, 127,
	/* 80 to 89 C */
	124, 120, 117, 112, 109, 107, 102,  99,  96,  94,
	/* 90 to 99 C */
	 91,  88,  86,  84,  81,  79,  76,  73,  72,  71,
	/* 100 to 105 C */
	 68,  66,  65,  62,  60,  59
};

/* NTC mapping table for charger IC */
static int temp_table_charger[] = {
	/* -15 C to -11 C */
	954, 952, 951, 949, 948,
	/* -10 C to -1 C */
	946, 944, 942, 940, 938, 936, 933, 931, 929, 926,
	/* 0 C to 9 C */
	923, 921, 918, 915, 911, 908, 905, 901, 898, 894,
	/* 10 C to 19 C */
	890, 886, 882, 877, 873, 868, 863, 858, 853, 848,
	/* 20 C to 29 C */
	843, 837, 831, 826, 819, 813, 807, 801, 794, 787,
	/* 30 C to 39 C */
	780, 773, 766, 759, 751, 744, 736, 728, 720, 712,
	/* 40 C to 49 C */
	704, 696, 688, 679, 671, 662, 654, 645, 636, 627,
	/* 50 C to 59 C */
	619, 610, 601, 592, 583, 574, 565, 556, 547, 538,
	/* 60 C to 69 C */
	529, 520, 511, 503, 494, 485, 476, 468, 459, 450,
	/* 70 C to 79 C */
	442, 434, 425, 417, 409, 401, 393, 385, 377, 370,
	/* 80 C to 89 C */
	362, 355, 348, 341, 334, 326, 319, 313, 306, 299,
	/* 90 C to 99 C */
	293, 287, 281, 274, 268, 262, 257, 251, 246, 240,
	/* 100 C to 105 C */
	235, 230, 224, 219, 214, 209,
};

extern u8 quanta_get_mbid(void);

static ssize_t show_name(struct device *dev, struct device_attribute *attr,
			char *buf)
{
	return sprintf(buf, "pmic_thermal\n");
}

static DEVICE_ATTR(name, S_IRUGO, show_name, NULL);

static ssize_t show_label(struct device *dev, struct device_attribute *attr,
			char *buf)
{
	int index = to_sensor_dev_attr(attr)->index;

	return sprintf(buf, "%s\n", index == 0 ? "battery" : "charger");
}

static SENSOR_DEVICE_ATTR(temp1_label, S_IRUGO,
			  show_label, NULL, 0);
static SENSOR_DEVICE_ATTR(temp2_label, S_IRUGO,
			  show_label, NULL, 1);


static ssize_t show_input(struct device *dev, struct device_attribute *attr,
			  char *buf)
{
	int index = to_sensor_dev_attr(attr)->index;
	int channel = (index == 0 ? 1 : 4);
	int i = 0;
	int temperature = 0;
	int table_size = (index == 0 ?
		ARRAY_SIZE(temp_table_battery) :
			ARRAY_SIZE(temp_table_charger));
	int *table = (index == 0 ?
		temp_table_battery : temp_table_charger);
	int offset = (index == 0 ? -20 : -15);

	struct twl6030_gpadc_request req;

	req.channels = 1 << channel;
	req.method = TWL6030_GPADC_SW2;
	req.active = 0;
	req.func_cb = NULL;

	/* Channel 1 is invalid for mbid >= 5 */
	if (quanta_get_mbid() >= 5 && index == 0)
		return sprintf(buf, "-1\n");

	if (twl6030_gpadc_conversion(&req) < 0) {
		return sprintf(buf, "-1\n");
	}

	for (i = 0; i < table_size; i++) {
		if (req.buf[channel].raw_code < table[i]) {
			temperature = offset + i;
		} else {
			break;
		}
	}

	return sprintf(buf, "%d\n", temperature * 1000);
}

static SENSOR_DEVICE_ATTR(temp1_input, S_IRUGO,
			  show_input, NULL, 0);
static SENSOR_DEVICE_ATTR(temp2_input, S_IRUGO,
			  show_input, NULL, 1);

static struct attribute *pmic_thermal_sensor_attributes[] = {
	&dev_attr_name.attr,
	&sensor_dev_attr_temp1_label.dev_attr.attr,
	&sensor_dev_attr_temp2_label.dev_attr.attr,
	&sensor_dev_attr_temp1_input.dev_attr.attr,
	&sensor_dev_attr_temp2_input.dev_attr.attr,
	NULL
};

static const struct attribute_group pmic_thermal_sensor_group = {
	.attrs = pmic_thermal_sensor_attributes,
};

static int __devinit pmic_thermal_sensor_probe(struct platform_device *pdev)
{
	int ret;

	ret = sysfs_create_group(&pdev->dev.kobj, &pmic_thermal_sensor_group);

	if (ret)
		return ret;

	hwmon_device_register(&pdev->dev);

	return 0;
}

static int __devexit pmic_thermal_sensor_remove(struct platform_device *pdev)
{
	hwmon_device_unregister(&pdev->dev);

	sysfs_remove_group(&pdev->dev.kobj, &pmic_thermal_sensor_group);

	platform_set_drvdata(pdev, NULL);

	return 0;
}

static struct platform_driver pmic_thermal_sensor_driver = {
	.probe = pmic_thermal_sensor_probe,
	.remove = __devexit_p(pmic_thermal_sensor_remove),
	.driver = {
		.name = "pmic_thermal_sensor",
		.owner = THIS_MODULE,
	},
};

static int __init pmic_thermal_sensor_init(void)
{
	return platform_driver_register(&pmic_thermal_sensor_driver);
}
module_init(pmic_thermal_sensor_init);

static void __exit pmic_thermal_sensor_exit(void)
{
	platform_driver_unregister(&pmic_thermal_sensor_driver);
}
module_exit(pmic_thermal_sensor_exit);

MODULE_AUTHOR("Donald Chan <hoiho@lab126.com>");
MODULE_DESCRIPTION("PMIC Thermal Sensor");
MODULE_LICENSE("GPL");
