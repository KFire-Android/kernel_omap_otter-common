/*
 * OMAP4 Bandgap temperature sensor driver
 *
 * Copyright (C) 2011 Texas Instruments Incorporated - http://www.ti.com/
 * Author: J Keerthy <j-keerthy@ti.com>
 * Author: Moiz Sonasath <m-sonasath@ti.com>
 * Couple of fixes, DT and MFD adaptation:
 *   Eduardo Valentin <eduardo.valentin@ti.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 */

#include <linux/module.h>
#include <linux/export.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/platform_device.h>
#include <linux/kernel.h>
#include <linux/jiffies.h>
#include <linux/err.h>
#include <linux/types.h>
#include <linux/mutex.h>
#include <linux/delay.h>
#include <linux/reboot.h>
#include <linux/of_platform.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/mfd/omap_control.h>
#include <mach/ctrl_module_core_44xx.h>

#include "omap-bandgap.h"

/* Offsets from the base of temperature sensor registers */

#define OMAP4460_TEMP_SENSOR_CTRL_OFFSET	0x32C
#define OMAP4460_BGAP_CTRL_OFFSET		0x378
#define OMAP4460_BGAP_COUNTER_OFFSET		0x37C
#define OMAP4460_BGAP_THRESHOLD_OFFSET		0x380
#define OMAP4460_BGAP_TSHUT_OFFSET		0x384
#define OMAP4460_BGAP_STATUS_OFFSET		0x388
#define OMAP4460_FUSE_OPP_BGAP			0x260

#define OMAP5430_TEMP_SENSOR_MPU_OFFSET		0x32C
#define OMAP5430_BGAP_CTRL_OFFSET		0x380
#define OMAP5430_BGAP_COUNTER_MPU_OFFSET	0x39C
#define OMAP5430_BGAP_THRESHOLD_MPU_OFFSET	0x384
#define OMAP5430_BGAP_TSHUT_MPU_OFFSET		0x390
#define OMAP5430_BGAP_STATUS_OFFSET		0x3A8
#define OMAP5430_FUSE_OPP_BGAP_MPU		0x1E4

#define OMAP5430_TEMP_SENSOR_GPU_OFFSET		0x330
#define OMAP5430_BGAP_COUNTER_GPU_OFFSET	0x3A0
#define OMAP5430_BGAP_THRESHOLD_GPU_OFFSET	0x388
#define OMAP5430_BGAP_TSHUT_GPU_OFFSET		0x394
#define OMAP5430_FUSE_OPP_BGAP_GPU		0x1E0

#define OMAP5430_TEMP_SENSOR_CORE_OFFSET	0x334
#define OMAP5430_BGAP_COUNTER_CORE_OFFSET	0x3A4
#define OMAP5430_BGAP_THRESHOLD_CORE_OFFSET	0x38C
#define OMAP5430_BGAP_TSHUT_CORE_OFFSET		0x398
#define OMAP5430_FUSE_OPP_BGAP_CORE		0x1E8

#define OMAP4460_TSHUT_HOT		900	/* 122 deg C */
#define OMAP4460_TSHUT_COLD		895	/* 100 deg C */
#define OMAP4460_T_HOT			800	/* 73 deg C */
#define OMAP4460_T_COLD			795	/* 71 deg C */
#define OMAP4460_MAX_FREQ		1500000
#define OMAP4460_MIN_FREQ		1000000
#define OMAP4460_MIN_TEMP		-40000
#define OMAP4460_MAX_TEMP		123000
#define OMAP4460_HYST_VAL		5000
#define OMAP4460_ADC_START_VALUE	530
#define OMAP4460_ADC_END_VALUE		932

#define OMAP5430_MPU_TSHUT_HOT		915
#define OMAP5430_MPU_TSHUT_COLD		900
#define OMAP5430_MPU_T_HOT		800
#define OMAP5430_MPU_T_COLD		795
#define OMAP5430_MPU_MAX_FREQ		1500000
#define OMAP5430_MPU_MIN_FREQ		1000000
#define OMAP5430_MPU_MIN_TEMP		-40000
#define OMAP5430_MPU_MAX_TEMP		125000
#define OMAP5430_MPU_HYST_VAL		5000
#define OMAP5430_ADC_START_VALUE	532
#define OMAP5430_ADC_END_VALUE		934

#define OMAP5430_GPU_TSHUT_HOT		915
#define OMAP5430_GPU_TSHUT_COLD		900
#define OMAP5430_GPU_T_HOT		800
#define OMAP5430_GPU_T_COLD		795
#define OMAP5430_GPU_MAX_FREQ		1500000
#define OMAP5430_GPU_MIN_FREQ		1000000
#define OMAP5430_GPU_MIN_TEMP		-40000
#define OMAP5430_GPU_MAX_TEMP		125000
#define OMAP5430_GPU_HYST_VAL		5000

#define OMAP5430_CORE_TSHUT_HOT		915
#define OMAP5430_CORE_TSHUT_COLD	900
#define OMAP5430_CORE_T_HOT		800
#define OMAP5430_CORE_T_COLD		795
#define OMAP5430_CORE_MAX_FREQ		1500000
#define OMAP5430_CORE_MIN_FREQ		1000000
#define OMAP5430_CORE_MIN_TEMP		-40000
#define OMAP5430_CORE_MAX_TEMP		125000
#define OMAP5430_CORE_HYST_VAL		5000

/* Global bandgap pointer used in PM notifier handlers. */
static struct omap_bandgap *g_bg_ptr;

/* TODO: provide data structures for 4430 */

/*
 * OMAP4460 has one instance of thermal sensor for MPU
 * need to describe the individual bit fields
 */
static struct temp_sensor_registers
omap4460_mpu_temp_sensor_registers = {
	.temp_sensor_ctrl = OMAP4460_TEMP_SENSOR_CTRL_OFFSET,
	.bgap_tempsoff_mask = OMAP4460_BGAP_TEMPSOFF_MASK,
	.bgap_soc_mask = OMAP4460_BGAP_TEMP_SENSOR_SOC_MASK,
	.bgap_eocz_mask = OMAP4460_BGAP_TEMP_SENSOR_EOCZ_MASK,
	.bgap_dtemp_mask = OMAP4460_BGAP_TEMP_SENSOR_DTEMP_MASK,

	.bgap_mask_ctrl = OMAP4460_BGAP_CTRL_OFFSET,
	.mask_hot_mask = OMAP4460_MASK_HOT_MASK,
	.mask_cold_mask = OMAP4460_MASK_COLD_MASK,

	.bgap_mode_ctrl = OMAP4460_BGAP_CTRL_OFFSET,
	.mode_ctrl_mask = OMAP4460_SINGLE_MODE_MASK,

	.bgap_counter = OMAP4460_BGAP_COUNTER_OFFSET,
	.counter_mask = OMAP4460_COUNTER_MASK,

	.bgap_threshold = OMAP4460_BGAP_THRESHOLD_OFFSET,
	.threshold_thot_mask = OMAP4460_T_HOT_MASK,
	.threshold_tcold_mask = OMAP4460_T_COLD_MASK,

	.tshut_threshold = OMAP4460_BGAP_TSHUT_OFFSET,
	.tshut_hot_mask = OMAP4460_TSHUT_HOT_MASK,
	.tshut_cold_mask = OMAP4460_TSHUT_COLD_MASK,

	.bgap_status = OMAP4460_BGAP_STATUS_OFFSET,
	.status_clean_stop_mask = OMAP4460_CLEAN_STOP_MASK,
	.status_bgap_alert_mask = OMAP4460_BGAP_ALERT_MASK,
	.status_hot_mask = OMAP4460_HOT_FLAG_MASK,
	.status_cold_mask = OMAP4460_COLD_FLAG_MASK,

	.bgap_efuse = OMAP4460_FUSE_OPP_BGAP,
};

/*
 * OMAP4460 has one instance of thermal sensor for MPU
 * need to describe the individual bit fields
 */
