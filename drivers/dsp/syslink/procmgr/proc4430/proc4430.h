/*
 * proc4430.h
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




#ifndef _SYSLINK_PROC_4430_H_
#define _SYSLINK_PROC_4430_H_


/* Module headers */
#include <procmgr.h>
#include "../procdefs.h"
#include <linux/types.h>

/*
  Module configuration structure.
 */
struct proc4430_config {
	struct mutex *gate_handle;
	/* void * of gate to be used for local thread safety */
};

/*
  Memory entry for slave memory map configuration
 */
struct proc4430_mem_entry {
	char name[PROCMGR_MAX_STRLEN];
	/* Name identifying the memory region. */
	u32 phys_addr;
	/* Physical address of the memory region. */
	u32 slave_virt_addr;
	/* Slave virtual address of the memory region. */
	u32 master_virt_addr;
	/* Master virtual address of the memory region. If specified as -1,
	* the master virtual address is assumed to be invalid, and shall be
	* set internally within the Processor module. */
	u32 size;
	/* Size (in bytes) of the memory region. */
	bool shared;
	/* Flag indicating whether the memory region is shared between master
	* and slave. */
};

/*
  Configuration parameters specific to this processor.
 */
struct proc4430_params {
	int num_mem_entries;
	/* Number of memory regions to be configured. */
	struct proc4430_mem_entry *mem_entries;
	/* Array of information structures for memory regions
	* to be configured. */
	u32 reset_vector_mem_entry;
	/* Index of the memory entry within the mem_entries array,
	* which is the resetVector memory region. */
};


/* Function to initialize the slave processor */
int proc4430_attach(void *handle, struct processor_attach_params *params);

/* Function to finalize the slave processor */
int proc4430_detach(void *handle);


/* Function to read from the slave processor's memory. */
int proc4430_read(void *handle, u32 proc_addr, u32 *num_bytes,
						void *buffer);

/* Function to write into the slave processor's memory. */
int proc4430_write(void *handle, u32 proc_addr, u32 *num_bytes,
					void *buffer);

/* Function to perform device-dependent operations. */
int proc4430_control(void *handle, int cmd, void *arg);

/* Function to translate between two types of address spaces. */
int proc4430_translate_addr(void *handle, void **dst_addr,
			enum proc_mgr_addr_type dst_addr_type,
			void *src_addr, enum proc_mgr_addr_type src_addr_type);

/* Function to retrive physical address translations */
int proc4430_virt_to_phys(void *handle, u32 da, u32 *mapped_entries,
					u32 num_of_entries);

/* =================================================
 *  APIs
 * =================================================
 */

/* Function to get the default configuration for the OMAP4430PROC module */
void proc4430_get_config(struct proc4430_config *cfg);

/* Function to setup the OMAP4430PROC module. */
int proc4430_setup(struct proc4430_config *cfg);

/* Function to destroy the OMAP4430PROC module. */
int proc4430_destroy(void);

/* Function to initialize the parameters for this processor instance. */
void proc4430_params_init(void *handle,
			 struct proc4430_params *params);

/* Function to create an instance of this processor. */
void *proc4430_create(u16 proc_id, const struct proc4430_params *params);

/* Function to delete an instance of this processor. */
int proc4430_delete(void **handle_ptr);

/* Function to open an instance of this processor. */
int proc4430_open(void **handle_ptr, u16 proc_id);

/* Function to close an instance of this processor. */
int proc4430_close(void *handle);

/* Function to get the proc info */
int proc4430_proc_info(void *handle, struct proc_mgr_proc_info *procinfo);

#endif
