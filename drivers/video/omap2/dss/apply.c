/*
 * Copyright (C) 2011 Texas Instruments
 * Author: Tomi Valkeinen <tomi.valkeinen@ti.com>
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

#define DSS_SUBSYS_NAME "APPLY"

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/jiffies.h>
#include <linux/seq_file.h>

#include <video/omapdss.h>

#include "dss.h"
#include "dss_features.h"
#include "dispc-compat.h"

struct callback_states {
	/*
	 * Keep track of callbacks at the last 3 levels of pipeline:
	 * info, shadow registers and in DISPC registers.
	 *
	 * Note: We zero the function pointer when moving from one level to
	 * another to avoid checking for dirty and shadow_dirty fields that
	 * are not common between overlay and manager cache structures.
	 */
	struct omapdss_ovl_cb info, shadow, dispc;
	bool dispc_displayed;
	bool shadow_enabled;
};

/*
 * We have 4 levels of cache for the dispc settings. First two are in SW and
 * the latter two in HW.
 *
 *       set_info()
 *          v
 * +--------------------+
 * |     user_info      |
 * +--------------------+
 *          v
 *        apply()
 *          v
 * +--------------------+
 * |       info         |
 * +--------------------+
 *          v
 *      write_regs()
 *          v
 * +--------------------+
 * |  shadow registers  |
 * +--------------------+
 *          v
 * VFP or lcd/digit_enable
 *          v
 * +--------------------+
 * |      registers     |
 * +--------------------+
 */

struct ovl_priv_data {

	bool user_info_dirty;
	struct omap_overlay_info user_info;

	bool info_dirty;
	struct omap_overlay_info info;

	/* callback data for the last 3 states */
	struct callback_states cb;

	/* overlay's channel in DISPC */
	int dispc_channel;

	bool shadow_info_dirty;

	bool extra_info_dirty;
	bool shadow_extra_info_dirty;

	bool enabled;
	enum omap_channel channel;
	u32 fifo_low, fifo_high;

	/*
	 * True if overlay is to be enabled. Used to check and calculate configs
	 * for the overlay before it is enabled in the HW.
	 */
	bool enabling;
};

struct mgr_priv_data {

	bool user_info_dirty;
	struct omap_overlay_manager_info user_info;

	bool info_dirty;
	struct omap_overlay_manager_info info;

	bool shadow_info_dirty;

	/* If true, GO bit is up and shadow registers cannot be written.
	 * Never true for manual update displays */
	bool busy;

	/* If true, dispc output is enabled */
	bool updating;

	/* If true, a display is enabled using this manager */
	bool enabled;

	bool extra_info_dirty;
	bool shadow_extra_info_dirty;

	struct omap_video_timings timings;
	struct dss_lcd_mgr_config lcd_config;

	void (*framedone_handler)(void *);
	void *framedone_handler_data;

	/* callback data for the last 3 states */
	struct callback_states cb;
};

static struct {
	struct ovl_priv_data ovl_priv_data_array[MAX_DSS_OVERLAYS];
	struct mgr_priv_data mgr_priv_data_array[MAX_DSS_MANAGERS];
	struct writeback_cache_data writeback_cache;

	bool irq_enabled;
	u32 comp_irq_enabled;
} dss_data;

/* propagating callback info between states */
static inline void
dss_ovl_configure_cb(struct callback_states *st, int i, bool enabled)
{
	/* complete info in shadow */
	dss_ovl_cb(&st->shadow, i, DSS_COMPLETION_ECLIPSED_SHADOW);

	/* propagate info to shadow */
	st->shadow = st->info;
	st->shadow_enabled = enabled;
	/* info traveled to shadow */
	st->info.fn = NULL;
}

static inline void
dss_ovl_program_cb(struct callback_states *st, int i)
{
	/* mark previous programming as completed */
	dss_ovl_cb(&st->dispc, i, st->dispc_displayed ?
				DSS_COMPLETION_RELEASED : DSS_COMPLETION_TORN);

	/* mark shadow info as programmed, not yet displayed */
	dss_ovl_cb(&st->shadow, i, DSS_COMPLETION_PROGRAMMED);

	/* if overlay/manager is not enabled, we are done now */
	if (!st->shadow_enabled) {
		dss_ovl_cb(&st->shadow, i, DSS_COMPLETION_RELEASED);
		st->shadow.fn = NULL;
	}

	/* propagate shadow to dispc */
	st->dispc = st->shadow;
	st->shadow.fn = NULL;
	st->dispc_displayed = false;
}

/* protects dss_data */
static spinlock_t data_lock;
/* lock for blocking functions */
static DEFINE_MUTEX(apply_lock);
static DECLARE_COMPLETION(extra_updated_completion);

static void dss_register_vsync_isr(void);

static struct ovl_priv_data *get_ovl_priv(struct omap_overlay *ovl)
{
	return &dss_data.ovl_priv_data_array[ovl->id];
}

static struct mgr_priv_data *get_mgr_priv(struct omap_overlay_manager *mgr)
{
	return &dss_data.mgr_priv_data_array[mgr->id];
}

static void apply_init_priv(void)
{
	const int num_ovls = dss_feat_get_num_ovls();
	struct mgr_priv_data *mp;
	int i;

	spin_lock_init(&data_lock);

	for (i = 0; i < num_ovls; ++i) {
		struct ovl_priv_data *op;

		op = &dss_data.ovl_priv_data_array[i];

		op->info.global_alpha = 255;

		switch (i) {
		case 0:
			op->info.zorder = 0;
			break;
		case 1:
			op->info.zorder =
				dss_has_feature(FEAT_ALPHA_FREE_ZORDER) ? 3 : 0;
			break;
		case 2:
			op->info.zorder =
				dss_has_feature(FEAT_ALPHA_FREE_ZORDER) ? 2 : 0;
			break;
		case 3:
			op->info.zorder =
				dss_has_feature(FEAT_ALPHA_FREE_ZORDER) ? 1 : 0;
			break;
		}

		op->user_info = op->info;
	}

	/*
	 * Initialize some of the lcd_config fields for TV manager, this lets
	 * us prevent checking if the manager is LCD or TV at some places
	 */
	mp = &dss_data.mgr_priv_data_array[OMAP_DSS_CHANNEL_DIGIT];

	mp->lcd_config.video_port_width = 24;
	mp->lcd_config.clock_info.lck_div = 1;
	mp->lcd_config.clock_info.pck_div = 1;
}

/*
 * A LCD manager's stallmode decides whether it is in manual or auto update. TV
 * manager is always auto update, stallmode field for TV manager is false by
 * default
 */
static bool ovl_manual_update(struct omap_overlay *ovl)
{
	struct mgr_priv_data *mp = get_mgr_priv(ovl->manager);

	return mp->lcd_config.stallmode;
}

static bool mgr_manual_update(struct omap_overlay_manager *mgr)
{
	struct mgr_priv_data *mp = get_mgr_priv(mgr);

	return mp->lcd_config.stallmode;
}

static int dss_check_settings_low(struct omap_overlay_manager *mgr,
		bool applying)
{
	struct omap_overlay_info *oi;
	struct omap_overlay_manager_info *mi;
	struct omap_overlay *ovl;
	struct omap_overlay_info *ois[MAX_DSS_OVERLAYS];
	struct ovl_priv_data *op;
	struct mgr_priv_data *mp;

	mp = get_mgr_priv(mgr);

	if (!mp->enabled)
		return 0;

	if (applying && mp->user_info_dirty)
		mi = &mp->user_info;
	else
		mi = &mp->info;

	/* collect the infos to be tested into the array */
	list_for_each_entry(ovl, &mgr->overlays, list) {
		op = get_ovl_priv(ovl);

		if (!op->enabled && !op->enabling)
			oi = NULL;
		else if (applying && op->user_info_dirty)
			oi = &op->user_info;
		else
			oi = &op->info;

		ois[ovl->id] = oi;
	}

	return dss_mgr_check(mgr, mi, &mp->timings, &mp->lcd_config, ois);
}

/*
 * check manager and overlay settings using overlay_info from data->info
 */
static int dss_check_settings(struct omap_overlay_manager *mgr)
{
	return dss_check_settings_low(mgr, false);
}

/*
 * check manager and overlay settings using overlay_info from ovl->info if
 * dirty and from data->info otherwise
 */
static int dss_check_settings_apply(struct omap_overlay_manager *mgr)
{
	return dss_check_settings_low(mgr, true);
}

