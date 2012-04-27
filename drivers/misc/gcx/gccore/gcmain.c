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

#include <linux/version.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/uaccess.h>
#include <linux/io.h>
#include <linux/pagemap.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/pm_runtime.h>
#include <linux/dma-mapping.h>
#include <plat/cpu.h>
#include <linux/delay.h>
#include <linux/debugfs.h>
#include <plat/omap_gcx.h>
#include <linux/delay.h>
#include <linux/opp.h>

#include "gcmain.h"

#define GC_ENABLE_SUSPEND

#define GCZONE_ALL		(~0U)
#define GCZONE_INIT		(1 << 0)
#define GCZONE_CONTEXT		(1 << 1)
#define GCZONE_POWER		(1 << 2)
#define GCZONE_PAGE		(1 << 3)
#define GCZONE_COMMIT		(1 << 4)
#define GCZONE_MAPPING		(1 << 5)
#define GCZONE_PROBE		(1 << 6)

#include <linux/gcx.h>
#include <linux/gccore.h>
#include "gccmdbuf.h"
#include "gcmmu.h"
#include <linux/gcdebug.h>

#define GC_POLL_PRCM_STBY 100

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

	GCPRINT(GCDBGFILTER, GCZONE_COMMIT, "++" GC_MOD_PREFIX
		"\n", __func__, __LINE__);

	/* Get current PID. */
	pid = fromuser ? current->tgid : 0;

	/* Search the list. */
	GCPRINT(GCDBGFILTER, GCZONE_CONTEXT, GC_MOD_PREFIX
		"scanning context records for pid %d.\n",
		__func__, __LINE__, pid);

	/* Try to locate the record. */
	list_for_each(ctxhead, &gccorecontext->mmuctxlist) {
		temp = list_entry(ctxhead, struct gcmmucontext, link);
		if (temp->pid == pid) {
			/* Success. */
			GCPRINT(GCDBGFILTER, GCZONE_CONTEXT, GC_MOD_PREFIX
				"context is found @ 0x%08X\n",
				__func__, __LINE__,
				(unsigned int) temp);

			goto exit;
		}
	}

	/* Get new record. */
	if (list_empty(&gccorecontext->mmuctxvac)) {
		GCPRINT(GCDBGFILTER, GCZONE_CONTEXT, GC_MOD_PREFIX
			"not found, allocating.\n",
			__func__, __LINE__);

		temp = kmalloc(sizeof(struct gcmmucontext), GFP_KERNEL);
		if (temp == NULL) {
			GCPRINT(NULL, 0, GC_MOD_PREFIX
				"out of memory.\n",
				__func__, __LINE__);
			gcerror = GCERR_SETGRP(GCERR_OODM,
						GCERR_IOCTL_CTX_ALLOC);
			goto fail;
		}

		GCPRINT(GCDBGFILTER, GCZONE_CONTEXT, GC_MOD_PREFIX
			"allocated @ 0x%08X\n",
			__func__, __LINE__, (unsigned int) temp);
	} else {
		ctxhead = gccorecontext->mmuctxvac.next;
		temp = list_entry(ctxhead, struct gcmmucontext, link);
		list_del(ctxhead);

		GCPRINT(GCDBGFILTER, GCZONE_CONTEXT, GC_MOD_PREFIX
			"not found, reusing vacant @ 0x%08X\n",
			__func__, __LINE__, (unsigned int) temp);
	}

	gcerror = gcmmu_create_context(temp);
	if (gcerror != GCERR_NONE)
		goto fail;

	gcerror = cmdbuf_map(temp);
	if (gcerror != GCERR_NONE)
		goto fail;

	temp->pid = pid;
	temp->dirty = true;

	/* Add the context to the list. */
	list_add(&temp->link, &gccorecontext->mmuctxlist);

exit:
	*gcmmucontext = temp;

	GCPRINT(GCDBGFILTER, GCZONE_COMMIT, "--" GC_MOD_PREFIX
		"\n", __func__, __LINE__);

	return GCERR_NONE;

fail:
	if (temp != NULL) {
		gcmmu_destroy_context(temp);
		list_add(&temp->link, &gccorecontext->mmuctxvac);
	}

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
		gcmmu_destroy_context(temp);
		list_del(head);
		kfree(temp);
	}
}

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
 * Page allocation routines.
 */

enum gcerror gc_alloc_noncached(struct gcpage *p, unsigned int size)
{
	enum gcerror gcerror;

	GCPRINT(GCDBGFILTER, GCZONE_PAGE, "++" GC_MOD_PREFIX
		"p = 0x%08X\n",
		__func__, __LINE__, (unsigned int) p);

