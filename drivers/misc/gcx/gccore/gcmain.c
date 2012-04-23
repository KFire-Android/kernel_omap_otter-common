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
#include <linux/clk.h>
#include <linux/dma-mapping.h>
#include <plat/cpu.h>
#include <linux/debugfs.h>
#include <plat/omap_gcx.h>
#include <linux/delay.h>

#include "gcmain.h"

#define GC_ENABLE_SUSPEND

#define GCZONE_ALL		(~0U)
#define GCZONE_CONTEXT		(1 << 0)
#define GCZONE_POWER		(1 << 1)
#define GCZONE_PAGE		(1 << 2)
#define GCZONE_COMMIT		(1 << 3)
#define GCZONE_MAPPING		(1 << 4)

#include <linux/gcx.h>
#include <linux/gccore.h>
#include "gccmdbuf.h"
#include "gcmmu.h"
#include <linux/gcdebug.h>

#define DEVICE_INT	(32 + 125)
#define DEVICE_REG_BASE	0x59000000
#define DEVICE_REG_SIZE	(32 * 1024)
#define GC_POLL_PRCM_STBY 100

/* Driver context structure. */
struct gccontext {
	struct mmu2dcontext mmu;
	int mmu_dirty;
};

struct gccore {
	void *priv;
};

static struct gccore gcdevice;
static bool g_irqinstalled;

static struct mutex mtx;
static struct dentry *g_debugRoot;

struct gccontextmap {
	pid_t pid;
	struct gccontext *context;
	struct gccontextmap *prev;
	struct gccontextmap *next;
};
static struct mutex g_maplock;
static struct gccontextmap *g_map;
static struct gccontextmap *g_mapvacant;
static int g_clientref;

static void *g_reg_base;
static struct omap_gcx_platform_data *g_gcxplat;
static bool gforceoff; /* protected by mtx */

static enum gcerror find_context(struct gccontextmap **context, int create)
{
	enum gcerror gcerror = GCERR_NONE;
	struct gccontextmap *prev;
	struct gccontextmap *curr;
	pid_t pid;

	/* Get current PID. */
	pid = 0;

	/* Search the list. */
	prev = NULL;
	curr = g_map;

	GCPRINT(GCDBGFILTER, GCZONE_CONTEXT, GC_MOD_PREFIX
		"scanning existing records for pid %d.\n",
		__func__, __LINE__, pid);

	/* Try to locate the record. */
	while (curr != NULL) {
		/* Found the record? */
		if (curr->pid == pid) {
			/* Move to the top of the list. */
			if (prev != NULL) {
				prev->next = curr->next;
				curr->next = g_map;
				g_map = curr;
			}

			/* Success. */
			GCPRINT(GCDBGFILTER, GCZONE_CONTEXT, GC_MOD_PREFIX
				"record is found @ 0x%08X\n",
				__func__, __LINE__, (unsigned int) curr);

			*context = curr;
			goto exit;
		}

		/* Get the next record. */
		prev = curr;
		curr = curr->next;
	}

	/* Not found, do we need to create a new one? */
	if (!create) {
		GCPRINT(GCDBGFILTER, GCZONE_CONTEXT, GC_MOD_PREFIX
			"not found, exiting.\n",
			__func__, __LINE__);
		gcerror = GCERR_NOT_FOUND;
		goto exit;
	}

	/* Get new record. */
	if (g_mapvacant == NULL) {
		GCPRINT(GCDBGFILTER, GCZONE_CONTEXT, GC_MOD_PREFIX
			"not found, allocating.\n",
			__func__, __LINE__);

		curr = kmalloc(sizeof(struct gccontextmap), GFP_KERNEL);
		if (curr == NULL) {
			GCPRINT(NULL, 0, GC_MOD_PREFIX
				"out of memory.\n",
				__func__, __LINE__);
			gcerror = GCERR_SETGRP(GCERR_OODM,
						GCERR_IOCTL_CTX_ALLOC);
			goto exit;
		}

		GCPRINT(GCDBGFILTER, GCZONE_CONTEXT, GC_MOD_PREFIX
			"allocated @ 0x%08X\n",
			__func__, __LINE__, (unsigned int) curr);
	} else {
		GCPRINT(GCDBGFILTER, GCZONE_CONTEXT, GC_MOD_PREFIX
			"not found, reusing record @ 0x%08X\n",
			__func__, __LINE__, (unsigned int) g_mapvacant);

		curr = g_mapvacant;
		g_mapvacant = g_mapvacant->next;
	}

