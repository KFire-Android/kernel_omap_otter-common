/*
 * The MPU local timer source file. In OMAP4, both cortex-a9 cores have
 * own timer in it's MPU domain. These timers will be driving the
 * linux kernel SMP tick framework when active. These timers are not
 * part of the wake up domain.
 *
 * Copyright (C) 2009 Texas Instruments, Inc.
 *
 * Author:
 *      Santosh Shilimkar <santosh.shilimkar@ti.com>
 *
 * This file is based on arm realview smp platform file.
 * Copyright (C) 2002 ARM Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/init.h>
#include <linux/smp.h>
#include <linux/clockchips.h>
#include <linux/clk.h>

#include <asm/irq.h>
#include <asm/smp_twd.h>
#include <asm/localtimer.h>

#include "clock.h"

/*
 * Setup the local clock events for a CPU.
 */
void __cpuinit local_timer_setup(struct clock_event_device *evt)
{
#ifdef CONFIG_CPU_FREQ
	unsigned long timer_rate;

	timer_rate = (mpu_timer_rate >> 1);

	/*
	 * Select the lowest timer target clock freq so that
	 * with pre-scaler, constant timer freq can be maintained
	 * for all OPPs
	 * Timer target_freq = 50 MHz
	 */
	evt->irq = OMAP44XX_IRQ_LOCALTIMER;
	twd_timer_setup_with_clock(evt, timer_rate, 50000000, 2);
#else
	evt->irq = OMAP44XX_IRQ_LOCALTIMER;
	twd_timer_setup(evt);
#endif
}

