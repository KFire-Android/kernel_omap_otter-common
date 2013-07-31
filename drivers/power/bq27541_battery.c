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
#include <linux/module.h>
#include <linux/param.h>
#include <linux/jiffies.h>
#include <linux/workqueue.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/idr.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <asm/unaligned.h>
#include <plat/led.h>
#include "kc1_summit/smb347.h"

#if defined(CONFIG_LAB126)
#if defined(CONFIG_EARLYSUSPEND)
#include <linux/earlysuspend.h>
#endif
#endif
#undef BAT_LOG

struct bq27541_info  {
    struct i2c_client           *bat_client;
    struct device               *dev;
    u8          		id;
    int			        temp;
    int			        ntc_temp;
    int			        voltage;
    int			        current_avg;
    int			        capacity;
    int			        status; 
    int			        state;
    int			        health;
    int			        sec;
    int			        weak_bat_count;
    int			        err_count; 
/*Manufacturer id check function */
    int			        manufacturer;
/*Manufacturer id check function */
/*Thermal event function */
    int			        temperature_step;
    struct power_supply	        bat;
    int			        disable_led;
    struct proc_dir_entry       *bq_proc_fs;
#ifdef BAT_LOG
    struct delayed_work         bat_log_work;
#endif
    struct delayed_work         bat_monitor_work;
	int			battery_polling;
	unsigned int		time_to_empty;
	unsigned int		time_to_full;
	unsigned int		cycle_count;
	unsigned int		average_power;
	unsigned int		available_energy;
	unsigned int		remaining_charge;
	unsigned int		remaining_charge_uncompensated;
	unsigned int		full_charge;
	unsigned int		full_charge_uncompensated;
	unsigned int		long_count;
	unsigned int		manufacturer_id_read_count;
#if defined(CONFIG_LAB126) && defined(CONFIG_EARLYSUSPEND)
	struct early_suspend	early_suspend;
#endif
};

enum bat_events
{
    EVENT_UNDER_LOW_VOLTGAE=30,
    EVENT_BELOW_LOW_VOLTGAE,
};

struct workqueue_struct    *bat_work_queue;
static struct blocking_notifier_head notifier_list;
/*Thermal event function */
static int g_full_available_capacity=4400;
static int fake_temp;static int fake_full_available_capacity;
/*Thermal event function */

#define UNKNOW         -1
#define NOT_RECOGNIZE  -2
#define ATL            0
#define THM            1
#define SWE            2

#define BAT_NORMAL_STATE                0x00
#define BAT_CRITICAL_STATE              0x01
#define REG_CONTROL                     0x00
#define REG_AT_RATE                     0x02//is a signed integer       ,Units are mA
#define REG_AT_RATE_TIME_TO_EMPTY       0x04//an unsigned integer value ,Units are minutes with a range of 0 to 65,534.

#define REG_TEMP                        0x06//an unsigned integer value ,Units are units of 0.1K
#define REG_VOLT                        0x08//an unsigned integer value ,Units are mV with a range of 0 to 6000 mV
#define REG_FLAGS                       0x0A

#define REG_NOMINAL_AVAILABLE_CAPACITY  0x0C//Units are mAh
#define REG_FULL_AVAILABLE_CHARGE	0x0E//Units are mAh
#define REG_REMAINING_CHARGE		0x10//Units are mAh
#define REG_FULL_CHARGE			0x12//Units are mAh

#define REG_AVERAGE_CURRENT             0x14//a signed integer value ,Units are mA.
#define REG_TIME_TO_EMPTY               0x16//a unsigned integer value ,Units are minutes.  A value of 65,535 indicates battery is not being discharged.
#define REG_TIME_TO_FULL                0x18//a unsigned integer value ,Units are minutes.    A value of 65,535 indicates battery is not being charged..

#define REG_STANDBY_CURRENT             0x1A//a unsigned integer value
#define REG_STANDBY_TIME_TO_EMPTY       0x1C//a unsigned integer value ,Units are minutes.  A value of 65,535 indicates battery is not being discharged
#define REG_MAX_LOAD_CURRENT            0x1E//a signed integer valuee ,Units are mA.
#define REG_MAX_LOAD_TIME_TO_EMPTY      0x20//an unsigned integer value ,Units are minutes. A value of 65,535 indicates battery is not being discharged

#define REG_AVAILABLE_ENERGY            0x22//an unsigned integer value. Units are mWh.
#define REG_AVERAGE_POWER		0x24//an unsigned integer value. Units are mW.A value of 0 indicates that the battery is not being discharged.
#define REG_TIME_TO_EMPTYT_AT_CONSTANT_POWER    0x26//an unsigned integer value. Units are minutes.A value of 0 indicates that the battery is not being discharged.

#define REG_CYCLE_COUNT                 0x2A //an unsigned integer value.a range of 0 to 65,535
#define REG_STATE_OF_CHARGE             0x2c //Relative State-of-Charge with a range of 0 to 100%.

//Control( ) Subcommands
#define CONTROL_STATUS                  0x00
#define DEVICE_TYPE                     0x01
#define FW_VERSION                      0x02
#define HW_VERSION                      0x03
#define SET_FULLSLEEP                   0x10
#define SET_HIBERNATE                   0x11
#define CLEAR_HIBERNATE                 0x12
#define SET_SHUTDOWN                    0x13
#define CLEAR_SHUTDOWN                  0x14
#define IT_ENABLE                       0x21
#define CAL_MODE                        0x40
#define RESET                           0x41

#define BQ27x00_REG_TEMP        0x06
#define BQ27x00_REG_VOLT        0x08
#define BQ27x00_REG_AI          0x14
#define BQ27x00_REG_FLAGS       0x0A
#define BQ27000_REG_RSOC        0x0B /* Relative State-of-Charge */
#define BQ27x00_REG_TTE         0x16
#define BQ27x00_REG_TTF         0x18
#define BQ27x00_REG_TTECP       0x26
#define BQ27500_REG_SOC         0x2c

#define BQ27500_FLAG_DSC        BIT(0)
#define BQ27000_FLAG_CHGS       BIT(7)
#define BQ27500_FLAG_FC         BIT(9)
#define BQ27500_FLAG_OTD        BIT(14)
#define BQ27500_FLAG_OTC        BIT(15)
/*Manufacturer id check function */
#define BQ27541_DATAFLASHBLOCK  0x3f
#define BQ27541_BLOCKDATA       0x40

#define BQ27541_LONG_COUNT	6

static int check_manufacturer(struct bq27541_info *di);


extern void twl6030_poweroff(void);

