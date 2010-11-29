/*
 * OMAP4 CDC TCXO support
 * The Clock Divider Chip (TCXO) is used on OMAP4 based SDP platforms
 *
 * Copyright (C) 2010 Texas Instruments, Inc.
 * Written by Rajendra Nayak <rnayak@ti.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/platform_device.h>
#include <linux/i2c.h>

#define DRIVER_DESC       "CDC TCXO driver"
#define DRIVER_NAME       "cdc_tcxo_driver"

static int cdc_tcxo_probe(struct i2c_client *client, \
				const struct i2c_device_id *id)
{
	int r = 0;
	struct i2c_msg msg;
	/*
	 * The Clock Driver Chip (TCXO) on OMAP4 based SDP needs to
	 * be programmed to output CLK1 based on REQ1 from OMAP.
	 * By default CLK1 is driven based on an internal REQ1INT signal
	 * which is always set to 1.
	 * Doing this helps gate sysclk (from CLK1) to OMAP while OMAP
	 * is in sleep states.
	 * Please refer to the TCXO Data sheet for understanding the
	 * programming sequence further.
	 */
	u8 buf[4] = {0x9f, 0xf0, 0, 0};

	msg.addr = client->addr;
	msg.flags = 0;
	msg.len = 4;
	msg.buf = buf;

	r = i2c_transfer(client->adapter, &msg, 1);

	return 0;
}

static const struct i2c_device_id cdc_tcxo_id[] = {
	{ "cdc_tcxo_driver", 0 },
	{ }
};

static struct i2c_driver cdc_tcxo_i2c_driver = {
	.driver = {
		.name		= DRIVER_NAME,
	 },
	.probe		= cdc_tcxo_probe,
	.id_table	= cdc_tcxo_id,
};

static int __init cdc_tcxo_init(void)
{
	int r;

	r = i2c_add_driver(&cdc_tcxo_i2c_driver);
	if (r < 0) {
		printk(KERN_WARNING DRIVER_NAME
		       " driver registration failed\n");
		return r;
	}

	return 0;
}

static void __exit cdc_tcxo_exit(void)
{
	i2c_del_driver(&cdc_tcxo_i2c_driver);
}


module_init(cdc_tcxo_init);
module_exit(cdc_tcxo_exit);

