/*
 * lvds-serlink.c
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
#include <linux/i2c/lvds-serlink.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/irqdomain.h>
#include <linux/module.h>
#include <linux/spinlock.h>
#include <linux/workqueue.h>
#include "lvds-serlink_reg.h"

static const struct i2c_device_id serlink_id[] = {
	{ "ds90uh925q", 8 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, serlink_id);

union ser_config0 {
	struct {
		u8 trfb:1;
		u8 pclk:1;
		u8 reserved1:1;
		u8 i2cpass:1;
		u8 filteren:1;
		u8 remoteautoack:1;
		u8 reserved2:1;
		u8 backcrc:1;
	} bits;
	u8 byte;
};

union ser_config1 {
	struct {
		u8 lfmodefreq:1;
		u8 lf_mode:1;
		u8 backwardmode:1;
		u8 backward_mode:1;
		u8 reserved1:1;
		u8 crc_reset:1;
		u8 reserved2:1;
		u8 failsafe:1;
	} bits;
	u8 byte;
};

union ser_data_control {
	struct {
		u8 chanb_en:1;
		u8 i2s_transport:1;
		u8 video_sel:1;
		u8 chanb_en_override:1;
		u8 i2s_repeat:1;
		u8 de_polarity:1;
		u8 pass_rgp:1;
		u8 reserved:1;
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

struct serlink {
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
static ssize_t show_serlink_reg_read(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct serlink_platform_data *d = i2c_get_clientdata(client);

	if (!d)
		return -EINVAL;

	d->data = i2c_read_le8(client, d->addr);
	dev_err(&client->dev, "%s %d client %x addr %x val %x\n", __func__,
			__LINE__, client->addr, d->addr, d->data);

	return sprintf(buf, "%d\n", d->data);
}

static ssize_t set_serlink_reg_write(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct serlink_platform_data *d = i2c_get_clientdata(client);
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

static DEVICE_ATTR(serlink_rw_data, S_IRUGO | S_IWUSR,
			show_serlink_reg_read, set_serlink_reg_write);

static struct attribute *serlink_attributes[] = {
	&dev_attr_serlink_rw_data.attr,
	NULL
};

static const struct attribute_group serlink_attr_group = {
	.attrs = serlink_attributes,
};

static int serlink_reset(struct i2c_client *client)
{
	u8 val = (1 < SERLINK_RESET1);
	return i2c_write_le8(client, SERLINK_RESET, val);
}

static int serlink_get_i2c_id(struct i2c_client *client)
{
	return i2c_read_le8(client, SERLINK_I2C_DEVICE_ID) >> 1;
}

static int serlink_setup_config(struct i2c_client *client, void *data)
{
	union ser_config0 config0;
	union ser_config1 config1;
	union ser_data_control datacontol;
	u8 val;
	dev_err(&client->dev, "1. client ptr %p i2c addr %x\n",
						client, client->addr);
	/*pclk strobe edge 1-rising 0-falling */
	config0.bits.trfb = 1;
	/*pclk auto switch 1-enable 0-disable */
	config0.bits.pclk = 1;
	config0.bits.reserved1 = 0;
	/*i2c pass through 1-enable 0-disable */
	config0.bits.i2cpass = 1;
	/*filter clk cycle 1-enable 0-disable */
	config0.bits.filteren = 0;
	/*remote slave ack 1-auto 0-manual */
	config0.bits.remoteautoack = 1;
	config0.bits.reserved2 = 0;
	/*back channel 1-enable 0-disable */
	config0.bits.backcrc = 1;
	val = i2c_write_le8(client, SERLINK_CONFIG_0, config0.byte);
	if (val < 0)
		goto error_config;
	dev_err(&client->dev, "config 0 : 0x%x\n", config0.byte);

	/*pclk range 0-(5-85) Mhz 1-(5-15) Mhz */
	config1.bits.lfmodefreq = 0;
	/*lf mode sel 0-hwd 1-reg*/
	config1.bits.lf_mode = 1;
	/*backward compatibility 0-fpd 3 1-FPD(2/1) */
	config1.bits.backwardmode = 0;
	/*back compat mode 0-hwd 1-reg */
	config1.bits.backward_mode = 1;
	config1.bits.reserved1 = 0;
	/*crc reset 0-normal 1-reset*/
	config1.bits.crc_reset = 0;
	config1.bits.reserved2 = 0;
	/*failsafe 0-high 1-low */
	config1.bits.failsafe = 1;
	val = i2c_write_le8(client, SERLINK_CONFIG_1, config1.byte);
	if (val < 0)
		goto error_config;
	dev_err(&client->dev, "config 1 : 0x%x\n", config1.byte);

	/*i2s chanB 0-dis 1-en */
	datacontol.bits.chanb_en = 0;
	/*i2s mode 0-island 1-frame*/
	datacontol.bits.i2s_transport = 0;
	/*bit mode 0-24 bit 1-18 bit*/
	datacontol.bits.video_sel = 0;
	/*channel override 0-hw 1-reg*/
	datacontol.bits.chanb_en_override = 0;
	/*repeat I2S 0-no 1-en*/
	datacontol.bits.i2s_repeat = 0;
	/*de polarity 0-activeh 1-activel*/
	datacontol.bits.de_polarity = 0;
	/*pass data 0-normal 1-w/o de*/
	datacontol.bits.pass_rgp = 1;
	datacontol.bits.reserved = 0;
	val = i2c_write_le8(client, SERLINK_DATA_CTRL, config1.byte);
	if (val < 0)
		goto error_config;
	dev_err(&client->dev, "datacontol 1 : 0x%x\n", datacontol.byte);

	return 0;