static bool need_isr(void)
{
	const int num_mgrs = dss_feat_get_num_mgrs();
	int i;

	for (i = 0; i < num_mgrs; ++i) {
		struct omap_overlay_manager *mgr;
		struct mgr_priv_data *mp;
		struct omap_overlay *ovl;

		mgr = omap_dss_get_overlay_manager(i);
		mp = get_mgr_priv(mgr);

		if (!mp->enabled)
			continue;

		if (mgr_manual_update(mgr)) {
			/* to catch FRAMEDONE */
			if (mp->updating)
				return true;
		} else {
			/* to catch GO bit going down */
			if (mp->busy)
				return true;

			/* to write new values to registers */
			if (mp->info_dirty)
				return true;

			/* to set GO bit */
			if (mp->shadow_info_dirty)
				return true;

			/*
			 * NOTE: we don't check extra_info flags for disabled
			 * managers, once the manager is enabled, the extra_info
			 * related manager changes will be taken in by HW.
			 */

			/* to write new values to registers */
			if (mp->extra_info_dirty)
				return true;

			/* to set GO bit */
			if (mp->shadow_extra_info_dirty)
				return true;

			list_for_each_entry(ovl, &mgr->overlays, list) {
				struct ovl_priv_data *op;

				op = get_ovl_priv(ovl);

				/*
				 * NOTE: we check extra_info flags even for
				 * disabled overlays, as extra_infos need to be
				 * always written.
				 */

				/* to write new values to registers */
				if (op->extra_info_dirty)
					return true;

				/* to set GO bit */
				if (op->shadow_extra_info_dirty)
					return true;

				if (!op->enabled)
					continue;

				/* to write new values to registers */
				if (op->info_dirty)
					return true;

				/* to set GO bit */
				if (op->shadow_info_dirty)
					return true;
			}
		}
	}

	return false;
}

static bool need_go(struct omap_overlay_manager *mgr)
{
	struct omap_overlay *ovl;
	struct mgr_priv_data *mp;
	struct ovl_priv_data *op;

	mp = get_mgr_priv(mgr);

	if (mp->shadow_info_dirty || mp->shadow_extra_info_dirty)
		return true;

	list_for_each_entry(ovl, &mgr->overlays, list) {
		op = get_ovl_priv(ovl);
		if (op->shadow_info_dirty || op->shadow_extra_info_dirty)
			return true;
	}

	return false;
}

/* returns true if an extra_info field is currently being updated */
static bool extra_info_update_ongoing(void)
{
	const int num_mgrs = dss_feat_get_num_mgrs();
	int i;

	for (i = 0; i < num_mgrs; ++i) {
		struct omap_overlay_manager *mgr;
		struct omap_overlay *ovl;
		struct mgr_priv_data *mp;

		mgr = omap_dss_get_overlay_manager(i);
		mp = get_mgr_priv(mgr);

		if (!mp->enabled)
			continue;

		if (!mp->updating)
			continue;

		if (mp->extra_info_dirty || mp->shadow_extra_info_dirty)
			return true;

		list_for_each_entry(ovl, &mgr->overlays, list) {
			struct ovl_priv_data *op = get_ovl_priv(ovl);

			if (op->extra_info_dirty || op->shadow_extra_info_dirty)
				return true;
		}
	}

	return false;
}

/* wait until no extra_info updates are pending */
static void wait_pending_extra_info_updates(void)
{
	bool updating;
	unsigned long flags;
	unsigned long t;
	int r;

	spin_lock_irqsave(&data_lock, flags);

	updating = extra_info_update_ongoing();

	if (!updating) {
		spin_unlock_irqrestore(&data_lock, flags);
		return;
	}

	init_completion(&extra_updated_completion);

	spin_unlock_irqrestore(&data_lock, flags);

	t = msecs_to_jiffies(500);
	r = wait_for_completion_timeout(&extra_updated_completion, t);
	if (r == 0)
		DSSWARN("timeout in wait_pending_extra_info_updates\n");
}

static inline struct omap_dss_device *dss_ovl_get_device(struct omap_overlay *ovl)
{
	return ovl->manager ?
		(ovl->manager->output ? ovl->manager->output->device : NULL) :
		NULL;
}

static inline struct omap_dss_device *dss_mgr_get_device(struct omap_overlay_manager *mgr)
{
	return mgr->output ? mgr->output->device : NULL;
}

static int dss_mgr_wait_for_vsync(struct omap_overlay_manager *mgr)
{
	unsigned long timeout = msecs_to_jiffies(500);
	struct omap_dss_device *dssdev = mgr->get_device(mgr);
	u32 irq;
	int r;

	r = dispc_runtime_get();
	if (r)
		return r;

	if (dssdev->type == OMAP_DISPLAY_TYPE_VENC)
		irq = DISPC_IRQ_EVSYNC_ODD;
	else if (dssdev->type == OMAP_DISPLAY_TYPE_HDMI)
		irq = DISPC_IRQ_EVSYNC_EVEN;
	else
		irq = dispc_mgr_get_vsync_irq(mgr->id);

	r = omap_dispc_wait_for_irq_interruptible_timeout(irq, timeout);

	dispc_runtime_put();

	return r;
}

static int dss_mgr_wait_for_go(struct omap_overlay_manager *mgr)
{
	unsigned long timeout = msecs_to_jiffies(500);
	struct mgr_priv_data *mp = get_mgr_priv(mgr);
	u32 irq;
	unsigned long flags;
	int r;
	int i;

	spin_lock_irqsave(&data_lock, flags);

	if (mgr_manual_update(mgr)) {
		spin_unlock_irqrestore(&data_lock, flags);
		return 0;
	}

	if (!mp->enabled) {
		spin_unlock_irqrestore(&data_lock, flags);
		return 0;
	}

	spin_unlock_irqrestore(&data_lock, flags);

	r = dispc_runtime_get();
	if (r)
		return r;

	irq = dispc_mgr_get_vsync_irq(mgr->id);

	i = 0;
	while (1) {
		bool shadow_dirty, dirty;

		spin_lock_irqsave(&data_lock, flags);
		dirty = mp->info_dirty;
		shadow_dirty = mp->shadow_info_dirty;
		spin_unlock_irqrestore(&data_lock, flags);

		if (!dirty && !shadow_dirty) {
			r = 0;
			break;
		}

		/* 4 iterations is the worst case:
		 * 1 - initial iteration, dirty = true (between VFP and VSYNC)
		 * 2 - first VSYNC, dirty = true
		 * 3 - dirty = false, shadow_dirty = true
		 * 4 - shadow_dirty = false */
		if (i++ == 3) {
			DSSERR("mgr(%d)->wait_for_go() not finishing\n",
					mgr->id);
			r = 0;
			break;
		}

		r = omap_dispc_wait_for_irq_interruptible_timeout(irq, timeout);
		if (r == -ERESTARTSYS)
			break;

		if (r) {
			DSSERR("mgr(%d)->wait_for_go() timeout\n", mgr->id);
			break;
		}
	}

	dispc_runtime_put();

	return r;
}

static int dss_mgr_wait_for_go_ovl(struct omap_overlay *ovl)
{
	unsigned long timeout = msecs_to_jiffies(500);
	struct ovl_priv_data *op;
	struct mgr_priv_data *mp;
	u32 irq;
	unsigned long flags;
	int r;
	int i;

	if (!ovl->manager)
		return 0;

	mp = get_mgr_priv(ovl->manager);

	spin_lock_irqsave(&data_lock, flags);

	if (ovl_manual_update(ovl)) {
		spin_unlock_irqrestore(&data_lock, flags);
		return 0;
	}

	if (!mp->enabled) {
		spin_unlock_irqrestore(&data_lock, flags);
		return 0;
	}

	spin_unlock_irqrestore(&data_lock, flags);

	r = dispc_runtime_get();
	if (r)
		return r;

	irq = dispc_mgr_get_vsync_irq(ovl->manager->id);

	op = get_ovl_priv(ovl);
	i = 0;
	while (1) {
		bool shadow_dirty, dirty;

		spin_lock_irqsave(&data_lock, flags);
		dirty = op->info_dirty;
		shadow_dirty = op->shadow_info_dirty;
		spin_unlock_irqrestore(&data_lock, flags);

		if (!dirty && !shadow_dirty) {
			r = 0;
			break;
		}

		/* 4 iterations is the worst case:
		 * 1 - initial iteration, dirty = true (between VFP and VSYNC)
		 * 2 - first VSYNC, dirty = true
		 * 3 - dirty = false, shadow_dirty = true
		 * 4 - shadow_dirty = false */
		if (i++ == 3) {
			DSSERR("ovl(%d)->wait_for_go() not finishing\n",
					ovl->id);
			r = 0;
			break;
		}

		r = omap_dispc_wait_for_irq_interruptible_timeout(irq, timeout);
		if (r == -ERESTARTSYS)
			break;

		if (r) {
			DSSERR("ovl(%d)->wait_for_go() timeout\n", ovl->id);
			break;
		}
	}

	dispc_runtime_put();

	return r;
}

