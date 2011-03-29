#define TMP_AUX_CLK_HACK 1 /* should be removed by Nov 13, 2010 */
#define SR_WA
/*
 * ipu_pm.c
 *
 * IPU Power Management support functions for TI OMAP processors.
 *
 * Copyright (C) 2009-2010 Texas Instruments, Inc.
 *
 * This package is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * THIS PACKAGE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#include <generated/autoconf.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/mod_devicetable.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/interrupt.h>
#include <linux/notifier.h>
#include <linux/clk.h>
#include <linux/uaccess.h>
#include <linux/irq.h>

#include <linux/platform_device.h>
#include <syslink/notify.h>
#include <syslink/notify_driver.h>
#include <syslink/notifydefs.h>
#include <syslink/notify_driverdefs.h>
#include <syslink/notify_ducatidriver.h>

/* Power Management headers */
#include <plat/omap_hwmod.h>
#include <plat/omap_device.h>
#include <plat/dma.h>
#include <plat/dmtimer.h>
#include <plat/clock.h>
#include <plat/i2c.h>
#include <plat/io.h>
#include <plat/iommu.h>
#include <plat/mailbox.h>
#include <plat/remoteproc.h>
#include <plat/omap-pm.h>
#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/semaphore.h>
#include <linux/jiffies.h>
#include <linux/clk.h>
#include <linux/regulator/consumer.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/i2c/twl.h>

/* Module headers */
#include <plat/ipu_dev.h>
#include "../../../../arch/arm/mach-omap2/cm.h"
#include "../../../../arch/arm/mach-omap2/prm.h"
#include "../../../../arch/arm/mach-omap2/prcm-common.h"
#include "../../../../arch/arm/mach-omap2/prm44xx.h"
#include "ipu_pm.h"

/** ============================================================================
 *  Macros and types
 *  ============================================================================
 */
#define HW_AUTO 3
#define CM_DUCATI_M3_CLKSTCTRL 0x4A008900
#define SL2_RESOURCE 10

#define proc_supported(proc_id) (proc_id == SYS_M3 || proc_id == APP_M3)
#define _has_cstrs(r) ((PM_FIRST_RES <= r && r < PM_NUM_RES_W_CSTRS) ? 1 : 0)
#define _is_res(r) ((PM_FIRST_RES <= r && r < PM_NUM_RES) ? 1 : 0)
#define _is_event(e) ((PM_FIRST_EVENT <= e && e < PM_LAST_EVENT) ? 1 : 0)

#define LINE_ID 0
#define NUM_SELF_PROC 2
#define IPU_KFIFO_SIZE 16
#define PM_VERSION 0x00020000

/* Flag provided by BIOS */
#define IDLE_FLAG_PHY_ADDR 0x9E0502D8

/* A9-M3 mbox status */
#define A9_M3_MBOX 0x4A0F4000
#define MBOX_MESSAGE_STATUS 0x000000CC

#define NUM_IDLE_CORES	((__raw_readl(appm3Idle) << 1) + \
				(__raw_readl(sysm3Idle)))

#define PENDING_MBOX_MSG	__raw_readl(a9_m3_mbox + MBOX_MESSAGE_STATUS)

/** ============================================================================
 *  Forward declarations of internal functions
 *  ============================================================================
 */

/* Request a sdma channels on behalf of an IPU client */
static inline int ipu_pm_get_sdma_chan(struct ipu_pm_object *handle,
				       struct rcb_block *rcb_p,
				       struct ipu_pm_params *params);

/* Request a gptimer on behalf of an IPU client */
static inline int ipu_pm_get_gptimer(struct ipu_pm_object *handle,
				     struct rcb_block *rcb_p,
				     struct ipu_pm_params *params);

/* Request an i2c bus on behalf of an IPU client */
static inline int ipu_pm_get_i2c_bus(struct ipu_pm_object *handle,
				     struct rcb_block *rcb_p,
				     struct ipu_pm_params *params);

/* Request a gpio on behalf of an IPU client */
static inline int ipu_pm_get_gpio(struct ipu_pm_object *handle,
				  struct rcb_block *rcb_p,
				  struct ipu_pm_params *params);

/* Request a regulator on behalf of an IPU client */
static inline int ipu_pm_get_regulator(struct ipu_pm_object *handle,
				       struct rcb_block *rcb_p,
				       struct ipu_pm_params *params);

/* Request an Aux clk on behalf of an IPU client */
static inline int ipu_pm_get_aux_clk(struct ipu_pm_object *handle,
				     struct rcb_block *rcb_p,
				     struct ipu_pm_params *params);

/* Request sys m3 on behalf of an IPU client */
static inline int ipu_pm_get_sys_m3(struct ipu_pm_object *handle,
				    struct rcb_block *rcb_p,
				    struct ipu_pm_params *params);

/* Request app m3 on behalf of an IPU client */
static inline int ipu_pm_get_app_m3(struct ipu_pm_object *handle,
				    struct rcb_block *rcb_p,
				    struct ipu_pm_params *params);

/* Request L3 Bus on behalf of an IPU client */
static inline int ipu_pm_get_l3_bus(struct ipu_pm_object *handle,
				    struct rcb_block *rcb_p,
				    struct ipu_pm_params *params);

/* Request IVA HD on behalf of an IPU client */
static inline int ipu_pm_get_iva_hd(struct ipu_pm_object *handle,
				    struct rcb_block *rcb_p,
				    struct ipu_pm_params *params);

/* Request FDIF on behalf of an IPU client */
static inline int ipu_pm_get_fdif(struct ipu_pm_object *handle,
				  struct rcb_block *rcb_p,
				  struct ipu_pm_params *params);

/* Request MPU on behalf of an IPU client */
static inline int ipu_pm_get_mpu(struct ipu_pm_object *handle,
				 struct rcb_block *rcb_p,
				 struct ipu_pm_params *params);

/* Request IPU on behalf of an IPU client */
static inline int ipu_pm_get_ipu(struct ipu_pm_object *handle,
				 struct rcb_block *rcb_p,
				 struct ipu_pm_params *params);

/* Request IVA SEQ0 on behalf of an IPU client */
static inline int ipu_pm_get_ivaseq0(struct ipu_pm_object *handle,
				     struct rcb_block *rcb_p,
				     struct ipu_pm_params *params);

/* Request IVA SEQ1 on behalf of an IPU client */
static inline int ipu_pm_get_ivaseq1(struct ipu_pm_object *handle,
				     struct rcb_block *rcb_p,
				     struct ipu_pm_params *params);

/* Request ISS on behalf of an IPU client */
static inline int ipu_pm_get_iss(struct ipu_pm_object *handle,
				 struct rcb_block *rcb_p,
				 struct ipu_pm_params *params);

/* Release a sdma on behalf of an IPU client */
static inline int ipu_pm_rel_sdma_chan(struct ipu_pm_object *handle,
				       struct rcb_block *rcb_p,
				       struct ipu_pm_params *params);

/* Release a gptimer on behalf of an IPU client */
static inline int ipu_pm_rel_gptimer(struct ipu_pm_object *handle,
				     struct rcb_block *rcb_p,
				     struct ipu_pm_params *params);

/* Release an i2c buses on behalf of an IPU client */
static inline int ipu_pm_rel_i2c_bus(struct ipu_pm_object *handle,
				     struct rcb_block *rcb_p,
				     struct ipu_pm_params *params);

/* Release a gpio on behalf of an IPU client */
static inline int ipu_pm_rel_gpio(struct ipu_pm_object *handle,
				  struct rcb_block *rcb_p,
				  struct ipu_pm_params *params);

/* Release a regulator on behalf of an IPU client */
static inline int ipu_pm_rel_regulator(struct ipu_pm_object *handle,
				       struct rcb_block *rcb_p,
				       struct ipu_pm_params *params);

/* Release an Aux clk on behalf of an IPU client */
static inline int ipu_pm_rel_aux_clk(struct ipu_pm_object *handle,
				     struct rcb_block *rcb_p,
				     struct ipu_pm_params *params);

/* Release sys m3 on behalf of an IPU client */
static inline int ipu_pm_rel_sys_m3(struct ipu_pm_object *handle,
				    struct rcb_block *rcb_p,
				    struct ipu_pm_params *params);

/* Release app m3 on behalf of an IPU client */
static inline int ipu_pm_rel_app_m3(struct ipu_pm_object *handle,
				    struct rcb_block *rcb_p,
				    struct ipu_pm_params *params);

/* Release L3 Bus on behalf of an IPU client */
static inline int ipu_pm_rel_l3_bus(struct ipu_pm_object *handle,
				    struct rcb_block *rcb_p,
				    struct ipu_pm_params *params);

/* Release IVA HD on behalf of an IPU client */
static inline int ipu_pm_rel_iva_hd(struct ipu_pm_object *handle,
				    struct rcb_block *rcb_p,
				    struct ipu_pm_params *params);

/* Release FDIF on behalf of an IPU client */
static inline int ipu_pm_rel_fdif(struct ipu_pm_object *handle,
				  struct rcb_block *rcb_p,
				  struct ipu_pm_params *params);

/* Release MPU on behalf of an IPU client */
static inline int ipu_pm_rel_mpu(struct ipu_pm_object *handle,
				 struct rcb_block *rcb_p,
				 struct ipu_pm_params *params);

/* Release IPU on behalf of an IPU client */
static inline int ipu_pm_rel_ipu(struct ipu_pm_object *handle,
				 struct rcb_block *rcb_p,
				 struct ipu_pm_params *params);

/* Release IVA SEQ0 on behalf of an IPU client */
static inline int ipu_pm_rel_ivaseq0(struct ipu_pm_object *handle,
				     struct rcb_block *rcb_p,
				     struct ipu_pm_params *params);

/* Release IVA SEQ1 on behalf of an IPU client */
static inline int ipu_pm_rel_ivaseq1(struct ipu_pm_object *handle,
				     struct rcb_block *rcb_p,
				     struct ipu_pm_params *params);

/* Release ISS on behalf of an IPU client */
static inline int ipu_pm_rel_iss(struct ipu_pm_object *handle,
				 struct rcb_block *rcb_p,
				 struct ipu_pm_params *params);

/* Request a res constraint on behalf of an IPU client */
static inline int ipu_pm_req_cstr(struct ipu_pm_object *handle,
				  struct rcb_block *rcb_p,
				  struct ipu_pm_params *params);

/* Release a res constraint on behalf of an IPU client */
static inline int ipu_pm_rel_cstr(struct ipu_pm_object *handle,
				  struct rcb_block *rcb_p,
				  struct ipu_pm_params *params);

/* Hibernate and watch dog timer interrupt */
static irqreturn_t ipu_pm_timer_interrupt(int irq,
					void *dev_id);

/* Hibernate and watch dog timer function */
static int ipu_pm_timer_state(int event);

#ifdef CONFIG_DUCATI_WATCH_DOG
/* Functions for reporing watch dog reset */
static int ipu_pm_notify_event(int event, void *data);
#endif

/** ============================================================================
 *  Globals
 *  ============================================================================
 */

/* Usage Masks */
static u32 GPTIMER_USE_MASK = 0xFFFF;
static u32 I2C_USE_MASK = 0xFFFF;
static u32 AUX_CLK_USE_MASK = 0xFFFF;

/* Previous voltage value of secondary camera regulator */
static u32 cam2_prev_volt;

/* Resources and notifications*/
static struct ipu_pm_object *pm_handle_appm3;
static struct ipu_pm_object *pm_handle_sysm3;
struct sms *global_rcb;
static struct workqueue_struct *ipu_resources;

/* Clean up and recover */
static struct workqueue_struct *ipu_clean_up;
struct work_struct clean;
static DECLARE_COMPLETION(ipu_clean_up_comp);
static bool recover;

/* Latency cstrs */
#ifdef CONFIG_OMAP_PM
static struct pm_qos_request_list *pm_qos_handle;
static struct pm_qos_request_list *pm_qos_handle_2;
#endif

/* Save/restore for hibernation */
static struct omap_rproc *sys_rproc;
static struct omap_rproc *app_rproc;
static struct omap_mbox *ducati_mbox;
static struct iommu *ducati_iommu;
static bool first_time = 1;
static bool _is_iommu_up;
static bool _is_mbox_up;

/* BIOS flags states for each core in IPU */
static void __iomem *sysm3Idle;
static void __iomem *appm3Idle;
static void __iomem *a9_m3_mbox;
#ifdef SR_WA
static void __iomem *issHandle;
static void __iomem *fdifHandle;
#endif

/* ISS optional clock */
static struct clk *clk_opt_iss;
/* Ducati Interrupt Capable Gptimers */
static int ipu_timer_list[NUM_IPU_TIMERS] = {
	GP_TIMER_9,
	GP_TIMER_11};

/* I2C spinlock assignment mapping table */
static int i2c_spinlock_list[I2C_BUS_MAX + 1] = {
	I2C_SL_INVAL,
	I2C_1_SL,
	I2C_2_SL,
	I2C_3_SL,
	I2C_4_SL};

/* Camera regulator */
static char *ipu_regulator_name[REGULATOR_MAX] = {
	"cam2pwr"};

static struct clk *aux_clk_ptr[NUM_AUX_CLK];

static char *aux_clk_name[NUM_AUX_CLK] = {
	"auxclk0_ck",
	"auxclk1_ck",
	"auxclk2_ck",
	"auxclk3_ck",
	"auxclk4_ck",
	"auxclk5_ck",
} ;

static char *aux_clk_source_name[] = {
	"sys_clkin_ck",
	"dpll_core_m3x2_ck",
	"dpll_per_m3x2_ck",
	NULL
} ;

