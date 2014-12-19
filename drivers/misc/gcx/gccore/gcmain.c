/*
 * gcmain.c
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

#include <linux/platform_device.h>
#include <linux/uaccess.h>
#include <linux/pm_runtime.h>
#include <linux/delay.h>
#include <plat/omap_gcx.h>
#include <linux/opp.h>
#include <linux/io.h>
#include <plat/omap_hwmod.h>
#include <plat/omap-pm.h>
#include "gcmain.h"

#define GCZONE_NONE		0
#define GCZONE_ALL		(~0U)
#define GCZONE_INIT		(1 << 0)
#define GCZONE_CONTEXT		(1 << 1)
#define GCZONE_POWER		(1 << 2)
#define GCZONE_COMMIT		(1 << 3)
#define GCZONE_MAPPING		(1 << 4)
#define GCZONE_PROBE		(1 << 5)
#define GCZONE_CALLBACK		(1 << 6)
#define GCZONE_FREQSCALE	(1 << 7)

GCDBG_FILTERDEF(core, GCZONE_NONE,
		"init",
		"context",
		"power",
		"commit",
		"mapping",
		"probe",
		"callback",
		"freqscale")


#if !defined(GC_ENABLE_SUSPEND)
#define GC_ENABLE_SUSPEND 1
#endif

#if !defined(CONFIG_HAS_EARLYSUSPEND)
#define CONFIG_HAS_EARLYSUSPEND 0
#endif

/* Driver private data. */
static struct gccorecontext g_context;


/*******************************************************************************
 * Context management.
 */

static enum gcerror find_context(struct gccorecontext *gccorecontext,
				 bool fromuser,
				 struct gcmmucontext **gcmmucontext)
{
	enum gcerror gcerror = GCERR_NONE;
	struct list_head *ctxhead;
	struct gcmmucontext *temp = NULL;
	pid_t pid;

	GCENTER(GCZONE_CONTEXT);

	/* Get current PID. */
	pid = fromuser ? current->tgid : 0;

	/* Search the list. */
	GCDBG(GCZONE_CONTEXT, "scanning context records for pid %d.\n", pid);

	/* Try to locate the record. */
	list_for_each(ctxhead, &gccorecontext->mmuctxlist) {
		temp = list_entry(ctxhead, struct gcmmucontext, link);
		if (temp->pid == pid) {
			/* Success. */
			GCDBG(GCZONE_CONTEXT, "context is found @ 0x%08X\n",
				(unsigned int) temp);

			goto exit;
		}
	}

	/* Get new record. */
	if (list_empty(&gccorecontext->mmuctxvac)) {
		GCDBG(GCZONE_CONTEXT, "not found, allocating.\n");

		temp = kmalloc(sizeof(struct gcmmucontext), GFP_KERNEL);
		if (temp == NULL) {
			GCERR("out of memory.\n");
			gcerror = GCERR_SETGRP(GCERR_OODM,
						GCERR_IOCTL_CTX_ALLOC);
			goto fail;
		}

		GCDBG(GCZONE_CONTEXT, "allocated @ 0x%08X\n",
			(unsigned int) temp);
	} else {
		ctxhead = gccorecontext->mmuctxvac.next;
		temp = list_entry(ctxhead, struct gcmmucontext, link);
		list_del(ctxhead);

		GCDBG(GCZONE_CONTEXT, "not found, reusing vacant @ 0x%08X\n",
			(unsigned int) temp);
	}

	gcerror = gcmmu_create_context(gccorecontext, temp, pid);
	if (gcerror != GCERR_NONE)
		goto fail;

	/* Add the context to the list. */
	list_add(&temp->link, &gccorecontext->mmuctxlist);

exit:
	*gcmmucontext = temp;

	GCEXIT(GCZONE_CONTEXT);
	return GCERR_NONE;

fail:
	if (temp != NULL) {
		gcmmu_destroy_context(gccorecontext, temp);
		list_add(&temp->link, &gccorecontext->mmuctxvac);
	}

	GCEXITARG(GCZONE_CONTEXT, "gcerror = 0x%08X\n", gcerror);
	return gcerror;
}

static void destroy_mmu_context(struct gccorecontext *gccorecontext)
{
	struct list_head *head;
	struct gcmmucontext *temp;

	/* Free vacant entry list. */
	while (!list_empty(&gccorecontext->mmuctxvac)) {
		head = gccorecontext->mmuctxvac.next;
		temp = list_entry(head, struct gcmmucontext, link);
		list_del(head);
		kfree(temp);
	}

	/* Free active contexts. */
	while (!list_empty(&gccorecontext->mmuctxlist)) {
		head = gccorecontext->mmuctxlist.next;
		temp = list_entry(head, struct gcmmucontext, link);
		gcmmu_destroy_context(gccorecontext, temp);
		list_del(head);
		kfree(temp);
	}
}

