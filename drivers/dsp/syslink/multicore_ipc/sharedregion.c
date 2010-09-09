/*
 *  sharedregion.c
 *
 *  The SharedRegion module is designed to be used in a
 *  multi-processor environment where there are memory regions
 *  that are shared and accessed across different processors
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
#include <linux/slab.h>
#include <syslink/atomic_linux.h>

#include <multiproc.h>
#include <nameserver.h>
#include <heapmemmp.h>
#include <sharedregion.h>

/* Macro to make a correct module magic number with refCount */
#define SHAREDREGION_MAKE_MAGICSTAMP(x)   ((SHAREDREGION_MODULEID << 16u) | (x))

#define SHAREDREGION_MAX_REGIONS_DEFAULT  4

#define ROUND_UP(a, b)	(((a) + ((b) - 1)) & (~((b) - 1)))

/* Module state object */
struct sharedregion_module_object {
	atomic_t ref_count; /* Reference count */
	struct mutex *local_lock; /* Handle to a gate instance */
	struct sharedregion_region *regions; /* Pointer to the regions */
	struct sharedregion_config cfg; /* Current config values */
	struct sharedregion_config def_cfg; /* Default config values */
	u32 num_offset_bits;
	/* no. of bits for the offset for a SRPtr. This value is calculated */
	u32 offset_mask; /* offset bitmask using for generating a SRPtr */
};

/* Shared region state object variable with default settings */
static struct sharedregion_module_object sharedregion_state = {
	.num_offset_bits = 0,
	.regions = NULL,
	.local_lock = NULL,
	.offset_mask = 0,
	.def_cfg.num_entries = 4u,
	.def_cfg.translate = true,
	.def_cfg.cache_line_size = 128u
};

/* Pointer to the SharedRegion module state */
static struct sharedregion_module_object *sharedregion_module = \
							&sharedregion_state;

/* Checks to make sure overlap does not exists.
 * Return error if overlap found. */
static int _sharedregion_check_overlap(void *base, u32 len);

/* Return the number of offsetBits bits */
static u32 _sharedregion_get_num_offset_bits(void);

/* This will get the sharedregion module configuration */
int sharedregion_get_config(struct sharedregion_config *config)
{
	BUG_ON((config == NULL));
	if (atomic_cmpmask_and_lt(&(sharedregion_module->ref_count),
				SHAREDREGION_MAKE_MAGICSTAMP(0),
				SHAREDREGION_MAKE_MAGICSTAMP(1)) == true) {
		memcpy(config, &sharedregion_module->def_cfg,
			sizeof(struct sharedregion_config));
	} else {
		memcpy(config, &sharedregion_module->cfg,
			sizeof(struct sharedregion_config));
	}
	return 0;
}
EXPORT_SYMBOL(sharedregion_get_config);

/* This will get setup the sharedregion module */
int sharedregion_setup(const struct sharedregion_config *config)
{
	struct sharedregion_config tmpcfg;
	u32 i;
	s32 retval = 0;

	/* This sets the refCount variable is not initialized, upper 16 bits is
	* written with module Id to ensure correctness of refCount variable
	*/
	atomic_cmpmask_and_set(&sharedregion_module->ref_count,
				SHAREDREGION_MAKE_MAGICSTAMP(0),
				SHAREDREGION_MAKE_MAGICSTAMP(0));

	if (atomic_inc_return(&sharedregion_module->ref_count)
			!= SHAREDREGION_MAKE_MAGICSTAMP(1)) {
		return 1;
	}

	if (config == NULL) {
		sharedregion_get_config(&tmpcfg);
		config = &tmpcfg;
	}

	if (WARN_ON(config->num_entries == 0)) {
		retval = -EINVAL;
		goto error;
	}

	memcpy(&sharedregion_module->cfg, config,
		sizeof(struct sharedregion_config));
	sharedregion_module->cfg.translate = true;

	sharedregion_module->regions = kmalloc(
					(sizeof(struct sharedregion_region) * \
					sharedregion_module->cfg.num_entries),
					GFP_KERNEL);
	if (sharedregion_module->regions == NULL) {
		retval = -ENOMEM;
		goto error;
	}
	for (i = 0; i < sharedregion_module->cfg.num_entries; i++) {
		sharedregion_module->regions[i].entry.base = NULL;
		sharedregion_module->regions[i].entry.len = 0;
		sharedregion_module->regions[i].entry.owner_proc_id = 0;
		sharedregion_module->regions[i].entry.is_valid = false;
		sharedregion_module->regions[i].entry.cache_enable = true;
		sharedregion_module->regions[i].entry.cache_line_size =
			sharedregion_module->cfg.cache_line_size;
		sharedregion_module->regions[i].entry.create_heap = false;
		sharedregion_module->regions[i].reserved_size = 0;
		sharedregion_module->regions[i].heap = NULL;
		sharedregion_module->regions[i].entry.name = NULL;
	}

	/* set the defaults for region 0  */
	sharedregion_module->regions[0].entry.create_heap = true;
	sharedregion_module->regions[0].entry.owner_proc_id = multiproc_self();

	sharedregion_module->num_offset_bits = \
					_sharedregion_get_num_offset_bits();
	sharedregion_module->offset_mask =
		(1 << sharedregion_module->num_offset_bits) - 1;

	sharedregion_module->local_lock = kmalloc(sizeof(struct mutex),
						GFP_KERNEL);
	if (sharedregion_module->local_lock == NULL) {
		retval = -ENOMEM;
		goto gate_create_fail;
	}
	mutex_init(sharedregion_module->local_lock);

	return 0;

gate_create_fail:
	kfree(sharedregion_module->regions);

error:
	printk(KERN_ERR "sharedregion_setup failed status:%x\n", retval);
	sharedregion_destroy();
	return retval;
}
EXPORT_SYMBOL(sharedregion_setup);

