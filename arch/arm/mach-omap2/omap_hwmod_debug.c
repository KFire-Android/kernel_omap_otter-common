/*
 * omap_hwmod debugfs implementation
 *
 * Copyright (C) 2010 Texas Instruments, Inc.
 *
 * Benoit Cousson
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Expose a debufs interface in order to check and modify hwmod state
 * The current directory structure is:
 *
 * hwmods
 *  +-hwmod_mpu
 *  +-hwmod_dsp
 *  .
 *  .
 *  +-hwmod_xxx   : hwmod node
 *     +-state    : internal state (RO)
 *     +-summary  : global view of hwmod definition
 *     +-resets   : reset lines
 *        +-rst1  : reset state / control (RW)
 *        +-rst2  :
 *
 * To do:
 * - Add irq / dma dump
 * - Add clock dump
 */
#undef DEBUG

#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/debugfs.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/seq_file.h>
#include <linux/slab.h>

#include <plat/clock.h>
#include <plat/common.h>
#include <plat/omap_hwmod.h>


/*
 * DEBUGFS helper macros
 *
 * This code is already used in several omap drivers, so eventually it will be
 * good to move that to plat-omap and share the same code.
 */

#define DEFINE_DEBUGFS_SHOW(__fops, __show)				\
static int __fops ## _open(struct inode *inode, struct file *file)	\
{									\
	return single_open(file, __show, inode->i_private);		\
}									\
static const struct file_operations __fops = {				\
	.owner	 	= THIS_MODULE,					\
	.open	 	= __fops ## _open,				\
	.read           = seq_read,					\
	.llseek         = seq_lseek,					\
	.release        = single_release,				\
};

/*
 * Aggregate reset information in a specific structure, because the reset
 * node does not contain any link to the parent hwmod structure
 */
struct reset_info {
	struct omap_hwmod 	*oh;
	const char		*name;
	u8			rst_shift;
};

/* internal hwmod states */
static const char* states[_HWMOD_STATE_LAST + 1] = {
	[_HWMOD_STATE_UNKNOWN] 		= "unknown",
	[_HWMOD_STATE_REGISTERED] 	= "registered",
	[_HWMOD_STATE_CLKS_INITED] 	= "clks_inited",
	[_HWMOD_STATE_INITIALIZED] 	= "initialized",
	[_HWMOD_STATE_ENABLED] 		= "enabled",
	[_HWMOD_STATE_IDLE] 		= "idle",
	[_HWMOD_STATE_DISABLED] 	= "disabled",
};

const char * _state_str(int state)
{
	if (state < 0 || state > _HWMOD_STATE_LAST)
		return "invalid_state";

	return states[state];
}

