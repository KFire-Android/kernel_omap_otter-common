/*
 * TI Palmas MFD IRQ Driver
 *
 * Copyright 2011 Texas Instruments Inc.
 *
 * Author: Graeme Gregory <gg@slimlogic.co.uk>
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under  the terms of the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the License, or (at your
 *  option) any later version.
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/bug.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/mfd/palmas.h>

static int irq_to_palmas_irq(struct palmas *palmas, int irq)
{
	return irq - palmas->irq_base;
}

/*
 * This is a threaded IRQ handler so can access I2C/SPI.  Since all
 * interrupts are clear on read the IRQ line will be reasserted and
 * the physical IRQ will be handled again if another interrupt is
 * asserted while we run - in the normal course of events this is a
 * rare occurrence so we save I2C/SPI reads.  We're also assuming that
 * it's rare to get lots of interrupts firing simultaneously so try to
 * minimise I/O.
 */
static irqreturn_t palmas_irq(int irq, void *irq_data)
{
	struct palmas *palmas = irq_data;
	u32 irq_sts;
	int slave;
	unsigned int reg, addr;

	slave = PALMAS_BASE_TO_SLAVE(PALMAS_INTERRUPT_BASE);

	addr = PALMAS_BASE_TO_REG(PALMAS_INTERRUPT_BASE, PALMAS_INT1_STATUS);
	regmap_read(palmas->regmap[slave], addr, &reg);
	irq_sts = reg & 0xFF;

	addr = PALMAS_BASE_TO_REG(PALMAS_INTERRUPT_BASE, PALMAS_INT2_STATUS);
	regmap_read(palmas->regmap[slave], addr, &reg);
	irq_sts |= (reg & 0xFF) << 8;

	addr = PALMAS_BASE_TO_REG(PALMAS_INTERRUPT_BASE, PALMAS_INT3_STATUS);
	regmap_read(palmas->regmap[slave], addr, &reg);
	irq_sts |= (reg & 0xFF) << 16;

	addr = PALMAS_BASE_TO_REG(PALMAS_INTERRUPT_BASE, PALMAS_INT4_STATUS);
	regmap_read(palmas->regmap[slave], addr, &reg);
	irq_sts |= (reg & 0xFF) << 24;

	irq_sts &= ~palmas->irq_mask;

	if (!irq_sts)
		return IRQ_NONE;

	while (irq_sts) {
		unsigned long pending = __ffs(irq_sts);
		irq_sts &= ~BIT(pending);
		handle_nested_irq(palmas->irq_base + pending);
	}

	return IRQ_HANDLED;
}

static void palmas_irq_lock(struct irq_data *data)
{
	struct palmas *palmas = irq_data_get_irq_chip_data(data);

	mutex_lock(&palmas->irq_lock);
}

static void palmas_irq_sync_unlock(struct irq_data *data)
{
	struct palmas *palmas = irq_data_get_irq_chip_data(data);
	u32 reg_mask;
	int slave;
	unsigned int reg, addr;

	slave = PALMAS_BASE_TO_SLAVE(PALMAS_INTERRUPT_BASE);

	addr = PALMAS_BASE_TO_REG(PALMAS_INTERRUPT_BASE, PALMAS_INT1_MASK);
	regmap_read(palmas->regmap[slave], addr, &reg);
	reg_mask = reg & 0xFF;

	addr = PALMAS_BASE_TO_REG(PALMAS_INTERRUPT_BASE, PALMAS_INT2_MASK);
	regmap_read(palmas->regmap[slave], addr, &reg);
	reg_mask = (reg & 0xFF) << 8;

	addr = PALMAS_BASE_TO_REG(PALMAS_INTERRUPT_BASE, PALMAS_INT3_MASK);
	regmap_read(palmas->regmap[slave], addr, &reg);
	reg_mask = (reg & 0xFF) << 16;

	addr = PALMAS_BASE_TO_REG(PALMAS_INTERRUPT_BASE, PALMAS_INT4_MASK);
	regmap_read(palmas->regmap[slave], addr, &reg);
	reg_mask = (reg & 0xFF) << 24;

	if (palmas->irq_mask != reg_mask) {
		addr = PALMAS_BASE_TO_REG(PALMAS_INTERRUPT_BASE,
				PALMAS_INT1_MASK);
		reg = palmas->irq_mask & 0xFF;
		regmap_write(palmas->regmap[slave], addr, reg);

		addr = PALMAS_BASE_TO_REG(PALMAS_INTERRUPT_BASE,
				PALMAS_INT2_MASK);
		reg = (palmas->irq_mask >> 8) & 0xFF;
		regmap_write(palmas->regmap[slave], addr, reg);

		addr = PALMAS_BASE_TO_REG(PALMAS_INTERRUPT_BASE,
				PALMAS_INT3_MASK);
		reg = (palmas->irq_mask >> 16) & 0xFF;
		regmap_write(palmas->regmap[slave], addr, reg);

		addr = PALMAS_BASE_TO_REG(PALMAS_INTERRUPT_BASE,
				PALMAS_INT4_MASK);
		reg = (palmas->irq_mask >> 24) & 0xFF;
		regmap_write(palmas->regmap[slave], addr, reg);
	}
	mutex_unlock(&palmas->irq_lock);
}

