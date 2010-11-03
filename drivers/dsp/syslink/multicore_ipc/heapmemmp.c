/*
 *  heapmemmp.c
 *
 *  Heap module manages variable size buffers that can be used
 *  in a multiprocessor system with shared memory.
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

#include <linux/module.h>
#include <linux/types.h>
#include <linux/string.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/slab.h>

#include <atomic_linux.h>
#include <multiproc.h>
#include <nameserver.h>
#include <sharedregion.h>
#include <gatemp.h>
#include <heapmemmp.h>

/*
 *  Name of the reserved nameserver used for heapmemmp.
 */
#define HEAPMEMMP_NAMESERVER			"HeapMemMP"
#define HEAPMEMMP_MAX_NAME_LEN			32
#define HEAPMEMMP_MAX_RUNTIME_ENTRIES	32
#define HEAPMEMMP_CACHESIZE				128
/* brief Macro to make a correct module magic number with ref_count */
#define HEAPMEMMP_MAKE_MAGICSTAMP(x)	((HEAPMEMMP_MODULEID << 12) | (x))

#define ROUND_UP(a, b)	(((a) + ((b) - 1)) & (~((b) - 1)))

/*
 *  Structure defining processor related information for the
 *  heapmemmp module
 */
struct heapmemmp_proc_attrs {
	bool creator; /* Creator or opener */
	u16 proc_id; /* Processor identifier */
	u32 open_count; /* open count in a processor */
};

/*
 * heapmemmp header structure
 */
struct heapmemmp_header {
    u32 *next; /* SRPtr to next header */
    u32 size; /* Size of this segment */
};

/*
 *  Structure defining attribute parameters for the heapmemmp module
 */
struct heapmemmp_attrs {
	VOLATILE u32 status; /* Module status */
	VOLATILE u32 *buf_ptr; /* Memory managed by instance */
	VOLATILE struct heapmemmp_header head; /* header */
	VOLATILE u32 *gatemp_addr; /* gatemp shared address (shm safe) */
};

/*
 *  Structure for heapmemmp module state
 */
struct heapmemmp_module_object {
	atomic_t ref_count; /* Reference count */
	void *nameserver; /* Nameserver handle */
	struct list_head obj_list; /* List holding created objects */
	struct mutex *local_lock; /* lock for protecting obj_list */
	struct heapmemmp_config cfg; /* Current config values */
	struct heapmemmp_config default_cfg; /* Default config values */
	struct heapmemmp_params default_inst_params; /* Default instance
						creation parameters */
};

static struct heapmemmp_module_object heapmemmp_state = {
	.obj_list = LIST_HEAD_INIT(heapmemmp_state.obj_list),
	.default_cfg.max_name_len = HEAPMEMMP_MAX_NAME_LEN,
	.default_cfg.max_runtime_entries = HEAPMEMMP_MAX_RUNTIME_ENTRIES,
	.default_inst_params.gate = NULL,
	.default_inst_params.name = NULL,
	.default_inst_params.region_id = 0,
	.default_inst_params.shared_addr = NULL,
	.default_inst_params.shared_buf_size = 0,
};

/* Pointer to module state */
static struct heapmemmp_module_object *heapmemmp_module = &heapmemmp_state;

/*
 *  Structure for the handle for the heapmemmp
 */
struct heapmemmp_obj {
	struct list_head list_elem; /* Used for creating a linked list */
	struct heapmemmp_attrs *attrs; /* The shared attributes structure */
	void *gate; /* Lock used for critical region management */
	void *ns_key; /* nameserver key required for remove */
	bool cache_enabled; /* Whether to do cache calls */
	u16 region_id; /* shared region index */
	u32 alloc_size; /* Size of allocated shared memory */
	char *buf; /* Pointer to allocated memory */
	u32 min_align; /* Minimum alignment required */
	u32 buf_size; /* Buffer Size */
	struct heapmemmp_proc_attrs owner; /* owner processor info */
	void *top; /* Pointer to the top object */
	struct heapmemmp_params params; /* The creation parameter structure */
};

#define heapmemmp_object heap_object

/* =============================================================================
 * Forward declarations of internal functions
 * =============================================================================
 */
static int heapmemmp_post_init(struct heapmemmp_object *handle);

/* =============================================================================
 * APIs called directly by applications
 * =============================================================================
 */

/*
 *  This will get default configuration for the
 *  heapmemmp module
 */
int heapmemmp_get_config(struct heapmemmp_config *cfgparams)
{
	s32 retval = 0;

	BUG_ON(cfgparams == NULL);

	if (cfgparams == NULL) {
		retval = -EINVAL;
		goto error;
	}

	if (atomic_cmpmask_and_lt(&(heapmemmp_module->ref_count),
				HEAPMEMMP_MAKE_MAGICSTAMP(0),
				HEAPMEMMP_MAKE_MAGICSTAMP(1)) == true)
		memcpy(cfgparams, &heapmemmp_module->default_cfg,
					sizeof(struct heapmemmp_config));
	else
		memcpy(cfgparams, &heapmemmp_module->cfg,
					sizeof(struct heapmemmp_config));
	return 0;
error:
	pr_err("heapmemmp_get_config failed status: %x\n", retval);
	return retval;
}
EXPORT_SYMBOL(heapmemmp_get_config);

/*
 *  This will setup the heapmemmp module
 *
 *  This function sets up the heapmemmp module. This function
 *  must be called before any other instance-level APIs can be
 *  invoked.
 *  Module-level configuration needs to be provided to this
 *  function. If the user wishes to change some specific config
 *  parameters, then heapmemmp_getconfig can be called to get
 *  the configuration filled with the default values. After this,
 *  only the required configuration values can be changed. If the
 *  user does not wish to make any change in the default parameters,
 *  the application can simply call heapmemmp_setup with NULL
 *  parameters. The default parameters would get automatically used.
 */
