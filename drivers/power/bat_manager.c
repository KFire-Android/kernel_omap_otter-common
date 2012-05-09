/*
 * SAMSUNG battery manager driver for Android
 *
 * Copyright (C) 2011 SAMSUNG ELECTRONICS.
 * HongMin Son(hongmin.son@samsung.com)
 *
 * This package is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * THIS PACKAGE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/workqueue.h>
#include <linux/power_supply.h>
#include <linux/wakelock.h>
#include <linux/err.h>
#include <linux/delay.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/android_alarm.h>
#include <linux/proc_fs.h>
#include <linux/usb/otg.h>
#include <linux/notifier.h>
#include <linux/reboot.h>
#include <linux/battery.h>
#include <linux/bat_manager.h>
#include <linux/gpio.h>

#define FAST_POLL	(1 * 40)
#define SLOW_POLL	(30 * 60)

#define STATUS_BATT_CHARGABLE           0x0
#define STATUS_BATT_FULL		0x1
#define STATUS_BATT_CHARGE_TIME_OUT	0x2
#define STATUS_BATT_ABNOMAL_TEMP	0x4

#define STABLE_LOW_BATTERY_DIFF		3
#define STABLE_LOW_BATTERY_DIFF_LOWBATT	1

struct charger_device_info {
	struct device		*dev;
	struct work_struct	bat_work;
	struct work_struct	charger_work;
	struct batman_platform_data *pdata;
	struct bat_information	bat_info;
	struct battery_manager_callbacks callbacks;

	struct otg_transceiver	*transceiver;
	struct notifier_block	otg_nb;
	struct mutex		mutex;
	struct mutex		cable_mutex;

	struct power_supply	psy_bat;
	struct power_supply	psy_usb;
	struct power_supply	psy_ac;
	struct alarm		alarm;
	struct workqueue_struct *monitor_wqueue;
	struct wake_lock	work_wake_lock;

	enum cable_type_t	cable_status;
	int			present;
	int			charge_status;
	int			discharge_status;
	int			fg_chk_cnt;
	unsigned long           chg_limit_time;
	unsigned int		polling_interval;
	int                     slow_poll;
	bool			is_cable_attached;
	bool			is_full_charged;
	bool			once_fully_charged;
	bool			low_batt_boot_flag;
	bool			is_low_batt_alarm;

	ktime_t                 last_poll;
};

static char *supply_list[] = {
	"battery",
};

static enum power_supply_property batman_battery_properties[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_TECHNOLOGY,
};

static enum power_supply_property batman_power_properties[] = {
	POWER_SUPPLY_PROP_ONLINE,
};

static void set_full_charge(struct battery_manager_callbacks *ptr)
{
	struct charger_device_info *di = container_of(ptr,
			struct charger_device_info, callbacks);

	pr_info("%s : Full Charged Interrupt occured !!!!!\n",
			__func__);
	di->is_full_charged = true;
	di->pdata->full_charger_comp(di->once_fully_charged, 1);
	wake_lock(&di->work_wake_lock);
	queue_work(di->monitor_wqueue, &di->bat_work);
}

static int fuelgauge_recovery(struct charger_device_info *di)
{
	int current_soc;

	if (di->bat_info.soc > 0) {
		pr_err("%s: Reduce the Reported SOC by 1 unit, wait for 30s\n",
				__func__);
		if (di->pdata->get_fuel_value)
			current_soc = di->pdata->get_fuel_value(READ_FG_SOC);

		if (current_soc) {
			pr_info("%s: Returning to Normal discharge path.\n",
					__func__);
			pr_info(" Actual SOC(%d) non-zero.\n",
					current_soc);
			di->is_low_batt_alarm = false;
			goto return_soc;
		} else {
			current_soc = di->bat_info.soc--;
			pr_info("%s: New Reduced Reported SOC  (%d).\n",
					__func__, di->bat_info.soc);
			goto return_soc ;
		}
	} else {
		if (!di->is_cable_attached) {
			current_soc = 0;
			pr_err("Set battery level as 0, power off.\n");
		}
	}

return_soc:
	return current_soc;
}

static void batman_low_battery_alarm(struct charger_device_info *di)
{
	int overcurrent_limit_in_soc;
	int current_soc = 0;

	if (di->pdata->get_fuel_value)
		current_soc = di->pdata->get_fuel_value(READ_FG_SOC);

	if (di->bat_info.soc <= STABLE_LOW_BATTERY_DIFF)
		overcurrent_limit_in_soc = STABLE_LOW_BATTERY_DIFF_LOWBATT;
	else
		overcurrent_limit_in_soc = STABLE_LOW_BATTERY_DIFF;

	if ((di->bat_info.soc - current_soc) > overcurrent_limit_in_soc) {
		pr_info("%s: Abnormal Current Consumption jump by %d units.\n",
			__func__, ((di->bat_info.soc - current_soc)));
		pr_info("Last Reported SOC (%d).\n",
				di->bat_info.soc);

		di->is_low_batt_alarm = true;
		wake_lock(&di->work_wake_lock);
		queue_work(di->monitor_wqueue, &di->bat_work);
	}
}

static void lowbat_fuel_alert(struct battery_manager_callbacks *ptr)
{
	struct charger_device_info *di = container_of(ptr,
			struct charger_device_info, callbacks);

	if (di->pdata->get_fuel_value &&
			di->pdata->get_fuel_value(READ_FG_STATUS))
		batman_low_battery_alarm(di);
}

static int batman_bat_get_property(struct power_supply *bat_ps,
		enum power_supply_property psp,
		union power_supply_propval *val)
{
	struct charger_device_info *di = container_of(bat_ps,
			struct charger_device_info, psy_bat);

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		val->intval = di->charge_status;
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		val->intval = di->bat_info.health;
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = 1;
		break;
	case POWER_SUPPLY_PROP_TEMP:
		val->intval = di->bat_info.temp;
		dev_dbg(di->dev, "temperature value : %d\n",
			val->intval);
		break;
	case POWER_SUPPLY_PROP_ONLINE:
		/* battery is always online */
		val->intval = 1;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		val->intval = di->bat_info.vcell;
		dev_dbg(di->dev, "voltage value : %d\n",
			val->intval);
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		if (di->charge_status == POWER_SUPPLY_STATUS_FULL)
			val->intval = 100;
		else
			val->intval = di->bat_info.soc;
		dev_dbg(di->dev, "soc value : %d\n",
			val->intval);
		break;
	case POWER_SUPPLY_PROP_TECHNOLOGY:
		val->intval = POWER_SUPPLY_TECHNOLOGY_LION;
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static int batman_usb_get_property(struct power_supply *ps,
		enum power_supply_property psp,
		union power_supply_propval *val)
{
	struct charger_device_info *di =
		container_of(ps, struct charger_device_info, psy_usb);

