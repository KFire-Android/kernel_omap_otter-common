/*
 *  messageq.h
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

#ifndef _MESSAGEQ_H_
#define _MESSAGEQ_H_

/* Standard headers */
#include <linux/types.h>

/* Utilities headers */
#include <linux/list.h>

/* Syslink headers */
#include <listmp.h>

/*!
 *  @def	MESSAGEQ_MODULEID
 *  @brief	Unique module ID.
 */
#define MESSAGEQ_MODULEID			(0xded2)


/* =============================================================================
 *  All success and failure codes for the module
 * =============================================================================
 */

/*!
 *  @def    MESSAGEQ_S_BUSY
 *  @brief  The resource is still in use
 */
#define MESSAGEQ_S_BUSY				2

/*!
 *  @def    MESSAGEQ_S_ALREADYSETUP
 *  @brief  The module has been already setup
 */
#define MESSAGEQ_S_ALREADYSETUP			1

/*!
 *  @def    MESSAGEQ_S_SUCCESS
 *  @brief  Operation is successful.
 */
#define MESSAGEQ_S_SUCCESS			0

/*!
 *  @def    MESSAGEQ_E_FAIL
 *  @brief  Operation is not successful.
 */
#define MESSAGEQ_E_FAIL				-1

/*!
 *  @def    MESSAGEQ_E_INVALIDARG
 *  @brief  There is an invalid argument.
 */
#define MESSAGEQ_E_INVALIDARG			-2

/*!
 *  @def    MESSAGEQ_E_MEMORY
 *  @brief  Operation resulted in memory failure.
 */
#define MESSAGEQ_E_MEMORY			-3

/*!
 *  @def    MESSAGEQ_E_ALREADYEXISTS
 *  @brief  The specified entity already exists.
 */
#define MESSAGEQ_E_ALREADYEXISTS		-4

/*!
 *  @def    MESSAGEQ_E_NOTFOUND
 *  @brief  Unable to find the specified entity.
 */
#define MESSAGEQ_E_NOTFOUND			-5

/*!
 *  @def    MESSAGEQ_E_TIMEOUT
 *  @brief  Operation timed out.
 */
#define MESSAGEQ_E_TIMEOUT			-6

/*!
 *  @def    MESSAGEQ_E_INVALIDSTATE
 *  @brief  Module is not initialized.
 */
#define MESSAGEQ_E_INVALIDSTATE			-7

/*!
 *  @def    MESSAGEQ_E_OSFAILURE
 *  @brief  A failure occurred in an OS-specific call
 */
#define MESSAGEQ_E_OSFAILURE			-8

/*!
 *  @def    MESSAGEQ_E_RESOURCE
 *  @brief  Specified resource is not available
 */
#define MESSAGEQ_E_RESOURCE			-9

/*!
 *  @def    MESSAGEQ_E_RESTART
 *  @brief  Operation was interrupted. Please restart the operation
 */
#define MESSAGEQ_E_RESTART			-10

/*!
 *  @def    MESSAGEQ_E_INVALIDMSG
 *  @brief  Operation is successful.
 */
#define MESSAGEQ_E_INVALIDMSG			-11

/*!
 *  @def    MESSAGEQ_E_NOTOWNER
 *  @brief  Not the owner
 */
#define MESSAGEQ_E_NOTOWNER			-12

/*!
 *  @def    MESSAGEQ_E_REMOTEACTIVE
 *  @brief  Operation is successful.
 */
#define MESSAGEQ_E_REMOTEACTIVE			-13

/*!
 *  @def    MESSAGEQ_E_INVALIDHEAPID
 *  @brief  Operation is successful.
 */
#define MESSAGEQ_E_INVALIDHEAPID		-14

/*!
 *  @def    MESSAGEQ_E_INVALIDPROCID
 *  @brief  Operation is successful.
 */
#define MESSAGEQ_E_INVALIDPROCID		-15

/*!
 *  @def    MESSAGEQ_E_MAXREACHED
 *  @brief  Operation is successful.
 */
#define MESSAGEQ_E_MAXREACHED			-16

/*!
 *  @def    MESSAGEQ_E_UNREGISTEREDHEAPID
 *  @brief  Operation is successful.
 */
