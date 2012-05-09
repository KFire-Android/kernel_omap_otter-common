/*
 *  smb347_charger.c
 *
 *  Copyright (C) 2011 Samsung Electronics
 *  SangYoung Son <hello.son@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/err.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/power_supply.h>
#include <linux/power/smb347_charger.h>
#include <linux/gpio.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/battery.h>

/* Slave address */
#define SMB347_SLAVE_ADDR		0x0C

/* SMB347 Registers. */
#define SMB347_CHARGE_CURRENT		0X00
#define SMB347_INPUT_CURRENTLIMIT	0X01
#define SMB347_VARIOUS_FUNCTIONS	0X02
#define SMB347_FLOAT_VOLTAGE		0X03
#define SMB347_CHARGE_CONTROL		0X04
#define SMB347_STAT_TIMERS_CONTROL	0x05
#define SMB347_PIN_ENABLE_CONTROL	0x06
#define SMB347_THERM_CONTROL_A		0x07
#define SMB347_SYSOK_USB30_SELECTION	0x08
#define SMB347_OTHER_CONTROL_A	0x09
#define SMB347_OTG_TLIM_THERM_CONTROL	0x0A
#define SMB347_LIMIT_CELL_TEMPERATURE_MONITOR	0x0B
#define SMB347_FAULT_INTERRUPT	0x0C
#define SMB347_STATUS_INTERRUPT	0x0D
#define SMB347_I2C_BUS_SLAVE_ADDR	0x0E

#define SMB347_COMMAND_A	0x30
#define SMB347_COMMAND_B	0x31
#define SMB347_COMMAND_C	0x33
#define SMB347_INTERRUPT_STATUS_A	0x35
#define SMB347_INTERRUPT_STATUS_B	0x36
#define SMB347_INTERRUPT_STATUS_C	0x37
#define SMB347_INTERRUPT_STATUS_D	0x38
#define SMB347_INTERRUPT_STATUS_E	0x39
#define SMB347_INTERRUPT_STATUS_F	0x3A
#define SMB347_STATUS_A	0x3B
#define SMB347_STATUS_B	0x3C
#define SMB347_STATUS_C	0x3D
#define SMB347_STATUS_D	0x3E
#define SMB347_STATUS_E	0x3F

/* Status register C */
#define SMB347_CHARGING_ENABLE	(1 << 0)
#define SMB347_CHARGING_STATUS	(1 << 5)
#define SMB347_CHARGER_ERROR	(1 << 6)

#define CHARGER_STATUS_FULL             0x1
#define CHARGER_STATUS_CHARGERERR       0x2
#define CHARGER_STATUS_USB_FAIL         0x3
#define CHARGER_VBATT_UVLO              0x4

struct smb347_chg_data {
	struct device *dev;
	struct i2c_client *client;
	struct smb_charger_data *pdata;
	struct smb_charger_callbacks callbacks;
	struct work_struct charger_work;
};

static int smb347_i2c_read(struct i2c_client *client, u8 reg, u8 *data)
{
	int ret = 0;

	if (!client) {
		dev_err(&client->dev, "%s: err\n", __func__);
		return -ENODEV;
	}

	ret = i2c_smbus_read_byte_data(client, reg);
	if (ret < 0)
		dev_err(&client->dev, "%s: err %d\n", __func__, ret);

	*data = ret & 0xff;
	return 0;
}

static int smb347_i2c_write(struct i2c_client *client, u8 reg, u8 data)
{
	int ret = 0;

	if (!client) {
		dev_err(&client->dev, "%s: err\n", __func__);
		return -ENODEV;
	}

	ret = i2c_smbus_write_byte_data(client, reg, data);
	if (ret < 0)
		dev_err(&client->dev, "%s: err %d\n", __func__, ret);

	return ret;
}

static void smb347_test_read(struct smb347_chg_data *chg)
{
	u8 data = 0;
	u32 addr = 0;
	pr_info("%s\n", __func__);

	for (addr = 0; addr <= 0x0E; addr++) {
		smb347_i2c_read(chg->client, addr, &data);
		dev_info(&chg->client->dev,
			"smb347 addr : 0x%02x data : 0x%02x\n",	addr, data);
	}

	for (addr = 0x30; addr <= 0x3F; addr++) {
		smb347_i2c_read(chg->client, addr, &data);
		dev_info(&chg->client->dev,
			"smb347 addr : 0x%02x data : 0x%02x\n",	addr, data);
	}
}

