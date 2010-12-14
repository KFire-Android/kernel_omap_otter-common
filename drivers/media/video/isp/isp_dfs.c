/*
 * isp_dfs.c
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

#include <media/v4l2-common.h>
#include <linux/debugfs.h>
#include <linux/slab.h>

#include "isp.h"
#include "ispreg.h"
#include "isp_dfs.h"

/*
 * Debugfs stuff.
 */
static struct dentry *isp_dfs_root;
static char *isp_dfs_buff;
static char *isp_dfs_offs;

static const struct dfs_label_type isp_dfs_lables[] = {
	{ "### ISP_REVISION          = ", ISP_REVISION            },
	{ "### ISP_SYSCONFIG         = ", ISP_SYSCONFIG           },
	{ "### ISP_SYSSTATUS         = ", ISP_SYSSTATUS           },
	{ "### ISP_IRQ0ENABLE        = ", ISP_IRQ0ENABLE          },
	{ "### ISP_IRQ0STATUS        = ", ISP_IRQ0STATUS          },
	{ "### ISP_IRQ1ENABLE        = ", ISP_IRQ1ENABLE          },
	{ "### ISP_IRQ1STATUS        = ", ISP_IRQ1STATUS          },
	{ "### ISP_TCTRL_GRESET_LEN  = ", ISP_TCTRL_GRESET_LENGTH },
	{ "### ISP_TCTRL_PSTRB_REPLY = ", ISP_TCTRL_PSTRB_REPLAY  },
	{ "### ISP_CTRL              = ", ISP_CTRL                },
	{ "### ISP_SECURE            = ", ISP_SECURE              },
	{ "### ISP_TCTRL_CTRL        = ", ISP_TCTRL_CTRL          },
	{ "### ISP_TCTRL_FRAME       = ", ISP_TCTRL_FRAME         },
	{ "### ISP_TCTRL_PSTRB_DELAY = ", ISP_TCTRL_PSTRB_DELAY   },
	{ "### ISP_TCTRL_STRB_DELAY  = ", ISP_TCTRL_STRB_DELAY    },
	{ "### ISP_TCTRL_SHUT_DELAY  = ", ISP_TCTRL_SHUT_DELAY    },
	{ "### ISP_TCTRL_PSTRB_LEN   = ", ISP_TCTRL_PSTRB_LENGTH  },
	{ "### ISP_TCTRL_STRB_LEN    = ", ISP_TCTRL_STRB_LENGTH   },
	{ "### ISP_TCTRL_SHUT_LEN    = ", ISP_TCTRL_SHUT_LENGTH   },
	{ "### ISP_PING_PONG_ADDR    = ", ISP_PING_PONG_ADDR      },
	{ "### ISP_PING_PONG_MRANGE  = ", ISP_PING_PONG_MEM_RANGE },
	{ "### ISP_PING_PONG_BUFSIZE = ", ISP_PING_PONG_BUF_SIZE  }
};

/**
 * isp_get_dfs_root - Return debug file system root entry.
 */
struct dentry *isp_get_dfs_root(void)
{
	return isp_dfs_root;
}
EXPORT_SYMBOL(isp_get_dfs_root);

void isp_dfs_dump(struct isp_device *isp)
{
	u32 idx;

	if (isp_dfs_buff != NULL) {
		isp_dfs_offs = isp_dfs_buff;
		for (idx = 0; idx < ARRAY_SIZE(isp_dfs_lables); idx++) {
			isp_dfs_offs += sprintf(isp_dfs_offs, "%s",
					isp_dfs_lables[idx].ascii_reg);
			isp_dfs_offs += sprintf(isp_dfs_offs, "0x%08X\n",
					isp_reg_readl(isp->dev,
						      OMAP3_ISP_IOMEM_MAIN,
						      isp_dfs_lables[idx]\
						      .reg_offset));
		}
	}
}

static ssize_t isp_dfs_read(struct file *file, char __user *buf, size_t count,
			    loff_t *ppos)
{
	if (isp_dfs_buff && isp_dfs_buff < isp_dfs_offs)
		return simple_read_from_buffer(buf, count, ppos, isp_dfs_buff,
					       isp_dfs_offs - isp_dfs_buff);

	return 0;
}

static const struct file_operations isp_dfs_ops = {
	.owner		= THIS_MODULE,
	.read		= isp_dfs_read
};

void isp_dfs_setup(struct isp_device *isp)
{
	unsigned dfs_tbl_size = 0;

	isp_dfs_root = debugfs_create_dir("isp", NULL);
	if (IS_ERR(isp_dfs_root)) {
		isp_dfs_root = NULL;
		return;
	}
	if (isp)
		isp->dfs_isp = debugfs_create_file("isp_regs", S_IRUGO,
					isp_dfs_root, isp, &isp_dfs_ops);

	if (IS_ERR(isp->dfs_isp)) {
		isp->dfs_isp = NULL;
		dev_err(isp->dev, "Can't create DFS isp files !");
		return;
	}

	/* Calculate total size of DFS table */
	dfs_tbl_size =
		ARRAY_SIZE(isp_dfs_lables) * MAX_DFS_LABEL_LEN +
		ARRAY_SIZE(isp_dfs_lables) * MAX_DFS_LABEL_VAL;
	if (dfs_tbl_size <= PAGE_SIZE) {
		isp_dfs_buff = kmalloc(PAGE_SIZE, GFP_KERNEL);
		isp_dfs_offs = isp_dfs_buff;
	} else {
		if (isp->dfs_isp) {
			debugfs_remove(isp->dfs_isp);
			isp->dfs_isp = NULL;
			dev_err(isp->dev, "To large DFS table: [%d]",
				dfs_tbl_size);
		}
		isp_dfs_buff = NULL;
		isp_dfs_offs = isp_dfs_buff;
	}
}

void isp_dfs_shutdown(void)
{
	if (isp_dfs_root) {
		debugfs_remove_recursive(isp_dfs_root);
		isp_dfs_root = NULL;
	}
	kfree(isp_dfs_buff);
	isp_dfs_buff = NULL;
	isp_dfs_offs = isp_dfs_buff;
}