	if (psp != POWER_SUPPLY_PROP_ONLINE)
		return -EINVAL;

	/* Set enable=1 only if the USB charger is connected */
	val->intval = (di->cable_status == CABLE_TYPE_USB);

	return 0;
}

static int batman_ac_get_property(struct power_supply *ps,
		enum power_supply_property psp,
		union power_supply_propval *val)
{
	struct charger_device_info *di =
		container_of(ps, struct charger_device_info, psy_ac);

	if (psp != POWER_SUPPLY_PROP_ONLINE)
		return -EINVAL;

	/* Set enable=1 only if the AC charger is connected */
	val->intval = (di->cable_status == CABLE_TYPE_AC);

	return 0;
}

static bool batman_check_UV_charging(struct charger_device_info *di)
{
	int fg_vcell = 0;
	int fg_current = 0;
	int threshold = 0;

	if (di->pdata->get_fuel_value) {
		fg_vcell = di->pdata->get_fuel_value(READ_FG_VCELL);
		fg_current = di->pdata->get_fuel_value(READ_FG_CURRENT);
	}

	threshold = 3300 + ((fg_current * 17) / 100);

	return (fg_vcell <= threshold ? 1 : 0);
}

static void batman_get_temp_status(struct charger_device_info *di)
{
	if (!di->pdata->get_fuel_value) {
		di->bat_info.health = POWER_SUPPLY_HEALTH_UNKNOWN;
		return;
	} else {
		di->bat_info.temp =
			di->pdata->get_fuel_value(READ_FG_TEMP);
	}

	if (di->bat_info.temp >= di->pdata->high_block_temp) {
		di->bat_info.health = POWER_SUPPLY_HEALTH_OVERHEAT;
	} else if (di->bat_info.temp <= di->pdata->high_recover_temp &&
			di->bat_info.temp >= di->pdata->low_recover_temp) {
		di->bat_info.health = POWER_SUPPLY_HEALTH_GOOD;
	} else if (di->bat_info.temp <= di->pdata->low_block_temp) {
		di->bat_info.health = POWER_SUPPLY_HEALTH_COLD;
	}
}

