/*
 *  heapbufmp.c
 *
 *  Heap module manages variable size buffers that can be used
 *  in a multiprocessor system with shared memory.
 *
 *  Copyright(C) 2008-2009 Texas Instruments, Inc.
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
#include <heapbufmp.h>

/*
 *  Name of the reserved nameserver used for heapbufmp.
 */
#define HEAPBUFMP_NAMESERVER			"HeapBufMP"
/* brief Macro to make a correct module magic number with ref_count */
#define HEAPBUFMP_MAKE_MAGICSTAMP(x)	((HEAPBUFMP_MODULEID << 12) | (x))
/* Max heapbufmp name length */
#define HEAPBUFMP_MAX_NAME_LEN			32
/* Max number of runtime entries */
#define HEAPBUFMP_MAX_RUNTIME_ENTRIES		32

#define ROUND_UP(a, b)	(((a) + ((b) - 1)) & (~((b) - 1)))

/*
 *  Structure defining attribute parameters for the heapbufmp module
 */
struct heapbufmp_attrs {
	VOLATILE u32 status; /* Module status */
	VOLATILE u32 *gatemp_addr; /* gatemp shared address(shm safe) */
	VOLATILE u32 *buf_ptr; /* Memory managed by instance */
	VOLATILE u32 num_free_blocks; /* Number of free blocks */
	VOLATILE u32 min_free_blocks; /* Min number of free blocks */
	VOLATILE u32 block_size; /* True size of each block */
	VOLATILE u32 align; /* Alignment of each block */
	VOLATILE u32 num_blocks; /* Number of individual blocks */
	VOLATILE u16 exact; /* For 'exact' allocation */
};

/*
 *  Structure defining processor related information for the
 *  heapbufmp module
 */
struct heapbufmp_proc_attrs {
	bool creator; /* Creator or opener */
	u16 proc_id; /* Processor identifier */
	u32 open_count; /* open count in a processor */
};

/*
 *  Structure for heapbufmp module state
 */
struct heapbufmp_module_object {
	atomic_t ref_count; /* Reference count */
	void *nameserver; /* Nameserver handle */
	struct list_head obj_list; /* List holding created objects */
	struct mutex *local_lock; /* lock for protecting obj_list */
	struct heapbufmp_config cfg; /* Current config values */
	struct heapbufmp_config default_cfg; /* Default config values */
	struct heapbufmp_params default_inst_params; /* Default instance
						creation parameters */
};

static struct heapbufmp_module_object heapbufmp_state = {
	.obj_list = LIST_HEAD_INIT(heapbufmp_state.obj_list),
	.default_cfg.max_name_len = HEAPBUFMP_MAX_NAME_LEN,
	.default_cfg.max_runtime_entries = HEAPBUFMP_MAX_RUNTIME_ENTRIES,
	.default_cfg.track_allocs = false,
	.default_inst_params.gate = NULL,
	.default_inst_params.exact = false,
	.default_inst_params.name = NULL,
	.default_inst_params.align = 1u,
	.default_inst_params.num_blocks = 0u,
	.default_inst_params.block_size = 0u,
	.default_inst_params.region_id = 0,
	.default_inst_params.shared_addr = NULL,
};

/* Pointer to module state */
static struct heapbufmp_module_object *heapbufmp_module = &heapbufmp_state;

/*
 *  Structure for the handle for the heapbufmp
 */
struct heapbufmp_obj {
	struct list_head list_elem; /* Used for creating a linked list */
	struct heapbufmp_attrs *attrs; /* The shared attributes structure */
	void *gate; /* Lock used for critical region management */
	void *ns_key; /* nameserver key required for remove */
	bool cache_enabled; /* Whether to do cache calls */
	u16 region_id; /* shared region index */
	u32 alloc_size; /* Size of allocated shared memory */
	char *buf; /* Pointer to allocated memory */
	void *free_list; /* List of free buffers */
	u32 block_size; /* Adjusted block_size */
	u32 align; /* Adjusted alignment */
	u32 num_blocks; /* Number of blocks in buffer */
	bool exact; /* Exact match flag */
	struct heapbufmp_proc_attrs owner; /* owner processor info */
	void *top; /* Pointer to the top object */
	struct heapbufmp_params params; /* The creation parameter structure */
};

#define heapbufmp_object heap_object

/* =============================================================================
 * Forward declarations of internal functions
 * =============================================================================
 */
static int heapbufmp_post_init(struct heapbufmp_object *handle);

/* =============================================================================
 * APIs called directly by applications
 * =============================================================================
 */
/*
 * ======== heapbufmp_get_config ========
 *  Purpose:
 *  This will get default configuration for the
 *  heapbufmp module
 */
int heapbufmp_get_config(struct heapbufmp_config *cfgparams)
{
	s32 retval = 0;

	BUG_ON(cfgparams == NULL);

	if (cfgparams == NULL) {
		retval = -EINVAL;
		goto error;
	}

	if (atomic_cmpmask_and_lt(&(heapbufmp_module->ref_count),
				HEAPBUFMP_MAKE_MAGICSTAMP(0),
				HEAPBUFMP_MAKE_MAGICSTAMP(1)) == true)
		memcpy(cfgparams, &heapbufmp_module->default_cfg,
					sizeof(struct heapbufmp_config));
	else
		memcpy(cfgparams, &heapbufmp_module->cfg,
					sizeof(struct heapbufmp_config));
	return 0;
error:
	pr_err("heapbufmp_get_config failed status: %x\n", retval);
	return retval;
}
EXPORT_SYMBOL(heapbufmp_get_config);

