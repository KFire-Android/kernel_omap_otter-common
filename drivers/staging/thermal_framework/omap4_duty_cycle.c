/*
 * Module to control max opp duty cycle
 *
 * Copyright (c) 2011 Texas Instrument
 * Contact: Eduardo Valentin <eduardo.valentin@ti.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/debugfs.h>
#include <linux/cpufreq.h>
#include <linux/cpu.h>
#include <linux/notifier.h>
#include <linux/workqueue.h>
#include <linux/mutex.h>
#include <linux/platform_device.h>
#include <linux/omap4_duty_cycle_governor.h>
#include <linux/omap4_duty_cycle.h>
#include <plat/omap_device.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Eduardo Valentin <eduardo.valentin@ti.com>");
MODULE_DESCRIPTION("Module to control max opp duty cycle");

#define NITRO_P(p, i)	((i) * (p) / 100)
enum omap4_duty_state {
	OMAP4_DUTY_NORMAL = 0,
	OMAP4_DUTY_HEATING,
	OMAP4_DUTY_COOLING_0,
	OMAP4_DUTY_COOLING_1,
};

/* state struct */
static unsigned int nitro_interval = 20000;
module_param(nitro_interval, int, 0);

static unsigned int nitro_percentage = 25;
module_param(nitro_percentage, int, 0);

static unsigned int nitro_rate = 1200000;
module_param(nitro_rate, int, 0);

static unsigned int cooling_rate = 1008000;
module_param(cooling_rate, int, 0);

struct duty_cycle_desc {
	bool enabled;
	bool saved_hotplug_enabled;
	int heating_budget;
	unsigned long t_heating_start;
	int (*cool_device) (struct thermal_dev *, int temp);
};

static struct duty_cycle_desc duty_desc;
static struct duty_cycle *t_duty;
static struct workqueue_struct *duty_wq;
static struct delayed_work work_exit_cool;
static struct delayed_work work_exit_heat;
static struct delayed_work work_enter_heat;
static struct work_struct work_enter_cool0;
static struct work_struct work_enter_cool1;
static struct work_struct work_cpu1_plugin;
static struct work_struct work_cpu1_plugout;
static enum omap4_duty_state state;

/* protect our data */
static DEFINE_MUTEX(mutex_duty);
/* used to protect the enable/disable operations */
static DEFINE_MUTEX(mutex_enabled);

static void omap4_duty_enter_normal(void)
{
	struct cpufreq_policy *policy = cpufreq_cpu_get(0);

	if (!policy) {
		pr_err("No CPUfreq policy at this point.\n");
		return;
	}

	pr_debug("%s enter at (%u)\n", __func__, policy->cur);
	state = OMAP4_DUTY_NORMAL;
	duty_desc.heating_budget = NITRO_P(nitro_percentage, nitro_interval);

	if (duty_desc.cool_device != NULL)
		duty_desc.cool_device(NULL, 0);

}

static void omap4_duty_exit_cool_wq(struct work_struct *work)
{
	mutex_lock(&mutex_duty);
	pr_debug("%s enter at ()\n", __func__);

	omap4_duty_enter_normal();
	mutex_unlock(&mutex_duty);
}

static void omap4_duty_enter_cooling(unsigned int next_max,
					enum omap4_duty_state next_state)
{
	struct cpufreq_policy *policy = cpufreq_cpu_get(0);

	if (!policy) {
		pr_err("No CPUfreq policy at this point.\n");
		return;
	}

	state = next_state;
	pr_debug("%s enter at (%u)\n", __func__, policy->cur);

	if ((duty_desc.cool_device != NULL) && (next_max != nitro_rate))
		duty_desc.cool_device(NULL, 1);

	queue_delayed_work(duty_wq, &work_exit_cool, msecs_to_jiffies(
		NITRO_P(100 - nitro_percentage, nitro_interval)));
}

static void omap4_duty_exit_heating_wq(struct work_struct *work)
{
	mutex_lock(&mutex_duty);
	pr_debug("%s enter at ()\n", __func__);

	omap4_duty_enter_cooling(cooling_rate, OMAP4_DUTY_COOLING_0);
	mutex_unlock(&mutex_duty);
}

