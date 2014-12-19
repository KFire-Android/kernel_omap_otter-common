/*
 * linux/drivers/power/fg/fg.c
 *
 * TI Fuel Gauge driver for Linux
 *
 * Copyright (C) 2008-2009 Texas Instruments, Inc.
 * Author: Texas Instruments, Inc.
 *
 * This package is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * THIS PACKAGE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#include <linux/init.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/power/ti-fg.h>
#include <linux/power_supply.h>

#include "fg.h"
#include "fg_ocv.h"
#include "fg_edv.h"
#include "fg_math.h"

/* Voltage and Current buffers */
#define AV_SIZE	5

static short av_v[AV_SIZE];
static short av_c[AV_SIZE];

static unsigned short av_v_index;
static unsigned short av_c_index;

/*
 * Capacity Learning Routine. The FG records a capacity that was put in, or
 * taken out of the battery on both Charge or Discharge. If, at the end of
 * charge or discharge, the gauge decides to update FCC, based on the learned
 * capacity amount, the gaue calls this function.
 */
void fg_learn_capacity(struct cell_state *cell, unsigned short capacity)
{
	short learn_limit;

	/* Make sure no learning is still in progress */
	cell->vcq = false;
	cell->vdq = false;

	/* Check if Learning needs to be Disqualified */
	if (cell->temperature < cell->config->low_temp) {
		dev_dbg(cell->dev, "LRN: Learning disqual (Temp) %d < %d\n",
			cell->temperature,
			cell->config->low_temp);
		return;
	}

	if (abs(cell->ocv_total_q) > cell->config->ocv->max_ocv_discharge) {
		dev_dbg(cell->dev, "LRN: Learning disqual (OCV) %d > %d\n",
			cell->ocv_total_q,
			cell->config->ocv->max_ocv_discharge);
		return;
	}

	dev_dbg(cell->dev, "LRN: Learn Capacity = %dmAh\n", capacity);

	/* Learned Capacity is much lower, then expected */
	learn_limit = cell->fcc - cell->config->max_decrement;
	if ((short) capacity < learn_limit) {
		capacity = learn_limit;
		dev_dbg(cell->dev, "LRN: Capacity is LOW, limit to %4dmAh\n",
			capacity);
	} else {
		learn_limit = cell->fcc + cell->config->max_increment;
		if ((short) capacity > learn_limit) {
			/* Learned Capacity is much greater, then expected */
			capacity = learn_limit;
			dev_dbg(cell->dev,
				"LRN: Capacity is HIGH, limit to %4dmAh\n",
				capacity);
		} else {
			/* capacity within expected range */
			dev_dbg(cell->dev,
				"LRN: Capacity within range (%d < %d < %d)\n",
				cell->fcc - cell->config->max_decrement,
				capacity,
				cell->fcc + cell->config->max_increment);
		}
	}

	/* Reset No Learn Counter */
	cell->learned_cycle = cell->cycle_count;

	cell->new_fcc = capacity;

	/* We can update FCC here, only if charging is on */
	if (!cell->edv2) {
		dev_dbg(cell->dev,
			"LRN: FCC Updated, newFCC = %d, FCC = %d mAh\n",
			cell->new_fcc,
			cell->fcc);

		cell->fcc = cell->new_fcc;
		cell->updated = true;
	} else {
		dev_dbg(cell->dev,
			"LRN: FCC <- %dmAh, on next CHG\n", cell->new_fcc);
	}

}


/*
 * Counts cycles, based on the passed charge, and updates CycleCount EEPROM
 * setting, if cycle is detected
 */
static void fg_count_cycle(struct cell_state *cell, short delta_q)
{
	if (delta_q < 0) {

		cell->cycle_q -= delta_q;

		if (cell->cycle_q > cell->config->cycle_threshold) {

			/* Check how many cycles ago we learned FCC,
			   and adjust FCC accordingly */
			if ((cell->cycle_count - cell->learned_cycle) >=
				NO_LEARNING_CYCLES) {

				/* Reset Learn Cycle Tracker */
				cell->learned_cycle = cell->cycle_count;

				/* We are canceling learning */
				cell->vcq = false;
				cell->vdq = false;

				/* Adjust FCC */
				if (cell->fcc > cell->config->fcc_adjust)
					cell->fcc -= cell->config->fcc_adjust;
				else
					cell->fcc = 0;

				cell->updated = true;
			}

			cell->cycle_q -= cell->config->cycle_threshold;

			if (cell->cycle_count < MAX_INT) {
				cell->cycle_count++;
				cell->updated = true;
			}

			dev_dbg(cell->dev, "DSG %dmAh, CycleCount = %d\n",
				cell->config->cycle_threshold,
				cell->cycle_count);
		}
	}
}


