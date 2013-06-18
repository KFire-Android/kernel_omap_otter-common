/*
 * Smart reflex Class 1.5 specific implementations
 *
 * Copyright (C) 2010-2012 Texas Instruments, Inc.
 * Nishanth Menon <nm@ti.com>
 * Andrii Tseglytskyi <andrii.tseglytskyi@ti.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/pm_qos.h>
#include <linux/sched.h>
#include <linux/suspend.h>
#include <linux/power/smartreflex.h>
#include <plat/cpu.h>
#include <plat/dvfs.h>
#include "voltage.h"

/**
 * DOC: Background into SmartReflex AVS classes
 *
 * Traditional SmartReflex AVS(Automatic Voltage Scaling) classes are:
 *
 * * Class 0 - Product test calibration
 *	Silicon is calibration at production floor and fused with voltages
 *	for each OPP
 * * Class 1 - Boot time calibration
 *	Silicon is calibrated once at boot time and voltages are stored for
 *	the lifetime of operation.
 * * Class 2 - Continuous s/w calibration
 *	SR module notifies s/w for any change in the system which is desired
 *	and the s/w makes runtime decisions in terms of setting the voltage,
 *	this mechanism could be used in the system which does not have PMIC
 *	capable of SR without using the voltage controller and voltage
 *	processor blocks.
 * * Class 3 - Continuous h/w calibration
 *	SR module is switch on after reaching a voltage level and SR
 *	continuously monitors the system and makes runtime adjustments without
 *	s/w involvement.
 *
 * DOC: Introduction into SmartReflex AVS 1.5 class
 *
 * OMAP3430 has used SmartReflex AVS and with a a PMIC which understands the SR
 * protocol, Class 3 has been used. With OMAP3630 onwards, a new SmartReflex AVS
 * class of operation Class 1.5 was introduced.
 * * Class 1.5 - Periodic s/w calibration
 *	This uses the h/w calibration loop and at the end of calibration
 *	stores the voltages to be used run time, periodic recalibration is
 *	performed as well.
 *
 * DOC: Operation of SmartReflex AVS 1.5 class
 *
 * The operational mode is describes as the following:
 * * SmartReflex AVS h/w calibration loop is essential to identify the optimal
 *	voltage for a given OPP.
 * * Once this optimal voltage is detected, SmartReflex AVS loop is disabled in
 *	class 1.5 mode of operation.
 * * Until there is a need for a recalibration, any further transition to an OPP
 *	voltage which is calibrated can use the calibrated voltage. In class1.5
 *	enabling the SR AVS h/w loop is not required until re-calibration.
 * * On a periodic basis (recommendation being once approximately 24 hours),
 *	software is expected to perform a recalibration to find a new optimal
 *	voltage which is compensated for device aging.
 *	- For performing this recalibration, the start voltage does not need to
 *	be the nominal voltage anymore. instead, the system can start with a
 *	voltage which is 50mV higher than the previously calibrated voltage to
 *	identify the new optimal voltage as the aging factor within a period of
 *	1 day is not going to be anywhere close to 50mV.
 *	- This "new starting point" for recalibration is called a dynamic
 *	nominal voltage for that voltage point.
 *
 * In short, with the introduction of SmartReflex class 1.5, there are three
 * new voltages possible in a system's DVFS transition:
 * * Nominal Voltage - The maximum voltage needed for a worst possible device
 *	in the worst possible conditions. This is the voltage we choose as
 *	the starting point for the h/w loop to optimize for the first time
 *	calibration on system bootup.
 * * Dynamic Nominal Voltage - Worst case voltage for a specific device in
 *	considering the system aging on the worst process device.
 * * Calibrated Voltage - Best voltage for the current device at a given point
 *	of time.
 *
 * In terms of the implementation, doing calibration involves waiting for the
 * SmartReflex h/w loop to settle down, and doing this as part of the DVFS flow
 * itself would increase the latency of DVFS transition when there is a need to
 * calibrate that opp. instead, the calibration is performed "out of path" using
 * a workqueue statemachine. The workqueue waits for the system stabilization,
 * then enables VP interrupts to monitor for system instability interms of
 * voltage oscillations that are reported back to the system as interrupts,
 * in case of prolonged system oscillations, nominal voltage is chosen as a
 * safe voltage and this event is logged in the system log for developer debug
 * and fixing.
 *
 * For the recalibration, a common workqueue for all domains is started at the
 * start of the class initialization and it resets the calibrated voltages
 * on a periodic basis. For distros that may choose not to do the recommended
 * periodic recalibration, instead choose to perform boot time calibration,
 * kconfig configuration option is provided to do so.
 */

 /* TODO: should be possible to make these computed values based on config. */
#define SRP5_SAMPLING_DELAY_MS	1
#define SRP5_STABLE_SAMPLES	10
#define SRP5_MAX_TRIGGERS	5

