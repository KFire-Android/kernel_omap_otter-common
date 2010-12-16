/*
 * notify_ducati.c
 *
 * Syslink driver support functions for TI OMAP processors.
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
#include <linux/semaphore.h>
#include <linux/timer.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/list.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <plat/mailbox.h>

#include <syslink/multiproc.h>
#include <syslink/atomic_linux.h>
#include <syslink/sharedregion.h>
#include <syslink/notify_driver.h>
#include <syslink/notifydefs.h>
#include <syslink/notify_driverdefs.h>
#include <syslink/notify_ducatidriver.h>



#define NOTIFYDUCATIDRIVER_MEM_ALIGN	0

#define NOTIFYDUCATIDRIVER_MAX_EVENTS	32

#define NOTIFYNONSHMDRV_MAX_EVENTS	1

#define NOTIFYNONSHMDRV_RESERVED_EVENTS	1

#define NOTIFYDRV_DUCATI_RECV_MBX	2

#define NOTIFYDRV_DUCATI_SEND_MBX	3

/* Get address of event entry. */
#define EVENTENTRY(event_chart, align, event_id)			\
			((struct notify_ducatidrv_event_entry *)	\
			((u32)event_chart + (align * event_id)));

/* Stamp indicating that the Notify Shared Memory driver on the
 * processor has been initialized. */
#define NOTIFYDUCATIDRIVER_INIT_STAMP	0xA9C8B7D6

/* Flag indicating event is set. */
#define NOTIFYDUCATIDRIVER_UP		1

/* Flag indicating event is not set. */
#define NOTIFYDUCATIDRIVER_DOWN		0

/*FIX ME: Make use of Multi Proc module */
#define SELF_ID		0

#define OTHER_ID	1

#define PROC_TESLA	0
#define PROC_DUCATI	1
#define PROC_GPP	2
#define PROCSYSM3	2
#define PROCAPPM3	3
#define MAX_SUBPROC_EVENTS	15

/* Macro to make a correct module magic number with refCount */
#define NOTIFYDUCATIDRIVER_MAKE_MAGICSTAMP(x) \
				((NOTIFY_DUCATIDRIVER_MODULEID << 12u) | (x))

#define ROUND_UP(a, b)	(((a) + ((b) - 1)) & (~((b) - 1)))

static struct omap_mbox *ducati_mbox;
static struct omap_mbox *tesla_mbox;
static int notify_shmdrv_isr(struct notifier_block *, unsigned long, void *);
static bool notify_shmdrv_isr_callback(void *ref_data, void* ntfy_msg);


/* Defines the notify_ducatidrv state object, which contains all
 * the module specific information. */
struct notify_ducatidrv_module {
	atomic_t ref_count;
	/* Reference count */
	struct notify_ducatidrv_config cfg;
	/* NotifyDriverShm configuration structure */
	struct notify_ducatidrv_config def_cfg;
	/* Default module configuration */
	struct notify_ducatidrv_params def_inst_params;
	/* Default instance parameters */
	struct mutex *gate_handle;
	/* Handle to the gate for local thread safety */
	struct notify_ducatidrv_object *driver_handles
				[MULTIPROC_MAXPROCESSORS][NOTIFY_MAX_INTLINES];
	/* Loader handle array. */
	struct mutex dh_lock;
	/* mutext for adding/removing driver handles safety */
	atomic_t mbox2_ref_count;
	/* Reference count for enabling/disabling ducati mailbox interrupt */
	atomic_t mbox1_ref_count;
	/* Reference count for enabling/disabling tesla mailbox interrupt */
};

/* Notify ducati driver instance object. */
struct notify_ducatidrv_object {
	struct notify_ducatidrv_params params;
	/* Instance parameters (configuration values) */
	VOLATILE struct notify_ducatidrv_proc_ctrl *self_proc_ctrl;
	/* Pointer to control structure in shared memory for self processor. */
	VOLATILE struct notify_ducatidrv_proc_ctrl *other_proc_ctrl;
	/* Pointer to control structure in shared memory for remote processor.*/
	VOLATILE struct notify_ducatidrv_event_entry *self_event_chart;
	/* Pointer to event chart for local processor */
	VOLATILE struct notify_ducatidrv_event_entry *other_event_chart;
	/* Pointer to event chart for remote processor */
	u32 reg_chart[NOTIFY_MAXEVENTS];
	/* Local event registration chart for tracking registered events. */
	u16 self_id;
	/* Self ID used for identification of local control region */
	u16 other_id;
	/* Other ID used for identification of remote control region */
	u16 remote_proc_id;
	/* Processor ID of the remote processor which which this driver instance
	 communicates. */
	struct notify_driver_object *drv_handle;
	/* Common NotifyDriver handle */
	u32 nesting;
	/* For disable/restore nesting */
	u32 cache_enabled;
	/* Whether to perform cache calls */
	u32 event_entry_size;
	/* Spacing between event entries   */
	u32 num_events;
	/* Number of events configured */
};


static struct notify_ducatidrv_module notify_ducatidriver_state = {
	.gate_handle = NULL,
	.def_inst_params.shared_addr = NULL,
	.def_inst_params.cache_enabled = false,
	.def_inst_params.cache_line_size = 128u,
	.def_inst_params.remote_proc_id = MULTIPROC_INVALIDID,
	.def_inst_params.line_id = 0,
	.def_inst_params.local_int_id = (u32) -1,
	.def_inst_params.remote_int_id = (u32) -1
};

static struct notifier_block ducati_notify_nb = {
	.notifier_call = notify_shmdrv_isr,
};

/* Get the default configuration for the notify_ducatidrv module. */
void notify_ducatidrv_get_config(struct notify_ducatidrv_config *cfg)
{
	int status = NOTIFY_S_SUCCESS;

	if (WARN_ON(unlikely(cfg == NULL))) {
		status = NOTIFY_E_INVALIDARG;
		goto exit;
	}

	if (atomic_cmpmask_and_lt(&(notify_ducatidriver_state.ref_count),
					NOTIFYDUCATIDRIVER_MAKE_MAGICSTAMP(0),
					NOTIFYDUCATIDRIVER_MAKE_MAGICSTAMP(1))
					== true)
		memcpy(cfg, &(notify_ducatidriver_state.def_cfg),
			sizeof(struct notify_ducatidrv_config));
	else
		memcpy(cfg, &(notify_ducatidriver_state.cfg),
			sizeof(struct notify_ducatidrv_config));

exit:
	if (status < 0) {
		pr_err("notify_ducatidrv_get_config failed! "
			"status = 0x%x", status);
	}
	return;
}
EXPORT_SYMBOL(notify_ducatidrv_get_config);

