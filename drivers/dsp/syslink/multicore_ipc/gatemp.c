/*
 *  gatemp.c
 *
 *  Gate wrapper implementation
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
#include <linux/list.h>
#include <linux/slab.h>
#include <linux/semaphore.h>

/* Syslink utilities headers */
#include <syslink/atomic_linux.h>

/* Syslink module headers */
#include <multiproc.h>
#include <igateprovider.h>
#include <igatempsupport.h>
#include <iobject.h>
#include <gate.h>
/*#include <memory.h>
#include <bitops.h>
#include <ti/syslink/utils/Cache.h>
*/
#include <nameserver.h>
#include <sharedregion.h>

/* Module level headers */
#include <gatemp.h>
#include <gatemp.h>
#include <gatempdefs.h>


/* -----------------------------------------------------------------------------
 * Macros
 * -----------------------------------------------------------------------------
 */
/* VERSION */
#define GATEMP_VERSION			(1)

/* CREATED */
#define GATEMP_CREATED			(0x11202009)

/* PROXYORDER_SYSTEM */
#define GATEMP_PROXYORDER_SYSTEM	(0)

/* PROXYORDER_CUSTOM1 */
#define GATEMP_PROXYORDER_CUSTOM1	(1)

/* PROXYORDER_CUSTOM2 */
#define GATEMP_PROXYORDER_CUSTOM2	(2)

/* PROXYORDER_NUM */
#define GATEMP_PROXYORDER_NUM		(3)

/* Macro to make a correct module magic number with refCount */
#define GATEMP_MAKE_MAGICSTAMP(x)	\
				((GATEMP_MODULEID << 12u) | (x))

/* Helper macros */
#define GETREMOTE(mask)		((enum gatemp_remote_protect)(mask >> 8))
#define GETLOCAL(mask)		((enum gatemp_local_protect)(mask & 0xFF))
#define SETMASK(remote_protect, local_protect) \
				((u32)(remote_protect << 8 | local_protect))


/* Name of the reserved NameServer used for GateMP. */
#define GATEMP_NAMESERVER  "GateMP"

#define ROUND_UP(a, b)	(((a) + ((b) - 1)) & (~((b) - 1)))

#define Gate_enterSystem()	(int *)0

#define Gate_leaveSystem(key)	(void)0

/* -----------------------------------------------------------------------------
 * Structs & Enums
 * -----------------------------------------------------------------------------
 */
/* Attrs */
struct gatemp_attrs {
	u16 mask;
	u16 creator_proc_id;
	u32 arg;
	u32 status;
};

/* Structure defining state of GateMP Module */
struct gatemp_module_state {
	void *name_server;
	int num_remote_system;
	int num_remote_custom1;
	int num_remote_custom2;
	int num_remote_system_reserved;
	int num_remote_custom1_reserved;
	int num_remote_custom2_reserved;
	u8 *remote_system_in_use_alloc;
	u8 *remote_custom1_in_use_alloc;
	u8 *remote_custom2_in_use_alloc;
	void **remote_system_gates_alloc;
	void **remote_custom1_gates_alloc;
	void **remote_custom2_gates_alloc;
	u8 *remote_system_in_use;
	u8 *remote_custom1_in_use;
	u8 *remote_custom2_in_use;
	void **remote_system_gates;
	void **remote_custom1_gates;
	void **remote_custom2_gates;
	struct igateprovider_object *gate_hwi;
	struct mutex *gate_mutex;
	struct igateprovider_object *gate_null;
	struct gatemp_object *default_gate;
	int proxy_map[GATEMP_PROXYORDER_NUM];
	atomic_t ref_count;
	struct gatemp_config cfg;
	/* Current config values */
	struct gatemp_config default_cfg;
	/* default config values */
	struct gatemp_params def_inst_params;
	/* default instance paramters */
	bool is_owner;
	/* Indicates if this processor is the owner */
	atomic_t attach_ref_count;
	/* Attach/detach reference count */
};

/* Structure defining instance of GateMP Module */
struct gatemp_object {
	IGATEPROVIDER_SUPEROBJECT; /* For inheritance from IGateProvider */
	IOBJECT_SUPEROBJECT; /* For inheritance for IObject */
	enum gatemp_remote_protect remote_protect;
	enum gatemp_local_protect local_protect;
	void *ns_key;
	int num_opens;
	u16 creator_proc_id;
	bool cache_enabled;
	struct gatemp_attrs *attrs;
	u16 region_id;
	uint alloc_size;
	void *proxy_attrs;
	u32 resource_id;
	void *gate_handle;
	enum ipc_obj_type obj_type; /* from shared region? */
};

/* Reserved */
struct gatemp_reserved {
	u32  version;
};

/* Localgate */
struct gatemp_local_gate {
	struct igateprovider_object *local_gate;
	int ref_count;
};

/*!
 *  @brief  Structure defining parameters for the GateMP module.
 */
struct _gatemp_params {
	char *name;
	u32 region_id;
	void *shared_addr;
	enum gatemp_local_protect local_protect;
	enum gatemp_remote_protect remote_protect;
	u32 resource_id;
	bool open_flag;
};

/* -----------------------------------------------------------------------------
 * Forward declaration
 * -----------------------------------------------------------------------------
 */
static void gatemp_set_region0_reserved(void *shared_addr);
static void gatemp_clear_region0_reserved(void);
static void gatemp_open_region0_reserved(void *shared_addr);
static void gatemp_close_region0_reserved(void *shared_addr);
static void gatemp_set_default_remote(void *handle);
static uint gatemp_get_free_resource(u8 *in_use, int num, int start_id);
static struct gatemp_object *_gatemp_create(
					const struct _gatemp_params *params);

/* -----------------------------------------------------------------------------
 * Globals
 * -----------------------------------------------------------------------------
 */
static struct gatemp_module_state gatemp_state = {
	.default_cfg.num_resources = 32,
	.default_cfg.max_name_len = 32,
	.default_cfg.default_protection = GATEMP_LOCALPROTECT_INTERRUPT,
	.def_inst_params.shared_addr = NULL,
	.def_inst_params.region_id = 0x0,
	.default_gate  = NULL
};

static struct gatemp_module_state *gatemp_module = &gatemp_state;
static struct gatemp_object *gatemp_first_object;

/* -----------------------------------------------------------------------------
 * APIs
 * -----------------------------------------------------------------------------
 */

void gatemp_get_config(struct gatemp_config *cfg)
{
	s32 retval = 0;

	if (WARN_ON(unlikely(cfg == NULL))) {
		retval = -EINVAL;
		goto exit;
	}

	if (atomic_cmpmask_and_lt(&(gatemp_module->ref_count),
				GATEMP_MAKE_MAGICSTAMP(0),
				GATEMP_MAKE_MAGICSTAMP(1)) == true) {
		/* Setup has not yet been called */
		memcpy((void *)cfg, &gatemp_module->default_cfg,
			sizeof(struct gatemp_config));
	} else {
		memcpy((void *)cfg, &gatemp_module->cfg,
			sizeof(struct gatemp_config));
	}

exit:
	if (retval < 0)
		pr_err("gatemp_get_config failed! status = 0x%x", retval);
	return;
}