int heapmemmp_setup(const struct heapmemmp_config *cfg)
{
	struct nameserver_params params;
	struct heapmemmp_config tmp_cfg;
	s32 retval  = 0;

	/* This sets the ref_count variable  not initialized, upper 16 bits is
	* written with module Id to ensure correctness of ref_count variable
	*/
	atomic_cmpmask_and_set(&heapmemmp_module->ref_count,
					HEAPMEMMP_MAKE_MAGICSTAMP(0),
					HEAPMEMMP_MAKE_MAGICSTAMP(0));
	if (atomic_inc_return(&heapmemmp_module->ref_count)
					!= HEAPMEMMP_MAKE_MAGICSTAMP(1)) {
		return 1;
	}

	if (cfg == NULL) {
		heapmemmp_get_config(&tmp_cfg);
		cfg = &tmp_cfg;
	}

	if (cfg->max_name_len == 0 ||
		cfg->max_name_len > HEAPMEMMP_MAX_NAME_LEN) {
		retval = -EINVAL;
		goto error;
	}

	/* Initialize the parameters */
	nameserver_params_init(&params);
	params.max_value_len = sizeof(u32);
	params.max_name_len = cfg->max_name_len;

	/* Create the nameserver for modules */
	heapmemmp_module->nameserver  =
		nameserver_create(HEAPMEMMP_NAMESERVER, &params);
	if (heapmemmp_module->nameserver == NULL) {
		retval = -EFAULT;
		goto error;
	}

	/* Construct the list object */
	INIT_LIST_HEAD(&heapmemmp_module->obj_list);
	/* Copy config info */
	memcpy(&heapmemmp_module->cfg, cfg, sizeof(struct heapmemmp_config));
	/* Create a lock for protecting list object */
	heapmemmp_module->local_lock = kmalloc(sizeof(struct mutex),
								GFP_KERNEL);
	mutex_init(heapmemmp_module->local_lock);
	if (heapmemmp_module->local_lock == NULL) {
		retval = -ENOMEM;
		heapmemmp_destroy();
		goto error;
	}

	return 0;

error:
	pr_err("heapmemmp_setup failed status: %x\n", retval);
	return retval;
}
EXPORT_SYMBOL(heapmemmp_setup);

/*
 *  This will destroy the heapmemmp module
 */
int heapmemmp_destroy(void)
{
	s32 retval  = 0;
	struct mutex *lock = NULL;
	struct heapmemmp_obj *obj = NULL;

	if (atomic_cmpmask_and_lt(&(heapmemmp_module->ref_count),
				HEAPMEMMP_MAKE_MAGICSTAMP(0),
				HEAPMEMMP_MAKE_MAGICSTAMP(1)) == true) {
		retval = -ENODEV;
		goto error;
	}

	if (atomic_dec_return(&heapmemmp_module->ref_count)
				== HEAPMEMMP_MAKE_MAGICSTAMP(0)) {
		/* Temporarily increment ref_count here. */
		atomic_set(&heapmemmp_module->ref_count,
					HEAPMEMMP_MAKE_MAGICSTAMP(1));

		/*  Check if any heapmemmp instances have not been
		 *  deleted/closed so far. if there any, delete or close them
		 */
		list_for_each_entry(obj, &heapmemmp_module->obj_list,
								list_elem) {
			if (obj->owner.proc_id == multiproc_get_id(NULL))
				retval = heapmemmp_delete(&obj->top);
			else
				retval = heapmemmp_close(obj->top);

			if (list_empty(&heapmemmp_module->obj_list))
				break;

			if (retval < 0)
				goto error;
		}

		/* Again reset ref_count. */
		atomic_set(&heapmemmp_module->ref_count,
					HEAPMEMMP_MAKE_MAGICSTAMP(0));

		if (likely(heapmemmp_module->nameserver != NULL)) {
			retval = nameserver_delete(&heapmemmp_module->
								nameserver);
			if (unlikely(retval != 0))
				goto error;
		}

		/* Delete the list lock */
		lock = heapmemmp_module->local_lock;
		retval = mutex_lock_interruptible(lock);
		if (retval)
			goto error;

		heapmemmp_module->local_lock = NULL;
		mutex_unlock(lock);
		kfree(lock);
		memset(&heapmemmp_module->cfg, 0,
				sizeof(struct heapmemmp_config));
	}

	return 0;

error:
	pr_err("heapmemmp_destroy failed status: %x\n", retval);
	return retval;
}
EXPORT_SYMBOL(heapmemmp_destroy);

/*
 *  This will get the intialization prams for a heapmemmp
 *  module instance
 */
void heapmemmp_params_init(struct heapmemmp_params *params)
{
	s32 retval  = 0;

	if (atomic_cmpmask_and_lt(&(heapmemmp_module->ref_count),
				HEAPMEMMP_MAKE_MAGICSTAMP(0),
				HEAPMEMMP_MAKE_MAGICSTAMP(1)) == true) {
		retval = -ENODEV;
		goto error;
	}

	BUG_ON(params == NULL);

	memcpy(params, &heapmemmp_module->default_inst_params,
				sizeof(struct heapmemmp_params));

	return;
error:
	pr_err("heapmemmp_params_init failed status: %x\n", retval);
}
EXPORT_SYMBOL(heapmemmp_params_init);

/*
 *  This will create a new instance of heapmemmp module
 *  This is an internal function as both heapmemmp_create
 *  and heapmemmp_open use the functionality
 *
 *  NOTE: The lock to protect the shared memory area
 *  used by heapmemmp is provided by the consumer of
 *  heapmemmp module
 */
