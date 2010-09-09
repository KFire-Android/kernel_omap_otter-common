/*
 * procmgr.c
 *
 * Syslink driver support functions for TI OMAP processors.
 *
 * Copyright (C) 2009-2010 Texas Instruments, Inc.
 *
 * This package is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * THIS PACKAGE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#include <linux/types.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/vmalloc.h>
#include <linux/slab.h>
#include <asm/atomic.h>

/* Module level headers */
#include <procmgr.h>
#include "procdefs.h"
#include "processor.h"
#include <syslink/atomic_linux.h>

/* ================================
 *  Macros and types
 * ================================
 */
/*! @brief Macro to make a correct module magic number with refCount */
#define PROCMGR_MAKE_MAGICSTAMP(x) ((PROCMGR_MODULEID << 12u) | (x))

/*
 *  ProcMgr Module state object
 */
struct proc_mgr_module_object {
	atomic_t ref_count;
	u32 config_size;
	/* Size of configuration structure */
	struct proc_mgr_config cfg;
	/* ProcMgr configuration structure */
	struct proc_mgr_config def_cfg;
	/* Default module configuration */
	struct proc_mgr_params def_inst_params;
	/* Default parameters for the ProcMgr instances */
	struct proc_mgr_attach_params def_attach_params;
	/* Default parameters for the ProcMgr attach function */
	struct mutex *gate_handle;
	/* handle of gate to be used for local thread safety */
	void *proc_handles[MULTIPROC_MAXPROCESSORS];
	/* Array of handles of ProcMgr instances */
};

/*
 * ProcMgr instance object
 */
struct proc_mgr_object {
	u16 proc_id;
	/* Processor ID associated with this ProcMgr. */
	struct processor_object *proc_handle;
	/* Processor ID of the processor being represented by this instance. */
	void *loader_handle;
	/*!< Handle to the Loader object associated with this ProcMgr. */
	void *pwr_handle;
	/*!< Handle to the PwrMgr object associated with this ProcMgr. */
	/*!< Processor ID of the processor being represented by this instance */
	struct proc_mgr_params params;
	/* ProcMgr instance params structure */
	struct proc_mgr_attach_params attach_params;
	/* ProcMgr attach params structure */
	u32 file_id;
	/*!< File ID of the loaded static executable */
	u16 num_mem_entries;
	/* Number of valid memory entries */
	struct proc_mgr_addr_info mem_entries[PROCMGR_MAX_MEMORY_REGIONS];
	/* Configuration of memory regions */
};

static struct proc_mgr_module_object proc_mgr_obj_state = {
	.config_size = sizeof(struct proc_mgr_config),
	.def_cfg.gate_handle = NULL,
	.gate_handle = NULL,
	.def_inst_params.proc_handle   = NULL,
	.def_attach_params.boot_mode = PROC_MGR_BOOTMODE_BOOT,
};


/*======================================
 * Function to get the default configuration for the ProcMgr
 * module.
 *
* This function can be called by the application to get their
* configuration parameter to ProcMgr_setup filled in by the
* ProcMgr module with the default parameters. If the user does
* not wish to make any change in the default parameters, this API
* is not required to be called.
 */
void proc_mgr_get_config(struct proc_mgr_config *cfg)
{
	BUG_ON(cfg == NULL);
	memcpy(cfg, &proc_mgr_obj_state.def_cfg,
			sizeof(struct proc_mgr_config));
	return;
}
EXPORT_SYMBOL(proc_mgr_get_config);

/*
 * Function to setup the ProcMgr module.
 *
 *This function sets up the ProcMgr module. This function must
 *be called before any other instance-level APIs can be invoked.
 *Module-level configuration needs to be provided to this
 *function. If the user wishes to change some specific config
 *parameters, then ProcMgr_getConfig can be called to get the
 *configuration filled with the default values. After this, only
 *the required configuration values can be changed. If the user
 *does not wish to make any change in the default parameters, the
 *application can simply call ProcMgr_setup with NULL parameters.
 *The default parameters would get automatically used.
 */