s32 gatemp_setup(const struct gatemp_config *cfg)
{
	s32 retval = 0;
	struct gatemp_config tmp_cfg;
	int i;
	struct nameserver_params params;

	/* This sets the ref_count variable is not initialized, upper 16 bits is
	* written with module Id to ensure correctness of refCount variable.
	*/
	atomic_cmpmask_and_set(&gatemp_module->ref_count,
				GATEMP_MAKE_MAGICSTAMP(0),
				GATEMP_MAKE_MAGICSTAMP(0));
	if (atomic_inc_return(&gatemp_module->ref_count)
				!= GATEMP_MAKE_MAGICSTAMP(1)) {
		return 1;
	}

	if (cfg == NULL) {
		gatemp_get_config(&tmp_cfg);
		cfg = &tmp_cfg;
	}

	gatemp_module->default_gate = NULL;
	for (i = 0; i < GATEMP_PROXYORDER_NUM; i++)
		gatemp_module->proxy_map[i] = i;

	if ((void *)gatemp_remote_custom1_proxy_create
			== (void *)gatemp_remote_system_proxy_create) {
		gatemp_module->proxy_map[GATEMP_PROXYORDER_CUSTOM1] =
			gatemp_module->proxy_map[GATEMP_PROXYORDER_SYSTEM];
	}

	if ((void *) gatemp_remote_system_proxy_create
			== (void *) gatemp_remote_custom2_proxy_create) {
		gatemp_module->proxy_map[GATEMP_PROXYORDER_CUSTOM2] =
			gatemp_module->proxy_map[GATEMP_PROXYORDER_SYSTEM];
	} else if ((void *) gatemp_remote_custom2_proxy_create
			== (void *) gatemp_remote_custom1_proxy_create) {
		gatemp_module->proxy_map[GATEMP_PROXYORDER_CUSTOM2] =
			gatemp_module->proxy_map[GATEMP_PROXYORDER_CUSTOM1];
	}

	/* Create MutexPri gate */
	gatemp_module->gate_mutex = kmalloc(sizeof(struct mutex), GFP_KERNEL);
	if (gatemp_module->gate_mutex == NULL) {
		retval = -ENOMEM;
		goto exit;
	}
	mutex_init(gatemp_module->gate_mutex);

	/* create Nameserver */
	nameserver_params_init(&params);
	params.max_runtime_entries = cfg->max_runtime_entries;
	params.max_name_len = cfg->max_name_len;
	params.max_value_len = 2 * sizeof(u32);
	gatemp_module->name_server = nameserver_create(GATEMP_NAMESERVER,
							&params);
	if (gatemp_module->name_server == NULL) {
		retval = -1;
		goto error_nameserver;
	}

	/* Get the number of configured instances from the plugged in
	* Proxy gates */
	gatemp_module->num_remote_system = \
				gatemp_remote_system_proxy_get_num_instances();
	gatemp_module->num_remote_custom1 = \
				gatemp_remote_custom1_proxy_get_num_instances();
	gatemp_module->num_remote_custom2 = \
				gatemp_remote_custom2_proxy_get_num_instances();
	gatemp_module->num_remote_system_reserved = \
				gatemp_remote_system_proxy_get_num_reserved();
	gatemp_module->num_remote_custom1_reserved = \
				gatemp_remote_custom1_proxy_get_num_reserved();
	gatemp_module->num_remote_custom2_reserved = \
				gatemp_remote_custom2_proxy_get_num_reserved();
	gatemp_module->remote_system_in_use_alloc = \
		kzalloc((sizeof(u8) * cfg->num_resources), GFP_KERNEL);
	if (gatemp_module->remote_system_in_use_alloc == NULL) {
		retval = -ENOMEM;
		goto error_remote_system_fail;
	}
	gatemp_module->remote_system_in_use = \
				gatemp_module->remote_system_in_use_alloc;

	gatemp_module->remote_custom1_in_use_alloc = \
		kzalloc((sizeof(u8) * cfg->num_resources), GFP_KERNEL);
	if (gatemp_module->remote_custom1_in_use_alloc == NULL) {
		retval = -ENOMEM;
		goto error_remote_custom1_fail;
	}
	gatemp_module->remote_custom1_in_use = \
				gatemp_module->remote_custom1_in_use_alloc;

	gatemp_module->remote_custom2_in_use_alloc = \
		kzalloc((sizeof(u8) * cfg->num_resources), GFP_KERNEL);
	if (gatemp_module->remote_custom2_in_use_alloc == NULL) {
		retval = -ENOMEM;
		goto error_remote_custom2_fail;
	}
	gatemp_module->remote_custom2_in_use = \
				gatemp_module->remote_custom2_in_use_alloc;

	if (gatemp_module->num_remote_system) {
		gatemp_module->remote_system_gates_alloc = kzalloc(
			(sizeof(void *) * gatemp_module->num_remote_system),
			GFP_KERNEL);
		if (gatemp_module->remote_system_gates_alloc == NULL) {
			retval = -ENOMEM;
			goto error_remote_system_gates_fail;
		}
	} else
		gatemp_module->remote_system_gates_alloc = NULL;
	gatemp_module->remote_system_gates = \
				gatemp_module->remote_system_gates_alloc;

	if (gatemp_module->num_remote_custom1) {
		gatemp_module->remote_custom1_gates_alloc = kzalloc(
			(sizeof(void *) * gatemp_module->num_remote_custom1),
			GFP_KERNEL);
		if (gatemp_module->remote_custom1_gates_alloc == NULL) {
			retval = -ENOMEM;
			goto error_remote_custom1_gates_fail;
		}
	} else
		gatemp_module->remote_custom1_gates_alloc = NULL;
	gatemp_module->remote_custom1_gates = \
				gatemp_module->remote_custom1_gates_alloc;

	if (gatemp_module->num_remote_custom2) {
		gatemp_module->remote_custom2_gates_alloc = kzalloc(
			(sizeof(void *) * gatemp_module->num_remote_custom2),
			GFP_KERNEL);
		if (gatemp_module->remote_custom2_gates_alloc == NULL) {
			retval = -ENOMEM;
			goto error_remote_custom2_gates_fail;
		}
	} else
		gatemp_module->remote_custom2_gates_alloc = NULL;
	gatemp_module->remote_custom2_gates = \
				gatemp_module->remote_custom2_gates_alloc;

	/* Copy the cfg */
	memcpy((void *) &gatemp_module->cfg, (void *) cfg,
			sizeof(struct gatemp_config));
	return 0;

error_remote_custom2_gates_fail:
	kfree(gatemp_module->remote_custom1_gates_alloc);
	gatemp_module->remote_custom1_gates_alloc = NULL;
	gatemp_module->remote_custom1_gates = NULL;
error_remote_custom1_gates_fail:
	kfree(gatemp_module->remote_system_gates_alloc);
	gatemp_module->remote_system_gates_alloc = NULL;
	gatemp_module->remote_system_gates = NULL;
error_remote_system_gates_fail:
	kfree(gatemp_module->remote_custom2_in_use_alloc);
	gatemp_module->remote_custom2_in_use_alloc = NULL;
	gatemp_module->remote_custom2_in_use = NULL;
error_remote_custom2_fail:
	kfree(gatemp_module->remote_custom1_in_use_alloc);
	gatemp_module->remote_custom1_in_use_alloc = NULL;
	gatemp_module->remote_custom1_in_use = NULL;
error_remote_custom1_fail:
	kfree(gatemp_module->remote_system_in_use_alloc);
	gatemp_module->remote_system_in_use_alloc = NULL;
	gatemp_module->remote_system_in_use = NULL;
error_remote_system_fail:
	if (gatemp_module->name_server)
		nameserver_delete(&gatemp_module->name_server);
error_nameserver:
	kfree(gatemp_module->gate_mutex);
	gatemp_module->gate_mutex = NULL;
exit:
	pr_err("gatemp_setup failed! status = 0x%x", retval);
	return retval;
}

