/*
 * Copyright (C) 2010 Texas Instruments
 * Author: Balaji T K
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _LINUX_BQ2415X_I2C_H
#define _LINUX_BQ2415X_I2C_H

#define BQ2415x_START_CHARGING		(1 << 0)
#define BQ2415x_STOP_CHARGING		(1 << 1)
#define BQ2415x_CHARGER_FAULT		(1 << 2)

#define BQ2415x_CHARGE_DONE		0x20
#define BQ2415x_FAULT_VBUS_OVP		0x31
#define BQ2415x_FAULT_SLEEP		0x32
#define BQ2415x_FAULT_BAD_ADAPTOR	0x33
#define BQ2415x_FAULT_BAT_OVP		0x34
#define BQ2415x_FAULT_THERMAL_SHUTDOWN	0x35
#define BQ2415x_FAULT_TIMER		0x36
#define BQ2415x_FAULT_NO_BATTERY	0x37

/* not a bq generated event,we use this to reset the
 * the timer from the twl driver.
 */
#define BQ2415x_RESET_TIMER		0x38

struct bq2415x_platform_data {
	int max_charger_currentmA;
	int max_charger_voltagemV;
	int termination_currentmA;
};

#endif
