/*
 * tlv320aic31xx-irq.c  --  Interrupt controller support for
 *			TI TLV320AIC31XX
 *
 * Author:	Mukund Navada <navada@ti.com>
 *		Mehar Bajwa <mehar.bajwa@ti.com>
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

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/irq.h>
#include <linux/mfd/core.h>
#include <linux/interrupt.h>

#include <linux/mfd/tlv320aic3xxx-core.h>
#include <linux/mfd/tlv320aic3xxx-registers.h>
#include <linux/mfd/tlv320aic31xx-registers.h>

#include <linux/delay.h>

struct aic3xxx_irq_data {
	int mask;
	int status;
};

static struct aic3xxx_irq_data aic3xxx_irqs[] = {
	{
	.mask = AIC3XXX_HEADSET_IN_M,
	.status = AIC3XXX_HEADSET_PLUG_UNPLUG_INT,
	 },
	{
	.mask = AIC3XXX_BUTTON_PRESS_M,
	.status = AIC3XXX_BUTTON_PRESS_INT,
	 },
	{
	.mask = AIC3XXX_DAC_DRC_THRES_M,
	.status = AIC3XXX_LEFT_DRC_THRES_INT | AIC3XXX_RIGHT_DRC_THRES_INT,
	 },
	{
	.mask = AIC3XXX_AGC_NOISE_M,
	.status = AIC3XXX_AGC_NOISE_INT,
	 },
	{
	.mask = AIC3XXX_OVER_CURRENT_M,
	.status = AIC3XXX_LEFT_OUTPUT_DRIVER_OVERCURRENT_INT |
			AIC3XXX_RIGHT_OUTPUT_DRIVER_OVERCURRENT_INT,
	},
	{
	.mask = AIC3XXX_OVERFLOW_M,
	.status = AIC3XXX_LEFT_DAC_OVERFLOW_INT |
			AIC3XXX_RIGHT_DAC_OVERFLOW_INT |
			AIC3XXX_MINIDSP_D_BARREL_SHIFT_OVERFLOW_INT |
			AIC3XXX_ADC_OVERFLOW_INT |
			AIC3XXX_MINIDSP_A_BARREL_SHIFT_OVERFLOW_INT,
	},
};


static inline struct aic3xxx_irq_data *irq_to_aic3xxx_irq(struct aic3xxx
							*aic3xxx, int irq)
{
	return &aic3xxx_irqs[irq - aic3xxx->irq_base];
}

static void aic3xxx_irq_lock(struct irq_data *data)
{
	struct aic3xxx *aic3xxx = irq_data_get_irq_chip_data(data);

	mutex_lock(&aic3xxx->irq_lock);
}

static void aic3xxx_irq_sync_unlock(struct irq_data *data)
{

	struct aic3xxx *aic3xxx = irq_data_get_irq_chip_data(data);

	/* write back to hardware any change in irq mask */
	if (aic3xxx->irq_masks_cur != aic3xxx->irq_masks_cache) {
		aic3xxx->irq_masks_cache = aic3xxx->irq_masks_cur;
		aic3xxx_reg_write(aic3xxx, AIC3XXX_INT1_CNTL,
				aic3xxx->irq_masks_cur);
	}

	mutex_unlock(&aic3xxx->irq_lock);
}


static void aic3xxx_irq_unmask(struct irq_data *data)
{
	struct aic3xxx *aic3xxx = irq_data_get_irq_chip_data(data);
	struct aic3xxx_irq_data *irq_data =
				irq_to_aic3xxx_irq(aic3xxx, data->irq);

	aic3xxx->irq_masks_cur |= irq_data->mask;
}

static void aic3xxx_irq_mask(struct irq_data *data)
{
	struct aic3xxx *aic3xxx = irq_data_get_irq_chip_data(data);
	struct aic3xxx_irq_data *irq_data =
				irq_to_aic3xxx_irq(aic3xxx, data->irq);

	aic3xxx->irq_masks_cur &= ~irq_data->mask;
}


static struct irq_chip aic3xxx_irq_chip = {

