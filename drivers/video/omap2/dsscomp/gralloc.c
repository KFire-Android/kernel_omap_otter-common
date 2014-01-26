/*
 * drivers/video/omap2/dsscomp/gralloc.c
 *
 * DSS Composition gralloc file
 *
 * Copyright (C) 2011 Texas Instruments, Inc
 * Author: Lajos Molnar <molnar@ti.com>
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
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/vmalloc.h>
#include "../../../drivers/gpu/drm/omapdrm/omap_dmm_tiler.h"
#include <video/dsscomp.h>
#include <plat/dsscomp.h>
#include "dsscomp.h"
#include "tiler-utils.h"

#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif
static bool blanked;
static bool presentation_mode;

#define NUM_TILER1D_SLOTS 2
#define MAX_NUM_TILER1D_SLOTS 4

static struct tiler1d_slot {
	struct list_head q;
	struct tiler_block *block_handle;
	u32 phys;
	u32 size;
	u32 *page_map;
	short id;
} slots[MAX_NUM_TILER1D_SLOTS];
static struct list_head free_slots;
static struct dsscomp_dev *cdev;
static DEFINE_MUTEX(mtx);
static struct semaphore free_slots_sem =
				__SEMAPHORE_INITIALIZER(free_slots_sem, 0);

/* gralloc composition sync object */
struct dsscomp_gralloc_t {
	void (*cb_fn)(void *, int);
	void *cb_arg;
	struct list_head q;
	struct list_head slots;
	atomic_t refs;
	bool early_callback;
	bool programmed;
};

/* queued gralloc compositions */
static LIST_HEAD(flip_queue);

static u32 ovl_use_mask[MAX_MANAGERS];

static inline bool needs_split(struct tiler1d_slot *slot)
{
	return slot->size == tiler1d_slot_size(cdev) >> PAGE_SHIFT;
}

static struct tiler1d_slot *split_slots(struct tiler1d_slot *slot);
static struct tiler1d_slot *merge_slots(struct tiler1d_slot *slot);
static struct tiler1d_slot *alloc_tiler_slot(void);

static void unpin_tiler_blocks(struct list_head *slots)
{
	struct tiler1d_slot *slot;

	/* unpin any tiler memory */
	list_for_each_entry(slot, slots, q) {
		tiler_unpin(slot->block_handle);
		up(&free_slots_sem);
	}

	/* free tiler slots */
	list_splice_init(slots, &free_slots);
}

static void dsscomp_gralloc_cb(void *data, int status)
{
	struct dsscomp_gralloc_t *gsync = data, *gsync_;
	bool early_cbs = true;
	LIST_HEAD(done);

	mutex_lock(&mtx);
	if (gsync->early_callback && status == DSS_COMPLETION_PROGRAMMED)
		gsync->programmed = true;

	if (status & DSS_COMPLETION_RELEASED) {
		if (atomic_dec_and_test(&gsync->refs))
			unpin_tiler_blocks(&gsync->slots);

		log_event(0, 0, gsync, "--refs=%d on %s",
				atomic_read(&gsync->refs),
				(u32) log_status_str(status));
	}

	/* get completed list items in order, if any */
	list_for_each_entry_safe(gsync, gsync_, &flip_queue, q) {
		if (gsync->cb_fn) {
			early_cbs &= gsync->early_callback && gsync->programmed;
			if (early_cbs) {
				gsync->cb_fn(gsync->cb_arg, 1);
				gsync->cb_fn = NULL;
			}
		}
		if (gsync->refs.counter && gsync->cb_fn)
			break;
		if (gsync->refs.counter == 0)
			list_move_tail(&gsync->q, &done);
	}
	mutex_unlock(&mtx);

	/* call back for completed composition with mutex unlocked */
	list_for_each_entry_safe(gsync, gsync_, &done, q) {
		if (debug & DEBUG_GRALLOC_PHASES)
			dev_info(DEV(cdev), "[%p] completed flip\n", gsync);

		log_event(0, 0, gsync, "calling %pf [%p]", (u32)gsync->cb_fn,
				(u32)gsync->cb_arg);

		if (gsync->cb_fn)
			gsync->cb_fn(gsync->cb_arg, 1);
		kfree(gsync);
	}
}

