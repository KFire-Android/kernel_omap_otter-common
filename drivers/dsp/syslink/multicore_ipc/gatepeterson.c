/*
 *  gatepeterson.c
 *
 *  The Gate Peterson Algorithm for mutual exclusion of shared memory.
 *  Current implementation works for 2 processors.
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

#include <syslink/atomic_linux.h>
#include <multiproc.h>
#include <sharedregion.h>
#include <gatemp.h>
#include <igatempsupport.h>
#include <igateprovider.h>
#include <iobject.h>
#include <gatepeterson.h>


/* IPC stubs */

#define GATEPETERSON_BUSY		1
#define GATEPETERSON_FREE		0
#define GATEPETERSON_VERSION		1
#define GATEPETERSON_CREATED		0x08201997 /* Stamp to indicate GP
							was created here */

/* Cache line size */
#define GATEPETERSON_CACHESIZE		128

/* Macro to make a correct module magic number with ref_count */
#define GATEPETERSON_MAKE_MAGICSTAMP(x) ((GATEPETERSON_MODULEID << 12) | (x))

/*
 *  structure for gatepeterson module state
 */
struct gatepeterson_module_object {
	atomic_t ref_count; /* Reference count */
	struct list_head obj_list;
	struct mutex *mod_lock; /* Lock for obj list */
	struct gatepeterson_config cfg;
	struct gatepeterson_config default_cfg;
	struct gatepeterson_params def_inst_params; /* default instance
							paramters */
};

/*
 *  Structure defining attribute parameters for the Gate Peterson module
 */
struct gatepeterson_attrs {
	VOLATILE u16 creator_proc_id;
	VOLATILE  u16 opener_proc_id;
};

/*
 *  Structure defining internal object for the Gate Peterson
 */
struct gatepeterson_object {
	IGATEPROVIDER_SUPEROBJECT; /* For inheritance from IGateProvider */
	IOBJECT_SUPEROBJECT;       /* For inheritance for IObject */
	struct list_head elem;
	VOLATILE struct gatepeterson_attrs *attrs; /* Instance attr */
	VOLATILE u16 *flag[2]; /* Flags for processors */
	VOLATILE u16 *turn; /* Indicates whoes turn it is now? */
	u16 self_id; /* Self identifier */
	u16 other_id; /* Other's identifier */
	u32 nested; /* Counter to track nesting */
	void *local_gate; /* Local lock handle */
	enum igatempsupport_local_protect local_protect; /* Type of local
						protection to be used */
	struct gatepeterson_params params;
	u32 ref_count; /* Local reference count */
	u32 cache_line_size; /* Cache Line Size */
	bool cache_enabled; /* Is cache enabled? */
};


/*
 *  Variable for holding state of the gatepeterson module
 */
static struct gatepeterson_module_object gatepeterson_state = {
	.obj_list	 = LIST_HEAD_INIT(gatepeterson_state.obj_list),
	.default_cfg.default_protection = GATEPETERSON_PROTECT_INTERRUPT,
	.default_cfg.num_instances	= 16,
	.def_inst_params.shared_addr	= NULL,
	.def_inst_params.resource_id	= 0x0,
	.def_inst_params.region_id	= 0x0
};

static struct gatepeterson_module_object *gatepeterson_module =
						&gatepeterson_state;

/* =============================================================================
 * Internal functions
 * =============================================================================
 */
#if 0
static void *_gatepeterson_create(enum igatempsupport_local_protect
							local_protect,
				const struct gatepeterson_params *params,
				bool create_flag);
#endif

static void gatepeterson_post_init(struct gatepeterson_object *obj);

#if 0
static bool gatepeterson_inc_refcount(const struct gatepeterson_params *params,
					void **handle);
#endif

/* TODO: figure these out */
#define gate_enter_system() (int *)0
#define gate_leave_system(key) {}

/* =============================================================================
 * APIS
 * =============================================================================
 */
