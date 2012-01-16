/* revert
 * linux/sound/soc/codecs/tlv320aic31xx.h
 *
 * Copyright (C) 2011 Texas Instruments, Inc.
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
 * Rev 0.1   ASoC driver support    Mistral         14-04-2010
 *
 * Rev 0.2   Updated based Review Comments Mistral      29-06-2010
 *
 * Rev 0.3   Updated for Codec Family Compatibility     12-07-2010
 *
 * Rev 0.4   Ported to 2.6.35 kernel
 *
 * Rev 0.5   Updated the code-base with ADC related register #defines.
 *
 * Rev 0.6   Added the PAGE_3 #define and the Prog Delay Timer Register
 *	     define to be used for HP and DAC Rampup time.
 */

#ifndef _TLV320AIC31xx_H
#define _TLV320AIC31XX_H

#define AIC31XX_VERSION "0.1"

//#define AIC31x_CODEC_DEBUG 1

#ifdef AIC31x_CODEC_DEBUG
#define DBG(x...) printk(KERN_INFO x)
#else
#define DBG(x...)
#endif


/*
 * Linux Developer needs to enable a particular flag in
 * this header file before performing a build of the
 * Audio Codec Driver.
 */
#define AIC3110_CODEC_SUPPORT

#ifdef AIC3110_CODEC_SUPPORT

/* 
 ****************************************************************************
 * BOARD SPECIFIC DEFINES
 ****************************************************************************
 */

/* The headset jack insertion GPIO used on the KC1 Board */
#define KC1_HEADSET_DETECT_GPIO_PIN 		49

/* Regulator Minimum Voltage */
#define REGU_MIN_VOL			3000000

/* Regulator Maximum Voltage */
#define REGU_MAX_VOL			3000000

/* 5V Boost Voltage GPIO Input on KC1 Board */
#define ADO_SPK_ENABLE_GPIO		119

/* Audio Gain Tuning as per the KC1 Board */
#define KC1_AUDIO_GAIN_TUNING_SELECT 1


#define AUDIO_NAME "aic3110"
#define AIC3110_VERSION "0.1"

#endif

#ifdef AIC3100_CODEC_SUPPORT

#define AUDIO_NAME "aic3100"
#define AIC3100_VERSION "0.1"

#endif

/* Enable register caching on write */
#define EN_REG_CACHE

/* Headphone Driver ON */
#define HP_DRIVER_ON                    0xCC

/*Headphone Driver OFF */
#define HP_DRIVER_OFF			0x0C

/*Speaker Driver ON */
#define SPK_DRV_ON			0xC6

/*Speaker Driver OFF */
#define SPK_DRV_OFF			0x06


#define HEADPHONE_ANALOG_VOL_MIN	0x20

/* Enable headset detection */
#define HEADSET_DETECTION

/* AIC31xx supported sample rate are 8k to 192k */
//#define AIC31XX_RATES	SNDRV_PCM_RATE_8000_192000

#define AIC31XX_RATES	(SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_11025 | SNDRV_PCM_RATE_16000 \
                    | SNDRV_PCM_RATE_22050 | SNDRV_PCM_RATE_32000 | SNDRV_PCM_RATE_44100 \
  		    | SNDRV_PCM_RATE_48000 | SNDRV_PCM_RATE_96000 | SNDRV_PCM_RATE_192000)

/* AIC31xx supports the word formats 16bits, 20bits, 24bits and 32 bits */
#define AIC31XX_FORMATS	(SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S20_3LE \
			 | SNDRV_PCM_FMTBIT_S24_3LE | SNDRV_PCM_FMTBIT_S32_LE)

#define AIC31XX_FREQ_12000000 12000000
#define AIC31XX_FREQ_24000000 24000000
#define AIC31XX_FREQ_19200000 19200000

/* AIC31xx register space */
#define AIC31xx_CACHEREGNUM	384

/* Audio data word length = 16-bits (default setting) */
#define AIC31XX_WORD_LEN_16BITS		0x00
#define AIC31XX_WORD_LEN_20BITS		0x01
#define AIC31XX_WORD_LEN_24BITS		0x02
#define AIC31XX_WORD_LEN_32BITS		0x03
#define	DATA_LEN_SHIFT			    4

/* sink: name of target widget */
#define AIC31XX_WIDGET_NAME		0
/* control: mixer control name */
#define AIC31XX_CONTROL_NAME		1
/* source: name of source name */
#define AIC31XX_SOURCE_NAME		2

/* D15..D8 aic31xx register offset */
#define AIC31XX_REG_OFFSET_INDEX        0
/* D7...D0 register data */
#define AIC31XX_REG_DATA_INDEX          1

/* Serial data bus uses I2S mode (Default mode) */
#define AIC31XX_I2S_MODE		0x00
#define AIC31XX_DSP_MODE		0x01
#define AIC31XX_RIGHT_JUSTIFIED_MODE	0x02
#define AIC31XX_LEFT_JUSTIFIED_MODE	0x03
#define AUDIO_MODE_SHIFT		   6

