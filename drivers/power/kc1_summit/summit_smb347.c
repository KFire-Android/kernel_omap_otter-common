/*
 * Charger driver for Summit SMB347
 *
 * Copyright (C) Quanta Computer Inc. All rights reserved.
 *  Eric Nien <Eric.Nien@quantatw.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/param.h>
#include <linux/gpio.h>
#include "smb347.h"

struct workqueue_struct	*summit_work_queue;
static int kc1_chargerdetect_setting[] = {
/*0*/FCC_2500mA|PCC_150mA|TC_250mA ,
/*1*/DC_ICL_1800mA|USBHC_ICL_1800mA ,
/*2*/SUS_CTRL_BY_REGISTER|BAT_TO_SYS_NORMAL|VFLT_PLUS_200mV|
	AICL_ENABLE|AIC_TH_4200mV|USB_IN_FIRST|BAT_OV_END_CHARGE ,
/*3*/PRE_CHG_VOLTAGE_THRESHOLD_3_0|FLOAT_VOLTAGE_4_2_0 ,
/*4*/AUTOMATIC_RECHARGE_ENABLE|CURRENT_TERMINATION_ENABLE|
	BMD_VIA_THERM_IO|AUTO_RECHARGE_100mV|APSD_ENABLE|NC_APSD_ENABLE|
	SECONDARY_INPUT_NOT_ACCEPTED_IN_OVLO ,
/*5*/STAT_ACTIVE_LOW|STAT_CHARGEING_STATE|STAT_ENABLE|
	NC_INPUT_HC_SETTING|CC_TIMEOUT_764MIN|PC_TIMEOUT_48MIN ,
/*6*/LED_BLINK_DISABLE|EN_PIN_ACTIVE_LOW|USB_HC_CONTROL_BY_PIN|
	USB_HC_DUAL_STATE|CHARGER_ERROR_NO_IRQ|APSD_DONE_IRQ|
	DCIN_INPUT_PRE_BIAS_ENABLE ,
/*7*/0x80|MIN_SYS_3_4_5_V|THERM_MONITOR_VDDCAP|THERM_MONITOR_ENABLE|
	SOFT_COLD_CC_FV_COMPENSATION|SOFT_HOT_CC_FV_COMPENSATION ,
/*8*/INOK_OPERATION|USB_2|VFLT_MINUS_240mV|HARD_TEMP_CHARGE_SUSPEND|
	PC_TO_FC_THRESHOLD_ENABLE|INOK_ACTIVE_LOW ,
/*9*/RID_DISABLE_OTG_I2C_CONTROL|OTG_PIN_ACTIVE_HIGH|
	LOW_BAT_VOLTAGE_3_4_6_V ,
/*a*/CCC_700mA|DTRT_130C|OTG_CURRENT_LIMIT_500mA|
	OTG_BAT_UVLO_THRES_2_7_V ,
/*b*/0x61 ,
/*c*/AICL_COMPLETE_TRIGGER_IRQ ,
/*d*/INOK_TRIGGER_IRQ|LOW_BATTERY_TRIGGER_IRQ ,
};
static int kc1_chargerdetect_setting_pvt[] = {
/*0*/FCC_2500mA|PCC_150mA|TC_150mA ,
/*1*/DC_ICL_1800mA|USBHC_ICL_1800mA ,
/*2*/SUS_CTRL_BY_REGISTER|BAT_TO_SYS_NORMAL|VFLT_PLUS_200mV|
	AICL_ENABLE|AIC_TH_4200mV|USB_IN_FIRST|BAT_OV_END_CHARGE ,
/*3*/PRE_CHG_VOLTAGE_THRESHOLD_3_0|FLOAT_VOLTAGE_4_2_0 ,
/*4*/AUTOMATIC_RECHARGE_ENABLE|CURRENT_TERMINATION_ENABLE|
	BMD_VIA_THERM_IO|AUTO_RECHARGE_100mV|APSD_ENABLE|NC_APSD_ENABLE|
	SECONDARY_INPUT_NOT_ACCEPTED_IN_OVLO ,
/*5*/STAT_ACTIVE_LOW|STAT_CHARGEING_STATE|STAT_ENABLE|
	NC_INPUT_HC_SETTING|CC_TIMEOUT_764MIN|PC_TIMEOUT_48MIN ,
/*6*/LED_BLINK_DISABLE|EN_PIN_ACTIVE_LOW|USB_HC_CONTROL_BY_PIN|
	USB_HC_DUAL_STATE|CHARGER_ERROR_NO_IRQ|APSD_DONE_IRQ|
	DCIN_INPUT_PRE_BIAS_ENABLE ,
/*7*/0x80|MIN_SYS_3_4_5_V|THERM_MONITOR_VDDCAP|THERM_MONITOR_ENABLE|
	SOFT_COLD_CC_FV_COMPENSATION|SOFT_HOT_FV_COMPENSATION ,
/*8*/INOK_OPERATION|USB_2|VFLT_MINUS_240mV|HARD_TEMP_CHARGE_SUSPEND|
	PC_TO_FC_THRESHOLD_ENABLE|INOK_ACTIVE_LOW ,
/*9*/RID_DISABLE_OTG_I2C_CONTROL|OTG_PIN_ACTIVE_HIGH|
	LOW_BAT_VOLTAGE_3_4_6_V ,
/*a*/CCC_700mA|DTRT_130C|OTG_CURRENT_LIMIT_500mA|
	OTG_BAT_UVLO_THRES_2_7_V ,
/*b*/0xf5 ,
/*c*/AICL_COMPLETE_TRIGGER_IRQ ,
/*d*/INOK_TRIGGER_IRQ|LOW_BATTERY_TRIGGER_IRQ ,
};
static int otter2_1731[] = {
/*0*/FCC_2500mA|PCC_150mA|TC_150mA ,
/*1*/DC_ICL_1800mA|USBHC_ICL_1800mA ,
/*2*/SUS_CTRL_BY_REGISTER|BAT_TO_SYS_NORMAL|VFLT_PLUS_200mV|
	AICL_ENABLE|AIC_TH_4200mV|USB_IN_FIRST|BAT_OV_END_CHARGE ,
/*3*/PRE_CHG_VOLTAGE_THRESHOLD_3_0|FLOAT_VOLTAGE_4_2_0 ,
/*4*/AUTOMATIC_RECHARGE_ENABLE|CURRENT_TERMINATION_ENABLE|
	BMD_VIA_THERM_IO|AUTO_RECHARGE_100mV|APSD_ENABLE|NC_APSD_ENABLE|
	SECONDARY_INPUT_NOT_ACCEPTED_IN_OVLO ,
/*5*/STAT_ACTIVE_LOW|STAT_CHARGEING_STATE|STAT_ENABLE|
	NC_INPUT_HC_SETTING|CC_TIMEOUT_764MIN|PC_TIMEOUT_48MIN ,
/*6*/LED_BLINK_DISABLE|EN_PIN_ACTIVE_LOW|USB_HC_CONTROL_BY_PIN|
	USB_HC_DUAL_STATE|CHARGER_ERROR_NO_IRQ|APSD_DONE_IRQ|
	DCIN_INPUT_PRE_BIAS_ENABLE ,
/*7*/0x80|MIN_SYS_3_4_5_V|THERM_MONITOR_VDDCAP|THERM_MONITOR_ENABLE|
	SOFT_COLD_CC_FV_COMPENSATION|SOFT_HOT_FV_COMPENSATION ,
/*8*/INOK_OPERATION|USB_2|VFLT_MINUS_240mV|HARD_TEMP_CHARGE_SUSPEND|
	PC_TO_FC_THRESHOLD_ENABLE|INOK_ACTIVE_LOW ,
/*9*/RID_DISABLE_OTG_I2C_CONTROL|OTG_PIN_ACTIVE_HIGH|
	LOW_BAT_VOLTAGE_3_4_6_V ,
/*a*/CCC_700mA|DTRT_130C|OTG_CURRENT_LIMIT_500mA|
	OTG_BAT_UVLO_THRES_2_7_V ,
/*b*/0xa6 ,
/*c*/AICL_COMPLETE_TRIGGER_IRQ ,
/*d*/INOK_TRIGGER_IRQ|LOW_BATTERY_TRIGGER_IRQ ,
};
static int kc1_phydetect_setting[] = {
FCC_2500mA|PCC_150mA|TC_150mA ,
DC_ICL_1800mA|USBHC_ICL_1800mA ,
SUS_CTRL_BY_REGISTER|BAT_TO_SYS_NORMAL|VFLT_PLUS_200mV|
	AICL_ENABLE|AIC_TH_4200mV|USB_IN_FIRST|BAT_OV_END_CHARGE ,
PRE_CHG_VOLTAGE_THRESHOLD_3_0|FLOAT_VOLTAGE_4_2_0 ,
/*detect by omap*/
AUTOMATIC_RECHARGE_ENABLE|CURRENT_TERMINATION_ENABLE|
	BMD_VIA_THERM_IO|AUTO_RECHARGE_100mV|APSD_DISABLE|
	NC_APSD_DISABLE|SECONDARY_INPUT_NOT_ACCEPTED_IN_OVLO ,
STAT_ACTIVE_LOW|STAT_CHARGEING_STATE|STAT_DISABLE|
	NC_INPUT_HC_SETTING|CC_TIMEOUT_DISABLED|PC_TIMEOUT_DISABLED ,
LED_BLINK_DISABLE|CHARGE_EN_I2C_0|USB_HC_CONTROL_BY_REGISTER|
	USB_HC_TRI_STATE|CHARGER_ERROR_NO_IRQ|APSD_DONE_IRQ|
	DCIN_INPUT_PRE_BIAS_ENABLE ,
0X80|MIN_SYS_3_4_5_V|THERM_MONITOR_VDDCAP|SOFT_COLD_NO_RESPONSE
	|SOFT_HOT_NO_RESPONSE ,
INOK_OPERATION|USB_2|VFLT_MINUS_60mV|PC_TO_FC_THRESHOLD_ENABLE|
	HARD_TEMP_CHARGE_SUSPEND|INOK_ACTIVE_LOW ,
RID_DISABLE_OTG_I2C_CONTROL|OTG_PIN_ACTIVE_HIGH|
	LOW_BAT_VOLTAGE_3_5_8_V ,
CCC_700mA|DTRT_130C|OTG_CURRENT_LIMIT_500mA|
	OTG_BAT_UVLO_THRES_2_7_V ,
0x61 ,
TEMP_OUTSIDE_COLD_HOT_HARD_LIMIITS_TRIGGER_IRQ|
	TEMP_OUTSIDE_COLD_HOT_SOFT_LIMIITS_TRIGGER_IRQ|
	USB_OVER_VOLTAGE_TRIGGER_IRQ|USB_UNDER_VOLTAGE_TRIGGER_IRQ|
	AICL_COMPLETE_TRIGGER_IRQ ,
CHARGE_TIMEOUT_TRIGGER_IRQ|TERMINATION_OR_TAPER_CHARGING_TRIGGER_IRQ|
	FAST_CHARGING_TRIGGER_IRQ|INOK_TRIGGER_IRQ|
	MISSING_BATTERY_TRIGGER_IRQ|LOW_BATTERY_TRIGGER_IRQ ,
};

