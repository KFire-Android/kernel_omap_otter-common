/*
 * OMAP4 Keypad Driver
 *
 * Copyright (C) 2010 Texas Instruments
 *
 * Author: Abraham Arce <x0066660@ti.com>
 * Initial Code: Syed Rafiuddin <rafiuddin.syed@ti.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/errno.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/input.h>
#include <linux/slab.h>
#include <linux/pm_runtime.h>

#include <linux/platform_data/omap4-keypad.h>

/* OMAP4 registers */
#define OMAP4_KBD_REVISION		0x00
#define OMAP4_KBD_SYSCONFIG		0x10
#define OMAP4_KBD_SYSSTATUS		0x14
#define OMAP4_KBD_IRQSTATUS		0x18
#define OMAP4_KBD_IRQENABLE		0x1C
#define OMAP4_KBD_WAKEUPENABLE		0x20
#define OMAP4_KBD_PENDING		0x24
#define OMAP4_KBD_CTRL			0x28
#define OMAP4_KBD_DEBOUNCINGTIME	0x2C
#define OMAP4_KBD_LONGKEYTIME		0x30
#define OMAP4_KBD_TIMEOUT		0x34
#define OMAP4_KBD_STATEMACHINE		0x38
#define OMAP4_KBD_ROWINPUTS		0x3C
#define OMAP4_KBD_COLUMNOUTPUTS		0x40
#define OMAP4_KBD_FULLCODE31_0		0x44
#define OMAP4_KBD_FULLCODE63_32		0x48

/* OMAP4 bit definitions */
#define OMAP4_DEF_IRQENABLE_EVENTEN	(1 << 0)
#define OMAP4_DEF_IRQENABLE_LONGKEY	(1 << 1)
#define OMAP4_DEF_IRQENABLE_TIMEOUTEN	(1 << 2)
#define OMAP4_DEF_WUP_EVENT_ENA		(1 << 0)
#define OMAP4_DEF_WUP_LONG_KEY_ENA	(1 << 1)
#define OMAP4_DEF_WUP_TIMEOUTEN_ENA	(1 << 2)
#define OMAP4_DEF_CTRL_NOSOFTMODE	(1 << 1)
#define OMAP4_DEF_CTRL_PTV		(1 << 2)
#define OMAP4_DEF_REPEAT_MODE		(1 << 8)
#define OMAP4_DEF_TIMEOUT_LONG_KEY	(1 << 7)
#define OMAP4_DEF_TIMEOUT_EMPTY		(1 << 6)
#define OMAP4_DEF_LONG_KEY		(1 << 5)

/* OMAP4 values */
#define OMAP4_VAL_IRQDISABLE		0x00
/* DEBOUNCE TIME VALUE = 0x2 PVT = 0x6  Tperiod = 12ms*/
#define OMAP4_VAL_DEBOUNCINGTIME	0x2
#define OMAP4_VAL_PVT			0x6

#define OMAP4_MASK_IRQSTATUSDISABLE	0xFFFF

enum {
	KBD_REVISION_OMAP4 = 0,
	KBD_REVISION_OMAP5,
};

struct omap4_keypad {
	struct input_dev *input;
	struct matrix_keymap_data *keymap_data;

	void __iomem *base;
	int irq;

	unsigned int rows;
	unsigned int cols;
	unsigned int revision;
	u32 irqstatus;
	u32 irqenable;
	u32 reg_offset;
	unsigned int row_shift;
	bool no_autorepeat;
	unsigned char key_state[8];
	unsigned short keymap[];
};

static int kbd_read_irqstatus(struct omap4_keypad *keypad_data, u32 offset)
{
	return __raw_readl(keypad_data->base + offset);
}

static int kbd_write_irqstatus(struct omap4_keypad *keypad_data,
						u32 value)
{
	return __raw_writel(value, keypad_data->base + keypad_data->irqstatus);
}

static int kbd_write_irqenable(struct omap4_keypad *keypad_data,
						u32 value)
{
	return __raw_writel(value, keypad_data->base + keypad_data->irqenable);
}

static int kbd_readl(struct omap4_keypad *keypad_data, u32 offset)
{
	return __raw_readl(keypad_data->base +
			keypad_data->reg_offset + offset);
}

static void kbd_writel(struct omap4_keypad *keypad_data, u32 offset, u32 value)
{
	__raw_writel(value, keypad_data->base +
			keypad_data->reg_offset + offset);
}

