/*
 * notify_driverdefs.h
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


#if !defined(_NOTIFY_DRIVERDEFS_H_)
#define _NOTIFY_DRIVERDEFS_H_


/* Module headers */
#include <syslink/multiproc.h>
#include <syslink/notify.h>
#include <syslink/_notify.h>


/* Enumerations to indicate types of Driver initialization status */
enum notify_driver_init_status {
	NOTIFY_DRIVERINITSTATUS_NOTDONE = 0,
	/* Driver initialization is not done. */
	NOTIFY_DRIVERINITSTATUS_DONE = 1,
	/* Driver initialization is complete. */
	NOTIFY_DRIVERINITSTATUS_INPROGRESS = 2,
	/* Driver initialization is in progress. */
	NOTIFY_DRIVERINITSTATUS_ENDVALUE = 3
	/* End delimiter indicating start of invalid values for this enum */
};

struct notify_driver_object;

/* This structure defines the function table interface for the Notify
 * driver.
 * This function table interface must be implemented by each Notify
 * driver and registered with the Notify module. */
struct notify_driver_fxn_table {
	int (*register_event)(struct notify_driver_object *handle,
								u32 event_id);
	/* interface function register_event */
	int (*unregister_event)(struct notify_driver_object *handle,
								u32 event_id);
	/* interface function unregister_event */
	int (*send_event)(struct notify_driver_object *handle, u32 event_id,
						u32 payload, bool wait_clear);
	/* interface function send_event */
	u32 (*disable)(struct notify_driver_object *handle);
	/* interface function disable */
	void (*enable)(struct notify_driver_object *handle);
	/* interface function enable */
	void (*disable_event)(struct notify_driver_object *handle,
								u32 event_id);
	/* interface function disable_event */
	void (*enable_event)(struct notify_driver_object *handle, u32 event_id);
	/* interface function enable_event */
};

/* This structure defines the Notify driver object and handle used
 * internally to contain all information required for the Notify driver
 * This object contains all information for the Notify module to be
 * able to identify and interact with the Notify driver. */
struct notify_driver_object {
	enum notify_driver_init_status is_init;
	struct notify_driver_fxn_table fxn_table;
	struct notify_object *notify_handle;
};

#if 0
/*
 *This structure defines information for all processors supported by
 *the Notify driver.
 *An instance of this object is provided for each processor handled by
 *the Notify driver, when registering itself with the Notify module.
 *
 */
struct notify_driver_proc_info {
	u32 max_events;
	u32 reserved_events;
	bool event_priority;
	u32 payload_size;
	u16 proc_id;
};

/*
 * This structure defines the structure for specifying Notify driver
 * attributes to the Notify module.
 * This structure provides information about the Notify driver to the
 * Notify module. The information is used by the Notify module mainly
 * for parameter validation. It may also be used by the Notify module
 * to take appropriate action if required, based on the characteristics
 * of the Notify driver.
 */
struct notify_driver_attrs {
	u32 numProc;
	struct notify_driver_proc_info
				proc_info[MULTIPROC_MAXPROCESSORS];
};

union notify_drv_procevents{
	struct {
		struct notify_shmdrv_attrs  attrs;
		struct notify_shmdrv_ctrl *ctrl_ptr;
	} shm_events;

	struct {
		/*Attributes */
		unsigned long int  num_events;
		unsigned long int  send_event_pollcount;
		/*Control Paramters */
		unsigned long int send_init_status ;
		struct notify_shmdrv_eventreg_mask  reg_mask ;
	} non_shm_events;
};

struct notify_drv_eventlist {
	unsigned long int event_handler_count;
	struct list_head listeners;
};

struct notify_drv_proc_module {
	unsigned long int proc_id;
	struct notify_drv_eventlist *event_list;
	struct notify_shmdrv_eventreg *reg_chart;
	union notify_drv_procevents events_obj;
};
#endif

#endif /* !defined(_NOTIFY_DRIVERDEFS_H_) */
