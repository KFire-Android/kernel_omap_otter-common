/*
 * gcmmu.h
 *
 * Copyright (C) 2010-2011 Vivante Corporation.
 *
 * This package is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * THIS PACKAGE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#ifndef GCMMU_H
#define GCMMU_H

#include <linux/gccore.h>
#include "gcmem.h"
#include "gcqueue.h"

/*******************************************************************************
 * Master table can be configured in 1KB mode with 256 maximum entries
 * or 4KB mode with 1024 maximum entries.
 *
 * Address bit allocation.
 * +------+------+--------+
 * | MTLB | STLB | Offset |
 * +------+------+--------+
 *
 * # of address bits | # of address bits | Page | Addressable | Total
 *          /        |          /        | size |  per one    | addressable
 * # of MTLB entries | # of STLB entries |      | MTLB entry  |
 * ------------------+-------------------+------+-------------+------------
 *       8 / 256     |      4 / 16       |  1MB |             |
 *                   |      8 / 256      | 64KB |     16MB    |    4GB
 *                   |     12 / 4096     |  4KB |             |
 * ------------------+-------------------+------+-------------+------------
 *      10 / 1024    |      6 / 64       | 64KB |             |
 *                   |     10 / 1024     |  4KB |      4MB    |    4GB
 */

/* Page size. */
#define GCMMU_PAGE_SIZE			4096

/* Master table definitions. */
#define GCMMU_MTLB_BITS			8
#define GCMMU_MTLB_ENTRY_NUM		(1 << GCMMU_MTLB_BITS)
#define GCMMU_MTLB_SIZE			(GCMMU_MTLB_ENTRY_NUM << 2)
#define GCMMU_MTLB_SHIFT		(32 - GCMMU_MTLB_BITS)
#define GCMMU_MTLB_MASK			(((1U << GCMMU_MTLB_BITS) - 1) \
						<< GCMMU_MTLB_SHIFT)

#if GCMMU_MTLB_BITS == 8
#	define GCMMU_MTLB_MODE		GCREG_MMU_CONFIGURATION_MODE_MODE1_K
#elif GCMMU_MTLB_BITS == 10
#	define GCMMU_MTLB_MODE		GCREG_MMU_CONFIGURATION_MODE_MODE4_K
#else
#	error Invalid GCMMU_MTLB_BITS.
#endif

#define GCMMU_MTLB_PRESENT_MASK		0x00000001
#define GCMMU_MTLB_EXCEPTION_MASK	0x00000002
#define GCMMU_MTLB_PAGE_SIZE_MASK	0x0000000C
#define GCMMU_MTLB_SLAVE_MASK		0xFFFFFFC0

#define GCMMU_MTLB_PRESENT		0x00000001
#define GCMMU_MTLB_EXCEPTION		0x00000002
#define GCMMU_MTLB_4K_PAGE		0x00000000

#define GCMMU_MTLB_ENTRY_VACANT		GCMMU_MTLB_EXCEPTION

/* Slave table definitions. */
#define GCMMU_STLB_BITS			12
#define GCMMU_STLB_ENTRY_NUM		(1 << GCMMU_STLB_BITS)
#define GCMMU_STLB_SIZE			(GCMMU_STLB_ENTRY_NUM << 2)
#define GCMMU_STLB_SHIFT		(32 - (GCMMU_MTLB_BITS \
						+ GCMMU_STLB_BITS))
#define GCMMU_STLB_MASK			(((1U << GCMMU_STLB_BITS) - 1) \
						<< GCMMU_STLB_SHIFT)

#define GCMMU_STLB_PRESENT_MASK		0x00000001
#define GCMMU_STLB_EXCEPTION_MASK	0x00000002
#define GCMMU_STLB_WRITEABLE_MASK	0x00000004
#define GCMMU_STLB_ADDRESS_MASK		0xFFFFF000

#define GCMMU_STLB_PRESENT		0x00000001
#define GCMMU_STLB_EXCEPTION		0x00000002
#define GCMMU_STLB_WRITEABLE		0x00000004

#define GCMMU_STLB_ENTRY_VACANT		GCMMU_STLB_EXCEPTION

/* Page offset definitions. */
#define GCMMU_OFFSET_BITS		(32 - GCMMU_MTLB_BITS - GCMMU_STLB_BITS)
#define GCMMU_OFFSET_MASK		((1U << GCMMU_OFFSET_BITS) - 1)

#define GCMMU_SAFE_ZONE_SIZE		64

/* STLB preallocation count. This value controls how many slave tables will be
 * allocated every time we run out of available slave tables during mapping.
 * The value must be between 1 and the total number of slave tables possible,
 * which is equal to 256 assuming 4KB page size. */
