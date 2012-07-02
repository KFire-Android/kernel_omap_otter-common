/*
 *  linux/drivers/mmc/core/mmc.c
 *
 *  Copyright (C) 2003-2004 Russell King, All Rights Reserved.
 *  Copyright (C) 2005-2007 Pierre Ossman, All Rights Reserved.
 *  MMCv4 support Copyright (C) 2006 Philip Langdale, All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/err.h>
#include <linux/slab.h>

#include <linux/mmc/host.h>
#include <linux/mmc/card.h>
#include <linux/mmc/mmc.h>

//qvx_emmc
#include <linux/scatterlist.h>
#include <asm/io.h>
#include <asm/uaccess.h>

#include "core.h"
#include "bus.h"
#include "mmc_ops.h"
#include "sd_ops.h"

#define EMMC		1
static int RW_OFFSET;
static int RW_SHIFT;

static const unsigned int tran_exp[] = {
	10000,		100000,		1000000,	10000000,
	0,		0,		0,		0
};

static const unsigned char tran_mant[] = {
	0,	10,	12,	13,	15,	20,	25,	30,
	35,	40,	45,	50,	55,	60,	70,	80,
};

static const unsigned int tacc_exp[] = {
	1,	10,	100,	1000,	10000,	100000,	1000000, 10000000,
};

static const unsigned int tacc_mant[] = {
	0,	10,	12,	13,	15,	20,	25,	30,
	35,	40,	45,	50,	55,	60,	70,	80,
};

#define UNSTUFF_BITS(resp,start,size)					\
	({								\
		const int __size = size;				\
		const u32 __mask = (__size < 32 ? 1 << __size : 0) - 1;	\
		const int __off = 3 - ((start) / 32);			\
		const int __shft = (start) & 31;			\
		u32 __res;						\
									\
		__res = resp[__off] >> __shft;				\
		if (__size + __shft > 32)				\
			__res |= resp[__off-1] << ((32 - __shft) % 32);	\
		__res & __mask;						\
	})

#if EMMC //qvx_emmc
static DEFINE_MUTEX(mmc_test_lock);

static int mmc_parse_ext_csd(struct mmc_card *card, u8 *ext_csd)
{
	/*
	 * EXT_CSD fields
	*/
        //
	printk("EXT_CSD_SEC_BAD_BLK_MGMNT = 0x%02x\n", ext_csd[EXT_CSD_SEC_BAD_BLK_MGMNT]);    /* R/W */
	printk("EXT_CSD_ENH_START_ADDR = 0x%02x%02x%02x%02x\n", ext_csd[EXT_CSD_ENH_START_ADDR+3]
		, ext_csd[EXT_CSD_ENH_START_ADDR+2], ext_csd[EXT_CSD_ENH_START_ADDR+1], ext_csd[EXT_CSD_ENH_START_ADDR]);       /* R/W, 4 bytes */
	printk("EXT_CSD_ENH_SIZE_MULT = 0x%02x%02x%02x\n", ext_csd[EXT_CSD_ENH_SIZE_MULT+2], ext_csd[EXT_CSD_ENH_SIZE_MULT+1], ext_csd[EXT_CSD_ENH_SIZE_MULT]);        /* R/W, 3 bytes */
	printk("EXT_CSD_GP_SIZE_MULT = 0x%02x%02x%02x%02x, %02x%02x%02x%02x, %02x%02x%02x%02x\n", ext_csd[EXT_CSD_GP_SIZE_MULT+11], ext_csd[EXT_CSD_GP_SIZE_MULT+10]
		, ext_csd[EXT_CSD_GP_SIZE_MULT+9], ext_csd[EXT_CSD_GP_SIZE_MULT+8], ext_csd[EXT_CSD_GP_SIZE_MULT+7], ext_csd[EXT_CSD_GP_SIZE_MULT+6]
		, ext_csd[EXT_CSD_GP_SIZE_MULT+5], ext_csd[EXT_CSD_GP_SIZE_MULT+4], ext_csd[EXT_CSD_GP_SIZE_MULT+3], ext_csd[EXT_CSD_GP_SIZE_MULT+2]
		, ext_csd[EXT_CSD_GP_SIZE_MULT+1], ext_csd[EXT_CSD_GP_SIZE_MULT]);         /* R/W, 12 bytes */
	printk("EXT_CSD_PARTITION_SETTING_COMPLETED = 0x%02x\n", ext_csd[EXT_CSD_PARTITION_SETTING_COMPLETED]);    /* R/W */
	printk("EXT_CSD_PARTITION_ATTRIBUTE = 0x%02x\n", ext_csd[EXT_CSD_PARTITION_ATTRIBUTE]); /* R/W */
	printk("EXT_CSD_MAX_ENH_SIZE_MULT = 0x%02x%02x%02x\n", ext_csd[EXT_CSD_MAX_ENH_SIZE_MULT+2], ext_csd[EXT_CSD_MAX_ENH_SIZE_MULT+1], ext_csd[EXT_CSD_MAX_ENH_SIZE_MULT]);    /* RO, 3 bytes */
	printk("EXT_CSD_PARTITION_SUPPORT = 0x%02x\n", ext_csd[EXT_CSD_PARTITION_SUPPORT]); /* RO */
	printk("EXT_CSD_HPI_MGMT = 0x%02x\n", ext_csd[EXT_CSD_HPI_MGMT]);             /* R/W/E_P */
	printk("EXT_CSD_RST_n_FUNCTION = 0x%02x\n", ext_csd[EXT_CSD_RST_n_FUNCTION]);       /* R/W */
	printk("EXT_CSD_BKOPS_EN = 0x%02x\n", ext_csd[EXT_CSD_BKOPS_EN]);             /* R/W */
	printk("EXT_CSD_BKOPS_START = 0x%02x\n", ext_csd[EXT_CSD_BKOPS_START]);          /* W/E_P */
	printk("EXT_CSD_WR_REL_PARAM = 0x%02x\n", ext_csd[EXT_CSD_WR_REL_PARAM]);         /* RO */
	printk("EXT_CSD_WR_REL_SET = 0x%02x\n", ext_csd[EXT_CSD_WR_REL_SET]);           /* R/W */
	printk("EXT_CSD_RPMB_SIZE_MULT = 0x%02x\n", ext_csd[EXT_CSD_RPMB_SIZE_MULT]);       /* RO */
	printk("EXT_CSD_FW_CONFIG = 0x%02x\n", ext_csd[EXT_CSD_FW_CONFIG]);            /* R/W */
	printk("EXT_CSD_USER_WP = 0x%02x\n", ext_csd[EXT_CSD_USER_WP]);              /* R/W, R/W/C_P, R/W/E_P */
	printk("EXT_CSD_BOOT_WP = 0x%02x\n", ext_csd[EXT_CSD_BOOT_WP]);              /* R/W, R/W/C_P */
	printk("EXT_CSD_ERASE_GROUP_DEF = 0x%02x\n", ext_csd[EXT_CSD_ERASE_GROUP_DEF]);      /* R/W/E_P */
	printk("EXT_CSD_BOOT_BUS_WIDTH = 0x%02x\n", ext_csd[EXT_CSD_BOOT_BUS_WIDTH]);       /* R/W/E */
	printk("EXT_CSD_BOOT_CONFIG_PROT = 0x%02x\n", ext_csd[EXT_CSD_BOOT_CONFIG_PROT]);     /* R/W, R/W/C_P */
	printk("EXT_CSD_PART_CONFIG = 0x%02x\n", ext_csd[EXT_CSD_PART_CONFIG]);     /* R/W/E, R/W/E_P */
	printk("EXT_CSD_ERASED_MEM_CONT = 0x%02x\n", ext_csd[EXT_CSD_ERASED_MEM_CONT]);       /* RO */
	printk("EXT_CSD_BUS_WIDTH = 0x%02x\n", ext_csd[EXT_CSD_BUS_WIDTH]);	/* R/W */
	printk("EXT_CSD_HS_TIMING = 0x%02x\n", ext_csd[EXT_CSD_HS_TIMING]);	/* R/W */
	printk("EXT_CSD_POWER_CLASS = 0x%02x\n", ext_csd[EXT_CSD_POWER_CLASS]);    /* R/W/E_P */
	printk("EXT_CSD_CMD_SET_REV = 0x%02x\n", ext_csd[EXT_CSD_CMD_SET_REV]);    /* RO */
	printk("EXT_CSD_CMD_SET = 0x%02x\n", ext_csd[EXT_CSD_CMD_SET]);        /* R/W/E_P */
	printk("EXT_CSD_REV = 0x%02x\n", ext_csd[EXT_CSD_REV]);		/* RO */
	printk("EXT_CSD_STRUCTURE = 0x%02x\n", ext_csd[EXT_CSD_STRUCTURE]);      /* RO */
	printk("EXT_CSD_CARD_TYPE = 0x%02x\n", ext_csd[EXT_CSD_CARD_TYPE]);	/* RO */
	printk("EXT_CSD_OUT_OF_INTERRUPT_TIME = 0x%02x\n", ext_csd[EXT_CSD_OUT_OF_INTERRUPT_TIME]);  /* RO */
	printk("EXT_CSD_PART_SWITCH_TIME = 0x%02x\n", ext_csd[EXT_CSD_PART_SWITCH_TIME]);  /* RO */
	printk("EXT_CSD_POWER_CL_52_195 = 0x%02x\n", ext_csd[EXT_CSD_POWER_CL_52_195]);        /* RO */
	printk("EXT_CSD_POWER_CL_26_195 = 0x%02x\n", ext_csd[EXT_CSD_POWER_CL_26_195]);        /* RO */
	printk("EXT_CSD_POWER_CL_52_360 = 0x%02x\n", ext_csd[EXT_CSD_POWER_CL_52_360]);        /* RO */
	printk("EXT_CSD_POWER_CL_26_360 = 0x%02x\n", ext_csd[EXT_CSD_POWER_CL_26_360]);        /* RO */
	printk("EXT_CSD_MIN_PERF_R_4_26 = 0x%02x\n", ext_csd[EXT_CSD_MIN_PERF_R_4_26]);        /* RO */
	printk("EXT_CSD_MIN_PERF_W_4_26 = 0x%02x\n", ext_csd[EXT_CSD_MIN_PERF_W_4_26]);        /* RO */
	printk("EXT_CSD_MIN_PERF_R_8_26_4_52 = 0x%02x\n", ext_csd[EXT_CSD_MIN_PERF_R_8_26_4_52]);   /* RO */
	printk("EXT_CSD_MIN_PERF_W_8_26_4_52 = 0x%02x\n", ext_csd[EXT_CSD_MIN_PERF_W_8_26_4_52]);   /* RO */
	printk("EXT_CSD_MIN_PERF_R_8_52 = 0x%02x\n", ext_csd[EXT_CSD_MIN_PERF_R_8_52]);        /* RO */
	printk("EXT_CSD_MIN_PERF_W_8_52 = 0x%02x\n", ext_csd[EXT_CSD_MIN_PERF_W_8_52]);        /* RO */
	printk("EXT_CSD_SEC_CNT = 0x%02x%02x%02x%02x\n", ext_csd[EXT_CSD_SEC_CNT+3], ext_csd[EXT_CSD_SEC_CNT+2]
		, ext_csd[EXT_CSD_SEC_CNT+1], ext_csd[EXT_CSD_SEC_CNT]);		/* RO, 4 bytes */
	printk("EXT_CSD_S_A_TIMEOUT = 0x%02x\n", ext_csd[EXT_CSD_S_A_TIMEOUT]);            /* RO */
	printk("EXT_CSD_S_C_VCCQ = 0x%02x\n", ext_csd[EXT_CSD_S_C_VCCQ]);               /* RO */
	printk("EXT_CSD_S_C_VCC = 0x%02x\n", ext_csd[EXT_CSD_S_C_VCC]);                /* RO */
	printk("EXT_CSD_HC_WP_GRP_SIZE = 0x%02x\n", ext_csd[EXT_CSD_HC_WP_GRP_SIZE]);         /* RO */
	printk("EXT_CSD_REL_WR_SEC_C = 0x%02x\n", ext_csd[EXT_CSD_REL_WR_SEC_C]);           /* RO */
	printk("EXT_CSD_ERASE_TIMEOUT_MULT = 0x%02x\n", ext_csd[EXT_CSD_ERASE_TIMEOUT_MULT]);     /* RO */
	printk("EXT_CSD_HC_ERASE_GRP_SIZE = 0x%02x\n", ext_csd[EXT_CSD_HC_ERASE_GRP_SIZE]);      /* RO */
	printk("EXT_CSD_ACC_SIZE = 0x%02x\n", ext_csd[EXT_CSD_ACC_SIZE]);               /* RO */
	printk("EXT_CSD_BOOT_MULT = 0x%02x\n", ext_csd[EXT_CSD_BOOT_MULT]);         /* RO */
	printk("EXT_CSD_BOOT_INFO = 0x%02x\n", ext_csd[EXT_CSD_BOOT_INFO]);              /* RO */
	printk("EXT_CSD_SEC_TRIM_MULT = 0x%02x\n", ext_csd[EXT_CSD_SEC_TRIM_MULT]);          /* RO */
	printk("EXT_CSD_SEC_ERASE_MULT = 0x%02x\n", ext_csd[EXT_CSD_SEC_ERASE_MULT]);         /* RO */
	printk("EXT_CSD_SEC_FEATURE_SUPPORT = 0x%02x\n", ext_csd[EXT_CSD_SEC_FEATURE_SUPPORT]);    /* RO */
	printk("EXT_CSD_TRIM_MULT = 0x%02x\n", ext_csd[EXT_CSD_TRIM_MULT]);              /* RO */
	printk("EXT_CSD_MIN_PERF_DDR_R_8_52 = 0x%02x\n", ext_csd[EXT_CSD_MIN_PERF_DDR_R_8_52]);    /* RO */
	printk("EXT_CSD_MIN_PERF_DDR_W_8_52 = 0x%02x\n", ext_csd[EXT_CSD_MIN_PERF_DDR_W_8_52]);    /* RO */
	printk("EXT_CSD_PWR_CL_DDR_52_195 = 0x%02x\n", ext_csd[EXT_CSD_PWR_CL_DDR_52_195]);      /* RO */
	printk("EXT_CSD_PWR_CL_DDR_52_360 = 0x%02x\n", ext_csd[EXT_CSD_PWR_CL_DDR_52_360]);      /* RO */
	printk("EXT_CSD_INI_TIMEOUT_AP = 0x%02x\n", ext_csd[EXT_CSD_INI_TIMEOUT_AP]);         /* RO */
	printk("EXT_CSD_CORRECTLY_PRG_SECTORS_NUM = 0x%02x%02x%02x%02x\n", ext_csd[EXT_CSD_CORRECTLY_PRG_SECTORS_NUM+3], ext_csd[EXT_CSD_CORRECTLY_PRG_SECTORS_NUM+2]
		, ext_csd[EXT_CSD_CORRECTLY_PRG_SECTORS_NUM+1], ext_csd[EXT_CSD_CORRECTLY_PRG_SECTORS_NUM]); /* RO, 4 bytes */
	printk("EXT_CSD_BKOPS_STATUS = 0x%02x\n", ext_csd[EXT_CSD_BKOPS_STATUS]);           /* RO */
	printk("EXT_CSD_BKOPS_SUPPORT = 0x%02x\n", ext_csd[EXT_CSD_BKOPS_SUPPORT]);          /* RO */
	printk("EXT_CSD_HPI_FEATURES = 0x%02x\n", ext_csd[EXT_CSD_HPI_FEATURES]);           /* RO */
	printk("EXT_CSD_S_CMD_SET = 0x%02x\n", ext_csd[EXT_CSD_S_CMD_SET]);              /* RO */
	return 0;
}

