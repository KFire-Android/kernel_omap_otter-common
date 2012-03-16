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

#include "dss.h"

static struct {
	struct mutex hdmi_lock;
#if defined(CONFIG_OMAP4_DSS_HDMI_AUDIO)
	/* protects calls to HDMI driver audio functionality */
	spinlock_t hdmi_sp_lock;
#endif
} hdmi;


static int hdmi_panel_probe(struct omap_dss_device *dssdev)
{
	DSSDBG("ENTER hdmi_panel_probe\n");

	dssdev->panel.config = OMAP_DSS_LCD_TFT |
			OMAP_DSS_LCD_IVS | OMAP_DSS_LCD_IHS;

	dssdev->panel.timings = (struct omap_video_timings){640, 480, 25175, 96, 16, 48, 2 , 11, 31};

	DSSDBG("hdmi_panel_probe x_res= %d y_res = %d\n",
		dssdev->panel.timings.x_res,
		dssdev->panel.timings.y_res);
	return 0;
}

static void hdmi_panel_remove(struct omap_dss_device *dssdev)
{

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

err:
	mutex_unlock(&hdmi.hdmi_lock);

	return r;
}

static void hdmi_panel_disable(struct omap_dss_device *dssdev)
{
	mutex_lock(&hdmi.hdmi_lock);

	if (dssdev->state == OMAP_DSS_DISPLAY_ACTIVE)
		omapdss_hdmi_display_disable(dssdev);

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

	r = omapdss_hdmi_display_enable(dssdev);
	if (r) {
		DSSERR("failed to power on\n");
		goto err;
	}

	dssdev->state = OMAP_DSS_DISPLAY_ACTIVE;

err:
	mutex_unlock(&hdmi.hdmi_lock);

	return r;
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
	.detect		= hdmi_detect,
#if defined(CONFIG_OMAP4_DSS_HDMI_AUDIO)
	.audio_enable	= hdmi_panel_audio_enable,
	.audio_start	= hdmi_panel_audio_start,
	.audio_detect	= hdmi_panel_audio_detect,
	.audio_config	= hdmi_panel_audio_config,
#endif
	.driver			= {
		.name   = "hdmi_panel",
		.owner  = THIS_MODULE,
	},
};

int hdmi_panel_init(void)
{
	mutex_init(&hdmi.hdmi_lock);

#if defined(CONFIG_OMAP4_DSS_HDMI_AUDIO)
	spin_lock_init(&hdmi.hdmi_sp_lock);
#endif

	omap_dss_register_driver(&hdmi_driver);

	return 0;
}

void hdmi_panel_exit(void)
{
	omap_dss_unregister_driver(&hdmi_driver);

}
