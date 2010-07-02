/*
 * OMAP OPP Interface
 *
 * Copyright (C) 2009-2010 Texas Instruments Incorporated.
 *	Nishanth Menon
 *	Romit Dasgupta <romit@ti.com>
 * Copyright (C) 2009 Deep Root Systems, LLC.
 *	Kevin Hilman
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#ifndef __ASM_ARM_OMAP_OPP_H
#define __ASM_ARM_OMAP_OPP_H

#include <linux/err.h>
#include <linux/cpufreq.h>

#include <plat/common.h>

/**
 * struct omap_opp_def - OMAP OPP Definition
 * @enabled:	True/false - is this OPP enabled/disabled by default
 * @freq:	Frequency in hertz corresponding to this OPP
 * @u_volt:	Nominal voltage in microvolts corresponding to this OPP
 *
 * OMAP SOCs have a standard set of tuples consisting of frequency and voltage
 * pairs that the device will support per voltage domain. This is called
 * Operating Points or OPP. The actual definitions of OMAP Operating Points
 * varies over silicon within the same family of devices. For a specific
 * domain, you can have a set of {frequency, voltage} pairs and this is denoted
 * by an array of omap_opp_def. As the kernel boots and more information is
 * available, a set of these are activated based on the precise nature of
 * device the kernel boots up on. It is interesting to remember that each IP
 * which belongs to a voltage domain may define their own set of OPPs on top
 * of this - but this is handled by the appropriate driver.
 */
struct omap_opp_def {
	char *hwmod_name;

	unsigned long freq;
	unsigned long u_volt;

	bool enabled;
};

/*
 * Initialization wrapper used to define an OPP.
 * To point at the end of a terminator of a list of OPPs,
 * use OMAP_OPP_DEF(0, 0, 0)
 */
#define OMAP_OPP_DEF(_hwmod_name, _enabled, _freq, _uv)	\
{						\
	.hwmod_name	= _hwmod_name,		\
	.enabled	= _enabled,		\
	.freq		= _freq,		\
	.u_volt		= _uv,			\
}

struct omap_opp;

#ifdef CONFIG_PM

unsigned long opp_get_voltage(const struct omap_opp *opp);

unsigned long opp_get_freq(const struct omap_opp *opp);

int opp_get_opp_count(struct device *dev);

struct omap_opp *opp_find_freq_exact(struct device *dev,
				     unsigned long freq, bool enabled);

struct omap_opp *opp_find_freq_floor(struct device *dev, unsigned long *freq);

struct omap_opp *opp_find_freq_ceil(struct device *dev, unsigned long *freq);

struct omap_opp *opp_find_voltage(struct device *dev, unsigned long volt);

int opp_add(const struct omap_opp_def *opp_def);

int opp_enable(struct omap_opp *opp);

int opp_disable(struct omap_opp *opp);

struct omap_opp *__deprecated opp_find_by_opp_id(struct device *dev,
						  u8 opp_id);
u8 __deprecated opp_get_opp_id(struct omap_opp *opp);

void opp_init_cpufreq_table(struct device *dev,
			    struct cpufreq_frequency_table **table);
#else
static inline unsigned long opp_get_voltage(const struct omap_opp *opp)
{
	return 0;
}

static inline unsigned long opp_get_freq(const struct omap_opp *opp)
{
	return 0;
}

static inline int opp_get_opp_count(struct omap_opp *oppl)
{
	return 0;
}

static inline struct omap_opp *opp_find_freq_exact(struct omap_opp *oppl,
						   unsigned long freq,
						   bool enabled)
{
	return ERR_PTR(-EINVAL);
}

static inline struct omap_opp *opp_find_freq_floor(struct omap_opp *oppl,
						   unsigned long *freq)
{
	return ERR_PTR(-EINVAL);
}

static inline struct omap_opp *opp_find_freq_ceil(struct omap_opp *oppl,
						  unsigned long *freq)
{
	return ERR_PTR(-EINVAL);
}

static inline struct omap_opp *opp_find_voltage(struct device *dev,
						unsigned long volt)
{
	return ERR_PTR(-EINVAL);
}

static inline struct omap_opp *opp_add(struct omap_opp *oppl,
				       const struct omap_opp_def *opp_def)
{
	return ERR_PTR(-EINVAL);
}

static inline int opp_enable(struct omap_opp *opp)
{
	return 0;
}

static inline int opp_disable(struct omap_opp *opp)
{
	return 0;
}

static inline struct omap_opp *__deprecated
opp_find_by_opp_id(struct omap_opp *opps, u8 opp_id)
{
	return ERR_PTR(-EINVAL);
}

static inline u8 __deprecated opp_get_opp_id(struct omap_opp *opp)
{
	return 0;
}

static inline
void opp_init_cpufreq_table(struct omap_opp *opps,
			    struct cpufreq_frequency_table **table)
{
}

#endif		/* CONFIG_PM */
#endif		/* __ASM_ARM_OMAP_OPP_H */
