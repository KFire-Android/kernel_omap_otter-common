/*
 * SMSC_ECE1099 Keypad driver
 *
 * Copyright (C) 2011 Texas Instruments Incorporated - http://www.ti.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/i2c.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/input.h>
#include <linux/gpio.h>
#include <linux/slab.h>
#include <linux/jiffies.h>
#include <linux/input/matrix_keypad.h>
#include <linux/i2c/smsc.h>
#include <linux/delay.h>

/* ECE1099 Register summary */
#define SMSC_GPIO_G1_INPUT	0x00
#define SMSC_GPIO_G2_INPUT	0x01
#define SMSC_GPIO_G3_INPUT	0x02
#define SMSC_GPIO_G4_INPUT	0x03
#define SMSC_GPIO_G1_OUTPUT	0x05
#define SMSC_GPIO_G2_OUTPUT	0x06
#define SMSC_GPIO_G3_OUTPUT	0x07
#define SMSC_GPIO_G4_OUTPUT	0x08
#define SMSC_GPIO_G1_INT_MASK	0x37
#define SMSC_GPIO_G2_INT_MASK	0x38
#define SMSC_GPIO_G3_INT_MASK	0x39
#define SMSC_GPIO_G4_INT_MASK	0x3A
#define SMSC_GPIO_G1_BASEADDR	0x0A
#define SMSC_GPIO_G2_BASEADDR	0x12
#define SMSC_GPIO_G3_BASEADDR	0x1A
#define SMSC_GPIO_G4_BASEADDR	0x22
#define SMSC_GPIO_G1_INT_STAT	0x32
#define SMSC_GPIO_G2_INT_STAT	0x33
#define SMSC_GPIO_G3_INT_STAT	0x34
#define SMSC_GPIO_G4_INT_STAT	0x35
#define SMSC_KSO_SELECT		0x40
#define SMSC_KSI_INPUT		0x41
#define SMSC_KSI_STATUS		0x42
#define SMSC_KSI_MASK		0x43
#define SMSC_RESET		0xF5
#define SMSC_TEST		0xF6
#define SMSC_GRP_INT		0xF9
#define SMSC_CLK_CTRL		0xFA
#define SMSC_WKUP_CTRL		0xFB
#define SMSC_DEVICE_ID		0xFC
#define SMSC_DEV_VERSION	0xFD
#define SMSC_VENDOR_ID_LSB	0xFE
#define SMSC_VENDOR_ID_MSB	0xFF

/* GPIO configuration register flags */
#define SMSC_GPIO_PU		(1 << 0)
#define SMSC_GPIO_POL		(1 << 2)
#define SMSC_GPIO_IRQ_RISING	(0x1 << 3)
#define SMSC_GPIO_IRQ_FALLING	(0x2 << 3)
#define SMSC_GPIO_IRQ_EDGE	(0x3 << 3)
#define SMSC_GPIO_OUT_CONF	(1 << 4)
#define SMSC_GPIO_DIR		(1 << 5)
#define SMSC_GPIO_ALT_FN	(1 << 6)

/* ECE1099 Register Clock control flags */
#define SMSC_CLOCK_INT_SMSBUS	(0x3 << 0)
#define SMSC_CLOCK_INT_BCLINK	(0x2 << 0)
#define SMSC_CLOCK_OSC		(1 << 2)
#define SMSC_CLOCK_ARA		(1 << 4)

/* Miscellaneaous register masks */
#define SMSC_KSO_ALL_LOW	0x20
#define SMSC_SET_LOW_PWR	0x0B
#define SMSC_SET_HIGH		0xFF
#define SMSC_SET_LOW		0x00
#define SMSC_KSO_EVAL		0x00

#define KEYPRESS_TIME          200
#define ROW_SHIFT              4
#define GPIO_BRANK_SIZE		7

struct smsc_keypad {
	unsigned int last_key_state[16];
	unsigned int last_col;
	unsigned int last_key_ms[16];
	unsigned short  keymap[128];
	struct i2c_client *client;
	struct input_dev *input;
	int rows, cols;
	unsigned irq;
	unsigned gpio;
	struct device *dbg_dev;
};