static struct temp_sensor_registers
omap5430_mpu_temp_sensor_registers = {
	.temp_sensor_ctrl = OMAP5430_TEMP_SENSOR_MPU_OFFSET,
	.bgap_tempsoff_mask = OMAP5430_BGAP_TEMPSOFF_MASK,
	.bgap_soc_mask = OMAP5430_BGAP_TEMP_SENSOR_SOC_MASK,
	.bgap_eocz_mask = OMAP5430_BGAP_TEMP_SENSOR_EOCZ_MASK,
	.bgap_dtemp_mask = OMAP5430_BGAP_TEMP_SENSOR_DTEMP_MASK,

	.bgap_mask_ctrl = OMAP5430_BGAP_CTRL_OFFSET,
	.mask_hot_mask = OMAP5430_MASK_HOT_MPU_MASK,
	.mask_cold_mask = OMAP5430_MASK_COLD_MPU_MASK,

	.bgap_mode_ctrl = OMAP5430_BGAP_COUNTER_MPU_OFFSET,
	.mode_ctrl_mask = OMAP5430_REPEAT_MODE_MASK,

	.bgap_counter = OMAP5430_BGAP_COUNTER_MPU_OFFSET,
	.counter_mask = OMAP5430_COUNTER_MASK,

	.bgap_threshold = OMAP5430_BGAP_THRESHOLD_MPU_OFFSET,
	.threshold_thot_mask = OMAP5430_T_HOT_MASK,
	.threshold_tcold_mask = OMAP5430_T_COLD_MASK,

	.tshut_threshold = OMAP5430_BGAP_TSHUT_MPU_OFFSET,
	.tshut_hot_mask = OMAP5430_TSHUT_HOT_MASK,
	.tshut_cold_mask = OMAP5430_TSHUT_COLD_MASK,

	.bgap_status = OMAP5430_BGAP_STATUS_OFFSET,
	.status_clean_stop_mask = 0x0,
	.status_bgap_alert_mask = OMAP5430_BGAP_ALERT_MASK,
	.status_hot_mask = OMAP5430_HOT_MPU_FLAG_MASK,
	.status_cold_mask = OMAP5430_COLD_MPU_FLAG_MASK,

	.bgap_efuse = OMAP5430_FUSE_OPP_BGAP_MPU,
};

/*
 * OMAP4460 has one instance of thermal sensor for MPU
 * need to describe the individual bit fields
 */
static struct temp_sensor_registers
omap5430_gpu_temp_sensor_registers = {
	.temp_sensor_ctrl = OMAP5430_TEMP_SENSOR_GPU_OFFSET,
	.bgap_tempsoff_mask = OMAP5430_BGAP_TEMPSOFF_MASK,
	.bgap_soc_mask = OMAP5430_BGAP_TEMP_SENSOR_SOC_MASK,
	.bgap_eocz_mask = OMAP5430_BGAP_TEMP_SENSOR_EOCZ_MASK,
	.bgap_dtemp_mask = OMAP5430_BGAP_TEMP_SENSOR_DTEMP_MASK,

	.bgap_mask_ctrl = OMAP5430_BGAP_CTRL_OFFSET,
	.mask_hot_mask = OMAP5430_MASK_HOT_MM_MASK,
	.mask_cold_mask = OMAP5430_MASK_COLD_MM_MASK,

	.bgap_mode_ctrl = OMAP5430_BGAP_COUNTER_GPU_OFFSET,
	.mode_ctrl_mask = OMAP5430_REPEAT_MODE_MASK,

	.bgap_counter = OMAP5430_BGAP_COUNTER_GPU_OFFSET,
	.counter_mask = OMAP5430_COUNTER_MASK,

	.bgap_threshold = OMAP5430_BGAP_THRESHOLD_GPU_OFFSET,
	.threshold_thot_mask = OMAP5430_T_HOT_MASK,
	.threshold_tcold_mask = OMAP5430_T_COLD_MASK,

	.tshut_threshold = OMAP5430_BGAP_TSHUT_GPU_OFFSET,
	.tshut_hot_mask = OMAP5430_TSHUT_HOT_MASK,
	.tshut_cold_mask = OMAP5430_TSHUT_COLD_MASK,

	.bgap_status = OMAP5430_BGAP_STATUS_OFFSET,
	.status_clean_stop_mask = 0x0,
	.status_bgap_alert_mask = OMAP5430_BGAP_ALERT_MASK,
	.status_hot_mask = OMAP5430_HOT_MM_FLAG_MASK,
	.status_cold_mask = OMAP5430_COLD_MM_FLAG_MASK,

	.bgap_efuse = OMAP5430_FUSE_OPP_BGAP_GPU,
};

/*
 * OMAP4460 has one instance of thermal sensor for MPU
 * need to describe the individual bit fields
 */
static struct temp_sensor_registers
omap5430_core_temp_sensor_registers = {
	.temp_sensor_ctrl = OMAP5430_TEMP_SENSOR_CORE_OFFSET,
	.bgap_tempsoff_mask = OMAP5430_BGAP_TEMPSOFF_MASK,
	.bgap_soc_mask = OMAP5430_BGAP_TEMP_SENSOR_SOC_MASK,
	.bgap_eocz_mask = OMAP5430_BGAP_TEMP_SENSOR_EOCZ_MASK,
	.bgap_dtemp_mask = OMAP5430_BGAP_TEMP_SENSOR_DTEMP_MASK,

	.bgap_mask_ctrl = OMAP5430_BGAP_CTRL_OFFSET,
	.mask_hot_mask = OMAP5430_MASK_HOT_CORE_MASK,
	.mask_cold_mask = OMAP5430_MASK_COLD_CORE_MASK,

	.bgap_mode_ctrl = OMAP5430_BGAP_COUNTER_CORE_OFFSET,
	.mode_ctrl_mask = OMAP5430_REPEAT_MODE_MASK,

	.bgap_counter = OMAP5430_BGAP_COUNTER_CORE_OFFSET,
	.counter_mask = OMAP5430_COUNTER_MASK,

	.bgap_threshold = OMAP5430_BGAP_THRESHOLD_CORE_OFFSET,
	.threshold_thot_mask = OMAP5430_T_HOT_MASK,
	.threshold_tcold_mask = OMAP5430_T_COLD_MASK,

	.tshut_threshold = OMAP5430_BGAP_TSHUT_CORE_OFFSET,
	.tshut_hot_mask = OMAP5430_TSHUT_HOT_MASK,
	.tshut_cold_mask = OMAP5430_TSHUT_COLD_MASK,

	.bgap_status = OMAP5430_BGAP_STATUS_OFFSET,
	.status_clean_stop_mask = 0x0,
	.status_bgap_alert_mask = OMAP5430_BGAP_ALERT_MASK,
	.status_hot_mask = OMAP5430_HOT_CORE_FLAG_MASK,
	.status_cold_mask = OMAP5430_COLD_CORE_FLAG_MASK,

	.bgap_efuse = OMAP5430_FUSE_OPP_BGAP_CORE,
};

/* Thresholds and limits for OMAP4460 MPU temperature sensor */
static struct temp_sensor_data omap4460_mpu_temp_sensor_data = {
	.tshut_hot = OMAP4460_TSHUT_HOT,
	.tshut_cold = OMAP4460_TSHUT_COLD,
	.t_hot = OMAP4460_T_HOT,
	.t_cold = OMAP4460_T_COLD,
	.min_freq = OMAP4460_MIN_FREQ,
	.max_freq = OMAP4460_MAX_FREQ,
	.max_temp = OMAP4460_MAX_TEMP,
	.min_temp = OMAP4460_MIN_TEMP,
	.hyst_val = OMAP4460_HYST_VAL,
	.adc_start_val = OMAP4460_ADC_START_VALUE,
	.adc_end_val = OMAP4460_ADC_END_VALUE,
	.update_int1 = 1000,
	.update_int2 = 2000,
};

/* Thresholds and limits for OMAP5430 MPU temperature sensor */
static struct temp_sensor_data omap5430_mpu_temp_sensor_data = {
	.tshut_hot = OMAP5430_MPU_TSHUT_HOT,
	.tshut_cold = OMAP5430_MPU_TSHUT_COLD,
	.t_hot = OMAP5430_MPU_T_HOT,
	.t_cold = OMAP5430_MPU_T_COLD,
	.min_freq = OMAP5430_MPU_MIN_FREQ,
	.max_freq = OMAP5430_MPU_MAX_FREQ,
	.max_temp = OMAP5430_MPU_MAX_TEMP,
	.min_temp = OMAP5430_MPU_MIN_TEMP,
	.hyst_val = OMAP5430_MPU_HYST_VAL,
	.adc_start_val = OMAP5430_ADC_START_VALUE,
	.adc_end_val = OMAP5430_ADC_END_VALUE,
	.update_int1 = 1000,
	.update_int2 = 2000,
};

/* Thresholds and limits for OMAP5430 GPU temperature sensor */
static struct temp_sensor_data omap5430_gpu_temp_sensor_data = {
	.tshut_hot = OMAP5430_GPU_TSHUT_HOT,
	.tshut_cold = OMAP5430_GPU_TSHUT_COLD,
	.t_hot = OMAP5430_GPU_T_HOT,
	.t_cold = OMAP5430_GPU_T_COLD,
	.min_freq = OMAP5430_GPU_MIN_FREQ,
	.max_freq = OMAP5430_GPU_MAX_FREQ,
	.max_temp = OMAP5430_GPU_MAX_TEMP,
	.min_temp = OMAP5430_GPU_MIN_TEMP,
	.hyst_val = OMAP5430_GPU_HYST_VAL,
	.adc_start_val = OMAP5430_ADC_START_VALUE,
	.adc_end_val = OMAP5430_ADC_END_VALUE,
	.update_int1 = 1000,
	.update_int2 = 2000,
};

