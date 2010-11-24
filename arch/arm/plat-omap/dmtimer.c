/*
 * linux/arch/arm/plat-omap/dmtimer.c
 *
 * OMAP Dual-Mode Timers
 *
 * Copyright (C) 2005 Nokia Corporation
 * OMAP2 support by Juha Yrjola
 * API improvements and OMAP2 clock framework support by Timo Teras
 *
 * Copyright (C) 2009 Texas Instruments
 * Added OMAP4 support - Santosh Shilimkar <santosh.shilimkar@ti.com>
 *
 * Copyright (C) 2010 Texas Instruments, Inc.
 * Add hwmod support
 * Thara Gopinath <thara@ti.com>
 * Tarun Kanti DebBarma <tarun.kanti@ti.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN
 * NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * You should have received a copy of the  GNU General Public License along
 * with this program; if not, write  to the Free Software Foundation, Inc.,
 * 675 Mass Ave, Cambridge, MA 02139, USA.
 */
#include <linux/init.h>
#include <linux/spinlock.h>
#include <linux/errno.h>
#include <linux/list.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <mach/hardware.h>
#include <linux/pm_runtime.h>
#include <plat/dmtimer.h>
#include <plat/omap_device.h>
#include <mach/irqs.h>

/* register offsets */
#define _OMAP_TIMER_ID_OFFSET		0x00
#define _OMAP_TIMER_OCP_CFG_OFFSET	0x10
#define _OMAP_TIMER_SYS_STAT_OFFSET	0x14
#define _OMAP_TIMER_STAT_OFFSET		0x18
#define _OMAP_TIMER_INT_EN_OFFSET	0x1c
#define _OMAP_TIMER_INT_CLR_OFFSET	0x30
#define _OMAP_TIMER_WAKEUP_EN_OFFSET	0x20
#define _OMAP_TIMER_CTRL_OFFSET		0x24
#define		OMAP_TIMER_CTRL_GPOCFG		(1 << 14)
#define		OMAP_TIMER_CTRL_CAPTMODE	(1 << 13)
#define		OMAP_TIMER_CTRL_PT		(1 << 12)
#define		OMAP_TIMER_CTRL_TCM_LOWTOHIGH	(0x1 << 8)
#define		OMAP_TIMER_CTRL_TCM_HIGHTOLOW	(0x2 << 8)
#define		OMAP_TIMER_CTRL_TCM_BOTHEDGES	(0x3 << 8)
#define		OMAP_TIMER_CTRL_SCPWM		(1 << 7)
#define		OMAP_TIMER_CTRL_CE		(1 << 6) /* compare enable */
#define		OMAP_TIMER_CTRL_PRE		(1 << 5) /* prescaler enable */
#define		OMAP_TIMER_CTRL_PTV_SHIFT	2 /* prescaler value shift */
#define		OMAP_TIMER_CTRL_POSTED		(1 << 2)
#define		OMAP_TIMER_CTRL_AR		(1 << 1) /* auto-reload enable */
#define		OMAP_TIMER_CTRL_ST		(1 << 0) /* start timer */
#define _OMAP_TIMER_COUNTER_OFFSET	0x28
#define _OMAP_TIMER_LOAD_OFFSET		0x2c
#define _OMAP_TIMER_TRIGGER_OFFSET	0x30
#define _OMAP_TIMER_WRITE_PEND_OFFSET	0x34
#define		WP_NONE			0	/* no write pending bit */
#define		WP_TCLR			(1 << 0)
#define		WP_TCRR			(1 << 1)
#define		WP_TLDR			(1 << 2)
#define		WP_TTGR			(1 << 3)
#define		WP_TMAR			(1 << 4)
#define		WP_TPIR			(1 << 5)
#define		WP_TNIR			(1 << 6)
#define		WP_TCVR			(1 << 7)
#define		WP_TOCR			(1 << 8)
#define		WP_TOWR			(1 << 9)
#define _OMAP_TIMER_MATCH_OFFSET	0x38
#define _OMAP_TIMER_CAPTURE_OFFSET	0x3c
#define _OMAP_TIMER_IF_CTRL_OFFSET	0x40
#define _OMAP_TIMER_CAPTURE2_OFFSET		0x44	/* TCAR2, 34xx only */
#define _OMAP_TIMER_TICK_POS_OFFSET		0x48	/* TPIR, 34xx only */
#define _OMAP_TIMER_TICK_NEG_OFFSET		0x4c	/* TNIR, 34xx only */
#define _OMAP_TIMER_TICK_COUNT_OFFSET		0x50	/* TCVR, 34xx only */
#define _OMAP_TIMER_TICK_INT_MASK_SET_OFFSET	0x54	/* TOCR, 34xx only */
#define _OMAP_TIMER_TICK_INT_MASK_COUNT_OFFSET	0x58	/* TOWR, 34xx only */

