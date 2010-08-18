/*
 * omap iommu: simple virtual address space management
 *
 * Copyright (C) 2008-2009 Nokia Corporation
 *
 * Written by Hiroshi DOYU <Hiroshi.DOYU@nokia.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/err.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/device.h>
#include <linux/scatterlist.h>
#include <linux/platform_device.h>
#include <linux/eventfd.h>
#include <asm/cacheflush.h>
#include <asm/mach/map.h>

#include <plat/iommu.h>
#include <plat/iovmm.h>

#include "iopgtable.h"

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/file.h>
#include <linux/poll.h>
#include <linux/swap.h>
#include <linux/genalloc.h>


/*
 * A device driver needs to create address mappings between:
 *
 * - iommu/device address
 * - physical address
 * - mpu virtual address
 *
 * There are 4 possible patterns for them:
 *
 *    |iova/			  mapping		iommu_		page
 *    | da	pa	va	(d)-(p)-(v)		function	type
 *  ---------------------------------------------------------------------------
 *  1 | c	c	c	 1 - 1 - 1	  _kmap() / _kunmap()	s
 *  2 | c	c,a	c	 1 - 1 - 1	_kmalloc()/ _kfree()	s
 *  3 | c	d	c	 1 - n - 1	  _vmap() / _vunmap()	s
 *  4 | c	d,a	c	 1 - n - 1	_vmalloc()/ _vfree()	n*
 *
 *
 *	'iova':	device iommu virtual address
 *	'da':	alias of 'iova'
 *	'pa':	physical address
 *	'va':	mpu virtual address
 *
 *	'c':	contiguous memory area
 *	'd':	discontiguous memory area
 *	'a':	anonymous memory allocation
 *	'()':	optional feature
 *
 *	'n':	a normal page(4KB) size is used.
 *	's':	multiple iommu superpage(16MB, 1MB, 64KB, 4KB) size is used.
 *
 *	'*':	not yet, but feasible.
 */

#define OMAP_IOVMM_NAME "iovmm-omap"

static atomic_t		num_of_iovmmus;
static struct class	*omap_iovmm_class;
static dev_t		omap_iovmm_dev;
static struct kmem_cache *iovm_area_cachep;

static int omap_create_vmm_pool(struct iodmm_struct *obj, int pool_id, int size,
									int sa)
{
	struct iovmm_pool *pool;
	struct iovmm_device *iovmm = obj->iovmm;

	pool = kzalloc(sizeof(struct iovmm_pool), GFP_ATOMIC);
	if (!pool)
		goto err_out;

	pool->pool_id = pool_id;
	pool->da_begin = sa;
	pool->da_end = sa + size;
	pool->genpool = gen_pool_create(12, -1);
	gen_pool_add(pool->genpool, pool->da_begin, size, -1);
	INIT_LIST_HEAD(&pool->list);
	list_add_tail(&pool->list, &iovmm->mmap_pool);
	return 0;

err_out:
	return -ENOMEM;
}

static int omap_delete_vmm_pool(struct iodmm_struct *obj, int pool_id)
{
	struct iovmm_pool *pool;
	struct iovmm_device *iovmm_obj = obj->iovmm;
	struct list_head *_pool, *_next_pool;

	list_for_each_safe(_pool, _next_pool, &iovmm_obj->mmap_pool) {
		pool = list_entry(_pool, struct iovmm_pool, list);
		if (pool->pool_id == pool_id) {
			gen_pool_destroy(pool->genpool);
			list_del(&pool->list);
			kfree(pool);
			return 0;
		}
	}
	return -ENODEV;
}

