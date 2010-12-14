/*
 * ispresizer_dfs.c
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
#include "ispresizer.h"
#include "ispresizer_dfs.h"

/*
 * Debugfs stuff.
 */
static char *ispresz_dfs_buff;
static char *ispresz_dfs_offs;

static const struct dfs_label_type ispresz_dfs_lables[] = {
	{ "### RSZ_PID               = ", ISPRSZ_PID        },
	{ "### RSZ_PCR               = ", ISPRSZ_PCR        },
	{ "### RESZ_CNT              = ", ISPRSZ_CNT        },
	{ "### RESZ_OUT_SIZE         = ", ISPRSZ_OUT_SIZE   },
	{ "### RESZ_IN_START         = ", ISPRSZ_IN_START   },
	{ "### RESZ_IN_SIZE          = ", ISPRSZ_IN_SIZE    },
	{ "### RESZ_SDR_INADD        = ", ISPRSZ_SDR_INADD  },
	{ "### RESZ_SDR_INOFF        = ", ISPRSZ_SDR_INOFF  },
	{ "### RESZ_SDR_OUTADD       = ", ISPRSZ_SDR_OUTADD },
	{ "### RESZ_SDR_OUTOFF       = ", ISPRSZ_SDR_OUTOFF },
	{ "### RESZ_HFILT10          = ", ISPRSZ_HFILT10    },
	{ "### RESZ_HFILT32          = ", ISPRSZ_HFILT32    },
	{ "### RESZ_HFILT54          = ", ISPRSZ_HFILT54    },
	{ "### RESZ_HFILT76          = ", ISPRSZ_HFILT76    },
	{ "### RESZ_HFILT98          = ", ISPRSZ_HFILT98    },
	{ "### RESZ_HFILT1110        = ", ISPRSZ_HFILT1110  },
	{ "### RESZ_HFILT1312        = ", ISPRSZ_HFILT1312  },
	{ "### RESZ_HFILT1514        = ", ISPRSZ_HFILT1514  },
	{ "### RESZ_HFILT1716        = ", ISPRSZ_HFILT1716  },
	{ "### RESZ_HFILT1918        = ", ISPRSZ_HFILT1918  },
	{ "### RESZ_HFILT2120        = ", ISPRSZ_HFILT2120  },
	{ "### RESZ_HFILT2322        = ", ISPRSZ_HFILT2322  },
	{ "### RESZ_HFILT2524        = ", ISPRSZ_HFILT2524  },
	{ "### RESZ_HFILT2726        = ", ISPRSZ_HFILT2726  },
	{ "### RESZ_HFILT2928        = ", ISPRSZ_HFILT2928  },
	{ "### RESZ_HFILT3130        = ", ISPRSZ_HFILT3130  },
	{ "### RESZ_VFILT10          = ", ISPRSZ_VFILT10    },
	{ "### RESZ_VFILT32          = ", ISPRSZ_VFILT32    },
	{ "### RESZ_VFILT54          = ", ISPRSZ_VFILT54    },
	{ "### RESZ_VFILT76          = ", ISPRSZ_VFILT76    },
	{ "### RESZ_VFILT98          = ", ISPRSZ_VFILT98    },
	{ "### RESZ_VFILT1110        = ", ISPRSZ_VFILT1110  },
	{ "### RESZ_VFILT1312        = ", ISPRSZ_VFILT1312  },
	{ "### RESZ_VFILT1514        = ", ISPRSZ_VFILT1514  },
	{ "### RESZ_VFILT1716        = ", ISPRSZ_VFILT1716  },
	{ "### RESZ_VFILT1918        = ", ISPRSZ_VFILT1918  },
	{ "### RESZ_VFILT2120        = ", ISPRSZ_VFILT2120  },
	{ "### RESZ_VFILT2322        = ", ISPRSZ_VFILT2322  },
	{ "### RESZ_VFILT2524        = ", ISPRSZ_VFILT2524  },
	{ "### RESZ_VFILT2726        = ", ISPRSZ_VFILT2726  },
	{ "### RESZ_VFILT2928        = ", ISPRSZ_VFILT2928  },
	{ "### RESZ_VFILT3130        = ", ISPRSZ_VFILT3130  },
	{ "### RESZ_YENH             = ", ISPRSZ_YENH       },
};

void ispresz_dfs_dump(struct isp_device *isp)
{
	u32 idx;

	if (ispresz_dfs_buff != NULL) {
		ispresz_dfs_offs = ispresz_dfs_buff;
		for (idx = 0; idx < ARRAY_SIZE(ispresz_dfs_lables); idx++) {
			ispresz_dfs_offs += sprintf(ispresz_dfs_offs, "%s",
					ispresz_dfs_lables[idx].ascii_reg);
			ispresz_dfs_offs += sprintf(ispresz_dfs_offs, "0x%08X\n"
					, isp_reg_readl(isp->dev,
							OMAP3_ISP_IOMEM_RESZ,
							ispresz_dfs_lables[idx]\
							.reg_offset));
		}
	}
}

static ssize_t ispresz_dfs_read(struct file *file, char __user *buf,
				size_t count, loff_t *ppos)
{
	if (ispresz_dfs_buff && ispresz_dfs_buff < ispresz_dfs_offs) {
		return simple_read_from_buffer(buf, count, ppos,
					       ispresz_dfs_buff,
					       ispresz_dfs_offs -
					       ispresz_dfs_buff);
	}
	return 0;
}

static const struct file_operations ispresz_dfs_ops = {
	.owner		= THIS_MODULE,
	.read		= ispresz_dfs_read
};

void ispresz_dfs_setup(struct isp_device *isp)
{
	struct dentry *dfs_root = isp_get_dfs_root();
	unsigned dfs_tbl_size = 0;

	if (isp && dfs_root) {
		isp->dfs_resz = debugfs_create_file("ispresz_regs", S_IRUGO,
					dfs_root, isp, &ispresz_dfs_ops);
		if (IS_ERR(isp->dfs_resz)) {
			isp->dfs_resz = NULL;
			dev_err(isp->dev, "Can't create DFS ispresz files !");
			return;
		}
		/* Calculate total size of DFS table */
		dfs_tbl_size =
			ARRAY_SIZE(ispresz_dfs_lables) * MAX_DFS_LABEL_LEN +
			ARRAY_SIZE(ispresz_dfs_lables) * MAX_DFS_LABEL_VAL;
		if (dfs_tbl_size <= PAGE_SIZE) {
			ispresz_dfs_buff = kmalloc(PAGE_SIZE, GFP_KERNEL);
			ispresz_dfs_offs = ispresz_dfs_buff;
		} else {
			if (dfs_root) {
				debugfs_remove(isp->dfs_resz);
				isp->dfs_resz = NULL;
				dev_err(isp->dev, "To large DFS table: [%d]",
					dfs_tbl_size);
			}
			ispresz_dfs_buff = NULL;
			ispresz_dfs_offs = ispresz_dfs_buff;
		}
	}
}

void ispresz_dfs_shutdown(struct isp_device *isp)
{
	struct dentry *dfs_root = isp_get_dfs_root();

	if (dfs_root)
		debugfs_remove(isp->dfs_resz);

	kfree(ispresz_dfs_buff);
	ispresz_dfs_buff = NULL;
	ispresz_dfs_offs = ispresz_dfs_buff;
}