/* Setup the notify_ducatidrv module. */
int notify_ducatidrv_setup(struct notify_ducatidrv_config *cfg)
{
	int status = 0;
	struct notify_ducatidrv_config tmp_cfg;
	u16 i;
	u16 j;

	/* Init the ref_count to 0 */
	atomic_cmpmask_and_set(&(notify_ducatidriver_state.ref_count),
					NOTIFYDUCATIDRIVER_MAKE_MAGICSTAMP(0),
					NOTIFYDUCATIDRIVER_MAKE_MAGICSTAMP(0));
	if (atomic_inc_return(&(notify_ducatidriver_state.ref_count)) !=
		NOTIFYDUCATIDRIVER_MAKE_MAGICSTAMP(1u)) {
		return NOTIFY_S_ALREADYSETUP;
	}
	atomic_set(&(notify_ducatidriver_state.mbox2_ref_count), 0);
	atomic_set(&(notify_ducatidriver_state.mbox1_ref_count), 0);

	if (cfg == NULL) {
		notify_ducatidrv_get_config(&tmp_cfg);
		cfg = &tmp_cfg;
	}

	/* Create a default gate handle here */
	notify_ducatidriver_state.gate_handle =
			kmalloc(sizeof(struct mutex), GFP_KERNEL);
	if (notify_ducatidriver_state.gate_handle == NULL) {
		status = NOTIFY_E_MEMORY;
		goto error_exit;
	}
	mutex_init(notify_ducatidriver_state.gate_handle);
	mutex_init(&notify_ducatidriver_state.dh_lock);

	for (i = 0 ; i < MULTIPROC_MAXPROCESSORS; i++)
		for (j = 0 ; j < NOTIFY_MAX_INTLINES; j++)
			notify_ducatidriver_state.driver_handles[i][j] = NULL;

	memcpy(&notify_ducatidriver_state.cfg, cfg,
			sizeof(struct notify_ducatidrv_config));

	/* Initialize the maibox module for Ducati */
	if (ducati_mbox == NULL) {
		ducati_mbox = omap_mbox_get("mailbox-2", &ducati_notify_nb);
		if (ducati_mbox == NULL) {
			pr_err("Failed in omap_mbox_get(ducati)\n");
			status = NOTIFY_E_INVALIDSTATE;
			goto error_mailbox_get_failed;
		}
	}

	/* Initialize the maibox module for Tesla */
	if (!tesla_mbox) {
		tesla_mbox = omap_mbox_get("mailbox-1", &ducati_notify_nb);
		if (!tesla_mbox) {
			pr_err("Failed in omap_mbox_get(tesla)\n");
			status = NOTIFY_E_INVALIDSTATE;
			goto error_mailbox_get_failed;
		}
	}
	return 0;

error_mailbox_get_failed:
	kfree(notify_ducatidriver_state.gate_handle);
error_exit:
	atomic_set(&(notify_ducatidriver_state.ref_count),
			NOTIFYDUCATIDRIVER_MAKE_MAGICSTAMP(0));
	pr_err("notify_ducatidrv_setup failed! status = 0x%x", status);
	return status;
}
EXPORT_SYMBOL(notify_ducatidrv_setup);

/* Destroy the notify_ducatidrv module. */
int notify_ducatidrv_destroy(void)
{
	int status = NOTIFY_S_SUCCESS;
	u16 i;
	u16 j;

	if (WARN_ON(unlikely(atomic_cmpmask_and_lt(
			&(notify_ducatidriver_state.ref_count),
			NOTIFYDUCATIDRIVER_MAKE_MAGICSTAMP(0),
			NOTIFYDUCATIDRIVER_MAKE_MAGICSTAMP(1)) == true))) {
		status = NOTIFY_E_INVALIDSTATE;
		goto exit;
	}
	if (!(atomic_dec_return(&notify_ducatidriver_state.ref_count) == \
				NOTIFYDUCATIDRIVER_MAKE_MAGICSTAMP(0)))
		return NOTIFY_S_ALREADYSETUP;

	/* Temporarily increment the refcount */
	atomic_set(&(notify_ducatidriver_state.ref_count),
					NOTIFYDUCATIDRIVER_MAKE_MAGICSTAMP(1));

	for (i = 0 ; i < MULTIPROC_MAXPROCESSORS; i++) {
		for (j = 0 ; j < NOTIFY_MAX_INTLINES; j++) {
			if (notify_ducatidriver_state.driver_handles[i][j] != \
				NULL) {
				notify_ducatidrv_delete(
					&notify_ducatidriver_state.\
						driver_handles[i][j]);
			}
		}
	}

	if (notify_ducatidriver_state.gate_handle != NULL)
		kfree(notify_ducatidriver_state.gate_handle);

	atomic_set(&(notify_ducatidriver_state.ref_count),
		NOTIFYDUCATIDRIVER_MAKE_MAGICSTAMP(0));

	/* Finalize the maibox module for Ducati */
	omap_mbox_put(ducati_mbox, &ducati_notify_nb);
	ducati_mbox = NULL;

	/* Finalize the maibox module for Tesla */
	omap_mbox_put(tesla_mbox, &ducati_notify_nb);
	tesla_mbox = NULL;

exit:
	if (status < 0) {
		pr_err("notify_ducatidrv_destroy failed! "
			"status = 0x%x", status);
	}
	return status;
}
EXPORT_SYMBOL(notify_ducatidrv_destroy);

/* Function to initialize the parameters for this notify_ducatidrv instance. */
void notify_ducatidrv_params_init(struct notify_ducatidrv_params *params)
{
	int status = NOTIFY_S_SUCCESS;

	if (WARN_ON(unlikely(atomic_cmpmask_and_lt(
			&(notify_ducatidriver_state.ref_count),
			NOTIFYDUCATIDRIVER_MAKE_MAGICSTAMP(0),
			NOTIFYDUCATIDRIVER_MAKE_MAGICSTAMP(1)) == true))) {
		status = NOTIFY_E_INVALIDSTATE;
		goto exit;
	}
	if (WARN_ON(unlikely(params == NULL))) {
		status = NOTIFY_E_INVALIDARG;
		goto exit;
	}

	/*Return updated notify_ducatidrv instance specific parameters*/
	memcpy(params, &(notify_ducatidriver_state.def_inst_params),
		sizeof(struct notify_ducatidrv_params));

exit:
	if (status < 0) {
		pr_err("notify_ducatidrv_params_init failed! "
			"status = 0x%x", status);
	}
	return;
}
EXPORT_SYMBOL(notify_ducatidrv_params_init);