static int omap_iovmm_ioctl(struct inode *inode, struct file *filp,
				unsigned int cmd, unsigned long args)
{
	struct iodmm_struct *obj;
	int ret = 0;
	obj = (struct iodmm_struct *)filp->private_data;

	if (!obj)
		return -EINVAL;

	if (_IOC_TYPE(cmd) != IOVMM_IOC_MAGIC)
		return -ENOTTY;

	switch (cmd) {
	case IOVMM_IOCSETTLBENT:
	{
		struct iotlb_entry e;
		int size;
		size = copy_from_user(&e, (void __user *)args,
					sizeof(struct iotlb_entry));
		if (size) {
			ret = -EINVAL;
			goto err_user_buf;
		}
		load_iotlb_entry(obj->iovmm->iommu, &e);
		break;
	}
	case IOVMM_IOCSETPTEENT:
	{
		struct iotlb_entry e;
		int size;
		int page_sz;
		struct dmm_map_object *dmm_obj;
		size = copy_from_user(&e, (void __user *)args,
				sizeof(struct iotlb_entry));
		if (size) {
			ret = -EINVAL;
			goto err_user_buf;
		}
		page_sz = e.pgsz;
		switch (page_sz) {
		case MMU_CAM_PGSZ_16M:
			size = PAGE_SIZE_16MB;
			break;
		case MMU_CAM_PGSZ_1M:
			size = PAGE_SIZE_1MB;
			break;
		case MMU_CAM_PGSZ_64K:
			size = PAGE_SIZE_64KB;
			break;
		case MMU_CAM_PGSZ_4K:
			size = PAGE_SIZE_4KB;
			break;
		default:
			size = 0;
			goto err_user_buf;
			break;
		}
		dmm_obj = add_mapping_info(obj, NULL, e.pa, e.da, size);

		iopgtable_store_entry(obj->iovmm->iommu, &e);
		break;
	}

	case IOVMM_IOCCREATEPOOL:
	{
		struct iovmm_pool_info pool_info;
		int size;

		size = copy_from_user(&pool_info, (void __user *)args,
						sizeof(struct iovmm_pool_info));
		if (size) {
			ret = -EINVAL;
			goto err_user_buf;
		}
		omap_create_vmm_pool(obj, pool_info.pool_id, pool_info.size,
							pool_info.da_begin);
		break;
	}
	case IOVMM_IOCMEMMAP:
	{
		struct  dmm_map_info map_info;
		int size;
		int status;

		size = copy_from_user(&map_info, (void __user *)args,
						sizeof(struct dmm_map_info));

		status = dmm_user(obj, map_info.mem_pool_id,
					map_info.da, map_info.mpu_addr,
					map_info.size, map_info.flags);
		copy_to_user((void __user *)args, &map_info,
					sizeof(struct dmm_map_info));
		ret = status;
		break;
	}
	case IOVMM_IOCMEMUNMAP:
	{
		u32 da;
		int size;
		int status = 0;

		size = copy_from_user(&da, (void __user *)args, sizeof(u32));
		if (size) {
			ret = -EINVAL;
			goto err_user_buf;
		}
		status = user_un_map(obj, da);
		ret = status;
		break;
	}
	case IOMMU_IOCEVENTREG:
	{
		int fd;
		int size;
		struct iommu_event_ntfy *fd_reg;

		size = copy_from_user(&fd, (void __user *)args, sizeof(int));
		if (size) {
			ret = -EINVAL;
			goto err_user_buf;
		}

		fd_reg = kzalloc(sizeof(struct iommu_event_ntfy), GFP_KERNEL);
		fd_reg->fd = fd;
		fd_reg->evt_ctx = eventfd_ctx_fdget(fd);
		INIT_LIST_HEAD(&fd_reg->list);
		spin_lock_irq(&obj->iovmm->iommu->event_lock);
		list_add_tail(&fd_reg->list, &obj->iovmm->iommu->event_list);
		spin_unlock_irq(&obj->iovmm->iommu->event_lock);
		break;
	}
	case IOMMU_IOCEVENTUNREG:
	{
		int fd;
		int size;
		struct iommu_event_ntfy *fd_reg, *temp_reg;

		size = copy_from_user(&fd, (void __user *)args, sizeof(int));
		if (size) {
			ret = -EINVAL;
			goto err_user_buf;
		}
		/* Free DMM mapped memory resources */
		spin_lock_irq(&obj->iovmm->iommu->event_lock);
		list_for_each_entry_safe(fd_reg, temp_reg,
				&obj->iovmm->iommu->event_list, list) {
			if (fd_reg->fd == fd) {
				list_del(&fd_reg->list);
				kfree(fd_reg);
			}
		}
		spin_unlock_irq(&obj->iovmm->iommu->event_lock);
		break;
	}
	case IOVMM_IOCMEMFLUSH:
	{
		int size;
		int status;
		struct dmm_dma_info dma_info;
		size = copy_from_user(&dma_info, (void __user *)args,
						sizeof(struct dmm_dma_info));
		if (size) {
			ret = -EINVAL;
			goto err_user_buf;
		}
		status = proc_begin_dma(obj, dma_info.pva, dma_info.ul_size,
								dma_info.dir);
		ret = status;
		break;
	}
	case IOVMM_IOCMEMINV:
	{
		int size;
		int status;
		struct dmm_dma_info dma_info;
		size = copy_from_user(&dma_info, (void __user *)args,
						sizeof(struct dmm_dma_info));
		if (size) {
			ret = -EINVAL;
			goto err_user_buf;
		}
		status = proc_end_dma(obj, dma_info.pva, dma_info.ul_size,
								dma_info.dir);
		ret = status;
		break;
	}
	case IOVMM_IOCDELETEPOOL:
	{
		int pool_id;
		int size;

		size = copy_from_user(&pool_id, (void __user *)args,
							sizeof(int));
		if (size) {
			ret = -EINVAL;
			goto err_user_buf;
		}
		ret = omap_delete_vmm_pool(obj, pool_id);
		break;
	}
	case IOVMM_IOCDATOPA:
	default:
		return -ENOTTY;
	}
err_user_buf:
	return ret;

}

