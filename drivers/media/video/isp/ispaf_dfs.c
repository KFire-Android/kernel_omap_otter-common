/*
 * ispaf_dfs.c
 *
 * Copyright (C) 2009 Texas Instruments.
 *
 * Contributors:
 * 	Atanas Filipov <afilipov@mm-sol.com>
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
#include <linux/slab.h>

#include "isp.h"
#include "ispreg.h"
#include "isp_af.h"
#include "ispaf_dfs.h"

/*
 * Debugfs stuff.
 */
static char *ispaf_dfs_buff;
static char *ispaf_dfs_offs;

void ispaf_dfs_dump(struct isp_device *isp)
{
	if (ispaf_dfs_buff != NULL) {
		ispaf_dfs_offs = ispaf_dfs_buff;
		ispaf_dfs_offs += sprintf(ispaf_dfs_offs, "PAXH/2: %4d\n",
				(isp_reg_readl(isp->dev, OMAP3_ISP_IOMEM_H3A,
						ISPH3A_AFPAX1) &
				ISPH3A_AF_PAXH_MASK) >> ISPH3A_AF_PAXH_SHIFT);
		ispaf_dfs_offs += sprintf(ispaf_dfs_offs, "PAXW/2: %4d\n",
				(isp_reg_readl(isp->dev, OMAP3_ISP_IOMEM_H3A,
						ISPH3A_AFPAX1) &
				ISPH3A_AF_PAXW_MASK) >> ISPH3A_AF_PAXW_SHIFT);
		ispaf_dfs_offs += sprintf(ispaf_dfs_offs, "PAXHC:  %4d\n",
				(isp_reg_readl(isp->dev, OMAP3_ISP_IOMEM_H3A,
						ISPH3A_AFPAX2) &
				ISPH3A_AF_PAXHC_MASK) >> ISPH3A_AF_PAXHC_SHIFT);
		ispaf_dfs_offs += sprintf(ispaf_dfs_offs, "PAXVC:  %4d\n",
				(isp_reg_readl(isp->dev, OMAP3_ISP_IOMEM_H3A,
						ISPH3A_AFPAX2) &
				ISPH3A_AF_PAXVC_MASK) >> ISPH3A_AF_PAXVC_SHIFT);
		ispaf_dfs_offs += sprintf(ispaf_dfs_offs, "PAXSH:  %4d\n",
				(isp_reg_readl(isp->dev, OMAP3_ISP_IOMEM_H3A,
						ISPH3A_AFPAXSTART) &
				ISPH3A_AF_PAXSH_MASK) >> ISPH3A_AF_PAXSH_SHIFT);
		ispaf_dfs_offs += sprintf(ispaf_dfs_offs, "PAXSV:  %4d\n",
				(isp_reg_readl(isp->dev, OMAP3_ISP_IOMEM_H3A,
						ISPH3A_AFPAXSTART) &
				ISPH3A_AF_PAXSV_MASK) >> ISPH3A_AF_PAXSV_SHIFT);
	}
}

static ssize_t ispaf_dfs_read(struct file *file, char __user *buf,
				size_t count, loff_t *ppos)
{
	if (ispaf_dfs_buff && ispaf_dfs_buff < ispaf_dfs_offs) {
		return simple_read_from_buffer(buf, count, ppos,
					       ispaf_dfs_buff,
					       ispaf_dfs_offs -
					       ispaf_dfs_buff);
	}
	return 0;
}

static const struct file_operations ispaf_dfs_ops = {
	.owner		= THIS_MODULE,
	.read		= ispaf_dfs_read
};

void ispaf_dfs_setup(struct isp_device *isp)
{
	struct dentry *dfs_root = isp_get_dfs_root();

	if (isp && dfs_root) {
		isp->dfs_af = debugfs_create_file("ispaf_regs", S_IRUGO,
					dfs_root, isp, &ispaf_dfs_ops);
		if (IS_ERR(isp->dfs_af)) {
			isp->dfs_af = NULL;
			dev_err(isp->dev, "Can't create DFS ispaf files !");
			return;
		}
		ispaf_dfs_buff = kmalloc(PAGE_SIZE, GFP_KERNEL);
		ispaf_dfs_offs = ispaf_dfs_buff;
	}
}

void ispaf_dfs_shutdown(struct isp_device *isp)
{
	struct dentry *dfs_root = isp_get_dfs_root();

	if (dfs_root)
		debugfs_remove(isp->dfs_af);

	kfree(ispaf_dfs_buff);
	ispaf_dfs_buff = NULL;
	ispaf_dfs_offs = ispaf_dfs_buff;
}