/*
 * This is invoked when the charge is complete. The conarge complete condition
 * is configurable, and can be set to be an input from a charger or, the gauge
 * itself can detect charge complete condition.
 * On charge complete, if charge cycle is qualified, the gauge learns a
 * capacity.
 * This is can be invoked from either foreground, or background context.
 */
static void fg_charge_complete(struct cell_state *cell)
{
	dev_dbg(cell->dev, "CHG: Charge Complete Detected!\n");

	/* Set Charge Complete Flag and Toggle a CC Pin, if configured to */
	cell->cc = true;
	cell->calibrate = true;

	/* Check if we can Learn capacity on Charge */
	if (cell->vcq) {
		dev_dbg(cell->dev, "LRN: Learned Capacity = %d + %d + %d mAh\n",
			cell->learn_q,
			cell->learn_offset,
			-cell->ocv_total_q);

		fg_learn_capacity(cell,
			cell->learn_q + cell->learn_offset - cell->ocv_total_q);
	}

	dev_dbg(cell->dev, "CHG: Setting RM to FCC\n");

	/* Set Remaining Capacities to Full */
	cell->nac = cell->fcc;

	/* Capacity learning complete */
	cell->vcq = false;
}


/*
 * Check for Charge Complete condition:
 * (voltage > cc_voltage) AND
 * ((current > cc_current) OR (top_off_capacity>cc_capacity))
 */
static bool fg_check_chg_complete(struct cell_state *cell, short delta_q)
{
	bool ret = false;

	if (cell->voltage >= (short)cell->config->cc_voltage) {

		/* Check if stable (V > CC_V) reached */
		if (cell->seq_cc_voltage >= cell->config->seq_cc) {

			/* Check if CC_Cap reached */
			if (cell->top_off_q > (cell->fcc / 100)
				* cell->config->cc_capacity)
				ret = true;
			else
				cell->top_off_q += delta_q;

			/* Start looking for stable (C > CC_Cur) */
			if ((cell->cur <= (short)cell->config->cc_current)
				&& (cell->av_current <=
					(short)cell->config->cc_current)) {

					if (cell->seq_cc_current >=
						cell->config->seq_cc)
						ret = true;
					else
						cell->seq_cc_current++;
			} else
				cell->seq_cc_current = 0;

		} else
			cell->seq_cc_voltage++;

	} else {
		cell->seq_cc_voltage = 0;
		cell->seq_cc_current = 0;
		cell->top_off_q = 0;
	}

	dev_dbg(cell->dev, "CHGCPL: seq_v %d; seq_c %d; top_q %d",
		cell->seq_cc_voltage,
		cell->seq_cc_current,
		cell->top_off_q);

	return ret;
}


/*
 * Routine that tracks charge process. Should be called, whenever the cell
 * is being charged.
 */
static void fg_charging(struct cell_state *cell, short delta_q)
{
	/* the battery is chargeing so clear the discharge flag */
	if (!cell->chg) {
		cell->charge_cycle_q = 0;
		cell->seq_cc_voltage = 0;
		cell->seq_cc_current = 0;
		cell->top_off_q = 0;
		dev_dbg(cell->dev, "CHG: Starting Charge Cycle\n");
		cell->chg = true;
	}

	cell->charge_cycle_q += delta_q;

	/* Check if need to cancel/disqualify Discharge, or start learning */
	if (cell->charge_cycle_q > cell->config->mode_switch_capacity) {

		if (cell->dsg) {
			dev_dbg(cell->dev, "CHG: DSG cleared\n");
			cell->dsg = false;
		}

		if (cell->vdq) {
			dev_dbg(cell->dev, "CHG:, DSG Learning clearer\n");
			cell->vdq = false;
		}

		/* Check if we can do learning on this cycle */
		if (!cell->vcq && cell->edv2) {
			dev_dbg(cell->dev, "CHG: Start Learning\n");

			cell->learn_offset = cell->nac;
			cell->learn_q = 0;
			cell->ocv_enter_q = 0;
			cell->ocv_total_q = 0;
			cell->cycle_dsg_estimate = 0;
			cell->vcq = true;

			dev_dbg(cell->dev,
				"CHG: LearnQ = %d mAh, LearnOffset = %d mAh\n",
				cell->learn_q,
				cell->learn_offset);
		}

		/* Update FCC, if there is a need */
		if (cell->fcc != cell->new_fcc) {
			cell->fcc = cell->new_fcc;
			cell->updated = true;
			dev_dbg(cell->dev, "LRN: FCC <- %d mAh\n", cell->fcc);
		}
	}

	/* Check for Charge Complete Condition */
	if (!cell->cc
		&& cell->config->cc_out
		&& fg_check_chg_complete(cell, delta_q))
			fg_charge_complete(cell);
}