static int omap_iovmm_open(struct inode *inode, struct file *filp)
{
	struct iodmm_struct *iodmm;
	struct iovmm_device *obj;

	obj = container_of(inode->i_cdev, struct iovmm_device, cdev);

	iodmm = kzalloc(sizeof(struct iodmm_struct), GFP_KERNEL);
	INIT_LIST_HEAD(&iodmm->map_list);
	spin_lock_init(&iodmm->dmm_map_lock);

	iodmm->iovmm = obj;
	obj->iommu = iommu_get(obj->name);
	filp->private_data = iodmm;

	return 0;
}
static int omap_iovmm_release(struct inode *inode, struct file *filp)
{
	int status = 0;
	struct iodmm_struct *obj;

	if (!filp->private_data) {
		status = -EIO;
		goto err;
	}
	obj = filp->private_data;
	flush_signals(current);
	user_remove_resources(obj);
	iommu_put(obj->iovmm->iommu);
	kfree(obj);
	filp->private_data = NULL;

err:
	return status;
}

static const struct file_operations omap_iovmm_fops = {
	.owner		=	THIS_MODULE,
	.open		=	omap_iovmm_open,
	.release	=	omap_iovmm_release,
	.ioctl		=	omap_iovmm_ioctl,
};

static int __devinit omap_iovmm_probe(struct platform_device *pdev)
{
	int err = -ENODEV;
	int major, minor;
	struct device *tmpdev;
	struct iommu_platform_data *pdata =
			(struct iommu_platform_data *)pdev->dev.platform_data;
	int ret = 0;
	struct iovmm_device *obj;

	obj = kzalloc(sizeof(struct iovm_struct), GFP_KERNEL);

	major = MAJOR(omap_iovmm_dev);
	minor = atomic_read(&num_of_iovmmus);
	atomic_inc(&num_of_iovmmus);

	obj->minor = minor;
	obj->name = pdata->name;
	INIT_LIST_HEAD(&obj->mmap_pool);

	cdev_init(&obj->cdev, &omap_iovmm_fops);
	obj->cdev.owner = THIS_MODULE;
	ret = cdev_add(&obj->cdev, MKDEV(major, minor), 1);
	if (ret) {
		dev_err(&pdev->dev, "%s: cdev_add failed: %d\n", __func__, ret);
		goto err_cdev;
	}

	tmpdev = device_create(omap_iovmm_class, NULL,
				MKDEV(major, minor),
				NULL,
				OMAP_IOVMM_NAME "%d", minor);
	if (IS_ERR(tmpdev)) {
		ret = PTR_ERR(tmpdev);
		pr_err("%s: device_create failed: %d\n", __func__, ret);
		goto clean_cdev;
	}

	pr_info("%s initialized %s, major: %d, base-minor: %d\n",
			OMAP_IOVMM_NAME,
			pdata->name,
			MAJOR(omap_iovmm_dev),
			minor);
	platform_set_drvdata(pdev, obj);
	return 0;
clean_cdev:
	cdev_del(&obj->cdev);
err_cdev:
	return err;
}

static int __devexit omap_iovmm_remove(struct platform_device *pdev)
{
	struct iovmm_device *obj = platform_get_drvdata(pdev);
	int major = MAJOR(omap_iovmm_dev);
	device_destroy(omap_iovmm_class, MKDEV(major, obj->minor));
	cdev_del(&obj->cdev);
	platform_set_drvdata(pdev, NULL);
	iopgtable_clear_entry_all(obj->iommu);
	iommu_put(obj->iommu);
	free_pages((unsigned long)obj->iommu->iopgd,
				get_order(IOPGD_TABLE_SIZE));
	kfree(obj);
	return 0;

}



static struct platform_driver omap_iovmm_driver = {
	.probe	= omap_iovmm_probe,
	.remove	= __devexit_p(omap_iovmm_remove),
	.driver	= {
		.name	= "omap-iovmm",
	},
};


/* return total bytes of sg buffers */
static size_t sgtable_len(const struct sg_table *sgt)
{
	unsigned int i, total = 0;
	struct scatterlist *sg;

	if (!sgt)
		return 0;

	for_each_sg(sgt->sgl, sg, sgt->nents, i) {
		size_t bytes;

		bytes = sg_dma_len(sg);

		if (!iopgsz_ok(bytes)) {
			pr_err("%s: sg[%d] not iommu pagesize(%x)\n",
			       __func__, i, bytes);
			return 0;
		}

		total += bytes;
	}

	return total;
}
#define sgtable_ok(x)	(!!sgtable_len(x))

/*
 * calculate the optimal number sg elements from total bytes based on
 * iommu superpages
 */
static unsigned int sgtable_nents(size_t bytes)
{
	int i;
	unsigned int nr_entries;
	const unsigned long pagesize[] = { SZ_16M, SZ_1M, SZ_64K, SZ_4K, };

	if (!IS_ALIGNED(bytes, PAGE_SIZE)) {
		pr_err("%s: wrong size %08x\n", __func__, bytes);
		return 0;
	}

	nr_entries = 0;
	for (i = 0; i < ARRAY_SIZE(pagesize); i++) {
		if (bytes >= pagesize[i]) {
			nr_entries += (bytes / pagesize[i]);
			bytes %= pagesize[i];
		}
	}
	BUG_ON(bytes);

	return nr_entries;
}