/* This will get destroy the sharedregion module */
int sharedregion_destroy(void)
{
	s32 retval = 0;
	void *local_lock = NULL;

	if (WARN_ON(atomic_cmpmask_and_lt(&(sharedregion_module->ref_count),
				SHAREDREGION_MAKE_MAGICSTAMP(0),
				SHAREDREGION_MAKE_MAGICSTAMP(1)) == true)) {
		retval = -ENODEV;
		goto error;
	}

	if (!(atomic_dec_return(&sharedregion_module->ref_count)
			== SHAREDREGION_MAKE_MAGICSTAMP(0))) {
		retval = 1;
		goto error;
	}

	retval = mutex_lock_interruptible(sharedregion_module->local_lock);
	if (retval)
		goto error;
	kfree(sharedregion_module->regions);
	memset(&sharedregion_module->cfg, 0,
		sizeof(struct sharedregion_config));
	sharedregion_module->num_offset_bits = 0;
	sharedregion_module->offset_mask = 0;
	mutex_unlock(sharedregion_module->local_lock);

	kfree(local_lock);
	return 0;

error:
	if (retval < 0) {
		printk(KERN_ERR "sharedregion_destroy failed status:%x\n",
			retval);
	}
	return retval;
}
EXPORT_SYMBOL(sharedregion_destroy);

/* Creates a heap by owner of region for each SharedRegion.
 * Function is called by Ipc_start(). Requires that SharedRegion 0
 * be valid before calling start(). */
int sharedregion_start(void)
{
	int retval = 0;
	struct sharedregion_region *region = NULL;
	void *shared_addr = NULL;
	struct heapmemmp_object *heap_handle = NULL;
	struct heapmemmp_params params;
	int i;

	if (WARN_ON(atomic_cmpmask_and_lt(&(sharedregion_module->ref_count),
				SHAREDREGION_MAKE_MAGICSTAMP(0),
				SHAREDREGION_MAKE_MAGICSTAMP(1)) == true)) {
		retval = -ENODEV;
		goto error;
	}

	if ((sharedregion_module->cfg.num_entries == 0) ||
		(sharedregion_module->regions[0].entry.is_valid == false)) {
		retval = -ENODEV;
		goto error;
	}

	/*
	 *  Loop through shared regions. If an owner of a region is specified
	 *  and create_heap has been specified for the SharedRegion, then
	 *  the owner creates a HeapMemMP and the other processors open it.
	 */
	for (i = 0; i < sharedregion_module->cfg.num_entries; i++) {
		region = &(sharedregion_module->regions[i]);
		if ((region->entry.is_valid)
			&& (region->entry.owner_proc_id == multiproc_self())
			&& (region->entry.create_heap)
			&& (region->heap == NULL)) {
			/* get the next free address in each region */
			shared_addr = (void *)((u32) region->entry.base
						+ region->reserved_size);

			/* Create the HeapMemMP in the region. */
			heapmemmp_params_init(&params);
			params.shared_addr = shared_addr;
			params.shared_buf_size =
				region->entry.len - region->reserved_size;

			/* Adjust to account for the size of HeapMemMP_Attrs */
			params.shared_buf_size -=
				((heapmemmp_shared_mem_req(&params) - \
					params.shared_buf_size));
			heap_handle = heapmemmp_create(&params);
			if (heap_handle == NULL) {
				retval = -1;
				break;
			} else {
				region->heap = heap_handle;
			}
		}
	}

error:
	if (retval < 0) {
		printk(KERN_ERR "sharedregion_start failed status:%x\n",
			retval);
	}
	return retval;
}
EXPORT_SYMBOL(sharedregion_start);

/* Function to stop the SharedRegion module */
int sharedregion_stop(void)
{
	int retval = 0;
	int tmp_status = 0;
	struct sharedregion_region *region = NULL;
	int i;

	if (WARN_ON(atomic_cmpmask_and_lt(&(sharedregion_module->ref_count),
				SHAREDREGION_MAKE_MAGICSTAMP(0),
				SHAREDREGION_MAKE_MAGICSTAMP(1)) == true)) {
		retval = -ENODEV;
		goto error;
	}

	if (WARN_ON((sharedregion_module->cfg.num_entries == 0)
		|| (sharedregion_module->regions[0].entry.is_valid == false))) {
		retval = -ENODEV;
		goto error;
	}

	/*
	 *  Loop through shared regions. If an owner of a region is specified
	 *  and create_heap has been specified for the SharedRegion, then
	 *  the other processors close it and the owner deletes the HeapMemMP.
	 */
	for (i = 0; i < sharedregion_module->cfg.num_entries; i++) {
		region = &(sharedregion_module->regions[i]);
		if ((region->entry.is_valid)
			&& (region->entry.owner_proc_id == multiproc_self())
			&& (region->entry.create_heap)
			&& (region->heap != NULL)) {
			/* Delete heap */
			tmp_status = heapmemmp_delete((void **)&(region->heap));
			if ((tmp_status < 0) && (retval >= 0))
				retval = -1;
		}
		memset(region, 0, sizeof(struct sharedregion_region));
	}

	/* set the defaults for region 0  */
	sharedregion_module->regions[0].entry.create_heap = true;
	sharedregion_module->regions[0].entry.owner_proc_id = multiproc_self();

error:
	if (retval < 0)
		printk(KERN_ERR "sharedregion_stop failed status:%x\n", retval);
	return retval;
}
EXPORT_SYMBOL(sharedregion_stop);

/* Opens a heap, for non-owner processors, for each SharedRegion. */
int sharedregion_attach(u16 remote_proc_id)
{
	int retval = 0;
	struct sharedregion_region *region = NULL;
	void *shared_addr = NULL;
	int i;

	if (WARN_ON(atomic_cmpmask_and_lt(&(sharedregion_module->ref_count),
				SHAREDREGION_MAKE_MAGICSTAMP(0),
				SHAREDREGION_MAKE_MAGICSTAMP(1)) == true)) {
		retval = -ENODEV;
		goto error;
	}

	if (WARN_ON((remote_proc_id > MULTIPROC_MAXPROCESSORS))) {
		retval = -EINVAL;
		goto error;
	}

	/*
	 *  Loop through the regions and open the heap if not owner
	 */
	for (i = 0; i < sharedregion_module->cfg.num_entries; i++) {
		region = &(sharedregion_module->regions[i]);
		if ((region->entry.is_valid) && \
			(region->entry.owner_proc_id != multiproc_self()) && \
			(region->entry.owner_proc_id != \
			SHAREDREGION_DEFAULTOWNERID) && \
			(region->entry.create_heap) && (region->heap == NULL)) {
			/* SharedAddr should match creator's for each region */
			shared_addr = (void *)((u32) region->entry.base +
							region->reserved_size);

			/* Heap should already be created so open by address */
			retval = heapmemmp_open_by_addr(shared_addr,
						(void **) &(region->heap));
			if (retval < 0) {
				retval = -1;
				break;
			}
		}
	}

error:
	if (retval < 0) {
		printk(KERN_ERR "sharedregion_attach failed status:%x\n",
			retval);
	}
	return retval;
}
EXPORT_SYMBOL(sharedregion_attach);

