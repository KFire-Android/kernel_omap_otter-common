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
#include <linux/sched.h>

#include "gcreg.h"
#include "gccmdbuf.h"
#include "gccore.h"

#if ENABLE_POLLING
extern volatile u32 int_data;
#endif


struct cmdbuf {
	struct mmu2dpage page;

	int mapped;
	u32 mapped_physical;

	u32 *logical;
	u32 physical;

	u32 available;
	u32 data_size;
};

static struct cmdbuf cmdbuf;

int cmdbuf_init(void)
{
	int ret;

	ret = mmu2d_alloc_pages(&cmdbuf.page, PAGE_SIZE);
	if (ret != 0)
		return ret;

	memset(cmdbuf.page.logical, 0x0, cmdbuf.page.size);

	cmdbuf.mapped = false;
	cmdbuf.logical = cmdbuf.page.logical;
	cmdbuf.physical = cmdbuf.page.physical;

	cmdbuf.available = PAGE_SIZE;
	cmdbuf.data_size = 0;

	CMDBUFPRINT(KERN_ERR "%s(%d): Initialized command buffer.\n",
		__func__, __LINE__);

	CMDBUFPRINT(KERN_ERR "%s(%d):   physical = 0x%08X\n",
		__func__, __LINE__, cmdbuf.page.physical);

	CMDBUFPRINT(KERN_ERR "%s(%d):   logical = 0x%08X\n",
		__func__, __LINE__, (u32) cmdbuf.page.logical);

	CMDBUFPRINT(KERN_ERR "%s(%d):   size = %d\n",
		__func__, __LINE__, cmdbuf.page.size);

	return 0;
}

int cmdbuf_map(struct mmu2dcontext *ctxt)
{
	int ret;
	struct mmu2dphysmem physmem;

	physmem.pagecount = 1;
	physmem.pages = (pte_t *) &cmdbuf.page.physical;
	physmem.pagesize = PAGE_SIZE;
	physmem.pageoffset = 0;

	ret = mmu2d_map_phys(ctxt, &physmem);
	if (ret != 0)
		return -ENOMEM;

	if (cmdbuf.mapped) {
		if (physmem.logical != cmdbuf.mapped_physical) {
			CMDBUFPRINT(KERN_ERR
		"%s(%d): WARNING: inconsitent command buffer mapping!\n",
						__func__, __LINE__);
		}
	} else {
		cmdbuf.mapped = true;
	}

	cmdbuf.mapped_physical = physmem.logical;
	cmdbuf.physical        = physmem.logical + cmdbuf.data_size;

	CMDBUFPRINT(KERN_ERR "%s(%d): Mapped command buffer.\n",
		__func__, __LINE__);

	CMDBUFPRINT(KERN_ERR "%s(%d):   physical = 0x%08X (mapped)\n",
		__func__, __LINE__, cmdbuf.mapped_physical);

	CMDBUFPRINT(KERN_ERR "%s(%d):   logical = 0x%08X\n",
		__func__, __LINE__, (u32) cmdbuf.page.logical);

	CMDBUFPRINT(KERN_ERR "%s(%d):   size = %d\n",
		__func__, __LINE__, cmdbuf.page.size);

	return 0;
}

int cmdbuf_alloc(u32 size, u32 **logical, u32 *physical)
{
	if ((cmdbuf.logical == NULL) || (size > cmdbuf.available))
		return -ENOMEM;

	size = (size + 3) & ~3;

	*logical  = cmdbuf.logical;
	*physical = cmdbuf.physical;

	cmdbuf.logical   += (size >> 2);
	cmdbuf.physical  += size;
	cmdbuf.available -= size;
	cmdbuf.data_size += size;

	return 0;
}