/*Hard low battery protection*/
#define HARD_LOW_VOLTAGE_THRESHOLD 3350000
#define RECHARGE_THRESHOLD 90
int bq275xx_register_notifier(struct notifier_block *nb)
{
    return blocking_notifier_chain_register(&notifier_list, nb);
}
EXPORT_SYMBOL_GPL(bq275xx_register_notifier);
int bq275xx_unregister_notifier(struct notifier_block *nb)
{
    return blocking_notifier_chain_unregister(&notifier_list, nb);
}
EXPORT_SYMBOL_GPL(bq275xx_unregister_notifier);
/*Thermal event function */
int get_battery_mAh(void)
{
    return g_full_available_capacity;
}
EXPORT_SYMBOL_GPL(get_battery_mAh);
/*Thermal event function */
int bq27x00_read(u8 reg, int *rt_value, int b_single,
            struct bq27541_info *di)
{
    struct i2c_client *client = di->bat_client;
    struct i2c_msg msg[1];
    unsigned char data[2];
    int err;

    if (!client->adapter)
        return -ENODEV;

    msg->addr = client->addr;
    msg->flags = 0;
    msg->len = 1;
    msg->buf = data;

    data[0] = reg;
    err = i2c_transfer(client->adapter, msg, 1);
    if (err >= 0) {
        if (!b_single)
            msg->len = 2;
        else
            msg->len = 1;

        msg->flags = I2C_M_RD;
        err = i2c_transfer(client->adapter, msg, 1);
        if (err >= 0) {
            if (!b_single)
                *rt_value = get_unaligned_le16(data);
            else
                *rt_value = data[0];
            return 0;
        }
    }
    return err;
}
int bq27x00_get_property(struct power_supply *psy,
    				enum power_supply_property psp,
    				union power_supply_propval *val)
{
	int ret = 0;
	struct bq27541_info *di = container_of(psy, struct bq27541_info, bat);

	switch (psp) {
	case POWER_SUPPLY_PROP_MANUFACTURER:
		switch (di->manufacturer) {
		case ATL:
			val->strval = "ATL";
			break;
		case THM:
			val->strval = "THM";
			break;
		case SWE:
			val->strval = "SWE";
			break;
		default:
			val->strval = "UNKNOWN";
			break;
		}
	break;
	case POWER_SUPPLY_PROP_STATUS:
		val->intval = di->status;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = di->voltage;
		if (psp == POWER_SUPPLY_PROP_PRESENT)
			val->intval = val->intval <= 0 ? 0 : 1;
		break;
	case POWER_SUPPLY_PROP_TECHNOLOGY:
		ret = val->intval = POWER_SUPPLY_TECHNOLOGY_LION;
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		val->intval = di->current_avg;
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		val->intval = di->capacity;
		break;
	case POWER_SUPPLY_PROP_TEMP:
		if (fake_temp == -990)
			val->intval = di->temp;
		else
			val->intval = fake_temp;
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		val->intval = di->health;
		break;
	case POWER_SUPPLY_PROP_TIME_TO_EMPTY_NOW:
		val->intval = di->time_to_empty;
		break;
	case POWER_SUPPLY_PROP_TIME_TO_FULL_NOW:
		val->intval = di->time_to_full;
		break;
	case POWER_SUPPLY_PROP_CYCLE_COUNT:
		val->intval = di->cycle_count;
		break;
	case POWER_SUPPLY_PROP_POWER_AVG:
		val->intval = di->average_power;
		break;
	case POWER_SUPPLY_PROP_ENERGY_AVG:
		val->intval = di->available_energy;
		break;
	case POWER_SUPPLY_PROP_CHARGE_NOW:
		val->intval = di->remaining_charge;
		break;
	case POWER_SUPPLY_PROP_CHARGE_NOW_DESIGN:
		val->intval = di->remaining_charge_uncompensated;
		break;
	case POWER_SUPPLY_PROP_CHARGE_FULL:
		if (fake_full_available_capacity == 0)
			val->intval = di->full_charge;
		else
			val->intval = fake_full_available_capacity;
		break;
	case POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN:
		val->intval = di->full_charge_uncompensated;
		break;
	default:
		return -EINVAL;
	}
	return ret;
}

/*
 * Return the battery temperature in tenths of degree Celsius
 * Or < 0 if something fails.
 */
int bq27x00_battery_temperature(struct bq27541_info *di,int* value)
{
    int ret=0;
    int temp = 0;

    ret = bq27x00_read(BQ27x00_REG_TEMP, &temp, 0, di);//0.1K
    if (ret) {
        dev_err(di->dev, "error reading temperature\n");
        return ret;
    }    
    //Kelvin to Celsius °C = K − 273.15
    *value=temp - 2731;
    return ret;
}

/*
 * Return the battery Voltage in milivolts
 * Or < 0 if something fails.
 */
int bq27x00_battery_voltage(struct bq27541_info *di,int* value)
{
    int ret=0;
    int volt = 0;
    ret = bq27x00_read(BQ27x00_REG_VOLT, &volt, 0, di);
    if (ret) {
        dev_err(di->dev, "error reading voltage\n");
        return ret;
    }
    *value =volt * 1000;
    return ret;
}

/*
 * Return the battery average current(milliamp)
 * Note that current can be negative signed as well
 * Or 0 if something fails.
 */
int bq27x00_battery_current(struct bq27541_info *di,int* value)
{
    int ret=0;
    int curr = 0;

    ret = bq27x00_read(BQ27x00_REG_AI, &curr, 0, di);
    if (ret) {
        dev_err(di->dev, "error reading current\n");
        return ret;
    }
    /* bq27500 returns signed value */
    curr = (int)(s16)curr;
    *value=curr * 1000;

    return ret;
}

/*
 * Return the battery Relative State-of-Charge
 * Or < 0 if something fails.
 */
int bq27x00_battery_rsoc(struct bq27541_info *di,int* value)
{
    int ret=0;
    int rsoc=0;
    ret = bq27x00_read(BQ27500_REG_SOC, &rsoc, 0, di);
    if (ret < 0) {
        dev_err(di->dev, "error reading rsoc\n");
        return ret;
    }
    *value=rsoc;
   
    return ret;
}

int bq27x00_battery_status(struct bq27541_info *di,int* value)
{
    int flags = 0;
    int curr = 0;
    int ret;

    ret = bq27x00_read(BQ27x00_REG_FLAGS, &flags, 0, di);
    if (ret < 0) {
        dev_err(di->dev, "error reading flags\n");
        return ret;
    }
    if (flags & BQ27500_FLAG_FC)
        *value = POWER_SUPPLY_STATUS_FULL;
    else if (flags & BQ27500_FLAG_DSC)
        *value = POWER_SUPPLY_STATUS_DISCHARGING;
    else{
        
        ret = bq27x00_read(BQ27x00_REG_AI, &curr, 0, di);
        if (ret < 0) {
            dev_err(di->dev, "error reading flags\n");
            return ret;
        }
        if(curr==0){
            *value = POWER_SUPPLY_STATUS_NOT_CHARGING;
        }else if(curr>0){
            *value = POWER_SUPPLY_STATUS_CHARGING;
        }
    }

    return ret;
}