/*
 * ======== gatepeterson_get_config ========
 *  Purpose:
 *  This will get the default configuration parameters for gatepeterson
 *  module
 */
void gatepeterson_get_config(struct gatepeterson_config *config)
{
	if (WARN_ON(config == NULL))
		goto exit;

	if (atomic_cmpmask_and_lt(&(gatepeterson_module->ref_count),
				GATEPETERSON_MAKE_MAGICSTAMP(0),
				GATEPETERSON_MAKE_MAGICSTAMP(1)) == true)
		memcpy(config, &gatepeterson_module->default_cfg,
					sizeof(struct gatepeterson_config));
	else
		memcpy(config, &gatepeterson_module->cfg,
					sizeof(struct gatepeterson_config));

exit:
	return;
}
EXPORT_SYMBOL(gatepeterson_get_config);

/*
 * ======== gatepeterson_setup ========
 *  Purpose:
 *  This will setup the gatepeterson module
 */
int gatepeterson_setup(const struct gatepeterson_config *config)
{
	struct gatepeterson_config tmp_cfg;
	int *key = NULL;
	s32 retval = 0;

	key = gate_enter_system();

	/* This sets the ref_count variable  not initialized, upper 16 bits is
	* written with module _id to ensure correctness of ref_count variable
	*/
	atomic_cmpmask_and_set(&gatepeterson_module->ref_count,
					GATEPETERSON_MAKE_MAGICSTAMP(0),
					GATEPETERSON_MAKE_MAGICSTAMP(0));

	if (atomic_inc_return(&gatepeterson_module->ref_count)
					!= GATEPETERSON_MAKE_MAGICSTAMP(1)) {
		gate_leave_system(key);
		return 1;
	}

	if (config == NULL) {
		gatepeterson_get_config(&tmp_cfg);
		config = &tmp_cfg;
	}
	gate_leave_system(key);

	memcpy(&gatepeterson_module->cfg, config,
					sizeof(struct gatepeterson_config));

	gatepeterson_module->mod_lock = kmalloc(sizeof(struct mutex),
								GFP_KERNEL);
	if (gatepeterson_module->mod_lock == NULL) {
		retval = -ENOMEM;
		goto exit;
	}
	mutex_init(gatepeterson_module->mod_lock);

	/* Initialize object list */
	INIT_LIST_HEAD(&gatepeterson_module->obj_list);

	return 0;

exit:
	atomic_set(&gatepeterson_module->ref_count,
				GATEPETERSON_MAKE_MAGICSTAMP(0));

	pr_err("gatepeterson_setup failed status: %x\n", retval);
	return retval;
}
EXPORT_SYMBOL(gatepeterson_setup);

/*
 * ======== gatepeterson_destroy ========
 *  Purpose:
 *  This will destroy the gatepeterson module
 */
int gatepeterson_destroy(void)
{
	struct gatepeterson_object *obj = NULL, *n;
	struct mutex *lock = NULL;
	s32 retval = 0;
	int *key = NULL;

	key = gate_enter_system();

	if (atomic_cmpmask_and_lt(&(gatepeterson_module->ref_count),
				GATEPETERSON_MAKE_MAGICSTAMP(0),
				GATEPETERSON_MAKE_MAGICSTAMP(1)) == true) {
		retval = -ENODEV;
		goto exit;
	}

	if (!(atomic_dec_return(&gatepeterson_module->ref_count)
			== GATEPETERSON_MAKE_MAGICSTAMP(0))) {
		gate_leave_system(key);
		retval = 1;
		goto exit;
	}
	atomic_set(&gatepeterson_module->ref_count,
				GATEPETERSON_MAKE_MAGICSTAMP(1));
	/* Check if any gatepeterson instances have not been
	*  ideleted/closed so far, if there any, delete or close them
	*/
	list_for_each_entry_safe(obj, n, &gatepeterson_module->obj_list, elem) {
		gatepeterson_delete((void **)&obj);

		if (list_empty(&gatepeterson_module->obj_list))
			break;
	}

	/* Again reset ref_count. */
	atomic_set(&gatepeterson_module->ref_count,
				GATEPETERSON_MAKE_MAGICSTAMP(0));
	gate_leave_system(key);

	retval = mutex_lock_interruptible(gatepeterson_module->mod_lock);
	if (retval != 0)
		goto exit;

	lock = gatepeterson_module->mod_lock;
	gatepeterson_module->mod_lock = NULL;
	memset(&gatepeterson_module->cfg, 0,
			sizeof(struct gatepeterson_config));
	mutex_unlock(lock);
	kfree(lock);
	return 0;

exit:
	if (retval < 0)
		pr_err("gatepeterson_destroy failed status:%x\n", retval);
	return retval;
}
EXPORT_SYMBOL(gatepeterson_destroy);

