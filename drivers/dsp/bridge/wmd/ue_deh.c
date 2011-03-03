/*
 * ue_deh.c
 *
 * DSP-BIOS Bridge driver support functions for TI OMAP processors.
 *
 * Implements upper edge DSP exception handling (DEH) functions.
 *
 * Copyright (C) 2005-2006 Texas Instruments, Inc.
 *
 * This package is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * THIS PACKAGE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

/*  ----------------------------------- Host OS */
#include <dspbridge/host_os.h>

/*  ----------------------------------- DSP/BIOS Bridge */
#include <dspbridge/std.h>
#include <dspbridge/dbdefs.h>

/*  ----------------------------------- Trace & Debug */
#include <dspbridge/dbc.h>

/*  ----------------------------------- OS Adaptation Layer */
#include <dspbridge/cfg.h>
#include <dspbridge/ntfy.h>
#include <dspbridge/drv.h>

/*  ----------------------------------- Link Driver */
#include <dspbridge/wmddeh.h>

/*  ----------------------------------- Platform Manager */
#include <dspbridge/dev.h>
#include <dspbridge/wcd.h>

/* ------------------------------------ Hardware Abstraction Layer */
#include <hw_defs.h>
#include <hw_mmu.h>

/*  ----------------------------------- This */
#include "mmu_fault.h"
#include "_tiomap.h"
#include "_deh.h"
#include "_tiomap_mmu.h"
#include "_tiomap_pwr.h"
#include <dspbridge/io_sm.h>

#include <plat/dmtimer.h>

/* GP Timer number to trigger interrupt for MMU-fault ISR on DSP */
#define GPTIMER_FOR_DSP_MMU_FAULT	7
/* Bit mask to enable overflow interrupt */
#define GPTIMER_IRQ_OVERFLOW           2
/* Max time to check for GP Timer IRQ */
#define GPTIMER_IRQ_WAIT_MAX_CNT       1000

static struct hw_mmu_map_attrs_t map_attrs = { HW_LITTLE_ENDIAN,
	HW_ELEM_SIZE16BIT,
	HW_MMU_CPUES
};

#define VIRT_TO_PHYS(x)       ((x) - PAGE_OFFSET + PHYS_OFFSET)

static u32 dummy_va_addr;

static struct omap_dm_timer *timer;

/*
 *  ======== bridge_deh_create ========
 *      Creates DEH manager object.
 */
int bridge_deh_create(OUT struct deh_mgr **phDehMgr,
			     struct dev_object *hdev_obj)
{
	int status = 0;
	struct deh_mgr *deh_mgr_obj = NULL;
	struct wmd_dev_context *hwmd_context = NULL;

