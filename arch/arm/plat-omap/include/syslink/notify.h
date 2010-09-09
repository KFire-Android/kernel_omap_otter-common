/*
 * notify.h
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


#if !defined(_NOTIFY_H_)
#define _NOTIFY_H_

/* The resource is still in use */
#define NOTIFY_S_BUSY			2

/* Module already set up */
#define NOTIFY_S_ALREADYSETUP		1

/* Operation is successful. */
#define NOTIFY_S_SUCCESS		0

/* Generic failure */
#define NOTIFY_E_FAIL			-1

/* Argument passed to function is invalid.. */
#define NOTIFY_E_INVALIDARG		-2

/* Operation resulted in memory failure. */
#define NOTIFY_E_MEMORY			-3

/* The specified entity already exists. */
#define NOTIFY_E_ALREADYEXISTS		-4

/* Unable to find the specified entity. */
#define NOTIFY_E_NOTFOUND		-5

/* Operation timed out. */
#define NOTIFY_E_TIMEOUT		-6

/* Module is not initialized. */
#define NOTIFY_E_INVALIDSTATE		-7

/* A failure occurred in an OS-specific call */
#define NOTIFY_E_OSFAILURE		-8

/* The module has been already setup */
#define NOTIFY_E_ALREADYSETUP		-9

/* Specified resource is not available */
#define NOTIFY_E_RESOURCE		-10

/* Operation was interrupted. Please restart the operation */
#define NOTIFY_E_RESTART		-11

/* The resource is still in use */
#define NOTIFY_E_BUSY			-12

/* Driver corresponding to the specified eventId is not registered */
#define NOTIFY_E_DRIVERNOTREGISTERED	-13

/* Event not registered */
#define NOTIFY_E_EVTNOTREGISTERED	-14

/* Event is disabled */
#define NOTIFY_E_EVTDISABLED		-15

/* Remote notification is not initialized */
#define NOTIFY_E_NOTINITIALIZED		-16

/* Trying to illegally use a reserved event */
#define NOTIFY_E_EVTRESERVED		-17

/* Macro to make a correct module magic number with refCount */
#define NOTIFY_MAKE_MAGICSTAMP(x) ((NOTIFY_MODULEID << 12u) | (x))

#define REG volatile

/* Maximum number of events supported by the Notify module */
#define NOTIFY_MAXEVENTS		(u16)32

/* Maximum number of IPC interrupt lines per processor. */
#define NOTIFY_MAX_INTLINES		4u

/* This key must be provided as the upper 16 bits of the eventNo when
 * registering for an event, if any reserved event numbers are to be
 * used. */
#define NOTIFY_SYSTEMKEY		0xC1D2


typedef void (*notify_fn_notify_cbck)(u16 proc_id, u16 line_id, u32 event_id,
					uint *arg, u32 payload);

extern struct notify_module_object notify_state;


/* Function to disable Notify module */
u32 notify_disable(u16 procId, u16 line_id);

/* Function to disable particular event */
void notify_disable_event(u16 proc_id, u16 line_id, u32 event_id);

/* Function to enable particular event */
void notify_enable_event(u16 proc_id, u16 line_id, u32 event_id);

/* Function to find out whether notification via interrupt line has been
 * registered. */
bool notify_is_registered(u16 proc_id, u16 line_id);

/* Returns the amount of shared memory used by one Notify instance. */
uint notify_shared_mem_req(u16 proc_id, void *shared_addr);

/* Function to register an event */
int notify_register_event(u16 proc_id, u16 line_id, u32 event_id,
			notify_fn_notify_cbck notify_callback_fxn,
			void *cbck_arg);

/* Function to register an event */
int notify_register_event_single(u16 proc_id, u16 line_id, u32 event_id,
			notify_fn_notify_cbck notify_callback_fxn,
			void *cbck_arg);

/* Function to restore Notify module state */
void notify_restore(u16 proc_id, u16 line_id, u32 key);

/* Function to send an event to other processor */
int notify_send_event(u16 proc_id, u16 line_id, u32 event_id, u32 payload,
			bool wait_clear);

/* Function to unregister an event */
int notify_unregister_event(u16 proc_id, u16 line_id, u32 event_id,
			notify_fn_notify_cbck notify_callback_fxn,
			void *cbck_arg);

/* Function to unregister an event */
int notify_unregister_event_single(u16 proc_id, u16 line_id, u32 event_id);


#endif /* !defined(_NOTIFY_H_) */