/* static struct clk *aux_clk_source_clocks[3]; */

static struct ipu_pm_module_object ipu_pm_state = {
	.def_cfg.reserved = 1,
	.gate_handle = NULL
} ;

/* Default params for ipu_pm handle */
static struct ipu_pm_params pm_params = {
	.pm_fdif_counter = 0,
	.pm_ipu_counter = 0,
	.pm_sys_m3_counter = 0,
	.pm_app_m3_counter = 0,
	.pm_iss_counter = 0,
	.pm_iva_hd_counter = 0,
	.pm_ivaseq0_counter = 0,
	.pm_ivaseq1_counter = 0,
	.pm_sl2if_counter = 0,
	.pm_l3_bus_counter = 0,
	.pm_mpu_counter = 0,
	.pm_sdmachan_counter = 0,
	.pm_gptimer_counter = 0,
	.pm_gpio_counter = 0,
	.pm_i2c_bus_counter = 0,
	.pm_regulator_counter = 0,
	.pm_aux_clk_counter = 0,
	.timeout = 10000,
	.shared_addr = NULL,
	.pm_num_events = NUMBER_PM_EVENTS,
	.pm_resource_event = PM_RESOURCE,
	.pm_notification_event = PM_NOTIFICATION,
	.proc_id = A9,
	.remote_proc_id = -1,
	.line_id = 0,
	.hib_timer_state = PM_HIB_TIMER_RESET,
	.wdt_time = 0
} ;

/* Functions to request resources */
static int (*request_fxn[PM_NUM_RES]) (struct ipu_pm_object *handle,
				       struct rcb_block *rcb_p,
				       struct ipu_pm_params *params) = {
	ipu_pm_get_fdif,
	ipu_pm_get_ipu,
	ipu_pm_get_sys_m3,
	ipu_pm_get_app_m3,
	ipu_pm_get_iss,
	ipu_pm_get_iva_hd,
	ipu_pm_get_ivaseq0,
	ipu_pm_get_ivaseq1,
	ipu_pm_get_l3_bus,
	ipu_pm_get_mpu,
	/* ipu_pm_get_sl2if, */
	/* ipu_pm_get_dsp, */
	ipu_pm_get_sdma_chan,
	ipu_pm_get_gptimer,
	ipu_pm_get_gpio,
	ipu_pm_get_i2c_bus,
	ipu_pm_get_regulator,
	ipu_pm_get_aux_clk,
};

/* Functions to release resources */
static int (*release_fxn[PM_NUM_RES]) (struct ipu_pm_object *handle,
				       struct rcb_block *rcb_p,
				       struct ipu_pm_params *params) = {
	ipu_pm_rel_fdif,
	ipu_pm_rel_ipu,
	ipu_pm_rel_sys_m3,
	ipu_pm_rel_app_m3,
	ipu_pm_rel_iss,
	ipu_pm_rel_iva_hd,
	ipu_pm_rel_ivaseq0,
	ipu_pm_rel_ivaseq1,
	ipu_pm_rel_l3_bus,
	ipu_pm_rel_mpu,
	/* ipu_pm_rel_sl2if, */
	/* ipu_pm_rel_dsp, */
	ipu_pm_rel_sdma_chan,
	ipu_pm_rel_gptimer,
	ipu_pm_rel_gpio,
	ipu_pm_rel_i2c_bus,
	ipu_pm_rel_regulator,
	ipu_pm_rel_aux_clk,

};

static struct blocking_notifier_head ipu_pm_notifier;

/*
  Function to schedule the recover process
 *
 */
static void ipu_pm_recover_schedule(void)
{
	struct ipu_pm_object *handle;

	INIT_COMPLETION(ipu_clean_up_comp);
	recover = true;

	/* get the handle to proper ipu pm object
	 * and flush any pending fifo message
	 */
	handle = ipu_pm_get_handle(APP_M3);
	if (handle)
		kfifo_reset(&handle->fifo);
	handle = ipu_pm_get_handle(SYS_M3);
	if (handle)
		kfifo_reset(&handle->fifo);

	/* schedule clean up work */
	queue_work(ipu_clean_up, &clean);
}

/*
  Function to clean/release all the resources
 *
 */
static void ipu_pm_clean_res(void)
{
	/* Check RAT and call each release function
	 * per resource in rcb
	 */
	unsigned used_res_mask = global_rcb->rat;
	struct ipu_pm_object *handle;
	struct ipu_pm_params *params;
	struct rcb_block *rcb_p;
	int cur_rcb = 0;
	u32 mask;
	int res;
	int retval = 0;

	/* The reserved RCB's are marked as valid
	 * by the remote proc but are not requesting any
	 * resource so no need to release.
	 * rcb = 0 is invalid.
	 * rcb = 1 is reserved.
	 * Start in 2
	 */
	used_res_mask &= RESERVED_RCBS;
	pr_debug("Resources Mask 0x%x\n", used_res_mask);

	if (!used_res_mask)
		goto complete_exit;

	while (cur_rcb < RCB_MAX && used_res_mask != 0) {
		cur_rcb++;
		mask = 1;
		mask = mask << cur_rcb;

		if (!(mask & used_res_mask))
			continue;

		rcb_p = (struct rcb_block *)&global_rcb->rcb[cur_rcb];
		handle = ipu_pm_get_handle(rcb_p->rqst_cpu);
		if (!handle) {
			pr_err("Invalid RCB, maybe corrupted!\n");
			used_res_mask &= ~mask;
			continue;
		}
		params = handle->params;
		res = rcb_p->sub_type;

		if (_has_cstrs(res)) {
			pr_debug("Releasing res:%d cstrs\n", res);
			retval = ipu_pm_rel_cstr(handle, rcb_p, params);
			if (retval)
				pr_err("Error releasing res:%d cstrs\n", res);
		}

		/* The first resource is internally created to manage the
		 * the constrainst of the IPU via SYS and APP and it shouldn't
		 * be released as a resource
		 */
		if (cur_rcb > 1) {
			if (_is_res(res)) {
				pr_debug("Releasing res:%d\n", res);
				retval = release_fxn[res](handle, rcb_p,
									params);
				if (retval)
					pr_err("Can't release resource: %d\n",
									   res);
			}
		}

		/* Clear the RCB from the RAT if success */
		if (!retval)
			used_res_mask &= ~mask;
		pr_debug("Mask 0x%x\n", used_res_mask);
	}

	global_rcb->rat = used_res_mask;
	if (!used_res_mask)
		goto complete_exit;

	pr_warning("%s:Not all resources were released", __func__);
	return;
complete_exit:
	complete_all(&ipu_clean_up_comp);
	return;
}

/*
  Work Function to clean up resources
 *
 */
static void ipu_pm_clean_work(struct work_struct *work)
{
	ipu_pm_clean_res();
}

/*
  Notifier Function to handle IOMMU events
 *
 */
static int ipu_pm_iommu_notifier_call(struct notifier_block *nb,
					unsigned long val, void *v)
{
	switch ((int)val) {
	case IOMMU_CLOSE:
		/*
		 * FIXME: this need to be checked by the iommu driver
		 * restore IOMMU since it is required the IOMMU
		 * is up and running for reclaiming MMU entries
		 */
		if (!_is_iommu_up) {
			iommu_restore_ctx(ducati_iommu);
			_is_iommu_up = 1;
		}
		return 0;
	case IOMMU_FAULT:
		ipu_pm_recover_schedule();
		return 0;
	default:
		return 0;
	}
}

static struct notifier_block ipu_pm_notify_nb_iommu_ducati = {
	.notifier_call = ipu_pm_iommu_notifier_call,
};

/*
  Work Function to req/rel a resource
 *
 */
static void ipu_pm_work(struct work_struct *work)
{
	struct ipu_pm_object *handle =
			container_of(work, struct ipu_pm_object, work);
	struct rcb_block *rcb_p;
	struct ipu_pm_msg im;
	struct ipu_pm_params *params;
	union message_slicer pm_msg;
	int action;
	int res;
	int rcb_num;
	int retval;
	int len;

	if (WARN_ON(handle == NULL))
		return;

	params = handle->params;
	if (WARN_ON(params == NULL))
		return;

	while (kfifo_len(&handle->fifo) >= sizeof(im)) {
		/* set retval for each iteration asumming error */
		retval = PM_UNSUPPORTED;
		spin_lock_irq(&handle->lock);
		len = kfifo_out(&handle->fifo, &im, sizeof(im));
		spin_unlock_irq(&handle->lock);

		if (unlikely(len != sizeof(im))) {
			pr_err("%s: unexpected amount of data from kfifo_out: "
					"%d\n", __func__, len);
			continue;
		}

		/* Get the payload */
		pm_msg.whole = im.pm_msg;
		/* Get the rcb_num */
		rcb_num = pm_msg.fields.rcb_num;
		/* Get pointer to the proper RCB */
		if (WARN_ON((rcb_num < RCB_MIN) || (rcb_num > RCB_MAX)))
			return;
		rcb_p = (struct rcb_block *)&handle->rcb_table->rcb[rcb_num];

		/* Get the type of resource and the actions required */
		action = rcb_p->msg_type;
		res = rcb_p->sub_type;
		if (!_is_res(res)) {
			pr_err("Invalid res number: %d\n", res);
			/* No need to continue, send error back */
			goto send_msg;
		}
		switch (action) {
		case PM_REQUEST_RESOURCE:
			if (request_fxn[res])
				retval = request_fxn[res](handle, rcb_p,
								  params);
			break;
		case PM_RELEASE_RESOURCE:
			if (release_fxn[res])
				retval = release_fxn[res](handle, rcb_p,
								  params);
			break;
		case PM_REQUEST_CONSTRAINTS:
			if (_has_cstrs(res))
				retval = ipu_pm_req_cstr(handle, rcb_p,
								 params);
			break;
		case PM_RELEASE_CONSTRAINTS:
			if (_has_cstrs(res))
				retval = ipu_pm_rel_cstr(handle, rcb_p,
								 params);
			break;
		default:
			pm_msg.fields.msg_type = PM_FAIL;
			break;
		}
send_msg:
		if (retval != PM_SUCCESS)
			pm_msg.fields.msg_type = PM_FAIL;
		/* Update the payload with the reply msg */
		pm_msg.fields.reply_flag = true;
		pm_msg.fields.parm = retval;

		/* Restore the payload and send to caller*/
		im.pm_msg = pm_msg.whole;

		/* Send the ACK to Remote Proc*/
		retval = notify_send_event(
					params->remote_proc_id,
					params->line_id,
					params->pm_resource_event | \
						(NOTIFY_SYSTEMKEY << 16),
					im.pm_msg,
					true);
		if (retval < 0)
			pr_err("Error sending notify event\n");
	}
}

void ipu_pm_callback(u16 proc_id, u16 line_id, u32 event_id,
					uint *arg, u32 payload)
{
	struct ipu_pm_object *handle;
	struct ipu_pm_msg im;

	/*get the handle to proper ipu pm object */
	handle = ipu_pm_get_handle(proc_id);

	if (WARN_ON(unlikely(handle == NULL)))
		return;

	im.proc_id = proc_id;
	im.pm_msg = payload;

	spin_lock_irq(&handle->lock);
	if (kfifo_avail(&handle->fifo) >= sizeof(im)) {
		kfifo_in(&handle->fifo, (unsigned char *)&im, sizeof(im));
		queue_work(ipu_resources, &handle->work);
	}
	spin_unlock_irq(&handle->lock);
}
EXPORT_SYMBOL(ipu_pm_callback);


/* Function for PM notifications Callback
 * This functions receives an event coming from
 * remote proc as an ack.
 * Post semaphore based in eventType (payload)
 * If PM_HIBERNATE is received the save_ctx is triggered
 * in order to put remote proc in reset.
 */
void ipu_pm_notify_callback(u16 proc_id, u16 line_id, u32 event_id,
					uint *arg, u32 payload)
{
	struct ipu_pm_object *handle;
	union message_slicer pm_msg;
	struct ipu_pm_params *params;
	enum pm_event_type event;
	int retval;

	/* get the handle to proper ipu pm object */
	handle = ipu_pm_get_handle(proc_id);
	if (WARN_ON(unlikely(handle == NULL)))
		return;
	params = handle->params;
	if (WARN_ON(unlikely(params == NULL)))
		return;

	pm_msg.whole = payload;
	/* get the event type sent by remote proc */
	event = pm_msg.fields.msg_subtype;
	if (!_is_event(event))
		goto error;
	if (event == PM_HIBERNATE) {
		/* If any resource in use, no hibernation */
		if (!(handle->rcb_table->state_flag & HIB_REF_MASK)) {
			/* Remote Proc is ready to hibernate
			 * PM_HIBERNATE is a one way notification
			 * Remote proc to Host proc
			 */
			pr_debug("Remote Proc is ready to hibernate\n");

			retval = ipu_pm_save_ctx(proc_id);
			if (retval)
				pr_err("Unable to stop proc %d\n", proc_id);
		} else
			pr_err("Hibernation is not allowed if resource in use");
	} else if (event == PM_ALIVE) {
		/* If resources are in use ipu will send an event to make
		 * sure it is in a good state but hibernation won't happen
		 * and the timer will be reloaded to hib_time again.
		 */
		pr_debug("Unable to stop proc, Resources in use\n");
		ipu_pm_timer_state(PM_HIB_TIMER_ON);
	} else {
		pr_debug("Remote Proc received %d event\n", event);
		handle->pm_event[event].pm_msg = payload;
		up(&handle->pm_event[event].sem_handle);
	}

	return;
error:
	pr_err("Unknow event received from remote proc: %d\n", event);
}
EXPORT_SYMBOL(ipu_pm_notify_callback);

