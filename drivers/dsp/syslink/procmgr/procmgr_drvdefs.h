/*
 * procmgr_drvdefs.h
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


#ifndef SYSLINK_PROCMGR_DRVDEFS_H
#define SYSLINK_PROCMGR_DRVDEFS_H

#include <linux/types.h>

/* Module headers */
#include <procmgr.h>


/* =================================
 *  Macros and types
 * =================================
 */
/*
 * Base structure for ProcMgr command args. This needs to be the first
 *  field in all command args structures.
 */
struct proc_mgr_cmd_args {
	int api_status;
	/*Status of the API being called. */
};

/*  --------------------------------------
 *  IOCTL command IDs for ProcMgr
 *  ---------------------------------------
 */
/*
 * Base command ID for ProcMgr
 */
#define PROCMGR_BASE_CMD			0x100

/*
 * Command for ProcMgr_getConfig
 */
#define CMD_PROCMGR_GETCONFIG		(PROCMGR_BASE_CMD + 1)

/*
 * Command for ProcMgr_setup
 */
#define CMD_PROCMGR_SETUP			(PROCMGR_BASE_CMD + 2)

/*
 * Command for ProcMgr_setup
 */
#define CMD_PROCMGR_DESTROY			(PROCMGR_BASE_CMD + 3)

/*
 * Command for ProcMgr_destroy
 */
#define CMD_PROCMGR_PARAMS_INIT		(PROCMGR_BASE_CMD + 4)

/*
 * Command for ProcMgr_create
 */
#define CMD_PROCMGR_CREATE			(PROCMGR_BASE_CMD + 5)

/*
 * Command for ProcMgr_delete
 */
#define CMD_PROCMGR_DELETE			(PROCMGR_BASE_CMD + 6)

/*
 * Command for ProcMgr_open
 */
#define CMD_PROCMGR_OPEN			(PROCMGR_BASE_CMD + 7)

/*
 * Command for ProcMgr_close
 */
#define CMD_PROCMGR_CLOSE			(PROCMGR_BASE_CMD + 8)

/*
 * Command for ProcMgr_getAttachParams
 */
#define CMD_PROCMGR_GETATTACHPARAMS		(PROCMGR_BASE_CMD + 9)

/*
 * Command for ProcMgr_attach
 */
#define CMD_PROCMGR_ATTACH			(PROCMGR_BASE_CMD + 10)

/*
 * Command for ProcMgr_detach
 */
#define CMD_PROCMGR_DETACH			(PROCMGR_BASE_CMD + 11)

/*
 * Command for ProcMgr_load
 */
#define CMD_PROCMGR_LOAD			(PROCMGR_BASE_CMD + 12)

/*
 * Command for ProcMgr_unload
 */
#define CMD_PROCMGR_UNLOAD			(PROCMGR_BASE_CMD + 13)

/*
 * Command for ProcMgr_getStartParams
 */
#define CMD_PROCMGR_GETSTARTPARAMS		(PROCMGR_BASE_CMD + 14)

/*
 * Command for ProcMgr_start
 */
#define CMD_PROCMGR_START			(PROCMGR_BASE_CMD + 15)

/*
 * Command for ProcMgr_stop
 */
#define CMD_PROCMGR_STOP			(PROCMGR_BASE_CMD + 16)

/*
 * Command for ProcMgr_getState
 */
#define CMD_PROCMGR_GETSTATE			(PROCMGR_BASE_CMD + 17)

/*
 * Command for ProcMgr_read
 */
#define CMD_PROCMGR_READ			(PROCMGR_BASE_CMD + 18)

/*
 * Command for ProcMgr_write
 */
#define CMD_PROCMGR_WRITE			(PROCMGR_BASE_CMD + 19)

/*
 * Command for ProcMgr_control
 */
#define CMD_PROCMGR_CONTROL			(PROCMGR_BASE_CMD + 20)

/*
 * Command for ProcMgr_translateAddr
 */
#define CMD_PROCMGR_TRANSLATEADDR		(PROCMGR_BASE_CMD + 22)

/*
 * Command for ProcMgr_getSymbolAddress
 */
#define CMD_PROCMGR_GETSYMBOLADDRESS		(PROCMGR_BASE_CMD + 23)

/*
 * Command for ProcMgr_map
 */
#define CMD_PROCMGR_MAP				(PROCMGR_BASE_CMD + 24)

/*
 * Command for ProcMgr_registerNotify
 */
#define CMD_PROCMGR_REGISTERNOTIFY		(PROCMGR_BASE_CMD + 25)

/*
 * Command for ProcMgr_getProcInfo
 */
#define CMD_PROCMGR_GETPROCINFO			(PROCMGR_BASE_CMD + 26)

