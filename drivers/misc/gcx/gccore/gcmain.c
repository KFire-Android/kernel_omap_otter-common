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

#include <linux/gcx.h>
#include <linux/gccore.h>
#include "gcmain.h"
#include "gccmdbuf.h"
#include "gcmmu.h"

#ifndef GC_DUMP
#	define GC_DUMP 0
#endif

#if GC_DUMP
#	define GC_PRINT printk
#else
#	define GC_PRINT(...)
#endif

#define GC_DETECT_TIMEOUT 0

#define DEVICE_INT	(32 + 125)
#define DEVICE_REG_BASE	0x59000000
#define DEVICE_REG_SIZE	(256 * 1024)

/* Driver context structure. */
struct gccontext {
	struct mmu2dcontext mmu;
	int mmu_dirty;
};

struct gccore {
	void *priv;
};

static struct gccore gcdevice;

static int irqline = 48;
module_param(irqline, int, 0644);

static long registerMemBase = 0xF1840000;
module_param(registerMemBase, long, 0644);

static struct mutex g_contextlock;
static struct mutex g_datalock;

struct gccontextmap {
	pid_t pid;
	struct gccontext *context;
	struct gccontextmap *prev;
	struct gccontextmap *next;
};
static struct mutex g_maplock;
static struct gccontextmap *g_map;
static struct gccontextmap *g_mapvacant;

static enum gcerror find_context(struct gccontextmap **context, int create)
{
	enum gcerror gcerror;
	struct gccontextmap *prev;
	struct gccontextmap *curr;
	int maplocked = 0;
	pid_t pid;

	GC_PRINT(GC_INFO_MSG " getting lock mutex.\n", __func__, __LINE__);

	/* Acquire map access mutex. */
	gcerror = gc_acquire_mutex(&g_maplock, GC_INFINITE);
	if (gcerror != GCERR_NONE) {
		GC_PRINT(GC_ERR_MSG " failed to acquire mutex (0x%08X).\n",
				__func__, __LINE__, gcerror);
		gcerror = GCERR_SETGRP(gcerror, GCERR_IOCTL_CTX_ALLOC);
		goto exit;
	}
	maplocked = 1;

	/* Get current PID. */
	pid = 0;

	/* Search the list. */
	prev = NULL;
	curr = g_map;

