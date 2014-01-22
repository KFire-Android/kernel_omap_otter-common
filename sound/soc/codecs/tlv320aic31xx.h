/*
 * linux/sound/soc/codecs/tlv320aic31xx.h
 *
 * Copyright (C) 2011 Texas Instruments, Inc.
 *
 * Based on sound/soc/codecs/tlv320aic31xx.c
 *
 * This package is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * THIS PACKAGE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 * The TLV320AIC31XX is a flexible, low-power, low-voltage stereo audio
 * codec with digital microphone inputs and programmable outputs.
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
 * Rev 0.5   Updated the code-base with ADC related register #defines.
 *
 * Rev 0.6   Updated the code-base with DAC and HP related ENUMS
 *
 * Rev 0.7   Ported to 3.0 Kernel			20-01-2012

 * Rev 0.8   Ported to 3.4 Kernel			21-01-2014
 */

#ifndef _TLV320AIC31XX_H
#define _TLV320AIC31XX_H

#define AIC31XX_VERSION "0.8"

#define AIC31x_CODEC_DEBUG 1

#ifdef AIC31x_CODEC_DEBUG
	#define dprintk(x...)   printk(x)
	#define DBG(x...)       printk(x)
#else
	#define dprintk(x...)
	#define DBG(x...)
#endif

/* Macro to enable AIC3100 codec support */
//#define AIC3110_CODEC_SUPPORT

/*
 ****************************************************************************
 * BOARD SPECIFIC DEFINES
 ****************************************************************************
 */

#ifndef AIC3100_CODEC_SUPPORT

#define AUDIO_NAME "aic31xx"
#define AIC3110_VERSION "0.1"

/* The headset jack insertion GPIO used on the Qoo Board */
#define Qoo_HEADSET_DETECT_GPIO_PIN		49

/* Regulator Minimum Voltage */
#define REGU_MIN_VOL			3000000

/* Regulator Maximum Voltage */
#define REGU_MAX_VOL			3000000

/* 5V Boost Voltage GPIO Input on Qoo Board */
#define ADO_SPK_ENABLE_GPIO		119

/* Audio Gain Tuning as per the Qoo Board */
#define Qoo_AUDIO_GAIN_TUNING_SELECT 1

#else

#define AUDIO_NAME "aic3100"
#define AIC3100_VERSION "0.1"

#endif

/* Headphones Driver */
#define HP_DRIVER_ON                    0xCC
#define HP_DRIVER_OFF                   0x0C

/* Speaker Driver */
#define SPK_DRV_ON			0xC6
#define SPK_DRV_OFF			0x06


#define AIC31XX_RATES	(SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_11025 | \
			SNDRV_PCM_RATE_16000 | SNDRV_PCM_RATE_22050 | \
			SNDRV_PCM_RATE_32000 | SNDRV_PCM_RATE_44100 | \
			SNDRV_PCM_RATE_48000 | SNDRV_PCM_RATE_96000 | \
			SNDRV_PCM_RATE_192000)

/* AIC31XX supports the word formats 16bits, 20bits, 24bits and 32 bits */
#define AIC31XX_FORMATS	(SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S20_3LE \
			 | SNDRV_PCM_FMTBIT_S24_3LE | SNDRV_PCM_FMTBIT_S32_LE)

#define AIC31XX_FREQ_12000000 12000000
#define AIC31XX_FREQ_24000000 24000000
#define AIC31XX_FREQ_19200000 19200000

/* Audio data word length = 16-bits (default setting) */
#define AIC31XX_WORD_LEN_16BITS		0x00
#define AIC31XX_WORD_LEN_20BITS		0x01
#define AIC31XX_WORD_LEN_24BITS		0x02
#define AIC31XX_WORD_LEN_32BITS		0x03
#define DAC_OSR_MSB_SHIFT				4

/* sink: name of target widget */
#define AIC31XX_WIDGET_NAME			0
/* control: mixer control name */
#define AIC31XX_CONTROL_NAME		1
/* source: name of source name */
#define AIC31XX_SOURCE_NAME			2

/* D15..D8 aic31xx register offset */
#define AIC31XX_REG_OFFSET_INDEX    0
/* D7...D0 register data */
#define AIC31XX_REG_DATA_INDEX      1

/* Serial data bus uses I2S mode (Default mode) */
#define AIC31XX_I2S_MODE			0x00
#define AIC31XX_DSP_MODE			0x01
#define AIC31XX_RIGHT_JUSTIFIED_MODE		0x02
#define AIC31XX_LEFT_JUSTIFIED_MODE		0x03
#define AUDIO_MODE_SHIFT			6

