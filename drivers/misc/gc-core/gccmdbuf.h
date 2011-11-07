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

#include "gcmmu.h"

#define ENABLE_GPU_COUNTERS	1
#define ENABLE_CMD_DEBUG	1

extern wait_queue_head_t gc_event;
extern int done;
extern u8 *reg_base;
#if ENABLE_CMD_DEBUG
#	define CMDBUFPRINT printk
#else
#	define CMDBUFPRINT(...)
#endif

int cmdbuf_init(void);
int cmdbuf_map(struct mmu2dcontext *ctxt);
int cmdbuf_alloc(u32 size, u32 **logical, u32 *physical);
int cmdbuf_flush(void);
void cmdbuf_dump(void);

u32 hw_read_reg(u32 address);
void hw_write_reg(u32 address, u32 data);

void gpu_id(void);
void gpu_status(char *function, int line, u32 acknowledge);

#endif
