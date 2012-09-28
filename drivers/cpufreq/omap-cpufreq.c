/*
 *  CPU frequency scaling for OMAP using OPP information
 *
 *  Copyright (C) 2005 Nokia Corporation
 *  Written by Tony Lindgren <tony@atomide.com>
 *
 *  Based on cpu-sa1110.c, Copyright (C) 2001 Russell King
 *
 * Copyright (C) 2007-2011 Texas Instruments, Inc.
 * - OMAP3/4 support by Rajendra Nayak, Santosh Shilimkar
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/cpufreq.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/err.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/opp.h>
#include <linux/cpu.h>
#include <linux/module.h>
#include <linux/thermal_framework.h>

#include <asm/smp_plat.h>
#include <asm/cpu.h>

#include <plat/clock.h>
#include <plat/omap-pm.h>
#include <plat/common.h>
#include <plat/omap_device.h>
#include <plat/dvfs.h>

#include <mach/hardware.h>

/* OPP tolerance in percentage */
#define	OPP_TOLERANCE	4

#ifdef CONFIG_SMP
struct lpj_info {
	unsigned long	ref;
	unsigned int	freq;
};

static DEFINE_PER_CPU(struct lpj_info, lpj_ref);
static struct lpj_info global_lpj_ref;
#endif

static struct cpufreq_frequency_table *freq_table;
static atomic_t freq_table_users = ATOMIC_INIT(0);
static struct clk *mpu_clk;
static char *mpu_clk_name;
static struct device *mpu_dev;
static DEFINE_MUTEX(omap_cpufreq_lock);

static unsigned int max_thermal;
static unsigned int max_freq;
static unsigned int current_target_freq;
static unsigned int current_cooling_level;
static unsigned int cpu_cooling_level;
static unsigned int case_cooling_level;
static unsigned int new_cooling_level;
static bool omap_cpufreq_ready;

static unsigned int en_therm_freq_print;
module_param(en_therm_freq_print, uint, 0644);
MODULE_PARM_DESC(en_therm_freq_print,
		"Enable Debug prints of CPU Freq change due to thermal");

static unsigned int omap_getspeed(unsigned int cpu)
{
	unsigned long rate;

	if (cpu >= NR_CPUS)
		return 0;

	rate = clk_get_rate(mpu_clk) / 1000;
	return rate;
}

static int omap_cpufreq_scale(struct cpufreq_policy *policy,
				unsigned int target_freq, unsigned int cur_freq,
				unsigned int relation)
{
	unsigned int i;
	int ret = 0;
	struct cpufreq_freqs freqs;

	if (!freq_table) {
		dev_err(mpu_dev, "%s: cpu%d: no freq table!\n", __func__,
				policy->cpu);
		return -EINVAL;
	}

	ret = cpufreq_frequency_table_target(policy, freq_table, target_freq,
			relation, &i);
	if (ret) {
		dev_dbg(mpu_dev, "%s: cpu%d: no freq match for %d(ret=%d)\n",
				__func__, policy->cpu, target_freq, ret);
		return ret;
	}
	freqs.new = freq_table[i].frequency;
	if (!freqs.new) {
		dev_err(mpu_dev, "%s: cpu%d: no match for freq %d\n", __func__,
				policy->cpu, target_freq);
		return -EINVAL;
	}

	freqs.old = omap_getspeed(policy->cpu);
	freqs.cpu = policy->cpu;

	/*
	 * If the new frequency is more than the max allowed
	 * frequency, go ahead and scale the mpu device to proper frequency
	 */
	if (freqs.new > max_thermal)
		freqs.new = max_thermal;

	if (freqs.old == freqs.new && cur_freq == freqs.new)
		return ret;

	/* notifiers */
	for_each_cpu(i, policy->cpus) {
		freqs.cpu = i;
		cpufreq_notify_transition(&freqs, CPUFREQ_PRECHANGE);
	}

#ifdef CONFIG_CPU_FREQ_DEBUG
	pr_info("cpufreq-omap: transition: %u --> %u\n", freqs.old, freqs.new);
#endif

	ret = omap_device_scale(mpu_dev, freqs.new * 1000);

	freqs.new = omap_getspeed(policy->cpu);

#ifdef CONFIG_SMP
	/*
	 * Note that loops_per_jiffy is not updated on SMP systems in
	 * cpufreq driver. So, update the per-CPU loops_per_jiffy value
	 * on frequency transition. We need to update all dependent CPUs.
	 */
	for_each_cpu(i, policy->cpus) {
		struct lpj_info *lpj = &per_cpu(lpj_ref, i);
		if (!lpj->freq) {
			lpj->ref = per_cpu(cpu_data, i).loops_per_jiffy;
			lpj->freq = freqs.old;
		}

		per_cpu(cpu_data, i).loops_per_jiffy =
			cpufreq_scale(lpj->ref, lpj->freq, freqs.new);
	}

	/* And don't forget to adjust the global one */
	if (!global_lpj_ref.freq) {
		global_lpj_ref.ref = loops_per_jiffy;
		global_lpj_ref.freq = freqs.old;
	}
	loops_per_jiffy = cpufreq_scale(global_lpj_ref.ref, global_lpj_ref.freq,
					freqs.new);
#endif

	/* notifiers */
	for_each_cpu(i, policy->cpus) {
		freqs.cpu = i;
		cpufreq_notify_transition(&freqs, CPUFREQ_POSTCHANGE);
	}

	return ret;
}

