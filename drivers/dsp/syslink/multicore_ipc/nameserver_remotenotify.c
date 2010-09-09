/*
 *  nameserver_remotenotify.c
 *
 *  The nameserver_remotenotify module provides functionality to get name
 *  value pair from a remote nameserver.
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

/* Standard headers */
#include <linux/types.h>

/* Utilities headers */
#include <linux/string.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/semaphore.h>

/* Syslink headers */
#include <syslink/atomic_linux.h>

/* Module level headers */
#include <multiproc.h>
#include <sharedregion.h>
#include <gate_remote.h>
#include <gatemp.h>
#include <nameserver.h>
#include <nameserver_remotenotify.h>
#include <notify.h>


/*
 *  Macro to make a correct module magic number with refCount
 */
#define NAMESERVERREMOTENOTIFY_MAKE_MAGICSTAMP(x)	\
			((NAMESERVERREMOTENOTIFY_MODULEID << 12u) | (x))

/*
 *  Cache line length
 *  TODO: Short-term hack. Make parameter or figure out some other way!
 */
#define NAMESERVERREMOTENOTIFY_CACHESIZE	128

/*
 *  Maximum length of value buffer that can be stored in the NameServer
 *  managed by this NameServerRemoteNotify instance. Value in 4-byte words
 */
#define NAMESERVERREMOTENOTIFY_MAXVALUEBUFLEN	75

/* Defines the nameserver_remotenotify state object, which contains all the
 * module specific information
 */
struct nameserver_remotenotify_module_object {
	struct nameserver_remotenotify_config cfg;
	struct nameserver_remotenotify_config def_cfg;
	struct nameserver_remotenotify_params def_inst_params;
	bool is_setup;
	void *gate_handle;
	atomic_t ref_count;
	void *nsr_handles[MULTIPROC_MAXPROCESSORS];
};

/*
 *  NameServer remote transport packet definition
 */
struct nameserver_remotenotify_message {
	u32 request; /* If this is a request set to 1 */
	u32 response; /* If this is a response set to 1 */
	u32 request_status; /* If request sucessful set to 1 */
	u32 value; /* Holds value if len <= 4 */
	u32 value_len; /* Len of value */
	u32 instance_name[8]; /* Name of NameServer instance */
	u32 name[8]; /* Size is 8 to align to 128 cache line boundary */
	u32 value_buf[NAMESERVERREMOTENOTIFY_MAXVALUEBUFLEN];
		/* Supports up to 300-byte value */
};

/*
 *  NameServer remote transport state object definition
 */
struct nameserver_remotenotify_obj {
	struct nameserver_remotenotify_message *msg[2];
	struct nameserver_remotenotify_params params;
	u16 region_id;
	u16 remote_proc_id;
	bool cache_enable;
	struct mutex *local_gate;
	void *gatemp;
	struct semaphore *sem_handle; /* Binary semaphore */
	u16 notify_event_id;
};

/*
 *  NameServer remote transport state object definition
 */
struct nameserver_remotenotify_object {
	int (*get)(void *,
		const char *instance_name, const char *name,
		void *value, u32 value_len, void *reserved);
	void *obj; /* Implementation specific object */
};

/*
 *  nameserver_remotenotify state object variable
 */
static struct nameserver_remotenotify_module_object
				nameserver_remotenotify_state = {
	.is_setup = false,
	.gate_handle = NULL,
	.def_cfg.notify_event_id = 1u,
	.def_inst_params.gatemp = NULL,
	.def_inst_params.shared_addr = NULL,
};

static void _nameserver_remotenotify_callback(u16 proc_id, u16 line_id,
					u32 event_id, uint *arg, u32 payload);

/*
 * This will get the default configuration for the  nameserver remote
 * module. This function can be called by the application to get their
 * configuration parameter to nameserver_remotenotify_setup filled
 * in by the nameserver_remotenotify module with the default
 * parameters. If the user does not wish to make any change in the
 * default parameters, this API is not required to be called
 */
void nameserver_remotenotify_get_config(
			struct nameserver_remotenotify_config *cfg)
{
	s32 retval = 0;