/* Function to create an instance of this Notify ducati driver. */
struct notify_ducatidrv_object *notify_ducatidrv_create(
				const struct notify_ducatidrv_params *params)
{
	int status = NOTIFY_S_SUCCESS;
	struct notify_ducatidrv_object *obj = NULL;
	struct notify_driver_object *drv_handle = NULL;
	struct notify_driver_fxn_table fxn_table;
	struct omap_mbox *mbox;
	atomic_t *mbx_cnt;
	u32 i;
	u16 region_id;
	uint region_cache_size;
	uint min_align;
	struct notify_ducatidrv_event_entry *event_entry;
	u32 proc_ctrl_size;

	if (WARN_ON(unlikely(atomic_cmpmask_and_lt(
			&(notify_ducatidriver_state.ref_count),
			NOTIFYDUCATIDRIVER_MAKE_MAGICSTAMP(0),
			NOTIFYDUCATIDRIVER_MAKE_MAGICSTAMP(1)) == true))) {
		status = NOTIFY_E_INVALIDSTATE;
		goto exit;
	}
	if (WARN_ON(unlikely(params == NULL))) {
		status = NOTIFY_E_INVALIDARG;
		goto exit;
	}
	if (WARN_ON(unlikely((params->remote_proc_id == MULTIPROC_INVALIDID)
			|| (params->remote_proc_id == multiproc_self())))) {
		status = NOTIFY_E_INVALIDARG;
		goto exit;
	}
	if (WARN_ON(unlikely(params->line_id >= NOTIFY_MAX_INTLINES))) {
		status = NOTIFY_E_INVALIDARG;
		goto exit;
	}
	if (WARN_ON(unlikely(((u32)params->shared_addr % \
		(u32) params->cache_line_size)) != 0)) {
		status = NOTIFY_E_INVALIDARG;
		goto exit;
	}

	if (params->remote_proc_id) {
		mbox = ducati_mbox;
		mbx_cnt = &notify_ducatidriver_state.mbox2_ref_count;
	} else {
		mbox = tesla_mbox;
		mbx_cnt = &notify_ducatidriver_state.mbox1_ref_count;
	}

	status = mutex_lock_interruptible(
				notify_ducatidriver_state.gate_handle);
	if (status)
		goto exit;

	/* Check if driver already exists. */
	drv_handle = notify_get_driver_handle(params->remote_proc_id,
						params->line_id);
	if (drv_handle != NULL) {
		status = NOTIFY_E_ALREADYEXISTS;
		goto error_unlock_and_return;
	}

	/* Function table information */
	fxn_table.register_event = (void *)&notify_ducatidrv_register_event;
	fxn_table.unregister_event = (void *)&notify_ducatidrv_unregister_event;
	fxn_table.send_event = (void *)&notify_ducatidrv_send_event;
	fxn_table.disable = (void *)&notify_ducatidrv_disable;
	fxn_table.enable = (void *)&notify_ducatidrv_enable;
	fxn_table.disable_event = (void *)&notify_ducatidrv_disable_event;
	fxn_table.enable_event = (void *)&notify_ducatidrv_enable_event;

	/* Register driver with the Notify module. */
	status = notify_register_driver(params->remote_proc_id,
					params->line_id, &fxn_table,
					&drv_handle);
	if (status < 0) {
		status = NOTIFY_E_FAIL;
		goto error_clean_and_exit;
	}

	/* Allocate memory for the notify_ducatidrv_object object. */
	obj = kzalloc(sizeof(struct notify_ducatidrv_object), GFP_ATOMIC);
	if (obj == NULL) {
		status = NOTIFY_E_MEMORY;
		goto error_clean_and_exit;
	}
	memcpy(&(obj->params), (void *) params,
				sizeof(struct notify_ducatidrv_params));
	obj->num_events = notify_state.cfg.num_events;
	/* Point to the generic drvHandle object from this specific
	 * NotifyDriverShm object. */
	obj->drv_handle = drv_handle;

	/* Determine obj->cacheEnabled using params->cacheEnabled and
	 * SharedRegion cache flag setting, if applicable. */
	obj->cache_enabled = params->cache_enabled;
	min_align = params->cache_line_size;
	region_id = sharedregion_get_id((void *)params->shared_addr);
	if (region_id != SHAREDREGION_INVALIDREGIONID) {
		/* Override the user cacheEnabled setting if the region
		 * cacheEnabled is FALSE. */
		if (!sharedregion_is_cache_enabled(region_id))
			obj->cache_enabled = false;

		region_cache_size = sharedregion_get_cache_line_size(region_id);

		/* Override the user cache line size setting if the region
		 * cache line size is smaller. */
		if (region_cache_size < min_align)
			min_align = region_cache_size;
	}

	if ((u32)params->shared_addr % min_align != 0) {
		status = NOTIFY_E_FAIL;
		goto error_clean_and_exit;
	}
	obj->remote_proc_id = params->remote_proc_id;
	obj->nesting = 0;
	if (params->remote_proc_id > multiproc_self()) {
		obj->self_id = SELF_ID;
		obj->other_id = OTHER_ID;
	} else {
		obj->self_id  = OTHER_ID;
		obj->other_id = SELF_ID;
	}

	proc_ctrl_size = ROUND_UP(sizeof(struct notify_ducatidrv_proc_ctrl),
				min_align);

	/* Save the eventEntrySize in obj since we will need it at runtime to
	 * index the event charts */

	obj->event_entry_size = ROUND_UP(
				sizeof(struct notify_ducatidrv_event_entry),
				min_align);
	obj->self_proc_ctrl = (struct notify_ducatidrv_proc_ctrl *)
					((u32) params->shared_addr + \
					(obj->self_id * proc_ctrl_size));
	obj->other_proc_ctrl = (struct notify_ducatidrv_proc_ctrl *)
					((u32) params->shared_addr + \
					(obj->other_id * proc_ctrl_size));
	obj->self_event_chart = (struct notify_ducatidrv_event_entry *)
					((u32) params->shared_addr + \
					(2 * proc_ctrl_size) + \
					(obj->event_entry_size * \
					obj->num_events * obj->self_id));
	obj->other_event_chart  = (struct notify_ducatidrv_event_entry *)
					((u32) params->shared_addr + \
					(2 * proc_ctrl_size) + \
					(obj->event_entry_size * \
					obj->num_events * obj->other_id));

	for (i = 0; i < obj->num_events; i++)
		obj->reg_chart[i] = (u32)-1;

	/* All events initially unflagged */
	for (i = 0; i < obj->num_events; i++) {
		event_entry = EVENTENTRY(obj->self_event_chart,
					obj->event_entry_size, i);
		event_entry->flag = 0;
	}

	/* All events initially not registered */
	obj->self_proc_ctrl->event_reg_mask = 0x0;

	/* Enable all events initially.*/
	obj->self_proc_ctrl->event_enable_mask = 0xFFFFFFFF;


	/*Set up the ISR on the MPU-Ducati FIFO */
	if (atomic_inc_return(mbx_cnt) == 1)
		omap_mbox_enable_irq(mbox, IRQ_RX);
	obj->self_proc_ctrl->recv_init_status = NOTIFYDUCATIDRIVER_INIT_STAMP;
	obj->self_proc_ctrl->send_init_status = NOTIFYDUCATIDRIVER_INIT_STAMP;

#if 0
	/* Write back our own ProcCtrl */
	if (obj->cache_enabled) {
		Cache_wbInv((void *) obj->self_proc_ctrl,
			sizeof(struct notify_ducatidrv_proc_ctrl),
			Cache_Type_ALL, true);
	}
#endif

	drv_handle->is_init = NOTIFY_DRIVERINITSTATUS_DONE;

	mutex_lock_killable(&notify_ducatidriver_state.dh_lock);
	/* Set the handle in the driverHandles array. */
	notify_ducatidriver_state.driver_handles
		[params->remote_proc_id][params->line_id] = obj;
	mutex_unlock(&notify_ducatidriver_state.dh_lock);
	mutex_unlock(notify_ducatidriver_state.gate_handle);
	return obj;

error_clean_and_exit:
	if (obj != NULL) {
		if (obj->self_proc_ctrl != NULL) {
			/* Clear initialization status in shared memory. */
			obj->self_proc_ctrl->recv_init_status = 0x0;
			obj->self_proc_ctrl->recv_init_status = 0x0;
			obj->self_proc_ctrl = NULL;
#if 0
			/* Write back our own ProcCtrl */
			if (obj->cache_enabled) {
				Cache_wbInv((void *) obj->self_proc_ctrl,
				sizeof(struct notify_ducatidrv_proc_ctrl),
				Cache_Type_ALL, true);
			}
#endif
			kfree(obj);
			obj = NULL;
		}
	}
	if (drv_handle != NULL) {
		/* Unregister driver from the Notify module*/
		notify_unregister_driver(drv_handle);
		drv_handle = NULL;
	}
error_unlock_and_return:
	/* Leave critical section protection. */
	mutex_unlock(notify_ducatidriver_state.gate_handle);
exit:
	pr_err("notify_ducatidrv_create failed! status = 0x%x", status);
	return NULL;
}
EXPORT_SYMBOL(notify_ducatidrv_create);

