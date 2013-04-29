
/*
 * linux/sound/soc/codecs/tlv320aic31xx.c
 *
 *
 * Copyright (C) 2010 Texas Instruments, Inc.
 *
 * Based on sound/soc/codecs/tlv320aic326x.c
 *
 * This package is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * THIS PACKAGE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/spi/spi.h>
#include <linux/slab.h>
#include <linux/clk.h>
#include <linux/gpio.h>
#include <linux/cdev.h>
#include <linux/io.h>
#include <linux/workqueue.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/input.h>

#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/initval.h>
#include <sound/tlv.h>
#include <asm/div64.h>

#include <linux/firmware.h>
#include "tlv320aic31xx.h"
#include <mach/gpio.h>

#include "aic3xxx/aic3xxx_cfw.h"
#include "aic3xxx/aic3xxx_cfw_ops.h"
#include <linux/mfd/tlv320aic31xx-registers.h>
#include <linux/mfd/tlv320aic3xxx-registers.h>
#include <linux/mfd/tlv320aic3xxx-core.h>

#include "tlv320aic31xx_default_fw.h"
#include <linux/clk.h>

#include <sound/jack.h>
#include <linux/irq.h>

/*******************************************************************************
				Macros
*******************************************************************************/


#define DSP_STATUS(rs, adc_dac, rpos, rspos) \
		(rs |= (((adc_dac>>rpos) & 1) << rspos))


#define AIC_FORCE_SWITCHES_ON

/*user defined Macros for kcontrol */

#define SOC_SINGLE_AIC31XX(xname) {			\
	.iface = SNDRV_CTL_ELEM_IFACE_MIXER,		\
	.name = xname,					\
	.info = __new_control_info,			\
	.get = __new_control_get,			\
	.put = __new_control_put,			\
	.access = SNDRV_CTL_ELEM_ACCESS_READWRITE,	\
}

#define    AUDIO_CODEC_HPH_DETECT_GPIO		(157)
#define    AUDIO_CODEC_PWR_ON_GPIO		(103)
#define    AUDIO_CODEC_RESET_GPIO		(37)
#define    AUDIO_CODEC_PWR_ON_GPIO_NAME		"audio_codec_pwron"
#define    AUDIO_CODEC_RESET_GPIO_NAME		"audio_codec_reset"

/******************************************************************************
				Function Prototypes
*******************************************************************************/


static int aic31xx_hw_params(struct snd_pcm_substream *substream,
			struct snd_pcm_hw_params *params,
			struct snd_soc_dai *dai);

static int aic31xx_mute(struct snd_soc_dai *dai, int mute);

static int aic31xx_set_dai_fmt(struct snd_soc_dai *codec_dai,
			unsigned int fmt);

static int aic31xx_set_mode_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol);

static int aic31xx_set_mode_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol);

static int __new_control_info(struct snd_kcontrol *,
		struct snd_ctl_elem_info *);

static int __new_control_get(struct snd_kcontrol *,
		struct snd_ctl_elem_value *);

static int __new_control_put(struct snd_kcontrol *,
		struct snd_ctl_elem_value *);

static int aic31xx_set_bias_level(struct snd_soc_codec *codec,
		enum snd_soc_bias_level level);

static u8 aic31xx_reg_ctl;


/*
 * Global Variables introduced to reduce Headphone Analog Volume Control
 * Registers at run-time
 */
struct i2c_msg i2c_right_transaction[120];
struct i2c_msg i2c_left_transaction[120];


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

static int aic31xx_set_mode_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct aic31xx_priv *priv_ds = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.integer.value[0] = ((priv_ds->cfw_p->cur_mode<<8) |
					priv_ds->cfw_p->cur_cfg);

	return 0;
}

static int aic31xx_set_mode_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct aic31xx_priv *priv_ds = snd_soc_codec_get_drvdata(codec);
	int next_mode = 0, next_cfg = 0;
	int ret = 0;

	next_mode = (ucontrol->value.integer.value[0]>>8);
	next_cfg = (ucontrol->value.integer.value[0])&0xFF;
	if (priv_ds == NULL)
		dev_err(codec->dev, "\nFirmware not loaded,"
					"no mode switch can occur\n");
	else
		ret = aic3xxx_cfw_setmode_cfg(priv_ds->cfw_p,
			next_mode, next_cfg);

	return ret;
}

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

#ifdef AIC3110_CODEC_SUPPORT
	/* SP driver mute control */
	SOC_DOUBLE_R("SP driver mute", AIC31XX_SPL_DRIVER_REG,
			AIC31XX_SPR_DRIVER_REG, 2, 2, 0),
#endif

