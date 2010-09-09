/*
 *  heapbufmp.h
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

#ifndef _HEAPBUFMP_H_
#define _HEAPBUFMP_H_

#include <linux/types.h>
#include <heap.h>
#include <listmp.h>

/*!
 *  @def	LISTMP_MODULEID
 *  @brief  Unique module ID.
 */
#define HEAPBUFMP_MODULEID	(0x4cd5)

/*
 *  Creation of Heap Buf succesful.
*/
#define HEAPBUFMP_CREATED            (0x05251995)

/*
 *  Version.
 */
#define HEAPBUFMP_VERSION            (1)

/* =============================================================================
 *  All success and failure codes for the module
 * =============================================================================
 */

/*!
 *  @def    HEAPBUFMP_S_BUSY
 *  @brief  The resource is still in use
 */
#define HEAPBUFMP_S_BUSY               2

/*!
 *  @def    HEAPBUFMP_S_ALREADYSETUP
 *  @brief  The module has been already setup
 */
#define HEAPBUFMP_S_ALREADYSETUP       1

/*!
 *  @def    HEAPBUFMP_S_SUCCESS
 *  @brief  Operation is successful.
 */
#define HEAPBUFMP_S_SUCCESS            0

/*!
 *  @def    HEAPBUFMP_E_FAIL
 *  @brief  Generic failure.
 */
#define HEAPBUFMP_E_FAIL              -1

/*!
 *  @def    HEAPBUFMP_E_INVALIDARG
 *  @brief  Argument passed to function is invalid.
 */
#define HEAPBUFMP_E_INVALIDARG          -2

/*!
 *  @def    HEAPBUFMP_E_MEMORY
 *  @brief  Operation resulted in memory failure.
 */
#define HEAPBUFMP_E_MEMORY              -3

/*!
 *  @def    HEAPBUFMP_E_ALREADYEXISTS
 *  @brief  The specified entity already exists.
 */
#define HEAPBUFMP_E_ALREADYEXISTS       -4

/*!
 *  @def    HEAPBUFMP_E_NOTFOUND
 *  @brief  Unable to find the specified entity.
 */
#define HEAPBUFMP_E_NOTFOUND            -5

/*!
 *  @def    HEAPBUFMP_E_TIMEOUT
 *  @brief  Operation timed out.
 */
#define HEAPBUFMP_E_TIMEOUT             -6

/*!
 *  @def    HEAPBUFMP_E_INVALIDSTATE
 *  @brief  Module is not initialized.
 */
#define HEAPBUFMP_E_INVALIDSTATE        -7

/*!
 *  @def    HEAPBUFMP_E_OSFAILURE
 *  @brief  A failure occurred in an OS-specific call  */
#define HEAPBUFMP_E_OSFAILURE           -8

/*!
 *  @def    HEAPBUFMP_E_RESOURCE
 *  @brief  Specified resource is not available  */
#define HEAPBUFMP_E_RESOURCE            -9

/*!
 *  @def    HEAPBUFMP_E_RESTART
 *  @brief  Operation was interrupted. Please restart the operation  */
#define HEAPBUFMP_E_RESTART             -10


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
struct heapbufmp_config {
	u32 max_name_len; /* Maximum length of name */
	u32 max_runtime_entries; /* Maximum number of heapbufmp instances */
				/* that can be created */
	bool track_allocs;	/* Track the maximum number of allocated */
				/* blocks */
};

/*
 *  Structure defining parameters for the HeapBuf module
 */
struct heapbufmp_params {
	char *name; /* Name of this instance */
	u16 region_id; /* Shared region ID */
	void *shared_addr; /* Physical address of the shared memory */
	u32 block_size; /* Size (in MAUs) of each block */
	u32 num_blocks; /* Number of fixed-size blocks */
	u32 align; /* Alignment (in MAUs, power of 2) of each block */
	bool exact; /* Only allocate on exact match of rquested size */
	void *gate; /* GateMP used for critical region management of */
		    /* the shared memory */
};

/*
 *  Stats structure for the getExtendedStats API.
 */
struct heapbufmp_extended_stats {
	u32 max_allocated_blocks;
	/* maximum number of blocks allocated from this heap instance */
	u32 num_allocated_blocks;
	/* total number of blocks currently allocated from this heap instance*/
};

/* =============================================================================
 *  APIs
 * =============================================================================
 */

/*
 *  Function to get default configuration for the heapbufmp module
 */
int heapbufmp_get_config(struct heapbufmp_config *cfgparams);

/*
 *  Function to setup the heapbufmp module
 */
int heapbufmp_setup(const struct heapbufmp_config *cfg);

/*
 *  Function to destroy the heapbufmp module
 */
int heapbufmp_destroy(void);

/* Initialize this config-params structure with supplier-specified
 *  defaults before instance creation
 */
void heapbufmp_params_init(struct heapbufmp_params *params);

/*
 *  Creates a new instance of heapbufmp module
 */
void *heapbufmp_create(const struct heapbufmp_params *params);

/*
 * Deletes a instance of heapbufmp module
 */
int heapbufmp_delete(void **handle_ptr);

/*
 *  Opens a created instance of heapbufmp module by name
 */
int heapbufmp_open(char *name, void **handle_ptr);

/*
 *  Opens a created instance of heapbufmp module by address
 */
int heapbufmp_open_by_addr(void *shared_addr, void **handle_ptr);

/*
 *  Closes previously opened/created instance of heapbufmp module
 */
int heapbufmp_close(void **handle_ptr);

/*
 *  Returns the amount of shared memory required for creation
 *  of each instance
 */
int heapbufmp_shared_mem_req(const struct heapbufmp_params *params);

/*
 *  Allocate a block
 */
void *heapbufmp_alloc(void *hphandle, u32 size, u32 align);

/*
 *  Frees the block to this heapbufmp
 */
int heapbufmp_free(void *hphandle, void *block, u32 size);

/*
 *  Get memory statistics
 */
void heapbufmp_get_stats(void *hphandle, struct memory_stats *stats);

/*
 *  Indicate whether the heap may block during an alloc or free call
 */
bool heapbufmp_isblocking(void *handle);

/*
 *  Get extended statistics
 */
void heapbufmp_get_extended_stats(void *hphandle,
				struct heapbufmp_extended_stats *stats);


#endif /* _HEAPBUFMP_H_ */
