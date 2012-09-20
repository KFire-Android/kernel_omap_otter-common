/*
 * Clock Divider Chip (TCXO) support
 *
 * Copyright (C) 2010 Texas Instruments, Inc.
 * Written by Rajendra Nayak <rnayak@ti.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/platform_device.h>
#include <linux/cdc_tcxo.h>
#include <linux/i2c.h>

#define DRIVER_DESC       "CDC TCXO driver"
#define DRIVER_NAME       "cdc_tcxo_driver"

struct cdc_tcxo_info {
	struct i2c_client *client;
	struct device *dev;
	char buf[4];
	int active;
};

static struct cdc_tcxo_info cdc_info;

int cdc_tcxo_set_req_int(int clk_id, int enable)
{
	char mask;
	int ret;

	if (!cdc_info.active)
		return -ENODEV;

	switch (clk_id) {
	case CDC_TCXO_CLK1:
		mask = CDC_TCXO_REQ1INT;
		break;
	case CDC_TCXO_CLK2:
		mask = CDC_TCXO_REQ2INT;
		break;
	case CDC_TCXO_CLK3:
		mask = CDC_TCXO_REQ3INT;
		break;
	case CDC_TCXO_CLK4:
		mask = CDC_TCXO_REQ4INT;
		break;
	default:
		dev_err(cdc_info.dev, "invalid clk_id: %d\n", clk_id);
		return -EINVAL;
	}

	if (enable)
		cdc_info.buf[CDC_TCXO_REG1] |= mask;
	else
		cdc_info.buf[CDC_TCXO_REG1] &= ~mask;

	ret = i2c_master_send(cdc_info.client, cdc_info.buf, CDC_TCXO_REGNUM);
	if (ret < 0) {
		dev_err(cdc_info.dev, "failed to write data (%d)\n", ret);
		return ret;
	}

	return 0;
}
EXPORT_SYMBOL(cdc_tcxo_set_req_int);

int cdc_tcxo_set_req_prio(int clk_id, int req_prio)
{
	char mask;
	int ret;

	if (!cdc_info.active)
		return -ENODEV;

	switch (clk_id) {
	case CDC_TCXO_CLK1:
		mask = CDC_TCXO_REQ1PRIO;
		break;
	case CDC_TCXO_CLK2:
		mask = CDC_TCXO_REQ2PRIO;
		break;
	case CDC_TCXO_CLK3:
		mask = CDC_TCXO_REQ3PRIO;
		break;
	case CDC_TCXO_CLK4:
		mask = CDC_TCXO_REQ4PRIO;
		break;
	default:
		dev_err(cdc_info.dev, "invalid clk_id: %d\n", clk_id);
		return -EINVAL;
	}

	if (req_prio == CDC_TCXO_PRIO_REQINT)
		cdc_info.buf[CDC_TCXO_REG3] |= mask;
	else
		cdc_info.buf[CDC_TCXO_REG3] &= ~mask;

	ret = i2c_master_send(cdc_info.client, cdc_info.buf, CDC_TCXO_REGNUM);
	if (ret < 0) {
		dev_err(cdc_info.dev, "failed to write data (%d)\n", ret);
		return ret;
	}

	return 0;
}
EXPORT_SYMBOL(cdc_tcxo_set_req_prio);

static int cdc_tcxo_probe(struct i2c_client *client, \
				const struct i2c_device_id *id)
{
	struct cdc_tcxo_platform_data *pdata = dev_get_platdata(&client->dev);
	int i, ret;

	cdc_info.client = client;
	cdc_info.dev = &client->dev;

	for (i = 0; i < CDC_TCXO_REGNUM; i++)
		cdc_info.buf[i] = pdata->buf[i];

	ret = i2c_master_send(client, cdc_info.buf, CDC_TCXO_REGNUM);
	if (ret < 0) {
		dev_err(cdc_info.dev, "failed to initialize registers (%d)\n",
			ret);
		return ret;
	}

	cdc_info.active = 1;

	return 0;
}

static const struct i2c_device_id cdc_tcxo_id[] = {
	{"cdc_tcxo_driver", 0},
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