/*
 * We expect events in 10uS, if we don't receive it in twice as long,
 * we stop waiting for the event and use the current value
 */
#define SRP5_MAX_CHECK_VPTRANS_US	20

/**
 * struct sr_classp5_calib_data - data meant to be used by calibration work
 * @work:	calibration work
 * @sr:		SmartReflex for which we are triggering
 * @vdata:	voltage data we are calibrating
 * @num_calib_triggers:	number of triggers from calibration loop
 * @num_osc_samples:	number of samples collected by isr
 * @u_volt_samples:	private data for collecting voltage samples in
 *			case oscillations. filled by the notifier and
 *			consumed by the work item.
 * @work_active:	have we scheduled a work item?
 * @qos:		pm qos handle
 */
struct sr_classp5_calib_data {
	struct delayed_work work;
	struct omap_sr *sr;
	struct omap_volt_data *vdata;
	u8 num_calib_triggers;
	u8 num_osc_samples;
	unsigned long u_volt_samples[SRP5_STABLE_SAMPLES];
	bool work_active;
	struct pm_qos_request qos;
};

/**
 * sr_classp5_notify() - isr notifier for status events
 * @sr:		SmartReflex for which we were triggered
 * @status:	notifier event to use
 *
 * This basically collects data for the work to use.
 */
static int sr_classp5_notify(struct omap_sr *sr, u32 status)
{
	struct voltagedomain *voltdm = NULL;
	struct sr_classp5_calib_data *work_data = NULL;
	int idx = 0;

	if (IS_ERR_OR_NULL(sr) || IS_ERR_OR_NULL(sr->voltdm)) {
		pr_err("%s: bad parameters!\n", __func__);
		return -EINVAL;
	}

	voltdm = sr->voltdm;
	work_data = (struct sr_classp5_calib_data *)sr->voltdm_cdata;
	if (IS_ERR_OR_NULL(work_data)) {
		pr_err("%s:%s no work data!!\n", __func__, sr->name);
		return -EINVAL;
	}

	/* Wait for transdone so that we know the voltage to read */
	do {
		if (omap_vp_is_transdone(voltdm))
			break;
		idx++;
		/* get some constant delay */
		udelay(1);
	} while (idx < SRP5_MAX_CHECK_VPTRANS_US);

	/*
	 * NOTE:
	 * If we timeout, we still read the data,
	 * if we are oscillating+irq latencies are too high, we could
	 * have scenarios where we miss transdone event. since
	 * we waited long enough, it is still safe to read the voltage
	 * as we would have waited long enough - Dont warn for this.
	 */
	idx = (work_data->num_osc_samples) % SRP5_STABLE_SAMPLES;
	work_data->u_volt_samples[idx] = omap_vp_get_curr_volt(voltdm);
	work_data->num_osc_samples++;

	omap_vp_clear_transdone(voltdm);

	return 0;
}

/**
 * sr_classp5_start_hw_loop()
 * @sr:		SmartReflex for which we start calibration
 *
 * Starts hardware calibration
 */
static int sr_classp5_start_hw_loop(struct omap_sr *sr)
{
	int res;

	omap_vp_enable(sr->voltdm);
	res = sr_enable(sr);
	if (res) {
		pr_err("%s: %s failed to start HW loop\n", __func__, sr->name);
		omap_vp_disable(sr->voltdm);
	}
	return res;
}

/**
 * sr_classp5_stop_hw_loop()
 * @sr:		SmartReflex for which we stop calibration
 *
 * Stops hardware calibration
 */
static void sr_classp5_stop_hw_loop(struct omap_sr *sr)
{
	sr_disable_errgen(sr);
	omap_vp_disable(sr->voltdm);
	sr_disable(sr);
}

/**
 * sr_classp5_adjust_margin() - helper to map back margins
 * @pmic:		pmic struct
 * @volt_margin:	margin to add
 * @volt_current:	current voltage
 *
 * If PMIC APIs exist, then convert to PMIC step size adjusted voltage
 * corresponding to margin requested.
 */
static inline u32 sr_classp5_adjust_margin(struct omap_voltdm_pmic *pmic,
					   u32 volt_margin, u32 volt_current)
{
	if (pmic && pmic->vsel_to_uv && pmic->uv_to_vsel) {
		/*
		 * To ensure conversion works:
		 * use a proper base voltage - we use the current volt
		 * then convert it with pmic routine to vsel and back
		 * to voltage, and finally remove the base voltage
		 */
		volt_margin += volt_current;
		volt_margin = pmic->uv_to_vsel(volt_margin);
		volt_margin = pmic->vsel_to_uv(volt_margin);
		volt_margin -= volt_current;
	}
	return volt_margin;
}

/**
 * sr_classp5_calib_work() - work which actually does the calibration
 * @work: pointer to the work
 *
 * calibration routine uses the following logic:
 * on the first trigger, we start the isr to collect sr voltages
 * wait for stabilization delay (reschdule self instead of sleeping)
 * after the delay, see if we collected any isr events
 * if none, we have calibrated voltage.
 * if there are any, we retry untill we giveup.
 * on retry timeout, select a voltage to use as safe voltage.
 */