/* Thresholds and limits for OMAP5430 CORE temperature sensor */
static struct temp_sensor_data omap5430_core_temp_sensor_data = {
	.tshut_hot = OMAP5430_CORE_TSHUT_HOT,
	.tshut_cold = OMAP5430_CORE_TSHUT_COLD,
	.t_hot = OMAP5430_CORE_T_HOT,
	.t_cold = OMAP5430_CORE_T_COLD,
	.min_freq = OMAP5430_CORE_MIN_FREQ,
	.max_freq = OMAP5430_CORE_MAX_FREQ,
	.max_temp = OMAP5430_CORE_MAX_TEMP,
	.min_temp = OMAP5430_CORE_MIN_TEMP,
	.hyst_val = OMAP5430_CORE_HYST_VAL,
	.adc_start_val = OMAP5430_ADC_START_VALUE,
	.adc_end_val = OMAP5430_ADC_END_VALUE,
	.update_int1 = 1000,
	.update_int2 = 2000,
};

/*
 * Temperature values in milli degree celsius
 * ADC code values from 530 to 923
 */
static int
omap4460_adc_to_temp[OMAP4460_ADC_END_VALUE - OMAP4460_ADC_START_VALUE + 1] = {
	-40000, -40000, -40000, -40000, -39800, -39400, -39000, -38600, -38200,
	-37800, -37300, -36800, -36400, -36000, -35600, -35200, -34800,
	-34300, -33800, -33400, -33000, -32600, -32200, -31800, -31300,
	-30800, -30400, -30000, -29600, -29200, -28700, -28200, -27800,
	-27400, -27000, -26600, -26200, -25700, -25200, -24800, -24400,
	-24000, -23600, -23200, -22700, -22200, -21800, -21400, -21000,
	-20600, -20200, -19700, -19200, -18800, -18400, -18000, -17600,
	-17200, -16700, -16200, -15800, -15400, -15000, -14600, -14200,
	-13700, -13200, -12800, -12400, -12000, -11600, -11200, -10700,
	-10200, -9800, -9400, -9000, -8600, -8200, -7700, -7200, -6800,
	-6400, -6000, -5600, -5200, -4800, -4300, -3800, -3400, -3000,
	-2600, -2200, -1800, -1300, -800, -400, 0, 400, 800, 1200, 1600,
	2100, 2600, 3000, 3400, 3800, 4200, 4600, 5100, 5600, 6000, 6400,
	6800, 7200, 7600, 8000, 8500, 9000, 9400, 9800, 10200, 10600, 11000,
	11400, 11900, 12400, 12800, 13200, 13600, 14000, 14400, 14800,
	15300, 15800, 16200, 16600, 17000, 17400, 17800, 18200, 18700,
	19200, 19600, 20000, 20400, 20800, 21200, 21600, 22100, 22600,
	23000, 23400, 23800, 24200, 24600, 25000, 25400, 25900, 26400,
	26800, 27200, 27600, 28000, 28400, 28800, 29300, 29800, 30200,
	30600, 31000, 31400, 31800, 32200, 32600, 33100, 33600, 34000,
	34400, 34800, 35200, 35600, 36000, 36400, 36800, 37300, 37800,
	38200, 38600, 39000, 39400, 39800, 40200, 40600, 41100, 41600,
	42000, 42400, 42800, 43200, 43600, 44000, 44400, 44800, 45300,
	45800, 46200, 46600, 47000, 47400, 47800, 48200, 48600, 49000,
	49500, 50000, 50400, 50800, 51200, 51600, 52000, 52400, 52800,
	53200, 53700, 54200, 54600, 55000, 55400, 55800, 56200, 56600,
	57000, 57400, 57800, 58200, 58700, 59200, 59600, 60000, 60400,
	60800, 61200, 61600, 62000, 62400, 62800, 63300, 63800, 64200,
	64600, 65000, 65400, 65800, 66200, 66600, 67000, 67400, 67800,
	68200, 68700, 69200, 69600, 70000, 70400, 70800, 71200, 71600,
	72000, 72400, 72800, 73200, 73600, 74100, 74600, 75000, 75400,
	75800, 76200, 76600, 77000, 77400, 77800, 78200, 78600, 79000,
	79400, 79800, 80300, 80800, 81200, 81600, 82000, 82400, 82800,
	83200, 83600, 84000, 84400, 84800, 85200, 85600, 86000, 86400,
	86800, 87300, 87800, 88200, 88600, 89000, 89400, 89800, 90200,
	90600, 91000, 91400, 91800, 92200, 92600, 93000, 93400, 93800,
	94200, 94600, 95000, 95500, 96000, 96400, 96800, 97200, 97600,
	98000, 98400, 98800, 99200, 99600, 100000, 100400, 100800, 101200,
	101600, 102000, 102400, 102800, 103200, 103600, 104000, 104400,
	104800, 105200, 105600, 106100, 106600, 107000, 107400, 107800,
	108200, 108600, 109000, 109400, 109800, 110200, 110600, 111000,
	111400, 111800, 112200, 112600, 113000, 113400, 113800, 114200,
	114600, 115000, 115400, 115800, 116200, 116600, 117000, 117400,
	117800, 118200, 118600, 119000, 119400, 119800, 120200, 120600,
	121000, 121400, 121800, 122200, 122600, 123000, 123400, 123800, 124200,
	124600, 124900, 125000, 125000, 125000, 125000
};

static int
omap5430_adc_to_temp[OMAP5430_ADC_END_VALUE - OMAP5430_ADC_START_VALUE + 1] = {
	-40000, -40000, -40000, -40000, -39800, -39400, -39000, -38600,
	-38200, -37800, -37300, -36800,
	-36400, -36000, -35600, -35200, -34800, -34300, -33800, -33400, -33000,
	-32600,
	-32200, -31800, -31300, -30800, -30400, -30000, -29600, -29200, -28700,
	-28200, -27800, -27400, -27000, -26600, -26200, -25700, -25200, -24800,
	-24400, -24000, -23600, -23200, -22700, -22200, -21800, -21400, -21000,
	-20600, -20200, -19700, -19200, -9300, -18400, -18000, -17600, -17200,
	-16700, -16200, -15800, -15400, -15000, -14600, -14200, -13700, -13200,
	-12800, -12400, -12000, -11600, -11200, -10700, -10200, -9800, -9400,
	-9000,
	-8600, -8200, -7700, -7200, -6800, -6400, -6000, -5600, -5200, -4800,
	-4300,
	-3800, -3400, -3000, -2600, -2200, -1800, -1300, -800, -400, 0, 400,
	800,
	1200, 1600, 2100, 2600, 3000, 3400, 3800, 4200, 4600, 5100, 5600, 6000,
	6400, 6800, 7200, 7600, 8000, 8500, 9000, 9400, 9800, 10200, 10800,
	11100,
	11400, 11900, 12400, 12800, 13200, 13600, 14000, 14400, 14800, 15300,
	15800,
	16200, 16600, 17000, 17400, 17800, 18200, 18700, 19200, 19600, 20000,
	20400,
	20800, 21200, 21600, 22100, 22600, 23000, 23400, 23800, 24200, 24600,
	25000,
	25400, 25900, 26400, 26800, 27200, 27600, 28000, 28400, 28800, 29300,
	29800,
	30200, 30600, 31000, 31400, 31800, 32200, 32600, 33100, 33600, 34000,
	34400,
	34800, 35200, 35600, 36000, 36400, 36800, 37300, 37800, 38200, 38600,
	39000,
	39400, 39800, 40200, 40600, 41100, 41600, 42000, 42400, 42800, 43200,
	43600,
	44000, 44400, 44800, 45300, 45800, 46200, 46600, 47000, 47400, 47800,
	48200,
	48600, 49000, 49500, 50000, 50400, 50800, 51200, 51600, 52000, 52400,
	52800,
	53200, 53700, 54200, 54600, 55000, 55400, 55800, 56200, 56600, 57000,
	57400,
	57800, 58200, 58700, 59200, 59600, 60000, 60400, 60800, 61200, 61600,
	62000,
	62400, 62800, 63300, 63800, 64200, 64600, 65000, 65400, 65800, 66200,
	66600,
	67000, 67400, 67800, 68200, 68700, 69200, 69600, 70000, 70400, 70800,
	71200,
	71600, 72000, 72400, 72800, 73200, 73600, 74100, 74600, 75000, 75400,
	75800,
	76200, 76600, 77000, 77400, 77800, 78200, 78600, 79000, 79400, 79800,
	80300,
	80800, 81200, 81600, 82000, 82400, 82800, 83200, 83600, 84000, 84400,
	84800,
	85200, 85600, 86000, 86400, 86800, 87300, 87800, 88200, 88600, 89000,
	89400,
	89800, 90200, 90600, 91000, 91400, 91800, 92200, 92600, 93000, 93400,
	93800,
	94200, 94600, 95000, 95500, 96000, 96400, 96800, 97200, 97600, 98000,
	98400,
	98800, 99200, 99600, 100000, 100400, 100800, 101200, 101600, 102000,
	102400,
	102800, 103200, 103600, 104000, 104400, 104800, 105200, 105600, 106100,
	106600, 107000, 107400, 107800, 108200, 108600, 109000, 109400, 109800,
	110200, 110600, 111000, 111400, 111800, 112200, 112600, 113000, 113400,
	113800, 114200, 114600, 115000, 115400, 115800, 116200, 116600, 117000,
	117400, 117800, 118200, 118600, 119000, 119400, 119800, 120200, 120600,
	121000, 121400, 121800, 122200, 122600, 123000, 123400, 123800, 124200,
	124600, 124900, 125000, 125000, 125000, 125000,
};

