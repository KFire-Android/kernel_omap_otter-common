/*
 *  transportshm.h
 *
 *  Shared memory based physical transport for
 *  communication with the remote processor.
 *
 *  This file contains the declarations of types and APIs as part
 *  of interface of the shared memory transport.
 *
 *  Copyright (C) 2008-2009 Texas Instruments, Inc.
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

#ifndef _TRANSPORTSHM_H_
#define _TRANSPORTSHM_H_

/* Standard headers */
#include <linux/types.h>

/* Utilities headers */
#include <linux/list.h>

/* =============================================================================
 *  All success and failure codes for the module
 * =============================================================================
 */
/* Unique module ID. */
#define TRANSPORTSHM_MODULEID		(0x0a7a)

/* =============================================================================
 *  All success and failure codes for the module
 * =============================================================================
 */
/* Error code base for TransportShm. */
#define TRANSPORTSHM_STATUSCODEBASE	(TRANSPORTSHM_MODULEID << 12)

/* Macro to make error code. */
#define TRANSPORTSHM_MAKE_FAILURE(x)	((int)  (0x80000000 \
				+ (TRANSPORTSHM_STATUSCODEBASE \
				+ (x))))

/* Macro to make success code. */
#define TRANSPORTSHM_MAKE_SUCCESS(x)	(TRANSPORTSHM_STATUSCODEBASE + (x))

/* Argument passed to a function is invalid. */
#define TRANSPORTSHM_E_INVALIDARG	TRANSPORTSHM_MAKE_FAILURE(1)

/* Invalid shared address size */
#define TRANSPORTSHM_E_INVALIDSIZE	TRANSPORTSHM_MAKE_FAILURE(2)

/* Module is not initialized. */
#define TRANSPORTSHM_E_INVALIDSTATE	TRANSPORTSHM_MAKE_FAILURE(3)

/* Versions don't match */
#define TRANSPORTSHM_E_BADVERSION	TRANSPORTSHM_MAKE_FAILURE(4)

/* General Failure */
#define TRANSPORTSHM_E_FAIL		TRANSPORTSHM_MAKE_FAILURE(5)

/* Memory allocation failed */
#define TRANSPORTSHM_E_MEMORY		TRANSPORTSHM_MAKE_FAILURE(6)

/* Failure in OS call. */
#define TRANSPORTSHM_E_OSFAILURE	TRANSPORTSHM_MAKE_FAILURE(7)

/* Invalid handle specified. */
#define TRANSPORTSHM_E_HANDLE		TRANSPORTSHM_MAKE_FAILURE(8)

/* The specified operation is not supported. */
#define TRANSPORTSHM_E_NOTSUPPORTED	TRANSPORTSHM_MAKE_FAILURE(9)

/* Operation successful. */
#define TRANSPORTSHM_SUCCESS		TRANSPORTSHM_MAKE_SUCCESS(0)

/* The MESSAGETRANSPORTSHM module has already been setup in this process. */
#define TRANSPORTSHM_S_ALREADYSETUP	TRANSPORTSHM_MAKE_SUCCESS(1)


/* =============================================================================
 * Structures & Enums
 * =============================================================================
 */

/*
 *    Structure defining the reason for error function being called
 */
enum transportshm_reason {
	TRANSPORTSHM_REASON_FAILEDPUT,
	/* Failed to send the message. */
	TRANSPORTSHM_REASON_INTERNALERR,
	/* An internal error occurred in the transport */
	TRANSPORTSHM_REASON_PHYSICALERR,
	/*  An error occurred in the physical link in the transport */
	TRANSPORTSHM_REASON_FAILEDALLOC
	/*  Failed to allocate a message. */
};

/*
 *     transport error callback function.
 *
 *  First parameter: Why the error function is being called.
 *
 *  Second parameter: Handle of transport that had the error. NULL denotes
 *  that it is a system error, not a specific transport.
 *
 *  Third parameter: Pointer to the message. This is only valid for
 *  #TRANSPORTSHM_REASON_FAILEDPUT.
 *
 *  Fourth parameter: Transport specific information. Refer to individual
 *  transports for more details.
 */

/*
 *    Module configuration structure.
 */
struct transportshm_config {
	void (*err_fxn)(enum transportshm_reason reason,
				void *handle,
				void *msg,
				u32 info);
	/* Asynchronous error function for the transport module */
};

/*
 *    Structure defining config parameters for the transport
 *  instances.
 */
struct transportshm_params {
	u32 priority;
	/*<  Priority of messages supported by this transport */
	void *gate;
	/*< Gate used for critical region management of the shared memory */
	void *shared_addr;
	/*<  Address of the shared memory. The creator must supply the shared
	*    memory that this will use for maintain shared state information.
	*/
	u32 notify_event_id;
	/*<  Notify event number to be used by the transport */
};

/*
 *    Structure defining Transport status values
 */
enum transportshm_status {
	transportshm_status_INIT,
	/*< Transport Shm instance has not not completed
	* initialization. */
	transportshm_status_UP,
	/*< Transport Shm instance is up and functional. */
	transportshm_status_DOWN,
	/*< Transport Shm instance is down and not functional. */
	transportshm_status_RESETTING
	/*< Transport Shm instance was up at one point and is in
	* process of resetting.
	*/
};


/* =============================================================================
 *  APIs called by applications
 * =============================================================================
 */
/* Function to get the default configuration for the TransportShm
 * module. */
void transportshm_get_config(struct transportshm_config *cfg);

/* Function to setup the TransportShm module. */
int transportshm_setup(const struct transportshm_config *cfg);

/* Function to destroy the TransportShm module. */
int transportshm_destroy(void);

/* Get the default parameters for the NotifyShmDriver. */
void transportshm_params_init(struct transportshm_params *params);

/* Create an instance of the TransportShm. */
void *transportshm_create(u16 proc_id,
			const struct transportshm_params *params);

/* Delete an instance of the TransportShm. */
int transportshm_delete(void **handle_ptr);

/* Open a created TransportShm instance by address */
int transportshm_open_by_addr(void *shared_addr, void **handle_ptr);

/* Close an opened instance */
int transportshm_close(void **handle_ptr);

/* Get the shared memory requirements for the TransportShm. */
u32 transportshm_shared_mem_req(const struct transportshm_params *params);

/* Set the asynchronous error function for the transport module */
void transportshm_set_err_fxn(void (*err_fxn)(enum transportshm_reason reason,
					      void *handle,
					      void *msg,
					      u32 info));


/* =============================================================================
 *  APIs called internally by TransportShm module.
 * =============================================================================
 */
/* Put msg to remote list */
int transportshm_put(void *handle, void *msg);

/* Control Function */
int transportshm_control(void *handle, u32 cmd, u32 *cmd_arg);

/* Get current status of the TransportShm */
enum transportshm_status transportshm_get_status(void *handle);

#endif /* _TRANSPORTSHM_H_ */
