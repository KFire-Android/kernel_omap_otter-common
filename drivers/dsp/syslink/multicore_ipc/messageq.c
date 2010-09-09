/*
 *  messageq.c
 *
 *  The messageQ module supports the structured sending and receiving of
 *  variable length messages. This module can be used for homogeneous or
 *  heterogeneous multi-processor messaging.
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

/*!
 *  MessageQ provides more sophisticated messaging than other modules. It is
 *  typically used for complex situations such as multi-processor messaging.
 *
 *  The following are key features of the MessageQ module:
 *  -Writers and readers can be relocated to another processor with no
 *   runtime code changes.
 *  -Timeouts are allowed when receiving messages.
 *  -Readers can determine the writer and reply back.
 *  -Receiving a message is deterministic when the timeout is zero.
 *  -Messages can reside on any message queue.
 *  -Supports zero-copy transfers.
 *  -Can send and receive from any type of thread.
 *  -Notification mechanism is specified by application.
 *  -Allows QoS (quality of service) on message buffer pools. For example,
 *   using specific buffer pools for specific message queues.
 *
 *  Messages are sent and received via a message queue. A reader is a thread
 *  that gets (reads) messages from a message queue. A writer is a thread that
 *  puts (writes) a message to a message queue. Each message queue has one
 *  reader and can have many writers. A thread may read from or write to
 *  multiple message queues.
 *
 *  Conceptually, the reader thread owns a message queue. The reader thread
 *  creates a message queue. Writer threads  a created message queues to
 *  get access to them.
 *
 *  Message queues are identified by a system-wide unique name. Internally,
 *  MessageQ uses the NameServer module for managing
 *  these names. The names are used for opening a message queue. Using
 *  names is not required.
 *
 *  Messages must be allocated from the MessageQ module. Once a message is
 *  allocated, it can be sent on any message queue. Once a message is sent, the
 *  writer loses ownership of the message and should not attempt to modify the
 *  message. Once the reader receives the message, it owns the message. It
 *  may either free the message or re-use the message.
 *
 *  Messages in a message queue can be of variable length. The only
 *  requirement is that the first field in the definition of a message must be a
 *  MsgHeader structure. For example:
 *  typedef struct MyMsg {
 *	  messageq_MsgHeader header;
 *	  ...
 *  } MyMsg;
 *
 *  The MessageQ API uses the messageq_MsgHeader internally. Your application
 *  should not modify or directly access the fields in the messageq_MsgHeader.
 *
 *  All messages sent via the MessageQ module must be allocated from a
 *  Heap implementation. The heap can be used for
 *  other memory allocation not related to MessageQ.
 *
 *  An application can use multiple heaps. The purpose of having multiple
 *  heaps is to allow an application to regulate its message usage. For
 *  example, an application can allocate critical messages from one heap of fast
 *  on-chip memory and non-critical messages from another heap of slower
 *  external memory
 *
 *  MessageQ does support the usage of messages that allocated via the
 *  alloc function. Please refer to the static_msg_init
 *  function description for more details.
 *
 *  In a multiple processor system, MessageQ communications to other
 *  processors via MessageQ_transport} instances. There must be one and
 *  only one IMessageQ_transport instance for each processor where communication
 *  is desired.
 *  So on a four processor system, each processor must have three
 *  IMessageQ_transport instance.
 *
 *  The user only needs to create the IMessageQ_transport instances. The
 *  instances are responsible for registering themselves with MessageQ.
 *  This is accomplished via the register_transport function.
 */



/* Standard headers */
#include <linux/types.h>
#include <linux/module.h>

/* Utilities headers */
#include <linux/string.h>
#include <linux/list.h>
#include <linux/slab.h>
#include <linux/semaphore.h>

/* Syslink headers */
#include <syslink/atomic_linux.h>

/* Module level headers */
#include <nameserver.h>
#include <multiproc.h>
#include <transportshm_setup_proxy.h>
#include <heap.h>
#include <messageq.h>
#include <transportshm.h>


/*  Macro to make a correct module magic number with refCount */
#define MESSAGEQ_MAKE_MAGICSTAMP(x) ((MESSAGEQ_MODULEID << 12u) | (x))

/* =============================================================================
 * Globals
 * =============================================================================
 */
/*!
 *  @brief  Name of the reserved NameServer used for MessageQ.
 */
#define MESSAGEQ_NAMESERVER  "MessageQ"

/*! Mask to extract priority setting */
#define MESSAGEQ_TRANSPORTPRIORITYMASK	0x1

/* =============================================================================
 * Structures & Enums
 * =============================================================================
 */
/* structure for MessageQ module state */
struct messageq_module_object {
	atomic_t ref_count;
	/* Reference count */
	void *ns_handle;
	/* Handle to the local NameServer used for storing GP objects */
	struct mutex *gate_handle;
	/* Handle of gate to be used for local thread safety */
	struct messageq_config cfg;
	/* Current config values */
	struct messageq_config default_cfg;
	/* Default config values */
	struct messageq_params default_inst_params;
	/* Default instance creation parameters */
	void *transports[MULTIPROC_MAXPROCESSORS][MESSAGEQ_NUM_PRIORITY_QUEUES];
	/* Transport to be set in messageq_register_transport */
	void **queues; /*messageq_handle *queues;*/
	/* Grow option */
	void **heaps; /*Heap_Handle *heaps; */
	/* Heap to be set in messageq_registerHeap */
	u16 num_queues;
	/* Heap to be set in messageq_registerHeap */
	u16 num_heaps;
	/* Number of Heaps */
	bool can_free_queues;
	/* Grow option */
	u16 seq_num;
	/* sequence number */
};

/* Structure for the Handle for the MessageQ. */
struct messageq_object {
	struct messageq_params params;
	/*! Instance specific creation parameters */
	u32 queue;
	/* Unique id */
	struct list_head normal_list;
	/* Embedded List objects */
	struct list_head high_list;
	/* Embedded List objects */
	void *ns_key;
	/* NameServer key */
	struct semaphore *synchronizer;
	/* Semaphore used for synchronizing message events */
};


static struct messageq_module_object messageq_state = {
				.ns_handle = NULL,
				.gate_handle = NULL,
				.queues = NULL,
				.heaps = NULL,
				.num_queues = 1,
				.num_heaps = 1,
				.can_free_queues = false,
				.default_cfg.trace_flag = false,
				.default_cfg.num_heaps = 1,
				.default_cfg.max_runtime_entries = 32,
				.default_cfg.max_name_len = 32,
				.default_inst_params.synchronizer = NULL
};

/* Pointer to the MessageQ module state */
static struct messageq_module_object *messageq_module = &messageq_state;

/* =============================================================================
 * Constants
 * =============================================================================
 */
/* Used to denote a message that was initialized
 * with the messageq_static_msg_init function. */