static void sr_classp5_calib_work(struct work_struct *work)
{
	struct sr_classp5_calib_data *work_data =
	    container_of(work, struct sr_classp5_calib_data, work.work);
	unsigned long u_volt_safe = 0, u_volt_current = 0, u_volt_margin = 0;
	struct omap_volt_data *volt_data;
	struct voltagedomain *voltdm;
	struct omap_sr *sr;
	struct omap_voltdm_pmic *pmic;
	int idx = 0;

	if (!work) {
		pr_err("%s: ooops.. null work_data?\n", __func__);
		return;
	}

	/*
	 * Handle the case where we might have just been scheduled AND
	 * disable was called.
	 */
	if (!mutex_trylock(&omap_dvfs_lock)) {
		schedule_delayed_work(&work_data->work,
				      msecs_to_jiffies(SRP5_SAMPLING_DELAY_MS *
						       SRP5_STABLE_SAMPLES));
		return;
	}

	sr = work_data->sr;
	voltdm = sr->voltdm;
	/*
	 * In the unlikely case that we did get through when unplanned,
	 * flag and return.
	 */
	if (unlikely(!work_data->work_active)) {
		pr_err("%s:%s unplanned work invocation!\n", __func__,
		       sr->name);
		/* No expectation of calibration, remove qos req */
		pm_qos_update_request(&work_data->qos, PM_QOS_DEFAULT_VALUE);
		mutex_unlock(&omap_dvfs_lock);
		return;
	}

	volt_data = work_data->vdata;

	work_data->num_calib_triggers++;
	/* if we are triggered first time, we need to start isr to sample */
	if (work_data->num_calib_triggers == 1) {
		/* We could be interrupted many times, so, only for debug */
		pr_debug("%s: %s: Calibration start: Voltage Nominal=%d\n",
			 __func__, sr->name, volt_data->volt_nominal);
		goto start_sampling;
	}

	/* Stop isr from interrupting our measurements :) */
	sr_notifier_control(sr, false);

	/*
	 * Quit sampling
	 * a) if we have oscillations
	 * b) if we have nominal voltage as the voltage
	 */
	if (work_data->num_calib_triggers == SRP5_MAX_TRIGGERS)
		goto stop_sampling;

	/* if there are no samples captured.. SR is silent, aka stability! */
	if (!work_data->num_osc_samples) {
		/* Did we interrupt too early? */
		u_volt_current = omap_vp_get_curr_volt(voltdm);
		/*
		 * If we waited enough amount of iterations, then we might be
		 * on a corner case where voltage adjusted = Vnom!
		 */
		if (work_data->num_calib_triggers < 2 &&
		    u_volt_current >= volt_data->volt_nominal)
			goto start_sampling;
		u_volt_safe = u_volt_current;
		goto done_calib;
	}

	/* we have potential oscillations/first sample */
start_sampling:
	work_data->num_osc_samples = 0;

	/* Clear transdone events so that we can go on. */
	do {
		if (!omap_vp_is_transdone(voltdm))
			break;
		idx++;
		/* get some constant delay */
		udelay(1);
		omap_vp_clear_transdone(voltdm);
	} while (idx < SRP5_MAX_CHECK_VPTRANS_US);
	if (idx >= SRP5_MAX_CHECK_VPTRANS_US)
		pr_warning("%s: timed out waiting for transdone clear!!\n",
			   __func__);

	/* Clear pending events */
	sr_notifier_control(sr, false);
	/* trigger sampling */
	sr_notifier_control(sr, true);
	schedule_delayed_work(&work_data->work,
			      msecs_to_jiffies(SRP5_SAMPLING_DELAY_MS *
					       SRP5_STABLE_SAMPLES));
	mutex_unlock(&omap_dvfs_lock);
	return;

stop_sampling:
	/*
	 * We are here for Oscillations due to two scenarios:
	 * a) SR is attempting to adjust voltage lower than VLIMITO
	 *    which VP will ignore, but SR will re-attempt
	 * b) actual oscillations
	 * NOTE: For debugging, enable debug to see the samples.
	 */
	pr_warning("%s: %s Stop sampling: Voltage Nominal=%d samples=%d\n",
		   __func__, sr->name,
		   volt_data->volt_nominal, work_data->num_osc_samples);

	/* pick up current voltage */
	u_volt_current = omap_vp_get_curr_volt(voltdm);

	/* Just in case we got more interrupts than our tiny buffer */
	if (work_data->num_osc_samples > SRP5_STABLE_SAMPLES)
		idx = SRP5_STABLE_SAMPLES;
	else
		idx = work_data->num_osc_samples;
	/* Index at 0 */
	idx -= 1;
	u_volt_safe = u_volt_current;
	/* Grab the max of the samples as the stable voltage */
	for (; idx >= 0; idx--) {
		pr_debug("%s: osc_v[%d]=%ld, safe_v=%ld\n", __func__, idx,
			 work_data->u_volt_samples[idx], u_volt_safe);
		if (work_data->u_volt_samples[idx] > u_volt_safe)
			u_volt_safe = work_data->u_volt_samples[idx];
	}

	/* Fall through to close up common stuff */
done_calib:
	sr_classp5_stop_hw_loop(sr);

	pmic = voltdm->pmic;

	/* Add Per OPP margin if needed */
	if (volt_data->volt_margin)
		u_volt_margin += sr_classp5_adjust_margin(pmic,
							 volt_data->volt_margin,
							 u_volt_current);
	u_volt_safe += u_volt_margin;

	/* just warn, dont clamp down on voltage */
	if (u_volt_margin && u_volt_safe > volt_data->volt_nominal) {
		pr_warning("%s: %s Vsafe %ld > Vnom %d. %ld[%d] margin on"
			   "vnom %d curr_v=%ld\n", __func__, sr->name,
			   u_volt_safe, volt_data->volt_nominal, u_volt_margin,
			   volt_data->volt_margin, volt_data->volt_nominal,
			   u_volt_current);
	}

	volt_data->volt_calibrated = u_volt_safe;
	/* Setup my dynamic voltage for the next calibration for this opp */
	volt_data->volt_dynamic_nominal = omap_get_dyn_nominal(volt_data);

	/*
	 * if the voltage we decided as safe is not the current voltage,
	 * switch
	 */
	if (volt_data->volt_calibrated != u_volt_current) {
		pr_debug("%s: %s reconfiguring to voltage %d\n",
			 __func__, sr->name, volt_data->volt_calibrated);
		voltdm_scale(voltdm, volt_data);
	}

	pr_info("%s: %s: Calibration complete: Voltage Nominal=%d "
		"Calib=%d Dyn=%d OPP_margin=%d total_margin=%ld\n",
		__func__, sr->name, volt_data->volt_nominal,
		volt_data->volt_calibrated, volt_data->volt_dynamic_nominal,
		volt_data->volt_margin, u_volt_margin);

	work_data->work_active = false;

	/* Calibration done, Remove qos req */
	pm_qos_update_request(&work_data->qos, PM_QOS_DEFAULT_VALUE);
	mutex_unlock(&omap_dvfs_lock);
}

