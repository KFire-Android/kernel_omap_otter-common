/*
 *  listmp.c
 *
 *  The listmp is a linked-list based module designed to be
 *  used in a multi-processor environment.  It is designed to
 *  provide a means of communication between different processors.
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
#include <linux/module.h>

/* Utilities headers */
#include <linux/list.h>
#include <linux/slab.h>
#include <linux/string.h>

/* Syslink headers */
#include <syslink/atomic_linux.h>

/* Module level headers */
#include <multiproc.h>
#include <nameserver.h>
#include <sharedregion.h>
#include <gatemp.h>
#include "_listmp.h"
#include <listmp.h>


/* =============================================================================
 * Globals
 * =============================================================================
 */
/* Macro to make a correct module magic number with ref_count */
#define LISTMP_MAKE_MAGICSTAMP(x)	((LISTMP_MODULEID << 12u) | (x))

/* Name of the reserved NameServer used for listmp. */
#define LISTMP_NAMESERVER		"ListMP"

#define ROUND_UP(a, b)			(((a) + ((b) - 1)) & (~((b) - 1)))

/* =============================================================================
 * Structures and Enums
 * =============================================================================
 */
/* structure for listmp module state */
struct listmp_module_object {
	atomic_t ref_count;
	/* Reference count */
	void *ns_handle;
	/* Handle to the local NameServer used for storing listmp objects */
	struct list_head obj_list;
	/* List holding created listmp objects */
	struct mutex *local_lock;
	/* Handle to lock for protecting obj_list */
	struct listmp_config cfg;
	/* Current config values */
	struct listmp_config default_cfg;
	/* Default config values */
	struct listmp_params default_inst_params;
	/* Default instance creation parameters */
};

/* Structure for the internal Handle for the listmp. */
struct listmp_object{
	struct list_head list_elem;
	/* Used for creating a linked list */
	VOLATILE struct listmp_attrs *attrs;
	/* Shared memory attributes */
	void *ns_key;
	/* nameserver key  required for remove */
	void *gatemp_handle;
	/* Gate for critical regions */
	u32 alloc_size;
	/* Shared memory allocated */
	u16 region_id;
	/* SharedRegion ID */
	bool cache_enabled;
	/* Whether to do cache calls */
	struct listmp_proc_attrs owner;
	/* Creator's attributes associated with an instance */
	struct listmp_params params;
	/* the parameter structure */
	void *top;
	/* Pointer to the top Object */
};

/* =============================================================================
 * Globals
 * =============================================================================
 */
/* Variable for holding state of the nameserver module. */
static struct listmp_module_object listmp_state = {
			.default_cfg.max_runtime_entries = 32,
			.default_cfg.max_name_len = 32,
			.default_inst_params.shared_addr = NULL,
			.default_inst_params.name = NULL,
			.default_inst_params.gatemp_handle = NULL,
			.default_inst_params.region_id = 0,
};

/* Pointer to the listmp module state */
static struct listmp_module_object *listmp_module = &listmp_state;

/* =============================================================================
 * Function definitions
 * =============================================================================
 */
/* Creates a new instance of listmp module. This is an internal
 * function because both listmp_create and
 * listmp_open call use the same functionality. */
static int _listmp_create(struct listmp_object **handle_ptr,
				struct listmp_params *params, u32 create_flag);


/* =============================================================================
 * Function API's
 * =============================================================================
 */
/* Function to get configuration parameters to setup the listmp module. */
void listmp_get_config(struct listmp_config *cfg_params)
{
	int status = 0;

	if (WARN_ON(unlikely(cfg_params == NULL))) {
		status = -EINVAL;
		goto exit;
	}

	if (atomic_cmpmask_and_lt(&(listmp_module->ref_count),
			LISTMP_MAKE_MAGICSTAMP(0),
			LISTMP_MAKE_MAGICSTAMP(1)) == true) {
		/* If setup has not yet been called) */
		memcpy(cfg_params, &listmp_module->default_cfg,
			sizeof(struct listmp_config));
	} else {
		memcpy(cfg_params, &listmp_module->cfg,
			sizeof(struct listmp_config));
	}

exit:
	if (status < 0)
		pr_err("listmp_get_config failed: status = 0x%x\n", status);
	return;
}

/* Function to setup the listmp module. */
int listmp_setup(const struct listmp_config *cfg)
{
	int status = 0;
	int status1 = 0;
	void *nshandle = NULL;
	struct nameserver_params params;
	struct listmp_config tmp_cfg;

	/* This sets the ref_count variable if not initialized, upper 16 bits is
	 * written with module Id to ensure correctness of ref_count variable.
	 */
	atomic_cmpmask_and_set(&listmp_module->ref_count,
			LISTMP_MAKE_MAGICSTAMP(0),
			LISTMP_MAKE_MAGICSTAMP(0));
	if (atomic_inc_return(&listmp_module->ref_count)
				!= LISTMP_MAKE_MAGICSTAMP(1)) {
		return 1;
	}

	if (cfg == NULL) {
		listmp_get_config(&tmp_cfg);
		cfg = &tmp_cfg;
	}

	if (WARN_ON(cfg->max_name_len == 0)) {
		status = -EINVAL;
		goto exit;
	}

	/* Initialize the parameters */
	nameserver_params_init(&params);
	params.max_value_len = 4;
	params.max_name_len = cfg->max_name_len;
	/* Create the nameserver for modules */
	nshandle = nameserver_create(LISTMP_NAMESERVER, &params);
	if (unlikely(nshandle == NULL)) {
		status = LISTMP_E_FAIL;
		goto exit;
	}
	listmp_module->ns_handle = nshandle;

	/* Construct the list object */
	INIT_LIST_HEAD(&listmp_module->obj_list);
	/* Create a lock for protecting list object */
	listmp_module->local_lock = kmalloc(sizeof(struct mutex), GFP_KERNEL);
	if (listmp_module->local_lock == NULL) {
		status = -ENOMEM;
		goto clean_nameserver;
	}
	mutex_init(listmp_module->local_lock);

	/* Copy the cfg */
	memcpy(&listmp_module->cfg, cfg, sizeof(struct listmp_config));
	return 0;

clean_nameserver:
	status1 = nameserver_delete(&(listmp_module->ns_handle));
	WARN_ON(status1 < 0);
	atomic_set(&listmp_module->ref_count, LISTMP_MAKE_MAGICSTAMP(0));
exit:
	pr_err("listmp_setup failed! status = 0x%x\n", status);
	return status;
}