	GCPRINT(GCDBGFILTER, GCZONE_CONTEXT, GC_MOD_PREFIX
		"creating new context.\n",
		__func__, __LINE__);

	curr->context = kzalloc(sizeof(*curr->context), GFP_KERNEL);
	if (curr->context == NULL) {
		gcerror = GCERR_SETGRP(GCERR_OODM, GCERR_CTX_ALLOC);
		goto exit;
	}

	gcerror = mmu2d_create_context(&curr->context->mmu);
	if (gcerror != GCERR_NONE)
		goto free_map_ctx;

#if MMU_ENABLE
	gcerror = cmdbuf_map(&curr->context->mmu);
	if (gcerror != GCERR_NONE)
		goto free_2d_ctx;
#endif

	curr->context->mmu_dirty = true;

	g_clientref += 1;

	/* Success? */
	if (gcerror == GCERR_NONE) {
		GCPRINT(GCDBGFILTER, GCZONE_CONTEXT, GC_MOD_PREFIX
			"new context created @ 0x%08X\n",
			__func__, __LINE__, (unsigned int) curr->context);

		/* Set the PID. */
		curr->pid = pid;

		/* Add to the list. */
		curr->prev = NULL;
		curr->next = g_map;
		if (g_map != NULL)
			g_map->prev = curr;
		g_map = curr;

		/* Set return value. */
		*context = curr;
	} else {
		GCPRINT(GCDBGFILTER, GCZONE_CONTEXT, GC_MOD_PREFIX
			"failed to create a context.\n",
			__func__, __LINE__);

		/* Add the record to the vacant list. */
		curr->next = g_mapvacant;
		g_mapvacant = curr;
	}
	goto exit;

free_2d_ctx:
	mmu2d_destroy_context(&curr->context->mmu);
free_map_ctx:
	kfree(curr->context);
exit:
	return gcerror;
}

static enum gcerror release_context(struct gccontextmap *context)
{
	enum gcerror gcerror;

	/* Remove from the list. */
	if (context->prev == NULL) {
		if (context != g_map) {
			gcerror = GCERR_NOT_FOUND;
			goto exit;
		}

		g_map = context->next;
		g_map->prev = NULL;
	} else {
		context->prev->next = context->next;
		context->next->prev = context->prev;
	}

	if (context->context != NULL) {
		gcerror = mmu2d_destroy_context(&context->context->mmu);
		if (gcerror != GCERR_NONE)
			goto exit;

		kfree(context->context);
		context->context = NULL;

		g_clientref -= 1;
	}

	kfree(context);

exit:
	return gcerror;
}

static void delete_context_map(void)
{
	struct gccontextmap *curr;

	while (g_map != NULL)
		release_context(g_map);

	while (g_mapvacant != NULL) {
		curr = g_mapvacant;
		g_mapvacant = g_mapvacant->next;
		kfree(curr);
	}
}

/*******************************************************************************
** Register access.
*/

unsigned int gc_read_reg(unsigned int address)
{
	return readl((unsigned char *) g_reg_base + address);
}

void gc_write_reg(unsigned int address, unsigned int data)
{
	writel(data, (unsigned char *) g_reg_base + address);
}

/*******************************************************************************
 * Page allocation routines.
 */

enum gcerror gc_alloc_pages(struct gcpage *p, unsigned int size)
{
	enum gcerror gcerror;
	int order;

	p->pages = NULL;
	p->logical = NULL;
	p->physical = ~0UL;

	order = get_order(size);

	p->order = order;
	p->size = (1 << order) * PAGE_SIZE;

