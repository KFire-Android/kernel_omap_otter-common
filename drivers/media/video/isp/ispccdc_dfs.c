/*
 * ispccdc_dfs.c
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

#include "isp.h"
#include "ispreg.h"
#include "ispccdc.h"
#include "ispccdc_dfs.h"

/*
 * Debugfs stuff.
 */
static char *ispccdc_dfs_buff;
static char *ispccdc_dfs_offs;

static const struct dfs_label_type ispccdc_dfs_lables[] = {
	{ "### CCDC_PID              = ", ISPCCDC_PID              },
	{ "### CCDC_PCR              = ", ISPCCDC_PCR              },
	{ "### CCDC_SYN_MODE         = ", ISPCCDC_SYN_MODE         },
	{ "### CCDC_HD_VD_WID        = ", ISPCCDC_HD_VD_WID        },
	{ "### CCDC_PIX_LINES        = ", ISPCCDC_PIX_LINES        },
	{ "### CCDC_HORZ_INFO        = ", ISPCCDC_HORZ_INFO        },
	{ "### CCDC_VERT_START       = ", ISPCCDC_VERT_START       },
	{ "### CCDC_VERT_LINES       = ", ISPCCDC_VERT_LINES       },
	{ "### CCDC_CULLING          = ", ISPCCDC_CULLING          },
	{ "### CCDC_HSIZE_OFF        = ", ISPCCDC_HSIZE_OFF        },
	{ "### CCDC_SDOFST           = ", ISPCCDC_SDOFST           },
	{ "### CCDC_SDR_ADDR         = ", ISPCCDC_SDR_ADDR         },
	{ "### CCDC_CLAMP            = ", ISPCCDC_CLAMP            },
	{ "### CCDC_DCSUB            = ", ISPCCDC_DCSUB            },
	{ "### CCDC_COLPTN           = ", ISPCCDC_COLPTN           },
	{ "### CCDC_BLKCMP           = ", ISPCCDC_BLKCMP           },
	{ "### CCDC_FPC              = ", ISPCCDC_FPC              },
	{ "### CCDC_FPC_ADDR         = ", ISPCCDC_FPC_ADDR         },
	{ "### CCDC_VDINT            = ", ISPCCDC_VDINT            },
	{ "### CCDC_ALAW             = ", ISPCCDC_ALAW             },
	{ "### CCDC_REC656IF         = ", ISPCCDC_REC656IF         },
	{ "### CCDC_CFG              = ", ISPCCDC_CFG              },
	{ "### CCDC_FMTCFG           = ", ISPCCDC_FMTCFG           },
	{ "### CCDC_FMT_HORZ         = ", ISPCCDC_FMT_HORZ         },
	{ "### CCDC_FMT_VERT         = ", ISPCCDC_FMT_VERT         },
	{ "### CCDC_FMT_ADDR0        = ", ISPCCDC_FMT_ADDR0        },
	{ "### CCDC_FMT_ADDR1        = ", ISPCCDC_FMT_ADDR1        },
	{ "### CCDC_FMT_ADDR2        = ", ISPCCDC_FMT_ADDR2        },
	{ "### CCDC_FMT_ADDR3        = ", ISPCCDC_FMT_ADDR3        },
	{ "### CCDC_FMT_ADDR4        = ", ISPCCDC_FMT_ADDR4        },
	{ "### CCDC_FMT_ADDR5        = ", ISPCCDC_FMT_ADDR5        },
	{ "### CCDC_FMT_ADDR6        = ", ISPCCDC_FMT_ADDR6        },
	{ "### CCDC_FMT_ADDR7        = ", ISPCCDC_FMT_ADDR7        },
	{ "### CCDC_PRGEVEN0         = ", ISPCCDC_PRGEVEN0         },
	{ "### CCDC_PRGEVEN1         = ", ISPCCDC_PRGEVEN1         },
	{ "### CCDC_PRGODD0          = ", ISPCCDC_PRGODD0          },
	{ "### CCDC_PRGODD1          = ", ISPCCDC_PRGODD1          },
	{ "### CCDC_VP_OUT           = ", ISPCCDC_VP_OUT           },
	{ "### CCDC_LSC_CONFIG       = ", ISPCCDC_LSC_CONFIG       },
	{ "### CCDC_LSC_INITIAL      = ", ISPCCDC_LSC_INITIAL      },
	{ "### CCDC_LSC_TABLE_BASE   = ", ISPCCDC_LSC_TABLE_BASE   },
	{ "### CCDC_LSC_TABLE_OFFSET = ", ISPCCDC_LSC_TABLE_OFFSET },
};

