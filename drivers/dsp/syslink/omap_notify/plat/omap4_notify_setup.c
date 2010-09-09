/*
 * omap4_notify_setup.c
 *
 * OMAP4 device-specific functions to setup the Notify module.
 *
 * Copyright (C) 2008-2009 Texas Instruments, Inc.
 *
 * This package is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * THIS PACKAGE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

/* Linux headers */
#include <linux/spinlock.h>
/*#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/uaccess.h>
#include <linux/io.h>
#include <asm/pgtable.h>*/

/* Module headers */
#include <syslink/multiproc.h>

#include <syslink/notify.h>
#include <syslink/notify_setup_proxy.h>
#include <syslink/notify_ducatidriver.h>
#include <syslink/notify_driver.h>
#include <syslink/notifydefs.h>


/* Handle to the NotifyDriver for line 0 */
static struct notify_ducatidrv_object *notify_setup_driver_handles[
						MULTIPROC_MAXPROCESSORS];

/* Handle to the Notify objects */
static
struct notify_object *notify_setup_notify_handles[MULTIPROC_MAXPROCESSORS];


/* Function to perform device specific setup for Notify module.
 * This function creates the Notify drivers. */
int notify_setup_omap4_attach(u16 proc_id, void *shared_addr)
{
	s32 status = NOTIFY_S_SUCCESS;
	struct notify_ducatidrv_params notify_shm_params;

	if (WARN_ON(unlikely(shared_addr == NULL))) {
		status = NOTIFY_E_INVALIDARG;
		goto exit;
	}
	if (WARN_ON(unlikely(proc_id == multiproc_self()))) {
		status = NOTIFY_E_INVALIDARG;
		goto exit;
	}

	notify_ducatidrv_params_init(&notify_shm_params);

	/* Currently not supporting caching on host side. */
	notify_shm_params.cache_enabled = false;
	notify_shm_params.line_id = 0;
	notify_shm_params.local_int_id = 77u; /* TBD: Ipc_getConfig */
	notify_shm_params.remote_int_id = 0u; /* TBD: Ipc_getConfig */
	notify_shm_params.remote_proc_id = proc_id;
	notify_shm_params.shared_addr = shared_addr;

	notify_setup_driver_handles[proc_id] = notify_ducatidrv_create(
							&notify_shm_params);
	if (notify_setup_driver_handles[proc_id] == NULL) {
		status = NOTIFY_E_FAIL;
		printk(KERN_ERR "notify_setup_omap4_attach: "
			"notify_ducatidrv_create failed! status = 0x%x",
			status);
		goto exit;
	}

	notify_setup_notify_handles[proc_id] = \
			notify_create(notify_setup_driver_handles[proc_id],
					proc_id, 0u, NULL);
	if (notify_setup_notify_handles[proc_id] == NULL) {
		status = NOTIFY_E_FAIL;
		printk(KERN_ERR "notify_setup_omap4_attach: notify_create "
			"failed!");
		goto exit;
	}

exit:
	if (status < 0) {
		printk(KERN_ERR "notify_setup_omap4_attach failed! "
			"status = 0x%x", status);
	}
	return status;
}


/* Function to perform device specific destroy for Notify module.
 * This function deletes the Notify drivers. */
int notify_setup_omap4_detach(u16 proc_id)
{
	s32 status = NOTIFY_S_SUCCESS;
	s32 tmp_status = NOTIFY_S_SUCCESS;

	if (WARN_ON(unlikely(proc_id == multiproc_self()))) {
		status = NOTIFY_E_INVALIDARG;
		goto exit;
	}

	/* Delete the notify driver to the M3 (Line 0) */
	status = notify_delete(&(notify_setup_notify_handles[proc_id]));
	if (status < 0) {
		printk(KERN_ERR "notify_setup_omap4_detach: notify_delete "
			"failed for line 0!");
	}

	tmp_status = notify_ducatidrv_delete(
				&(notify_setup_driver_handles[proc_id]));
	if ((tmp_status < 0) && (status >= 0)) {
		status = tmp_status;
		printk(KERN_ERR "notify_setup_omap4_detach: "
			"notify_ducatidrv_delete failed for line 0!");
	}

exit:
	if (status < 0) {
		printk(KERN_ERR "notify_setup_omap4_detach failed! "
			"status = 0x%x", status);
	}
	return status;
}


/* Return the amount of shared memory required  */
uint notify_setup_omap4_shared_mem_req(u16 remote_proc_id, void *shared_addr)
{
	int status = NOTIFY_S_SUCCESS;
	uint mem_req = 0x0;
	struct notify_ducatidrv_params params;

	if (WARN_ON(unlikely(shared_addr == NULL))) {
		status = NOTIFY_E_INVALIDARG;
		goto exit;
	}

	notify_ducatidrv_params_init(&params);
	params.shared_addr = shared_addr;

	mem_req = notify_ducatidrv_shared_mem_req(&params);

exit:
	if (status < 0) {
		printk(KERN_ERR "notify_setup_omap4_shared_mem_req failed!"
			" status = 0x%x", status);
	}
	return mem_req;
}

bool notify_setup_omap4_int_line_available(u16 remote_proc_id)
{
	return true;
}