static void dss_ovl_write_regs(struct omap_overlay *ovl)
{
	struct ovl_priv_data *op = get_ovl_priv(ovl);
	struct omap_overlay_info *oi;
	bool replication;
	struct mgr_priv_data *mp;
	int r;

	DSSDBG("writing ovl %d regs", ovl->id);

	if (!op->enabled || !op->info_dirty)
		return;

	oi = &op->info;

	mp = get_mgr_priv(ovl->manager);

	replication = dss_ovl_use_replication(mp->lcd_config, oi->color_mode);

	r = dispc_ovl_setup(ovl->id, oi, replication, &mp->timings, false);
	if (r) {
		/*
		 * We can't do much here, as this function can be called from
		 * vsync interrupt.
		 */
		DSSERR("dispc_ovl_setup failed for ovl %d\n", ovl->id);

		/* This will leave fifo configurations in a nonoptimal state */
		op->enabled = false;
		dispc_ovl_enable(ovl->id, false);
		dss_ovl_configure_cb(&op->cb, ovl->id, op->enabled);
		return;
	}

	op->info_dirty = false;
	if (mp->updating) {
		dss_ovl_configure_cb(&op->cb, ovl->id, op->enabled);
		op->shadow_info_dirty = true;
	}
}

static void dss_ovl_write_regs_extra(struct omap_overlay *ovl)
{
	struct ovl_priv_data *op = get_ovl_priv(ovl);
	struct mgr_priv_data *mp;
	struct writeback_cache_data *wbc;
	bool m2m_with_ovl = false;
	bool m2m_with_mgr = false;

	DSSDBG("writing ovl %d regs extra", ovl->id);

	if (!op->extra_info_dirty)
		return;

	if (dss_has_feature(FEAT_WB)) {
		/*
		 * Check, if this overlay is source for wb, then ignore mgr
		 * sources here.
		 */
		wbc = &dss_data.writeback_cache;
		if (wbc->enabled && omap_dss_check_wb(wbc, ovl->id, -1)) {
			DSSDBG("wb->enabled=%d for plane:%d\n",
						wbc->enabled, ovl->id);
			m2m_with_ovl = true;
		}
		/*
		 * Check, if this overlay is source for manager, which is
		 * source for wb, then ignore ovl sources.
		 */
		if (wbc->enabled && omap_dss_check_wb(wbc, -1, op->channel)) {
			DSSDBG("check wb mgr wb->enabled=%d for plane:%d\n",
							wbc->enabled, ovl->id);
			m2m_with_mgr = true;
		}
	}

	/* note: write also when op->enabled == false, so that the ovl gets
	 * disabled */

	dispc_ovl_enable(ovl->id, op->enabled);
	if (!m2m_with_ovl)
		dispc_ovl_set_channel_out(ovl->id, op->channel);
	else
		dispc_set_wb_channel_out(ovl->id);

	dispc_ovl_set_fifo_threshold(ovl->id, op->fifo_low, op->fifo_high);

	mp = get_mgr_priv(ovl->manager);

	op->extra_info_dirty = false;
	if (mp->updating) {
		dss_ovl_configure_cb(&op->cb, ovl->id, op->enabled);
		op->shadow_extra_info_dirty = true;
	}
}

static void dss_mgr_write_regs(struct omap_overlay_manager *mgr)
{
	struct mgr_priv_data *mp = get_mgr_priv(mgr);
	struct omap_overlay *ovl;
	struct ovl_priv_data *op;
	int used_ovls = 0;

	DSSDBG("writing mgr %d regs", mgr->id);

	if (!mp->enabled && !mp->info.wb_only)
		return;

	WARN_ON(mp->busy);

	/* Commit overlay settings */
	list_for_each_entry(ovl, &mgr->overlays, list) {
		dss_ovl_write_regs(ovl);
		dss_ovl_write_regs_extra(ovl);
		op = get_ovl_priv(ovl);
		if (op->channel == mgr->id && op->enabled)
			used_ovls++;
	}

	if (mp->info_dirty) {
		dispc_mgr_setup(mgr->id, &mp->info);

		mp->info_dirty = false;
		if (mp->updating) {
			dss_ovl_configure_cb(&mp->cb, mgr->id, used_ovls);
			mp->shadow_info_dirty = true;
		}
	}
}

static void dss_mgr_write_regs_extra(struct omap_overlay_manager *mgr)
{
	struct mgr_priv_data *mp = get_mgr_priv(mgr);

	DSSDBG("writing mgr %d regs extra", mgr->id);

	if (!mp->extra_info_dirty)
		return;

#ifdef CONFIG_DISPLAY_SKIP_INIT
	if (!omapdss_skipinit() || mgr->id != OMAP_DSS_CHANNEL_LCD) {
#endif
		dispc_mgr_set_timings(mgr->id, &mp->timings);

		/* lcd_config parameters */
		if (dss_mgr_is_lcd(mgr->id))
			dispc_mgr_set_lcd_config(mgr->id, &mp->lcd_config);
#ifdef CONFIG_DISPLAY_SKIP_INIT
	}
#endif

	mp->extra_info_dirty = false;
	if (mp->updating)
		mp->shadow_extra_info_dirty = true;
}

static void dss_write_regs(void)
{
	const int num_mgrs = omap_dss_get_num_overlay_managers();
	int i;

	for (i = 0; i < num_mgrs; ++i) {
		struct omap_overlay_manager *mgr;
		struct mgr_priv_data *mp;
		int r;

		mgr = omap_dss_get_overlay_manager(i);
		mp = get_mgr_priv(mgr);

		if (!mp->enabled || mgr_manual_update(mgr) || mp->busy)
			if (!mp->info.wb_only)
				continue;

		r = dss_check_settings(mgr);
		if (r) {
			DSSERR("cannot write registers for manager %s: "
					"illegal configuration\n", mgr->name);
			continue;
		}

		dss_mgr_write_regs(mgr);
		dss_mgr_write_regs_extra(mgr);
	}
}

static void dss_set_go_bits(void)
{
	const int num_mgrs = omap_dss_get_num_overlay_managers();
	int i;

	for (i = 0; i < num_mgrs; ++i) {
		struct omap_overlay_manager *mgr;
		struct mgr_priv_data *mp;

		mgr = omap_dss_get_overlay_manager(i);
		mp = get_mgr_priv(mgr);

		if (!mp->enabled || mgr_manual_update(mgr) || mp->busy)
			continue;

		if (!need_go(mgr))
			continue;

		mp->busy = true;

		if (!dss_data.irq_enabled && need_isr())
			dss_register_vsync_isr();

		dispc_mgr_go(mgr->id);
	}

}

static void dss_wb_set_dss_writeback_info(struct omap_dss_writeback_info *dss_info,
					  struct omap_writeback_info *wb_info)
{
	dss_info->buf_width	= wb_info->buffer_width;
	dss_info->width		= wb_info->width;
	dss_info->height	= wb_info->height;
	dss_info->color_mode	= wb_info->dss_mode;
	dss_info->p_uv_addr	= wb_info->p_uv_addr;
	dss_info->paddr		= wb_info->paddr;
	dss_info->pre_mult_alpha = 0;
	dss_info->rotation	= wb_info->rotation;
	dss_info->rotation_type	= wb_info->rotation_type;
	dss_info->mirror	= false;
}

static void dss_wb_write_regs(struct omap_overlay_manager *mgr,
			      struct omap_writeback *wb)
{
	struct writeback_cache_data *wbc;
	struct mgr_priv_data *mp;
	struct omap_dss_writeback_info dss_wb_info;
	u32 fifo_low, fifo_high;
	bool use_fifo_merge = false;
	struct omap_video_timings *timings = NULL;
	int r = 0;

	if (dss_has_feature(FEAT_WB))
		wbc = &dss_data.writeback_cache;
	else
		wbc = NULL;

	mp = get_mgr_priv(mgr);
	if (!mp)
		timings = &mp->timings;

