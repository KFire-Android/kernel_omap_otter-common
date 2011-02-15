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

struct vib_data {
	struct timed_output_dev dev;
	struct work_struct vib_work;
	struct hrtimer timer;
	spinlock_t lock;

	struct twl4030_codec_vibra_data *pdata;
	struct twl6040_codec *twl6040;

	int vib_power_state;
	int vib_state;
};

struct vib_data *misc_data;

static void vib_set(int on)
{
	struct twl6040_codec *twl6040 = misc_data->twl6040;
	u8 reg = 0;

	if (on) {
		reg = twl6040_reg_read(twl6040, TWL6040_REG_VIBCTLL);
		twl6040_reg_write(twl6040, TWL6040_REG_VIBCTLL,
				  reg | TWL6040_VIBENAL | TWL6040_VIBCTRLLP);

		reg = twl6040_reg_read(twl6040, TWL6040_REG_VIBCTLR);
		twl6040_reg_write(twl6040, TWL6040_REG_VIBCTLR,
				  reg | TWL6040_VIBENAR | TWL6040_VIBCTRLRN);

	} else {
		reg = twl6040_reg_read(twl6040, TWL6040_REG_VIBCTLL)
			& ~TWL6040_VIBENAL;
		twl6040_reg_write(twl6040, TWL6040_REG_VIBCTLL, reg);

		reg = twl6040_reg_read(twl6040, TWL6040_REG_VIBCTLR)
			& ~TWL6040_VIBENAR;
		twl6040_reg_write(twl6040, TWL6040_REG_VIBCTLR, reg);
	}
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

	twl6040_reg_write(data->twl6040, TWL6040_REG_VIBDATL, 0x32);
	twl6040_reg_write(data->twl6040, TWL6040_REG_VIBDATR, 0x32);

	spin_lock_irqsave(&data->lock, flags);
	hrtimer_cancel(&data->timer);

	if (value == 0)
		data->vib_state = 0;
	else {
		value = (value > data->pdata->max_timeout ?
				 data->pdata->max_timeout : value);
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
	struct vib_data *data = dev_get_drvdata(dev);

	twl6040_disable(data->twl6040);

	return 0;
}

static int vib_resume(struct device *dev)
{
	struct vib_data *data = dev_get_drvdata(dev);

	twl6040_enable(data->twl6040);

	return 0;
}
#else
#define vib_suspend NULL
#define vib_resume  NULL
#endif

static const struct dev_pm_ops vib_pm_ops = {
	.suspend = vib_suspend,
	.resume = vib_resume,
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

	twl6040_enable(data->twl6040);
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

	twl6040_disable(data->twl6040);

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