static bool mmc_create_gpp(struct mmc_card *card, int gpp1, int gpp2, int gpp3, int gpp4)
{
//	int size;
	int err, base;
	static u8 *ext_csd_data;

	int gp1_size_multi_0 = 0;
	int gp1_size_multi_1 = 0;
	int gp1_size_multi_2 = 0;
	int gp2_size_multi_0 = 0;
	int gp2_size_multi_1 = 0;
	int gp2_size_multi_2 = 0;
	int gp3_size_multi_0 = 0;
	int gp3_size_multi_1 = 0;
	int gp3_size_multi_2 = 0;
	int gp4_size_multi_0 = 0;
	int gp4_size_multi_1 = 0;
	int gp4_size_multi_2 = 0;
//	int enh_size_multi_0 = 1;
//	int enh_size_multi_1 = 0;
//	int enh_size_multi_2 = 0;
//	int enh_start_addr_0 = 0;
//	int enh_start_addr_1 = 0;
//	int enh_start_addr_2 = 0;
//	int enh_start_addr_3 = 0;
	int enh_attribute = 0x0e;
//	int addr_offset = 0;

	int enable_erase_greop_def = 1; /* 175 */
	int gpp_completed = 1; /* 155 */
//	int densidy = 0;

	ext_csd_data = kmalloc(512, GFP_KERNEL);
	if (!ext_csd_data) {
		printk(KERN_ERR "%s: could not allocate a buffer to "
			"receive the ext_csd.\n", mmc_hostname(card->host));
		return false;
	}

	printk ("READ EXT_CSD first\n");
	err = mmc_send_ext_csd(card, ext_csd_data);
	if (err)
	{
		if (err != -EINVAL)
		{
			kfree(ext_csd_data);
			return false;
		}
		if (card->csd.capacity == (4096 * 512))
		{
			printk(KERN_ERR "%s: unable to read EXT_CSD "
				"on a possible high capacity card. "
				"Card will be ignored.\n",
				mmc_hostname(card->host));
		}
		else
		{
			printk(KERN_WARNING "%s: unable to read "
				"EXT_CSD, performance might "
				"suffer.\n",
				mmc_hostname(card->host));
			err = 0;
		}

		kfree(ext_csd_data);
		return false;
	}

	printk ("Check PARTITIONING_EN\n");
	if ((ext_csd_data[EXT_CSD_PARTITION_SUPPORT]&0x01) != 0x01)
	{
		printk ("PARTITIONING_EN != 1\n");
		kfree(ext_csd_data);
		return false;
	}
	printk ("Check EN_ATTRIBUTE_EN\n");
	if ((ext_csd_data[EXT_CSD_PARTITION_SUPPORT]&0x02) != 0x02)
	{
		printk ("EN_ATTRIBUTE_EN != 1\n");
		kfree(ext_csd_data);
		return false;
	}
	printk ("EXT_CSD_HC_WP_GRP_SIZE = %d\n", ext_csd_data[EXT_CSD_HC_WP_GRP_SIZE]);
	printk ("EXT_CSD_HC_ERASE_GRP_SIZE = %d\n", ext_csd_data[EXT_CSD_HC_ERASE_GRP_SIZE]);

	base = ext_csd_data[EXT_CSD_HC_WP_GRP_SIZE] * ext_csd_data[EXT_CSD_HC_ERASE_GRP_SIZE] * 512/1024;

	printk ("Set ERASE_GROUP_DEF to indicate EXT_CSD_HC_WP_GRP_SIZE and EXT_CSD_HC_ERASE_GRP_SIZE are used\n");
	err = mmc_switch(card, EXT_CSD_CMD_SET_NORMAL,
			EXT_CSD_ERASE_GROUP_DEF, enable_erase_greop_def, 0);
	if (err)
	{
		printk ("Fail to set EXT_CSD_ERASE_GROUP_DEF\n");
		kfree(ext_csd_data);
		return false;
	}

	printk ("Configure partition 1 to %d MBytes\n", gpp1);
	gp1_size_multi_2 = (gpp1/base)/(256*256);
	gp1_size_multi_1 = ((gpp1/base)%(256*256))/256;
	gp1_size_multi_0 = (gpp1/base)%256;
	printk ("gp1_size_multi_2 = 0x%02x, gp1_size_multi_1 = 0x%02x, gp1_size_multi_0 = 0x%02x\n", gp1_size_multi_2, gp1_size_multi_1, gp1_size_multi_0);
	printk ("Configure partition 2 to %d MBytes\n", gpp2);
	gp2_size_multi_2 = (gpp2/base)/(256*256);
	gp2_size_multi_1 = ((gpp2/base)%(256*256))/256;
	gp2_size_multi_0 = (gpp2/base)%256;
	printk ("gp2_size_multi_2 = 0x%02x, gp2_size_multi_1 = 0x%02x, gp2_size_multi_0 = 0x%02x\n", gp2_size_multi_2, gp2_size_multi_1, gp2_size_multi_0);
	printk ("Configure partition 3 to %d MBytes\n", gpp3);
	gp3_size_multi_2 = (gpp3/base)/(256*256);
	gp3_size_multi_1 = ((gpp3/base)%(256*256))/256;
	gp3_size_multi_0 = (gpp3/base)%256;
	printk ("gp3_size_multi_2 = 0x%02x, gp3_size_multi_1 = 0x%02x, gp3_size_multi_0 = 0x%02x\n", gp3_size_multi_2, gp3_size_multi_1, gp3_size_multi_0);
	printk ("Configure partition 4 to %d MBytes\n", gpp4);
	gp4_size_multi_2 = (gpp4/base)/(256*256);
	gp4_size_multi_1 = ((gpp4/base)%(256*256))/256;
	gp4_size_multi_0 = (gpp4/base)%256;
	printk ("gp4_size_multi_2 = %d, gp4_size_multi_1 = %d, gp4_size_multi_0 = %d\n", gp4_size_multi_2, gp4_size_multi_1, gp4_size_multi_0);
#if 1
	err = mmc_switch(card, EXT_CSD_CMD_SET_NORMAL,
			EXT_CSD_GP_SIZE_MULT, gp1_size_multi_0, 0);
	if (err)
	{
		printk ("Fail to set GP_SIZE_MULTI_1_0\n");
		return false;
	}
	err = mmc_switch(card, EXT_CSD_CMD_SET_NORMAL,
			EXT_CSD_GP_SIZE_MULT + 1, gp1_size_multi_1, 0);
	if (err)
	{
		printk ("Fail to set GP_SIZE_MULTI_1_1\n");
		return false;
	}
	err = mmc_switch(card, EXT_CSD_CMD_SET_NORMAL,
			EXT_CSD_GP_SIZE_MULT + 2, gp1_size_multi_2, 0);
	if (err)
	{
		printk ("Fail to set GP_SIZE_MULTI_1_2\n");
		return false;
	}

	err = mmc_switch(card, EXT_CSD_CMD_SET_NORMAL,
			EXT_CSD_GP_SIZE_MULT + 3, gp2_size_multi_0, 0);
	if (err)
	{
		printk ("Fail to set GP_SIZE_MULTI_2_0\n");
		return false;
	}
	err = mmc_switch(card, EXT_CSD_CMD_SET_NORMAL,
			EXT_CSD_GP_SIZE_MULT + 4, gp2_size_multi_1, 0);
	if (err)
	{
		printk ("Fail to set GP_SIZE_MULTI_2_1\n");
		return false;
	}
	err = mmc_switch(card, EXT_CSD_CMD_SET_NORMAL,
			EXT_CSD_GP_SIZE_MULT + 5, gp2_size_multi_2, 0);
	if (err)
	{
		printk ("Fail to set GP_SIZE_MULTI_2_2\n");
		return false;
	}

	err = mmc_switch(card, EXT_CSD_CMD_SET_NORMAL,
			EXT_CSD_GP_SIZE_MULT + 6, gp3_size_multi_0, 0);
	if (err)
	{
		printk ("Fail to set GP_SIZE_MULTI_3_0\n");
		return false;
	}
	err = mmc_switch(card, EXT_CSD_CMD_SET_NORMAL,
			EXT_CSD_GP_SIZE_MULT + 7, gp3_size_multi_1, 0);
	if (err)
	{
		printk ("Fail to set GP_SIZE_MULTI_3_1\n");
		return false;
	}
	err = mmc_switch(card, EXT_CSD_CMD_SET_NORMAL,
			EXT_CSD_GP_SIZE_MULT + 8, gp3_size_multi_2, 0);
	if (err)
	{
		printk ("Fail to set GP_SIZE_MULTI_3_2\n");
		return false;
	}

	err = mmc_switch(card, EXT_CSD_CMD_SET_NORMAL,
			EXT_CSD_GP_SIZE_MULT + 9, gp4_size_multi_0, 0);
	if (err)
	{
		printk ("Fail to set GP_SIZE_MULTI_4_0\n");
		return false;
	}
	err = mmc_switch(card, EXT_CSD_CMD_SET_NORMAL,
			EXT_CSD_GP_SIZE_MULT + 10, gp4_size_multi_1, 0);
	if (err)
	{
		printk ("Fail to set GP_SIZE_MULTI_4_1\n");
		return false;
	}
	err = mmc_switch(card, EXT_CSD_CMD_SET_NORMAL,
			EXT_CSD_GP_SIZE_MULT + 11, gp4_size_multi_2, 0);
	if (err)
	{
		printk ("Fail to set GP_SIZE_MULTI_4_2\n");
		return false;
	}

	err = mmc_switch(card, EXT_CSD_CMD_SET_NORMAL,
			EXT_CSD_PARTITION_ATTRIBUTE, enh_attribute, 0);
	if (err)
	{
		printk ("Fail to set PARTITIONS_ATTRIBUTE\n");
		return false;
	}

	printk ("Set EXT_CSD_PARTITION_SETTING_COMPLETED\n");
	err = mmc_switch(card, EXT_CSD_CMD_SET_NORMAL,
			EXT_CSD_PARTITION_SETTING_COMPLETED, gpp_completed, 0);
	if (err)
	{
		printk ("Fail to set EXT_CSD_PARTITION_SETTING_COMPLETED\n");
		kfree(ext_csd_data);
		return false;
	}
#endif
	kfree(ext_csd_data);
	return true;
}