/*
 * ======== gatepeterson_get_num_instances ========
 *  Purpose:
 *  Function to return the number of instances configured in the module.
 */
u32 gatepeterson_get_num_instances(void)
{
	return gatepeterson_module->default_cfg.num_instances;
}
EXPORT_SYMBOL(gatepeterson_get_num_instances);

/*
 * ======== gatepeterson_get_num_reserved ========
 *  Purpose:
 *  Function to return the number of instances not under GateMP's control.
 */
u32 gatepeterson_get_num_reserved(void)
{
	return 0;
}
EXPORT_SYMBOL(gatepeterson_get_num_reserved);

/*
 * ======== gatepeterson_locks_init ========
 *  Purpose:
 *  Function to initialize the locks.
 */
inline void gatepeterson_locks_init(void)
{
	/* Do nothing*/
}

/*
 * ======== gatepeterson_params_init ========
 *  Purpose:
 *  This will  Initialize this config-params structure with
 *  supplier-specified defaults before instance creation
 */
void gatepeterson_params_init(struct gatepeterson_params *params)
{
	int *key = NULL;

	key = gate_enter_system();

	if (WARN_ON(atomic_cmpmask_and_lt(&(gatepeterson_module->ref_count),
				GATEPETERSON_MAKE_MAGICSTAMP(0),
				GATEPETERSON_MAKE_MAGICSTAMP(1)) == true))
		goto exit;

	if (WARN_ON(params == NULL))
		goto exit;

	memcpy(params, &(gatepeterson_module->def_inst_params),
				sizeof(struct gatepeterson_params));

exit:
	gate_leave_system(key);
	return;
}
EXPORT_SYMBOL(gatepeterson_params_init);


