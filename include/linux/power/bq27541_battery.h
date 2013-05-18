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

#ifndef __BQ27541_BATTERY_H_
#define __BQ27541_BATTERY_H_

/* Manufacturer */
#define MANU_UNKNOWN			-1
#define MANU_NOT_RECOGNIZE		-2
#define MANU_ATL			0
#define MANU_THM			1
#define MANU_SWE			2

#define BAT_NORMAL_STATE		0x00
#define BAT_CRITICAL_STATE		0x01

#define REG_CONTROL			0x00
#define REG_AT_RATE			0x02 // is a signed integer       ,Units are mA
#define REG_AT_RATE_TIME_TO_EMPTY	0x04 // an unsigned integer value ,Units are minutes with a range of 0 to 65,534.
#define REG_TEMP			0x06 // an unsigned integer value ,Units are units of 0.1K
#define REG_VOLT			0x08 // an unsigned integer value ,Units are mV with a range of 0 to 6000 mV
#define REG_FLAGS			0x0A
#define REG_NOMINAL_AVAILABLE_CAPACITY	0x0C // Units are mAh
#define REG_FULL_AVAILABLE_CHARGE	0x0E // Units are mAh
#define REG_REMAINING_CHARGE		0x10 // Units are mAh
#define REG_FULL_CHARGE			0x12 // Units are mAh
#define REG_AVERAGE_CURRENT		0x14 // a signed integer value ,Units are mA.
#define REG_TIME_TO_EMPTY		0x16 // a unsigned integer value ,Units are minutes.  A value of 65,535 indicates battery is not being discharged.
#define REG_TIME_TO_FULL		0x18 // a unsigned integer value ,Units are minutes.    A value of 65,535 indicates battery is not being charged..
#define REG_STANDBY_CURRENT		0x1A // a unsigned integer value
#define REG_STANDBY_TIME_TO_EMPTY	0x1C // a unsigned integer value ,Units are minutes.  A value of 65,535 indicates battery is not being discharged
#define REG_MAX_LOAD_CURRENT		0x1E // a signed integer valuee ,Units are mA.
#define REG_MAX_LOAD_TIME_TO_EMPTY	0x20 // an unsigned integer value ,Units are minutes. A value of 65,535 indicates battery is not being discharged
#define REG_AVAILABLE_ENERGY		0x22 // an unsigned integer value. Units are mWh.
#define REG_AVERAGE_POWER		0x24 // an unsigned integer value. Units are mW.A value of 0 indicates that the battery is not being discharged.
#define REG_TTO_EMPTYT_AT_CNSTNT_POWER	0x26 // an unsigned integer value. Units are minutes.A value of 0 indicates that the battery is not being discharged.

#define REG_CYCLE_COUNT			0x2A // an unsigned integer value.a range of 0 to 65,535
#define REG_STATE_OF_CHARGE		0x2c // Relative State-of-Charge with a range of 0 to 100%.

//Control( ) Subcommands
#define CONTROL_STATUS			0x00
#define DEVICE_TYPE			0x01
#define FW_VERSION			0x02
#define HW_VERSION			0x03
#define SET_FULLSLEEP			0x10
#define SET_HIBERNATE			0x11
#define CLEAR_HIBERNATE			0x12
#define SET_SHUTDOWN			0x13
#define CLEAR_SHUTDOWN			0x14
#define IT_ENABLE			0x21
#define CAL_MODE			0x40
#define RESET				0x41

#define BQ27x00_REG_TEMP		0x06
#define BQ27x00_REG_VOLT		0x08
#define BQ27x00_REG_AI			0x14
#define BQ27x00_REG_FLAGS		0x0A
#define BQ27000_REG_RSOC		0x0B // Relative State-of-Charge
#define BQ27x00_REG_TTE			0x16
#define BQ27x00_REG_TTF			0x18
#define BQ27x00_REG_TTECP		0x26
#define BQ27500_REG_SOC			0x2c

#define BQ27500_FLAG_DSC		BIT(0)
#define BQ27000_FLAG_CHGS		BIT(7)
#define BQ27500_FLAG_FC			BIT(9)
#define BQ27500_FLAG_OTD		BIT(14)
#define BQ27500_FLAG_OTC		BIT(15)

/* Manufacturer id check function */
#define BQ27541_DATAFLASHBLOCK		0x3f
#define BQ27541_BLOCKDATA		0x40

#define BQ27541_LONG_COUNT		6

/* Hard low battery protection */
#define HARD_LOW_VOLTAGE_THRESHOLD	3350000
#define RECHARGE_THRESHOLD		90

#ifdef CONFIG_PROC_FS
#define BQ_PROC_FILE			"driver/bq"
#define BQ_MAJOR			620
#endif

enum bat_events
{
	EVENT_UNDER_LOW_VOLTGAE=30,
	EVENT_BELOW_LOW_VOLTGAE,
};

struct bq27541_battery_platform_data {
	void (*led_callback)(u8 green_value, u8 orange_value);
};

#endif /* __BQ27541_BATTERY_H_ */