	if (WARN_ON(unlikely(cfg == NULL))) {
		retval = -EINVAL;
		goto exit;
	}

	if (nameserver_remotenotify_state.is_setup == false)
		memcpy(cfg, &(nameserver_remotenotify_state.def_cfg),
			sizeof(struct nameserver_remotenotify_config));
	else
		memcpy(cfg, &(nameserver_remotenotify_state.cfg),
			sizeof(struct nameserver_remotenotify_config));

exit:
	if (retval < 0) {
		printk(KERN_ERR "nameserver_remotenotify_get_config failed!"
			" retval = 0x%x", retval);
	}
	return;
}
EXPORT_SYMBOL(nameserver_remotenotify_get_config);

/*
 * This will setup the nameserver_remotenotify module
 * This function sets up the nameserver_remotenotify module. This
 * function must be called before any other instance-level APIs can
 * be invoked
 * Module-level configuration needs to be provided to this
 * function. If the user wishes to change some specific config
 * parameters, then nameserver_remotenotify_get_config can be called
 * to get the configuration filled with the default values. After
 * this, only the required configuration values can be changed. If
 * the user does not wish to make any change in the default
 * parameters, the application can simply call
 * nameserver_remotenotify_setup with NULL parameters. The default
 * parameters would get automatically used
 */
int nameserver_remotenotify_setup(struct nameserver_remotenotify_config *cfg)
{
	struct nameserver_remotenotify_config tmp_cfg;
	s32 retval = 0;
	struct mutex *lock = NULL;

	/* This sets the ref_count variable is not initialized, upper 16 bits is
	* written with module Id to ensure correctness of refCount variable.
	*/
	atomic_cmpmask_and_set(&nameserver_remotenotify_state.ref_count,
				NAMESERVERREMOTENOTIFY_MAKE_MAGICSTAMP(0),
				NAMESERVERREMOTENOTIFY_MAKE_MAGICSTAMP(0));
	if (atomic_inc_return(&nameserver_remotenotify_state.ref_count)
				!= NAMESERVERREMOTENOTIFY_MAKE_MAGICSTAMP(1)) {
		return 1;
	}

	if (cfg == NULL) {
		nameserver_remotenotify_get_config(&tmp_cfg);
		cfg = &tmp_cfg;
	}

	/* Create a default gate handle for local module protection */
	lock = kmalloc(sizeof(struct mutex), GFP_KERNEL);
	if (lock == NULL) {
		retval = -ENOMEM;
		goto exit;
	}
	mutex_init(lock);
	nameserver_remotenotify_state.gate_handle = lock;

	memcpy(&nameserver_remotenotify_state.cfg, cfg,
		sizeof(struct nameserver_remotenotify_config));
	memset(&nameserver_remotenotify_state.nsr_handles, 0,
		(sizeof(void *) * MULTIPROC_MAXPROCESSORS));
	nameserver_remotenotify_state.is_setup = true;
	return 0;

exit:
	printk(KERN_ERR "nameserver_remotenotify_setup failed! retval = 0x%x",
		retval);
	return retval;
}
EXPORT_SYMBOL(nameserver_remotenotify_setup);

/*
 * This will destroy the nameserver_remotenotify module.
 * Once this function is called, other nameserver_remotenotify
 * module APIs, except for the nameserver_remotenotify_get_config
 * API  cannot be called anymore.
 */
int nameserver_remotenotify_destroy(void)
{
	s32 retval = 0;

	if (WARN_ON(unlikely(atomic_cmpmask_and_lt(
			&(nameserver_remotenotify_state.ref_count),
			NAMESERVERREMOTENOTIFY_MAKE_MAGICSTAMP(0),
			NAMESERVERREMOTENOTIFY_MAKE_MAGICSTAMP(1)) == true))) {
		retval = -ENODEV;
		goto exit;
	}

	if (!(atomic_dec_return(&nameserver_remotenotify_state.ref_count)
			== NAMESERVERREMOTENOTIFY_MAKE_MAGICSTAMP(0))) {
		retval = 1;
		goto exit;
	}

	kfree(nameserver_remotenotify_state.gate_handle);

	nameserver_remotenotify_state.is_setup = false;
	return 0;

exit:
	printk(KERN_ERR "nameserver_remotenotify_destroy failed! retval = 0x%x",
		retval);
	return retval;
}
EXPORT_SYMBOL(nameserver_remotenotify_destroy);

