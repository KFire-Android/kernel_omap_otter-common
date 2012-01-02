/*
 * omap-abe-dbg.c  --  OMAP ALSA SoC DAI driver using Audio Backend
 *
 * Copyright (C) 2010 Texas Instruments
 *
 * Contact: Liam Girdwood <lrg@ti.com>
 *          Misael Lopez Cruz <misael.lopez@ti.com>
 *          Sebastien Guiriec <s-guiriec@ti.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 */

#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/dma-mapping.h>
#include <linux/debugfs.h>

#include <plat/dma.h>
#include <plat/dma-44xx.h>

#include <sound/soc.h>

#include "omap-abe-priv.h"

/* TODO: size in bytes of debug options */
#define OMAP_ABE_DBG_FLAG1_SIZE	0
#define OMAP_ABE_DBG_FLAG2_SIZE	0
#define OMAP_ABE_DBG_FLAG3_SIZE	0

#ifdef CONFIG_DEBUG_FS

static int abe_dbg_get_dma_pos(struct omap_abe *abe)
{
	return omap_get_dma_dst_pos(abe->debugfs.dma_ch) - abe->debugfs.buffer_addr;
}

static void abe_dbg_dma_irq(int ch, u16 stat, void *data)
{
}

static int abe_dbg_start_dma(struct omap_abe *abe, int circular)
{
	struct omap_dma_channel_params dma_params;
	int err;

	/* start the DMA in either :-
	 *
	 * 1) circular buffer mode where the DMA will restart when it get to
	 *    the end of the buffer.
	 * 2) default mode, where DMA stops at the end of the buffer.
	 */

	abe->debugfs.dma_req = OMAP44XX_DMA_ABE_REQ_7;
	err = omap_request_dma(abe->debugfs.dma_req, "ABE debug",
			       abe_dbg_dma_irq, abe, &abe->debugfs.dma_ch);
	if (abe->debugfs.circular) {
		/*
		 * Link channel with itself so DMA doesn't need any
		 * reprogramming while looping the buffer
		 */
		omap_dma_link_lch(abe->debugfs.dma_ch, abe->debugfs.dma_ch);
	}

	memset(&dma_params, 0, sizeof(dma_params));
	dma_params.data_type = OMAP_DMA_DATA_TYPE_S32;
	dma_params.trigger = abe->debugfs.dma_req;
	dma_params.sync_mode = OMAP_DMA_SYNC_FRAME;
	dma_params.src_amode = OMAP_DMA_AMODE_DOUBLE_IDX;
	dma_params.dst_amode = OMAP_DMA_AMODE_POST_INC;
	dma_params.src_or_dst_synch = OMAP_DMA_SRC_SYNC;
	dma_params.src_start = abe->aess->fw_info->map[OMAP_AESS_DMEM_DEBUG_FIFO_ID].offset + ABE_DEFAULT_BASE_ADDRESS_L3 + ABE_DMEM_BASE_OFFSET_MPU;
	dma_params.dst_start = abe->debugfs.buffer_addr;
	dma_params.src_port = OMAP_DMA_PORT_MPUI;
	dma_params.src_ei = 1;
	dma_params.src_fi = 1 - abe->debugfs.elem_bytes;

	 /* 128 bytes shifted into words */
	dma_params.elem_count = abe->debugfs.elem_bytes >> 2;
	dma_params.frame_count =
			abe->debugfs.buffer_bytes / abe->debugfs.elem_bytes;
	omap_set_dma_params(abe->debugfs.dma_ch, &dma_params);

	omap_enable_dma_irq(abe->debugfs.dma_ch, OMAP_DMA_FRAME_IRQ);
	omap_set_dma_src_burst_mode(abe->debugfs.dma_ch, OMAP_DMA_DATA_BURST_16);
	omap_set_dma_dest_burst_mode(abe->debugfs.dma_ch, OMAP_DMA_DATA_BURST_16);

	abe->debugfs.reader_offset = 0;

	pm_runtime_get_sync(abe->dev);
	omap_start_dma(abe->debugfs.dma_ch);
	return 0;
}