/* Function to delete the instance of shared memory driver */
int notify_ducatidrv_delete(struct notify_ducatidrv_object **handle_ptr)
{
	int status = NOTIFY_S_SUCCESS;
	int tmp_status = NOTIFY_S_SUCCESS;
	struct notify_ducatidrv_object *obj = NULL;
	struct omap_mbox *mbox;
	atomic_t *mbx_cnt;

	if (WARN_ON(unlikely(atomic_cmpmask_and_lt(
			&(notify_ducatidriver_state.ref_count),
			NOTIFYDUCATIDRIVER_MAKE_MAGICSTAMP(0),
			NOTIFYDUCATIDRIVER_MAKE_MAGICSTAMP(1)) == true))) {
		status = NOTIFY_E_INVALIDSTATE;
		goto exit;
	}

	if (WARN_ON(unlikely(handle_ptr == NULL))) {
		status = NOTIFY_E_INVALIDARG;
		goto exit;
	}
	if (WARN_ON(unlikely(*handle_ptr == NULL))) {
		status = NOTIFY_E_INVALIDARG;
		goto exit;
	}

	obj = (struct notify_ducatidrv_object *)(*handle_ptr);
	if (obj != NULL) {
		mutex_lock_killable(&notify_ducatidriver_state.dh_lock);
		if (obj->remote_proc_id) {
			mbox = ducati_mbox;
			mbx_cnt = &notify_ducatidriver_state.mbox2_ref_count;
		} else {
			mbox = tesla_mbox;
			mbx_cnt = &notify_ducatidriver_state.mbox1_ref_count;
		}
		/* Uninstall the ISRs & Disable the Mailbox interrupt.*/
		if (atomic_dec_and_test(mbx_cnt))
			omap_mbox_disable_irq(mbox, IRQ_RX);

		if (obj->self_proc_ctrl != NULL) {
			/* Clear initialization status in shared memory. */
			obj->self_proc_ctrl->recv_init_status = 0x0;
			obj->self_proc_ctrl->recv_init_status = 0x0;
			obj->self_proc_ctrl = NULL;
#if 0
			/* Write back our own ProcCtrl */
			if (obj->cache_enabled) {
				Cache_wbInv((void *) obj->self_proc_ctrl,
				sizeof(struct notify_ducatidrv_proc_ctrl),
				Cache_Type_ALL, true);
			}
#endif
		}

		tmp_status = notify_unregister_driver(obj->drv_handle);
		if (status >= 0 && tmp_status < 0)
			status = tmp_status;

		notify_ducatidriver_state.driver_handles
			[obj->params.remote_proc_id][obj->params.line_id] = \
									NULL;
		kfree(obj);
		obj = NULL;
		mutex_unlock(&notify_ducatidriver_state.dh_lock);
	}

exit:
	if (status < 0)
		pr_err("notify_ducatidrv_delete failed! status = 0x%x", status);
	return status;
}
EXPORT_SYMBOL(notify_ducatidrv_delete);