/* Closes a heap, for non-owner processors, for each SharedRegion. */
int sharedregion_detach(u16 remote_proc_id)
{
	int retval = 0;
	int tmp_status = 0;
	struct sharedregion_region *region = NULL;
	u16 i;

	if (WARN_ON(atomic_cmpmask_and_lt(&(sharedregion_module->ref_count),
				SHAREDREGION_MAKE_MAGICSTAMP(0),
				SHAREDREGION_MAKE_MAGICSTAMP(1)) == true)) {
		retval = -ENODEV;
		goto error;
	}

	if (WARN_ON((remote_proc_id > MULTIPROC_MAXPROCESSORS))) {
		retval = -EINVAL;
		goto error;
	}

	/*
	 *  Loop through the regions and open the heap if not owner
	 */
	for (i = 0; i < sharedregion_module->cfg.num_entries; i++) {
		region = &(sharedregion_module->regions[i]);
		if ((region->entry.is_valid) && \
			(region->entry.owner_proc_id != multiproc_self()) && \
			(region->entry.owner_proc_id != \
			SHAREDREGION_DEFAULTOWNERID) && \
			(region->entry.create_heap) && (region->heap != NULL)) {
			/* Heap should already be created so open by address */
			tmp_status = heapmemmp_close((void **) &(region->heap));
			if ((tmp_status < 0) && (retval >= 0)) {
				retval = -1;
				printk(KERN_ERR "sharedregion_detach: "
					"heapmemmp_close failed!");
			}
		}
	}

error:
	if (retval < 0) {
		printk(KERN_ERR "sharedregion_detach failed status:%x\n",
			retval);
	}
	return retval;
}
EXPORT_SYMBOL(sharedregion_detach);

/* This will return the address pointer associated with the
 * shared region pointer */
void *sharedregion_get_ptr(u32 *srptr)
{
	struct sharedregion_region *region = NULL;
	void *return_ptr = NULL;
	u16 region_id;
	s32 retval = 0;

	if (WARN_ON(atomic_cmpmask_and_lt(&(sharedregion_module->ref_count),
				SHAREDREGION_MAKE_MAGICSTAMP(0),
				SHAREDREGION_MAKE_MAGICSTAMP(1)) == true)) {
		retval = -ENODEV;
		goto error;
	}

	if (srptr == SHAREDREGION_INVALIDSRPTR)
		goto error;

	if (sharedregion_module->cfg.translate == false)
		return_ptr = (void *)srptr;
	else {
		region_id = \
			((u32)(srptr) >> sharedregion_module->num_offset_bits);
		if (region_id >= sharedregion_module->cfg.num_entries) {
			retval = -EINVAL;
			goto error;
		}

		region = &(sharedregion_module->regions[region_id]);
		return_ptr = (void *)(((u32)srptr & \
					sharedregion_module->offset_mask) + \
					(u32) region->entry.base);
	}
	return return_ptr;

error:
	printk(KERN_ERR "sharedregion_get_ptr failed 0x%x\n", retval);
	return (void *)NULL;

}
EXPORT_SYMBOL(sharedregion_get_ptr);

/* This will return sharedregion pointer associated with the
 * an address in a shared region area registered with the
 * sharedregion module */
u32 *sharedregion_get_srptr(void *addr, u16 id)
{
	struct sharedregion_region *region = NULL;
	u32 *ret_ptr = SHAREDREGION_INVALIDSRPTR ;
	s32 retval = 0;

	if (WARN_ON(atomic_cmpmask_and_lt(&(sharedregion_module->ref_count),
				SHAREDREGION_MAKE_MAGICSTAMP(0),
				SHAREDREGION_MAKE_MAGICSTAMP(1)) == true)) {
		retval = -ENODEV;
		goto error;
	}

	if (WARN_ON(addr == NULL))
		goto error;

	if (WARN_ON(id >= sharedregion_module->cfg.num_entries))
		goto error;

	if (sharedregion_module->cfg.translate == false)
		ret_ptr = (u32 *)addr;
	else {
		region = &(sharedregion_module->regions[id]);
		/*
		 *  Note: The very last byte on the very last id cannot be
		 *        mapped because SharedRegion_INVALIDSRPTR which is ~0
		 *        denotes an error. Since pointers should be word
		 *        aligned, we don't expect this to be a problem.
		 *
		 *        ie: numEntries = 4, id = 3, base = 0x00000000,
		 *            len = 0x40000000 ==> address 0x3fffffff would be
		 *            invalid because the SRPtr for this address is
		 *            0xffffffff
		 */
		if (((u32) addr >= (u32) region->entry.base) && ((u32) addr < \
			((u32) region->entry.base + region->entry.len))) {
			ret_ptr = (u32 *)
				((id << sharedregion_module->num_offset_bits) |
				((u32) addr - (u32) region->entry.base));
		}
	}
	return ret_ptr;

error:
	printk(KERN_ERR	"sharedregion_get_srptr failed 0x%x\n", retval);
	return (u32 *)NULL;
}
EXPORT_SYMBOL(sharedregion_get_srptr);

#if 0
/*
 * ======== sharedregion_add ========
 *  Purpose:
 *  This will add a memory segment to the lookup table
 *  during runtime by base and length
 */
