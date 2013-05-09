/*
 * Remote Processor Framework
 *
 * Copyright (C) 2011 Texas Instruments, Inc.
 * Copyright (C) 2011 Google, Inc.
 *
 * Ohad Ben-Cohen <ohad@wizery.com>
 * Brian Swetland <swetland@google.com>
 * Mark Grosen <mgrosen@ti.com>
 * Fernando Guzman Lugo <fernando.lugo@ti.com>
 * Suman Anna <s-anna@ti.com>
 * Robert Tivy <rtivy@ti.com>
 * Armando Uribe De Leon <x0095078@ti.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#define pr_fmt(fmt)    "%s: " fmt, __func__

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/dma-mapping.h>
#include <linux/firmware.h>
#include <linux/string.h>
#include <linux/debugfs.h>
#include <linux/remoteproc.h>
#include <linux/iommu.h>
#include <linux/klist.h>
#include <linux/elf.h>
#include <linux/virtio_ids.h>
#include <linux/virtio_ring.h>
#include <asm/byteorder.h>
#include <linux/pm_runtime.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/vmalloc.h>
#include <linux/rproc_drm.h>

#include "remoteproc_internal.h"

#define DEFAULT_AUTOSUSPEND_TIMEOUT 5000

#define dev_to_rproc(dev) container_of(dev, struct rproc, dev)

static void klist_rproc_get(struct klist_node *n);
static void klist_rproc_put(struct klist_node *n);

/*
 * klist of the available remote processors.
 *
 * We need this in order to support name-based lookups (needed by the
 * rproc_get_by_name()).
 *
 * That said, we don't use rproc_get_by_name() at this point.
 * The use cases that do require its existence should be
 * scrutinized, and hopefully migrated to rproc_boot() using device-based
 * binding.
 *
 * If/when this materializes, we could drop the klist (and the by_name
 * API).
 */
static DEFINE_KLIST(rprocs, klist_rproc_get, klist_rproc_put);

typedef int (*rproc_handle_resources_t)(struct rproc *rproc,
				struct resource_table *table, int len);
typedef int (*rproc_handle_resource_t)(struct rproc *rproc, void *, int avail);

/* Unique numbering for remoteproc devices */
static unsigned int dev_index;

static const char * const rproc_err_names[] = {
	[RPROC_ERR_MMUFAULT]	= "mmufault",
	[RPROC_ERR_EXCEPTION]	= "device exception",
	[RPROC_ERR_WATCHDOG]	= "watchdog fired",
};

static void rproc_activate_iommu(struct rproc *rproc)
{
	struct iommu_domain *domain = rproc->domain;

	dev_dbg(&rproc->dev, "activating iommu\n");
	if (domain)
		iommu_domain_activate(domain);
}

static void rproc_idle_iommu(struct rproc *rproc)
{
	struct iommu_domain *domain = rproc->domain;

	dev_dbg(&rproc->dev, "de-activating iommu\n");
	if (domain)
		iommu_domain_idle(domain);
}

static int rproc_resume(struct device *dev)
{
	struct rproc *rproc = dev_to_rproc(dev);
	int ret = 0;

	dev_dbg(dev, "Enter %s\n", __func__);

	mutex_lock(&rproc->lock);
	/* system suspend has finished */
	rproc->system_suspended = false;
	if (rproc->state != RPROC_SUSPENDED)
		goto out;

	/* resume no needed (already autosuspended when system suspend) */
	if (!rproc->need_resume)
		goto out;

	/*
	 * if a message came after rproc was system suspended, need_resume is
	 * true and if runtime status is suspended in this case  we need to
	 * resume and then runtime status needs to be updated too, thas is easy
	 * acomplish by calling pm_runtime_resume
	 */
	if (pm_runtime_status_suspended(dev)) {
		pm_runtime_resume(dev);
		goto mark;
	}

	/*
	 * if a system suspend request came when rproc was running, the
	 * rproc was forced to go to suspend by the system suspend callback.
	 * However, the rproc runtime status is still active. In this case,
	 * we only need to wake up the rproc, its associated iommu and update
	 * the rproc state
	 */
	rproc_activate_iommu(rproc);
	if (rproc->ops->resume) {
		ret = rproc->ops->resume(rproc);
		if (ret) {
			rproc_idle_iommu(rproc);
			dev_err(dev, "resume failed %d\n", ret);
			goto out;
		}
	}
	rproc->state = RPROC_RUNNING;
mark:
	/* if a resume was needed we need to schedule next autosupend */
	pm_runtime_mark_last_busy(dev);
	pm_runtime_autosuspend(dev);
out:
	mutex_unlock(&rproc->lock);
	return ret;
}

static int rproc_suspend(struct device *dev)
{
	struct rproc *rproc = dev_to_rproc(dev);
	int ret = 0;

	dev_dbg(dev, "Enter %s\n", __func__);

	mutex_lock(&rproc->lock);
	if (rproc->state != RPROC_RUNNING)
		goto out;

	if (rproc->ops->suspend) {
		ret = rproc->ops->suspend(rproc, false);
		if (ret) {
			dev_err(dev, "suspend failed %d\n", ret);
			goto out;
		}
	}
	rproc_idle_iommu(rproc);

	/*
	 * rproc was not already suspended, so we forced the suspend and
	 * therefore we need a resume in the system resume callback
	 */
	rproc->need_resume = 1;
	rproc->state = RPROC_SUSPENDED;
out:
	if (!ret)
		rproc->system_suspended = true;
	mutex_unlock(&rproc->lock);
	return ret;
}

static int rproc_runtime_resume(struct device *dev)
{
	struct rproc *rproc = dev_to_rproc(dev);
	int ret = 0;

	dev_dbg(dev, "Enter %s\n", __func__);

	if (rproc->state != RPROC_SUSPENDED)
		goto out;

	/*
	 * if we are in system suspend we cannot wakeup the rproc yet,
	 * instead set need_resume flag so that rproc can be resumed in
	 * the system resume callback
	 */
	if (rproc->system_suspended) {
		rproc->need_resume = true;
		return -EAGAIN;
	}

	rproc_activate_iommu(rproc);
	if (rproc->ops->resume) {
		ret = rproc->ops->resume(rproc);
		if (ret) {
			rproc_idle_iommu(rproc);
			dev_err(dev, "resume failed %d\n", ret);
			goto out;
		}
	}

	rproc->state = RPROC_RUNNING;
out:
	return ret;
}

static int rproc_runtime_suspend(struct device *dev)
{
	struct rproc *rproc = dev_to_rproc(dev);
	int ret;

	dev_dbg(dev, "Enter %s\n", __func__);

	if (rproc->state != RPROC_RUNNING)
		goto out;

	if (rproc->ops->suspend) {
		ret = rproc->ops->suspend(rproc, true);
		if (ret)
			goto abort;
	}
	rproc_idle_iommu(rproc);

	rproc->state = RPROC_SUSPENDED;
out:
	return 0;
abort:
	dev_dbg(dev, "auto suspend aborted %d\n", ret);
	pm_runtime_mark_last_busy(dev);
	return ret;
}

static int rproc_runtime_idle(struct device *dev)
{
	dev_dbg(dev, "Enter %s\n", __func__);

	/* try to auto suspend when idle is called */
	return pm_runtime_autosuspend(dev);
}

static const struct dev_pm_ops rproc_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(rproc_suspend, rproc_resume)
	SET_RUNTIME_PM_OPS(rproc_runtime_suspend, rproc_runtime_resume,
			rproc_runtime_idle)
};

/* translate rproc_err to string */
static const char *rproc_err_to_string(enum rproc_err type)
{
	if (type < ARRAY_SIZE(rproc_err_names))
		return rproc_err_names[type];
	return "unkown";
}

/**
 * rproc_error_reporter() - rproc error reporter function
 * @rproc: remote processor
 * @type: fatal error
 *
 * This function must be called every time a fatal error is detected
 */
void rproc_error_reporter(struct rproc *rproc, enum rproc_err type)
{
	struct device *dev;

	if (!rproc) {
		pr_err("invalid rproc handle\n");
		return;
	}

	dev = &rproc->dev;

	dev_err(dev, "fatal error #%u detected in %s: error type %s\n",
		++rproc->crash_cnt, rproc->name, rproc_err_to_string(type));

	/*
	 * as this function can be called from a ISR or a atomic context
	 * we need to create a workqueue to handle the error
	 */
	schedule_work(&rproc->error_handler);
}
EXPORT_SYMBOL(rproc_error_reporter);

/*
 * This is the IOMMU fault handler we register with the IOMMU API
 * (when relevant; not all remote processors access memory through
 * an IOMMU).
 *
 * IOMMU core will invoke this handler whenever the remote processor
 * will try to access an unmapped device address.
 *
 * Currently this is mostly a stub, but it will be later used to trigger
 * the recovery of the remote processor.
 */
static int rproc_iommu_fault(struct iommu_domain *domain, struct device *dev,
		unsigned long iova, int flags, void *token)
{
	struct rproc *rproc = token;

	dev_err(dev, "iommu fault: da 0x%lx flags 0x%x\n", iova, flags);

	rproc_error_reporter(rproc, RPROC_ERR_MMUFAULT);

	/*
	 * Let the iommu core know we're not really handling this fault;
	 * we just plan to use this as a recovery trigger.
	 */
	return -ENOSYS;
}

static int rproc_enable_iommu(struct rproc *rproc)
{
	struct iommu_domain *domain;
	struct device *dev = rproc->dev.parent;
	int ret;
	int iommu_sdata[2] = {0, 0};

	/*
	 * We currently use iommu_present() to decide if an IOMMU
	 * setup is needed.
	 *
	 * This works for simple cases, but will easily fail with
	 * platforms that do have an IOMMU, but not for this specific
	 * rproc.
	 *
	 * This will be easily solved by introducing hw capabilities
	 * that will be set by the remoteproc driver.
	 */
	if (!iommu_present(dev->bus)) {
		dev_dbg(dev, "iommu not found\n");
		return 0;
	}

	domain = iommu_domain_alloc(dev->bus);
	if (!domain) {
		dev_err(dev, "can't alloc iommu domain\n");
		return -ENOMEM;
	}

	iommu_set_fault_handler(domain, rproc_iommu_fault, rproc);
	iommu_sdata[0] = rproc_secure_get_mode(rproc);
	iommu_sdata[1] = rproc_secure_get_ttb(rproc);
	ret = iommu_domain_add_iommudata(domain, iommu_sdata,
						sizeof(iommu_sdata));
	if (ret) {
		dev_err(dev, "can't add iommu secure data: %d\n", ret);
		goto free_domain;
	}

	ret = iommu_attach_device(domain, dev);
	if (ret) {
		dev_err(dev, "can't attach iommu device: %d\n", ret);
		goto free_domain;
	}

	rproc->domain = domain;

	return 0;

free_domain:
	iommu_domain_free(domain);
	return ret;
}