int aicl_results[]={
	300 , 500 , 700 , 900 , 1200 , 1500 , 1800 , 2000 , 2200 ,
	2500 , 2500 , 2500 , 2500 , 2500 , 2500 , 2500
};
int accc[]={
	100 , 150 , 200 , 250 , 700 , 900 , 1200 , 1500 , 1800 ,
	2000 , 2200 , 2500
};
int summit_bat_notifier_call(struct notifier_block *nb ,  unsigned long val ,
		 void *priv)
{
	int result = NOTIFY_OK;
	struct summit_smb347_info *di = container_of(nb ,
		struct summit_smb347_info ,  bat_notifier);
	writeIntoCBuffer(&di->events , val);
	queue_delayed_work(summit_work_queue ,
		&di->summit_monitor_work , 0);
	return result;

}
int summit_usb_notifier_call(struct notifier_block *nb ,  unsigned long val ,
		 void *priv) {
	int result = NOTIFY_OK;
	struct summit_smb347_info *di = container_of(nb ,
		struct summit_smb347_info ,  usb_notifier);
	if (val == USB_EVENT_DETECT_SOURCE)
		wake_lock(&di->chrg_lock);
	writeIntoCBuffer(&di->events , val);
	queue_delayed_work(summit_work_queue ,
		&di->summit_monitor_work , 0);
	return result;
}
static irqreturn_t summit_charger_interrupt(int irq , void *_di)
{
	struct summit_smb347_info *di = _di;
	wake_lock(&di->summit_lock);
	writeIntoCBuffer(&di->events ,
		EVENT_INTERRUPT_FROM_CHARGER);
	queue_delayed_work(summit_work_queue ,
		&di->summit_monitor_work , 0);
	return IRQ_HANDLED;
}
void summit_monitor_work_func(struct work_struct *work){
	struct summit_smb347_info *di = container_of(work ,
		struct summit_smb347_info ,  summit_monitor_work.work);
	while (isCBufferNotEmpty(&di->events)) {
		int event = readFromCBuffer(&di->events);
		if (event == EVENT_INTERRUPT_FROM_CHARGER) {
			summit_read_interrupt_a(di);
			mdelay(1);
			summit_read_interrupt_b(di);
			mdelay(1);
			summit_read_interrupt_c(di);
			mdelay(1);
			summit_read_interrupt_d(di);
			mdelay(1);
			summit_read_interrupt_e(di);
			mdelay(1);
			summit_read_interrupt_f(di);
			wake_unlock(&di->summit_lock);
		} else
			summit_fsm_stateTransform(di , event);
	}
}
void summit_check_work_func(struct work_struct *work){
	struct summit_smb347_info *di = container_of(work ,
		struct summit_smb347_info ,  summit_check_work.work);
	summit_read_interrupt_d(di);
}
/*
* Battery missing detect only has effect when enable charge.
*
**/
int summit_check_bmd(struct summit_smb347_info *di){
	int value = 0;
	int result = 0;
	value = i2c_smbus_read_byte_data(di->client ,
		SUMMIT_SMB347_INTSTAT_REG_B);
	result = BAT_MISS_STATUS_BIT(value);
	if (result)
		dev_dbg(di->dev , "%s:Battery Miss\n" , __func__);
	else
		dev_dbg(di->dev , "%s:Battery Not Miss\n" , __func__);
	return result;
}

void summit_switch_mode(struct summit_smb347_info *di ,
	int mode){
	int command = 0;
	int value = 0;
	value = i2c_smbus_read_byte_data(di->client ,
		SUMMIT_SMB347_ENABLE_CONTROL);
	if (IS_BIT_SET(value , 4)) {
		/*USB_HC_CONTROL_BY_REGISTER*/
		summit_write_config(di , 1);
		CLEAR_BIT(value , 4);
		i2c_smbus_write_byte_data(di->client ,
			SUMMIT_SMB347_ENABLE_CONTROL , value);
		summit_write_config(di , 0);
	}
	switch (mode) {
	case USB1_OR_15_MODE:
			COMMAND_USB1(command);
			dev_dbg(di->dev , "%s:USB1_MODE\n" , __func__);
		break;
	case USB5_OR_9_MODE:
			COMMAND_USB5(command);
			dev_dbg(di->dev , "%s:USB5_MODE\n" , __func__);
		break;
	case HC_MODE:
			COMMAND_HC_MODE(command);
			dev_dbg(di->dev , "%s:HC_MODE\n" , __func__);
	break;
	}
	i2c_smbus_write_byte_data(di->client ,
		SUMMIT_SMB347_COMMAND_REG_B , command);
}

int summit_charge_reset(struct summit_smb347_info *di){
	int command = 0;
	COMMAND_POWER_ON_RESET(command);
	i2c_smbus_write_byte_data(di->client ,
		SUMMIT_SMB347_COMMAND_REG_B , command);
	/* Recommended delay is 20ms ,  make it 50 ms to be really safe */
	mdelay(50);
	return 0;
}
int summit_config_charge_enable(struct summit_smb347_info *di ,
	int setting) {
	int config = 0;
	if (setting != CHARGE_EN_I2C_0 &&
		setting != CHARGE_EN_I2C_1 &&
		setting != EN_PIN_ACTIVE_HIGH &&
		setting != EN_PIN_ACTIVE_LOW)
		return 0;
	summit_write_config(di , 1);
	mdelay(1);
	config = i2c_smbus_read_byte_data(di->client ,
		SUMMIT_SMB347_ENABLE_CONTROL);
	dev_info(di->dev , "%s:config=0x%x\n" , __func__ , config);
	CLEAR_BIT(config , 5);
	CLEAR_BIT(config , 6);
	config |= setting;
	dev_info(di->dev , "%s:config=0x%x\n" , __func__ , config);
	i2c_smbus_write_byte_data(di->client ,
		SUMMIT_SMB347_ENABLE_CONTROL , config);
	mdelay(1);
	config = i2c_smbus_read_byte_data(di->client ,
		SUMMIT_SMB347_ENABLE_CONTROL);
	dev_info(di->dev , "%s:config=0x%x\n" , __func__ , config);
	summit_write_config(di , 0);
	return 1;
}

