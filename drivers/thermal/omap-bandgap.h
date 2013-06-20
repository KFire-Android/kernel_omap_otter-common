/*
 * OMAP4 Bandgap temperature sensor driver
 *
 * Copyright (C) 2011 Texas Instruments Incorporated - http://www.ti.com/
 * Contact:
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
#ifndef __OMAP_BANDGAP_H
#define __OMAP_BANDGAP_H

#include <linux/mutex.h>
#include <linux/types.h>
#include <linux/err.h>

/* Offsets from the base of temperature sensor registers */

#define OMAP4460_TEMP_SENSOR_CTRL_OFFSET	0x32C
#define OMAP4460_BGAP_CTRL_OFFSET		0x378
#define OMAP4460_BGAP_COUNTER_OFFSET		0x37C
#define OMAP4460_BGAP_THRESHOLD_OFFSET		0x380
#define OMAP4460_BGAP_TSHUT_OFFSET		0x384
#define OMAP4460_BGAP_STATUS_OFFSET		0x388
#define OMAP4460_FUSE_OPP_BGAP			0x260

#define OMAP4460_TSHUT_HOT		866	/* 100 deg C */
#define OMAP4460_TSHUT_COLD		817	/* 80 deg C */
#define OMAP4460_T_HOT			800	/* 73 deg C */
#define OMAP4460_T_COLD			795	/* 71 deg C */
#define OMAP4460_MAX_FREQ		1500000
#define OMAP4460_MIN_FREQ		1000000
#define OMAP4460_MIN_TEMP		-40000
#define OMAP4460_MAX_TEMP		123000
#define OMAP4460_HYST_VAL		5000
#define OMAP4460_ADC_START_VALUE	530
#define OMAP4460_ADC_END_VALUE		932

/* COBRA-BUG-175: set T_HOT to 0x3FF */
#define OMAP54XX_ES1_0_TSHUT_HOT	1023

#define OMAP5430_MPU_T_HOT		800
#define OMAP5430_MPU_T_COLD		795
#define OMAP5430_MPU_MAX_FREQ		1500000
#define OMAP5430_MPU_MIN_FREQ		1000000
#define OMAP5430_MPU_MIN_TEMP		-40000
#define OMAP5430_MPU_MAX_TEMP		125000
#define OMAP5430_MPU_HYST_VAL		5000
#define OMAP5430_ADC_START_VALUE	532
#define OMAP5430_ADC_END_VALUE		934
#define OMAP5430_ES2_ADC_START_VALUE	540
#define OMAP5430_ES2_ADC_END_VALUE	945

#define OMAP5430_GPU_T_HOT		800
#define OMAP5430_GPU_T_COLD		795
#define OMAP5430_GPU_MAX_FREQ		1500000
#define OMAP5430_GPU_MIN_FREQ		1000000
#define OMAP5430_GPU_MIN_TEMP		-40000
#define OMAP5430_GPU_MAX_TEMP		125000
#define OMAP5430_GPU_HYST_VAL		5000

#define OMAP5430_CORE_T_HOT		800
#define OMAP5430_CORE_T_COLD		795
#define OMAP5430_CORE_MAX_FREQ		1500000
#define OMAP5430_CORE_MIN_FREQ		1000000
#define OMAP5430_CORE_MIN_TEMP		-40000
#define OMAP5430_CORE_MAX_TEMP		125000
#define OMAP5430_CORE_HYST_VAL		5000

struct omap_bandgap_data;

/**
 * struct omap_bandgap - bandgap device structure
 * @dev: device pointer
 * @conf: platform data with sensor data
 * @fclock: pointer to functional clock of temperature sensor
 * @div_clk: pointer to parent clock of temperature sensor fclk
 * @conv_table: Pointer to adc to temperature conversion table
 * @bg_mutex: Mutex for sysfs, irq and PM
 * @irq: MPU Irq number for thermal alert
 * @tshut_gpio: GPIO where Tshut signal is routed
 * @clk_rate: Holds current clock rate
 * @bg_clk_idle: Flag to assist balanced clock enable/disable during idle
 */