/* register offsets with the write pending bit encoded */
#define	WPSHIFT					16

#define OMAP_TIMER_ID_REG			(_OMAP_TIMER_ID_OFFSET \
							| (WP_NONE << WPSHIFT))

#define OMAP_TIMER_OCP_CFG_REG			(_OMAP_TIMER_OCP_CFG_OFFSET \
							| (WP_NONE << WPSHIFT))

#define OMAP_TIMER_SYS_STAT_REG			(_OMAP_TIMER_SYS_STAT_OFFSET \
							| (WP_NONE << WPSHIFT))

#define OMAP_TIMER_STAT_REG			(_OMAP_TIMER_STAT_OFFSET \
							| (WP_NONE << WPSHIFT))

#define OMAP_TIMER_INT_EN_REG			(_OMAP_TIMER_INT_EN_OFFSET \
							| (WP_NONE << WPSHIFT))

#define	OMAP_TIMER_INT_CLR_REG			(_OMAP_TIMER_INT_CLR_OFFSET \
							| (WP_NONE << WPSHIFT))

#define OMAP_TIMER_WAKEUP_EN_REG		(_OMAP_TIMER_WAKEUP_EN_OFFSET \
							| (WP_NONE << WPSHIFT))

#define OMAP_TIMER_CTRL_REG			(_OMAP_TIMER_CTRL_OFFSET \
							| (WP_TCLR << WPSHIFT))

#define OMAP_TIMER_COUNTER_REG			(_OMAP_TIMER_COUNTER_OFFSET \
							| (WP_TCRR << WPSHIFT))

#define OMAP_TIMER_LOAD_REG			(_OMAP_TIMER_LOAD_OFFSET \
							| (WP_TLDR << WPSHIFT))

#define OMAP_TIMER_TRIGGER_REG			(_OMAP_TIMER_TRIGGER_OFFSET \
							| (WP_TTGR << WPSHIFT))

#define OMAP_TIMER_WRITE_PEND_REG		(_OMAP_TIMER_WRITE_PEND_OFFSET \
							| (WP_NONE << WPSHIFT))

#define OMAP_TIMER_MATCH_REG			(_OMAP_TIMER_MATCH_OFFSET \
							| (WP_TMAR << WPSHIFT))

#define OMAP_TIMER_CAPTURE_REG			(_OMAP_TIMER_CAPTURE_OFFSET \
							| (WP_NONE << WPSHIFT))

#define OMAP_TIMER_IF_CTRL_REG			(_OMAP_TIMER_IF_CTRL_OFFSET \
							| (WP_NONE << WPSHIFT))

#define OMAP_TIMER_CAPTURE2_REG			(_OMAP_TIMER_CAPTURE2_OFFSET \
							| (WP_NONE << WPSHIFT))

#define OMAP_TIMER_TICK_POS_REG			(_OMAP_TIMER_TICK_POS_OFFSET \
							| (WP_TPIR << WPSHIFT))

#define OMAP_TIMER_TICK_NEG_REG			(_OMAP_TIMER_TICK_NEG_OFFSET \
							| (WP_TNIR << WPSHIFT))

#define OMAP_TIMER_TICK_COUNT_REG		(_OMAP_TIMER_TICK_COUNT_OFFSET \
							| (WP_TCVR << WPSHIFT))

#define OMAP_TIMER_TICK_INT_MASK_SET_REG				\
		(_OMAP_TIMER_TICK_INT_MASK_SET_OFFSET | (WP_TOCR << WPSHIFT))

#define OMAP_TIMER_TICK_INT_MASK_COUNT_REG				\
		(_OMAP_TIMER_TICK_INT_MASK_COUNT_OFFSET | (WP_TOWR << WPSHIFT))