/* Function for send PM Notifications
 * Function used by devh and dev_pm
 * Recieves evenType: Suspend, Resume, Proc_Obit
 * Send event to Ducati
 * Pend semaphore based in event_type (payload)
 * Return ACK to caller
 */
int ipu_pm_notifications(int proc_id, enum pm_event_type event, void *data)
{
	struct ipu_pm_object *handle;
	struct ipu_pm_params *params;
	union message_slicer pm_msg;
	int retval;
	int pm_ack = 0;

	handle = ipu_pm_get_handle(proc_id);
	if (WARN_ON(handle == NULL))
		return -EINVAL;
	params = handle->params;
	if (WARN_ON(params == NULL))
		return -EINVAL;

	/* Prepare the message for remote proc */
	pm_msg.fields.msg_type = PM_NOTIFICATIONS;
	pm_msg.fields.msg_subtype = event;
	pm_msg.fields.parm = PM_SUCCESS;

	/* put general purpose message in share memory
	 * this message is just for devh but it can be use
	 * for sending any message to the remote proc
	 * along with the notification
	 */
	handle->rcb_table->gp_msg = (unsigned)data;

	/* send the request to IPU*/
	retval = notify_send_event(params->remote_proc_id,
				   params->line_id,
				   params->pm_notification_event | \
					(NOTIFY_SYSTEMKEY << 16),
				   (unsigned int)pm_msg.whole,
				   true);
	if (retval < 0)
		goto error_send;

	/* wait for remote proc ack (ipu_pm_notify_callback)*/
	retval = down_timeout(&handle->pm_event[event].sem_handle,
			      msecs_to_jiffies(params->timeout));

	if (retval < 0)
		goto error;

	pm_msg.whole = handle->pm_event[event].pm_msg;
	if (pm_msg.fields.parm != PM_SUCCESS) {
		pr_err("Error in Proc:%d for Event_type:%d\n", proc_id, event);
		return -EINVAL;
	}

	return pm_ack;

error_send:
	pr_err("Error sending Notify Event:%d to Proc:%d\n",
						params->pm_notification_event,
						proc_id);
	return -EINVAL;
error:
	pr_err("Time out when sending Notify Event:%d to Proc:%d\n",
						params->pm_notification_event,
						proc_id);
	return -EBUSY;
}
EXPORT_SYMBOL(ipu_pm_notifications);

/*
  Request a sdma channels on behalf of an IPU client
 *
 */
static inline int ipu_pm_get_sdma_chan(struct ipu_pm_object *handle,
				       struct rcb_block *rcb_p,
				       struct ipu_pm_params *params)
{
	int pm_sdmachan_num;
	int pm_sdmachan_dummy;
	int ch;
	int ch_aux;
	int retval;

	/* Get number of channels from RCB */
	pm_sdmachan_num = rcb_p->num_chan;
	if (WARN_ON((pm_sdmachan_num <= 0) ||
			(pm_sdmachan_num > SDMA_CHANNELS_MAX))) {
		pr_err("%s %d Not able to provide %d channels\n", __func__
								, __LINE__
							     , pm_sdmachan_num);
		return PM_INVAL_NUM_CHANNELS;
	}

	/* Request resource using PRCM API */
	for (ch = 0; ch < pm_sdmachan_num; ch++) {
		retval = omap_request_dma(params->proc_id,
			"ducati-ss",
			NULL,
			NULL,
			&pm_sdmachan_dummy);
		if (retval == 0) {
			params->pm_sdmachan_counter++;
			rcb_p->channels[ch] = (unsigned char)pm_sdmachan_dummy;
			pr_debug("Providing sdma ch %d\n", pm_sdmachan_dummy);
		} else {
			pr_err("%s %d Error providing sdma channel\n", __func__
								    , __LINE__);
			goto clean_sdma;
		}
	}
	return PM_SUCCESS;
clean_sdma:
	/*failure, need to free the channels*/
	pr_debug("Freeing sdma channel because of failure\n");
	for (ch_aux = 0; ch_aux < ch; ch_aux++) {
		pm_sdmachan_dummy = (int)rcb_p->channels[ch_aux];
		omap_free_dma(pm_sdmachan_dummy);
		params->pm_sdmachan_counter--;
	}
	return PM_INSUFFICIENT_CHANNELS;
}

/*
  Request a gptimer on behalf of an IPU client
 *
 */
static inline int ipu_pm_get_gptimer(struct ipu_pm_object *handle,
				     struct rcb_block *rcb_p,
				     struct ipu_pm_params *params)
{
	struct omap_dm_timer *p_gpt = NULL;
	int pm_gp_num;

	/* Request resource using PRCM API */
	for (pm_gp_num = 0; pm_gp_num < NUM_IPU_TIMERS; pm_gp_num++) {
		if (GPTIMER_USE_MASK & (1 << ipu_timer_list[pm_gp_num])) {
			p_gpt = omap_dm_timer_request_specific
				(ipu_timer_list[pm_gp_num]);
		} else
			continue;
		if (p_gpt != NULL) {
			/* Check the range of the src clk */
			if (rcb_p->data[0] < OMAP_TIMER_SRC_SYS_CLK ||
				rcb_p->data[0] > OMAP_TIMER_SRC_32_KHZ) {
				/* Default src clk is SYS_CLK */
				pr_debug("Setting Default Clock Source\n");
				rcb_p->data[0] = OMAP_TIMER_SRC_SYS_CLK;
			}
			/* Set the src clk of gpt */
			omap_dm_timer_set_source(p_gpt, rcb_p->data[0]);
			/* Clear the bit in the usage mask */
			GPTIMER_USE_MASK &= ~(1 << ipu_timer_list[pm_gp_num]);
			break;
		}
	}
	if (p_gpt == NULL) {
		pr_err("%s %d Error providing gp_timer\n", __func__, __LINE__);
		return PM_NO_GPTIMER;
	} else {
		/* Store the gptimer number and base address */
		rcb_p->fill9 = ipu_timer_list[pm_gp_num];
		pr_debug("Providing gp_timer %d\n", rcb_p->fill9);
		rcb_p->mod_base_addr = (unsigned)p_gpt;
		params->pm_gptimer_counter++;
		return PM_SUCCESS;
	}
}

/*
  Request an i2c bus on behalf of an IPU client
 *
 */
static inline int ipu_pm_get_i2c_bus(struct ipu_pm_object *handle,
				     struct rcb_block *rcb_p,
				     struct ipu_pm_params *params)
{
	struct clk *p_i2c_clk;
	int i2c_clk_status;
	char i2c_name[I2C_NAME_SIZE];
	int pm_i2c_bus_num;

	pm_i2c_bus_num = rcb_p->fill9;
	if (WARN_ON((pm_i2c_bus_num < I2C_BUS_MIN) ||
			(pm_i2c_bus_num > I2C_BUS_MAX)))
		goto error;

	if (I2C_USE_MASK & (1 << pm_i2c_bus_num)) {
		/* building the name for i2c_clk */
		sprintf(i2c_name, "i2c%d_fck", pm_i2c_bus_num);

		/* Request resource using PRCM API */
		p_i2c_clk = omap_clk_get_by_name(i2c_name);
		if (p_i2c_clk == NULL) {
			pr_err("%s %d Error providing i2c_%d\n"
						, __func__, __LINE__
						, pm_i2c_bus_num);
			goto error;
		}
		i2c_clk_status = clk_enable(p_i2c_clk);
		if (i2c_clk_status != 0) {
			pr_err("%s %d Error enabling i2c_%d clock\n"
						, __func__, __LINE__
						, pm_i2c_bus_num);
			goto error;
		}
		/* Clear the bit in the usage mask */
		I2C_USE_MASK &= ~(1 << pm_i2c_bus_num);
		rcb_p->mod_base_addr = (unsigned)p_i2c_clk;
		/* Get the HW spinlock and store it in the RCB */
		rcb_p->data[0] = i2c_spinlock_list[pm_i2c_bus_num];
		params->pm_i2c_bus_counter++;
		pr_debug("Providing %s\n", i2c_name);

		return PM_SUCCESS;
	}
error:
	return PM_NO_I2C;
}

/*
  Request a gpio on behalf of an IPU client
 *
 */
static inline int ipu_pm_get_gpio(struct ipu_pm_object *handle,
				  struct rcb_block *rcb_p,
				  struct ipu_pm_params *params)
{
	int pm_gpio_num;
	int retval;

	pm_gpio_num = rcb_p->fill9;
	retval = gpio_request(pm_gpio_num , "ducati-ss");
	if (retval != 0) {
		pr_err("%s %d Error providing gpio %d\n", __func__, __LINE__
							, pm_gpio_num);
		return PM_NO_GPIO;
	}
	pr_debug("Providing gpio %d\n", pm_gpio_num);
	params->pm_gpio_counter++;

	return PM_SUCCESS;
}

/*
  Request a regulator on behalf of an IPU client
 *
 */
static inline int ipu_pm_get_regulator(struct ipu_pm_object *handle,
				       struct rcb_block *rcb_p,
				       struct ipu_pm_params *params)
{
	struct regulator *p_regulator = NULL;
	char *regulator_name;
	int pm_regulator_num;
	int retval = 0;

	pm_regulator_num = rcb_p->fill9;
	if (WARN_ON((pm_regulator_num < REGULATOR_MIN) ||
			(pm_regulator_num > REGULATOR_MAX))) {
		pr_err("%s %d Invalid regulator %d\n", __func__, __LINE__
						     , pm_regulator_num);
		goto error;
	}

	/*
	  FIXME:Only providing 1 regulator, if more are provided
	 *	this check is not valid.
	 */
	if (WARN_ON(params->pm_regulator_counter > 0)) {
		pr_err("%s %d No more regulators\n", __func__, __LINE__);
		goto error;
	}

	/* Search the name of regulator based on the id and request it */
	regulator_name = ipu_regulator_name[pm_regulator_num - 1];
	p_regulator = regulator_get(NULL, regulator_name);
	if (p_regulator == NULL) {
		pr_err("%s %d Error providing regulator %s\n", __func__
							     , __LINE__
							     , regulator_name);
		goto error;
	}

	/* Get and store the regulator default voltage */
	cam2_prev_volt = regulator_get_voltage(p_regulator);

	/* Set the regulator voltage min = data[0]; max = data[1]*/
	retval = regulator_set_voltage(p_regulator, rcb_p->data[0],
					rcb_p->data[1]);
	if (retval) {
		pr_err("%s %d Error setting %s voltage\n", __func__
							 , __LINE__
							 , regulator_name);
		goto error;
	}

	/* Store the regulator handle in the RCB */
	rcb_p->mod_base_addr = (unsigned)p_regulator;
	params->pm_regulator_counter++;
	pr_debug("Providing regulator %s\n", regulator_name);

	return PM_SUCCESS;
error:
	return PM_INVAL_REGULATOR;
}

/*
  Request an Aux clk on behalf of an IPU client
 *
 */
static inline int ipu_pm_get_aux_clk(struct ipu_pm_object *handle,
				     struct rcb_block *rcb_p,
				     struct ipu_pm_params *params)
{
	u32 a_clk = 0;
	int ret;
	int pm_aux_clk_num;

	pm_aux_clk_num = rcb_p->fill9;

	if (WARN_ON((pm_aux_clk_num < AUX_CLK_MIN) ||
			(pm_aux_clk_num > AUX_CLK_MAX))) {
		pr_err("%s %d Invalid aux_clk %d\n", __func__, __LINE__
						   , pm_aux_clk_num);
		return PM_INVAL_AUX_CLK;
	}

	if (AUX_CLK_USE_MASK & (1 << pm_aux_clk_num)) {
		struct clk *aux_clk;
		struct clk *aux_clk_src_ptr;

		aux_clk = clk_get(NULL, aux_clk_name[pm_aux_clk_num]);
		if (!aux_clk) {
			pr_err("%s %d Unable to get %s\n", __func__, __LINE__
						, aux_clk_name[pm_aux_clk_num]);
			return PM_NO_AUX_CLK;
		}
		if (unlikely(aux_clk->usecount != 0)) {
			pr_err("%s %d aux_clk%d->usecount = %d, expected to "
				"be zero as there should be no other users\n",
				__func__, __LINE__, pm_aux_clk_num,
				aux_clk->usecount);
		}

		aux_clk_src_ptr = clk_get(NULL,
			aux_clk_source_name[PER_DPLL_CLK]);
		if (!aux_clk_src_ptr) {
			pr_err("%s %d Unable to get aux_clk source %s\n"
							, __func__, __LINE__
					, aux_clk_source_name[PER_DPLL_CLK]);
			return PM_NO_AUX_CLK;
		}
		ret = clk_set_parent(aux_clk, aux_clk_src_ptr);
		if (ret) {
			pr_err("%s %d Unable to set clk %s"
				" as parent of aux_clk %s\n"
				, __func__, __LINE__
				, aux_clk_source_name[PER_DPLL_CLK]
				, aux_clk_name[pm_aux_clk_num]);
			return PM_NO_AUX_CLK;
		}

		/* update divisor manually until API available */
		a_clk = __raw_readl(AUX_CLK_REG(pm_aux_clk_num));
		MASK_CLEAR_FIELD(a_clk, AUX_CLK_CLKDIV);
		MASK_SET_FIELD(a_clk, AUX_CLK_CLKDIV, 0xA);

		/* Enable and configure aux clock */
		__raw_writel(a_clk, AUX_CLK_REG(pm_aux_clk_num));

		ret = clk_enable(aux_clk);
		if (ret) {
			pr_err("%s %d Unable to enable aux_clk %s\n"
							, __func__, __LINE__
						, aux_clk_name[pm_aux_clk_num]);
			return PM_NO_AUX_CLK;
		}

		aux_clk_ptr[pm_aux_clk_num] = aux_clk;

		/* Clear the bit in the usage mask */
		AUX_CLK_USE_MASK &= ~(1 << pm_aux_clk_num);

		pr_debug("Providing aux_clk_%d [0x%x] [0x%x]\n", pm_aux_clk_num,
				__raw_readl(AUX_CLK_REG(pm_aux_clk_num)),
				__raw_readl(AUX_CLK_REG_REQ(pm_aux_clk_num)));

		/* Store the aux clk addres in the RCB */
		rcb_p->mod_base_addr =
				(unsigned __force)AUX_CLK_REG(pm_aux_clk_num);
		params->pm_aux_clk_counter++;
	} else {
		pr_err("%s %d Error providing aux_clk %d\n", __func__, __LINE__
							   , pm_aux_clk_num);
		return PM_NO_AUX_CLK;
	}

