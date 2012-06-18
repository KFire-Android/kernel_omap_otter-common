/*
 * Remote Processor Resource Manager - OMAP specific
 *
 * Copyright (C) 2012 Texas Instruments, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef _PLAT_RPMSG_RESMGR_H
#define _PLAT_RPMSG_RESMGR_H
#include <linux/device.h>

/*
 * struct omap_rprm_regulator - omap resmgr regulator data
 * @name:	name of the regulator
 * @fixed:	true if the voltage is fixed and therefore is not programmable
 */
struct omap_rprm_regulator {
	const char *name;
	bool fixed;
};

/*
 * struct omap_rprm_auxclk - omap resmgr auxclk data
 * @name	name of the auxiliary clock
 * @parents	array of possible parents names for the auxclk
 * @parents_cnt number of possible parents
 */
struct omap_rprm_auxclk {
	const char *name;
	const char * const *parents;
	u32 parents_cnt;
};

/*
 * struct omap_rprm_ops - operations exported for this module
 *			(only constraints at the comment)
 * @set_max_dev_wakeup_lat:	set a latency constraint to @tdev
 * @device_scale:		scale @tdev, it can be used to set a frequency
 *				constraint in @tdev
 * @lookup_regulator:		return the regulator identified by the @reg_id
 * @lookup_auxclk		return the auxclk identified by the @id
 *
 */
struct omap_rprm_ops {
	int (*set_max_dev_wakeup_lat)(struct device *rdev, struct device *tdev,
			unsigned long val);
	int (*device_scale)(struct device *rdev, struct device *tdev,
			unsigned long val);
	struct omap_rprm_regulator *(*lookup_regulator)(u32 reg_id);
	struct omap_rprm_auxclk *(*lookup_auxclk)(u32 id);
};

/*
 * struct omap_rprm_pdata - omap resmgr platform data
 * @iss_opt_clk_name	name of the ISS optional clock name
 * @iss_opt_clk		clk handle reference to the ISS optional clock
 * @ops:		ops exported by this module
 */
struct omap_rprm_pdata {
	const char *iss_opt_clk_name;
	struct clk *iss_opt_clk;
	struct omap_rprm_ops *ops;
};

void __init omap_rprm_regulator_init(struct omap_rprm_regulator *regulators,
			     u32 regulator_cnt);
u32 omap_rprm_get_regulators(struct omap_rprm_regulator **regulators);

#endif /* _PLAT_RPMSG_RESMGR_H */