static irqreturn_t talert_irq_handler(int irq, void *data)
{
	struct omap_bandgap *bg_ptr = data;
	struct device *cdev = bg_ptr->dev->parent;
	struct temp_sensor_registers *tsr;
	u32 t_hot = 0, t_cold = 0, temp, ctrl;
	int i, r;

	bg_ptr = data;
	/* Read the status of t_hot */
	for (i = 0; i < bg_ptr->pdata->sensor_count; i++) {
		tsr = bg_ptr->pdata->sensors[i].registers;
		r = omap_control_readl(cdev, tsr->bgap_status, &t_hot);
		t_hot &= tsr->status_hot_mask;

		/* Read the status of t_cold */
		r |= omap_control_readl(cdev, tsr->bgap_status, &t_cold);
		t_cold &= tsr->status_cold_mask;

		if (!t_cold && !t_hot)
			continue;

		r |= omap_control_readl(cdev, tsr->bgap_mask_ctrl, &ctrl);
		/*
		 * One TALERT interrupt: Two sources
		 * If the interrupt is due to t_hot then mask t_hot and
		 * and unmask t_cold else mask t_cold and unmask t_hot
		 */
		if (t_hot) {
			ctrl &= ~tsr->mask_hot_mask;
			ctrl |= tsr->mask_cold_mask;
		} else if (t_cold) {
			ctrl &= ~tsr->mask_cold_mask;
			ctrl |= tsr->mask_hot_mask;
		}

		r |= omap_control_writel(cdev, ctrl, tsr->bgap_mask_ctrl);

		if (r) {
			dev_err(bg_ptr->dev, "failed to ack talert interrupt\n");
			return IRQ_NONE;
		}

		/* read temperature */
		r = omap_control_readl(cdev, tsr->temp_sensor_ctrl, &temp);
		temp &= tsr->bgap_dtemp_mask;

		/* report temperature to whom may concern */
		if (bg_ptr->pdata->report_temperature)
			bg_ptr->pdata->report_temperature(bg_ptr, i);
	}

	return IRQ_HANDLED;
}

static irqreturn_t omap_bandgap_tshut_irq_handler(int irq, void *data)
{
	orderly_poweroff(true);

	return IRQ_HANDLED;
}

static
int adc_to_temp_conversion(struct omap_bandgap *bg_ptr, int id, int adc_val,
			   u32 *t)
{
	struct temp_sensor_data *ts_data = bg_ptr->pdata->sensors[id].ts_data;

	/* look up for temperature in the table and return the temperature */
	if (adc_val < ts_data->adc_start_val || adc_val > ts_data->adc_end_val)
		return -ERANGE;

	*t = bg_ptr->conv_table[adc_val - ts_data->adc_start_val];

	return 0;
}

static int temp_to_adc_conversion(long temp, struct omap_bandgap *bg_ptr, int i,
				  u32 *adc)
{
	struct temp_sensor_data *ts_data = bg_ptr->pdata->sensors[i].ts_data;
	int high, low, mid;

	low = 0;
	high = ts_data->adc_end_val - ts_data->adc_start_val;
	mid = (high + low) / 2;

	if (temp < bg_ptr->conv_table[high] || temp > bg_ptr->conv_table[high])
		return -EINVAL;

	while (low < high) {
		if (temp < bg_ptr->conv_table[mid])
			high = mid - 1;
		else
			low = mid + 1;
		mid = (low + high) / 2;
	}

	*adc = ts_data->adc_start_val + low;

	return 0;
}

static int temp_sensor_unmask_interrupts(struct omap_bandgap *bg_ptr, int id,
					 u32 t_hot, u32 t_cold)
{
	struct device *cdev = bg_ptr->dev->parent;
	struct temp_sensor_registers *tsr;
	u32 temp, reg_val;
	int err;

	/* Read the current on die temperature */
	tsr = bg_ptr->pdata->sensors[id].registers;
	err = omap_control_readl(cdev, tsr->temp_sensor_ctrl, &temp);
	temp &= tsr->bgap_dtemp_mask;

	err |= omap_control_readl(cdev, tsr->bgap_mask_ctrl, &reg_val);
	if (temp < t_hot)
		reg_val |= tsr->mask_hot_mask;
	else
		reg_val &= ~tsr->mask_hot_mask;

	if (t_cold < temp)
		reg_val |= tsr->mask_cold_mask;
	else
		reg_val &= ~tsr->mask_cold_mask;
	err |= omap_control_writel(cdev, reg_val, tsr->bgap_mask_ctrl);

	if (err)
		dev_err(bg_ptr->dev, "failed to unmask interrupts\n");

	return -EIO;
}

static
int add_hyst(int adc_val, int hyst_val, struct omap_bandgap *bg_ptr, int i,
	     u32 *sum)
{
	int ret;
	u32 temp;

	ret = adc_to_temp_conversion(bg_ptr, i, adc_val, &temp);
	if (ret < 0)
		return ret;

	temp += hyst_val;

	return temp_to_adc_conversion(temp, bg_ptr, i, sum);
}

static
int temp_sensor_configure_thot(struct omap_bandgap *bg_ptr, int id, int t_hot)
{
	struct temp_sensor_data *ts_data = bg_ptr->pdata->sensors[id].ts_data;
	struct device *cdev = bg_ptr->dev->parent;
	struct temp_sensor_registers *tsr;
	u32 thresh_val, reg_val, cold;
	int err;

	tsr = bg_ptr->pdata->sensors[id].registers;
	/* obtain the T cold value */
	err = omap_control_readl(cdev, tsr->bgap_threshold, &thresh_val);
	cold = (thresh_val & tsr->threshold_tcold_mask) >>
	    __ffs(tsr->threshold_tcold_mask);
	if (t_hot <= cold) {
		/* change the t_cold to t_hot - 5000 millidegrees */
		err |= add_hyst(t_hot, -ts_data->hyst_val, bg_ptr, id, &cold);
		/* write the new t_cold value */
		reg_val = thresh_val & (~tsr->threshold_tcold_mask);
		reg_val |= cold << __ffs(tsr->threshold_tcold_mask);
		err |= omap_control_writel(cdev, reg_val, tsr->bgap_threshold);
		thresh_val = reg_val;
	}

	/* write the new t_hot value */
	reg_val = thresh_val & ~tsr->threshold_thot_mask;
	reg_val |= (t_hot << __ffs(tsr->threshold_thot_mask));
	err |= omap_control_writel(cdev, reg_val, tsr->bgap_threshold);
	if (err) {
		dev_err(bg_ptr->dev, "failed to reprogram thot threshold\n");
		return -EIO;
	}

	return temp_sensor_unmask_interrupts(bg_ptr, id, t_hot, cold);
}

