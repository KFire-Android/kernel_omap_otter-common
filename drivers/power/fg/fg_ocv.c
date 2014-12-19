/*
 * linux/drivers/power/fg/fg_ocv.c
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

#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/time.h>
#include <linux/device.h>
#include <linux/power/ti-fg.h>

#include "fg_ocv.h"
#include "fg_math.h"


/* OCV Lookup table */
#define INTERPOLATE_MAX		1000
unsigned short interpolate(unsigned short value,
				unsigned short *table,
				unsigned char size)
{
	unsigned char i;
	unsigned short d;

	for (i = 0; i < size; i++)
		if (value < table[i])
			break;

	if ((i > 0)  && (i < size)) {
		d = (value - table[i-1]) * (INTERPOLATE_MAX/(size-1));
		d /=  table[i] - table[i-1];
		d = d + (i-1) * (INTERPOLATE_MAX/(size-1));
	} else {
		d = i * DIV_ROUND_CLOSEST(INTERPOLATE_MAX, size);
	}

	if (d > 1000)
		d = 1000;

	return d;
}


/*
 * Open Circuit Voltage (OCV) correction routine. This function estimates SOC,
 * based on the voltage.
 */
void fg_ocv(struct cell_state *cell)
{
	int tmp;
	dev_dbg(cell->dev, "FG: OCV Correction\n");

	/* Reset EL counter */
	cell->electronics_load = 0;
	cell->cumulative_sleep = 0;

	tmp = interpolate(cell->av_voltage, cell->config->ocv->table,
		OCV_TABLE_SIZE);

	cell->soc = DIV_ROUND_CLOSEST(tmp * MAX_PERCENTAGE, INTERPOLATE_MAX);
	cell->nac = DIV_ROUND_CLOSEST(tmp * cell->fcc, INTERPOLATE_MAX);

	if (!cell->ocv && cell->init) {
		cell->ocv = true;
		cell->ocv_enter_q = cell->nac;
		dev_dbg(cell->dev, "LRN: Entering OCV, OCVEnterQ = %dmAh\n",
			cell->ocv_enter_q);
	}

	do_gettimeofday(&cell->last_ocv);
}


/* Check if the cell is in Sleep */
bool fg_check_relaxed(struct cell_state *cell)
{
	struct timeval now;
	do_gettimeofday(&now);

	if (!cell->sleep) {
		if (abs(cell->cur) <=
			cell->config->ocv->sleep_enter_current) {

			if (cell->sleep_samples < MAX_UINT8)
				cell->sleep_samples++;

			if (cell->sleep_samples >=
				cell->config->ocv->sleep_enter_samples) {
				/* Entering sleep mode */
				cell->sleep_timer.tv_sec = now.tv_sec;
				cell->el_timer.tv_sec = now.tv_sec;
				cell->sleep = true;
				dev_dbg(cell->dev, "Sleeping\n");
				cell->calibrate = true;
			}
		} else {
			cell->sleep_samples = 0;
		}
	} else {
		/* The battery cell is Sleeping, checking if need to exit
		   sleep mode count number of seconds that cell spent in
		   sleep */
		cell->cumulative_sleep += now.tv_sec - cell->el_timer.tv_sec;
		cell->el_timer.tv_sec = now.tv_sec;

		/* Check if we need to reset Sleep */
		if (abs(cell->av_current) >
			cell->config->ocv->sleep_exit_current) {

			if (abs(cell->cur) >
				cell->config->ocv->sleep_exit_current) {

				if (cell->sleep_samples < MAX_UINT8)
					cell->sleep_samples++;

			} else {
				cell->sleep_samples = 0;
			}

			/* Check if we need to reset a Sleep timer */
			if (cell->sleep_samples >
				cell->config->ocv->sleep_exit_samples) {
				/* Exit sleep mode */
				cell->sleep_timer.tv_sec = 0;
				cell->sleep = false;
				cell->relax = false;
				dev_dbg(cell->dev,
					"Not relaxed and not sleeping\n");
			}
		} else {
			cell->sleep_samples = 0;

			if (!cell->relax) {

				if (now.tv_sec-cell->sleep_timer.tv_sec >
					cell->config->ocv->relax_period) {

					cell->relax = true;
					dev_dbg(cell->dev, "Relaxed\n");
					cell->calibrate = true;
				}
			}
		}
	}

	return cell->relax;
}
