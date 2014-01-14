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
#include <video/omapdss.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/of.h>
#ifdef CONFIG_SWITCH_STATE
#include <linux/switch.h>
#endif
#include <linux/delay.h>

#include "dss.h"
#include "hdmi.h"

enum omap_hdmi_users {
	HDMI_USER_VIDEO	= 1 << 0,
	HDMI_USER_AUDIO	= 1 << 1,
};
static struct {
	/* This protects the panel ops, mainly when accessing the HDMI IP. */
	struct mutex lock;
	struct omap_dss_device *dssdev;
#ifdef CONFIG_SWITCH_STATE
	struct switch_dev hpd_switch;
#endif
	int hpd_gpio;
	u8 usage;
#if defined(CONFIG_OMAP4_DSS_HDMI_AUDIO) || \
	defined(CONFIG_OMAP5_DSS_HDMI_AUDIO)
	/* This protects the audio ops, specifically. */
	spinlock_t audio_lock;
#endif
} hdmi;

int hdmi_get_current_hpd(void)
{
#ifndef CONFIG_USE_FB_MODE_DB
	return gpio_get_value(hdmi.hpd_gpio);
#else
	bool force_timings = omapdss_hdmi_get_force_timings();
	pr_info("%s ==> force_timings = %d\n", __func__, force_timings);
	return force_timings ? force_timings : gpio_get_value(hdmi.hpd_gpio);
#endif
}

static irqreturn_t hpd_enable_handler(int irq, void *ptr)
{
	int hpd = hdmi_get_current_hpd();
	pr_info("hpd %d\n", hpd);

#ifdef CONFIG_USE_FB_MODE_DB
	omapdss_hdmi_reset_force_timings();
#endif

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

static ssize_t hdmi_edid_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	ssize_t size = omapdss_get_edid(buf);
	if (size)
		print_hex_dump(KERN_ERR, " \t", DUMP_PREFIX_NONE, 16, 1,
			       buf, size, false);
	return size;
}

static DEVICE_ATTR(edid, S_IRUGO | S_IWUSR, hdmi_edid_show, NULL);

static int hdmi_panel_probe(struct omap_dss_device *dssdev)
{
	int r;
	struct omap_dss_hdmi_data *priv = dssdev->data;
	/* Initialize default timings to VGA in DVI mode */
	const struct omap_video_timings default_timings = {
		.x_res		= 640,
		.y_res		= 480,
		.pixel_clock	= 25175,
		.hsw		= 96,
		.hfp		= 16,
		.hbp		= 48,
		.vsw		= 2,
		.vfp		= 11,
		.vbp		= 31,

		.vsync_level	= OMAPDSS_SIG_ACTIVE_LOW,
		.hsync_level	= OMAPDSS_SIG_ACTIVE_LOW,

		.interlace	= false,
	};

	DSSDBG("ENTER hdmi_panel_probe\n");

	dssdev->panel.timings = default_timings;

	/* sysfs entry to provide user space control to set
	 * quantization range
	 */
	if (device_create_file(&dssdev->dev, &dev_attr_range))
		DSSERR("failed to create sysfs file\n");

	/* sysfs entry to provide user space control to set deepcolor mode,
	 * hdmi_timings and edid.
	*/
	if (device_create_file(&dssdev->dev, &dev_attr_deepcolor) ||
#ifdef CONFIG_USE_FB_MODE_DB
	    device_create_file(&dssdev->dev, &dev_attr_hdmi_timings) ||
#endif
	    device_create_file(&dssdev->dev, &dev_attr_edid))
		DSSERR("failed to create sysfs file\n");

	DSSINFO("hdmi_panel_probe x_res= %d y_res = %d\n",
		dssdev->panel.timings.x_res,
		dssdev->panel.timings.y_res);
#ifndef CONFIG_USE_FB_MODE_DB
	omapdss_hdmi_display_set_timing(dssdev, &dssdev->panel.timings);
#endif
	hdmi.dssdev = dssdev;
	hdmi.hpd_gpio = priv->hpd_gpio;

	r = request_threaded_irq(gpio_to_irq(hdmi.hpd_gpio),
		NULL, hpd_enable_handler,
		IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING
		| IRQF_ONESHOT, "hpd", NULL);
	if (r < 0) {
		pr_err("hdmi_panel: request_irq %d failed\n",
			gpio_to_irq(hdmi.hpd_gpio));
		r = -EINVAL;
		goto err_irq;
	}

	return 0;

err_irq:
	gpio_free(hdmi.hpd_gpio);
	return r;
}