/* 8 bit mask value */
#define AIC31XX_8BITS_MASK	0xFF

/* shift value for CLK_REG_3 register */
#define CLK_REG_3_SHIFT					6

/* number of codec specific register for configuration */
#define NO_FEATURE_REGS				2

/****************************************************************************/
/****************************************************************************/
#define BIT7		(1 << 7)
#define BIT6		(1 << 6)
#define BIT5		(1 << 5)
#define BIT4		(1 << 4)
#define	BIT3		(1 << 3)
#define BIT2		(1 << 2)
#define BIT1		(1 << 1)
#define BIT0		(1 << 0)

#define HP_UNMUTE			BIT2
#define HPL_UNMUTE			BIT3
#define ENABLE_DAC_CHN			(BIT6 | BIT7)
#define ENABLE_ADC_CHN			(BIT6 | BIT7)
#define BCLK_DIR_CTRL			(BIT2 | BIT3)
#define CODEC_CLKIN_MASK		0x03
#define MCLK_2_CODEC_CLKIN		0x00
#define CODEC_MUX_VALUE			0X03
/*Bclk_in selection*/
#define BDIV_CLKIN_MASK			0x03
#define DAC_MOD_CLK_2_BDIV_CLKIN	BIT0
#define ADC_MOD_CLK_2_BDIV_CLKIN	(BIT1 | BIT0)
#define SOFT_RESET			0x01
#define PAGE0				0x00
#define PAGE1				0x01
#define CLEAR				0x00
#define BIT_CLK_MASTER			BIT3
#define WORD_CLK_MASTER			BIT2
#define BCLK_INV_MASK			0x08
#define HIGH_PLL			BIT6
#define ENABLE_PLL			BIT7
#define ENABLE_NDAC			BIT7
#define ENABLE_MDAC			BIT7
#define ENABLE_NADC			BIT7
#define ENABLE_MADC			BIT7
#define ENABLE_BCLK			BIT7
#define ENABLE_DAC			(0x03 << 6)
#define ENABLE_ADC			BIT7
#define LDAC_2_LCHN			BIT4
#define RDAC_2_RCHN			BIT2
#define RDAC_2_RAMP			BIT2
#define LDAC_2_LAMP			BIT6
#define LDAC_CHNL_2_HPL			BIT3
#define RDAC_CHNL_2_HPR			BIT3
#define LDAC_LCHN_RCHN_2		(BIT4 | BIT5)
#define SOFT_STEP_2WCLK			BIT0
#define MUTE_ON				0x0C
/* DEFAULT VOL MODIFIED [OLD VAL = 0XFC & 0x81] */
#define DAC_DEFAULT_VOL			0xB0
/* HP DEFAULT VOL Updated from -78.3db to -16db */
#define HP_DEFAULT_VOL			0x20
#define SPK_DEFAULT_VOL			0x75
#define DISABLE_ANALOG			BIT3
#define LDAC_2_HPL_ROUTEON		BIT3
#define RDAC_2_HPR_ROUTEON		BIT3
#define LINEIN_L_2_LMICPGA_10K		BIT6
#define LINEIN_L_2_LMICPGA_20K		BIT7
#define LINEIN_L_2_LMICPGA_40K		(0x3 << 6)
#define LINEIN_R_2_RMICPGA_10K		BIT6
#define LINEIN_R_2_RMICPGA_20K		BIT7
#define LINEIN_R_2_RMICPGA_40K		(0x3 << 6)

/* Headphone POP Removal Settings defines */
#define HP_POWER_UP_0_USEC	0
#define HP_POWER_UP_15_3_USEC   BIT3
#define HP_POWER_UP_153_USEC    BIT4
#define HP_POWER_UP_1_53_MSEC   (BIT4 | BIT3)
#define HP_POWER_UP_15_3_MSEC   BIT5
#define HP_POWER_UP_76_2_MSEC   (BIT5 | BIT3)
#define HP_POWER_UP_153_MSEC    (BIT5 | BIT4)
#define HP_POWER_UP_304_MSEC    (BIT5 | BIT4 | BIT3)
#define HP_POWER_UP_610_MSEC    (BIT6)
#define HP_POWER_UP_1_2_SEC     (BIT6 | BIT3)
#define HP_POWER_UP_3_04_SEC	(BIT6 | BIT4)
#define HP_POWER_UP_6_1_SEC	(BIT6 | BIT4 | BIT3)
/* Driver Ramp-up Time Settings */
#define HP_DRIVER_0_MS		0
#define HP_DRIVER_0_98_MS	BIT1
#define HP_DRIVER_1_95_MS	BIT2
#define HP_DRIVER_3_9_MS	(BIT2 | BIT1)

