/*
 * proc4430_drvdefs.h
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


#ifndef _SYSLINK_PROC4430_H
#define _SYSLINK_PROC4430_H


/* Module headers */
#include "../procmgr_drvdefs.h"
#include "proc4430.h"


/*  ----------------------------------------------------------------------------
 *  IOCTL command IDs for OMAP4430PROC
 *  ----------------------------------------------------------------------------
 */
/*
 * Base command ID for OMAP4430PROC
 */
#define PROC4430_BASE_CMD		 0x200

/*
 * Command for PROC4430_getConfig
 */
#define CMD_PROC4430_GETCONFIG		(PROC4430_BASE_CMD + 1)

/*
 * Command for PROC4430_setup
 */
#define CMD_PROC4430_SETUP		(PROC4430_BASE_CMD + 2)

/*
 * Command for PROC4430_setup
 */
#define CMD_PROC4430_DESTROY		(PROC4430_BASE_CMD + 3)

/*
 * Command for PROC4430_destroy
 */
#define CMD_PROC4430_PARAMS_INIT	(PROC4430_BASE_CMD + 4)

/*
 * Command for PROC4430_create
 */
#define CMD_PROC4430_CREATE		(PROC4430_BASE_CMD + 5)

/*
 * Command for PROC4430_delete
 */
#define CMD_PROC4430_DELETE		(PROC4430_BASE_CMD + 6)

/*
 * Command for PROC4430_open
 */
#define CMD_PROC4430_OPEN		(PROC4430_BASE_CMD + 7)

/*
 * Command for PROC4430_close
 */
#define CMD_PROC4430_CLOSE		(PROC4430_BASE_CMD + 8)


/*  ---------------------------------------------------
 *  Command arguments for OMAP4430PROC
 *  ---------------------------------------------------
 */
/*
 * Command arguments for PROC4430_getConfig
 */
struct proc4430_cmd_args_get_config {
	struct proc_mgr_cmd_args command_args;
	/* Common command args */
	struct proc4430_config *cfg;
	/* Pointer to the OMAP4430PROC module configuration structure
	* in which the default config is to be returned. */
};

/*
 * Command arguments for PROC4430_setup
 */
struct proc4430_cmd_args_setup {
	struct proc_mgr_cmd_args command_args;
	/* Common command args */
	struct proc4430_config *cfg;
	/* Optional OMAP4430PROC module configuration. If provided as NULL,
	* default configuration is used. */
};

/*
 * Command arguments for PROC4430_destroy
 */
struct proc4430_cmd_args_destroy {
	struct proc_mgr_cmd_args command_args;
	/* Common command args */
};

/*
 * Command arguments for struct struct proc4430_params_init
 */
struct proc4430_cmd_args_params_init {
	struct proc_mgr_cmd_args command_args;
	/* Common command args */
	void *handle;
	/* void * to the processor instance. */
	struct proc4430_params *params;
	/* Configuration parameters. */
};

/*
 * Command arguments for PROC4430_create
 */
struct proc4430_cmd_args_create {
	struct proc_mgr_cmd_args	 command_args;
	/* Common command args */
	u16 proc_id;
	/* Processor ID for which this processor instance is required. */
	struct proc4430_params *params;
	/*Configuration parameters. */
	void *handle;
	/* void * to the created processor instance. */
};

/*
 * Command arguments for PROC4430_delete
 */
struct proc4430_cmd_args_delete {
	struct proc_mgr_cmd_args command_args;
	/* Common command args */
	void *handle;
	/* Pointer to handle to the processor instance */
};

/*
 * Command arguments for PROC4430_open
 */
struct proc4430_cmd_args_open {
	struct proc_mgr_cmd_args command_args;
	/* Common command args */
	u16 proc_id;
	/* Processor ID addressed by this OMAP4430PROC instance. */
	void *handle;
	/* Return parameter: void * to the processor instance */
};

/*
 * Command arguments for PROC4430_close
 */
struct proc4430_cmd_args_close {
	struct proc_mgr_cmd_args command_args;
	/* Common command args */
	void *handle;
	/* void * to the processor instance */
};

#endif
