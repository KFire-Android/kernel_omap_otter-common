/*
 * gcmain.c
 *
 * Copyright (C) 2010-2011 Vivante Corporation.
 *
 * This package is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * THIS PACKAGE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#include <linux/version.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/cdev.h>            /* struct cdev */
#include <linux/fs.h>              /* register_chrdev_region() */
#include <linux/device.h>          /* struct class */
#include <linux/platform_device.h> /* platform_device() */
#include <linux/err.h>             /* IS_ERR() */
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/uaccess.h>	   /* copy_from_user() */
#include <linux/io.h>              /* ioremap_nocache() */
#include <linux/sched.h>           /* send_sig() */
#include <linux/pagemap.h>         /* page_cache_release() */
#include <linux/wait.h>
#include <linux/workqueue.h>
#include <linux/list.h>
#include <linux/syscalls.h>        /* sys_getpid() */

#include "gccore.h"
#include "gcreg.h"
#include "gccmdbuf.h"

#define GC_DEVICE "gc-core"

#if 1
#	define DEVICE_INT	0
#	define DEVICE_REG_BASE	0
#	define DEVICE_REG_SIZE	(256 * 1024)

#else
#	define DEVICE_INT	9
#	define DEVICE_REG_BASE	0xB0040000
#	define DEVICE_REG_SIZE	(256 * 1024)

#endif

#if !defined(PFN_DOWN)
#	define PFN_DOWN(x) \
		((x) >> PAGE_SHIFT)
#endif

#if !defined(phys_to_pfn)
#	define phys_to_pfn(phys) \
		(PFN_DOWN(phys))
#endif

#if !defined(phys_to_page)
#	define phys_to_page(paddr) \
		(pfn_to_page(phys_to_pfn(paddr)))
#endif

#define db(x) \
	printk(KERN_NOTICE "%s(%d): %s=(%08ld)\n", __func__, __LINE__, #x, \
							 (unsigned long)x);