/* Function to destroy the listmp module. */
int listmp_destroy(void)
{
	int status = 0;
	int status1 = 0;
	struct list_head *elem = NULL;
	struct list_head *head = &listmp_module->obj_list;
	struct list_head *next;

	if (WARN_ON(unlikely(atomic_cmpmask_and_lt(&(listmp_module->ref_count),
				LISTMP_MAKE_MAGICSTAMP(0),
				LISTMP_MAKE_MAGICSTAMP(1)) == true))) {
		status = -ENODEV;
		goto exit;
	}

	if (!(atomic_dec_return(&listmp_module->ref_count) == \
				LISTMP_MAKE_MAGICSTAMP(0))) {
		status = 1;
		goto exit;
	}

	/* Temporarily increment ref_count here. */
	atomic_set(&listmp_module->ref_count, LISTMP_MAKE_MAGICSTAMP(1));
	/* Check if any listmp instances have not been
	 * deleted so far. If not, delete them. */
	for (elem = (head)->next; elem != (head); elem = next) {
		/* Retain the next pointer so it doesn't get overwritten */
		next = elem->next;
		if (((struct listmp_object *) elem)->owner.proc_id == \
			multiproc_self()) {
			status1 = listmp_delete((void **)
					&(((struct listmp_object *)elem)->top));
			WARN_ON(status1 < 0);
		} else {
			status1 = listmp_close((void **)
					&(((struct listmp_object *)elem)->top));
			WARN_ON(status1 < 0);
		}
	}

	if (likely(listmp_module->ns_handle != NULL)) {
		/* Delete the nameserver for modules */
		status = nameserver_delete(&(listmp_module->ns_handle));
		WARN_ON(status < 0);
	}

	/* Destruct the list object */
	list_del(&listmp_module->obj_list);
	/* Delete the list lock */
	kfree(listmp_module->local_lock);
	listmp_module->local_lock = NULL;

	memset(&listmp_module->cfg, 0, sizeof(struct listmp_config));

	/* Again reset ref_count. */
	atomic_set(&listmp_module->ref_count, LISTMP_MAKE_MAGICSTAMP(0));

exit:
	if (status < 0)
		pr_err("listmp_destroy failed! status = 0x%x\n", status);
	return status;
}

/* Function to initialize the config-params structure with supplier-specified
 * defaults before instance creation. */
void listmp_params_init(struct listmp_params *params)
{
	s32 status = 0;

	if (WARN_ON(unlikely(atomic_cmpmask_and_lt(&(listmp_module->ref_count),
				LISTMP_MAKE_MAGICSTAMP(0),
				LISTMP_MAKE_MAGICSTAMP(1)) == true))) {
		status = -ENODEV;
		goto exit;
	}
	if (WARN_ON(unlikely(params == NULL))) {
		status = -EINVAL;
		goto exit;
	}

	memcpy((void *)params, (void *)&listmp_module->default_inst_params,
		sizeof(struct listmp_params));

exit:
	if (status < 0)
		pr_err("listmp_params_init failed! status = 0x%x\n", status);
	return;
}

/* Creates a new instance of listmp module. */
void *listmp_create(const struct listmp_params *params)
{
	s32 status = 0;
	struct listmp_object *obj = NULL;
	struct listmp_params sparams;
	u32 key;

	if (WARN_ON(unlikely(atomic_cmpmask_and_lt(&(listmp_module->ref_count),
				LISTMP_MAKE_MAGICSTAMP(0),
				LISTMP_MAKE_MAGICSTAMP(1)) == true))) {
		status = -ENODEV;
		goto exit;
	}
	if (WARN_ON(unlikely(params == NULL))) {
		status = -EINVAL;
		goto exit;
	}

	memcpy(&sparams, params, sizeof(struct listmp_params));

	key = mutex_lock_interruptible(listmp_module->local_lock);
	if (key)
		goto exit;
	status = _listmp_create(&obj, &sparams, (u32) true);
	mutex_unlock(listmp_module->local_lock);

exit:
	if (status < 0)
		pr_err("listmp_create failed! status = 0x%x\n", status);
	return (void *)obj;
}

/* Deletes a instance of listmp instance object. */
int listmp_delete(void **listmp_handleptr)
{
	int status = 0;
	struct listmp_object *obj = NULL;
	struct listmp_params *params = NULL;
	u32 key;

	if (WARN_ON(unlikely(atomic_cmpmask_and_lt(&(listmp_module->ref_count),
				LISTMP_MAKE_MAGICSTAMP(0),
				LISTMP_MAKE_MAGICSTAMP(1)) == true))) {
		status = -ENODEV;
		goto exit;
	}
	if (WARN_ON(unlikely(listmp_handleptr == NULL))) {
		status = -EINVAL;
		goto exit;
	}
	if (WARN_ON(unlikely(*listmp_handleptr == NULL))) {
		status = -EINVAL;
		goto exit;
	}

	obj = (struct listmp_object *)(*listmp_handleptr);
	params = (struct listmp_params *)&obj->params;

	if (unlikely(obj->owner.proc_id != multiproc_self())) {
		status = -ENODEV;
		goto exit;
	}
	if (unlikely(obj->owner.open_count > 1)) {
		status = -ENODEV;
		goto exit;
	}
	if (unlikely(obj->owner.open_count != 1)) {
		status = -ENODEV;
		goto exit;
	}

	/* Remove from  the local list */
	key = mutex_lock_interruptible(listmp_module->local_lock);
	list_del(&obj->list_elem);
	mutex_unlock(listmp_module->local_lock);

	if (likely(params->name != NULL)) {
		/* Free memory for the name */
		kfree(params->name);
		/* Remove from the name server */
		if (obj->ns_key != NULL) {
			nameserver_remove_entry(listmp_module->ns_handle,
						obj->ns_key);
			obj->ns_key = NULL;
		}
	}

	/* Now free the obj */
	kfree(obj);
	obj = NULL;
	*listmp_handleptr = NULL;

exit:
	if (status < 0)
		pr_err("listmp_delete failed! status = 0x%x\n", status);
	return status;
}

