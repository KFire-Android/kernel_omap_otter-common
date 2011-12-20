/* drivers/misc/twl6040-vib.c
 *
 * Copyright (C) 2010 Texas Instruments, Inc.
 * Copyright (C) 2008 Google, Inc.
 * Author: Dan Murphy <dmurphy@ti.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * Derived from: vib-gpio.c
 * Additional derivation from: twl6040-vibra.c
 */

#include <linux/err.h>
#include <linux/hrtimer.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/workqueue.h>
#include <linux/i2c/twl.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/twl6040-vib.h>
#include <linux/mfd/twl6040-codec.h>
#include "../staging/android/timed_output.h"

/* milliseconds */
#define TWL6040_VIB_POWER_DOWN_DELAY	5000

struct vib_data {
	struct timed_output_dev dev;
	struct work_struct vib_work;
	struct delayed_work power_work;
	struct hrtimer timer;
	spinlock_t lock;
	struct mutex io_mutex;
	struct mutex power_mutex;

	struct twl4030_codec_vibra_data *pdata;
	struct twl6040 *twl6040;

	int vib_power_state;
	int vib_state;
	bool powered;
};

static struct vib_data *misc_data;

static irqreturn_t twl6040_vib_irq_handler(int irq, void *data)
{
	struct vib_data *misc_data = data;
	struct twl6040 *twl6040 = misc_data->twl6040;
	u8 intid = 0, status = 0;

	intid = twl6040_reg_read(twl6040, TWL6040_REG_INTID);

	if (intid & TWL6040_VIBINT) {
		status = twl6040_reg_read(twl6040, TWL6040_REG_STATUS);
		if (status & TWL6040_VIBLOCDET) {
			pr_warn("Vibra left overcurrent detected\n");
			twl6040_clear_bits(twl6040, TWL6040_REG_VIBCTLL,
					   TWL6040_VIBENAL);
		}
		if (status & TWL6040_VIBROCDET) {
			pr_warn("Vibra right overcurrent detected\n");
			twl6040_clear_bits(twl6040, TWL6040_REG_VIBCTLR,
					   TWL6040_VIBENAR);
		}
	}

	return IRQ_HANDLED;
}

static void twl6040_vib_power_work(struct work_struct *work)
{
	mutex_lock(&misc_data->power_mutex);

	if (misc_data->powered) {
		twl6040_disable(misc_data->twl6040);
		misc_data->powered = false;
	}

	mutex_unlock(&misc_data->power_mutex);
}

static int twl6040_vib_power(bool on)
{
	int ret = 0;

	mutex_lock(&misc_data->power_mutex);

	cancel_delayed_work_sync(&misc_data->power_work);

	if (on == misc_data->powered)
		goto out;

	if (on) {
		/* vibra power-up is immediate */
		ret = twl6040_enable(misc_data->twl6040);
		if (ret)
			goto out;
		misc_data->powered = true;
	} else {
		/* vibra power-down is deferred */
		schedule_delayed_work(&misc_data->power_work,
			msecs_to_jiffies(TWL6040_VIB_POWER_DOWN_DELAY));
	}

out:
	mutex_unlock(&misc_data->power_mutex);
	return ret;
}

static void vib_set(int const new_power_state)
{
	struct twl6040 *twl6040 = misc_data->twl6040;
	u8 speed = misc_data->pdata->voltage_raise_speed;
	int ret;

	mutex_lock(&misc_data->io_mutex);

	/* already in requested state */
	if (new_power_state == misc_data->vib_power_state)
		goto out;

	/**
	 * @warning  VIBDATx registers MUST be setted BEFORE VIBENAx bit
	 *           setted in corresponding VIBCTLx registers
	 */
	if (new_power_state) {
		ret = twl6040_vib_power(true);
		if (ret)
			goto out;

		if (speed == 0x00)
			speed = 0x32;

		twl6040_reg_write(twl6040, TWL6040_REG_VIBDATL, speed);
		twl6040_reg_write(twl6040, TWL6040_REG_VIBDATR, speed);

		/*
		 * ERRATA: Disable overcurrent protection for at least
		 * 2.5ms when enabling vibrator drivers to avoid false
		 * overcurrent detection
		 */
		twl6040_set_bits(twl6040, TWL6040_REG_VIBCTLL,
				 TWL6040_VIBENAL | TWL6040_VIBCTRLLP);
		twl6040_set_bits(twl6040, TWL6040_REG_VIBCTLR,
				 TWL6040_VIBENAR | TWL6040_VIBCTRLRP);

		mdelay(4);

		twl6040_clear_bits(twl6040, TWL6040_REG_VIBCTLL,
				 TWL6040_VIBCTRLLP);
		twl6040_clear_bits(twl6040, TWL6040_REG_VIBCTLR,
				 TWL6040_VIBCTRLRP);
	} else {
		twl6040_reg_write(twl6040, TWL6040_REG_VIBDATL, 0x00);
		twl6040_reg_write(twl6040, TWL6040_REG_VIBDATR, 0x00);

		twl6040_clear_bits(twl6040, TWL6040_REG_VIBCTLL,
				   TWL6040_VIBENAL);
		twl6040_clear_bits(twl6040, TWL6040_REG_VIBCTLR,
				   TWL6040_VIBENAR);

		twl6040_vib_power(false);
	}
	misc_data->vib_power_state = new_power_state;

out:
	mutex_unlock(&misc_data->io_mutex);
}

