/*
 * twl_core.c - driver for TWL4030/TWL5030/TWL60X0/TPS659x0 PM
 * and audio CODEC devices
 *
 * Copyright (C) 2005-2006 Texas Instruments, Inc.
 *
 * Modifications to defer interrupt handling to a kernel thread:
 * Copyright (C) 2006 MontaVista Software, Inc.
 *
 * Based on tlv320aic23.c:
 * Copyright (c) by Kai Svahn <kai.svahn@nokia.com>
 *
 * Code cleanup and modifications to IRQ handler.
 * by syed khasim <x0khasim@ti.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 */

#include <linux/init.h>
#include <linux/mutex.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/err.h>

#include <linux/regulator/machine.h>
#include <linux/regulator/consumer.h>
#include <linux/i2c.h>
#include <linux/i2c/twl.h>

#if defined(CONFIG_ARCH_OMAP2) || defined(CONFIG_ARCH_OMAP3)
#include <plat/cpu.h>
#endif

/*
 * The TWL4030 "Triton 2" is one of a family of a multi-function "Power
 * Management and System Companion Device" chips originally designed for
 * use in OMAP2 and OMAP 3 based systems.  Its control interfaces use I2C,
 * often at around 3 Mbit/sec, including for interrupt handling.
 *
 * This driver core provides genirq support for the interrupts emitted,
 * by the various modules, and exports register access primitives.
 *
 * FIXME this driver currently requires use of the first interrupt line
 * (and associated registers).
 */

#define DRIVER_NAME			"twl"

//#if defined(CONFIG_Q_CHARGER)
#define    twl_has_qcharger()   true
//#else
//#define    twl_has_qcharger()    false
//#endif

#if defined(CONFIG_TWL4030_BCI_BATTERY) || \
	defined(CONFIG_TWL4030_BCI_BATTERY_MODULE) || \
	defined(CONFIG_TWL6030_BCI_BATTERY) || \
	defined(CONFIG_TWL6030_BCI_BATTERY_MODULE)
#define twl_has_bci()		true
#else
#define twl_has_bci()		false
#endif

#if defined(CONFIG_KEYBOARD_TWL4030) || defined(CONFIG_KEYBOARD_TWL4030_MODULE)
#define twl_has_keypad()	true
#else
#define twl_has_keypad()	false
#endif

#if defined(CONFIG_GPIO_TWL4030) || defined(CONFIG_GPIO_TWL4030_MODULE)
#define twl_has_gpio()	true
#else
#define twl_has_gpio()	false
#endif

#if defined(CONFIG_REGULATOR_TWL4030) \
	|| defined(CONFIG_REGULATOR_TWL4030_MODULE)
#define twl_has_regulator()	true
#else
#define twl_has_regulator()	false
#endif

#if defined(CONFIG_TWL4030_MADC) || defined(CONFIG_TWL4030_MADC_MODULE) ||\
    defined(CONFIG_TWL6030_GPADC) || defined(CONFIG_TWL6030_GPADC_MODULE)
#define twl_has_madc()	true
#else
#define twl_has_madc()	false
#endif

#ifdef CONFIG_TWL4030_POWER
#define twl_has_power()        true
#else
#define twl_has_power()        false
#endif

#if defined(CONFIG_RTC_DRV_TWL4030) || defined(CONFIG_RTC_DRV_TWL4030_MODULE)
#define twl_has_rtc()	true
#else
#define twl_has_rtc()	false
#endif

#if defined(CONFIG_TWL4030_USB) || defined(CONFIG_TWL4030_USB_MODULE) ||\
    defined(CONFIG_TWL6030_USB) || defined(CONFIG_TWL6030_USB_MODULE)
#define twl_has_usb()	true
#else
#define twl_has_usb()	false
#endif

#if defined(CONFIG_TWL4030_WATCHDOG) || \
	defined(CONFIG_TWL4030_WATCHDOG_MODULE)
#define twl_has_watchdog()        true
#else
#define twl_has_watchdog()        false
#endif

#if defined(CONFIG_TWL4030_CODEC) || defined(CONFIG_TWL4030_CODEC_MODULE) ||\
	defined(CONFIG_TWL6040_CODEC) || defined(CONFIG_TWL6040_CODEC_MODULE)
#define twl_has_codec()	true
#else
#define twl_has_codec()	false
#endif

/* Triton Core internal information (BEGIN) */

/* Last - for index max*/
#define TWL4030_MODULE_LAST		TWL4030_MODULE_SECURED_REG

#define TWL_NUM_SLAVES		4

#if defined(CONFIG_INPUT_TWL4030_PWRBUTTON) \
	|| defined(CONFIG_INPUT_TWL4030_PWRBUTTON_MODULE)
#define twl_has_pwrbutton()	true
#else
#define twl_has_pwrbutton()	false
#endif

#define SUB_CHIP_ID0 0
#define SUB_CHIP_ID1 1
#define SUB_CHIP_ID2 2
#define SUB_CHIP_ID3 3

#define TWL_MODULE_LAST TWL4030_MODULE_LAST

/* Base Address defns for twl4030_map[] */

/* subchip/slave 0 - USB ID */
#define TWL4030_BASEADD_USB		0x0000

/* subchip/slave 1 - AUD ID */
#define TWL4030_BASEADD_AUDIO_VOICE	0x0000
#define TWL4030_BASEADD_GPIO		0x0098
#define TWL4030_BASEADD_INTBR		0x0085
#define TWL4030_BASEADD_PIH		0x0080
#define TWL4030_BASEADD_TEST		0x004C

/* subchip/slave 2 - AUX ID */
#define TWL4030_BASEADD_INTERRUPTS	0x00B9
#define TWL4030_BASEADD_LED		0x00EE
#define TWL4030_BASEADD_MADC		0x0000
#define TWL4030_BASEADD_MAIN_CHARGE	0x0074
#define TWL4030_BASEADD_PRECHARGE	0x00AA
#define TWL4030_BASEADD_PWM0		0x00F8
#define TWL4030_BASEADD_PWM1		0x00FB
#define TWL4030_BASEADD_PWMA		0x00EF
#define TWL4030_BASEADD_PWMB		0x00F1
#define TWL4030_BASEADD_KEYPAD		0x00D2

#define TWL5031_BASEADD_ACCESSORY	0x0074 /* Replaces Main Charge */
#define TWL5031_BASEADD_INTERRUPTS	0x00B9 /* Different than TWL4030's
						  one */

/* subchip/slave 3 - POWER ID */
#define TWL4030_BASEADD_BACKUP		0x0014
#define TWL4030_BASEADD_INT		0x002E
#define TWL4030_BASEADD_PM_MASTER	0x0036
#define TWL4030_BASEADD_PM_RECEIVER	0x005B
#define TWL4030_BASEADD_RTC		0x001C
#define TWL4030_BASEADD_SECURED_REG	0x0000

/* Triton Core internal information (END) */


/* subchip/slave 0 0x48 - POWER */
#define TWL6030_BASEADD_RTC		0x0000
#define TWL6030_BASEADD_MEM		0x0017
#define TWL6030_BASEADD_PM_MASTER	0x001F
#define TWL6030_BASEADD_PM_SLAVE_MISC	0x0030 /* PM_RECEIVER */
#define TWL6030_BASEADD_PM_MISC		0x00E2
#define TWL6030_BASEADD_PM_PUPD		0x00F0