static struct i2c_client *kp_client;

static int smsc_write_data(int reg, int reg_data)
{
	int ret;

	ret = i2c_smbus_write_byte_data(kp_client, reg, reg_data);
	if (ret < 0)
		dev_err(&kp_client->dev, "smbus write error!!!\n");

	return ret;
}

static int smsc_read_data(int reg)
{
	int ret;

	ret = i2c_smbus_read_byte_data(kp_client, reg);
	if (ret < 0)
		dev_err(&kp_client->dev, "smbus read error!!!\n");

	return ret;
}

static void smsc_kp_scan(struct smsc_keypad *kp)
{
	struct input_dev *input = kp->input;
	int i, j;
	int row, col;
	int temp, code;
	unsigned int new_state[16];
	unsigned int bits_changed;
	int this_ms;

	/* Disable smsc keypad interrupts */
	smsc_write_data(SMSC_KSI_MASK, SMSC_SET_LOW);
	/* Clear all KSI Interrupts */
	smsc_write_data(SMSC_KSI_STATUS, SMSC_SET_HIGH);


	/* Scan for row and column */
	for (i = 0; i < kp->cols; i++) {
		smsc_write_data(SMSC_KSO_SELECT, SMSC_KSO_EVAL + i);
		/* Read Row Status */
		temp = smsc_read_data(SMSC_KSI_INPUT);
		if (temp != SMSC_SET_HIGH) {
			col = i;
			for (j = 0; j < kp->rows; j++) {
				if ((temp & 0x01) == 0x00) {
					row = j;
					new_state[col] =  (1 << row);
					bits_changed =
						kp->last_key_state[col] ^ new_state[col];
					this_ms = jiffies_to_msecs(jiffies);
					if (bits_changed != 0 || (!bits_changed &&
						((this_ms - kp->last_key_ms[col]) >= KEYPRESS_TIME))) {
						code = MATRIX_SCAN_CODE(row, col, ROW_SHIFT);
						input_event(input, EV_MSC, MSC_SCAN, code);
						input_report_key(input, kp->keymap[code], 1);
						input_report_key(input, kp->keymap[code], 0);
						kp->last_key_state[col] = new_state[col];
						if (kp->last_col != col)
							kp->last_key_state[kp->last_col] = 0;
						kp->last_key_ms[col] = this_ms;
					}
				}
				temp = temp >> 1;
			}
		}
	}
	input_sync(input);

	smsc_write_data(SMSC_KSI_MASK, SMSC_SET_HIGH);
	/* Set up Low Power Mode (Wake-up) (0xFB) */
	smsc_write_data(SMSC_WKUP_CTRL, SMSC_SET_LOW_PWR);
	/*Enable Keypad Scan (generate interrupt on key press) (0x40)*/
	smsc_write_data(SMSC_KSO_SELECT, SMSC_KSO_ALL_LOW);
}

static irqreturn_t do_kp_irq(int irq, void *_kp)
{
	struct smsc_keypad *kp = _kp;
	int int_status;

	int_status = smsc_read_data(SMSC_KSI_STATUS);
	if (!int_status)
		return IRQ_NONE;

	smsc_kp_scan(kp);

	return IRQ_HANDLED;
}

