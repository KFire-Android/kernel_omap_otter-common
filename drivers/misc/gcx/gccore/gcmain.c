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

GCDBG_FILTERDEF(core, GCZONE_NONE,
		"init",
		"context",
		"power",
		"commit",
		"mapping",
		"probe")


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

/*
 * scale gcxx device
 */
static void gcxxx_device_scale(struct gccorecontext *core, int idx)
{
	int ret;

	if (!core->opp_count || (idx >= core->opp_count))
		return;
	if (!core->plat || !core->plat->scale_dev)
		return;
	if (core->cur_freq != core->opp_freqs[idx]) {
		ret = core->plat->scale_dev(
			core->bb2ddevice, core->opp_freqs[idx]);
		if (!ret)
			core->cur_freq = core->opp_freqs[idx];
	}
}

static void gcpwr_enable_pulse_skipping(struct gccorecontext *gccorecontext)
{
	GCENTER(GCZONE_POWER);

	if (!gccorecontext->clockenabled)
		goto exit;

	if (!gccorecontext->pulseskipping) {
		union gcclockcontrol gcclockcontrol;

		/* opp scale */
		gcxxx_device_scale(gccorecontext, 0);

		/* Enable loading and set to minimum value. */
		gcclockcontrol.raw = 0;
		gcclockcontrol.reg.pulsecount = 1;
		gcclockcontrol.reg.pulseset = true;
		gc_write_reg(GCREG_HI_CLOCK_CONTROL_Address,
				gcclockcontrol.raw);

		/* Disable loading. */
		gcclockcontrol.reg.pulseset = false;
		gc_write_reg(GCREG_HI_CLOCK_CONTROL_Address,
				gcclockcontrol.raw);

		/* Pulse skipping enabled. */
		gccorecontext->pulseskipping = true;
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

	if (gccorecontext->pulseskipping) {
		union gcclockcontrol gcclockcontrol;

		/* Enable loading and set to maximum value. */
		gcclockcontrol.reg.pulsecount = 64;
		gcclockcontrol.reg.pulseset = true;
		gc_write_reg(GCREG_HI_CLOCK_CONTROL_Address,
				gcclockcontrol.raw);

		/* Disable loading. */
		gcclockcontrol.reg.pulseset = false;
		gc_write_reg(GCREG_HI_CLOCK_CONTROL_Address,
				gcclockcontrol.raw);

		/* Pulse skipping disabled. */
		gccorecontext->pulseskipping = false;
	}

	/* opp device scale */
	gcxxx_device_scale(gccorecontext,
			   gccorecontext->opp_count - 1);

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
		mdelay(1);

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


/*******************************************************************************
 * Command buffer submission.
 */

void gc_commit(struct gccommit *gccommit, bool fromuser)
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
	if ((gccommit->entrypipe != GCPIPE_2D) &&
		(gccommit->entrypipe != GCPIPE_3D)) {
		gccommit->gcerror = GCERR_CMD_ENTRY_PIPE;
		goto exit;
	}

	if ((gccommit->exitpipe != GCPIPE_2D) &&
		(gccommit->exitpipe != GCPIPE_3D)) {
		gccommit->gcerror = GCERR_CMD_EXIT_PIPE;
		goto exit;
	}

	/* Locate the client entry. */
	gccommit->gcerror = find_context(gccorecontext, fromuser,
						&gcmmucontext);
	if (gccommit->gcerror != GCERR_NONE)
		goto exit;

	/* Set the master table. */
	gccommit->gcerror = gcmmu_set_master(gccorecontext, gcmmucontext);
	if (gccommit->gcerror != GCERR_NONE)
		goto exit;

	/* Set the correct graphics pipe. */
	if (gccorecontext->gcpipe != gccommit->entrypipe) {
		static struct gcregpipeselect gcregpipeselect[] = {
			/* GCPIPE_UNKNOWN */
			{ 0, 0 },

			/* GCPIPE_2D */
			{ GCREG_PIPE_SELECT_PIPE_PIPE2D, 0 },

			/* GCPIPE_2D */
			{ GCREG_PIPE_SELECT_PIPE_PIPE2D, 0 }
		};

		GCDBG(GCZONE_COMMIT, "allocating space for pipe switch.\n");
		gccommit->gcerror = gcqueue_alloc(gccorecontext, gcmmucontext,
						  sizeof(struct gcmopipesel),
						  (void **) &gcmopipesel, NULL);
		if (gccommit->gcerror != GCERR_NONE)
			goto exit;

		gcmopipesel->pipesel_ldst = gcmopipesel_pipesel_ldst;
		gcmopipesel->pipesel.reg = gcregpipeselect[gccommit->entrypipe];
	}

	/* Update the current pipe. */
	gccorecontext->gcpipe = gccommit->exitpipe;

	/* Go through all buffers one at a time. */
	gcbuffer = gccommit->buffer;
	while (gcbuffer != NULL) {
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
		gccommit->gcerror = gcqueue_alloc(gccorecontext, gcmmucontext,
						  buffersize,
						  (void **) &logical,
						  &address);
		if (gccommit->gcerror != GCERR_NONE)
			goto exit;

		if (fromuser) {
			/* Copy command buffer. */
			if (copy_from_user(logical, gcbuffer->head,
						buffersize)) {
				GCERR("failed to read data.\n");
				gccommit->gcerror = GCERR_USER_READ;
				goto exit;
			}
		} else {
			memcpy(logical, gcbuffer->head, buffersize);
		}

		/* Process fixups. */
		gccommit->gcerror = gcmmu_fixup(gcbuffer->fixuphead, logical);
		if (gccommit->gcerror != GCERR_NONE)
			goto exit;

		/* Get the next buffer. */
		gcbuffer = gcbuffer->next;
	}

	/* Add the callback. */
	if (gccommit->callback != NULL) {
		gccommit->gcerror = gcqueue_callback(gccorecontext,
						     gcmmucontext,
						     gccommit->callback,
						     gccommit->callbackparam);
		if (gccommit->gcerror != GCERR_NONE)
			goto exit;
	}

	/* Process unmappings. */
	list_for_each(head, &gccommit->unmap) {
		gcschedunmap = list_entry(head, struct gcschedunmap, link);
		gccommit->gcerror = gcqueue_schedunmap(gccorecontext,
						       gcmmucontext,
						       gcschedunmap->handle);
		if (gccommit->gcerror != GCERR_NONE)
			goto exit;
	}

	/* Execute the buffer. */
	gccommit->gcerror = gcqueue_execute(gccorecontext, false,
					    gccommit->asynchronous);

exit:
	GCUNLOCK(&gccorecontext->mmucontextlock);

	GCEXITARG(GCZONE_COMMIT, "gc%s = 0x%08X\n",
		(gccommit->gcerror == GCERR_NONE) ? "result" : "error",
		gccommit->gcerror);
}
EXPORT_SYMBOL(gc_commit);

void gc_map(struct gcmap *gcmap, bool fromuser)
{
	struct gccorecontext *gccorecontext = &g_context;
	struct gcmmucontext *gcmmucontext;
	struct gcmmuphysmem mem;
	struct gcmmuarena *mapped = NULL;

	GCENTER(GCZONE_MAPPING);

	GCLOCK(&gccorecontext->mmucontextlock);

	/* Locate the client entry. */
	gcmap->gcerror = find_context(gccorecontext, fromuser, &gcmmucontext);
	if (gcmap->gcerror != GCERR_NONE)
		goto exit;

	GCDBG(GCZONE_MAPPING, "map client buffer\n");

	/* Initialize the mapping parameters. */
	if (gcmap->pagearray == NULL) {
		mem.base = ((u32) gcmap->buf.logical) & ~(PAGE_SIZE - 1);
		mem.offset = ((u32) gcmap->buf.logical) & (PAGE_SIZE - 1);
		mem.pages = NULL;

		GCDBG(GCZONE_MAPPING, "  logical = 0x%08X\n",
			(unsigned int) gcmap->buf.logical);
	} else {
		mem.base = 0;
		mem.offset = gcmap->buf.offset;
		mem.pages = gcmap->pagearray;

		GCDBG(GCZONE_MAPPING, "  pagearray = 0x%08X\n",
			(unsigned int) gcmap->pagearray);
	}

	GCDBG(GCZONE_MAPPING, "  size = %d\n", gcmap->size);

	mem.count = DIV_ROUND_UP(gcmap->size + mem.offset, PAGE_SIZE);
	mem.pagesize = gcmap->pagesize ? gcmap->pagesize : PAGE_SIZE;

	/* Map the buffer. */
	gcmap->gcerror = gcmmu_map(gccorecontext, gcmmucontext, &mem, &mapped);
	if (gcmap->gcerror != GCERR_NONE)
		goto exit;

	gcmap->handle = (unsigned int) mapped;

	GCDBG(GCZONE_MAPPING, "  mapped address = 0x%08X\n", mapped->address);
	GCDBG(GCZONE_MAPPING, "  handle = 0x%08X\n", (unsigned int) mapped);

exit:
	GCUNLOCK(&gccorecontext->mmucontextlock);

	GCEXITARG(GCZONE_MAPPING, "gc%s = 0x%08X\n",
		(gcmap->gcerror == GCERR_NONE) ? "result" : "error",
		gcmap->gcerror);
}
EXPORT_SYMBOL(gc_map);

void gc_unmap(struct gcmap *gcmap, bool fromuser)
{
	struct gccorecontext *gccorecontext = &g_context;
	struct gcmmucontext *gcmmucontext;

	GCENTER(GCZONE_MAPPING);

	GCLOCK(&gccorecontext->mmucontextlock);

	/* Locate the client entry. */
	gcmap->gcerror = find_context(gccorecontext, fromuser, &gcmmucontext);
	if (gcmap->gcerror != GCERR_NONE)
		goto exit;

	GCDBG(GCZONE_MAPPING, "unmap client buffer\n");
	GCDBG(GCZONE_MAPPING, "  handle = 0x%08X\n", gcmap->handle);

	/* Schedule unmapping. */
	gcmap->gcerror = gcqueue_schedunmap(gccorecontext, gcmmucontext,
					    gcmap->handle);
	if (gcmap->gcerror != GCERR_NONE)
		goto exit;

	/* Execute the buffer. */
	gcmap->gcerror = gcqueue_execute(gccorecontext, false, false);
	if (gcmap->gcerror != GCERR_NONE)
		goto exit;

	/* Invalidate the handle. */
	gcmap->handle = ~0U;

exit:
	GCUNLOCK(&gccorecontext->mmucontextlock);

	GCEXITARG(GCZONE_MAPPING, "gc%s = 0x%08X\n",
		(gcmap->gcerror == GCERR_NONE) ? "result" : "error",
		gcmap->gcerror);
}
EXPORT_SYMBOL(gc_unmap);

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
	unsigned long freq = 0;
	struct gccorecontext *core = &g_context;

	/* Query supported OPPs */
	rcu_read_lock();

	core->opp_count = opp_get_opp_count(&pdev->dev);
	if (core->opp_count <= 0) {
		core->opp_count = 0;
		goto done;
	}

	core->opp_freqs = kzalloc((core->opp_count) * sizeof(unsigned long),
			GFP_KERNEL);
	if (!core->opp_freqs) {
		core->opp_count = 0;
		goto done;
	}

	for (i = 0; i < core->opp_count; i++) {
		struct opp *opp = opp_find_freq_ceil(&pdev->dev, &freq);
		if (IS_ERR_OR_NULL(opp)) {
			core->opp_count = i;
			goto done;
		}
		core->opp_freqs[i] = freq++; /* get freq, prepare to next */
	}
done:
	rcu_read_unlock();

	/* set lowest opp */
	if (core->opp_count)
		gcxxx_device_scale(core, 0);

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

	GCDBG(GCZONE_PROBE, "GPU IDENTITY:\n");
	GCDBG(GCZONE_PROBE, "  model=%X\n",
	      gc_read_reg(GC_CHIP_ID_Address));
	GCDBG(GCZONE_PROBE, "  revision=%X\n",
	      gc_read_reg(GC_CHIP_REV_Address));
	GCDBG(GCZONE_PROBE, "  date=%X\n",
	      gc_read_reg(GC_CHIP_DATE_Address));
	GCDBG(GCZONE_PROBE, "  time=%X\n",
	      gc_read_reg(GC_CHIP_TIME_Address));
	GCDBG(GCZONE_PROBE, "  chipFeatures=0x%08X\n",
	      gc_read_reg(GC_FEATURES_Address));
	GCDBG(GCZONE_PROBE, "  minorFeatures=0x%08X\n",
	      gc_read_reg(GC_MINOR_FEATURES0_Address));

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
