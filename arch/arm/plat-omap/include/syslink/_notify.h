/*
 *  _notify.h
 *
 *  The MessageQ module supports the structured sending and receiving of
 *  variable length messages. This module can be used for homogeneous or
 *  heterogeneous multi-processor messaging.
 *
 *  Copyright (C) 2009 Texas Instruments, Inc.
 *
 *  This package is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 *
 *  THIS PACKAGE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 *  IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 *  WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR
 *  PURPOSE.
 */

#if !defined(__NOTIFY_H_)
#define __NOTIFY_H_


/* Module headers */
#include <syslink/notify.h>


/* Module ID for notify. */
#define NOTIFY_MODULEID		((u16) 0x5F84)

/* Mask to check for event ID. */
#define NOTIFY_EVENT_MASK	((u16) 0xFFFF)

#define ISRESERVED(event_id, reserved_event)	\
			(((event_id & NOTIFY_EVENT_MASK) >= reserved_event) || \
			((event_id >> 16) == NOTIFY_SYSTEMKEY))

/* This structure defines attributes for initialization of the notify module. */
struct notify_config {
	u32 num_events;
	/* Number of events to be supported */
	u32 send_event_poll_count;
	/* Poll for specified amount before send_event times out */
	u32 num_lines;
	/* Max. number of interrupt lines between a single pair of processors */
	u32 reserved_events;
	/* Number of reserved events to be supported */
};

/* This structure defines the configuration structure for initialization
 * of the notify object. */
struct notify_params {
	u32 reserved; /* Reserved field */
};


/* Function to get the default configuration for the notify module. */
void notify_get_config(struct notify_config *cfg);

/* Function to setup the notify module */
int notify_setup(struct notify_config *cfg);

/* Function to destroy the notify module */
int notify_destroy(void);

/* Function to create an instance of notify driver */
struct notify_object *notify_create(void *driver_handle, u16 remote_proc_id,
			u16 line_id, const struct notify_params *params);

/* Function to delete an instance of notify driver */
int notify_delete(struct notify_object **handle_ptr);

/* Function to call device specific the notify module setup */
int notify_attach(u16 proc_id, void *shared_addr);

/* Function to destroy the device specific notify module */
int notify_detach(u16 proc_id);

/* Function registered as callback with the notify driver */
void notify_exec(struct notify_object *obj, u32 event_id, u32 payload);


#endif /* !defined(__NOTIFY_H_) */