/* number of codec specific register for configuration */
#define NO_FEATURE_REGS			   2

/* 8 bit mask value */
#define AIC31XX_8BITS_MASK		0xFF

/* ****************** Page 0 Registers **************************************/
#define	PAGE_SELECT			    0
#define	RESET				    1
#define OT_FLAG				    3
#define	CLK_REG_1			    4
#define	CLK_REG_2			    5
#define	CLK_REG_3			    6
#define	CLK_REG_4			    7
#define	CLK_REG_5			    8
#define	NDAC_CLK_REG			   11
#define	MDAC_CLK_REG			   12
#define DAC_OSR_MSB			   13
#define DAC_OSR_LSB			   14
#define	NADC_CLK_REG			   18
#define	MADC_CLK_REG			   19
#define ADC_OSR_REG			   20
#define CLK_MUX_REG			   25
#define CLK_MVAL_REG			   26
#define INTERFACE_SET_REG_1		   27
#define DATA_SLOT_OFFSET		   28
#define INTERFACE_SET_REG_2		   29
#define BCLK_N_VAL			   30
#define INTERFACE_SET_REG_3		   31
#define INTERFACE_SET_REG_4		   32
#define INTERFACE_SET_REG_5		   33
#define I2C_BUS_COND			   34
#define ADC_FLAG			   36
#define DAC_FLAG_1			   37
#define DAC_FLAG_2			   38
#define OVERFLOW_REG		           39
#define SHORT_CKT_FLAG                     44
#define ADC_INTR_STATUS			   45
#define DAC_INTR_STATUS                    46
#define ADC_INTR_FLAG			   47
#define INTL_CRTL_REG_1			   48
#define INTL_CRTL_REG_2			   49
#define GPIO_CRTL_REG_1			   51
#define DOUT_CTRL			   53 	
#define DIN_CTRL			   54
#define DAC_PRB_SEL_REG			   60
#define ADC_PRB_SEL_REG			   61
#define DAC_CHN_REG			   63
#define DAC_MUTE_CTRL_REG		   64
#define LDAC_VOL			   65
#define RDAC_VOL			   66

#define HEADSET_DETECT			67

#define DRC_CTL_REG_1			68
#define DRC_CTL_REG_2			69
#define DRC_CTL_REG_3			70

#define LEFT_BEEP_GEN			71
#define RIGHT_BEEP_GEN			72
#define BEEP_LENGTH_MSB			73
#define BEEP_LENGTH_MID			74
#define BEEP_LENGTH_LSB			75
#define BEEP_SINX_MSB			76
#define BEEP_SINX_LSB			77
#define BEEP_COSX_MSB			78
#define BEEP_COSX_LSB			79


#define ADC_DIG_MIC			    81
#define	ADC_FGA				    82
#define	ADC_CGA				    83

#define AGC_CTRL_1			    86
#define AGC_CTRL_2			    87
#define AGC_CTRL_3			    88
#define AGC_CTRL_4			    89
#define AGC_CTRL_5			    90
#define AGC_CTRL_6			    91
#define AGC_CTRL_7			    92
#define AGC_CTRL_8			    93

#define ADC_DC_1			    102
#define ADC_DC_2			    103
#define PIN_VOL_CTRL			    116
#define PIN_VOL_GAIN			    117


/******************** Page 1 Registers **************************************/
#define PAGE_1				    128
#define HP_SPK_ERR_CTL                  (PAGE_1 + 30)
#define HEADPHONE_DRIVER		(PAGE_1 + 31)
#define CLASSD_SPEAKER_AMP		(PAGE_1 + 32)
#define HP_POP_CTRL			(PAGE_1 + 33)
#define PGA_RAMP_CTRL			(PAGE_1 + 34)
#define DAC_MIX_CTRL			(PAGE_1 + 35)
#define L_ANLOG_VOL_2_HPL		(PAGE_1 + 36)
#define R_ANLOG_VOL_2_HPR		(PAGE_1 + 37)
#define L_ANLOG_VOL_2_SPL		(PAGE_1 + 38)
#define R_ANLOG_VOL_2_SPR		(PAGE_1 + 39)
#define HPL_DRIVER			(PAGE_1 + 40)
#define HPR_DRIVER			(PAGE_1 + 41)
#define SPL_DRIVER			(PAGE_1 + 42)
#define SPR_DRIVER			(PAGE_1 + 43)
#define HP_DRIVER_CTRL			(PAGE_1 + 44)
#define MICBIAS_CTRL			(PAGE_1 + 46)
#define MIC_PGA				(PAGE_1 + 47)
#define MIC_GAIN			(PAGE_1 + 48)
#define ADC_IP_SEL			(PAGE_1 + 49)
#define CM_SET	         		(PAGE_1 + 50)

