/*
 *  transportshm.c
 *
 *  Shared Memory Transport module
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

/* Standard headers */
#include <linux/types.h>

/* Utilities headers */
#include <linux/string.h>
#include <linux/slab.h>

/* Syslink headers */
#include <syslink/atomic_linux.h>
/* Module level headers */
#include <multiproc.h>
#include <sharedregion.h>
#include <nameserver.h>
#include <gatepeterson.h>
#include <notify.h>
#include <messageq.h>
#include <listmp.h>
#include <gatemp.h>
#include <transportshm.h>

/* =============================================================================
 * Globals
 * =============================================================================
 */

/* Indicates that the transport is up. */
#define TRANSPORTSHM_UP	0xBADC0FFE

/* transportshm Version. */
#define TRANSPORTSHM_VERSION	1

/*!
 *  @brief  Macro to make a correct module magic number with refCount
 */
#define TRANSPORTSHM_MAKE_MAGICSTAMP(x)                                \
				((TRANSPORTSHM_MODULEID << 12u) | (x))

#define ROUND_UP(a, b)	(((a) + ((b) - 1)) & (~((b) - 1)))

typedef void (*transportshm_err_fxn)(enum transportshm_reason reason,
					void *handle,
					void *msg,
					u32 info);

/* =============================================================================
 * Structures & Enums
 * =============================================================================
 */
/*
 *  Defines the transportshm state object, which contains all the
 *           module specific information.
 */
struct transportshm_module_object {
	atomic_t ref_count;
	/* Flag to indicate initialization state of transportshr */
	struct transportshm_config cfg;
	/* transportshm configuration structure */
	struct transportshm_config def_cfg;
	/* Default module configuration */
	struct transportshm_params def_inst_params;
	/* Default instance parameters */
	void *gate_handle;
	/* Handle to the gate for local thread safety */
	void *transports
		[MULTIPROC_MAXPROCESSORS][MESSAGEQ_NUM_PRIORITY_QUEUES];
	/* Transport to be set in messageq_register_transport */
	transportshm_err_fxn err_fxn;
	/* Error function */

};

/*
 * Structure of attributes in shared memory
 */
struct transportshm_attrs {
	VOLATILE u32 flag;		/* flag */
	VOLATILE u32 creator_proc_id;	/* Creator processor ID */
	VOLATILE u32 notify_event_id;	/* Notify event number */
	VOLATILE u16 priority;		/* priority */
	VOLATILE u32 *gatemp_addr;	/* gatemp shared memory srptr */
};

/*
 * Structure defining config parameters for the MessageQ transport
 *  instances.
 */
struct transportshm_object {
	VOLATILE struct transportshm_attrs *self;
	/* Attributes for local processor in shared memory */
	VOLATILE struct transportshm_attrs *other;
	/* Attributes for remote processor in shared memory */
	void *local_list;
	/* List for this processor	*/
	void *remote_list;
	/* List for remote processor	*/
	VOLATILE int status;
	/* Current status		 */
	u32 alloc_size;
	/* Shared memory allocated	 */
	bool cache_enabled;
	/* Whether to cache calls	 */
	int notify_event_id;
	/* Notify event to be used	*/
	u16 region_id;
	/* The shared region id		*/
	u16 remote_proc_id;
	/* dst proc id			*/
	u32 priority;
	/* Priority of messages supported by this transport */
	void *gate;
	/* Gate for critical regions	*/
	struct transportshm_params params;
	/* Instance specific parameters  */
};

/* =============================================================================
 *  Globals
 * =============================================================================
 */
/*
 *  @var    transportshm_state
 *
 * transportshm state object variable
 */
static struct transportshm_module_object transportshm_state = {
	.gate_handle = NULL,
	.def_cfg.err_fxn = NULL,
	.def_inst_params.gate = NULL,
	.def_inst_params.shared_addr = NULL,
	.def_inst_params.notify_event_id = (u32)(-1),
	.def_inst_params.priority = MESSAGEQ_NORMALPRI
};

/* Pointer to module state */
static struct transportshm_module_object *transportshm_module =
							&transportshm_state;

/* =============================================================================
 * Forward declarations of internal functions
 * =============================================================================
 */
