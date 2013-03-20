/* OMAP Identity file for OMAP boards.
 *
 * Copyright (C) 2011 Google, Inc.
 * Copyright (C) 2010 Texas Instruments
 *
 * Based on mach-omap2/board-omap4panda.c
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/stat.h>
#include <asm/system_info.h>
#include <mach/id.h>
#include <linux/platform_device.h>
#include <linux/string.h>

#include <mach/hardware.h>

#include <plat/omap_apps_brd_id.h>

static char omap_mach_print[255];

static ssize_t omap_soc_family_show(struct kobject *kobj,
				    struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "OMAP%04x\n", GET_OMAP_TYPE);
}

static ssize_t omap_soc_revision_show(struct kobject *kobj,
				 struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "ES%d.%d\n", (GET_OMAP_REVISION() >> 4) & 0xf,
		       GET_OMAP_REVISION() & 0xf);
}

static const char *omap_types[] = {
	[OMAP2_DEVICE_TYPE_TEST]	= "TST",
	[OMAP2_DEVICE_TYPE_EMU]		= "EMU",
	[OMAP2_DEVICE_TYPE_SEC]		= "HS",
	[OMAP2_DEVICE_TYPE_GP]		= "GP",
	[OMAP2_DEVICE_TYPE_BAD]		= "BAD",
};

static ssize_t omap_prod_id_show(struct kobject *kobj,
				struct kobj_attribute *attr, char *buf)
{
	struct  omap_die_id opi;
	omap_get_production_id(&opi);
	return sprintf(buf, "%08X-%08X\n", opi.id_1, opi.id_0);
}

static ssize_t omap_die_id_show(struct kobject *kobj,
				struct kobj_attribute *attr, char *buf)
{
	struct  omap_die_id opi;
	omap_get_die_id(&opi);
	return sprintf(buf, "%08X-%08X-%08X-%08X\n", opi.id_3,
					opi.id_2, opi.id_1, opi.id_0);
}

static ssize_t omap_soc_type_show(struct kobject *kobj,
				 struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n", omap_types[omap_type()]);
}

#define OMAP_SOC_ATTR_RO(_name, _show) \
	struct kobj_attribute omap_soc_prop_attr_##_name = \
		__ATTR(_name, S_IRUGO, _show, NULL)

static OMAP_SOC_ATTR_RO(family, omap_soc_family_show);
static OMAP_SOC_ATTR_RO(revision, omap_soc_revision_show);
static OMAP_SOC_ATTR_RO(type, omap_soc_type_show);
static OMAP_SOC_ATTR_RO(production_id, omap_prod_id_show);
static OMAP_SOC_ATTR_RO(die_id, omap_die_id_show);

static struct attribute *omap_soc_prop_attrs[] = {
	&omap_soc_prop_attr_family.attr,
	&omap_soc_prop_attr_revision.attr,
	&omap_soc_prop_attr_type.attr,
	&omap_soc_prop_attr_production_id.attr,
	&omap_soc_prop_attr_die_id.attr,
	NULL,
};

static struct attribute_group omap_soc_prop_attr_group = {
	.attrs = omap_soc_prop_attrs,
};

void __init omap_create_board_props(void)
{
	struct kobject *board_props_kobj;
	struct kobject *soc_kobj = NULL;
	int ret = 0;
	char soc_f[10], soc_r[10], soc_t[10], soc_pid[20], soc_die[40];

	board_props_kobj = kobject_create_and_add("board_properties", NULL);
	if (!board_props_kobj)
		goto err_board_obj;

	soc_kobj = kobject_create_and_add("soc", board_props_kobj);
	if (!soc_kobj)
		goto err_soc_obj;

	ret = sysfs_create_group(soc_kobj, &omap_soc_prop_attr_group);
	if (ret)
		goto err_sysfs_create;

	omap_soc_family_show(NULL, NULL, soc_f);
	omap_soc_revision_show(NULL, NULL, soc_r);
	omap_soc_type_show(NULL, NULL, soc_t);
	omap_prod_id_show(NULL, NULL, soc_pid);
	omap_die_id_show(NULL, NULL, soc_die);
	snprintf(omap_mach_print, ARRAY_SIZE(omap_mach_print),
		 "\n Revision : %04x\n Serial\t: %08x%08x\n"
		 "SoC Information:\n CPU\t: %s Rev\t: %s"
		 " Type\t: %s Production ID: %s Die ID\t: %s",
		 system_rev, system_serial_high, system_serial_low,
		 soc_f, soc_r, soc_t, soc_pid, soc_die);

	mach_panic_string = omap_mach_print;

	return;

err_sysfs_create:
	kobject_put(soc_kobj);
err_soc_obj:
	kobject_put(board_props_kobj);
err_board_obj:
	if (!board_props_kobj || !soc_kobj || ret)
		pr_err("failed to create board_properties\n");
}

static struct {
	u32 class;
	char *name;
} boards_info[] __initdata = {
	{
		.class = BOARD_OMAP4_PANDA,
		.name = "Panda",
	},
	{
		.class = BOARD_OMAP4_BLAZE,
		.name = "Blaze/SDP",
	},
	{
		.class = BOARD_OMAP4_TABLET1,
		.name = "Tablet 1",
	},
	{
		.class = BOARD_OMAP4_TABLET2,
		.name = "Tablet 2",
	},
	{
		.class = BOARD_OMAP5_SEVM,
		.name = "sEVM",
	},
	{
		.class = BOARD_OMAP5_SEVM2,
		.name = "sEVM",
	},
	{
		.class = BOARD_OMAP5_SEVM3,
		.name = "sEVM",
	},
	{
		.class = BOARD_OMAP5_UEVM,
		.name = "uEVM",
	},
	{
		.class = BOARD_OMAP5_UEVM2,
		.name = "uEVM",
	},
	{
		.class = BOARD_OMAP5_TEVM,
		.name = "tEVM",
	},
};

/**
 * struct board_revision -The board revision info
 *
 * @class:	board class
 * @rev_mod:	board version (includes board class and revision)
 * @rev:	revision of specific board
 * @name:	board name
 *
 * The board revision is string which stored in the board EEPROM.
 * The string format is xxx -BBBB-CCC(x). Here is:
 * - BBBB - value reflects major changes in board,
 * - CCC  - minor changes.
 * Boot-loader converts this string into decimal value and passes it to
 * the kernel.
 * struct board_revision stores hex-converted data.
 * Example: 2170001 in decimal will be converted into 0x2170001
 */