static void hdmi_panel_remove(struct omap_dss_device *dssdev)
{
	device_remove_file(&dssdev->dev, &dev_attr_deepcolor);
	device_remove_file(&dssdev->dev, &dev_attr_range);
#ifdef CONFIG_USE_FB_MODE_DB
	device_remove_file(&dssdev->dev, &dev_attr_hdmi_timings);
#endif
	device_remove_file(&dssdev->dev, &dev_attr_edid);
}

#if defined(CONFIG_OMAP4_DSS_HDMI_AUDIO) || \
	defined(CONFIG_OMAP5_DSS_HDMI_AUDIO)
static int hdmi_panel_audio_enable(struct omap_dss_device *dssdev)
{
	unsigned long flags;
	int r;

	mutex_lock(&hdmi.lock);
	spin_lock_irqsave(&hdmi.audio_lock, flags);

	/* enable audio only if the display is active and supports audio */
	if (dssdev->state != OMAP_DSS_DISPLAY_ACTIVE ||
	    !hdmi_mode_has_audio()) {
		DSSERR("audio not supported or display is off\n");
		r = -EPERM;
		goto err;
	}

	r = hdmi_audio_enable();

	if (!r)
		dssdev->audio_state = OMAP_DSS_AUDIO_ENABLED;

err:
	spin_unlock_irqrestore(&hdmi.audio_lock, flags);
	mutex_unlock(&hdmi.lock);
	return r;
}

static void hdmi_panel_audio_disable(struct omap_dss_device *dssdev)
{
	unsigned long flags;

	spin_lock_irqsave(&hdmi.audio_lock, flags);

	if (dssdev->state != OMAP_DSS_DISPLAY_DISABLED)
		hdmi_audio_disable();

	dssdev->audio_state = OMAP_DSS_AUDIO_DISABLED;

	spin_unlock_irqrestore(&hdmi.audio_lock, flags);
}

static int hdmi_panel_audio_start(struct omap_dss_device *dssdev)
{
	unsigned long flags;
	int r;

	spin_lock_irqsave(&hdmi.audio_lock, flags);
	/*
	 * No need to check the panel state. It was checked when trasitioning
	 * to AUDIO_ENABLED.
	 */
	if (dssdev->audio_state != OMAP_DSS_AUDIO_ENABLED) {
		DSSERR("audio start from invalid state\n");
		r = -EPERM;
		goto err;
	}

	r = hdmi_audio_start();

	if (!r)
		dssdev->audio_state = OMAP_DSS_AUDIO_PLAYING;

err:
	spin_unlock_irqrestore(&hdmi.audio_lock, flags);
	return r;
}

static void hdmi_panel_audio_stop(struct omap_dss_device *dssdev)
{
	unsigned long flags;

	spin_lock_irqsave(&hdmi.audio_lock, flags);

	hdmi_audio_stop();
	dssdev->audio_state = OMAP_DSS_AUDIO_ENABLED;

	spin_unlock_irqrestore(&hdmi.audio_lock, flags);
}

static bool hdmi_panel_audio_supported(struct omap_dss_device *dssdev)
{
	bool r = false;

	mutex_lock(&hdmi.lock);

	if (dssdev->state != OMAP_DSS_DISPLAY_ACTIVE)
		goto err;

	if (!hdmi_mode_has_audio())
		goto err;

	r = true;
err:
	mutex_unlock(&hdmi.lock);
	return r;
}

static int hdmi_panel_audio_config(struct omap_dss_device *dssdev,
		struct omap_dss_audio *audio)
{
	unsigned long flags;
	int r;

	mutex_lock(&hdmi.lock);
	spin_lock_irqsave(&hdmi.audio_lock, flags);

	/* config audio only if the display is active and supports audio */
	if (dssdev->state != OMAP_DSS_DISPLAY_ACTIVE ||
	    !hdmi_mode_has_audio()) {
		DSSERR("audio not supported or display is off\n");
		r = -EPERM;
		goto err;
	}

	r = hdmi_audio_config(audio);