	.name = "tlv320aic31xx",
	.irq_bus_lock = aic3xxx_irq_lock,
	.irq_bus_sync_unlock = aic3xxx_irq_sync_unlock,
	.irq_mask = aic3xxx_irq_mask,
	.irq_unmask = aic3xxx_irq_unmask,
};

static irqreturn_t aic3xxx_irq_thread(int irq, void *data)
{

	struct aic3xxx *aic3xxx = data;
	u8 status[4];
	u8 overflow_status = 0;

	/* Reading sticky bit registers acknowledges
		the interrupt to the device */
	aic3xxx_bulk_read(aic3xxx, AIC3XXX_INT_STICKY_FLAG2, 4, status);
	printk(KERN_INFO"aic3xxx_irq_thread %x\n", status[2]);

	/* report  */
	if (status[2] & aic3xxx_irqs[AIC31XX_IRQ_HEADSET_DETECT].status)
		handle_nested_irq(aic3xxx->irq_base);
	if (status[2] & aic3xxx_irqs[AIC31XX_IRQ_BUTTON_PRESS].status)
		handle_nested_irq(aic3xxx->irq_base + 1);
	if (status[2] & aic3xxx_irqs[AIC31XX_IRQ_DAC_DRC].status)
		handle_nested_irq(aic3xxx->irq_base + 2);
	if (status[2] & aic3xxx_irqs[AIC31XX_IRQ_AGC_NOISE].status)
		handle_nested_irq(aic3xxx->irq_base + 3);
	if (status[0] & aic3xxx_irqs[AIC31XX_IRQ_OVER_CURRENT].status)
		handle_nested_irq(aic3xxx->irq_base + 4);
	if (overflow_status & aic3xxx_irqs[AIC31XX_IRQ_OVERFLOW_EVENT].status)
		handle_nested_irq(aic3xxx->irq_base + 5);


	/* ack unmasked irqs */
	/* No need to acknowledge the interrupt on AIC3xxx */

	return IRQ_HANDLED;
}

int aic3xxx_irq_init(struct aic3xxx *aic3xxx)
{
	int ret;
	unsigned int cur_irq;
	mutex_init(&aic3xxx->irq_lock);

	aic3xxx->irq_masks_cur = 0x0;
	aic3xxx->irq_masks_cache = 0x0;
	aic3xxx_reg_write(aic3xxx, AIC3XXX_INT1_CNTL, 0x00);
	if (!aic3xxx->irq) {
		dev_warn(aic3xxx->dev,
				"no interrupt specified, no interrupts\n");
		aic3xxx->irq_base = 0;
		return 0;
	}

	if (!aic3xxx->irq_base) {
		dev_err(aic3xxx->dev,
				"no interrupt base specified, no interrupts\n");
		return 0;
	}


	/* Register them with genirq */
	for (cur_irq = aic3xxx->irq_base;
		cur_irq < aic3xxx->irq_base + ARRAY_SIZE(aic3xxx_irqs);
		cur_irq++) {
		irq_set_chip_data(cur_irq, aic3xxx);
		irq_set_chip_and_handler(cur_irq, &aic3xxx_irq_chip,
				handle_edge_irq);
		irq_set_nested_thread(cur_irq, 1);

		/* ARM needs us to explicitly flag the IRQ as valid
		 * and will set them noprobe when we do so. */
#ifdef CONFIG_ARM
		set_irq_flags(cur_irq, IRQF_VALID);
#else
		set_irq_noprobe(cur_irq);
#endif
	}

	ret = request_threaded_irq(aic3xxx->irq, NULL, aic3xxx_irq_thread,
				IRQF_TRIGGER_RISING,
				"tlv320aic31xx", aic3xxx);


	if (ret < 0) {
		dev_err(aic3xxx->dev, "failed to request IRQ %d: %d\n",
			aic3xxx->irq, ret);
		return ret;
	}

	return 0;

}
EXPORT_SYMBOL(aic3xxx_irq_init);

void aic3xxx_irq_exit(struct aic3xxx *aic3xxx)
{
	if (aic3xxx->irq)
		free_irq(aic3xxx->irq, aic3xxx);
}
EXPORT_SYMBOL(aic3xxx_irq_exit);

MODULE_DESCRIPTION("Interrupt controller support for TI TLV320AIC31XX");
MODULE_LICENSE("GPL");