#if CONFIG_OMAP_SR_CLASS1_P5_RECALIBRATION_DELAY
/* recal_work:	recalibration work */
static struct delayed_work recal_work;
static struct pm_qos_request recal_qos;
static unsigned long next_recal_time;
static bool recal_scheduled;

/**
 * sr_classp5_voltdm_recal() - Helper routine to reset calibration.
 * @voltdm:	Voltage domain to reset calibration for
 * @user:	unused
 *
 * NOTE: Appropriate locks must be held by calling path to ensure mutual
 * exclusivity
 */
static int sr_classp5_voltdm_recal(struct voltagedomain *voltdm, void *user)
{
	struct omap_volt_data *vdata;

	/*
	 * we need to go no further if sr is not enabled for this domain or
	 * voltage processor is not present for this voltage domain
	 * (example vdd_wakeup). Class 1.5 requires Voltage processor
	 * to function.
	 */
	if (!voltdm->vp || !omap_sr_is_enabled(voltdm))
		return 0;

	vdata = omap_voltage_get_curr_vdata(voltdm);
	if (!vdata) {
		pr_err("%s: unable to find current voltage for vdd_%s\n",
		       __func__, voltdm->name);
		return -ENXIO;
	}

	omap_sr_disable(voltdm);
	omap_voltage_calib_reset(voltdm);
	voltdm_reset(voltdm);
	omap_sr_enable(voltdm);
	pr_info("%s: %s: calibration reset\n", __func__, voltdm->name);

	return 0;
}

/**
 * sr_classp5_recal_work() - work which actually does the recalibration
 * @work: pointer to the work
 *
 * on a periodic basis, we come and reset our calibration setup
 * so that a recalibration of the OPPs take place. This takes
 * care of aging factor in the system.
 */
