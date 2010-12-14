/*
 * isppreview_dfs.c
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
#include "isppreview.h"
#include "isppreview_dfs.h"

/*
 * Debugfs stuff.
 */
static char *ispprev_dfs_buff;
static char *ispprev_dfs_offs;

static const struct dfs_label_type ispprev_dfs_lables[] = {
	{ "### PERV_PCR              = ", ISPPRV_PCR          },
	{ "### PREV_HORZ_INFO        = ", ISPPRV_HORZ_INFO    },
	{ "### PREV_VERT_INFO        = ", ISPPRV_VERT_INFO    },
	{ "### PREV_RSDR_ADDR        = ", ISPPRV_RSDR_ADDR    },
	{ "### PREV_RADR_OFFSET      = ", ISPPRV_RADR_OFFSET  },
	{ "### PREV_DSDR_ADDR        = ", ISPPRV_DSDR_ADDR    },
	{ "### PREV_DRKF_OFFSET      = ", ISPPRV_DRKF_OFFSET  },
	{ "### PREV_WSDR_ADDR        = ", ISPPRV_WSDR_ADDR    },
	{ "### PREV_WADD_OFFSET      = ", ISPPRV_WADD_OFFSET  },
	{ "### PREV_AVE              = ", ISPPRV_AVE          },
	{ "### PREV_HMED             = ", ISPPRV_HMED         },
	{ "### PREV_NF               = ", ISPPRV_NF           },
	{ "### PREV_WB_DGAIN         = ", ISPPRV_WB_DGAIN     },
	{ "### PREV_WBGAIN           = ", ISPPRV_WBGAIN       },
	{ "### PREV_WBSEL            = ", ISPPRV_WBSEL        },
	{ "### PREV_CFA              = ", ISPPRV_CFA          },
	{ "### PREV_BLKADJOFF        = ", ISPPRV_BLKADJOFF    },
	{ "### PREV_RGB_MAT1         = ", ISPPRV_RGB_MAT1     },
	{ "### PREV_RGB_MAT2         = ", ISPPRV_RGB_MAT2     },
	{ "### PREV_RGB_MAT3         = ", ISPPRV_RGB_MAT3     },
	{ "### PREV_RGB_MAT4         = ", ISPPRV_RGB_MAT4     },
	{ "### PREV_RGB_MAT5         = ", ISPPRV_RGB_MAT5     },
	{ "### PREV_RGB_OFF1         = ", ISPPRV_RGB_OFF1     },
	{ "### PREV_RGB_OFF2         = ", ISPPRV_RGB_OFF2     },
	{ "### PREV_CSC0             = ", ISPPRV_CSC0         },
	{ "### PREV_CSC1             = ", ISPPRV_CSC1         },
	{ "### PREV_CSC2             = ", ISPPRV_CSC2         },
	{ "### PREV_CSC_OFFSET       = ", ISPPRV_CSC_OFFSET   },
	{ "### PREV_CNT_BRT          = ", ISPPRV_CNT_BRT      },
	{ "### PREV_CSUP             = ", ISPPRV_CSUP         },
	{ "### PREV_SETUP_YC         = ", ISPPRV_SETUP_YC     },
	{ "### PREV_SET_TBL_ADDR     = ", ISPPRV_SET_TBL_ADDR },
	{ "### PREV_SET_TBL_DATA     = ", ISPPRV_SET_TBL_DATA },
	{ "### PREV_CDC_THR0         = ", ISPPRV_CDC_THR0     },
	{ "### PREV_CDC_THR1         = ", ISPPRV_CDC_THR1     },
	{ "### PREV_CDC_THR2         = ", ISPPRV_CDC_THR2     },
	{ "### PREV_CDC_THR3         = ", ISPPRV_CDC_THR3     },
};

void ispprev_dfs_dump(struct isp_device *isp)
{
	u32 idx;

	if (ispprev_dfs_buff != NULL) {
		ispprev_dfs_offs = ispprev_dfs_buff;
		for (idx = 0; idx < ARRAY_SIZE(ispprev_dfs_lables); idx++) {
			ispprev_dfs_offs += sprintf(ispprev_dfs_offs, "%s",
					ispprev_dfs_lables[idx].ascii_reg);
			ispprev_dfs_offs += sprintf(ispprev_dfs_offs, "0x%08X\n"
					, isp_reg_readl(isp->dev,
							OMAP3_ISP_IOMEM_RESZ,
							ispprev_dfs_lables[idx]\
							.reg_offset));
		}
	}
}

static ssize_t ispprev_dfs_read(struct file *file, char __user *buf,
				size_t count, loff_t *ppos)
{
	if (ispprev_dfs_buff && ispprev_dfs_buff < ispprev_dfs_offs) {
		return simple_read_from_buffer(buf, count, ppos,
					       ispprev_dfs_buff,
					       ispprev_dfs_offs -
					       ispprev_dfs_buff);
	}
	return 0;
}

static const struct file_operations ispprev_dfs_ops = {
	.owner		= THIS_MODULE,
	.read		= ispprev_dfs_read
};

void ispprev_dfs_setup(struct isp_device *isp)
{
	struct dentry *dfs_root = isp_get_dfs_root();
	unsigned dfs_tbl_size = 0;

	if (isp && dfs_root) {
		isp->dfs_prev = debugfs_create_file("ispprev_regs", S_IRUGO,
					dfs_root, isp, &ispprev_dfs_ops);
		if (IS_ERR(isp->dfs_prev)) {
			isp->dfs_prev = NULL;
			dev_err(isp->dev, "Can't create DFS ispprev files !");
			return;
		}
		/* Calculate total size of DFS table */
		dfs_tbl_size =
			ARRAY_SIZE(ispprev_dfs_lables) * MAX_DFS_LABEL_LEN +
			ARRAY_SIZE(ispprev_dfs_lables) * MAX_DFS_LABEL_VAL;
		if (dfs_tbl_size <= PAGE_SIZE) {
			ispprev_dfs_buff = kmalloc(PAGE_SIZE, GFP_KERNEL);
			ispprev_dfs_offs = ispprev_dfs_buff;
		} else {
			if (dfs_root) {
				debugfs_remove(isp->dfs_prev);
				isp->dfs_prev = NULL;
				dev_err(isp->dev, "To large DFS table: [%d]",
					dfs_tbl_size);
			}
			ispprev_dfs_buff = NULL;
			ispprev_dfs_offs = ispprev_dfs_buff;
		}
	}
}

void ispprev_dfs_shutdown(struct isp_device *isp)
{
	struct dentry *dfs_root = isp_get_dfs_root();

	if (dfs_root)
		debugfs_remove(isp->dfs_prev);

	kfree(ispprev_dfs_buff);
	ispprev_dfs_buff = NULL;
	ispprev_dfs_offs = ispprev_dfs_buff;
}