int proc_mgr_setup(struct proc_mgr_config *cfg)
{
	int retval = 0;
	struct proc_mgr_config tmp_cfg;

	/* This sets the refCount variable is not initialized, upper 16 bits is
	* written with module Id to ensure correctness of refCount variable.
	*/
	atomic_cmpmask_and_set(&proc_mgr_obj_state.ref_count,
		PROCMGR_MAKE_MAGICSTAMP(0), PROCMGR_MAKE_MAGICSTAMP(0));

	if (atomic_inc_return(&proc_mgr_obj_state.ref_count)
		!= PROCMGR_MAKE_MAGICSTAMP(1u))
		return 0;
	if (cfg == NULL) {
		proc_mgr_get_config(&tmp_cfg);
		cfg = &tmp_cfg;
	}
	if (cfg->gate_handle != NULL) {
		proc_mgr_obj_state.gate_handle = cfg->gate_handle;
	} else {
		/* User has not provided any gate handle, so create a
		*default handle.
		*/
		proc_mgr_obj_state.gate_handle = kmalloc(sizeof(struct mutex),
						GFP_KERNEL);
		mutex_init(proc_mgr_obj_state.gate_handle);
	}
	memcpy(&proc_mgr_obj_state.cfg, cfg, sizeof(struct proc_mgr_config));
	/* Initialize the procHandles array. */
	memset(&proc_mgr_obj_state.proc_handles, 0,
		(sizeof(void *) * MULTIPROC_MAXPROCESSORS));
	return retval;
}
EXPORT_SYMBOL(proc_mgr_setup);

/*==================================
 *  Function to destroy the ProcMgr module.
 *
 * Once this function is called, other ProcMgr module APIs, except
 * for the proc_mgr_get_config API cannot be called anymore.
 */
int proc_mgr_destroy(void)
{
	int retval = 0;
	int i;

	if (atomic_cmpmask_and_lt(&(proc_mgr_obj_state.ref_count),
		PROCMGR_MAKE_MAGICSTAMP(0), PROCMGR_MAKE_MAGICSTAMP(1))
		 == true) {
		printk(KERN_ERR "proc_mgr_destroy: Error - module not initialized\n");
		return -EFAULT;
	}
	if (atomic_dec_return(&proc_mgr_obj_state.ref_count)
		 == PROCMGR_MAKE_MAGICSTAMP(0)) {

		/* Check if any ProcMgr instances have not been deleted so far
		*. If not,delete them
		*/
		for (i = 0 ; i < MULTIPROC_MAXPROCESSORS; i++) {
			if (proc_mgr_obj_state.proc_handles[i] != NULL)
				proc_mgr_delete
				(&(proc_mgr_obj_state.proc_handles[i]));
		}

		mutex_destroy(proc_mgr_obj_state.gate_handle);
		kfree(proc_mgr_obj_state.gate_handle);
		/* Decrease the refCount */
		atomic_set(&proc_mgr_obj_state.ref_count,
			PROCMGR_MAKE_MAGICSTAMP(0));
	}
	return retval;;
}
EXPORT_SYMBOL(proc_mgr_destroy);

/*=====================================
 *  Function to initialize the parameters for the ProcMgr instance.
 *
 * This function can be called by the application to get their
 * configuration parameter to ProcMgr_create filled in by the
 * ProcMgr module with the default parameters.
 */