void ispccdc_dfs_dump(struct isp_device *isp)
{
	u32 idx;

	if (ispccdc_dfs_buff != NULL) {
		ispccdc_dfs_offs = ispccdc_dfs_buff;
		for (idx = 0; idx < ARRAY_SIZE(ispccdc_dfs_lables); idx++) {
			ispccdc_dfs_offs += sprintf(ispccdc_dfs_offs, "%s",
					ispccdc_dfs_lables[idx].ascii_reg);
			ispccdc_dfs_offs += sprintf(ispccdc_dfs_offs, "0x%08X\n"
					, isp_reg_readl(isp->dev,
							OMAP3_ISP_IOMEM_RESZ,
							ispccdc_dfs_lables[idx]\
							.reg_offset));
		}
	}
}

static ssize_t ispccdc_dfs_read(struct file *file, char __user *buf,
				size_t count, loff_t *ppos)
{
	if (ispccdc_dfs_buff && ispccdc_dfs_buff < ispccdc_dfs_offs) {
		return simple_read_from_buffer(buf, count, ppos,
					       ispccdc_dfs_buff,
					       ispccdc_dfs_offs -
					       ispccdc_dfs_buff);
	}
	return 0;
}

static const struct file_operations ispccdc_dfs_ops = {
	.owner		= THIS_MODULE,
	.read		= ispccdc_dfs_read
};

void ispccdc_dfs_setup(struct isp_device *isp)
{
	struct dentry *dfs_root = isp_get_dfs_root();
	unsigned dfs_tbl_size = 0;

	if (isp && dfs_root) {
		isp->dfs_ccdc = debugfs_create_file("ispccdc_regs", S_IRUGO,
					dfs_root, isp, &ispccdc_dfs_ops);
		if (IS_ERR(isp->dfs_ccdc)) {
			isp->dfs_ccdc = NULL;
			dev_err(isp->dev, "Can't create DFS ispccdc files !");
			return;
		}
		/* Calculate total size of DFS table */
		dfs_tbl_size =
			ARRAY_SIZE(ispccdc_dfs_lables) * MAX_DFS_LABEL_LEN +
			ARRAY_SIZE(ispccdc_dfs_lables) * MAX_DFS_LABEL_VAL;
		if (dfs_tbl_size <= PAGE_SIZE) {
			ispccdc_dfs_buff = kmalloc(PAGE_SIZE, GFP_KERNEL);
			ispccdc_dfs_offs = ispccdc_dfs_buff;
		} else {
			if (dfs_root) {
				debugfs_remove(isp->dfs_ccdc);
				isp->dfs_ccdc = NULL;
				dev_err(isp->dev, "To large DFS table: [%d]",
					dfs_tbl_size);
			}
			ispccdc_dfs_buff = NULL;
			ispccdc_dfs_offs = ispccdc_dfs_buff;
		}
	}
}

void ispccdc_dfs_shutdown(struct isp_device *isp)
{
	struct dentry *dfs_root = isp_get_dfs_root();

	if (dfs_root)
		debugfs_remove(isp->dfs_ccdc);

	kfree(ispccdc_dfs_buff);
	ispccdc_dfs_buff = NULL;
	ispccdc_dfs_offs = ispccdc_dfs_buff;
}
