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
#include "gccmdbuf.h"

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
	int irqline;			/* GPU IRQ line. */
	bool irqenabled;		/* IRQ ennabled flag. */
	bool isrroutine;		/* ISR installed flag. */
	void *regbase;			/* Virtual pointer to the GPU
					   register bank. */

	bool platdriver;		/* Platform driver install flag. */
	struct device *device;		/* gccore device. */
	struct device *bb2ddevice;	/* BB2D device. */
	struct omap_gcx_platform_data *plat; /* platform data */

	enum gcpower gcpower;		/* Current power mode. */
	struct clk *clk;		/* GPU clock descriptor. */
	bool clockenabled;		/* Clock enabled flag. */
	bool pulseskipping;		/* Pulse skipping enabled flag. */
	bool forceoff;			/* Enters OFF mode as opposed to SLOW
					   mode after every commit. */

	struct completion intready;	/* Interrupt comletion. */
	unsigned int intdata;		/* Interrupt acknowledge data. */

	struct gcmmu gcmmu;		/* MMU object. */

	struct list_head mmuctxlist;	/* List of active contexts. */
	struct list_head mmuctxvac;	/* Vacant contexts. */
	struct gcmmucontext *mmucontext;/* Current MMU context. */
	struct mutex mmucontextlock;

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

enum gcerror gc_set_power(struct gccorecontext *context,
				enum gcpower gcpower);
enum gcerror gc_get_power(void);


/*******************************************************************************
 * Interrupt.
 */

void gc_wait_interrupt(void);
unsigned int gc_get_interrupt_data(void);

#endif
