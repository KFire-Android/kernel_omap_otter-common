/*
 * gccore.h
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

#ifndef GCCORE_H
#define GCCORE_H

/* FIXME/TODO: this define will go away in the final release. */
#define ENABLE_POLLING 1

/* gc-core platform device data structure */
struct gccore_plat_data {
	void __iomem *base;
	int irq;
};

/* IOCTL parameters. */
#define GCIOCTL_TYPE 0x5D
#define GCIOCTL_BASE 0x5D

/*******************************************************************************
 * API errors.
 */

#define GCERR_SETGRP(error, group) \
( \
	(enum gcerror) \
	((error & GCERR_GENERIC_MASK) | group) \
)

#define GCERR_GENERIC(error) \
( \
	(error & GCERR_GENERIC_MASK) << GCERR_GENERIC_SHIFT \
)

#define GCERR_GROUP(error) \
( \
	(error & GCERR_GROUP_MASK) << GCERR_GROUP_SHIFT \
)

enum gcerror {
	/***********************************************************************
	** No error / success.
	*/
	GCERR_NONE = 0,

	/***********************************************************************
	** Error code zones.
	*/

	/* Generic error code zone. These errors inform of the low level
	   reason of the faulure, but don't carry information about which
	   logical part of the code generated the error. */
	GCERR_GENERIC_SIZE = 12,
	GCERR_GENERIC_SHIFT = 0,
	GCERR_GENERIC_MASK
		= ((1 << GCERR_GENERIC_SIZE) - 1) << GCERR_GENERIC_SHIFT,

	/* Group error code zone. These errors inform about the logical part
	   of the code where the error occurred. */
	GCERR_GROUP_SIZE = (32 - GCERR_GENERIC_SIZE),
	GCERR_GROUP_SHIFT = GCERR_GENERIC_SIZE,
	GCERR_GROUP_MASK
		= ((1 << GCERR_GROUP_SIZE) - 1) << GCERR_GROUP_SHIFT,

	/***********************************************************************
	** Generic zone errors.
	*/

	GCERR_OODM			/* Out of dynamic memory. */
	= GCERR_GENERIC(1),

	GCERR_OOPM			/* Out of paged memory. */
	= GCERR_GENERIC(2),

	GCERR_PMMAP			/* Paged memory mapping. */
	= GCERR_GENERIC(3),

	GCERR_USER_READ			/* Reading user input. */
	= GCERR_GENERIC(4),

	GCERR_USER_WRITE		/* Writing user output. */
	= GCERR_GENERIC(5),

	/***********************************************************************
	** Group zone errors.
	*/

	/**** Command queue errors. */
	GCERR_CMD_ALLOC			/* Buffer allocation. */
	= GCERR_GROUP(0x01000),

	/**** MMU errors. */
	GCERR_MMU_CTXT_BAD		/* Invalid context. */
	= GCERR_GROUP(0x02000),

	GCERR_MMU_MTLB_ALLOC		/* MTLB allocation. */
	= GCERR_GROUP(0x02010),

	GCERR_MMU_MTLB_SET		/* MTLB setting. */
	= GCERR_GROUP(0x02020),

	GCERR_MMU_STLB_ALLOC		/* STLB allocation. */
	= GCERR_GROUP(0x02030),

	GCERR_MMU_STLBIDX_ALLOC		/* STLB index allocation. */
	= GCERR_GROUP(0x02040),

	GCERR_MMU_ARENA_ALLOC		/* Vacant arena allocation. */
	= GCERR_GROUP(0x02050),

	GCERR_MMU_OOM			/* No available arenas to allocate. */
	= GCERR_GROUP(0x02060),

	GCERR_MMU_SAFE_ALLOC		/* Safe zone allocation. */
	= GCERR_GROUP(0x02070),

	GCERR_MMU_INIT			/* MMU initialization. */
	= GCERR_GROUP(0x02080),

	GCERR_MMU_ARG			/* Invalid argument. */
	= GCERR_GROUP(0x02090),

	GCERR_MMU_CLIENT		/* Client initialization. */
	= GCERR_GROUP(0x020A0),

	GCERR_MMU_BUFFER_BAD		/* Invalid buffer to map. */
	= GCERR_GROUP(0x020B0),

	GCERR_MMU_PAGE_BAD		/* Bad page within the buffer. */
	= GCERR_GROUP(0x020C0),

	GCERR_MMU_DESC_ALLOC		/* Bad page within the buffer. */
	= GCERR_GROUP(0x020D0),

	GCERR_MMU_PHYS_ALLOC		/* Bad page within the buffer. */
	= GCERR_GROUP(0x020E0),

	GCERR_MMU_OFFSET		/* Bad buffer offset. */
	= GCERR_GROUP(0x020F0),
};

/*******************************************************************************
 * Commit API entry.
 */

struct gccommit;
struct gcbuffer;
struct gcfixup;

/* IOCTL entry point. */
#define GCIOCTL_COMMIT _IOWR(GCIOCTL_TYPE, GCIOCTL_BASE + 0, struct gccommit)

/* Entry point. */
int gc_commit(struct gccommit *gccommit);

/* Commit header; contains pointers to the head and the tail of a linked list
   of command buffers to execute. */
struct gccommit {
	enum gcerror gcerror;		/* Return status code. */
	struct gcbuffer *buffer;	/* Command buffer list. */
};

/* Command buffer header. */
#define GC_BUFFER_SIZE 81920
struct gcbuffer {
	struct gcfixup *fixuphead;	/* Address fixup list. */
	struct gcfixup *fixuptail;

	unsigned int pixelcount;	/* Number of pixels to be rendered. */

	unsigned int *head;		/* Pointers to the head and tail */
	unsigned int *tail;		/* of the command buffer. */

	unsigned int available;		/* Number of bytes available in the
					   buffer. */
	struct gcbuffer *next;		/* Pointer to the next commmand
					   buffer. */
};

/* Fixup entry. */
struct gcfixupentry {
	unsigned int dataoffset;	/* Offset into the commmand buffer
					   where fixup is to be performed. */
	unsigned int surfoffset;	/* Offset to be added to the base
					   address of the surface. */
};

/* Address fixup array. */
#define GC_FIXUP_MAX 1024
struct gcfixup {
	struct gcfixup *next;
	unsigned int count;
	struct gcfixupentry fixup[GC_FIXUP_MAX];
};

/*******************************************************************************
 * Map/unmap API entries.
 */

struct gcmap;

/* IOCTL entry points. */
#define GCIOCTL_MAP   _IOWR(GCIOCTL_TYPE, GCIOCTL_BASE + 1, struct gcmap)
#define GCIOCTL_UNMAP _IOWR(GCIOCTL_TYPE, GCIOCTL_BASE + 2, struct gcmap)

/* Entry point. */
int gc_map(struct gcmap *gcmap);
int gc_unmap(struct gcmap *gcmap);

struct gcmap {
	enum gcerror gcerror;		/* Return status code. */

	void *logical;			/* Pointer to the buffer. */
	size_t size;			/* Size of the buffer. */

	unsigned int handle;		/* Mapped handle of the buffer. */

	unsigned long pagecount;
	unsigned long *pagearray;
};

#endif
