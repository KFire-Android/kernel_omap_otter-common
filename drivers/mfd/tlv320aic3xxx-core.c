/*
*
 * tlv320aic31xx-core.c  -- driver for TLV320AIC31XX
 *
 * Author:      Mukund Navada <navada@ti.com>
 *              Mehar Bajwa <mehar.bajwa@ti.com>
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
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/err.h>
#include <linux/delay.h>
#include <linux/mfd/core.h>
#include <linux/pm_runtime.h>
#include <linux/regulator/consumer.h>
#include <linux/regulator/machine.h>
#include <linux/gpio.h>

#include <linux/mfd/tlv320aic3xxx-core.h>
#include <linux/mfd/tlv320aic3xxx-registers.h>
#include <linux/mfd/tlv320aic31xx-registers.h>

int set_aic3xxx_book(struct aic3xxx *aic3xxx, int book)
{
	int ret = 0;
	u8 page_buf[] = { 0x0, 0x0 };
	u8 book_buf[] = { 0x0, 0x0 };

	ret = aic3xxx_i2c_write_device(aic3xxx, page_buf[0], &page_buf[1], 1);

	if (ret < 0)
		return ret;
	book_buf[1] = book;

	ret = aic3xxx_i2c_write_device(aic3xxx, book_buf[0], &book_buf[1], 1);

	if (ret < 0)
		return ret;
	aic3xxx->book_no = book;
	aic3xxx->page_no = 0;

	return ret;
}

int set_aic3xxx_page(struct aic3xxx *aic3xxx, int page)
{
	int ret = 0;
	u8 page_buf[] = { 0x0, 0x0 };

	page_buf[1] = page;
	ret = aic3xxx_i2c_write_device(aic3xxx, page_buf[0], &page_buf[1], 1);

	if (ret < 0)
		return ret;
	aic3xxx->page_no = page;
	return ret;
}
/**
 * aic3xxx_reg_read: Read a single TLV320AIC31xx register.
 *
 * @aic3xxx: Device to read from.
 * @reg: Register to read.
 */
int aic3xxx_reg_read(struct aic3xxx *aic3xxx, unsigned int reg)
{
	unsigned char val;
	int ret;
	aic31xx_reg_union *aic_reg = (aic31xx_reg_union *) &reg;
	u8 book, page, offset;

	page = aic_reg->aic3xxx_register.page;
	book = aic_reg->aic3xxx_register.book;
	offset = aic_reg->aic3xxx_register.offset;

	mutex_lock(&aic3xxx->io_lock);
	if (aic3xxx->book_no != book) {
		ret = set_aic3xxx_book(aic3xxx, book);
		if (ret < 0) {
			mutex_unlock(&aic3xxx->io_lock);
			return ret;
		}
	}
	if (aic3xxx->page_no != page) {
		ret = set_aic3xxx_page(aic3xxx, page);
		if (ret < 0) {
			mutex_unlock(&aic3xxx->io_lock);
			return ret;
		}
	}
	ret = aic3xxx_i2c_read_device(aic3xxx, offset, &val, 1);
	mutex_unlock(&aic3xxx->io_lock);

	if (ret < 0)
		return ret;
	else
		return val;
}
EXPORT_SYMBOL_GPL(aic3xxx_reg_read);

/**
 * aic3xxx_bulk_read: Read multiple TLV320AIC31xx registers
 *
 * @aic3xxx: Device to read from
 * @reg: First register
 * @count: Number of registers
 * @buf: Buffer to fill.  The data will be returned big endian.
 */
int aic3xxx_bulk_read(struct aic3xxx *aic3xxx, unsigned int reg,
			int count, u8 *buf)
{
	int ret;
	aic31xx_reg_union *aic_reg = (aic31xx_reg_union *) &reg;
	u8 book, page, offset;

	page = aic_reg->aic3xxx_register.page;
	book = aic_reg->aic3xxx_register.book;
	offset = aic_reg->aic3xxx_register.offset;

	mutex_lock(&aic3xxx->io_lock);
	if (aic3xxx->book_no != book) {
		ret = set_aic3xxx_book(aic3xxx, book);
		if (ret < 0) {
			mutex_unlock(&aic3xxx->io_lock);
			return ret;
		}
	}

	if (aic3xxx->page_no != page) {
		ret = set_aic3xxx_page(aic3xxx, page);
		if (ret < 0) {
			mutex_unlock(&aic3xxx->io_lock);
			return ret;
		}
	}
	ret = aic3xxx_i2c_read_device(aic3xxx, offset, buf, count);
	mutex_unlock(&aic3xxx->io_lock);
		return ret;
}
EXPORT_SYMBOL_GPL(aic3xxx_bulk_read);

/**
 * aic3xxx_reg_write: Write a single TLV320AIC31xx register.
 *
 * @aic3xxx: Device to write to.
 * @reg: Register to write to.
 * @val: Value to write.
 */
