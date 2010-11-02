/*
 * Copyright (C) 2005-2007 by Texas Instruments
 * Some code has been taken from tusb6010.c
 * Copyrights for that are attributable to:
 * Copyright (C) 2006 Nokia Corporation
 * Tony Lindgren <tony@atomide.com>
 *
 * This file is part of the Inventra Controller Driver for Linux.
 *
 * The Inventra Controller Driver for Linux is free software; you
 * can redistribute it and/or modify it under the terms of the GNU
 * General Public License version 2 as published by the Free Software
 * Foundation.
 *
 * The Inventra Controller Driver for Linux is distributed in
 * the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
 * License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with The Inventra Controller Driver for Linux ; if not,
 * write to the Free Software Foundation, Inc., 59 Temple Place,
 * Suite 330, Boston, MA  02111-1307  USA
 *
 */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/pm_runtime.h>
#include <linux/wakelock.h>

#include <plat/omap-pm.h>

#include "musb_core.h"
#include "omap2430.h"


static struct timer_list musb_idle_timer;
static struct wake_lock usb_lock;

 void musb_enable_vbus(struct musb *musb)
 {
	 int val;

	/* enable VBUS valid, id groung*/
	omap_writel(0x00000005, 0x4A00233C);

	/* start the session */
	val = omap_readb(0x4A0AB060);
	val |= 0x1;
	omap_writel(val, 0x4A0AB060);

	while (musb_readb(musb->mregs, MUSB_DEVCTL)&0x80) {
		mdelay(20);
		DBG(1, "devcontrol before vbus=%x\n", musb_readb(musb->mregs,
						 MUSB_DEVCTL));
	}
	if (musb->xceiv->set_vbus)
		otg_set_vbus(musb->xceiv, 1);

 }

 /* blocking notifier support */
int musb_notifier_call(struct notifier_block *nb,
		unsigned long event, void *unused)
{
	struct musb	*musb = container_of(nb, struct musb, nb);
	struct device *dev = musb->controller;
	struct musb_hdrc_platform_data *pdata = dev->platform_data;
	static int hostmode;

	switch (event) {
	case USB_EVENT_ID:
		DBG(1, "ID GND\n");
		musb->is_active = 1;
		if (omap_readl(0x4A002300)&0x1) {
			omap_writel(0x0, 0x4A002300);
			mdelay(500);
		}
		hostmode = 1;
		musb_enable_vbus(musb);
		break;

	case USB_EVENT_VBUS:
		DBG(1, "VBUS Connect\n");
		wake_lock(&usb_lock);
		musb->is_active = 1;
		if (omap_readl(0x4A002300)&0x1) {
			omap_writel(0x0, 0x4A002300);
			mdelay(400);
		}
		/* hold the L3 constraint as there was performance drop with
		 * ondemand governor
		 */
		if (pdata->set_min_bus_tput)
			pdata->set_min_bus_tput(musb->controller,
					OCP_INITIATOR_AGENT, (200*1000*4));

		if (!hostmode) {
			/* Enable VBUS Valid, BValid, AValid. Clear SESSEND.*/
			omap_writel(0x00000015, 0x4A00233C);
		}
		break;

	case USB_EVENT_NONE:
		DBG(1, "VBUS Disconnect\n");
		omap_writel(0x00000018, 0x4A00233C);

		if (musb->xceiv->set_vbus)
			otg_set_vbus(musb->xceiv, 0);

		/* put the phy in powerdown mode*/
		omap_writel(0x1, 0x4A002300);
		hostmode = 0;
		wake_unlock(&usb_lock);

		/* Release L3 constraint */
		if (pdata->set_min_bus_tput)
			pdata->set_min_bus_tput(musb->controller,
						OCP_INITIATOR_AGENT, -1);
		break;
	default:
		DBG(1, "ID float\n");
		return NOTIFY_DONE;
	}

	return NOTIFY_OK;
}