static int batman_charging_status_check(struct charger_device_info *di)
{
	int ret = 0;

	ktime_t ktime;
	struct timespec cur_time;

	ktime = alarm_get_elapsed_realtime();
	cur_time = ktime_to_timespec(ktime);

	switch (di->discharge_status) {
	case STATUS_BATT_CHARGABLE:
		if (di->chg_limit_time &&
				cur_time.tv_sec > di->chg_limit_time) {
			if (di->pdata->bootmode == 5 ||
					di->cable_status != CABLE_TYPE_USB) {
				if (di->bat_info.vcell > 4150000)
					di->charge_status =
						POWER_SUPPLY_STATUS_FULL;
			}
			di->once_fully_charged = true;
			di->discharge_status = STATUS_BATT_CHARGE_TIME_OUT;
		} else if (di->bat_info.health ==
				POWER_SUPPLY_HEALTH_OVERHEAT ||
				di->bat_info.health ==
				POWER_SUPPLY_HEALTH_COLD) {
			if (di->pdata->bootmode == 5 ||
					di->cable_status != CABLE_TYPE_USB)
				di->charge_status =
					POWER_SUPPLY_STATUS_NOT_CHARGING;
			di->discharge_status = STATUS_BATT_ABNOMAL_TEMP;
		} else if (di->is_full_charged) {
			if (di->pdata->bootmode == 5 ||
					di->cable_status != CABLE_TYPE_USB) {
				di->charge_status = POWER_SUPPLY_STATUS_FULL;
				di->once_fully_charged = true;
			}
			di->discharge_status = STATUS_BATT_FULL;
		}
		break;
	case STATUS_BATT_ABNOMAL_TEMP:
		if (di->bat_info.temp <= di->pdata->high_recover_temp &&
				di->bat_info.temp >=
				di->pdata->low_recover_temp) {
			di->discharge_status = STATUS_BATT_CHARGABLE;
			if (di->pdata->bootmode == 5 ||
					di->cable_status != CABLE_TYPE_USB)
				di->charge_status =
					POWER_SUPPLY_STATUS_CHARGING;
		}
		break;
	case STATUS_BATT_FULL:
	case STATUS_BATT_CHARGE_TIME_OUT:
		if (di->bat_info.vcell < di->pdata->recharge_voltage)
			di->discharge_status = STATUS_BATT_CHARGABLE;
		break;
	}

	if (!di->discharge_status) {
		di->pdata->set_charger_en(1);
		if (!di->chg_limit_time && (di->pdata->bootmode == 5 ||
				(di->cable_status != CABLE_TYPE_USB))) {
			di->chg_limit_time =
				di->once_fully_charged ?
				cur_time.tv_sec +
				di->pdata->limit_recharging_time :
				cur_time.tv_sec +
				di->pdata->limit_charging_time;
		}
	} else {
		di->pdata->set_charger_en(0);
		di->chg_limit_time = 0;
	}

	return ret;
}