	/* setup WB for capture mode */
	if (wbc && wbc->enabled && wbc->dirty && mp) {
		dispc_ovl_compute_fifo_thresholds(OMAP_DSS_WB, &fifo_low,
				&fifo_high, use_fifo_merge, true);
		dispc_ovl_set_fifo_threshold(OMAP_DSS_WB, fifo_low, fifo_high);

		dss_wb_set_dss_writeback_info(&dss_wb_info, &wb->info);

		dispc_wb_set_channel_in(wbc->source);

		/* change resolution of manager if it works in MEM2MEM mode */
		if (wbc->mode == OMAP_WB_MEM2MEM_MODE) {
			if (wbc->source == OMAP_WB_TV)
				dispc_mgr_set_size(OMAP_DSS_CHANNEL_DIGIT,
						   wbc->out_width,
						   wbc->out_height);
			else if (wbc->source == OMAP_WB_LCD2)
				dispc_mgr_set_size(OMAP_DSS_CHANNEL_LCD2,
						   wbc->out_width,
						   wbc->out_height);
			else if (wbc->source == OMAP_WB_LCD1)
				dispc_mgr_set_size(OMAP_DSS_CHANNEL_LCD,
						   wbc->out_width,
						   wbc->out_height);
		}

		/* writeback is enabled for this plane - set accordingly */
		r = dispc_wb_setup(&dss_wb_info, wb->info.mode, timings);
		if (r)
			DSSERR("dispc_setup_wb failed with error %d\n", r);
		wbc->dirty = false;
		wbc->shadow_dirty = true;
	}
}
static void dss_wb_ovl_enable(void)
{
	struct writeback_cache_data *wbc;
	struct omap_overlay *ovl;
	const int num_ovls = dss_feat_get_num_ovls();
	int i;

	if (dss_has_feature(FEAT_WB))
		wbc = &dss_data.writeback_cache;
	else
		wbc = NULL;
	if (dss_has_feature(FEAT_WB)) {
		/* Enable WB plane and source plane */
		DSSDBG("configure manager wbc->shadow_dirty = %d",
		wbc->shadow_dirty);
		if (wbc->shadow_dirty && wbc->enabled) {
			switch (wbc->source) {
			case OMAP_WB_GFX:
			case OMAP_WB_VID1:
			case OMAP_WB_VID2:
			case OMAP_WB_VID3:
				wbc->shadow_dirty = false;
				dispc_ovl_enable(OMAP_DSS_WB, true);
				break;
			case OMAP_WB_LCD1:
			case OMAP_WB_LCD2:
			case OMAP_WB_LCD3:
			case OMAP_WB_TV:
				dispc_ovl_enable(OMAP_DSS_WB, true);
				wbc->shadow_dirty = false;
				break;
			}
		} else if (wbc->dirty && !wbc->enabled) {
			if (wbc->mode == OMAP_WB_MEM2MEM_MODE &&
				wbc->source >= OMAP_WB_GFX) {
				/* This is a workaround. According to TRM
				 * we should disable the manager but it will
				 * cause blinking of panel. WA is to disable
				 * pipe which was used as source of WB and do
				 * dummy enable and disable of WB.
				 */
				dispc_ovl_enable(OMAP_DSS_WB, true);
			} else if (wbc->mode == OMAP_WB_MEM2MEM_MODE &&
					wbc->source < OMAP_WB_GFX) {
				/* This is a workaround that prevents SYNC_LOST
				 * on changing pipe channelout from manager
				 * which was used as a source of wb to another
				 * manager. Manager could free pipes after wb
				 * will send SYNC message but that will start
				 * wb capture. To prevent that we reconnect the
				 * pipe from the manager to wb and do a dummy
				 * enabling and disabling of wb - the pipe will
				 * be freed and capture won't start because
				 * source pipe is switched off. */
				for (i = 0; i < num_ovls; ++i) {
					ovl = omap_dss_get_overlay(i);

					if ((int)ovl->manager->id ==
						(int)wbc->source) {
						dispc_ovl_enable(i, false);
						dispc_wb_set_channel_in(
							OMAP_DSS_GFX + i);
						dispc_set_wb_channel_out(i);
						dispc_ovl_enable(
							OMAP_DSS_WB, true);
						dispc_ovl_enable(
							OMAP_DSS_WB, false);
					}
				}
			} else
			/* capture mode case */
			dispc_ovl_enable(OMAP_DSS_WB, false);
			wbc->dirty = false;
		}
	}
}

static void dss_wb_set_go_bits(void)
{
	struct writeback_cache_data *wbc;
	if (dss_has_feature(FEAT_WB))
		wbc = &dss_data.writeback_cache;
	else
		wbc = NULL;
	/* WB GO bit has to be used only in case of
	 * capture mode and not in memory mode
	 */
	if (wbc && wbc->mode != OMAP_WB_MEM2MEM_MODE)
		dispc_wb_go();
}

static void mgr_clear_shadow_dirty(struct omap_overlay_manager *mgr)
{
	struct omap_overlay *ovl;
	struct mgr_priv_data *mp;
	struct ovl_priv_data *op;

	mp = get_mgr_priv(mgr);

	if (mp->shadow_info_dirty)
		dss_ovl_program_cb(&mp->cb, mgr->id);

	mp->shadow_info_dirty = false;
	mp->shadow_extra_info_dirty = false;

	list_for_each_entry(ovl, &mgr->overlays, list) {
		op = get_ovl_priv(ovl);
		if (op->shadow_info_dirty || op->shadow_extra_info_dirty) {
			dss_ovl_program_cb(&op->cb, ovl->id);
			op->dispc_channel = op->channel;
		}
		op->shadow_info_dirty = false;
		op->shadow_extra_info_dirty = false;
	}
}

static void schedule_completion_irq(void);

static void dss_completion_irq_handler(void *data, u32 mask)
{
	struct mgr_priv_data *mp;
	struct ovl_priv_data *op;
	struct omap_overlay_manager *mgr;
	struct omap_overlay *ovl;
	int num_ovls = dss_feat_get_num_ovls();
	const int num_mgrs = dss_feat_get_num_mgrs();
	const u32 masks[] = {
		DISPC_IRQ_FRAMEDONE | DISPC_IRQ_VSYNC,
		DISPC_IRQ_FRAMEDONETV | DISPC_IRQ_EVSYNC_EVEN |
		DISPC_IRQ_EVSYNC_ODD,
		DISPC_IRQ_FRAMEDONE2 | DISPC_IRQ_VSYNC2,
		DISPC_IRQ_FRAMEDONE3 | DISPC_IRQ_VSYNC3
	};
	int i;

	spin_lock(&data_lock);

	for (i = 0; i < num_mgrs; i++) {
		mgr = omap_dss_get_overlay_manager(i);
		mp = get_mgr_priv(mgr);
		if (mask & masks[i]) {
			if (mgr && mgr->output->device)
				mgr->output->device->first_vsync = true;
			dss_ovl_cb(&mp->cb.dispc, i, DSS_COMPLETION_DISPLAYED);
			mp->cb.dispc_displayed = true;
		}
	}

	/* notify all overlays on that manager */
	for (i = 0; i < num_ovls; i++) {
		ovl = omap_dss_get_overlay(i);
		op = get_ovl_priv(ovl);
		if (mask & masks[op->channel]) {
			dss_ovl_cb(&op->cb.dispc, i, DSS_COMPLETION_DISPLAYED);
			op->cb.dispc_displayed = true;
		}
	}

	schedule_completion_irq();

	spin_unlock(&data_lock);
}

static void schedule_completion_irq(void)
{
	struct mgr_priv_data *mp;
	struct ovl_priv_data *op;
	const int num_ovls = ARRAY_SIZE(dss_data.ovl_priv_data_array);
	const int num_mgrs = dss_feat_get_num_mgrs();
	const u32 masks[] = {
		DISPC_IRQ_FRAMEDONE | DISPC_IRQ_VSYNC,
		DISPC_IRQ_FRAMEDONETV | DISPC_IRQ_EVSYNC_EVEN |
		DISPC_IRQ_EVSYNC_ODD,
		DISPC_IRQ_FRAMEDONE2 | DISPC_IRQ_VSYNC2,
		DISPC_IRQ_FRAMEDONE3 | DISPC_IRQ_VSYNC3
	};
	u32 mask = 0;
	int i;

	for (i = 0; i < num_mgrs; i++) {
		mp = &dss_data.mgr_priv_data_array[i];
		if (mp->cb.dispc.fn && (mp->cb.dispc.mask &
					DSS_COMPLETION_DISPLAYED))
			mask |= masks[i];
	}

	/* notify all overlays on that manager */
	for (i = 0; i < num_ovls; i++) {
		op = &dss_data.ovl_priv_data_array[i];
		if (op->cb.dispc.fn && op->enabled &&
				(op->cb.dispc.mask & DSS_COMPLETION_DISPLAYED))
			mask |= masks[op->channel];
	}

	if (mask != dss_data.comp_irq_enabled) {
		if (dss_data.comp_irq_enabled)
			omap_dispc_unregister_isr(dss_completion_irq_handler,
					NULL, dss_data.comp_irq_enabled);
		if (mask)
			omap_dispc_register_isr(dss_completion_irq_handler,
					NULL, mask);
		dss_data.comp_irq_enabled = mask;
	}
}

static void dss_mgr_start_update_compat(struct omap_overlay_manager *mgr)
{
	struct mgr_priv_data *mp = get_mgr_priv(mgr);
	unsigned long flags;
	int r;

	spin_lock_irqsave(&data_lock, flags);

	WARN_ON(mp->updating);

	r = dss_check_settings(mgr);
	if (r) {
		DSSERR("cannot start manual update: illegal configuration\n");
		spin_unlock_irqrestore(&data_lock, flags);
		return;
	}

	schedule_completion_irq();

	dss_mgr_write_regs(mgr);
	dss_mgr_write_regs_extra(mgr);

	mp->updating = true;

	if (!dss_data.irq_enabled && need_isr())
		dss_register_vsync_isr();

	dispc_mgr_enable_sync(mgr->id);

	/* for manually updated displays invoke dsscomp callbacks manually,
	 * as logic that relays on shadow_dirty flag can't correctly release
	 * previous composition
	 */
	dss_ovl_configure_cb(&mp->cb, mgr->id, true);
	dss_ovl_program_cb(&mp->cb, mgr->id);

	mgr_clear_shadow_dirty(mgr);

	spin_unlock_irqrestore(&data_lock, flags);
}