#define MESSAGEQ_E_UNREGISTEREDHEAPID		-17

/*!
 *  @def    MESSAGEQ_E_CANNOTFREESTATICMSG
 *  @brief  Operation is successful.
 */
#define MESSAGEQ_E_CANNOTFREESTATICMSG		-18


/* =============================================================================
 * Macros and types
 * =============================================================================
 */
/*!
 *	@brief	Mask to extract version setting
 */
#define MESSAGEQ_HEADERVERSION			0x2000u

/*! Mask to extract Trace setting */
#define MESSAGEQ_TRACEMASK			(uint) 0x1000

/*! Shift for Trace setting */
#define MESSAGEQ_TRACESHIFT			(uint) 12

/*!
 *	@brief	Mask to extract priority setting
 */
#define MESSAGEQ_PRIORITYMASK			0x3u

/*!
 *  Used as the timeout value to specify wait forever
 */
#define MESSAGEQ_FOREVER			(~((u32) 0))

/*!
 *  Invalid message id
 */
#define MESSAGEQ_INVALIDMSGID			0xFFFF

/*!
 * Invalid message queue
 */
#define MESSAGEQ_INVALIDMESSAGEQ		0xFFFF

/*!
 * Indicates that if maximum number of message queues are already created,
 * should allow growth to create additional Message Queue.
 */
#define MESSAGEQ_ALLOWGROWTH			(~((u32) 0))

/*!
 * Number of types of priority queues for each transport
 */
#define MESSAGEQ_NUM_PRIORITY_QUEUES		2


/* =============================================================================
 * Structures & Enums
 * =============================================================================
 */
/*!
 * Message priority
 */
enum messageq_priority {
	MESSAGEQ_NORMALPRI = 0,
	/*!< Normal priority message */
	MESSAGEQ_HIGHPRI = 1,
	/*!< High priority message */
	MESSAGEQ_RESERVEDPRI = 2,
	/*!< Reserved value for message priority */
	MESSAGEQ_URGENTPRI = 3
	/*!< Urgent priority message */
};

/*! Structure which defines the first field in every message */
struct msgheader {
	u32 reserved0;
	/*!< Reserved field */
	u32 reserved1;
	/*!< Reserved field */
	u32 msg_size;
	/*!< Size of the message (including header) */
	u16 flags;
	/*!< Flags */
	u16 msg_id;
	/*!< Maximum length for Message queue names */
	u16 dst_id;
	/*!< Maximum length for Message queue names */
	u16 dst_proc;
	/*!< Maximum length for Message queue names */
	u16 reply_id;
	/*!< Maximum length for Message queue names */
	u16 reply_proc;
	/*!< Maximum length for Message queue names */
	u16 src_proc;
	/*!< Maximum length for Message queue names */
	u16 heap_id;
	/*!< Maximum length for Message queue names */
	u16 seq_num;
	/*!< sequence number */
	u32 reserved;
	/*!< Reserved field */
};

/*! Structure which defines the first field in every message */
#define messageq_msg	struct msgheader *
/*typedef struct msgheader *messageq_msg;*/


/*!
 *  @brief	Structure defining config parameters for the MessageQ Buf module.
 */
struct messageq_config {
	bool trace_flag;
	/*!< Trace Flag
	*  This flag allows the configuration of the default module trace
	*  settings.
	*/

	u16 num_heaps;
	/*!< Number of heapIds in the system
	*  This allows MessageQ to pre-allocate the heaps table.
	*  The heaps table is used when registering heaps.
	*  The default is 1 since generally all systems need at least one heap.
	*  There is no default heap, so unless the system is only using
	*  staticMsgInit, the application must register a heap.
	*/

	u32 max_runtime_entries;
	/*!
	*  Maximum number of MessageQs that can be dynamically created
	*/

	u32 max_name_len;
	/*!< Maximum length for Message queue names */
};

struct messageq_params {
	void *synchronizer;
	/*!< Synchronizer instance used to signal IO completion
	*
	* The synchronizer is used in the #MessageQ_put and #MessageQ_get calls.
	* The synchronizer signal is called as part of the #MessageQ_put call.
	* The synchronizer waits in the #MessageQ_get if there are no messages
	* present.
	*/
};

