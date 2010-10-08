/*
 * notifydefs.h
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


#if !defined(_NOTIFYDEFS_H_)
#define _NOTIFYDEFS_H_

/* Linux headers */
#include <linux/list.h>

/* Osal And Utils  headers */
#include <syslink/atomic_linux.h>

/* Module headers */
#include <syslink/notify.h>
#include <syslink/_notify.h>
#include <syslink/notify_driverdefs.h>


/*Macro to make a correct module magic number with ref Count */
#define NOTIFY_MAKE_MAGICSTAMP(x)	((NOTIFY_MODULEID << 12u) | (x))

/* Maximum number of Notify drivers supported. */
#define NOTIFY_MAX_DRIVERS		4u

/* Mask to check for system key. */
#define NOTIFY_SYSTEMKEY_MASK		((u16)0xFFFF0000)


/* Defines the Event callback information instance */
struct notify_event_callback {
	notify_fn_notify_cbck fn_notify_cbck;
	/* Callback function pointer */
	uint *cbck_arg;
	/* Argument associated with callback function */
};

/* Defines the Notify state object, which contains all the module
 * specific information. */
struct notify_module_object {
	atomic_t ref_count;
	/* Reference count */
	struct notify_config cfg;
	/* Notify configuration structure */
	struct notify_config def_cfg;
	/* Default module configuration */
	struct mutex *gate_handle;
	/* Handle of gate to be used for local thread safety */
	struct notify_driver_object
		drivers[NOTIFY_MAX_DRIVERS][NOTIFY_MAX_INTLINES];
	/* Array of configured drivers. */
	u32 local_enable_mask;
	/* This is used for local/loopback events. Default to enabled (-1) */
	bool start_complete;
	/* TRUE if start() was called */
	bool is_setup;
	/* Indicates whether the Notify module is setup. */
	struct notify_object *local_notify_handle;
	/* Handle to Notify object for local notifications. */
};

/* Defines the Notify instance object. */
struct notify_object {
	uint nesting;
	/* Disable/restore nesting */
	void *driver_handle;
	/* Handle to device specific driver */
	u16 remote_proc_id;
	/* Remote MultiProc id */
	u16 line_id;
	/* Interrupt line id */
	struct notify_event_callback callbacks[NOTIFY_MAXEVENTS];
	/* List of event callbacks registered */
	struct list_head event_list[NOTIFY_MAXEVENTS];
	/* List of event listeners registered */
	struct mutex lock;
	/* Lock for event_list */
};


#endif  /* !defined (_NOTIFYDEFS_H_) */