static void dss_apply_irq_handler(void *data, u32 mask);

static void dss_register_vsync_isr(void)
{
	const int num_mgrs = dss_feat_get_num_mgrs();
	u32 mask;
	int r, i;

	mask = 0;
	for (i = 0; i < num_mgrs; ++i)
		mask |= dispc_mgr_get_vsync_irq(i);

	for (i = 0; i < num_mgrs; ++i)
		mask |= dispc_mgr_get_framedone_irq(i);

	r = omap_dispc_register_isr(dss_apply_irq_handler, NULL, mask);
	WARN_ON(r);

	dss_data.irq_enabled = true;
}

static void dss_unregister_vsync_isr(void)
{
	const int num_mgrs = dss_feat_get_num_mgrs();
	u32 mask;
	int r, i;

	mask = 0;
	for (i = 0; i < num_mgrs; ++i)
		mask |= dispc_mgr_get_vsync_irq(i);

	for (i = 0; i < num_mgrs; ++i)
		mask |= dispc_mgr_get_framedone_irq(i);

	r = omap_dispc_unregister_isr(dss_apply_irq_handler, NULL, mask);
	WARN_ON(r);

	dss_data.irq_enabled = false;
}

static void dss_apply_irq_handler(void *data, u32 mask)
{
	const int num_mgrs = dss_feat_get_num_mgrs();
	int i;
	bool extra_updating;

	spin_lock(&data_lock);

	/* clear busy, updating flags, shadow_dirty flags */
	for (i = 0; i < num_mgrs; i++) {
		struct omap_overlay_manager *mgr;
		struct mgr_priv_data *mp;

		mgr = omap_dss_get_overlay_manager(i);
		mp = get_mgr_priv(mgr);

		if (!mp->enabled)
			continue;

		mp->updating = dispc_mgr_is_enabled(i);

		if (!mgr_manual_update(mgr)) {
			bool was_busy = mp->busy;
			mp->busy = dispc_mgr_go_busy(i);

			if (was_busy && !mp->busy)
				mgr_clear_shadow_dirty(mgr);
		}
	}

	schedule_completion_irq();
	dss_write_regs();
	dss_set_go_bits();

	extra_updating = extra_info_update_ongoing();
	if (!extra_updating)
		complete_all(&extra_updated_completion);

	/* call framedone handlers for manual update displays */
	for (i = 0; i < num_mgrs; i++) {
		struct omap_overlay_manager *mgr;
		struct mgr_priv_data *mp;

		mgr = omap_dss_get_overlay_manager(i);
		mp = get_mgr_priv(mgr);

		if (!mgr_manual_update(mgr) || !mp->framedone_handler)
			continue;

		if (mask & dispc_mgr_get_framedone_irq(i))
			mp->framedone_handler(mp->framedone_handler_data);
	}

	if (!need_isr())
		dss_unregister_vsync_isr();

	spin_unlock(&data_lock);
}

int dss_mgr_blank(struct omap_overlay_manager *mgr,
			bool wait_for_go)
{
	struct ovl_priv_data *op;
	struct mgr_priv_data *mp;
	unsigned long flags;
	int r, r_get, i;
	const int num_mgrs = dss_feat_get_num_mgrs();

	DSSDBG("dss_mgr_blank(%s,wait=%d)\n", mgr->name, wait_for_go);

	r = dispc_runtime_get();
	r_get = r;
	/* still clear cache even if failed to get clocks, just don't config */


	/* disable overlays in overlay user info structs and in data info */
	for (i = 0; i < omap_dss_get_num_overlays(); i++) {
		struct omap_overlay *ovl;

		ovl = omap_dss_get_overlay(i);

		if (ovl->manager != mgr)
			continue;

		r = ovl->disable(ovl);

		spin_lock_irqsave(&data_lock, flags);
		op = get_ovl_priv(ovl);

		/* complete unconfigured info */
		if (op->user_info_dirty)
			dss_ovl_cb(&op->user_info.cb, i,
					DSS_COMPLETION_ECLIPSED_SET);
		dss_ovl_cb(&op->cb.info, i, DSS_COMPLETION_ECLIPSED_CACHE);
		op->cb.info.fn = NULL;

		op->user_info_dirty = false;
		op->info_dirty = true;
		op->enabled = false;
		spin_unlock_irqrestore(&data_lock, flags);
	}

	spin_lock_irqsave(&data_lock, flags);
	/* dirty manager */
	mp = get_mgr_priv(mgr);
	if (mp->user_info_dirty)
		dss_ovl_cb(&mp->user_info.cb, mgr->id,
				DSS_COMPLETION_ECLIPSED_SET);
	dss_ovl_cb(&mp->cb.info, mgr->id, DSS_COMPLETION_ECLIPSED_CACHE);
	mp->cb.info.fn = NULL;
	mp->user_info.cb.fn = NULL;
	mp->info_dirty = true;
	mp->user_info_dirty = false;

	/*
	 * TRICKY: Enable apply irq even if not waiting for vsync, so that
	 * DISPC programming takes place in case GO bit was on.
	 */
	if (!dss_data.irq_enabled) {
		u32 mask = 0;
		for (i = 0; i < num_mgrs; ++i)
			mask |= dispc_mgr_get_vsync_irq(i);

		for (i = 0; i < num_mgrs; ++i)
			mask |= dispc_mgr_get_framedone_irq(i);

		r = omap_dispc_register_isr(dss_apply_irq_handler, NULL, mask);
		dss_data.irq_enabled = true;
	}

	if (!r_get) {
		dss_write_regs();
		dss_set_go_bits();
	}

	if (r_get || !wait_for_go) {
		/* pretend that programming has happened */
		for (i = 0; i < omap_dss_get_num_overlays(); ++i) {
			op = &dss_data.ovl_priv_data_array[i];
			if (op->channel != mgr->id)
				continue;
			if (op->info_dirty)
				dss_ovl_configure_cb(&op->cb, i, false);
			if (op->shadow_info_dirty) {
				dss_ovl_program_cb(&op->cb, i);
				op->dispc_channel = op->channel;
				op->shadow_info_dirty = false;
			} else {
				pr_warn("ovl%d-shadow is not dirty\n", i);
			}
		}

		if (mp->info_dirty)
			dss_ovl_configure_cb(&mp->cb, i, false);
		if (mp->shadow_info_dirty) {
			dss_ovl_program_cb(&mp->cb, i);
			mp->shadow_info_dirty = false;
		} else {
			pr_warn("mgr%d-shadow is not dirty\n", mgr->id);
		}
	}

	spin_unlock_irqrestore(&data_lock, flags);

	if (wait_for_go)
		mgr->wait_for_go(mgr);

	if (!r_get)
		dispc_runtime_put();

	return r;
}

int omap_dss_manager_unregister_callback(struct omap_overlay_manager *mgr,
					 struct omapdss_ovl_cb *cb)
{
	unsigned long flags;
	int r = 0;
	struct mgr_priv_data *mp = get_mgr_priv(mgr);
	spin_lock_irqsave(&data_lock, flags);
	if (mp->user_info_dirty &&
	    mp->user_info.cb.fn == cb->fn &&
	    mp->user_info.cb.data == cb->data)
		mp->user_info.cb.fn = NULL;
	else
		r = -EPERM;
	spin_unlock_irqrestore(&data_lock, flags);
	return r;
}

static void omap_dss_mgr_apply_ovl(struct omap_overlay *ovl)
{
	struct ovl_priv_data *op;

	op = get_ovl_priv(ovl);

	if (!op->user_info_dirty)
		return;

	/* complete unconfigured info */
	dss_ovl_cb(&op->cb.info, ovl->id,
		   DSS_COMPLETION_ECLIPSED_CACHE);

	op->cb.info = op->user_info.cb;
	op->user_info.cb.fn = NULL;

	op->user_info_dirty = false;
	op->info_dirty = true;
	op->info = op->user_info;
}

static void omap_dss_mgr_apply_mgr(struct omap_overlay_manager *mgr)
{
	struct mgr_priv_data *mp;

	mp = get_mgr_priv(mgr);

	if (!mp->user_info_dirty)
		return;

	/* complete unconfigured info */
	dss_ovl_cb(&mp->cb.info, mgr->id,
		   DSS_COMPLETION_ECLIPSED_CACHE);

	mp->cb.info = mp->user_info.cb;
	mp->user_info.cb.fn = NULL;

	mp->user_info_dirty = false;
	mp->info_dirty = true;
	mp->info = mp->user_info;
}

