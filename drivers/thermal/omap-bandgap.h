/*
 * OMAP4 Bandgap temperature sensor driver
 *
 * Copyright (C) 2011 Texas Instruments Incorporated - http://www.ti.com/
 * Contact:
 *   Eduardo Valentin <eduardo.valentin@ti.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 */
#ifndef __OMAP_BANDGAP_H
#define __OMAP_BANDGAP_H

struct omap_bandgap_data;

/**
 * struct omap_bandgap - bandgap device structure
 * @dev: device pointer
 * @pdata: platform data with sensor data
 * @fclock: pointer to functional clock of temperature sensor
 * @div_clk: pointer to parent clock of temperature sensor fclk
 * @conv_table: Pointer to adc to temperature conversion table
 * @bg_mutex: Mutex for sysfs, irq and PM
 * @irq: MPU Irq number for thermal alert
 * @tshut_gpio: GPIO where Tshut signal is routed
 * @clk_rate: Holds current clock rate
 */
struct omap_bandgap {
	struct device			*dev;
	const struct omap_bandgap_data	*pdata;
	struct clk			*fclock;
	struct clk			*div_clk;
	int				*conv_table;
	struct mutex			bg_mutex; /* Mutex for irq and PM */
	int				irq;
	int				tshut_gpio;
	u32				clk_rate;
};

int omap_bandgap_read_thot(struct omap_bandgap *bg_ptr, int id, int *thot);
int omap_bandgap_write_thot(struct omap_bandgap *bg_ptr, int id, int val);
int omap_bandgap_read_tcold(struct omap_bandgap *bg_ptr, int id, int *tcold);
int omap_bandgap_write_tcold(struct omap_bandgap *bg_ptr, int id, int val);
int omap_bandgap_read_update_interval(struct omap_bandgap *bg_ptr, int id,
				      int *interval);
int omap_bandgap_write_update_interval(struct omap_bandgap *bg_ptr, int id,
				       u32 interval);
int omap_bandgap_read_temperature(struct omap_bandgap *bg_ptr, int id,
				  int *temperature);

#endif