void proc_mgr_params_init(void *handle, struct proc_mgr_params *params)
{
	struct proc_mgr_object *proc_handle = (struct proc_mgr_object *)handle;

	if (WARN_ON(params == NULL))
		goto exit;
	if (atomic_cmpmask_and_lt(&(proc_mgr_obj_state.ref_count),
		PROCMGR_MAKE_MAGICSTAMP(0), PROCMGR_MAKE_MAGICSTAMP(1))
		 == true) {
		printk(KERN_ERR "proc_mgr_params_init: Error - module not initialized\n");
	}
	if (handle == NULL) {
		memcpy(params, &(proc_mgr_obj_state.def_inst_params),
				sizeof(struct proc_mgr_params));
	} else {
		/* Return updated ProcMgr instance specific parameters. */
		memcpy(params, &(proc_handle->params),
				sizeof(struct proc_mgr_params));
	}
exit:
	return;
}
EXPORT_SYMBOL(proc_mgr_params_init);

/*=====================================
 *  Function to create a ProcMgr object for a specific slave
 * processor.
 *
 * This function creates an instance of the ProcMgr module and
 * returns an instance handle, which is used to access the
 * specified slave processor. The processor ID specified here is
 * the ID of the slave processor as configured with the MultiProc
 * module.
 * Instance-level configuration needs to be provided to this
 * function. If the user wishes to change some specific config
 * parameters, then struct proc_mgr_params_init can be called to get the
 * configuration filled with the default values. After this, only
 * the required configuration values can be changed. For this
 * API, the params argument is not optional, since the user needs
 * to provide some essential values such as loader, PwrMgr and
 * Processor instances to be used with this ProcMgr instance.
 */
void *proc_mgr_create(u16 proc_id, const struct proc_mgr_params *params)
{
	struct proc_mgr_object *handle = NULL;

	BUG_ON(!IS_VALID_PROCID(proc_id));
	BUG_ON(params == NULL);
	BUG_ON(params->proc_handle == NULL);
	if (atomic_cmpmask_and_lt(&(proc_mgr_obj_state.ref_count),
		PROCMGR_MAKE_MAGICSTAMP(0), PROCMGR_MAKE_MAGICSTAMP(1))
		 == true) {
		printk(KERN_ERR "proc_mgr_create: Error - module not initialized\n");
		return NULL;
	}
	if (proc_mgr_obj_state.proc_handles[proc_id] != NULL) {
		handle = proc_mgr_obj_state.proc_handles[proc_id];
		printk(KERN_WARNING "proc_mgr_create:"
			"Processor already exists for specified"
			"%d  proc_id, handle = 0x%x\n", proc_id, (u32)handle);
		return handle;
	}
	WARN_ON(mutex_lock_interruptible(proc_mgr_obj_state.gate_handle));
	handle = (struct proc_mgr_object *)
				vmalloc(sizeof(struct proc_mgr_object));
	BUG_ON(handle == NULL);
	memset(handle, 0, sizeof(struct proc_mgr_object));
	memcpy(&(handle->params), params, sizeof(struct proc_mgr_params));
	handle->proc_id = proc_id;
	handle->proc_handle = params->proc_handle;
	handle->loader_handle = params->loader_handle;
	handle->pwr_handle = params->pwr_handle;
	proc_mgr_obj_state.proc_handles[proc_id] = handle;
	mutex_unlock(proc_mgr_obj_state.gate_handle);
	return handle;
}
EXPORT_SYMBOL(proc_mgr_create);

/*===================================
 *  Function to delete a ProcMgr object for a specific slave
 * processor.
 *
 * Once this function is called, other ProcMgr instance level APIs
 * that require the instance handle cannot be called.
 *
 */
int
proc_mgr_delete(void **handle_ptr)
{
	int retval = 0;
	struct proc_mgr_object *handle;

	BUG_ON(handle_ptr == NULL);
	if (atomic_cmpmask_and_lt(&(proc_mgr_obj_state.ref_count),
		PROCMGR_MAKE_MAGICSTAMP(0), PROCMGR_MAKE_MAGICSTAMP(1))
		 == true) {
		printk(KERN_ERR "proc_mgr_delete: Error - module not initialized\n");
		return -EFAULT;
	}

	handle = (struct proc_mgr_object *)(*handle_ptr);
	WARN_ON(mutex_lock_interruptible(proc_mgr_obj_state.gate_handle));
	proc_mgr_obj_state.proc_handles[handle->proc_id] = NULL;
	vfree(handle);
	*handle_ptr = NULL;
	mutex_unlock(proc_mgr_obj_state.gate_handle);
	return retval;
}
EXPORT_SYMBOL(proc_mgr_delete);

