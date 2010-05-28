/*
 * linux/drivers/power/twl6030_bci_battery.c
 *
 * OMAP2430/3430 BCI battery driver for Linux
 *
 * Copyright (C) 2008-2009 Texas Instruments, Inc.
 * Author: Texas Instruments, Inc.
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
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/i2c/twl.h>
#include <linux/power_supply.h>
#include <linux/i2c/twl6030-gpadc.h>

#define CONTROLLER_INT_MASK	0x00
#define CONTROLLER_CTRL1	0x01
#define CONTROLLER_WDG		0x02
#define CONTROLLER_STAT1	0x03
#define CHARGERUSB_INT_STATUS	0x04
#define CHARGERUSB_INT_MASK	0x05
#define CHARGERUSB_STATUS_INT1	0x06
#define CHARGERUSB_STATUS_INT2	0x07
#define CHARGERUSB_CTRL1	0x08
#define CHARGERUSB_CTRL2	0x09
#define CHARGERUSB_CTRL3	0x0A
#define CHARGERUSB_STAT1	0x0B
#define CHARGERUSB_VOREG	0x0C
#define CHARGERUSB_VICHRG	0x0D
#define CHARGERUSB_CINLIMIT	0x0E
#define CHARGERUSB_CTRLLIMIT1	0x0F
#define CHARGERUSB_CTRLLIMIT2	0x10

#define REG_FG_REG_00	0x00
#define REG_FG_REG_01	0x01
#define REG_FG_REG_02	0x02
#define REG_FG_REG_03	0x03
#define REG_FG_REG_04	0x04
#define REG_FG_REG_05	0x05
#define REG_FG_REG_06	0x06
#define REG_FG_REG_07	0x07
#define REG_FG_REG_08	0x08
#define REG_FG_REG_09	0x09
#define REG_FG_REG_10	0x0A
#define REG_FG_REG_11	0x0B

/* CONTROLLER_INT_MASK */
#define MVAC_FAULT		(1 << 6)
#define MAC_EOC			(1 << 5)
#define MBAT_REMOVED		(1 << 4)
#define MFAULT_WDG		(1 << 3)
#define MBAT_TEMP		(1 << 2)
#define MVBUS_DET		(1 << 1)
#define MVAC_DET		(1 << 0)

/* CONTROLLER_CTRL1 */
#define CONTROLLER_CTRL1_EN_CHARGER	(1 << 4)
#define CONTROLLER_CTRL1_SEL_CHARGER	(1 << 3)

/* CONTROLLER_STAT1 */
#define CONTROLLER_STAT1_EXTCHRG_STATZ	(1 << 7)
#define CONTROLLER_STAT1_CHRG_DET_N	(1 << 5)
#define CONTROLLER_STAT1_FAULT_WDG	(1 << 4)
#define CONTROLLER_STAT1_VAC_DET	(1 << 3)
#define VAC_DET	(1 << 3)
#define CONTROLLER_STAT1_VBUS_DET	(1 << 2)
#define VBUS_DET	(1 << 2)
#define CONTROLLER_STAT1_BAT_REMOVED	(1 << 1)
#define CONTROLLER_STAT1_BAT_TEMP_OVRANGE (1 << 0)

/* CHARGERUSB_INT_MASK */
#define CURRENT_TERM_INT	(1 << 3)
#define CHARGERUSB_STAT		(1 << 2)
#define CHARGERUSB_THMREG	(1 << 1)
#define CHARGERUSB_FAULT	(1 << 0)

/* CHARGERUSB_INT_STATUS */
#define MASK_MCURRENT_TERM		(1 << 3)
#define MASK_MCHARGERUSB_STAT		(1 << 2)
#define MASK_MCHARGERUSB_THMREG		(1 << 1)
#define MASK_MCHARGERUSB_FAULT		(1 << 0)

