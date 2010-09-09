/*
 *  gatemp.h
 *
 *  gatemp wrapper defines
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

#ifndef _GATEMP_H_
#define _GATEMP_H_

#include <sharedregion.h>

/* Unique module ID. */
#define GATEMP_MODULEID		(0xAF70)

/* The resource is still in use */
#define GateMP_S_BUSY		2

/* The module has been already setup */
#define GateMP_S_ALREADYSETUP	1

/* Operation is successful. */
#define GateMP_S_SUCCESS	0

/* Generic failure. */
#define GateMP_E_FAIL		-1

/* The specified entity already exists. */
#define GateMP_E_ALREADYEXISTS	-4

/* Unable to find the specified entity. */
#define GateMP_E_NOTFOUND	-5

/* Operation timed out. */
#define GateMP_E_TIMEOUT	-6

/* Module is not initialized. */
#define GateMP_E_INVALIDSTATE	-7

/* A failure occurred in an OS-specific call */
#define GateMP_E_OSFAILURE	-8

/* Specified resource is not available */
#define GateMP_E_RESOURCE	-9

/* Operation was interrupted. Please restart the operation */
#define GateMP_E_RESTART	-10

/* Gate is local gate not remote */
#define GateMP_E_LOCALGATE	-11


/*
 * A set of local context protection levels
 *
 *  Each member corresponds to a specific local processor gates used for
 *  local protection.
 *
 *  In linux user mode, the following are the mapping for the constants
 *   - INTERRUPT -> [N/A]
 *   - TASKLET   -> [N/A]
 *   - THREAD    -> GateMutex
 *   - PROCESS   -> GateMutex
 *
 *  In linux kernel mode, the following are the mapping for the constants
 *   - INTERRUPT -> [Interrupts disabled]
 *   - TASKLET   -> GateMutex
 *   - THREAD    -> GateMutex
 *   - PROCESS   -> GateMutex
 *
 *  For SYS/BIOS users, the following are the mappings for the constants
 *   - INTERRUPT -> GateHwi: disables interrupts
 *   - TASKLET   -> GateSwi: disables Swi's (software interrupts)
 *   - THREAD    -> GateMutexPri: based on Semaphores
 *   - PROCESS   -> GateMutexPri: based on Semaphores
 */
enum gatemp_local_protect {
	GATEMP_LOCALPROTECT_NONE = 0,
	/* Use no local protection */

	GATEMP_LOCALPROTECT_INTERRUPT = 1,
	/* Use the INTERRUPT local protection level */

	GATEMP_LOCALPROTECT_TASKLET = 2,
	/* Use the TASKLET local protection level */

	GATEMP_LOCALPROTECT_THREAD = 3,
	/* Use the THREAD local protection level */

	GATEMP_LOCALPROTECT_PROCESS = 4
	/* Use the PROCESS local protection level */
};

/*
 * Type of remote Gate
 *
 *  Each member corresponds to a specific type of remote gate.
 *  Each enum value corresponds to the following remote protection levels:
 *   - NONE      -> No remote protection (the gatemp instance will
 *                  exclusively offer local protection configured in
 *                  #GateMP_Params::local_protect
 *   - SYSTEM    -> Use the SYSTEM remote protection level (default for
 *                  remote protection
 *   - CUSTOM1   -> Use the CUSTOM1 remote protection level
 *   - CUSTOM2   -> Use the CUSTOM2 remote protection level
 */
enum gatemp_remote_protect {
	GATEMP_REMOTEPROTECT_NONE = 0,
	/* No remote protection (the gatemp instance will exclusively
	 * offer local protection configured in #GateMP_Params::local_protect)
	 */

	GATEMP_REMOTEPROTECT_SYSTEM = 1,
	/* Use the SYSTEM remote protection level (default remote protection) */

	GATEMP_REMOTEPROTECT_CUSTOM1 = 2,
	/* Use the CUSTOM1 remote protection level */

	GATEMP_REMOTEPROTECT_CUSTOM2 = 3
	/* Use the CUSTOM2 remote protection level */
};

/* Structure defining parameters for the gatemp module. */
struct gatemp_params {
	char *name;
	/* Name of this instance.
	 * The name (if not NULL) must be unique among all GateMP
	 * instances in the entire system.  When creating a new
	 * heap, it is necessary to supply an instance name.
	 */

	u32 region_id;
	/* Shared region ID
	 * The index corresponding to the shared region from which shared memory
	 * will be allocated.
	 * If not specified, the default of '0' will be used.
	 */

	void *shared_addr;
	/* Physical address of the shared memory
	 * This value can be left as 'null' unless it is required to place the
	 * heap at a specific location in shared memory.  If sharedAddr is null,
	 * then shared memory for a new instance will be allocated from the
	 * heap belonging to the region identified by #GateMP_Params::region_id.
	 */

	enum gatemp_local_protect local_protect;
	/* Local protection level.
	 * The default value is #GATEMP_LOCALPROTECT_THREAD */

	enum gatemp_remote_protect remote_protect;
	/* Remote protection level
	 * The default value is #GATEMP_REMOTEPROTECT_SYSTEM */
};

/* Structure defining config parameters for the gatemp module. */
struct gatemp_config {
	u32 num_resources;
	/* Maximum number of resources */
	enum gatemp_local_protect default_protection;
	u32 max_name_len;
	u32 max_runtime_entries;
};


/* Close an opened gate */
int gatemp_close(void **handle_ptr);

/* Create a gatemp instance */
void *gatemp_create(const struct gatemp_params *params);

/* Delete a created gatemp instance */
int gatemp_delete(void **handle_ptr);

/* Query the gate */
bool gatemp_query(int qual);

/* Get the default remote gate */
void *gatemp_get_default_remote(void);

/* Get the local protect gate. */
enum gatemp_local_protect gatemp_get_local_protect(void *obj);

/* Get the remote protect gate. */
enum gatemp_remote_protect gatemp_get_remote_protect(void *obj);

/* Open a created gatemp by name */
int gatemp_open(char *name, void **handle_ptr);

/* Open a created gatemp by address */
int gatemp_open_by_addr(void *shared_addr, void **handle_ptr);

/* Initialize a gatemp parameters struct */
void gatemp_params_init(struct gatemp_params *params);

/* Amount of shared memory required for creation of each instance */
uint gatemp_shared_mem_req(const struct gatemp_params *params);

/* Enter the gatemp */
int *gatemp_enter(void *handle);

/* Leave the gatemp */
void gatemp_leave(void *handle, int *key);

/* Get the default configuration for the gatemp module. */
void gatemp_get_config(struct gatemp_config *cfg_params);

/* Setup the gatemp module. */
s32 gatemp_setup(const struct gatemp_config *cfg);

/* Function to destroy the gatemp module. */
s32 gatemp_destroy(void);

/* Function to attach gatemp to a remote processor */
int gatemp_attach(u16 remote_proc_id, void *shared_addr);

/* Function to detach gatemp from a remote processor */
int gatemp_detach(u16 remote_proc_id, void *shared_addr);

/* Function to start gatemp */
int gatemp_start(void *shared_addr);

/* Function to start gatemp */
int gatemp_stop(void);

/* Function to create local gatemp */
void *gatemp_create_local(enum gatemp_local_protect local_protect);

/* Function to return size required in shared region 0 */
uint gatemp_get_region0_reserved_size(void);

/* Function to get the shared address of a gatemp object */
u32 *gatemp_get_shared_addr(void *obj);


#endif /* _GATEMP_H_ */