static bool mmc_write_gpp(struct mmc_card *card, int partition, int offset, const char* data)
{
	struct mmc_request mrq;
	struct mmc_command cmd;
	struct mmc_data mmcdata;
	struct scatterlist sg;

	int i, err;
	u8 test_data_write[512] __attribute__((aligned(512)));
	int enable_erase_greop_def = 1;

	int gpp_action = 1; /* partition number */
	static u8 *ext_csd_data;
	int part_complete = 0;

	memset(&mrq, 0, sizeof(struct mmc_request));
	memset(&cmd, 0, sizeof(struct mmc_command));
	memset(&mmcdata, 0, sizeof(struct mmc_data));
	memset(test_data_write, 0, sizeof(test_data_write));

	switch (partition)
	{
		case 1:
			printk("Write General Purpose Partition 1\n");
			gpp_action = 0x4;
			break;
		case 2:
			printk("Write General Purpose Partition 2\n");
			gpp_action = 0x5;
			break;
		case 3:
			printk("Write General Purpose Partition 3\n");
			gpp_action = 0x6;
			break;
		case 4:
			printk("Write General Purpose Partition 4\n");
			gpp_action = 0x7;
			break;
		case 5:
			printk("Write Boot Partition 2\n");
			gpp_action = 0x2;
			break;
		case 6:
			//printk("Write Boot Partition 1\n");
			gpp_action = 0x1;
			break;
		default:
			printk("\nun-recognize partition number\n");
			return false;
	}

#if 1
	ext_csd_data = kmalloc(512, GFP_KERNEL);
	if (!ext_csd_data) {
		printk(KERN_ERR "%s: could not allocate a buffer to "
			"receive the ext_csd.\n", mmc_hostname(card->host));
                mutex_unlock(&mmc_test_lock);
		return false;
	}
	err = mmc_send_ext_csd(card, ext_csd_data);
	if (err)
	{
		/*
		 * We all hosts that cannot perform the command
		 * to fail more gracefully
		 */
		if (err != -EINVAL)
		{
			kfree(ext_csd_data);
			mutex_unlock(&mmc_test_lock);
			printk(KERN_ERR "err != -EINVAL\n");
			return false;
		}

		/*
		 * High capacity cards should have this "magic" size
		 * stored in their CSD.
		 */
		if (card->csd.capacity == (4096 * 512)) {
			printk(KERN_ERR "%s: unable to read EXT_CSD "
				"on a possible high capacity card. "
				"Card will be ignored.\n",
				mmc_hostname(card->host));
		} else {
			printk(KERN_WARNING "%s: unable to read "
				"EXT_CSD, performance might "
				"suffer.\n",
				mmc_hostname(card->host));
			err = 0;
		}

		kfree(ext_csd_data);
		mutex_unlock(&mmc_test_lock);
		return false;
	}

	//printk ("Check PARTITION_SETTING_COMPLETED first\n");
	part_complete = ext_csd_data[EXT_CSD_PARTITION_SETTING_COMPLETED]&0x01;
	if (part_complete)
	{
		printk ("Set ERASE_GROUP_DEF before read, write, erase, and write protect\n");
		err = mmc_switch(card, EXT_CSD_CMD_SET_NORMAL,
				EXT_CSD_ERASE_GROUP_DEF, enable_erase_greop_def, 0);
		if (err)
		{
			printk ("Fail to set EXT_CSD_ERASE_GROUP_DEF\n");
			kfree(ext_csd_data);
			return false;
		}
	}
	else
	{
		//printk ("Partition is not ready, use boot partition 2, offset = 0x%x\n", offset);
		partition = 5;
		gpp_action = 0x2;
		if( (offset < 0) || (offset > 0x20000) ) //<128KB
			offset = 0;
		else
			offset = RW_OFFSET;
	}
#endif
	//printk ("Set PARTITION_ACCESS in PARTITION_CONFIG to partition %d\n", partition);
	err = mmc_switch(card, EXT_CSD_CMD_SET_NORMAL,
			EXT_CSD_PART_CONFIG, gpp_action, 0);
	if (err)
	{
		printk ("Fail to set PARTITION_ACCESS in PARTITION_CONFIG to partition %d\n", partition);
		kfree(ext_csd_data);
		return false;
	}
	printk ("Write data = %s to partition %d, offset 0x%x, block = %d,strlen(data) = %d \n",data, partition, offset, offset/512,strlen(data));

	for (i = 0; i < 512; i++){
		if(i > 512)
			break;
		test_data_write[i] = data[i];
	}

	cmd.opcode = MMC_WRITE_BLOCK;
	cmd.arg = offset/512 ;
	cmd.flags = MMC_RSP_R1 | MMC_CMD_ADTC;;

	mmcdata.flags = MMC_DATA_WRITE;
	mmcdata.blksz = 512;
	mmcdata.blocks = 1;
	mmcdata.sg = &sg;
	mmcdata.sg_len = 1;

	mrq.cmd = &cmd;
	mrq.data = &mmcdata;

	sg_init_one(&sg, test_data_write, 512);

	mmcdata.timeout_ns = 0;
	mmcdata.timeout_clks = 64;
	mmc_set_data_timeout(&mmcdata, card);

	mmc_wait_for_req(card->host, &mrq);

	if (cmd.error || mmcdata.error ) {
		printk(KERN_INFO "Failed to send CMD24: %d %d\n", cmd.error, mmcdata.error);
		kfree(ext_csd_data);
		return false;
	}
	//printk ("Set PARTITION_ACCESS in PARTITION_CONFIG to default\n");
	err = mmc_switch(card, EXT_CSD_CMD_SET_NORMAL,
			EXT_CSD_PART_CONFIG, 0, 0);
	if (err)
	{
		printk ("Fail to set PARTITION_ACCESS in PARTITION_CONFIG to default\n");
		kfree(ext_csd_data);
		return false;
	}
	if (part_complete){
		printk ("Reset ERASE_GROUP_DEF before read, write, erase, and write protect\n");
		err = mmc_switch(card, EXT_CSD_CMD_SET_NORMAL,
				EXT_CSD_ERASE_GROUP_DEF, 0, 0);
		if (err){
			printk ("Fail to reset EXT_CSD_ERASE_GROUP_DEF\n");
			kfree(ext_csd_data);
			return false;
		}
	}
	kfree(ext_csd_data);
	return true;
}

static bool mmc_read_gpp(struct mmc_card *card, int partition, int offset, char* read_data)
{
	struct mmc_request mrq;
	struct mmc_command cmd;
	struct mmc_data data;
	struct scatterlist sg;

	int err;
	static u8 test_data_read[512] __attribute__((aligned(512)));
	int enable_erase_group = 1;
	static u8 *ext_csd_data;

	int gpp_action = 1; /* partition number */
	int part_complete = 0;

	switch (partition)
	{
		case 1:
			printk("Read General Purpose Partition 1\n");
			gpp_action = 0x4;
			break;
		case 2:
			printk("Read General Purpose Partition 2\n");
			gpp_action = 0x5;
			break;
		case 3:
			printk("Read General Purpose Partition 3\n");
			gpp_action = 0x6;
			break;
		case 4:
			printk("Read General Purpose Partition 4\n");
			gpp_action = 0x7;
			break;
		case 5:
			printk("Read Boot Partition 2\n");
			gpp_action = 0x2;
			break;
		case 6:
			printk("Read Boot Partition 1\n");
			gpp_action = 0x1;
			break;
		default:
			printk("\nun-recognize partition number\n");
			return false;
	}

	memset(&mrq, 0, sizeof(struct mmc_request));
	memset(&cmd, 0, sizeof(struct mmc_command));
	memset(&data, 0, sizeof(struct mmc_data));
	memset(test_data_read, 0, sizeof(test_data_read));
#if 1
	ext_csd_data = kmalloc(512, GFP_KERNEL);
	if (!ext_csd_data) {
		printk(KERN_ERR "%s: could not allocate a buffer to "
			"receive the ext_csd.\n", mmc_hostname(card->host));
		mutex_unlock(&mmc_test_lock);
		return false;
	}
	err = mmc_send_ext_csd(card, ext_csd_data);
	if (err)
	{
		/*
		 * We all hosts that cannot perform the command
		 * to fail more gracefully
		 */
		if (err != -EINVAL)
		{
			kfree(ext_csd_data);
			mutex_unlock(&mmc_test_lock);
			printk(KERN_ERR "err != -EINVAL\n");
			return false;
		}

		/*
		 * High capacity cards should have this "magic" size
		 * stored in their CSD.
		 */
		if (card->csd.capacity == (4096 * 512)) {
			printk(KERN_ERR "%s: unable to read EXT_CSD "
				"on a possible high capacity card. "
				"Card will be ignored.\n",
				mmc_hostname(card->host));
		} else {
			printk(KERN_WARNING "%s: unable to read "
				"EXT_CSD, performance might "
				"suffer.\n",
				mmc_hostname(card->host));
			err = 0;
		}

		kfree(ext_csd_data);
		return false;
	}

	//printk ("Check PARTITION_SETTING_COMPLETED first\n");
	part_complete = ext_csd_data[EXT_CSD_PARTITION_SETTING_COMPLETED]&0x01;
	if (part_complete)
	{
		printk ("Set ERASE_GROUP_DEF before read, write, erase, and write protect\n");
		err = mmc_switch(card, EXT_CSD_CMD_SET_NORMAL,
				EXT_CSD_ERASE_GROUP_DEF, enable_erase_group, 0);
		if (err){
			printk ("Fail to set EXT_CSD_ERASE_GROUP_DEF\n");
			kfree(ext_csd_data);
			return false;
		}
	}
	else{
		printk ("Partition is not ready, use boot partition 2, offset 0\n");
		partition = 5;
		gpp_action = 0x2;
		if( (offset < 0) || (offset > 0x20000) ) //<128KB
			offset = 0;
		else
			offset = RW_OFFSET;
	}
#endif
	//printk ("Set PARTITION_ACCESS in PARTITION_CONFIG to partition %d\n", partition);
	err = mmc_switch(card, EXT_CSD_CMD_SET_NORMAL,
			EXT_CSD_PART_CONFIG, gpp_action, 0);
	if (err)
        {
		printk ("Fail to set PARTITION_ACCESS in PARTITION_CONFIG to partition %d\n", partition);
		kfree(ext_csd_data);
		return false;
	}
	printk ("Read data from partition %d, offset 0x%x, block = %d\n", partition, offset, offset/512);

	cmd.opcode = MMC_READ_SINGLE_BLOCK;
	cmd.arg = offset/512 ;
	cmd.flags = MMC_RSP_R1 | MMC_CMD_ADTC;;

	data.flags = MMC_DATA_READ;
	data.blksz = 512;
	data.blocks = 1;
	data.sg = &sg;
	data.sg_len = 1;

	mrq.cmd = &cmd;
	mrq.data = &data;

	sg_init_one(&sg, test_data_read, sizeof(test_data_read));

	data.timeout_ns = 0;
	data.timeout_clks = 64;

	mmc_set_data_timeout(&data, card);

	mmc_wait_for_req(card->host, &mrq);

	if (cmd.error) {
		printk(KERN_INFO "Failed to send CMD18: %d %d\n", cmd.error, data.error);
		kfree(ext_csd_data);
		return false;
	}

#if 0
	printk("00 ");
	for (i = 0; i < 512; i++)
	{
		printk("%02x ", test_data_read[i]);
		if ((i%16) == 15)
			printk("\n%02d ", (i/16) + 1);
	}
#endif
	memcpy(read_data, test_data_read, 512);
	//printk ("Set PARTITION_ACCESS in PARTITION_CONFIG to default\n");
	err = mmc_switch(card, EXT_CSD_CMD_SET_NORMAL,
			EXT_CSD_PART_CONFIG, 0, 0);
	if (err)
	{
		printk ("Fail to set PARTITION_ACCESS in PARTITION_CONFIG to default\n");
		kfree(ext_csd_data);
		return true;
	}
	if (part_complete){
		printk ("Reset ERASE_GROUP_DEF before read, write, erase, and write protect\n");
		err = mmc_switch(card, EXT_CSD_CMD_SET_NORMAL,
				EXT_CSD_ERASE_GROUP_DEF, 0, 0);
		if (err){
			printk ("Fail to reset EXT_CSD_ERASE_GROUP_DEF\n");
			kfree(ext_csd_data);
			return false;
		}
	}
	kfree(ext_csd_data);
	return true;
}

