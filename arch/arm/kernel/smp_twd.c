/*
 *  linux/arch/arm/kernel/smp_twd.c
 *
 *  Copyright (C) 2002 ARM Ltd.
 *  All Rights Reserved
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/smp.h>
#include <linux/jiffies.h>
#include <linux/clockchips.h>
#include <linux/irq.h>
#include <linux/io.h>
#include <linux/cpufreq.h>

#include <asm/smp_twd.h>
#include <asm/hardware/gic.h>

/* set up by the platform code */
void __iomem *twd_base;

static unsigned long twd_timer_rate;
static unsigned long twd_clock_ratio;
static unsigned long twd_target_rate;

static void twd_set_mode(enum clock_event_mode mode,
			struct clock_event_device *clk)
{
	unsigned long ctrl = __raw_readl(twd_base + TWD_TIMER_CONTROL);
	ctrl &= TWD_TIMER_CONTROL_PRESCALE_MASK;

	switch (mode) {
	case CLOCK_EVT_MODE_PERIODIC:
		/* timer load already set up */
		ctrl |= TWD_TIMER_CONTROL_ENABLE | TWD_TIMER_CONTROL_IT_ENABLE
			| TWD_TIMER_CONTROL_PERIODIC;
		break;
	case CLOCK_EVT_MODE_ONESHOT:
		/* period set, and timer enabled in 'next_event' hook */
		ctrl |= TWD_TIMER_CONTROL_IT_ENABLE | TWD_TIMER_CONTROL_ONESHOT;
		break;
	case CLOCK_EVT_MODE_UNUSED:
	case CLOCK_EVT_MODE_SHUTDOWN:
	default:
		break;
	}

	__raw_writel(ctrl, twd_base + TWD_TIMER_CONTROL);
}

static int twd_set_next_event(unsigned long evt,
			struct clock_event_device *unused)
{
	unsigned long ctrl = __raw_readl(twd_base + TWD_TIMER_CONTROL);

	ctrl |= TWD_TIMER_CONTROL_ENABLE;

	__raw_writel(evt, twd_base + TWD_TIMER_COUNTER);
	__raw_writel(ctrl, twd_base + TWD_TIMER_CONTROL);

	return 0;
}

static void __twd_recalc_prescaler(unsigned long cpu_rate)
{
	u32 ctrl;
	int prescaler;
	unsigned long twd_clock_input;

	WARN_ON(twd_clock_ratio == 0 || twd_timer_rate == 0);

	twd_clock_input = cpu_rate / twd_clock_ratio;

	prescaler = DIV_ROUND_CLOSEST(twd_clock_input, twd_timer_rate);
	prescaler = clamp(prescaler - 1, 0, 0xFF);

	pr_debug("CPU%d TWD prescaler= %d for input_clock %ldMHz "
		"and target clock %ldMHz\n",
		hard_smp_processor_id(), prescaler,
		twd_clock_input/1000000, twd_timer_rate/1000000);

	ctrl = __raw_readl(twd_base + TWD_TIMER_CONTROL);
	ctrl &= ~TWD_TIMER_CONTROL_PRESCALE_MASK;
	ctrl |= prescaler << 8;
	__raw_writel(ctrl, twd_base + TWD_TIMER_CONTROL);
}

/*
 * local_timer_ack: checks for a local timer interrupt.
 *
 * If a local timer interrupt has occurred, acknowledge and return 1.
 * Otherwise, return 0.
 */
int twd_timer_ack(void)
{
	if (__raw_readl(twd_base + TWD_TIMER_INTSTAT)) {
		__raw_writel(1, twd_base + TWD_TIMER_INTSTAT);
		return 1;
	}

	return 0;
}

static void __cpuinit twd_calibrate_rate(unsigned long target_rate,
	unsigned int arch_clk_divider)
{
	unsigned long load, count;
	u64 waitjiffies;
	unsigned long cpu_rate;
	static bool printed;

	if (!printed)
		pr_info("Calibrating local timer... ");
	/*
	 * If this is the first time round, we need to work out how fast
	 * the timer ticks
	 */
	if (twd_timer_rate == 0) {

		/* Wait for a tick to start */
		waitjiffies = get_jiffies_64() + 1;

		while (get_jiffies_64() < waitjiffies)
			udelay(10);

		/* OK, now the tick has started, let's get the timer going */
		waitjiffies += 5;

		/* enable, no interrupt or reload */
		__raw_writel(0x1, twd_base + TWD_TIMER_CONTROL);

		/* maximum value */
		__raw_writel(0xFFFFFFFFU, twd_base + TWD_TIMER_COUNTER);

		while (get_jiffies_64() < waitjiffies)
			udelay(10);

		count = __raw_readl(twd_base + TWD_TIMER_COUNTER);

		twd_timer_rate = (0xFFFFFFFFU - count) * (HZ / 5);
	}

	/*
	 * If a target rate has been requested, adjust the TWD prescaler
	 * to get the closest lower frequency.
	 */
	if (target_rate) {
		twd_clock_ratio = arch_clk_divider;
		twd_target_rate = target_rate;

		cpu_rate = twd_timer_rate * arch_clk_divider;
		twd_timer_rate = twd_target_rate;
		__twd_recalc_prescaler(cpu_rate);
	}

	if (!printed) {
		pr_info("%lu.%02luMHz.\n",
			twd_timer_rate / 1000000,
			(twd_timer_rate / 10000) % 100);
		printed = 1;
	}

	load = twd_timer_rate / HZ;

	__raw_writel(load, twd_base + TWD_TIMER_LOAD);
}