	GC_PRINT(GC_INFO_MSG " scanning existing records for pid %d.\n",
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
			GC_PRINT(GC_INFO_MSG " record is found @ 0x%08X\n",
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
		GC_PRINT(GC_INFO_MSG " not found, exiting.\n",
			__func__, __LINE__);
		gcerror = GCERR_NOT_FOUND;
		goto exit;
	}

	/* Get new record. */
	if (g_mapvacant == NULL) {
		GC_PRINT(GC_INFO_MSG " not found, allocating.\n",
			__func__, __LINE__);

		curr = kmalloc(sizeof(struct gccontextmap), GFP_KERNEL);
		if (curr == NULL) {
			GC_PRINT(GC_ERR_MSG " out of memory.\n",
					__func__, __LINE__);
			gcerror = GCERR_SETGRP(GCERR_OODM,
						GCERR_IOCTL_CTX_ALLOC);
			goto exit;
		}

		GC_PRINT(GC_INFO_MSG " allocated @ 0x%08X\n",
			__func__, __LINE__, (unsigned int) curr);
	} else {
		GC_PRINT(GC_INFO_MSG " not found, reusing record @ 0x%08X\n",
			__func__, __LINE__, (unsigned int) g_mapvacant);

		curr = g_mapvacant;
		g_mapvacant = g_mapvacant->next;
	}

	GC_PRINT(GC_INFO_MSG " creating new context.\n",
		__func__, __LINE__);

	/* Create the context. */
	gcerror = gc_attach(&curr->context);

	/* Success? */
	if (gcerror == GCERR_NONE) {
		GC_PRINT(GC_INFO_MSG " new context created @ 0x%08X\n",
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
		GC_PRINT(GC_INFO_MSG " failed to create a context.\n",
			__func__, __LINE__);

		/* Add the record to the vacant list. */
		curr->next = g_mapvacant;
		g_mapvacant = curr;
	}

exit:
	if (maplocked)
		mutex_unlock(&g_maplock);

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
		gcerror = gc_detach(&context->context);
		if (gcerror != GCERR_NONE)
			goto exit;
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

static void *g_reg_base;

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
	void *logical;
	int order, count;

	p->pages = NULL;
	p->logical = NULL;
	p->physical = ~0UL;

	order = get_order(size);

	p->order = order;
	p->size = (1 << order) * PAGE_SIZE;

	GC_PRINT(GC_INFO_MSG " requested size=%d\n", __func__, __LINE__, size);
	GC_PRINT(GC_INFO_MSG " aligned size=%d\n", __func__, __LINE__, p->size);
	GC_PRINT(GC_INFO_MSG " order=%d\n", __func__, __LINE__, order);

	p->pages = alloc_pages(GFP_KERNEL, order);
	if (p->pages == NULL) {
		gcerror = GCERR_OOPM;
		goto fail;
	}

	p->physical = page_to_phys(p->pages);
	p->logical = (unsigned int *) page_address(p->pages);

	if (p->logical == NULL) {
		gcerror = GCERR_PMMAP;
		goto fail;
	}

	/* Reserve pages. */
	logical = p->logical;
	count = p->size / PAGE_SIZE;

	while (count) {
		SetPageReserved(virt_to_page(logical));

		logical = (unsigned char *) logical + PAGE_SIZE;
		count  -= 1;
	}

	GC_PRINT(GC_INFO_MSG " (0x%08X) pages=0x%08X,"
				" logical=0x%08X, physical=0x%08X, size=%d\n",
				__func__, __LINE__,
				(unsigned int) p,
				(unsigned int) p->pages,
				(unsigned int) p->logical,
				(unsigned int) p->physical,
				p->size);

	return GCERR_NONE;

fail:
	gc_free_pages(p);
	return gcerror;
}

void gc_free_pages(struct gcpage *p)
{
	GC_PRINT(GC_INFO_MSG " (0x%08X) pages=0x%08X,"
				" logical=0x%08X, physical=0x%08X, size=%d\n",
				__func__, __LINE__,
				(unsigned int) p,
				(unsigned int) p->pages,
				(unsigned int) p->logical,
				(unsigned int) p->physical,
				p->size);

	if (p->logical != NULL) {
		void *logical;
		int count;

		logical = p->logical;
		count = p->size / PAGE_SIZE;

		while (count) {
			ClearPageReserved(virt_to_page(logical));

			logical = (unsigned char *) logical + PAGE_SIZE;
			count  -= 1;
		}

		p->logical = NULL;
	}

	if (p->pages != NULL) {
		__free_pages(p->pages, p->order);
		p->pages = NULL;
	}

	p->physical = ~0UL;
	p->order = 0;
	p->size = 0;
}

void gc_flush_pages(struct gcpage *p)
{
	GC_PRINT(GC_INFO_MSG " (0x%08X) pages=0x%08X,"
				" logical=0x%08X, physical=0x%08X, size=%d\n",
				__func__, __LINE__,
				(unsigned int) p,
				(unsigned int) p->pages,
				(unsigned int) p->logical,
				(unsigned int) p->physical,
				p->size);

	dmac_flush_range(p->logical, (unsigned char *) p->logical + p->size);
	outer_flush_range(p->physical, p->physical + p->size);
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

#if 0
static struct workqueue_struct *gcwq;
DECLARE_WAIT_QUEUE_HEAD(gc_event);
int done;

static void gc_work(struct work_struct *ignored)
{
	done = true;
	wake_up_interruptible(&gc_event);
}
static DECLARE_WORK(gcwork, gc_work);
#endif

static irqreturn_t gc_irq(int irq, void *p)
{
	unsigned int data;

	/* Read gcregIntrAcknowledge register. */
	data = gc_read_reg(GCREG_INTR_ACKNOWLEDGE_Address);

	/* Our interrupt? */
	if (data == 0)
		return IRQ_NONE;

	GC_PRINT(GC_INFO_MSG " data=0x%08X\n",
		__func__, __LINE__, data);

#if GC_DUMP
	/* Dump GPU status. */
	gc_gpu_status((char *) __func__, __LINE__, &data);
#endif

#if 1
	g_gccoredata = data & 0x3FFFFFFF;
	complete(&g_gccoreint);
#else
	/* TODO: we need to wait for an interrupt after enabling
			the mmu, but we don't want to send a signal to
			the user space.  Instead, we want to send a signal
			to the driver.
	*/

	if (data == 0x10000) {
		queue_work(gcwq, &gcwork);
	} else {
		client = (struct clientinfo *)
				((struct gccore *)p)->priv;
		if (client != NULL)
			send_sig(SIGUSR1, client->task, 0);
	}
#endif

	return IRQ_HANDLED;
}

/*******************************************************************************
 * GPU power level control.
 */

struct gctransition {
	int irq;
	int irq_install;

	int reset;

	int clk;
	int clk_on;

	int pulse;
	int pulse_skip;
};

static struct gctransition g_gctransition[4][4] = {
	/* FROM: GCPWR_UNKNOWN */
	{
		/* TO: GCPWR_UNKNOWN */
		{0},

		/* TO: GCPWR_OFF */
		{
			true, false,		/* IRQ routine. */
			true,			/* Hardware reset. */
			true, false,		/* External clock. */
			false, false		/* Internal pulse skipping. */
		},

		/* TO: GCPWR_SUSPEND */
		{
			true, true,		/* IRQ routine. */
			true,			/* Hardware reset. */
			true, true,		/* External clock. */
			true, true		/* Internal pulse skipping. */
		},

		/* TO: GCPWR_ON */
		{
			true, true,		/* IRQ routine. */
			true,			/* Hardware reset. */
			true, true,		/* External clock. */
			false, false		/* Internal pulse skipping. */
		},
	},

	/* FROM: GCPWR_OFF */
	{
		/* TO: GCPWR_UNKNOWN */
		{0},

		/* TO: GCPWR_OFF */
		{
			false, false,		/* IRQ routine. */
			false,			/* Hardware reset. */
			false, false,		/* External clock. */
			false, false		/* Internal pulse skipping. */
		},

		/* TO: GCPWR_SUSPEND */
		{
			true, true,		/* IRQ routine. */
			false,			/* Hardware reset. */
			true, true,		/* External clock. */
			true, true		/* Internal pulse skipping. */
		},

		/* TO: GCPWR_ON */
		{
			true, true,		/* IRQ routine. */
			false,			/* Hardware reset. */
			true, true,		/* External clock. */
			false, false		/* Internal pulse skipping. */
		},
	},

	/* FROM: GCPWR_SUSPEND */
	{
		/* TO: GCPWR_UNKNOWN */
		{0},

		/* TO: GCPWR_OFF */
		{
			true, false,		/* IRQ routine. */
			false,			/* Hardware reset. */
			true, false,		/* External clock. */
			false, false		/* Internal pulse skipping. */
		},

		/* TO: GCPWR_SUSPEND */
		{
			false, false,		/* IRQ routine. */
			false,			/* Hardware reset. */
			false, false,		/* External clock. */
			false, false		/* Internal pulse skipping. */
		},

		/* TO: GCPWR_ON */
		{
			false, false,		/* IRQ routine. */
			false,			/* Hardware reset. */
			false, false,		/* External clock. */
			true, false		/* Internal pulse skipping. */
		},
	},

	/* FROM: GCPWR_ON */
	{
		/* TO: GCPWR_UNKNOWN */
		{0},

		/* TO: GCPWR_OFF */
		{
			true, false,		/* IRQ routine. */
			false,			/* Hardware reset. */
			true, false,		/* External clock. */
			false, false		/* Internal pulse skipping. */
		},

		/* TO: GCPWR_SUSPEND */
		{
			false, false,		/* IRQ routine. */
			false,			/* Hardware reset. */
			false, false,		/* External clock. */
			true, true		/* Internal pulse skipping. */
		},

		/* TO: GCPWR_ON */
		{
			false, false,		/* IRQ routine. */
			false,			/* Hardware reset. */
			false, false,		/* External clock. */
			false, false		/* Internal pulse skipping. */
		},
	},
};

enum gcpower g_gcpower;
static struct clk *g_bb2d_clk;
unsigned long g_bb2d_clk_rate;

enum gcerror gc_set_power(enum gcpower gcpower)
{
	enum gcerror gcerror = GCERR_NONE;
	struct gctransition *gctransition;
	int ret;

	if (gcpower == GCPWR_UNKNOWN) {
		gcerror = GCERR_POWER_MODE;
		goto fail;
	}

	GC_PRINT(GC_INFO_MSG " power state %d --> %d\n",
			__func__, __LINE__, g_gcpower, gcpower);

	gctransition = &g_gctransition[g_gcpower][gcpower];

	/* Enable/disable external clock. */
	if (gctransition->clk) {
		if (gctransition->clk_on) {
			GC_PRINT(GC_INFO_MSG " CLOCK ON\n",
				__func__, __LINE__);

			ret = clk_enable(g_bb2d_clk);
			if (ret < 0) {
				GC_PRINT(GC_ERR_MSG
					" failed to enable bb2d_fck (%d).\n",
					 __func__, __LINE__, ret);
				gcerror = GCERR_POWER_CLOCK_ON;
				goto fail;
			}
		} else {
			GC_PRINT(GC_INFO_MSG " CLOCK OFF\n",
				__func__, __LINE__);

			clk_disable(g_bb2d_clk);
		}
	}

	/* Install/remove IRQ handler. */
	if (gctransition->irq) {
		if (gctransition->irq_install) {
			GC_PRINT(GC_INFO_MSG " IRQ INSTALL\n",
				__func__, __LINE__);

			ret = request_irq(DEVICE_INT, gc_irq, IRQF_SHARED,
						DEV_NAME, &gcdevice);
			if (ret < 0) {
				GC_PRINT(GC_ERR_MSG
					" failed to install IRQ (%d).\n",
					 __func__, __LINE__, ret);
				gcerror = GCERR_POWER_CLOCK_ON;
				goto fail;
			}
		} else {
			GC_PRINT(GC_INFO_MSG " IRQ REMOVE\n",
				__func__, __LINE__);

			free_irq(DEVICE_INT, &gcdevice);
		}
	}

	/* Reset the GPU. */
	if (gctransition->reset) {
		union gcclockcontrol gcclockcontrol;
		union gcidle gcidle;

		GC_PRINT(GC_INFO_MSG " RESET\n",
			__func__, __LINE__);

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
				GC_PRINT(GC_ERR_MSG " FE NOT IDLE\n",
					__func__, __LINE__);

				continue;
			}

			/* Read reset register. */
			gcclockcontrol.raw
				= gc_read_reg(GCREG_HI_CLOCK_CONTROL_Address);

			/* Try resetting again if 2D is not idle. */
			if (!gcclockcontrol.reg.idle2d) {
				GC_PRINT(GC_ERR_MSG " 2D NOT IDLE\n",
					__func__, __LINE__);

				continue;
			}

			/* GPU is idle. */
			break;
		}
	}

	/* Enable/disable pulse skipping. */
	if (gctransition->pulse) {
		union gcclockcontrol gcclockcontrol;
		gcclockcontrol.raw = 0;

		if (gctransition->pulse_skip) {
			GC_PRINT(GC_INFO_MSG " PULSE SKIP ENABLE\n",
				__func__, __LINE__);

			/* Enable loading and set to minimum value. */
			gcclockcontrol.reg.pulsecount = 1;
			gcclockcontrol.reg.pulseset = true;
			gc_write_reg(GCREG_HI_CLOCK_CONTROL_Address,
					gcclockcontrol.raw);
		} else {
			GC_PRINT(GC_INFO_MSG " PULSE SKIP DISABLE\n",
				__func__, __LINE__);

			/* Enable loading and set to maximum value. */
			gcclockcontrol.reg.pulsecount = 64;
			gcclockcontrol.reg.pulseset = true;
			gc_write_reg(GCREG_HI_CLOCK_CONTROL_Address,
					gcclockcontrol.raw);
		}

		/* Disable loading. */
		gcclockcontrol.reg.pulseset = false;
		gc_write_reg(GCREG_HI_CLOCK_CONTROL_Address,
				gcclockcontrol.raw);
	}

	/* Set new power state. */
	g_gcpower = gcpower;

fail:
	return gcerror;
}

/*******************************************************************************
 * Context management.
 */

static int g_clientref;

enum gcerror gc_attach(struct gccontext **gccontext)
{
	enum gcerror gcerror;
	struct gccontext *temp = NULL;
	int mmuinit = 0;
	int cmdbufmap = 0;

