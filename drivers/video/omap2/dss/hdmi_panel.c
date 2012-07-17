/*
 * hdmi_panel.c
 *
 * HDMI library support functions for TI OMAP4 processors.
 *
 * Copyright (C) 2010-2011 Texas Instruments Incorporated - http://www.ti.com/
 * Authors:	Mythri P k <mythripk@ti.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/mutex.h>
#include <linux/module.h>
#include <linux/switch.h>
#include <video/omapdss.h>
#include <linux/slab.h>
#include <linux/gpio.h>

#include "dss.h"

static struct {
	struct omap_dss_device *dssdev;
	struct mutex hdmi_lock;
	struct switch_dev hpd_switch;
	int hpd_gpio;
#if defined(CONFIG_OMAP4_DSS_HDMI_AUDIO)
	/* protects calls to HDMI driver audio functionality */
	spinlock_t hdmi_sp_lock;
#endif
} hdmi;

int hdmi_get_current_hpd(void)
{
	return gpio_get_value(hdmi.hpd_gpio);
}

static irqreturn_t hpd_enable_handler(int irq, void *ptr)
{
	int hpd = hdmi_get_current_hpd();

	pr_info("hpd %d\n", hpd);
	hdmi_panel_hpd_handler(hpd);

	return IRQ_HANDLED;
}

static ssize_t hdmi_deepcolor_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int deepcolor;

	deepcolor = omapdss_hdmi_get_deepcolor();

	return snprintf(buf, PAGE_SIZE, "%d\n", deepcolor);
}

static ssize_t hdmi_deepcolor_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t size)
{
	int r, deepcolor, curr_deepcolor;

	r = kstrtoint(buf, 0, &deepcolor);
	if (r || deepcolor > 3)
		return -EINVAL;

	curr_deepcolor = omapdss_hdmi_get_deepcolor();

	if (deepcolor == curr_deepcolor)
		return size;

	if (hdmi.dssdev->state != OMAP_DSS_DISPLAY_ACTIVE)
		r = omapdss_hdmi_set_deepcolor(hdmi.dssdev, deepcolor, false);
	else
		r = omapdss_hdmi_set_deepcolor(hdmi.dssdev, deepcolor, true);
	if (r)
		return r;

	return size;
}

static DEVICE_ATTR(deepcolor, S_IRUGO | S_IWUSR, hdmi_deepcolor_show,
			hdmi_deepcolor_store);

static ssize_t hdmi_range_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int r;

	r = omapdss_hdmi_get_range();
	return snprintf(buf, PAGE_SIZE, "%d\n", r);
}

static ssize_t hdmi_range_store(struct device *dev,
	struct device_attribute *attr,
	const char *buf, size_t size)
{
	unsigned long range;
	int r = kstrtoul(buf, 0, &range);

	if (r || range > 1)
		return -EINVAL;

	r = omapdss_hdmi_set_range(range);
	if (r)
		return r;
	return size;
}

static DEVICE_ATTR(range, S_IRUGO | S_IWUSR, hdmi_range_show, hdmi_range_store);

