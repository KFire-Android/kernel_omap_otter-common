/*
 * linux/drivers/video/omap2/dss/overlay.c
 *
 * Copyright (C) 2009 Nokia Corporation
 * Author: Tomi Valkeinen <tomi.valkeinen@nokia.com>
 *
 * Some code and ideas taken from drivers/video/omap/ driver
 * by Imre Deak.
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

#define DSS_SUBSYS_NAME "OVERLAY"

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/err.h>
#include <linux/sysfs.h>
#include <linux/kobject.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/slab.h>

#include <video/omapdss.h>
#include <plat/cpu.h>

#include "dss.h"
#include "dss_features.h"

static int num_overlays;
static struct omap_overlay *overlays;

static ssize_t overlay_name_show(struct omap_overlay *ovl, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n", ovl->name);
}

static ssize_t overlay_manager_show(struct omap_overlay *ovl, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n",
			ovl->manager ? ovl->manager->name : "<none>");
}

static ssize_t overlay_manager_store(struct omap_overlay *ovl, const char *buf,
		size_t size)
{
	int i, r;
	struct omap_overlay_manager *mgr = NULL;
	struct omap_overlay_manager *old_mgr;
	int len = size;

	if (buf[size-1] == '\n')
		--len;

	if (len > 0) {
		for (i = 0; i < omap_dss_get_num_overlay_managers(); ++i) {
			mgr = omap_dss_get_overlay_manager(i);

			if (sysfs_streq(buf, mgr->name))
				break;

			mgr = NULL;
		}
	}

	if (len > 0 && mgr == NULL)
		return -EINVAL;

	if (mgr)
		DSSDBG("manager %s found\n", mgr->name);

	if (mgr == ovl->manager)
		return size;

	old_mgr = ovl->manager;

	r = dispc_runtime_get();
	if (r)
		return r;

	/* detach old manager */
	if (old_mgr) {
		r = ovl->unset_manager(ovl);
		if (r) {
			DSSERR("detach failed\n");
			goto err;
		}

		r = old_mgr->apply(old_mgr);
		if (r)
			goto err;
	}

	if (mgr) {
		r = ovl->set_manager(ovl, mgr);
		if (r) {
			DSSERR("Failed to attach overlay\n");
			goto err;
		}

		r = mgr->apply(mgr);
		if (r)
			goto err;
	}

	dispc_runtime_put();

	return size;

err:
	dispc_runtime_put();
	return r;
}

static ssize_t overlay_input_size_show(struct omap_overlay *ovl, char *buf)
{
	struct omap_overlay_info info;

	ovl->get_overlay_info(ovl, &info);

	return snprintf(buf, PAGE_SIZE, "%d,%d\n",
			info.width, info.height);
}

static ssize_t overlay_screen_width_show(struct omap_overlay *ovl, char *buf)
{
	struct omap_overlay_info info;

	ovl->get_overlay_info(ovl, &info);

	return snprintf(buf, PAGE_SIZE, "%d\n", info.screen_width);
}

static ssize_t overlay_position_show(struct omap_overlay *ovl, char *buf)
{
	struct omap_overlay_info info;

	ovl->get_overlay_info(ovl, &info);

	return snprintf(buf, PAGE_SIZE, "%d,%d\n",
			info.pos_x, info.pos_y);
}

static ssize_t overlay_position_store(struct omap_overlay *ovl,
		const char *buf, size_t size)
{
	int r;
	char *last;
	struct omap_overlay_info info;

	ovl->get_overlay_info(ovl, &info);

	info.pos_x = simple_strtoul(buf, &last, 10);
	++last;
	if (last - buf >= size)
		return -EINVAL;

	info.pos_y = simple_strtoul(last, &last, 10);

	r = ovl->set_overlay_info(ovl, &info);
	if (r)
		return r;

	if (ovl->manager) {
		r = ovl->manager->apply(ovl->manager);
		if (r)
			return r;
	}

	return size;
}