/* Common Mode VOltage SEttings */
#define CM_VOLTAGE_FROM_AVDD		0
#define CM_VOLTAGE_FROM_BAND_GAP	BIT0

/* HP Debounce Time Settings */
#define HP_DEBOUNCE_32_MS	BIT2
#define HP_DEBOUNCE_64_MS	BIT3
#define HP_DEBOUNCE_128_MS	(BIT3 | BIT2)
#define HP_DEBOUNCE_256_MS	BIT4
#define HP_DEBOUNCE_512_MS	(BIT4 | BIT2)
#define HP_DEBOUNCE_TIME_IN_MS	(64)

/*HS DETECT ENABLE*/
#define HS_DETECT_EN		BIT7

/*HS BUTTON PRESS TIME */
#define HS_BUTTON_PRESS_8_MS	BIT0
#define HS_BUTTON_PRESS_16_MS	BIT1
#define HS_BUTTON_PRESS_32_MS	(BIT1 | BIT0)


/****************************************************************************
 * DAPM widget related #defines
 ***************************************************************************
 */
#define LMUTE_ENUM		0
#define RMUTE_ENUM		1
#define DACEXTRA_ENUM		2
#define DACCONTROL_ENUM		3
#define SOFTSTEP_ENUM		4
#define BEEP_ENUM		5
#define MICBIAS_ENUM		6
#define DACLEFTIP_ENUM		7
#define DACRIGHTIP_ENUM		8
#define VOLTAGE_ENUM		9
#define HSET_ENUM		10
#define DRC_ENUM		11
#define MIC1LP_ENUM		12
#define MIC1RP_ENUM		13
#define MIC1LM_ENUM		14
#define MIC_ENUM		15
#define CM_ENUM			16
#define MIC1LMM_ENUM		17
#define MIC1_ENUM		18
#define MIC2_ENUM		19
#define MIC3_ENUM		20
#define ADCMUTE_ENUM		21
#define HPL_ENUM		22
#define HPR_ENUM		23
#define LDAC_ENUM		24
#define RDAC_ENUM		25


/* Masks used for updating register bits */
#define INTERFACE_REG1_DATA_LEN_MASK    0x30
#define INTERFACE_REG1_DATA_LEN_SHIFT   (4)
#define INTERFACE_REG1_DATA_TYPE_MASK   0xC0
#define INTERFACE_REG1_DATA_TYPE_SHIFT  (6)
#define INTERFACE_REG1_MASTER_MASK      0x0C
#define INTERFACE_REG1_MASTER_SHIFT     (2)
#define INTERFACE_REG1_MASK             0x7F
#define INTERFACE_REG2_MASK             0xFF
#define INTERFACE_REG3_MASK             0x08

#define AIC31XX_MAKE_REG(book, page, offset) \
			(unsigned int)((book << 16)|(page << 8)|offset)

/* ****************** Book 0 Registers **************************************/
/* ****************** AIC31XX has one book only *****************************/

/* ****************** Page 0 Registers **************************************/
/* Page select register */
#define AIC31XX_PAGE_SEL_REG		AIC31XX_MAKE_REG(0, 0, 0)
/* Software reset register */
#define AIC31XX_RESET_REG		AIC31XX_MAKE_REG(0, 0, 1)
#define AIC31XX_REV_PG_ID		AIC31XX_MAKE_REG(0, 0, 2)
/* Revision and PG-ID */
//#define AIC31XX_REV_PG_ID		AIC31XX_MAKE_REG(0, 0, 3)
#define AIC31XX_REV_MASK		(0b01110000)
#define AIC31XX_REV_SHIFT		4
#define AIC31XX_PG_MASK			(0b00000001)
#define AIC31XX_PG_SHIFT		0
/* OT FLAG register */
#define AIC31XX_OT_FLAG_REG		AIC31XX_MAKE_REG(0, 0, 3)

