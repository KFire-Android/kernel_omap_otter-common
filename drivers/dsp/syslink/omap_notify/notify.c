/*
 * notify.c
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
#include <linux/spinlock.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/uaccess.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <asm/pgtable.h>

#include "../ipu_pm/ipu_pm.h"

#include <syslink/atomic_linux.h>
#include <syslink/multiproc.h>
#include <syslink/notify.h>
#include <syslink/_notify.h>
#include <syslink/notifydefs.h>
#include <syslink/notify_driver.h>
#include <syslink/notify_setup_proxy.h>

struct notify_event_listener {
	struct list_head element;
	struct notify_event_callback callback;
};

/* Function registered with notify_exec when multiple registrations are present
 * for the events. */
static void _notify_exec_many(u16 proc_id, u16 line_id, u32 event_id, uint *arg,
			u32 payload);

struct notify_module_object notify_state = {
	.def_cfg.num_events = 32u,
	.def_cfg.send_event_poll_count = -1u,
	.def_cfg.num_lines = 1u,
	.def_cfg.reserved_events = 5u,
	.gate_handle = NULL,
	.local_notify_handle = NULL
};

/* Get the default configuration for the Notify module. */
void notify_get_config(struct notify_config *cfg)
{
	s32 retval = 0;

	if (WARN_ON(unlikely(cfg == NULL))) {
		retval = -EINVAL;
		goto exit;
	}

	if (atomic_cmpmask_and_lt(&(notify_state.ref_count),
			NOTIFY_MAKE_MAGICSTAMP(0),
			NOTIFY_MAKE_MAGICSTAMP(1)) == true)
		memcpy(cfg, &notify_state.def_cfg,
			sizeof(struct notify_config));
	else
		memcpy(cfg, &notify_state.cfg, sizeof(struct notify_config));

exit:
	if (retval < 0)
		pr_err("notify_get_config failed! status = 0x%x", retval);
	return;
}
EXPORT_SYMBOL(notify_get_config);

/* This function sets up the Notify module. This function must be called
 * before any other instance-level APIs can be invoked. */
int notify_setup(struct notify_config *cfg)
{
	int status = NOTIFY_S_SUCCESS;
	struct notify_config tmp_cfg;

	atomic_cmpmask_and_set(&notify_state.ref_count,
				NOTIFY_MAKE_MAGICSTAMP(0),
				NOTIFY_MAKE_MAGICSTAMP(0));
	if (atomic_inc_return(&notify_state.ref_count)
				!= NOTIFY_MAKE_MAGICSTAMP(1u)) {
		return NOTIFY_S_ALREADYSETUP;
	}

	if (cfg == NULL) {
		notify_get_config(&tmp_cfg);
		cfg = &tmp_cfg;
	}

	if (cfg->num_events > NOTIFY_MAXEVENTS) {
		status = NOTIFY_E_INVALIDARG;
		goto exit;
	}
	if (cfg->num_lines > NOTIFY_MAX_INTLINES) {
		status = NOTIFY_E_INVALIDARG;
		goto exit;
	}
	if (cfg->reserved_events > cfg->num_events) {
		status = NOTIFY_E_INVALIDARG;
		goto exit;
	}

	notify_state.gate_handle = kmalloc(sizeof(struct mutex), GFP_ATOMIC);
	if (notify_state.gate_handle == NULL) {
		status = NOTIFY_E_FAIL;
		goto exit;
	}
	/*User has not provided any gate handle,
	so create a default handle.*/
	mutex_init(notify_state.gate_handle);

	memcpy(&notify_state.cfg, cfg, sizeof(struct notify_config));
	notify_state.local_enable_mask = -1u;
	notify_state.start_complete = false;
	memset(&notify_state.drivers, 0, (sizeof(struct notify_driver_object) *
				NOTIFY_MAX_DRIVERS * NOTIFY_MAX_INTLINES));

	/* tbd: Should return Notify_Handle */
	notify_state.local_notify_handle = notify_create(NULL, multiproc_self(),
								0, NULL);
	if (notify_state.local_notify_handle == NULL) {
		status = NOTIFY_E_FAIL;
		goto local_notify_fail;
	}
	return 0;

local_notify_fail:
	kfree(notify_state.gate_handle);
exit:
	atomic_set(&notify_state.ref_count, NOTIFY_MAKE_MAGICSTAMP(0));
	pr_err("notify_setup failed! status = 0x%x", status);
	return status;
}
EXPORT_SYMBOL(notify_setup);

/* Once this function is called, other Notify module APIs,
 * except for the Notify_getConfig API cannot be called anymore. */