static ssize_t overlay_output_size_show(struct omap_overlay *ovl, char *buf)
{
	struct omap_overlay_info info;

	ovl->get_overlay_info(ovl, &info);

	return snprintf(buf, PAGE_SIZE, "%d,%d\n",
			info.out_width, info.out_height);
}

static ssize_t overlay_output_size_store(struct omap_overlay *ovl,
		const char *buf, size_t size)
{
	int r;
	char *last;
	struct omap_overlay_info info;

	ovl->get_overlay_info(ovl, &info);

	info.out_width = simple_strtoul(buf, &last, 10);
	++last;
	if (last - buf >= size)
		return -EINVAL;

	info.out_height = simple_strtoul(last, &last, 10);

	r = ovl->set_overlay_info(ovl, &info);
	if (r)
		return r;

	if (ovl->manager) {
		r = ovl->manager->apply(ovl->manager);
		if (r)
			return r;
	}

	return size;
}

static ssize_t overlay_enabled_show(struct omap_overlay *ovl, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", ovl->is_enabled(ovl));
}

static ssize_t overlay_enabled_store(struct omap_overlay *ovl, const char *buf,
		size_t size)
{
	int r;
	bool enable;

	r = strtobool(buf, &enable);
	if (r)
		return r;

	if (enable)
		r = ovl->enable(ovl);
	else
		r = ovl->disable(ovl);

	if (r)
		return r;

	return size;
}

static ssize_t overlay_global_alpha_show(struct omap_overlay *ovl, char *buf)
{
	struct omap_overlay_info info;

	ovl->get_overlay_info(ovl, &info);

	return snprintf(buf, PAGE_SIZE, "%d\n",
			info.global_alpha);
}

static ssize_t overlay_global_alpha_store(struct omap_overlay *ovl,
		const char *buf, size_t size)
{
	int r;
	u8 alpha;
	struct omap_overlay_info info;

	if ((ovl->caps & OMAP_DSS_OVL_CAP_GLOBAL_ALPHA) == 0)
		return -ENODEV;

	r = kstrtou8(buf, 0, &alpha);
	if (r)
		return r;

	ovl->get_overlay_info(ovl, &info);

	info.global_alpha = alpha;

	r = ovl->set_overlay_info(ovl, &info);
	if (r)
		return r;

	if (ovl->manager) {
		r = ovl->manager->apply(ovl->manager);
		if (r)
			return r;
	}

	return size;
}

static ssize_t overlay_pre_mult_alpha_show(struct omap_overlay *ovl,
		char *buf)
{
	struct omap_overlay_info info;

	ovl->get_overlay_info(ovl, &info);

	return snprintf(buf, PAGE_SIZE, "%d\n",
			info.pre_mult_alpha);
}

static ssize_t overlay_pre_mult_alpha_store(struct omap_overlay *ovl,
		const char *buf, size_t size)
{
	int r;
	u8 alpha;
	struct omap_overlay_info info;

	if ((ovl->caps & OMAP_DSS_OVL_CAP_PRE_MULT_ALPHA) == 0)
		return -ENODEV;

	r = kstrtou8(buf, 0, &alpha);
	if (r)
		return r;

	ovl->get_overlay_info(ovl, &info);

	info.pre_mult_alpha = alpha;

	r = ovl->set_overlay_info(ovl, &info);
	if (r)
		return r;

	if (ovl->manager) {
		r = ovl->manager->apply(ovl->manager);
		if (r)
			return r;
	}

	return size;
}

static ssize_t overlay_zorder_show(struct omap_overlay *ovl, char *buf)
{
	struct omap_overlay_info info;

	ovl->get_overlay_info(ovl, &info);

	return snprintf(buf, PAGE_SIZE, "%d\n", info.zorder);
}

static ssize_t overlay_zorder_store(struct omap_overlay *ovl,
		const char *buf, size_t size)
{
	int r;
	u8 zorder;
	struct omap_overlay_info info;

	if ((ovl->caps & OMAP_DSS_OVL_CAP_ZORDER) == 0)
		return -ENODEV;

	r = kstrtou8(buf, 0, &zorder);
	if (r)
		return r;

	ovl->get_overlay_info(ovl, &info);

	info.zorder = zorder;

	r = ovl->set_overlay_info(ovl, &info);
	if (r)
		return r;

	if (ovl->manager) {
		r = ovl->manager->apply(ovl->manager);
		if (r)
			return r;
	}

	return size;
}