static void rproc_disable_iommu(struct rproc *rproc)
{
	struct iommu_domain *domain = rproc->domain;
	struct device *dev = rproc->dev.parent;

	if (!domain)
		return;

	iommu_detach_device(domain, dev);
	iommu_domain_free(domain);

	return;
}

static int rproc_program_iommu(struct rproc *rproc)
{
	struct rproc_mem_entry *entry;
	int ret = 0;
	int smode = rproc_secure_get_mode(rproc);

	if (smode)
		return 0;

	list_for_each_entry(entry, &rproc->mappings, node) {
		/*
		 * no point in handling devmem resource without
		 * a valid iommu domain
		 */
		if (entry->type == RSC_DEVMEM && !rproc->domain) {
			ret = -EINVAL;
			goto out;
		}
		if (entry->type == RSC_CARVEOUT && !rproc->domain)
			continue;

		/* should never hit this, add this for sanity */
		if (entry->mapped) {
			WARN_ON(1);
			continue;
		}

		/* cleanup will be taken care in rproc_resource_cleanup */
		ret = iommu_map(rproc->domain, entry->da, entry->dma,
						entry->len, entry->flags);
		if (ret) {
			dev_err(&rproc->dev, "iommu_map failed: %d\n", ret);
			goto out;
		}
		entry->mapped = true;
		dev_dbg(&rproc->dev,
				"mapped entry pa 0x%x, da 0x%x, len 0x%x\n",
				entry->dma, entry->da, entry->len);
	}

out:
	return ret;
}

/*
 * Some remote processors will ask us to allocate them physically contiguous
 * memory regions (which we call "carveouts"), and map them to specific
 * device addresses (which are hardcoded in the firmware).
 *
 * They may then ask us to copy objects into specific device addresses (e.g.
 * code/data sections) or expose us certain symbols in other device address
 * (e.g. their trace buffer).
 *
 * This function is a helper function with which we can go over the allocated
 * carveouts and translate specific device address to kernel virtual addresses
 * so we can access the referenced memory.
 *
 * This function is exported so that it can also be used by individual remote
 * processor driver implementations to convert a device address into the
 * corresponding kernel virtual address that it can operate on.
 *
 * Note: phys_to_virt(iommu_iova_to_phys(rproc->domain, da)) will work too,
 * but only on kernel direct mapped RAM memory. Instead, we're just using
 * here the output of the DMA API, which should be more correct.
 */
void *rproc_da_to_va(struct rproc *rproc, u64 da, int len)
{
	struct rproc_mem_entry *carveout;
	void *ptr = NULL;

	list_for_each_entry(carveout, &rproc->carveouts, node) {
		int offset = da - carveout->da;

		/* try next carveout if da is too small */
		if (offset < 0)
			continue;

		/* try next carveout if da is too large */
		if (offset + len > carveout->len)
			continue;

		ptr = carveout->va + offset;

		break;
	}

	return ptr;
}
EXPORT_SYMBOL(rproc_da_to_va);

int rproc_pa_to_da(struct rproc *rproc, phys_addr_t pa, u64 *da)
{
	int ret = -EINVAL;
	struct rproc_mem_entry *maps = NULL;

	if (!rproc || !da)
		return -EINVAL;

	if (mutex_lock_interruptible(&rproc->lock))
		return -EINTR;

	if (rproc->state == RPROC_RUNNING || rproc->state == RPROC_SUSPENDED) {
		/* Look in the mappings first */
		list_for_each_entry(maps, &rproc->mappings, node) {
			if (pa >= maps->dma && pa < (maps->dma + maps->len)) {
				*da = maps->da + (pa - maps->dma);
				ret = 0;
				goto exit;
			}
		}
		/* If not, check in the carveouts */
		list_for_each_entry(maps, &rproc->carveouts, node) {
			if (pa >= maps->dma && pa < (maps->dma + maps->len)) {
				*da = maps->da + (pa - maps->dma);
				ret = 0;
				break;
			}
		}

	}
exit:
	mutex_unlock(&rproc->lock);
	return ret;

}
EXPORT_SYMBOL(rproc_pa_to_da);


/**
 * rproc_load_segments() - load firmware segments to memory
 * @rproc: remote processor which will be booted using these fw segments
 * @elf_data: the content of the ELF firmware image
 * @len: firmware size (in bytes)
 *
 * This function loads the firmware segments to memory, where the remote
 * processor expects them.
 *
 * Some remote processors will expect their code and data to be placed
 * in specific device addresses, and can't have them dynamically assigned.
 *
 * We currently support only those kind of remote processors, and expect
 * the program header's paddr member to contain those addresses. We then go
 * through the physically contiguous "carveout" memory regions which we
 * allocated (and mapped) earlier on behalf of the remote processor,
 * and "translate" device address to kernel addresses, so we can copy the
 * segments where they are expected.
 *
 * Currently we only support remote processors that required carveout
 * allocations and got them mapped onto their iommus. Some processors
 * might be different: they might not have iommus, and would prefer to
 * directly allocate memory for every segment/resource. This is not yet
 * supported, though.
 */
static int
rproc_load_segments(struct rproc *rproc, const u8 *elf_data, size_t len)
{
	struct device *dev = &rproc->dev;
	struct elf32_hdr *ehdr;
	struct elf32_phdr *phdr;
	int i, ret = 0;

	ehdr = (struct elf32_hdr *)elf_data;
	phdr = (struct elf32_phdr *)(elf_data + ehdr->e_phoff);

	/* go through the available ELF segments */
	for (i = 0; i < ehdr->e_phnum; i++, phdr++) {
		u32 da = phdr->p_paddr;
		u32 memsz = phdr->p_memsz;
		u32 filesz = phdr->p_filesz;
		u32 offset = phdr->p_offset;
		void *ptr;

		if (phdr->p_type != PT_LOAD)
			continue;

		dev_dbg(dev, "phdr: type %d da 0x%x memsz 0x%x filesz 0x%x\n",
					phdr->p_type, da, memsz, filesz);

		if (filesz > memsz) {
			dev_err(dev, "bad phdr filesz 0x%x memsz 0x%x\n",
							filesz, memsz);
			ret = -EINVAL;
			break;
		}

		if (offset + filesz > len) {
			dev_err(dev, "truncated fw: need 0x%x avail 0x%zx\n",
					offset + filesz, len);
			ret = -EINVAL;
			break;
		}

		/* grab the kernel address for this device address */
		ptr = rproc_da_to_va(rproc, da, memsz);
		if (!ptr) {
			dev_err(dev, "bad phdr da 0x%x mem 0x%x\n", da, memsz);
			ret = -EINVAL;
			break;
		}

		/* put the segment where the remote processor expects it */
		if (phdr->p_filesz)
			memcpy(ptr, elf_data + phdr->p_offset, filesz);

		/*
		 * Zero out remaining memory for this segment.
		 *
		 * This isn't strictly required since dma_alloc_coherent already
		 * did this for us. albeit harmless, we may consider removing
		 * this.
		 */
		if (memsz > filesz)
			memset(ptr + filesz, 0, memsz - filesz);
	}

	return ret;
}

static void rproc_reset_poolmem(struct rproc *rproc)
{
	struct rproc_mem_pool *pool = rproc->memory_pool;

	if (!pool || !pool->mem_base || !pool->mem_size) {
		pr_warn("invalid pool\n");
		return;
	}

	pool->cur_ipc_base = pool->mem_base;
	pool->cur_ipc_size = SZ_256K;
	pool->cur_ipcbuf_base = pool->cur_ipc_base + pool->cur_ipc_size;
	pool->cur_ipcbuf_size = SZ_1M - pool->cur_ipc_size;
	pool->cur_fw_base = pool->mem_base + SZ_1M;
	pool->cur_fw_size = pool->mem_size - SZ_1M;
}

/**
 * rproc_alloc_poolmem() - allocate from the rproc carveout pool
 * @rproc: remote processor to allocate the memory for
 * @size: size of the allocation
 * @pa: pointer to return the physical address of the allocated segment
 * @type: segment type to identify the carveout range to allocate from
 *
 * This function is the basic allocator function from the rproc carveout
 * pool. The pool itself is divided into 3 different regions currently,
 * one for the rproc shared vring objects, another for the vring data
 * buffers and the last for all the firmware segments. The regions are
 * partitioned to mimic the current ranges of the CMA allocated addresses
 * for OMAP so that the same firmware can be used.
 */
static int
rproc_alloc_poolmem(struct rproc *rproc, u32 size, dma_addr_t *pa, int type)
{
	struct rproc_mem_pool *pool = rproc->memory_pool;
	size_t align_size = PAGE_SIZE << get_order(size);
	size_t max_cma_align = PAGE_SIZE << CONFIG_CMA_ALIGNMENT;
	size_t offset;

	/* enforce the max alignment, aligned with CMA */
	if (align_size > max_cma_align)
		align_size = max_cma_align;
	else
		size = align_size;

	*pa = 0;
	if (!pool || !pool->mem_base || !pool->mem_size) {
		pr_warn("invalid pool\n");
		return -EINVAL;
	}

	switch (type) {
	case RPROC_MEM_IPC_BUF:
		if (pool->cur_ipcbuf_size < size) {
			pr_warn("out of carveout memory\n");
			return -ENOMEM;
		}

		offset = ALIGN(pool->cur_ipcbuf_base, align_size) -
							pool->cur_ipcbuf_base;
		*pa = pool->cur_ipcbuf_base + offset;
		pool->cur_ipcbuf_base += (size + offset);
		pool->cur_ipcbuf_size -= (size + offset);
		break;
	case RPROC_MEM_IPC:
		if (pool->cur_ipc_size < size) {
			pr_warn("out of carveout memory\n");
			return -ENOMEM;
		}

		offset = ALIGN(pool->cur_ipc_base, align_size) -
							pool->cur_ipc_base;
		*pa = pool->cur_ipc_base + offset;
		pool->cur_ipc_base += (size + offset);
		pool->cur_ipc_size -= (size + offset);
		break;
	case RPROC_MEM_FW:
		if (pool->cur_fw_size < size) {
			pr_warn("out of carveout memory\n");
			return -ENOMEM;
		}

		offset = ALIGN(pool->cur_fw_base, align_size) -
							pool->cur_fw_base;
		*pa = pool->cur_fw_base + offset;
		pool->cur_fw_base += (size + offset);
		pool->cur_fw_size -= (size + offset);
		break;
	default:
		/* do nothing */
		break;
	}
	return 0;
}