	return PM_SUCCESS;
}

/*
  Request sys m3 on behalf of an IPU client
 *
 */
static inline int ipu_pm_get_sys_m3(struct ipu_pm_object *handle,
				    struct rcb_block *rcb_p,
				    struct ipu_pm_params *params)
{
	if (params->pm_sys_m3_counter) {
		pr_err("%s %d SYS_M3 already requested\n", __func__, __LINE__);
		return PM_UNSUPPORTED;
	}

	params->pm_sys_m3_counter++;
	pr_debug("Request SYS M3\n");

	return PM_SUCCESS;
}

/*
  Request app m3 on behalf of an IPU client
 *
 */
static inline int ipu_pm_get_app_m3(struct ipu_pm_object *handle,
				    struct rcb_block *rcb_p,
				    struct ipu_pm_params *params)
{
	if (params->pm_app_m3_counter) {
		pr_err("%s %d APP_M3 already requested\n", __func__, __LINE__);
		return PM_UNSUPPORTED;
	}

	params->pm_app_m3_counter++;
	pr_debug("Request APP M3\n");

	return PM_SUCCESS;
}

/*
  Request L3 Bus on behalf of an IPU client
 *
 */
static inline int ipu_pm_get_l3_bus(struct ipu_pm_object *handle,
				    struct rcb_block *rcb_p,
				    struct ipu_pm_params *params)
{
	if (params->pm_l3_bus_counter) {
		pr_err("%s %d L3_BUS already requested\n", __func__, __LINE__);
		return PM_UNSUPPORTED;
	}

	params->pm_l3_bus_counter++;
	pr_debug("Request L3 BUS\n");

	return PM_SUCCESS;
}

/*
  Request IVA HD on behalf of an IPU client
 *
 */
static inline int ipu_pm_get_iva_hd(struct ipu_pm_object *handle,
				    struct rcb_block *rcb_p,
				    struct ipu_pm_params *params)
{
	int retval;

	if (params->pm_iva_hd_counter) {
		pr_err("%s %d IVA_HD already requested\n", __func__, __LINE__);
		return PM_UNSUPPORTED;
	}

	retval = ipu_pm_module_start(rcb_p->sub_type);
	if (retval) {
		pr_err("%s %d Error requesting IVA_HD\n", __func__, __LINE__);
		return PM_UNSUPPORTED;
	}
	/* set Next State to INACTIVE for IVAHD */
	prm_write_mod_reg(0xff0e02,
			 OMAP4430_PRM_IVAHD_MOD,
			 OMAP4_PM_IVAHD_PWRSTCTRL_OFFSET);

	params->pm_iva_hd_counter++;
	pr_debug("Request IVA_HD\n");

#ifdef CONFIG_OMAP_PM
	pr_debug("Request MPU wakeup latency\n");
	retval = omap_pm_set_max_mpu_wakeup_lat(&pm_qos_handle,
						IPU_PM_MM_MPU_LAT_CONSTRAINT);
	if (retval) {
		pr_err("%s %d Error setting MPU cstr\n", __func__, __LINE__);
		return PM_UNSUPPORTED;
	}
#endif

	return PM_SUCCESS;
}

/*
  Request FDIF on behalf of an IPU client
 *
 */
static inline int ipu_pm_get_fdif(struct ipu_pm_object *handle,
				  struct rcb_block *rcb_p,
				  struct ipu_pm_params *params)
{
	int retval;

	if (params->pm_fdif_counter) {
		pr_err("%s %d FDIF already requested\n", __func__, __LINE__);
		return PM_UNSUPPORTED;
	}

	retval = ipu_pm_module_start(rcb_p->sub_type);
	if (retval) {
		pr_err("%s %d Error requesting FDIF\n", __func__, __LINE__);
		return PM_UNSUPPORTED;
	}
	params->pm_fdif_counter++;
	pr_debug("Request FDIF\n");

	return PM_SUCCESS;
}

/*
  Request MPU on behalf of an IPU client
 *
 */
static inline int ipu_pm_get_mpu(struct ipu_pm_object *handle,
				 struct rcb_block *rcb_p,
				 struct ipu_pm_params *params)
{
	if (params->pm_mpu_counter) {
		pr_err("%s %d MPU already requested\n", __func__, __LINE__);
		return PM_UNSUPPORTED;
	}

	params->pm_mpu_counter++;
	pr_debug("Request MPU\n");

	return PM_SUCCESS;
}

/*
  Request IPU on behalf of an IPU client
 *
 */
static inline int ipu_pm_get_ipu(struct ipu_pm_object *handle,
				 struct rcb_block *rcb_p,
				 struct ipu_pm_params *params)
{
	if (params->pm_ipu_counter) {
		pr_err("%s %d IPU already requested\n", __func__, __LINE__);
		return PM_UNSUPPORTED;
	}

	params->pm_ipu_counter++;
	pr_debug("Request IPU\n");

	return PM_SUCCESS;
}

/*
  Request IVA SEQ0 on behalf of an IPU client
 *
 */
static inline int ipu_pm_get_ivaseq0(struct ipu_pm_object *handle,
				     struct rcb_block *rcb_p,
				     struct ipu_pm_params *params)
{
	int retval;

	if (params->pm_ivaseq0_counter) {
		pr_err("%s %d IVASEQ0 already requested\n", __func__, __LINE__);
		return PM_UNSUPPORTED;
	}

	retval = ipu_pm_module_start(rcb_p->sub_type);
	if (retval) {
		pr_err("%s %d Error requesting IVASEQ0\n", __func__, __LINE__);
		return PM_UNSUPPORTED;
	}
	params->pm_ivaseq0_counter++;
	pr_debug("Request IVASEQ0\n");

	return PM_SUCCESS;
}

/*
  Request IVA SEQ1 on behalf of an IPU client
 *
 */
static inline int ipu_pm_get_ivaseq1(struct ipu_pm_object *handle,
				     struct rcb_block *rcb_p,
				     struct ipu_pm_params *params)
{
	int retval;

	if (params->pm_ivaseq1_counter) {
		pr_err("%s %d IVASEQ1 already requested\n", __func__, __LINE__);
		return PM_UNSUPPORTED;
	}

	retval = ipu_pm_module_start(rcb_p->sub_type);
	if (retval) {
		pr_err("%s %d Error requesting IVASEQ1\n", __func__, __LINE__);
		return PM_UNSUPPORTED;
	}
	params->pm_ivaseq1_counter++;
	pr_debug("Request IVASEQ1\n");

	/*Requesting SL2*/
	/* FIXME: sl2if should be moved to a independent function */
	retval = ipu_pm_module_start(SL2_RESOURCE);
	if (retval) {
		pr_err("%s %d Error requesting sl2if\n", __func__, __LINE__);
		return PM_UNSUPPORTED;
	}
	params->pm_sl2if_counter++;
	pr_debug("Request sl2if\n");

	return PM_SUCCESS;
}

/*
  Request ISS on behalf of an IPU client
 *
 */
static inline int ipu_pm_get_iss(struct ipu_pm_object *handle,
				 struct rcb_block *rcb_p,
				 struct ipu_pm_params *params)
{
	int retval;
	struct omap_ipupm_mod_platform_data *pd = ipupm_get_plat_data();

	if (params->pm_iss_counter) {
		pr_err("%s %d ISS already requested\n", __func__, __LINE__);
		return PM_UNSUPPORTED;
	}

#if TMP_AUX_CLK_HACK
	rcb_p->fill9 = AUX_CLK_MIN;
	ipu_pm_get_aux_clk(handle, rcb_p, params);
#endif

	retval = ipu_pm_module_start(rcb_p->sub_type);
	if (retval) {
		pr_err("%s %d Error requesting ISS\n", __func__, __LINE__);
		return PM_UNSUPPORTED;
	}

	clk_opt_iss = clk_get(pd[rcb_p->sub_type].dev, "ctrlclk");
	if (!clk_opt_iss) {
		pr_err("%s %d failed to get the iss ctrl clock\n",
						__func__, __LINE__);
		return PM_UNSUPPORTED;
	}

	retval = clk_enable(clk_opt_iss);
	if (retval != 0) {
		pr_err("%s %d failed to enable the iss opt clock\n",
							__func__, __LINE__);
		return PM_UNSUPPORTED;
	}

#ifdef CONFIG_OMAP_PM
	pr_debug("Request MPU wakeup latency\n");
	retval = omap_pm_set_max_mpu_wakeup_lat(&pm_qos_handle,
						IPU_PM_MM_MPU_LAT_CONSTRAINT);
	if (retval) {
		pr_err("%s %d Error setting MPU cstr\n", __func__, __LINE__);
		return PM_UNSUPPORTED;
	}
#endif

	params->pm_iss_counter++;
	pr_debug("Request ISS\n");

	return PM_SUCCESS;
}

/*
  Release a sdma on behalf of an IPU client
 *
 */
static inline int ipu_pm_rel_sdma_chan(struct ipu_pm_object *handle,
				       struct rcb_block *rcb_p,
				       struct ipu_pm_params *params)
{
	int pm_sdmachan_num;
	int pm_sdmachan_dummy;
	int ch;

	/* Release resource using PRCM API */
	pm_sdmachan_num = rcb_p->num_chan;
	for (ch = 0; ch < pm_sdmachan_num; ch++) {
		pm_sdmachan_dummy = (int)rcb_p->channels[ch];
		omap_free_dma(pm_sdmachan_dummy);
		params->pm_sdmachan_counter--;
		pr_debug("Releasing sdma ch %d\n", pm_sdmachan_dummy);
	}
	return PM_SUCCESS;
}

/*
  Release a gptimer on behalf of an IPU client
 *
 */
static inline int ipu_pm_rel_gptimer(struct ipu_pm_object *handle,
				     struct rcb_block *rcb_p,
				     struct ipu_pm_params *params)
{
	struct omap_dm_timer *p_gpt;
	int pm_gptimer_num;

	p_gpt = (struct omap_dm_timer *)rcb_p->mod_base_addr;
	pm_gptimer_num = rcb_p->fill9;

	/* Check the usage mask */
	if (GPTIMER_USE_MASK & (1 << pm_gptimer_num)) {
		pr_err("%s %d Invalid gptimer %d\n", __func__, __LINE__
						   , pm_gptimer_num);
		return PM_NO_GPTIMER;
	}

	/* Set the usage mask for reuse */
	GPTIMER_USE_MASK |= (1 << pm_gptimer_num);

	/* Release resource using PRCM API */
	if (!p_gpt) {
		pr_err("%s %d Null gptimer\n", __func__, __LINE__);
		return PM_NO_GPTIMER;
	}
	omap_dm_timer_free(p_gpt);
	rcb_p->mod_base_addr = 0;
	params->pm_gptimer_counter--;
	pr_debug("Releasing gptimer %d\n", pm_gptimer_num);
	return PM_SUCCESS;
}

/*
  Release an i2c buses on behalf of an IPU client
 *
 */
static inline int ipu_pm_rel_i2c_bus(struct ipu_pm_object *handle,
				     struct rcb_block *rcb_p,
				     struct ipu_pm_params *params)
{
	struct clk *p_i2c_clk;
	int pm_i2c_bus_num;

	pm_i2c_bus_num = rcb_p->fill9;
	p_i2c_clk = (struct clk *)rcb_p->mod_base_addr;

	/* Check the usage mask */
	if (I2C_USE_MASK & (1 << pm_i2c_bus_num)) {
		pr_err("%s %d Invalid i2c_%d\n", __func__, __LINE__
					       , pm_i2c_bus_num);
		return PM_NO_I2C;
	}

	if (!p_i2c_clk) {
		pr_err("%s %d Null i2c_%d\n", __func__, __LINE__
					    , pm_i2c_bus_num);
		return PM_NO_I2C;
	}

	/* Release resource using PRCM API */
	clk_disable(p_i2c_clk);
	rcb_p->mod_base_addr = 0;

	/* Set the usage mask for reuse */
	I2C_USE_MASK |= (1 << pm_i2c_bus_num);

	params->pm_i2c_bus_counter--;
	pr_debug("Releasing i2c_%d\n", pm_i2c_bus_num);

	return PM_SUCCESS;
}

/*
  Release a gpio on behalf of an IPU client
 *
 */
static inline int ipu_pm_rel_gpio(struct ipu_pm_object *handle,
				  struct rcb_block *rcb_p,
				  struct ipu_pm_params *params)
{
	int pm_gpio_num;

