/*
 * drivers/i2c/chips/twl6030-gpadc.c
 *
 * TWL6030 GPADC module driver
 *
 * Copyright (C) 2009 Texas Instruments Inc.
 * Nishant Kamat <nskamat@ti.com>
 *
 * Based on twl4030-madc.c
 * Copyright (C) 2008 Nokia Corporation
 * Mikko Ylinen <mikko.k.ylinen@nokia.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 */

#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#include <linux/slab.h>
#include <linux/i2c/twl.h>
#include <linux/i2c/twl6030-gpadc.h>

#include <linux/uaccess.h>

#define TWL6030_GPADC_PFX	"twl6030-gpadc: "
#define ENABLE_GPADC	0x02
#define REG_TOGGLE1	0x90

#define TWL6030_GPADC_MASK		0x20

struct twl6030_gpadc_data {
	struct device		*dev;
	struct mutex		lock;
	struct work_struct	ws;
	struct twl6030_gpadc_request	requests[TWL6030_GPADC_NUM_METHODS];
	int irq_n;
};

static struct twl6030_gpadc_data *the_gpadc;

static
const struct twl6030_gpadc_conversion_method twl6030_conversion_methods[] = {
	[TWL6030_GPADC_RT] = {
		.sel	= TWL6030_GPADC_RTSELECT_LSB,
		.rbase	= TWL6030_GPADC_RTCH0_LSB,
	},
	/*
	 * TWL6030_GPADC_SW1 is not supported as
	 * interrupt from RT and SW1 cannot be differentiated
	 */
	[TWL6030_GPADC_SW2] = {
		.rbase	= TWL6030_GPADC_GPCH0_LSB,
		.ctrl	= TWL6030_GPADC_CTRL_P2,
		.enable = TWL6030_GPADC_CTRL_P2_SP2,
	},
};

static int twl6030_gpadc_read(struct twl6030_gpadc_data *gpadc, u8 reg)
{
	int ret;
	u8 val;

	ret = twl_i2c_read_u8(TWL_MODULE_MADC, &val, reg);
	if (ret) {
		dev_dbg(gpadc->dev, "unable to read register 0x%X\n", reg);
		return ret;
	}

	return val;
}

static void twl6030_gpadc_write(struct twl6030_gpadc_data *gpadc,
				u8 reg, u8 val)
{
	int ret;

	ret = twl_i2c_write_u8(TWL_MODULE_MADC, val, reg);
	if (ret)
		dev_err(gpadc->dev, "unable to write register 0x%X\n", reg);
}

static int twl6030_gpadc_channel_raw_read(struct twl6030_gpadc_data *gpadc,
					  u8 reg)
{
	u8 msb, lsb;

	/* For each ADC channel, we have MSB and LSB register pair. MSB address
	 * is always LSB address+1. reg parameter is the addr of LSB register */
	msb = twl6030_gpadc_read(gpadc, reg + 1);
	lsb = twl6030_gpadc_read(gpadc, reg);

	return (int)((msb << 8) | lsb);
}

static int twl6030_gpadc_read_channels(struct twl6030_gpadc_data *gpadc,
		u8 reg_base, u16 channels, int *buf)
{
	int count = 0;
	u8 reg, i;

	if (unlikely(!buf))
		return 0;

	for (i = 0; i < TWL6030_GPADC_MAX_CHANNELS; i++) {
		if (channels & (1<<i)) {
			reg = reg_base + 2*i;
			buf[i] = twl6030_gpadc_channel_raw_read(gpadc, reg);
			count++;
		}
	}
	return count;
}

static void twl6030_gpadc_enable_irq(u16 method)
{
	twl6030_interrupt_unmask(TWL6030_GPADC_MASK << method,
						REG_INT_MSK_LINE_B);
	twl6030_interrupt_unmask(TWL6030_GPADC_MASK << method,
						REG_INT_MSK_STS_B);
}

static void twl6030_gpadc_disable_irq(u16 method)
{
	twl6030_interrupt_mask(TWL6030_GPADC_MASK << method,
						REG_INT_MSK_LINE_B);
	twl6030_interrupt_mask(TWL6030_GPADC_MASK << method,
						REG_INT_MSK_STS_B);
}

static irqreturn_t twl6030_gpadc_irq_handler(int irq, void *_req)
{
	struct twl6030_gpadc_request *req = _req;

#ifdef CONFIG_LOCKDEP
	/* WORKAROUND for lockdep forcing IRQF_DISABLED on us, which
	 * we don't want and can't tolerate.  Although it might be
	 * friendlier not to borrow this thread context...
	 */
	local_irq_enable();
#endif

	/* Find the cause of the interrupt and enable the pending
	   bit for the corresponding method */
	twl6030_gpadc_disable_irq(req->method);
	req->result_pending = 1;

	schedule_work(&the_gpadc->ws);

	return IRQ_HANDLED;
}