s32 gatemp_destroy(void)
{
	s32 retval = 0;

	if (WARN_ON(unlikely(atomic_cmpmask_and_lt(
				&(gatemp_module->ref_count),
				GATEMP_MAKE_MAGICSTAMP(0),
				GATEMP_MAKE_MAGICSTAMP(1)) == true))) {
		retval = -ENODEV;
		goto exit;
	}
	if (!(atomic_dec_return(&gatemp_module->ref_count)
			== GATEMP_MAKE_MAGICSTAMP(0))) {
		retval = 1;
		goto exit;
	}

	kfree(gatemp_module->gate_mutex);
	gatemp_module->gate_mutex = NULL;

	if (gatemp_module->name_server)
		nameserver_delete(&gatemp_module->name_server);

	kfree(gatemp_module->remote_system_in_use_alloc);
	gatemp_module->remote_system_in_use_alloc = NULL;
	gatemp_module->remote_system_in_use = NULL;

	kfree(gatemp_module->remote_custom1_in_use_alloc);
	gatemp_module->remote_custom1_in_use_alloc = NULL;
	gatemp_module->remote_custom1_in_use = NULL;

	kfree(gatemp_module->remote_custom2_in_use_alloc);
	gatemp_module->remote_custom2_in_use_alloc = NULL;
	gatemp_module->remote_custom2_in_use = NULL;

	kfree(gatemp_module->remote_system_gates_alloc);
	gatemp_module->remote_system_gates_alloc = NULL;
	gatemp_module->remote_system_gates = NULL;

	kfree(gatemp_module->remote_custom1_gates_alloc);
	gatemp_module->remote_custom1_gates_alloc = NULL;
	gatemp_module->remote_custom1_gates = NULL;

	kfree(gatemp_module->remote_custom2_gates_alloc);
	gatemp_module->remote_custom2_gates_alloc = NULL;
	gatemp_module->remote_custom2_gates = NULL;

	/* Clear cfg area */
	memset((void *) &gatemp_module->cfg, 0, sizeof(struct gatemp_config));
	gatemp_module->is_owner = false;
	return 0;

exit:
	if (retval < 0)
		pr_err("gatemp_destroy failed! status = 0x%x", retval);
	return retval;
}

static void _gatemp_get_shared_params(struct gatemp_params *sparams,
					const struct _gatemp_params *params)
{
	sparams->name = params->name;
	sparams->region_id = params->region_id;
	sparams->shared_addr = params->shared_addr;
	sparams->local_protect = \
			(enum gatemp_local_protect) params->local_protect;
	sparams->remote_protect = \
			(enum gatemp_remote_protect) params->remote_protect;
}

void gatemp_params_init(struct gatemp_params *params)
{
	params->name = NULL;
	params->region_id = 0;
	params->shared_addr = NULL;
	params->local_protect = GATEMP_LOCALPROTECT_INTERRUPT;
	params->remote_protect = GATEMP_REMOTEPROTECT_SYSTEM;
}

