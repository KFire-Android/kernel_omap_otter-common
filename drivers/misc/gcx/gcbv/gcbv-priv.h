/*
 * gcbv-priv.h
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

#ifndef GCBV_PRIV
#define GCBV_PRIV

#include <linux/bltsville.h>
#include <linux/gccore.h>

enum bverror gcbv_map(struct bvbuffdesc *buffdesc);
enum bverror gcbv_unmap(struct bvbuffdesc *buffdesc);
enum bverror gcbv_blt(struct bvbltparams *bltparams);
void gcbv_assign(void);
void gcbv_clear(void);

#endif