struct omap_dm_timer {
	int id;
	unsigned long phys_base;
	unsigned long fclk_rate;
	int irq;
	void __iomem *io_base;
	unsigned reserved:1;
	unsigned enabled:1;
	unsigned posted:1;
	unsigned is_initialized:1;
	struct platform_device *pdev;
	struct list_head node;
};

static LIST_HEAD(omap_timer_list);
static spinlock_t dm_timer_lock;
static int spinlock_initialized __initdata;

/*
 * Reads timer registers in posted and non-posted mode. The posted mode bit
 * is encoded in reg. Note that in posted mode write pending bit must be
 * checked. Otherwise a read of a non completed write will produce an error.
 */
static inline u32 omap_dm_timer_read_reg(struct omap_dm_timer *timer, u32 reg)
{
	struct omap_dmtimer_platform_data *pdata = \
					timer->pdev->dev.platform_data;

	if (reg >= OMAP_TIMER_STAT_REG && reg <= OMAP_TIMER_INT_EN_REG)
		reg += pdata->offset1;
	else if (reg >= OMAP_TIMER_WAKEUP_EN_REG)
		reg += pdata->offset2;
	if (timer->posted)
		while (readl(timer->io_base + \
			((OMAP_TIMER_WRITE_PEND_REG + pdata->offset2) & 0xff))
				& (reg >> WPSHIFT))
			cpu_relax();
	return readl(timer->io_base + (reg & 0xff));
}

/*
 * Writes timer registers in posted and non-posted mode. The posted mode bit
 * is encoded in reg. Note that in posted mode the write pending bit must be
 * checked. Otherwise a write on a register which has a pending write will be
 * lost.
 */
static void omap_dm_timer_write_reg(struct omap_dm_timer *timer, u32 reg,
						u32 value)
{
	struct omap_dmtimer_platform_data *pdata = \
					timer->pdev->dev.platform_data;

	if (reg >= OMAP_TIMER_STAT_REG && reg <= OMAP_TIMER_INT_EN_REG)
		reg += pdata->offset1;
	else if (reg >= OMAP_TIMER_WAKEUP_EN_REG)
		reg += pdata->offset2;
	if (timer->posted)
		while (readl(timer->io_base + \
			((OMAP_TIMER_WRITE_PEND_REG + pdata->offset2) & 0xff))
				& (reg >> WPSHIFT))
			cpu_relax();
	writel(value, timer->io_base + (reg & 0xff));
}

static void omap_dm_timer_wait_for_reset(struct omap_dm_timer *timer)
{
	int c;
	u32 reg_address;
	int reset_active;
	struct omap_dmtimer_platform_data *pdata = \
					timer->pdev->dev.platform_data;

	if (pdata->timer_ip_type == OMAP_TIMER_IP_VERSION_2) {
		reg_address = OMAP_TIMER_OCP_CFG_REG;
		reset_active = 1;
	} else {
		reg_address = OMAP_TIMER_SYS_STAT_REG;
		reset_active = 0;
	}

	c = 0;
	while (omap_dm_timer_read_reg(timer, reg_address) == reset_active) {
		c++;
		if (c > 100000) {
			printk(KERN_ERR "%s:Timer failed to reset\n", __func__);
			return;
		}
	}
}

static void omap_dm_timer_reset(struct omap_dm_timer *timer)
{
	if (!cpu_class_is_omap2() || timer->id != 0) {
		omap_dm_timer_write_reg(timer, OMAP_TIMER_IF_CTRL_REG, 0x06);
		omap_dm_timer_wait_for_reset(timer);
	}
	omap_dm_timer_set_source(timer, OMAP_TIMER_SRC_32_KHZ);

	/* Match hardware reset default of posted mode */
	omap_dm_timer_write_reg(timer, OMAP_TIMER_IF_CTRL_REG,
			OMAP_TIMER_CTRL_POSTED);
	timer->posted = 1;
}

static void omap_dm_timer_prepare(struct omap_dm_timer *timer)
{
	omap_dm_timer_enable(timer);
	omap_dm_timer_reset(timer);
}

struct omap_dm_timer *omap_dm_timer_request(void)
{
	struct omap_dm_timer *timer;
	unsigned long flags;
	int found = 0;

	spin_lock_irqsave(&dm_timer_lock, flags);
	list_for_each_entry(timer, &omap_timer_list, node) {
		if (timer->reserved)
			continue;
		timer->reserved = 1;
		found = 1;
		break;
	}
	spin_unlock_irqrestore(&dm_timer_lock, flags);