static int gatemp_instance_init(struct gatemp_object *obj,
			 const struct _gatemp_params *params)
{
	int *key;
	void *remote_handle;
	gatemp_remote_system_proxy_params system_params;
	gatemp_remote_custom1_proxy_params custom1_params;
	gatemp_remote_custom2_proxy_params custom2_params;
	u32 min_align;
	u32 offset;
	u32 *shared_shm_base;
	struct gatemp_params sparams;
	u32 ns_value[2];
	uint cache_line_size = 0;
	int retval = 0;
	void *region_heap;

	/* No parameter check since this function will be called internally */

	/* Initialize resource_id to an invalid value */
	obj->resource_id = (u32)-1;

	/* Open GateMP instance */
	if (params->open_flag == true) {
		/* all open work done here except for remote gate_handle */
		obj->local_protect = params->local_protect;
		obj->remote_protect = params->remote_protect;
		obj->ns_key = NULL;
		obj->num_opens = 1;
		obj->creator_proc_id = MULTIPROC_INVALIDID;
		obj->attrs = (struct gatemp_attrs *)params->shared_addr;
		obj->region_id = sharedregion_get_id((void *)obj->attrs);
		obj->cache_enabled = \
				sharedregion_is_cache_enabled(obj->region_id);
		obj->obj_type = IPC_OBJTYPE_OPENDYNAMIC;

		/* Assert that the buffer is in a valid shared region */
		if (obj->region_id == SHAREDREGION_INVALIDREGIONID)
			retval = 1;

		if (retval == 0) {
			cache_line_size = sharedregion_get_cache_line_size(
								obj->region_id);

			obj->alloc_size = 0;

			/*min_align = Memory_getMaxDefaultTypeAlign();*/
			min_align = 4;
			if (cache_line_size > min_align)
				min_align = cache_line_size;

			offset = ROUND_UP(sizeof(struct gatemp_attrs), \
								min_align);
			obj->proxy_attrs = (void *)((u32)obj->attrs + offset);
		}
		goto proxy_work;
	}

	/* Create GateMP instance */
	obj->local_protect = params->local_protect;
	obj->remote_protect = params->remote_protect;
	obj->ns_key = NULL;
	obj->num_opens = 0;
	obj->creator_proc_id = multiproc_self();

	/* No Remote Protection needed, just create the local protection */
	if (obj->remote_protect == GATEMP_REMOTEPROTECT_NONE) {
		/* Creating a local gate (Attrs is in local memory) */
		/* all work done here and return */
		obj->gate_handle = gatemp_create_local(obj->local_protect);

		if (params->shared_addr != NULL) {
			obj->attrs = params->shared_addr;
			obj->obj_type = IPC_OBJTYPE_CREATEDYNAMIC;
			/* Need cache settings since attrs is in shared mem */
			obj->region_id = \
				sharedregion_get_id((void *)obj->attrs);
			obj->cache_enabled = \
				sharedregion_is_cache_enabled(obj->region_id);
		} else {
			obj->obj_type = IPC_OBJTYPE_LOCAL;
			obj->cache_enabled = false; /* local */
			obj->region_id = SHAREDREGION_INVALIDREGIONID;
			/* Using default target alignment */
			obj->attrs = kmalloc(sizeof(struct gatemp_attrs),
						GFP_KERNEL);
			if (obj->attrs == NULL)
				return 2;
		}

		if (retval == 0) {
			obj->attrs->arg = (u32)obj;
			obj->attrs->mask = SETMASK(obj->remote_protect,
							obj->local_protect);
			obj->attrs->creator_proc_id = obj->creator_proc_id;
			obj->attrs->status = GATEMP_CREATED;
#if 0
			if (obj->cache_enabled) {
				/* Need to write back memory if cache is enabled
				 * because cache will be invalidated during
				 * open_by_addr */
				Cache_wbInv(obj->attrs,
						sizeof(struct gatemp_attrs),
						Cache_Type_ALL, true);
			}
#endif
			if (params->name != NULL) {
				/*  Top 16 bits = procId of creator. Bottom 16
				 * bits = '0' if local, '1' otherwise */
				ns_value[0] = (u32)obj->attrs;
				ns_value[1] = multiproc_self() << 16;
				obj->ns_key = nameserver_add(
						gatemp_module->name_server,
						params->name, &ns_value,
						2 * sizeof(u32));
			}
		}
		goto proxy_work;
	}

	/* Create remote protection */
	if (params->shared_addr == NULL) {
		/* If sharedAddr = NULL we are creating dynamically from the
		 * heap */
		obj->obj_type = IPC_OBJTYPE_CREATEDYNAMIC_REGION;
		obj->region_id = params->region_id;
		_gatemp_get_shared_params(&sparams, params);
		obj->alloc_size = gatemp_shared_mem_req(&sparams);
		obj->cache_enabled = sharedregion_is_cache_enabled(
								obj->region_id);

		/* The region heap will do the alignment */
		region_heap = sharedregion_get_heap(obj->region_id);
		WARN_ON(region_heap == NULL);
		obj->attrs = sl_heap_alloc(region_heap, obj->alloc_size, 0);
		if (obj->attrs == NULL)
			retval = 3;

		if (retval == 0) {
			cache_line_size = sharedregion_get_cache_line_size(
								obj->region_id);
			/*min_align = Memory_getMaxDefaultTypeAlign();*/
			min_align = 4;

			if (cache_line_size > min_align)
				min_align = cache_line_size;

			offset = ROUND_UP(sizeof(struct gatemp_attrs), \
								min_align);
			obj->proxy_attrs = (void *)((u32)obj->attrs + offset);
		}
	} else { /* creating using shared_addr */
		obj->region_id = sharedregion_get_id(params->shared_addr);
		/* Assert that the buffer is in a valid shared region */
		if (obj->region_id == SHAREDREGION_INVALIDREGIONID)
			retval = 4;

		cache_line_size = sharedregion_get_cache_line_size(
								obj->region_id);
		/* Assert that shared_addr is cache aligned */
		if ((retval == 0) && (((u32)params->shared_addr % \
							cache_line_size) != 0))
			retval = 5;

		if (retval == 0) {
			obj->obj_type = IPC_OBJTYPE_CREATEDYNAMIC;
			obj->attrs = (struct gatemp_attrs *)params->shared_addr;
			obj->cache_enabled = \
				sharedregion_is_cache_enabled(obj->region_id);

			/*min_align = Memory_getMaxDefaultTypeAlign();*/
			min_align = 4;
			if (cache_line_size > min_align)
				min_align = cache_line_size;
			offset = ROUND_UP(sizeof(struct gatemp_attrs), \
								min_align);
			obj->proxy_attrs = (void *)((u32)obj->attrs + offset);
		}
	}

proxy_work:
	/* Proxy work for open and create done here */
	switch (obj->remote_protect) {
	case GATEMP_REMOTEPROTECT_SYSTEM:
		if (obj->obj_type != IPC_OBJTYPE_OPENDYNAMIC) {
			/* Created Instance */
			obj->resource_id = gatemp_get_free_resource(
				gatemp_module->remote_system_in_use,
				gatemp_module->num_remote_system,
				gatemp_module->num_remote_system_reserved);
			if (obj->resource_id == -1)
				retval = 6;
		} else {
			/* resource_id set by open call */
			obj->resource_id = params->resource_id;
		}

		if (retval == 0) {
			/* Create the proxy object */
			gatemp_remote_system_proxy_params_init(&system_params);
			system_params.resource_id = obj->resource_id;
			system_params.open_flag = \
				(obj->obj_type == IPC_OBJTYPE_OPENDYNAMIC);
			system_params.shared_addr = obj->proxy_attrs;
			system_params.region_id = obj->region_id;
			remote_handle = gatemp_remote_system_proxy_create(
					(enum igatempsupport_local_protect)
					obj->local_protect,
					&system_params);

			if (remote_handle == NULL)
				retval = 7;

			if (retval == 0) {
				/* Finish filling in the object */
				obj->gate_handle = remote_handle;

				/* Fill in the local array because it is
				 * cooked */
				key = Gate_enterSystem();
				gatemp_module->remote_system_gates[
					obj->resource_id] = (void *)obj;
				Gate_leaveSystem(key);
			}
		}
		break;

	case GATEMP_REMOTEPROTECT_CUSTOM1:
		if (obj->obj_type != IPC_OBJTYPE_OPENDYNAMIC) {
			/* Created Instance */
			obj->resource_id = gatemp_get_free_resource(
				gatemp_module->remote_custom1_in_use,
				gatemp_module->num_remote_custom1,
				gatemp_module->num_remote_custom1_reserved);
			if (obj->resource_id == -1)
				retval = 6;
		} else {
			/* resource_id set by open call */
			obj->resource_id = params->resource_id;
		}

		if (retval == 0) {
			/* Create the proxy object */
			gatemp_remote_custom1_proxy_params_init(\
							&custom1_params);
			custom1_params.resource_id = obj->resource_id;
			custom1_params.open_flag = \
				(obj->obj_type == IPC_OBJTYPE_OPENDYNAMIC);
			custom1_params.shared_addr = obj->proxy_attrs;
			custom1_params.region_id = obj->region_id;
			remote_handle = gatemp_remote_custom1_proxy_create(
					(enum igatempsupport_local_protect)
					obj->local_protect,
					&custom1_params);
			if (remote_handle == NULL)
				retval = 7;

			if (retval == 0) {
				/* Finish filling in the object */
				obj->gate_handle = remote_handle;

				/* Fill in the local array because it is
				 * cooked */
				key = Gate_enterSystem();
				gatemp_module->remote_custom1_gates[
					obj->resource_id] = (void *)obj;
				Gate_leaveSystem(key);
			}
		}
		break;

	case GATEMP_REMOTEPROTECT_CUSTOM2:
		if (obj->obj_type != IPC_OBJTYPE_OPENDYNAMIC) {
			/* Created Instance */
			obj->resource_id = gatemp_get_free_resource(
				gatemp_module->remote_custom2_in_use,
				gatemp_module->num_remote_custom2,
				gatemp_module->num_remote_custom1_reserved);
			if (obj->resource_id == -1)
				retval = 6;
		} else {
			/* resource_id set by open call */
			obj->resource_id = params->resource_id;
		}

		if (retval == 0) {
			/* Create the proxy object */
			gatemp_remote_custom2_proxy_params_init(\
							&custom2_params);
			custom2_params.resource_id = obj->resource_id;
			custom2_params.open_flag = \
				(obj->obj_type == IPC_OBJTYPE_OPENDYNAMIC);
			custom2_params.shared_addr = obj->proxy_attrs;
			custom2_params.region_id = obj->region_id;
			remote_handle = gatemp_remote_custom2_proxy_create(
					(enum igatempsupport_local_protect)
					obj->local_protect,
					&custom2_params);
			if (remote_handle == NULL)
				retval = 7;

			if (retval == 0) {
				/* Finish filling in the object */
				obj->gate_handle = remote_handle;

				/* Fill in the local array because it is
				 * cooked */
				key = Gate_enterSystem();
				gatemp_module->remote_custom2_gates[
					obj->resource_id] = (void *)obj;
				Gate_leaveSystem(key);
			}
		}
		break;

	default:
		break;
	}

	/* Place Name/Attrs into NameServer table */
	if ((obj->obj_type != IPC_OBJTYPE_OPENDYNAMIC) && (retval == 0)) {
		/* Fill in the attrs */
		obj->attrs->arg = obj->resource_id;
		obj->attrs->mask = \
			SETMASK(obj->remote_protect, obj->local_protect);
		obj->attrs->creator_proc_id = obj->creator_proc_id;
		obj->attrs->status = GATEMP_CREATED;
#if 0
		if (obj->cache_enabled) {
			Cache_wbInv(obj->attrs, sizeof(struct gatemp_attrs),
				Cache_Type_ALL, true);
		}
#endif

		if (params->name != NULL) {
			shared_shm_base = sharedregion_get_srptr(obj->attrs,
								obj->region_id);
			ns_value[0] = (u32)shared_shm_base;
			/*  Top 16 bits = procId of creator, Bottom 16
			 * bits = '0' if local, '1' otherwise */
			ns_value[1] = multiproc_self() << 16 | 1;
			obj->ns_key = nameserver_add(gatemp_module->name_server,
						params->name, &ns_value,
						2 * sizeof(u32));
			if (obj->ns_key == NULL)
				retval = 8;
		}
	}

	if (retval != 0) {
		pr_err("gatemp_instance_init failed! status = 0x%x", retval);
	}
	return retval;
}