/* Register a callback for an event with the Notify driver. */
int notify_ducatidrv_register_event(struct notify_driver_object *handle,
					u32 event_id)
{
	int status = NOTIFY_S_SUCCESS;
	struct notify_ducatidrv_object *obj;
	VOLATILE struct notify_ducatidrv_event_entry *event_entry;
	int i;
	int j;

	if (WARN_ON(unlikely(handle == NULL))) {
		status = NOTIFY_E_INVALIDARG;
		goto exit;
	}
	if (WARN_ON(unlikely(handle->is_init != \
					NOTIFY_DRIVERINITSTATUS_DONE))) {
		status = NOTIFY_E_INVALIDARG;
		goto exit;
	}
	if (WARN_ON(unlikely(handle->notify_handle == NULL))) {
		status = NOTIFY_E_INVALIDARG;
		goto exit;
	}
	if (WARN_ON(unlikely(handle->notify_handle->driver_handle == NULL))) {
		status = NOTIFY_E_DRIVERNOTREGISTERED;
		goto exit;
	}

	obj = (struct notify_ducatidrv_object *)
				handle->notify_handle->driver_handle;
	if (WARN_ON(unlikely(obj->reg_chart == NULL))) {
		status = NOTIFY_E_FAIL;
		goto exit;
	}

	/* This function is only called for the first register, i.e. when the
	 * first callback is being registered. */
	/* Add an entry for the registered event into the Event Registration
	 * Chart, in ascending order of event numbers (and decreasing
	 * priorities). There is no need to make this atomic since
	 * Notify_exec cannot preempt: shared memory hasn't been modified yet.
	 */
	for (i = 0 ; i < obj->num_events; i++) {
		/* Find the correct slot in the registration array. */
		if (obj->reg_chart[i] == (u32) -1) {
			for (j = (i - 1); j >= 0; j--) {
				if (event_id < obj->reg_chart[j]) {
					obj->reg_chart[j + 1] = \
						obj->reg_chart[j];
					i = j;
				} else {
					/* End the loop, slot found. */
					j = -1;
				}
			}
			obj->reg_chart[i] = event_id;
			break;
		}
	}

	/* Clear any pending unserviced event as there are no listeners
	 * for the pending event */
	event_entry = EVENTENTRY(obj->self_event_chart, obj->event_entry_size,
					event_id);
	event_entry->flag = NOTIFYDUCATIDRIVER_DOWN;

	/* Set the registered bit in shared memory and write back */
	set_bit(event_id, (unsigned long *)
			&(obj->self_proc_ctrl->event_reg_mask));

#if 0
	/* Write back both the flag and the reg mask */
	if (obj->cache_enabled) {
		/* Writeback eventRegMask */
		Cache_wbInv((void *) obj->self_proc_ctrl,
			sizeof(struct notify_ducatidrv_proc_ctrl),
			Cache_Type_ALL, true);
		/* Writeback event entry */
		Cache_wbInv((void *) event_entry,
			sizeof(struct notify_ducatidrv_event_entry),
			Cache_Type_ALL, true);
	}
#endif

exit:
	if (status < 0) {
		pr_err("notify_ducatidrv_register_event failed! "
			"status = 0x%x", status);
	}
	return status;
}

/* Unregister a callback for an event with the Notify driver. */
int notify_ducatidrv_unregister_event(struct notify_driver_object *handle,
					u32 event_id)
{
	int status = NOTIFY_S_SUCCESS;
	struct notify_ducatidrv_object *obj;
	VOLATILE struct notify_ducatidrv_event_entry *event_entry;
	int i;
	int j;

	if (WARN_ON(unlikely(handle == NULL))) {
		status = NOTIFY_E_INVALIDARG;
		goto exit;
	}
	if (WARN_ON(unlikely(handle->is_init != \
					NOTIFY_DRIVERINITSTATUS_DONE))) {
		status = NOTIFY_E_INVALIDARG;
		goto exit;
	}
	if (WARN_ON(unlikely(handle->notify_handle == NULL))) {
		status = NOTIFY_E_INVALIDARG;
		goto exit;
	}
	if (WARN_ON(unlikely(handle->notify_handle->driver_handle == NULL))) {
		status = NOTIFY_E_DRIVERNOTREGISTERED;
		goto exit;
	}

	obj = (struct notify_ducatidrv_object *)
				handle->notify_handle->driver_handle;
	if (WARN_ON(unlikely(obj->reg_chart == NULL))) {
		status = NOTIFY_E_FAIL;
		goto exit;
	}

	/* This function is only called for the last unregister, i.e. when the
	 * final remaining callback is being unregistered.
	 * Unset the registered bit in shared memory */
	clear_bit(event_id, (unsigned long *)
			&(obj->self_proc_ctrl->event_reg_mask));

	/* Clear any pending unserviced event as there are no listeners
	 * for the pending event.  This should be done only after the event
	 * is unregistered from shared memory so the other processor doesn't
	 * successfully send an event our way immediately after unflagging this
	 * event. */
	event_entry = EVENTENTRY(obj->self_event_chart, obj->event_entry_size,
					event_id);
	event_entry->flag = NOTIFYDUCATIDRIVER_DOWN;

#if 0
	/* Write back both the flag and the reg mask */
	if (obj->cache_enabled) {
		/* Writeback event entry */
		Cache_wbInv((void *) event_entry,
			sizeof(struct notify_ducatidrv_event_entry),
			Cache_Type_ALL, true);
		/* Writeback eventRegMask */
		Cache_wbInv((void *) obj->self_proc_ctrl,
			sizeof(struct notify_ducatidrv_proc_ctrl),
			Cache_Type_ALL, true);
	}
#endif

	/* Re-arrange eventIds in the Event Registration Chart so there is
	 * no gap caused by the removal of this eventId
	 *
	 * There is no need to make this atomic since Notify_exec cannot
	 * preempt: the event has already been disabled in shared memory
	 * (see above) */
	for (i = 0 ; i < obj->num_events; i++) {
		/* Find the correct slot in the registration array. */
		if (event_id == obj->reg_chart[i]) {
			obj->reg_chart[i] = (u32) -1;
			for (j = (i + 1); (j != obj->num_events) && \
				(obj->reg_chart[j] != (u32)-1); j++) {
				obj->reg_chart[j - 1] = obj->reg_chart[j];
				obj->reg_chart[j] = (u32)-1;
			}
			break;
		}
	}

exit:
	if (status < 0) {
		pr_err("notify_ducatidrv_unregister_event failed! "
			"status = 0x%x", status);
	}
	return status;
}

