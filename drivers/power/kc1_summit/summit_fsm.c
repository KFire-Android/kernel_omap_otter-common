#include "smb347.h"
#include <plat/led.h>
/*#define DEBUG 1*/
extern struct workqueue_struct	*summit_work_queue;
void summit_init_fsm(struct summit_smb347_info *di)
{
	di->current_state = STATE_SUSPEND;
}
const char *fsm_state_string(int state)
{
	switch (state) {
	case STATE_SUSPEND:	  return "suspend";
	case STATE_ONDEMAND:	return "ondemand";
	case STATE_INIT: return "init";
	case STATE_PC:	  return "pc";
	case STATE_CD:	return "STATE_CD";
	case STATE_DC:	return "Dedicated charger";
	case STATE_UC:  return "Unknow charger";
	case STATE_OTHER:	  return "Other charger";
	case STATE_CHARGING:  return "charging";
	case STATE_SHUTDOWN: return "shutdown";
	default:			return "UNDEFINED";
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
	dev_info(di->dev,
	"%s:state : %s(%d) -> %s(%d) ; event : %s\n", __func__,
	fsm_state_string(di->current_state) , di->current_state ,
	fsm_state_string(state) , state , fsm_event_string(event));
	di->current_state = state;
	summit_fsm_doAction(di , event);
}

void summit_fsm_doAction(struct summit_smb347_info *di,int event)
{
	int state = di->current_state;
	int i,cc,temp = 0;
	int config, command = 0;
	u8  pre_protect_event = 0;
	switch (state) {
	case STATE_SUSPEND:
		if (event == USB_EVENT_NONE) {
			di->usb_online = 0;
			di->ac_online = 0;
			power_supply_changed(&di->usb);
			power_supply_changed(&di->ac);
			/* stat*/
			summit_write_config(di , 1);
			config = i2c_smbus_read_byte_data(di->client,
					SUMMIT_SMB347_STAT_TIMERS);
			CLEAR_BIT(config , 5);
			i2c_smbus_write_byte_data(di->client,
					SUMMIT_SMB347_STAT_TIMERS, config);
			summit_write_config(di , 0);
			mdelay(10);
			summit_enable_stat_interrupt(di , LOW_BATTERY_TRIGGER_IRQ);
			/*Let charger go to suspend*/
			/*gpio_direction_output(di->pin_susp, 0);*/
			/*
			 * Turn off LEDs when USB is unplugged,
			 * this is safe when the battery is already
			 * discharging or full. There's no way
			 * the battery is being charged when USB
			 * is not connected.
			 */
			omap4430_green_led_set(NULL, 0);
			omap4430_orange_led_set(NULL, 0);
			CLEAR_BAT_FULL(di->protect_event);
			/*
			 * Hold the wake lock, and schedule a delayed work
			 * to release it in 1 second
			 */
			wake_lock(&di->chrg_lock);
			schedule_delayed_work(&di->disconnect_work,
				msecs_to_jiffies(1000));
		}
		break;
	case STATE_INIT:
		if (event == USB_EVENT_DETECT_SOURCE) {
			gpio_direction_output(di->pin_susp, 1);
			/*
			 * Cancel the scheduled delayed work that
			 * releases the wakelock
			 */
			cancel_delayed_work_sync(&di->disconnect_work);
			di->fake_disconnect = 0;
			if (di->mbid != 0) {
				temp = -1;
				config = i2c_smbus_read_byte_data(di->client,
					SUMMIT_SMB347_STAT_TIMERS);
				 if (IS_BIT_CLEAR(config , 5)) {
					/*The detect need to be redo again.*/
					dev_info(di->dev, "Redo apsd\n");
					/*disable stat*/
					summit_write_config(di , 1);
					config = i2c_smbus_read_byte_data(di->client,
						SUMMIT_SMB347_STAT_TIMERS);
					SET_BIT(config , 5);
					i2c_smbus_write_byte_data(di->client,
						SUMMIT_SMB347_STAT_TIMERS, config);
					summit_write_config(di , 0);
					summit_config_apsd(di , 0);
					summit_config_apsd(di , 1);
					queue_delayed_work_on(0 , summit_work_queue,
						&di->summit_check_work, msecs_to_jiffies(2000));
				} else {
					/*The detect has already done in u-boot*/
					dev_info(di->dev, "The detect has already done in u-boot\n");
					temp = summit_get_apsd_result(di);
					writeIntoCBuffer(&di->events , temp);
					queue_delayed_work_on(0 , summit_work_queue,
						&di->summit_monitor_work, 0);
				}
			}
			summit_charger_reconfig(di);
			summit_config_time_protect(di , 1);
			/*reload the protection setting*/
			if (IS_ADJUST_CURRENT_VOLTAGE_ENABLE(di->protect_enable) &&
				di->mbid >= 4) {
				summit_adjust_charge_current(di , event);
			}
			if (pre_protect_event != di->protect_event &&
				di->mbid >= 4) {
				summit_protection(di);
			}
			writeIntoCBuffer(&di->events , EVENT_CURRENT_THERMAL_ADJUST);
			queue_delayed_work(summit_work_queue ,
				&di->summit_monitor_work , 0);
		}
		if (event == EVENT_APSD_COMPLETE) {
				writeIntoCBuffer(&di->events, summit_get_apsd_result(di));
				queue_delayed_work_on(0 , summit_work_queue,
					&di->summit_monitor_work, 0);
		}
		break;
	case STATE_ONDEMAND:
		summit_charge_reset(di);
		break;
	case STATE_PC:
		if (event == EVENT_DETECT_PC) {
			atomic_notifier_call_chain(&di->xceiv->notifier,
				USB_EVENT_VBUS , NULL);
			di->usb_online = 1;
			di->ac_online = 0;
			power_supply_changed(&di->usb);
			power_supply_changed(&di->ac);
		}
		break;
	case STATE_CD:
		if (event == USB_EVENT_HOST_CHARGER) {
			if (di->mbid == 0)
				summit_switch_mode(di , HC_MODE);
			di->usb_online = 1;
			di->ac_online = 0;
			power_supply_changed(&di->usb);
			power_supply_changed(&di->ac);
			atomic_notifier_call_chain(&di->xceiv->notifier,
				USB_EVENT_VBUS , NULL);
		}
		break;
	case STATE_OTHER:
	/*It will switch to usb5 mode,so need to switch hc mode.*/
		if (event == EVENT_DETECT_OTHER) {
			if (di->mbid == 0) {
				summit_switch_mode(di, HC_MODE);
				if ((di->protect_enable == 1 &&
				di->protect_event == 0) ||
				di->protect_enable == 0)
					summit_charge_enable(di , 1);
			} else {
				summit_write_config(di, 1);
				config = i2c_smbus_read_byte_data(di->client,
					SUMMIT_SMB347_ENABLE_CONTROL);
				command &= ~(0x1 << (4));
				i2c_smbus_write_byte_data(di->client,
					SUMMIT_SMB347_ENABLE_CONTROL , config);
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
			atomic_notifier_call_chain(&di->xceiv->notifier,
				USB_EVENT_CHARGER, NULL);
		break;
	case STATE_DC:
		if (event == USB_EVENT_CHARGER) {
			if (di->mbid == 0)
				summit_switch_mode(di, HC_MODE);
			if (di->ac_online == 0) {
				di->usb_online = 0;
				di->ac_online = 1;
				power_supply_changed(&di->usb);
				power_supply_changed(&di->ac);
				atomic_notifier_call_chain(&di->xceiv->notifier,
					USB_EVENT_CHARGER , NULL);
			}
		}
		break;
	case STATE_UC:
		if (event == EVENT_DETECT_TBD) {
			if (di->mbid == 0) {
				summit_switch_mode(di, HC_MODE);
				if ((di->protect_enable == 1 &&
					di->protect_event == 0) ||
					di->protect_enable == 0)
					summit_charge_enable(di , 1);
			}
			di->usb_online = 0;
			di->ac_online = 1;
			power_supply_changed(&di->usb);
			power_supply_changed(&di->ac);
			atomic_notifier_call_chain(&di->xceiv->notifier,
				USB_EVENT_CHARGER , NULL);
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
			if (IS_ADJUST_CURRENT_VOLTAGE_ENABLE(di->protect_enable) && di->mbid >= 4)
				summit_adjust_charge_current(di, event);
			break;
		case EVENT_SHUTDOWN:
			if(di->max_aicl != 1800){
				di->max_aicl = 1800;
				summit_config_aicl(di,0,4200);
				i = summit_find_aicl(di->max_aicl);
				mdelay(2);
				summit_config_inpu_current(di,CONFIG_NO_CHANGE,i);
				mdelay(2);
				summit_config_aicl(di,1,4200);
			}
			/*reset charge current to maximum*/
			summit_charge_enable(di , 1);
			summit_force_precharge(di , 0);
			summit_config_charge_current(di , 7 << 5 ,
						CONFIG_NO_CHANGE , CONFIG_NO_CHANGE);
			break;
		case EVENT_CURRENT_THERMAL_ADJUST:
				i = summit_find_aicl(di->max_aicl);
				pr_info("%s pre_aicl =%d max_aicl=%d\n",__func__,di->pre_max_aicl , di->max_aicl);
				if (i > -1) {
					if (di->pre_max_aicl < di->max_aicl){
						summit_config_aicl(di,0,4200);
					}
					summit_config_inpu_current(di,CONFIG_NO_CHANGE,i);
					if (di->pre_max_aicl < di->max_aicl){
						mdelay(5);
						summit_config_aicl(di,1,4200);
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
				} else {
					SET_BAT_NO_SUIT_CURRENT(di->protect_event);
				}
			break;
		}
		if ((pre_protect_event != di->protect_event && di->mbid >= 4) ||
			(event == EVENT_RECHECK_PROTECTION)) {
			if (event == EVENT_RECHECK_PROTECTION)
				summit_adjust_charge_current(di , di->bat_thermal_step);
			summit_protection(di);
		} else if (di->mbid < 4) {
			summit_charge_enable(di , 1);
		}
	}
}