static int _heapmemmp_create(void **handle_ptr,
				const struct heapmemmp_params *params,
				u32 create_flag)
{
	s32 retval  = 0;
	struct heapmemmp_obj *obj = NULL;
	struct heapmemmp_object *handle = NULL;
	void *gate_handle = NULL;
	void *local_addr = NULL;
	u32 *shared_shm_base;

	if (atomic_cmpmask_and_lt(&(heapmemmp_module->ref_count),
				HEAPMEMMP_MAKE_MAGICSTAMP(0),
				HEAPMEMMP_MAKE_MAGICSTAMP(1)) == true) {
		retval = -ENODEV;
		goto error;
	}

	BUG_ON(handle_ptr == NULL);

	BUG_ON(params == NULL);

	/* No need for parameter checks, since this is an internal function. */

	/* Initialize return parameter. */
	*handle_ptr = NULL;

	handle = kmalloc(sizeof(struct heapmemmp_object), GFP_KERNEL);
	if (handle == NULL) {
		retval = -ENOMEM;
		goto error;
	}

	obj =  kmalloc(sizeof(struct heapmemmp_obj), GFP_KERNEL);
	if (obj == NULL) {
		retval = -ENOMEM;
		goto error;
	}

	handle->obj = (struct heapmemmp_obj *)obj;
	handle->alloc = &heapmemmp_alloc;
	handle->free  = &heapmemmp_free;
	handle->get_stats = &heapmemmp_get_stats;
	handle->is_blocking = &heapmemmp_isblocking;

	obj->ns_key = NULL;
	obj->alloc_size = 0;

	/* Put in local ilst */
	retval = mutex_lock_interruptible(heapmemmp_module->local_lock);
	if (retval < 0)
		goto error;

	INIT_LIST_HEAD(&obj->list_elem);
	list_add(&obj->list_elem, &heapmemmp_module->obj_list);
	mutex_unlock(heapmemmp_module->local_lock);

	if (create_flag == false) {
		obj->owner.creator = false;
		obj->owner.open_count = 0;
		obj->owner.proc_id = MULTIPROC_INVALIDID;
		obj->top = handle;

		obj->attrs = (struct heapmemmp_attrs *) params->shared_addr;

		/* No need to Cache_inv- already done in openByAddr() */
		obj->buf = (char *) sharedregion_get_ptr((u32 *)obj->
							attrs->buf_ptr);
		obj->buf_size	  = obj->attrs->head.size;
		obj->region_id	 = sharedregion_get_id(obj->buf);
		obj->cache_enabled = sharedregion_is_cache_enabled(obj->
								region_id);

		/* Set min_align */
		obj->min_align = sizeof(struct heapmemmp_header);
		if (sharedregion_get_cache_line_size(obj->region_id)
			> obj->min_align) {
			obj->min_align = sharedregion_get_cache_line_size(
							obj->region_id);
		}

		local_addr = sharedregion_get_ptr((u32 *)obj->attrs->
								gatemp_addr);
		retval = gatemp_open_by_addr(local_addr, &gate_handle);
		if (retval < 0) {
			retval = -EFAULT;
			goto error;
		}
		obj->gate = gate_handle;


	} else {
		obj->owner.creator = true;
		obj->owner.open_count = 1;
		obj->owner.proc_id = multiproc_self();
		obj->top = handle;

		/* Creating the gate */
		if (params->gate != NULL)
			obj->gate = params->gate;
		else {
			/* If no gate specified, get the default system gate */
			obj->gate = gatemp_get_default_remote();
		}

		if (obj->gate == NULL) {
			retval = -EFAULT;
			goto error;
		}

		obj->buf_size = params->shared_buf_size;

		if (params->shared_addr == NULL) {
			/* Creating using a shared region ID */
			/* It is allowed to have NULL name for an anonymous,
			 * not to be opened by name, heap.
			 */
			/* Will be allocated in post_init */
			obj->attrs = NULL;
			obj->region_id = params->region_id;
		} else {
			/* Creating using shared_addr */
			obj->region_id = sharedregion_get_id(params->
								shared_addr);

			/* Assert that the buffer is in a valid shared
			 * region
			 */
			if (obj->region_id == SHAREDREGION_INVALIDREGIONID) {
				retval = -EFAULT;
				goto error;
			} else if ((u32) params->shared_addr
				 % sharedregion_get_cache_line_size(obj->
				region_id) != 0) {
				retval = -EFAULT;
				goto error;
			}
			/* obj->buf will get alignment-adjusted in
			 * postInit
			 */
			obj->buf = (char *)((u32)params->shared_addr + \
						sizeof(struct heapmemmp_attrs));
			obj->attrs = (struct heapmemmp_attrs *)
							params->shared_addr;
		}

		obj->cache_enabled = sharedregion_is_cache_enabled(
							   obj->region_id);

		/* Set min_align */
		obj->min_align = sizeof(struct heapmemmp_header);
		if (sharedregion_get_cache_line_size(obj->region_id)
			> obj->min_align)
			obj->min_align = sharedregion_get_cache_line_size(
							obj->region_id);
		retval = heapmemmp_post_init(handle);
		if (retval < 0) {
			retval = -EFAULT;
			goto error;
		}

		/* Populate the params member */
		memcpy(&obj->params, params, sizeof(struct heapmemmp_params));
		if (params->name != NULL) {
			obj->params.name = kmalloc(strlen(params->name) + 1,
								GFP_KERNEL);
			if (obj->params.name == NULL) {
				retval  = -ENOMEM;
				goto error;
			}
			strncpy(obj->params.name, params->name,
						strlen(params->name) + 1);
		}

		/* We will store a shared pointer in the NameServer */
		shared_shm_base = sharedregion_get_srptr(obj->attrs,
							   obj->region_id);
		if (obj->params.name != NULL) {
			obj->ns_key =
			  nameserver_add_uint32(heapmemmp_module->nameserver,
						params->name,
						(u32) shared_shm_base);
			if (obj->ns_key == NULL) {
				retval = -EFAULT;
				goto error;
			}
		}
	}

	*handle_ptr = (void *)handle;
	return retval;

error:
	handle_ptr = (void *)handle;
	/* Do whatever cleanup is required*/
	if (create_flag == true)
		heapmemmp_delete(handle_ptr);
	else
		heapmemmp_close(handle_ptr);
	pr_err("_heapmemmp_create failed status: %x\n", retval);
	return retval;
}

/*
 *  This will create a new instance of heapmemmp module
 */
void *heapmemmp_create(const struct heapmemmp_params *params)
{
	s32 retval = 0;
	struct heapmemmp_object *handle = NULL;
	struct heapmemmp_params sparams;

	BUG_ON(params == NULL);

	if (atomic_cmpmask_and_lt(&(heapmemmp_module->ref_count),
				HEAPMEMMP_MAKE_MAGICSTAMP(0),
				HEAPMEMMP_MAKE_MAGICSTAMP(1)) == true) {
		retval = -ENODEV;
		goto error;
	}

	if (params == NULL) {
		retval = -EINVAL;
		goto error;
	}

	if (params->shared_buf_size == 0) {
		retval = -EINVAL;
		goto error;
	}

	memcpy(&sparams, (void *)params, sizeof(struct heapmemmp_params));
	retval = _heapmemmp_create((void **)&handle, params, true);
	if (retval < 0)
		goto error;

	return (void *)handle;

error:
	pr_err("heapmemmp_create failed status: %x\n", retval);
	return (void *)handle;
}
EXPORT_SYMBOL(heapmemmp_create);

/*
 *  This will delete an instance of heapmemmp module
 */