/* allocate and initialize sg_table header(a kind of 'superblock') */
static struct sg_table *sgtable_alloc(const size_t bytes, u32 flags)
{
	unsigned int nr_entries;
	int err;
	struct sg_table *sgt;

	if (!bytes)
		return ERR_PTR(-EINVAL);

	if (!IS_ALIGNED(bytes, PAGE_SIZE))
		return ERR_PTR(-EINVAL);

	/* FIXME: IOVMF_DA_FIXED should support 'superpages' */
	if ((flags & IOVMF_LINEAR) && (flags & IOVMF_DA_ANON)) {
		nr_entries = sgtable_nents(bytes);
		if (!nr_entries)
			return ERR_PTR(-EINVAL);
	} else
		nr_entries =  bytes / PAGE_SIZE;

	sgt = kzalloc(sizeof(*sgt), GFP_KERNEL);
	if (!sgt)
		return ERR_PTR(-ENOMEM);

	err = sg_alloc_table(sgt, nr_entries, GFP_KERNEL);
	if (err) {
		kfree(sgt);
		return ERR_PTR(err);
	}

	pr_debug("%s: sgt:%p(%d entries)\n", __func__, sgt, nr_entries);

	return sgt;
}

/* free sg_table header(a kind of superblock) */
static void sgtable_free(struct sg_table *sgt)
{
	if (!sgt)
		return;

	sg_free_table(sgt);
	kfree(sgt);

	pr_debug("%s: sgt:%p\n", __func__, sgt);
}

/* map 'sglist' to a contiguous mpu virtual area and return 'va' */
static void *vmap_sg(const struct sg_table *sgt)
{
	u32 va;
	size_t total;
	unsigned int i;
	struct scatterlist *sg;
	struct vm_struct *new;
	const struct mem_type *mtype;

	mtype = get_mem_type(MT_DEVICE);
	if (!mtype)
		return ERR_PTR(-EINVAL);

	total = sgtable_len(sgt);
	if (!total)
		return ERR_PTR(-EINVAL);

	new = __get_vm_area(total, VM_IOREMAP, VMALLOC_START, VMALLOC_END);
	if (!new)
		return ERR_PTR(-ENOMEM);
	va = (u32)new->addr;

	for_each_sg(sgt->sgl, sg, sgt->nents, i) {
		size_t bytes;
		u32 pa;
		int err;

		pa = sg_phys(sg);
		bytes = sg_dma_len(sg);

		BUG_ON(bytes != PAGE_SIZE);

		err = ioremap_page(va,  pa, mtype);
		if (err)
			goto err_out;

		va += bytes;
	}

	flush_cache_vmap((unsigned long)new->addr,
				(unsigned long)(new->addr + total));
	return new->addr;

err_out:
	WARN_ON(1); /* FIXME: cleanup some mpu mappings */
	vunmap(new->addr);
	return ERR_PTR(-EAGAIN);
}

static inline void vunmap_sg(const void *va)
{
	vunmap(va);
}

static struct iovm_struct *__find_iovm_area(struct iommu *obj, const u32 da)
{
	struct iovm_struct *tmp;

	list_for_each_entry(tmp, &obj->mmap, list) {
		if ((da >= tmp->da_start) && (da < tmp->da_end)) {
			size_t len;

			len = tmp->da_end - tmp->da_start;

			dev_dbg(obj->dev, "%s: %08x-%08x-%08x(%x) %08x\n",
				__func__, tmp->da_start, da, tmp->da_end, len,
				tmp->flags);

			return tmp;
		}
	}

	return NULL;
}

/**
 * find_iovm_area  -  find iovma which includes @da
 * @da:		iommu device virtual address
 *
 * Find the existing iovma starting at @da
 */
struct iovm_struct *find_iovm_area(struct iommu *obj, u32 da)
{
	struct iovm_struct *area;

	mutex_lock(&obj->mmap_lock);
	area = __find_iovm_area(obj, da);
	mutex_unlock(&obj->mmap_lock);

	return area;
}
EXPORT_SYMBOL_GPL(find_iovm_area);

/*
 * This finds the hole(area) which fits the requested address and len
 * in iovmas mmap, and returns the new allocated iovma.
 */
