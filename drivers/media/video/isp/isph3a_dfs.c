/*
 * isph3a_dfs.c
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
#include "isph3a.h"
#include "isph3a_dfs.h"

/*
 * Debugfs stuff.
 */
static char *isph3a_dfs_buff;
static char *isph3a_dfs_offs;

static const struct dfs_label_type isph3a_dfs_lables[] = {
	{ "### H3A_PID               = ", ISPH3A_PID        },
	{ "### H3A_PCR               = ", ISPH3A_PCR        },
	{ "### H3A_AFPAX1            = ", ISPH3A_AFPAX1     },
	{ "### H3A_AFPAX2            = ", ISPH3A_AFPAX2     },
	{ "### H3A_AFPAXSTART        = ", ISPH3A_AFPAXSTART },
	{ "### H3A_AFIIRSH           = ", ISPH3A_AFIIRSH    },
	{ "### H3A_AFBUFST           = ", ISPH3A_AFBUFST    },
	{ "### H3A_AFCOEF010         = ", ISPH3A_AFCOEF010  },
	{ "### H3A_AFCOEF032         = ", ISPH3A_AFCOEF032  },
	{ "### H3A_AFCOEF054         = ", ISPH3A_AFCOEF054  },
	{ "### H3A_AFCOEF076         = ", ISPH3A_AFCOEF076  },
	{ "### H3A_AFCOEF098         = ", ISPH3A_AFCOEF098  },
	{ "### H3A_AFCOEF0010        = ", ISPH3A_AFCOEF0010 },
	{ "### H3A_AFCOEF110         = ", ISPH3A_AFCOEF110  },
	{ "### H3A_AFCOEF132         = ", ISPH3A_AFCOEF132  },
	{ "### H3A_AFCOEF154         = ", ISPH3A_AFCOEF154  },
	{ "### H3A_AFCOEF176         = ", ISPH3A_AFCOEF176  },
	{ "### H3A_AFCOEF198         = ", ISPH3A_AFCOEF198  },
	{ "### H3A_AFCOEF1010        = ", ISPH3A_AFCOEF1010 },
	{ "### H3A_AEWWIN1           = ", ISPH3A_AEWWIN1    },
	{ "### H3A_AEWINSTART        = ", ISPH3A_AEWINSTART },
	{ "### H3A_AEWINBLK          = ", ISPH3A_AEWINBLK   },
	{ "### H3A_AEWSUBWIN         = ", ISPH3A_AEWSUBWIN  },
	{ "### H3A_AEWBUFST          = ", ISPH3A_AEWBUFST   },
};

void isph3a_dfs_dump(struct isp_device *isp)
{
	u32 idx;

	if (isph3a_dfs_buff != NULL) {
		isph3a_dfs_offs = isph3a_dfs_buff;
		for (idx = 0; idx < ARRAY_SIZE(isph3a_dfs_lables); idx++) {
			isph3a_dfs_offs += sprintf(isph3a_dfs_offs, "%s",
					isph3a_dfs_lables[idx].ascii_reg);
			isph3a_dfs_offs += sprintf(isph3a_dfs_offs, "0x%08X\n"
					, isp_reg_readl(isp->dev,
							OMAP3_ISP_IOMEM_H3A,
							isph3a_dfs_lables[idx]\
							.reg_offset));
		}
	}
}

static ssize_t isph3a_dfs_read(struct file *file, char __user *buf,
				size_t count, loff_t *ppos)
{
	if (isph3a_dfs_buff && isph3a_dfs_buff < isph3a_dfs_offs) {
		return simple_read_from_buffer(buf, count, ppos,
					       isph3a_dfs_buff,
					       isph3a_dfs_offs -
					       isph3a_dfs_buff);
	}
	return 0;
}

static const struct file_operations isph3a_dfs_ops = {
	.owner		= THIS_MODULE,
	.read		= isph3a_dfs_read
};

void isph3a_dfs_setup(struct isp_device *isp)
{
	struct dentry *dfs_root = isp_get_dfs_root();
	unsigned dfs_tbl_size = 0;

	if (isp && dfs_root) {
		isp->dfs_h3a = debugfs_create_file("isph3a_regs", S_IRUGO,
					dfs_root, isp, &isph3a_dfs_ops);
		if (IS_ERR(isp->dfs_h3a)) {
			isp->dfs_h3a = NULL;
			dev_err(isp->dev, "Can't create DFS isph3a files !");
			return;
		}
		/* Calculate total size of DFS table */
		dfs_tbl_size =
			ARRAY_SIZE(isph3a_dfs_lables) * MAX_DFS_LABEL_LEN +
			ARRAY_SIZE(isph3a_dfs_lables) * MAX_DFS_LABEL_VAL;
		if (dfs_tbl_size <= PAGE_SIZE) {
			isph3a_dfs_buff = kmalloc(PAGE_SIZE, GFP_KERNEL);
			isph3a_dfs_offs = isph3a_dfs_buff;
		} else {
			if (dfs_root) {
				debugfs_remove(isp->dfs_h3a);
				isp->dfs_h3a = NULL;
				dev_err(isp->dev, "To large DFS table: [%d]",
					dfs_tbl_size);
			}
			isph3a_dfs_buff = NULL;
			isph3a_dfs_offs = isph3a_dfs_buff;
		}
	}
}

void isph3a_dfs_shutdown(struct isp_device *isp)
{
	struct dentry *dfs_root = isp_get_dfs_root();

	if (dfs_root)
		debugfs_remove(isp->dfs_h3a);

	kfree(isph3a_dfs_buff);
	isph3a_dfs_buff = NULL;
	isph3a_dfs_offs = isph3a_dfs_buff;
}
