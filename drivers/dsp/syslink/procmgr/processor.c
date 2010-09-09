/*
 * processor.c
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

/* Module level headers */
#include "procdefs.h"
#include "processor.h"



/* =========================================
 * Functions called by ProcMgr
 * =========================================
 */
/*
 * Function to attach to the Processor.
 *
 * This function calls into the specific Processor implementation
 * to attach to it.
 * This function is called from the ProcMgr attach function, and
 * hence is used to perform any activities that may be required
 * once the slave is powered up.
 * Depending on the type of Processor, this function may or may not
 * perform any activities.
 */
inline int processor_attach(void *handle,
				struct processor_attach_params *params)
{
	int retval = 0;
	struct processor_object *proc_handle =
				(struct processor_object *)handle;

	BUG_ON(handle == NULL);
	BUG_ON(params == NULL);
	BUG_ON(proc_handle->proc_fxn_table.attach == NULL);

	proc_handle->boot_mode = params->params->boot_mode;
	retval = proc_handle->proc_fxn_table.attach(handle, params);

	if (proc_handle->boot_mode == PROC_MGR_BOOTMODE_BOOT)
		proc_handle->state = PROC_MGR_STATE_POWERED;
	else if (proc_handle->boot_mode == PROC_MGR_BOOTMODE_NOLOAD)
		proc_handle->state = PROC_MGR_STATE_LOADED;
	else if (proc_handle->boot_mode == PROC_MGR_BOOTMODE_NOBOOT)
		proc_handle->state = PROC_MGR_STATE_RUNNNING;
	return retval;
}


/*
 * Function to detach from the Processor.
 *
 * This function calls into the specific Processor implementation
 * to detach from it.
 * This function is called from the ProcMgr detach function, and
 * hence is useful to perform any activities that may be required
 * before the slave is powered down.
 * Depending on the type of Processor, this function may or may not
 * perform any activities.
 */
inline int processor_detach(void *handle)
{
	int retval = 0;
	struct processor_object *proc_handle =
					(struct processor_object *)handle;

	BUG_ON(handle == NULL);
	BUG_ON(proc_handle->proc_fxn_table.detach == NULL);

	retval = proc_handle->proc_fxn_table.detach(handle);
	/* For all boot modes, at the end of detach, the Processor is in
	* unknown state.
	*/
	proc_handle->state = PROC_MGR_STATE_UNKNOWN;
	return retval;
}


/*
 * Function to read from the slave processor's memory.
 *
 * This function calls into the specific Processor implementation
 * to read from the slave processor's memory. It reads from the
 * specified address in the processor's address space and copies
 * the required number of bytes into the specified buffer.
 * It returns the number of bytes actually read in the num_bytes
 * parameter.
 * Depending on the processor implementation, it may result in
 * reading from shared memory or across a peripheral physical
 * connectivity.
 * The handle specifies the specific Processor instance to be used.
 */
inline int processor_read(void *handle, u32 proc_addr,
			u32 *num_bytes, void *buffer)
{
	int retval = 0;
	struct processor_object *proc_handle =
				(struct processor_object *)handle;

	BUG_ON(handle == NULL);
	BUG_ON(proc_addr == 0);
	BUG_ON(num_bytes == 0);
	BUG_ON(buffer == NULL);
	BUG_ON(proc_handle->proc_fxn_table.read == NULL);

	retval = proc_handle->proc_fxn_table.read(handle, proc_addr,
						num_bytes, buffer);
	return retval;
}


/*
 * Function to write into the slave processor's memory.
 *
 * This function calls into the specific Processor implementation
 * to write into the slave processor's memory. It writes into the
 * specified address in the processor's address space and copies
 * the required number of bytes from the specified buffer.
 * It returns the number of bytes actually written in the num_bytes
 * parameter.
 * Depending on the processor implementation, it may result in
 * writing into shared memory or across a peripheral physical
 * connectivity.
 * The handle specifies the specific Processor instance to be used.
 */