	GCPRINT(GCDBGFILTER, GCZONE_PAGE, GC_MOD_PREFIX
		"requested size=%d\n", __func__, __LINE__, size);

	GCPRINT(GCDBGFILTER, GCZONE_PAGE, GC_MOD_PREFIX
		"aligned size=%d\n", __func__, __LINE__, p->size);

	GCPRINT(GCDBGFILTER, GCZONE_PAGE, GC_MOD_PREFIX
		"order=%d\n", __func__, __LINE__, order);

	p->logical = dma_alloc_coherent(NULL, p->size, &p->physical,
								GFP_KERNEL);
	if (!p->logical) {
		gcerror = GCERR_OOPM;
		goto fail;
	}

	GCPRINT(GCDBGFILTER, GCZONE_PAGE, GC_MOD_PREFIX
		"container = 0x%08X\n",
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

	return GCERR_NONE;

fail:
	gc_free_pages(p);
	return gcerror;
}

void gc_free_pages(struct gcpage *p)
{
	GCPRINT(GCDBGFILTER, GCZONE_PAGE, GC_MOD_PREFIX
		"container = 0x%08X\n",
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
		dma_free_coherent(NULL, p->size, p->logical, p->physical);
		p->logical = NULL;
	}

	p->physical = ~0UL;
	p->order = 0;
	p->size = 0;
}

void gc_flush_pages(struct gcpage *p)
{
	GCPRINT(GCDBGFILTER, GCZONE_PAGE, GC_MOD_PREFIX
		"container = 0x%08X\n",
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
}

/*******************************************************************************
 * Interrupt handling.
 */

struct completion g_gccoreint;
static unsigned int g_gccoredata;

void gc_wait_interrupt(void)
{
	gc_wait_completion(&g_gccoreint, GC_INFINITE);
}

unsigned int gc_get_interrupt_data(void)
{
	unsigned int data;

	data = g_gccoredata;
	g_gccoredata = 0;

	return data;
}

static irqreturn_t gc_irq(int irq, void *p)
{
	unsigned int data;

	/* Read gcregIntrAcknowledge register. */
	data = gc_read_reg(GCREG_INTR_ACKNOWLEDGE_Address);

	/* Our interrupt? */
	if (data == 0)
		return IRQ_NONE;

	gc_debug_cache_gpu_status_from_irq(data);

	g_gccoredata = data;
	complete(&g_gccoreint);

	return IRQ_HANDLED;
}

/*******************************************************************************
 * GPU power level control.
 */

#include <plat/omap_hwmod.h>
#include <plat/omap-pm.h>

static struct clk *g_bb2d_clk;
struct device *g_bb2d_dev;
static enum gcpower g_gcpower = GCPWR_UNKNOWN;
static bool g_clockenabled;
static bool g_irqenabled;
static bool g_pulseskipping;

void gc_reset_gpu(void)
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
		gc_delay(1);

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
	g_pulseskipping = false;

	GCPRINT(GCDBGFILTER, GCZONE_POWER, GC_MOD_PREFIX
		"gpu reset.\n",
		__func__, __LINE__);
}

enum gcerror gcpwr_enable_clock(enum gcpower prevstate)
{
	enum gcerror gcerror = GCERR_NONE;
	int ret;

	if (!g_clockenabled) {
		/* Enable the clock. */
		ret = clk_enable(g_bb2d_clk);
		if (ret < 0) {
			GCPRINT(NULL, 0, GC_MOD_PREFIX
					" failed to enable bb2d_fck (%d).\n",
					__func__, __LINE__, ret);
			gcerror = GCERR_POWER_CLOCK_ON;
			goto exit;
		}

		/* Signal software not idle. */
		gc_write_reg(GC_GP_OUT0_Address, 0);

		/* Clock enabled. */
		g_clockenabled = true;
	}
	GCPRINT(GCDBGFILTER, GCZONE_POWER, GC_MOD_PREFIX
		"clock %s.\n",
		__func__, __LINE__, g_clockenabled ? "enabled" : "disabled");