	if (!params->pm_gpio_counter) {
		pr_err("%s %d No gpio requested\n", __func__, __LINE__);
		return PM_NO_GPIO;
	}

	pm_gpio_num = rcb_p->fill9;
	gpio_free(pm_gpio_num);
	params->pm_gpio_counter--;
	pr_debug("Releasing gpio %d\n", pm_gpio_num);

	return PM_SUCCESS;
}

/*
  Release a regulator on behalf of an IPU client
 *
 */
static inline int ipu_pm_rel_regulator(struct ipu_pm_object *handle,
				       struct rcb_block *rcb_p,
				       struct ipu_pm_params *params)
{
	struct regulator *p_regulator = NULL;
	int retval = 0;

	/* Get the regulator */
	p_regulator = (struct regulator *)rcb_p->mod_base_addr;

	if (!p_regulator) {
		pr_err("%s %d Null regulator\n", __func__, __LINE__);
		return PM_INVAL_REGULATOR;
	}

	/* Restart the voltage to the default value */
	retval = regulator_set_voltage(p_regulator, cam2_prev_volt,
					cam2_prev_volt);
	if (retval) {
		pr_err("%s %d Error restoring voltage\n", __func__, __LINE__);
		return PM_INVAL_REGULATOR;
	}

	/* Release resource using PRCM API */
	regulator_put(p_regulator);

	rcb_p->mod_base_addr = 0;
	params->pm_regulator_counter--;
	pr_debug("Releasing regulator\n");

	return PM_SUCCESS;
}

/*
  Release an Aux clk on behalf of an IPU client
 *
 */
static inline int ipu_pm_rel_aux_clk(struct ipu_pm_object *handle,
				     struct rcb_block *rcb_p,
				     struct ipu_pm_params *params)
{
	struct clk *aux_clk;
	int pm_aux_clk_num;

	pm_aux_clk_num = rcb_p->fill9;

	/* Check the usage mask */
	if (AUX_CLK_USE_MASK & (1 << pm_aux_clk_num)) {
		pr_err("%s %d Invalid aux_clk_%d\n", __func__, __LINE__
						   , pm_aux_clk_num);
		return PM_INVAL_AUX_CLK;
	}

	aux_clk = aux_clk_ptr[pm_aux_clk_num];
	if (!aux_clk) {
		pr_err("%s %d Null aux_clk %s\n", __func__, __LINE__
						, aux_clk_name[pm_aux_clk_num]);
		return PM_INVAL_AUX_CLK;
	}
	clk_disable(aux_clk);

	aux_clk_ptr[pm_aux_clk_num] = 0;

	/* Set the usage mask for reuse */
	AUX_CLK_USE_MASK |= (1 << pm_aux_clk_num);

	rcb_p->mod_base_addr = 0;
	params->pm_aux_clk_counter--;
	pr_debug("Releasing aux_clk_%d\n", pm_aux_clk_num);

	return PM_SUCCESS;
}

/*
  Release sys m3 on behalf of an IPU client
 *
 */
static inline int ipu_pm_rel_sys_m3(struct ipu_pm_object *handle,
				    struct rcb_block *rcb_p,
				    struct ipu_pm_params *params)
{
	if (!params->pm_sys_m3_counter) {
		pr_err("%s %d SYSM3 not requested\n", __func__, __LINE__);
		goto error;
	}

	params->pm_sys_m3_counter--;
	pr_debug("Release SYS M3\n");

	return PM_SUCCESS;
error:
	return PM_UNSUPPORTED;
}

/*
  Release app m3 on behalf of an IPU client
 *
 */
static inline int ipu_pm_rel_app_m3(struct ipu_pm_object *handle,
				    struct rcb_block *rcb_p,
				    struct ipu_pm_params *params)
{
	if (!params->pm_app_m3_counter) {
		pr_err("%s %d APPM3 not requested\n", __func__, __LINE__);
		goto error;
	}

	params->pm_app_m3_counter--;
	pr_debug("Release APP M3\n");

	return PM_SUCCESS;
error:
	return PM_UNSUPPORTED;
}

/*
  Release L3 Bus on behalf of an IPU client
 *
 */
static inline int ipu_pm_rel_l3_bus(struct ipu_pm_object *handle,
				    struct rcb_block *rcb_p,
				    struct ipu_pm_params *params)
{
	if (!params->pm_l3_bus_counter) {
		pr_err("%s %d L3_BUS not requested\n", __func__, __LINE__);
		goto error;
	}

	params->pm_l3_bus_counter--;
	pr_debug("Release L3 BUS\n");

	return PM_SUCCESS;
error:
	return PM_UNSUPPORTED;
}

/*
  Release FDIF on behalf of an IPU client
 *
 */
static inline int ipu_pm_rel_fdif(struct ipu_pm_object *handle,
				  struct rcb_block *rcb_p,
				  struct ipu_pm_params *params)
{
	int retval;

	if (!params->pm_fdif_counter) {
		pr_err("%s %d FDIF not requested\n", __func__, __LINE__);
		goto error;
	}

	retval = ipu_pm_module_stop(rcb_p->sub_type);
	if (retval)
		return PM_UNSUPPORTED;

#ifdef SR_WA
	/* Make sure the clock domain is in idle if not softreset */
	if ((cm_read_mod_reg(OMAP4430_CM2_CAM_MOD,
			OMAP4_CM_CAM_CLKSTCTRL_OFFSET)) & 0x400) {
		__raw_writel(__raw_readl(fdifHandle + 0x10) | 0x1,
							fdifHandle + 0x10);
	}
#endif

	params->pm_fdif_counter--;
	pr_debug("Release FDIF\n");

	return PM_SUCCESS;
error:
	return PM_UNSUPPORTED;
}

/*
  Release MPU on behalf of an IPU client
 *
 */
static inline int ipu_pm_rel_mpu(struct ipu_pm_object *handle,
				 struct rcb_block *rcb_p,
				 struct ipu_pm_params *params)
{
	if (!params->pm_mpu_counter) {
		pr_err("%s %d MPU not requested\n", __func__, __LINE__);
		goto error;
	}

	params->pm_mpu_counter--;
	pr_debug("Release MPU\n");

	return PM_SUCCESS;
error:
	return PM_UNSUPPORTED;
}

/*
  Release IPU on behalf of an IPU client
 *
 */
static inline int ipu_pm_rel_ipu(struct ipu_pm_object *handle,
				 struct rcb_block *rcb_p,
				 struct ipu_pm_params *params)
{
	if (!params->pm_ipu_counter) {
		pr_err("%s %d IPU not requested\n", __func__, __LINE__);
		goto error;
	}

	params->pm_ipu_counter--;
	pr_debug("Release IPU\n");

	return PM_SUCCESS;
error:
	return PM_UNSUPPORTED;
}

/*
  Release IVA HD on behalf of an IPU client
 *
 */
static inline int ipu_pm_rel_iva_hd(struct ipu_pm_object *handle,
				    struct rcb_block *rcb_p,
				    struct ipu_pm_params *params)
{
	int retval;

	if (!params->pm_iva_hd_counter) {
		pr_err("%s %d IVA_HD not requested\n", __func__, __LINE__);
		goto error;
	}

	/* Releasing SL2 */
	/* FIXME: sl2if should be moved to a independent function */
	if (params->pm_sl2if_counter) {
		retval = ipu_pm_module_stop(SL2_RESOURCE);
		if (retval) {
			pr_err("%s %d Error releasing sl2if\n"
							, __func__, __LINE__);
			return PM_UNSUPPORTED;
		}
		params->pm_sl2if_counter--;
		pr_debug("Release SL2IF\n");
	}

	retval = ipu_pm_module_stop(rcb_p->sub_type);
	if (retval) {
		pr_err("%s %d Error releasing IVA_HD\n", __func__, __LINE__);
		return PM_UNSUPPORTED;
	}
	params->pm_iva_hd_counter--;
	pr_debug("Release IVA_HD\n");

#ifdef CONFIG_OMAP_PM
	if (params->pm_iva_hd_counter == 0 && params->pm_iss_counter == 0) {
		pr_debug("Release MPU wakeup latency\n");
		retval = omap_pm_set_max_mpu_wakeup_lat(&pm_qos_handle,
						IPU_PM_NO_MPU_LAT_CONSTRAINT);
		if (retval) {
			pr_err("%s %d Error setting MPU cstr\n"
							, __func__, __LINE__);
			goto error;
		}
	}
#endif
	return PM_SUCCESS;
error:
	return PM_UNSUPPORTED;
}

/*
  Release IVA SEQ0 on behalf of an IPU client
 *
 */
static inline int ipu_pm_rel_ivaseq0(struct ipu_pm_object *handle,
				     struct rcb_block *rcb_p,
				     struct ipu_pm_params *params)
{
	int retval;

	if (!params->pm_ivaseq0_counter) {
		pr_err("%s %d IVASEQ0 not requested\n", __func__, __LINE__);
		goto error;
	}

	retval = ipu_pm_module_stop(rcb_p->sub_type);
	if (retval) {
		pr_err("%s %d Error releasing IVASEQ0\n", __func__, __LINE__);
		return PM_UNSUPPORTED;
	}
	params->pm_ivaseq0_counter--;
	pr_debug("Release IVASEQ0\n");

	return PM_SUCCESS;
error:
	return PM_UNSUPPORTED;
}

/*
  Release IVA SEQ1 on behalf of an IPU client
 *
 */
static inline int ipu_pm_rel_ivaseq1(struct ipu_pm_object *handle,
				     struct rcb_block *rcb_p,
				     struct ipu_pm_params *params)
{
	int retval;

	if (!params->pm_ivaseq1_counter) {
		pr_err("%s %d IVASEQ1 not requested\n", __func__, __LINE__);
		goto error;
	}

	retval = ipu_pm_module_stop(rcb_p->sub_type);
	if (retval) {
		pr_err("%s %d Error releasing IVASEQ1\n", __func__, __LINE__);
		return PM_UNSUPPORTED;
	}
	params->pm_ivaseq1_counter--;
	pr_debug("Release IVASEQ1\n");

	return PM_SUCCESS;
error:
	return PM_UNSUPPORTED;
}

/*
  Release ISS on behalf of an IPU client
 *
 */
static inline int ipu_pm_rel_iss(struct ipu_pm_object *handle,
				 struct rcb_block *rcb_p,
				 struct ipu_pm_params *params)
{
	int retval;

	if (!params->pm_iss_counter) {
		pr_err("%s %d ISS not requested\n", __func__, __LINE__);
		goto error;
	}

#if TMP_AUX_CLK_HACK
	rcb_p->fill9 = AUX_CLK_MIN;
	ipu_pm_rel_aux_clk(handle, rcb_p, params);
#endif

	retval = ipu_pm_module_stop(rcb_p->sub_type);
	if (retval) {
		pr_err("%s %d Error releasing ISS\n", __func__, __LINE__);
		return PM_UNSUPPORTED;
	}

#ifdef SR_WA
	/* Make sure the clock domain is in idle if not softreset */
	if ((cm_read_mod_reg(OMAP4430_CM2_CAM_MOD,
			OMAP4_CM_CAM_CLKSTCTRL_OFFSET)) & 0x100) {
		__raw_writel(__raw_readl(issHandle + 0x10) | 0x1,
							issHandle + 0x10);
	}
#endif
	if (clk_opt_iss) {
		clk_disable(clk_opt_iss);
		clk_put(clk_opt_iss);
	}

	params->pm_iss_counter--;
	pr_debug("Release ISS\n");

#ifdef CONFIG_OMAP_PM
	if (params->pm_iva_hd_counter == 0 && params->pm_iss_counter == 0) {
		pr_debug("Release MPU wakeup latency\n");
		retval = omap_pm_set_max_mpu_wakeup_lat(&pm_qos_handle,
						IPU_PM_NO_MPU_LAT_CONSTRAINT);
		if (retval) {
			pr_err("%s %d Error setting MPU cstr\n"
							, __func__, __LINE__);
			goto error;
		}
	}
#endif
	return PM_SUCCESS;
error:
	return PM_UNSUPPORTED;
}

/*
  Request a FDIF constraint on behalf of an IPU client
 *
 */
static inline int ipu_pm_req_cstr(struct ipu_pm_object *handle,
				  struct rcb_block *rcb_p,
				  struct ipu_pm_params *params)
{
	int perf;
	int lat;
	int bw;
	int retval;
	unsigned temp_rsrc;
	u32 cstr_flags;

	/* Get the configurable constraints */
	cstr_flags = rcb_p->data[0];

	/* TODO: call the baseport APIs */
	if (cstr_flags & PM_CSTR_PERF_MASK) {
		switch (rcb_p->sub_type) {
		case MPU:
			temp_rsrc = IPU_PM_MPU;
			break;
		case IPU:
		case ISS:
		case FDIF:
			temp_rsrc = IPU_PM_CORE;
			break;
		default:
			temp_rsrc = rcb_p->sub_type;
			break;
		}
		perf = rcb_p->data[1];
		pr_debug("Request perfomance Cstr %d\n", perf);
		retval = ipu_pm_module_set_rate(rcb_p->sub_type,
						temp_rsrc,
						perf);
		if (retval)
			return PM_UNSUPPORTED;
	}

