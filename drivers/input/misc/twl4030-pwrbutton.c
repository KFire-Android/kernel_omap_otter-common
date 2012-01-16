/**
 * twl4030-pwrbutton.c - TWL4030 Power Button Input Driver
 *
 * Copyright (C) 2008-2009 Nokia Corporation
 *
 * Written by Peter De Schrijver <peter.de-schrijver@nokia.com>
 * Several fixes by Felipe Balbi <felipe.balbi@nokia.com>
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
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/i2c/twl.h>
#include <linux/metricslog.h>
#include <linux/jiffies.h>
#include <linux/leds.h>
#include <linux/wakelock.h>
#include <linux/workqueue.h>
#include <plat/led.h>
#define PWR_PWRON_IRQ (1 << 0)


struct input_dev *g_pwr ;

#define STATE_UNKNOWN   -1
#define STATE_RELEASE    0
#define STATE_PRESS      1

static int prev_press = STATE_UNKNOWN;

static struct delayed_work pwrbutton_work;
static struct wake_lock pwrbutton_wakelock;
static int g_in_suspend;
static irqreturn_t powerbutton_irq(int irq, void *_pwr)
{
	struct input_dev *pwr = _pwr;
	int err;
	u8 value;

	err = twl_i2c_read_u8(TWL4030_MODULE_PM_MASTER, &value,REG_STS_HW_CONDITIONS);
	if (!err)  {
		char buf[128];

		int press = (value & PWR_PWRON_IRQ) ? STATE_RELEASE : STATE_PRESS;

		char *action = press ? "press" : "release";

		input_report_key(pwr, KEY_POWER, press);
		input_sync(pwr);

		sprintf(buf, "%s:powi%c:action=%s:", __func__, action[0], action);

		log_to_metrics(ANDROID_LOG_INFO, "PowerKeyEvent", buf);

		strcat(buf, "\n");
		printk(buf);

		if(press != prev_press) {
			if(press) {
				prev_press = STATE_PRESS;

			} else {
				prev_press = STATE_RELEASE;

				/* Check if we are connected to USB */
				if (!twl_i2c_read_u8(TWL6030_MODULE_CHARGER,
						&value, 0x03) && !(value & (1 << 2))) {

					/*
					 * No USB, hold a partial wakelock,
					 * scheduled a work 2 seconds later
					 * to switch off the LED
					 */
					wake_lock(&pwrbutton_wakelock);
					cancel_delayed_work_sync(&pwrbutton_work);
					omap4430_orange_led_set(NULL, 0);
					omap4430_green_led_set(NULL, 255);

					schedule_delayed_work(&pwrbutton_work,
						msecs_to_jiffies(2000));
				}
			}
		}else{
			if(g_in_suspend==1){
					wake_lock(&pwrbutton_wakelock);
					cancel_delayed_work_sync(&pwrbutton_work);
					input_report_key(pwr, KEY_POWER, 1);
					input_report_key(pwr, KEY_POWER, 0);
					input_sync(pwr);
					omap4430_orange_led_set(NULL, 0);
					omap4430_green_led_set(NULL, 255);
					schedule_delayed_work(&pwrbutton_work,
						msecs_to_jiffies(2000));
			}
		}
	} else {
		dev_err(pwr->dev.parent, "twl4030: i2c error %d while reading"
			" TWL4030 PM_MASTER STS_HW_CONDITIONS register\n", err);

                /* If an error occurs we don't want to display the SysReq the next time user will
                 * push and release power button.
                 */
                prev_press = STATE_UNKNOWN;
	}

	return IRQ_HANDLED;
}
void twl_system_poweroff(void)
{
    printk("          system_poweroff\n ");
    //twl_i2c_write_u8(0x0d, 0x40, 0x25);//reboot
    twl_i2c_write_u8(0x0d, 0x07, 0x25);//shutdown
}