/* This is just test code for now that does the setup + apply.
   It still uses userspace virtual addresses, but maps non
   TILER buffers into 1D */
int dsscomp_gralloc_queue_ioctl(struct dsscomp_setup_dispc_data *d)
{
	struct tiler_pa_info *pas[MAX_OVERLAYS];
	s32 ret;
	u32 i;

	/* convert virtual addresses to physical and get tiler pa infos */
	for (i = 0; i < d->num_ovls; i++) {
		struct dss2_ovl_info *oi = d->ovls + i;
		u32 addr = (u32) oi->address;

		pas[i] = NULL;

		/* only supporting DIRECT buffer types */

		/* assume virtual NV12 for now */
		if (oi->cfg.color_mode == OMAP_DSS_COLOR_NV12)
			oi->uv = tiler_virt2phys(addr +
					oi->cfg.height * oi->cfg.stride);
		else
			oi->uv = 0;
		oi->ba = tiler_virt2phys(addr);

		/* map non-TILER buffers to 1D */
		if (oi->ba && !is_tiler_addr(oi->ba))
			pas[i] = user_block_to_pa(addr & PAGE_MASK,
				PAGE_ALIGN(oi->cfg.height * oi->cfg.stride +
					(addr & ~PAGE_MASK)) >> PAGE_SHIFT);
	}
	ret = dsscomp_gralloc_queue(d, pas, false, NULL, NULL);
	for (i = 0; i < d->num_ovls; i++)
		tiler_pa_free(pas[i]);
	return ret;
}

static bool dsscomp_is_any_device_active(void)
{
	struct omap_dss_device *dssdev;
	u32 display_ix;

	/* We have to also check for device active in case HWC logic has
	 * not yet started for boot logo.
	 */
	for (display_ix = 0 ; display_ix < cdev->num_displays ; display_ix++) {
		dssdev = cdev->displays[display_ix];
		if (dssdev && dssdev->state == OMAP_DSS_DISPLAY_ACTIVE)
			return true;
	}
	return false;
}