static int  __devinit smsc_kp_initialize(struct smsc_keypad *kp)
{
	int ret = 0;
	int gpio, mask, reg;

	/* Disable all GPIO interrupts */
	for (reg = SMSC_GPIO_G1_INT_MASK; reg <= SMSC_GPIO_G4_INT_MASK; reg++) {
		ret = smsc_write_data(reg, SMSC_SET_LOW);
		if (ret < 0)
			goto exit;
	}

	/* Clear all GPIO interrupt status */
	for (reg = SMSC_GPIO_G1_INT_STAT; reg <= SMSC_GPIO_G4_INT_STAT; reg++) {
		ret = smsc_write_data(reg, SMSC_SET_HIGH);
		if (ret < 0)
			goto exit;
	}

	/* Configure the Interface Selection */
	ret = mask = smsc_read_data(SMSC_CLK_CTRL);
	if (ret < 0)
		goto exit;
	mask |= SMSC_CLOCK_INT_SMSBUS;
	ret  = smsc_write_data(SMSC_CLK_CTRL, mask);
	if (ret < 0)
		goto exit;

	/* Set the SMBus Alert Response Address */
	ret = mask = smsc_read_data(SMSC_CLK_CTRL);
	if (ret < 0)
		goto exit;
	mask |= SMSC_CLOCK_ARA;
	ret  = smsc_write_data(SMSC_CLK_CTRL, mask);
	if (ret < 0)
		goto exit;

	/* Configure GPIO Group 2 as the keypad matrix rows (KSI) */
	mask = (SMSC_GPIO_PU | SMSC_GPIO_IRQ_FALLING | SMSC_GPIO_ALT_FN);
	mask &= ~SMSC_GPIO_DIR;
	for (gpio = 0; gpio <= GPIO_BRANK_SIZE; gpio++) {
		ret = smsc_write_data((SMSC_GPIO_G2_BASEADDR + gpio), mask);
		if (ret < 0)
			goto exit;
	}

	/* Configure GPIO Group 1,3 and 4 as the keypad matrix columns (KSO) */
	mask = (SMSC_GPIO_ALT_FN | SMSC_GPIO_DIR | SMSC_GPIO_OUT_CONF);
	for (gpio = 0; gpio <= GPIO_BRANK_SIZE; gpio++) {
		ret = smsc_write_data((SMSC_GPIO_G1_BASEADDR + gpio), mask);
		if (ret < 0)
			goto exit;
		ret = smsc_write_data((SMSC_GPIO_G3_BASEADDR + gpio), mask);
		if (ret < 0)
			goto exit;
		ret = smsc_write_data((SMSC_GPIO_G4_BASEADDR + gpio), mask);
		if (ret < 0)
			goto exit;
	}

	/* Set up Low Power Mode (Wake-up) */
	ret = smsc_write_data(SMSC_WKUP_CTRL, SMSC_SET_LOW_PWR);
	if (ret < 0)
		goto exit;

	/* Enable smsc keypad interrupts */
	ret = smsc_write_data(SMSC_KSI_MASK, SMSC_SET_HIGH);
	if (ret < 0)
		goto exit;

	/* Enable Keypad Scan (generate interrupt on key press) */
	ret = smsc_write_data(SMSC_KSO_SELECT, SMSC_KSO_ALL_LOW);
	if (ret < 0)
		goto exit;

	return 0;

exit:
	dev_err(&kp->client->dev, "fail to configure the keyboard\n");
	return ret;
}