int sharedregion_add(u32 index, void *base, u32 len)
{
	struct sharedregion_info *entry = NULL;
	struct sharedregion_info *table = NULL;
	s32 retval = 0;
	u32 i;
	u16 myproc_id;
	bool overlap = false;
	bool same = false;

	if (WARN_ON(atomic_cmpmask_and_lt(&(sharedregion_module->ref_count),
				SHAREDREGION_MAKE_MAGICSTAMP(0),
				SHAREDREGION_MAKE_MAGICSTAMP(1)) == true)) {
		retval = -ENODEV;
		goto error;
	}

	if (index >= sharedregion_module->cfg.num_entries ||
			sharedregion_module->region_size < len) {
		retval = -EINVAL;
		goto error;
	}

	myproc_id = multiproc_get_id(NULL);
	retval = mutex_lock_interruptible(sharedregion_module->local_lock);
	if (retval)
		goto error;


	table = sharedregion_module->table;
	/* Check for overlap */
	for (i = 0; i < sharedregion_module->cfg.num_entries; i++) {
		entry = (table
			 + (myproc_id * sharedregion_module->cfg.num_entries)
			 + i);
		if (entry->is_valid) {
			/* Handle duplicate entry */
			if ((base == entry->base) && (len == entry->len)) {
				same = true;
				break;
			}

			if ((base >= entry->base) &&
				(base < (void *)
					((u32)entry->base + entry->len))) {
				overlap = true;
				break;
			}

			if ((base < entry->base) &&
				(void *)((u32)base + len) >= entry->base) {
				overlap = true;
				break;
			}
		}
	}

	if (same) {
		retval = 1;
		goto success;
	}

	if (overlap) {
		/* FHACK: FIX ME */
		retval = 1;
		goto mem_overlap_error;
	}

	entry = (table
		 + (myproc_id * sharedregion_module->cfg.num_entries)
		 + index);
	if (entry->is_valid == false) {
		entry->base = base;
		entry->len = len;
		entry->is_valid = true;

	} else {
		/* FHACK: FIX ME */
		sharedregion_module->ref_count_table[(myproc_id *
					sharedregion_module->cfg.num_entries)
					+ index] += 1;
		retval = 1;
		goto dup_entry_error;
	}

success:
	mutex_unlock(sharedregion_module->local_lock);
	return 0;

dup_entry_error: /* Fall through */
mem_overlap_error:
	printk(KERN_WARNING "sharedregion_add entry exists status: %x\n",
		retval);
	mutex_unlock(sharedregion_module->local_lock);

error:
	if (retval < 0)
		printk(KERN_ERR "sharedregion_add failed status:%x\n", retval);
	return retval;
}
EXPORT_SYMBOL(sharedregion_add);

/*
 * ======== sharedregion_remove ========
 *  Purpose:
 *  This will removes a memory segment to the lookup table
 *  during runtime by base and length
 */
int sharedregion_remove(u32 index)
{
	struct sharedregion_info *entry = NULL;
	struct sharedregion_info *table = NULL;
	u16 myproc_id;
	s32 retval = 0;

	if (WARN_ON(atomic_cmpmask_and_lt(&(sharedregion_module->ref_count),
				SHAREDREGION_MAKE_MAGICSTAMP(0),
				SHAREDREGION_MAKE_MAGICSTAMP(1)) == true)) {
		retval = -ENODEV;
		goto error;
	}

	if (index >= sharedregion_module->cfg.num_entries) {
		retval = -EINVAL;
		goto error;
	}

	retval = mutex_lock_interruptible(sharedregion_module->local_lock);
	if (retval)
		goto error;

	myproc_id = multiproc_get_id(NULL);
	table = sharedregion_module->table;
	entry = (table
		 + (myproc_id * sharedregion_module->cfg.num_entries)
		 + index);

	if (sharedregion_module->ref_count_table[(myproc_id *
			sharedregion_module->cfg.num_entries)
			+ index] > 0)
		sharedregion_module->ref_count_table[(myproc_id *
					sharedregion_module->cfg.num_entries)
					+ index] -= 1;
	else {
		entry->is_valid = false;
		entry->base = NULL;
		entry->len = 0;
	}
	mutex_unlock(sharedregion_module->local_lock);
	return 0;

error:
	printk(KERN_ERR "sharedregion_remove failed status:%x\n", retval);
	return retval;
}
EXPORT_SYMBOL(sharedregion_remove);

/*
 * ======== sharedregion_get_table_info ========
 *  Purpose:
 *  This will get the table entry information for the
 *  specified index and id
 */
int sharedregion_get_table_info(u32 index, u16 proc_id,
				struct sharedregion_info *info)
{
	struct sharedregion_info *entry = NULL;
	struct sharedregion_info *table = NULL;
	u16 proc_count;
	s32 retval = 0;

	BUG_ON(info == NULL);
	if (WARN_ON(atomic_cmpmask_and_lt(&(sharedregion_module->ref_count),
				SHAREDREGION_MAKE_MAGICSTAMP(0),
				SHAREDREGION_MAKE_MAGICSTAMP(1)) == true)) {
		retval = -ENODEV;
		goto error;
	}

	proc_count = multiproc_get_max_processors();
	if (index >= sharedregion_module->cfg.num_entries ||
			proc_id >= proc_count) {
		retval = -EINVAL;
		goto error;
	}

	retval = mutex_lock_interruptible(sharedregion_module->local_lock);
	if (retval)
		goto error;

	table = sharedregion_module->table;
	entry = (table
		 + (proc_id * sharedregion_module->cfg.num_entries)
		 + index);
	memcpy((void *) info, (void *) entry, sizeof(struct sharedregion_info));
	mutex_unlock(sharedregion_module->local_lock);
	return 0;

error:
	printk(KERN_ERR "sharedregion_get_table_info failed status:%x\n",
		retval);
	return retval;
}
EXPORT_SYMBOL(sharedregion_get_table_info);

/*
 * ======== sharedregion_set_table_info ========
 *  Purpose:
 *  This will set the table entry information for the
 *  specified index and id
 */
int sharedregion_set_table_info(u32 index, u16 proc_id,
				struct sharedregion_info *info)
{
	struct sharedregion_info *entry = NULL;
	struct sharedregion_info *table = NULL;
	u16 proc_count;
	s32 retval = 0;

	BUG_ON(info == NULL);
	if (WARN_ON(atomic_cmpmask_and_lt(&(sharedregion_module->ref_count),
				SHAREDREGION_MAKE_MAGICSTAMP(0),
				SHAREDREGION_MAKE_MAGICSTAMP(1)) == true)) {
		retval = -ENODEV;
		goto error;
	}

	proc_count = multiproc_get_max_processors();
	if (index >= sharedregion_module->cfg.num_entries ||
			proc_id >= proc_count) {
		retval = -EINVAL;
		goto error;
	}

	retval = mutex_lock_interruptible(sharedregion_module->local_lock);
	if (retval)
		goto error;

	table = sharedregion_module->table;
	entry = (table
		 + (proc_id * sharedregion_module->cfg.num_entries)
		 + index);
	memcpy((void *) entry, (void *) info, sizeof(struct sharedregion_info));
	mutex_unlock(sharedregion_module->local_lock);
	return 0;

error:
	printk(KERN_ERR "sharedregion_set_table_info failed status:%x\n",
		retval);
	return retval;
}
EXPORT_SYMBOL(sharedregion_set_table_info);
#endif

