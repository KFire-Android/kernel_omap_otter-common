/*
 * Charger driver for TI BQ27541
 *
 * Copyright (C) Quanta Computer Inc. All rights reserved.
 *  Eric Nien <Eric.Nien@quantatw.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __BQ27541_BATTERY_PLATFORM_DATA_H_
#define __BQ27541_BATTERY_PLATFORM_DATA_H_

struct bq27541_battery_platform_data {
	void (*led_callback)(u8 green_value, u8 orange_value);
};

#endif /* __BQ27541_BATTERY_PLATFORM_DATA_H_ */