static void gatemp_instance_finalize(struct gatemp_object *obj, int status)
{
	int *system_key = (int *)0;
	gatemp_remote_system_proxy_handle system_handle;
	gatemp_remote_custom1_proxy_handle custom1_handle;
	gatemp_remote_custom2_proxy_handle custom2_handle;
	int retval = 0;
	void **remote_handles = NULL;
	u8 *in_use_array = NULL;
	u32 num_resources = 0;

	/* No parameter check since this function will be called internally */

	/* Cannot call when numOpen is non-zero. */
	if (obj->num_opens != 0) {
		retval = GateMP_E_INVALIDSTATE;
		goto exit;
	}

	/* Remove from NameServer */
	if (obj->ns_key != NULL) {
		nameserver_remove_entry(gatemp_module->name_server,
			obj->ns_key);
	}
	/* Set the status to 0 */
	if (obj->obj_type != IPC_OBJTYPE_OPENDYNAMIC) {
		obj->attrs->status = 0;
#if 0
		if (obj->cache_enabled)
			Cache_wbInv(obj->attrs, sizeof(struct gatemp_attrs),
					Cache_Type_ALL, true);
#endif
	}

	/* If ObjType_LOCAL, memory was allocated from the local system heap.
	 * obj->attrs might be NULL if the Memory_alloc failed in Instance_init
	 */
	if (obj->remote_protect == GATEMP_REMOTEPROTECT_NONE)
		kfree(obj->attrs);

	/* Delete if a remote gate */
	switch (obj->remote_protect) {
		/* Delete proxy instance... need to downCast */
	case GATEMP_REMOTEPROTECT_SYSTEM:
		if (obj->gate_handle) {
			system_handle = (gatemp_remote_system_proxy_handle)
							(obj->gate_handle);
			gatemp_remote_system_proxy_delete(&system_handle);
		}
		in_use_array = gatemp_module->remote_system_in_use;
		remote_handles = gatemp_module->remote_system_gates;
		num_resources = gatemp_module->num_remote_system;
		break;
	case GATEMP_REMOTEPROTECT_CUSTOM1:
		if (obj->gate_handle) {
			custom1_handle = (gatemp_remote_custom1_proxy_handle)
							(obj->gate_handle);
			gatemp_remote_custom1_proxy_delete(&custom1_handle);
		}
		in_use_array = gatemp_module->remote_custom1_in_use;
		remote_handles = gatemp_module->remote_custom1_gates;
		num_resources = gatemp_module->num_remote_custom1;
		break;
	case GATEMP_REMOTEPROTECT_CUSTOM2:
		if (obj->gate_handle) {
			custom2_handle = (gatemp_remote_custom2_proxy_handle)
							(obj->gate_handle);
			gatemp_remote_custom2_proxy_delete(&custom2_handle);
		}
		in_use_array = gatemp_module->remote_custom2_in_use;
		remote_handles = gatemp_module->remote_custom2_gates;
		num_resources = gatemp_module->num_remote_custom2;
		break;
	case GATEMP_REMOTEPROTECT_NONE:
		/* Nothing else to finalize. Any alloc'ed memory has already
		 * been freed */
		return;
	default:
		/* Nothing to do */
		break;
	}

	/* Clear the handle array entry in local memory */
	if (obj->resource_id != (uint)-1)
		remote_handles[obj->resource_id] = NULL;

	if (obj->obj_type != IPC_OBJTYPE_OPENDYNAMIC &&
		obj->resource_id != (uint)-1) {
		/* Only enter default gate if not deleting default gate. */
		if (obj != gatemp_module->default_gate)
			system_key = gatemp_enter(gatemp_module->default_gate);
		/* Clear the resource used flag in shared memory */
		in_use_array[obj->resource_id] = false;
#if 0
		if (obj->cache_enabled) {
			Cache_wbInv(in_use_array, num_resources, Cache_Type_ALL,
					true);
		}
#endif
		/* Only leave default gate if not deleting default gate. */
		if (obj != gatemp_module->default_gate)
			gatemp_leave(gatemp_module->default_gate, system_key);
	}

	if (obj->obj_type == IPC_OBJTYPE_CREATEDYNAMIC_REGION) {
		if (obj->attrs) {
			/* Free memory allocated from the region heap */
			sl_heap_free(sharedregion_get_heap(obj->region_id),
					obj->attrs, obj->alloc_size);
		}
	}

exit:
	if (retval < 0) {
		pr_err("gatemp_instance_finalize failed! "
			"status = 0x%x", retval);
	}
	return;
}

int *gatemp_enter(void *obj)
{
	int *key;
	struct gatemp_object *gmp_handle = (struct gatemp_object *)obj;

	key = igateprovider_enter(gmp_handle->gate_handle);

	return key;
}

void gatemp_leave(void *obj, int *key)
{
	struct gatemp_object *gmp_handle = (struct gatemp_object *)obj;

	igateprovider_leave(gmp_handle->gate_handle, key);
}

int gatemp_open(char *name, void **handle)
{
	u32 *shared_shm_base;
	int retval;
	u32 len;
	void *shared_addr;
	u32 ns_value[2];

	if (WARN_ON(unlikely(atomic_cmpmask_and_lt(
				&(gatemp_module->ref_count),
				GATEMP_MAKE_MAGICSTAMP(0),
				GATEMP_MAKE_MAGICSTAMP(1)) == true))) {
		retval = -ENODEV;
		goto exit;
	}
	if (WARN_ON(unlikely(name == NULL))) {
		retval = -EINVAL;
		goto exit;
	}
	if (WARN_ON(unlikely(handle == NULL))) {
		retval = -EINVAL;
		goto exit;
	}

	len = sizeof(ns_value);
	/* Get the Attrs out of the NameServer instance.
	 * Search all processors. */
	retval = nameserver_get(gatemp_module->name_server, name, &ns_value,
				&len, NULL);
	if (retval < 0) {
		*handle = NULL;
		return GateMP_E_NOTFOUND;
	}

	/* The least significant bit of nsValue[1] == 0 means its a
	 * local GateMP, otherwise its a remote GateMP. */
	if (!(ns_value[1] & 0x1) && ((ns_value[1] >> 16) != multiproc_self())) {
		*handle = NULL;
		return -1;
	}

	if ((ns_value[1] & 0x1) == 0) {
		/* Opening a local GateMP locally. The GateMP is created
		* from a local heap so don't do SharedRegion Ptr conversion. */
		shared_addr = (u32 *)ns_value[0];
	} else {
		/* Opening a remote GateMP. Need to do SR ptr conversion. */
		shared_shm_base = (u32 *)ns_value[0];
		shared_addr = sharedregion_get_ptr(shared_shm_base);
	}

	retval = gatemp_open_by_addr(shared_addr, handle);

exit:
	if (retval < 0)
		pr_err("gatemp_open failed! status = 0x%x", retval);
	return retval;
}

int gatemp_open_by_addr(void *shared_addr, void **handle)
{
	int retval = 0;
	int *key;
	struct gatemp_object *obj = NULL;
	struct _gatemp_params params;
	struct gatemp_attrs *attrs;
#if 0
	u16 region_id;
#endif

	if (WARN_ON(unlikely(atomic_cmpmask_and_lt(
				&(gatemp_module->ref_count),
				GATEMP_MAKE_MAGICSTAMP(0),
				GATEMP_MAKE_MAGICSTAMP(1)) == true))) {
		retval = -ENODEV;
		goto exit;
	}
	if (WARN_ON(unlikely(shared_addr == NULL))) {
		retval = -EINVAL;
		goto exit;
	}
	if (WARN_ON(unlikely(handle == NULL))) {
		retval = -EINVAL;
		goto exit;
	}

	attrs = (struct gatemp_attrs *)shared_addr;

#if 0
	/* get the region id and invalidate attrs is needed */
	region_id = sharedregion_get_id(shared_addr);
	if (region_id != SHAREDREGION_INVALIDREGIONID) {
		if (sharedregion_is_cache_enabled(region_id))
			Cache_inv(attrs, sizeof(struct gatemp_attrs),
				Cache_Type_ALL, true);
	}
#endif

	if (attrs->status != GATEMP_CREATED) {
		retval = -1;
		goto exit;
	}

	/* Local gate */
	if (GETREMOTE(attrs->mask) == GATEMP_REMOTEPROTECT_NONE) {
		if (attrs->creator_proc_id != multiproc_self())
			retval = GateMP_E_LOCALGATE; /* TBD */
		else {
			key = Gate_enterSystem();
			obj = (void *)attrs->arg;
			*handle = obj;
			obj->num_opens++;
			Gate_leaveSystem(key);
		}
	} else {
		/* Remote case */
		switch (GETREMOTE(attrs->mask)) {
		case GATEMP_REMOTEPROTECT_SYSTEM:
			obj = (struct gatemp_object *)
				gatemp_module->remote_system_gates[attrs->arg];
			break;

		case GATEMP_REMOTEPROTECT_CUSTOM1:
			obj = (struct gatemp_object *)
				gatemp_module->remote_custom1_gates[attrs->arg];
			break;

		case GATEMP_REMOTEPROTECT_CUSTOM2:
			obj = (struct gatemp_object *)
				gatemp_module->remote_custom2_gates[attrs->arg];
			break;

		default:
			break;
		}

		/* If the object is NULL, then it must have been created on a
		 * remote processor. Need to create a local object. This is
		 * accomplished by setting the open_flag to true. */
		if ((retval == 0) && (obj == NULL)) {
			/* Create a GateMP object with the open_flag set to
			 * true */
			params.name = NULL;
			params.open_flag = true;
			params.shared_addr = shared_addr;
			params.resource_id = attrs->arg;
			params.local_protect = GETLOCAL(attrs->mask);
			params.remote_protect = GETREMOTE(attrs->mask);

			obj = _gatemp_create(&params);
			if (obj == NULL)
				retval = GateMP_E_FAIL;
		} else {
			obj->num_opens++;
		}

		/* Return the "opened" GateMP instance  */
		*handle = obj;
	}

exit:
	if (retval < 0)
		pr_err("gatemp_open_by_addr failed! status = 0x%x", retval);
	return retval;
}

