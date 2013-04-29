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
 * Rev 0.1   ASoC driver support    TI         26-07-2012
 *
 *	The AIC31xx ASoC driver is ported for the codec AIC31XX.
 */

#ifndef _TLV320AIC31XX_H
#define _TLV320AIC31XX_H
#include "aic3xxx/aic3xxx_cfw.h"
#include "aic3xxx/aic3xxx_cfw_ops.h"
#define AIC31XX_MCBSP_SLAVE /*aic3111 master*/
#if 0
#ifdef DEBUG
	#define dprintk(x...)   printk(x)
	#define DBG(x...)       printk(x)
#else
	#define dprintk(x...)
	#define DBG(x...)
#endif
#endif

#define AUDIO_NAME "aic31xx"
/* Macro to enable the inclusion of tiload kernel driver */
#define AIC31XX_TiLoad
/* Macro enables or disables support for miniDSP in the driver */
/* Enable the AIC31XX_TiLoad macro first before enabling these macros */
/* #define CONFIG_MINI_DSP */
#undef CONFIG_MINI_DSP

/* #define MULTIBYTE_CONFIG_SUPPORT */
#undef MULTIBYTE_CONFIG_SUPPORT

/* #define AIC31XX_SYNC_MODE */
#undef AIC31XX_SYNC_MODE

/* #define AIC31XX_ASI1_MASTER */
#undef AIC31XX_ASI1_MASTER
/* Macro for McBsp master / slave configuration */
#define AIC31XX_MCBSP_SLAVE	/*31XX master*/
/* #undef AIC31XX_MCBSP_SLAVE */

/* Enable this macro allow for different ASI formats */
/* #define ASI_MULTI_FMT */
#undef ASI_MULTI_FMT

// FIXME-HASH: This was enabled in 3.0 driver
/* Enable register caching on write */
#define EN_REG_CACHE

/* Enable Headset Detection */
/* #define HEADSET_DETECTION */
#undef HEADSET_DETECTION

/* Enable or disable controls to have Input routing*/
/* #define FULL_IN_CNTL */
#undef FULL_IN_CNTL
/* AIC31XX supported sample rate are 8k to 192k */
#define AIC31XX_RATES	SNDRV_PCM_RATE_8000_192000

/* AIC31XX supports the word formats 16bits, 20bits, 24bits and 32 bits */
#define AIC31XX_FORMATS	(SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S20_3LE \
			 | SNDRV_PCM_FMTBIT_S24_3LE | SNDRV_PCM_FMTBIT_S32_LE)

#define AIC31XX_FREQ_12000000 12000000
#define AIC31XX_FREQ_19200000 19200000
#define AIC31XX_FREQ_24000000 24000000
#define AIC31XX_FREQ_38400000 38400000
#define AIC31XX_FREQ_12288000 12288000
/* Audio data word length = 16-bits (default setting) */
#define AIC31XX_WORD_LEN_16BITS		0x00
#define AIC31XX_WORD_LEN_20BITS		0x01
#define AIC31XX_WORD_LEN_24BITS		0x02
#define AIC31XX_WORD_LEN_32BITS		0x03

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
/* shift value for DAC_OSR_MSB register */
#define DAC_OSR_MSB_SHIFT				4

/* number of codec specific register for configuration */
#define NO_FEATURE_REGS				2

/* AIC31XX register space */
#define	AIC31XX_CACHEREGNUM				1024
/* Updated from 256 to support Page 3 registers */

#define DSP_NON_SYNC_MODE(state)	(!((state & 0x03) && (state & 0x30)))

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
#define HP_DEFAULT_VOL			0x75
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
#define HP_POWER_UP_15_3_USEC	BIT3
#define HP_POWER_UP_153_USEC	BIT4
#define HP_POWER_UP_1_53_MSEC	(BIT4 | BIT3)
#define HP_POWER_UP_15_3_MSEC	BIT5
#define HP_POWER_UP_76_2_MSEC	(BIT5 | BIT3)
#define HP_POWER_UP_153_MSEC	(BIT5 | BIT4)
#define HP_POWER_UP_304_MSEC	(BIT5 | BIT4 | BIT3)
#define HP_POWER_UP_610_MSEC	(BIT6)
#define HP_POWER_UP_1_2_SEC	(BIT6 | BIT3)
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


/****************************************************************************/
/*  DAPM related Enum Defines */
/****************************************************************************/

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


#define AIC31XX_COPS_MDSP_A	0x10


