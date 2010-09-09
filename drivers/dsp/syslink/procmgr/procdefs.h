/*
 * procdefs.h
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

#ifndef SYSLINK_PROCDEFS_H
#define SYSLINK_PROCDEFS_H

#include <linux/types.h>

/* Module level headers */
#include <procmgr.h>


/* =============================
 *  Macros and types
 * =============================
 */
/*
 * Enumerates the types of Endianism of slave processor.
 */
enum processor_endian{
	PROCESSOR_ENDIAN_DEFAULT = 0,
	/* Default endianism (no conversion required) */
	PROCESSOR_ENDIAN_BIG = 1,
	/* Big endian */
	PROCESSOR_ENDIAN_LITTLE = 2,
	/* Little endian */
	PROCESSOR_ENDIAN_ENDVALUE = 3
	/* End delimiter indicating start of invalid values for this enum */
};


/*
 * Configuration parameters for attaching to the slave Processor
 */
struct processor_attach_params {
	struct proc_mgr_attach_params *params;
	/* Common attach parameters for ProcMgr */
	u16 num_mem_entries;
	/* Number of valid memory entries */
	struct proc_mgr_addr_info mem_entries[PROCMGR_MAX_MEMORY_REGIONS];
	/* Configuration of memory regions */
};

/*
 * Function pointer type for the function to attach to the processor.
 */
typedef int (*processor_attach_fxn) (void *handle,
				struct processor_attach_params *params);

/*
 * Function pointer type for the function to detach from the
 * procssor
 */
typedef int (*processor_detach_fxn) (void *handle);

/*
 * Function pointer type for the function to read from the slave
 *		processor's memory.
 */
typedef int (*processor_read_fxn) (void *handle, u32 proc_addr,
				u32 *num_bytes, void *buffer);

/*
 *Function pointer type for the function to write into the slave
 *processor's memory.
 */
typedef int (*processor_write_fxn) (void *handle, u32 proc_addr,
				u32 *num_bytes, void *buffer);

/*
 *Function pointer type for the function to perform device-dependent
 *		operations.
 */
typedef int (*processor_control_fxn) (void *handle, int cmd, void *arg);

/*
 *Function pointer type for the function to translate between
 *		two types of address spaces.
 */
typedef int (*processor_translate_addr_fxn) (void *handle, void **dst_addr,
		enum proc_mgr_addr_type dstAddrType, void *srcAddr,
		enum proc_mgr_addr_type srcAddrType);

/*
 *Function pointer type for the function that returns proc info
 */
typedef int (*processor_proc_info) (void *handle,
				struct  proc_mgr_proc_info *proc_info);

/*
 *Function pointer type for the function that returns proc info
 */
typedef int (*processor_virt_to_phys_fxn) (void *handle, u32 da,
			u32 *mapped_entries, u32 num_of_entries);


/* =============================
 *  Function table interface
 * =============================
 */
/*
 *Function table interface for Processor.
 */
struct processor_fxn_table {
	processor_attach_fxn attach;
	/* Function to attach to the slave processor */
	processor_detach_fxn detach;
	/* Function to detach from the slave processor */
	processor_read_fxn read;
	/* Function to read from the slave processor's memory */
	processor_write_fxn write;
	/* Function to write into the slave processor's memory */
	processor_control_fxn  control;
	/* Function to perform device-dependent control function */
	processor_translate_addr_fxn translateAddr;
	/* Function to translate between address ranges */
	processor_proc_info procinfo;
	/* Function to convert Virtual to Physical pages */
	processor_virt_to_phys_fxn virt_to_phys;
};

/* =============================
 * Processor structure
 * =============================
 */
/*
 *  Generic Processor object. This object defines the handle type for all
 *  Processor operations.
 */
struct processor_object {
	struct processor_fxn_table proc_fxn_table;
	/* interface function table to plug into the generic Processor. */
	enum proc_mgr_state state;
	/* State of the slave processor */
	enum proc_mgr_boot_mode boot_mode;
	/* Boot mode for the slave processor. */
	void *object;
	/* Pointer to Processor-specific object. */
	u16 proc_id;
	/* Processor ID addressed by this Processor instance. */
};
#endif
