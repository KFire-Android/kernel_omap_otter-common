/*
 * linux/sound/soc/codecs/tlv320aic31xx.c
 *
 *
 * Copyright (C) 2014 Texas Instruments, Inc.
 *
 * This package is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * THIS PACKAGE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 * History:
 *
 * Rev 0.1   ASoC driver support			14-04-2010
 *
 * Rev 0.2   Updated based Review Comments		29-06-2010
 *
 * Rev 0.3   Updated for Codec Family Compatibility     12-07-2010
 *
 * Rev 0.4   Ported to 2.6.35 kernel
 *
 * Rev 0.5   Updated the aic31xx_power_up(), aic31xx_power_down() and
 *           aic31xx_mute_codec() functions to enable and disable the ADC
 *           related registers.
 *
 * Rev 0.6   Updated the PLL Settings and also updated the Common Mode Gain
 *           Settings for better recording volume.
 *
 * Rev 0.7   updated the aic31xx_headset_speaker_path() function to check for
 *	     both playback and record. During record, if the headset jack is
 *	     removed, then the Audio Codec will be powered down.
 *
 * Rev 0.8   updated the aic31xx_hw_params() and aic31xx_set_bias_level()
 *	     functions to check the jack status before starting recording.
 *           Added the aic31xx_mic_check() function to check for the Jack Type
 *	     before allowing Audio Recording.
 *
 * Rev 0.9   Updated the HEADPHONE_DRIVER Register to have CM Voltage Settings
 *	     of 1.5V
 *
 * Rev 1.0   Implemented the DAPM support for power management and simultaneous
 *	     playback and capture support is provided
 *
 * Rev 1.1   Ported the driver to Linux 3.0 Kernel

 * Rev 1.2   Ported the driver to Linux 3.4 Kernel
 */

/******************************************************************************
 * INCLUDE HEADER FILES
 *****************************************************************************/
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/clk.h>
#include <linux/gpio.h>
#include <linux/cdev.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/input.h>
#include <linux/irq.h>

#include <linux/regulator/consumer.h>
#include <linux/regulator/machine.h>

#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/soc-dai.h>
#include <sound/soc-dapm.h>
#include <sound/initval.h>
#include <sound/tlv.h>
#include <sound/jack.h>
#include <asm/div64.h>

#include "tlv320aic31xx.h"
#include <mach/gpio.h>
#include <plat/io.h>


/******************************************************************************
 * GLOBAL VARIABLES
 *****************************************************************************/

static struct i2c_client *tlv320aic31xx_client;
struct regulator *audio_regulator;
static struct i2c_board_info tlv320aic31xx_hwmon_info = {
	I2C_BOARD_INFO("tlv320aic3110", 0x18),
};


/******************************************************************************
				Function Prototypes
*******************************************************************************/

/* Device I/O API */
int aic3xxx_reg_read(struct aic3xxx *aic3xxx, unsigned int reg);
int aic3xxx_reg_write(struct aic3xxx *aic3xxx, unsigned int reg,
                unsigned char val);
int aic3xxx_set_bits(struct aic3xxx *aic3xxx, unsigned int reg,
                unsigned char mask, unsigned char val);
int aic3xxx_wait_bits(struct aic3xxx *aic3xxx, unsigned int reg,
                      unsigned char mask, unsigned char val, int delay,
                      int counter);

int aic3xxx_i2c_read_device(struct aic3xxx *aic3xxx, u8 offset,
                                void *dest, int count);
int aic3xxx_i2c_write_device(struct aic3xxx *aic3xxx, u8 offset,
                                const void *src, int count);

void aic31xx_hs_jack_detect(struct snd_soc_codec *codec,
                                struct snd_soc_jack *jack, int report);
static int aic31xx_mute_codec(struct snd_soc_codec *codec, int mute);
static int aic31xx_mute(struct snd_soc_dai *dai, int mute);

static int aic31xx_mute(struct snd_soc_dai *dai, int mute);

static int aic31xx_set_dai_fmt(struct snd_soc_dai *codec_dai,
			unsigned int fmt);

static int aic31xx_set_bias_level(struct snd_soc_codec *codec,
		enum snd_soc_bias_level level);

/*
 * Global Variables introduced to reduce Headphone Analog Volume Control
 * Registers at run-time
 */
struct i2c_msg i2c_right_transaction[120];
struct i2c_msg i2c_left_transaction[120];

int aic3xxx_i2c_read_device(struct aic3xxx *aic3xxx, u8 offset,
				void *dest, int count)
{
	struct i2c_client *i2c = to_i2c_client(aic3xxx->dev);
	int ret;
	ret = i2c_master_send(i2c, &offset, 1);
	if (ret < 0)
		return ret;

	if (ret != 1)
		return -EIO;


	ret = i2c_master_recv(i2c, dest, count);
	if (ret < 0)
		return ret;

	if (ret != count)
		return -EIO;
	return ret;
}

int aic3xxx_i2c_write_device(struct aic3xxx *aic3xxx , u8 offset,
				const void *src, int count)
{
	struct i2c_client *i2c = to_i2c_client(aic3xxx->dev);
	u8 write_buf[count+1];
	int ret;

	write_buf[0] = offset;
	memcpy(&write_buf[1], src, count);
	ret = i2c_master_send(i2c, write_buf, count + 1);

	if (ret < 0)
		return ret;
	if (ret != (count + 1))
		return -EIO;

	return 0;
}

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

/*
 *----------------------------------------------------------------------------
 * Function : aic31xx_codec_read
 * Purpose  : This function is to read the aic31xx register space.
 *
 *----------------------------------------------------------------------------
 */
unsigned int aic31xx_codec_read(struct snd_soc_codec *codec, unsigned int reg)
{

	u8 value;
	aic31xx_reg_union *aic_reg = (aic31xx_reg_union *)&reg;
	value = aic3xxx_reg_read(codec->control_data, reg);
	dev_dbg(codec->dev, "p%d , r 30%x %x\n", aic_reg->aic3xxx_register.page,
			aic_reg->aic3xxx_register.offset, value);
	return value;
}

/*
 *----------------------------------------------------------------------------
 * Function : aic31xx_codec_write
 * Purpose  : This function is to write to the aic31xx register space.
 *
 *----------------------------------------------------------------------------
 */
int aic31xx_codec_write(struct snd_soc_codec *codec, unsigned int reg,
			unsigned int value)
{
	aic31xx_reg_union *aic_reg = (aic31xx_reg_union *)&reg;
	dev_dbg(codec->dev, "p %d, w 30 %x %x\n",
			aic_reg->aic3xxx_register.page,
			aic_reg->aic3xxx_register.offset, value);
	return aic3xxx_reg_write(codec->control_data, reg, value);
}

/*
 * Debug routine to dump all the Registers of Page 0
*/
void debug_print_registers(struct snd_soc_codec *codec)
{
	unsigned int i;
	u32 data;

	for (i = 0 ; i < 118 ; i++) {
		data = snd_soc_read(codec, i);
		dev_dbg(codec->dev, "reg = %d val = %x\n", i, data);
	}
	/* for ADC registers */
	dev_dbg(codec->dev, "*** Page 1:\n");
	for (i = AIC31XX_HPHONE_DRIVERS ; i <= AIC31XX_MICPGA_CM_REG ; i++) {
		data = snd_soc_read(codec, i);
		dev_dbg(codec->dev, "reg = %d val = %x\n", i, data);
	}
}

void debug_print_one_register(struct snd_soc_codec *codec, unsigned int i)
{
	u32 data;
	data = snd_soc_read(codec, i);
	dev_dbg(codec->dev, "reg = %d val = %x\n", i, data);

}


/**
 *aic31xx_dac_power_up_event: Headset popup reduction and powering up dsps together
 *			when they are in sync mode
 * @w: pointer variable to dapm_widget
 * @kcontrol: pointer to sound control
 * @event: event element information
 *
 * Returns 0 for success.
 */
static int aic31xx_dac_power_up_event(struct snd_soc_dapm_widget *w,
				struct snd_kcontrol *kcontrol, int event)
{
	return 0;
}

/**
 * aic31xx_adc_power_up_event: To get DSP run state to perform synchronization
 * @w: pointer variable to dapm_widget
 * @kcontrol: pointer to sound control
 * @event: event element information
 *
 * Returns 0 for success.
 */
static int aic31xx_adc_power_up_event(struct snd_soc_dapm_widget *w,
			struct snd_kcontrol *kcontrol, int event)
{
	return 0;
}

/*Hp_power_up_event without powering on/off headphone driver,
* instead muting hpl & hpr */

static int aic31xx_hp_power_up_event(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	u8 lv, rv, val;
	struct snd_soc_codec *codec = w->codec;

	if (event & SND_SOC_DAPM_POST_PMU) {

		if (!(strcmp(w->name, "HPL Driver"))) {
			lv = snd_soc_read(codec, AIC31XX_LEFT_ANALOG_HPL);
			lv |= 0x7f;
			rv = snd_soc_read(codec, AIC31XX_RIGHT_ANALOG_HPR);
			rv |= 0x7f;
			val = lv ^ ((lv ^ rv) & -(lv < rv));
			while (val > 9) {
				snd_soc_write(codec, AIC31XX_LEFT_ANALOG_HPL,
						(0x80 | (val & 0x7f)));
				snd_soc_write(codec, AIC31XX_RIGHT_ANALOG_HPR,
						(0x80 | (val & 0x7f)));
				val--;
			}
		}

	}

	if (event & SND_SOC_DAPM_PRE_PMD) {

		if (!(strcmp(w->name, "HPL Driver"))) {
			lv = snd_soc_read(codec, AIC31XX_LEFT_ANALOG_HPL);
			lv &= 0x7f;
			rv = snd_soc_read(codec, AIC31XX_RIGHT_ANALOG_HPR);
			rv &= 0x7f;
			val = lv ^ ((lv ^ rv) & -(lv < rv));
			while (val < 127) {
				snd_soc_write(codec, AIC31XX_LEFT_ANALOG_HPL,
						(0x80 | (val & 0x7f)));
				snd_soc_write(codec, AIC31XX_RIGHT_ANALOG_HPR,
						(0x80 | (val & 0x7f)));
				val++;
			}
			/* The D7 bit is set to zero to mute the gain */
			snd_soc_write(codec, AIC31XX_LEFT_ANALOG_HPL,
						(val & 0x7f));
			snd_soc_write(codec, AIC31XX_RIGHT_ANALOG_HPR,
						(val & 0x7f));
		}

	}

	return 0;
}


