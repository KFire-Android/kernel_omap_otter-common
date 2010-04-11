/*
 * tcm_dbg.h
 *
 * Debug Utility definitions.
 *
 * Copyright (C) 2008-2010 Texas Instruments, Inc.
 *
 * This package is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * THIS PACKAGE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */


#ifndef _TILER_DEBUG_
#define _TILER_DEBUG_

/* TODO: once TCM debug prints are resolved, move these into tcm_utils.h */

#undef DEBUG
/* #define DEBUG_ENTRY_EXIT */

#undef DEBUG_ALL

#ifdef DEBUG
#define P(fmt, args...) printk(KERN_NOTICE "%s()::[%d]\n", __func__, __LINE__);
#else
#define P(fmt, args...)
#endif
#define P0 P

#define PE(fmt, args...) printk(KERN_ERR "%s()::[%d]\n", __func__, __LINE__);
#define P5 PE

#ifdef DEBUG_ALL
#define L1
#define L2
#define L3
#endif


#ifdef L1
#define P1      P
#else
#define P1(args...)
#endif

#ifdef L2
#define P2      P
#else
#define P2(args...)
#endif

#ifdef L3
#define P3      P
#else
#define P3(args...)
#endif

#endif