static void charger_detect_work(struct work_struct *work)
{
	struct charger_device_info *di =
		container_of(work, struct charger_device_info, charger_work);

	mutex_lock(&di->cable_mutex);
	if (!gpio_get_value(di->pdata->ta_gpio)) {
		di->is_cable_attached = true;
		di->cable_status = di->pdata->get_charger_type();
		if (di->pdata->bootmode == 5 ||
				di->cable_status != CABLE_TYPE_USB)
			di->charge_status = POWER_SUPPLY_STATUS_CHARGING;
	} else {
		if (di->is_full_charged) {
			di->pdata->update_fullcap_value();
			di->is_full_charged = false;
		}
		di->is_cable_attached = false;
		di->charge_status = POWER_SUPPLY_STATUS_DISCHARGING;
		di->cable_status = CABLE_TYPE_NONE;
		di->once_fully_charged = false;
		di->chg_limit_time = 0;
	}

	di->discharge_status = STATUS_BATT_CHARGABLE;
	pr_info("%s: cable flag : %d, cable_type : %d\n",
			__func__, di->is_cable_attached, di->cable_status);
	di->pdata->set_charger_state(di->cable_status);
	di->pdata->set_charger_en(di->cable_status);

	mutex_unlock(&di->cable_mutex);
	power_supply_changed(&di->psy_ac);
	if (di->pdata->bootmode == 5)
		power_supply_changed(&di->psy_usb);

	wake_lock(&di->work_wake_lock);
	queue_work(di->monitor_wqueue, &di->bat_work);
}

static int batman_get_bat_level(struct charger_device_info *di)
{
	int soc = 0;

	/* check VFcapacity every five minutes */
	if (!(di->fg_chk_cnt++ % 10)) {
		di->pdata->check_vf_fullcap_range();
		di->fg_chk_cnt = 1;
	}

	if (di->pdata->get_fuel_value) {
		soc = di->is_low_batt_alarm ? fuelgauge_recovery(di) :
			di->pdata->get_fuel_value(READ_FG_SOC);
		if (!di->is_full_charged && soc > 99 && di->is_cable_attached)
			soc = 99;
		di->bat_info.soc = soc;
	}

	if (di->low_batt_boot_flag) {
		soc = 0;

		if (di->is_cable_attached && !batman_check_UV_charging(di)) {
			di->pdata->fg_adjust_capacity();
			di->low_batt_boot_flag = 0;
		} else if (!di->is_cable_attached) {
			di->low_batt_boot_flag = 0;
		}
	}

	/* if (!battery->pdata->check_jig_status() && */
	if (!di->pdata->jig_on &&
			di->charge_status != POWER_SUPPLY_STATUS_CHARGING)
		soc = di->pdata->low_bat_compensation(di->bat_info);

	return soc;
}

static void batman_program_alarm(struct charger_device_info *di, int seconds)
{
	ktime_t low_interval = ktime_set(seconds - 10, 0);
	ktime_t slack = ktime_set(20, 0);
	ktime_t next;

	next = ktime_add(di->last_poll, low_interval);
	alarm_start_range(&di->alarm, next, ktime_add(next, slack));
}

static void battery_manager_work(struct work_struct *work)
{
	struct charger_device_info *di =
		container_of(work, struct charger_device_info, bat_work);

	int prev_status = di->charge_status;
	int prev_soc = di->bat_info.soc;

	unsigned long flags;
	struct timespec ts;

	mutex_lock(&di->mutex);
	if (di->pdata->get_fuel_value) {
		di->bat_info.vcell =
			di->pdata->get_fuel_value(READ_FG_VCELL);
		di->bat_info.avg_current =
			di->pdata->get_fuel_value(READ_FG_AVG_CURRENT);
		di->bat_info.fg_current =
			di->pdata->get_fuel_value(READ_FG_CURRENT);
		di->bat_info.soc = batman_get_bat_level(di);
	}

	batman_get_temp_status(di);

	if (di->is_cable_attached)
		batman_charging_status_check(di);

	mutex_unlock(&di->mutex);

	if ((di->bat_info.soc != prev_soc) || (di->bat_info.soc <= 3) ||
			(di->charge_status != prev_status))
		power_supply_changed(&di->psy_bat);

	dev_info(di->dev, "vcell = %d soc = %d  current = %d avg_current = %d "
		"status = %d health = %d temp = %d "
		"discharge status = %d, limit time : %ld, bootmode : %d\n",
		di->bat_info.vcell, di->bat_info.soc, di->bat_info.fg_current,
		di->bat_info.avg_current, di->charge_status,
		di->bat_info.health, di->bat_info.temp,
		di->discharge_status, di->chg_limit_time, di->pdata->bootmode);

	di->last_poll = alarm_get_elapsed_realtime();
	ts = ktime_to_timespec(di->last_poll);

	local_irq_save(flags);
	wake_unlock(&di->work_wake_lock);
	batman_program_alarm(di, FAST_POLL);
	local_irq_restore(flags);
}