static void sr_classp5_recal_work(struct work_struct *work)
{
	unsigned long delay;

	/* try lock only to avoid deadlock with suspend handler */
	if (!mutex_trylock(&omap_dvfs_lock)) {
		pr_err("%s: Can't acquire lock, delay recalibration\n",
		       __func__);
		schedule_delayed_work(&recal_work, msecs_to_jiffies
				      (SRP5_SAMPLING_DELAY_MS *
				       SRP5_STABLE_SAMPLES));
		return;
	}

	/*
	 * Deny Idle during recalibration due to the following reasons:
	 * - HW loop is enabled when we enter this function
	 * - HW loop may be triggered at any moment of time from idle
	 * - As result we may have race between SmartReflex disabling/enabling
	 * from CPU Idle and from recalibration function
	 */
	pm_qos_update_request(&recal_qos, 0);

	delay = msecs_to_jiffies(CONFIG_OMAP_SR_CLASS1_P5_RECALIBRATION_DELAY);

	if (voltdm_for_each(sr_classp5_voltdm_recal, NULL))
		pr_err("%s: Recalibration failed\n", __func__);

	/* Enable CPU Idle */
	pm_qos_update_request(&recal_qos, PM_QOS_DEFAULT_VALUE);

	next_recal_time = jiffies + delay;
	schedule_delayed_work(&recal_work, delay);
	mutex_unlock(&omap_dvfs_lock);
}

static void sr_classp5_recal_cleanup(void)
{
	if (!recal_scheduled)
		return;

	cancel_delayed_work_sync(&recal_work);
	recal_scheduled = false;
	pr_debug("%s: Re-calibration work canceled\n", __func__);
}


static void sr_classp5_recal_resume(void)
{
	unsigned long delay, now_time = jiffies;

	if (recal_scheduled)
		return;

	if (time_before(now_time, next_recal_time))
		delay = next_recal_time - now_time;
	else
		delay = 0;

	pr_debug("%s: Recalibration work rescheduled to delay of %d msec",
		 __func__, jiffies_to_msecs(delay));

	/* Reschedule recalibration work on each resume */
	schedule_delayed_work(&recal_work, delay);
	recal_scheduled = true;

	return;
}

static int sr_classp5_recal_sleep_pm_callback(struct notifier_block *nfb,
						    unsigned long action,
						    void *ignored)
{
	switch (action) {
	case PM_HIBERNATION_PREPARE:
	case PM_SUSPEND_PREPARE:
		/*
		 * Need to make sure that re-calibration works
		 * is canceled before moving to Suspend mode.
		 * Otherwise this work may be executed at any moment, trigger
		 * SmartReflex and race with DVFS and MPUSS CPU Idle notifiers.
		 * As result - system will crash
		 */
		mutex_lock(&omap_dvfs_lock);
		sr_classp5_recal_cleanup();
		mutex_unlock(&omap_dvfs_lock);
		return NOTIFY_OK;

	case PM_POST_HIBERNATION:
	case PM_POST_SUSPEND:
		/*
		 * Reschedule re-calibration work, which was canceled
		 * during suspend preparation
		 */
		sr_classp5_recal_resume();
		return NOTIFY_OK;
	}

	return NOTIFY_DONE;
}

static struct notifier_block sr_classp5_recal_sleep_pm_notifier = {
	.notifier_call = sr_classp5_recal_sleep_pm_callback,
	.priority = 0,
};

static void __init sr_classp5_recal_register_sleep_pm_notifier(void)
{
	register_pm_notifier(&sr_classp5_recal_sleep_pm_notifier);
}

static void sr_classp5_recal_init(void)
{
	unsigned long delay;
	INIT_DELAYED_WORK_DEFERRABLE(&recal_work, sr_classp5_recal_work);
	pm_qos_add_request(&recal_qos, PM_QOS_CPU_DMA_LATENCY,
			   PM_QOS_DEFAULT_VALUE);
	delay = msecs_to_jiffies(CONFIG_OMAP_SR_CLASS1_P5_RECALIBRATION_DELAY);
	schedule_delayed_work(&recal_work, delay);
	next_recal_time = jiffies + delay;
	sr_classp5_recal_register_sleep_pm_notifier();
	recal_scheduled = true;
	pr_info("SmartReflex Recalibration delay = %dms\n",
		CONFIG_OMAP_SR_CLASS1_P5_RECALIBRATION_DELAY);
}
#else
static inline void sr_classp5_recal_init(void)
{
}
#endif			/* CONFIG_OMAP_SR_CLASS1_P5_RECALIBRATION_DELAY */

/**
 * sr_classp5_calibration_schedule() - schedule calibration for sr
 * @sr:	pointer for SR
 *
 * configure the AVS for offline calibration
 */
static int sr_classp5_calibration_schedule(struct omap_sr *sr)
{
	struct sr_classp5_calib_data *work_data;
	struct omap_volt_data *volt_data;

	work_data = (struct sr_classp5_calib_data *)sr->voltdm_cdata;
	if (IS_ERR_OR_NULL(work_data)) {
		pr_err("%s: bad work data %s\n", __func__, sr->name);
		return -EINVAL;
	}

	volt_data = omap_voltage_get_curr_vdata(sr->voltdm);
	if (IS_ERR_OR_NULL(volt_data)) {
		pr_err("%s: Voltage data is NULL. Cannot start work %s\n",
		       __func__, sr->name);
		return -ENODATA;
	}

	work_data->vdata = volt_data;
	work_data->work_active = true;
	work_data->num_calib_triggers = 0;
	/* Dont interrupt me until calibration is complete */
	pm_qos_update_request(&work_data->qos, 0);
	/* program the workqueue and leave it to calibrate offline.. */
	schedule_delayed_work(&work_data->work,
			      msecs_to_jiffies(SRP5_SAMPLING_DELAY_MS *
					       SRP5_STABLE_SAMPLES));
	return 0;
}