struct device *gc_get_dev(void)
{
	return g_context.device;
}
EXPORT_SYMBOL(gc_get_dev);


/*******************************************************************************
** Register access.
*/

unsigned int gc_read_reg(unsigned int address)
{
	return readl((unsigned char *) g_context.regbase + address);
}

void gc_write_reg(unsigned int address, unsigned int data)
{
	writel(data, (unsigned char *) g_context.regbase + address);
}


/*******************************************************************************
 * Power management.
 */

static void gcpwr_enable_clock(struct gccorecontext *gccorecontext)
{
	bool ctxlost;

	GCENTER(GCZONE_POWER);

	ctxlost = gccorecontext->plat->was_context_lost(gccorecontext->device);

	if (!gccorecontext->clockenabled) {
		/* Enable the clock. */
		pm_runtime_get_sync(gccorecontext->device);

		/* Signal software not idle. */
		gc_write_reg(GC_GP_OUT0_Address, 0);

		/* Clock enabled. */
		gccorecontext->clockenabled = true;
	} else if (ctxlost) {
		GCDBG(GCZONE_POWER, "hardware context lost.\n");
		if (gc_read_reg(GC_GP_OUT0_Address)) {
			GCDBG(GCZONE_POWER, "reset idle register.\n");
			gc_write_reg(GC_GP_OUT0_Address, 0);
		}
	}

	GCDBG(GCZONE_POWER, "clock %s.\n",
		gccorecontext->clockenabled ? "enabled" : "disabled");

	if (ctxlost || (gccorecontext->gcpower == GCPWR_UNKNOWN))
		gcpwr_reset(gccorecontext);

	GCEXIT(GCZONE_POWER);
}

static void gcpwr_disable_clock(struct gccorecontext *gccorecontext)
{
	GCENTER(GCZONE_POWER);

	if (gccorecontext->clockenabled) {
		gc_debug_poweroff_cache();

		/* Signal software idle. */
		gc_write_reg(GC_GP_OUT0_Address, 1);

		/* Disable the clock. */
		pm_runtime_put_sync(gccorecontext->device);

		/* Clock disabled. */
		gccorecontext->clockenabled = false;
	}

	GCDBG(GCZONE_POWER, "clock %s.\n",
		gccorecontext->clockenabled ? "enabled" : "disabled");

	GCEXIT(GCZONE_POWER);
}

static void gcpwr_scale(struct gccorecontext *gccorecontext, int index)
{
	int ret;

	GCENTERARG(GCZONE_FREQSCALE, "index=%d\n", index);

	if ((index < 0) || (index >= gccorecontext->opp_count)) {
		GCERR("invalid index %d.\n", index);
		goto exit;
	}

	if ((gccorecontext->plat == NULL) ||
	    (gccorecontext->plat->scale_dev == NULL)) {
		GCERR("scale interface is not initialized.\n");
		goto exit;
	}

	if (gccorecontext->cur_freq == gccorecontext->opp_freqs[index])
		goto exit;

	ret = gccorecontext->plat->scale_dev(gccorecontext->bb2ddevice,
					     gccorecontext->opp_freqs[index]);
	if (ret != 0) {
		GCERR("failed to scale the device.\n");
		goto exit;
	}

	gccorecontext->cur_freq = gccorecontext->opp_freqs[index];
	GCDBG(GCZONE_FREQSCALE, "frequency set to %dMHz\n",
	      gccorecontext->cur_freq / 1000 / 1000);

exit:
	GCEXIT(GCZONE_FREQSCALE);
}

static void gcpwr_set_pulse_skipping(unsigned int pulsecount)
{
	union gcclockcontrol gcclockcontrol;

	GCENTER(GCZONE_POWER);

	/* Set the pulse skip value. */
	gcclockcontrol.raw = 0;
	gcclockcontrol.reg.pulsecount = pulsecount;

	/* Initiate loading. */
	gcclockcontrol.reg.pulseset = 1;
	GCDBG(GCZONE_POWER, "pulse skip = 0x%08X\n", gcclockcontrol.raw);
	gc_write_reg(GCREG_HI_CLOCK_CONTROL_Address, gcclockcontrol.raw);

	/* Lock the value. */
	gcclockcontrol.reg.pulseset = 0;
	GCDBG(GCZONE_POWER, "pulse skip = 0x%08X\n", gcclockcontrol.raw);
	gc_write_reg(GCREG_HI_CLOCK_CONTROL_Address, gcclockcontrol.raw);

	GCEXIT(GCZONE_POWER);
}