/* Clock clock Gen muxing, Multiplexers*/
#define AIC31XX_CLK_R1			AIC31XX_MAKE_REG(0, 0, 4)
#define AIC31XX_PLL_CLKIN_MASK		(0b00001100)
#define AIC31XX_PLL_CLKIN_SHIFT		2
#define AIC31XX_PLL_CLKIN_MCLK		0
/* PLL P and R-VAL register*/
#define AIC31XX_CLK_R2			AIC31XX_MAKE_REG(0, 0, 5)
/* PLL J-VAL register*/
#define AIC31XX_CLK_R3			AIC31XX_MAKE_REG(0, 0, 6)
/* PLL D-VAL MSB register */
#define AIC31XX_CLK_R4			AIC31XX_MAKE_REG(0, 0, 7)
/* PLL D-VAL LSB register */
#define AIC31XX_CLK_R5			AIC31XX_MAKE_REG(0, 0, 8)
/* DAC NDAC_VAL register*/
#define AIC31XX_NDAC_CLK_R6		AIC31XX_MAKE_REG(0, 0, 11)
/* DAC MDAC_VAL register */
#define AIC31XX_MDAC_CLK_R7		AIC31XX_MAKE_REG(0, 0, 12)
/*DAC OSR setting register1, MSB value*/
#define AIC31XX_DAC_OSR_MSB		AIC31XX_MAKE_REG(0, 0, 13)
/*DAC OSR setting register 2, LSB value*/
#define AIC31XX_DAC_OSR_LSB		AIC31XX_MAKE_REG(0, 0, 14)
#define AIC31XX_MINI_DSP_INPOL		AIC31XX_MAKE_REG(0, 0, 16)
/*Clock setting register 8, PLL*/
#define AIC31XX_NADC_CLK_R8		AIC31XX_MAKE_REG(0, 0, 18)
/*Clock setting register 9, PLL*/
#define AIC31XX_MADC_CLK_R9		AIC31XX_MAKE_REG(0, 0, 19)
/*ADC Oversampling (AOSR) Register*/
#define AIC31XX_ADC_OSR_REG		AIC31XX_MAKE_REG(0, 0, 20)
/*ADC minidsp engine decimation*/
#define AIC31XX_ADC_DSP_DECI		AIC31XX_MAKE_REG(0, 0, 22)
/*Clock setting register 9, Multiplexers*/
#define AIC31XX_CLK_MUX_R9		AIC31XX_MAKE_REG(0, 0, 25)
/*Clock setting register 10, CLOCKOUT M divider value*/
#define AIC31XX_CLK_R10			AIC31XX_MAKE_REG(0, 0, 26)
/*Audio Interface Setting Register 1*/
#define AIC31XX_INTERFACE_SET_REG_1	AIC31XX_MAKE_REG(0, 0, 27)
/*Audio Data Slot Offset Programming*/
#define AIC31XX_DATA_SLOT_OFFSET	AIC31XX_MAKE_REG(0, 0, 28)
/*Audio Interface Setting Register 2*/
#define AIC31XX_INTERFACE_SET_REG_2	AIC31XX_MAKE_REG(0, 0, 29)
/*Clock setting register 11, BCLK N Divider*/
#define AIC31XX_CLK_R11			AIC31XX_MAKE_REG(0, 0, 30)
/*Audio Interface Setting Register 3, Secondary Audio Interface*/
#define AIC31XX_INTERFACE_SET_REG_3	AIC31XX_MAKE_REG(0, 0, 31)
/*Audio Interface Setting Register 4*/
#define AIC31XX_INTERFACE_SET_REG_4	AIC31XX_MAKE_REG(0, 0, 32)
/*Audio Interface Setting Register 5*/
#define AIC31XX_INTERFACE_SET_REG_5	AIC31XX_MAKE_REG(0, 0, 33)
/* I2C Bus Condition */
#define AIC31XX_I2C_FLAG_REG		AIC31XX_MAKE_REG(0, 0, 34)
/* ADC FLAG */
#define AIC31XX_ADC_FLAG		AIC31XX_MAKE_REG(0, 0, 36)
/* DAC Flag Registers */
#define AIC31XX_DAC_FLAG_1		AIC31XX_MAKE_REG(0, 0, 37)
#define AIC31XX_DAC_FLAG_2		AIC31XX_MAKE_REG(0, 0, 38)

/* Sticky Interrupt flag (overflow) */
#define AIC31XX_OVERFLOW_FLAG_REG	AIC31XX_MAKE_REG(0, 0, 39)
#define AIC31XX_LEFT_DAC_OVERFLOW_INT			0x80
#define AIC31XX_RIGHT_DAC_OVERFLOW_INT			0x40
#define AIC31XX_MINIDSP_D_BARREL_SHIFT_OVERFLOW_INT	0x20
#define AIC31XX_ADC_OVERFLOW_INT			0x08
#define AIC31XX_MINIDSP_A_BARREL_SHIFT_OVERFLOW_INT	0x02