	if (!r)
		dssdev->audio_state = OMAP_DSS_AUDIO_CONFIGURED;

err:
	spin_unlock_irqrestore(&hdmi.audio_lock, flags);
	mutex_unlock(&hdmi.lock);
	return r;
}

#else
static int hdmi_panel_audio_enable(struct omap_dss_device *dssdev)
{
	return -EPERM;
}

static void hdmi_panel_audio_disable(struct omap_dss_device *dssdev)
{
}

static int hdmi_panel_audio_start(struct omap_dss_device *dssdev)
{
	return -EPERM;
}

static void hdmi_panel_audio_stop(struct omap_dss_device *dssdev)
{
}

static bool hdmi_panel_audio_supported(struct omap_dss_device *dssdev)
{
	return false;
}

static int hdmi_panel_audio_config(struct omap_dss_device *dssdev,
		struct omap_dss_audio *audio)
{
	return -EPERM;
}
#endif

static int hdmi_panel_enable(struct omap_dss_device *dssdev)
{
	int r = 0;
	DSSDBG("ENTER hdmi_panel_enable\n");

	mutex_lock(&hdmi.lock);

	hdmi.usage |= HDMI_USER_VIDEO;
	if (dssdev->state != OMAP_DSS_DISPLAY_DISABLED) {
		r = -EINVAL;
		goto err;
	}
#ifndef CONFIG_USE_FB_MODE_DB
	omapdss_hdmi_display_set_timing(dssdev, &dssdev->panel.timings);
#endif

	/* Turn on HDMI only if HPD is high */
	if (!hdmi_get_current_hpd()) {
		r = 0;
		goto err;
	}

	r = omapdss_hdmi_display_enable(dssdev);
	if (r) {
		DSSERR("failed to power on\n");
		goto err;
	}

	dssdev->state = OMAP_DSS_DISPLAY_ACTIVE;

err:
	mutex_unlock(&hdmi.lock);

	return r;
}

static int hdmi_panel_3d_enable(struct omap_dss_device *dssdev,
				struct s3d_disp_info *info, int code)
{
	int r = 0;
	DSSDBG("ENTER hdmi_panel_3d_enable\n");

	mutex_lock(&hdmi.lock);

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
	mutex_unlock(&hdmi.lock);

	return r;
}

static void hdmi_panel_disable(struct omap_dss_device *dssdev)
{
	mutex_lock(&hdmi.lock);

	hdmi.usage &= ~HDMI_USER_VIDEO;
	if (hdmi.usage) {
		DSSINFO("HDMI in use\n");
		goto done;
	}

	if (dssdev->state == OMAP_DSS_DISPLAY_ACTIVE) {
#ifdef CONFIG_USE_FB_MODE_DB
		dssdev->output->manager->blank(dssdev->output->manager, true);
#endif
		msleep(100);
		omapdss_hdmi_display_disable(dssdev);
	}

	dssdev->state = OMAP_DSS_DISPLAY_DISABLED;

done:
	mutex_unlock(&hdmi.lock);
}

enum {
	HPD_STATE_OFF,
	HPD_STATE_START,
	HPD_STATE_EDID_TRYLAST = HPD_STATE_START + 5,
	HPD_STATE_EDID_READ_OK = HPD_STATE_EDID_TRYLAST + 1,
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

	DSSINFO("in hpd work %d, state=%d\n", state, dssdev->state);
	mutex_lock(&hdmi.lock);

	/* Make sure it is not a debounce */
	if (!hdmi_get_current_hpd() && state == HPD_STATE_START)
		state = HPD_STATE_OFF;