static void musb_do_idle(unsigned long _musb)
{
	struct musb	*musb = (void *)_musb;
	unsigned long	flags;
#ifdef CONFIG_USB_MUSB_HDRC_HCD
	u8	power;
#endif
	u8	devctl;

	spin_lock_irqsave(&musb->lock, flags);

	devctl = musb_readb(musb->mregs, MUSB_DEVCTL);

	switch (musb->xceiv->state) {
	case OTG_STATE_A_WAIT_BCON:
		devctl &= ~MUSB_DEVCTL_SESSION;
		musb_writeb(musb->mregs, MUSB_DEVCTL, devctl);

		devctl = musb_readb(musb->mregs, MUSB_DEVCTL);
		if (devctl & MUSB_DEVCTL_BDEVICE) {
			musb->xceiv->state = OTG_STATE_B_IDLE;
			MUSB_DEV_MODE(musb);
		} else {
			musb->xceiv->state = OTG_STATE_A_IDLE;
			MUSB_HST_MODE(musb);
		}
		break;
#ifdef CONFIG_USB_MUSB_HDRC_HCD
	case OTG_STATE_A_SUSPEND:
		/* finish RESUME signaling? */
		if (musb->port1_status & MUSB_PORT_STAT_RESUME) {
			power = musb_readb(musb->mregs, MUSB_POWER);
			power &= ~MUSB_POWER_RESUME;
			DBG(1, "root port resume stopped, power %02x\n", power);
			musb_writeb(musb->mregs, MUSB_POWER, power);
			musb->is_active = 1;
			musb->port1_status &= ~(USB_PORT_STAT_SUSPEND
						| MUSB_PORT_STAT_RESUME);
			musb->port1_status |= USB_PORT_STAT_C_SUSPEND << 16;
			usb_hcd_poll_rh_status(musb_to_hcd(musb));
			/* NOTE: it might really be A_WAIT_BCON ... */
			musb->xceiv->state = OTG_STATE_A_HOST;
		}
		break;
#endif
#ifdef CONFIG_USB_MUSB_HDRC_HCD
	case OTG_STATE_A_HOST:
		devctl = musb_readb(musb->mregs, MUSB_DEVCTL);
		if (devctl &  MUSB_DEVCTL_BDEVICE)
			musb->xceiv->state = OTG_STATE_B_IDLE;
		else
			musb->xceiv->state = OTG_STATE_A_WAIT_BCON;
#endif
	default:
		break;
	}
	spin_unlock_irqrestore(&musb->lock, flags);
}


void musb_platform_try_idle(struct musb *musb, unsigned long timeout)
{
	unsigned long		default_timeout = jiffies + msecs_to_jiffies(3);
	static unsigned long	last_timer;

	if (timeout == 0)
		timeout = default_timeout;

	/* Never idle if active, or when VBUS timeout is not set as host */
	if (musb->is_active || ((musb->a_wait_bcon == 0)
			&& (musb->xceiv->state == OTG_STATE_A_WAIT_BCON))) {
		DBG(4, "%s active, deleting timer\n", otg_state_string(musb));
		del_timer(&musb_idle_timer);
		last_timer = jiffies;
		return;
	}

	if (time_after(last_timer, timeout)) {
		if (!timer_pending(&musb_idle_timer))
			last_timer = timeout;
		else {
			DBG(4, "Longer idle timer already pending, ignoring\n");
			return;
		}
	}
	last_timer = timeout;

	DBG(4, "%s inactive, for idle timer for %lu ms\n",
		otg_state_string(musb),
		(unsigned long)jiffies_to_msecs(timeout - jiffies));
	mod_timer(&musb_idle_timer, timeout);
}

void musb_platform_enable(struct musb *musb)
{
}
void musb_platform_disable(struct musb *musb)
{
}
static void omap_set_vbus(struct musb *musb, int is_on)
{
	u8		devctl;
	/* HDRC controls CPEN, but beware current surges during device
	 * connect.  They can trigger transient overcurrent conditions
	 * that must be ignored.
	 */

	devctl = musb_readb(musb->mregs, MUSB_DEVCTL);

	if (is_on) {
		musb->is_active = 1;
		musb->xceiv->default_a = 1;
		musb->xceiv->state = OTG_STATE_A_WAIT_VRISE;
		devctl |= MUSB_DEVCTL_SESSION;

		MUSB_HST_MODE(musb);
	} else {
		musb->is_active = 0;

		/* NOTE:  we're skipping A_WAIT_VFALL -> A_IDLE and
		 * jumping right to B_IDLE...
		 */

		musb->xceiv->default_a = 0;
		musb->xceiv->state = OTG_STATE_B_IDLE;
		devctl &= ~MUSB_DEVCTL_SESSION;

		MUSB_DEV_MODE(musb);
	}
	musb_writeb(musb->mregs, MUSB_DEVCTL, devctl);

	DBG(1, "VBUS %s, devctl %02x "
		/* otg %3x conf %08x prcm %08x */ "\n",
		otg_state_string(musb),
		musb_readb(musb->mregs, MUSB_DEVCTL));
}

static int musb_platform_resume(struct musb *musb);

int musb_platform_set_mode(struct musb *musb, u8 musb_mode)
{
	u8	devctl = musb_readb(musb->mregs, MUSB_DEVCTL);

	devctl |= MUSB_DEVCTL_SESSION;
	musb_writeb(musb->mregs, MUSB_DEVCTL, devctl);

	return 0;
}

int is_musb_active(struct device *dev)
{
	struct musb *musb;

#ifdef CONFIG_USB_MUSB_HDRC_HCD
	/* usbcore insists dev->driver_data is a "struct hcd *" */
	musb = hcd_to_musb(dev_get_drvdata(dev));
#else
	musb = dev_get_drvdata(dev);
#endif
	return musb->is_active;
}

