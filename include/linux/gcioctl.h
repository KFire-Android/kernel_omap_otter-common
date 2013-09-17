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

#include "list.h"
#include "gcerror.h"
#include "gcreg.h"
#include "cache-2dmanager.h"
#include "bverror.h"

/* IOCTL parameters. */
#define GCIOCTL_TYPE 0x5D
#define GCIOCTL_BASE 0x5D


/*******************************************************************************
 * Capability query API entry.
 */

#define GCIOCTL_GETCAPS _IOWR(GCIOCTL_TYPE, GCIOCTL_BASE + 0x00, \
			      struct gcicaps)

/* GCIOCTL_CALLBACK_ALLOC / GCIOCTL_CALLBACK_FREE:
 *   To be able to use the callback mechanism each user space client must
 *   use the ALLOC/FREE APIs to manage a kernel side callback object
 *   represented by the handle member of struct gcicallback.
 *   ALLOC API allocates the object and returns the handle to it. */
struct gcicaps {
	/* Error code. */
	enum gcerror gcerror;

	/* Capabilities and characteristics. */
	unsigned int gcmodel;
	unsigned int gcrevision;
	unsigned int gcdate;
	unsigned int gctime;
	union gcfeatures gcfeatures;
	union gcfeatures0 gcfeatures0;
	union gcfeatures1 gcfeatures1;
	union gcfeatures2 gcfeatures2;
	union gcfeatures3 gcfeatures3;
};

/*******************************************************************************
 * Commit API entry.
 */

#define GCIOCTL_COMMIT _IOWR(GCIOCTL_TYPE, GCIOCTL_BASE + 0x10, \
			     struct gcicommit)

/* GPU graphics pipe definition. */
enum gcpipe {
	GCPIPE_UNKNOWN,
	GCPIPE_2D,
	GCPIPE_3D
};

/* Commit header; contains pointers to the head and the tail of a linked list
   of command buffers to execute. */
struct gcicommit {
	/* Return status code. */
	enum gcerror gcerror;

	/* Entry graphics pipe specifies the pipe the GPU is expected to be in
	 * for successfull execution of the specified command buffers in this
	 * call. Exit graphics pipe specifies the state of the GPU after the
	 * buffers are executed. */
	enum gcpipe entrypipe;
	enum gcpipe exitpipe;

	/* List of command buffers to be executed (gcbuffer). */
	struct list_head buffer;

	/* Pointer to the callback function to be called when the GPU completes
	 * execution of all buffers specified in this call. This member can be
	 * NULL if no callback is desired. callbackparam specifies data to be
	 * passed to the callback. */
	void (*callback) (void *callbackparam);
	void *callbackparam;

	/* Callback object handle allocated with GCIOCTL_CALLBACK_ALLOC API. */
	unsigned long handle;

	/* If asynchronous is set to true, the call returns immediately without
	 * waiting until all specified buffers have been executed. If set to
	 * false, the call does not return until execution is finished. */
	bool asynchronous;

	/* Scheduled unmappings (gcschedunmap). */
	struct list_head unmap;
};

/* Command buffer header. */
#define GC_BUFFER_SIZE (32 * 1024)
struct gcbuffer {
	/* Fixup list (gcfixup). */
	struct list_head fixup;

	/* Number of pixels to be rendered. */
	unsigned int pixelcount;

	/* Pointers to the head and tail of the command buffer list. */
	unsigned int *head;
	unsigned int *tail;

	/* Number of bytes available in the buffer for allocation. */
	unsigned int available;

	/* Commmand buffer list (gcbuffer). */
	struct list_head link;
};

/* Fixup entry. */
struct gcfixupentry {
	/* Offset into the commmand buffer where fixup is to be performed. */
	unsigned int dataoffset;

	/* Offset to be added to the translated address. */
	unsigned int surfoffset;
};

/* Address fixup array. */
#define GC_FIXUP_MAX 1024
struct gcfixup {
	/* Fixup list (gcfixup). */
	struct list_head link;

	/* Fixup array. */
	unsigned int count;
	struct gcfixupentry fixup[GC_FIXUP_MAX];
};

