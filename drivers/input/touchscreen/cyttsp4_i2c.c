/*
 * Source for:
 * Cypress TrueTouch(TM) Standard Product (TTSP) I2C touchscreen driver.
 * For use with Cypress Gen4 and Solo parts.
 * Supported parts include:
 * CY8CTMA398
 * CY8CTMA884
 *
 * Copyright (C) 2009-2011 Cypress Semiconductor, Inc.
 * Copyright (C) 2011 Motorola Mobility, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2, and only version 2, as published by the
 * Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Contact Cypress Semiconductor at www.cypress.com <kev@cypress.com>
 *
 */

#include "cyttsp4_core.h"

#include <linux/i2c.h>
#include <linux/slab.h>

#define CY_I2C_DATA_SIZE  256

struct cyttsp4_i2c {
	struct cyttsp4_bus_ops ops;
	struct i2c_client *client;
	void *ttsp_client;
	u8 wr_buf[CY_I2C_DATA_SIZE];
};

static s32 cyttsp4_i2c_read_block_data(void *handle, u16 subaddr,
	u8 length, void *values, int i2c_addr, bool use_subaddr)
{
	struct cyttsp4_i2c *ts = container_of(handle, struct cyttsp4_i2c, ops);
	int retval = 0;
	u8 sub_addr[2];
	int subaddr_len;

	if (use_subaddr) {
		subaddr_len = 1;
		sub_addr[0] = subaddr;
	}

	ts->client->addr = i2c_addr;
	if (!use_subaddr)
		goto read_packet;

	/* write subaddr */
	retval = i2c_master_send(ts->client, sub_addr, subaddr_len);
	if (retval < 0)
		return retval;
	else if (retval != subaddr_len)
		return -EIO;

read_packet:
	retval = i2c_master_recv(ts->client, values, length);

	return (retval < 0) ? retval : retval != length ? -EIO : 0;
}

static s32 cyttsp4_i2c_write_block_data(void *handle, u16 subaddr,
	u8 length, const void *values, int i2c_addr, bool use_subaddr)
{
	struct cyttsp4_i2c *ts = container_of(handle, struct cyttsp4_i2c, ops);
	int retval;

	if (use_subaddr) {
		ts->wr_buf[0] = subaddr;
		memcpy(&ts->wr_buf[1], values, length);
		length += 1;
	} else {
		memcpy(&ts->wr_buf[0], values, length);
	}
	ts->client->addr = i2c_addr;
	retval = i2c_master_send(ts->client, ts->wr_buf, length);

	return (retval < 0) ? retval : retval != length ? -EIO : 0;
}

static int __devinit cyttsp4_i2c_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	struct cyttsp4_i2c *ts;

	printk(KERN_INFO "cypress probe started\n");

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C))
		return -EIO;

	/* allocate and clear memory */
	ts = kzalloc(sizeof(*ts), GFP_KERNEL);
	if (!ts) {
		dev_dbg(&client->dev, "%s: Error, kzalloc.\n", __func__);
		return -ENOMEM;
	}

	/* register driver_data */
	ts->client = client;
	i2c_set_clientdata(client, ts);
	ts->ops.write = cyttsp4_i2c_write_block_data;
	ts->ops.read = cyttsp4_i2c_read_block_data;
	ts->ops.dev = &client->dev;
	ts->ops.dev->bus = &i2c_bus_type;

	ts->ttsp_client = cyttsp4_core_init(&ts->ops, &client->dev,
			client->irq, client->name);
	if (ts->ttsp_client == NULL) {
		kfree(ts);
		return -ENODATA;
	}

	dev_dbg(ts->ops.dev, "%s: Registration complete\n", __func__);

	return 0;
}


/* registered in driver struct */
static int __devexit cyttsp4_i2c_remove(struct i2c_client *client)
{
	struct cyttsp4_i2c *ts;

	ts = i2c_get_clientdata(client);
	cyttsp4_core_release(ts->ttsp_client);
	kfree(ts);
	return 0;
}

#if defined(CONFIG_PM) && !defined(CONFIG_HAS_EARLYSUSPEND)
static int cyttsp4_i2c_suspend(struct i2c_client *client, pm_message_t message)
{
	struct cyttsp4_i2c *ts = i2c_get_clientdata(client);

	return cyttsp4_suspend(ts);
}

static int cyttsp4_i2c_resume(struct i2c_client *client)
{
	struct cyttsp4_i2c *ts = i2c_get_clientdata(client);

	return cyttsp4_resume(ts);
}
#endif

static const struct i2c_device_id cyttsp4_i2c_id[] = {
	{ CY_I2C_NAME, 0 },  { }
};

static struct i2c_driver cyttsp4_i2c_driver = {
	.driver = {
		.name = CY_I2C_NAME,
		.owner = THIS_MODULE,
	},
	.probe = cyttsp4_i2c_probe,
	.remove = __devexit_p(cyttsp4_i2c_remove),
	.id_table = cyttsp4_i2c_id,
#if defined(CONFIG_PM) && !defined(CONFIG_HAS_EARLYSUSPEND)
	.suspend = cyttsp4_i2c_suspend,
	.resume = cyttsp4_i2c_resume,
#endif
};

static int __init cyttsp4_i2c_init(void)
{
	return i2c_add_driver(&cyttsp4_i2c_driver);
}

static void __exit cyttsp4_i2c_exit(void)
{
	return i2c_del_driver(&cyttsp4_i2c_driver);
}

module_init(cyttsp4_i2c_init);
module_exit(cyttsp4_i2c_exit);

MODULE_ALIAS("i2c:cyttsp4");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Cypress TrueTouch(R) Standard Product (TTSP) I2C driver");
MODULE_AUTHOR("Cypress");
MODULE_DEVICE_TABLE(i2c, cyttsp4_i2c_id);