/**
 * rproc_alloc_memory() - memory allocator for rproc segments
 * @rproc: remote processor to allocate the memory for
 * @size: size of the allocation
 * @pa: pointer to return the physical address of the allocated segment
 * @type: segment type to identify the carveout range to allocate from
 *
 * This function is the primary allocator function that needs to be used
 * for allocating memory for different rproc firmware segments. It provides
 * an unified interface and allocated memory from either the default CMA
 * pool or the carveout pool associated with the rproc device. The carveout
 * allocation gives out both the physical as well as the equivalent kernel
 * virtual address.
 *
 * Note: The kernel virtual address returned in the case of a carveout pool
 * may not exactly meet the same criteria as the virtual address returned
 * from the dma_alloc_coherent API. Any kernel functions trying to leverage
 * this address for virt to phys conversions or preparing sg lists need to
 * be careful.
 */
void *rproc_alloc_memory(struct rproc *rproc, u32 size, dma_addr_t *dma,
								int type)
{
	void *va = NULL;
	struct device *dev = &rproc->dev;

	/* use a carveout pool only if the rproc is configured so */
	if (rproc->memory_pool) {
		int ret = rproc_alloc_poolmem(rproc, size, dma, type);
		if (ret) {
			dev_err(dev, "rproc_alloc_poolmem failed 0x%x\n", size);
			goto out;
		}

		/* ioremaping normal memory, so make sparse happy */
		va = (__force void *) ioremap((unsigned long)(*dma), size);
		if (!va) {
			dev_err(dev, "iomap error: (phys 0x%08x size 0x%x)\n",
						(u32)(*dma), size);
			goto out;
		}
	} else {
		va = dma_alloc_coherent(dev->parent, size, dma, GFP_KERNEL);
		if (!va) {
			dev_err(dev, "dma_alloc_coherent failed: size==0x%x, dma==0x%x\n", size, dma);
			goto out;
		}
	}

out:
	return va;
}

void rproc_free_memory(struct rproc *rproc, u32 size, void *va, dma_addr_t dma)
{
	struct device *dev = &rproc->dev;

	if (rproc->memory_pool) {
		/*
		 * just unmap it, the pool need not be managed, it would
		 * simply just need to be reset. normal memory is being
		 * unmapped, so make sparse also happy.
		 */
		iounmap((__force void __iomem *)va);
	} else {
		dma_free_coherent(dev->parent, size, va, dma);
	}
}

int rproc_alloc_vring(struct rproc_vdev *rvdev, int i)
{
	struct rproc *rproc = rvdev->rproc;
	struct device *dev = &rproc->dev;
	struct rproc_vring *vring = &rvdev->vring[i];
	dma_addr_t dma;
	void *va;
	int ret, size, notifyid;

	/* actual size of vring (in bytes) */
	size = PAGE_ALIGN(vring_size(vring->len, vring->align));

	if (!idr_pre_get(&rproc->notifyids, GFP_KERNEL)) {
		dev_err(dev, "idr_pre_get failed\n");
		return -ENOMEM;
	}

	/*
	 * Allocate non-cacheable memory for the vring. In the future
	 * this call will also configure the IOMMU for us
	 * TODO: let the rproc know the da of this vring
	 */
	va = rproc_alloc_memory(rproc, size, &dma, RPROC_MEM_IPC);
	if (!va) {
		dev_err(dev, "rproc_alloc_memory failed\n");
		return -ENOMEM;
	}

	/*
	 * Assign an rproc-wide unique index for this vring
	 * TODO: assign a notifyid for rvdev updates as well
	 * TODO: let the rproc know the notifyid of this vring
	 * TODO: support predefined notifyids (via resource table)
	 */
	ret = idr_get_new(&rproc->notifyids, vring, &notifyid);
	if (ret) {
		dev_err(dev, "idr_get_new failed: %d\n", ret);
		rproc_free_memory(rproc, size, va, dma);
		return ret;
	}

	dev_dbg(dev, "vring%d: va %p dma %x size %x idr %d\n", i, va,
					dma, size, notifyid);

	vring->va = va;
	vring->dma = dma;
	vring->notifyid = notifyid;

	return 0;
}

static int
rproc_parse_vring(struct rproc_vdev *rvdev, struct fw_rsc_vdev *rsc, int i)
{
	struct rproc *rproc = rvdev->rproc;
	struct device *dev = &rproc->dev;
	struct fw_rsc_vdev_vring *vring = &rsc->vring[i];

	dev_dbg(dev, "vdev rsc: vring%d: da %x, qsz %d, align %d\n",
				i, vring->da, vring->num, vring->align);

	/* make sure reserved bytes are zeroes */
	if (vring->reserved) {
		dev_err(dev, "vring rsc has non zero reserved bytes\n");
		return -EINVAL;
	}

	/* verify queue size and vring alignment are sane */
	if (!vring->num || !vring->align) {
		dev_err(dev, "invalid qsz (%d) or alignment (%d)\n",
						vring->num, vring->align);
		return -EINVAL;
	}

	rvdev->vring[i].len = vring->num;
	rvdev->vring[i].align = vring->align;
	rvdev->vring[i].rvdev = rvdev;

	return 0;
}

void rproc_free_vring(struct rproc_vring *rvring)
{
	int size = PAGE_ALIGN(vring_size(rvring->len, rvring->align));
	struct rproc *rproc = rvring->rvdev->rproc;

	rproc_free_memory(rproc, size, rvring->va, rvring->dma);
	idr_remove(&rproc->notifyids, rvring->notifyid);
}

/**
 * rproc_handle_vdev() - handle a vdev fw resource
 * @rproc: the remote processor
 * @rsc: the vring resource descriptor
 * @avail: size of available data (for sanity checking the image)
 *
 * This resource entry requests the host to statically register a virtio
 * device (vdev), and setup everything needed to support it. It contains
 * everything needed to make it possible: the virtio device id, virtio
 * device features, vrings information, virtio config space, etc...
 *
 * Before registering the vdev, the vrings are allocated from non-cacheable
 * physically contiguous memory. Currently we only support two vrings per
 * remote processor (temporary limitation). We might also want to consider
 * doing the vring allocation only later when ->find_vqs() is invoked, and
 * then release them upon ->del_vqs().
 *
 * Note: @da is currently not really handled correctly: we dynamically
 * allocate it using the DMA API, ignoring requested hard coded addresses,
 * and we don't take care of any required IOMMU programming. This is all
 * going to be taken care of when the generic iommu-based DMA API will be
 * merged. Meanwhile, statically-addressed iommu-based firmware images should
 * use RSC_DEVMEM resource entries to map their required @da to the physical
 * address of their base CMA region (ouch, hacky!).
 *
 * Returns 0 on success, or an appropriate error code otherwise
 */
static int rproc_handle_vdev(struct rproc *rproc, struct fw_rsc_vdev *rsc,
								int avail)
{
	struct device *dev = &rproc->dev;
	struct rproc_vdev *rvdev;
	int i, ret;

	/* make sure resource isn't truncated */
	if (sizeof(*rsc) + rsc->num_of_vrings * sizeof(struct fw_rsc_vdev_vring)
			+ rsc->config_len > avail) {
		dev_err(dev, "vdev rsc is truncated\n");
		return -EINVAL;
	}

	/* make sure reserved bytes are zeroes */
	if (rsc->reserved[0] || rsc->reserved[1]) {
		dev_err(dev, "vdev rsc has non zero reserved bytes\n");
		return -EINVAL;
	}

	dev_dbg(dev, "vdev rsc: id %d, dfeatures %x, cfg len %d, %d vrings\n",
		rsc->id, rsc->dfeatures, rsc->config_len, rsc->num_of_vrings);

	/* we currently support only two vrings per rvdev */
	if (rsc->num_of_vrings > ARRAY_SIZE(rvdev->vring)) {
		dev_err(dev, "too many vrings: %d\n", rsc->num_of_vrings);
		return -EINVAL;
	}

	rvdev = kzalloc(sizeof(struct rproc_vdev), GFP_KERNEL);
	if (!rvdev)
		return -ENOMEM;

	rvdev->rproc = rproc;

	/* parse the vrings */
	for (i = 0; i < rsc->num_of_vrings; i++) {
		ret = rproc_parse_vring(rvdev, rsc, i);
		if (ret)
			goto free_rvdev;
	}

	/* remember the device features */
	rvdev->dfeatures = rsc->dfeatures;

	list_add_tail(&rvdev->node, &rproc->rvdevs);
	rproc->num_rvdevs++;

	return 0;

free_rvdev:
	kfree(rvdev);
	return ret;
}

/**
 * rproc_handle_last_trace() - setup a buffer to capture the trace snapshot
 *				before recovery
 * @rproc: the remote processor
 * @trace: the trace resource descriptor
 * @count: the index of the trace under process
 *
 * The last trace is allocated and the contents of the trace buffer are
 * copied during a recovery cleanup. Once, the contents get copied, the
 * trace buffers are cleaned up for re-use.
 *
 * It might also happen that the remoteproc binary changes between the
 * time that it was loaded and the time that it crashed. In this case,
 * the trace descriptors might have changed too. The last traces are
 * re-built as required in this case.
 *
 * Returns 0 on success, or an appropriate error code otherwise
 */
static int rproc_handle_last_trace(struct rproc *rproc,
				struct rproc_mem_entry *trace, int count)
{
	struct rproc_mem_entry *trace_last, *tmp_trace;
	struct device *dev = &rproc->dev;
	char name[15];
	int i = 0;
	bool new_trace = false;

	if (!rproc || !trace)
		return -EINVAL;

