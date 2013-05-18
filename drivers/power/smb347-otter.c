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
static int pre_status = 0;

extern int get_battery_mAh(void);

static int kc1_chargerdetect_setting[] = {
/*0*/	FCC_2500mA | PCC_150mA | TC_250mA ,
/*1*/	DC_ICL_1800mA | USBHC_ICL_1800mA ,
/*2*/	SUS_CTRL_BY_REGISTER | BAT_TO_SYS_NORMAL | VFLT_PLUS_200mV | AICL_ENABLE | AIC_TH_4200mV | USB_IN_FIRST | BAT_OV_END_CHARGE ,
/*3*/	PRE_CHG_VOLTAGE_THRESHOLD_3_0 | FLOAT_VOLTAGE_4_2_0 ,
/*4*/	AUTOMATIC_RECHARGE_ENABLE | CURRENT_TERMINATION_ENABLE | BMD_VIA_THERM_IO | AUTO_RECHARGE_100mV | APSD_ENABLE |
		NC_APSD_ENABLE | SECONDARY_INPUT_NOT_ACCEPTED_IN_OVLO ,
/*5*/	STAT_ACTIVE_LOW|STAT_CHARGEING_STATE | STAT_ENABLE | NC_INPUT_HC_SETTING | CC_TIMEOUT_764MIN | PC_TIMEOUT_48MIN ,
/*6*/	LED_BLINK_DISABLE | EN_PIN_ACTIVE_LOW | USB_HC_CONTROL_BY_PIN | USB_HC_DUAL_STATE | CHARGER_ERROR_NO_IRQ |
		APSD_DONE_IRQ | DCIN_INPUT_PRE_BIAS_ENABLE ,
/*7*/	0x80 | MIN_SYS_3_4_5_V | THERM_MONITOR_VDDCAP | THERM_MONITOR_ENABLE | SOFT_COLD_CC_FV_COMPENSATION | SOFT_HOT_CC_FV_COMPENSATION ,
/*8*/	INOK_OPERATION | USB_2 | VFLT_MINUS_240mV | HARD_TEMP_CHARGE_SUSPEND | PC_TO_FC_THRESHOLD_ENABLE|INOK_ACTIVE_LOW ,
/*9*/	RID_DISABLE_OTG_I2C_CONTROL | OTG_PIN_ACTIVE_HIGH | LOW_BAT_VOLTAGE_3_4_6_V ,
/*a*/	CCC_700mA | DTRT_130C | OTG_CURRENT_LIMIT_500mA | OTG_BAT_UVLO_THRES_2_7_V ,
/*b*/	0x61 ,
/*c*/	AICL_COMPLETE_TRIGGER_IRQ ,
/*d*/	INOK_TRIGGER_IRQ|LOW_BATTERY_TRIGGER_IRQ ,
};

static int kc1_chargerdetect_setting_pvt[] = {
/*0*/	FCC_2500mA|PCC_150mA|TC_150mA,
/*1*/	DC_ICL_1800mA|USBHC_ICL_1800mA,
/*2*/	SUS_CTRL_BY_REGISTER | BAT_TO_SYS_NORMAL | VFLT_PLUS_200mV | AICL_ENABLE | AIC_TH_4200mV | USB_IN_FIRST | BAT_OV_END_CHARGE,
/*3*/	PRE_CHG_VOLTAGE_THRESHOLD_3_0 | FLOAT_VOLTAGE_4_2_0,
/*4*/	AUTOMATIC_RECHARGE_ENABLE | CURRENT_TERMINATION_ENABLE | BMD_VIA_THERM_IO | AUTO_RECHARGE_100mV | APSD_ENABLE | NC_APSD_ENABLE |
		SECONDARY_INPUT_NOT_ACCEPTED_IN_OVLO,
/*5*/	STAT_ACTIVE_LOW | STAT_CHARGEING_STATE | STAT_ENABLE | NC_INPUT_HC_SETTING | CC_TIMEOUT_764MIN | PC_TIMEOUT_48MIN,
/*6*/	LED_BLINK_DISABLE | EN_PIN_ACTIVE_LOW | USB_HC_CONTROL_BY_PIN | USB_HC_DUAL_STATE | CHARGER_ERROR_NO_IRQ | APSD_DONE_IRQ |
		DCIN_INPUT_PRE_BIAS_ENABLE,
/*7*/	0x80 | MIN_SYS_3_4_5_V | THERM_MONITOR_VDDCAP | THERM_MONITOR_ENABLE | SOFT_COLD_CC_FV_COMPENSATION | SOFT_HOT_FV_COMPENSATION,
/*8*/	INOK_OPERATION | USB_2 | VFLT_MINUS_240mV | HARD_TEMP_CHARGE_SUSPEND | PC_TO_FC_THRESHOLD_ENABLE | INOK_ACTIVE_LOW,
/*9*/	RID_DISABLE_OTG_I2C_CONTROL | OTG_PIN_ACTIVE_HIGH | LOW_BAT_VOLTAGE_3_4_6_V,
/*a*/	CCC_700mA | DTRT_130C | OTG_CURRENT_LIMIT_500mA | OTG_BAT_UVLO_THRES_2_7_V,
/*b*/	0xf5,
/*c*/	AICL_COMPLETE_TRIGGER_IRQ,
/*d*/	INOK_TRIGGER_IRQ | LOW_BATTERY_TRIGGER_IRQ,
};

static int otter2_1731[] = {
/*0*/	FCC_2500mA | PCC_150mA | TC_150mA,
/*1*/	DC_ICL_1800mA | USBHC_ICL_1800mA,
/*2*/	SUS_CTRL_BY_REGISTER | BAT_TO_SYS_NORMAL | VFLT_PLUS_200mV | AICL_ENABLE | AIC_TH_4200mV | USB_IN_FIRST | BAT_OV_END_CHARGE,
/*3*/	PRE_CHG_VOLTAGE_THRESHOLD_3_0 | FLOAT_VOLTAGE_4_2_0,
/*4*/	AUTOMATIC_RECHARGE_ENABLE | CURRENT_TERMINATION_ENABLE | BMD_VIA_THERM_IO | AUTO_RECHARGE_100mV | APSD_ENABLE | NC_APSD_ENABLE |
		SECONDARY_INPUT_NOT_ACCEPTED_IN_OVLO,
/*5*/	STAT_ACTIVE_LOW | STAT_CHARGEING_STATE | STAT_ENABLE | NC_INPUT_HC_SETTING | CC_TIMEOUT_764MIN | PC_TIMEOUT_48MIN,
/*6*/	LED_BLINK_DISABLE | EN_PIN_ACTIVE_LOW | USB_HC_CONTROL_BY_PIN | USB_HC_DUAL_STATE | CHARGER_ERROR_NO_IRQ | APSD_DONE_IRQ | DCIN_INPUT_PRE_BIAS_ENABLE,
/*7*/	0x80 | MIN_SYS_3_4_5_V | THERM_MONITOR_VDDCAP | THERM_MONITOR_ENABLE | SOFT_COLD_CC_FV_COMPENSATION | SOFT_HOT_FV_COMPENSATION,
/*8*/	INOK_OPERATION | USB_2 | VFLT_MINUS_240mV | HARD_TEMP_CHARGE_SUSPEND | PC_TO_FC_THRESHOLD_ENABLE | INOK_ACTIVE_LOW,
/*9*/	RID_DISABLE_OTG_I2C_CONTROL | OTG_PIN_ACTIVE_HIGH | LOW_BAT_VOLTAGE_3_4_6_V,
/*a*/	CCC_700mA | DTRT_130C | OTG_CURRENT_LIMIT_500mA | OTG_BAT_UVLO_THRES_2_7_V,
/*b*/	0xa6,
/*c*/	AICL_COMPLETE_TRIGGER_IRQ,
/*d*/	INOK_TRIGGER_IRQ | LOW_BATTERY_TRIGGER_IRQ,
};