int dsscomp_gralloc_queue(struct dsscomp_setup_dispc_data *d,
			struct tiler_pa_info **pas,
			bool early_callback,
			void (*cb_fn)(void *, int), void *cb_arg)
{
	u32 i;
	int r = 0;
	struct omap_dss_device *dev;
	struct omap_overlay_manager *mgr;
	static DEFINE_MUTEX(local_mtx);
	struct dsscomp *comp[MAX_MANAGERS];
	u32 ovl_new_use_mask[MAX_MANAGERS];
	u32 mgr_set_mask = 0;
	u32 ovl_set_mask = 0;
	struct tiler1d_slot *slot = NULL;
	u32 slot_used = 0;
#ifdef CONFIG_DEBUG_FS
	u32 ms = ktime_to_ms(ktime_get());
#endif
	u32 channels[MAX_MANAGERS], ch;
	int skip;
	struct dsscomp_gralloc_t *gsync;
	struct dss2_rect_t win = { .w = 0 };

	/* reserve tiler areas if not already done so */
	dsscomp_gralloc_init(cdev);

	dump_total_comp_info(cdev, d, "queue");
	for (i = 0; i < d->num_ovls; i++)
		dump_ovl_info(cdev, d->ovls + i);

	mutex_lock(&local_mtx);

	mutex_lock(&mtx);

	/* create sync object with 1 temporary ref */
	gsync = kzalloc(sizeof(*gsync), GFP_KERNEL);
	gsync->cb_arg = cb_arg;
	gsync->cb_fn = cb_fn;
	gsync->refs.counter = 1;
	gsync->early_callback = early_callback;
	INIT_LIST_HEAD(&gsync->slots);
	list_add_tail(&gsync->q, &flip_queue);
	if (debug & DEBUG_GRALLOC_PHASES)
		dev_info(DEV(cdev), "[%p] queuing flip\n", gsync);

	log_event(0, ms, gsync, "new in %pf (refs=1)",
			(u32)dsscomp_gralloc_queue, 0);

	/* ignore frames while we are blanked */
	skip = blanked;
	if (skip && (debug & DEBUG_PHASES))
		dev_info(DEV(cdev), "[%p,%08x] ignored\n", gsync, d->sync_id);

	/* mark blank frame by NULL tiler pa pointer */
	if (!skip && pas == NULL)
		blanked = true;

	mutex_unlock(&mtx);

	d->num_mgrs = min_t(u16, d->num_mgrs, ARRAY_SIZE(d->mgrs));
	d->num_ovls = min_t(u16, d->num_ovls, ARRAY_SIZE(d->ovls));

	memset(comp, 0, sizeof(comp));
	memset(ovl_new_use_mask, 0, sizeof(ovl_new_use_mask));

	if (skip || !dsscomp_is_any_device_active())
		goto skip_comp;

	d->mode = DSSCOMP_SETUP_DISPLAY;

	/* mark managers we are using */
	for (i = 0; i < d->num_mgrs; i++) {
		/* verify display is valid & connected, ignore if not */
		if (d->mgrs[i].ix >= cdev->num_displays)
			continue;
		dev = cdev->displays[d->mgrs[i].ix];
		if (!dev) {
			dev_warn(DEV(cdev), "failed to get display%d\n",
					d->mgrs[i].ix);
			continue;
		}
		mgr = dev->output->manager;
		if (!mgr) {
			dev_warn(DEV(cdev), "no manager for display%d\n",
					d->mgrs[i].ix);
			continue;
		}
		ch = mgr->id;
		channels[i] = ch;
		mgr_set_mask |= 1 << ch;

		/* swap red & blue if requested */
		if (d->mgrs[i].swap_rb)
			swap_rb_in_mgr_info(d->mgrs + i);
	}

	/* create dsscomp objects for set managers (including active ones) */
	for (ch = 0; ch < MAX_MANAGERS; ch++) {
		if (!(mgr_set_mask & (1 << ch)))
			continue;

		mgr = cdev->mgrs[ch];

		comp[ch] = dsscomp_new(mgr);
		if (IS_ERR(comp[ch])) {
			comp[ch] = NULL;
			dev_warn(DEV(cdev), "failed to get composition on %s\n",
					mgr->name);
			continue;
		}

		comp[ch]->must_apply = true;
		r = dsscomp_setup(comp[ch], d->mode, win);
		if (r)
			dev_err(DEV(cdev), "failed to setup comp (%d)\n", r);
	}

	/* configure manager data from gralloc composition */
	for (i = 0; i < d->num_mgrs; i++) {
		ch = channels[i];
		r = dsscomp_set_mgr(comp[ch], d->mgrs + i);
		if (r)
			dev_err(DEV(cdev), "failed to set mgr%d (%d)\n", ch, r);
	}

	/* NOTE: none of the dsscomp sets should fail as composition is new */
	for (i = 0; i < d->num_ovls; i++) {
		struct dss2_ovl_info *oi = d->ovls + i;
		u32 size;
		int j;
		for (j = 0; j < d->num_mgrs; j++)
			if (d->mgrs[j].ix == oi->cfg.mgr_ix) {
				/* swap red & blue if requested */
				if (d->mgrs[j].swap_rb)
					swap_rb_in_ovl_info(d->ovls + i);
				break;
			}

		if (j == d->num_mgrs) {
			dev_err(DEV(cdev), "invalid manager %d for ovl%d\n",
						ch, oi->cfg.ix);
			continue;
		}

		/* skip overlays on compositions we could not create */
		ch = channels[j];
		if (!comp[ch])
			continue;
		/* copy prior overlay to avoid mapping layers twice to 1D */
		if (oi->addressing == OMAP_DSS_BUFADDR_OVL_IX) {
			unsigned int j = oi->ba;
			if (j >= i || !d->ovls[j].cfg.enabled) {
				WARN(1, "Invalid clone layer (%u)", j);
				goto skip_buffer;
			}

			oi->ba = d->ovls[j].ba;
			oi->uv = d->ovls[j].uv;
			goto skip_map1d;
		} else if (oi->addressing == OMAP_DSS_BUFADDR_FB) {
			/* get fb */
			int fb_ix = (oi->ba >> 28);
			int fb_uv_ix = (oi->uv >> 28);
			struct fb_info *fbi = NULL, *fbi_uv = NULL;
			size_t hs_size = oi->cfg.height * oi->cfg.stride;
			if (fb_ix >= num_registered_fb ||
			    (oi->cfg.color_mode == OMAP_DSS_COLOR_NV12 &&
			     fb_uv_ix >= num_registered_fb)) {
				WARN(1, "display has no framebuffer");
				goto skip_buffer;
			}

			fbi_uv = registered_fb[fb_ix];
			fbi = fbi_uv;
			if (oi->cfg.color_mode == OMAP_DSS_COLOR_NV12)
				fbi_uv = registered_fb[fb_uv_ix];

			if (hs_size + oi->ba > fbi->fix.smem_len ||
			    (oi->cfg.color_mode == OMAP_DSS_COLOR_NV12 &&
			     (hs_size >> 1) + oi->uv > fbi_uv->fix.smem_len)) {
				WARN(1, "image outside of framebuffer memory");
				goto skip_buffer;
			}

			oi->ba += fbi->fix.smem_start;
			oi->uv += fbi_uv->fix.smem_start;
			goto skip_map1d;
		} else if (oi->addressing != OMAP_DSS_BUFADDR_DIRECT) {
			goto skip_buffer;
		}

		/* map non-TILER buffers to 1D */

		/* skip 2D and disabled layers */
		if (!pas[i] || !oi->cfg.enabled)
			goto skip_map1d;

		if (!slot) {
			mutex_lock(&mtx);
			/* separate comp for tv means presentation mode */
			if (d->num_mgrs == 1 && d->mgrs[0].ix == 1)
				presentation_mode = true;
			else if (cdev->mgrs[1]->output && (d->num_mgrs == 2 ||
				cdev->mgrs[1]->output->device->state !=
					OMAP_DSS_DISPLAY_ACTIVE))
				presentation_mode = false;

			slot = alloc_tiler_slot();
			if (IS_ERR_OR_NULL(slot)) {
				dev_warn(DEV(cdev), "could not obtain "
							"tiler slot");
				slot = NULL;
				mutex_unlock(&mtx);
				goto skip_buffer;
			}
			list_move(&slot->q, &gsync->slots);
			mutex_unlock(&mtx);
		}

		size = oi->cfg.stride * oi->cfg.height;
		if (oi->cfg.color_mode == OMAP_DSS_COLOR_NV12)
			size += size >> 2;
		size = DIV_ROUND_UP(size, PAGE_SIZE);

		if (slot_used + size > slot->size) {
			dev_err(DEV(cdev), "tiler slot not big enough for "
					"frame %d + %d > %d", slot_used, size,
					slot->size);
			goto skip_buffer;
		}

		/* "map" into TILER 1D - will happen after loop */
		oi->ba = slot->phys + (slot_used << PAGE_SHIFT) +
			(oi->ba & ~PAGE_MASK);
		if (oi->cfg.color_mode == OMAP_DSS_COLOR_NV12)
			oi->uv = oi->ba + oi->cfg.stride * oi->cfg.height;
		memcpy(slot->page_map + slot_used, pas[i]->mem,
		       sizeof(*slot->page_map) * size);
		slot_used += size;
		goto skip_map1d;

skip_buffer:
		oi->cfg.enabled = false;
skip_map1d:

		if (oi->cfg.enabled)
			ovl_new_use_mask[ch] |= 1 << oi->cfg.ix;

		r = dsscomp_set_ovl(comp[ch], oi);
		if (r)
			dev_err(DEV(cdev), "failed to set ovl%d (%d)\n",
					oi->cfg.ix, r);
		else
			ovl_set_mask |= 1 << oi->cfg.ix;
	}

	if (slot && slot_used) {
		r = tiler_pin_phys(slot->block_handle, slot->page_map,
						slot_used);
		if (r)
			dev_err(DEV(cdev), "failed to pin %d pages into"
			" %d-pg slots (%d)\n", slot_used,
			tiler1d_slot_size(cdev) >> PAGE_SHIFT, r);
	}

	for (ch = 0; ch < MAX_MANAGERS; ch++) {
		/* disable all overlays not specifically set from prior frame */
		u32 mask = ovl_use_mask[ch] & ~ovl_set_mask;

		if (!comp[ch])
			continue;

		while (mask) {
			struct dss2_ovl_info oi = {
				.cfg.zonly = true,
				.cfg.enabled = false,
				.cfg.ix = fls(mask) - 1,
			};
			dsscomp_set_ovl(comp[ch], &oi);
			mask &= ~(1 << oi.cfg.ix);
		}

		/* associate dsscomp objects with this gralloc composition */
		comp[ch]->extra_cb = dsscomp_gralloc_cb;
		comp[ch]->extra_cb_data = gsync;
		atomic_inc(&gsync->refs);
		log_event(0, ms, gsync, "++refs=%d for [%p]",
				atomic_read(&gsync->refs), (u32) comp[ch]);

		r = dsscomp_delayed_apply(comp[ch]);
		if (r)
			dev_err(DEV(cdev), "failed to apply comp (%d)\n", r);
		else
			ovl_use_mask[ch] = ovl_new_use_mask[ch];
	}
skip_comp:
	/* release sync object ref - this completes unapplied compositions */
	dsscomp_gralloc_cb(gsync, DSS_COMPLETION_RELEASED);

	mutex_unlock(&local_mtx);

	return r;
}
EXPORT_SYMBOL(dsscomp_gralloc_queue);