static struct iovm_struct *alloc_iovm_area(struct iommu *obj, u32 da,
					   size_t bytes, u32 flags)
{
	struct iovm_struct *new, *tmp;
	u32 start, prev_end, alignement;

	if (!obj || !bytes)
		return ERR_PTR(-EINVAL);
	start = da;
	alignement = PAGE_SIZE;
	if (flags & IOVMF_DA_ANON) {
		/*
		 * Reserve the first page for NULL
		 */
		start = PAGE_SIZE;
		if (flags & IOVMF_LINEAR)
			alignement = iopgsz_max(bytes);
		start = roundup(start, alignement);
	}
	tmp = NULL;
	if (list_empty(&obj->mmap))
		goto found;
	prev_end = 0;
	list_for_each_entry(tmp, &obj->mmap, list) {

		if ((prev_end <= start) && (start + bytes < tmp->da_start))
			goto found;

		if (flags & IOVMF_DA_ANON)
			start = roundup(tmp->da_end, alignement);

		prev_end = tmp->da_end;
	}

	if ((start > prev_end) && (ULONG_MAX - start >= bytes))
		goto found;

	dev_dbg(obj->dev, "%s: no space to fit %08x(%x) flags: %08x\n",
		__func__, da, bytes, flags);

	return ERR_PTR(-EINVAL);

found:
	new = kmem_cache_zalloc(iovm_area_cachep, GFP_KERNEL);
	if (!new)
		return ERR_PTR(-ENOMEM);

	new->iommu = obj;
	new->da_start = start;
	new->da_end = start + bytes;
	new->flags = flags;

	/*
	 * keep ascending order of iovmas
	 */
	if (tmp)
		list_add_tail(&new->list, &tmp->list);
	else
		list_add(&new->list, &obj->mmap);

	dev_dbg(obj->dev, "%s: found %08x-%08x-%08x(%x) %08x\n",
		__func__, new->da_start, start, new->da_end, bytes, flags);

	return new;
}

static void free_iovm_area(struct iommu *obj, struct iovm_struct *area)
{
	size_t bytes;

	BUG_ON(!obj || !area);

	bytes = area->da_end - area->da_start;

	dev_dbg(obj->dev, "%s: %08x-%08x(%x) %08x\n",
		__func__, area->da_start, area->da_end, bytes, area->flags);

	list_del(&area->list);
	kmem_cache_free(iovm_area_cachep, area);
}

/**
 * da_to_va - convert (d) to (v)
 * @obj:	objective iommu
 * @da:		iommu device virtual address
 * @va:		mpu virtual address
 *
 * Returns mpu virtual addr which corresponds to a given device virtual addr
 */
void *da_to_va(struct iommu *obj, u32 da)
{
	void *va = NULL;
	struct iovm_struct *area;

	mutex_lock(&obj->mmap_lock);

	area = __find_iovm_area(obj, da);
	if (!area) {
		dev_dbg(obj->dev, "%s: no da area(%08x)\n", __func__, da);
		goto out;
	}
	va = area->va;
out:
	mutex_unlock(&obj->mmap_lock);

	return va;
}
EXPORT_SYMBOL_GPL(da_to_va);

static void sgtable_fill_vmalloc(struct sg_table *sgt, void *_va)
{
	unsigned int i;
	struct scatterlist *sg;
	void *va = _va;
	void *va_end;

	for_each_sg(sgt->sgl, sg, sgt->nents, i) {
		struct page *pg;
		const size_t bytes = PAGE_SIZE;

		/*
		 * iommu 'superpage' isn't supported with 'iommu_vmalloc()'
		 */
		pg = vmalloc_to_page(va);
		BUG_ON(!pg);
		sg_set_page(sg, pg, bytes, 0);

		va += bytes;
	}

	va_end = _va + PAGE_SIZE * i;
}

static inline void sgtable_drain_vmalloc(struct sg_table *sgt)
{
	/*
	 * Actually this is not necessary at all, just exists for
	 * consistency of the code readability.
	 */
	BUG_ON(!sgt);
}

static void sgtable_fill_kmalloc(struct sg_table *sgt, u32 pa, size_t len)
{
	unsigned int i;
	struct scatterlist *sg;
	void *va;

	va = phys_to_virt(pa);

	for_each_sg(sgt->sgl, sg, sgt->nents, i) {
		size_t bytes;

		bytes = iopgsz_max(len);

		BUG_ON(!iopgsz_ok(bytes));

		sg_set_buf(sg, phys_to_virt(pa), bytes);
		/*
		 * 'pa' is cotinuous(linear).
		 */
		pa += bytes;
		len -= bytes;
	}
	BUG_ON(len);
}

static inline void sgtable_drain_kmalloc(struct sg_table *sgt)
{
	/*
	 * Actually this is not necessary at all, just exists for
	 * consistency of the code readability
	 */
	BUG_ON(!sgt);
}

