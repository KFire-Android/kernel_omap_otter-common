/*
 * omap_bandgap.c
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

#include <mach/omap4-common.h>

#include <plat/control.h>

struct omap_bandgap_adc_entry {
	int temp_min;
	int temp_max;
};

static struct omap_bandgap_adc_entry omap_bandgap_adc_table[] = {
	{	-40000,		-40000	},
	{	-40000,		-40000	},
	{	-40000,		-40000	},
	{	-40000,		-40000	},
	{	-40000,		-40000	},
	{	-40000,		-40000	},
	{	-40000,		-40000	},
	{	-40000,		-40000	},
	{	-40000,		-40000	},
	{	-40000,		-40000	},
	{	-40000,		-40000	},
	{	-40000,		-40000	},
	{	-40000,		-40000	},
	{	-40000,		-38000	},
	{	-38000,		-35000	},
	{	-35000,		-34000	},
	{	-34000,		-32000	},
	{	-32000,		-30000	},
	{	-30000,		-28000	},
	{	-28000,		-26000	},
	{	-26000,		-24000	},
	{	-24000,		-22000	},
	{	-22000,		-20000	},
	{	-20000,		-18500	},
	{	-18500,		-17000	},
	{	-17000,		-15000	},
	{	-15000,		-13500	},
	{	-13500,		-12000	},
	{	-12000,		-10000	},
	{	-10000,		-8000	},
	{	-8000,		-6500	},
	{	-5000,		-3500	},
	{	-3500,		-1500	},
	{	-1500,		0	},
	{	0,		2000	},
	{	2000,		3500	},
	{	3500,		5000	},
	{	5000,		6500	},
	{	6500,		8500	},
	{	8500,		10000	},
	{	10000,		12000	},
	{	12000,		13500	},
	{	13500,		15000	},
	{	15000,		17000	},
	{	17000,		19000	},
	{	19000,		21000	},
	{	21000,		23000	},
	{	23000,		25000	},
	{	25000,		27000	},
	{	27000,		28500	},
	{	28500,		30000	},
	{	30000,		32000	},
	{	32000,		33500	},
	{	33500,		35000	},
	{	35000,		37000	},
	{	37000,		38500	},
	{	38500,		40000	},
	{	40000,		42000	},
	{	42000,		43500	},
	{	43500,		45000	},
	{	45000,		47000	},
	{	47000,		48500	},
	{	48500,		50000	},
	{	50000,		52000	},
	{	52000,		53500	},
	{	53500,		55000	},
	{	55000,		57000	},
	{	57000,		58500	},
	{	58500,		60000	},
	{	60000,		62000	},
	{	62000,		64000	},
	{	64000,		66000	},
	{	66000,		68000	},
	{	68000,		70000	},
	{	70000,		71500	},
	{	71500,		73000	},
	{	73500,		75000	},
	{	75000,		77000	},
	{	77000,		78500	},
	{	78500,		80000	},
	{	80000,		82000	},
	{	82000,		83500	},
	{	83500,		85000	},
	{	85000,		87000	},
	{	87000,		88500	},
	{	88500,		90000	},
	{	90000,		92000	},
	{	92000,		93500	},
	{	93500,		95000	},
	{	95000,		97000	},
	{	97000,		98500	},
	{	98500,		100000	},
	{	100000,		102000	},
	{	102000,		103500	},
	{	103500,		105000	},
	{	105000,		107000	},
	{	107000,		109000	},
	{	109000,		111000	},
	{	111000,		113000	},
	{	113000,		115000	},
	{	115000,		117000	},
	{	117000,		118500	},
	{	118500,		120000	},
	{	120000,		122000	},
	{	122000,		123500	},
	{	123500,		125000	},
	{	125000,		125000	},
	{	125000,		125000	},
	{	125000,		125000	},
	{	125000,		125000	},
	{	125000,		125000	},
	{	125000,		125000	},
	{	125000,		125000	},
	{	125000,		125000	},
	{	125000,		125000	},
	{	125000,		125000	},
	{	125000,		125000	},
	{	125000,		125000	},
	{	125000,		125000	},
	{	125000,		125000	},
	{	125000,		125000	},
	{	125000,		125000	},
	{	125000,		125000	},
	{	125000,		125000	},
	{	125000,		125000	},
	{	125000,		125000	},
	{	125000,		125000	},
};

static ssize_t show_name(struct device *dev, struct device_attribute *attr,
			char *buf)
{
	return sprintf(buf, "omap_bandgap_sensor\n");
}

static DEVICE_ATTR(name, S_IRUGO, show_name, NULL);

static inline u32 omap_get_temp_sensor(void)
{
	return omap_ctrl_readl(OMAP4_CTRL_MODULE_CORE_TEMP_SENSOR);
}

static inline s16 omap_get_temp_adc_index(void)
{
	u8 retries = 0;
	s16 val = 0;

	/* Trigger an ADC conversion of the bandgap sensor */
	omap_ctrl_writel(0x00000200, OMAP4_CTRL_MODULE_CORE_TEMP_SENSOR);
	msleep(1);
	omap_ctrl_writel(0x00000000, OMAP4_CTRL_MODULE_CORE_TEMP_SENSOR);
	msleep(1);

	/* Wait for ADC end of conversion up to 5 retries */
	while (retries++ < 5) {
		if ((omap_get_temp_sensor() & 0x100) != 0x100)
			break;

		msleep(5);
	}

	if (retries >= 5)
		return -1;

	val = omap_ctrl_readl(OMAP4_CTRL_MODULE_CORE_TEMP_SENSOR) & 0xff;

	return val;
}

