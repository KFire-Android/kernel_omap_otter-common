/**
 * Palmas Power Button Input Driver
 *
 * Copyright (C) 2012 Texas Instrument Inc
 *
 * Written by Girish S Ghongdemath <girishsg@ti.com>
 * Based on twl4030-pwrbutton.c driver.
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

#include <linux/delay.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/mfd/palmas.h>

#define PALMAS_PWR_KEY_PRESS	0x01
#define PALMAS_PWR_KEY_Q_TIME	20	/* Time in ms */

struct palmas_pwron {
	struct palmas *palmas;
	struct input_dev *input_dev;
	struct delayed_work input_work;
	int current_state;
};

static int palmas_get_pwr_state(struct palmas_pwron *pwron)
{
	unsigned int addr;
	unsigned int reg;
	int ret, slave;

	slave = PALMAS_BASE_TO_SLAVE(PALMAS_INTERRUPT_BASE);
	addr = PALMAS_BASE_TO_REG(PALMAS_INTERRUPT_BASE,
			PALMAS_INT1_LINE_STATE);

	ret = regmap_read(pwron->palmas->regmap[slave], addr, &reg);
	if (ret) {
		pr_err("%s:Cannot read palmas PWRON status.\n", __func__);
		return -1;
	}
	/* PWRON line state is bit two of the register.
	 * Shift down the bit and invert the data for the key to be
	 * recognized.
	 */
	return !(reg >> 1 & PALMAS_PWR_KEY_PRESS);
}

static void palmas_power_work(struct work_struct *work)
{
	struct palmas_pwron *pwron = container_of((struct delayed_work *)work,
					  struct palmas_pwron,
					  input_work);
	int next_state;

	next_state = palmas_get_pwr_state(pwron);
	if (next_state < 0)
		return;

	/* If the state did not change then schedule a work item to check the
	 * status of the power button line */
	if (next_state == pwron->current_state)
		schedule_delayed_work(&pwron->input_work,
			msecs_to_jiffies(PALMAS_PWR_KEY_Q_TIME));
	else {
		pwron->current_state = next_state;
		input_report_key(pwron->input_dev, KEY_POWER,
				pwron->current_state);
		input_sync(pwron->input_dev);
	}
}

static irqreturn_t pwron_irq(int irq, void *palmas_pwron)
{
	struct palmas_pwron *pwron = palmas_pwron;
	struct input_dev *input_dev = pwron->input_dev;

	cancel_delayed_work_sync(&pwron->input_work);

	pwron->current_state = PALMAS_PWR_KEY_PRESS;

	input_report_key(input_dev, KEY_POWER, pwron->current_state);
	input_sync(input_dev);

	schedule_delayed_work(&pwron->input_work, 0);

	return IRQ_HANDLED;
}

static int __devinit palmas_pwron_probe(struct platform_device *pdev)
{
	struct palmas *palmas = dev_get_drvdata(pdev->dev.parent);
	struct input_dev *input_dev;
	struct palmas_pwron *pwron;
	int irq = platform_get_irq_byname(pdev, "PWRON_BUTTON");
	unsigned int addr;
	int err, slave;


	pwron = kzalloc(sizeof(*pwron), GFP_KERNEL);
	if (!pwron) {
		dev_err(&pdev->dev, "PWRON memory allocation failed\n");
		return -ENOMEM;
	}

	input_dev = input_allocate_device();
	if (!input_dev) {
			dev_err(&pdev->dev,
				"input_dev memory allocation failed\n");
			err = -ENOMEM;
			goto free_pwron;
	}

	input_dev->evbit[0] = BIT_MASK(EV_KEY);
	input_dev->keybit[BIT_WORD(KEY_POWER)] = BIT_MASK(KEY_POWER);
	input_dev->name = "palmas_pwron";
	input_dev->phys = "palmas_pwron/input0";
	input_dev->dev.parent = &pdev->dev;

	pwron->palmas = palmas;
	pwron->input_dev = input_dev;

	slave = PALMAS_BASE_TO_SLAVE(PALMAS_PMU_CONTROL_BASE);
	addr = PALMAS_BASE_TO_REG(PALMAS_PMU_CONTROL_BASE,
				  PALMAS_LONG_PRESS_KEY);

	/* 6 Seconds as the LPK_TIME Long Press Key Time */
	regmap_update_bits(palmas->regmap[slave], addr,
			LONG_PRESS_KEY_LPK_TIME_MASK, 0);

	INIT_DELAYED_WORK(&pwron->input_work, palmas_power_work);

	err = request_threaded_irq(irq, NULL, pwron_irq,
			IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING,
			"palmas_pwron", pwron);
	if (err < 0) {
		dev_err(&pdev->dev, "Can't get IRQ for pwron: %d\n", err);
		goto free_input_dev;
	}

	enable_irq_wake(irq);

	err = input_register_device(input_dev);
	if (err) {
		dev_dbg(&pdev->dev, "Can't register power button: %d\n", err);
		goto free_irq;
	}

	platform_set_drvdata(pdev, pwron);

	return 0;

free_irq:
	free_irq(irq, input_dev);
free_input_dev:
	input_free_device(input_dev);
free_pwron:
	kfree(pwron);
	return err;
}

static int __devexit palmas_pwron_remove(struct platform_device *pdev)
{
	struct palmas_pwron *pwron = platform_get_drvdata(pdev);
	struct input_dev *input_dev = pwron->input_dev;
	int irq = platform_get_irq(pdev, 0);

	disable_irq_wake(irq);
	free_irq(irq, input_dev);
	input_unregister_device(input_dev);

	return 0;
}

#ifdef CONFIG_PM
/**
 *	palmas_pwron_suspend - called on device suspend
 *	@dev: device being suspended
 *
 *	Cancel the pending work item for reading the power button
 */
static int palmas_pwron_suspend(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct palmas_pwron *pwron = platform_get_drvdata(pdev);

	cancel_delayed_work_sync(&pwron->input_work);

	return 0;
}

/**
 *	palmas_pwron_resume - called on device resume
 *	@dev: device being resumed
 *
 *	Do nothing for now as there is nothing to do but leave the placeholder
 */
static int palmas_pwron_resume(struct device *dev)
{
	/* Do nothing for now */
	return 0;
}

static UNIVERSAL_DEV_PM_OPS(palmas_pwron_pm, palmas_pwron_suspend,
			palmas_pwron_resume, NULL);

#else
static UNIVERSAL_DEV_PM_OPS(palmas_pwron_pm, NULL, NULL, NULL);
#endif

static struct platform_driver palmas_pwron_driver = {
	.remove		= __devexit_p(palmas_pwron_remove),
	.driver		= {
		.name	= "palmas-pwrbutton",
		.owner	= THIS_MODULE,
		.pm	= &palmas_pwron_pm,
	},
};

static int __init palmas_pwron_init(void)
{
	return platform_driver_probe(&palmas_pwron_driver,
			palmas_pwron_probe);
}
module_init(palmas_pwron_init);

static void __exit palmas_pwron_exit(void)
{
	platform_driver_unregister(&palmas_pwron_driver);
}
module_exit(palmas_pwron_exit);

MODULE_ALIAS("platform:palmas-pwrbutton");
MODULE_DESCRIPTION("Palmas Power Button");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Girish S Ghongdemath <girishsg@ti.com>");