static ssize_t set_emmc_data(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t len) //, loff_t *f_ops)
{
	struct mmc_card *card;
	static u8 *ext_csd_data;
	int gpp1_size;
	int err;
	int offset;

	mutex_lock(&mmc_test_lock);
	card = container_of(dev, struct mmc_card, dev);
	mmc_claim_host(card->host);
	//printk("Allocate %s\n", mmc_hostname(card->host));

	ext_csd_data = kmalloc(512, GFP_KERNEL);
	if (!ext_csd_data) {
		printk(KERN_ERR "%s: could not allocate a buffer to "
			"receive the ext_csd.\n", mmc_hostname(card->host));
		kfree(ext_csd_data);
		mmc_release_host(card->host);
                mutex_unlock(&mmc_test_lock);
		return false;
	}
	//printk ("READ EXT_CSD first\n");
	err = mmc_send_ext_csd(card, ext_csd_data);
#if 0 //with GPP1
	gpp1_size = ext_csd_data[EXT_CSD_GP_SIZE_MULT+2]*65536 + ext_csd_data[EXT_CSD_GP_SIZE_MULT+1]*256 + ext_csd_data[EXT_CSD_GP_SIZE_MULT]
			* ext_csd_data[EXT_CSD_HC_WP_GRP_SIZE] * ext_csd_data[EXT_CSD_HC_ERASE_GRP_SIZE] * 512 * 1024;
	printk ("gpp1_size = %d\n", gpp1_size);

	offset = (gpp1_size-512*1024)/(256*1024);

	if (offset < 0)
	{
		mmc_write_gpp(card, 5, 0, buf);
	}
	else
	{
		mmc_write_gpp(card, 1, offset, buf);
	}
#else //only use boot partition 2

	mmc_write_gpp(card, 5, 0, buf);
#endif

	//mmc_switch(card, EXT_CSD_CMD_SET_NORMAL,EXT_CSD_PART_CONF, 0); //qvx_emmc

	kfree(ext_csd_data);
	mmc_release_host(card->host);
        mutex_unlock(&mmc_test_lock);
	return (512*1024);
}

static ssize_t get_emmc_data(struct device *dev, struct device_attribute *devattr,
				char *buf) //, size_t len, loff_t *f_ops)
{
	struct mmc_card *card;
	int ret, err;
	static u8 *ext_csd_data;
	u8* read_data;
	static int offset;
	int gpp1_size;
	printk ("\n%s\n", __func__);

	mutex_lock(&mmc_test_lock);


	card = container_of(dev, struct mmc_card, dev);
	mmc_claim_host(card->host);


	ext_csd_data = kmalloc(512, GFP_KERNEL);
	if (!ext_csd_data) {
		printk(KERN_ERR "%s: could not allocate a buffer to "
			"receive the ext_csd.\n", mmc_hostname(card->host));
		kfree(ext_csd_data);
		mmc_release_host(card->host);
                mutex_unlock(&mmc_test_lock);
		return false;
	}
	//printk ("READ EXT_CSD first\n");
	err = mmc_send_ext_csd(card, ext_csd_data);

	//mmc_parse_ext_csd(card, ext_csd_data); //qvx_emmc

	read_data = kmalloc(512, GFP_KERNEL);
#if 0 //with GPP1
	gpp1_size = ext_csd_data[EXT_CSD_GP_SIZE_MULT+2]*65536 + ext_csd_data[EXT_CSD_GP_SIZE_MULT+1]*256 + ext_csd_data[EXT_CSD_GP_SIZE_MULT]
			* ext_csd_data[EXT_CSD_HC_WP_GRP_SIZE] * ext_csd_data[EXT_CSD_HC_ERASE_GRP_SIZE] * 512 * 1024;
	printk ("gpp1_size = %d\n", gpp1_size);

	offset = (gpp1_size-512*1024)/(256*1024);

	if (offset < 0)
	{
		err = mmc_read_gpp(card, 5, 0, read_data);
	}
	else
	{
		err = mmc_read_gpp(card, 1, offset, read_data);
	}
	//offset++;
#else

	err = mmc_read_gpp(card, 5, 0, read_data);
#endif
#if 0
	buf = kmalloc(512, GFP_KERNEL);

	for (i = 0; i < 512; i++)
	{
		buf[i] = 0x00;
	}
#endif
	if (err == true)
	{
		memcpy(buf, read_data, 512);
		ret = 512;
	}
	else
        {
		printk("Fail to read data\n");
	}

#if 0
	printk("00 ");
	for (i = 0; i < 512; i++){
		printk("%02x ", buf[i]);
		if ((i%16) == 15)
			printk("\n%02d ", (i/16) + 1);
	}
	printk("\n");
#endif

	kfree(ext_csd_data);
	mmc_release_host(card->host);
        mutex_unlock(&mmc_test_lock);
	//unlock_kernel();
	return ret;
}

static ssize_t get_emmc_extcsd(struct device *dev, struct device_attribute *devattr,
				char *buf, size_t len, loff_t *f_ops)
{
	struct mmc_card *card;
	int ret, err;
	static u8 *ext_csd_data;

	card = container_of(dev, struct mmc_card, dev);
	mmc_claim_host(card->host);

	ext_csd_data = kmalloc(512, GFP_KERNEL);
	if (!ext_csd_data) {
		printk(KERN_ERR "%s: could not allocate a buffer to "
			"receive the ext_csd.\n", mmc_hostname(card->host));
		kfree(ext_csd_data);
		mmc_release_host(card->host);
		return false;
	}
	err = mmc_send_ext_csd(card, ext_csd_data);

	mmc_parse_ext_csd(card, ext_csd_data); //IR2_emmc

	kfree(ext_csd_data);
	mmc_release_host(card->host);
	return ret;
}

static ssize_t set_offset(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t len) //, loff_t *f_ops)
{
	memcpy(&RW_OFFSET, buf, sizeof(RW_OFFSET));
	printk("set Offset 0x%x, %d \n",RW_OFFSET,RW_OFFSET);
	return sizeof(RW_OFFSET);
}
static ssize_t get_offset(struct device *dev, struct device_attribute *devattr,
				char *buf) // , size_t len, loff_t *f_ops
{

	memcpy(buf, &RW_OFFSET, sizeof(RW_OFFSET));
	printk("Offset 0x%x\n",RW_OFFSET);
	return sizeof(RW_OFFSET);
}
#endif //EMMC

/*
* Given the decoded CSD structure, decode the raw CID to our CID structure.
*/
static int mmc_decode_cid(struct mmc_card *card)
{
	u32 *resp = card->raw_cid;

	/*
	 * The selection of the format here is based upon published
	 * specs from sandisk and from what people have reported.
	 */
	switch (card->csd.mmca_vsn) {
	case 0: /* MMC v1.0 - v1.2 */
	case 1: /* MMC v1.4 */
		card->cid.manfid	= UNSTUFF_BITS(resp, 104, 24);
		card->cid.prod_name[0]	= UNSTUFF_BITS(resp, 96, 8);
		card->cid.prod_name[1]	= UNSTUFF_BITS(resp, 88, 8);
		card->cid.prod_name[2]	= UNSTUFF_BITS(resp, 80, 8);
		card->cid.prod_name[3]	= UNSTUFF_BITS(resp, 72, 8);
		card->cid.prod_name[4]	= UNSTUFF_BITS(resp, 64, 8);
		card->cid.prod_name[5]	= UNSTUFF_BITS(resp, 56, 8);
		card->cid.prod_name[6]	= UNSTUFF_BITS(resp, 48, 8);
		card->cid.hwrev		= UNSTUFF_BITS(resp, 44, 4);
		card->cid.fwrev		= UNSTUFF_BITS(resp, 40, 4);
		card->cid.serial	= UNSTUFF_BITS(resp, 16, 24);
		card->cid.month		= UNSTUFF_BITS(resp, 12, 4);
		card->cid.year		= UNSTUFF_BITS(resp, 8, 4) + 1997;
		break;

	case 2: /* MMC v2.0 - v2.2 */
	case 3: /* MMC v3.1 - v3.3 */
	case 4: /* MMC v4 */
		card->cid.manfid	= UNSTUFF_BITS(resp, 120, 8);
		card->cid.oemid		= UNSTUFF_BITS(resp, 104, 16);
		card->cid.prod_name[0]	= UNSTUFF_BITS(resp, 96, 8);
		card->cid.prod_name[1]	= UNSTUFF_BITS(resp, 88, 8);
		card->cid.prod_name[2]	= UNSTUFF_BITS(resp, 80, 8);
		card->cid.prod_name[3]	= UNSTUFF_BITS(resp, 72, 8);
		card->cid.prod_name[4]	= UNSTUFF_BITS(resp, 64, 8);
		card->cid.prod_name[5]	= UNSTUFF_BITS(resp, 56, 8);
		card->cid.serial	= UNSTUFF_BITS(resp, 16, 32);
		card->cid.month		= UNSTUFF_BITS(resp, 12, 4);
		card->cid.year		= UNSTUFF_BITS(resp, 8, 4) + 1997;
		break;

	default:
		printk(KERN_ERR "%s: card has unknown MMCA version %d\n",
			mmc_hostname(card->host), card->csd.mmca_vsn);
		return -EINVAL;
	}

	return 0;
}

static void mmc_set_erase_size(struct mmc_card *card)
{
	if (card->ext_csd.erase_group_def & 1)
		card->erase_size = card->ext_csd.hc_erase_size;
	else
		card->erase_size = card->csd.erase_size;

	mmc_init_erase(card);
}

/*
 * Given a 128-bit response, decode to our card CSD structure.
 */
static int mmc_decode_csd(struct mmc_card *card)
{
	struct mmc_csd *csd = &card->csd;
	unsigned int e, m, a, b;
	u32 *resp = card->raw_csd;

	/*
	 * We only understand CSD structure v1.1 and v1.2.
	 * v1.2 has extra information in bits 15, 11 and 10.
	 * We also support eMMC v4.4 & v4.41.
	 */
	csd->structure = UNSTUFF_BITS(resp, 126, 2);
	if (csd->structure == 0) {
		printk(KERN_ERR "%s: unrecognised CSD structure version %d\n",
			mmc_hostname(card->host), csd->structure);
		return -EINVAL;
	}

	csd->mmca_vsn	 = UNSTUFF_BITS(resp, 122, 4);
	m = UNSTUFF_BITS(resp, 115, 4);
	e = UNSTUFF_BITS(resp, 112, 3);
	csd->tacc_ns	 = (tacc_exp[e] * tacc_mant[m] + 9) / 10;
	csd->tacc_clks	 = UNSTUFF_BITS(resp, 104, 8) * 100;

	m = UNSTUFF_BITS(resp, 99, 4);
	e = UNSTUFF_BITS(resp, 96, 3);
	csd->max_dtr	  = tran_exp[e] * tran_mant[m];
	csd->cmdclass	  = UNSTUFF_BITS(resp, 84, 12);

	e = UNSTUFF_BITS(resp, 47, 3);
	m = UNSTUFF_BITS(resp, 62, 12);
	csd->capacity	  = (1 + m) << (e + 2);

	csd->read_blkbits = UNSTUFF_BITS(resp, 80, 4);
	csd->read_partial = UNSTUFF_BITS(resp, 79, 1);
	csd->write_misalign = UNSTUFF_BITS(resp, 78, 1);
	csd->read_misalign = UNSTUFF_BITS(resp, 77, 1);
	csd->r2w_factor = UNSTUFF_BITS(resp, 26, 3);
	csd->write_blkbits = UNSTUFF_BITS(resp, 22, 4);
	csd->write_partial = UNSTUFF_BITS(resp, 21, 1);

	if (csd->write_blkbits >= 9) {
		a = UNSTUFF_BITS(resp, 42, 5);
		b = UNSTUFF_BITS(resp, 37, 5);
		csd->erase_size = (a + 1) * (b + 1);
		csd->erase_size <<= csd->write_blkbits - 9;
	}

	return 0;
}

/*
 * Read extended CSD.
 */