#ifdef CONFIG_EARLYSUSPEND
static int blank_complete;
static DECLARE_WAIT_QUEUE_HEAD(early_suspend_wq);

static void dsscomp_early_suspend_cb(void *data, int status)
{
	blank_complete = true;
	wake_up(&early_suspend_wq);
}

static void dsscomp_early_suspend(struct early_suspend *h)
{
	struct dsscomp_setup_dispc_data d = {
		.num_mgrs = 0,
	};

	int err, mgr_ix;

	pr_info("DSSCOMP: %s\n", __func__);

	/*dsscomp_gralloc_queue() expects all blanking mgrs set up in comp */
	for (mgr_ix = 0 ; mgr_ix < cdev->num_mgrs ; mgr_ix++) {
		struct omap_dss_device *dssdev = cdev->mgrs[mgr_ix]->device;
		if (dssdev && dssdev->state == OMAP_DSS_DISPLAY_ACTIVE) {
			d.num_mgrs++;
			d.mgrs[mgr_ix].ix = mgr_ix;
			d.mgrs[mgr_ix].alpha_blending = true;
		}
	}

	/* use gralloc queue as we need to blank all screens */
	blank_complete = false;
	dsscomp_gralloc_queue(&d, NULL, true, dsscomp_early_suspend_cb, NULL);

	/* wait until composition is displayed */
	err = wait_event_timeout(early_suspend_wq, blank_complete,
				 msecs_to_jiffies(500));
	if (err == 0)
		pr_warn("DSSCOMP: timeout blanking screen\n");
	else
		pr_info("DSSCOMP: blanked screen\n");
}