int bq27x00_battery_health(struct bq27541_info *di, int *value)
{
	int flags;
	int ret;

	*value = POWER_SUPPLY_HEALTH_UNKNOWN;

	ret = bq27x00_read(BQ27x00_REG_FLAGS, &flags, 0, di);
	if (ret < 0) {
		dev_err(di->dev, "error reading flags\n");
		return ret;
	}

	if (flags & (BQ27500_FLAG_OTC | BQ27500_FLAG_OTD))
		*value = POWER_SUPPLY_HEALTH_OVERHEAT;
	else
		*value = POWER_SUPPLY_HEALTH_GOOD;

	return ret;
}

static int bq27x00_battery_time_to_full(struct bq27541_info *di, unsigned int *value)
{
	int ret = -1, time = 0;

	*value = 0;

	ret = bq27x00_read(REG_TIME_TO_FULL, &time, 0, di);

	if (ret) {
		dev_err(di->dev, "error reading time to full\n");
		return ret;
	}

	/* Convert minutes to seconds */
	*value = (unsigned int)(time * 60);

	return ret;
}

static int bq27x00_battery_time_to_empty(struct bq27541_info *di, unsigned int *value)
{
	int ret = -1, time = 0;

	*value = 0;

	ret = bq27x00_read(REG_TIME_TO_EMPTY, &time, 0, di);

	if (ret) {
		dev_err(di->dev, "error reading time to empty\n");
		return ret;
	}

	/* Convert minutes to seconds */
	*value = (unsigned int)(time * 60);

	return ret;
}

static int bq27x00_battery_cycle_count(struct bq27541_info *di, unsigned int *value)
{
	int ret = -1, count = 0;

	*value = 0;

	ret = bq27x00_read(REG_CYCLE_COUNT, &count, 0, di);

	if (ret) {
		dev_err(di->dev, "error reading cycle count\n");
		return ret;
	}

	*value = (unsigned int)count;

	return 0;
}

static int bq27x00_battery_average_power(struct bq27541_info *di, unsigned int *value)
{
	int ret = -1, power = 0;

	*value = 0;

	ret = bq27x00_read(REG_AVERAGE_POWER, &power, 0, di);

	if (ret) {
		dev_err(di->dev, "error reading average power\n");
		return ret;
	}

	/* Convert mW to uW */
	*value = (unsigned int)(power * 1000);

	return 0;
}

static int bq27x00_battery_available_energy(struct bq27541_info *di, unsigned int *value)
{
	int ret = -1, energy = 0;

	*value = 0;

	ret = bq27x00_read(REG_AVAILABLE_ENERGY, &energy, 0, di);

	if (ret) {
		dev_err(di->dev, "error reading available energy\n");
		return ret;
	}

	/* Convert mWh to uWh */
	*value = (unsigned int)(energy * 1000);

	return 0;
}

static int bq27x00_battery_remaining_charge(struct bq27541_info *di, unsigned int *value)
{
	int ret = -1, charge = 0;

	*value = 0;

	ret = bq27x00_read(REG_REMAINING_CHARGE, &charge, 0, di);

	if (ret) {
		dev_err(di->dev, "error reading remaining capacity\n");
		return ret;
	}

	/* Convert mAh to uAh */
	*value = (unsigned int)(charge * 1000);

	return 0;
}

static int bq27x00_battery_nominal_available_charge(struct bq27541_info *di, unsigned int *value)
{
	int ret = -1, charge = 0;

	*value = 0;

	ret = bq27x00_read(REG_NOMINAL_AVAILABLE_CAPACITY, &charge, 0, di);

	if (ret) {
		dev_err(di->dev, "error reading remaining capacity\n");
		return ret;
	}

	/* Convert mAh to uAh */
	*value = (unsigned int)(charge * 1000);

	return 0;
}



static int bq27x00_battery_full_charge(struct bq27541_info *di, unsigned int *value)
{
	int ret = -1, charge = 0;

	*value = 0;

	ret = bq27x00_read(REG_FULL_CHARGE, &charge, 0, di);

	if (ret) {
		dev_err(di->dev, "error reading full charge capacity\n");
		return ret;
	}

	/* Convert mAh to uAh */
	*value = (unsigned int)(charge * 1000);

	return 0;
}

static int bq27x00_battery_full_charge_uncompensated(struct bq27541_info *di, unsigned int *value)
{
	int ret = -1, charge = 0;

	*value = 0;

	ret = bq27x00_read(REG_FULL_AVAILABLE_CHARGE, &charge, 0, di);

	if (ret) {
		dev_err(di->dev, "error reading full charge (uncompensated)\n");
		return ret;
	}

	/* Convert mAh to uAh */
	*value = (unsigned int)(charge * 1000);

	return 0;
}

#ifdef BAT_LOG
void bat_log_work_func(struct work_struct *work)
{
    struct bq27541_info *di = container_of(work,
                struct bq27541_info, bat_log_work.work);
    if(di->state==BAT_CRITICAL_STATE)
        queue_delayed_work(bat_work_queue,&di->bat_log_work,
                            msecs_to_jiffies(60000 * 1));
}
#endif
/*Thermal event function */
/*
    EVENT_TEMP_PROTECT_STEP_1,//       temp < -20
    EVENT_TEMP_PROTECT_STEP_2,// -20 < temp < 0
    EVENT_TEMP_PROTECT_STEP_3,//   0 < temp < 8
    EVENT_TEMP_PROTECT_STEP_4,//   8 < temp < 14
    EVENT_TEMP_PROTECT_STEP_5,//  14 < temp < 23
    EVENT_TEMP_PROTECT_STEP_6,//  23 < temp < 45
    EVENT_TEMP_PROTECT_STEP_7,//  45 < temp < 60
    EVENT_TEMP_PROTECT_STEP_8,//  60 < temp 
*/
void bat_temp_charge_protect(struct bq27541_info *di){
    int event=0;
    if(fake_temp!=-990)
        di->temp=fake_temp;
    if(di->temp > -200){
        if(di->temp > 0){
            if(di->temp > 8){
                if(di->temp > 140){
                    if(di->temp > 230){
                        if(di->temp > 450){
                            if(di->temp > 600){
                                event=EVENT_TEMP_PROTECT_STEP_8;
                            }else{
                                event=EVENT_TEMP_PROTECT_STEP_7;
                            }
                        }else{
                            event=EVENT_TEMP_PROTECT_STEP_6;
                    }
                    }else{
                        event=EVENT_TEMP_PROTECT_STEP_5;
                    }
                }else{
                    event=EVENT_TEMP_PROTECT_STEP_4;
                }
            }else{
                event=EVENT_TEMP_PROTECT_STEP_3;
            }
        }else{
            event=EVENT_TEMP_PROTECT_STEP_2;
        }
    }else{
        event=EVENT_TEMP_PROTECT_STEP_1;
    }
    if(di->temperature_step!=event){
        di->temperature_step=event;
        dev_err(di->dev, "termperature=%d \n",di->temp);
        blocking_notifier_call_chain(&notifier_list, event, NULL);
    }
}