static void smb347_charger_init(struct smb347_chg_data *chg)
{
	/* Allow volatile writes to CONFIG registers */
	smb347_i2c_write(chg->client, SMB347_COMMAND_A, 0x80);

	/* Command B : USB1 mode, USB mode */
	smb347_i2c_write(chg->client, SMB347_COMMAND_B, 0x00);

	/* Charge curr : Fast-chg 2200mA */
	/* Pre-charge curr 250mA, Term curr 250mA */
	smb347_i2c_write(chg->client, SMB347_CHARGE_CURRENT, 0xDD);

	/* Pin enable control : Charger enable control EN Pin - Active Low */
	/*  : USB5/1/HC or USB9/1.5/HC Control - Register Control */
	/*  : USB5/1/HC Input state - Tri-state Input */
	smb347_i2c_write(chg->client, SMB347_PIN_ENABLE_CONTROL, 0x60);

	/* Input current limit : DCIN 1800mA, USBIN HC 1800mA */
	smb347_i2c_write(chg->client, SMB347_INPUT_CURRENTLIMIT, 0x66);

	/* Various func. : USBIN primary input, VCHG func. enable */
	smb347_i2c_write(chg->client, SMB347_VARIOUS_FUNCTIONS, 0x87);

	/* Float voltage : 4.2V */
	smb347_i2c_write(chg->client, SMB347_FLOAT_VOLTAGE, 0x63);

	/* Charge control : Auto recharge disable, APSD disable */
	smb347_i2c_write(chg->client, SMB347_CHARGE_CONTROL, 0x80);

	/* STAT, Timer control : STAT active low, Complete time out 1527min. */
	smb347_i2c_write(chg->client, SMB347_STAT_TIMERS_CONTROL, 0x1A);

	/* Therm control : Therm monitor disable */
	smb347_i2c_write(chg->client, SMB347_THERM_CONTROL_A, 0x7F);

	/* USB selection : USB2.0(100mA/500mA), INOK polarity */
	/* Active low */
	smb347_i2c_write(chg->client, SMB347_SYSOK_USB30_SELECTION, 0x08);

	/* Other control */
	smb347_i2c_write(chg->client, SMB347_OTHER_CONTROL_A, 0x1D);

	/* OTG tlim therm control */
	smb347_i2c_write(chg->client, SMB347_OTG_TLIM_THERM_CONTROL, 0x3F);

	/* Limit cell temperature */
	smb347_i2c_write(chg->client,
			SMB347_LIMIT_CELL_TEMPERATURE_MONITOR, 0x01);

	/* Fault interrupt : Clear */
	smb347_i2c_write(chg->client, SMB347_FAULT_INTERRUPT, 0x00);

	/* STATUS ingerrupt : Clear */
	smb347_i2c_write(chg->client, SMB347_STATUS_INTERRUPT, 0x00);
}

static int smb347_read_status(struct smb_charger_callbacks *ptr)
{
	struct smb347_chg_data *chg = container_of(ptr,
			struct smb347_chg_data, callbacks);

	u8 res = 0;
	u8 reg_c;
	int ret;

	ret = smb347_i2c_read(chg->client, SMB347_STATUS_C, &reg_c);
	if (ret < 0) {
		dev_err(&chg->client->dev, "%s: I2C Read fail addr : 0x%x\n",
			__func__, SMB347_STATUS_C);
		msleep(50);
		smb347_i2c_read(chg->client, SMB347_STATUS_C, &reg_c);
	}

	dev_info(&chg->client->dev,
		"addr : 0x%x, data : 0x%x\n", SMB347_STATUS_C, reg_c);

	if (reg_c & SMB347_CHARGER_ERROR)
		res = CHARGER_STATUS_CHARGERERR;
	else if (reg_c & SMB347_CHARGING_STATUS)
		res = CHARGER_STATUS_FULL;

	return res;
}

