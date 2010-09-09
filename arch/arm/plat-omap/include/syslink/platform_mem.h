/*
 *  platform_mem.c
 *
 * Target memory management interface implementation.
 *
 *  This abstracts the Memory management interface in the kernel
 *  code. Allocation, Freeing-up, copy and address translate are
 *  supported for the kernel memory management.
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

#ifndef _PLATFORM_MEM_H_
#define _PLATFORM_MEM_H_

#include <linux/types.h>

/*
 *  MEMORYOS_MODULEID
 *  Module ID for platform mem module
 */
#define PLATFORM_MEM_MODULEID				 (u16) 0x97D2

/*
 *  Enumerates the types of caching for memory regions
 */
enum platform_mem_cache_flags {
	PLATFORM_MEM_CACHE_FLAGS_DEFAULT  = 0x00000000,
	/* Default flags - Cached */
	PLATFORM_MEM_CACHE_FLAGS_CACHED   = 0x00010000,
	/* Cached memory */
	PLATFORM_MEM_CACHE_FLAGS_UNCACHED = 0x00020000,
	/* Uncached memory */
	PLATFORM_MEM_CACHE_FLAGS_END_VALUE = 0x00030000
	/* End delimiter indicating start of invalid values for this enum */
};

/*
 *  Enumerates the types of memory allocation
 */
enum platform_mem_mtype_flags{
	PLATFORM_MEM_MTYPE_FLAGS_DEFAULT  = 0x00000000,
	/* Default flags - virtually contiguous */
	PLATFORM_MEM_MTYPE_FLAGS_PHYSICAL = 0x00000001,
	/* Physically contiguous */
	PLATFORM_MEM_MTYPE_FLAGS_DMA	  = 0x00000002,
	/* Physically contiguous */
	PLATFORM_MEM_MTYPE_FLAGS_END_VALUE = 0x00000003
	/* End delimiter indicating start of invalid values for this enum */
};

/*
 *  Enumerates the types of translation
 */
enum memory_xlt_flags{
	PLATFORM_MEM_XLT_FLAGS_VIRT2PHYS = 0x00000000,
	/* Virtual to physical */
	PLATFORM_MEM_XLT_FLAGS_PHYS2VIRT = 0x00000001,
	/* Virtual to physical */
	PLATFORM_MEM_XLT_FLAGS_END_VALUE  = 0x00000002
	/* End delimiter indicating start of invalid values for this enum */
};

/*
 *  Structure containing information required for mapping a
 *  memory region.
 */
struct platform_mem_map_info {
	u32 src;
	/* Address to be mapped. */
	u32 size;
	/*  Size of memory region to be mapped. */
	u32 dst;
	/* Mapped address. */
	bool	 is_cached;
	/* Whether the mapping is to a cached area or uncached area. */
	void *drv_handle;
	/* Handle to the driver that is implementing the mmap call. Ignored for
	Kernel-side version. */
};

/*
 * Structure containing information required for unmapping a
 * memory region.
 */
struct platform_mem_unmap_info {
	u32  addr;
	/* Address to be unmapped.*/
	u32  size;
	/* Size of memory region to be unmapped.*/
	bool	is_cached;
	/* Whether the mapping is to a cached area or uncached area.  */
};

/*
 *  Structure containing information required for mapping a
 *		   memory region.
 */
#define memory_map_info   struct platform_mem_map_info

/*
 * Structure containing information required for unmapping a
 * memory region.
 */
#define memory_unmap_info struct platform_mem_unmap_info


/* =============================================================================
 *  APIs
 * =============================================================================
 */
/* Initialize the platform mem module. */
int platform_mem_setup(void);

/* Finalize the platform mem module. */
int  platform_mem_destroy(void);

/* Maps a memory area into virtual space. */
int  platform_mem_map(memory_map_info *map_info);

/* Unmaps a memory area into virtual space. */
int  platform_mem_unmap(memory_unmap_info *unmap_info);

/* Translate API */
void *platform_mem_translate(void *srcAddr, enum memory_xlt_flags flags);

#endif /* ifndef _PLATFORM_MEM_H_ */