	/* we need a new trace in this case */
	if (count > rproc->num_last_traces) {
		new_trace = true;
		/*
		 * make sure snprintf always null terminates, even if truncating
		 */
		snprintf(name, sizeof(name), "trace%d_last", (count - 1));
		trace_last = kzalloc(sizeof *trace_last, GFP_KERNEL);
		if (!trace_last) {
			dev_err(dev, "kzalloc failed for trace%d_last\n",
									count);
			return -ENOMEM;
		}
	} else {
		/* try to reuse buffers here */
		list_for_each_entry_safe(trace_last, tmp_trace,
				&rproc->last_traces, node) {
			if (++i == count)
				break;
		}

		/* if we can reuse the trace, copy buffer and exit */
		if (trace_last->len == trace->len)
			goto copy_and_exit;

		/* can reuse the trace struct but not the buffer */
		vfree(trace_last->va);
		trace_last->va = NULL;
		trace_last->len = 0;
	}

	trace_last->len = trace->len;
	trace_last->va = vmalloc(sizeof(u32) * trace_last->len);
	if (!trace_last->va) {
		dev_err(dev, "vmalloc failed for trace%d_last\n", count);
		if (!new_trace) {
			list_del(&trace_last->node);
			rproc->num_last_traces--;
		}
		kfree(trace_last);
		return -ENOMEM;
	}

	/* create the debugfs entry */
	if (new_trace) {
		trace_last->priv = rproc_create_trace_file(name, rproc,
				trace_last);
		if (!trace_last->priv) {
			dev_err(dev, "trace%d_last create debugfs failed\n",
							count);
			vfree(trace_last->va);
			kfree(trace_last);
			return -EINVAL;
		}

		/* add it to the trace list */
		list_add_tail(&trace_last->node, &rproc->last_traces);
		rproc->num_last_traces++;
	}

copy_and_exit:
	/* copy the trace to last trace */
	memcpy(trace_last->va, trace->va, trace->len);

	return 0;
}

/**
 * rproc_handle_trace() - handle a shared trace buffer resource
 * @rproc: the remote processor
 * @rsc: the trace resource descriptor
 * @avail: size of available data (for sanity checking the image)
 *
 * In case the remote processor dumps trace logs into memory,
 * export it via debugfs.
 *
 * Currently, the 'da' member of @rsc should contain the device address
 * where the remote processor is dumping the traces. Later we could also
 * support dynamically allocating this address using the generic
 * DMA API (but currently there isn't a use case for that).
 *
 * Returns 0 on success, or an appropriate error code otherwise
 */
static int rproc_handle_trace(struct rproc *rproc, struct fw_rsc_trace *rsc,
								int avail)
{
	struct rproc_mem_entry *trace;
	struct device *dev = &rproc->dev;
	void *ptr;
	char name[15];

	if (sizeof(*rsc) > avail) {
		dev_err(dev, "trace rsc is truncated\n");
		return -EINVAL;
	}

	/* make sure reserved bytes are zeroes */
	if (rsc->reserved) {
		dev_err(dev, "trace rsc has non zero reserved bytes\n");
		return -EINVAL;
	}

	/* what's the kernel address of this resource ? */
	ptr = rproc_da_to_va(rproc, rsc->da, rsc->len);
	if (!ptr) {
		dev_err(dev, "erroneous trace resource entry\n");
		return -EINVAL;
	}

	trace = kzalloc(sizeof(*trace), GFP_KERNEL);
	if (!trace) {
		dev_err(dev, "kzalloc trace failed\n");
		return -ENOMEM;
	}

	/* set the trace buffer dma properties */
	trace->len = rsc->len;
	trace->va = ptr;

	/* make sure snprintf always null terminates, even if truncating */
	snprintf(name, sizeof(name), "trace%d", rproc->num_traces);

	/* create the debugfs entry */
	trace->priv = rproc_create_trace_file(name, rproc, trace);
	if (!trace->priv) {
		trace->va = NULL;
		kfree(trace);
		return -EINVAL;
	}

	list_add_tail(&trace->node, &rproc->traces);

	rproc->num_traces++;

	dev_dbg(dev, "%s added: va %p, da 0x%x, len 0x%x\n", name, ptr,
						rsc->da, rsc->len);

	return 0;
}

/**
 * rproc_handle_devmem() - handle devmem resource entry
 * @rproc: remote processor handle
 * @rsc: the devmem resource entry
 * @avail: size of available data (for sanity checking the image)
 *
 * Remote processors commonly need to access certain on-chip peripherals.
 *
 * Some of these remote processors access memory via an iommu device,
 * and might require us to configure their iommu before they can access
 * the on-chip peripherals they need.
 *
 * This resource entry is a request to map such a peripheral device. The
 * entry information is stored in a mapping entry, and will be mapped
 * into the IOMMU all at once along with all the mapping entries. This is
 * being done to support a secure playback usecase, wherein the iommu
 * will be configured differently.
 *
 * These devmem entries will contain the physical address of the device in
 * the 'pa' member. If a specific device address is expected, then 'da' will
 * contain it (currently this is the only use case supported). 'len' will
 * contain the size of the physical region we need to map.
 *
 * Currently we just "trust" those devmem entries to contain valid physical
 * addresses, but this is going to change: we want the implementations to
 * tell us ranges of physical addresses the firmware is allowed to request,
 * and not allow firmwares to request access to physical addresses that
 * are outside those ranges.
 */
static int rproc_handle_devmem(struct rproc *rproc, struct fw_rsc_devmem *rsc,
								int avail)
{
	struct rproc_mem_entry *mapping;
	struct device *dev = &rproc->dev;

	if (sizeof(*rsc) > avail) {
		dev_err(dev, "devmem rsc is truncated\n");
		return -EINVAL;
	}

	mapping = kzalloc(sizeof(*mapping), GFP_KERNEL);
	if (!mapping) {
		dev_err(dev, "kzalloc mapping failed\n");
		return -ENOMEM;
	}

	/*
	 * We'll need this info later when we'll want to map at a later time
	 * or unmap everything (e.g. on shutdown)
	 *
	 * We can't trust the remote processor not to change the resource
	 * table, so we must maintain this info independently.
	 */
	mapping->dma = rsc->pa;
	mapping->da = rsc->da;
	mapping->len = rsc->len;
	mapping->memregion = rsc->memregion;
	mapping->type = RSC_DEVMEM;
	mapping->flags = rsc->flags;
	mapping->mapped = false;
	list_add_tail(&mapping->node, &rproc->mappings);

	dev_dbg(dev, "processed devmem pa 0x%x, da 0x%x, len 0x%x\n",
					rsc->pa, rsc->da, rsc->len);

	return 0;
}

/**
 * rproc_handle_carveout() - handle phys contig memory allocation requests
 * @rproc: rproc handle
 * @rsc: the resource entry
 * @avail: size of available data (for image validation)
 *
 * This function will handle firmware requests for allocation of physically
 * contiguous memory regions.
 *
 * These request entries should come first in the firmware's resource table,
 * as other firmware entries might request placing other data objects inside
 * these memory regions (e.g. data/code segments, trace resource entries, ...).
 *
 * Allocating memory this way helps utilizing the reserved physical memory
 * (e.g. CMA) more efficiently, and also minimizes the number of TLB entries
 * needed to map it (in case @rproc is using an IOMMU). Reducing the TLB
 * pressure is important; it may have a substantial impact on performance.
 */
static int rproc_handle_carveout(struct rproc *rproc,
				struct fw_rsc_carveout *rsc, int avail)
{
	struct rproc_mem_entry *carveout, *mapping;
	struct device *dev = &rproc->dev;
	dma_addr_t dma;
	void *va;
	int ret;

	if (sizeof(*rsc) > avail) {
		dev_err(dev, "carveout rsc is truncated\n");
		return -EINVAL;
	}

	dev_dbg(dev, "carveout rsc: da %x, pa %x, len %x, flags %x\n",
			rsc->da, rsc->pa, rsc->len, rsc->flags);

	carveout = kzalloc(sizeof(*carveout), GFP_KERNEL);
	if (!carveout) {
		dev_err(dev, "kzalloc carveout failed\n");
		return -ENOMEM;
	}

	va = rproc_alloc_memory(rproc, rsc->len, &dma, RPROC_MEM_FW);
	if (!va) {
		dev_err(dev, "rproc_alloc_memory failed: %d\n", rsc->len);
		ret = -ENOMEM;
		goto free_carv;
	}

	dev_dbg(dev, "carveout va %p, dma %x, len 0x%x\n", va, dma, rsc->len);

	/*
	 * Ok, this is non-standard.
	 *
	 * Sometimes we can't rely on the generic iommu-based DMA API
	 * to dynamically allocate the device address and then set the IOMMU
	 * tables accordingly, because some remote processors might
	 * _require_ us to use hard coded device addresses that their
	 * firmware was compiled with.
	 *
	 * In this case, we must use the IOMMU API directly and map
	 * the memory to the device address as expected by the remote
	 * processor. The required information is stored in a mapping
	 * entry, and will be mapped into the IOMMU all at once along
	 * with the rest of the mapping entries. This is being done to
	 * support a secure playback usecase, wherein the iommu will be
	 * configured differently.
	 *
	 * Obviously such remote processor devices should not be configured
	 * to use the iommu-based DMA API: we expect 'dma' to contain the
	 * physical address in this case.
	 */
	mapping = kzalloc(sizeof(*mapping), GFP_KERNEL);
	if (!mapping) {
		dev_err(dev, "kzalloc mapping failed\n");
		ret = -ENOMEM;
		goto dma_free;
	}

	/*
	 * We'll need this info later when we'll want to map at a later time
	 * or unmap everything (e.g. on shutdown).
	 *
	 * We can't trust the remote processor not to change the
	 * resource table, so we must maintain this info independently.
	 */
	mapping->dma = dma;
	mapping->da = rsc->da;
	mapping->len = rsc->len;
	mapping->type = RSC_CARVEOUT;
	mapping->flags = rsc->flags;
	mapping->mapped = false;
	list_add_tail(&mapping->node, &rproc->mappings);

	dev_dbg(dev, "carveout processed 0x%x to 0x%x\n", rsc->da, dma);

	/*
	 * Some remote processors might need to know the pa
	 * even though they are behind an IOMMU. E.g., OMAP4's
	 * remote M3 processor needs this so it can control
	 * on-chip hardware accelerators that are not behind
	 * the IOMMU, and therefor must know the pa.
	 *
	 * Generally we don't want to expose physical addresses
	 * if we don't have to (remote processors are generally
	 * _not_ trusted), so we might want to do this only for
	 * remote processor that _must_ have this (e.g. OMAP4's
	 * dual M3 subsystem).
	 */
	rsc->pa = dma;

	carveout->va = va;
	carveout->len = rsc->len;
	carveout->dma = dma;
	carveout->da = rsc->da;
	carveout->memregion = rsc->memregion;