/*
 * ======== heapbufmp_setup ========
 *  Purpose:
 *  This will setup the heapbufmp module
 *
 *  This function sets up the heapbufmp module. This function
 *  must be called before any other instance-level APIs can be
 *  invoked.
 *  Module-level configuration needs to be provided to this
 *  function. If the user wishes to change some specific config
 *  parameters, then heapbufmp_getconfig can be called to get
 *  the configuration filled with the default values. After this,
 *  only the required configuration values can be changed. If the
 *  user does not wish to make any change in the default parameters,
 *  the application can simply call heapbufmp_setup with NULL
 *  parameters. The default parameters would get automatically used.
 */
int heapbufmp_setup(const struct heapbufmp_config *cfg)
{
	struct nameserver_params params;
	struct heapbufmp_config tmp_cfg;
	s32 retval  = 0;

	/* This sets the ref_count variable  not initialized, upper 16 bits is
	 * written with module Id to ensure correctness of ref_count variable
	 */
	atomic_cmpmask_and_set(&heapbufmp_module->ref_count,
					HEAPBUFMP_MAKE_MAGICSTAMP(0),
					HEAPBUFMP_MAKE_MAGICSTAMP(0));
	if (atomic_inc_return(&heapbufmp_module->ref_count)
					!= HEAPBUFMP_MAKE_MAGICSTAMP(1)) {
		return 1;
	}

	if (cfg == NULL) {
		heapbufmp_get_config(&tmp_cfg);
		cfg = &tmp_cfg;
	}

	if (cfg->max_name_len == 0 ||
		cfg->max_name_len > HEAPBUFMP_MAX_NAME_LEN) {
		retval = -EINVAL;
		goto error;
	}

	/* Initialize the parameters */
	nameserver_params_init(&params);
	params.max_value_len = sizeof(u32);
	params.max_name_len = cfg->max_name_len;
	params.max_runtime_entries = cfg->max_runtime_entries;

	/* Create the nameserver for modules */
	heapbufmp_module->nameserver =
		nameserver_create(HEAPBUFMP_NAMESERVER, &params);
	if (heapbufmp_module->nameserver == NULL) {
		retval = -EFAULT;
		goto error;
	}

	/* Construct the list object */
	INIT_LIST_HEAD(&heapbufmp_module->obj_list);
	/* Copy config info */
	memcpy(&heapbufmp_module->cfg, cfg, sizeof(struct heapbufmp_config));
	/* Create a lock for protecting list object */
	heapbufmp_module->local_lock = kmalloc(sizeof(struct mutex),
								GFP_KERNEL);
	mutex_init(heapbufmp_module->local_lock);
	if (heapbufmp_module->local_lock == NULL) {
		retval = -ENOMEM;
		heapbufmp_destroy();
		goto error;
	}

	return 0;

error:
	pr_err("heapbufmp_setup failed status: %x\n", retval);
	return retval;
}
EXPORT_SYMBOL(heapbufmp_setup);

/*
 * ======== heapbufmp_destroy ========
 *  Purpose:
 *  This will destroy the heapbufmp module
 */
int heapbufmp_destroy(void)
{
	s32 retval  = 0;
	struct mutex *lock = NULL;
	struct heapbufmp_obj *obj = NULL;

	if (atomic_cmpmask_and_lt(&(heapbufmp_module->ref_count),
				HEAPBUFMP_MAKE_MAGICSTAMP(0),
				HEAPBUFMP_MAKE_MAGICSTAMP(1)) == true) {
		retval = -ENODEV;
		goto error;
	}

	if (atomic_dec_return(&heapbufmp_module->ref_count)
				== HEAPBUFMP_MAKE_MAGICSTAMP(0)) {
		/* Temporarily increment ref_count here. */
		atomic_set(&heapbufmp_module->ref_count,
					HEAPBUFMP_MAKE_MAGICSTAMP(1));

		/*  Check if any heapbufmp instances have not been
		 *  deleted/closed so far. if there any, delete or close them
		 */
		list_for_each_entry(obj, &heapbufmp_module->obj_list,
								list_elem) {
			if (obj->owner.proc_id == multiproc_get_id(NULL))
				retval = heapbufmp_delete(&obj->top);
			else
				retval = heapbufmp_close(obj->top);

			if (list_empty(&heapbufmp_module->obj_list))
				break;

			if (retval < 0)
				goto error;
		}

		/* Again reset ref_count. */
		atomic_set(&heapbufmp_module->ref_count,
					HEAPBUFMP_MAKE_MAGICSTAMP(0));

		if (likely(heapbufmp_module->nameserver != NULL)) {
			retval = nameserver_delete(&heapbufmp_module->
							nameserver);
			if (unlikely(retval != 0))
				goto error;
		}

		/* Delete the list lock */
		lock = heapbufmp_module->local_lock;
		retval = mutex_lock_interruptible(lock);
		if (retval)
			goto error;

		heapbufmp_module->local_lock = NULL;
		mutex_unlock(lock);
		kfree(lock);
		memset(&heapbufmp_module->cfg, 0,
				sizeof(struct heapbufmp_config));
	}

	return 0;

error:
	pr_err("heapbufmp_destroy failed status: %x\n", retval);
	return retval;
}
EXPORT_SYMBOL(heapbufmp_destroy);

/*
 * ======== heapbufmp_params_init ========
 *  Purpose:
 *  This will get the intialization prams for a heapbufmp
 *  module instance
 */
