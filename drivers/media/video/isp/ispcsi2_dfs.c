/*
 * ispcsi2_dfs.c
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
#include "ispcsi2.h"
#include "ispcsi2_dfs.h"

/*
 * Debugfs stuff.
 */
static char *ispcsi2_dfs_buff;
static char *ispcsi2_dfs_offs;

static const struct dfs_label_type ispcsi2_dfs_lables[] = {
	{ "### CSI2_REVISION         = ", ISPCSI2_REVISION             },
	{ "### CSI2_SYSCONFIG        = ", ISPCSI2_SYSCONFIG            },
	{ "### CSI2_SYSSTATUS        = ", ISPCSI2_SYSSTATUS            },
	{ "### CSI2_IRQSTATUS        = ", ISPCSI2_IRQSTATUS            },
	{ "### CSI2_IRQENABLE        = ", ISPCSI2_IRQENABLE            },
	{ "### CSI2_CTRL             = ", ISPCSI2_CTRL                 },
	{ "### CSI2_DBG_H            = ", ISPCSI2_DBG_H                },
	{ "### CSI2_GNQ              = ", ISPCSI2_GNQ                  },
	{ "### CSI2_CMPXIO_CFG1      = ", ISPCSI2_COMPLEXIO_CFG1       },
	{ "### CSI2_CMPXIO1_IRQSTAT  = ", ISPCSI2_COMPLEXIO1_IRQSTATUS },
	{ "### CSI2_SHORT_PACKET     = ", ISPCSI2_SHORT_PACKET         },
	{ "### CSI2_CMPXIO1_IRQENBL  = ", ISPCSI2_COMPLEXIO1_IRQENABLE },
	{ "### CSI2_DBG_P            = ", ISPCSI2_DBG_P                },
	{ "### CSI2_TIMING           = ", ISPCSI2_TIMING               },

	{ "... CSI2_CTX_CTRL1[0]     = ", ISPCSI2_CTX_CTRL1(0)         },
	{ "... CSI2_CTX_CTRL2[0]     = ", ISPCSI2_CTX_CTRL2(0)         },
	{ "... CSI2_CTX_DT_OFST[0]   = ", ISPCSI2_CTX_DAT_OFST(0)      },
	{ "... CSI2_CTX_DPINGADDR[0] = ", ISPCSI2_CTX_DAT_PING_ADDR(0) },
	{ "... CSI2_CTX_DPONGADDR[0] = ", ISPCSI2_CTX_DAT_PONG_ADDR(0) },
	{ "... CSI2_CTX_IRQENABLE[0] = ", ISPCSI2_CTX_IRQENABLE(0)     },
	{ "... CSI2_CTX_IRQSTATUS[0] = ", ISPCSI2_CTX_IRQSTATUS(0)     },
	{ "... CSI2_CTX_CTRL3[0]     = ", ISPCSI2_CTX_CTRL3(0)         },

	{ "... CSI2_CTX_CTRL1[1]     = ", ISPCSI2_CTX_CTRL1(1)	       },
	{ "... CSI2_CTX_CTRL2[1]     = ", ISPCSI2_CTX_CTRL2(1)	       },
	{ "... CSI2_CTX_DT_OFST[1]   = ", ISPCSI2_CTX_DAT_OFST(1)      },
	{ "... CSI2_CTX_DPINGADDR[1] = ", ISPCSI2_CTX_DAT_PING_ADDR(1) },
	{ "... CSI2_CTX_DPONGADDR[1] = ", ISPCSI2_CTX_DAT_PONG_ADDR(1) },
	{ "... CSI2_CTX_IRQENABLE[1] = ", ISPCSI2_CTX_IRQENABLE(1)     },
	{ "... CSI2_CTX_IRQSTATUS[1] = ", ISPCSI2_CTX_IRQSTATUS(1)     },
	{ "... CSI2_CTX_CTRL3[1]     = ", ISPCSI2_CTX_CTRL3(1)	       },
};

void ispcsi2_dfs_dump(struct isp_device *isp)
{
	u32 idx;

	if (ispcsi2_dfs_buff != NULL) {
		ispcsi2_dfs_offs = ispcsi2_dfs_buff;
		for (idx = 0; idx < ARRAY_SIZE(ispcsi2_dfs_lables); idx++) {
			ispcsi2_dfs_offs += sprintf(ispcsi2_dfs_offs, "%s",
					ispcsi2_dfs_lables[idx].ascii_reg);
			ispcsi2_dfs_offs += sprintf(ispcsi2_dfs_offs, "0x%08X\n"
					, isp_reg_readl(isp->dev,
							OMAP3_ISP_IOMEM_CSI2A,
							ispcsi2_dfs_lables[idx]\
							.reg_offset));
		}
	}
}

static ssize_t ispcsi2_dfs_read(struct file *file, char __user *buf,
				size_t count, loff_t *ppos)
{
	if (ispcsi2_dfs_buff && ispcsi2_dfs_buff < ispcsi2_dfs_offs) {
		return simple_read_from_buffer(buf, count, ppos,
					       ispcsi2_dfs_buff,
					       ispcsi2_dfs_offs -
					       ispcsi2_dfs_buff);
	}
	return 0;
}

static const struct file_operations ispcsi2_dfs_ops = {
	.owner		= THIS_MODULE,
	.read		= ispcsi2_dfs_read
};

void ispcsi2_dfs_setup(struct isp_device *isp)
{
	struct dentry *dfs_root = isp_get_dfs_root();
	unsigned dfs_tbl_size = 0;

	if (isp && dfs_root) {
		isp->dfs_csi2 = debugfs_create_file("ispcsi2_regs", S_IRUGO,
					dfs_root, isp, &ispcsi2_dfs_ops);
		if (IS_ERR(isp->dfs_csi2)) {
			isp->dfs_csi2 = NULL;
			dev_err(isp->dev, "Can't create DFS ispcsi2 files !");
			return;
		}
		/* Calculate total size of DFS table */
		dfs_tbl_size =
			ARRAY_SIZE(ispcsi2_dfs_lables) * MAX_DFS_LABEL_LEN +
			ARRAY_SIZE(ispcsi2_dfs_lables) * MAX_DFS_LABEL_VAL;
		if (dfs_tbl_size <= PAGE_SIZE) {
			ispcsi2_dfs_buff = kmalloc(PAGE_SIZE, GFP_KERNEL);
			ispcsi2_dfs_offs = ispcsi2_dfs_buff;
		} else {
			if (dfs_root) {
				debugfs_remove(isp->dfs_csi2);
				isp->dfs_csi2 = NULL;
				dev_err(isp->dev, "To large DFS table: [%d]",
					dfs_tbl_size);
			}
			ispcsi2_dfs_buff = NULL;
			ispcsi2_dfs_offs = ispcsi2_dfs_buff;
		}
	}
}

void ispcsi2_dfs_shutdown(struct isp_device *isp)
{
	struct dentry *dfs_root = isp_get_dfs_root();

	if (dfs_root)
		debugfs_remove(isp->dfs_csi2);

	kfree(ispcsi2_dfs_buff);
	ispcsi2_dfs_buff = NULL;
	ispcsi2_dfs_offs = ispcsi2_dfs_buff;
}