/*
 * Routine that tracks discharge process. Should be called, whenever the cell
 * is being discahrged.
 */
static void fg_discharging(struct cell_state *cell, short delta_q)
{
	/* Starting brand new Discharge Cycle */
	if (!cell->dsg) {
		dev_dbg(cell->dev, "DSG: Starting Discharge Cycle\n");
		cell->discharge_cycle_q = 0;
		cell->dsg = true;
	}

	cell->discharge_cycle_q -= delta_q;
	fg_count_cycle(cell, delta_q);

	/* Check if need to cancel/disqualify Charge, or start learning */
	if (cell->discharge_cycle_q > cell->config->mode_switch_capacity) {
		if (cell->chg) {
			dev_dbg(cell->dev,
				"DSG: CHG cleared due to Discharge Cycle\n");
			cell->chg = false;
		}

		if (cell->vcq) {
			dev_dbg(cell->dev,
				"DSG: VCQ cleared due to Discharge Cycle\n");
			cell->vcq = false;
		}

		/* Check if we can do learning on this cycle */
		if (!cell->vdq &&
			(cell->nac >= (cell->fcc - cell->config->near_full))) {
			dev_dbg(cell->dev, "DSG: Start Learning\n");
			cell->learn_offset = cell->fcc - cell->nac;
			cell->learn_q = 0;
			cell->ocv_enter_q = 0;
			cell->ocv_total_q = 0;
			cell->cycle_dsg_estimate = 0;
			cell->vdq = true;
			dev_dbg(cell->dev,
				"LearnQ = %d mAh, LearnOffset = %d mAh\n",
				cell->learn_q,
				cell->learn_offset);
		}
	}
}


/*
 * Initialize the battery cell state structure,
 * Setup initial parameters and timers
 * NOTE: make sure *cell is 0-filled!
 */
void fg_init(struct cell_state *cell, short voltage)
{
	unsigned short i;

	cell->fcc = cell->config->design_capacity;
	cell->qmax = cell->config->design_qmax;
	cell->new_fcc = cell->fcc;

	cell->voltage = voltage;
	cell->av_voltage = voltage;

	for (i = 0; i < AV_SIZE; i++) {
		av_v[i] = voltage;
		av_c[i] = 0;
	}
	av_v_index = 0;
	av_c_index = 0;

	cell->temperature = 200;
	cell->cycle_count = 1;

	cell->learned_cycle = cell->cycle_count;
	cell->prev_soc = -1;

	/* On init, get SOC from OCV */
	fg_ocv(cell);
	dev_dbg(cell->dev, "FG: Init (%dv, %dmAh, %d%%)\n",
		voltage, cell->nac, cell->soc);

	/* Update EDV flags */
	fg_update_edv_flags(cell);

	do_gettimeofday(&cell->last_correction);

	cell->init = true;
}


/* Increments accumulator with top (limit) and bottom (0) limiting */
static short fg_accumulate(short acc, short limit, short delta_q)
{
	acc += delta_q;

	if (acc < 0)
		return 0;
	if (acc > limit)
		return limit;
	return acc;
}


#ifdef DEBUG
/* Prints battery cell state flags */
static void print_flags(struct cell_state *cell)
{
	if (cell->dsg)
		dev_dbg(cell->dev, "DSG ");
	if (cell->chg)
		dev_dbg(cell->dev, "CHG ");
	if (cell->full)
		dev_dbg(cell->dev, "FULL ");
	if (cell->edv0)
		dev_dbg(cell->dev, "EDV0 ");
	if (cell->edv1)
		dev_dbg(cell->dev, "EDV1 ");
	if (cell->edv2)
		dev_dbg(cell->dev, "EDV2 ");
	if (cell->vcq)
		dev_dbg(cell->dev, "VCQ ");
	if (cell->vdq)
		dev_dbg(cell->dev, "VDQ ");
	if (cell->init)
		dev_dbg(cell->dev, "INIT ");
	if (cell->ocv)
		dev_dbg(cell->dev, "OCV ");
	if (cell->cc)
		dev_dbg(cell->dev, "CC ");
	if (cell->sleep)
		dev_dbg(cell->dev, "SLEEP ");
	if (cell->relax)
		dev_dbg(cell->dev, "RELAX ");
}
#endif