static int mmc_get_ext_csd(struct mmc_card *card, u8 **new_ext_csd)
{
	int err;
	u8 *ext_csd;

	BUG_ON(!card);
	BUG_ON(!new_ext_csd);

	*new_ext_csd = NULL;

	if (card->csd.mmca_vsn < CSD_SPEC_VER_4)
		return 0;

	/*
	 * As the ext_csd is so large and mostly unused, we don't store the
	 * raw block in mmc_card.
	 */
	ext_csd = kmalloc(512, GFP_KERNEL);
	if (!ext_csd) {
		printk(KERN_ERR "%s: could not allocate a buffer to "
			"receive the ext_csd.\n", mmc_hostname(card->host));
		return -ENOMEM;
	}

	err = mmc_send_ext_csd(card, ext_csd);
	if (err) {
		kfree(ext_csd);
		*new_ext_csd = NULL;

		/* If the host or the card can't do the switch,
		 * fail more gracefully. */
		if ((err != -EINVAL)
		 && (err != -ENOSYS)
		 && (err != -EFAULT))
			return err;

		/*
		 * High capacity cards should have this "magic" size
		 * stored in their CSD.
		 */
		if (card->csd.capacity == (4096 * 512)) {
			printk(KERN_ERR "%s: unable to read EXT_CSD "
				"on a possible high capacity card. "
				"Card will be ignored.\n",
				mmc_hostname(card->host));
		} else {
			printk(KERN_WARNING "%s: unable to read "
				"EXT_CSD, performance might "
				"suffer.\n",
				mmc_hostname(card->host));
			err = 0;
		}
	} else
		*new_ext_csd = ext_csd;

	return err;
}

/*
 * Decode extended CSD.
 */
static int mmc_read_ext_csd(struct mmc_card *card, u8 *ext_csd)
{
	int err = 0;

	BUG_ON(!card);

	if (!ext_csd)
		return 0;

	/* Version is coded in the CSD_STRUCTURE byte in the EXT_CSD register */
	card->ext_csd.raw_ext_csd_structure = ext_csd[EXT_CSD_STRUCTURE];
	if (card->csd.structure == 3) {
		if (card->ext_csd.raw_ext_csd_structure > 2) {
			printk(KERN_ERR "%s: unrecognised EXT_CSD structure "
				"version %d\n", mmc_hostname(card->host),
					card->ext_csd.raw_ext_csd_structure);
			err = -EINVAL;
			goto out;
		}
	}

	card->ext_csd.rev = ext_csd[EXT_CSD_REV];
	if (card->ext_csd.rev > 5) {
		printk(KERN_ERR "%s: unrecognised EXT_CSD revision %d\n",
			mmc_hostname(card->host), card->ext_csd.rev);
		err = -EINVAL;
		goto out;
	}

	card->ext_csd.raw_sectors[0] = ext_csd[EXT_CSD_SEC_CNT + 0];
	card->ext_csd.raw_sectors[1] = ext_csd[EXT_CSD_SEC_CNT + 1];
	card->ext_csd.raw_sectors[2] = ext_csd[EXT_CSD_SEC_CNT + 2];
	card->ext_csd.raw_sectors[3] = ext_csd[EXT_CSD_SEC_CNT + 3];
	if (card->ext_csd.rev >= 2) {
		card->ext_csd.sectors =
			ext_csd[EXT_CSD_SEC_CNT + 0] << 0 |
			ext_csd[EXT_CSD_SEC_CNT + 1] << 8 |
			ext_csd[EXT_CSD_SEC_CNT + 2] << 16 |
			ext_csd[EXT_CSD_SEC_CNT + 3] << 24;

		/* Cards with density > 2GiB are sector addressed */
		if (card->ext_csd.sectors > (2u * 1024 * 1024 * 1024) / 512)
			mmc_card_set_blockaddr(card);
	}
	card->ext_csd.raw_card_type = ext_csd[EXT_CSD_CARD_TYPE];
	switch (ext_csd[EXT_CSD_CARD_TYPE] & EXT_CSD_CARD_TYPE_MASK) {
	case EXT_CSD_CARD_TYPE_DDR_52 | EXT_CSD_CARD_TYPE_52 |
	     EXT_CSD_CARD_TYPE_26:
		card->ext_csd.hs_max_dtr = 52000000;
		card->ext_csd.card_type = EXT_CSD_CARD_TYPE_DDR_52;
		break;
	case EXT_CSD_CARD_TYPE_DDR_1_2V | EXT_CSD_CARD_TYPE_52 |
	     EXT_CSD_CARD_TYPE_26:
		card->ext_csd.hs_max_dtr = 52000000;
		card->ext_csd.card_type = EXT_CSD_CARD_TYPE_DDR_1_2V;
		break;
	case EXT_CSD_CARD_TYPE_DDR_1_8V | EXT_CSD_CARD_TYPE_52 |
	     EXT_CSD_CARD_TYPE_26:
		card->ext_csd.hs_max_dtr = 52000000;
		card->ext_csd.card_type = EXT_CSD_CARD_TYPE_DDR_1_8V;
		break;
	case EXT_CSD_CARD_TYPE_52 | EXT_CSD_CARD_TYPE_26:
		card->ext_csd.hs_max_dtr = 52000000;
		break;
	case EXT_CSD_CARD_TYPE_26:
		card->ext_csd.hs_max_dtr = 26000000;
		break;
	default:
		/* MMC v4 spec says this cannot happen */
		printk(KERN_WARNING "%s: card is mmc v4 but doesn't "
			"support any high-speed modes.\n",
			mmc_hostname(card->host));
	}

	card->ext_csd.raw_s_a_timeout = ext_csd[EXT_CSD_S_A_TIMEOUT];
	card->ext_csd.raw_erase_timeout_mult =
		ext_csd[EXT_CSD_ERASE_TIMEOUT_MULT];
	card->ext_csd.raw_hc_erase_grp_size =
		ext_csd[EXT_CSD_HC_ERASE_GRP_SIZE];
	if (card->ext_csd.rev >= 3) {
		u8 sa_shift = ext_csd[EXT_CSD_S_A_TIMEOUT];
		card->ext_csd.part_config = ext_csd[EXT_CSD_PART_CONFIG];

		/* EXT_CSD value is in units of 10ms, but we store in ms */
		card->ext_csd.part_time = 10 * ext_csd[EXT_CSD_PART_SWITCH_TIME];

		/* Sleep / awake timeout in 100ns units */
		if (sa_shift > 0 && sa_shift <= 0x17)
			card->ext_csd.sa_timeout =
					1 << ext_csd[EXT_CSD_S_A_TIMEOUT];
		card->ext_csd.erase_group_def =
			ext_csd[EXT_CSD_ERASE_GROUP_DEF];
		card->ext_csd.hc_erase_timeout = 300 *
			ext_csd[EXT_CSD_ERASE_TIMEOUT_MULT];
		card->ext_csd.hc_erase_size =
			ext_csd[EXT_CSD_HC_ERASE_GRP_SIZE] << 10;

		card->ext_csd.rel_sectors = ext_csd[EXT_CSD_REL_WR_SEC_C];

		/*
		 * There are two boot regions of equal size, defined in
		 * multiples of 128K.
		 */
		card->ext_csd.boot_size = ext_csd[EXT_CSD_BOOT_MULT] << 17;
	}

	card->ext_csd.raw_hc_erase_gap_size =
		ext_csd[EXT_CSD_PARTITION_ATTRIBUTE];
	card->ext_csd.raw_sec_trim_mult =
		ext_csd[EXT_CSD_SEC_TRIM_MULT];
	card->ext_csd.raw_sec_erase_mult =
		ext_csd[EXT_CSD_SEC_ERASE_MULT];
	card->ext_csd.raw_sec_feature_support =
		ext_csd[EXT_CSD_SEC_FEATURE_SUPPORT];
	card->ext_csd.raw_trim_mult =
		ext_csd[EXT_CSD_TRIM_MULT];
	if (card->ext_csd.rev >= 4) {
		/*
		 * Enhanced area feature support -- check whether the eMMC
		 * card has the Enhanced area enabled.  If so, export enhanced
		 * area offset and size to user by adding sysfs interface.
		 */
		card->ext_csd.raw_partition_support = ext_csd[EXT_CSD_PARTITION_SUPPORT];
		if ((ext_csd[EXT_CSD_PARTITION_SUPPORT] & 0x2) &&
		    (ext_csd[EXT_CSD_PARTITION_ATTRIBUTE] & 0x1)) {
			u8 hc_erase_grp_sz =
				ext_csd[EXT_CSD_HC_ERASE_GRP_SIZE];
			u8 hc_wp_grp_sz =
				ext_csd[EXT_CSD_HC_WP_GRP_SIZE];

			card->ext_csd.enhanced_area_en = 1;
			/*
			 * calculate the enhanced data area offset, in bytes
			 */
			card->ext_csd.enhanced_area_offset =
				(ext_csd[139] << 24) + (ext_csd[138] << 16) +
				(ext_csd[137] << 8) + ext_csd[136];
			if (mmc_card_blockaddr(card))
				card->ext_csd.enhanced_area_offset <<= 9;
			/*
			 * calculate the enhanced data area size, in kilobytes
			 */
			card->ext_csd.enhanced_area_size =
				(ext_csd[142] << 16) + (ext_csd[141] << 8) +
				ext_csd[140];
			card->ext_csd.enhanced_area_size *=
				(size_t)(hc_erase_grp_sz * hc_wp_grp_sz);
			card->ext_csd.enhanced_area_size <<= 9;
		} else {
			/*
			 * If the enhanced area is not enabled, disable these
			 * device attributes.
			 */
			card->ext_csd.enhanced_area_offset = -EINVAL;
			card->ext_csd.enhanced_area_size = -EINVAL;
		}
		card->ext_csd.sec_trim_mult =
			ext_csd[EXT_CSD_SEC_TRIM_MULT];
		card->ext_csd.sec_erase_mult =
			ext_csd[EXT_CSD_SEC_ERASE_MULT];
		card->ext_csd.sec_feature_support =
			ext_csd[EXT_CSD_SEC_FEATURE_SUPPORT];
		card->ext_csd.trim_timeout = 300 *
			ext_csd[EXT_CSD_TRIM_MULT];
	}

	if (card->ext_csd.rev >= 5)
		card->ext_csd.rel_param = ext_csd[EXT_CSD_WR_REL_PARAM];

	card->ext_csd.raw_erased_mem_count = ext_csd[EXT_CSD_ERASED_MEM_CONT];
	if (ext_csd[EXT_CSD_ERASED_MEM_CONT])
		card->erased_byte = 0xFF;
	else
		card->erased_byte = 0x0;

out:
	return err;
}

static inline void mmc_free_ext_csd(u8 *ext_csd)
{
	kfree(ext_csd);
}


static int mmc_compare_ext_csds(struct mmc_card *card, unsigned bus_width)
{
	u8 *bw_ext_csd;
	int err;

	if (bus_width == MMC_BUS_WIDTH_1)
		return 0;

	err = mmc_get_ext_csd(card, &bw_ext_csd);

	if (err || bw_ext_csd == NULL) {
		if (bus_width != MMC_BUS_WIDTH_1)
			err = -EINVAL;
		goto out;
	}

	if (bus_width == MMC_BUS_WIDTH_1)
		goto out;

	/* only compare read only fields */
	err = (!(card->ext_csd.raw_partition_support ==
			bw_ext_csd[EXT_CSD_PARTITION_SUPPORT]) &&
		(card->ext_csd.raw_erased_mem_count ==
			bw_ext_csd[EXT_CSD_ERASED_MEM_CONT]) &&
		(card->ext_csd.rev ==
			bw_ext_csd[EXT_CSD_REV]) &&
		(card->ext_csd.raw_ext_csd_structure ==
			bw_ext_csd[EXT_CSD_STRUCTURE]) &&
		(card->ext_csd.raw_card_type ==
			bw_ext_csd[EXT_CSD_CARD_TYPE]) &&
		(card->ext_csd.raw_s_a_timeout ==
			bw_ext_csd[EXT_CSD_S_A_TIMEOUT]) &&
		(card->ext_csd.raw_hc_erase_gap_size ==
			bw_ext_csd[EXT_CSD_HC_WP_GRP_SIZE]) &&
		(card->ext_csd.raw_erase_timeout_mult ==
			bw_ext_csd[EXT_CSD_ERASE_TIMEOUT_MULT]) &&
		(card->ext_csd.raw_hc_erase_grp_size ==
			bw_ext_csd[EXT_CSD_HC_ERASE_GRP_SIZE]) &&
		(card->ext_csd.raw_sec_trim_mult ==
			bw_ext_csd[EXT_CSD_SEC_TRIM_MULT]) &&
		(card->ext_csd.raw_sec_erase_mult ==
			bw_ext_csd[EXT_CSD_SEC_ERASE_MULT]) &&
		(card->ext_csd.raw_sec_feature_support ==
			bw_ext_csd[EXT_CSD_SEC_FEATURE_SUPPORT]) &&
		(card->ext_csd.raw_trim_mult ==
			bw_ext_csd[EXT_CSD_TRIM_MULT]) &&
		(card->ext_csd.raw_sectors[0] ==
			bw_ext_csd[EXT_CSD_SEC_CNT + 0]) &&
		(card->ext_csd.raw_sectors[1] ==
			bw_ext_csd[EXT_CSD_SEC_CNT + 1]) &&
		(card->ext_csd.raw_sectors[2] ==
			bw_ext_csd[EXT_CSD_SEC_CNT + 2]) &&
		(card->ext_csd.raw_sectors[3] ==
			bw_ext_csd[EXT_CSD_SEC_CNT + 3]));
	if (err)
		err = -EINVAL;

out:
	mmc_free_ext_csd(bw_ext_csd);
	return err;
}


