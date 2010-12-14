/*
 * ispcsi1_dfs.c
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
#include "isp_csi.h"
#include "ispcsi1_dfs.h"

/*
 * Debugfs stuff.
 */
static char *ispcsi1_dfs_buff;
static char *ispcsi1_dfs_offs;

static const struct dfs_label_type ispcsi1_dfs_lables[] = {
	{ "### CSI1_REVISION         = ", ISPCSI1_REVISION       },
	{ "### CSI1_SYSCONFIG        = ", ISPCSI1_SYSCONFIG      },
	{ "### CSI1_SYSSTATUS        = ", ISPCSI1_SYSSTATUS      },
	{ "### CSI1_LC01_IRQENABLE   = ", ISPCSI1_LC01_IRQENABLE },
	{ "### CSI1_LC01_IRQSTATUS   = ", ISPCSI1_LC01_IRQSTATUS },
	{ "### CSI1_LC23_IRQENABLE   = ", ISPCSI1_LC23_IRQENABLE },
	{ "### CSI1_LC23_IRQSTATUS   = ", ISPCSI1_LC23_IRQSTATUS },
	{ "### CSI1_LCM_IRQENABLE    = ", ISPCSI1_LCM_IRQENABLE  },
	{ "### CSI1_LCM_IRQSTATUS    = ", ISPCSI1_LCM_IRQSTATUS  },
	{ "### CSI1_CTRL             = ", ISPCSI1_CTRL           },
	{ "### CSI1_DBG              = ", ISPCSI1_DBG            },
	{ "### CSI1_GNQ              = ", ISPCSI1_GNQ            },
	{ "### CSI1_CTRL1            = ", ISPCSI1_CTRL1          },
	{ "### CSI1_LCM_CTRL         = ", ISPCSI1_LCM_CTRL       },
	{ "### CSI1_LCM_VSIZE        = ", ISPCSI1_LCM_VSIZE      },
	{ "### CSI1_LCM_HSIZE        = ", ISPCSI1_LCM_HSIZE      },
	{ "### CSI1_LCM_PREFETCH     = ", ISPCSI1_LCM_PREFETCH   },
	{ "### CSI1_LCM_SRC_ADDR     = ", ISPCSI1_LCM_SRC_ADDR   },
	{ "### CSI1_LCM_SRC_OFST     = ", ISPCSI1_LCM_SRC_OFST   },
	{ "### CSI1_LCM_DST_ADDR     = ", ISPCSI1_LCM_DST_ADDR   },
	{ "### CSI1_LCM_DST_OFST     = ", ISPCSI1_LCM_DST_OFST   },
	{ "### CSIB_SYSCONFIG        = ", ISP_CSIB_SYSCONFIG     },
	{ "### CSIA_SYSCONFIG        = ", ISP_CSIA_SYSCONFIG     },

	{ "... LCx_CTRL[0]           = ", ISPCSI1_LCx_CTRL(0)          },
	{ "... LCx_CODE[0]           = ", ISPCSI1_LCx_CODE(0)          },
	{ "... LCx_STAT_START[0]     = ", ISPCSI1_LCx_STAT_START(0)    },
	{ "... LCx_STAT_SIZE[0]      = ", ISPCSI1_LCx_STAT_SIZE(0)     },
	{ "... LCx_SOF_ADDR[0]       = ", ISPCSI1_LCx_SOF_ADDR(0)      },
	{ "... LCx_EOF_ADDR[0]       = ", ISPCSI1_LCx_EOF_ADDR(0)      },
	{ "... LCx_DAT_START[0]      = ", ISPCSI1_LCx_DAT_START(0)     },
	{ "... LCx_DAT_SIZE[0]       = ", ISPCSI1_LCx_DAT_SIZE(0)      },
	{ "... LCx_DAT_PING_ADDR[0]  = ", ISPCSI1_LCx_DAT_PING_ADDR(0) },
	{ "... LCx_DAT_PONG_ADDR[0]  = ", ISPCSI1_LCx_DAT_PONG_ADDR(0) },
	{ "... LCx_DAT_OFST[0]       = ", ISPCSI1_LCx_DAT_OFST(0)      },

	{ "... LCx_CTRL[1]           = ", ISPCSI1_LCx_CTRL(1)	       },
	{ "... LCx_CODE[1]           = ", ISPCSI1_LCx_CODE(1)	       },
	{ "... LCx_STAT_START[1]     = ", ISPCSI1_LCx_STAT_START(1)    },
	{ "... LCx_STAT_SIZE[1]      = ", ISPCSI1_LCx_STAT_SIZE(1)     },
	{ "... LCx_SOF_ADDR[1]       = ", ISPCSI1_LCx_SOF_ADDR(1)      },
	{ "... LCx_EOF_ADDR[1]       = ", ISPCSI1_LCx_EOF_ADDR(1)      },
	{ "... LCx_DAT_START[1]      = ", ISPCSI1_LCx_DAT_START(1)     },
	{ "... LCx_DAT_SIZE[1]       = ", ISPCSI1_LCx_DAT_SIZE(1)      },
	{ "... LCx_DAT_PING_ADDR[1]  = ", ISPCSI1_LCx_DAT_PING_ADDR(1) },
	{ "... LCx_DAT_PONG_ADDR[1]  = ", ISPCSI1_LCx_DAT_PONG_ADDR(1) },
	{ "... LCx_DAT_OFST[1]       = ", ISPCSI1_LCx_DAT_OFST(1)      },

	{ "... LCx_CTRL[2]           = ", ISPCSI1_LCx_CTRL(2)	       },
	{ "... LCx_CODE[2]           = ", ISPCSI1_LCx_CODE(2)	       },
	{ "... LCx_STAT_START[2]     = ", ISPCSI1_LCx_STAT_START(2)    },
	{ "... LCx_STAT_SIZE[2]      = ", ISPCSI1_LCx_STAT_SIZE(2)     },
	{ "... LCx_SOF_ADDR[2]       = ", ISPCSI1_LCx_SOF_ADDR(2)      },
	{ "... LCx_EOF_ADDR[2]       = ", ISPCSI1_LCx_EOF_ADDR(2)      },
	{ "... LCx_DAT_START[2]      = ", ISPCSI1_LCx_DAT_START(2)     },
	{ "... LCx_DAT_SIZE[2]       = ", ISPCSI1_LCx_DAT_SIZE(2)      },
	{ "... LCx_DAT_PING_ADDR[2]  = ", ISPCSI1_LCx_DAT_PING_ADDR(2) },
	{ "... LCx_DAT_PONG_ADDR[2]  = ", ISPCSI1_LCx_DAT_PONG_ADDR(2) },
	{ "... LCx_DAT_OFST[2]       = ", ISPCSI1_LCx_DAT_OFST(2)      },

	{ "... LCx_CTRL[3]           = ", ISPCSI1_LCx_CTRL(3)	       },
	{ "... LCx_CODE[3]           = ", ISPCSI1_LCx_CODE(3)	       },
	{ "... LCx_STAT_START[3]     = ", ISPCSI1_LCx_STAT_START(3)    },
	{ "... LCx_STAT_SIZE[3]      = ", ISPCSI1_LCx_STAT_SIZE(3)     },
	{ "... LCx_SOF_ADDR[3]       = ", ISPCSI1_LCx_SOF_ADDR(3)      },
	{ "... LCx_EOF_ADDR[3]       = ", ISPCSI1_LCx_EOF_ADDR(3)      },
	{ "... LCx_DAT_START[3]      = ", ISPCSI1_LCx_DAT_START(3)     },
	{ "... LCx_DAT_SIZE[3]       = ", ISPCSI1_LCx_DAT_SIZE(3)      },
	{ "... LCx_DAT_PING_ADDR[3]  = ", ISPCSI1_LCx_DAT_PING_ADDR(3) },
	{ "... LCx_DAT_PONG_ADDR[3]  = ", ISPCSI1_LCx_DAT_PONG_ADDR(3) },
	{ "... LCx_DAT_OFST[3]       = ", ISPCSI1_LCx_DAT_OFST(3)      },
};