	if (cstr_flags & PM_CSTR_LAT_MASK) {
		switch (rcb_p->sub_type) {
		case MPU:
			temp_rsrc = IPU_PM_MPU;
			break;
		case IPU:
		case L3_BUS:
			temp_rsrc = IPU_PM_CORE;
			break;
		case ISS:
		case FDIF:
			temp_rsrc = IPU_PM_SELF;
			break;
		default:
			temp_rsrc = rcb_p->sub_type;
			break;
		}
		lat = rcb_p->data[2];
		pr_debug("Request latency Cstr %d\n", lat);
		retval = ipu_pm_module_set_latency(rcb_p->sub_type,
						   temp_rsrc,
						   lat);
		if (retval)
			return PM_UNSUPPORTED;
	}

	if (cstr_flags & PM_CSTR_BW_MASK) {
		bw = rcb_p->data[3];
		pr_debug("Request bandwidth Cstr L3bus:%d\n", bw);
		retval = ipu_pm_module_set_bandwidth(rcb_p->sub_type,
						     rcb_p->sub_type,
						     bw);
		if (retval)
			return PM_UNSUPPORTED;
	}

	return PM_SUCCESS;
}

/*
  Release a constraint on behalf of an IPU client
 *
 */
static inline int ipu_pm_rel_cstr(struct ipu_pm_object *handle,
				  struct rcb_block *rcb_p,
				  struct ipu_pm_params *params)
{
	u32 cstr_flags;
	unsigned temp_rsrc;
	int retval;

	/* Get the configurable constraints */
	cstr_flags = rcb_p->data[0];

	/* TODO: call the baseport APIs */
	if (cstr_flags & PM_CSTR_PERF_MASK) {
		switch (rcb_p->sub_type) {
		case MPU:
			temp_rsrc = IPU_PM_MPU;
			break;
		case IPU:
		case ISS:
		case FDIF:
			temp_rsrc = IPU_PM_CORE;
			break;
		default:
			temp_rsrc = rcb_p->sub_type;
			break;
		}
		pr_debug("Release perfomance Cstr\n");
		retval = ipu_pm_module_set_rate(rcb_p->sub_type,
						temp_rsrc,
						NO_FREQ_CONSTRAINT);
		if (retval)
			return PM_UNSUPPORTED;
	}

	if (cstr_flags & PM_CSTR_LAT_MASK) {
		switch (rcb_p->sub_type) {
		case MPU:
			temp_rsrc = IPU_PM_MPU;
			break;
		case IPU:
		case L3_BUS:
			temp_rsrc = IPU_PM_CORE;
			break;
		case ISS:
		case FDIF:
			temp_rsrc = IPU_PM_SELF;
			break;
		default:
			temp_rsrc = rcb_p->sub_type;
			break;
		}
		pr_debug("Release latency Cstr\n");
		retval = ipu_pm_module_set_latency(rcb_p->sub_type,
						   temp_rsrc,
						   NO_LAT_CONSTRAINT);
		if (retval)
			return PM_UNSUPPORTED;
	}

	if (cstr_flags & PM_CSTR_BW_MASK) {
		pr_debug("Release bandwidth Cstr\n");
		retval = ipu_pm_module_set_bandwidth(rcb_p->sub_type,
						     rcb_p->sub_type,
						     NO_BW_CONSTRAINT);
		if (retval)
			return PM_UNSUPPORTED;
	}

	return PM_SUCCESS;
}

/*
  Function to set init parameters
 *
 */
void ipu_pm_params_init(struct ipu_pm_params *params)
{
	int retval = 0;

	if (WARN_ON(unlikely(params == NULL))) {
		retval = -EINVAL;
		goto exit;
	}

	memcpy(params, &(pm_params), sizeof(struct ipu_pm_params));
	return;
exit:
	pr_err("ipu_pm_params_init failed status(0x%x)\n", retval);
}
EXPORT_SYMBOL(ipu_pm_params_init);

/*
  Function to calculate ipu_pm mem required
 *
 */
int ipu_pm_mem_req(const struct ipu_pm_params *params)
{
	/* Memory required for ipu pm module */
	/* FIXME: Maybe more than this is needed */
	return sizeof(struct sms);
}
EXPORT_SYMBOL(ipu_pm_mem_req);

/*
  Function to register events
  This function will register the events needed for ipu_pm
  the events reserved for power management are 2 and 3
  both sysm3 and appm3 will use the same events.
 */
int ipu_pm_init_transport(struct ipu_pm_object *handle)
{
	int retval = 0;
	struct ipu_pm_params *params;

	if (WARN_ON(unlikely(handle == NULL))) {
		retval = -EINVAL;
		goto pm_register_fail;
	}

	params = handle->params;
	if (WARN_ON(unlikely(params == NULL))) {
		retval = -EINVAL;
		goto pm_register_fail;
	}

	retval = notify_register_event(
		params->remote_proc_id,
		params->line_id,
		params->pm_resource_event | \
				(NOTIFY_SYSTEMKEY << 16),
		(notify_fn_notify_cbck)ipu_pm_callback,
		(void *)NULL);
	if (retval < 0)
		goto pm_register_fail;

	retval = notify_register_event(
		params->remote_proc_id,
		params->line_id,
		params->pm_notification_event | \
				(NOTIFY_SYSTEMKEY << 16),
		(notify_fn_notify_cbck)ipu_pm_notify_callback,
		(void *)NULL);

	if (retval < 0) {
		retval = notify_unregister_event(
		params->remote_proc_id,
		params->line_id,
		params->pm_resource_event | \
				(NOTIFY_SYSTEMKEY << 16),
		(notify_fn_notify_cbck)ipu_pm_callback,
		(void *)NULL);
		if (retval < 0)
			pr_err("Error unregistering notify event\n");
		goto pm_register_fail;
	}
	return retval;

pm_register_fail:
	pr_err("pm register events failed status(0x%x)", retval);
	return retval;
}

/*
  Function to create ipu pm object
 *
 */
struct ipu_pm_object *ipu_pm_create(const struct ipu_pm_params *params)
{
	int i;
	int retval = 0;

	if (WARN_ON(unlikely(params == NULL))) {
		retval = -EINVAL;
		goto exit;
	}

	if (params->remote_proc_id == SYS_M3) {
		pm_handle_sysm3 = kmalloc(sizeof(struct ipu_pm_object),
						GFP_ATOMIC);

		if (WARN_ON(unlikely(pm_handle_sysm3 == NULL))) {
			retval = -EINVAL;
			goto exit;
		}

		pm_handle_sysm3->rcb_table = (struct sms *)params->shared_addr;

		pm_handle_sysm3->pm_event = kzalloc(sizeof(struct pm_event)
					* params->pm_num_events, GFP_KERNEL);

		if (WARN_ON(unlikely(pm_handle_sysm3->pm_event == NULL))) {
			retval = -EINVAL;
			kfree(pm_handle_sysm3);
			goto exit;
		}

		/* Each event has it own sem */
		for (i = 0; i < params->pm_num_events; i++) {
			sema_init(&pm_handle_sysm3->pm_event[i].sem_handle, 0);
			pm_handle_sysm3->pm_event[i].event_type = i;
		}

		pm_handle_sysm3->params = kzalloc(sizeof(struct ipu_pm_params)
							, GFP_KERNEL);

		if (WARN_ON(unlikely(pm_handle_sysm3->params == NULL))) {
			retval = -EINVAL;
			kfree(pm_handle_sysm3->pm_event);
			kfree(pm_handle_sysm3);
			goto exit;
		}

		memcpy(pm_handle_sysm3->params, params,
			sizeof(struct ipu_pm_params));

		/* Check the SW version on both sides */
		if (WARN_ON(pm_handle_sysm3->rcb_table->pm_version !=
						PM_VERSION))
			pr_warning("Mismatch in PM version Host:0x%08x "
					"Remote:0x%08x", PM_VERSION,
					pm_handle_sysm3->rcb_table->pm_version);

		spin_lock_init(&pm_handle_sysm3->lock);
		INIT_WORK(&pm_handle_sysm3->work, ipu_pm_work);

		if (!kfifo_alloc(&pm_handle_sysm3->fifo,
				IPU_KFIFO_SIZE * sizeof(struct ipu_pm_msg),
				GFP_KERNEL))
			return pm_handle_sysm3;

		retval = -ENOMEM;
		kfree(pm_handle_sysm3->params);
		kfree(pm_handle_sysm3->pm_event);
		kfree(pm_handle_sysm3);
	} else if (params->remote_proc_id == APP_M3) {
		pm_handle_appm3 = kmalloc(sizeof(struct ipu_pm_object),
						GFP_ATOMIC);

		if (WARN_ON(unlikely(pm_handle_appm3 == NULL))) {
			retval = -EINVAL;
			goto exit;
		}

		pm_handle_appm3->rcb_table = (struct sms *)params->shared_addr;

		pm_handle_appm3->pm_event = kzalloc(sizeof(struct pm_event)
					* params->pm_num_events, GFP_KERNEL);

		if (WARN_ON(unlikely(pm_handle_appm3->pm_event == NULL))) {
			retval = -EINVAL;
			kfree(pm_handle_appm3);
			goto exit;
		}

		/* Each event has it own sem */
		for (i = 0; i < params->pm_num_events; i++) {
			sema_init(&pm_handle_appm3->pm_event[i].sem_handle, 0);
			pm_handle_appm3->pm_event[i].event_type = i;
		}

		pm_handle_appm3->params = kzalloc(sizeof(struct ipu_pm_params)
						, GFP_KERNEL);

		if (WARN_ON(unlikely(pm_handle_appm3->params == NULL))) {
			retval = -EINVAL;
			kfree(pm_handle_appm3->pm_event);
			kfree(pm_handle_appm3);
			goto exit;
		}

		memcpy(pm_handle_appm3->params, params,
			sizeof(struct ipu_pm_params));

		/* Check the SW version on both sides */
		if (WARN_ON(pm_handle_appm3->rcb_table->pm_version !=
						PM_VERSION))
			pr_warning("Mismatch in PM version Host:0x%08x "
					"Remote:0x%08x", PM_VERSION,
					pm_handle_appm3->rcb_table->pm_version);

		spin_lock_init(&pm_handle_appm3->lock);
		INIT_WORK(&pm_handle_appm3->work, ipu_pm_work);

		if (!kfifo_alloc(&pm_handle_appm3->fifo,
				IPU_KFIFO_SIZE * sizeof(struct ipu_pm_msg),
				GFP_KERNEL))
			return pm_handle_appm3;

		retval = -ENOMEM;
		kfree(pm_handle_appm3->params);
		kfree(pm_handle_appm3->pm_event);
		kfree(pm_handle_appm3);
	} else
		retval = -EINVAL;

exit:
	pr_err("ipu_pm_create failed! status = 0x%x\n", retval);
	return NULL;
}

/*
  Function to delete ipu pm object
 *
 */
void ipu_pm_delete(struct ipu_pm_object *handle)
{
	int retval = 0;
	struct ipu_pm_params *params;

	if (WARN_ON(unlikely(handle == NULL))) {
		retval = -EINVAL;
		goto exit;
	}

	params = handle->params;
	if (WARN_ON(unlikely(params == NULL))) {
		retval = -EINVAL;
		goto exit;
	}

	/* Release the shared RCB */
	handle->rcb_table = NULL;

	kfree(handle->pm_event);
	if (params->remote_proc_id == SYS_M3)
		pm_handle_sysm3 = NULL;
	else
		pm_handle_appm3 = NULL;
	kfree(handle->params);
	kfree(handle);
	return;
exit:
	pr_err("ipu_pm_delete is already NULL status = 0x%x\n", retval);
}

/*
  Function to get the ducati state flag from Share memory
 *
 */
u32 ipu_pm_get_state(int proc_id)
{
	struct ipu_pm_object *handle;

	/* get the handle to proper ipu pm object */
	handle = ipu_pm_get_handle(proc_id);
	if (WARN_ON(unlikely(handle == NULL)))
		return -EINVAL;

	return handle->rcb_table->state_flag;
}
EXPORT_SYMBOL(ipu_pm_get_state);

/*
  Function to get ipu pm object
 *
 */
struct ipu_pm_object *ipu_pm_get_handle(int proc_id)
{
	if (proc_id == SYS_M3)
		return pm_handle_sysm3;
	else if (proc_id == APP_M3)
		return pm_handle_appm3;
	else
		return NULL;
}
EXPORT_SYMBOL(ipu_pm_get_handle);

/*
  Function to save a processor context and send it to hibernate
 *
 */