#define MESSAGEQ_STATICMSG 0xFFFF


/* =============================================================================
 * Forward declarations of internal functions
 * =============================================================================
 */
/* Grow the MessageQ table */
static u16 _messageq_grow(struct messageq_object *obj);

/* Initializes a message not obtained from MessageQ_alloc */
static void messageq_msg_init(messageq_msg msg);

/* =============================================================================
 * APIS
 * =============================================================================
 */
/*
 * ======== messageq_get_config ========
 *  Purpose:
 *  Function to get the default configuration for the MessageQ
 *  module.
 *
 *  This function can be called by the application to get their
 *  configuration parameter to MessageQ_setup filled in by the
 *  MessageQ module with the default parameters. If the user does
 *  not wish to make any change in the default parameters, this API
 *  is not required to be called.
 *  the listmp_sharedmemory module.
 */
void messageq_get_config(struct messageq_config *cfg)
{
	if (WARN_ON(unlikely(cfg == NULL)))
		goto exit;

	if (likely(atomic_cmpmask_and_lt(&(messageq_module->ref_count),
				MESSAGEQ_MAKE_MAGICSTAMP(0),
				MESSAGEQ_MAKE_MAGICSTAMP(1)) == true)) {
		/* (If setup has not yet been called) */
		memcpy(cfg, &messageq_module->default_cfg,
			sizeof(struct messageq_config));
	} else {
		memcpy(cfg, &messageq_module->cfg,
			sizeof(struct messageq_config));
	}
	return;

exit:
	printk(KERN_ERR "messageq_get_config: Argument of type "
		"(struct messageq_config *) passed is null!\n");
}
EXPORT_SYMBOL(messageq_get_config);

/*
 * ======== messageq_setup ========
 *  Purpose:
 *  Function to setup the MessageQ module.
 *
 *  This function sets up the MessageQ module. This function must
 *  be called before any other instance-level APIs can be invoked.
 *  Module-level configuration needs to be provided to this
 *  function. If the user wishes to change some specific config
 *  parameters, then MessageQ_getConfig can be called to get the
 *  configuration filled with the default values. After this, only
 *  the required configuration values can be changed. If the user
 *  does not wish to make any change in the default parameters, the
 *  application can simply call MessageQ with NULL parameters.
 *  The default parameters would get automatically used.
 */
int messageq_setup(const struct messageq_config *cfg)
{
	int status = 0;
	struct nameserver_params params;
	struct messageq_config tmpcfg;

	/* This sets the ref_count variable is not initialized, upper 16 bits is
	* written with module Id to ensure correctness of refCount variable.
	*/
	atomic_cmpmask_and_set(&messageq_module->ref_count,
				MESSAGEQ_MAKE_MAGICSTAMP(0),
				MESSAGEQ_MAKE_MAGICSTAMP(0));
	if (unlikely(atomic_inc_return(&messageq_module->ref_count)
				!= MESSAGEQ_MAKE_MAGICSTAMP(1))) {
		return 1;
	}

	if (unlikely(cfg == NULL)) {
		messageq_get_config(&tmpcfg);
		cfg = &tmpcfg;
	}

	if (WARN_ON(unlikely(cfg->max_name_len == 0))) {
		status = -EINVAL;
		goto exit;
	}
	if (WARN_ON(unlikely(cfg->max_runtime_entries == 0))) {
		status = -EINVAL;
		goto exit;
	}

	/* User has not provided any gate handle, so create a default
	* handle for protecting list object */
	messageq_module->gate_handle = kmalloc(sizeof(struct mutex),
				GFP_KERNEL);
	if (unlikely(messageq_module->gate_handle == NULL)) {
		/*! @retval MESSAGEQ_E_FAIL Failed to create lock! */
		status = MESSAGEQ_E_FAIL;
		printk(KERN_ERR "messageq_setup: Failed to create a "
			"mutex.\n");
		status = -ENOMEM;
		goto exit;
	}
	mutex_init(messageq_module->gate_handle);

	memcpy(&messageq_module->cfg, (void *) cfg,
		sizeof(struct messageq_config));
	/* Initialize the parameters */
	nameserver_params_init(&params);
	params.max_value_len = sizeof(u32);
	params.max_name_len = cfg->max_name_len;
	params.max_runtime_entries = cfg->max_runtime_entries;

	messageq_module->seq_num = 0;

	/* Create the nameserver for modules */
	messageq_module->ns_handle = nameserver_create(MESSAGEQ_NAMESERVER,
								&params);
	if (unlikely(messageq_module->ns_handle == NULL)) {
		/*! @retval MESSAGEQ_E_FAIL Failed to create the
		 * MessageQ nameserver*/
		status = MESSAGEQ_E_FAIL;
		printk(KERN_ERR "messageq_setup: Failed to create the messageq"
			"nameserver!\n");
		goto exit;
	}

	messageq_module->num_heaps = cfg->num_heaps;
	messageq_module->heaps = kzalloc(sizeof(void *) * \
				messageq_module->num_heaps, GFP_KERNEL);
	if (unlikely(messageq_module->heaps == NULL)) {
		status = -ENOMEM;
		goto exit;
	}

	messageq_module->num_queues = cfg->max_runtime_entries;
	messageq_module->queues = kzalloc(sizeof(struct messageq_object *) * \
				messageq_module->num_queues, GFP_KERNEL);
	if (unlikely(messageq_module->queues == NULL)) {
		status = -ENOMEM;
		goto exit;
	}

	memset(&(messageq_module->transports), 0, (sizeof(void *) * \
				MULTIPROC_MAXPROCESSORS * \
				MESSAGEQ_NUM_PRIORITY_QUEUES));
	return status;

exit:
	if (status < 0) {
		messageq_destroy();
		printk(KERN_ERR "messageq_setup failed! status = 0x%x\n",
			status);
	}
	return status;
}
EXPORT_SYMBOL(messageq_setup);