/*
 * Command for ProcMgr_unmap
 */
#define CMD_PROCMGR_UNMAP			(PROCMGR_BASE_CMD + 27)

/*
 * Command for ProcMgr_getVirtToPhysPages
 */
#define CMD_PROCMGR_GETVIRTTOPHYS		(PROCMGR_BASE_CMD + 28)


/*
 * Command to get Board revision
 */
#define CMD_PROCMGR_GETBOARDREV			(PROCMGR_BASE_CMD + 31)



/*  ----------------------------------------------------------------------------
 *  Command arguments for ProcMgr
 *  ----------------------------------------------------------------------------
 */
/*
 * Command arguments for ProcMgr_getConfig
 */
struct proc_mgr_cmd_args_get_config {
	struct proc_mgr_cmd_args commond_args;
	/*Common command args */
	struct proc_mgr_config *cfg;
	/*Pointer to the ProcMgr module configuration structure in which the
	 default config is to be returned. */
};

/*
 * Command arguments for ProcMgr_setup
 */
struct proc_mgr_cmd_args_setup {
	struct proc_mgr_cmd_args commond_args;
	/*Common command args */
	struct proc_mgr_config *cfg;
	/*Optional ProcMgr module configuration. If provided as NULL, default
	 configuration is used. */
};

/*
 * Command arguments for ProcMgr_destroy
 */
struct proc_mgr_cmd_args_destroy {
	struct proc_mgr_cmd_args commond_args;
	/*Common command args */
};

/*
 * Command arguments for ProcMgr_Params_init
 */
struct proc_mgr_cmd_args_params_init {
	struct proc_mgr_cmd_args commond_args;
	/*Common command args */
	void *handle;
	/*Handle to the ProcMgr object. */
	struct proc_mgr_params *params;
	/*Pointer to the ProcMgr instance params structure in which the default
	 params is to be returned. */
};

/*
 * Command arguments for ProcMgr_create
 */
struct proc_mgr_cmd_args_create {
	struct proc_mgr_cmd_args commond_args;
	/*Common command args */
	u16 proc_id;
	/*Processor ID represented by this ProcMgr instance */
	struct proc_mgr_params  params;
	/*ProcMgr instance configuration parameters. */
	void *handle;
	/*Handle to the created ProcMgr object */
};

/*
 * Command arguments for ProcMgr_delete
 */
struct proc_mgr_cmd_args_delete{
	struct proc_mgr_cmd_args commond_args;
	/*Common command args */
	void *handle;
	/*Pointer to Handle to the ProcMgr object */
};

/*
 * Command arguments for ProcMgr_open
 */
struct proc_mgr_cmd_args_open {
	struct proc_mgr_cmd_args commond_args;
	/*Common command args */
	u16 proc_id;
	/*Processor ID represented by this ProcMgr instance */
	void *handle;
	/*Handle to the opened ProcMgr object. */
	struct proc_mgr_proc_info proc_info;
	/*Processor information.  */
};

/*
 * Command arguments for ProcMgr_close
 */
struct proc_mgr_cmd_args_close{
	struct proc_mgr_cmd_args commond_args;
	/*Common command args */
	void *handle;
	/*Handle to the ProcMgr object */
	struct proc_mgr_proc_info proc_info;
	/*Processor information.  */
};

/*
 * Command arguments for ProcMgr_getAttachParams
 */
struct proc_mgr_cmd_args_get_attach_params{
	struct proc_mgr_cmd_args commond_args;
	/*Common command args */
	void *handle;
	/*Handle to the ProcMgr object. */
	struct proc_mgr_attach_params *params;
	/*Pointer to the ProcMgr attach params structure in which the default
	 params is to be returned. */
};

/*
 * Command arguments for ProcMgr_attach
 */
struct proc_mgr_cmd_args_attach {
	struct proc_mgr_cmd_args commond_args;
	/*Common command args */
	void *handle;
	/*Handle to the ProcMgr object. */
	struct proc_mgr_attach_params *params;
	/*Optional ProcMgr attach parameters.  */
	struct proc_mgr_proc_info proc_info;
	/*Processor information.  */
};

/*
 * Command arguments for ProcMgr_detach
 */
struct proc_mgr_cmd_args_detach {
	struct proc_mgr_cmd_args commond_args;
	/*Common command args */
	void *handle;
	/*Handle to the ProcMgr object */
	struct proc_mgr_proc_info proc_info;
	/*Processor information.  */
};


/*
 * Command arguments for ProcMgr_getStartParams
 */
