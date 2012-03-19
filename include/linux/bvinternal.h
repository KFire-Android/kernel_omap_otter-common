/*
 * bvinternal.h
 *
 * Copyright (C) 2011 Texas Instruments, Inc.
 *
 * This package is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * THIS PACKAGE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

/*
 * This file contains definitions used by implementations of BLTsville
 * 2-D libraries.  It should not be used by clients.
 */

#ifndef BVINTERNAL_H
#define BVINTENRAL_H

/*
 * bvbuffmap - The bvbuffmap structure is used to track resources
 * associated with a buffer, such as a h/w MMU entry.  The implementations
 * add bvbuffmap objects when they allocate the resources.  Then when a
 * buffer is accessed, the implementations can regain access to the
 * associated resources.  The implementations allocate and populate this
 * structure when a bv_map() call is made.  It is used in subsequent
 * bv_blt() and bv_unmap() calls.  The latter frees the associated resource
 * and the structure (if applicable).  Note that a given resource might be
 * used by more than one implementation.
 */
struct bvbuffmap {
	unsigned int structsize; /* used to ID structure ver */

	/* function to unmap this resource */
	BVFN_UNMAP bv_unmap;

	unsigned long handle;	 /* resource-specific info */

	/* pointer to next resource mapping structure */
	struct bvbuffmap *nextmap;
};

#endif /* BVINTERNAL_H */