static int hdmi_panel_probe(struct omap_dss_device *dssdev)
{
	struct omap_dss_hdmi_data *priv;
	int r;
	DSSDBG("ENTER hdmi_panel_probe\n");

	dssdev->panel.config = OMAP_DSS_LCD_TFT |
			OMAP_DSS_LCD_IVS | OMAP_DSS_LCD_IHS;

	hdmi.dssdev = dssdev;

	/* sysfs entry to provide user space control to set
	 * quantization range
	 */
	if (device_create_file(&dssdev->dev, &dev_attr_range))
		DSSERR("failed to create sysfs file\n");

	dssdev->panel.timings = (struct omap_video_timings){640, 480, 25175, 96, 16, 48, 2 , 11, 31};

	/* sysfs entry to to set deepcolor mode and hdmi_timings */
	if (device_create_file(&dssdev->dev, &dev_attr_deepcolor) ||
	    device_create_file(&dssdev->dev, &dev_attr_hdmi_timings))
		DSSERR("failed to create sysfs file\n");

	DSSDBG("hdmi_panel_probe x_res= %d y_res = %d\n",
		dssdev->panel.timings.x_res,
		dssdev->panel.timings.y_res);

	priv = hdmi.dssdev->data;

	r = gpio_request_one(priv->ct_cp_hpd_gpio, GPIOF_OUT_INIT_HIGH,
				"hdmi_gpio_ct_cp_hpd");
	if (r) {
		DSSERR("Could not get HDMI_CT_CP_HPD gpio\n");
		goto done_err;
	}

	r = gpio_request_one(priv->ls_oe_gpio, GPIOF_OUT_INIT_HIGH,
				"hdmi_gpio_ls_oe");
	if (r) {
		DSSERR("Could not get HDMI_GPIO_LS_OE gpio\n");
		goto done_err1;
	}

	r = gpio_request_one(priv->hpd_gpio, GPIOF_DIR_IN,
				"hdmi_gpio_hpd");
	if (r) {
		DSSERR("Could not get HDMI_HPD gpio\n");
		goto done_hpd_err;
	}

	hdmi.hpd_gpio = priv->hpd_gpio;
	r = request_threaded_irq(gpio_to_irq(hdmi.hpd_gpio),
		NULL, hpd_enable_handler,
		IRQF_DISABLED | IRQF_TRIGGER_RISING |
		IRQF_TRIGGER_FALLING, "hpd", NULL);
	if (r < 0) {
		pr_err("hdmi: request_irq %d failed\n",
			gpio_to_irq(hdmi.hpd_gpio));
		r = -EINVAL;
		goto err_irq;
	}
	return 0;

err_irq:
	gpio_free(priv->hpd_gpio);
done_hpd_err:
	gpio_free(priv->ls_oe_gpio);
done_err1:
	gpio_free(priv->ct_cp_hpd_gpio);
done_err:
	device_remove_file(&dssdev->dev, &dev_attr_deepcolor);
	device_remove_file(&dssdev->dev, &dev_attr_hdmi_timings);
	device_remove_file(&dssdev->dev, &dev_attr_range);
	return r;
}

static void hdmi_panel_remove(struct omap_dss_device *dssdev)
{
	struct omap_dss_hdmi_data *priv = dssdev->data;

	device_remove_file(&dssdev->dev, &dev_attr_deepcolor);
	device_remove_file(&dssdev->dev, &dev_attr_range);
	device_remove_file(&dssdev->dev, &dev_attr_hdmi_timings);
	free_irq(gpio_to_irq(hdmi.hpd_gpio), NULL);
	gpio_free(priv->hpd_gpio);
	gpio_free(priv->ls_oe_gpio);
	gpio_free(priv->ct_cp_hpd_gpio);
}

static int hdmi_panel_enable(struct omap_dss_device *dssdev)
{
	int r = 0;
	DSSDBG("ENTER hdmi_panel_enable\n");

	mutex_lock(&hdmi.hdmi_lock);

	if (dssdev->state != OMAP_DSS_DISPLAY_DISABLED) {
		r = -EINVAL;
		goto err;
	}

	r = omapdss_hdmi_display_enable(dssdev);
	if (r) {
		DSSERR("failed to power on\n");
		goto err;
	}

	dssdev->state = OMAP_DSS_DISPLAY_ACTIVE;

	hdmi_inform_power_on_to_cec(true);

err:
	mutex_unlock(&hdmi.hdmi_lock);

	return r;
}

static int hdmi_panel_3d_enable(struct omap_dss_device *dssdev,
				struct s3d_disp_info *info, int code)
{
	int r = 0;
	DSSDBG("ENTER hdmi_panel_3d_enable\n");

	mutex_lock(&hdmi.hdmi_lock);

	if (dssdev->state != OMAP_DSS_DISPLAY_DISABLED) {
		r = -EINVAL;
		goto err;
	}

	r = omapdss_hdmi_display_3d_enable(dssdev, info, code);
	if (r) {
		DSSERR("failed to power on\n");
		goto err;
	}