struct proc_mgr_cmd_args_get_start_params {
	struct proc_mgr_cmd_args commond_args;
	/*Common command args */
	void *handle;
	/*Entry point for the image*/
	u32 entry_point;
	/*Handle to the ProcMgr object */
	struct proc_mgr_start_params *params;
	/*Pointer to the ProcMgr start params structure in which the default
	 params is to be returned. */
};

/*
 * Command arguments for ProcMgr_getState
 */
struct proc_mgr_cmd_args_get_state {
	struct proc_mgr_cmd_args commond_args;
	/*Common command args */
	void *handle;
	/* Handle to the ProcMgr object */
	enum proc_mgr_state proc_mgr_state;
	/*Current state of the ProcMgr object. */
};

/*
 * Command arguments for ProcMgr_read
 */
struct proc_mgr_cmd_args_read {
	struct proc_mgr_cmd_args commond_args;
	/*Common command args */
	void *handle;
	/*Handle to the ProcMgr object */
	u32 proc_addr;
	/*Address in space processor's address space of the memory region to
	 read from. */
	u32 num_bytes;
	/*IN/OUT parameter. As an IN-parameter, it takes in the number of bytes
	 to be read. When the function returns, this parameter contains the
	 number of bytes actually read. */
	void *buffer;
	/*User-provided buffer in which the slave processor's memory contents
	 are to be copied. */
};

/*
 * Command arguments for ProcMgr_write
 */
struct proc_mgr_cmd_args_write {
	struct proc_mgr_cmd_args commond_args;
	/*Common command args */
	void *handle;
	/*Handle to the ProcMgr object */
	u32 proc_addr;
	/*Address in space processor's address space of the memory region to
	 write into. */
	u32 num_bytes;
	/*IN/OUT parameter. As an IN-parameter, it takes in the number of bytes
	 to be written. When the function returns, this parameter contains the
	 number of bytes actually written. */
	void *buffer;
	/*User-provided buffer from which the data is to be written into the
	 slave processor's memory. */
};

/*
 * Command arguments for ProcMgr_control
 */
struct proc_mgr_cmd_args_control {
	struct proc_mgr_cmd_args commond_args;
	/*Common command args */
	void *handle;
	/*Handle to the ProcMgr object */
	int cmd;
	/*Device specific processor command */
	void *arg;
	/*Arguments specific to the type of command. */
};

/*
 * Command arguments for ProcMgr_translateAddr
 */
struct proc_mgr_cmd_args_translate_addr {
	struct proc_mgr_cmd_args commond_args;
	/*Common command args */
	void *handle;
	/*Handle to the ProcMgr object */
	void *dst_addr;
	/*Return parameter: Pointer to receive the translated address. */
	enum proc_mgr_addr_type dst_addr_type;
	/*Destination address type requested */
	void *src_addr;
	/*Source address in the source address space */
	enum proc_mgr_addr_type src_addr_type;
	/*Source address type */
};

/*
 * Command arguments for ProcMgr_getSymbolAddress
 */
struct proc_mgr_cmd_args_get_symbol_address {
	struct proc_mgr_cmd_args commond_args;
	/*Common command args */
	void *handle;
	/*Handle to the ProcMgr object */
	u32 file_id;
	/*ID of the file received from the load function */
	char *symbol_name;
	/*Name of the symbol */
	u32 sym_value;
	/*Return parameter: Symbol address */
};

/*
 * Command arguments for ProcMgr_registerNotify
 */
struct proc_mgr_cmd_args_register_notify {
	struct proc_mgr_cmd_args commond_args;
	/*Common command args */
	void *handle;
	/*Handle to the ProcMgr object */
	int (*callback_fxn)(u16 proc_id, void *handle,
		enum proc_mgr_state from_state, enum proc_mgr_state to_state);
	/*Handling function to be registered. */
	void *args;
	/*Optional arguments associated with the handler fxn. */
	enum proc_mgr_state state[];
	/*Array of target states for which registration is required. */
};

/*
 * Command arguments for ProcMgr_getProcInfo
 */
struct proc_mgr_cmd_args_get_proc_info {
	struct proc_mgr_cmd_args commond_args;
	/*Common command args */
	void *handle;
	/*Handle to the ProcMgr object */
	struct proc_mgr_proc_info *proc_info;
	/*Pointer to the ProcInfo object to be populated. */
};

/*
 * Command arguments for ProcMgr_virtToPhys
 */
struct proc_mgr_cmd_args_get_virt_to_phys {
	struct proc_mgr_cmd_args commond_args;
	/*Common command args */
	void *handle;
	u32 da;
	/* mem entries buffer */
	u32 *mem_entries;
	/* number of entries */
	u32 num_of_entries;
};

struct proc_mgr_cmd_args_cpurev {
	struct proc_mgr_cmd_args commond_args;
	u32 *cpu_rev;
};
#endif