/* Function to destroy the MessageQ module. */
int messageq_destroy(void)
{
	int status = 0;
	int tmp_status = 0;
	u32 i;

	if (unlikely(atomic_cmpmask_and_lt(&(messageq_module->ref_count),
				MESSAGEQ_MAKE_MAGICSTAMP(0),
				MESSAGEQ_MAKE_MAGICSTAMP(1)) == true)) {
		status = -ENODEV;
		goto exit;
	}

	if (!(atomic_dec_return(&messageq_module->ref_count)
					== MESSAGEQ_MAKE_MAGICSTAMP(0))) {
		status = 1;
		goto exit;
	}

	/* Temporarily increment the refcount */
	atomic_set(&messageq_module->ref_count, MESSAGEQ_MAKE_MAGICSTAMP(1));

	/* Delete any Message Queues that have not been deleted so far. */
	for (i = 0; i < messageq_module->num_queues; i++) {
		if (messageq_module->queues[i] != NULL) {
			tmp_status = \
				messageq_delete(&(messageq_module->queues[i]));
			if (unlikely(tmp_status < 0 && status >= 0)) {
				status = tmp_status;
				printk(KERN_ERR "messageq_destroy: "
					"messageq_delete failed for queue %d",
					i);
			}
		}
	}

	if (likely(messageq_module->ns_handle != NULL)) {
		/* Delete the nameserver for modules */
		tmp_status = nameserver_delete(&messageq_module->ns_handle);
		if (unlikely(tmp_status < 0 && status >= 0)) {
			status = tmp_status;
			printk(KERN_ERR "messageq_destroy: "
				"nameserver_delete failed");
		}
	}

	/* Delete the gate if created internally */
	if (likely(messageq_module->gate_handle != NULL)) {
		kfree(messageq_module->gate_handle);
		messageq_module->gate_handle = NULL;
	}

	memset(&(messageq_module->transports), 0, (sizeof(void *) * \
		MULTIPROC_MAXPROCESSORS * MESSAGEQ_NUM_PRIORITY_QUEUES));
	if (likely(messageq_module->heaps != NULL)) {
		kfree(messageq_module->heaps);
		messageq_module->heaps = NULL;
	}
	if (likely(messageq_module->queues != NULL)) {
		kfree(messageq_module->queues);
		messageq_module->queues = NULL;
	}

	memset(&messageq_module->cfg, 0, sizeof(struct messageq_config));
	messageq_module->num_queues = 0;
	messageq_module->num_heaps = 1;
	messageq_module->can_free_queues = true;
	atomic_set(&messageq_module->ref_count, MESSAGEQ_MAKE_MAGICSTAMP(0));

exit:
	if (status < 0) {
		printk(KERN_ERR "messageq_destroy failed! status = 0x%x\n",
			status);
	}
	return status;
}
EXPORT_SYMBOL(messageq_destroy);

/* Initialize this config-params structure with supplier-specified
 *  defaults before instance creation. */
void messageq_params_init(struct messageq_params *params)
{
	if (unlikely(atomic_cmpmask_and_lt(&(messageq_module->ref_count),
				MESSAGEQ_MAKE_MAGICSTAMP(0),
				MESSAGEQ_MAKE_MAGICSTAMP(1)) == true))
		goto exit;
	if (WARN_ON(unlikely(params == NULL))) {
		printk(KERN_ERR "messageq_params_init failed:Argument of "
			"type(messageq_params *) is NULL!\n");
		goto exit;
	}

	memcpy(params, &(messageq_module->default_inst_params),
		sizeof(struct messageq_params));

exit:
	return;
}
EXPORT_SYMBOL(messageq_params_init);

/* Creates a new instance of MessageQ module. */
void *messageq_create(char *name, const struct messageq_params *params)
{
	int status = 0;
	struct messageq_object *obj = NULL;
	bool found = false;
	u16 count = 0;
	int  i;
	u16 start;
	u16 queueIndex = 0;

	if (unlikely(atomic_cmpmask_and_lt(&(messageq_module->ref_count),
				MESSAGEQ_MAKE_MAGICSTAMP(0),
				MESSAGEQ_MAKE_MAGICSTAMP(1)) == true)) {
		status = -ENODEV;
		goto exit;
	}

	/* Create the generic obj */
	obj = kzalloc(sizeof(struct messageq_object), 0);
	if (unlikely(obj == NULL)) {
		status = -ENOMEM;
		goto exit;
	}

	status = mutex_lock_interruptible(messageq_module->gate_handle);
	if (status)
		goto exit;
	start = 0; /* Statically allocated objects not supported */
	count = messageq_module->num_queues;
	/* Search the dynamic array for any holes */
	for (i = start; i < count ; i++) {
		if (messageq_module->queues[i] == NULL) {
			messageq_module->queues[i] = (void *) obj;
			queueIndex = i;
			found = true;
			break;
		}
	}
	/*
	 *  If no free slot was found:
	 *  - if no growth allowed, raise an error
	 *  - if growth is allowed, grow the array
	 */
	if (unlikely(found == false)) {
		/* Growth is always allowed */
		queueIndex = _messageq_grow(obj);
		if (unlikely(queueIndex == MESSAGEQ_INVALIDMESSAGEQ)) {
			mutex_unlock(messageq_module->gate_handle);
			status = MESSAGEQ_E_FAIL;
			printk(KERN_ERR "messageq_create: Failed to grow the "
				"queue array!");
			goto exit;
		}
	}

	if (params != NULL) {
		/* Populate the params member */
		memcpy((void *) &obj->params, (void *)params,
			sizeof(struct messageq_params));
		if (unlikely(params->synchronizer == NULL))
			obj->synchronizer = \
				kzalloc(sizeof(struct semaphore), GFP_KERNEL);
		else
			obj->synchronizer = params->synchronizer;
	} else {
		/*obj->synchronizer = OsalSemaphore_create(
				OsalSemaphore_Type_Binary
				| OsalSemaphore_IntType_Interruptible);*/
		obj->synchronizer = kzalloc(sizeof(struct semaphore),
						GFP_KERNEL);
	}
	if (unlikely(obj->synchronizer == NULL)) {
		mutex_unlock(messageq_module->gate_handle);
		status = MESSAGEQ_E_FAIL;
		printk(KERN_ERR "messageq_create: Failed to create "
			"synchronizer semaphore!\n");
		goto exit;
	} else {
		sema_init(obj->synchronizer, 0);
	}
	mutex_unlock(messageq_module->gate_handle);

	/* Construct the list object */
	INIT_LIST_HEAD(&obj->normal_list);
	INIT_LIST_HEAD(&obj->high_list);

	/* Update processor information */
	obj->queue = ((u32)(multiproc_self()) << 16) | queueIndex;
	if (likely(name != NULL)) {
		obj->ns_key = nameserver_add_uint32(messageq_module->ns_handle,
						name, obj->queue);
		if (unlikely(obj->ns_key == NULL)) {
			status = MESSAGEQ_E_FAIL;
			printk(KERN_ERR "messageq_create: Failed to add "
				"the messageq name!\n");
		}
	}

exit:
	if (unlikely(status < 0)) {
		messageq_delete((void **)&obj);
		printk(KERN_ERR "messageq_create failed! status = 0x%x\n",
			status);
	}
	return (void *) obj;
}
EXPORT_SYMBOL(messageq_create);