static int omap_verify_speed(struct cpufreq_policy *policy)
{
	if (!freq_table)
		return -EINVAL;
	return cpufreq_frequency_table_verify(policy, freq_table);
}

static int omap_target(struct cpufreq_policy *policy,
			unsigned int target_freq, unsigned int relation)
{
	unsigned int i;
	int ret = 0;

	if (!freq_table) {
		dev_err(mpu_dev, "%s: cpu%d: no freq table!\n", __func__,
			policy->cpu);
		return -EINVAL;
	}
	ret = cpufreq_frequency_table_target(policy, freq_table, target_freq,
								relation, &i);
	if (ret) {
		dev_dbg(mpu_dev, "%s: cpu%d: no freq match for %d(ret=%d)\n",
				__func__, policy->cpu, target_freq, ret);
		return ret;
	}

	mutex_lock(&omap_cpufreq_lock);
	current_target_freq = freq_table[i].frequency;
	ret = omap_cpufreq_scale(policy, current_target_freq, policy->cur,
				 relation);
	mutex_unlock(&omap_cpufreq_lock);

	return ret;
}

static inline void freq_table_free(void)
{
	if (atomic_dec_and_test(&freq_table_users))
		opp_free_cpufreq_table(mpu_dev, &freq_table);
}

#ifdef CONFIG_THERMAL_FRAMEWORK
static unsigned int omap_thermal_lower_speed(void)
{
	unsigned int max = 0;
	unsigned int curr;
	int i;

	curr = max_thermal;

	for (i = 0; freq_table[i].frequency != CPUFREQ_TABLE_END; i++)
		if (freq_table[i].frequency > max &&
				freq_table[i].frequency < curr)
			max = freq_table[i].frequency;

	if (!max)
		return curr;

	return max;
}

/* This function needs to be called with omap_cpufreq_lock held */
static void omap_thermal_step_freq_down(struct cpufreq_policy *policy)
{
	unsigned int cur;

	max_thermal = omap_thermal_lower_speed();

	if (en_therm_freq_print)
		pr_info("%s: temperature too high, starting cpu throtling at max %u\n",
			__func__, max_thermal);

	cur = omap_getspeed(0);
	if (cur > max_thermal)
		omap_cpufreq_scale(policy, max_thermal, cur,
				   CPUFREQ_RELATION_L);
}

/* This function needs to be called with omap_cpufreq_lock held */
static void omap_thermal_step_freq_up(struct cpufreq_policy *policy)
{
	unsigned int cur;

	max_thermal = max_freq;

	if (en_therm_freq_print)
		pr_info("%s: temperature reduced, stepping up to %i\n",
			__func__, current_target_freq);

	cur = omap_getspeed(0);
	omap_cpufreq_scale(policy, current_target_freq, cur,
			   CPUFREQ_RELATION_L);
}

/*
 * cpufreq_apply_cooling: based on requested cooling level, throttle the cpu
 * @param cooling_level: percentage of required cooling at the moment
 *
 * The maximum cpu frequency will be readjusted based on the required
 * cooling_level.
*/
static int cpufreq_apply_cooling(struct thermal_dev *dev, int cooling_level)
{
	struct cpufreq_policy policy;

	cpufreq_get_policy(&policy, 0);

	mutex_lock(&omap_cpufreq_lock);

	if (!strcmp(dev->domain_name, "case")) {
		if (cooling_level > case_subzone_number)
			case_cooling_level++;
		if (cooling_level == 0)
			case_cooling_level = 0;
	} else {
		cpu_cooling_level = cooling_level;
	}

	if (case_cooling_level > cpu_cooling_level)
		new_cooling_level = case_cooling_level;
	else
		new_cooling_level = cpu_cooling_level;

	pr_debug("%s: cooling_level %d case %d cpu %d new %d curr %d\n",
		__func__, cooling_level, case_cooling_level,
		cpu_cooling_level, new_cooling_level, current_cooling_level);

	if (new_cooling_level < current_cooling_level) {
		pr_debug("%s: Unthrottle cool level %i curr cool %i\n",
			__func__, new_cooling_level, current_cooling_level);
		omap_thermal_step_freq_up(&policy);
	} else if (new_cooling_level > current_cooling_level) {
		pr_debug("%s: Throttle cool level %i curr cool %i\n",
			__func__, new_cooling_level, current_cooling_level);
		omap_thermal_step_freq_down(&policy);
	}

	current_cooling_level = new_cooling_level;

	mutex_unlock(&omap_cpufreq_lock);

	return 0;
}

static struct thermal_dev_ops cpufreq_cooling_ops = {
	.cool_device = cpufreq_apply_cooling,
};

static struct thermal_dev thermal_dev = {
	.name		= "cpufreq_cooling.0",
	.domain_name	= "cpu",
	.dev_ops	= &cpufreq_cooling_ops,
};

