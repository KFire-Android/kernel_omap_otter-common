/*
 * tps6130x-regulator.h
 *
 * Regulator driver for TPS6130x
 *
 * Copyright (C) 2011 Texas Instrument Incorporated - http://www.ti.com/
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation version 2.
 *
 * This program is distributed "as is" WITHOUT ANY WARRANTY of any kind,
 * whether express or implied; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 */

#ifndef __LINUX_REGULATOR_TPS6130X_H
#define __LINUX_REGULATOR_TPS6130X_H

/**
 * tps6130x_platform_data - platform data for tps6130x regulator
 * @enable: Put tps6130x in normal/reset mode (used for tps61301/tps61305)
 *	    If not provided, it's assumed chip is always enabled:
 *	    - tps61300/tps61306
 *	    - tps61301/tps61305 with NRESET forced to low by hardware
 */

struct tps6130x_platform_data {
	int (*chip_enable)(int on);
};

#endif