/* This will get the current configuration values */
void nameserver_remotenotify_params_init(
				struct nameserver_remotenotify_params *params)
{
	if (WARN_ON(unlikely(atomic_cmpmask_and_lt(
			&(nameserver_remotenotify_state.ref_count),
			NAMESERVERREMOTENOTIFY_MAKE_MAGICSTAMP(0),
			NAMESERVERREMOTENOTIFY_MAKE_MAGICSTAMP(1)) == true))) {
		printk(KERN_ERR "nameserver_remotenotify_params_init failed: "
			"Module is not initialized!\n");
		return;
	}

	if (WARN_ON(unlikely(params == NULL))) {
		printk(KERN_ERR "nameserver_remotenotify_params_init failed: "
			"Argument of type(nameserver_remotenotify_params *) "
			"is NULL!\n");
		return;
	}

	memcpy(params, &(nameserver_remotenotify_state.def_inst_params),
		sizeof(struct nameserver_remotenotify_params));

}
EXPORT_SYMBOL(nameserver_remotenotify_params_init);

/* This will be called when a notify event is received */
static void _nameserver_remotenotify_callback(u16 proc_id, u16 line_id,
					u32 event_id, uint *arg, u32 payload)
{
	struct nameserver_remotenotify_obj *handle = NULL;
	u16  offset = 0;
	void *nshandle = NULL;
	u32 value_len;
	int *key;
	s32 retval = 0;

	if (WARN_ON(unlikely(atomic_cmpmask_and_lt(
			&(nameserver_remotenotify_state.ref_count),
			NAMESERVERREMOTENOTIFY_MAKE_MAGICSTAMP(0),
			NAMESERVERREMOTENOTIFY_MAKE_MAGICSTAMP(1)) == true))) {
		retval = -ENODEV;
		goto exit;
	}
	if (WARN_ON(unlikely(arg == NULL))) {
		retval = -EINVAL;
		goto exit;
	}
	if (WARN_ON(unlikely(proc_id >= multiproc_get_num_processors()))) {
		retval = -EINVAL;
		goto exit;
	}

	handle = (struct nameserver_remotenotify_obj *)arg;
	if ((multiproc_self() > proc_id))
		offset = 1;

#if 0
	if (handle->cache_enable) {
		/* write back shared memory that was modified */
		Cache_wbInv(handle->msg[0],
			sizeof(struct nameserver_remotenotify_message) << 1,
			Cache_Type_ALL, TRUE);
	}
#endif

	if (handle->msg[1 - offset]->request != true)
		goto signal_response;

	/* This is a request */
	value_len = handle->msg[1 - offset]->value_len;
	nshandle = nameserver_get_handle((const char *)
				handle->msg[1 - offset]->instance_name);
	if (nshandle != NULL) {
		/* Search for the NameServer entry */
		if (value_len == sizeof(u32)) {
			retval = nameserver_get_local_uint32(nshandle,
					(const char *) handle->msg[1 - offset]->
					name, &handle->msg[1 - offset]->value);
		} else {
			retval = nameserver_get_local(nshandle,
					(const char *) handle->msg[1 - offset]->
					name,
					&handle->msg[1 - offset]->value_buf,
					&value_len);
		}
	}
	BUG_ON(retval != 0 && retval != -ENOENT);

	key = gatemp_enter(handle->gatemp);
	if (retval == 0) {
		handle->msg[1 - offset]->request_status = true;
		handle->msg[1 - offset]->value_len = value_len;
	}
	/* Send a response back */
	handle->msg[1 - offset]->response = true;
	handle->msg[1 - offset]->request = false;

#if 0
	if (handle->cache_enable) {
		/* write back shared memory that was modified */
		Cache_wbInv(handle->msg[1 - offset],
			sizeof(struct nameserver_remotenotify_message),
			Cache_Type_ALL, TRUE);
	}
#endif
	/* now we can leave the gate */
	gatemp_leave(handle->gatemp, key);

	/*
	*  The NotifyDriver handle must exists at this point,
	*  otherwise the notify_sendEvent should have failed
	*/
	retval = notify_send_event(handle->remote_proc_id, 0,
			(handle->notify_event_id  | (NOTIFY_SYSTEMKEY << 16)),
			0xCBC7, false);

signal_response:
	if (handle->msg[offset]->response == true)
		up(handle->sem_handle);

exit:
	if (retval < 0) {
		printk(KERN_ERR "nameserver_remotenotify_callback failed! "
			"status = 0x%x\n", retval);
	}
	return;
}