	p->pages = NULL;
	p->order = 0;
	p->size = (size + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
	p->logical = NULL;
	p->physical = ~0UL;

	GCPRINT(GCDBGFILTER, GCZONE_PAGE, GC_MOD_PREFIX
		"requested size=%d\n", __func__, __LINE__, size);

	GCPRINT(GCDBGFILTER, GCZONE_PAGE, GC_MOD_PREFIX
		"aligned size=%d\n", __func__, __LINE__, p->size);

	p->logical = dma_alloc_coherent(NULL, p->size, &p->physical,
						GFP_KERNEL);
	if (!p->logical) {
		GCPRINT(NULL, 0, GC_MOD_PREFIX
			"failed to allocate memory\n",
			__func__, __LINE__);

		gcerror = GCERR_OOPM;
		goto exit;
	}

	GCPRINT(GCDBGFILTER, GCZONE_PAGE, GC_MOD_PREFIX
		"logical=0x%08X\n",
		__func__, __LINE__, (unsigned int) p->logical);

	GCPRINT(GCDBGFILTER, GCZONE_PAGE, GC_MOD_PREFIX
		"physical=0x%08X\n",
		__func__, __LINE__, (unsigned int) p->physical);

	GCPRINT(GCDBGFILTER, GCZONE_PAGE, "--" GC_MOD_PREFIX
		"\n", __func__, __LINE__);

	return GCERR_NONE;

exit:
	gc_free_noncached(p);

	GCPRINT(GCDBGFILTER, GCZONE_PAGE, "--" GC_MOD_PREFIX
		"gcerror = 0x%08X\n", __func__, __LINE__, gcerror);

	return gcerror;
}

void gc_free_noncached(struct gcpage *p)
{
	GCPRINT(GCDBGFILTER, GCZONE_PAGE, "++" GC_MOD_PREFIX
		"p = 0x%08X\n",
		__func__, __LINE__, (unsigned int) p);

	if (p->logical != NULL) {
		dma_free_coherent(NULL, p->size, p->logical, p->physical);
		p->logical = NULL;
	}

	p->physical = ~0UL;
	p->size = 0;

	GCPRINT(GCDBGFILTER, GCZONE_PAGE, "--" GC_MOD_PREFIX
		"\n", __func__, __LINE__);
}

enum gcerror gc_alloc_cached(struct gcpage *p, unsigned int size)
{
	enum gcerror gcerror;
	struct page *pages;
	int count;

	GCPRINT(GCDBGFILTER, GCZONE_PAGE, "++" GC_MOD_PREFIX
		"p = 0x%08X\n",
		__func__, __LINE__, (unsigned int) p);

	p->order = get_order(size);
	p->pages = NULL;
	p->size = (1 << p->order) * PAGE_SIZE;
	p->logical = NULL;
	p->physical = ~0UL;

	GCPRINT(GCDBGFILTER, GCZONE_PAGE, GC_MOD_PREFIX
		"requested size=%d\n", __func__, __LINE__, size);

	GCPRINT(GCDBGFILTER, GCZONE_PAGE, GC_MOD_PREFIX
		"aligned size=%d\n", __func__, __LINE__, p->size);

	p->pages = alloc_pages(GFP_KERNEL, p->order);
	if (p->pages == NULL) {
		GCPRINT(NULL, 0, GC_MOD_PREFIX
			"failed to allocate memory\n",
			__func__, __LINE__);

		gcerror = GCERR_OOPM;
		goto exit;
	}

	p->physical = page_to_phys(p->pages);
	p->logical = (unsigned int *) page_address(p->pages);

	if (p->logical == NULL) {
		GCPRINT(NULL, 0, GC_MOD_PREFIX
			"failed to retrieve page virtual address\n",
			__func__, __LINE__);

		gcerror = GCERR_PMMAP;
		goto exit;
	}

	/* Reserve pages. */
	pages = p->pages;
	count = p->size / PAGE_SIZE;

	while (count--)
		SetPageReserved(pages++);

	GCPRINT(GCDBGFILTER, GCZONE_PAGE, GC_MOD_PREFIX
		"page array=0x%08X\n",
		__func__, __LINE__, (unsigned int) p->pages);

	GCPRINT(GCDBGFILTER, GCZONE_PAGE, GC_MOD_PREFIX
		"logical=0x%08X\n",
		__func__, __LINE__, (unsigned int) p->logical);

	GCPRINT(GCDBGFILTER, GCZONE_PAGE, GC_MOD_PREFIX
		"physical=0x%08X\n",
		__func__, __LINE__, (unsigned int) p->physical);

	GCPRINT(GCDBGFILTER, GCZONE_PAGE, "--" GC_MOD_PREFIX
		"\n", __func__, __LINE__);

	return GCERR_NONE;

exit:
	gc_free_cached(p);

