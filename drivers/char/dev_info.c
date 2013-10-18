/*
 *dev_info.c
 *
 * Copyright (C) Amazon Technologies Inc. All rights reserved.
 *
 * Dinko Mironov (dmironov@mm-sol.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/proc_fs.h>
#include <linux/dev_info.h>

#include <../common.h>

struct dev_info_manuf_id_map {
	unsigned int id;
	char *name;
};

#define ID2NAME(ID,NAME)  {(ID), (NAME)}

static struct dev_info_manuf_id_map dev_info_id_table_emmc[] = {
	ID2NAME(0x11, "Toshiba"),
	ID2NAME(0x15, "Samsung"),
	ID2NAME(0x45, "Sandisk"),
	ID2NAME(0x90, "Hynix"),
	ID2NAME(0xFE, "Micron"),
};

static struct dev_info_manuf_id_map dev_info_id_table_emif[] = {
	ID2NAME(0x01, "Samsung"),
	ID2NAME(0x03, "Elpida"),
	ID2NAME(0x06, "Hynix"),
	ID2NAME(0xFF, "Micron"),
};

struct dev_info_ctx {
	char *dev_model;
	char *emif_model;
	char *emmc_model;
	u64 emmc_size;
	u32 emmc_gsize;
};

static struct dev_info_ctx dev_info;
static char *dev_info_unknown = "Unknown";

int devinfo_search(char **name, int id,
	struct dev_info_manuf_id_map *table, int table_size)
{
	int i;

	*name = dev_info_unknown;
	for (i = 0 ; i < table_size; i++) {
		if (table[i].id == id) {
			*name = table[i].name;
			return 0;
		}
	}

	return -ENOENT;
}

void devinfo_set_emmc_id(u8 id)
{
	struct dev_info_ctx *ctx = &dev_info;
	struct dev_info_manuf_id_map *table = dev_info_id_table_emmc;
	int table_size = ARRAY_SIZE(dev_info_id_table_emmc);

	devinfo_search(&ctx->emmc_model, id, table, table_size);
}

void devinfo_set_emmc_size(u32 sectors)
{
	struct dev_info_ctx *ctx = &dev_info;

	ctx->emmc_size = (u64)sectors << 9; 		// To convert in Bytes
	ctx->emmc_gsize = sectors >> 21;		// To convert in GBytes

	if (ctx->emmc_gsize <= 8)
		ctx->emmc_gsize = 8;
	else if (ctx->emmc_gsize <= 16)
		ctx->emmc_gsize = 16;
	else if(ctx->emmc_gsize > 16 && ctx->emmc_gsize <= 32)
		ctx->emmc_gsize = 32;
	else if(ctx->emmc_gsize > 32)
		ctx->emmc_gsize = 64;
}

void devinfo_set_emif(int id)
{
	struct dev_info_ctx *ctx = &dev_info;
	struct dev_info_manuf_id_map *table = dev_info_id_table_emif;
	int table_size = ARRAY_SIZE(dev_info_id_table_emif);

	devinfo_search(&ctx->emif_model, id, table, table_size);
}

static int devinfo_devmodel_read( char *page, char **start, off_t off,
                   int count, int *eof, void *data)
{
	struct dev_info_ctx *ctx = data;

	return sprintf(page, "%s\n", ctx->dev_model);
}

static int devinfo_emmcinfo_read(char *page, char **start,
	off_t off, int count, int *eof, void *data)
{
	struct dev_info_ctx *ctx = data;

	return sprintf(page, "%s %uG (real: %llu bytes)\n",
			ctx->emmc_model, ctx->emmc_gsize, ctx->emmc_size);
}

static int devinfo_emmcsize_read(char *page, char **start,
	off_t off, int count, int *eof, void *data)
{
	struct dev_info_ctx *ctx = data;

	return sprintf(page, "%uG\n", ctx->emmc_gsize);
}

static int devinfo_draminfo_read(char *page, char **start,
	off_t off, int count, int *eof, void *data)
{
	struct dev_info_ctx *ctx = data;

	return sprintf(page, "%s\n", ctx->emif_model);
}

static int __devinit devmodel_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct dev_info_ctx *ctx = &dev_info;
	struct dev_info_platform_data *info = dev_get_platdata(dev);
	struct proc_dir_entry *proc;

	proc = create_proc_read_entry("devmodel", 0444, NULL,
			devinfo_devmodel_read, ctx);
	if (!proc) {
		dev_err(dev, "Couldn't create proc entry 'devmodel'\n");
		goto exit0;
	}

	proc = create_proc_read_entry("emmcinfo", 0444, NULL,
			devinfo_emmcinfo_read, ctx);
	if (!proc) {
		dev_err(dev, "Couldn't create proc entry 'emmcinfo'\n");
		goto exit1;
	}

	proc = create_proc_read_entry("emmcsize", 0444, NULL,
			devinfo_emmcsize_read, ctx);
	if (!proc) {
		dev_err(dev, "Couldn't create proc entry 'emmcsize'\n");
		goto exit2;
	}

	proc = create_proc_read_entry("draminfo", 0444, NULL,
			devinfo_draminfo_read, ctx);
	if (!proc) {
		dev_err(dev, "Couldn't create proc entry 'draminfo'\n");
		goto exit3;
	}

	if (!ctx->emmc_model)
		ctx->emmc_model = dev_info_unknown;

	if (!ctx->dev_model)
		ctx->dev_model = dev_info_unknown;

	if (!ctx->emif_model)
		ctx->emif_model = dev_info_unknown;

	if (info)
		ctx->dev_model = info->dev_model;

	return 0;

exit3:
	remove_proc_entry("emmcsize", NULL);
exit2:
	remove_proc_entry("emmcinfo", NULL);
exit1:
	remove_proc_entry("devmodel", NULL);
exit0:
	return -ENOMEM;
}

static int __devexit devmodel_remove(struct platform_device *pdev)
{
	remove_proc_entry("devmodel", NULL);

	return 0;
}

static struct platform_driver devmodel_driver = {
	.probe		= devmodel_probe,
	.remove		= __devexit_p(devmodel_remove),
	.driver		= {
		.name	= "dev_info",
		.owner	= THIS_MODULE,
	},
};

static int __init devmodel_init(void)
{
	return platform_driver_register(&devmodel_driver);
}
module_init(devmodel_init);

static void __exit devmodel_exit(void)
{
	platform_driver_unregister(&devmodel_driver);
}
module_exit(devmodel_exit);

