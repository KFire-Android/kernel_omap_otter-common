/*
 *  gatepeterson.h
 *
 *  The Gate Peterson Algorithm for mutual exclusion of shared memory.
 *  Current implementation works for 2 processors.
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

#ifndef _GATEPETERSON_H_
#define _GATEPETERSON_H_

#include <linux/types.h>

#include <igatempsupport.h>

/*
 *  GATEPETERSON_MODULEID
 *  Unique module ID
 */
#define GATEPETERSON_MODULEID       (0xF415)

/*
 *  A set of context protection levels that each correspond to
 *  single processor gates used for local protection
 */
enum gatepeterson_protect {
	GATEPETERSON_PROTECT_DEFAULT	= 0,
	GATEPETERSON_PROTECT_NONE	= 1,
	GATEPETERSON_PROTECT_INTERRUPT	= 2,
	GATEPETERSON_PROTECT_TASKLET	= 3,
	GATEPETERSON_PROTECT_THREAD	= 4,
	GATEPETERSON_PROTECT_PROCESS	= 5,
	GATEPETERSON_PROTECT_END_VALUE	= 6
};

/*
 *  Structure defining config parameters for the Gate Peterson
 *  module
 */
struct gatepeterson_config {
	enum gatepeterson_protect default_protection;
	/*!< Default module-wide local context protection level. The level of
	* protection specified here determines which local gate is created per
	* GatePeterson instance for local protection during create. The instance
	* configuration parameter may be usedto override this module setting per
	* instance.  The configuration used here should reflect both the context
	* in which enter and leave are to be called,as well as the maximum level
	* of protection needed locally.
	*/
	u32 num_instances;
	/*!< Maximum number of instances supported by the GatePeterson module */

};

/*
 *  Structure defining config parameters for the Gate Peterson
 *  instances
 */
struct gatepeterson_params {
	IGATEMPSUPPORT_SUPERPARAMS;
};

/*
 *  Function to initialize the parameter structure
 */
void gatepeterson_get_config(struct gatepeterson_config *config);

/*
 *  Function to initialize GP module
 */
int gatepeterson_setup(const struct gatepeterson_config *config);

/*
 *  Function to destroy the GP module
 */
int gatepeterson_destroy(void);

/*
 *  Function to initialize the parameter structure
 */
void gatepeterson_params_init(struct gatepeterson_params *params);

/*
 *  Function to create an instance of GatePeterson
 */
void *gatepeterson_create(enum igatempsupport_local_protect local_protect,
				const struct gatepeterson_params *params);

/*
 *  Function to delete an instance of GatePeterson
 */
int gatepeterson_delete(void **gphandle);

/*
 *  Function to open a previously created instance by address
 */
int gatepeterson_open_by_addr(enum igatempsupport_local_protect local_protect,
				void *shared_addr, void **gphandle);

/*
 *  Function to close a previously opened instance
 */
int gatepeterson_close(void **gphandle);

/*
 * Function to enter the gate peterson
 */
int *gatepeterson_enter(void *gphandle);

/*
 *Function to leave the gate peterson
 */
void gatepeterson_leave(void *gphandle, int *key);

/*
 * Function to return the shared memory requirement
 */
u32 gatepeterson_shared_mem_req(const struct gatepeterson_params *params);

/*
 * Function to return the number of instances configured in the module.
 */
u32 gatepeterson_get_num_instances(void);

/*
 * Function to return the number of instances not controlled by GateMP.
 */
u32 gatepeterson_get_num_reserved(void);

/*
 * Function to initialize the locks module.
 */
void gatepeterson_locks_init(void);

#endif /* _GATEPETERSON_H_ */