	GCPRINT(GCDBGFILTER, GCZONE_PAGE, "--" GC_MOD_PREFIX
		"gcerror = 0x%08X\n", __func__, __LINE__, gcerror);

	return gcerror;
}

void gc_free_cached(struct gcpage *p)
{
	GCPRINT(GCDBGFILTER, GCZONE_PAGE, "++" GC_MOD_PREFIX
		"p = 0x%08X\n",
		__func__, __LINE__, (unsigned int) p);

	GCPRINT(GCDBGFILTER, GCZONE_PAGE, GC_MOD_PREFIX
		"page array=0x%08X\n",
		__func__, __LINE__, (unsigned int) p->pages);

	GCPRINT(GCDBGFILTER, GCZONE_PAGE, GC_MOD_PREFIX
		"logical=0x%08X\n",
		__func__, __LINE__, (unsigned int) p->logical);

	GCPRINT(GCDBGFILTER, GCZONE_PAGE, GC_MOD_PREFIX
		"physical=0x%08X\n",
		__func__, __LINE__, (unsigned int) p->physical);

	GCPRINT(GCDBGFILTER, GCZONE_PAGE, GC_MOD_PREFIX
		"size=%d\n",
		__func__, __LINE__, p->size);

	if (p->logical != NULL) {
		struct page *pages;
		int count;

		pages = p->pages;
		count = p->size / PAGE_SIZE;

		while (count--)
			ClearPageReserved(pages++);

		p->logical = NULL;
	}

	if (p->pages != NULL) {
		__free_pages(p->pages, p->order);
		p->pages = NULL;
	}

	p->physical = ~0UL;
	p->order = 0;
	p->size = 0;

	GCPRINT(GCDBGFILTER, GCZONE_PAGE, "--" GC_MOD_PREFIX
		"\n", __func__, __LINE__);
}

void gc_flush_cached(struct gcpage *p)
{
	GCPRINT(GCDBGFILTER, GCZONE_PAGE, "++" GC_MOD_PREFIX
		"p = 0x%08X\n",
		__func__, __LINE__, (unsigned int) p);

	dmac_flush_range(p->logical, (unsigned char *) p->logical + p->size);
	outer_flush_range(p->physical, p->physical + p->size);

	GCPRINT(GCDBGFILTER, GCZONE_PAGE, "--" GC_MOD_PREFIX
		"\n", __func__, __LINE__);
}

void gc_flush_region(unsigned int physical, void *logical,
			unsigned int offset, unsigned int size)
{
	unsigned char *startlog;
	unsigned int startphys;

	GCPRINT(GCDBGFILTER, GCZONE_PAGE, "++" GC_MOD_PREFIX
		"\n", __func__, __LINE__);

	GCPRINT(GCDBGFILTER, GCZONE_PAGE, GC_MOD_PREFIX
		"logical=0x%08X\n",
		__func__, __LINE__, (unsigned int) logical);

	GCPRINT(GCDBGFILTER, GCZONE_PAGE, GC_MOD_PREFIX
		"physical=0x%08X\n",
		__func__, __LINE__, physical);

	GCPRINT(GCDBGFILTER, GCZONE_PAGE, GC_MOD_PREFIX
		"offset=%d\n",
		__func__, __LINE__, offset);

	GCPRINT(GCDBGFILTER, GCZONE_PAGE, GC_MOD_PREFIX
		"size=%d\n",
		__func__, __LINE__, size);

	startlog = (unsigned char *) logical + offset;
	startphys = physical + offset;

	dmac_flush_range(startlog, startlog + size);
	outer_flush_range(startphys, startphys + size);

	GCPRINT(GCDBGFILTER, GCZONE_PAGE, "--" GC_MOD_PREFIX
		"\n", __func__, __LINE__);
}

/*******************************************************************************
 * Interrupt handling.
 */

void gc_wait_interrupt(void)
{
	while (true) {
		if (wait_for_completion_timeout(&g_context.intready, HZ * 5))
			break;

		GCGPUSTATUS(NULL, 0, __func__, __LINE__, NULL);
	}
}

unsigned int gc_get_interrupt_data(void)
{
	unsigned int data;

	data = g_context.intdata;
	g_context.intdata = 0;

	return data;
}

static irqreturn_t gc_irq(int irq, void *_gccorecontext)
{
	struct gccorecontext *gccorecontext;
	unsigned int data;

	/* Read gcregIntrAcknowledge register. */
	data = gc_read_reg(GCREG_INTR_ACKNOWLEDGE_Address);

	/* Our interrupt? */
	if (data == 0)
		return IRQ_NONE;

	gc_debug_cache_gpu_status_from_irq(data);

	gccorecontext = (struct gccorecontext *) _gccorecontext;
	gccorecontext->intdata = data;
	complete(&gccorecontext->intready);

	return IRQ_HANDLED;
}

/*******************************************************************************
 * GPU power level control.
 */

#include <plat/omap_hwmod.h>
#include <plat/omap-pm.h>

static void gc_reset_gpu(struct gccorecontext *gccorecontext)
{
	union gcclockcontrol gcclockcontrol;
	union gcidle gcidle;

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
			GCPRINT(NULL, 0, GC_MOD_PREFIX
				" FE NOT IDLE\n",
				__func__, __LINE__);

			continue;
		}