static int _set_state(void *data, u64 val)
{
	*(u8 *)data = val;
	return 0;
}
static int _get_state(void *data, u64 *val)
{
	*val = *(u8 *)data;
	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(state_fops, _get_state, _set_state, "%llu\n");

static int default_open(struct inode *inode, struct file *file)
{
	if (inode->i_private)
		file->private_data = inode->i_private;

	return 0;
}

#define MAX_BUFFER_SIZE 16

static ssize_t read_hwmod_state(struct file *file, char __user *user_buf,
			        size_t count, loff_t *ppos)
{
	char buf[MAX_BUFFER_SIZE];
	struct omap_hwmod *oh = file->private_data;
	const char *msg = _state_str(oh->_state);

	int len = snprintf(buf, MAX_BUFFER_SIZE, "%s\n", msg);

	return simple_read_from_buffer(user_buf, count, ppos, buf, len);
}

static ssize_t write_hwmod_state(struct file *file, const char __user *user_buf,
			         size_t count, loff_t *ppos)
{
	char buf[32];
	int buf_size;
	char *p;
	int len;
	struct omap_hwmod *oh = file->private_data;

	if (!oh || !oh->name)
		return -EINVAL;

	buf_size = min(count, (sizeof(buf)-1));
	if (copy_from_user(buf, user_buf, buf_size))
		return -EFAULT;

	p = memchr(buf, '\n', buf_size);
	len = p ? p - buf : buf_size;
	buf[len] = '\0';

	if (!strncmp(buf, "enable", len))
		omap_hwmod_enable(oh);
	else if (!strncmp(buf, "idle", len))
		omap_hwmod_idle(oh);
	else if (!strncmp(buf, "disable", len))
		omap_hwmod_shutdown(oh);
	else
		pr_warning("write_hwmod_state: invalid state: %s\n", buf);

	return count;
}

static const struct file_operations hwmod_state_fops = {
	.read =		read_hwmod_state,
	.write =	write_hwmod_state,
	.open =		default_open,
};

/**
 * _reset_get - helper function for debugfs read to reset line
 * @data: pointer to data initialized during debugfs_create_file. In this
 * case this is a pointer to struct reset_info
 * @val: pointer to the value that will be read and propagate to the debufs
 * interface
 *
 * Returns: 0 if the reset is deasserted or asserted (0 or 1), any other states
 * are invalid, so return -EINVAL in that case.
 */
static int _reset_get(void *data, u64 *val)
{
	struct reset_info *info = data;
	int state;

	state = omap_hwmod_hardreset_state(info->oh, info->name);

	pr_debug("_reset_get: %s:%d %d\n", info->name, info->rst_shift, state);

	*val = (u64)state;

	if (state >= 0)
		return 0;

	return *val;
}

/**
 * _reset_set - helper function for debugfs write to reset line
 * @data: pointer to data initialized during debugfs_create_file. In this
 * case this is a pointer to struct reset_info
 * @val: value that should written from the debufs interface
 *
 * Assert or deassert the reset line depending of the value written
 * on the debugfs "rst" file entry
 * Returns -EINVAL if val is not 0 or 1.
 */
static int _reset_set(void *data, u64 val)
{
	struct reset_info *info = data;
	int ret = -EINVAL;

	pr_debug("_reset_set: %s[%d]: %llu\n", info->name, info->rst_shift,
					       val);

	if (val == 1)
		ret = omap_hwmod_hardreset_assert(info->oh, info->name);
	else if	(val == 0)
		ret = omap_hwmod_hardreset_deassert(info->oh, info->name);

	return ret;
}

DEFINE_SIMPLE_ATTRIBUTE(reset_fops, _reset_get, _reset_set, "%llu\n");

static struct omap_hwmod_addr_space* _print_addresses(struct seq_file *s,
						      struct omap_hwmod *oh)
{
	struct omap_hwmod_ocp_if *os = NULL;
	struct omap_hwmod_addr_space *mem;
	int j;

	if (!oh || oh->slaves_cnt == 0) {
		seq_printf(s, "  address:      N/A\n");
		return NULL;
	}

	for (j = 0; j < oh->slaves_cnt; j++) {
		int i;

		os = oh->slaves[j];
		if (!os->addr)
			continue;

		if (os->user & OCP_USER_MPU) {
			for (i = 0, mem = os->addr; i < os->addr_cnt; i++, mem++)
				seq_printf(s, "  address:      0x%08x-0x%08x (mpu)\n",
					   mem->pa_start, mem->pa_end);
		} else {
			for (i = 0, mem = os->addr; i < os->addr_cnt; i++, mem++)
				seq_printf(s, "  address:      0x%08x-0x%08x\n",
					   mem->pa_start, mem->pa_end);
		}
	}

	return NULL;
}

static int show_hwmod_summary(struct omap_hwmod *oh, void* param)
{
	struct seq_file *s = param;
	int i;

	if (!oh || !oh->name)
		return -EINVAL;

	seq_printf(s, "%s\n", oh->name);
	seq_printf(s, "  state:        %s:%d\n", _state_str(oh->_state),
		   oh->_state);
	seq_printf(s, "  class:        %s\n", oh->class->name);
	seq_printf(s, "  flags:        0x%04x\n", oh->flags);

	_print_addresses(s, oh);

	if (!IS_ERR_OR_NULL(oh->_clk))
		seq_printf(s, "  clock:        %s\n", oh->_clk->name);

	if (oh->rst_lines_cnt > 0)
		seq_printf(s, "  resets(%d):\n", oh->rst_lines_cnt);
	for (i = 0; i < oh->rst_lines_cnt; i++) {
		int state = omap_hwmod_hardreset_state(oh,
						       oh->rst_lines[i].name);
		seq_printf(s, "    %s:%d\n", oh->rst_lines[i].name,
			   state);
	}

	if (oh->mpu_irqs_cnt > 0)
		seq_printf(s, "  irqs(%d):\n", oh->mpu_irqs_cnt);
	for (i = 0; i < oh->mpu_irqs_cnt; i++)
		seq_printf(s, "    %s:%d\n", oh->mpu_irqs[i].name,
			   oh->mpu_irqs[i].irq);

	if (oh->sdma_reqs_cnt > 0)
		seq_printf(s, "  dmas(%d):\n", oh->sdma_reqs_cnt);
	for (i = 0; i < oh->sdma_reqs_cnt; i++)
		seq_printf(s, "    %s:%d\n", oh->sdma_reqs[i].name,
			   oh->sdma_reqs[i].dma_req);

	return 0;
}

static int show_hwmod(struct seq_file *s, void *unused)
{
	struct omap_hwmod *oh = s->private;

	show_hwmod_summary(oh, s);

	return 0;
}
DEFINE_DEBUGFS_SHOW(_hwmod_fops, show_hwmod);

/**
 * create_debugfs_entry - create a debugfs entry per omap_hwmod
 * @oh: struct omap_hwmod *
 * @parent: reference to the parent directory dentry needed for the creation
 * of the files inside the debugfs directory
 *
 * Create a directory entry for each hwmod.
 * Each directory will contain at least one state file and potentially
 * some files that will represent the HW reset lines of that IP.
 */
static int create_debugfs_entry(struct omap_hwmod *oh, void* parent)
{
	struct dentry *d, *fd;
	int i;

	if (!oh || !oh->name)
		return -EINVAL;

	pr_debug("creating debugfs entry: %s[%p]\n", oh->name, oh);

	d = debugfs_create_dir(oh->name, parent);
	if (IS_ERR(d))
		return PTR_ERR(d);

	fd = debugfs_create_file("summary", S_IRUGO, d, oh, &_hwmod_fops);
	if (IS_ERR(fd))
		return PTR_ERR(fd);

	fd = debugfs_create_file("state", S_IRUGO|S_IWUSR, d, oh,
				 &hwmod_state_fops);
	if (IS_ERR(fd))
		return PTR_ERR(fd);

	if (oh->rst_lines_cnt > 0) {
		struct dentry *drst = debugfs_create_dir("resets", d);
		if (IS_ERR(drst))
			return PTR_ERR(drst);

		/* Create one directory entry per reset line */
		for (i = 0; i < oh->rst_lines_cnt; i++) {
			struct reset_info *info;
			const char *name = oh->rst_lines[i].name;

			/* XXX need to find a way to free that memory */
			info = kmalloc(sizeof(struct reset_info), GFP_KERNEL);
			if (!info)
				return -ENOMEM;

			info->oh 	= oh;
			info->name 	= oh->rst_lines[i].name;
			info->rst_shift = oh->rst_lines[i].rst_shift;

			debugfs_create_file(name, S_IRUGO|S_IWUSR, drst, info,
					    &reset_fops);
		}
	}

	return 0;
}

/* hwmod_debugfs_init - Initialize the debugfs directory tree for hwmods */
static int __init hwmod_debugfs_init(void)
{
	struct dentry *d;

	pr_warning("omap_hwmod: Initialize debugfs support");

	d = debugfs_create_dir("hwmods", NULL);
	if (IS_ERR(d))
		return PTR_ERR(d);

	omap_hwmod_for_each(create_debugfs_entry, d);

	return 0;
}
late_initcall(hwmod_debugfs_init);