/* Return the region info */
void sharedregion_get_region_info(u16 id, struct sharedregion_region *region)
{
	struct sharedregion_region *regions = NULL;
	s32 retval = 0;

	if (WARN_ON(atomic_cmpmask_and_lt(&(sharedregion_module->ref_count),
				SHAREDREGION_MAKE_MAGICSTAMP(0),
				SHAREDREGION_MAKE_MAGICSTAMP(1)) == true)) {
		retval = -ENODEV;
		goto error;
	}

	if (WARN_ON(id >= sharedregion_module->cfg.num_entries)) {
		retval = -EINVAL;
		goto error;
	}

	if (WARN_ON(region == NULL)) {
		retval = -EINVAL;
		goto error;
	}

	regions = &(sharedregion_module->regions[id]);
	memcpy((void *) region, (void *) regions,
		sizeof(struct sharedregion_region));

error:
	if (retval < 0) {
		printk(KERN_ERR "sharedregion_get_region_info failed: "
			"status = 0x%x", retval);
	}
	return;
}

/* Whether address translation is enabled */
bool sharedregion_translate_enabled(void)
{
	return sharedregion_module->cfg.translate;
}

/* Gets the number of regions */
u16 sharedregion_get_num_regions(void)
{
	return sharedregion_module->cfg.num_entries;
}

/* Sets the table information entry in the table */
int sharedregion_set_entry(u16 id, struct sharedregion_entry *entry)
{
	int retval = 0;
	struct sharedregion_region *region = NULL;
	void *shared_addr = NULL;
	struct heapmemmp_object *heap_handle = NULL;
	struct heapmemmp_object **heap_handle_ptr = NULL;
	struct heapmemmp_params params;

	if (WARN_ON(atomic_cmpmask_and_lt(&(sharedregion_module->ref_count),
				SHAREDREGION_MAKE_MAGICSTAMP(0),
				SHAREDREGION_MAKE_MAGICSTAMP(1)) == true)) {
		retval = -ENODEV;
		goto error;
	}

	if (WARN_ON(entry == NULL)) {
		retval = -EINVAL;
		goto error;
	}

	if (WARN_ON(id >= sharedregion_module->cfg.num_entries)) {
		retval = -EINVAL;
		goto error;
	}

	region = &(sharedregion_module->regions[id]);

	/* Make sure region does not overlap existing ones */
	retval = _sharedregion_check_overlap(region->entry.base,
						region->entry.len);
	if (retval < 0) {
		printk(KERN_ERR "sharedregion_set_entry: Entry is overlapping "
			"existing entry!");
		goto error;
	}
	if (region->entry.is_valid) {
		/*region entry should be invalid at this point */
		retval = -EEXIST;
		printk(KERN_ERR "_sharedregion_setEntry: Entry already exists");
		goto error;
	}
	if ((entry->cache_enable) && (entry->cache_line_size == 0)) {
		/* if cache enabled, cache line size must != 0 */
		retval = -1;
		printk(KERN_ERR "_sharedregion_setEntry: If cache enabled, "
			"cache line size must != 0");
		goto error;
	}

	/* needs to be thread safe */
	retval = mutex_lock_interruptible(sharedregion_module->local_lock);
	if (retval)
		goto error;
	/* set specified region id to entry values */
	memcpy((void *)&(region->entry), (void *)entry,
		sizeof(struct sharedregion_entry));
	mutex_unlock(sharedregion_module->local_lock);

	if (entry->owner_proc_id == multiproc_self()) {
		if ((entry->create_heap) && (region->heap == NULL)) {
			/* get current Ptr (reserve memory with size of 0) */
			shared_addr = sharedregion_reserve_memory(id, 0);
			heapmemmp_params_init(&params);
			params.shared_addr = shared_addr;
			params.shared_buf_size = region->entry.len - \
						region->reserved_size;

			/*
			 *  Calculate size of HeapMemMP_Attrs and adjust
			 *  shared_buf_size. Size of HeapMemMP_Attrs =
			 *  HeapMemMP_sharedMemReq(&params) -
			 *  params.shared_buf_size
			 */
			params.shared_buf_size -= \
				(heapmemmp_shared_mem_req(&params) - \
				params.shared_buf_size);

			heap_handle = heapmemmp_create(&params);
			if (heap_handle == NULL) {
				region->entry.is_valid = false;
				retval = -ENOMEM;
				goto error;
			} else
				region->heap = heap_handle;
		}
	} else {
		if ((entry->create_heap) && (region->heap == NULL)) {
			/* shared_addr should match creator's for each region */
			shared_addr = (void *)((u32) region->entry.base
						+ region->reserved_size);

			/* set the pointer to a heap handle */
			heap_handle_ptr = \
				(struct heapmemmp_object **) &(region->heap);

			/* open the heap by address */
			retval = heapmemmp_open_by_addr(shared_addr, (void **)
							heap_handle_ptr);
			if (retval < 0) {
				region->entry.is_valid = false;
				retval = -1;
				goto error;
			}
		}
	}
	return 0;

error:
	printk(KERN_ERR "sharedregion_set_entry failed! status = 0x%x", retval);
	return retval;
}

/* Clears the region in the table */
int sharedregion_clear_entry(u16 id)
{
	int retval = 0;
	struct sharedregion_region *region = NULL;
	struct heapmemmp_object *heapmem_ptr = NULL;
	u16 my_id;
	u16 owner_proc_id;

	if (WARN_ON(atomic_cmpmask_and_lt(&(sharedregion_module->ref_count),
				SHAREDREGION_MAKE_MAGICSTAMP(0),
				SHAREDREGION_MAKE_MAGICSTAMP(1)) == true)) {
		retval = -ENODEV;
		goto error;
	}

	if (WARN_ON(id >= sharedregion_module->cfg.num_entries)) {
		retval = -EINVAL;
		goto error;
	}

	/* Need to make sure not trying to clear Region 0 */
	if (WARN_ON(id == 0)) {
		retval = -EINVAL;
		goto error;
	}

	my_id = multiproc_self();

	/* Needs to be thread safe */
	retval = mutex_lock_interruptible(sharedregion_module->local_lock);
	if (retval)
		goto error;
	region = &(sharedregion_module->regions[id]);

	/* Store these fields to local variables */
	owner_proc_id = region->entry.owner_proc_id;
	heapmem_ptr = region->heap;

	/* Clear region to their defaults */
	region->entry.is_valid = false;
	region->entry.base = NULL;
	region->entry.len = 0u;
	region->entry.owner_proc_id = SHAREDREGION_DEFAULTOWNERID;
	region->entry.cache_enable = true;
	region->entry.cache_line_size = \
				sharedregion_module->cfg.cache_line_size;
	region->entry.create_heap = false;
	region->entry.name = NULL;
	region->reserved_size = 0u;
	region->heap = NULL;
	mutex_unlock(sharedregion_module->local_lock);

	/* Delete or close previous created heap outside the gate */
	if (heapmem_ptr != NULL) {
		if (owner_proc_id == my_id) {
			retval = heapmemmp_delete((void **) &heapmem_ptr);
			if (retval < 0) {
				retval = -1;
				goto error;
			}
		} else if (owner_proc_id != (u16) SHAREDREGION_DEFAULTOWNERID) {
			retval = heapmemmp_close((void **) &heapmem_ptr);
			if (retval < 0) {
				retval = -1;
				goto error;
			}
		}
	}
	return 0;

error:
	printk(KERN_ERR "sharedregion_clear_entry failed! status = 0x%x",
		retval);
	return retval;
}

