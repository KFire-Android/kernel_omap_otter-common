/*
 * OMAP IPU_PM driver
 *
 * Copyright (C) 2010 Texas Instruments Inc.
 *
 * Written by Paul Hunt <hunt@ti.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 */

#ifndef IPU_PM_H
#define IPU_PM_H

#include <linux/ioctl.h>
#include <linux/cdev.h>

#include "../../../drivers/dsp/syslink/ipu_pm/ipu_pm.h"

#define IPU_PM_IOC_MAGIC		(0xDB)

/* magic char is irrelevant until trying to upstream
 * except for reducing exposure to misdirected ioctls
 * see Documentation/ioctl/ioctl-number.txt
 */
#define IPU_PM_IOC_SUSPEND	_IOW(IPU_PM_IOC_MAGIC, 0, int)
#define IPU_PM_IOC_RESUME	_IOW(IPU_PM_IOC_MAGIC, 1, int)

#define IPU_PM_IOC_MAXNR		3

#define IPUPM_CAPS_START_BIT		0
#define IPUPM_CAPS_STOP_BIT		1
#define IPUPM_CAPS_PERF_BIT		2
#define IPUPM_CAPS_LAT_BIT		3
#define IPUPM_CAPS_BDW_BIT		4
/* omap_device built elsewhere */
#define IPUPM_CAPS_EXTINIT_BIT		5
#define IPUPM_CAPS_START		(1 << IPUPM_CAPS_START_BIT)
#define IPUPM_CAPS_STOP			(1 << IPUPM_CAPS_STOP_BIT)
#define IPUPM_CAPS_PERF			(1 << IPUPM_CAPS_PERF_BIT)
#define IPUPM_CAPS_LAT			(1 << IPUPM_CAPS_LAT_BIT)
#define IPUPM_CAPS_BDW			(1 << IPUPM_CAPS_BDW_BIT)
#define IPUPM_CAPS_EXTINIT		(1 << IPUPM_CAPS_EXTINIT_BIT)

#define IPU_PM_SELF			100
#define IPU_PM_MPU			101
#define IPU_PM_CORE			102

struct omap_ipupm_mod;

struct omap_ipupm_mod_ops {
	int (*start)(enum res_type ipupm_modnum);
	int (*stop)(enum res_type ipupm_modnum);
};

struct omap_ipupm_mod_platform_data {
	struct platform_device *pdev;
	struct device *dev;
	char *name;
	char *oh_name;
	struct omap_hwmod *oh;
	struct kobject kobj;
	u32 caps;
	struct pm_qos_request_list *qos_request;
	struct omap_ipupm_mod_ops *ops;
};

struct omap_ipupm_mod {
	struct device *dev;
	struct cdev cdev;
	atomic_t count;
	int state;
	int minor;
};

struct ipu_pm_dev {
	/* FIXME: maybe more is needed */
	struct cdev cdev;
};

extern int ipu_pm_first_dev;

extern int ipu_pm_module_start(unsigned rsrc);
extern int ipu_pm_module_stop(unsigned rsrc);
extern int ipu_pm_module_set_rate(unsigned rsrc,
				  unsigned target_rsrc,
				  unsigned rate);
extern int ipu_pm_module_set_latency(unsigned rsrc,
				     unsigned target_rsrc,
				     int latency);
extern int ipu_pm_module_set_bandwidth(unsigned rsrc,
				       unsigned target_rsrc,
				       int bandwidth);

#endif /* IPU_PM_H */