struct omap_bandgap {
	struct device			*dev;
	struct omap_bandgap_data	*conf;
	struct clk			*fclock;
	struct clk			*div_clk;
	const int			*conv_table;
	struct mutex			bg_mutex; /* Mutex for irq and PM */
	int				irq;
	int				tshut_gpio;
	u32				clk_rate;
	bool				bg_clk_idle;
};

/**
 * The register offsets and bit fields might change across
 * OMAP versions hence populating them in this structure.
 */

struct temp_sensor_registers {
	u32	temp_sensor_ctrl;
	u32	bgap_tempsoff_mask;
	u32	bgap_soc_mask;
	u32	bgap_eocz_mask;
	u32	bgap_dtemp_mask;

	u32	bgap_mask_ctrl;
	u32	mask_hot_mask;
	u32	mask_cold_mask;
	u32	mask_sidlemode_mask;
	u32	mask_freeze_mask;
	u32	mask_clear_mask;
	u32	mask_clear_accum_mask;

	u32	bgap_mode_ctrl;
	u32	mode_ctrl_mask;

	u32	bgap_counter;
	u32	counter_mask;

	u32	bgap_threshold;
	u32	threshold_thot_mask;
	u32	threshold_tcold_mask;

	u32	tshut_threshold;
	u32	tshut_efuse_mask;
	u32	tshut_efuse_shift;
	u32	tshut_hot_mask;
	u32	tshut_cold_mask;

	u32	bgap_status;
	u32	status_clean_stop_mask;
	u32	status_bgap_alert_mask;
	u32	status_hot_mask;
	u32	status_cold_mask;

	u32	bgap_cumul_dtemp;
	u32	ctrl_dtemp_0;
	u32	ctrl_dtemp_1;
	u32	ctrl_dtemp_2;
	u32	ctrl_dtemp_3;
	u32	ctrl_dtemp_4;
	u32	bgap_efuse;
};

/**
 * The thresholds and limits for temperature sensors.
 */
struct temp_sensor_data {
	u32	tshut_hot;
	u32	tshut_cold;
	u32	t_hot;
	u32	t_cold;
	u32	min_freq;
	u32	max_freq;
	int	max_temp;
	int	min_temp;
	int	hyst_val;
	u32	adc_start_val;
	u32	adc_end_val;
	u32	update_int1;
	u32	update_int2;
	u32	stats_en;
	u32	avg_number;
	u32	avg_period;
	u32	safe_temp_trend;
	u32	stable_cnt;
};

/**
 * struct temp_sensor_regval - temperature sensor register values
 * @bg_mode_ctrl: temp sensor control register value
 * @bg_ctrl: bandgap ctrl register value
 * @bg_counter: bandgap counter value
 * @bg_threshold: bandgap threshold register value
 * @tshut_threshold: bandgap tshut register value
 */
struct temp_sensor_regval {
	u32			bg_mode_ctrl;
	u32			bg_ctrl;
	u32			bg_counter;
	u32			bg_threshold;
	u32			tshut_threshold;
};

/**
 * struct omap_temp_sensor - bandgap temperature sensor platform data
 * @ts_data: pointer to struct with thresholds, limits of temperature sensor
 * @registers: pointer to the list of register offsets and bitfields
 * @regval: temperature sensor register values
 * @domain: the name of the domain where the sensor is located
 * @data: thermal framework related data
 * @slope: slope value to compute hot-spot
 * @offset: offset  value to compute hot-spot
 */
struct omap_temp_sensor {
	struct temp_sensor_data		*ts_data;
	struct temp_sensor_registers	*registers;
	struct temp_sensor_regval	regval;
	char				*domain;
	void				*data;
	/* for hotspot extrapolation */
	int				slope;
	int				constant;
};