static int gatepeterson_instance_init(struct gatepeterson_object *obj,
		enum igatempsupport_local_protect local_protect,
		const struct gatepeterson_params *params)
{
	s32 retval = 0;

	IGATEPROVIDER_OBJECTINITIALIZER(obj, gatepeterson);

	if (params->shared_addr == NULL) {
		retval = 1;
		goto exit;
	}

	/* Create the local gate */
	obj->local_gate = gatemp_create_local(
				(enum gatemp_local_protect)local_protect);
	obj->cache_enabled  = sharedregion_is_cache_enabled(params->region_id);
	obj->cache_line_size =
		sharedregion_get_cache_line_size(params->region_id);

	/* Settings for both the creator and opener */
	if (obj->cache_line_size > sizeof(struct gatepeterson_attrs)) {
		obj->attrs   = params->shared_addr;
		obj->flag[0] = (u16 *)((u32)(obj->attrs) +
							obj->cache_line_size);
		obj->flag[1] = (u16 *)((u32)(obj->flag[0]) +
							obj->cache_line_size);
		obj->turn = (u16 *)((u32)(obj->flag[1]) +
							obj->cache_line_size);
	} else {
		obj->attrs   = params->shared_addr;
		obj->flag[0] = (u16 *)((u32)(obj->attrs) +
					sizeof(struct gatepeterson_attrs));
		obj->flag[1] = (u16 *)((u32)(obj->flag[0]) + sizeof(u16));
		obj->turn	= (u16 *)((u32)(obj->flag[1]) + sizeof(u16));
	}
	obj->nested  = 0;

	if (!params->open_flag) {
		/* Creating. */
		obj->self_id = 0;
		obj->other_id = 1;
		gatepeterson_post_init(obj);
	} else {
#if 0
		Cache_inv((Ptr)obj->attrs, sizeof(struct gatepeterson_attrs),
				Cache_Type_ALL, TRUE);
#endif
		if (obj->attrs->creator_proc_id == multiproc_self()) {
			/* Opening locally */
			obj->self_id	= 0;
			obj->other_id	= 1;
		} else {
			/* Trying to open a gate remotely */
			obj->self_id	= 1;
			obj->other_id	= 0;
			if (obj->attrs->opener_proc_id == MULTIPROC_INVALIDID) {
				/* Opening remotely for the first time */
				obj->attrs->opener_proc_id = multiproc_self();
			} else if (obj->attrs->opener_proc_id !=
							multiproc_self()) {
				retval = -EACCES;
				goto exit;
			}
#if 0
			if (obj->cache_enabled) {
				Cache_wbInv((Ptr)obj->attrs,
					sizeof(struct gatepeterson_attrs),
					Cache_Type_ALL, TRUE);
			}
#endif
		}
	}

exit:
	if (retval < 0)
		pr_err("gatemp_instance_init failed! status = 0x%x", retval);
	return retval;
}

static void gatepeterson_instance_finalize(struct gatepeterson_object *obj,
						int status)
{
	switch (status) {
	/* No break here. Fall through to the next. */
	case 0:
		{
			/* Modify shared memory */
			obj->attrs->opener_proc_id = MULTIPROC_INVALIDID;
#if 0
			Cache_wbInv((Ptr)obj->attrs, sizeof(GatePeterson_Attrs),
				Cache_Type_ALL, TRUE);
#endif
		}
		/* No break here. Fall through to the next. */

	case 1:
		{
			/* Nothing to be done. */
		}
	}
	return;
}



#if 0
/*
 * ======== gatepeterson_create ========
 *  Purpose:
 *  This will creates a new instance of gatepeterson module
 */
void *gatepeterson_create(enum igatempsupport_local_protect local_protect,
				const struct gatepeterson_params *params)
{
	void *handle = NULL;
	s32 retval = 0;

	BUG_ON(params == NULL);
	if (atomic_cmpmask_and_lt(&(gatepeterson_module->ref_count),
					GATEPETERSON_MAKE_MAGICSTAMP(0),
				GATEPETERSON_MAKE_MAGICSTAMP(1)) == true) {
		retval = -ENODEV;
		goto exit;
	}

	if (WARN_ON(params->shared_addr == NULL)) {
		retval = -EINVAL;
		goto exit;
	}

	handle = _gatepeterson_create(local_protect, params, true);
	return handle;

exit:
	return NULL;
}
EXPORT_SYMBOL(gatepeterson_create);

/*
 * ======== gatepeterson_delete ========
 *  Purpose:
 *  This will deletes an instance of gatepeterson module
 */
int gatepeterson_delete(void **gphandle)