/* Callback function registered with the Notify module. */
static void _transportshm_notify_fxn(u16 proc_id,
				     u16 line_id,
				     u32 event_id,
				     uint *arg,
				     u32 payload);

/* Function to create/open the handle. */
static int _transportshm_create(struct transportshm_object **handle_ptr,
				u16 proc_id,
				const struct transportshm_params *params,
				bool create_flag);

/* =============================================================================
 * APIs called directly by applications
 * =============================================================================
 */
/*
 * ======== transportshm_get_config ========
 *  Purpose:
 *  Get the default configuration for the transportshm
 *  module.
 *
 *  This function can be called by the application to get their
 *  configuration parameter to transportshm_setup filled in
 *  by the transportshm module with the default parameters.
 *  If the user does not wish to make any change in the default
 *  parameters, this API is not required to be called.
 */
void transportshm_get_config(struct transportshm_config *cfg)
{
	if (WARN_ON(cfg == NULL))
		goto exit;

	if (atomic_cmpmask_and_lt(&(transportshm_module->ref_count),
			TRANSPORTSHM_MAKE_MAGICSTAMP(0),
			TRANSPORTSHM_MAKE_MAGICSTAMP(1)) == true) {
		memcpy(cfg, &(transportshm_module->def_cfg),
			sizeof(struct transportshm_config));
	} else {
		memcpy(cfg, &(transportshm_module->cfg),
			sizeof(struct transportshm_config));
	}
	return;

exit:
	pr_err("transportshm_get_config: Argument of type"
		"(struct transportshm_config *) passed is null!\n");
}


/*
 * ======== transportshm_setup ========
 *  Purpose:
 *  Setup the transportshm module.
 *
 *  This function sets up the transportshm module. This
 *  function must be called before any other instance-level APIs can
 *  be invoked.
 *  Module-level configuration needs to be provided to this
 *  function. If the user wishes to change some specific config
 *  parameters, then transportshm_getconfig can be called
 *  to get the configuration filled with the default values. After
 *  this, only the required configuration values can be changed. If
 *  the user does not wish to make any change in the default
 *  parameters, the application can simply call
 *  transportshm_setup with NULL parameters. The default
 *  parameters would get automatically used.
 */
int transportshm_setup(const struct transportshm_config *cfg)
{
	int status = TRANSPORTSHM_SUCCESS;
	struct transportshm_config tmpCfg;

	/* This sets the refCount variable is not initialized, upper 16 bits is
	* written with module Id to ensure correctness of refCount variable.
	*/
	atomic_cmpmask_and_set(&transportshm_module->ref_count,
			TRANSPORTSHM_MAKE_MAGICSTAMP(0),
			TRANSPORTSHM_MAKE_MAGICSTAMP(0));

	if (atomic_inc_return(&transportshm_module->ref_count)
		!= TRANSPORTSHM_MAKE_MAGICSTAMP(1u)) {
		return 1;
	}

	if (cfg == NULL) {
		transportshm_get_config(&tmpCfg);
		cfg = &tmpCfg;
	}

	transportshm_module->gate_handle = \
				kmalloc(sizeof(struct mutex), GFP_KERNEL);
	if (transportshm_module->gate_handle == NULL) {
		/* @retval TRANSPORTSHM_E_FAIL Failed to create
		GateMutex! */
		status = TRANSPORTSHM_E_FAIL;
		pr_err("transportshm_setup: Failed to create mutex!\n");
		atomic_set(&transportshm_module->ref_count,
				TRANSPORTSHM_MAKE_MAGICSTAMP(0));
		goto exit;
	}
	mutex_init(transportshm_module->gate_handle);

	/* Copy the user provided values into the state object. */
	memcpy(&transportshm_module->cfg, cfg,
			sizeof(struct transportshm_config));
	memset(&(transportshm_module->transports), 0, (sizeof(void *) * \
		MULTIPROC_MAXPROCESSORS * MESSAGEQ_NUM_PRIORITY_QUEUES));
	return status;

exit:
	pr_err("transportshm_setup failed: status = 0x%x", status);
	return status;
}


/*
 * ======== transportshm_destroy ========
 *  Purpose:
 *  Destroy the transportshm module.
 *
 *  Once this function is called, other transportshm module
 *  APIs, except for the transportshm_getConfig API cannot
 *  be called anymore.
 */