	if (prevstate == GCPWR_UNKNOWN)
		gc_reset_gpu();

exit:
	return gcerror;
}

void gcpwr_disable_clock(enum gcpower prevstate)
{
	u32 trys = 0;
	if (!g_clockenabled)
		return;

	gc_debug_poweroff_cache();

	/* Signal software idle. */
	gc_write_reg(GC_GP_OUT0_Address, 1);
	while (!g_gcxplat->prcm_bb2d_idlest() && ++trys != GC_POLL_PRCM_STBY)
		udelay(100);
	/* Not much we can do here, so assert */
	WARN(trys == GC_POLL_PRCM_STBY, "gccore device not in idle\n");
	/* Disable the clock. */
	clk_disable(g_bb2d_clk);

	/* Clock disabled. */
	g_clockenabled = false;
	GCPRINT(GCDBGFILTER, GCZONE_POWER, GC_MOD_PREFIX
			"clock disabled.\n",
			__func__, __LINE__);
}

void gcpwr_enable_pulse_skipping(enum gcpower prevstate)
{
	union gcclockcontrol gcclockcontrol;

	if (!g_clockenabled)
		return;

	if (g_pulseskipping) {
		GCPRINT(GCDBGFILTER, GCZONE_POWER, GC_MOD_PREFIX
			"pulse skipping is already enabled.\n",
			__func__, __LINE__);
	} else {
		omap_pm_set_min_bus_tput(g_bb2d_dev, OCP_INITIATOR_AGENT, -1);

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
		g_pulseskipping = true;
		GCPRINT(GCDBGFILTER, GCZONE_POWER, GC_MOD_PREFIX
			"pulse skipping enabled.\n",
			__func__, __LINE__);
	}
}

void gcpwr_disable_pulse_skipping(enum gcpower prevstate)
{
	union gcclockcontrol gcclockcontrol;

	if (!g_clockenabled)
		return;

	if (g_pulseskipping) {
		/* Set the min l3 data throughput */
		omap_pm_set_min_bus_tput(g_bb2d_dev, OCP_INITIATOR_AGENT,
						200*1000*4);

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
		g_pulseskipping = false;
		GCPRINT(GCDBGFILTER, GCZONE_POWER, GC_MOD_PREFIX
			"pulse skipping disabled.\n",
			__func__, __LINE__);
	} else {
		GCPRINT(GCDBGFILTER, GCZONE_POWER, GC_MOD_PREFIX
			"pulse skipping is already disabled.\n",
			__func__, __LINE__);
	}
}

enum gcerror gc_set_power(enum gcpower gcpower)
{
	enum gcerror gcerror = GCERR_NONE;

	if (gcpower == GCPWR_UNKNOWN) {
		gcerror = GCERR_POWER_MODE;
		goto exit;
	}