int cmdbuf_flush(void)
{
	int ret;
	u32 *buffer;
	u32 base, physical;
	u32 count;

#if ENABLE_POLLING
	u32 retry;
#endif

	ret = cmdbuf_alloc(4 * sizeof(u32), &buffer, &physical);
	if (ret != 0)
		goto fail;

	/* Append EVENT(Event, destination). */
	buffer[0]
		= SETFIELDVAL(0, AQ_COMMAND_LOAD_STATE_COMMAND, OPCODE,
								 LOAD_STATE)
		| SETFIELD(0, AQ_COMMAND_LOAD_STATE_COMMAND, ADDRESS,
								AQEventRegAddrs)
		| SETFIELD(0, AQ_COMMAND_LOAD_STATE_COMMAND, COUNT, 1);

	buffer[1]
		= SETFIELDVAL(0, AQ_EVENT, PE_SRC, ENABLE)
		| SETFIELD(0, AQ_EVENT, EVENT_ID, 16);

	/* Stop FE. */
	buffer[2]
		= SETFIELDVAL(0, AQ_COMMAND_END_COMMAND, OPCODE, END);

#if ENABLE_CMD_DEBUG
	/* Dump command buffer. */
	cmdbuf_dump();
#endif

	/* Determine the command buffer base address. */
	base = cmdbuf.mapped ? cmdbuf.mapped_physical : cmdbuf.page.physical;

	/* Compute the data count. */
	count = (cmdbuf.data_size + 7) >> 3;

#if ENABLE_POLLING
	int_data = 0;
#endif

	CMDBUFPRINT("starting DMA at 0x%08X with count of %d\n", base, count);

#if ENABLE_CMD_DEBUG || ENABLE_GPU_COUNTERS
	/* Reset hardware counters. */
	hw_write_reg(GC_RESET_MEM_COUNTERS_Address, 1);
#endif

	/* Enable all events. */
	hw_write_reg(AQ_INTR_ENBL_Address, ~0U);

	/* Write address register. */
	hw_write_reg(AQ_CMD_BUFFER_ADDR_Address, base);

	/* Write control register. */
	hw_write_reg(AQ_CMD_BUFFER_CTRL_Address,
		SETFIELDVAL(0, AQ_CMD_BUFFER_CTRL, ENABLE, ENABLE) |
		SETFIELD(0, AQ_CMD_BUFFER_CTRL, PREFETCH, count)
		);

	/* Wait for the interrupt. */
#if ENABLE_POLLING
	retry = 0;
	while (1) {
		if (int_data != 0)
			break;

		msleep(500);
		retry += 1;

		if ((retry % 5) == 0)
			gpu_status((char *) __func__, __LINE__, 0);
	}
#else
	wait_event_interruptible(gc_event, done == true);
#endif

#if ENABLE_CMD_DEBUG
	gpu_status((char *) __func__, __LINE__, 0);
#endif

	/* Reset the buffer. */
	cmdbuf.logical  = cmdbuf.page.logical;
	cmdbuf.physical = base;

	cmdbuf.available = cmdbuf.page.size;
	cmdbuf.data_size = 0;

fail:
	return ret;
}

u32 hw_read_reg(u32 address)
{
	return readl(reg_base + address);
}

void hw_write_reg(u32 address, u32 data)
{
	writel(data, reg_base + address);
}

void gpu_id(void)
{
	u32 chipModel;
	u32 chipRevision;
	u32 chipDate;
	u32 chipTime;
	u32 chipFeatures;
	u32 chipMinorFeatures;

	chipModel = hw_read_reg(GC_CHIP_ID_Address);
	chipRevision = hw_read_reg(GC_CHIP_REV_Address);
	chipDate = hw_read_reg(GC_CHIP_DATE_Address);
	chipTime = hw_read_reg(GC_CHIP_TIME_Address);
	chipFeatures = hw_read_reg(GC_FEATURES_Address);
	chipMinorFeatures = hw_read_reg(GC_MINOR_FEATURES0_Address);

	printk(KERN_DEBUG "CHIP IDENTITY\n");
	printk(KERN_DEBUG "  model=%X\n", chipModel);
	printk(KERN_DEBUG "  revision=%X\n", chipRevision);
	printk(KERN_DEBUG "  date=%X\n", chipDate);
	printk(KERN_DEBUG "  time=%X\n", chipTime);
	printk(KERN_DEBUG "  chipFeatures=0x%08X\n", chipFeatures);
}