int transportshm_destroy(void)
{
	struct transportshm_object *obj = NULL;
	int status = 0;
	u16 i;
	u16 j;

	if (WARN_ON(atomic_cmpmask_and_lt(
			&(transportshm_module->ref_count),
			TRANSPORTSHM_MAKE_MAGICSTAMP(0),
			TRANSPORTSHM_MAKE_MAGICSTAMP(1)) == true)) {
		status = -ENODEV;
		goto exit;
	}

	if (!(atomic_dec_return(&transportshm_module->ref_count)
				== TRANSPORTSHM_MAKE_MAGICSTAMP(0))) {
		status = 1;
		goto exit;
	}

	/* Temporarily increment ref_count here. */
	atomic_set(&transportshm_module->ref_count,
			TRANSPORTSHM_MAKE_MAGICSTAMP(1));

	/* Delete any Transports that have not been deleted so far. */
	for (i = 0; i < MULTIPROC_MAXPROCESSORS; i++) {
		for (j = 0 ; j < MESSAGEQ_NUM_PRIORITY_QUEUES; j++) {
			if (transportshm_module->transports[i][j] != \
				NULL) {
				obj = (struct transportshm_object *)
					transportshm_module->transports[i][j];
				if (obj->self != NULL) {
					if (obj->self->creator_proc_id
							== multiproc_self())
						transportshm_delete(
							&(transportshm_module->
							transports[i][j]));
					else
						transportshm_close(
							&(transportshm_module->
							transports[i][j]));
				}
			}
		}
	}

	/* Decrease the ref_count */
	atomic_set(&transportshm_module->ref_count,
				TRANSPORTSHM_MAKE_MAGICSTAMP(0));

	if (transportshm_module->gate_handle != NULL) {
		kfree(transportshm_module->gate_handle);
		transportshm_module->gate_handle = NULL;
	}
	return 0;

exit:
	if (status < 0)
		pr_err("transportshm_destroy failed: status = 0x%x\n", status);
	return status;
}


/*
 * ======== transportshm_params_init ========
 *  Purpose:
 *  Get Instance parameters
 */
void transportshm_params_init(struct transportshm_params *params)
{
	if (WARN_ON(atomic_cmpmask_and_lt(
			&(transportshm_module->ref_count),
			TRANSPORTSHM_MAKE_MAGICSTAMP(0),
			TRANSPORTSHM_MAKE_MAGICSTAMP(1)) == true)) {
		pr_err("transportshm_params_init: Module was "
				" not initialized\n");
		goto exit;
	}

	if (WARN_ON(params == NULL)) {
		pr_err("transportshm_params_init: Argument of"
				" type (struct transportshm_params *) "
				"is NULL!");
		goto exit;
	}

	memcpy(params, &(transportshm_module->def_inst_params),
			sizeof(struct transportshm_params));

exit:
	return;
}

/*
 * ======== transportshm_create ========
 *  Purpose:
 *  Create a transport instance. This function waits for the remote
 *  processor to complete its transport creation. Hence it must be
 *  called only after the remote processor is running.
 */
void *transportshm_create(u16 proc_id,
			const struct transportshm_params *params)
{
	struct transportshm_object *handle = NULL;
	int status = 0;

	BUG_ON(params == NULL);
	BUG_ON(!(proc_id < multiproc_get_num_processors()));
	if (WARN_ON(atomic_cmpmask_and_lt(
			&(transportshm_module->ref_count),
			TRANSPORTSHM_MAKE_MAGICSTAMP(0),
			TRANSPORTSHM_MAKE_MAGICSTAMP(1)) == true)) {
		status = -ENODEV;
		goto exit;
	}

	if (WARN_ON(params == NULL)) {
		status = -EINVAL;
		goto exit;
	}
	if (transportshm_module->transports[proc_id][params->priority] \
		!= NULL) {
		/* Specified transport is already registered. */
		status = MESSAGEQ_E_ALREADYEXISTS;
		goto exit;
	}

	status = _transportshm_create(&handle, proc_id, params, true);

	if (status < 0) {
		status = -EFAULT;
		goto exit;
	}

	return handle;

exit:
	pr_err("transportshm_create failed: status = 0x%x\n", status);
	return handle;
}