static void pwrbutton_work_func(struct work_struct *work)
{
	u8 value = 0;

	/* Switch off LED if it's not connected to USB */
	if (!twl_i2c_read_u8(TWL6030_MODULE_CHARGER, &value, 0x03)
			&& !(value & (1 << 2))) {
		omap4430_orange_led_set(NULL, 0);
		omap4430_green_led_set(NULL, 0);
	}

	wake_unlock(&pwrbutton_wakelock);
}
static int twl4030_pwrbutton_suspend(struct platform_device *pdev)
{
	int err;
	u8 value;
	int press;
	struct input_dev *pwr = platform_get_drvdata(pdev);
	g_in_suspend=1;
    	return 0;
}
static int twl4030_pwrbutton_resume(struct platform_device *pdev)
{
	int err;
	u8 value;
	int press;
	struct input_dev *pwr = platform_get_drvdata(pdev);
	g_in_suspend=0;
	err = twl_i2c_read_u8(TWL4030_MODULE_PM_MASTER, &value,REG_STS_HW_CONDITIONS);
	press = (value & PWR_PWRON_IRQ) ? STATE_RELEASE : STATE_PRESS;
	if(press) {
		prev_press = STATE_PRESS;
		input_report_key(pwr, KEY_POWER, 1);
		input_report_key(pwr, KEY_POWER, 0);
		input_sync(pwr);
		//Turn green LED on immediately at full intensity and keep on for 2 seconds and then turn off
		if (!twl_i2c_read_u8(TWL6030_MODULE_CHARGER,
						&value, 0x03) && !(value & (1 << 2))) {
					/* If it's not connected to USB, blink */
					omap4430_orange_led_set(NULL, 0);
					omap4430_green_led_set(NULL, 255);
		}
	}
    	return 0;
}
static int __devinit twl4030_pwrbutton_probe(struct platform_device *pdev)
{
	struct input_dev *pwr;
	int irq = platform_get_irq(pdev, 0);
	int err;
	pwr = input_allocate_device();
	if (!pwr) {
		dev_dbg(&pdev->dev, "Can't allocate power button\n");
		return -ENOMEM;
	}
	g_in_suspend=0;
	pwr->evbit[0] = BIT_MASK(EV_KEY);
	pwr->keybit[BIT_WORD(KEY_POWER)] = BIT_MASK(KEY_POWER);
	pwr->name = "twl4030_pwrbutton";
	pwr->phys = "twl4030_pwrbutton/input0";
	pwr->dev.parent = &pdev->dev;
	twl6030_interrupt_unmask(0x03,REG_INT_MSK_LINE_A);
	twl6030_interrupt_unmask(0x03,REG_INT_MSK_STS_A);
	err = request_threaded_irq(irq, NULL, powerbutton_irq, IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING,
				"twl4030_pwrbutton", pwr);
	if (err < 0) {
		dev_dbg(&pdev->dev, "Can't get IRQ for pwrbutton: %d\n", err);
		goto free_input_dev;
	}
        g_pwr=pwr;
	err = input_register_device(pwr);
	if (err) {
		dev_dbg(&pdev->dev, "Can't register power button: %d\n", err);
		goto free_irq;
	}
	pm_power_off = twl_system_poweroff;
	platform_set_drvdata(pdev, pwr);

	/* Set up wakelock */
	wake_lock_init(&pwrbutton_wakelock, WAKE_LOCK_SUSPEND, "twl4030-pwrbutton");

	/* Set up delayed work */
	INIT_DELAYED_WORK(&pwrbutton_work, pwrbutton_work_func);

	return 0;

free_irq:
	free_irq(irq, NULL);
free_input_dev:
	input_free_device(pwr);
	return err;
}

static int __devexit twl4030_pwrbutton_remove(struct platform_device *pdev)
{
	struct input_dev *pwr = platform_get_drvdata(pdev);
	int irq = platform_get_irq(pdev, 0);

	free_irq(irq, pwr);
	input_unregister_device(pwr);

	return 0;
}

struct platform_driver twl4030_pwrbutton_driver = {
	.probe		= twl4030_pwrbutton_probe,
	.remove		= __devexit_p(twl4030_pwrbutton_remove),
	.suspend	=	twl4030_pwrbutton_suspend,
	.resume		=	twl4030_pwrbutton_resume,
	.driver		= {
		.name	= "twl4030_pwrbutton",
		.owner	= THIS_MODULE,
	},
};

static int __init twl4030_pwrbutton_init(void)
{
	return platform_driver_register(&twl4030_pwrbutton_driver);
}
module_init(twl4030_pwrbutton_init);

static void __exit twl4030_pwrbutton_exit(void)
{
	platform_driver_unregister(&twl4030_pwrbutton_driver);
}
module_exit(twl4030_pwrbutton_exit);

MODULE_ALIAS("platform:twl4030_pwrbutton");
MODULE_DESCRIPTION("Triton2 Power Button");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Peter De Schrijver <peter.de-schrijver@nokia.com>");
MODULE_AUTHOR("Felipe Balbi <felipe.balbi@nokia.com>");

