/*
 * OMAP4 Bandgap temperature sensor driver
 *
 * Copyright (C) 2011-2012 Texas Instruments Incorporated - http://www.ti.com/
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
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/clk.h>
#include <linux/gpio.h>
#include <linux/platform_device.h>
#include <linux/err.h>
#include <linux/types.h>
#include <linux/mutex.h>
#include <linux/reboot.h>
#include <linux/of_platform.h>
#include <linux/of_irq.h>
#include <linux/mfd/omap_control.h>

#include "omap-bandgap.h"

/* Global bandgap pointer used in PM notifier handlers. */
static struct omap_bandgap *g_bg_ptr;

static int omap_bandgap_power(struct omap_bandgap *bg_ptr, bool on)
{
	struct device *cdev = bg_ptr->dev->parent;
	struct temp_sensor_registers *tsr;
	int i, r = 0;
	u32 ctrl;

	if (!OMAP_BANDGAP_HAS(bg_ptr, POWER_SWITCH))
		return 0;

	for (i = 0; i < bg_ptr->conf->sensor_count; i++) {
		tsr = bg_ptr->conf->sensors[i].registers;
		r |= omap_control_readl(cdev, tsr->temp_sensor_ctrl, &ctrl);
		ctrl &= ~tsr->bgap_tempsoff_mask;
		/* For OMAP4460/70 the TEMPSOFF bit must be set
		 * to '1' each time before Bandgap clocks are gated.
		 * The power switching procedure is described in
		 * OMAP4460 TRM section 18.4.10 (19.4.10 for OMAP4470).
		 */
		ctrl |= !on << __ffs(tsr->bgap_tempsoff_mask);

		/* write BGAP_TEMPSOFF should be reset to 0 */
		r |= omap_control_writel(cdev, ctrl, tsr->temp_sensor_ctrl);
	}

	return r;
}

static u32 omap_bandgap_read_temp(struct omap_bandgap *bg_ptr, int id)
{
	struct device *cdev = bg_ptr->dev->parent;
	struct temp_sensor_registers *tsr;
	int r;
	u32 temp, ctrl, reg;

	tsr = bg_ptr->conf->sensors[id].registers;
	reg = tsr->temp_sensor_ctrl;

	if (OMAP_BANDGAP_HAS(bg_ptr, FREEZE_BIT)) {
		omap_control_readl(cdev, tsr->bgap_mask_ctrl, &ctrl);
		ctrl |= tsr->mask_freeze_mask;
		omap_control_writel(cdev, ctrl, tsr->bgap_mask_ctrl);
		/*
		 * In case we cannot read from cur_dtemp / dtemp_0,
		 * then we read from the last valid temp read
		 */
		reg = tsr->ctrl_dtemp_1;
	}

	/* read temperature */
	r = omap_control_readl(cdev, reg, &temp);
	temp &= tsr->bgap_dtemp_mask;

	if (OMAP_BANDGAP_HAS(bg_ptr, FREEZE_BIT)) {
		omap_control_readl(cdev, tsr->bgap_mask_ctrl, &ctrl);
		ctrl &= ~tsr->mask_freeze_mask;
		omap_control_writel(cdev, ctrl, tsr->bgap_mask_ctrl);
	}

	return temp;
}

/* This is the Talert handler. Call it only if HAS(TALERT) is set */
static irqreturn_t talert_irq_handler(int irq, void *data)
{
	struct omap_bandgap *bg_ptr = data;
	struct device *cdev = bg_ptr->dev->parent;
	struct temp_sensor_registers *tsr;
	u32 t_hot = 0, t_cold = 0, ctrl;
	int i, r;

	bg_ptr = data;
	/* Read the status of t_hot */
	for (i = 0; i < bg_ptr->conf->sensor_count; i++) {
		tsr = bg_ptr->conf->sensors[i].registers;
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

		dev_dbg(bg_ptr->dev,
			"%s: IRQ from %s sensor: hotevent %d coldevent %d\n",
			__func__, bg_ptr->conf->sensors[i].domain,
			t_hot, t_cold);

		/* report temperature to whom may concern */
		if (bg_ptr->conf->report_temperature)
			bg_ptr->conf->report_temperature(bg_ptr, i);
	}

	return IRQ_HANDLED;
}

