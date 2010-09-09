/*
 * processor.h
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

#ifndef SYSLINK_PROCESSOR_H_
#define SYSLINK_PROCESSOR_H_

#include <linux/types.h>

/* Module level headers */
#include "procdefs.h"

/* ===================================
 *  APIs
 * ===================================
 */
/* Function to attach to the Processor. */
int processor_attach(void *handle, struct processor_attach_params *params);

/* Function to detach from the Processor. */
int processor_detach(void *handle);

/* Function to read from the slave processor's memory. */
int processor_read(void *handle, u32 proc_addr, u32 *num_bytes, void *buffer);

/* Function to read write into the slave processor's memory. */
int processor_write(void *handle, u32 proc_addr, u32 *num_bytes, void *buffer);

/* Function to get the current state of the slave Processor as maintained on
 * the master Processor state machine.
 */
enum proc_mgr_state processor_get_state(void *handle);

/* Function to set the current state of the slave Processor to specified value.
 */
void processor_set_state(void *handle, enum proc_mgr_state state);

/* Function to perform device-dependent operations. */
int processor_control(void *handle, int cmd, void *arg);

/* Function to translate between two types of address spaces. */
int processor_translate_addr(void *handle, void **dst_addr,
	enum proc_mgr_addr_type dst_addr_type, void *src_addr,
	enum proc_mgr_addr_type src_addr_type);

/* Function that registers for notification when the slave processor
 * transitions to any of the states specified.
 */
int processor_register_notify(void *handle, proc_mgr_callback_fxn fxn,
		void *args, enum proc_mgr_state state[]);

/* Function that returns the return value of specific processor info
 */
int processor_get_proc_info(void *handle, struct proc_mgr_proc_info *procinfo);

int processor_virt_to_phys(void *handle, u32 da, u32 *mapped_entries,
						u32 num_of_entries);
#endif