int notify_destroy(void)
{
	int i;
	int j;
	int status = NOTIFY_S_SUCCESS;

	if (WARN_ON(unlikely(atomic_cmpmask_and_lt(&(notify_state.ref_count),
				NOTIFY_MAKE_MAGICSTAMP(0),
				NOTIFY_MAKE_MAGICSTAMP(1)) == true))) {
		status = NOTIFY_E_INVALIDSTATE;
		goto exit;
	}
	if (!(atomic_dec_return(&notify_state.ref_count)
			== NOTIFY_MAKE_MAGICSTAMP(0)))
		return NOTIFY_S_ALREADYSETUP;

	/* Temporarily increment refCount here. */
	atomic_set(&notify_state.ref_count, NOTIFY_MAKE_MAGICSTAMP(1));
	if (notify_state.local_notify_handle != NULL)
		status = notify_delete(&notify_state.local_notify_handle);
	atomic_set(&notify_state.ref_count, NOTIFY_MAKE_MAGICSTAMP(0));

	/* Check if any Notify driver instances have
	 * not been deleted so far. If not, assert. */
	for (i = 0; i < NOTIFY_MAX_DRIVERS; i++)
		for (j = 0; j < NOTIFY_MAX_INTLINES; j++)
			WARN_ON(notify_state.drivers[i][j].is_init != false);

	kfree(notify_state.gate_handle);

exit:
	if (status < 0)
		pr_err("notify_destroy failed! status = 0x%x", status);
	return status;
}
EXPORT_SYMBOL(notify_destroy);

/* Function to create an instance of Notify driver */
struct notify_object *notify_create(void *driver_handle, u16 remote_proc_id,
			u16 line_id, const struct notify_params *params)
{
	int status = NOTIFY_S_SUCCESS;
	struct notify_object *obj = NULL;
	uint i;

	/* driver_handle can be NULL for local create */
	/* params can be NULL */

	if (WARN_ON(unlikely(atomic_cmpmask_and_lt(&(notify_state.ref_count),
				NOTIFY_MAKE_MAGICSTAMP(0),
				NOTIFY_MAKE_MAGICSTAMP(1)) == true))) {
		status = NOTIFY_E_INVALIDSTATE;
		goto exit;
	}
	if (WARN_ON(unlikely(remote_proc_id >= \
					multiproc_get_num_processors()))) {
		status = NOTIFY_E_INVALIDARG;
		goto exit;
	}
	if (WARN_ON(unlikely(line_id >= NOTIFY_MAX_INTLINES))) {
		status = NOTIFY_E_INVALIDARG;
		goto exit;
	}
	/* Allocate memory for the Notify object. */
	obj = kzalloc(sizeof(struct notify_object), GFP_KERNEL);
	if (obj == NULL) {
		status = NOTIFY_E_MEMORY;
		goto exit;
	}

	obj->remote_proc_id = remote_proc_id;
	obj->line_id = line_id;
	obj->nesting = 0;
	mutex_init(&obj->lock);

	for (i = 0; i < notify_state.cfg.num_events; i++)
		INIT_LIST_HEAD(&obj->event_list[i]);

	/* Used solely for remote driver
	 * (NULL if remote_proc_id == self) */
	obj->driver_handle = driver_handle;
	/* Send this handle to the NotifyDriver */
	status = notify_set_driver_handle(remote_proc_id, line_id, obj);
	if (status < 0)
		goto notify_handle_fail;

	/* For local notify */
	if (driver_handle == NULL)
		/* Set driver status to indicate that it is done. */
		notify_state.drivers[multiproc_self()][line_id].is_init =
			NOTIFY_DRIVERINITSTATUS_DONE;
	return obj;

notify_handle_fail:
	notify_set_driver_handle(remote_proc_id, line_id, NULL);
	kfree(obj);
	obj = NULL;
exit:
	if (status < 0)
		pr_err("notify_create failed! status = 0x%x", status);
	return obj;
}


/* Function to delete an instance of Notify driver */
int notify_delete(struct notify_object **handle_ptr)
{
	int status = NOTIFY_S_SUCCESS;
	struct notify_object *obj;
	u16 i;

	if (WARN_ON(unlikely(atomic_cmpmask_and_lt(&(notify_state.ref_count),
				NOTIFY_MAKE_MAGICSTAMP(0),
				NOTIFY_MAKE_MAGICSTAMP(1)) == true))) {
		status = NOTIFY_E_INVALIDSTATE;
		goto exit;
	}
	if (WARN_ON(unlikely((handle_ptr == NULL) || (*handle_ptr == NULL)))) {
		status = NOTIFY_E_INVALIDARG;
		goto exit;
	}

	obj = (struct notify_object *)(*handle_ptr);

	if (obj->remote_proc_id == multiproc_self()) {
		if (WARN_ON(obj->line_id >= NOTIFY_MAX_INTLINES ||
			multiproc_self() >= MULTIPROC_MAXPROCESSORS)) {
			status = NOTIFY_E_INVALIDARG;
			goto exit;
		}
		notify_state.drivers[multiproc_self()][obj->line_id].is_init =
			NOTIFY_DRIVERINITSTATUS_NOTDONE;
	}
	notify_set_driver_handle(obj->remote_proc_id, obj->line_id, NULL);
	for (i = 0; i < notify_state.cfg.num_events; i++)
		INIT_LIST_HEAD(&obj->event_list[i]);

	kfree(obj);
	obj = NULL;
	*handle_ptr = NULL;

exit:
	if (status < 0)
		pr_err("notify_delete failed! status = 0x%x", status);
	return status;
}

