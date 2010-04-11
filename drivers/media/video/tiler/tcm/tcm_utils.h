/*
 * tcm_utils.h
 *
 * Utility functions for implementing TILER container managers.
 *
 * Copyright (C) 2009-2010 Texas Instruments, Inc.
 *
 * This package is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * THIS PACKAGE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#ifndef _TCM_UTILS_H
#define _TCM_UTILS_H

#include "tcm.h"

#define AREA_FMT   "(%03d %03d)-(%03d %03d)"
#define AREA(area) (area)->p0.x, (area)->p0.y, (area)->p1.x, (area)->p1.y
#define PA(level, msg, area) P##level(msg " " AREA_FMT "\n", AREA(area))

/* assign coordinates to area */
static inline
void assign(struct tcm_area *a, u16 x0, u16 y0, u16 x1, u16 y1)
{
	a->p0.x = x0;
	a->p0.y = y0;
	a->p1.x = x1;
	a->p1.y = y1;
}

static inline
void dump_area(struct tcm_area *area)
{
	printk(KERN_NOTICE AREA_FMT "\n", AREA(area));
}

#endif