static void batman_battery_alarm(struct alarm *alarm)
{
	struct charger_device_info *di =
		container_of(alarm, struct charger_device_info, alarm);

	wake_lock(&di->work_wake_lock);
	queue_work(di->monitor_wqueue, &di->bat_work);
}

static int otg_handle_notification(struct notifier_block *nb,
		unsigned long event, void *unused)
{
	struct charger_device_info *di = container_of(nb,
			struct charger_device_info, otg_nb);

	switch (event) {
	case USB_EVENT_VBUS_CHARGER:
		pr_info("[BAT_MANAGER] Charger Connected\n");
		di->is_low_batt_alarm = false;
		break;
	case USB_EVENT_CHARGER_NONE:
		pr_info("[BAT_MANAGER] Charger Disconnect\n");
		break;
	default:
		return NOTIFY_OK;
	}

	wake_lock(&di->work_wake_lock);
	schedule_work(&di->charger_work);
	return NOTIFY_OK;
}

enum {
	BATT_VOL = 0,
	BATT_VOL_ADC,
	BATT_TEMP,
	BATT_TEMP_ADC,
	BATT_CHARGING_SOURCE,
	BATT_FG_SOC,
	BATT_RESET_SOC,
	BATT_TEMP_CHECK,
	BATT_FULL_CHECK,
	BATT_TYPE,
	BATT_CHARGE_MODE,
	BATT_FG_AVG_CURRENT,
};

static ssize_t battery_manager_show_attrs(struct device *dev,
		struct device_attribute *attr, char *buf);
static ssize_t battery_manager_store_attrs(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count);

#define SEC_BATTERY_ATTR(_name)						\
{									\
	.attr = {.name = #_name, .mode = 0664, /*.owner = THIS_MODULE*/ },\
	.show = battery_manager_show_attrs,				\
	.store = battery_manager_store_attrs,				\
}

static struct device_attribute battery_manager_attrs[] = {
	SEC_BATTERY_ATTR(batt_vol),
	SEC_BATTERY_ATTR(batt_vol_adc),
	SEC_BATTERY_ATTR(batt_temp),
	SEC_BATTERY_ATTR(batt_temp_adc),
	SEC_BATTERY_ATTR(charging_source),
	SEC_BATTERY_ATTR(fg_read_soc),
	SEC_BATTERY_ATTR(batt_reset_soc),
	SEC_BATTERY_ATTR(batt_temp_check),
	SEC_BATTERY_ATTR(batt_full_check),
	SEC_BATTERY_ATTR(batt_type),
	SEC_BATTERY_ATTR(batt_lp_charging),
	SEC_BATTERY_ATTR(current_avg),
};

static ssize_t battery_manager_show_attrs(struct device *dev,
		struct device_attribute *attr, char *buf)
{

	struct power_supply *psy = dev_get_drvdata(dev);
	struct charger_device_info *di
		= container_of(psy, struct charger_device_info, psy_bat);

	int i = 0;
	const ptrdiff_t off = attr - battery_manager_attrs;

	switch (off) {
	case BATT_VOL:
		if (di->pdata->get_fuel_value)
			i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
				di->pdata->get_fuel_value(READ_FG_VCELL));
		break;
	case BATT_TEMP:
		if (di->pdata->get_fuel_value)
			i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
				di->pdata->get_fuel_value(READ_FG_TEMP));
		break;
	case BATT_FG_SOC:
		if (di->pdata->get_fuel_value)
			i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
				di->pdata->get_fuel_value(READ_FG_SOC));
		break;
	case BATT_FG_AVG_CURRENT:
		if (di->pdata->get_fuel_value)
			i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
				di->pdata->get_fuel_value(READ_FG_AVG_CURRENT));
		break;
	default:
		i = -EINVAL;
	}

	return i;
}
static ssize_t battery_manager_store_attrs(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int x = 0;
	int ret = 0;
	int val = 0;

	struct power_supply *psy = dev_get_drvdata(dev);
	struct charger_device_info *di
		= container_of(psy, struct charger_device_info, psy_bat);

	const ptrdiff_t off = attr - battery_manager_attrs;

	switch (off) {
	case BATT_RESET_SOC:
		val = sscanf(buf, "%d\n", &x);
		if (val == 1 && x == 1 && di->pdata->reset_fuel_soc &&
				di->pdata->get_fuel_value) {
			di->pdata->reset_fuel_soc();
			msleep(200);
			di->bat_info.vcell =
				di->pdata->get_fuel_value(READ_FG_VCELL);
			di->bat_info.avg_current =
				di->pdata->get_fuel_value(READ_FG_AVG_CURRENT);
			di->bat_info.fg_current =
				di->pdata->get_fuel_value(READ_FG_CURRENT);
			di->bat_info.soc =
				di->pdata->get_fuel_value(READ_FG_SOC);
			di->bat_info.temp =
				di->pdata->get_fuel_value(READ_FG_TEMP);
			ret = count;
		}
		break;
	default:
		ret = -EINVAL;
	}

	return ret;
}