#ifdef AIC3100_CODEC_SUPPORT
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
	SOC_ENUM("HP Output common - mode voltage control",
			aic31xx_enum[VOLTAGE_ENUM]),

	/* Headset detection Enable/Disable control */
	SOC_ENUM("Headset detection Enable / Disable", aic31xx_enum[HSET_ENUM]),

	/* DRC Enable/Disable control */
	SOC_ENUM("DRC Enable / Disable", aic31xx_enum[DRC_ENUM]),
	/* DRC Threshold value control */
	SOC_SINGLE("DRC Threshold value(0 = -3 db, 7 = -24 db)",
			AIC31XX_DRC_CTRL_REG_1, 2, 0x07, 0),
	/* DRC Hysteresis value control */
	SOC_SINGLE("DRC Hysteresis value(0 = 0 db, 3 = 3 db)",
			AIC31XX_DRC_CTRL_REG_1, 0, 0x03, 0),
	/* DRC Hold time control */
	SOC_SINGLE("DRC hold time", AIC31XX_DRC_CTRL_REG_2, 3, 0x0F, 0),
	/* DRC attack rate control */ SOC_SINGLE("DRC attack rate",
			AIC31XX_DRC_CTRL_REG_3, 4, 0x0F, 0),
	/* DRC decay rate control */
	SOC_SINGLE("DRC decay rate", AIC31XX_DRC_CTRL_REG_3, 0, 0x0F, 0),
	/* MIC1LP selection for ADC I/P P-terminal */
	SOC_ENUM("MIC1LP selection for ADC I/P P - terminal",
			aic31xx_enum[MIC1LP_ENUM]),
	/* MIC1RP selection for ADC I/P P-terminal */
	SOC_ENUM("MIC1RP selection for ADC I/P P - terminal",
			aic31xx_enum[MIC1RP_ENUM]),
	/* MIC1LM selection for ADC I/P P-terminal */
	SOC_ENUM("MIC1LM selection for ADC I/P P - terminal",
			aic31xx_enum[MIC1LM_ENUM]),
	/* CM selection for ADC I/P M-terminal */
	SOC_ENUM("CM selection for ADC IP M - terminal",
			aic31xx_enum[CM_ENUM]),
	/* MIC1LM selection for ADC I/P M-terminal */
	SOC_ENUM("MIC1LM selection for ADC I/P M - terminal",
			aic31xx_enum[MIC1LMM_ENUM]),
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
	SOC_SINGLE("HP_DETECT_DEBOUNCE_TIME", AIC31XX_HS_DETECT_REG,
			2, 0x05, 0),
	/* HP Button Press Debounce Time */
	SOC_SINGLE("HP_BUTTON_DEBOUNCE_TIME", AIC31XX_HS_DETECT_REG,
			0, 0x03, 0),
	/* Added for Debugging */
	SOC_SINGLE("LoopBack_Control", AIC31XX_INTERFACE_SET_REG_2, 4, 4, 0),


#ifdef AIC3110_CODEC_SUPPORT
	/* For AIC3110 output is stereo so we are using	SOC_DOUBLE_R macro */

	/* SP Class-D driver output stage gain Control */
	SOC_DOUBLE_R_TLV("Class - D driver Volume", AIC31XX_SPL_DRIVER_REG,
			AIC31XX_SPR_DRIVER_REG, 3, 0x04, 0, class_D_drv_tlv),

#endif

#ifdef AIC3100_CODEC_SUPPORT
	/* SP Class-D driver output stage gain Control */
	SOC_SINGLE("Class - D driver Volume(0 = 6 dB, 4 = 24 dB)",
			AIC31XX_SPL_DRIVER_REG, 3, 0x04, 0),
#endif

	/* HP Analog Gain Volume Control */
	SOC_DOUBLE_R_TLV("HP Analog Gain", AIC31XX_LEFT_ANALOG_HPL, \
			AIC31XX_RIGHT_ANALOG_HPR, 0, 0x7F, 1, hp_vol_tlv),

#ifdef AIC3110_CODEC_SUPPORT
	/* SP Analog Gain Volume Control */
	SOC_DOUBLE_R_TLV("SP Analog Gain", AIC31XX_LEFT_ANALOG_SPL, \
			AIC31XX_RIGHT_ANALOG_SPR, 0, 0x7F, 1, sp_vol_tlv),
#endif

#ifdef AIC3100_CODEC_SUPPORT
	/* SP Analog Gain Volume Control */
	SOC_SINGLE("SP Analog Gain(0 = 0 dB, 127 = -78.3 dB)",
			AIC31XX_LEFT_ANALOG_SPL, 0, 0x7F, 1),