/* =============================================================================
 *  APIs
 * =============================================================================
 */
/* Functions to get the configuration for messageq setup */
void messageq_get_config(struct messageq_config *cfg);

/* Function to setup the MessageQ module. */
int messageq_setup(const struct messageq_config *cfg);

/* Function to destroy the MessageQ module. */
int messageq_destroy(void);

/* Returns the amount of shared memory used by one transport instance.
 *
 *  The MessageQ module itself does not use any shared memory but the
 *  underlying transport may use some shared memory.
 */
uint messageq_shared_mem_req(void *shared_addr);

/* Calls the SetupProxy function to setup the MessageQ transports. */
int messageq_attach(u16 remote_proc_id, void *shared_addr);

/* Calls the SetupProxy function to detach the MessageQ transports. */
int messageq_detach(u16 remote_proc_id);

/* Initialize this config-params structure with supplier-specified
 * defaults before instance creation.
 */
void messageq_params_init(struct messageq_params *params);

/* Create a message queue */
void *messageq_create(char *name, const struct messageq_params *params);

/* Deletes a instance of MessageQ module. */
int messageq_delete(void **messageq_handleptr);

/* Open a message queue */
int messageq_open(char *name, u32 *queue_id);

/* Close an opened message queue handle */
int messageq_close(u32 *queue_id);

/* Allocates a message from the heap */
messageq_msg messageq_alloc(u16 heapId, u32 size);

/* Frees a message back to the heap */
int messageq_free(messageq_msg msg);

/* Initializes a message not obtained from MessageQ_alloc */
void messageq_static_msg_init(messageq_msg msg, u32 size);

/* Place a message onto a message queue */
int messageq_put(u32 queueId, messageq_msg msg);

/* Gets a message for a message queue and blocks if the queue is empty */
int messageq_get(void *messageq_handle, messageq_msg *msg, u32 timeout);

/* Register a heap with MessageQ */
int messageq_register_heap(void *heap_handle, u16 heap_id);

/* Unregister a heap with MessageQ */
int messageq_unregister_heap(u16 heapId);

/* Returns the number of messages in a message queue */
int messageq_count(void *messageq_handle);

/* Get the proc Id of the message. */
u16 messageq_get_proc_id(void *messageq_handle);

/* Get the queue Id of the message. */
u32 messageq_get_queue_id(void *messageq_handle);

/* Set the destination queue of the message. */
void messageq_set_reply_queue(void *messageq_handle, messageq_msg msg);

/* Set the tracing of a message */
void messageq_set_msg_trace(messageq_msg msg, bool trace_flag);

/*
 *  Functions to set Message properties
 */
/*!
 *  @brief   Returns the MessageQ_Queue handle of the destination
 *           message queue for the specified message.
 */
u32 messageq_get_dst_queue(messageq_msg msg);

/*!
 *  @brief   Returns the message ID of the specified message.
 */
u16 messageq_get_msg_id(messageq_msg msg);

/*!
 *  @brief   Returns the size of the specified message.
 */
u32 messageq_get_msg_size(messageq_msg msg);

/*!
 *  @brief	Gets the message priority of a message
 */
u32 messageq_get_msg_pri(messageq_msg msg);

/*!
 *  @brief   Returns the MessageQ_Queue handle of the destination
 *           message queue for the specified message.
 */
u32 messageq_get_reply_queue(messageq_msg msg);

/*!
 *  @brief   Sets the message ID in the specified message.
 */
void messageq_set_msg_id(messageq_msg msg, u16 msg_id);
/*!
 *  @brief   Sets the message priority in the specified message.
 */
void messageq_set_msg_pri(messageq_msg msg, u32 priority);

/* =============================================================================
 *  APIs called internally by MessageQ transports
 * =============================================================================
 */
/* Register a transport with MessageQ */
int messageq_register_transport(void *imessageq_transport_handle,
					u16 proc_id, u32 priority);

/* Unregister a transport with MessageQ */
void messageq_unregister_transport(u16 proc_id, u32 priority);


#endif /* _MESSAGEQ_H_ */