static int battery_manager_create_attrs(struct device *dev)
{
	int i, rc;

	for (i = 0; i < ARRAY_SIZE(battery_manager_attrs); i++) {
		rc = device_create_file(dev, &battery_manager_attrs[i]);
		if (rc)
			goto create_attrs_failed;
	}
	goto succeed;

create_attrs_failed:
	while (i--)
		device_remove_file(dev, &battery_manager_attrs[i]);
succeed:
	return rc;
}

static int __devinit batman_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct charger_device_info *di;

	di = kzalloc(sizeof(*di), GFP_KERNEL);
	if (!di)
		return -ENOMEM;

	di->dev = &pdev->dev;
	di->pdata = pdev->dev.platform_data;

	di->psy_bat.name = "battery";
	di->psy_bat.type = POWER_SUPPLY_TYPE_BATTERY;
	di->psy_bat.properties = batman_battery_properties;
	di->psy_bat.num_properties = ARRAY_SIZE(batman_battery_properties);
	di->psy_bat.get_property = batman_bat_get_property;

	di->psy_usb.name = "usb";
	di->psy_usb.type = POWER_SUPPLY_TYPE_USB;
	di->psy_usb.supplied_to = supply_list;
	di->psy_usb.num_supplicants = ARRAY_SIZE(supply_list);
	di->psy_usb.properties = batman_power_properties;
	di->psy_usb.num_properties = ARRAY_SIZE(batman_power_properties);
	di->psy_usb.get_property = batman_usb_get_property;

	di->psy_ac.name = "ac",
	di->psy_ac.type = POWER_SUPPLY_TYPE_MAINS;
	di->psy_ac.supplied_to = supply_list;
	di->psy_ac.num_supplicants = ARRAY_SIZE(supply_list);
	di->psy_ac.properties = batman_power_properties;
	di->psy_ac.num_properties = ARRAY_SIZE(batman_power_properties);
	di->psy_ac.get_property = batman_ac_get_property;

	di->discharge_status = 0;
	di->cable_status = CABLE_TYPE_NONE;
	di->bat_info.health = POWER_SUPPLY_HEALTH_GOOD;

	/* init power supplier framework */
	ret = power_supply_register(&pdev->dev, &di->psy_bat);
	if (ret) {
		pr_err("Failed to register power supply psy_bat\n");
		goto err_supply_reg_bat;
	}
	ret = power_supply_register(&pdev->dev, &di->psy_usb);
	if (ret) {
		pr_err("Failed to register power supply psy_usb\n");
		goto err_supply_reg_usb;
	}
	ret = power_supply_register(&pdev->dev, &di->psy_ac);
	if (ret) {
		pr_err("Failed to register power supply psy_ac\n");
		goto err_supply_reg_ac;
	}

	di->charge_status = (di->pdata->bootmode == 5) ?
			POWER_SUPPLY_STATUS_CHARGING :
			POWER_SUPPLY_STATUS_DISCHARGING ;

	platform_set_drvdata(pdev, di);

	wake_lock_init(&di->work_wake_lock, WAKE_LOCK_SUSPEND,
			"wakelock-charger");
	mutex_init(&di->mutex);
	mutex_init(&di->cable_mutex);

	INIT_WORK(&di->bat_work, battery_manager_work);
	INIT_WORK(&di->charger_work, charger_detect_work);

	di->monitor_wqueue = create_workqueue(dev_name(&pdev->dev));
	if (!di->monitor_wqueue) {
		pr_err("Failed to create freezeable monitor workqueue\n");
		ret = -ENOMEM;
		goto err_wqueue;
	}

	di->transceiver = otg_get_transceiver();
	if (di->transceiver) {
		di->otg_nb.notifier_call = otg_handle_notification;
		ret = otg_register_notifier(di->transceiver, &di->otg_nb);
		if (ret) {
			pr_err("failure to register otg notifier\n");
			goto otg_reg_notifier_failed;
		}
	} else {
		pr_err("failure to otg get transceiver\n");
	}

	ret = battery_manager_create_attrs(di->psy_bat.dev);
	if (ret)
		pr_err("%s : Failed to create_attrs\n", __func__);

	alarm_init(&di->alarm, ANDROID_ALARM_ELAPSED_REALTIME_WAKEUP,
			batman_battery_alarm);

	di->callbacks.set_full_charge = set_full_charge;
	di->callbacks.fuel_alert_lowbat = lowbat_fuel_alert;
	if (di->pdata->register_callbacks)
		di->pdata->register_callbacks(&di->callbacks);

	if (!gpio_get_value(di->pdata->ta_gpio)) {
		if (batman_check_UV_charging(di))
			di->low_batt_boot_flag = true;
	}

	schedule_work(&di->charger_work);
	pr_info("%s: Battery manager probe done\n", __func__);

	return 0;