static void omap4_duty_enter_c0_wq(struct work_struct *work)
{
	mutex_lock(&mutex_duty);
	pr_debug("%s enter at ()\n", __func__);

	omap4_duty_enter_cooling(cooling_rate, OMAP4_DUTY_COOLING_0);
	mutex_unlock(&mutex_duty);
}

static void omap4_duty_enter_c1_wq(struct work_struct *work)
{
	mutex_lock(&mutex_duty);
	pr_debug("%s enter at ()\n", __func__);

	omap4_duty_enter_cooling(nitro_rate, OMAP4_DUTY_COOLING_1);
	mutex_unlock(&mutex_duty);
}

static void omap4_duty_enter_heating(void)
{
	pr_debug("%s enter at ()\n", __func__);
	state = OMAP4_DUTY_HEATING;

	queue_delayed_work(duty_wq, &work_exit_heat,
				msecs_to_jiffies(duty_desc.heating_budget));
}

static void omap4_duty_enter_heat_wq(struct work_struct *work)
{
	mutex_lock(&mutex_duty);
	pr_debug("%s enter at ()\n", __func__);

	omap4_duty_enter_heating();
	mutex_unlock(&mutex_duty);
}

static int omap4_duty_frequency_change(struct notifier_block *nb,
					unsigned long val, void *data)
{
	struct cpufreq_freqs *freqs = data;
	pr_debug("%s enter\n", __func__);

	/* We are interested only in POSTCHANGE transactions */
	if (val != CPUFREQ_POSTCHANGE)
		goto done;

	pr_debug("%s: freqs %u->%u state: %d\n", __func__, freqs->old,
							freqs->new, state);
	switch (state) {
	case OMAP4_DUTY_NORMAL:
		if (freqs->new == nitro_rate) {
			duty_desc.t_heating_start = jiffies;
			queue_work(duty_wq, &work_enter_heat.work);
		}
		break;
	case OMAP4_DUTY_HEATING:
		if (freqs->new < nitro_rate) {
			int diff = jiffies_to_msecs(jiffies) -
				jiffies_to_msecs(duty_desc.t_heating_start);
			if (diff < 0)
				duty_desc.heating_budget = 0;
			else
				duty_desc.heating_budget -= diff;
			if (duty_desc.heating_budget <= 0) {
				queue_work(duty_wq, &work_enter_cool0);
			} else {
				cancel_delayed_work_sync(&work_exit_heat);
				queue_work(duty_wq, &work_enter_cool1);
			}
		}
		break;
	case OMAP4_DUTY_COOLING_0:
		break;
	case OMAP4_DUTY_COOLING_1:
		if (freqs->new == nitro_rate) {
			duty_desc.t_heating_start = jiffies;
			cancel_delayed_work_sync(&work_exit_cool);
			queue_work(duty_wq, &work_enter_heat.work);
		}
		break;
	}

done:
	return 0;
}

static struct notifier_block omap4_duty_nb = {
	.notifier_call	= omap4_duty_frequency_change,
};

static int omap4_duty_cycle_set_enabled(bool val, bool update)
{
	int ret;

	ret = mutex_lock_interruptible(&mutex_enabled);
	if (ret)
		return ret;

	ret = mutex_lock_interruptible(&mutex_duty);
	if (ret)
		goto unlock_enabled;

	if (duty_desc.enabled != val) {
		duty_desc.enabled = val;
		if (duty_desc.enabled) {
			/* Register the cpufreq notification */
			if (cpufreq_register_notifier(&omap4_duty_nb,
						CPUFREQ_TRANSITION_NOTIFIER)) {
				pr_err("%s: failed to setup cpufreq_notifier\n",
							__func__);
				ret = -EINVAL;
				goto unlock;
			}
			omap4_duty_enter_normal();
			/* We need to check in which frequency we are */
			if (cpufreq_get(0) == nitro_rate)
				queue_delayed_work(duty_wq, &work_enter_heat,
						msecs_to_jiffies(1));
		} else {
			cpufreq_unregister_notifier(&omap4_duty_nb,
					CPUFREQ_TRANSITION_NOTIFIER);
			mutex_unlock(&mutex_duty);
			cancel_delayed_work_sync(&work_enter_heat);
			cancel_delayed_work_sync(&work_exit_heat);
			cancel_work_sync(&work_enter_cool0);
			cancel_work_sync(&work_enter_cool1);
			cancel_delayed_work_sync(&work_exit_cool);
			ret = mutex_lock_interruptible(&mutex_duty);
			if (ret)
				goto unlock_enabled;
			omap4_duty_enter_normal();
		}
		/* we want to make sure the user gets what is asked */
		if (num_online_cpus() == 1 && update)
			duty_desc.saved_hotplug_enabled = duty_desc.enabled;
	}

unlock:
	mutex_unlock(&mutex_duty);
unlock_enabled:
	mutex_unlock(&mutex_enabled);
	return ret;
}

