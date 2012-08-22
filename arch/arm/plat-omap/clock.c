/*
 *  linux/arch/arm/plat-omap/clock.c
 *
 *  Copyright (C) 2004 - 2008 Nokia corporation
 *  Written by Tuukka Tikkanen <tuukka.tikkanen@elektrobit.com>
 *
 *  Modified for omap shared clock framework by Tony Lindgren <tony@atomide.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/string.h>
#include <linux/clk.h>
#include <linux/mutex.h>
#include <linux/cpufreq.h>
#include <linux/notifier.h>
#include <linux/debugfs.h>
#include <linux/io.h>
#include <linux/slab.h>

#include <plat/clock.h>

static LIST_HEAD(clocks);
static LIST_HEAD(clk_notifier_list);
static DEFINE_MUTEX(clocks_mutex);
static DEFINE_SPINLOCK(clockfw_lock);

static struct clk_functions *arch_clock;

/**
 * omap_clk_notify - call clk notifier chain
 * @clk: struct clk * that is changing rate
 * @msg: clk notifier type (i.e., CLK_POST_RATE_CHANGE; see mach/clock.h)
 * @old_rate: old rate
 * @new_rate: new rate
 *
 * Triggers a notifier call chain on the post-clk-rate-change notifier
 * for clock 'clk'.  Passes a pointer to the struct clk and the
 * previous and current rates to the notifier callback.  Intended to be
 * called by internal clock code only.  No return value.
 */
static void omap_clk_notify(struct clk *clk, unsigned long msg,
		unsigned long old_rate, unsigned long new_rate)
{
	struct clk_notifier *cn;
	struct clk_notifier_data cnd;

	cnd.clk = clk;
	cnd.old_rate = old_rate;
	cnd.new_rate = new_rate;

	list_for_each_entry(cn, &clk_notifier_list, node) {
		if (cn->clk == clk) {
			blocking_notifier_call_chain(&cn->notifier_head, msg,
					&cnd);
			break;
		}
	}
}

/**
 * omap_clk_notify_downstream - trigger clock change notifications
 * @clk: struct clk * to start the notifications with
 * @msg: notifier msg - see "Clk notifier callback types"
 * @new_rate: new rate
 *
 * Call clock change notifiers on clocks starting with @clk and including
 * all of @clk's downstream children clocks.  Returns NOTIFY_DONE.
 */
static int omap_clk_notify_downstream(struct clk *clk, unsigned long msg,
		unsigned long new_rate)
{
	unsigned long new_child_rate;
	struct clk *child_clk;

	/* send out notification for this clock */
	if (clk->notifier_count)
		omap_clk_notify(clk, msg, clk->rate, new_rate);

	list_for_each_entry(child_clk, &clk->children, sibling) {
		if (child_clk->speculate) {
			new_child_rate =
				child_clk->speculate(child_clk, new_rate);
			omap_clk_notify_downstream(child_clk, msg,
					new_child_rate);
		}
	}

	return NOTIFY_DONE;
}

/*
 * Standard clock functions defined in include/linux/clk.h
 */

int clk_enable(struct clk *clk)
{
	unsigned long flags;
	int ret;

	if (clk == NULL || IS_ERR(clk))
		return -EINVAL;

	if (!arch_clock || !arch_clock->clk_enable)
		return -EINVAL;

	spin_lock_irqsave(&clockfw_lock, flags);
	ret = arch_clock->clk_enable(clk);
	spin_unlock_irqrestore(&clockfw_lock, flags);

	return ret;
}
EXPORT_SYMBOL(clk_enable);

void clk_disable(struct clk *clk)
{
	unsigned long flags;

	if (clk == NULL || IS_ERR(clk))
		return;

	if (!arch_clock || !arch_clock->clk_disable)
		return;

	spin_lock_irqsave(&clockfw_lock, flags);
	if (clk->usecount == 0) {
		pr_err("Trying disable clock %s with 0 usecount\n",
		       clk->name);
		WARN_ON(1);
		goto out;
	}

	arch_clock->clk_disable(clk);

out:
	spin_unlock_irqrestore(&clockfw_lock, flags);
}
EXPORT_SYMBOL(clk_disable);

unsigned long clk_get_rate(struct clk *clk)
{
	unsigned long flags;
	unsigned long ret;

	if (clk == NULL || IS_ERR(clk))
		return 0;

	spin_lock_irqsave(&clockfw_lock, flags);
	ret = clk->rate;
	spin_unlock_irqrestore(&clockfw_lock, flags);

	return ret;
}
EXPORT_SYMBOL(clk_get_rate);