/* Send a notification event to the registered users for this
 * notification on the specified processor. */
int notify_ducatidrv_send_event(struct notify_driver_object *handle,
				u32 event_id, u32 payload, bool wait_clear)
{
	int status = NOTIFY_S_SUCCESS;
	struct notify_ducatidrv_object *obj;
	struct omap_mbox *mbox;
	VOLATILE struct notify_ducatidrv_event_entry *event_entry;
	int max_poll_count;
	int i = 0;
	mbox_msg_t msg;

	if (WARN_ON(unlikely(handle == NULL))) {
		status = NOTIFY_E_INVALIDARG;
		goto exit;
	}
	if (WARN_ON(unlikely(handle->is_init != \
					NOTIFY_DRIVERINITSTATUS_DONE))) {
		status = NOTIFY_E_INVALIDARG;
		goto exit;
	}
	if (WARN_ON(unlikely(handle->notify_handle == NULL))) {
		status = NOTIFY_E_INVALIDARG;
		goto exit;
	}
	if (WARN_ON(unlikely(handle->notify_handle->driver_handle == NULL))) {
		status = NOTIFY_E_DRIVERNOTREGISTERED;
		goto exit;
	}

	obj = (struct notify_ducatidrv_object *)
				handle->notify_handle->driver_handle;

	mbox = (obj->remote_proc_id) ? ducati_mbox : tesla_mbox;
	if (WARN_ON(unlikely(obj->reg_chart == NULL))) {
		status = NOTIFY_E_FAIL;
		goto exit;
	}

	dsb();
	event_entry = EVENTENTRY(obj->other_event_chart, obj->event_entry_size,
					event_id);
#if 0
	/* Invalidate cache for the other processor's procCtrl. */
	if (obj->cache_enabled) {
		Cache_wbInv((void *) obj->other_proc_ctrl,
			sizeof(struct notify_ducatidrv_proc_ctrl),
			Cache_Type_ALL, true);
	}
#endif
	max_poll_count = notify_state.cfg.send_event_poll_count;

	/* Check whether driver on other processor is initialized */
	if (obj->other_proc_ctrl->recv_init_status != \
					NOTIFYDUCATIDRIVER_INIT_STAMP) {
		/* This may be used for polling till other-side driver is ready,
		 * so do not set failure reason. */
		status = NOTIFY_E_NOTINITIALIZED;
		goto exit;
	}
	/* Check if other side has registered to receive this event. */
	if (!test_bit(event_id, (unsigned long *)
				&obj->other_proc_ctrl->event_reg_mask)) {
		status = NOTIFY_E_EVTNOTREGISTERED;
		/* This may be used for polling till other-side is ready, so
		* do not set failure reason. */
		goto exit;
	}
	if (!test_bit(event_id, (unsigned long *)
				&obj->other_proc_ctrl->event_enable_mask)) {
		status = NOTIFY_E_EVTDISABLED;
		/* This may be used for polling till other-side is ready, so
		* do not set failure reason. */
		goto exit;
	}
#if 0
	if (obj->cache_enabled) {
		Cache_inv((void *)event_entry,
			sizeof(struct notify_ducatidrv_event_entry),
			Cache_Type_ALL, TRUE);
	}
#endif
	dsb();
	status = mutex_lock_interruptible(
				notify_ducatidriver_state.gate_handle);
	if (status)
		goto exit;

	if (wait_clear == true) {
		/*Wait for completion of prev
		event from other side*/
		while ((event_entry->flag != NOTIFYDUCATIDRIVER_DOWN) && \
			(status >= 0)) {
			/* Leave critical section protection. Create a window
			 * of opportunity for other interrupts to be handled.*/
			mutex_unlock(notify_ducatidriver_state.gate_handle);
			i++;
			if ((max_poll_count != (u32)-1) && \
				(i == max_poll_count)) {
				status = NOTIFY_E_TIMEOUT;
				break;
			}

#if 0
			if (obj->cache_enabled) {
				Cache_inv((void *)event_entry,
				sizeof(struct notify_ducatidrv_event_entry),
				Cache_Type_ALL, TRUE);
			}
#endif
			dsb();

			/* Enter critical section protection. */
			status = mutex_lock_interruptible(
					notify_ducatidriver_state.gate_handle);
		}
	}

	if (status >= 0) {
		/* Set the event bit field and payload. */
		event_entry->payload = payload;
		event_entry->flag = NOTIFYDUCATIDRIVER_UP;

#if 0
		if (obj->cache_enabled) {
			Cache_inv((void *)event_entry,
			sizeof(struct notify_ducatidrv_event_entry),
			Cache_Type_ALL, TRUE);
		}
#endif
		dsb();

		/* Send an interrupt with the event information to the
		 * remote processor */
		msg = ((obj->remote_proc_id << 16) | event_id);
		status = omap_mbox_msg_send(mbox, msg);

		/* Leave critical section protection. */
		mutex_unlock(notify_ducatidriver_state.gate_handle);
	}

exit:
	if (status < 0) {
		pr_err("notify_ducatidrv_sendevent failed! "
			"status = 0x%x", status);
	}
	return status;
}

/* Disable all events for this Notify driver.*/
int notify_ducatidrv_disable(struct notify_driver_object *handle)
{
	int status = NOTIFY_S_SUCCESS;
	struct notify_ducatidrv_object *obj;
	struct omap_mbox *mbox;

	/* All the below parameter checking is unnecessary, but added to
	 * make sure the driver object is initialized properly */
	if (WARN_ON(unlikely(handle == NULL))) {
		status = NOTIFY_E_INVALIDARG;
		goto exit;
	}
	if (WARN_ON(unlikely(handle->is_init != \
					NOTIFY_DRIVERINITSTATUS_DONE))) {
		status = NOTIFY_E_INVALIDARG;
		goto exit;
	}
	if (WARN_ON(unlikely(handle->notify_handle == NULL))) {
		status = NOTIFY_E_INVALIDARG;
		goto exit;
	}
	if (WARN_ON(unlikely(handle->notify_handle->driver_handle == NULL))) {
		status = NOTIFY_E_DRIVERNOTREGISTERED;
		goto exit;
	}

	obj = (struct notify_ducatidrv_object *)
				handle->notify_handle->driver_handle;

	mbox = (obj->remote_proc_id) ? ducati_mbox : tesla_mbox;
	if (WARN_ON(unlikely(obj->reg_chart == NULL))) {
		status = NOTIFY_E_FAIL;
		goto exit;
	}

	/* Disable the mailbox interrupt associated with ducati mailbox */
	omap_mbox_disable_irq(mbox, IRQ_RX);

exit:
	if (status < 0) {
		pr_err("notify_ducatidrv_disable failed! "
			"status = 0x%x", status);
	}
	/*No flags to be returned. */
	return 0;
}