static struct thermal_dev case_thermal_dev = {
	.name		= "cpufreq_cooling.1",
	.domain_name	= "case",
	.dev_ops	= &cpufreq_cooling_ops,
};

static int __init omap_cpufreq_cooling_init(void)
{
	int ret;

	ret = thermal_cooling_dev_register(&thermal_dev);
	if (ret)
		return ret;

	return thermal_cooling_dev_register(&case_thermal_dev);
}

static void __exit omap_cpufreq_cooling_exit(void)
{
	thermal_cooling_dev_unregister(&thermal_dev);
	thermal_cooling_dev_unregister(&case_thermal_dev);
}
#else
static int __init omap_cpufreq_cooling_init(void)
{
	return 0;
}

static void __exit omap_cpufreq_cooling_exit(void)
{
}
#endif

static int __cpuinit omap_cpu_init(struct cpufreq_policy *policy)
{
	int result = 0;
	int i;

	mpu_clk = clk_get(NULL, mpu_clk_name);
	if (IS_ERR(mpu_clk))
		return PTR_ERR(mpu_clk);

	if (policy->cpu >= NR_CPUS) {
		result = -EINVAL;
		goto fail_ck;
	}

	policy->cur = policy->min = policy->max = omap_getspeed(policy->cpu);

	if (atomic_inc_return(&freq_table_users) == 1)
		result = opp_init_cpufreq_table(mpu_dev, &freq_table);

	if (result) {
		dev_err(mpu_dev, "%s: cpu%d: failed creating freq table[%d]\n",
				__func__, policy->cpu, result);
		goto fail_ck;
	}

	result = cpufreq_frequency_table_cpuinfo(policy, freq_table);
	if (result)
		goto fail_table;

	cpufreq_frequency_table_get_attr(freq_table, policy->cpu);

	policy->min = policy->cpuinfo.min_freq;
	policy->max = policy->cpuinfo.max_freq;
	policy->cur = omap_getspeed(policy->cpu);

	for (i = 0; freq_table[i].frequency != CPUFREQ_TABLE_END; i++)
		max_freq = max(freq_table[i].frequency, max_freq);

	/*
	 * On OMAP SMP configuartion, both processors share the voltage
	 * and clock. So both CPUs needs to be scaled together and hence
	 * needs software co-ordination. Use cpufreq affected_cpus
	 * interface to handle this scenario. Additional is_smp() check
	 * is to keep SMP_ON_UP build working.
	 */
	if (is_smp()) {
		policy->shared_type = CPUFREQ_SHARED_TYPE_ANY;
		cpumask_setall(policy->cpus);
	}

	/* FIXME: what's the actual transition time? */
	policy->cpuinfo.transition_latency = 300 * 1000;

	return 0;

fail_table:
	freq_table_free();
fail_ck:
	clk_put(mpu_clk);
	return result;
}

static int omap_cpu_exit(struct cpufreq_policy *policy)
{
	freq_table_free();
	clk_put(mpu_clk);
	return 0;
}

static struct freq_attr *omap_cpufreq_attr[] = {
	&cpufreq_freq_attr_scaling_available_freqs,
	NULL,
};

static struct cpufreq_driver omap_driver = {
	.flags		= CPUFREQ_STICKY,
	.verify		= omap_verify_speed,
	.target		= omap_target,
	.get		= omap_getspeed,
	.init		= omap_cpu_init,
	.exit		= omap_cpu_exit,
	.name		= "omap",
	.attr		= omap_cpufreq_attr,
};

static int __init omap_cpufreq_init(void)
{
	int ret;

	if (cpu_is_omap24xx())
		mpu_clk_name = "virt_prcm_set";
	else if (cpu_is_omap34xx())
		mpu_clk_name = "dpll1_ck";
	else if (cpu_is_omap443x() || cpu_is_omap54xx())
		mpu_clk_name = "dpll_mpu_ck";
	else if (cpu_is_omap446x())
		mpu_clk_name = "virt_dpll_mpu_ck";

	if (!mpu_clk_name) {
		pr_err("%s: unsupported Silicon?\n", __func__);
		return -EINVAL;
	}

	mpu_dev = omap_device_get_by_hwmod_name("mpu");
	if (!mpu_dev) {
		pr_warning("%s: unable to get the mpu device\n", __func__);
		return -EINVAL;
	}

	ret = cpufreq_register_driver(&omap_driver);
	omap_cpufreq_ready = !ret;

	max_thermal = max_freq;
	current_cooling_level = 0;
	en_therm_freq_print = 0;
	cpu_cooling_level = 0;
	case_cooling_level = 0;

	if (!ret)
		ret = omap_cpufreq_cooling_init();

	return ret;
}

static void __exit omap_cpufreq_exit(void)
{
	omap_cpufreq_cooling_exit();
	cpufreq_unregister_driver(&omap_driver);
}

MODULE_DESCRIPTION("cpufreq driver for OMAP SoCs");
MODULE_LICENSE("GPL");
module_init(omap_cpufreq_init);
module_exit(omap_cpufreq_exit);
