/*
 * Provides code common for host and device side USB.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, version 2.
 *
 * If either host side (ie. CONFIG_USB=y) or device side USB stack
 * (ie. CONFIG_USB_GADGET=y) is compiled in the kernel, this module is
 * compiled-in as well.  Otherwise, if either of the two stacks is
 * compiled as module, this file is compiled as module as well.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/usb/ch9.h>
#include <linux/usb/of.h>

const char *usb_speed_string(enum usb_device_speed speed)
{
	static const char *const names[] = {
		[USB_SPEED_UNKNOWN] = "UNKNOWN",
		[USB_SPEED_LOW] = "low-speed",
		[USB_SPEED_FULL] = "full-speed",
		[USB_SPEED_HIGH] = "high-speed",
		[USB_SPEED_WIRELESS] = "wireless",
		[USB_SPEED_SUPER] = "super-speed",
	};

	if (speed < 0 || speed >= ARRAY_SIZE(names))
		speed = USB_SPEED_UNKNOWN;
	return names[speed];
}
EXPORT_SYMBOL_GPL(usb_speed_string);

#ifdef CONFIG_OF
static const char *const usb_dr_modes[] = {
	[USB_DR_MODE_UNKNOWN]		= "",
	[USB_DR_MODE_HOST]		= "host",
	[USB_DR_MODE_PERIPHERAL]	= "peripheral",
	[USB_DR_MODE_OTG]		= "otg",
};

/**
 * of_usb_get_dr_mode - Get dual role mode for given device_node
 * @np:	Pointer to the given device_node
 *
 * The function gets phy interface string from property 'dr_mode',
 * and returns the correspondig enum usb_dr_mode
 */
enum usb_dr_mode of_usb_get_dr_mode(struct device_node *np)
{
	const char *dr_mode;
	int err, i;

	err = of_property_read_string(np, "dr_mode", &dr_mode);
	if (err < 0)
		return USB_DR_MODE_UNKNOWN;

	for (i = 0; i < ARRAY_SIZE(usb_dr_modes); i++)
		if (!strcmp(dr_mode, usb_dr_modes[i]))
			return i;

	return USB_DR_MODE_UNKNOWN;
}
EXPORT_SYMBOL_GPL(of_usb_get_dr_mode);

static const char *const usb_maximum_speed[] = {
	[USB_SPEED_UNKNOWN]	= "",
	[USB_SPEED_LOW]		= "lowspeed",
	[USB_SPEED_FULL]	= "fullspeed",
	[USB_SPEED_HIGH]	= "highspeed",
	[USB_SPEED_WIRELESS]	= "wireless",
	[USB_SPEED_SUPER]	= "superspeed",
};

/**
 * of_usb_get_maximum_speed - Get maximum requested speed for a given USB
 * controller.
 * @np: Pointer to the given device_node
 *
 * The function gets the maximum speed string from property "maximum-speed",
 * and returns the corresponding enum usb_device_speed.
 */
enum usb_device_speed of_usb_get_maximum_speed(struct device_node *np)
{
	const char *maximum_speed;
	int err;
	int i;

	err = of_property_read_string(np, "maximum-speed", &maximum_speed);
	if (err < 0)
		return USB_SPEED_UNKNOWN;

	for (i = 0; i < ARRAY_SIZE(usb_maximum_speed); i++)
		if (strcmp(maximum_speed, usb_maximum_speed[i]) == 0)
			return i;

	return USB_SPEED_UNKNOWN;
}
EXPORT_SYMBOL_GPL(of_usb_get_maximum_speed);

#endif

MODULE_LICENSE("GPL");
