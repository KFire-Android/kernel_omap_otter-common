/*
 * omap dmm user: virtual address space management
 *
 * Copyright (C) 2010-2011 Texas Instruments
 *
 * Written by Hari Kanigeri <h-kanigeri2@ti.com>
 *            Ramesh Gupta <grgupta@ti.com>
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/dma-mapping.h>

#ifndef __DMM_USER_MMAP_H
#define __DMM_USER_MMAP_H


#define DMM_IOC_MAGIC		'V'

#define DMM_IOCSETTLBENT	_IO(DMM_IOC_MAGIC, 0)
#define DMM_IOCMEMMAP		_IO(DMM_IOC_MAGIC, 1)
#define DMM_IOCMEMUNMAP		_IO(DMM_IOC_MAGIC, 2)
#define DMM_IOCDATOPA		_IO(DMM_IOC_MAGIC, 3)
#define DMM_IOCMEMFLUSH		_IO(DMM_IOC_MAGIC, 4)
#define DMM_IOCMEMINV		_IO(DMM_IOC_MAGIC, 5)
#define DMM_IOCCREATEPOOL	_IO(DMM_IOC_MAGIC, 6)
#define DMM_IOCDELETEPOOL	_IO(DMM_IOC_MAGIC, 7)
#define IOMMU_IOCEVENTREG	_IO(DMM_IOC_MAGIC, 10)
#define IOMMU_IOCEVENTUNREG	_IO(DMM_IOC_MAGIC, 11)

#define DMM_DA_ANON	0x1
#define DMM_DA_PHYS	0x2
#define DMM_DA_USER	0x4

struct iovmm_pool {
	u32			pool_id;
	u32			da_begin;
	u32			da_end;
	struct gen_pool		*genpool;
	struct list_head	list;
};

struct iovmm_pool_info {
	u32	pool_id;
	u32	da_begin;
	u32	da_end;
	u32	size;
	u32	flags;
};

/* used to cache dma mapping information */
struct device_dma_map_info {
	/* number of elements requested by us */
	int	num_pages;
	/* list of buffers used in this DMA action */
	struct scatterlist	*sg;
};

struct dmm_map_info {
	u32	mpu_addr;
	u32	*da;
	u32	num_of_buf;
	u32	size;
	u32	pool_id;
	u32	flags;
};

struct dmm_dma_info {
	void	*pva;
	u32	ul_size;
	enum dma_data_direction	dir;
};

struct dmm_map_object {
	struct list_head	link;
	u32	da;
	u32	va;
	u32	size;
	u32	num_usr_pgs;
	struct gen_pool	*gen_pool;
	struct page	**pages;
	struct device_dma_map_info	dma_info;
};

struct iodmm_struct {
	struct iovmm_device	*iovmm;
	struct list_head	map_list;
	u32			pool_id;
	pid_t			tgid;
};

struct iovmm_device {
	/* iommu object which this belongs to */
	struct iommu		*iommu;
	const char		*name;
	/* List of memory pool it manages */
	struct list_head        mmap_pool;
	struct mutex		dmm_map_lock;
	int			minor;
	struct cdev		cdev;
	int			refcount;
};

/* user dmm functions */
int dmm_user(struct iodmm_struct *obj,  void __user *args);

void user_remove_resources(struct iodmm_struct *obj);

int user_un_map(struct iodmm_struct *obj, const void __user *args);

int proc_begin_dma(struct iodmm_struct *obj, const void __user *args);

int proc_end_dma(struct iodmm_struct *obj, const void __user *args);

int omap_create_dmm_pool(struct iodmm_struct *obj, const void __user *args);

int omap_delete_dmm_pools(struct iodmm_struct *obj);

int program_tlb_entry(struct iodmm_struct *obj, const void __user *args);

int register_mmufault(struct iodmm_struct *obj, const void __user *args);

int unregister_mmufault(struct iodmm_struct *obj, const void __user *args);
#endif