int gatemp_close(void **handle)
{
	int *key;
	struct gatemp_object *gate_handle = NULL;
	int count;
	int retval = 0;

	if (WARN_ON(unlikely(atomic_cmpmask_and_lt(
				&(gatemp_module->ref_count),
				GATEMP_MAKE_MAGICSTAMP(0),
				GATEMP_MAKE_MAGICSTAMP(1)) == true))) {
		retval = -ENODEV;
		goto exit;
	}
	if (WARN_ON(unlikely((handle == NULL) || (*handle == NULL)))) {
		retval = -EINVAL;
		goto exit;
	}

	gate_handle = (struct gatemp_object *)(*handle);
	/* Cannot call with the num_opens equal to zero. This is either
	 * a created handle or been closed already. */
	if (unlikely(gate_handle->num_opens == 0)) {
		retval = -EINVAL;
		goto exit;
	}

	key = Gate_enterSystem();
	count = --gate_handle->num_opens;
	Gate_leaveSystem(key);

	/* If the gate is remote, call the close function */
	if (gate_handle->remote_protect != GATEMP_REMOTEPROTECT_NONE) {
		/* if the count is zero and the gate is opened, then this
		 * object was created in the open (i.e. the create happened
		 * on a remote processor.**/
		if ((count == 0) && \
			(gate_handle->obj_type == IPC_OBJTYPE_OPENDYNAMIC))
			gatemp_delete((void **)&gate_handle);
	}
	*handle = NULL;
	return 0;

exit:
	pr_err("gatemp_close failed! status = 0x%x", retval);
	return retval;
}

u32 *gatemp_get_shared_addr(void *obj)
{
	u32 *sr_ptr;
	struct gatemp_object *object = (struct gatemp_object *) obj;

	sr_ptr = sharedregion_get_srptr(object->attrs, object->region_id);

	return sr_ptr;
}

bool gatemp_query(int qual)
{
	return false;
}

void *gatemp_get_default_remote(void)
{
	return gatemp_module->default_gate;
}

enum gatemp_local_protect gatemp_get_local_protect(void *obj)
{
	struct gatemp_object *gmp_handle = (struct gatemp_object *)obj;

	WARN_ON(obj == NULL);

	return gmp_handle->local_protect;
}

enum gatemp_remote_protect gatemp_get_remote_protect(void *obj)
{
	struct gatemp_object *gmp_handle = (struct gatemp_object *)obj;

	WARN_ON(obj == NULL);

	return gmp_handle->remote_protect;
}

void *gatemp_create_local(enum gatemp_local_protect local_protect)
{
	void *gate_handle = NULL;

	/* Create the local gate. */
	switch (local_protect) {
	case GATEMP_LOCALPROTECT_NONE:
		/* Plug with the GateNull singleton */
		gate_handle = NULL;
		break;

	case GATEMP_LOCALPROTECT_INTERRUPT:
		/* Plug with the GateHwi singleton */
		gate_handle = gate_system_handle;
		break;

	case GATEMP_LOCALPROTECT_TASKLET:
		/* Plug with the GateSwi singleton */
		gate_handle = gatemp_module->gate_mutex;
		break;

	case GATEMP_LOCALPROTECT_THREAD:
	case GATEMP_LOCALPROTECT_PROCESS:
		/* Plug with the GateMutexPri singleton */
		gate_handle = gatemp_module->gate_mutex;
		break;

	default:
		break;
	}

	return gate_handle;
}

uint gatemp_shared_mem_req(const struct gatemp_params *params)
{
	uint mem_req, min_align;
	u16 region_id;
	gatemp_remote_system_proxy_params system_params;
	gatemp_remote_custom1_proxy_params custom1_params;
	gatemp_remote_custom2_proxy_params custom2_params;

	if (params->shared_addr)
		region_id = sharedregion_get_id(params->shared_addr);
	else
		region_id = params->region_id;

	/*min_align = Memory_getMaxDefaultTypeAlign();*/min_align = 4;
	if (sharedregion_get_cache_line_size(region_id) > min_align)
		min_align = sharedregion_get_cache_line_size(region_id);

	mem_req = ROUND_UP(sizeof(struct gatemp_attrs), min_align);

	/* add the amount of shared memory required by proxy */
	if (params->remote_protect == GATEMP_REMOTEPROTECT_SYSTEM) {
		gatemp_remote_system_proxy_params_init(&system_params);
		system_params.region_id = region_id;
		mem_req += gatemp_remote_system_proxy_shared_mem_req(
							&system_params);
	} else if (params->remote_protect == GATEMP_REMOTEPROTECT_CUSTOM1) {
		gatemp_remote_custom1_proxy_params_init(&custom1_params);
		custom1_params.region_id = region_id;
		mem_req += gatemp_remote_custom1_proxy_shared_mem_req(
							&custom1_params);
	} else if (params->remote_protect == GATEMP_REMOTEPROTECT_CUSTOM2) {
		gatemp_remote_custom2_proxy_params_init(&custom2_params);
		custom2_params.region_id = region_id;
		mem_req += gatemp_remote_custom2_proxy_shared_mem_req(
							&custom2_params);
	}

	return mem_req;
}

uint gatemp_get_region0_reserved_size(void)
{
	uint reserved, min_align;

	/*min_align = Memory_getMaxDefaultTypeAlign();*/min_align = 4;

	if (sharedregion_get_cache_line_size(0) > min_align)
		min_align = sharedregion_get_cache_line_size(0);

	reserved = ROUND_UP(sizeof(struct gatemp_reserved), min_align);

	reserved += ROUND_UP(gatemp_module->num_remote_system, min_align);

	if (gatemp_module->proxy_map[GATEMP_PROXYORDER_CUSTOM1] ==
			GATEMP_PROXYORDER_CUSTOM1) {
		reserved += ROUND_UP(gatemp_module->num_remote_custom1,
					min_align);
	}

	if (gatemp_module->proxy_map[GATEMP_PROXYORDER_CUSTOM2] ==
			GATEMP_PROXYORDER_CUSTOM2) {
		reserved += ROUND_UP(gatemp_module->num_remote_custom2,
					min_align);
	}

	return reserved;
}