inline int processor_write(void *handle, u32 proc_addr, u32 *num_bytes,
							void *buffer)
{
	int retval = 0;
	struct processor_object *proc_handle =
				(struct processor_object *)handle;
	BUG_ON(handle == NULL);
	BUG_ON(proc_addr == 0);
	BUG_ON(num_bytes == 0);
	BUG_ON(buffer == NULL);
	BUG_ON(proc_handle->proc_fxn_table.write == NULL);

	retval = proc_handle->proc_fxn_table.write(handle, proc_addr,
						num_bytes, buffer);
	return retval;
}


/*
 * Function to get the current state of the slave Processor.
 *
 * This function gets the state of the slave processor as
 * maintained on the master Processor state machine. It does not
 * go to the slave processor to get its actual state at the time
 * when this API is called.
 */
enum proc_mgr_state processor_get_state(void *handle)
{
	struct processor_object *proc_handle =
				(struct processor_object *)handle;

	BUG_ON(handle == NULL);

	return proc_handle->state;
}


/*
 * Function to set the current state of the slave Processor
 * to specified value.
 *
 * This function is used to set the state of the processor to the
 * value as specified. This function may be used by external
 * entities that affect the state of the slave processor, such as
 * PwrMgr, error handler, or ProcMgr.
 */
void processor_set_state(void *handle, enum proc_mgr_state state)
{
	struct processor_object *proc_handle =
				(struct processor_object *)handle;

	BUG_ON(handle == NULL);
	proc_handle->state = state;
}


/*
 * Function to perform device-dependent operations.
 *
 * This function calls into the specific Processor implementation
 * to perform device dependent control operations. The control
 * operations supported by the device are exposed directly by the
 * specific implementation of the Processor interface. These
 * commands and their specific argument types are used with this
 * function.
 */
inline int processor_control(void *handle, int cmd, void *arg)
{
	int retval = 0;
	struct processor_object *proc_handle =
				(struct processor_object *)handle;

	BUG_ON(handle == NULL);
	BUG_ON(proc_handle->proc_fxn_table.control == NULL);

	retval = proc_handle->proc_fxn_table.control(handle, cmd, arg);
	return retval;
}


/*
 * Function to translate between two types of address spaces.
 *
 * This function translates addresses between two types of address
 * spaces. The destination and source address types are indicated
 * through parameters specified in this function.
 */
inline int processor_translate_addr(void *handle, void **dst_addr,
		enum proc_mgr_addr_type dst_addr_type, void *src_addr,
		enum proc_mgr_addr_type src_addr_type)
{
	int retval = 0;
	struct processor_object *proc_handle =
					(struct processor_object *)handle;

	BUG_ON(handle == NULL);
	BUG_ON(dst_addr == NULL);
	BUG_ON(src_addr == NULL);
	BUG_ON(dst_addr_type >= PROC_MGR_ADDRTYPE_ENDVALUE);
	BUG_ON(src_addr_type >= PROC_MGR_ADDRTYPE_ENDVALUE);
	BUG_ON(proc_handle->proc_fxn_table.translateAddr == NULL);

	retval = proc_handle->proc_fxn_table.translateAddr(handle,
			dst_addr, dst_addr_type, src_addr, src_addr_type);
	return retval;
}


/*
 * Function that registers for notification when the slave
 * processor transitions to any of the states specified.
 *
 * This function allows the user application to register for
 * changes in processor state and take actions accordingly.

 */
inline int processor_register_notify(void *handle, proc_mgr_callback_fxn fxn,
				void *args, enum proc_mgr_state state[])
{
	int retval = 0;

	BUG_ON(handle == NULL);
	BUG_ON(fxn == NULL);

	/* TODO: TBD: To be implemented. */
	return retval;
}

/*
 * Function that returns the proc instance mem info
 */
int processor_get_proc_info(void *handle, struct proc_mgr_proc_info *procinfo)
{
	struct processor_object *proc_handle =
				(struct processor_object *)handle;
	int retval;
	retval = proc_handle->proc_fxn_table.procinfo(proc_handle, procinfo);
	return retval;
}

/*
 * Function that returns the address translations
 */
int processor_virt_to_phys(void *handle, u32 da, u32 *mapped_entries,
						u32 num_of_entries)
{
	struct processor_object *proc_handle =
				(struct processor_object *)handle;
	int retval;
	retval = proc_handle->proc_fxn_table.virt_to_phys(handle, da,
				mapped_entries, num_of_entries);
	return retval;
}