/* Deletes a instance of MessageQ module. */
int messageq_delete(void **msg_handleptr)
{
	int status = 0;
	int tmp_status = 0;
	struct messageq_object *obj = NULL;
	messageq_msg temp_msg;

	if (WARN_ON(unlikely(atomic_cmpmask_and_lt(
				&(messageq_module->ref_count),
				MESSAGEQ_MAKE_MAGICSTAMP(0),
				MESSAGEQ_MAKE_MAGICSTAMP(1)) == true))) {
		status = -ENODEV;
		goto exit;
	}
	if (WARN_ON(msg_handleptr == NULL)) {
		status = -EINVAL;
		goto exit;
	}
	if (WARN_ON(*msg_handleptr == NULL)) {
		status = -EINVAL;
		goto exit;
	}

	obj = (struct messageq_object *) (*msg_handleptr);

	/* Take the local lock */
	status = mutex_lock_interruptible(messageq_module->gate_handle);
	if (status)
		goto exit;

	if (unlikely(obj->ns_key != NULL)) {
		/* remove from the name serve */
		status = nameserver_remove_entry(messageq_module->ns_handle,
			obj->ns_key);
		if (unlikely(status < 0)) {
			printk(KERN_ERR "messageq_delete: nameserver_remove_"
				"entry failed! status = 0x%x", status);
		}
	}

	/* Remove all the messages for the message queue's normal_list queue
	 * and free the list */
	while (true) {
		if (!list_empty(&obj->normal_list)) {
			temp_msg = (messageq_msg) (obj->normal_list.next);
			list_del_init(obj->normal_list.next);
		} else
			break;
		tmp_status = messageq_free(temp_msg);
		if (unlikely((tmp_status < 0) && (status >= 0))) {
			status = tmp_status;
			printk(KERN_ERR "messageq_delete: messageq_free failed"
				" for normal_list!");
		}
	}
	list_del(&obj->normal_list);

	/* Remove all the messages for the message queue's normal_list queue
	 * and free the list */
	while (true) {
		if (!list_empty(&obj->high_list)) {
			temp_msg = (messageq_msg) (obj->high_list.next);
			list_del_init(obj->high_list.next);
		} else
			break;
		tmp_status = messageq_free(temp_msg);
		if (unlikely((tmp_status < 0) && (status >= 0))) {
			status = tmp_status;
			printk(KERN_ERR "messageq_delete: messageq_free failed"
				" for high_list!");
		}
	}
	list_del(&obj->high_list);

	/*if (obj->synchronizer != NULL)
		status = OsalSemaphore_delete(&obj->synchronizer);*/
	if (obj->synchronizer != NULL) {
		kfree(obj->synchronizer);
		obj->synchronizer = NULL;
	}
	/* Clear the MessageQ obj from array. */
	messageq_module->queues[obj->queue & 0xFFFF] = NULL;

	/* Release the local lock */
	mutex_unlock(messageq_module->gate_handle);

	/* Now free the obj */
	kfree(obj);
	*msg_handleptr = NULL;

exit:
	if (status < 0) {
		printk(KERN_ERR "messageq_delete failed! status = 0x%x\n",
			status);;
	}
	return status;
}
EXPORT_SYMBOL(messageq_delete);

/* Opens a created instance of MessageQ module. */
int messageq_open(char *name, u32 *queue_id)
{
	int status = 0;

	if (WARN_ON(unlikely(atomic_cmpmask_and_lt(
				&(messageq_module->ref_count),
				MESSAGEQ_MAKE_MAGICSTAMP(0),
				MESSAGEQ_MAKE_MAGICSTAMP(1)) == true))) {
		status = -ENODEV;
		goto exit;
	}
	if (WARN_ON(unlikely(name == NULL))) {
		status = -EINVAL;
		goto exit;
	}
	if (WARN_ON(unlikely(queue_id == NULL))) {
		status = -EINVAL;
		goto exit;
	}

	/* Initialize return queue ID to invalid. */
	*queue_id = MESSAGEQ_INVALIDMESSAGEQ;
	status = nameserver_get_uint32(messageq_module->ns_handle, name,
					queue_id, NULL);

exit:
	if (status < 0) {
		printk(KERN_ERR "messageq_open failed! status = 0x%x\n",
			status);
	}
	return status;
}
EXPORT_SYMBOL(messageq_open);

/* Closes previously opened/created instance of MessageQ module. */
int messageq_close(u32 *queue_id)
{
	s32 status = 0;

	if (WARN_ON(unlikely(atomic_cmpmask_and_lt(
				&(messageq_module->ref_count),
				MESSAGEQ_MAKE_MAGICSTAMP(0),
				MESSAGEQ_MAKE_MAGICSTAMP(1)) == true))) {
		status = -ENODEV;
		goto exit;
	}
	if (WARN_ON(unlikely(queue_id == NULL))) {
		printk(KERN_ERR "messageq_close: queue_id passed is NULL!\n");
		status = -EINVAL;
		goto exit;
	}

	*queue_id = MESSAGEQ_INVALIDMESSAGEQ;

exit:
	if (status < 0) {
		printk(KERN_ERR "messageq_close failed! status = 0x%x\n",
			status);
	}
	return status;
}
EXPORT_SYMBOL(messageq_close);

/* Retrieve a message */
int messageq_get(void *messageq_handle, messageq_msg *msg,
							u32 timeout)
{
	int status = 0;
	struct messageq_object *obj = (struct messageq_object *)messageq_handle;

	if (WARN_ON(unlikely(atomic_cmpmask_and_lt(
				&(messageq_module->ref_count),
				MESSAGEQ_MAKE_MAGICSTAMP(0),
				MESSAGEQ_MAKE_MAGICSTAMP(1)) == true))) {
		status = -ENODEV;
		goto exit;
	}
	if (WARN_ON(unlikely(msg == NULL))) {
		status = -EINVAL;
		goto exit;
	}
	if (WARN_ON(unlikely(obj == NULL))) {
		status = -EINVAL;
		goto exit;
	}

	/* Keep looping while there is no element in the list */
	/* Take the local lock */
	status = mutex_lock_interruptible(messageq_module->gate_handle);
	if (status)
		goto exit;
	if (!list_empty(&obj->high_list)) {
		*msg = (messageq_msg) (obj->high_list.next);
		list_del_init(obj->high_list.next);
	}
	/* Leave the local lock */
	mutex_unlock(messageq_module->gate_handle);
	while (*msg == NULL) {
		status = mutex_lock_interruptible(messageq_module->gate_handle);
		if (status)
			goto exit;
		if (!list_empty(&obj->normal_list)) {
			*msg = (messageq_msg) (obj->normal_list.next);
			list_del_init(obj->normal_list.next);
		}
		mutex_unlock(messageq_module->gate_handle);

		if (*msg == NULL) {
			/*
			 *  Block until notified.  If pend times-out, no message
			 *  should be returned to the caller
			 */
			/*! @retval NULL timeout has occurred */
			if (obj->synchronizer != NULL) {
				/* TODO: cater to different timeout values */
				/*status = OsalSemaphore_pend(
					obj->synchronizer, timeout); */
				if (timeout == MESSAGEQ_FOREVER) {
					if (down_interruptible
							(obj->synchronizer)) {
						status = -ERESTARTSYS;
					}
				} else {
					status = down_timeout(obj->synchronizer,
						msecs_to_jiffies(timeout));
				}
				if (status < 0) {
					*msg = NULL;
					break;
				}
			}
			status = mutex_lock_interruptible(
					messageq_module->gate_handle);
			if (status)
				goto exit;
			if (!list_empty(&obj->high_list)) {
				*msg = (messageq_msg) (obj->high_list.next);
				list_del_init(obj->high_list.next);
			}
			mutex_unlock(messageq_module->gate_handle);
		}
	}

exit:
	if (unlikely((messageq_module->cfg.trace_flag == true) && \
		((*msg != NULL) && \
		(((*msg)->flags & MESSAGEQ_TRACEMASK) != 0)))) {
		printk(KERN_INFO "messageq_get: *msg = 0x%x seq_num = 0x%x "
			"src_proc = 0x%x obj = 0x%x\n", (uint)(*msg),
			((*msg)->seq_num), ((*msg)->src_proc), (uint)(obj));
	}
	if (status < 0 && status != -ETIME)
		printk(KERN_ERR "messageq_get failed! status = 0x%x\n", status);
	return status;
}
EXPORT_SYMBOL(messageq_get);