static void gatemp_set_region0_reserved(void *shared_addr)
{
	struct gatemp_reserved *reserve;
	u32 min_align, offset;

	/*min_align = Memory_getMaxDefaultTypeAlign();*/min_align = 4;
	if (sharedregion_get_cache_line_size(0) > min_align)
		min_align = sharedregion_get_cache_line_size(0);

	/* setup struct gatemp_reserved fields */
	reserve = (struct gatemp_reserved *)shared_addr;
	reserve->version = GATEMP_VERSION;

	if (sharedregion_is_cache_enabled(0)) {
#if 0
		Cache_wbInv(shared_addr, sizeof(struct gatemp_reserved),
			Cache_Type_ALL, true);
#endif
	}

	/* initialize the in-use array in shared memory for the system gates. */
	offset = ROUND_UP(sizeof(struct gatemp_reserved), min_align);
	gatemp_module->remote_system_in_use =
					(void *)((u32)shared_addr + offset);
	memset(gatemp_module->remote_system_in_use, 0,
			gatemp_module->num_remote_system);

	if (sharedregion_is_cache_enabled(0)) {
#if 0
		Cache_wbInv(gatemp_module->remote_system_in_use,
				gatemp_module->num_remote_system,
				Cache_Type_ALL, true);
#endif
	}

	/* initialize the in-use array in shared memory for the custom1 gates.
	 * Need to check if this proxy is the same as system */
	offset = ROUND_UP(gatemp_module->num_remote_system, min_align);
	if (gatemp_module->proxy_map[GATEMP_PROXYORDER_CUSTOM1] ==
			GATEMP_PROXYORDER_CUSTOM1) {
		if (gatemp_module->num_remote_custom1 != 0) {
			gatemp_module->remote_custom1_in_use =
				gatemp_module->remote_system_in_use + offset;
		}

		memset(gatemp_module->remote_custom1_in_use, 0,
			gatemp_module->num_remote_custom1);

		if (sharedregion_is_cache_enabled(0)) {
#if 0
			Cache_wbInv(gatemp_module->remote_custom1_in_use,
					gatemp_module->num_remote_custom1,
					Cache_Type_ALL, true);
#endif
		}
	} else {
		gatemp_module->remote_custom1_in_use = \
					gatemp_module->remote_system_in_use;
		gatemp_module->remote_custom1_gates = \
					gatemp_module->remote_system_gates;
	}

	/*  initialize the in-use array in shared memory for the custom2 gates.
	 *  Need to check if this proxy is the same as system or custom1 */
	offset = ROUND_UP(gatemp_module->num_remote_custom1, min_align);
	if (gatemp_module->proxy_map[GATEMP_PROXYORDER_CUSTOM2] ==
			GATEMP_PROXYORDER_CUSTOM2) {
		if (gatemp_module->num_remote_custom2 != 0) {
			gatemp_module->remote_custom2_in_use =
				gatemp_module->remote_custom1_in_use + offset;
		}
		memset(gatemp_module->remote_custom2_in_use, 0,
			gatemp_module->num_remote_custom2);

		if (sharedregion_is_cache_enabled(0)) {
#if 0
			Cache_wbInv(gatemp_module->remote_custom2_in_use,
					gatemp_module->num_remote_custom2,
					Cache_Type_ALL, true);
#endif
		}
	} else if (gatemp_module->proxy_map[GATEMP_PROXYORDER_CUSTOM2] ==
			GATEMP_PROXYORDER_CUSTOM1) {
		gatemp_module->remote_custom2_in_use =
				gatemp_module->remote_custom1_in_use;
		gatemp_module->remote_custom2_gates =
				gatemp_module->remote_custom1_gates;
	} else {
		gatemp_module->remote_custom2_in_use = \
				gatemp_module->remote_system_in_use;
		gatemp_module->remote_custom2_gates = \
				gatemp_module->remote_system_gates;
	}

	return;
}

static void gatemp_clear_region0_reserved(void)
{
	pr_info("gatemp_clear_region0_reserved: either nothing to do "
		"or not implemented");
}

static void gatemp_open_region0_reserved(void *shared_addr)
{
	struct gatemp_reserved *reserve;
	u32 min_align, offset;

	/*min_align = Memory_getMaxDefaultTypeAlign();*/min_align = 4;
	if (sharedregion_get_cache_line_size(0) > min_align)
		min_align = sharedregion_get_cache_line_size(0);


	/* setup struct gatemp_reserved fields */
	reserve = (struct gatemp_reserved *)shared_addr;

	if (reserve->version != GATEMP_VERSION) {
		/* TBD */
		return;
	}

	offset = ROUND_UP(sizeof(struct gatemp_reserved), min_align);
	gatemp_module->remote_system_in_use = \
				(void *)((u32)shared_addr + offset);

	/* initialize the in-use array in shared memory for the custom1 gates.
	 *  Need to check if this proxy is the same as system */
	offset = ROUND_UP(gatemp_module->num_remote_system, min_align);
	if (gatemp_module->proxy_map[GATEMP_PROXYORDER_CUSTOM1] ==
			GATEMP_PROXYORDER_CUSTOM1) {
		if (gatemp_module->num_remote_custom1 != 0) {
			gatemp_module->remote_custom1_in_use =
				gatemp_module->remote_system_in_use + offset;
		}
	} else {
		gatemp_module->remote_custom1_in_use = \
				gatemp_module->remote_system_in_use;
		gatemp_module->remote_custom1_gates = \
				gatemp_module->remote_system_gates;
	}

	offset = ROUND_UP(gatemp_module->num_remote_custom1, min_align);
	if (gatemp_module->proxy_map[GATEMP_PROXYORDER_CUSTOM2] ==
			GATEMP_PROXYORDER_CUSTOM2) {
		if (gatemp_module->num_remote_custom2 != 0) {
			gatemp_module->remote_custom2_in_use =
				gatemp_module->remote_custom1_in_use + offset;
		}
	} else if (gatemp_module->proxy_map[GATEMP_PROXYORDER_CUSTOM2] ==
			GATEMP_PROXYORDER_CUSTOM1) {
		gatemp_module->remote_custom2_in_use =
				gatemp_module->remote_custom1_in_use;
		gatemp_module->remote_custom2_gates =
				gatemp_module->remote_custom1_gates;
	} else {
		gatemp_module->remote_custom2_in_use = \
				gatemp_module->remote_system_in_use;
		gatemp_module->remote_custom2_gates = \
				gatemp_module->remote_system_gates;
	}

	return;
}

static void gatemp_close_region0_reserved(void *shared_addr)
{
	pr_info("gatemp_close_region0_reserved: either nothing to do "
		"or not implemented");
}

static void gatemp_set_default_remote(void *handle)
{
	gatemp_module->default_gate = handle;
}

int gatemp_start(void *shared_addr)
{
	struct sharedregion_entry entry;
	struct gatemp_params gatemp_params;
	void *default_gate;
	int retval = 0;

	/* get region 0 information */
	sharedregion_get_entry(0, &entry);

	/* if entry owner proc is not specified return */
	if (entry.owner_proc_id != SHAREDREGION_DEFAULTOWNERID) {
		if (entry.owner_proc_id == multiproc_self()) {
			/* Intialize the locks if ncessary.*/
			gatemp_remote_system_proxy_locks_init();
			gatemp_remote_custom1_proxy_locks_init();
			gatemp_remote_custom2_proxy_locks_init();
		}

		/* Init params for default gate */
		gatemp_params_init(&gatemp_params);
		gatemp_params.shared_addr = (void *)((u32)shared_addr +
					gatemp_get_region0_reserved_size());
		gatemp_params.local_protect = GATEMP_LOCALPROTECT_TASKLET;

		if (multiproc_get_num_processors() > 1) {
			gatemp_params.remote_protect = \
						GATEMP_REMOTEPROTECT_SYSTEM;
		} else {
			gatemp_params.remote_protect = \
						GATEMP_REMOTEPROTECT_NONE;
		}

		if (entry.owner_proc_id == multiproc_self()) {
			gatemp_module->is_owner = true;

			/* if owner of the SharedRegion */
			gatemp_set_region0_reserved(shared_addr);

			/* create default GateMP */
			default_gate = gatemp_create(&gatemp_params);

			if (default_gate != NULL) {
				/* set the default GateMP for creator */
				gatemp_set_default_remote(default_gate);
			} else {
				retval = -1;
			}
		}
	}

	if (retval < 0)
		pr_err("gatemp_start failed! status = 0x%x", retval);
	return retval;
}