static bool mmc_write_bp1(struct mmc_card *card, int partition, int offset, const char* data)
{
	struct mmc_request mrq;
	struct mmc_command cmd;
	struct mmc_data mmcdata;
	struct scatterlist sg;

	int i, err;
	u8 test_data_write[512] __attribute__((aligned(512)));
	int enable_erase_greop_def = 1;

	int gpp_action = 1; /* partition number */
	static u8 *ext_csd_data;
	int part_complete = 0;

	memset(&mrq, 0, sizeof(struct mmc_request));
	memset(&cmd, 0, sizeof(struct mmc_command));
	memset(&mmcdata, 0, sizeof(struct mmc_data));
	memset(test_data_write, 0, sizeof(test_data_write));

	switch (partition)
	{
		case 1:
			printk("Write General Purpose Partition 1\n");
			gpp_action = 0x4;
			break;
		case 2:
			printk("Write General Purpose Partition 2\n");
			gpp_action = 0x5;
			break;
		case 3:
			printk("Write General Purpose Partition 3\n");
			gpp_action = 0x6;
			break;
		case 4:
			printk("Write General Purpose Partition 4\n");
			gpp_action = 0x7;
			break;
		case 5:
			printk("Write Boot Partition 2\n");
			gpp_action = 0x2;
			break;
		case 6:
			printk("Write Boot Partition 1\n");
			gpp_action = 0x1;
			break;
		default:
			printk("\nun-recognize partition number\n");
			return false;
	}

#if 1
	ext_csd_data = kmalloc(512, GFP_KERNEL);
	if (!ext_csd_data) {
		printk(KERN_ERR "%s: could not allocate a buffer to "
			"receive the ext_csd.\n", mmc_hostname(card->host));
                mutex_unlock(&mmc_test_lock);
		return false;
	}
	err = mmc_send_ext_csd(card, ext_csd_data);
	if (err)
	{
		/*
		 * We all hosts that cannot perform the command
		 * to fail more gracefully
		 */
		if (err != -EINVAL)
		{
			kfree(ext_csd_data);
			mutex_unlock(&mmc_test_lock);
			printk(KERN_ERR "err != -EINVAL\n");
			return false;
		}

		/*
		 * High capacity cards should have this "magic" size
		 * stored in their CSD.
		 */
		if (card->csd.capacity == (4096 * 512)) {
			printk(KERN_ERR "%s: unable to read EXT_CSD "
				"on a possible high capacity card. "
				"Card will be ignored.\n",
				mmc_hostname(card->host));
		} else {
			printk(KERN_WARNING "%s: unable to read "
				"EXT_CSD, performance might "
				"suffer.\n",
				mmc_hostname(card->host));
			err = 0;
		}

		kfree(ext_csd_data);
		mutex_unlock(&mmc_test_lock);
		return false;
	}

	//printk ("Check PARTITION_SETTING_COMPLETED first\n");
	part_complete = ext_csd_data[EXT_CSD_PARTITION_SETTING_COMPLETED]&0x01;
	if (part_complete)
	{
		printk ("Set ERASE_GROUP_DEF before read, write, erase, and write protect\n");
		err = mmc_switch(card, EXT_CSD_CMD_SET_NORMAL,
				EXT_CSD_ERASE_GROUP_DEF, enable_erase_greop_def, 0);
		if (err)
		{
			printk ("Fail to set EXT_CSD_ERASE_GROUP_DEF\n");
			kfree(ext_csd_data);
			return false;
		}
	}
	else
	{
		//printk ("\n<ker> Partition is not ready, use boot partition 1, offset = 0x%x\n", offset);
		partition = 6;
		gpp_action = 0x1;
		if( (offset < 0) || (offset > 0x20000) ) //<128KB
			offset = 0;
		else
			offset = RW_SHIFT;
	}
#endif
	//printk ("Set PARTITION_ACCESS in PARTITION_CONFIG to partition %d\n", partition);
	err = mmc_switch(card, EXT_CSD_CMD_SET_NORMAL,
			EXT_CSD_PART_CONFIG, gpp_action, 0);
	if (err)
	{
		printk ("Fail to set PARTITION_ACCESS in PARTITION_CONFIG to partition %d\n", partition);
		kfree(ext_csd_data);
		return false;
	}
	//printk ("\n<ker> Write data = %s to partition %d, offset 0x%x, block = %d,strlen(data) = %d \n",data, partition, offset, offset/512,strlen(data));

	for (i = 0; i < 512; i++){
		if(i > 512)
			break;
		test_data_write[i] = data[i];
	}

	cmd.opcode = MMC_WRITE_BLOCK;
	cmd.arg = offset/512 ;
	cmd.flags = MMC_RSP_R1 | MMC_CMD_ADTC;;

	mmcdata.flags = MMC_DATA_WRITE;
	mmcdata.blksz = 512;
	mmcdata.blocks = 1;
	mmcdata.sg = &sg;
	mmcdata.sg_len = 1;

	mrq.cmd = &cmd;
	mrq.data = &mmcdata;

	sg_init_one(&sg, test_data_write, 512);

	mmcdata.timeout_ns = 0;
	mmcdata.timeout_clks = 64;
	mmc_set_data_timeout(&mmcdata, card);

	mmc_wait_for_req(card->host, &mrq);

	if (cmd.error || mmcdata.error ) {
		printk(KERN_INFO "Failed to send CMD24: %d %d\n", cmd.error, mmcdata.error);
		kfree(ext_csd_data);
		return false;
	}
	//printk ("Set PARTITION_ACCESS in PARTITION_CONFIG to default\n");
	err = mmc_switch(card, EXT_CSD_CMD_SET_NORMAL,
			EXT_CSD_PART_CONFIG, 0, 0);
	if (err)
	{
		printk ("Fail to set PARTITION_ACCESS in PARTITION_CONFIG to default\n");
		kfree(ext_csd_data);
		return false;
	}
	if (part_complete){
		printk ("Reset ERASE_GROUP_DEF before read, write, erase, and write protect\n");
		err = mmc_switch(card, EXT_CSD_CMD_SET_NORMAL,
				EXT_CSD_ERASE_GROUP_DEF, 0, 0);
		if (err){
			printk ("Fail to reset EXT_CSD_ERASE_GROUP_DEF\n");
			kfree(ext_csd_data);
			return false;
		}
	}
	kfree(ext_csd_data);

	return true;
}

static bool mmc_read_bp1(struct mmc_card *card, int partition, int offset, char* read_data)
{
	struct mmc_request mrq;
	struct mmc_command cmd;
	struct mmc_data data;
	struct scatterlist sg;

	int err;
	static u8 test_data_read[512] __attribute__((aligned(512)));
	int enable_erase_group = 1;
	static u8 *ext_csd_data;

	int gpp_action = 1; /* partition number */
	int part_complete = 0;

	switch (partition)
	{
		case 1:
			printk("Read General Purpose Partition 1\n");
			gpp_action = 0x4;
			break;
		case 2:
			printk("Read General Purpose Partition 2\n");
			gpp_action = 0x5;
			break;
		case 3:
			printk("Read General Purpose Partition 3\n");
			gpp_action = 0x6;
			break;
		case 4:
			printk("Read General Purpose Partition 4\n");
			gpp_action = 0x7;
			break;
		case 5:
			printk("Read Boot Partition 2\n");
			gpp_action = 0x2;
			break;
		case 6:
			//printk("Read Boot Partition 1\n");
			gpp_action = 0x1;
			break;
		default:
			printk("\nun-recognize partition number\n");
			return false;
	}

	memset(&mrq, 0, sizeof(struct mmc_request));
	memset(&cmd, 0, sizeof(struct mmc_command));
	memset(&data, 0, sizeof(struct mmc_data));
	memset(test_data_read, 0, sizeof(test_data_read));
#if 1
	ext_csd_data = kmalloc(512, GFP_KERNEL);
	if (!ext_csd_data) {
		printk(KERN_ERR "%s: could not allocate a buffer to "
			"receive the ext_csd.\n", mmc_hostname(card->host));
		mutex_unlock(&mmc_test_lock);
		return false;
	}
	err = mmc_send_ext_csd(card, ext_csd_data);
	if (err)
	{
		/*
		 * We all hosts that cannot perform the command
		 * to fail more gracefully
		 */
		if (err != -EINVAL)
		{
			kfree(ext_csd_data);
			mutex_unlock(&mmc_test_lock);
			printk(KERN_ERR "err != -EINVAL\n");
			return false;
		}

		/*
		 * High capacity cards should have this "magic" size
		 * stored in their CSD.
		 */
		if (card->csd.capacity == (4096 * 512)) {
			printk(KERN_ERR "%s: unable to read EXT_CSD "
				"on a possible high capacity card. "
				"Card will be ignored.\n",
				mmc_hostname(card->host));
		} else {
			printk(KERN_WARNING "%s: unable to read "
				"EXT_CSD, performance might "
				"suffer.\n",
				mmc_hostname(card->host));
			err = 0;
		}

		kfree(ext_csd_data);
		return false;
	}

	//printk ("Check PARTITION_SETTING_COMPLETED first\n");
	part_complete = ext_csd_data[EXT_CSD_PARTITION_SETTING_COMPLETED]&0x01;
	if (part_complete)
	{
		printk ("Set ERASE_GROUP_DEF before read, write, erase, and write protect\n");
		err = mmc_switch(card, EXT_CSD_CMD_SET_NORMAL,
				EXT_CSD_ERASE_GROUP_DEF, enable_erase_group, 0);
		if (err){
			printk ("Fail to set EXT_CSD_ERASE_GROUP_DEF\n");
			kfree(ext_csd_data);
			return false;
		}
	}
	else{
		//printk ("\n<ker> Partition is not ready, use boot partition 1, offset 0\n");
		partition = 6;
		gpp_action = 0x1;
		if( (offset < 0) || (offset > 0x20000) ) //<128KB
			offset = 0;
		else
			offset = RW_SHIFT;
	}
#endif
	//printk ("Set PARTITION_ACCESS in PARTITION_CONFIG to partition %d\n", partition);
	err = mmc_switch(card, EXT_CSD_CMD_SET_NORMAL,
			EXT_CSD_PART_CONFIG, gpp_action, 0);
	if (err)
        {
		printk ("Fail to set PARTITION_ACCESS in PARTITION_CONFIG to partition %d\n", partition);
		kfree(ext_csd_data);
		return false;
	}
	//printk ("\n <ker> Read data from partition %d, offset 0x%x, block = %d\n", partition, offset, offset/512);

	cmd.opcode = MMC_READ_SINGLE_BLOCK;
	cmd.arg = offset/512 ;
	cmd.flags = MMC_RSP_R1 | MMC_CMD_ADTC;;

	data.flags = MMC_DATA_READ;
	data.blksz = 512;
	data.blocks = 1;
	data.sg = &sg;
	data.sg_len = 1;

	mrq.cmd = &cmd;
	mrq.data = &data;

	sg_init_one(&sg, test_data_read, sizeof(test_data_read));

	data.timeout_ns = 0;
	data.timeout_clks = 64;

	mmc_set_data_timeout(&data, card);

	mmc_wait_for_req(card->host, &mrq);

	if (cmd.error) {
		printk(KERN_INFO "Failed to send CMD18: %d %d\n", cmd.error, data.error);
		kfree(ext_csd_data);
		return false;
	}

#if 0
	printk("00 ");
	for (i = 0; i < 512; i++)
	{
		printk("%02x ", test_data_read[i]);
		if ((i%16) == 15)
			printk("\n%02d ", (i/16) + 1);
	}
#endif
	memcpy(read_data, test_data_read, 512);
	//printk ("Set PARTITION_ACCESS in PARTITION_CONFIG to default\n");
	err = mmc_switch(card, EXT_CSD_CMD_SET_NORMAL,
			EXT_CSD_PART_CONFIG, 0, 0);
	if (err)
	{
		printk ("Fail to set PARTITION_ACCESS in PARTITION_CONFIG to default\n");
		kfree(ext_csd_data);
		return true;
	}
	if (part_complete){
		printk ("Reset ERASE_GROUP_DEF before read, write, erase, and write protect\n");
		err = mmc_switch(card, EXT_CSD_CMD_SET_NORMAL,
				EXT_CSD_ERASE_GROUP_DEF, 0, 0);
		if (err){
			printk ("Fail to reset EXT_CSD_ERASE_GROUP_DEF\n");
			kfree(ext_csd_data);
			return false;
		}
	}
	kfree(ext_csd_data);

	return true;
}


