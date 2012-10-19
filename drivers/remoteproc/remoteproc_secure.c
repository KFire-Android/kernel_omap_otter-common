/*
 * Remote Processor Framework
 *
 * Copyright (C) 2012 Texas Instruments, Inc.
 * Copyright (C) 2012 Google, Inc.
 *
 * Shahid Akhtar <sakhtar@ti.com>
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
#include <linux/remoteproc.h>
#include <linux/iommu.h>
#include <linux/klist.h>
#include <linux/elf.h>
#include <asm/byteorder.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/vmalloc.h>
#include <linux/rproc_drm.h>
#include <linux/rproc_secure.h>
#include "remoteproc_internal.h"

static struct completion secure_reload_complete;
static struct completion secure_complete;
static struct work_struct secure_validate;
static struct mutex secure_lock;
static enum rproc_secure_st secure_state;
static int secure_reload;
static struct rproc_sec_params *secure_params;
static rproc_drm_invoke_service_t rproc_secure_drm_service_alternate;
static int rproc_secure_drm_service(
	enum rproc_service_enum service,
	struct rproc_sec_params *rproc_sec_params);

#define dev_to_rproc(dev) container_of(dev, struct rproc, dev)

/**
 * rproc_secure_work() - authenticate ducati code sections
 *
 * Workqueue to authenticate all ducati code. Since the authentication
 * can take some time, we use a workqueue. If the authentication is
 * done while entering secure mode, set completion flag to notify
 * thread requesting reload
 */
static void rproc_secure_work(struct work_struct *work)
{
	int ret = 0;
	enum rproc_secure_st state = secure_state;

	/* call the secure mode API to validate code */
	ret = rproc_secure_drm_service(AUTHENTICATION_A2, secure_params);
	if (ret)
		pr_debug("%s: error failed to validate code\n", __func__);

	if (!ret)
		secure_state = RPROC_SECURE_AUTHENTICATED;
	else
		secure_state = RPROC_SECURE_OFF;

	if (state == RPROC_SECURE_RELOAD)
		complete_all(&secure_reload_complete);

	mutex_unlock(&secure_lock);
	complete_all(&secure_complete);
	return;
}

/**
 * rproc_secure_init() - initialize secure params
 *
 */
void rproc_secure_init(struct rproc *rproc)
{
	dev_dbg(&rproc->dev, "init secure service\n");

	secure_reload = 0;
	secure_state = RPROC_SECURE_OFF;
	secure_params = NULL;
	INIT_WORK(&secure_validate, rproc_secure_work);
	mutex_init(&secure_lock);
	return;
}

/**
 * rproc_secure_reset() - reset secure service and firewalls
 *
 */
void rproc_secure_reset(struct rproc *rproc)
{
	dev_dbg(&rproc->dev, "reseting secure service\n");

	rproc_secure_drm_service(AUTHENTICATION_A0, NULL);

	return;
}

/**
 * rproc_secure_boot() - Secure ducati code during boot
 * @rproc: rproc handle
 * @fw:	firmware image loaded into core
 *
 * Parse the fw image to locate sections and location of memory
 * that is to be firewalled for secure playback. Invoke secure
 * service to authenticate ducati boot code (blocking call) and
 * then invoke the workqueue to authenticate all remaining code.
 * In addition, it looks at all carveout memory to update a list
 * of TTBR entries that is provided to secure service.
 *
 * The function should be called after all code has been loaded to
 * memory. The .certificate section contains certificate used for
 * authentication. The .iommu_ttbr section contains list of TTBR
 * entries provided to secure service. The .secure_params section
 * contains the struct tf_rproc_sec_params that contains all data
 * passed to secure service. The .iommu_ttbr_updates section contains
 * details of carveout sections that are dynamically loaded and the
 * TTBR entries for these sections are updated here. Location of
 * secure input and secure_output buffers are also provided to the
 * secure service.
 *
 * Returns 0 on success and if image is unsigned
 */
