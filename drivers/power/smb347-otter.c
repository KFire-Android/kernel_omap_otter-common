/*
 * Charger driver for Summit SMB347
 *
 * Copyright (C) Quanta Computer Inc. All rights reserved.
 *  Eric Nien <Eric.Nien@quantatw.com>
 *
 * Edits/Additions by Hashcode: 2013-05-21
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/param.h>
#include <linux/gpio.h>
#include <linux/slab.h>
#include <linux/irq.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/thermal_framework.h>
#include <linux/usb/omap_usb.h>
#include <linux/usb/musb-omap.h>
#include <linux/power_supply.h>
#include <linux/i2c/twl.h>

#include <asm/io.h>

#include <plat/irqs.h>
#include <plat/usb.h>

// Platform Data
#include <linux/power/smb347-otter.h>

#define DRIVER_VERSION			"2.0"
#define DRIVER_DEBUG			1

#define CONTROLLER_STAT1		0x03
#define VBUS_DET			(1 << 2)

/* I2C chip addresses */
#define SMB347_I2C_ADDRESS		0x5F
#define SMB347_I2C_ADDRESS_SECONDARY	0x06

/* Bit operator */
#define IS_BIT_SET(reg, x)		(((reg)&(0x1<<(x)))!=0)
#define IS_BIT_CLEAR(reg, x)		(((reg)&(0x1<<(x)))==0)
#define CLEAR_BIT(reg, x)		((reg)&=(~(0x1<<(x))))
#define SET_BIT(reg, x)			((reg)|=(0x1<<(x)))

#define SET_BAT_NO_SUIT_CURRENT(X)	SET_BIT(X,8)
#define CLEAR_BAT_NO_SUIT_CURRENT(X)	CLEAR_BIT(X,8)
#define IS_BAT_NO_SUIT_CURRENT(X)	IS_BIT_SET(X,8)

#define SET_BAT_TOO_WEAK(X)		SET_BIT(X,9)
#define CLEAR_BAT_TOO_WEAK(X)		CLEAR_BIT(X,9)
#define IS_BAT_TOO_WEAK(X)		IS_BIT_SET(X,9)

/* REGISTERS */
/* R0 Charge Current */
#define SMB347_CHARGE_CURRENT		0x00
/* R1 Input Current Limits */
#define SMB347_INPUT_CURR_LIMIT		0x01
/* R2 Functions */
#define SMB347_FUNCTIONS		0x02
/* R3 Float Voltage */
#define SMB347_FLOAT_VOLTAGE		0x03
/* R4 Charger Control */
#define SMB347_CHARGE_CONTROL		0x04
/* R4 [2] Automatic Power Source Detection */
#define APSD_ENABLE				(2)
/* R5 Charger Control */
#define SMB347_STAT_TIMERS		0x05
/* R6 Enable Control */
#define SMB347_ENABLE_CONTROL		0x06
/* R4 [2] Control Send IRQ on Error */
#define CHARGER_ERROR_IRQ          		(2)
/* R7 Thermal Control */
#define SMB347_THERMAL_CONTROL		0x07
#define SMB347_SYSOK_USB30		0x08
#define SMB347_OTHER_CONTROL_A		0x09
#define SMB347_OTG_THERM_CONTROL	0x0A
#define SMB347_CELL_TEMP		0x0B
#define SMB347_FAULT_INTERRUPT		0x0C
#define SMB347_INTERRUPT_STAT		0x0D
#define SMB347_SLAVE_ADDR		0x0E

/* Command register */
#define SMB347_COMMAND_REG_A		0x30
#define SMB347_COMMAND_REG_B		0x31
#define SMB347_COMMAND_REG_C		0x33
#define SMB347_INTSTAT_REG_A		0x35
#define SMB347_INTSTAT_REG_B		0x36
#define SMB347_INTSTAT_REG_C		0x37
#define SMB347_INTSTAT_REG_D		0x38
#define SMB347_INTSTAT_REG_E		0x39
#define SMB347_INTSTAT_REG_F		0x3A
#define SMB347_STATUS_REG_A		0x3B
#define SMB347_STATUS_REG_B		0x3C
#define SMB347_STATUS_REG_C		0x3D
#define SMB347_STATUS_REG_D		0x3E
#define SMB347_STATUS_REG_E		0x3F

/* RC FAULT Interrupt Register */
#define TEMP_OUTSIDE_COLD_HOT_HARD_LIMIITS_TRIGGER_IRQ	(1<<7)
#define TEMP_OUTSIDE_COLD_HOT_SOFT_LIMIITS_TRIGGER_IRQ	(1<<6)
#define OTG_BAT_FAIL_ULVO_TRIGGER_IRQ			(1<<5)
#define OTG_OVER_CURRENT_LIMIT_TRIGGER_IRQ		(1<<4)
#define USB_OVER_VOLTAGE_TRIGGER_IRQ			(1<<3)
#define USB_UNDER_VOLTAGE_TRIGGER_IRQ			(1<<2)
#define AICL_COMPLETE_TRIGGER_IRQ			(1<<1)
#define INTERNAL_OVER_TEMP_TRIGGER_IRQ			(1)

/* RD STATUS Interrupt Register */
#define CHARGE_TIMEOUT_TRIGGER_IRQ			(1<<7)
#define OTG_INSERTED_REMOVED_TRIGGER_IRQ		(1<<6)
#define BAT_OVER_VOLTAGE_TRIGGER_IRQ			(1<<5)
#define TERMINATION_OR_TAPER_CHARGING_TRIGGER_IRQ	(1<<4)
#define FAST_CHARGING_TRIGGER_IRQ			(1<<3)
#define INOK_TRIGGER_IRQ				(1<<2)
#define MISSING_BATTERY_TRIGGER_IRQ			(1<<1)
#define LOW_BATTERY_TRIGGER_IRQ				(1)

/* Interrupt Status Register A */
#define HOT_TEMP_HARD_LIMIT_IRQ		(1<<7)
#define HOT_TEMP_HARD_LIMIT_STATUS	(1<<6)
#define COLD_TEMP_HARD_LIMIT_IRQ	(1<<5)
#define COLD_TEMP_HARD_LIMIT_STATUS	(1<<4)
#define HOT_TEMP_SOFT_LIMIT_IRQ		(1<<3)
#define HOT_TEMP_SOFT_LIMIT_STATUS	(1<<2)
#define COLD_TEMP_SOFT_LIMIT_IRQ	(1<<1)
#define COLD_TEMP_SOFT_LIMIT_STATUS	(1)

/* Interrupt Status Register B */
#define BAT_OVER_VOLTAGE_IRQ		(1<<7)
#define BAT_OVER_VOLTAGE_STATUS		(1<<6)
#define MISSING_BAT_IRQ			(1<<5)
#define MISSING_BAT_STATUS		(1<<4)
#define LOW_BAT_VOLTAGE_IRQ		(1<<3)
#define LOW_BAT_VOLTAGE_STATUS		(1<<2)
#define PRE_TO_FAST_CHARGE_VOLTAGE_IRQ	(1<<1)
#define PRE_TO_FAST_CHARGE_VOLTAGE_STATUS (1)

/* Interrupt Status Register C */
#define INTERNAL_TEMP_LIMIT_IRQ		(1<<7)
#define INTERNAL_TEMP_LIMIT_STATUS	(1<<6)
#define RE_CHARGE_BAT_THRESHOLD_IRQ	(1<<5)
#define RE_CHARGE_BAT_THRESHOLD_STATUS	(1<<4)
#define TAPER_CHARGER_MODE_IRQ		(1<<3)
#define TAPER_CHARGER_MODE_STATUS	(1<<2)
#define TERMINATION_CC_IRQ		(1<<1)
#define TERMINATION_CC_STATUS		(1)

/* Interrupt Status Register D */
#define APSD_COMPLETED_IRQ		(1<<7)
#define APSD_COMPLETED_STATUS		(1<<6)
#define AICL_COMPLETED_IRQ		(1<<5)
#define AICL_COMPLETED_STATUS		(1<<4)
#define COMPLETE_CHARGE_TIMEOUT_IRQ	(1<<3)
#define COMPLETE_CHARGE_TIMEOUT_STATUS	(1<<2)
#define PC_TIMEOUT_IRQ			(1<<1)
#define PC_TIMEOUT_STATUS		(1)

#define SMB347_IS_APSD_DONE(value)	((value) & (1<<3))
#define SMB347_APSD_RESULT(value)	((value) & 0x7)
#define SMB347_APSD_RESULT_NONE		(0)
#define SMB347_APSD_RESULT_CDP		(1)
#define SMB347_APSD_RESULT_DCP		(2)
#define SMB347_APSD_RESULT_OTHER	(3)
#define SMB347_APSD_RESULT_SDP		(4)
#define SMB347_APSD_RESULT_ACA		(5)
#define SMB347_APSD_RESULT_TBD_1	(6)
#define SMB347_APSD_RESULT_TBD_2	(7)

#define SMB347_IS_AICL_DONE(value)	((value) & (1<<4))
#define SMB347_AICL_RESULT(value)	((value) & 0xf)

/* Interrupt Status Register E */
#define DCIN_OVER_VOLTAGE_IRQ		(1<<7)
#define DCIN_OVER_VOLTAGE_STATUS	(1<<6)
#define DCIN_UNDER_VOLTAGE_IRQ		(1<<5)
#define DCIN_UNDER_VOLTAGE_STATUS	(1<<4)
#define USBIN_OVER_VOLTAGE_IRQ		(1<<3)
#define USBIN_OVER_VOLTAGE_STATUS	(1<<2)
#define USBIN_UNDER_VOLTAGE_IRQ		(1<<1)
#define USBIN_UNDER_VOLTAGE_STATUS	(1)

/* Interrupt Status Register F */
#define OTG_OVER_CURRENT_IRQ		(1<<7)
#define OTG_OVER_CURRENT_STATUS		(1<<6)
#define OTG_BAT_UNDER_VOLTAGE_IRQ	(1<<5)
#define OTG_BAT_UNDER_VOLTAGE_STATUS	(1<<4)
#define OTG_DETECTION_IRQ		(1<<3)
#define OTG_DETECTION_STATUS		(1<<2)
#define POWER_OK_IRQ			(1<<1)
#define POWER_OK_STATUS			(1)

#define LOW_BATTERY_TRIGGER_IRQ	(1)

/* TWL6030 defines */
#define USB_ID_INT_EN_HI_SET		0x14
#define USB_ID_INT_EN_HI_CLR		0x15


struct summit_smb347_info {
	struct i2c_client	*client;
	struct device		*dev;
	struct summit_smb347_platform_data	*pdata;

	u8			id;
	atomic_t		usb_online;
	atomic_t		ac_online;
	u16			protect_event;  	//Let the driver level to do the protection
	u16			protect_enable;		//Let the driver level to do the protection
	int			bat_thermal_step;
	int			irq;
	void __iomem		*usb_phy;
	int			thermal_adjust_mode;

	int			max_aicl;
	int			pre_max_aicl;
	int			charge_current;
	int			charge_current_redunction;
	unsigned long		usb_max_power;

	struct power_supply	usb;
	struct power_supply	ac;

	struct usb_phy		*otg_xceiv;

	struct work_struct	irq_work;
	struct delayed_work	twl6030_work;
#if defined(CONFIG_HAS_WAKELOCK)
	struct wake_lock	usb_wake_lock;
#endif
	struct mutex		lock;

#ifdef CONFIG_THERMAL_FRAMEWORK
	struct thermal_dev	*tdev;
	int			max_thermal_charge_current;
#endif