static int kbd_read_revision(struct omap4_keypad *keypad_data, u32 offset)
{
	int reg;
	reg = __raw_readl(keypad_data->base + offset);
	reg &= 0x03 << 30;
	reg >>= 30;

	switch (reg) {
	case 1:
		return KBD_REVISION_OMAP5;
	case 0:
		return KBD_REVISION_OMAP4;
	default:
		return -EINVAL;
	}
}

/* Interrupt handler */
static irqreturn_t omap4_keypad_interrupt(int irq, void *dev_id)
{
	struct omap4_keypad *keypad_data = dev_id;
	struct input_dev *input_dev = keypad_data->input;
	unsigned char key_state[ARRAY_SIZE(keypad_data->key_state)];
	unsigned int col, row, code, changed;
	u32 *new_state = (u32 *) key_state;


	*new_state = kbd_readl(keypad_data, OMAP4_KBD_FULLCODE31_0);
	*(new_state + 1) = kbd_readl(keypad_data, OMAP4_KBD_FULLCODE63_32);

	for (row = 0; row < keypad_data->rows; row++) {
		changed = key_state[row] ^ keypad_data->key_state[row];
		if (!changed)
			continue;

		for (col = 0; col < keypad_data->cols; col++) {
			if (changed & (1 << col)) {
				code = MATRIX_SCAN_CODE(row, col,
						keypad_data->row_shift);
				input_event(input_dev, EV_MSC, MSC_SCAN, code);
				input_report_key(input_dev,
						 keypad_data->keymap[code],
						 key_state[row] & (1 << col));
			}
		}
	}

	input_sync(input_dev);

	memcpy(keypad_data->key_state, key_state,
		sizeof(keypad_data->key_state));

	/* clear pending interrupts */
	kbd_write_irqstatus(keypad_data,
		kbd_read_irqstatus(keypad_data, keypad_data->irqstatus));

	return IRQ_HANDLED;
}

static int omap4_keypad_open(struct input_dev *input)
{
	struct omap4_keypad *keypad_data = input_get_drvdata(input);

	pm_runtime_get_sync(input->dev.parent);

	disable_irq(keypad_data->irq);

	keypad_data->revision = kbd_read_revision(keypad_data,
			OMAP4_KBD_REVISION);
	switch (keypad_data->revision) {
	case 1:
		keypad_data->irqstatus = OMAP4_KBD_IRQSTATUS + 0x0c;
		keypad_data->irqenable = OMAP4_KBD_IRQENABLE + 0x0c;
		keypad_data->reg_offset = 0x10;
		break;
	case 0:
		keypad_data->irqstatus = OMAP4_KBD_IRQSTATUS;
		keypad_data->irqenable = OMAP4_KBD_IRQENABLE;
		keypad_data->reg_offset = 0x00;
		break;
	default:
		pr_err("Omap keypad not found\n");
		return -ENODEV;
	}

	kbd_writel(keypad_data, OMAP4_KBD_CTRL,
			OMAP4_DEF_CTRL_NOSOFTMODE
			| (OMAP4_VAL_PVT << OMAP4_DEF_CTRL_PTV));

	kbd_writel(keypad_data, OMAP4_KBD_DEBOUNCINGTIME,
			OMAP4_VAL_DEBOUNCINGTIME);

	/* Enable event IRQ */
	kbd_write_irqenable(keypad_data, OMAP4_DEF_IRQENABLE_EVENTEN);

	/* Enable event wkup */
	kbd_writel(keypad_data, OMAP4_KBD_WAKEUPENABLE,
			OMAP4_DEF_WUP_EVENT_ENA);

	/* clear pending interrupts */
	kbd_write_irqstatus(keypad_data,
		kbd_read_irqstatus(keypad_data, keypad_data->irqstatus));

	enable_irq(keypad_data->irq);

	return 0;
}

static void omap4_keypad_close(struct input_dev *input)
{
	struct omap4_keypad *keypad_data = input_get_drvdata(input);

	disable_irq(keypad_data->irq);

	/* Disable interrupts */
	kbd_write_irqenable(keypad_data, OMAP4_VAL_IRQDISABLE);

	/* clear pending interrupts */
	kbd_write_irqstatus(keypad_data,
		kbd_read_irqstatus(keypad_data, keypad_data->irqstatus));

	enable_irq(keypad_data->irq);

	pm_runtime_put_sync(input->dev.parent);
}