/*
 * Optional clock functions defined in include/linux/clk.h
 */

long clk_round_rate(struct clk *clk, unsigned long rate)
{
	unsigned long flags;
	long ret;

	if (clk == NULL || IS_ERR(clk))
		return 0;

	if (!arch_clock || !arch_clock->clk_round_rate)
		return 0;

	spin_lock_irqsave(&clockfw_lock, flags);
	ret = arch_clock->clk_round_rate(clk, rate);
	spin_unlock_irqrestore(&clockfw_lock, flags);

	return ret;
}
EXPORT_SYMBOL(clk_round_rate);

int clk_set_rate(struct clk *clk, unsigned long rate)
{
	unsigned long flags;
	unsigned long new_rate;
	unsigned long msg;
	int ret = -EINVAL;

	if (clk == NULL || IS_ERR(clk))
		return ret;

	if (!arch_clock || !arch_clock->clk_set_rate)
		return ret;

	if (clk->round_rate)
		new_rate = clk->round_rate(clk, rate);
	else
		new_rate = rate;

	omap_clk_notify_downstream(clk, CLK_PRE_RATE_CHANGE, new_rate);

	spin_lock_irqsave(&clockfw_lock, flags);
	ret = arch_clock->clk_set_rate(clk, rate);
	if (ret == 0)
		propagate_rate(clk);
	spin_unlock_irqrestore(&clockfw_lock, flags);

	msg = (ret) ? CLK_ABORT_RATE_CHANGE : CLK_POST_RATE_CHANGE;
	omap_clk_notify_downstream(clk, msg, clk->rate);

	return ret;
}
EXPORT_SYMBOL(clk_set_rate);

int clk_set_parent(struct clk *clk, struct clk *parent)
{
	unsigned long flags;
	unsigned long new_rate;
	unsigned long msg;
	int ret = -EINVAL;

	if (clk == NULL || IS_ERR(clk) || parent == NULL || IS_ERR(parent))
		return ret;

	if (!arch_clock || !arch_clock->clk_set_parent)
		return ret;

	new_rate = arch_clock->clk_round_rate_parent(clk, parent);
	omap_clk_notify_downstream(clk, CLK_PRE_RATE_CHANGE, new_rate);

	spin_lock_irqsave(&clockfw_lock, flags);
	if (clk->usecount == 0) {
		ret = arch_clock->clk_set_parent(clk, parent);
		if (ret == 0)
			propagate_rate(clk);
	} else
		ret = -EBUSY;
	spin_unlock_irqrestore(&clockfw_lock, flags);

	msg = (ret) ? CLK_ABORT_RATE_CHANGE : CLK_POST_RATE_CHANGE;
	omap_clk_notify_downstream(clk, msg, clk->rate);

	return ret;
}
EXPORT_SYMBOL(clk_set_parent);

struct clk *clk_get_parent(struct clk *clk)
{
	return clk->parent;
}
EXPORT_SYMBOL(clk_get_parent);

/*
 * OMAP specific clock functions shared between omap1 and omap2
 */

int __initdata mpurate;

/*
 * By default we use the rate set by the bootloader.
 * You can override this with mpurate= cmdline option.
 */
static int __init omap_clk_setup(char *str)
{
	get_option(&str, &mpurate);

	if (!mpurate)
		return 1;

	if (mpurate < 1000)
		mpurate *= 1000000;

	return 1;
}
__setup("mpurate=", omap_clk_setup);

/* Used for clocks that always have same value as the parent clock */
unsigned long followparent_recalc(struct clk *clk)
{
	return clk->parent->rate;
}

/*
 * Used for clocks that have the same value as the parent clock,
 * divided by some factor
 */
unsigned long omap_fixed_divisor_recalc(struct clk *clk)
{
	WARN_ON(!clk->fixed_div);

	return clk->parent->rate / clk->fixed_div;
}

void clk_reparent(struct clk *child, struct clk *parent)
{
	list_del_init(&child->sibling);
	if (parent)
		list_add(&child->sibling, &parent->children);
	child->parent = parent;

	/* now do the debugfs renaming to reattach the child
	   to the proper parent */
}

/* Propagate rate to children */
void propagate_rate(struct clk *tclk)
{
	struct clk *clkp;

	list_for_each_entry(clkp, &tclk->children, sibling) {
		if (clkp->recalc)
			clkp->rate = clkp->recalc(clkp);
		propagate_rate(clkp);
	}
}