/*
 * ======== transportshm_delete ========
 *  Purpose:
 *  Delete instance
 */
int transportshm_delete(void **handle_ptr)
{
	int status = 0;
	int tmp_status = 0;
	struct transportshm_object *handle;
	u16 proc_id;
	u32 key;

	if (WARN_ON(atomic_cmpmask_and_lt(
			&(transportshm_module->ref_count),
			TRANSPORTSHM_MAKE_MAGICSTAMP(0),
			TRANSPORTSHM_MAKE_MAGICSTAMP(1)) == true)) {
		status = -ENODEV;
		goto exit;
	}
	if (WARN_ON(handle_ptr == NULL)) {
		status = -EINVAL;
		goto exit;
	}
	if (WARN_ON(*handle_ptr == NULL)) {
		status = -EINVAL;
		pr_warn("transportshm_delete: Invalid NULL"
			" mqtshm_handle specified! status = 0x%x\n", status);
		goto exit;
	}

	key = mutex_lock_interruptible(transportshm_state.gate_handle);
	if (key < 0)
		goto exit;

	handle = (struct transportshm_object *) (*handle_ptr);
	if (handle != NULL) {
		if (handle->self != NULL) {
			proc_id = handle->self->creator_proc_id;
			/* Clear handle in the local array */
			transportshm_module->
				transports[proc_id][handle->priority] = NULL;
			/* clear the self flag */
			handle->self->flag = 0;
#if 0
			if (EXPECT_FALSE(handle->cache_enabled)) {
				Cache_wbinv((Ptr)&(handle->self->flag),
					sharedregion_get_cache_line_size(
						handle->region_id),
						Cache_Type_ALL,
						true);
			}
#endif
		}

		if (handle->local_list != NULL) {
			status = listmp_delete(&handle->local_list);
			if (status < 0)
				pr_warn("transportshm_delete: "
						"Failed to delete local listmp "
						"instance!\n");
		}

		if (handle->remote_list != NULL) {
			tmp_status = listmp_close(&handle->remote_list);
			if ((tmp_status < 0) && (status >= 0)) {
				status = tmp_status;
				pr_warn("transportshm_delete: "
						"Failed to close remote listmp "
						"instance!\n");
			}
		}

		messageq_unregister_transport(handle->
				remote_proc_id, handle->params.priority);

		tmp_status = notify_unregister_event_single(handle->
						remote_proc_id,
						0,
						handle->notify_event_id);
		if (tmp_status < 0) {
			status = tmp_status;
			pr_warn("transportshm_delete: Failed to "
					"unregister notify event!\n");
		}

		kfree(handle);
		*handle_ptr = NULL;
	}
	mutex_unlock(transportshm_state.gate_handle);
	return status;

exit:
	if (status < 0)
		pr_err("transportshm_delete failed: "
			"status = 0x%x\n", status);
	return status;
}

/*
 * ========== transportshm_open_by_addr ===========
 * Open a transport instance
 */
int
transportshm_open_by_addr(void *shared_addr, void **handle_ptr)
{
	int status = 0;
	struct transportshm_attrs *attrs = NULL;
	struct transportshm_params params;
	u16 id;

	BUG_ON(shared_addr == NULL);
	BUG_ON(handle_ptr == NULL);

	if (WARN_ON(atomic_cmpmask_and_lt(
			&(transportshm_module->ref_count),
			TRANSPORTSHM_MAKE_MAGICSTAMP(0),
			TRANSPORTSHM_MAKE_MAGICSTAMP(1)) == true)) {
		status = -ENODEV;
		goto exit;
	}

	if (shared_addr == NULL) {
		status = -EINVAL;
		goto exit;
	}

	if (handle_ptr == NULL) {
		status = -EINVAL;
		goto exit;
	}

	attrs = (struct transportshm_attrs *) shared_addr;
	id = sharedregion_get_id(shared_addr);

	if (id == SHAREDREGION_INVALIDREGIONID) {
		status = -EFAULT;
		goto exit;
	}
	if (((u32) shared_addr % sharedregion_get_cache_line_size(id) != 0)) {
		status = -EFAULT;
		goto exit;
	}

#if 0
	/* invalidate the attrs before using it */
	if (EXPECT_FALSE(SharedRegion_isCacheEnabled(id))) {
		Cache_inv((Ptr) attrs,
			sizeof(struct transportshm_attrs),
			Cache_Type_ALL,
			true);
	}
#endif
	transportshm_params_init(&params);
	/* set params field */
	params.shared_addr = shared_addr;
	params.notify_event_id = attrs->notify_event_id | \
						(NOTIFY_SYSTEMKEY << 16);
	params.priority = attrs->priority;

	if (unlikely(attrs->flag != TRANSPORTSHM_UP)) {
		status = -EFAULT;
		*handle_ptr = NULL;
		goto exit;
	}

	/* Create the object */
	status = _transportshm_create((struct transportshm_object **)
					handle_ptr,
					attrs->creator_proc_id, &params, false);
	if (status < 0)
		goto exit;

	return status;

exit:
	pr_err("transportshm_open_by_addr failed: status = 0x%x\n", status);
	return status;
}

