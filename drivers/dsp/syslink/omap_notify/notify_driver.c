/*
 * notify_driver.c
 *
 * Syslink driver support for OMAP Processors.
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

#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/uaccess.h>
#include <linux/spinlock.h>
#include <linux/io.h>
#include <asm/pgtable.h>

#include <syslink/gt.h>
#include <syslink/notify.h>
#include <syslink/notify_driver.h>
#include <syslink/notifydefs.h>
#include <syslink/atomic_linux.h>


/* Function to register driver with the Notify module. */
int notify_register_driver(u16 remote_proc_id,
			u16 line_id,
			struct notify_driver_fxn_table *fxn_table,
			struct notify_driver_object **driver_handle)
{
	int status = NOTIFY_S_SUCCESS;
	struct notify_driver_object *drv_handle = NULL;

	if (WARN_ON(atomic_cmpmask_and_lt(&(notify_state.ref_count),
			NOTIFY_MAKE_MAGICSTAMP(0),
			NOTIFY_MAKE_MAGICSTAMP(1)) == true)) {
		status = NOTIFY_E_INVALIDSTATE;
		goto exit;
	}
	if (WARN_ON(fxn_table == NULL)) {
		status = NOTIFY_E_INVALIDARG;
		goto exit;
	}
	if (WARN_ON(remote_proc_id >= multiproc_get_num_processors())) {
		status = NOTIFY_E_INVALIDARG;
		goto exit;
	}
	if (WARN_ON(line_id >= NOTIFY_MAX_INTLINES)) {
		status = NOTIFY_E_INVALIDARG;
		goto exit;
	}
	if (WARN_ON(driver_handle == NULL)) {
		status = NOTIFY_E_INVALIDARG;
		goto exit;
	}

	*driver_handle = NULL;
	if (mutex_lock_interruptible(notify_state.gate_handle) != 0)
		WARN_ON(1);
	drv_handle = &(notify_state.drivers[remote_proc_id][line_id]);
	if (drv_handle->is_init == NOTIFY_DRIVERINITSTATUS_DONE) {
		status = NOTIFY_E_ALREADYEXISTS;
		mutex_unlock(notify_state.gate_handle);
		goto exit;
	}
	mutex_unlock(notify_state.gate_handle);
	WARN_ON(status < 0);

	/*Complete registration of the driver. */
	memcpy(&(drv_handle->fxn_table), fxn_table,
				sizeof(struct notify_driver_fxn_table));
	drv_handle->notify_handle = NULL; /* Initialize to NULL. */
	drv_handle->is_init = NOTIFY_DRIVERINITSTATUS_DONE;
	*driver_handle = drv_handle;

exit:
	return status;
}
EXPORT_SYMBOL(notify_register_driver);

/* Function to unregister driver with the Notify module. */
int notify_unregister_driver(struct notify_driver_object *drv_handle)
{
	int status = NOTIFY_E_FAIL;
	s32 key;

	if (WARN_ON(atomic_cmpmask_and_lt(&(notify_state.ref_count),
			NOTIFY_MAKE_MAGICSTAMP(0),
			NOTIFY_MAKE_MAGICSTAMP(1)) == true)) {
		status = NOTIFY_E_INVALIDSTATE;
		goto exit;
	}
	if (WARN_ON(drv_handle == NULL)) {
		status = NOTIFY_E_INVALIDARG;
		goto exit;
	}

	key = mutex_lock_interruptible(notify_state.gate_handle);
	if (key)
		goto exit;

	/* Unregister the driver. */
	drv_handle->is_init = NOTIFY_DRIVERINITSTATUS_NOTDONE;
	mutex_unlock(notify_state.gate_handle);
	status = NOTIFY_S_SUCCESS;

exit:
	return status;
}
EXPORT_SYMBOL(notify_unregister_driver);

/* Function to set the Notify object handle maintained within the
 * Notify module. */
int notify_set_driver_handle(u16 remote_proc_id, u16 line_id,
				struct notify_object *handle)
{
	s32 status = NOTIFY_S_SUCCESS;

	/* Handle can be set to NULL */
	if (WARN_ON(atomic_cmpmask_and_lt(&(notify_state.ref_count),
			NOTIFY_MAKE_MAGICSTAMP(0),
			NOTIFY_MAKE_MAGICSTAMP(1)) == true)) {
		status = NOTIFY_E_INVALIDSTATE;
		goto exit;
	}
	if (WARN_ON(remote_proc_id >= multiproc_get_num_processors())) {
		status = NOTIFY_E_INVALIDARG;
		goto exit;
	}
	if (WARN_ON(line_id >= NOTIFY_MAX_INTLINES)) {
		status = NOTIFY_E_INVALIDARG;
		goto exit;
	}

	notify_state.drivers[remote_proc_id][line_id].notify_handle = handle;

exit:
	return status;
}
EXPORT_SYMBOL(notify_set_driver_handle);


/* Function to find and return the driver handle maintained within
 * the Notify module. */
struct notify_driver_object *notify_get_driver_handle(u16 remote_proc_id,
					u16 line_id)
{
	struct notify_driver_object *handle = NULL;
	s32 status = NOTIFY_S_SUCCESS;

	if (WARN_ON(atomic_cmpmask_and_lt(&(notify_state.ref_count),
			NOTIFY_MAKE_MAGICSTAMP(0),
			NOTIFY_MAKE_MAGICSTAMP(1)) == true)) {
		status = NOTIFY_E_INVALIDSTATE;
		goto exit;
	}
	if (WARN_ON(remote_proc_id >= multiproc_get_num_processors())) {
		status = NOTIFY_E_INVALIDARG;
		goto exit;
	}
	if (WARN_ON(line_id >= NOTIFY_MAX_INTLINES)) {
		status = NOTIFY_E_INVALIDARG;
		goto exit;
	}

	handle = &(notify_state.drivers[remote_proc_id][line_id]);
	/* Check whether the driver handle slot is occupied. */
	if (handle->is_init == NOTIFY_DRIVERINITSTATUS_NOTDONE)
		handle = NULL;

exit:
	if (status < 0) {
		pr_err("notify_get_driver_handle failed! "
			"status = 0x%x\n", status);
	}
	return handle;
}
EXPORT_SYMBOL(notify_get_driver_handle);