static void gcpwr_enable_pulse_skipping(struct gccorecontext *gccorecontext)
{
	GCENTER(GCZONE_POWER);

	if (!gccorecontext->clockenabled)
		goto exit;

	if (gccorecontext->pulseskipping != 1) {
		/* Set the lowest frequency. */
		gcpwr_scale(gccorecontext, 0);

		/* Set 1 clock pulse for every 64 clocks. */
		gcpwr_set_pulse_skipping(1);

		/* Pulse skipping enabled. */
		gccorecontext->pulseskipping = 1;
	}

	GCDBG(GCZONE_POWER, "pulse skipping %s.\n",
	      gccorecontext->pulseskipping ? "enabled" : "disabled");

exit:
	GCEXIT(GCZONE_POWER);
}

static void gcpwr_disable_pulse_skipping(struct gccorecontext *gccorecontext)
{
	GCENTER(GCZONE_POWER);

	if (!gccorecontext->clockenabled)
		goto exit;

	if (gccorecontext->pulseskipping != 0) {
		/* Set the maximum frequency. */
		gcpwr_scale(gccorecontext, gccorecontext->opp_count - 1);

		/* Set full speed. */
		gcpwr_set_pulse_skipping(64);

		/* Pulse skipping disabled. */
		gccorecontext->pulseskipping = 0;
	}

	GCDBG(GCZONE_POWER, "pulse skipping %s.\n",
	      gccorecontext->pulseskipping ? "enabled" : "disabled");

exit:
	GCEXIT(GCZONE_POWER);
}

void gcpwr_set(struct gccorecontext *gccorecontext, enum gcpower gcpower)
{
	GCENTER(GCZONE_POWER);

	GCLOCK(&gccorecontext->powerlock);

	if (gcpower != gccorecontext->gcpower) {
		switch (gcpower) {
		case GCPWR_ON:
			gcpwr_enable_clock(gccorecontext);
			gcpwr_disable_pulse_skipping(gccorecontext);
			break;

		case GCPWR_LOW:
			gcpwr_enable_clock(gccorecontext);
			gcpwr_enable_pulse_skipping(gccorecontext);
			break;

		case GCPWR_OFF:
			gcpwr_enable_pulse_skipping(gccorecontext);
			gcpwr_disable_clock(gccorecontext);
			break;

		default:
			GCERR("unsupported power mode %d.\n", gcpower);
			goto exit;
		}

		GCDBG(GCZONE_POWER, "power state %d --> %d\n",
		      gccorecontext->gcpower, gcpower);

		/* Set new power state. */
		gccorecontext->gcpower = gcpower;
	}

exit:
	GCUNLOCK(&gccorecontext->powerlock);

	GCEXIT(GCZONE_POWER);
}

enum gcpower gcpwr_get(void)
{
	return g_context.gcpower;
}

void gcpwr_reset(struct gccorecontext *gccorecontext)
{
	union gcclockcontrol gcclockcontrol;
	union gcidle gcidle;

	GCENTER(GCZONE_POWER);

	GCLOCK(&gccorecontext->resetlock);

	/* Read current clock control value. */
	gcclockcontrol.raw
		= gc_read_reg(GCREG_HI_CLOCK_CONTROL_Address);

	while (true) {
		/* Isolate the GPU. */
		gcclockcontrol.reg.isolate = 1;
		gc_write_reg(GCREG_HI_CLOCK_CONTROL_Address,
				gcclockcontrol.raw);

		/* Set soft reset. */
		gcclockcontrol.reg.reset = 1;
		gc_write_reg(GCREG_HI_CLOCK_CONTROL_Address,
				gcclockcontrol.raw);

		/* Wait for reset. */
		msleep(1);

		/* Reset soft reset bit. */
		gcclockcontrol.reg.reset = 0;
		gc_write_reg(GCREG_HI_CLOCK_CONTROL_Address,
				gcclockcontrol.raw);

		/* Reset GPU isolation. */
		gcclockcontrol.reg.isolate = 0;
		gc_write_reg(GCREG_HI_CLOCK_CONTROL_Address,
				gcclockcontrol.raw);

		/* Read idle register. */
		gcidle.raw = gc_read_reg(GCREG_HI_IDLE_Address);

		/* Try resetting again if FE not idle. */
		if (!gcidle.reg.fe) {
			GCERR("FE NOT IDLE\n");
			continue;
		}

		/* Read reset register. */
		gcclockcontrol.raw
			= gc_read_reg(GCREG_HI_CLOCK_CONTROL_Address);

		/* Try resetting again if 2D is not idle. */
		if (!gcclockcontrol.reg.idle2d) {
			GCERR("2D NOT IDLE\n");
			continue;
		}

		/* GPU is idle. */
		break;
	}

	/* Pulse skipping disabled. */
	gccorecontext->pulseskipping = false;

	GCUNLOCK(&gccorecontext->resetlock);

	GCEXIT(GCZONE_POWER);
}

