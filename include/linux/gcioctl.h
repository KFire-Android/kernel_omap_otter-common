/*
 * gcioctl.h
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

#ifndef GCIOCTL_H
#define GCIOCTL_H

#include "gcerror.h"
#include <linux/bverror.h>

/* IOCTL parameters. */
#define GCIOCTL_TYPE 0x5D
#define GCIOCTL_BASE 0x5D

/*******************************************************************************
 * Commit API entry.
 */

struct gccommit;
struct gcbuffer;
struct gcfixup;

#define GCIOCTL_COMMIT _IOWR(GCIOCTL_TYPE, GCIOCTL_BASE + 0x10, struct gccommit)

/* Commit header; contains pointers to the head and the tail of a linked list
   of command buffers to execute. */
struct gccommit {
	enum gcerror gcerror;		/* Return status code. */
	struct gcbuffer *buffer;	/* Command buffer list. */
};

/* Command buffer header. */
#define GC_BUFFER_SIZE (128 * 1024)
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

#define GCIOCTL_MAP   _IOWR(GCIOCTL_TYPE, GCIOCTL_BASE + 0x20, struct gcmap)
#define GCIOCTL_UNMAP _IOWR(GCIOCTL_TYPE, GCIOCTL_BASE + 0x21, struct gcmap)

struct gcmap {
	enum gcerror gcerror;		/* Return status code. */
	unsigned int handle;		/* Mapped handle of the buffer. */

	void *logical;			/* Pointer to the buffer. */
	unsigned int size;		/* Size of the buffer. */
	unsigned long pagecount;
	unsigned long *pagearray;
};

/*******************************************************************************
 * BLTsville: blit API entry.
 */

struct gcbvblt;

#define GCIOCTL_BVBLT _IOWR(GCIOCTL_TYPE, GCIOCTL_BASE + 0x30, struct gcbvblt)

struct gcbvblt {
	enum bverror bverror;		/* Return status code. */
	struct bvbltparams *bltparams;	/* Blit parameters. */
	char *errdesc;			/* Blit error message. */
	int errdesclen;			/* Maximum length of the error
					   message. */
};

/*******************************************************************************
 * BLTsville: map/unmap API entries.
 */

struct gcbvmap;

#define GCIOCTL_BVMAP   _IOWR(GCIOCTL_TYPE, GCIOCTL_BASE + 0x40, struct gcbvmap)
#define GCIOCTL_BVUNMAP _IOWR(GCIOCTL_TYPE, GCIOCTL_BASE + 0x41, struct gcbvmap)

struct gcbvmap {
	enum bverror bverror;		/* Return status code. */
	struct bvbuffdesc *buffdesc;	/* Surface descriptor. */
};

#endif