/* Count the number of messages in the queue */
int messageq_count(void *messageq_handle)
{
	struct messageq_object *obj = (struct messageq_object *)messageq_handle;
	int count = 0;
	struct list_head *elem;
	int key;
	s32 status = 0;

	if (WARN_ON(unlikely(atomic_cmpmask_and_lt(
				&(messageq_module->ref_count),
				MESSAGEQ_MAKE_MAGICSTAMP(0),
				MESSAGEQ_MAKE_MAGICSTAMP(1)) == true))) {
		status = -ENODEV;
		goto exit;
	}
	if (WARN_ON(obj == NULL)) {
		status = -EINVAL;
		printk(KERN_ERR "messageq_count: obj passed is NULL!\n");
		goto exit;
	}

	key = mutex_lock_interruptible(messageq_module->gate_handle);
	if (key < 0)
		return key;

	list_for_each(elem, &obj->high_list) {
		count++;
	}
	list_for_each(elem, &obj->normal_list) {
		count++;
	}
	mutex_unlock(messageq_module->gate_handle);

exit:
	if (status < 0)
		printk(KERN_ERR "messageq_count failed! status = 0x%x", status);
	return count;
}
EXPORT_SYMBOL(messageq_count);

/* Initialize a static message */
void messageq_static_msg_init(messageq_msg msg, u32 size)
{
	s32 status = 0;

	if (WARN_ON(unlikely(atomic_cmpmask_and_lt(
				&(messageq_module->ref_count),
				MESSAGEQ_MAKE_MAGICSTAMP(0),
				MESSAGEQ_MAKE_MAGICSTAMP(1)) == true))) {
		status = -ENODEV;
		goto exit;
	}
	if (WARN_ON(msg == NULL)) {
		printk(KERN_ERR "messageq_static_msg_init: msg is invalid!\n");
		goto exit;
	}

	/* Fill in the fields of the message */
	messageq_msg_init(msg);
	msg->heap_id = MESSAGEQ_STATICMSG;
	msg->msg_size = size;

	if (unlikely((messageq_module->cfg.trace_flag == true) || \
		(((*msg).flags & MESSAGEQ_TRACEMASK) != 0))) {
		printk(KERN_INFO "messageq_static_msg_init: msg = 0x%x "
			"seq_num = 0x%x src_proc = 0x%x", (uint)(msg),
			(msg)->seq_num, (msg)->src_proc);
	}

exit:
	if (status < 0) {
		printk(KERN_ERR "messageq_static_msg_init failed! "
			"status = 0x%x", status);
	}
	return;
}
EXPORT_SYMBOL(messageq_static_msg_init);

/* Allocate a message and initial the needed fields (note some
 * of the fields in the header at set via other APIs or in the
 * messageq_put function. */
messageq_msg messageq_alloc(u16 heap_id, u32 size)
{
	int status = 0;
	messageq_msg msg = NULL;

	if (WARN_ON(unlikely(atomic_cmpmask_and_lt(
				&(messageq_module->ref_count),
				MESSAGEQ_MAKE_MAGICSTAMP(0),
				MESSAGEQ_MAKE_MAGICSTAMP(1)) == true))) {
		status = -ENODEV;
		goto exit;
	}
	if (WARN_ON(unlikely(heap_id >= messageq_module->num_heaps))) {
		status = -EINVAL;
		goto exit;
	}
	if (WARN_ON(unlikely(messageq_module->heaps[heap_id] == NULL))) {
		status = -EINVAL;
		goto exit;
	}

	/* Allocate the message. No alignment requested */
	msg = sl_heap_alloc(messageq_module->heaps[heap_id], size, 0);
	if (msg == NULL) {
		status = -ENOMEM;
		goto exit;
	}

	/* Fill in the fields of the message */
	messageq_msg_init(msg);
	msg->msg_size = size;
	msg->heap_id = heap_id;

	if (unlikely((messageq_module->cfg.trace_flag == true) || \
		(((*msg).flags & MESSAGEQ_TRACEMASK) != 0))) {
		printk(KERN_INFO "messageq_alloc: msg = 0x%x seq_num = 0x%x "
			"src_proc = 0x%x", (uint)(msg), (msg)->seq_num,
			(msg)->src_proc);
	}

exit:
	if (status < 0)
		printk(KERN_ERR "messageq_alloc failed! status = 0x%x", status);
	return msg;
}
EXPORT_SYMBOL(messageq_alloc);

/* Frees the message. */
int messageq_free(messageq_msg msg)
{
	u32 status = 0;
	void *heap = NULL;

	if (WARN_ON(unlikely(atomic_cmpmask_and_lt(
				&(messageq_module->ref_count),
				MESSAGEQ_MAKE_MAGICSTAMP(0),
				MESSAGEQ_MAKE_MAGICSTAMP(1)) == true))) {
		status = -ENODEV;
		goto exit;
	}
	if (WARN_ON(unlikely(msg == NULL))) {
		status = -EINVAL;
		goto exit;
	}
	if (unlikely(msg->heap_id == MESSAGEQ_STATICMSG)) {
		status = MESSAGEQ_E_CANNOTFREESTATICMSG;
		goto exit;
	}
	if (unlikely(msg->heap_id >= messageq_module->num_heaps)) {
		status = MESSAGEQ_E_INVALIDHEAPID;
		goto exit;
	}
	if (unlikely(messageq_module->heaps[msg->heap_id] == NULL)) {
		status = MESSAGEQ_E_INVALIDHEAPID;
		goto exit;
	}

	if (unlikely((messageq_module->cfg.trace_flag == true) || \
		(((*msg).flags & MESSAGEQ_TRACEMASK) != 0))) {
		printk(KERN_INFO "messageq_free: msg = 0x%x seq_num = 0x%x "
			"src_proc = 0x%x", (uint)(msg), (msg)->seq_num,
			(msg)->src_proc);
	}
	heap = messageq_module->heaps[msg->heap_id];
	sl_heap_free(heap, msg, msg->msg_size);

exit:
	if (status < 0) {
		printk(KERN_ERR "messageq_free failed! status = 0x%x\n",
			status);
	}
	return status;
}
EXPORT_SYMBOL(messageq_free);