/* This function registers a callback for a specific event with the
 * Notify module. */
int notify_register_event(u16 proc_id, u16 line_id, u32 event_id,
		notify_fn_notify_cbck notify_callback_fxn, void *cbck_arg)
{
	int status = NOTIFY_S_SUCCESS;
	u32 stripped_event_id = (event_id & NOTIFY_EVENT_MASK);
	struct notify_driver_object *driver_handle;
	struct list_head *event_list;
	struct notify_event_listener *listener;
	bool list_was_empty;
	struct notify_object *obj;

	if (WARN_ON(unlikely(atomic_cmpmask_and_lt(&(notify_state.ref_count),
				NOTIFY_MAKE_MAGICSTAMP(0),
				NOTIFY_MAKE_MAGICSTAMP(1)) == true))) {
		status = NOTIFY_E_INVALIDSTATE;
		goto exit;
	}
	if (WARN_ON(unlikely(proc_id >= multiproc_get_num_processors()))) {
		status = NOTIFY_E_INVALIDARG;
		goto exit;
	}
	if (WARN_ON(unlikely(line_id >= NOTIFY_MAX_INTLINES))) {
		status = NOTIFY_E_INVALIDARG;
		goto exit;
	}
	if (WARN_ON(unlikely(notify_callback_fxn == NULL))) {
		status = NOTIFY_E_INVALIDARG;
		goto exit;
	}
	if (WARN_ON(unlikely((stripped_event_id >= \
					notify_state.cfg.num_events)))) {
		status = NOTIFY_E_EVTNOTREGISTERED;
		goto exit;
	}
	if (WARN_ON(unlikely(!ISRESERVED(event_id,
				notify_state.cfg.reserved_events)))) {
		status = NOTIFY_E_EVTRESERVED;
		goto exit;
	}

	driver_handle = notify_get_driver_handle(proc_id, line_id);
	if (WARN_ON(driver_handle == NULL)) {
		status = NOTIFY_E_DRIVERNOTREGISTERED;
		goto exit;
	}
	if (WARN_ON(driver_handle->is_init != NOTIFY_DRIVERINITSTATUS_DONE)) {
		status = NOTIFY_E_FAIL;
		goto exit;
	}

	obj = (struct notify_object *)driver_handle->notify_handle;
	if (WARN_ON(obj == NULL)) {
		status = NOTIFY_E_FAIL;
		goto exit;
	}

	listener = kmalloc(sizeof(struct notify_event_listener), GFP_KERNEL);
	if (listener == NULL) {
		status = NOTIFY_E_MEMORY;
		goto exit;
	}
	listener->callback.fn_notify_cbck = notify_callback_fxn;
	listener->callback.cbck_arg = cbck_arg;

	event_list = &(obj->event_list[stripped_event_id]);
	list_was_empty = list_empty(event_list);
	mutex_lock_killable(&obj->lock);
	list_add_tail((struct list_head *) listener, event_list);
	mutex_unlock(&obj->lock);
	if (list_was_empty) {
		/* Registering this event for the first time. Need to
		 * register the callback function.
		 */
		status = notify_register_event_single(proc_id, line_id,
					event_id, _notify_exec_many,
					(uint *) obj);
	}

exit:
	if (status < 0)
		pr_err("notify_register_event failed! status = 0x%x", status);
	return status;
}
EXPORT_SYMBOL(notify_register_event);

/* This function registers a single callback for a specific event with the
 * Notify module. */
int notify_register_event_single(u16 proc_id, u16 line_id, u32 event_id,
		notify_fn_notify_cbck notify_callback_fxn, void *cbck_arg)
{
	int status = NOTIFY_S_SUCCESS;
	u32 stripped_event_id = (event_id & NOTIFY_EVENT_MASK);
	struct notify_driver_object *driver_handle;
	struct notify_object *obj;

	if (WARN_ON(unlikely(atomic_cmpmask_and_lt(&(notify_state.ref_count),
				NOTIFY_MAKE_MAGICSTAMP(0),
				NOTIFY_MAKE_MAGICSTAMP(1)) == true))) {
		status = NOTIFY_E_INVALIDSTATE;
		goto exit;
	}
	if (WARN_ON(unlikely(proc_id >= multiproc_get_num_processors()))) {
		status = NOTIFY_E_INVALIDARG;
		goto exit;
	}
	if (WARN_ON(unlikely(line_id >= NOTIFY_MAX_INTLINES))) {
		status = NOTIFY_E_INVALIDARG;
		goto exit;
	}
	if (WARN_ON(unlikely(notify_callback_fxn == NULL))) {
		status = NOTIFY_E_INVALIDARG;
		goto exit;
	}
	if (WARN_ON(unlikely((stripped_event_id >= \
					notify_state.cfg.num_events)))) {
		status = NOTIFY_E_EVTNOTREGISTERED;
		goto exit;
	}
	if (WARN_ON(unlikely(!ISRESERVED(event_id,
				notify_state.cfg.reserved_events)))) {
		status = NOTIFY_E_EVTRESERVED;
		goto exit;
	}

	driver_handle = notify_get_driver_handle(proc_id, line_id);
	if (WARN_ON(driver_handle == NULL)) {
		status = NOTIFY_E_DRIVERNOTREGISTERED;
		goto exit;
	}
	if (WARN_ON(driver_handle->is_init != NOTIFY_DRIVERINITSTATUS_DONE)) {
		status = NOTIFY_E_FAIL;
		goto exit;
	}

	obj = (struct notify_object *)driver_handle->notify_handle;
	if (WARN_ON(obj == NULL)) {
		status = NOTIFY_E_FAIL;
		goto exit;
	}

	if (obj->callbacks[stripped_event_id].fn_notify_cbck != NULL) {
		status = NOTIFY_E_ALREADYEXISTS;
		goto exit;
	}

	obj->callbacks[stripped_event_id].fn_notify_cbck = notify_callback_fxn;
	obj->callbacks[stripped_event_id].cbck_arg = cbck_arg;

	if (proc_id != multiproc_self()) {
		status = driver_handle->fxn_table.register_event(driver_handle,
							stripped_event_id);
	}
exit:
	if (status < 0) {
		pr_err("notify_register_event_single failed! "
			"status = 0x%x", status);
	}
	return status;
}
EXPORT_SYMBOL(notify_register_event_single);

