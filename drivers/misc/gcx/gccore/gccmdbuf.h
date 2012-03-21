/*
 * gccmdbuf.h
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

#ifndef GCCMDBUF_H
#define GCCMDBUF_H

#include <linux/gccore.h>
#include "gcmmu.h"

enum gcerror cmdbuf_init(void);
enum gcerror cmdbuf_map(struct mmu2dcontext *ctxt);
enum gcerror cmdbuf_alloc(u32 size, void **logical, u32 *physical);
int cmdbuf_flush(void *logical);
void cmdbuf_physical(bool forcephysical);

#endif
