/*
 * cma3000_d0x_i2c.c
 *
 * Implements I2C interface for VTI CMA300_D0x Accelerometer driver
 *
 * Copyright (C) 2010 Texas Instruments
 * Author: Hemanth V <hemanthv@ti.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/i2c/cma3000.h>
#include "cma3000_d0x.h"

int cma3000_set(struct cma3000_accl_data *accl, u8 reg, u8 val, char *msg)
{
	int ret = i2c_smbus_write_byte_data(accl->client, reg, val);
	if (ret < 0)
		dev_err(&accl->client->dev,
			"i2c_smbus_write_byte_data failed (%s)\n", msg);
	return ret;
}

int cma3000_read(struct cma3000_accl_data *accl, u8 reg, char *msg)
{
	int ret = i2c_smbus_read_byte_data(accl->client, reg);
	if (ret < 0)
		dev_err(&accl->client->dev,
			"i2c_smbus_read_byte_data failed (%s)\n", msg);
	return ret;
}

static int __devinit cma3000_accl_probe(struct i2c_client *client,
					const struct i2c_device_id *id)
{
	int ret;
	struct cma3000_accl_data *data = NULL;

	data = kzalloc(sizeof(*data), GFP_KERNEL);
	if (data == NULL) {
		ret = -ENOMEM;
		goto err_op_failed;
	}

	data->client = client;
	i2c_set_clientdata(client, data);

	ret = cma3000_init(data);
	if (ret)
		goto err_op_failed;

	return 0;

err_op_failed:

	if (data != NULL)
		kfree(data);

	return ret;
}

static int __devexit cma3000_accl_remove(struct i2c_client *client)
{
	struct cma3000_accl_data *data = i2c_get_clientdata(client);
	int ret;

	ret = cma3000_exit(data);
	i2c_set_clientdata(client, NULL);
	kfree(data);

	return ret;
}

#ifdef CONFIG_PM
static int cma3000_accl_suspend(struct i2c_client *client, pm_message_t mesg)
{
	struct cma3000_accl_data *data = i2c_get_clientdata(client);

	return cma3000_poweroff(data);
}

static int cma3000_accl_resume(struct i2c_client *client)
{
	struct cma3000_accl_data *data = i2c_get_clientdata(client);

	return cma3000_poweron(data);
}
#endif

static const struct i2c_device_id cma3000_id[] = {
	{ "cma3000_accl", 0 },
	{ },
};

static struct i2c_driver cma3000_accl_driver = {
	.probe		= cma3000_accl_probe,
	.remove		= cma3000_accl_remove,
	.id_table	= cma3000_id,
#ifdef CONFIG_PM
	.suspend	= cma3000_accl_suspend,
	.resume		= cma3000_accl_resume,
#endif
	.driver = {
		.name = "cma3000_accl"
	},
};

static int __init cma3000_accl_init(void)
{
	return i2c_add_driver(&cma3000_accl_driver);
}

static void __exit cma3000_accl_exit(void)
{
	i2c_del_driver(&cma3000_accl_driver);
}

module_init(cma3000_accl_init);
module_exit(cma3000_accl_exit);

MODULE_DESCRIPTION("CMA3000-D0x Accelerometer Driver");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Hemanth V <hemanthv@ti.com>");
