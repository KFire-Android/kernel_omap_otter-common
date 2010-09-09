/*
 *  sysmemmgr.c
 *
 *  Manager for the Slave system memory. Slave system level memory is allocated
 *  through this modules.
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
#include <linux/slab.h>
/* Utils headers */
#include <linux/vmalloc.h>
#include <syslink/atomic_linux.h>
#include <syslink/platform_mem.h>
/*#include <GateMutex.h>
#include <Memory.h>
#include <Trace.h>*/


/* Module level headers */
#include <sysmemmgr.h>
/*#include <BuddyPageAllocator.h>*/


/* =============================================================================
 * Macros
 * =============================================================================
 */
/*! @brief Event reserved for System memory manager */
#define SYSMEMMGR_EVENTNO		12

/* Macro to make a correct module magic number with ref_count */
#define SYSMEMMGR_MAKE_MAGICSTAMP(x)	((SYSMEMMGR_MODULEID << 12) | (x))

/* =============================================================================
 * Structs & Enums
 * =============================================================================
 */
/*! @brief Structure containing list of buffers. The list is kept sorted by
 * address. */
struct sysmemmgr_static_mem_struct {
	struct sysmemmgr_static_mem_struct *next;
	/*!< Pointer to next entry */
	u32 address;
	/*!< Address of this entry */
	u32 size;
	/*!< Size of this entry */
};


/*! @brief Static memory manager object. */
struct sysmemmgr_static_mem_mgr_obj {
	struct sysmemmgr_static_mem_struct head;
	/*!< Pointer to head entry */
	struct sysmemmgr_static_mem_struct tail;
	/*!< Pointer to tail entry */
};

/*!
 *  @brief  Structure defining state object of system memory manager.
 */
struct sysmemmgr_module_object {
	atomic_t ref_count;
	/*!< Reference count */
	struct sysmemmgr_static_mem_mgr_obj static_mem_obj;
	/*!< Static memory manager object */
	struct mutex *gate_handle;
	/*!< Pointer to lock */
	struct sysmemmgr_config cfg;
	/*!< Current configuration values */
	struct sysmemmgr_config default_cfg;
	/*!< Default configuration values */
};


/*!
 *  @brief  Object containing state of the system memory manager.
 */
static struct sysmemmgr_module_object sysmemmgr_state = {
	.default_cfg.sizeof_valloc = 0x100000,
	.default_cfg.sizeof_palloc = 0x100000,
	.default_cfg.page_size = 0x1000,
	.default_cfg.event_no = SYSMEMMGR_EVENTNO,
};


/* =============================================================================
 * APIS
 * =============================================================================
 */
/*
 * ======== sysmemmgr_get_config ========
 *  Purpose:
 *  Function to get the default values for configuration.
 */
void sysmemmgr_get_config(struct sysmemmgr_config *config)
{
	if (WARN_ON(config == NULL))
		goto err_exit;

	if (atomic_cmpmask_and_lt(&(sysmemmgr_state.ref_count),
				SYSMEMMGR_MAKE_MAGICSTAMP(0),
				SYSMEMMGR_MAKE_MAGICSTAMP(1)) == true)
		memcpy((void *) config, (void *)(&sysmemmgr_state.default_cfg),
				sizeof(struct sysmemmgr_config));
	else
		memcpy((void *) config, (void *)(&sysmemmgr_state.cfg),
				sizeof(struct sysmemmgr_config));

	return;

err_exit:
	printk(KERN_ERR "sysmemmgr_get_config: Argument of type "
		"(struct sysmemmgr_config *) passed is NULL\n");
	return;
}


/*
 * ======== sysmemmgr_setup ========
 *  Purpose:
 *  Function to get the default values for configuration.
 */