		/* Read reset register. */
		gcclockcontrol.raw
			= gc_read_reg(GCREG_HI_CLOCK_CONTROL_Address);

		/* Try resetting again if 2D is not idle. */
		if (!gcclockcontrol.reg.idle2d) {
			GCPRINT(NULL, 0, GC_MOD_PREFIX
				" 2D NOT IDLE\n",
				__func__, __LINE__);

			continue;
		}

		/* GPU is idle. */
		break;
	}

	/* Pulse skipping disabled. */
	gccorecontext->pulseskipping = false;

	GCPRINT(GCDBGFILTER, GCZONE_POWER, GC_MOD_PREFIX
		"gpu reset.\n",
		__func__, __LINE__);
}

static void gcpwr_enable_clock(struct gccorecontext *gccorecontext)
{
	bool ctxlost = gccorecontext->plat->was_context_lost(
		gccorecontext->device);

	if (!gccorecontext->clockenabled) {
		/* Enable the clock. */
		pm_runtime_get_sync(gccorecontext->device);

		/* Signal software not idle. */
		gc_write_reg(GC_GP_OUT0_Address, 0);

		/* Clock enabled. */
		gccorecontext->clockenabled = true;
	} else if (ctxlost) {
		u32 reg;
		dev_info(gccorecontext->device, "unexpected context\n");
		reg = gc_read_reg(GC_GP_OUT0_Address);
		if (reg) {
			dev_info(gccorecontext->device, "reset gchold\n");
			gc_write_reg(GC_GP_OUT0_Address, 0);
		}
	}

	GCPRINT(GCDBGFILTER, GCZONE_POWER, GC_MOD_PREFIX
		"clock %s.\n", __func__, __LINE__,
		gccorecontext->clockenabled ? "enabled" : "disabled");

	if (ctxlost || gccorecontext->gcpower == GCPWR_UNKNOWN)
		gc_reset_gpu(gccorecontext);
}

static void gcpwr_disable_clock(struct gccorecontext *gccorecontext)
{
	if (gccorecontext->clockenabled) {
		gc_debug_poweroff_cache();

		/* Signal software idle. */
		gc_write_reg(GC_GP_OUT0_Address, 1);

		/* Disable the clock. */
		pm_runtime_put_sync(gccorecontext->device);

		/* Clock disabled. */
		gccorecontext->clockenabled = false;
	}

	GCPRINT(GCDBGFILTER, GCZONE_POWER, GC_MOD_PREFIX
		"clock %s.\n", __func__, __LINE__,
		gccorecontext->clockenabled ? "enabled" : "disabled");
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
	if (!gccorecontext->clockenabled)
		return;

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
		GCPRINT(GCDBGFILTER, GCZONE_POWER, GC_MOD_PREFIX
			"pulse skipping enabled.\n",
			__func__, __LINE__);
	}

	GCPRINT(GCDBGFILTER, GCZONE_POWER, GC_MOD_PREFIX
		"pulse skipping %s.\n", __func__, __LINE__,
		gccorecontext->pulseskipping ? "enabled" : "disabled");
}