void bat_monitor_work_func(struct work_struct *work)
{
    struct bq27541_info *di = container_of(work,
                struct bq27541_info, bat_monitor_work.work);
    int err = 0;
    int new_value=0;
    int change=0;
	u8 value = 0;
    //For dead battery
    if(di->err_count>=15){
        printk("bat:%s get i2c error\n",__func__);
        err=bq27x00_battery_voltage(di,&new_value);
        if (err){
            queue_delayed_work(bat_work_queue,&di->bat_monitor_work,
                    msecs_to_jiffies(10000 * 1));
            blocking_notifier_call_chain(&notifier_list, EVENT_BATTERY_I2C_ERROR, NULL);
		di->health = POWER_SUPPLY_HEALTH_UNKNOWN;
		power_supply_changed(&di->bat);
        }else{//dead battery is wake up
            printk("bat:%s dead battery is wake up\n",__func__);
            di->err_count=0;
            queue_delayed_work(bat_work_queue,&di->bat_monitor_work,0);
            blocking_notifier_call_chain(&notifier_list, EVENT_BATTERY_I2C_NORMAL, NULL);
		di->health = POWER_SUPPLY_HEALTH_GOOD;
		power_supply_changed(&di->bat);
        }
    }else{
		/*
		 * We query the four most important variables frequently:
		 *
		 * temperature, voltage, capacity, status
		 *
		 * These four are used by various code in upper layers for
		 * power management. The rest of the variables are not that
		 * important or do not change much over time so we do not
		 * need to query them too often
		 */
		if (bq27x00_battery_temperature(di, &new_value)) {
			di->err_count++;
		} else {
			if (new_value != di->temp) {
				di->temp = new_value;
				change = 1;
				bat_temp_charge_protect(di);
			}
		}

		if (bq27x00_battery_voltage(di, &new_value)) {
			di->err_count++;
		} else {
			if (new_value != di->voltage) {
				di->voltage = new_value;
				change = 1;

				//Low battery protection
		                //If android with weak battery has some problem and not be shutdown then driver need to turn off the system.
				if((di->voltage <= HARD_LOW_VOLTAGE_THRESHOLD)
						&& (di->status==POWER_SUPPLY_STATUS_DISCHARGING))
					blocking_notifier_call_chain(&notifier_list, EVENT_WEAK_BATTERY, NULL);
			}
		}

		if (bq27x00_battery_rsoc(di, &new_value)) {
			di->err_count++;
		} else {
			if (new_value != di->capacity) {
				di->capacity = new_value;
				change = 1;

				//Recharge event
				if((di->status==POWER_SUPPLY_STATUS_NOT_CHARGING)
						&& (di->capacity<=RECHARGE_THRESHOLD))
				blocking_notifier_call_chain(&notifier_list, EVENT_RECHARGE_BATTERY, NULL);
			}
		}

		if (bq27x00_battery_status(di, &new_value)) {
			di->err_count++;
		} else {
			if (new_value!=di->status) {
				di->status = new_value;
				change = 1;

				//Full charge protectition event
				if (di->status == POWER_SUPPLY_STATUS_FULL)
					blocking_notifier_call_chain(&notifier_list, EVENT_FULL_BATTERY, NULL);
			}
		}

		/*
		 * Average current is also required to be
		 * queried frequently for factory testing
		 */
		if (bq27x00_battery_current(di, &new_value)){
			di->err_count++;
		} else {
			if (new_value != di->current_avg) {
				di->current_avg=new_value;
				change = 1;
			}
		}

		if (++di->long_count != BQ27541_LONG_COUNT) {
			/* Done with the important ones, skip the rest for now */
			goto update_status;
		} else {
			di->long_count = 0;
		}

	    bq27x00_read(REG_FULL_AVAILABLE_CHARGE, &g_full_available_capacity, 0, di);

		if (bq27x00_battery_health(di, &new_value)) {
			di->err_count++;
		} else {
			if (new_value != di->health) {
				di->health = new_value;
				change = 1;
			}
		}

		/* Query TimeToEmpty() register */
		if (bq27x00_battery_time_to_empty(di, &new_value)) {
			di->err_count++;
		} else if (new_value != di->time_to_empty) {
			di->time_to_empty = new_value;
			change = 1;
		}

		/* Query TimeToFull() register */
		if (bq27x00_battery_time_to_full(di, &new_value)) {
			di->err_count++;
		} else if (new_value != di->time_to_full) {
			di->time_to_full = new_value;
			change = 1;
		}

		/* Query CycleCount() register */
		if (bq27x00_battery_cycle_count(di, &new_value)) {
			di->err_count++;
		} else if (new_value != di->cycle_count) {
			di->cycle_count = new_value;
			change = 1;
		}

		/* Query AveragePower() register */
		if (bq27x00_battery_average_power(di, &new_value)) {
			di->err_count++;
		} else if (new_value != di->average_power) {
			di->average_power = new_value;
			change = 1;
		}

		/* Query AvailableEnergy() register */
		if (bq27x00_battery_available_energy(di, &new_value)) {
			di->err_count++;
		} else if (new_value != di->available_energy) {
			di->available_energy = new_value;
			change = 1;
		}

		/* Query RemainingCapacity() register */
		if (bq27x00_battery_remaining_charge(di, &new_value)) {
			di->err_count++;
		} else if (new_value != di->remaining_charge) {
			di->remaining_charge = new_value;
			change = 1;
		}

		/* Query NominalAvailableCapacity() register */
		if (bq27x00_battery_nominal_available_charge(di, &new_value)) {
			di->err_count++;
		} else if (new_value != di->remaining_charge_uncompensated) {
			di->remaining_charge_uncompensated = new_value;
			change = 1;
		}

		/* Query FullChargeCapacity() register */
		if (bq27x00_battery_full_charge(di, &new_value)) {
			di->err_count++;
		} else if (new_value != di->full_charge) {
			di->full_charge = new_value;
			change = 1;
		}

		/* Query FullAvailableCapacity() register */
		if (bq27x00_battery_full_charge_uncompensated(di, &new_value)) {
			di->err_count++;
		} else if (new_value != di->full_charge_uncompensated) {
			di->full_charge_uncompensated = new_value;
			change = 1;
		}



		if (++di->manufacturer_id_read_count != 2) {
			goto update_status;
		} else {
			di->manufacturer_id_read_count = 0;
		}

		/* Check for manufacturer ID every minute */
		new_value = check_manufacturer(di);

		if (new_value != di->manufacturer){
			di->manufacturer = new_value;
			change = 1;
            
			if (di->manufacturer >= 0)
				blocking_notifier_call_chain(&notifier_list,
					EVENT_RECOGNIZE_BATTERY, NULL);

			if (di->manufacturer == NOT_RECOGNIZE)
				blocking_notifier_call_chain(&notifier_list,
					EVENT_NOT_RECOGNIZE_BATTERY, NULL);
            
			if (di->manufacturer == UNKNOW)
				blocking_notifier_call_chain(&notifier_list,
					EVENT_UNKNOW_BATTERY, NULL);
        	}

update_status:

	if (change) {
		power_supply_changed(&di->bat);

		// LED function


		if ((di->disable_led == 0) &&
			!twl_i2c_read_u8(TWL6030_MODULE_CHARGER, &value, 0x03)) {

		        if ((di->status == POWER_SUPPLY_STATUS_CHARGING)
					&& (value & (1 << 2))) {
				if(di->capacity < 90) {
					/*
					 * Battery being charged, capacity < 90%: Amber LED
					 */
					omap4430_green_led_set(NULL, 0);
					omap4430_orange_led_set(NULL, 255);
				} else {
					/*
					 * Battery being charged, capacity >= 90%: Green LED
					 */
					omap4430_orange_led_set(NULL, 0);
					omap4430_green_led_set(NULL, 255);
				}
		        } else if (di->status == POWER_SUPPLY_STATUS_FULL) {
				if (value & (1 << 2)) {
					/* Set to green if connected to USB */
					omap4430_orange_led_set(NULL, 0);
					omap4430_green_led_set(NULL, 255);
				} else {
					omap4430_green_led_set(NULL, 0);
					omap4430_orange_led_set(NULL, 0);
				}
		        }else if(value & (1 << 2)){
				omap4430_green_led_set(NULL, 0);
				omap4430_orange_led_set(NULL, 0);
			}
		}
	}

        if(di->err_count>=15){
            printk("bat:%s get i2c error\n",__func__);
            queue_delayed_work(bat_work_queue,&di->bat_monitor_work,
                            msecs_to_jiffies(10000 * 1));
        }else
            queue_delayed_work(bat_work_queue,&di->bat_monitor_work,
            msecs_to_jiffies(5000 * 1));
    }
}