static ssize_t overlay_decim_show(u16 min, u16 max, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d..%d\n", min, max);
}

static ssize_t overlay_x_decim_show(struct omap_overlay *ovl, char *buf)
{
	struct omap_overlay_info info;

	ovl->get_overlay_info(ovl, &info);
	return overlay_decim_show(info.min_x_decim, info.max_x_decim, buf);
}

static ssize_t overlay_y_decim_show(struct omap_overlay *ovl, char *buf)
{
	struct omap_overlay_info info;

	ovl->get_overlay_info(ovl, &info);
	return overlay_decim_show(info.min_y_decim, info.max_y_decim, buf);
}

static ssize_t overlay_decim_store(u16 *min, u16 *max,
						const char *buf, size_t size)
{
	char *last;

	*min = *max = simple_strtoul(buf, &last, 10);
	if (last < buf + size && *last == '.') {
		/* check for .. separator */
		if (last + 2 >= buf + size || last[1] != '.')
			return -EINVAL;

		*max = simple_strtoul(last + 2, &last, 10);

		/* fix order */
		if (*max < *min)
			swap(*min, *max);
	}

	/* decimation must be positive */
	if (*min == 0)
		return -EINVAL;

	return 0;
}

static ssize_t overlay_x_decim_store(struct omap_overlay *ovl,
						const char *buf, size_t size)
{
	int r;
	struct omap_overlay_info info;

	ovl->get_overlay_info(ovl, &info);

	r = overlay_decim_store(&info.min_x_decim, &info.max_x_decim,
				buf, size);

	r = r ? : ovl->set_overlay_info(ovl, &info);

	if (!r && ovl->manager)
		r = ovl->manager->apply(ovl->manager);

	return r ? : size;
}

static ssize_t overlay_y_decim_store(struct omap_overlay *ovl,
						const char *buf, size_t size)
{
	int r;
	struct omap_overlay_info info;

	ovl->get_overlay_info(ovl, &info);

	r = overlay_decim_store(&info.min_y_decim, &info.max_y_decim,
				   buf, size);

	r = r ? : ovl->set_overlay_info(ovl, &info);

	if (!r && ovl->manager)
		r = ovl->manager->apply(ovl->manager);

	return r ? : size;
}

struct overlay_attribute {
	struct attribute attr;
	ssize_t (*show)(struct omap_overlay *, char *);
	ssize_t	(*store)(struct omap_overlay *, const char *, size_t);
};

#define OVERLAY_ATTR(_name, _mode, _show, _store) \
	struct overlay_attribute overlay_attr_##_name = \
	__ATTR(_name, _mode, _show, _store)

static OVERLAY_ATTR(name, S_IRUGO, overlay_name_show, NULL);
static OVERLAY_ATTR(manager, S_IRUGO|S_IWUSR,
		overlay_manager_show, overlay_manager_store);
static OVERLAY_ATTR(input_size, S_IRUGO, overlay_input_size_show, NULL);
static OVERLAY_ATTR(screen_width, S_IRUGO, overlay_screen_width_show, NULL);
static OVERLAY_ATTR(position, S_IRUGO|S_IWUSR,
		overlay_position_show, overlay_position_store);
static OVERLAY_ATTR(output_size, S_IRUGO|S_IWUSR,
		overlay_output_size_show, overlay_output_size_store);
static OVERLAY_ATTR(enabled, S_IRUGO|S_IWUSR,
		overlay_enabled_show, overlay_enabled_store);
static OVERLAY_ATTR(global_alpha, S_IRUGO|S_IWUSR,
		overlay_global_alpha_show, overlay_global_alpha_store);
static OVERLAY_ATTR(pre_mult_alpha, S_IRUGO|S_IWUSR,
		overlay_pre_mult_alpha_show,
		overlay_pre_mult_alpha_store);
