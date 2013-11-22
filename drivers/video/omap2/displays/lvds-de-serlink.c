/*
 * lvds-de-serlink.c
 *
 * lvds based serial link interface for onboard communication.
 * Copyright (C) 2010-2011 Texas Instruments Incorporated - http://www.ti.com/
 * Authors: Dandawate Saket <dsaket@ti.com>
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

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/i2c/lvds-de-serlink.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/irqdomain.h>
#include <linux/module.h>
#include <linux/spinlock.h>
#include <linux/workqueue.h>
#include "lvds-de-serlink_reg.h"

static const struct i2c_device_id dserlink_id[] = {
	{ "ds90uh928q", 8 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, dserlink_id);

union dser_config1 {
	struct {
		u8 reserved:1;
		u8 de_rgb:1;
		u8 auto_ack:1;
		u8 i2c_pass:1;
		u8 en_filter:1;
		u8 failsafe:1;
		u8 back_crc:1;
		u8 reserved1:1;
	} bits;
	u8 byte;
};

union dser_config0 {
	struct {
		u8 lfmode:1;
		u8 lf_overide:1;
		u8 bkwdmode:1;
		u8 bk_override:1;
		u8 oss_sel:1;
		u8 autoclk:1;
		u8 oss_en:1;
		u8 oen:1;
	} bits;
	u8 byte;
};

union sys_fs_data {
	struct {
		u8 data:8;
		u8 addr:8;
		u8 i2c_addr:8;
		u8 ops:8;
	} bytes;
	u32 integer;
};

struct dserlink {
	struct i2c_client *client;
	struct mutex lock; /* protect 'out' */
	struct work_struct work; /* irq demux work */
	struct irq_domain *irq_domain; /* for irq demux  */
	spinlock_t slock; /* protect irq demux */
	unsigned out; /* software latch */
	unsigned status; /* current status */
	int irq; /* real irq number */

	int (*write)(struct i2c_client *client, unsigned data);
	int (*read)(struct i2c_client *client);
};


/*-------------------------------------------------------------------------*/

/* Talk to 8-bit link */

static int i2c_write_le8(struct i2c_client *client, unsigned addr,
							unsigned data)
{
	return i2c_smbus_write_byte_data(client, addr, data);
}

static int i2c_read_le8(struct i2c_client *client, unsigned addr)
{
	return (int)i2c_smbus_read_byte_data(client, addr);
}


/*-------------------------------------------------------------------------*/
static ssize_t show_dserlink_reg_read(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct dserlink_platform_data *d = i2c_get_clientdata(client);

	if (!d)
		return -EINVAL;

	d->data = i2c_read_le8(client, d->addr);
	dev_err(&client->dev, "%s %d client %x addr %x val %x\n", __func__,
			__LINE__, client->addr, d->addr, d->data);

	return sprintf(buf, "%d\n", d->data);
}

static ssize_t set_dserlink_reg_write(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct dserlink_platform_data *d = i2c_get_clientdata(client);
	int err;
	union sys_fs_data fs_data;

	if (!d)
		return -EINVAL;

	err = kstrtou32(buf, 0, &fs_data.integer);
	if (err)
		return err;

	client->addr = fs_data.bytes.i2c_addr;
	d->addr = fs_data.bytes.addr;
	d->data = fs_data.bytes.data;

	if (!fs_data.bytes.ops)
		d->data = i2c_read_le8(client, d->addr);
	else
		err = i2c_write_le8(client, d->addr, d->data);

	dev_err(&client->dev, "%s %d oper %d client %x addr %x val %x\n",
					__func__, __LINE__, fs_data.bytes.ops,
					client->addr, d->addr, d->data);

	return 1;
}

static DEVICE_ATTR(dserlink_rw_data, S_IRUGO | S_IWUSR,
			show_dserlink_reg_read, set_dserlink_reg_write);

static struct attribute *dserlink_attributes[] = {
	&dev_attr_dserlink_rw_data.attr,
	NULL
};

static const struct attribute_group dserlink_attr_group = {
	.attrs = dserlink_attributes,
};

static int dserlink_reset(struct i2c_client *client)
{
	u8 val = (1 < DSERLINK_RESET1);
	return i2c_write_le8(client, DSERLINK_RESET, val);
}

static int dserlink_get_i2c_id(struct i2c_client *client)
{
	return i2c_read_le8(client, DSERLINK_I2C_DEVICE_ID) >> 1;
}

static int dserlink_en_backchannel(struct i2c_client *client)
{
	return i2c_write_le8(client, DSERLINK_RESET, 1 << DSERLINK_BC_EN);
}
static int dserlink_setup_config(struct i2c_client *client, void *data)
{
	union dser_config0 config0;
	union dser_config1 config1;
	u8 val;
	dev_err(&client->dev, "1. client ptr %p i2c addr %x\n",
						client, client->addr);
	/* Config 0  */
	config0.bits.lfmode = 0;
	config0.bits.lf_overide = 1;
	config0.bits.bkwdmode = 0;
	config0.bits.bk_override = 1;
	config0.bits.oss_sel = 0;
	config0.bits.autoclk = 1;
	config0.bits.oss_en = 0;
	config0.bits.oen = 0;

	val = i2c_write_le8(client, DSERLINK_CONFIG0, config0.byte);
	if (val < 0)
		goto error_config;
	dev_err(&client->dev, "config 0 : 0x%x\n", config0.byte);

	/* Config 1  */
	config1.bits.reserved = 0;
	config1.bits.de_rgb = 0;
	config1.bits.auto_ack = 1;
	config1.bits.i2c_pass = 1;
	config1.bits.en_filter = 0;
	config1.bits.failsafe = 0;
	config1.bits.back_crc = 1;
	config1.bits.reserved1 = 0;

	val = i2c_write_le8(client, DSERLINK_CONFIG1, config1.byte);
	if (val < 0)
		goto error_config;
	dev_err(&client->dev, "config 1 : 0x%x\n", config1.byte);

	return 0;
error_config:
	return -EINVAL;
}