void ispcsi1_dfs_dump(struct isp_device *isp)
{
	u32 idx;

	if (ispcsi1_dfs_buff != NULL) {
		ispcsi1_dfs_offs = ispcsi1_dfs_buff;
		for (idx = 0; idx < ARRAY_SIZE(ispcsi1_dfs_lables); idx++) {
			ispcsi1_dfs_offs += sprintf(ispcsi1_dfs_offs, "%s",
					ispcsi1_dfs_lables[idx].ascii_reg);
			ispcsi1_dfs_offs += sprintf(ispcsi1_dfs_offs, "0x%08X\n"
					, isp_reg_readl(isp->dev,
							OMAP3_ISP_IOMEM_CCP2,
							ispcsi1_dfs_lables[idx]\
							.reg_offset));
		}
	}
}

static ssize_t ispcsi1_dfs_read(struct file *file, char __user *buf,
				size_t count, loff_t *ppos)
{
	if (ispcsi1_dfs_buff && ispcsi1_dfs_buff < ispcsi1_dfs_offs) {
		return simple_read_from_buffer(buf, count, ppos,
					       ispcsi1_dfs_buff,
					       ispcsi1_dfs_offs -
					       ispcsi1_dfs_buff);
	}
	return 0;
}

static const struct file_operations ispcsi1_dfs_ops = {
	.owner		= THIS_MODULE,
	.read		= ispcsi1_dfs_read
};

void ispcsi1_dfs_setup(struct isp_device *isp)
{
	struct dentry *dfs_root = isp_get_dfs_root();
	unsigned dfs_tbl_size = 0;

	if (isp && dfs_root) {
		isp->dfs_csi1 = debugfs_create_file("ispcsi1_regs", S_IRUGO,
					dfs_root, isp, &ispcsi1_dfs_ops);
		if (IS_ERR(isp->dfs_csi1)) {
			isp->dfs_csi1 = NULL;
			dev_err(isp->dev, "Can't create DFS ispcsi1 files !");
			return;
		}
		/* Calculate total size of DFS table */
		dfs_tbl_size =
			ARRAY_SIZE(ispcsi1_dfs_lables) * MAX_DFS_LABEL_LEN +
			ARRAY_SIZE(ispcsi1_dfs_lables) * MAX_DFS_LABEL_VAL;
		if (dfs_tbl_size <= PAGE_SIZE) {
			ispcsi1_dfs_buff = kmalloc(PAGE_SIZE, GFP_KERNEL);
			ispcsi1_dfs_offs = ispcsi1_dfs_buff;
		} else {
			if (dfs_root) {
				debugfs_remove(isp->dfs_csi1);
				isp->dfs_csi1 = NULL;
				dev_err(isp->dev, "To large DFS table: [%d]",
					dfs_tbl_size);
			}
			ispcsi1_dfs_buff = NULL;
			ispcsi1_dfs_offs = ispcsi1_dfs_buff;
		}
	}
}

void ispcsi1_dfs_shutdown(struct isp_device *isp)
{
	struct dentry *dfs_root = isp_get_dfs_root();

	if (dfs_root)
		debugfs_remove(isp->dfs_csi1);

	kfree(ispcsi1_dfs_buff);
	ispcsi1_dfs_buff = NULL;
	ispcsi1_dfs_offs = ispcsi1_dfs_buff;
}