	if (state == HPD_STATE_OFF) {
#ifdef CONFIG_SWITCH_STATE
		switch_set_state(&hdmi.hpd_switch, 0);
#endif
		DSSINFO("%s state = %d\n", __func__, state);
		if (dssdev->state == OMAP_DSS_DISPLAY_ACTIVE) {
			hdmi_notify_hpd(dssdev, false);
			mutex_unlock(&hdmi.lock);
			dssdev->driver->disable(dssdev);
			/* clear EDID and mode */
			omapdss_hdmi_clear_edid();
			mutex_lock(&hdmi.lock);
		}
		hdmi_set_ls_state(LS_HPD_ON);
		goto done;
	} else {
		if (state == HPD_STATE_EDID_TRYLAST) {
			DSSINFO("Failed to read EDID after %d times."
				"Giving up.", state - HPD_STATE_START);
			hdmi_set_ls_state(LS_HPD_ON);
			goto done;
		} else if (state == HPD_STATE_START ||
				state != HPD_STATE_EDID_READ_OK) {
			hdmi_set_ls_state(LS_ENABLED);
			/* Read EDID before we turn on the HDMI */
			DSSINFO("%s state = %d\n", __func__, state);
			if (hdmi_read_valid_edid()
#ifdef CONFIG_USE_FB_MODE_DB
			    || omapdss_hdmi_get_force_timings()
#endif
			    ) {
#ifdef CONFIG_USE_FB_MODE_DB
				/* get monspecs from edid */
				hdmi_get_monspecs(dssdev);
				DSSINFO("panel size %d by %d\n",
						dssdev->panel.monspecs.max_x,
						dssdev->panel.monspecs.max_y);
				dssdev->panel.width_in_um =
					dssdev->panel.monspecs.max_x * 10000;
				dssdev->panel.height_in_um =
					dssdev->panel.monspecs.max_y * 10000;
#endif
				atomic_set(&d->state,
						HPD_STATE_EDID_READ_OK - 1);
			}
		} else if (state == HPD_STATE_EDID_READ_OK) {
			/* If device is in disabled state turn on the HDMI */
			if (dssdev->state == OMAP_DSS_DISPLAY_DISABLED) {
				/* Power on the HDMI if edid is read
				     successfully*/
				mutex_unlock(&hdmi.lock);
				dssdev->driver->enable(dssdev);
				mutex_lock(&hdmi.lock);
			}
			/* We have active hdmi so communicate attach*/
			hdmi_notify_hpd(dssdev, true);
#ifdef CONFIG_SWITCH_STATE
			switch_set_state(&hdmi.hpd_switch, 1);
#endif
			DSSINFO("%s state = %d\n", __func__, state);
			goto done;
		}
		if (atomic_add_unless(&d->state, 1, HPD_STATE_OFF)) {
			queue_delayed_work(my_workq, &d->dwork,
							msecs_to_jiffies(60));
		}
	}
done:
	mutex_unlock(&hdmi.lock);
}

int hdmi_panel_hpd_handler(int hpd)
{
	cancel_delayed_work(&hpd_work.dwork);
	atomic_set(&hpd_work.state, hpd ? HPD_STATE_START : HPD_STATE_OFF);
	queue_delayed_work(my_workq, &hpd_work.dwork,
					msecs_to_jiffies(hpd ? 500 : 30));
	return 0;
}

int hdmi_panel_set_mode(struct fb_videomode *vm, int code, int mode)
{
	return omapdss_hdmi_display_set_mode2(hdmi.dssdev, vm, code, mode);
}
static void hdmi_get_timings(struct omap_dss_device *dssdev,
			struct omap_video_timings *timings)
{
	mutex_lock(&hdmi.lock);

	*timings = dssdev->panel.timings;

	mutex_unlock(&hdmi.lock);
}

static void hdmi_set_timings(struct omap_dss_device *dssdev,
			struct omap_video_timings *timings)
{
	DSSDBG("hdmi_set_timings\n");

	mutex_lock(&hdmi.lock);

	/*
	 * TODO: notify audio users that there was a timings change. For
	 * now, disable audio locally to not break our audio state machine.
	 */
	hdmi_panel_audio_disable(dssdev);

	dssdev->panel.timings = *timings;
#ifndef CONFIG_USE_FB_MODE_DB
	omapdss_hdmi_display_set_timing(dssdev, timings);
	mutex_unlock(&hdmi.lock);
#else
	mutex_unlock(&hdmi.lock);
	omapdss_hdmi_display_set_timing(dssdev, timings);
#endif
}

static int hdmi_check_timings(struct omap_dss_device *dssdev,
			struct omap_video_timings *timings)
{
	int r = 0;

	DSSDBG("hdmi_check_timings\n");

	mutex_lock(&hdmi.lock);

	r = omapdss_hdmi_display_check_timing(dssdev, timings);

	mutex_unlock(&hdmi.lock);
	return r;
}

static int hdmi_read_edid(struct omap_dss_device *dssdev, u8 *buf, int len)
{
	int r;
	bool need_enable;

	mutex_lock(&hdmi.lock);

	need_enable = dssdev->state == OMAP_DSS_DISPLAY_DISABLED;

	if (need_enable) {
		r = omapdss_hdmi_core_enable(dssdev);
		if (r)
			goto err;
	}

	r = omapdss_hdmi_read_edid(buf, len);

	if (need_enable)
		omapdss_hdmi_core_disable(dssdev);
err:
	mutex_unlock(&hdmi.lock);

	return r;
}