/* This function un-registers the callback for the specific event with
 * the Notify module. */
int notify_unregister_event(u16 proc_id, u16 line_id, u32 event_id,
		notify_fn_notify_cbck notify_callback_fxn, void *cbck_arg)
{
	int status = NOTIFY_S_SUCCESS;
	u32 stripped_event_id = (event_id & NOTIFY_EVENT_MASK);
	struct notify_event_listener *listener;
	bool found = false;
	struct notify_driver_object *driver_handle;
	struct list_head *event_list;
	struct notify_object *obj;
	/*int *sys_key;*/

	if (WARN_ON(unlikely(atomic_cmpmask_and_lt(&(notify_state.ref_count),
				NOTIFY_MAKE_MAGICSTAMP(0),
				NOTIFY_MAKE_MAGICSTAMP(1)) == true))) {
		status = NOTIFY_E_INVALIDSTATE;
		goto exit;
	}
	if (WARN_ON(unlikely(proc_id >= multiproc_get_num_processors()))) {
		status = NOTIFY_E_INVALIDARG;
		goto exit;
	}
	if (WARN_ON(unlikely(line_id >= NOTIFY_MAX_INTLINES))) {
		status = NOTIFY_E_INVALIDARG;
		goto exit;
	}
	if (WARN_ON(unlikely(notify_callback_fxn == NULL))) {
		status = NOTIFY_E_INVALIDARG;
		goto exit;
	}
	if (WARN_ON(unlikely((stripped_event_id >= \
					notify_state.cfg.num_events)))) {
		status = NOTIFY_E_EVTNOTREGISTERED;
		goto exit;
	}
	if (WARN_ON(unlikely(!ISRESERVED(event_id,
				notify_state.cfg.reserved_events)))) {
		status = NOTIFY_E_EVTRESERVED;
		goto exit;
	}

	driver_handle = notify_get_driver_handle(proc_id, line_id);
	if (WARN_ON(driver_handle == NULL)) {
		status = NOTIFY_E_DRIVERNOTREGISTERED;
		goto exit;
	}
	if (WARN_ON(driver_handle->is_init != NOTIFY_DRIVERINITSTATUS_DONE)) {
		status = NOTIFY_E_FAIL;
		goto exit;
	}

	obj = (struct notify_object *)driver_handle->notify_handle;
	if (WARN_ON(obj == NULL)) {
		status = NOTIFY_E_FAIL;
		goto exit;
	}

	event_list = &(obj->event_list[stripped_event_id]);
	if (list_empty(event_list)) {
		status = NOTIFY_E_NOTFOUND;
		goto exit;
	}

	mutex_lock_killable(&obj->lock);
	list_for_each_entry(listener, event_list, element) {
		/* Hash not matches, take next node */
		if ((listener->callback.fn_notify_cbck == notify_callback_fxn)
			&& (listener->callback.cbck_arg == cbck_arg)) {
			list_del((struct list_head *)listener);
			found = true;
			break;
		}
	}
	if (found == false) {
		status = NOTIFY_E_NOTFOUND;
		mutex_unlock(&obj->lock);
		goto exit;
	}

	if (list_empty(event_list)) {
		status = notify_unregister_event_single(proc_id, line_id,
								event_id);
	}
	mutex_unlock(&obj->lock);
	kfree(listener);
exit:
	if (status < 0) {
		pr_err("notify_unregister_event failed! "
			"status = 0x%x", status);
	}
	return status;
}
EXPORT_SYMBOL(notify_unregister_event);

/* This function un-registers a single callback for the specific event with
 * the Notify module. */