	/*  Message manager will be created when a file is loaded, since
	 *  size of message buffer in shared memory is configurable in
	 *  the base image. */
	/* Get WMD context info. */
	dev_get_wmd_context(hdev_obj, &hwmd_context);
	DBC_ASSERT(hwmd_context);
	dummy_va_addr = 0;
	/* Allocate IO manager object: */
	deh_mgr_obj = kzalloc(sizeof(struct deh_mgr), GFP_KERNEL);
	if (deh_mgr_obj == NULL) {
		status = -ENOMEM;
	} else {
		/* Create an NTFY object to manage notifications */
		deh_mgr_obj->ntfy_obj = kmalloc(sizeof(struct ntfy_object),
							GFP_KERNEL);
		if (deh_mgr_obj->ntfy_obj)
			ntfy_init(deh_mgr_obj->ntfy_obj);
		else
			status = -ENOMEM;

		deh_mgr_obj->mmu_wq = create_workqueue("dsp-mmu_wq");
		if (!deh_mgr_obj->mmu_wq)
			status = -ENOMEM;

		INIT_WORK(&deh_mgr_obj->fault_work, mmu_fault_work);


		if (DSP_SUCCEEDED(status)) {
			/* Fill in context structure */
			deh_mgr_obj->hwmd_context = hwmd_context;
			deh_mgr_obj->err_info.dw_err_mask = 0L;
			deh_mgr_obj->err_info.dw_val1 = 0L;
			deh_mgr_obj->err_info.dw_val2 = 0L;
			deh_mgr_obj->err_info.dw_val3 = 0L;
			/* Install ISR function for DSP MMU fault */
			if ((request_irq(INT_DSP_MMU_IRQ, mmu_fault_isr, 0,
					 "DspBridge\tiommu fault",
					 (void *)deh_mgr_obj)) == 0)
				status = 0;
			else
				status = -EPERM;
		}
	}
	if (DSP_FAILED(status)) {
		/* If create failed, cleanup */
		bridge_deh_destroy((struct deh_mgr *)deh_mgr_obj);
		*phDehMgr = NULL;
	} else {
		timer = omap_dm_timer_request_specific(
					GPTIMER_FOR_DSP_MMU_FAULT);
		if (timer)
			omap_dm_timer_disable(timer);
		else {
			pr_err("%s:GPTimer not available\n", __func__);
			return -ENODEV;
		}
		*phDehMgr = (struct deh_mgr *)deh_mgr_obj;
	}

	return status;
}

/*
 *  ======== bridge_deh_destroy ========
 *      Destroys DEH manager object.
 */
int bridge_deh_destroy(struct deh_mgr *hdeh_mgr)
{
	int status = 0;
	struct deh_mgr *deh_mgr_obj = (struct deh_mgr *)hdeh_mgr;

	if (deh_mgr_obj) {
		/* Release dummy VA buffer */
		bridge_deh_release_dummy_mem();
		/* If notification object exists, delete it */
		if (deh_mgr_obj->ntfy_obj) {
			(void)ntfy_delete(deh_mgr_obj->ntfy_obj);
			kfree(deh_mgr_obj->ntfy_obj);
		}
		/* Disable DSP MMU fault */
		free_irq(INT_DSP_MMU_IRQ, deh_mgr_obj);

		destroy_workqueue(deh_mgr_obj->mmu_wq);

		/* Deallocate the DEH manager object */
		kfree(deh_mgr_obj);
		/* The GPTimer is no longer needed */
		omap_dm_timer_free(timer);
		timer = NULL;
	}

	return status;
}

/*
 *  ======== bridge_deh_register_notify ========
 *      Registers for DEH notifications.
 */
int bridge_deh_register_notify(struct deh_mgr *hdeh_mgr, u32 event_mask,
				   u32 notify_type,
				   struct dsp_notification *hnotification)
{
	int status = 0;
	struct deh_mgr *deh_mgr_obj = (struct deh_mgr *)hdeh_mgr;

	if (deh_mgr_obj) {
		if (event_mask)
			status = ntfy_register(deh_mgr_obj->ntfy_obj,
				hnotification, event_mask, notify_type);
		else
			status = ntfy_unregister(deh_mgr_obj->ntfy_obj,
							hnotification);
	}

	return status;
}

/*
 *  ======== bridge_deh_notify ========
 *      DEH error notification function. Informs user about the error.
 */