static LIST_HEAD(root_clks);

/**
 * recalculate_root_clocks - recalculate and propagate all root clocks
 *
 * Recalculates all root clocks (clocks with no parent), which if the
 * clock's .recalc is set correctly, should also propagate their rates.
 * Called at init.
 */
void recalculate_root_clocks(void)
{
	struct clk *clkp;

	list_for_each_entry(clkp, &root_clks, sibling) {
		if (clkp->recalc)
			clkp->rate = clkp->recalc(clkp);
		propagate_rate(clkp);
	}
}

/**
 * clk_preinit - initialize any fields in the struct clk before clk init
 * @clk: struct clk * to initialize
 *
 * Initialize any struct clk fields needed before normal clk initialization
 * can run.  No return value.
 */
void clk_preinit(struct clk *clk)
{
	INIT_LIST_HEAD(&clk->children);
}

int clk_register(struct clk *clk)
{
	if (clk == NULL || IS_ERR(clk))
		return -EINVAL;

	/*
	 * trap out already registered clocks
	 */
	if (clk->node.next || clk->node.prev)
		return 0;

	mutex_lock(&clocks_mutex);
	if (clk->parent)
		list_add(&clk->sibling, &clk->parent->children);
	else
		list_add(&clk->sibling, &root_clks);

	list_add(&clk->node, &clocks);
	if (clk->init)
		clk->init(clk);
	mutex_unlock(&clocks_mutex);

	return 0;
}
EXPORT_SYMBOL(clk_register);

void clk_unregister(struct clk *clk)
{
	if (clk == NULL || IS_ERR(clk))
		return;

	mutex_lock(&clocks_mutex);
	list_del(&clk->sibling);
	list_del(&clk->node);
	mutex_unlock(&clocks_mutex);
}
EXPORT_SYMBOL(clk_unregister);

void clk_enable_init_clocks(void)
{
	struct clk *clkp;

	list_for_each_entry(clkp, &clocks, node) {
		if (clkp->flags & ENABLE_ON_INIT)
			clk_enable(clkp);
	}
}

/**
 * omap_clk_get_by_name - locate OMAP struct clk by its name
 * @name: name of the struct clk to locate
 *
 * Locate an OMAP struct clk by its name.  Assumes that struct clk
 * names are unique.  Returns NULL if not found or a pointer to the
 * struct clk if found.
 */
struct clk *omap_clk_get_by_name(const char *name)
{
	struct clk *c;
	struct clk *ret = NULL;

	mutex_lock(&clocks_mutex);

	list_for_each_entry(c, &clocks, node) {
		if (!strcmp(c->name, name)) {
			ret = c;
			break;
		}
	}

	mutex_unlock(&clocks_mutex);

	return ret;
}

int omap_clk_enable_autoidle_all(void)
{
	struct clk *c;
	unsigned long flags;

	spin_lock_irqsave(&clockfw_lock, flags);

	list_for_each_entry(c, &clocks, node)
		if (c->ops->allow_idle)
			c->ops->allow_idle(c);

	spin_unlock_irqrestore(&clockfw_lock, flags);

	return 0;
}

int omap_clk_disable_autoidle_all(void)
{
	struct clk *c;
	unsigned long flags;

	spin_lock_irqsave(&clockfw_lock, flags);

	list_for_each_entry(c, &clocks, node)
		if (c->ops->deny_idle)
			c->ops->deny_idle(c);

	spin_unlock_irqrestore(&clockfw_lock, flags);

	return 0;
}

/*
 * clock notifiers
 */

/**
 * clk_notifier_register - add a clock parameter change notifier
 * @clk: struct clk * to watch
 * @nb: struct notifier_block * with callback info
 *
 * Request notification for changes to the clock 'clk'.  This uses a
 * blocking notifier.  Callback code must not call back into the clock
 * framework.  Pre-notifier callbacks will be passed the previous and
 * new rate of the clock.
 *
 * clk_notifier_register() must be called from process
 * context.  Returns -EINVAL if called with null arguments, -ENOMEM
 * upon allocation failure; otherwise, passes along the return value
 * of blocking_notifier_chain_register().
 */
