/*
 *  gatehwspinlock.h
 *
 *  Defines for gatehwspinlock.
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


#ifndef GATEHWSPINLOCK_H_
#define GATEHWSPINLOCK_H_

/* Module headers */
#include <multiproc.h>
#include <gatemp.h>
#include <igatempsupport.h>
#include <sharedregion.h>

/* Unique module ID. */
#define GATEHWSPINLOCK_MODULEID			(0xF416)

/* =============================================================================
 * Module Success and Failure codes
 * =============================================================================
 */
/* Argument passed to a function is invalid. */
#define GATEHWSPINLOCK_E_INVALIDARG       -1

/* Memory allocation failed. */
#define GATEHWSPINLOCK_E_MEMORY           -2

/* the name is already registered or not. */
#define GATEHWSPINLOCK_E_BUSY             -3

/* Generic failure. */
#define GATEHWSPINLOCK_E_FAIL             -4

/* name not found in the nameserver. */
#define GATEHWSPINLOCK_E_NOTFOUND         -5

/* Module is not initialized. */
#define GATEHWSPINLOCK_E_INVALIDSTATE     -6

/* Instance is not created on this processor. */
#define GATEHWSPINLOCK_E_NOTONWER         -7

/* Remote opener of the instance has not closed the instance. */
#define GATEHWSPINLOCK_E_REMOTEACTIVE     -8

/* Indicates that the instance is in use. */
#define GATEHWSPINLOCK_E_INUSE            -9

/* Failure in OS call. */
#define GATEHWSPINLOCK_E_OSFAILURE        -10

/* Version mismatch error. */
#define GATEHWSPINLOCK_E_VERSION          -11

/* Operation successful. */
#define GATEHWSPINLOCK_S_SUCCESS            0

/* The GATEHWSPINLOCK module has already been setup in this process. */
#define GATEHWSPINLOCK_S_ALREADYSETUP     1


/* =============================================================================
 * Macros
 * =============================================================================
 */

/* Q_BLOCKING */
#define GATEHWSEM_Q_BLOCKING   (1)

/* Q_PREEMPTING */
#define GATEHWSEM_Q_PREEMPTING (2)


/* =============================================================================
 * Enums & Structures
 * =============================================================================
 */

/* Structure defining config parameters for the gatehwspinlock module. */
struct gatehwspinlock_config {
	enum gatemp_local_protect default_protection;
	/* Default module-wide local context protection level. The level of
	 * protection specified here determines which local gate is created
	 * per gatehwspinlock instance for local protection during create.
	 * The instance configuration parameter may be used to override this
	 * module setting per instance.  The configuration used here should
	 * reflect both the context in which enter and leave are to be called,
	 * as well as the maximum level protection needed locally.
	 */
	u32 base_addr;
	/* Device-specific base address for HW Semaphore subsystem in HOST OS
	 * address space, this is updated in Ipc module */
	u32 num_locks;
	/* Device-specific number of semphores in the HW Semaphore subsystem */
};

/* Structure defining config parameters for the gatehwspinlock instances. */
struct gatehwspinlock_params {
	IGATEMPSUPPORT_SUPERPARAMS;
};


/* Inherit everything from IGateMPSupport */
IGATEMPSUPPORT_INHERIT(gatehwspinlock);


/* =============================================================================
 * APIs
 * =============================================================================
 */
/* Function to get the default configuration for the gatehwspinlock module. */
void gatehwspinlock_get_config(struct gatehwspinlock_config *config);

/* Function to setup the gatehwspinlock module. */
int gatehwspinlock_setup(const struct gatehwspinlock_config *config);

/* Function to destroy the gatehwspinlock module */
int gatehwspinlock_destroy(void);

/* Get the default parameters for the gatehwspinlock instance. */
void gatehwspinlock_params_init(struct gatehwspinlock_params *params);

/* Function to create an instance of gatehwspinlock */
void *gatehwspinlock_create(enum igatempsupport_local_protect local_protect,
				const struct gatehwspinlock_params *params);

/* Function to delete an instance of gatehwspinlock */
int gatehwspinlock_delete(void **handle_ptr);

/* Function to enter the gatehwspinlock instance */
int *gatehwspinlock_enter(void *handle);

/* Function to leave the gatehwspinlock instance */
void gatehwspinlock_leave(void *handle, int *key);

/* Function to return the shared memory requirement for a single instance */
u32 gatehwspinlock_shared_mem_req(const struct gatehwspinlock_params *params);

/* Function to return the number of instances configured in the module. */
u32 gatehwspinlock_get_num_instances(void);

/* Function to return the number of instances not controlled by GateMP. */
u32 gatehwspinlock_get_num_reserved(void);

/* Function to initialize the locks module. */
void gatehwspinlock_locks_init(void);

#endif /* ifndef GATEHWSPINLOCK_H_ */