static void omap4_duty_enable_wq(struct work_struct *work)
{
	omap4_duty_cycle_set_enabled(duty_desc.saved_hotplug_enabled, false);
}

static void omap4_duty_disable_wq(struct work_struct *work)
{
	/* we don't want to overwrite what the user has requested */
	mutex_lock(&mutex_duty);
	duty_desc.saved_hotplug_enabled = duty_desc.enabled;
	mutex_unlock(&mutex_duty);
	omap4_duty_cycle_set_enabled(false, false);
}

static int omap4_duty_cycle_cpu_callback(struct notifier_block *nfb,
					unsigned long action, void *hcpu)
{
	unsigned int cpu = (unsigned long)hcpu;

	/* for now we don't care about cpu 0 */
	if (cpu == 0 || duty_wq == NULL)
		return NOTIFY_OK;

	switch (action) {
	case CPU_ONLINE:
	case CPU_ONLINE_FROZEN:
		queue_work(duty_wq, &work_cpu1_plugin);
		break;
	case CPU_DOWN_PREPARE:
	case CPU_DOWN_PREPARE_FROZEN:
		queue_work(duty_wq, &work_cpu1_plugout);
		break;
	case CPU_DOWN_FAILED:
	case CPU_DOWN_FAILED_FROZEN:
		queue_work(duty_wq, &work_cpu1_plugin);
		break;
	}
	return NOTIFY_OK;
}

static struct notifier_block __refdata omap4_duty_cycle_cpu_notifier = {
	.notifier_call = omap4_duty_cycle_cpu_callback,
};

static ssize_t show_nitro_interval(struct device *dev,
			struct device_attribute *devattr, char *buf)
{
	int ret;

	ret = mutex_lock_interruptible(&mutex_duty);
	if (ret)
		return ret;
	ret = sprintf(buf, "%u\n", nitro_interval);
	mutex_unlock(&mutex_duty);

	return ret;
}

static ssize_t store_nitro_interval(struct device *dev,
					struct device_attribute *attr,
					const char *buf,
					size_t count)
{
	unsigned long val;
	int ret;

	ret = strict_strtoul(buf, 0, &val);
	if (ret)
		return ret;
	if (val == 0)
		return -EINVAL;

	ret = mutex_lock_interruptible(&mutex_duty);
	if (ret)
		return ret;
	nitro_interval = val;
	mutex_unlock(&mutex_duty);

	return count;
}
static DEVICE_ATTR(nitro_interval, S_IRUGO | S_IWUSR, show_nitro_interval,
							store_nitro_interval);

static ssize_t show_nitro_percentage(struct device *dev,
			struct device_attribute *devattr, char *buf)
{
	int ret;

	ret = mutex_lock_interruptible(&mutex_duty);
	if (ret)
		return ret;
	ret = sprintf(buf, "%u\n", nitro_percentage);
	mutex_unlock(&mutex_duty);

	return ret;
}

static ssize_t store_nitro_percentage(struct device *dev,
					struct device_attribute *attr,
					const char *buf,
					size_t count)
{
	unsigned long val;
	int ret;

	ret = strict_strtoul(buf, 0, &val);
	if (ret)
		return ret;
	if (val == 0)
		return -EINVAL;

	ret = mutex_lock_interruptible(&mutex_duty);
	if (ret)
		return ret;
	nitro_percentage = val;
	mutex_unlock(&mutex_duty);

	return count;
}
static DEVICE_ATTR(nitro_percentage, S_IRUGO | S_IWUSR, show_nitro_percentage,
							store_nitro_percentage);

static ssize_t show_nitro_rate(struct device *dev,
			struct device_attribute *devattr, char *buf)
{
	int ret;

