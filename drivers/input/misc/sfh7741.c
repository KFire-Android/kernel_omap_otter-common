/*
 * SFH7741 Proximity Driver
 *
 * Copyright (C) 2010 Texas Instruments
 *
 * Author: Shubhrajyoti D <shubhrajyoti@ti.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
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

#include <linux/interrupt.h>
#include <linux/pm.h>
#include <linux/platform_device.h>
#include <linux/input.h>
#include <linux/input/sfh7741.h>
#include <linux/slab.h>

struct sfh7741_drvdata {
	struct input_dev *input;
	int irq;
	int prox_enable;
	/* mutex for sysfs operations */
	struct mutex lock;
	void (*activate_func)(int state);
	int (*read_prox)(void);
};

static irqreturn_t sfh7741_isr(int irq, void *dev_id)
{
	struct sfh7741_drvdata *ddata = dev_id;
	int proximity;

	proximity = ddata->read_prox();
	input_report_abs(ddata->input, ABS_DISTANCE, proximity);
	input_sync(ddata->input);

	return IRQ_HANDLED;
}

static ssize_t set_prox_state(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	int state;
	struct platform_device *pdev = to_platform_device(dev);
	struct sfh7741_drvdata *ddata = platform_get_drvdata(pdev);

	if (sscanf(buf, "%u", &state) != 1)
		return -EINVAL;

	if ((state != 1) && (state != 0))
		return -EINVAL;

	ddata->activate_func(state);

	mutex_lock(&ddata->lock);
	if (state != ddata->prox_enable) {
		if (state)
			enable_irq(ddata->irq);
		else
			disable_irq(ddata->irq);
		ddata->prox_enable = state;
	}
	mutex_unlock(&ddata->lock);
	return strnlen(buf, count);
}

static ssize_t show_prox_state(struct device *dev,
			struct device_attribute *attr,
			char *buf)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct sfh7741_drvdata *ddata = platform_get_drvdata(pdev);
	return sprintf(buf, "%u\n", ddata->prox_enable);
}
static DEVICE_ATTR(state, S_IWUSR | S_IRUGO, show_prox_state, set_prox_state);


static struct attribute *sfh7741_attributes[] = {
	&dev_attr_state.attr,
	NULL
};

static const struct attribute_group sfh7741_attr_group = {
	.attrs = sfh7741_attributes,
};

static int __devinit sfh7741_probe(struct platform_device *pdev)
{
	struct sfh7741_platform_data *pdata = pdev->dev.platform_data;
	struct sfh7741_drvdata *ddata;
	struct device *dev = &pdev->dev;
	struct input_dev *input;
	int  error;
	char *desc = "sfh7741";

	pr_info("SFH7741: Proximity sensor\n");

	if (!pdata->activate_func || !pdata->read_prox) {
		dev_err(dev, "The activate and read func not allocated\n");
		return -EINVAL;
	}

	ddata = kzalloc(sizeof(struct sfh7741_drvdata),
			GFP_KERNEL);
	if (!ddata)
		return -ENOMEM;
	input = input_allocate_device();
	if (!input) {
		dev_err(dev, "failed to allocate input device\n");
		kfree(ddata);
		return -ENOMEM;
	}

	input->name = pdev->name;
	input->phys = "sfh7741/input0";
	input->dev.parent = &pdev->dev;

	input->id.bustype = BUS_HOST;
	ddata->irq = pdata->irq;
	ddata->prox_enable = pdata->prox_enable;

	ddata->activate_func = pdata->activate_func;
	ddata->read_prox =  pdata->read_prox;

	ddata->input = input;
	__set_bit(EV_ABS, input->evbit);

	input_set_abs_params(input, ABS_DISTANCE, 0, 1, 0, 0);

	error = input_register_device(input);
	if (error) {
		dev_err(dev, "Unable to register input device,error: %d\n",
				error);
		goto fail1;
	}

	platform_set_drvdata(pdev, ddata);

	error = request_threaded_irq(pdata->irq , NULL ,
				sfh7741_isr,
				IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING,
				desc, ddata);
	if (error) {
		dev_err(dev, "Unable to claim irq %d; error %d\n",
			pdata->irq, error);
		goto fail2;
	}

	mutex_init(&ddata->lock);
	error = sysfs_create_group(&dev->kobj, &sfh7741_attr_group);
	if (error) {
		dev_err(dev, "failed to create sysfs entries\n");
		mutex_destroy(&ddata->lock);
	}

	return 0;

fail2:
	input_unregister_device(input);
	platform_set_drvdata(pdev, NULL);
fail1:
	input_free_device(input);
	kfree(ddata);
	return error;

}

static int __devexit sfh7741_remove(struct platform_device *pdev)
{
	struct sfh7741_drvdata *ddata = platform_get_drvdata(pdev);
	struct device *dev = &pdev->dev;
	mutex_destroy(&ddata->lock);
	sysfs_remove_group(&dev->kobj, &sfh7741_attr_group);
	free_irq(ddata->irq, (void *)ddata);
	input_unregister_device(ddata->input);
	kfree(ddata);
	return 0;
}

#ifdef CONFIG_PM
static int sfh7741_suspend(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct sfh7741_drvdata *ddata = platform_get_drvdata(pdev);
	ddata->activate_func(0);
	return 0;
}

static int sfh7741_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct sfh7741_drvdata *ddata = platform_get_drvdata(pdev);
	ddata->activate_func(1);
	return 0;
}

static const struct dev_pm_ops sfh7741_pm_ops = {
	.suspend	= sfh7741_suspend,
	.resume		= sfh7741_resume,
};
#endif

static struct platform_driver sfh7741_device_driver = {
	.probe		= sfh7741_probe,
	.remove		= __devexit_p(sfh7741_remove),
	.driver		= {
		.name	= "sfh7741",
		.owner	= THIS_MODULE,
#ifdef CONFIG_PM
		.pm	= &sfh7741_pm_ops,
#endif
	}
};

static int __init sfh7741_init(void)
{
	return platform_driver_register(&sfh7741_device_driver);
}

static void __exit sfh7741_exit(void)
{
	platform_driver_unregister(&sfh7741_device_driver);
}

module_init(sfh7741_init);
module_exit(sfh7741_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Texas Instruments");
MODULE_DESCRIPTION("Proximity sensor SFH7741 driver");
MODULE_ALIAS("platform:sfh7741");