void heapbufmp_params_init(struct heapbufmp_params *params)
{
	s32 retval  = 0;

	if (atomic_cmpmask_and_lt(&(heapbufmp_module->ref_count),
				HEAPBUFMP_MAKE_MAGICSTAMP(0),
				HEAPBUFMP_MAKE_MAGICSTAMP(1)) == true) {
		retval = -ENODEV;
		goto error;
	}

	BUG_ON(params == NULL);

	memcpy(params, &heapbufmp_module->default_inst_params,
				sizeof(struct heapbufmp_params));

	return;
error:
	pr_err("heapbufmp_params_init failed status: %x\n", retval);
}
EXPORT_SYMBOL(heapbufmp_params_init);

/*
 * ======== _heapbufmp_create ========
 *  Purpose:
 *  This will create a new instance of heapbufmp module
 *  This is an internal function as both heapbufmp_create
 *  and heapbufmp_open use the functionality
 *
 *  NOTE: The lock to protect the shared memory area
 *  used by heapbufmp is provided by the consumer of
 *  heapbufmp module
 */
static int _heapbufmp_create(void **handle_ptr,
				const struct heapbufmp_params *params,
				u32 create_flag)
{
	s32 retval  = 0;
	struct heapbufmp_obj *obj = NULL;
	struct heapbufmp_object *handle = NULL;
	void *gate_handle = NULL;
	void *local_addr = NULL;
	u32 *shared_shm_base;
	u32 min_align;

	if (atomic_cmpmask_and_lt(&(heapbufmp_module->ref_count),
				HEAPBUFMP_MAKE_MAGICSTAMP(0),
				HEAPBUFMP_MAKE_MAGICSTAMP(1)) == true) {
		retval = -ENODEV;
		goto error;
	}

	/* No need for parameter checks, since this is an internal function. */

	/* Initialize return parameter. */
	*handle_ptr = NULL;

	handle = kmalloc(sizeof(struct heapbufmp_object), GFP_KERNEL);
	if (handle == NULL) {
		retval = -ENOMEM;
		goto error;
	}

	obj = kmalloc(sizeof(struct heapbufmp_obj), GFP_KERNEL);
	if (obj == NULL) {
		retval = -ENOMEM;
		goto error;
	}

	handle->obj = (struct heapbufmp_obj *)obj;
	handle->alloc = &heapbufmp_alloc;
	handle->free = &heapbufmp_free;
	handle->get_stats = &heapbufmp_get_stats;
	handle->is_blocking = &heapbufmp_isblocking;

	obj->ns_key	 = NULL;
	obj->alloc_size = 0;

	/* Put in local ilst */
	retval = mutex_lock_interruptible(heapbufmp_module->local_lock);
	if (retval < 0)
		goto error;

	INIT_LIST_HEAD(&obj->list_elem);
	list_add(&obj->list_elem, &heapbufmp_module->obj_list);
	mutex_unlock(heapbufmp_module->local_lock);

	if (create_flag == false) {
		obj->owner.creator = false;
		obj->owner.open_count = 0;
		obj->owner.proc_id = MULTIPROC_INVALIDID;
		obj->top = handle;

		obj->attrs = (struct heapbufmp_attrs *) params->shared_addr;

		/* No need to Cache_inv- already done in openByAddr() */
		obj->align          = obj->attrs->align;
		obj->num_blocks     = obj->attrs->num_blocks;
		obj->block_size     = obj->attrs->block_size;
		obj->exact          = obj->attrs->exact;
		obj->buf            = sharedregion_get_ptr((u32 *)obj->attrs->
								buf_ptr);
		obj->region_id      = sharedregion_get_id(obj->buf);

		/* Set min_align */
		min_align = 4; /* memory_get_max_default_type_align(); */
		if (sharedregion_get_cache_line_size(obj->region_id) >
								min_align)
			min_align = sharedregion_get_cache_line_size(obj->
								region_id);
		obj->cache_enabled = sharedregion_is_cache_enabled(obj->
								region_id);

		local_addr = sharedregion_get_ptr((u32 *)obj->attrs->
								gatemp_addr);
		retval = gatemp_open_by_addr(local_addr, &gate_handle);

		if (retval < 0) {
			retval = -EFAULT;
			goto error;
		}
		obj->gate = gate_handle;

		/* Open the ListMP */
		local_addr = (void *) ROUND_UP(((u32)obj->attrs
					 + sizeof(struct heapbufmp_attrs)),
					 min_align);
		retval = listmp_open_by_addr(local_addr, &(obj->free_list));

		if (retval < 0) {
			retval = -EFAULT;
			goto error;
		}
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

		obj->exact      = params->exact;
		obj->align      = params->align;
		obj->num_blocks = params->num_blocks;

		if (params->shared_addr == NULL) {
			/* Creating using a shared region ID */
			/* It is allowed to have NULL name for an anonymous, not
			 * to be opened by name, heap.
			 */
			/* Will be allocated in post_init */
			obj->attrs = NULL;
			obj->region_id = params->region_id;
		} else {
			/* Creating using shared_addr */
			obj->region_id = sharedregion_get_id(
							params->shared_addr);

			/* Assert that the buffer is in a valid shared
			 * region
			 */
			if (obj->region_id == SHAREDREGION_INVALIDREGIONID) {
				retval = -EFAULT;
				goto error;
			} else if (((u32) params->shared_addr
					% sharedregion_get_cache_line_size(obj->
					region_id) != 0)) {
				retval = -EFAULT;
				goto error;
			}
			obj->attrs = (struct heapbufmp_attrs *)
							params->shared_addr;
		}

		obj->cache_enabled = sharedregion_is_cache_enabled(
								obj->region_id);

		/* Fix the alignment (alignment may be needed even if
		 * cache is disabled)
		 */
		obj->align = 4; /* memory_get_max_default_type_align(); */
		if (sharedregion_get_cache_line_size(obj->region_id) >
								obj->align)
			obj->align = sharedregion_get_cache_line_size(
								obj->region_id);

		/* Round the block_size up by the adjusted alignment */
		obj->block_size = ROUND_UP(params->block_size, obj->align);

		retval = heapbufmp_post_init(handle);
		if (retval < 0) {
			retval = -EFAULT;
			goto error;
		}

		/* Populate the params member */
		memcpy(&obj->params, params, sizeof(struct heapbufmp_params));
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
		shared_shm_base = sharedregion_get_srptr((void *)obj->attrs,
							obj->region_id);
		if (obj->params.name != NULL) {
			obj->ns_key = nameserver_add_uint32(
						heapbufmp_module->nameserver,
						params->name,
						(u32)shared_shm_base);
			if (obj->ns_key == NULL) {
				retval = -EFAULT;
				goto error;
			}
		}
	}

	*handle_ptr = (void *)handle;
	return retval;


error:
	*handle_ptr = (void *)handle;

	/* Do whatever cleanup is required*/
	if (create_flag == true)
		heapbufmp_delete(handle_ptr);
	else
		heapbufmp_close(handle_ptr);

	pr_err("_heapbufmp_create failed status: %x\n", retval);
	return retval;
}