/* Put a message in the queue */
int messageq_put(u32 queue_id, messageq_msg msg)
{
	int status = 0;
	u16 dst_proc_id = (u16)(queue_id >> 16);
	struct messageq_object *obj = NULL;
	void *transport = NULL;
	u32 priority;

	if (WARN_ON(unlikely(atomic_cmpmask_and_lt(
				&(messageq_module->ref_count),
				MESSAGEQ_MAKE_MAGICSTAMP(0),
				MESSAGEQ_MAKE_MAGICSTAMP(1)) == true))) {
		status = -ENODEV;
		goto exit;
	}
	if (WARN_ON(unlikely(queue_id == MESSAGEQ_INVALIDMESSAGEQ))) {
		status = -EINVAL;
		goto exit;
	}
	if (WARN_ON(unlikely(msg == NULL))) {
		status = -EINVAL;
		goto exit;
	}

	msg->dst_id = (u16)(queue_id);
	msg->dst_proc = (u16)(queue_id >> 16);
	if (likely(dst_proc_id != multiproc_self())) {
		if (unlikely(dst_proc_id >= multiproc_get_num_processors())) {
			/* Invalid destination processor id */
			status = MESSAGEQ_E_INVALIDPROCID;
			goto exit;
		}

		priority = (u32)((msg->flags) & MESSAGEQ_TRANSPORTPRIORITYMASK);
		/* Call the transport associated with this message queue */
		transport = messageq_module->transports[dst_proc_id][priority];
		if (transport == NULL) {
			/* Try the other transport */
			priority = !priority;
			transport =
			messageq_module->transports[dst_proc_id][priority];
		}

		if (unlikely(transport == NULL)) {
			status = -ENODEV;
			goto exit;
		}
		status = transportshm_put(transport, msg);
		if (unlikely(status < 0))
			goto exit;
	} else {
		/* It is a local MessageQ */
		obj = (struct messageq_object *)
				(messageq_module->queues[(u16)(queue_id)]);
		/* Check for MessageQ Validity. */
		if (obj == NULL) {
			status = MESSAGEQ_E_INVALIDMSG;
			goto exit;
		}
		status = mutex_lock_interruptible(messageq_module->gate_handle);
		if (status < 0)
			goto exit;
		if ((msg->flags & MESSAGEQ_PRIORITYMASK) == \
			MESSAGEQ_URGENTPRI) {
			list_add((struct list_head *) msg, &obj->high_list);
		} else {
			if ((msg->flags & MESSAGEQ_PRIORITYMASK) == \
				MESSAGEQ_NORMALPRI) {
				list_add_tail((struct list_head *) msg,
					&obj->normal_list);
			} else {
				list_add_tail((struct list_head *) msg,
					&obj->high_list);
			}
		}
		mutex_unlock(messageq_module->gate_handle);

		/* Notify the reader. */
		if (obj->synchronizer != NULL) {
			up(obj->synchronizer);
			/*OsalSemaphore_post(obj->synchronizer);*/
		}
	}
	if (unlikely((messageq_module->cfg.trace_flag == true) || \
		(((*msg).flags & MESSAGEQ_TRACEMASK) != 0))) {
		printk(KERN_INFO "messageq_put: msg = 0x%x seq_num = 0x%x "
			"src_proc = 0x%x dst_proc_id = 0x%x\n", (uint)(msg),
			(msg)->seq_num, (msg)->src_proc, (msg)->dst_proc);
	}

exit:
	if (status < 0)
		printk(KERN_ERR "messageq_put failed! status = 0x%x\n", status);
	return status;
}
EXPORT_SYMBOL(messageq_put);

/* Register a heap */
int messageq_register_heap(void *heap_handle, u16 heap_id)
{
	int status = 0;

	if (WARN_ON(unlikely(atomic_cmpmask_and_lt(
				&(messageq_module->ref_count),
				MESSAGEQ_MAKE_MAGICSTAMP(0),
				MESSAGEQ_MAKE_MAGICSTAMP(1)) == true))) {
		status = -ENODEV;
		goto exit;
	}
	if (WARN_ON(unlikely(heap_handle == NULL))) {
		/*! @retval -EINVAL Invalid heap_id */
		status = -EINVAL;
		goto exit;
	}
	/* Make sure the heap_id is valid */
	if (WARN_ON(unlikely(heap_id >= messageq_module->num_heaps))) {
		/*! @retval -EINVAL Invalid heap_id */
		status = -EINVAL;
		goto exit;
	}

	status = mutex_lock_interruptible(messageq_module->gate_handle);
	if (status)
		goto exit;
	if (messageq_module->heaps[heap_id] == NULL)
		messageq_module->heaps[heap_id] = heap_handle;
	else {
		/*! @retval MESSAGEQ_E_ALREADYEXISTS Specified heap is
		already registered. */
		status = MESSAGEQ_E_ALREADYEXISTS;
	}
	mutex_unlock(messageq_module->gate_handle);

exit:
	if (status < 0) {
		printk(KERN_ERR "messageq_register_heap failed! "
			"status = 0x%x\n", status);
	}
	return status;
}
EXPORT_SYMBOL(messageq_register_heap);

/* Unregister a heap */
int messageq_unregister_heap(u16 heap_id)
{
	int status = 0;

	if (WARN_ON(unlikely(atomic_cmpmask_and_lt(
				&(messageq_module->ref_count),
				MESSAGEQ_MAKE_MAGICSTAMP(0),
				MESSAGEQ_MAKE_MAGICSTAMP(1)) == true))) {
		status = -ENODEV;
		goto exit;
	}
	/* Make sure the heap_id is valid */
	if (WARN_ON(unlikely(heap_id >= messageq_module->num_heaps))) {
		/*! @retval -EINVAL Invalid heap_id */
		status = -EINVAL;
		goto exit;
	}

	status = mutex_lock_interruptible(messageq_module->gate_handle);
	if (status)
		goto exit;
	if (messageq_module->heaps != NULL)
		messageq_module->heaps[heap_id] = NULL;
	mutex_unlock(messageq_module->gate_handle);

exit:
	if (status < 0) {
		printk(KERN_ERR "messageq_unregister_heap failed! "
			"status = 0x%x\n", status);
	}
	return status;
}
EXPORT_SYMBOL(messageq_unregister_heap);