unsigned int gcpwr_get_speed(void)
{
	struct gccorecontext *gccorecontext = &g_context;
	static const int seccount = 2;
	unsigned int cyclecount;
	unsigned int speedmhz = 0;

	GCLOCK(&gccorecontext->powerlock);

	if (gccorecontext->gcpower == GCPWR_ON) {
		/* Reset cycle counter and sleep. */
		gc_write_reg(GC_TOTAL_CYCLES_Address, 0);
		msleep(seccount * 1000);

		/* Read the cycle counter and compute the speed. */
		cyclecount = gc_read_reg(GC_TOTAL_CYCLES_Address);
		speedmhz = cyclecount / 1000 / 1000 / seccount;
	}

	GCUNLOCK(&gccorecontext->powerlock);

	return speedmhz;
}

/*******************************************************************************
 * Public API.
 */

void gc_caps(struct gcicaps *gcicaps)
{
	struct gccorecontext *gccorecontext = &g_context;

	/* Copy capabilities. */
	gcicaps->gcmodel = gccorecontext->gcmodel;
	gcicaps->gcrevision = gccorecontext->gcrevision;
	gcicaps->gcdate = gccorecontext->gcdate;
	gcicaps->gctime = gccorecontext->gctime;
	gcicaps->gcfeatures = gccorecontext->gcfeatures;
	gcicaps->gcfeatures0 = gccorecontext->gcfeatures0;
	gcicaps->gcfeatures1 = gccorecontext->gcfeatures1;
	gcicaps->gcfeatures2 = gccorecontext->gcfeatures2;
	gcicaps->gcfeatures3 = gccorecontext->gcfeatures3;

	/* Success. */
	gcicaps->gcerror = GCERR_NONE;
}