enum power_supply_property bq27x00_battery_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_TECHNOLOGY,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_TIME_TO_EMPTY_NOW,
	POWER_SUPPLY_PROP_TIME_TO_FULL_NOW,
	POWER_SUPPLY_PROP_CYCLE_COUNT,
	POWER_SUPPLY_PROP_POWER_AVG,
	POWER_SUPPLY_PROP_ENERGY_AVG,
	POWER_SUPPLY_PROP_CHARGE_NOW,
	POWER_SUPPLY_PROP_CHARGE_NOW_DESIGN,
	POWER_SUPPLY_PROP_CHARGE_FULL,
/*Manufacturer id check function */
   	POWER_SUPPLY_PROP_MANUFACTURER,
	POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN,
};

#if defined(CONFIG_LAB126)
static struct timespec bq27541_suspend_time;
static int bq27541_suspend_capacity = -1;
#endif

static int bq27541_suspend(struct i2c_client *client, pm_message_t mesg)
{
	struct bq27541_info *di = i2c_get_clientdata(client);

	cancel_delayed_work_sync(&di->bat_monitor_work);

#if defined(CONFIG_LAB126)


	/* Use the cached value */
	bq27541_suspend_capacity = di->capacity;

	bq27541_suspend_time = current_kernel_time();
#endif

	return 0;
}

static int bq27541_resume(struct i2c_client *client)
{
    struct bq27541_info *di = i2c_get_clientdata(client);
    int err = 0;
    int new_value=0;
#if defined(CONFIG_LAB126)
	struct timespec diff;
#endif

    //Low battery protection
//	printk(KERN_INFO "at %s\n", __FUNCTION__);
    err=bq27x00_battery_voltage(di,&new_value);
    if (err){
        di->err_count++;
    }else{
        di->voltage=new_value;
    }

    err = bq27x00_battery_remaining_charge(di, &new_value);

    if (err) {
        di->err_count++;
    } else {
        di->remaining_charge=new_value;
    }

    err=bq27x00_battery_rsoc(di,&new_value);
    if (err){
        di->err_count++;
    }else{
            di->capacity=new_value;
    }

    power_supply_changed(&di->bat);  
    queue_delayed_work(bat_work_queue,&di->bat_monitor_work,
            msecs_to_jiffies(2500 * 1));

#if defined(CONFIG_LAB126)
	/* Compute elapsed time and determine battery drainage */
	diff = timespec_sub(current_kernel_time(),
				bq27541_suspend_time);

	if (!err && bq27541_suspend_capacity != -1) {
		dev_info(di->dev, "Suspend drainage: %d %% over %ld msecs\n",
			bq27541_suspend_capacity - new_value,
			diff.tv_sec * 1000 + diff.tv_nsec / NSEC_PER_MSEC);
	} else {
		dev_err(di->dev, "Unable to obtain suspend drainage\n");
	}
#endif

    return 0;
}

static ssize_t at_rate_time_to_empty_show(struct device *dev, struct device_attribute *attr,char *buf)
{
    struct bq27541_info *di = i2c_get_clientdata(to_i2c_client(dev));
    int ret=0;
    int time=0;
    ret = bq27x00_read(REG_AT_RATE_TIME_TO_EMPTY, &time, 0, di);
    if (ret < 0) {
        return 0;
    }
    return sprintf(buf, "%d minute\n", time);
}

static DEVICE_ATTR(at_rate_time_to_empty, S_IRUGO, at_rate_time_to_empty_show, NULL);

static ssize_t at_rate_show(struct device *dev, struct device_attribute *attr,char *buf)
{
    struct bq27541_info *di = i2c_get_clientdata(to_i2c_client(dev));
    int ret=0;
    int at_rate=0;
    ret = bq27x00_read(REG_AT_RATE, &at_rate, 0, di);
    if (ret) {
        return 0;
    }
    /* bq27500 returns signed value */
    at_rate = (int)(s16)at_rate;
    return sprintf(buf, "%d mA\n", at_rate);
}

static DEVICE_ATTR(at_rate, S_IRUGO, at_rate_show, NULL);

