/*
 * OMAP16XX-specific gpio code
 *
 * Copyright (C) 2010 Texas Instruments, Inc.
 *
 * Author:
 *	Charulatha V <charu@ti.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/gpio.h>

#define OMAP1610_GPIO1_BASE		0xfffbe400
#define OMAP1610_GPIO2_BASE		0xfffbec00
#define OMAP1610_GPIO3_BASE		0xfffbb400
#define OMAP1610_GPIO4_BASE		0xfffbbc00
#define OMAP1_MPUIO_VBASE		OMAP1_MPUIO_BASE

static struct omap_gpio_dev_attr omap16xx_gpio_attr = {
	.bank_width = 16,
};

/*
 * OMAP16XX MPU GPIO interface data
 */
static struct __initdata resource omap16xx_mpu_gpio_resources[] = {
	{
		.start	= OMAP1_MPUIO_VBASE,
		.end	= OMAP1_MPUIO_VBASE + SZ_2K - 1,
		.flags	= IORESOURCE_MEM,
	},
	{
		.start	= INT_MPUIO,
		.flags	= IORESOURCE_IRQ,
	},
};

static struct __initdata omap_gpio_platform_data omap16xx_mpu_gpio_config = {
	.virtual_irq_start	= IH_MPUIO_BASE,
	.bank_type		= METHOD_MPUIO,
	.gpio_attr		= &omap16xx_gpio_attr,
};

static struct __initdata platform_device omap16xx_mpu_gpio = {
	.name           = "omap-gpio",
	.id             = 0,
	.dev            = {
		.platform_data = &omap16xx_mpu_gpio_config,
	},
	.num_resources = ARRAY_SIZE(omap16xx_mpu_gpio_resources),
	.resource = omap16xx_mpu_gpio_resources,
};

/*
 * OMAP16XX GPIO1 interface data
 */
static struct __initdata resource omap16xx_gpio1_resources[] = {
	{
		.start	= OMAP1610_GPIO1_BASE,
		.end	= OMAP1610_GPIO1_BASE + SZ_2K - 1,
		.flags	= IORESOURCE_MEM,
	},
	{
		.start	= INT_GPIO_BANK1,
		.flags	= IORESOURCE_IRQ,
	},
};

static struct __initdata omap_gpio_platform_data omap16xx_gpio1_config = {
	.virtual_irq_start	= IH_GPIO_BASE,
	.bank_type		= METHOD_GPIO_1610,
	.gpio_attr		= &omap16xx_gpio_attr,
};

static struct __initdata platform_device omap16xx_gpio1 = {
	.name           = "omap-gpio",
	.id             = 1,
	.dev            = {
		.platform_data = &omap16xx_gpio1_config,
	},
	.num_resources = ARRAY_SIZE(omap16xx_gpio1_resources),
	.resource = omap16xx_gpio1_resources,
};

/*
 * OMAP16XX GPIO2 interface data
 */
static struct __initdata resource omap16xx_gpio2_resources[] = {
	{
		.start	= OMAP1610_GPIO2_BASE,
		.end	= OMAP1610_GPIO2_BASE + SZ_2K - 1,
		.flags	= IORESOURCE_MEM,
	},
	{
		.start	= INT_1610_GPIO_BANK2,
		.flags	= IORESOURCE_IRQ,
	},
};

static struct __initdata omap_gpio_platform_data omap16xx_gpio2_config = {
	.virtual_irq_start	= IH_GPIO_BASE + 16,
	.bank_type		= METHOD_GPIO_1610,
	.gpio_attr		= &omap16xx_gpio_attr,
};

static struct __initdata platform_device omap16xx_gpio2 = {
	.name           = "omap-gpio",
	.id             = 2,
	.dev            = {
		.platform_data = &omap16xx_gpio2_config,
	},
	.num_resources = ARRAY_SIZE(omap16xx_gpio2_resources),
	.resource = omap16xx_gpio2_resources,
};

/*
 * OMAP16XX GPIO3 interface data
 */
static struct __initdata resource omap16xx_gpio3_resources[] = {
	{
		.start	= OMAP1610_GPIO3_BASE,
		.end	= OMAP1610_GPIO3_BASE + SZ_2K - 1,
		.flags	= IORESOURCE_MEM,
	},
	{
		.start	= INT_1610_GPIO_BANK3,
		.flags	= IORESOURCE_IRQ,
	},
};

static struct __initdata omap_gpio_platform_data omap16xx_gpio3_config = {
	.virtual_irq_start	= IH_GPIO_BASE + 32,
	.bank_type		= METHOD_GPIO_1610,
	.gpio_attr		= &omap16xx_gpio_attr,
};

static struct __initdata platform_device omap16xx_gpio3 = {
	.name           = "omap-gpio",
	.id             = 3,
	.dev            = {
		.platform_data = &omap16xx_gpio3_config,
	},
	.num_resources = ARRAY_SIZE(omap16xx_gpio3_resources),
	.resource = omap16xx_gpio3_resources,
};

/*
 * OMAP16XX GPIO4 interface data
  */
static struct __initdata resource omap16xx_gpio4_resources[] = {
	{
		.start	= OMAP1610_GPIO4_BASE,
		.end	= OMAP1610_GPIO4_BASE + SZ_2K - 1,
		.flags	= IORESOURCE_MEM,
	},
	{
		.start	= INT_1610_GPIO_BANK4,
		.flags	= IORESOURCE_IRQ,
	},
};

static struct __initdata omap_gpio_platform_data omap16xx_gpio4_config = {
	.virtual_irq_start	= IH_GPIO_BASE + 48,
	.bank_type		= METHOD_GPIO_1610,
	.gpio_attr		= &omap16xx_gpio_attr,
};

static struct __initdata platform_device omap16xx_gpio4 = {
	.name           = "omap-gpio",
	.id             = 4,
	.dev            = {
		.platform_data = &omap16xx_gpio4_config,
	},
	.num_resources = ARRAY_SIZE(omap16xx_gpio4_resources),
	.resource = omap16xx_gpio4_resources,
};

static struct __initdata platform_device * omap16xx_gpio_dev[] = {
	&omap16xx_mpu_gpio,
	&omap16xx_gpio1,
	&omap16xx_gpio2,
	&omap16xx_gpio3,
	&omap16xx_gpio4,
};

/*
 * omap16xx_gpio_init needs to be done before
 * machine_init functions access gpio APIs.
 * Hence omap16xx_gpio_init is a postcore_initcall.
 */
static int __init omap16xx_gpio_init(void)
{
	int i;

	if (!cpu_is_omap16xx())
		return -EINVAL;

	for (i = 0; i < sizeof(omap16xx_gpio_dev); i++)
			platform_device_register(omap16xx_gpio_dev[i]);

	gpio_bank_count = sizeof(omap16xx_gpio_dev);

	return 0;
}
postcore_initcall(omap16xx_gpio_init);