int summit_charge_enable(struct summit_smb347_info *di ,
	int enable) {
	int command = 0;
	int config , result = 0;
	config = i2c_smbus_read_byte_data(di->client ,
		SUMMIT_SMB347_ENABLE_CONTROL);
	config &= 0x60;
	switch (config) {
	case CHARGE_EN_I2C_0:
			command = i2c_smbus_read_byte_data(di->client ,
				SUMMIT_SMB347_COMMAND_REG_A);
			if (command < 0) {
				dev_err(di->dev ,  "Got I2C error %d in %s\n" ,
					command ,  __func__);
				return -1;
			}
			if (enable)
				SET_BIT(command , 1);
			else
				CLEAR_BIT(command , 1);
			i2c_smbus_write_byte_data(di->client ,
				SUMMIT_SMB347_COMMAND_REG_A , command);
		break;
	case CHARGE_EN_I2C_1:
			command = i2c_smbus_read_byte_data(di->client ,
				SUMMIT_SMB347_COMMAND_REG_A);
			if (command < 0) {
				dev_err(di->dev ,  "Got I2C error %d in %s\n" ,
					command ,  __func__);
				return -1;
			}
			if (enable)
				CLEAR_BIT(command , 1);
			else
				CLEAR_BIT(command , 1);
			i2c_smbus_write_byte_data(di->client ,
				SUMMIT_SMB347_COMMAND_REG_A , command);
		break;
	case EN_PIN_ACTIVE_HIGH:
			if (enable)
				gpio_set_value(di->pin_en ,  1);
			else
				gpio_set_value(di->pin_en ,  0);
		break;
	case EN_PIN_ACTIVE_LOW:
			if (enable)
				gpio_set_value(di->pin_en ,  0);
			else
				gpio_set_value(di->pin_en ,  1);
		break;
	}
	return result;
}
void summit_config_suspend_by_register(struct summit_smb347_info *di ,
	int byregister) {
	int config = 0;
	summit_write_config(di , 1);
	config = i2c_smbus_read_byte_data(di->client ,
		SUMMIT_SMB347_FUNCTIONS);
	if (byregister) {
		/*Controlled by Register*/
		SET_BIT(config , 7);
		i2c_smbus_write_byte_data(di->client ,
			SUMMIT_SMB347_FUNCTIONS , config);
	} else {
		CLEAR_BIT(config , 7);
		i2c_smbus_write_byte_data(di->client ,
			SUMMIT_SMB347_FUNCTIONS , config);
	}
	summit_write_config(di , 0);
}
void summit_suspend_mode(struct summit_smb347_info *di ,
	int enable) {
	int config = 0;
	int command = 0;
	config = i2c_smbus_read_byte_data(di->client ,
		SUMMIT_SMB347_FUNCTIONS);
	if (IS_BIT_SET(config , 7)) {
		/*Controlled by Register*/
		command = i2c_smbus_read_byte_data(di->client ,
			SUMMIT_SMB347_COMMAND_REG_A);
		if (command < 0) {
			dev_err(di->dev ,  "Got I2C error %d in %s\n" ,
				command ,  __func__);
			return;
		}

		if (enable)
			COMMAND_SUSPEND_MODE_ENABLE(command);
		else
			COMMAND_SUSPEND_MODE_DISABLE(command);
		i2c_smbus_write_byte_data(di->client ,
			SUMMIT_SMB347_COMMAND_REG_A , command);
	} else {
		/*Controlled by Pin*/
		if (enable)
			gpio_set_value(di->pin_susp ,  0);
		else
			gpio_set_value(di->pin_susp ,  1);
	}
}
void summit_force_precharge(struct summit_smb347_info *di ,
	int enable) {
	int config , command = 0;
	summit_write_config(di , 1);
	command = i2c_smbus_read_byte_data(di->client ,
		SUMMIT_SMB347_COMMAND_REG_A);
	if (command < 0) {
		dev_err(di->dev ,  "Got I2C error %d in %s\n" ,
			command ,  __func__);
		return;
	}

	config = i2c_smbus_read_byte_data(di->client ,
		SUMMIT_SMB347_SYSOK_USB30);
	if (enable) {
		SET_BIT(config , 1);
		COMMAND_FORCE_PRE_CHARGE_CURRENT_SETTING(command);
	} else {
		CLEAR_BIT(config , 1);
		COMMAND_ALLOW_FAST_CHARGE_CURRENT_SETTING(command);
	}
	i2c_smbus_write_byte_data(di->client ,
		SUMMIT_SMB347_COMMAND_REG_A , command);
	i2c_smbus_write_byte_data(di->client ,
		SUMMIT_SMB347_SYSOK_USB30 , config);
	summit_write_config(di , 0);
}
void summit_charger_stat_output(struct summit_smb347_info *di ,
	int enable){
	int command = 0;
	command = i2c_smbus_read_byte_data(di->client ,
		SUMMIT_SMB347_COMMAND_REG_A);
	if (command < 0) {
		dev_err(di->dev ,  "Got I2C error %d in %s\n" ,
			command ,  __func__);
		return;
	}

	if (enable)
		COMMAND_STAT_OUTPUT_ENABLE(command);
	else
		COMMAND_STAT_OUTPUT_DISABLE(command);
	i2c_smbus_write_byte_data(di->client ,
		SUMMIT_SMB347_COMMAND_REG_A , command);
}

void summit_write_config(struct summit_smb347_info *di ,
	int enable) {
	int command = 0;
	command = i2c_smbus_read_byte_data(di->client ,
		SUMMIT_SMB347_COMMAND_REG_A);
	if (command < 0) {
		dev_err(di->dev ,  "Got I2C error %d in %s\n" ,
			command ,  __func__);
		return;
	}

	if (enable)
		COMMAND_ALLOW_WRITE_TO_CONFIG_REGISTER(command);
	else
		COMMAND_NOT_ALLOW_WRITE_TO_CONFIG_REGISTER(command);
	i2c_smbus_write_byte_data(di->client ,
		SUMMIT_SMB347_COMMAND_REG_A , command);
}

