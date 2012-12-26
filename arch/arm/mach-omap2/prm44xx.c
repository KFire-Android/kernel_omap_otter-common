/*
 * OMAP4 PRM module functions
 *
 * Copyright (C) 2011 Texas Instruments, Inc.
 * Copyright (C) 2010 Nokia Corporation
 * Benoît Cousson
 * Paul Walmsley
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/io.h>

#include <plat/cpu.h>
#include <plat/irqs.h>
#include <plat/prcm.h>

#include "iomap.h"
#include "common.h"
#include "vp.h"
#include "prm44xx.h"
#include "prm54xx.h"
#include "prm-regbits-44xx.h"
#include "prcm44xx.h"
#include "prminst44xx.h"
#include "voltage.h"

static const struct omap_prcm_irq omap4_prcm_irqs[] = {
	OMAP_PRCM_IRQ("wkup",   0,      0),
	OMAP_PRCM_IRQ("io",     9,      1),
};

static struct omap_prcm_irq_setup omap4_prcm_irq_setup = {
	.ack			= OMAP4_PRM_IRQSTATUS_MPU_OFFSET,
	.mask			= OMAP4_PRM_IRQENABLE_MPU_OFFSET,
	.nr_regs		= 2,
	.irqs			= omap4_prcm_irqs,
	.nr_irqs		= ARRAY_SIZE(omap4_prcm_irqs),
	.irq			= OMAP44XX_IRQ_PRCM,
	.read_pending_irqs	= &omap44xx_prm_read_pending_irqs,
	.ocp_barrier		= &omap44xx_prm_ocp_barrier,
	.save_and_clear_irqen	= &omap44xx_prm_save_and_clear_irqen,
	.restore_irqen		= &omap44xx_prm_restore_irqen,
};

/* PRM low-level functions */

/* Read a register in a CM/PRM instance in the PRM module */
u32 omap4_prm_read_inst_reg(s16 inst, u16 reg)
{
	return __raw_readl(prm_base + inst + reg);
}

/* Write into a register in a CM/PRM instance in the PRM module */
void omap4_prm_write_inst_reg(u32 val, s16 inst, u16 reg)
{
	__raw_writel(val, prm_base + inst + reg);
}

/* Read-modify-write a register in a PRM module. Caller must lock */
u32 omap4_prm_rmw_inst_reg_bits(u32 mask, u32 bits, s16 inst, s16 reg)
{
	u32 v;

	v = omap4_prm_read_inst_reg(inst, reg);
	v &= ~mask;
	v |= bits;
	omap4_prm_write_inst_reg(v, inst, reg);

	return v;
}

/* PRM VP */

/*
 * struct omap4_vp - OMAP4 VP register access description.
 * @irqstatus_mpu: offset to IRQSTATUS_MPU register for VP
 * @tranxdone_status: VP_TRANXDONE_ST bitmask in PRM_IRQSTATUS_MPU reg
 */
struct omap4_vp {
	u32 irqstatus_mpu;
	u32 tranxdone_status;
};

static struct omap4_vp omap4_vp[] = {
	[OMAP4_VP_VDD_MPU_ID] = {
		.irqstatus_mpu = OMAP4_PRM_IRQSTATUS_MPU_2_OFFSET,
		.tranxdone_status = OMAP4430_VP_MPU_TRANXDONE_ST_MASK,
	},
	[OMAP4_VP_VDD_IVA_ID] = {
		.irqstatus_mpu = OMAP4_PRM_IRQSTATUS_MPU_OFFSET,
		.tranxdone_status = OMAP4430_VP_IVA_TRANXDONE_ST_MASK,
	},
	[OMAP4_VP_VDD_CORE_ID] = {
		.irqstatus_mpu = OMAP4_PRM_IRQSTATUS_MPU_OFFSET,
		.tranxdone_status = OMAP4430_VP_CORE_TRANXDONE_ST_MASK,
	},
};

u32 omap4_prm_vp_check_txdone(u8 vp_id)
{
	struct omap4_vp *vp = &omap4_vp[vp_id];
	u32 irqstatus;

	irqstatus = omap4_prminst_read_inst_reg(OMAP4430_PRM_PARTITION,
						OMAP4430_PRM_OCP_SOCKET_INST,
						vp->irqstatus_mpu);
	return irqstatus & vp->tranxdone_status;
}

void omap4_prm_vp_clear_txdone(u8 vp_id)
{
	struct omap4_vp *vp = &omap4_vp[vp_id];

	omap4_prminst_write_inst_reg(vp->tranxdone_status,
				     OMAP4430_PRM_PARTITION,
				     OMAP4430_PRM_OCP_SOCKET_INST,
				     vp->irqstatus_mpu);
};