static struct omap4_keypad *omap_keypad_parse_dt(struct device *dev)
{
	struct matrix_keymap_data *keymap_data;
	struct device_node *np = dev->of_node;
	struct platform_device *pdev = to_platform_device(dev);
	struct omap4_keypad *keypad_data = platform_get_drvdata(pdev);
	uint32_t *keymap;
	int proplen = 0, i;
	const __be32 *prop = of_get_property(np, "linux,keymap", &proplen);

	keymap_data = devm_kzalloc(dev, sizeof(*keymap_data), GFP_KERNEL);
	if (!keymap_data) {
		dev_err(dev, "could not allocate memory for keymap data\n");
		return NULL;
	}
	keypad_data->keymap_data = keymap_data;

	keymap_data->keymap_size = proplen / sizeof(u32);

	keymap = devm_kzalloc(dev,
		sizeof(uint32_t) * keymap_data->keymap_size, GFP_KERNEL);
	if (!keymap) {
		dev_err(dev, "could not allocate memory for keymap\n");
		return NULL;
	}
	keypad_data->keymap_data->keymap = keymap;

	for (i = 0; i < keymap_data->keymap_size; i++) {
		u32 tmp = be32_to_cpup(prop+i);
		int key_code, row, col;

		row = (tmp >> 24) & 0xff;
		col = (tmp >> 16) & 0xff;
		key_code = tmp & 0xffff;
		*keymap++ = KEY(row, col, key_code);
	}

	if (of_get_property(np, "linux,input-no-autorepeat", NULL))
		keypad_data->no_autorepeat = true;

	return keypad_data;
}

static int __devinit omap4_keypad_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;
	const struct omap4_keypad_platform_data *pdata;
	struct omap4_keypad *keypad_data;
	struct input_dev *input_dev;
	struct resource *res;
	resource_size_t size;
	unsigned int row_shift = 0, max_keys = 0;
	uint32_t num_rows = 0, num_cols = 0, no_autorepeat = 0;
	int irq;
	int error;

	/* platform data */
	pdata = pdev->dev.platform_data;
	if (np) {
		of_property_read_u32(np, "keypad,num-rows", &num_rows);
		of_property_read_u32(np, "keypad,num-columns", &num_cols);
		if (!num_rows || !num_cols) {
			dev_err(&pdev->dev, "number of keypad rows/columns not specified\n");
			return -EINVAL;
		}
		of_property_read_u32(np, "linux,input-no-autorepeat",
			&no_autorepeat);
	} else if (pdata) {
		num_rows = pdata->rows;
		num_cols = pdata->cols;
		no_autorepeat = pdata->no_autorepeat;
	} else {
		dev_err(&pdev->dev, "no platform data defined\n");
		return -EINVAL;
	}

	row_shift = get_count_order(num_cols);
	max_keys = num_rows << row_shift;

	keypad_data = devm_kzalloc(dev, sizeof(struct omap4_keypad) +
			max_keys * sizeof(keypad_data->keymap[0]),
				GFP_KERNEL);

	if (!keypad_data) {
		dev_err(&pdev->dev, "keypad_data memory allocation failed\n");
		return -ENOMEM;
	}

	platform_set_drvdata(pdev, keypad_data);

	if (np) {
		keypad_data = omap_keypad_parse_dt(&pdev->dev);
	} else {
		keypad_data->keymap_data =
				(struct matrix_keymap_data *)pdata->keymap_data;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(&pdev->dev, "no base address specified\n");
		return -EINVAL;
	}

	irq = platform_get_irq(pdev, 0);
	if (!irq) {
		dev_err(&pdev->dev, "no keyboard irq assigned\n");
		return -EINVAL;
	}

	size = resource_size(res);

	res = request_mem_region(res->start, size, pdev->name);
	if (!res) {
		dev_err(&pdev->dev, "can't request mem region\n");
		error = -EBUSY;
		goto err_free_keypad;
	}

	keypad_data->base = ioremap(res->start, resource_size(res));
	if (!keypad_data->base) {
		dev_err(&pdev->dev, "can't ioremap mem resource\n");
		error = -ENOMEM;
		goto err_release_mem;
	}

	keypad_data->rows = num_rows;
	keypad_data->cols = num_cols;
	keypad_data->no_autorepeat = no_autorepeat;
	keypad_data->irq = irq;
	keypad_data->row_shift = row_shift;

	/* input device allocation */
	keypad_data->input = input_dev = input_allocate_device();
	if (!input_dev) {
		error = -ENOMEM;
		goto err_unmap;
	}

	input_dev->name = pdev->name;
	input_dev->dev.parent = &pdev->dev;
	input_dev->id.bustype = BUS_HOST;
	input_dev->id.vendor = 0x0001;
	input_dev->id.product = 0x0001;
	input_dev->id.version = 0x0001;

	input_dev->open = omap4_keypad_open;
	input_dev->close = omap4_keypad_close;

	input_dev->keycode	= keypad_data->keymap;
	input_dev->keycodesize	= sizeof(keypad_data->keymap[0]);
	input_dev->keycodemax	= max_keys;

	__set_bit(EV_KEY, input_dev->evbit);

	if (!keypad_data->no_autorepeat)
		__set_bit(EV_REP, input_dev->evbit);

	input_set_capability(input_dev, EV_MSC, MSC_SCAN);

	input_set_drvdata(input_dev, keypad_data);

	matrix_keypad_build_keymap(keypad_data->keymap_data, row_shift,
			input_dev->keycode, input_dev->keybit);

	/*
	 * Set irq level detection for mpu. Edge event are missed
	 * in gic if the mpu is in low power and keypad event
	 * is a wakeup
	 */
	error = request_irq(keypad_data->irq, omap4_keypad_interrupt,
			     IRQF_TRIGGER_HIGH,
			     "omap4-keypad", keypad_data);
	if (error) {
		dev_err(&pdev->dev, "failed to register interrupt\n");
		goto err_free_input;
	}
	enable_irq_wake(OMAP44XX_IRQ_KBD_CTL);

	pm_runtime_enable(&pdev->dev);

	error = input_register_device(keypad_data->input);
	if (error < 0) {
		dev_err(&pdev->dev, "failed to register input device\n");
		goto err_pm_disable;
	}

	return 0;