/**
 * sr_classp5_enable() - class enable for a voltage domain
 * @sr:			SR to enable
 *
 * When this gets called, we use the h/w loop to setup our voltages
 * to an calibrated voltage.
 *
 * NOTE: Appropriate locks must be held by calling path to ensure mutual
 * exclusivity
 */
static int sr_classp5_enable(struct omap_sr *sr)
{
	int res;
	struct omap_volt_data *volt_data = NULL;
	struct voltagedomain *voltdm = NULL;
	struct sr_classp5_calib_data *work_data = NULL;

	if (IS_ERR_OR_NULL(sr) || IS_ERR_OR_NULL(sr->voltdm)) {
		pr_err("%s: bad parameters!\n", __func__);
		return -EINVAL;
	}

	work_data = (struct sr_classp5_calib_data *)sr->voltdm_cdata;
	if (IS_ERR_OR_NULL(work_data)) {
		pr_err("%s: bad work data %s\n", __func__, sr->name);
		return -EINVAL;
	}

	if (is_idle_task(current))
		return 0;

	voltdm = sr->voltdm;
	volt_data = omap_voltage_get_curr_vdata(voltdm);
	if (IS_ERR_OR_NULL(volt_data)) {
		pr_err("%s: Voltage data is NULL. Cannot enable %s\n",
		       __func__, sr->name);
		return -ENODATA;
	}

	/*
	 * We are resuming from OFF - enable clocks manually to allow OFF-mode.
	 * Clocks will be disabled at "complete" stage by PM Core
	 */
	if (sr->suspended) {
		if (!volt_data->volt_calibrated || work_data->work_active)
			/* !!! Should never ever be here !!!*/
			WARN(true, "Trying to resume with invalid AVS state\n");
		else
			sr->ops->get(sr);
	}

	/* We donot expect work item to be active here */
	WARN_ON(work_data->work_active);

	/* If already calibrated, nothing to do here.. */
	if (volt_data->volt_calibrated)
		return 0;

	res = sr_classp5_start_hw_loop(sr);

	/*
	 * Calibrate Voltage on first switch to OPP, don't calibrate if called
	 * from system - wide suspend/resume path - it is not allowed to deal
	 * with pm_qos and work scheduling in this case
	 */
	if (!res && !volt_data->volt_calibrated && !sr->suspended)
		sr_classp5_calibration_schedule(sr);

	return res;
}

/**
 * sr_classp5_disable() - disable for a voltage domain
 * @sr: SmartReflex module, which need to be disabled
 * @is_volt_reset: reset the voltage?
 *
 * This function has the necessity to either disable SR alone OR disable SR
 * and reset voltage to appropriate level depending on is_volt_reset parameter.
 *
 * NOTE: Appropriate locks must be held by calling path to ensure mutual
 * exclusivity
 */
static int sr_classp5_disable(struct omap_sr *sr, int is_volt_reset)
{
	struct voltagedomain *voltdm = NULL;
	struct omap_volt_data *volt_data = NULL;
	struct sr_classp5_calib_data *work_data = NULL;

	if (IS_ERR_OR_NULL(sr) || IS_ERR_OR_NULL(sr->voltdm)) {
		pr_err("%s: bad parameters!\n", __func__);
		return -EINVAL;
	}

	work_data = (struct sr_classp5_calib_data *)sr->voltdm_cdata;
	if (IS_ERR_OR_NULL(work_data)) {
		pr_err("%s: bad work data %s\n", __func__, sr->name);
		return -EINVAL;
	}

	if (is_idle_task(current)) {
		/*
		 * we should not have seen this path if calibration !complete
		 * pm_qos constraint is already released after voltage
		 * calibration work is finished
		 */
		WARN_ON(work_data->work_active);

		return 0;
	}

	/* Rest is regular DVFS path */

	voltdm = sr->voltdm;
	volt_data = omap_voltage_get_curr_vdata(voltdm);
	if (IS_ERR_OR_NULL(volt_data)) {
		pr_warning("%s: Voltage data is NULL. Cannot disable %s\n",
			   __func__, sr->name);
		return -ENODATA;
	}

	/* need to do rest of code ONLY if required */
	if (volt_data->volt_calibrated && !work_data->work_active) {
		/*
		 * We are going OFF - disable clocks manually to allow OFF-mode.
		 */
		if (sr->suspended)
			sr->ops->put(sr);
		return 0;
	}

	if (work_data->work_active) {
		/* flag work is dead and remove the old work */
		work_data->work_active = false;
		cancel_delayed_work_sync(&work_data->work);
		sr_notifier_control(sr, false);
	}

	sr_classp5_stop_hw_loop(sr);

	if (is_volt_reset)
		voltdm_reset(sr->voltdm);

	/* Canceled SR, so no more need to keep request */
	pm_qos_update_request(&work_data->qos, PM_QOS_DEFAULT_VALUE);

	/*
	 * We are going OFF - disable clocks manually to allow OFF-mode.
	 */
	if (sr->suspended) {
		/* !!! Should never ever be here - no guarantee to recover !!!*/
		WARN(true, "Trying to go OFF with invalid AVS state\n");
		sr->ops->put(sr);
	}

	return 0;
}