int notify_unregister_event_single(u16 proc_id, u16 line_id, u32 event_id)
{
	int status = NOTIFY_S_SUCCESS;
	u32 stripped_event_id = (event_id & NOTIFY_EVENT_MASK);
	struct notify_driver_object *driver_handle;
	struct notify_object *obj;

	if (WARN_ON(unlikely(atomic_cmpmask_and_lt(&(notify_state.ref_count),
				NOTIFY_MAKE_MAGICSTAMP(0),
				NOTIFY_MAKE_MAGICSTAMP(1)) == true))) {
		status = NOTIFY_E_INVALIDSTATE;
		goto exit;
	}
	if (WARN_ON(unlikely(proc_id >= multiproc_get_num_processors()))) {
		status = NOTIFY_E_INVALIDARG;
		goto exit;
	}
	if (WARN_ON(unlikely(line_id >= NOTIFY_MAX_INTLINES))) {
		status = NOTIFY_E_INVALIDARG;
		goto exit;
	}
	if (WARN_ON(unlikely((stripped_event_id >= \
					notify_state.cfg.num_events)))) {
		status = NOTIFY_E_EVTNOTREGISTERED;
		goto exit;
	}
	if (WARN_ON(unlikely(!ISRESERVED(event_id,
				notify_state.cfg.reserved_events)))) {
		status = NOTIFY_E_EVTRESERVED;
		goto exit;
	}

	driver_handle = notify_get_driver_handle(proc_id, line_id);
	if (WARN_ON(driver_handle == NULL)) {
		status = NOTIFY_E_DRIVERNOTREGISTERED;
		goto exit;
	}
	if (WARN_ON(driver_handle->is_init != NOTIFY_DRIVERINITSTATUS_DONE)) {
		status = NOTIFY_E_FAIL;
		goto exit;
	}

	obj = (struct notify_object *)driver_handle->notify_handle;
	if (WARN_ON(obj == NULL)) {
		status = NOTIFY_E_FAIL;
		goto exit;
	}

	if (obj->callbacks[stripped_event_id].fn_notify_cbck == NULL) {
		status = NOTIFY_E_FAIL;
		goto exit;
	}

	obj->callbacks[stripped_event_id].fn_notify_cbck = NULL;
	obj->callbacks[stripped_event_id].cbck_arg = NULL;
	if (proc_id != multiproc_self()) {
		status = driver_handle->fxn_table.unregister_event(
					driver_handle, stripped_event_id);
	}
exit:
	if (status < 0) {
		pr_err("notify_unregister_event_single failed! "
			"status = 0x%x", status);
	}
	return status;
}
EXPORT_SYMBOL(notify_unregister_event_single);

/* This function sends a notification to the specified event. */
int notify_send_event(u16 proc_id, u16 line_id, u32 event_id, u32 payload,
				bool wait_clear)
{
	int status = NOTIFY_S_SUCCESS;
	u32 stripped_event_id = (event_id & NOTIFY_EVENT_MASK);
	struct notify_driver_object *driver_handle;
	struct notify_object *obj;

	if (WARN_ON(unlikely(atomic_cmpmask_and_lt(&(notify_state.ref_count),
				NOTIFY_MAKE_MAGICSTAMP(0),
				NOTIFY_MAKE_MAGICSTAMP(1)) == true))) {
		status = NOTIFY_E_INVALIDSTATE;
		goto exit;
	}
	if (WARN_ON(unlikely(proc_id >= multiproc_get_num_processors()))) {
		status = NOTIFY_E_INVALIDARG;
		goto exit;
	}
	if (WARN_ON(unlikely(line_id >= NOTIFY_MAX_INTLINES))) {
		status = NOTIFY_E_INVALIDARG;
		goto exit;
	}
	if (WARN_ON(unlikely((stripped_event_id >= \
					notify_state.cfg.num_events)))) {
		status = NOTIFY_E_EVTNOTREGISTERED;
		goto exit;
	}
	if (WARN_ON(unlikely(!ISRESERVED(event_id,
				notify_state.cfg.reserved_events)))) {
		status = NOTIFY_E_EVTRESERVED;
		goto exit;
	}

	driver_handle = notify_get_driver_handle(proc_id, line_id);
	if (WARN_ON(driver_handle == NULL)) {
		status = NOTIFY_E_DRIVERNOTREGISTERED;
		goto exit;
	}
	if (WARN_ON(driver_handle->is_init != NOTIFY_DRIVERINITSTATUS_DONE)) {
		status = NOTIFY_E_FAIL;
		goto exit;
	}

	obj = (struct notify_object *)driver_handle->notify_handle;
	if (WARN_ON(obj == NULL)) {
		status = NOTIFY_E_FAIL;
		goto exit;
	}

	/* Maybe the proc is shutdown this functions will check and
	 * restore if needed.
	 * Currently only used to enable Idle and set HW_AUTO as
	 * the ducati_clkstctrl mode*/
	status = ipu_pm_restore_ctx(proc_id);
	if (status)
		goto exit;

	if (proc_id != multiproc_self()) {
		status = driver_handle->fxn_table.send_event(driver_handle,
					stripped_event_id, payload, wait_clear);
	} else {
		/* If nesting == 0 (the driver is enabled) and the event is
		 * enabled, send the event */
		if (obj->callbacks[stripped_event_id].fn_notify_cbck == NULL) {
			/* No callbacks are registered locally for the event. */
			status = NOTIFY_E_EVTNOTREGISTERED;
		} else if (obj->nesting != 0) {
			/* Driver is disabled */
			status = NOTIFY_E_FAIL;
		} else if (!test_bit(stripped_event_id, (unsigned long *)
				&notify_state.local_enable_mask)) {
			/* Event is disabled */
			status = NOTIFY_E_EVTDISABLED;
		} else {
			/* Execute the callback function registered to the
			 * event */
			notify_exec(obj, event_id, payload);
		}
	}
exit:
	if (status < 0)
		pr_err("notify_send_event failed! status = 0x%x", status);
	return status;
}
EXPORT_SYMBOL(notify_send_event);