/*
 * ========== transportshm_close ===========
 * Close an opened transport instance
 */
int
transportshm_close(void **handle_ptr)
{
	int status = 0;
	int tmp_status = 0;
	struct transportshm_object *obj;

	if (WARN_ON(atomic_cmpmask_and_lt(
			&(transportshm_module->ref_count),
			TRANSPORTSHM_MAKE_MAGICSTAMP(0),
			TRANSPORTSHM_MAKE_MAGICSTAMP(1)) == true)) {
		status = -ENODEV;
		goto exit;
	}
	if (WARN_ON(handle_ptr == NULL)) {
		status = -EINVAL;
		goto exit;
	}
	if (WARN_ON(*handle_ptr == NULL)) {
		status = -EINVAL;
		goto exit;
	}

	obj = (struct transportshm_object *)(*handle_ptr);
	transportshm_module->transports[obj->remote_proc_id]
					[obj->params.priority] = NULL;

	if (obj->other != NULL) {
		/* other flag was set by remote proc */
		obj->other->flag = 0;
#if 0
		if (EXPECT_FALSE(obj->cache_enabled)) {
			Cache_wbInv(&(obj->other->flag),
				 sharedregion_get_cache_line_size
						(obj->region_id),
				 Cache_Type_ALL,
				 TRUE);
		}
#endif
	}

	if (obj->gate != NULL) {
		status = gatemp_close(&obj->gate);
		if (status < 0) {
			status = TRANSPORTSHM_E_FAIL;
			pr_err("transportshm_close: "
				"gatemp_close failed, status [0x%x]\n",
				status);
		}
	}

	if (obj->local_list != NULL) {
		tmp_status = listmp_close(&obj->local_list);
		if ((tmp_status < 0) && (status >= 0)) {
			status = TRANSPORTSHM_E_FAIL;
			pr_err("transportshm_close: "
				"listmp_close(local_list) failed, "
				"status [0x%x]\n", status);
		}
	}

	if (obj->remote_list != NULL) {
		tmp_status = listmp_close(&obj->remote_list);
		if ((tmp_status < 0) && (status >= 0)) {
			status = TRANSPORTSHM_E_FAIL;
			pr_err("transportshm_close: "
				"listmp_close(remote_list) failed, "
				"status [0x%x]\n", status);
		}
	}

	messageq_unregister_transport(obj->remote_proc_id,
					obj->params.priority);

	tmp_status = notify_unregister_event_single(obj->remote_proc_id, 0,
			(obj->notify_event_id | (NOTIFY_SYSTEMKEY << 16)));
	if ((tmp_status < 0) && (status >= 0))
		status = TRANSPORTSHM_E_FAIL;

	kfree(obj);
	*handle_ptr = NULL;
exit:
	if (status < 0)
		pr_err("transportshm_close failed: status = 0x%x\n", status);
	return status;
}