static int aic31xx_sp_event(struct snd_soc_dapm_widget *w,
			struct snd_kcontrol *kcontrol, int event)
{
	int lv, rv, val;
	struct snd_soc_codec *codec = w->codec;
	int ret_wbits = 0;
	unsigned int reg_mask = 0;

	if (event & SND_SOC_DAPM_POST_PMU) {
		/* Check for the DAC FLAG register to know if the SPL & SPR are
		 * really powered up
		 */
		if (w->shift == 7)
			ret_wbits = aic3xxx_wait_bits(codec->control_data,
						AIC31XX_DAC_FLAG_1, reg_mask,
						0x0,AIC31XX_TIME_DELAY,
						AIC31XX_DELAY_COUNTER);
			if (!ret_wbits)
				dev_dbg(codec->dev, "SPL power timedout\n");


		if (w->shift == 6)
			ret_wbits = aic3xxx_wait_bits(codec->control_data,
						AIC31XX_DAC_FLAG_1, reg_mask,
						0x0,AIC31XX_TIME_DELAY,
						AIC31XX_DELAY_COUNTER);
			if (!ret_wbits)
				dev_dbg(codec->dev, "SPR power timedout\n");

		if (!(strcmp(w->name, "SPL Class - D"))) {
			lv = snd_soc_read(codec,
						AIC31XX_LEFT_ANALOG_SPL);
			lv |= 0x7f;
			rv = snd_soc_read(codec, AIC31XX_RIGHT_ANALOG_SPR);
			rv |= 0x7f;
			val = lv ^ ((lv ^ rv) & -(lv < rv));
			while (val >= 0) {
				snd_soc_write(codec, AIC31XX_LEFT_ANALOG_SPL,
						(0x80 | (val & 0x7f)));
				snd_soc_write(codec, AIC31XX_RIGHT_ANALOG_SPR,
						(0x80 | (val & 0x7f)));
				val--;
			}
		}
	}

	if (event & SND_SOC_DAPM_POST_PMD) {
		/* Check for the DAC FLAG register to know if the SPL & SPR are
		 * powered down
		 */
		if (w->shift == 7)
			ret_wbits = aic3xxx_wait_bits(codec->control_data,
						AIC31XX_DAC_FLAG_1, reg_mask,
						0x0,AIC31XX_TIME_DELAY,
						AIC31XX_DELAY_COUNTER);
			if (!ret_wbits)
				dev_dbg(codec->dev, "SPL power timedout\n");
		if (w->shift == 6)
			ret_wbits = aic3xxx_wait_bits(codec->control_data,
						AIC31XX_DAC_FLAG_1, reg_mask,
						0x0,AIC31XX_TIME_DELAY,
						AIC31XX_DELAY_COUNTER);
			if (!ret_wbits)
				dev_dbg(codec->dev, "SPR power timedout\n");

	}
	if (event & SND_SOC_DAPM_PRE_PMD) {
		if (!(strcmp(w->name, "SPL Class - D"))) {
			lv = snd_soc_read(codec, AIC31XX_LEFT_ANALOG_SPL);
			lv &= 0x7f;
			rv = snd_soc_read(codec, AIC31XX_RIGHT_ANALOG_SPR);
			rv &= 0x7f;
			val = lv ^ ((lv ^ rv) & -(lv < rv));
			while (val < 127) {
				snd_soc_write(codec, AIC31XX_LEFT_ANALOG_SPL,
						(0x80 | (val & 0x7f)));
				snd_soc_write(codec, AIC31XX_RIGHT_ANALOG_SPR,
						(0x80 | (val & 0x7f)));
				val++;
			}
			/* The D7 bit is set to zero to mute the gain */
			snd_soc_write(codec, AIC31XX_LEFT_ANALOG_SPL,
						(val & 0x7f));
			snd_soc_write(codec, AIC31XX_RIGHT_ANALOG_SPR,
						(val & 0x7f));
		}

	}

	return 0;
}

/* The updated aic31xx_divs Array for the KCI board having 19.2 Mhz
 * Master Clock Input coming from the FSREF2_CLK pin of OMAP4
 */
static const struct aic31xx_rate_divs aic31xx_divs[] = {
	/*
	 * mclk, rate, p_val, pll_j, pll_d, dosr, ndac, mdac, aosr, nadc, madc,
	 * blck_N, codec_speficic_initializations
	 */
	/* 8k rate */
	{19200000, 8000, 1, 5, 1200, 768, 16, 1, 128, 48, 2, 24},
	{19200000, 8000, 1, 5, 1200, 256, 24, 2, 0, 24, 2, 8},
	/* 11.025k rate */
	{19200000, 11025, 1, 4, 4100, 256, 15, 2, 128, 30, 2, 8},
	{19200000, 11025, 1, 4, 4100, 256, 15, 2, 0, 15, 2, 8},
	/* 12K rate */
	{19200000, 12000, 1, 4, 8000, 256, 15, 2, 128, 30, 2, 8},
	{19200000, 12000, 1, 4, 8000, 256, 15, 2, 0, 15, 2, 8},
	/* 16k rate */
	{19200000, 16000, 1, 5, 1200, 256, 12, 2, 128, 24, 2, 8},
	{19200000, 16000, 1, 5, 1200, 256, 12, 2, 0, 12, 2, 8},
	/* 22.05k rate */
	{19200000, 22050, 1, 4, 7040, 256, 8, 2, 128, 16, 2, 8},
	{19200000, 22050, 1, 4, 7040, 256, 8, 2, 0, 8, 2, 8},
	/* 24k rate */
	{19200000, 24000, 1, 5, 1200, 256, 8, 2, 128, 16, 2, 8},
	{19200000, 24000, 1, 5, 1200, 256, 8, 2, 0, 8, 2, 8},
	/* 32k rate */
	{19200000, 32000, 1, 5, 1200, 256, 6, 2, 128, 12, 2, 8},
	{19200000, 32000, 1, 5, 1200, 256, 6, 2, 0, 6, 2, 8},
	/* 44.1k rate */
	{19200000, 44100, 1, 4, 7040, 128, 4, 4, 128, 8, 2, 4},
	{19200000, 44100, 1, 4, 7040, 256, 4, 2, 0, 4, 2, 8},
	/* 48k rate */
	{19200000, 48000, 1, 5, 1200, 128, 4, 4, 128, 8, 2, 4},
	{19200000, 48000, 1, 5, 1200, 128, 4, 4, 0, 4, 2, 8},
	/*96k rate */
	{19200000, 96000, 1, 5, 1200, 256, 2, 2, 128, 4, 2, 8},
	{19200000, 96000, 1, 5, 1200, 256, 2, 2, 0, 2, 2, 8},
	/*192k */
	{19200000, 192000, 1, 5, 1200, 256, 2, 1, 128, 4, 1, 16},
	{19200000, 192000, 1, 5, 1200, 256, 2, 1, 0, 2, 1, 16},
};


/*
 * aic31xx_get_divs
 *
 * This function is to get required divisor from the "aic31xx_divs" table.
 */
static inline int aic31xx_get_divs(int mclk, int rate)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(aic31xx_divs); i++) {
		if ((aic31xx_divs[i].rate == rate) &&
				(aic31xx_divs[i].mclk == mclk)) {
			return i;
		}
	}
	DBG("##Master clock and sample rate is not supported\n");
	return -EINVAL;
}

/*
 * This function is to set the hardware parameters for aic31xx.  The
 * functions set the sample rate and audio serial data word length.
 */