/*======================================
 *  Function to open a handle to an existing ProcMgr object handling
 * the proc_id.
 *
 * This function returns a handle to an existing ProcMgr instance
 * created for this proc_id. It enables other entities to access
 * and use this ProcMgr instance.
 */
int proc_mgr_open(void **handle_ptr, u16 proc_id)
{
	int retval = 0;

	BUG_ON(handle_ptr == NULL);
	BUG_ON(!IS_VALID_PROCID(proc_id));
	if (atomic_cmpmask_and_lt(&(proc_mgr_obj_state.ref_count),
		PROCMGR_MAKE_MAGICSTAMP(0), PROCMGR_MAKE_MAGICSTAMP(1))
		 == true) {
		printk(KERN_ERR "proc_mgr_open: Error - module not initialized\n");
		return -EFAULT;
	}

	WARN_ON(mutex_lock_interruptible(proc_mgr_obj_state.gate_handle));
	*handle_ptr = proc_mgr_obj_state.proc_handles[proc_id];
	mutex_unlock(proc_mgr_obj_state.gate_handle);
	return retval;
}
EXPORT_SYMBOL(proc_mgr_open);

/*=====================================
 *  Function to close this handle to the ProcMgr instance.
 *
 * This function closes the handle to the ProcMgr instance
 * obtained through proc_mgr_open call made earlier.
 */
int proc_mgr_close(void *handle)
{
	int retval = 0;

	BUG_ON(handle == NULL);
	if (atomic_cmpmask_and_lt(&(proc_mgr_obj_state.ref_count),
		PROCMGR_MAKE_MAGICSTAMP(0), PROCMGR_MAKE_MAGICSTAMP(1))
		 == true) {
		printk(KERN_ERR "proc_mgr_close: Error - module not initialized\n");
		return -EFAULT;
	}
	/* Nothing to be done for closing the handle. */
	return retval;
}
EXPORT_SYMBOL(proc_mgr_close);

/*========================================
 *  Function to initialize the parameters for the ProcMgr attach
 * function.
 *
 * This function can be called by the application to get their
 * configuration parameter to proc_mgr_attach filled in by the
 * ProcMgr module with the default parameters. If the user does
 * not wish to make any change in the default parameters, this API
 * is not required to be called.
 */
void proc_mgr_get_attach_params(void *handle,
				struct proc_mgr_attach_params *params)
{
	struct proc_mgr_object *proc_mgr_handle =
				(struct proc_mgr_object *)handle;
	BUG_ON(params == NULL);
	if (atomic_cmpmask_and_lt(&(proc_mgr_obj_state.ref_count),
		PROCMGR_MAKE_MAGICSTAMP(0), PROCMGR_MAKE_MAGICSTAMP(1))
		 == true) {
		printk(KERN_ERR "proc_mgr_get_attach_params:"
			"Error - module not initialized\n");
	}
	if (handle == NULL) {
		memcpy(params, &(proc_mgr_obj_state.def_attach_params),
				sizeof(struct proc_mgr_attach_params));
	} else {
		/* Return updated ProcMgr instance specific parameters. */
		memcpy(params, &(proc_mgr_handle->attach_params),
				sizeof(struct proc_mgr_attach_params));
	}
	return;
}
EXPORT_SYMBOL(proc_mgr_get_attach_params);