u32 omap4_prm_vcvp_read(u8 offset)
{
	return omap4_prminst_read_inst_reg(OMAP4430_PRM_PARTITION,
					   OMAP4430_PRM_DEVICE_INST, offset);
}

void omap4_prm_vcvp_write(u32 val, u8 offset)
{
	omap4_prminst_write_inst_reg(val, OMAP4430_PRM_PARTITION,
				     OMAP4430_PRM_DEVICE_INST, offset);
}

u32 omap4_prm_vcvp_rmw(u32 mask, u32 bits, u8 offset)
{
	return omap4_prminst_rmw_inst_reg_bits(mask, bits,
					       OMAP4430_PRM_PARTITION,
					       OMAP4430_PRM_DEVICE_INST,
					       offset);
}

u32 omap5_prm_vcvp_read(u8 offset)
{
	return omap4_prminst_read_inst_reg(OMAP54XX_PRM_PARTITION,
					   OMAP54XX_PRM_DEVICE_INST, offset);
}

void omap5_prm_vcvp_write(u32 val, u8 offset)
{
	omap4_prminst_write_inst_reg(val, OMAP54XX_PRM_PARTITION,
				     OMAP54XX_PRM_DEVICE_INST, offset);
}

u32 omap5_prm_vcvp_rmw(u32 mask, u32 bits, u8 offset)
{
	return omap4_prminst_rmw_inst_reg_bits(mask, bits,
					       OMAP54XX_PRM_PARTITION,
					       OMAP54XX_PRM_DEVICE_INST,
					       offset);
}

static inline u32 _read_pending_irq_reg(u16 irqen_offs, u16 irqst_offs)
{
	u32 mask, st;

	/* XXX read mask from RAM? */
	mask = omap4_prm_read_inst_reg(OMAP4430_PRM_OCP_SOCKET_INST,
				       irqen_offs);
	st = omap4_prm_read_inst_reg(OMAP4430_PRM_OCP_SOCKET_INST, irqst_offs);

	return mask & st;
}

/**
 * omap44xx_prm_read_pending_irqs - read pending PRM MPU IRQs into @events
 * @events: ptr to two consecutive u32s, preallocated by caller
 *
 * Read PRM_IRQSTATUS_MPU* bits, AND'ed with the currently-enabled PRM
 * MPU IRQs, and store the result into the two u32s pointed to by @events.
 * No return value.
 */
void omap44xx_prm_read_pending_irqs(unsigned long *events)
{
	events[0] = _read_pending_irq_reg(OMAP4_PRM_IRQENABLE_MPU_OFFSET,
					  OMAP4_PRM_IRQSTATUS_MPU_OFFSET);

	events[1] = _read_pending_irq_reg(OMAP4_PRM_IRQENABLE_MPU_2_OFFSET,
					  OMAP4_PRM_IRQSTATUS_MPU_2_OFFSET);
}

/**
 * omap44xx_prm_ocp_barrier - force buffered MPU writes to the PRM to complete
 *
 * Force any buffered writes to the PRM IP block to complete.  Needed
 * by the PRM IRQ handler, which reads and writes directly to the IP
 * block, to avoid race conditions after acknowledging or clearing IRQ
 * bits.  No return value.
 */
void omap44xx_prm_ocp_barrier(void)
{
	omap4_prm_read_inst_reg(OMAP4430_PRM_OCP_SOCKET_INST,
				OMAP4_REVISION_PRM_OFFSET);
}

/**
 * omap44xx_prm_save_and_clear_irqen - save/clear PRM_IRQENABLE_MPU* regs
 * @saved_mask: ptr to a u32 array to save IRQENABLE bits
 *
 * Save the PRM_IRQENABLE_MPU and PRM_IRQENABLE_MPU_2 registers to
 * @saved_mask.  @saved_mask must be allocated by the caller.
 * Intended to be used in the PRM interrupt handler suspend callback.
 * The OCP barrier is needed to ensure the write to disable PRM
 * interrupts reaches the PRM before returning; otherwise, spurious
 * interrupts might occur.  No return value.
 */