static int aic31xx_hw_params(struct snd_pcm_substream *substream,
				struct snd_pcm_hw_params *params,
				struct snd_soc_dai *tmp)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_codec *codec = rtd->codec;
	struct aic31xx_priv *aic31xx = snd_soc_codec_get_drvdata(codec);
	int i;
	u8 data;
	dev_dbg(codec->dev, "%s\n", __func__);

	/* Setting the playback status.
	 * Update the capture_stream Member of the Codec's Private structure
	 * to denote that we will be performing Audio capture from now on.
	 */
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		aic31xx->playback_status = 1;
		aic31xx->playback_stream = 1;
	} else if ((substream->stream != SNDRV_PCM_STREAM_PLAYBACK) && \
							(codec->active < 2))
		aic31xx->playback_stream = 0;
	if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
		aic31xx->playback_status = 1;
		aic31xx->capture_stream = 1;
	} else if ((substream->stream != SNDRV_PCM_STREAM_CAPTURE) &&
							(codec->active < 2))
		aic31xx->capture_stream = 0;

	dev_dbg(codec->dev,
		"%s: playback_stream= %d capture_stream = %d"
		"priv_playback_stream= %d priv_record_stream = %d\n" ,
		__func__, SNDRV_PCM_STREAM_PLAYBACK, SNDRV_PCM_STREAM_CAPTURE,
		aic31xx->playback_stream, aic31xx->capture_stream);


	i = aic31xx_get_divs(aic31xx->sysclk, params_rate(params));
	if (i < 0) {
		DBG("sampling rate not supported\n");
		DBG("%s: Exiting with error\n", __func__);
		return i;
	}

	/* We will fix R value to 1 and will make P & J=K.D as
	 * varialble  Setting P & R values
	 */
	if (codec->active < 2) {
		snd_soc_update_bits(codec, AIC31XX_CLK_R2, 0x7F, ((aic31xx_divs[i].p_val << 4) | 0x01));

		/* J value */
		snd_soc_update_bits(codec, AIC31XX_CLK_R3, 0x7F, aic31xx_divs[i].pll_j);

		/* MSB & LSB for D value */
		aic31xx_codec_write(codec, AIC31XX_CLK_R4, (aic31xx_divs[i].pll_d >> 8));
		aic31xx_codec_write(codec, AIC31XX_CLK_R5, (aic31xx_divs[i].pll_d & AIC31XX_8BITS_MASK));

		/* NDAC divider value */
		snd_soc_update_bits(codec, AIC31XX_NDAC_CLK_R6, 0x7F, aic31xx_divs[i].ndac);

		/* MDAC divider value */
		snd_soc_update_bits(codec, AIC31XX_MDAC_CLK_R7, 0x7F, aic31xx_divs[i].mdac);

		/* DOSR MSB & LSB values */
		aic31xx_codec_write(codec, AIC31XX_DAC_OSR_MSB, aic31xx_divs[i].dosr >> 8);
		aic31xx_codec_write(codec, AIC31XX_DAC_OSR_LSB, aic31xx_divs[i].dosr & AIC31XX_8BITS_MASK);

		/* NADC divider value */
		snd_soc_update_bits(codec, AIC31XX_NADC_CLK_R8, 0x7F, aic31xx_divs[i].nadc);

		/* MADC divider value */
		snd_soc_update_bits(codec, AIC31XX_MADC_CLK_R9, 0x7F, aic31xx_divs[i].madc);

		/* AOSR value */
		aic31xx_codec_write(codec, AIC31XX_ADC_OSR_REG, aic31xx_divs[i].aosr);

		/* BCLK N divider */
		snd_soc_update_bits(codec, AIC31XX_CLK_R11, 0x7F, aic31xx_divs[i].blck_N);

		DBG("## Writing NDAC %d MDAC %d NADC %d MADC %d DOSR %d AOSR %d\n", aic31xx_divs[i].ndac, aic31xx_divs[i].mdac,
			aic31xx_divs[i].nadc, aic31xx_divs[i].madc,
			aic31xx_divs[i].dosr, aic31xx_divs[i].aosr);
	}

	data = snd_soc_read(codec, AIC31XX_INTERFACE_SET_REG_1);
	data = data & ~(3 << 4);

	dev_dbg(codec->dev, "##- Data length: %d\n", params_format(params));

	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_S16_LE:
		break;
	case SNDRV_PCM_FORMAT_S20_3LE:
		data |= (AIC31XX_WORD_LEN_20BITS << DAC_OSR_MSB_SHIFT);
		break;
	case SNDRV_PCM_FORMAT_S24_LE:
		data |= (AIC31XX_WORD_LEN_24BITS << DAC_OSR_MSB_SHIFT);
		break;
	case SNDRV_PCM_FORMAT_S32_LE:
		data |= (AIC31XX_WORD_LEN_32BITS << DAC_OSR_MSB_SHIFT);
		break;
	}

	/* Write to Page 0 Reg 27 for the Codec Interface control 1
	 * Register */
	snd_soc_write(codec, AIC31XX_INTERFACE_SET_REG_1, data);

	return 0;
}

/*
 * aic31xx_dac_mute - mute or unmute the left and right DAC

 */
static int aic31xx_dac_mute(struct snd_soc_codec *codec, int mute)
{
	struct aic31xx_priv *aic31xx = snd_soc_codec_get_drvdata(codec);

	dev_dbg(codec->dev,
		"%s: mute = %d\t priv->mute = %d\t headset_detect = %d\n",
		__func__, mute,	aic31xx->mute, aic31xx->headset_connected);

	/* Also update the global Playback Status Flag. This is required for
	 * biquad update.
	*/
	if ((mute) && (aic31xx->mute != 1)) {
		aic31xx->playback_status = 0;

		if (!aic31xx->headset_connected) {
			/*Switch off the DRC*/
			snd_soc_update_bits(codec, AIC31XX_DRC_CTRL_REG_1, 0x60,
					(CLEAR & ~(BIT6 | BIT5)));
		}
		snd_soc_update_bits(codec, AIC31XX_DAC_MUTE_CTRL_REG, 0x0C,
					(CLEAR | MUTE_ON));
		aic31xx->mute = 1;
	} else if (!mute) {
		aic31xx->playback_status = 1;
		/* Check whether Playback or Record Session is about to Start */
		if (aic31xx->playback_stream) {
			if (!aic31xx->headset_connected) {
				/*DRC enable for speaker path*/
				snd_soc_update_bits(codec,
					AIC31XX_DRC_CTRL_REG_1, 0x60,
					(CLEAR | (BIT6 | BIT5)));

			} else {
				/*Switch off the DRC*/
				snd_soc_update_bits(codec,
					AIC31XX_DRC_CTRL_REG_1, 0x60,
					(CLEAR & ~(BIT6 | BIT5)));
			}
			snd_soc_update_bits(codec, AIC31XX_DAC_MUTE_CTRL_REG,
						0x0C, (CLEAR & ~MUTE_ON));
		}
		aic31xx->power_status = 1;
		aic31xx->mute = 0;
	}
#ifdef AIC31XX_DEBUG
	debug_print_registers(codec);
#endif
	dev_dbg(codec->dev, "##-aic31xx_mute_codec %d\n", mute);

	return 0;
}

/*
 * aic31xx_mute- mute or unmute the left and right DAC
 */
static int aic31xx_mute_codec(struct snd_soc_codec *codec, int mute)
{
	int result = 0;
	struct aic31xx_priv *aic31xx = snd_soc_codec_get_drvdata(codec);
	dev_dbg(codec->dev, "%s: mute = %d\t priv_mute = %d\n",
		__func__, mute, aic31xx->mute);

	dev_dbg(codec->dev, "%s:lock  mute = %d\t priv_mute = %d\n",
		__func__, mute, aic31xx->mute);

	/* Check for playback and record status and accordingly
	 * mute or unmute the ADC or the DAC
	 */
	if ((mute == 1) && (codec->active != 0)) {
		if ((aic31xx->playback_stream == 1) &&
					(aic31xx->capture_stream == 1)) {
			dev_warn(codec->dev, "Session still active\n");
		return 0;
		}
	}
	if (aic31xx->playback_stream)
		result = aic31xx_dac_mute(codec, mute);

	dev_dbg(codec->dev, "%s: mute = %d\t priv_mute = %d\n",
		__func__, mute, aic31xx->mute);
	return result;
}

/*
 * aic31xx_mute- mute or unmute the left and right DAC
 */
static int aic31xx_mute(struct snd_soc_dai *dai, int mute)
{
	return aic31xx_mute_codec(dai->codec, mute);
}

/*
 * aic31xx_set_dai_sysclk
 *
 * This function is to the DAI system clock
 */
static int aic31xx_set_dai_sysclk(struct snd_soc_dai *codec_dai,
		int clk_id, unsigned int freq, int dir)
{
	struct snd_soc_codec *codec = codec_dai->codec;
	struct aic31xx_priv *aic31xx = snd_soc_codec_get_drvdata(codec);

	DBG("%s: Entered\n", __func__);
	DBG("###aic31xx_set_dai_sysclk SysClk %x\n", freq);
	switch (freq) {
		case AIC31XX_FREQ_12000000:
		case AIC31XX_FREQ_24000000:
		case AIC31XX_FREQ_19200000:
			aic31xx->sysclk = freq;
			DBG("%s: Exiting\n", __func__);
			return 0;
	}
	DBG("Invalid frequency to set DAI system clock\n");
	DBG("%s: Exiting with error\n", __func__);
	return -EINVAL;
}

/*
 * aic31xx_set_dai_fmt
 *
 * This function is to set the DAI format
 */
static int aic31xx_set_dai_fmt(struct snd_soc_dai *codec_dai,
		unsigned int fmt)
{
	struct snd_soc_codec *codec = codec_dai->codec;
	struct aic31xx_priv *aic31xx = snd_soc_codec_get_drvdata(codec);
	u8 iface_reg1 = 0;
	u8 iface_reg3 = 0;
	u8 dsp_a_val = 0;

	dev_dbg(codec->dev, "%s: Entered\n", __func__);
	dev_dbg(codec->dev, "###aic31xx_set_dai_fmt %x\n", fmt);


	dev_dbg(codec->dev, "##+ aic31xx_set_dai_fmt (%x)\n", fmt);

	/* set master/slave audio interface */
	switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
	case SND_SOC_DAIFMT_CBM_CFM:
		aic31xx->master = 1;
		iface_reg1 |= BIT_CLK_MASTER | WORD_CLK_MASTER;
		break;
	case SND_SOC_DAIFMT_CBS_CFS:
		aic31xx->master = 0;
		iface_reg1 &= ~(BIT_CLK_MASTER | WORD_CLK_MASTER);
		break;
	case SND_SOC_DAIFMT_CBS_CFM:
		aic31xx->master = 0;
		iface_reg1 |= BIT_CLK_MASTER;
		iface_reg1 &= ~(WORD_CLK_MASTER);
		break;
	default:
		dev_alert(codec->dev, "Invalid DAI master/slave interface\n");
		return -EINVAL;
	}
	/* interface format */
	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAIFMT_I2S:
		break;
	case SND_SOC_DAIFMT_DSP_A:
			dsp_a_val = 0x1;
	case SND_SOC_DAIFMT_DSP_B:
		switch (fmt & SND_SOC_DAIFMT_INV_MASK) {
			case SND_SOC_DAIFMT_NB_NF:
				break;
			case SND_SOC_DAIFMT_IB_NF:
				iface_reg3 |=BCLK_INV_MASK;
				break;
			default:
				return -EINVAL;
		}
		iface_reg1 |= (AIC31XX_DSP_MODE << AUDIO_MODE_SHIFT);
		break;
	case SND_SOC_DAIFMT_RIGHT_J:
		iface_reg1 |= (AIC31XX_RIGHT_JUSTIFIED_MODE << AUDIO_MODE_SHIFT);
		break;
	case SND_SOC_DAIFMT_LEFT_J:
		iface_reg1 |= (AIC31XX_LEFT_JUSTIFIED_MODE << AUDIO_MODE_SHIFT);
		break;
	default:
		dev_alert(codec->dev, "Invalid DAI interface format\n");
		return -EINVAL;
	}

	snd_soc_update_bits(codec, AIC31XX_INTERFACE_SET_REG_1,
			INTERFACE_REG1_DATA_TYPE_MASK |
			INTERFACE_REG1_MASTER_MASK,
			iface_reg1);
	snd_soc_update_bits(codec, AIC31XX_DATA_SLOT_OFFSET,
			INTERFACE_REG2_MASK,
			dsp_a_val);
	snd_soc_update_bits(codec, AIC31XX_INTERFACE_SET_REG_2,
			INTERFACE_REG3_MASK,
			iface_reg3);

	dev_dbg(codec->dev, "##-aic31xx_set_dai_fmt Master %d\n",
		aic31xx->master);
	dev_dbg(codec->dev, "%s: Exiting\n", __func__);

	return 0;
}