/*
 *  Function to attach the client to the specified slave and also
 * initialize the slave (if required).
 *
 * This function attaches to an instance of the ProcMgr module and
 * performs any hardware initialization required to power up the
 * slave device. This function also performs the required state
 * transitions for this ProcMgr instance to ensure that the local
 * object representing the slave device correctly indicates the
 * state of the slave device. Depending on the slave boot mode
 * being used, the slave may be powered up, in reset, or even
 * running state.
 * Configuration parameters need to be provided to this
 * function. If the user wishes to change some specific config
 * parameters, then proc_mgr_get_attach_params can be called to get
 * the configuration filled with the default values. After this,
 * only the required configuration values can be changed. If the
 * user does not wish to make any change in the default parameters,
 * the application can simply call proc_mgr_attach with NULL
 * parameters.
 * The default parameters would get automatically used.
 */
int proc_mgr_attach(void *handle, struct proc_mgr_attach_params *params)
{
	int retval = 0;
	struct proc_mgr_object *proc_mgr_handle =
				(struct proc_mgr_object *)handle;
	struct proc_mgr_attach_params tmp_params;
	struct processor_attach_params proc_attach_params;

	if (params == NULL) {
		proc_mgr_get_attach_params(handle, &tmp_params);
		params = &tmp_params;
	}
	if (atomic_cmpmask_and_lt(&(proc_mgr_obj_state.ref_count),
		PROCMGR_MAKE_MAGICSTAMP(0), PROCMGR_MAKE_MAGICSTAMP(1))
		 == true) {
		printk(KERN_ERR "proc_mgr_attach:"
			"Error - module not initialized\n");
		return -EFAULT;
	}
	if (WARN_ON(handle == NULL)) {
		retval = -EFAULT;
		goto exit;
	}
	if (WARN_ON(params->boot_mode == PROC_MGR_BOOTMODE_ENDVALUE)) {
		retval = -EINVAL;
		goto exit;
	}
	WARN_ON(mutex_lock_interruptible(proc_mgr_obj_state.gate_handle));
	/* Copy the user provided values into the instance object. */
	memcpy(&(proc_mgr_handle->attach_params), params,
				sizeof(struct proc_mgr_attach_params));
	proc_attach_params.params = params;
	proc_attach_params.num_mem_entries = 0;
	/* Attach to the specified Processor instance. */
	retval = processor_attach(proc_mgr_handle->proc_handle,
					&proc_attach_params);
	proc_mgr_handle->num_mem_entries = proc_attach_params.num_mem_entries;
	printk(KERN_INFO "proc_mgr_attach:proc_mgr_handle->num_mem_entries = %d\n",
			proc_mgr_handle->num_mem_entries);
	/* Store memory information in local object.*/
	memcpy(&(proc_mgr_handle->mem_entries),
		&(proc_attach_params.mem_entries),
		sizeof(proc_mgr_handle->mem_entries));
	mutex_unlock(proc_mgr_obj_state.gate_handle);
exit:
	return retval;
}
EXPORT_SYMBOL(proc_mgr_attach);

/*===================================
 *  Function to detach the client from the specified slave and also
 * finalze the slave (if required).
 *
 * This function detaches from an instance of the ProcMgr module
 * and performs any hardware finalization required to power down
 * the slave device. This function also performs the required state
 * transitions for this ProcMgr instance to ensure that the local
 * object representing the slave device correctly indicates the
 * state of the slave device. Depending on the slave boot mode
 * being used, the slave may be powered down, in reset, or left in
 * its original state.
*/
int proc_mgr_detach(void *handle)
{
	int retval = 0;
	struct proc_mgr_object *proc_mgr_handle =
				(struct proc_mgr_object *)handle;
	if (atomic_cmpmask_and_lt(&(proc_mgr_obj_state.ref_count),
		PROCMGR_MAKE_MAGICSTAMP(0), PROCMGR_MAKE_MAGICSTAMP(1))
		 == true) {
		printk(KERN_ERR "proc_mgr_detach:"
			"Error - module not initialized\n");
		return -EFAULT;
	}
	BUG_ON(handle == NULL);
	WARN_ON(mutex_lock_interruptible(proc_mgr_obj_state.gate_handle));
	/* Detach from the Processor. */
	retval = processor_detach(proc_mgr_handle->proc_handle);
	mutex_unlock(proc_mgr_obj_state.gate_handle);
	return retval;
}
EXPORT_SYMBOL(proc_mgr_detach);