int sysmemmgr_setup(struct sysmemmgr_config *config)
{
	int status = 0;
	struct sysmemmgr_static_mem_mgr_obj *smmObj = NULL;

	/* This sets the ref_count variable is not initialized, upper 16 bits is
	 * written with module Id to ensure correctness of ref_count variable.
	 */
	atomic_cmpmask_and_set(&sysmemmgr_state.ref_count,
		SYSMEMMGR_MAKE_MAGICSTAMP(0), SYSMEMMGR_MAKE_MAGICSTAMP(0));

	if (atomic_inc_return(&sysmemmgr_state.ref_count) != \
			SYSMEMMGR_MAKE_MAGICSTAMP(1)) {
		status = SYSMEMMGR_S_ALREADYSETUP;
		goto exit;
	}

	if (WARN_ON(config == NULL)) {
		/* Config parameters are not provided */
		status = -EINVAL;
		goto err_config;
	}
	if (WARN_ON((config->static_virt_base_addr == (u32) NULL)
			&& (config->static_mem_size != 0))) {
		/* Virtual Base address of static memory region is NULL */
		status = -EINVAL;
		goto err_virt_addr;
	}
	if (WARN_ON((config->static_phys_base_addr == (u32) NULL)
			&& (config->static_mem_size != 0))) {
		/*Physical Base address of static memory region is NULL */
		status = -EINVAL;
		goto err_phys_addr;
	}

	/* Copy the config parameters to the module state */
	memcpy((void *)(&sysmemmgr_state.cfg), (void *) config,
		sizeof(struct sysmemmgr_config));

	/* Create the static memory allocator */
	if (config->static_mem_size != 0) {
		smmObj = &sysmemmgr_state.static_mem_obj;
		smmObj->head.address = config->static_virt_base_addr;
		smmObj->head.size = 0;
		smmObj->tail.address = (config->static_virt_base_addr + \
						config->static_mem_size);
		smmObj->tail.size = 0;
		smmObj->head.next = &smmObj->tail;
		smmObj->tail.next = NULL;
	}

	/* Create the lock */
	sysmemmgr_state.gate_handle = kzalloc(sizeof(struct mutex), GFP_KERNEL);
	if (sysmemmgr_state.gate_handle == NULL) {
		/* Failed to create gate handle */
		status = -ENOMEM;
		goto err_mem_gate;
	}
	return 0;

err_mem_gate:
	printk(KERN_ERR "sysmemmgr_setup: Failed to create gate handle\n");
	goto exit;

err_phys_addr:
	printk(KERN_ERR "sysmemmgr_setup: Physical Base address of static "
		"memory region is NULL\n");
	goto exit;

err_virt_addr:
	printk(KERN_ERR "sysmemmgr_setup: Virtual Base address of static "
		"memory region is NULL\n");
	goto exit;

err_config:
	printk(KERN_ERR "sysmemmgr_setup: Argument of type "
		"(struct sysmemmgr_config *) passed is NULL\n");
	goto exit;

exit:
	if (status < 0) {
		atomic_set(&sysmemmgr_state.ref_count,
			SYSMEMMGR_MAKE_MAGICSTAMP(0));
	}
	return status;
}


/*
 * ======== sysmemmgr_destroy ========
 *  Purpose:
 *  Function to finalize the system memory manager module.
 */
int sysmemmgr_destroy(void)
{
	int status = 0;

	if (atomic_cmpmask_and_lt(&(sysmemmgr_state.ref_count),
		SYSMEMMGR_MAKE_MAGICSTAMP(0), SYSMEMMGR_MAKE_MAGICSTAMP(1)) == \
		true) {
		/*! @retval SYSMEMMGR_E_INVALIDSTATE Module was not
		 *  initialized */
		status = SYSMEMMGR_E_INVALIDSTATE;
		goto err_exit;
	}

	if (atomic_dec_return(&sysmemmgr_state.ref_count) == \
		SYSMEMMGR_MAKE_MAGICSTAMP(0)) {
		/* Delete the lock */
		kfree(sysmemmgr_state.gate_handle);
	}
	return 0;

err_exit:
	printk(KERN_ERR "sysmemgr_destroy: Module was not initialized\n");
	return status;
}


/*
 * ======== sysmemmgr_alloc ========
 *  Purpose:
 *  Function to allocate a memory block.
 */
void *sysmemmgr_alloc(u32 size, enum sysmemmgr_allocflag flag)
{
	int status = 0;
	struct sysmemmgr_static_mem_mgr_obj *smObj  = NULL;
	struct sysmemmgr_static_mem_struct *ptr = NULL;
	struct sysmemmgr_static_mem_struct *newptr = NULL;
	void *ret_ptr = NULL;

	if (atomic_cmpmask_and_lt(&(sysmemmgr_state.ref_count),
		SYSMEMMGR_MAKE_MAGICSTAMP(0), SYSMEMMGR_MAKE_MAGICSTAMP(1)) == \
		true) {
		/*! @retval SYSMEMMGR_E_INVALIDSTATE Module was not
		 *  initialized */
		status = SYSMEMMGR_E_INVALIDSTATE;
		goto err_exit;
	}

	if ((flag & sysmemmgr_allocflag_physical) && \
		!(flag & sysmemmgr_allocflag_dma)) {
		/* TBD: works with DMM
		ret_ptr = platform_mem_alloc (size, 0,
			MemoryOS_MemTypeFlags_Physical); */
		if (ret_ptr == NULL) {
			if (sysmemmgr_state.cfg.static_mem_size == 0) {
				/* Memory pool is not configured. */
				status = -ENOMEM;
				goto exit;
			}

			smObj = &sysmemmgr_state.static_mem_obj;
			ptr = &smObj->head;
			while (ptr && ptr->next) {
				if (((ptr->next->address - \
					(ptr->address + ptr->size)) >= size))
					break;
				ptr = ptr->next;
			}

			if (ptr->next == NULL) {
				status = -ENOMEM;
				goto exit;
			}

			newptr = vmalloc(
				sizeof(struct sysmemmgr_static_mem_struct));
			if (newptr != NULL) {
				newptr->address = ptr->address + ptr->size;
				newptr->size = size;
				newptr->next = ptr->next;
				ptr->next = newptr;
				ret_ptr = (void *) newptr->address;
			} else {
				status = -ENOMEM;
			}
		}
		goto exit;
	}

	if (flag & sysmemmgr_allocflag_physical) {
		ret_ptr = kmalloc(size, GFP_KERNEL);
		if (ret_ptr == NULL)
			status = -ENOMEM;
		goto exit;
	}

	if (flag & sysmemmgr_allocflag_dma) {
		ret_ptr = kmalloc(size, GFP_KERNEL | GFP_DMA);
		if (ret_ptr == NULL)
			status = -ENOMEM;
		goto exit;
	}

	ret_ptr = vmalloc(size);
	if (ret_ptr == NULL) {
		status = -ENOMEM;
		goto exit;
	}

err_exit:
	printk(KERN_ERR "sysmemgr_alloc: Module was not initialized\n");
exit:
	if (WARN_ON(ret_ptr == NULL))
		printk(KERN_ERR "sysmemmgr_alloc: Allocation failed\n");
	return ret_ptr;
}


