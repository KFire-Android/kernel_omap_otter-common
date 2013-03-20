/*
 * linux/drivers/power/fg/fg_math.c
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

#include <linux/types.h>
#include "fg_math.h"

/* IIR Filter */
short filter(short y, short x, short a)
{
	int l;

	l = (int) a * y;
	l += (int) (256 - a) * x;
	l /= 256;

	return (short) l;
}

/* Returns diviation between 'size' array members */
unsigned short diff_array(short *arr, unsigned char size)
{
	unsigned char i;
	unsigned int diff = 0;

	for (i = 0; i < size-1; i++)
		diff += abs(arr[i] - arr[i+1]);

	if (diff > MAX_UNSIGNED_INT)
		diff = MAX_UNSIGNED_INT;

	return (unsigned short) diff;
}