void gc_commit(struct gcicommit *gcicommit, bool fromuser)
{
	struct gccorecontext *gccorecontext = &g_context;
	struct gcmmucontext *gcmmucontext;
	struct gcbuffer *gcbuffer;
	unsigned int buffersize;
	unsigned int *logical;
	unsigned int address;
	struct gcmopipesel *gcmopipesel;
	struct gcschedunmap *gcschedunmap;
	struct list_head *head;

	GCENTER(GCZONE_COMMIT);

	GCLOCK(&gccorecontext->mmucontextlock);

	/* Validate pipe values. */
	if ((gcicommit->entrypipe != GCPIPE_2D) &&
		(gcicommit->entrypipe != GCPIPE_3D)) {
		gcicommit->gcerror = GCERR_CMD_ENTRY_PIPE;
		goto exit;
	}

	if ((gcicommit->exitpipe != GCPIPE_2D) &&
		(gcicommit->exitpipe != GCPIPE_3D)) {
		gcicommit->gcerror = GCERR_CMD_EXIT_PIPE;
		goto exit;
	}

	/* Locate the client entry. */
	gcicommit->gcerror = find_context(gccorecontext, fromuser,
					  &gcmmucontext);
	if (gcicommit->gcerror != GCERR_NONE)
		goto exit;

	/* Set the master table. */
	gcicommit->gcerror = gcmmu_set_master(gccorecontext, gcmmucontext);
	if (gcicommit->gcerror != GCERR_NONE)
		goto exit;

	/* Set the correct graphics pipe. */
	if (gccorecontext->gcpipe != gcicommit->entrypipe) {
		static struct gcregpipeselect gcregpipeselect[] = {
			/* GCPIPE_UNKNOWN */
			{ 0, 0 },

			/* GCPIPE_2D */
			{ GCREG_PIPE_SELECT_PIPE_PIPE2D, 0 },

			/* GCPIPE_2D */
			{ GCREG_PIPE_SELECT_PIPE_PIPE2D, 0 }
		};

		GCDBG(GCZONE_COMMIT, "allocating space for pipe switch.\n");
		gcicommit->gcerror = gcqueue_alloc(gccorecontext, gcmmucontext,
						  sizeof(struct gcmopipesel),
						  (void **) &gcmopipesel, NULL);
		if (gcicommit->gcerror != GCERR_NONE)
			goto exit;

		gcmopipesel->pipesel_ldst = gcmopipesel_pipesel_ldst;
		gcmopipesel->pipesel.reg
			= gcregpipeselect[gcicommit->entrypipe];
	}

	/* Update the current pipe. */
	gccorecontext->gcpipe = gcicommit->exitpipe;

	/* Go through all buffers one at a time. */
	list_for_each(head, &gcicommit->buffer) {
		gcbuffer = list_entry(head, struct gcbuffer, link);
		GCDBG(GCZONE_COMMIT, "gcbuffer = 0x%08X\n",
		      (unsigned int) gcbuffer);

		/* Flush MMU. */
		gcmmu_flush(gccorecontext, gcmmucontext);

		/* Compute the size of the command buffer. */
		buffersize
			= (unsigned char *) gcbuffer->tail
			- (unsigned char *) gcbuffer->head;

		GCDBG(GCZONE_COMMIT, "buffersize = %d\n", buffersize);

		/* Reserve command buffer space. */
		GCDBG(GCZONE_COMMIT, "allocating command buffer space.\n");
		gcicommit->gcerror = gcqueue_alloc(gccorecontext, gcmmucontext,
						  buffersize,
						  (void **) &logical,
						  &address);
		if (gcicommit->gcerror != GCERR_NONE)
			goto exit;

		if (fromuser) {
			/* Copy command buffer. */
			if (copy_from_user(logical, gcbuffer->head,
						buffersize)) {
				GCERR("failed to read data.\n");
				gcicommit->gcerror = GCERR_USER_READ;
				goto exit;
			}
		} else {
			memcpy(logical, gcbuffer->head, buffersize);
		}

		/* Process fixups. */
		gcicommit->gcerror = gcmmu_fixup(&gcbuffer->fixup, logical);
		if (gcicommit->gcerror != GCERR_NONE)
			goto exit;
	}

	/* Add the callback. */
	if (gcicommit->callback != NULL) {
		gcicommit->gcerror = gcqueue_callback(gccorecontext,
						     gcmmucontext,
						     gcicommit->callback,
						     gcicommit->callbackparam);
		if (gcicommit->gcerror != GCERR_NONE)
			goto exit;
	}

	/* Process unmappings. */
	list_for_each(head, &gcicommit->unmap) {
		gcschedunmap = list_entry(head, struct gcschedunmap, link);
		gcicommit->gcerror = gcqueue_schedunmap(gccorecontext,
						       gcmmucontext,
						       gcschedunmap->handle);
		if (gcicommit->gcerror != GCERR_NONE)
			goto exit;
	}

	/* Execute the buffer. */
	gcicommit->gcerror = gcqueue_execute(gccorecontext, false,
					     gcicommit->asynchronous);

exit:
	GCUNLOCK(&gccorecontext->mmucontextlock);

	GCEXITARG(GCZONE_COMMIT, "gc%s = 0x%08X\n",
		(gcicommit->gcerror == GCERR_NONE) ? "result" : "error",
		gcicommit->gcerror);
}
EXPORT_SYMBOL(gc_commit);

void gc_map(struct gcimap *gcimap, bool fromuser)
{
	struct gccorecontext *gccorecontext = &g_context;
	struct gcmmucontext *gcmmucontext;
	struct gcmmuphysmem mem;
	struct gcmmuarena *mapped = NULL;

	GCENTER(GCZONE_MAPPING);

	GCLOCK(&gccorecontext->mmucontextlock);

	/* Locate the client entry. */
	gcimap->gcerror = find_context(gccorecontext,
				       fromuser,
				       &gcmmucontext);
	if (gcimap->gcerror != GCERR_NONE)
		goto exit;

	GCDBG(GCZONE_MAPPING, "map client buffer\n");

	/* Initialize the mapping parameters. */
	if (gcimap->pagearray == NULL) {
		mem.base = ((u32) gcimap->buf.logical) & ~(PAGE_SIZE - 1);
		mem.offset = ((u32) gcimap->buf.logical) & (PAGE_SIZE - 1);
		mem.pages = NULL;

		GCDBG(GCZONE_MAPPING, "  logical = 0x%08X\n",
		      (unsigned int) gcimap->buf.logical);
	} else {
		mem.base = 0;
		mem.offset = gcimap->buf.offset;
		mem.pages = gcimap->pagearray;

		GCDBG(GCZONE_MAPPING, "  pagearray = 0x%08X\n",
		      (unsigned int) gcimap->pagearray);
	}

	GCDBG(GCZONE_MAPPING, "  size = %d\n", gcimap->size);

	mem.count = DIV_ROUND_UP(gcimap->size + mem.offset, PAGE_SIZE);
	mem.pagesize = gcimap->pagesize ? gcimap->pagesize : PAGE_SIZE;

	/* Map the buffer. */
	gcimap->gcerror = gcmmu_map(gccorecontext, gcmmucontext, &mem, &mapped);
	if (gcimap->gcerror != GCERR_NONE)
		goto exit;

	gcimap->handle = (unsigned int) mapped;

	GCDBG(GCZONE_MAPPING, "  mapped address = 0x%08X\n", mapped->address);
	GCDBG(GCZONE_MAPPING, "  handle = 0x%08X\n", (unsigned int) mapped);

exit:
	GCUNLOCK(&gccorecontext->mmucontextlock);

	GCEXITARG(GCZONE_MAPPING, "gc%s = 0x%08X\n",
		(gcimap->gcerror == GCERR_NONE) ? "result" : "error",
		gcimap->gcerror);
}
EXPORT_SYMBOL(gc_map);