/* This function disables all events. This is equivalent to global
 * interrupt disable, however restricted within interrupts handled by
 * the Notify module. All callbacks registered for all events are
 * disabled with this API. It is not possible to disable a specific
 * callback. */
u32 notify_disable(u16 proc_id, u16 line_id)
{
	uint key = 0;
	int status = NOTIFY_S_SUCCESS;
	struct notify_driver_object *driver_handle;
	struct notify_object *obj;

	if (WARN_ON(unlikely(atomic_cmpmask_and_lt(&(notify_state.ref_count),
				NOTIFY_MAKE_MAGICSTAMP(0),
				NOTIFY_MAKE_MAGICSTAMP(1)) == true))) {
		status = NOTIFY_E_INVALIDSTATE;
		goto exit;
	}
	if (WARN_ON(unlikely(proc_id >= multiproc_get_num_processors()))) {
		status = NOTIFY_E_INVALIDARG;
		goto exit;
	}
	if (WARN_ON(unlikely(line_id >= NOTIFY_MAX_INTLINES))) {
		status = NOTIFY_E_INVALIDARG;
		goto exit;
	}

	driver_handle = notify_get_driver_handle(proc_id, line_id);
	if (WARN_ON(driver_handle == NULL)) {
		status = NOTIFY_E_DRIVERNOTREGISTERED;
		goto exit;
	}
	if (WARN_ON(driver_handle->is_init != NOTIFY_DRIVERINITSTATUS_DONE)) {
		status = NOTIFY_E_FAIL;
		goto exit;
	}

	obj = (struct notify_object *)driver_handle->notify_handle;
	if (WARN_ON(obj == NULL)) {
		status = NOTIFY_E_FAIL;
		goto exit;
	}

	mutex_lock_killable(&obj->lock);
	obj->nesting++;
	if (obj->nesting == 1) {
		/* Disable receiving all events */
		if (proc_id != multiproc_self())
			driver_handle->fxn_table.disable(driver_handle);
	}
	key = obj->nesting;
	mutex_unlock(&obj->lock);
exit:
	if (status < 0)
		pr_err("notify_disable failed! status = 0x%x", status);
	return key;
}
EXPORT_SYMBOL(notify_disable);

/* This function restores the Notify module to the state before the
 * last notify_disable() was called. This is equivalent to global
 * interrupt restore, however restricted within interrupts handled by
 * the Notify module. All callbacks registered for all events as
 * specified in the flags are enabled with this API. It is not possible
 * to enable a specific callback. */
void notify_restore(u16 proc_id, u16 line_id, u32 key)
{
	int status = NOTIFY_S_SUCCESS;
	struct notify_driver_object *driver_handle;
	struct notify_object *obj;

	if (WARN_ON(unlikely(atomic_cmpmask_and_lt(&(notify_state.ref_count),
				NOTIFY_MAKE_MAGICSTAMP(0),
				NOTIFY_MAKE_MAGICSTAMP(1)) == true))) {
		status = NOTIFY_E_INVALIDSTATE;
		goto exit;
	}
	if (WARN_ON(unlikely(proc_id >= multiproc_get_num_processors()))) {
		status = NOTIFY_E_INVALIDARG;
		goto exit;
	}
	if (WARN_ON(unlikely(line_id >= NOTIFY_MAX_INTLINES))) {
		status = NOTIFY_E_INVALIDARG;
		goto exit;
	}

	driver_handle = notify_get_driver_handle(proc_id, line_id);
	if (WARN_ON(driver_handle == NULL)) {
		status = NOTIFY_E_DRIVERNOTREGISTERED;
		goto exit;
	}
	if (WARN_ON(driver_handle->is_init != NOTIFY_DRIVERINITSTATUS_DONE)) {
		status = NOTIFY_E_FAIL;
		goto exit;
	}

	obj = (struct notify_object *)driver_handle->notify_handle;
	if (WARN_ON(obj == NULL)) {
		status = NOTIFY_E_FAIL;
		goto exit;
	}

	if (key != obj->nesting) {
		status = NOTIFY_E_INVALIDSTATE;
		goto exit;
	}

	mutex_lock_killable(&obj->lock);
	obj->nesting--;
	if (obj->nesting == 0) {
		/* Enable receiving events */
		if (proc_id != multiproc_self())
			driver_handle->fxn_table.enable(driver_handle);
	}
	mutex_unlock(&obj->lock);
exit:
	if (status < 0)
		pr_err("notify_restore failed! status = 0x%x", status);
	return;
}
EXPORT_SYMBOL(notify_restore);