/*
 * ======== heapbufmp_create ========
 *  Purpose:
 *  This will create a new instance of heapbufmp module
 */
void *heapbufmp_create(const struct heapbufmp_params *params)
{
	s32 retval = 0;
	struct heapbufmp_object *handle = NULL;
	struct heapbufmp_params sparams;

	BUG_ON(params == NULL);

	if (atomic_cmpmask_and_lt(&(heapbufmp_module->ref_count),
				HEAPBUFMP_MAKE_MAGICSTAMP(0),
				HEAPBUFMP_MAKE_MAGICSTAMP(1)) == true) {
		retval = -ENODEV;
		goto error;
	}

	if (params == NULL) {
		retval = -EINVAL;
		goto error;
	}

	if (params->block_size == 0) {
		retval = -EINVAL;
		goto error;
	}

	if (params->num_blocks == 0) {
		retval = -EINVAL;
		goto error;
	}

	memcpy(&sparams, (void *)params, sizeof(struct heapbufmp_params));
	retval = _heapbufmp_create((void **)&handle, &sparams, true);
	if (retval < 0)
		goto error;

	return (void *)handle;

error:
	pr_err("heapbufmp_create failed status: %x\n", retval);
	return (void *)handle;
}
EXPORT_SYMBOL(heapbufmp_create);

/*
 * ======== heapbufmp_delete ========
 *  Purpose:
 *  This will delete an instance of heapbufmp module
 */