#define dbx(x) \
	printk(KERN_NOTICE "%s(%d): %s=(0x%08lx)\n", __func__, __LINE__, \
						 #x, (unsigned long)x);

#define dbs(x) \
	printk(KERN_NOTICE "%s(%d): %s=(%s)\n", __func__, __LINE__, #x, x);

static dev_t dev;
static struct cdev cd;
static struct device *device;
static struct class *class;
u8 *reg_base;

DECLARE_WAIT_QUEUE_HEAD(gc_event);
int done;

struct gckGALDEVICE {
	void *priv;
};

static struct gckGALDEVICE gckdevice;

static int irqline = 48;
module_param(irqline, int, 0644);

static long registerMemBase = 0xF1840000;
module_param(registerMemBase, long, 0644);

struct clientinfo {
	struct list_head head;
	struct mmu2dcontext ctxt;
	u32 pid;
	struct task_struct *task;
	int mmu_dirty;
};

enum memtype {
	USER,
	KERNEL,
};

struct meminfo {
	struct list_head mmu_node;
	enum memtype memtype;
	struct mmu2dphysmem mmu2dphysmem;
};

struct bufinfo {
	struct list_head surf_node;
	u32 pa;
	u16 order;
	struct page *pg;
};

struct bltinfo {
	u32 *cmdbuf;
	u32 cmdlen;
	u32 offset[128];
};

struct bvbuffmap;

struct bvbuffdesc {
	unsigned int structsize;
	void *virtaddr;
	unsigned long length;
	struct bvbuffmap *map;
};

static void gc_work(struct work_struct *ignored)
{
	done = true;
	wake_up_interruptible(&gc_event);
}

static DECLARE_WORK(gcwork, gc_work);
static struct workqueue_struct *gcwq;
static LIST_HEAD(mmu_list);
static LIST_HEAD(surf_list);

#if ENABLE_POLLING
volatile u32 int_data;
#endif

static irqreturn_t gc_irq(int irq, void *p)
{
	u32 data = 0;

#if !ENABLE_POLLING
	struct clientinfo *client = NULL;
#endif

	/* Read AQIntrAcknowledge register. */
	data = hw_read_reg(AQ_INTR_ACKNOWLEDGE_Address);

	printk(KERN_ERR "%s():%d:data=(%lx)\n", __func__, __LINE__,
						(unsigned long)data);

	/* Our interrupt? */
	if (data != 0) {
#if MMU2D_DUMP
		/* Dump GPU status. */
		gpu_status((char *) __func__, __LINE__, data);
#endif

#if ENABLE_POLLING
		int_data = data;
#else

		/* TODO: we need to wait for an interrupt after enabling
			 the mmu, but we don't want to send a signal to
			 the user space.  Instead, we want to send a signal
			 to the driver.
		*/

		if (data == 0x10000) {
			queue_work(gcwq, &gcwork);
		} else {
			client = (struct clientinfo *)
					((struct gckGALDEVICE *)p)->priv;
			if (client != NULL)
				send_sig(SIGUSR1, client->task, 0);
		}
#endif
	}

	return IRQ_HANDLED;
}

static u32 virt2phys(u32 usr)
{
	pmd_t *pmd;
	pte_t *ptep;
	pgd_t *pgd = pgd_offset(current->mm, usr);

	if (pgd_none(*pgd) || pgd_bad(*pgd))
		return 0;

	pmd = pmd_offset(pgd, usr);
	if (pmd_none(*pmd) || pmd_bad(*pmd))
		return 0;

	ptep = pte_offset_map(pmd, usr);
	if (ptep && pte_present(*ptep))
		return (*ptep & PAGE_MASK) | (~PAGE_MASK & usr);

	return 0;
}

static int find_client(struct clientinfo **client)
{
	int ret;
	struct clientinfo *ci = NULL;
	int found = false;

	/* TODO: find the proc ID */
	list_for_each_entry(ci, &mmu_list, head) {
		if (ci->pid == current->tgid) {
			found = true;
			break;
		}
	}

	if (!found) {
		MMU2D_PRINT(KERN_ERR "%s(%d): Creating client record.\n",
			__func__, __LINE__);

		ci = kzalloc(sizeof(struct clientinfo), GFP_KERNEL);
		if (ci == NULL) {
			ret = -ENOMEM;
			goto fail;
		}

		memset(ci, 0, sizeof(struct clientinfo));
		INIT_LIST_HEAD(&ci->head);
		ci->pid = current->tgid;
		ci->task = current;

		ret = mmu2d_create_context(&ci->ctxt);
		if (ret != 0)
			goto fail;

		ret = cmdbuf_map(&ci->ctxt);
		if (ret != 0)
			goto fail;

		ci->mmu_dirty = true;

		list_add(&ci->head, &mmu_list);
	}

	gckdevice.priv = (void *) ci;
	*client = ci;
	return 0;

fail:
	if (ci != NULL) {
		if (ci->ctxt.mmu != NULL)
			mmu2d_destroy_context(&ci->ctxt);

		kfree(ci);
	}

	return ret;
}

static int doblit(unsigned long arg)
{
	int ret;
	struct clientinfo *client = NULL;
	struct bltinfo bltinfo = {0};
	struct bufinfo *bufinfo = NULL;
	u32 *buffer;
	u32 physical;
	u32 flush_size = 0, size;
	u32 i;

	MMU2D_PRINT(KERN_ERR "%s(%d): submitting a blit.\n",
		__func__, __LINE__);

	ret = find_client(&client);
	if (ret != 0)
		return ret;

	if (copy_from_user(&bltinfo, (void __user *)arg,
				sizeof(struct bltinfo)))
		return -EFAULT;

	/* 2d mmu fixup */
	i = 0;
	while (bltinfo.offset[i] != 0) {
		bufinfo =
		(struct bufinfo *)bltinfo.cmdbuf[bltinfo.offset[i] - 1];
		bltinfo.cmdbuf[bltinfo.offset[i] - 1] = bufinfo->pa;
		i++;
	}

	ret = mmu2d_set_master(&client->ctxt);
	if (ret != 0)
		return -EFAULT;

	if (client->mmu_dirty) {
		flush_size = mmu2d_flush(NULL, 0, 0);
		if (flush_size <= 0)
			return -ENOMEM;

		size = flush_size + bltinfo.cmdlen;
		if (ret != 0)
			return ret;

		ret = cmdbuf_alloc(size, &buffer, &physical);
		if (ret != 0)
			return ret;

		mmu2d_flush(buffer, physical, bltinfo.cmdlen + 4 * sizeof(u32));

		client->mmu_dirty = false;
	} else {
		ret = cmdbuf_alloc(bltinfo.cmdlen, &buffer, &physical);
		if (ret != 0)
			return ret;
	}

	memcpy((u8 *) buffer + flush_size, bltinfo.cmdbuf, bltinfo.cmdlen);

	ret = cmdbuf_flush();
	if (ret != 0)
		return ret;

	return 0;
}

/* get physical pages of a user block */
static struct meminfo *user_block_to_pa(u32 usr_addr, u32 num_pg)
{
	struct task_struct *curr_task = current;
	struct mm_struct *mm = current->mm;
	struct vm_area_struct *vma = NULL;

	struct meminfo *pa = NULL;
	struct page **pages = NULL;
	u32 *mem = NULL, write = 0, i = 0;
	int usr_count = 0;

	pa = kzalloc(sizeof(*pa), GFP_KERNEL);
	if (!pa)
		return NULL;

	mem = kzalloc(num_pg * sizeof(*mem), GFP_KERNEL);
	if (!mem) {
		kfree(pa);
		return NULL;
	}

	pages = kmalloc(num_pg * sizeof(*pages), GFP_KERNEL);
	if (!pages) {
		kfree(mem);
		kfree(pa);
		return NULL;
	}

	/*
	 * Important Note: usr_addr is mapped from user
	 * application process to current process - it must lie
	 * completely within the current virtual memory address
	 * space in order to be of use to us here.
	 */
	down_read(&mm->mmap_sem);
	vma = find_vma(mm, usr_addr + (num_pg << PAGE_SHIFT) - 1);

	if (!vma || (usr_addr < vma->vm_start)) {
		kfree(mem);
		kfree(pa);
		kfree(pages);
		up_read(&mm->mmap_sem);
		printk(KERN_ERR "Address is outside VMA: address start = %08x, "
			"user end = %08x\n",
			usr_addr, (usr_addr + (num_pg << PAGE_SHIFT)));
		return ERR_PTR(-EFAULT);
	}

	if (vma->vm_flags & (VM_WRITE | VM_MAYWRITE))
		write = 1;

	usr_count = get_user_pages(curr_task, mm, usr_addr, num_pg, write, 1,
					pages, NULL);

	if (usr_count > 0) {
		/* process user allocated buffer */
		if (usr_count != num_pg) {
			/* release the pages we did get */
			for (i = 0; i < usr_count; i++)
				page_cache_release(pages[i]);
		} else {
			/* fill in the physical address information */
			for (i = 0; i < num_pg; i++) {
				mem[i] = page_to_phys(pages[i]);
				BUG_ON(pages[i] != phys_to_page(mem[i]));
			}
		}
	} else {
		/* fallback for kernel allocated buffers */
		for (i = 0; i < num_pg; i++) {
			mem[i] = virt2phys(usr_addr);

			if (!mem[i]) {
				printk(KERN_ERR "VMA not in page table\n");
				break;
			}

			usr_addr += PAGE_SIZE;
		}
	}

	up_read(&mm->mmap_sem);

	kfree(pages);

	/* if failed to map all pages */
	if (i < num_pg) {
		kfree(mem);
		kfree(pa);
		return ERR_PTR(-EFAULT);
	}

	pa->mmu2dphysmem.pages = (pte_t *)mem;
	pa->memtype = usr_count > 0 ? USER : KERNEL;
	pa->mmu2dphysmem.pagecount = num_pg;
	return pa;
}

static int domap(unsigned long arg)
{
	int ret;
	u32 usr_addr = 0, offset = 0, num_pg = 0;
	struct clientinfo *client = NULL;
	struct bvbuffdesc bufdesc = {0};
	struct bufinfo *bufinfo = NULL;
	struct meminfo *mi = NULL;

	if (copy_from_user(&bufdesc, (void __user *)arg, sizeof(bufdesc)))
		return -EFAULT;

	ret = find_client(&client);
	if (ret)
		return ret;

	usr_addr = ((u32) bufdesc.virtaddr) & ~(PAGE_SIZE - 1);
	offset   = ((u32) bufdesc.virtaddr) &  (PAGE_SIZE - 1);
	num_pg   = DIV_ROUND_UP(bufdesc.length + offset, PAGE_SIZE);

	/* TODO: track mi and bufinfo for cleanup */
	mi = user_block_to_pa(usr_addr, num_pg);
	if (!mi)
		goto error;

	bufinfo = kzalloc(sizeof(*bufinfo), GFP_KERNEL);
	if (!bufinfo)
		goto error;

	mi->mmu2dphysmem.pagesize = PAGE_SIZE;
	mi->mmu2dphysmem.pageoffset = offset;

	ret = mmu2d_map_phys(&client->ctxt, &mi->mmu2dphysmem);
	if (ret)
		goto error;

	bufinfo->pa = mi->mmu2dphysmem.logical;
	bufdesc.map = (struct bvbuffmap *)bufinfo;

	DBGPRINT(KERN_ERR
		 "%s(%d): mapped 0x%08X to 0x%08X (pseudo ptr 0x%08X)\n",
		__func__, __LINE__,
		(u32) bufdesc.virtaddr,
		(u32) bufinfo->pa,
		(u32) bufdesc.map
		);

	if (copy_to_user((void __user *)arg, &bufdesc, sizeof(bufdesc)))
		goto error;

	/* no errors, so exit */
	goto exit;
error:
	return -EFAULT;
exit:
	return 0;
}

static int dounmap(unsigned long arg)
{
	struct bvbuffdesc bufdesc = {0};

	if (copy_from_user(&bufdesc, (void __user *)arg,
					sizeof(bufdesc)))
		return -EFAULT;

	if (copy_to_user((void __user *)arg, &bufdesc,
			sizeof(bufdesc)))
		return -EFAULT;

	return 0;
}

static long ioctl(struct file *filp, u32 cmd, unsigned long arg)
{
	switch (cmd) {
	case BLT:
		return doblit(arg);

	case MAP:
		return domap(arg);

	case UMAP:
		return dounmap(arg);
	}

	return -EINVAL;
}

static s32 open(struct inode *ip, struct file *filp)
{
	return 0;
}

static s32 release(struct inode *ip, struct file *filp)
{
	return 0;
}

static const struct file_operations ops = {
	.open    = open,
	.release = release,
	.unlocked_ioctl = ioctl,
};

static struct platform_driver pd = {
	.driver = {
		.owner = THIS_MODULE,
		.name = GC_DEVICE,
	},
	.probe = NULL,
	.shutdown = NULL,
	.remove = NULL,
};

#define GC_MINOR 0
#define GC_COUNT 1

static int __init gc_init(void)
{
	int ret = 0;
	u32 clock = 0;

	ret = alloc_chrdev_region(&dev, GC_MINOR, GC_COUNT, GC_DEVICE);
	if (ret != 0)
		return ret;

	cdev_init(&cd, &ops);
	cd.owner = THIS_MODULE;
	ret = cdev_add(&cd, dev, 1);
	if (ret)
		goto free_chrdev_region;

	class = class_create(THIS_MODULE, GC_DEVICE);
	if (IS_ERR(class)) {
		ret = PTR_ERR(class);
		goto free_cdev;
	}

	device = device_create(class, NULL, dev, NULL, GC_DEVICE);

	if (IS_ERR(device)) {
		ret = PTR_ERR(device);
		goto free_class;
	}

	ret = platform_driver_register(&pd);
	if (ret)
		goto free_device;

	/* TODO: clean this up in release call, after a blowup */
	/* create interrupt handler */
	ret = request_irq(DEVICE_INT, gc_irq, IRQF_SHARED, "gc-core",
			&gckdevice);
	if (ret)
		goto free_plat_reg;

	reg_base = ioremap_nocache(DEVICE_REG_BASE, DEVICE_REG_SIZE);
	if (!reg_base)
		goto free_plat_reg;

	gcwq = create_workqueue("gcwq");
	if (!gcwq)
		goto free_reg_mapping;

	/* gcvPOWER_ON */
	clock = SETFIELD(0, AQ_HI_CLOCK_CONTROL, CLK2D_DIS, 0) |
		SETFIELD(0, AQ_HI_CLOCK_CONTROL, FSCALE_VAL, 64) |
		SETFIELD(0, AQ_HI_CLOCK_CONTROL, FSCALE_CMD_LOAD, 1);

	hw_write_reg(AQ_HI_CLOCK_CONTROL_Address, clock);

	/* Done loading the frequency scaler. */
	clock = SETFIELD(clock, AQ_HI_CLOCK_CONTROL, FSCALE_CMD_LOAD, 0);

	hw_write_reg(AQ_HI_CLOCK_CONTROL_Address, clock);

	/* Print GPU ID. */
	gpu_id();

	/* Initialize the command buffer. */
	ret = cmdbuf_init();
	if (ret)
		goto free_workq;

	/* no errors, so exit */
	goto exit;
free_workq:
	destroy_workqueue(gcwq);
free_reg_mapping:
	iounmap(reg_base);
free_plat_reg:
	platform_driver_unregister(&pd);
free_device:
	device_destroy(class, MKDEV(MAJOR(dev), 0));
free_class:
	class_destroy(class);
free_cdev:
	cdev_del(&cd);
free_chrdev_region:
	unregister_chrdev_region(dev, 0);
exit:
	return ret;
}

static void __exit gc_exit(void)
{
	destroy_workqueue(gcwq);
	iounmap(reg_base);
	platform_driver_unregister(&pd);
	device_destroy(class, MKDEV(MAJOR(dev), 0));
	class_destroy(class);
	cdev_del(&cd);
	unregister_chrdev_region(dev, 0);
}

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("www.vivantecorp.com");
MODULE_AUTHOR("www.ti.com");
module_init(gc_init);
module_exit(gc_exit);