static int dserlink_read_serializer_id(struct i2c_client *client, void *arg)
{
	u8 ret;

	/* unlock the deserializer */
	ret = i2c_write_le8(client, DSERLINK_SER_I2C_ID, 0);
	if (ret < 0)
		goto error_config;

	/* read serializer id */
	ret = i2c_read_le8(client, DSERLINK_SER_I2C_ID);
	if (ret < 0)
		goto error_config;

	*(u8 *)arg = (ret >> 1);

	return 0;

error_config:
	return -EINVAL;

}

static int dserlink_dump_reg(struct i2c_client *client)
{
	u8 val;

	/* Validate the link */
	val = i2c_read_le8(client, DSERLINK_CONFIG0);
	dev_err(&client->dev, "DSERLINK_CONFIG_0 : 0x%x\n", val);

	val = i2c_read_le8(client, DSERLINK_CONFIG1);
	dev_err(&client->dev, "DSERLINK_CONFIG_1 : 0x%x\n", val);

	val = i2c_read_le8(client, DSERLINK_GN_STS);
	dev_err(&client->dev, "DSERLINK_GN_STS : 0x%x\n", val);

	val = i2c_read_le8(client, DSERLINK_DATA_CTL);
	dev_err(&client->dev, "DSERLINK_DATA_CTL : 0x%x\n", val);

	val = i2c_read_le8(client, DSERLINK_RX_STS);
	dev_err(&client->dev, "DSERLINK_RX_STS : 0x%x\n", val);

	return 0;

}
static int dserlink_command(struct i2c_client *client,
					unsigned int cmd, void *arg)
{
	int status = -EINVAL;
	struct dser_i2c_data *pd = NULL;

	switch (cmd) {
	case DSER_RESET:
		/* Reset the FPDLINK*/
		status = dserlink_reset(client);
	break;

	case DSER_EN_BC:
		/* Enable  i2c back channel */
		status = dserlink_en_backchannel(client);
	break;

	case DSER_CONFIGURE:
		/* Setup basic configuration 0 & 1*/
		status = dserlink_setup_config(client, arg);
	break;

	case DSER_GET_I2C_ADDR:
		/* get the serializer ID registered inside */
		status = dserlink_get_i2c_id(client);
	break;

	case DSER_GET_SER_I2C_ADDR:
		/* Read auto populated serializer i2c ID*/
		status = dserlink_read_serializer_id(client, arg);
	break;

	case DSER_READ:
		pd = (struct dser_i2c_data *)arg;
		if (pd)
			pd->data = status = i2c_read_le8(client,
							pd->addr);
	break;

	case DSER_WRITE:
		pd = (struct dser_i2c_data *)arg;
		if (pd)
			status = i2c_write_le8(client, pd->addr,
							pd->data);
	break;

	case DSER_REG_DUMP:
		status = dserlink_dump_reg(client);
	break;

	default:
		return -EINVAL;
	}

	return status;

}

static void dserlink_init_client(struct i2c_client *client)
{
	struct dserlink_platform_data *data = i2c_get_clientdata(client);

	if (!data)
		dev_err(&client->dev, "init client data failed '%s'\n",
				client->name);

	data->addr = 0;
	data->data = 0;
}

static int dserlink_probe(struct i2c_client *client,
			 const struct i2c_device_id *id)
{
	struct dserlink_platform_data	*pdata;
	int status;

	pdata = devm_kzalloc(&client->dev,
				sizeof(struct dserlink_platform_data),
				GFP_KERNEL);
	if (!pdata)
		return -ENOMEM;

	i2c_set_clientdata(client, pdata);

	dserlink_init_client(client);

	/* Register sysfs hooks */
	status = sysfs_create_group(&client->dev.kobj, &dserlink_attr_group);
	if (status)
		goto fail;

	dev_err(&client->dev, "probe error %d for '%s'\n",
			status, client->name);

	return status;
fail:
	return -EINVAL;

}

static int dserlink_remove(struct i2c_client *client)
{
	struct dserlink_platform_data	*pdata = i2c_get_clientdata(client);
	kfree(pdata);

	sysfs_remove_group(&client->dev.kobj, &dserlink_attr_group);

	return 0;
}

static const struct of_device_id dserlink_dt_ids[] = {
	{.compatible = "ti,ds90uh928q", },
	{ }
};

MODULE_DEVICE_TABLE(of, dserlink_dt_ids);

static struct i2c_driver dserlink_driver = {
	.driver = {
		.name = "dserlink",
		.owner = THIS_MODULE,
		.of_match_table	= dserlink_dt_ids,
	},
	.probe = dserlink_probe,
	.remove = dserlink_remove,
	.command = dserlink_command,
	.id_table = dserlink_id,
};

static int __init dserlink_init(void)
{
	return i2c_add_driver(&dserlink_driver);
}
module_init(dserlink_init);

static void __exit dserlink_exit(void)
{
	i2c_del_driver(&dserlink_driver);
}
module_exit(dserlink_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Saket Dandawate");