static void gcpwr_disable_pulse_skipping(struct gccorecontext *gccorecontext)
{
	if (!gccorecontext->clockenabled)
		return;

	if (gccorecontext->pulseskipping) {
		union gcclockcontrol gcclockcontrol;

		/* opp device scale */
		gcxxx_device_scale(gccorecontext,
				   gccorecontext->opp_count - 1);

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

	GCPRINT(GCDBGFILTER, GCZONE_POWER, GC_MOD_PREFIX
		"pulse skipping %s.\n", __func__, __LINE__,
		gccorecontext->pulseskipping ? "enabled" : "disabled");
}

enum gcerror gc_set_power(struct gccorecontext *gccorecontext,
				enum gcpower gcpower)
{
	enum gcerror gcerror = GCERR_NONE;

	if (gcpower == GCPWR_UNKNOWN) {
		gcerror = GCERR_POWER_MODE;
		goto exit;
	}

	if (gcpower != gccorecontext->gcpower) {
		GCPRINT(GCDBGFILTER, GCZONE_POWER, GC_MOD_PREFIX
			"power state %d --> %d\n",
			__func__, __LINE__, gccorecontext->gcpower, gcpower);

		switch (gcpower) {
		case GCPWR_ON:
			gcpwr_enable_clock(gccorecontext);
			gcpwr_disable_pulse_skipping(gccorecontext);

			if (!gccorecontext->irqenabled) {
				enable_irq(gccorecontext->irqline);
				gccorecontext->irqenabled = true;
			}
			break;

		case GCPWR_LOW:
			gcpwr_enable_pulse_skipping(gccorecontext);
			break;

		case GCPWR_OFF:
			gcpwr_disable_clock(gccorecontext);

			if (gccorecontext->irqenabled) {
				disable_irq(gccorecontext->irqline);
				gccorecontext->irqenabled = false;
			}
			break;

		default:
			gcerror = GCERR_POWER_MODE;
			goto exit;
		}

		/* Set new power state. */
		gccorecontext->gcpower = gcpower;
	}

exit:
	return gcerror;
}

enum gcerror gc_get_power(void)
{
	return g_context.gcpower;
}

/*******************************************************************************
 * Command buffer submission.
 */

void gc_commit(struct gccommit *gccommit, bool fromuser)
{
	struct gccorecontext *gccorecontext = &g_context;
	struct gcmmucontext *gcmmucontext;
	struct gcbuffer *gcbuffer;
	unsigned int cmdflushsize;
	unsigned int mmuflushsize;
	unsigned int buffersize;
	unsigned int allocsize;
	unsigned int *logical;
	unsigned int address;
	struct gcmopipesel *gcmopipesel;

	GCPRINT(GCDBGFILTER, GCZONE_COMMIT, "++" GC_MOD_PREFIX
		"\n", __func__, __LINE__);

	mutex_lock(&gccorecontext->mmucontextlock);

	/* Enable power to the chip. */
	gc_set_power(gccorecontext, GCPWR_ON);

	/* Locate the client entry. */
	gccommit->gcerror = find_context(gccorecontext, fromuser,
						&gcmmucontext);
	if (gccommit->gcerror != GCERR_NONE)
		goto exit;

	/* Different context? */
	if (gccorecontext->mmucontext != gcmmucontext) {
		gccorecontext->mmucontext = gcmmucontext;
		gcmmucontext->dirty = true;
	}

	/* Set the client's master table. */
	gccommit->gcerror = gcmmu_set_master(gcmmucontext);
	if (gccommit->gcerror != GCERR_NONE)
		goto exit;

	/* Set 2D pipe. */
	gccommit->gcerror = cmdbuf_alloc(sizeof(struct gcmopipesel),
					(void **) &gcmopipesel, NULL);
	if (gccommit->gcerror != GCERR_NONE)
		goto exit;

	gcmopipesel->pipesel_ldst = gcmopipesel_pipesel_ldst;
	gcmopipesel->pipesel.reg = gcregpipeselect_2D;

	/* Determine command buffer flush size. */
	cmdflushsize = cmdbuf_flush(NULL);

	/* Go through all buffers one at a time. */
	gcbuffer = gccommit->buffer;
	while (gcbuffer != NULL) {
		GCPRINT(GCDBGFILTER, GCZONE_COMMIT, GC_MOD_PREFIX
			"gcbuffer = 0x%08X\n",
			__func__, __LINE__, gcbuffer);

		/* Compute the size of the command buffer. */
		buffersize
			= (unsigned char *) gcbuffer->tail
			- (unsigned char *) gcbuffer->head;

		GCPRINT(GCDBGFILTER, GCZONE_COMMIT, GC_MOD_PREFIX
			"buffersize = %d\n",
			__func__, __LINE__, buffersize);

		/* Determine MMU flush size. */
		mmuflushsize = gcmmucontext->dirty
			? gcmmu_flush(NULL, 0, 0) : 0;

		/* Reserve command buffer space. */
		allocsize = mmuflushsize + buffersize + cmdflushsize;
		gccommit->gcerror = cmdbuf_alloc(allocsize,
						(void **) &logical, &address);
		if (gccommit->gcerror != GCERR_NONE)
			goto exit;

		/* Append MMU flush. */
		if (gcmmucontext->dirty) {
			gcmmu_flush(logical, address, allocsize);

			/* Skip MMU flush. */
			logical = (unsigned int *)
				((unsigned char *) logical + mmuflushsize);

			/* Validate MMU state. */
			gcmmucontext->dirty = false;
		}

		if (fromuser) {
			/* Copy command buffer. */
			if (copy_from_user(logical, gcbuffer->head,
						buffersize)) {
				GCPRINT(NULL, 0, GC_MOD_PREFIX
					"failed to read data.\n",
					__func__, __LINE__);
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

		/* Skip the command buffer. */
		logical = (unsigned int *)
			((unsigned char *) logical + buffersize);

		/* Execute the current command buffer. */
		cmdbuf_flush(logical);

		/* Get the next buffer. */
		gcbuffer = gcbuffer->next;
	}

exit:
	gc_set_power(gccorecontext, GCPWR_LOW);
	if (gccorecontext->forceoff)
		gc_set_power(gccorecontext, GCPWR_OFF);

	mutex_unlock(&gccorecontext->mmucontextlock);

	GCPRINT(GCDBGFILTER, GCZONE_COMMIT, "--" GC_MOD_PREFIX
		"gc%s = 0x%08X\n", __func__, __LINE__,
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

	GCPRINT(GCDBGFILTER, GCZONE_MAPPING, "++" GC_MOD_PREFIX
		"\n", __func__, __LINE__);

	mutex_lock(&gccorecontext->mmucontextlock);

	/* Locate the client entry. */
	gcmap->gcerror = find_context(gccorecontext, fromuser, &gcmmucontext);
	if (gcmap->gcerror != GCERR_NONE)
		goto exit;

	GCPRINT(GCDBGFILTER, GCZONE_MAPPING, GC_MOD_PREFIX
		"map client buffer\n",
		__func__, __LINE__);

	/* Initialize the mapping parameters. */
	if (gcmap->pagearray == NULL) {
		mem.base = ((u32) gcmap->buf.logical) & ~(PAGE_SIZE - 1);
		mem.offset = ((u32) gcmap->buf.logical) & (PAGE_SIZE - 1);
		mem.pages = NULL;

		GCPRINT(GCDBGFILTER, GCZONE_MAPPING, GC_MOD_PREFIX
			"  logical = 0x%08X\n",
			__func__, __LINE__, (unsigned int) gcmap->buf.logical);
	} else {
		mem.base = 0;
		mem.offset = gcmap->buf.offset;
		mem.pages = gcmap->pagearray;

		GCPRINT(GCDBGFILTER, GCZONE_MAPPING, GC_MOD_PREFIX
			"  pagearray = 0x%08X\n",
			__func__, __LINE__, (unsigned int) gcmap->pagearray);
	}

	GCPRINT(GCDBGFILTER, GCZONE_MAPPING, GC_MOD_PREFIX
		"  size = %d\n",
		__func__, __LINE__, gcmap->size);

	mem.count = DIV_ROUND_UP(gcmap->size + mem.offset, PAGE_SIZE);
	mem.pagesize = gcmap->pagesize ? gcmap->pagesize : PAGE_SIZE;

	/* Map the buffer. */
	gcmap->gcerror = gcmmu_map(gcmmucontext, &mem, &mapped);
	if (gcmap->gcerror != GCERR_NONE)
		goto exit;

	/* Invalidate the MMU. */
	gcmmucontext->dirty = true;

	gcmap->handle = (unsigned int) mapped;

	GCPRINT(GCDBGFILTER, GCZONE_MAPPING, GC_MOD_PREFIX
		"  mapped address = 0x%08X\n",
		__func__, __LINE__, mapped->address);

	GCPRINT(GCDBGFILTER, GCZONE_MAPPING, GC_MOD_PREFIX
		"  handle = 0x%08X\n",
		__func__, __LINE__, (unsigned int) mapped);

exit:
	mutex_unlock(&gccorecontext->mmucontextlock);

	GCPRINT(GCDBGFILTER, GCZONE_MAPPING, "--" GC_MOD_PREFIX
		"gc%s = 0x%08X\n", __func__, __LINE__,
		(gcmap->gcerror == GCERR_NONE) ? "result" : "error",
		gcmap->gcerror);
}
EXPORT_SYMBOL(gc_map);

void gc_unmap(struct gcmap *gcmap, bool fromuser)
{
	struct gccorecontext *gccorecontext = &g_context;
	struct gcmmucontext *gcmmucontext;

	GCPRINT(GCDBGFILTER, GCZONE_MAPPING, "++" GC_MOD_PREFIX
		"\n", __func__, __LINE__);

	mutex_lock(&gccorecontext->mmucontextlock);

	/* Locate the client entry. */
	gcmap->gcerror = find_context(gccorecontext, fromuser, &gcmmucontext);
	if (gcmap->gcerror != GCERR_NONE)
		goto exit;

	GCPRINT(GCDBGFILTER, GCZONE_MAPPING, GC_MOD_PREFIX
		"unmap client buffer\n",
		__func__, __LINE__);

	GCPRINT(GCDBGFILTER, GCZONE_MAPPING, GC_MOD_PREFIX
		"  size = %d\n",
		__func__, __LINE__, gcmap->size);

	GCPRINT(GCDBGFILTER, GCZONE_MAPPING, GC_MOD_PREFIX
		"  handle = 0x%08X\n",
		__func__, __LINE__, gcmap->handle);

	/* Map the buffer. */
	gcmap->gcerror = gcmmu_unmap(gcmmucontext,
					(struct gcmmuarena *) gcmap->handle);
	if (gcmap->gcerror != GCERR_NONE)
		goto exit;

	/* Invalidate the MMU. */
	gcmmucontext->dirty = true;

	/* Invalidate the handle. */
	gcmap->handle = ~0U;

exit:
	mutex_unlock(&gccorecontext->mmucontextlock);

	GCPRINT(GCDBGFILTER, GCZONE_MAPPING, "--" GC_MOD_PREFIX
		"gc%s = 0x%08X\n", __func__, __LINE__,
		(gcmap->gcerror == GCERR_NONE) ? "result" : "error",
		gcmap->gcerror);
}
EXPORT_SYMBOL(gc_unmap);

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
	int ret;
	struct gccorecontext *gccorecontext = &g_context;

	GCPRINT(GCDBGFILTER, GCZONE_PROBE, "++" GC_MOD_PREFIX
		"\n", __func__, __LINE__);

	gccorecontext->plat = (struct omap_gcx_platform_data *)
		pdev->dev.platform_data;

	gccorecontext->regbase = gccorecontext->plat->regbase;
	gccorecontext->irqline = platform_get_irq(pdev, pdev->id);

	ret = request_irq(gccorecontext->irqline, gc_irq, IRQF_SHARED,
				GC_DEV_NAME, gccorecontext);
	if (ret < 0) {
		GCPRINT(NULL, 0, GC_MOD_PREFIX
			"failed to install IRQ (%d).\n",
			__func__, __LINE__, ret);
		goto fail;
	}

	gccorecontext->isrroutine = true;

	/* Disable IRQ. */
	disable_irq(gccorecontext->irqline);
	gccorecontext->irqenabled = false;

	gccorecontext->device = &pdev->dev;

	pm_runtime_enable(gccorecontext->device);
	(void)gccorecontext->plat->was_context_lost(gccorecontext->device);

	gc_probe_opp(pdev);

	GCPRINT(GCDBGFILTER, GCZONE_PROBE, "--" GC_MOD_PREFIX
		"\n", __func__, __LINE__);

	return 0;

fail:
	GCPRINT(GCDBGFILTER, GCZONE_PROBE, "--" GC_MOD_PREFIX
		"ret = %d\n", __func__, __LINE__, ret);

	return ret;
}

static int gc_remove(struct platform_device *pdev)
{
	kfree(g_context.opp_freqs);
	return 0;
}

#if defined(GC_ENABLE_SUSPEND)
static int gc_suspend(struct platform_device *pdev, pm_message_t s)
{
	GCPRINT(GCDBGFILTER, GCZONE_POWER, "++" GC_MOD_PREFIX
		"\n", __func__, __LINE__);

	if (gc_set_power(&g_context, GCPWR_OFF))
		GCPRINT(NULL, 0, GC_MOD_PREFIX
			"suspend failure.\n",
			__func__, __LINE__);

	GCPRINT(GCDBGFILTER, GCZONE_POWER, "--" GC_MOD_PREFIX
		"\n", __func__, __LINE__);

	return 0;
}

static int gc_resume(struct platform_device *pdev)
{
	GCPRINT(GCDBGFILTER, GCZONE_POWER, "++" GC_MOD_PREFIX
		"\n", __func__, __LINE__);

	GCPRINT(GCDBGFILTER, GCZONE_POWER, "--" GC_MOD_PREFIX
		"\n", __func__, __LINE__);

	return 0;
}
#endif

static struct platform_driver plat_drv = {
	.probe = gc_probe,
	.remove = gc_remove,
#if defined(GC_ENABLE_SUSPEND)
	.suspend = gc_suspend,
	.resume = gc_resume,
#endif
	.driver = {
		.owner = THIS_MODULE,
		.name = "gccore",
	},
};

#if defined(CONFIG_HAS_EARLYSUSPEND)
#include <linux/earlysuspend.h>
static void gc_early_suspend(struct early_suspend *h)
{
	struct gccorecontext *gccorecontext = &g_context;

	GCPRINT(GCDBGFILTER, GCZONE_POWER, "++" GC_MOD_PREFIX
		"\n", __func__, __LINE__);

	mutex_lock(&gccorecontext->mmucontextlock);
	gccorecontext->forceoff = true;
	gc_set_power(gccorecontext, GCPWR_OFF);
	mutex_unlock(&gccorecontext->mmucontextlock);

	GCPRINT(GCDBGFILTER, GCZONE_POWER, "--" GC_MOD_PREFIX
		"\n", __func__, __LINE__);
}

static void gc_late_resume(struct early_suspend *h)
{
	struct gccorecontext *gccorecontext = &g_context;

	mutex_lock(&gccorecontext->mmucontextlock);
	gccorecontext->forceoff = false;
	mutex_unlock(&gccorecontext->mmucontextlock);
}

static struct early_suspend early_suspend_info = {
	.suspend = gc_early_suspend,
	.resume = gc_late_resume,
	.level = EARLY_SUSPEND_LEVEL_DISABLE_FB,
};
#endif /* CONFIG_HAS_EARLYSUSPEND */

/*******************************************************************************
 * Driver init/shutdown.
 */

static int gc_init(void);
static void gc_exit(void);

static int gc_init(void)
{
	int result;
	struct gccorecontext *gccorecontext = &g_context;

	GCPRINT(GCDBGFILTER, GCZONE_INIT, "++" GC_MOD_PREFIX
		"\n", __func__, __LINE__);

	/* check if hardware is available */
	if (!cpu_is_omap447x()) {
		GCPRINT(GCDBGFILTER, GCZONE_INIT, GC_MOD_PREFIX
			"gcx hardware is not present\n",
			__func__, __LINE__);

		goto exit;
	}

	/* Initialize data structutres. */
	mutex_init(&gccorecontext->mmucontextlock);
	init_completion(&gccorecontext->intready);
	INIT_LIST_HEAD(&gccorecontext->mmuctxlist);
	INIT_LIST_HEAD(&gccorecontext->mmuctxvac);

	gccorecontext->bb2ddevice = omap_hwmod_name_get_dev("bb2d");
	if (gccorecontext->bb2ddevice == NULL) {
		GCPRINT(NULL, 0, GC_MOD_PREFIX
			"cannot find bb2d device.\n",
			 __func__, __LINE__);
		result = -EINVAL;
		goto fail;
	}

	result = platform_driver_register(&plat_drv);
	if (result < 0) {
		GCPRINT(NULL, 0, GC_MOD_PREFIX
			"failed to register platform driver.\n",
			 __func__, __LINE__);
		goto fail;
	}
	gccorecontext->platdriver = true;

#if defined(CONFIG_HAS_EARLYSUSPEND)
	register_early_suspend(&early_suspend_info);
#endif

	/* Initialize the command buffer. */
	if (cmdbuf_init() != GCERR_NONE) {
		GCPRINT(NULL, 0, GC_MOD_PREFIX
			"failed to initialize command buffer.\n",
			 __func__, __LINE__);
		result = -EINVAL;
		goto fail;
	}

	/* Create debugfs entry */
	gccorecontext->dbgroot = debugfs_create_dir("gcx", NULL);
	if (gccorecontext->dbgroot)
		gc_debug_init(gccorecontext->dbgroot);

exit:
	GCPRINT(GCDBGFILTER, GCZONE_PROBE, "--" GC_MOD_PREFIX
		"\n", __func__, __LINE__);

	return 0;

fail:
	gc_exit();

	GCPRINT(GCDBGFILTER, GCZONE_PROBE, "--" GC_MOD_PREFIX
		"result = %d\n", __func__, __LINE__, result);

	return result;
}

static void gc_exit(void)
{
	struct gccorecontext *gccorecontext = &g_context;

	GCPRINT(GCDBGFILTER, GCZONE_INIT, "++" GC_MOD_PREFIX
		"\n", __func__, __LINE__);

	if (cpu_is_omap447x()) {
		destroy_mmu_context(gccorecontext);
		gc_set_power(gccorecontext, GCPWR_OFF);
		pm_runtime_disable(gccorecontext->device);

		if (gccorecontext->platdriver) {
			platform_driver_unregister(&plat_drv);
			gccorecontext->platdriver = false;
		}

#if defined(CONFIG_HAS_EARLYSUSPEND)
		unregister_early_suspend(&early_suspend_info);
#endif

		if (gccorecontext->regbase != NULL) {
			iounmap(gccorecontext->regbase);
			gccorecontext->regbase = NULL;
		}

		if (gccorecontext->dbgroot) {
			debugfs_remove_recursive(gccorecontext->dbgroot);
			gccorecontext->dbgroot = NULL;
		}

		mutex_destroy(&gccorecontext->mmucontextlock);

		if (gccorecontext->isrroutine) {
			free_irq(gccorecontext->irqline, gccorecontext);
			gccorecontext->isrroutine = false;
		}
	}

	GCPRINT(GCDBGFILTER, GCZONE_PROBE, "--" GC_MOD_PREFIX
		"\n", __func__, __LINE__);
}

static int __init gc_init_wrapper(void)
{
	return gc_init();
}

static void __exit gc_exit_wrapper(void)
{
	gc_exit();
}

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("www.vivantecorp.com");
MODULE_AUTHOR("www.ti.com");
module_init(gc_init_wrapper);
module_exit(gc_exit_wrapper);