/* This function disables a specific event. All callbacks registered
 * for the specific event are disabled with this API. It is not
 * possible to disable a specific callback. */
void notify_disable_event(u16 proc_id, u16 line_id, u32 event_id)
{
	int status = 0;
	u32 stripped_event_id = (event_id & NOTIFY_EVENT_MASK);
	struct notify_driver_object *driver_handle;
	struct notify_object *obj;

	if (WARN_ON(unlikely(atomic_cmpmask_and_lt(&(notify_state.ref_count),
				NOTIFY_MAKE_MAGICSTAMP(0),
				NOTIFY_MAKE_MAGICSTAMP(1)) == true))) {
		status = NOTIFY_E_INVALIDSTATE;
		goto exit;
	}
	if (WARN_ON(unlikely(proc_id >= multiproc_get_num_processors()))) {
		status = NOTIFY_E_INVALIDARG;
		goto exit;
	}
	if (WARN_ON(unlikely(line_id >= NOTIFY_MAX_INTLINES))) {
		status = NOTIFY_E_INVALIDARG;
		goto exit;
	}
	if (WARN_ON(unlikely((stripped_event_id >= \
					notify_state.cfg.num_events)))) {
		status = NOTIFY_E_EVTNOTREGISTERED;
		goto exit;
	}
	if (WARN_ON(unlikely(!ISRESERVED(event_id,
				notify_state.cfg.reserved_events)))) {
		status = NOTIFY_E_EVTRESERVED;
		goto exit;
	}

	driver_handle = notify_get_driver_handle(proc_id, line_id);
	if (WARN_ON(driver_handle == NULL)) {
		status = NOTIFY_E_DRIVERNOTREGISTERED;
		goto exit;
	}
	if (WARN_ON(driver_handle->is_init != NOTIFY_DRIVERINITSTATUS_DONE)) {
		status = NOTIFY_E_FAIL;
		goto exit;
	}

	obj = (struct notify_object *)driver_handle->notify_handle;
	if (WARN_ON(obj == NULL)) {
		status = NOTIFY_E_FAIL;
		goto exit;
	}

	if (proc_id != multiproc_self()) {
		driver_handle->fxn_table.disable_event(driver_handle,
							stripped_event_id);
	} else {
		clear_bit(stripped_event_id,
			(unsigned long *) &notify_state.local_enable_mask);
	}
exit:
	if (status < 0)
		pr_err("notify_disable_event failed! status = 0x%x", status);
	return;
}
EXPORT_SYMBOL(notify_disable_event);

/* This function enables a specific event. All callbacks registered for
 * this specific event are enabled with this API. It is not possible to
 * enable a specific callback. */
void notify_enable_event(u16 proc_id, u16 line_id, u32 event_id)
{
	int status = 0;
	u32 stripped_event_id = (event_id & NOTIFY_EVENT_MASK);
	struct notify_driver_object *driver_handle;
	struct notify_object *obj;

	if (WARN_ON(unlikely(atomic_cmpmask_and_lt(&(notify_state.ref_count),
				NOTIFY_MAKE_MAGICSTAMP(0),
				NOTIFY_MAKE_MAGICSTAMP(1)) == true))) {
		status = NOTIFY_E_INVALIDSTATE;
		goto exit;
	}
	if (WARN_ON(unlikely(proc_id >= multiproc_get_num_processors()))) {
		status = NOTIFY_E_INVALIDARG;
		goto exit;
	}
	if (WARN_ON(unlikely(line_id >= NOTIFY_MAX_INTLINES))) {
		status = NOTIFY_E_INVALIDARG;
		goto exit;
	}
	if (WARN_ON(unlikely((stripped_event_id >= \
					notify_state.cfg.num_events)))) {
		status = NOTIFY_E_EVTNOTREGISTERED;
		goto exit;
	}
	if (WARN_ON(unlikely(!ISRESERVED(event_id,
				notify_state.cfg.reserved_events)))) {
		status = NOTIFY_E_EVTRESERVED;
		goto exit;
	}

	driver_handle = notify_get_driver_handle(proc_id, line_id);
	if (WARN_ON(driver_handle == NULL)) {
		status = NOTIFY_E_DRIVERNOTREGISTERED;
		goto exit;
	}
	if (WARN_ON(driver_handle->is_init != NOTIFY_DRIVERINITSTATUS_DONE)) {
		status = NOTIFY_E_FAIL;
		goto exit;
	}

	obj = (struct notify_object *)driver_handle->notify_handle;
	if (WARN_ON(obj == NULL)) {
		status = NOTIFY_E_FAIL;
		goto exit;
	}

	if (proc_id != multiproc_self()) {
		driver_handle->fxn_table.enable_event(driver_handle,
							stripped_event_id);
	} else {
		set_bit(stripped_event_id,
			(unsigned long *)&notify_state.local_enable_mask);
	}
exit:
	if (status < 0)
		pr_err("notify_enable_event failed! status = 0x%x", status);
	return;
}
EXPORT_SYMBOL(notify_enable_event);