static ssize_t nominal_available_capacity_show(struct device *dev, struct device_attribute *attr,char *buf)
{
    struct bq27541_info *di = i2c_get_clientdata(to_i2c_client(dev));
    int ret=0;
    int capacity=0;
    ret = bq27x00_read(REG_NOMINAL_AVAILABLE_CAPACITY, &capacity, 0, di);
    if (ret) {
        return 0;
    }
    return sprintf(buf, "%d mAh\n", capacity);
}

static DEVICE_ATTR(nominal_available_capacity, S_IRUGO, nominal_available_capacity_show, NULL);

static ssize_t standby_current_show(struct device *dev, struct device_attribute *attr,char *buf)
{
    struct bq27541_info *di = i2c_get_clientdata(to_i2c_client(dev));
    int ret=0;
    int curr=0;
    ret = bq27x00_read(REG_STANDBY_CURRENT, &curr, 0, di);
    if (ret) {
        return 0;
    }
    /* bq27500 returns signed value */
    curr = (int)(s16)curr;
    return sprintf(buf, "%d mA\n", curr);
}

static DEVICE_ATTR(standby_current, S_IRUGO, standby_current_show, NULL);

static ssize_t standby_time_to_empty_show(struct device *dev, struct device_attribute *attr,char *buf)
{
    struct bq27541_info *di = i2c_get_clientdata(to_i2c_client(dev));
    int ret=0;
    int minute=0;
    ret = bq27x00_read(REG_STANDBY_TIME_TO_EMPTY, &minute, 0, di);
    if (ret) {
        return 0;
    }
    return sprintf(buf, "%d minute\n", minute);
}

static DEVICE_ATTR(standby_time_to_empty, S_IRUGO, standby_time_to_empty_show, NULL);

static ssize_t max_load_current_show(struct device *dev, struct device_attribute *attr,char *buf)
{
    struct bq27541_info *di = i2c_get_clientdata(to_i2c_client(dev));
    int ret=0;
    int curr=0;
    ret = bq27x00_read(REG_MAX_LOAD_CURRENT, &curr, 0, di);
    if (ret) {
        return 0;
    }
    /* bq27500 returns signed value */
    curr = (int)(s16)curr;
    return sprintf(buf, "%d mA\n", curr);
}

static DEVICE_ATTR(max_load_current, S_IRUGO, max_load_current_show, NULL);

static ssize_t max_load_time_to_empty_show(struct device *dev, struct device_attribute *attr,char *buf)
{
    struct bq27541_info *di = i2c_get_clientdata(to_i2c_client(dev));
    int ret=0;
    int minute=0;
    ret = bq27x00_read(REG_MAX_LOAD_TIME_TO_EMPTY, &minute, 0, di);
    if (ret) {
        return 0;
    }
    return sprintf(buf, "%d minute\n", minute);
}

static DEVICE_ATTR(max_load_time_to_empty, S_IRUGO, max_load_time_to_empty_show, NULL);

static ssize_t time_to_empty_at_constant_power_show(struct device *dev, struct device_attribute *attr,char *buf)
{
    struct bq27541_info *di = i2c_get_clientdata(to_i2c_client(dev));
    int ret=0;
    int minute=0;
    ret = bq27x00_read(REG_TIME_TO_EMPTYT_AT_CONSTANT_POWER, &minute, 0, di);
    if (ret) {
        return 0;
    }
    return sprintf(buf, "%d minute\n", minute);
}

static DEVICE_ATTR(time_to_empty_at_constant_power, S_IRUGO, time_to_empty_at_constant_power_show, NULL);

static ssize_t battery_polling_show(struct device *dev,
			struct device_attribute *attr, char *buffer)
{
	struct bq27541_info *di = i2c_get_clientdata(to_i2c_client(dev));

	return sprintf(buffer, "%d\n", di->battery_polling);
}

static ssize_t battery_polling_store(struct device *dev,
			struct device_attribute *attr, const char *buffer,
			size_t size)
{
	struct bq27541_info *di = i2c_get_clientdata(to_i2c_client(dev));
	unsigned long value = simple_strtoul(buffer, NULL, 0);

	if (value == 0) {
		di->battery_polling = 0;

		/* Cancel any pending works */
		cancel_delayed_work_sync(&di->bat_monitor_work);
	} else if (value == 1) {
		/* Start polling the batery */
		if (di->battery_polling == 0)
			queue_delayed_work(bat_work_queue,
				&di->bat_monitor_work,
				msecs_to_jiffies(5000 * 1));

		di->battery_polling = 1;
	}

	return size;
}
static ssize_t tempstep_show(struct device *dev, struct device_attribute *attr,char *buf)
{
    struct bq27541_info *di = i2c_get_clientdata(to_i2c_client(dev));
    return sprintf(buf, "%d\n", di->temperature_step);
}
static DEVICE_ATTR(tempstep, S_IRUGO, tempstep_show, NULL);
static DEVICE_ATTR(polling, S_IRUGO | S_IWUSR,
		battery_polling_show, battery_polling_store);
static ssize_t disable_led_show(struct device *dev,
			struct device_attribute *attr, char *buffer)
{
	struct bq27541_info *di = i2c_get_clientdata(to_i2c_client(dev));

	return sprintf(buffer, "%d\n", di->disable_led);
}
static ssize_t disable_led_store(struct device *dev,
			struct device_attribute *attr, const char *buffer,
			size_t size)
{
	struct bq27541_info *di = i2c_get_clientdata(to_i2c_client(dev));
	unsigned long value = simple_strtoul(buffer, NULL, 0);

	if (value == 0) {
		di->disable_led = 0;
	} else if (value == 1) {
		di->disable_led = 1;
                omap4430_green_led_set(NULL, 0);
                omap4430_orange_led_set(NULL, 0);
	}

	return size;
}
static DEVICE_ATTR(disable_led, S_IRUGO | S_IWUSR,
		disable_led_show, disable_led_store);

static struct attribute *bq_attrs[] = {
	&dev_attr_at_rate_time_to_empty.attr,
	&dev_attr_at_rate.attr,
	&dev_attr_nominal_available_capacity.attr,
	&dev_attr_standby_current.attr,
	&dev_attr_standby_time_to_empty.attr,
	&dev_attr_max_load_current.attr,
	&dev_attr_max_load_time_to_empty.attr,
	&dev_attr_time_to_empty_at_constant_power.attr,
	&dev_attr_polling.attr,
        &dev_attr_disable_led.attr,
    &dev_attr_tempstep.attr,
	NULL
};