	/* USB DATA */
	int			usb_vusb_on;
};

enum usb_charger_events {
	EVENT_CHECKINIT=9,
	EVENT_INTERRUPT_FROM_CHARGER,
	EVENT_CHANGE_TO_ONDEMAND,
	EVENT_CHANGE_TO_INTERNAL_FSM,
	EVENT_DETECT_PC,
	EVENT_DETECT_TBD,
	EVENT_DETECT_OTHER,
	EVENT_OVER_HOT_TEMP_HARD_LIMIT,
	EVENT_BELOW_HOT_TEMP_HARD_LIMIT,
	EVENT_OVER_HOT_TEMP_SOFT_LIMIT,
	EVENT_BELOW_HOT_TEMP_SOFT_LIMIT,
	EVENT_OVER_COLD_TEMP_HARD_LIMIT,
	EVENT_BELOW_COLD_TEMP_HARD_LIMIT,
	EVENT_OVER_COLD_TEMP_SOFT_LIMIT,
	EVENT_BELOW_COLD_TEMP_SOFT_LIMIT,
	EVENT_APSD_NOT_RUNNING,
	EVENT_APSD_COMPLETE,
	EVENT_APSD_NOT_COMPLETE,
	EVENT_AICL_COMPLETE,
	EVENT_APSD_AGAIN,
	EVENT_OVER_LOW_BATTERY,
	EVENT_BELOW_LOW_BATTERY,
	EVENT_RECOGNIZE_BATTERY,
	EVENT_NOT_RECOGNIZE_BATTERY,
	EVENT_UNKNOW_BATTERY,
	EVENT_WEAK_BATTERY,
	EVENT_HEALTHY_BATTERY,    //voltage > 3.4V && capacity > 2%
	EVENT_FULL_BATTERY,       //
	EVENT_RECHARGE_BATTERY,   //
	EVENT_BATTERY_I2C_ERROR,
	EVENT_BATTERY_I2C_NORMAL,
	EVENT_BATTERY_NTC_ZERO,
	EVENT_BATTERY_NTC_NORMAL,
	EVENT_TEMP_PROTECT_STEP_1,//       temp < -20
	EVENT_TEMP_PROTECT_STEP_2,// -20 < temp < 0
	EVENT_TEMP_PROTECT_STEP_3,//   0 < temp < 8
	EVENT_TEMP_PROTECT_STEP_4,//  8  < temp < 14
	EVENT_TEMP_PROTECT_STEP_5,//  14 < temp < 23
	EVENT_TEMP_PROTECT_STEP_6,//  23 < temp < 45
	EVENT_TEMP_PROTECT_STEP_7,//  45 < temp < 60
	EVENT_TEMP_PROTECT_STEP_8,//  60 < temp
	EVENT_RECHECK_PROTECTION,
	EVENT_SHUTDOWN,
	EVENT_CURRENT_THERMAL_ADJUST,
};

extern u8 quanta_get_mbid(void);
extern void twl6030_poweroff(void);

/* external TWL6030 functions */
extern void twl6030_set_callback(void *data);
extern void twl6030_usb_ldo_on(void);
extern void twl6030_usb_ldo_off(void);
extern void twl6030_vusb_init(void);

const int aicl_results[] =		{ 300, 500, 700, 900, 1200, 1500, 1800, 2000, 2200, 2500, 2500, 2500, 2500, 2500, 2500, 2500 };
const int accc[] =			{ 100, 150, 200, 250, 700, 900, 1200, 1500, 1800, 2000, 2200, 2500 };
const int fast_charge_current_list[] =	{ 700, 900, 1200, 1500, 1800, 2000, 2200, 2500 };
const int precharge_current_list[] =	{ 100, 150, 200, 250 };

enum {
	SMB347_USB_MODE_1,
	SMB347_USB_MODE_5,
	SMB347_USB_MODE_HC,
};

/* GPIO FUNCTION */
static inline int irq_to_gpio(unsigned irq) {
	int tmp = irq - IH_GPIO_BASE;
	if (tmp < OMAP_MAX_GPIO_LINES)
		return tmp;
	return -EIO;
}

/* I2C FUNCTIONS */
static int smb347_i2c_read(struct i2c_client *i2c_client, u8 reg_num, u8 *value) {
	struct summit_smb347_info *di = i2c_get_clientdata(i2c_client);
	s32 error = i2c_smbus_read_byte_data(i2c_client, reg_num);
	if (error < 0) {
		dev_err(di->dev, "i2c error at %s for reg_num %2x: %d\n", __FUNCTION__, reg_num, error);
		return error;
	}
	*value = (unsigned char)(error & 0xff);
	return 0;
}

static int smb347_i2c_write(struct i2c_client *i2c_client, u8 reg_num, u8 value) {
	struct summit_smb347_info *di = i2c_get_clientdata(i2c_client);
	s32 error = i2c_smbus_write_byte_data(i2c_client, reg_num, value);
	if (error < 0) {
		dev_err(di->dev, "i2c error at %s for reg_num %2x: %d\n", __FUNCTION__, reg_num, error);
	}
	return error;
}
/* END I2C FUNCTIONS */

/* FCC / PCC FUNCTIONS */
int summit_find_pre_cc(int cc){
	int i = 0;
	for (i = 3 ; i > -1 ; i--) {
		if (precharge_current_list[i] <= cc)
			break;
	}
	return i;
}