/* Function to open a listmp instance */
int listmp_open(char *name, void **listmp_handleptr)
{
	int status = 0;
	void *shared_addr = NULL;
	bool done_flag = false;
	struct list_head *elem;
	u32 key;
	u32 shared_shm_base;

	if (WARN_ON(unlikely(atomic_cmpmask_and_lt(&(listmp_module->ref_count),
				LISTMP_MAKE_MAGICSTAMP(0),
				LISTMP_MAKE_MAGICSTAMP(1)) == true))) {
		status = -ENODEV;
		goto exit;
	}
	if (WARN_ON(unlikely(listmp_handleptr == NULL))) {
		status = -EINVAL;
		goto exit;
	}
	if (WARN_ON(unlikely(name == NULL))) {
		status = -EINVAL;
		goto exit;
	}

	/* First check in the local list */
	list_for_each(elem, &listmp_module->obj_list) {
		if (((struct listmp_object *)elem)->params.name != NULL) {
			if (strcmp(((struct listmp_object *)elem)->params.name,
				name) == 0) {
				key = mutex_lock_interruptible(
					listmp_module->local_lock);
				if (((struct listmp_object *)elem)
					->owner.proc_id == multiproc_self())
					((struct listmp_object *)elem)
						->owner.open_count++;
				mutex_unlock(listmp_module->local_lock);
				*listmp_handleptr = \
					(((struct listmp_object *)elem)->top);
				done_flag = true;
				break;
			}
		}
	}

	if (likely(done_flag == false)) {
		/* Find in name server */
		status = nameserver_get_uint32(listmp_module->ns_handle,
			name, &shared_shm_base, NULL);
		if (unlikely(status < 0)) {
			status = ((status == -ENOENT) ? status : -1);
			goto exit;
		}
		shared_addr = sharedregion_get_ptr((u32 *)shared_shm_base);
		if (unlikely(shared_addr == NULL)) {
			status = LISTMP_E_FAIL;
			goto exit;
		}
		status = listmp_open_by_addr(shared_addr, listmp_handleptr);
	}

#if 0
	if (status >= 0) {
		attrs = (struct listmp_attrs *) (params->shared_addr);
		if (unlikely(attrs->status != (LISTMP_CREATED)))
				status = LISTMP_E_NOTCREATED;
		else if (unlikely(attrs->version !=
					(LISTMP_VERSION)))
				status = LISTMP_E_VERSION;
	}

	if (likely(status >= 0))
		*listmp_handleptr = (listmp_handle)
				_listmp_create(params, false);
#endif

exit:
	if (status < 0)
		pr_err("listmp_open failed! status = 0x%x\n", status);
	return status;
}

/* Function to open a listmp instance by address */
int listmp_open_by_addr(void *shared_addr, void **listmp_handleptr)
{
	int status = 0;
	bool done_flag = false;
	struct listmp_params params;
	struct list_head *elem;
	u32 key;
	struct listmp_attrs *attrs;
	u16 id;

	if (WARN_ON(unlikely(atomic_cmpmask_and_lt(&(listmp_module->ref_count),
				LISTMP_MAKE_MAGICSTAMP(0),
				LISTMP_MAKE_MAGICSTAMP(1)) == true))) {
		status = -ENODEV;
		goto exit;
	}
	if (WARN_ON(listmp_handleptr == NULL)) {
		status = -EINVAL;
		goto exit;
	}
	if (WARN_ON(shared_addr == NULL)) {
		status = -EINVAL;
		goto exit;
	}

	/* First check in the local list */
	list_for_each(elem, &listmp_module->obj_list) {
		if (((struct listmp_object *)elem)->params.shared_addr == \
			shared_addr) {
			key = mutex_lock_interruptible(
						listmp_module->local_lock);
			if (((struct listmp_object *)elem)->owner.proc_id == \
				multiproc_self())
				((struct listmp_object *)elem)
							->owner.open_count++;
			mutex_unlock(listmp_module->local_lock);
			*listmp_handleptr = \
				(((struct listmp_object *)elem)->top);
			done_flag = true;
			break;
		}
	}

	if (likely(done_flag == false)) {
		listmp_params_init(&params);
		params.shared_addr = shared_addr;

		attrs = (struct listmp_attrs *)(shared_addr);
		id = sharedregion_get_id(shared_addr);
#if 0
		if (sharedregion_is_cache_enabled(id))
			Cache_inv(in_use, num * sizeof(u8), Cache_Type_ALL,
				true);
#endif
		if (unlikely(attrs->status != LISTMP_CREATED)) {
			*listmp_handleptr = NULL;
			status = -ENOENT;
		} else {
			key = mutex_lock_interruptible(
				listmp_module->local_lock);
			status = _listmp_create((struct listmp_object **)
						listmp_handleptr, &params,
						(u32) false);
			mutex_unlock(listmp_module->local_lock);
		}
	}

exit:
	if (status < 0)
		pr_err("listmp_open failed! status = 0x%x\n", status);
	return status;
}