/* This will get a remote name value pair */
int nameserver_remotenotify_get(void *rhandle, const char *instance_name,
				const char *name, void *value, u32 *value_len,
				void *reserved)
{
	struct nameserver_remotenotify_object *handle = NULL;
	struct nameserver_remotenotify_obj *obj = NULL;
	s32 offset = 0;
	s32 len;
	int *key;
	s32 retval = 0;

	if (WARN_ON(unlikely(atomic_cmpmask_and_lt(
			&(nameserver_remotenotify_state.ref_count),
			NAMESERVERREMOTENOTIFY_MAKE_MAGICSTAMP(0),
			NAMESERVERREMOTENOTIFY_MAKE_MAGICSTAMP(1)) == true))) {
		retval = -ENODEV;
		goto exit;
	}
	if (WARN_ON(unlikely(rhandle == NULL))) {
		retval = -EINVAL;
		goto exit;
	}
	if (WARN_ON(unlikely(instance_name == NULL))) {
		retval = -EINVAL;
		goto exit;
	}
	if (WARN_ON(unlikely(name == NULL))) {
		retval = -EINVAL;
		goto exit;
	}
	if (WARN_ON(unlikely(value == NULL))) {
		retval = -EINVAL;
		goto exit;
	}
	if (WARN_ON(unlikely(value_len == NULL))) {
		retval = -EINVAL;
		goto exit;
	}
	if (WARN_ON(unlikely((*value_len == 0) || \
		(*value_len > NAMESERVERREMOTENOTIFY_MAXVALUEBUFLEN)))) {
		retval = -EINVAL;
		goto exit;
	}

	handle = (struct nameserver_remotenotify_object *)rhandle;
	obj = (struct nameserver_remotenotify_obj *)handle->obj;
	if (multiproc_self() > obj->remote_proc_id)
		offset = 1;

#if 0
	if (obj->cache_enable) {
		/* write back shared memory that was modified */
		Cache_wbInv(obj->msg[offset],
			sizeof(struct nameserver_remotenotify_message),
			Cache_Type_ALL, TRUE);
	}
#endif
	/* Allow only one request to be procesed at a time */
	retval = mutex_lock_interruptible(obj->local_gate);
	if (retval)
		goto exit;

	key = gatemp_enter(obj->gatemp);
	/* This is a request message */
	obj->msg[offset]->request = 1;
	obj->msg[offset]->response = 0;
	obj->msg[offset]->request_status = 0;
	obj->msg[offset]->value_len = *value_len;
	len = strlen(instance_name) + 1; /* Take termination null char */
	if (len >= 32) {
		retval = -EINVAL;
		goto inval_len_error;
	}
	strncpy((char *)obj->msg[offset]->instance_name, instance_name, len);
	len = strlen(name) + 1;
	if (len >= 32) {
		retval = -EINVAL;
		goto inval_len_error;
	}
	strncpy((char *)obj->msg[offset]->name, name, len);

	/* Send the notification to remote processor */
	retval = notify_send_event(obj->remote_proc_id, 0,
			(obj->notify_event_id  | (NOTIFY_SYSTEMKEY << 16)),
			0x8307, /* Payload */
			false); /* Not sending a payload */
	if (retval < 0) {
		/* Undo previous operations */
		obj->msg[offset]->request = 0;
		obj->msg[offset]->value_len = 0;
		goto notify_error;
	}
	gatemp_leave(obj->gatemp, key);

	/* Pend on the semaphore */
	retval = down_interruptible(obj->sem_handle);
	if (retval)
		goto down_error;

	key = gatemp_enter(obj->gatemp);

	if (obj->cache_enable) {
#if 0
		/* write back shared memory that was modified */
		Cache_wbInv(obj->msg[offset],
			sizeof(struct nameserver_remotenotify_message),
			Cache_Type_ALL, TRUE);
#endif
	}
	if (obj->msg[offset]->request_status != true) {
		retval = -ENOENT;
		goto request_error;
	}

	if (obj->msg[offset]->value_len == sizeof(u32))
		memcpy((void *)value, (void *) &(obj->msg[offset]->value),
			sizeof(u32));
	else
		memcpy((void *)value, (void *)&(obj->msg[offset]->value_buf),
			obj->msg[offset]->value_len);
	*value_len = obj->msg[offset]->value_len;

	obj->msg[offset]->request_status = false;
	retval = 0;

inval_len_error:
notify_error:
request_error:
	obj->msg[offset]->request = 0;
	obj->msg[offset]->response = 0;
	gatemp_leave(obj->gatemp, key);
down_error:
	/* un-block so that subsequent requests can be honored */
	mutex_unlock(obj->local_gate);
exit:
	if (retval < 0)
		printk(KERN_ERR "nameserver_remotenotify_get failed! "
			"status = 0x%x", retval);
	return retval;
}
EXPORT_SYMBOL(nameserver_remotenotify_get);