	dssdev->state = OMAP_DSS_DISPLAY_ACTIVE;

err:
	mutex_unlock(&hdmi.hdmi_lock);

	return r;
}

static void hdmi_panel_disable(struct omap_dss_device *dssdev)
{
	mutex_lock(&hdmi.hdmi_lock);

	if (dssdev->state == OMAP_DSS_DISPLAY_ACTIVE) {
		hdmi_inform_power_on_to_cec(false);
		omapdss_hdmi_display_disable(dssdev);
	}

	dssdev->state = OMAP_DSS_DISPLAY_DISABLED;

	mutex_unlock(&hdmi.hdmi_lock);
}

static int hdmi_panel_suspend(struct omap_dss_device *dssdev)
{
	int r = 0;

	mutex_lock(&hdmi.hdmi_lock);

	if (dssdev->state != OMAP_DSS_DISPLAY_ACTIVE) {
		r = -EINVAL;
		goto err;
	}

	dssdev->state = OMAP_DSS_DISPLAY_SUSPENDED;

	omapdss_hdmi_display_disable(dssdev);

err:
	mutex_unlock(&hdmi.hdmi_lock);

	return r;
}

static int hdmi_panel_resume(struct omap_dss_device *dssdev)
{
	int r = 0;

	mutex_lock(&hdmi.hdmi_lock);

	if (dssdev->state != OMAP_DSS_DISPLAY_SUSPENDED) {
		r = -EINVAL;
		goto err;
	}

	dssdev->state = OMAP_DSS_DISPLAY_DISABLED;
err:
	mutex_unlock(&hdmi.hdmi_lock);

	hdmi_panel_hpd_handler(hdmi_get_current_hpd());

	return r;
}

enum {
	HPD_STATE_OFF,
	HPD_STATE_START,
	HPD_STATE_EDID_TRYLAST = HPD_STATE_START + 5,
};

static struct hpd_worker_data {
	struct delayed_work dwork;
	atomic_t state;
} hpd_work;
static struct workqueue_struct *my_workq;

static void hdmi_hotplug_detect_worker(struct work_struct *work)
{
	struct hpd_worker_data *d = container_of(work, typeof(*d), dwork.work);
	struct omap_dss_device *dssdev = NULL;
	int state = atomic_read(&d->state);

	dssdev = hdmi.dssdev;
	if (dssdev == NULL) {
		DSSERR("%s NULL device\n", __func__);
		return;
	}

	DSSDBG("in hpd work %d, state=%d\n", state, dssdev->state);
	mutex_lock(&hdmi.hdmi_lock);
	if (state == HPD_STATE_OFF) {
		switch_set_state(&hdmi.hpd_switch, 0);
		hdmi_inform_hpd_to_cec(false);
		if (dssdev->state == OMAP_DSS_DISPLAY_ACTIVE) {
			hdmi_notify_hpd(dssdev, false);
			mutex_unlock(&hdmi.hdmi_lock);
			dssdev->driver->disable(dssdev);
			mutex_lock(&hdmi.hdmi_lock);
		}
		goto done;
	} else {
		if (state == HPD_STATE_START) {
			mutex_unlock(&hdmi.hdmi_lock);
			dssdev->driver->enable(dssdev);
			mutex_lock(&hdmi.hdmi_lock);
			hdmi_notify_hpd(dssdev, true);
		} else if (dssdev->state != OMAP_DSS_DISPLAY_ACTIVE ||
			   hdmi.hpd_switch.state) {
			/* powered down after enable - skip EDID read */
			goto done;
		} else if (hdmi_read_valid_edid()) {
			/* get monspecs from edid */
			hdmi_get_monspecs(dssdev);
			DSSDBG("panel size %d by %d\n",
					dssdev->panel.monspecs.max_x,
					dssdev->panel.monspecs.max_y);
			dssdev->panel.width_in_um =
					dssdev->panel.monspecs.max_x * 10000;
			dssdev->panel.height_in_um =
					dssdev->panel.monspecs.max_y * 10000;
			switch_set_state(&hdmi.hpd_switch, 1);
			hdmi_inform_hpd_to_cec(true);
			goto done;
		} else if (state == HPD_STATE_EDID_TRYLAST) {
			DSSDBG("Failed to read EDID after %d times. Giving up.",
						state - HPD_STATE_START);
			goto done;
		}
		if (atomic_add_unless(&d->state, 1, HPD_STATE_OFF))
			queue_delayed_work(my_workq, &d->dwork,
							msecs_to_jiffies(60));
	}
done:
	mutex_unlock(&hdmi.hdmi_lock);
}