/* subchip/slave 1 0x49 - FEATURE */
#define TWL6030_BASEADD_USB		0x0000
#define TWL6030_BASEADD_GPADC_CTRL	0x002E
#define TWL6030_BASEADD_AUX		0x0090
#define TWL6030_BASEADD_PWM		0x00BA
#define TWL6030_BASEADD_GASGAUGE	0x00C0
#define TWL6030_BASEADD_PIH		0x00D0
#define TWL6030_BASEADD_CHARGER		0x00E0

/* subchip/slave 2 0x4A - DFT */
#define TWL6030_BASEADD_DIEID		0x00C0

/* subchip/slave 3 0x4B - AUDIO */
#define TWL6030_BASEADD_AUDIO		0x0000
#define TWL6030_BASEADD_RSV		0x0000
#define TWL6030_BASEADD_ZERO		0x0000

/* Few power values */
#define R_CFG_BOOT			0x05
#define R_PROTECT_KEY			0x0E

/* access control values for R_PROTECT_KEY */
#define KEY_UNLOCK1			0xce
#define KEY_UNLOCK2			0xec
#define KEY_LOCK			0x00

/* some fields in R_CFG_BOOT */
#define HFCLK_FREQ_19p2_MHZ		(1 << 0)
#define HFCLK_FREQ_26_MHZ		(2 << 0)
#define HFCLK_FREQ_38p4_MHZ		(3 << 0)
#define HIGH_PERF_SQ			(1 << 3)
#define CK32K_LOWPWR_EN			(1 << 7)

#define CLK32KG_CFG_STATE		0xBE
#define PHOENIX_START_CONDITION		0x1F
#define PHOENIX_MSK_TRANSITION 		0x20
#define PHOENIX_STS_HW_CONDITIONS 	0x21
#define PHOENIX_LAST_TURNOFF_STS 	0x22
#define PHOENIX_SENS_TRANSITION 	0x2A	


#define VMEM_CFG_GRP 0x64
#define VMEM_CFG_TRANS 0x65
#define VMEM_CFG_STATE 0x66
#define VMEM_CFG_VOLTAGE 0x68

#define V1V29_CFG_GRP 0x40
#define V1V29_CFG_TRANS 0x41
#define V1V29_CFG_STATE 0x42
#define V1V29_CFG_VOLTAGE 0x44

#define V2V1_CFG_GRP 0x4c
#define V2V1_CFG_TRANS 0x4d
#define V2V1_CFG_STATE 0x4e
#define V2V1_CFG_VOLTAGE 0x50

#define VMEM_CFG_GRP 0x64
#define VMEM_CFG_TRANS 0x65
#define VMEM_CFG_STATE 0x66
#define VMEM_CFG_VOLTAGE 0x68

#define VAUX1_CFG_GRP 0x84
#define VAUX1_CFG_TRANS 0x85
#define VAUX1_CFG_STATE 0x86
#define VAUX1_CFG_VOLTAGE 0x87

#define VUSB_CFG_GRP 0xa0
#define VUSB_CFG_TRANS 0xa1
#define VUSB_CFG_STATE 0xa2
#define VUSB_CFG_VOLTAGE 0xa3

#define VAUX2_CFG_GRP 0x88
#define VAUX2_CFG_TRANS 0x89
#define VAUX2_CFG_STATE 0x8a
#define VAUX2_CFG_VOLTAGE 0x8b

#define VAUX3_CFG_GRP 0x8c
#define VAUX3_CFG_TRANS 0X8D
#define VAUX3_CFG_STATE 0x8E
#define VAUX3_CFG_VOLTAGE 0x8f

#define VCXIO_CFG_GRP 0x90
#define VCXIO_CFG_TRANS 0x91
#define VCXIO_CFG_STATE 0x92
#define VCXIO_CFG_VOLTAGE 0x93

#define VDAC_CFG_GRP 0x94
#define VDAC_CFG_TRANS 0x95
#define VDAC_CFG_STATE 0x96
#define VDAC_CFG_VOLTAGE 0x97

#define VMMC_CFG_GRP 0x98
#define VMMC_CFG_TRANS 0x99
#define VMMC_CFG_STATE 0x9A
#define VMMC_CFG_VOLTAGE 0x9B

#define VUSIM_CFG_GRP 0xa4
#define VUSIM_CFG_TRANS 0xa5
#define VUSIM_CFG_STATE 0xa6
#define VUSIM_CFG_VOLTAGE 0xa7

/* chip-specific feature flags, for i2c_device_id.driver_data */
#define TWL4030_VAUX2		BIT(0)	/* pre-5030 voltage ranges */
#define TPS_SUBSET		BIT(1)	/* tps659[23]0 have fewer LDOs */
#define TWL5031			BIT(2)  /* twl5031 has different registers */
#define TWL6030_CLASS		BIT(3)	/* TWL6030 class */

/*----------------------------------------------------------------------*/

/* is driver active, bound to a chip? */
static bool inuse;

static unsigned int twl_id;
unsigned int twl_rev(void)
{
	return twl_id;
}
EXPORT_SYMBOL(twl_rev);

/* Structure for each TWL4030/TWL6030 Slave */
struct twl_client {
	struct i2c_client *client;
	u8 address;

	/* max numb of i2c_msg required is for read =2 */
	struct i2c_msg xfer_msg[2];

	/* To lock access to xfer_msg */
	struct mutex xfer_lock;
};

static struct twl_client twl_modules[TWL_NUM_SLAVES];


/* mapping the module id to slave id and base address */
struct twl_mapping {
	unsigned char sid;	/* Slave ID */
	unsigned char base;	/* base address */
};
static struct twl_mapping *twl_map;

static struct twl_mapping twl4030_map[TWL4030_MODULE_LAST + 1] = {
	/*
	 * NOTE:  don't change this table without updating the
	 * <linux/i2c/twl.h> defines for TWL4030_MODULE_*
	 * so they continue to match the order in this table.
	 */

	{ 0, TWL4030_BASEADD_USB },

	{ 1, TWL4030_BASEADD_AUDIO_VOICE },
	{ 1, TWL4030_BASEADD_GPIO },
	{ 1, TWL4030_BASEADD_INTBR },
	{ 1, TWL4030_BASEADD_PIH },
	{ 1, TWL4030_BASEADD_TEST },

	{ 2, TWL4030_BASEADD_KEYPAD },
	{ 2, TWL4030_BASEADD_MADC },
	{ 2, TWL4030_BASEADD_INTERRUPTS },
	{ 2, TWL4030_BASEADD_LED },
	{ 2, TWL4030_BASEADD_MAIN_CHARGE },
	{ 2, TWL4030_BASEADD_PRECHARGE },
	{ 2, TWL4030_BASEADD_PWM0 },
	{ 2, TWL4030_BASEADD_PWM1 },
	{ 2, TWL4030_BASEADD_PWMA },
	{ 2, TWL4030_BASEADD_PWMB },
	{ 2, TWL5031_BASEADD_ACCESSORY },
	{ 2, TWL5031_BASEADD_INTERRUPTS },