/* Checks for right conditions for OCV correction */
static bool fg_can_ocv(struct cell_state *cell)
{
	struct timeval now;
	int tmp;

	do_gettimeofday(&now);
	tmp = now.tv_sec - cell->last_ocv.tv_sec;

	/* Don't do OCV to often */
	if ((tmp < cell->config->ocv->ocv_period) && cell->init)
		return false;

	/* Voltage should be stable */
	if (cell->config->ocv->voltage_diff <= diff_array(av_v, AV_SIZE))
		return false;

	/* Current should be stable */
	if (cell->config->ocv->current_diff <= diff_array(av_c, AV_SIZE))
		return false;

	/* SOC should be out of Flat Zone */
	if ((cell->soc >= cell->config->ocv->flat_zone_low)
		&& (cell->soc <= cell->config->ocv->flat_zone_high))
			return false;

	/* Current should be less then SleepEnterCurrent */
	if (abs(cell->cur) >= cell->config->ocv->sleep_enter_current)
		return false;

	/* Don't allow OCV below EDV1, unless OCVbelowEDV1 is set */
	if (cell->edv1 && !cell->config->ocv_below_edv1)
		return false;

	return true;
}


/* Applies an estimated Electronic Load to the NAC */
static void fg_el(struct cell_state *cell)
{
	int el_delta;

	/* No EL correction, if changer is connected */
	if (*cell->charge_status == POWER_SUPPLY_STATUS_CHARGING) {
		cell->electronics_load = 0;
		cell->cumulative_sleep = 0;
		return;
	}

	dev_dbg(cell->dev, "FG: EL Correction\n");

	/* Increment Electronics Load */
	cell->electronics_load += cell->config->electronics_load
				* cell->cumulative_sleep;

	/* See if we have more then 1mAh: cell->electronics_load is in 10uAs.
	   We need to convert uAs to mAh: 10uAs = 1/100mAs, 1uAs = 1/3600mAh */
	el_delta = cell->electronics_load / 100 / 3600;

	if (el_delta > 0) {
		cell->electronics_load %= el_delta;

		/* first decrement overcharge capacity, if any */
		if (cell->overcharge_q > 0) {
			cell->overcharge_q -= el_delta;
			if (cell->overcharge_q < 0) {
				cell->nac += cell->overcharge_q;
				cell->learn_q += cell->overcharge_q;
				cell->overcharge_q = 0;
			}
		} else {
			/* decrement NAC, if no overcharge present */
			cell->nac -= el_delta;
			cell->learn_q -= el_delta;
		}

		if (cell->nac < 0)
			cell->nac = 0;

		/* count cycle self discharge */
		if (cell->cycle_dsg_estimate < MAX_UNSIGNED_INT)
			cell->cycle_dsg_estimate += el_delta;

		/* Disqualify learning, if too much self-discharge */
		if (cell->cycle_dsg_estimate > cell->config->max_dsg_estimate) {
			if (cell->vcq || cell->vdq) {
				dev_dbg(cell->dev,
					"Learning Disqualified, too much EL: "
					"%d > %d\n",
					cell->cycle_dsg_estimate,
					cell->config->max_dsg_estimate);

				cell->vcq = false;
				cell->vdq = false;
			}
		}
	}

	cell->cumulative_sleep = 0;
}


/*
 * Coulomb Counter (CC) correction routine. This function adjusts SOC,
 * based on the passed capacity, read from the CC.
 */
static void fg_cc(struct cell_state *cell, short delta_q)
{
	dev_dbg(cell->dev, "FG: CC Correction\n");

	/* Check if we are just exited OCV */
	if (cell->ocv) {
		cell->ocv = false;
		cell->ocv_total_q += cell->ocv_enter_q - cell->nac;
		dev_dbg(cell->dev,
			"FG: Exit OCV, OCVEnterQ = %dmAh, OCVTotalQ = %dmAh\n",
			cell->ocv_enter_q, cell->ocv_total_q);
	}

	/* See if we are under 0 relative capacity level */
	if ((cell->nac > 0) || cell->edv0
		|| ((cell->negative_q >= 0) && (delta_q > 0))) {

		/* Count Overcharge Capacity */
		if ((cell->nac == cell->fcc) && cell->cc)
			if ((cell->overcharge_q < cell->config->max_overcharge)
				|| (delta_q < 0))
					cell->overcharge_q += delta_q;

		if (cell->overcharge_q < 0)
			cell->overcharge_q = 0;

		/* Do not correct NAC, until Overcharge present */
		if (cell->overcharge_q <= 0) {
			cell->learn_q += delta_q;
			cell->nac = fg_accumulate(cell->nac, cell->fcc,
							delta_q);
		}

		cell->soc = DIV_ROUND_CLOSEST(cell->nac * MAX_PERCENTAGE,
						cell->fcc);
	} else {
		/* Wait untill we get to 0 level, then start counting */
		cell->negative_q += delta_q;
	}

	/* EDV adjustments, only if discharging */
	if (!cell->sleep)
		if (cell->cur < 0)
			fg_edv(cell);

	fg_el(cell);
}