static int omap_dss_mgr_apply(struct omap_overlay_manager *mgr)
{
	unsigned long flags;
	struct omap_overlay *ovl;
	struct omap_overlay_manager_info info;
	int r;

	DSSDBG("omap_dss_mgr_apply(%s)\n", mgr->name);

	mgr->get_manager_info(mgr, &info);

	spin_lock_irqsave(&data_lock, flags);

	if (!(mgr->get_device(mgr))) {
		pr_info_ratelimited("cannot aply mgr(%s)--invalid device\n",
				mgr->name);
		r = -ENODEV;
		goto done;
	}

	if (!info.wb_only) {
		r = dss_check_settings_apply(mgr);
		if (r) {
			DSSERR("failed to apply: illegal configuration.\n");
			goto done;
		}
	}

	/* Configure overlays */
	list_for_each_entry(ovl, &mgr->overlays, list)
		omap_dss_mgr_apply_ovl(ovl);

	if ((mgr->get_device(mgr))->state != OMAP_DSS_DISPLAY_ACTIVE &&
	     !info.wb_only) {
		struct writeback_cache_data *wbc;

		if (dss_has_feature(FEAT_WB))
			wbc = &dss_data.writeback_cache;
		else
			wbc = NULL;

		/* in case, if WB was configured with MEM2MEM with manager
		 * mode, but manager, which is source for WB, is not marked as
		 * wb_only, then skip apply operation. We have such case, when
		 * composition was sent to disable pipes, which are sources for
		 * WB.
		 */
		if (wbc && wbc->mode == OMAP_WB_MEM2MEM_MODE &&
		    (int)wbc->source == (int)mgr->id &&
		    (mgr->get_device(mgr)) &&
		    (mgr->get_device(mgr))->state != OMAP_DSS_DISPLAY_ACTIVE) {
			r = 0;
			goto done;
		}

		pr_info_ratelimited("cannot apply mgr(%s) on inactive device\n",
				mgr->name);
		r = -ENODEV;
		goto done;
	}

	/* Configure manager */
	omap_dss_mgr_apply_mgr(mgr);
done:
	spin_unlock_irqrestore(&data_lock, flags);

	return r;
}

static int omap_dss_wb_mgr_apply(struct omap_overlay_manager *mgr,
				 struct omap_writeback *wb)
{
	struct writeback_cache_data *wbc;
	unsigned long flags;

	DSSDBG("omap_dss_wb_mgr_apply(%s)\n", mgr->name);
	if (!wb) {
		DSSERR("[%s][%d] No WB!\n", __FILE__, __LINE__);
		return -EINVAL;
	}

	/* skip composition, if manager is enabled. It happens when HDMI/TV
	 * physical layer is activated in the time, when MEM2MEM with manager
	 * mode is used.
	 */
	if (wb->info.source == OMAP_WB_TV &&
			dispc_mgr_is_enabled(OMAP_DSS_CHANNEL_DIGIT) &&
				wb->info.mode == OMAP_WB_MEM2MEM_MODE) {
		DSSERR("manager %d busy, dropping\n", mgr->id);
		return -EBUSY;
	}

	spin_lock_irqsave(&data_lock, flags);
	wbc = &dss_data.writeback_cache;

	if (wb && wb->info.enabled) {
		/* if source is an overlay, mode cannot be capture */
		if ((wb->info.source >= OMAP_WB_GFX) &&
			(wb->info.mode != OMAP_WB_MEM2MEM_MODE))
			return -EINVAL;
		wbc->enabled = true;
		wbc->mode = wb->info.mode;
		wbc->color_mode = wb->info.dss_mode;
		wbc->out_width = wb->info.out_width;
		wbc->out_height = wb->info.out_height;
		wbc->width = wb->info.width;
		wbc->height = wb->info.height;

		wbc->paddr = wb->info.paddr;
		wbc->p_uv_addr = wb->info.p_uv_addr;

		wbc->capturemode = wb->info.capturemode;
		wbc->burst_size = BURST_SIZE_X8;

		/*
		 * only these FIFO values work in WB capture mode for all
		 * downscale scenarios. Other FIFO values cause a SYNC_LOST
		 * on LCD due to b/w issues.
		 */
		wbc->fifo_high = 0x10;
		wbc->fifo_low = 0x8;
		wbc->source = wb->info.source;

		wbc->rotation = wb->info.rotation;
		wbc->rotation_type = wb->info.rotation_type;

		wbc->dirty = true;
		wbc->shadow_dirty = false;
	} else if (wb && (wbc->enabled != wb->info.enabled)) {
		/* disable WB if not disabled already*/
		wbc->enabled = wb->info.enabled;
		wbc->dirty = true;
		wbc->shadow_dirty = false;
	}

	dss_wb_write_regs(mgr, wb);
	dss_wb_ovl_enable();
	dss_wb_set_go_bits();

	spin_unlock_irqrestore(&data_lock, flags);
	return 0;
}

#ifdef CONFIG_DEBUG_FS
static void seq_print_cb(struct seq_file *s, struct omapdss_ovl_cb *cb)
{
        if (!cb->fn) {
                seq_printf(s, "(none)\n");
                return;
        }

        seq_printf(s, "mask=%c%c%c%c [%p] %pf\n",
                   (cb->mask & DSS_COMPLETION_CHANGED) ? 'C' : '-',
                   (cb->mask & DSS_COMPLETION_PROGRAMMED) ? 'P' : '-',
                   (cb->mask & DSS_COMPLETION_DISPLAYED) ? 'D' : '-',
                   (cb->mask & DSS_COMPLETION_RELEASED) ? 'R' : '-',
                   cb->data,
                   cb->fn);
}
#endif

void seq_print_cbs(struct omap_overlay_manager *mgr, struct seq_file *s)
{
#ifdef CONFIG_DEBUG_FS
        struct mgr_priv_data *mp;
        unsigned long flags;

        spin_lock_irqsave(&data_lock, flags);

        mp = get_mgr_priv(mgr);

        seq_printf(s, "  DISPC pipeline:\n\n"
                                "    user_info:%13s ", mp->user_info_dirty ?
                                "DIRTY" : "clean");
        seq_print_cb(s, &mp->user_info.cb);
        seq_printf(s, "    info:%12s ", mp->info_dirty ? "DIRTY" : "clean");
        seq_print_cb(s, &mp->cb.info);
        seq_printf(s, "    shadow:  %s %s ", mp->cb.shadow_enabled ? "ACT" :
                                "off", mp->shadow_info_dirty ?
                                "DIRTY" : "clean");
        seq_print_cb(s, &mp->cb.shadow);
        seq_printf(s, "    dispc:%12s ", mp->cb.dispc_displayed ?
                                "DISPLAYED" : "");
        seq_print_cb(s, &mp->cb.dispc);
        seq_printf(s, "\n");

        spin_unlock_irqrestore(&data_lock, flags);
#endif
}

static void dss_apply_ovl_enable(struct omap_overlay *ovl, bool enable)
{
	struct ovl_priv_data *op;

	op = get_ovl_priv(ovl);

	if (op->enabled == enable)
		return;

	op->enabled = enable;
	op->extra_info_dirty = true;
}

static void dss_apply_ovl_fifo_thresholds(struct omap_overlay *ovl,
		u32 fifo_low, u32 fifo_high)
{
	struct ovl_priv_data *op = get_ovl_priv(ovl);

	if (op->fifo_low == fifo_low && op->fifo_high == fifo_high)
		return;

	op->fifo_low = fifo_low;
	op->fifo_high = fifo_high;
	op->extra_info_dirty = true;
}

static void dss_ovl_setup_fifo(struct omap_overlay *ovl)
{
	struct ovl_priv_data *op = get_ovl_priv(ovl);
	u32 fifo_low, fifo_high;
	bool use_fifo_merge = false;

	if (!op->enabled && !op->enabling)
		return;

	dispc_ovl_compute_fifo_thresholds(ovl->id, &fifo_low, &fifo_high,
			use_fifo_merge, ovl_manual_update(ovl));

	dss_apply_ovl_fifo_thresholds(ovl, fifo_low, fifo_high);
}

static void dss_mgr_setup_fifos(struct omap_overlay_manager *mgr)
{
	struct omap_overlay *ovl;
	struct mgr_priv_data *mp;

	mp = get_mgr_priv(mgr);

	if (!mp->enabled)
		return;

	list_for_each_entry(ovl, &mgr->overlays, list)
		dss_ovl_setup_fifo(ovl);
}

static void dss_setup_fifos(void)
{
	const int num_mgrs = omap_dss_get_num_overlay_managers();
	struct omap_overlay_manager *mgr;
	int i;

	for (i = 0; i < num_mgrs; ++i) {
		mgr = omap_dss_get_overlay_manager(i);
		dss_mgr_setup_fifos(mgr);
	}
}