/**
 * struct omap_bandgap_data - bandgap platform data structure
 * @features: indicates various features supported by silicon
 * @conv_table: Pointer to adc to temperature conversion table
 * @fclock_name: clock name of the functional clock
 * @div_ck_nme: clock name of the clock divisor
 * @sensor_count: count of temperature sensor device in scm
 * @rev: Revision of the temperature sensor
 * @accurate: Accuracy of the temperature
 * @sensors: array of sensors present in this bandgap instance
 * @expose_sensor: callback to export sensor to thermal API
 */
struct omap_bandgap_data {
#define OMAP_BANDGAP_FEATURE_TSHUT		(1 << 0)
#define OMAP_BANDGAP_FEATURE_TSHUT_CONFIG	(1 << 1)
#define OMAP_BANDGAP_FEATURE_TALERT		(1 << 2)
#define OMAP_BANDGAP_FEATURE_MODE_CONFIG	(1 << 3)
#define OMAP_BANDGAP_FEATURE_COUNTER		(1 << 4)
#define OMAP_BANDGAP_FEATURE_POWER_SWITCH	(1 << 5)
#define OMAP_BANDGAP_FEATURE_CLK_CTRL		(1 << 6)
#define OMAP_BANDGAP_FEATURE_FREEZE_BIT		(1 << 7)
#define OMAP_BANDGAP_FEATURE_TSHUT_CONFIG_ONCE	(1 << 8)
#define OMAP_BANDGAP_HAS(b, f)			\
			((b)->conf->features & OMAP_BANDGAP_FEATURE_ ## f)
	unsigned int			features;
	int				*conv_table;
	char				*fclock_name;
	char				*div_ck_name;
	int				sensor_count;
	int				rev;
	bool				accurate;
	int (*report_temperature)(struct omap_bandgap *bg_ptr, int id);
	int (*expose_sensor)(struct omap_bandgap *bg_ptr, int id,
							char *domain);
	int (*remove_sensor)(struct omap_bandgap *bg_ptr, int id);

	/* this needs to be at the end */
	struct omap_temp_sensor		sensors[];
};


int omap_bandgap_read_thot(struct omap_bandgap *bg_ptr, int id, int *thot);
int omap_bandgap_write_thot(struct omap_bandgap *bg_ptr, int id, int val);
int omap_bandgap_read_tcold(struct omap_bandgap *bg_ptr, int id, int *tcold);
int omap_bandgap_write_tcold(struct omap_bandgap *bg_ptr, int id, int val);
int omap_bandgap_read_update_interval(struct omap_bandgap *bg_ptr, int id,
				      int *interval);
int omap_bandgap_write_update_interval(struct omap_bandgap *bg_ptr, int id,
				       u32 interval);
int omap_bandgap_read_temperature(struct omap_bandgap *bg_ptr, int id,
				  int *temperature);
int omap_bandgap_set_sensor_data(struct omap_bandgap *bg_ptr, int id,
				void *data);
void *omap_bandgap_get_sensor_data(struct omap_bandgap *bg_ptr, int id);

#ifdef CONFIG_OMAP4_BG_TEMP_SENSOR_DATA
extern struct omap_bandgap_data omap4460_data;
extern struct omap_bandgap_data omap4470_data;
#else
#define omap4460_data					NULL
#define omap4470_data					NULL
#endif

#ifdef CONFIG_OMAP5_BG_TEMP_SENSOR_DATA
extern struct omap_bandgap_data omap5430_data;
#else
#define omap5430_data					NULL
#endif

#ifdef CONFIG_OMAP_DIE_TEMP_SENSOR
int omap_thermal_expose_sensor(struct omap_bandgap *bg_ptr, int id,
					char *domain);
int omap_thermal_report_temperature(struct omap_bandgap *bg_ptr, int id);
int omap_thermal_remove_sensor(struct omap_bandgap *bg_ptr, int id);
#else
static inline int omap_thermal_expose_sensor(struct omap_bandgap *bg_ptr,
						int id, char *domain)
{
	return 0;
}
static
inline int omap_thermal_report_temperature(struct omap_bandgap *bg_ptr, int id)
{
	return 0;
}
static
inline int omap_thermal_remove_sensor(struct omap_bandgap *bg_ptr, int id)
{
	return 0;
}
#endif

#endif