	{ 3, TWL4030_BASEADD_BACKUP },
	{ 3, TWL4030_BASEADD_INT },
	{ 3, TWL4030_BASEADD_PM_MASTER },
	{ 3, TWL4030_BASEADD_PM_RECEIVER },
	{ 3, TWL4030_BASEADD_RTC },
	{ 3, TWL4030_BASEADD_SECURED_REG },
};

static struct twl_mapping twl6030_map[] = {
	/*
	 * NOTE:  don't change this table without updating the
	 * <linux/i2c/twl.h> defines for TWL4030_MODULE_*
	 * so they continue to match the order in this table.
	 */
	{ SUB_CHIP_ID1, TWL6030_BASEADD_USB },
	{ SUB_CHIP_ID3, TWL6030_BASEADD_AUDIO },
	{ SUB_CHIP_ID2, TWL6030_BASEADD_DIEID },
	{ SUB_CHIP_ID2, TWL6030_BASEADD_RSV },
	{ SUB_CHIP_ID1, TWL6030_BASEADD_PIH },

	{ SUB_CHIP_ID2, TWL6030_BASEADD_RSV },
	{ SUB_CHIP_ID2, TWL6030_BASEADD_RSV },
	{ SUB_CHIP_ID1, TWL6030_BASEADD_GPADC_CTRL },
	{ SUB_CHIP_ID2, TWL6030_BASEADD_RSV },
	{ SUB_CHIP_ID2, TWL6030_BASEADD_RSV },

	{ SUB_CHIP_ID1, TWL6030_BASEADD_CHARGER },
	{ SUB_CHIP_ID1, TWL6030_BASEADD_GASGAUGE },
	{ SUB_CHIP_ID1, TWL6030_BASEADD_PWM },
	{ SUB_CHIP_ID0, TWL6030_BASEADD_ZERO },
	{ SUB_CHIP_ID1, TWL6030_BASEADD_ZERO },

	{ SUB_CHIP_ID2, TWL6030_BASEADD_ZERO },
	{ SUB_CHIP_ID2, TWL6030_BASEADD_ZERO },
	{ SUB_CHIP_ID2, TWL6030_BASEADD_RSV },
	{ SUB_CHIP_ID2, TWL6030_BASEADD_RSV },
	{ SUB_CHIP_ID2, TWL6030_BASEADD_RSV },
	{ SUB_CHIP_ID0, TWL6030_BASEADD_PM_MASTER },
	{ SUB_CHIP_ID0, TWL6030_BASEADD_PM_SLAVE_MISC },

	{ SUB_CHIP_ID0, TWL6030_BASEADD_RTC },
	{ SUB_CHIP_ID0, TWL6030_BASEADD_MEM },
};
//For proc
#include <linux/proc_fs.h>
#include <asm/uaccess.h>

#include <linux/gpio.h>
//====================================================================================================
#ifdef	CONFIG_PROC_FS
#define	SKELETON_PROC_FILE	    "driver/twl"
#define SKELETON_MAJOR                699
static struct proc_dir_entry *twl_proc_file;
static int index;
static int mod_no;