/* This will setup the nameserver remote module */
void *nameserver_remotenotify_create(u16 remote_proc_id,
			const struct nameserver_remotenotify_params *params)
{
	struct nameserver_remotenotify_object *handle = NULL;
	struct nameserver_remotenotify_obj *obj = NULL;
	u32 offset = 0;
	s32 retval = 0;
	s32 retval1 = 0;

	if (WARN_ON(unlikely(atomic_cmpmask_and_lt(
			&(nameserver_remotenotify_state.ref_count),
			NAMESERVERREMOTENOTIFY_MAKE_MAGICSTAMP(0),
			NAMESERVERREMOTENOTIFY_MAKE_MAGICSTAMP(1)) == true))) {
		retval = -ENODEV;
		goto exit;
	}
	if (WARN_ON(unlikely((remote_proc_id == multiproc_self()) &&
			(remote_proc_id >= multiproc_get_num_processors())))) {
		retval = -EINVAL;
		goto exit;
	}
	if (WARN_ON(unlikely(params == NULL))) {
		retval = -EINVAL;
		goto exit;
	}
	if (WARN_ON(unlikely(params->shared_addr == NULL))) {
		retval = -EINVAL;
		goto exit;
	}

	obj = kzalloc(sizeof(struct nameserver_remotenotify_obj), GFP_KERNEL);
	handle = kmalloc(sizeof(struct nameserver_remotenotify_object),
								GFP_KERNEL);
	if (obj == NULL || handle == NULL) {
		retval = -ENOMEM;
		goto mem_error;
	}

	handle->get = (void *)&nameserver_remotenotify_get;
	handle->obj = (void *)obj;
	obj->local_gate = kmalloc(sizeof(struct mutex), GFP_KERNEL);
	if (obj->local_gate == NULL) {
		retval = -ENOMEM;
		goto mem_error;
	}

	obj->remote_proc_id = remote_proc_id;
	if (multiproc_self() > remote_proc_id)
		offset = 1;

	obj->region_id = sharedregion_get_id(params->shared_addr);
	if (((u32) params->shared_addr % sharedregion_get_cache_line_size(
		obj->region_id)) != 0) {
		retval = -EFAULT;
		goto notify_error;
	}

	obj->msg[0] = (struct nameserver_remotenotify_message *)
				(params->shared_addr);
	obj->msg[1] = (struct nameserver_remotenotify_message *)
				((u32)obj->msg[0] +
				sizeof(struct nameserver_remotenotify_message));
	obj->gatemp = params->gatemp;
	obj->remote_proc_id = remote_proc_id;
	obj->notify_event_id = \
			nameserver_remotenotify_state.cfg.notify_event_id;
	/* Clear out self shared structures */
	memset(obj->msg[offset], 0,
			sizeof(struct nameserver_remotenotify_message));
	memcpy((void *)&obj->params, (void *)params,
				sizeof(struct nameserver_remotenotify_params));

	/* determine cacheability of the object from the regionId */
	obj->cache_enable = sharedregion_is_cache_enabled(obj->region_id);
	if (obj->cache_enable) {
#if 0
		/* write back shared memory that was modified */
		Cache_wbInv(obj->msg[offset],
			sizeof(struct nameserver_remotenotify_message),
			Cache_Type_ALL, TRUE);
#endif
	}

	retval = notify_register_event_single(remote_proc_id,
					0, /* TBD: Interrupt line id */
					(obj->notify_event_id | \
						(NOTIFY_SYSTEMKEY << 16)),
					_nameserver_remotenotify_callback,
					(void *)obj);
	if (retval < 0)
		goto notify_error;

	retval = nameserver_register_remote_driver((void *)handle,
							remote_proc_id);
	obj->sem_handle = kzalloc(sizeof(struct semaphore), GFP_KERNEL);
	if (obj->sem_handle == NULL) {
		retval = -ENOMEM;
		goto sem_alloc_error;
	}

	sema_init(obj->sem_handle, 0);
	/* its is at the end since its init state = unlocked? */
	mutex_init(obj->local_gate);
	return (void *)handle;

sem_alloc_error:
	nameserver_unregister_remote_driver(remote_proc_id);
	/* Do we want to check the staus ? */
	retval1 = notify_unregister_event_single(obj->remote_proc_id, 0,
			(obj->notify_event_id | (NOTIFY_SYSTEMKEY << 16)));

notify_error:
	kfree(obj->local_gate);

mem_error:
	kfree(obj);
	kfree(handle);

exit:
	printk(KERN_ERR "nameserver_remotenotify_create failed! "
		"status = 0x%x\n", retval);
	return NULL;
}
EXPORT_SYMBOL(nameserver_remotenotify_create);