extern int get_battery_mAh(void);
int pre_current[4] = {
100 , 150 , 200 , 250
};
int fast_current[8] = {
700 , 900 , 1200 , 1500 , 1800 , 2000 , 2200 , 2500
};
int summit_find_pre_cc(int cc){
	int i = 0;
	for (i = 3 ; i > -1 ; i--) {
		if (pre_current[i] <= cc)
			break;
	}
	return i;
}
int summit_find_fast_cc(int cc){
	int i = 0;
	for (i = 7 ; i > -1 ; i--) {
		if (fast_current[i] <= cc)
			break;
	}
	return i;
}
int summit_find_aicl(int aicl){
	int i = 0;
	for (i = 9 ; i > -1 ; i--) {
		if (aicl_results[i] <= aicl)
			break;
	}
	return i;
}
extern void twl6030_poweroff(void);
/*
*EVENT_TEMP_PROTECT_STEP_3:0 < temp < 8
*EVENT_TEMP_PROTECT_STEP_4:8 < temp < 14
*	find the fast charge current for 0.18C
*	if No suit fast charge current
*	then find the pre charge current for 0.18C
*		if No suit current , then stop charge
*EVENT_TEMP_PROTECT_STEP_5:14 < temp < 23  ,  0.5C (max) to 4.2V
*EVENT_TEMP_PROTECT_STEP_6:23 < temp < 45  ,  0.7C (max) to 4.2V
*EVENT_TEMP_PROTECT_STEP_7:45 < temp < 60  ,  0.5C (max) to 4.0V
*	find the fast charge current for 0.5C or 0.7C
*	if No suit fast charge current
*	then find the pre charge current for 0.5C
*		if No suit current , then stop charge
*/
void summit_adjust_charge_current(struct summit_smb347_info *di , int event)
{
	int C = 0;
	int i , cc = 0;
	C = get_battery_mAh();
	dev_info(di->dev , " C=%d mAh\n" , C);
	switch (event) {
	case EVENT_TEMP_PROTECT_STEP_4:
	case EVENT_TEMP_PROTECT_STEP_3:
			cc = C * 18 / 100;
			i = summit_find_fast_cc(cc);
			if (i > -1) {
				summit_force_precharge(di , 0);
				summit_config_charge_current(di , i << 5 ,
					CONFIG_NO_CHANGE , CONFIG_NO_CHANGE);
				CLEAR_BAT_NO_SUIT_CURRENT(di->protect_event);
			} else {
				i = summit_find_pre_cc(cc);
				if (i > -1) {
					summit_force_precharge(di , 1);
					summit_config_charge_current(di , CONFIG_NO_CHANGE ,
						i << 3 , CONFIG_NO_CHANGE);
					CLEAR_BAT_NO_SUIT_CURRENT(di->protect_event);
				} else {
					summit_force_precharge(di , 0);
					SET_BAT_NO_SUIT_CURRENT(di->protect_event);
				}
			}
		break;
	case EVENT_TEMP_PROTECT_STEP_5:
	case EVENT_TEMP_PROTECT_STEP_6:
	case EVENT_TEMP_PROTECT_STEP_7:
			if ((event == EVENT_TEMP_PROTECT_STEP_7) ||
				(event == EVENT_TEMP_PROTECT_STEP_5))
				cc = C >> 1;
			else
				cc = C * 70 / 100;
			i = summit_find_fast_cc(cc);
			if (i > -1) {
				summit_force_precharge(di , 0);
				summit_config_charge_current(di , i << 5 ,
					CONFIG_NO_CHANGE , CONFIG_NO_CHANGE);
				CLEAR_BAT_NO_SUIT_CURRENT(di->protect_event);
			} else {
				i = summit_find_pre_cc(cc);
				if (i > -1) {
					summit_force_precharge(di , 1);
					summit_config_charge_current(di , CONFIG_NO_CHANGE ,
						i << 3 , CONFIG_NO_CHANGE);
					CLEAR_BAT_NO_SUIT_CURRENT(di->protect_event);
				} else {
					summit_force_precharge(di , 0);
					SET_BAT_NO_SUIT_CURRENT(di->protect_event);
				}
			}
		break;
	}
}
int summit_protection(struct summit_smb347_info *di){
	if ((IS_BAT_TOO_WEAK(di->protect_enable) &&
		IS_BAT_TOO_WEAK(di->protect_event)) ||
		(IS_BAT_TOO_COLD_FOR_DISCHARGE(di->protect_enable) &&
		IS_BAT_TOO_COLD_FOR_DISCHARGE(di->protect_event)) ||
		(IS_BAT_TOO_HOT_FOR_CHARGE(di->protect_enable) &&
		IS_BAT_TOO_HOT_FOR_CHARGE(di->protect_event)))
		twl6030_poweroff();

	if ((IS_BAT_FULL(di->protect_enable) &&
		IS_BAT_FULL(di->protect_event)) ||
		(IS_BAT_I2C_FAIL(di->protect_enable) &&
		IS_BAT_I2C_FAIL(di->protect_event)) ||
		(IS_BAT_NOT_RECOGNIZE(di->protect_enable) &&
		IS_BAT_NOT_RECOGNIZE(di->protect_event)) ||
		(IS_BAT_NTC_ERR(di->protect_enable) &&
		IS_BAT_NTC_ERR(di->protect_event)) ||
		(IS_BAT_TOO_COLD_FOR_CHARGE(di->protect_enable) &&
		IS_BAT_TOO_COLD_FOR_CHARGE(di->protect_event)) ||
		(IS_BAT_NO_SUIT_CURRENT(di->protect_enable) &&
		IS_BAT_NO_SUIT_CURRENT(di->protect_event)))
		summit_charge_enable(di , 0);
	else
		summit_charge_enable(di , 1);
	return 0;
}
void summit_config_charge_voltage(struct summit_smb347_info *di ,
	int pre_volt , int float_volt)
{
	int config = 0;
	summit_write_config(di , 1);
	mdelay(1);
	config = i2c_smbus_read_byte_data(di->client ,
		SUMMIT_SMB347_FLOAT_VOLTAGE);
	if (pre_volt != CONFIG_NO_CHANGE) {
		config &= ~PRE_CHG_VOLTAGE_CLEAN;
		config |= pre_volt;
	}
	if (float_volt != CONFIG_NO_CHANGE) {
		config &= ~FLOAT_VOLTAGE_CLEAN;
		config |= float_volt;
	}
	i2c_smbus_write_byte_data(di->client , SUMMIT_SMB347_FLOAT_VOLTAGE ,
		config);
	mdelay(1);
	summit_write_config(di , 0);
}
void summit_config_charge_current(struct summit_smb347_info *di ,
	int fc_current , int pc_current , int tc_current)
{
	int config = 0;
	summit_write_config(di , 1);
	mdelay(1);
	config = i2c_smbus_read_byte_data(di->client ,
		SUMMIT_SMB347_CHARGE_CURRENT);
	if (fc_current != CONFIG_NO_CHANGE) {
		config &= ~FCC_CLEAN;
		config |= fc_current;
	}
	if (pc_current != CONFIG_NO_CHANGE) {
		config &= ~PCC_CLEAN;
		config |= pc_current;
	}
	if (tc_current != CONFIG_NO_CHANGE) {
		config &= ~TC_CLEAN;
		config |= tc_current;
	}
	i2c_smbus_write_byte_data(di->client , SUMMIT_SMB347_CHARGE_CURRENT ,
		config);
	mdelay(1);
	summit_write_config(di , 0);
}