/*
 * ======== transportshm_put ========
 *  Purpose:
 *  Put msg to remote list
*/
int transportshm_put(void *handle, void *msg)
{
	int status = 0;
	struct transportshm_object *obj = NULL;
	/*int *key;*/

	if (WARN_ON(atomic_cmpmask_and_lt(
			&(transportshm_module->ref_count),
			TRANSPORTSHM_MAKE_MAGICSTAMP(0),
			TRANSPORTSHM_MAKE_MAGICSTAMP(1)) == true)) {
		status = -ENODEV;
		goto exit;
	}
	if (WARN_ON(handle == NULL)) {
		status = -EINVAL;
		goto exit;
	}
	if (WARN_ON(msg == NULL)) {
		status = -EINVAL;
		goto exit;
	}

	obj = (struct transportshm_object *)handle;
#if 0
	/* writeback invalidate the message */
	if (EXPECT_FALSE(obj->cache_enabled)) {
		Cache_wbinv((Ptr) msg,
		 ((MessageQ_Msg)(msg))->msgSize,
		 Cache_Type_ALL,
		 true);
	}
#endif
	/* make sure ListMP_put and sendEvent are done before remote executes */
	/*key = gatemp_enter(obj->gate);*/
	status = listmp_put_tail(obj->remote_list, (struct listmp_elem *) msg);
	if (status < 0) {
		pr_err("transportshm_put: Failed to put "
			"message in the shared list! status = 0x%x\n", status);
		goto exit_with_gate;
	}

	status = notify_send_event(obj->remote_proc_id, 0,
				obj->notify_event_id, 0, false);
	if (status < 0)
		goto notify_send_fail;
	else
		goto exit_with_gate;


notify_send_fail:
	pr_err("transportshm_put: Notification to remote "
		"processor failed, status = 0x%x\n", status);
	/* If sending the event failed, then remove the element from the */
	/* list.  Ignore the status of remove. */
	listmp_remove(obj->remote_list, (struct listmp_elem *) msg);

exit_with_gate:
	/*gatemp_leave(obj->gate, key);*/
exit:
	if (status < 0)
		pr_err("transportshm_put failed: status = 0x%x\n", status);
	return status;
}

/*
 * ======== transportshm_control ========
 *  Purpose:
 *  Control Function
*/
int transportshm_control(void *handle, u32 cmd, u32 *cmd_arg)
{
	BUG_ON(handle == NULL);

	pr_err("transportshm_control not supported!\n");
	return TRANSPORTSHM_E_NOTSUPPORTED;
}

/*
 * ======== transportshm_get_status ========
 *  Purpose:
 *  Get status
 */
enum transportshm_status transportshm_get_status(void *handle)
{
	struct transportshm_object *obj = \
			(struct transportshm_object *) handle;

	BUG_ON(obj == NULL);

	return obj->status;
}

/*
 * ======== transportshm_put ========
 *  Purpose:
 *  Get shared memory requirements.
 */
u32 transportshm_shared_mem_req(const struct transportshm_params *params)
{
	u32 mem_req = 0;
	s32 min_align;
	u16 region_id;
	struct listmp_params list_params;
	s32 status = 0;

	if (params == NULL) {
		status = -EINVAL;
		goto exit;
	}
	region_id = sharedregion_get_id(params->shared_addr);

	if (region_id == SHAREDREGION_INVALIDREGIONID)  {
		status = -EFAULT;
		goto exit;
	}

	min_align = 4; /*memory_get_max_default_type_align(); */
	if (sharedregion_get_cache_line_size(region_id) > min_align)
		min_align = sharedregion_get_cache_line_size(region_id);

	/* for the Attrs structure */
	mem_req = ROUND_UP(sizeof(struct transportshm_attrs), min_align);

	/* for the second Attrs structure */
	mem_req += ROUND_UP(sizeof(struct transportshm_attrs), min_align);

	listmp_params_init(&list_params);
	list_params.region_id = region_id;
	/* for local listMP */
	mem_req += listmp_shared_mem_req(&list_params);

	/* for remote listMP */
	mem_req += listmp_shared_mem_req(&list_params);

exit:
	if (status < 0)
		pr_err("transportshm_shared_mem_req failed: "
			"status = 0x%x\n", status);

	return mem_req;
}


/* =============================================================================
 * internal functions
 * =============================================================================
 */
/*
 * ======== _transportshm_notify_fxn ========
 *  Purpose:
 *  Callback function registered with the Notify module.
 */
