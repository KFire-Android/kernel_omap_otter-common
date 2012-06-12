/*
 * gcdebug.h
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

#ifndef GCDEBUG_H
#define GCDEBUG_H

#define GCGPUSTATUS() \
	gc_debug_dump_status(__func__, __LINE__)

void gc_debug_init(void);
void gc_debug_shutdown(void);

void gc_debug_poweroff_cache(void);
void gc_debug_cache_gpu_status_from_irq(unsigned int acknowledge);
void gc_debug_dump_status(const char *function, int line);

void gc_debug_blt(int srccount, int dstWidth, int dstHeight);

#endif