static
int temp_sensor_init_talert_thresholds(struct omap_bandgap *bg_ptr, int id,
				       int t_hot, int t_cold)
{
	struct device *cdev = bg_ptr->dev->parent;
	struct temp_sensor_registers *tsr;
	u32 reg_val, thresh_val;
	int err;

	tsr = bg_ptr->pdata->sensors[id].registers;
	err = omap_control_readl(cdev, tsr->bgap_threshold, &thresh_val);

	/* write the new t_cold value */
	reg_val = thresh_val & ~tsr->threshold_tcold_mask;
	reg_val |= (t_cold << __ffs(tsr->threshold_tcold_mask));
	err |= omap_control_writel(cdev, reg_val, tsr->bgap_threshold);
	if (err) {
		dev_err(bg_ptr->dev, "failed to reprogram tcold threshold\n");
		return -EIO;
	}

	err = omap_control_readl(cdev, tsr->bgap_threshold, &thresh_val);

	/* write the new t_hot value */
	reg_val = thresh_val & ~tsr->threshold_thot_mask;
	reg_val |= (t_hot << __ffs(tsr->threshold_thot_mask));
	err |= omap_control_writel(cdev, reg_val, tsr->bgap_threshold);
	if (err) {
		dev_err(bg_ptr->dev, "failed to reprogram thot threshold\n");
		return -EIO;
	}

	err = omap_control_readl(cdev, tsr->bgap_mask_ctrl, &reg_val);
	reg_val |= tsr->mask_hot_mask;
	reg_val |= tsr->mask_cold_mask;
	err |= omap_control_writel(cdev, reg_val, tsr->bgap_mask_ctrl);
	if (err) {
		dev_err(bg_ptr->dev, "failed to reprogram thot threshold\n");
		return -EIO;
	}

	return err;
}

static
int temp_sensor_configure_tcold(struct omap_bandgap *bg_ptr, int id,
				int t_cold)
{
	struct temp_sensor_data *ts_data = bg_ptr->pdata->sensors[id].ts_data;
	struct device *cdev = bg_ptr->dev->parent;
	struct temp_sensor_registers *tsr;
	u32 thresh_val, reg_val, hot;
	int err;

	tsr = bg_ptr->pdata->sensors[id].registers;
	/* obtain the T cold value */
	err = omap_control_readl(cdev, tsr->bgap_threshold, &thresh_val);
	hot = (thresh_val & tsr->threshold_thot_mask) >>
	    __ffs(tsr->threshold_thot_mask);

	if (t_cold >= hot) {
		/* change the t_hot to t_cold + 5000 millidegrees */
		err |= add_hyst(t_cold, ts_data->hyst_val, bg_ptr, id, &hot);
		/* write the new t_hot value */
		reg_val = thresh_val & (~tsr->threshold_thot_mask);
		reg_val |= hot << __ffs(tsr->threshold_thot_mask);
		err |= omap_control_writel(cdev, reg_val, tsr->bgap_threshold);
		thresh_val = reg_val;
	}

	/* write the new t_cold value */
	reg_val = thresh_val & ~tsr->threshold_tcold_mask;
	reg_val |= (t_cold << __ffs(tsr->threshold_tcold_mask));
	err |= omap_control_writel(cdev, reg_val, tsr->bgap_threshold);
	if (err) {
		dev_err(bg_ptr->dev, "failed to reprogram tcold threshold\n");
		return -EIO;
	}

	return temp_sensor_unmask_interrupts(bg_ptr, id, hot, t_cold);
}

static int temp_sensor_configure_tshut_hot(struct omap_bandgap *bg_ptr,
					   int id, int tshut_hot)
{
	struct device *cdev = bg_ptr->dev->parent;
	struct temp_sensor_registers *tsr;
	u32 reg_val;
	int err;

	tsr = bg_ptr->pdata->sensors[id].registers;
	err = omap_control_readl(cdev, tsr->tshut_threshold, &reg_val);
	reg_val &= ~tsr->tshut_hot_mask;
	reg_val |= tshut_hot << __ffs(tsr->tshut_hot_mask);
	err |= omap_control_writel(cdev, reg_val, tsr->tshut_threshold);
	if (err) {
		dev_err(bg_ptr->dev, "failed to reprogram tshut thot\n");
		return -EIO;
	}

	return 0;
}

static int temp_sensor_configure_tshut_cold(struct omap_bandgap *bg_ptr,
					    int id, int tshut_cold)
{
	struct device *cdev = bg_ptr->dev->parent;
	struct temp_sensor_registers *tsr;
	u32 reg_val;
	int err;

	tsr = bg_ptr->pdata->sensors[id].registers;
	err = omap_control_readl(cdev, tsr->tshut_threshold, &reg_val);
	reg_val &= ~tsr->tshut_cold_mask;
	reg_val |= tshut_cold << __ffs(tsr->tshut_cold_mask);
	err |= omap_control_writel(cdev, reg_val, tsr->tshut_threshold);
	if (err) {
		dev_err(bg_ptr->dev, "failed to reprogram tshut tcold\n");
		return -EIO;
	}

	return 0;
}

static int configure_temp_sensor_counter(struct omap_bandgap *bg_ptr, int id,
					 u32 counter)
{
	struct device *cdev = bg_ptr->dev->parent;
	struct temp_sensor_registers *tsr;
	u32 val;
	int err;

	tsr = bg_ptr->pdata->sensors[id].registers;
	err = omap_control_readl(cdev, tsr->bgap_counter, &val);
	val &= ~tsr->counter_mask;
	val |= counter << __ffs(tsr->counter_mask);
	err |= omap_control_writel(cdev, val, tsr->bgap_counter);
	if (err) {
		dev_err(bg_ptr->dev, "failed to reprogram tshut tcold\n");
		return -EIO;
	}

	return 0;
}

/* Exposed APIs */
/**
 * omap_bandgap_read_thot() - reads sensor current thot
 * @bg_ptr - pointer to bandgap instance
 * @id - sensor id
 * @thot - resulting current thot value
 *
 * returns 0 on success or the proper error code
 */
int omap_bandgap_read_thot(struct omap_bandgap *bg_ptr, int id,
			      int *thot)
{
	struct device *cdev = bg_ptr->dev->parent;
	struct temp_sensor_registers *tsr;
	u32 temp;
	int ret;

	tsr = bg_ptr->pdata->sensors[id].registers;
	ret = omap_control_readl(cdev, tsr->bgap_threshold, &temp);
	temp = (temp & tsr->threshold_thot_mask) >>
		__ffs(tsr->threshold_thot_mask);
	ret |= adc_to_temp_conversion(bg_ptr, id, temp, &temp);
	if (ret) {
		dev_err(bg_ptr->dev, "failed to read thot\n");
		return -EIO;
	}

	*thot = temp;

	return 0;
}

/**
 * omap_bandgap_write_thot() - sets sensor current thot
 * @bg_ptr - pointer to bandgap instance
 * @id - sensor id
 * @val - desired thot value
 *
 * returns 0 on success or the proper error code
 */
int omap_bandgap_write_thot(struct omap_bandgap *bg_ptr, int id, int val)
{
	struct temp_sensor_data *ts_data = bg_ptr->pdata->sensors[id].ts_data;
	struct temp_sensor_registers *tsr;
	u32 t_hot;
	int ret;

	tsr = bg_ptr->pdata->sensors[id].registers;

	if (val < ts_data->min_temp + ts_data->hyst_val)
		return -EINVAL;
	ret = temp_to_adc_conversion(val, bg_ptr, id, &t_hot);
	if (ret < 0)
		return ret;

	mutex_lock(&bg_ptr->bg_mutex);
	temp_sensor_configure_thot(bg_ptr, id, t_hot);
	mutex_unlock(&bg_ptr->bg_mutex);

	return 0;
}

/**
 * omap_bandgap_read_tcold() - reads sensor current tcold
 * @bg_ptr - pointer to bandgap instance
 * @id - sensor id
 * @tcold - resulting current tcold value
 *
 * returns 0 on success or the proper error code
 */
int omap_bandgap_read_tcold(struct omap_bandgap *bg_ptr, int id,
			       int *tcold)
{
	struct device *cdev = bg_ptr->dev->parent;
	struct temp_sensor_registers *tsr;
	u32 temp;
	int ret;

	tsr = bg_ptr->pdata->sensors[id].registers;
	ret = omap_control_readl(cdev, tsr->bgap_threshold, &temp);
	temp = (temp & tsr->threshold_tcold_mask)
	    >> __ffs(tsr->threshold_tcold_mask);
	ret |= adc_to_temp_conversion(bg_ptr, id, temp, &temp);
	if (ret)
		return -EIO;

	*tcold = temp;

	return 0;
}

/**
 * omap_bandgap_write_tcold() - sets the sensor tcold
 * @bg_ptr - pointer to bandgap instance
 * @id - sensor id
 * @val - desired tcold value
 *
 * returns 0 on success or the proper error code
 */