int ipu_pm_save_ctx(int proc_id)
{
	int retval = 0;
	int flag;
	int num_loaded_cores = 0;
	int sys_loaded;
	int app_loaded;
	unsigned long timeout;
	struct ipu_pm_object *handle;

	/* get the handle to proper ipu pm object */
	handle = ipu_pm_get_handle(proc_id);
	if (unlikely(handle == NULL))
		return 0;

	/* get M3's load flag */
	sys_loaded = (ipu_pm_get_state(proc_id) & SYS_PROC_LOADED) >>
								PROC_LD_SHIFT;
	app_loaded = (ipu_pm_get_state(proc_id) & APP_PROC_LOADED) >>
								PROC_LD_SHIFT;

	/* If already down don't kill it twice */
	if (ipu_pm_get_state(proc_id) & SYS_PROC_DOWN) {
		pr_warn("ipu already hibernated, no need to save again");
		return 0;
	}

#ifdef CONFIG_OMAP_PM
	retval = omap_pm_set_max_sdma_lat(&pm_qos_handle_2,
						IPU_PM_NO_MPU_LAT_CONSTRAINT);
	if (retval)
		pr_info("Unable to remove cstr on IPU\n");
#endif

	/* Because of the current scheme, we need to check
	 * if APPM3 is enable and we need to shut it down too
	 * Sysm3 is the only want sending the hibernate message
	*/
	mutex_lock(ipu_pm_state.gate_handle);
	if (proc_id == SYS_M3 || proc_id == APP_M3) {
		if (!sys_loaded)
			goto exit;

		num_loaded_cores = app_loaded + sys_loaded;

#ifdef CONFIG_SYSLINK_IPU_SELF_HIBERNATION
		/* Turn off timer before hibernation */
		ipu_pm_timer_state(PM_HIB_TIMER_OFF);
#endif

		flag = 1;
		timeout = jiffies + msecs_to_jiffies(WAIT_FOR_IDLE_TIMEOUT);
		/* Wait fot Ducati to hibernate */
		do {
			/* Checking if IPU is really in idle */
			if (NUM_IDLE_CORES == num_loaded_cores) {
				flag = 0;
				break;
			}
		} while (!time_after(jiffies, timeout));
		if (flag)
			goto error;

		/* Check for APPM3, if loaded reset first */
		if (app_loaded) {
			pr_info("Sleep APPM3\n");
			retval = rproc_sleep(app_rproc);
			cm_write_mod_reg(HW_AUTO,
					 OMAP4430_CM2_CORE_MOD,
					 OMAP4_CM_DUCATI_CLKSTCTRL_OFFSET);
			if (retval)
				goto error;
			handle->rcb_table->state_flag |= APP_PROC_DOWN;
		}
		pr_info("Sleep SYSM3\n");
		retval = rproc_sleep(sys_rproc);
		cm_write_mod_reg(HW_AUTO,
				 OMAP4430_CM2_CORE_MOD,
				 OMAP4_CM_DUCATI_CLKSTCTRL_OFFSET);
		if (retval)
			goto error;
		handle->rcb_table->state_flag |= SYS_PROC_DOWN;

		/* If there is a message in the mbox restore
		 * immediately after save.
		 */
		if (ducati_mbox && PENDING_MBOX_MSG)
			goto restore;

		if (ducati_mbox) {
			omap_mbox_save_ctx(ducati_mbox);
			_is_mbox_up = 0;
		} else
			pr_err("Not able to save mbox");
		if (ducati_iommu) {
			iommu_save_ctx(ducati_iommu);
			_is_iommu_up = 0;
		} else
			pr_err("Not able to save iommu");
	} else
		goto error;
exit:
	mutex_unlock(ipu_pm_state.gate_handle);
	return 0;
error:
#ifdef CONFIG_SYSLINK_IPU_SELF_HIBERNATION
	ipu_pm_timer_state(PM_HIB_TIMER_ON);
#endif
	mutex_unlock(ipu_pm_state.gate_handle);
	pr_debug("Aborting hibernation process\n");
	return -EINVAL;
restore:
	pr_debug("Starting restore_ctx since messages pending in mbox\n");
	mutex_unlock(ipu_pm_state.gate_handle);
	ipu_pm_restore_ctx(proc_id);
	return 0;
}
EXPORT_SYMBOL(ipu_pm_save_ctx);

/* Function to check if a processor is shutdown
 * if shutdown then restore context else return.
 */
int ipu_pm_restore_ctx(int proc_id)
{
	int retval = 0;
	int sys_loaded;
	int app_loaded;
	struct ipu_pm_object *handle;

	/*If feature not supported by proc, return*/
	if (!proc_supported(proc_id))
		return 0;

	/* get the handle to proper ipu pm object */
	handle = ipu_pm_get_handle(proc_id);

	if (WARN_ON(unlikely(handle == NULL)))
		return -EINVAL;

	/* FIXME: This needs mor analysis.
	 * Since the sync of IPU and MPU is done this is a safe place
	 * to switch to HW_AUTO to allow transition of clocks to gated
	 * supervised by HW.
	*/
	if (first_time) {
		/* Enable/disable ipu hibernation*/
#ifdef CONFIG_SYSLINK_IPU_SELF_HIBERNATION
		handle->rcb_table->pm_flags.hibernateAllowed = 1;
		/* turn on ducati hibernation timer */
		ipu_pm_timer_state(PM_HIB_TIMER_ON);
#else
		handle->rcb_table->pm_flags.hibernateAllowed = 0;
#endif
		pr_debug("hibernateAllowed=%d\n",
				handle->rcb_table->pm_flags.hibernateAllowed);
		first_time = 0;
		cm_write_mod_reg(HW_AUTO,
				 OMAP4430_CM2_CORE_MOD,
				 OMAP4_CM_DUCATI_CLKSTCTRL_OFFSET);
#ifdef CONFIG_OMAP_PM
		retval = omap_pm_set_max_sdma_lat(&pm_qos_handle_2,
						IPU_PM_MM_MPU_LAT_CONSTRAINT);
		if (retval)
			pr_info("Unable to set cstr on IPU\n");
#endif
	}

	/* Check if the M3 was loaded */
	sys_loaded = (ipu_pm_get_state(proc_id) & SYS_PROC_LOADED) >>
								PROC_LD_SHIFT;
	app_loaded = (ipu_pm_get_state(proc_id) & APP_PROC_LOADED) >>
								PROC_LD_SHIFT;

	/* Because of the current scheme, we need to check
	 * if APPM3 is enable and we need to enable it too
	 * In both cases we should check if for both cores
	 * and enable them if they were loaded.
	*/
	mutex_lock(ipu_pm_state.gate_handle);
	if (proc_id == SYS_M3 || proc_id == APP_M3) {
		if (!(ipu_pm_get_state(proc_id) & SYS_PROC_DOWN))
			goto exit;

		if (ducati_mbox && !_is_mbox_up) {
			omap_mbox_restore_ctx(ducati_mbox);
			_is_mbox_up = 1;
		} else
			pr_err("Not able to restore mbox");
		if (ducati_iommu && !_is_iommu_up) {
			iommu_restore_ctx(ducati_iommu);
			_is_iommu_up = 1;
		} else
			pr_err("Not able to restore iommu");

		pr_info("Wakeup SYSM3\n");
		retval = rproc_wakeup(sys_rproc);
		cm_write_mod_reg(HW_AUTO,
				 OMAP4430_CM2_CORE_MOD,
				 OMAP4_CM_DUCATI_CLKSTCTRL_OFFSET);
		if (retval)
			goto error;
		handle->rcb_table->state_flag &= ~SYS_PROC_DOWN;
		if (ipu_pm_get_state(proc_id) & APP_PROC_LOADED) {
			pr_info("Wakeup APPM3\n");
			retval = rproc_wakeup(app_rproc);
			cm_write_mod_reg(HW_AUTO,
				 OMAP4430_CM2_CORE_MOD,
				 OMAP4_CM_DUCATI_CLKSTCTRL_OFFSET);
			if (retval)
				goto error;
			handle->rcb_table->state_flag &= ~APP_PROC_DOWN;
		}
#ifdef CONFIG_SYSLINK_IPU_SELF_HIBERNATION
		/* turn on ducati hibernation timer */
		ipu_pm_timer_state(PM_HIB_TIMER_ON);
#endif
#ifdef CONFIG_OMAP_PM
		retval = omap_pm_set_max_sdma_lat(&pm_qos_handle_2,
						IPU_PM_MM_MPU_LAT_CONSTRAINT);
		if (retval)
			pr_info("Unable to set cstr on IPU\n");
#endif
	} else
		goto error;
exit:
	mutex_unlock(ipu_pm_state.gate_handle);
	return retval;
error:
	mutex_unlock(ipu_pm_state.gate_handle);
	pr_debug("Aborting restoring process\n");
	return -EINVAL;
}
EXPORT_SYMBOL(ipu_pm_restore_ctx);

/* Get the default configuration for the ipu_pm module.
 * needed in ipu_pm_setup.
 */
void ipu_pm_get_config(struct ipu_pm_config *cfg)
{
	int retval = 0;

	if (WARN_ON(unlikely(cfg == NULL))) {
		retval = -EINVAL;
		goto exit;
	}

	if (atomic_cmpmask_and_lt(&(ipu_pm_state.ref_count),
			IPU_PM_MAKE_MAGICSTAMP(0),
			IPU_PM_MAKE_MAGICSTAMP(1)) == true)
		memcpy(cfg, &ipu_pm_state.def_cfg,
			sizeof(struct ipu_pm_config));
	else
		memcpy(cfg, &ipu_pm_state.cfg, sizeof(struct ipu_pm_config));
	return;

exit:
	if (retval < 0)
		pr_err("ipu_pm_get_config failed! status = 0x%x", retval);
	return;
}
EXPORT_SYMBOL(ipu_pm_get_config);

/* Function to setup ipu pm object
 * This function is called in platform_setup()
 * This function will load the default configuration for ipu_pm
 * in this function we can decide what is going to be controled
 * by ipu_pm (DVFS, NOTIFICATIONS, ...) this configuration can
 * can be changed on run-time.
 * Also the workqueue is created and the local mutex
 */
int ipu_pm_setup(struct ipu_pm_config *cfg)
{
	struct ipu_pm_config tmp_cfg;
	int retval = 0;
	struct mutex *lock = NULL;

	/* This sets the ref_count variable is not initialized, upper 16 bits is
	  written with module Id to ensure correctness of refCount variable.
	 */
	atomic_cmpmask_and_set(&ipu_pm_state.ref_count,
				IPU_PM_MAKE_MAGICSTAMP(0),
				IPU_PM_MAKE_MAGICSTAMP(0));
	if (atomic_inc_return(&ipu_pm_state.ref_count)
				!= IPU_PM_MAKE_MAGICSTAMP(1)) {
		return 1;
	}

	if (cfg == NULL) {
		ipu_pm_get_config(&tmp_cfg);
		cfg = &tmp_cfg;
	}

	/* Create a default gate handle for local module protection */
	lock = kmalloc(sizeof(struct mutex), GFP_KERNEL);
	if (lock == NULL) {
		retval = -ENOMEM;
		goto exit;
	}
	mutex_init(lock);
	ipu_pm_state.gate_handle = lock;

	/* Create the wq for req/rel resources */
	ipu_resources = create_singlethread_workqueue("ipu_resources");
	ipu_clean_up = create_singlethread_workqueue("ipu_clean_up");
	INIT_WORK(&clean, ipu_pm_clean_work);

	/* No proc attached yet */
	global_rcb = NULL;
	pm_handle_appm3 = NULL;
	pm_handle_sysm3 = NULL;
	ducati_iommu = NULL;
	ducati_mbox = NULL;
	sys_rproc = NULL;
	app_rproc = NULL;

	memcpy(&ipu_pm_state.cfg, cfg, sizeof(struct ipu_pm_config));
	ipu_pm_state.is_setup = true;

	/* MBOX flag to check if there are pending messages */
	a9_m3_mbox = ioremap(A9_M3_MBOX, PAGE_SIZE);
	if (!a9_m3_mbox) {
		retval = -ENOMEM;
		goto exit;
	}

	/* BIOS flags to know the state of IPU cores */
	sysm3Idle = ioremap(IDLE_FLAG_PHY_ADDR, (sizeof(void) * 2));
	if (!sysm3Idle) {
		iounmap(a9_m3_mbox);
		retval = -ENOMEM;
		goto exit;
	}

	appm3Idle = (void *)sysm3Idle + sizeof(void *);
	if (!appm3Idle) {
		retval = -ENOMEM;
		iounmap(a9_m3_mbox);
		iounmap(sysm3Idle);
		goto exit;
	}
#ifdef SR_WA
	issHandle = ioremap(0x52000000, (sizeof(void) * 1));
	fdifHandle = ioremap(0x4A10A000, (sizeof(void) * 1));
#endif
	BLOCKING_INIT_NOTIFIER_HEAD(&ipu_pm_notifier);

	return retval;
exit:
	pr_err("ipu_pm_setup failed! retval = 0x%x", retval);
	return retval;
}
EXPORT_SYMBOL(ipu_pm_setup);

/* Function to attach ipu pm object
 * This function is called in ipc_attach()
 * This function will create the object based on the remoteproc id
 * It is also recieving the shared address pointer to use in rcb
 */