#define AIC31XX_COPS_MDSP_D	0x03
#define AIC31XX_COPS_MDSP_D_L	0x02
#define AIC31XX_COPS_MDSP_D_R	0x01

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

int get_runstate(void *);

int aic31xx_dsp_pwrup(void *, int);

int aic31xx_pwr_down(void *, int , int , int , int);

int aic31xx_dsp_pwrdwn_status(void *);

int aic31xx_ops_reg_read(void *p, unsigned int reg);

int aic31xx_ops_reg_write(void *p, unsigned int reg, unsigned char mval);

int aic31xx_ops_set_bits(void *p, unsigned int reg, unsigned char mask,
				unsigned char val);

int aic31xx_ops_bulk_read(void *p, unsigned int reg, int count, u8 *buf);

int aic31xx_ops_bulk_write(void *p, unsigned int reg, int count, const u8 *buf);

int aic31xx_ops_lock(void *pv);

int aic31xx_ops_unlock(void *pv);

int aic31xx_ops_stop(void *pv, int mask);

int aic31xx_ops_restore(void *pv, int run_state);

int aic31xx_ops_adaptivebuffer_swap(void *pv, int mask);

int aic31xx_restart_dsps_sync(void *pv, int run_state);

/*
 *----------------------------------------------------------------------------
 * @struct  aic31xx_setup_data |
 *          i2c specific data setup for AIC31XX.
 * @field   unsigned short |i2c_address |
 *          Unsigned short for i2c address.
 *----------------------------------------------------------------------------
 */
/*struct aic31xx_setup_data {
	unsigned short i2c_address;
};*/
struct aic31xx_jack_data {
	struct snd_soc_jack *jack;
	int report;
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
	u16 reg_offset;
	u8 reg_val;
};

struct aic31xx_priv {
	u32 sysclk;
	s32 master;
	u8  mute;
	u8  headset_connected;
	u8  headset_current_status;
	u8  power_status;
	u8  playback_status;
	u8 capture_stream;
	u8 from_resume;
	struct aic31xx_jack_data hs_jack;
	struct aic31xx_configs hp_analog_right_vol[120];
	struct aic31xx_configs hp_analog_left_vol[120];
	struct workqueue_struct *workqueue;
	struct delayed_work delayed_work;
	struct input_dev *idev;
	struct snd_soc_codec *codec;
	struct mutex mutex;
	struct mutex cfw_mutex;
	struct cfw_state cfw_ps;
	struct cfw_state *cfw_p;
	struct aic3xxx_pdata *pdata;
	u8 i2c_regs_status;
	u8 playback_stream;	/* 1 denotes Playback */
	int mute_asi; /* Bit 0 -> ASI1, Bit 1-> ASI2, Bit 2 -> ASI3*/
	int asi_fmt[2];
	int dsp_runstate;
	struct firmware *cur_fw;
	int isdefault_fw;
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

/*
 *----------------------------------------------------------------------------
 * @struct  snd_soc_codec_dai |
 *          It is SoC Codec DAI structure which has DAI capabilities viz.,
 *          playback and capture, DAI runtime information viz. state of DAI
 *			and pop wait state, and DAI private data.
 *----------------------------------------------------------------------------
 */
extern struct snd_soc_dai tlv320aic31xx_dai;

/*
 *----------------------------------------------------------------------------
int aic31xx_write(struct snd_soc_codec *codec, unsigned int reg,
	unsigned int value)
 * @struct  snd_soc_codec_device |
 *          This structure is soc audio codec device sturecute which pointer
 *          to basic functions aic31xx_probe(), aic31xx_remove(),
 *			aic31xx_suspend() and aic31xx_resume()
 *
 */
extern struct snd_soc_codec_device soc_codec_dev_aic31xx;
extern struct aic3xxx_codec_ops aic31xx_cfw_codec_ops;
void aic31xx_hs_jack_detect(struct snd_soc_codec *codec,
				struct snd_soc_jack *jack, int report);
void aic31xx_firmware_load(const struct firmware *fw, void *context);
unsigned int aic3xxx_read(struct snd_soc_codec *codec, unsigned int reg);
int aic3xxx_write(struct snd_soc_codec *codec, unsigned int reg,
				unsigned int value);
static int aic31xx_mute_codec(struct snd_soc_codec *codec, int mute);
static int aic31xx_mute(struct snd_soc_dai *dai, int mute);

int aic31xx_mic_check(struct snd_soc_codec *codec);
#ifdef AIC31XX_TiLoad
	int aic31xx_driver_init(struct snd_soc_codec *codec);
#endif
#endif				/* _TLV320AIC31XX_H */

