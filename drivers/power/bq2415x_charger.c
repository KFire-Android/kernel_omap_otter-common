/*
 * drivers/power/bq2415x_battery.c
 *
 * BQ24153 / BQ24156 battery charging driver
 *
 * Copyright (C) 2010 Texas Instruments, Inc.
 * Author: Texas Instruments, Inc.
 *
 * This package is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * THIS PACKAGE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/jiffies.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/i2c/twl.h>
#include <linux/i2c/bq2415x.h>

/* BQ24153 / BQ24156 / BQ24158 */
/* Status/Control Register */
#define REG_STATUS_CONTROL			0x00
#define		TIMER_RST			(1 << 7)
#define		ENABLE_STAT_PIN			(1 << 6)

/* Control Register */
#define REG_CONTROL_REGISTER			0x01
#define		INPUT_CURRENT_LIMIT_SHIFT	6
#define		ENABLE_ITERM_SHIFT		3

/* Control/Battery Voltage Register */
#define REG_BATTERY_VOLTAGE			0x02
#define		VOLTAGE_SHIFT			2

/* Vender/Part/Revision Register */
#define REG_PART_REVISION			0x03

/* Battery Termination/Fast Charge Current Register */
#define REG_BATTERY_CURRENT			0x04
#define		BQ24156_CURRENT_SHIFT		3
#define		BQ24153_CURRENT_SHIFT		4

/* Special Charger Voltage/Enable Pin Status Register */
#define REG_SPECIAL_CHARGER_VOLTAGE		0x05

/* Safety Limit Register */
#define REG_SAFETY_LIMIT			0x06
#define		MAX_CURRENT_SHIFT		4

#define BQ24153 (1 << 3)
#define BQ24156 (1 << 6)
#define BQ24158 (1 << 8)

#define DRIVER_NAME			"bq2415x"
static int timer_fault;

struct charge_params {
	unsigned long		currentmA;
	unsigned long		voltagemV;
	unsigned long		term_currentmA;
	unsigned long		enable_iterm;
	bool			enable;
};

struct bq2415x_device_info {
	struct device		*dev;
	struct i2c_client	*client;
	struct charge_params	params;
	struct delayed_work	bq2415x_charger_work;
	struct notifier_block	nb;

	unsigned short		status_reg;
	unsigned short		control_reg;
	unsigned short		voltage_reg;
	unsigned short		bqchip_version;
	unsigned short		current_reg;
	unsigned short		special_charger_reg;

	unsigned int		cin_limit;
	unsigned int		currentmA;
	unsigned int		voltagemV;
	unsigned int		max_currentmA;
	unsigned int		max_voltagemV;
	unsigned int		term_currentmA;

	bool			cfg_params;
	bool			enable_iterm;
	bool			active;
};

static int bq2415x_write_block(struct bq2415x_device_info *di, u8 *value,
						u8 reg, unsigned num_bytes)
{
	struct i2c_msg msg[1];
	int ret;

	*value		= reg;

	msg[0].addr	= di->client->addr;
	msg[0].flags	= 0;
	msg[0].buf	= value;
	msg[0].len	= num_bytes + 1;

	ret = i2c_transfer(di->client->adapter, msg, 1);

	/* i2c_transfer returns number of messages transferred */
	if (ret != 1) {
		dev_err(di->dev,
			"i2c_write failed to transfer all messages\n");
		if (ret < 0)
			return ret;
		else
			return -EIO;
	} else {
		return 0;
	}
}

static int bq2415x_read_block(struct bq2415x_device_info *di, u8 *value,
						u8 reg, unsigned num_bytes)
{
	struct i2c_msg msg[2];
	u8 buf;
	int ret;

	buf		= reg;

	msg[0].addr	= di->client->addr;
	msg[0].flags	= 0;
	msg[0].buf	= &buf;
	msg[0].len	= 1;

	msg[1].addr	= di->client->addr;
	msg[1].flags	= I2C_M_RD;
	msg[1].buf	= value;
	msg[1].len	= num_bytes;

	ret = i2c_transfer(di->client->adapter, msg, 2);

	/* i2c_transfer returns number of messages transferred */
	if (ret != 2) {
		dev_err(di->dev,
			"i2c_write failed to transfer all messages\n");
		if (ret < 0)
			return ret;
		else
			return -EIO;
	} else {
		return 0;
	}
}

