/*
 * gccmdbuf.c
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

#include <linux/init.h>
#include <linux/module.h>
#include <linux/mm.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/wait.h>

#include <linux/gcx.h>
#include "gcmain.h"
#include "gccmdbuf.h"

#define GC_ENABLE_GPU_COUNTERS	0

#ifndef GC_DUMP
#	define GC_DUMP 0
#endif

#if GC_DUMP
#	define GC_PRINT printk
#else
#	define GC_PRINT(...)
#endif

#define GC_CMD_BUF_PAGES	20
#define GC_CMD_BUF_SIZE		(PAGE_SIZE * GC_CMD_BUF_PAGES)

struct cmdbuf {
	struct gcpage page;

	int mapped;
	u32 mapped_physical;

	u32 *logical;
	u32 physical;

	u32 available;
	u32 data_size;
};

static struct cmdbuf cmdbuf;

enum gcerror cmdbuf_init(void)
{
	enum gcerror gcerror;

	gcerror = gc_alloc_pages(&cmdbuf.page, GC_CMD_BUF_SIZE);
	if (gcerror != GCERR_NONE)
		return GCERR_SETGRP(gcerror, GCERR_CMD_ALLOC);

	memset(cmdbuf.page.logical, 0x0, cmdbuf.page.size);

	cmdbuf.mapped = false;
	cmdbuf.logical = cmdbuf.page.logical;
	cmdbuf.physical = cmdbuf.page.physical;

	cmdbuf.available = cmdbuf.page.size;
	cmdbuf.data_size = 0;

	GC_PRINT(KERN_INFO "%s(%d): Initialized command buffer.\n",
		__func__, __LINE__);

	GC_PRINT(KERN_INFO "%s(%d):   physical = 0x%08X\n",
		__func__, __LINE__, cmdbuf.page.physical);

	GC_PRINT(KERN_INFO "%s(%d):   logical = 0x%08X\n",
		__func__, __LINE__, (u32) cmdbuf.page.logical);

	GC_PRINT(KERN_INFO "%s(%d):   size = %d\n",
		__func__, __LINE__, cmdbuf.page.size);

	return GCERR_NONE;
}

enum gcerror cmdbuf_map(struct mmu2dcontext *ctxt)
{
	enum gcerror gcerror;
	struct mmu2dphysmem mem;
	struct mmu2darena *mapped;
	pte_t physpages[GC_CMD_BUF_PAGES];
	unsigned char *logical;
	int i;

	logical = (unsigned char *) cmdbuf.page.logical;
	for (i = 0; i < GC_CMD_BUF_PAGES; i += 1) {
		physpages[i] = page_to_phys(virt_to_page(logical));
		logical += PAGE_SIZE;
	}

	mem.base = (u32) cmdbuf.page.logical;
	mem.offset = 0;
	mem.count = GC_CMD_BUF_PAGES;
	mem.pages = physpages;
	mem.pagesize = PAGE_SIZE;

	gcerror = mmu2d_map(ctxt, &mem, &mapped);
	if (gcerror != 0)
		return gcerror;

	if (cmdbuf.mapped) {
		if (mapped->address != cmdbuf.mapped_physical) {
			GC_PRINT(KERN_WARNING
				"%s(%d): inconsitent command buffer mapping!\n",
				__func__, __LINE__);
		}
	} else {
		cmdbuf.mapped = true;
	}

	cmdbuf.mapped_physical = mapped->address;
	cmdbuf.physical        = mapped->address + cmdbuf.data_size;

	GC_PRINT(KERN_INFO "%s(%d): Mapped command buffer.\n",
		__func__, __LINE__);

	GC_PRINT(KERN_INFO "%s(%d):   physical = 0x%08X (mapped)\n",
		__func__, __LINE__, cmdbuf.mapped_physical);

	GC_PRINT(KERN_INFO "%s(%d):   logical = 0x%08X\n",
		__func__, __LINE__, (u32) cmdbuf.page.logical);

	GC_PRINT(KERN_INFO "%s(%d):   size = %d\n",
		__func__, __LINE__, cmdbuf.page.size);

	return GCERR_NONE;
}

enum gcerror cmdbuf_alloc(u32 size, void **logical, u32 *physical)
{
	if ((cmdbuf.logical == NULL) || (size > cmdbuf.available))
		return GCERR_CMD_ALLOC;

	size = (size + 3) & ~3;

	if (logical != NULL)
		*logical = cmdbuf.logical;

	if (physical != NULL)
		*physical = cmdbuf.physical;

	cmdbuf.logical   += (size >> 2);
	cmdbuf.physical  += size;
	cmdbuf.available -= size;
	cmdbuf.data_size += size;

	return GCERR_NONE;
}

int cmdbuf_flush(void *logical)
{
	static const int flushSize
		= sizeof(struct gcmosignal) + sizeof(struct gccmdend);

	if (logical != NULL) {
		struct gcmosignal *gcmosignal;
		struct gccmdend *gccmdend;
		u32 base, count;

		/* Configure the signal. */
		gcmosignal = (struct gcmosignal *) logical;
		gcmosignal->signal_ldst = gcmosignal_signal_ldst;
		gcmosignal->signal.raw = 0;
		gcmosignal->signal.reg.id = 16;
		gcmosignal->signal.reg.pe = GCREG_EVENT_PE_SRC_ENABLE;
		gcmosignal->signal.reg.fe = GCREG_EVENT_FE_SRC_DISABLE;

		/* Configure the end command. */
		gccmdend = (struct gccmdend *) (gcmosignal + 1);
		gccmdend->cmd.fld = gcfldend;