void gc_unmap(struct gcimap *gcimap, bool fromuser)
{
	struct gccorecontext *gccorecontext = &g_context;
	struct gcmmucontext *gcmmucontext;

	GCENTER(GCZONE_MAPPING);

	GCLOCK(&gccorecontext->mmucontextlock);

	/* Locate the client entry. */
	gcimap->gcerror = find_context(gccorecontext,
				       fromuser,
				       &gcmmucontext);
	if (gcimap->gcerror != GCERR_NONE)
		goto exit;

	GCDBG(GCZONE_MAPPING, "unmap client buffer\n");
	GCDBG(GCZONE_MAPPING, "  handle = 0x%08X\n", gcimap->handle);

	/* Schedule unmapping. */
	gcimap->gcerror = gcqueue_schedunmap(gccorecontext, gcmmucontext,
					    gcimap->handle);
	if (gcimap->gcerror != GCERR_NONE)
		goto exit;

	/* Execute the buffer. */
	gcimap->gcerror = gcqueue_execute(gccorecontext, false, false);
	if (gcimap->gcerror != GCERR_NONE)
		goto exit;

	/* Invalidate the handle. */
	gcimap->handle = ~0U;

exit:
	GCUNLOCK(&gccorecontext->mmucontextlock);

	GCEXITARG(GCZONE_MAPPING, "gc%s = 0x%08X\n",
		(gcimap->gcerror == GCERR_NONE) ? "result" : "error",
		gcimap->gcerror);
}
EXPORT_SYMBOL(gc_unmap);

void gc_callback(struct gcicallbackarm *gcicallbackarm, bool fromuser)
{
	struct gccorecontext *gccorecontext = &g_context;
	struct gcmmucontext *gcmmucontext;

	GCENTER(GCZONE_CALLBACK);

	GCLOCK(&gccorecontext->mmucontextlock);

	/* Locate the client entry. */
	gcicallbackarm->gcerror = find_context(gccorecontext, fromuser,
					       &gcmmucontext);
	if (gcicallbackarm->gcerror != GCERR_NONE)
		goto exit;

	/* Schedule callback. */
	gcicallbackarm->gcerror
		= gcqueue_callback(gccorecontext,
				   gcmmucontext,
				   gcicallbackarm->callback,
				   gcicallbackarm->callbackparam);
	if (gcicallbackarm->gcerror != GCERR_NONE)
		goto exit;

exit:
	GCUNLOCK(&gccorecontext->mmucontextlock);

	GCEXITARG(GCZONE_CALLBACK, "gc%s = 0x%08X\n",
		  (gcicallbackarm->gcerror == GCERR_NONE) ? "result" : "error",
		   gcicallbackarm->gcerror);
}
EXPORT_SYMBOL(gc_callback);

void gc_release(void)
{
	struct gccorecontext *gccorecontext = &g_context;
	struct list_head *ctxhead;
	struct gcmmucontext *temp = NULL;
	pid_t pid;

	GCENTER(GCZONE_CONTEXT);

	GCLOCK(&gccorecontext->mmucontextlock);

	pid = current->tgid;
	GCDBG(GCZONE_CONTEXT, "scanning context records for pid %d.\n", pid);

	list_for_each(ctxhead, &gccorecontext->mmuctxlist) {
		temp = list_entry(ctxhead, struct gcmmucontext, link);
		if (temp->pid == pid) {
			GCDBG(GCZONE_CONTEXT, "context is found @ 0x%08X\n",
			      (unsigned int) temp);

			gcmmu_destroy_context(gccorecontext, temp);
			list_move(ctxhead, &gccorecontext->mmuctxvac);
			break;
		}
	}

	GCUNLOCK(&gccorecontext->mmucontextlock);

	GCEXIT(GCZONE_CONTEXT);
}
EXPORT_SYMBOL(gc_release);