static int bq2415x_write_byte(struct bq2415x_device_info *di, u8 value, u8 reg)
{
	/* 2 bytes offset 1 contains the data offset 0 is used by i2c_write */
	u8 temp_buffer[2] = { 0 };

	/* offset 1 contains the data */
	temp_buffer[1] = value;
	return bq2415x_write_block(di, temp_buffer, reg, 1);
}

static int bq2415x_read_byte(struct bq2415x_device_info *di, u8 *value, u8 reg)
{
	return bq2415x_read_block(di, value, reg, 1);
}


static void bq2415x_config_status_reg(struct bq2415x_device_info *di)
{
	di->status_reg = (TIMER_RST | ENABLE_STAT_PIN);
	bq2415x_write_byte(di, di->status_reg, REG_STATUS_CONTROL);
	return;
}

static int bq2415x_charger_event(struct notifier_block *nb, unsigned long event,
				void *_data)
{

	struct bq2415x_device_info *di;
	struct charge_params *data;
	u8 read_reg[7] = {0};
	int ret = 0;

	di = container_of(nb, struct bq2415x_device_info, nb);
	data = &di->params;
	di->cfg_params = 1;

	if (event & BQ2415x_CHARGER_FAULT) {
		bq2415x_read_block(di, &read_reg[0], 0, 7);
		ret = read_reg[0] & 0x3F;
		return ret;
	}

	if (data->enable == 0) {
		di->currentmA = data->currentmA;
		di->voltagemV = data->voltagemV;
		di->enable_iterm = data->enable_iterm;
	}

	if ((event & BQ2415x_START_CHARGING) && (di->active == 0)) {
		schedule_delayed_work(&di->bq2415x_charger_work,
						msecs_to_jiffies(0));
		di->active = 1;
	}

	if (event & BQ2415x_STOP_CHARGING) {
		cancel_delayed_work(&di->bq2415x_charger_work);
		di->active = 0;
	}

	if (event & BQ2415x_RESET_TIMER) {
		/* reset 32 second timer */
		bq2415x_config_status_reg(di);
	}

	return ret;
}


static void bq2415x_config_control_reg(struct bq2415x_device_info *di)
{
	u8 Iin_limit;

	if (di->cin_limit <= 100)
		Iin_limit = 0;
	else if (di->cin_limit > 100 && di->cin_limit <= 500)
		Iin_limit = 1;
	else if (di->cin_limit > 500 && di->cin_limit <= 800)
		Iin_limit = 2;
	else
		Iin_limit = 3;

	di->control_reg = ((Iin_limit << INPUT_CURRENT_LIMIT_SHIFT)
				| (di->enable_iterm << ENABLE_ITERM_SHIFT));
	bq2415x_write_byte(di, di->control_reg, REG_CONTROL_REGISTER);
	return;
}

static void bq2415x_config_voltage_reg(struct bq2415x_device_info *di)
{
	unsigned int voltagemV;
	u8 Voreg;

	voltagemV = di->voltagemV;
	if (voltagemV < 3500)
		voltagemV = 3500;
	else if (voltagemV > 4440)
		voltagemV = 4440;

	Voreg = (voltagemV - 3500)/20;
	di->voltage_reg = (Voreg << VOLTAGE_SHIFT);
	bq2415x_write_byte(di, di->voltage_reg, REG_BATTERY_VOLTAGE);
	return;
}