static void smb347_set_charging_state(struct smb_charger_callbacks *ptr,
		int cable_status)
{
	struct smb347_chg_data *chg = container_of(ptr,
			struct smb347_chg_data, callbacks);

	if (cable_status) {
		/* Init smb347 charger */
		smb347_charger_init(chg);

		switch (cable_status) {
		case CABLE_TYPE_AC:
			/* Input current limit : DCIN 1800mA, USBIN HC 1800mA */
			smb347_i2c_write(chg->client,
				SMB347_INPUT_CURRENTLIMIT, 0x66);

			/* CommandB : High-current mode */
			smb347_i2c_write(chg->client, SMB347_COMMAND_B, 0x03);

			pr_info("%s : 1.8A charging enable\n", __func__);
			break;
		case CABLE_TYPE_USB:
			/* CommandB : USB5 */
			smb347_i2c_write(chg->client, SMB347_COMMAND_B, 0x02);
			pr_info("%s : LOW(USB5) charging enable\n", __func__);
			break;
		default:
			/* CommandB : USB1 */
			smb347_i2c_write(chg->client, SMB347_COMMAND_B, 0x00);
			pr_info("%s : LOW(USB1) charging enable\n", __func__);
			break;
		}
	}
}

int smb347_get_charging_current(struct smb347_chg_data *chg)
{
	u8 data = 0;
	int get_current = 0;

	smb347_i2c_read(chg->client, SMB347_CHARGE_CURRENT, &data);
	switch (data >> 5) {
	case 0:
		get_current = 700;
		break;
	case 1:
		get_current = 900;
		break;
	case 2:
		get_current = 1200;
		break;
	case 3:
		get_current = 1500;
		break;
	case 4:
		get_current = 1800;
		break;
	case 5:
		get_current = 2000;
		break;
	case 6:
		get_current = 2200;
		break;
	case 7:
		get_current = 2500;
		break;
	default:
		get_current = 700;
		break;
	}
	pr_debug("%s: Get charging current as %dmA.\n", __func__, get_current);
	return get_current;
}

static int smb347_i2c_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
	struct smb347_chg_data *chg;
	int ret = 0;

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE))
		return -EIO;

	dev_info(&client->dev, "%s : smb347 Charger Driver Loading\n",
		__func__);

	chg = kzalloc(sizeof(struct smb347_chg_data), GFP_KERNEL);
	if (!chg)
		return -ENOMEM;

	chg->client = client;
	if (!chg->client) {
		dev_err(&client->dev, "%s : No client\n", __func__);
		ret = -EINVAL;
		goto err_client;
	} else {
		chg->dev = &client->dev;
	}

	chg->pdata = client->dev.platform_data;
	if (!chg->pdata) {
		dev_err(&client->dev, "%s : No platform data supplied\n",
			__func__);
		ret = -EINVAL;
		goto err_pdata;
	}

	i2c_set_clientdata(client, chg);

	chg->callbacks.set_charging_state = smb347_set_charging_state;
	chg->callbacks.get_status_reg = smb347_read_status;
	if (chg->pdata->register_callbacks)
		chg->pdata->register_callbacks(&chg->callbacks);

	pr_info("smb347 charger initialized.\n");

	return 0;

err_pdata:
err_client:
	kfree(chg);
	return ret;
}

static int __devexit smb347_remove(struct i2c_client *client)
{
	struct smb347_chg_data *chg = i2c_get_clientdata(client);

	if (chg->pdata && chg->pdata->unregister_callbacks)
		chg->pdata->unregister_callbacks();

	kfree(chg);
	return 0;
}

static const struct i2c_device_id smb347_id[] = {
	{ "smb347-charger", 0 },
};


static struct i2c_driver smb347_i2c_driver = {
	.driver = {
		.owner	= THIS_MODULE,
		.name	= "smb347-charger",
	},
	.probe		= smb347_i2c_probe,
	.remove		= smb347_remove,
	.id_table	= smb347_id,
};


MODULE_DEVICE_TABLE(i2c, smb347_id);

static int __init smb347_init(void)
{
	return i2c_add_driver(&smb347_i2c_driver);
}

static void __exit smb347_exit(void)
{
	i2c_del_driver(&smb347_i2c_driver);
}

module_init(smb347_init);
module_exit(smb347_exit);

MODULE_AUTHOR("SangYoung Son <hello.son@samsung.com>");
MODULE_DESCRIPTION("smb347 charger driver");
MODULE_LICENSE("GPL");