/* Clears the reserve memory for each region in the table */
void sharedregion_clear_reserved_memory(void)
{
	struct sharedregion_region *region = NULL;
	int i;

	/*
	 *  Loop through shared regions. If an owner of a region is specified,
	 *  the owner zeros out the reserved memory in each region.
	 */
	for (i = 0; i < sharedregion_module->cfg.num_entries; i++) {
		region = &(sharedregion_module->regions[i]);
		if ((region->entry.is_valid) && \
			(region->entry.owner_proc_id == multiproc_self())) {
			/* Clear reserved memory */
			memset(region->entry.base, 0, region->reserved_size);

			/* Writeback invalidate cache if enabled in region */
			if (region->entry.cache_enable) {
				/* TODO: Enable cache */
				/* Cache_wbInv(region->entry.base,
						region->reserved_size,
						Cache_Type_ALL,
						true); */
			}
		}
	}
}

/* Initializes the entry fields */
void sharedregion_entry_init(struct sharedregion_entry *entry)
{
	s32 retval = 0;

	if (WARN_ON(atomic_cmpmask_and_lt(&(sharedregion_module->ref_count),
				SHAREDREGION_MAKE_MAGICSTAMP(0),
				SHAREDREGION_MAKE_MAGICSTAMP(1)) == true)) {
		retval = -ENODEV;
		goto error;
	}

	if (WARN_ON(entry == NULL)) {
		retval = -EINVAL;
		goto error;
	}

	/* init the entry to default values */
	entry->base = NULL;
	entry->len = 0;
	entry->owner_proc_id = SHAREDREGION_DEFAULTOWNERID;
	entry->cache_enable = false; /*Set to true once cache API is done */
	entry->cache_line_size = sharedregion_module->cfg.cache_line_size;
	entry->create_heap = false;
	entry->name = NULL;
	entry->is_valid = false;

error:
	if (retval < 0) {
		printk(KERN_ERR "sharedregion_entry_init failed: "
			"status = 0x%x", retval);
	}
	return;
}

/* Returns Heap Handle of associated id */
void *sharedregion_get_heap(u16 id)
{
	struct heapmemmp_object *heap = NULL;
	s32 retval = 0;

	if (WARN_ON(atomic_cmpmask_and_lt(&(sharedregion_module->ref_count),
				SHAREDREGION_MAKE_MAGICSTAMP(0),
				SHAREDREGION_MAKE_MAGICSTAMP(1)) == true)) {
		retval = -ENODEV;
		goto error;
	}

	if (WARN_ON(id >= sharedregion_module->cfg.num_entries)) {
		retval = -EINVAL;
		goto error;
	}

	/*
	 *  If translate == true or translate == false
	 *  and 'id' is not INVALIDREGIONID, then assert id is valid.
	 *  Return the heap associated with the region id.
	 *
	 *  If those conditions are not met, the id is from
	 *  an addres in local memory so return NULL.
	 */
	if ((sharedregion_module->cfg.translate) || \
			((sharedregion_module->cfg.translate == false) && \
			(id != SHAREDREGION_INVALIDREGIONID))) {
		heap = sharedregion_module->regions[id].heap;
	}
	return (void *)heap;

error:
	printk(KERN_ERR "sharedregion_get_heap failed: status = 0x%x", retval);
	return (void *)NULL;
}

/* This will return the id for the specified address pointer. */
u16 sharedregion_get_id(void *addr)
{
	struct sharedregion_region *region = NULL;
	u16 region_id = SHAREDREGION_INVALIDREGIONID;
	u16 i;
	s32 retval = -ENOENT;

	if (WARN_ON(atomic_cmpmask_and_lt(&(sharedregion_module->ref_count),
				SHAREDREGION_MAKE_MAGICSTAMP(0),
				SHAREDREGION_MAKE_MAGICSTAMP(1)) == true)) {
		retval = -ENODEV;
		goto error;
	}

	if (WARN_ON(addr == NULL)) {
		retval = -EINVAL;
		goto error;
	}

	retval = mutex_lock_interruptible(sharedregion_module->local_lock);
	if (retval) {
		retval = -ENODEV;
		goto error;
	}
	for (i = 0; i < sharedregion_module->cfg.num_entries; i++) {
		region = &(sharedregion_module->regions[i]);
		if (region->entry.is_valid && (addr >= region->entry.base) &&
			(addr < (void *)((u32)region->entry.base + \
			(region->entry.len)))) {
			region_id = i;
			retval = 0;
			break;
		}
	}
	mutex_unlock(sharedregion_module->local_lock);

error:
	if (retval < 0) {
		printk(KERN_ERR "sharedregion_get_id failed: "
			"status = 0x%x", retval);
	}
	return region_id;
}
EXPORT_SYMBOL(sharedregion_get_id);

/* Returns the id of shared region that matches name.
 * Returns sharedregion_INVALIDREGIONID if no region is found. */
