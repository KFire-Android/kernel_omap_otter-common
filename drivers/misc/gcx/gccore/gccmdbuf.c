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
#include "gcmain.h"

#define GCZONE_NONE		0
#define GCZONE_ALL		(~0U)
#define GCZONE_INIT		(1 << 0)
#define GCZONE_MAPPING		(1 << 1)
#define GCZONE_COMMIT		(1 << 2)
#define GCZONE_BUFFER		(1 << 3)
#define GCZONE_ALLOC		(1 << 4)
#define GCZONE_FLUSH		(1 << 5)

GCDBG_FILTERDEF(cmdbuf, GCZONE_NONE,
		"init",
		"mapping",
		"commit",
		"buffer",
		"alloc",
		"flush")

#define GC_CMD_BUF_PAGES	32
#define GC_CMD_BUF_SIZE		(PAGE_SIZE * GC_CMD_BUF_PAGES)

struct cmdbuf {
	struct gcpage page;

	bool mapped;
	bool forcephysical;
	u32 physicalbase;
	u32 mappedbase;

	u32 available;
	u32 data_size;
};

static struct cmdbuf cmdbuf;

enum gcerror cmdbuf_init(void)
{
	enum gcerror gcerror;

	GCENTER(GCZONE_INIT);

	gcerror = gc_alloc_noncached(&cmdbuf.page, GC_CMD_BUF_SIZE);
	if (gcerror != GCERR_NONE)
		return GCERR_SETGRP(gcerror, GCERR_CMD_ALLOC);

	cmdbuf.mapped = false;
	cmdbuf.forcephysical = false;
	cmdbuf.physicalbase = cmdbuf.page.physical;
	cmdbuf.available = cmdbuf.page.size;
	cmdbuf.data_size = 0;

	GCDBG(GCZONE_INIT, "initialized command buffer.\n");
	GCDBG(GCZONE_INIT, "  physical = 0x%08X\n", cmdbuf.page.physical);
	GCDBG(GCZONE_INIT, "  logical = 0x%08X\n", (u32) cmdbuf.page.logical);
	GCDBG(GCZONE_INIT, "  size = %d\n", cmdbuf.page.size);

	GCEXIT(GCZONE_INIT);
	return GCERR_NONE;
}

void cmdbuf_physical(bool forcephysical)
{
	GCENTERARG(GCZONE_ALLOC, "forcephysical = %d\n", forcephysical);
	cmdbuf.forcephysical = forcephysical;
	GCEXIT(GCZONE_ALLOC);
}

enum gcerror cmdbuf_map(struct gccorecontext *gccorecontext,
			struct gcmmucontext *ctxt)
{
	enum gcerror gcerror;
	struct gcmmuphysmem mem;
	struct gcmmuarena *mapped;
	pte_t physpages[GC_CMD_BUF_PAGES];
	unsigned char *physical;
	int i;

	GCENTER(GCZONE_MAPPING);

	if (cmdbuf.data_size != 0)
		GCERR("command buffer has data!\n");

	physical = (unsigned char *) cmdbuf.page.physical;
	for (i = 0; i < GC_CMD_BUF_PAGES; i += 1) {
		physpages[i] = (pte_t)physical;
		physical += PAGE_SIZE;
	}

	mem.base = (u32) cmdbuf.page.logical;
	mem.offset = 0;
	mem.count = GC_CMD_BUF_PAGES;
	mem.pages = physpages;
	mem.pagesize = PAGE_SIZE;

	gcerror = gcmmu_map(gccorecontext, ctxt, &mem, &mapped);
	if (gcerror != 0)
		return gcerror;

	if (cmdbuf.mapped) {
		if (mapped->address != cmdbuf.mappedbase)
			GCERR("inconsitent command buffer mapping!\n");
	} else {
		cmdbuf.mapped = true;
	}

	cmdbuf.mappedbase = mapped->address;