/*===================================
 *  Function to get the current state of the slave Processor.
 *
 * This function gets the state of the slave processor as
 * maintained on the master Processor state machine. It does not
 * go to the slave processor to get its actual state at the time
 * when this API is called.
 *
 */
enum proc_mgr_state proc_mgr_get_state(void *handle)
{
	struct proc_mgr_object *proc_mgr_handle =
				(struct proc_mgr_object *)handle;
	enum proc_mgr_state state = PROC_MGR_STATE_UNKNOWN;
	if (atomic_cmpmask_and_lt(&(proc_mgr_obj_state.ref_count),
		PROCMGR_MAKE_MAGICSTAMP(0), PROCMGR_MAKE_MAGICSTAMP(1))
		 == true) {
		printk(KERN_ERR "proc_mgr_get_state:"
			"Error - module not initialized\n");
		return -EFAULT;
	}
	BUG_ON(handle == NULL);

	WARN_ON(mutex_lock_interruptible(proc_mgr_obj_state.gate_handle));
	state = processor_get_state(proc_mgr_handle->proc_handle);
	mutex_unlock(proc_mgr_obj_state.gate_handle);
	return state;
}
EXPORT_SYMBOL(proc_mgr_get_state);

/*==================================================
 *  Function to read from the slave processor's memory.
 *
 * This function reads from the specified address in the
 * processor's address space and copies the required number of
 * bytes into the specified buffer.
 * It returns the number of bytes actually read in thenum_bytes
 * parameter.
 */
int proc_mgr_read(void *handle, u32 proc_addr, u32 *num_bytes, void *buffer)
{
	int retval = 0;
	struct proc_mgr_object *proc_mgr_handle =
					(struct proc_mgr_object *)handle;
	void *addr;

	if (atomic_cmpmask_and_lt(&(proc_mgr_obj_state.ref_count),
		PROCMGR_MAKE_MAGICSTAMP(0), PROCMGR_MAKE_MAGICSTAMP(1))
		 == true) {
		printk(KERN_ERR "proc_mgr_read:"
			"Error - module not initialized\n");
		return -EFAULT;
	}
	BUG_ON(handle == NULL);
	BUG_ON(proc_addr == 0);
	BUG_ON(num_bytes == NULL);
	BUG_ON(buffer == NULL);

	/* Check if the address is already mapped */
	retval = proc_mgr_translate_addr(handle,
					(void **) &addr,
					PROC_MGR_ADDRTYPE_MASTERKNLVIRT,
					(void *) proc_addr,
					PROC_MGR_ADDRTYPE_SLAVEVIRT);


	if (retval >= 0) {
		/* Enter critical section protection. */
		WARN_ON(mutex_lock_interruptible(
					proc_mgr_obj_state.gate_handle));

		retval = processor_read(proc_mgr_handle->proc_handle, (u32)addr,
							num_bytes, buffer);

		/* Leave critical section protection. */
		mutex_unlock(proc_mgr_obj_state.gate_handle);
	}

	WARN_ON(retval < 0);
	return retval;
}
EXPORT_SYMBOL(proc_mgr_read);

/*
 *  Function to write into the slave processor's memory.
 *
 * This function writes into the specified address in the
 * processor's address space and copies the required number of
 * bytes from the specified buffer.
 * It returns the number of bytes actually written in thenum_bytes
 * parameter.
 */