	temp = kzalloc(sizeof(struct gccontext), GFP_KERNEL);
	if (temp == NULL) {
		gcerror = GCERR_SETGRP(GCERR_OODM, GCERR_CTX_ALLOC);
		goto fail;
	}

	gcerror = mmu2d_create_context(&temp->mmu);
	if (gcerror != GCERR_NONE)
		goto fail;
	mmuinit = 1;

	gcerror = cmdbuf_map(&temp->mmu);
	if (gcerror != GCERR_NONE)
		goto fail;
	cmdbufmap = 1;

	temp->mmu_dirty = true;
	*gccontext = temp;

	g_clientref += 1;

	return GCERR_NONE;

fail:
	gc_detach(&temp);
	return gcerror;
}
EXPORT_SYMBOL(gc_attach);

enum gcerror gc_detach(struct gccontext **gccontext)
{
	enum gcerror gcerror;
	struct gccontext *temp;

	temp = *gccontext;
	if (temp == NULL) {
		gcerror = GCERR_NONE;
	} else {
		gcerror = gc_unlock(temp);
		if (gcerror != GCERR_NONE)
			goto exit;

		gcerror = mmu2d_destroy_context(&temp->mmu);
		if (gcerror != GCERR_NONE)
			goto exit;

		kfree(temp);
		*gccontext = NULL;

		g_clientref -= 1;
		if (g_clientref == 0) {
			gcerror = gc_set_power(GCPWR_OFF);
			if (gcerror != GCERR_NONE)
				goto exit;
		}

		gcerror = GCERR_NONE;
	}

exit:
	return gcerror;
}
EXPORT_SYMBOL(gc_detach);

enum gcerror gc_lock(struct gccontext *gccontext)
{
	enum gcerror gcerror;
	int contextlocked = 0;
	int datalocked = 0;

