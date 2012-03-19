/*
 * gc2d.h
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

#ifndef GC2D_H
#define GC2D_H

#include <linux/bltsville.h>
#include <linux/bvinternal.h>
#include <linux/ocd.h>

enum bverror gcbv_map(struct bvbuffdesc *buffdesc);
enum bverror gcbv_unmap(struct bvbuffdesc *buffdesc);
enum bverror gcbv_blt(struct bvbltparams *bltparams);

#endif
