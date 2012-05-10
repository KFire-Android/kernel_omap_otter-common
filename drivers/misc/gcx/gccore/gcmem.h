/*
 * gcmem.h
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

#ifndef GCMEM_H
#define GCMEM_H

struct gcpage {
	unsigned int order;
	struct page *pages;

	unsigned int size;
	unsigned int physical;
	unsigned int *logical;
};

enum gcerror gc_alloc_noncached(struct gcpage *p, unsigned int size);
void gc_free_noncached(struct gcpage *p);

enum gcerror gc_alloc_cached(struct gcpage *p, unsigned int size);
void gc_free_cached(struct gcpage *p);
void gc_flush_cached(struct gcpage *p);
void gc_flush_region(unsigned int physical, void *logical,
			unsigned int offset, unsigned int size);

#endif