#endif
	/* Program Registers */
	SOC_SINGLE_AIC31XX("Program Registers"),

	SOC_SINGLE_EXT("FIRMWARE SET MODE", SND_SOC_NOPM, 0, 0xffff, 0,
			aic31xx_set_mode_get, aic31xx_set_mode_put),
};


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
	int reg_mask = 0;
	int ret_wbits = 0;
	int run_state_mask;
	int sync_needed = 0, non_sync_state = 0;
	int other_dsp = 0, run_state = 0;
	struct aic31xx_priv *aic31xx = snd_soc_codec_get_drvdata(w->codec);

	if (w->shift == 7) {
		reg_mask = AIC31XX_LDAC_POWER_STATUS_MASK ;
		run_state_mask = AIC31XX_COPS_MDSP_D_L;
	}
	if (w->shift == 6) {
		reg_mask = AIC31XX_RDAC_POWER_STATUS_MASK ;
		run_state_mask = AIC31XX_COPS_MDSP_D_R;
	}

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		ret_wbits = aic3xxx_wait_bits(w->codec->control_data,
						AIC31XX_DAC_FLAG_1,
						reg_mask, reg_mask,
						AIC31XX_TIME_DELAY,
						AIC31XX_DELAY_COUNTER);
		sync_needed = aic3xxx_reg_read(w->codec->control_data,
						AIC31XX_DAC_PRB);
			non_sync_state =
				DSP_NON_SYNC_MODE(aic31xx->dsp_runstate);
			other_dsp =
				aic31xx->dsp_runstate & AIC31XX_COPS_MDSP_A;

		if (sync_needed && non_sync_state && other_dsp) {
			run_state =
				get_runstate(
					aic31xx->codec->control_data);
			aic31xx_dsp_pwrdwn_status(aic31xx->codec);
			aic31xx_dsp_pwrup(aic31xx->codec, run_state);
		}
		aic31xx->dsp_runstate |= run_state_mask;
		if (!ret_wbits) {
			dev_err(w->codec->dev, "DAC_post_pmu timed out\n");
			return -1;
		}
		break;
	case SND_SOC_DAPM_POST_PMD:
		ret_wbits = aic3xxx_wait_bits(w->codec->control_data,
			AIC31XX_DAC_FLAG_1, reg_mask, 0,
			AIC31XX_TIME_DELAY, AIC31XX_DELAY_COUNTER);
		aic31xx->dsp_runstate =
			(aic31xx->dsp_runstate & ~run_state_mask);
	if (!ret_wbits) {
		dev_err(w->codec->dev, "DAC_post_pmd timed out\n");
		return -1;
	}
		break;
		return -EINVAL;
	}
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
	int run_state = 0;
	int non_sync_state = 0, sync_needed = 0;
	int other_dsp = 0;
	int run_state_mask = 0;
	struct aic31xx_priv *aic31xx = snd_soc_codec_get_drvdata(w->codec);
	int reg_mask = 0;
	int ret_wbits = 0;

	if (w->shift == 7) {
		reg_mask = AIC31XX_ADC_POWER_STATUS_MASK;
		run_state_mask = AIC31XX_COPS_MDSP_A;
	}
	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		ret_wbits = aic3xxx_wait_bits(w->codec->control_data,
						AIC31XX_ADC_FLAG, reg_mask,
						reg_mask, AIC31XX_TIME_DELAY,
						AIC31XX_DELAY_COUNTER);

		sync_needed = aic3xxx_reg_read(w->codec->control_data,
						AIC31XX_DAC_PRB);
		non_sync_state = DSP_NON_SYNC_MODE(aic31xx->dsp_runstate);
		other_dsp = aic31xx->dsp_runstate & AIC31XX_COPS_MDSP_D;
		if (sync_needed && non_sync_state && other_dsp) {
			run_state = get_runstate(
						aic31xx->codec->control_data);
			aic31xx_dsp_pwrdwn_status(aic31xx->codec);
			aic31xx_dsp_pwrup(aic31xx->codec, run_state);
		}
		aic31xx->dsp_runstate |= run_state_mask;
		if (!ret_wbits) {
			dev_err(w->codec->dev, "ADC POST_PMU timedout\n");
			return -1;
		}
		break;

	case SND_SOC_DAPM_POST_PMD:
		ret_wbits = aic3xxx_wait_bits(w->codec->control_data,
						AIC31XX_ADC_FLAG, reg_mask, 0,
						AIC31XX_TIME_DELAY,
						AIC31XX_DELAY_COUNTER);
		aic31xx->dsp_runstate = (aic31xx->dsp_runstate &
					 ~run_state_mask);
		if (!ret_wbits) {
			dev_err(w->codec->dev, "ADC POST_PMD timedout\n");
			return -1;
		}
		break;

		return -EINVAL;
	}
	return 0;
}

/*Hp_power_up_event without powering on/off headphone driver,
* instead muting hpl & hpr */