static ssize_t twl_proc_read(char *page, char **start, off_t off, int count,
			 int *eof, void *data)
{
	
	//printk(KERN_INFO "%s\n",__func__);
	u8 value=0;
	char	*next = page;
	unsigned size = count;
	int t;

	if (off != 0)
		return 0;
	//you can read value from the register of hardwae in here
	twl_i2c_read_u8(mod_no, &value,index);
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
static ssize_t twl_proc_write(struct file *filp, 
		const char *buff,unsigned long len, void *data)
{
	//printk(KERN_INFO "%s\n",__func__);
	u32 reg_val;
    	u8 value=0;
    	struct regulator *vdds_dsi_reg;
	char messages[256], vol[256];
	if (len > 256)
		len = 256;
	if (copy_from_user(messages, buff, len))
		return -EFAULT;

	if ('-' == messages[0]) {
		/* set the register index */
		memcpy(vol, messages+1, len-1);
		index = (int) simple_strtoul(vol, NULL, 16);
		printk(KERN_INFO"%s:set register 0x%x\n",__func__, index);
	}else if('+'==messages[0]) {
		// set the register value 
		memcpy(vol, messages+1, len-1);
		reg_val = (int)simple_strtoul(vol, NULL, 16);
		//you can read value from the register of hardwae in here
		twl_i2c_write_u8(mod_no, reg_val, index);
		printk(KERN_INFO"%s:register 0x%x: set value 0x%x\n",__func__, index, reg_val);
    	} else if ('!' == messages[0]) {
		switch(messages[1]){
			case 'a'://TWL6030_BASEADD_PWM
				mod_no=0x0c;
			break;
			case 'b'://TWL6030_BASEADD_PIH
				mod_no=0x04;
			break;
			case 'c'://TWL6030_BASEADD_PIH
				mod_no=0x04;
			break;
			case 'd'://SUB_CHIP_ID0, TWL6030_BASEADD_ZERO
				mod_no=0x0d;
			break;
			case 'e':// SUB_CHIP_ID1, TWL6030_BASEADD_ZERO 
				mod_no=0x0e;
			break;
			case 'f':// SUB_CHIP_ID2, TWL6030_BASEADD_ZERO
				mod_no=0x0f;
                twl_i2c_read_u8(TWL6030_MODULE_ID0, &value, 0xA0);
                printk("VUSB_CFG_GRP=0%x\n",value);
                twl_i2c_read_u8(TWL6030_MODULE_ID0, &value, 0xA1);
                printk("VUSB_CFG_TRANS=0%x\n",value);
                twl_i2c_read_u8(TWL6030_MODULE_ID0, &value, 0xA2);
                printk("VUSB_CFG_STATE=0%x\n",value);
			break;
            case 'g':
                vdds_dsi_reg = regulator_get(NULL, "usb-phy");    
                if (IS_ERR(vdds_dsi_reg)) {
                    pr_err("Unable to get usb-phy regulator\n");
                }
                regulator_enable(vdds_dsi_reg);
                regulator_put(vdds_dsi_reg);
            break;
            case 'h':
                vdds_dsi_reg = regulator_get(NULL, "usb-phy");
                if (IS_ERR(vdds_dsi_reg)) {
                    pr_err("Unable to get usb-phy regulator\n");
                }
                regulator_disable(vdds_dsi_reg);
                regulator_put(vdds_dsi_reg);
            break;
            case 'i':
                vdds_dsi_reg = regulator_get(NULL, "audio-pwr");
                regulator_disable(vdds_dsi_reg);
                //regulator_set_voltage(vdds_dsi_reg,1200000,1200000);
                regulator_put(vdds_dsi_reg);
            break;

            case 'j':
                vdds_dsi_reg = regulator_get(NULL, "audio-pwr");
                regulator_set_voltage(vdds_dsi_reg,1200000,2800000);
                regulator_put(vdds_dsi_reg);
            break;

            case 'k':
                vdds_dsi_reg = regulator_get(NULL, "audio-pwr");
                regulator_set_voltage(vdds_dsi_reg,2950000,2950000);
                regulator_put(vdds_dsi_reg);
            break;
		}
	}

	return len;
}

static void create_twl_proc_file(void)
{
	twl_proc_file = create_proc_entry(SKELETON_PROC_FILE, SKELETON_MAJOR, NULL);
	if (twl_proc_file) {
		//skeleton_proc_file->data = driver_info; 
		twl_proc_file->read_proc = twl_proc_read; 
		twl_proc_file->write_proc = twl_proc_write; 
	} else
		printk(KERN_ERR "%sproc file create failed!\n",__func__);
}

static void remove_twl_proc_file(void)
{
	printk(KERN_INFO "%s\n",__func__);
	remove_proc_entry(SKELETON_PROC_FILE, NULL);
}
#endif
/*----------------------------------------------------------------------*/

/* Exported Functions */

/**
 * twl_i2c_write - Writes a n bit register in TWL4030/TWL5030/TWL60X0
 * @mod_no: module number
 * @value: an array of num_bytes+1 containing data to write
 * @reg: register address (just offset will do)
 * @num_bytes: number of bytes to transfer
 *
 * IMPORTANT: for 'value' parameter: Allocate value num_bytes+1 and
 * valid data starts at Offset 1.
 *
 * Returns the result of operation - 0 is success
 */
int twl_i2c_write(u8 mod_no, u8 *value, u8 reg, unsigned num_bytes)
{
	int ret;
	int sid;
	struct twl_client *twl;
	struct i2c_msg *msg;

	if (unlikely(mod_no > TWL_MODULE_LAST)) {
		pr_err("%s: invalid module number %d\n", DRIVER_NAME, mod_no);
		return -EPERM;
	}
	sid = twl_map[mod_no].sid;
	twl = &twl_modules[sid];

	if (unlikely(!inuse)) {
		pr_err("%s: client %d is not initialized\n", DRIVER_NAME, sid);
		return -EPERM;
	}
	mutex_lock(&twl->xfer_lock);
	/*
	 * [MSG1]: fill the register address data
	 * fill the data Tx buffer
	 */
	msg = &twl->xfer_msg[0];
	msg->addr = twl->address;
	msg->len = num_bytes + 1;
	msg->flags = 0;
	msg->buf = value;
	/* over write the first byte of buffer with the register address */
	*value = twl_map[mod_no].base + reg;
	ret = i2c_transfer(twl->client->adapter, twl->xfer_msg, 1);
	mutex_unlock(&twl->xfer_lock);

	/* i2c_transfer returns number of messages transferred */
	if (ret != 1) {
		pr_err("%s: i2c_write failed to transfer all messages\n",
			DRIVER_NAME);
		if (ret < 0)
			return ret;
		else
			return -EIO;
	} else {
		return 0;
	}
}
EXPORT_SYMBOL(twl_i2c_write);

/**
 * twl_i2c_read - Reads a n bit register in TWL4030/TWL5030/TWL60X0
 * @mod_no: module number
 * @value: an array of num_bytes containing data to be read
 * @reg: register address (just offset will do)
 * @num_bytes: number of bytes to transfer
 *
 * Returns result of operation - num_bytes is success else failure.
 */
int twl_i2c_read(u8 mod_no, u8 *value, u8 reg, unsigned num_bytes)
{
	int ret;
	u8 val;
	int sid;
	struct twl_client *twl;
	struct i2c_msg *msg;

	if (unlikely(mod_no > TWL_MODULE_LAST)) {
		pr_err("%s: invalid module number %d\n", DRIVER_NAME, mod_no);
		return -EPERM;
	}
	sid = twl_map[mod_no].sid;
	twl = &twl_modules[sid];

	if (unlikely(!inuse)) {
		pr_err("%s: client %d is not initialized\n", DRIVER_NAME, sid);
		return -EPERM;
	}
	mutex_lock(&twl->xfer_lock);
	/* [MSG1] fill the register address data */
	msg = &twl->xfer_msg[0];
	msg->addr = twl->address;
	msg->len = 1;
	msg->flags = 0;	/* Read the register value */
	val = twl_map[mod_no].base + reg;
	msg->buf = &val;
	/* [MSG2] fill the data rx buffer */
	msg = &twl->xfer_msg[1];
	msg->addr = twl->address;
	msg->flags = I2C_M_RD;	/* Read the register value */
	msg->len = num_bytes;	/* only n bytes */
	msg->buf = value;
	ret = i2c_transfer(twl->client->adapter, twl->xfer_msg, 2);
	mutex_unlock(&twl->xfer_lock);

	/* i2c_transfer returns number of messages transferred */
	if (ret != 2) {
		pr_err("%s: i2c_read failed to transfer all messages\n",
			DRIVER_NAME);
		if (ret < 0)
			return ret;
		else
			return -EIO;
	} else {
		return 0;
	}
}
EXPORT_SYMBOL(twl_i2c_read);

/**
 * twl_i2c_write_u8 - Writes a 8 bit register in TWL4030/TWL5030/TWL60X0
 * @mod_no: module number
 * @value: the value to be written 8 bit
 * @reg: register address (just offset will do)
 *
 * Returns result of operation - 0 is success
 */
int twl_i2c_write_u8(u8 mod_no, u8 value, u8 reg)
{

	/* 2 bytes offset 1 contains the data offset 0 is used by i2c_write */
	u8 temp_buffer[2] = { 0 };
	/* offset 1 contains the data */
	temp_buffer[1] = value;
	return twl_i2c_write(mod_no, temp_buffer, reg, 1);
}
EXPORT_SYMBOL(twl_i2c_write_u8);

/**
 * twl_i2c_read_u8 - Reads a 8 bit register from TWL4030/TWL5030/TWL60X0
 * @mod_no: module number
 * @value: the value read 8 bit
 * @reg: register address (just offset will do)
 *
 * Returns result of operation - 0 is success
 */
int twl_i2c_read_u8(u8 mod_no, u8 *value, u8 reg)
{
	return twl_i2c_read(mod_no, value, reg, 1);
}
EXPORT_SYMBOL(twl_i2c_read_u8);

/*----------------------------------------------------------------------*/

static struct device *
add_numbered_child(unsigned chip, const char *name, int num,
		void *pdata, unsigned pdata_len,
		bool can_wakeup, int irq0, int irq1)
{
	struct platform_device	*pdev;
	struct twl_client	*twl = &twl_modules[chip];
	int			status;

	pdev = platform_device_alloc(name, num);
	if (!pdev) {
		dev_dbg(&twl->client->dev, "can't alloc dev\n");
		status = -ENOMEM;
		goto err;
	}

	device_init_wakeup(&pdev->dev, can_wakeup);
	pdev->dev.parent = &twl->client->dev;

	if (pdata) {
		status = platform_device_add_data(pdev, pdata, pdata_len);
		if (status < 0) {
			dev_dbg(&pdev->dev, "can't add platform_data\n");
			goto err;
		}
	}

	if (irq0) {
		struct resource r[2] = {
			{ .start = irq0, .flags = IORESOURCE_IRQ, },
			{ .start = irq1, .flags = IORESOURCE_IRQ, },
		};

		status = platform_device_add_resources(pdev, r, irq1 ? 2 : 1);
		if (status < 0) {
			dev_dbg(&pdev->dev, "can't add irqs\n");
			goto err;
		}
	}
	status = platform_device_add(pdev);

err:
	if (status < 0) {
		platform_device_put(pdev);
		dev_err(&twl->client->dev, "can't add %s dev\n", name);
		return ERR_PTR(status);
	}
	return &pdev->dev;
}

static inline struct device *add_child(unsigned chip, const char *name,
		void *pdata, unsigned pdata_len,
		bool can_wakeup, int irq0, int irq1)
{
	return add_numbered_child(chip, name, -1, pdata, pdata_len,
		can_wakeup, irq0, irq1);
}

static struct device *
add_regulator_linked(int num, struct regulator_init_data *pdata,
		struct regulator_consumer_supply *consumers,
		unsigned num_consumers)
{
	unsigned sub_chip_id;
	/* regulator framework demands init_data ... */
	if (!pdata)
		return NULL;

	if (consumers) {
		pdata->consumer_supplies = consumers;
		pdata->num_consumer_supplies = num_consumers;
	}

	/* NOTE:  we currently ignore regulator IRQs, e.g. for short circuits */
	sub_chip_id = twl_map[TWL_MODULE_PM_MASTER].sid;
	return add_numbered_child(sub_chip_id, "twl_reg", num,
		pdata, sizeof(*pdata), false, 0, 0);
}

static struct device *
add_regulator(int num, struct regulator_init_data *pdata)
{
	return add_regulator_linked(num, pdata, NULL, 0);
}

/*
 * NOTE:  We know the first 8 IRQs after pdata->base_irq are
 * for the PIH, and the next are for the PWR_INT SIH, since
 * that's how twl_init_irq() sets things up.
 */

static int
add_children(struct twl4030_platform_data *pdata, unsigned long features)
{
	struct device	*child;
	unsigned sub_chip_id;

	if (twl_has_gpio() && pdata->gpio) {
		child = add_child(SUB_CHIP_ID1, "twl4030_gpio",
				pdata->gpio, sizeof(*pdata->gpio),
				false, pdata->irq_base + GPIO_INTR_OFFSET, 0);
		if (IS_ERR(child))
			return PTR_ERR(child);
	}

	if (twl_has_keypad() && pdata->keypad) {
		child = add_child(SUB_CHIP_ID2, "twl4030_keypad",
				pdata->keypad, sizeof(*pdata->keypad),
				true, pdata->irq_base + KEYPAD_INTR_OFFSET, 0);
		if (IS_ERR(child))
			return PTR_ERR(child);
	}
		if (twl_has_bci() && pdata->bci &&
			!(features & (TPS_SUBSET | TWL5031))) {
			child = add_child(3, "twl4030_bci",
			pdata->bci, sizeof(*pdata->bci),
			false,
			/* irq0 = CHG_PRES, irq1 = BCI */
			pdata->irq_base + BCI_PRES_INTR_OFFSET,
			pdata->irq_base + BCI_INTR_OFFSET);
			if (IS_ERR(child))
				return PTR_ERR(child);
		}
	if (twl_has_bci() && pdata->bci &&
	    (features & TWL6030_CLASS)) {
		child = add_child(1, "twl6030_bci",
				pdata->bci, sizeof(*pdata->bci),
				false,
				pdata->irq_base + CHARGER_INTR_OFFSET,
				pdata->irq_base + CHARGERFAULT_INTR_OFFSET);
	}


	if (twl_has_madc() && pdata->madc && twl_class_is_4030()) {
		child = add_child(2, "twl4030_madc",
				pdata->madc, sizeof(*pdata->madc),
				true, pdata->irq_base + MADC_INTR_OFFSET, 0);
		if (IS_ERR(child))
			return PTR_ERR(child);
	}

	if (twl_has_madc() && pdata->madc && twl_class_is_6030()) {
		child = add_child(1, "twl6030_gpadc",
				pdata->madc, sizeof(*pdata->madc),
				true, pdata->irq_base + MADC_INTR_OFFSET,
				pdata->irq_base + GPADCSW_INTR_OFFSET);
		if (IS_ERR(child))
			return PTR_ERR(child);
	}

	if (twl_has_rtc()) {
		/*
		 * REVISIT platform_data here currently might expose the
		 * "msecure" line ... but for now we just expect board
		 * setup to tell the chip "it's always ok to SET_TIME".
		 * Eventually, Linux might become more aware of such
		 * HW security concerns, and "least privilege".
		 */
		sub_chip_id = twl_map[TWL_MODULE_RTC].sid;
		child = add_child(sub_chip_id, "twl_rtc",
				NULL, 0,
				true, pdata->irq_base + RTC_INTR_OFFSET, 0);
		if (IS_ERR(child))
			return PTR_ERR(child);
	}

	if (twl_has_usb() && pdata->usb && twl_class_is_4030()) {

		static struct regulator_consumer_supply usb1v5 = {
			.supply =	"usb1v5",
		};
		static struct regulator_consumer_supply usb1v8 = {
			.supply =	"usb1v8",
		};
		static struct regulator_consumer_supply usb3v1 = {
			.supply =	"usb3v1",
		};

	/* First add the regulators so that they can be used by transceiver */
		if (twl_has_regulator()) {
			/* this is a template that gets copied */
			struct regulator_init_data usb_fixed = {
				.constraints.valid_modes_mask =
					REGULATOR_MODE_NORMAL
					| REGULATOR_MODE_STANDBY,
				.constraints.valid_ops_mask =
					REGULATOR_CHANGE_MODE
					| REGULATOR_CHANGE_STATUS,
			};

			child = add_regulator_linked(TWL4030_REG_VUSB1V5,
						      &usb_fixed, &usb1v5, 1);
			if (IS_ERR(child))
				return PTR_ERR(child);

			child = add_regulator_linked(TWL4030_REG_VUSB1V8,
						      &usb_fixed, &usb1v8, 1);
			if (IS_ERR(child))
				return PTR_ERR(child);

			child = add_regulator_linked(TWL4030_REG_VUSB3V1,
						      &usb_fixed, &usb3v1, 1);
			if (IS_ERR(child))
				return PTR_ERR(child);

		}

		child = add_child(0, "twl4030_usb",
				pdata->usb, sizeof(*pdata->usb),
				true,
				/* irq0 = USB_PRES, irq1 = USB */
				pdata->irq_base + USB_PRES_INTR_OFFSET,
				pdata->irq_base + USB_INTR_OFFSET);

		if (IS_ERR(child))
			return PTR_ERR(child);

		/* we need to connect regulators to this transceiver */
		if (twl_has_regulator() && child) {
			usb1v5.dev = child;
			usb1v8.dev = child;
			usb3v1.dev = child;
		}
	}
	if (twl_has_watchdog()) {
		child = add_child(0, "twl4030_wdt", NULL, 0, false, 0, 0);
		if (IS_ERR(child))
			return PTR_ERR(child);
	}

	if (twl_has_pwrbutton()) {
		child = add_child(1, "twl4030_pwrbutton",NULL, 0, true, pdata->irq_base + PWR_INTR_OFFSET, 0);
				//NULL, 0, true, pdata->irq_base + 8 + 0, 0);
		if (IS_ERR(child))
			return PTR_ERR(child);
	}
	if (twl_has_codec() && pdata->codec && twl_class_is_4030()) {
		sub_chip_id = twl_map[TWL_MODULE_AUDIO_VOICE].sid;
		child = add_child(sub_chip_id, "twl4030-audio",
				pdata->codec, sizeof(*pdata->codec),
				false, 0, 0);
		if (IS_ERR(child))
			return PTR_ERR(child);
	}

	/* Phoenix codec driver is probed directly atm */
	if (twl_has_codec() && pdata->codec && twl_class_is_6030()) {
		sub_chip_id = twl_map[TWL_MODULE_AUDIO_VOICE].sid;
		child = add_child(sub_chip_id, "twl6040_audio",
				pdata->codec, sizeof(*pdata->codec),
				false, 0, 0);
		if (IS_ERR(child))
			return PTR_ERR(child);
	}

	/* twl4030 regulators */
	if (twl_has_regulator() && twl_class_is_4030()) {
		child = add_regulator(TWL4030_REG_VPLL1, pdata->vpll1);
		if (IS_ERR(child))
			return PTR_ERR(child);

		child = add_regulator(TWL4030_REG_VIO, pdata->vio);
		if (IS_ERR(child))
			return PTR_ERR(child);

		child = add_regulator(TWL4030_REG_VDD1, pdata->vdd1);
		if (IS_ERR(child))
			return PTR_ERR(child);

		child = add_regulator(TWL4030_REG_VDD2, pdata->vdd2);
		if (IS_ERR(child))
			return PTR_ERR(child);

		child = add_regulator(TWL4030_REG_VMMC1, pdata->vmmc1);
		if (IS_ERR(child))
			return PTR_ERR(child);

		child = add_regulator(TWL4030_REG_VDAC, pdata->vdac);
		if (IS_ERR(child))
			return PTR_ERR(child);

		child = add_regulator((features & TWL4030_VAUX2)
					? TWL4030_REG_VAUX2_4030
					: TWL4030_REG_VAUX2,
				pdata->vaux2);
		if (IS_ERR(child))
			return PTR_ERR(child);

		child = add_regulator(TWL4030_REG_VINTANA1, pdata->vintana1);
		if (IS_ERR(child))
			return PTR_ERR(child);

		child = add_regulator(TWL4030_REG_VINTANA2, pdata->vintana2);
		if (IS_ERR(child))
			return PTR_ERR(child);

		child = add_regulator(TWL4030_REG_VINTDIG, pdata->vintdig);
		if (IS_ERR(child))
			return PTR_ERR(child);
	}

	/* maybe add LDOs that are omitted on cost-reduced parts */
	if (twl_has_regulator() && !(features & TPS_SUBSET)
	  && twl_class_is_4030()) {
		child = add_regulator(TWL4030_REG_VPLL2, pdata->vpll2);
		if (IS_ERR(child))
			return PTR_ERR(child);

		child = add_regulator(TWL4030_REG_VMMC2, pdata->vmmc2);
		if (IS_ERR(child))
			return PTR_ERR(child);

		child = add_regulator(TWL4030_REG_VSIM, pdata->vsim);
		if (IS_ERR(child))
			return PTR_ERR(child);

		child = add_regulator(TWL4030_REG_VAUX1, pdata->vaux1);
		if (IS_ERR(child))
			return PTR_ERR(child);

		child = add_regulator(TWL4030_REG_VAUX3, pdata->vaux3);
		if (IS_ERR(child))
			return PTR_ERR(child);

		child = add_regulator(TWL4030_REG_VAUX4, pdata->vaux4);
		if (IS_ERR(child))
			return PTR_ERR(child);

		child = add_regulator(TWL6030_REG_CLK32KG, pdata->clk32kg);
		if (IS_ERR(child))
			return PTR_ERR(child);
	}

	/* twl6030 regulators */
	if (twl_has_regulator() && twl_class_is_6030()) {
		child = add_regulator(TWL6030_REG_VMMC, pdata->vmmc);
		if (IS_ERR(child))
			return PTR_ERR(child);

		child = add_regulator(TWL6030_REG_VPP, pdata->vpp);
		if (IS_ERR(child))
			return PTR_ERR(child);

		child = add_regulator(TWL6030_REG_VUSIM, pdata->vusim);
		if (IS_ERR(child))
			return PTR_ERR(child);

		child = add_regulator(TWL6030_REG_VANA, pdata->vana);
		if (IS_ERR(child))
			return PTR_ERR(child);

		child = add_regulator(TWL6030_REG_VCXIO, pdata->vcxio);
		if (IS_ERR(child))
			return PTR_ERR(child);

		child = add_regulator(TWL6030_REG_VDAC, pdata->vdac);
		if (IS_ERR(child))
			return PTR_ERR(child);

		child = add_regulator(TWL6030_REG_VUSB, pdata->vusb);
		if (IS_ERR(child))
			return PTR_ERR(child);

		child = add_regulator(TWL6030_REG_VAUX1_6030, pdata->vaux1);
		if (IS_ERR(child))
			return PTR_ERR(child);

		child = add_regulator(TWL6030_REG_VAUX2_6030, pdata->vaux2);
		if (IS_ERR(child))
			return PTR_ERR(child);

		child = add_regulator(TWL6030_REG_VAUX3_6030, pdata->vaux3);
		if (IS_ERR(child))
			return PTR_ERR(child);
	}

	if (twl_has_usb() && twl_class_is_6030()) {
		child = add_child(0, "twl6030_usb",
			pdata->usb, sizeof(*pdata->usb),
			true,
			/* irq1 = VBUS_PRES, irq0 = USB ID */
			pdata->irq_base + USBOTG_INTR_OFFSET,
			pdata->irq_base + USB_PRES_INTR_OFFSET);

		if (IS_ERR(child))
			return PTR_ERR(child);
	}
	return 0;
}

/*----------------------------------------------------------------------*/

/*
 * These three functions initialize the on-chip clock framework,
 * letting it generate the right frequencies for USB, MADC, and
 * other purposes.
 */
static inline int __init protect_pm_master(void)
{
	int e = 0;

	e = twl_i2c_write_u8(TWL_MODULE_PM_MASTER, KEY_LOCK,
			R_PROTECT_KEY);
	return e;
}

static inline int __init unprotect_pm_master(void)
{
	int e = 0;

	e |= twl_i2c_write_u8(TWL_MODULE_PM_MASTER, KEY_UNLOCK1,
			R_PROTECT_KEY);
	e |= twl_i2c_write_u8(TWL_MODULE_PM_MASTER, KEY_UNLOCK2,
			R_PROTECT_KEY);
	return e;
}

static void clocks_init(struct device *dev,
			struct twl4030_clock_init_data *clock)
{
	int e = 0;
	struct clk *osc;
	u32 rate;
	u8 ctrl = HFCLK_FREQ_26_MHZ;

#if defined(CONFIG_ARCH_OMAP2) || defined(CONFIG_ARCH_OMAP3)
	if (cpu_is_omap2430())
		osc = clk_get(dev, "osc_ck");
	else
		osc = clk_get(dev, "osc_sys_ck");

	if (IS_ERR(osc)) {
		printk(KERN_WARNING "Skipping twl internal clock init and "
				"using bootloader value (unknown osc rate)\n");
		return;
	}

	rate = clk_get_rate(osc);
	clk_put(osc);

#else
	/* REVISIT for non-OMAP systems, pass the clock rate from
	 * board init code, using platform_data.
	 */
	osc = ERR_PTR(-EIO);

	printk(KERN_WARNING "Skipping twl internal clock init and "
	       "using bootloader value (unknown osc rate)\n");

	return;
#endif

	switch (rate) {
	case 19200000:
		ctrl = HFCLK_FREQ_19p2_MHZ;
		break;
	case 26000000:
		ctrl = HFCLK_FREQ_26_MHZ;
		break;
	case 38400000:
		ctrl = HFCLK_FREQ_38p4_MHZ;
		break;
	}

	ctrl |= HIGH_PERF_SQ;
	if (clock && clock->ck32k_lowpwr_enable)
		ctrl |= CK32K_LOWPWR_EN;

	e |= unprotect_pm_master();
	/* effect->MADC+USB ck en */
	e |= twl_i2c_write_u8(TWL_MODULE_PM_MASTER, ctrl, R_CFG_BOOT);
	e |= protect_pm_master();

	if (e < 0)
		pr_err("%s: clock init err [%d]\n", DRIVER_NAME, e);
}

/*----------------------------------------------------------------------*/

static int __devexit twl_remove(struct i2c_client *client)
{
	unsigned i;
	int status;

	if (twl_class_is_4030())
		status = twl4030_exit_irq();
	else
		status = twl6030_exit_irq();

	if (status < 0)
		return status;

	for (i = 0; i < TWL_NUM_SLAVES; i++) {
		struct twl_client	*twl = &twl_modules[i];

		if (twl->client && twl->client != client)
			i2c_unregister_device(twl->client);
		twl_modules[i].client = NULL;
	}
	inuse = false;
	return 0;
}

static void _init_twl6030_settings(void)
{
	/* USB_VBUS_CTRL_CLR */
	twl_i2c_write_u8(TWL6030_MODULE_ID1, 0xFF, 0x05);
	/* USB_ID_CRTL_CLR */
	twl_i2c_write_u8(TWL6030_MODULE_ID1, 0xFF, 0x07);

	/* GPADC_CTRL */
	twl_i2c_write_u8(TWL6030_MODULE_ID1, 0x00, 0x2E);
	/* TOGGLE1 */
	twl_i2c_write_u8(TWL6030_MODULE_ID1, 0x51, 0x90);
	/* MISC1 */
	twl_i2c_write_u8(TWL6030_MODULE_ID0, 0x00, 0xE4);
	/* MISC2 */
	twl_i2c_write_u8(TWL6030_MODULE_ID0, 0x00, 0xE5);

	/*
	 * BBSPOR_CFG - Disable BB charging. It should be
	 * taken care by proper driver
	 */
	twl_i2c_write_u8(TWL6030_MODULE_ID0, 0x62, 0xE6);
	/* CFG_INPUT_PUPD2 */
	twl_i2c_write_u8(TWL6030_MODULE_ID0, 0x65, 0xF1);
	/* CFG_INPUT_PUPD4 */
	twl_i2c_write_u8(TWL6030_MODULE_ID0, 0x00, 0xF3);
	/* CFG_LDO_PD2 */
	twl_i2c_write_u8(TWL6030_MODULE_ID0, 0x00, 0xF5);
	/* CHARGERUSB_CTRL3 */
	twl_i2c_write_u8(TWL6030_MODULE_ID1, 0x21, 0xEA);

	/* SYSEN_CFG_TRANS */
	twl_i2c_write_u8(TWL6030_MODULE_ID0, 0x01, 0xB3);
	twl_i2c_write_u8(TWL6030_MODULE_ID0, 0x01, 0xB4);
	twl_i2c_write_u8(TWL6030_MODULE_ID0, 0x20, 0xB5);

	/* TMP */
	twl_i2c_write_u8(TWL6030_MODULE_ID0, 0x01, 0xCE);
	twl_i2c_write_u8(TWL6030_MODULE_ID0, 0x01, 0xCF);
	twl_i2c_write_u8(TWL6030_MODULE_ID0, 0x20, 0xD0);

	/* VBATMIN_HI */
	twl_i2c_write_u8(TWL6030_MODULE_ID0, 0x00, 0xC9);

	/* Set DEVOFF, MOD/CON_ACT2OFF/SLP2OFF transition */
	twl_i2c_write_u8(TWL6030_MODULE_ID0, 0x06, 0x25);
}

#ifdef CONFIG_PM
static int
twl_suspend(struct i2c_client *client, pm_message_t message)
{
	/* Make sure below init twl settings are not left on */
	if (twl_class_is_6030())
		_init_twl6030_settings();

	return 0;
}

static int
twl_resume(struct i2c_client *client)
{
	return 0;
}
#else
#define twl_suspend NULL
#define twl_resume NULL
#endif

/* NOTE:  this driver only handles a single twl4030/tps659x0 chip */
static int __devinit
twl_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	int				status;
	unsigned			i;
	struct twl4030_platform_data	*pdata = client->dev.platform_data;
	u8 temp = 0;
	u8 twl_reg;

	if (!pdata) {
		dev_dbg(&client->dev, "no platform data?\n");
		return -EINVAL;
	}

	if (i2c_check_functionality(client->adapter, I2C_FUNC_I2C) == 0) {
		dev_dbg(&client->dev, "can't talk I2C?\n");
		return -EIO;
	}

	if (inuse) {
		dev_dbg(&client->dev, "driver is already in use\n");
		return -EBUSY;
	}

	for (i = 0; i < TWL_NUM_SLAVES; i++) {
		struct twl_client	*twl = &twl_modules[i];

		twl->address = client->addr + i;

		if (i == 0)
			twl->client = client;
		else {
			twl->client = i2c_new_dummy(client->adapter,
					twl->address);
			if (!twl->client) {
				dev_err(&client->dev,
					"can't attach client %d\n", i);
				status = -ENOMEM;
				goto fail;
			}
		}
		mutex_init(&twl->xfer_lock);
	}
	inuse = true;
	if ((id->driver_data) & TWL6030_CLASS) {
		twl_id = TWL6030_CLASS_ID;
		twl_map = &twl6030_map[0];
	} else {
		twl_id = TWL4030_CLASS_ID;
		twl_map = &twl4030_map[0];
	}

	/* setup clock framework */
	clocks_init(&client->dev, pdata->clock);

	/* load power event scripts */
	if (twl_has_power()) {
		twl4030_power_sr_init();
		 if (pdata->power)
			twl4030_power_init(pdata->power);
	}

	/* Maybe init the T2 Interrupt subsystem */
	if (client->irq
			&& pdata->irq_base
			&& pdata->irq_end > pdata->irq_base) {
		if (twl_class_is_4030()) {
			twl4030_init_chip_irq(id->name);
			status = twl4030_init_irq(client->irq, pdata->irq_base,
			pdata->irq_end);
		} else {
			status = twl6030_init_irq(client->irq, pdata->irq_base,
			pdata->irq_end);
		}

		if (status < 0)
			goto fail;
	}

	/* Disable TWL4030/TWL5030 I2C Pull-up on I2C1 and I2C4(SR) interface.
	 * Program I2C_SCL_CTRL_PU(bit 0)=0, I2C_SDA_CTRL_PU (bit 2)=0,
	 * SR_I2C_SCL_CTRL_PU(bit 4)=0 and SR_I2C_SDA_CTRL_PU(bit 6)=0.
	 */

	if (twl_class_is_4030()) {
		twl_i2c_read_u8(TWL4030_MODULE_INTBR, &temp, REG_GPPUPDCTR1);
		temp &= ~(SR_I2C_SDA_CTRL_PU | SR_I2C_SCL_CTRL_PU | \
		I2C_SDA_CTRL_PU | I2C_SCL_CTRL_PU);
		twl_i2c_write_u8(TWL4030_MODULE_INTBR, temp, REG_GPPUPDCTR1);
	}

	if (twl_class_is_6030()) {

		/* Print some useful registers at boot up */
		twl_i2c_read_u8(TWL6030_MODULE_ID0, &twl_reg, PHOENIX_START_CONDITION);
		printk(KERN_INFO "TWL6030: Start condition(PHOENIX_START_CONDITION) is 0x%02x\n", twl_reg);
		twl_i2c_read_u8(TWL6030_MODULE_ID0, &twl_reg, PHOENIX_LAST_TURNOFF_STS);
		printk(KERN_INFO "TWL6030: Last turn off status (PHOENIX_LAST_TURN_OFF_STATUS) is 0x%02x\n", twl_reg);	
		twl_i2c_read_u8(TWL6030_MODULE_ID0, &twl_reg, PHOENIX_STS_HW_CONDITIONS);
		printk(KERN_INFO "TWL6030: Hardware Conditions (PHOENIX_STS_HW_CONDITIONS) is 0x%02x\n", twl_reg);	


		twl_i2c_write_u8(TWL6030_MODULE_ID0, 0xE1, CLK32KG_CFG_STATE);

		twl_i2c_write_u8(TWL6030_MODULE_ID0, 0x0,VMEM_CFG_GRP);
		//kc1_setting
    		//rtc off mode low power,BBSPOR_CFG,VRTC_EN_OFF_STS
    		twl_i2c_write_u8(TWL6030_MODULE_ID0, 0x72,0xe6);

    		twl_i2c_write_u8(TWL6030_MODULE_ID0, 0X0,VMEM_CFG_GRP);
    		twl_i2c_write_u8(TWL6030_MODULE_ID0, 0x0,VMEM_CFG_STATE);
    		twl_i2c_write_u8(TWL6030_MODULE_ID0, 0x0,VMEM_CFG_TRANS);

    		twl_i2c_write_u8(TWL6030_MODULE_ID0, 0xC0, PHOENIX_MSK_TRANSITION);
    		twl_i2c_write_u8(TWL6030_MODULE_ID0, 0x01, VCXIO_CFG_TRANS);

    		twl_i2c_write_u8(TWL6030_MODULE_ID0, 0x0,VDAC_CFG_GRP);
    		twl_i2c_write_u8(TWL6030_MODULE_ID0, 0x0,VDAC_CFG_TRANS);
    		twl_i2c_write_u8(TWL6030_MODULE_ID0, 0x0,VDAC_CFG_STATE);
    		twl_i2c_write_u8(TWL6030_MODULE_ID0, 0x0,VMMC_CFG_GRP);
    		twl_i2c_write_u8(TWL6030_MODULE_ID0, 0x0,VMMC_CFG_TRANS);
   		twl_i2c_write_u8(TWL6030_MODULE_ID0, 0x0,VMMC_CFG_STATE);

    		twl_i2c_write_u8(TWL6030_MODULE_ID0, 0x0,VAUX3_CFG_GRP);
		twl_i2c_write_u8(TWL6030_MODULE_ID0, 0x0,VAUX3_CFG_STATE);

    		twl_i2c_write_u8(TWL6030_MODULE_ID0,  0x0,VAUX2_CFG_GRP);
   		twl_i2c_write_u8(TWL6030_MODULE_ID0, 0x0,VAUX2_CFG_STATE);
    		twl_i2c_write_u8(TWL6030_MODULE_ID0, 0x9,VAUX2_CFG_VOLTAGE);

    		twl_i2c_write_u8(TWL6030_MODULE_ID0,0x10,0xec);
		twl_i2c_write_u8(TWL6030_MODULE_ID0, 0x1,VUSIM_CFG_GRP);
		twl_i2c_write_u8(TWL6030_MODULE_ID0, 0x21,VUSIM_CFG_STATE);
		twl_i2c_write_u8(TWL6030_MODULE_ID0, 0x15,VUSIM_CFG_VOLTAGE);
  
    		twl_i2c_write_u8(TWL6030_MODULE_ID0, 0x3c,V2V1_CFG_VOLTAGE);
   		 //32k
    		twl_i2c_write_u8(TWL6030_MODULE_ID0, 0x1,CLK32KG_CFG_STATE);

		/* Configuration of VBATMIN_HI_THRESHOLD to 3.45V */
		twl_i2c_write_u8(TWL6030_MODULE_ID0, 0x1B, 0x24);
		twl_i2c_write_u8(TWL6030_MODULE_ID0, 0xE1, 0xCA);
                /*Mask all of pmic charger interrupt*/
		twl_i2c_write_u8(TWL6030_MODULE_CHARGER, 0xf,
						CHARGERUSB_INT_MASK);
                /*Disable the pmic charger*/
		twl_i2c_write_u8(TWL6030_MODULE_CHARGER,0,
						CONTROLLER_CTRL1);
	}
	status = add_children(pdata, id->driver_data);
    create_twl_proc_file();
fail:
	if (status < 0)
		twl_remove(client);
	return status;
}