static ssize_t get_bp1_data(struct device *dev, struct device_attribute *devattr,
				char *buf) //, size_t len, loff_t *f_ops)
{
	struct mmc_card *card;
	int ret, err;
	static u8 *ext_csd_data;
	u8* read_data;
	static int offset;
	int gpp1_size;

	mutex_lock(&mmc_test_lock);


	card = container_of(dev, struct mmc_card, dev);
	mmc_claim_host(card->host);


	ext_csd_data = kmalloc(512, GFP_KERNEL);
	if (!ext_csd_data) {
		printk(KERN_ERR "%s: could not allocate a buffer to "
			"receive the ext_csd.\n", mmc_hostname(card->host));
		kfree(ext_csd_data);
		mmc_release_host(card->host);
                mutex_unlock(&mmc_test_lock);
		return false;
	}
	//printk ("READ EXT_CSD first\n");
	err = mmc_send_ext_csd(card, ext_csd_data);

	//mmc_parse_ext_csd(card, ext_csd_data); //qvx_emmc

	read_data = kmalloc(512, GFP_KERNEL);
#if 0 //with GPP1
	gpp1_size = ext_csd_data[EXT_CSD_GP_SIZE_MULT+2]*65536 + ext_csd_data[EXT_CSD_GP_SIZE_MULT+1]*256 + ext_csd_data[EXT_CSD_GP_SIZE_MULT]
			* ext_csd_data[EXT_CSD_HC_WP_GRP_SIZE] * ext_csd_data[EXT_CSD_HC_ERASE_GRP_SIZE] * 512 * 1024;
	printk ("gpp1_size = %d\n", gpp1_size);

	offset = (gpp1_size-512*1024)/(256*1024);

	if (offset < 0)
	{
		err = mmc_read_gpp(card, 5, 0, read_data);
	}
	else
	{
		err = mmc_read_gpp(card, 1, offset, read_data);
	}
	//offset++;
#else

	err = mmc_read_bp1(card, 6, 0, read_data);
#endif
#if 0
	buf = kmalloc(512, GFP_KERNEL);

	for (i = 0; i < 512; i++)
	{
		buf[i] = 0x00;
	}
#endif
	if (err == true)
	{
		memcpy(buf, read_data, 512);
		ret = 512;
	}
	else
        {
		printk("Fail to read data\n");
	}

#if 0
	printk("00 ");
	for (i = 0; i < 512; i++){
		printk("%02x ", buf[i]);
		if ((i%16) == 15)
			printk("\n%02d ", (i/16) + 1);
	}
	printk("\n");
#endif

	kfree(ext_csd_data);
	mmc_release_host(card->host);
	mutex_unlock(&mmc_test_lock);
	//unlock_kernel();
	return ret;
}

static ssize_t set_bp1_data(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t len) //, loff_t *f_ops)
{
	struct mmc_card *card;
	static u8 *ext_csd_data;
	int gpp1_size;
	int err;
	int offset;

	mutex_lock(&mmc_test_lock);
	card = container_of(dev, struct mmc_card, dev);
	mmc_claim_host(card->host);
	//printk("Allocate %s\n", mmc_hostname(card->host));

	ext_csd_data = kmalloc(512, GFP_KERNEL);
	if (!ext_csd_data) {
		printk(KERN_ERR "%s: could not allocate a buffer to "
			"receive the ext_csd.\n", mmc_hostname(card->host));
		kfree(ext_csd_data);
		mmc_release_host(card->host);
                mutex_unlock(&mmc_test_lock);
		return false;
	}
	//printk ("READ EXT_CSD first\n");
	err = mmc_send_ext_csd(card, ext_csd_data);
#if 0 //with GPP1
	gpp1_size = ext_csd_data[EXT_CSD_GP_SIZE_MULT+2]*65536 + ext_csd_data[EXT_CSD_GP_SIZE_MULT+1]*256 + ext_csd_data[EXT_CSD_GP_SIZE_MULT]
			* ext_csd_data[EXT_CSD_HC_WP_GRP_SIZE] * ext_csd_data[EXT_CSD_HC_ERASE_GRP_SIZE] * 512 * 1024;
	printk ("gpp1_size = %d\n", gpp1_size);

	offset = (gpp1_size-512*1024)/(256*1024);

	if (offset < 0)
	{
		mmc_write_gpp(card, 5, 0, buf);
	}
	else
	{
		mmc_write_gpp(card, 1, offset, buf);
	}
#else //only use boot partition 2

	mmc_write_bp1(card, 6, 0, buf);
#endif

	//mmc_switch(card, EXT_CSD_CMD_SET_NORMAL,EXT_CSD_PART_CONF, 0); //qvx_emmc

	kfree(ext_csd_data);
	mmc_release_host(card->host);
	mutex_unlock(&mmc_test_lock);
	return (512*1024);
}


static ssize_t get_shift(struct device *dev, struct device_attribute *devattr,
				char *buf) //, size_t len, loff_t *f_ops)
{
	memcpy(buf, &RW_SHIFT, sizeof(RW_SHIFT));
	//printk("<ker> RW_SHIFT 0x%x\n",RW_SHIFT);
	return sizeof(RW_SHIFT);
}

static ssize_t set_shift(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t len) //, loff_t *f_ops)
{
	memcpy(&RW_SHIFT, buf, sizeof(RW_SHIFT));
	//printk("<ker> set shift 0x%x, %d \n",RW_SHIFT,RW_SHIFT);
	return sizeof(RW_SHIFT);
}

MMC_DEV_ATTR(cid, "%08x%08x%08x%08x\n", card->raw_cid[0], card->raw_cid[1],
	card->raw_cid[2], card->raw_cid[3]);
MMC_DEV_ATTR(csd, "%08x%08x%08x%08x\n", card->raw_csd[0], card->raw_csd[1],
	card->raw_csd[2], card->raw_csd[3]);
MMC_DEV_ATTR(date, "%02d/%04d\n", card->cid.month, card->cid.year);
MMC_DEV_ATTR(erase_size, "%u\n", card->erase_size << 9);
MMC_DEV_ATTR(preferred_erase_size, "%u\n", card->pref_erase << 9);
MMC_DEV_ATTR(fwrev, "0x%x\n", card->cid.fwrev);
MMC_DEV_ATTR(hwrev, "0x%x\n", card->cid.hwrev);
MMC_DEV_ATTR(manfid, "0x%06x\n", card->cid.manfid);
MMC_DEV_ATTR(name, "%s\n", card->cid.prod_name);
MMC_DEV_ATTR(oemid, "0x%04x\n", card->cid.oemid);
MMC_DEV_ATTR(serial, "0x%08x\n", card->cid.serial);
MMC_DEV_ATTR(enhanced_area_offset, "%llu\n",
		card->ext_csd.enhanced_area_offset);
MMC_DEV_ATTR(enhanced_area_size, "%u\n", card->ext_csd.enhanced_area_size);

#if EMMC 
MMC_DEV_ATTR(density, "0x%08x\n",card->ext_csd.sectors);
//static DEVICE_ATTR(extcsd, S_IRUGO | S_IWUSR, get_emmc_extcsd, NULL);
//boot partiton 2 
static DEVICE_ATTR(offset, 0666, get_offset, set_offset);
static DEVICE_ATTR(bp2, 0666, get_emmc_data, set_emmc_data);
// boot partition 1 control
static DEVICE_ATTR(bp1, 0666, get_bp1_data, set_bp1_data);
static DEVICE_ATTR(shift, 0666, get_shift, set_shift);
// boot partition 1 control
#endif

static struct attribute *mmc_std_attrs[] = {
	&dev_attr_cid.attr,
	&dev_attr_csd.attr,
	&dev_attr_date.attr,
	&dev_attr_erase_size.attr,
	&dev_attr_preferred_erase_size.attr,
	&dev_attr_fwrev.attr,
	&dev_attr_hwrev.attr,
	&dev_attr_manfid.attr,
	&dev_attr_name.attr,
	&dev_attr_oemid.attr,
	&dev_attr_serial.attr,
	&dev_attr_enhanced_area_offset.attr,
	&dev_attr_enhanced_area_size.attr,
#if EMMC 
	&dev_attr_density.attr,
	&dev_attr_bp2.attr,
	&dev_attr_offset.attr,
	&dev_attr_bp1.attr,
	&dev_attr_shift.attr,
#endif
	NULL,
};

static struct attribute_group mmc_std_attr_group = {
	.attrs = mmc_std_attrs,
};

static const struct attribute_group *mmc_attr_groups[] = {
	&mmc_std_attr_group,
	NULL,
};

static struct device_type mmc_type = {
	.groups = mmc_attr_groups,
};

/*
 * Handle the detection and initialisation of a card.
 *
 * In the case of a resume, "oldcard" will contain the card
 * we're trying to reinitialise.
 */
static int mmc_init_card(struct mmc_host *host, u32 ocr,
	struct mmc_card *oldcard)
{
	struct mmc_card *card;
	int err, ddr = 0;
	u32 cid[4];
	unsigned int max_dtr;
	u32 rocr;
	u8 *ext_csd = NULL;

	BUG_ON(!host);
	WARN_ON(!host->claimed);

	/*
	 * Since we're changing the OCR value, we seem to
	 * need to tell some cards to go back to the idle
	 * state.  We wait 1ms to give cards time to
	 * respond.
	 */
	mmc_go_idle(host);

	/* The extra bit indicates that we support high capacity */
	err = mmc_send_op_cond(host, ocr | (1 << 30), &rocr);
	if (err)
		goto err;

	/*
	 * For SPI, enable CRC as appropriate.
	 */
	if (mmc_host_is_spi(host)) {
		err = mmc_spi_set_crc(host, use_spi_crc);
		if (err)
			goto err;
	}

	/*
	 * Fetch CID from card.
	 */
	if (mmc_host_is_spi(host))
		err = mmc_send_cid(host, cid);
	else
		err = mmc_all_send_cid(host, cid);
	if (err)
		goto err;

	if (oldcard) {
		if (memcmp(cid, oldcard->raw_cid, sizeof(cid)) != 0) {
			err = -ENOENT;
			goto err;
		}

		card = oldcard;
	} else {
		/*
		 * Allocate card structure.
		 */
		card = mmc_alloc_card(host, &mmc_type);
		if (IS_ERR(card)) {
			err = PTR_ERR(card);
			goto err;
		}

		card->type = MMC_TYPE_MMC;
		card->rca = 1;
		memcpy(card->raw_cid, cid, sizeof(card->raw_cid));
	}

	/*
	 * For native busses:  set card RCA and quit open drain mode.
	 */
	if (!mmc_host_is_spi(host)) {
		err = mmc_set_relative_addr(card);
		if (err)
			goto free_card;

		mmc_set_bus_mode(host, MMC_BUSMODE_PUSHPULL);
	}

	if (!oldcard) {
		/*
		 * Fetch CSD from card.
		 */
		err = mmc_send_csd(card, card->raw_csd);
		if (err)
			goto free_card;

		err = mmc_decode_csd(card);
		if (err)
			goto free_card;
		err = mmc_decode_cid(card);
		if (err)
			goto free_card;
	}

	/*
	 * Select card, as all following commands rely on that.
	 */
	if (!mmc_host_is_spi(host)) {
		err = mmc_select_card(card);
		if (err)
			goto free_card;
	}

	if (!oldcard) {
		/*
		 * Fetch and process extended CSD.
		 */

		err = mmc_get_ext_csd(card, &ext_csd);
		if (err)
			goto free_card;
		err = mmc_read_ext_csd(card, ext_csd);
		if (err)
			goto free_card;

		/* If doing byte addressing, check if required to do sector
		 * addressing.  Handle the case of <2GB cards needing sector
		 * addressing.  See section 8.1 JEDEC Standard JED84-A441;
		 * ocr register has bit 30 set for sector addressing.
		 */
		if (!(mmc_card_blockaddr(card)) && (rocr & (1<<30)))
			mmc_card_set_blockaddr(card);

		/* Erase size depends on CSD and Extended CSD */
		mmc_set_erase_size(card);
	}