	GC_PRINT(GC_ERR_MSG " locking on context 0x%08X\n",
			__func__, __LINE__, (unsigned int) gccontext);

	/* Acquire context info acccess mutex. */
	gcerror = gc_acquire_mutex(&g_datalock, GC_INFINITE);
	if (gcerror != GCERR_NONE) {
		GC_PRINT(GC_ERR_MSG " failed to acquire mutex (0x%08X).\n",
				__func__, __LINE__, gcerror);
		gcerror = GCERR_SETGRP(gcerror, GCERR_CTX_CHANGE);
		goto fail;
	}
	datalocked = 1;

	/* Acquire conntext lock mutex. */
	gcerror = gc_acquire_mutex(&g_contextlock, GC_INFINITE);
	if (gcerror != GCERR_NONE) {
		GC_PRINT(GC_ERR_MSG " failed to acquire mutex (0x%08X).\n",
				__func__, __LINE__, gcerror);
		gcerror = GCERR_SETGRP(gcerror, GCERR_CTX_CHANGE);
		goto fail;
	}
	contextlocked = 1;

	gccontext->mmu_dirty = true;

	GC_PRINT(GC_ERR_MSG " context locked\n",
			__func__, __LINE__);

	mutex_unlock(&g_datalock);
	return GCERR_NONE;

fail:
	if (contextlocked)
		mutex_unlock(&g_contextlock);