int heapmemmp_delete(void **handle_ptr)
{
	int status = 0;
	struct heapmemmp_object *handle = NULL;
	struct heapmemmp_obj *obj = NULL;
	struct heapmemmp_params *params = NULL;
	struct heapmemmp_object *region_heap = NULL;
	s32 retval = 0;
	int *key = NULL;

	if (atomic_cmpmask_and_lt(&(heapmemmp_module->ref_count),
				HEAPMEMMP_MAKE_MAGICSTAMP(0),
				HEAPMEMMP_MAKE_MAGICSTAMP(1)) == true) {
		retval = -ENODEV;
		goto error;
	}

	if (WARN_ON(handle_ptr == NULL)) {
		retval = -EINVAL;
		goto error;
	}

	handle = (struct heapmemmp_object *)(*handle_ptr);
	if (WARN_ON(handle == NULL)) {
		retval = -EINVAL;
		goto error;
	}

	obj = (struct heapmemmp_obj *)handle->obj;
	if (obj != NULL) {
		if (obj->owner.proc_id != multiproc_self()) {
			status = -ENODEV;
			goto error;
		}

		/* Take the local lock */
		key = gatemp_enter(obj->gate);

		if (obj->owner.open_count > 1) {
			retval = -ENODEV;
			goto device_busy_error;
		}

		retval = mutex_lock_interruptible(heapmemmp_module->local_lock);
		if (retval < 0)
			goto lock_error;

		/* Remove frmo the local list */
		list_del(&obj->list_elem);

		mutex_unlock(heapmemmp_module->local_lock);

		params = (struct heapmemmp_params *) &obj->params;

		if (likely(params->name != NULL)) {
			if (likely(obj->ns_key != NULL)) {
				nameserver_remove_entry(heapmemmp_module->
					nameserver, obj->ns_key);
				obj->ns_key = NULL;
			}
			kfree(params->name);
		}

		/* Set status to 'not created' */
	    if (obj->attrs != NULL) {
#if 0
			obj->attrs->status = 0;
			if (obj->cache_enabled) {
				cache_wbinv(obj->attrs,
					sizeof(struct heapmemmp_attrs),
					CACHE_TYPE_ALL, true);
			}
#endif
		}

		/* Release the shared lock */
		gatemp_leave(obj->gate, key);

		/* If necessary, free shared memory  if memory is internally
		 * allocated
		 */
		region_heap = sharedregion_get_heap(obj->region_id);

		if ((region_heap != NULL) &&
			(obj->params.shared_addr == NULL) &&
			(obj->attrs != NULL)) {
			sl_heap_free(region_heap, obj->attrs, obj->alloc_size);
		}

		kfree(obj);
		kfree(region_heap);

		*handle_ptr = NULL;
	} else {	/* obj == NULL */
		kfree(handle);
		*handle_ptr = NULL;
	}

	return 0;

lock_error:
device_busy_error:
	gatemp_leave(obj->gate, key);

error:
	pr_err("heapmemmp_delete failed status: %x\n", retval);
	return retval;
}
EXPORT_SYMBOL(heapmemmp_delete);

/*
 *  This will opens a created instance of heapmemmp
 *  module
 */
int heapmemmp_open(char *name, void **handle_ptr)
{
	s32 retval = 0;
	u32 *shared_shm_base = SHAREDREGION_INVALIDSRPTR;
	void *shared_addr = NULL;
	struct heapmemmp_obj *obj = NULL;
	bool done_flag = false;
	struct list_head *elem = NULL;

	BUG_ON(name  == NULL);
	BUG_ON(handle_ptr == NULL);

	if (unlikely(
		atomic_cmpmask_and_lt(&(heapmemmp_module->ref_count),
				HEAPMEMMP_MAKE_MAGICSTAMP(0),
				HEAPMEMMP_MAKE_MAGICSTAMP(1)) == true)) {
		retval = -ENODEV;
		goto error;
	}

	if (name == NULL) {
		retval = -EINVAL;
		goto error;
	}

	if (handle_ptr == NULL) {
		retval = -EINVAL;
		goto error;
	}

	/* First check in the local list */
	list_for_each(elem, &heapmemmp_module->obj_list) {
		obj = (struct heapmemmp_obj *)elem;
		if (obj->params.name != NULL) {
			if (strcmp(obj->params.name, name)
				== 0) {
				retval = mutex_lock_interruptible(
					heapmemmp_module->local_lock);
				if (retval < 0)
					goto error;
				/* Check if we have created the heapmemmp */
				/* or not */
				if (obj->owner.proc_id == multiproc_self())
					obj->owner.open_count++;

				*handle_ptr = (void *)obj->top;
				mutex_unlock(heapmemmp_module->local_lock);
				done_flag = true;
				break;
			}
		}
	}

	if (likely(done_flag == false)) {
		/* Find in name server */
		retval = nameserver_get_uint32(heapmemmp_module->nameserver,
						name,
						&shared_shm_base,
						NULL);
		if (unlikely(retval < 0))
			goto error;

		/*
		 * Convert from shared region pointer to local address
		 */
		shared_addr = sharedregion_get_ptr(shared_shm_base);
		if (unlikely(shared_addr == NULL)) {
			retval = -EINVAL;
			goto error;
		}

		retval = heapmemmp_open_by_addr(shared_addr, handle_ptr);

		if (unlikely(retval < 0))
			goto error;
	}

	return 0;

error:
	pr_err("heapmemmp_open failed status: %x\n", retval);
	return retval;
}
EXPORT_SYMBOL(heapmemmp_open);

/*
 *  This will closes previously opened/created instance
 *  of heapmemmp module
 */