static int aic31xx_hp_power_up_event(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	u8 lv, rv, val;
	struct snd_soc_codec *codec = w->codec;
	struct aic31xx_priv *aic31xx = snd_soc_codec_get_drvdata(codec);

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


		if (aic31xx->from_resume) {
			aic31xx_mute_codec(codec, 0);
			aic31xx->from_resume = 0;
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
//	u8 counter;
//	int value;
	int lv, rv, val;
	struct snd_soc_codec *codec = w->codec;
//	struct aic31xx_priv *aic31xx = snd_soc_codec_get_drvdata(codec);
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


static int pll_power_on_event(struct snd_soc_dapm_widget *w, \
		struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;

	if (SND_SOC_DAPM_EVENT_ON(event))
		dev_dbg(codec->dev, "pll->on pre_pmu");
	else if (SND_SOC_DAPM_EVENT_OFF(event))
		dev_dbg(codec->dev, "pll->off\n");

	mdelay(10);
	return 0;
}

static const struct snd_soc_dapm_widget aic31xx_dapm_widgets[] = {
	/* DACs */
	SND_SOC_DAPM_DAC_E("Left DAC", "Left Playback",
			AIC31XX_DAC_CHN_REG, 7, 0, aic31xx_dac_power_up_event,
			SND_SOC_DAPM_POST_PMU |	SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_DAC_E("Right DAC", "Right Playback",
			AIC31XX_DAC_CHN_REG, 6, 0, aic31xx_dac_power_up_event,
			SND_SOC_DAPM_POST_PMU |	SND_SOC_DAPM_POST_PMD),

	/* Output Mixers */
	SND_SOC_DAPM_MIXER("Left Output Mixer", SND_SOC_NOPM, 0, 0,
			left_output_mixer_controls,
			ARRAY_SIZE(left_output_mixer_controls)),
	SND_SOC_DAPM_MIXER("Right Output Mixer", SND_SOC_NOPM, 0, 0,
			right_output_mixer_controls,
			ARRAY_SIZE(right_output_mixer_controls)),

	/* Output drivers */
	SND_SOC_DAPM_PGA_E("HPL Driver", AIC31XX_HPL_DRIVER_REG, 2, 0, \
			NULL, 0, aic31xx_hp_power_up_event, \
			SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_PRE_PMD),
	SND_SOC_DAPM_PGA_E("HPR Driver", AIC31XX_HPR_DRIVER_REG, 2, 0, \
			NULL, 0, aic31xx_hp_power_up_event,
			SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_PRE_PMD),


#ifndef AIC3100_CODEC_SUPPORT
	/* For AIC31XX and AIC3110 as it is stereo both left and right channel
	 * class-D can be powered up/down
	 */
	SND_SOC_DAPM_PGA_E("SPL Class - D", AIC31XX_CLASS_D_SPK, 7, 0, NULL, 0,
				aic31xx_sp_event, SND_SOC_DAPM_POST_PMU |
				SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_PGA_E("SPR Class - D", AIC31XX_CLASS_D_SPK, 6, 0, NULL, 0,
				aic31xx_sp_event, SND_SOC_DAPM_POST_PMU | \
				SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),
#endif

#ifdef AIC3100_CODEC_SUPPORT
	/* For AIC3100 as is mono only left
	 * channel class-D can be powered up/down
	 */
	SND_SOC_DAPM_PGA_E("SPL Class - D", AIC31XX_CLASS_D_SPK, 7, 0, NULL, 0, \
			aic31xx_sp_event, SND_SOC_DAPM_POST_PMU | \
			SND_SOC_DAPM_POST_PMD),

#endif

	/* ADC */
	SND_SOC_DAPM_ADC_E("ADC", "Capture", AIC31XX_ADC_CHN_REG, 7, 0, \
			aic31xx_adc_power_up_event, SND_SOC_DAPM_POST_PMU | \
			SND_SOC_DAPM_POST_PMD),

	/*Input Selection to MIC_PGA*/
	SND_SOC_DAPM_MIXER("P_Input_Mixer", SND_SOC_NOPM, 0, 0,
		pos_mic_pga_controls, ARRAY_SIZE(pos_mic_pga_controls)),
	SND_SOC_DAPM_MIXER("M_Input_Mixer", SND_SOC_NOPM, 0, 0,
		neg_mic_pga_controls, ARRAY_SIZE(neg_mic_pga_controls)),

	/*Enabling & Disabling MIC Gain Ctl */
	SND_SOC_DAPM_PGA("MIC_GAIN_CTL", AIC31XX_MICPGA_VOL_CTRL,
		7, 1, NULL, 0),

	SND_SOC_DAPM_SUPPLY("PLLCLK", AIC31XX_CLK_R2, 7, 0, pll_power_on_event,
				 SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
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


/* aic31xx_firmware_load
   This function is called by the request_firmware_nowait function as soon
   as the firmware has been loaded from the file. The firmware structure
   contains the data and the size of the firmware loaded.
 */

void aic31xx_firmware_load(const struct firmware *fw, void *context)
{
	struct snd_soc_codec *codec = context;
	struct aic31xx_priv *private_ds = snd_soc_codec_get_drvdata(codec);
	int ret = 0;
	aic3xxx_cfw_lock(private_ds->cfw_p, 1);
	if (private_ds->cur_fw != NULL)
		release_firmware(private_ds->cur_fw);
	private_ds->cur_fw = NULL ;

	if (fw != NULL) {
		dev_dbg(codec->dev, "Default firmware load\n\n");
		private_ds->cur_fw = (void *)fw;
		ret = aic3xxx_cfw_reload(private_ds->cfw_p, (void *)fw->data,
						fw->size);
		if (ret < 0) { /* reload failed */
			dev_err(codec->dev, "Firmware binary load failed\n");
			release_firmware(private_ds->cur_fw);
			private_ds->cur_fw = NULL;
			fw = NULL;
		} else
			private_ds->isdefault_fw = 0;
	}

	if (fw == NULL) {
		/* either request_firmware or reload failed */
		dev_dbg(codec->dev, "Default firmware load\n");
		ret = aic3xxx_cfw_reload(private_ds->cfw_p, default_firmware,
			sizeof(default_firmware));
		if (ret < 0)
			dev_err(codec->dev, "Default firmware load failed\n");
		else
			private_ds->isdefault_fw = 1;
	}
	aic3xxx_cfw_lock(private_ds->cfw_p, 0); /*  release the lock */
	if (ret >= 0) {
		/* init function for transition */
		aic3xxx_cfw_transition(private_ds->cfw_p, "INIT");
		if (!private_ds->isdefault_fw) {
			aic3xxx_cfw_add_modes(codec, private_ds->cfw_p);
			aic3xxx_cfw_add_controls(codec, private_ds->cfw_p);
		}
		aic3xxx_cfw_setmode_cfg(private_ds->cfw_p, 0, 0);
	}
}


static const struct snd_soc_dapm_route
aic31xx_audio_map[] = {

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


#define AIC31XX_DAPM_ROUTE_NUM (sizeof(aic31xx_dapm_routes)/		\
				sizeof(struct snd_soc_dapm_route))



/*
 * __new_control_info - This function is to initialize data for new control
 * required to program the aic31xx registers.
 */
static int __new_control_info(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_info *uinfo)
{

	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	dev_dbg(codec->dev, "+ new control info\n");

	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
	uinfo->count = 1;
	uinfo->value.integer.min = 0;
	uinfo->value.integer.max = 65535;

	return 0;
}

/*
 * __new_control_get - read data of new control for program the aic31xx
 * registers.
 */
static int __new_control_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	u32 val;

	dev_dbg(codec->dev, "%s (%d)\n", __func__, aic31xx_reg_ctl);

	val = snd_soc_read(codec, aic31xx_reg_ctl);
	ucontrol->value.integer.value[0] = val;

	dev_dbg(codec->dev, "%s val: %d\n", __func__, val);

	return 0;
}

/*
 * __new_control_put - this is called to pass data from user/application to
 * the driver.
 */
static int __new_control_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	u32 data_from_user = ucontrol->value.integer.value[0];
	u8 val = data_from_user & 0x00ff;
	u32 reg = data_from_user >> 8;/* MAKE_REG(book, page, offset) */
	snd_soc_write(codec, reg, val);
	aic31xx_reg_ctl = reg;

	return 0;
}

/*
 * aic31xx_add_controls - add non dapm kcontrols.
 *
 * The different controls are in "aic31xx_snd_controls" table. The following
 * different controls are supported
 *
 *	# DAC Playback volume control
 *	# PCM Playback Volume
 *	# HP Driver Gain
 *	# HP DAC Playback Switch
 *	# PGA Capture Volume
 *	# Program Registers
 */
static int aic31xx_add_controls(struct snd_soc_codec *codec)
{
	int err;

	dev_dbg(codec->dev, "%s\n", __func__);

	err = snd_soc_add_codec_controls(codec, aic31xx_snd_controls,
				ARRAY_SIZE(aic31xx_snd_controls));
	if (err < 0) {
		printk(KERN_INFO "Invalid control\n");
		return err;
	}


	return 0;
}


/*
 * aic31xx_add_widgets
 *
 * adds all the ASoC Widgets identified by aic31xx_snd_controls array. This
 * routine will be invoked * during the Audio Driver Initialization.
 */
static int aic31xx_add_widgets(struct snd_soc_codec *codec)
{
	struct snd_soc_dapm_context *dapm = &codec->dapm;
	int ret = 0;
	dev_dbg(codec->dev, "###aic31xx_add_widgets\n");
	ret = snd_soc_dapm_new_controls(dapm, aic31xx_dapm_widgets,
					ARRAY_SIZE(aic31xx_dapm_widgets));
	if (!ret)
		dev_dbg(codec->dev, "#Completed adding dapm widgets size = %d\n",
					ARRAY_SIZE(aic31xx_dapm_widgets));

	ret = snd_soc_dapm_add_routes(dapm, aic31xx_audio_map,
					ARRAY_SIZE(aic31xx_audio_map));
	if (!ret)
		dev_dbg(codec->dev, "#Completed adding DAPM routes = %d\n",
				ARRAY_SIZE(aic31xx_audio_map));

	ret = snd_soc_dapm_new_widgets(dapm);
	if (!ret)
		dev_dbg(codec->dev, "widgets updated\n");

	return 0;
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
* aic31xx_multi_i2s_set_pll
*
* This function is invoked as part of the PLL call-back
* handler from the ALSA layer.
*/
static int aic31xx_set_dai_pll(struct snd_soc_dai *dai,
		int pll_id, int source, unsigned int freq_in,
		unsigned int freq_out)
{
	struct snd_soc_codec *codec = dai->codec;
	struct aic31xx_priv *aic31xx = snd_soc_codec_get_drvdata(codec);


	dev_dbg(codec->dev, "In aic31xx: dai_set_pll\n");
	dev_dbg(codec->dev, "%d, %s, dai->id = %d\n",
		__LINE__ , __func__ , dai->id);
	snd_soc_update_bits(codec, AIC31XX_CLK_R1,
		AIC31XX_PLL_CLKIN_MASK, source << AIC31XX_PLL_CLKIN_SHIFT);
	/*  TODO: How to select low/high clock range? */

	mutex_lock(&aic31xx->cfw_mutex);
	aic3xxx_cfw_set_pll(aic31xx->cfw_p, dai->id);
	mutex_unlock(&aic31xx->cfw_mutex);

	dev_dbg(codec->dev, "%s: DAI ID %d PLL_ID %d InFreq %d OutFreq %d\n",
		__func__, pll_id, dai->id, freq_in, freq_out);
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


static int aic31xx_suspend(struct snd_soc_codec *codec)
{
	int val, lv, rv;
	struct aic31xx_priv *aic31xx = snd_soc_codec_get_drvdata(codec);
	dev_dbg(codec->dev, "%s: Entered\n", __func__);

	if (aic31xx->playback_status == 0) {

		lv = snd_soc_read(codec, AIC31XX_LEFT_ANALOG_HPL);
		lv |= 0x7f;
		rv = snd_soc_read(codec, AIC31XX_RIGHT_ANALOG_HPR);
		rv |= 0x7f;
		val = lv ^ ((lv ^ rv) & -(lv < rv));
		while (val > 0) {
			snd_soc_write(codec, AIC31XX_LEFT_ANALOG_HPL, val);
			mdelay(1);
			snd_soc_write(codec, AIC31XX_RIGHT_ANALOG_HPR, val);
			val--;
			mdelay(1);
		}
		aic31xx->from_resume = 0;
		aic31xx_mute_codec(codec, 1);
		aic31xx_set_bias_level(codec, SND_SOC_BIAS_OFF);

		/* Bit 7 of Page 1/ Reg 46 gives the soft powerdown control.
		 * Setting this bit will further reduces the amount of power
		 * consumption
		 */
		val = snd_soc_read(codec, AIC31XX_MICBIAS_CTRL_REG);
		snd_soc_write(codec, AIC31XX_MICBIAS_CTRL_REG, val | BIT7);

		/*Switching off the headphone driver*/
		val = snd_soc_read(codec, AIC31XX_HPHONE_DRIVERS);
		val &= (~(BIT7));
		val &= (~(BIT6));
		snd_soc_write(codec, AIC31XX_HPHONE_DRIVERS, val);

	}
	dev_dbg(codec->dev, "%s: Exiting\n", __func__);
	return 0;
}

static int aic31xx_resume(struct snd_soc_codec *codec)
{
	struct aic31xx_priv *priv = snd_soc_codec_get_drvdata(codec);
	u8 val;

	dev_dbg(codec->dev, "###aic31xx_resume\n");
	dev_dbg(codec->dev, "%s: Entered\n", __func__);


	/* Switching on the headphone driver*/
	val = snd_soc_read(codec, AIC31XX_HPHONE_DRIVERS);
	val |=  (BIT7|BIT6);
	snd_soc_write(codec, AIC31XX_HPHONE_DRIVERS, val);

	priv->from_resume = 1;

	val = snd_soc_read(codec, AIC31XX_MICBIAS_CTRL_REG);
	snd_soc_write(codec, AIC31XX_MICBIAS_CTRL_REG, val & ~BIT7);
	dev_dbg(codec->dev, "%s: Exiting\n", __func__);

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

/*
 *----------------------------------------------------------------------------
 * Function : aic31xx_probe
 * Purpose  : This is first driver function called by the SoC core driver.
 *
 *----------------------------------------------------------------------------
 */

static int aic31xx_codec_probe(struct snd_soc_codec *codec)
{


	int ret = 0;
//	int value;
	struct aic3xxx *control;
	struct aic31xx_priv *aic31xx;
	struct aic31xx_jack_data *jack;

	if (codec == NULL)
		dev_err(codec->dev, "codec pointer is NULL.\n");


	codec->control_data = dev_get_drvdata(codec->dev->parent);
	control = codec->control_data;

	aic31xx = kzalloc(sizeof(struct aic31xx_priv), GFP_KERNEL);

	if (aic31xx == NULL)
		return -ENOMEM;

	snd_soc_codec_set_drvdata(codec, aic31xx);
	aic31xx->pdata = dev_get_platdata(codec->dev->parent);
	aic31xx->codec = codec;
	aic31xx->playback_status = 0;
	aic31xx->power_status = 0;
	aic31xx->mute = 1;
	aic31xx->headset_connected = 0;
	aic31xx->i2c_regs_status = 0;

	aic31xx->cur_fw = NULL;
	aic31xx->isdefault_fw = 0;
	aic31xx->cfw_p = &(aic31xx->cfw_ps);
	aic31xx_codec_write(codec, AIC31XX_RESET_REG , 0x01);
	mdelay(10);

	aic3xxx_cfw_init(aic31xx->cfw_p, &aic31xx_cfw_codec_ops, aic31xx);
	aic31xx->workqueue = create_singlethread_workqueue("aic31xx-codec");
	if (!aic31xx->workqueue) {
		ret = -ENOMEM;
		goto work_err;
	}
	aic31xx->power_status = 1;
	aic31xx->headset_connected = 1;
	aic31xx->mute = 0;
	aic31xx_dac_mute(codec, 1);
	aic31xx->headset_connected = 0;
	aic31xx->headset_current_status = 0;

	mutex_init(&aic31xx->mutex);
	mutex_init(&codec->mutex);
	mutex_init(&aic31xx->cfw_mutex);
	aic31xx->dsp_runstate = 0;
	/* use switch-class based headset reporting if platform requires it */
	jack = &aic31xx->hs_jack;

	aic31xx->idev = input_allocate_device();

	if (aic31xx->idev <= 0) {

		dev_dbg(codec->dev, "Allocate failed\n");
		goto input_dev_err;
	}

	input_set_capability(aic31xx->idev, EV_KEY, KEY_MEDIA);

	ret = input_register_device(aic31xx->idev);
	if (ret < 0) {
		dev_err(codec->dev, "register input dev fail\n");
		goto input_dev_err;
	}
		/* Dynamic Headset detection enabled */
		snd_soc_update_bits(codec, AIC31XX_HS_DETECT_REG,
			AIC31XX_HEADSET_IN_MASK, AIC31XX_HEADSET_IN_MASK);

	/* off, with power on */
	aic31xx_set_bias_level(codec, SND_SOC_BIAS_STANDBY);
	aic31xx->mute_asi = 0;

	aic31xx_add_controls(codec);
	aic31xx_add_widgets(codec);
	ret = aic31xx_driver_init(codec);
	if (ret < 0)
		dev_dbg(codec->dev,
	"\nAIC31xx CODEC: aic31xx_probe: TiLoad Initialization failed\n");


	dev_dbg(codec->dev, "%d, %s, Firmware test\n", __LINE__, __func__);
	request_firmware_nowait(THIS_MODULE, FW_ACTION_HOTPLUG,
		"tlv320aic31xx_fw_v1.bin", codec->dev, GFP_KERNEL, codec,
		aic31xx_firmware_load);

	return 0;


input_dev_err:
	input_unregister_device(aic31xx->idev);
	input_free_device(aic31xx->idev);

work_err:
	kfree(aic31xx);

	return 0;
}

/*
 * Remove aic31xx soc device
 */
static int aic31xx_codec_remove(struct snd_soc_codec *codec)
{
	/* power down chip */
	struct aic31xx_priv *aic31xx = snd_soc_codec_get_drvdata(codec);
	struct aic3xxx *control = codec->control_data;
//	struct aic31xx_jack_data *jack = &aic31xx->hs_jack;

	aic31xx_set_bias_level(codec, SND_SOC_BIAS_OFF);

	/* free_irq if any */
	switch (control->type) {
	case TLV320AIC31XX:
		if (control->irq) {
			aic3xxx_free_irq(control, AIC31XX_IRQ_HEADSET_DETECT,
				codec);
			aic3xxx_free_irq(control, AIC31XX_IRQ_BUTTON_PRESS,
				codec);
		}
		break;
	}
	/* release firmware if any */
	if (aic31xx->cur_fw != NULL)
		release_firmware(aic31xx->cur_fw);

	/* Unregister input device and switch */

	input_unregister_device(aic31xx->idev);
	input_free_device(aic31xx->idev);
	kfree(aic31xx);

	return 0;
}


int aic31xx_ops_reg_read(void *p, unsigned int reg)
{
	struct aic31xx_priv *ps = p;
	union cfw_register *c = (union cfw_register *) &reg;
	aic31xx_reg_union mreg;

	mreg.aic3xxx_register.offset = c->offset;
	mreg.aic3xxx_register.page = c->page;
	mreg.aic3xxx_register.book = c->book;
	mreg.aic3xxx_register.reserved = 0;

		return aic3xxx_reg_read(ps->codec->control_data,
				mreg.aic3xxx_register_int);

}

int aic31xx_ops_reg_write(void  *p, unsigned int reg, unsigned char mval)
{
	struct aic31xx_priv *ps = p;
	aic31xx_reg_union mreg;
	union cfw_register *c = (union cfw_register *) &reg;

		mreg.aic3xxx_register.offset = c->offset;
		mreg.aic3xxx_register.page = c->page;
		mreg.aic3xxx_register.book = c->book;
		mreg.aic3xxx_register.reserved = 0;
		mval = c->data;
		printk(KERN_INFO"page %d book %d offset %d\n",
			mreg.aic3xxx_register.page, mreg.aic3xxx_register.book,
			mreg.aic3xxx_register.offset);
		return aic3xxx_reg_write(ps->codec->control_data,
			mreg.aic3xxx_register_int, mval);
}

int aic31xx_ops_set_bits(void *p, unsigned int reg, unsigned char mask, unsigned char val)
{
	struct aic31xx_priv *ps = p;

	aic31xx_reg_union mreg;
	union cfw_register *c = (union cfw_register *) &reg;
	mreg.aic3xxx_register.offset = c->offset;
	mreg.aic3xxx_register.page = c->page;
	mreg.aic3xxx_register.book = c->book;
	mreg.aic3xxx_register.reserved = 0;

	return aic3xxx_set_bits(ps->codec->control_data,
		mreg.aic3xxx_register_int, mask, val);
}

int aic31xx_ops_bulk_read(void *p, unsigned int reg, int count, u8 *buf)
{
	struct aic31xx_priv  *ps = p;

	aic31xx_reg_union mreg;
	union cfw_register *c = (union cfw_register *) &reg;
	mreg.aic3xxx_register.offset = c->offset;
	mreg.aic3xxx_register.page = c->page;
	mreg.aic3xxx_register.book = c->book;
	mreg.aic3xxx_register.reserved = 0;

		return aic3xxx_bulk_read(ps->codec->control_data,
		mreg.aic3xxx_register_int, count, buf);
}

int aic31xx_ops_bulk_write(void *p, unsigned int reg , int count, const u8 *buf)
{
	struct aic31xx_priv *ps = p;
	aic31xx_reg_union mreg;
	union cfw_register *c = (union cfw_register *) &reg;

	mreg.aic3xxx_register.offset = c->offset;
	mreg.aic3xxx_register.page = c->page;
	mreg.aic3xxx_register.book = c->book;
	mreg.aic3xxx_register.reserved = 0;

		aic3xxx_bulk_write(ps->codec->control_data,
		mreg.aic3xxx_register_int, count, buf);
	return 0;
}

/*****************************************************************************
  Function Name : aic31xx_ops_lock
Argument      : pointer argument to the codec
Return value  : Integer
Purpose       : To Read the run state of the DAC and ADC
by reading the codec and returning the run state

Run state Bit format

------------------------------------------------------
D31|..........| D7 | D6|  D5  |  D4  | D3 | D2 | D1  |   D0  |
R               R    R     R     ADC    R    R   LDAC   RDAC
------------------------------------------------------

 *****************************************************************************/

int aic31xx_ops_lock(void *pv)
{
	int run_state = 0;
	struct aic31xx_priv *aic31xx = (struct aic31xx_priv *) pv;
	mutex_lock(&aic31xx->codec->mutex);

	/* Reading the run state of adc and dac */
	run_state = get_runstate(aic31xx->codec->control_data);

	printk(KERN_INFO"< LOCK >runstate in lock fn : %x\n", run_state);
	return run_state;
}

/*****************************************************************************
Function name	: aic31xx_ops_unlock
Argument	: pointer argument to the codec
Return Value	: integer returning 0
Purpose	: To unlock the mutex acqiured for reading
run state of the codec
 *****************************************************************************/
int aic31xx_ops_unlock(void *pv)
{
	/*Releasing the lock of mutex */
	struct aic31xx_priv *aic31xx = (struct aic31xx_priv *) pv;

	printk(KERN_INFO "< UNLOCK >Unlock function\n");
	mutex_unlock(&aic31xx->codec->mutex);
	return 0;
}

/*****************************************************************************
*Function Name	:aic31xx_ops_stop
*Argument	:pointer Argument to the codec
*mask tells us the bit format of the
*codec running state

*Bit Format:
------------------------------------------------------
D31|..........| D7 | D6| D5 | D4 | D3 | D2 | D1 | D0 |
R               R    R    R    A    R    R   DL   DR
------------------------------------------------------
R  - Reserved
A  - minidsp_A
D  - minidsp_D
 *****************************************************************************/

int aic31xx_ops_stop(void *pv, int mask)
{
	int	run_state = 0;
	int	cur_state = 0;
	int	count = 100;
	struct 	aic31xx_priv *aic31xx = (struct aic31xx_priv *) pv;
	int	limask = 0;

	mutex_lock(&aic31xx->codec->mutex);
	printk(KERN_INFO "< STOP >Ops stop after lock\n");
	run_state = get_runstate(aic31xx->codec->control_data);
	limask = (mask & AIC3XX_COPS_MDSP_A);
	if (limask) {
		if ((limask & 0x10) == 0x10)
			/*Single ADC's in AIC31xx needs to be switched off */
			aic3xxx_set_bits(aic31xx->codec->control_data,
			AIC31XX_ADC_DATAPATH_SETUP, 0x80, 0);

		}
		limask = (mask & AIC3XX_COPS_MDSP_D);
		if (limask) {
			if ((limask & 0x03) == 0x03) {
				/* Both DAC to be switched off*/
				aic3xxx_set_bits(aic31xx->codec->control_data,
				AIC31XX_DAC_DATAPATH_SETUP, 0xC0, 0);
		} else {
			if (limask & 0x01)  /* Right DAC to be switched off */
				aic3xxx_set_bits(aic31xx->codec->control_data,
					AIC31XX_DAC_DATAPATH_SETUP, 0x40, 0);

				if (limask & 0x02)
					/*Left DAC to be switched off*/
					aic3xxx_set_bits(aic31xx->
						codec->control_data,
						AIC31XX_DAC_DATAPATH_SETUP,
						0x80, 0);
				}
			}
		/* waiting for write to complete*/

	do {
		cur_state = get_runstate(aic31xx->codec->control_data);
		count--;
	} while (((cur_state&mask & AIC3XX_COPS_MDSP_A) ||
			(cur_state&mask & AIC3XX_COPS_MDSP_D)) && count);

	return run_state;

}

/*****************************************************************************
*Function name	:aic31xx_ops_restore
*Argument	:pointer argument to the codec, run_state
*Return Value	:integer returning 0
*Purpose	:To unlock the mutex acqiured for reading
*run state of the codec and to restore the states of the dsp
 *****************************************************************************/
int aic31xx_ops_restore(void *pv, int run_state)
{
	struct aic31xx_priv *aic31xx = (struct aic31xx_priv *)pv;

	aic31xx_dsp_pwrup(pv, run_state);

	printk(KERN_INFO"< RESTORE >Restore function Before unlock\n");
	mutex_unlock(&aic31xx->codec->mutex);

	return 0;
}

/*****************************************************************************
*Function name	:aic31xx_ops_adaptivebuffer_swap
*Argument	:pointer argument to the codec, mask tells us which dsp has to
*		 be chosen for swapping
*Return Value	:integer returning 0
*Purpose	:To swap the coefficient buffers of minidsp according to mask
 *****************************************************************************/

int aic31xx_ops_adaptivebuffer_swap(void *pv, int mask)
{

	int read_state = 0;
	struct aic31xx_priv *aic31xx = (struct aic31xx_priv *) pv;

	if (mask & AIC3XX_ABUF_MDSP_D1) {

		int count = 0;

		aic3xxx_set_bits(aic31xx->codec->control_data,
			AIC31XX_DAC_ADAPTIVE_BANK1_REG, 0x1, 0x1);
		do {
			read_state = aic3xxx_reg_read(aic31xx->codec->
				control_data, AIC31XX_DAC_ADAPTIVE_BANK1_REG);
			count++;
		} while ((read_state & 0x1) && (count <= 60));
	}

	return 0;
}

/*****************************************************************************
*Function name	:get_runstate
*Argument	:pointer argument to the codec
*Return Value	:integer returning the runstate
*Purpose	:To read the current state of the dac's and adc's
 *****************************************************************************/

int get_runstate(void *ps)
{

	struct aic3xxx *pr = ps;
	int run_state = 0;
	int DAC_state = 0, ADC_state = 0;
	/* Read the run state */
	DAC_state = aic3xxx_reg_read(pr, AIC31XX_DAC_FLAG_1);
	ADC_state = aic3xxx_reg_read(pr, AIC31XX_ADC_FLAG);

	DSP_STATUS(run_state, ADC_state, 6, 4);
	DSP_STATUS(run_state, DAC_state, 7, 1);
	DSP_STATUS(run_state, DAC_state, 3, 0);

	return run_state;

}

/*****************************************************************************
*Function name	:aic31xx_dsp_pwrdwn_status
*Argument	:pointer argument to the codec , cur_state of dac's and adc's
*Return Value	:integer returning 0
*Purpose	:To read the status of dsp's
 *****************************************************************************/

int aic31xx_dsp_pwrdwn_status(void *pv)
{
/*ptr to the priv data structure*/

	struct aic31xx_priv *aic31xx = pv;
	int count = 100;
	int cur_state = 0;

	aic3xxx_set_bits(aic31xx->codec->control_data,
		AIC31XX_ADC_DATAPATH_SETUP, 0x80, 0);
	aic3xxx_set_bits(aic31xx->codec->control_data,
		AIC31XX_DAC_DATAPATH_SETUP, 0xC0, 0);

	do {
		cur_state = get_runstate(aic31xx->codec->control_data);
	} while (cur_state && count--);


	return 0;
}

int aic31xx_dsp_pwrup(void *pv, int state)
{

	int cur_state = 0;
	int count = 100;
	struct aic31xx_priv *aic31xx  = (struct aic31xx_priv *)pv;

	if (state & AIC31XX_COPS_MDSP_A)
		aic3xxx_set_bits(aic31xx->codec->control_data,
		AIC31XX_ADC_DATAPATH_SETUP, 0x80, 0x80);

	if (state & AIC31XX_COPS_MDSP_D)
		aic3xxx_set_bits(aic31xx->codec->control_data,
		AIC31XX_DAC_DATAPATH_SETUP, 0xC0, 0xC0);
	else {
		if (state & AIC31XX_COPS_MDSP_D_L)
			aic3xxx_set_bits(aic31xx->codec->control_data,
			AIC31XX_DAC_DATAPATH_SETUP, 0x80, 0x80);
		if (state & AIC31XX_COPS_MDSP_D_R)
			aic3xxx_set_bits(aic31xx->codec->control_data,
			AIC31XX_DAC_DATAPATH_SETUP, 0x40, 0x40);
	}

	/*loop for waiting for the dsps to power up together*/
	do {
		cur_state = get_runstate(aic31xx->codec->control_data);
	} while ((state == cur_state) && count--);

	return 0;
}

struct aic3xxx_codec_ops aic31xx_cfw_codec_ops = {
	.reg_read	=	aic31xx_ops_reg_read,
	.reg_write	=	aic31xx_ops_reg_write,
	.set_bits	=	aic31xx_ops_set_bits,
	.bulk_read	=	aic31xx_ops_bulk_read,
	.bulk_write	=	aic31xx_ops_bulk_write,
	.lock		=	aic31xx_ops_lock,
	.unlock		=	aic31xx_ops_unlock,
	.stop		=	aic31xx_ops_stop,
	.restore	=	aic31xx_ops_restore,
	.bswap		=	aic31xx_ops_adaptivebuffer_swap,
};

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
	.set_pll	= aic31xx_set_dai_pll,
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
}
};


static int aic31xx_probe(struct platform_device *pdev)
{
	int ret;
	ret = snd_soc_register_codec(&pdev->dev, &soc_codec_driver_aic31xx,
			aic31xx_dai_driver, ARRAY_SIZE(aic31xx_dai_driver));

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