/* Whether notification via interrupt line has been registered. */
bool notify_is_registered(u16 proc_id, u16 line_id)
{
	int status = NOTIFY_S_SUCCESS;
	bool is_registered = false;
	struct notify_driver_object *driver_handle;

	if (WARN_ON(unlikely(atomic_cmpmask_and_lt(&(notify_state.ref_count),
				NOTIFY_MAKE_MAGICSTAMP(0),
				NOTIFY_MAKE_MAGICSTAMP(1)) == true))) {
		status = NOTIFY_E_INVALIDSTATE;
		goto exit;
	}
	if (WARN_ON(unlikely(proc_id >= multiproc_get_num_processors()))) {
		status = NOTIFY_E_INVALIDARG;
		goto exit;
	}
	if (WARN_ON(unlikely(line_id >= NOTIFY_MAX_INTLINES))) {
		status = NOTIFY_E_INVALIDARG;
		goto exit;
	}

	driver_handle = notify_get_driver_handle(proc_id, line_id);
	if ((driver_handle != NULL) && (driver_handle->notify_handle != NULL))
		is_registered = true;

exit:
	if (status < 0)
		pr_err("notify_is_registered failed! status = 0x%x", status);
	return is_registered;
}
EXPORT_SYMBOL(notify_is_registered);

/* Creates notify drivers and registers them with Notify */
int notify_attach(u16 proc_id, void *shared_addr)
{
	int status = NOTIFY_S_SUCCESS;

	/* Use the NotifySetup proxy to setup drivers */
	status = notify_setup_proxy_attach(proc_id, shared_addr);

	notify_state.start_complete = true;

	return status;
}
EXPORT_SYMBOL(notify_attach);

/* Creates notify drivers and registers them with Notify */
int notify_detach(u16 proc_id)
{
	int status = 0;

	/* Use the NotifySetup proxy to destroy drivers */
	status = notify_setup_proxy_detach(proc_id);

	notify_state.start_complete = false;

	return status;
}
EXPORT_SYMBOL(notify_detach);

/* Returns the total amount of shared memory used by the Notify module
 * and all instances after notify_start has been called. */
uint notify_shared_mem_req(u16 proc_id, void *shared_addr)
{
	uint mem_req = 0x0;

	if (multiproc_get_num_processors() > 1)
		/* Determine device-specific shared memory requirements */
		mem_req = notify_setup_proxy_shared_mem_req(proc_id,\
								shared_addr);
	else
		/* Only 1 processor: no shared memory needed */
		mem_req = 0;

	return mem_req;
}

/* Indicates whether notify_start is completed. */
inline bool _notify_start_complete(void)
{
	return notify_state.start_complete;
}

/* Function registered as callback with the Notify driver */
void notify_exec(struct notify_object *obj, u32 event_id, u32 payload)
{
	struct notify_event_callback *callback;

	WARN_ON(obj == NULL);
	if (WARN_ON((event_id >= notify_state.cfg.num_events) ||
		(event_id >= NOTIFY_MAXEVENTS))) {
		pr_err("Invalid event_id %d\n", event_id);
	} else {
		callback = &(obj->callbacks[event_id]);
		WARN_ON(callback->fn_notify_cbck == NULL);

		/* Execute the callback function with its argument
		   and the payload */
		callback->fn_notify_cbck(obj->remote_proc_id, obj->line_id,
				event_id, callback->cbck_arg, payload);
	}
}


/* Function registered with notify_exec when multiple registrations are present
 * for the events. */
static void _notify_exec_many(u16 proc_id, u16 line_id, u32 event_id, uint *arg,
			u32 payload)
{
	struct notify_object *obj = (struct notify_object *)arg;
	struct list_head *event_list;
	struct notify_event_listener *listener;

	WARN_ON(proc_id >= multiproc_get_num_processors());
	WARN_ON(obj == NULL);
	WARN_ON(line_id >= NOTIFY_MAX_INTLINES);
	WARN_ON(event_id >= notify_state.cfg.num_events);

	/* Both loopback and the the event itself are enabled */
	event_list = &(obj->event_list[event_id]);

	/* Enter critical section protection. */
	mutex_lock_killable(&obj->lock);
	/* Use "NULL" to get the first EventListener on the list */
	list_for_each_entry(listener, event_list, element) {
		listener->callback.fn_notify_cbck(proc_id, line_id, event_id,
				listener->callback.cbck_arg, payload);
	}
	/* Leave critical section protection. */
	mutex_unlock(&obj->lock);
}