static void _transportshm_notify_fxn(u16 proc_id, u16 line_id, u32 event_no,
					uint *arg, u32 payload)
{
	struct transportshm_object *obj = NULL;
	messageq_msg msg = NULL;
	u32 queue_id;
	u32 key;

	key = mutex_lock_interruptible(transportshm_state.gate_handle);

	if (key < 0)
		goto mutex_fail;

	if (WARN_ON(arg == NULL))
		goto exit;

	obj = (struct transportshm_object *)arg;
	/*  While there is are messages, get them out and send them to
	 *  their final destination. */
	if (obj->local_list)
		msg = (messageq_msg) listmp_get_head(obj->local_list);
	else
		goto exit;
	while (msg != NULL) {
		/* Get the destination message queue Id */
		queue_id = messageq_get_dst_queue(msg);

		/* put the message to the destination queue */
		messageq_put(queue_id, msg);
		if (obj->local_list)
			msg = (messageq_msg)
				listmp_get_head(obj->local_list);
		else
			msg = NULL;
	}
	mutex_unlock(transportshm_state.gate_handle);
	return;

exit:
	mutex_unlock(transportshm_state.gate_handle);
mutex_fail:
	pr_err("transportshm_notify_fxn: argument passed is NULL!\n");
	return;
}


/*
 * ======== transportshm_delete ========
 *  Purpose:
 *  This will set the asynchronous error function for the transport module
 */
void transportshm_set_err_fxn(	void (*err_fxn)(
				enum transportshm_reason reason,
				void *handle,
				void *msg,
				u32 info))
{
	int key;

	key = mutex_lock_interruptible(transportshm_module->gate_handle);
	if (key < 0)
		goto exit;

	transportshm_module->cfg.err_fxn = err_fxn;
	mutex_unlock(transportshm_module->gate_handle);

exit:
	return;
}


/*
 * ========= _transportshm_create =========
 *  Purpose:
 *  Internal function for create()/open()
 */