/* Restore the notify_ducatidrv to the state before the last disable was
 * called. */
void notify_ducatidrv_enable(struct notify_driver_object *handle)
{
	int status = NOTIFY_S_SUCCESS;
	struct notify_ducatidrv_object *obj;
	struct omap_mbox *mbox;

	/* All the below parameter checking is unnecessary, but added to
	 * make sure the driver object is initialized properly */
	if (WARN_ON(unlikely(handle == NULL))) {
		status = NOTIFY_E_INVALIDARG;
		goto exit;
	}
	if (WARN_ON(unlikely(handle->is_init != \
					NOTIFY_DRIVERINITSTATUS_DONE))) {
		status = NOTIFY_E_INVALIDARG;
		goto exit;
	}
	if (WARN_ON(unlikely(handle->notify_handle == NULL))) {
		status = NOTIFY_E_INVALIDARG;
		goto exit;
	}
	if (WARN_ON(unlikely(handle->notify_handle->driver_handle == NULL))) {
		status = NOTIFY_E_DRIVERNOTREGISTERED;
		goto exit;
	}

	obj = (struct notify_ducatidrv_object *)
				handle->notify_handle->driver_handle;

	mbox = (obj->remote_proc_id) ? ducati_mbox : tesla_mbox;
	if (WARN_ON(unlikely(obj->reg_chart == NULL))) {
		status = NOTIFY_E_FAIL;
		goto exit;
	}

	/*Enable the receive interrupt for ducati */
	omap_mbox_enable_irq(mbox, IRQ_RX);

exit:
	if (status < 0) {
		pr_err("notify_ducatidrv_enable failed! "
			"status = 0x%x", status);
	}
	return;
}

/* Disable a specific event for this Notify ducati driver */
void notify_ducatidrv_disable_event(struct notify_driver_object *handle,
					u32 event_id)
{
	int status = NOTIFY_S_SUCCESS;
	struct notify_ducatidrv_object *obj;
	VOLATILE struct notify_ducatidrv_event_entry *event_entry;

	if (WARN_ON(unlikely(handle == NULL))) {
		status = NOTIFY_E_INVALIDARG;
		goto exit;
	}
	if (WARN_ON(unlikely(handle->is_init != \
					NOTIFY_DRIVERINITSTATUS_DONE))) {
		status = NOTIFY_E_INVALIDARG;
		goto exit;
	}
	if (WARN_ON(unlikely(handle->notify_handle == NULL))) {
		status = NOTIFY_E_INVALIDARG;
		goto exit;
	}
	if (WARN_ON(unlikely(handle->notify_handle->driver_handle == NULL))) {
		status = NOTIFY_E_DRIVERNOTREGISTERED;
		goto exit;
	}

	obj = (struct notify_ducatidrv_object *)
		handle->notify_handle->driver_handle;
	if (event_id > obj->num_events) {
		status = NOTIFY_E_FAIL;
		goto exit;
	}

	/* Enter critical section protection. */
	status = mutex_lock_interruptible(
					notify_ducatidriver_state.gate_handle);
	if (status)
		goto exit;
	clear_bit(event_id, (unsigned long *)
			&(obj->self_proc_ctrl->event_enable_mask));
	/* Leave critical section protection. */
	mutex_unlock(notify_ducatidriver_state.gate_handle);
#if 0
	if (obj->cache_enabled) {
		/* Writeback event_enable_mask */
		Cache_wbInv((void *) obj->self_proc_ctrl,
			sizeof(struct notify_ducatidrv_proc_ctrl),
			Cache_Type_ALL, true);
	}
#endif

	event_entry = EVENTENTRY(obj->self_event_chart, obj->event_entry_size,
					event_id)
#if 0
	if (obj->cache_enabled) {
		/* Writeback event entry */
		Cache_wbInv((void *) event_entry,
			sizeof(struct notify_ducatidrv_event_entry),
			Cache_Type_ALL, true);
	}
#endif

	/* Disable incoming Notify interrupts.  This is done to ensure that the
	 * eventEntry->flag is read atomically with any write back to shared
	 * memory */
	notify_ducatidrv_disable(handle);

	/* Is the local notify_ducatidrv_disable_event happening between the
	 * following two notify_ducatidrv_send_event operations on the remote
	 * processor?
	 * 1. Writing notify_ducatidrv_UP to shared memory
	 * 2. Sending the interrupt across
	 * If so, we should handle this event so the other core isn't left
	 * spinning until the event is re-enabled and the next
	 * notify_ducatidrv_isr executes This race condition is very rare but we
	 * need to account for it: */
	if (event_entry->flag == NOTIFYDUCATIDRIVER_UP) {
		/* Acknowledge the event. No need to store the payload. The
		 * other side will not send this event again even though flag is
		 * down, because the event is now disabled. So the payload
		 * within the eventChart will not get overwritten. */
		event_entry->flag = NOTIFYDUCATIDRIVER_DOWN;
#if 0
		/* Write back acknowledgement */
		if (obj->cache_enabled) {
			Cache_wbInv(event_entry,
				sizeof(struct notify_ducatidrv_event_entry),
				Cache_Type_ALL, TRUE);
		}
#endif
		/* Execute the callback function. This will execute in a Task
		 * or Swi context (not Hwi!) */
		notify_exec(obj->drv_handle->notify_handle, event_id,
			event_entry->payload);
	}

	/* Re-enable incoming Notify interrupts */
	notify_ducatidrv_enable(handle);

exit:
	if (status < 0) {
		pr_err("notify_ducatidrv_disable_event failed! "
			"status = 0x%x", status);
	}
	return;
}

