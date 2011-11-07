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

#if 1
#	define DBGPRINT printk
#else
#	define DBGPRINT(...)
#endif

#define ENABLE_POLLING 1

#define BLT _IOW('x', 100, u32)
#define MAP _IOWR('x', 101, u32)
#define UMAP _IOW('x', 102, u32)

extern u32 *cmdbuf_start_logical;
extern u32 cmdbuf_start_physical;
#endif