#define AIC31XX_INT_STICKY_FLAG1	AIC31XX_MAKE_REG(0, 0, 42)
#define AIC31XX_LEFT_DAC_OVERFLOW_INT			0x80
#define AIC31XX_RIGHT_DAC_OVERFLOW_INT			0x40
#define AIC31XX_MINIDSP_D_BARREL_SHIFT_OVERFLOW_INT	0x20
#define AIC31XX_ADC_OVERFLOW_INT			0x08
#define AIC31XX_LEFT_ADC_OVERFLOW_INT			0x08
#define AIC31XX_RIGHT_ADC_OVERFLOW_INT			0x04
#define AIC31XX_MINIDSP_A_BARREL_SHIFT_OVERFLOW_INT	0x02

/* Sticky Interrupt flags 1 and 2 registers (DAC) */
#define AIC31XX_INTR_DAC_FLAG_1		AIC31XX_MAKE_REG(0, 0, 44)
#define AIC31XX_LEFT_OUTPUT_DRIVER_OVERCURRENT_INT	0x80
#define AIC31XX_RIGHT_OUTPUT_DRIVER_OVERCURRENT_INT	0x40
#define AIC31XX_BUTTON_PRESS_INT			0x20
#define AIC31XX_HEADSET_PLUG_UNPLUG_INT			0x10
#define AIC31XX_LEFT_DRC_THRES_INT			0x08
#define AIC31XX_RIGHT_DRC_THRES_INT			0x04
#define AIC31XX_MINIDSP_D_STD_INT			0x02
#define AIC31XX_MINIDSP_D_AUX_INT			0x01

#define AIC31XX_INTR_DAC_FLAG_2		AIC31XX_MAKE_REG(0, 0, 46)

/* Sticky Interrupt flags 1 and 2 registers (ADC) */
#define AIC31XX_INTR_ADC_FLAG_1		AIC31XX_MAKE_REG(0, 0, 45)
#define AIC31XX_AGC_NOISE_INT		0x40
#define AIC31XX_MINIDSP_A_STD_INT	0x10
#define AIC31XX_MINIDSP_A_AUX_INT	0x08

#define AIC31XX_INTR_ADC_FLAG_2		AIC31XX_MAKE_REG(0, 0, 47)

/* INT1 interrupt control */
#define AIC31XX_INT1_CTRL_REG		AIC31XX_MAKE_REG(0, 0, 48)
#define AIC31XX_HEADSET_IN_MASK		0x80
#define AIC31XX_BUTTON_PRESS_MASK	0x40
#define AIC31XX_DAC_DRC_THRES_MASK	0x20
#define AIC31XX_AGC_NOISE_MASK		0x10
#define AIC31XX_OVER_CURRENT_MASK	0x08
#define AIC31XX_ENGINE_GEN__MASK	0x04

/* INT2 interrupt control */
#define AIC31XX_INT2_CTRL_REG		AIC31XX_MAKE_REG(0, 0, 49)

/* GPIO1 control */
#define AIC31XX_GPIO1_IO_CTRL		AIC31XX_MAKE_REG(0, 0, 51)
#define AIC31XX_GPIO_D5_D2		(0b00111100)
#define AIC31XX_GPIO_D2_SHIFT		2