static bool hdmi_detect(struct omap_dss_device *dssdev)
{
	int r;
	bool need_enable;

	mutex_lock(&hdmi.lock);

	need_enable = dssdev->state == OMAP_DSS_DISPLAY_DISABLED;

	if (need_enable) {
		r = omapdss_hdmi_core_enable(dssdev);
		if (r)
			goto err;
	}

	r = omapdss_hdmi_detect();

	if (need_enable)
		omapdss_hdmi_core_disable(dssdev);
err:
	mutex_unlock(&hdmi.lock);

	return r;
}

#if defined(CONFIG_OF)
static const struct of_device_id hdmi_panel_of_match[] = {
	{
		.compatible = "ti,hdmi_panel",
	},
	{},
};

MODULE_DEVICE_TABLE(of, hdmi_panel_of_match);
#else
#define dss_of_match NULL
#endif

#ifdef CONFIG_USE_FB_MODE_DB
static int hdmi_get_modedb(struct omap_dss_device *dssdev,
			   struct fb_videomode *modedb, int modedb_len)
{
	struct fb_monspecs *specs = &dssdev->panel.monspecs;
	if (specs->modedb_len < modedb_len)
		modedb_len = specs->modedb_len;
	memcpy(modedb, specs->modedb, sizeof(*modedb) * modedb_len);
	return modedb_len;
}
#endif

static struct omap_dss_driver hdmi_driver = {
	.probe		= hdmi_panel_probe,
	.remove		= hdmi_panel_remove,
	.enable		= hdmi_panel_enable,
	.disable	= hdmi_panel_disable,
	.get_timings	= hdmi_get_timings,
	.set_timings	= hdmi_set_timings,
	.check_timings	= hdmi_check_timings,
	.read_edid	= hdmi_read_edid,
#ifdef CONFIG_USE_FB_MODE_DB
	.get_modedb	= hdmi_get_modedb,
	.set_mode       = omapdss_hdmi_display_set_mode,
#endif
	.detect		= hdmi_detect,
	.audio_enable	= hdmi_panel_audio_enable,
	.audio_disable	= hdmi_panel_audio_disable,
	.audio_start	= hdmi_panel_audio_start,
	.audio_stop	= hdmi_panel_audio_stop,
	.audio_supported	= hdmi_panel_audio_supported,
	.audio_config	= hdmi_panel_audio_config,
	.s3d_enable	= hdmi_panel_3d_enable,
	.driver			= {
		.name   = "hdmi_panel",
		.owner  = THIS_MODULE,
		.of_match_table = hdmi_panel_of_match,
	},
};

int hdmi_panel_init(void)
{
	int r;
	mutex_init(&hdmi.lock);

#if defined(CONFIG_OMAP4_DSS_HDMI_AUDIO) || \
	defined (CONFIG_OMAP5_DSS_HDMI_AUDIO)
	spin_lock_init(&hdmi.audio_lock);
#endif

#ifdef CONFIG_SWITCH_STATE
	hdmi.hpd_switch.name = "hdmi";
	hdmi.usage = 0;
	r = switch_dev_register(&hdmi.hpd_switch);
	if (r)
		goto err_event;
	/* Init switch state to zero */
	switch_set_state(&hdmi.hpd_switch, 0);
#endif

	my_workq = create_singlethread_workqueue("hdmi_hotplug");
	if (!my_workq) {
		r = -EINVAL;
		goto err_work;
	}

	INIT_DELAYED_WORK(&hpd_work.dwork, hdmi_hotplug_detect_worker);
	omap_dss_register_driver(&hdmi_driver);

	return 0;

err_work:
#ifdef CONFIG_SWITCH_STATE
	switch_dev_unregister(&hdmi.hpd_switch);
err_event:
#endif
	return r;
}

void hdmi_panel_exit(void)
{
	destroy_workqueue(my_workq);
	omap_dss_unregister_driver(&hdmi_driver);

#ifdef CONFIG_SWITCH_STATE
	switch_dev_unregister(&hdmi.hpd_switch);
#endif
}
