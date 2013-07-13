/*
 * gcbvdebug.h
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

#ifndef GCBVDEBUG_H
#define GCBVDEBUG_H

void gcbv_debug_init(void);
void gcbv_debug_shutdown(void);

void gcbv_debug_finalize_batch(unsigned int reason);

void gcbv_debug_blt(int srccount, int dstWidth, int dstHeight);
void gcbv_debug_scaleblt(bool scalex, bool scaley, bool singlepass);

#endif