static void bq2415x_config_current_reg(struct bq2415x_device_info *di)
{
	unsigned int currentmA = 0;
	unsigned int term_currentmA = 0;
	u8 Vichrg = 0;
	u8 shift = 0;
	u8 Viterm = 0;

	currentmA = di->currentmA;
	term_currentmA = di->term_currentmA;

	if (currentmA < 550)
		currentmA = 550;

	if ((di->bqchip_version & (BQ24153 | BQ24158))) {
		shift = BQ24153_CURRENT_SHIFT;
		if (currentmA > 1250)
			currentmA = 1250;
	}

	if ((di->bqchip_version & BQ24156)) {
		shift = BQ24156_CURRENT_SHIFT;
		if (currentmA > 1550)
			currentmA = 1550;
	}

	if (term_currentmA > 350)
		term_currentmA = 350;

	Vichrg = (currentmA - 550)/100;
	Viterm = term_currentmA/50;

	di->current_reg = (Vichrg << shift | Viterm);
	bq2415x_write_byte(di, di->current_reg, REG_BATTERY_CURRENT);
	return;
}

static void bq2415x_config_special_charger_reg(struct bq2415x_device_info *di)
{
	u8 Vsreg = 2;				/* 160/80 */

	di->special_charger_reg = Vsreg;
	bq2415x_write_byte(di, di->special_charger_reg,
					REG_SPECIAL_CHARGER_VOLTAGE);
	return;
}

static void bq2415x_config_safety_reg(struct bq2415x_device_info *di,
						unsigned int max_currentmA,
						unsigned int max_voltagemV)
{
	u8 Vmchrg;
	u8 Vmreg;
	u8 limit_reg;

	if (max_currentmA < 550)
		max_currentmA = 550;
	else if (max_currentmA > 1550)
		max_currentmA = 1550;


	if (max_voltagemV < 4200)
		max_voltagemV = 4200;
	else if (max_voltagemV > 4440)
		max_voltagemV = 4440;

	di->max_voltagemV = max_voltagemV;
	di->max_currentmA = max_currentmA;
	di->voltagemV = max_voltagemV;
	di->currentmA = max_currentmA;

	Vmchrg = (max_currentmA - 550)/100;
	Vmreg = (max_voltagemV - 4200)/20;
	limit_reg = ((Vmchrg << MAX_CURRENT_SHIFT) | Vmreg);
	bq2415x_write_byte(di, limit_reg, REG_SAFETY_LIMIT);
	return;
}

static void
bq2415x_charger_update_status(struct bq2415x_device_info *di)
{
	u8 read_reg[7] = {0};

	timer_fault = 0;
	bq2415x_read_block(di, &read_reg[0], 0, 7);


	if ((read_reg[0] & 0x30) == 0x20)
		dev_dbg(di->dev, "CHARGE DONE\n");

	if ((read_reg[0] & 0x7) == 0x6)
		timer_fault = 1;

	if (read_reg[0] & 0x7) {
		di->cfg_params = 1;
		dev_err(di->dev, "CHARGER FAULT %x\n", read_reg[0]);
	}

	if ((timer_fault == 1) || (di->cfg_params == 1)) {
		bq2415x_write_byte(di, di->control_reg, REG_CONTROL_REGISTER);
		bq2415x_write_byte(di, di->voltage_reg, REG_BATTERY_VOLTAGE);
		bq2415x_write_byte(di, di->current_reg, REG_BATTERY_CURRENT);
		bq2415x_config_special_charger_reg(di);
		di->cfg_params = 0;
	}

	/* reset 32 second timer */
	bq2415x_config_status_reg(di);

	return;
}

static void bq2415x_charger_work(struct work_struct *work)
{
	struct bq2415x_device_info *di = container_of(work,
		struct bq2415x_device_info, bq2415x_charger_work.work);

	bq2415x_charger_update_status(di);
	schedule_delayed_work(&di->bq2415x_charger_work,
						msecs_to_jiffies(30000));
}


static ssize_t bq2415x_set_enable_itermination(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t count)
{
	long val;
	int status = count;
	struct bq2415x_device_info *di = dev_get_drvdata(dev);

	if ((strict_strtol(buf, 10, &val) < 0) || (val < 0) || (val > 1))
		return -EINVAL;
	di->enable_iterm = val;
	bq2415x_config_control_reg(di);

	return status;
}