void bridge_deh_notify(struct deh_mgr *hdeh_mgr, u32 ulEventMask, u32 dwErrInfo)
{
	struct deh_mgr *deh_mgr_obj = (struct deh_mgr *)hdeh_mgr;
	struct wmd_dev_context *dev_context;
	int status = 0;
	u32 mem_physical = 0;
	u32 hw_mmu_max_tlb_count = 31;
	extern u32 fault_addr;
	struct cfg_hostres *resources;
	hw_status hw_status_obj;
	u32 cnt = 0;


	if (deh_mgr_obj) {
		printk(KERN_INFO
		       "bridge_deh_notify: ********** DEVICE EXCEPTION "
		       "**********\n");
		dev_context =
		    (struct wmd_dev_context *)deh_mgr_obj->hwmd_context;
		resources = dev_context->resources;

		switch (ulEventMask) {
		case DSP_SYSERROR:
			/* reset err_info structure before use */
			deh_mgr_obj->err_info.dw_err_mask = DSP_SYSERROR;
			deh_mgr_obj->err_info.dw_val1 = 0L;
			deh_mgr_obj->err_info.dw_val2 = 0L;
			deh_mgr_obj->err_info.dw_val3 = 0L;
			deh_mgr_obj->err_info.dw_val1 = dwErrInfo;
			printk(KERN_ERR
			       "bridge_deh_notify: DSP_SYSERROR, err_info "
			       "= 0x%x\n", dwErrInfo);
			dump_dl_modules(dev_context);
			dump_dsp_stack(dev_context);

			break;
		case DSP_MMUFAULT:
			/* MMU fault routine should have set err info
			 * structure */
			deh_mgr_obj->err_info.dw_err_mask = DSP_MMUFAULT;
			printk(KERN_INFO "bridge_deh_notify: DSP_MMUFAULT,"
			       "err_info = 0x%x\n", dwErrInfo);
			printk(KERN_INFO
			       "bridge_deh_notify: DSP_MMUFAULT, High "
			       "Address = 0x%x\n",
			       (unsigned int)deh_mgr_obj->err_info.dw_val1);
			printk(KERN_INFO "bridge_deh_notify: DSP_MMUFAULT, Low "
			       "Address = 0x%x\n",
			       (unsigned int)deh_mgr_obj->err_info.dw_val2);
			printk(KERN_INFO
			       "bridge_deh_notify: DSP_MMUFAULT, fault "
			       "address = 0x%x\n", (unsigned int)fault_addr);
			dummy_va_addr = (u32) kzalloc(sizeof(char) * 0x1000,
								GFP_ATOMIC);
			mem_physical =
			    VIRT_TO_PHYS(PG_ALIGN_LOW
					 ((u32) dummy_va_addr, PG_SIZE4K));
			dev_context = (struct wmd_dev_context *)
			    deh_mgr_obj->hwmd_context;

			print_dsp_trace_buffer(dev_context);
			dump_dl_modules(dev_context);

			/* Reset the dynamic mmu index to fixed count if it
			 * exceeds 31. So that the dynmmuindex is always
			 * between the range of standard/fixed entries
			 * and 31. */
			if (dev_context->num_tlb_entries >
			    hw_mmu_max_tlb_count) {
				dev_context->num_tlb_entries =
				    dev_context->fixed_tlb_entries;
			}
			if (DSP_SUCCEEDED(status)) {
				hw_status_obj =
				    hw_mmu_tlb_add(resources->dw_dmmu_base,
						   mem_physical, fault_addr,
						   HW_PAGE_SIZE4KB, 1,
						   &map_attrs, HW_SET, HW_SET);
			}
			/*
			 * Send a GP Timer interrupt to DSP
			 * The DSP expects a GP timer interrupt after an
			 * MMU-Fault Request GPTimer
			 */
			if (timer) {
				omap_dm_timer_enable(timer);
				/* Enable overflow interrupt */
				omap_dm_timer_set_int_enable(timer,
						GPTIMER_IRQ_OVERFLOW);
				/*
				 * Set counter value to overflow counter after
				 * one tick and start timer
				 */
				omap_dm_timer_set_load_start(timer, 0,
								0xfffffffe);

				/* Wait 80us for timer to overflow */
				udelay(80);

				/*
				 * Check interrupt status and
				 * wait for interrupt
				 */
				cnt = 0;
				while (!(omap_dm_timer_read_status(timer) &
					GPTIMER_IRQ_OVERFLOW)) {
					if (cnt++ >=
						GPTIMER_IRQ_WAIT_MAX_CNT) {
						pr_err("%s: GPTimer interrupt"
							" failed\n", __func__);
						break;
					}
				}
			}

			/* Clear MMU interrupt */
			hw_mmu_event_ack(resources->dw_dmmu_base,
					 HW_MMU_TRANSLATION_FAULT);
			dump_dsp_stack(deh_mgr_obj->hwmd_context);
			if (timer)
				omap_dm_timer_disable(timer);
			break;
#ifdef CONFIG_BRIDGE_NTFY_PWRERR
		case DSP_PWRERROR:
			/* reset err_info structure before use */
			deh_mgr_obj->err_info.dw_err_mask = DSP_PWRERROR;
			deh_mgr_obj->err_info.dw_val1 = 0L;
			deh_mgr_obj->err_info.dw_val2 = 0L;
			deh_mgr_obj->err_info.dw_val3 = 0L;
			deh_mgr_obj->err_info.dw_val1 = dwErrInfo;
			printk(KERN_ERR
			       "bridge_deh_notify: DSP_PWRERROR, err_info "
			       "= 0x%x\n", dwErrInfo);
			break;
#endif /* CONFIG_BRIDGE_NTFY_PWRERR */
#ifdef CONFIG_BRIDGE_WDT3
		case DSP_WDTOVERFLOW:
			deh_mgr_obj->err_info.dw_err_mask = DSP_WDTOVERFLOW;
			deh_mgr_obj->err_info.dw_val1 = 0L;
			deh_mgr_obj->err_info.dw_val2 = 0L;
			deh_mgr_obj->err_info.dw_val3 = 0L;
			pr_err("bridge_deh_notify: DSP_WDTOVERFLOW\n ");
			break;
#endif
		default:
			dev_dbg(bridge, "%s: Unknown Error, err_info = 0x%x\n",
				__func__, dwErrInfo);
			break;
		}

		/* Filter subsequent notifications when an error occurs */
		if (dev_context->dw_brd_state != BRD_ERROR) {
			ntfy_notify(deh_mgr_obj->ntfy_obj, ulEventMask);
#ifdef CONFIG_BRIDGE_RECOVERY
			bridge_recover_schedule();
#endif
		}

		/* Set the Board state as ERROR */
		dev_context->dw_brd_state = BRD_ERROR;
		/* Disable all the clocks that were enabled by DSP */
		(void)dsp_peripheral_clocks_disable(dev_context, NULL);
#ifdef CONFIG_BRIDGE_WDT3
		/*
		 * Avoid the subsequent WDT if it happens once,
		 * also If MMU fault occurs
		 */
		dsp_wdt_enable(false);
#endif

	}
}

/*
 *  ======== bridge_deh_get_info ========
 *      Retrieves error information.
 */
int bridge_deh_get_info(struct deh_mgr *hdeh_mgr,
			    struct dsp_errorinfo *pErrInfo)
{
	int status = 0;
	struct deh_mgr *deh_mgr_obj = (struct deh_mgr *)hdeh_mgr;

	DBC_REQUIRE(deh_mgr_obj);
	DBC_REQUIRE(pErrInfo);

	if (deh_mgr_obj) {
		/* Copy DEH error info structure to PROC error info
		 * structure. */
		pErrInfo->dw_err_mask = deh_mgr_obj->err_info.dw_err_mask;
		pErrInfo->dw_val1 = deh_mgr_obj->err_info.dw_val1;
		pErrInfo->dw_val2 = deh_mgr_obj->err_info.dw_val2;
		pErrInfo->dw_val3 = deh_mgr_obj->err_info.dw_val3;
	} else {
		status = -EFAULT;
	}

	return status;
}

/*
 *  ======== bridge_deh_release_dummy_mem ========
 *      Releases memory allocated for dummy page
 */
void bridge_deh_release_dummy_mem(void)
{
	kfree((void *)dummy_va_addr);
	dummy_va_addr = 0;
}