static void twl6030_gpadc_work(struct work_struct *ws)
{
	const struct twl6030_gpadc_conversion_method *method;
	struct twl6030_gpadc_data *gpadc;
	struct twl6030_gpadc_request *r;
	int len, i;

	gpadc = container_of(ws, struct twl6030_gpadc_data, ws);
	mutex_lock(&gpadc->lock);

	for (i = 0; i < TWL6030_GPADC_NUM_METHODS; i++) {

		r = &gpadc->requests[i];

		/* No pending results for this method, move to next one */
		if (!r->result_pending)
			continue;

		method = &twl6030_conversion_methods[r->method];

		/* Read results */
		len = twl6030_gpadc_read_channels(gpadc, method->rbase,
						 r->channels, r->rbuf);

		/* Return results to caller */
		if (r->func_cb != NULL) {
			r->func_cb(len, r->channels, r->rbuf);
			r->func_cb = NULL;
		}

		/* Free request */
		r->result_pending = 0;
		r->active	  = 0;
	}

	mutex_unlock(&gpadc->lock);
}

static int twl6030_gpadc_set_irq(struct twl6030_gpadc_data *gpadc,
		struct twl6030_gpadc_request *req)
{
	struct twl6030_gpadc_request *p;

	p = &gpadc->requests[req->method];
	p->channels = req->channels;
	p->method = req->method;
	p->func_cb = req->func_cb;
	p->type = req->type;

	twl6030_gpadc_enable_irq(req->method);

	return 0;
}

static inline void
twl6030_gpadc_start_conversion(struct twl6030_gpadc_data *gpadc,
			       int conv_method)
{
	const struct twl6030_gpadc_conversion_method *method;

	method = &twl6030_conversion_methods[conv_method];
	twl_i2c_write_u8(TWL6030_MODULE_ID1, ENABLE_GPADC, REG_TOGGLE1);

	switch (conv_method) {
	case TWL6030_GPADC_SW2:
		twl6030_gpadc_write(gpadc, method->ctrl, method->enable);
		break;
	case TWL6030_GPADC_RT:
	default:
		break;
	}
}

static int twl6030_gpadc_wait_conversion_ready(
		struct twl6030_gpadc_data *gpadc,
		unsigned int timeout_ms, u8 status_reg)
{
	unsigned long timeout;

	timeout = jiffies + msecs_to_jiffies(timeout_ms);
	do {
		u8 reg;

		reg = twl6030_gpadc_read(gpadc, status_reg);
		if (!(reg & TWL6030_GPADC_BUSY) && (reg & TWL6030_GPADC_EOC_SW))
			return 0;
	} while (!time_after(jiffies, timeout));

	return -EAGAIN;
}

int twl6030_gpadc_conversion(struct twl6030_gpadc_request *req)
{
	const struct twl6030_gpadc_conversion_method *method;
	u8 ch_msb, ch_lsb;
	int ret = 0;

	if (unlikely(!req))
		return -EINVAL;

	mutex_lock(&the_gpadc->lock);

	/* Do we have a conversion request ongoing */
	if (the_gpadc->requests[req->method].active) {
		ret = -EBUSY;
		goto out;
	}

	method = &twl6030_conversion_methods[req->method];

	if (req->method == TWL6030_GPADC_RT) {
		ch_msb = (req->channels >> 8) & 0xff;
		ch_lsb = req->channels & 0xff;
		twl6030_gpadc_write(the_gpadc, method->sel + 1, ch_msb);
		twl6030_gpadc_write(the_gpadc, method->sel, ch_lsb);
	}

	if ((req->type == TWL6030_GPADC_IRQ_ONESHOT) &&
		 (req->func_cb != NULL)) {
		twl6030_gpadc_set_irq(the_gpadc, req);
		twl6030_gpadc_start_conversion(the_gpadc, req->method);
		the_gpadc->requests[req->method].active = 1;
		ret = 0;
		goto out;
	}

	/* With RT method we should not be here anymore */
	if (req->method == TWL6030_GPADC_RT) {
		ret = -EINVAL;
		goto out;
	}

	twl6030_gpadc_start_conversion(the_gpadc, req->method);
	the_gpadc->requests[req->method].active = 1;

	/* Wait until conversion is ready (ctrl register returns EOC) */
	ret = twl6030_gpadc_wait_conversion_ready(the_gpadc, 5, method->ctrl);
	if (ret) {
		dev_dbg(the_gpadc->dev, "conversion timeout!\n");
		the_gpadc->requests[req->method].active = 0;
		goto out;
	}

	ret = twl6030_gpadc_read_channels(the_gpadc, method->rbase,
					  req->channels, req->rbuf);
	the_gpadc->requests[req->method].active = 0;

out:
	mutex_unlock(&the_gpadc->lock);

	return ret;
}
EXPORT_SYMBOL(twl6030_gpadc_conversion);