/* Defines a link list of scheduled unmappings. */
struct gcschedunmap {
	/* Map handle. */
	unsigned long handle;

	/* Previous/next unmapping info (gcschedunmap). */
	struct list_head link;
};


/*******************************************************************************
 * Map/unmap API entries.
 */

#define GCIOCTL_MAP   _IOWR(GCIOCTL_TYPE, GCIOCTL_BASE + 0x20, \
			    struct gcimap)
#define GCIOCTL_UNMAP _IOWR(GCIOCTL_TYPE, GCIOCTL_BASE + 0x21, \
			    struct gcimap)

struct gcimap {
	/* Return status code. */
	enum gcerror gcerror;

	/* Mapped handle of the buffer. */
	unsigned long handle;

	union {
		/* Pointer to the buffer to be mapped;
		 * used when pagearray is not provided (set to NULL). */
		void *logical;

		/* Page offset of the buffer to be mapped;
		 * used when pagearray is provided. */
		unsigned int offset;
	} buf;

	/* Size of a physical page, 0 for  default. */
	unsigned int pagesize;

	/* Pointer to array of physical pages. */
	unsigned long *pagearray;

	/* Size of the buffer to be mappped. */
	unsigned int size;
};


/*******************************************************************************
 * Cache manipulation API entries.
 */

#define GCIOCTL_CACHE _IOW(GCIOCTL_TYPE, GCIOCTL_BASE + 0x30, \
			   struct gcicache)

struct gcicache {
	/* Number of regions. */
	int count;

	/* The most regions that we deal with is 3. */
	struct c2dmrgn rgn[3];

	/* Direction of data. */
	int dir;
};


/*******************************************************************************
 * Callback API entry.
 */

#define GCIOCTL_CALLBACK_ALLOC _IOWR(GCIOCTL_TYPE, GCIOCTL_BASE + 0x40, \
				     struct gcicallback)
#define GCIOCTL_CALLBACK_FREE  _IOWR(GCIOCTL_TYPE, GCIOCTL_BASE + 0x41, \
				     struct gcicallback)
#define GCIOCTL_CALLBACK_WAIT  _IOWR(GCIOCTL_TYPE, GCIOCTL_BASE + 0x42, \
				     struct gcicallbackwait)
#define GCIOCTL_CALLBACK_ARM   _IOWR(GCIOCTL_TYPE, GCIOCTL_BASE + 0x43, \
				     struct gcicallbackarm)

/* GCIOCTL_CALLBACK_ALLOC / GCIOCTL_CALLBACK_FREE:
 *   To be able to use the callback mechanism each user space client must
 *   use the ALLOC/FREE APIs to manage a kernel side callback object
 *   represented by the handle member of struct gcicallback.
 *   ALLOC API allocates the object and returns the handle to it. */
struct gcicallback {
	/* Error code. */
	enum gcerror gcerror;

	/* Callback object handle. */
	unsigned long handle;
};

/* GCIOCTL_CALLBACK_WAIT:
 *   Called by the user level client to block and wait until the hardware
 *   has executed a callback that was previosuly scheduled for the handle. */
struct gcicallbackwait {
	/* Error code. */
	enum gcerror gcerror;

	/* Callback object handle. */
	unsigned long handle;

	/* Timeout in milliseconds. */
	unsigned long timeoutms;

	/* OUT: if the call succeeds, callback and callbackparam are
	 *      initialized with the callback to call. */
	void (*callback) (void *callbackparam);
	void *callbackparam;
};

/* GCIOCTL_CALLBACK_ARM:
 *   Called by the client to arm a callback. This is similar to what
 *   COMMIT API does, but in a separate API. */
struct gcicallbackarm {
	/* Return status code. */
	enum gcerror gcerror;

	/* Pointer to the callback function to be called when the GPU completes
	 * execution of all buffers specified in this call. This member can be
	 * NULL if no callback is desired. callbackparam specifies data to be
	 * passed to the callback. */
	void (*callback) (void *callbackparam);
	void *callbackparam;

	/* Callback object handle allocated with GCIOCTL_CALLBACK_ALLOC API. */
	unsigned long handle;
};

#endif