/* Enable a specific event for this Notify ducati driver */
void notify_ducatidrv_enable_event(struct notify_driver_object *handle,
								u32 event_id)
{
	int status = 0;
	struct notify_ducatidrv_object *obj;

	if (WARN_ON(unlikely(handle == NULL))) {
		status = NOTIFY_E_INVALIDARG;
		goto exit;
	}
	if (WARN_ON(unlikely(handle->is_init != \
					NOTIFY_DRIVERINITSTATUS_DONE))) {
		status = NOTIFY_E_INVALIDARG;
		goto exit;
	}
	if (WARN_ON(unlikely(handle->notify_handle == NULL))) {
		status = NOTIFY_E_INVALIDARG;
		goto exit;
	}
	if (WARN_ON(unlikely(handle->notify_handle->driver_handle == NULL))) {
		status = NOTIFY_E_DRIVERNOTREGISTERED;
		goto exit;
	}

	obj = (struct notify_ducatidrv_object *)
		handle->notify_handle->driver_handle;
	if (event_id > obj->num_events) {
		status = NOTIFY_E_FAIL;
		goto exit;
	}

	/* Enter critical section protection. */
	status = mutex_lock_interruptible(
					notify_ducatidriver_state.gate_handle);
	if (status)
		goto exit;
	set_bit(event_id, (unsigned long *)
			&(obj->self_proc_ctrl->event_enable_mask));
	/* Leave critical section protection. */
	mutex_unlock(notify_ducatidriver_state.gate_handle);
#if 0
	if (obj->cache_enabled) {
		/* Writeback event_enable_mask */
		Cache_wbInv((void *) obj->self_proc_ctrl,
			sizeof(struct notify_ducatidrv_proc_ctrl),
			Cache_Type_ALL, true);
	}
#endif

exit:
	if (status < 0) {
		pr_err("notify_ducatidrv_enable_event failed! "
			"status = 0x%x", status);
	}
	return;
}

/* Get the shared memory requirements for the notify_ducatidrv. */
uint notify_ducatidrv_shared_mem_req(
				const struct notify_ducatidrv_params *params)
{
	uint mem_req = 0;
	u16 region_id;
	uint region_cache_size;
	uint min_align;
	s32 status = NOTIFY_S_SUCCESS;

	if (WARN_ON(unlikely(atomic_cmpmask_and_lt(
			&(notify_ducatidriver_state.ref_count),
			NOTIFYDUCATIDRIVER_MAKE_MAGICSTAMP(0),
			NOTIFYDUCATIDRIVER_MAKE_MAGICSTAMP(1)) == true))) {
		status = NOTIFY_E_INVALIDSTATE;
		goto exit;
	}
	if (WARN_ON(unlikely(params == NULL))) {
		status = NOTIFY_E_INVALIDARG;
		goto exit;
	}

	/* Determine obj->cache_enabled using params->cache_enabled and
	 * sharedregion cache flag setting, if applicable. */
	min_align = params->cache_line_size;
	region_id = sharedregion_get_id((void *) params->shared_addr);
	if (region_id != SHAREDREGION_INVALIDREGIONID) {
		region_cache_size = sharedregion_get_cache_line_size(region_id);

		/* Override the user cache line size setting if the region
		 * cache line size is smaller. */
		if (region_cache_size < min_align)
			min_align = region_cache_size;
	}

	/* Determine obj->eventEntrySize which will be used to ROUND_UP
	 * addresses */
	mem_req = ((ROUND_UP(sizeof(struct notify_ducatidrv_proc_ctrl),
			min_align)) * 2) + \
			((ROUND_UP(sizeof(struct notify_ducatidrv_event_entry),
			min_align) * 2 * notify_state.cfg.num_events));

exit:
	if (status < 0) {
		pr_err("notify_ducatidrv_shared_mem_req failed!"
			" status = 0x%x", status);
	}
	return mem_req;
}

/* This function implements the interrupt service routine for the interrupt
 * received from the Ducati processor. */
static int notify_shmdrv_isr(struct notifier_block *nb, unsigned long val,
								void *ntfy_msg)
{
	/* Decode the msg to identify the processor that has sent the message */
	u32 proc_id = (u32)ntfy_msg;
	struct notify_ducatidrv_object *obj;

	mutex_lock_killable(&notify_ducatidriver_state.dh_lock);
	/* Call the corresponding prpc_id callback */
	obj = notify_ducatidriver_state.driver_handles[proc_id][0];
	if (obj)
		notify_shmdrv_isr_callback(obj, ntfy_msg);
	mutex_unlock(&notify_ducatidriver_state.dh_lock);

	return 0;
}
EXPORT_SYMBOL(notify_shmdrv_isr);

static bool notify_shmdrv_isr_callback(void *ref_data, void *notify_msg)
{
	u32 payload = 0;
	u32 i = 0;
	VOLATILE struct notify_ducatidrv_event_entry  *event_entry;
	struct notify_ducatidrv_object *obj;
	u32 event_id;

	obj = (struct notify_ducatidrv_object *) ref_data;

	dsb();
	/* Execute the loop till no asserted event is found for one complete
	 * loop through all registered events */
	do {
		/* Check if the entry is a valid registered event.*/
		event_id = obj->reg_chart[i];
		if (event_id == (u32) -1)
			break;

		event_entry = EVENTENTRY(obj->self_event_chart,
					obj->event_entry_size, event_id);
#if 0
		if (obj->cache_enabled) {
			Cache_inv((void *)event_entry,
			sizeof(struct notify_ducatidrv_event_entry),
			Cache_Type_ALL, TRUE);
		}
#endif
		dsb();

		/* Determine the current high priority event.*/
		/* Check if the event is set and enabled.*/
		if (event_entry->flag == NOTIFYDUCATIDRIVER_UP &&
			test_bit(event_id, (unsigned long *)
				&obj->self_proc_ctrl->event_enable_mask)) {
			payload = event_entry->payload;

			/* Acknowledge the event. */
			event_entry->flag = NOTIFYDUCATIDRIVER_DOWN;

			/* Write back acknowledgement */
#if 0
			if (obj->cache_enabled) {
				Cache_inv((void *)event_entry,
				sizeof(struct notify_ducatidrv_event_entry),
				Cache_Type_ALL, TRUE);
			}
#endif
			dsb();

			/* Execute the callback function */
			notify_exec(obj->drv_handle->notify_handle, event_id,
					payload);
			/* reinitialize the event check counter. */
			i = 0;
		} else {
			/* check for next event. */
			i++;
		}
	} while ((event_id != (u32) -1) && (i < obj->num_events));

	return true;
}