/*
 * ======== sysmemmgr_free ========
 *  Purpose:
 *  Function to de-allocate a previous allocated memory block.
 */
int sysmemmgr_free(void *blk, u32 size, enum sysmemmgr_allocflag flag)
{
	int status = 0;
	struct sysmemmgr_static_mem_mgr_obj *smObj = NULL;
	struct sysmemmgr_static_mem_struct *ptr = NULL;
	struct sysmemmgr_static_mem_struct *prev = NULL;

	if (atomic_cmpmask_and_lt(&(sysmemmgr_state.ref_count),
		SYSMEMMGR_MAKE_MAGICSTAMP(0), SYSMEMMGR_MAKE_MAGICSTAMP(1)) == \
		true) {
		/*! @retval SYSMEMMGR_E_INVALIDSTATE Module was not
		 *  initialized */
		status = SYSMEMMGR_E_INVALIDSTATE;
		goto err_exit;
	}

	if ((flag & sysmemmgr_allocflag_physical) && \
		!(flag & sysmemmgr_allocflag_dma)) {
		if (((u32) blk >= sysmemmgr_state.cfg.static_virt_base_addr)
			&& ((u32) blk < \
			(sysmemmgr_state.cfg.static_virt_base_addr + \
			sysmemmgr_state.cfg.static_mem_size))) {
			smObj = &sysmemmgr_state.static_mem_obj;
			ptr = &smObj->head;
			while (ptr && ptr->next) {
				if (ptr->next->address == (u32) blk)
					break;
				ptr = ptr->next;
			}
			prev = ptr;
			ptr  = ptr->next;
			prev->next = ptr->next;

			/* Free the node */
			vfree(ptr);
		} else {
			kfree(blk);
		}
	} else if (flag & sysmemmgr_allocflag_physical) {
		kfree(blk);
	} else if (flag & sysmemmgr_allocflag_dma) {
		kfree(blk);
	} else {
		vfree(blk);
	}
	return 0;

err_exit:
	printk(KERN_ERR "sysmemgr_free: Module was not initialized\n");
	return status;
}


/*
 * ======== sysmemmgr_setup ========
 *  Purpose:
 *  Function to translate an address among different address spaces.
 */
void *sysmemmgr_translate(void *src_addr, enum sysmemmgr_xltflag flags)
{
	void *ret_ptr = NULL;

	switch (flags) {
	case sysmemmgr_xltflag_kvirt2phys:
	{
		if (((u32) src_addr >= \
			sysmemmgr_state.cfg.static_virt_base_addr) && \
			((u32) src_addr < \
			(sysmemmgr_state.cfg.static_virt_base_addr + \
				sysmemmgr_state.cfg.static_mem_size))) {
			ret_ptr = (void *)(((u32) src_addr - \
				sysmemmgr_state.cfg.static_virt_base_addr) + \
				(sysmemmgr_state.cfg.static_phys_base_addr));
		} else {
			ret_ptr = platform_mem_translate(src_addr,
					PLATFORM_MEM_XLT_FLAGS_VIRT2PHYS);
		}
	}
	break;

	case sysmemmgr_xltflag_phys2kvirt:
	{
		if (((u32) src_addr >= \
			sysmemmgr_state.cfg.static_phys_base_addr) && \
			((u32) src_addr < \
			(sysmemmgr_state.cfg.static_phys_base_addr + \
				sysmemmgr_state.cfg.static_mem_size))) {
			ret_ptr = (void *)(((u32) src_addr - \
				sysmemmgr_state.cfg.static_phys_base_addr) + \
				(sysmemmgr_state.cfg.static_virt_base_addr));
		} else {
			ret_ptr = platform_mem_translate(src_addr,
					PLATFORM_MEM_XLT_FLAGS_PHYS2VIRT);
		}
	}
	break;

	default:
	{
		printk(KERN_ALERT "sysmemmgr_translate: Unhandled translation "
			"flag\n");
	}
	break;
	}

	return ret_ptr;
}