	list_add_tail(&carveout->node, &rproc->carveouts);

	return 0;

dma_free:
	rproc_free_memory(rproc, rsc->len, va, dma);
free_carv:
	kfree(carveout);
	return ret;
}

/**
 * rproc_handle_suspd_time() - handle auto suspend timeout resource
 * @rproc: the remote processor
 * @rsc: auto suspend tiemout resource descriptor
 * @avail: size of available data (for sanity checking the image)
 *
 * Set the autosuspend timeout the rproc will use
 *
 * Returns 0 on success, or an appropriate error code otherwise
 */
static int rproc_handle_suspd_time(struct rproc *rproc,
		 struct fw_rsc_suspd_time *rsc, int avail)
{
	struct device *dev = &rproc->dev;

	if (sizeof(*rsc) > avail) {
		dev_err(dev, "suspend timeout rsc is truncated\n");
		return -EINVAL;
	}

	rproc->auto_suspend_timeout = rsc->suspd_time;
	return 0;
}

/**
 * rproc_handle_custom_rsc() - provide implementation specific hook
 *			       to handle custom resources
 * @rproc: the remote processor
 * @rsc: custom resource to be handled by drivers
 * @avail: size of available data
 *
 * Remoteproc implementations might want to add resource table
 * entries that are not generic enough to be handled by the framework.
 * This provides a hook to handle such custom resources.
 *
 * Returns 0 on success, or an appropriate error code otherwise
 */
static int rproc_handle_custom_rsc(struct rproc *rproc,
		 struct fw_rsc_custom *rsc, int avail)
{
	struct device *dev = &rproc->dev;

	if (!rproc->ops->handle_custom_rsc) {
		dev_err(dev, "custom resource handler unavailable\n");
		return -EINVAL;
	}

	if (sizeof(*rsc) > avail) {
		dev_err(dev, "custom resource is truncated\n");
		return -EINVAL;
	}

	return rproc->ops->handle_custom_rsc(rproc, (void *)rsc);
}

/*
 * A lookup table for resource handlers. The indices are defined in
 * enum fw_resource_type.
 */
static rproc_handle_resource_t rproc_handle_rsc[] = {
	[RSC_CARVEOUT] = (rproc_handle_resource_t)rproc_handle_carveout,
	[RSC_DEVMEM] = (rproc_handle_resource_t)rproc_handle_devmem,
	[RSC_TRACE] = (rproc_handle_resource_t)rproc_handle_trace,
	[RSC_SUSPD_TIME] = (rproc_handle_resource_t)rproc_handle_suspd_time,
	[RSC_CUSTOM] = (rproc_handle_resource_t)rproc_handle_custom_rsc,
	[RSC_VDEV] = NULL, /* VDEVs were handled upon registrarion */
};

/* handle firmware resource entries before booting the remote processor */
static int
rproc_handle_boot_rsc(struct rproc *rproc, struct resource_table *table, int len)
{
	struct device *dev = &rproc->dev;
	rproc_handle_resource_t handler;
	int ret = 0, i;

	for (i = 0; i < table->num; i++) {
		int offset = table->offset[i];
		struct fw_rsc_hdr *hdr = (void *)table + offset;
		int avail = len - offset - sizeof(*hdr);
		void *rsc = (void *)hdr + sizeof(*hdr);

		/* make sure table isn't truncated */
		if (avail < 0) {
			dev_err(dev, "rsc table is truncated\n");
			return -EINVAL;
		}

		dev_dbg(dev, "rsc: type %d\n", hdr->type);

		if (hdr->type >= RSC_LAST) {
			dev_warn(dev, "unsupported resource %d\n", hdr->type);
			continue;
		}

		handler = rproc_handle_rsc[hdr->type];
		if (!handler)
			continue;

		ret = handler(rproc, rsc, avail);
		if (ret)
			break;
	}

	return ret;
}

/* handle firmware resource entries while registering the remote processor */
static int
rproc_handle_virtio_rsc(struct rproc *rproc, struct resource_table *table, int len)
{
	struct device *dev = &rproc->dev;
	int ret = 0, i;
	struct rproc_vdev *rvdev, *rvtmp;

	rproc->num_rvdevs = 0;
	for (i = 0; i < table->num; i++) {
		int offset = table->offset[i];
		struct fw_rsc_hdr *hdr = (void *)table + offset;
		int avail = len - offset - sizeof(*hdr);
		struct fw_rsc_vdev *vrsc;

		/* make sure table isn't truncated */
		if (avail < 0) {
			dev_err(dev, "rsc table is truncated\n");
			return -EINVAL;
		}

		dev_dbg(dev, "%s: rsc type %d\n", __func__, hdr->type);

		if (hdr->type != RSC_VDEV)
			continue;

		vrsc = (struct fw_rsc_vdev *)hdr->data;

		ret = rproc_handle_vdev(rproc, vrsc, avail);
		if (ret) {
			list_for_each_entry_safe(rvdev, rvtmp, &rproc->rvdevs,
							node) {
				rproc->num_rvdevs--;
				list_del(&rvdev->node);
				kfree(rvdev);
			}
			return ret;
		}
	}

	/* register all remote vdevs */
	list_for_each_entry_safe(rvdev, rvtmp, &rproc->rvdevs, node) {
		ret = rproc_add_virtio_dev(rvdev, VIRTIO_ID_RPMSG);
		if (ret)
			break;
	}

	return ret;
}

/* handle firmware version entry before loading the firmware sections */
static int
rproc_handle_fw_version(struct rproc *rproc, const char *version, int versz)
{
	struct device *dev = &rproc->dev;

	rproc->fw_version = kmemdup(version, versz, GFP_KERNEL);
	if (!rproc->fw_version) {
		dev_err(dev, "%s: version kzalloc failed\n", __func__);
		return -ENOMEM;
	}
	return 0;
}

/**
 * rproc_find_fw_section() - find the section named in fw
 * @rproc: the rproc handle
 * @elf_data: the content of the ELF firmware image
 * @len: firmware size (in bytes)
 * @sect: name of fw section
 * @sectsz: size of the fw section
 *
 * This function finds the given section inside the remote processor's
 * firmware.
 *
 * Returns the pointer to the section if it is found, and write its
 * size into @sectsz. If a valid section isn't found, NULL is returned
 * (and @sectsz isn't set).
 */
static const u8 *
rproc_find_fw_section(struct rproc *rproc, const u8 *elf_data, size_t len,
						const char *sect, int *sectsz)
{
	struct elf32_hdr *ehdr;
	struct elf32_shdr *shdr;
	const char *name_sect;
	struct device *dev = &rproc->dev;
	const u8 *sectaddr = NULL;
	int i;

	ehdr = (struct elf32_hdr *)elf_data;
	shdr = (struct elf32_shdr *)(elf_data + ehdr->e_shoff);
	name_sect = elf_data + shdr[ehdr->e_shstrndx].sh_offset;

	/* look for the section */
	for (i = 0; i < ehdr->e_shnum; i++, shdr++) {
		int size = shdr->sh_size;
		int offset = shdr->sh_offset;

		if (strcmp(name_sect + shdr->sh_name, sect))
			continue;

		/* make sure we have the entire table */
		if (offset + size > len) {
			dev_err(dev, "%s truncated\n", sect);
			return NULL;
		}
		sectaddr = elf_data + offset;
		*sectsz = shdr->sh_size;

		break;
	}

	return sectaddr;
}

/**
 * rproc_find_rsc_table() - find the resource table
 * @rproc: the rproc handle
 * @elf_data: the content of the ELF firmware image
 * @len: firmware size (in bytes)
 * @tablesz: place holder for providing back the table size
 *
 * This function finds the resource table inside the remote processor's
 * firmware. It is used both upon the registration of @rproc (in order
 * to look for and register the supported virito devices), and when the
 * @rproc is booted.
 *
 * Returns the pointer to the resource table if it is found, and write its
 * size into @tablesz. If a valid table isn't found, NULL is returned
 * (and @tablesz isn't set).
 */
static struct resource_table *
rproc_find_rsc_table(struct rproc *rproc, const u8 *elf_data, size_t len,
							int *tablesz)
{
	struct device *dev = &rproc->dev;
	struct resource_table *table = NULL;
	int size;

	table = (struct resource_table *) rproc_find_fw_section(rproc, elf_data,
		len, ".resource_table", &size);

	if (!table)
		return NULL;

	/* make sure table has at least the header */
	if (sizeof(struct resource_table) > size) {
		dev_err(dev, "header-less resource table\n");
		return NULL;
	}

	/* we don't support any version beyond the first */
	if (table->ver != 1) {
		dev_err(dev, "unsupported fw ver: %d\n", table->ver);
		return NULL;
	}

	/* make sure reserved bytes are zeroes */
	if (table->reserved[0] || table->reserved[1]) {
		dev_err(dev, "non zero reserved bytes\n");
		return NULL;
	}

	/* make sure the offsets array isn't truncated */
	if (table->num * sizeof(table->offset[0]) +
			sizeof(struct resource_table) > size) {
		dev_err(dev, "resource table incomplete\n");
		return NULL;
	}

	*tablesz = size;
	return table;
}

/**
 * rproc_free_last_trace() - helper function to cleanup a last trace entry
 * @trace: the last trace element to be cleaned up
 */
static void rproc_free_last_trace(struct rproc_mem_entry *trace)
{
	rproc_remove_trace_file(trace->priv);
	list_del(&trace->node);
	vfree(trace->va);
	kfree(trace);
}

/**
 * rproc_resource_cleanup() - clean up and free all acquired resources
 * @rproc: rproc handle
 *
 * This function will free all resources acquired for @rproc, and it
 * is called whenever @rproc either shuts down or fails to boot.
 */
