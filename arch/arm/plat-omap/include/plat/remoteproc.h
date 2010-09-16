/*
 * OMAP Remote Processor driver
 *
 * Copyright (C) 2010 Texas Instruments Inc.
 *
 * Written by Ohad Ben-Cohen <ohad@wizery.com>
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

#ifndef REMOTEPROC_H
#define REMOTEPROC_H

#include <linux/ioctl.h>
#include <linux/cdev.h>

#define RPROC_IOC_MAGIC		'P'

#define RPROC_IOCMONITOR	_IO(RPROC_IOC_MAGIC, 0)
#define RPROC_IOCSTART		_IO(RPROC_IOC_MAGIC, 1)
#define RPROC_IOCSTOP		_IO(RPROC_IOC_MAGIC, 2)
#define RPROC_IOCGETSTATE	_IOR(RPROC_IOC_MAGIC, 3, int)

#define RPROC_IOC_MAXNR		(3)

#ifdef CONFIG_ARCH_OMAP4
#define NR_RPROC_OMAP4_DEVICES 3
#else
#define NR_RPROC_OMAP4_DEVICES 0
#endif

struct omap_rproc;

struct omap_rproc_ops {
	int (*start)(struct device *dev, u32 start_addr);
	int (*stop)(struct device *dev);
	int (*sleep)(struct device *dev);
	int (*wakeup)(struct device *dev);
	int (*get_state)(struct device *dev);
};

struct omap_rproc_clk_t {
	void *clk_handle;
	const char *dev_id;
	const char *con_id;
};

/* RPROC events. */
enum {
	OMAP_RPROC_START,
	OMAP_RPROC_STOP,
};

/* RPROC states. */
enum {
	OMAP_RPROC_STOPPED,
	OMAP_RPROC_RUNNING,
	OMAP_RPROC_HIBERNATING,
};

struct omap_rproc_platform_data {
	struct omap_rproc_ops *ops;
	char *name;
	char *oh_name;
	int timer_id;
};

struct omap_rproc {
	const char *name;
	struct device *dev;
	struct cdev cdev;
	atomic_t count;
	int state;
	int minor;
	struct blocking_notifier_head	notifier;
	struct mutex lock;
	int timer_id;
	struct omap_dm_timer *dmtimer;
};

struct omap_rproc_start_args {
	u32 start_addr;
};

extern int rproc_start(struct omap_rproc *rproc, const void __user *arg);
extern int rproc_stop(struct omap_rproc *rproc);
extern int rproc_sleep(struct omap_rproc *rproc);
extern int rproc_wakeup(struct omap_rproc *rproc);

extern int omap_rproc_register_notifier(struct omap_rproc *rproc,
					struct notifier_block *nb);
extern int omap_rproc_unregister_notifier(struct omap_rproc *rproc,
					struct notifier_block *nb);
extern int omap_rproc_notify_event(struct omap_rproc *obj, int event,
								void *data);

extern struct omap_rproc *omap_rproc_get(const char *name);
extern void omap_rproc_put(struct omap_rproc *obj);

struct omap_rproc_platform_data *omap3_get_rproc_data(void);
int omap3_get_rproc_data_size(void);
struct omap_rproc_platform_data *omap4_get_rproc_data(void);
#endif /* REMOTEPROC_H */