int heapmemmp_close(void **handle_ptr)
{
	struct heapmemmp_object *handle = NULL;
	struct heapmemmp_obj *obj = NULL;
	s32 retval = 0;

	if (atomic_cmpmask_and_lt(&(heapmemmp_module->ref_count),
				HEAPMEMMP_MAKE_MAGICSTAMP(0),
				HEAPMEMMP_MAKE_MAGICSTAMP(1)) == true) {
		retval = -ENODEV;
		goto error;
	}

	if (WARN_ON(handle_ptr == NULL)) {
		retval  = -EINVAL;
		goto error;
	}

	if (WARN_ON(*handle_ptr == NULL)) {
		retval  = -EINVAL;
		goto error;
	}

	handle = (struct heapmemmp_object *)(*handle_ptr);
	obj = (struct heapmemmp_obj *)handle->obj;

	if (obj != NULL) {
		retval = mutex_lock_interruptible(heapmemmp_module->
								local_lock);
		if (retval)
			goto error;

		/* opening an instance created locally */
		if (obj->owner.proc_id == multiproc_self())
				obj->owner.open_count--;

		/* Check if HeapMemMP is opened on same processor and
		 * this is the last closure.
		 */
		if ((obj->owner.creator == false) &&
				(obj->owner.open_count == 0)) {
			list_del(&obj->list_elem);

			if (obj->gate != NULL) {
				/* Close the instance gate */
				gatemp_close(&obj->gate);
			}

			/* Now free the handle */
			kfree(obj);
			obj = NULL;
			kfree(handle);
			*handle_ptr = NULL;
		}

		mutex_unlock(heapmemmp_module->local_lock);
	} else {
		kfree(handle);
		*handle_ptr = NULL;
	}
	return 0;

error:
	pr_err("heapmemmp_close failed status: %x\n", retval);
	return retval;
}
EXPORT_SYMBOL(heapmemmp_close);

/*
 *  This will allocs a block of memory
 */
void *heapmemmp_alloc(void *hphandle, u32 size, u32 align)
{
	char *alloc_addr = NULL;
	struct heapmemmp_object *handle = NULL;
	struct heapmemmp_obj *obj = NULL;
	int *key = NULL;
	struct heapmemmp_header *prev_header;
	struct heapmemmp_header *new_header;
	struct heapmemmp_header *cur_header;
	u32 cur_size;
	u32 adj_size;
	u32 remain_size;
	u32 adj_align;
	u32 offset;
	s32 retval = 0;

	if (atomic_cmpmask_and_lt(&(heapmemmp_module->ref_count),
				HEAPMEMMP_MAKE_MAGICSTAMP(0),
				HEAPMEMMP_MAKE_MAGICSTAMP(1)) == true) {
		retval = -ENODEV;
		goto error;
	}
	if (WARN_ON(hphandle == NULL)) {
		retval  = -EINVAL;
		goto error;
	}
	if (WARN_ON(size == 0)) {
		retval  = -EINVAL;
		goto error;
	}

	handle = (struct heapmemmp_object *)(hphandle);
	obj = (struct heapmemmp_obj *)handle->obj;
	if (WARN_ON(obj == NULL)) {
		retval = -EINVAL;
		goto error;
	}

	adj_size = size;

	/* Make size requested a multipel of min_align */
	offset = (adj_size & (obj->min_align - 1));
	if (offset != 0)
		adj_size += (obj->min_align - offset);

	/*
	 *  Make sure the alignment is at least as large as obj->min_align
	 *  Note: adjAlign must be a power of 2 (by function constraint) and
	 *  obj->min_align is also a power of 2,
	 */
	adj_align = align;
	if (adj_align == 0)
		adj_align =  obj->min_align;

	if (adj_align & (obj->min_align - 1))
		/* adj_align is less than obj->min_align */
		adj_align = obj->min_align;

	/* No need to Cache_inv Attrs- 'head' should be constant */
	prev_header = (struct heapmemmp_header *) &obj->attrs->head;

	key = gatemp_enter(obj->gate);
	/*
	 *  The block will be allocated from cur_header. Maintain a pointer to
	 *  prev_header so prev_header->next can be updated after the alloc.
	 */
#if 0
	if (unlikely(obj->cache_enabled))
		Cache_inv(prev_header,
			   sizeof(struct heapmemmp_header),
			   Cache_Type_ALL,
			   true); /* A1 */
#endif
	cur_header = (struct heapmemmp_header *)
				sharedregion_get_ptr(prev_header->next);
	/* A1 */

	/* Loop over the free list. */
	while (cur_header != NULL) {
#if 0
		/* Invalidate cur_header */
		if (unlikely(obj->cache_enabled))
			Cache_inv(cur_header,
				  sizeof(struct heapmemmp_header),
				  Cache_Type_ALL,
				  true); /* A2 */
#endif

		cur_size = cur_header->size;

		/*
		 *  Determine the offset from the beginning to make sure
		 *  the alignment request is honored.
		 */
		offset = (u32)cur_header & (adj_align - 1);
		if (offset)
			offset = adj_align - offset;

		/* Internal Assert that offset is a multiple of */
		/* obj->min_align */
		if (((offset & (obj->min_align - 1)) != 0)) {
			retval = -EINVAL;
			goto error;
		}
		/* big enough? */

		/* This is the "else" part of the next if block, but we are */
		/* moving it here to save indent space */
		if (cur_size < (adj_size + offset)) {
			prev_header = cur_header;
			cur_header = sharedregion_get_ptr(cur_header->next);
			/* We can quit this iteration of the while loop here */
			continue;
		}

		/* if (cur_size >= (adj_size + offset)) */

		/* Set the pointer that will be returned. */
		/* Alloc from front */
		alloc_addr = (char *) ((u32) cur_header + offset);
		/*
		 *  Determine the remaining memory after the
		 *  allocated block.
		 *  Note: this cannot be negative because of above
		 *  comparison.
		 */
		remain_size = cur_size - adj_size - offset;

		/* Internal Assert that remain_size is a multiple of
		 * obj->min_align
		 */
		if (((remain_size & (obj->min_align - 1)) != 0)) {
			alloc_addr = NULL;
			break;
		}
		/*
		 *  If there is memory at the beginning (due to alignment
		 *  requirements), maintain it in the list.
		 *
		 *  offset and remain_size must be multiples of sizeof(struct
		 *  heapmemmp_header). Therefore the address of the new_header
		 *  below must be a multiple of the sizeof(struct
		 *  heapmemmp_header), thus maintaining the requirement.
		 */
		if (offset) {
			/* Adjust the cur_header size accordingly */
			cur_header->size = offset; /* B2 */
			/* Cache wb at end of this if block */

			/*
			 *  If there is remaining memory, add into the free
			 *  list.
			 *  Note: no need to coalesce and we have heapmemmp
			 *  locked so it is safe.
			 */
			if (offset && remain_size) {
				new_header = (struct heapmemmp_header *)
						((u32) alloc_addr + adj_size);

				/* cur_header has been inv at top of 'while' */
				/* loop */
				new_header->next = cur_header->next;  /* B1 */
				new_header->size = remain_size;       /* B1 */
#if 0
				if (unlikely(obj->cache_enabled))
					/* Writing back cur_header will */
					/* cache-wait */
					Cache_wbInv(new_header,
						sizeof(struct
							heapmemmp_header),
						Cache_Type_ALL,
						false); /* B1 */
#endif

				cur_header->next = sharedregion_get_srptr
							(new_header,
							 obj->region_id);
				BUG_ON(cur_header->next
					== SHAREDREGION_INVALIDSRPTR);
			}
#if 0
			/* Write back (and invalidate) new_header and */
			/* cur_header */
			if (unlikely(obj->cache_enabled))
				/* B2 */
				Cache_wbInv(cur_header,
					 sizeof(struct heapmemmp_header),
					 Cache_Type_ALL,
					 true);
#endif
		} else if (remain_size) {
			/*
			 *  If there is any remaining, link it in,
			 *  else point to the next free block.
			 *  Note: no need to coalesce and we have heapmemmp
			 *  locked so it is safe.
			 */

			new_header = (struct heapmemmp_header *)
				((u32) alloc_addr + adj_size);

			new_header->next = cur_header->next; /* A2, B3  */
			new_header->size = remain_size;      /* B3      */

#if 0
			if (unlikely(obj->cache_enabled))
				/* Writing back prev_header will cache-wait */
				Cache_wbInv(new_header,
				 sizeof(struct heapmemmp_header),
				 Cache_Type_ALL,
				 false); /* B3 */
#endif

			/* B4 */
			prev_header->next = sharedregion_get_srptr(new_header,
							obj->region_id);
		} else
			/* cur_header has been inv at top of 'while' loop */
			prev_header->next = cur_header->next; /* A2, B4 */

#if 0
		if (unlikely(obj->cache_enabled))
			/* B4 */
			Cache_wbInv(prev_header,
					 sizeof(struct heapmemmp_header),
					 Cache_Type_ALL,
					 true);
#endif

		/* Success, return the allocated memory */
		break;

	}

	gatemp_leave(obj->gate, key);

	if (alloc_addr == NULL)
		pr_err("heapmemmp_alloc returned NULL\n");
	return alloc_addr;

error:
	pr_err("heapmemmp_alloc failed status: %x\n", retval);
	return NULL;
}
EXPORT_SYMBOL(heapmemmp_alloc);