static void vib_update(struct work_struct *work)
{
	vib_set(misc_data->vib_state);
}

static enum hrtimer_restart vib_timer_func(struct hrtimer *timer)
{
	struct vib_data *data =
	    container_of(timer, struct vib_data, timer);
	data->vib_state = 0;
	schedule_work(&data->vib_work);
	return HRTIMER_NORESTART;
}

static int vib_get_time(struct timed_output_dev *dev)
{
	struct vib_data *data =
	    container_of(dev, struct vib_data, dev);

	if (hrtimer_active(&data->timer)) {
		ktime_t r = hrtimer_get_remaining(&data->timer);
		struct timeval t = ktime_to_timeval(r);
		return t.tv_sec * 1000 + t.tv_usec / 1000;
	} else
		return 0;
}

static void vib_enable(struct timed_output_dev *dev, int value)
{
	struct vib_data *data = container_of(dev, struct vib_data, dev);
	unsigned long flags;

	if (value < 0) {
		pr_err("%s: Invalid vibrator timer value\n", __func__);
		return;
	}

	spin_lock_irqsave(&data->lock, flags);
	hrtimer_cancel(&data->timer);

	if (value == 0)
		data->vib_state = 0;
	else {
		value = (value > data->pdata->max_timeout ?
				 data->pdata->max_timeout : value);

		/* add hardware power-up time to requested timeout */
		if (!misc_data->powered)
			value += TWL6040_POWER_UP_TIME;

		data->vib_state = 1;
		hrtimer_start(&data->timer,
			      ktime_set(value / 1000, (value % 1000) * 1000000),
			      HRTIMER_MODE_REL);
	}

	spin_unlock_irqrestore(&data->lock, flags);

	schedule_work(&data->vib_work);
}

/*
 * This is a temporary solution until a more global haptics soltion is
 * available for haptics that need to occur in any application
 */
void vibrator_haptic_fire(int value)
{
	vib_enable(&misc_data->dev, value);
}

#if CONFIG_PM
static int vib_suspend(struct device *dev)
{
	hrtimer_cancel(&misc_data->timer);
	vib_set(0);
	flush_delayed_work_sync(&misc_data->power_work);

	return 0;
}
#else
#define vib_suspend NULL
#endif

static const struct dev_pm_ops vib_pm_ops = {
	.suspend = vib_suspend,
};

static int vib_probe(struct platform_device *pdev)
{
	struct twl4030_codec_vibra_data *pdata = pdev->dev.platform_data;
	struct vib_data *data;
	int ret = 0;

	if (!pdata) {
		ret = -EBUSY;
		goto err0;
	}

	data = kzalloc(sizeof(struct vib_data), GFP_KERNEL);
	if (!data) {
		ret = -ENOMEM;
		goto err0;
	}

	data->pdata = pdata;
	data->twl6040 = dev_get_drvdata(pdev->dev.parent);

	INIT_WORK(&data->vib_work, vib_update);
	INIT_DELAYED_WORK(&data->power_work, twl6040_vib_power_work);

	hrtimer_init(&data->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);

	data->timer.function = vib_timer_func;
	spin_lock_init(&data->lock);

	data->dev.name = "vibrator";
	data->dev.get_time = vib_get_time;
	data->dev.enable = vib_enable;
	ret = timed_output_dev_register(&data->dev);
	if (ret < 0)
		goto err1;

	if (data->pdata->init)
		ret = data->pdata->init();
	if (ret < 0)
		goto err2;

	misc_data = data;
	platform_set_drvdata(pdev, data);

	mutex_init(&misc_data->io_mutex);
	mutex_init(&misc_data->power_mutex);

	ret = twl6040_request_irq(data->twl6040, TWL6040_IRQ_VIB,
				twl6040_vib_irq_handler, 0,
				"twl6040_irq_vib", data);
	if (ret) {
		pr_err("%s: VIB IRQ request failed: %d\n", __func__, ret);
		goto err2;
	}

	vib_enable(&data->dev, data->pdata->initial_vibrate);

	return 0;

err2:
	timed_output_dev_unregister(&data->dev);
err1:
	kfree(data);
err0:
	return ret;
}

static int vib_remove(struct platform_device *pdev)
{
	struct vib_data *data = platform_get_drvdata(pdev);

	if (data->pdata->exit)
		data->pdata->exit();

	hrtimer_cancel(&data->timer);
	vib_set(0);
	flush_delayed_work_sync(&data->power_work);
	twl6040_free_irq(data->twl6040, TWL6040_IRQ_VIB, data);
	timed_output_dev_unregister(&data->dev);
	kfree(data);

	return 0;
}

/* TO DO: Need to make this drivers own platform data entries */
static struct platform_driver twl6040_vib_driver = {
	.probe = vib_probe,
	.remove = vib_remove,
	.driver = {
		   .name = VIB_NAME,
		   .owner = THIS_MODULE,
		   .pm = &vib_pm_ops,
	},
};

static int __init twl6040_vib_init(void)
{
	return platform_driver_register(&twl6040_vib_driver);
}

static void __exit twl6040_vib_exit(void)
{
	platform_driver_unregister(&twl6040_vib_driver);
}

module_init(twl6040_vib_init);
module_exit(twl6040_vib_exit);

MODULE_AUTHOR("Dan Murphy <dmurphy@ti.com>");
MODULE_DESCRIPTION("TWL6040 Vibrator Driver");
MODULE_LICENSE("GPL");