{
	struct gatepeterson_object *obj = NULL;
	s32 retval;

	BUG_ON(gphandle == NULL);
	BUG_ON(*gphandle == NULL);
	if (atomic_cmpmask_and_lt(&(gatepeterson_module->ref_count),
				GATEPETERSON_MAKE_MAGICSTAMP(0),
				GATEPETERSON_MAKE_MAGICSTAMP(1)) == true) {
		retval = -ENODEV;
		goto exit;
	}

	obj = (struct gatepeterson_object *)(*gphandle);
	if (unlikely(obj == NULL)) {
		retval = -EINVAL;
		goto exit;
	}

	if (unlikely(obj->attrs == NULL)) {
		retval = -EINVAL;
		goto exit;
	}

	/* Check if we have created the GP or not */
	if (WARN_ON(unlikely(obj->attrs->creator_proc_id !=
						multiproc_get_id(NULL)))) {
		retval = -EACCES;
		goto exit;
	}

	if (obj->ref_count != 0) {
		retval = -EBUSY;
		goto exit;
	}

	retval = mutex_lock_interruptible(gatepeterson_module->mod_lock);
	if (retval)
		goto exit;

	list_del(&obj->elem); /* Remove the GP instance from the GP list */
	mutex_unlock(gatepeterson_module->mod_lock);
	/* Modify shared memory */
	obj->attrs->opener_proc_id = MULTIPROC_INVALIDID;
#if 0
	Cache_wbInv((Ptr)obj->attrs, sizeof(struct gatepeterson_attrs),
			Cache_Type_ALL, true);
#endif

	kfree(obj);
	*gphandle = NULL;
	return 0;

exit:
	pr_err("gatepeterson_delete failed status: %x\n", retval);
	return retval;
}
EXPORT_SYMBOL(gatepeterson_delete);

#else

/* Override the IObject interface to define create and delete APIs */
IOBJECT_CREATE1(gatepeterson, enum igatempsupport_local_protect);

#endif

#if 0
/*
 * ======== gatepeterson_open ========
 *  Purpose:
 *  This will opens a created instance of gatepeterson
 *  module by shared addr.
 */
int gatepeterson_open_by_addr(enum igatempsupport_local_protect local_protect,
				void *shared_addr, void **handle_ptr)
{
	void *temp = NULL;
	s32 retval = 0;
	struct gatepeterson_params params;

	BUG_ON(shared_addr == NULL);
	BUG_ON(handle_ptr == NULL);
	if (atomic_cmpmask_and_lt(&(gatepeterson_module->ref_count),
				GATEPETERSON_MAKE_MAGICSTAMP(0),
				GATEPETERSON_MAKE_MAGICSTAMP(1)) == true) {
		retval = -ENODEV;
		goto exit;
	}

	if (shared_addr == NULL) {
		retval = -EINVAL;
		goto exit;
	}

	if (handle_ptr == NULL) {
		retval = -EINVAL;
		goto exit;
	}

	if (gatepeterson_inc_refcount(&params, &temp)) {
		retval = -EBUSY;
		goto exit; /* It's already opened from local processor */
	}

	gatepeterson_params_init(&params);
	params.shared_addr = shared_addr;
	params.region_id = sharedregion_get_id(shared_addr);

	*handle_ptr = _gatepeterson_create(local_protect, &params, false);
	return 0;

exit:
	pr_err("gatepeterson_open failed status: %x\n", retval);
	return retval;
}
EXPORT_SYMBOL(gatepeterson_open_by_addr);

/*
 * ======== gatepeterson_close ========
 *  Purpose:
 *  This will closes previously opened/created instance
 * of gatepeterson module
 */
int gatepeterson_close(void **gphandle)
{
	struct gatepeterson_object *obj = NULL;
	struct gatepeterson_params *params = NULL;
	s32 retval = 0;

	BUG_ON(gphandle == NULL);
	if (atomic_cmpmask_and_lt(&(gatepeterson_module->ref_count),
				GATEPETERSON_MAKE_MAGICSTAMP(0),
				GATEPETERSON_MAKE_MAGICSTAMP(1)) == true) {
		retval = -ENODEV;
		goto exit;
	}

	if (WARN_ON(*gphandle == NULL)) {
		retval = -EINVAL;
		goto exit;
	}

	obj = (struct gatepeterson_object *) (*gphandle);
	if (unlikely(obj == NULL)) {
		retval = -EINVAL;
		goto exit;
	}

	key = gatemp_enter(obj->local_gate);

	if (obj->ref_count > 1) {
		obj->ref_count--;
		gatemp_leave(obj->local_gate, key);
		goto exit;
	}

	retval = mutex_lock_interruptible(gatepeterson_module->mod_lock);
	if (retval)
		goto error_handle;

	list_del(&obj->elem);
	mutex_unlock(gatepeterson_module->mod_lock);
	params = &obj->params;

	gatemp_leave(obj->local_gate, key);

	gatemp_delete(obj->local_gate);

	kfree(obj);
	*gphandle = NULL;
	return 0;

error_handle:
	gatemp_leave(obj->local_gate, key);

exit:
	pr_err("gatepeterson_close failed status: %x\n", retval);
	return retval;
}
EXPORT_SYMBOL(gatepeterson_close);
#endif