int proc_mgr_write(void *handle, u32 proc_addr, u32 *num_bytes, void *buffer)
{
	int retval = 0;
	struct proc_mgr_object *proc_mgr_handle =
					(struct proc_mgr_object *)handle;
	if (atomic_cmpmask_and_lt(&(proc_mgr_obj_state.ref_count),
		PROCMGR_MAKE_MAGICSTAMP(0), PROCMGR_MAKE_MAGICSTAMP(1))
		 == true) {
		printk(KERN_ERR "proc_mgr_write:"
			"Error - module not initialized\n");
		return -EFAULT;
	}
	BUG_ON(proc_addr == 0);
	BUG_ON(num_bytes == NULL);
	BUG_ON(buffer == NULL);

	WARN_ON(mutex_lock_interruptible(proc_mgr_obj_state.gate_handle));
	retval = processor_write(proc_mgr_handle->proc_handle, proc_addr,
							num_bytes, buffer);
	WARN_ON(retval < 0);
	mutex_unlock(proc_mgr_obj_state.gate_handle);
	return retval;;
}
EXPORT_SYMBOL(proc_mgr_write);


/*===================================
 *  Function to perform device-dependent operations.
 *
 * This function performs control operations supported by the
 * as exposed directly by the specific implementation of the
 * Processor interface. These commands and their specific argument
 * types are used with this function.
 */
int proc_mgr_control(void *handle, int cmd, void *arg)
{
	int retval = 0;
	struct proc_mgr_object *proc_mgr_handle
					= (struct proc_mgr_object *)handle;
	if (atomic_cmpmask_and_lt(&(proc_mgr_obj_state.ref_count),
		PROCMGR_MAKE_MAGICSTAMP(0), PROCMGR_MAKE_MAGICSTAMP(1))
		 == true) {
		printk(KERN_ERR "proc_mgr_control:"
			"Error - module not initialized\n");
		return -EFAULT;
	}
	BUG_ON(handle == NULL);
	WARN_ON(mutex_lock_interruptible(proc_mgr_obj_state.gate_handle));
	/* Perform device-dependent control operation. */
	retval = processor_control(proc_mgr_handle->proc_handle, cmd, arg);
	WARN_ON(retval < 0);
	mutex_unlock(proc_mgr_obj_state.gate_handle);
	return retval;;
}
EXPORT_SYMBOL(proc_mgr_control);

/*========================================
 *  Function to translate between two types of address spaces.
 *
 * This function translates addresses between two types of address
 * spaces. The destination and source address types are indicated
 * through parameters specified in this function.
 */
int proc_mgr_translate_addr(void *handle, void **dst_addr,
		enum proc_mgr_addr_type dst_addr_type, void *src_addr,
		enum proc_mgr_addr_type src_addr_type)
{
	int retval = 0;
	struct proc_mgr_object *proc_mgr_handle =
					(struct proc_mgr_object *)handle;

	if (atomic_cmpmask_and_lt(&(proc_mgr_obj_state.ref_count),
		PROCMGR_MAKE_MAGICSTAMP(0), PROCMGR_MAKE_MAGICSTAMP(1))
		 == true) {
		printk(KERN_ERR "proc_mgr_translate_addr:"
			"Error - module not initialized\n");
		return -EFAULT;
	}
	BUG_ON(dst_addr == NULL);
	BUG_ON(handle == NULL);
	BUG_ON(dst_addr_type > PROC_MGR_ADDRTYPE_ENDVALUE);
	BUG_ON(src_addr == NULL);
	BUG_ON(src_addr_type > PROC_MGR_ADDRTYPE_ENDVALUE);

	WARN_ON(mutex_lock_interruptible(proc_mgr_obj_state.gate_handle));
	/* Translate the address. */
	retval = processor_translate_addr(proc_mgr_handle->proc_handle,
		dst_addr, dst_addr_type, src_addr, src_addr_type);
	WARN_ON(retval < 0);
	mutex_unlock(proc_mgr_obj_state.gate_handle);
	return retval;;
}
EXPORT_SYMBOL(proc_mgr_translate_addr);