/* CHARGERUSB_STATUS_INT1 */
#define CHARGERUSB_STATUS_INT1_TMREG	(1 << 7)
#define CHARGERUSB_STATUS_INT1_NO_BAT	(1 << 6)
#define CHARGERUSB_STATUS_INT1_BST_OCP	(1 << 5)
#define CHARGERUSB_STATUS_INT1_TH_SHUTD	(1 << 4)
#define CHARGERUSB_STATUS_INT1_BAT_OVP	(1 << 3)
#define CHARGERUSB_STATUS_INT1_POOR_SRC	(1 << 2)
#define CHARGERUSB_STATUS_INT1_SLP_MODE	(1 << 1)
#define CHARGERUSB_STATUS_INT1_VBUS_OVP	(1 << 0)

/* CHARGERUSB_STATUS_INT2 */
#define ICCLOOP		(1 << 3)
#define CURRENT_TERM	(1 << 2)
#define CHARGE_DONE	(1 << 1)
#define ANTICOLLAPSE	(1 << 0)

/* CHARGERUSB_CTRL1 */
#define SUSPEND_BOOT	(1 << 7)
#define OPA_MODE	(1 << 6)
#define HZ_MODE		(1 << 5)
#define TERM		(1 << 4)

/* CHARGERUSB_CTRL2 */
#define CHARGERUSB_CTRL2_VITERM_50	(0 << 5)
#define CHARGERUSB_CTRL2_VITERM_100	(1 << 5)
#define CHARGERUSB_CTRL2_VITERM_150	(2 << 5)

/* CHARGERUSB_CTRL3 */
#define VBUSCHRG_LDO_OVRD	(1 << 7)
#define CHARGE_ONCE		(1 << 6)
#define BST_HW_PR_DIS		(1 << 5)
#define AUTOSUPPLY		(1 << 3)
#define BUCK_HSILIM		(1 << 0)

/* CHARGERUSB_VOREG */
#define CHARGERUSB_VOREG_3P52		0x01
#define CHARGERUSB_VOREG_4P0		0x19
#define CHARGERUSB_VOREG_4P2		0x23
#define CHARGERUSB_VOREG_4P76		0x3F

/* CHARGERUSB_VICHRG */
#define CHARGERUSB_VICHRG_300		0x0
#define CHARGERUSB_VICHRG_500		0x4
#define CHARGERUSB_VICHRG_1500		0xE

/* CHARGERUSB_CINLIMIT */
#define CHARGERUSB_CIN_LIMIT_100	0x1
#define CHARGERUSB_CIN_LIMIT_300	0x5
#define CHARGERUSB_CIN_LIMIT_500	0x9
#define CHARGERUSB_CIN_LIMIT_NONE	0xF

/* CHARGERUSB_CTRLLIMIT2 */
#define CHARGERUSB_CTRLLIMIT2_1500	0x0E

#define REG_TOGGLE1		0x90
#define ENABLE_FUELGUAGE	0x20

/* TWL6030_GPADC_CTRL */
#define ENABLE_ISOURCE		0x80

#define REG_MISC1		0xE4
#define VAC_MEAS		0x04
#define VBAT_MEAS		0x02
#define BB_MEAS			0x01

#define REG_USB_VBUS_CTRL_SET	0x04
#define VBUS_MEAS		0x01
#define REG_USB_ID_CTRL_SET	0x06
#define ID_MEAS			0x01

/* BQ24156 */
#define REG_STATUS_CONTROL		0x00
#define REG_CONTROL_REGISTER		0x01
#define REG_CONTROL_BATTERY_VOLTAGE	0x02
#define REG_BATTERY_TERMINATION		0x04
#define REG_SPECIAL_CHARGER_VOLTAGE	0x05
#define REG_SAFETY_LIMIT		0x06


/* Ptr to thermistor table */
int *therm_tbl;
int charger_source;

struct twl6030_bci_device_info {
	struct device		*dev;

	unsigned long		update_time;
	int			voltage_uV;
	int			bk_voltage_uV;
	int			current_uA;
	int			temp_C;
	int			charge_rsoc;
	int			charge_status;
	int			vac_priority;

	struct power_supply	bat;
	struct power_supply	bk_bat;
	struct delayed_work	twl6030_bci_monitor_work;
	struct delayed_work	twl6030_bk_bci_monitor_work;
};

static int charge_state;

