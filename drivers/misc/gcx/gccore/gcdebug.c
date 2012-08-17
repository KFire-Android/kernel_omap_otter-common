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
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/gcx.h>
#include <linux/gccore.h>
#include "gcmain.h"

#define MMU_UNITS  4
#define MMU_ERROR(irq_ack) ((irq_ack & 0x40000000) != 0)

#if GCDEBUG_ENABLE
#define STATUS_DUMP(s, msg, ...)					\
	do {								\
		if (s)							\
			seq_printf(s, msg, __VA_ARGS__);		\
		else							\
			gc_dump_string(NULL, 0, msg, __VA_ARGS__);	\
	} while (0)
#else
#define STATUS_DUMP(s, msg, ...)					\
	do {								\
		if (s)							\
			seq_printf(s, msg, __VA_ARGS__);		\
		else							\
			printk(KERN_ERR msg, __VA_ARGS__);		\
	} while (0)
#endif

enum gc_debug_when {
	GC_DEBUG_USER_REQUEST,
	GC_DEBUG_DRIVER_POWEROFF,
	GC_DEBUG_DRIVER_IRQ,
	GC_DEBUG_DRIVER_REQUEST,
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
static struct dentry *debug_root;

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
	g_gcGpuId.chipMinorFeatures = gc_read_reg(GC_FEATURES0_Address);
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
	bool           dma_addr_changing;
	const char        *name;
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

enum gc_debug_when gc_gpu_status_when;

struct gc_gpu_status gc_gpu_status = {
	.name = "GPU status"
};
struct gc_gpu_status gc_gpu_status_last_error = {
	.name = "GPU last error status"
};

/* By default we don't cache the status on every irq */
static int gc_cache_status_every_irq;


static bool gc_dma_addr_changing(void)
{
	static const unsigned int detectcount = 1000;
	unsigned int address1, address2;
	unsigned int i;

	address1 = gc_read_reg(GCREG_FE_DEBUG_CUR_CMD_ADR_Address);
	for (i = 0; i < detectcount; i += 1) {
		address2 = gc_read_reg(GCREG_FE_DEBUG_CUR_CMD_ADR_Address);
		if (address1 != address2)
			break;
	}

	return i < detectcount;
}

void gc_debug_cache_gpu_status(
	struct gc_gpu_status *status,
	unsigned int acknowledge)
{
	int i;

	if (!status) {
		status = &gc_gpu_status;
		gc_gpu_status_when = GC_DEBUG_DRIVER_REQUEST;
	}

	status->dma_addr_changing = gc_dma_addr_changing();

	status->idle =
		gc_read_reg(GCREG_HI_IDLE_Address);
	status->dma_state =
		gc_read_reg(GCREG_FE_DEBUG_STATE_Address);
	status->dma_addr =
		gc_read_reg(GCREG_FE_DEBUG_CUR_CMD_ADR_Address);
	status->dma_low_data =
		gc_read_reg(GCREG_FE_DEBUG_CMD_LOW_REG_Address);
	status->dma_high_data =
		gc_read_reg(GCREG_FE_DEBUG_CMD_HI_REG_Address);
	status->total_reads =
		gc_read_reg(GC_TOTAL_READS_Address);
	status->total_writes =
		gc_read_reg(GC_TOTAL_WRITES_Address);
	status->total_read_bursts =
		gc_read_reg(GC_TOTAL_READ_BURSTS_Address);
	status->total_write_bursts =
		gc_read_reg(GC_TOTAL_WRITE_BURSTS_Address);
	status->total_read_reqs =
		gc_read_reg(GC_TOTAL_READ_REQS_Address);
	status->total_write_reqs =
		gc_read_reg(GC_TOTAL_WRITE_REQS_Address);
	status->irq_acknowledge = acknowledge;

	/* Is it valid/useful to read the mmu registers for
	 * other error conditions? */
	if (MMU_ERROR(acknowledge)) {
		status->mmu_status =
			gc_read_reg(GCREG_MMU_STATUS_Address);

		for (i = 0; i < MMU_UNITS; i++)
			status->exception_address[i] =
				gc_read_reg(GCREG_MMU_EXCEPTION_Address + i);
	} else {
		status->mmu_status = 0;

		for (i = 0; i < MMU_UNITS; i++)
			status->exception_address[i] = 0;
	}

	status->valid = true;
}

void gc_debug_cache_gpu_status_from_irq(unsigned int acknowledge)
{
	bool haveError = (acknowledge & 0xC0000000) != 0;

	if (!gc_cache_status_every_irq && !haveError) {
		/* called from irq, no error, not caching every irq */
		return;
	}

	gc_debug_cache_gpu_status(&gc_gpu_status, acknowledge);
	gc_gpu_status_when = GC_DEBUG_DRIVER_IRQ;

	if (haveError)
		memcpy(&gc_gpu_status_last_error, &gc_gpu_status,
		       sizeof(struct gc_gpu_status));
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
	case GC_DEBUG_DRIVER_REQUEST:
		return "GC_DEBUG_DRIVER_REQUEST";
	case GC_DEBUG_DRIVER_IRQ:
		return "GC_DEBUG_DRIVER_IRQ";
	}