int hdmi_panel_hpd_handler(int hpd)
{
	__cancel_delayed_work(&hpd_work.dwork);
	atomic_set(&hpd_work.state, hpd ? HPD_STATE_START : HPD_STATE_OFF);
	queue_delayed_work(my_workq, &hpd_work.dwork,
					msecs_to_jiffies(hpd ? 40 : 30));
	return 0;
}

int hdmi_panel_set_mode(struct fb_videomode *vm, int code, int mode)
{
	return omapdss_hdmi_display_set_mode2(hdmi.dssdev, vm, code, mode);
}
static void hdmi_get_timings(struct omap_dss_device *dssdev,
			struct omap_video_timings *timings)
{
	mutex_lock(&hdmi.hdmi_lock);

	*timings = dssdev->panel.timings;

	mutex_unlock(&hdmi.hdmi_lock);
}

static void hdmi_set_timings(struct omap_dss_device *dssdev,
			struct omap_video_timings *timings)
{
	DSSDBG("hdmi_set_timings\n");

	mutex_lock(&hdmi.hdmi_lock);

	dssdev->panel.timings = *timings;
	omapdss_hdmi_display_set_timing(dssdev);

	mutex_unlock(&hdmi.hdmi_lock);
}

static int hdmi_check_timings(struct omap_dss_device *dssdev,
			struct omap_video_timings *timings)
{
	int r = 0;

	DSSDBG("hdmi_check_timings\n");

	mutex_lock(&hdmi.hdmi_lock);

	r = omapdss_hdmi_display_check_timing(dssdev, timings);

	mutex_unlock(&hdmi.hdmi_lock);
	return r;
}

static int hdmi_read_edid(struct omap_dss_device *dssdev, u8 *buf, int len)
{
	int r;

	mutex_lock(&hdmi.hdmi_lock);

	if (dssdev->state != OMAP_DSS_DISPLAY_ACTIVE) {
		r = omapdss_hdmi_display_enable(dssdev);
		if (r)
			goto err;
	}

	r = omapdss_hdmi_read_edid(buf, len);

	if (dssdev->state == OMAP_DSS_DISPLAY_DISABLED ||
			dssdev->state == OMAP_DSS_DISPLAY_SUSPENDED)
		omapdss_hdmi_display_disable(dssdev);
err:
	mutex_unlock(&hdmi.hdmi_lock);

	return r;
}

static bool hdmi_detect(struct omap_dss_device *dssdev)
{
	int r;

	mutex_lock(&hdmi.hdmi_lock);

	if (dssdev->state != OMAP_DSS_DISPLAY_ACTIVE) {
		r = omapdss_hdmi_display_enable(dssdev);
		if (r)
			goto err;
	}

	r = omapdss_hdmi_detect();

	if (dssdev->state == OMAP_DSS_DISPLAY_DISABLED ||
			dssdev->state == OMAP_DSS_DISPLAY_SUSPENDED)
		omapdss_hdmi_display_disable(dssdev);
err:
	mutex_unlock(&hdmi.hdmi_lock);

	return r;
}

#if defined(CONFIG_OMAP4_DSS_HDMI_AUDIO)
static int hdmi_panel_audio_enable(struct omap_dss_device *dssdev, bool enable)
{
	unsigned long flags;
	int r;

	spin_lock_irqsave(&hdmi.hdmi_sp_lock, flags);

	r = hdmi_audio_enable(enable);

	spin_unlock_irqrestore(&hdmi.hdmi_sp_lock, flags);
	return r;
}