static int __devinit
smsc_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct smsc_keypad_data *pdata = client->dev.platform_data;
	const struct matrix_keymap_data *keymap_data;
	struct input_dev *input;
	struct smsc_keypad *kp;
	int ret = 0;
	int col;

	if (!pdata || !pdata->rows || !pdata->cols || !pdata->keymap_data) {
		dev_err(&client->dev, "Invalid platform_data\n");
		return -EINVAL;
	}

	keymap_data = pdata->keymap_data;
	kp = kzalloc(sizeof(*kp), GFP_KERNEL);
	if (!kp)
		return -ENOMEM;

	input = input_allocate_device();
	if (!input)
		goto err1;

	kp->gpio = client->irq;
	/* Get the debug Device */
	kp->dbg_dev = &client->dev;
	kp->input = input;
	kp->rows = pdata->rows;
	kp->cols = pdata->cols;

	for (col = 0; col < kp->cols; col++) {
		kp->last_key_state[col] = 0;
		kp->last_key_ms[col] = 0;
	}

	/* setup input device */
	 __set_bit(EV_KEY, input->evbit);

	if (i2c_check_functionality(client->adapter, I2C_FUNC_I2C) == 0) {
		dev_dbg(&client->dev, "can't talk I2C?\n");
		ret = -EIO;
		goto err2;
	}

	kp_client = client;

	/* Print SMSC revision info */
	ret = smsc_read_data(SMSC_DEVICE_ID);
	if (ret < 0)
		goto err2;

	input->id.product = ret;

	ret = smsc_read_data(SMSC_DEV_VERSION);
	input->id.version = ret;

	ret = smsc_read_data(SMSC_VENDOR_ID_LSB);
	input->id.vendor = 0x0;
	input->id.vendor |= ret;

	ret = smsc_read_data(SMSC_VENDOR_ID_MSB);
	input->id.vendor |= (ret << 8);

	dev_info(&client->dev, "SMSC: Device ID: %d, Version No.: %d, Vendor ID: 0x%x\n",
		input->id.product, input->id.version, input->id.vendor);

	/* Enable auto repeat feature of Linux input subsystem */
	if (pdata->rep)
		__set_bit(EV_REP, input->evbit);

	input_set_capability(input, EV_MSC, MSC_SCAN);
	input->name             = "smsc_keypad";
	input->phys             = "smsc_keypad/input0";
	input->dev.parent       = &client->dev;
	input->id.bustype       = BUS_HOST;
	input->keycode          = kp->keymap;
	input->keycodesize      = sizeof(kp->keymap[0]);
	input->keycodemax       = ARRAY_SIZE(kp->keymap);

	ret = input_register_device(input);
	if (ret) {
		dev_err(kp->dbg_dev,
			"Unable to register keypad device\n");
		goto err2;
	}

	ret = smsc_kp_initialize(kp);
	if (ret)
		goto err3;

	matrix_keypad_build_keymap(pdata->keymap_data, ROW_SHIFT,
			input->keycode, input->keybit);

	ret = gpio_request_one(kp->gpio, GPIOF_IN, "smsc_keypad");
	if (ret) {
		dev_err(&client->dev, "keypad: gpio request failure\n");
		goto err3;
	}

	/*
	* This ISR will always execute in kernel thread context because of
	* the need to access the SMSC over the I2C bus.
	*/
	kp->irq = gpio_to_irq(kp->gpio);

	ret = request_threaded_irq(kp->irq, NULL, do_kp_irq,
		IRQF_DISABLED | IRQF_TRIGGER_LOW | IRQF_ONESHOT,
							client->name, kp);
	if (ret) {
		dev_info(&client->dev, "request_irq failed for irq no=%d\n",
			client->irq);
		goto err4;
	}

	i2c_set_clientdata(client, kp);

	return 0;

err4:
	free_irq(kp->irq, NULL);
	gpio_free(kp->gpio);
err3:
	input_unregister_device(input);
err2:
	input_free_device(input);
err1:
	kfree(kp);

	return ret;
}

static int smsc_remove(struct i2c_client *client)
{
	struct smsc_keypad *kp = i2c_get_clientdata(client);

	smsc_write_data(SMSC_CLK_CTRL, SMSC_SET_LOW);
	free_irq(kp->irq, NULL);
	gpio_free(kp->gpio);
	input_unregister_device(kp->input);
	input_free_device(kp->input);
	kfree(kp);

	return 0;
}

static const struct i2c_device_id smsc_id[] = {
	{ "smsc", 0 },
	{ }
};

static struct i2c_driver smsc_driver = {
	.driver = {
		.name	= "smsc-keypad",
		.owner  = THIS_MODULE,
	},
	.probe		= smsc_probe,
	.remove		= smsc_remove,
	.id_table	= smsc_id,
};

static int __init smsc_init(void)
{
	return i2c_add_driver(&smsc_driver);
}

static void __exit smsc_exit(void)
{
	i2c_del_driver(&smsc_driver);
}

MODULE_AUTHOR("G Kondaiah Manjunath <manjugk@ti.com>");
MODULE_DESCRIPTION("SMSC ECE1099 Keypad driver");
MODULE_LICENSE("GPL");

module_init(smsc_init);
module_exit(smsc_exit);