static void dsscomp_late_resume(struct early_suspend *h)
{
	pr_info("DSSCOMP: %s\n", __func__);
	blanked = false;
}

static struct early_suspend early_suspend_info = {
	.suspend = dsscomp_early_suspend,
	.resume = dsscomp_late_resume,
	.level = EARLY_SUSPEND_LEVEL_DISABLE_FB,
};
#endif

void dsscomp_dbg_gralloc(struct seq_file *s)
{
#ifdef CONFIG_DEBUG_FS
	struct dsscomp_gralloc_t *g;
	struct tiler1d_slot *t;
	struct dsscomp *c;
#ifdef CONFIG_DSSCOMP_DEBUG_LOG
		int i;
#endif

	mutex_lock(&dbg_mtx);
	seq_printf(s, "ACTIVE GRALLOC FLIPS\n\n");
	list_for_each_entry(g, &flip_queue, q) {
		char *sep = "";
		seq_printf(s, "  [%p] (refs=%d)\n"
			   "    slots=[", g, atomic_read(&g->refs));
		list_for_each_entry(t, &g->slots, q) {
			seq_printf(s, "%s%08x", sep, t->phys);
			sep = ", ";
		}
		seq_printf(s, "]\n    cmdcb=[%08x] ", (u32) g->cb_arg);
		if (g->cb_fn)
			seq_printf(s, "%pf\n\n  ", g->cb_fn);
		else
			seq_printf(s, "(called)\n\n  ");

		list_for_each_entry(c, &dbg_comps, dbg_q) {
			if (c->extra_cb && c->extra_cb_data == g)
				seq_printf(s, "|      %8s      ",
						cdev->mgrs[c->ix]->name);
		}
		seq_printf(s, "\n  ");
		list_for_each_entry(c, &dbg_comps, dbg_q) {
			if (c->extra_cb && c->extra_cb_data == g)
				seq_printf(s, "| [%08x] %7s ", (u32) c,
					   log_state_str(c->state));
		}
#ifdef CONFIG_DSSCOMP_DEBUG_LOG
		for (i = 0; i < ARRAY_SIZE(c->dbg_log); i++) {
			int go = false;
			seq_printf(s, "\n  ");
			list_for_each_entry(c, &dbg_comps, dbg_q) {
				if (!c->extra_cb || c->extra_cb_data != g)
					continue;
				if (i < c->dbg_used) {
					u32 dbg_t = c->dbg_log[i].t;
					u32 state = c->dbg_log[i].state;
					seq_printf(s, "| % 6d.%03d %7s ",
							dbg_t / 1000,
							dbg_t % 1000,
							log_state_str(state));
					go |= c->dbg_used > i + 1;
				} else {
					seq_printf(s, "%-21s", "|");
				}
			}
			if (!go)
				break;
		}
#endif
		seq_printf(s, "\n\n");
	}
	seq_printf(s, "\n");
	mutex_unlock(&dbg_mtx);
#endif
}