	if (found)
		omap_dm_timer_prepare(timer);
	else
		timer = NULL;

	return timer;
}
EXPORT_SYMBOL_GPL(omap_dm_timer_request);

struct omap_dm_timer *omap_dm_timer_request_specific(int id)
{
	struct omap_dm_timer *timer;
	unsigned long flags;
	int found = 0;

	spin_lock_irqsave(&dm_timer_lock, flags);
	list_for_each_entry(timer, &omap_timer_list, node) {
		if (timer->id == id-1 && !timer->reserved) {
			found = 1;
			timer->reserved = 1;
			break;
		}
	}
	spin_unlock_irqrestore(&dm_timer_lock, flags);

	if (found)
		omap_dm_timer_prepare(timer);
	else
		timer = NULL;
	return timer;
}
EXPORT_SYMBOL_GPL(omap_dm_timer_request_specific);

void omap_dm_timer_free(struct omap_dm_timer *timer)
{
	omap_dm_timer_enable(timer);
	omap_dm_timer_reset(timer);
	omap_dm_timer_disable(timer);

	WARN_ON(!timer->reserved);
	timer->reserved = 0;
}
EXPORT_SYMBOL_GPL(omap_dm_timer_free);

void omap_dm_timer_enable(struct omap_dm_timer *timer)
{
	struct omap_dmtimer_platform_data *pdata = \
				timer->pdev->dev.platform_data;

	if (timer->enabled)
		return;

	if (pdata->omap_dm_clk_enable)
		pdata->omap_dm_clk_enable(timer->pdev);

	timer->enabled = 1;
}
EXPORT_SYMBOL_GPL(omap_dm_timer_enable);

void omap_dm_timer_disable(struct omap_dm_timer *timer)
{
	struct omap_dmtimer_platform_data *pdata = \
		timer->pdev->dev.platform_data;

	if (!timer->enabled)
		return;

	if (pdata->omap_dm_clk_disable)
		pdata->omap_dm_clk_disable(timer->pdev);

	timer->enabled = 0;
}

EXPORT_SYMBOL_GPL(omap_dm_timer_disable);

int omap_dm_timer_get_irq(struct omap_dm_timer *timer)
{
	return timer->irq;
}
EXPORT_SYMBOL_GPL(omap_dm_timer_get_irq);

/**
 * omap_dm_timer_modify_idlect_mask - Check if any running timers use ARMXOR
 * @inputmask: current value of idlect mask
 */
__u32 omap_dm_timer_modify_idlect_mask(__u32 inputmask)
{
	int i = 0;
	u32 l;
	struct omap_dm_timer *timer;

	if (cpu_class_is_omap2())
		return 0;

	/* If ARMXOR cannot be idled this function call is unnecessary */
	if (!(inputmask & (1 << 1)))
		return inputmask;

	/* If any active timer is using ARMXOR return modified mask */
	list_for_each_entry(timer, &omap_timer_list, node) {
		l = omap_dm_timer_read_reg(timer, OMAP_TIMER_CTRL_REG);
		if (l & OMAP_TIMER_CTRL_ST) {
			if (((omap_readl(MOD_CONF_CTRL_1) >> (i * 2)) & 0x03) == 0)
				inputmask &= ~(1 << 1);
			else
				inputmask &= ~(1 << 2);
		}
		i++;
	}

	return inputmask;
}
EXPORT_SYMBOL_GPL(omap_dm_timer_modify_idlect_mask);

struct clk *omap_dm_timer_get_fclk(struct omap_dm_timer *timer)
{
	struct omap_dmtimer_platform_data *pdata = \
				timer->pdev->dev.platform_data;

	if (pdata->omap_dm_get_timer_clk)
		return pdata->omap_dm_get_timer_clk(timer->pdev);

	return NULL;
}
EXPORT_SYMBOL_GPL(omap_dm_timer_get_fclk);

void omap_dm_timer_trigger(struct omap_dm_timer *timer)
{
	omap_dm_timer_write_reg(timer, OMAP_TIMER_TRIGGER_REG, 0);
}
EXPORT_SYMBOL_GPL(omap_dm_timer_trigger);