/* This will delete the nameserver remote transport instance */
int nameserver_remotenotify_delete(void **rhandle)
{
	struct nameserver_remotenotify_object *handle = NULL;
	struct nameserver_remotenotify_obj *obj = NULL;
	s32 retval = 0;
	s32 retval1 = 0;
	struct mutex *gate = NULL;

	if (WARN_ON(unlikely(atomic_cmpmask_and_lt(
			&(nameserver_remotenotify_state.ref_count),
			NAMESERVERREMOTENOTIFY_MAKE_MAGICSTAMP(0),
			NAMESERVERREMOTENOTIFY_MAKE_MAGICSTAMP(1)) == true))) {
		retval = -ENODEV;
		goto exit;
	}
	if (WARN_ON(unlikely((rhandle == NULL) || (*rhandle == NULL)))) {
		retval = -EINVAL;
		goto exit;
	}

	handle = (struct nameserver_remotenotify_object *)(*rhandle);
	obj = (struct nameserver_remotenotify_obj *)handle->obj;
	if (obj == NULL) {
		retval = -EINVAL;
		goto free_handle;
	}

	gate = obj->local_gate;
	retval = mutex_lock_interruptible(gate);
	if (retval)
		goto free_handle;

	kfree(obj->sem_handle);
	obj->sem_handle = NULL;

	retval1 = nameserver_unregister_remote_driver(obj->remote_proc_id);
	/* Do we have to bug_on/warn_on oops here intead of exit ?*/
	if (retval1 < 0 && retval >= 0)
		retval = retval1;

	/* Unregister the event from Notify */
	retval1 = notify_unregister_event_single(obj->remote_proc_id, 0,
			(obj->notify_event_id | (NOTIFY_SYSTEMKEY << 16)));
	if (retval1 < 0 && retval >= 0)
		retval = retval1;
	kfree(obj);
	mutex_unlock(gate);
	kfree(gate);

free_handle:
	kfree(handle);
	*rhandle = NULL;

exit:
	if (retval < 0) {
		printk(KERN_ERR "nameserver_remotenotify_delete failed! "
			"status = 0x%x\n", retval);
	}
	return retval;
}
EXPORT_SYMBOL(nameserver_remotenotify_delete);