/* create 'da' <-> 'pa' mapping from 'sgt' */
static int map_iovm_area(struct iommu *obj, struct iovm_struct *new,
			 const struct sg_table *sgt, u32 flags)
{
	int err;
	unsigned int i, j;
	struct scatterlist *sg;
	u32 da = new->da_start;

	if (!obj || !sgt)
		return -EINVAL;

	BUG_ON(!sgtable_ok(sgt));

	for_each_sg(sgt->sgl, sg, sgt->nents, i) {
		u32 pa;
		int pgsz;
		size_t bytes;
		struct iotlb_entry e;

		pa = sg_phys(sg);
		bytes = sg_dma_len(sg);

		flags &= ~IOVMF_PGSZ_MASK;
		pgsz = bytes_to_iopgsz(bytes);
		if (pgsz < 0)
			goto err_out;
		flags |= pgsz;

		pr_debug("%s: [%d] %08x %08x(%x)\n", __func__,
			 i, da, pa, bytes);

		iotlb_init_entry(&e, da, pa, flags);
		err = iopgtable_store_entry(obj, &e);
		if (err)
			goto err_out;

		da += bytes;
	}
	return 0;

err_out:
	da = new->da_start;

	for_each_sg(sgt->sgl, sg, i, j) {
		size_t bytes;

		bytes = iopgtable_clear_entry(obj, da);

		BUG_ON(!iopgsz_ok(bytes));

		da += bytes;
	}
	return err;
}

/* release 'da' <-> 'pa' mapping */
static void unmap_iovm_area(struct iommu *obj, struct iovm_struct *area)
{
	u32 start;
	size_t total = area->da_end - area->da_start;

	BUG_ON((!total) || !IS_ALIGNED(total, PAGE_SIZE));

	start = area->da_start;
	while (total > 0) {
		size_t bytes;

		bytes = iopgtable_clear_entry(obj, start);
		if (bytes == 0)
			bytes = PAGE_SIZE;
		else
			dev_dbg(obj->dev, "%s: unmap %08x(%x) %08x\n",
				__func__, start, bytes, area->flags);

		BUG_ON(!IS_ALIGNED(bytes, PAGE_SIZE));

		total -= bytes;
		start += bytes;
	}
	BUG_ON(total);
}

/* template function for all unmapping */
static struct sg_table *unmap_vm_area(struct iommu *obj, const u32 da,
				      void (*fn)(const void *), u32 flags)
{
	struct sg_table *sgt = NULL;
	struct iovm_struct *area;

	if (!IS_ALIGNED(da, PAGE_SIZE)) {
		dev_err(obj->dev, "%s: alignment err(%08x)\n", __func__, da);
		return NULL;
	}

	mutex_lock(&obj->mmap_lock);

	area = __find_iovm_area(obj, da);
	if (!area) {
		dev_dbg(obj->dev, "%s: no da area(%08x)\n", __func__, da);
		goto out;
	}

	if ((area->flags & flags) != flags) {
		dev_err(obj->dev, "%s: wrong flags(%08x)\n", __func__,
			area->flags);
		goto out;
	}
	sgt = (struct sg_table *)area->sgt;

	unmap_iovm_area(obj, area);

	fn(area->va);

	dev_dbg(obj->dev, "%s: %08x-%08x-%08x(%x) %08x\n", __func__,
		area->da_start, da, area->da_end,
		area->da_end - area->da_start, area->flags);

	free_iovm_area(obj, area);
out:
	mutex_unlock(&obj->mmap_lock);

	return sgt;
}

static u32 map_iommu_region(struct iommu *obj, u32 da,
	      const struct sg_table *sgt, void *va, size_t bytes, u32 flags)
{
	int err = -ENOMEM;
	struct iovm_struct *new;

	mutex_lock(&obj->mmap_lock);

	new = alloc_iovm_area(obj, da, bytes, flags);
	if (IS_ERR(new)) {
		err = PTR_ERR(new);
		goto err_alloc_iovma;
	}
	new->va = va;
	new->sgt = sgt;

	if (map_iovm_area(obj, new, sgt, new->flags))
		goto err_map;

	mutex_unlock(&obj->mmap_lock);

	dev_dbg(obj->dev, "%s: da:%08x(%x) flags:%08x va:%p\n",
		__func__, new->da_start, bytes, new->flags, va);

	return new->da_start;

err_map:
	free_iovm_area(obj, new);
err_alloc_iovma:
	mutex_unlock(&obj->mmap_lock);
	return err;
}

static inline u32 __iommu_vmap(struct iommu *obj, u32 da,
		 const struct sg_table *sgt, void *va, size_t bytes, u32 flags)
{
	return map_iommu_region(obj, da, sgt, va, bytes, flags);
}

/**
 * iommu_vmap  -  (d)-(p)-(v) address mapper
 * @obj:	objective iommu
 * @sgt:	address of scatter gather table
 * @flags:	iovma and page property
 *
 * Creates 1-n-1 mapping with given @sgt and returns @da.
 * All @sgt element must be io page size aligned.
 */
u32 iommu_vmap(struct iommu *obj, u32 da, const struct sg_table *sgt,
		 u32 flags)
{
	size_t bytes;
	void *va = NULL;

	if (!obj || !obj->dev || !sgt)
		return -EINVAL;

	bytes = sgtable_len(sgt);
	if (!bytes)
		return -EINVAL;
	bytes = PAGE_ALIGN(bytes);

	if (flags & IOVMF_MMIO) {
		va = vmap_sg(sgt);
		if (IS_ERR(va))
			return PTR_ERR(va);
	}

	flags &= IOVMF_HW_MASK;
	flags |= IOVMF_DISCONT;
	flags |= IOVMF_MMIO;
	flags |= (da ? IOVMF_DA_FIXED : IOVMF_DA_ANON);

	da = __iommu_vmap(obj, da, sgt, va, bytes, flags);
	if (IS_ERR_VALUE(da))
		vunmap_sg(va);

	return da;
}
EXPORT_SYMBOL_GPL(iommu_vmap);