void omap44xx_prm_save_and_clear_irqen(u32 *saved_mask)
{
	saved_mask[0] =
		omap4_prm_read_inst_reg(OMAP4430_PRM_OCP_SOCKET_INST,
					OMAP4_PRM_IRQENABLE_MPU_OFFSET);
	saved_mask[1] =
		omap4_prm_read_inst_reg(OMAP4430_PRM_OCP_SOCKET_INST,
					OMAP4_PRM_IRQENABLE_MPU_2_OFFSET);

	omap4_prm_write_inst_reg(0, OMAP4430_PRM_OCP_SOCKET_INST,
				 OMAP4_PRM_IRQENABLE_MPU_OFFSET);
	omap4_prm_write_inst_reg(0, OMAP4430_PRM_OCP_SOCKET_INST,
				 OMAP4_PRM_IRQENABLE_MPU_2_OFFSET);

	/* OCP barrier */
	omap4_prm_read_inst_reg(OMAP4430_PRM_OCP_SOCKET_INST,
				OMAP4_REVISION_PRM_OFFSET);
}

/**
 * omap44xx_prm_restore_irqen - set PRM_IRQENABLE_MPU* registers from args
 * @saved_mask: ptr to a u32 array of IRQENABLE bits saved previously
 *
 * Restore the PRM_IRQENABLE_MPU and PRM_IRQENABLE_MPU_2 registers from
 * @saved_mask.  Intended to be used in the PRM interrupt handler resume
 * callback to restore values saved by omap44xx_prm_save_and_clear_irqen().
 * No OCP barrier should be needed here; any pending PRM interrupts will fire
 * once the writes reach the PRM.  No return value.
 */
void omap44xx_prm_restore_irqen(u32 *saved_mask)
{
	omap4_prm_write_inst_reg(saved_mask[0], OMAP4430_PRM_OCP_SOCKET_INST,
				 OMAP4_PRM_IRQENABLE_MPU_OFFSET);
	omap4_prm_write_inst_reg(saved_mask[1], OMAP4430_PRM_OCP_SOCKET_INST,
				 OMAP4_PRM_IRQENABLE_MPU_2_OFFSET);
}

/**
 * omap44xx_prm_reconfigure_io_chain - clear latches and reconfigure I/O chain
 *
 * Clear any previously-latched I/O wakeup events and ensure that the
 * I/O wakeup gates are aligned with the current mux settings.  Works
 * by asserting WUCLKIN, waiting for WUCLKOUT to be asserted, and then
 * deasserting WUCLKIN and waiting for WUCLKOUT to be deasserted.
 * No return value. XXX Are the final two steps necessary?
 */
void omap44xx_prm_reconfigure_io_chain(void)
{
	int i = 0;
	u32 pending_irq;

	s16 dev_inst = cpu_is_omap44xx() ? OMAP4430_PRM_DEVICE_INST :
					   OMAP54XX_PRM_DEVICE_INST;

	/*
	 * Do not reconfigure I/O chain if we have pending I/O wakeup
	 * interrupt. This will be done later at the end of I/O wakeup
	 * interrupt handler routine (1). Otherwise wakeup events will
	 * be missed because next two steps clear latches and in
	 * the handler (1) all WAKEUP_EVENT bits will be cleared
	 */
	if (cpu_is_omap44xx()) {
		pending_irq = _read_pending_irq_reg(
					OMAP4_PRM_IRQENABLE_MPU_OFFSET,
					OMAP4_PRM_IRQSTATUS_MPU_OFFSET);
		if (pending_irq & OMAP4430_IO_ST_MASK)
			return;
	}

	/* Trigger WUCLKIN enable */
	omap4_prm_rmw_inst_reg_bits(OMAP4430_WUCLK_CTRL_MASK,
			OMAP4430_WUCLK_CTRL_MASK, dev_inst,
			OMAP4_PRM_IO_PMCTRL_OFFSET);
	omap_test_timeout(
		(((omap4_prm_read_inst_reg(dev_inst,
			OMAP4_PRM_IO_PMCTRL_OFFSET) &
			OMAP4430_WUCLK_STATUS_MASK) >>
			OMAP4430_WUCLK_STATUS_SHIFT) == 1),
		MAX_IOPAD_LATCH_TIME, i);
	if (i == MAX_IOPAD_LATCH_TIME)
		pr_warn("PRM: I/O chain clock line assertion timed out\n");

	/* Trigger WUCLKIN disable */
	omap4_prm_rmw_inst_reg_bits(OMAP4430_WUCLK_CTRL_MASK, 0x0,
			dev_inst, OMAP4_PRM_IO_PMCTRL_OFFSET);
	omap_test_timeout(
		(((omap4_prm_read_inst_reg(dev_inst,
			OMAP4_PRM_IO_PMCTRL_OFFSET) &
			OMAP4430_WUCLK_STATUS_MASK) >>
			OMAP4430_WUCLK_STATUS_SHIFT) == 0),
		MAX_IOPAD_LATCH_TIME, i);
	if (i == MAX_IOPAD_LATCH_TIME)
		pr_warn("PRM: I/O chain clock line deassertion timed out\n");

	return;
}

