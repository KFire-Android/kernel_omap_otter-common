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
 * struct omap_rprm_ops - operations exported for this module
 *			(only constraints at the comment)
 * @set_min_bus_tput:		set a throughput constraint to the bus there
 *				@tdev is connected
 * @set_max_dev_wakeup_lat:	set a latency constraint to @tdev
 * @device_scale:		scale @tdev, it can be used to set a frequency
 *				constraint in @tdev
 */
struct omap_rprm_ops {
	int (*set_min_bus_tput)(struct device *rdev, struct device *tdev,
			unsigned long val);
	int (*set_max_dev_wakeup_lat)(struct device *rdev, struct device *tdev,
			unsigned long val);
	int (*device_scale)(struct device *rdev, struct device *tdev,
			unsigned long val);
};

/*
 * struct omap_rprm_pdata - omap resmgr platform data
 * @ops:	ops exported by this module (constraints)
 */
struct omap_rprm_pdata {
	struct omap_rprm_ops *ops;
};

#endif /* _PLAT_RPMSG_RESMGR_H */