static int dss_mgr_enable_compat(struct omap_overlay_manager *mgr)
{
	struct mgr_priv_data *mp = get_mgr_priv(mgr);
	unsigned long flags;
	int r;

	mutex_lock(&apply_lock);

	if (mp->enabled)
		goto out;

	spin_lock_irqsave(&data_lock, flags);

	mp->enabled = true;

	r = dss_check_settings(mgr);
	if (r) {
		DSSERR("failed to enable manager %d: check_settings failed\n",
				mgr->id);
		goto err;
	}

	dss_setup_fifos();

	dss_write_regs();
	dss_set_go_bits();

	if (!mgr_manual_update(mgr))
		mp->updating = true;

	if (!dss_data.irq_enabled && need_isr())
		dss_register_vsync_isr();

	spin_unlock_irqrestore(&data_lock, flags);

	if (!mgr_manual_update(mgr))
		dispc_mgr_enable_sync(mgr->id);

out:
	mutex_unlock(&apply_lock);

	return 0;

err:
	mp->enabled = false;
	spin_unlock_irqrestore(&data_lock, flags);
	mutex_unlock(&apply_lock);
	return r;
}

static void dss_mgr_disable_compat(struct omap_overlay_manager *mgr)
{
	struct mgr_priv_data *mp = get_mgr_priv(mgr);
	unsigned long flags;

	mutex_lock(&apply_lock);

	if (!mp->enabled)
		goto out;

	if (!mgr_manual_update(mgr))
		dispc_mgr_disable_sync(mgr->id);

	spin_lock_irqsave(&data_lock, flags);

	mp->updating = false;
	mp->enabled = false;

	spin_unlock_irqrestore(&data_lock, flags);

out:
	mutex_unlock(&apply_lock);
}

static int dss_mgr_set_info(struct omap_overlay_manager *mgr,
		struct omap_overlay_manager_info *info)
{
	struct mgr_priv_data *mp = get_mgr_priv(mgr);
	unsigned long flags;
	int r;

	r = dss_mgr_simple_check(mgr, info);
	if (r)
		return r;

	spin_lock_irqsave(&data_lock, flags);

	mp->user_info = *info;
	mp->user_info_dirty = true;

	spin_unlock_irqrestore(&data_lock, flags);

	return 0;
}

static void dss_mgr_get_info(struct omap_overlay_manager *mgr,
		struct omap_overlay_manager_info *info)
{
	struct mgr_priv_data *mp = get_mgr_priv(mgr);
	unsigned long flags;

	spin_lock_irqsave(&data_lock, flags);

	*info = mp->user_info;

	spin_unlock_irqrestore(&data_lock, flags);
}

static int dss_mgr_set_output(struct omap_overlay_manager *mgr,
		struct omap_dss_output *output)
{
	int r;

	mutex_lock(&apply_lock);

	if (mgr->output) {
		DSSERR("manager %s is already connected to an output\n",
			mgr->name);
		r = -EINVAL;
		goto err;
	}

	if ((mgr->supported_outputs & output->id) == 0) {
		DSSERR("output does not support manager %s\n",
			mgr->name);
		r = -EINVAL;
		goto err;
	}

	output->manager = mgr;
	mgr->output = output;

	mutex_unlock(&apply_lock);

	return 0;
err:
	mutex_unlock(&apply_lock);
	return r;
}

static int dss_mgr_unset_output(struct omap_overlay_manager *mgr)
{
	int r;
	struct mgr_priv_data *mp = get_mgr_priv(mgr);
	unsigned long flags;

	mutex_lock(&apply_lock);

	if (!mgr->output) {
		DSSERR("failed to unset output, output not set\n");
		r = -EINVAL;
		goto err;
	}

	spin_lock_irqsave(&data_lock, flags);

	if (mp->enabled) {
		DSSERR("output can't be unset when manager is enabled\n");
		r = -EINVAL;
		goto err1;
	}

	spin_unlock_irqrestore(&data_lock, flags);

	mgr->output->manager = NULL;
	mgr->output = NULL;

	mutex_unlock(&apply_lock);

	return 0;
err1:
	spin_unlock_irqrestore(&data_lock, flags);
err:
	mutex_unlock(&apply_lock);

	return r;
}

static void dss_apply_mgr_timings(struct omap_overlay_manager *mgr,
		const struct omap_video_timings *timings)
{
	struct mgr_priv_data *mp = get_mgr_priv(mgr);

	mp->timings = *timings;
	mp->extra_info_dirty = true;
}

static void dss_mgr_set_timings_compat(struct omap_overlay_manager *mgr,
		const struct omap_video_timings *timings)
{
	unsigned long flags;
	struct mgr_priv_data *mp = get_mgr_priv(mgr);

	spin_lock_irqsave(&data_lock, flags);

	if (mp->updating) {
		DSSERR("cannot set timings for %s: manager needs to be disabled\n",
			mgr->name);
		goto out;
	}

	dss_apply_mgr_timings(mgr, timings);
out:
	spin_unlock_irqrestore(&data_lock, flags);
}

static void dss_apply_mgr_lcd_config(struct omap_overlay_manager *mgr,
		const struct dss_lcd_mgr_config *config)
{
	struct mgr_priv_data *mp = get_mgr_priv(mgr);

	mp->lcd_config = *config;
	mp->extra_info_dirty = true;
}

static void dss_mgr_set_lcd_config_compat(struct omap_overlay_manager *mgr,
		const struct dss_lcd_mgr_config *config)
{
	unsigned long flags;
	struct mgr_priv_data *mp = get_mgr_priv(mgr);

	spin_lock_irqsave(&data_lock, flags);

	if (mp->enabled) {
		DSSERR("cannot apply lcd config for %s: manager needs to be disabled\n",
			mgr->name);
		goto out;
	}

	dss_apply_mgr_lcd_config(mgr, config);
out:
	spin_unlock_irqrestore(&data_lock, flags);
}

static int dss_ovl_set_info(struct omap_overlay *ovl,
		struct omap_overlay_info *info)
{
	struct ovl_priv_data *op = get_ovl_priv(ovl);
	unsigned long flags;
	int r;

	r = dss_ovl_simple_check(ovl, info);
	if (r)
		return r;

	spin_lock_irqsave(&data_lock, flags);

	op->user_info = *info;
	op->user_info_dirty = true;

	spin_unlock_irqrestore(&data_lock, flags);

	return 0;
}

static void dss_ovl_get_info(struct omap_overlay *ovl,
		struct omap_overlay_info *info)
{
	struct ovl_priv_data *op = get_ovl_priv(ovl);
	unsigned long flags;

	spin_lock_irqsave(&data_lock, flags);

	*info = op->user_info;

	spin_unlock_irqrestore(&data_lock, flags);
}

static int dss_ovl_set_manager(struct omap_overlay *ovl,
		struct omap_overlay_manager *mgr)
{
	struct ovl_priv_data *op = get_ovl_priv(ovl);
	unsigned long flags;
	int r;

	if (!mgr)
		return -EINVAL;

	mutex_lock(&apply_lock);

	if (ovl->manager) {
		DSSERR("overlay '%s' already has a manager '%s'\n",
				ovl->name, ovl->manager->name);
		r = -EINVAL;
		goto err;
	}

	r = dispc_runtime_get();
	if (r)
		goto err;

	spin_lock_irqsave(&data_lock, flags);

	if (op->enabled) {
		spin_unlock_irqrestore(&data_lock, flags);
		DSSERR("overlay has to be disabled to change the manager\n");
		r = -EINVAL;
		goto err1;
	}

	dispc_ovl_set_channel_out(ovl->id, mgr->id);

	op->channel = mgr->id;

	ovl->manager = mgr;
	list_add_tail(&ovl->list, &mgr->overlays);

	spin_unlock_irqrestore(&data_lock, flags);

	dispc_runtime_put();

	mutex_unlock(&apply_lock);

	return 0;

err1:
	dispc_runtime_put();
err:
	mutex_unlock(&apply_lock);
	return r;
}

static int dss_ovl_unset_manager(struct omap_overlay *ovl)
{
	struct ovl_priv_data *op = get_ovl_priv(ovl);
	unsigned long flags;
	int r;

	mutex_lock(&apply_lock);

	if (!ovl->manager) {
		DSSERR("failed to detach overlay: manager not set\n");
		r = -EINVAL;
		goto err;
	}

	spin_lock_irqsave(&data_lock, flags);

	if (op->enabled) {
		spin_unlock_irqrestore(&data_lock, flags);
		DSSERR("overlay has to be disabled to unset the manager\n");
		r = -EINVAL;
		goto err;
	}

	spin_unlock_irqrestore(&data_lock, flags);

	/* wait for pending extra_info updates to ensure the ovl is disabled */
	wait_pending_extra_info_updates();

	/*
	 * For a manual update display, there is no guarantee that the overlay
	 * is really disabled in HW, we may need an extra update from this
	 * manager before the configurations can go in. Return an error if the
	 * overlay needed an update from the manager.
	 *
	 * TODO: Instead of returning an error, try to do a dummy manager update
	 * here to disable the overlay in hardware. Use the *GATED fields in
	 * the DISPC_CONFIG registers to do a dummy update.
	 */
	spin_lock_irqsave(&data_lock, flags);

	if (ovl_manual_update(ovl) && op->extra_info_dirty) {
		spin_unlock_irqrestore(&data_lock, flags);
		DSSERR("need an update to change the manager\n");
		r = -EINVAL;
		goto err;
	}

	ovl->manager = NULL;
	list_del(&ovl->list);

	spin_unlock_irqrestore(&data_lock, flags);

	mutex_unlock(&apply_lock);

	return 0;
err:
	mutex_unlock(&apply_lock);
	return r;
}