/*
 *  This will free a block of memory
 */
int heapmemmp_free(void *hphandle, void *addr, u32 size)
{
	struct heapmemmp_object *handle = NULL;
	s32 retval = 0;
	struct heapmemmp_obj *obj = NULL;
	int *key = NULL;
	struct heapmemmp_header *next_header;
	struct heapmemmp_header *new_header;
	struct heapmemmp_header *cur_header;
	u32 offset;

	if (atomic_cmpmask_and_lt(&(heapmemmp_module->ref_count),
				HEAPMEMMP_MAKE_MAGICSTAMP(0),
				HEAPMEMMP_MAKE_MAGICSTAMP(1)) == true) {
		retval = -ENODEV;
		goto error;
	}
	if (WARN_ON(hphandle == NULL)) {
		retval  = -EINVAL;
		goto error;
	}
	if (WARN_ON(addr == NULL)) {
		retval  = -EINVAL;
		goto error;
	}

	handle = (struct heapmemmp_object *)(hphandle);
	obj = (struct heapmemmp_obj *)handle->obj;
	if (WARN_ON(obj == NULL)) {
		retval = -EINVAL;
		goto error;
	}

	/*
	 * obj->attrs never changes, doesn't need Gate protection
	 * and Cache invalidate
	 */
	cur_header = (struct heapmemmp_header *) &(obj->attrs->head);

	/* Restore size to actual allocated size */
	offset = size & (obj->min_align - 1);
	if (offset != 0)
		size += obj->min_align - offset;

	key = gatemp_enter(obj->gate);

	new_header = (struct heapmemmp_header *) addr;

#if 0
	if (unlikely(obj->cacheEnabled)) {
		/* A1 */
		Cache_inv(cur_header,
				   sizeof(struct heapmemmp_header),
				   Cache_Type_ALL,
				   true);
	}
#endif
	next_header = sharedregion_get_ptr(cur_header->next);

	if (unlikely(!(((u32)new_header >= (u32)obj->buf)
				&& ((u32)new_header + size
					<= (u32)obj->buf + obj->buf_size)))) {
		retval = -EFAULT;
		goto error;
	}
	/* Go down freelist and find right place for buf */
	while ((next_header != NULL) && (next_header < new_header)) {
#if 0
		if (unlikely(obj->cacheEnabled))
			Cache_inv(next_header,
				   sizeof(struct heapmemmp_header),
				   Cache_Type_ALL,
				   true); /* A2 */
#endif

		/* Make sure the addr is not in this free block */
		if (unlikely((u32)new_header < \
				((u32)next_header + next_header->size))) {
			/* A2 */
			retval = -EFAULT;
			goto error;
		}

		cur_header = next_header;
		/* A2 */
		next_header = sharedregion_get_ptr(next_header->next);
	}

	new_header->next = sharedregion_get_srptr(next_header,
							obj->region_id);
	new_header->size = size;

	/* B1, A1 */
	cur_header->next = sharedregion_get_srptr(new_header,
							obj->region_id);

	/* Join contiguous free blocks */
	if (next_header != NULL) {
		/*
		 *  Verify the free size is not overlapping. Not all cases
		 *  are detectable, but it is worth a shot. Note: only do
		 *  this assert if next_header is non-NULL.
		 */
		if (unlikely(((u32)new_header + size) > (u32)next_header)) {
			/* A2 */
			retval = -EFAULT;
			goto error;
		}
		/* Join with upper block */
		if (((u32)new_header + size) == (u32)next_header) {
#if 0
			if (unlikely(obj->cacheEnabled))
				Cache_inv(next_header,
					   sizeof(struct heapmemmp_header),
					   Cache_Type_ALL,
					   true);
#endif
			new_header->next = next_header->next; /* A2, B2 */
			new_header->size += next_header->size; /* A2, B2 */
			/* Don't Cache_wbInv, this will be done later */
		}
	}

	/*
	 * Join with lower block. Make sure to check to see if not the
	 * first block. No need to invalidate attrs since head
	 * shouldn't change.
	 */
	if ((cur_header != &obj->attrs->head)
		&& (((u32) cur_header + cur_header->size)
			== (u32) new_header)) {
		/*
		 * Don't Cache_inv new_header since new_header has
		 * data that hasn't been written back yet (B2)
		 */
		cur_header->next = new_header->next; /* B1, B2 */
		cur_header->size += new_header->size; /* B1, B2 */
	}
#if 0
		if (unlikely(obj->cacheEnabled)) {
			Cache_wbInv(cur_header,
					 sizeof(struct heapmemmp_header),
					 Cache_Type_ALL,
					 false); /* B1 */
			Cache_wbInv(new_header,
					 sizeof(struct heapmemmp_header),
					 Cache_Type_ALL,
					 true);  /* B2 */
		}
#endif

	gatemp_leave(obj->gate, key);
	return 0;

error:
	pr_err("heapmemmp_free failed status: %x\n", retval);
	return retval;
}
EXPORT_SYMBOL(heapmemmp_free);