void omap_dm_timer_start(struct omap_dm_timer *timer)
{
	u32 l;

	l = omap_dm_timer_read_reg(timer, OMAP_TIMER_CTRL_REG);
	if (!(l & OMAP_TIMER_CTRL_ST)) {
		l |= OMAP_TIMER_CTRL_ST;
		omap_dm_timer_write_reg(timer, OMAP_TIMER_CTRL_REG, l);
	}
}
EXPORT_SYMBOL_GPL(omap_dm_timer_start);

void omap_dm_timer_stop(struct omap_dm_timer *timer)
{
	u32 l;

	l = omap_dm_timer_read_reg(timer, OMAP_TIMER_CTRL_REG);
	if (l & OMAP_TIMER_CTRL_ST) {
		l &= ~0x1;
		omap_dm_timer_write_reg(timer, OMAP_TIMER_CTRL_REG, l);
#ifdef CONFIG_ARCH_OMAP2PLUS
		/* Readback to make sure write has completed */
		omap_dm_timer_read_reg(timer, OMAP_TIMER_CTRL_REG);
		 /*
		  * Wait for functional clock period x 3.5 to make sure that
		  * timer is stopped
		  */
		/* FIXME: causing div by zero error. check if this is needed */
		/*udelay(3500000 / timer->fclk_rate + 1);*/

	/* Ack possibly pending interrupt */
	omap_dm_timer_write_reg(timer, OMAP_TIMER_STAT_REG,
			OMAP_TIMER_INT_OVERFLOW);
#endif
	}
}
EXPORT_SYMBOL_GPL(omap_dm_timer_stop);

int omap_dm_timer_set_source(struct omap_dm_timer *timer, int source)
{
	struct omap_dmtimer_platform_data *pdata = \
				timer->pdev->dev.platform_data;

	if (pdata->omap_dm_set_source_clk)
		timer->fclk_rate = pdata->omap_dm_set_source_clk
				(timer->pdev, source);
	return 0;
}
EXPORT_SYMBOL_GPL(omap_dm_timer_set_source);


void omap_dm_timer_set_load(struct omap_dm_timer *timer, int autoreload,
			    unsigned int load)
{
	u32 l;

	l = omap_dm_timer_read_reg(timer, OMAP_TIMER_CTRL_REG);
	if (autoreload)
		l |= OMAP_TIMER_CTRL_AR;
	else
		l &= ~OMAP_TIMER_CTRL_AR;
	omap_dm_timer_write_reg(timer, OMAP_TIMER_CTRL_REG, l);
	omap_dm_timer_write_reg(timer, OMAP_TIMER_LOAD_REG, load);

	omap_dm_timer_write_reg(timer, OMAP_TIMER_TRIGGER_REG, 0);
}
EXPORT_SYMBOL_GPL(omap_dm_timer_set_load);

/* Optimized set_load which removes costly spin wait in timer_start */
void omap_dm_timer_set_load_start(struct omap_dm_timer *timer, int autoreload,
                            unsigned int load)
{
	u32 l;

	l = omap_dm_timer_read_reg(timer, OMAP_TIMER_CTRL_REG);
	if (autoreload) {
		l |= OMAP_TIMER_CTRL_AR;
		omap_dm_timer_write_reg(timer, OMAP_TIMER_LOAD_REG, load);
	} else {
		l &= ~OMAP_TIMER_CTRL_AR;
	}
	l |= OMAP_TIMER_CTRL_ST;

	omap_dm_timer_write_reg(timer, OMAP_TIMER_COUNTER_REG, load);
	omap_dm_timer_write_reg(timer, OMAP_TIMER_CTRL_REG, l);
}
EXPORT_SYMBOL_GPL(omap_dm_timer_set_load_start);

void omap_dm_timer_set_match(struct omap_dm_timer *timer, int enable,
			     unsigned int match)
{
	u32 l;

	l = omap_dm_timer_read_reg(timer, OMAP_TIMER_CTRL_REG);
	if (enable)
		l |= OMAP_TIMER_CTRL_CE;
	else
		l &= ~OMAP_TIMER_CTRL_CE;
	omap_dm_timer_write_reg(timer, OMAP_TIMER_CTRL_REG, l);
	omap_dm_timer_write_reg(timer, OMAP_TIMER_MATCH_REG, match);
}
EXPORT_SYMBOL_GPL(omap_dm_timer_set_match);