static ssize_t show_max_min(struct device *dev, struct device_attribute *attr,
			  char *buf)
{
	int index = to_sensor_dev_attr(attr)->index;
	int adc_index = omap_get_temp_adc_index();

	if (adc_index < 0 || adc_index > 127) {
		return sprintf(buf, "-1\n");
	} else {
		if (index == 0) {
			return sprintf(buf, "%d\n",
				omap_bandgap_adc_table[adc_index].temp_min);
		} else {
			return sprintf(buf, "%d\n",
				omap_bandgap_adc_table[adc_index].temp_max);
		}
	}
}

static SENSOR_DEVICE_ATTR(temp1_min, S_IRUGO,
			  show_max_min, NULL, 0);
static SENSOR_DEVICE_ATTR(temp1_max, S_IRUGO,
			  show_max_min, NULL, 1);

static struct attribute *omap_bandgap_sensor_attributes[] = {
	&dev_attr_name.attr,
	&sensor_dev_attr_temp1_max.dev_attr.attr,
	&sensor_dev_attr_temp1_min.dev_attr.attr,
	NULL
};

static const struct attribute_group omap_bandgap_sensor_group = {
	.attrs = omap_bandgap_sensor_attributes,
};

static int __devinit omap_bandgap_sensor_probe(struct platform_device *pdev)
{
	int ret;

	ret = sysfs_create_group(&pdev->dev.kobj, &omap_bandgap_sensor_group);

	if (ret)
		return ret;

	hwmon_device_register(&pdev->dev);

	return 0;
}

static int __devexit omap_bandgap_sensor_remove(struct platform_device *pdev)
{
	hwmon_device_unregister(&pdev->dev);

	sysfs_remove_group(&pdev->dev.kobj, &omap_bandgap_sensor_group);

	platform_set_drvdata(pdev, NULL);

	return 0;
}

static struct platform_driver omap_bandgap_sensor_driver = {
	.probe = omap_bandgap_sensor_probe,
	.remove = __devexit_p(omap_bandgap_sensor_remove),
	.driver = {
		.name = "omap_bandgap_sensor",
		.owner = THIS_MODULE,
	},
};

static int __init omap_bandgap_sensor_init(void)
{
	return platform_driver_register(&omap_bandgap_sensor_driver);
}
module_init(omap_bandgap_sensor_init);

static void __exit omap_bandgap_sensor_exit(void)
{
	platform_driver_unregister(&omap_bandgap_sensor_driver);
}
module_exit(omap_bandgap_sensor_exit);

MODULE_AUTHOR("Donald Chan <hoiho@lab126.com>");
MODULE_DESCRIPTION("OMAP Bandgap Temperature Sensor");
MODULE_LICENSE("GPL");