/*
 * ======== gatepeterson_enter ========
 *  Purpose:
 *  This will enters the gatepeterson instance
 */
int *gatepeterson_enter(void *gphandle)
{
	struct gatepeterson_object *obj = NULL;
	s32 retval = 0;
	int *key = NULL;

	if (WARN_ON(atomic_cmpmask_and_lt(&(gatepeterson_module->ref_count),
				GATEPETERSON_MAKE_MAGICSTAMP(0),
				GATEPETERSON_MAKE_MAGICSTAMP(1)) == true)) {
		retval = -ENODEV;
		goto exit;
	}
	if (WARN_ON(gphandle == NULL)) {
		retval = -EINVAL;
		goto exit;
	}

	obj = (struct gatepeterson_object *) gphandle;

	/* Enter local gate */
	if (obj->local_gate != NULL) {
		retval = mutex_lock_interruptible(obj->local_gate);
		if (retval)
			goto exit;
	}

	/* If the gate object has already been entered, return the key */
	obj->nested++;
	if (obj->nested > 1)
		return key;

	/* indicate, needs to use the resource. */
	*((u32 *)obj->flag[obj->self_id]) = GATEPETERSON_BUSY ;
#if 0
	if (obj->cacheEnabled)
		Cache_wbInv((Ptr)obj->flag[obj->selfId],
			obj->cacheLineSize, Cache_Type_ALL, true);
#endif
	/* Give away the turn. */
	*((u32 *)(obj->turn)) = obj->other_id;
#if 0
	if (obj->cacheEnabled) {
		Cache_wbInv((Ptr)obj->turn, obj->cacheLineSize,
						Cache_Type_ALL, true);
		Cache_inv((Ptr)obj->flag[obj->otherId], obj->cacheLineSize,
						Cache_Type_ALL, true);
	}
#endif

	/* Wait while other processor is using the resource and has
	 *  the turn
	 */
	while ((*((VOLATILE u32 *) obj->flag[obj->other_id])
		== GATEPETERSON_BUSY) &&
		(*((VOLATILE u32  *)obj->turn) == obj->other_id)) {
		/* Empty body loop */
		/* Except for cache stuff */
#if 0
		if (obj->cacheEnabled) {
			Cache_inv((Ptr)obj->flag[obj->otherId], obj->
					cacheLineSize, Cache_Type_ALL, true);
			Cache_inv((Ptr)obj->turn, obj->
					cacheLineSize, Cache_Type_ALL, true);
		}
		udelay(10);
#endif
	}

	return key;

exit:
	return NULL;
}
EXPORT_SYMBOL(gatepeterson_enter);

/*
 * ======== gatepeterson_leave ========
 *  Purpose:
 *  This will leaves the gatepeterson instance
 */