int omap_bandgap_write_tcold(struct omap_bandgap *bg_ptr, int id, int val)
{
	struct temp_sensor_data *ts_data = bg_ptr->pdata->sensors[id].ts_data;
	struct temp_sensor_registers *tsr;
	u32 t_cold;
	int ret;

	tsr = bg_ptr->pdata->sensors[id].registers;
	if (val > ts_data->max_temp + ts_data->hyst_val)
		return -EINVAL;

	ret = temp_to_adc_conversion(val, bg_ptr, id, &t_cold);
	if (ret < 0)
		return ret;

	mutex_lock(&bg_ptr->bg_mutex);
	temp_sensor_configure_tcold(bg_ptr, id, t_cold);
	mutex_unlock(&bg_ptr->bg_mutex);

	return 0;
}

/**
 * omap_bandgap_read_update_interval() - read the sensor update interval
 * @bg_ptr - pointer to bandgap instance
 * @id - sensor id
 * @interval - resulting update interval in miliseconds
 *
 * returns 0 on success or the proper error code
 */
int omap_bandgap_read_update_interval(struct omap_bandgap *bg_ptr, int id,
					 int *interval)
{
	struct device *cdev = bg_ptr->dev->parent;
	struct temp_sensor_registers *tsr;
	u32 time;
	int ret;

	tsr = bg_ptr->pdata->sensors[id].registers;
	ret = omap_control_readl(cdev, tsr->bgap_counter, &time);
	if (ret)
		return ret;
	time = (time & tsr->counter_mask) >> __ffs(tsr->counter_mask);
	time = time * 1000 / bg_ptr->clk_rate;

	*interval = time;

	return 0;
}

/**
 * omap_bandgap_write_update_interval() - set the update interval
 * @bg_ptr - pointer to bandgap instance
 * @id - sensor id
 * @interval - desired update interval in miliseconds
 *
 * returns 0 on success or the proper error code
 */
int omap_bandgap_write_update_interval(struct omap_bandgap *bg_ptr,
					  int id, u32 interval)
{
	interval = interval * bg_ptr->clk_rate / 1000;
	mutex_lock(&bg_ptr->bg_mutex);
	configure_temp_sensor_counter(bg_ptr, id, interval);
	mutex_unlock(&bg_ptr->bg_mutex);

	return 0;
}

/**
 * omap_bandgap_read_temperature() - report current temperature
 * @bg_ptr - pointer to bandgap instance
 * @id - sensor id
 * @temperature - resulting temperature
 *
 * returns 0 on success or the proper error code
 */
int omap_bandgap_read_temperature(struct omap_bandgap *bg_ptr, int id,
				     int *temperature)
{
	struct device *cdev = bg_ptr->dev->parent;
	struct temp_sensor_registers *tsr;
	u32 temp;
	int ret;

	tsr = bg_ptr->pdata->sensors[id].registers;
	ret = omap_control_readl(cdev, tsr->temp_sensor_ctrl, &temp);
	temp &= tsr->bgap_dtemp_mask;

	ret |= adc_to_temp_conversion(bg_ptr, id, temp, &temp);
	if (ret)
		return -EIO;

	*temperature = temp;

	return 0;
}

/**
 * omap_bandgap_set_sensor_data() - helper function to store thermal
 * framework related data.
 * @bg_ptr - pointer to bandgap instance
 * @id - sensor id
 * @data - thermal framework related data to be stored
 *
 * returns 0 on success or the proper error code
 */
int omap_bandgap_set_sensor_data(struct omap_bandgap *bg_ptr, int id,
				void *data)
{
	if (!bg_ptr) {
		pr_err("%s:Invalid pointer\n", __func__);
		return -EINVAL;
	}

	if ((id < 0) || (id >= bg_ptr->pdata->sensor_count)) {
		dev_err(bg_ptr->dev, "Invalid sensor device\n");
		return -ENODEV;
	}

	bg_ptr->pdata->sensors[id].data = data;

	return 0;
}

/**
 * omap_bandgap_get_sensor_data() - helper function to get thermal
 * framework related data.
 * @bg_ptr - pointer to bandgap instance
 * @id - sensor id
 *
 * returns data stored by set function with sensor id on success or NULL
 */
void *omap_bandgap_get_sensor_data(struct omap_bandgap *bg_ptr, int id)
{
	if (!bg_ptr) {
		pr_err("%s:Invalid pointer\n", __func__);
		return ERR_PTR(-EINVAL);
	}

	if ((id < 0) || (id >= bg_ptr->pdata->sensor_count)) {
		dev_err(bg_ptr->dev, "Invalid sensor device\n");
		return ERR_PTR(-ENODEV);
	}

	return bg_ptr->pdata->sensors[id].data;
}

/**
 * enable_continuous_mode() - One time enabling of continuous conversion mode
 * @bg_ptr - pointer to scm instance
 */
static int enable_continuous_mode(struct omap_bandgap *bg_ptr)
{
	struct device *cdev = bg_ptr->dev->parent;
	struct temp_sensor_registers *tsr;
	int i, r = 0;
	u32 val;

	for (i = 0; i < bg_ptr->pdata->sensor_count; i++) {
		tsr = bg_ptr->pdata->sensors[i].registers;
		r = omap_control_readl(cdev, tsr->bgap_mode_ctrl, &val);

		val |= 1 << __ffs(tsr->mode_ctrl_mask);

		r |= omap_control_writel(cdev, val, tsr->bgap_mode_ctrl);
		if (r)
			dev_err(bg_ptr->dev, "could not save sensor %d\n", i);
	}

	return r ? -EIO : 0;
}

static int omap_bandgap_tshut_init(struct omap_bandgap *bg_ptr,
				      struct platform_device *pdev)
{
	int gpio_nr = bg_ptr->tshut_gpio;
	int status;

	/* Request for gpio_86 line */
	status = gpio_request(gpio_nr, "tshut");
	if (status < 0) {
		dev_err(&pdev->dev,
			"Could not request for TSHUT GPIO:%i\n", gpio_nr);
		return status;
	}
	status = gpio_direction_input(gpio_nr);
	if (status) {
		dev_err(&pdev->dev,
			"Cannot set input TSHUT GPIO %d\n", gpio_nr);
		return status;
	}

	status = request_irq(gpio_to_irq(gpio_nr),
			     omap_bandgap_tshut_irq_handler,
			     IRQF_TRIGGER_RISING, "tshut",
			     NULL);
	if (status) {
		gpio_free(gpio_nr);
		dev_err(&pdev->dev, "request irq failed for TSHUT");
	}


	return 0;
}

static int omap_bandgap_talert_init(struct omap_bandgap *bg_ptr,
				       struct platform_device *pdev)
{
	int ret;

	bg_ptr->irq = platform_get_irq(pdev, 0);
	if (bg_ptr->irq < 0) {
		dev_err(&pdev->dev, "get_irq failed\n");
		return bg_ptr->irq;
	}
	ret = request_threaded_irq(bg_ptr->irq, NULL,
				   talert_irq_handler,
				   IRQF_TRIGGER_HIGH | IRQF_ONESHOT,
				   "talert", bg_ptr);
	if (ret) {
		dev_err(&pdev->dev, "Request threaded irq failed.\n");
		return ret;
	}

	return 0;
}

static struct omap_bandgap_data omap4460_data = {
	.has_talert = true,
	.has_tshut = true,
	.fclock_name = "bandgap_ts_fclk",
	.div_ck_name = "div_ts_ck",
	.conv_table = omap4460_adc_to_temp,
	.sensors = {
		{
			.registers = &omap4460_mpu_temp_sensor_registers,
			.ts_data = &omap4460_mpu_temp_sensor_data,
			.domain = "cpu",
			.slope = 376,
			.constant_offset = -16000,
		},
	},
	.sensor_count = 1,
};

static struct omap_bandgap_data omap5430_data = {
	.has_talert = true,
	.has_tshut = false,
	.fclock_name = "ts_clk_div_ck",
	.div_ck_name = "ts_clk_div_ck",
	.conv_table = omap5430_adc_to_temp,
	.report_temperature = omap5_thermal_report_temperature,
	.expose_sensor = omap5_thermal_expose_sensor,
	.remove_sensor = omap5_thermal_remove_sensor,
	.sensors = {
		{
			.registers = &omap5430_mpu_temp_sensor_registers,
			.ts_data = &omap5430_mpu_temp_sensor_data,
			.domain = "cpu",
			.slope = 196,
			.constant_offset = -6822,
		},
		{
			.registers = &omap5430_gpu_temp_sensor_registers,
			.ts_data = &omap5430_gpu_temp_sensor_data,
			.domain = "gpu",
			.slope = 64,
			.constant_offset = -980,
		},
		{
			.registers = &omap5430_core_temp_sensor_registers,
			.ts_data = &omap5430_core_temp_sensor_data,
			.domain = "core",
			.slope = 0,
			.constant_offset = 0,
		},
	},
	.sensor_count = 3,
};

