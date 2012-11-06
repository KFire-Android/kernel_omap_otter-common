/*
 * linux/drivers/power/fg/fg_edv.c
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
#include <linux/device.h>
#include <linux/power/ti-fg.h>

#include "fg_math.h"
#include "fg_edv.h"

/* Current that that is used for the EDV */
#define EDV_CURRENT	(cell->config->edv->averaging ? \
				cell->av_current : \
				cell->cur)

/* Voltage that that is used for the EDV */
#define EDV_VOLTAGE	(cell->config->edv->averaging ? \
				cell->av_voltage : \
				cell->voltage)


/* Init EDV point */
void fg_init_edv(struct cell_state *cell, struct edv_point *edv)
{
	if ((cell->edv.percent == edv->percent) && cell->init)
		return;

	cell->edv.voltage = edv->voltage;
	cell->edv.percent = edv->percent;

	cell->edv.min_capacity = DIV_ROUND_CLOSEST(cell->fcc * edv->percent,
							MAX_PERCENTAGE);
}



/*
 * Updates EDV0, EDV1, EDV2 falgs according to SOC. Except when EDV is
 * working
 */
void fg_update_edv_flags(struct cell_state *cell)
{
	unsigned char soc;

	soc = DIV_ROUND_CLOSEST(cell->nac * MAX_PERCENTAGE, cell->fcc);

	if (soc > cell->config->edv->edv[2].percent) {
		cell->edv2 = false;
		cell->edv1 = false;
		cell->edv0 = false;
		fg_init_edv(cell, &cell->config->edv->edv[2]);
	} else if (((soc < cell->config->edv->edv[2].percent) || !cell->init)
			&& (soc > cell->config->edv->edv[1].percent)) {
		/* If soc == EDV2 at init, start looking for EDV1 */
		cell->edv2 = true;
		cell->edv1 = false;
		cell->edv0 = false;
		cell->vdq  = false;
		fg_init_edv(cell, &cell->config->edv->edv[1]);
	} else if ((soc < cell->config->edv->edv[1].percent) || !cell->init) {
		/* If soc == EDV1 at init, start looking for EDV0 */
		cell->edv2 = true;
		cell->edv1 = true;
		cell->vdq  = false;

		if (soc > cell->config->edv->edv[0].percent)
			cell->edv0 = false;

		if (cell->edv.percent != cell->config->edv->edv[0].percent)
			fg_init_edv(cell, &cell->config->edv->edv[0]);
	}

}


/* Test EDV point, against current discharge settings and conditions */
short fg_test_edv(struct cell_state *cell)
{
	bool bad_load = 0;

	/* Checking if Current is Bad (Too Low, ot Too High) */
	bad_load |= -cell->cur < (short) cell->config->light_load;
	bad_load |= -cell->cur > cell->config->edv->overload_current;

	/* Checking if capacity droped below EDV%, restoring if so */
	if (cell->nac < cell->edv.min_capacity) {
		dev_dbg(cell->dev, "NAC(%d) < EDV Capacity(%d)\n",
			cell->nac, cell->edv.min_capacity);
		if (bad_load && cell->vdq) {
			cell->vdq = false;
			dev_dbg(cell->dev,
				"LRN: Current out of qualification range"
				"Disqual. %d < Cur < %d\n",
				cell->config->light_load,
				cell->config->edv->overload_current);
		}
		cell->nac = cell->edv.min_capacity;
	}

	/* If Voltage is under EDV Voltage, reporting EDV */
	if (EDV_VOLTAGE <= cell->edv.voltage) {
		dev_dbg(cell->dev, "Voltage %dmV <= EDV %dmV\n",
			EDV_VOLTAGE, cell->edv.voltage);
		/* If VDQ, checking if it needs to be cleared */
		if (bad_load && cell->vdq) {
			cell->vdq = false;
			dev_dbg(cell->dev,
				"LRN: Current out of qualification range"
				"Disqual. %d < Cur < %d\n",
				cell->config->light_load,
				cell->config->edv->overload_current);
		}

		return true;
	}

	return false;
}


/*
 * EDV targeting routine, that tests EDV point and Adjusts a capacity, based
 * on the Voltage and NAC
 */
bool fg_look_for_edv(struct cell_state *cell)
{
	if (fg_test_edv(cell)) {
		cell->seq_edvs++;
		dev_dbg(cell->dev, "EDV: EDV detected #%d\n", cell->seq_edvs);
	} else {
		cell->seq_edvs = 0;
	}


	if (cell->seq_edvs > cell->config->edv->seq_edv) {
		/* EDV point is reached */
		cell->seq_edvs = 0;
		dev_dbg(cell->dev, "EDV: EDV reached (%dmV, %dmAh)\n",
			EDV_VOLTAGE, cell->edv.min_capacity);

		/* Let's check whether discharge cycle is still useable
		   for learning */
		if ((short) cell->voltage <
			(short) (cell->edv.voltage -
				cell->config->deep_dsg_voltage)) {
			dev_dbg(cell->dev, "Learning Cycle isn't qualified\n");
			cell->vdq = false;
		}

		/* Set capacity to the EDV% Level */
		if (cell->nac > cell->edv.min_capacity)
			cell->nac = cell->edv.min_capacity;

		return 1;
	}

	return 0;
}


/*
 * EDV algorithm, should be called on discharge, adjusts NAC, based
 * on the discharge conditions. Also set EDV2, 1, 0 flags if needed
 */
void fg_edv(struct cell_state *cell)
{
	/* Check if EDV condition reached and we are above EDV0*/
	if (!cell->edv0 && fg_look_for_edv(cell)) {
		/* EDV2 point reached */
		if (!cell->edv2) {

			/* todo : recalibrate? */

			cell->edv2 = true;

			if (cell->vdq) {
				dev_dbg(cell->dev, "LRN: Learned Capacity ="
					" %d + %d + %d + %d mAh\n",
					-cell->learn_q,
					cell->ocv_total_q,
					cell->learn_offset,
					cell->nac);

				fg_learn_capacity(cell,
					-cell->learn_q +
					cell->ocv_total_q +
					cell->learn_offset +
					cell->nac);
			}

			cell->vdq = false;
			fg_init_edv(cell, &cell->config->edv->edv[1]);
		} else if (!cell->edv1) {
			/* EDV1 point reached */
			cell->edv1 = true;
			fg_init_edv(cell, &cell->config->edv->edv[0]);
		} else {
			/* EDV0 point reached */
			cell->edv0 = true;
		}
	}
}