	ret = mutex_lock_interruptible(&mutex_duty);
	if (ret)
		return ret;
	ret = sprintf(buf, "%u\n", nitro_rate);
	mutex_unlock(&mutex_duty);

	return ret;
}

static ssize_t store_nitro_rate(struct device *dev,
					struct device_attribute *attr,
					const char *buf,
					size_t count)
{
	unsigned long val;
	int ret;

	ret = strict_strtoul(buf, 0, &val);
	if (ret)
		return ret;
	if (val == 0)
		return -EINVAL;

	ret = mutex_lock_interruptible(&mutex_duty);
	if (ret)
		return ret;
	nitro_rate = val;
	mutex_unlock(&mutex_duty);

	return count;
}
static DEVICE_ATTR(nitro_rate, S_IRUGO | S_IWUSR, show_nitro_rate,
							store_nitro_rate);

static ssize_t show_cooling_rate(struct device *dev,
			struct device_attribute *devattr, char *buf)
{
	int ret;

	ret = mutex_lock_interruptible(&mutex_duty);
	if (ret)
		return ret;
	ret = sprintf(buf, "%u\n", cooling_rate);
	mutex_unlock(&mutex_duty);

	return ret;
}

static ssize_t store_cooling_rate(struct device *dev,
					struct device_attribute *attr,
					const char *buf,
					size_t count)
{
	unsigned long val;
	int ret;

	ret = strict_strtoul(buf, 0, &val);
	if (ret)
		return ret;
	if (val == 0)
		return -EINVAL;

	ret = mutex_lock_interruptible(&mutex_duty);
	if (ret)
		return ret;
	cooling_rate = val;
	mutex_unlock(&mutex_duty);

	return count;
}
static DEVICE_ATTR(cooling_rate, S_IRUGO | S_IWUSR, show_cooling_rate,
							store_cooling_rate);

static ssize_t show_enabled(struct device *dev,
			struct device_attribute *devattr, char *buf)
{
	int ret;

	ret = mutex_lock_interruptible(&mutex_duty);
	if (ret)
		return ret;
	ret = sprintf(buf, "%u\n", (unsigned int)duty_desc.enabled);
	mutex_unlock(&mutex_duty);

	return ret;
}

static ssize_t store_enabled(struct device *dev,
					struct device_attribute *attr,
					const char *buf,
					size_t count)
{
	int ret;
	unsigned long val;

	ret = strict_strtoul(buf, 0, &val);
	if (ret)
		return ret;

	ret = omap4_duty_cycle_set_enabled(!!val, true);
	if (ret)
		return ret;

	return count;
}
static DEVICE_ATTR(enabled, S_IRUGO | S_IWUSR, show_enabled, store_enabled);

static struct attribute *attrs[] = {
	&dev_attr_nitro_interval.attr,
	&dev_attr_nitro_percentage.attr,
	&dev_attr_nitro_rate.attr,
	&dev_attr_cooling_rate.attr,
	&dev_attr_enabled.attr,
	NULL,
};

static const struct attribute_group attr_group = {
	.attrs = attrs,
};

static int __init omap4_duty_probe(struct platform_device *pdev)
{
	int rval;

	rval = sysfs_create_group(&pdev->dev.kobj, &attr_group);
	if (rval < 0) {
		dev_dbg(&pdev->dev, "Could not register sysfs interface.\n");
		return rval;
	}

	return 0;
}

static int  __exit omap4_duty_remove(struct platform_device *pdev)
{
	sysfs_remove_group(&pdev->dev.kobj, &attr_group);

	return 0;
}

static struct platform_device *omap4_duty_device;

static struct platform_driver omap4_duty_driver = {
	.driver = {
		.owner	= THIS_MODULE,
		.name	= "omap4_duty_cycle",
	},
	.remove		= __exit_p(omap4_duty_remove),
};

static int update_params(struct duty_cycle_params *dc_params)
{
	int ret;

	ret = mutex_lock_interruptible(&mutex_duty);
	if (ret)
		return ret;

	cooling_rate = dc_params->cooling_rate;
	nitro_rate = dc_params->nitro_rate;
	nitro_percentage = dc_params->nitro_percentage;
	nitro_interval = dc_params->nitro_interval;

	mutex_unlock(&mutex_duty);

	return 0;
}