/**
 * iommu_vunmap  -  release virtual mapping obtained by 'iommu_vmap()'
 * @obj:	objective iommu
 * @da:		iommu device virtual address
 *
 * Free the iommu virtually contiguous memory area starting at
 * @da, which was returned by 'iommu_vmap()'.
 */
struct sg_table *iommu_vunmap(struct iommu *obj, u32 da)
{
	struct sg_table *sgt;
	/*
	 * 'sgt' is allocated before 'iommu_vmalloc()' is called.
	 * Just returns 'sgt' to the caller to free
	 */
	sgt = unmap_vm_area(obj, da, vunmap_sg, IOVMF_DISCONT | IOVMF_MMIO);
	if (!sgt)
		dev_dbg(obj->dev, "%s: No sgt\n", __func__);
	return sgt;
}
EXPORT_SYMBOL_GPL(iommu_vunmap);

/**
 * iommu_vmalloc  -  (d)-(p)-(v) address allocator and mapper
 * @obj:	objective iommu
 * @da:		contiguous iommu virtual memory
 * @bytes:	allocation size
 * @flags:	iovma and page property
 *
 * Allocate @bytes linearly and creates 1-n-1 mapping and returns
 * @da again, which might be adjusted if 'IOVMF_DA_ANON' is set.
 */
u32 iommu_vmalloc(struct iommu *obj, u32 da, size_t bytes, u32 flags)
{
	void *va;
	struct sg_table *sgt;

	if (!obj || !obj->dev || !bytes)
		return -EINVAL;

	bytes = PAGE_ALIGN(bytes);

	va = vmalloc(bytes);
	if (!va)
		return -ENOMEM;

	sgt = sgtable_alloc(bytes, flags);
	if (IS_ERR(sgt)) {
		da = PTR_ERR(sgt);
		goto err_sgt_alloc;
	}
	sgtable_fill_vmalloc(sgt, va);

	flags &= IOVMF_HW_MASK;
	flags |= IOVMF_DISCONT;
	flags |= IOVMF_ALLOC;
	flags |= (da ? IOVMF_DA_FIXED : IOVMF_DA_ANON);

	da = __iommu_vmap(obj, da, sgt, va, bytes, flags);
	if (IS_ERR_VALUE(da))
		goto err_iommu_vmap;

	return da;

err_iommu_vmap:
	sgtable_drain_vmalloc(sgt);
	sgtable_free(sgt);
err_sgt_alloc:
	vfree(va);
	return da;
}
EXPORT_SYMBOL_GPL(iommu_vmalloc);

/**
 * iommu_vfree  -  release memory allocated by 'iommu_vmalloc()'
 * @obj:	objective iommu
 * @da:		iommu device virtual address
 *
 * Frees the iommu virtually continuous memory area starting at
 * @da, as obtained from 'iommu_vmalloc()'.
 */
void iommu_vfree(struct iommu *obj, const u32 da)
{
	struct sg_table *sgt;

	sgt = unmap_vm_area(obj, da, vfree, IOVMF_DISCONT | IOVMF_ALLOC);
	if (!sgt)
		dev_dbg(obj->dev, "%s: No sgt\n", __func__);
	sgtable_free(sgt);
}
EXPORT_SYMBOL_GPL(iommu_vfree);

static u32 __iommu_kmap(struct iommu *obj, u32 da, u32 pa, void *va,
			  size_t bytes, u32 flags)
{
	struct sg_table *sgt;

	sgt = sgtable_alloc(bytes, flags);
	if (IS_ERR(sgt))
		return PTR_ERR(sgt);

	sgtable_fill_kmalloc(sgt, pa, bytes);

	da = map_iommu_region(obj, da, sgt, va, bytes, flags);
	if (IS_ERR_VALUE(da)) {
		sgtable_drain_kmalloc(sgt);
		sgtable_free(sgt);
	}

	return da;
}

/**
 * iommu_kmap  -  (d)-(p)-(v) address mapper
 * @obj:	objective iommu
 * @da:		contiguous iommu virtual memory
 * @pa:		contiguous physical memory
 * @flags:	iovma and page property
 *
 * Creates 1-1-1 mapping and returns @da again, which can be
 * adjusted if 'IOVMF_DA_ANON' is set.
 */
