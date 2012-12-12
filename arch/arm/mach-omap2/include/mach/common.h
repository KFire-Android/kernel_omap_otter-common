/*
 * Common header to contain shared information with core drivers
 *
 * Copyright (C) 2012 Texas Instruments Incorporated - http://www.ti.com/
 *	Nishanth Menon
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.

 * This program is distributed "as is" WITHOUT ANY WARRANTY of any
 * kind, whether express or implied; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#ifndef __ARCH_ARM_MACH_OMAP2_MACH_COMMON_H_
#define __ARCH_ARM_MACH_OMAP2_MACH_COMMON_H_

#if defined(CONFIG_ARCH_OMAP4) || defined(CONFIG_ARCH_OMAP5)

/* Notifier flags for core DPLL changes */
#define OMAP_CORE_DPLL_PRECHANGE	1
#define OMAP_CORE_DPLL_POSTCHANGE	2
struct omap_dpll_notifier {
	u32 rate;
};
int omap4_core_dpll_register_notifier(struct notifier_block *nb);
int omap4_core_dpll_unregister_notifier(struct notifier_block *nb);

int omap4_prcm_freq_update(void);
#else
static inline int omap4_core_dpll_register_notifier(struct notifier_block *nb)
{
	return -EINVAL;
}
static inline int omap4_core_dpll_unregister_notifier(struct notifier_block *nb)
{
	return -EINVAL;
}
static inline int omap4_prcm_freq_update(void)
{
	return -EINVAL;
}
#endif

struct voltagedomain;
/* Notifier values for voltage changes */
#define OMAP_VOLTAGE_PRECHANGE	1
#define OMAP_VOLTAGE_POSTCHANGE	2

/**
 * struct omap_voltage_notifier - notifier data that is passed along
 * @voltdm:		voltage domain for the notification
 * @target_volt:	what voltage is happening
 * @op_result:		valid only for POSTCHANGE, tells the result of
 *			the operation.
 *
 * This provides notification
 */
struct omap_voltage_notifier {
	struct voltagedomain	*voltdm;
	unsigned long		target_volt;
	int			op_result;
};

int voltdm_register_notifier(struct voltagedomain *voltdm,
			     struct notifier_block *nb);

int voltdm_unregister_notifier(struct voltagedomain *voltdm,
			       struct notifier_block *nb);

struct voltagedomain *voltdm_lookup(const char *name);

#if defined(CONFIG_PM) && \
		(defined(CONFIG_ARCH_OMAP4) || defined(CONFIG_ARCH_OMAP5))
extern void omap_pm_clear_dsp_wake_up(void);
#else
static inline void omap_pm_clear_dsp_wake_up(void) { }
#endif

#endif			/* __ARCH_ARM_MACH_OMAP2_MACH_COMMON_H_ */
