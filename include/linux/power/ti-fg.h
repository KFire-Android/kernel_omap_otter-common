/*
 * include/linux/power/ti-fg.h
 *
 * TI Fuel Gauge header
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

#ifndef __TI_MIS_H__
#define __TI_MIS_H__

#include <linux/time.h>

/* Fuel Gauge Constatnts */
#define MAX_CAPACITY		0x7fff
#define MAX_SOC			100
#define MAX_PERCENTAGE		100

/* Num, cycles with no Learning, after this many cycles, the gauge
   start adjusting FCC, based on Estimated Cell Degradation */
#define NO_LEARNING_CYCLES	25

/* Size of the OCV Lookup table */
#define OCV_TABLE_SIZE		21

/* OCV Configuration */
struct ocv_config {
	unsigned char voltage_diff;
	unsigned char current_diff;

	unsigned short sleep_enter_current;
	unsigned char sleep_enter_samples;

	unsigned short sleep_exit_current;
	unsigned char sleep_exit_samples;

	unsigned short long_sleep_current;

	unsigned int ocv_period;
	unsigned int relax_period;

	unsigned char flat_zone_low;
	unsigned char flat_zone_high;

	unsigned short max_ocv_discharge;

	unsigned short table[OCV_TABLE_SIZE];
};

/* EDV Point */
struct edv_point {
	short voltage;
	unsigned char percent;
};

/* EDV Point tracking data */
struct edv_state {
	short voltage;
	unsigned char percent;
	short min_capacity;
	unsigned char edv_cmp;
};

/* EDV Configuration */
struct edv_config {
	bool averaging;

	unsigned char seq_edv;

	unsigned char filter_light;
	unsigned char filter_heavy;
	short overload_current;

	struct edv_point edv[3];
};

/* General Battery Cell Configuration */
struct cell_config {
	bool cc_polarity;
	bool cc_out;
	bool ocv_below_edv1;

	short cc_voltage;
	short cc_current;
	unsigned char cc_capacity;
	unsigned char seq_cc;

	unsigned short design_capacity;
	short design_qmax;

	unsigned char r_sense;

	unsigned char qmax_adjust;
	unsigned char fcc_adjust;

	unsigned short max_overcharge;
	unsigned short electronics_load; /* *10uAh */

	short max_increment;
	short max_decrement;
	unsigned char low_temp;
	unsigned short deep_dsg_voltage;
	unsigned short max_dsg_estimate;
	unsigned char light_load;
	unsigned short near_full;
	unsigned short cycle_threshold;
	unsigned short recharge;

	unsigned char mode_switch_capacity;

	unsigned char call_period;

	struct ocv_config *ocv;
	struct edv_config *edv;
};

/* Cell State */
struct cell_state {
	short soc;

	short nac;

	short fcc;
	short qmax;

	short voltage;
	short av_voltage;
	short cur;
	short av_current;

	short temperature;
	short cycle_count;

	bool sleep;
	bool relax;

	bool chg;
	bool dsg;

	bool edv0;
	bool edv1;
	bool edv2;
	bool ocv;
	bool cc;
	bool full;

	bool vcq;
	bool vdq;
	bool init;

	struct timeval last_correction;
	struct timeval last_ocv;
	struct timeval sleep_timer;
	struct timeval el_timer;
	unsigned int cumulative_sleep;

	short prev_soc;
	short learn_q;
	unsigned short dod_eoc;
	short learn_offset;
	unsigned short learned_cycle;
	short new_fcc;
	short ocv_total_q;
	short ocv_enter_q;
	short negative_q;
	short overcharge_q;
	short charge_cycle_q;
	short discharge_cycle_q;
	short cycle_q;
	short top_off_q;
	unsigned char seq_cc_voltage;
	unsigned char seq_cc_current;
	unsigned char sleep_samples;
	unsigned char seq_edvs;

	unsigned int electronics_load;
	unsigned short cycle_dsg_estimate;

	struct edv_state edv;

	bool updated;
	bool calibrate;

	struct cell_config *config;
	struct device *dev;

	int *charge_status;
};

#endif