int gatemp_stop(void)
{
	int retval = 0;

	/* if entry owner proc is not specified return */
	if (gatemp_module->is_owner == true) {
		/* if owner of the SharedRegion */
		gatemp_clear_region0_reserved();

		gatemp_delete((void **)&gatemp_module->default_gate);

		/* set the default GateMP for creator */
		gatemp_set_default_remote(NULL);
	}

	return retval;
}


/*
 *************************************************************************
 *      Internal functions
 *************************************************************************
 */
static uint gatemp_get_free_resource(u8 *in_use, int num, int start_id)
{
	int *key = NULL;
	bool flag = false;
	uint resource_id;
	void *default_gate;

	/* Need to look at shared memory. Enter default gate */
	default_gate = gatemp_get_default_remote();

	if (default_gate)
		key = gatemp_enter(default_gate);

#if 0
	/* Invalidate cache before looking at the in-use flags */
	if (sharedregion_is_cache_enabled(0))
		Cache_inv(in_use, num * sizeof(u8), Cache_Type_ALL, true);
#endif

	/* Find a free resource id. Note: zero is reserved on the
	 * system proxy for the default gate. */
	for (resource_id = start_id; resource_id < num; resource_id++) {
		/* If not in-use, set the in_use to true to prevent other
		 * creates from getting this one. */
		if (in_use[resource_id] == false) {
			flag = true;

			/* Denote in shared memory that the resource is used */
			in_use[resource_id] = true;
			break;
		}
	}

#if 0
	/* Write-back if a in-use flag was changed */
	if (flag == true && sharedregion_is_cache_enabled(0))
		Cache_wbInv(in_use, num * sizeof(u8), Cache_Type_ALL, true);
#endif

	/* Done with the critical section */
	if (default_gate)
		gatemp_leave(default_gate, key);

	if (flag == false)
		resource_id = -1;

	return resource_id;
}

void *gatemp_create(const struct gatemp_params *params)
{
	struct _gatemp_params _params;
	struct gatemp_object *handle = NULL;
	s32 retval = 0;

	if (WARN_ON(unlikely(atomic_cmpmask_and_lt(
			&(gatemp_module->ref_count),
			GATEMP_MAKE_MAGICSTAMP(0),
			GATEMP_MAKE_MAGICSTAMP(1)) == true))) {
		retval = -ENODEV;
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

	memset(&_params, 0, sizeof(struct _gatemp_params));
	memcpy(&_params, params, sizeof(struct gatemp_params));

	handle = _gatemp_create(&_params);

exit:
	if (retval < 0)
		pr_err("gatemp_create failed! status = 0x%x", retval);
	return (void *)handle;
}

static struct gatemp_object *_gatemp_create(const struct _gatemp_params *params)
{
	struct gatemp_object *obj = NULL;
	s32 retval = 0;
	int *key;

	/* No parameter checking since internal function */

	obj = kmalloc(sizeof(struct gatemp_object), GFP_KERNEL);
	if (obj == NULL) {
		retval = -ENOMEM;
		goto exit;
	}

	obj->status = gatemp_instance_init(obj, params);
	if (obj->status != 0) {
		retval = -1;
		goto gatemp_init_fail;
	}

	key = Gate_enterSystem();
	if (gatemp_first_object == NULL) {
		gatemp_first_object = obj;
		obj->next = NULL;
	} else {
		obj->next = gatemp_first_object;
		gatemp_first_object = obj;
	}
	Gate_leaveSystem(key);
	return (void *)obj;

gatemp_init_fail:
	kfree(obj);
	obj = NULL;
exit:
	pr_err("_gatemp_create failed! status = 0x%x", retval);
	return (void *)NULL;
}

int gatemp_delete(void **handle)
{
	int *key;
	struct gatemp_object *temp;
	bool found = false;
	int retval = 0;

	if (WARN_ON(unlikely(atomic_cmpmask_and_lt(
				&(gatemp_module->ref_count),
				GATEMP_MAKE_MAGICSTAMP(0),
				GATEMP_MAKE_MAGICSTAMP(1)) == true))) {
		retval = -ENODEV;
		goto exit;
	}
	if (WARN_ON(unlikely((handle == NULL) || (*handle == NULL)))) {
		retval = -EINVAL;
		goto exit;
	}

	key = Gate_enterSystem();
	if ((struct gatemp_object *)*handle == gatemp_first_object) {
		gatemp_first_object = ((struct gatemp_object *)(*handle))->next;
		found = true;
	} else {
		temp = gatemp_first_object;
		while (temp) {
			if (temp->next == (struct gatemp_object *)(*handle)) {
				temp->next = ((struct gatemp_object *)
							(*handle))->next;
				found = true;
				break;
			} else {
				temp = temp->next;
			}
		}
	}
	Gate_leaveSystem(key);

	if (found == false) {
		retval = -EINVAL;
		goto exit;
	}

	gatemp_instance_finalize(*handle, ((struct gatemp_object *)
							(*handle))->status);
	kfree((*handle));
	*handle = NULL;
	return 0;

exit:
	pr_err("gatemp_delete failed! status = 0x%x", retval);
	return retval;
}

int gatemp_attach(u16 remote_proc_id, void *shared_addr)
{
	int retval = 0;
	void *gatemp_shared_addr;
	struct sharedregion_entry entry;
	void *default_gate;

	if (WARN_ON(unlikely(atomic_cmpmask_and_lt(
				&(gatemp_module->ref_count),
				GATEMP_MAKE_MAGICSTAMP(0),
				GATEMP_MAKE_MAGICSTAMP(1)) == true))) {
		retval = -ENODEV;
		goto exit;
	}
	if (WARN_ON(unlikely(shared_addr == NULL))) {
		retval = -EINVAL;
		goto exit;
	}
	if (atomic_inc_return(&gatemp_module->attach_ref_count) != 1)
		return 1;

	/* get region 0 information */
	sharedregion_get_entry(0, &entry);

	gatemp_shared_addr = (void *)((u32)shared_addr +
					gatemp_get_region0_reserved_size());

	if ((entry.owner_proc_id != multiproc_self()) &&
		(entry.owner_proc_id != SHAREDREGION_DEFAULTOWNERID)) {
		gatemp_module->is_owner = false;

		/* if not the owner of the SharedRegion */
		gatemp_open_region0_reserved(shared_addr);

		/* open the gate by address */
		retval = gatemp_open_by_addr(gatemp_shared_addr, &default_gate);
		/* set the default GateMP for opener */
		if (retval >= 0)
			gatemp_set_default_remote(default_gate);
	}

exit:
	if (retval < 0)
		pr_err("gatemp_attach failed! status = 0x%x", retval);
	return retval;
}

int gatemp_detach(u16 remote_proc_id, void *shared_addr)
{
	int retval = 0;

	if (WARN_ON(unlikely(atomic_cmpmask_and_lt(
				&(gatemp_module->ref_count),
				GATEMP_MAKE_MAGICSTAMP(0),
				GATEMP_MAKE_MAGICSTAMP(1)) == true))) {
		retval = -ENODEV;
		goto exit;
	}
	if (WARN_ON(unlikely(shared_addr == NULL))) {
		retval = -EINVAL;
		goto exit;
	}
	if (!(atomic_dec_return(&gatemp_module->attach_ref_count) == 0))
		return 1;

	/* if entry owner proc is not specified return */
	if (gatemp_module->is_owner == false) {
		gatemp_close_region0_reserved(shared_addr);

		retval = gatemp_close((void **)&gatemp_module->default_gate);

		/* set the default GateMP for opener */
		gatemp_set_default_remote(NULL);
	}

exit:
	if (retval < 0)
		pr_err("gatemp_detach failed! status = 0x%x", retval);
	return retval;
}
