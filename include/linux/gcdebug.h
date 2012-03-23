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

struct dentry;

void gc_debug_init(struct dentry *debug_root);

void gc_debug_poweroff_cache(void);
void gc_debug_cache_gpu_status_from_irq(unsigned int acknowledge);

void gc_debug_blt(int srccount, int dstWidth, int dstHeight);


#endif