/*
 *  This will get memory statistics
 */
void heapmemmp_get_stats(void *hphandle, struct memory_stats *stats)
{
	struct heapmemmp_object *object = NULL;
	struct heapmemmp_obj *obj = NULL;
	struct heapmemmp_header *cur_header = NULL;
	int *key = NULL;
	s32 status = 0;

	if (atomic_cmpmask_and_lt(&(heapmemmp_module->ref_count),
				HEAPMEMMP_MAKE_MAGICSTAMP(0),
				HEAPMEMMP_MAKE_MAGICSTAMP(1)) == true) {
		status = -ENODEV;
		goto error;
	}
	if (WARN_ON(hphandle == NULL)) {
		status  = -EINVAL;
		goto error;
	}
	if (WARN_ON(stats == NULL)) {
		status  = -EINVAL;
		goto error;
	}

	object = (struct heapmemmp_object *)(hphandle);
	obj = (struct heapmemmp_obj *)object->obj;
	if (WARN_ON(obj == NULL)) {
		status = -EINVAL;
		goto error;
	}

	stats->total_size = obj->buf_size;
	stats->total_free_size = 0; /* determined later */
	stats->largest_free_size = 0; /* determined later */

	key = gatemp_enter(obj->gate);
	cur_header = sharedregion_get_ptr(obj->attrs->head.next);

	while (cur_header != NULL) {
#if 0
		/* Invalidate cur_header */
		if (unlikely(obj->cacheEnabled)) {
			Cache_inv(cur_header,
				   sizeof(struct heapmemmp_header),
				   Cache_Type_ALL,
				   true);
		}
#endif
		stats->total_free_size += cur_header->size;
		if (stats->largest_free_size < cur_header->size)
			stats->largest_free_size = cur_header->size;

		/* This condition is required to avoid assertions during call
		 * to SharedRegion_getPtr  because at the end of the
		 * calculation cur_header->next  will become
		 * SHAREDREGION_INVALIDSRPTR.
		 */
		if (cur_header->next != SHAREDREGION_INVALIDSRPTR)
			cur_header = sharedregion_get_ptr(cur_header->next);
		else
			cur_header = NULL;
	}

	gatemp_leave(obj->gate, key);
error:
	if (status < 0)
		pr_err("heapmemmp_get_stats status: %x\n", status);
}
EXPORT_SYMBOL(heapmemmp_get_stats);

/*
 *  Indicate whether the heap may block during an alloc or free call
 */
bool heapmemmp_isblocking(void *handle)
{
	bool isblocking = false;
	s32 retval  = 0;

	if (WARN_ON(handle == NULL)) {
		retval  = -EINVAL;
		goto error;
	}

	/* TBD: Figure out how to determine whether the gate is blocking */
	isblocking = true;

	/* retval true  Heap blocks during alloc/free calls */
	/* retval false Heap does not block during alloc/free calls */
	return isblocking;

error:
	pr_err("heapmemmp_isblocking status: %x\n", retval);
	return isblocking;
}
EXPORT_SYMBOL(heapmemmp_isblocking);

/*
 *  This will get extended statistics
 */
void heapmemmp_get_extended_stats(void *hphandle,
				 struct heapmemmp_extended_stats *stats)
{
	int status = 0;
	struct heapmemmp_object *object = NULL;
	struct heapmemmp_obj *obj = NULL;

	if (atomic_cmpmask_and_lt(&(heapmemmp_module->ref_count),
				HEAPMEMMP_MAKE_MAGICSTAMP(0),
				HEAPMEMMP_MAKE_MAGICSTAMP(1)) == true) {
		status = -ENODEV;
		goto error;
	}
	if (WARN_ON(heapmemmp_module->nameserver == NULL)) {
		status  = -EINVAL;
		goto error;
	}
	if (WARN_ON(hphandle == NULL)) {
		status  = -EINVAL;
		goto error;
	}
	if (WARN_ON(stats == NULL)) {
		status = -EINVAL;
		goto error;
	}

	object = (struct heapmemmp_object *)hphandle;
	obj = (struct heapmemmp_obj *)object->obj;
	if (WARN_ON(obj == NULL)) {
		status = -EINVAL;
		goto error;
	}

	stats->buf   = obj->buf;
	stats->size  = obj->buf_size;

	return;

error:
	pr_err("heapmemmp_get_extended_stats status: %x\n", status);
}
EXPORT_SYMBOL(heapmemmp_get_extended_stats);

/*
 *  This will get amount of shared memory required for
 *  creation of each instance
 */
int heapmemmp_shared_mem_req(const struct heapmemmp_params *params)
{
	int mem_req = 0;
	s32 retval = 0;
	u32 region_id;
	u32 min_align;

	if (params == NULL) {
		retval = -EINVAL;
		goto error;
	}

	if (params->shared_addr == NULL)
		region_id = params->region_id;
	else
		region_id = sharedregion_get_id(params->shared_addr);

	if (region_id == SHAREDREGION_INVALIDREGIONID)  {
		retval = -EFAULT;
		goto error;
	}

	min_align = sizeof(struct heapmemmp_header);
	if (sharedregion_get_cache_line_size(region_id) > min_align)
		min_align = sharedregion_get_cache_line_size(region_id);

	/* Add size of heapmemmp Attrs */
	mem_req = ROUND_UP(sizeof(struct heapmemmp_attrs), min_align);

	/* Add the buffer size */
	mem_req += params->shared_buf_size;

	/* Make sure the size is a multiple of min_align (round down) */
	mem_req = (mem_req / min_align) * min_align;

	return mem_req;

error:
	pr_err("heapmemmp_shared_mem_req retval: %x\n", retval);
	return mem_req;
}
EXPORT_SYMBOL(heapmemmp_shared_mem_req);