static struct attribute_group bq_attr_grp = {
    .attrs = bq_attrs,
};
/*Manufacturer id check function */
static int bat_name[3][8] = {
{0x41,0x54,0x4C,0x20,0x4B,0x43,0x31,0x20}, //ATL KC1
{0x54,0x48,0x4D,0x20,0x4B,0x43,0x31,0x20},   //THM KC1
{0x53,0x57,0x45,0x20,0x4B,0x43,0x31,0x20}    //SWE KC1
};

static int check_manufacturer(struct bq27541_info *di)
{
	u8 m_name[8];
	int i = 0, offset = 0;

	i2c_smbus_write_byte_data(di->bat_client, BQ27541_DATAFLASHBLOCK, 1);
	mdelay(10);

	/* Read the manufacturer block */
	memset(m_name, 0, sizeof(m_name));

	if (i2c_smbus_read_i2c_block_data(di->bat_client,
			BQ27541_BLOCKDATA, 8, m_name) < 0) {
		dev_err(&di->bat_client->dev, "Failed to read manufacturer ID\n");
		return UNKNOW;
	}

    for(i=0;i<3;i++){
        for(offset=0;offset<8;offset++){
            if(m_name[offset]!=bat_name[i][offset])
                break;
            if(offset==7)
                return i;
        }
    }
    return NOT_RECOGNIZE;
}
/*Manufacturer id check function */

#ifdef    CONFIG_PROC_FS
#define   BQ_PROC_FILE	"driver/bq"
#define   BQ_MAJOR          620

static u8 index=0xe;

static ssize_t bq_proc_read(char *page, char **start, off_t off, int count,
    		 int *eof, void *data)
{
    struct bq27541_info *di=data;
    //printk(KERN_INFO "%s\n",__func__);

    u32 value=0;
    char	*next = page;
    unsigned size = count;
    int t;

    if (off != 0)
    	return 0;
    //you can read value from the register of hardwae in here
    value =i2c_smbus_read_byte_data(di->bat_client,index);
    if (value < 0) {
        printk(KERN_INFO"error reading index=0x%x value=0x%x\n",index,value);
        return value;
    }
    t = scnprintf(next, size, "%d",value);
    size -= t;
    next += t;
    printk(KERN_INFO"%s:register 0x%x: return value 0x%x\n",__func__, index, value);
    *eof = 1;
    return count - size;
}

static ssize_t bq_proc_write(struct file *filp, 
    	const char *buff,unsigned long len, void *data)
{
    struct bq27541_info *di=data;
    u32 reg_val;

    char messages[256];

    if (len > 256)
    	len = 256;

    if (copy_from_user(messages, buff, len))
    	return -EFAULT;
    
    if ('-' == messages[0]) {
    	/* set the register index */
    	//memcpy(vol, messages+1, len-1);
    	index = (int) simple_strtoul(messages+1, NULL, 10);
        fake_temp=index*10;
    	printk(KERN_INFO"%s:fake_temp %d\n",__func__, fake_temp);
    }else if('+'==messages[0]) {
    	// set the register value 
    	//memcpy(vol, messages+1, len-1);
    	reg_val = (int)simple_strtoul(messages+1, NULL, 10);
        fake_full_available_capacity=reg_val*100;
        printk(KERN_INFO"%s:fake_full_available_capacity %d\n",__func__, fake_full_available_capacity);
    	//you can read value from the register of hardwae in here
    	//i2c_smbus_write_byte_data(di->bat_client, index, reg_val);
    	//printk(KERN_INFO"%s:register 0x%x: set value 0x%x\n",__func__, index, reg_val);
    }else if ('!' == messages[0]) {
        switch(messages[1]){
            case 'a':
                cancel_delayed_work(&di->bat_monitor_work);
                flush_scheduled_work();
            break;
            case 'b':
                queue_delayed_work(bat_work_queue,&di->bat_monitor_work,msecs_to_jiffies(1000 * 1));
            break;
            case 'c':   
                di->capacity=0;
                power_supply_changed(&di->bat);
            break;
            case 'd':
                blocking_notifier_call_chain(&notifier_list, EVENT_UNKNOW_BATTERY, NULL);
            break;
            case 'e':
                blocking_notifier_call_chain(&notifier_list, EVENT_RECOGNIZE_BATTERY, NULL);
            break;
            case 'f':
                blocking_notifier_call_chain(&notifier_list, EVENT_NOT_RECOGNIZE_BATTERY, NULL);
            break;
            case 'g':
                blocking_notifier_call_chain(&notifier_list, EVENT_WEAK_BATTERY, NULL);
            break;
            case 'h':
                blocking_notifier_call_chain(&notifier_list, EVENT_BATTERY_I2C_ERROR, NULL);
            break;
            case 'i':
                blocking_notifier_call_chain(&notifier_list, EVENT_FULL_BATTERY, NULL);
            break;
            case 'j':
                blocking_notifier_call_chain(&notifier_list, EVENT_RECHARGE_BATTERY, NULL);
            break;
            case 'k':
                blocking_notifier_call_chain(&notifier_list, EVENT_BATTERY_NTC_ZERO, NULL);
            break;
            case 'l':
                blocking_notifier_call_chain(&notifier_list, EVENT_BATTERY_NTC_NORMAL, NULL);
            break;
            case '1'://temp < -20
                fake_temp=-200;
                //blocking_notifier_call_chain(&notifier_list, EVENT_TEMP_PROTECT_STEP_1, NULL);                
            break;
            case '2'://-20 < temp < 0 , Stop Charge
                fake_temp=-250;
                //blocking_notifier_call_chain(&notifier_list, EVENT_TEMP_PROTECT_STEP_2, NULL);                  
            break;
            case '3'://0 < temp < 14 , 0.18C (max) to 4.0V
                fake_temp=-100;
                //blocking_notifier_call_chain(&notifier_list, EVENT_TEMP_PROTECT_STEP_3, NULL);  
            break;
            case '4'://8 < temp < 14 , 0.18C (max) to 4.2V
                blocking_notifier_call_chain(&notifier_list, EVENT_TEMP_PROTECT_STEP_4, NULL);                
            break;
            case '5'://14 < temp < 23 , 0.5C (max) to 4.2V
                blocking_notifier_call_chain(&notifier_list, EVENT_TEMP_PROTECT_STEP_5, NULL);                  
            break;
            case '6'://23 < temp < 45 , 0.7C (max) to 4.2V
                blocking_notifier_call_chain(&notifier_list, EVENT_TEMP_PROTECT_STEP_6, NULL);  
            break;
            case '7'://45 < temp < 60 , 0.5C (max) to 4V
                blocking_notifier_call_chain(&notifier_list, EVENT_TEMP_PROTECT_STEP_7, NULL);  
            break;
            case '8'://60 > temp , Stop Charge and Shutdown
                blocking_notifier_call_chain(&notifier_list, EVENT_TEMP_PROTECT_STEP_8, NULL);  
            break;
    	}
    }
    return len;
}