int clk_notifier_register(struct clk *clk, struct notifier_block *nb)
{
	struct clk_notifier *cn = NULL, *cn_new = NULL;
	int r;
	unsigned long flags;
	struct clk *clkp;

	if (!clk || !nb)
		return -EINVAL;

	/* Allocate this here speculatively to avoid GFP_ATOMIC */
	cn_new = kzalloc(sizeof(struct clk_notifier), GFP_KERNEL);
	if (!cn_new)
		return -ENOMEM;

	spin_lock_irqsave(&clockfw_lock, flags);

	list_for_each_entry(cn, &clk_notifier_list, node)
		if (cn->clk == clk)
			break;

	if (cn->clk != clk) {
		cn_new->clk = clk;
		BLOCKING_INIT_NOTIFIER_HEAD(&cn_new->notifier_head);

		list_add(&cn_new->node, &clk_notifier_list);
		cn = cn_new;
	} else {
		kfree(cn_new); /* didn't need it after all */
	}

	r = blocking_notifier_chain_register(&cn->notifier_head, nb);
	if (!r) {
		clkp = clk;
		do {
			clkp->notifier_count++;
		} while ((clkp = clkp->parent));
	}

	spin_unlock_irqrestore(&clockfw_lock, flags);

	return r;
}

/**
 * clk_notifier_unregister - remove a clock change notifier
 * @clk: struct clk *
 * @nb: struct notifier_block * with callback info
 *
 * Request no further notification for changes to clock 'clk'.  This
 * function presently does not release memory allocated by its
 * corresponding _register function; see inline comments for more
 * information.  Returns -EINVAL if called with null arguments;
 * otherwise, passes along the return value of
 * blocking_notifier_chain_unregister().
 */
int clk_notifier_unregister(struct clk *clk, struct notifier_block *nb)
{
	struct clk_notifier *cn = NULL;
	struct clk *clkp;
	int r = -EINVAL;
	unsigned long flags;

	if (!clk || !nb)
		return -EINVAL;

	spin_lock_irqsave(&clockfw_lock, flags);

	list_for_each_entry(cn, &clk_notifier_list, node)
		if (cn->clk == clk)
			break;

	if (cn->clk == clk) {
		r = blocking_notifier_chain_unregister(&cn->notifier_head, nb);

		if (!r) {
			clkp = clk;
			do {
				clkp->notifier_count--;
			} while ((clkp = clkp->parent));
		}

		/*
		 * XXX ugh, layering violation.  there should be some
		 * support in the notifier code for this.
		 */
		if (!cn->notifier_head.head)
			kfree(cn);

	} else {
		r = -ENOENT;
	}

	spin_unlock_irqrestore(&clockfw_lock, flags);

	return r;
}

/*
 * Low level helpers
 */
static int clkll_enable_null(struct clk *clk)
{
	return 0;
}

static void clkll_disable_null(struct clk *clk)
{
}

const struct clkops clkops_null = {
	.enable		= clkll_enable_null,
	.disable	= clkll_disable_null,
};

/*
 * Dummy functions used for virtual dummy clocks
 */
long clk_dummy_round_rate(struct clk *clk, unsigned long rate)
{
	return rate;
};

int clk_dummy_set_rate(struct clk *clk, unsigned long rate)
{
	clk->rate = rate;
	return 0;
};

/*
 * Dummy clock
 *
 * Used for clock aliases that are needed on some OMAPs, but not others
 */
struct clk dummy_ck = {
	.name	= "dummy",
	.ops	= &clkops_null,
};

#ifdef CONFIG_CPU_FREQ
void clk_init_cpufreq_table(struct cpufreq_frequency_table **table)
{
	unsigned long flags;

	if (!arch_clock || !arch_clock->clk_init_cpufreq_table)
		return;

	spin_lock_irqsave(&clockfw_lock, flags);
	arch_clock->clk_init_cpufreq_table(table);
	spin_unlock_irqrestore(&clockfw_lock, flags);
}

void clk_exit_cpufreq_table(struct cpufreq_frequency_table **table)
{
	unsigned long flags;

	if (!arch_clock || !arch_clock->clk_exit_cpufreq_table)
		return;

	spin_lock_irqsave(&clockfw_lock, flags);
	arch_clock->clk_exit_cpufreq_table(table);
	spin_unlock_irqrestore(&clockfw_lock, flags);
}
#endif

/*
 *
 */

#ifdef CONFIG_OMAP_RESET_CLOCKS
/*
 * Disable any unused clocks left on by the bootloader
 */
