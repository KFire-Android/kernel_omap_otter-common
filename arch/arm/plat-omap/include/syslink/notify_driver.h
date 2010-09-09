/*
 * notify_driver.h
 *
 * Notify driver support for OMAP Processors.
 *
 * Copyright (C) 2008-2009 Texas Instruments, Inc.
 *
 * This package is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * THIS PACKAGE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */


#if !defined _NOTIFY_DRIVER_H_
#define _NOTIFY_DRIVER_H_


/* Module includes */
#include <syslink/notify_driverdefs.h>
#include <syslink/_notify.h>


/* Function to register notify driver */
int notify_register_driver(u16 remote_proc_id,
			u16 line_id,
			struct notify_driver_fxn_table *fxn_table,
			struct notify_driver_object **driver_handle);

/* Function to unregister notify driver */
int notify_unregister_driver(struct notify_driver_object *drv_handle);

/* Function to set the driver handle */
int notify_set_driver_handle(u16 remote_proc_id, u16 line_id,
				struct notify_object *handle);

/* Function to find the driver in the list of drivers */
struct notify_driver_object *notify_get_driver_handle(u16 remote_proc_id,
							u16 line_id);


#endif  /* !defined (_NOTIFY_DRIVER_H_) */
