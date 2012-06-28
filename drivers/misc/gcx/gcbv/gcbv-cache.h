/*
 * gcbv-cache.h
 *
 * Copyright (C) 2012 Texas Instruments Corporation.
 *
 * This package is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * THIS PACKAGE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#ifndef GCBV_CACHE_H_
#define GCBV_CACHE_H_

enum bverror gcbvcacheop(int count, struct c2dmrgn rgn[],
		enum bvcacheop cacheop);

#endif /* GCBV_CACHE_H_ */