int rproc_secure_boot(struct rproc *rproc, const struct firmware *fw)
{
	struct device *dev = &rproc->dev;
	int ret = 0;
	void *ptr;
	u32  ttbr_da = 0, toc_da = 0, *ttbr = NULL, i;
	struct rproc_iommu_ttbr_update *ttbru = NULL;
	struct rproc_mem_entry *maps = NULL;
	struct elf32_hdr *ehdr = (struct elf32_hdr *) fw->data;
	struct elf32_shdr *shdr =
		(struct elf32_shdr *) (fw->data + ehdr->e_shoff);
	const char *name_sect = fw->data + shdr[ehdr->e_shstrndx].sh_offset;

	/* enter secure authentication process */
	mutex_lock(&secure_lock);

	/* parse fw image to location of required data sections */
	for (i = 0; i < ehdr->e_shnum; i++, shdr++) {
		u32 filesz = shdr->sh_size;
		u32 da = shdr->sh_addr;
		const char *name = name_sect + shdr->sh_name;

		if (!strcmp(name, ".certificate")) {
			ptr = rproc_da_to_va(rproc, da, filesz);
			toc_da = da;
		} else if (!strcmp(name, ".iommu_ttbr")) {
			ptr = rproc_da_to_va(rproc, da, filesz);
			ttbr = (u32 *) ptr;
			ttbr_da = da;
		} else if (!strcmp(name, ".secure_params")) {
			ptr = rproc_da_to_va(rproc, da, filesz);
			secure_params = (struct rproc_sec_params *) ptr;
		} else if (!strcmp(name, ".iommu_ttbr_updates")) {
			ptr = rproc_da_to_va(rproc, da, filesz);
			ttbru = (struct rproc_iommu_ttbr_update *) ptr;
		} else
			continue;

		if (!ptr) {
			dev_err(dev, "bad phdr da 0x%x mem 0x%x\n",
				da, filesz);
			ret = -EINVAL;
			goto out;
		}
	}

	/* assume image is unsigned if required sections are missing */
	if (!toc_da || !ttbr || !secure_params || !ttbru) {
		dev_dbg(dev, "unsigned ducati image\n");
		goto out;
	}

	/* update params for secure service */
	list_for_each_entry(maps, &rproc->carveouts, node) {
		if (maps->memregion == RPROC_MEMREGION_CODE) {
			/* update location of carveout region */
			secure_params->ducati_base_address = maps->dma;
			secure_params->ducati_code = maps->dma;
			secure_params->ducati_code_size = maps->len;

			/* convert device address to physical addr */
			secure_params->ducati_toc_address =
				maps->dma - maps->da + toc_da;
			secure_params->ducati_page_table_address =
				maps->dma - maps->da + ttbr_da;
		}
		if (maps->memregion == RPROC_MEMREGION_DATA) {
			secure_params->ducati_data = maps->dma;
			secure_params->ducati_data_size = maps->len;
		}
		if (maps->memregion == RPROC_MEMREGION_SMEM) {
			secure_params->ducati_smem = maps->dma;
			secure_params->ducati_smem_size = maps->len;
		}
		/* update TTBR entries for this carveout region */
		for (i = 0; i < ttbru->count; i++)
			ttbr[ttbru->index + i] += maps->dma;
		ttbru++;
	}

	/* find location of vring and iobufs */
	list_for_each_entry(maps, &rproc->mappings, node) {
		if (maps->memregion == RPROC_MEMREGION_VRING) {
			secure_params->ducati_vring_smem = maps->dma;
			secure_params->ducati_vring_size = maps->len;
		}
		if (maps->memregion == RPROC_MEMREGION_1D) {
			/* TODO: Hardcoded NEED to be acquired from ION */
			secure_params->secure_buffer_address = maps->dma;
			secure_params->secure_buffer_size = maps->len;
		}
	}

	/* TODO: Hardcoded NEED to be acquired from ION */
	secure_params->decoded_buffer_address = (dma_addr_t) 0xB5200000;
	secure_params->decoded_buffer_size = (uint32_t) 0x5100000;

	/* validate boot section */
	ret = rproc_secure_drm_service(AUTHENTICATION_A1, secure_params);
	if (ret) {
		dev_err(dev, "error failed to validate boot code\n");
		goto out;
	}

	/* validate all code */
	init_completion(&secure_complete);
	schedule_work(&secure_validate);

	return ret;
out:
	if (secure_state == RPROC_SECURE_RELOAD)
		complete_all(&secure_reload_complete);

	mutex_unlock(&secure_lock);
	return ret;
}

/**
 * rproc_set_secure() - Enable/Disable secure mode
 * @name: name of remote core
 * @enable: 1 to enable and 0 to disable secure mode
 *
 * Called by user to enable secure mode for ducati code. If the
 * ducati image is not authenticated, the image will be reloaded
 * and authenticated before firewalls are enabled. The reload is
 * done only once.
 *
 * Returns 0 on success
 */
int rproc_set_secure(const char *name, bool enable)
{
	int ret = 0;

	if (!secure_params)
		return -ENODEV;

	if (enable) { /* entering secure mode */
		/* enter secure mode, reload once if fails */
		ret = rproc_secure_drm_service(
				ENTER_SECURE_PLAYBACK_AFTER_AUTHENTICATION,
				secure_params);

		/* TODO: Reload only if return value is non-zero */

		/* reload code to authenticate */
		secure_state = RPROC_SECURE_RELOAD;
		init_completion(&secure_reload_complete);
		ret = rproc_reload(name);
		if (ret)
			goto out;
		wait_for_completion(&secure_reload_complete);

		/* authentication failed after reload */
		if (secure_state != RPROC_SECURE_AUTHENTICATED) {
			ret = -ENODEV; /* authentication failed */
			pr_err("%s: failed authentication after reload\n",
				__func__);
			goto out;
		}

		/* invoke secure service for secure mode */
		ret = rproc_secure_drm_service(
			ENTER_SECURE_PLAYBACK_AFTER_AUTHENTICATION,
			secure_params);
	} else { /* disable secure mode */
		/* invoke service to exit secure mode */
		ret = rproc_secure_drm_service(EXIT_SECURE_PLAYBACK,
			secure_params);
		if (ret)
			pr_err("%s: error disabling secure mode %d\n",
				__func__, ret);
	}
out:
	return ret;
}
EXPORT_SYMBOL(rproc_set_secure);

/**
 * rproc_secure_drm_service() - invoke the specified rproc_drm service
 * @service: the secure service that is being invoked
 * @rproc_sec_params: secure params needed by the service
 *
 * returns 0 on success
 */
static int rproc_secure_drm_service(enum rproc_service_enum service,
				    struct rproc_sec_params
				    *rproc_sec_params)
{
	int ret = 0;
	if (rproc_secure_drm_service_alternate)
		ret = rproc_secure_drm_service_alternate(service,
							 rproc_sec_params);
	return ret;
}

/**
 * rproc_register_drm_service() - called by the rproc_drm module
 * to register the service.
 * @rproc_drm_service: the rproc_drm service that is being registered
 *
 * remoteproc calls this rproc_drm.rproc_drm_invoke_service upon calls
 *  to rproc_drm_invoke_service
 *
 * returns 0 on success
 */
int rproc_register_drm_service(rproc_drm_invoke_service_t
			       rproc_drm_service)
{
	if (rproc_drm_service == NULL)
		return -ENODEV;

	rproc_secure_drm_service_alternate = rproc_drm_service ;
	return 0;

}
EXPORT_SYMBOL(rproc_register_drm_service);