static bool dss_ovl_is_enabled(struct omap_overlay *ovl)
{
	struct ovl_priv_data *op = get_ovl_priv(ovl);
	unsigned long flags;
	bool e;

	spin_lock_irqsave(&data_lock, flags);

	e = op->enabled;

	spin_unlock_irqrestore(&data_lock, flags);

	return e;
}

int dss_mgr_set_ovls(struct omap_overlay_manager *mgr)
{
	unsigned long flags;
	int i;
	if (!mgr || !mgr->ovls) {
		DSSERR("null pointer\n");
		return -EINVAL;
	}
	if (mgr->num_ovls > dss_feat_get_num_ovls()) {
		DSSERR("Invalid number of overlays passed\n");
		return -EINVAL;
	}
	mutex_lock(&apply_lock);
	spin_lock_irqsave(&data_lock, flags);

	for (i = 0; i < mgr->num_ovls; i++) {
		if (mgr != mgr->ovls[i]->manager) {
			DSSERR("Invalid mgr for ovl#%d\n", mgr->ovls[i]->id);
			spin_unlock_irqrestore(&data_lock, flags);
			mutex_unlock(&apply_lock);
			return -EINVAL;
		}
		/* Enable the overlay */
		if (mgr->ovls[i]->enabled) {
			struct ovl_priv_data *op = get_ovl_priv(mgr->ovls[i]);
			op->enabling = true;
			dss_setup_fifos();
			op->enabling = false;
			dss_apply_ovl_enable(mgr->ovls[i], true);
		} else {
			dss_apply_ovl_enable(mgr->ovls[i], false);
		}
	}
	dss_write_regs();
	dss_set_go_bits();
	spin_unlock_irqrestore(&data_lock, flags);
	wait_pending_extra_info_updates();
	mutex_unlock(&apply_lock);
	return 0;
}

static int dss_ovl_enable(struct omap_overlay *ovl)
{
	struct ovl_priv_data *op = get_ovl_priv(ovl);
	unsigned long flags;
	int r;

	mutex_lock(&apply_lock);

	if (op->enabled) {
		r = 0;
		goto err1;
	}

	if (ovl->manager == NULL || ovl->manager->output == NULL) {
		r = -EINVAL;
		goto err1;
	}

	spin_lock_irqsave(&data_lock, flags);

	op->enabling = true;

	r = dss_check_settings(ovl->manager);
	if (r) {
		DSSERR("failed to enable overlay %d: check_settings failed\n",
				ovl->id);
		goto err2;
	}

	dss_setup_fifos();

	op->enabling = false;
	dss_apply_ovl_enable(ovl, true);

	dss_write_regs();
	dss_set_go_bits();

	spin_unlock_irqrestore(&data_lock, flags);

	mutex_unlock(&apply_lock);

	return 0;
err2:
	op->enabling = false;
	spin_unlock_irqrestore(&data_lock, flags);
err1:
	mutex_unlock(&apply_lock);
	return r;
}

static int dss_ovl_disable(struct omap_overlay *ovl)
{
	struct ovl_priv_data *op = get_ovl_priv(ovl);
	unsigned long flags;
	int r;

	mutex_lock(&apply_lock);

	if (!op->enabled) {
		r = 0;
		goto err;
	}

	if (ovl->manager == NULL || ovl->manager->output == NULL) {
		r = -EINVAL;
		goto err;
	}

	spin_lock_irqsave(&data_lock, flags);

	dss_apply_ovl_enable(ovl, false);
	dss_write_regs();
	dss_set_go_bits();

	spin_unlock_irqrestore(&data_lock, flags);

	mutex_unlock(&apply_lock);

	return 0;

err:
	mutex_unlock(&apply_lock);
	return r;
}

static int dss_mgr_register_framedone_handler_compat(struct omap_overlay_manager *mgr,
		void (*handler)(void *), void *data)
{
	struct mgr_priv_data *mp = get_mgr_priv(mgr);

	if (mp->framedone_handler)
		return -EBUSY;

	mp->framedone_handler = handler;
	mp->framedone_handler_data = data;

	return 0;
}

static void dss_mgr_unregister_framedone_handler_compat(struct omap_overlay_manager *mgr,
		void (*handler)(void *), void *data)
{
	struct mgr_priv_data *mp = get_mgr_priv(mgr);

	WARN_ON(mp->framedone_handler != handler ||
			mp->framedone_handler_data != data);

	mp->framedone_handler = NULL;
	mp->framedone_handler_data = NULL;
}

static const struct dss_mgr_ops apply_mgr_ops = {
	.start_update = dss_mgr_start_update_compat,
	.enable = dss_mgr_enable_compat,
	.disable = dss_mgr_disable_compat,
	.set_timings = dss_mgr_set_timings_compat,
	.set_lcd_config = dss_mgr_set_lcd_config_compat,
	.register_framedone_handler = dss_mgr_register_framedone_handler_compat,
	.unregister_framedone_handler = dss_mgr_unregister_framedone_handler_compat,
};

static int compat_refcnt;
static DEFINE_MUTEX(compat_init_lock);

int omapdss_compat_init(void)
{
	struct platform_device *pdev = dss_get_core_pdev();
	struct omap_dss_device *dssdev = NULL;
	int i, r;

	if(!pdev)
		return -ENODEV;

	mutex_lock(&compat_init_lock);

	if (compat_refcnt++ > 0)
		goto out;

	apply_init_priv();

	dss_init_overlay_managers(pdev);
	dss_init_overlays(pdev);

	for (i = 0; i < omap_dss_get_num_overlay_managers(); i++) {
		struct omap_overlay_manager *mgr;

		mgr = omap_dss_get_overlay_manager(i);

		mgr->set_output = &dss_mgr_set_output;
		mgr->unset_output = &dss_mgr_unset_output;
		mgr->apply = &omap_dss_mgr_apply;
		mgr->wb_apply = &omap_dss_wb_mgr_apply;
		mgr->set_manager_info = &dss_mgr_set_info;
		mgr->get_manager_info = &dss_mgr_get_info;
		mgr->wait_for_go = &dss_mgr_wait_for_go;
		mgr->wait_for_vsync = &dss_mgr_wait_for_vsync;
		mgr->set_ovl = &dss_mgr_set_ovls;
		mgr->get_device = &dss_mgr_get_device;
	}

	for (i = 0; i < omap_dss_get_num_overlays(); i++) {
		struct omap_overlay *ovl = omap_dss_get_overlay(i);

		ovl->is_enabled = &dss_ovl_is_enabled;
		ovl->enable = &dss_ovl_enable;
		ovl->disable = &dss_ovl_disable;
		ovl->set_manager = &dss_ovl_set_manager;
		ovl->unset_manager = &dss_ovl_unset_manager;
		ovl->set_overlay_info = &dss_ovl_set_info;
		ovl->get_overlay_info = &dss_ovl_get_info;
		ovl->wait_for_go = &dss_mgr_wait_for_go_ovl;
		ovl->get_device = &dss_ovl_get_device;
	}

	r = dss_install_mgr_ops(&apply_mgr_ops);
	if (r)
		goto err_mgr_ops;

	for_each_dss_dev(dssdev) {
		r = display_init_sysfs(pdev, dssdev);
		/* XXX uninit sysfs files on error */
		if (r)
			goto err_disp_sysfs;

		BLOCKING_INIT_NOTIFIER_HEAD(&dssdev->state_notifiers);
	}

	dispc_runtime_get();

	r = dss_dispc_initialize_irq();
	if (r)
		goto err_init_irq;

	dispc_runtime_put();

out:
	mutex_unlock(&compat_init_lock);

	return 0;

err_init_irq:
	dispc_runtime_put();

err_disp_sysfs:
	dss_uninstall_mgr_ops();

err_mgr_ops:
	dss_uninit_overlay_managers(pdev);
	dss_uninit_overlays(pdev);

	compat_refcnt--;

	mutex_unlock(&compat_init_lock);

	return r;
}
EXPORT_SYMBOL(omapdss_compat_init);

void omapdss_compat_uninit(void)
{
	struct platform_device *pdev = dss_get_core_pdev();
	struct omap_dss_device *dssdev = NULL;

	mutex_lock(&compat_init_lock);

	if (--compat_refcnt > 0)
		goto out;

	dss_dispc_uninitialize_irq();

	for_each_dss_dev(dssdev)
		display_uninit_sysfs(pdev, dssdev);

	dss_uninstall_mgr_ops();

	dss_uninit_overlay_managers(pdev);
	dss_uninit_overlays(pdev);
out:
	mutex_unlock(&compat_init_lock);
}
EXPORT_SYMBOL(omapdss_compat_uninit);