static void twl6030_start_usb_charger(void)
{
	charger_source = 2;
	pr_debug("USB charger detected\n");
	twl_i2c_write_u8(TWL6030_MODULE_CHARGER, CHARGERUSB_VICHRG_1500,
					CHARGERUSB_VICHRG);
	twl_i2c_write_u8(TWL6030_MODULE_CHARGER, CHARGERUSB_CIN_LIMIT_NONE,
					CHARGERUSB_CINLIMIT);
	twl_i2c_write_u8(TWL6030_MODULE_CHARGER, MBAT_TEMP,
						CONTROLLER_INT_MASK);
	twl_i2c_write_u8(TWL6030_MODULE_CHARGER,
			MASK_MCHARGERUSB_THMREG, CHARGERUSB_INT_MASK);
	twl_i2c_write_u8(TWL6030_MODULE_CHARGER,
			CHARGERUSB_VOREG_4P2, CHARGERUSB_VOREG);
	twl_i2c_write_u8(TWL6030_MODULE_CHARGER,
		CHARGERUSB_CTRL2_VITERM_100, CHARGERUSB_CTRL2);
	twl_i2c_write_u8(TWL6030_MODULE_CHARGER, TERM, CHARGERUSB_CTRL1);
	twl_i2c_write_u8(TWL6030_MODULE_CHARGER,
			CONTROLLER_CTRL1_EN_CHARGER,
			CONTROLLER_CTRL1);
}

static void twl6030_start_ac_charger(void)
{
	pr_debug("AC charger detected\n");
	charger_source = 1;
	twl_i2c_write_u8(TWL6030_MODULE_BQ, 0xC0,
				REG_STATUS_CONTROL);
	twl_i2c_write_u8(TWL6030_MODULE_BQ, 0xC8,
				REG_CONTROL_REGISTER);
	twl_i2c_write_u8(TWL6030_MODULE_BQ, 0x7C,
				REG_CONTROL_BATTERY_VOLTAGE);
	twl_i2c_write_u8(TWL6030_MODULE_BQ, 0x51,
				REG_BATTERY_TERMINATION);
	twl_i2c_write_u8(TWL6030_MODULE_BQ, 0x02,
				REG_SPECIAL_CHARGER_VOLTAGE);
	twl_i2c_write_u8(TWL6030_MODULE_CHARGER,
			CONTROLLER_CTRL1_EN_CHARGER |
			CONTROLLER_CTRL1_SEL_CHARGER,
			CONTROLLER_CTRL1);
}
/*
 * Interrupt service routine
 *
 * Attends to TWL 6030 power module interruptions events, specifically
 * USB_PRES (USB charger presence) CHG_PRES (AC charger presence) events
 *
 */
static irqreturn_t twl6030charger_ctrl_interrupt(int irq, void *_di)
{
	struct twl6030_bci_device_info *di = _di;
	int ret;
	u8 stat_toggle, stat_reset, stat_set = 0;
	u8 present_charge_state;
	u8 ac_or_vbus, no_ac_and_vbus;

#ifdef CONFIG_LOCKDEP
	/* WORKAROUND for lockdep forcing IRQF_DISABLED on us, which
	 * we don't want and can't tolerate.  Although it might be
	 * friendlier not to borrow this thread context...
	 */
	local_irq_enable();
#endif

	/* read charger controller_stat1 */
	ret = twl_i2c_read_u8(TWL6030_MODULE_CHARGER, &present_charge_state,
		CONTROLLER_STAT1);
	if (ret)
		return IRQ_NONE;

	stat_toggle = charge_state ^ present_charge_state;
	stat_set = stat_toggle & present_charge_state;
	stat_reset = stat_toggle & charge_state;

	no_ac_and_vbus = !((present_charge_state) & (VBUS_DET | VAC_DET));
	ac_or_vbus = charge_state & (VBUS_DET | VAC_DET);
	if (no_ac_and_vbus && ac_or_vbus) {
		pr_debug("No Charging source\n");
		/* disable charging when no source present */
	}

	charge_state = present_charge_state;

	if (stat_reset & VBUS_DET) {
		pr_debug("usb removed\n");
		if (present_charge_state & VAC_DET)
			twl6030_start_ac_charger();

	}
	if (stat_set & VBUS_DET) {
		if ((present_charge_state & VAC_DET) && (di->vac_priority))
			pr_debug("USB charger detected, continue with VAC\n");
		else
			twl6030_start_usb_charger();
	}

	if (stat_reset & VAC_DET) {
		pr_debug("vac removed\n");
		if (present_charge_state & VBUS_DET)
			twl6030_start_usb_charger();
	}
	if (stat_set & VAC_DET) {
		if ((present_charge_state & VBUS_DET) && !(di->vac_priority))
			pr_debug("AC charger detected, continue with VBUS\n");
		else
			twl6030_start_ac_charger();
	}


	if (stat_set & CONTROLLER_STAT1_FAULT_WDG)
		pr_debug("Fault watchdog fired\n");
	if (stat_reset & CONTROLLER_STAT1_FAULT_WDG)
		pr_debug("Fault watchdog recovered\n");
	if (stat_set & CONTROLLER_STAT1_BAT_REMOVED)
		pr_debug("Battery removed\n");
	if (stat_reset & CONTROLLER_STAT1_BAT_REMOVED)
		pr_debug("Battery inserted\n");
	if (stat_set & CONTROLLER_STAT1_BAT_TEMP_OVRANGE)
		pr_debug("Battery temperature overrange\n");
	if (stat_reset & CONTROLLER_STAT1_BAT_TEMP_OVRANGE)
		pr_debug("Battery temperature within range\n");

#if 0
	power_supply_changed(&di->bat);
#endif

	return IRQ_HANDLED;
}