void omap_dm_timer_set_pwm(struct omap_dm_timer *timer, int def_on,
			   int toggle, int trigger)
{
	u32 l;

	l = omap_dm_timer_read_reg(timer, OMAP_TIMER_CTRL_REG);
	l &= ~(OMAP_TIMER_CTRL_GPOCFG | OMAP_TIMER_CTRL_SCPWM |
	       OMAP_TIMER_CTRL_PT | (0x03 << 10));
	if (def_on)
		l |= OMAP_TIMER_CTRL_SCPWM;
	if (toggle)
		l |= OMAP_TIMER_CTRL_PT;
	l |= trigger << 10;
	omap_dm_timer_write_reg(timer, OMAP_TIMER_CTRL_REG, l);
}
EXPORT_SYMBOL_GPL(omap_dm_timer_set_pwm);

void omap_dm_timer_set_prescaler(struct omap_dm_timer *timer, int prescaler)
{
	u32 l;

	l = omap_dm_timer_read_reg(timer, OMAP_TIMER_CTRL_REG);
	l &= ~(OMAP_TIMER_CTRL_PRE | (0x07 << 2));
	if (prescaler >= 0x00 && prescaler <= 0x07) {
		l |= OMAP_TIMER_CTRL_PRE;
		l |= prescaler << 2;
	}
	omap_dm_timer_write_reg(timer, OMAP_TIMER_CTRL_REG, l);
}
EXPORT_SYMBOL_GPL(omap_dm_timer_set_prescaler);

void omap_dm_timer_set_int_enable(struct omap_dm_timer *timer,
				  unsigned int value)
{
	omap_dm_timer_write_reg(timer, OMAP_TIMER_INT_EN_REG, value);
	omap_dm_timer_write_reg(timer, OMAP_TIMER_WAKEUP_EN_REG, value);
}
EXPORT_SYMBOL_GPL(omap_dm_timer_set_int_enable);

void omap_dm_timer_set_int_disable(struct omap_dm_timer *timer,
					unsigned int value)
{
	u32 l;
	struct omap_dmtimer_platform_data *pdata = \
					timer->pdev->dev.platform_data;

	l = omap_dm_timer_read_reg(timer, OMAP_TIMER_WAKEUP_EN_REG);
	if (pdata->timer_ip_type == OMAP_TIMER_IP_VERSION_2) {
		l |= value;
		omap_dm_timer_write_reg(timer, OMAP_TIMER_INT_CLR_REG, value);
	} else {
		l &= ~value;
		omap_dm_timer_write_reg(timer, OMAP_TIMER_INT_EN_REG, l);
	}
	omap_dm_timer_write_reg(timer, OMAP_TIMER_WAKEUP_EN_REG, l);
}
EXPORT_SYMBOL_GPL(omap_dm_timer_set_int_disable);

unsigned int omap_dm_timer_read_status(struct omap_dm_timer *timer)
{
	unsigned int l;

	l = omap_dm_timer_read_reg(timer, OMAP_TIMER_STAT_REG);

	return l;
}
EXPORT_SYMBOL_GPL(omap_dm_timer_read_status);

void omap_dm_timer_write_status(struct omap_dm_timer *timer, unsigned int value)
{
	omap_dm_timer_write_reg(timer, OMAP_TIMER_STAT_REG, value);
}
EXPORT_SYMBOL_GPL(omap_dm_timer_write_status);

unsigned int omap_dm_timer_read_counter(struct omap_dm_timer *timer)
{
	unsigned int l;

	l = omap_dm_timer_read_reg(timer, OMAP_TIMER_COUNTER_REG);

	return l;
}
EXPORT_SYMBOL_GPL(omap_dm_timer_read_counter);

void omap_dm_timer_write_counter(struct omap_dm_timer *timer, unsigned int value)
{
	omap_dm_timer_write_reg(timer, OMAP_TIMER_COUNTER_REG, value);
}
EXPORT_SYMBOL_GPL(omap_dm_timer_write_counter);

int omap_dm_timers_active(void)
{
	struct omap_dm_timer *timer;

	list_for_each_entry(timer, &omap_timer_list, node) {
		if (!timer->enabled)
			continue;

		if (omap_dm_timer_read_reg(timer, OMAP_TIMER_CTRL_REG) &
		    OMAP_TIMER_CTRL_ST) {
			return 1;
		}
	}
	return 0;
}
EXPORT_SYMBOL_GPL(omap_dm_timers_active);