void summit_config_inpu_current(struct summit_smb347_info *di ,
	int dc_in , int usb_in){
	int config = 0;
	summit_write_config(di , 1);
	mdelay(1);
	config = i2c_smbus_read_byte_data(di->client ,
		SUMMIT_SMB347_INPUT_CURR_LIMIT);
	if (dc_in != CONFIG_NO_CHANGE) {
		config &= ~DC_ICL_CLEAN;
		config |= dc_in;
	}
	if (usb_in != CONFIG_NO_CHANGE) {
		config &= ~USBHC_ICL_CLEAN;
		config |= usb_in;
	}
	i2c_smbus_write_byte_data(di->client ,
		SUMMIT_SMB347_INPUT_CURR_LIMIT , config);
	mdelay(1);
	summit_write_config(di , 0);
}
void summit_config_voltage(struct summit_smb347_info *di , int pre_vol_thres ,
	int fast_vol_thres){
	int config = 0;
	summit_write_config(di , 1);
	mdelay(1);
	config = i2c_smbus_read_byte_data(di->client ,
		SUMMIT_SMB347_FLOAT_VOLTAGE);
	if (pre_vol_thres != CONFIG_NO_CHANGE) {
		config &= ~PRE_CHG_VOLTAGE_CLEAN;
		config |= pre_vol_thres;
	}
	if (fast_vol_thres != CONFIG_NO_CHANGE) {
		config &= ~FLOAT_VOLTAGE_CLEAN;
		config |= fast_vol_thres;
	}
	i2c_smbus_write_byte_data(di->client , SUMMIT_SMB347_FLOAT_VOLTAGE ,
		config);
	mdelay(1);
	summit_write_config(di , 0);
}
void summit_config_apsd(struct summit_smb347_info *di ,
	int enable)	{
	int config = 0;
	summit_write_config(di , 1);
	config = i2c_smbus_read_byte_data(di->client , 0x6);
	config |= 0x2;
	i2c_smbus_write_byte_data(di->client , 0x6 , config);
	mdelay(10);
	config = i2c_smbus_read_byte_data(di->client ,
		SUMMIT_SMB347_CHARGE_CONTROL);
	if (enable)
		SET_APSD_ENABLE(config);
	else
		SET_APSD_DISABLE(config);
	i2c_smbus_write_byte_data(di->client , SUMMIT_SMB347_CHARGE_CONTROL ,
		config);
	summit_write_config(di , 0);
}
void summit_config_aicl(struct summit_smb347_info *di ,
	int enable , int aicl_thres) {
	int config = 0;
	summit_write_config(di , 1);
	mdelay(1);
	config = i2c_smbus_read_byte_data(di->client ,
		SUMMIT_SMB347_FUNCTIONS);
	if (enable)
		SET_AICL_ENABLE(config);
	else
		SET_AICL_DISABLE(config);
	if (aicl_thres == 4200)
		SET_AIC_TH_4200mV(config);
	else if (aicl_thres == 4500)
		SET_AIC_TH_4500mV(config);
	i2c_smbus_write_byte_data(di->client , SUMMIT_SMB347_FUNCTIONS ,
		config);
	mdelay(1);
	summit_write_config(di , 0);
}
void summit_enable_fault_interrupt(struct summit_smb347_info *di ,
	int enable) {
	int config = 0;
	summit_write_config(di , 1);
	mdelay(1);
	config = i2c_smbus_read_byte_data(di->client ,
		SUMMIT_SMB347_FAULT_INTERRUPT);
	config |= enable;
	i2c_smbus_write_byte_data(di->client , SUMMIT_SMB347_FAULT_INTERRUPT ,
		config);
	mdelay(1);
	summit_write_config(di , 0);
}
void summit_disable_fault_interrupt(struct summit_smb347_info *di ,
	int disable) {
	int config = 0;
	summit_write_config(di , 1);
	mdelay(1);
	config = i2c_smbus_read_byte_data(di->client ,
		SUMMIT_SMB347_FAULT_INTERRUPT);
	config &= ~disable;
	i2c_smbus_write_byte_data(di->client , SUMMIT_SMB347_FAULT_INTERRUPT ,
		config);
	mdelay(1);
	summit_write_config(di , 0);
}
void summit_enable_stat_interrupt(struct summit_smb347_info *di ,
	int enable) {
	int config = 0;
	summit_write_config(di , 1);
	mdelay(1);
	config = i2c_smbus_read_byte_data(di->client ,
		SUMMIT_SMB347_INTERRUPT_STAT);
	config |= enable;
	i2c_smbus_write_byte_data(di->client , SUMMIT_SMB347_INTERRUPT_STAT ,
		config);
	mdelay(1);
	summit_write_config(di , 0);
}
void summit_disable_stat_interrupt(struct summit_smb347_info *di ,
	int disable) {
	int config = 0;
	summit_write_config(di , 1);
	mdelay(1);
	config = i2c_smbus_read_byte_data(di->client ,
		SUMMIT_SMB347_INTERRUPT_STAT);
	config &= ~disable;
	i2c_smbus_write_byte_data(di->client , SUMMIT_SMB347_INTERRUPT_STAT ,
		config);
	mdelay(1);
	summit_write_config(di , 0);
}
void summit_config_time_protect(struct summit_smb347_info *di ,
	int enable) {
	int config = 0;
	summit_write_config(di , 1);
	config = i2c_smbus_read_byte_data(di->client ,
		SUMMIT_SMB347_STAT_TIMERS);
	if (enable) {
		/*enable Pre-charge Timeout 48 min*/
		CLEAR_BIT(config , 1); CLEAR_BIT(config , 0);
		/*enable complete charge timeout 764 min*/
		CLEAR_BIT(config , 3); SET_BIT(config , 2);
	} else {
		/*disable Pre-charge timeout*/
		SET_BIT(config , 1); SET_BIT(config , 0);
		/*disable complete charge timeout*/
		SET_BIT(config , 2); SET_BIT(config , 3);
	}
	i2c_smbus_write_byte_data(di->client , SUMMIT_SMB347_STAT_TIMERS ,
		config);
	summit_write_config(di , 0);
}
int summit_get_mode(struct summit_smb347_info *di){
	int value = 0;
	int temp = 0;
	value = i2c_smbus_read_byte_data(di->client ,
		SUMMIT_SMB347_STATUS_REG_E);
	temp = value;
	return USB15_HC_MODE(temp);
}
int summit_get_apsd_result(struct summit_smb347_info *di){
	int status , result;
	status = i2c_smbus_read_byte_data(di->client ,
		SUMMIT_SMB347_STATUS_REG_D);
	result = status;
	if (APSD_STATUS(status)) {
		switch (APSD_RESULTS(result)) {
		case APSD_NOT_RUN:
				dev_info(di->dev , "APSD_NOT_RUN\n");
			break;
		case APSD_CHARGING_DOWNSTREAM_PORT:
				dev_info(di->dev , "CHARGING_DOWNSTREAM_PORT\n");
				return USB_EVENT_HOST_CHARGER;
			break;
		case APSD_DEDICATED_DOWNSTREAM_PORT:
				dev_info(di->dev , "DEDICATED_DOWNSTREAM_PORT\n");
				return USB_EVENT_CHARGER;
			break;
		case APSD_OTHER_DOWNSTREAM_PORT:
				dev_info(di->dev , "OTHER_DOWNSTREAM_PORT\n");
				return EVENT_DETECT_OTHER;
			break;
		case APSD_STANDARD_DOWNSTREAM_PORT:
				dev_info(di->dev , "STANDARD_DOWNSTREAM_PORT\n");
				return EVENT_DETECT_PC;
			break;
		case APSD_ACA_CHARGER:
				dev_info(di->dev , "ACA_CHARGER\n");
			break;
		case APSD_TBD:
				dev_info(di->dev , "TBD\n");
				return EVENT_DETECT_TBD;
			break;
		}
	} else {
		 dev_dbg(di->dev , "APSD NOt Completed\n");
		 return EVENT_APSD_NOT_COMPLETE;
	}
	return -1;
}
int summit_get_aicl_result(struct summit_smb347_info *di){
	int value = 0;
	value = i2c_smbus_read_byte_data(di->client ,
		SUMMIT_SMB347_STATUS_REG_E);
	return aicl_results[AICL_RESULT(value)];
}
void summit_read_interrupt_a(struct summit_smb347_info *di){
	int value = 0;
	value = i2c_smbus_read_byte_data(di->client ,
		SUMMIT_SMB347_INTSTAT_REG_A);
	if (HOT_TEMP_HARD_LIMIT_IRQ & value) {
		dev_dbg(di->dev , "Hot Temperature Hard Limit IRQ\n");
		if (HOT_TEMP_HARD_LIMIT_STATUS & value) {
			dev_dbg(di->dev , "Event:Over Hot Temperature Hard Limit\n");
			writeIntoCBuffer(&di->events ,
				EVENT_OVER_HOT_TEMP_HARD_LIMIT);
			queue_delayed_work(summit_work_queue ,
				&di->summit_monitor_work , msecs_to_jiffies(1));
		} else {
			dev_dbg(di->dev , "Event:Below Hot Temperature Hard Limit Status\n");
			writeIntoCBuffer(&di->events ,
				EVENT_BELOW_HOT_TEMP_HARD_LIMIT);
			queue_delayed_work(summit_work_queue ,
				&di->summit_monitor_work , msecs_to_jiffies(1));
		}
	}
	if (COLD_TEMP_HARD_LIMIT_IRQ & value) {
		dev_dbg(di->dev , "Cold Temperature Hard Limit IRQ\n");
		if (COLD_TEMP_HARD_LIMIT_STATUS & value) {
			dev_dbg(di->dev , "Event:Over Cold Temperature Hard Limit Status\n");
			writeIntoCBuffer(&di->events ,
				EVENT_OVER_COLD_TEMP_HARD_LIMIT);
			queue_delayed_work(summit_work_queue ,
				&di->summit_monitor_work , msecs_to_jiffies(1));
		} else {
			dev_dbg(di->dev , "Event:Below Cold Temperature Hard Limit Status\n");
			writeIntoCBuffer(&di->events ,
				EVENT_BELOW_COLD_TEMP_HARD_LIMIT);
			queue_delayed_work(summit_work_queue ,
				&di->summit_monitor_work , msecs_to_jiffies(1));
		}
	}
	if (HOT_TEMP_SOFT_LIMIT_IRQ & value) {
		dev_dbg(di->dev , "Hot Temperature Soft Limit IRQ\n");
		if (HOT_TEMP_SOFT_LIMIT_STATUS & value) {
			dev_dbg(di->dev , "Event:Over Hot Temperature Soft Limit\n");
			writeIntoCBuffer(&di->events ,
				EVENT_OVER_HOT_TEMP_SOFT_LIMIT);
			queue_delayed_work(summit_work_queue ,
				&di->summit_monitor_work , msecs_to_jiffies(1));
		} else {
			dev_dbg(di->dev , "Event:Below Hot Temperature Soft Limit\n");
			writeIntoCBuffer(&di->events ,
				EVENT_BELOW_HOT_TEMP_SOFT_LIMIT);
			queue_delayed_work(summit_work_queue ,
				&di->summit_monitor_work , msecs_to_jiffies(1));
		}
	}
	if (COLD_TEMP_SOFT_LIMIT_IRQ & value) {
		dev_dbg(di->dev , "Cold Temperature Soft Limit IRQ\n");
		if (COLD_TEMP_SOFT_LIMIT_STATUS & value) {
			dev_dbg(di->dev , "Event:Over Cold Temperature Soft Limit Status\n");
			writeIntoCBuffer(&di->events ,
				EVENT_OVER_COLD_TEMP_SOFT_LIMIT);
			queue_delayed_work(summit_work_queue ,
				&di->summit_monitor_work , msecs_to_jiffies(1));
		} else {
			dev_dbg(di->dev , "Event:Below Hot Temperature Soft Limit Status\n");
			writeIntoCBuffer(&di->events ,
				EVENT_BELOW_COLD_TEMP_SOFT_LIMIT);
			queue_delayed_work(summit_work_queue ,
				&di->summit_monitor_work , msecs_to_jiffies(1));
		}
	}
}