error_config:
	return -EINVAL;
}

static int serlink_validate_link(struct i2c_client *client)
{
	return i2c_read_le8(client, SERLINK_GEN_STS) & SERLINK_LINK_DETECT;
}

static int serlink_validate_fclk(struct i2c_client *client)
{
	return i2c_read_le8(client, SERLINK_GEN_STS) & SERLINK_PCLK;
}

static int serlink_read_deserializer(struct i2c_client *client, void *arg)
{
	u8 ret;

	/* unlock the deserializer */
	ret = i2c_write_le8(client, SERLINK_I2C_DES_ID, 0);
	if (ret < 0)
		goto error_config;

	/* read  deserializer id */
	ret = i2c_read_le8(client, SERLINK_I2C_DES_ID);
	if (ret < 0)
		goto error_config;

	*(u8 *)arg = (ret >> 1);

	return 0;

error_config:
	return -EINVAL;

}


static int serlink_dump_reg(struct i2c_client *client)
{
	u8 val;

	/* Validate the link */
	val = i2c_read_le8(client, SERLINK_CONFIG_0);
	dev_err(&client->dev, "SERLINK_CONFIG_0 : 0x%x\n", val);

	val = i2c_read_le8(client, SERLINK_CONFIG_1);
	dev_err(&client->dev, "SERLINK_CONFIG_1 : 0x%x\n", val);

	val = i2c_read_le8(client, SERLINK_DATA_CTRL);
	dev_err(&client->dev, "SERLINK_DATA_CTRL : 0x%x\n", val);

	val = i2c_read_le8(client, SERLINK_MODE_STS);
	dev_err(&client->dev, "SERLINK_MODE_STS : 0x%x\n", val);

	val = i2c_read_le8(client, SERLINK_GEN_STS);
	dev_err(&client->dev, "SERLINK_GEN_STS : 0x%x\n", val);

	return 0;

}
static int serlink_command(struct i2c_client *client,
					unsigned int cmd, void *arg)
{
	int status = -EINVAL;
	struct ser_i2c_data *pd = NULL;

	switch (cmd) {
	case SER_RESET:
		/* Reset the FPDLINK*/
		status = serlink_reset(client);
	break;

	case SER_CONFIGURE:
		/* Setup basic configuration 0 & 1*/
		status = serlink_setup_config(client, arg);
	break;

	case SER_GET_I2C_ADDR:
		/* get the serializer ID registered inside */
		status = serlink_get_i2c_id(client);
	break;

	case SER_VALIDATE_LINK:
		/* Check for Link present */
		status = serlink_validate_link(client);
	break;

	case SER_VALIDATE_PCLK:
		/* Check for Pixel Clock */
		status = serlink_validate_fclk(client);
	break;

	case SER_GET_DES_I2C_ADDR:
		/* Read auto populated Deserializer i2c ID*/
		status = serlink_read_deserializer(client, arg);
	break;

	case SER_READ:
		pd = (struct ser_i2c_data *)arg;
		if (pd)
			pd->data = status = i2c_read_le8(client,
							pd->addr);
	break;

	case SER_WRITE:
		pd = (struct ser_i2c_data *)arg;
		if (pd)
			status = i2c_write_le8(client, pd->addr,
							pd->data);
	break;

	case SER_REG_DUMP:
		status = serlink_dump_reg(client);
	break;

	default:
		return -EINVAL;
	}

	return status;

}

static void serlink_init_client(struct i2c_client *client)
{
	struct serlink_platform_data *data = i2c_get_clientdata(client);

	if (!data)
		dev_err(&client->dev, "init client data failed '%s'\n",
				client->name);

	data->addr = 0;
	data->data = 0;
}

static int serlink_probe(struct i2c_client *client,
			 const struct i2c_device_id *id)
{
	struct serlink_platform_data	*pdata;
	int status;

	pdata = devm_kzalloc(&client->dev, sizeof(struct serlink_platform_data),
			    GFP_KERNEL);
	if (!pdata)
		return -ENOMEM;

	i2c_set_clientdata(client, pdata);

	serlink_init_client(client);

	/* Register sysfs hooks */
	status = sysfs_create_group(&client->dev.kobj, &serlink_attr_group);
	if (status)
		goto fail;

	dev_err(&client->dev, "probe error %d for '%s'\n",
			status, client->name);

	return status;
fail:
	return -EINVAL;

}

static int serlink_remove(struct i2c_client *client)
{
	struct serlink_platform_data	*pdata = i2c_get_clientdata(client);
	kfree(pdata);

	sysfs_remove_group(&client->dev.kobj, &serlink_attr_group);

	return 0;
}


static const struct of_device_id serlink_dt_ids[] = {
	{.compatible = "ti,ds90uh925q", },
	{ }
};

MODULE_DEVICE_TABLE(of, serlink_dt_ids);

static struct i2c_driver serlink_driver = {
	.driver = {
		.name = "serlink",
		.owner = THIS_MODULE,
		.of_match_table	= serlink_dt_ids,
	},
	.probe = serlink_probe,
	.remove = serlink_remove,
	.command = serlink_command,
	.id_table = serlink_id,
};

static int __init serlink_init(void)
{
	return i2c_add_driver(&serlink_driver);
}
module_init(serlink_init);

static void __exit serlink_exit(void)
{
	i2c_del_driver(&serlink_driver);
}
module_exit(serlink_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Saket Dandawate");