static void palmas_irq_unmask(struct irq_data *data)
{
	struct palmas *palmas = irq_data_get_irq_chip_data(data);

	palmas->irq_mask &= ~(1 << irq_to_palmas_irq(palmas, data->irq));
}

static void palmas_irq_mask(struct irq_data *data)
{
	struct palmas *palmas = irq_data_get_irq_chip_data(data);

	palmas->irq_mask |= (1 << irq_to_palmas_irq(palmas, data->irq));
}

static struct irq_chip palmas_irq_chip = {
	.name = "palmas",
	.irq_bus_lock = palmas_irq_lock,
	.irq_bus_sync_unlock = palmas_irq_sync_unlock,
	.irq_mask = palmas_irq_mask,
	.irq_unmask = palmas_irq_unmask,
};

int palmas_irq_init(struct palmas *palmas)
{
	int ret, cur_irq;
	unsigned int reg, addr;
	int slave;

	if (!palmas->irq) {
		dev_warn(palmas->dev, "No interrupt support, no core IRQ\n");
		return -EINVAL;
	}

	if (!palmas->irq_base) {
		dev_warn(palmas->dev, "No interrupt support, no IRQ base\n");
		return -EINVAL;
	}

	/* Mask all IRQs by setting all registers to 0xff */
	palmas->irq_mask = 0xffffffff;

	slave = PALMAS_BASE_TO_SLAVE(PALMAS_INTERRUPT_BASE);

	reg = 0xFF;

	addr = PALMAS_BASE_TO_REG(PALMAS_INTERRUPT_BASE, PALMAS_INT1_MASK);
	regmap_write(palmas->regmap[slave], addr, reg);

	addr = PALMAS_BASE_TO_REG(PALMAS_INTERRUPT_BASE, PALMAS_INT2_MASK);
	regmap_write(palmas->regmap[slave], addr, reg);

	addr = PALMAS_BASE_TO_REG(PALMAS_INTERRUPT_BASE, PALMAS_INT3_MASK);
	regmap_write(palmas->regmap[slave], addr, reg);

	addr = PALMAS_BASE_TO_REG(PALMAS_INTERRUPT_BASE, PALMAS_INT4_MASK);
	regmap_write(palmas->regmap[slave], addr, reg);

	/* Change into clear on read mode for efficiency */
	addr = PALMAS_BASE_TO_REG(PALMAS_INTERRUPT_BASE, PALMAS_INT_CTRL);
	reg = INT_CTRL_INT_CLEAR;

	regmap_write(palmas->regmap[slave], addr, reg);

	/* Clear any pendings IRQs */
	addr = PALMAS_BASE_TO_REG(PALMAS_INTERRUPT_BASE, PALMAS_INT1_STATUS);
	regmap_read(palmas->regmap[slave], addr, &reg);

	addr = PALMAS_BASE_TO_REG(PALMAS_INTERRUPT_BASE, PALMAS_INT2_STATUS);
	regmap_read(palmas->regmap[slave], addr, &reg);

	addr = PALMAS_BASE_TO_REG(PALMAS_INTERRUPT_BASE, PALMAS_INT3_STATUS);
	regmap_read(palmas->regmap[slave], addr, &reg);

	addr = PALMAS_BASE_TO_REG(PALMAS_INTERRUPT_BASE, PALMAS_INT4_STATUS);
	regmap_read(palmas->regmap[slave], addr, &reg);

	mutex_init(&palmas->irq_lock);

	/* Register with genirq */
	for (cur_irq = palmas->irq_base;
	     cur_irq < PALMAS_NUM_IRQ + palmas->irq_base;
	     cur_irq++) {
		irq_set_chip_data(cur_irq, palmas);
		irq_set_chip_and_handler(cur_irq, &palmas_irq_chip,
					 handle_edge_irq);
		irq_set_nested_thread(cur_irq, 1);

		/* ARM needs us to explicitly flag the IRQ as valid
		 * and will set them noprobe when we do so. */
#ifdef CONFIG_ARM
		set_irq_flags(cur_irq, IRQF_VALID);
#else
		irq_set_noprobe(cur_irq);
#endif
	}

	ret = request_threaded_irq(palmas->irq, NULL, palmas_irq, IRQF_ONESHOT,
				   "palmas-irq", palmas);

	irq_set_irq_type(palmas->irq, IRQ_TYPE_LEVEL_LOW);

	if (ret != 0)
		dev_err(palmas->dev, "Failed to request IRQ: %d\n", ret);

	return ret;
}

int palmas_irq_exit(struct palmas *palmas)
{
	free_irq(palmas->irq, palmas);
	return 0;
}
