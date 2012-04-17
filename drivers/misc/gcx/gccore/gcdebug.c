/*
 * gcdebug.c
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

#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/module.h>
#include <linux/gcx.h>
#include <linux/gccore.h>
#include <linux/gcdebug.h>
#include "gcmmu.h"


#define MMU_UNITS  4
#define MMU_ERROR(irq_ack) ((irq_ack & 0x40000000) != 0)

enum gc_debug_when {
	GC_DEBUG_USER_REQUEST,
	GC_DEBUG_DRIVER_POWEROFF,
	GC_DEBUG_DRIVER_IRQ
};


struct gc_gpu_id {
	bool	     valid;
	unsigned int chipModel;
	unsigned int chipRevision;
	unsigned int chipDate;
	unsigned int chipTime;
	unsigned int chipFeatures;
	unsigned int chipMinorFeatures;
};

static struct gc_gpu_id g_gcGpuId;



void gc_debug_cache_gpu_id(void)
{
	if (g_gcGpuId.valid) {
		/* only cached once */
		return;
	}

	g_gcGpuId.chipModel = gc_read_reg(GC_CHIP_ID_Address);
	g_gcGpuId.chipRevision = gc_read_reg(GC_CHIP_REV_Address);
	g_gcGpuId.chipDate = gc_read_reg(GC_CHIP_DATE_Address);
	g_gcGpuId.chipTime = gc_read_reg(GC_CHIP_TIME_Address);
	g_gcGpuId.chipFeatures = gc_read_reg(GC_FEATURES_Address);
	g_gcGpuId.chipMinorFeatures = gc_read_reg(GC_MINOR_FEATURES0_Address);
	g_gcGpuId.valid = 1;
}

static int gc_debug_show_gpu_id(struct seq_file *s, void *unused)
{
	if (!g_gcGpuId.valid) {
		seq_printf(s, "GC gpu id cache not valid.  "
			   "GC must be powered on once.\n");
		return 0;
	}

	seq_printf(s, "model=%X\n", g_gcGpuId.chipModel);
	seq_printf(s, "revision=%X\n", g_gcGpuId.chipRevision);
	seq_printf(s, "date=%X\n", g_gcGpuId.chipDate);
	seq_printf(s, "time=%X\n", g_gcGpuId.chipTime);
	seq_printf(s, "chipFeatures=0x%08X\n", g_gcGpuId.chipFeatures);

	return 0;
}

static int gc_debug_open_gpu_id(struct inode *inode, struct file *file)
{
	return single_open(file, gc_debug_show_gpu_id, inode->i_private);
}

static const struct file_operations gc_debug_fops_gpu_id = {
	.open = gc_debug_open_gpu_id,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};


struct gc_gpu_status {
	bool		   valid;
	const char        *name;
	enum gc_debug_when when;
	unsigned int	   idle;
	unsigned int	   dma_state;
	unsigned int	   dma_addr;
	unsigned int	   dma_low_data;
	unsigned int	   dma_high_data;
	unsigned int	   total_reads;
	unsigned int	   total_writes;
	unsigned int	   total_read_bursts;
	unsigned int	   total_write_bursts;
	unsigned int	   total_read_reqs;
	unsigned int	   total_write_reqs;
	unsigned int	   irq_acknowledge;
	unsigned int	   mmu_status;
	unsigned int	   exception_address[MMU_UNITS];
};

struct gc_gpu_status g_gcGpuStatus = {
	.name = "GPU status"
};
struct gc_gpu_status g_gcGpuStatusLastError = {
	.name = "GPU last error status"
};

/* By default we don't cache the status on every irq */
static int g_gcCacheStatusEveryIrq;