void summit_read_interrupt_b(struct summit_smb347_info *di){
	int value = 0;
	value = i2c_smbus_read_byte_data(di->client ,
		SUMMIT_SMB347_INTSTAT_REG_B);
	if (BAT_OVER_VOLTAGE_IRQ & value) {
		dev_dbg(di->dev , "Battery Over-voltage Condition IRQ\n");
		if (BAT_OVER_VOLTAGE_STATUS & value)
			dev_dbg(di->dev , "Battery Over-voltage Status\n");
	}
	if (MISSING_BAT_IRQ & value) {
		dev_info(di->dev , "Missing Batter IRQ\n");
		if (MISSING_BAT_STATUS & value)
			dev_info(di->dev , "Missing Battery\n");
		else
			dev_info(di->dev , "Not Missing Battery\n");
	}
	if (LOW_BAT_VOLTAGE_IRQ & value) {
		dev_info(di->dev , "Low-battery Voltage IRQ\n");
		if (LOW_BAT_VOLTAGE_STATUS & value) {
			dev_info(di->dev , "Below Low-battery Voltage\n");
			writeIntoCBuffer(&di->events ,
				EVENT_BELOW_LOW_BATTERY);
			queue_delayed_work(summit_work_queue ,
				&di->summit_monitor_work , msecs_to_jiffies(1));
		} else {
			dev_info(di->dev , "Above Low-battery Voltage\n");
			writeIntoCBuffer(&di->events ,
				EVENT_OVER_LOW_BATTERY);
			queue_delayed_work(summit_work_queue ,
				&di->summit_monitor_work , msecs_to_jiffies(1));
		}
	}
	if (PRE_TO_FAST_CHARGE_VOLTAGE_IRQ & value) {
		dev_dbg(di->dev , "Pre-to-Fast Charge Battery Voltage IRQ\n");
		if (PRE_TO_FAST_CHARGE_VOLTAGE_STATUS & value)
			dev_dbg(di->dev , "Pre-to-Fast Charge Battery Voltage Status\n");
	}
}
void summit_read_interrupt_c(struct summit_smb347_info *di){
	int value = 0;
	value = i2c_smbus_read_byte_data(di->client ,
		SUMMIT_SMB347_INTSTAT_REG_C);
	if (INTERNAL_TEMP_LIMIT_IRQ & value) {
		dev_dbg(di->dev , "Internal Temperature Limit IRQ\n");
		if (INTERNAL_TEMP_LIMIT_STATUS & value)
			dev_dbg(di->dev , "Internal Temperature Limit Status\n");
	}
	if (RE_CHARGE_BAT_THRESHOLD_IRQ & value) {
		dev_dbg(di->dev , "Re-charge Battery Threshold IRQ\n");
		if (RE_CHARGE_BAT_THRESHOLD_STATUS & value)
			dev_dbg(di->dev , "Re-charge Battery Threshold Status\n");
	}
	if (TAPER_CHARGER_MODE_IRQ & value) {
		dev_dbg(di->dev , "Taper Charging Mode IRQ\n");
		if (TAPER_CHARGER_MODE_STATUS & value)
			dev_dbg(di->dev , "Taper Charging Mode Status\n");
	}
	if (TERMINATION_CC_IRQ & value) {
		dev_dbg(di->dev , "Termination Charging Current Hit IRQ\n");
		if (TERMINATION_CC_STATUS & value)
			dev_dbg(di->dev , "Termination Charging Current Hit Status\n");
	}
}
void summit_read_interrupt_d(struct summit_smb347_info *di){
	int value = 0;
#ifdef CONFIG_LAB126
	char *thermal_metric_prefix = "smb347:def:";
	char buf[THERMO_METRICS_STR_LEN];
#endif
	value = i2c_smbus_read_byte_data(di->client ,
		SUMMIT_SMB347_INTSTAT_REG_D);
	if (APSD_COMPLETED_IRQ & value) {
		dev_dbg(di->dev , "APSD Completed IRQ\n");
		if (APSD_COMPLETED_STATUS & value) {
			dev_dbg(di->dev , "Event:APSD Completed Status\n");
			writeIntoCBuffer(&di->events ,
				EVENT_APSD_COMPLETE);
			queue_delayed_work(summit_work_queue ,
				&di->summit_monitor_work , msecs_to_jiffies(1));
		}
	}
	if (AICL_COMPLETED_IRQ & value) {
		dev_dbg(di->dev , "AICL Complete IRQ\n");
		if (AICL_COMPLETED_STATUS & value) {
			dev_dbg(di->dev , "AICL Complete Status\n");
			writeIntoCBuffer(&di->events ,
				EVENT_AICL_COMPLETE);
			queue_delayed_work(summit_work_queue ,
			&di->summit_monitor_work , msecs_to_jiffies(1));
		}
	}
	if (COMPLETE_CHARGE_TIMEOUT_IRQ & value) {
		dev_dbg(di->dev , "Complete Charge Timeout IRQ\n");
		if (COMPLETE_CHARGE_TIMEOUT_STATUS & value){
			snprintf(buf, THERMO_METRICS_STR_LEN,"%scomplete_charge_timeout=1:", thermal_metric_prefix);
			log_to_metrics(ANDROID_LOG_INFO, "", buf);
		}
	}
	if (PC_TIMEOUT_IRQ & value) {
		dev_dbg(di->dev , "Pre-Charge Timeout IRQ\n");
		if (PC_TIMEOUT_STATUS & value){
			snprintf(buf, THERMO_METRICS_STR_LEN,"%sprecharge_timeout=1:", thermal_metric_prefix);
			log_to_metrics(ANDROID_LOG_INFO, "", buf);
		}
	}
}
void summit_read_interrupt_e(struct summit_smb347_info *di){
	int value = 0;
	value = i2c_smbus_read_byte_data(di->client ,
		SUMMIT_SMB347_INTSTAT_REG_E);
	if (DCIN_OVER_VOLTAGE_IRQ & value) {
		dev_dbg(di->dev , "DCIN Over-voltage IRQ\n");
		if (DCIN_OVER_VOLTAGE_STATUS & value)
			dev_dbg(di->dev , "DCIN Over-voltage\n");
	}
	if (DCIN_UNDER_VOLTAGE_IRQ & value) {
		dev_dbg(di->dev , "DCIN Under-voltage IRQ\n");
		if (DCIN_UNDER_VOLTAGE_STATUS & value)
			dev_dbg(di->dev , "DCIN Under-voltage\n");
	}

	if (USBIN_OVER_VOLTAGE_IRQ & value) {
		dev_dbg(di->dev , "USBIN Over-voltage IRQ\n");
		if (USBIN_OVER_VOLTAGE_STATUS & value)
			dev_dbg(di->dev , "USBIN Over-voltage\n");
	}
	if (USBIN_UNDER_VOLTAGE_IRQ & value) {
		dev_dbg(di->dev , "USBIN Under-voltage IRQ\n");
		if (USBIN_UNDER_VOLTAGE_STATUS & value)
			dev_dbg(di->dev , "USBIN Under-voltage Status\n");
		else
			dev_dbg(di->dev , "USBIN\n");
	}
}
void summit_read_interrupt_f(struct summit_smb347_info *di){
	int value = 0;
	value = i2c_smbus_read_byte_data(di->client ,
		SUMMIT_SMB347_INTSTAT_REG_F);
	if (OTG_OVER_CURRENT_IRQ & value) {
		dev_dbg(di->dev , "OTG Over-current Limit IRQ\n");
		if (OTG_OVER_CURRENT_STATUS & value)
				dev_dbg(di->dev , "OTG Over-current Limit Status\n");
	}
	if (OTG_BAT_UNDER_VOLTAGE_IRQ & value) {
		dev_dbg(di->dev , "OTG Battery Under-voltage IRQ\n");
		if (OTG_BAT_UNDER_VOLTAGE_STATUS & value)
			dev_dbg(di->dev , "OTG Battery Under-voltage Status\n");
	}
	if (OTG_DETECTION_IRQ & value) {
		dev_dbg(di->dev , "OTG Detection IRQ\n");
		if (OTG_DETECTION_STATUS & value)
			dev_dbg(di->dev , "OTG Detection Status\n");
	}
	if (POWER_OK_IRQ  & value) {
		dev_dbg(di->dev , "USBIN Under-voltage IRQ\n");
		if (POWER_OK_STATUS & value)
			dev_dbg(di->dev , "Power OK Status\n");
	}
}