static ssize_t bq2415x_show_enable_itermination(struct device *dev,
				  struct device_attribute *attr,
				  char *buf)
{
	unsigned long val;
	struct bq2415x_device_info *di = dev_get_drvdata(dev);

	val = di->enable_iterm;
	return sprintf(buf, "%lu\n", val);
}

static ssize_t bq2415x_set_cin_limit(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t count)
{
	long val;
	int status = count;
	struct bq2415x_device_info *di = dev_get_drvdata(dev);

	if ((strict_strtol(buf, 10, &val) < 0) || (val < 100)
					|| (val > di->max_currentmA))
		return -EINVAL;
	di->cin_limit = val;
	bq2415x_config_control_reg(di);

	return status;
}

static ssize_t bq2415x_show_cin_limit(struct device *dev,
				  struct device_attribute *attr,
				  char *buf)
{
	unsigned long val;
	struct bq2415x_device_info *di = dev_get_drvdata(dev);

	val = di->cin_limit;
	return sprintf(buf, "%lu\n", val);
}

static ssize_t bq2415x_set_regulation_voltage(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t count)
{
	long val;
	int status = count;
	struct bq2415x_device_info *di = dev_get_drvdata(dev);

	if ((strict_strtol(buf, 10, &val) < 0) || (val < 3500)
					|| (val > di->max_voltagemV))
		return -EINVAL;
	di->voltagemV = val;
	bq2415x_config_voltage_reg(di);

	return status;
}

static ssize_t bq2415x_show_regulation_voltage(struct device *dev,
				  struct device_attribute *attr,
				  char *buf)
{
	unsigned long val;
	struct bq2415x_device_info *di = dev_get_drvdata(dev);

	val = di->voltagemV;
	return sprintf(buf, "%lu\n", val);
}

static ssize_t bq2415x_set_charge_current(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t count)
{
	long val;
	int status = count;
	struct bq2415x_device_info *di = dev_get_drvdata(dev);

	if ((strict_strtol(buf, 10, &val) < 0) || (val < 550)
					|| (val > di->max_currentmA))
		return -EINVAL;
	di->currentmA = val;
	bq2415x_config_current_reg(di);

	return status;
}

static ssize_t bq2415x_show_charge_current(struct device *dev,
				  struct device_attribute *attr,
				  char *buf)
{
	unsigned long val;
	struct bq2415x_device_info *di = dev_get_drvdata(dev);

	val = di->currentmA;
	return sprintf(buf, "%lu\n", val);
}

static ssize_t bq2415x_set_termination_current(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t count)
{
	long val;
	int status = count;
	struct bq2415x_device_info *di = dev_get_drvdata(dev);

	if ((strict_strtol(buf, 10, &val) < 0) || (val < 0) || (val > 350))
		return -EINVAL;
	di->term_currentmA = val;
	bq2415x_config_current_reg(di);

	return status;
}

static ssize_t bq2415x_show_termination_current(struct device *dev,
				  struct device_attribute *attr,
				  char *buf)
{
	unsigned long val;
	struct bq2415x_device_info *di = dev_get_drvdata(dev);

	val = di->term_currentmA;
	return sprintf(buf, "%lu\n", val);
}

static DEVICE_ATTR(enable_itermination, S_IWUSR | S_IRUGO,
				bq2415x_show_enable_itermination,
				bq2415x_set_enable_itermination);
static DEVICE_ATTR(cin_limit, S_IWUSR | S_IRUGO,
				bq2415x_show_cin_limit,
				bq2415x_set_cin_limit);
static DEVICE_ATTR(regulation_voltage, S_IWUSR | S_IRUGO,
				bq2415x_show_regulation_voltage,
				bq2415x_set_regulation_voltage);
static DEVICE_ATTR(charge_current, S_IWUSR | S_IRUGO,
				bq2415x_show_charge_current,
				bq2415x_set_charge_current);
static DEVICE_ATTR(termination_current, S_IWUSR | S_IRUGO,
				bq2415x_show_termination_current,
				bq2415x_set_termination_current);