static int _transportshm_create(struct transportshm_object **handle_ptr,
		u16 proc_id, const struct transportshm_params *params,
		bool create_flag)
{
	int status = 0;
	struct transportshm_object *handle = NULL;
	void *local_addr = NULL;
	int local_index;
	int remote_index;
	u32 min_align;
	struct listmp_params listmp_params[2];

	BUG_ON(handle_ptr == NULL);
	BUG_ON(params == NULL);
	BUG_ON(proc_id >= multiproc_get_num_processors());

	/*
	 *  Determine who gets the '0' slot and who gets the '1' slot
	 *  The '0' slot is given to the lower multiproc id.
	 */
	if (multiproc_self() < proc_id) {
		local_index = 0;
		remote_index = 1;
	} else {
		local_index = 1;
		remote_index = 0;
	}

	handle = kzalloc(sizeof(struct transportshm_object), GFP_KERNEL);
	if (handle == NULL) {
		status = -ENOMEM;
		goto exit;
	}
	*handle_ptr = handle;

	if (create_flag == false) {
		/* Open by shared addr */
		handle->self = (struct transportshm_attrs *)
							params->shared_addr;
		handle->region_id = sharedregion_get_id(params->shared_addr);
		BUG_ON(handle->region_id == SHAREDREGION_INVALIDREGIONID);

		handle->cache_enabled = sharedregion_is_cache_enabled(handle->
								region_id);

		local_addr = sharedregion_get_ptr((u32 *)handle->self->
								gatemp_addr);
		BUG_ON(local_addr == NULL);

		status = gatemp_open_by_addr(local_addr, &handle->gate);
		if (status < 0) {
			status = -EFAULT;
			goto exit;
		}
	} else {
		/* Init the gate for ListMP create below */
		if (params->gate != NULL)
			handle->gate = params->gate;
		else
			handle->gate = gatemp_get_default_remote();

		if (handle->gate == NULL) {
			status = -EFAULT;
			goto exit;
		}
		memcpy(&(handle->params), params,
				sizeof(struct transportshm_params));
		handle->region_id = sharedregion_get_id(params->shared_addr);

		/* Assert that the buffer is in a valid shared
		 * region
		 */
		if (handle->region_id == SHAREDREGION_INVALIDREGIONID) {
			status = -EFAULT;
			goto exit;
		}
		if (((u32)params->shared_addr
			% sharedregion_get_cache_line_size(handle->region_id)
			!= 0)) {
			status = -EFAULT;
			goto exit;
		}

		/* set handle's cache_enabled, type, attrs */
		handle->cache_enabled = sharedregion_is_cache_enabled(
							handle->region_id);
		handle->self = (struct transportshm_attrs *)
							params->shared_addr;
	}

	/* Determine the minimum alignment to align to */
	min_align = 4; /*memory_get_max_default_type_align(); */
	if (sharedregion_get_cache_line_size(handle->region_id) > min_align)
		min_align = sharedregion_get_cache_line_size(handle->
								region_id);
	/*
	 *  Carve up the shared memory.
	 *  If cache is enabled, these need to be on separate cache
	 *  lines. This is done with min_align and ROUND_UP function.
	 */

	handle->other = (struct transportshm_attrs *)(((u32)handle->self) +
		(ROUND_UP(sizeof(struct transportshm_attrs), min_align)));


	listmp_params_init(&(listmp_params[0]));
	listmp_params[0].gatemp_handle = handle->gate;
	listmp_params[0].shared_addr = (void *)(((u32)handle->other)
		 + (ROUND_UP(sizeof(struct transportshm_attrs), min_align)));

	listmp_params_init(&listmp_params[1]);
	listmp_params[1].gatemp_handle = handle->gate;
	listmp_params[1].shared_addr = (void *)
			(((u32)listmp_params[0].shared_addr)
			+ listmp_shared_mem_req(&listmp_params[0]));

	handle->notify_event_id = params->notify_event_id;
	handle->priority = params->priority;
	handle->remote_proc_id = proc_id;

	if (create_flag == true) {
		handle->local_list =
				listmp_create(&(listmp_params[local_index]));
		if (handle->local_list == NULL) {
			status = -EFAULT;
			goto exit;
		}
		handle->remote_list = listmp_create(
					&(listmp_params[remote_index]));
		if (handle->remote_list == NULL) {
			status = -EFAULT;
			goto exit;
		}
	} else {
		/* Open the local ListMP instance */
		status = listmp_open_by_addr(
				listmp_params[local_index].shared_addr,
				&(handle->local_list));
		if (status < 0) {
			status = -EFAULT;
			goto exit;
		}

		status = listmp_open_by_addr(
				listmp_params[remote_index].shared_addr,
				&(handle->remote_list));
		if (status < 0) {
			status = -EFAULT;
			goto exit;
		}
	}

	status = notify_register_event_single(proc_id,
						0, /* lineId */
						params->notify_event_id,
						_transportshm_notify_fxn,
						handle);
	if (status < 0) {
		status = -EFAULT;
		goto exit;
	}

	if (create_flag == true) {
		handle->self->creator_proc_id = multiproc_self();
		handle->self->notify_event_id = handle->notify_event_id;
		handle->self->priority = handle->priority;

		/* Store the GateMP shared_addr in the Attrs */
		handle->self->gatemp_addr =
					gatemp_get_shared_addr(handle->gate);
		handle->self->flag = TRANSPORTSHM_UP;
#if 0
		if (EXPECT_FALSE(handle->cache_enabled)) {
			Cache_wbinv((Ptr) handle->self,
				     sizeof(struct transportshm_attrs),
				     Cache_Type_ALL,
				     true);
		}
#endif
	} else {
		handle->other->flag = TRANSPORTSHM_UP;
#if 0
		if (EXPECT_FALSE(handle->cache_enabled)) {
			Cache_wb((Ptr)&(handle->other->flag),
				min_align,
				Cache_Type_ALL,
				true);
		}
#endif
	}

	/* Register the transport with MessageQ */
	status = messageq_register_transport(handle, proc_id,
							params->priority);
	if (status < 0) {
		status = -EFAULT;
		goto exit;
	}
	handle->status = TRANSPORTSHM_UP;
	/* Set handle in the local array. */
	transportshm_module->transports[handle->remote_proc_id]
		[handle->params.priority] = handle;

	return status;

exit:
	/* Cleanup in case of error. */
	if (create_flag == true)
		transportshm_delete((void **)handle_ptr);
	else
		transportshm_close((void **)handle_ptr);

	return status;
}