int __init musb_platform_init(struct musb *musb)
{
	u32 l;
	struct device *dev = musb->controller;
	struct musb_hdrc_platform_data *plat = dev->platform_data;
	struct omap_musb_board_data *data = plat->board_data;
	int status;

	/* We require some kind of external transceiver, hooked
	 * up through ULPI.  TWL4030-family PMICs include one,
	 * which needs a driver, drivers aren't always needed.
	 */
	musb->xceiv = otg_get_transceiver();
	if (!musb->xceiv) {
		pr_err("HS USB OTG: no transceiver configured\n");
		return -ENODEV;
	}

	if (cpu_is_omap44xx()) {
		/* disable the optional 60M clock if enabled by romcode*/
		l = omap_readl(0x4A009360);
		l &= ~0x00000100;
		omap_writel(l, 0x4A009360);
		omap_writel(0x1, 0x4A002300);
	}

	/* Fixme this can be enabled when load the gadget driver also*/
	musb_platform_resume(musb);

	/*powerup the phy as romcode would have put the phy in some state
	* which is impacting the core retention if the gadget driver is not
	* loaded.
	*/
	l = musb_readl(musb->mregs, OTG_INTERFSEL);

	if (data->interface_type == MUSB_INTERFACE_UTMI) {
		/* OMAP4 uses Internal PHY GS70 which uses UTMI interface */
		l &= ~ULPI_12PIN;       /* Disable ULPI */
		l |= UTMI_8BIT;         /* Enable UTMI  */
	} else {
		l |= ULPI_12PIN;
	}

	musb_writel(musb->mregs, OTG_INTERFSEL, l);

	pr_debug("HS USB OTG: revision 0x%x, sysconfig 0x%02x, "
			"sysstatus 0x%x, intrfsel 0x%x, simenable  0x%x\n",
			musb_readl(musb->mregs, OTG_REVISION),
			musb_readl(musb->mregs, OTG_SYSCONFIG),
			musb_readl(musb->mregs, OTG_SYSSTATUS),
			musb_readl(musb->mregs, OTG_INTERFSEL),
			musb_readl(musb->mregs, OTG_SIMENABLE));

	if (is_host_enabled(musb))
		musb->board_set_vbus = omap_set_vbus;

	setup_timer(&musb_idle_timer, musb_do_idle, (unsigned long) musb);
	plat->is_usb_active = is_musb_active;

	if (cpu_is_omap44xx()) {
		wake_lock_init(&usb_lock, WAKE_LOCK_SUSPEND, "musb_wake_lock");

		/* register for transciever notification*/
		status = otg_register_notifier(musb->xceiv, &musb->nb);

		if (status) {
			DBG(1, "notification register failed\n");
			wake_lock_destroy(&usb_lock);
		}
	}
	return 0;
}

#ifdef CONFIG_PM
void musb_platform_save_context(struct musb *musb,
		struct musb_context_registers *musb_context)
{
	void __iomem *musb_base = musb->mregs;

	musb_writel(musb_base, OTG_FORCESTDBY, 1);
}

void musb_platform_restore_context(struct musb *musb,
		struct musb_context_registers *musb_context)
{
	void __iomem *musb_base = musb->mregs;

	musb_writel(musb_base, OTG_FORCESTDBY, 0);
}
#endif

static int musb_platform_suspend(struct musb *musb)
{
	u32 l;
	struct device *dev = musb->controller;
	struct musb_hdrc_platform_data *pdata = dev->platform_data;
	struct omap_hwmod *oh = pdata->oh;

	if (!musb->clock)
		return 0;

	/* in any role */
	l = musb_readl(musb->mregs, OTG_FORCESTDBY);
	l |= ENABLEFORCE;	/* enable MSTANDBY */
	musb_writel(musb->mregs, OTG_FORCESTDBY, l);

	pdata->enable_wakeup(oh->od);
	otg_set_suspend(musb->xceiv, 1);

	return 0;
}

static int musb_platform_resume(struct musb *musb)
{
	u32 l;
	struct device *dev = musb->controller;
	struct musb_hdrc_platform_data *pdata = dev->platform_data;
	struct omap_hwmod *oh = pdata->oh;
	struct clk *phyclk;
	struct clk *clk48m;

	if (!musb->clock)
		return 0;

	otg_set_suspend(musb->xceiv, 0);
	pm_runtime_enable(dev);
	pm_runtime_get_sync(dev);

	if (cpu_is_omap44xx()) {
		/* Enable the phy clocks*/
		phyclk = clk_get(NULL, "ocp2scp_usb_phy_ick");
		clk_enable(phyclk);
		clk48m = clk_get(NULL, "ocp2scp_usb_phy_phy_48m");
		clk_enable(clk48m);
	}

	pdata->disable_wakeup(oh->od);
	l = musb_readl(musb->mregs, OTG_FORCESTDBY);
	l &= ~ENABLEFORCE;	/* disable MSTANDBY */
	musb_writel(musb->mregs, OTG_FORCESTDBY, l);

	return 0;
}


int musb_platform_exit(struct musb *musb)
{

	if (cpu_is_omap44xx()) {
		/* register for transciever notification*/
		otg_unregister_notifier(musb->xceiv, &musb->nb);
		wake_lock_destroy(&usb_lock);
	}
	musb_platform_suspend(musb);

	return 0;
}
