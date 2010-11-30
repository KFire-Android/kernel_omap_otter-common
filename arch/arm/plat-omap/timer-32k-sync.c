/*
 * timer-32k-sync.c -- OMAP 32k Sync Timer Clocksource Driver
 *
 * Copyright (C) 2005-2010 Tony Lindgren <tony@atomide.com>
 * Copyright (C) 2010 Nokia Corporation
 * Copyright (C) 2010 Felipe Balbi <me@felipebalbi.com>
 * Copyright (C) 2009 Texas Instruments
 * Added OMAP4 support - Santosh Shilimkar <santosh.shilimkar@ti.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#include <linux/kernel.h>
#include <linux/time.h>
#include <linux/clocksource.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/slab.h>

struct omap_32k_sync_device {
	struct timespec         persistent_ts;
	struct clocksource      cs;
	cycles_t                cycles;
	cycles_t                last_cycles;

	struct device           *dev;
	struct clk              *ick;
	void __iomem            *base;

	/*
	 * offset_32k holds the init time counter value. It is then subtracted
	 * from every counter read to achieve a counter that counts time from the
	 * kernel boot (needed for sched_clock()).
	 */
	u32                     offset_32k __read_mostly;
};

#define to_omap_32k(cs)        (container_of(cs, struct omap_32k_sync_device, cs))

static struct omap_32k_sync_device     *thecs;

static inline u32 omap_32k_sync_readl(const void __iomem *base, unsigned offset)
{
	return __raw_readl(base + offset);
}

static cycle_t omap_32k_sync_32k_read(struct clocksource *cs)
{
	struct omap_32k_sync_device     *omap = to_omap_32k(cs);

	return omap_32k_sync_readl(omap->base, 0x10) - omap->offset_32k;
}

/*
 * Returns current time from boot in nsecs. It's OK for this to wrap
 * around for now, as it's just a relative time stamp.
 */
unsigned long long sched_clock(void)
{
	struct omap_32k_sync_device     *omap = thecs;

	if (!omap)
		return 0;

	return clocksource_cyc2ns(omap->cs.read(&omap->cs),
			omap->cs.mult, omap->cs.shift);
}

/**
 * read_persistent_clock -  Return time from a persistent clock.
 *
 * Reads the time from a source which isn't disabled during PM, the
 * 32k sync timer.  Convert the cycles elapsed since last read into
 * nsecs and adds to a monotonically increasing timespec.
 */
void read_persistent_clock(struct timespec *ts)
{
	struct omap_32k_sync_device     *omap = thecs;
	unsigned long long      nsecs;
	cycles_t                delta;
	struct timespec         *tsp;

	if (!omap) {
		ts->tv_sec = 0;
		ts->tv_nsec = 0;
		return;
	}

	tsp = &omap->persistent_ts;

	omap->last_cycles = omap->cycles;
	omap->cycles = omap->cs.read(&omap->cs);
	delta = omap->cycles - omap->last_cycles;

	nsecs = clocksource_cyc2ns(delta,
			omap->cs.mult, omap->cs.shift);

	timespec_add_ns(tsp, nsecs);
	*ts = *tsp;
}

/* ------------------------------------------------------------------------- */
#define DEVNAME "timer32k"
#include <linux/fs.h>
#include <linux/timer-32k.h>

static int omap_32k_ioctl(struct inode *inode, struct file *file,
		unsigned int cmd, unsigned long arg)
{
	struct omap_32k_sync_device *omap = thecs;

	if (!omap)
		return -ENODEV;

    switch (cmd) {
		case OMAP_32K_READ: {
			uint32_t *t = (uint32_t *)arg;
			*t = omap_32k_sync_readl(omap->base, 0x10)
							- omap->offset_32k;
			return 0;
		}
		case OMAP_32K_READRAW: {
			uint32_t *t = (uint32_t *)arg;
			*t = omap_32k_sync_readl(omap->base, 0x10);
			return 0;
		}
    }

    return -EINVAL;
}