static int gc_probe_opp(struct platform_device *pdev)
{
	int i;
	unsigned int size;
	unsigned long freq = 0;
	struct gccorecontext *gccorecontext = &g_context;

	/* Query supported OPPs. */
	rcu_read_lock();

	gccorecontext->opp_count = opp_get_opp_count(&pdev->dev);
	if (gccorecontext->opp_count <= 0) {
		gccorecontext->opp_count = 0;
		goto done;
	}

	size = gccorecontext->opp_count * sizeof(unsigned long);
	gccorecontext->opp_freqs = kzalloc(size, GFP_ATOMIC);
	if (!gccorecontext->opp_freqs) {
		gccorecontext->opp_count = 0;
		goto done;
	}

	GCDBG(GCZONE_FREQSCALE, "frequency scaling table:\n");

	for (i = 0; i < gccorecontext->opp_count; i++) {
		struct opp *opp = opp_find_freq_ceil(&pdev->dev, &freq);
		if (IS_ERR_OR_NULL(opp)) {
			gccorecontext->opp_count = i;
			goto done;
		}

		/* Set freq, prepare to next. */
		gccorecontext->opp_freqs[i] = freq++;
		GCDBG(GCZONE_FREQSCALE, "  [%d] 0x%08X\n",
		      i, gccorecontext->opp_freqs[i]);
	}

done:
	rcu_read_unlock();
	gcpwr_set(gccorecontext, GCPWR_LOW);
	return 0;
}

static int gc_probe(struct platform_device *pdev)
{
	struct gccorecontext *gccorecontext = &g_context;

	GCENTER(GCZONE_PROBE);

	gccorecontext->plat = (struct omap_gcx_platform_data *)
			       pdev->dev.platform_data;
	gccorecontext->regbase = gccorecontext->plat->regbase;
	gccorecontext->irqline = platform_get_irq(pdev, pdev->id);
	gccorecontext->device = &pdev->dev;

	pm_runtime_enable(gccorecontext->device);
	gccorecontext->plat->was_context_lost(gccorecontext->device);

	gc_probe_opp(pdev);

	pm_runtime_get_sync(gccorecontext->device);

	gccorecontext->gcmodel = gc_read_reg(GC_CHIP_ID_Address);
	gccorecontext->gcrevision = gc_read_reg(GC_CHIP_REV_Address);
	gccorecontext->gcdate = gc_read_reg(GC_CHIP_DATE_Address);
	gccorecontext->gctime = gc_read_reg(GC_CHIP_TIME_Address);
	gccorecontext->gcfeatures.raw = gc_read_reg(GC_FEATURES_Address);
	gccorecontext->gcfeatures0.raw = gc_read_reg(GC_FEATURES0_Address);
	gccorecontext->gcfeatures1.raw = gc_read_reg(GC_FEATURES1_Address);
	gccorecontext->gcfeatures2.raw = gc_read_reg(GC_FEATURES2_Address);
	gccorecontext->gcfeatures3.raw = gc_read_reg(GC_FEATURES3_Address);

	GCDBG(GCZONE_PROBE, "GPU IDENTITY:\n");
	GCDBG(GCZONE_PROBE, "  model=%X\n", gccorecontext->gcmodel);
	GCDBG(GCZONE_PROBE, "  revision=%X\n", gccorecontext->gcrevision);
	GCDBG(GCZONE_PROBE, "  date=%X\n", gccorecontext->gcdate);
	GCDBG(GCZONE_PROBE, "  time=%X\n", gccorecontext->gctime);
	GCDBG(GCZONE_PROBE, "  features=0x%08X\n", gccorecontext->gcfeatures);
	GCDBG(GCZONE_PROBE, "  features0=0x%08X\n", gccorecontext->gcfeatures0);
	GCDBG(GCZONE_PROBE, "  features1=0x%08X\n", gccorecontext->gcfeatures1);
	GCDBG(GCZONE_PROBE, "  features2=0x%08X\n", gccorecontext->gcfeatures2);
	GCDBG(GCZONE_PROBE, "  features3=0x%08X\n", gccorecontext->gcfeatures3);

	pm_runtime_put_sync(gccorecontext->device);

	GCEXIT(GCZONE_PROBE);
	return 0;
}

static int gc_remove(struct platform_device *pdev)
{
	kfree(g_context.opp_freqs);
	return 0;
}

#if GC_ENABLE_SUSPEND
static int gc_suspend(struct platform_device *pdev, pm_message_t s)
{
	GCENTER(GCZONE_POWER);
	gcqueue_wait_idle(&g_context);
	GCEXIT(GCZONE_POWER);
	return 0;
}

static int gc_resume(struct platform_device *pdev)
{
	GCENTER(GCZONE_POWER);
	GCEXIT(GCZONE_POWER);
	return 0;
}
#endif

