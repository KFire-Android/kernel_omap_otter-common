/*
 *  sysmemmgr.h
 *
 *  Manager for the Slave system memory. Slave system level memory is allocated
 *  through this module.
 *
 *  Copyright (C) 2009 Texas Instruments, Inc.
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


#ifndef _SYSTEMMEMORYMANAGER_H_
#define _SYSTEMMEMORYMANAGER_H_


/*!
 *  @def    SYSMEMMGR_MODULEID
 *  @brief  Module identifier for System memory manager.
 */
#define SYSMEMMGR_MODULEID		(0xb53d)

/*!
 *  @def    SYSMEMMGR_STATUSCODEBASE
 *  @brief  Error code base for system memory manager module.
 */
#define SYSMEMMGR_STATUSCODEBASE	(SYSMEMMGR_MODULEID << 12u)

/*!
 *  @def    SYSMEMMGR_MAKE_ERROR
 *  @brief  Macro to make error code.
 */
#define SYSMEMMGR_MAKE_ERROR(x)		((int)(0x80000000 + \
						(SYSMEMMGR_STATUSCODEBASE + \
						(x))))

/*!
 *  @def    SYSMEMMGR_MAKE_SUCCESS
 *  @brief  Macro to make success code.
 */
#define SYSMEMMGR_MAKE_SUCCESS(x)	(SYSMEMMGR_STATUSCODEBASE + (x))

/*!
 *  @def    SYSMEMMGR_E_CREATEALLOCATOR
 *  @brief  Static allocator creation failed.
 */
#define SYSMEMMGR_E_CREATEALLOCATOR	SYSMEMMGR_MAKE_ERROR(1)

/*!
 *  @def    SYSMEMMGR_E_CREATELOCK
 *  @brief  Mutex lock creation failed.
 */
#define SYSMEMMGR_E_CREATELOCK		SYSMEMMGR_MAKE_ERROR(2)

/*!
 *  @def    SYSMEMMGR_E_INVALIDSTATE
 *  @brief  Module is not initialized.
 */
#define SYSMEMMGR_E_INVALIDSTATE	SYSMEMMGR_MAKE_ERROR(3)

/*!
 *  @def    SYSMEMMGR_E_INVALIDARG
 *  @brief  Argument passed to a function is invalid.
 */
#define SYSMEMMGR_E_INVALIDARG		SYSMEMMGR_MAKE_ERROR(4)

/*!
 *  @def    SYSMEMMGR_E_BPAFREE
 *  @brief  Freeing to buddy allocator failed.
 */
#define SYSMEMMGR_E_BPAFREE		SYSMEMMGR_MAKE_ERROR(5)

/*!
 *  @def    SYSMEMMGR_E_MEMORY
 *  @brief  Memory allocation failed.
 */
#define SYSMEMMGR_E_MEMORY		SYSMEMMGR_MAKE_ERROR(6)

/*!
 *  @def    SYSMEMMGR_SUCCESS
 *  @brief  Operation successful.
 */
#define SYSMEMMGR_SUCCESS		SYSMEMMGR_MAKE_SUCCESS(0)

/*!
 *  @def    SYSMEMMGR_S_ALREADYSETUP
 *  @brief  Module already initialized.
 */
#define SYSMEMMGR_S_ALREADYSETUP	SYSMEMMGR_MAKE_SUCCESS(1)

/*!
 *  @def    SYSMEMMGR_S_DRVALREADYOPENED
 *  @brief  Internal OS Driver is already opened.
 */
#define SYSMEMMGR_S_DRVALREADYOPENED	SYSMEMMGR_MAKE_SUCCESS(2)

/*!
 *  @brief  Configuration data structure of system memory manager.
 */
struct sysmemmgr_config {
	u32 sizeof_valloc;
	/* Total size for virtual memory allocation */
	u32 sizeof_palloc;
	/* Total size for physical memory allocation */
	u32 static_phys_base_addr;
	/* Physical address of static memory region */
	u32 static_virt_base_addr;
	/* Virtual  address of static memory region */
	u32 static_mem_size;
	/* size of static memory region */
	u32 page_size;
	/* Page size */
	u32 event_no;
	/* Event number to be used */
};

/*!
 *  @brief  Flag used for allocating memory blocks.
 */
enum sysmemmgr_allocflag {
	sysmemmgr_allocflag_uncached = 0u,
	/* Flag used for allocating uncacheable block */
	sysmemmgr_allocflag_cached = 1u,
	/* Flag used for allocating cacheable block */
	sysmemmgr_allocflag_physical = 2u,
	/* Flag used for allocating physically contiguous block */
	sysmemmgr_allocflag_virtual = 3u,
	/* Flag used for allocating virtually contiguous block */
	sysmemmgr_allocflag_dma = 4u
	/* Flag used for allocating DMAable (physically contiguous) block */
};

/*!
 *  @brief  Flag used for translating address.
 */
enum sysmemmgr_xltflag {
	sysmemmgr_xltflag_kvirt2phys = 0x0001u,
	/* Flag used for converting Kernel virtual address to physical
	 *  address */
	sysmemmgr_xltflag_kvirt2uvirt = 0x0002u,
	/* Flag used for converting Kernel virtual address to user virtual
	 *  address */
	sysmemmgr_xltflag_uvirt2phys = 0x0003u,
	/* Flag used for converting user virtual address to physical address */
	sysmemmgr_xltflag_uvirt2kvirt = 0x0004u,
	/* Flag used for converting user virtual address to Kernel virtual
	 *  address */
	sysmemmgr_xltflag_phys2kvirt = 0x0005u,
	/* Flag used for converting physical address to user virtual address */
	sysmemmgr_xltflag_phys2uvirt = 0x0006u
	/* Flag used for converting physical address to Kernel virtual
	 *  address */
};


/* Function prototypes */
void sysmemmgr_get_config(struct sysmemmgr_config *config);

int sysmemmgr_setup(struct sysmemmgr_config *params);

int sysmemmgr_destroy(void);

int sysmemmgr_attach(u16 slave_id);

void *sysmemmgr_alloc(u32 size, enum sysmemmgr_allocflag flag);

int sysmemmgr_free(void *blk, u32 size, enum sysmemmgr_allocflag flag);

void *sysmemmgr_translate(void *srcAddr, enum sysmemmgr_xltflag flag);


#endif /* _SYSTEMMEMORYMANAGER_H_ */