	if (gcpower != g_gcpower) {
		GCPRINT(GCDBGFILTER, GCZONE_POWER, GC_MOD_PREFIX
			"power state %d --> %d\n",
			__func__, __LINE__, g_gcpower, gcpower);

		switch (gcpower) {
		case GCPWR_ON:
			gcerror = gcpwr_enable_clock(g_gcpower);
			if (gcerror != GCERR_NONE)
				goto exit;

			gcpwr_disable_pulse_skipping(g_gcpower);

			if (!g_irqenabled) {
				enable_irq(DEVICE_INT);
				g_irqenabled = true;
			}
			break;

		case GCPWR_LOW:
			gcpwr_enable_pulse_skipping(g_gcpower);
			break;

		case GCPWR_OFF:
			gcpwr_disable_clock(g_gcpower);
			if (g_irqenabled) {
				disable_irq(DEVICE_INT);
				g_irqenabled = false;
			}
			break;

		default:
			gcerror = GCERR_POWER_MODE;
			goto exit;
		}

		/* Set new power state. */
		g_gcpower = gcpower;
	}

exit:
	return gcerror;
}

enum gcerror gc_get_power(void)
{
	return g_gcpower;
}

/*******************************************************************************
 * Command buffer submission.
 */

void gc_commit(struct gccommit *gccommit, int fromuser)
{
	struct gcbuffer *gcbuffer;
	unsigned int cmdflushsize;
	unsigned int mmuflushsize;
	unsigned int buffersize;
	unsigned int allocsize;
	unsigned int *logical;
	unsigned int address;
	struct gcmopipesel *gcmopipesel;
	struct gccontextmap *context;
	struct gccommit kgccommit;

	GCPRINT(GCDBGFILTER, GCZONE_COMMIT, "++" GC_MOD_PREFIX
		"\n", __func__, __LINE__);

	mutex_lock(&mtx);

	/* Enable power to the chip. */
	gc_set_power(GCPWR_ON);

	/* Locate the client entry. */
	kgccommit.gcerror = find_context(&context, true);
	if (kgccommit.gcerror != GCERR_NONE)
		goto exit;

	context->context->mmu_dirty = true;

	/* Set the client's master table. */
	gccommit->gcerror = mmu2d_set_master(&context->context->mmu);
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
		mmuflushsize = context->context->mmu_dirty
			? mmu2d_flush(NULL, 0, 0) : 0;

		/* Reserve command buffer space. */
		allocsize = mmuflushsize + buffersize + cmdflushsize;
		gccommit->gcerror = cmdbuf_alloc(allocsize,
						(void **) &logical, &address);
		if (gccommit->gcerror != GCERR_NONE)
			goto exit;

		/* Append MMU flush. */
		if (context->context->mmu_dirty) {
			mmu2d_flush(logical, address, allocsize);

			/* Skip MMU flush. */
			logical = (unsigned int *)
				((unsigned char *) logical + mmuflushsize);

			/* Validate MMU state. */
			context->context->mmu_dirty = false;
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
		gccommit->gcerror = mmu2d_fixup(gcbuffer->fixuphead, logical);
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
	gc_set_power(GCPWR_LOW);
	if (gforceoff)
		gc_set_power(GCPWR_OFF);

	mutex_unlock(&mtx);

	GCPRINT(GCDBGFILTER, GCZONE_COMMIT, "--" GC_MOD_PREFIX
		"\n", __func__, __LINE__);
}
EXPORT_SYMBOL(gc_commit);

void gc_map(struct gcmap *gcmap)
{
	struct mmu2dphysmem mem;
	struct mmu2darena *mapped = NULL;
	struct gccontextmap *context;
	struct gcmap kgcmap;

	GCPRINT(GCDBGFILTER, GCZONE_MAPPING, "++" GC_MOD_PREFIX
		"\n", __func__, __LINE__);

	mutex_lock(&mtx);

	/* Locate the client entry. */
	kgcmap.gcerror = find_context(&context, true);
	if (kgcmap.gcerror != GCERR_NONE)
		goto exit;

	context->context->mmu_dirty = true;

	GCPRINT(GCDBGFILTER, GCZONE_MAPPING, GC_MOD_PREFIX
		"map client buffer\n",
		__func__, __LINE__);

	GCPRINT(GCDBGFILTER, GCZONE_MAPPING, GC_MOD_PREFIX
		"  logical = 0x%08X\n",
		__func__, __LINE__, (unsigned int) gcmap->logical);

	GCPRINT(GCDBGFILTER, GCZONE_MAPPING, GC_MOD_PREFIX
		"  size = %d\n",
		__func__, __LINE__, gcmap->size);

	/* Initialize the mapping parameters. See if we were passed a list
	 * of pages first
	 */
	if (gcmap->pagecount > 0 && gcmap->pagearray != NULL) {
		GCPRINT(GCDBGFILTER, GCZONE_MAPPING, GC_MOD_PREFIX
			"got page array %p with %lu pages",
			__func__, __LINE__, gcmap->pagearray, gcmap->pagecount);
		mem.base = 0;
		mem.offset = 0;
		mem.count = gcmap->pagecount;
		mem.pages = gcmap->pagearray;
	} else {
		GCPRINT(GCDBGFILTER, GCZONE_MAPPING, GC_MOD_PREFIX
			"gcmap->logical = %p\n",
			__func__, __LINE__, gcmap->logical);
		mem.base = ((u32) gcmap->logical) & ~(PAGE_SIZE - 1);
		mem.offset = ((u32) gcmap->logical) & (PAGE_SIZE - 1);
		mem.count = DIV_ROUND_UP(gcmap->size + mem.offset, PAGE_SIZE);
		mem.pages = NULL;
	}
	mem.pagesize = PAGE_SIZE;

	/* Map the buffer. */
	gcmap->gcerror = mmu2d_map(&context->context->mmu, &mem, &mapped);
	if (gcmap->gcerror != GCERR_NONE)
		goto exit;

	/* Invalidate the MMU. */
	context->context->mmu_dirty = true;

	gcmap->handle = (unsigned int) mapped;

	GCPRINT(GCDBGFILTER, GCZONE_MAPPING, GC_MOD_PREFIX
		"  mapped address = 0x%08X\n",
		__func__, __LINE__, mapped->address);

	GCPRINT(GCDBGFILTER, GCZONE_MAPPING, GC_MOD_PREFIX
		"  handle = 0x%08X\n",
		__func__, __LINE__, (unsigned int) mapped);

exit:
	mutex_unlock(&mtx);

	GCPRINT(GCDBGFILTER, GCZONE_MAPPING, "--" GC_MOD_PREFIX
		"\n", __func__, __LINE__);
}
EXPORT_SYMBOL(gc_map);

void gc_unmap(struct gcmap *gcmap)
{
	struct gccontextmap *context;
	struct gcmap kgcmap;

	GCPRINT(GCDBGFILTER, GCZONE_MAPPING, "++" GC_MOD_PREFIX
		"\n", __func__, __LINE__);

	mutex_lock(&mtx);

	/* Locate the client entry. */
	kgcmap.gcerror = find_context(&context, true);
	if (kgcmap.gcerror != GCERR_NONE)
		goto exit;

	context->context->mmu_dirty = true;

	GCPRINT(GCDBGFILTER, GCZONE_MAPPING, GC_MOD_PREFIX
		"unmap client buffer\n",
		__func__, __LINE__);

	GCPRINT(GCDBGFILTER, GCZONE_MAPPING, GC_MOD_PREFIX
		"  logical = 0x%08X\n",
		__func__, __LINE__, (unsigned int) gcmap->logical);

	GCPRINT(GCDBGFILTER, GCZONE_MAPPING, GC_MOD_PREFIX
		"  size = %d\n",
		__func__, __LINE__, gcmap->size);

	GCPRINT(GCDBGFILTER, GCZONE_MAPPING, GC_MOD_PREFIX
		"  handle = 0x%08X\n",
		__func__, __LINE__, gcmap->handle);

	/* Map the buffer. */
	gcmap->gcerror = mmu2d_unmap(&context->context->mmu,
					(struct mmu2darena *) gcmap->handle);
	if (gcmap->gcerror != GCERR_NONE)
		goto exit;

	/* Invalidate the MMU. */
	context->context->mmu_dirty = true;

	/* Invalidate the handle. */
	gcmap->handle = ~0U;

exit:
	mutex_unlock(&mtx);

	GCPRINT(GCDBGFILTER, GCZONE_MAPPING, "--" GC_MOD_PREFIX
		"\n", __func__, __LINE__);
}
EXPORT_SYMBOL(gc_unmap);

static int gc_probe(struct platform_device *pdev)
{
	g_gcxplat = (struct omap_gcx_platform_data *)pdev->dev.platform_data;
	return 0;
}

#if defined(GC_ENABLE_SUSPEND)
static int gc_suspend(struct platform_device *pdev, pm_message_t s)
{
	GCPRINT(GCDBGFILTER, GCZONE_POWER, "++" GC_MOD_PREFIX
		"\n", __func__, __LINE__);
	if (gc_set_power(GCPWR_OFF))
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
	mutex_lock(&mtx);
	gforceoff = true;
	gc_set_power(GCPWR_OFF);
	mutex_unlock(&mtx);
}

static void gc_late_resume(struct early_suspend *h)
{
	mutex_lock(&mtx);
	gforceoff = false;
	mutex_unlock(&mtx);
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

static int __init gc_init(void)
{
	int ret;

	/* check if hardware is available */
	if (!cpu_is_omap447x())
		return 0;

	/* Initialize context mutex. */
	mutex_init(&mtx);

	/* Initialize interrupt completion. */
	init_completion(&g_gccoreint);

	g_bb2d_clk = clk_get(NULL, "bb2d_fck");
	if (IS_ERR(g_bb2d_clk)) {
		GCPRINT(NULL, 0, GC_MOD_PREFIX
			"cannot find bb2d_fck.\n",
			 __func__, __LINE__);
		goto fail;
	}

	GCPRINT(GCDBGFILTER, GCZONE_POWER, GC_MOD_PREFIX
		"BB2D clock is %ldMHz\n",
		__func__, __LINE__, (clk_get_rate(g_bb2d_clk) / 1000000));

	g_bb2d_dev = omap_hwmod_name_get_dev("bb2d");
	if (g_bb2d_dev == NULL) {
		GCPRINT(NULL, 0, GC_MOD_PREFIX
			"cannot find bb2d_fck.\n",
			 __func__, __LINE__);
		goto fail;
	}

	/* Map GPU registers. */
	g_reg_base = ioremap_nocache(DEVICE_REG_BASE, DEVICE_REG_SIZE);
	if (g_reg_base == NULL) {
		GCPRINT(NULL, 0, GC_MOD_PREFIX
			"failed to map registers.\n",
			__func__, __LINE__);
		goto fail;
	}

	/* Install IRQ. */
	ret = request_irq(DEVICE_INT, gc_irq, IRQF_SHARED,
				GC_DEV_NAME, &gcdevice);
	if (ret < 0) {
		GCPRINT(NULL, 0, GC_MOD_PREFIX
			"failed to install IRQ (%d).\n",
			__func__, __LINE__, ret);
		goto fail;
	}

	g_irqinstalled = true;

	/* Disable IRQ. */
	disable_irq(DEVICE_INT);
	g_irqenabled = false;

	/* Initialize the command buffer. */
	if (cmdbuf_init() != GCERR_NONE) {
		GCPRINT(NULL, 0, GC_MOD_PREFIX
			"failed to initialize command buffer.\n",
			 __func__, __LINE__);
		goto fail;
	}

	/* Create debugfs entry */
	g_debugRoot = debugfs_create_dir("gcx", NULL);
	if (g_debugRoot)
		gc_debug_init(g_debugRoot);

	mutex_init(&g_maplock);

#if defined(CONFIG_HAS_EARLYSUSPEND)
	register_early_suspend(&early_suspend_info);
#endif

	return platform_driver_register(&plat_drv);
fail:
	if (g_irqinstalled)
		free_irq(DEVICE_INT, &gcdevice);

	if (g_reg_base != NULL) {
		iounmap(g_reg_base);
		g_reg_base = NULL;
	}

	if (g_bb2d_clk)
		clk_put(g_bb2d_clk);

	return -EINVAL;
}

static void __exit gc_exit(void)
{
	if (!cpu_is_omap447x())
		return;

	platform_driver_unregister(&plat_drv);
#if defined(CONFIG_HAS_EARLYSUSPEND)
	unregister_early_suspend(&early_suspend_info);
#endif
	delete_context_map();
	mutex_destroy(&g_maplock);
	gc_set_power(GCPWR_OFF);

	if (g_reg_base != NULL) {
		iounmap(g_reg_base);
		g_reg_base = NULL;
	}

	if (g_bb2d_clk)
		clk_put(g_bb2d_clk);

	if (g_debugRoot)
		debugfs_remove_recursive(g_debugRoot);

	mutex_destroy(&mtx);

	if (g_irqinstalled)
		free_irq(DEVICE_INT, &gcdevice);
}

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("www.vivantecorp.com");
MODULE_AUTHOR("www.ti.com");
module_init(gc_init);
module_exit(gc_exit);
