/*
 * gcmain.h
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

#ifndef GCMAIN_H
#define GCMAIN_H

#include <linux/gcx.h>
#include <linux/gccore.h>
#include "gcmmu.h"

#define GC_DEV_NAME	"gccore"


/*******************************************************************************
 * Power management modes.
 */

enum gcpower {
	GCPWR_UNKNOWN,
	GCPWR_ON,
	GCPWR_LOW,
	GCPWR_OFF
};


/*******************************************************************************
 * Driver context.
 */

struct omap_gcx_platform_data;

struct gccorecontext {
	/* GPU IRQ line. */
	int irqline;

	/* Capabilities and characteristics. */
	unsigned int gcmodel;
	unsigned int gcrevision;
	unsigned int gcdate;
	unsigned int gctime;
	union gcfeatures gcfeatures;
	union gcfeatures0 gcfeatures0;
	union gcfeatures1 gcfeatures1;
	union gcfeatures2 gcfeatures2;
	union gcfeatures3 gcfeatures3;

	/* Virtual pointer to the GPU register bank. */
	void *regbase;

	/* Platform driver install flag. */
	bool platdriver;
	struct omap_gcx_platform_data *plat;

	/* Pointers to the gccore and BB2D devices. */
	struct device *device;
	struct device *bb2ddevice;

	/* Current power mode. */
	enum gcpower gcpower;
	GCLOCK_TYPE powerlock;
	GCLOCK_TYPE resetlock;

	/* Current graphics pipe. */
	enum gcpipe gcpipe;

	/* Power mode flags. */
	bool clockenabled;
	int pulseskipping;

	/* MMU and command buffer managers. */
	struct gcmmu gcmmu;
	struct gcqueue gcqueue;

	/* MMU context lists. */
	struct list_head mmuctxlist;
	struct list_head mmuctxvac;
	GCLOCK_TYPE mmucontextlock;

	/* Device frequency scaling. */
	int opp_count;
	unsigned long *opp_freqs;
	unsigned long  cur_freq;
};


/*******************************************************************************
 * Register access.
 */

unsigned int gc_read_reg(unsigned int address);
void gc_write_reg(unsigned int address, unsigned int data);


/*******************************************************************************
 * Power management.
 */

enum gcpower gcpwr_get(void);
void gcpwr_set(struct gccorecontext *gccorecontext, enum gcpower gcpower);
void gcpwr_reset(struct gccorecontext *gccorecontext);
unsigned int gcpwr_get_speed(void);

#endif