#define GCMMU_STLB_PREALLOC_COUNT	(GCMMU_MTLB_ENTRY_NUM / 4)


/*******************************************************************************
 * MMU structures.
 */

/* This union defines a location within MMU. */
union gcmmuloc {
	struct _loc {
		unsigned int stlb:GCMMU_STLB_BITS;
		unsigned int mtlb:GCMMU_MTLB_BITS;
	} loc;

	unsigned int absolute;
};

/* Arenas describe memory regions. Two lists of areanas are maintained:
 * one that defines a list of vacant arenas ready to map and the other
 * a list of already mapped arenas. */
struct gcmmuarena {
	/* Arena starting and ending points. */
	union gcmmuloc start;
	union gcmmuloc end;

	/* Number of pages. */
	unsigned int count;

	/* Mapped virtual pointer. */
	unsigned int address;

	/* Client's virtual pointer. */
	void *logical;

	/* Size of the mapped buffer. */
	unsigned int size;

	/* Page descriptor array. */
	struct page **pages;

	/* Prev/next arena. */
	struct list_head link;
};

/* MMU shared object. */
struct gcmmu {
	/* Access lock. */
	GCLOCK_TYPE lock;

	/* Reference count. */
	int refcount;

	/* One physical page used for MMU management. */
	struct gcpage gcpage;

	/* Physical command buffer. */
	unsigned int cmdbufphys;
	unsigned int *cmdbuflog;
	unsigned int cmdbufsize;

	/* Safe zone location. */
	unsigned int safezonephys;
	unsigned int *safezonelog;
	unsigned int safezonesize;

	/* Currently set master table. */
	unsigned int master;

	/* Available page allocation arenas. */
	struct list_head vacarena;
};

/* Slave table descriptor. */
struct gcmmustlb {
	unsigned int physical;
	unsigned int *logical;
};

struct gcmmustlbblock;
struct gcmmucontext {
	/* Access lock. */
	GCLOCK_TYPE lock;

	/* PID of the owner process. */
	pid_t pid;

	/* If marked as dirty, MMU cache needs to be flushed. */
	bool dirty;

	/* Master table allocation. */
	struct gcpage master;

	/* Slave table descriptors. */
	struct gcmmustlb slave[GCMMU_MTLB_ENTRY_NUM];
	struct gcmmustlbblock *slavealloc;

	/* MMU configuration. */
	union mmuconfig {
		struct gcregmmuconfiguration reg;
		unsigned int raw;
	} mmuconfig;

	unsigned long physical;

	/* Page mapping tracking. */
	struct list_head vacant;
	struct list_head allocated;

	/* Driver instance has only one set of command buffers that must be
	 * mapped the same exact way in all clients. This array stores
	 * pointers to arena structures of mapped storage buffers. */
	struct gcmmuarena *storagearray[GC_STORAGE_COUNT];

	/* Prev/next context. */
	struct list_head link;
};


/*******************************************************************************
 * MMU management API.
 */

struct gcmmuphysmem {
	/* Virtual pointer and offset of the first page to map. */
	unsigned int base;
	unsigned int offset;

	/* An array of physical addresses of the pages to map. */
	unsigned int count;
	pte_t *pages;

	/* 0 => system default. */
	int pagesize;
};

struct gccorecontext;

enum gcerror gcmmu_init(struct gccorecontext *gccorecontext);
void gcmmu_exit(struct gccorecontext *gccorecontext);

enum gcerror gcmmu_create_context(struct gccorecontext *gccorecontext,
				  struct gcmmucontext *gcmmucontext,
				  pid_t pid);
enum gcerror gcmmu_destroy_context(struct gccorecontext *gccorecontext,
				   struct gcmmucontext *gcmmucontext);

enum gcerror gcmmu_enable(struct gccorecontext *gccorecontext,
			  struct gcqueue *gcqueue);
enum gcerror gcmmu_set_master(struct gccorecontext *gccorecontext,
			      struct gcmmucontext *gcmmucontext);

enum gcerror gcmmu_map(struct gccorecontext *gccorecontext,
		       struct gcmmucontext *gcmmucontext,
		       struct gcmmuphysmem *mem,
		       struct gcmmuarena **mapped);
enum gcerror gcmmu_unmap(struct gccorecontext *gccorecontext,
			 struct gcmmucontext *gcmmucontext,
			 struct gcmmuarena *mapped);

enum gcerror gcmmu_flush(struct gccorecontext *gccorecontext,
			 struct gcmmucontext *gcmmucontext);
void gcmmu_flush_finalize(struct gccmdbuf *openentry,
			  struct gcmommuflush *flushlogical,
			  unsigned int flushaddress);

enum gcerror gcmmu_fixup(struct list_head *fixuplist,
			 unsigned int *data);

#endif
