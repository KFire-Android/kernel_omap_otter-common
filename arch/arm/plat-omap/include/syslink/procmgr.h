/*
 * procmgr.h
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
#ifndef SYSLINK_PROC_MGR_H
#define SYSLINK_PROC_MGR_H

#include <linux/types.h>
#include <syslink/multiproc.h>



#define PROCMGR_MODULEID		0xf2ba

/*
 * Maximum name length for ProcMgr module strings.
 */
#define PROCMGR_MAX_STRLEN		32

/*
 * Maximum number of memory regions supported by ProcMgr module.
 */
#define PROCMGR_MAX_MEMORY_REGIONS	32

/*
 *	IS_VALID_PROCID
 *   Checks if the Processor ID is valid
 */
#define IS_VALID_PROCID(id)		(id < MULTIPROC_MAXPROCESSORS)


/*
 *   Enumerations to indicate Processor states.
 */
enum proc_mgr_state {
	PROC_MGR_STATE_UNKNOWN = 0,
	/* Unknown Processor state (e.g. at startup or error). */
	PROC_MGR_STATE_POWERED = 1,
	/* Indicates the Processor is powered up. */
	PROC_MGR_STATE_RESET = 2,
	/* Indicates the Processor is reset. */
	PROC_MGR_STATE_LOADED = 3,
	/* Indicates the Processor is loaded. */
	PROC_MGR_STATE_RUNNNING = 4,
	/* Indicates the Processor is running. */
	PROC_MGR_STATE_UNAVAILABLE = 5,
	/* Indicates the Processor is unavailable to the physical transport. */
	PROC_MGR_STATE_ENDVALUE = 6
	/* End delimiter indicating start of invalid values for this enum */
};

/*
 *   Enumerations to indicate different types of slave boot modes.
 */
enum proc_mgr_boot_mode {
	PROC_MGR_BOOTMODE_BOOT = 0,
	/* ProcMgr is responsible for loading the slave and its reset control */
	PROC_MGR_BOOTMODE_NOLOAD = 1,
	/* ProcMgr is not responsible for loading the slave. It is responsible
		 for reset control of the slave. */
	PROC_MGR_BOOTMODE_NOBOOT = 2,
	/* ProcMgr is not responsible for loading or reset control of the slave.
	 The slave either self-boots, or this is done by some entity outside of
	 the ProcMgr module. */
	PROC_MGR_BOOTMODE_ENDVALUE = 3
	/* End delimiter indicating start of invalid values for this enum */
} ;

/*
 *   Enumerations to indicate address types used for translation
 */
enum proc_mgr_addr_type{
	PROC_MGR_ADDRTYPE_MASTERKNLVIRT = 0,
	/* Kernel Virtual address on master processor */
	PROC_MGR_ADDRTYPE_MASTERUSRVIRT = 1,
	/* User Virtual address on master processor */
	PROC_MGR_ADDRTYPE_SLAVEVIRT = 2,
	/* Virtual address on slave processor */
	PROC_MGR_ADDRTYPE_ENDVALUE = 3
	/* End delimiter indicating start of invalid values for this enum */
};

/*
 *   Enumerations to indicate types of address mapping
 */
enum proc_mgr_map_type {
	PROC_MGR_MAPTYPE_VIRT = 0,
	/* Map/unmap to virtual address space (kernel/user) */
	PROC_MGR_MAPTYPE_SLAVE = 1,
	/* Map/unmap to slave address space */
	PROC_MGR_MAPTYPE_ENDVALUE = 2
	/* End delimiter indicating start of invalid values for this enum */
};

/*
 * Module configuration structure.
 */
struct proc_mgr_config {
	void *gate_handle;
} ;

/*
 *   Configuration parameters specific to the slave ProcMgr instance.
 */
struct proc_mgr_params {
	void *proc_handle;
	/* void * to the Processor object associated with this ProcMgr. */
	void *loader_handle;
    /*!< Handle to the Loader object associated with this ProcMgr. */
	void *pwr_handle;
    /*!< Handle to the PwrMgr object associated with this ProcMgr. */
};