static const struct of_device_id of_omap_bandgap_match[] = {
	/*
	 * TODO: Add support to 4430
	 * { .compatible = "ti,omap4430-bandgap", .data = , },
	 */
	{ .compatible = "ti,omap4460-bandgap", .data = &omap4460_data, },
	{ .compatible = "ti,omap5430-bandgap", .data = &omap5430_data, },
	{ },
};

static
struct omap_bandgap *omap_bandgap_platform_build(struct platform_device *pdev)
{
	struct omap_bdg_data *pdata = dev_get_platdata(&pdev->dev);
	struct omap_bandgap *bg_ptr;

	bg_ptr = devm_kzalloc(&pdev->dev, sizeof(struct omap_bandgap),
				    GFP_KERNEL);
	if (!bg_ptr) {
		dev_err(&pdev->dev, "Unable to allocate mem for driver ref\n");
		return ERR_PTR(-ENOMEM);
	}

	if (pdata->rev == 1)
		bg_ptr->pdata = &omap4460_data;
	else if (pdata->rev == 2)
		bg_ptr->pdata = &omap5430_data;

	bg_ptr->irq = platform_get_irq(pdev, 0);
	if (bg_ptr->irq < 0) {
		dev_err(&pdev->dev, "get_irq failed\n");
		return ERR_PTR(-EINVAL);
	}

	if (bg_ptr->pdata->has_tshut) {
		bg_ptr->tshut_gpio = pdata->tshut_gpio;
		if (!gpio_is_valid(bg_ptr->tshut_gpio)) {
			dev_err(&pdev->dev, "invalid gpio for tshut (%d)\n",
				bg_ptr->tshut_gpio);
			return ERR_PTR(-EINVAL);
		}
	}

	return bg_ptr;
}


static struct omap_bandgap *omap_bandgap_build(struct platform_device *pdev)
{
	struct device_node *node = pdev->dev.of_node;
	const struct of_device_id *of_id;
	struct omap_bandgap *bg_ptr;
	u32 prop;

	/* just for the sake */
	if (!node) {
		dev_err(&pdev->dev, "no platform information available\n");
		return ERR_PTR(-EINVAL);
	}

	bg_ptr = devm_kzalloc(&pdev->dev, sizeof(struct omap_bandgap),
				    GFP_KERNEL);
	if (!bg_ptr) {
		dev_err(&pdev->dev, "Unable to allocate mem for driver ref\n");
		return ERR_PTR(-ENOMEM);
	}

	of_id = of_match_device(of_omap_bandgap_match, &pdev->dev);
	if (of_id)
		bg_ptr->pdata = of_id->data;

	if (bg_ptr->pdata->has_tshut) {
		if (of_property_read_u32(node, "ti,tshut-gpio", &prop) < 0) {
			dev_err(&pdev->dev, "missing tshut gpio in device tree\n");
			return ERR_PTR(-EINVAL);
		}
		bg_ptr->tshut_gpio = prop;
		if (!gpio_is_valid(bg_ptr->tshut_gpio)) {
			dev_err(&pdev->dev, "invalid gpio for tshut (%d)\n",
				bg_ptr->tshut_gpio);
			return ERR_PTR(-EINVAL);
		}
	}

	return bg_ptr;
}

static
int __devinit omap_bandgap_probe(struct platform_device *pdev)
{
	struct device *cdev = pdev->dev.parent;
	struct omap_bandgap *bg_ptr;
	int clk_rate, ret = 0, i;

	if (!cdev) {
		dev_err(&pdev->dev, "no omap control ref in our parent\n");
		return -EINVAL;
	}

	if (pdev->dev.platform_data)
		bg_ptr = omap_bandgap_platform_build(pdev);
	else
		bg_ptr = omap_bandgap_build(pdev);

	if (IS_ERR_OR_NULL(bg_ptr)) {
		dev_err(&pdev->dev, "failed to fetch platform data\n");
		return PTR_ERR(bg_ptr);
	}

	if (bg_ptr->pdata->has_tshut) {
		ret = omap_bandgap_tshut_init(bg_ptr, pdev);
		if (ret) {
			dev_err(&pdev->dev,
				"failed to initialize system tshut IRQ\n");
			return ret;
		}
	}

	bg_ptr->fclock = clk_get(NULL, bg_ptr->pdata->fclock_name);
	ret = IS_ERR_OR_NULL(bg_ptr->fclock);
	if (ret) {
		dev_err(&pdev->dev, "failed to request fclock reference\n");
		goto free_irqs;
	}

	bg_ptr->div_clk = clk_get(NULL,  bg_ptr->pdata->div_ck_name);
	ret = IS_ERR_OR_NULL(bg_ptr->div_clk);
	if (ret) {
		dev_err(&pdev->dev,
			"failed to request div_ts_ck clock ref\n");
		goto free_irqs;
	}

	bg_ptr->conv_table = bg_ptr->pdata->conv_table;
	for (i = 0; i < bg_ptr->pdata->sensor_count; i++) {
		struct temp_sensor_registers *tsr;
		u32 val;

		tsr = bg_ptr->pdata->sensors[i].registers;
		/*
		 * check if the efuse has a non-zero value if not
		 * it is an untrimmed sample and the temperatures
		 * may not be accurate
		 */
		ret = omap_control_readl(cdev, tsr->bgap_efuse, &val);
		if (ret || !val)
			dev_info(&pdev->dev,
				 "Non-trimmed BGAP, Temp not accurate\n");
	}

	clk_rate = clk_round_rate(bg_ptr->div_clk,
				  bg_ptr->pdata->sensors[0].ts_data->max_freq);
	if (clk_rate < bg_ptr->pdata->sensors[0].ts_data->min_freq ||
	    clk_rate == 0xffffffff) {
		ret = -ENODEV;
		goto put_clks;
	}

	ret = clk_set_rate(bg_ptr->div_clk, clk_rate);
	if (ret) {
		dev_err(&pdev->dev, "Cannot set clock rate\n");
		goto put_clks;
	}

	bg_ptr->clk_rate = clk_rate;
	clk_enable(bg_ptr->fclock);

	mutex_init(&bg_ptr->bg_mutex);
	bg_ptr->dev = &pdev->dev;
	platform_set_drvdata(pdev, bg_ptr);

	/* 1 clk cycle */
	for (i = 0; i < bg_ptr->pdata->sensor_count; i++)
		configure_temp_sensor_counter(bg_ptr, i, 1);

	for (i = 0; i < bg_ptr->pdata->sensor_count; i++) {
		struct temp_sensor_data *ts_data;

		ts_data = bg_ptr->pdata->sensors[i].ts_data;

		temp_sensor_init_talert_thresholds(bg_ptr, i,
						   ts_data->t_hot,
						   ts_data->t_cold);
		temp_sensor_configure_tshut_hot(bg_ptr, i,
						ts_data->tshut_hot);
		temp_sensor_configure_tshut_cold(bg_ptr, i,
						 ts_data->tshut_cold);
	}

	enable_continuous_mode(bg_ptr);

	/* Set .250 seconds time as default counter */
	for (i = 0; i < bg_ptr->pdata->sensor_count; i++)
		configure_temp_sensor_counter(bg_ptr, i,
					      bg_ptr->clk_rate / 4);

	/* Every thing is good? Then expose the sensors */
	for (i = 0; i < bg_ptr->pdata->sensor_count; i++) {
		char *domain;

		domain = bg_ptr->pdata->sensors[i].domain;
		if (bg_ptr->pdata->expose_sensor)
			bg_ptr->pdata->expose_sensor(bg_ptr, i, domain);
	}

	/*
	 * Enable the Interrupts once everything is set. Otherwise irq handler
	 * might be called as soon as it is enabled where as rest of framework
	 * is still getting initialised.
	 */
	if (bg_ptr->pdata->has_talert) {
		ret = omap_bandgap_talert_init(bg_ptr, pdev);
		if (ret) {
			dev_err(&pdev->dev, "failed to initialize Talert IRQ\n");
			i = bg_ptr->pdata->sensor_count;
			goto put_clks;
		}
	}

	g_bg_ptr = bg_ptr;
	g_bg_ptr->bg_in_suspend = false;
	g_bg_ptr->bg_clk_idle = false;
	return 0;

put_clks:
	clk_disable(bg_ptr->fclock);
	clk_put(bg_ptr->fclock);
	clk_put(bg_ptr->div_clk);
free_irqs:
	if (bg_ptr->pdata->has_tshut) {
		free_irq(gpio_to_irq(bg_ptr->tshut_gpio), NULL);
		gpio_free(bg_ptr->tshut_gpio);
	}

	return ret;
}