#if GC_DUMP
		/* Dump command buffer. */
		cmdbuf_dump();
#endif

		/* Determine the command buffer base address. */
		base = cmdbuf.mapped
			? cmdbuf.mapped_physical : cmdbuf.page.physical;

		/* Compute the data count. */
		count = (cmdbuf.data_size + 7) >> 3;

		GC_PRINT("starting DMA at 0x%08X with count of %d\n",
			base, count);

		gc_flush_pages(&cmdbuf.page);

		/* Enable power to the chip. */
		gc_set_power(GCPWR_ON);

#if GC_DUMP || GC_ENABLE_GPU_COUNTERS
		/* Reset hardware counters. */
		gc_write_reg(GC_RESET_MEM_COUNTERS_Address, 1);
#endif

		/* Enable all events. */
		gc_write_reg(GCREG_INTR_ENBL_Address, ~0U);

		/* Write address register. */
		gc_write_reg(GCREG_CMD_BUFFER_ADDR_Address, base);

		/* Write control register. */
		gc_write_reg(GCREG_CMD_BUFFER_CTRL_Address,
			SETFIELDVAL(0, GCREG_CMD_BUFFER_CTRL, ENABLE, ENABLE) |
			SETFIELD(0, GCREG_CMD_BUFFER_CTRL, PREFETCH, count)
			);

		/* Wait for the interrupt. */
#if 1
		gc_wait_interrupt();

		GC_PRINT(KERN_INFO "%s(%d): data = 0x%08X\n",
			__func__, __LINE__, gc_get_interrupt_data());

		/* Go to suspend. */
		gc_set_power(GCPWR_SUSPEND);
#else
		wait_event_interruptible(gc_event, done == true);
#endif

		/* Reset the buffer. */
		cmdbuf.logical  = cmdbuf.page.logical;
		cmdbuf.physical = base;

		cmdbuf.available = cmdbuf.page.size;
		cmdbuf.data_size = 0;
	}

	return flushSize;
}

void cmdbuf_dump(void)
{
	unsigned int i, count, base;

	base = cmdbuf.mapped ? cmdbuf.mapped_physical : cmdbuf.page.physical;

	GC_PRINT(KERN_INFO "%s(%d): Current command buffer.\n",
		__func__, __LINE__);

	GC_PRINT(KERN_INFO "%s(%d):   physical = 0x%08X%s\n",
		__func__, __LINE__, base, cmdbuf.mapped ? " (mapped)" : "");

	GC_PRINT(KERN_INFO "%s(%d):   logical = 0x%08X\n",
		__func__, __LINE__, (unsigned int) cmdbuf.page.logical);

	GC_PRINT(KERN_INFO "%s(%d):   current data size = %d\n",
		__func__, __LINE__, cmdbuf.data_size);

	GC_PRINT(KERN_INFO "%s(%d)\n",
		__func__, __LINE__);

	count = cmdbuf.data_size / 4;

	for (i = 0; i < count; i += 1) {
		GC_PRINT(KERN_INFO "%s(%d):   [0x%08X]: 0x%08X\n",
			__func__, __LINE__,
			base + i * 4,
			cmdbuf.page.logical[i]
			);
	}
}