static void abe_dbg_stop_dma(struct omap_abe *abe)
{
	/* Since we are using self linking, there is a
	chance that the DMA as re-enabled the channel just after disabling it */
	while (omap_get_dma_active_status(abe->debugfs.dma_ch))
		omap_stop_dma(abe->debugfs.dma_ch);

	if (abe->debugfs.circular)
		omap_dma_unlink_lch(abe->debugfs.dma_ch, abe->debugfs.dma_ch);

	omap_free_dma(abe->debugfs.dma_ch);
	pm_runtime_put_sync(abe->dev);
}

static int abe_open_data(struct inode *inode, struct file *file)
{
	struct omap_abe *abe = inode->i_private;

	/* adjust debug word size based on any user params */
	if (abe->debugfs.format1)
		abe->debugfs.elem_bytes += OMAP_ABE_DBG_FLAG1_SIZE;
	if (abe->debugfs.format2)
		abe->debugfs.elem_bytes += OMAP_ABE_DBG_FLAG2_SIZE;
	if (abe->debugfs.format3)
		abe->debugfs.elem_bytes += OMAP_ABE_DBG_FLAG3_SIZE;

	abe->debugfs.buffer_bytes = abe->debugfs.elem_bytes * 4 *
							abe->debugfs.buffer_msecs;

	abe->debugfs.buffer = dma_alloc_writecombine(abe->dev,
			abe->debugfs.buffer_bytes, &abe->debugfs.buffer_addr, GFP_KERNEL);
	if (abe->debugfs.buffer == NULL) {
		dev_err(abe->dev, "can't alloc %d bytes for trace DMA buffer\n",
				abe->debugfs.buffer_bytes);
		return -ENOMEM;
	}

	file->private_data = inode->i_private;
	abe->debugfs.complete = 0;
	abe_dbg_start_dma(abe, abe->debugfs.circular);

	return 0;
}

static int abe_release_data(struct inode *inode, struct file *file)
{
	struct omap_abe *abe = inode->i_private;

	abe_dbg_stop_dma(abe);

	dma_free_writecombine(abe->dev, abe->debugfs.buffer_bytes,
				      abe->debugfs.buffer, abe->debugfs.buffer_addr);
	return 0;
}

static ssize_t abe_copy_to_user(struct omap_abe *abe, char __user *user_buf,
			       size_t count)
{
	/* check for reader buffer wrap */
	if (abe->debugfs.reader_offset + count > abe->debugfs.buffer_bytes) {
		size_t size = abe->debugfs.buffer_bytes - abe->debugfs.reader_offset;

		/* wrap */
		if (copy_to_user(user_buf,
			abe->debugfs.buffer + abe->debugfs.reader_offset, size))
			return -EFAULT;

		/* need to just return if non circular */
		if (!abe->debugfs.circular) {
			abe->debugfs.complete = 1;
			return count;
		}

		if (copy_to_user(user_buf + size,
			abe->debugfs.buffer, count - size))
			return -EFAULT;
		abe->debugfs.reader_offset = count - size;
		return count;
	} else {
		/* no wrap */
		if (copy_to_user(user_buf,
			abe->debugfs.buffer + abe->debugfs.reader_offset, count))
			return -EFAULT;
		abe->debugfs.reader_offset += count;

		if (!abe->debugfs.circular &&
				abe->debugfs.reader_offset == abe->debugfs.buffer_bytes)
			abe->debugfs.complete = 1;

		return count;
	}
}