/*
 * aic31xx_set_bias_level - This function is to get triggered when dapm
 * events occurs.
 */
static int aic31xx_set_bias_level(struct snd_soc_codec *codec,
				enum snd_soc_bias_level level)
{


	dev_dbg(codec->dev, "## aic31xx_set_bias_level %d\n", level);

	if (level == codec->dapm.bias_level) {
		dev_dbg(codec->dev, "##set_bias_level: level returning...\r\n");
		return 0;
	}

	switch (level) {
	/* full On */
	case SND_SOC_BIAS_ON:
		dev_dbg(codec->dev, "###aic31xx_set_bias_level BIAS_ON\n");
		break;

	/* partial On */
	case SND_SOC_BIAS_PREPARE:
		dev_dbg(codec->dev, "###aic31xx_set_bias_level BIAS_PREPARE\n");
		break;

	/* Off, with power */
	case SND_SOC_BIAS_STANDBY:
		dev_dbg(codec->dev, "###aic31xx_set_bias_level STANDBY\n");
		break;

	/* Off, without power */
	case SND_SOC_BIAS_OFF:
		dev_dbg(codec->dev, "###aic31xx_set_bias_level OFF\n");
		break;

	}
	codec->dapm.bias_level = level;
	dev_dbg(codec->dev, "## aic31xx_set_bias_level %d\n", level);

	return 0;
}


/*
 *----------------------------------------------------------------------------
 * Function : aic31xx_mic_check
 * Purpose  : This function checks for the status of the Page0 Register 67
 *            [Headset Detect] Register and checks if Bit6 is set. This denotes
 *            that a Jack with Microphone is plugged in or not.
 *
 * Returns  : 1 is the Bit 6 of Pg 0 Reg 67 is set
 *            0 is the Bit 6 of Pg 0 Reg 67 is not set.
 *----------------------------------------------------------------------------
 */
int aic31xx_mic_check(struct snd_soc_codec *codec)
{
//	struct aic31xx_priv *aic31xx = snd_soc_codec_get_drvdata(codec);
	int status, value, state = 0, switch_state = 0;
	status = snd_soc_update_bits(codec, AIC31XX_HS_DETECT_REG, 0x80, 0);
	mdelay(10);

	value = snd_soc_read(codec, AIC31XX_MICBIAS_CTRL_REG);
	snd_soc_update_bits(codec, AIC31XX_MICBIAS_CTRL_REG,
				value, (BIT1|BIT0));
	mdelay(10);
	snd_soc_update_bits(codec, AIC31XX_HS_DETECT_REG, 0x80, 0x80);
	mdelay(HP_DEBOUNCE_TIME_IN_MS);
	status = snd_soc_read(codec, AIC31XX_HS_DETECT_REG);

	switch (status & AIC31XX_HS_MASK) {
	case  AIC31XX_HS_MASK:
		state |= SND_JACK_HEADSET;
		break;
	case AIC31XX_HP_MASK:
		state |= SND_JACK_HEADPHONE;
		break;
	default:
		break;
	}

	if ((state & SND_JACK_HEADSET) == SND_JACK_HEADSET)
		switch_state |= (1<<0);
	else if (state & SND_JACK_HEADPHONE)
		switch_state |= (1<<1);
	dev_dbg(codec->dev, "Headset status =%x, state = %x, switch_state = %x\n",
		status, state, switch_state);

	return switch_state;
}


static struct aic3xxx_irq_data aic3xxx_irqs[] = {
	{
	.mask = AIC31XX_HEADSET_IN_MASK,
	.status = AIC31XX_HEADSET_PLUG_UNPLUG_INT,
	 },
	{
	.mask = AIC31XX_BUTTON_PRESS_MASK,
	.status = AIC31XX_BUTTON_PRESS_INT,
	 },
	{
	.mask = AIC31XX_DAC_DRC_THRES_MASK,
	.status = AIC31XX_LEFT_DRC_THRES_INT | AIC31XX_RIGHT_DRC_THRES_INT,
	 },
	{
	.mask = AIC31XX_AGC_NOISE_MASK,
	.status = AIC31XX_AGC_NOISE_INT,
	 },
	{
	.mask = AIC31XX_OVER_CURRENT_MASK,
	.status = AIC31XX_LEFT_OUTPUT_DRIVER_OVERCURRENT_INT | AIC31XX_RIGHT_OUTPUT_DRIVER_OVERCURRENT_INT,
	},
	{
	.mask = AIC31XX_ENGINE_GEN__MASK,
	.status = AIC31XX_LEFT_DAC_OVERFLOW_INT | AIC31XX_RIGHT_DAC_OVERFLOW_INT |
		AIC31XX_MINIDSP_D_BARREL_SHIFT_OVERFLOW_INT |
		AIC31XX_ADC_OVERFLOW_INT |
		AIC31XX_MINIDSP_A_BARREL_SHIFT_OVERFLOW_INT,
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
		aic3xxx_reg_write(aic3xxx, AIC31XX_INT1_CTRL_REG, aic3xxx->irq_masks_cur);
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
	aic3xxx_bulk_read(aic3xxx, AIC31XX_INTR_DAC_FLAG_1, 4, status);
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
	aic3xxx_reg_write(aic3xxx, AIC31XX_INT1_CTRL_REG, 0x00);
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
		set_irq_flags(cur_irq, IRQF_VALID);
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

void aic3xxx_irq_exit(struct aic3xxx *aic3xxx)
{
	if (aic3xxx->irq)
		free_irq(aic3xxx->irq, aic3xxx);
}

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

	if (aic3xxx->pdata.regulator_name) {
		audio_regulator = regulator_get(NULL, aic3xxx->pdata.regulator_name);
		if (IS_ERR(audio_regulator)) {
			dev_err(aic3xxx->dev, "regulator_get error: %s\n", aic3xxx->pdata.regulator_name);
			return -1;
		}

		if (aic3xxx->pdata.regulator_min_uV > 0 && aic3xxx->pdata.regulator_max_uV > 0) {
			ret = regulator_set_voltage(audio_regulator,
					aic3xxx->pdata.regulator_min_uV,
					aic3xxx->pdata.regulator_max_uV);
			if (ret) {
				dev_err(aic3xxx->dev, "regulator_set 3V error: %s min=%d, max=%d\n",
					aic3xxx->pdata.regulator_name,
					aic3xxx->pdata.regulator_min_uV,
					aic3xxx->pdata.regulator_max_uV);
				return -1;
			}
			regulator_enable(audio_regulator);
		}
	}
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
	ret = aic3xxx_reg_write(aic3xxx, AIC31XX_RESET_REG, resetVal);
	if (ret < 0) {
		dev_err(aic3xxx->dev, "Could not write to AIC31xx register\n");
		goto err_return;
	}

	mdelay(10);
	ret = aic3xxx_reg_read(aic3xxx, AIC31XX_REV_PG_ID);
	if (ret < 0) {
		dev_err(aic3xxx->dev, "Failed to read ID register\n");
		goto err_return;
	}

	dev_info(aic3xxx->dev, "aic3xxx revision %d\n", ret);

	for (i = 0; i < aic3xxx->pdata.num_gpios; i++) {
		aic3xxx_reg_write(aic3xxx, aic3xxx->pdata.gpio_defaults[i].reg,
			aic3xxx->pdata.gpio_defaults[i].value);
	}

	aic3xxx->irq_base = aic3xxx->pdata.irq_base;

	/* codec interrupt */
	if (aic3xxx->irq) {
		ret = aic3xxx_irq_init(aic3xxx);
		if (ret < 0)
			goto err_return;
	}

	return 0;

err_return:
	if (aic3xxx->pdata.gpio_reset)
		gpio_free(aic3xxx->pdata.gpio_reset);

	return ret;
}

void aic3xxx_device_exit(struct aic3xxx *aic3xxx)
{

	aic3xxx_irq_exit(aic3xxx);

	if (aic3xxx->pdata.gpio_reset)
		gpio_free(aic3xxx->pdata.gpio_reset);

	if (audio_regulator) {
		regulator_disable(audio_regulator);
		regulator_put(audio_regulator);
	}
}

static int pll_power_on_event(struct snd_soc_dapm_widget *w, \
		struct snd_kcontrol *kcontrol, int event)
{
//	struct snd_soc_codec *codec = w->codec;