static
int __devexit omap_bandgap_remove(struct platform_device *pdev)
{
	struct omap_bandgap *bg_ptr = platform_get_drvdata(pdev);
	int i;

	/* First thing is to remove sensor interfaces */
	for (i = 0; i < bg_ptr->pdata->sensor_count; i++)
		if (bg_ptr->pdata->remove_sensor)
			bg_ptr->pdata->remove_sensor(bg_ptr, i);

	clk_disable(bg_ptr->fclock);
	clk_put(bg_ptr->fclock);
	clk_put(bg_ptr->div_clk);
	if (bg_ptr->pdata->has_talert)
		free_irq(bg_ptr->irq, bg_ptr);
	if (bg_ptr->pdata->has_tshut) {
		free_irq(gpio_to_irq(bg_ptr->tshut_gpio), NULL);
		gpio_free(bg_ptr->tshut_gpio);
	}

	return 0;
}

#ifdef CONFIG_PM
static int omap_bandgap_save_ctxt(struct omap_bandgap *bg_ptr)
{
	struct device *cdev = bg_ptr->dev->parent;
	int err = 0;
	int i;

	for (i = 0; i < bg_ptr->pdata->sensor_count; i++) {
		struct temp_sensor_registers *tsr;
		struct temp_sensor_regval *rval;

		rval = &bg_ptr->pdata->sensors[i].regval;
		tsr = bg_ptr->pdata->sensors[i].registers;

		err = omap_control_readl(cdev, tsr->bgap_mode_ctrl,
					 &rval->bg_mode_ctrl);
		err |= omap_control_readl(cdev,	tsr->bgap_mask_ctrl,
					  &rval->bg_ctrl);
		err |= omap_control_readl(cdev,	tsr->bgap_counter,
					  &rval->bg_counter);
		err |= omap_control_readl(cdev, tsr->bgap_threshold,
					  &rval->bg_threshold);
		err |= omap_control_readl(cdev, tsr->tshut_threshold,
					  &rval->tshut_threshold);

		if (err)
			dev_err(bg_ptr->dev, "could not save sensor %d\n", i);
	}

	return err ? -EIO : 0;
}

static int
omap_bandgap_force_single_read(struct omap_bandgap *bg_ptr, int id)
{
	struct device *cdev = bg_ptr->dev->parent;
	struct temp_sensor_registers *tsr;
	u32 temp = 0, counter = 1000;
	int err;

	tsr = bg_ptr->pdata->sensors[id].registers;
	/* Select single conversion mode */
	err = omap_control_readl(cdev, tsr->bgap_mode_ctrl, &temp);
	temp &= ~(1 << __ffs(tsr->mode_ctrl_mask));
	omap_control_writel(cdev, temp, tsr->bgap_mode_ctrl);

	/* Start of Conversion = 1 */
	err |= omap_control_readl(cdev, tsr->temp_sensor_ctrl, &temp);
	temp |= 1 << __ffs(tsr->bgap_soc_mask);
	omap_control_writel(cdev, temp, tsr->temp_sensor_ctrl);
	/* Wait until DTEMP is updated */
	err |= omap_control_readl(cdev, tsr->temp_sensor_ctrl, &temp);
	temp &= (tsr->bgap_dtemp_mask);
	while ((temp == 0) && --counter) {
		err |= omap_control_readl(cdev, tsr->temp_sensor_ctrl, &temp);
		temp &= (tsr->bgap_dtemp_mask);
	}
	/* Start of Conversion = 0 */
	err |= omap_control_readl(cdev, tsr->temp_sensor_ctrl, &temp);
	temp &= ~(1 << __ffs(tsr->bgap_soc_mask));
	err |= omap_control_writel(cdev, temp, tsr->temp_sensor_ctrl);

	return err ? -EIO : 0;
}

static int omap_bandgap_restore_ctxt(struct omap_bandgap *bg_ptr)
{
	struct device *cdev = bg_ptr->dev->parent;
	int i, err = 0;
	u32 temp = 0;

	for (i = 0; i < bg_ptr->pdata->sensor_count; i++) {
		struct temp_sensor_registers *tsr;
		struct temp_sensor_regval *rval;
		u32 val;

		rval = &bg_ptr->pdata->sensors[i].regval;
		tsr = bg_ptr->pdata->sensors[i].registers;

		err = omap_control_readl(cdev, tsr->bgap_counter, &val);
		if (val == 0) {
			err |= omap_control_writel(cdev, rval->bg_threshold,
						   tsr->bgap_threshold);
			err |= omap_control_writel(cdev, rval->tshut_threshold,
						   tsr->tshut_threshold);
			/* Force immediate temperature measurement and update
			 * of the DTEMP field
			 */
			omap_bandgap_force_single_read(bg_ptr, i);
			err |= omap_control_writel(cdev, rval->bg_counter,
						   tsr->bgap_counter);
			err |= omap_control_writel(cdev, rval->bg_mode_ctrl,
						   tsr->bgap_mode_ctrl);
			err |= omap_control_writel(cdev, rval->bg_ctrl,
						   tsr->bgap_mask_ctrl);
		} else {
			err |= omap_control_readl(cdev, tsr->temp_sensor_ctrl,
						 &temp);
			temp &= (tsr->bgap_dtemp_mask);
			if (temp == 0) {
				omap_bandgap_force_single_read(bg_ptr, i);
				err |= omap_control_readl(cdev,
							  tsr->bgap_mask_ctrl,
							  &temp);
				temp |= 1 << __ffs(tsr->mode_ctrl_mask);
				err |= omap_control_writel(cdev, temp,
							   tsr->bgap_mask_ctrl);
			}
		}
		if (err)
			dev_err(bg_ptr->dev, "could not save sensor %d\n", i);
	}

	return err ? -EIO : 0;
}

static int omap_bandgap_suspend(struct device *dev)
{
	struct omap_bandgap *bg_ptr = dev_get_drvdata(dev);
	int err;

	g_bg_ptr->bg_in_suspend = true;

	err = omap_bandgap_save_ctxt(bg_ptr);
	clk_disable(bg_ptr->fclock);

	return err;
}

static int omap_bandgap_resume(struct device *dev)
{
	struct omap_bandgap *bg_ptr = dev_get_drvdata(dev);

	clk_enable(bg_ptr->fclock);

	g_bg_ptr->bg_in_suspend = false;

	return omap_bandgap_restore_ctxt(bg_ptr);
}
static const struct dev_pm_ops omap_bandgap_dev_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(omap_bandgap_suspend,
				omap_bandgap_resume)
};

#define DEV_PM_OPS	(&omap_bandgap_dev_pm_ops)
#else
#define DEV_PM_OPS	NULL
#endif

void omap_bandgap_prepare_for_idle(void)
{
	/*
	 * If the System-wide Suspend/Resume is initiated, then clocks
	 * are taken care by the suspend/resume handlers. Hence nothing
	 * todo in the notifiers, so just return.
	 */
	if (g_bg_ptr->bg_in_suspend)
		return;

	clk_disable(g_bg_ptr->fclock);
	g_bg_ptr->bg_clk_idle = true;

}

void omap_bandgap_resume_after_idle(void)
{
	u8 i;
	/*
	 * If the System-wide Suspend/Resume is initiated, then clocks
	 * are taken care by the suspend/resume handlers. Hence nothing
	 * todo in the notifiers, so just return.
	 */
	if (g_bg_ptr->bg_in_suspend)
		return;

	/* Enable clock for sensor, if it was disabled during idle */
	if (g_bg_ptr->bg_clk_idle) {
		clk_enable(g_bg_ptr->fclock);
		/*
		 * Since the clocks are gated, the temperature reading
		 * is not correct. Hence force the single read to get the
		 * current temperature and then configure back to continuous
		 * montior mode.
		 */
		for (i = 0; i < g_bg_ptr->pdata->sensor_count; i++)
			omap_bandgap_force_single_read(g_bg_ptr, i);

		enable_continuous_mode(g_bg_ptr);

		g_bg_ptr->bg_clk_idle = false;
	}

}

static struct platform_driver omap_bandgap_sensor_driver = {
	.probe = omap_bandgap_probe,
	.remove = omap_bandgap_remove,
	.driver = {
			.name = "omap-bandgap",
			.pm = DEV_PM_OPS,
			.of_match_table	= of_omap_bandgap_match,
	},
};

module_platform_driver(omap_bandgap_sensor_driver);
early_platform_init("early_omap_temperature", &omap_bandgap_sensor_driver);

MODULE_DESCRIPTION("OMAP4+ bandgap temperature sensor driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:omap-bandgap");
MODULE_AUTHOR("Texas Instrument Inc.");