int heapbufmp_delete(void **handle_ptr)
{
	int status = 0;
	struct heapbufmp_object *handle = NULL;
	struct heapbufmp_obj *obj = NULL;
	struct heapbufmp_params *params = NULL;
	struct heapbufmp_object *region_heap = NULL;
	s32 retval = 0;
	int *key;

	if (atomic_cmpmask_and_lt(&(heapbufmp_module->ref_count),
				HEAPBUFMP_MAKE_MAGICSTAMP(0),
				HEAPBUFMP_MAKE_MAGICSTAMP(1)) == true) {
		retval = -ENODEV;
		goto error;
	}

	if (WARN_ON(handle_ptr == NULL)) {
		retval = -EINVAL;
		goto error;
	}

	handle = (struct heapbufmp_object *)(*handle_ptr);
	if (WARN_ON(handle == NULL)) {
		retval = -EINVAL;
		goto error;
	}

	obj = (struct heapbufmp_obj *)handle->obj;
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

		retval = mutex_lock_interruptible(heapbufmp_module->local_lock);
		if (retval < 0)
			goto lock_error;

		/* Remove frmo the local list */
		list_del(&obj->list_elem);

		mutex_unlock(heapbufmp_module->local_lock);

		params = (struct heapbufmp_params *) &obj->params;

		if (likely(params->name != NULL)) {
			if (likely(obj->ns_key != NULL)) {
				nameserver_remove_entry(heapbufmp_module->
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
				cache_wbinv(obj->attrs, sizeof(struct
					heapbufmp_attrs), CACHE_TYPE_ALL,
					true);
			}
#endif
		}

		/* Release the shared lock */
		gatemp_leave(obj->gate, key);

		if (obj->free_list != NULL)
			/* Free the list */
			listmp_delete(&obj->free_list);

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
		kfree(handle);

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
	pr_err("heapbufmp_delete failed status: %x\n", retval);
	return retval;
}
EXPORT_SYMBOL(heapbufmp_delete);

/*
 * ======== heapbufmp_open  ========
 *  Purpose:
 *  This will opens a created instance of heapbufmp
 *  module
 */
int heapbufmp_open(char *name, void **handle_ptr)
{
	s32 retval = 0;
	u32 *shared_shm_base = SHAREDREGION_INVALIDSRPTR;
	u32 *shared_addr = NULL;
	struct heapbufmp_obj *obj = NULL;
	bool done_flag = false;
	struct list_head *elem = NULL;

	BUG_ON(name  == NULL);
	BUG_ON(handle_ptr == NULL);

	if (unlikely(
			atomic_cmpmask_and_lt(&(heapbufmp_module->ref_count),
				HEAPBUFMP_MAKE_MAGICSTAMP(0),
				HEAPBUFMP_MAKE_MAGICSTAMP(1)) == true)) {
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
	list_for_each(elem, &heapbufmp_module->obj_list) {
		obj = (struct heapbufmp_obj *)elem;
		if (obj->params.name != NULL) {
			if (strcmp(obj->params.name, name) == 0) {
				retval = mutex_lock_interruptible(
					heapbufmp_module->local_lock);
				if (retval < 0)
					goto error;
				/* Check if we have created the heapbufmp or
				 * not
				 */
				if (obj->owner.proc_id == multiproc_self())
					obj->owner.open_count++;

				*handle_ptr = (void *)obj->top;
				mutex_unlock(heapbufmp_module->local_lock);
				done_flag = true;
				break;
			}
		}
	}

	if (likely(done_flag == false)) {
		/* Find in name server */
		retval = nameserver_get_uint32(heapbufmp_module->nameserver,
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

		retval = heapbufmp_open_by_addr(shared_addr, handle_ptr);

		if (unlikely(retval < 0))
			goto error;
	}

	return 0;

error:
	pr_err("heapbufmp_open failed status: %x\n", retval);
	return retval;
}
EXPORT_SYMBOL(heapbufmp_open);

/*
 * ======== heapbufmp_close  ========
 *  Purpose:
 *  This will closes previously opened/created instance
 *  of heapbufmp module
 */
int heapbufmp_close(void **handle_ptr)
{
	struct heapbufmp_object *handle = NULL;
	struct heapbufmp_obj *obj = NULL;
	s32 retval = 0;

	if (atomic_cmpmask_and_lt(&(heapbufmp_module->ref_count),
				HEAPBUFMP_MAKE_MAGICSTAMP(0),
				HEAPBUFMP_MAKE_MAGICSTAMP(1)) == true) {
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

	handle = (struct heapbufmp_object *)(*handle_ptr);
	obj = (struct heapbufmp_obj *)handle->obj;

	if (obj != NULL) {
		retval = mutex_lock_interruptible(heapbufmp_module->
							local_lock);
		if (retval)
			goto error;

		/* opening an instance created locally */
		if (obj->owner.proc_id == multiproc_self())
			obj->owner.open_count--;

		/* Check if HeapMemMP is opened on same processor
		 * and this is the last closure.
		 */
		if ((obj->owner.creator == false)
				&& (obj->owner.open_count == 0)) {
			list_del(&obj->list_elem);

			if (obj->free_list != NULL)
				/* Close the list */
				listmp_close(&obj->free_list);

			if (obj->gate != NULL)
				/* Close the instance gate */
				gatemp_close(&obj->gate);

			/* Now free the handle */
			kfree(obj);
			obj = NULL;
			kfree(handle);
			*handle_ptr = NULL;
		}

		mutex_unlock(heapbufmp_module->local_lock);
	} else {
		kfree(handle);
		*handle_ptr = NULL;
	}
	return 0;

error:
	pr_err("heapbufmp_close failed status: %x\n", retval);
	return retval;
}
EXPORT_SYMBOL(heapbufmp_close);

/*
 * ======== heapbufmp_alloc  ========
 *  Purpose:
 *  This will allocs a block of memory
 */
void *heapbufmp_alloc(void *hphandle, u32 size, u32 align)
{
	char *block = NULL;
	struct heapbufmp_object *handle = NULL;
	struct heapbufmp_obj *obj = NULL;
	int *key;
	s32 retval = 0;

	if (atomic_cmpmask_and_lt(&(heapbufmp_module->ref_count),
				HEAPBUFMP_MAKE_MAGICSTAMP(0),
				HEAPBUFMP_MAKE_MAGICSTAMP(1)) == true) {
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

	handle = (struct heapbufmp_object *)(hphandle);
	obj = (struct heapbufmp_obj *)handle->obj;
	if (WARN_ON(obj == NULL)) {
		retval  = -EINVAL;
		goto error;
	}

	if (WARN_ON(unlikely(size > obj->block_size))) {
		retval  = -EINVAL;
		goto error;
	}
	if (WARN_ON(unlikely((obj->exact == true)
			  && (size != obj->block_size)))) {
		retval  = -EINVAL;
		goto error;
	}
	if (WARN_ON(unlikely(align > obj->align))) {
		retval  = -EINVAL;
		goto error;
	}

	/*key = gatemp_enter(obj->gate); gate protection acquired in listmp */
	block = listmp_get_head((struct listmp_object *) obj->free_list);
	if (unlikely(block == NULL)) {
		retval = -ENOMEM;
		goto error;
	}
	key = gatemp_enter(obj->gate); /*gatemp call moved down */
	if (unlikely(heapbufmp_module->cfg.track_allocs)) {
#if 0
		/* Make sure the attrs are not in cache */
		if (unlikely(obj->cache_enabled)) {
			Cache_inv((Ptr) obj->attrs,
					   sizeof(heapbufmp_attrs),
					   Cache_Type_ALL,
					   true);
		}
#endif

		obj->attrs->num_free_blocks--;

		if (obj->attrs->num_free_blocks
			< obj->attrs->min_free_blocks) {
			/* save the new minimum */
			obj->attrs->min_free_blocks =
						obj->attrs->num_free_blocks;
		}
#if 0
		/* Make sure the attrs are written out to memory */
		if (EXPECT_false(obj->cacheEnabled == true)) {
			Cache_wbInv((Ptr) obj->attrs,
						 sizeof(heapbufmp_attrs),
						 Cache_Type_ALL,
						 true);
		}
#endif
	}
	gatemp_leave(obj->gate, key);

	if (block == NULL)
		pr_err("heapbufmp_alloc returned NULL\n");

	return block;
error:
	pr_err("heapbufmp_alloc failed status: %x\n", retval);
	return NULL;
}
EXPORT_SYMBOL(heapbufmp_alloc);

/*
 * ======== heapbufmp_free  ========
 *  Purpose:
 *  This will free a block of memory
 */
int heapbufmp_free(void *hphandle, void *block, u32 size)
{
	struct heapbufmp_object *handle = NULL;
	s32 retval = 0;
	struct heapbufmp_obj *obj = NULL;
	int *key;

	if (atomic_cmpmask_and_lt(&(heapbufmp_module->ref_count),
				HEAPBUFMP_MAKE_MAGICSTAMP(0),
				HEAPBUFMP_MAKE_MAGICSTAMP(1)) == true) {
		retval = -ENODEV;
		goto error;
	}
	if (WARN_ON(hphandle == NULL)) {
		retval  = -EINVAL;
		goto error;
	}
	if (WARN_ON(block == NULL)) {
		retval  = -EINVAL;
		goto error;
	}

	handle = (struct heapbufmp_object *)(hphandle);
	obj = (struct heapbufmp_obj *)handle->obj;
	if (WARN_ON(obj == NULL)) {
		retval  = -EINVAL;
		goto error;
	}

	/* key = gatemp_enter(obj->gate); */
	retval = listmp_put_tail(obj->free_list, block);
	if (unlikely(retval < 0)) {
		retval = -EFAULT;
		goto error;
	}
	key = gatemp_enter(obj->gate); /*gatemp call moved down */
	if (unlikely(heapbufmp_module->cfg.track_allocs)) {
#if 0
		/* Make sure the attrs are not in cache */
		if (EXPECT_false(obj->cacheEnabled == true)) {
			Cache_inv((Ptr) obj->attrs,
					   sizeof(heapbufmp_attrs),
					   Cache_Type_ALL,
					   true);
		}
#endif

		obj->attrs->num_free_blocks++;
#if 0
		/* Make sure the attrs are written out to memory */
		if (EXPECT_false(obj->cacheEnabled == true)) {
			Cache_wbInv((Ptr) obj->attrs,
						 sizeof(heapbufmp_attrs),
						 Cache_Type_ALL,
						 true);
		}
#endif
	}

	gatemp_leave(obj->gate, key);

	return 0;

error:
	pr_err("heapbufmp_free failed status: %x\n", retval);
	return retval;
}
EXPORT_SYMBOL(heapbufmp_free);

/*
 * ======== heapbufmp_get_stats  ========
 *  Purpose:
 *  This will get memory statistics
 */
void heapbufmp_get_stats(void *hphandle, struct memory_stats *stats)
{
	struct heapbufmp_object *object = NULL;
	struct heapbufmp_obj *obj = NULL;
	int *key;
	s32 retval = 0;
	u32 block_size;

	if (atomic_cmpmask_and_lt(&(heapbufmp_module->ref_count),
				HEAPBUFMP_MAKE_MAGICSTAMP(0),
				HEAPBUFMP_MAKE_MAGICSTAMP(1)) == true) {
		retval = -ENODEV;
		goto error;
	}
	if (WARN_ON(hphandle == NULL)) {
		retval  = -EINVAL;
		goto error;
	}
	if (WARN_ON(stats == NULL)) {
		retval  = -EINVAL;
		goto error;
	}

	object = (struct heapbufmp_object *)(hphandle);
	obj = (struct heapbufmp_obj *)object->obj;
	if (WARN_ON(obj == NULL)) {
		retval  = -EINVAL;
		goto error;
	}

	block_size = obj->attrs->block_size;

	if (unlikely(heapbufmp_module->cfg.track_allocs)) {

		key = gatemp_enter(obj->gate);
#if 0
		/* Make sure the attrs are not in cache */
		if (EXPECT_false(obj->cacheEnabled == true)) {
			Cache_inv((Ptr) obj->attrs,
				   sizeof(heapbufmp_attrs),
				   Cache_Type_ALL,
				   true);
		}
#endif

		stats->total_free_size = block_size * obj->attrs->
							num_free_blocks;
		stats->largest_free_size = (obj->attrs->num_free_blocks > 0) ?
					block_size : 0; /* determined later */

		gatemp_leave(obj->gate, key);
	} else {
		/* Tracking disabled */
		stats->total_free_size = 0;
		stats->largest_free_size = 0;
	}
	return;

error:
	if (retval < 0)
		pr_err("heapbufmp_get_stats status: [0x%x]\n", retval);
}
EXPORT_SYMBOL(heapbufmp_get_stats);

/*
 * ======== heapbufmp_isblocking  ========
 *  Purpose:
 *  Indicate whether the heap may block during an alloc or free call
 */
bool heapbufmp_isblocking(void *handle)
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
	pr_err("heapbufmp_isblocking status: %x\n", retval);
	return isblocking;
}
EXPORT_SYMBOL(heapbufmp_isblocking);

/*
 * ======== heapbufmp_get_extended_stats  ========
 *  Purpose:
 *  This will get extended statistics
 */
void heapbufmp_get_extended_stats(void *hphandle,
				struct heapbufmp_extended_stats *stats)
{
	s32 retval = 0;
	struct heapbufmp_object *object = NULL;
	struct heapbufmp_obj *obj = NULL;
	int *key;

	if (atomic_cmpmask_and_lt(&(heapbufmp_module->ref_count),
				HEAPBUFMP_MAKE_MAGICSTAMP(0),
				HEAPBUFMP_MAKE_MAGICSTAMP(1)) == true) {
		retval = -ENODEV;
		goto error;
	}
	if (WARN_ON(heapbufmp_module->nameserver == NULL)) {
		retval  = -EINVAL;
		goto error;
	}
	if (WARN_ON(hphandle == NULL)) {
		retval  = -EINVAL;
		goto error;
	}

	object = (struct heapbufmp_object *)(hphandle);
	obj = (struct heapbufmp_obj *)object->obj;
	if (WARN_ON(obj == NULL)) {
		retval  = -EINVAL;
		goto error;
	}
#if 0
	/* Make sure the attrs are not in cache */
	if (EXPECT_false(obj->cacheEnabled == true)) {
		Cache_inv((Ptr) obj->attrs,
				   sizeof(heapbufmp_attrs),
				   Cache_Type_ALL,
				   true);
	}
#endif
	/*
	 *  The maximum number of allocations for this HeapBufMP(for any given
	 *  instance of time during its liftime) is computed as follows:
	 *
	 *  max_allocated_blocks = obj->num_blocks - obj->min_free_blocks
	 *
	 *  Note that max_allocated_blocks is *not* the maximum allocation
	 *  count, but rather the maximum allocations seen at any snapshot of
	 *  time in the HeapBufMP instance.
	 */
	key = gatemp_enter(obj->gate);
	/* if nothing has been alloc'ed yet, return 0 */
	if ((s32)(obj->attrs->min_free_blocks) == -1)
		stats->max_allocated_blocks = 0;
	else
		stats->max_allocated_blocks =   obj->attrs->num_blocks
						- obj->attrs->min_free_blocks;

	/*
	 * current # of alloc'ed blocks is computed
	 * using curr # of free blocks
	 */
	stats->num_allocated_blocks = obj->attrs->num_blocks
						- obj->attrs->num_free_blocks;

	gatemp_leave(obj->gate, key);

	return;

error:
	pr_err("heapbufmp_get_extended_stats status: %x\n", retval);
}
EXPORT_SYMBOL(heapbufmp_get_extended_stats);

/*
 * ======== heapbufmp_shared_mem_req ========
 *  Purpose:
 *  This will get amount of shared memory required for
 *  creation of each instance
 */
int heapbufmp_shared_mem_req(const struct heapbufmp_params *params)
{
	int mem_req = 0;
	struct listmp_params listmp_params;
	u32 buf_align = 0;
	u32 block_size = 0;

	s32 status = 0;
	u32 region_id;
	u32 min_align;

	if (WARN_ON(params == NULL)) {
		status = -EINVAL;
		goto error;
	}
	if (WARN_ON(params->block_size == 0)) {
		status = -EINVAL;
		goto error;
	}
	if (WARN_ON(params->num_blocks == 0)) {
		status = -EINVAL;
		goto error;
	}

	if (params->shared_addr == NULL)
		region_id = params->region_id;
	else
		region_id = sharedregion_get_id(params->shared_addr);

	if (region_id == SHAREDREGION_INVALIDREGIONID)  {
		status = -EFAULT;
		goto error;
	}

	buf_align = params->align;

	min_align = 4; /* memory_get_default_type_align() */
	if (sharedregion_get_cache_line_size(region_id) > min_align)
		min_align = sharedregion_get_cache_line_size(region_id);

	if (buf_align < min_align)
		buf_align = min_align;

	/* Determine the actual block size */
	block_size = ROUND_UP(params->block_size, buf_align);

	/* Add size of HeapBufMP Attrs */
	mem_req = ROUND_UP(sizeof(struct heapbufmp_attrs), min_align);

	/*
	 *  Add size of ListMP Attrs.  No need to init params since it's
	 *  not used to create.
	 */
	listmp_params_init(&listmp_params);
	listmp_params.region_id = region_id;
	mem_req += listmp_shared_mem_req(&listmp_params);

	/* Round by the buffer alignment */
	mem_req = ROUND_UP(mem_req, buf_align);

	/*
	 *  Add the buffer size. No need to subsequently round because the
	 *  product should be a multiple of cacheLineSize if cache alignment
	 *  is enabled
	 */
	mem_req += (block_size * params->num_blocks);

	return mem_req;
error:
	pr_err("heapbufmp_shared_mem_req status: %x\n", status);
	return mem_req;
}
EXPORT_SYMBOL(heapbufmp_shared_mem_req);


/*
 * ======== heapbufmp_open_by_addr ========
 *  Purpose:
 *  Open existing heapbufmp based on address
 */
int
heapbufmp_open_by_addr(void *shared_addr, void **handle_ptr)
{
	s32 retval = 0;
	bool done_flag = false;
	struct heapbufmp_attrs *attrs = NULL;
	u16 id = 0;
	struct heapbufmp_params params;
	struct heapbufmp_obj *obj = NULL;

	if (unlikely(atomic_cmpmask_and_lt(&(heapbufmp_module->ref_count),
					   HEAPBUFMP_MAKE_MAGICSTAMP(0),
					   HEAPBUFMP_MAKE_MAGICSTAMP(1))
					  == true)) {
		retval = -ENODEV;
		goto error;
	}
	if (unlikely(handle_ptr == NULL)) {
		retval = -EINVAL;
		goto error;
	}

	/* First check in the local list */
	list_for_each_entry(obj, &heapbufmp_module->obj_list, list_elem) {
		if (obj->params.shared_addr == shared_addr) {
			retval = mutex_lock_interruptible(heapbufmp_module->
								local_lock);
			if (retval < 0)
				goto error;

			if (obj->owner.proc_id == multiproc_self())
				obj->owner.open_count++;

			mutex_unlock(heapbufmp_module->local_lock);
			*handle_ptr = obj->top;
			done_flag = true;
			break;
		}
	}

	/* If not already existing locally, create object locally for open. */
	if (unlikely(done_flag == false)) {
		heapbufmp_params_init(&params);
		params.shared_addr = shared_addr;
		attrs = (struct heapbufmp_attrs *) shared_addr;
		id = sharedregion_get_id(shared_addr);
#if 0
		if (unlikely(sharedregion_is_cache_enabled(id))) {
			Cache_inv(attrs,
					   sizeof(heapbufmp_attrs),
					   Cache_Type_ALL,
					   true);
		}
#endif
		if (unlikely(attrs->status != HEAPBUFMP_CREATED)) {
			*handle_ptr = NULL;
			retval = -ENOENT;
			goto error;
		}

		retval = _heapbufmp_create(handle_ptr, &params, false);

		if (unlikely(retval < 0))
			goto error;
	}
	return 0;

error:
	pr_err("heapbufmp_open_by_addr status: %x\n", retval);

	return retval;
}


/* =========================================================================
 * Internal functions
 * =========================================================================
 */
/*
 *                          Shared memory Layout:
 *
 *          sharedAddr   -> ---------------------------
 *                         |  heapbufmp_attrs        |
 *                         | (min_align PADDING)     |
 *                         |-------------------------|
 *                         |  ListMP shared instance |
 *                         | (bufAlign PADDING)     |
 *                         |-------------------------|
 *                         |  HeapBufMP BUFFER       |
 *                         |-------------------------|
 */


/*
 * ======== heapbufmp_post_init ========
 *  Purpose:
 *  Slice and dice the buffer up into the correct size blocks and
 *  add to the freelist.
 */
static int heapbufmp_post_init(struct heapbufmp_object *handle)
{
	s32 retval = 0;
	char *buf = NULL;
	struct heapbufmp_obj *obj = NULL;
	struct heapbufmp_object *region_heap = NULL;
	struct heapbufmp_params params;
	struct listmp_params listmp_params;
	u32 min_align;
	u32 i;

	obj = (struct heapbufmp_obj *)handle->obj;
	if (WARN_ON(obj == NULL)) {
		retval  = -EINVAL;
		goto error;
	}

	min_align = 4; /* memory_get_default_type_align() */
	if (sharedregion_get_cache_line_size(obj->region_id) > min_align)
		min_align = sharedregion_get_cache_line_size(obj->region_id);

	if (obj->attrs == NULL) {
		heapbufmp_params_init(&params);
		params.region_id = obj->region_id;
		params.num_blocks    = obj->num_blocks;
		params.align         = obj->align;
		params.block_size    = obj->block_size;
		obj->alloc_size = heapbufmp_shared_mem_req(&params);

		region_heap = sharedregion_get_heap(obj->region_id);
		if (region_heap == NULL) {
			retval = -EFAULT;
			goto error;
		}

		obj->attrs = sl_heap_alloc(region_heap, obj->alloc_size,
						min_align);
		if (obj->attrs == NULL) {
			retval = -ENOMEM;
			goto error;
		}
	}

	/* Store the GateMP sharedAddr in the HeapBuf Attrs */
	obj->attrs->gatemp_addr = gatemp_get_shared_addr(obj->gate);

	/* Create the free_list */
	listmp_params_init(&listmp_params);
	listmp_params.shared_addr = (void *)ROUND_UP((u32)obj->attrs
					+ sizeof(struct heapbufmp_attrs),
						min_align);
	listmp_params.gatemp_handle = obj->gate;
	obj->free_list = listmp_create(&listmp_params);
	if (obj->free_list == NULL) {
		retval = -EFAULT;
		goto error;
	}

	/* obj->buf will get alignment-adjusted in postInit */
	obj->buf = (void *)((u32)listmp_params.shared_addr
			+ listmp_shared_mem_req(&listmp_params));
	buf = obj->buf = (char *)ROUND_UP((u32)obj->buf, obj->align);

	obj->attrs->num_free_blocks	= obj->num_blocks;
	obj->attrs->min_free_blocks	= (32)-1;
	obj->attrs->block_size		= obj->block_size;
	obj->attrs->align		= obj->align;
	obj->attrs->num_blocks		= obj->num_blocks;
	obj->attrs->exact		= obj->exact ? 1 : 0;

	/* Put a SRPtr in attrs */
	obj->attrs->buf_ptr = sharedregion_get_srptr(obj->buf,
							obj->region_id);
	BUG_ON(obj->attrs->buf_ptr == SHAREDREGION_INVALIDSRPTR);

	/*
	 * Split the buffer into blocks that are length "block_size" and
	 * add into the free_list Queue.
	 */
	for (i = 0; i < obj->num_blocks; i++) {
		/* Add the block to the free_list */
		retval = listmp_put_tail(obj->free_list,
					(struct listmp_elem *)buf);
		if (retval < 0) {
			retval = -EFAULT;
			goto created_free_list_error;
		}

		buf = (char *)((u32)buf + obj->block_size);
	}

	/* Last thing, set the status */
	obj->attrs->status = HEAPBUFMP_CREATED;
#if 0
	if (unlikely(obj->cacheEnabled)) {
		Cache_wbInv((Ptr) obj->attrs,
			     sizeof(heapbufmp_attrs),
			     Cache_Type_ALL,
			     true);
	}
#endif
	return 0;

created_free_list_error:
	listmp_delete(&obj->free_list);

error:
	pr_err("heapmem_post_init status: %x\n", retval);
	return retval;
}
