/*
 *  transportshm_setup.c
 *
 *  Shared Memory Transport setup module
 *
 *  Copyright (C) 2009 Texas Instruments, Inc.
 *
 *  This package is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 *
 *  THIS PACKAGE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 *  IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 *  WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR
 *  PURPOSE.
 */

/* Standard headers */
#include <linux/types.h>

/* Utilities headers */
#include <linux/string.h>
#include <linux/slab.h>

/* Syslink headers */
#include <syslink/atomic_linux.h>
/* Module level headers */
#include <multiproc.h>
#include <sharedregion.h>
#include <nameserver.h>
#include <gatepeterson.h>
#include <notify.h>
#include <messageq.h>
#include <listmp.h>
#include <gatemp.h>
#include <transportshm.h>
#include <transportshm_setup.h>

/* =============================================================================
 * Structures & Enums
 * =============================================================================
 */
/* structure for transportshm_setup module state */
struct transportshm_setup_module_object {
	void *handles[MULTIPROC_MAXPROCESSORS];
	/* Store a handle per remote proc */
};


/* =============================================================================
 *  Globals
 * =============================================================================
 */
static struct transportshm_setup_module_object transportshm_setup_state = {
	.handles[0] = NULL
};

/* Pointer to the transportshm_setup module state */
static struct transportshm_setup_module_object *transportshm_setup_module =
						&transportshm_setup_state;


/* =============================================================================
 *  Functions
 * =============================================================================
 */
/*
 * =========== transportshm_setup_attach ===========
 * Function that will be called in messageq_attach.  Creates a
 * transportshm object for a given processor
 */
int transportshm_setup_attach(u16 remote_proc_id, u32 *shared_addr)
{
	s32 status = 0;
	struct transportshm_params params;
	void *handle;

	BUG_ON(remote_proc_id >= MULTIPROC_MAXPROCESSORS);

	if (WARN_ON(unlikely(shared_addr == NULL))) {
		status = -EINVAL;
		goto exit;
	}

	/* Init the transport parameters */
	transportshm_params_init(&params);
	params.gate = gatemp_get_default_remote();
	params.shared_addr = shared_addr;

	/* Make sure notify driver has been created */
	if (unlikely(notify_is_registered(remote_proc_id, 0) == false)) {
		status = TRANSPORTSHM_E_FAIL;
		goto exit;
	}

	if (multiproc_self() < remote_proc_id) {
		handle = transportshm_create(remote_proc_id, &params);
		if (unlikely(handle == NULL)) {
			status = TRANSPORTSHM_E_FAIL;
			goto exit;
		}

		transportshm_setup_module->handles[remote_proc_id] = handle;
	} else {
		status = transportshm_open_by_addr(params.shared_addr, &handle);
		if (status < 0)
			goto exit;

		transportshm_setup_module->handles[remote_proc_id] = handle;
	}

exit:
	if (status < 0)
		printk(KERN_ERR "transportshm_setup_attach failed! status "
			"= 0x%x", status);
	return status;
}

/*
 * =========== transportshm_setup_detach ===========
 * Function that will be called in messageq_detach.  Deletes a
 * transportshm object created by transportshm_setup_attach.
 */
int transportshm_setup_detach(u16 remote_proc_id)
{
	int status = 0;
	void *handle = NULL;

	if (WARN_ON(remote_proc_id >= MULTIPROC_MAXPROCESSORS)) {
		status = -EINVAL;
		goto exit;
	}

	handle = transportshm_setup_module->handles[remote_proc_id];
	if (WARN_ON(unlikely(handle == NULL))) {
		status = -EINVAL;
		goto exit;
	}

	if (multiproc_self() < remote_proc_id) {
		/* Delete the transport */
		status = transportshm_delete(&handle);
		if (unlikely(status < 0)) {
			status = TRANSPORTSHM_E_FAIL;
			goto exit;
		}
		transportshm_setup_module->handles[remote_proc_id] = NULL;
	} else {
		status = transportshm_close(&handle);
		if (unlikely(status < 0)) {
			status = TRANSPORTSHM_E_FAIL;
			goto exit;
		}
		transportshm_setup_module->handles[remote_proc_id] = NULL;
	}

exit:
	if (status < 0)
		printk(KERN_ERR "transportshm_setup_detach failed! status "
			"= 0x%x", status);
	return status;
}


/*
 * =========== transportshm_setup_shared_mem_req ===========
 * Function that returns the amount of shared memory required
 */
u32 transportshm_setup_shared_mem_req(u32 *shared_addr)
{
	u32 mem_req = 0x0;
	int status = 0;
	struct transportshm_params params;

	/* Don't do anything if only 1 processor in system */
	if (likely(multiproc_get_num_processors() != 1)) {
		BUG_ON(shared_addr == NULL);

		if (unlikely(shared_addr == NULL)) {
			status = -EINVAL;
			goto exit;
		}

		transportshm_params_init(&params);
		params.shared_addr = shared_addr;

		mem_req += transportshm_shared_mem_req(&params);
	}

exit:
	if (status < 0)
		printk(KERN_ERR "transportshm_setup_shared_mem_req failed! "
			"status = 0x%x", status);
	return mem_req;
}

/* Determines if a transport has been registered to a remote processor */
bool transportshm_setup_is_registered(u16 remote_proc_id)
{
	bool registered;

	registered = (transportshm_setup_module->handles[remote_proc_id] !=
				NULL);

	return registered;
}