	GCDBG(GCZONE_MAPPING, "  physical address = 0x%08X\n",
		cmdbuf.physicalbase);
	GCDBG(GCZONE_MAPPING, "  mapped address = 0x%08X\n",
		cmdbuf.mappedbase);
	GCDBG(GCZONE_MAPPING, "  logical = 0x%08X\n",
		(u32) cmdbuf.page.logical);
	GCDBG(GCZONE_MAPPING, "  size = %d\n",
		cmdbuf.page.size);

	GCEXIT(GCZONE_MAPPING);
	return GCERR_NONE;
}

enum gcerror cmdbuf_alloc(u32 size, void **logical, u32 *physical)
{
	enum gcerror gcerror = GCERR_NONE;
	unsigned int base;

	GCENTER(GCZONE_ALLOC);

	if (size > cmdbuf.available) {
		gcerror = GCERR_CMD_ALLOC;
		goto fail;
	}

	/* Determine the command buffer base address. */
	base = (cmdbuf.mapped && !cmdbuf.forcephysical)
		? cmdbuf.mappedbase : cmdbuf.physicalbase;

	if (logical != NULL)
		*logical = (unsigned int *)
			((unsigned char *) cmdbuf.page.logical
					+ cmdbuf.data_size);

	if (physical != NULL)
		*physical = base + cmdbuf.data_size;

	GCDBG(GCZONE_ALLOC, "logical = 0x%08X\n",
		(unsigned int) ((unsigned char *) cmdbuf.page.logical
						+ cmdbuf.data_size));
	GCDBG(GCZONE_ALLOC, "address = 0x%08X\n", base + cmdbuf.data_size);
	GCDBG(GCZONE_ALLOC, "size = %d\n", size);

	/* Determine the data size. */
	size = (size + 3) & ~3;

	/* Update current sizes. */
	cmdbuf.available -= size;
	cmdbuf.data_size += size;

fail:
	GCEXIT(GCZONE_ALLOC);
	return gcerror;
}

int cmdbuf_flush(void *logical)
{
	static const int flushSize
		= sizeof(struct gcmosignal) + sizeof(struct gccmdend);

	GCENTER(GCZONE_FLUSH);

	if (logical != NULL) {
		struct gcmosignal *gcmosignal;
		struct gccmdend *gccmdend;
		u32 base, count;
		u32 ack;

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

		/* Determine the command buffer base address. */
		base = (cmdbuf.mapped && !cmdbuf.forcephysical)
			? cmdbuf.mappedbase : cmdbuf.physicalbase;

		/* Compute the data count. */
		count = (cmdbuf.data_size + 7) >> 3;

		GCDBG(GCZONE_COMMIT,
			"starting DMA at 0x%08X with count of %d\n",
			base, count);

		/* Dump command buffer. */
		GCDUMPBUFFER(GCZONE_BUFFER, cmdbuf.page.logical, base,
				cmdbuf.data_size);

		/* Enable all events. */
		gc_write_reg(GCREG_INTR_ENBL_Address, ~0U);

		/* Write address register. */
		gc_write_reg(GCREG_CMD_BUFFER_ADDR_Address, base);

		wmb();

		/* Write control register. */
		gc_write_reg(GCREG_CMD_BUFFER_CTRL_Address,
			SETFIELDVAL(0, GCREG_CMD_BUFFER_CTRL, ENABLE, ENABLE) |
			SETFIELD(0, GCREG_CMD_BUFFER_CTRL, PREFETCH, count)
			);

		/* Wait for the interrupt. */
		gc_wait_interrupt();
		ack = gc_get_interrupt_data();

		GCDBG(GCZONE_COMMIT, "ack = 0x%08X\n", ack);

		if ((ack & 0xC0000000) != 0)
			GCGPUSTATUS(0, &ack);

		/* Reset the buffer. */
		cmdbuf.available = cmdbuf.page.size;
		cmdbuf.data_size = 0;
	}

	GCEXITARG(GCZONE_FLUSH, "flushSize = %d\n", flushSize);
	return flushSize;
}