int summit_find_fast_cc(int cc){
	int i = 0;
	for (i = 7 ; i > -1 ; i--) {
		if (fast_charge_current_list[i] <= cc)
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
/* END FCC / PCC FUNCTIONS */

static inline int smb347_apsd_result(int reg) {
	if (SMB347_IS_APSD_DONE(reg))
		return SMB347_APSD_RESULT(reg);
	else
		return -1;
}

static const char *smb347_apsd_result_string(u8 value) {
	switch (smb347_apsd_result(value)) {
	case SMB347_APSD_RESULT_CDP:
		return "CDP";
		break;
	case SMB347_APSD_RESULT_DCP:
		return "DCP";
		break;
	case SMB347_APSD_RESULT_OTHER:
		return "Other Downstream Port";
		break;
	case SMB347_APSD_RESULT_SDP:
		return "SDP";
		break;
	case SMB347_APSD_RESULT_ACA:
		return "ADA charger";
		break;
	case SMB347_APSD_RESULT_TBD_1:
	case SMB347_APSD_RESULT_TBD_2:
		return "TBD";
		break;
	case -1:
		return "not run";
		break;
	case SMB347_APSD_RESULT_NONE:
	default:
		return "unknown";
		break;
	}
}

static inline int smb347_aicl_current(int reg) {
	if (SMB347_IS_AICL_DONE(reg)) {
		int value = SMB347_AICL_RESULT(reg);
		if (value >= 0 && value <= 15)
			return aicl_results[value];
	}
	return -1;
}

static void smb347_aicl_complete(struct summit_smb347_info *di) {
	u8 value = 0xff;
	mutex_lock(&di->lock);
	if (!smb347_i2c_read(di->client, SMB347_STATUS_REG_E, &value))
		dev_info(di->dev, "AICL result: %d mA\n", aicl_results[value & 0xf]);
	mutex_unlock(&di->lock);
	return;
}

static int smb347_config(struct summit_smb347_info *di, int enable) {
	unsigned char value = 0xff;
	int ret = smb347_i2c_read(di->client, SMB347_COMMAND_REG_A, &value);
	if (!ret) {
		if (enable)
			value |= 0x80;
		else
			value &= ~0x80;
		ret = smb347_i2c_write(di->client, SMB347_COMMAND_REG_A, value);
	}
	return ret;
}

#if 1
void smb347_redo_apsd(struct summit_smb347_info *di , int enable) {
	int config = 0;
        smb347_config(di, 1);
	config = i2c_smbus_read_byte_data(di->client, SMB347_ENABLE_CONTROL);
	SET_BIT(config, CHARGER_ERROR_IRQ);
	i2c_smbus_write_byte_data(di->client, SMB347_ENABLE_CONTROL, config);
	mdelay(10);
	config = i2c_smbus_read_byte_data(di->client, SMB347_CHARGE_CONTROL);
	if (enable)
		SET_BIT(config, APSD_ENABLE);
	else
		CLEAR_BIT(config, APSD_ENABLE);
	i2c_smbus_write_byte_data(di->client , SMB347_CHARGE_CONTROL, config);
        smb347_config(di, 0);
}
#else
void smb347_redo_apsd(struct summit_smb347_info *di, int enable) {
        u8 value;
        smb347_config(di, 1);
        if(enable == 0) {
                smb347_i2c_read(di->client, SMB347_CHARGE_CONTROL, &value);
                value &= ~0x04;
                smb347_i2c_write(di->client, SMB347_CHARGE_CONTROL, value);
        }
        else {
                smb347_i2c_read(di->client, SMB347_CHARGE_CONTROL, &value);
                value |= 0x04;
                smb347_i2c_write(di->client, SMB347_CHARGE_CONTROL, value);
        }
	mdelay(10);
        smb347_config(di, 0);
}
#endif
static int smb347_set_charge_control(struct summit_smb347_info *di) {
	int ret = -1;
	u8 value = 0xff;

	if (smb347_config(di, 1)) {
		dev_err(di->dev, "Unable to enable writes to CONFIG regs\n");
		goto done;
	}

	if ((ret = smb347_i2c_read(di->client, SMB347_ENABLE_CONTROL, &value)))
		goto done;

	ret = -1;

	/* Clear bits 5 and 6 */
	value &= ~(0x3 << 5);

	/* Set bit 5 */
	value |= (0x1 << 5);

	/* Clear bits 4, Set HC mode to Register Control */
	value &= ~(0x1 << 4);

	if ((ret = smb347_i2c_write(di->client, SMB347_ENABLE_CONTROL, value)))
		goto done;

	if (smb347_config(di, 0)) {
		dev_err(di->dev, "Unable to disable writes to CONFIG regs\n");
		ret = -1;
		goto done;
	}

done:
	return ret;
}

static int smb347_switch_mode(struct summit_smb347_info *di, int mode) {
	unsigned char value = 0xff;
	int ret = smb347_i2c_read(di->client, SMB347_COMMAND_REG_B, &value);
	if (!ret) {
		switch (mode) {
			case SMB347_USB_MODE_1:
				dev_info(di->dev, "Switching to USB1 mode\n");
				value &= ~0x01;
				value &= ~0x02;
				break;
			case SMB347_USB_MODE_5:
				dev_info(di->dev, "Switching to USB5 mode\n");
				value &= ~0x01;
				value |= 0x02;
				break;
			case SMB347_USB_MODE_HC:
				dev_info(di->dev, "Switching to HC mode\n");
				value |= 0x01;
				break;
			default:
				dev_err(di->dev, "Unknown USB mode: %d\n", mode);
				return -1;
		}
		ret = smb347_i2c_write(di->client, SMB347_COMMAND_REG_B, value);
	}
	return ret;
}

static int smb347_fast_charge_current_limit(struct summit_smb347_info *di) {
	int ret = -1;
	u8 value = 0xff;

	if (smb347_config(di, 1)) {
		dev_err(di->dev, "Unable to enable writes to CONFIG regs\n");
		goto done;
	}

	if ((ret = smb347_i2c_read(di->client, SMB347_CHARGE_CURRENT, &value)))
		goto done;

	ret = -1;

	/* Clear bits 5, 6, 7 */
	value &= ~(0x7 << 5);

	/* Limit Fast Charge Current to 1800 mA */
	value |= (0x4 << 5);

	if ((ret = smb347_i2c_write(di->client, SMB347_CHARGE_CURRENT, value)))
		goto done;

	if (smb347_config(di, 0)) {
		dev_err(di->dev, "Unable to enable writes to CONFIG regs\n");
		goto done;
	}
done:
	return ret;
}

static int smb347_set_termination_current(struct summit_smb347_info *di) {
	int ret = -1;
	u8 value = 0xff;

	if (smb347_config(di, 1)) {
		dev_err(di->dev, "Unable to enable writes to CONFIG regs\n");
		goto done;
	}

	if ((ret = smb347_i2c_read(di->client, SMB347_CHARGE_CURRENT, &value)))
		goto done;

	ret = -1;

	/* Clear bit 2, set bits 0 and 1 (Termination current = 150 mA) */
	value &= ~(1 << 2);
	value |= 0x03;

	if ((ret = smb347_i2c_write(di->client, SMB347_CHARGE_CURRENT, value)))
		goto done;

	if (smb347_config(di, 0)) {
		dev_err(di->dev, "Unable to disable writes to CONFIG regs\n");
		ret = -1;
		goto done;
	}

done:
	return ret;
}

static int smb347_set_charge_timeout_interrupt(struct summit_smb347_info *di) {
	int ret = -1;
	u8 value = 0xff;

	if ((ret = smb347_i2c_read(di->client, SMB347_INTERRUPT_STAT, &value)))
		goto done;

	if (smb347_config(di, 1)) {
		dev_err(di->dev, "Unable to enable writes to CONFIG regs\n");
		ret = -1;
		goto done;
	}

	/* Set bit 7 for enable Charge-Timeout interrupt */
	value |= 0x80;

	ret = smb347_i2c_write(di->client, SMB347_INTERRUPT_STAT, value);

	if (smb347_config(di, 0)) {
		dev_err(di->dev, "Unable to disable writes to CONFIG regs\n");
		ret = -1;
	}

done:
	return ret;
}

static int smb347_change_timeout_usb(struct summit_smb347_info *di, int enable) {
	u8 value = 0xff;
	int ret = 0;

	if ((ret = smb347_i2c_read(di->client, SMB347_STAT_TIMERS, &value)))
		goto done;

	if (smb347_config(di, 1)) {
		dev_err(di->dev, "Unable to enable writes to CONFIG regs\n");
		goto done;
	}

	value &= ~(0x0C);
	if (enable)
		value |= 0x08; //usb: 1527min
	else
		value |= 0x04; //ac: 764min

	if ((ret = smb347_i2c_write(di->client, SMB347_STAT_TIMERS, value)))
		dev_err(di->dev, "%s: Unable to write SMB347_STAT_TIMERS: %d\n", __FUNCTION__, ret);

	if (smb347_config(di, 0))
		dev_err(di->dev, "Unable to disable writes to CONFIG regs\n");

done:
	return ret;
}

static void smb347_apsd_complete(struct summit_smb347_info *di) {
	u8 value = 0xff;
	int ret = -1;
	int type = POWER_SUPPLY_TYPE_USB;

	mutex_lock(&di->lock);

	ret = smb347_i2c_read(di->client, SMB347_STATUS_REG_D, &value);
	if (ret) {
		mutex_unlock(&di->lock);
		return;
	}

	dev_info(di->dev, "Detected charger: %s\n", smb347_apsd_result_string(value));

	switch (smb347_apsd_result(value)) {

	case SMB347_APSD_RESULT_SDP:
	case SMB347_APSD_RESULT_CDP:
		atomic_set(&di->ac_online, 0);
		atomic_set(&di->usb_online, 1);

		if (smb347_apsd_result(value) == SMB347_APSD_RESULT_CDP)
			type = POWER_SUPPLY_TYPE_USB_CDP;

		if (smb347_apsd_result(value) == SMB347_APSD_RESULT_SDP)
			smb347_switch_mode(di, SMB347_USB_MODE_5);
		else
			smb347_switch_mode(di, SMB347_USB_MODE_HC);

		smb347_change_timeout_usb(di, 1);
		break;

	case SMB347_APSD_RESULT_DCP:
	case SMB347_APSD_RESULT_OTHER:
		atomic_set(&di->ac_online, 1);
		atomic_set(&di->usb_online, 0);

		type = POWER_SUPPLY_TYPE_USB_DCP;

		/* Switch to HC mode */
		smb347_switch_mode(di, SMB347_USB_MODE_HC);

		smb347_change_timeout_usb(di, 0);
		break;

	default:
		atomic_set(&di->ac_online, 0);
		atomic_set(&di->usb_online, 0);
		break;
	}

	power_supply_changed(&di->usb);
	power_supply_changed(&di->ac);

	mutex_unlock(&di->lock);

	return;
}

void smb347_read_fault_interrupt_setting(struct summit_smb347_info *di) {
	int value = 0;

	value = i2c_smbus_read_byte_data(di->client, SMB347_FAULT_INTERRUPT);
	dev_info(di->dev, "Fault interrupt == 0x%x\n", value);
	if (TEMP_OUTSIDE_COLD_HOT_HARD_LIMIITS_TRIGGER_IRQ & value)
		dev_info(di->dev, "  Temperature outsid cold/hot hard limits\n");
	if (TEMP_OUTSIDE_COLD_HOT_SOFT_LIMIITS_TRIGGER_IRQ & value)
		dev_info(di->dev, "  Temperature outsid cold/hot soft limits\n");
	if (OTG_BAT_FAIL_ULVO_TRIGGER_IRQ & value)
		dev_info(di->dev, "  OTG Battery Fail(ULVO)\n");
	if (OTG_OVER_CURRENT_LIMIT_TRIGGER_IRQ & value)
		dev_info(di->dev, "  OTG Over-current Limit\n");
	if (USB_OVER_VOLTAGE_TRIGGER_IRQ & value)
		dev_info(di->dev, "  USB Over-Voltage\n");
	if (USB_UNDER_VOLTAGE_TRIGGER_IRQ & value)
		dev_info(di->dev, "  USB Under-voltage\n");
	if (AICL_COMPLETE_TRIGGER_IRQ & value)
		dev_info(di->dev, "  AICL Complete\n");
	if (INTERNAL_OVER_TEMP_TRIGGER_IRQ & value)
		dev_info(di->dev, "  Internal Over-temperature\n");
}

void smb347_read_status_interrupt_setting(struct summit_smb347_info *di){
	int value = 0;

	value = i2c_smbus_read_byte_data(di->client, SMB347_INTERRUPT_STAT);
	dev_info(di->dev, "Status interrupt == 0x%x\n", value);
	if (CHARGE_TIMEOUT_TRIGGER_IRQ & value)
		dev_info(di->dev, "  Charge timeout\n");
	if (OTG_INSERTED_REMOVED_TRIGGER_IRQ & value)
		dev_info(di->dev, "  OTG Insert/Removed\n");
	if (BAT_OVER_VOLTAGE_TRIGGER_IRQ & value)
		dev_info(di->dev, "  Battery Over-voltage\n");
	if (TERMINATION_OR_TAPER_CHARGING_TRIGGER_IRQ & value)
		dev_info(di->dev, "  Termination or Taper Charging\n");
	if (FAST_CHARGING_TRIGGER_IRQ & value)
		dev_info(di->dev, "  Fast Charging\n");
	if (INOK_TRIGGER_IRQ & value)
		dev_info(di->dev, "  USB Under-voltage\n");
	if (MISSING_BATTERY_TRIGGER_IRQ & value)
		dev_info(di->dev, "  Missing Battery\n");
	if (LOW_BATTERY_TRIGGER_IRQ & value)
		dev_info(di->dev, "  Low Battery\n");
}

int smb347_charger_reconfig(struct summit_smb347_info *di) {
	int index = 0;
	int headfile, result = 0;

	smb347_config(di, 1);
	mdelay(1);
	for (index = 0 ; index <= 0x0d; index++) {
		headfile = i2c_smbus_read_byte_data(di->client, index);
		if (headfile != di->pdata->charger_setting[index]) {
			if (di->pdata->charger_setting_skip_flag) {
				if (index != 0x05 && index != 0x03 && index != 0x06)
					i2c_smbus_write_byte_data(di->client, index, di->pdata->charger_setting[index]);
			}
			else
				i2c_smbus_write_byte_data(di->client, index, di->pdata->charger_setting[index]);
			result = i2c_smbus_read_byte_data(di->client, index);
		}
	}
	mdelay(1);
	smb347_config(di, 0);
	return 0;
}

static int smb347_read_id(struct summit_smb347_info *di, int *id) {
	int config = i2c_smbus_read_byte_data(di->client, SMB347_SLAVE_ADDR);
	dev_info(di->dev, "%s: config = 0x%x\n", __func__, config);
	if (config < 0) {
		pr_err("i2c error at %s: %d\n", __FUNCTION__, config);
		return config;
	}
	*id = config >> 1;
	return 0;
}

static int smb347_configure_otg(struct summit_smb347_info *di, int enable) {
	u8 value = 0xff;

	if (smb347_config(di, 1)) {
		dev_err(di->dev, "Unable to enable writes to CONFIG regs\n");
		goto error;
	}
	if (enable) {
		/* Configure INOK to be active high */
		mdelay(1);
		if (smb347_i2c_write(di->client, SMB347_SYSOK_USB30, 0x01))
			goto error;
		mdelay(1);
		if (smb347_i2c_read(di->client, SMB347_OTG_THERM_CONTROL, &value))
			goto error;
		mdelay(1);
		/* Change "OTG output current limit" to 250mA */
		if (smb347_i2c_write(di->client, SMB347_OTG_THERM_CONTROL, (value & (~(1<<3)))))
			goto error;
		/* Enable OTG */
		mdelay(1);
		if (smb347_i2c_write(di->client, SMB347_COMMAND_REG_A, (1<<4)))
			goto error;
	} else {
		/* Disable OTG */
		mdelay(1);
		if (smb347_i2c_read(di->client, SMB347_COMMAND_REG_A, &value))
			goto error;
		mdelay(1);
		if (smb347_i2c_write(di->client, SMB347_COMMAND_REG_A, (value & (~(1<<4)))))
			goto error;
		mdelay(1);
		if (smb347_i2c_read(di->client, SMB347_OTG_THERM_CONTROL, &value))
			goto error;
		mdelay(1);
		/* Change "OTG output current limit" to 250mA */
		if (smb347_i2c_write(di->client, SMB347_OTG_THERM_CONTROL, (value & (~(1<<3)))))
			goto error;
		/* Configure INOK to be active low */
		mdelay(1);
		if (smb347_i2c_read(di->client, SMB347_SYSOK_USB30, &value))
			goto error;
		mdelay(1);
		if (smb347_i2c_write(di->client, SMB347_SYSOK_USB30, (value & (~(1)))))
			goto error;
        }
	mdelay(1);
	if (smb347_config(di, 0)) {
		dev_err(di->dev, "Unable to disable writes to CONFIG regs\n");
		goto error;
	}
	return 0;
error:
		dev_info(di->dev, "1\n");
        return -1;
}

int local_summit_suspend = 0;
int smb347_suspend_mode(struct summit_smb347_info *di, int enable) {
	int ret = -1;
	unsigned char value = 0xff;

	mutex_lock(&di->lock);
	if ((ret = smb347_i2c_read(di->client, SMB347_COMMAND_REG_A, &value)))
		goto done;

	if (enable) {
		value |= 0x04;
		local_summit_suspend = 1;
	} else {
		value &= ~(0x04);
		local_summit_suspend = 0;
	}

	ret = smb347_i2c_write(di->client, SMB347_COMMAND_REG_A, value);

done:
	mutex_unlock(&di->lock);
	return 0;
}

#if 0
/*
 * Battery missing detect only has effect when enable charge.
 *
 */
int summit_check_bmd(struct summit_smb347_info *di) {
	int value = 0;
	int result = 0;

	value = i2c_smbus_read_byte_data(di->client, SMB347_INTSTAT_REG_B);
	result = BAT_MISS_STATUS_BIT(value);
	if (result)
		dev_info(di->dev , "%s:Battery Miss\n" , __func__);
	else
		dev_info(di->dev , "%s:Battery Not Miss\n" , __func__);
	return result;
}

int summit_charge_reset(struct summit_smb347_info *di){
	int command = 0;

	COMMAND_POWER_ON_RESET(command);
	i2c_smbus_write_byte_data(di->client, SMB347_COMMAND_REG_B, command);
	/* Recommended delay is 20ms ,  make it 50 ms to be really safe */
	mdelay(50);
	return 0;
}

int summit_config_charge_enable(struct summit_smb347_info *di, int setting) {
	int config = 0;
	if (setting != CHARGE_EN_I2C_0 &&
		setting != CHARGE_EN_I2C_1 &&
		setting != EN_PIN_ACTIVE_HIGH &&
		setting != EN_PIN_ACTIVE_LOW)
		return 0;
	smb347_config(di, 1);
	mdelay(1);
	config = i2c_smbus_read_byte_data(di->client, SMB347_ENABLE_CONTROL);
	dev_info(di->dev, "%s:config=0x%x\n", __func__, config);
	CLEAR_BIT(config, 5);
	CLEAR_BIT(config, 6);
	config |= setting;
	dev_info(di->dev, "%s:config=0x%x\n", __func__, config);
	i2c_smbus_write_byte_data(di->client, SMB347_ENABLE_CONTROL, config);
	mdelay(1);
	config = i2c_smbus_read_byte_data(di->client, SMB347_ENABLE_CONTROL);
	dev_info(di->dev, "%s:config=0x%x\n", __func__, config);
	smb347_config(di, 0);
	return 1;
}

int summit_charge_enable(struct summit_smb347_info *di, int enable) {
	int command = 0;
	int config , result = 0;

	config = i2c_smbus_read_byte_data(di->client, SMB347_ENABLE_CONTROL);
	config &= 0x60;
	switch (config) {
	case CHARGE_EN_I2C_0:
		command = i2c_smbus_read_byte_data(di->client, SMB347_COMMAND_REG_A);
		if (command < 0) {
			dev_err(di->dev,  "Got I2C error %d in %s\n", command, __func__);
			return -1;
		}
		if (enable)
			SET_BIT(command, 1);
		else
			CLEAR_BIT(command, 1);
		i2c_smbus_write_byte_data(di->client, SMB347_COMMAND_REG_A, command);
		break;
	case CHARGE_EN_I2C_1:
		command = i2c_smbus_read_byte_data(di->client, SMB347_COMMAND_REG_A);
		if (command < 0) {
			dev_err(di->dev, "Got I2C error %d in %s\n", command, __func__);
			return -1;
		}
		if (enable)
			CLEAR_BIT(command , 1);
		else
			CLEAR_BIT(command , 1);
		i2c_smbus_write_byte_data(di->client, SMB347_COMMAND_REG_A, command);
		break;
	case EN_PIN_ACTIVE_HIGH:
		if (enable)
			gpio_set_value(di->pdata->pin_en,  1);
		else
			gpio_set_value(di->pdata->pin_en,  0);
		break;
	case EN_PIN_ACTIVE_LOW:
		if (enable)
			gpio_set_value(di->pdata->pin_en,  0);
		else
			gpio_set_value(di->pdata->pin_en,  1);
		break;
	}
	return result;
}

void summit_config_suspend_by_register(struct summit_smb347_info *di, int byregister) {
	int config = 0;

	smb347_config(di , 1);
	config = i2c_smbus_read_byte_data(di->client, SMB347_FUNCTIONS);
	if (byregister) {
		/*Controlled by Register*/
		SET_BIT(config , 7);
		i2c_smbus_write_byte_data(di->client, SMB347_FUNCTIONS, config);
	} else {
		CLEAR_BIT(config, 7);
		i2c_smbus_write_byte_data(di->client, SMB347_FUNCTIONS, config);
	}
	smb347_config(di , 0);
}

void summit_suspend_mode(struct summit_smb347_info *di, int enable) {
	int config = 0;
	int command = 0;

	config = i2c_smbus_read_byte_data(di->client, SMB347_FUNCTIONS);
	if (IS_BIT_SET(config, 7)) {
		/*Controlled by Register*/
		command = i2c_smbus_read_byte_data(di->client, SMB347_COMMAND_REG_A);
		if (command < 0) {
			dev_err(di->dev, "Got I2C error %d in %s\n", command, __func__);
			return;
		}

		if (enable)
			COMMAND_SUSPEND_MODE_ENABLE(command);
		else
			COMMAND_SUSPEND_MODE_DISABLE(command);
		i2c_smbus_write_byte_data(di->client, SMB347_COMMAND_REG_A, command);
	} else {
		/*Controlled by Pin*/
		if (enable)
			gpio_set_value(di->pdata->pin_susp,  0);
		else
			gpio_set_value(di->pdata->pin_susp,  1);
	}
}

void summit_force_precharge(struct summit_smb347_info *di, int enable) {
	int config, command = 0;

	smb347_config(di, 1);
	command = i2c_smbus_read_byte_data(di->client, SMB347_COMMAND_REG_A);
	if (command < 0) {
		dev_err(di->dev, "Got I2C error %d in %s\n", command, __func__);
		return;
	}
	config = i2c_smbus_read_byte_data(di->client, SMB347_SYSOK_USB30);
	if (enable) {
		SET_BIT(config , 1);
		COMMAND_FORCE_PRE_CHARGE_CURRENT_SETTING(command);
	} else {
		CLEAR_BIT(config , 1);
		COMMAND_ALLOW_FAST_CHARGE_CURRENT_SETTING(command);
	}
	i2c_smbus_write_byte_data(di->client, SMB347_COMMAND_REG_A, command);
	i2c_smbus_write_byte_data(di->client, SMB347_SYSOK_USB30, config);
	smb347_config(di, 0);
}

void summit_charger_stat_output(struct summit_smb347_info *di, int enable) {
	int command = 0;

	command = i2c_smbus_read_byte_data(di->client, SMB347_COMMAND_REG_A);
	if (command < 0) {
		dev_err(di->dev, "Got I2C error %d in %s\n", command, __func__);
		return;
	}
	if (enable)
		COMMAND_STAT_OUTPUT_ENABLE(command);
	else
		COMMAND_STAT_OUTPUT_DISABLE(command);
	i2c_smbus_write_byte_data(di->client, SMB347_COMMAND_REG_A, command);
}

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
	int i, cc = 0;

	C = get_battery_mAh();
	dev_info(di->dev , " C=%d mAh\n" , C);
	switch (event) {
	case EVENT_TEMP_PROTECT_STEP_3:
	case EVENT_TEMP_PROTECT_STEP_4:
		cc = C * 18 / 100;
		i = summit_find_fast_cc(cc);
		if (i > -1) {
			summit_force_precharge(di, 0);
			summit_config_charge_current(di, i << 5, CONFIG_NO_CHANGE, CONFIG_NO_CHANGE);
			CLEAR_BAT_NO_SUIT_CURRENT(di->protect_event);
		} else {
			i = summit_find_pre_cc(cc);
			if (i > -1) {
				summit_force_precharge(di, 1);
				summit_config_charge_current(di, CONFIG_NO_CHANGE, i << 3, CONFIG_NO_CHANGE);
				CLEAR_BAT_NO_SUIT_CURRENT(di->protect_event);
			} else {
				summit_force_precharge(di, 0);
				SET_BAT_NO_SUIT_CURRENT(di->protect_event);
			}
		}
		break;
	case EVENT_TEMP_PROTECT_STEP_5:
	case EVENT_TEMP_PROTECT_STEP_6:
	case EVENT_TEMP_PROTECT_STEP_7:
		if ((event == EVENT_TEMP_PROTECT_STEP_7) || (event == EVENT_TEMP_PROTECT_STEP_5))
			cc = C >> 1;
		else
			cc = C * 70 / 100;
		i = summit_find_fast_cc(cc);
		if (i > -1) {
			summit_force_precharge(di , 0);
			summit_config_charge_current(di, i << 5, CONFIG_NO_CHANGE, CONFIG_NO_CHANGE);
			CLEAR_BAT_NO_SUIT_CURRENT(di->protect_event);
		} else {
			i = summit_find_pre_cc(cc);
			if (i > -1) {
				summit_force_precharge(di, 1);
				summit_config_charge_current(di, CONFIG_NO_CHANGE, i << 3, CONFIG_NO_CHANGE);
				CLEAR_BAT_NO_SUIT_CURRENT(di->protect_event);
			} else {
				summit_force_precharge(di, 0);
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
		summit_charge_enable(di, 0);
	else
		summit_charge_enable(di, 1);
	return 0;
}

void summit_config_charge_voltage(struct summit_smb347_info *di, int pre_volt, int float_volt) {
	int config = 0;

	smb347_config(di, 1);
	mdelay(1);
	config = i2c_smbus_read_byte_data(di->client, SMB347_FLOAT_VOLTAGE);
	if (pre_volt != CONFIG_NO_CHANGE) {
		config &= ~PRE_CHG_VOLTAGE_CLEAN;
		config |= pre_volt;
	}
	if (float_volt != CONFIG_NO_CHANGE) {
		config &= ~FLOAT_VOLTAGE_CLEAN;
		config |= float_volt;
	}
	i2c_smbus_write_byte_data(di->client, SMB347_FLOAT_VOLTAGE, config);
	mdelay(1);
	smb347_config(di, 0);
}

void summit_config_charge_current(struct summit_smb347_info *di, int fc_current, int pc_current, int tc_current) {
	int config = 0;

	smb347_config(di, 1);
	mdelay(1);
	config = i2c_smbus_read_byte_data(di->client, SMB347_CHARGE_CURRENT);
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
	i2c_smbus_write_byte_data(di->client, SMB347_CHARGE_CURRENT, config);
	mdelay(1);
	smb347_config(di , 0);
}

void summit_config_inpu_current(struct summit_smb347_info *di, int dc_in, int usb_in) {
	int config = 0;

	smb347_config(di, 1);
	mdelay(1);
	config = i2c_smbus_read_byte_data(di->client, SMB347_INPUT_CURR_LIMIT);
	if (dc_in != CONFIG_NO_CHANGE) {
		config &= ~DC_ICL_CLEAN;
		config |= dc_in;
	}
	if (usb_in != CONFIG_NO_CHANGE) {
		config &= ~USBHC_ICL_CLEAN;
		config |= usb_in;
	}
	i2c_smbus_write_byte_data(di->client, SMB347_INPUT_CURR_LIMIT, config);
	mdelay(1);
	smb347_config(di , 0);
}

void summit_config_voltage(struct summit_smb347_info *di, int pre_vol_thres, int fast_vol_thres) {
	int config = 0;

	smb347_config(di, 1);
	mdelay(1);
	config = i2c_smbus_read_byte_data(di->client, SMB347_FLOAT_VOLTAGE);
	if (pre_vol_thres != CONFIG_NO_CHANGE) {
		config &= ~PRE_CHG_VOLTAGE_CLEAN;
		config |= pre_vol_thres;
	}
	if (fast_vol_thres != CONFIG_NO_CHANGE) {
		config &= ~FLOAT_VOLTAGE_CLEAN;
		config |= fast_vol_thres;
	}
	i2c_smbus_write_byte_data(di->client, SMB347_FLOAT_VOLTAGE, config);
	mdelay(1);
	smb347_config(di , 0);
}

void summit_config_apsd(struct summit_smb347_info *di, int enable) {
	int config = 0;

	smb347_config(di, 1);
	config = i2c_smbus_read_byte_data(di->client , 0x6);
	config |= 0x2;
	i2c_smbus_write_byte_data(di->client, 0x6, config);
	mdelay(10);
	config = i2c_smbus_read_byte_data(di->client, SMB347_CHARGE_CONTROL);
	if (enable)
		SET_APSD_ENABLE(config);
	else
		SET_APSD_DISABLE(config);
	i2c_smbus_write_byte_data(di->client, SMB347_CHARGE_CONTROL, config);
	smb347_config(di, 0);
}

void summit_config_aicl(struct summit_smb347_info *di, int enable, int aicl_thres) {
	int config = 0;

	smb347_config(di, 1);
	mdelay(1);
	config = i2c_smbus_read_byte_data(di->client, SMB347_FUNCTIONS);
	if (enable)
		SET_AICL_ENABLE(config);
	else
		SET_AICL_DISABLE(config);
	if (aicl_thres == 4200)
		SET_AIC_TH_4200mV(config);
	else if (aicl_thres == 4500)
		SET_AIC_TH_4500mV(config);
	i2c_smbus_write_byte_data(di->client, SMB347_FUNCTIONS, config);
	mdelay(1);
	smb347_config(di, 0);
}

void summit_enable_fault_interrupt(struct summit_smb347_info *di, int enable) {
	int config = 0;

	smb347_config(di, 1);
	mdelay(1);
	config = i2c_smbus_read_byte_data(di->client, SMB347_FAULT_INTERRUPT);
	config |= enable;
	i2c_smbus_write_byte_data(di->client, SMB347_FAULT_INTERRUPT, config);
	mdelay(1);
	smb347_config(di, 0);
}

void summit_disable_fault_interrupt(struct summit_smb347_info *di, int disable) {
	int config = 0;

	smb347_config(di, 1);
	mdelay(1);
	config = i2c_smbus_read_byte_data(di->client, SMB347_FAULT_INTERRUPT);
	config &= ~disable;
	i2c_smbus_write_byte_data(di->client, SMB347_FAULT_INTERRUPT, config);
	mdelay(1);
	smb347_config(di, 0);
}

void summit_enable_stat_interrupt(struct summit_smb347_info *di, int enable) {
	int config = 0;
	smb347_config(di, 1);
	mdelay(1);
	config = i2c_smbus_read_byte_data(di->client, SMB347_INTERRUPT_STAT);
	config |= enable;
	i2c_smbus_write_byte_data(di->client, SMB347_INTERRUPT_STAT, config);
	mdelay(1);
	smb347_config(di, 0);
}

void summit_disable_stat_interrupt(struct summit_smb347_info *di, int disable) {
	int config = 0;
	smb347_config(di, 1);
	mdelay(1);
	config = i2c_smbus_read_byte_data(di->client, SMB347_INTERRUPT_STAT);
	config &= ~disable;
	i2c_smbus_write_byte_data(di->client, SMB347_INTERRUPT_STAT, config);
	mdelay(1);
	smb347_config(di, 0);
}

int summit_get_mode(struct summit_smb347_info *di) {
	int value = 0;
	int temp = 0;

	value = i2c_smbus_read_byte_data(di->client, SMB347_STATUS_REG_E);
	temp = value;
	return USB15_HC_MODE(temp);
}

int summit_get_apsd_result(struct summit_smb347_info *di) {
	int status, result;

	status = i2c_smbus_read_byte_data(di->client, SMB347_STATUS_REG_D);
	result = status;
	if (APSD_STATUS(status)) {
		switch (APSD_RESULTS(result)) {
		case APSD_NOT_RUN:
			dev_info(di->dev, "APSD_NOT_RUN\n");
			break;
		case APSD_CHARGING_DOWNSTREAM_PORT:
			dev_info(di->dev, "CHARGING_DOWNSTREAM_PORT\n");
			return USB_EVENT_HOST_CHARGER;
			break;
		case APSD_DEDICATED_DOWNSTREAM_PORT:
			dev_info(di->dev, "DEDICATED_DOWNSTREAM_PORT\n");
			return USB_EVENT_CHARGER;
			break;
		case APSD_OTHER_DOWNSTREAM_PORT:
			dev_info(di->dev, "OTHER_DOWNSTREAM_PORT\n");
			return EVENT_DETECT_OTHER;
			break;
		case APSD_STANDARD_DOWNSTREAM_PORT:
			dev_info(di->dev, "STANDARD_DOWNSTREAM_PORT\n");
			return EVENT_DETECT_PC;
			break;
		case APSD_ACA_CHARGER:
			dev_info(di->dev, "ACA_CHARGER\n");
			break;
		case APSD_TBD:
			dev_info(di->dev, "TBD\n");
			return EVENT_DETECT_TBD;
			break;
		}
	} else {
		 dev_info(di->dev, "APSD NOT Completed\n");
		 return EVENT_APSD_NOT_COMPLETE;
	}
	return -1;
}

int summit_get_aicl_result(struct summit_smb347_info *di) {
	return aicl_results[AICL_RESULT(i2c_smbus_read_byte_data(di->client, SMB347_STATUS_REG_E))];
}
#endif

/* BEGIN POWER SUPPLY */
static enum power_supply_property summit_usb_props[] = { POWER_SUPPLY_PROP_ONLINE, };
static enum power_supply_property summit_ac_props[] = { POWER_SUPPLY_PROP_ONLINE, };

static int summit_usb_get_property(struct power_supply *psy, enum power_supply_property psp, union power_supply_propval *val)
{
	struct summit_smb347_info *di = container_of(psy, struct summit_smb347_info, usb);
	switch (psp) {
		case POWER_SUPPLY_PROP_ONLINE:
			val->intval = atomic_read(&di->usb_online);
		break;
		default:
			return -EINVAL;
	}
	return 0;
}

static int summit_ac_get_property(struct power_supply *psy, enum power_supply_property psp, union power_supply_propval *val)
{
	struct summit_smb347_info *di = container_of(psy, struct summit_smb347_info, ac);
	switch (psp) {
		case POWER_SUPPLY_PROP_ONLINE:
			val->intval = atomic_read(&di->ac_online);
		break;
		default:
			return -EINVAL;
	}
	return 0;
}
/* END POWER SUPPLY */

/* BEGIN THERMAL FRAMEWORK */
#ifdef CONFIG_THERMAL_FRAMEWORK
#define THERMAL_PREFIX "Thermal Policy: Charger cooling agent: "
#define THERMAL_INFO(fmt, args...) do { printk(KERN_INFO THERMAL_PREFIX fmt, ## args); } while(0)

#ifdef THERMAL_DEBUG
#define THERMAL_DBG(fmt, args...) do { printk(KERN_DEBUG THERMAL_PREFIX fmt, ## args); } while(0)
#else
#define THERMAL_DBG(fmt, args...) do {} while(0)
#endif
static int smb347_apply_cooling(struct thermal_dev *dev, int level)
{
	struct summit_smb347_info *di = i2c_get_clientdata(to_i2c_client(dev->dev));
	int current_limit, previous_max_charge_current;	
	unsigned char temp = 0xff;
	int ret = -1;
	static int previous_cooling_level = 0, new_cooling_level = 0;

	/* transform into current limitation */
	current_limit = thermal_cooling_device_reduction_get(dev, level);

	if (current_limit < 0 || current_limit > 1800)
		return -EINVAL;

	THERMAL_DBG("%s : %d : %d\n",__func__,__LINE__,current_limit);

	/* for logging ...*/
	previous_max_charge_current = di->max_thermal_charge_current;

	new_cooling_level = level;

	if (current_limit != previous_max_charge_current) {	
		smb347_config(di, 1);

		switch (current_limit){
			case 1800:
				/* If summit charger is in suspend mode, wake up it. */
				if (local_summit_suspend == 1) smb347_suspend_mode(di, 0);

				/* Set the limitation of input current to 1800 mA. */
				smb347_i2c_write(di->client, SMB347_INPUT_CURR_LIMIT, 0x66);

				/* Redo the AICL to change the input current limit. */
				ret = smb347_i2c_read(di->client, SMB347_FUNCTIONS, &temp);
				if (ret) {
					pr_err("%s: Unable to read SMB347_FUNCTIONS: %d\n",__FUNCTION__, ret);
					return ret;
				}
				/* Disable AICL */
				temp &= ~0x10;
				smb347_i2c_write(di->client, SMB347_FUNCTIONS, temp);

				msleep(10);

				/* Enable AICL */
				temp |= 0x10;
				smb347_i2c_write(di->client, SMB347_FUNCTIONS, temp);

				msleep(10);

				/* Set the charging current to 1800 mA. */
				ret = smb347_i2c_read(di->client, SMB347_CHARGE_CURRENT, &temp);
				if (ret) {
					pr_err("%s: Unable to read SMB347_CHARGE_CURRENT: %d\n",__FUNCTION__, ret);
					return ret;
				}
				temp &= ~0xE0;
				temp |= 4 << 5;
				smb347_i2c_write(di->client, SMB347_CHARGE_CURRENT, temp);
				di->max_thermal_charge_current = 1800;
				break;

			case 1500:
				/* If summit charger is in suspend mode, wake up it. */
				if (local_summit_suspend == 1) smb347_suspend_mode(di, 0);

				/* Set the limitation of input current to 1500 mA. */
				smb347_i2c_write(di->client, SMB347_INPUT_CURR_LIMIT, 0x65);

				/* Redo the AICL to change the input current limit. */
				ret = smb347_i2c_read(di->client, SMB347_FUNCTIONS, &temp);
				if (ret) {
					pr_err("%s: Unable to read SUMMIT_SMB347_FUNCTIONS: %d\n",__FUNCTION__, ret);
					return ret;
				}
				/* Disable AICL */
				temp &= ~0x10;
				smb347_i2c_write(di->client, SMB347_FUNCTIONS, temp);

				msleep(10);

				/* Enable AICL */
				temp |= 0x10;
				smb347_i2c_write(di->client, SMB347_FUNCTIONS, temp);

				msleep(10);

				/* Set the charging current to 1500 mA. */
				ret = smb347_i2c_read(di->client, SMB347_CHARGE_CURRENT, &temp);
				if (ret) {
					pr_err("%s: Unable to read SMB347_CHARGE_CURRENT: %d\n",__FUNCTION__, ret);
					return ret;
				}

				temp &= ~0xE0;
				temp |= 3 << 5;
				smb347_i2c_write(di->client, SMB347_CHARGE_CURRENT, temp);

				di->max_thermal_charge_current = 1500;
				break;

			case 1200:
				/* If summit charger is in suspend mode, wake up it. */
				if (local_summit_suspend == 1) smb347_suspend_mode(di, 0);

				/* Set the limitation of input current to 1200 mA. */
				smb347_i2c_write(di->client, SMB347_INPUT_CURR_LIMIT, 0x64);

				/* Redo the AICL to change the input current limit. */
				ret = smb347_i2c_read(di->client, SMB347_FUNCTIONS, &temp);
				if (ret) {
					pr_err("%s: Unable to read SMB347_FUNCTIONS: %d\n",__FUNCTION__, ret);
					return ret;
				}
				/* Disable AICL */
				temp &= ~0x10;
				smb347_i2c_write(di->client, SMB347_FUNCTIONS, temp);

				msleep(10);

				/* Enable AICL */
				temp |= 0x10;
				smb347_i2c_write(di->client, SMB347_FUNCTIONS, temp);

				msleep(10);

				/* Set the charging current to 1200 mA. */
				ret = smb347_i2c_read(di->client,SMB347_CHARGE_CURRENT, &temp);
				if (ret) {
					pr_err("%s: Unable to read SMB347_CHARGE_CURRENT: %d\n",__FUNCTION__, ret);
					return ret;
				}

				temp &= ~0xE0;
				temp |= 2 << 5;
				smb347_i2c_write(di->client, SMB347_CHARGE_CURRENT, temp);

				di->max_thermal_charge_current = 1200;
				break;

			case 900:
				/* If summit charger is in suspend mode, wake up it. */
				if (local_summit_suspend == 1) smb347_suspend_mode(di, 0);

				/* Set the limitation of input current to 900 mA. */
				smb347_i2c_write(di->client, SMB347_INPUT_CURR_LIMIT, 0x63);

				/* Redo the AICL to change the input current limit. */
				ret = smb347_i2c_read(di->client, SMB347_FUNCTIONS, &temp);
				if (ret) {
					pr_err("%s: Unable to read SMB347_FUNCTIONS: %d\n",__FUNCTION__, ret);
					return ret;
				}
				/* Disable AICL */
				temp &= ~0x10;
				smb347_i2c_write(di->client, SMB347_FUNCTIONS, temp);

				msleep(10);

				/* Enable AICL */
				temp |= 0x10;
				smb347_i2c_write(di->client, SMB347_FUNCTIONS, temp);

				msleep(10);

				/* Set the charging current to 900 mA. */
				ret = smb347_i2c_read(di->client, SMB347_CHARGE_CURRENT, &temp);
				if (ret) {
					pr_err("%s: Unable to read SMB347_CHARGE_CURRENT: %d\n",__FUNCTION__, ret);
					return ret;
				}

				temp &= ~0xE0;
				temp |= 1 << 5;
				smb347_i2c_write(di->client, SMB347_CHARGE_CURRENT, temp);

				di->max_thermal_charge_current = 900;
				break;

			case 0:
				/* Receiving no charging command, summit charger enters to suspend mode. */
				if (local_summit_suspend == 0) smb347_suspend_mode(di, 1);

				/* Set the limitation of input current to 1800 mA. */
				smb347_i2c_write(di->client, SMB347_INPUT_CURR_LIMIT, 0x66);

				/* Redo the AICL to change the input current limit. */
				ret = smb347_i2c_read(di->client, SMB347_FUNCTIONS, &temp);
				if (ret) {
					pr_err("%s: Unable to read SUMMIT_SMB347_FUNCTIONS: %d\n",__FUNCTION__, ret);
					return ret;
				}
				/* Disable AICL */
				temp &= ~0x10;
				smb347_i2c_write(di->client, SMB347_FUNCTIONS, temp);

				msleep(10);

				/* Enable AICL */
				temp |= 0x10;
				smb347_i2c_write(di->client, SMB347_FUNCTIONS, temp);

				msleep(10);

				di->max_thermal_charge_current = 0;
				break;
		}

		smb347_config(di,0);

		THERMAL_INFO("max charge current transision from %d to %d",previous_max_charge_current,di->max_thermal_charge_current); 
	}

	previous_cooling_level = new_cooling_level;

	return 0;
}

static struct thermal_dev_ops smb347_cooling_ops = {
	.cool_device = smb347_apply_cooling,
};

static struct thermal_dev case_thermal_dev = {
	.name		= "charger_cooling",
	.domain_name	= "case",
	.dev_ops	= &smb347_cooling_ops,
};
#endif
/* END THERMAL */

/* BEGIN "TO" STRING FUNCTIONS */
static const char *smb347_vbus_id_status_string(int status) {
	switch(status) {
	case OMAP_MUSB_ID_GROUND : return "OMAP_MUSB_ID_GROUND";
	case OMAP_MUSB_ID_FLOAT : return "OMAP_MUSB_ID_FLOAT";
	case OMAP_MUSB_VBUS_VALID : return "OMAP_MUSB_VBUS_VALID";
	case OMAP_MUSB_VBUS_OFF : return "OMAP_MUSB_VBUS_OFF";
	}
	return "OMAP_MUSB_UNKNOWN";
}

static const char *smb347_otg_state_string(enum usb_otg_state state) {
	switch(state) {
	case OTG_STATE_UNDEFINED : return "OTG_STATE_UNDEFINED";
	case OTG_STATE_B_IDLE : return "OTG_STATE_B_IDLE";
	case OTG_STATE_B_SRP_INIT : return "OTG_STATE_B_SRP_INIT";
	case OTG_STATE_B_PERIPHERAL : return "OTG_STATE_B_PERIPHERAL";
	case OTG_STATE_B_WAIT_ACON : return "OTG_STATE_B_WAIT_ACON";
	case OTG_STATE_B_HOST : return "OTG_STATE_B_HOST";
	case OTG_STATE_A_IDLE : return "OTG_STATE_A_IDLE";
	case OTG_STATE_A_WAIT_VRISE : return "OTG_STATE_A_WAIT_VRISE";
	case OTG_STATE_A_WAIT_BCON : return "OTG_STATE_A_WAIT_BCON";
	case OTG_STATE_A_HOST : return "OTG_STATE_A_HOST";
	case OTG_STATE_A_SUSPEND : return "OTG_STATE_A_SUSPEND";
	case OTG_STATE_A_PERIPHERAL : return "OTG_STATE_A_PERIPHERAL";
	case OTG_STATE_A_WAIT_VFALL : return "OTG_STATE_A_WAIT_VFALL";
	case OTG_STATE_A_VBUS_ERR : return "OTG_STATE_A_VBUS_ERR";
	}
	return "OTG_STATE_KNOWN";
}

static const char *smb347_phy_type_string(enum usb_phy_type type) {
	switch(type) {
	case USB_PHY_TYPE_UNDEFINED : return "USB_PHY_TYPE_UNDEFINED";
	case USB_PHY_TYPE_USB2 : return "USB_PHY_TYPE_USB2";
	case USB_PHY_TYPE_USB3 : return "USB_PHY_TYPE_USB3";
	}
	return "USB_PHY_TYPE_UNKNOWN";
}

static const char *smb347_phy_event_string(enum usb_phy_events event) {
	switch(event) {
	case USB_EVENT_NONE : return "USB_EVENT_NONE";
	case USB_EVENT_VBUS : return "USB_EVENT_VBUS";
	case USB_EVENT_ID : return "USB_EVENT_ID";
	case USB_EVENT_DETECT_SOURCE : return "USB_EVENT_DETECT_SOURCE";
	case USB_EVENT_CHARGER : return "USB_EVENT_CHARGER";
	case USB_EVENT_HOST_CHARGER : return "USB_EVENT_HOST_CHARGER";
	case USB_EVENT_ENUMERATED : return "USB_EVENT_ENUMERATED";
	}
	return "USB_EVENT_UNKNOWN";
}
/* END STRING FUNCTIONS */

/* BEGIN TWL6030 UTILITY FUNCTIONS */
static inline int twl6030_writeb(struct summit_smb347_info *di, u8 module, u8 data, u8 address)
{
	int ret = 0;
	ret = twl_i2c_write_u8(module, data, address);
	if (ret < 0)
		dev_err(di->dev, "Write[0x%x] Error %d\n", address, ret);
	return ret;
}

static inline u8 twl6030_readb(struct summit_smb347_info *di, u8 module, u8 address)
{
	u8 data, ret = 0;
	ret = twl_i2c_read_u8(module, &data, address);
	if (ret >= 0)
		ret = data;
	else
		dev_err(di->dev, "readb[0x%x,0x%x] Error %d\n", module, address, ret);
	return ret;
}
/* END TWL6030 Functions */

/* IRQ FUNCTIONS */
void smb347_read_interrupt_a(struct summit_smb347_info *di) {
	int value = 0;
	value = i2c_smbus_read_byte_data(di->client, SMB347_INTSTAT_REG_A);
	if (HOT_TEMP_HARD_LIMIT_IRQ & value) {
		dev_dbg(di->dev , "Hot Temperature Hard Limit IRQ\n");
		if (HOT_TEMP_HARD_LIMIT_STATUS & value) {
			dev_dbg(di->dev, "  Event: Over Hot Temperature Hard Limit\n");
//			writeIntoCBuffer(&di->events, EVENT_OVER_HOT_TEMP_HARD_LIMIT);
//			queue_delayed_work(summit_work_queue, &di->summit_monitor_work , msecs_to_jiffies(1));
		} else {
			dev_dbg(di->dev, "  Event: Below Hot Temperature Hard Limit Status\n");
//			writeIntoCBuffer(&di->events, EVENT_BELOW_HOT_TEMP_HARD_LIMIT);
//			queue_delayed_work(summit_work_queue, &di->summit_monitor_work , msecs_to_jiffies(1));
		}
	}
	if (COLD_TEMP_HARD_LIMIT_IRQ & value) {
		dev_dbg(di->dev, "Cold Temperature Hard Limit IRQ\n");
		if (COLD_TEMP_HARD_LIMIT_STATUS & value) {
			dev_dbg(di->dev, "  Event: Over Cold Temperature Hard Limit Status\n");
//			writeIntoCBuffer(&di->events, EVENT_OVER_COLD_TEMP_HARD_LIMIT);
//			queue_delayed_work(summit_work_queue, &di->summit_monitor_work , msecs_to_jiffies(1));
		} else {
			dev_dbg(di->dev, "  Event: Below Cold Temperature Hard Limit Status\n");
//			writeIntoCBuffer(&di->events, EVENT_BELOW_COLD_TEMP_HARD_LIMIT);
//			queue_delayed_work(summit_work_queue, &di->summit_monitor_work , msecs_to_jiffies(1));
		}
	}
	if (HOT_TEMP_SOFT_LIMIT_IRQ & value) {
		dev_dbg(di->dev, "Hot Temperature Soft Limit IRQ\n");
		if (HOT_TEMP_SOFT_LIMIT_STATUS & value) {
			dev_dbg(di->dev, "  Event: Over Hot Temperature Soft Limit\n");
//			writeIntoCBuffer(&di->events, EVENT_OVER_HOT_TEMP_SOFT_LIMIT);
//			queue_delayed_work(summit_work_queue, &di->summit_monitor_work , msecs_to_jiffies(1));
		} else {
			dev_dbg(di->dev, "  Event: Below Hot Temperature Soft Limit\n");
//			writeIntoCBuffer(&di->events, EVENT_BELOW_HOT_TEMP_SOFT_LIMIT);
//			queue_delayed_work(summit_work_queue, &di->summit_monitor_work , msecs_to_jiffies(1));
		}
	}
	if (COLD_TEMP_SOFT_LIMIT_IRQ & value) {
		dev_dbg(di->dev, "Cold Temperature Soft Limit IRQ\n");
		if (COLD_TEMP_SOFT_LIMIT_STATUS & value) {
			dev_dbg(di->dev, "  Event: Over Cold Temperature Soft Limit Status\n");
//			writeIntoCBuffer(&di->events, EVENT_OVER_COLD_TEMP_SOFT_LIMIT);
//			queue_delayed_work(summit_work_queue, &di->summit_monitor_work , msecs_to_jiffies(1));
		} else {
			dev_dbg(di->dev, "  Event: Below Hot Temperature Soft Limit Status\n");
//			writeIntoCBuffer(&di->events, EVENT_BELOW_COLD_TEMP_SOFT_LIMIT);
//			queue_delayed_work(summit_work_queue, &di->summit_monitor_work , msecs_to_jiffies(1));
		}
	}
}

void smb347_read_interrupt_b(struct summit_smb347_info *di) {
	int value = 0;

	value = i2c_smbus_read_byte_data(di->client, SMB347_INTSTAT_REG_B);
	if (BAT_OVER_VOLTAGE_IRQ & value) {
		dev_info(di->dev, "Battery Over-voltage Condition IRQ\n");
		if (BAT_OVER_VOLTAGE_STATUS & value)
			dev_info(di->dev, "  Battery Over-voltage Status\n");
	}
	if (MISSING_BAT_IRQ & value) {
		dev_info(di->dev, "Missing Batter IRQ\n");
		if (MISSING_BAT_STATUS & value)
			dev_info(di->dev, "  Missing Battery\n");
		else
			dev_info(di->dev, "  Not Missing Battery\n");
	}
	if (LOW_BAT_VOLTAGE_IRQ & value) {
		dev_info(di->dev, "Low-battery Voltage IRQ\n");
		if (LOW_BAT_VOLTAGE_STATUS & value) {
			dev_info(di->dev, "  Below Low-battery Voltage\n");
//			write_event_buffer(&di->events, EVENT_BELOW_LOW_BATTERY);
//			queue_delayed_work(summit_work_queue, &di->summit_monitor_work, msecs_to_jiffies(1));
		} else {
			dev_info(di->dev, "  Above Low-battery Voltage\n");
//			write_event_buffer(&di->events, EVENT_OVER_LOW_BATTERY);
//			queue_delayed_work(summit_work_queue, &di->summit_monitor_work, msecs_to_jiffies(1));
		}
	}
	if (PRE_TO_FAST_CHARGE_VOLTAGE_IRQ & value) {
		dev_info(di->dev, "Pre-to-Fast Charge Battery Voltage IRQ\n");
		if (PRE_TO_FAST_CHARGE_VOLTAGE_STATUS & value)
			dev_info(di->dev, "  Pre-to-Fast Charge Battery Voltage Status\n");
	}
}

void smb347_read_interrupt_c(struct summit_smb347_info *di) {
	int value = 0;

	value = i2c_smbus_read_byte_data(di->client, SMB347_INTSTAT_REG_C);
	if (INTERNAL_TEMP_LIMIT_IRQ & value) {
		dev_info(di->dev, "Internal Temperature Limit IRQ\n");
		if (INTERNAL_TEMP_LIMIT_STATUS & value)
			dev_info(di->dev, "  Internal Temperature Limit Status\n");
	}
	if (RE_CHARGE_BAT_THRESHOLD_IRQ & value) {
		dev_info(di->dev, "Re-charge Battery Threshold IRQ\n");
		if (RE_CHARGE_BAT_THRESHOLD_STATUS & value)
			dev_info(di->dev, "  Re-charge Battery Threshold Status\n");
	}
	if (TAPER_CHARGER_MODE_IRQ & value) {
		dev_info(di->dev, "Taper Charging Mode IRQ\n");
		if (TAPER_CHARGER_MODE_STATUS & value)
			dev_info(di->dev, "  Taper Charging Mode Status\n");
	}
	if (TERMINATION_CC_IRQ & value) {
		dev_info(di->dev, "Termination Charging Current Hit IRQ\n");
		if (TERMINATION_CC_STATUS & value)
			dev_info(di->dev, "  Termination Charging Current Hit Status\n");
	}
}

void smb347_read_interrupt_d(struct summit_smb347_info *di) {
	int value = 0;

	value = i2c_smbus_read_byte_data(di->client, SMB347_INTSTAT_REG_D);
	if (APSD_COMPLETED_IRQ & value) {
		dev_info(di->dev, "APSD Completed IRQ\n");
		if (APSD_COMPLETED_STATUS & value) {
			dev_info(di->dev, "  Event: APSD Completed Status\n");
			smb347_apsd_complete(di);
//			write_event_buffer(&di->events, EVENT_APSD_COMPLETE);
//			queue_delayed_work(summit_work_queue, &di->summit_monitor_work, msecs_to_jiffies(1));
		}
	}
	if (AICL_COMPLETED_IRQ & value) {
		dev_info(di->dev, "AICL Complete IRQ\n");
		if (AICL_COMPLETED_STATUS & value) {
			dev_info(di->dev, "  Event: AICL Complete Status\n");
			smb347_aicl_complete(di);
//			write_event_buffer(&di->events, EVENT_AICL_COMPLETE);
//			queue_delayed_work(summit_work_queue, &di->summit_monitor_work, msecs_to_jiffies(1));
		}
	}
	if (COMPLETE_CHARGE_TIMEOUT_IRQ & value) {
		dev_info(di->dev, "Complete Charge Timeout IRQ\n");
	}
	if (PC_TIMEOUT_IRQ & value) {
		dev_info(di->dev, "Pre-Charge Timeout IRQ\n");
	}
}

void smb347_read_interrupt_e(struct summit_smb347_info *di) {
	int value = 0;

	value = i2c_smbus_read_byte_data(di->client, SMB347_INTSTAT_REG_E);
	if (DCIN_OVER_VOLTAGE_IRQ & value) {
		dev_info(di->dev, "DCIN Over-voltage IRQ\n");
		if (DCIN_OVER_VOLTAGE_STATUS & value)
			dev_info(di->dev, "  DCIN Over-voltage\n");
	}
	if (DCIN_UNDER_VOLTAGE_IRQ & value) {
		dev_info(di->dev, "DCIN Under-voltage IRQ\n");
		if (DCIN_UNDER_VOLTAGE_STATUS & value)
			dev_info(di->dev, "  DCIN Under-voltage\n");
	}

	if (USBIN_OVER_VOLTAGE_IRQ & value) {
		dev_info(di->dev, "USBIN Over-voltage IRQ\n");
		if (USBIN_OVER_VOLTAGE_STATUS & value)
			dev_info(di->dev, "  USBIN Over-voltage\n");
	}
	if (USBIN_UNDER_VOLTAGE_IRQ & value) {
		dev_info(di->dev, "USBIN Under-voltage IRQ\n");
		if (USBIN_UNDER_VOLTAGE_STATUS & value)
			dev_info(di->dev, "  USBIN Under-voltage Status\n");
		else
			dev_info(di->dev, "  USBIN\n");
	}
}

void smb347_read_interrupt_f(struct summit_smb347_info *di) {
	int value = 0;

	value = i2c_smbus_read_byte_data(di->client, SMB347_INTSTAT_REG_F);
	if (OTG_OVER_CURRENT_IRQ & value) {
		dev_info(di->dev, "OTG Over-current Limit IRQ\n");
		if (OTG_OVER_CURRENT_STATUS & value)
			dev_info(di->dev, "  OTG Over-current Limit Status\n");
	}
	if (OTG_BAT_UNDER_VOLTAGE_IRQ & value) {
		dev_info(di->dev, "OTG Battery Under-voltage IRQ\n");
		if (OTG_BAT_UNDER_VOLTAGE_STATUS & value)
			dev_info(di->dev, "  OTG Battery Under-voltage Status\n");
	}
	if (OTG_DETECTION_IRQ & value) {
		dev_info(di->dev, "OTG Detection IRQ\n");
		if (OTG_DETECTION_STATUS & value)
			dev_info(di->dev, "  OTG Detection Status\n");
	}
	if (POWER_OK_IRQ  & value) {
		dev_info(di->dev, "USBIN Under-voltage IRQ\n");
		if (POWER_OK_STATUS & value)
			dev_info(di->dev, "  Power OK Status\n");
	}
}
/* END IRQ FUNCTIONS */

/* SYSFS */
static ssize_t smb347_status_show(struct device *dev ,
	struct device_attribute *attr , char *buf)
{
	struct summit_smb347_info *di = i2c_get_clientdata(to_i2c_client(dev));
	int index = 0;
	int setting = 0;
	for (index = 0 ; index <= 0x0d ; index++) {
		setting = i2c_smbus_read_byte_data(di->client , index);
		dev_info(di->dev, "%s: R%d : setting = 0x%x\n", __func__ ,index, setting);
		mdelay(1);
	}
	return sprintf(buf ,  "1\n");
}

static DEVICE_ATTR(status, S_IRUGO | S_IWUSR | S_IWGRP, smb347_status_show,  NULL);

static struct attribute *smb347_attrs[] = {
	&dev_attr_status.attr,
	NULL
};

static struct attribute_group smb347_attr_grp = {
	.attrs = smb347_attrs ,
};

/* END SYSFS */

/* IRQ worker */
static void summit_smb347_irq_worker(struct work_struct *work) {
	u8 value = 0xff;
	struct summit_smb347_info *di = container_of(work, struct summit_smb347_info, irq_work);

	BUG_ON(!di);
	dev_info(di->dev, "!! SMB347 INTERRUPT !!\n");

	smb347_read_status_interrupt_setting(di);
	mdelay(1);
	smb347_read_fault_interrupt_setting(di);
	mdelay(1);
	smb347_read_interrupt_a(di);
	mdelay(1);
	smb347_read_interrupt_b(di);
	mdelay(1);
	smb347_read_interrupt_c(di);
	mdelay(1);
	smb347_read_interrupt_d(di);
	mdelay(1);
	smb347_read_interrupt_e(di);
	mdelay(1);
	smb347_read_interrupt_f(di);

	return;
}

static irqreturn_t summit_smb347_irq(int irq, void *data)
{
	struct summit_smb347_info *di = (struct summit_smb347_info *)data;

	schedule_work(&di->irq_work);

	return IRQ_HANDLED;
}

/* TWL6030 Callback */
int smb347_twl6030_callback(int vbus_state, int prev_status, void *data) {
	struct summit_smb347_info *di = (struct summit_smb347_info *)data;
	int status = 0;

#ifdef DRIVER_DEBUG
	dev_info(di->dev, "twl6030 notifier: vbus_state           = %s\n", smb347_vbus_id_status_string(vbus_state));
	dev_info(di->dev, "twl6030 notifier: prev_vbus_id_status  = %s\n", smb347_vbus_id_status_string(prev_status));
	dev_info(di->dev, "twl6030 notifier: otg->type            = %s\n", smb347_phy_type_string(di->otg_xceiv->type));
	dev_info(di->dev, "twl6030 notifier: otg->state           = %s\n", smb347_otg_state_string(di->otg_xceiv->state));
	dev_info(di->dev, "twl6030 notifier: otg->last_event      = %s\n", smb347_phy_event_string(di->otg_xceiv->last_event));
#endif

	switch (vbus_state) {
	/* VUSB ON */
	case OMAP_MUSB_VBUS_VALID:
		if ((prev_status == OMAP_MUSB_VBUS_VALID) && (di->usb_vusb_on == 1)) {
			status = -1;
			goto exit;
		}

		//detect by charger
		__raw_writel(0x40000000,OMAP2_L4_IO_ADDRESS(0x4a100000 + (0x620)));

		smb347_redo_apsd(di, 0);
		if (di->usb_vusb_on == 0) {
#if defined(CONFIG_HAS_WAKELOCK)
			wake_lock(&di->usb_wake_lock);
#endif
			di->usb_vusb_on = 1;
			twl6030_usb_ldo_on();
		}
		mdelay(300);
		smb347_redo_apsd(di, 1);
//		di->otg_xceiv->default_a = false;
//		di->otg_xceiv->state = OTG_STATE_B_IDLE;
		status = OMAP_MUSB_VBUS_VALID;
		omap_musb_mailbox(status);
		break;
	/* OTG */
	case OMAP_MUSB_ID_GROUND:
		if (prev_status == OMAP_MUSB_ID_GROUND) {
			status = -1;
			goto exit;
		}
		twl6030_writeb(di, TWL_MODULE_USB, 0x1, USB_ID_INT_EN_HI_CLR);
		twl6030_writeb(di, TWL_MODULE_USB, 0x10, USB_ID_INT_EN_HI_SET);
		smb347_configure_otg(di, 1);
		status = OMAP_MUSB_ID_GROUND;
		omap_musb_mailbox(OMAP_MUSB_ID_GROUND);
		break;
	/* REMOVE OTG */
	case OMAP_MUSB_ID_FLOAT:
		if (prev_status != OMAP_MUSB_ID_GROUND) {
			status = -1;
			goto exit;
		}
		// Turn off LEDS
		if (di->pdata->led_callback)
			di->pdata->led_callback(0,0);
		smb347_configure_otg(di, 0);
		status = OMAP_MUSB_ID_FLOAT;
		omap_musb_mailbox(OMAP_MUSB_ID_FLOAT);
	/* VUSB OFF */
	default:
		if ((prev_status == OMAP_MUSB_VBUS_OFF) && (di->usb_vusb_on == 0)) {
			status = -1;
			goto exit;
		}
		// Turn off LEDS
		if (di->pdata->led_callback)
			di->pdata->led_callback(0,0);
		smb347_switch_mode(di, SMB347_USB_MODE_1);
		if (di->usb_vusb_on) {
			twl6030_usb_ldo_off();
			di->usb_vusb_on = 0;
#if defined(CONFIG_HAS_WAKELOCK)
			wake_lock_timeout(&di->usb_wake_lock, msecs_to_jiffies(2000));
#endif
		}
		status = OMAP_MUSB_VBUS_OFF;
		omap_musb_mailbox(status);
	}
exit:
#ifdef DRIVER_DEBUG
	dev_info(di->dev, "twl6030 notifier: status               = %s\n", smb347_vbus_id_status_string(status));
#endif
	return status;
}

/* First time call for TWL6030 setup */
static void twl6030_work_func(struct work_struct *work) {
	twl6030_vusb_init();
}

/*
Dvt(mbid=4)
->SUSP pin ->UART4_RX/GPIO155
->EN pin   ->C2C_DATA12/GPIO101
->
*/
static int __devinit summit_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct summit_smb347_info *di;
	u8 value, config;
	int ret = 0, chip_id = 0, gpio = -1;
	struct thermal_dev *tdev = NULL;

	di = kzalloc(sizeof(*di) ,  GFP_KERNEL);
	if (!di)
		return -ENOMEM;

	di->pdata = dev_get_platdata(&client->dev);

	tdev = kzalloc(sizeof(struct thermal_dev), GFP_KERNEL);
	if (tdev == NULL){
		kfree(di);
		return -ENOMEM;
	}

	di->client			= client;
	di->dev				= &client->dev;
	di->irq				= client->irq;
	di->protect_enable		= 0;
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

	/* Check for device ID */
	if (smb347_read_id(di, &chip_id) < 0) {
		dev_err(di->dev, "%s: Unable to detect device ID\n",__func__);
		return -ENODEV;
	}
	ret = twl_i2c_read_u8(TWL6030_MODULE_CHARGER, &value, CONTROLLER_STAT1);
	if (value & VBUS_DET) {
		dev_info(di->dev , "USBIN in use\n");
	} else {
		smb347_config(di, 1);
		config = i2c_smbus_read_byte_data(di->client, SMB347_STAT_TIMERS);
		CLEAR_BIT(config, 5);
		i2c_smbus_write_byte_data(di->client, SMB347_STAT_TIMERS, config);
		smb347_config(di, 0);
	}

	if (di->pdata->charger_setting) {
		smb347_charger_reconfig(di);
		mdelay(10);
	}

	dev_info(di->dev, "Summit SMB347 detected, chip_id=0x%x board id=%d\n", chip_id, di->pdata->mbid);

#ifdef CONFIG_THERMAL_FRAMEWORK
	memcpy(tdev, &case_thermal_dev, sizeof(struct thermal_dev));
	tdev->dev = di->dev;
	ret = thermal_cooling_dev_register(tdev);
	di->tdev = tdev;
#endif

	/* Set up mutex */
	mutex_init(&di->lock);

	di->otg_xceiv = usb_get_phy(USB_PHY_TYPE_USB2);
	wake_lock_init(&di->usb_wake_lock, WAKE_LOCK_SUSPEND, "smb347_usb_wake_lock");

	di->usb.name		= "usb";
	di->usb.type		= POWER_SUPPLY_TYPE_USB;
	di->usb.properties	= summit_usb_props;
	di->usb.num_properties	= ARRAY_SIZE(summit_usb_props);
	di->usb.get_property	= summit_usb_get_property;
	if (power_supply_register(di->dev, &di->usb))
		dev_err(di->dev, "failed to register usb power supply\n");

	di->ac.name		= "ac";
	di->ac.type		= POWER_SUPPLY_TYPE_MAINS;
	di->ac.properties	= summit_ac_props;
	di->ac.num_properties	= ARRAY_SIZE(summit_ac_props);
	di->ac.get_property	= summit_ac_get_property;
	if (power_supply_register(di->dev, &di->ac))
		dev_err(di->dev, "failed to register ac power supply\n");

	/* Turn off OTG mode */
	smb347_configure_otg(di, 0);

#if 0
	/* Limit the fast charge current to 1800 mA */
	smb347_fast_charge_current_limit(di);

	/* for thermal logging;
	 * Make sure max_thermal_charge_current has the same value
	 * set by above smb347_fast_charge_current_limit
	 */
	di->max_thermal_charge_current = 1800;

	/* Enable charge enable control via I2C (active low) */
	smb347_set_charge_control(di);

	/* Limit termination current to 150 mA */
	smb347_set_termination_current(di);

	/* Enable Charge-Timeout interrupt */
	smb347_set_charge_timeout_interrupt(di);

#if defined(CONFIG_MACH_OMAP4_BOWSER_SUBTYPE_JEM)
	/* Limit the fast charge current to 1800mA,
	   pre-charge current to 250mA and
	   Terminal current to 250mA by HW requirements */
	smb347_set_current_setting(di);

	/* Set Charge Curretn Comesation to 900mA */
	smb347_set_current_compensation(di);

	/* Limit Hard/Soft temperature to 60/0(Hard), 45/15(Soft) and
	   enable Temerature interrupt */
	smb347_set_temperature_threshold(di);
	smb347_set_temperature_interrupt(di);
#endif
#endif
	/*enable low battery interrupt*/
//	summit_enable_stat_interrupt(di, LOW_BATTERY_TRIGGER_IRQ);

	/* Init work */
	INIT_WORK(&di->irq_work, summit_smb347_irq_worker);
	INIT_DELAYED_WORK(&di->twl6030_work,  twl6030_work_func);

	if (di->pdata->use_irq_as_gpio) {
		gpio = irq_to_gpio(di->irq);
		if (gpio_is_valid(gpio)) {
			gpio_request(gpio, "smb347-irq");
			gpio_direction_input(gpio);
		} else {
			dev_err(di->dev, "Invalid gpio %d\n", gpio);
			di->client->irq = -1;
			di->irq = -1;
		}
	}

	/* Enable interrupt */
	if (di->irq != -1) {
		if (di->pdata->irq_trigger_falling)
			ret = request_threaded_irq(di->irq, NULL, summit_smb347_irq, IRQF_TRIGGER_FALLING, "smb347_irq",  di);
		else
			ret = request_threaded_irq(di->irq, NULL, summit_smb347_irq, IRQF_TRIGGER_RISING, "smb347_irq", di);
		if (ret) {
			dev_err(di->dev, "Unable to set up threaded IRQ\n");
		}
		else if (di->pdata->irq_set_awake)
			irq_set_irq_wake(di->irq , 1);
	}

	ret = sysfs_create_group(&di->dev->kobj, &smb347_attr_grp);
	if (ret) {
		dev_dbg(di->dev , "could not create sysfs_create_group\n");
	}

	/* Initialize the TWL6030 mode */
	twl6030_set_callback(di);
	schedule_delayed_work(&di->twl6030_work, msecs_to_jiffies(1000));

	return 0;
}

static int __devexit summit_remove(struct i2c_client *client) {
	struct summit_smb347_info *di = i2c_get_clientdata(client);

	dev_info(di->dev, "%s\n" , __func__);
	disable_irq(di->irq);
	free_irq(di->irq, di);
#ifdef CONFIG_THERMAL_FRAMEWORK
	thermal_cooling_dev_unregister(di->tdev);
#endif
	sysfs_remove_group(&di->dev->kobj,  &smb347_attr_grp);
	cancel_delayed_work_sync(&di->twl6030_work);
	power_supply_unregister(&di->ac);
	power_supply_unregister(&di->usb);

	kfree(di);
	return 0;
}

static void summit_shutdown(struct i2c_client *client) {
	struct summit_smb347_info *di = i2c_get_clientdata(client);

	dev_info(di->dev ,  "%s\n" ,  __func__);

	disable_irq(di->irq);
	free_irq(di->irq ,  di);
	cancel_delayed_work_sync(&di->twl6030_work);
}

static const struct i2c_device_id summit_smb347_id[] = {
	{ "summit_smb347" ,  0 } ,
	{ } ,
};

MODULE_DEVICE_TABLE(i2c, summit_smb347_id);

static struct i2c_driver summit_i2c_driver = {
	.driver = {
		.name = "summit_smb347",
	},
	.id_table	= summit_smb347_id,
	.probe		= summit_probe,
	.remove		= summit_remove,
	.shutdown	= summit_shutdown,
};

module_i2c_driver(summit_i2c_driver);

MODULE_DESCRIPTION("Summit SMB347Driver");
MODULE_AUTHOR("Eric Nien");
MODULE_LICENSE("GPL");