static long twl6030_gpadc_ioctl(struct file *filp, unsigned int cmd,
			       unsigned long arg)
{
	struct twl6030_gpadc_user_parms par;
	int val, ret;

	ret = copy_from_user(&par, (void __user *) arg, sizeof(par));
	if (ret) {
		dev_dbg(the_gpadc->dev, "copy_from_user: %d\n", ret);
		return -EACCES;
	}

	switch (cmd) {
	case TWL6030_GPADC_IOCX_ADC_RAW_READ: {
		struct twl6030_gpadc_request req;
		if (par.channel >= TWL6030_GPADC_MAX_CHANNELS)
			return -EINVAL;

		req.channels = (1 << par.channel);
		req.method	= TWL6030_GPADC_SW2;
		req.func_cb	= NULL;

		val = twl6030_gpadc_conversion(&req);
		if (likely(val > 0)) {
			par.status = 0;
			par.result = (u16)req.rbuf[par.channel];
		} else if (val == 0) {
			par.status = -ENODATA;
		} else {
			par.status = val;
		}
		break;
					     }
	default:
		return -EINVAL;
	}

	ret = copy_to_user((void __user *) arg, &par, sizeof(par));
	if (ret) {
		dev_dbg(the_gpadc->dev, "copy_to_user: %d\n", ret);
		return -EACCES;
	}

	return 0;
}

static const struct file_operations twl6030_gpadc_fileops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = twl6030_gpadc_ioctl
};

static struct miscdevice twl6030_gpadc_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "twl6030-gpadc",
	.fops = &twl6030_gpadc_fileops
};

static int __init twl6030_gpadc_probe(struct platform_device *pdev)
{
	struct twl6030_gpadc_data *gpadc;
	struct twl6030_gpadc_platform_data *pdata = pdev->dev.platform_data;
	int ret;

	gpadc = kzalloc(sizeof *gpadc, GFP_KERNEL);
	if (!gpadc)
		return -ENOMEM;

	if (!pdata) {
		dev_dbg(&pdev->dev, "platform_data not available\n");
		ret = -EINVAL;
		goto err_pdata;
	}
	gpadc->dev = &pdev->dev;

	ret = misc_register(&twl6030_gpadc_device);
	if (ret) {
		dev_dbg(&pdev->dev, "could not register misc_device\n");
		goto err_misc;
	}

	ret = request_irq(platform_get_irq(pdev, 0), twl6030_gpadc_irq_handler,
			0, "twl6030_gpadc", &gpadc->requests[TWL6030_GPADC_RT]);
	ret = request_irq(platform_get_irq(pdev, 1), twl6030_gpadc_irq_handler,
		0, "twl6030_gpadc", &gpadc->requests[TWL6030_GPADC_SW2]);

	if (ret) {
		dev_dbg(&pdev->dev, "could not request irq\n");
		goto err_irq;
	}

	platform_set_drvdata(pdev, gpadc);
	mutex_init(&gpadc->lock);
	INIT_WORK(&gpadc->ws, twl6030_gpadc_work);

	the_gpadc = gpadc;

	return 0;

err_irq:
	misc_deregister(&twl6030_gpadc_device);

err_misc:
err_pdata:
	kfree(gpadc);

	return ret;
}

static int __exit twl6030_gpadc_remove(struct platform_device *pdev)
{
	struct twl6030_gpadc_data *gpadc = platform_get_drvdata(pdev);

	twl6030_gpadc_disable_irq(TWL6030_GPADC_RT);
	twl6030_gpadc_disable_irq(TWL6030_GPADC_SW2);
	free_irq(platform_get_irq(pdev, 0), gpadc);
	cancel_work_sync(&gpadc->ws);
	misc_deregister(&twl6030_gpadc_device);

	return 0;
}

static struct platform_driver twl6030_gpadc_driver = {
	.probe		= twl6030_gpadc_probe,
	.remove		= __exit_p(twl6030_gpadc_remove),
	.driver		= {
		.name	= "twl6030_gpadc",
		.owner	= THIS_MODULE,
	},
};

static int __init twl6030_gpadc_init(void)
{
	return platform_driver_register(&twl6030_gpadc_driver);
}
module_init(twl6030_gpadc_init);

static void __exit twl6030_gpadc_exit(void)
{
	platform_driver_unregister(&twl6030_gpadc_driver);
}
module_exit(twl6030_gpadc_exit);

MODULE_ALIAS("platform:twl6030-gpadc");
MODULE_AUTHOR("Texas Instruments Inc.");
MODULE_DESCRIPTION("twl6030 ADC driver");
MODULE_LICENSE("GPL");