/*
 *   Configuration parameters specific to the slave ProcMgr instance.
 */
struct proc_mgr_attach_params {
	enum proc_mgr_boot_mode boot_mode;
	/* Boot mode for the slave processor. */
} ;


/*
 *   This structure defines information about memory regions mapped by
 *  the ProcMgr module.
 */
struct  proc_mgr_addr_info {
/*	bool is_init;  */
	unsigned short is_init;
	/* Is this memory region initialized? */
	u32 addr[PROC_MGR_ADDRTYPE_ENDVALUE];
	/* Addresses for each type of address space */
	u32 size;
	/* Size of the memory region in bytes */
};

/*
 *   Characteristics of the slave processor
 */
struct proc_mgr_proc_info {
	enum proc_mgr_boot_mode	boot_mode;
	/* Boot mode of the processor. */
	u16 num_mem_entries;
	/* Number of valid memory entries */
	struct proc_mgr_addr_info mem_entries[PROCMGR_MAX_MEMORY_REGIONS];
	/* Configuration of memory regions */
};


/*
 * Function pointer type that is passed to the proc_mgr_registerNotify function
*/
typedef int (*proc_mgr_callback_fxn)(u16 proc_id, void *handle,
		enum proc_mgr_state from_state, enum proc_mgr_state to_state);

/* Function to get the default configuration for the ProcMgr module. */
void proc_mgr_get_config(struct proc_mgr_config *cfg);

/* Function to setup the ProcMgr module. */
int proc_mgr_setup(struct proc_mgr_config *cfg);

/* Function to destroy the ProcMgr module. */
int proc_mgr_destroy(void);

/* Function to initialize the parameters for the ProcMgr instance. */
void proc_mgr_params_init(void *handle, struct proc_mgr_params *params);

/* Function to create a ProcMgr object for a specific slave processor. */
void *proc_mgr_create(u16 proc_id, const struct proc_mgr_params *params);

/* Function to delete a ProcMgr object for a specific slave processor. */
int proc_mgr_delete(void **handle_ptr);

/* Function to open a handle to an existing ProcMgr object handling the
 * proc_id.
 */
int proc_mgr_open(void **handle, u16 proc_id);

/* Function to close this handle to the ProcMgr instance. */
int proc_mgr_close(void *handle);

/* Function to initialize the parameters for the ProcMgr attach function. */
void proc_mgr_get_attach_params(void *handle,
		struct proc_mgr_attach_params *params);

/* Function to attach the client to the specified slave and also initialize the
 * slave(if required).
 */
int proc_mgr_attach(void *handle, struct proc_mgr_attach_params *params);

/* Function to detach the client from the specified slave and also finalze the
 * slave(if required).
 */
int proc_mgr_detach(void *handle);

/* Function to get the current state of the slave Processor as maintained on
 * the master Processor state machine.
 */
enum proc_mgr_state proc_mgr_get_state(void *handle);

/* Function to read from the slave Processor's memory space. */
int proc_mgr_read(void *handle, u32 proc_addr, u32 *num_bytes,
		  void *buffer);

/* Function to read from the slave Processor's memory space. */
int proc_mgr_write(void *handle, u32 proc_addr, u32 *num_bytes, void *buffer);

/* Function that provides a hook for performing device dependent operations on
 * the slave Processor.
 */
int proc_mgr_control(void *handle, int cmd, void *arg);

int proc_mgr_translate_addr(void *handle, void **dst_addr,
		enum proc_mgr_addr_type dst_addr_type, void *src_addr,
		enum proc_mgr_addr_type src_addr_type);

/* Function that registers for notification when the slave processor
 * transitions to any of the states specified.
 */
int proc_mgr_register_notify(void *handle, proc_mgr_callback_fxn fxn,
				void *args, enum proc_mgr_state state[]);

/* Function that returns information about the characteristics of the slave
 * processor.
 */
int proc_mgr_get_proc_info(void *handle, struct proc_mgr_proc_info *proc_info);

/* Function that returns virtual to physical translations
 */
int proc_mgr_virt_to_phys(void *handle, u32 da, u32 *mapped_entries,
						u32 num_of_entries);

#endif