static int hdmi_panel_audio_start(struct omap_dss_device *dssdev, bool start)
{
	unsigned long flags;
	int r;

	spin_lock_irqsave(&hdmi.hdmi_sp_lock, flags);

	r = hdmi_audio_start(start);

	spin_unlock_irqrestore(&hdmi.hdmi_sp_lock, flags);
	return r;
}

static bool hdmi_panel_audio_detect(struct omap_dss_device *dssdev)
{
	unsigned long flags;
	bool r = false;

	spin_lock_irqsave(&hdmi.hdmi_sp_lock, flags);

	if (dssdev->state != OMAP_DSS_DISPLAY_ACTIVE)
		goto err;

	if (!hdmi_mode_has_audio())
		goto err;

	r = true;
err:
	spin_unlock_irqrestore(&hdmi.hdmi_sp_lock, flags);
	return r;
}

static int hdmi_panel_audio_config(struct omap_dss_device *dssdev,
		struct snd_aes_iec958 *iec, struct snd_cea_861_aud_if *aud_if)
{
	unsigned long flags;
	int r;

	spin_lock_irqsave(&hdmi.hdmi_sp_lock, flags);

	r = hdmi_audio_config(iec, aud_if);

	spin_unlock_irqrestore(&hdmi.hdmi_sp_lock, flags);
	return r;
}

#endif

static int hdmi_get_modedb(struct omap_dss_device *dssdev,
			   struct fb_videomode *modedb, int modedb_len)
{
	struct fb_monspecs *specs = &dssdev->panel.monspecs;
	if (specs->modedb_len < modedb_len)
		modedb_len = specs->modedb_len;
	memcpy(modedb, specs->modedb, sizeof(*modedb) * modedb_len);
	return modedb_len;
}

static struct omap_dss_driver hdmi_driver = {
	.probe		= hdmi_panel_probe,
	.remove		= hdmi_panel_remove,
	.enable		= hdmi_panel_enable,
	.disable	= hdmi_panel_disable,
	.suspend	= hdmi_panel_suspend,
	.resume		= hdmi_panel_resume,
	.get_timings	= hdmi_get_timings,
	.set_timings	= hdmi_set_timings,
	.check_timings	= hdmi_check_timings,
	.read_edid	= hdmi_read_edid,
	.get_modedb	= hdmi_get_modedb,
	.set_mode       = omapdss_hdmi_display_set_mode,
	.detect		= hdmi_detect,
#if defined(CONFIG_OMAP4_DSS_HDMI_AUDIO)
	.audio_enable	= hdmi_panel_audio_enable,
	.audio_start	= hdmi_panel_audio_start,
	.audio_detect	= hdmi_panel_audio_detect,
	.audio_config	= hdmi_panel_audio_config,
#endif
	.s3d_enable	= hdmi_panel_3d_enable,
	.driver			= {
		.name   = "hdmi_panel",
		.owner  = THIS_MODULE,
	},
};

int hdmi_panel_init(void)
{
	int r = 0;
	mutex_init(&hdmi.hdmi_lock);

#if defined(CONFIG_OMAP4_DSS_HDMI_AUDIO)
	spin_lock_init(&hdmi.hdmi_sp_lock);
#endif
	hdmi.hpd_switch.name = "hdmi";
	r = switch_dev_register(&hdmi.hpd_switch);
	if (r)
		goto err_event;

	my_workq = create_singlethread_workqueue("hdmi_hotplug");
	if (!my_workq) {
		r = -EINVAL;
		goto err_work;
	}

	INIT_DELAYED_WORK(&hpd_work.dwork, hdmi_hotplug_detect_worker);
	omap_dss_register_driver(&hdmi_driver);

	return 0;

err_work:
	switch_dev_unregister(&hdmi.hpd_switch);
err_event:
	return r;
}

void hdmi_panel_exit(void)
{
	destroy_workqueue(my_workq);
	omap_dss_unregister_driver(&hdmi_driver);

	switch_dev_unregister(&hdmi.hpd_switch);
}