void summit_read_fault_interrupt_setting(struct summit_smb347_info *di){
	int value = 0;
	value = i2c_smbus_read_byte_data(di->client ,
		SUMMIT_SMB347_FAULT_INTERRUPT);
	dev_info(di->dev , "Fault interrupt=0x%x\n" , value);
	if (TEMP_OUTSIDE_COLD_HOT_HARD_LIMIITS_TRIGGER_IRQ & value)
		dev_info(di->dev , "Temperature outsid cold/hot hard limits\n");
	if (TEMP_OUTSIDE_COLD_HOT_SOFT_LIMIITS_TRIGGER_IRQ & value)
		dev_info(di->dev , "Temperature outsid cold/hot soft limits\n");
	if (OTG_BAT_FAIL_ULVO_TRIGGER_IRQ & value)
		dev_info(di->dev , "OTG Battery Fail(ULVO)\n");
	if (OTG_OVER_CURRENT_LIMIT_TRIGGER_IRQ & value)
		dev_info(di->dev , "OTG Over-current Limit\n");
	if (USB_OVER_VOLTAGE_TRIGGER_IRQ & value)
		dev_info(di->dev , "USB Over-Voltage\n");
	if (USB_UNDER_VOLTAGE_TRIGGER_IRQ & value)
		dev_info(di->dev , "USB Under-voltage\n");
	if (AICL_COMPLETE_TRIGGER_IRQ & value)
		dev_info(di->dev , "AICL Complete\n");
	if (INTERNAL_OVER_TEMP_TRIGGER_IRQ & value)
		dev_info(di->dev , "Internal Over-temperature\n");
}
void summit_read_status_interrupt_setting(struct summit_smb347_info *di){
	int value = 0;
	value = i2c_smbus_read_byte_data(di->client ,
		SUMMIT_SMB347_INTERRUPT_STAT);
	dev_info(di->dev , "Status interrupt=0x%x\n" , value);
	if (CHARGE_TIMEOUT_TRIGGER_IRQ & value)
		dev_info(di->dev , "Charge timeout\n");
	if (OTG_INSERTED_REMOVED_TRIGGER_IRQ & value)
		dev_info(di->dev , "OTG Insert/Removed\n");
	if (BAT_OVER_VOLTAGE_TRIGGER_IRQ & value)
		dev_info(di->dev , "Battery Over-voltage\n");
	if (TERMINATION_OR_TAPER_CHARGING_TRIGGER_IRQ & value)
		dev_info(di->dev , "Termination or Taper Charging\n");
	if (FAST_CHARGING_TRIGGER_IRQ & value)
		dev_info(di->dev , "Fast Charging\n");
	if (INOK_TRIGGER_IRQ & value)
		dev_info(di->dev , "USB Under-voltage\n");
	if (MISSING_BATTERY_TRIGGER_IRQ & value)
		dev_info(di->dev , "Missing Battery\n");
	if (LOW_BATTERY_TRIGGER_IRQ & value)
		dev_info(di->dev , "Low Battery\n");
}

int summit_charger_reconfig(struct summit_smb347_info *di){
	int index = 0;
	int headfile , setting , result = 0;
	summit_write_config(di , 1);
	mdelay(1);

	for (index = 0 ; index <= 0x0d ; index++) {
		headfile = i2c_smbus_read_byte_data(di->client , index);
		if (di->mbid == 0) {
			setting = kc1_phydetect_setting[index];
			if (headfile != kc1_phydetect_setting[index]) {
				i2c_smbus_write_byte_data(di->client , index ,
				kc1_phydetect_setting[index]);
				result = i2c_smbus_read_byte_data(di->client , index);
			}
		} else {
			if (di->mbid >= 5) {
				if (di->client->addr == SUMMIT_SMB347_I2C_ADDRESS_SECONDARY) {
					setting = kc1_chargerdetect_setting_pvt[index];
					if (headfile != kc1_chargerdetect_setting_pvt[index]) {
						if (index != 0x05  && index != 0x3 && index != 0x6)
							i2c_smbus_write_byte_data(di->client , index ,
							kc1_chargerdetect_setting_pvt[index]);
					}
				} else {
					setting = otter2_1731[index];
					if (headfile != otter2_1731[index]) {
						if (index != 0x05  && index != 0x3 && index != 0x6)
							i2c_smbus_write_byte_data(di->client , index ,
							otter2_1731[index]);
					}
				}
			} else {
				setting = kc1_chargerdetect_setting[index];
				if (headfile != kc1_chargerdetect_setting[index]) {
					if (index != 0x05  && index != 0x3 && index != 0x6)
						i2c_smbus_write_byte_data(di->client , index ,
						kc1_chargerdetect_setting[index]);
				}
			}
		}
		result = i2c_smbus_read_byte_data(di->client , index);
	}
	mdelay(1);
	summit_write_config(di , 0);
	return 0;
}
int summit_smb347_read_id(struct summit_smb347_info *di)
{
	int config = 0;
	config = i2c_smbus_read_byte_data(di->client ,
		SUMMIT_SMB347_SLAVE_ADDR);
	pr_info("%s : config=0x%x\n",__func__,config);
	if (config < 0) {
		pr_err("i2c error at %s: %d\n", __FUNCTION__, config);
		return config;
	}
	di->id = config >> 1;
	return 0;
}

#if 1
static int summit_suspend(struct i2c_client *client ,  pm_message_t mesg)
{
	return 0;
}

static int summit_resume(struct i2c_client *client)
{
	return 0;
}
#endif

static void disconnect_work_func(struct work_struct *work)
{
	struct summit_smb347_info *di =
		container_of(work ,  struct summit_smb347_info ,
			disconnect_work.work);

	BUG_ON(!di);

	wake_unlock(&di->chrg_lock);
}

extern u8 quanta_get_mbid(void);
extern int bq275xx_register_notifier(struct notifier_block *nb);
extern int bq275xx_unregister_notifier(struct notifier_block *nb);
#ifdef CONFIG_THERMAL_FRAMEWORK
#define THERMAL_PREFIX "Thermal Policy: Charger cooling agent: "
#define THERMAL_INFO(fmt, args...) do { printk(KERN_INFO THERMAL_PREFIX fmt, ## args); } while(0)