/* Function to close a previously opened instance */
int listmp_close(void **listmp_handleptr)
{
	int status = 0;
	struct listmp_object *obj = NULL;
	struct listmp_params *params = NULL;
	u32 key;

	if (WARN_ON(unlikely(atomic_cmpmask_and_lt(&(listmp_module->ref_count),
				LISTMP_MAKE_MAGICSTAMP(0),
				LISTMP_MAKE_MAGICSTAMP(1)) == true))) {
		status = -ENODEV;
		goto exit;
	}
	if (WARN_ON(unlikely(listmp_handleptr == NULL))) {
		status = -EINVAL;
		goto exit;
	}
	if (WARN_ON(unlikely(*listmp_handleptr == NULL))) {
		status = -EINVAL;
		goto exit;
	}

	obj = (struct listmp_object *)(*listmp_handleptr);
	params = (struct listmp_params *)&obj->params;

	key = mutex_lock_interruptible(listmp_module->local_lock);
	if (unlikely(obj->owner.proc_id == multiproc_self()))
		(obj)->owner.open_count--;

	/* Check if ListMP is opened on same processor*/
	if (likely((((struct listmp_object *)obj)->owner.creator == false))) {
		list_del(&obj->list_elem);
		/* remove from the name server */
		if (params->name != NULL)
			/* Free memory for the name */
			kfree(params->name);
		gatemp_close(&obj->gatemp_handle);

		kfree(obj);
		obj = NULL;
		*listmp_handleptr = NULL;
	}

	mutex_unlock(listmp_module->local_lock);

exit:
	if (status < 0)
		pr_err("listmp_close failed! status = 0x%x\n", status);
	return status;
}

/* Function to check if the shared memory list is empty */
bool listmp_empty(void *listmp_handle)
{
	int status = 0;
	bool is_empty = false;
	struct listmp_object *obj = NULL;
	int *key;
	struct listmp_elem *shared_head;

	if (WARN_ON(unlikely(atomic_cmpmask_and_lt(&(listmp_module->ref_count),
				LISTMP_MAKE_MAGICSTAMP(0),
				LISTMP_MAKE_MAGICSTAMP(1)) == true))) {
		status = -ENODEV;
		goto exit;
	}
	if (WARN_ON(unlikely(listmp_handle == NULL))) {
		status = -EINVAL;
		goto exit;
	}

	obj = (struct listmp_object *)listmp_handle;
	key = gatemp_enter(obj->gatemp_handle);
#if 0
	if (unlikely(obj->cache_enabled)) {
		Cache_inv((void *)&(obj->attrs->head),
			sizeof(struct listmp_elem), Cache_Type_ALL, true);
	}
#endif

	/*  true if list is empty */
	shared_head = (struct listmp_elem *)(sharedregion_get_srptr(
				(void *)&(obj->attrs->head), obj->region_id));
	dsb();
	if (obj->attrs->head.next == shared_head)
		is_empty = true;

	gatemp_leave(obj->gatemp_handle, key);

exit:
	return is_empty;
}

/* Retrieves the gatemp handle associated with the listmp instance. */
void *listmp_get_gate(void *listmp_handle)
{
	struct listmp_object *obj = NULL;
	void *gatemp_handle = NULL;
	s32 retval = 0;

	if (WARN_ON(unlikely(atomic_cmpmask_and_lt(&(listmp_module->ref_count),
				LISTMP_MAKE_MAGICSTAMP(0),
				LISTMP_MAKE_MAGICSTAMP(1)) == true))) {
		retval = -ENODEV;
		goto exit;
	}
	if (WARN_ON(unlikely(listmp_handle == NULL))) {
		retval = -EINVAL;
		goto exit;
	}

	obj = (struct listmp_object *)listmp_handle;
	gatemp_handle = obj->gatemp_handle;

exit:
	if (retval < 0)
		pr_err("listmp_get_gate failed! status = 0x%x", retval);
	return gatemp_handle;
}

/* Function to get head element from a shared memory list */
void *listmp_get_head(void *listmp_handle)
{
	struct listmp_object *obj = NULL;
	struct listmp_elem *elem = NULL;
	struct listmp_elem *local_head_next = NULL;
	struct listmp_elem *local_next = NULL;
	s32 retval = 0;
	int *key = NULL;

	if (WARN_ON(unlikely(atomic_cmpmask_and_lt(&(listmp_module->ref_count),
				LISTMP_MAKE_MAGICSTAMP(0),
				LISTMP_MAKE_MAGICSTAMP(1)) == true))) {
		retval = -ENODEV;
		goto exit;
	}
	if (WARN_ON(unlikely(listmp_handle == NULL))) {
		retval = -EINVAL;
		goto exit;
	}

	obj = (struct listmp_object *)listmp_handle;
	key = gatemp_enter(obj->gatemp_handle);
#if 0
	if (unlikely(obj->cache_enabled)) {
		Cache_inv((void *)&(obj->attrs->head),
			sizeof(struct listmp_elem), Cache_Type_ALL, true);
	}
#endif

	local_head_next = sharedregion_get_ptr((u32 *)obj->attrs->head.next);
	WARN_ON(local_head_next == NULL);
	dsb();
	/* See if the listmp_object was empty */
	if (local_head_next != (struct listmp_elem *)&obj->attrs->head) {
		/* Elem to return */
		elem = local_head_next;
		dsb();
		if (WARN_ON(elem == NULL)) {
			retval = -EFAULT;
			goto gate_leave_and_exit;
		}
#if 0
		if (unlikely(obj->cache_enabled)) {
			Cache_inv((void *)local_head_next,
				sizeof(struct listmp_elem), Cache_Type_ALL,
				true);
		}
#endif
		local_next = sharedregion_get_ptr((u32 *)elem->next);
		if (WARN_ON(local_next == NULL)) {
			retval = -EFAULT;
			goto gate_leave_and_exit;
		}

		/* Fix the head of the list next pointer */
		obj->attrs->head.next = elem->next;
		dsb();
		/* Fix the prev pointer of the new first elem on the list */
		local_next->prev = local_head_next->prev;
#if 0
		if (unlikely(obj->cache_enabled)) {
			Cache_inv((void *)&(obj->attrs->head),
				sizeof(struct listmp_elem), Cache_Type_ALL,
				true);
			Cache_inv((void *)local_next,
				sizeof(struct listmp_elem), Cache_Type_ALL,
				true);
		}
#endif
	}

gate_leave_and_exit:
	gatemp_leave(obj->gatemp_handle, key);

exit:
	if (retval < 0)
		pr_err("listmp_get_head failed! status = 0x%x", retval);

	return elem;
}