/*=================================
 *  Function that registers for notification when the slave
 * processor transitions to any of the states specified.
 *
 * This function allows the user application to register for
 * changes in processor state and take actions accordingly.
 *
 */
int proc_mgr_register_notify(void *handle, proc_mgr_callback_fxn fxn,
				void *args, enum proc_mgr_state state[])
{
	int retval = 0;
	struct proc_mgr_object *proc_mgr_handle
				= (struct proc_mgr_object *)handle;
	if (atomic_cmpmask_and_lt(&(proc_mgr_obj_state.ref_count),
		PROCMGR_MAKE_MAGICSTAMP(0), PROCMGR_MAKE_MAGICSTAMP(1))
		 == true) {
		printk(KERN_ERR "proc_mgr_register_notify:"
			"Error - module not initialized\n");
		return -EFAULT;
	}
	BUG_ON(handle == NULL);
	BUG_ON(fxn == NULL);
	WARN_ON(mutex_lock_interruptible(proc_mgr_obj_state.gate_handle));
	retval = processor_register_notify(proc_mgr_handle->proc_handle, fxn,
					args, state);
	WARN_ON(retval < 0);
	mutex_unlock(proc_mgr_obj_state.gate_handle);
	return retval;
}
EXPORT_SYMBOL(proc_mgr_register_notify);

/*
 *  Function that returns information about the characteristics of
 * the slave processor.
 */
int proc_mgr_get_proc_info(void *handle, struct proc_mgr_proc_info *proc_info)
{
	struct proc_mgr_object *proc_mgr_handle =
					(struct proc_mgr_object *)handle;
	struct processor_object *proc_handle;

	struct proc_mgr_proc_info proc_info_test;

	if (atomic_cmpmask_and_lt(&(proc_mgr_obj_state.ref_count),
		PROCMGR_MAKE_MAGICSTAMP(0), PROCMGR_MAKE_MAGICSTAMP(1))
		 == true) {
		printk(KERN_ERR "proc_mgr_get_proc_info:"
			"Error - module not initialized\n");
		return -EFAULT;
	}
	if (WARN_ON(handle == NULL))
		goto error_exit;
	if (WARN_ON(proc_info == NULL))
		goto error_exit;
	proc_handle = proc_mgr_handle->proc_handle;
	if (WARN_ON(proc_handle == NULL))
		goto error_exit;

	WARN_ON(mutex_lock_interruptible(proc_mgr_obj_state.gate_handle));

	processor_get_proc_info(proc_handle, &proc_info_test);
	/* Return bootMode information. */
	proc_info->boot_mode = proc_mgr_handle->attach_params.boot_mode;
	/* Return memory information. */
	proc_info->num_mem_entries = proc_mgr_handle->num_mem_entries;
	memcpy(&(proc_info->mem_entries),
			&(proc_mgr_handle->mem_entries),
			sizeof(proc_mgr_handle->mem_entries));
	mutex_unlock(proc_mgr_obj_state.gate_handle);
	return 0;
error_exit:
	return -EFAULT;
}
EXPORT_SYMBOL(proc_mgr_get_proc_info);

/*============================================
 *  Function to get virtual to physical address translations
 *
 * This function retrieves physical entries
 *
 */
int proc_mgr_virt_to_phys(void *handle, u32 da, u32 *mapped_entries,
						u32 num_of_entries)
{
	int retval = 0;
	struct proc_mgr_object *proc_mgr_handle =
				(struct proc_mgr_object *)handle;

	WARN_ON(mutex_lock_interruptible(proc_mgr_obj_state.gate_handle));

	/* Map to host address space. */
	retval = processor_virt_to_phys(proc_mgr_handle->proc_handle, da,
				mapped_entries, num_of_entries);
	WARN_ON(retval < 0);
	mutex_unlock(proc_mgr_obj_state.gate_handle);
	return retval;;
}
EXPORT_SYMBOL(proc_mgr_virt_to_phys);