/*DOUT Function Control*/
#define AIC31XX_DOUT_CTRL_REG		AIC31XX_MAKE_REG(0, 0, 53)
/*DIN Function Control*/
#define AIC31XX_DIN_CTL_REG		AIC31XX_MAKE_REG(0, 0, 54)
/*DAC Instruction Set Register*/
#define AIC31XX_DAC_PRB			AIC31XX_MAKE_REG(0, 0, 60)
/*ADC Instruction Set Register*/
#define AIC31XX_ADC_PRB			AIC31XX_MAKE_REG(0, 0, 61)
/*DAC channel setup register*/
#define AIC31XX_DAC_CHN_REG		AIC31XX_MAKE_REG(0, 0, 63)
/*DAC Mute and volume control register*/
#define AIC31XX_DAC_MUTE_CTRL_REG	AIC31XX_MAKE_REG(0, 0, 64)
/*Left DAC channel digital volume control*/
#define AIC31XX_LDAC_VOL_REG		AIC31XX_MAKE_REG(0, 0, 65)
/*Right DAC channel digital volume control*/
#define AIC31XX_RDAC_VOL_REG		AIC31XX_MAKE_REG(0, 0, 66)
/* Headset detection */
#define AIC31XX_HS_DETECT_REG		AIC31XX_MAKE_REG(0, 0, 67)
#define AIC31XX_HS_MASK			(0b01100000)
#define AIC31XX_HP_MASK			(0b00100000)
/* DRC Control Registers */
#define AIC31XX_DRC_CTRL_REG_1		AIC31XX_MAKE_REG(0, 0, 68)
#define AIC31XX_DRC_CTRL_REG_2		AIC31XX_MAKE_REG(0, 0, 69)
#define AIC31XX_DRC_CTRL_REG_3		AIC31XX_MAKE_REG(0, 0, 70)
/* Beep Generator */
#define AIC31XX_BEEP_GEN_L		AIC31XX_MAKE_REG(0, 0, 71)
#define AIC31XX_BEEP_GEN_R		AIC31XX_MAKE_REG(0, 0, 72)
/* Beep Length */
#define AIC31XX_BEEP_LEN_MSB_REG	AIC31XX_MAKE_REG(0, 0, 73)
#define AIC31XX_BEEP_LEN_MID_REG	AIC31XX_MAKE_REG(0, 0, 74)
#define AIC31XX_BEEP_LEN_LSB_REG	AIC31XX_MAKE_REG(0, 0, 75)
/* Beep Functions */
#define AIC31XX_BEEP_SINX_MSB_REG	AIC31XX_MAKE_REG(0, 0, 76)
#define AIC31XX_BEEP_SINX_LSB_REG	AIC31XX_MAKE_REG(0, 0, 77)
#define AIC31XX_BEEP_COSX_MSB_REG	AIC31XX_MAKE_REG(0, 0, 78)
#define AIC31XX_BEEP_COSX_LSB_REG	AIC31XX_MAKE_REG(0, 0, 79)
#define AIC31XX_ADC_CHN_REG		AIC31XX_MAKE_REG(0, 0, 81)
#define AIC31XX_ADC_VOL_FGC		AIC31XX_MAKE_REG(0, 0, 82)
#define AIC31XX_ADC_VOL_CGC		AIC31XX_MAKE_REG(0, 0, 83)

/*Channel AGC Control Register 1*/
#define AIC31XX_CHN_AGC_R1		AIC31XX_MAKE_REG(0, 0, 86)
/*Channel AGC Control Register 2*/
#define AIC31XX_CHN_AGC_R2		AIC31XX_MAKE_REG(0, 0, 87)
/*Channel AGC Control Register 2*/
#define AIC31XX_CHN_AGC_R3		AIC31XX_MAKE_REG(0, 0, 88)
/*Channel AGC Control Register 2*/
#define AIC31XX_CHN_AGC_R4		AIC31XX_MAKE_REG(0, 0, 89)
/*Channel AGC Control Register 2*/
#define AIC31XX_CHN_AGC_R5		AIC31XX_MAKE_REG(0, 0, 90)
/*Channel AGC Control Register 2*/
#define AIC31XX_CHN_AGC_R6		AIC31XX_MAKE_REG(0, 0, 91)
/*Channel AGC Control Register 2*/
#define AIC31XX_CHN_AGC_R7		AIC31XX_MAKE_REG(0, 0, 92)
/* VOL/MICDET-Pin SAR ADC Volume Control */
#define AIC31XX_VOL_MICDECT_ADC		AIC31XX_MAKE_REG(0, 0, 116)
/* VOL/MICDET-Pin Gain*/
#define AIC31XX_VOL_MICDECT_GAIN	AIC31XX_MAKE_REG(0, 0, 117)
/*Power Masks*/
#define AIC31XX_LDAC_POWER_STATUS_MASK		0x80
#define AIC31XX_RDAC_POWER_STATUS_MASK		0x08
#define AIC31XX_ADC_POWER_STATUS_MASK		0x40
/*Time Delay*/
#define AIC31XX_TIME_DELAY		5000
#define AIC31XX_DELAY_COUNTER		100