	return "unknown";
}


static void gc_dump_status_internal(struct seq_file *s,
				    struct gc_gpu_status *status)
{
	if (!status->valid) {
		STATUS_DUMP(s, "%s: not valid.\n", status->name);
		return;
	}

	STATUS_DUMP(s, "  idle = 0x%08X\n", status->idle);
	STATUS_DUMP(s, "  DMA state = 0x%08X\n", status->dma_state);
	STATUS_DUMP(s, "  DMA address = 0x%08X (%s)\n", status->dma_addr,
		    status->dma_addr_changing ? "running" : "not running");
	STATUS_DUMP(s, "  DMA low data = 0x%08X\n", status->dma_low_data);
	STATUS_DUMP(s, "  DMA high data = 0x%08X\n", status->dma_high_data);
	STATUS_DUMP(s, "  Total memory reads = %d\n", status->total_reads);
	STATUS_DUMP(s, "  Total memory writes = %d\n", status->total_writes);
	STATUS_DUMP(s, "  Total memory read 64-bit bursts = %d\n",
				status->total_read_bursts);
	STATUS_DUMP(s, "  Total memory write 64-bit bursts = %d\n",
				status->total_write_bursts);
	STATUS_DUMP(s, "  Total memory read requests = %d\n",
				status->total_read_reqs);
	STATUS_DUMP(s, "  Total memory write requests = %d\n",
				status->total_write_reqs);

	if (MMU_ERROR(status->irq_acknowledge)) {
		int i;

		STATUS_DUMP(s, "mmu status = 0x%08X\n", status->mmu_status);

		for (i = 0; i < MMU_UNITS; i++)
			STATUS_DUMP(s, "exception address %d = 0x%08X\n",
				    i, status->exception_address[i]);
	}
}

void gc_debug_dump_status(const char *function, int line)
{
	STATUS_DUMP(NULL, "Current GC gpu status requested from %s(%d).\n",
				function, line);

	gc_debug_cache_gpu_status(&gc_gpu_status, 0);
	gc_gpu_status_when = GC_DEBUG_DRIVER_REQUEST;

	gc_dump_status_internal(NULL, &gc_gpu_status);

	GCDBG_FLUSHDUMP(NULL);
}

static int gc_debug_show_gpu_status(struct seq_file *s, void *data)
{
	const char *powerString = gc_power_string(gcpwr_get());

	struct gc_gpu_status *status = (struct gc_gpu_status *)s->private;

	if (!status) {
		printk(KERN_ERR "%s: null status\n", __func__);
		return 0;
	}

	seq_printf(s, "GC gpu current power status: %s\n", powerString);

	if (gcpwr_get() == GCPWR_ON) {
		/* update the gpu status now */
		gc_debug_cache_gpu_status(&gc_gpu_status, 0);
		gc_gpu_status_when = GC_DEBUG_USER_REQUEST;
	}

	seq_printf(s, "%s: cached at: %s\n", status->name,
			   gc_when_string(gc_gpu_status_when));

	gc_dump_status_internal(s, status);

	return 0;
}

static int gc_debug_open_gpu_status(struct inode *inode, struct file *file)
{
	return single_open(file, gc_debug_show_gpu_status, &gc_gpu_status);
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
			   &gc_gpu_status_last_error);
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

/*****************************************************************************/

static int gc_debug_show_log_dump(struct seq_file *s, void *data)
{
	GCDBG_FLUSHDUMP(s);
	return 0;
}

static int gc_debug_open_log_dump(struct inode *inode, struct file *file)
{
	return single_open(file, gc_debug_show_log_dump, 0);
}