void gc_debug_cache_gpu_status_internal(enum gc_debug_when when,
					unsigned int acknowledge)
{
	bool haveError = ((when == GC_DEBUG_DRIVER_IRQ) &&
			  (acknowledge & 0xC0000000) != 0);
	int i;

	if ((when == GC_DEBUG_DRIVER_IRQ) && !haveError &&
	    !g_gcCacheStatusEveryIrq) {
		/* called from irq, no error, not caching every irq */
		return;
	}

	g_gcGpuStatus.when = when;
	g_gcGpuStatus.idle =
		gc_read_reg(GCREG_HI_IDLE_Address);
	g_gcGpuStatus.dma_state =
		gc_read_reg(GCREG_FE_DEBUG_STATE_Address);
	g_gcGpuStatus.dma_addr =
		gc_read_reg(GCREG_FE_DEBUG_CUR_CMD_ADR_Address);
	g_gcGpuStatus.dma_low_data =
		gc_read_reg(GCREG_FE_DEBUG_CMD_LOW_REG_Address);
	g_gcGpuStatus.dma_high_data =
		gc_read_reg(GCREG_FE_DEBUG_CMD_HI_REG_Address);
	g_gcGpuStatus.total_reads =
		gc_read_reg(GC_TOTAL_READS_Address);
	g_gcGpuStatus.total_writes =
		gc_read_reg(GC_TOTAL_WRITES_Address);
	g_gcGpuStatus.total_read_bursts =
		gc_read_reg(GC_TOTAL_READ_BURSTS_Address);
	g_gcGpuStatus.total_write_bursts =
		gc_read_reg(GC_TOTAL_WRITE_BURSTS_Address);
	g_gcGpuStatus.total_read_reqs =
		gc_read_reg(GC_TOTAL_READ_REQS_Address);
	g_gcGpuStatus.total_write_reqs =
		gc_read_reg(GC_TOTAL_WRITE_REQS_Address);
	g_gcGpuStatus.irq_acknowledge = acknowledge;

	/* Is it valid/useful to read the mmu registers for
	 * other error conditions? */
	if (haveError && MMU_ERROR(acknowledge)) {
		g_gcGpuStatus.mmu_status =
			gc_read_reg(GCREG_MMU_STATUS_Address);

		for (i = 0; i < MMU_UNITS; i++)
			g_gcGpuStatus.exception_address[i] =
				gc_read_reg(GCREG_MMU_EXCEPTION_Address + i);
	} else {
		g_gcGpuStatus.mmu_status = 0;

		for (i = 0; i < MMU_UNITS; i++)
			g_gcGpuStatus.exception_address[i] = 0;
	}

	g_gcGpuStatus.valid = true;

	if (haveError)
		memcpy(&g_gcGpuStatusLastError, &g_gcGpuStatus,
		       sizeof(struct gc_gpu_status));
}

void gc_debug_cache_gpu_status_from_irq(unsigned int acknowledge)
{
	gc_debug_cache_gpu_status_internal(GC_DEBUG_DRIVER_IRQ, acknowledge);
}

static const char *gc_power_string(enum gcpower power)
{
	switch (power) {
	case GCPWR_UNKNOWN:
		return "GCPWR_UNKNOWN";
	case GCPWR_OFF:
		return "GCPWR_OFF";
	case GCPWR_ON:
		return "GCPWR_ON";
	case GCPWR_LOW:
		return "GCPWR_LOW";
	}

	return "unknown";
}

static const char *gc_when_string(enum gc_debug_when when)
{
	switch (when) {
	case GC_DEBUG_USER_REQUEST:
		return "GC_DEBUG_USER_REQUEST";
	case GC_DEBUG_DRIVER_POWEROFF:
		return "GC_DEBUG_DRIVER_POWEROFF";
	case GC_DEBUG_DRIVER_IRQ:
		return "GC_DEBUG_DRIVER_IRQ";
	}

	return "unknown";
}


static int gc_debug_show_gpu_status(struct seq_file *s, void *data)
{
	const char *powerString = gc_power_string(gc_get_power());

	struct gc_gpu_status *status = (struct gc_gpu_status *)s->private;

	if (!status) {
		printk(KERN_ERR "%s: null status\n", __func__);
		return 0;
	}

	seq_printf(s, "GC gpu current power status: %s\n", powerString);

	if (gc_get_power() == GCPWR_ON) {
		/* update the gpu status now */
		gc_debug_cache_gpu_status_internal(GC_DEBUG_USER_REQUEST, 0);
	}

	if (!status->valid) {
		seq_printf(s, "%s: not valid.\n", status->name);
		return 0;
	}

	seq_printf(s, "%s: cached at: %s\n", status->name,
		   gc_when_string(status->when));

	seq_printf(s, "idle = 0x%08X\n", status->idle);
	seq_printf(s, "DMA state = 0x%08X\n", status->dma_state);
	seq_printf(s, "DMA address = 0x%08X\n", status->dma_addr);
	seq_printf(s, "DMA low data = 0x%08X\n", status->dma_low_data);
	seq_printf(s, "DMA high data = 0x%08X\n", status->dma_high_data);
	seq_printf(s, "Total memory reads = %d\n", status->total_reads);
	seq_printf(s, "Total memory writes = %d\n", status->total_writes);
	seq_printf(s, "Total memory read 64-bit bursts = %d\n",
		   status->total_read_bursts);
	seq_printf(s, "Total memory write 64-bit bursts = %d\n",
		   status->total_write_bursts);
	seq_printf(s, "Total memory read requests = %d\n",
		   status->total_read_reqs);
	seq_printf(s, "Total memory write requests = %d\n",
		   status->total_write_reqs);
	seq_printf(s, "irq acknowledge = 0x%08X\n", status->irq_acknowledge);

	if (MMU_ERROR(status->irq_acknowledge)) {
		int i;

		seq_printf(s, "mmu status = 0x%08X\n", status->mmu_status);

		for (i = 0; i < MMU_UNITS; i++)
			seq_printf(s, "exception address %d = 0x%08X\n",
				   i, status->exception_address[i]);
	}

	return 0;
}