err_pm_disable:
	pm_runtime_disable(&pdev->dev);
	free_irq(keypad_data->irq, keypad_data);
err_free_input:
	input_free_device(input_dev);
err_unmap:
	iounmap(keypad_data->base);
err_release_mem:
	release_mem_region(res->start, size);
err_free_keypad:
	kfree(keypad_data);
	return error;
}

static int __devexit omap4_keypad_remove(struct platform_device *pdev)
{
	struct omap4_keypad *keypad_data = platform_get_drvdata(pdev);
	struct resource *res;

	free_irq(keypad_data->irq, keypad_data);

	pm_runtime_disable(&pdev->dev);

	input_unregister_device(keypad_data->input);

	iounmap(keypad_data->base);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	release_mem_region(res->start, resource_size(res));

	kfree(keypad_data);
	platform_set_drvdata(pdev, NULL);

	return 0;
}

static const struct of_device_id omap_keypad_dt_match[] = {
	{ .compatible = "ti,omap4-keypad" },
	{},
};
MODULE_DEVICE_TABLE(of, omap_keypad_dt_match);

#ifdef CONFIG_PM
static int omap4_keypad_suspend(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct omap4_keypad *keypad_data = platform_get_drvdata(pdev);

	omap4_keypad_close(keypad_data->input);
	return 0;
}

static int omap4_keypad_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct omap4_keypad *keypad_data = platform_get_drvdata(pdev);

	omap4_keypad_open(keypad_data->input);
	return 0;
}

static const struct dev_pm_ops omap4_keypad_pm_ops = {
	.suspend = omap4_keypad_suspend,
	.resume = omap4_keypad_resume,
};
#define OMAP4_KEYPAD_PM_OPS (&omap4_keypad_pm_ops)
#else
#define OMAP4_KEYPAD_PM_OPS NULL
#endif

static struct platform_driver omap4_keypad_driver = {
	.probe		= omap4_keypad_probe,
	.remove		= __devexit_p(omap4_keypad_remove),
	.driver		= {
		.name	= "omap4-keypad",
		.owner	= THIS_MODULE,
		.pm	= OMAP4_KEYPAD_PM_OPS,
		.of_match_table = of_match_ptr(omap_keypad_dt_match),
	},
};
module_platform_driver(omap4_keypad_driver);

MODULE_AUTHOR("Texas Instruments");
MODULE_DESCRIPTION("OMAP4 Keypad Driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:omap4-keypad");
