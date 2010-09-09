/*
 * OMAP Device Handler driver
 *
 * Copyright (C) 2010 Texas Instruments Inc.
 *
 * Written by Angela Stegmaier <angelabaker@ti.com>
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

#ifndef DEVH_H
#define DEVH_H

#include <linux/ioctl.h>
#include <linux/cdev.h>
#include <linux/semaphore.h>

#define DEVH_IOC_MAGIC		'P'

#define DEVH_IOCWAITONEVENTS	_IOR(DEVH_IOC_MAGIC, 0, int)

#define DEVH_IOC_MAXNR		(1)

struct omap_devh;

struct omap_devh_ops {
	int (*register_notifiers)(struct omap_devh *devh);
	int (*unregister_notifiers)(struct omap_devh *devh);
};

struct omap_devh_platform_data {
	struct omap_devh_ops *ops;
	char *name;
	int  proc_id;
	struct semaphore sem_handle;
	void *private_data;
};

struct omap_devh {
	struct device *dev;
	struct cdev cdev;
	atomic_t count;
	int state;
	int minor;
	char *name;
};

extern struct omap_devh_platform_data *devh_get_plat_data(void);
extern int devh_get_plat_data_size(void);

#endif /* DEVH_H */