	if (datalocked)
		mutex_unlock(&g_datalock);

	return gcerror;
}
EXPORT_SYMBOL(gc_lock);

enum gcerror gc_unlock(struct gccontext *gccontext)
{
	enum gcerror gcerror;
	int datalocked = 0;

	GC_PRINT(GC_ERR_MSG " unlocking from context 0x%08X\n",
			__func__, __LINE__, (unsigned int) gccontext);

	/* Acquire context info acccess mutex. */
	gcerror = gc_acquire_mutex(&g_datalock, GC_INFINITE);
	if (gcerror != GCERR_NONE) {
		GC_PRINT(GC_ERR_MSG " failed to acquire mutex (0x%08X).\n",
				__func__, __LINE__, gcerror);
		gcerror = GCERR_SETGRP(gcerror, GCERR_CTX_CHANGE);
		goto exit;
	}
	datalocked = 1;

	mutex_unlock(&g_contextlock);

exit:
	if (datalocked)
		mutex_unlock(&g_datalock);

	return gcerror;
}
EXPORT_SYMBOL(gc_unlock);

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
	int locked = 0;
	struct gccontextmap *context;
	struct gccommit kgccommit;

	/* Locate the client entry. */
	kgccommit.gcerror = find_context(&context, true);
	if (kgccommit.gcerror != GCERR_NONE)
		goto exit;