static irqreturn_t twl6030charger_fault_interrupt(int irq, void *_di)
{
	int ret;

	u8 usb_charge_sts, usb_charge_sts1, usb_charge_sts2;

	ret = twl_i2c_read_u8(TWL6030_MODULE_CHARGER, &usb_charge_sts,
						CHARGERUSB_INT_STATUS);
	ret = twl_i2c_read_u8(TWL6030_MODULE_CHARGER, &usb_charge_sts1,
						CHARGERUSB_STATUS_INT1);
	ret = twl_i2c_read_u8(TWL6030_MODULE_CHARGER, &usb_charge_sts2,
						CHARGERUSB_STATUS_INT2);
	if (usb_charge_sts2 & CHARGE_DONE)
		pr_debug("USB charge done\n");

	pr_debug("Charger fault detected STS, INT1, INT2 %x %x %x\n",
	    usb_charge_sts, usb_charge_sts1, usb_charge_sts2);
	return IRQ_HANDLED;
}

/*
 * Return battery temperature
 * Or < 0 on failure.
 */
static int twl6030battery_temperature(void)
{
	/* FIXME : TBD */
	return 0;
}

/*
 * Return battery voltage
 * Or < 0 on failure.
 */
static int twl6030battery_voltage(void)
{
	struct twl6030_gpadc_request req;
	int temp, temp2;

	req.channels = (1 << 7) | (1 << 10);
	req.method = TWL6030_GPADC_SW1;
	req.active = 0;
	req.func_cb = NULL;
	twl6030_gpadc_conversion(&req);
	temp = (u16)req.rbuf[7] * 25/4; /* multiply by 6.25 to convert to mV */

	if (charger_source == 2)
		temp2 = (u16)req.rbuf[10];
		/* FIXME only for USB charging current*/

	return  temp;

}

/*
 * Return the battery current
 * Or < 0 on failure.
 */
static int twl6030battery_current(void)
{
	int temp, ret;
	u8 read_value1, read_value2;

	ret = twl_i2c_read_u8(TWL6030_MODULE_GASGAUGE, &read_value1,
							REG_FG_REG_10);
	ret = twl_i2c_read_u8(TWL6030_MODULE_GASGAUGE, &read_value2,
							REG_FG_REG_11);
	temp = (read_value2 << 8) + read_value1;
	if (temp >= 0x2000)
		temp = -(0x3fff - temp); /* 10bit -ive number to 16 bit +ive */

	return temp;

}

/*
 * Return the battery backup voltage
 * Or < 0 on failure.
 */
static int twl6030backupbatt_voltage(void)
{
	/* FIXME : TBD */
	return 0;
}

/*
 * Returns the main charge FSM status
 * Or < 0 on failure.
 */