/******************** Page 1 Registers **************************************/
/* Headphone / Speaker protection power bits */
#define AIC31XX_HP_SPK_ERR_CTL		AIC31XX_MAKE_REG(0, 1, 30)
/* Headphone drivers */
#define AIC31XX_HPHONE_DRIVERS		AIC31XX_MAKE_REG(0, 1, 31)
/* Class-D Speakear Amplifier */
#define AIC31XX_CLASS_D_SPK		AIC31XX_MAKE_REG(0, 1, 32)
/* HP Output Drivers POP Removal Settings */
#define AIC31XX_HP_OUT_DRIVERS		AIC31XX_MAKE_REG(0, 1, 33)
/* Output Driver PGA Ramp-Down Period Control */
#define AIC31XX_PGA_RAMP_REG		AIC31XX_MAKE_REG(0, 1, 34)
/* DAC_L and DAC_R Output Mixer Routing */
#define AIC31XX_DAC_MIXER_ROUTING	AIC31XX_MAKE_REG(0, 1, 35)
/*Left Analog Vol to HPL */
#define AIC31XX_LEFT_ANALOG_HPL		AIC31XX_MAKE_REG(0, 1, 36)
/* Right Analog Vol to HPR */
#define AIC31XX_RIGHT_ANALOG_HPR	AIC31XX_MAKE_REG(0, 1, 37)
/* Left Analog Vol to SPL */
#define AIC31XX_LEFT_ANALOG_SPL		AIC31XX_MAKE_REG(0, 1, 38)
/* Right Analog Vol to SPR */
#define AIC31XX_RIGHT_ANALOG_SPR	AIC31XX_MAKE_REG(0, 1, 39)
/* HPL Driver */
#define AIC31XX_HPL_DRIVER_REG		AIC31XX_MAKE_REG(0, 1, 40)
/* HPR Driver */
#define AIC31XX_HPR_DRIVER_REG		AIC31XX_MAKE_REG(0, 1, 41)
/* SPL Driver */
#define AIC31XX_SPL_DRIVER_REG		AIC31XX_MAKE_REG(0, 1, 42)
/* SPR Driver */
#define AIC31XX_SPR_DRIVER_REG		AIC31XX_MAKE_REG(0, 1, 43)
/* HP Driver Control */
#define AIC31XX_HP_DRIVER_CONTROL	AIC31XX_MAKE_REG(0, 1, 44)
/*MICBIAS Configuration Register*/
#define AIC31XX_MICBIAS_CTRL_REG	AIC31XX_MAKE_REG(0, 1, 46)
/* MIC PGA*/
#define AIC31XX_MICPGA_VOL_CTRL		AIC31XX_MAKE_REG(0, 1, 47)
/* Delta-Sigma Mono ADC Channel Fine-Gain Input Selection for P-Terminal */
#define AIC31XX_MICPGA_PIN_CFG		AIC31XX_MAKE_REG(0, 1, 48)
/* ADC Input Selection for M-Terminal */
#define AIC31XX_MICPGA_MIN_CFG		AIC31XX_MAKE_REG(0, 1, 49)
/* Input CM Settings */
#define AIC31XX_MICPGA_CM_REG		AIC31XX_MAKE_REG(0, 1, 50)

/* ****************** Page 3 Registers **************************************/

/* Timer Clock MCLK Divider */
#define AIC31XX_TIMER_MCLK_DIV				AIC31XX_MAKE_REG(0, 3, 16)

/* ****************** Page 8 Registers **************************************/
#define AIC31XX_DAC_ADAPTIVE_BANK1_REG			AIC31XX_MAKE_REG(0, 8, 1)



/*****************************************************************************
 * Structures Definitions
 *****************************************************************************
 */

struct aic31xx_jack_data {
	struct snd_soc_jack *jack;
	int report;
};

struct aic3xxx_irq_data {
	int mask;
	int status;
};

/*configs |
 *          AIC31XX initialization data which has register offset and register
 *          value.
 * @field   u16 | reg_offset |
 *          AIC31XX Register offsets required for initialization..
 * @field   u8 | reg_val |
 *          value to set the AIC31XX register to initialize the AIC31XX.
 *----------------------------------------------------------------------------
 */
struct aic31xx_configs {
	unsigned int reg;
	u8 reg_val;
};


/*
 *----------------------------------------------------------------------------
 * @struct  dac3100_priv |
 *          dac3100 priviate data structure to set the system clock, mode and
 *          page number.
 * @field   u32 | sysclk |
 *          system clock
 * @field   s32 | master |
 *          master/slave mode setting for dac3100
 * @field   u8 | page_no |
 *          page number. Here, page 0 and page 1 are used.
 * @field   u8 | codec   |
 *          codec strucuture. Used while freeing the Driver Resources
 *----------------------------------------------------------------------------
 */
