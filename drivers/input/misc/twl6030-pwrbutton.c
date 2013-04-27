/**
 * twl6030-pwrbutton.c - TWL6030 Power Button Input Driver
 *
 * Copyright (C) 2011
 *
 * Written by Dan Murphy <DMurphy@ti.com>
 *
 * This file is subject to the terms and conditions of the GNU General
 * Public License. See the file "COPYING" in the main directory of this
 * archive for more details.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Derived Work from: twl4030-pwrbutton.c from
 * Peter De Schrijver <peter.de-schrijver@nokia.com>
 * Felipe Balbi <felipe.balbi@nokia.com>
 *
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/i2c/twl.h>
#include <linux/delay.h>

#define PWR_PWRON_IRQ (1 << 0)
#define STS_HW_CONDITIONS 0x21

static int was_suspend;

struct twl6030_pwr_button {
	struct input_dev *input_dev;
	struct device *dev;
	int report_key;
};

static irqreturn_t powerbutton_irq(int irq, void *_pwr)
{
	struct twl6030_pwr_button *pwr = _pwr;
	char hw_state;
	int pwr_val;
	int ret = 0;
	static int prev_pwr_val = 0xFFFF;
	static int push_release_flag;

	ret = twl_i2c_read_u8(TWL6030_MODULE_ID0, &hw_state, STS_HW_CONDITIONS);
	if (IS_ERR_VALUE(ret))
		dev_dbg(pwr->dev, "TWL6030:readb[0x%x,0x%x] Error %d\n",
			TWL6030_MODULE_ID0, STS_HW_CONDITIONS, ret);

	pwr_val = !(hw_state & PWR_PWRON_IRQ);
	if ((prev_pwr_val != pwr_val)) {
		/*
		 * we can loose press key after suspend, but it
		 * always press, so rewrite on press. The up will
		 * be generated from twl shadow register
		 */
		if (was_suspend) {
			was_suspend = 0;
			pwr_val = 1;
		}
		push_release_flag = 0;
		input_report_key(pwr->input_dev, pwr->report_key, pwr_val);
		input_sync(pwr->input_dev);
	} else if (!push_release_flag) {
		/*
		 * We got the same value twice.
		 * It means we have lost one interrupt,
		 * so additional key event have to be emulated.
		 */
		push_release_flag = 1;
		input_report_key(pwr->input_dev, pwr->report_key, !pwr_val);
		input_sync(pwr->input_dev);

		msleep(20);

		input_report_key(pwr->input_dev, pwr->report_key, pwr_val);
		input_sync(pwr->input_dev);
	} else {
		push_release_flag = 0;
	}

	prev_pwr_val = pwr_val;
	return IRQ_HANDLED;
}

static int __devinit twl6030_pwrbutton_probe(struct platform_device *pdev)
{
	struct twl6030_pwr_button *pwr_button;
	int irq = platform_get_irq(pdev, 0);
	int err = -ENODEV;

	pr_info("%s: Enter\n", __func__);
	pwr_button = kzalloc(sizeof(struct twl6030_pwr_button), GFP_KERNEL);
	if (!pwr_button)
		return -ENOMEM;

	pwr_button->input_dev = input_allocate_device();
	if (!pwr_button->input_dev) {
		dev_dbg(&pdev->dev, "Can't allocate power button\n");
		goto input_error;
	}

	__set_bit(EV_KEY, pwr_button->input_dev->evbit);

	pwr_button->report_key = KEY_END;
	pwr_button->dev = &pdev->dev;
	pwr_button->input_dev->evbit[0] = BIT_MASK(EV_KEY);
	pwr_button->input_dev->keybit[BIT_WORD(pwr_button->report_key)] =
			BIT_MASK(pwr_button->report_key);
	pwr_button->input_dev->name = "twl6030_pwrbutton";
	pwr_button->input_dev->phys = "twl6030_pwrbutton/input0";
	pwr_button->input_dev->dev.parent = &pdev->dev;

	err = request_threaded_irq(irq, NULL, powerbutton_irq,
				   IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING,
				   "twl6030_pwrbutton", pwr_button);
	if (IS_ERR_VALUE(err)) {
		dev_dbg(&pdev->dev, "Can't get IRQ for pwrbutton: %d\n", err);
		goto free_input_dev;
	}

	err = input_register_device(pwr_button->input_dev);
	if (IS_ERR_VALUE(err)) {
		dev_dbg(&pdev->dev, "Can't register power button: %d\n", err);
		goto free_irq;
	}

	twl6030_interrupt_unmask(0x01, REG_INT_MSK_LINE_A);
	twl6030_interrupt_unmask(0x01, REG_INT_MSK_STS_A);

	platform_set_drvdata(pdev, pwr_button);
	return 0;

free_irq:
	free_irq(irq, NULL);
free_input_dev:
	input_free_device(pwr_button->input_dev);
input_error:
	kfree(pwr_button);
	return err;
}

static int __devexit twl6030_pwrbutton_remove(struct platform_device *pdev)
{
	struct input_dev *pwr = platform_get_drvdata(pdev);
	int irq = platform_get_irq(pdev, 0);

	free_irq(irq, pwr);
	input_unregister_device(pwr);

	return 0;
}

/* need to fix long press key after suspend */
static int pwrbutton_suspend(struct device *dev)
{
	was_suspend = 1;
	return 0;
}

static const struct dev_pm_ops pwrbutton_pm_ops = {
	.suspend	 = pwrbutton_suspend,
};

struct platform_driver twl6030_pwrbutton_driver = {
	.probe		= twl6030_pwrbutton_probe,
	.remove		= __devexit_p(twl6030_pwrbutton_remove),
	.driver		= {
		.name	= "twl6030_pwrbutton",
		.owner	= THIS_MODULE,
		.pm	= &pwrbutton_pm_ops,
	},
};

static int __init twl6030_pwrbutton_init(void)
{
	return platform_driver_register(&twl6030_pwrbutton_driver);
}
module_init(twl6030_pwrbutton_init);

static void __exit twl6030_pwrbutton_exit(void)
{
	platform_driver_unregister(&twl6030_pwrbutton_driver);
}
module_exit(twl6030_pwrbutton_exit);

MODULE_ALIAS("platform:twl6030_pwrbutton");
MODULE_DESCRIPTION("Triton2 Power Button");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Dan Murphy <DMurphy@ti.com>");