u16 sharedregion_get_id_by_name(char *name)
{
	struct sharedregion_region *region = NULL;
	u16 region_id = SHAREDREGION_INVALIDREGIONID;
	u16 i;
	s32 retval = 0;

	if (WARN_ON(atomic_cmpmask_and_lt(&(sharedregion_module->ref_count),
				SHAREDREGION_MAKE_MAGICSTAMP(0),
				SHAREDREGION_MAKE_MAGICSTAMP(1)) == true)) {
		retval = -ENODEV;
		goto error;
	}

	if (WARN_ON(name == NULL)) {
		retval = -EINVAL;
		goto error;
	}

	/* Needs to be thread safe */
	retval = mutex_lock_interruptible(sharedregion_module->local_lock);
	if (retval)
		goto error;
	/* loop through entries to find matching name */
	for (i = 0; i < sharedregion_module->cfg.num_entries; i++) {
		region = &(sharedregion_module->regions[i]);
		if (region->entry.is_valid) {
			if (strcmp(region->entry.name, name) == 0) {
				region_id = i;
				break;
			}
		}
	}
	mutex_unlock(sharedregion_module->local_lock);

error:
	if (retval < 0) {
		printk(KERN_ERR "sharedregion_get_id_by_name failed: "
			"status = 0x%x", retval);
	}
	return region_id;
}

/* Gets the entry information for the specified region id */
int sharedregion_get_entry(u16 id, struct sharedregion_entry *entry)
{
	int retval = 0;
	struct sharedregion_region *region = NULL;

	if (WARN_ON(atomic_cmpmask_and_lt(&(sharedregion_module->ref_count),
				SHAREDREGION_MAKE_MAGICSTAMP(0),
				SHAREDREGION_MAKE_MAGICSTAMP(1)) == true)) {
		retval = -ENODEV;
		goto error;
	}

	if (WARN_ON(entry == NULL)) {
		retval = -EINVAL;
		goto error;
	}

	if (WARN_ON(id >= sharedregion_module->cfg.num_entries)) {
		retval = -EINVAL;
		goto error;
	}

	region = &(sharedregion_module->regions[id]);
	memcpy((void *) entry, (void *) &(region->entry),
			sizeof(struct sharedregion_entry));
	return 0;

error:
	printk(KERN_ERR "sharedregion_get_entry failed: status = 0x%x", retval);
	return retval;
}

/* Get cache line size */
uint sharedregion_get_cache_line_size(u16 id)
{
	uint cache_line_size = sizeof(int);
	s32 retval = 0;

	if (WARN_ON(atomic_cmpmask_and_lt(&(sharedregion_module->ref_count),
				SHAREDREGION_MAKE_MAGICSTAMP(0),
				SHAREDREGION_MAKE_MAGICSTAMP(1)) == true)) {
		retval = -ENODEV;
		goto error;
	}

	if (WARN_ON(id >= sharedregion_module->cfg.num_entries)) {
		retval = -EINVAL;
		goto error;
	}

	/*
	 *  If translate == true or translate == false
	 *  and 'id' is not INVALIDREGIONID, then assert id is valid.
	 *  Return the heap associated with the region id.
	 *
	 *  If those conditions are not met, the id is from
	 *  an addres in local memory so return NULL.
	 */
	if ((sharedregion_module->cfg.translate) || \
			((sharedregion_module->cfg.translate == false) && \
			(id != SHAREDREGION_INVALIDREGIONID))) {
		cache_line_size =
			sharedregion_module->regions[id].entry.cache_line_size;
	}
	return cache_line_size;

error:
	printk(KERN_ERR "sharedregion_get_cache_line_size failed: "
		"status = 0x%x", retval);
	return cache_line_size;
}

/* Is cache enabled? */
bool sharedregion_is_cache_enabled(u16 id)
{
	bool cache_enable = false;
	s32 retval = 0;

	if (WARN_ON(atomic_cmpmask_and_lt(&(sharedregion_module->ref_count),
				SHAREDREGION_MAKE_MAGICSTAMP(0),
				SHAREDREGION_MAKE_MAGICSTAMP(1)) == true)) {
		retval = -ENODEV;
		goto error;
	}

	if (WARN_ON(id >= sharedregion_module->cfg.num_entries)) {
		retval = -EINVAL;
		goto error;
	}

	/*
	 *  If translate == true or translate == false
	 *  and 'id' is not INVALIDREGIONID, then assert id is valid.
	 *  Return the heap associated with the region id.
	 *
	 *  If those conditions are not met, the id is from
	 *  an address in local memory so return NULL.
	 */
	if ((sharedregion_module->cfg.translate) || \
			((sharedregion_module->cfg.translate == false) && \
			(id != SHAREDREGION_INVALIDREGIONID))) {
		cache_enable = \
			sharedregion_module->regions[id].entry.cache_enable;
	}
	return cache_enable;

error:
	printk(KERN_ERR "sharedregion_is_cache_enabled failed: "
		"status = 0x%x", retval);
	return false;
}

/* Reserves the specified amount of memory from the specified region id. */
void *sharedregion_reserve_memory(u16 id, uint size)
{
	void *ret_ptr = NULL;
	struct sharedregion_region *region = NULL;
	u32 min_align;
	uint new_size;
	uint cur_size;
	uint cache_line_size = 0;
	s32 retval = 0;

	if (WARN_ON(atomic_cmpmask_and_lt(&(sharedregion_module->ref_count),
				SHAREDREGION_MAKE_MAGICSTAMP(0),
				SHAREDREGION_MAKE_MAGICSTAMP(1)) == true)) {
		retval = -ENODEV;
		goto error;
	}

	if (WARN_ON(id >= sharedregion_module->cfg.num_entries)) {
		retval = -EINVAL;
		goto error;
	}

	if (WARN_ON(sharedregion_module->regions[id].entry.is_valid == false)) {
		retval = -EINVAL;
		goto error;
	}

	/*TODO: min_align = Memory_getMaxDefaultTypeAlign();*/min_align = 4;
	cache_line_size = sharedregion_get_cache_line_size(id);
	if (cache_line_size > min_align)
		min_align = cache_line_size;

	region = &(sharedregion_module->regions[id]);

	/* Set the current size to the reserved_size */
	cur_size = region->reserved_size;

	/* No need to round here since cur_size is already aligned */
	ret_ptr = (void *)((u32) region->entry.base + cur_size);

	/*  Round the new size to the min alignment since */
	new_size = ROUND_UP(size, min_align);

	/* Need to make sure (cur_size + new_size) is smaller than region len */
	if (region->entry.len < (cur_size + new_size)) {
		retval = -EINVAL;
		printk(KERN_ERR "sharedregion_reserve_memory: Too large size "
					"is requested to be reserved!");
		goto error;
	}

	/* Add the new size to current size */
	region->reserved_size = cur_size + new_size;
	return ret_ptr;

error:
	printk(KERN_ERR "sharedregion_reserve_memory failed: "
		"status = 0x%x", retval);
	return (void *)NULL;
}