void gpu_status(char *function, int line, u32 acknowledge)
{
	int i;
	u32 idle;
	u32 dma_state, dma_addr;
	u32 dma_low_data, dma_high_data;
	u32 status;
	u32 mmu;
	u32 address;
	u32 total_reads;
	u32 total_writes;
	u32 total_read_bursts;
	u32 total_write_bursts;
	u32 total_read_reqs;
	u32 total_write_reqs;

	CMDBUFPRINT(KERN_ERR "%s(%d): Current GPU status.\n",
		function, line);

	idle = hw_read_reg(AQ_HI_IDLE_Address);
	CMDBUFPRINT(KERN_ERR "%s(%d):   idle = 0x%08X\n",
		function, line, idle);

	dma_state = hw_read_reg(AQFE_DEBUG_STATE_Address);
	CMDBUFPRINT(KERN_ERR "%s(%d):   DMA state = 0x%08X\n",
		function, line, dma_state);

	dma_addr = hw_read_reg(AQFE_DEBUG_CUR_CMD_ADR_Address);
	CMDBUFPRINT(KERN_ERR "%s(%d):   DMA address = 0x%08X\n",
		function, line, dma_addr);

	dma_low_data = hw_read_reg(AQFE_DEBUG_CMD_LOW_REG_Address);
	CMDBUFPRINT(KERN_ERR "%s(%d):   DMA low data = 0x%08X\n",
		function, line, dma_low_data);

	dma_high_data = hw_read_reg(AQFE_DEBUG_CMD_LOW_REG_Address);
	CMDBUFPRINT(KERN_ERR "%s(%d):   DMA high data = 0x%08X\n",
		function, line, dma_high_data);

	total_reads = hw_read_reg(GC_TOTAL_READS_Address);
	CMDBUFPRINT(KERN_ERR "%s(%d):   Total memory reads = %d\n",
		function, line, total_reads);

	total_writes = hw_read_reg(GC_TOTAL_WRITES_Address);
	CMDBUFPRINT(KERN_ERR "%s(%d):   Total memory writes = %d\n",
		function, line, total_writes);

	total_read_bursts = hw_read_reg(GC_TOTAL_READ_BURSTS_Address);
	CMDBUFPRINT(KERN_ERR "%s(%d):   Total memory read 64-bit bursts = %d\n",
		function, line, total_read_bursts);

	total_write_bursts = hw_read_reg(GC_TOTAL_WRITE_BURSTS_Address);
	CMDBUFPRINT(KERN_ERR
		"%s(%d):   Total memory write 64-bit bursts = %d\n",
		function, line, total_write_bursts);

	total_read_reqs = hw_read_reg(GC_TOTAL_READ_REQS_Address);
	CMDBUFPRINT(KERN_ERR "%s(%d):   Total memory read requests = %d\n",
		function, line, total_read_reqs);

	total_write_reqs = hw_read_reg(GC_TOTAL_WRITE_REQS_Address);
	CMDBUFPRINT(KERN_ERR "%s(%d):   Total memory write requests = %d\n",
		function, line, total_write_reqs);

	CMDBUFPRINT(KERN_ERR "%s(%d):   interrupt acknowledge = 0x%08X\n",
		function, line, acknowledge);

	if (acknowledge & 0x80000000) {
		CMDBUFPRINT(KERN_ERR "%s(%d):   *** BUS ERROR ***\n",
			function, line);
	}

	if (acknowledge & 0x40000000) {
		CMDBUFPRINT(KERN_ERR "%s(%d):   *** MMU ERROR ***\n",
			function, line);

		status = hw_read_reg(GCREG_MMU_STATUS_Address);
		CMDBUFPRINT(KERN_ERR "%s(%d):   MMU status = 0x%08X\n",
			function, line, status);

		for (i = 0; i < 4; i += 1) {
			mmu = status & 0xF;
			status >>= 4;

			if (mmu) {
				switch (mmu) {
				case 1:
					CMDBUFPRINT(KERN_ERR
					 "%s(%d):   MMU%d: slave not present\n",
							function, line, i);
					break;

				case 2:
					CMDBUFPRINT(KERN_ERR
					 "%s(%d):   MMU%d: page not present\n",
						function, line, i);
					break;

				case 3:
					CMDBUFPRINT(KERN_ERR
					 "%s(%d):   MMU%d: write violation\n",
						function, line, i);
					break;

				default:
					CMDBUFPRINT(KERN_ERR
					 "%s(%d):   MMU%d: unknown state\n",
						function, line, i);
				}

				address = hw_read_reg
					(GCREG_MMU_EXCEPTION_Address + i);
				CMDBUFPRINT(KERN_ERR
				"%s(%d):   MMU%d: exception address = 0x%08X\n",
					function, line, i, address);
			}
		}
	}
}

void cmdbuf_dump(void)
{
	u32 i, count, base;

	base = cmdbuf.mapped ? cmdbuf.mapped_physical : cmdbuf.page.physical;

	CMDBUFPRINT(KERN_ERR "%s(%d): Current command buffer.\n",
		__func__, __LINE__);

	CMDBUFPRINT(KERN_ERR "%s(%d):   physical = 0x%08X%s\n",
		__func__, __LINE__, base, cmdbuf.mapped ? " (mapped)" : "");

	CMDBUFPRINT(KERN_ERR "%s(%d):   logical = 0x%08X\n",
		__func__, __LINE__, (u32) cmdbuf.page.logical);

	CMDBUFPRINT(KERN_ERR "%s(%d):   current data size = %d\n",
		__func__, __LINE__, cmdbuf.data_size);

	CMDBUFPRINT(KERN_ERR "%s(%d)\n",
		__func__, __LINE__);

	count = cmdbuf.data_size / 4;

	for (i = 0; i < count; i += 1) {
		CMDBUFPRINT(KERN_ERR "%s(%d):   [0x%08X]: 0x%08X\n",
			__func__, __LINE__,
			base + i * 4,
			cmdbuf.page.logical[i]
			);
	}
}