static int twl6030bci_status(void)
{
	int ret;
	u8 status;

	ret = twl_i2c_read_u8(TWL6030_MODULE_CHARGER, &status,
						CONTROLLER_CTRL1);
	if (ret) {
		pr_err("twl6030_bci: error reading CONTROLLER_CTRL1\n");
		return ret;
	}

	return (int) (status);
}


/*
 * Setup the twl6030 BCI module to enable backup
 * battery charging.
 */
static int twl6030backupbatt_voltage_setup(void)
{
	/* FIXME : TBD */
	return 0;
}

/*
 * Setup the twl6030 BCI module to measure battery
 * temperature
 */
static int twl6030battery_temp_setup(void)
{
	/* FIXME : TBD */
	return 0;
}

static enum power_supply_property twl6030_bci_battery_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_TEMP,
};

static enum power_supply_property twl6030_bk_bci_battery_props[] = {
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
};

static void
twl6030_bk_bci_battery_read_status(struct twl6030_bci_device_info *di)
{
	twl6030backupbatt_voltage_setup();
	di->bk_voltage_uV = twl6030backupbatt_voltage();
}

static void twl6030_bk_bci_battery_work(struct work_struct *work)
{
	struct twl6030_bci_device_info *di = container_of(work,
		struct twl6030_bci_device_info,
		twl6030_bk_bci_monitor_work.work);

	twl6030_bk_bci_battery_read_status(di);
	schedule_delayed_work(&di->twl6030_bk_bci_monitor_work, 5000);
}

static void twl6030_bci_battery_read_status(struct twl6030_bci_device_info *di)
{
	di->temp_C = twl6030battery_temperature();
	di->voltage_uV = twl6030battery_voltage();
	di->current_uA = twl6030battery_current();
}

static void
twl6030_bci_battery_update_status(struct twl6030_bci_device_info *di)
{
	if (charger_source == 1) {
		/* reconfig params for ac charging & reset 32 second timer */
		twl_i2c_write_u8(TWL6030_MODULE_BQ, 0xC0, REG_STATUS_CONTROL);
		twl_i2c_write_u8(TWL6030_MODULE_BQ, 0xC8, REG_CONTROL_REGISTER);
		twl_i2c_write_u8(TWL6030_MODULE_BQ, 0x7C,
						REG_CONTROL_BATTERY_VOLTAGE);
		twl_i2c_write_u8(TWL6030_MODULE_BQ, 0x51,
						REG_BATTERY_TERMINATION);
		twl_i2c_write_u8(TWL6030_MODULE_BQ, 0x02,
						REG_SPECIAL_CHARGER_VOLTAGE);
	}
	twl6030_bci_battery_read_status(di);
	di->charge_status = POWER_SUPPLY_STATUS_UNKNOWN;

	if (power_supply_am_i_supplied(&di->bat))
		di->charge_status = POWER_SUPPLY_STATUS_CHARGING;
	else
		di->charge_status = POWER_SUPPLY_STATUS_DISCHARGING;
}

static void twl6030_bci_battery_work(struct work_struct *work)
{
	struct twl6030_bci_device_info *di = container_of(work,
		struct twl6030_bci_device_info, twl6030_bci_monitor_work.work);

	twl6030_bci_battery_update_status(di);
	schedule_delayed_work(&di->twl6030_bci_monitor_work, 1000);
}


#define to_twl6030_bci_device_info(x) container_of((x), \
			struct twl6030_bci_device_info, bat);

static void twl6030_bci_battery_external_power_changed(struct power_supply *psy)
{
	struct twl6030_bci_device_info *di = to_twl6030_bci_device_info(psy);

	cancel_delayed_work(&di->twl6030_bci_monitor_work);
	schedule_delayed_work(&di->twl6030_bci_monitor_work, 0);
}

#define to_twl6030_bk_bci_device_info(x) container_of((x), \
		struct twl6030_bci_device_info, bk_bat);

static int twl6030_bk_bci_battery_get_property(struct power_supply *psy,
					enum power_supply_property psp,
					union power_supply_propval *val)
{
	struct twl6030_bci_device_info *di = to_twl6030_bk_bci_device_info(psy);