/*
 * Setup the local clock events for a CPU.
 */
static void __cpuinit __twd_timer_setup(struct clock_event_device *clk,
	unsigned long target_rate, unsigned int arch_clk_divider)
{
	unsigned long flags;

	twd_calibrate_rate(target_rate, arch_clk_divider);

	clk->name = "local_timer";

	/* TWD stops in deeper C-states and needs C3STOP */
	clk->features = CLOCK_EVT_FEAT_PERIODIC | CLOCK_EVT_FEAT_ONESHOT |
			CLOCK_EVT_FEAT_C3STOP;
	clk->rating = 350;
	clk->set_mode = twd_set_mode;
	clk->set_next_event = twd_set_next_event;
	clk->shift = 20;
	clk->mult = div_sc(twd_timer_rate, NSEC_PER_SEC, clk->shift);
	clk->max_delta_ns = clockevent_delta2ns(0xffffffff, clk);
	clk->min_delta_ns = clockevent_delta2ns(0xf, clk);

	/* Make sure our local interrupt controller has this enabled */
	local_irq_save(flags);
	irq_to_desc(clk->irq)->status |= IRQ_NOPROBE;
	get_irq_chip(clk->irq)->unmask(clk->irq);
	local_irq_restore(flags);

	clockevents_register_device(clk);
}

#ifdef CONFIG_CPU_FREQ

static void twd_recalc_prescaler_all_cpu(void *data)
{
	unsigned long cpu_rate = *(unsigned long *)data;
	__twd_recalc_prescaler(cpu_rate);
}

static void twd_recalc_prescaler(unsigned int cpu, unsigned long cpu_rate)
{
	smp_call_function_any(cpumask_of(cpu),
		twd_recalc_prescaler_all_cpu, &cpu_rate, 1);
}

static int twd_cpufreq_notifier(struct notifier_block *nb,
				  unsigned long val, void *data)
{
	struct cpufreq_freqs *freq = data;

	/*
	 * FIXME: If we need to calibrate before freq change. Currently
	 * only after CPU freq change is done, timer clock is recalibrated
	 */
	switch (val) {
	case CPUFREQ_PRECHANGE:
		break;
	case CPUFREQ_POSTCHANGE:
	case CPUFREQ_RESUMECHANGE:
		if (!(freq->old == freq->new))
			twd_recalc_prescaler(freq->cpu, freq->new * 1000);
		break;
	default:
		break;
	}

	return 0;
}

static struct notifier_block twd_cpufreq_notifier_block = {
	.notifier_call  = twd_cpufreq_notifier
};

static int __init twd_init_cpufreq(void)
{
	if (!twd_timer_rate)
		return 0;

	if (cpufreq_register_notifier(&twd_cpufreq_notifier_block,
				      CPUFREQ_TRANSITION_NOTIFIER))
		pr_err("smp_twd: Failed to setup cpufreq notifier\n");

	return 0;
}
late_initcall(twd_init_cpufreq);
#endif

void __cpuinit twd_timer_setup_with_clock(struct clock_event_device *clk,
	 unsigned long twd_clock_rate, unsigned long target_rate,
	unsigned int arch_clk_divider)
{
	if (twd_clock_rate)
		twd_timer_rate = twd_clock_rate;

	__twd_timer_setup(clk, target_rate, arch_clk_divider);
}

void __cpuinit twd_timer_setup(struct clock_event_device *clk)
{
	__twd_timer_setup(clk, 0, 0);
}

#ifdef CONFIG_HOTPLUG_CPU
/*
 * take a local timer down
 */
void twd_timer_stop(void)
{
	__raw_writel(0, twd_base + TWD_TIMER_CONTROL);
}
#endif