void dsscomp_gralloc_init(struct dsscomp_dev *cdev_)
{
	int i;

	if (!cdev_)
		return;

	/* save at least cdev pointer */
	if (!cdev) {
		cdev = cdev_;

#ifdef CONFIG_HAS_EARLYSUSPEND
		register_early_suspend(&early_suspend_info);
#endif
	}

	if (!free_slots.next) {
		INIT_LIST_HEAD(&free_slots);
		for (i = 0; i < MAX_NUM_TILER1D_SLOTS; i++)
			slots[i].id = -1;

		for (i = 0; i < NUM_TILER1D_SLOTS; i++) {
			struct tiler_block *block_handle =
				tiler_reserve_1d(tiler1d_slot_size(cdev_));
			if (IS_ERR_OR_NULL(block_handle)) {
				pr_err("could not allocate tiler block\n");
				break;
			}
			slots[i].block_handle = block_handle;
			slots[i].phys = tiler_ssptr(block_handle);
			slots[i].size =  tiler1d_slot_size(cdev_) >> PAGE_SHIFT;
			slots[i].page_map = vmalloc(sizeof(*slots[i].page_map) *
						slots[i].size);
			if (!slots[i].page_map) {
				pr_err("could not allocate page_map\n");
				tiler_unpin(block_handle);
				break;
			}
			slots[i].id = i;
			list_add(&slots[i].q, &free_slots);
			up(&free_slots_sem);
		}
		/* reset free_slots if no TILER memory could be reserved */
		if (!i)
			ZERO(free_slots);
	}
}