static int __init omap_32k_sync_register_chrdev(void)
{
	int major;
	static struct file_operations fops = {
		.ioctl = omap_32k_ioctl
	};
	static struct class *cls;
	struct device *dev;

	major = register_chrdev(0, DEVNAME, &fops);

	if (major <= 0) {
		printk(KERN_ERR "timer-32k-sync: unable to get major number\n");
		goto exit_disable;
	}

	cls = class_create(THIS_MODULE, DEVNAME);

	if (IS_ERR(cls)) {
		printk(KERN_ERR "timer-32k-sync: unable to create device class\n");
		goto exit_unregister;
	}

	dev = device_create(cls, NULL, MKDEV(major, 0), NULL, DEVNAME);

	if (IS_ERR(dev)) {
		printk(KERN_ERR "timer-32k-sync: unable to create device\n");
		goto exit_destroy_class;
	}

	return 0;

exit_destroy_class:
	class_destroy(cls);
exit_unregister:
	unregister_chrdev(major, DEVNAME);
exit_disable:
	return -EBUSY;
}
/* ------------------------------------------------------------------------- */

static int __init omap_32k_sync_probe(struct platform_device *pdev)
{
	struct omap_32k_sync_device             *omap;
	struct resource                 *res;
	struct clk                      *ick;

	int                             ret;

	void __iomem                    *base;

	omap = kzalloc(sizeof(*omap), GFP_KERNEL);
	if (!omap) {
		dev_dbg(&pdev->dev, "unable to allocate memory\n");
		ret = -ENOMEM;
		goto err0;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_dbg(&pdev->dev, "couldn't get resource\n");
		ret = -ENODEV;
		goto err1;
	}

	base = ioremap(res->start, resource_size(res));
	if (!base) {
		dev_dbg(&pdev->dev, "ioremap failed\n");
		ret = -ENOMEM;
		goto err2;
	}

	ick = clk_get(&pdev->dev, "ick");
	if (IS_ERR(ick)) {
		dev_dbg(&pdev->dev, "couldn't get clock\n");
		ret = PTR_ERR(ick);
		goto err3;
	}

	ret = clk_enable(ick);
	if (ret) {
		dev_dbg(&pdev->dev, "couldn't enable clock\n");
		goto err4;
	}

	omap->base      = base;
	omap->dev       = &pdev->dev;
	omap->ick       = ick;

	omap->cs.name   = "timer-32k";
	omap->cs.rating = 250;
	omap->cs.read   = omap_32k_sync_32k_read;
	omap->cs.mask   = CLOCKSOURCE_MASK(32);
	omap->cs.shift  = 10;
	omap->cs.flags  = CLOCK_SOURCE_IS_CONTINUOUS;
	omap->cs.mult   = clocksource_hz2mult(32768, omap->cs.shift);

	platform_set_drvdata(pdev, omap);

	ret = clocksource_register(&omap->cs);
	if (ret) {
		dev_dbg(&pdev->dev, "failed to register clocksource\n");
		goto err5;
	}

	/* initialize our offset */
	omap->offset_32k        =  omap_32k_sync_32k_read(&omap->cs);

	/*
	 * REVISIT for now we need to keep a global static pointer
	 * to this clocksource instance. Would it make any sense
	 * to provide a get_clocksource() to fetch the clocksource
	 * we just registered ?
	 */
	thecs = omap;

	omap_32k_sync_register_chrdev();

	return 0;

err5:
	clk_disable(ick);

err4:
	clk_put(ick);

err3:
	iounmap(base);

err2:
err1:
	kfree(omap);

err0:
	return ret;
}

static int __exit omap_32k_sync_remove(struct platform_device *pdev)
{
	struct omap_32k_sync_device     *omap = platform_get_drvdata(pdev);

	clocksource_unregister(&omap->cs);
	clk_disable(omap->ick);
	clk_put(omap->ick);
	iounmap(omap->base);
	kfree(omap);
	platform_set_drvdata(pdev, NULL);

	return 0;
}

static void omap_32k_sync_shutdown(struct platform_device *pdev)
{
	struct omap_32k_sync_device     *omap = platform_get_drvdata(pdev);

	if (!cpu_is_omap3630())
		clk_disable(omap->ick);
}

static struct platform_driver omap_32k_sync_driver = {
	.remove         = __exit_p(omap_32k_sync_remove),
	.shutdown       = omap_32k_sync_shutdown,
	.driver         = {
		.name   = "omap-32k-sync-timer",
	},
};

static int __init omap_32k_sync_init(void)
{
	return platform_driver_probe(&omap_32k_sync_driver, omap_32k_sync_probe);
}
arch_initcall(omap_32k_sync_init);

static void __exit omap_32k_sync_exit(void)
{
	platform_driver_unregister(&omap_32k_sync_driver);
}
module_exit(omap_32k_sync_exit);

MODULE_AUTHOR("Felipe Balbi <me@felipebalbi.com>");
MODULE_LICENSE("GPL v2");