static OVERLAY_ATTR(x_decim, S_IRUGO|S_IWUSR,
		overlay_x_decim_show, overlay_x_decim_store);
static OVERLAY_ATTR(y_decim, S_IRUGO|S_IWUSR,
		overlay_y_decim_show, overlay_y_decim_store);
static OVERLAY_ATTR(zorder, S_IRUGO|S_IWUSR,
		overlay_zorder_show, overlay_zorder_store);

static struct attribute *overlay_sysfs_attrs[] = {
	&overlay_attr_name.attr,
	&overlay_attr_manager.attr,
	&overlay_attr_input_size.attr,
	&overlay_attr_screen_width.attr,
	&overlay_attr_position.attr,
	&overlay_attr_output_size.attr,
	&overlay_attr_enabled.attr,
	&overlay_attr_global_alpha.attr,
	&overlay_attr_pre_mult_alpha.attr,
	&overlay_attr_zorder.attr,
	&overlay_attr_x_decim.attr,
	&overlay_attr_y_decim.attr,
	NULL
};

static ssize_t overlay_attr_show(struct kobject *kobj, struct attribute *attr,
		char *buf)
{
	struct omap_overlay *overlay;
	struct overlay_attribute *overlay_attr;

	overlay = container_of(kobj, struct omap_overlay, kobj);
	overlay_attr = container_of(attr, struct overlay_attribute, attr);

	if (!overlay_attr->show)
		return -ENOENT;

	return overlay_attr->show(overlay, buf);
}

static ssize_t overlay_attr_store(struct kobject *kobj, struct attribute *attr,
		const char *buf, size_t size)
{
	struct omap_overlay *overlay;
	struct overlay_attribute *overlay_attr;

	overlay = container_of(kobj, struct omap_overlay, kobj);
	overlay_attr = container_of(attr, struct overlay_attribute, attr);

	if (!overlay_attr->store)
		return -ENOENT;

	return overlay_attr->store(overlay, buf, size);
}

static int init_default_overlay_params(struct omap_overlay *ovl)
{
	struct omap_overlay_info info;
	const struct omap_dss_cconv_coefs ctbl_bt601_5 = {
		298, 409, 0, 298, -208, -100, 298, 0, 517, 0,
	};

	ovl->get_overlay_info(ovl, &info);

	/* default paddr should be a non-zero value */
	info.paddr = 1;
	info.color_mode = OMAP_DSS_COLOR_ARGB32;
	info.min_x_decim = 1;
	info.min_y_decim = 1;
	info.max_x_decim = (cpu_is_omap44xx() || cpu_is_omap54xx()) ? 16 : 1;
	info.max_y_decim = (cpu_is_omap44xx() || cpu_is_omap54xx()) ? 16 : 1;
	info.cconv = ctbl_bt601_5;
	return ovl->set_overlay_info(ovl, &info);
}

static const struct sysfs_ops overlay_sysfs_ops = {
	.show = overlay_attr_show,
	.store = overlay_attr_store,
};

static struct kobj_type overlay_ktype = {
	.sysfs_ops = &overlay_sysfs_ops,
	.default_attrs = overlay_sysfs_attrs,
};

int omap_dss_get_num_overlays(void)
{
	return num_overlays;
}
EXPORT_SYMBOL(omap_dss_get_num_overlays);

struct omap_overlay *omap_dss_get_overlay(int num)
{
	if (num >= num_overlays)
		return NULL;

	return &overlays[num];
}
EXPORT_SYMBOL(omap_dss_get_overlay);