static void rproc_resource_cleanup(struct rproc *rproc)
{
	struct rproc_mem_entry *entry, *tmp;
	struct device *dev = &rproc->dev;
	int count = 0, i = rproc->num_traces;

	/* clean up debugfs trace entries */
	list_for_each_entry_safe(entry, tmp, &rproc->traces, node) {
		/* handle last trace here */
		if (rproc->state == RPROC_CRASHED)
			rproc_handle_last_trace(rproc, entry, ++count);

		rproc_remove_trace_file(entry->priv);
		list_del(&entry->node);
		kfree(entry);
	}
	rproc->num_traces = 0;

	/*
	 * clean up debugfs last trace entries. This either deletes all last
	 * trace entries during cleanup or just the remaining entries, if any,
	 * in case of a crash.
	 */
	list_for_each_entry_safe(entry, tmp, &rproc->last_traces, node) {
		/* skip the valid traces */
		if ((i--) && (rproc->state == RPROC_CRASHED))
			continue;
		rproc_free_last_trace(entry);
		rproc->num_last_traces--;
	}

	/* clean up carveout allocations */
	list_for_each_entry_safe(entry, tmp, &rproc->carveouts, node) {
		rproc_free_memory(rproc, entry->len, entry->va, entry->dma);
		list_del(&entry->node);
		kfree(entry);
	}

	/* reconfigure the carveout pool, if it exists */
	if (rproc->memory_pool)
		rproc_reset_poolmem(rproc);

	/* clean up iommu mapping entries */
	list_for_each_entry_safe(entry, tmp, &rproc->mappings, node) {
		size_t unmapped;

		if (entry->mapped) {
			unmapped = iommu_unmap(rproc->domain, entry->da,
								entry->len);
			if (unmapped != entry->len) {
				/* nothing much to do besides complaining */
				dev_err(dev, "failed to unmap %u/%zu\n",
							entry->len, unmapped);
			}
		}

		list_del(&entry->node);
		kfree(entry);
	}
}