/* This will give shared memory requirements for the
 * nameserver remote transport instance */
uint nameserver_remotenotify_shared_mem_req(const
			struct nameserver_remotenotify_params *params)
{
	uint total_size = 0;

	/* params is not used- to remove warning. */
	(void)params;

	/* Two Message structs are required. One for sending request and
	 * another one for sending response. */
	if (multiproc_get_num_processors() > 1)
		total_size = \
			(2 * sizeof(struct nameserver_remotenotify_message));

	return total_size;
}
EXPORT_SYMBOL(nameserver_remotenotify_shared_mem_req);

int nameserver_remotenotify_attach(u16 remote_proc_id, void *shared_addr)
{
	struct nameserver_remotenotify_params nsr_params;
	s32 retval = 0;

	if (WARN_ON(unlikely(atomic_cmpmask_and_lt(
			&(nameserver_remotenotify_state.ref_count),
			NAMESERVERREMOTENOTIFY_MAKE_MAGICSTAMP(0),
			NAMESERVERREMOTENOTIFY_MAKE_MAGICSTAMP(1)) == true))) {
		retval = -ENODEV;
		goto exit;
	}
	if (WARN_ON(unlikely(shared_addr == NULL))) {
		retval = -EINVAL;
		goto exit;
	}
	if (WARN_ON(unlikely(gatemp_get_default_remote() == NULL))) {
		retval = -1;
		goto exit;
	}

	/* Use default GateMP */
	nameserver_remotenotify_params_init(&nsr_params);
	nsr_params.gatemp = gatemp_get_default_remote();
	nsr_params.shared_addr = shared_addr;

	/* create only if notify driver has been created to remote proc */
	if (!notify_is_registered(remote_proc_id, 0)) {
		retval = -1;
		goto exit;
	}

	nameserver_remotenotify_state.nsr_handles[remote_proc_id] =
		nameserver_remotenotify_create(remote_proc_id, &nsr_params);
	if (nameserver_remotenotify_state.nsr_handles[remote_proc_id] == NULL) {
			retval = -1;
			goto exit;
	}
	return 0;

exit:
	printk(KERN_ERR "nameserver_remotenotify_attach failed! status = 0x%x",
		retval);
	return retval;
}

static void *_nameserver_remotenotify_get_handle(u16 remote_proc_id)
{
	void *handle = NULL;

	if (remote_proc_id <= multiproc_get_num_processors()) {
		handle = \
		nameserver_remotenotify_state.nsr_handles[remote_proc_id];
	}

	return handle;
};


int nameserver_remotenotify_detach(u16 remote_proc_id)
{
	void *handle = NULL;
	int retval = 0;

	if (WARN_ON(unlikely(atomic_cmpmask_and_lt(
			&(nameserver_remotenotify_state.ref_count),
			NAMESERVERREMOTENOTIFY_MAKE_MAGICSTAMP(0),
			NAMESERVERREMOTENOTIFY_MAKE_MAGICSTAMP(1)) == true))) {
		retval = -ENODEV;
		goto exit;
	}

	handle = _nameserver_remotenotify_get_handle(remote_proc_id);
	if (handle == NULL) {
		retval = -1;
		goto exit;
	}

	nameserver_remotenotify_delete(&handle);
	nameserver_remotenotify_state.nsr_handles[remote_proc_id] = NULL;
	return 0;

exit:
	printk(KERN_ERR "nameserver_remotenotify_detach failed! status = 0x%x",
		retval);
	return retval;
}