static const struct i2c_device_id twl_ids[] = {
	{ "twl4030", TWL4030_VAUX2 },	/* "Triton 2" */
	{ "twl5030", 0 },		/* T2 updated */
	{ "twl5031", TWL5031 },		/* TWL5030 updated */
	{ "tps65950", 0 },		/* catalog version of twl5030 */
	{ "tps65930", TPS_SUBSET },	/* fewer LDOs and DACs; no charger */
	{ "tps65920", TPS_SUBSET },	/* fewer LDOs; no codec or charger */
	{ "twl6030", TWL6030_CLASS },	/* "Phoenix power chip" */
	{ /* end of list */ },
};
MODULE_DEVICE_TABLE(i2c, twl_ids);

/* One Client Driver , 4 Clients */
static struct i2c_driver twl_driver = {
	.driver.name	= DRIVER_NAME,
	.id_table	= twl_ids,
	.probe		= twl_probe,
	.suspend	= twl_suspend,
	.resume		= twl_resume,
	.remove		= __devexit_p(twl_remove),
};

static int __init twl_init(void)
{
	return i2c_add_driver(&twl_driver);
}
subsys_initcall(twl_init);

static void __exit twl_exit(void)
{
	i2c_del_driver(&twl_driver);
}
module_exit(twl_exit);

MODULE_AUTHOR("Texas Instruments, Inc.");
MODULE_DESCRIPTION("I2C Core interface for TWL");
MODULE_LICENSE("GPL");