void dss_init_overlays(struct platform_device *pdev)
{
	int i, r;

	num_overlays = dss_feat_get_num_ovls();

	overlays = kzalloc(sizeof(struct omap_overlay) * num_overlays,
			GFP_KERNEL);

	BUG_ON(overlays == NULL);

	for (i = 0; i < num_overlays; ++i) {
		struct omap_overlay *ovl = &overlays[i];

		switch (i) {
		case 0:
			ovl->name = "gfx";
			ovl->id = OMAP_DSS_GFX;
			break;
		case 1:
			ovl->name = "vid1";
			ovl->id = OMAP_DSS_VIDEO1;
			break;
		case 2:
			ovl->name = "vid2";
			ovl->id = OMAP_DSS_VIDEO2;
			break;
		case 3:
			ovl->name = "vid3";
			ovl->id = OMAP_DSS_VIDEO3;
			break;
		}

		ovl->is_enabled = &dss_ovl_is_enabled;
		ovl->enable = &dss_ovl_enable;
		ovl->disable = &dss_ovl_disable;
		ovl->set_manager = &dss_ovl_set_manager;
		ovl->unset_manager = &dss_ovl_unset_manager;
		ovl->set_overlay_info = &dss_ovl_set_info;
		ovl->get_overlay_info = &dss_ovl_get_info;
		ovl->wait_for_go = &dss_mgr_wait_for_go_ovl;

		ovl->caps = dss_feat_get_overlay_caps(ovl->id);
		ovl->supported_modes =
			dss_feat_get_supported_color_modes(ovl->id);

		r = kobject_init_and_add(&ovl->kobj, &overlay_ktype,
				&pdev->dev.kobj, "overlay%d", i);

		if (r)
			DSSERR("failed to create sysfs file\n");

		r = init_default_overlay_params(ovl);
		if (r)
			DSSERR("failed to set default overlay params\n");
	}
}

/* connect overlays to the new device, if not already connected. if force
 * selected, connect always. */
void dss_recheck_connections(struct omap_dss_device *dssdev, bool force)
{
	int i;
	struct omap_overlay_manager *lcd_mgr;
	struct omap_overlay_manager *tv_mgr;
	struct omap_overlay_manager *lcd2_mgr = NULL;
	struct omap_overlay_manager *mgr = NULL;

	lcd_mgr = omap_dss_get_overlay_manager(OMAP_DSS_OVL_MGR_LCD);
	tv_mgr = omap_dss_get_overlay_manager(OMAP_DSS_OVL_MGR_TV);
	if (dss_has_feature(FEAT_MGR_LCD2))
		lcd2_mgr = omap_dss_get_overlay_manager(OMAP_DSS_OVL_MGR_LCD2);

	if (dssdev->channel == OMAP_DSS_CHANNEL_LCD2) {
		if (!lcd2_mgr->device || force) {
			if (lcd2_mgr->device)
				lcd2_mgr->unset_device(lcd2_mgr);
			lcd2_mgr->set_device(lcd2_mgr, dssdev);
			mgr = lcd2_mgr;
		}
	} else if (dssdev->type != OMAP_DISPLAY_TYPE_VENC
			&& dssdev->type != OMAP_DISPLAY_TYPE_HDMI) {
		if (!lcd_mgr->device || force) {
			if (lcd_mgr->device)
				lcd_mgr->unset_device(lcd_mgr);
			lcd_mgr->set_device(lcd_mgr, dssdev);
			mgr = lcd_mgr;
		}
	}

	if (dssdev->type == OMAP_DISPLAY_TYPE_VENC
			|| dssdev->type == OMAP_DISPLAY_TYPE_HDMI) {
		if (!tv_mgr->device || force) {
			if (tv_mgr->device)
				tv_mgr->unset_device(tv_mgr);
			tv_mgr->set_device(tv_mgr, dssdev);
			mgr = tv_mgr;
		}
	}

	if (mgr && force) {
		dispc_runtime_get();

		for (i = 0; i < dss_feat_get_num_ovls(); i++) {
			struct omap_overlay *ovl;
			ovl = omap_dss_get_overlay(i);
			if (ovl->manager)
				ovl->unset_manager(ovl);
			ovl->set_manager(ovl, mgr);
		}

		dispc_runtime_put();
	}
}

void dss_uninit_overlays(struct platform_device *pdev)
{
	int i;

	for (i = 0; i < num_overlays; ++i) {
		struct omap_overlay *ovl = &overlays[i];

		kobject_del(&ovl->kobj);
		kobject_put(&ovl->kobj);
	}

	kfree(overlays);
	overlays = NULL;
	num_overlays = 0;
}