/* Unreserve the specified amount of memory from the specified region id. */
void sharedregion_unreserve_memory(u16 id, uint size)
{
	struct sharedregion_region *region = NULL;
	u32 min_align;
	uint new_size;
	uint cur_size;
	uint cache_line_size = 0;
	s32 retval = 0;

	if (WARN_ON(atomic_cmpmask_and_lt(&(sharedregion_module->ref_count),
				SHAREDREGION_MAKE_MAGICSTAMP(0),
				SHAREDREGION_MAKE_MAGICSTAMP(1)) == true)) {
		retval = -ENODEV;
		goto error;
	}

	if (WARN_ON(id >= sharedregion_module->cfg.num_entries)) {
		retval = -EINVAL;
		goto error;
	}

	if (WARN_ON(sharedregion_module->regions[id].entry.is_valid == false)) {
		retval = -EINVAL;
		goto error;
	}

	/*TODO: min_align = Memory_getMaxDefaultTypeAlign();*/min_align = 4;
	cache_line_size = sharedregion_get_cache_line_size(id);
	if (cache_line_size > min_align)
		min_align = cache_line_size;

	region = &(sharedregion_module->regions[id]);

	/* Set the current size to the unreservedSize */
	cur_size = region->reserved_size;

	/*  Round the new size to the min alignment since */
	new_size = ROUND_UP(size, min_align);

	/* Add the new size to current size */
	region->reserved_size = cur_size - new_size;

error:
	if (retval < 0) {
		printk(KERN_ERR "sharedregion_unreserve_memory failed: "
			"status = 0x%x", retval);
	}
	return;
}

/* =============================================================================
 *  Internal Functions
 * =============================================================================
 */
/* Checks to make sure overlap does not exists. */
static int _sharedregion_check_overlap(void *base, u32 len)
{
	int retval = 0;
	struct sharedregion_region *region = NULL;
	u32 i;

	if (WARN_ON(atomic_cmpmask_and_lt(&(sharedregion_module->ref_count),
				SHAREDREGION_MAKE_MAGICSTAMP(0),
				SHAREDREGION_MAKE_MAGICSTAMP(1)) == true)) {
		retval = -ENODEV;
		goto error;
	}

	retval = mutex_lock_interruptible(sharedregion_module->local_lock);
	if (retval)
		goto error;

	/* check whether new region overlaps existing ones */
	for (i = 0; i < sharedregion_module->cfg.num_entries; i++) {
		region = &(sharedregion_module->regions[i]);
		if (region->entry.is_valid) {
			if (base >= region->entry.base) {
				if (base < (void *)((u32) region->entry.base
							+ region->entry.len)) {
					retval = -1;
					printk(KERN_ERR "_sharedregion_check_"
						"_overlap failed: Specified "
						"region falls within another "
						"region!");
					break;
				}
			} else {
				if ((void *)((u32) base + len) > \
					region->entry.base) {
					retval = -1;
					printk(KERN_ERR "_sharedregion_check_"
						"_overlap failed: Specified "
						"region spans across multiple "
						"regions!");
					break;
				}
			}
		}
	}

	mutex_unlock(sharedregion_module->local_lock);
	return 0;

error:
	printk(KERN_ERR "_sharedregion_check_overlap failed: "
		"status = 0x%x", retval);
	return retval;
}

/* Return the number of offset_bits bits */
static u32 _sharedregion_get_num_offset_bits(void)
{
	u32 num_entries = sharedregion_module->cfg.num_entries;
	u32 index_bits = 0;
	u32 num_offset_bits = 0;
	s32 retval = 0;

	if (WARN_ON(atomic_cmpmask_and_lt(&(sharedregion_module->ref_count),
				SHAREDREGION_MAKE_MAGICSTAMP(0),
				SHAREDREGION_MAKE_MAGICSTAMP(1)) == true)) {
		retval = -ENODEV;
		goto error;
	}

	if (num_entries == 0 || num_entries == 1)
		index_bits = num_entries;
	else {
		num_entries = num_entries - 1;

		/* determine the number of bits for the index */
		while (num_entries) {
			index_bits++;
			num_entries = num_entries >> 1;
		}
	}
	num_offset_bits = 32 - index_bits;

error:
	if (retval < 0) {
		printk(KERN_ERR "_sharedregion_get_num_offset_bits failed: "
			"status = 0x%x", retval);
	}
	return num_offset_bits;
}

/* Sets the table information entry in the table (doesn't create heap). */
int _sharedregion_set_entry(u16 id, struct sharedregion_entry *entry)
{
	int retval = 0;
	struct sharedregion_region *region = NULL;

	if (WARN_ON(atomic_cmpmask_and_lt(&(sharedregion_module->ref_count),
				SHAREDREGION_MAKE_MAGICSTAMP(0),
				SHAREDREGION_MAKE_MAGICSTAMP(1)) == true)) {
		retval = -ENODEV;
		goto error;
	}

	if (WARN_ON(entry == NULL)) {
		retval = -EINVAL;
		goto error;
	}

	if (WARN_ON(id >= sharedregion_module->cfg.num_entries)) {
		retval = -EINVAL;
		goto error;
	}

	region = &(sharedregion_module->regions[id]);

	/* Make sure region does not overlap existing ones */
	retval = _sharedregion_check_overlap(region->entry.base,
						region->entry.len);
	if (retval < 0) {
		printk(KERN_ERR "_sharedregion_set_entry: Entry is overlapping "
			"existing entry!");
		goto error;
	}
	if (region->entry.is_valid) {
		/*region entry should be invalid at this point */
		retval = -EEXIST;
		printk(KERN_ERR "_sharedregion_set_entry: ntry already exists");
		goto error;
	}
	/* Fail if cacheEnabled and cache_line_size equal 0 */
	if ((entry->cache_enable) && (entry->cache_line_size == 0)) {
		/* if cache enabled, cache line size must != 0 */
		retval = -1;
		printk(KERN_ERR "_sharedregion_set_entry: If cache enabled, "
			"cache line size must != 0");
		goto error;
	}

	/* needs to be thread safe */
	retval = mutex_lock_interruptible(sharedregion_module->local_lock);
	if (retval)
		goto error;
	/* set specified region id to entry values */
	memcpy((void *)&(region->entry), (void *)entry,
		sizeof(struct sharedregion_entry));
	mutex_unlock(sharedregion_module->local_lock);
	return 0;

error:
	printk(KERN_ERR "_sharedregion_set_entry failed! status = 0x%x",
		retval);
	return retval;
}