#define L_MICPGA_P			(PAGE_1 + 52)
#define L_MICPGA_N			(PAGE_1 + 54)
#define R_MICPGA_P			(PAGE_1 + 55)
#define R_MICPGA_N			(PAGE_1 + 57)

#define PAGE_3				(128 * 3)
#define TIMER_CLOCK_MCLK_DIVIDER	(PAGE_3 + 16)

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
#define	DAC_MOD_CLK_2_BDIV_CLKIN 	BIT0
#define ADC_MOD_CLK_2_BDIV_CLKIN	(BIT1 | BIT0)
#define SOFT_RESET			0x01
#define PAGE0				0x00
#define PAGE1				0x01
#define BIT_CLK_MASTER			BIT3
#define WORD_CLK_MASTER			BIT2
#define	HIGH_PLL 			    BIT6
#define ENABLE_PLL			    BIT7
#define ENABLE_NDAC			    BIT7
#define ENABLE_MDAC			    BIT7
#define ENABLE_NADC			    BIT7
#define ENABLE_MADC			BIT7
#define ENABLE_BCLK			BIT7
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
#define LMUTE_ENUM		    0
#define RMUTE_ENUM		    1
#define DACEXTRA_ENUM		    2
#define DACCONTROL_ENUM		    3
#define SOFTSTEP_ENUM	            4
#define BEEP_ENUM		    5
#define MICBIAS_ENUM		    6
#define DACLEFTIP_ENUM		    7
#define DACRIGHTIP_ENUM		    8
#define VOLTAGE_ENUM		    9
#define HSET_ENUM		    10
#define DRC_ENUM		    11
#define MIC1LP_ENUM		    12
#define MIC1RP_ENUM		    13
#define MIC1LM_ENUM		    14
#define MIC_ENUM		    15
#define CM_ENUM			    16
#define MIC1LMM_ENUM		    17
#define MIC1_ENUM		    18
#define MIC2_ENUM		    19
#define MIC3_ENUM		    20
#define ADCMUTE_ENUM		    21

/*****************************************************************************
 * Structures Definitions
 *****************************************************************************
 */
/*
 *----------------------------------------------------------------------------
 * @struct  aic31xx_setup_data |
 *          i2c specific data setup for AIC31xx.
 * @field   unsigned short |i2c_address |
 *          Unsigned short for i2c address.
 *----------------------------------------------------------------------------
 */
struct aic31xx_setup_data
{
	unsigned short i2c_address;
};

/*
 *----------------------------------------------------------------------------
 * @struct  aic31xx_configs |
 *          AIC31xx initialization data which has register offset and register

 *          value.
 * @field   u16 | reg_offset |
 *          AIC31xx Register offsets required for initialization..
 * @field   u8 | reg_val |
 *          value to set the AIC31xx register to initialize the AIC31xx.

 *----------------------------------------------------------------------------
 */
struct aic31xx_configs
{
	u16 reg_offset;
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
struct aic31xx_priv
{
	u32 sysclk;
	s32 master;
	u8 page_no;
	void *control_data;
	u8  mute;
	u8  headset_connected;
        u8  headset_current_status; 
	u8  power_status;
	u8  playback_status;
	struct mutex mutex_codec;
        struct snd_soc_codec codec;
        u8  i2c_regs_status;
        u32   hp_driver_pop_time;
        u32   hp_driver_ramp_time; 
	u8 playback_stream;	/* 1 denotes Playback and 0 denotes record */
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
struct aic31xx_rate_divs
{
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
	struct aic31xx_configs codec_specific_regs[NO_FEATURE_REGS];
};

/*
 *----------------------------------------------------------------------------
 * @struct  snd_soc_codec_dai |
 *          It is SoC Codec DAI structure which has DAI capabilities viz.,
 *          playback and capture, DAI runtime information viz. state of DAI
 *			and pop wait state, and DAI private data.
 *----------------------------------------------------------------------------
 */
extern struct snd_soc_dai_driver tlv320aic31xx_dai[];

/*
 *----------------------------------------------------------------------------
 * @struct  snd_soc_codec_device |
 *          This structure is soc audio codec device sturecute which pointer
 *          to basic functions aic31xx_probe(), aic31xx_remove(),
 *			aic31xx_suspend() and aic31xx_resume()
 *
 */
extern struct snd_soc_codec_driver soc_codec_dev_aic31xx; 

/* Extern Function Declaration for Headset Speaker Path Detection */
extern int aic31xx_headset_speaker_path (struct snd_soc_codec *codec, int gpio_status);
extern u8 aic31xx_abe_fixup;
extern int aic31xx_mic_check(struct snd_soc_codec *codec);
extern int aic31xx_startup (struct snd_pcm_substream *, struct snd_soc_codec *);

#endif /* _TLV320AIC31XX_H */