static int kc1_phydetect_setting[] = {
/*0*/	FCC_2500mA | PCC_150mA | TC_150mA,
/*1*/	DC_ICL_1800mA | USBHC_ICL_1800mA,
/*2*/	SUS_CTRL_BY_REGISTER | BAT_TO_SYS_NORMAL | VFLT_PLUS_200mV | AICL_ENABLE | AIC_TH_4200mV | USB_IN_FIRST | BAT_OV_END_CHARGE,
/*3*/	PRE_CHG_VOLTAGE_THRESHOLD_3_0 | FLOAT_VOLTAGE_4_2_0,
/*detect by omap*/
/*4*/	AUTOMATIC_RECHARGE_ENABLE | CURRENT_TERMINATION_ENABLE | BMD_VIA_THERM_IO | AUTO_RECHARGE_100mV | APSD_DISABLE |
		NC_APSD_DISABLE | SECONDARY_INPUT_NOT_ACCEPTED_IN_OVLO,
/*5*/	STAT_ACTIVE_LOW | STAT_CHARGEING_STATE | STAT_DISABLE | NC_INPUT_HC_SETTING | CC_TIMEOUT_DISABLED | PC_TIMEOUT_DISABLED,
/*6*/	LED_BLINK_DISABLE | CHARGE_EN_I2C_0 | USB_HC_CONTROL_BY_REGISTER | USB_HC_TRI_STATE | CHARGER_ERROR_NO_IRQ |
		APSD_DONE_IRQ | DCIN_INPUT_PRE_BIAS_ENABLE,
/*7*/	0X80 | MIN_SYS_3_4_5_V | THERM_MONITOR_VDDCAP | SOFT_COLD_NO_RESPONSE | SOFT_HOT_NO_RESPONSE,
/*8*/	INOK_OPERATION | USB_2 | VFLT_MINUS_60mV | PC_TO_FC_THRESHOLD_ENABLE | HARD_TEMP_CHARGE_SUSPEND | INOK_ACTIVE_LOW ,
/*9*/	RID_DISABLE_OTG_I2C_CONTROL | OTG_PIN_ACTIVE_HIGH | LOW_BAT_VOLTAGE_3_5_8_V,
/*a*/	CCC_700mA | DTRT_130C | OTG_CURRENT_LIMIT_500mA | OTG_BAT_UVLO_THRES_2_7_V,
/*b*/	0x61,
/*c*/	TEMP_OUTSIDE_COLD_HOT_HARD_LIMIITS_TRIGGER_IRQ | TEMP_OUTSIDE_COLD_HOT_SOFT_LIMIITS_TRIGGER_IRQ |
		USB_OVER_VOLTAGE_TRIGGER_IRQ | USB_UNDER_VOLTAGE_TRIGGER_IRQ | AICL_COMPLETE_TRIGGER_IRQ,
/*d*/	CHARGE_TIMEOUT_TRIGGER_IRQ | TERMINATION_OR_TAPER_CHARGING_TRIGGER_IRQ | FAST_CHARGING_TRIGGER_IRQ | INOK_TRIGGER_IRQ |
		MISSING_BATTERY_TRIGGER_IRQ | LOW_BATTERY_TRIGGER_IRQ,
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
	struct summit_smb347_info *di = container_of(nb, struct summit_smb347_info,  bat_notifier);
	writeIntoCBuffer(&di->events , val);
	queue_delayed_work(summit_work_queue, &di->summit_monitor_work, 0);
	return result;
}