/* Main FG entry point. This function needs to be called periodically.*/
void fg_process(struct cell_state *cell, short delta_q, short voltage,
		short cur, short temperature)
{
	int i, tmp;
	struct timeval now;

	if (!cell->init)
		return;

	/* Update voltage and add it to the buffer, update average*/
	tmp = 0;
	cell->voltage = voltage;
	av_v_index++;
	av_v_index %= AV_SIZE;
	av_v[av_v_index] = voltage;
	for (i = 0; i < AV_SIZE; i++)
		tmp += av_v[i];
	cell->av_voltage = tmp/AV_SIZE;

	/* Update current and add it to the buffer, update average*/
	tmp = 0;
	cell->cur = cur;
	av_c_index++;
	av_c_index %= AV_SIZE;
	av_c[av_c_index] = cur;
	for (i = 0; i < AV_SIZE; i++)
		tmp += av_c[i];
	cell->av_current = tmp/AV_SIZE;

	/* Update temperature*/
	cell->temperature = temperature;

	/* Check time since last_call */
	do_gettimeofday(&now);
	tmp = now.tv_sec - cell->last_correction.tv_sec;

	/* Check what capacity currection algorithm should we use: OCV or CC */
	if ((tmp > cell->config->ocv->relax_period)
		&& (abs(cell->cur) < cell->config->ocv->long_sleep_current)) {
			fg_ocv(cell);
	} else if (fg_check_relaxed(cell)) {
		/* We are not doing any active CHG/DSG, clear flags
		   this does not compromise learning cycles */
		cell->chg = false;
		cell->dsg = false;

		/* Checking if we can do an OCV correction */
		if (fg_can_ocv(cell))
			fg_ocv(cell);
		else
			fg_cc(cell, delta_q);
	} else /* Not Relaxed: actively charging or discharging */
		fg_cc(cell, delta_q);


	/* Charge / Discharge spesific functionality */
	if (!cell->sleep) {
		if (cell->cur > 0)
			fg_charging(cell, delta_q);
		else if (cell->cur < 0)
			fg_discharging(cell, delta_q);
	}

	/* Update Regular SOC */
	cell->soc = DIV_ROUND_CLOSEST(cell->nac * MAX_PERCENTAGE, cell->fcc);

	fg_update_edv_flags(cell);

	/* Check if battery is full */
	if (cell->nac >= cell->fcc) {
		cell->full = true;
	} else {
		cell->full = false;
		if (cell->nac <= (cell->fcc - cell->config->recharge))
			cell->cc = false;
	}

	/* Checking if we need to set an updated flag (is SOC changed) */
	if (cell->prev_soc != cell->soc) {
		cell->prev_soc = cell->soc;
		cell->updated = true;
	}

	cell->last_correction.tv_sec = now.tv_sec;

#ifdef DEBUG
	/* Printing Debug Data */
	dev_dbg(cell->dev,
		"FG: ACC;%2d ; RM;%4d;mAh ; SOC;%3d;%% ; VOLT;%4d;%4d;mV ; "
		"CUR;%5d;%5d;mA ; "
		"EDV;%dmV/%02d%% ; "
		"LLL;%4d;%4d;%4d ; "
		"N/O;%4d;%4d ; "
		"CS;%4d ; EL;%5d ; "
		"F/C;%d;%d ; CP;%d",
		delta_q, cell->nac, cell->soc, cell->voltage, cell->av_voltage,
		cell->cur, cell->av_current,
		cell->edv.voltage, cell->edv.percent,
		cell->learn_q, cell->learn_offset, cell->ocv_total_q,
		cell->negative_q, cell->overcharge_q,
		cell->cumulative_sleep, cell->electronics_load,
		cell->fcc, cell->cycle_count, tmp);

	print_flags(cell);
#endif
}