int aic3xxx_reg_write(struct aic3xxx *aic3xxx, unsigned int reg,
			unsigned char val)
{
	aic31xx_reg_union *aic_reg = (aic31xx_reg_union *) &reg;
	int ret = 0;
	u8 page, book, offset;

	page = aic_reg->aic3xxx_register.page;
	book = aic_reg->aic3xxx_register.book;
	offset = aic_reg->aic3xxx_register.offset;

	mutex_lock(&aic3xxx->io_lock);
	if (book != aic3xxx->book_no) {
		ret = set_aic3xxx_book(aic3xxx, book);
		if (ret < 0) {
			mutex_unlock(&aic3xxx->io_lock);
			return ret;
		}
	}
	if (page != aic3xxx->page_no) {
		ret = set_aic3xxx_page(aic3xxx, page);
		if (ret < 0) {
			mutex_unlock(&aic3xxx->io_lock);
			return ret;
		}
	}
	ret = aic3xxx_i2c_write_device(aic3xxx, offset, &val, 1);

	mutex_unlock(&aic3xxx->io_lock);
	return ret;

}
EXPORT_SYMBOL_GPL(aic3xxx_reg_write);

/**
 * aic3xxx_bulk_write: Write multiple TLV320AIC31xx registers
 *
 * @aic3xxx: Device to write to
 * @reg: First register
 * @count: Number of registers
 * @buf: Buffer to write from.  Data must be big-endian formatted.
 */
int aic3xxx_bulk_write(struct aic3xxx *aic3xxx, unsigned int reg,
			int count, const u8 *buf)
{
	aic31xx_reg_union *aic_reg = (aic31xx_reg_union *) &reg;
	int ret = 0;
	u8 page, book, offset;

	page = aic_reg->aic3xxx_register.page;
	book = aic_reg->aic3xxx_register.book;
	offset = aic_reg->aic3xxx_register.offset;

	mutex_lock(&aic3xxx->io_lock);
	if (book != aic3xxx->book_no) {
		ret = set_aic3xxx_book(aic3xxx, book);
		if (ret < 0) {
			mutex_unlock(&aic3xxx->io_lock);
			return ret;
		}
	}
	if (page != aic3xxx->page_no) {
		ret = set_aic3xxx_page(aic3xxx, page);
		if (ret < 0) {
			mutex_unlock(&aic3xxx->io_lock);
			return ret;
		}
	}
	ret = aic3xxx_i2c_write_device(aic3xxx, offset, buf, count);
	mutex_unlock(&aic3xxx->io_lock);
	return ret;
}
EXPORT_SYMBOL_GPL(aic3xxx_bulk_write);

/**
 * aic3xxx_set_bits: Set the value of a bitfield in a TLV320AIC31xx register
 *
 * @aic3xxx: Device to write to.
 * @reg: Register to write to.
 * @mask: Mask of bits to set.
 * @val: Value to set (unshifted)
 */
int aic3xxx_set_bits(struct aic3xxx *aic3xxx, unsigned int reg,
			unsigned char mask, unsigned char val)
{
	aic31xx_reg_union *aic_reg = (aic31xx_reg_union *) &reg;
	int ret = 0;
	u8 page, book, offset, r;

	page = aic_reg->aic3xxx_register.page;
	book = aic_reg->aic3xxx_register.book;
	offset = aic_reg->aic3xxx_register.offset;

	mutex_lock(&aic3xxx->io_lock);
	if (book != aic3xxx->book_no) {
		ret = set_aic3xxx_book(aic3xxx, book);
		if (ret < 0) {
			mutex_unlock(&aic3xxx->io_lock);
			return ret;
		}
	}
	if (page != aic3xxx->page_no) {
		ret = set_aic3xxx_page(aic3xxx, page);
		if (ret < 0) {
			mutex_unlock(&aic3xxx->io_lock);
			return ret;
		}
	}

	ret = aic3xxx_i2c_read_device(aic3xxx, offset, &r, 1);
	if (ret < 0)
		goto out;


	r &= ~mask;
	r |= (val & mask);

	ret = aic3xxx_i2c_write_device(aic3xxx, offset , &r, 1);

out:
	mutex_unlock(&aic3xxx->io_lock);

	return ret;
}
EXPORT_SYMBOL_GPL(aic3xxx_set_bits);

/**
 * aic3xxx_wait_bits: wait for a value of a bitfield in a TLV320AIC31xx register
 *
 * @aic3xxx: Device to write to.
 * @reg: Register to write to.
 * @mask: Mask of bits to set.
 * @val: Value to set (unshifted)
 * @mdelay: mdelay value in each iteration in milliseconds
 * @count: iteration count for timeout
 */
int aic3xxx_wait_bits(struct aic3xxx *aic3xxx, unsigned int reg,
			unsigned char mask, unsigned char val, int sleep,
			int counter)
{
	unsigned int status;
	int timeout = sleep * counter;
	int ret;
	status = aic3xxx_reg_read(aic3xxx, reg);
	while (((status & mask) != val) && counter) {
		usleep_range(sleep, sleep + 100);
		ret = aic3xxx_reg_read(aic3xxx, reg);
		counter--;
	};
	if (!counter)
		dev_err(aic3xxx->dev,
			"wait_bits timedout (%d millisecs). lastval 0x%x\n",
			timeout, status);
	return counter;
}
EXPORT_SYMBOL_GPL(aic3xxx_wait_bits);