static int __devinit omap_dm_timer_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct omap_dm_timer *timer;
	struct resource *res;
	struct omap_dmtimer_platform_data *pdata = pdev->dev.platform_data;
	bool enabled =  false;

	if (!pdev || !pdev->dev.platform_data) {
		pr_err("%s:Timer device initialized without platform data\n",
		__func__);
		return -EINVAL;
	}
	dev_info(&pdev->dev, "%s:[id=%d]\n", __func__, pdev->id);
	/*
	 * early timers are already registered and in list.
	 * what we need to do during second phase of probe
	 * is to free the prevoious platform data and then
	 * assign the newly allocated / configured platform
	 * data which contains different (i) timer enable/disable
	 * functions (ii) timer set/get clock functions
	 */
	list_for_each_entry(timer, &omap_timer_list, node)
		if (timer->id == pdev->id) {
			/*
			 * Before switching to pm_runtime, disable
			 * timer clock using old timer->pdev handle.
			 */
			if (timer->enabled) {
				omap_dm_timer_disable(timer);
				enabled = true;
			}

			/* replace early platform device with new allocation */
			timer->pdev = pdev;

			pm_runtime_enable(&pdev->dev);

			/*
			 * Enable timer clock using new timer->pdev
			 * handle which now uses pm_runtime api. this is needed
			 * to ensure device clock in  mandated state before
			 * making clock state transitions.
			 */
			if (enabled)
				omap_dm_timer_enable(timer);
			return 0;
		}
	timer = kzalloc(sizeof(struct omap_dm_timer), GFP_KERNEL);
	if (!timer) {
		pr_err("%s: No memory for omap_dm_timer\n", __func__);
		return -ENOMEM;
	}

	res = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (unlikely(!res)) {
		pr_err("%s:timer%d has invalid IRQ resource\n",
			__func__, pdev->id + 1);
		ret = -ENODEV;
		goto exit_1;
	}
	timer->irq = res->start;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (unlikely(!res)) {
		pr_err("%s:timer%d has invalid memory resource\n",
			__func__, pdev->id + 1);
		ret = -ENOMEM;
		goto exit_1;
	}
	timer->io_base = ioremap(res->start, resource_size(res));
	if (!timer->io_base) {
		dev_err(&pdev->dev, "%s: ioremap failed\n", __func__);
		ret = -ENOMEM;
		goto exit_2;
	}

	timer->pdev = pdev;
	timer->id = pdev->id;
	timer->reserved = 0;
	timer->is_initialized = 1;
	list_add_tail(&timer->node, &omap_timer_list);

	if (pdata->timer_ip_type != OMAP_TIMER_IP_LEGACY)
		pm_runtime_enable(&pdev->dev);
	/*
	 * initializing spinlock here so that it is available
	 * during early timers access. please note that probe
	 * is initialized during early init through separate
	 * path early_platform_init()
	 */
	if (!spinlock_initialized) {
		spin_lock_init(&dm_timer_lock);
		spinlock_initialized = 1;
	}

	dev_info(&pdev->dev, " registered\n");

	return ret;
exit_2:
	release_mem_region(res->start, resource_size(res));
exit_1:
	kfree(timer);
	return ret;
	}

static int __devexit omap_dm_timer_remove(struct platform_device *pdev)
{
	struct resource *res;
	struct omap_dm_timer *timer;

	list_for_each_entry(timer, &omap_timer_list, node) {
		if (timer->id == pdev->id && timer->is_initialized) {
			iounmap(timer->io_base);
			res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
			release_mem_region(res->start, resource_size(res));
			kfree(timer);
			list_del(&timer->node);
			return 0;
		}
	}
	return -EINVAL;
}

static struct platform_driver omap_dmtimer_driver = {
	.probe          = omap_dm_timer_probe,
	.remove			= omap_dm_timer_remove,
	.driver         = {
		.name   = "dmtimer",
	},
};

static int __init omap_dm_timer_init(void)
{
	return platform_driver_register(&omap_dmtimer_driver);
}

static void __exit omap_dm_timer_exit(void)
{
	platform_driver_unregister(&omap_dmtimer_driver);
}

early_platform_init("earlytimer", &omap_dmtimer_driver);
module_init(omap_dm_timer_init);
module_exit(omap_dm_timer_exit);

MODULE_DESCRIPTION("OMAP DUAL MODE TIMER DRIVER");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:" DRIVER_NAME);
MODULE_AUTHOR("Texas Instruments Inc");