	/* Set context. */
	kgccommit.gcerror = gc_lock(context->context);
	if (kgccommit.gcerror != GCERR_NONE)
		goto exit;
	locked = 1;

	/* Set 2D pipe. */
	gccommit->gcerror = cmdbuf_alloc(sizeof(struct gcmopipesel),
					(void **) &gcmopipesel, NULL);
	if (gccommit->gcerror != GCERR_NONE)
		goto exit;

	gcmopipesel->pipesel_ldst = gcmopipesel_pipesel_ldst;
	gcmopipesel->pipesel.reg = gcregpipeselect_2D;

	/* Set the client's master table. */
	gccommit->gcerror = mmu2d_set_master(&context->context->mmu);
	if (gccommit->gcerror != GCERR_NONE)
		goto exit;

	/* Determine command buffer flush size. */
	cmdflushsize = cmdbuf_flush(NULL);

	/* Go through all buffers one at a time. */
	gcbuffer = gccommit->buffer;
	while (gcbuffer != NULL) {
		/* Compute the size of the command buffer. */
		buffersize
			= (unsigned char *) gcbuffer->tail
			- (unsigned char *) gcbuffer->head;

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
				GC_PRINT(GC_ERR_MSG " failed to read data.\n",
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
	if (locked)
		gc_unlock(context->context);
}
EXPORT_SYMBOL(gc_commit);

void gc_map(struct gcmap *gcmap)
{
	struct mmu2dphysmem mem;
	struct mmu2darena *mapped = NULL;
	int locked = 0;
	struct gccontextmap *context;
	struct gcmap kgcmap;

	/* Locate the client entry. */
	kgcmap.gcerror = find_context(&context, true);
	if (kgcmap.gcerror != GCERR_NONE)
		goto exit;

	/* Set context. */
	kgcmap.gcerror = gc_lock(context->context);
	if (kgcmap.gcerror != GCERR_NONE)
		goto exit;
	locked = 1;

	GC_PRINT(GC_INFO_MSG " map client buffer\n",
			__func__, __LINE__);
	GC_PRINT(GC_INFO_MSG "   logical = 0x%08X\n",
			__func__, __LINE__, (unsigned int) gcmap->logical);
	GC_PRINT(GC_INFO_MSG "   size = %d\n",
			__func__, __LINE__, gcmap->size);

	/* Initialize the mapping parameters. See if we were passed a list
	 * of pages first
	 */
	if (gcmap->pagecount > 0 && gcmap->pagearray != NULL) {
		GC_PRINT(KERN_ERR "%s: Got page array %p with %lu pages",
			__func__, gcmap->pagearray, gcmap->pagecount);
		mem.base = 0;
		mem.offset = 0;
		mem.count = gcmap->pagecount;
		mem.pages = gcmap->pagearray;
	} else {
		GC_PRINT(KERN_ERR "%s: gcmap->logical = %p\n",
			__func__, gcmap->logical);
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

	GC_PRINT(GC_INFO_MSG "   mapped address = 0x%08X\n",
			__func__, __LINE__, mapped->address);
	GC_PRINT(GC_INFO_MSG "   handle = 0x%08X\n",
			__func__, __LINE__, (unsigned int) mapped);

exit:
	if (locked)
		gc_unlock(context->context);
}
EXPORT_SYMBOL(gc_map);

void gc_unmap(struct gcmap *gcmap)
{
	int locked = 0;
	struct gccontextmap *context;
	struct gcmap kgcmap;

	/* Locate the client entry. */
	kgcmap.gcerror = find_context(&context, true);
	if (kgcmap.gcerror != GCERR_NONE)
		goto exit;

	/* Set context. */
	kgcmap.gcerror = gc_lock(context->context);
	if (kgcmap.gcerror != GCERR_NONE)
		goto exit;
	locked = 1;

	GC_PRINT(GC_INFO_MSG " unmap client buffer\n",
			__func__, __LINE__);
	GC_PRINT(GC_INFO_MSG "   logical = 0x%08X\n",
			__func__, __LINE__, (unsigned int) gcmap->logical);
	GC_PRINT(GC_INFO_MSG "   size = %d\n",
			__func__, __LINE__, gcmap->size);
	GC_PRINT(GC_INFO_MSG "   handle = 0x%08X\n",
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
	if (locked)
		gc_unlock(context->context);
}
EXPORT_SYMBOL(gc_unmap);

/*******************************************************************************
 * Driver init/shutdown.
 */

static int __init gc_init(void)
{
	/* Initialize context mutexes. */
	mutex_init(&g_contextlock);
	mutex_init(&g_datalock);

	/* Initialize interrupt completion. */
	init_completion(&g_gccoreint);

	/* Set power mode. */
	g_gcpower = GCPWR_UNKNOWN;

	/* Locate the clock entry. */
	g_bb2d_clk = clk_get(NULL, "bb2d_fck");
	if (IS_ERR(g_bb2d_clk)) {
		GC_PRINT(GC_ERR_MSG " cannot find bb2d_fck.\n",
			 __func__, __LINE__);
		goto fail;
	}

	g_bb2d_clk_rate = clk_get_rate(g_bb2d_clk);
	GC_PRINT(GC_INFO_MSG " BB2D clock is %ldMHz\n",
			__func__, __LINE__, (g_bb2d_clk_rate / 1000000));

	/* Map GPU registers. */
	g_reg_base = ioremap_nocache(DEVICE_REG_BASE, DEVICE_REG_SIZE);
	if (g_reg_base == NULL) {
		GC_PRINT(GC_ERR_MSG " failed to map registers.\n",
			 __func__, __LINE__);
		goto fail;
	}

	/* Initialize the command buffer. */
	if (cmdbuf_init() != GCERR_NONE) {
		GC_PRINT(GC_ERR_MSG " failed to initialize command buffer.\n",
			 __func__, __LINE__);
		goto fail;
	}

#if 0
	gcwq = create_workqueue("gcwq");
	if (!gcwq)
		goto free_reg_mapping;
	destroy_workqueue(gcwq);
#endif

	mutex_init(&g_maplock);
	/* Success. */
	return 0;

fail:
	if (g_reg_base != NULL) {
		iounmap(g_reg_base);
		g_reg_base = NULL;
	}

	return -EINVAL;
}

static void __exit gc_exit(void)
{

	delete_context_map();
	mutex_destroy(&g_maplock);

	if (g_reg_base != NULL) {
		iounmap(g_reg_base);
		g_reg_base = NULL;
	}
}

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("www.vivantecorp.com");
MODULE_AUTHOR("www.ti.com");
module_init(gc_init);
module_exit(gc_exit);