/* Register a transport */
int messageq_register_transport(void *messageq_transportshm_handle,
				 u16 proc_id, u32 priority)
{
	int  status = 0;

	if (WARN_ON(unlikely(atomic_cmpmask_and_lt(
				&(messageq_module->ref_count),
				MESSAGEQ_MAKE_MAGICSTAMP(0),
				MESSAGEQ_MAKE_MAGICSTAMP(1)) == true))) {
		status = -ENODEV;
		goto exit;
	}
	if (WARN_ON(unlikely(messageq_transportshm_handle == NULL))) {
		status = -EINVAL;
		goto exit;
	}
	if (WARN_ON(unlikely(proc_id >= multiproc_get_num_processors()))) {
		status = -EINVAL;
		goto exit;
	}

	status = mutex_lock_interruptible(messageq_module->gate_handle);
	if (status)
		goto exit;
	if (messageq_module->transports[proc_id][priority] == NULL) {
		messageq_module->transports[proc_id][priority] = \
			messageq_transportshm_handle;
	} else {
		/*! @retval MESSAGEQ_E_ALREADYEXISTS Specified transport is
		already registered. */
		status = MESSAGEQ_E_ALREADYEXISTS;
	}
	mutex_unlock(messageq_module->gate_handle);

exit:
	if (status < 0) {
		printk(KERN_ERR "messageq_register_transport failed! "
			"status = 0x%x\n", status);
	}
	return status;
}
EXPORT_SYMBOL(messageq_register_transport);

/* Unregister a transport */
void messageq_unregister_transport(u16 proc_id, u32 priority)
{
	int  status = 0;

	if (WARN_ON(unlikely(atomic_cmpmask_and_lt(
				&(messageq_module->ref_count),
				MESSAGEQ_MAKE_MAGICSTAMP(0),
				MESSAGEQ_MAKE_MAGICSTAMP(1)) == true))) {
		status = -ENODEV;
		goto exit;
	}
	if (WARN_ON(proc_id >= multiproc_get_num_processors())) {
		/*! @retval MESSAGEQ_E_PROCIDINVALID Invalid proc_id */
		status = -EINVAL;
		goto exit;
	}

	status = mutex_lock_interruptible(messageq_module->gate_handle);
	if (status)
		goto exit;
	if (messageq_module->transports[proc_id][priority] != NULL)
		messageq_module->transports[proc_id][priority] = NULL;
	mutex_unlock(messageq_module->gate_handle);

exit:
	if (status < 0) {
		printk(KERN_ERR "messageq_unregister_transport failed! "
			"status = 0x%x\n", status);
	}
	return;
}
EXPORT_SYMBOL(messageq_unregister_transport);

/* Set the destination queue of the message. */
void messageq_set_reply_queue(void *messageq_handle, messageq_msg msg)
{
	s32 status = 0;

	struct messageq_object *obj = \
			(struct messageq_object *) messageq_handle;

	if (WARN_ON(unlikely(messageq_handle == NULL))) {
		status = -EINVAL;
		goto exit;
	}
	if (WARN_ON(unlikely(msg == NULL))) {
		status = -EINVAL;
		goto exit;
	}

	msg->reply_id = (u16)(obj->queue);
	msg->reply_proc = (u16)(obj->queue >> 16);
	return;

exit:
	printk(KERN_ERR "messageq_set_reply_queue failed: status = 0x%x",
		status);
	return;
}
EXPORT_SYMBOL(messageq_set_reply_queue);

/* Get the queue _id of the message. */
u32 messageq_get_queue_id(void *messageq_handle)
{
	struct messageq_object *obj = \
			(struct messageq_object *) messageq_handle;
	u32 queue_id = MESSAGEQ_INVALIDMESSAGEQ;

	if (WARN_ON(unlikely(obj == NULL))) {
		printk(KERN_ERR "messageq_get_queue_id: obj passed is NULL!\n");
		goto exit;
	}

	queue_id = (obj->queue);

exit:
	return queue_id;
}
EXPORT_SYMBOL(messageq_get_queue_id);

/* Get the proc _id of the message. */
u16 messageq_get_proc_id(void *messageq_handle)
{
	struct messageq_object *obj = \
			(struct messageq_object *) messageq_handle;
	u16 proc_id = MULTIPROC_INVALIDID;

	if (WARN_ON(unlikely(obj == NULL))) {
		printk(KERN_ERR "messageq_get_proc_id: obj passed is NULL!\n");
		goto exit;
	}

	proc_id = (u16)(obj->queue >> 16);

exit:
	return proc_id;
}
EXPORT_SYMBOL(messageq_get_proc_id);

/* Get the destination queue of the message. */
u32 messageq_get_dst_queue(messageq_msg msg)
{
	u32 queue_id = MESSAGEQ_INVALIDMESSAGEQ;

	if (WARN_ON(unlikely(msg == NULL))) {
		printk(KERN_ERR "messageq_get_dst_queue: msg passed is "
			"NULL!\n");
		goto exit;
	}

	/*construct queue value */
	if (msg->dst_id != (u32)MESSAGEQ_INVALIDMESSAGEQ)
		queue_id = ((u32) multiproc_self() << 16) | msg->dst_id;

exit:
	return queue_id;
}
EXPORT_SYMBOL(messageq_get_dst_queue);

/* Get the message id of the message. */
u16 messageq_get_msg_id(messageq_msg msg)
{
	u16 id = MESSAGEQ_INVALIDMSGID;

	if (WARN_ON(unlikely(msg == NULL))) {
		printk(KERN_ERR "messageq_get_msg_id: msg passed is NULL!\n");
		goto exit;
	}

	id = msg->msg_id;

exit:
	return id;
}
EXPORT_SYMBOL(messageq_get_msg_id);

/* Get the message size of the message. */
u32 messageq_get_msg_size(messageq_msg msg)
{
	u32 size = 0;

	if (WARN_ON(unlikely(msg == NULL))) {
		printk(KERN_ERR "messageq_get_msg_size: msg passed is NULL!\n");
		goto exit;
	}

	size = msg->msg_size;

exit:
	return size;
}
EXPORT_SYMBOL(messageq_get_msg_size);

/* Get the message priority of the message. */
u32 messageq_get_msg_pri(messageq_msg msg)
{
	u32 priority = MESSAGEQ_NORMALPRI;

	if (WARN_ON(unlikely(msg == NULL))) {
		printk(KERN_ERR "messageq_get_msg_pri: msg passed is NULL!\n");
		goto exit;
	}

	priority = ((u32)(msg->flags & MESSAGEQ_PRIORITYMASK));

exit:
	return priority;
}
EXPORT_SYMBOL(messageq_get_msg_pri);