static struct mfd_cell aic3xxx_devs[] = {
	{
	.name = "tlv320aic31xx-codec",
	},
	{
	.name = "tlv320aic31xx-gpio",
	},
};

/**
 * Instantiate the generic non-control parts of the device.
 */
int aic3xxx_device_init(struct aic3xxx *aic3xxx)
{
	int ret, i;
	u8 resetVal = 1;

	dev_info(aic3xxx->dev, "aic3xxx_device_init beginning\n");

	mutex_init(&aic3xxx->io_lock);
	dev_set_drvdata(aic3xxx->dev, aic3xxx);

	if (dev_get_platdata(aic3xxx->dev))
		memcpy(&aic3xxx->pdata, dev_get_platdata(aic3xxx->dev),
			sizeof(aic3xxx->pdata));

	/*GPIO reset for TLV320AIC31xx codec */
	if (aic3xxx->pdata.gpio_reset) {
		ret = gpio_request(aic3xxx->pdata.gpio_reset,
				"aic31xx-reset-pin");
		if (ret != 0) {
			dev_err(aic3xxx->dev, "not able to acquire gpio\n");
			goto err_return;
		}
		gpio_direction_output(aic3xxx->pdata.gpio_reset, 1);
		mdelay(5);
		gpio_direction_output(aic3xxx->pdata.gpio_reset, 0);
		mdelay(5);
		gpio_direction_output(aic3xxx->pdata.gpio_reset, 1);
		mdelay(5);
	}

	/* run the codec through software reset */
	ret = aic3xxx_reg_write(aic3xxx, AIC3XXX_RESET, resetVal);
	if (ret < 0) {
		dev_err(aic3xxx->dev, "Could not write to AIC31xx register\n");
		goto err_return;
	}

	mdelay(10);
	ret = aic3xxx_reg_read(aic3xxx, AIC3XXX_REV_PG_ID);
	if (ret < 0) {
		dev_err(aic3xxx->dev, "Failed to read ID register\n");
		goto err_return;
	}

	dev_info(aic3xxx->dev, "aic3xxx revision %d\n", ret);

	/*If naudint is gpio convert it to irq number */
	if (aic3xxx->pdata.gpio_irq == 1) {
		aic3xxx->irq = gpio_to_irq(aic3xxx->pdata.naudint_irq);
		gpio_request(aic3xxx->pdata.naudint_irq, "aic31xx-gpio-irq");
		gpio_direction_input(aic3xxx->pdata.naudint_irq);
	} else {
		aic3xxx->irq = aic3xxx->pdata.naudint_irq;
	}

	for (i = 0; i < aic3xxx->pdata.num_gpios; i++) {
		aic3xxx_reg_write(aic3xxx, aic3xxx->pdata.gpio_defaults[i].reg,
			aic3xxx->pdata.gpio_defaults[i].value);
	}

	aic3xxx->irq_base = aic3xxx->pdata.irq_base;

	/* codec interrupt */
	if (aic3xxx->irq) {
		ret = aic3xxx_irq_init(aic3xxx);
		if (ret < 0)
			goto err_irq;
	}

	ret = mfd_add_devices(aic3xxx->dev, -1,
				aic3xxx_devs, ARRAY_SIZE(aic3xxx_devs),
				NULL, 0);
	if (ret != 0) {
		dev_err(aic3xxx->dev, "Failed to add children: %d\n", ret);
		goto err_mfd;
	}
	dev_info(aic3xxx->dev, "aic3xxx_device_init added mfd devices\n");
	
	return 0;

err_mfd:
	aic3xxx_irq_exit(aic3xxx);
err_irq:

	if (aic3xxx->pdata.gpio_irq)
		gpio_free(aic3xxx->pdata.naudint_irq);
err_return:

	if (aic3xxx->pdata.gpio_reset)
		gpio_free(aic3xxx->pdata.gpio_reset);

	return ret;
}
EXPORT_SYMBOL_GPL(aic3xxx_device_init);

void aic3xxx_device_exit(struct aic3xxx *aic3xxx)
{

	mfd_remove_devices(aic3xxx->dev);

	aic3xxx_irq_exit(aic3xxx);


	if (aic3xxx->pdata.gpio_irq)
		gpio_free(aic3xxx->pdata.naudint_irq);
	if (aic3xxx->pdata.gpio_reset)
		gpio_free(aic3xxx->pdata.gpio_reset);

}
EXPORT_SYMBOL_GPL(aic3xxx_device_exit);

MODULE_AUTHOR("Mukund Navada <navada@ti.comm>");
MODULE_AUTHOR("Mehar Bajwa <mehar.bajwa@ti.com>");
MODULE_DESCRIPTION("Core support for the TLV320AIC3XXX audio CODEC");
MODULE_LICENSE("GPL");