u32 iommu_kmap(struct iommu *obj, u32 da, u32 pa, size_t bytes,
		 u32 flags)
{
	void *va;

	if (!obj || !obj->dev || !bytes)
		return -EINVAL;

	bytes = PAGE_ALIGN(bytes);

	va = ioremap(pa, bytes);
	if (!va)
		return -ENOMEM;

	flags &= IOVMF_HW_MASK;
	flags |= IOVMF_LINEAR;
	flags |= IOVMF_MMIO;
	flags |= (da ? IOVMF_DA_FIXED : IOVMF_DA_ANON);

	da = __iommu_kmap(obj, da, pa, va, bytes, flags);
	if (IS_ERR_VALUE(da))
		iounmap(va);

	return da;
}
EXPORT_SYMBOL_GPL(iommu_kmap);

/**
 * iommu_kunmap  -  release virtual mapping obtained by 'iommu_kmap()'
 * @obj:	objective iommu
 * @da:		iommu device virtual address
 *
 * Frees the iommu virtually contiguous memory area starting at
 * @da, which was passed to and was returned by'iommu_kmap()'.
 */
void iommu_kunmap(struct iommu *obj, u32 da)
{
	struct sg_table *sgt;
	typedef void (*func_t)(const void *);

	sgt = unmap_vm_area(obj, da, (func_t)__iounmap,
			    IOVMF_LINEAR | IOVMF_MMIO);
	if (!sgt)
		dev_dbg(obj->dev, "%s: No sgt\n", __func__);
	sgtable_free(sgt);
}
EXPORT_SYMBOL_GPL(iommu_kunmap);

/**
 * iommu_kmalloc  -  (d)-(p)-(v) address allocator and mapper
 * @obj:	objective iommu
 * @da:		contiguous iommu virtual memory
 * @bytes:	bytes for allocation
 * @flags:	iovma and page property
 *
 * Allocate @bytes linearly and creates 1-1-1 mapping and returns
 * @da again, which might be adjusted if 'IOVMF_DA_ANON' is set.
 */
u32 iommu_kmalloc(struct iommu *obj, u32 da, size_t bytes, u32 flags)
{
	void *va;
	u32 pa;

	if (!obj || !obj->dev || !bytes)
		return -EINVAL;

	bytes = PAGE_ALIGN(bytes);

	va = kmalloc(bytes, GFP_KERNEL | GFP_DMA);
	if (!va)
		return -ENOMEM;
	pa = virt_to_phys(va);

	flags &= IOVMF_HW_MASK;
	flags |= IOVMF_LINEAR;
	flags |= IOVMF_ALLOC;
	flags |= (da ? IOVMF_DA_FIXED : IOVMF_DA_ANON);

	da = __iommu_kmap(obj, da, pa, va, bytes, flags);
	if (IS_ERR_VALUE(da))
		kfree(va);

	return da;
}
EXPORT_SYMBOL_GPL(iommu_kmalloc);

/**
 * iommu_kfree  -  release virtual mapping obtained by 'iommu_kmalloc()'
 * @obj:	objective iommu
 * @da:		iommu device virtual address
 *
 * Frees the iommu virtually contiguous memory area starting at
 * @da, which was passed to and was returned by'iommu_kmalloc()'.
 */
void iommu_kfree(struct iommu *obj, u32 da)
{
	struct sg_table *sgt;

	sgt = unmap_vm_area(obj, da, kfree, IOVMF_LINEAR | IOVMF_ALLOC);
	if (!sgt)
		dev_dbg(obj->dev, "%s: No sgt\n", __func__);
	sgtable_free(sgt);
}
EXPORT_SYMBOL_GPL(iommu_kfree);

static int __init iovmm_init(void)
{
	const unsigned long flags = SLAB_HWCACHE_ALIGN;
	struct kmem_cache *p;
	int num, ret;

	p = kmem_cache_create("iovm_area_cache", sizeof(struct iovm_struct), 0,
			      flags, NULL);
	if (!p)
		return -ENOMEM;

	iovm_area_cachep = p;
	num = iommu_get_plat_data_size();
	ret = alloc_chrdev_region(&omap_iovmm_dev, 0, num, OMAP_IOVMM_NAME);
	if (ret) {
		pr_err("%s: alloc_chrdev_region failed: %d\n", __func__, ret);
		goto out;
	}
	omap_iovmm_class = class_create(THIS_MODULE, OMAP_IOVMM_NAME);
	if (IS_ERR(omap_iovmm_class)) {
		ret = PTR_ERR(omap_iovmm_class);
		pr_err("%s: class_create failed: %d\n", __func__, ret);
		goto unreg_region;
	}
	atomic_set(&num_of_iovmmus, 0);

	return platform_driver_register(&omap_iovmm_driver);
unreg_region:
	unregister_chrdev_region(omap_iovmm_dev, num);
out:
	return ret;
}
module_init(iovmm_init);

static void __exit iovmm_exit(void)
{
	kmem_cache_destroy(iovm_area_cachep);
}
module_exit(iovmm_exit);

MODULE_DESCRIPTION("omap iommu: simple virtual address space management");
MODULE_AUTHOR("Hiroshi DOYU <Hiroshi.DOYU@nokia.com>");
MODULE_LICENSE("GPL v2");