/* Get the embedded source message queue out of the message. */
u32 messageq_get_reply_queue(messageq_msg msg)
{
	u32 queue = MESSAGEQ_INVALIDMESSAGEQ;

	if (WARN_ON(unlikely(msg == NULL))) {
		printk(KERN_ERR "messageq_get_reply_queue: msg passed is "
			"NULL!\n");
		goto exit;
	}

	if (msg->reply_id != (u16)MESSAGEQ_INVALIDMESSAGEQ)
		queue = ((u32)(msg->reply_proc) << 16) | msg->reply_id;

exit:
	return queue;
}
EXPORT_SYMBOL(messageq_get_reply_queue);

/* Set the message id of the message. */
void messageq_set_msg_id(messageq_msg msg, u16 msg_id)
{
	if (WARN_ON(unlikely(msg == NULL))) {
		printk(KERN_ERR "messageq_set_msg_id: msg passed is NULL!\n");
		goto exit;
	}

	msg->msg_id = msg_id;

exit:
	return;
}
EXPORT_SYMBOL(messageq_set_msg_id);

/* Set the priority of the message. */
void messageq_set_msg_pri(messageq_msg msg, u32 priority)
{
	if (WARN_ON(unlikely(msg == NULL))) {
		printk(KERN_ERR "messageq_set_msg_pri: msg passed is NULL!\n");
		goto exit;
	}

	msg->flags = priority & MESSAGEQ_PRIORITYMASK;

exit:
	return;
}
EXPORT_SYMBOL(messageq_set_msg_pri);

/* Sets the tracing of a message */
void messageq_set_msg_trace(messageq_msg msg, bool trace_flag)
{
	if (WARN_ON(unlikely(msg == NULL))) {
		printk(KERN_ERR "messageq_set_msg_trace: msg passed is "
			"NULL!\n");
		goto exit;
	}

	msg->flags = (msg->flags & ~MESSAGEQ_TRACEMASK) | \
			(trace_flag << MESSAGEQ_TRACESHIFT);

	printk(KERN_INFO "messageq_set_msg_trace: msg = 0x%x, seq_num = 0x%x"
			"src_proc = 0x%x trace_flag = 0x%x", (uint)msg,
			msg->seq_num, msg->src_proc, trace_flag);
exit:
	return;
}

/* Returns the amount of shared memory used by one transport instance.
 *
 *  The MessageQ module itself does not use any shared memory but the
 *  underlying transport may use some shared memory.
 */
uint messageq_shared_mem_req(void *shared_addr)
{
	uint mem_req;

	if (likely(multiproc_get_num_processors() > 1)) {
		/* Determine device-specific shared memory requirements */
		mem_req = messageq_setup_transport_proxy_shared_mem_req(
								shared_addr);
	} else {
		/* Only 1 processor: no shared memory needed */
		mem_req = 0;
	}

	return mem_req;
}
EXPORT_SYMBOL(messageq_shared_mem_req);

/* Calls the SetupProxy to setup the MessageQ transports. */
int messageq_attach(u16 remote_proc_id, void *shared_addr)
{
	int status = MESSAGEQ_S_SUCCESS;

	if (likely(multiproc_get_num_processors() > 1)) {
		/* Use the messageq_setup_transport_proxy to attach
		 * transports */
		status = messageq_setup_transport_proxy_attach(
						remote_proc_id, shared_addr);
		if (status < 0) {
			printk(KERN_ERR "messageq_attach failed in transport"
					"setup, status = 0x%x", status);
		}
	}

	/*! @retval MESSAGEQ_S_SUCCESS Operation successfully completed! */
	return status;
}
EXPORT_SYMBOL(messageq_attach);

/* Calls the SetupProxy to detach the MessageQ transports. */
int messageq_detach(u16 remote_proc_id)
{
	int status = MESSAGEQ_S_SUCCESS;

	if (likely(multiproc_get_num_processors() > 1)) {
		/* Use the messageq_setup_transport_proxy to detach
		 * transports */
		status = messageq_setup_transport_proxy_detach(remote_proc_id);
		if (unlikely(status < 0)) {
			printk(KERN_ERR "messageq_detach failed in transport"
					"detach, status = 0x%x", status);
		}
	}

	/*! @retval MESSAGEQ_S_SUCCESS Operation successfully completed! */
	return status;
}
EXPORT_SYMBOL(messageq_detach);

/* =============================================================================
 * Internal functions
 * =============================================================================
 */
/* Grow the MessageQ table */
static u16 _messageq_grow(struct messageq_object *obj)
{
	u16 queue_index = messageq_module->num_queues;
	int old_size;
	void **queues;
	void **oldqueues;

	/* No parameter validation required since this is an internal func. */
	old_size = (messageq_module->num_queues) * \
			sizeof(struct messageq_object *);
	queues = kmalloc(old_size + sizeof(struct messageq_object *),
				GFP_KERNEL);
	if (queues == NULL) {
		printk(KERN_ERR "_messageq_grow: Growing the messageq "
			"failed!\n");
		goto exit;
	}

	/* Copy contents into new table */
	memcpy(queues, messageq_module->queues, old_size);
	/* Fill in the new entry */
	queues[queue_index] = (void *)obj;
	/* Hook-up new table */
	oldqueues = messageq_module->queues;
	messageq_module->queues = queues;
	messageq_module->num_queues++;

	/* Delete old table if not statically defined*/
	if (messageq_module->can_free_queues == true)
		kfree(oldqueues);
	else
		messageq_module->can_free_queues = true;

exit:
	return queue_index;
}

/* This is a helper function to initialize a message. */
static void messageq_msg_init(messageq_msg msg)
{
	s32 status = 0;

	if (WARN_ON(unlikely(atomic_cmpmask_and_lt(
				&(messageq_module->ref_count),
				MESSAGEQ_MAKE_MAGICSTAMP(0),
				MESSAGEQ_MAKE_MAGICSTAMP(1)) == true))) {
		status = -ENODEV;
		goto exit;
	}

	if (WARN_ON(unlikely(msg == NULL))) {
		status = -EINVAL;
		goto exit;
	}

	msg->reply_id = (u16) MESSAGEQ_INVALIDMESSAGEQ;
	msg->msg_id = MESSAGEQ_INVALIDMSGID;
	msg->dst_id = (u16) MESSAGEQ_INVALIDMESSAGEQ;
	msg->flags = MESSAGEQ_HEADERVERSION | MESSAGEQ_NORMALPRI;
	msg->src_proc = multiproc_self();

	status = mutex_lock_interruptible(messageq_module->gate_handle);
	if (status < 0)
		goto exit;
	msg->seq_num  = messageq_module->seq_num++;
	mutex_unlock(messageq_module->gate_handle);

exit:
	if (status < 0) {
		printk(KERN_ERR "messageq_msg_init: Invalid NULL msg "
			"specified!\n");
	}
	return;
}
