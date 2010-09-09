/*
 *  heap.h
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

#ifndef _HEAP_H_
#define _HEAP_H_

#include <linux/types.h>

/*
 *  Structure defining memory related statistics
 */
struct memory_stats{
	u32 total_size; /* Total memory size */
	u32 total_free_size; /* Total free memory size */
	u32 largest_free_size; /* Largest free memory size */
};

/*!
 *  ======== extendedstats ========
 *  Stats structure for the get_extended_stats API.
 *
 *  max_allocated_blocks: The maximum number of blocks allocated
 *  from this heap at any single point in time during the lifetime of this
 *  heap instance.
 *
 *  num_allocated_blocks: The total number of blocks currently
 *  allocated in this Heap instance.
 */
struct heap_extended_stats {
	u32 max_allocated_blocks;
	u32 num_allocated_blocks;
};

/*
 *  Structure defining config parameters for the heapbuf module
 */
struct heap_config {
	u32 max_name_len; /* Maximum length of name */
	bool track_max_allocs; /* Track the max number of allocated blocks */
};

/*
 *  Structure for the handle for the heap
 */
struct heap_object {
	void* (*alloc) (void *handle, u32 size, u32 align);
	int (*free) (void *handle, void *block, u32 size);
	void (*get_stats) (void *handle, struct memory_stats *stats);
	void (*get_extended_stats) (void *handle,
					struct heap_extended_stats *stats);
	bool (*is_blocking) (void *handle);
	void *obj;
};

/*
 *  Allocate a block
 */
void *sl_heap_alloc(void *handle, u32 size, u32 align);

/*
 *  Frees the block to this Heap
 */
int sl_heap_free(void *handle, void *block, u32 size);

/*
 * Get heap statistics
 */
void sl_heap_get_stats(void *handle, struct memory_stats *stats);

/*
 * Get heap extended statistics
 */
void sl_heap_get_extended_stats(void *hphandle,
				struct heap_extended_stats *stats);

/*
 * Indicates whether a heap will block on free or alloc
 */
bool sl_heap_is_blocking(void *hphandle);

#endif /* _HEAP_H_ */