static int __init clk_disable_unused(void)
{
	struct clk *ck;
	unsigned long flags;

	if (!arch_clock || !arch_clock->clk_disable_unused)
		return 0;

	pr_info("clock: disabling unused clocks to save power\n");

	spin_lock_irqsave(&clockfw_lock, flags);
	list_for_each_entry(ck, &clocks, node) {
		if (ck->ops == &clkops_null)
			continue;

		if (ck->usecount > 0 || !ck->enable_reg)
			continue;

		arch_clock->clk_disable_unused(ck);
	}
	spin_unlock_irqrestore(&clockfw_lock, flags);

	return 0;
}
late_initcall(clk_disable_unused);
late_initcall(omap_clk_enable_autoidle_all);
#endif

int __init clk_init(struct clk_functions * custom_clocks)
{
	if (!custom_clocks) {
		pr_err("No custom clock functions registered\n");
		BUG();
	}

	arch_clock = custom_clocks;

	return 0;
}

#if defined(CONFIG_PM_DEBUG) && defined(CONFIG_DEBUG_FS)
/*
 *	debugfs support to trace clock tree hierarchy and attributes
 */

#include <linux/debugfs.h>
#include <linux/seq_file.h>

static struct dentry *clk_debugfs_root;

static int clk_dbg_show_summary(struct seq_file *s, void *unused)
{
	struct clk *c;
	struct clk *pa;

	seq_printf(s, "%-30s %-30s %-10s %s\n",
		"clock-name", "parent-name", "rate", "use-count");

	mutex_lock(&clocks_mutex);
	list_for_each_entry(c, &clocks, node) {
		pa = c->parent;
		seq_printf(s, "%-30s %-30s %-10lu %d\n",
			c->name, pa ? pa->name : "none", c->rate, c->usecount);
	}

	mutex_unlock(&clocks_mutex);
	return 0;
}

static int clk_dbg_open(struct inode *inode, struct file *file)
{
	return single_open(file, clk_dbg_show_summary, inode->i_private);
}

static const struct file_operations debug_clock_fops = {
	.open           = clk_dbg_open,
	.read           = seq_read,
	.llseek         = seq_lseek,
	.release        = single_release,
};

static int clk_debugfs_register_one(struct clk *c)
{
	int err;
	struct dentry *d, *child, *child_tmp;
	struct clk *pa = c->parent;
	char s[255];
	char *p = s;

	p += sprintf(p, "%s", c->name);
	d = debugfs_create_dir(s, pa ? pa->dent : clk_debugfs_root);
	if (!d)
		return -ENOMEM;
	c->dent = d;

	d = debugfs_create_u8("usecount", S_IRUGO, c->dent, (u8 *)&c->usecount);
	if (!d) {
		err = -ENOMEM;
		goto err_out;
	}
	d = debugfs_create_u32("rate", S_IRUGO, c->dent, (u32 *)&c->rate);
	if (!d) {
		err = -ENOMEM;
		goto err_out;
	}
	d = debugfs_create_x32("flags", S_IRUGO, c->dent, (u32 *)&c->flags);
	if (!d) {
		err = -ENOMEM;
		goto err_out;
	}
	return 0;

err_out:
	d = c->dent;
	list_for_each_entry_safe(child, child_tmp, &d->d_subdirs, d_u.d_child)
		debugfs_remove(child);
	debugfs_remove(c->dent);
	return err;
}

static int clk_debugfs_register(struct clk *c)
{
	int err;
	struct clk *pa = c->parent;

	if (pa && !pa->dent) {
		err = clk_debugfs_register(pa);
		if (err)
			return err;
	}

	if (!c->dent) {
		err = clk_debugfs_register_one(c);
		if (err)
			return err;
	}
	return 0;
}

static int __init clk_debugfs_init(void)
{
	struct clk *c;
	struct dentry *d;
	int err;

	d = debugfs_create_dir("clock", NULL);
	if (!d)
		return -ENOMEM;
	clk_debugfs_root = d;

	list_for_each_entry(c, &clocks, node) {
		err = clk_debugfs_register(c);
		if (err)
			goto err_out;
	}

	d = debugfs_create_file("summary", S_IRUGO,
		d, NULL, &debug_clock_fops);
	if (!d)
		return -ENOMEM;

	return 0;
err_out:
	debugfs_remove_recursive(clk_debugfs_root);
	return err;
}
late_initcall(clk_debugfs_init);

#endif /* defined(CONFIG_PM_DEBUG) && defined(CONFIG_DEBUG_FS) */
