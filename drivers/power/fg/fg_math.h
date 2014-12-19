/*
 * linux/drivers/power/fg/fg_math.h
 *
 * TI Fuel Gauge driver for Linux
 *
 * Copyright (C) 2008-2009 Texas Instruments, Inc.
 * Author: Texas Instruments, Inc.
 *
 * This package is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * THIS PACKAGE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#ifndef __FG_MATH_H
#define __FG_MATH_H

#include <linux/types.h>
#include "fg.h"
#include "fg_ocv.h"

#define MAX_CHAR		0x7F
#define MAX_UNSIGNED_CHAR	0xFF
#define MAX_INT			0x7FFF
#define MAX_UNSIGNED_INT	0xFFFF
#define MAX_INT8		0x7F
#define MAX_UINT8		0xFF

/* IIR Filter */
short filter(short y, short x, short a);

/* Diviation between 'size' array members */
unsigned short diff_array(short *arr, unsigned char size);

#endif
