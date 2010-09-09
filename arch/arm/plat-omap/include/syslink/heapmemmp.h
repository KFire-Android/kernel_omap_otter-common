/*
 *  heapmemmp.h
 *
 *  Heap module manages fixed size buffers that can be used
 *  in a multiprocessor system with shared memory.
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

#ifndef _HEAPMEMMP_H_
#define _HEAPMEMMP_H_

#include <linux/types.h>
#include <heap.h>
#include <listmp.h>

/*!
 *  @def	LISTMP_MODULEID
 *  @brief  Unique module ID.
 */
#define HEAPMEMMP_MODULEID	(0x4cd7)

/*
 *  Creation of Heap Buf succesful.
*/
#define HEAPMEMMP_CREATED            (0x07041776)

/*
 *  Version.
 */
#define HEAPMEMMP_VERSION            (1)

/* =============================================================================
 *  All success and failure codes for the module
 * =============================================================================
 */

/*!
 *  @def    HEAPMEMMP_S_BUSY
 *  @brief  The resource is still in use
 */
#define HEAPMEMMP_S_BUSY               2

/*!
 *  @def    HEAPMEMMP_S_ALREADYSETUP
 *  @brief  The module has been already setup
 */
#define HEAPMEMMP_S_ALREADYSETUP       1

/*!
 *  @def    HEAPMEMMP_S_SUCCESS
 *  @brief  Operation is successful.
 */
#define HEAPMEMMP_S_SUCCESS            0

/*!
 *  @def    HEAPMEMMP_E_FAIL
 *  @brief  Generic failure.
 */
#define HEAPMEMMP_E_FAIL              -1

/*!
 *  @def    HEAPMEMMP_E_INVALIDARG
 *  @brief  Argument passed to function is invalid.
 */
#define HEAPMEMMP_E_INVALIDARG          -2

/*!
 *  @def    HEAPMEMMP_E_MEMORY
 *  @brief  Operation resulted in memory failure.
 */
#define HEAPMEMMP_E_MEMORY              -3

/*!
 *  @def    HEAPMEMMP_E_ALREADYEXISTS
 *  @brief  The specified entity already exists.
 */
#define HEAPMEMMP_E_ALREADYEXISTS       -4

/*!
 *  @def    HEAPMEMMP_E_NOTFOUND
 *  @brief  Unable to find the specified entity.
 */
#define HEAPMEMMP_E_NOTFOUND            -5

/*!
 *  @def    HEAPMEMMP_E_TIMEOUT
 *  @brief  Operation timed out.
 */
#define HEAPMEMMP_E_TIMEOUT             -6

/*!
 *  @def    HEAPMEMMP_E_INVALIDSTATE
 *  @brief  Module is not initialized.
 */
#define HEAPMEMMP_E_INVALIDSTATE        -7

/*!
 *  @def    HEAPMEMMP_E_OSFAILURE
 *  @brief  A failure occurred in an OS-specific call  */
#define HEAPMEMMP_E_OSFAILURE           -8

/*!
 *  @def    HEAPMEMMP_E_RESOURCE
 *  @brief  Specified resource is not available  */
#define HEAPMEMMP_E_RESOURCE            -9

/*!
 *  @def    HEAPMEMMP_E_RESTART
 *  @brief  Operation was interrupted. Please restart the operation  */
#define HEAPMEMMP_E_RESTART             -10


/* =============================================================================
 * Macros
 * =============================================================================
 */


/* =============================================================================
 * Structures & Enums
 * =============================================================================
 */

/*
 *  Structure defining config parameters for the HeapBuf module.
 */
struct heapmemmp_config {
	u32 max_name_len; /* Maximum length of name */
	u32 max_runtime_entries; /* Maximum number of heapmemmp instances */
				/* that can be created */
};

/*
 *  Structure defining parameters for the HeapBuf module
 */
struct heapmemmp_params {
	char *name; /* Name of this instance */
	u16 region_id; /* Shared region ID */
	void *shared_addr; /* Physical address of the shared memory */
	u32 shared_buf_size; /* Size of shared buffer */
	void *gate; /* GateMP used for critical region management of the */
			/* shared memory */
};

/*
 *  Stats structure for the getExtendedStats API.
 */
struct heapmemmp_extended_stats {
	void *buf;
	/* Local address of the shared buffer */

	u32 size;
	/* Size of the shared buffer */
};

/* =============================================================================
 *  APIs
 * =============================================================================
 */

/*
 *  Function to get default configuration for the heapmemmp module
 */
int heapmemmp_get_config(struct heapmemmp_config *cfgparams);

/*
 *  Function to setup the heapmemmp module
 */
int heapmemmp_setup(const struct heapmemmp_config *cfg);

/*
 *  Function to destroy the heapmemmp module
 */
int heapmemmp_destroy(void);

/* Initialize this config-params structure with supplier-specified
 *  defaults before instance creation
 */
void heapmemmp_params_init(struct heapmemmp_params *params);

/*
 *  Creates a new instance of heapmemmp module
 */
void *heapmemmp_create(const struct heapmemmp_params *params);

/*
 * Deletes a instance of heapmemmp module
 */
int heapmemmp_delete(void **handle_ptr);

/*
 *  Opens a created instance of heapmemmp module by name
 */
int heapmemmp_open(char *name, void **handle_ptr);

/*
 *  Opens a created instance of heapmemmp module by address
 */
int heapmemmp_open_by_addr(void *shared_addr, void **handle_ptr);

/*
 *  Closes previously opened/created instance of heapmemmp module
 */
int heapmemmp_close(void **handle_ptr);

/*
 *  Returns the amount of shared memory required for creation
 *  of each instance
 */
int heapmemmp_shared_mem_req(const struct heapmemmp_params *params);

/*
 *  Allocate a block
 */
void *heapmemmp_alloc(void *hphandle, u32 size, u32 align);

/*
 *  Frees the block to this heapmemmp
 */
int heapmemmp_free(void *hphandle, void *block, u32 size);

/*
 *  Get memory statistics
 */
void heapmemmp_get_stats(void *hphandle, struct memory_stats *stats);

/*
 *  Indicate whether the heap may block during an alloc or free call
 */
bool heapmemmp_isblocking(void *handle);

/*
 *  Get extended statistics
 */
void heapmemmp_get_extended_stats(void *hphandle,
				       struct heapmemmp_extended_stats *stats);
/*
 *  Restore an instance to it's original created state.
 */
void heapmemmp_restore(void *handle);

#endif /* _HEAPMEMMP_H_ */
