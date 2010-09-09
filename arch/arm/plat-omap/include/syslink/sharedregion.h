/*
 *  sharedregion.h
 *
 *  The SharedRegion module is designed to be used in a
 *  multi-processor environment where there are memory regions
 *  that are shared and accessed across different processors
 *
 *  Copyright (C) 2008-2010 Texas Instruments, Inc.
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

#ifndef _SHAREDREGION_H_
#define _SHAREDREGION_H_

#include <linux/types.h>
#include <heapmemmp.h>

/*
 *  SHAREDREGION_MODULEID
 *  Module ID for Shared region manager
 */
#define SHAREDREGION_MODULEID		(0x5D8A)

/*
 *  Name of the reserved nameserver used for application
 */
#define SHAREDREGION_NAMESERVER		"SHAREDREGION"

/*
 *  Name of the reserved nameserver used for application
 */
#define SHAREDREGION_INVALIDREGIONID	((u16)(~0))

/*!
 *  @def        SharedRegion_DEFAULTOWNERID
 *  @brief      Default owner processor id
 */
#define SHAREDREGION_DEFAULTOWNERID	((u16)(~0))

/*
 *  Name of the reserved nameserver used for application
 */
#define SHAREDREGION_INVALIDSRPTR	((u32 *)(~0))


struct sharedregion_entry {
	/* The base address of the region */
	void *base;
	/* The length of the region */
	uint len;
	/* The MultiProc id of the owner of the region */
	u16 owner_proc_id;
	/* Whether the region is valid */
	bool is_valid;
	/* Whether to perform cache operations for the region */
	bool cache_enable;
	/* The cache line size of the region */
	uint cache_line_size;
	/* Whether a heap is created for the region */
	bool create_heap;
	/* The name of the region */
	char *name;
};

/*
 * Module configuration structure
 */
struct sharedregion_config {
	uint cache_line_size;
	/*
	*  Worst-case cache line size
	*
	*  This is the default system cache line size for all modules.
	*  When a module puts structures in shared memory, this value is
	*  used to make sure items are aligned on a cache line boundary.
	*  If no cacheLineSize is specified for a region, it will use this
	*  value.
	*/

	u16 num_entries;
	/*
	*  The number of shared region table entries.
	*
	*  This value is used for calculating the number of bits for the offset.
	*  Note: This value must be the same across all processors in the
	*        system. Increasing this parameter will increase the footprint
	*        and the time for translating a pointer to a SRPtr.
	*/

	bool translate;
	/*
	*  This configuration parameter should be set to 'true'
	*  if and only if all shared memory regions are the same
	*  for all processors. If 'true', it results in a fast
	*  getPtr and getSRPtr.
	*/
};

/*
 *  Information stored on a per region basis
 */
struct sharedregion_region {
	struct sharedregion_entry entry;
	uint reserved_size;
	struct heapmemmp_object *heap;
};


/*
 *  Function to get the configuration
 */
int sharedregion_get_config(struct sharedregion_config *config);

/*
 *  Function to setup the SharedRegion module
 */
int sharedregion_setup(const struct sharedregion_config *config);

/*
 *  Function to destroy the SharedRegion module
 */
int sharedregion_destroy(void);

/*
 *  Creates a heap by owner of region for each SharedRegion.
 *  Function is called by ipc_start(). Requires that SharedRegion 0
 *  be valid before calling start().
 */
int sharedregion_start(void);

/*
 *  Function to stop Shared Region 0
 */
int sharedregion_stop(void);

/*
 *  Opens a heap, for non-owner processors, for each SharedRegion.
 */
int sharedregion_attach(u16 remote_proc_id);

/*
 *  Closes a heap, for non-owner processors, for each SharedRegion.
 */
int sharedregion_detach(u16 remote_proc_id);

/*
 *  Reserve shared region memory
 */
void *sharedregion_reserve_memory(u16 id, u32 size);

/*
 *  Reserve shared region memory
 */
void sharedregion_unreserve_memory(u16 id, u32 size);

/*
 *  Sets the entry at the specified region id(doesn't create heap)
 */
int _sharedregion_set_entry(u16 region_id, struct sharedregion_entry *entry);

/*
 *  Function to clear the reserved memory
 */
void sharedregion_clear_reserved_memory(void);

/*
 *  Return the region info
 */
void sharedregion_get_region_info(u16 i, struct sharedregion_region *region);

/*
 *  Clears the entry at the specified region id
 */
int sharedregion_clear_entry(u16 region_id);

/*
 *  Initializes the entry fields
 */
void sharedregion_entry_init(struct sharedregion_entry *entry);

/*
 *  Gets the cache line size for the specified region id
 */
uint sharedregion_get_cache_line_size(u16 region_id);

/*
 *  Gets the entry information for the specified region id
 */
int sharedregion_get_entry(u16 region_id, struct sharedregion_entry *entry);

/*
 *  Gets the heap associated with the specified region id
 */
void *sharedregion_get_heap(u16 region_id);

/*
 *  Gets the region id for the specified address
 */
u16 sharedregion_get_id(void *addr);

/*
 *  Gets the id of a region, given the name
 */
u16 sharedregion_get_id_by_name(char *name);

/*
 *  Gets the number of regions
 */
u16 sharedregion_get_num_regions(void);

/*
 *  Returns the address pointer associated with the shared region pointer
 */
void *sharedregion_get_ptr(u32 *srptr);

/*
 *  Returns the shared region pointer
 */
u32 *sharedregion_get_srptr(void *addr, u16 index);

/*
 *  whether cache enable was specified
 */
bool sharedregion_is_cache_enabled(u16 region_id);

/*
 *  Sets the entry at the specified region id
 */
int sharedregion_set_entry(u16 region_id, struct sharedregion_entry *entry);

/*
 *  Whether address translation is enabled
 */
bool sharedregion_translate_enabled(void);

#endif /* _SHAREDREGION_H */
