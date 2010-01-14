/*
 * dmm_reg.h
 *
 * DMM driver support functions for TI OMAP processors.
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
#ifndef DMM_REG_H
#define DMM_REG_H

#define BITS_32(in_NbBits) \
  ((((unsigned long)1 << in_NbBits) - 1) | ((unsigned long)1 << in_NbBits))

#define BITFIELD_32(in_UpBit, in_LowBit) \
  (BITS_32(in_UpBit) & ~((BITS_32(in_LowBit)) >> 1))

#define BITFIELD BITFIELD_32

extern void *dmm_base;
#endif