static struct tiler1d_slot *alloc_tiler_slot(void)
{
	struct tiler1d_slot *slot, *ret;
	if (down_timeout(&free_slots_sem,
			msecs_to_jiffies(100))) {
		return ERR_PTR(-ETIME);
	}
	slot = list_first_entry(&free_slots, typeof(*slot), q);
	if (presentation_mode && needs_split(slot)) {
		ret = split_slots(slot);
		if (IS_ERR_OR_NULL(ret))
			goto err;
		dev_dbg(DEV(cdev),
			"slot split, size %u block 0x%x\n",
			slot->size, slot->phys);
	} else if (!presentation_mode && !needs_split(slot)) {
		ret = merge_slots(slot);
		if (IS_ERR_OR_NULL(ret))
			goto err;
		dev_dbg(DEV(cdev),
			"slot merged, size %u ptr 0x%x\n",
			slot->size, slot->phys);
	}

	return slot;
err:
	up(&free_slots_sem);
	return ret;
}

static struct tiler1d_slot *merge_slots(struct tiler1d_slot *slot)
{
	struct tiler1d_slot *slot2free;
	u32 new_size = tiler1d_slot_size(cdev);

	list_for_each_entry(slot2free, &free_slots, q)
		if (!needs_split(slot2free) && slot2free != slot)
			break;

	if (&slot2free->q == &free_slots || slot2free->id == -1) {
		dev_err(DEV(cdev), "%s: no free slot to megre\n", __func__);
		return ERR_PTR(-EINVAL);
	}

	down(&free_slots_sem);
	list_del(&slot2free->q);

	dev_dbg(DEV(cdev), "%s: merging with %d id\n", __func__,
						slot2free->id);
	/*FIXME: potentially unsafe, as tiler1d space
	 * might be overtaken before we claim it again.
	 * Will be fixed later with tiler slot splitting API
	*/
	tiler_release(slot->block_handle);

	tiler_release(slot2free->block_handle);

	slot->size = new_size >> PAGE_SHIFT;
	slot->block_handle = tiler_reserve_1d(new_size);

	if (IS_ERR_OR_NULL(slot->block_handle)) {
		dev_err(DEV(cdev), "%s: failed to allocate slot\n", __func__);
		return ERR_PTR(-ENOMEM);
	}

	slot->phys = tiler_ssptr(slot->block_handle);

	vfree(slot2free->page_map);
	slot2free->id = -1;

	return slot;
}

static struct tiler1d_slot *split_slots(struct tiler1d_slot *slot)
{
	int i;
	u32 new_size = tiler1d_slot_size(cdev)/2;

	/*FIXME: potentially unsafe, as tiler1d space
	 * might be overtaken before we claim it again.
	 * Will be fixed later with tiler slot splitting API
	*/
	tiler_release(slot->block_handle);
	for (i = 0; i < MAX_NUM_TILER1D_SLOTS; i++)
		if (slots[i].id == -1)
			break;

	if (i == MAX_NUM_TILER1D_SLOTS) {
		dev_err(DEV(cdev), "%s: all slots allocated\n", __func__);
		return ERR_PTR(-EINVAL);
	}

	dev_dbg(DEV(cdev), "%s: splitting to %d id\n", __func__, i);
	slot->size = slots[i].size = new_size >> PAGE_SHIFT;
	slots[i].page_map = vmalloc(sizeof(*slots[i].page_map) *
				slots[i].size*2);
	slot->block_handle = tiler_reserve_1d(new_size);
	slots[i].block_handle = tiler_reserve_1d(new_size);

	if (IS_ERR_OR_NULL(slot->block_handle) ||
		IS_ERR_OR_NULL(slots[i].block_handle)) {
		dev_err(DEV(cdev), "%s: failed to allocate slot\n", __func__);
		return ERR_PTR(-ENOMEM);
	}

	slot->phys = tiler_ssptr(slot->block_handle);
	slots[i].phys = tiler_ssptr(slots[i].block_handle);
	slots[i].id = i;
	list_add(&slots[i].q, &free_slots);
	up(&free_slots_sem);

	return &slots[i];
}

void dsscomp_gralloc_exit(void)
{
	struct tiler1d_slot *slot;

#ifdef CONFIG_HAS_EARLYSUSPEND
	unregister_early_suspend(&early_suspend_info);
#endif

	list_for_each_entry(slot, &free_slots, q) {
		vfree(slot->page_map);
		tiler_unpin(slot->block_handle);
	}
	INIT_LIST_HEAD(&free_slots);
}