/*
 *  Open existing heapmemmp based on address
 */
int
heapmemmp_open_by_addr(void *shared_addr, void **handle_ptr)
{
	s32 retval = 0;
	bool done_flag = false;
	struct heapmemmp_attrs *attrs = NULL;
	struct list_head *elem = NULL;
	u16 id = 0;
	struct heapmemmp_params params;
	struct heapmemmp_obj *obj = NULL;

	BUG_ON(handle_ptr == NULL);
	BUG_ON(shared_addr == NULL);

	if (unlikely(atomic_cmpmask_and_lt(&(heapmemmp_module->ref_count),
					HEAPMEMMP_MAKE_MAGICSTAMP(0),
					HEAPMEMMP_MAKE_MAGICSTAMP(1))
					 == true)) {
		retval = -ENODEV;
		goto error;
	}

	if (unlikely(handle_ptr == NULL)) {
		retval = -EINVAL;
		goto error;
	}

	/* First check in the local list */
	list_for_each(elem, (struct list_head *)&heapmemmp_module->obj_list) {
		obj = (struct heapmemmp_obj *)elem;
		if (obj->params.shared_addr == shared_addr) {
			retval = mutex_lock_interruptible(heapmemmp_module->
								local_lock);
			if (retval < 0)
				goto error;

			if (obj->owner.proc_id == multiproc_self())
				obj->owner.open_count++;

			mutex_unlock(heapmemmp_module->local_lock);
			*handle_ptr = obj->top;
			done_flag = true;
			break;
		}
	}

	/* If not already existing locally, create object locally for open. */
	if (unlikely(done_flag == false)) {
		heapmemmp_params_init(&params);
		params.shared_addr = shared_addr;
		attrs = (struct heapmemmp_attrs *) shared_addr;
		id = sharedregion_get_id(shared_addr);
#if 0
		if (unlikely(sharedregion_is_cache_enabled(id))) {
			Cache_inv(attrs,
				  sizeof(struct heapmemmp_attrs),
				  Cache_Type_ALL,
				  true);
		}
#endif
		if (unlikely(attrs->status != HEAPMEMMP_CREATED)) {
			*handle_ptr = NULL;
			retval = -ENOENT;
			goto error;
		}

		retval = _heapmemmp_create(handle_ptr, &params, false);
		if (unlikely(retval < 0))
			goto error;
	}
	return 0;

error:
	pr_err("heapmemmp_open_by_addr status: %x\n", retval);
	return retval;
}


/* =============================================================================
 * Internal functions
 * =============================================================================
 */
/*
 *  Slice and dice the buffer up into the correct size blocks and
 *  add to the freelist.
 */
static int heapmemmp_post_init(struct heapmemmp_object *handle)
{
	s32 retval = 0;
	struct heapmemmp_obj *obj = NULL;
	struct heapmemmp_object *region_heap = NULL;
	struct heapmemmp_params params;

	BUG_ON(handle == NULL);

	obj = (struct heapmemmp_obj *) handle->obj;
	if (obj->attrs == NULL) {
		heapmemmp_params_init(&params);
		params.region_id = obj->region_id;
		params.shared_buf_size = obj->buf_size;
		obj->alloc_size = heapmemmp_shared_mem_req(&params);
		region_heap = sharedregion_get_heap(obj->region_id);

		if (region_heap == NULL) {
			retval = -EFAULT;
			goto error;
		}

		obj->attrs = sl_heap_alloc(region_heap,
					obj->alloc_size,
					obj->min_align);

		if (obj->attrs == NULL) {
			retval = -ENOMEM;
			goto error;
		}
		obj->buf = (void *)((u32)obj->attrs +
				sizeof(struct heapmemmp_attrs));
	}

	/* Round obj->buf up by obj->min_align */
	obj->buf = (void *) ROUND_UP((u32)obj->buf, (obj->min_align));

	if (unlikely(obj->buf_size
			< sharedregion_get_cache_line_size(obj->region_id))) {
		retval = -EFAULT;
		goto error;
	}

	/* Make sure the size is a multiple of obj->min_align */
	obj->buf_size = (obj->buf_size / obj->min_align) * obj->min_align;

	obj->attrs->gatemp_addr = gatemp_get_shared_addr(obj->gate);
	obj->attrs->buf_ptr = sharedregion_get_srptr(obj->buf, obj->region_id);

	/* Store computed obj->buf_size in shared mem */
	obj->attrs->head.size = obj->buf_size;

	/* Place the initial header */
	heapmemmp_restore((struct heapmemmp_object *) handle);

	/* Last thing, set the status */
	obj->attrs->status = HEAPMEMMP_CREATED;
#if 0
	if (unlikely(obj->cacheEnabled))
		Cache_wbInv((Ptr) obj->attrs,
					 sizeof(heapmemmp_Attrs),
					 Cache_Type_ALL,
					 true);
#endif

	return 0;
error:
	pr_err("heapmemmp_post_init status: %x\n", retval);
	return retval;
}


/*
 *  Restore an instance to it's original created state.
 */
void
heapmemmp_restore(void *handle)
{
	struct heapmemmp_header *beg_header = NULL;
	struct heapmemmp_obj    *obj        = NULL;

	obj = ((struct heapmemmp_object *) handle)->obj;
	BUG_ON(obj == NULL);

	/*
	 *  Fill in the top of the memory block
	 *  next: pointer will be NULL (end of the list)
	 *  size: size of this block
	 *  NOTE: no need to Cache_inv because obj->attrs->bufPtr
	 *  should be const
	 */
	beg_header = (struct heapmemmp_header *) obj->buf;
	beg_header->next = (u32 *)SHAREDREGION_INVALIDSRPTR;
	beg_header->size = obj->buf_size;

	obj->attrs->head.next = (u32 *)obj->attrs->buf_ptr;
#if 0
	if (unlikely(obj->cacheEnabled)) {
		Cache_wbInv((Ptr)&(obj->attrs->head),
				   sizeof(struct heapmemmp_header),
				   Cache_Type_ALL,
				   false);
		Cache_wbInv(begHeader,
			    sizeof(struct heapmemmp_header),
			    Cache_Type_ALL,
			    true);
	}
#endif
}