struct aic31xx_priv {
	u32 sysclk;
	s32 master;
	u8  mute;
	u8  headset_connected;
	u8  headset_current_status;
	u8  power_status;
	u8  playback_status;
	u8  capture_stream;
	struct aic31xx_jack_data hs_jack;
	struct delayed_work delayed_work;
	struct input_dev *idev;
	struct snd_soc_codec *codec;
	struct mutex mutex;
	struct aic3xxx_pdata *pdata;
	u8 i2c_regs_status;
	u8 playback_stream;	/* 1 denotes Playback */
	struct aic31xx_configs hp_analog_right_vol[120];
	struct aic31xx_configs hp_analog_left_vol[120];
};

/*
 *----------------------------------------------------------------------------
 * @struct  aic31xx_rate_divs |
 *          Setting up the values to get different freqencies
 *
 * @field   u32 | mclk |
 *          Master clock
 * @field   u32 | rate |
 *          sample rate
 * @field   u8 | p_val |
 *          value of p in PLL
 * @field   u32 | pll_j |
 *          value for pll_j
 * @field   u32 | pll_d |
 *          value for pll_d
 * @field   u32 | dosr |
 *          value to store dosr
 * @field   u32 | ndac |
 *          value for ndac
 * @field   u32 | mdac |
 *          value for mdac
 * @field   u32 | aosr |
 *          value for aosr
 * @field   u32 | nadc |
 *          value for nadc
 * @field   u32 | madc |
 *          value for madc
 * @field   u32 | blck_N |
 *          value for block N
 * @field   u32 | aic31xx_configs |
 *          configurations for aic31xx register value
 *----------------------------------------------------------------------------
 */
struct aic31xx_rate_divs {
	u32 mclk;
	u32 rate;
	u8 p_val;
	u8 pll_j;
	u16 pll_d;
	u16 dosr;
	u8 ndac;
	u8 mdac;
	u8 aosr;
	u8 nadc;
	u8 madc;
	u8 blck_N;
/*	struct aic31xx_configs codec_specific_regs[NO_FEATURE_REGS];*/
};


#define AIC31XX_IRQ_HEADSET_DETECT	0
#define AIC31XX_IRQ_BUTTON_PRESS	1
#define AIC31XX_IRQ_DAC_DRC		2
#define AIC31XX_IRQ_AGC_NOISE		3
#define AIC31XX_IRQ_OVER_CURRENT	4
#define AIC31XX_IRQ_OVERFLOW_EVENT	5
#define AIC31XX_IRQ_SPEAKER_OVER_TEMP	6

#define AIC31XX_GPIO1			7

typedef union aic3xxx_reg_union {
	struct aic3xxx_reg {
	u8 offset;
	u8 page;
	u8 book;
	u8 reserved;
	} aic3xxx_register;
	unsigned int aic3xxx_register_int;
} aic31xx_reg_union;

struct aic3xxx_gpio_setup {
	unsigned int reg;
	u8 value;
};

struct aic3xxx_pdata {
	unsigned int audio_mclk1;
	unsigned int audio_mclk2;
	unsigned int gpio_reset; /* is the codec being reset by a gpio
				[host] pin, if yes provide the number. */
	int num_gpios;
	struct aic3xxx_gpio_setup *gpio_defaults;/* all gpio configuration */
	unsigned int irq_base;
	char *regulator_name;
	int regulator_min_uV;
	int regulator_max_uV;
};

struct aic3xxx {
	struct mutex io_lock;
	struct mutex irq_lock;
	struct device *dev;
	struct aic3xxx_pdata pdata;
	unsigned int irq;
	unsigned int irq_base;

	u8 irq_masks_cur;
	u8 irq_masks_cache;

	/* Used over suspend/resume */
	bool suspended;

	u8 book_no;
	u8 page_no;
};

static inline int aic3xxx_request_irq(struct aic3xxx *aic3xxx, int irq,
				      irq_handler_t handler,
				      unsigned long irqflags, const char *name,
				      void *data)
{
	if (!aic3xxx->irq_base)
		return -EINVAL;

	return request_threaded_irq(irq, NULL, handler,
				    irqflags, name, data);
}

static inline int aic3xxx_free_irq(struct aic3xxx *aic3xxx, int irq, void *data)
{
	if (!aic3xxx->irq_base)
		return -EINVAL;

	free_irq(aic3xxx->irq_base + irq, data);
	return 0;
}

int aic31xx_mic_check(struct snd_soc_codec *codec);

#endif				/* _TLV320AIC31XX_H */