otg_reg_notifier_failed:
	destroy_workqueue(di->monitor_wqueue);
err_wqueue:
	mutex_destroy(&di->mutex);
	mutex_destroy(&di->cable_mutex);
	wake_lock_destroy(&di->work_wake_lock);
	power_supply_unregister(&di->psy_ac);
err_supply_reg_ac:
	power_supply_unregister(&di->psy_usb);
err_supply_reg_usb:
	power_supply_unregister(&di->psy_bat);
err_supply_reg_bat:
	kfree(di);

	return ret;
}

static int __devexit batman_remove(struct platform_device *pdev)
{
	struct charger_device_info *di = platform_get_drvdata(pdev);

	power_supply_unregister(&di->psy_bat);
	power_supply_unregister(&di->psy_usb);
	power_supply_unregister(&di->psy_ac);
	alarm_cancel(&di->alarm);
	cancel_work_sync(&di->charger_work);
	flush_workqueue(di->monitor_wqueue);
	destroy_workqueue(di->monitor_wqueue);
	if (di->transceiver)
		otg_put_transceiver(di->transceiver);
	wake_lock_destroy(&di->work_wake_lock);
	mutex_destroy(&di->mutex);
	mutex_destroy(&di->cable_mutex);

	return 0;
}

#ifdef CONFIG_PM
static int batman_suspend(struct device *pdev)
{
	struct charger_device_info *di = dev_get_drvdata(pdev);
	if (!di->is_cable_attached) {
		batman_program_alarm(di, SLOW_POLL);
		di->slow_poll = 1;
	}

	return 0;
}

static void batman_resume(struct device *pdev)
{
	struct charger_device_info *di = dev_get_drvdata(pdev);
	if (di->slow_poll) {
		batman_program_alarm(di, FAST_POLL);
		di->slow_poll = 0;
	}
}

#else
#define batman_suspend NULL
#define batman_resume NULL
#endif /* CONFIG_PM */

static const struct dev_pm_ops batman_pm_ops = {
	.prepare = batman_suspend,
	.complete = batman_resume,
};

struct platform_driver battery_manager_driver = {
	.probe = batman_probe,
	.remove = __devexit_p(batman_remove),
	.driver = {
		.name = "battery_manager",
		.pm = &batman_pm_ops,
	},
};

static int __init batman_init(void)
{
	return platform_driver_register(&battery_manager_driver);
}
late_initcall(batman_init);

static void __exit batman_exit(void)
{
	platform_driver_unregister(&battery_manager_driver);
}
module_exit(batman_exit);

MODULE_AUTHOR("HongMin Son<hongmin.son@samsung.com>");
MODULE_LICENSE("GPL");