	switch (psp) {
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		val->intval = di->bk_voltage_uV;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int twl6030_bci_battery_get_property(struct power_supply *psy,
					enum power_supply_property psp,
					union power_supply_propval *val)
{
	struct twl6030_bci_device_info *di;
	int status = 0;

	di = to_twl6030_bci_device_info(psy);

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		val->intval = di->charge_status;
		return 0;
	default:
		break;
	}

	switch (psp) {
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		val->intval = di->voltage_uV;
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		val->intval = di->current_uA;
		break;
	case POWER_SUPPLY_PROP_TEMP:
		val->intval = di->temp_C;
		break;
	case POWER_SUPPLY_PROP_ONLINE:
		status = twl6030bci_status();
		if ((status & CONTROLLER_CTRL1_EN_CHARGER)) {
			if ((status & CONTROLLER_CTRL1_SEL_CHARGER))
				val->intval = POWER_SUPPLY_TYPE_MAINS;
			else
				val->intval = POWER_SUPPLY_TYPE_USB;
		} else
			val->intval = 0;
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		/* FIXME correct the threshold
		 * need to get the correct percentage value per the
		 * battery characteristics. Approx values for now.
		 */
		if (di->voltage_uV < 3594)
			val->intval = 5;
		else if (di->voltage_uV < 3651 && di->voltage_uV > 3594)
			val->intval = 20;
		else if (di->voltage_uV < 3702 && di->voltage_uV > 3651)
			val->intval = 50;
		else if (di->voltage_uV < 3900 && di->voltage_uV > 3702)
			val->intval = 75;
		else if (di->voltage_uV > 3900)
			val->intval = 90;
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static char *twl6030_bci_supplied_to[] = {
	"twl6030_bci_battery",
};

static int __init twl6030_bci_battery_probe(struct platform_device *pdev)
{
	struct twl6030_bci_platform_data *pdata = pdev->dev.platform_data;
	struct twl6030_bci_device_info *di;
	int irq;
	int ret;
	u8 rd_reg, controller_stat = 0;

	di = kzalloc(sizeof(*di), GFP_KERNEL);
	if (!di)
		return -ENOMEM;

	if (!pdata) {
		dev_dbg(&pdev->dev, "platform_data not available\n");
		ret = -EINVAL;
		goto err_pdata;
	}

	di->dev = &pdev->dev;
	di->bat.name = "twl6030_bci_battery";
	di->bat.supplied_to = twl6030_bci_supplied_to;
	di->bat.num_supplicants = ARRAY_SIZE(twl6030_bci_supplied_to);
	di->bat.type = POWER_SUPPLY_TYPE_BATTERY;
	di->bat.properties = twl6030_bci_battery_props;
	di->bat.num_properties = ARRAY_SIZE(twl6030_bci_battery_props);
	di->bat.get_property = twl6030_bci_battery_get_property;
	di->bat.external_power_changed =
			twl6030_bci_battery_external_power_changed;

	di->charge_status = POWER_SUPPLY_STATUS_UNKNOWN;

	di->bk_bat.name = "twl6030_bci_bk_battery";
	di->bk_bat.type = POWER_SUPPLY_TYPE_BATTERY;
	di->bk_bat.properties = twl6030_bk_bci_battery_props;
	di->bk_bat.num_properties = ARRAY_SIZE(twl6030_bk_bci_battery_props);
	di->bk_bat.get_property = twl6030_bk_bci_battery_get_property;
	di->bk_bat.external_power_changed = NULL;

	di->vac_priority = 1;
	platform_set_drvdata(pdev, di);

	/* settings for temperature sensing */
	ret = twl6030battery_temp_setup();
	if (ret)
		goto temp_setup_fail;

	/* request charger fault interruption */
	irq = platform_get_irq(pdev, 1);
	ret = request_irq(irq, twl6030charger_fault_interrupt,
		0, "twl_bci_fault", di);
	if (ret) {
		dev_dbg(&pdev->dev, "could not request irq %d, status %d\n",
			irq, ret);
		goto batt_irq_fail;
	}

	/* request charger ctrl interruption */
	irq = platform_get_irq(pdev, 0);
	ret = request_irq(irq, twl6030charger_ctrl_interrupt,
		0, "twl_bci_ctrl", di);

	if (ret) {
		dev_dbg(&pdev->dev, "could not request irq %d, status %d\n",
			irq, ret);
		goto chg_irq_fail;
	}

	twl6030_interrupt_unmask(TWL6030_CHARGER_CTRL_INT_MASK,
						REG_INT_MSK_LINE_C);
	twl6030_interrupt_unmask(TWL6030_CHARGER_CTRL_INT_MASK,
						REG_INT_MSK_STS_C);
	twl6030_interrupt_unmask(TWL6030_CHARGER_FAULT_INT_MASK,
						REG_INT_MSK_LINE_C);
	twl6030_interrupt_unmask(TWL6030_CHARGER_FAULT_INT_MASK,
						REG_INT_MSK_STS_C);

	ret = power_supply_register(&pdev->dev, &di->bat);
	if (ret) {
		dev_dbg(&pdev->dev, "failed to register main battery\n");
		goto batt_failed;
	}

	INIT_DELAYED_WORK_DEFERRABLE(&di->twl6030_bci_monitor_work,
				twl6030_bci_battery_work);
	schedule_delayed_work(&di->twl6030_bci_monitor_work, 0);

	ret = power_supply_register(&pdev->dev, &di->bk_bat);
	if (ret) {
		dev_dbg(&pdev->dev, "failed to register backup battery\n");
		goto bk_batt_failed;
	}

	INIT_DELAYED_WORK_DEFERRABLE(&di->twl6030_bk_bci_monitor_work,
				twl6030_bk_bci_battery_work);
	schedule_delayed_work(&di->twl6030_bk_bci_monitor_work, 500);

	twl_i2c_read_u8(TWL6030_MODULE_ID0, &rd_reg, REG_MISC1);
	rd_reg = rd_reg | VAC_MEAS | VBAT_MEAS | BB_MEAS;
	twl_i2c_write_u8(TWL6030_MODULE_ID0, rd_reg, REG_MISC1);

	twl_i2c_read_u8(TWL6030_MODULE_ID1, &rd_reg, REG_TOGGLE1);
	rd_reg = rd_reg | ENABLE_FUELGUAGE;
	twl_i2c_write_u8(TWL6030_MODULE_ID1, rd_reg, REG_TOGGLE1);

	twl_i2c_read_u8(TWL_MODULE_MADC, &rd_reg, TWL6030_GPADC_CTRL);
	rd_reg = rd_reg | ENABLE_ISOURCE;
	twl_i2c_write_u8(TWL_MODULE_MADC, rd_reg, TWL6030_GPADC_CTRL);

	twl_i2c_read_u8(TWL_MODULE_USB, &rd_reg, REG_USB_VBUS_CTRL_SET);
	rd_reg = rd_reg | VBUS_MEAS;
	twl_i2c_write_u8(TWL_MODULE_USB, rd_reg, REG_USB_VBUS_CTRL_SET);

	twl_i2c_read_u8(TWL_MODULE_USB, &rd_reg, REG_USB_ID_CTRL_SET);
	rd_reg = rd_reg | ID_MEAS;
	twl_i2c_write_u8(TWL_MODULE_USB, rd_reg, REG_USB_ID_CTRL_SET);

	/* initialize for USB charging */
	twl_i2c_write_u8(TWL6030_MODULE_CHARGER, MBAT_TEMP,
						CONTROLLER_INT_MASK);
	twl_i2c_write_u8(TWL6030_MODULE_CHARGER, MASK_MCHARGERUSB_THMREG,
						CHARGERUSB_INT_MASK);
	twl_i2c_write_u8(TWL6030_MODULE_CHARGER, CHARGERUSB_VOREG_4P2,
						CHARGERUSB_VOREG);
	twl_i2c_write_u8(TWL6030_MODULE_CHARGER, CHARGERUSB_VICHRG_1500,
						CHARGERUSB_VICHRG);
	twl_i2c_write_u8(TWL6030_MODULE_CHARGER, CHARGERUSB_CTRL2_VITERM_100,
						CHARGERUSB_CTRL2);
	twl_i2c_write_u8(TWL6030_MODULE_CHARGER, CHARGERUSB_CIN_LIMIT_NONE,
						CHARGERUSB_CINLIMIT);
	twl_i2c_write_u8(TWL6030_MODULE_CHARGER, CHARGERUSB_CTRLLIMIT2_1500,
					CHARGERUSB_CTRLLIMIT2);

	twl_i2c_write_u8(TWL6030_MODULE_BQ, 0xa0, REG_SAFETY_LIMIT);

	twl_i2c_read_u8(TWL6030_MODULE_CHARGER, &controller_stat,
		CONTROLLER_STAT1);

	if (controller_stat & VBUS_DET)
		twl6030_start_usb_charger();

	if (controller_stat & VAC_DET)
		twl6030_start_ac_charger();

	return 0;

bk_batt_failed:
	power_supply_unregister(&di->bat);
batt_failed:
	free_irq(irq, di);
chg_irq_fail:
	irq = platform_get_irq(pdev, 1);
	free_irq(irq, NULL);
err_pdata:
batt_irq_fail:
temp_setup_fail:
	kfree(di);

	return ret;
}

static int __exit twl6030_bci_battery_remove(struct platform_device *pdev)
{
	struct twl6030_bci_device_info *di = platform_get_drvdata(pdev);
	int irq;

	twl6030_interrupt_mask(TWL6030_CHARGER_CTRL_INT_MASK,
						REG_INT_MSK_LINE_C);
	twl6030_interrupt_mask(TWL6030_CHARGER_CTRL_INT_MASK,
						REG_INT_MSK_STS_C);
	twl6030_interrupt_mask(TWL6030_CHARGER_FAULT_INT_MASK,
						REG_INT_MSK_LINE_C);
	twl6030_interrupt_mask(TWL6030_CHARGER_FAULT_INT_MASK,
						REG_INT_MSK_STS_C);

	irq = platform_get_irq(pdev, 0);
	free_irq(irq, di);

	irq = platform_get_irq(pdev, 1);
	free_irq(irq, di);

	cancel_delayed_work(&di->twl6030_bci_monitor_work);
	cancel_delayed_work(&di->twl6030_bk_bci_monitor_work);
	flush_scheduled_work();
	power_supply_unregister(&di->bat);
	power_supply_unregister(&di->bk_bat);
	platform_set_drvdata(pdev, NULL);
	kfree(di);

	return 0;
}

#ifdef CONFIG_PM
static int twl6030_bci_battery_suspend(struct platform_device *pdev,
	pm_message_t state)
{
	struct twl6030_bci_device_info *di = platform_get_drvdata(pdev);

	di->charge_status = POWER_SUPPLY_STATUS_UNKNOWN;
	cancel_delayed_work(&di->twl6030_bci_monitor_work);
	cancel_delayed_work(&di->twl6030_bk_bci_monitor_work);
	return 0;
}

static int twl6030_bci_battery_resume(struct platform_device *pdev)
{
	struct twl6030_bci_device_info *di = platform_get_drvdata(pdev);

	schedule_delayed_work(&di->twl6030_bci_monitor_work, 0);
	schedule_delayed_work(&di->twl6030_bk_bci_monitor_work, 50);
	return 0;
}
#else
#define twl6030_bci_battery_suspend	NULL
#define twl6030_bci_battery_resume	NULL
#endif /* CONFIG_PM */

static struct platform_driver twl6030_bci_battery_driver = {
	.probe		= twl6030_bci_battery_probe,
	.remove		= __exit_p(twl6030_bci_battery_remove),
	.suspend	= twl6030_bci_battery_suspend,
	.resume		= twl6030_bci_battery_resume,
	.driver		= {
		.name	= "twl6030_bci",
	},
};

MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:twl6030_bci");
MODULE_AUTHOR("Texas Instruments Inc");

static int __init twl6030_battery_init(void)
{
	return platform_driver_register(&twl6030_bci_battery_driver);
}
module_init(twl6030_battery_init);

static void __exit twl6030_battery_exit(void)
{
	platform_driver_unregister(&twl6030_bci_battery_driver);
}
module_exit(twl6030_battery_exit);

