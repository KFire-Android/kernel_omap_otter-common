
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

 * Rev 0.8   Ported to Linux 3.4 Kernel			06-05-2013
 */
#ifndef _TLV320AIC31XX_H
#define _TLV320AIC31XX_H
#include "aic31xx/aic3xxx_cfw.h"
#include "aic31xx/aic3xxx_cfw_ops.h"
#include "../../../drivers/staging/android/switch/switch.h"
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
/* Enable register caching on write */
/* #define EN_REG_CACHE */

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


/* The headset jack insertion GPIO used on the Qoo Board */
#define Qoo_HEADSET_DETECT_GPIO_PIN		49


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
	struct switch_dev sdev;
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
int aic31xx_mute_codec(struct snd_soc_codec *codec, int mute);
int aic31xx_mic_check(struct snd_soc_codec *codec);

/*moved tlv320aic3xxx-core.h to here*/

enum aic3xxx_type {
	TLV320AIC31XX = 0,
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


/****************************************************************************/

/*
 *****************************************************************************
 * Structures Definitions
 *****************************************************************************
 */
/*
 *----------------------------------------------------------------------------
 * @struct  aic3xxx_setup_data |
 *          i2c specific data setup for AIC31XX.
 * @field   unsigned short |i2c_address |
 *          Unsigned short for i2c address.
 *----------------------------------------------------------------------------
 */
struct aic3xxx_setup_data {
	unsigned short i2c_address;
};

/* GPIO API */
#define AIC31XX_NUM_GPIO 1 /*only one GPIO present*/
enum {
	AIC31XX_GPIO1_FUNC_DISABLED		= 0,
	AIC31XX_GPIO1_FUNC_INPUT		= 1,
	AIC31XX_GPIO1_FUNC_OUTPUT		= 3,
	AIC31XX_GPIO1_FUNC_CLOCK_OUTPUT		= 4,
	AIC31XX_GPIO1_FUNC_INT1_OUTPUT		= 5,
	AIC31XX_GPIO1_FUNC_INT2_OUTPUT		= 6,
	AIC31XX_GPIO1_FUNC_ADC_MOD_CLK_OUTPUT	= 10,

};

struct aic3xxx_configs {
	u8 book_no;
	u16 reg_offset;
	u8 reg_val;
};
#if 0
struct aic3xxx_gpio_setup {
       u8 used; /*GPIO, GPI and GPO is used in the board, used = 1 else 0*/
       u8 in; /*GPIO is used as input, in = 1 else
				in = 0. GPI in = 1, GPO in = 0*/
	unsigned int in_reg; /* if GPIO is input,
				register to write the mask to.*/
	u8 in_reg_bitmask; /* bitmask for 'value' to be written into in_reg*/
	u8 in_reg_shift; /* bits to shift to write 'value' into in_reg*/
	u8 value; /* value to be written gpio_control_reg if GPIO is output,
				in_reg if its input*/
	unsigned int reg;
};
#endif

struct aic3xxx_gpio_setup {
	unsigned int reg;
	u8 value;
};

struct aic3xxx_pdata {
	unsigned int audio_mclk1;
	unsigned int audio_mclk2;
	unsigned int gpio_irq; /* whether AIC31XX interrupts the host AP
				on a GPIO pin of AP */
	unsigned int gpio_reset; /* is the codec being reset by a gpio
				[host] pin, if yes provide the number. */
	int num_gpios;