/* make sure this fw image is sane */
static int rproc_fw_sanity_check(struct rproc *rproc, const struct firmware *fw)
{
	const char *name = rproc->firmware;
	struct device *dev = &rproc->dev;
	struct elf32_hdr *ehdr;
	char class;

	if (!fw) {
		dev_err(dev, "failed to load %s\n", name);
		return -EINVAL;
	}

	if (fw->size < sizeof(struct elf32_hdr)) {
		dev_err(dev, "Image is too small\n");
		return -EINVAL;
	}

	ehdr = (struct elf32_hdr *)fw->data;

	/* We only support ELF32 at this point */
	class = ehdr->e_ident[EI_CLASS];
	if (class != ELFCLASS32) {
		dev_err(dev, "Unsupported class: %d\n", class);
		return -EINVAL;
	}

	/* We assume the firmware has the same endianess as the host */
# ifdef __LITTLE_ENDIAN
	if (ehdr->e_ident[EI_DATA] != ELFDATA2LSB) {
# else /* BIG ENDIAN */
	if (ehdr->e_ident[EI_DATA] != ELFDATA2MSB) {
# endif
		dev_err(dev, "Unsupported firmware endianess\n");
		return -EINVAL;
	}

	if (fw->size < ehdr->e_shoff + sizeof(struct elf32_shdr)) {
		dev_err(dev, "Image is too small\n");
		return -EINVAL;
	}

	if (memcmp(ehdr->e_ident, ELFMAG, SELFMAG)) {
		dev_err(dev, "Image is corrupted (bad magic)\n");
		return -EINVAL;
	}

	if (ehdr->e_phnum == 0) {
		dev_err(dev, "No loadable segments\n");
		return -EINVAL;
	}

	if (ehdr->e_phoff > fw->size) {
		dev_err(dev, "Firmware size is too small\n");
		return -EINVAL;
	}

	return 0;
}

/*
 * take a firmware and boot a remote processor with it.
 */
static int rproc_fw_boot(struct rproc *rproc, const struct firmware *fw)
{
	struct device *dev = &rproc->dev;
	const char *name = rproc->firmware;
	struct elf32_hdr *ehdr;
	struct resource_table *table;
	int ret, tablesz, versz;
	const u8 *version;
	int smode = rproc_secure_get_mode(rproc);

	ret = rproc_fw_sanity_check(rproc, fw);
	if (ret)
		return ret;

	ehdr = (struct elf32_hdr *)fw->data;

	dev_info(dev, "Booting fw image %s, size %zd\n", name, fw->size);

	/*
	 * The ELF entry point is the rproc's boot addr (though this is not
	 * a configurable property of all remote processors: some will always
	 * boot at a specific hardcoded address).
	 */
	rproc->bootaddr = ehdr->e_entry;

	/* look for the resource table */
	table = rproc_find_rsc_table(rproc, fw->data, fw->size, &tablesz);
	if (!table) {
		ret = -EINVAL;
		goto clean_up;
	}

	/* handle fw resources which are required to boot rproc */
	ret = rproc_handle_boot_rsc(rproc, table, tablesz);
	if (ret) {
		dev_err(dev, "Failed to process resources: %d\n", ret);
		goto clean_up;
	}

	/* look for the firmware version, and store if present */
	version = rproc_find_fw_section(rproc, fw->data, fw->size,
				".version", &versz);
	if (version) {
		ret = rproc_handle_fw_version(rproc, version, versz);
		if (ret) {
			dev_err(dev, "Failed to process version info: %d\n",
					ret);
			goto clean_up;
		}
	}

	/* load the ELF segments to memory */
	ret = rproc_load_segments(rproc, fw->data, fw->size);
	if (ret) {
		dev_err(dev, "Failed to load program segments: %d\n", ret);
		goto free_version;
	}

	/* parse the secure sections */
	ret = rproc_secure_parse_fw(rproc, fw->data);
	if (ret) {
		dev_err(dev, "Failed to parse secure sections: %d\n", ret);
		goto free_version;
	}

	/*
	 * if enabling an IOMMU isn't relevant for this rproc, this is
	 * just a nop. the cleanup path is a bit out of order, but that
	 * is fine since the domain will be free and the actual disable
	 * call will be a nop.
	 */
	ret = rproc_enable_iommu(rproc);
	if (ret) {
		dev_err(dev, "can't enable iommu: %d\n", ret);
		goto free_version;
	}

	/* map the different remoteproc memory regions into the iommu */
	ret = rproc_program_iommu(rproc);
	if (ret) {
		dev_err(dev, "can't program iommu: %d\n", ret);
		goto free_version;
	}

	/* check and validate secure certificate */
	rproc_secure_boot(rproc);

	/* power up the remote processor */
	ret = rproc->ops->start(rproc);
	if (ret) {
		dev_err(dev, "can't start rproc %s: %d\n", rproc->name, ret);
		goto free_version;
	}

	rproc->state = RPROC_RUNNING;
	pm_runtime_set_active(dev);
	if (!smode)
		pm_runtime_enable(dev);
	pm_runtime_get_noresume(dev);
	rproc->auto_suspend_timeout = rproc->auto_suspend_timeout ? :
					DEFAULT_AUTOSUSPEND_TIMEOUT;
	if (rproc->auto_suspend_timeout >= 0) {
		pm_runtime_set_autosuspend_delay(dev,
						 rproc->auto_suspend_timeout);
		pm_runtime_use_autosuspend(dev);
		pm_runtime_mark_last_busy(dev);
		pm_runtime_put_autosuspend(dev);
	}

	dev_info(dev, "remote processor %s is now up\n", rproc->name);

	return 0;

free_version:
	kfree(rproc->fw_version);
clean_up:
	rproc_resource_cleanup(rproc);
	rproc_disable_iommu(rproc);
	return ret;
}

/*
 * take a firmware and look for virtio devices to register.
 *
 * Note: this function is called asynchronously upon registration of the
 * remote processor (so we must wait until it completes before we try
 * to unregister the device. one other option is just to use kref here,
 * that might be cleaner).
 */
static void rproc_fw_config_virtio(const struct firmware *fw, void *context)
{
	struct rproc *rproc = context;
	struct resource_table *table;
	int ret, tablesz;

	if (rproc_fw_sanity_check(rproc, fw) < 0)
		goto out;

	/* look for the resource table */
	table = rproc_find_rsc_table(rproc, fw->data, fw->size, &tablesz);
	if (!table)
		goto out;

	/* look for virtio devices and register them */
	ret = rproc_handle_virtio_rsc(rproc, table, tablesz);
	if (ret)
		goto out;

out:
	if (fw)
		release_firmware(fw);
	/* allow rproc_unregister() contexts, if any, to proceed */
	complete_all(&rproc->firmware_loading_complete);
}

/**
 * rproc_boot() - boot a remote processor
 * @rproc: handle of a remote processor
 *
 * Boot a remote processor (i.e. load its firmware, power it on, ...).
 *
 * If the remote processor is already powered on, this function immediately
 * returns (successfully).
 *
 * Returns 0 on success, and an appropriate error value otherwise.
 */
int rproc_boot(struct rproc *rproc)
{
	const struct firmware *firmware_p;
	struct device *dev;
	int ret;

	if (!rproc) {
		pr_err("invalid rproc handle\n");
		return -EINVAL;
	}

	dev = &rproc->dev;

	ret = mutex_lock_interruptible(&rproc->lock);
	if (ret) {
		dev_err(dev, "can't lock rproc %s: %d\n", rproc->name, ret);
		return ret;
	}

	/* loading a firmware is required */
	if (!rproc->firmware) {
		dev_err(dev, "%s: no firmware to load\n", __func__);
		ret = -EINVAL;
		goto unlock_mutex;
	}

	/* prevent underlying implementation from being removed */
	if (!try_module_get(dev->parent->driver->owner)) {
		dev_err(dev, "%s: can't get owner\n", __func__);
		ret = -EINVAL;
		goto unlock_mutex;
	}

	/* skip the boot process if rproc is already powered up */
	if (atomic_inc_return(&rproc->power) > rproc->num_rvdevs) {
		ret = 0;
		goto unlock_mutex;
	}

	/* skip booting until the last virtio device is probed */
	if (atomic_read(&rproc->power) != rproc->num_rvdevs) {
		dev_info(dev, "skipping power up until last virtio device %s\n",
								rproc->name);
		ret = 0;
		goto unlock_mutex;
	}

	dev_info(dev, "powering up %s\n", rproc->name);

	/* load firmware */
	ret = request_firmware(&firmware_p, rproc->firmware, dev);
	if (ret < 0) {
		dev_err(dev, "request_firmware failed: %d\n", ret);
		goto downref_rproc;
	}

	ret = rproc_fw_boot(rproc, firmware_p);

	release_firmware(firmware_p);

downref_rproc:
	if (ret) {
		module_put(dev->parent->driver->owner);
		atomic_dec(&rproc->power);
	}
unlock_mutex:
	mutex_unlock(&rproc->lock);
	return ret;
}
EXPORT_SYMBOL(rproc_boot);

/**
 * rproc_shutdown() - power off the remote processor
 * @rproc: the remote processor
 *
 * Power off a remote processor (previously booted with rproc_boot()).
 *
 * In case @rproc is still being used by an additional user(s), then
 * this function will just decrement the power refcount and exit,
 * without really powering off the device.
 *
 * Every call to rproc_boot() must (eventually) be accompanied by a call
 * to rproc_shutdown(). Calling rproc_shutdown() redundantly is a bug.
 *
 * Notes:
 * - we're not decrementing the rproc's refcount, only the power refcount.
 *   which means that the @rproc handle stays valid even after rproc_shutdown()
 *   returns, and users can still use it with a subsequent rproc_boot(), if
 *   needed.
 * - don't call rproc_shutdown() to unroll rproc_get_by_name(), exactly
 *   because rproc_shutdown() _does not_ decrement the refcount of @rproc.
 *   To decrement the refcount of @rproc, use rproc_put() (but _only_ if
 *   you acquired @rproc using rproc_get_by_name()).
 */
void rproc_shutdown(struct rproc *rproc)
{
	struct device *dev = &rproc->dev;
	int ret;
	int smode = rproc_secure_get_mode(rproc);

	ret = mutex_lock_interruptible(&rproc->lock);
	if (ret) {
		dev_err(dev, "can't lock rproc %s: %d\n", rproc->name, ret);
		return;
	}

	/* if the remote proc is still needed, bail out */
	if (!atomic_dec_and_test(&rproc->power))
		goto out;

	/*
	 * if autosuspend is enabled we need to cancel possible already
	 * scheduled runtime suspend. Also, if it is already autosuspended
	 * we need to wake it up to avoid stopping the processor with
	 * context saved which can cause a issue when it is started again
	 */
	if (rproc->auto_suspend_timeout >= 0)
		pm_runtime_get_sync(dev);

	pm_runtime_put_noidle(&rproc->dev);
	if (smode || !rproc_is_secure(rproc))
		pm_runtime_disable(&rproc->dev);
	pm_runtime_set_suspended(&rproc->dev);

	/* power off the remote processor */
	ret = rproc->ops->stop(rproc);
	if (ret) {
		atomic_inc(&rproc->power);
		dev_err(dev, "can't stop rproc: %d\n", ret);
		goto out;
	}

	/* free fw version */
	kfree(rproc->fw_version);

	/* clean up all acquired resources */
	rproc_resource_cleanup(rproc);

	rproc_disable_iommu(rproc);

	rproc->state = RPROC_OFFLINE;

	dev_info(dev, "stopped remote processor %s\n", rproc->name);

out:
	mutex_unlock(&rproc->lock);
	if (!ret)
		module_put(dev->parent->driver->owner);
}
EXPORT_SYMBOL(rproc_shutdown);

/**
 * rproc_release() - completely deletes the existence of a remote processor
 * @kref: the rproc's kref
 *
 * This function should _never_ be called directly.
 *
 * The only reasonable location to use it is as an argument when kref_put'ing
 * @rproc's refcount.
 *
 * This way it will be called when no one holds a valid pointer to this @rproc
 * anymore (and obviously after it is removed from the rprocs klist).
 *
 * Note: this function is not static because rproc_vdev_release() needs it when
 * it decrements @rproc's refcount.
 */
void rproc_release(struct kref *kref)
{
	struct rproc *rproc = container_of(kref, struct rproc, refcount);
	struct rproc_mem_entry *entry, *tmp;

	/* clean up debugfs last trace entries */
	list_for_each_entry_safe(entry, tmp, &rproc->last_traces, node) {
		rproc_free_last_trace(entry);
		rproc->num_last_traces--;
	}

	dev_info(&rproc->dev, "removing %s\n", rproc->name);

	rproc_delete_debug_dir(rproc);

	/*
	 * At this point no one holds a reference to rproc anymore,
	 * so we can directly unroll rproc_alloc()
	 */
	rproc_free(rproc);
}

/* will be called when an rproc is added to the rprocs klist */
static void klist_rproc_get(struct klist_node *n)
{
	struct rproc *rproc = container_of(n, struct rproc, node);

	kref_get(&rproc->refcount);
}

/* will be called when an rproc is removed from the rprocs klist */
static void klist_rproc_put(struct klist_node *n)
{
	struct rproc *rproc = container_of(n, struct rproc, node);

	kref_put(&rproc->refcount, rproc_release);
}

static struct rproc *next_rproc(struct klist_iter *i)
{
	struct klist_node *n;

	n = klist_next(i);
	if (!n)
		return NULL;

	return container_of(n, struct rproc, node);
}

/**
 * _reset_all_vdev() - reset all virtio devices
 * @rproc: the remote processor
 *
 * This function reset all the rpmsg driver and also the remoteproc. That must
 * not be called during normal use cases, it could be used as a last resource
 * in the error handler to make a recovery of the remoteproc. This function can
 * sleep, so that it cannot be called from atomic context.
 */
static int _reset_all_vdev(struct rproc *rproc)
{
	struct rproc_vdev *rvdev, *rvtmp;

	dev_dbg(&rproc->dev, "reseting virtio devices for %s\n", rproc->name);

	/* reset firewalls */
	rproc_secure_reset(rproc);

	/* clean up remote vdev entries */
	list_for_each_entry_safe(rvdev, rvtmp, &rproc->rvdevs, node)
		rproc_remove_virtio_dev(rvdev);

	/* run rproc_fw_config_virtio to create vdevs again */
	return request_firmware_nowait(THIS_MODULE, FW_ACTION_HOTPLUG,
			rproc->firmware, &rproc->dev, GFP_KERNEL,
			rproc, rproc_fw_config_virtio);
}

/**
 * rproc_error_handler_work() - handle a faltar error
 *
 * This function needs to handle everything related to a fatal error,
 * like cpu registers and stack dump, information to help to
 * debug the fatal error, etc.
 */
static void rproc_error_handler_work(struct work_struct *work)
{
	struct rproc *rproc = container_of(work, struct rproc, error_handler);
	struct device *dev = &rproc->dev;

	dev_dbg(dev, "enter %s\n", __func__);

	mutex_lock(&rproc->lock);
	if (rproc->state == RPROC_CRASHED || rproc->state == RPROC_OFFLINE) {
		mutex_unlock(&rproc->lock);
		return;
	}

	rproc->state = RPROC_CRASHED;
	mutex_unlock(&rproc->lock);
	/*
	 * if recovery enabled reset all virtio devices, so that all rpmsg
	 * drivers can be restarted in order to make them functional again
	 */
	if (!rproc->recovery_disabled) {
		dev_err(dev, "trying to recover %s\n", rproc->name);
		_reset_all_vdev(rproc);
	}
}

/**
 * rproc_recover() - trigger recovery for a rproc
 *
 * This function is used to trigger a rproc recovery explicitly.
 * Recovery could be disabled using debugfs while debugging, and
 * this function provides the mechanism to reload and restore the
 * rproc functionality after the debug is complete.
 */
void rproc_recover(struct rproc *rproc)
{
	if (rproc->recovery_disabled && rproc->state == RPROC_CRASHED) {
		dev_info(&rproc->dev, "rproc recovering....\n");
		_reset_all_vdev(rproc);
	}
}

/**
 * rproc_reload() - trigger reload for a rproc
 *
 * This function is used to trigger a rproc reload explicitly.
 */
int rproc_reload(const char *name)
{
	struct rproc *rproc;
	struct klist_iter i;
	int ret = 0;

	/* find the remote processor, and upref its refcount */
	klist_iter_init(&rprocs, &i);
	while ((rproc = next_rproc(&i)) != NULL)
		if (!strcmp(rproc->name, name)) {
			kref_get(&rproc->refcount);
			break;
		}
	klist_iter_exit(&i);

	/* can't find this rproc ? */
	if (!rproc) {
		pr_err("can't find remote processor %s\n", name);
		return -ENODEV;
	}

	dev_info(&rproc->dev, "rproc reloading....\n");
	_reset_all_vdev(rproc);
	return ret;
}

/**
 * rproc_get_by_name() - find a remote processor by name and boot it
 * @name: name of the remote processor
 *
 * Finds an rproc handle using the remote processor's name, and then
 * boot it. If it's already powered on, then just immediately return
 * (successfully).
 *
 * Returns the rproc handle on success, and NULL on failure.
 *
 * This function increments the remote processor's refcount, so always
 * use rproc_put() to decrement it back once rproc isn't needed anymore.
 *
 * Note: currently this function (and its counterpart rproc_put()) are not
 * being used. We need to scrutinize the use cases
 * that still need them, and see if we can migrate them to use the non
 * name-based boot/shutdown interface.
 */
struct rproc *rproc_get_by_name(const char *name)
{
	struct rproc *rproc;
	struct klist_iter i;
	int ret;

	/* find the remote processor, and upref its refcount */
	klist_iter_init(&rprocs, &i);
	while ((rproc = next_rproc(&i)) != NULL)
		if (!strcmp(rproc->name, name)) {
			kref_get(&rproc->refcount);
			break;
		}
	klist_iter_exit(&i);

	/* can't find this rproc ? */
	if (!rproc) {
		pr_err("can't find remote processor %s\n", name);
		return NULL;
	}

	ret = rproc_boot(rproc);
	if (ret < 0) {
		kref_put(&rproc->refcount, rproc_release);
		return NULL;
	}

	return rproc;
}
EXPORT_SYMBOL(rproc_get_by_name);

/**
 * rproc_put() - decrement the refcount of a remote processor, and shut it down
 * @rproc: the remote processor
 *
 * This function tries to shutdown @rproc, and it then decrements its
 * refcount.
 *
 * After this function returns, @rproc may _not_ be used anymore, and its
 * handle should be considered invalid.
 *
 * This function should be called _iff_ the @rproc handle was grabbed by
 * calling rproc_get_by_name().
 */
void rproc_put(struct rproc *rproc)
{
	/* try to power off the remote processor */
	rproc_shutdown(rproc);

	/* downref rproc's refcount */
	kref_put(&rproc->refcount, rproc_release);
}
EXPORT_SYMBOL(rproc_put);

int rproc_set_constraints(struct device *dev, struct rproc *rproc,
				enum rproc_constraint type, long v)
{
	int ret;
	char *cname[] = {"scale", "latency", "bandwidth"};
	int (*func)(struct device *, struct rproc *, long);

	switch (type) {
	case RPROC_CONSTRAINT_FREQUENCY:
		func = rproc->ops->set_frequency;
		break;
	case RPROC_CONSTRAINT_BANDWIDTH:
		func = rproc->ops->set_bandwidth;
		break;
	case RPROC_CONSTRAINT_LATENCY:
		func = rproc->ops->set_latency;
		break;
	default:
		dev_err(&rproc->dev, "invalid constraint\n");
		return -EINVAL;
	}

	if (!func) {
		dev_err(&rproc->dev, "%s: no %s constraint\n",
			__func__, cname[type]);
		return -EINVAL;
	}

	mutex_lock(&rproc->lock);
	if (rproc->state == RPROC_OFFLINE) {
		pr_err("%s: rproc inactive\n", __func__);
		mutex_unlock(&rproc->lock);
		return -EPERM;
	}

	dev_dbg(&rproc->dev, "set %s constraint %ld\n", cname[type], v);
	ret = func(dev, rproc, v);
	if (ret)
		dev_err(&rproc->dev, "error %s constraint\n", cname[type]);
	mutex_unlock(&rproc->lock);

	return ret;
}
EXPORT_SYMBOL(rproc_set_constraints);


static int rproc_loader_thread(struct rproc *rproc)
{
	const struct firmware *fw;
	struct device *dev = &rproc->dev;
	unsigned long to;
	int ret;

	/* wait until 120 secs for the firmware */
	to = jiffies + msecs_to_jiffies(120000);

	/* wait until uevent can be sent */
	while (kobject_uevent(&dev->kobj, KOBJ_CHANGE) &&
				time_after(to, jiffies))
		msleep(1000);

	if (time_before(to, jiffies)) {
		ret = -ETIME;
		dev_err(dev, "failed waiting for udev %d\n", ret);
		return ret;
	}

	/* make some retries in case FS is not up yet */
	while (request_firmware(&fw, rproc->firmware, dev) &&
				time_after(to, jiffies))
		msleep(1000);

	if (!fw) {
		ret = -ETIME;
		dev_err(dev, "error %d requesting firmware %s\n",
							ret, rproc->firmware);
		return ret;
	}

	rproc_fw_config_virtio(fw, rproc);

	return 0;
}

/**
 * rproc_register() - register a remote processor
 * @rproc: the remote processor handle to register
 *
 * Registers @rproc with the remoteproc framework, after it has been
 * allocated with rproc_alloc().
 *
 * This is called by the platform-specific rproc implementation, whenever
 * a new remote processor device is probed.
 *
 * Returns 0 on success and an appropriate error code otherwise.
 *
 * Note: this function initiates an asynchronous firmware loading
 * context, which will look for virtio devices supported by the rproc's
 * firmware.
 *
 * If found, those virtio devices will be created and added, so as a result
 * of registering this remote processor, additional virtio drivers might be
 * probed.
 */
int rproc_register(struct rproc *rproc)
{
	struct device *dev = &rproc->dev;
	int ret = 0;

	ret = device_add(dev);
	if (ret < 0)
		return ret;

	/* expose to rproc_get_by_name users */
	klist_add_tail(&rproc->node, &rprocs);

	dev_info(dev, "%s is available\n", rproc->name);

	dev_info(dev, "Note: remoteproc is still under development and considered experimental.\n");
	dev_info(dev, "THE BINARY FORMAT IS NOT YET FINALIZED, and backward compatibility isn't yet guaranteed.\n");

	/* create debugfs entries */
	rproc_create_debug_dir(rproc);

	/* rproc_unregister() calls must wait until async loader completes */
	init_completion(&rproc->firmware_loading_complete);

	/* initialize secure service */
	rproc_secure_reset(rproc);

	/*
	 * We must retrieve early virtio configuration info from
	 * the firmware (e.g. whether to register a virtio device,
	 * what virtio features does it support, ...).
	 *
	 * We're initiating an asynchronous firmware loading, so we can
	 * be built-in kernel code, without hanging the boot process.
	 */
	kthread_run((void *)rproc_loader_thread, rproc, "rproc_loader");
	if (ret < 0) {
		dev_err(dev, "request_firmware_nowait failed: %d\n", ret);
		complete_all(&rproc->firmware_loading_complete);
		klist_remove(&rproc->node);
	}

	return ret;
}
EXPORT_SYMBOL(rproc_register);

static void rproc_class_release(struct device *dev)
{
	struct rproc *rproc = container_of(dev, struct rproc, dev);

	kfree(rproc->memory_pool);
	kfree(rproc);
}

static struct class rproc_class = {
	.name		= "rproc",
	.owner		= THIS_MODULE,
	.dev_release	= rproc_class_release,
	.pm		= &rproc_pm_ops,
};

/**
 * rproc_alloc() - allocate a remote processor handle
 * @dev: the underlying device
 * @name: name of this remote processor
 * @ops: platform-specific handlers (mainly start/stop)
 * @firmware: name of firmware file to load
 * @pool_data: optional carveout pool data
 * @len: length of private data needed by the rproc driver (in bytes)
 *
 * Allocates a new remote processor handle, but does not register
 * it yet.
 *
 * This function should be used by rproc implementations during initialization
 * of the remote processor.
 *
 * After creating an rproc handle using this function, and when ready,
 * implementations should then call rproc_register() to complete
 * the registration of the remote processor.
 *
 * On success the new rproc is returned, and on failure, NULL.
 *
 * Note: _never_ directly deallocate @rproc, even if it was not registered
 * yet. Instead, if you just need to unroll rproc_alloc(), use rproc_free().
 */
struct rproc *rproc_alloc(struct device *dev, const char *name,
				const struct rproc_ops *ops,
				const char *firmware,
				struct rproc_mem_pool_data *pool_data,
				int len)
{
	struct rproc *rproc;
	struct rproc_mem_pool *pool = NULL;

	if (!dev || !name || !ops)
		return NULL;

	rproc = kzalloc(sizeof(struct rproc) + len, GFP_KERNEL);
	if (!rproc) {
		dev_err(dev, "%s: kzalloc failed\n", __func__);
		return NULL;
	}

	if (pool_data) {
		rproc->memory_pool = kzalloc(sizeof(*pool), GFP_KERNEL);
		if (!rproc->memory_pool) {
			dev_err(dev, "%s: kzalloc failed\n", __func__);
			kfree(rproc);
			return NULL;
		}
	}

	rproc->name = name;
	rproc->ops = ops;
	rproc->firmware = firmware;
	rproc->priv = &rproc[1];

	/*
	 * the current ranges for different sections are roughly chosen
	 * based on the current OMAP configuration of 512 bytes each
	 * for 512 vring buffers, and the alignment CMA gives back for
	 * these sizes.
	 */
	if (pool_data) {
		pool = rproc->memory_pool;
		pool->mem_base = pool_data->mem_base;
		pool->mem_size = pool_data->mem_size;
		pool->cur_ipc_base = pool->mem_base;
		pool->cur_ipc_size = SZ_256K;
		pool->cur_ipcbuf_base = pool->cur_ipc_base +
							pool->cur_ipc_size;
		pool->cur_ipcbuf_size = SZ_1M - pool->cur_ipc_size;
		pool->cur_fw_base = pool->mem_base + SZ_1M;
		pool->cur_fw_size = pool->mem_size - SZ_1M;
	}

	device_initialize(&rproc->dev);
	rproc->dev.parent = dev;
	rproc->dev.class = &rproc_class;

	/* Assign a unique device index and name */
	rproc->index = dev_index++;
	dev_set_name(&rproc->dev, "remoteproc%d", rproc->index);

	atomic_set(&rproc->power, 0);

	kref_init(&rproc->refcount);

	mutex_init(&rproc->lock);

	idr_init(&rproc->notifyids);

	INIT_LIST_HEAD(&rproc->carveouts);
	INIT_LIST_HEAD(&rproc->mappings);
	INIT_LIST_HEAD(&rproc->traces);
	INIT_LIST_HEAD(&rproc->last_traces);
	INIT_LIST_HEAD(&rproc->rvdevs);

	INIT_WORK(&rproc->error_handler, rproc_error_handler_work);

	rproc_secure_init(rproc);

	rproc->state = RPROC_OFFLINE;

	return rproc;
}
EXPORT_SYMBOL(rproc_alloc);

/**
 * rproc_free() - free an rproc handle that was allocated by rproc_alloc
 * @rproc: the remote processor handle
 *
 * This function should _only_ be used if @rproc was only allocated,
 * but not registered yet.
 *
 * If @rproc was already successfully registered (by calling rproc_register()),
 * then use rproc_unregister() instead.
 */
void rproc_free(struct rproc *rproc)
{
	idr_remove_all(&rproc->notifyids);
	idr_destroy(&rproc->notifyids);

	put_device(&rproc->dev);
}
EXPORT_SYMBOL(rproc_free);

/**
 * rproc_unregister() - unregister a remote processor
 * @rproc: rproc handle to unregister
 *
 * Unregisters a remote processor, and decrements its refcount.
 * If its refcount drops to zero, then @rproc will be freed. If not,
 * it will be freed later once the last reference is dropped.
 *
 * This function should be called when the platform specific rproc
 * implementation decides to remove the rproc device. it should
 * _only_ be called if a previous invocation of rproc_register()
 * has completed successfully.
 *
 * After rproc_unregister() returns, @rproc is _not_ valid anymore and
 * it shouldn't be used. More specifically, don't call rproc_free()
 * or try to directly free @rproc after rproc_unregister() returns;
 * none of these are needed, and calling them is a bug.
 *
 * Returns 0 on success and -EINVAL if @rproc isn't valid.
 */
int rproc_unregister(struct rproc *rproc)
{
	struct rproc_vdev *rvdev, *tmp;

	if (!rproc)
		return -EINVAL;

	/* if rproc is just being registered, wait */
	wait_for_completion(&rproc->firmware_loading_complete);

	/* clean up remote vdev entries */
	list_for_each_entry_safe(rvdev, tmp, &rproc->rvdevs, node)
		rproc_remove_virtio_dev(rvdev);

	/* the rproc is downref'ed as soon as it's removed from the klist */
	klist_del(&rproc->node);

	device_del(&rproc->dev);

	/* the rproc will only be released after its refcount drops to zero */
	kref_put(&rproc->refcount, rproc_release);

	return 0;
}
EXPORT_SYMBOL(rproc_unregister);

static int __init remoteproc_init(void)
{
	int ret;

	ret = class_register(&rproc_class);
	if (ret)
		return ret;

	rproc_init_debugfs();

	return 0;
}
module_init(remoteproc_init);

static void __exit remoteproc_exit(void)
{
	rproc_exit_debugfs();

	class_unregister(&rproc_class);
}
module_exit(remoteproc_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Generic Remote Processor Framework");