void gatepeterson_leave(void *gphandle, int *key)
{
	struct gatepeterson_object *obj	= NULL;

	if (WARN_ON(atomic_cmpmask_and_lt(&(gatepeterson_module->ref_count),
				GATEPETERSON_MAKE_MAGICSTAMP(0),
				GATEPETERSON_MAKE_MAGICSTAMP(1)) == true))
		goto exit;

	BUG_ON(gphandle == NULL);

	obj = (struct gatepeterson_object *)gphandle;

	/* Release the resource and leave system gate. */
	obj->nested--;
	if (obj->nested == 0) {
		*((VOLATILE u32 *)obj->flag[obj->self_id]) = GATEPETERSON_FREE;
#if 0
		if (obj->cacheEnabled)
			Cache_wbInv((Ptr)obj->flag[obj->selfId],
			obj->cacheLineSize, Cache_Type_ALL, true);
#endif
	}
	/* Leave local gate */
	mutex_unlock(obj->local_gate);

exit:
	return;
}
EXPORT_SYMBOL(gatepeterson_leave);

/*
 * ======== gatepeterson_shared_mem_req ========
 *  Purpose:
 *  This will give the amount of shared memory required
 *  for creation of each instance
 */
u32 gatepeterson_shared_mem_req(const struct gatepeterson_params *params)
{
	u32 mem_req = 0;

	if (sharedregion_get_cache_line_size(params->region_id) >=
			sizeof(struct gatepeterson_attrs))
		/* 4 Because shared of shared memory usage */
		mem_req = 4 * sharedregion_get_cache_line_size(params->
								region_id);
	else
		mem_req = sizeof(struct gatepeterson_attrs) +
						sizeof(u16) * 3;

	return mem_req;
}
EXPORT_SYMBOL(gatepeterson_shared_mem_req);

/*
 *************************************************************************
 *                       Internal functions
 *************************************************************************
 */
#if 0
/*
 * ======== gatepeterson_create ========
 *  Purpose:
 *  Creates a new instance of gatepeterson module.
 *  This is an internal function because both
 *  gatepeterson_create and gatepeterson_open
 *  call use the same functionality.
 */
static void *_gatepeterson_create(enum igatempsupport_local_protect
		local_protect, const struct gatepeterson_params *params,
							bool create_flag)
{
	int status = 0;
	struct gatepeterson_object *handle = NULL;
	struct gatepeterson_obj *obj = NULL;

	handle = kmalloc(sizeof(struct gatepeterson_object), GFP_KERNEL);
	if (handle == NULL) {
		status = -ENOMEM;
		goto exit;
	}

	obj = kmalloc(sizeof(struct gatepeterson_obj), GFP_KERNEL);
	if (obj == NULL) {
		status = -ENOMEM;
		goto obj_alloc_fail;
	}

	if (local_protect >= GATEPETERSON_PROTECT_END_VALUE) {
		status = -EINVAL;
		goto exit_with_cleanup;
	}

	handle->obj = obj;
	handle->enter = &gatepeterson_enter;
	handle->leave = &gatepeterson_leave;

	/* Create the local gate */
	obj->local_gate = gatemp_create_local(local_protect);
	obj->cache_enabled  = sharedregion_is_cache_enabled(params->region_id);
	obj->cache_line_size = sharedregion_get_cache_line_size(params->
								region_id);

	/* Settings for both the creator and opener */
	if (obj->cache_line_size > sizeof(struct gatepeterson_attrs)) {
		obj->attrs = params->shared_addr;
		obj->flag[0] = (u16 *)((u32)(obj->attrs) + obj->
							cache_line_size);
		obj->flag[1] = (u16 *)((u32)(obj->flag[0]) + obj->
							cache_line_size);
		obj->turn = (u16 *)((u32)(obj->flag[1]) + obj->
							cache_line_size);
	} else {
		obj->attrs   = params->shared_addr;
		obj->flag[0] = (u16 *)((u32)(obj->attrs) +
					sizeof(struct gatepeterson_attrs));
		obj->flag[1] = (u16 *)((u32)(obj->flag[0]) + sizeof(u16));
		obj->turn	= (u16 *)((u32)(obj->flag[1]) + sizeof(u16));
	}
	obj->nested  = 0;

	if (params->open_flag == false) {
		/* Creating. */
		obj->self_id = 0;
		obj->other_id = 1;
		obj->ref_count = 0;
		gatepeterson_post_init(obj);
	} else {
#if 0
		Cache_inv((Ptr)obj->attrs, sizeof(struct gatepeterson_attrs),
				Cache_Type_ALL, true);
#endif
		obj->ref_count = 1;
		if (obj->attrs->creator_proc_id == multiproc_self()) {
			/* Opening locally */
			obj->self_id		 = 0;
			obj->other_id		= 1;
		} else {
			/* Trying to open a gate remotely */
			obj->self_id		 = 1;
			obj->other_id		= 0;
			if (obj->attrs->opener_proc_id == MULTIPROC_INVALIDID)
				/* Opening remotely for the first time */
				obj->attrs->opener_proc_id = multiproc_self();
			else if (obj->attrs->opener_proc_id !=
						multiproc_self()) {
				status = -EFAULT;
				goto exit_with_cleanup;
			}
#if 0
		if (status >= 0) {
			if (obj->cache_enabled) {
				Cache_wbInv((Ptr)obj->attrs,
					sizeof(struct gatepeterson_attrs),
					Cache_Type_ALL, true);
			}
		}
#endif
		}
	}

	status = mutex_lock_interruptible(gatepeterson_module->mod_lock);
	if (status)
		goto mod_lock_fail;

	list_add_tail(&obj->elem, &gatepeterson_module->obj_list);
	mutex_unlock(gatepeterson_module->mod_lock);

	return handle;
mod_lock_fail:
exit_with_cleanup:
	gatemp_delete(&obj->local_gate);
	kfree(obj);

obj_alloc_fail:
	kfree(handle);
	handle = NULL;

exit:
	if (create_flag == true)
		pr_err("_gatepeterson_create (create) failed "
			"status: %x\n", status);
	else
		pr_err("_gatepeterson_create (open) failed "
			"status: %x\n", status);

	return NULL;
}

