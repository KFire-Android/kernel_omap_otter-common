/*
 * bv_gc2d-priv.h
 *
 * Copyright (C) 2011, Texas Instruments, Inc.
 *
 * This package is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * THIS PACKAGE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#ifndef BLTVILLE_BVGC2D_PRIV
#define BLTVILLE_BVGC2D_PRIV

#include <linux/bltsville.h>
#include <linux/gccore.h>

enum bverror bv_gc2d_map(struct bvbuffdesc *buffdesc);
enum bverror bv_gc2d_unmap(struct bvbuffdesc *buffdesc);
enum bverror bv_gc2d_blt(struct bvbltparams *bltparams);
void bv_gc2d_fillentry(void);
void bv_gc2d_clearentry(void);
int bv_gc2d_init(void);
int bv_gc2d_exit(void);

#endif