	/*
	 * If enhanced_area_en is TRUE, host needs to enable ERASE_GRP_DEF
	 * bit.  This bit will be lost every time after a reset or power off.
	 */
	if (card->ext_csd.enhanced_area_en) {
		err = mmc_switch(card, EXT_CSD_CMD_SET_NORMAL,
				 EXT_CSD_ERASE_GROUP_DEF, 1, 0);

		if (err && err != -EBADMSG)
			goto free_card;

		if (err) {
			err = 0;
			/*
			 * Just disable enhanced area off & sz
			 * will try to enable ERASE_GROUP_DEF
			 * during next time reinit
			 */
			card->ext_csd.enhanced_area_offset = -EINVAL;
			card->ext_csd.enhanced_area_size = -EINVAL;
		} else {
			card->ext_csd.erase_group_def = 1;
			/*
			 * enable ERASE_GRP_DEF successfully.
			 * This will affect the erase size, so
			 * here need to reset erase size
			 */
			mmc_set_erase_size(card);
		}
	}

	/*
	 * Ensure eMMC user default partition is enabled
	 */
	if (card->ext_csd.part_config & EXT_CSD_PART_CONFIG_ACC_MASK) {
		card->ext_csd.part_config &= ~EXT_CSD_PART_CONFIG_ACC_MASK;
		err = mmc_switch(card, EXT_CSD_CMD_SET_NORMAL, EXT_CSD_PART_CONFIG,
				 card->ext_csd.part_config,
				 card->ext_csd.part_time);
		if (err && err != -EBADMSG)
			goto free_card;
	}

	/*
	 * Activate high speed (if supported)
	 */
	if ((card->ext_csd.hs_max_dtr != 0) &&
		(host->caps & MMC_CAP_MMC_HIGHSPEED)) {
		err = mmc_switch(card, EXT_CSD_CMD_SET_NORMAL,
				 EXT_CSD_HS_TIMING, 1, 0);
		if (err && err != -EBADMSG)
			goto free_card;

		if (err) {
			printk(KERN_WARNING "%s: switch to highspeed failed\n",
			       mmc_hostname(card->host));
			err = 0;
		} else {
			mmc_card_set_highspeed(card);
			mmc_set_timing(card->host, MMC_TIMING_MMC_HS);
		}
	}

	/*
	 * Compute bus speed.
	 */
	max_dtr = (unsigned int)-1;

	if (mmc_card_highspeed(card)) {
		if (max_dtr > card->ext_csd.hs_max_dtr)
			max_dtr = card->ext_csd.hs_max_dtr;
	} else if (max_dtr > card->csd.max_dtr) {
		max_dtr = card->csd.max_dtr;
	}

	mmc_set_clock(host, max_dtr);

	/*
	 * Indicate DDR mode (if supported).
	 */
	if (mmc_card_highspeed(card)) {
		if ((card->ext_csd.card_type & EXT_CSD_CARD_TYPE_DDR_1_8V)
			&& ((host->caps & (MMC_CAP_1_8V_DDR |
			     MMC_CAP_UHS_DDR50))))
				ddr = MMC_1_8V_DDR_MODE;
		else if ((card->ext_csd.card_type & EXT_CSD_CARD_TYPE_DDR_1_2V)
			&& ((host->caps & (MMC_CAP_1_2V_DDR |
			     MMC_CAP_UHS_DDR50))))
				ddr = MMC_1_2V_DDR_MODE;
	}

	/*
	 * Activate wide bus and DDR (if supported).
	 */
	if ((card->csd.mmca_vsn >= CSD_SPEC_VER_4) &&
	    (host->caps & (MMC_CAP_4_BIT_DATA | MMC_CAP_8_BIT_DATA))) {
		static unsigned ext_csd_bits[][2] = {
			{ EXT_CSD_BUS_WIDTH_8, EXT_CSD_DDR_BUS_WIDTH_8 },
			{ EXT_CSD_BUS_WIDTH_4, EXT_CSD_DDR_BUS_WIDTH_4 },
			{ EXT_CSD_BUS_WIDTH_1, EXT_CSD_BUS_WIDTH_1 },
		};
		static unsigned bus_widths[] = {
			MMC_BUS_WIDTH_8,
			MMC_BUS_WIDTH_4,
			MMC_BUS_WIDTH_1
		};
		unsigned idx, bus_width = 0;

		if (host->caps & MMC_CAP_8_BIT_DATA)
			idx = 0;
		else
			idx = 1;
		for (; idx < ARRAY_SIZE(bus_widths); idx++) {
			bus_width = bus_widths[idx];
			if (bus_width == MMC_BUS_WIDTH_1)
				ddr = 0; /* no DDR for 1-bit width */
			err = mmc_switch(card, EXT_CSD_CMD_SET_NORMAL,
					 EXT_CSD_BUS_WIDTH,
					 ext_csd_bits[idx][0],
					 0);
			if (!err) {
				mmc_set_bus_width(card->host, bus_width);

				/*
				 * If controller can't handle bus width test,
				 * compare ext_csd previously read in 1 bit mode
				 * against ext_csd at new bus width
				 */
				if (!(host->caps & MMC_CAP_BUS_WIDTH_TEST))
					err = mmc_compare_ext_csds(card,
						bus_width);
				else
					err = mmc_bus_test(card, bus_width);
				if (!err)
					break;
			}
		}

		if (!err && ddr) {
			err = mmc_switch(card, EXT_CSD_CMD_SET_NORMAL,
					 EXT_CSD_BUS_WIDTH,
					 ext_csd_bits[idx][1],
					 0);
		}
		if (err) {
			printk(KERN_WARNING "%s: switch to bus width %d ddr %d "
				"failed\n", mmc_hostname(card->host),
				1 << bus_width, ddr);
			goto free_card;
		} else if (ddr) {
			/*
			 * eMMC cards can support 3.3V to 1.2V i/o (vccq)
			 * signaling.
			 *
			 * EXT_CSD_CARD_TYPE_DDR_1_8V means 3.3V or 1.8V vccq.
			 *
			 * 1.8V vccq at 3.3V core voltage (vcc) is not required
			 * in the JEDEC spec for DDR.
			 *
			 * Do not force change in vccq since we are obviously
			 * working and no change to vccq is needed.
			 *
			 * WARNING: eMMC rules are NOT the same as SD DDR
			 */
			if (ddr == MMC_1_2V_DDR_MODE) {
				err = mmc_set_signal_voltage(host,
					MMC_SIGNAL_VOLTAGE_120, 0);
				if (err)
					goto err;
			}
			mmc_card_set_ddr_mode(card);
			mmc_set_timing(card->host, MMC_TIMING_UHS_DDR50);
			mmc_set_bus_width(card->host, bus_width);
		}
	}

	if (!oldcard)
		host->card = card;

	mmc_free_ext_csd(ext_csd);
	return 0;

free_card:
	if (!oldcard)
		mmc_remove_card(card);
err:
	mmc_free_ext_csd(ext_csd);

	return err;
}

/*
 * Host is being removed. Free up the current card.
 */
static void mmc_remove(struct mmc_host *host)
{
	BUG_ON(!host);
	BUG_ON(!host->card);

	mmc_remove_card(host->card);
	host->card = NULL;
}

/*
 * Card detection callback from host.
 */
static void mmc_detect(struct mmc_host *host)
{
	int err;

	BUG_ON(!host);
	BUG_ON(!host->card);

	mmc_claim_host(host);

	/*
	 * Just check if our card has been removed.
	 */
	err = mmc_send_status(host->card, NULL);

	mmc_release_host(host);

	if (err) {
		mmc_remove(host);

		mmc_claim_host(host);
		mmc_detach_bus(host);
		mmc_power_off(host);
		mmc_release_host(host);
	}
}

/*
 * Suspend callback from host.
 */
static int mmc_suspend(struct mmc_host *host)
{
	int err = 0;

	BUG_ON(!host);
	BUG_ON(!host->card);

	mmc_claim_host(host);
	if (mmc_card_can_sleep(host))
		err = mmc_card_sleep(host);
	else if (!mmc_host_is_spi(host))
		mmc_deselect_cards(host);
	host->card->state &= ~MMC_STATE_HIGHSPEED;
	mmc_release_host_sync(host);

	return err;
}

/*
 * Resume callback from host.
 *
 * This function tries to determine if the same card is still present
 * and, if so, restore all state to it.
 */
static int mmc_resume(struct mmc_host *host)
{
	int err;

	BUG_ON(!host);
	BUG_ON(!host->card);

	mmc_claim_host(host);
	err = mmc_init_card(host, host->ocr, host->card);
	mmc_release_host_sync(host);

	return err;
}

static int mmc_power_restore(struct mmc_host *host)
{
	int ret;

	host->card->state &= ~MMC_STATE_HIGHSPEED;
	mmc_claim_host(host);
	ret = mmc_init_card(host, host->ocr, host->card);
	mmc_release_host(host);

	return ret;
}

static int mmc_sleep(struct mmc_host *host)
{
	struct mmc_card *card = host->card;
	int err = -ENOSYS;

        //If Manufacturer ID is Samsung (0x15), bypass Sleep command transmission as Samsung EMMC goes automatically in sleep mode (HW feature)	
	if(card->cid.manfid == 0x15)
	return 0;

	if (card && card->ext_csd.rev >= 3) {
		err = mmc_card_sleepawake(host, 1);
		if (err < 0)
			pr_debug("%s: Error %d while putting card into sleep",
				 mmc_hostname(host), err);
	}

	return err;
}

static int mmc_awake(struct mmc_host *host)
{
	struct mmc_card *card = host->card;
	int err = -ENOSYS;

	//If Manufacturer ID is Samsung (0x15), bypass Sleep command transmission as Samsung EMMC goes automatically in awake mode(HW feature)	
	if(card->cid.manfid == 0x15)
	return 0;

	if (card && card->ext_csd.rev >= 3) {
		err = mmc_card_sleepawake(host, 0);
		if (err < 0)
			pr_debug("%s: Error %d while awaking sleeping card",
				 mmc_hostname(host), err);
	}

	return err;
}

static const struct mmc_bus_ops mmc_ops = {
	.awake = mmc_awake,
	.sleep = mmc_sleep,
	.remove = mmc_remove,
	.detect = mmc_detect,
	.suspend = NULL,
	.resume = NULL,
	.power_restore = mmc_power_restore,
};

static const struct mmc_bus_ops mmc_ops_unsafe = {
	.awake = mmc_awake,
	.sleep = mmc_sleep,
	.remove = mmc_remove,
	.detect = mmc_detect,
	.suspend = mmc_suspend,
	.resume = mmc_resume,
	.power_restore = mmc_power_restore,
};

static void mmc_attach_bus_ops(struct mmc_host *host)
{
	const struct mmc_bus_ops *bus_ops;

	if (!mmc_card_is_removable(host))
		bus_ops = &mmc_ops_unsafe;
	else
		bus_ops = &mmc_ops;
	mmc_attach_bus(host, bus_ops);
}

/*
 * Starting point for MMC card init.
 */
int mmc_attach_mmc(struct mmc_host *host)
{
	int err;
	u32 ocr;

	BUG_ON(!host);
	WARN_ON(!host->claimed);

	err = mmc_send_op_cond(host, 0, &ocr);
	if (err)
		return err;

	mmc_attach_bus_ops(host);
	if (host->ocr_avail_mmc)
		host->ocr_avail = host->ocr_avail_mmc;

	/*
	 * We need to get OCR a different way for SPI.
	 */
	if (mmc_host_is_spi(host)) {
		err = mmc_spi_read_ocr(host, 1, &ocr);
		if (err)
			goto err;
	}

	/*
	 * Sanity check the voltages that the card claims to
	 * support.
	 */
	if (ocr & 0x7F) {
		printk(KERN_WARNING "%s: card claims to support voltages "
		       "below the defined range. These will be ignored.\n",
		       mmc_hostname(host));
		ocr &= ~0x7F;
	}

	host->ocr = mmc_select_voltage(host, ocr);

	/*
	 * Can we support the voltage of the card?
	 */
	if (!host->ocr) {
		err = -EINVAL;
		goto err;
	}

	/*
	 * Detect and init the card.
	 */
	err = mmc_init_card(host, host->ocr, NULL);
	if (err)
		goto err;

	mmc_release_host(host);
	err = mmc_add_card(host->card);
	mmc_claim_host(host);
	if (err)
		goto remove_card;

	return 0;

remove_card:
	mmc_release_host(host);
	mmc_remove_card(host->card);
	mmc_claim_host(host);
	host->card = NULL;
err:
	mmc_detach_bus(host);

	printk(KERN_ERR "%s: error %d whilst initialising MMC card\n",
		mmc_hostname(host), err);

	return err;
}