	if (event == (SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD))
		mdelay(10);
	return 0;
}

/* Left Output Mixer */
static const struct snd_kcontrol_new
left_output_mixer_controls[] = {
	SOC_DAPM_SINGLE("From DAC_L", AIC31XX_DAC_MIXER_ROUTING, 6, 1, 0),
	SOC_DAPM_SINGLE("From MIC1LP", AIC31XX_DAC_MIXER_ROUTING, 5, 1, 0),
	SOC_DAPM_SINGLE("From MIC1RP", AIC31XX_DAC_MIXER_ROUTING, 4, 1, 0),
};

/* Right Output Mixer - Valid only for AIC31xx, 3110, 3100 */
static const struct
snd_kcontrol_new right_output_mixer_controls[] = {
	SOC_DAPM_SINGLE("From DAC_R", AIC31XX_DAC_MIXER_ROUTING, 2, 1, 0),
	SOC_DAPM_SINGLE("From MIC1RP", AIC31XX_DAC_MIXER_ROUTING, 1, 1, 0),
};

static const struct
snd_kcontrol_new pos_mic_pga_controls[] = {
	SOC_DAPM_SINGLE("MIC1LP_PGA_CNTL", AIC31XX_MICPGA_PIN_CFG, 6, 0x3, 0),
	SOC_DAPM_SINGLE("MIC1RP_PGA_CNTL", AIC31XX_MICPGA_PIN_CFG, 4, 0x3, 0),
	SOC_DAPM_SINGLE("MIC1LM_PGA_CNTL", AIC31XX_MICPGA_PIN_CFG, 2, 0x3, 0),
};

static const struct
snd_kcontrol_new neg_mic_pga_controls[] = {
	SOC_DAPM_SINGLE("CM_PGA_CNTL", AIC31XX_MICPGA_MIN_CFG, 6, 0x3, 0),
	SOC_DAPM_SINGLE("MIC1LM_PGA_CNTL", AIC31XX_MICPGA_MIN_CFG, 4, 0x3, 0),
};


static const struct snd_soc_dapm_widget aic31xx_dapm_widgets[] = {
	/* DACs */
	SND_SOC_DAPM_DAC_E("Left DAC", "Left Playback", AIC31XX_DAC_CHN_REG, 7, 0,
		aic31xx_dac_power_up_event, SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_DAC_E("Right DAC", "Right Playback", AIC31XX_DAC_CHN_REG, 6, 0,
		aic31xx_dac_power_up_event, SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),

	/* Output Mixers */
	SND_SOC_DAPM_MIXER("Left Output Mixer", SND_SOC_NOPM, 0, 0,
		left_output_mixer_controls, ARRAY_SIZE(left_output_mixer_controls)),
	SND_SOC_DAPM_MIXER("Right Output Mixer", SND_SOC_NOPM, 0, 0,
		right_output_mixer_controls, ARRAY_SIZE(right_output_mixer_controls)),

	/* Output drivers */
	SND_SOC_DAPM_PGA_E("HPL Driver", AIC31XX_HPL_DRIVER_REG, 2, 0,
		NULL, 0, aic31xx_hp_power_up_event, SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_PGA_E("HPR Driver", AIC31XX_HPR_DRIVER_REG, 2, 0,
		NULL, 0, aic31xx_hp_power_up_event, SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),


#ifndef AIC3100_CODEC_SUPPORT
	/* For AIC31XX and AIC3110 as it is stereo both left and right channel
	 * class-D can be powered up/down
	 */
	SND_SOC_DAPM_PGA_E("SPL Class - D", AIC31XX_CLASS_D_SPK, 7, 0,
		NULL, 0, aic31xx_sp_event, SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_PGA_E("SPR Class - D", AIC31XX_CLASS_D_SPK, 6, 0,
		NULL, 0, aic31xx_sp_event, SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),
#endif

#ifdef AIC3100_CODEC_SUPPORT
	/* For AIC3100 as is mono only left
	 * channel class-D can be powered up/down
	 */
	SND_SOC_DAPM_PGA_E("SPL Class - D", AIC31XX_CLASS_D_SPK, 7, 0,
		NULL, 0, aic31xx_sp_event, SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),

#endif

	/* ADC */
	SND_SOC_DAPM_ADC_E("ADC", "Capture", AIC31XX_ADC_CHN_REG, 7, 0,
		aic31xx_adc_power_up_event, SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),

	/*Input Selection to MIC_PGA*/
	SND_SOC_DAPM_MIXER("P_Input_Mixer", SND_SOC_NOPM, 0, 0,
		pos_mic_pga_controls, ARRAY_SIZE(pos_mic_pga_controls)),
	SND_SOC_DAPM_MIXER("M_Input_Mixer", SND_SOC_NOPM, 0, 0,
		neg_mic_pga_controls, ARRAY_SIZE(neg_mic_pga_controls)),

	/*Enabling & Disabling MIC Gain Ctl */
	SND_SOC_DAPM_PGA("MIC_GAIN_CTL", AIC31XX_MICPGA_VOL_CTRL, 7, 1, NULL, 0),

	SND_SOC_DAPM_SUPPLY("PLLCLK", AIC31XX_CLK_R2, 7, 0, pll_power_on_event,  SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_SUPPLY("CODEC_CLK_IN", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY("NDAC_DIV", AIC31XX_NDAC_CLK_R6, 7, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY("MDAC_DIV", AIC31XX_MDAC_CLK_R7, 7, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY("NADC_DIV", AIC31XX_NADC_CLK_R8, 7, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY("MADC_DIV", AIC31XX_MADC_CLK_R9, 7, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY("BCLK_N_DIV", AIC31XX_CLK_R11, 7, 0, NULL, 0),

	/* Outputs */
	SND_SOC_DAPM_OUTPUT("HPL"),
	SND_SOC_DAPM_OUTPUT("HPR"),
	SND_SOC_DAPM_OUTPUT("SPL"),

#ifndef AIC3100_CODEC_SUPPORT
	SND_SOC_DAPM_OUTPUT("SPR"),
#endif

	/* Inputs */
	SND_SOC_DAPM_INPUT("MIC1LP"),
	SND_SOC_DAPM_INPUT("MIC1RP"),
	SND_SOC_DAPM_INPUT("MIC1LM"),
	SND_SOC_DAPM_INPUT("INTMIC"),
};


static const struct snd_soc_dapm_route aic31xx_audio_map[] = {

	{"CODEC_CLK_IN", NULL, "PLLCLK"},
	{"NDAC_DIV", NULL, "CODEC_CLK_IN"},
	{"NADC_DIV", NULL, "CODEC_CLK_IN"},
	{"MDAC_DIV", NULL, "NDAC_DIV"},
	{"MADC_DIV", NULL, "NADC_DIV"},
	{"BCLK_N_DIV", NULL, "MADC_DIV"},
	{"BCLK_N_DIV", NULL, "MDAC_DIV"},

	/* Clocks for ADC */
	{"ADC", NULL, "MADC_DIV"},
	{"ADC", NULL, "BCLK_N_DIV"},

	/* Mic input */
	{"P_Input_Mixer", "MIC1LP_PGA_CNTL", "MIC1LP"},
	{"P_Input_Mixer", "MIC1RP_PGA_CNTL", "MIC1RP"},
	{"P_Input_Mixer", "MIC1LM_PGA_CNTL", "MIC1LM"},

	{"M_Input_Mixer", "CM_PGA_CNTL", "MIC1LM"},
	{"M_Input_Mixer", "MIC1LM_PGA_CNTL", "MIC1LM"},

	{"MIC_GAIN_CTL", NULL, "P_Input_Mixer"},
	{"MIC_GAIN_CTL", NULL, "M_Input_Mixer"},

	{"ADC", NULL, "MIC_GAIN_CTL"},
	{"ADC", NULL, "INTMIC"},

	/* Clocks for DAC */
	{"Left DAC", NULL, "MDAC_DIV" },
	{"Right DAC", NULL, "MDAC_DIV"},
	{"Left DAC", NULL, "BCLK_N_DIV" },
	{"Right DAC", NULL, "BCLK_N_DIV"},

	/* Left Output */
	{"Left Output Mixer", "From DAC_L", "Left DAC"},
	{"Left Output Mixer", "From MIC1LP", "MIC1LP"},
	{"Left Output Mixer", "From MIC1RP", "MIC1RP"},

	/* Right Output */
	{"Right Output Mixer", "From DAC_R", "Right DAC"},
	{"Right Output Mixer", "From MIC1RP", "MIC1RP"},

	/* HPL path */
	{"HPL Driver", NULL, "Left Output Mixer"},
	{"HPL", NULL, "HPL Driver"},

	/* HPR path */
	{"HPR Driver", NULL, "Right Output Mixer"},
	{"HPR", NULL, "HPR Driver"},

	/* SPK L path */
	{"SPL Class - D", NULL, "Left Output Mixer"},
	{"SPL", NULL, "SPL Class - D"},

#ifndef AIC3100_CODEC_SUPPORT
	/* SPK R path */
	{"SPR Class - D", NULL, "Right Output Mixer"},
	{"SPR", NULL, "SPR Class - D"},
#endif
};

static const char *const dac_mute[] = {"Unmute", "Mute"};
static const char *const adc_mute[] = {"Unmute", "Mute"};
static const char *const hpl_pwr[] = {"Off", "On"};
static const char *const hpr_pwr[] = {"Off", "On"};
static const char *const ldac_pwr[] = {"Off", "On"};
static const char *const rdac_pwr[] = {"Off", "On"};

static const char *const dacvolume_extra[] = {"L & R Ind Vol", "LVol = RVol",
					"RVol = LVol"};
static const char *const dacvolume_control[] = {"control register", "pin"};
static const char *const dacsoftstep_control[] = {"1 step / sample",
					"1 step / 2 sample", "disabled"};

static const char *const beep_generator[] = {"Disabled", "Enabled"};

static const char *const micbias_voltage[] = {"off", "2 V", "2.5 V", "AVDD"};
static const char *const dacleftip_control[] = {"off", "left data",
					"right data", "(left + right) / 2"};
static const char *const dacrightip_control[] = { "off", "right data",
					"left data", "(left+right)/2" };

static const char *const dacvoltage_control[] = {"1.35 V", "5 V ", "1.65 V",
					"1.8 V"};
static const char *const headset_detection[] = {"Disabled", "Enabled"};
static const char *const drc_enable[] = {"Disabled", "Enabled"};
static const char *const mic1lp_enable[] = {"off", "10 k", "20 k", "40 k"};
static const char *const mic1rp_enable[] = {"off", "10 k", "20 k", "40 k"};
static const char *const mic1lm_enable[] = {"off", "10 k", "20 k", "40 k"};
static const char *const cm_enable[] = {"off", "10 k", "20 k", "40 k"};
static const char *const mic_enable[] = {"Gain controlled by D0 - D6",
					"0 db Gain"};
static const char *const mic1_enable[] = {"floating",
					"connected to CM internally"};

/* Creates an array of the Single Ended Widgets */
static const struct soc_enum aic31xx_enum[] = {
	SOC_ENUM_SINGLE(AIC31XX_DAC_MUTE_CTRL_REG, 3, 2, dac_mute),
	SOC_ENUM_SINGLE(AIC31XX_DAC_MUTE_CTRL_REG, 2, 2, dac_mute),
	SOC_ENUM_SINGLE(AIC31XX_DAC_MUTE_CTRL_REG, 0, 3, dacvolume_extra),
	SOC_ENUM_SINGLE(AIC31XX_VOL_MICDECT_ADC, 7, 2, dacvolume_control),
	SOC_ENUM_SINGLE(AIC31XX_DAC_CHN_REG, 0, 3, dacsoftstep_control),
	SOC_ENUM_SINGLE(AIC31XX_BEEP_GEN_L, 7, 2, beep_generator),
	SOC_ENUM_SINGLE(AIC31XX_MICBIAS_CTRL_REG, 0, 4, micbias_voltage),
	SOC_ENUM_SINGLE(AIC31XX_DAC_CHN_REG, 4, 4, dacleftip_control),
	SOC_ENUM_SINGLE(AIC31XX_DAC_CHN_REG, 2, 4, dacrightip_control),
	SOC_ENUM_SINGLE(AIC31XX_HPHONE_DRIVERS, 3, 4, dacvoltage_control),
	SOC_ENUM_SINGLE(AIC31XX_HS_DETECT_REG, 7, 2, headset_detection),
	SOC_ENUM_DOUBLE(AIC31XX_DRC_CTRL_REG_1, 6, 5, 2, drc_enable),
	SOC_ENUM_SINGLE(AIC31XX_MICPGA_PIN_CFG, 6, 4, mic1lp_enable),
	SOC_ENUM_SINGLE(AIC31XX_MICPGA_PIN_CFG, 4, 4, mic1rp_enable),
	SOC_ENUM_SINGLE(AIC31XX_MICPGA_PIN_CFG, 2, 4, mic1lm_enable),
	SOC_ENUM_SINGLE(AIC31XX_MICPGA_VOL_CTRL, 7, 2, mic_enable),
	SOC_ENUM_SINGLE(AIC31XX_MICPGA_MIN_CFG, 6, 4, cm_enable),
	SOC_ENUM_SINGLE(AIC31XX_MICPGA_MIN_CFG, 4, 4, mic1lm_enable),
	SOC_ENUM_SINGLE(AIC31XX_MICPGA_CM_REG, 7, 2, mic1_enable),
	SOC_ENUM_SINGLE(AIC31XX_MICPGA_CM_REG, 6, 2, mic1_enable),
	SOC_ENUM_SINGLE(AIC31XX_MICPGA_CM_REG, 5, 2, mic1_enable),
	SOC_ENUM_SINGLE(AIC31XX_ADC_VOL_FGC, 7, 2, adc_mute),
	SOC_ENUM_SINGLE(AIC31XX_HPHONE_DRIVERS, 7, 2, hpl_pwr),
	SOC_ENUM_SINGLE(AIC31XX_HPHONE_DRIVERS, 6, 2, hpr_pwr),
	SOC_ENUM_SINGLE(AIC31XX_DAC_CHN_REG, 7, 2, ldac_pwr),
	SOC_ENUM_SINGLE(AIC31XX_DAC_CHN_REG, 6, 2, rdac_pwr),
};

static const DECLARE_TLV_DB_SCALE(dac_vol_tlv, -6350, 50, 0);
static const DECLARE_TLV_DB_SCALE(adc_fgain_tlv, 00, 10, 0);
static const DECLARE_TLV_DB_SCALE(adc_cgain_tlv, -2000, 50, 0);
static const DECLARE_TLV_DB_SCALE(mic_pga_tlv, 0, 50, 0);
static const DECLARE_TLV_DB_SCALE(hp_drv_tlv, 0, 100, 0);
static const DECLARE_TLV_DB_SCALE(class_D_drv_tlv, 600, 600, 0);
static const DECLARE_TLV_DB_SCALE(hp_vol_tlv, -7830, 60, 0);
static const DECLARE_TLV_DB_SCALE(sp_vol_tlv, -7830, 60, 0);
/*
 * controls that need to be exported to the user space
 */
static const struct snd_kcontrol_new aic31xx_snd_controls[] = {
	/* DAC Volume Control */
	 SOC_DOUBLE_R_SX_TLV("DAC Playback Volume", AIC31XX_LDAC_VOL_REG,
		AIC31XX_RDAC_VOL_REG, 8, 0xffffff81, 0x30, dac_vol_tlv),
	/* DAC mute control */
	SOC_ENUM("Left DAC Mute", aic31xx_enum[LMUTE_ENUM]),
	SOC_ENUM("Right DAC Mute", aic31xx_enum[RMUTE_ENUM]),
	/* DAC volume Extra control */
	SOC_ENUM("DAC volume Extra control", aic31xx_enum[DACEXTRA_ENUM]),
	/* DAC volume Control register/pin control */
	SOC_ENUM("DAC volume Control register/pin",
		aic31xx_enum[DACCONTROL_ENUM]),
	/* DAC Volume soft stepping control */
	SOC_ENUM("DAC Volume soft stepping", aic31xx_enum[SOFTSTEP_ENUM]),
	/* HP driver mute control */
	SOC_DOUBLE_R("HP driver mute", AIC31XX_HPL_DRIVER_REG,
		AIC31XX_HPR_DRIVER_REG, 2, 2, 0),

#ifndef AIC3100_CODEC_SUPPORT
	/* SP driver mute control */
	SOC_DOUBLE_R("SP driver mute", AIC31XX_SPL_DRIVER_REG,
		AIC31XX_SPR_DRIVER_REG, 2, 2, 0),
#else
	SOC_SINGLE("SP driver mute", AIC31XX_SPL_DRIVER_REG,
		2, 2, 0);
#endif
	/* ADC FINE GAIN */
	SOC_SINGLE_TLV("ADC FINE GAIN", AIC31XX_ADC_VOL_FGC, 4, 4, 1,
		adc_fgain_tlv),
	/* ADC COARSE GAIN */
	SOC_DOUBLE_S8_TLV("ADC COARSE GAIN", AIC31XX_ADC_VOL_CGC,
		0xffffff68, 0x28, adc_cgain_tlv),
	/* ADC MIC PGA GAIN */
	SOC_SINGLE_TLV("ADC MIC_PGA GAIN", AIC31XX_MICPGA_VOL_CTRL, 0,
		119, 0, mic_pga_tlv),

	/* HP driver Volume Control */
	SOC_DOUBLE_R_TLV("HP driver Volume", AIC31XX_HPL_DRIVER_REG,
		AIC31XX_HPR_DRIVER_REG, 3, 0x09, 0, hp_drv_tlv),
	/* Left DAC input selection control */
	SOC_ENUM("Left DAC input selection", aic31xx_enum[DACLEFTIP_ENUM]),
	/* Right DAC input selection control */
	SOC_ENUM("Right DAC input selection", aic31xx_enum[DACRIGHTIP_ENUM]),

	/* Beep generator Enable/Disable control */
	SOC_ENUM("Beep generator Enable / Disable", aic31xx_enum[BEEP_ENUM]),
	/* Beep generator Volume Control */
	SOC_DOUBLE_R("Beep Volume Control(0 = -61 db, 63 = 2 dB)",
		AIC31XX_BEEP_GEN_L, AIC31XX_BEEP_GEN_R, 0, 0x3F, 1),
	/* Beep Length MSB control */
	SOC_SINGLE("Beep Length MSB", AIC31XX_BEEP_LEN_MSB_REG, 0, 255, 0),
	/* Beep Length MID control */
	SOC_SINGLE("Beep Length MID", AIC31XX_BEEP_LEN_MID_REG, 0, 255, 0),
	/* Beep Length LSB control */
	SOC_SINGLE("Beep Length LSB", AIC31XX_BEEP_LEN_LSB_REG, 0, 255, 0),
	/* Beep Sin(x) MSB control */
	SOC_SINGLE("Beep Sin(x) MSB", AIC31XX_BEEP_SINX_MSB_REG, 0, 255, 0),
	/* Beep Sin(x) LSB control */
	SOC_SINGLE("Beep Sin(x) LSB", AIC31XX_BEEP_SINX_LSB_REG, 0, 255, 0),
	/* Beep Cos(x) MSB control */
	SOC_SINGLE("Beep Cos(x) MSB", AIC31XX_BEEP_COSX_MSB_REG, 0, 255, 0),
	/* Beep Cos(x) LSB control */
	SOC_SINGLE("Beep Cos(x) LSB", AIC31XX_BEEP_COSX_LSB_REG, 0, 255, 0),

	/* Mic Bias voltage */
	SOC_ENUM("Mic Bias Voltage", aic31xx_enum[MICBIAS_ENUM]),

	/* DAC Processing Block Selection */
	SOC_SINGLE("DAC Processing Block Selection(0 <->25)",
		AIC31XX_DAC_PRB, 0, 0x19, 0),
	/* ADC Processing Block Selection */
	SOC_SINGLE("ADC Processing Block Selection(0 <->25)",
		AIC31XX_ADC_PRB, 0, 0x12, 0),

	/* Throughput of 7-bit vol ADC for pin control */
	SOC_SINGLE("Throughput of 7 - bit vol ADC for pin",
		AIC31XX_VOL_MICDECT_ADC, 0, 0x07, 0),

	/* Audio gain control (AGC) */
	SOC_SINGLE("Audio Gain Control(AGC)", AIC31XX_CHN_AGC_R1, 7, 0x01, 0),
	/* AGC Target level control */
	SOC_SINGLE("AGC Target Level Control", AIC31XX_CHN_AGC_R1, 4, 0x07, 1),
	/* AGC Maximum PGA applicable */
	SOC_SINGLE("AGC Maximum PGA Control", AIC31XX_CHN_AGC_R3, 0, 0x77, 0),
	/* AGC Attack Time control */
	SOC_SINGLE("AGC Attack Time control", AIC31XX_CHN_AGC_R4, 3, 0x1F, 0),
	/* AGC Attac Time Multiply factor */
	SOC_SINGLE("AGC_ATC_TIME_MULTIPLIER", AIC31XX_CHN_AGC_R4, 0, 8, 0),
	/* AGC Decay Time control */
	SOC_SINGLE("AGC Decay Time control", AIC31XX_CHN_AGC_R5, 3, 0x1F, 0),
	/* AGC Decay Time Multiplier */
	SOC_SINGLE("AGC_DECAY_TIME_MULTIPLIER", AIC31XX_CHN_AGC_R5, 0, 8, 0),
	/* AGC HYSTERISIS */
	SOC_SINGLE("AGC_HYSTERISIS", AIC31XX_CHN_AGC_R2, 6, 3, 0),
	/* AGC Noise Threshold */
	SOC_SINGLE("AGC_NOISE_THRESHOLD", AIC31XX_CHN_AGC_R2, 1, 32, 1),
	/* AGC Noise Bounce control */
	SOC_SINGLE("AGC Noice bounce control", AIC31XX_CHN_AGC_R6, 0, 0x1F, 0),
	/* AGC Signal Bounce control */
	SOC_SINGLE("AGC Signal bounce control", AIC31XX_CHN_AGC_R7, 0, 0x0F, 0),

	/* HP Output common-mode voltage control */
	SOC_ENUM("HP Output common - mode voltage control", aic31xx_enum[VOLTAGE_ENUM]),

	/* Headset detection Enable/Disable control */
	SOC_ENUM("Headset detection Enable / Disable", aic31xx_enum[HSET_ENUM]),

	/* DRC Enable/Disable control */
	SOC_ENUM("DRC Enable / Disable", aic31xx_enum[DRC_ENUM]),
	/* DRC Threshold value control */
	SOC_SINGLE("DRC Threshold value(0 = -3 db, 7 = -24 db)", AIC31XX_DRC_CTRL_REG_1, 2, 0x07, 0),
	/* DRC Hysteresis value control */
	SOC_SINGLE("DRC Hysteresis value(0 = 0 db, 3 = 3 db)", AIC31XX_DRC_CTRL_REG_1, 0, 0x03, 0),
	/* DRC Hold time control */
	SOC_SINGLE("DRC hold time", AIC31XX_DRC_CTRL_REG_2, 3, 0x0F, 0),
	/* DRC attack rate control */
	SOC_SINGLE("DRC attack rate", AIC31XX_DRC_CTRL_REG_3, 4, 0x0F, 0),
	/* DRC decay rate control */
	SOC_SINGLE("DRC decay rate", AIC31XX_DRC_CTRL_REG_3, 0, 0x0F, 0),
	/* MIC1LP selection for ADC I/P P-terminal */
	SOC_ENUM("MIC1LP selection for ADC I/P P - terminal", aic31xx_enum[MIC1LP_ENUM]),
	/* MIC1RP selection for ADC I/P P-terminal */
	SOC_ENUM("MIC1RP selection for ADC I/P P - terminal", aic31xx_enum[MIC1RP_ENUM]),
	/* MIC1LM selection for ADC I/P P-terminal */
	SOC_ENUM("MIC1LM selection for ADC I/P P - terminal", aic31xx_enum[MIC1LM_ENUM]),
	/* CM selection for ADC I/P M-terminal */
	SOC_ENUM("CM selection for ADC IP M - terminal", aic31xx_enum[CM_ENUM]),
	/* MIC1LM selection for ADC I/P M-terminal */
	SOC_ENUM("MIC1LM selection for ADC I/P M - terminal", aic31xx_enum[MIC1LMM_ENUM]),
	/* MIC PGA Setting */
	SOC_ENUM("MIC PGA Setting", aic31xx_enum[MIC_ENUM]),
	/* MIC1LP CM Setting */
	SOC_ENUM("MIC1LP CM Setting", aic31xx_enum[MIC1_ENUM]),
	/* MIC1RP CM Setting */
	SOC_ENUM("MIC1RP CM Setting", aic31xx_enum[MIC2_ENUM]),
	/* MIC1LP CM Setting */
	SOC_ENUM("MIC1LM CM Setting", aic31xx_enum[MIC3_ENUM]),
	/* ADC mute control */
	SOC_ENUM("ADC Mute", aic31xx_enum[ADCMUTE_ENUM]),
	/* DAC Left & Right Power Control */
	SOC_ENUM("LDAC_PWR_CTL", aic31xx_enum[LDAC_ENUM]),
	SOC_ENUM("RDAC_PWR_CTL", aic31xx_enum[RDAC_ENUM]),

	/* HP Driver Power up/down control */
	SOC_ENUM("HPL_PWR_CTL", aic31xx_enum[HPL_ENUM]),
	SOC_ENUM("HPR_PWR_CTL", aic31xx_enum[HPR_ENUM]),

	/* MIC PGA Enable/Disable */
	SOC_SINGLE("MIC_PGA_EN_CTL", AIC31XX_MICPGA_VOL_CTRL, 7, 2, 0),

	/* HP Detect Debounce Time */
	SOC_SINGLE("HP_DETECT_DEBOUNCE_TIME", AIC31XX_HS_DETECT_REG, 2, 0x05, 0),
	/* HP Button Press Debounce Time */
	SOC_SINGLE("HP_BUTTON_DEBOUNCE_TIME", AIC31XX_HS_DETECT_REG, 0, 0x03, 0),
	/* Added for Debugging */
	SOC_SINGLE("LoopBack_Control", AIC31XX_INTERFACE_SET_REG_2, 4, 4, 0),


#ifndef AIC3100_CODEC_SUPPORT
	/* For AIC3110 output is stereo so we are using	SOC_DOUBLE_R macro */

	/* SP Class-D driver output stage gain Control */
	SOC_DOUBLE_R_TLV("Class - D driver Volume", AIC31XX_SPL_DRIVER_REG,
		AIC31XX_SPR_DRIVER_REG, 3, 0x04, 0, class_D_drv_tlv),
#else
	/* SP Class-D driver output stage gain Control */
	SOC_SINGLE("Class - D driver Volume",
		AIC31XX_SPL_DRIVER_REG, 3, 0x04, 0),
#endif

	/* HP Analog Gain Volume Control */
	SOC_DOUBLE_R_TLV("HP Analog Gain", AIC31XX_LEFT_ANALOG_HPL,
		AIC31XX_RIGHT_ANALOG_HPR, 0, 0x7F, 1, hp_vol_tlv),

#ifndef AIC3100_CODEC_SUPPORT
	/* SP Analog Gain Volume Control */
	SOC_DOUBLE_R_TLV("SP Analog Gain", AIC31XX_LEFT_ANALOG_SPL,
		AIC31XX_RIGHT_ANALOG_SPR, 0, 0x7F, 1, sp_vol_tlv),
#else
	/* SP Analog Gain Volume Control */
	SOC_SINGLE("SP Analog Gain",
		AIC31XX_LEFT_ANALOG_SPL, 0, 0x7F, 1),
#endif
};


/*
 * The global Register Initialization sequence Array. During the Audio
 * Driver initialization, this array will be utilized to perform the
 * default initialization of the audio Driver.
 */
static const struct
aic31xx_configs aic31xx_reg_init[] = {

	/* Clock settings */
	{AIC31XX_CLK_R1,		CODEC_MUX_VALUE},
	{AIC31XX_INTERFACE_SET_REG_1,	BCLK_DIR_CTRL},
	{AIC31XX_INTERFACE_SET_REG_2,	DAC_MOD_CLK_2_BDIV_CLKIN},

	/* POP_REMOVAL: Step_1: Setting HP in weakly driver common mode */
	{AIC31XX_HPL_DRIVER_REG,	0x00},
	{AIC31XX_HPR_DRIVER_REG,	0x00},

	/* Step_2: HP pop removal settings */
	{AIC31XX_HP_OUT_DRIVERS,	CM_VOLTAGE_FROM_AVDD},

	/* Step_3: Configuring HP in Line out Mode */
	{AIC31XX_HP_DRIVER_CONTROL,	0x06},

	/* Step_4: Powering up the HP in Line out mode */
	{AIC31XX_HPHONE_DRIVERS,	HP_DRIVER_ON},

	/* Step_5: Reconfiguring the CM to Band Gap mode */
	{AIC31XX_HP_OUT_DRIVERS,	BIT7 | HP_POWER_UP_76_2_MSEC | HP_DRIVER_3_9_MS | CM_VOLTAGE_FROM_BAND_GAP},

	/* Speaker Ramp up time scaled to 30.5ms */
	{AIC31XX_PGA_RAMP_REG,		0x70},

	/* Headset Detect setting */
	{AIC31XX_INT1_CTRL_REG,		AIC31XX_HEADSET_IN_MASK | AIC31XX_BUTTON_PRESS_MASK},

	/* previous value was 0x01 */
	{AIC31XX_MICPGA_CM_REG,		0x20},

	/* short circuit protection of HP and Speaker power bits */
	{AIC31XX_HP_SPK_ERR_CTL,	3},

	/* Headset detection enabled by default and Debounce programmed to 32 ms
	 * for Headset Detection and 32ms for Headset button-press Detection
	 */
	{AIC31XX_HS_DETECT_REG,		HP_DEBOUNCE_32_MS | HS_DETECT_EN | HS_BUTTON_PRESS_32_MS},

};

/*
 *----------------------------------------------------------------------------
 * Function : aic31xx_codec_probe
 * Purpose  : This is first driver function called by the SoC core driver.
 *
 *----------------------------------------------------------------------------
 */
static int aic31xx_codec_probe(struct snd_soc_codec *codec)
{


	int ret = 0, i;
	struct aic3xxx *control;
	struct aic31xx_priv *priv;
	struct aic31xx_jack_data *jack;
	struct i2c_adapter *adapter;

	if (codec == NULL)
		dev_err(codec->dev, "codec pointer is NULL.\n");


	control = kzalloc(sizeof(*control), GFP_KERNEL);
	if (control == NULL)
		return -ENOMEM;

	adapter = i2c_get_adapter(3);
	if (!adapter) {
		DBG("##Can't get i2c adapter\n");
		ret = -ENODEV;
		return ret;
	}
	DBG("##i2c_get_adapter success. Creating a new i2c client device..\n");

	tlv320aic31xx_client = i2c_new_device(adapter, &tlv320aic31xx_hwmon_info);
	if (!tlv320aic31xx_client) {
		DBG("##can't add i2c device\n");
		ret = -ENODEV;
		return ret;
	}
	DBG("##i2c_device Pntr %x\n", (unsigned int) tlv320aic31xx_client);
	i2c_set_clientdata(tlv320aic31xx_client, control);
	control->dev = &tlv320aic31xx_client->dev;
	control->irq = tlv320aic31xx_client->irq;

	ret = aic3xxx_device_init(control);
	if (ret) {
		DBG("##Can't init i2c aic3xxx device\n");
		ret = -ENODEV;
	}

	codec->control_data = dev_get_drvdata(codec->dev->parent);

	priv = kzalloc(sizeof(struct aic31xx_priv), GFP_KERNEL);

	if (priv == NULL)
		return -ENOMEM;

	snd_soc_codec_set_drvdata(codec, priv);
	priv->pdata = dev_get_platdata(codec->dev->parent);
	priv->codec = codec;
	priv->playback_status = 0;
	priv->power_status = 0;
	priv->mute = 1;
	priv->headset_connected = 0;
	priv->i2c_regs_status = 0;

	aic31xx_codec_write(codec, AIC31XX_RESET_REG , 0x01);
	mdelay(10);

	for (i = 0; i < sizeof(aic31xx_reg_init)/sizeof(struct aic31xx_configs); i++) {
		aic31xx_codec_write(codec, aic31xx_reg_init[i].reg, aic31xx_reg_init[i].reg_val);
		mdelay(5);
	}

	priv->power_status = 1;
	priv->headset_connected = 1;
	priv->mute = 0;
	aic31xx_dac_mute(codec, 1);
	priv->headset_connected = 0;
	priv->headset_current_status = 0;

	mutex_init(&priv->mutex);
	mutex_init(&codec->mutex);
	/* use switch-class based headset reporting if platform requires it */
	jack = &priv->hs_jack;

	priv->idev = input_allocate_device();

	if (priv->idev <= 0) {

		dev_dbg(codec->dev, "Allocate failed\n");
		goto input_dev_err;
	}

	input_set_capability(priv->idev, EV_KEY, KEY_MEDIA);

	ret = input_register_device(priv->idev);
	if (ret < 0) {
		dev_err(codec->dev, "register input dev fail\n");
		goto input_dev_err;
	}

	/* Dynamic Headset detection enabled */
	snd_soc_update_bits(codec, AIC31XX_HS_DETECT_REG,
		AIC31XX_HEADSET_IN_MASK, AIC31XX_HEADSET_IN_MASK);

	/* off, with power on */
	aic31xx_set_bias_level(codec, SND_SOC_BIAS_STANDBY);

#if 0
	ret = snd_soc_dapm_new_widgets(&codec->dapm);
	if (!ret)
		snd_printd(KERN_ERR "widgets updated\n");
#endif

	return 0;


input_dev_err:
	input_unregister_device(priv->idev);
	input_free_device(priv->idev);
	kfree(priv);

	return 0;
}

/*
 * Remove aic31xx soc device
 */
static int aic31xx_codec_remove(struct snd_soc_codec *codec)
{
	struct aic31xx_priv *aic31xx = snd_soc_codec_get_drvdata(codec);
	struct aic3xxx *control = codec->control_data;

	/* power down chip */
	aic31xx_set_bias_level(codec, SND_SOC_BIAS_OFF);

	if (control->irq) {
		aic3xxx_free_irq(control, AIC31XX_IRQ_HEADSET_DETECT, codec);
		aic3xxx_free_irq(control, AIC31XX_IRQ_BUTTON_PRESS, codec);
	}

	aic3xxx_device_exit(control);

	/* Unregister input device and switch */
	input_unregister_device(aic31xx->idev);
	input_free_device(aic31xx->idev);
	kfree(aic31xx);

	return 0;
}


static int aic31xx_suspend(struct snd_soc_codec *codec)
{
	struct aic31xx_priv *aic31xx = snd_soc_codec_get_drvdata(codec);
        u8 val;

        dev_dbg(codec->dev, "%s: Entered\n", __func__);

        if (aic31xx->playback_status == 0) {
                aic31xx_set_bias_level(codec, SND_SOC_BIAS_OFF);

                /* Bit 7 of Page 1/ Reg 46 gives the soft powerdown control.
                 * Setting this bit will further reduces the amount of power
                 * consumption
                 */
                val = snd_soc_read(codec, AIC31XX_MICBIAS_CTRL_REG);
                snd_soc_write(codec, AIC31XX_MICBIAS_CTRL_REG, val | BIT7);

		/* Disable Audio clock from FREF_CLK2_OUT */
		omap_writew(omap_readw(0x4a30a318) & 0xFEFF, 0x4a30a318);

		regulator_disable(audio_regulator);
        }
        dev_dbg(codec->dev, "%s: Exiting\n", __func__);
        return 0;
}


static int aic31xx_resume(struct snd_soc_codec *codec)
{
        struct aic31xx_priv *aic31xx = snd_soc_codec_get_drvdata(codec);
        u8 val;

        dev_dbg(codec->dev, "###aic31xx_resume\n");
        dev_dbg(codec->dev, "%s: Entered\n", __func__);

	if (regulator_set_voltage(audio_regulator, aic31xx->pdata->regulator_min_uV, aic31xx->pdata->regulator_max_uV))
		printk(KERN_INFO "%s: regulator_set 3V error\n", __func__);

	regulator_enable(audio_regulator);

	/* Enable Audio clock from FREF_CLK2_OUT */
	omap_writew(omap_readw(0x4a30a318) | ~0xFEFF, 0x4a30a318);

        val = snd_soc_read(codec, AIC31XX_MICBIAS_CTRL_REG);
        snd_soc_write(codec, AIC31XX_MICBIAS_CTRL_REG, val & ~BIT7);

	/* Mute the codec to avoid pops while coming out of Suspend */
	aic31xx->mute = 0;
	aic31xx_mute_codec(codec, 1);

        dev_dbg(codec->dev, "%s: Exiting\n", __func__);
        return 0;

}

/*
 *----------------------------------------------------------------------------
 * @struct  snd_soc_codec_device |
 *          This structure is soc audio codec device sturecute which pointer
 *          to basic functions aic31xx_probe(), aic31xx_remove(),
 *          aic31xx_suspend() and aic31xx_resume()
 *----------------------------------------------------------------------------
 */
static struct snd_soc_codec_driver soc_codec_driver_aic31xx = {
	.probe			= aic31xx_codec_probe,
	.remove			= aic31xx_codec_remove,
	.suspend		= aic31xx_suspend,
	.resume			= aic31xx_resume,
	.set_bias_level		= aic31xx_set_bias_level,
	.read			= aic31xx_codec_read,
	.write			= aic31xx_codec_write,
	.reg_cache_size		= 0,
	.reg_word_size		= sizeof(u8),
	.reg_cache_default	= NULL,

	.controls		= aic31xx_snd_controls,
	.num_controls		= ARRAY_SIZE(aic31xx_snd_controls),
	.dapm_widgets		= aic31xx_dapm_widgets,
	.num_dapm_widgets	= ARRAY_SIZE(aic31xx_dapm_widgets),
	.dapm_routes		= aic31xx_audio_map,
	.num_dapm_routes	= ARRAY_SIZE(aic31xx_audio_map),
};

/*
 *----------------------------------------------------------------------------
 * @struct  snd_soc_codec_dai |
 *		It is SoC Codec DAI structure which has DAI capabilities viz.,
 *		playback and capture, DAI runtime information viz. state of DAI
 *		and pop wait state, and DAI private data.
 *		The AIC31xx rates ranges from 8k to 192k
 *		The PCM bit format supported are 16, 20, 24 and 32 bits
 *----------------------------------------------------------------------------
 */

/*
 * DAI ops
 */

static struct snd_soc_dai_ops aic31xx_dai_ops = {
	.hw_params	= aic31xx_hw_params,
	.digital_mute	= aic31xx_mute,
	.set_sysclk	= aic31xx_set_dai_sysclk,
	.set_fmt	= aic31xx_set_dai_fmt,
};

/*
 * It is SoC Codec DAI structure which has DAI capabilities viz.,
 * playback and capture, DAI runtime information viz. state of DAI and
 * pop wait state, and DAI private data.  The aic31xx rates ranges
 * from 8k to 192k The PCM bit format supported are 16, 20, 24 and 32
 * bits
 */
static struct snd_soc_dai_driver aic31xx_dai_driver[] = {
	{
		.name = "tlv320aic31xx-MM_EXT",
		.playback = {
			.stream_name	 = "Playback",
			.channels_min	 = 1,
			.channels_max	 = 2,
			.rates		 = AIC31XX_RATES,
			.formats	 = AIC31XX_FORMATS,
		},
		.capture = {
			.stream_name	 = "Capture",
			.channels_min	 = 1,
			.channels_max	 = 1,
			.rates		 = AIC31XX_RATES,
			.formats	 = AIC31XX_FORMATS,
		},
		.ops = &aic31xx_dai_ops,
	},
};


static int aic31xx_probe(struct platform_device *pdev)
{
	int ret;
	DBG("Came to tlv320aic31xx_codec_probe...\n");

	ret = snd_soc_register_codec(&pdev->dev, &soc_codec_driver_aic31xx,
			aic31xx_dai_driver, ARRAY_SIZE(aic31xx_dai_driver));

	DBG("snd_soc_register_codec returned %d\n", ret);
	return ret;
}

static int aic31xx_remove(struct platform_device *pdev)
{
	snd_soc_unregister_codec(&pdev->dev);
	return 0;
}

static struct platform_driver aic31xx_codec_driver = {
	.driver = {
		.name	 = "tlv320aic31xx-codec",
		.owner	 = THIS_MODULE,
	},
	.probe	 = aic31xx_probe,
	.remove	 = __devexit_p(aic31xx_remove),
};

module_platform_driver(aic31xx_codec_driver);

MODULE_DESCRIPTION("ASoC TLV320AIC3111 codec driver");
MODULE_AUTHOR("naresh@ti.com");
MODULE_AUTHOR("vinod.raghunathan@symphonysv.com");
MODULE_LICENSE("GPL");