/* Function to get tail element from a shared memory list */
void *listmp_get_tail(void *listmp_handle)
{
	struct listmp_object *obj = NULL;
	struct listmp_elem *elem = NULL;
	int *key;
	struct listmp_elem *local_head_prev = NULL;
	struct listmp_elem *local_prev = NULL;
	s32 retval = 0;

	if (WARN_ON(unlikely(atomic_cmpmask_and_lt(&(listmp_module->ref_count),
				LISTMP_MAKE_MAGICSTAMP(0),
				LISTMP_MAKE_MAGICSTAMP(1)) == true))) {
		retval = -ENODEV;
		goto exit;
	}
	if (WARN_ON(listmp_module->ns_handle == NULL)) {
		retval = -EINVAL;
		goto exit;
	}
	if (WARN_ON(unlikely(listmp_handle == NULL))) {
		retval = -EINVAL;
		goto exit;
	}

	obj = (struct listmp_object *)listmp_handle;
	key = gatemp_enter(obj->gatemp_handle);
#if 0
	if (unlikely(obj->cache_enabled)) {
		Cache_inv((void *)&(obj->attrs->head),
			sizeof(struct listmp_elem), Cache_Type_ALL, true);
	}
#endif

	local_head_prev = sharedregion_get_ptr((u32 *)obj->attrs->head.prev);
	WARN_ON(local_head_prev == NULL);

	/* See if the listmp_object was empty */
	if (local_head_prev != (struct listmp_elem *)&obj->attrs->head) {
		/* Elem to return */
		elem = local_head_prev;
		if (WARN_ON(elem == NULL)) {
			retval = -EFAULT;
			goto gate_leave_and_exit;
		}
#if 0
		if (unlikely(obj->cache_enabled)) {
			Cache_inv((void *)local_head_prev,
				sizeof(struct listmp_elem), Cache_Type_ALL,
				true);
		}
#endif
		local_prev = sharedregion_get_ptr((u32 *)elem->prev);
		if (WARN_ON(local_prev == NULL)) {
			retval = -EFAULT;
			goto gate_leave_and_exit;
		}

		/* Fix the head of the list prev pointer */
		obj->attrs->head.prev = elem->prev;

		/* Fix the next pointer of the new last elem on the list */
		local_prev->next = local_head_prev->next;
#if 0
		if (unlikely(obj->cache_enabled)) {
			Cache_inv((void *)&(obj->attrs->head),
				sizeof(struct listmp_elem), Cache_Type_ALL,
				true);
			Cache_inv((void *)local_prev,
				sizeof(struct listmp_elem), Cache_Type_ALL,
				true);
		}
#endif
	}

gate_leave_and_exit:
	gatemp_leave(obj->gatemp_handle, key);

exit:
	if (retval < 0)
		pr_err("listmp_get_tail failed! status = 0x%x", retval);

	return elem;
}

/* Function to put head element into a shared memory list */
int listmp_put_head(void *listmp_handle, struct listmp_elem *elem)
{
	int status = 0;
	struct listmp_object *obj = NULL;
	struct listmp_elem *local_next_elem = NULL;
	int *key;
	struct listmp_elem *shared_elem = NULL;
	u32 index;

	if (WARN_ON(unlikely(atomic_cmpmask_and_lt(&(listmp_module->ref_count),
				LISTMP_MAKE_MAGICSTAMP(0),
				LISTMP_MAKE_MAGICSTAMP(1)) == true))) {
		status = -ENODEV;
		goto exit;
	}
	if (WARN_ON(unlikely(listmp_handle == NULL))) {
		status = -EINVAL;
		goto exit;
	}
	if (WARN_ON(unlikely(elem == NULL))) {
		status = -EINVAL;
		goto exit;
	}

	obj = (struct listmp_object *)listmp_handle;
	dsb();
	index = sharedregion_get_id(elem);
	shared_elem = (struct listmp_elem *)sharedregion_get_srptr((void *)elem,
									index);
	WARN_ON((u32 *)shared_elem == SHAREDREGION_INVALIDSRPTR);
	dsb();

	key = gatemp_enter(obj->gatemp_handle);
#if 0
	if (unlikely(obj->cache_enabled)) {
		Cache_inv((void *)&(obj->attrs->head),
			sizeof(struct listmp_elem), Cache_Type_ALL, true);
	}
#endif
	/* Add the new elem into the list */
	elem->next = obj->attrs->head.next;
	dsb();
	local_next_elem = sharedregion_get_ptr((u32 *)elem->next);
	if (WARN_ON(local_next_elem == NULL)) {
		status = -EFAULT;
		goto gate_leave_and_exit;
	}
#if 0
	if (unlikely(obj->cache_enabled)) {
		Cache_inv((void *)local_next_elem,
			sizeof(struct listmp_elem), Cache_Type_ALL, true);
	}
#endif
	elem->prev = local_next_elem->prev;
	local_next_elem->prev = shared_elem;
	obj->attrs->head.next = shared_elem;
#if 0
	if (unlikely(obj->cache_enabled)) {
		/* Need to do cache operations */
		Cache_inv((void *)local_next_elem,
			sizeof(struct listmp_elem), Cache_Type_ALL, true);
		Cache_inv((void *)&(obj->attrs->head),
			sizeof(struct listmp_elem), Cache_Type_ALL, true);
		/* writeback invalidate only the elem structure */
		Cache_inv((void *)elem,
			sizeof(struct listmp_elem), Cache_Type_ALL, true);
	}
#endif

gate_leave_and_exit:
	gatemp_leave(obj->gatemp_handle, key);

exit:
	if (status < 0)
		pr_err("listmp_put_head failed! status = 0x%x\n", status);
	return status;
}