	struct aic3xxx_gpio_setup *gpio_defaults;/* all gpio configuration */
	int naudint_irq; /* audio interrupt */
	unsigned int irq_base;
};

/*
 *----------------------------------------------------------------------------
 * @struct  aic3xxx_rate_divs |
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
 */
struct aic3xxx {
	struct mutex io_lock;
	struct mutex irq_lock;
	enum aic3xxx_type type;
	struct device *dev;
	struct regmap *regmap;
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

/* Device I/O API */
int aic3xxx_reg_read(struct aic3xxx *aic3xxx, unsigned int reg);
int aic3xxx_reg_write(struct aic3xxx *aic3xxx, unsigned int reg,
		unsigned char val);
int aic3xxx_set_bits(struct aic3xxx *aic3xxx, unsigned int reg,
		unsigned char mask, unsigned char val);
int aic3xxx_bulk_read(struct aic3xxx *aic3xxx, unsigned int reg,
		int count, u8 *buf);
int aic3xxx_bulk_write(struct aic3xxx *aic3xxx, unsigned int reg,
		       int count, const u8 *buf);
int aic3xxx_wait_bits(struct aic3xxx *aic3xxx, unsigned int reg,
		      unsigned char mask, unsigned char val, int delay,
		      int counter);

int aic3xxx_irq_init(struct aic3xxx *aic3xxx);
void aic3xxx_irq_exit(struct aic3xxx *aic3xxx);
int aic3xxx_device_init(struct aic3xxx *aic3xxx);
void aic3xxx_device_exit(struct aic3xxx *aic3xxx);
int aic3xxx_i2c_read_device(struct aic3xxx *aic3xxx, u8 offset,
				void *dest, int count);
int aic3xxx_i2c_write_device(struct aic3xxx *aic3xxx, u8 offset,
				const void *src, int count);

/*moved tlv320aic31xx-registers.h to here*/

#define MAKE_REG(book, page, offset) \
			(unsigned int)((book << 16)|(page << 8)|offset)

/* ****************** Book 0 Registers **************************************/
/* ****************** AIC31XX has one book only *****************************/

/* ****************** Page 0 Registers **************************************/
/* Page select register */
#define AIC31XX_PAGE_SEL_REG		MAKE_REG(0, 0, 0)
/* Software reset register */
#define AIC31XX_RESET_REG		MAKE_REG(0, 0, 1)
/* OT FLAG register */
#define AIC31XX_OT_FLAG_REG		MAKE_REG(0, 0, 3)
/* Revision and PG-ID */
#define AIC31XX_REV_PG_ID		MAKE_REG(0, 0, 3)
#define AIC31XX_REV_MASK		(0b01110000)
#define AIC31XX_REV_SHIFT		4
#define AIC31XX_PG_MASK			(0b00000001)
#define AIC31XX_PG_SHIFT		0

/* Clock clock Gen muxing, Multiplexers*/
#define AIC31XX_CLK_R1			MAKE_REG(0, 0, 4)
#define AIC31XX_PLL_CLKIN_MASK		(0b00001100)
#define AIC31XX_PLL_CLKIN_SHIFT		2
#define AIC31XX_PLL_CLKIN_MCLK		0
/* PLL P and R-VAL register*/
#define AIC31XX_CLK_R2			MAKE_REG(0, 0, 5)
/* PLL J-VAL register*/
#define AIC31XX_CLK_R3			MAKE_REG(0, 0, 6)
/* PLL D-VAL MSB register */
#define AIC31XX_CLK_R4			MAKE_REG(0, 0, 7)
/* PLL D-VAL LSB register */
#define AIC31XX_CLK_R5			MAKE_REG(0, 0, 8)
/* DAC NDAC_VAL register*/
#define AIC31XX_NDAC_CLK_R6		MAKE_REG(0, 0, 11)
/* DAC MDAC_VAL register */
#define AIC31XX_MDAC_CLK_R7		MAKE_REG(0, 0, 12)
/*DAC OSR setting register1, MSB value*/
#define AIC31XX_DAC_OSR_MSB		MAKE_REG(0, 0, 13)
/*DAC OSR setting register 2, LSB value*/
#define AIC31XX_DAC_OSR_LSB		MAKE_REG(0, 0, 14)
#define AIC31XX_MINI_DSP_INPOL		MAKE_REG(0, 0, 16)
/*Clock setting register 8, PLL*/
#define AIC31XX_NADC_CLK_R8		MAKE_REG(0, 0, 18)
/*Clock setting register 9, PLL*/
#define AIC31XX_MADC_CLK_R9		MAKE_REG(0, 0, 19)
/*ADC Oversampling (AOSR) Register*/
#define AIC31XX_ADC_OSR_REG		MAKE_REG(0, 0, 20)
/*ADC minidsp engine decimation*/
#define AIC31XX_ADC_DSP_DECI		MAKE_REG(0, 0, 22)
/*Clock setting register 9, Multiplexers*/
#define AIC31XX_CLK_MUX_R9		MAKE_REG(0, 0, 25)
/*Clock setting register 10, CLOCKOUT M divider value*/
#define AIC31XX_CLK_R10			MAKE_REG(0, 0, 26)
/*Audio Interface Setting Register 1*/
#define AIC31XX_INTERFACE_SET_REG_1	MAKE_REG(0, 0, 27)
/*Audio Data Slot Offset Programming*/
#define AIC31XX_DATA_SLOT_OFFSET	MAKE_REG(0, 0, 28)
/*Audio Interface Setting Register 2*/
#define AIC31XX_INTERFACE_SET_REG_2	MAKE_REG(0, 0, 29)
/*Clock setting register 11, BCLK N Divider*/
#define AIC31XX_CLK_R11			MAKE_REG(0, 0, 30)
/*Audio Interface Setting Register 3, Secondary Audio Interface*/
#define AIC31XX_INTERFACE_SET_REG_3	MAKE_REG(0, 0, 31)
/*Audio Interface Setting Register 4*/
#define AIC31XX_INTERFACE_SET_REG_4	MAKE_REG(0, 0, 32)
/*Audio Interface Setting Register 5*/
#define AIC31XX_INTERFACE_SET_REG_5	MAKE_REG(0, 0, 33)
/* I2C Bus Condition */
#define AIC31XX_I2C_FLAG_REG		MAKE_REG(0, 0, 34)
/* ADC FLAG */
#define AIC31XX_ADC_FLAG		MAKE_REG(0, 0, 36)
/* DAC Flag Registers */
#define AIC31XX_DAC_FLAG_1		MAKE_REG(0, 0, 37)
#define AIC31XX_DAC_FLAG_2		MAKE_REG(0, 0, 38)

/* Sticky Interrupt flag (overflow) */
#define AIC31XX_OVERFLOW_FLAG_REG	MAKE_REG(0, 0, 39)
#define AIC31XX_LEFT_DAC_OVERFLOW_INT			0x80
#define AIC31XX_RIGHT_DAC_OVERFLOW_INT			0x40
#define AIC31XX_MINIDSP_D_BARREL_SHIFT_OVERFLOW_INT	0x20
#define AIC31XX_ADC_OVERFLOW_INT			0x08
#define AIC31XX_MINIDSP_A_BARREL_SHIFT_OVERFLOW_INT	0x02

/* Sticky Interrupt flags 1 and 2 registers (DAC) */
#define AIC31XX_INTR_DAC_FLAG_1		MAKE_REG(0, 0, 44)
#define AIC31XX_LEFT_OUTPUT_DRIVER_OVERCURRENT_INT	0x80
#define AIC31XX_RIGHT_OUTPUT_DRIVER_OVERCURRENT_INT	0x40
#define AIC31XX_BUTTON_PRESS_INT			0x20
#define AIC31XX_HEADSET_PLUG_UNPLUG_INT			0x10
#define AIC31XX_LEFT_DRC_THRES_INT			0x08
#define AIC31XX_RIGHT_DRC_THRES_INT			0x04
#define AIC31XX_MINIDSP_D_STD_INT			0x02
#define AIC31XX_MINIDSP_D_AUX_INT			0x01

#define AIC31XX_INTR_DAC_FLAG_2		MAKE_REG(0, 0, 46)

/* Sticky Interrupt flags 1 and 2 registers (ADC) */
#define AIC31XX_INTR_ADC_FLAG_1		MAKE_REG(0, 0, 45)
#define AIC31XX_AGC_NOISE_INT		0x40
#define AIC31XX_MINIDSP_A_STD_INT	0x10
#define AIC31XX_MINIDSP_A_AUX_INT	0x08

#define AIC31XX_INTR_ADC_FLAG_2		MAKE_REG(0, 0, 47)

/* INT1 interrupt control */
#define AIC31XX_INT1_CTRL_REG		MAKE_REG(0, 0, 48)
#define AIC31XX_HEADSET_IN_MASK		0x80
#define AIC31XX_BUTTON_PRESS_MASK	0x40
#define AIC31XX_DAC_DRC_THRES_MASK	0x20
#define AIC31XX_AGC_NOISE_MASK		0x10
#define AIC31XX_OVER_CURRENT_MASK	0x08
#define AIC31XX_ENGINE_GEN__MASK	0x04

/* INT2 interrupt control */
#define AIC31XX_INT2_CTRL_REG		MAKE_REG(0, 0, 49)

/* GPIO1 control */
#define AIC31XX_GPIO1_IO_CTRL		MAKE_REG(0, 0, 51)
#define AIC31XX_GPIO_D5_D2		(0b00111100)
#define AIC31XX_GPIO_D2_SHIFT		2

/*DOUT Function Control*/
#define AIC31XX_DOUT_CTRL_REG		MAKE_REG(0, 0, 53)
/*DIN Function Control*/
#define AIC31XX_DIN_CTL_REG		MAKE_REG(0, 0, 54)
/*DAC Instruction Set Register*/
#define AIC31XX_DAC_PRB			MAKE_REG(0, 0, 60)
/*ADC Instruction Set Register*/
#define AIC31XX_ADC_PRB			MAKE_REG(0, 0, 61)
/*DAC channel setup register*/
#define AIC31XX_DAC_CHN_REG		MAKE_REG(0, 0, 63)
/*DAC Mute and volume control register*/
#define AIC31XX_DAC_MUTE_CTRL_REG	MAKE_REG(0, 0, 64)
/*Left DAC channel digital volume control*/
#define AIC31XX_LDAC_VOL_REG		MAKE_REG(0, 0, 65)
/*Right DAC channel digital volume control*/
#define AIC31XX_RDAC_VOL_REG		MAKE_REG(0, 0, 66)
/* Headset detection */
#define AIC31XX_HS_DETECT_REG		MAKE_REG(0, 0, 67)
#define AIC31XX_HS_MASK			(0b01100000)
#define AIC31XX_HP_MASK			(0b00100000)
/* DRC Control Registers */
#define AIC31XX_DRC_CTRL_REG_1		MAKE_REG(0, 0, 68)
#define AIC31XX_DRC_CTRL_REG_2		MAKE_REG(0, 0, 69)
#define AIC31XX_DRC_CTRL_REG_3		MAKE_REG(0, 0, 70)
/* Beep Generator */
#define AIC31XX_BEEP_GEN_L		MAKE_REG(0, 0, 71)
#define AIC31XX_BEEP_GEN_R		MAKE_REG(0, 0, 72)
/* Beep Length */
#define AIC31XX_BEEP_LEN_MSB_REG	MAKE_REG(0, 0, 73)
#define AIC31XX_BEEP_LEN_MID_REG	MAKE_REG(0, 0, 74)
#define AIC31XX_BEEP_LEN_LSB_REG	MAKE_REG(0, 0, 75)
/* Beep Functions */
#define AIC31XX_BEEP_SINX_MSB_REG	MAKE_REG(0, 0, 76)
#define AIC31XX_BEEP_SINX_LSB_REG	MAKE_REG(0, 0, 77)
#define AIC31XX_BEEP_COSX_MSB_REG	MAKE_REG(0, 0, 78)
#define AIC31XX_BEEP_COSX_LSB_REG	MAKE_REG(0, 0, 79)
#define AIC31XX_ADC_CHN_REG		MAKE_REG(0, 0, 81)
#define AIC31XX_ADC_VOL_FGC		MAKE_REG(0, 0, 82)
#define AIC31XX_ADC_VOL_CGC		MAKE_REG(0, 0, 83)

/*Channel AGC Control Register 1*/
#define AIC31XX_CHN_AGC_R1		MAKE_REG(0, 0, 86)
/*Channel AGC Control Register 2*/
#define AIC31XX_CHN_AGC_R2		MAKE_REG(0, 0, 87)
/*Channel AGC Control Register 2*/
#define AIC31XX_CHN_AGC_R3		MAKE_REG(0, 0, 88)
/*Channel AGC Control Register 2*/
#define AIC31XX_CHN_AGC_R4		MAKE_REG(0, 0, 89)
/*Channel AGC Control Register 2*/
#define AIC31XX_CHN_AGC_R5		MAKE_REG(0, 0, 90)
/*Channel AGC Control Register 2*/
#define AIC31XX_CHN_AGC_R6		MAKE_REG(0, 0, 91)
/*Channel AGC Control Register 2*/
#define AIC31XX_CHN_AGC_R7		MAKE_REG(0, 0, 92)
/* VOL/MICDET-Pin SAR ADC Volume Control */
#define AIC31XX_VOL_MICDECT_ADC		MAKE_REG(0, 0, 116)
/* VOL/MICDET-Pin Gain*/
#define AIC31XX_VOL_MICDECT_GAIN	MAKE_REG(0, 0, 117)
/*Power Masks*/
#define AIC31XX_LDAC_POWER_STATUS_MASK		0x80
#define AIC31XX_RDAC_POWER_STATUS_MASK		0x08
#define AIC31XX_ADC_POWER_STATUS_MASK		0x40
/*Time Delay*/
#define AIC31XX_TIME_DELAY		5000
#define AIC31XX_DELAY_COUNTER		100



/******************** Page 1 Registers **************************************/
/* Headphone drivers */
#define AIC31XX_HPHONE_DRIVERS		MAKE_REG(0, 1, 31)
/* Class-D Speakear Amplifier */
#define AIC31XX_CLASS_D_SPK		MAKE_REG(0, 1, 32)
/* HP Output Drivers POP Removal Settings */
#define AIC31XX_HP_OUT_DRIVERS		MAKE_REG(0, 1, 33)
/* Output Driver PGA Ramp-Down Period Control */
#define AIC31XX_PGA_RAMP_REG		MAKE_REG(0, 1, 34)
/* DAC_L and DAC_R Output Mixer Routing */
#define AIC31XX_DAC_MIXER_ROUTING	MAKE_REG(0, 1, 35)
/*Left Analog Vol to HPL */
#define AIC31XX_LEFT_ANALOG_HPL		MAKE_REG(0, 1, 36)
/* Right Analog Vol to HPR */
#define AIC31XX_RIGHT_ANALOG_HPR	MAKE_REG(0, 1, 37)
/* Left Analog Vol to SPL */
#define AIC31XX_LEFT_ANALOG_SPL		MAKE_REG(0, 1, 38)
/* Right Analog Vol to SPR */
#define AIC31XX_RIGHT_ANALOG_SPR	MAKE_REG(0, 1, 39)
/* HPL Driver */
#define AIC31XX_HPL_DRIVER_REG		MAKE_REG(0, 1, 40)
/* HPR Driver */
#define AIC31XX_HPR_DRIVER_REG		MAKE_REG(0, 1, 41)
/* SPL Driver */
#define AIC31XX_SPL_DRIVER_REG		MAKE_REG(0, 1, 42)
/* SPR Driver */
#define AIC31XX_SPR_DRIVER_REG		MAKE_REG(0, 1, 43)
/* HP Driver Control */
#define AIC31XX_HP_DRIVER_CONTROL	MAKE_REG(0, 1, 44)
/*MICBIAS Configuration Register*/
#define AIC31XX_MICBIAS_CTRL_REG	MAKE_REG(0, 1, 46)
/* MIC PGA*/
#define AIC31XX_MICPGA_VOL_CTRL		MAKE_REG(0, 1, 47)
/* Delta-Sigma Mono ADC Channel Fine-Gain Input Selection for P-Terminal */
#define AIC31XX_MICPGA_PIN_CFG		MAKE_REG(0, 1, 48)
/* ADC Input Selection for M-Terminal */
#define AIC31XX_MICPGA_MIN_CFG		MAKE_REG(0, 1, 49)
/* Input CM Settings */
#define AIC31XX_MICPGA_CM_REG		MAKE_REG(0, 1, 50)

/* ****************** Page 3 Registers **************************************/

/* Timer Clock MCLK Divider */
#define AIC31XX_TIMER_MCLK_DIV		MAKE_REG(0, 3, 16)

/* ****************** Page 8 Registers **************************************/
#define AIC31XX_DAC_ADAPTIVE_BANK1_REG	MAKE_REG(0, 8, 1)


#define AIC31XX_ADC_DATAPATH_SETUP      MAKE_REG(0, 0, 81)
#define AIC31XX_DAC_DATAPATH_SETUP      MAKE_REG(0, 0, 63)

/*moved tlv320aic3xxx-registers.h to here*/

#define AIC3XXX_MAKE_REG(book, page, offset)	(unsigned int)((book << 16) | \
						(page << 8)| \
						offset)

#define AIC3XXX_RESET		AIC3XXX_MAKE_REG(0, 0, 1)
#define AIC3XXX_REV_PG_ID	AIC3XXX_MAKE_REG(0, 0, 2)
#define AIC3XXX_REV_M		(0b01110000)
#define AIC3XXX_REV_S		4
#define AIC3XXX_PG_M			(0b00000111)
#define AIC3XXX_PG_S	 	0

#define AIC3XXX_INT_STICKY_FLAG1		AIC3XXX_MAKE_REG(0, 0, 42)
#define AIC3XXX_LEFT_DAC_OVERFLOW_INT	0x80
#define AIC3XXX_RIGHT_DAC_OVERFLOW_INT	0x40
#define AIC3XXX_MINIDSP_D_BARREL_SHIFT_OVERFLOW_INT	0x20
#define AIC3XXX_ADC_OVERFLOW_INT	0x08
#define AIC3XXX_LEFT_ADC_OVERFLOW_INT	0x08
#define AIC3XXX_RIGHT_ADC_OVERFLOW_INT	0x04
#define AIC3XXX_MINIDSP_A_BARREL_SHIFT_OVERFLOW_INT	0x02
#define AIC3XXX_INT_STICKY_FLAG2		AIC3XXX_MAKE_REG(0, 0, 44)
#define AIC3XXX_LEFT_OUTPUT_DRIVER_OVERCURRENT_INT	0x80
#define AIC3XXX_RIGHT_OUTPUT_DRIVER_OVERCURRENT_INT	0x40
#define AIC3XXX_BUTTON_PRESS_INT			0x20
#define AIC3XXX_HEADSET_PLUG_UNPLUG_INT			0x10
#define AIC3XXX_LEFT_DRC_THRES_INT			0x08
#define AIC3XXX_RIGHT_DRC_THRES_INT			0x04
#define AIC3XXX_MINIDSP_D_STD_INT			0x02
#define AIC3XXX_MINIDSP_D_AUX_INT			0x01
#define AIC3XXX_INT_STICKY_FLAG3		AIC3XXX_MAKE_REG(0, 0, 45)
#define AIC3XXX_SPK_OVER_CURRENT_INT			0x80
#define AIC3XXX_LEFT_AGC_NOISE_INT			0x40
#define AIC3XXX_RIGHT_AGC_NOISE_INT			0x20
#define AIC3XXX_MINIDSP_A_STD_INT			0x10
#define AIC3XXX_MINIDSP_A_AUX_INT			0x08
#define AIC3XXX_LEFT_ADC_DC_DATA_AVAILABLE_INT		0x04
#define AIC3XXX_RIGHT_ADC_DC_DATA_AVAILABLE_INT		0x02
#define AIC3XXX_CP_SHORT_CIRCUIT_INT			0x01
#define AIC3XXX_INT1_CNTL		AIC3XXX_MAKE_REG(0, 0, 48)
#define AIC3XXX_HEADSET_IN_M		0x80
#define AIC3XXX_BUTTON_PRESS_M	0x40
#define AIC3XXX_DAC_DRC_THRES_M	0x20
#define AIC3XXX_AGC_NOISE_M		0x10
#define AIC3XXX_AGC_NOISE_INT		0x40
#define AIC3XXX_OVER_CURRENT_M	0x08
#define AIC3XXX_OVERFLOW_M		0x04
#define AIC3XXX_SPK_OVERCURRENT_M	0x02
#define AIC3XXX_CP_SHORT_CIRCUIT_M	0x02
#define AIC3XXX_INT2_CNTL		AIC3XXX_MAKE_REG(0, 0, 49)
#define AIC3XXX_INT_FMT			AIC3XXX_MAKE_REG(0, 0, 51)
#define AIC3XXX_DEVICE_ID		AIC3XXX_MAKE_REG(0, 0, 125)

#endif				/* _TLV320AIC31XX_H */