/* This is the Tshut handler. Call it only if HAS(TSHUT) is set */
static irqreturn_t omap_bandgap_tshut_irq_handler(int irq, void *data)
{
	pr_emerg("%s: TSHUT temperature reached. Needs shut down...\n",
		 __func__);
	orderly_poweroff(true);

	return IRQ_HANDLED;
}

static
int adc_to_temp_conversion(struct omap_bandgap *bg_ptr, int id, int adc_val,
			   u32 *t)
{
	struct temp_sensor_data *ts_data = bg_ptr->conf->sensors[id].ts_data;

	/* look up for temperature in the table and return the temperature */
	if (adc_val < ts_data->adc_start_val || adc_val > ts_data->adc_end_val)
		return -ERANGE;

	*t = bg_ptr->conv_table[adc_val - ts_data->adc_start_val];

	return 0;
}

static int temp_to_adc_conversion(long temp, struct omap_bandgap *bg_ptr, int i,
				  u32 *adc)
{
	struct temp_sensor_data *ts_data = bg_ptr->conf->sensors[i].ts_data;
	int high, low, mid;

	low = 0;
	high = ts_data->adc_end_val - ts_data->adc_start_val;
	mid = (high + low) / 2;

	if (temp < bg_ptr->conv_table[low] || temp > bg_ptr->conv_table[high])
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

/* Talert masks. Call it only if HAS(TALERT) is set */
static int temp_sensor_unmask_interrupts(struct omap_bandgap *bg_ptr, int id,
					 u32 t_hot, u32 t_cold)
{
	struct device *cdev = bg_ptr->dev->parent;
	struct temp_sensor_registers *tsr;
	u32 temp, reg_val;
	int err;

	tsr = bg_ptr->conf->sensors[id].registers;
	err = omap_control_readl(cdev, tsr->bgap_mask_ctrl, &reg_val);

	/* Read the current on die temperature */
	temp = omap_bandgap_read_temp(bg_ptr, id);

	if (temp < t_hot)
		reg_val |= tsr->mask_hot_mask;
	else
		reg_val &= ~tsr->mask_hot_mask;

	if (t_cold < temp)
		reg_val |= tsr->mask_cold_mask;
	else
		reg_val &= ~tsr->mask_cold_mask;
	err |= omap_control_writel(cdev, reg_val, tsr->bgap_mask_ctrl);

	if (err) {
		dev_err(bg_ptr->dev, "failed to unmask interrupts\n");
		return -EIO;
	}

	return 0;
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

/* Talert Thot threshold. Call it only if HAS(TALERT) is set */
static
int temp_sensor_configure_thot(struct omap_bandgap *bg_ptr, int id, int t_hot)
{
	struct temp_sensor_data *ts_data = bg_ptr->conf->sensors[id].ts_data;
	struct device *cdev = bg_ptr->dev->parent;
	struct temp_sensor_registers *tsr;
	u32 thresh_val, reg_val, cold;
	int err;

	tsr = bg_ptr->conf->sensors[id].registers;
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

/* Talert Thot and Tcold thresholds. Call it only if HAS(TALERT) is set */
static
int temp_sensor_init_talert_thresholds(struct omap_bandgap *bg_ptr, int id,
				       int t_hot, int t_cold)
{
	struct device *cdev = bg_ptr->dev->parent;
	struct temp_sensor_registers *tsr;
	u32 reg_val, thresh_val;
	int err;

	tsr = bg_ptr->conf->sensors[id].registers;
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

/* Talert Tcold threshold. Call it only if HAS(TALERT) is set */
static
int temp_sensor_configure_tcold(struct omap_bandgap *bg_ptr, int id,
				int t_cold)
{
	struct temp_sensor_data *ts_data = bg_ptr->conf->sensors[id].ts_data;
	struct device *cdev = bg_ptr->dev->parent;
	struct temp_sensor_registers *tsr;
	u32 thresh_val, reg_val, hot;
	int err;

	tsr = bg_ptr->conf->sensors[id].registers;
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

/* This is Tshut config. Call it only if HAS(TSHUT_CONFIG) is set */
static int temp_sensor_configure_tshut(struct omap_bandgap *bg_ptr,
				       int id, int tshut_cold, int tshut_hot)
{
	struct device *cdev = bg_ptr->dev->parent;
	struct temp_sensor_registers *tsr;
	u32 reg_val = 0;
	int err;

	tsr = bg_ptr->conf->sensors[id].registers;

	/* If one time writable register ensure it wasn't configured before,
	 * if not using the HW TSHUT feature, just return success
	 */
	if (OMAP_BANDGAP_HAS(bg_ptr, TSHUT_CONFIG_ONCE)) {
		err = omap_control_readl(cdev, tsr->tshut_threshold, &reg_val);
		if (err) {
			dev_err(bg_ptr->dev, "failed to read tshut register\n");
			return err;
		} else if (reg_val) {
			return 0;
		}

	} else if (!OMAP_BANDGAP_HAS(bg_ptr, TSHUT_CONFIG)) {
		return 0;
	}

	reg_val = (tshut_cold << __ffs(tsr->tshut_cold_mask)) &
							tsr->tshut_cold_mask;
	reg_val |= (tshut_hot << __ffs(tsr->tshut_hot_mask)) &
							tsr->tshut_hot_mask;
	err = omap_control_writel(cdev, reg_val, tsr->tshut_threshold);
	if (err) {
		dev_err(bg_ptr->dev, "failed to reprogram tshut thot\n");
		return -EIO;
	}

	return 0;
}

/* This is counter config. Call it only if HAS(COUNTER) is set */
static int configure_temp_sensor_counter(struct omap_bandgap *bg_ptr, int id,
					 u32 counter)
{
	struct device *cdev = bg_ptr->dev->parent;
	struct temp_sensor_registers *tsr;
	u32 val;
	int err;

	tsr = bg_ptr->conf->sensors[id].registers;
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

#define bandgap_is_valid(b)						\
			(!IS_ERR_OR_NULL(b))
#define bandgap_is_valid_sensor_id(b, i)				\
			((i) >= 0 && (i) < (b)->conf->sensor_count)
static inline int omap_bandgap_validate(struct omap_bandgap *bg_ptr, int id)
{
	if (!bandgap_is_valid(bg_ptr)) {
		pr_err("%s: invalid bandgap pointer\n", __func__);
		return -EINVAL;
	}

	if (!bandgap_is_valid_sensor_id(bg_ptr, id)) {
		dev_err(bg_ptr->dev, "%s: sensor id out of range (%d)\n",
			__func__, id);
		return -ERANGE;
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
	struct device *cdev;
	struct temp_sensor_registers *tsr;
	u32 temp;
	int ret;

	ret = omap_bandgap_validate(bg_ptr, id);
	if (ret)
		return ret;

	/* If not using the HW Alert feature, just return success */
	if (!OMAP_BANDGAP_HAS(bg_ptr, TALERT))
		return 0;

	cdev = bg_ptr->dev->parent;
	tsr = bg_ptr->conf->sensors[id].registers;
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
	struct temp_sensor_data *ts_data;
	struct temp_sensor_registers *tsr;
	u32 t_hot;
	int ret;

	ret = omap_bandgap_validate(bg_ptr, id);
	if (ret)
		return ret;

	/* If not using the HW Alert feature, just return success */
	if (!OMAP_BANDGAP_HAS(bg_ptr, TALERT))
		return 0;

	ts_data = bg_ptr->conf->sensors[id].ts_data;
	tsr = bg_ptr->conf->sensors[id].registers;

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
	struct device *cdev;
	struct temp_sensor_registers *tsr;
	u32 temp;
	int ret;

	ret = omap_bandgap_validate(bg_ptr, id);
	if (ret)
		return ret;

	/* If not using the HW Alert feature, just return success */
	if (!OMAP_BANDGAP_HAS(bg_ptr, TALERT))
		return 0;

	cdev = bg_ptr->dev->parent;
	tsr = bg_ptr->conf->sensors[id].registers;
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
	struct temp_sensor_data *ts_data;
	struct temp_sensor_registers *tsr;
	u32 t_cold;
	int ret;

	ret = omap_bandgap_validate(bg_ptr, id);
	if (ret)
		return ret;

	/* If not using the HW Alert feature, just return success */
	if (!OMAP_BANDGAP_HAS(bg_ptr, TALERT))
		return 0;

	ts_data = bg_ptr->conf->sensors[id].ts_data;
	tsr = bg_ptr->conf->sensors[id].registers;
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
	struct device *cdev;
	struct temp_sensor_registers *tsr;
	u32 time;
	int ret;

	ret = omap_bandgap_validate(bg_ptr, id);
	if (ret)
		return ret;

	if (!OMAP_BANDGAP_HAS(bg_ptr, COUNTER))
		return 0;

	tsr = bg_ptr->conf->sensors[id].registers;
	cdev = bg_ptr->dev->parent;
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
	int ret = omap_bandgap_validate(bg_ptr, id);
	if (ret)
		return ret;

	if (!OMAP_BANDGAP_HAS(bg_ptr, COUNTER))
		return 0;

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
	struct device *cdev;
	struct temp_sensor_registers *tsr;
	u32 temp;
	int ret;

	ret = omap_bandgap_validate(bg_ptr, id);
	if (ret)
		return ret;

	tsr = bg_ptr->conf->sensors[id].registers;
	cdev = bg_ptr->dev->parent;
	mutex_lock(&bg_ptr->bg_mutex);
	temp = omap_bandgap_read_temp(bg_ptr, id);
	mutex_unlock(&bg_ptr->bg_mutex);

	ret |= adc_to_temp_conversion(bg_ptr, id, temp, &temp);
	if (ret) {
		dump_stack();
		return -EIO;
	}

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
	int ret = omap_bandgap_validate(bg_ptr, id);
	if (ret)
		return ret;

	bg_ptr->conf->sensors[id].data = data;

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
	int ret = omap_bandgap_validate(bg_ptr, id);
	if (ret)
		return ERR_PTR(ret);

	return bg_ptr->conf->sensors[id].data;
}

static int
omap_bandgap_force_single_read(struct omap_bandgap *bg_ptr, int id)
{
	struct device *cdev = bg_ptr->dev->parent;
	struct temp_sensor_registers *tsr;
	u32 temp = 0, counter = 1000;
	int err = 0;

	tsr = bg_ptr->conf->sensors[id].registers;
	/* Select single conversion mode */
	if (OMAP_BANDGAP_HAS(bg_ptr, MODE_CONFIG)) {
		err = omap_control_readl(cdev, tsr->bgap_mode_ctrl, &temp);
		temp &= ~(1 << __ffs(tsr->mode_ctrl_mask));
		omap_control_writel(cdev, temp, tsr->bgap_mode_ctrl);
	}

	/* Start of Conversion = 1 */
	err |= omap_control_readl(cdev, tsr->temp_sensor_ctrl, &temp);
	temp |= 1 << __ffs(tsr->bgap_soc_mask);
	omap_control_writel(cdev, temp, tsr->temp_sensor_ctrl);
	/* Wait until DTEMP is updated */
	temp = omap_bandgap_read_temp(bg_ptr, id);

	while ((temp == 0) && --counter)
		temp = omap_bandgap_read_temp(bg_ptr, id);

	/* Start of Conversion = 0 */
	err |= omap_control_readl(cdev, tsr->temp_sensor_ctrl, &temp);
	temp &= ~(1 << __ffs(tsr->bgap_soc_mask));
	err |= omap_control_writel(cdev, temp, tsr->temp_sensor_ctrl);

	return err ? -EIO : 0;
}

/**
 * enable_continuous_mode() - One time enabling of continuous conversion mode
 * @bg_ptr - pointer to scm instance
 *
 * Call this function only if HAS(MODE_CONFIG) is set
 */
static int enable_continuous_mode(struct omap_bandgap *bg_ptr)
{
	struct device *cdev = bg_ptr->dev->parent;
	struct temp_sensor_registers *tsr;
	int i, r = 0;
	u32 val;

	for (i = 0; i < bg_ptr->conf->sensor_count; i++) {
		/* Perform a single read just before enabling continuous */
		r = omap_bandgap_force_single_read(bg_ptr, i);
		tsr = bg_ptr->conf->sensors[i].registers;
		r |= omap_control_readl(cdev, tsr->bgap_mode_ctrl, &val);

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

/* Initialization of Talert. Call it only if HAS(TALERT) is set */
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

static const struct of_device_id of_omap_bandgap_match[];

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

	switch (pdata->rev) {
#ifdef CONFIG_OMAP4_BG_TEMP_SENSOR_DATA
	case 1:
		if (cpu_is_omap447x())
			bg_ptr->conf = (void *)&omap4470_data;
		else
			bg_ptr->conf = (void *)&omap4460_data;
		break;
#endif
#ifdef CONFIG_OMAP5_BG_TEMP_SENSOR_DATA
	case 2:
		bg_ptr->conf = (void *)&omap5430_data;
		break;
#endif
	default: /* No sensor Data Fail the probe */
		return ERR_PTR(-ENODEV);
	}

	bg_ptr->irq = platform_get_irq(pdev, 0);
	if (bg_ptr->irq < 0) {
		dev_err(&pdev->dev, "get_irq failed\n");
		return ERR_PTR(-EINVAL);
	}

	if (OMAP_BANDGAP_HAS(bg_ptr, TSHUT)) {
		bg_ptr->tshut_gpio = pdata->tshut_gpio;
		if (!gpio_is_valid(bg_ptr->tshut_gpio)) {
			dev_err(&pdev->dev, "invalid gpio for tshut (%d)\n",
				bg_ptr->tshut_gpio);
			return ERR_PTR(-EINVAL);
		}
	}

	/*
	 * On OMAP5 ES1.0, due to TSHUT HW errata, the TALERT feature
	 * will be used by PPA to mointor the temperature and does
	 * HW reset if the junction temperature crosses 118degC. Due to
	 * this, driver SW will not use this feature on Non-GP devices,
	 * but will use on GP device (since PPA will not be running on
	 * GP devices).
	 */
	if (((omap_rev() == OMAP5430_REV_ES1_0) ||
		(omap_rev() == OMAP5432_REV_ES1_0)) &&
		(omap_type() != OMAP2_DEVICE_TYPE_GP))
			bg_ptr->conf->features &= ~OMAP_BANDGAP_FEATURE_TALERT;

	/*
	 * On OMAP5 ES1.0, due to TSHUT HW errata, thresholds for TSHUT
	 * are set by PPA. Due to this, driver SW will not set these
	 * tshut thershold on Non-GP devices, but will set it on GP device
	 * (since PPA will not be running on GP devices).
	 */
	if (((omap_rev() == OMAP5430_REV_ES1_0) ||
		(omap_rev() == OMAP5432_REV_ES1_0)) &&
		(omap_type() != OMAP2_DEVICE_TYPE_GP))
			bg_ptr->conf->features &=
					~OMAP_BANDGAP_FEATURE_TSHUT_CONFIG;

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
		bg_ptr->conf = of_id->data;

	if (OMAP_BANDGAP_HAS(bg_ptr, TSHUT)) {
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
	u32 val;

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

	if (OMAP_BANDGAP_HAS(bg_ptr, TSHUT)) {
		ret = omap_bandgap_tshut_init(bg_ptr, pdev);
		if (ret) {
			dev_err(&pdev->dev,
				"failed to initialize system tshut IRQ\n");
			return ret;
		}
	}

	bg_ptr->fclock = clk_get(NULL, bg_ptr->conf->fclock_name);
	ret = IS_ERR_OR_NULL(bg_ptr->fclock);
	if (ret) {
		dev_err(&pdev->dev, "failed to request fclock reference\n");
		goto free_irqs;
	}

	bg_ptr->div_clk = clk_get(NULL,  bg_ptr->conf->div_ck_name);
	ret = IS_ERR_OR_NULL(bg_ptr->div_clk);
	if (ret) {
		dev_err(&pdev->dev,
			"failed to request div_ts_ck clock ref\n");
		goto free_irqs;
	}

	bg_ptr->conv_table = bg_ptr->conf->conv_table;
	for (i = 0; i < bg_ptr->conf->sensor_count; i++) {
		struct temp_sensor_registers *tsr;

		tsr = bg_ptr->conf->sensors[i].registers;
		/*
		 * check if the efuse has a non-zero value if not
		 * it is an untrimmed sample and the temperatures
		 * may not be accurate
		 */
		ret = omap_control_readl(cdev, tsr->bgap_efuse, &val);
		if (ret || !val)
			dev_info(&pdev->dev,
				 "Non-trimmed BGAP, Temp not accurate\n");

		/*
		 * COBRA-BUG-175: Workaround for spurious TSHUT. Set hot
		 * threshold to 0x3FF, which disables H/W TSHUT mechanism.
		 */
		if (cpu_is_omap54xx() &&
			(omap_rev() == OMAP5430_REV_ES1_0 ||
			 omap_rev() == OMAP5432_REV_ES1_0))
			bg_ptr->conf->sensors[i].ts_data->tshut_hot =
						OMAP54XX_ES1_0_TSHUT_HOT;
	}

	clk_rate = clk_round_rate(bg_ptr->div_clk,
				  bg_ptr->conf->sensors[0].ts_data->max_freq);
	if (clk_rate < bg_ptr->conf->sensors[0].ts_data->min_freq ||
	    clk_rate == 0xffffffff) {
		ret = -ENODEV;
		dev_err(&pdev->dev, "wrong clock rate (%d)\n", clk_rate);
		goto put_clks;
	}

	ret = clk_set_rate(bg_ptr->div_clk, clk_rate);
	if (ret) {
		dev_err(&pdev->dev, "Cannot set clock rate\n");
		goto put_clks;
	}

	bg_ptr->clk_rate = clk_rate;
	if (OMAP_BANDGAP_HAS(bg_ptr, CLK_CTRL))
		clk_enable(bg_ptr->fclock);


	mutex_init(&bg_ptr->bg_mutex);
	bg_ptr->dev = &pdev->dev;
	platform_set_drvdata(pdev, bg_ptr);

	omap_bandgap_power(bg_ptr, true);

	/* Set default counter to 1 for now */
	if (OMAP_BANDGAP_HAS(bg_ptr, COUNTER))
		for (i = 0; i < bg_ptr->conf->sensor_count; i++)
			configure_temp_sensor_counter(bg_ptr, i, 1);

	for (i = 0; i < bg_ptr->conf->sensor_count; i++) {
		struct temp_sensor_data *ts_data;
		struct temp_sensor_registers *tsr;

		ts_data = bg_ptr->conf->sensors[i].ts_data;
		tsr = bg_ptr->conf->sensors[i].registers;

		if (OMAP_BANDGAP_HAS(bg_ptr, TALERT))
			temp_sensor_init_talert_thresholds(bg_ptr, i,
							   ts_data->t_hot,
							   ts_data->t_cold);

		if (OMAP_BANDGAP_HAS(bg_ptr, TSHUT_CONFIG) ||
		    OMAP_BANDGAP_HAS(bg_ptr, TSHUT_CONFIG_ONCE)) {
			/*
			 * On OMAP54xx ES2.0, TSHUT values are loaded from
			 * e-fuse. To override the e-fuse value, need to set
			 * MUXCTRL bit(31) to 1.
			 * Older TRM (ver J) has this bit information. Later TRM
			 * shows this bit as reserved. This code need re-visit
			 * after the wakeup since SW control will be disabled
			 * in production devices.
			 */
			if (cpu_is_omap543x() &&
			    (omap_rev() != OMAP5430_REV_ES1_0) &&
			    (omap_rev() != OMAP5432_REV_ES1_0)) {
				struct temp_sensor_registers *tsr;
				tsr = bg_ptr->conf->sensors[i].registers;

				omap_control_writel(cdev,
						(1 << tsr->tshut_efuse_shift),
						tsr->tshut_threshold);
			}

			temp_sensor_configure_tshut(bg_ptr, i,
						    ts_data->tshut_cold,
						    ts_data->tshut_hot);
		}
	}

	if (OMAP_BANDGAP_HAS(bg_ptr, MODE_CONFIG))
		enable_continuous_mode(bg_ptr);

	/* Set .250 seconds time as default counter */
	if (OMAP_BANDGAP_HAS(bg_ptr, COUNTER))
		for (i = 0; i < bg_ptr->conf->sensor_count; i++)
			configure_temp_sensor_counter(bg_ptr, i,
					      bg_ptr->clk_rate / 4);

	/* Every thing is good? Then expose the sensors */
	for (i = 0; i < bg_ptr->conf->sensor_count; i++) {
		char *domain;

		domain = bg_ptr->conf->sensors[i].domain;
		if (bg_ptr->conf->expose_sensor)
			bg_ptr->conf->expose_sensor(bg_ptr, i, domain);
	}

	/*
	 * Enable the Interrupts once everything is set. Otherwise irq handler
	 * might be called as soon as it is enabled where as rest of framework
	 * is still getting initialised.
	 */
	if (OMAP_BANDGAP_HAS(bg_ptr, TALERT)) {
		ret = omap_bandgap_talert_init(bg_ptr, pdev);
		if (ret) {
			dev_err(&pdev->dev, "failed to initialize Talert IRQ\n");
			i = bg_ptr->conf->sensor_count;
			goto put_clks;
		}
	}

	g_bg_ptr = bg_ptr;
	g_bg_ptr->bg_clk_idle = false;

	return 0;

put_clks:
	if (OMAP_BANDGAP_HAS(bg_ptr, CLK_CTRL))
		clk_disable(bg_ptr->fclock);
	clk_put(bg_ptr->fclock);
	clk_put(bg_ptr->div_clk);
free_irqs:
	if (OMAP_BANDGAP_HAS(bg_ptr, TSHUT)) {
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
	for (i = 0; i < bg_ptr->conf->sensor_count; i++)
		if (bg_ptr->conf->remove_sensor)
			bg_ptr->conf->remove_sensor(bg_ptr, i);

	omap_bandgap_power(bg_ptr, false);

	if (OMAP_BANDGAP_HAS(bg_ptr, CLK_CTRL))
		clk_disable(bg_ptr->fclock);
	clk_put(bg_ptr->fclock);
	clk_put(bg_ptr->div_clk);
	if (OMAP_BANDGAP_HAS(bg_ptr, TALERT))
		free_irq(bg_ptr->irq, bg_ptr);
	if (OMAP_BANDGAP_HAS(bg_ptr, TSHUT)) {
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

	for (i = 0; i < bg_ptr->conf->sensor_count; i++) {
		struct temp_sensor_registers *tsr;
		struct temp_sensor_regval *rval;

		rval = &bg_ptr->conf->sensors[i].regval;
		tsr = bg_ptr->conf->sensors[i].registers;

		if (OMAP_BANDGAP_HAS(bg_ptr, MODE_CONFIG))
			err = omap_control_readl(cdev, tsr->bgap_mode_ctrl,
						 &rval->bg_mode_ctrl);

		if (OMAP_BANDGAP_HAS(bg_ptr, COUNTER))
			err |= omap_control_readl(cdev,	tsr->bgap_counter,
						&rval->bg_counter);

		if (OMAP_BANDGAP_HAS(bg_ptr, TALERT)) {
			err |= omap_control_readl(cdev,	tsr->bgap_mask_ctrl,
					  &rval->bg_ctrl);

			err |= omap_control_readl(cdev, tsr->bgap_threshold,
						  &rval->bg_threshold);
		}

		if (OMAP_BANDGAP_HAS(bg_ptr, TSHUT_CONFIG) ||
		    OMAP_BANDGAP_HAS(bg_ptr, TSHUT_CONFIG_ONCE))
			err |= omap_control_readl(cdev, tsr->tshut_threshold,
						  &rval->tshut_threshold);

		if (err)
			dev_err(bg_ptr->dev, "could not save sensor %d\n", i);
	}

	return err ? -EIO : 0;
}

static int omap_bandgap_restore_ctxt(struct omap_bandgap *bg_ptr)
{
	struct device *cdev = bg_ptr->dev->parent;
	int i, err = 0;
	u32 val = 0;

	for (i = 0; i < bg_ptr->conf->sensor_count; i++) {
		struct temp_sensor_registers *tsr;
		struct temp_sensor_regval *rval;

		rval = &bg_ptr->conf->sensors[i].regval;
		tsr = bg_ptr->conf->sensors[i].registers;

		if (OMAP_BANDGAP_HAS(bg_ptr, TSHUT_CONFIG)) {
			err |= omap_control_writel(cdev, rval->tshut_threshold,
							tsr->tshut_threshold);
		} else if (OMAP_BANDGAP_HAS(bg_ptr, TSHUT_CONFIG_ONCE)) {
			err |= omap_control_readl(cdev, tsr->tshut_threshold,
						  &val);
			if (!(err || val))
				err |= omap_control_writel(cdev,
							rval->tshut_threshold,
							tsr->tshut_threshold);
		}
		/* Force immediate temperature measurement and update
		 * of the DTEMP field
		 */
		omap_bandgap_force_single_read(bg_ptr, i);
		if (OMAP_BANDGAP_HAS(bg_ptr, COUNTER))
			err |= omap_control_writel(cdev, rval->bg_counter,
						tsr->bgap_counter);

		if (OMAP_BANDGAP_HAS(bg_ptr, TALERT)) {
			err = omap_control_writel(cdev, rval->bg_threshold,
							tsr->bgap_threshold);

			err |= omap_control_writel(cdev, rval->bg_ctrl,
							tsr->bgap_mask_ctrl);
		}

		if (OMAP_BANDGAP_HAS(bg_ptr, MODE_CONFIG))
			err |= omap_control_writel(cdev, rval->bg_mode_ctrl,
							tsr->bgap_mode_ctrl);
		if (err)
			dev_err(bg_ptr->dev, "could not save sensor %d\n", i);
	}

	return err ? -EIO : 0;
}

static int omap_bandgap_suspend(struct device *dev)
{
	struct omap_bandgap *bg_ptr = dev_get_drvdata(dev);
	int err;

	err = omap_bandgap_save_ctxt(bg_ptr);
	omap_bandgap_power(bg_ptr, false);

	if (OMAP_BANDGAP_HAS(bg_ptr, CLK_CTRL))
		clk_disable(bg_ptr->fclock);

	return err;
}

static int omap_bandgap_resume(struct device *dev)
{
	struct omap_bandgap *bg_ptr = dev_get_drvdata(dev);
	struct device *cdev = bg_ptr->dev->parent;
	struct temp_sensor_registers *tsr;
	u32 i, val = 0;

	/* Disable continuous mode first */
	if (OMAP_BANDGAP_HAS(bg_ptr, MODE_CONFIG)) {
		for (i = 0; i < bg_ptr->conf->sensor_count; i++) {
			tsr = bg_ptr->conf->sensors[i].registers;
			omap_control_readl(cdev, tsr->bgap_mode_ctrl, &val);
			val &= ~(1 << __ffs(tsr->mode_ctrl_mask));
			omap_control_writel(cdev, val, tsr->bgap_mode_ctrl);
		}
	}


	if (OMAP_BANDGAP_HAS(bg_ptr, CLK_CTRL))
		clk_enable(bg_ptr->fclock);

	omap_bandgap_power(bg_ptr, true);

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
	 * These functions are directly called from PM framework
	 * during CpuIdle execution. Hence need to check if the
	 * driver is initialised before accessing any driver related
	 * data. If the driver probe has failed, then accessing the
	 * driver data will crash the system.
	 */
	if (!g_bg_ptr)
		return;

	omap_bandgap_power(g_bg_ptr, false);
	if (OMAP_BANDGAP_HAS(g_bg_ptr, CLK_CTRL))
		clk_disable(g_bg_ptr->fclock);

	g_bg_ptr->bg_clk_idle = true;

}

void omap_bandgap_resume_after_idle(void)
{
	u8 i;

	/*
	 * These functions are directly called from PM framework
	 * during CpuIdle execution. Hence need to check if the
	 * driver is initialised before accessing any driver related
	 * data. If the driver probe has failed, then accessing the
	 * driver data will crash the system.
	 */
	if (!g_bg_ptr)
		return;

	/* Enable clock for sensor, if it was disabled during idle */
	if (g_bg_ptr->bg_clk_idle) {
		if (OMAP_BANDGAP_HAS(g_bg_ptr, CLK_CTRL))
			clk_enable(g_bg_ptr->fclock);

		omap_bandgap_power(g_bg_ptr, true);
		/*
		 * Since the clocks are gated, the temperature reading
		 * is not correct. Hence force the single read to get the
		 * current temperature and then configure back to continuous
		 * montior mode.
		 */
		for (i = 0; i < g_bg_ptr->conf->sensor_count; i++)
			omap_bandgap_force_single_read(g_bg_ptr, i);

		enable_continuous_mode(g_bg_ptr);

		g_bg_ptr->bg_clk_idle = false;
	}

}

static const struct of_device_id of_omap_bandgap_match[] = {
#ifdef CONFIG_OMAP4_BG_TEMP_SENSOR_DATA
	/*
	 * TODO: Add support to 4430
	 */
	{
		.compatible = "ti,omap4460-bandgap",
		.data = (void *)&omap4460_data,
	},
	{
		.compatible = "ti,omap4470-bandgap",
		.data = (void *)&omap4470_data,
	},

#endif
#ifdef CONFIG_OMAP5_BG_TEMP_SENSOR_DATA
	{
		.compatible = "ti,omap5430-bandgap",
		.data = (void *)&omap5430_data,
	},
#endif
	/* Sentinel */
	{ },
};
MODULE_DEVICE_TABLE(of, of_omap_bandgap_match);

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