int duty_cooling_dev_register(struct duty_cycle_dev *duty_cycle_dev)
{
	duty_desc.cool_device = duty_cycle_dev->cool_device;

	return 0;
}

void duty_cooling_dev_unregister(void)
{
	duty_desc.cool_device = NULL;
}

static int __init omap4_duty_register(void)
{
	t_duty = kzalloc(sizeof(struct duty_cycle), GFP_KERNEL);
	if (IS_ERR_OR_NULL(t_duty)) {
		pr_err("%s:Cannot allocate memory\n", __func__);

		return -ENOMEM;
	}
	t_duty->enable = omap4_duty_cycle_set_enabled;
	t_duty->update_params = update_params;
	omap4_duty_cycle_register(t_duty);

	return 0;
}

/* Module Interface */
static int __init omap4_duty_module_init(void)
{
	int err = 0;

	if (!cpu_is_omap443x())
		return 0;

	if ((!nitro_interval) || (nitro_percentage > 100) ||
		(nitro_percentage <= 0) || (nitro_rate <= 0) ||
		(cooling_rate <= 0))
		return -EINVAL;

	/* Data initialization */
	duty_wq = create_workqueue("omap4_duty_cycle");
	INIT_DELAYED_WORK(&work_exit_cool, omap4_duty_exit_cool_wq);
	INIT_DELAYED_WORK(&work_exit_heat, omap4_duty_exit_heating_wq);
	INIT_DELAYED_WORK(&work_enter_heat, omap4_duty_enter_heat_wq);
	INIT_WORK(&work_enter_cool0, omap4_duty_enter_c0_wq);
	INIT_WORK(&work_enter_cool1, omap4_duty_enter_c1_wq);
	INIT_WORK(&work_cpu1_plugin, omap4_duty_enable_wq);
	INIT_WORK(&work_cpu1_plugout, omap4_duty_disable_wq);
	duty_desc.heating_budget = NITRO_P(nitro_percentage, nitro_interval);

	if (num_online_cpus() > 1)
		err = omap4_duty_cycle_set_enabled(true, false);
	else
		err = omap4_duty_cycle_set_enabled(false, false);
	if (err) {
		pr_err("%s: failed to initialize the duty cycle\n", __func__);
		goto exit;
	}

	err = register_hotcpu_notifier(&omap4_duty_cycle_cpu_notifier);
	if (err) {
		pr_err("%s: failed to register cpu hotplug callback\n",
								__func__);
		goto disable;
	}

	omap4_duty_device = platform_device_register_simple("omap4_duty_cycle",
								-1, NULL, 0);
	if (IS_ERR(omap4_duty_device)) {
		err = PTR_ERR(omap4_duty_device);
		pr_err("Unable to register omap4 duty cycle device\n");
		goto unregister_hotplug;
	}

	err = omap4_duty_register();
	if (err)
		goto exit_pdevice;

	err = platform_driver_probe(&omap4_duty_driver, omap4_duty_probe);
	if (err)
		goto unregister_duty;

	return 0;

unregister_duty:
	kfree(t_duty);
exit_pdevice:
	platform_device_unregister(omap4_duty_device);
unregister_hotplug:
	unregister_hotcpu_notifier(&omap4_duty_cycle_cpu_notifier);
disable:
	if (duty_desc.enabled)
		omap4_duty_cycle_set_enabled(false, false);
exit:
	return err;
}

static void __exit omap4_duty_module_exit(void)
{
	platform_device_unregister(omap4_duty_device);
	platform_driver_unregister(&omap4_duty_driver);

	cpufreq_unregister_notifier(&omap4_duty_nb,
						CPUFREQ_TRANSITION_NOTIFIER);
	unregister_hotcpu_notifier(&omap4_duty_cycle_cpu_notifier);

	cancel_delayed_work_sync(&work_exit_cool);
	cancel_delayed_work_sync(&work_exit_heat);
	cancel_delayed_work_sync(&work_enter_heat);
	cancel_work_sync(&work_enter_cool0);
	cancel_work_sync(&work_enter_cool1);
	cancel_work_sync(&work_cpu1_plugin);
	cancel_work_sync(&work_cpu1_plugout);
	kfree(t_duty);
	destroy_workqueue(duty_wq);

	pr_debug("%s Done\n", __func__);
}

module_init(omap4_duty_module_init);
module_exit(omap4_duty_module_exit);