/* Function to put tail element into a shared memory list */
int listmp_put_tail(void *listmp_handle, struct listmp_elem *elem)
{
	int status = 0;
	struct listmp_object *obj = NULL;
	int *key;
	struct listmp_elem *local_prev_elem = NULL;
	struct listmp_elem *shared_elem = NULL;
	u32 index;

	if (WARN_ON(unlikely(atomic_cmpmask_and_lt(&(listmp_module->ref_count),
				LISTMP_MAKE_MAGICSTAMP(0),
				LISTMP_MAKE_MAGICSTAMP(1)) == true))) {
		status = -ENODEV;
		goto exit;
	}
	if (WARN_ON(unlikely(listmp_handle == NULL))) {
		status = -EINVAL;
		goto exit;
	}
	if (WARN_ON(elem == NULL)) {
		status = -EINVAL;
		goto exit;
	}

	obj = (struct listmp_object *)listmp_handle;
	dsb();
	/* Safe to do outside the gate */
	index = sharedregion_get_id(elem);
	shared_elem = (struct listmp_elem *)sharedregion_get_srptr((void *)elem,
									index);
	WARN_ON((u32 *)shared_elem == SHAREDREGION_INVALIDSRPTR);
	dsb();

	key = gatemp_enter(obj->gatemp_handle);
#if 0
	if (unlikely(obj->cache_enabled)) {
		Cache_inv((void *)&(obj->attrs->head),
			sizeof(struct listmp_elem), Cache_Type_ALL, true);
	}
#endif
	if (WARN_ON(obj->attrs == NULL)) {
		status = -EFAULT;
		goto gate_leave_and_exit;
	}

	elem->prev = obj->attrs->head.prev;
	dsb();
	local_prev_elem = sharedregion_get_ptr((u32 *)elem->prev);
	if (WARN_ON(local_prev_elem == NULL)) {
		status = -EFAULT;
		goto gate_leave_and_exit;
	}
	dsb();
#if 0
	if (unlikely(obj->cache_enabled)) {
		Cache_inv((void *)local_next_elem,
			sizeof(struct listmp_elem), Cache_Type_ALL, true);
	}
#endif
	/* Add the new elem into the list */
	elem->next = local_prev_elem->next;
	local_prev_elem->next = shared_elem;
	obj->attrs->head.prev = shared_elem;
#if 0
	if (unlikely(obj->cache_enabled)) {
		/* Need to do cache operations */
		Cache_inv((void *)local_prev_elem,
			sizeof(struct listmp_elem), Cache_Type_ALL, true);
		Cache_inv((void *)&(obj->attrs->head),
			sizeof(struct listmp_elem), Cache_Type_ALL, true);
		/* writeback invalidate only the elem structure */
		Cache_inv((void *)elem,
			sizeof(struct listmp_elem), Cache_Type_ALL, true);
	}
#endif

gate_leave_and_exit:
	gatemp_leave(obj->gatemp_handle, key);

exit:
	if (status < 0)
		pr_err("listmp_put_tail failed! status = 0x%x\n", status);
	return status;
}

/* Function to insert an element into a shared memory list */
int listmp_insert(void *listmp_handle, struct listmp_elem *new_elem,
			struct listmp_elem *cur_elem)
{
	int status = 0;
	struct listmp_object *obj = NULL;
	struct listmp_elem *local_prev_elem = NULL;
	int *key;
	struct listmp_elem *shared_new_elem = NULL;
	struct listmp_elem *shared_cur_elem = NULL;
	u32 index;

	if (WARN_ON(unlikely(atomic_cmpmask_and_lt(&(listmp_module->ref_count),
				LISTMP_MAKE_MAGICSTAMP(0),
				LISTMP_MAKE_MAGICSTAMP(1)) == true))) {
		status = -ENODEV;
		goto exit;
	}
	if (WARN_ON(unlikely(new_elem == NULL))) {
		status = -EINVAL;
		goto exit;
	}
	if (WARN_ON(unlikely(cur_elem == NULL))) {
		status = -EINVAL;
		goto exit;
	}

	obj = (struct listmp_object *)listmp_handle;
	dsb();
	/* Get SRPtr for new_elem */
	index = sharedregion_get_id(new_elem);
	shared_new_elem = (struct listmp_elem *)
				sharedregion_get_srptr((void *)new_elem, index);
	WARN_ON((u32 *)shared_new_elem == SHAREDREGION_INVALIDSRPTR);
	dsb();
	/* Get SRPtr for cur_elem */
	index = sharedregion_get_id(cur_elem);
	shared_cur_elem = (struct listmp_elem *)
				sharedregion_get_srptr((void *)cur_elem, index);
	WARN_ON((u32 *)shared_cur_elem == SHAREDREGION_INVALIDSRPTR);
	dsb();

	key = gatemp_enter(obj->gatemp_handle);
#if 0
	if (unlikely(obj->cache_enabled)) {
		Cache_inv((void *)cur_elem,
			sizeof(struct listmp_elem), Cache_Type_ALL, true);
	}
#endif
	local_prev_elem = sharedregion_get_ptr((u32 *)cur_elem->prev);
	if (WARN_ON(local_prev_elem == NULL)) {
		status = -EFAULT;
		goto gate_leave_and_exit;
	}
	dsb();
#if 0
	if (unlikely(obj->cache_enabled)) {
		Cache_inv((void *)local_prev_elem,
			sizeof(struct listmp_elem), Cache_Type_ALL, true);
	}
#endif
	new_elem->next = shared_cur_elem;
	new_elem->prev = cur_elem->prev;
	local_prev_elem->next = shared_new_elem;
	cur_elem->prev = shared_new_elem;
	dsb();
#if 0
	if (unlikely(obj->cache_enabled)) {
		/* Need to do cache operations */
		Cache_inv((void *)local_prev_elem,
			sizeof(struct listmp_elem), Cache_Type_ALL, true);
		Cache_inv((void *)cur_elem,
			sizeof(struct listmp_elem), Cache_Type_ALL, true);
		/* writeback invalidate only the elem structure */
		Cache_inv((void *)new_elem,
			sizeof(struct listmp_elem), Cache_Type_ALL, true);
	}
#endif

gate_leave_and_exit:
	gatemp_leave(obj->gatemp_handle, key);

exit:
	if (status < 0)
		pr_err("listmp_insert failed! status = 0x%x\n", status);
	return status;
}