void create_bq_procfs(struct bq27541_info *di)
{
    printk(KERN_INFO "%s\n",__func__);
    di->bq_proc_fs = create_proc_entry(BQ_PROC_FILE, BQ_MAJOR, NULL);
    if (di->bq_proc_fs) {
    	di->bq_proc_fs->data = di; 
    	di->bq_proc_fs->read_proc = bq_proc_read; 
    	di->bq_proc_fs->write_proc = bq_proc_write; 
    } else
    	printk(KERN_ERR "%sproc file create failed!\n",__func__);
}

void remove_bq_procfs(void)
{
    printk(KERN_INFO "%s\n",__func__);
    remove_proc_entry(BQ_PROC_FILE, NULL);
}
#endif

#if defined(CONFIG_LAB126) && defined(CONFIG_EARLYSUSPEND)

static struct timespec bq27541_early_suspend_time;
static int bq27541_early_suspend_capacity = -1;

static void bq27541_early_suspend(struct early_suspend *handler)
{

	struct bq27541_info *di = container_of(handler, struct bq27541_info,
			early_suspend);

	/* Use the cached value */
	bq27541_early_suspend_capacity = di->capacity;

	bq27541_early_suspend_time = current_kernel_time();

	return;
};

static void bq27541_late_resume(struct early_suspend *handler)
{
	int value = -1;
	struct bq27541_info *di = container_of(handler, struct bq27541_info,
			early_suspend);

	/* Compute elapsed time and determine battery drainage */
	struct timespec diff = timespec_sub(current_kernel_time(),
				bq27541_early_suspend_time);

	value = di->capacity;

	if (bq27541_early_suspend_capacity != -1) {
		dev_info(di->dev, "Screen off drainage: %d %% over %ld msecs\n",
			bq27541_early_suspend_capacity - value,
			diff.tv_sec * 1000 + diff.tv_nsec / NSEC_PER_MSEC);
	} else {
		dev_err(di->dev, "Unable to obtain screen off drainage\n");
	}

	return;
}

#endif

static int __devinit bq27541_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
    struct bq27541_info *di;
    int ret;
    int error;

    printk(KERN_INFO "%s\n",__func__);
    di = kzalloc(sizeof(*di), GFP_KERNEL);
    if (!di) {
        return -ENOMEM;
    }
    di->bat_client                     = client;
    di->dev                        = &client->dev;
    di->bat.type = POWER_SUPPLY_TYPE_BATTERY;
    di->bat.name = "battery";
    di->bat.properties = bq27x00_battery_props;
    di->bat.num_properties = ARRAY_SIZE(bq27x00_battery_props);
    di->bat.get_property = bq27x00_get_property;
    di->bat.external_power_changed = NULL;
    di->temp = 0;
    di->ntc_temp = 0;
    di->voltage = 0;
    di->current_avg = 0;
    di->capacity = 0;
    di->status = 0;
    di->sec = 0;
/*Manufacturer id check function */
    di->manufacturer = UNKNOW;
    di->battery_polling = 1;fake_temp=-990;fake_full_available_capacity=0;
    di->disable_led=0;
    di->health = POWER_SUPPLY_HEALTH_GOOD;
    i2c_set_clientdata(client, di);
    ret = power_supply_register(di->dev, &di->bat);
    if (ret) {
    	dev_dbg(di->dev,"failed to register battery\n");
    	//goto bk_batt_failed;
    }
/*Manufacturer id check function */
    di->manufacturer=check_manufacturer(di);
    bq27x00_read(REG_FULL_AVAILABLE_CHARGE, &g_full_available_capacity, 0, di);
    //create_bq_procfs(di);
#ifdef BAT_LOG
    INIT_DELAYED_WORK_DEFERRABLE(&di->bat_log_work,bat_log_work_func);
#endif
    INIT_DELAYED_WORK_DEFERRABLE(&di->bat_monitor_work,bat_monitor_work_func);
    queue_delayed_work(bat_work_queue,&di->bat_monitor_work,
            msecs_to_jiffies(1000 * 1));
    BLOCKING_INIT_NOTIFIER_HEAD(&notifier_list);

    /* Create a sysfs node */
    error = sysfs_create_group(&di->dev->kobj, &bq_attr_grp);
    if (error) {
    	dev_dbg(di->dev,"could not create sysfs_create_group\n");
    	return -1;
    }

#if defined(CONFIG_LAB126) && defined(CONFIG_EARLYSUSPEND)
	di->early_suspend.level = EARLY_SUSPEND_LEVEL_DISABLE_FB + 1;
	di->early_suspend.suspend = bq27541_early_suspend;
	di->early_suspend.resume = bq27541_late_resume;
	register_early_suspend(&di->early_suspend);
#endif

    return 0;
}

static int __devexit bq27541_remove(struct i2c_client *client)
{
    struct bq27541_info *di = i2c_get_clientdata(client);
    dev_dbg(di->dev,KERN_INFO "%s\n",__func__);
    cancel_delayed_work(&di->bat_monitor_work);
    flush_scheduled_work();
    kfree(di);
    remove_bq_procfs();
    return 0;
}

static void bq27541_shutdown(struct i2c_client *client)
{
	struct bq27541_info *di = i2c_get_clientdata(client);

	dev_info(di->dev, "%s\n", __FUNCTION__);
	cancel_delayed_work_sync(&di->bat_monitor_work);
}

static const struct i2c_device_id bq27541_id[] =  {
    { "bq27541", 0 },
    { },
};

static struct i2c_driver bq27541_i2c_driver = {
    .driver = {
        .name = "bq27541",
    },
    .suspend = bq27541_suspend,
    .resume = bq27541_resume,
    .probe = bq27541_probe,
    .remove = bq27541_remove,
    .id_table = bq27541_id,
	.shutdown = bq27541_shutdown,
};

static int __init bq27541_init(void)
{
    int ret = 0;
    printk(KERN_INFO "bq27541 Driver\n");
    bat_work_queue = create_singlethread_workqueue("battery");
    if (bat_work_queue == NULL) {
        ret = -ENOMEM;
    }
    ret = i2c_add_driver(&bq27541_i2c_driver);
    if (ret) {
        printk(KERN_ERR "bq27541: Could not add driver\n");
        return ret;
    }
    return 0;
}

static void __exit bq27541_exit(void)
{
    destroy_workqueue(bat_work_queue);
    i2c_del_driver(&bq27541_i2c_driver);
}

subsys_initcall(bq27541_init);
module_exit(bq27541_exit);

MODULE_DESCRIPTION("TI BQ27541 Driver");
MODULE_AUTHOR("Eric Nien");
MODULE_LICENSE("GPL");