static ssize_t abe_read_data(struct file *file, char __user *user_buf,
			       size_t count, loff_t *ppos)
{
	ssize_t ret = 0;
	struct omap_abe *abe = file->private_data;
	DECLARE_WAITQUEUE(wait, current);
	int dma_offset, bytes;

	add_wait_queue(&abe->debugfs.wait, &wait);
	do {
		set_current_state(TASK_INTERRUPTIBLE);
		/* TODO: Check if really needed. Or adjust sleep delay
		 * If not delay trace is not working */
		msleep_interruptible(1);

		/* is DMA finished ? */
		if (abe->debugfs.complete)
			break;

		dma_offset = abe_dbg_get_dma_pos(abe);


		/* get maximum amount of debug bytes we can read */
		if (dma_offset >= abe->debugfs.reader_offset) {
			/* dma ptr is ahead of reader */
			bytes = dma_offset - abe->debugfs.reader_offset;
		} else {
			/* dma ptr is behind reader */
			bytes = dma_offset + abe->debugfs.buffer_bytes -
				abe->debugfs.reader_offset;
		}

		if (count > bytes)
			count = bytes;

		if (count > 0) {
			ret = abe_copy_to_user(abe, user_buf, count);
			break;
		}

		if (file->f_flags & O_NONBLOCK) {
			ret = -EAGAIN;
			break;
		}

		if (signal_pending(current)) {
			ret = -ERESTARTSYS;
			break;
		}

		schedule();

	} while (1);

	__set_current_state(TASK_RUNNING);
	remove_wait_queue(&abe->debugfs.wait, &wait);

	return ret;
}

static const struct file_operations omap_abe_fops = {
	.open = abe_open_data,
	.read = abe_read_data,
	.release = abe_release_data,
};

void abe_init_debugfs(struct omap_abe *abe)
{
	abe->debugfs.d_root = debugfs_create_dir("omap-abe", NULL);
	if (!abe->debugfs.d_root) {
		dev_err(abe->dev, "Failed to create debugfs directory\n");
		return;
	}

	abe->debugfs.d_fmt1 = debugfs_create_bool("format1", 0644,
						 abe->debugfs.d_root,
						 &abe->debugfs.format1);
	if (!abe->debugfs.d_fmt1)
		dev_err(abe->dev, "Failed to create format1 debugfs file\n");

	abe->debugfs.d_fmt2 = debugfs_create_bool("format2", 0644,
						 abe->debugfs.d_root,
						 &abe->debugfs.format2);
	if (!abe->debugfs.d_fmt2)
		dev_err(abe->dev, "Failed to create format2 debugfs file\n");

	abe->debugfs.d_fmt3 = debugfs_create_bool("format3", 0644,
						 abe->debugfs.d_root,
						 &abe->debugfs.format3);
	if (!abe->debugfs.d_fmt3)
		dev_err(abe->dev, "Failed to create format3 debugfs file\n");

	abe->debugfs.d_elem_bytes = debugfs_create_u32("element_bytes", 0604,
						 abe->debugfs.d_root,
						 &abe->debugfs.elem_bytes);
	if (!abe->debugfs.d_elem_bytes)
		dev_err(abe->dev, "Failed to create element size debugfs file\n");

	abe->debugfs.d_size = debugfs_create_u32("msecs", 0644,
						 abe->debugfs.d_root,
						 &abe->debugfs.buffer_msecs);
	if (!abe->debugfs.d_size)
		dev_err(abe->dev, "Failed to create buffer size debugfs file\n");

	abe->debugfs.d_circ = debugfs_create_bool("circular", 0644,
						 abe->debugfs.d_root,
						 &abe->debugfs.circular);
	if (!abe->debugfs.d_size)
		dev_err(abe->dev, "Failed to create circular mode debugfs file\n");

	abe->debugfs.d_data = debugfs_create_file("debug", 0644,
						 abe->debugfs.d_root,
						 abe, &omap_abe_fops);
	if (!abe->debugfs.d_data)
		dev_err(abe->dev, "Failed to create data debugfs file\n");

	abe->debugfs.d_opp = debugfs_create_u32("opp_level", 0604,
						 abe->debugfs.d_root,
						 &abe->opp.level);
	if (!abe->debugfs.d_opp)
		dev_err(abe->dev, "Failed to create OPP level debugfs file\n");

	init_waitqueue_head(&abe->debugfs.wait);
}

void abe_cleanup_debugfs(struct omap_abe *abe)
{
	debugfs_remove_recursive(abe->debugfs.d_root);
}

#else

inline void abe_init_debugfs(struct omap_abe *abe)
{
}

inline void abe_cleanup_debugfs(struct omap_abe *abe)
{
}
#endif