/* Function to remove a element from a shared memory list */
int listmp_remove(void *listmp_handle, struct listmp_elem *elem)
{
	int status = 0;
	struct listmp_object *obj = NULL;
	struct listmp_elem *local_prev_elem = NULL;
	struct listmp_elem *local_next_elem = NULL;
	int *key;

	if (WARN_ON(unlikely(atomic_cmpmask_and_lt(&(listmp_module->ref_count),
				LISTMP_MAKE_MAGICSTAMP(0),
				LISTMP_MAKE_MAGICSTAMP(1)) == true))) {
		status = -ENODEV;
		goto exit;
	}
	if (WARN_ON(elem == NULL)) {
		status = -EINVAL;
		goto exit;
	}

	obj = (struct listmp_object *)listmp_handle;

	key = gatemp_enter(obj->gatemp_handle);
#if 0
	if (unlikely(obj->cache_enabled)) {
		Cache_inv((void *)elem,
			sizeof(struct listmp_elem), Cache_Type_ALL, true);
	}
#endif
	local_prev_elem = sharedregion_get_ptr((u32 *)elem->prev);
	local_next_elem = sharedregion_get_ptr((u32 *)elem->next);
	dsb();
	if (WARN_ON((local_next_elem == NULL) || (local_prev_elem == NULL))) {
		status = -EFAULT;
		goto gate_leave_and_exit;
	}
#if 0
	if (unlikely(obj->cache_enabled)) {
		/* Need to do cache operations */
		Cache_inv((void *)local_prev_elem,
			sizeof(struct listmp_elem), Cache_Type_ALL, true);
		Cache_inv((void *)local_next_elem,
			sizeof(struct listmp_elem), Cache_Type_ALL, true);
	}
#endif
	local_prev_elem->next = elem->next;
	local_next_elem->prev = elem->prev;
#if 0
	if (unlikely(obj->cache_enabled)) {
		/* Need to do cache operations */
		Cache_inv((void *)local_prev_elem,
			sizeof(struct listmp_elem), Cache_Type_ALL, true);
		Cache_inv((void *)local_next_elem,
			sizeof(struct listmp_elem), Cache_Type_ALL, true);
	}
#endif

gate_leave_and_exit:
	gatemp_leave(obj->gatemp_handle, key);

exit:
	if (status < 0)
		pr_err("listmp_remove failed! status = 0x%x\n", status);
	return status;
}

/* Function to traverse to next element in shared memory list */
void *listmp_next(void *listmp_handle, struct listmp_elem *elem)
{
	int status = 0;
	struct listmp_object *obj = NULL;
	struct listmp_elem *ret_elem = NULL;
	int *key;

	if (WARN_ON(unlikely(atomic_cmpmask_and_lt(&(listmp_module->ref_count),
				LISTMP_MAKE_MAGICSTAMP(0),
				LISTMP_MAKE_MAGICSTAMP(1)) == true))) {
		status = -ENODEV;
		goto exit;
	}

	obj = (struct listmp_object *)listmp_handle;

	key = gatemp_enter(obj->gatemp_handle);
	/* If element is NULL start at head */
	if (elem == NULL)
		elem = (struct listmp_elem *)&obj->attrs->head;
#if 0
	if (unlikely(obj->cache_enabled)) {
		Cache_inv((void *)elem,
			sizeof(struct listmp_elem), Cache_Type_ALL, true);
	}
#endif
	ret_elem = sharedregion_get_ptr((u32 *)elem->next);
	WARN_ON(ret_elem == NULL);
	/*  NULL if list is empty */
	if (ret_elem == (struct listmp_elem *)&obj->attrs->head)
		ret_elem = NULL;
	gatemp_leave(obj->gatemp_handle, key);

exit:
	if (status < 0)
		pr_err("listmp_next failed! status = 0x%x\n", status);
	return ret_elem;
}

/* Function to traverse to prev element in shared memory list */
void *listmp_prev(void *listmp_handle, struct listmp_elem *elem)
{
	int status = 0;
	struct listmp_object *obj = NULL;
	struct listmp_elem *ret_elem = NULL;
	int *key;

	if (WARN_ON(unlikely(atomic_cmpmask_and_lt(&(listmp_module->ref_count),
				LISTMP_MAKE_MAGICSTAMP(0),
				LISTMP_MAKE_MAGICSTAMP(1)) == true))) {
		status = -ENODEV;
		goto exit;
	}

	obj = (struct listmp_object *)listmp_handle;

	key = gatemp_enter(obj->gatemp_handle);
	/* If element is NULL start at head */
	if (elem == NULL)
		elem = (struct listmp_elem *)&obj->attrs->head;
#if 0
	if (unlikely(obj->cache_enabled)) {
		Cache_inv((void *)elem,
			sizeof(struct listmp_elem), Cache_Type_ALL, true);
	}
#endif
	ret_elem = sharedregion_get_ptr((u32 *)elem->prev);
	WARN_ON(ret_elem == NULL);
	/*  NULL if list is empty */
	if (ret_elem == (struct listmp_elem *)&obj->attrs->head)
		ret_elem = NULL;
	gatemp_leave(obj->gatemp_handle, key);

exit:
	if (status < 0)
		pr_err("listmp_prev failed! status = 0x%x\n", status);
	return ret_elem;
}

/* Function to return the amount of shared memory required for creation of
 * each instance. */
uint listmp_shared_mem_req(const struct listmp_params *params)
{
	int retval = 0;
	uint mem_req = 0;
	uint min_align;
	u16 region_id;

	if (WARN_ON(unlikely(params == NULL))) {
		retval = -EINVAL;
		goto exit;
	}

	if (params->shared_addr == NULL)
		region_id = params->region_id;
	else
		region_id = sharedregion_get_id(params->shared_addr);
	WARN_ON(region_id == SHAREDREGION_INVALIDREGIONID);

	/*min_align = Memory_getMaxDefaultTypeAlign();*/min_align = 4;
	if (sharedregion_get_cache_line_size(region_id) > min_align)
		min_align = sharedregion_get_cache_line_size(region_id);

	mem_req = ROUND_UP(sizeof(struct listmp_attrs), min_align);

exit:
	if (retval < 0) {
		pr_err("listmp_shared_mem_req failed! status = 0x%x\n",
			retval);
	}
	return mem_req;
}

