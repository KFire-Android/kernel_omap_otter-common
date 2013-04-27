/*
 * tlv320aic31xx-i2c.c  -- driver for TLV320AIC31XX TODO
 *
 * Author:	Mukund Navada <navada@ti.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 */

#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/pm_runtime.h>
#include <linux/slab.h>

#include <linux/mfd/tlv320aic3xxx-core.h>
#include <linux/mfd/tlv320aic31xx-registers.h>
#include <linux/mfd/tlv320aic3xxx-registers.h>


#ifdef CONFIG_PM
static int aic3xxx_suspend(struct device *dev)
{
	struct aic3xxx *aic3xxx = dev_get_drvdata(dev);

	aic3xxx->suspended = true;

	return 0;
}

static int aic3xxx_resume(struct device *dev)
{
	struct aic3xxx *aic3xxx = dev_get_drvdata(dev);

	aic3xxx->suspended = false;

	return 0;
}
#endif

int aic3xxx_i2c_read_device(struct aic3xxx *aic3xxx, u8 offset,
				void *dest, int count)
{
	struct i2c_client *i2c = to_i2c_client(aic3xxx->dev);
	int ret;
	ret = i2c_master_send(i2c, &offset, 1);
	if (ret < 0)
		return ret;

	if (ret != 1)
		return -EIO;


	ret = i2c_master_recv(i2c, dest, count);
	if (ret < 0)
		return ret;

	if (ret != count)
		return -EIO;
	return ret;
}
EXPORT_SYMBOL_GPL(aic3xxx_i2c_read_device);


int aic3xxx_i2c_write_device(struct aic3xxx *aic3xxx , u8 offset,
				const void *src, int count)
{
	struct i2c_client *i2c = to_i2c_client(aic3xxx->dev);
	u8 write_buf[count+1];
	int ret;

	write_buf[0] = offset;
	memcpy(&write_buf[1], src, count);
	ret = i2c_master_send(i2c, write_buf, count + 1);

	if (ret < 0)
		return ret;
	if (ret != (count + 1))
		return -EIO;

	return 0;
}
EXPORT_SYMBOL_GPL(aic3xxx_i2c_write_device);


static __devinit int aic3xxx_i2c_probe(struct i2c_client *i2c,
					const struct i2c_device_id *id)
{
	struct aic3xxx *aic3xxx;
	aic3xxx = kzalloc(sizeof(*aic3xxx), GFP_KERNEL);
	if (aic3xxx == NULL)
		return -ENOMEM;
	i2c_set_clientdata(i2c, aic3xxx);
	aic3xxx->dev = &i2c->dev;
	aic3xxx->type = id->driver_data;
	aic3xxx->irq = i2c->irq;

	return aic3xxx_device_init(aic3xxx);
}

static int __devexit aic3xxx_i2c_remove(struct i2c_client *i2c)
{

	struct aic3xxx *aic3xxx = dev_get_drvdata(&i2c->dev);
	aic3xxx_device_exit(aic3xxx);
	return 0;
}

static const struct i2c_device_id aic3xxx_i2c_id[] = {
	{ "tlv320aic31xx", TLV320AIC31XX },
	{ }
};
MODULE_DEVICE_TABLE(i2c, aic3xxx_i2c_id);

static UNIVERSAL_DEV_PM_OPS(aic3xxx_pm_ops, aic3xxx_suspend, aic3xxx_resume,
				NULL);

static struct i2c_driver aic3xxx_i2c_driver = {
	.driver = {
		.name	= "tlv320aic31xx",
		.owner	= THIS_MODULE,
		.pm	= &aic3xxx_pm_ops,
	},
	.probe		= aic3xxx_i2c_probe,
	.remove		= __devexit_p(aic3xxx_i2c_remove),
	.id_table	= aic3xxx_i2c_id,
};

static int __init aic3xxx_i2c_init(void)
{
	int ret;

	ret = i2c_add_driver(&aic3xxx_i2c_driver);
	if (ret != 0)
		pr_err("Failed to register aic3xxx I2C driver: %d\n", ret);

	return ret;
}
module_init(aic3xxx_i2c_init);

static void __exit aic3xxx_i2c_exit(void)
{
	i2c_del_driver(&aic3xxx_i2c_driver);
}
module_exit(aic3xxx_i2c_exit);

MODULE_DESCRIPTION("TLV320AIC31XX I2C bus interface");
MODULE_AUTHOR("Mukund Navada <navada@ti.com>");
MODULE_LICENSE("GPL");