int summit_usb_notifier_call(struct notifier_block *nb,  unsigned long val, void *priv) {
	int result = NOTIFY_OK;
	struct summit_smb347_info *di = container_of(nb, struct summit_smb347_info, usb_notifier);
	if (val == USB_EVENT_DETECT_SOURCE)
		wake_lock(&di->chrg_lock);
	writeIntoCBuffer(&di->events , val);
	queue_delayed_work(summit_work_queue, &di->summit_monitor_work, 0);
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
 */
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
				gpio_set_value(di->pdata->pin_en ,  1);
			else
				gpio_set_value(di->pdata->pin_en ,  0);
		break;
	case EN_PIN_ACTIVE_LOW:
			if (enable)
				gpio_set_value(di->pdata->pin_en ,  0);
			else
				gpio_set_value(di->pdata->pin_en ,  1);
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
			gpio_set_value(di->pdata->pin_susp ,  0);
		else
			gpio_set_value(di->pdata->pin_susp ,  1);
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

int pre_current[4] =	{ 100 , 150 ,  200 ,  250 };
int fast_current[8] =	{ 700 , 900 , 1200 , 1500 , 1800 , 2000 , 2200 , 2500 };

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
	}
	if (PC_TIMEOUT_IRQ & value) {
		dev_dbg(di->dev , "Pre-Charge Timeout IRQ\n");
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
		if (di->pdata->mbid == 0) {
			setting = kc1_phydetect_setting[index];
			if (headfile != kc1_phydetect_setting[index]) {
				i2c_smbus_write_byte_data(di->client , index ,
				kc1_phydetect_setting[index]);
				result = i2c_smbus_read_byte_data(di->client , index);
			}
		} else {
			if (di->pdata->mbid >= 5) {
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

/* BEGIN CIRCLE BUFFER */
void initCBuffer(circular_buffer_t *cbuffer , int size)
{
	int i;
	cbuffer->datas = NULL;
	cbuffer->datas = (int *)kmalloc((size*sizeof(int)), GFP_ATOMIC);
	if (NULL == cbuffer->datas)
		printk(KERN_ERR "%s:circular buffer failed allocation\n", __func__);

	for (i = 0; i < size; i++)
		cbuffer->datas[i] = -1;
	cbuffer->buffer_size = size;
	cbuffer->fill_count = 0;
	cbuffer->start_index = 0;
}

void releaseCBuffer(circular_buffer_t *cbuffer)
{   
	kfree(cbuffer->datas);
}

uint32_t readFromCBuffer(circular_buffer_t *cbuffer)
{
	uint32_t data;
	if (cbuffer->fill_count == 0)
		return 0xffff;
	data = cbuffer->datas[cbuffer->start_index];
	cbuffer->datas[cbuffer->start_index] = -1;
	if (cbuffer->start_index == (cbuffer->buffer_size-1))
		cbuffer->start_index = 0;
	else
		cbuffer->start_index++;
	cbuffer->fill_count--;
	return data;
}

void writeIntoCBuffer (circular_buffer_t *cbuffer , uint32_t data)
{
	int temp = 0;
	if ((cbuffer->start_index+cbuffer->fill_count) > (cbuffer->buffer_size-1)) {
		temp = cbuffer->start_index + cbuffer->fill_count-cbuffer->buffer_size;
		cbuffer->datas[temp] = data;
	} else
		cbuffer->datas[cbuffer->start_index+cbuffer->fill_count] = data;
	cbuffer->datas[cbuffer->start_index+cbuffer->fill_count] = data;
	if (cbuffer->fill_count == cbuffer->buffer_size)
		cbuffer->start_index++;
	else
		cbuffer->fill_count++;
}

int isCBufferNotEmpty(circular_buffer_t *cbuffer)
{
	if (cbuffer->fill_count == 0)
		return 0;
	else
		return 1;
}
/* END CIRCLE BUFFER */

/* SYSFS ENTRIES */
static ssize_t summit_version_show(struct device *dev ,
	struct device_attribute *attr , char *buf)
{
	ssize_t result = 0;
	result = sprintf(buf ,  "%s\n" ,  "2011/7/9--1.1");
	return result;
}

static ssize_t summit_apsd_setting_show(struct device *dev ,
	struct device_attribute *attr , char *buf)
{
	struct summit_smb347_info *di = i2c_get_clientdata(to_i2c_client(dev));
	ssize_t result = 0;
	int config = 0;
	config = i2c_smbus_read_byte_data(di->client ,
	SUMMIT_SMB347_CHARGE_CONTROL);
	if (IS_APSD_ENABLE(config))
		result = sprintf(buf ,  "%s\n" ,  "APSD_ENABLE");
	else
		result = sprintf(buf ,  "%s\n" ,  "APSD_DISABLE");
	return result;
}

static ssize_t summit_apsd_setting_store(struct device *dev ,
	struct device_attribute *attr , const char *buf , size_t len)
{
	struct summit_smb347_info *di = i2c_get_clientdata(to_i2c_client(dev));
	int enable , r = 0;
	r = kstrtoint(buf ,  10 , &enable);
	if (r)
		return len;
	summit_config_apsd(di , enable);
	return len;
}

static ssize_t summit_apsd_status_show(struct device *dev ,
	struct device_attribute *attr , char *buf)
{
	struct summit_smb347_info *di = i2c_get_clientdata(to_i2c_client(dev));
	ssize_t result = 0;
	int status = 0;
	int apsd = 0;
	status = i2c_smbus_read_byte_data(di->client ,
			SUMMIT_SMB347_INTSTAT_REG_D);
	apsd = status;
	if (APSD_STATUS(status)) {
		switch (APSD_RESULTS(apsd)) {
		case APSD_NOT_RUN:
			result = sprintf(buf ,  "%s" ,
			"APSD Not run");
		break;
		case APSD_CHARGING_DOWNSTREAM_PORT:
			result = sprintf(buf ,  "%s" ,
			"Charging Downstream Port\n");
		break;
		case APSD_DEDICATED_DOWNSTREAM_PORT:
			result = sprintf(buf ,  "%s" ,
			"Dedicated Charging Port\n");
		break;
		case APSD_OTHER_DOWNSTREAM_PORT:
			result = sprintf(buf ,  "%s" ,
			"Other Charging Port\n");
		break;
		case APSD_STANDARD_DOWNSTREAM_PORT:
			result = sprintf(buf ,  "%s" ,
			"Standard Downstream Port\n");
		break;
		case APSD_ACA_CHARGER:
			result = sprintf(buf ,  "%s" ,
			"ACA Charger\n");
		break;
		case APSD_TBD:
			result = sprintf(buf ,  "%s" ,
			"TBD\n");
		break;
		default:
			result = sprintf(buf ,  "%s" , "");
		break;
		}
	} else
		result = sprintf(buf ,  "%s\n" ,
		"APSD NOt Completed\n");
	return result;
}

static ssize_t summit_aicl_setting_show(struct device *dev ,
	struct device_attribute *attr , char *buf)
{
	struct summit_smb347_info *di = i2c_get_clientdata(to_i2c_client(dev));
	ssize_t result = 0;
	int config = 0;
	config = i2c_smbus_read_byte_data(di->client , SUMMIT_SMB347_FUNCTIONS);
	if (IS_AICL_ENABLE(config))
		result = sprintf(buf ,  "%s\n" ,  "AICL ENABLE");
	else
		result = sprintf(buf ,  "%s\n" ,  "AICL DISABLE");
	return result;
}

static ssize_t summit_aicl_setting_store(struct device *dev ,
	struct device_attribute *attr , const char *buf , size_t len)
{
	struct summit_smb347_info *di = i2c_get_clientdata(to_i2c_client(dev));

	int input = 0;
	int r;
	r = kstrtoint(buf ,  10 , &input);
	if (r)
		return len;
	di->max_aicl = input;
	//config = i2c_smbus_read_byte_data(di->client , 2);
	if(di->max_aicl > 1800)
		di->max_aicl = 1800;
	if(di->max_aicl < 300)
		di->max_aicl = 300;
	writeIntoCBuffer(&di->events , EVENT_CURRENT_THERMAL_ADJUST);
	queue_delayed_work(summit_work_queue ,
		&di->summit_monitor_work , 0);
	return len;
}

static ssize_t summit_aicl_status_show(struct device *dev ,
	struct device_attribute *attr , char *buf)
{
	struct summit_smb347_info *di = i2c_get_clientdata(to_i2c_client(dev));
	ssize_t result = 0;
	int status = 0;
	int aicl = 0;
	status = i2c_smbus_read_byte_data(di->client ,
			SUMMIT_SMB347_STATUS_REG_E);
	aicl = status;
	if (AICL_STATUS(status)) {
		result = sprintf(buf ,  "%d" ,
		aicl_results[AICL_RESULT(aicl)]);
		pre_status=status;
	} else if (USB15_HC_MODE(status) == USB1_OR_15_MODE)
		{
		result = sprintf(buf ,  "%s\n" ,  "100\n");	
		pre_status=0;
	 } else if (USB15_HC_MODE(status) == USB5_OR_9_MODE)
		{
		result = sprintf(buf ,  "%s\n" ,  "500\n");		
		pre_status=1;
	 } else {
		if(pre_status!=0){
 		result = sprintf(buf ,  "%d" ,
		aicl_results[AICL_RESULT(pre_status)]);
		}
		}
	return result;
}

static ssize_t summit_mode_show(struct device *dev ,
	struct device_attribute *attr , char *buf)
{
	struct summit_smb347_info *di = i2c_get_clientdata(to_i2c_client(dev));
	ssize_t result = 0;
	int command = 0;
	command = i2c_smbus_read_byte_data(di->client ,
			SUMMIT_SMB347_COMMAND_REG_B);
	if (IS_BIT_SET(command , 0))
		result = sprintf(buf ,  "%s\n" ,  "HC_MODE\n");
	else{
		if (IS_BIT_SET(command , 1))
			result = sprintf(buf ,  "%s\n" ,  "USB5_MODE\n");
		else
			result = sprintf(buf ,  "%s\n" ,  "HC_MODE\n");
	}
	return result;
}

static ssize_t summit_mode_store(struct device *dev ,
	struct device_attribute *attr , const char *buf , size_t len)
{
	struct summit_smb347_info *di = i2c_get_clientdata(to_i2c_client(dev));
	int mode = 0;
	int r;
	r = kstrtoint(buf ,  10 , &mode);
	if (r)
		return len;
	summit_switch_mode(di , mode);
	return len;
}

static ssize_t summit_mode_status_show(struct device *dev ,
	struct device_attribute *attr , char *buf)
{
	struct summit_smb347_info *di = i2c_get_clientdata(to_i2c_client(dev));
	ssize_t result = 0;
	int status = 0;
	status = i2c_smbus_read_byte_data(di->client ,
			SUMMIT_SMB347_STATUS_REG_E);
	if (USB15_HC_MODE(status) == USB1_OR_15_MODE)
		result = sprintf(buf ,  "%s\n" ,  "USB1_MODE\n");
	if (USB15_HC_MODE(status) == USB5_OR_9_MODE)
		result = sprintf(buf ,  "%s\n" ,  "USB5_MODE\n");
	if (USB15_HC_MODE(status) == HC_MODE)
		result = sprintf(buf ,  "%s\n" ,  "HC_MODE\n");
	return result;
}

static ssize_t summit_charge_show(struct device *dev ,
	struct device_attribute *attr , char *buf)
{
	struct summit_smb347_info *di = i2c_get_clientdata(to_i2c_client(dev));
	int value = 0;

	value = i2c_smbus_read_byte_data(di->client ,
			SUMMIT_SMB347_STATUS_REG_C);

	if (value < 0) {
		return sprintf(buf ,  "-1\n");
	} else {
		return sprintf(buf ,  "%d\n" ,  IS_BIT_SET(value ,  0));
	}
}

static ssize_t summit_charge_store(struct device *dev ,
	struct device_attribute *attr , const char *buf , size_t len)
{
	int value , r = 0;
	struct summit_smb347_info *di = i2c_get_clientdata(to_i2c_client(dev));

	/* Don't perform anything for anything below DVT */
	if (di->pdata->mbid < 4)
		return len;
	r = kstrtoint(buf ,  10 , &value);
	if (r)
		return len;
	if (di->current_state == STATE_SUSPEND)
		return len;
	if (value) {
		if (di->pdata->mbid >= 5) {
			/* Unit is PVT ,  disable charging if battery is bad */
			if (di->bad_battery)
				summit_charge_enable(di ,  0);
			else
				summit_charge_enable(di ,  1);
		} else
			summit_charge_enable(di ,  1);
	} else
		summit_charge_enable(di ,  0);

	return len;
}

static ssize_t summit_charge_status_show(struct device *dev ,
	struct device_attribute *attr , char *buf)
{
	struct summit_smb347_info *di = i2c_get_clientdata(to_i2c_client(dev));
	ssize_t result = 0;
	int status = 0;
	status = i2c_smbus_read_byte_data(di->client ,
			SUMMIT_SMB347_STATUS_REG_C);
	switch(CHARGEING_STATUS(status)){
		case NO_CHARGING_STATUS:
			result = sprintf(buf ,  "%s" ,"no-charging");
		break;
		case PRE_CHARGING_STATUS:
			result = sprintf(buf ,  "%s" ,"pre-charging");
		break;
		case FAST_CHARGING_STATUS:
			result = sprintf(buf ,  "%s" ,"fast-charging");
		break;
		case TAPER_CHARGING_STATUS:
			result = sprintf(buf ,  "%s" ,"taper-charging");
		break;
	}
	return result;
}

static ssize_t summit_discharge_show(struct device *dev ,
	struct device_attribute *attr , char *buf)
{
	struct summit_smb347_info *di = i2c_get_clientdata(to_i2c_client(dev));
	return sprintf(buf ,  "%d\n" ,  di->flag_discharge);
}

static ssize_t summit_discharge_store(struct device *dev ,
	struct device_attribute *attr , const char *buf , size_t len)
{
	struct summit_smb347_info *di = i2c_get_clientdata(to_i2c_client(dev));
	int value = 0;
	int r;
	r = kstrtoint(buf ,  10 , &value);
	if (r)
		return len;
	di->flag_discharge = (u8) value;
	if (di->flag_discharge == 1)
		summit_suspend_mode(di , 1);
	else
		summit_suspend_mode(di , 0);
	return len;
}

static ssize_t summit_status_show(struct device *dev ,
	struct device_attribute *attr , char *buf)
{
	struct summit_smb347_info *di = i2c_get_clientdata(to_i2c_client(dev));
	int index = 0;
	int setting = 0;
	for (index = 0 ; index <= 0x0d ; index++) {
		setting = i2c_smbus_read_byte_data(di->client , index);
		dev_dbg(di->dev , "%s: R%d : setting = 0x%x " ,
		__func__ , index , setting);
		mdelay(1);
	}
	summit_read_fault_interrupt_setting(di);
	summit_read_status_interrupt_setting(di);
	summit_read_interrupt_a(di);
	summit_read_interrupt_b(di);
	summit_read_interrupt_c(di);
	summit_read_interrupt_d(di);
	summit_read_interrupt_e(di);
	return sprintf(buf ,  "%d\n" ,  di->flag_discharge);
}

static ssize_t summit_present_show(struct device *dev ,
	struct device_attribute *attr , char *buf)
{
	struct summit_smb347_info *di = i2c_get_clientdata(to_i2c_client(dev));
	int config = 0;
	int result = 0;
	config = i2c_smbus_read_byte_data(di->client , 0xe);
	if ((config >> 1) == 0x06)
		result = 1;
	else
		result = 0;
	return sprintf(buf ,  "%d\n" ,  result);
}
static ssize_t summit_fake_disconnect_store(struct device *dev ,
	struct device_attribute *attr , const char *buf , size_t len)
{
	struct summit_smb347_info *di = i2c_get_clientdata(to_i2c_client(dev));
	int value , r = 0;
	r = kstrtoint(buf ,  10 , &value);
	if (r)
		return len;
	if (value == 1) {
		di->fake_disconnect = 1;
		atomic_notifier_call_chain(&di->xceiv->notifier ,
		USB_EVENT_NONE ,  NULL);
	}
	return len;
}
static ssize_t summit_protect_enable_show(struct device *dev ,
	struct device_attribute *attr , char *buf)
{
	struct summit_smb347_info *di = i2c_get_clientdata(to_i2c_client(dev));
	ssize_t result = 0;
	result = sprintf(buf ,  "\
	Over charge protection:%s(%d)\n\
	Gas gauge i2c error protection:%s(%d)\n\
	Battery recognize protection:%s(%d)\n\
	NTC error protection:%s(%d)\n\
	Battert hard cold charge protection:%s(%d)\n\
	Battert hard cold discharge protection:%s(%d)\n\
	Battert hard hot charge protection:%s(%d)\n\
	Battert charge current adjust protection:%s(%d)\n\
	Battert no suit charge current protection:%s(%d)\n\
	Battert weak protection:%s(%d)\n" ,
	IS_BIT_SET(di->protect_enable , 0) ? "enable" : "disable" ,
	IS_BIT_SET(di->protect_event , 0) ,
	IS_BIT_SET(di->protect_enable , 1) ? "enable" : "disable" ,
	IS_BIT_SET(di->protect_event , 1) ,
	IS_BIT_SET(di->protect_enable , 2) ? "enable" : "disable" ,
	IS_BIT_SET(di->protect_event , 2) ,
	IS_BIT_SET(di->protect_enable , 3) ? "enable" : "disable" ,
	IS_BIT_SET(di->protect_event , 3) ,
	IS_BIT_SET(di->protect_enable , 4) ? "enable" : "disable" ,
	IS_BIT_SET(di->protect_event , 4) ,
	IS_BIT_SET(di->protect_enable , 5) ? "enable" : "disable" ,
	IS_BIT_SET(di->protect_event , 5) ,
	IS_BIT_SET(di->protect_enable , 6) ? "enable" : "disable" ,
	IS_BIT_SET(di->protect_event , 6) ,
	IS_BIT_SET(di->protect_enable , 7) ? "enable" : "disable" ,
	IS_BIT_SET(di->protect_event , 7) ,
	IS_BIT_SET(di->protect_enable , 8) ? "enable" : "disable" ,
	IS_BIT_SET(di->protect_event , 8) ,
	IS_BIT_SET(di->protect_enable , 9) ? "enable" : "disable" ,
	IS_BIT_SET(di->protect_event , 9));
	return result;
}
/*
Bit0 : Over charge protection
Bit1 : Gas gauge i2c error protection
Bit2 : Battery recognize protection
Bit3 : NTC error protection
Bit4 : Battert hard cold charge protection
Bit5 : Battert hard cold discharge protection
Bit6 : Battert hard hot charge protection
Bit7 : Battert charge current adjust protection
Bit8 : Battert no suit charge current protection
Bit9 : Battert too weak protection
*/
static ssize_t summit_protect_enable_store(struct device *dev ,
	struct device_attribute *attr , const char *buf , size_t len)
{
	struct summit_smb347_info *di = i2c_get_clientdata(to_i2c_client(dev));
	int value;
	int r;
	r = kstrtoint(buf ,  10 , &value);
	if (r)
		return len;
	if (value >= 1 && value <= 10) {
		value = value-1;
		SET_BIT(di->protect_enable , value);
	}
	summit_fsm_stateTransform(di , EVENT_RECHECK_PROTECTION);
	return len;
}
static ssize_t summit_protect_disable_store(struct device *dev ,
	struct device_attribute *attr , const char *buf , size_t len)
{
	struct summit_smb347_info *di = i2c_get_clientdata(to_i2c_client(dev));
	int value;
	int r;
	r = kstrtoint(buf ,  10 , &value);
	if (r)
		return len;

	if (value >= 1 && value <= 10) {
		value = value-1;
		CLEAR_BIT(di->protect_enable , value);
	}
	if (di->protect_enable == 0) {
		summit_config_charge_voltage(di , CONFIG_NO_CHANGE ,
		FLOAT_VOLTAGE_4_2_0);
		summit_config_charge_current(di , FCC_2500mA ,
		PCC_150mA , TC_250mA);
		summit_charge_enable(di , 1);
	}
	summit_fsm_stateTransform(di , EVENT_RECHECK_PROTECTION);
	return len;
}
static ssize_t summit_float_voltage_show(struct device *dev ,
	struct device_attribute *attr , char *buf)
{
	int config = 0;
	struct summit_smb347_info *di = i2c_get_clientdata(to_i2c_client(dev));

	config = i2c_smbus_read_byte_data(di->client ,
	SUMMIT_SMB347_FLOAT_VOLTAGE);

	if (config < 0)
		return sprintf(buf ,  "-1\n");
	else {
		config &= 0x3f;

		if (config >= 50)
			config = 50;

		return sprintf(buf , "%d\n" , 3500 + config * 20);
	}
}

static ssize_t summit_float_voltage_store(struct device *dev ,
	struct device_attribute *attr , const char *buf , size_t len)
{
	struct summit_smb347_info *di = i2c_get_clientdata(to_i2c_client(dev));
	int value;
	int r;
	r = kstrtoint(buf ,  10 , &value);
	if (r)
		return len;

	if (value < 3500 || value > 4500)
		return len;

	/* Compute the right value to modify in register */
	value = (value - 3500) / 20;

	summit_config_charge_voltage(di ,  CONFIG_NO_CHANGE ,  value);

	return len;
}

static ssize_t summit_fast_current_show(struct device *dev ,
	struct device_attribute *attr , char *buf)
{
	int config = 0;
	struct summit_smb347_info *di = i2c_get_clientdata(to_i2c_client(dev));
	config = i2c_smbus_read_byte_data(di->client ,
	SUMMIT_SMB347_CHARGE_CURRENT);
	config = (config >> 5) & 7;
	return sprintf(buf ,  "fast_current=%d mA\n" ,  fast_current[config]);
}

static ssize_t summit_fast_current_store(struct device *dev ,
	struct device_attribute *attr , const char *buf , size_t len)
{
	struct summit_smb347_info *di = i2c_get_clientdata(to_i2c_client(dev));
	int value;
	int r;
	r = kstrtoint(buf ,  10 , &value);
	if (r)
		return len;
	if (value >= 0 && value <= 7)
		summit_config_charge_current(di, value << 5, PCC_150mA, TC_250mA);
	return len;
}
const int fast_charge_current_list[] =	{ 700,  900, 1200, 1500, 1800, 2000, 2200, 2500 };
const int precharge_current_list[] =	{ 100,  150,  200,  250 };

static ssize_t summit_charge_current_show(struct device *dev ,
	struct device_attribute *attr , char *buf)
{
	int val = 0;
	struct summit_smb347_info *di = i2c_get_clientdata(to_i2c_client(dev));
	val = i2c_smbus_read_byte_data(di->client ,
		SUMMIT_SMB347_STATUS_REG_B);

	if (val < 0)
		return sprintf(buf ,  "-1\n");

	if ((val & (1 << 5))) {
		/* Fast charge current */
		return sprintf(buf ,  "%d\n" ,
				fast_charge_current_list[val & 0x7]);
	} else {
		/* Pre-charge current */
		return sprintf(buf ,  "%d\n" ,
				precharge_current_list[(val >> 3) & 0x3]);
	}
}

static ssize_t summit_charge_current_store(struct device *dev ,
	struct device_attribute *attr ,  const char *buf ,  size_t len)
{

	struct summit_smb347_info *di = i2c_get_clientdata(to_i2c_client(dev));
	int value;
	int r;
	r = kstrtoint(buf ,  10 , &value);
		if (r)
		return len;

	/* Don't perform anything for anything below DVT */
	if (di->pdata->mbid < 4)
		return len;

	if (value < 0 || value > 2000) {
		dev_err(dev ,  "Invalid charge current setting\n");
		return len;
	}
	di->charge_current = value;
	writeIntoCBuffer(&di->events , EVENT_CURRENT_THERMAL_ADJUST);
	queue_delayed_work(summit_work_queue ,
		&di->summit_monitor_work , 0);
#if 0
	/* Locate the first smaller current in fast charge current first */
	for (i = ARRAY_SIZE(fast_charge_current_list) - 1; i >= 0; i--)
		if (fast_charge_current_list[i] <= value)
			/* Found ,  stop */
			break;

	if (i >= 0) {
		/* Disable force precharge ,  set fast charge current */
		dev_info(dev ,  "Found fast charge current: %d\n" ,  i);
		summit_force_precharge(di ,  0);
		summit_config_charge_current(di ,  i << 5 ,
			CONFIG_NO_CHANGE ,  CONFIG_NO_CHANGE);
		return len;
	}

	/* Locate the first smaller current in pre-charge current */
	for (i = ARRAY_SIZE(precharge_current_list) - 1; i >= 0; i--)
		if (precharge_current_list[i] <= value)
			break;

	if (i >= 0) {
		/* Force pre-charge ,  set pre-charge current */
		dev_info(dev ,  "Found pre-charge current: %d\n" ,  i);
		summit_force_precharge(di ,  1);
		summit_config_charge_current(di ,  CONFIG_NO_CHANGE ,
			i << 3 ,  CONFIG_NO_CHANGE);
		return len;
	}

	/* Something wrong here */
	dev_warn(dev ,  "Unable to find a charge current setting for %d mA\n" ,
		value);
#endif
	return len;
}
static ssize_t summit_bad_battery_show(struct device *dev ,
	struct device_attribute *attr , char *buf)
{
	struct summit_smb347_info *di = i2c_get_clientdata(to_i2c_client(dev));
	return sprintf(buf ,  "%d\n" , di->bad_battery);
}
/*Android code can read the battery ID and write ‘1’ to bad_battery if the battery is invalid*/
static ssize_t summit_bad_battery_store(struct device *dev ,
	struct device_attribute *attr ,  const char *buf ,  size_t len)
{
	struct summit_smb347_info *di = i2c_get_clientdata(to_i2c_client(dev));
	int value;
	int r;
	r = kstrtoint(buf ,  10 , &value);
	if (r)
		return len;
	if (value == 1)
		di->bad_battery=1;
	else  if (value == 0)
		di->bad_battery=0;
	/* Don't perform anything for anything below PVT */
	if (di->pdata->mbid < 5)
		return len;
	else{
		/*If userspace writes ‘1’ to bad_battery ,
			and unit is PVT and beyond ,  disable charging.*/
		if(di->bad_battery==1)
			summit_charge_enable(di , 0);
	}
	return len;
}

static ssize_t summit_precharge_timeout_show(struct device *dev ,
	struct device_attribute *attr , char *buf)
{
	struct summit_smb347_info *di = i2c_get_clientdata(to_i2c_client(dev));
	int result = 0;

	int value = 0;
	value = i2c_smbus_read_byte_data(di->client ,
		SUMMIT_SMB347_INTSTAT_REG_D);
	if(PC_TIMEOUT_STATUS & value)
		result = 1;
	else
		result = 0;
	return sprintf(buf ,  "%d" , result);
}
static ssize_t summit_completecharge_timeout_show(struct device *dev ,
	struct device_attribute *attr , char *buf)
{
	struct summit_smb347_info *di = i2c_get_clientdata(to_i2c_client(dev));
	int result = 0;
	int value = 0;
	value = i2c_smbus_read_byte_data(di->client ,
		SUMMIT_SMB347_INTSTAT_REG_D);
	if(COMPLETE_CHARGE_TIMEOUT_STATUS & value)
		result = 1;
	else
		result = 0;
	return sprintf(buf ,  "%d" , result);
}
static ssize_t summit_thermal_mode_show(struct device *dev ,
	struct device_attribute *attr , char *buf)
{
	ssize_t result = 0;
	struct summit_smb347_info *di = i2c_get_clientdata(to_i2c_client(dev));
	if (di->thermal_adjust_mode == 0)
		result = sprintf(buf ,  "%s" ,"adjust input current");
	else
		result = sprintf(buf ,  "%s" ,"adjust charge current");
	return result;
}
static ssize_t summit_thermal_mode_store(struct device *dev ,
	struct device_attribute *attr ,  const char *buf ,  size_t len)
{
	struct summit_smb347_info *di = i2c_get_clientdata(to_i2c_client(dev));
	int value;
	int r;
	r = kstrtoint(buf ,  10 , &value);
	if (r)
		return len;
	di->thermal_adjust_mode = value;
	return len;
}
static DEVICE_ATTR(version ,
	S_IRUGO | S_IWUSR | S_IWGRP ,
	summit_version_show ,  NULL);
static DEVICE_ATTR(apsd_setting ,
	S_IRUGO | S_IWUSR | S_IWGRP ,
	summit_apsd_setting_show ,
	summit_apsd_setting_store);
static DEVICE_ATTR(apsd_status ,
	S_IRUGO | S_IWUSR | S_IWGRP  ,
	summit_apsd_status_show ,  NULL);
static DEVICE_ATTR(aicl_setting ,
	S_IRUGO | S_IWUSR | S_IWGRP ,
	summit_aicl_setting_show ,
	summit_aicl_setting_store);
static DEVICE_ATTR(aicl_status ,
	S_IRUGO | S_IWUSR | S_IWGRP  ,
	summit_aicl_status_show ,  NULL);
static DEVICE_ATTR(mode_command ,
	S_IRUGO | S_IWUSR | S_IWGRP ,
	summit_mode_show ,
	summit_mode_store);
static DEVICE_ATTR(mode_status ,
	S_IRUGO | S_IWUSR | S_IWGRP ,
	summit_mode_status_show ,  NULL);
static DEVICE_ATTR(charge_command ,
	S_IRUGO | S_IWUSR ,
	summit_charge_show ,
	summit_charge_store);
static DEVICE_ATTR(charge_status ,
	S_IRUGO | S_IWUSR | S_IWGRP  ,
	summit_charge_status_show ,  NULL);
static DEVICE_ATTR(discharge ,
	S_IRUGO | S_IWUSR | S_IWGRP  ,
	summit_discharge_show ,
	summit_discharge_store);
static DEVICE_ATTR(status ,
	S_IRUGO | S_IWUSR | S_IWGRP  ,
	summit_status_show ,  NULL);
static DEVICE_ATTR(present ,
	S_IRUGO | S_IWUSR | S_IWGRP ,
	summit_present_show ,  NULL);
static DEVICE_ATTR(fake_disconnect ,
	S_IRUGO | S_IWUSR | S_IWGRP ,
	NULL ,
	summit_fake_disconnect_store);
static DEVICE_ATTR(protect_enable ,
	S_IRUGO | S_IWUSR | S_IWGRP ,
	summit_protect_enable_show ,
	summit_protect_enable_store);
static DEVICE_ATTR(protect_disable ,
	S_IRUGO | S_IWUSR | S_IWGRP ,
	summit_protect_enable_show ,
	summit_protect_disable_store);
static DEVICE_ATTR(float_voltage ,
	S_IRUGO | S_IWUSR | S_IWGRP ,
	summit_float_voltage_show ,
	summit_float_voltage_store);
static DEVICE_ATTR(fast_current ,
	S_IRUGO | S_IWUSR | S_IWGRP ,
	summit_fast_current_show ,
	summit_fast_current_store);
static DEVICE_ATTR(charge_current ,
	S_IRUGO | S_IWUSR | S_IWGRP ,
	summit_charge_current_show ,
	summit_charge_current_store);
static DEVICE_ATTR(bad_battery ,
	S_IRUGO | S_IWUSR | S_IWGRP ,
	summit_bad_battery_show ,
	summit_bad_battery_store);
static DEVICE_ATTR(precharge_timeout ,
	S_IRUGO | S_IWUSR | S_IWGRP ,
	summit_precharge_timeout_show ,
	NULL);
static DEVICE_ATTR(completecharge_timeout ,
	S_IRUGO | S_IWUSR | S_IWGRP ,
	summit_completecharge_timeout_show ,
	NULL);
static DEVICE_ATTR(thermal_mode ,
	S_IRUGO | S_IWUSR | S_IWGRP ,
	summit_thermal_mode_show ,
	summit_thermal_mode_store);
static struct attribute *summit_attrs[] = {
	&dev_attr_version.attr ,
	&dev_attr_charge_command.attr ,
	&dev_attr_charge_status.attr ,
	&dev_attr_mode_command.attr ,
	&dev_attr_mode_status.attr ,
	&dev_attr_apsd_setting.attr ,
	&dev_attr_apsd_status.attr ,
	&dev_attr_aicl_setting.attr ,
	&dev_attr_aicl_status.attr ,
	&dev_attr_discharge.attr ,
	&dev_attr_present.attr ,
	&dev_attr_status.attr ,
	&dev_attr_fake_disconnect.attr ,
	&dev_attr_protect_enable.attr ,
	&dev_attr_protect_disable.attr ,
	&dev_attr_float_voltage.attr ,
	&dev_attr_fast_current.attr ,
	&dev_attr_charge_current.attr ,
	&dev_attr_bad_battery.attr ,
	&dev_attr_precharge_timeout.attr ,
	&dev_attr_completecharge_timeout.attr ,
	&dev_attr_thermal_mode.attr ,
	NULL
};

static struct attribute_group summit_attr_grp = {
	.attrs = summit_attrs ,
};
/* END SYSFS ENTRIES */


/* BEGIN FSM */
void summit_init_fsm(struct summit_smb347_info *di)
{
	di->current_state = STATE_SUSPEND;
}

const char *fsm_state_string(int state)
{
	switch (state) {
	case STATE_SUSPEND:	return "suspend";
	case STATE_ONDEMAND:	return "ondemand";
	case STATE_INIT:	return "init";
	case STATE_PC:		return "pc";
	case STATE_CD:		return "STATE_CD";
	case STATE_DC:		return "Dedicated charger";
	case STATE_UC:		return "Unknow charger";
	case STATE_OTHER:	return "Other charger";
	case STATE_CHARGING:	return "charging";
	case STATE_SHUTDOWN:	return "shutdown";
	default:		return "UNDEFINED";
	}
}
const char *fsm_event_string(int event)
{
	switch (event) {
	/*from USB*/
	case USB_EVENT_NONE:
		return "USB_EVENT_NONE";
	case USB_EVENT_VBUS:
		return "USB_EVENT_VBUS";
	case USB_EVENT_ENUMERATED:
		return "USB_EVENT_ENUMERATED";
	case USB_EVENT_DETECT_SOURCE:
		return "USB_EVENT_DETECT_SOURCE";
	case USB_EVENT_ID:
		return "USB_EVENT_ID";
	case EVENT_CHECKINIT:
		return "EVENT_CHECKINIT";
	case EVENT_INTERRUPT_FROM_CHARGER:
		return "EVENT_INTERRUPT_FROM_CHARGER";
	case EVENT_CHANGE_TO_ONDEMAND:
		return "EVENT_CHANGE_TO_ONDEMAND";
	case EVENT_CHANGE_TO_INTERNAL_FSM:
		return "EVENT_CHANGE_TO_INTERNAL_FSM";
	case EVENT_DETECT_PC:
		return "EVENT_DETECT_PC";
	case USB_EVENT_HOST_CHARGER:
		return "USB_EVENT_HOST_CHARGER";
	case USB_EVENT_CHARGER:
		return "USB_EVENT_CHARGER";
	case EVENT_DETECT_TBD:
		return "EVENT_DETECT_TBD";
	case EVENT_DETECT_OTHER:
		return "EVENT_DETECT_OTHER";
	case EVENT_BELOW_HOT_TEMP_HARD_LIMIT:
		return "EVENT_BELOW_HOT_TEMP_HARD_LIMIT";
	case EVENT_OVER_HOT_TEMP_SOFT_LIMIT:
		return "EVENT_OVER_HOT_TEMP_SOFT_LIMIT";
	case EVENT_BELOW_HOT_TEMP_SOFT_LIMIT:
		return "EVENT_BELOW_HOT_TEMP_SOFT_LIMIT";
	case EVENT_OVER_COLD_TEMP_HARD_LIMIT:
		return "EVENT_OVER_COLD_TEMP_HARD_LIMIT";
	case EVENT_BELOW_COLD_TEMP_HARD_LIMIT:
		return "EVENT_BELOW_COLD_TEMP_HARD_LIMIT";
	case EVENT_OVER_COLD_TEMP_SOFT_LIMIT:
		return "EVENT_OVER_COLD_TEMP_SOFT_LIMIT";
	case EVENT_BELOW_COLD_TEMP_SOFT_LIMIT:
		return "EVENT_BELOW_COLD_TEMP_SOFT_LIMIT";
	case EVENT_APSD_NOT_RUNNING:
		return "EVENT_APSD_NOT_RUNNING";
	case EVENT_APSD_COMPLETE:
		return "EVENT_APSD_COMPLETE";
	case EVENT_APSD_NOT_COMPLETE:
		return "EVENT_APSD_NOT_COMPLETE";
	case EVENT_AICL_COMPLETE:
		return "EVENT_AICL_COMPLETE";
	case EVENT_APSD_AGAIN:
		return "EVENT_APSD_AGAIN";
	case EVENT_OVER_LOW_BATTERY:
		return "EVENT_OVER_LOW_BATTERY";
	case EVENT_BELOW_LOW_BATTERY:
		return "EVENT_BELOW_LOW_BATTERY";
	/*from battery*/
	case EVENT_RECOGNIZE_BATTERY:
		return "EVENT_RECOGNIZE_BATTERY";
	case EVENT_NOT_RECOGNIZE_BATTERY:
		return "EVENT_NOT_RECOGNIZE_BATTERY";
	case EVENT_UNKNOW_BATTERY:
		return "EVENT_UNKNOW_BATTERY";
	case EVENT_WEAK_BATTERY:
		return "EVENT_WEAK_BATTERY";
	case EVENT_FULL_BATTERY:
		return "EVENT_FULL_BATTERY";
	case EVENT_RECHARGE_BATTERY:
		return "EVENT_RECHARGE_BATTERY";
	case EVENT_BATTERY_NTC_ZERO:
		return "EVENT_BATTERY_NTC_ZERO";
	case EVENT_BATTERY_NTC_NORMAL:
		return "EVENT_BATTERY_NTC_NORMAL";
	case EVENT_BATTERY_I2C_ERROR:
		return "EVENT_BATTERY_I2C_ERROR";
	case EVENT_BATTERY_I2C_NORMAL:
		return "EVENT_BATTERY_I2C_NORMAL";
	case EVENT_HEALTHY_BATTERY:
		return "EVENT_HEALTHY_BATTERY";
	case EVENT_TEMP_PROTECT_STEP_1:
		return "EVENT_TEMP_PROTECT_STEP_1";
	case EVENT_TEMP_PROTECT_STEP_2:
		return "EVENT_TEMP_PROTECT_STEP_2";
	case EVENT_TEMP_PROTECT_STEP_3:
		return "EVENT_TEMP_PROTECT_STEP_3";
	case EVENT_TEMP_PROTECT_STEP_4:
		return "EVENT_TEMP_PROTECT_STEP_4";
	case EVENT_TEMP_PROTECT_STEP_5:
		return "EVENT_TEMP_PROTECT_STEP_5";
	case EVENT_TEMP_PROTECT_STEP_6:
		return "EVENT_TEMP_PROTECT_STEP_6";
	case EVENT_TEMP_PROTECT_STEP_7:
		return "EVENT_TEMP_PROTECT_STEP_7";
	case EVENT_TEMP_PROTECT_STEP_8:
		return "EVENT_TEMP_PROTECT_STEP_8";
	case EVENT_RECHECK_PROTECTION:
		return "EVENT_RECHECK_PROTECTION";
	case EVENT_SHUTDOWN:
		return "SHUTDOWN";
	case EVENT_CURRENT_THERMAL_ADJUST:
		return "EVENT_CURRENT_THERMAL_ADJUST";
	default:
		return "UNDEFINED";
	}
}
void summit_fsm_stateTransform(struct summit_smb347_info *di,int event)
{
	int state = di->current_state;
	int i,cc,temp = 0;
	int config, command = 0;
	u8 pre_protect_event = 0;

	if (event == USB_EVENT_NONE && state != STATE_SHUTDOWN)
		state = STATE_SUSPEND;
	else if (event == EVENT_CHANGE_TO_ONDEMAND)
		state = STATE_ONDEMAND;
	else if (event == EVENT_SHUTDOWN)
		state = STATE_SHUTDOWN;
	else {
		switch (state) {
		case STATE_ONDEMAND:
			break;
		case STATE_SUSPEND:
			if (event == EVENT_CHECKINIT ||
				event == USB_EVENT_DETECT_SOURCE)
				state = STATE_INIT;
			break;
		case STATE_INIT:
			switch (event) {
			case USB_EVENT_VBUS:
			case EVENT_DETECT_PC:
				state = STATE_PC;
				break;
			case USB_EVENT_HOST_CHARGER:
				state = STATE_CD;
				break;
			case USB_EVENT_CHARGER:
				state = STATE_DC;
				break;
			case EVENT_DETECT_TBD:
				state = STATE_UC;
				break;
			case EVENT_DETECT_OTHER:
				state = STATE_OTHER;
				break;
			}
			break;
		case STATE_PC:
			switch (event) {
			case EVENT_DETECT_TBD:
				state = STATE_UC;
				break;
			}
			break;
		}
	}
	dev_info(di->dev, "%s:state : %s(%d) -> %s(%d) ; event : %s\n", __func__,
		fsm_state_string(di->current_state) , di->current_state ,
		fsm_state_string(state) , state , fsm_event_string(event));
	di->current_state = state;

	switch (state) {
	case STATE_SUSPEND:
		if (event == USB_EVENT_NONE) {
			di->usb_online = 0;
			di->ac_online = 0;
			power_supply_changed(&di->usb);
			power_supply_changed(&di->ac);
			/* stat*/
			summit_write_config(di , 1);
			config = i2c_smbus_read_byte_data(di->client, SUMMIT_SMB347_STAT_TIMERS);
			CLEAR_BIT(config , 5);
			i2c_smbus_write_byte_data(di->client, SUMMIT_SMB347_STAT_TIMERS, config);
			summit_write_config(di , 0);
			mdelay(10);
			summit_enable_stat_interrupt(di , LOW_BATTERY_TRIGGER_IRQ);
			/* Let charger go to suspend? */
			/*gpio_direction_output(di->pdata->pin_susp, 0);*/
			/*
			 * Turn off LEDs when USB is unplugged,
			 * this is safe when the battery is already
			 * discharging or full. There's no way
			 * the battery is being charged when USB
			 * is not connected.
			 */
			if (di->pdata->led_callback)
				di->pdata->led_callback(0, 0);
			CLEAR_BAT_FULL(di->protect_event);
			/*
			 * Hold the wake lock, and schedule a delayed work
			 * to release it in 1 second
			 */
			wake_lock(&di->chrg_lock);
			schedule_delayed_work(&di->disconnect_work, msecs_to_jiffies(1000));
		}
		break;
	case STATE_INIT:
		if (event == USB_EVENT_DETECT_SOURCE) {
			gpio_direction_output(di->pdata->pin_susp, 1);
			/*
			 * Cancel the scheduled delayed work that
			 * releases the wakelock
			 */
			cancel_delayed_work_sync(&di->disconnect_work);
			di->fake_disconnect = 0;
			if (di->pdata->mbid != 0) {
				temp = -1;
				config = i2c_smbus_read_byte_data(di->client, SUMMIT_SMB347_STAT_TIMERS);
				 if (IS_BIT_CLEAR(config , 5)) {
					/*The detect need to be redo again.*/
					dev_info(di->dev, "Redo apsd\n");
					/*disable stat*/
					summit_write_config(di , 1);
					config = i2c_smbus_read_byte_data(di->client, SUMMIT_SMB347_STAT_TIMERS);
					SET_BIT(config , 5);
					i2c_smbus_write_byte_data(di->client, SUMMIT_SMB347_STAT_TIMERS, config);
					summit_write_config(di, 0);
					summit_config_apsd(di, 0);
					summit_config_apsd(di, 1);
					queue_delayed_work_on(0, summit_work_queue, &di->summit_check_work, msecs_to_jiffies(2000));
				} else {
					/*The detect has already done in u-boot*/
					dev_info(di->dev, "The detect has already done in u-boot\n");
					temp = summit_get_apsd_result(di);
					writeIntoCBuffer(&di->events , temp);
					queue_delayed_work_on(0, summit_work_queue, &di->summit_monitor_work, 0);
				}
			}
			summit_charger_reconfig(di);
			summit_config_time_protect(di, 1);
			/*reload the protection setting*/
			if (IS_ADJUST_CURRENT_VOLTAGE_ENABLE(di->protect_enable) && di->pdata->mbid >= 4) {
				summit_adjust_charge_current(di , event);
			}
			if (pre_protect_event != di->protect_event && di->pdata->mbid >= 4) {
				summit_protection(di);
			}
			writeIntoCBuffer(&di->events , EVENT_CURRENT_THERMAL_ADJUST);
			queue_delayed_work(summit_work_queue, &di->summit_monitor_work, 0);
		}
		if (event == EVENT_APSD_COMPLETE) {
			writeIntoCBuffer(&di->events, summit_get_apsd_result(di));
			queue_delayed_work_on(0, summit_work_queue, &di->summit_monitor_work, 0);
		}
		break;
	case STATE_ONDEMAND:
		summit_charge_reset(di);
		break;
	case STATE_PC:
		if (event == EVENT_DETECT_PC) {
			atomic_notifier_call_chain(&di->xceiv->notifier, USB_EVENT_VBUS, NULL);
			di->usb_online = 1;
			di->ac_online = 0;
			power_supply_changed(&di->usb);
			power_supply_changed(&di->ac);
		}
		break;
	case STATE_CD:
		if (event == USB_EVENT_HOST_CHARGER) {
			if (di->pdata->mbid == 0)
				summit_switch_mode(di , HC_MODE);
			di->usb_online = 1;
			di->ac_online = 0;
			power_supply_changed(&di->usb);
			power_supply_changed(&di->ac);
			atomic_notifier_call_chain(&di->xceiv->notifier, USB_EVENT_VBUS, NULL);
		}
		break;
	case STATE_OTHER:
		/*It will switch to usb5 mode,so need to switch hc mode.*/
		if (event == EVENT_DETECT_OTHER) {
			if (di->pdata->mbid == 0) {
				summit_switch_mode(di, HC_MODE);
				if ((di->protect_enable == 1 && di->protect_event == 0) || di->protect_enable == 0)
					summit_charge_enable(di , 1);
			} else {
				summit_write_config(di, 1);
				config = i2c_smbus_read_byte_data(di->client, SUMMIT_SMB347_ENABLE_CONTROL);
				command &= ~(0x1 << (4));
				i2c_smbus_write_byte_data(di->client, SUMMIT_SMB347_ENABLE_CONTROL , config);
				summit_write_config(di , 0);
				summit_switch_mode(di , HC_MODE);
			}
			di->usb_online = 0;
			di->ac_online = 1;
			power_supply_changed(&di->usb);
			power_supply_changed(&di->ac);
		}
		/*Some PC get a usb port that was detected by Other charger*/
		if (event == EVENT_AICL_COMPLETE)
			atomic_notifier_call_chain(&di->xceiv->notifier, USB_EVENT_CHARGER, NULL);
		break;
	case STATE_DC:
		if (event == USB_EVENT_CHARGER) {
			if (di->pdata->mbid == 0)
				summit_switch_mode(di, HC_MODE);
			if (di->ac_online == 0) {
				di->usb_online = 0;
				di->ac_online = 1;
				power_supply_changed(&di->usb);
				power_supply_changed(&di->ac);
				atomic_notifier_call_chain(&di->xceiv->notifier, USB_EVENT_CHARGER, NULL);
			}
		}
		break;
	case STATE_UC:
		if (event == EVENT_DETECT_TBD) {
			if (di->pdata->mbid == 0) {
				summit_switch_mode(di, HC_MODE);
				if ((di->protect_enable == 1 && di->protect_event == 0) || di->protect_enable == 0)
					summit_charge_enable(di, 1);
			}
			di->usb_online = 0;
			di->ac_online = 1;
			power_supply_changed(&di->usb);
			power_supply_changed(&di->ac);
			atomic_notifier_call_chain(&di->xceiv->notifier, USB_EVENT_CHARGER, NULL);
		}
		break;
	}

	if (di->current_state != STATE_SUSPEND) {
		pre_protect_event = di->protect_event;
		switch (event) {
		case EVENT_FULL_BATTERY:
			SET_BAT_FULL(di->protect_event);
			break;
		case EVENT_RECHARGE_BATTERY:
			CLEAR_BAT_FULL(di->protect_event);
			break;
		case EVENT_BATTERY_I2C_ERROR:
			SET_BAT_I2C_FAIL(di->protect_event);
			break;
		case EVENT_BATTERY_I2C_NORMAL:
			CLEAR_BAT_I2C_FAIL(di->protect_event);
			break;
		case EVENT_NOT_RECOGNIZE_BATTERY:
			SET_BAT_NOT_RECOGNIZE(di->protect_event);
			break;
		case EVENT_RECOGNIZE_BATTERY:
			CLEAR_BAT_NOT_RECOGNIZE(di->protect_event);
			break;
		case EVENT_BATTERY_NTC_ZERO:
			SET_BAT_NTC_ERR(di->protect_event);
			break;
		case EVENT_BATTERY_NTC_NORMAL:
			CLEAR_BAT_NTC_ERR(di->protect_event);
			break;
		case EVENT_WEAK_BATTERY:
			SET_BAT_TOO_WEAK(di->protect_event);
			break;
		case EVENT_TEMP_PROTECT_STEP_2:
			/* -20 < temp < 0 Stop Charge*/
			SET_BAT_TOO_COLD_FOR_CHARGE(di->protect_event);
			break;
		case EVENT_TEMP_PROTECT_STEP_1:
			/* temp < -20 Stop Discharge*/
			SET_BAT_TOO_COLD_FOR_DISCHARGE(di->protect_event);
			break;
		case EVENT_TEMP_PROTECT_STEP_8:
			/* 60 > temp*/
			SET_BAT_TOO_HOT_FOR_CHARGE(di->protect_event);
			break;
		case EVENT_TEMP_PROTECT_STEP_3:
		case EVENT_TEMP_PROTECT_STEP_4:
		case EVENT_TEMP_PROTECT_STEP_5:
		case EVENT_TEMP_PROTECT_STEP_6:
		case EVENT_TEMP_PROTECT_STEP_7:
			CLEAR_BAT_TOO_COLD_FOR_CHARGE(di->protect_event);
			CLEAR_BAT_TOO_COLD_FOR_DISCHARGE(di->protect_event);
			CLEAR_BAT_TOO_HOT_FOR_CHARGE(di->protect_event);
			di->bat_thermal_step = event;
			if (IS_ADJUST_CURRENT_VOLTAGE_ENABLE(di->protect_enable) && di->pdata->mbid >= 4)
				summit_adjust_charge_current(di, event);
			break;
		case EVENT_SHUTDOWN:
			if (di->max_aicl != 1800) {
				di->max_aicl = 1800;
				summit_config_aicl(di, 0, 4200);
				i = summit_find_aicl(di->max_aicl);
				mdelay(2);
				summit_config_inpu_current(di,CONFIG_NO_CHANGE,i);
				mdelay(2);
				summit_config_aicl(di, 1, 4200);
			}
			/*reset charge current to maximum*/
			summit_charge_enable(di , 1);
			summit_force_precharge(di , 0);
			summit_config_charge_current(di, 7 << 5, CONFIG_NO_CHANGE, CONFIG_NO_CHANGE);
			break;
		case EVENT_CURRENT_THERMAL_ADJUST:
			i = summit_find_aicl(di->max_aicl);
			pr_info("%s pre_aicl =%d max_aicl=%d\n", __func__, di->pre_max_aicl, di->max_aicl);
			if (i > -1) {
				if (di->pre_max_aicl < di->max_aicl){
					summit_config_aicl(di, 0, 4200);
				}
				summit_config_inpu_current(di,CONFIG_NO_CHANGE,i);
				if (di->pre_max_aicl < di->max_aicl){
					mdelay(5);
					summit_config_aicl(di, 1, 4200);
				}
				di->pre_max_aicl = di->max_aicl;
			}
			pr_info("%s di->charge_current =%d di->charge_current_redunction=%d\n",__func__,di->charge_current ,di->charge_current_redunction);
			cc = di->charge_current - di->charge_current_redunction;
			if(cc < 0)
				cc = 0;
			if( cc > 0){
				i = summit_find_fast_cc(cc);
				if (i > -1) {
					summit_force_precharge(di , 0);
					summit_config_charge_current(di, i << 5, CONFIG_NO_CHANGE, CONFIG_NO_CHANGE);
					CLEAR_BAT_NO_SUIT_CURRENT(di->protect_event);
				} else {
					i = summit_find_pre_cc(cc);
					if (i > -1) {
						summit_force_precharge(di , 1);
						summit_config_charge_current(di, CONFIG_NO_CHANGE, i << 3, CONFIG_NO_CHANGE);
						CLEAR_BAT_NO_SUIT_CURRENT(di->protect_event);
					} else {
						summit_force_precharge(di , 0);
						SET_BAT_NO_SUIT_CURRENT(di->protect_event);
					}
				}
			} else {
				SET_BAT_NO_SUIT_CURRENT(di->protect_event);
			}
			break;
		}
		if ((pre_protect_event != di->protect_event && di->pdata->mbid >= 4) ||
			(event == EVENT_RECHECK_PROTECTION)) {
			if (event == EVENT_RECHECK_PROTECTION)
				summit_adjust_charge_current(di, di->bat_thermal_step);
			summit_protection(di);
		} else if (di->pdata->mbid < 4) {
			summit_charge_enable(di, 1);
		}
	}
}
/* END FSM */

/* BEGIN POWER SUPPLY */
static enum power_supply_property summit_usb_props[] = { POWER_SUPPLY_PROP_ONLINE, };
static enum power_supply_property summit_ac_props[] = { POWER_SUPPLY_PROP_ONLINE, };

static int summit_ac_get_property(struct power_supply *psy, enum power_supply_property psp, union power_supply_propval *val)
{
	struct summit_smb347_info *di = container_of(psy,struct summit_smb347_info, ac);
	switch (psp) {
		case POWER_SUPPLY_PROP_ONLINE:
			val->intval = di->ac_online;
		break;
		default:
			return -EINVAL;
	}
	return 0;
}

static int summit_usb_get_property(struct power_supply *psy, enum power_supply_property psp, union power_supply_propval *val)
{
	struct summit_smb347_info *di = container_of(psy,struct summit_smb347_info, usb);
	switch (psp) {
		case POWER_SUPPLY_PROP_ONLINE:
			val->intval = di->usb_online;
		break;
		default:
			return -EINVAL;
	}
	return 0;
}
/* END POWER SUPPLY */

/*
Dvt(mbid=4)
->SUSP pin ->UART4_RX/GPIO155
->EN pin   ->C2C_DATA12/GPIO101
->
*/
static int __devinit summit_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct summit_smb347_info *di;
	u8 value , config;
	int ret;
	int status;
	struct summit_smb347_platform_data *pdata = client->dev.platform_data;

	printk(KERN_INFO "%s\n" , __func__);
	di = kzalloc(sizeof(*di) ,  GFP_KERNEL);
	if (!di)
		return -ENOMEM;

	di->pdata = pdata;

	di->client			= client;
	di->dev				= &client->dev;
	di->usb_notifier.notifier_call	= summit_usb_notifier_call;
	di->bat_notifier.notifier_call	= summit_bat_notifier_call;
	di->irq				= client->irq;
	di->xceiv			= usb_get_phy(USB_PHY_TYPE_USB2);
	di->bad_battery			= 0;
	di->protect_enable		= 0;
	di->fake_disconnect		= 0;
	di->thermal_adjust_mode		= 0;
	di->max_aicl			= di->pdata->initial_max_aicl;
	di->pre_max_aicl		= di->pdata->initial_pre_max_aicl;
	di->charge_current		= di->pdata->initial_charge_current;
	di->charge_current_redunction	= 0;
	di->protect_enable		= 0;

	i2c_set_clientdata(client, di);

	if (di->pdata->mbid >= 4) {
		/* Only work after DVT */
		SET_BAT_NO_SUIT_CURRENT(di->protect_enable);
	}
	if (di->pdata->pin_en) {
		gpio_request(di->pdata->pin_en, di->pdata->pin_en_name);
		gpio_direction_output(di->pdata->pin_en, 0);
		mdelay(10);
	}
	if (di->pdata->pin_susp) {
		gpio_request(di->pdata->pin_susp,  di->pdata->pin_susp_name);
		gpio_direction_output(di->pdata->pin_susp, 1);
		mdelay(10);
	}

	if (di->pdata->mbid >= 4) {
		/* Check for device ID */
		if (summit_smb347_read_id(di) < 0) {
			pr_err("%s:Unable to detect device ID\n",__func__);
			return -ENODEV;
		}
		ret = twl_i2c_read_u8(TWL6030_MODULE_CHARGER, &value, CONTROLLER_STAT1);
		if (value & VBUS_DET) {
			dev_info(di->dev , "USBIN in use\n");
		} else {
			summit_write_config(di , 1);
			config = i2c_smbus_read_byte_data(di->client, SUMMIT_SMB347_STAT_TIMERS);
			CLEAR_BIT(config , 5);
			i2c_smbus_write_byte_data(di->client, SUMMIT_SMB347_STAT_TIMERS, config);
			summit_write_config(di , 0);
		}
		summit_charger_reconfig(di);
		mdelay(10);
	}
	printk(KERN_INFO "Summit SMB347 detected ,chip_id=0x%x board id=%d\n", di->id , di->pdata->mbid);

#ifdef CONFIG_THERMAL_FRAMEWORK
	//di->tdev = kzalloc(sizeof(struct thermal_dev), GFP_KERNEL);
	di->tdev.name = "charger_cooling";
	di->tdev.domain_name = "case";
	di->tdev.dev_ops = &smb347_cooling_ops,
	di->tdev.dev = di->dev;
	ret = thermal_cooling_dev_register(&di->tdev);
#endif
	INIT_DELAYED_WORK_DEFERRABLE(&di->summit_monitor_work, summit_monitor_work_func);
	INIT_DELAYED_WORK_DEFERRABLE(&di->summit_check_work, summit_check_work_func);
	initCBuffer(&di->events , 30);
	summit_init_fsm(di);
	usb_phy_init(di->xceiv);
	status = usb_register_notifier(di->xceiv, &di->usb_notifier);
	status = bq275xx_register_notifier(&di->bat_notifier);
	wake_lock_init(&di->chrg_lock, WAKE_LOCK_SUSPEND, "usb_wake_lock");
	wake_lock_init(&di->summit_lock, WAKE_LOCK_SUSPEND, "summit_wake_lock");

	di->usb.name		= "usb";
	di->usb.type		= POWER_SUPPLY_TYPE_USB;
	di->usb.properties	= summit_usb_props;
	di->usb.num_properties	= ARRAY_SIZE(summit_usb_props);
	di->usb.get_property	= summit_usb_get_property;

	di->ac.name		= "ac";
	di->ac.type		= POWER_SUPPLY_TYPE_MAINS;
	di->ac.properties	= summit_ac_props;
	di->ac.num_properties	= ARRAY_SIZE(summit_ac_props);
	di->ac.get_property	= summit_ac_get_property;

	ret = power_supply_register(di->dev, &di->usb);
	if (ret)
		dev_dbg(di->dev,"failed to register usb power supply\n");

	ret = power_supply_register(di->dev, &di->ac);
	if (ret)
		dev_dbg(di->dev,"failed to register ac power supply\n");

	/* Create a sysfs node */
	ret = sysfs_create_group(&di->dev->kobj ,  &summit_attr_grp);
	if (ret) {
		dev_dbg(di->dev , "could not create sysfs_create_group\n");
		return -1;
	}
	dev_dbg(di->dev , KERN_DEBUG"%s:init done\n" , __func__);

	INIT_DELAYED_WORK(&di->disconnect_work ,  disconnect_work_func);

	/*enable low battery interrupt*/
	summit_enable_stat_interrupt(di, LOW_BATTERY_TRIGGER_IRQ);
	/* request charger ctrl interruption */
	ret = request_threaded_irq(di->irq, NULL, summit_charger_interrupt, IRQF_TRIGGER_FALLING, "summit_isr",  di);
	if (ret) {
		printk(KERN_ERR "Failed to request IRQ %d: %d\n", client->irq,  ret);
	}
	irq_set_irq_wake(di->irq , 1);

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

	/* Remove sysfs node */
	sysfs_remove_group(&di->dev->kobj ,  &summit_attr_grp);

	power_supply_unregister(&di->ac);
	power_supply_unregister(&di->usb);

	kfree(di);

	return 0;
}

static void summit_shutdown(struct i2c_client *client)
{
	struct summit_smb347_info *di = i2c_get_clientdata(client);

	dev_info(di->dev ,  "%s\n" ,  __func__);

	disable_irq(di->irq);
	free_irq(di->irq ,  di);
	usb_register_notifier(di->xceiv, &di->usb_notifier);
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
	.id_table	= summit_smb347_id ,
	.probe		= summit_probe ,
	.suspend	= summit_suspend ,
	.resume		= summit_resume ,
	.remove		= summit_remove ,
	.shutdown	= summit_shutdown ,
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