static struct attribute *bq2415x_attributes[] = {
	&dev_attr_enable_itermination.attr,
	&dev_attr_cin_limit.attr,
	&dev_attr_regulation_voltage.attr,
	&dev_attr_charge_current.attr,
	&dev_attr_termination_current.attr,
	NULL,
};

static const struct attribute_group bq2415x_attr_group = {
	.attrs = bq2415x_attributes,
};

static int __devinit bq2415x_charger_probe(struct i2c_client *client,
				 const struct i2c_device_id *id)
{
	struct bq2415x_device_info *di;
	struct bq2415x_platform_data *pdata = client->dev.platform_data;
	int ret;
	u8 read_reg = 0;

	di = kzalloc(sizeof(*di), GFP_KERNEL);
	if (!di)
		return -ENOMEM;

	di->dev = &client->dev;
	di->client = client;

	i2c_set_clientdata(client, di);

	ret = bq2415x_read_byte(di, &read_reg, REG_PART_REVISION);

	if (ret < 0) {
		dev_err(&client->dev, "chip not present at address %x\n",
								client->addr);
		ret = -EINVAL;
		goto err_kfree;
	}

	if ((read_reg & 0x18) == 0x00 && (client->addr == 0x6a))
		di->bqchip_version = BQ24156;

	if (di->bqchip_version == 0) {
		dev_dbg(&client->dev, "unknown bq chip\n");
		dev_dbg(&client->dev, "Chip address %x", client->addr);
		dev_dbg(&client->dev, "bq chip version reg value %x", read_reg);
		ret = -EINVAL;
		goto err_kfree;
	}

	di->nb.notifier_call = bq2415x_charger_event;
	bq2415x_config_safety_reg(di, pdata->max_charger_currentmA,
						pdata->max_charger_voltagemV);
	di->cin_limit = 900;
	di->term_currentmA = pdata->termination_currentmA;
	bq2415x_config_control_reg(di);
	bq2415x_config_voltage_reg(di);
	bq2415x_config_current_reg(di);


	INIT_DELAYED_WORK_DEFERRABLE(&di->bq2415x_charger_work,
				bq2415x_charger_work);

	di->active = 0;
	di->params.enable = 1;
	di->cfg_params = 1;
	di->enable_iterm = 1;

	ret = bq2415x_read_byte(di, &read_reg, REG_SPECIAL_CHARGER_VOLTAGE);
	if (!(read_reg & 0x08)) {
		di->active = 1;
		schedule_delayed_work(&di->bq2415x_charger_work, 0);
	}

	ret = sysfs_create_group(&client->dev.kobj, &bq2415x_attr_group);
	if (ret)
		dev_dbg(&client->dev, "could not create sysfs files\n");

	twl6030_register_notifier(&di->nb, 1);

	return 0;

err_kfree:
	kfree(di);

	return ret;
}

static int __devexit bq2415x_charger_remove(struct i2c_client *client)
{
	struct bq2415x_device_info *di = i2c_get_clientdata(client);

	sysfs_remove_group(&client->dev.kobj, &bq2415x_attr_group);
	cancel_delayed_work(&di->bq2415x_charger_work);
	flush_scheduled_work();
	twl6030_unregister_notifier(&di->nb, 1);
	kfree(di);

	return 0;
}

static const struct i2c_device_id bq2415x_id[] = {
	{ "bq24156", 0 },
	{},
};

static struct i2c_driver bq2415x_charger_driver = {
	.probe		= bq2415x_charger_probe,
	.remove		= __devexit_p(bq2415x_charger_remove),
	.id_table	= bq2415x_id,
	.driver		= {
		.name	= "bq2415x_charger",
	},
};

static int __init bq2415x_charger_init(void)
{
	return i2c_add_driver(&bq2415x_charger_driver);
}
module_init(bq2415x_charger_init);

static void __exit bq2415x_charger_exit(void)
{
	i2c_del_driver(&bq2415x_charger_driver);
}
module_exit(bq2415x_charger_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Texas Instruments Inc");