/**
 * sr_classp5_configure() - configuration function
 * @sr:	SmartReflex module to be configured
 *
 * we dont do much here other than setup some registers for
 * the sr module involved.
 */
static int sr_classp5_configure(struct omap_sr *sr)
{
	if (IS_ERR_OR_NULL(sr) || IS_ERR_OR_NULL(sr->voltdm)) {
		pr_err("%s: bad parameters!\n", __func__);
		return -EINVAL;
	}

	return sr_configure_errgen(sr);
}

/**
 * sr_classp5_init() - class p5 init
 * @sr:			SR to init
 * @class_priv_data:	private data for the class (unused)
 *
 * we do class specific initialization like creating sysfs/debugfs entries
 * needed, spawning of a kthread if needed etc.
 */
static int sr_classp5_init(struct omap_sr *sr, void *class_priv_data)
{
	void **voltdm_cdata = NULL;
	struct sr_classp5_calib_data *work_data = NULL;

	if (IS_ERR_OR_NULL(sr) || IS_ERR_OR_NULL(sr->voltdm)) {
		pr_err("%s: bad parameters!\n", __func__);
		return -EINVAL;
	}

	voltdm_cdata = &sr->voltdm_cdata;
	if (*voltdm_cdata) {
		pr_err("%s: ooopps.. class already initialized for %s! bug??\n",
		       __func__, sr->name);
		return -EINVAL;
	}

	/* setup our work params */
	work_data = kzalloc(sizeof(struct sr_classp5_calib_data), GFP_KERNEL);
	if (!work_data) {
		pr_err("%s: no memory to allocate work data on domain %s\n",
		       __func__, sr->name);
		return -ENOMEM;
	}

	work_data->sr = sr;

	INIT_DELAYED_WORK_DEFERRABLE(&work_data->work, sr_classp5_calib_work);
	*voltdm_cdata = (void *)work_data;
	pm_qos_add_request(&work_data->qos, PM_QOS_CPU_DMA_LATENCY,
			   PM_QOS_DEFAULT_VALUE);

	return 0;
}

/**
 * sr_classp5_deinit() - class p5 deinitialization
 * @sr:		SR to deinit
 * @class_priv_data: class private data for deinitialiation (unused)
 *
 * currently only resets the calibrated voltage forcing DVFS voltages
 * to be used in the system
 *
 * NOTE: Appropriate locks must be held by calling path to ensure mutual
 * exclusivity
 */
static int sr_classp5_deinit(struct omap_sr *sr, void *class_priv_data)
{
	struct voltagedomain *voltdm = NULL;
	void **voltdm_cdata = NULL;
	struct sr_classp5_calib_data *work_data = NULL;

	if (IS_ERR_OR_NULL(sr) || IS_ERR_OR_NULL(sr->voltdm)) {
		pr_err("%s: bad parameters!\n", __func__);
		return -EINVAL;
	}

	voltdm = sr->voltdm;
	voltdm_cdata = &sr->voltdm_cdata;

	if (IS_ERR_OR_NULL(*voltdm_cdata)) {
		pr_err("%s: ooopps.. class not initialized for %s! bug??\n",
		       __func__, voltdm->name);
		return -EINVAL;
	}
	work_data = (struct sr_classp5_calib_data *)*voltdm_cdata;

	/*
	 * we dont have SR periodic calib anymore.. so reset calibs
	 * we are already protected by appropriate locks, so no lock needed
	 * here.
	 */
	if (work_data->work_active)
		sr_classp5_disable(sr, 0);

	/* Ensure worker canceled. */
	cancel_delayed_work_sync(&work_data->work);
	omap_voltage_calib_reset(voltdm);
	voltdm_reset(voltdm);
	pm_qos_remove_request(&work_data->qos);

	kfree(work_data);
	*voltdm_cdata = NULL;

	return 0;
}

/**
 * sr_classp5_suspend_noirq() - class suspend_noirq handler
 * @sr:	SmartReflex module which is moving to suspend
 *
 * The purpose of suspend_noirq handler is to make sure that Calibration
 * works are canceled before moving to OFF mode.
 * Otherwise these works may be executed at any moment, trigger
 * SmartReflex and race with CPU Idle notifiers. As result - system
 * will crash
 */