int ipu_pm_attach(u16 remote_proc_id, void *shared_addr)
{
	struct ipu_pm_params params;
	struct ipu_pm_object *handle;
	int retval = 0;

	/* Since currently the share memory used by remote cores
	 * to manage the rcb (resources) is one, a global pointer
	 * can be used to access it.
	 */
	if (!global_rcb)
		global_rcb = (struct sms *)shared_addr;

	ipu_pm_params_init(&params);
	params.remote_proc_id = remote_proc_id;
	params.shared_addr = (void *)global_rcb;
	params.line_id = LINE_ID;
	params.shared_addr_size = ipu_pm_mem_req(NULL);

	handle = ipu_pm_create(&params);
	if (WARN_ON(unlikely(handle == NULL))) {
		retval = -EINVAL;
		goto exit;
	}

	retval = ipu_pm_init_transport(handle);
	if (retval)
		goto exit;

	/* FIXME the physical address should be calculated */
	pr_debug("ipu_pm_attach at va0x%x pa0x9cf00400\n",
			(unsigned int)shared_addr);

	/* Get remote processor handle to save/restore */
	if (remote_proc_id == SYS_M3 && IS_ERR_OR_NULL(sys_rproc)) {
		pr_debug("requesting sys_rproc\n");
		sys_rproc = omap_rproc_get("ducati-proc0");
		if (sys_rproc == NULL) {
			retval = PTR_ERR(sys_rproc);
			sys_rproc = NULL;
			pr_err("%s %d failed to get sysm3 handle",
						__func__, __LINE__);
			goto exit;
		}
		handle->dmtimer = NULL;
	} else if (remote_proc_id == APP_M3 && IS_ERR_OR_NULL(app_rproc)) {
		pr_debug("requesting app_rproc\n");
		app_rproc = omap_rproc_get("ducati-proc1");
		if (IS_ERR_OR_NULL(app_rproc)) {
			retval = PTR_ERR(app_rproc);
			app_rproc = NULL;
			pr_err("%s %d failed to get appm3 handle",
						__func__, __LINE__);
			goto exit;
		}
		handle->dmtimer = NULL;
	}

	if (IS_ERR_OR_NULL(ducati_iommu)) {
		pr_debug("requesting ducati_iommu\n");
		ducati_iommu = iommu_get("ducati");
		if (IS_ERR_OR_NULL(ducati_iommu)) {
			retval = PTR_ERR(ducati_iommu);
			ducati_iommu = NULL;
			pr_err("%s %d failed to get iommu handle",
						__func__, __LINE__);
			goto exit;
		}
		_is_iommu_up = 1;
		iommu_register_notifier(ducati_iommu,
				&ipu_pm_notify_nb_iommu_ducati);
	}
	/* Get mailbox for save/restore */
	if (IS_ERR_OR_NULL(ducati_mbox)) {
		pr_debug("requesting ducati_mbox\n");
		ducati_mbox = omap_mbox_get("mailbox-2", NULL);
		if (IS_ERR_OR_NULL(ducati_mbox)) {
			retval = PTR_ERR(ducati_mbox);
			ducati_mbox = NULL;
			pr_err("%s %d failed to get mailbox handle",
						__func__, __LINE__);
			goto exit;
		}
	}

	return retval;
exit:
	pr_err("ipu_pm_attach failed! retval = 0x%x", retval);
	return retval;
}
EXPORT_SYMBOL(ipu_pm_attach);

/* Function to deattach ipu pm object
 * This function is called in ipc_detach()
 * This function will delete the object based
 * on the remoteproc id and unregister the notify
 * events used by ipu_pm module
 */
int ipu_pm_detach(u16 remote_proc_id)
{
	struct ipu_pm_object *handle;
	struct ipu_pm_params *params;
	int retval = 0;

	/* get the handle to proper ipu pm object */
	handle = ipu_pm_get_handle(remote_proc_id);
	if (WARN_ON(unlikely(handle == NULL))) {
		retval = -EINVAL;
		goto exit;
	}

	params = handle->params;
	if (WARN_ON(unlikely(params == NULL))) {
		retval = -EINVAL;
		goto exit;
	}

#ifdef CONFIG_SYSLINK_IPU_SELF_HIBERNATION
	/* reset the ducati hibernation timer */
	if (remote_proc_id == SYS_M3)
		ipu_pm_timer_state(PM_HIB_TIMER_RESET);
#endif

	/* When recovering clean_up was called, so wait for completion.
	 * If not make sure there is no resource pending.
	 */
	if (recover) {
		pr_debug("Recovering IPU\n");
		while (!wait_for_completion_timeout(&ipu_clean_up_comp,
					msecs_to_jiffies(params->timeout)))
			pr_warn("%s: handle(s) still opened\n", __func__);
		/* Get rid of any pending work */
		flush_workqueue(ipu_resources);
	} else {
		ipu_pm_clean_res();
	}

	/* unregister the events used for ipu_pm */
	retval = notify_unregister_event(
		params->remote_proc_id,
		params->line_id,
		params->pm_resource_event | (NOTIFY_SYSTEMKEY << 16),
		(notify_fn_notify_cbck)ipu_pm_callback,
		(void *)NULL);
	if (retval < 0) {
		pr_err("Error unregistering notify event\n");
		goto exit;
	}
	retval = notify_unregister_event(
		params->remote_proc_id,
		params->line_id,
		params->pm_notification_event | (NOTIFY_SYSTEMKEY << 16),
		(notify_fn_notify_cbck)ipu_pm_notify_callback,
		(void *)NULL);
	if (retval < 0) {
		pr_err("Error unregistering notify event\n");
		goto exit;
	}

	/* Put remote processor handle to save/restore */
	if (remote_proc_id == SYS_M3 && !IS_ERR_OR_NULL(sys_rproc)) {
		pr_debug("releasing sys_rproc\n");
		omap_rproc_put(sys_rproc);
		sys_rproc = NULL;
	} else if (remote_proc_id == APP_M3 && !IS_ERR_OR_NULL(app_rproc)) {
		pr_debug("releasing app_rproc\n");
		omap_rproc_put(app_rproc);
		app_rproc = NULL;
	}

	if (IS_ERR_OR_NULL(sys_rproc) && IS_ERR_OR_NULL(app_rproc)) {
		if (!IS_ERR_OR_NULL(ducati_iommu)) {
			/*
			 * Restore iommu to allow process's iommu cleanup
			 * after ipu_pm is shutdown
			 */
			if (ipu_pm_get_state(SYS_M3) & SYS_PROC_DOWN)
				iommu_restore_ctx(ducati_iommu);
			iommu_unregister_notifier(ducati_iommu,
					&ipu_pm_notify_nb_iommu_ducati);
			pr_debug("releasing ducati_iommu\n");
			/*
			 * FIXME: this need to be checked by the iommu driver
			 * restore IOMMU since it is required the IOMMU
			 * is up and running for reclaiming MMU entries
			 */
			if (!_is_iommu_up) {
				iommu_restore_ctx(ducati_iommu);
				_is_iommu_up = 1;
			}
			iommu_put(ducati_iommu);
			ducati_iommu = NULL;
		}
		/* Get mailbox for save/restore */
		if (!IS_ERR_OR_NULL(ducati_mbox)) {
			pr_debug("releasing ducati_mbox\n");
			omap_mbox_put(ducati_mbox, NULL);
			ducati_mbox = NULL;
		}
		/* Reset the state_flag */
		handle->rcb_table->state_flag = 0;
		if (recover)
			recover = false;
		global_rcb = NULL;
		first_time = 1;
	}

	/* Deleting the handle based on remote_proc_id */
	ipu_pm_delete(handle);

	return retval;
exit:
	pr_err("ipu_pm_detach failed handle null retval 0x%x", retval);
	return retval;
}
EXPORT_SYMBOL(ipu_pm_detach);

/* Function to destroy ipu_pm module
 * this function will destroy the structs
 * created to set the configuration
 */
int ipu_pm_destroy(void)
{
	int retval = 0;
	struct mutex *lock = NULL;

	if (WARN_ON(unlikely(atomic_cmpmask_and_lt(
				&ipu_pm_state.ref_count,
				IPU_PM_MAKE_MAGICSTAMP(0),
				IPU_PM_MAKE_MAGICSTAMP(1)) == true))) {
		retval = -ENODEV;
		goto exit;
	}

	if (!(atomic_dec_return(&ipu_pm_state.ref_count)
					== IPU_PM_MAKE_MAGICSTAMP(0))) {
		retval = 1;
		goto exit;
	}

	if (WARN_ON(ipu_pm_state.gate_handle == NULL)) {
		retval = -ENODEV;
		goto exit;
	}

	retval = mutex_lock_interruptible(ipu_pm_state.gate_handle);
	if (retval)
		goto exit;

	lock = ipu_pm_state.gate_handle;
	ipu_pm_state.gate_handle = NULL;
	mutex_unlock(lock);
	kfree(lock);
	/* Delete the wq for req/rel resources */
	destroy_workqueue(ipu_resources);
	destroy_workqueue(ipu_clean_up);

	first_time = 1;
	iounmap(sysm3Idle);
	iounmap(a9_m3_mbox);
#ifdef SR_WA
	iounmap(issHandle);
	iounmap(fdifHandle);
	issHandle = NULL;
	fdifHandle = NULL;
#endif
	sysm3Idle = NULL;
	appm3Idle = NULL;
	global_rcb = NULL;

	return retval;
exit:
	if (retval < 0)
		pr_err("ipu_pm_destroy failed, retval: %x\n", retval);
	return retval;
}
EXPORT_SYMBOL(ipu_pm_destroy);

static irqreturn_t ipu_pm_timer_interrupt(int irq, void *dev_id)
{
	struct omap_dm_timer *gpt = (struct omap_dm_timer *)dev_id;

	ipu_pm_timer_state(PM_HIB_TIMER_EXPIRE);
	omap_dm_timer_write_status(gpt, OMAP_TIMER_INT_OVERFLOW);
	return IRQ_HANDLED;
}

/* Function implements hibernation and watch dog timer
 * The functionality is based on following states
 * RESET:		Timer is disabed
 * OFF:			Timer is OFF
 * ON:			Timer running
 * HIBERNATE:	Waking up for ducati cores to hibernate
 * WD_RESET:	Waiting for Ducati cores to complete hibernation
 */
static int ipu_pm_timer_state(int event)
{
	int retval = 0;
	int tick_rate;
	struct ipu_pm_object *handle;
	struct ipu_pm_params *params;

	handle = ipu_pm_get_handle(SYS_M3);
	if (handle == NULL) {
		pr_err("ipu_pm_timer_state handle ptr NULL\n");
		retval = PTR_ERR(handle);
		goto exit;
	}
	params = handle->params;
	if (params == NULL) {
		pr_err("ipu_pm_timer_state params ptr NULL\n");
		retval = PTR_ERR(params);
		goto exit;
	}
	if (sys_rproc == NULL)
		goto exit;

	switch (event) {
	case PM_HIB_TIMER_EXPIRE:
		if (params->hib_timer_state == PM_HIB_TIMER_ON) {
			pr_debug("Starting hibernation, waking up M3 cores");
			handle->rcb_table->state_flag |= (SYS_PROC_HIB |
					APP_PROC_HIB | ENABLE_IPU_HIB);
#ifdef CONFIG_DUCATI_WATCH_DOG
			if (sys_rproc->dmtimer != NULL)
				omap_dm_timer_set_load(sys_rproc->dmtimer, 1,
						params->wdt_time);
			params->hib_timer_state = PM_HIB_TIMER_WDRESET;
		} else if (params->hib_timer_state ==
			PM_HIB_TIMER_WDRESET) {
			/* notify devh to begin error recovery here */
			pr_debug("Timer ISR: Trigger WD reset + recovery\n");
			ipu_pm_recover_schedule();
			ipu_pm_notify_event(0, NULL);
			if (sys_rproc->dmtimer != NULL)
				omap_dm_timer_stop(sys_rproc->dmtimer);
			params->hib_timer_state = PM_HIB_TIMER_OFF;
#endif
		}
		break;
	case PM_HIB_TIMER_RESET:
		/* disable timer and remove irq handler */
		if (handle->dmtimer) {
			free_irq(OMAP44XX_IRQ_GPT3, (void *)handle->dmtimer);
			handle->dmtimer = NULL;
			params->hib_timer_state = PM_HIB_TIMER_RESET;
		}
		break;
	case PM_HIB_TIMER_OFF: /* disable timer */
		/* no need to disable timer since it
		 * is done in rproc context */
		params->hib_timer_state = PM_HIB_TIMER_OFF;
		break;
	case PM_HIB_TIMER_ON: /* enable timer */
		if (params->hib_timer_state == PM_HIB_TIMER_RESET) {
			tick_rate = clk_get_rate(omap_dm_timer_get_fclk(
					sys_rproc->dmtimer));
			handle->rcb_table->hib_time = 0xFFFFFFFF - (
					(tick_rate/1000) * PM_HIB_DEFAULT_TIME);
			params->wdt_time = 0xFFFFFFFF - (
					(tick_rate/1000) * PM_HIB_WDT_TIME);
			retval = request_irq(OMAP44XX_IRQ_GPT3,
					ipu_pm_timer_interrupt,
					IRQF_DISABLED,
					"HIB_TIMER",
					(void *)sys_rproc->dmtimer);
			if (retval < 0)
				pr_warn("request_irq status: %x\n", retval);
			/*
			 * store the dmtimer handle locally to use during
			 * free_irq as dev_id token in cases where the remote
			 * proc frees the dmtimer handle first
			 */
			handle->dmtimer = sys_rproc->dmtimer;
		}
		if (sys_rproc->dmtimer != NULL)
			omap_dm_timer_set_load_start(sys_rproc->dmtimer, 1,
					handle->rcb_table->hib_time);
		params->hib_timer_state = PM_HIB_TIMER_ON;
		break;
	}
	return retval;
exit:
	if (retval < 0)
		pr_err("ipu_pm_timer_state failed, retval: %x\n", retval);
	return retval;
}

/*
 * ======== ipu_pm_notify_event ========
 *  IPU event notifications.
 */
#ifdef CONFIG_DUCATI_WATCH_DOG
static int ipu_pm_notify_event(int event, void *data)
{
	return blocking_notifier_call_chain(&ipu_pm_notifier, event, data);
}
#endif

/*
 * ======== ipu_pm_register_notifier ========
 *  Register for IPC events.
 */
int ipu_pm_register_notifier(struct notifier_block *nb)
{
	if (!nb)
		return -EINVAL;
	return blocking_notifier_chain_register(&ipu_pm_notifier, nb);
}
EXPORT_SYMBOL_GPL(ipu_pm_register_notifier);

/*
 * ======== ipu_pm_unregister_notifier ========
 *  Un-register for events.
 */
int ipu_pm_unregister_notifier(struct notifier_block *nb)
{
	if (!nb)
		return -EINVAL;
	return blocking_notifier_chain_unregister(&ipu_pm_notifier, nb);
}
EXPORT_SYMBOL_GPL(ipu_pm_unregister_notifier);