#ifdef THERMAL_DEBUG
#define THERMAL_DBG(fmt, args...) do { printk(KERN_DEBUG THERMAL_PREFIX fmt, ## args); } while(0)
#else
#define THERMAL_DBG(fmt, args...) do {} while(0)
#endif
static int smb347_apply_cooling(struct thermal_dev *dev,
				int level)
{
	struct summit_smb347_info *di = container_of(dev ,
	struct summit_smb347_info ,  tdev);

	int value;
	value = thermal_cooling_device_reduction_get(dev, level);
	if(value >=0){
		if(di->thermal_adjust_mode == 0) {
			pr_info("%s : %d max_aicl=%d \n",__func__,
					level,value);
			di->max_aicl = value;
			if(di->max_aicl > 1800)
				di->max_aicl = 1800;
			if(di->max_aicl < 300)
				di->max_aicl = 300;
		}else{
			pr_info("%s : %d current=%d current_redunction=%d\n",__func__,
				level,di->charge_current,di->charge_current_redunction);
			di->charge_current_redunction = value;
		}
		writeIntoCBuffer(&di->events , EVENT_CURRENT_THERMAL_ADJUST);
		queue_delayed_work(summit_work_queue ,
			&di->summit_monitor_work , 0);
	}
	return 0;
}

static struct thermal_dev_ops smb347_cooling_ops = {
	.cool_device = smb347_apply_cooling,
};
#endif

/*
Dvt(mbid=4)
->SUSP pin ->UART4_RX/GPIO155
->EN pin   ->C2C_DATA12/GPIO101
->
*/
static int __devinit summit_probe(struct i2c_client *client ,
	const struct i2c_device_id *id)
{
	struct summit_smb347_info *di;


	u8 value , config;
	int ret;
	int status;
	printk(KERN_INFO "%s\n" , __func__);
	di = kzalloc(sizeof(*di) ,  GFP_KERNEL);
	if (!di)
		return -ENOMEM;
	di->client = client;
	di->dev = &client->dev;
	di->usb_notifier.notifier_call = summit_usb_notifier_call;
	di->bat_notifier.notifier_call = summit_bat_notifier_call;
	di->irq	= client->irq;
	di->mbid = 0;
	di->xceiv = otg_get_transceiver();
	di->bad_battery = 0;
	di->pin_en = 101;
	di->pin_susp = 155;
	di->protect_enable = 0;
	di->fake_disconnect = 0;
	di->pre_max_aicl = 1800;
	di->max_aicl = 1800;
	di->thermal_adjust_mode = 0;
	di->charge_current = 2000;
	di->charge_current_redunction = 0;
	i2c_set_clientdata(client ,  di);
	di->mbid = quanta_get_mbid();

	printk(KERN_INFO "Board id=%d\n" , di->mbid);
	if (di->mbid <= 3) {
		di->protect_enable = 0;
	} else {
		/*Only work after DVT*/
		di->protect_enable		  = 0;
		SET_BAT_NO_SUIT_CURRENT(di->protect_enable);
		gpio_request(di->pin_en ,  "CHARGE-EN");
		gpio_direction_output(di->pin_en ,  0);
		gpio_request(di->pin_susp ,  "CHARGE-SUSP");
		gpio_direction_output(di->pin_susp ,  1);
		mdelay(10);
		/* Check for device ID */
		if (summit_smb347_read_id(di) < 0) {
			pr_err("%s:Unable to detect device ID\n",__func__);
			return -ENODEV;
		}
		ret = twl_i2c_read_u8(TWL6030_MODULE_CHARGER ,  &value ,
			CONTROLLER_STAT1);
		if (value & VBUS_DET) {
			dev_info(di->dev , "USBIN in use\n");
		} else {
			summit_write_config(di , 1);
			config = i2c_smbus_read_byte_data(di->client ,
				SUMMIT_SMB347_STAT_TIMERS);
			CLEAR_BIT(config , 5);
			i2c_smbus_write_byte_data(di->client ,
				SUMMIT_SMB347_STAT_TIMERS , config);
			summit_write_config(di , 0);
		}
		summit_charger_reconfig(di);
		mdelay(10);
		/*gpio_direction_output(di->pin_susp ,  0);*/
		printk(KERN_INFO "Summit SMB347 detected ,chip_id=0x%x board id=%d\n",
			di->id , di->mbid);
	}
#ifdef CONFIG_THERMAL_FRAMEWORK
	//di->tdev = kzalloc(sizeof(struct thermal_dev), GFP_KERNEL);
	di->tdev.name = "charger_cooling";
	di->tdev.domain_name = "case";
	di->tdev.dev_ops = &smb347_cooling_ops,
	di->tdev.dev = di->dev;
	ret = thermal_cooling_dev_register(&di->tdev);
#endif
	INIT_DELAYED_WORK_DEFERRABLE(&di->summit_monitor_work ,
		summit_monitor_work_func);
	INIT_DELAYED_WORK_DEFERRABLE(&di->summit_check_work ,
		summit_check_work_func);
	initCBuffer(&di->events , 30);
	summit_init_fsm(di);
	status = otg_register_notifier(di->xceiv , &di->usb_notifier);
	status = bq275xx_register_notifier(&di->bat_notifier);
	wake_lock_init(&di->chrg_lock ,  WAKE_LOCK_SUSPEND ,
		"usb_wake_lock");
	wake_lock_init(&di->summit_lock , WAKE_LOCK_SUSPEND ,
		"summit_wake_lock");
	create_summit_powersupplyfs(di);
	create_summit_sysfs(di);

	INIT_DELAYED_WORK(&di->disconnect_work ,  disconnect_work_func);

	/*enable low battery interrupt*/
	summit_enable_stat_interrupt(di , LOW_BATTERY_TRIGGER_IRQ);
	/* request charger ctrl interruption */
	ret = request_threaded_irq(di->irq ,  NULL ,  summit_charger_interrupt ,
		IRQF_TRIGGER_FALLING ,  "summit_isr" ,  di);
	if (ret) {
		printk(KERN_ERR "Failed to request IRQ %d: %d\n" ,
			client->irq ,  ret);
	}
	irq_set_irq_wake(di->irq , 1);
#if 1

#endif

	return 0;
}

static int __devexit summit_remove(struct i2c_client *client)
{
	struct summit_smb347_info *di = i2c_get_clientdata(client);
	dev_dbg(di->dev , KERN_INFO "%s\n" , __func__);
	disable_irq(di->irq);
	free_irq(di->irq ,  di);
	usb_unregister_notify(&di->usb_notifier);
	bq275xx_unregister_notifier(&di->bat_notifier);
	cancel_delayed_work(&di->summit_check_work);
	cancel_delayed_work(&di->summit_monitor_work);
	flush_scheduled_work();
	releaseCBuffer(&di->events);
	remove_summit_powersupplyfs(di);
	remove_summit_sysfs(di);

	kfree(di);

	return 0;
}

static void summit_shutdown(struct i2c_client *client)
{
	struct summit_smb347_info *di = i2c_get_clientdata(client);

	dev_info(di->dev ,  "%s\n" ,  __func__);

	disable_irq(di->irq);
	free_irq(di->irq ,  di);
	otg_unregister_notifier(di->xceiv ,  &di->usb_notifier);
	bq275xx_unregister_notifier(&di->bat_notifier);
	cancel_delayed_work_sync(&di->summit_monitor_work);
	cancel_delayed_work_sync(&di->summit_check_work);
	summit_fsm_stateTransform(di ,  EVENT_SHUTDOWN);
}

static const struct i2c_device_id summit_smb347_id[] =  {
	{ "summit_smb347" ,  0 } ,
	{ } ,
};

static struct i2c_driver summit_i2c_driver = {
	.driver = {
		.name = "summit_smb347" ,
	} ,
	.probe = summit_probe ,
	.suspend = summit_suspend ,
	.resume = summit_resume ,
	.remove = summit_remove ,
	.id_table = summit_smb347_id ,
	.shutdown = summit_shutdown ,
};

static int __init summit_init(void)
{
	int ret = 0;
	printk(KERN_INFO "Summit SMB347 Driver\n");
	summit_work_queue = create_singlethread_workqueue("summit");
	if (summit_work_queue == NULL)
		ret = -ENOMEM;
	ret = i2c_add_driver(&summit_i2c_driver);
	if (ret) {
		printk(KERN_ERR "summit_smb347: Could not add driver\n");
		return ret;
	}
	return 0;
}

static void __exit summit_exit(void)
{
	destroy_workqueue(summit_work_queue);
	i2c_del_driver(&summit_i2c_driver);
}

module_init(summit_init);
module_exit(summit_exit);

MODULE_DESCRIPTION("Summit SMB347Driver");
MODULE_AUTHOR("Eric Nien");
MODULE_LICENSE("GPL");
