/*
 * Remote Processor - omap-specific bits
 *
 * Copyright (C) 2011 Texas Instruments, Inc.
 * Copyright (C) 2011 Google, Inc.
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

#ifndef _PLAT_REMOTEPROC_H
#define _PLAT_REMOTEPROC_H

struct rproc_ops;
struct platform_device;

/* struct omap_rproc_timers_info - timers for the omap rproc
 *
 * @id: timer id to use by the remoteproc
 * @odt: timer pointer
 */
struct omap_rproc_timers_info {
	int id;
	struct omap_dm_timer *odt;
};

/*
 * struct omap_rproc_pdata - omap remoteproc's platform data
 * @name: the remoteproc's name
 * @oh_name: omap hwmod device
 * @oh_name_opt: optional, secondary omap hwmod device
 * @firmware: name of firmware file to load
 * @mbox_name: name of omap mailbox device to use with this rproc
 * @ops: start/stop rproc handlers
 * @device_enable: omap-specific handler for enabling a device
 * @device_shutdown: omap-specific handler for shutting down a device
 * @boot_reg: physical address of the control register for storing boot address
 * @omap_rproc_timers_info: timer(s) info rproc needs
 */
struct omap_rproc_pdata {
	const char *name;
	const char *oh_name;
	const char *oh_name_opt;
	const char *firmware;
	const char *mbox_name;
	const struct rproc_ops *ops;
	struct omap_rproc_timers_info *timers;
	int (*device_enable) (struct platform_device *pdev);
	int (*device_shutdown) (struct platform_device *pdev);
	u32 boot_reg;
	u8 timers_cnt;
};

#if defined(CONFIG_OMAP_REMOTEPROC) || defined(CONFIG_OMAP_REMOTEPROC_MODULE)

void __init omap_rproc_reserve_cma(int);

#else

void __init omap_rproc_reserve_cma(int)
{
}

#endif

/* Defines to identify CMA memory size and location based on platform type */
#define RPROC_CMA_OMAP4		1
#define RPROC_CMA_OMAP5		2

#endif /* _PLAT_REMOTEPROC_H */