int dss_ovl_simple_check(struct omap_overlay *ovl,
		const struct omap_overlay_info *info)
{
	if (info->paddr == 0) {
		DSSERR("check_overlay: paddr cannot be 0\n");
		return -EINVAL;
	}

	if ((ovl->caps & OMAP_DSS_OVL_CAP_SCALE) == 0) {
		if (info->out_width != 0 && info->width != info->out_width) {
			DSSERR("check_overlay: overlay %d doesn't support "
					"scaling\n", ovl->id);
			return -EINVAL;
		}

		if (info->out_height != 0 && info->height != info->out_height) {
			DSSERR("check_overlay: overlay %d doesn't support "
					"scaling\n", ovl->id);
			return -EINVAL;
		}
	}

	if ((ovl->supported_modes & info->color_mode) == 0) {
		DSSERR("check_overlay: overlay %d doesn't support mode %d\n",
				ovl->id, info->color_mode);
		return -EINVAL;
	}

	if (info->zorder >= omap_dss_get_num_overlays()) {
		DSSERR("check_overlay: zorder %d too high\n", info->zorder);
		return -EINVAL;
	}

	return 0;
}

int dss_ovl_check(struct omap_overlay *ovl,
		struct omap_overlay_info *info, struct omap_dss_device *dssdev)
{
	u16 outw, outh;
	u16 dw, dh;
	struct omap_writeback_info wb_info;
	struct omap_writeback *wb;


	if (dssdev == NULL)
		return 0;

	wb = omap_dss_get_wb(0);
	if (wb) {
		wb->get_wb_info(wb, &wb_info);

		if (wb_info.mode == OMAP_WB_MEM2MEM_MODE &&
			(int)ovl->manager->id == (int)wb_info.source &&
			dssdev->state != OMAP_DSS_DISPLAY_ACTIVE) {
			dw = wb_info.width;
			dh = wb_info.height;
		} else
			dssdev->driver->get_resolution(dssdev, &dw, &dh);
	} else
		dssdev->driver->get_resolution(dssdev, &dw, &dh);


	if ((ovl->caps & OMAP_DSS_OVL_CAP_SCALE) == 0) {
		outw = info->width;
		outh = info->height;
	} else {
		if (info->out_width == 0)
			outw = info->width;
		else
			outw = info->out_width;

		if (info->out_height == 0)
			outh = info->height;
		else
			outh = info->out_height;
	}

	if (!info->wb_source) {
		if (dw < info->pos_x + outw) {
			DSSERR("%s: overlay %d horizontally not inside the "
					"display area (%d + %d >= %d)\n",
				__func__, ovl->id, info->pos_x, outw, dw);
			return -EINVAL;
		}

		if (dh < info->pos_y + outh) {
			DSSERR("%s: overlay %d vertically not inside the "
					"display area (%d + %d >= %d)\n",
				__func__, ovl->id, info->pos_y, outh, dh);
			return -EINVAL;
		}
	}

	if (dssdev->type == OMAP_DISPLAY_TYPE_DSI) {
		/*
		 * OMAP44xx/ OMAP5 ES1.0/2.0 Errata i339: DSI: Minimum of 2
		 * pixels should be transferred through DISPC Video Port. Min
		 * number of pixels from the video port should be at least 2
		 * (greater than 1). Image with less than 2 pixels is
		 * not expected to be used in a real applicative use case.
		 *
		 * The WA proposed is to use the OCP L4 slave port through CPU
		 * or sDMA while sending one pixel. But we avoid supporting
		 * less than 2 pixels transfers itself. The reason is that this
		 * is non-realistic case but the implementation to support this
		 * is slightly complex.
		 */
		if (((cpu_is_omap54xx() && (omap_rev() <= OMAP5430_REV_ES2_0
			|| omap_rev() == OMAP5432_REV_ES1_0))
			|| cpu_is_omap44xx()) &&
			(info->width * info->height < 2)) {
			DSSWARN("check_overlay: too small frame %d*%d\n",
				info->width, info->height);
			return -EINVAL;
		}
	}

	return 0;
}