/**
 * omap44xx_prm_enable_io_wakeup - enable wakeup events from I/O wakeup latches
 *
 * Activates the I/O wakeup event latches and allows events logged by
 * those latches to signal a wakeup event to the PRCM.  For I/O wakeups
 * to occur, WAKEUPENABLE bits must be set in the pad mux registers, and
 * omap44xx_prm_reconfigure_io_chain() must be called.  No return value.
 */
static void __init omap44xx_prm_enable_io_wakeup(void)
{
	s16 dev_inst = cpu_is_omap44xx() ? OMAP4430_PRM_DEVICE_INST :
					   OMAP54XX_PRM_DEVICE_INST;

	omap4_prm_rmw_inst_reg_bits(OMAP4430_GLOBAL_WUEN_MASK,
			OMAP4430_GLOBAL_WUEN_MASK, dev_inst,
			OMAP4_PRM_IO_PMCTRL_OFFSET);
}

static int __init omap4xxx_prcm_init(void)
{
	int ret = 0;

	if (cpu_is_omap44xx() || cpu_is_omap54xx()) {
		ret = omap_prcm_register_chain_handler(&omap4_prcm_irq_setup);
		/*
		 * Don't enable daisy chain, if failed to register
		 * chain interrupt handler.
		 */
		if (ret) {
			WARN(1, "PRM: Failed to register chain handler!!!");
			return ret;
		}
		omap44xx_prm_enable_io_wakeup();
	}

	/*
	 * Errata i612 - Wkup Clk Recycling Needed After Warm Reset
	 * CRITICALITY: Low
	 * REVISIONS IMPACTED: OMAP4430 all, OMAP543x ES1.0
	 * Hardware does not recycle the I/O wake-up clock upon a global warm
	 * reset. When a warm reset is done, wakeups of daisy I/Os are disabled,
	 * but without the wake-up clock running, this change is not latched.
	 * Hence there is a possibility of seeing unexpected I/O wake-up events
	 * after warm reset.
	 *
	 * As W/A the call to omap44xx_prm_reconfigure_io_chain() has been added
	 * at PM initialization time for I/O pads daisy chain resetting.
	 */
	if (cpu_is_omap443x() || omap_rev() == OMAP5430_REV_ES1_0 ||
	    omap_rev() == OMAP5432_REV_ES1_0)
		omap44xx_prm_reconfigure_io_chain();

	return ret;
}
subsys_initcall(omap4xxx_prcm_init);

/* PRM ABB */

/*
 * struct omap4_vp - OMAP4 VP register access description.
 * @irqstatus_mpu: offset to IRQSTATUS_MPU register for VP
 * @tranxdone_status: VP_TRANXDONE_ST bitmask in PRM_IRQSTATUS_MPU reg
 */
struct omap4_abb {
	u32 irqstatus_mpu;
	u32 tranxdone_status;
};

static struct omap4_abb omap4_abb[] = {
	[OMAP4_VP_VDD_MPU_ID] = {
		.irqstatus_mpu = OMAP4_PRM_IRQSTATUS_MPU_2_OFFSET,
		.tranxdone_status = OMAP4430_ABB_MPU_DONE_ST_MASK,
	},
	[OMAP4_VP_VDD_IVA_ID] = {
		.irqstatus_mpu = OMAP4_PRM_IRQSTATUS_MPU_OFFSET,
		.tranxdone_status = OMAP4430_ABB_IVA_DONE_ST_MASK,
	},
};

u32 omap4_prm_abb_check_txdone(u8 abb_id)
{
	struct omap4_abb *abb = &omap4_abb[abb_id];
	u32 irqstatus;

	irqstatus = omap4_prminst_read_inst_reg(OMAP4430_PRM_PARTITION,
						OMAP4430_PRM_OCP_SOCKET_INST,
						abb->irqstatus_mpu);
	return irqstatus & abb->tranxdone_status;
}

void omap4_prm_abb_clear_txdone(u8 abb_id)
{
	struct omap4_abb *abb = &omap4_abb[abb_id];

	omap4_prminst_write_inst_reg(abb->tranxdone_status,
				     OMAP4430_PRM_PARTITION,
				     OMAP4430_PRM_OCP_SOCKET_INST,
				     abb->irqstatus_mpu);
};