#endif

/*
 * ======== gatepeterson_post_init ========
 *  Purpose:
 *  Function to be called during
 *  1. module startup to complete the initialization of all static instances
 *  2. instance_init to complete the initialization of a dynamic instance
 *
 *  Main purpose is to set up shared memory
 */
static void gatepeterson_post_init(struct gatepeterson_object *obj)
{
	/* Set up shared memory */
	*(obj->turn)       = 0;
	*(obj->flag[0])    = 0;
	*(obj->flag[1])    = 0;
	obj->attrs->creator_proc_id = multiproc_self();
	obj->attrs->opener_proc_id  = MULTIPROC_INVALIDID;
#if 0
	/*
	* Write everything back to memory. This assumes that obj->attrs is
	* equal to the shared memory base address
	*/
	if (obj->cacheEnabled) {
		Cache_wbInv((Ptr)obj->attrs, sizeof(struct gatepeterson_attrs),
			Cache_Type_ALL, false);
		Cache_wbInv((Ptr)(obj->flag[0]), obj->cacheLineSize * 3,
			Cache_Type_ALL, true);
	}
#endif
}

#if 0
/*
 * ======== gatepeterson_inc_refcount ========
 *  Purpose:
 *  This will increment the reference count while opening
 *  a GP instance if it is already opened from local processor
 */
static bool gatepeterson_inc_refcount(const struct gatepeterson_params *params,
					void **handle)
{
	struct gatepeterson_object *obj = NULL;
	s32 retval  = 0;
	bool done = false;

	list_for_each_entry(obj, &gatepeterson_module->obj_list, elem) {
		if (params->shared_addr != NULL) {
			if (obj->params.shared_addr == params->shared_addr) {
				retval = mutex_lock_interruptible(
						gatepeterson_module->mod_lock);
				if (retval)
					break;

				obj->ref_count++;
				*handle = obj;
				mutex_unlock(gatepeterson_module->mod_lock);
				done = true;
				break;
			}
		}
	}

	return done;
}
#endif