static const struct file_operations gc_debug_fops_log_dump = {
	.open = gc_debug_open_log_dump,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

/*****************************************************************************/

static int gc_debug_show_log_enable(struct seq_file *s, void *data)
{
	GCDBG_SHOWENABLED(s);
	return 0;
}

static int gc_debug_open_log_enable(struct inode *inode, struct file *file)
{
	return single_open(file, gc_debug_show_log_enable, 0);
}

static ssize_t gc_debug_write_log_enable(
	struct file *file,
	const char __user *user_buf,
	size_t count, loff_t *ppos)
{
	char buf[256];
	size_t len;
	unsigned long val;
	int ret;
	char *name = 0;

	len = min(count, sizeof(buf) - 1);
	if (copy_from_user(buf, user_buf, len))
		return -EFAULT;

	buf[len] = '\0';

	ret = kstrtoul(buf, 0, &val);
	if (ret < 0) {
		int i;
		for (i = 0; i < len - 1; i++) {
			if (buf[i] == ' ') {
				buf[i] = 0;

				ret = kstrtoul(&buf[i+1], 0, &val);
				if (ret < 0)
					return -EINVAL;

				name = &buf[0];
				break;
			}
		}

		if (!name)
			return -EINVAL;
	}

	if (name)
		GCDBG_SETFILTER(name, val);
	else if (val)
		GCDBG_ENABLEDUMP();
	else
		GCDBG_DISABLEDUMP();

	return count;
}

static const struct file_operations gc_debug_fops_log_enable = {
	.open    = gc_debug_open_log_enable,
	.write   = gc_debug_write_log_enable,
	.read    = seq_read,
	.llseek  = seq_lseek,
	.release = single_release,
};

/*****************************************************************************/

static int gc_debug_show_log_reset(struct seq_file *s, void *data)
{
	GCDBG_RESETDUMP();
	return 0;
}

static int gc_debug_open_log_reset(struct inode *inode, struct file *file)
{
	return single_open(file, gc_debug_show_log_reset, 0);
}

static ssize_t gc_debug_write_log_reset(
	struct file *file,
	const char __user *user_buf,
	size_t count, loff_t *ppos)
{
	GCDBG_RESETDUMP();
	return count;
}

static const struct file_operations gc_debug_fops_log_reset = {
	.open    = gc_debug_open_log_reset,
	.write   = gc_debug_write_log_reset,
	.read    = seq_read,
	.llseek  = seq_lseek,
	.release = single_release,
};

/*****************************************************************************/

static int gc_debug_show_cur_freq(struct seq_file *s, void *data)
{
	unsigned mhz = gcpwr_get_speed();
	if (mhz)
		seq_printf(s, "cur freq: %d mhz\n", mhz);
	else
		seq_printf(s, "unable to read cur freq\n");

	return 0;
}

static int gc_debug_open_cur_freq(struct inode *inode, struct file *file)
{
	return single_open(file, gc_debug_show_cur_freq, 0);
}

static const struct file_operations gc_debug_fops_cur_freq = {
	.open    = gc_debug_open_cur_freq,
	.write   = NULL,
	.read    = seq_read,
	.llseek  = seq_lseek,
	.release = single_release,
};

/*****************************************************************************/

void gc_debug_init(void)
{
	struct dentry *logDir;

	debug_root = debugfs_create_dir("gcx", NULL);
	if (!debug_root)
		return;

	debugfs_create_file("id", 0664, debug_root, NULL,
			    &gc_debug_fops_gpu_id);
	debugfs_create_file("status", 0664, debug_root, NULL,
			    &gc_debug_fops_gpu_status);
	debugfs_create_file("blt_stats", 0664, debug_root, NULL,
			    &gc_debug_fops_blt_stats);
	debugfs_create_file("last_error", 0664, debug_root, NULL,
			    &gc_debug_fops_gpu_last_error);
	debugfs_create_bool("cache_status_every_irq", 0664, debug_root,
			    &gc_cache_status_every_irq);
	debugfs_create_file("cur_freq", 0664, debug_root, NULL,
			    &gc_debug_fops_cur_freq);

	logDir = debugfs_create_dir("log", debug_root);
	if (!logDir)
		return;

	debugfs_create_file("enable", 0664, logDir, NULL,
						&gc_debug_fops_log_enable);
	debugfs_create_file("reset", 0664, logDir, NULL,
						&gc_debug_fops_log_reset);
	debugfs_create_file("dump", 0664, logDir, NULL,
						&gc_debug_fops_log_dump);
}

void gc_debug_shutdown(void)
{
	if (debug_root)
		debugfs_remove_recursive(debug_root);
}

/* called just BEFORE powering off */
void gc_debug_poweroff_cache(void)
{
	/* gpu id is read only once */
	gc_debug_cache_gpu_id();
}