static int sr_classp5_suspend_noirq(struct omap_sr *sr)
{
	struct sr_classp5_calib_data *work_data;
	struct omap_volt_data *volt_data;
	struct voltagedomain *voltdm;
	int ret = 0;

	if (IS_ERR_OR_NULL(sr)) {
		pr_err("%s: bad parameters!\n", __func__);
		return -EINVAL;
	}

	work_data = (struct sr_classp5_calib_data *)sr->voltdm_cdata;
	if (IS_ERR_OR_NULL(work_data)) {
		pr_err("%s: bad work data %s\n", __func__, sr->name);
		return -EINVAL;
	}

	/*
	 * At suspend_noirq the code isn't needed to be protected by
	 * omap_dvfs_lock, but - Let's be paranoid (may have smth on other CPUx)
	 */
	mutex_lock(&omap_dvfs_lock);

	voltdm = sr->voltdm;
	volt_data = omap_voltage_get_curr_vdata(voltdm);
	if (IS_ERR_OR_NULL(volt_data)) {
		pr_warning("%s: Voltage data is NULL. Cannot disable %s\n",
			   __func__, sr->name);
		ret = -ENODATA;
		goto finish_suspend;
	}


	/*
	 * Check if calibration is active at this moment if yes -
	 * abort suspend.
	 */
	if (work_data->work_active) {
		pr_warn("%s: %s Calibration is active, abort suspend (Vnom=%u)\n",
			__func__, sr->name, volt_data->volt_nominal);
		ret = -EBUSY;
		goto finish_suspend;
	}

	/*
	 * Check if current voltage is calibrated if no -
	 * abort suspend.
	 */
	if (!volt_data->volt_calibrated) {
		pr_warn("%s: %s Calibration hasn't been done, abort suspend  (Vnom=%u)\n",
			__func__, sr->name, volt_data->volt_nominal);
		ret = -EBUSY;
		goto finish_suspend;
	}

	/* Let's be paranoid - cancel Calibration work manually */
	cancel_delayed_work_sync(&work_data->work);
	work_data->work_active = false;

finish_suspend:
	mutex_unlock(&omap_dvfs_lock);
	return ret;
}

/**
 * sr_classp5_resume_noirq() - class resume_noirq handler
 * @sr:	SmartReflex module which is moving to resume
 *
 * The main purpose of resume handler is to
 * reschedule calibration work if the resume has been started with
 * un-calibrated voltage.
 */
static int sr_classp5_resume_noirq(struct omap_sr *sr)
{
	struct omap_volt_data *volt_data;

	/*
	* If we resume from suspend and voltage is not calibrated, it means
	* that calibration work was not completed after re-calibration work
	* (not scheduled at all or canceled due to suspend or un-calibrated
	* OPP was selected while suspending/resuming), so reschedule
	* it here.
	*
	* At this stage the code isn't needed to be protected by
	* omap_dvfs_lock, but - Let's be paranoid (may have smth on other CPUx)
	*
	* More over, we shouldn't be here in case if voltage is un-calibrated
	* voltage, so produce warning.
	*/
	mutex_lock(&omap_dvfs_lock);
	volt_data = omap_voltage_get_curr_vdata(sr->voltdm);
	if (!volt_data->volt_calibrated) {
		sr_classp5_calibration_schedule(sr);
		WARN(true, "sr_classp5: Resume with un-calibrated voltage\n");
	}
	mutex_unlock(&omap_dvfs_lock);

	return 0;
}

/* SR class structure */
static struct omap_sr_class_data classp5_data = {
	.enable = sr_classp5_enable,
	.disable = sr_classp5_disable,
	.configure = sr_classp5_configure,
	.init = sr_classp5_init,
	.deinit = sr_classp5_deinit,
	.notify = sr_classp5_notify,
	.class_type = SR_CLASS1P5,
	.suspend_noirq = sr_classp5_suspend_noirq,
	.resume_noirq = sr_classp5_resume_noirq,
	/*
	 * trigger for bound - this tells VP that SR has a voltage
	 * change. we should try and ensure transdone is set before reading
	 * vp voltage.
	 */
	.notify_flags = SR_NOTIFY_MCUBOUND,
};

static int __init sr_classp5_driver_init(void)
{
	int ret = -EINVAL;
	char *name = "1.5";

	/* Enable this class only for OMAP3630 and OMAP4, OMAP5 */
	if (!(cpu_is_omap3630() || cpu_is_omap44xx() || cpu_is_omap54xx()))
		return -EINVAL;

	ret = sr_register_class(&classp5_data);
	if (ret) {
		pr_err("SmartReflex Class %s:"
		       "failed to register with %d\n", name, ret);
	} else {
		sr_classp5_recal_init();
		pr_info("SmartReflex Class %s initialized\n", name);
	}
	return ret;
}
subsys_initcall(sr_classp5_driver_init);