static int gc_debug_open_gpu_status(struct inode *inode, struct file *file)
{
	return single_open(file, gc_debug_show_gpu_status, &g_gcGpuStatus);
}

static const struct file_operations gc_debug_fops_gpu_status = {
	.open = gc_debug_open_gpu_status,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static int gc_debug_open_gpu_last_error(struct inode *inode, struct file *file)
{
	return single_open(file,
			   gc_debug_show_gpu_status,
			   &g_gcGpuStatusLastError);
}

static const struct file_operations gc_debug_fops_gpu_last_error = {
	.open = gc_debug_open_gpu_last_error,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

/*****************************************************************************/

#define MAX_BLT_SOURCES   8

struct gc_blt_status {
	int totalCount;
	long long int totalPixels;
	int srcCount[MAX_BLT_SOURCES + 1];
	long long int srcCountPixels[MAX_BLT_SOURCES + 1];
};

static struct gc_blt_status g_gcBltStats;

void gc_debug_blt(int srccount, int dstWidth, int dstHeight)
{
	int pixels;

	if (srccount > MAX_BLT_SOURCES)
		return;

	pixels = dstWidth * dstHeight;

	g_gcBltStats.srcCount[srccount]++;
	g_gcBltStats.srcCountPixels[srccount] += pixels;

	g_gcBltStats.totalPixels += pixels;
	g_gcBltStats.totalCount++;
}

static void gc_debug_reset_blt_stats(void)
{
	int i;

	for (i = 1; i <= MAX_BLT_SOURCES; i++) {
		g_gcBltStats.srcCount[i] = 0;
		g_gcBltStats.srcCountPixels[i] = 0;
	}

	g_gcBltStats.totalCount = 0;
	g_gcBltStats.totalPixels = 0;
}

static int gc_debug_show_blt_stats(struct seq_file *s, void *data)
{
	int i;

	seq_printf(s, "total blts: %d\n", g_gcBltStats.totalCount);

	if (g_gcBltStats.totalCount) {
		for (i = 1; i <= MAX_BLT_SOURCES; i++) {
			int count = g_gcBltStats.srcCount[i];
			int total = g_gcBltStats.totalPixels;

			seq_printf(s, " %d src: %d (%d%%)\n",
					   i, count, count * 100 / total);
		}
	}

	seq_printf(s, "total dst pixels: %lld\n", g_gcBltStats.totalPixels);

	if (g_gcBltStats.totalPixels) {
		for (i = 1; i <= MAX_BLT_SOURCES; i++) {
			long long int count = g_gcBltStats.srcCountPixels[i];
			long long int total = g_gcBltStats.totalPixels;

			seq_printf(s, " %d src: %lld (%lld%%)\n",
					   i, count,
					   div64_s64(count * 100, total));
		}
	}

	gc_debug_reset_blt_stats();

	return 0;
}

static int gc_debug_open_blt_stats(struct inode *inode, struct file *file)
{
	return single_open(file, gc_debug_show_blt_stats, 0);
}

static const struct file_operations gc_debug_fops_blt_stats = {
	.open = gc_debug_open_blt_stats,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};



void gc_debug_init(struct dentry *debug_root)
{
	debugfs_create_file("id", 0664, debug_root, NULL,
			    &gc_debug_fops_gpu_id);
	debugfs_create_file("status", 0664, debug_root, NULL,
			    &gc_debug_fops_gpu_status);
	debugfs_create_file("blt_stats", 0664, debug_root, NULL,
			    &gc_debug_fops_blt_stats);
	debugfs_create_file("last_error", 0664, debug_root, NULL,
			    &gc_debug_fops_gpu_last_error);
	debugfs_create_bool("cache_status_every_irq", 0666, debug_root,
			    &g_gcCacheStatusEveryIrq);
}

/* called just BEFORE powering off */
void gc_debug_poweroff_cache(void)
{
	/* gpu id is read only once */
	gc_debug_cache_gpu_id();
}