struct board_revision {
	int class;
	int rev_mod;
	int rev;
	char name[BOARD_NAME_MAX_SIZE];
};

static struct board_revision omap_board;

int __init omap_init_board_version(int forced_rev)
{
	int i = 0, hex = 0, dec = system_rev;

	if (forced_rev) {
		hex = dec = forced_rev;
	} else {
		/*
		 * Converting to heximal value,
		 * e.g. dec:2170003 became a 0x2170003,
		 * this make check process more easy and quick
		 */
		do {
			hex |= dec%10 << i*4;
			i++;
		} while (dec /= 10);
		dec = system_rev;
	}

	switch (dec) {
	/*
	 * U-BOOT passing fake board identifiers for Blaze(4430SDP) and
	 * Panda boards. We have to have separate checks for them.
	 */
	case OMAP4_PANDA:
		omap_board.class = BOARD_OMAP4_PANDA;
		omap_board.rev_mod = (BOARD_OMAP4_PANDA << BOARD_CLASS_OFF);
		break;
	case OMAP4_BLAZE:
		omap_board.class = BOARD_OMAP4_BLAZE;
		omap_board.rev_mod = (BOARD_OMAP4_BLAZE << BOARD_CLASS_OFF);
		break;
	default:
		omap_board.class = hex >> BOARD_CLASS_OFF;
		omap_board.rev_mod = hex;
		omap_board.rev = BOARD_GET_REV(hex);
	}

	for (i = 0; i < ARRAY_SIZE(boards_info); i++) {
		if (omap_board.class == boards_info[i].class) {
			strlcpy(omap_board.name, boards_info[i].name,
						 BOARD_NAME_MAX_SIZE);
			return omap_board.rev_mod;
		}
	}

	strcpy(omap_board.name, "UNKNOWN");
	return omap_board.rev_mod;
}

bool omap_is_board_version(int req_board_version)
{
	return req_board_version == omap_board.rev_mod;
}

int omap_get_board_version(void)
{
	return omap_board.rev_mod;
}

bool omap_is_board_class(int req_class)
{
	return req_class == omap_board.class;
}

int omap_get_board_class(void)
{
	return omap_board.class;
}

bool omap_is_board_revision(int req_rev)
{
	return req_rev == omap_board.rev;
}

int omap_get_board_revision(void)
{
	return omap_board.rev;
}

const char *omap_get_board_name()
{
	return (const char *)omap_board.name;
}