/* Clears a listmp element's pointers */
static void _listmp_elem_clear(struct listmp_elem *elem)
{
	u32 *shared_elem;
	int id;

	WARN_ON(elem == NULL);

	id = sharedregion_get_id(elem);
	shared_elem = sharedregion_get_srptr(elem, id);
	elem->next = elem->prev = (struct listmp_elem *)shared_elem;
#if 0
	if (unlikely(obj->cache_enabled)) {
		Cache_inv((void *)elem,
			sizeof(struct listmp_elem), Cache_Type_ALL, true);
	}
#endif
}

/* Creates a new instance of listmp module. This is an internal
 * function because both listmp_create and
 * listmp_open call use the same functionality. */
static int _listmp_create(struct listmp_object **handle_ptr,
				struct listmp_params *params, u32 create_flag)
{
	int status = 0;
	struct listmp_object *obj = NULL;
	void *local_addr  = NULL;
	u32 *shared_shm_base;
	struct listmp_params sparams;
	u16 name_len;

	if (WARN_ON(unlikely(handle_ptr == NULL))) {
		status = -EINVAL;
		goto exit;
	}
	if (WARN_ON(unlikely(params == NULL))) {
		status = -EINVAL;
		goto exit;
	}

	/* Allow local lock not being provided. Don't do protection if local
	* lock is not provided.
	*/
	/* Create the handle */
	obj = kzalloc(sizeof(struct listmp_object), GFP_KERNEL);
	*handle_ptr = obj;
	if (obj == NULL) {
		status = -ENOMEM;
		goto exit;
	}

	/* Populate the params member */
	memcpy((void *)&obj->params, (void *)params,
			sizeof(struct listmp_params));

	if (create_flag == false) {
		/* Update attrs */
		obj->attrs = (struct listmp_attrs *)params->shared_addr;
		obj->region_id = sharedregion_get_id((void *)&obj->attrs->head);
		obj->cache_enabled = sharedregion_is_cache_enabled(
								obj->region_id);
		/* get the local address of the SRPtr */
		local_addr = sharedregion_get_ptr(obj->attrs->gatemp_addr);
		status = gatemp_open_by_addr(local_addr, &(obj->gatemp_handle));
		if (status < 0)
			goto error;
	} else {
		INIT_LIST_HEAD(&obj->list_elem);

		/* init the gate */
		if (params->gatemp_handle != NULL)
			obj->gatemp_handle = params->gatemp_handle;
		else
			obj->gatemp_handle = gatemp_get_default_remote();
		if (obj->gatemp_handle == NULL)
			goto error;

		if (params->shared_addr == NULL) {
			obj->region_id = params->region_id;
			obj->cache_enabled = sharedregion_is_cache_enabled(
								obj->region_id);

			listmp_params_init(&sparams);
			sparams.region_id = params->region_id;
			obj->alloc_size = listmp_shared_mem_req(&sparams);

			/* HeapMemMP will do the alignment * */
			obj->attrs = sl_heap_alloc(
					sharedregion_get_heap(obj->region_id),
					obj->alloc_size,
					0);
			if (obj->attrs == NULL) {
				status = -ENOMEM;
				goto error;
			}
		} else {
			obj->region_id = sharedregion_get_id(
							params->shared_addr);
			if (unlikely(obj->region_id == \
				SHAREDREGION_INVALIDREGIONID)) {
				status = -1;
				goto error;
			}
			if (((u32) params->shared_addr % \
				sharedregion_get_cache_line_size(
							obj->region_id)) != 0) {
				status = -EFAULT;
				goto error;
			}

			obj->cache_enabled = sharedregion_is_cache_enabled(
								obj->region_id);
			obj->attrs = (struct listmp_attrs *)params->shared_addr;
		}

		_listmp_elem_clear((struct listmp_elem *)&obj->attrs->head);
		obj->attrs->gatemp_addr = gatemp_get_shared_addr(
							obj->gatemp_handle);
#if 0
		if (unlikely(obj->cache_enabled)) {
			Cache_inv((void *)obj->attrs,
				sizeof(struct listmp_attrs), Cache_Type_ALL,
				true);
		}
#endif
		if (params->name != NULL) {
			name_len = strlen(params->name) + 1;
			/* Copy the name */
			obj->params.name = kmalloc(name_len, GFP_KERNEL);
			if (obj->params.name == NULL) {
				/*  NULL if Memory allocation failed for
				name */
				status = -ENOMEM;
				goto error;
			}
			strncpy(obj->params.name, params->name, name_len);
			shared_shm_base = sharedregion_get_srptr((void *)
						obj->attrs, obj->region_id);
			WARN_ON(shared_shm_base == SHAREDREGION_INVALIDSRPTR);

			/* Add list instance to name server */
			obj->ns_key = nameserver_add_uint32(
					listmp_module->ns_handle, params->name,
					(u32)shared_shm_base);
			if (unlikely(obj->ns_key == NULL)) {
				status = -EFAULT;
				goto error;
			}
		}
		obj->attrs->status = LISTMP_CREATED;
	}

	/* Update owner and opener details */
	if (create_flag == true) {
		obj->owner.creator = true;
		obj->owner.open_count = 1;
		obj->owner.proc_id = multiproc_self();
	} else {
		obj->owner.creator = false;
		obj->owner.open_count = 0;
		obj->owner.proc_id = MULTIPROC_INVALIDID;
	}
	obj->top = obj;

	/* Put in the module list */
	/* Function is called already with mutex acquired. So, no need to lock
	 * here */
	INIT_LIST_HEAD(&obj->list_elem);
	list_add_tail((&obj->list_elem), &listmp_module->obj_list);
	return 0;

error:
	if (status < 0) {
		if (create_flag == true) {
			if (obj->params.name != NULL) {
				if (obj->ns_key != NULL) {
					nameserver_remove_entry(
						listmp_module->ns_handle,
						obj->ns_key);
				}
				kfree(obj->params.name);
			}
			if (params->shared_addr == NULL) {
				if (obj->attrs != NULL) {
					sl_heap_free(sharedregion_get_heap(
							obj->region_id),
							(void *)obj->attrs,
							obj->alloc_size);
				}
			}
		}
		kfree(obj);
		obj = NULL;
	}

exit:
	if (status < 0)
		pr_err("_listmp_create failed! status = 0x%x\n", status);
	return status;
}