static struct platform_driver plat_drv = {
	.probe = gc_probe,
	.remove = gc_remove,
#if GC_ENABLE_SUSPEND
	.suspend = gc_suspend,
	.resume = gc_resume,
#endif
	.driver = {
		.owner = THIS_MODULE,
		.name = "gccore",
	},
};

#if CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
static void gc_early_suspend(struct early_suspend *h)
{
	GCENTER(GCZONE_POWER);
	gcqueue_wait_idle(&g_context);
	GCEXIT(GCZONE_POWER);
}

static void gc_late_resume(struct early_suspend *h)
{
	GCENTER(GCZONE_POWER);
	GCEXIT(GCZONE_POWER);
}

static struct early_suspend early_suspend_info = {
	.suspend = gc_early_suspend,
	.resume = gc_late_resume,
	.level = EARLY_SUSPEND_LEVEL_DISABLE_FB,
};
#endif


/*******************************************************************************
 * Driver init/shutdown.
 */

static int gc_init(struct gccorecontext *gccorecontext);
static void gc_exit(struct gccorecontext *gccorecontext);

static int gc_init(struct gccorecontext *gccorecontext)
{
	int result;

	GCENTER(GCZONE_INIT);

	/* check if hardware is available */
	if (!cpu_is_omap447x()) {
		GCDBG(GCZONE_INIT, "gcx hardware is not present\n");
		goto exit;
	}

	/* Initialize data structutres. */
	GCLOCK_INIT(&gccorecontext->powerlock);
	GCLOCK_INIT(&gccorecontext->resetlock);
	GCLOCK_INIT(&gccorecontext->mmucontextlock);
	INIT_LIST_HEAD(&gccorecontext->mmuctxlist);
	INIT_LIST_HEAD(&gccorecontext->mmuctxvac);

	/* Pulse skipping isn't known. */
	gccorecontext->pulseskipping = -1;

	/* Initialize MMU. */
	if (gcmmu_init(gccorecontext) != GCERR_NONE) {
		GCERR("failed to initialize MMU.\n");
		result = -EINVAL;
		goto fail;
	}

	gccorecontext->bb2ddevice = omap_hwmod_name_get_dev("bb2d");
	if (gccorecontext->bb2ddevice == NULL) {
		GCERR("cannot find bb2d device.\n");
		result = -EINVAL;
		goto fail;
	}

	result = platform_driver_register(&plat_drv);
	if (result < 0) {
		GCERR("failed to register platform driver.\n");
		goto fail;
	}
	gccorecontext->platdriver = true;

#if CONFIG_HAS_EARLYSUSPEND
	register_early_suspend(&early_suspend_info);
#endif

	/* Initialize the command buffer. */
	if (gcqueue_start(gccorecontext) != GCERR_NONE) {
		GCERR("failed to initialize command buffer.\n");
		result = -EINVAL;
		goto fail;
	}

	/* Create debugfs entry. */
	gc_debug_init();

exit:
	GCEXIT(GCZONE_INIT);
	return 0;

fail:
	gc_exit(gccorecontext);

	GCEXITARG(GCZONE_INIT, "result = %d\n", result);
	return result;
}

static void gc_exit(struct gccorecontext *gccorecontext)
{
	GCENTER(GCZONE_INIT);

	if (cpu_is_omap447x()) {
		/* Stop command queue thread. */
		gcqueue_stop(gccorecontext);

		/* Destroy MMU. */
		destroy_mmu_context(gccorecontext);
		gcmmu_exit(gccorecontext);

		/* Disable power. */
		pm_runtime_disable(gccorecontext->device);

		if (gccorecontext->platdriver) {
			platform_driver_unregister(&plat_drv);
			gccorecontext->platdriver = false;
		}

#if CONFIG_HAS_EARLYSUSPEND
		unregister_early_suspend(&early_suspend_info);
#endif

		gc_debug_shutdown();

		GCLOCK_DESTROY(&gccorecontext->mmucontextlock);
		GCLOCK_DESTROY(&gccorecontext->resetlock);
		GCLOCK_DESTROY(&gccorecontext->powerlock);
	}

	GCEXIT(GCZONE_PROBE);
}

static int __init gc_init_wrapper(void)
{
	GCDBG_INIT();
	GCDBG_REGISTER(core);
	GCDBG_REGISTER(mem);
	GCDBG_REGISTER(mmu);
	GCDBG_REGISTER(queue);

	return gc_init(&g_context);
}

static void __exit gc_exit_wrapper(void)
{
	gc_exit(&g_context);
	GCDBG_EXIT();
}

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("www.vivantecorp.com");
MODULE_AUTHOR("www.ti.com");
module_init(gc_init_wrapper);
module_exit(gc_exit_wrapper);
