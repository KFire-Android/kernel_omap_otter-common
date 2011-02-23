/*
 *  sysmgr.h
 *
 *  Defines for System manager.
 *
 *  Copyright(C) 2009 Texas Instruments, Inc.
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

#ifndef _IPC_H_
#define _IPC_H_


/* Module headers */
#include <multiproc.h>
#include <gatepeterson.h>
#include <sharedregion.h>
#include <notify.h>
#include <notify_ducatidriver.h>
#include <heap.h>
#include <heapbufmp.h>
#include <heapmemmp.h>



/* Unique module ID. */
#define IPC_MODULEID			(0xF086)


/* =============================================================================
 * Module Success and Failure codes
 * =============================================================================
 */
/* The resource is still in use */
#define IPC_S_BUSY		2

/* The module has been already setup */
#define IPC_S_ALREADYSETUP	1

/* Operation is successful. */
#define IPC_S_SUCCESS		0

/* Generic failure. */
#define IPC_E_FAIL		-1

/* Argument passed to function is invalid. */
#define IPC_E_INVALIDARG	-2

/* Operation resulted in memory failure. */
#define IPC_E_MEMORY		-3

/* The specified entity already exists. */
#define IPC_E_ALREADYEXISTS	-4

/* Unable to find the specified entity. */
#define IPC_E_NOTFOUND		-5

/* Operation timed out. */
#define IPC_E_TIMEOUT		-6

/* Module is not initialized. */
#define IPC_E_INVALIDSTATE	-7

/* A failure occurred in an OS-specific call */
#define IPC_E_OSFAILURE		-8

/* Specified resource is not available */
#define IPC_E_RESOURCE		-9

/* Operation was interrupted. Please restart the operation */
#define IPC_E_RESTART		-10


/* =============================================================================
 * Macros
 * =============================================================================
 */
/* IPC_CONTROLCMD_LOADCALLBACK */
#define IPC_CONTROLCMD_LOADCALLBACK	(0xBABE0000)

/* IPC_CONTROLCMD_STARTCALLBACK */
#define IPC_CONTROLCMD_STARTCALLBACK	(0xBABE0001)

/* IPC_CONTROLCMD_STOPCALLBACK */
#define IPC_CONTROLCMD_STOPCALLBACK	(0xBABE0002)


/* =============================================================================
 * Enums & Structures
 * =============================================================================
 */
/* the different options for processor synchronization */
enum ipc_proc_sync {
	IPC_PROCSYNC_NONE,          /* don't do any processor sync            */
	IPC_PROCSYNC_PAIR,          /* sync pair of processors in ipc_attach  */
	IPC_PROCSYNC_ALL            /* sync all processors, done in ipc_start */
};

/* ipc configuration structure. */
struct ipc_config {
	enum ipc_proc_sync proc_sync;
	/* the different options for processor synchronization */
};

/* ipc configuration structure. */
struct ipc_params {
	bool setup_messageq;
	bool setup_notify;
	bool setup_ipu_pm;
	bool slave;
	enum ipc_proc_sync proc_sync;
};

/* IPC events. */
enum {
	IPC_CLOSE = 0,
	IPC_START = 1,
	IPC_STOP = 2,
};

/* =============================================================================
 * APIs
 * =============================================================================
 */
/* Attach to remote processor */
int ipc_attach(u16 remote_proc_id);

/* Detach from the remote processor */
int ipc_detach(u16 remote_proc_id);

/* Reads the config entry from the config area. */
int ipc_read_config(u16 remote_proc_id, u32 tag, void *cfg, u32 size);

/* Reserves memory, creates default gatemp and heapmemmp */
int ipc_start(void);

/* Writes the config entry to the config area. */
int ipc_write_config(u16 remote_proc_id, u32 tag, void *cfg, u32 size);

/* Returns default configuration values for ipc. */
void ipc_get_config(struct ipc_config *cfg_params);

/* Sets up ipc for this processor. */
int ipc_setup(const struct ipc_config *cfg_params);

/* Destroys ipc for this processor. */
int ipc_destroy(void);

/* Creates a ipc. */
int ipc_create(u16 proc_id, struct ipc_params *params);

/* Function to control a Ipc instance for a slave */
int ipc_control(u16 proc_id, u32 cmd_id, void *arg);

/* Function to read configuration information from ipc module */
int ipc_read_config(u16 remote_proc_id, u32 tag, void *cfg, u32 size);

/* Function to write configuration information to ipc module */
int ipc_write_config(u16 remote_proc_id, u32 tag, void *cfg, u32 size);

/* Clears memory, deletes  default gatemp and heapmemmp */
int ipc_stop(void);

/* IPC event notifications. */
int ipc_notify_event(int event, void *data);

/* Register for IPC events. */
int ipc_register_notifier(struct notifier_block *nb);

/* Un-register for IPC events. */
int ipc_unregister_notifier(struct notifier_block *nb);

/* check if ipc is in recovery state */
#ifdef CONFIG_SYSLINK_RECOVERY
bool ipc_recovering(void);

/* Indicate to schedule the recovery mechanism */
void ipc_recover_schedule(void);
#endif /* ifdef CONFIG_SYSLINK_RECOVERY */
#endif /* ifndef _IPC_H_ */
