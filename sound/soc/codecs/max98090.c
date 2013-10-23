/*
 * max98090.c -- MAX98090 ALSA SoC Audio driver
 *
 * Copyright 2011-2012 Maxim Integrated Products
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/pm.h>
#include <linux/pm_runtime.h>
#include <linux/regmap.h>
#include <linux/slab.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/tlv.h>
#include <sound/max98090.h>
#include "max98090.h"

#include <linux/version.h>

#define DEBUG
#define EXTMIC_METHOD
#define EXTMIC_METHOD_TEST

/* Allows for sparsely populated register maps */
static struct reg_default max98090_reg[] = {
	{ 0x00, 0x00 }, /* 00 Software Reset */
	{ 0x03, 0x04 }, /* 03 Interrupt Masks */
	{ 0x04, 0x00 }, /* 04 System Clock Quick */
	{ 0x05, 0x00 }, /* 05 Sample Rate Quick */
	{ 0x06, 0x00 }, /* 06 DAI Interface Quick */
	{ 0x07, 0x00 }, /* 07 DAC Path Quick */
	{ 0x08, 0x00 }, /* 08 Mic/Direct to ADC Quick */
	{ 0x09, 0x00 }, /* 09 Line to ADC Quick */
	{ 0x0A, 0x00 }, /* 0A Analog Mic Loop Quick */
	{ 0x0B, 0x00 }, /* 0B Analog Line Loop Quick */
	{ 0x0C, 0x00 }, /* 0C Reserved */
	{ 0x0D, 0x00 }, /* 0D Input Config */
	{ 0x0E, 0x1B }, /* 0E Line Input Level */
	{ 0x0F, 0x00 }, /* 0F Line Config */

	{ 0x10, 0x14 }, /* 10 Mic1 Input Level */
	{ 0x11, 0x14 }, /* 11 Mic2 Input Level */
	{ 0x12, 0x00 }, /* 12 Mic Bias Voltage */
	{ 0x13, 0x00 }, /* 13 Digital Mic Config */
	{ 0x14, 0x00 }, /* 14 Digital Mic Mode */
	{ 0x15, 0x00 }, /* 15 Left ADC Mixer */
	{ 0x16, 0x00 }, /* 16 Right ADC Mixer */
	{ 0x17, 0x03 }, /* 17 Left ADC Level */
	{ 0x18, 0x03 }, /* 18 Right ADC Level */
	{ 0x19, 0x00 }, /* 19 ADC Biquad Level */
	{ 0x1A, 0x00 }, /* 1A ADC Sidetone */
	{ 0x1B, 0x00 }, /* 1B System Clock */
	{ 0x1C, 0x00 }, /* 1C Clock Mode */
	{ 0x1D, 0x00 }, /* 1D Any Clock 1 */
	{ 0x1E, 0x00 }, /* 1E Any Clock 2 */
	{ 0x1F, 0x00 }, /* 1F Any Clock 3 */

	{ 0x20, 0x00 }, /* 20 Any Clock 4 */
	{ 0x21, 0x00 }, /* 21 Master Mode */
	{ 0x22, 0x00 }, /* 22 Interface Format */
	{ 0x23, 0x00 }, /* 23 TDM Format 1*/
	{ 0x24, 0x00 }, /* 24 TDM Format 2*/
	{ 0x25, 0x00 }, /* 25 I/O Configuration */
	{ 0x26, 0x80 }, /* 26 Filter Config */
	{ 0x27, 0x00 }, /* 27 DAI Playback Level */
	{ 0x28, 0x00 }, /* 28 EQ Playback Level */
	{ 0x29, 0x00 }, /* 29 Left HP Mixer */
	{ 0x2A, 0x00 }, /* 2A Right HP Mixer */
	{ 0x2B, 0x00 }, /* 2B HP Control */
	{ 0x2C, 0x1A }, /* 2C Left HP Volume */
	{ 0x2D, 0x1A }, /* 2D Right HP Volume */
	{ 0x2E, 0x00 }, /* 2E Left Spk Mixer */
	{ 0x2F, 0x00 }, /* 2F Right Spk Mixer */

	{ 0x30, 0x00 }, /* 30 Spk Control */
	{ 0x31, 0x2C }, /* 31 Left Spk Volume */
	{ 0x32, 0x2C }, /* 32 Right Spk Volume */
	{ 0x33, 0x00 }, /* 33 ALC Timing */
	{ 0x34, 0x00 }, /* 34 ALC Compressor */
	{ 0x35, 0x00 }, /* 35 ALC Expander */
	{ 0x36, 0x00 }, /* 36 ALC Gain */
	{ 0x37, 0x00 }, /* 37 Rcv/Line OutL Mixer */
	{ 0x38, 0x00 }, /* 38 Rcv/Line OutL Control */
	{ 0x39, 0x15 }, /* 39 Rcv/Line OutL Volume */
	{ 0x3A, 0x00 }, /* 3A Line OutR Mixer */
	{ 0x3B, 0x00 }, /* 3B Line OutR Control */
	{ 0x3C, 0x15 }, /* 3C Line OutR Volume */
	{ 0x3D, 0x00 }, /* 3D Jack Detect */
	{ 0x3E, 0x00 }, /* 3E Input Enable */
	{ 0x3F, 0x00 }, /* 3F Output Enable */

	{ 0x40, 0x00 }, /* 40 Level Control */
	{ 0x41, 0x00 }, /* 41 DSP Filter Enable */
	{ 0x42, 0x00 }, /* 42 Bias Control */
	{ 0x43, 0x00 }, /* 43 DAC Control */
	{ 0x44, 0x06 }, /* 44 ADC Control */
	{ 0x45, 0x00 }, /* 45 Device Shutdown */
	{ 0x46, 0x00 }, /* 46 Equalizer Band 1 Coefficient B0 */
	{ 0x47, 0x00 }, /* 47 Equalizer Band 1 Coefficient B0 */
	{ 0x48, 0x00 }, /* 48 Equalizer Band 1 Coefficient B0 */
	{ 0x49, 0x00 }, /* 49 Equalizer Band 1 Coefficient B1 */
	{ 0x4A, 0x00 }, /* 4A Equalizer Band 1 Coefficient B1 */
	{ 0x4B, 0x00 }, /* 4B Equalizer Band 1 Coefficient B1 */
	{ 0x4C, 0x00 }, /* 4C Equalizer Band 1 Coefficient B2 */
	{ 0x4D, 0x00 }, /* 4D Equalizer Band 1 Coefficient B2 */
	{ 0x4E, 0x00 }, /* 4E Equalizer Band 1 Coefficient B2 */
	{ 0x4F, 0x00 }, /* 4F Equalizer Band 1 Coefficient A1 */

	{ 0x50, 0x00 }, /* 50 Equalizer Band 1 Coefficient A1 */
	{ 0x51, 0x00 }, /* 51 Equalizer Band 1 Coefficient A1 */
	{ 0x52, 0x00 }, /* 52 Equalizer Band 1 Coefficient A2 */
	{ 0x53, 0x00 }, /* 53 Equalizer Band 1 Coefficient A2 */
	{ 0x54, 0x00 }, /* 54 Equalizer Band 1 Coefficient A2 */
	{ 0x55, 0x00 }, /* 55 Equalizer Band 2 Coefficient B0 */
	{ 0x56, 0x00 }, /* 56 Equalizer Band 2 Coefficient B0 */
	{ 0x57, 0x00 }, /* 57 Equalizer Band 2 Coefficient B0 */
	{ 0x58, 0x00 }, /* 58 Equalizer Band 2 Coefficient B1 */
	{ 0x59, 0x00 }, /* 59 Equalizer Band 2 Coefficient B1 */
	{ 0x5A, 0x00 }, /* 5A Equalizer Band 2 Coefficient B1 */
	{ 0x5B, 0x00 }, /* 5B Equalizer Band 2 Coefficient B2 */
	{ 0x5C, 0x00 }, /* 5C Equalizer Band 2 Coefficient B2 */
	{ 0x5D, 0x00 }, /* 5D Equalizer Band 2 Coefficient B2 */
	{ 0x5E, 0x00 }, /* 5E Equalizer Band 2 Coefficient A1 */
	{ 0x5F, 0x00 }, /* 5F Equalizer Band 2 Coefficient A1 */

	{ 0x60, 0x00 }, /* 60 Equalizer Band 2 Coefficient A1 */
	{ 0x61, 0x00 }, /* 61 Equalizer Band 2 Coefficient A2 */
	{ 0x62, 0x00 }, /* 62 Equalizer Band 2 Coefficient A2 */
	{ 0x63, 0x00 }, /* 63 Equalizer Band 2 Coefficient A2 */
	{ 0x64, 0x00 }, /* 64 Equalizer Band 3 Coefficient B0 */
	{ 0x65, 0x00 }, /* 65 Equalizer Band 3 Coefficient B0 */
	{ 0x66, 0x00 }, /* 66 Equalizer Band 3 Coefficient B0 */
	{ 0x67, 0x00 }, /* 67 Equalizer Band 3 Coefficient B1 */
	{ 0x68, 0x00 }, /* 68 Equalizer Band 3 Coefficient B1 */
	{ 0x69, 0x00 }, /* 69 Equalizer Band 3 Coefficient B1 */
	{ 0x6A, 0x00 }, /* 6A Equalizer Band 3 Coefficient B2 */
	{ 0x6B, 0x00 }, /* 6B Equalizer Band 3 Coefficient B2 */
	{ 0x6C, 0x00 }, /* 6C Equalizer Band 3 Coefficient B2 */
	{ 0x6D, 0x00 }, /* 6D Equalizer Band 3 Coefficient A1 */
	{ 0x6E, 0x00 }, /* 6E Equalizer Band 3 Coefficient A1 */
	{ 0x6F, 0x00 }, /* 6F Equalizer Band 3 Coefficient A1 */

	{ 0x70, 0x00 }, /* 70 Equalizer Band 3 Coefficient A2 */
	{ 0x71, 0x00 }, /* 71 Equalizer Band 3 Coefficient A2 */
	{ 0x72, 0x00 }, /* 72 Equalizer Band 3 Coefficient A2 */
	{ 0x73, 0x00 }, /* 73 Equalizer Band 4 Coefficient B0 */
	{ 0x74, 0x00 }, /* 74 Equalizer Band 4 Coefficient B0 */
	{ 0x75, 0x00 }, /* 75 Equalizer Band 4 Coefficient B0 */
	{ 0x76, 0x00 }, /* 76 Equalizer Band 4 Coefficient B1 */
	{ 0x77, 0x00 }, /* 77 Equalizer Band 4 Coefficient B1 */
	{ 0x78, 0x00 }, /* 78 Equalizer Band 4 Coefficient B1 */
	{ 0x79, 0x00 }, /* 79 Equalizer Band 4 Coefficient B2 */
	{ 0x7A, 0x00 }, /* 7A Equalizer Band 4 Coefficient B2 */
	{ 0x7B, 0x00 }, /* 7B Equalizer Band 4 Coefficient B2 */
	{ 0x7C, 0x00 }, /* 7C Equalizer Band 4 Coefficient A1 */
	{ 0x7D, 0x00 }, /* 7D Equalizer Band 4 Coefficient A1 */
	{ 0x7E, 0x00 }, /* 7E Equalizer Band 4 Coefficient A1 */
	{ 0x7F, 0x00 }, /* 7F Equalizer Band 4 Coefficient A2 */

	{ 0x80, 0x00 }, /* 80 Equalizer Band 4 Coefficient A2 */
	{ 0x81, 0x00 }, /* 81 Equalizer Band 4 Coefficient A2 */
	{ 0x82, 0x00 }, /* 82 Equalizer Band 5 Coefficient B0 */
	{ 0x83, 0x00 }, /* 83 Equalizer Band 5 Coefficient B0 */
	{ 0x84, 0x00 }, /* 84 Equalizer Band 5 Coefficient B0 */
	{ 0x85, 0x00 }, /* 85 Equalizer Band 5 Coefficient B1 */
	{ 0x86, 0x00 }, /* 86 Equalizer Band 5 Coefficient B1 */
	{ 0x87, 0x00 }, /* 87 Equalizer Band 5 Coefficient B1 */
	{ 0x88, 0x00 }, /* 88 Equalizer Band 5 Coefficient B2 */
	{ 0x89, 0x00 }, /* 89 Equalizer Band 5 Coefficient B2 */
	{ 0x8A, 0x00 }, /* 8A Equalizer Band 5 Coefficient B2 */
	{ 0x8B, 0x00 }, /* 8B Equalizer Band 5 Coefficient A1 */
	{ 0x8C, 0x00 }, /* 8C Equalizer Band 5 Coefficient A1 */
	{ 0x8D, 0x00 }, /* 8D Equalizer Band 5 Coefficient A1 */
	{ 0x8E, 0x00 }, /* 8E Equalizer Band 5 Coefficient A2 */
	{ 0x8F, 0x00 }, /* 8F Equalizer Band 5 Coefficient A2 */

	{ 0x90, 0x00 }, /* 90 Equalizer Band 5 Coefficient A2 */
	{ 0x91, 0x00 }, /* 91 Equalizer Band 6 Coefficient B0 */
	{ 0x92, 0x00 }, /* 92 Equalizer Band 6 Coefficient B0 */
	{ 0x93, 0x00 }, /* 93 Equalizer Band 6 Coefficient B0 */
	{ 0x94, 0x00 }, /* 94 Equalizer Band 6 Coefficient B1 */
	{ 0x95, 0x00 }, /* 95 Equalizer Band 6 Coefficient B1 */
	{ 0x96, 0x00 }, /* 96 Equalizer Band 6 Coefficient B1 */
	{ 0x97, 0x00 }, /* 97 Equalizer Band 6 Coefficient B2 */
	{ 0x98, 0x00 }, /* 98 Equalizer Band 6 Coefficient B2 */
	{ 0x99, 0x00 }, /* 99 Equalizer Band 6 Coefficient B2 */
	{ 0x9A, 0x00 }, /* 9A Equalizer Band 6 Coefficient A1 */
	{ 0x9B, 0x00 }, /* 9B Equalizer Band 6 Coefficient A1 */
	{ 0x9C, 0x00 }, /* 9C Equalizer Band 6 Coefficient A1 */
	{ 0x9D, 0x00 }, /* 9D Equalizer Band 6 Coefficient A2 */
	{ 0x9E, 0x00 }, /* 9E Equalizer Band 6 Coefficient A2 */
	{ 0x9F, 0x00 }, /* 9F Equalizer Band 6 Coefficient A2 */

	{ 0xA0, 0x00 }, /* A0 Equalizer Band 7 Coefficient B0 */
	{ 0xA1, 0x00 }, /* A1 Equalizer Band 7 Coefficient B0 */
	{ 0xA2, 0x00 }, /* A2 Equalizer Band 7 Coefficient B0 */
	{ 0xA3, 0x00 }, /* A3 Equalizer Band 7 Coefficient B1 */
	{ 0xA4, 0x00 }, /* A4 Equalizer Band 7 Coefficient B1 */
	{ 0xA5, 0x00 }, /* A5 Equalizer Band 7 Coefficient B1 */
	{ 0xA6, 0x00 }, /* A6 Equalizer Band 7 Coefficient B2 */
	{ 0xA7, 0x00 }, /* A7 Equalizer Band 7 Coefficient B2 */
	{ 0xA8, 0x00 }, /* A8 Equalizer Band 7 Coefficient B2 */
	{ 0xA9, 0x00 }, /* A9 Equalizer Band 7 Coefficient A1 */
	{ 0xAA, 0x00 }, /* AA Equalizer Band 7 Coefficient A1 */
	{ 0xAB, 0x00 }, /* AB Equalizer Band 7 Coefficient A1 */
	{ 0xAC, 0x00 }, /* AC Equalizer Band 7 Coefficient A2 */
	{ 0xAD, 0x00 }, /* AD Equalizer Band 7 Coefficient A2 */
	{ 0xAE, 0x00 }, /* AE Equalizer Band 7 Coefficient A2 */
	{ 0xAF, 0x00 }, /* AF ADC Biquad Coefficient B0 */

	{ 0xB0, 0x00 }, /* B0 ADC Biquad Coefficient B0 */
	{ 0xB1, 0x00 }, /* B1 ADC Biquad Coefficient B0 */
	{ 0xB2, 0x00 }, /* B2 ADC Biquad Coefficient B1 */
	{ 0xB3, 0x00 }, /* B3 ADC Biquad Coefficient B1 */
	{ 0xB4, 0x00 }, /* B4 ADC Biquad Coefficient B1 */
	{ 0xB5, 0x00 }, /* B5 ADC Biquad Coefficient B2 */
	{ 0xB6, 0x00 }, /* B6 ADC Biquad Coefficient B2 */
	{ 0xB7, 0x00 }, /* B7 ADC Biquad Coefficient B2 */
	{ 0xB8, 0x00 }, /* B8 ADC Biquad Coefficient A1 */
	{ 0xB9, 0x00 }, /* B9 ADC Biquad Coefficient A1 */
	{ 0xBA, 0x00 }, /* BA ADC Biquad Coefficient A1 */
	{ 0xBB, 0x00 }, /* BB ADC Biquad Coefficient A2 */
	{ 0xBC, 0x00 }, /* BC ADC Biquad Coefficient A2 */
	{ 0xBD, 0x00 }, /* BD ADC Biquad Coefficient A2 */
	{ 0xBE, 0x00 }, /* BE */
	{ 0xBF, 0x00 }, /* BF */

	{ 0xC0, 0x00 }, /* C0 */
	{ 0xC1, 0x00 }, /* C1 */
	{ 0xC2, 0x00 }, /* C2 */
	{ 0xC3, 0x00 }, /* C3 */
	{ 0xC4, 0x00 }, /* C4 */
	{ 0xC5, 0x00 }, /* C5 */
	{ 0xC6, 0x00 }, /* C6 */
	{ 0xC7, 0x00 }, /* C7 */
	{ 0xC8, 0x00 }, /* C8 */
	{ 0xC9, 0x00 }, /* C9 */
	{ 0xCA, 0x00 }, /* CA */
	{ 0xCB, 0x00 }, /* CB */
	{ 0xCC, 0x00 }, /* CC */
	{ 0xCD, 0x00 }, /* CD */
	{ 0xCE, 0x00 }, /* CE */
	{ 0xCF, 0x00 }, /* CF */

	{ 0xD0, 0x00 }, /* D0 */
	{ 0xD1, 0x00 }, /* D1 */
	{ 0xD2, 0x00 }, /* D2 */
	{ 0xD3, 0x00 }, /* D3 */
	{ 0xD4, 0x00 }, /* D4 */
	{ 0xD5, 0x00 }, /* D5 */
	{ 0xD6, 0x00 }, /* D6 */
	{ 0xD7, 0x00 }, /* D7 */
	{ 0xD8, 0x00 }, /* D8 */
	{ 0xD9, 0x00 }, /* D9 */
	{ 0xDA, 0x00 }, /* DA */
	{ 0xDB, 0x00 }, /* DB */
	{ 0xDC, 0x00 }, /* DC */
	{ 0xDD, 0x00 }, /* DD */
	{ 0xDE, 0x00 }, /* DE */
	{ 0xDF, 0x00 }, /* DF */

	{ 0xE0, 0x00 }, /* E0 */
	{ 0xE1, 0x00 }, /* E1 */
	{ 0xE2, 0x00 }, /* E2 */
	{ 0xE3, 0x00 }, /* E3 */
	{ 0xE4, 0x00 }, /* E4 */
	{ 0xE5, 0x00 }, /* E5 */
	{ 0xE6, 0x00 }, /* E6 */
	{ 0xE7, 0x00 }, /* E7 */
	{ 0xE8, 0x00 }, /* E8 */
	{ 0xE9, 0x00 }, /* E9 */
	{ 0xEA, 0x00 }, /* EA */
	{ 0xEB, 0x00 }, /* EB */
	{ 0xEC, 0x00 }, /* EC */
	{ 0xED, 0x00 }, /* ED */
	{ 0xEE, 0x00 }, /* EE */
	{ 0xEF, 0x00 }, /* EF */

	{ 0xF0, 0x00 }, /* F0 */
	{ 0xF1, 0x00 }, /* F1 */
	{ 0xF2, 0x00 }, /* F2 */
	{ 0xF3, 0x00 }, /* F3 */
	{ 0xF4, 0x00 }, /* F4 */
	{ 0xF5, 0x00 }, /* F5 */
	{ 0xF6, 0x00 }, /* F6 */
	{ 0xF7, 0x00 }, /* F7 */
	{ 0xF8, 0x00 }, /* F8 */
	{ 0xF9, 0x00 }, /* F9 */
	{ 0xFA, 0x00 }, /* FA */
	{ 0xFB, 0x00 }, /* FB */
	{ 0xFC, 0x00 }, /* FC */
	{ 0xFD, 0x00 }, /* FD */
	{ 0xFE, 0x00 }, /* FE */
};

static bool max98090_volatile_register(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case M98090_REG_01_IRQ_STATUS:
	case M98090_REG_02_JACK_STATUS:
	case M98090_REG_FF_REV_ID:
		return true;
	default:
		return false;
	}
}

static bool max98090_readable_register(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case M98090_REG_01_IRQ_STATUS:
	case M98090_REG_02_JACK_STATUS:
	case M98090_REG_03_IRQ_ENABLE:
	case M98090_REG_0C_RESERVED:
	case M98090_REG_0D_CFG_INPUT:
	case M98090_REG_0E_LVL_LINE:
	case M98090_REG_0F_CFG_LINE:
	case M98090_REG_10_LVL_MIC1:
	case M98090_REG_11_LVL_MIC2:
	case M98090_REG_12_MIC_BIAS:
	case M98090_REG_13_MIC_CFG1:
	case M98090_REG_14_MIC_CFG2:
	case M98090_REG_15_MIX_ADC_L:
	case M98090_REG_16_MIX_ADC_R:
	case M98090_REG_17_LVL_ADC_L:
	case M98090_REG_18_LVL_ADC_R:
	case M98090_REG_19_LVL_BIQUAD:
	case M98090_REG_1A_LVL_SIDETONE:
	case M98090_REG_1B_SYS_CLOCK:
	case M98090_REG_1C_CLOCK_MODE:
	case M98090_REG_1D_CLOCK_DAI1_NI_HI:
	case M98090_REG_1E_CLOCK_DAI2_NI_LO:
	case M98090_REG_1F_CLOCK_DAI3_MI_HI:
	case M98090_REG_20_CLOCK_DAI4_MI_LO:
	case M98090_REG_21_CLOCK_MAS_MODE:
	case M98090_REG_22_DAI_INTERFACE_FORMAT:
	case M98090_REG_23_DAI_TDM_CONTROL:
	case M98090_REG_24_DAI_TDM_FORMAT:
	case M98090_REG_25_DAI_IOCFG:
	case M98090_REG_26_DAI_FILTERS:
	case M98090_REG_27_DAI_LVL:
	case M98090_REG_28_DAI_LVL_EQ:
	case M98090_REG_29_MIX_HP_LEFT:
	case M98090_REG_2A_MIX_HP_RIGHT:
	case M98090_REG_2B_MIX_HP_CNTL:
	case M98090_REG_2C_LVL_HP_LEFT:
	case M98090_REG_2D_LVL_HP_RIGHT:
	case M98090_REG_2E_MIX_SPK_LEFT:
	case M98090_REG_2F_MIX_SPK_RIGHT:
	case M98090_REG_30_MIX_SPK_CNTL:
	case M98090_REG_31_LVL_SPK_LEFT:
	case M98090_REG_32_LVL_SPK_RIGHT:
	case M98090_REG_33_ALC_TIMING:
	case M98090_REG_34_ALC_CMPR:
	case M98090_REG_35_ALC_EXP:
	case M98090_REG_36_LVL_ALC:
	case M98090_REG_37_MIX_RCV_LEFT:
	case M98090_REG_38_MIX_RCV_CTRL_LEFT:
	case M98090_REG_39_LVL_RCV_LEFT:
	case M98090_REG_3A_MIX_RCV_RIGHT:
	case M98090_REG_3B_MIX_RCV_CNTL_RIGHT:
	case M98090_REG_3C_LVL_RCV_RIGHT:
	case M98090_REG_3D_CFG_JACK:
	case M98090_REG_3E_PWR_EN_IN:
	case M98090_REG_3F_PWR_EN_OUT:
	case M98090_REG_40_CFG_LVL:
	case M98090_REG_41_DSP_EQ_EN:
	case M98090_REG_42_BIAS_CNTL:
	case M98090_REG_43_DAC_CFG:
	case M98090_REG_44_ADC_CFG:
	case M98090_REG_45_PWR_SYS:
	case M98090_REG_46_EQ_BASE ... M98090_REG_46_EQ_BASE + 0x68:
	case M98090_REG_AF_BIQUAD_BASE ... M98090_REG_AF_BIQUAD_BASE + 0x0E:
	case M98090_REG_BE_DMIC3_VOLUME:
	case M98090_REG_BF_DMIC4_VOLUME:
	case M98090_REG_C0_DMIC34_BQ_PREATTEN:
	case M98090_REG_C1_RECORD_TDM_SLOT:
	case M98090_REG_C2_SAMPLE_RATE:
	case M98090_REG_C3_DMIC34_BIQUAD_BASE ... M98090_REG_C3_DMIC34_BIQUAD_BASE + 0x0E:
		return true;
	default:
		return false;
	}
}

static int max98090_reset(struct max98090_priv *max98090)
{
	int ret;

	/* Reset the codec by writing to this write-only reset register */
	ret = regmap_write(max98090->regmap, M98090_REG_00_SW_RESET,
		M98090_SWRESET_MASK);
	if (ret < 0) {
		dev_err(max98090->codec->dev,
			"Failed to reset codec: %d\n", ret);
		return ret;
	}

	msleep(20);
	return ret;
}

static const unsigned int max98090_micboost_tlv[] = {
	TLV_DB_RANGE_HEAD(2),
	0, 1, TLV_DB_SCALE_ITEM(0, 2000, 0),
	2, 2, TLV_DB_SCALE_ITEM(3000, 0, 0),
};

static const DECLARE_TLV_DB_SCALE(max98090_mic_tlv, 0, 100, 0);

static const unsigned int max98090_lin_tlv[] = {
	TLV_DB_RANGE_HEAD(2),
	0, 3, TLV_DB_SCALE_ITEM(-600, 300, 0),
	4, 5, TLV_DB_SCALE_ITEM(1400, 600, 0),
};

static const DECLARE_TLV_DB_SCALE(max98090_adcboost_tlv, 0, 600, 0);
static const DECLARE_TLV_DB_SCALE(max98090_adc_tlv, -1200, 100, 0);

static const DECLARE_TLV_DB_SCALE(max98090_dvg_tlv, 0, 600, 0);
static const DECLARE_TLV_DB_SCALE(max98090_dv_tlv, -1500, 100, 0);

static const DECLARE_TLV_DB_SCALE(max98090_sidetone_tlv, -6050, 200, 0);

static const DECLARE_TLV_DB_SCALE(max98090_alc_tlv, -1500, 100, 0);
static const DECLARE_TLV_DB_SCALE(max98090_alcmakeup_tlv, 0, 100, 0);
static const DECLARE_TLV_DB_SCALE(max98090_alccomp_tlv, -3100, 100, 0);
static const DECLARE_TLV_DB_SCALE(max98090_alcexp_tlv, -6600, 100, 0);

static const unsigned int max98090_mixout_tlv[] = {
	TLV_DB_RANGE_HEAD(2),
	0, 1, TLV_DB_SCALE_ITEM(-1200, 250, 0),
	2, 3, TLV_DB_SCALE_ITEM(-600, 600, 0),
};

static const unsigned int max98090_hp_tlv[] = {
	TLV_DB_RANGE_HEAD(5),
	0, 6, TLV_DB_SCALE_ITEM(-6700, 400, 0),
	7, 14, TLV_DB_SCALE_ITEM(-4000, 300, 0),
	15, 21, TLV_DB_SCALE_ITEM(-1700, 200, 0),
	22, 27, TLV_DB_SCALE_ITEM(-400, 100, 0),
	28, 31, TLV_DB_SCALE_ITEM(150, 50, 0),
};

static const unsigned int max98090_spk_tlv[] = {
	TLV_DB_RANGE_HEAD(5),
	0, 4, TLV_DB_SCALE_ITEM(-4800, 400, 0),
	5, 10, TLV_DB_SCALE_ITEM(-2900, 300, 0),
	11, 14, TLV_DB_SCALE_ITEM(-1200, 200, 0),
	15, 29, TLV_DB_SCALE_ITEM(-500, 100, 0),
	30, 39, TLV_DB_SCALE_ITEM(950, 50, 0),
};

static const unsigned int max98090_rcv_lout_tlv[] = {
	TLV_DB_RANGE_HEAD(5),
	0, 6, TLV_DB_SCALE_ITEM(-6200, 400, 0),
	7, 14, TLV_DB_SCALE_ITEM(-3500, 300, 0),
	15, 21, TLV_DB_SCALE_ITEM(-1200, 200, 0),
	22, 27, TLV_DB_SCALE_ITEM(100, 100, 0),
	28, 31, TLV_DB_SCALE_ITEM(650, 50, 0),
};

static int max98090_get_enab_tlv(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct max98090_priv *max98090 = snd_soc_codec_get_drvdata(codec);
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	unsigned int mask = (1 << fls(mc->max)) - 1;
	unsigned int val = snd_soc_read(codec, mc->reg);
	unsigned int *select;

	switch (mc->reg) {
	case M98090_REG_10_LVL_MIC1:
		select = &(max98090->mic1pre);
		break;
	case M98090_REG_11_LVL_MIC2:
		select = &(max98090->mic2pre);
		break;
	case M98090_REG_1A_LVL_SIDETONE:
		select = &(max98090->sidetone);
		break;
	default:
		return -EINVAL;
	}

	val = (val >> mc->shift) & mask;

	if (val >= 1) {
		/* If on, return the volume */
		val = val - 1;
		*select = val;
	} else {
		/* If off, return last stored value */
		val = *select;
	}

	ucontrol->value.integer.value[0] = val;
	return 0;
}

static int max98090_put_enab_tlv(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct max98090_priv *max98090 = snd_soc_codec_get_drvdata(codec);
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	unsigned int max = mc->max;
	unsigned int mask = (1 << fls(mc->max)) - 1;
	unsigned int sel = ucontrol->value.integer.value[0];
	unsigned int val = snd_soc_read(codec, mc->reg);
	unsigned int *select;

	switch (mc->reg) {
	case M98090_REG_10_LVL_MIC1:
		select = &(max98090->mic1pre);
		break;
	case M98090_REG_11_LVL_MIC2:
		select = &(max98090->mic2pre);
		break;
	case M98090_REG_1A_LVL_SIDETONE:
		select = &(max98090->sidetone);
		break;
	default:
		return -EINVAL;
	}

	val = (val >> mc->shift) & mask;

	if (sel > max)
		*select = max;
	else
		*select = sel;

	/* Setting a volume is only valid if it is already On */
	if (val >= 1) {
		sel = sel + 1;
		snd_soc_update_bits(codec, mc->reg, mask << mc->shift,
				sel << mc->shift);
	}

	return 0;
}

static const char * max98090_mixg_text[] = { "Normal", "6dB" };

static const struct soc_enum max98090_mixg135_enum =
	SOC_ENUM_SINGLE(M98090_REG_0E_LVL_LINE, M98090_MIXG135_SHIFT,
		ARRAY_SIZE(max98090_mixg_text), max98090_mixg_text);

static const struct soc_enum max98090_mixg246_enum =
	SOC_ENUM_SINGLE(M98090_REG_0E_LVL_LINE, M98090_MIXT246_SHIFT,
		ARRAY_SIZE(max98090_mixg_text), max98090_mixg_text);
	
static const char * max98090_osr128_text[] = { "64*fs", "128*fs" };

static const struct soc_enum max98090_osr128_enum =
	SOC_ENUM_SINGLE(M98090_REG_44_ADC_CFG, M98090_ADC_OSR128_SHIFT,
		ARRAY_SIZE(max98090_osr128_text), max98090_osr128_text);

static const char * max98090_enableddisabled_text[] =
	{ "Disabled", "Enabled" };
static const char * max98090_enableddisabled_inv_text[] =
	{ "Enabled", "Disabled" };
/* Note: Inverted Logic (0 = Enabled) */

static const struct soc_enum max98090_eqclpn_enum =
	SOC_ENUM_SINGLE(M98090_REG_28_DAI_LVL_EQ, M98090_DAI_LVL_EQCLPN_SHIFT,
		ARRAY_SIZE(max98090_enableddisabled_inv_text),
		max98090_enableddisabled_inv_text);
	
static const struct soc_enum max98090_sdien_enum =
	SOC_ENUM_SINGLE(M98090_REG_25_DAI_IOCFG, M98090_DAI_SDIEN_SHIFT,
		ARRAY_SIZE(max98090_enableddisabled_text),
		max98090_enableddisabled_text);

static const struct soc_enum max98090_sdoen_enum =
	SOC_ENUM_SINGLE(M98090_REG_25_DAI_IOCFG, M98090_DAI_SDOEN_SHIFT,
		ARRAY_SIZE(max98090_enableddisabled_text),
		max98090_enableddisabled_text);	
	
static const struct soc_enum max98090_hizoff_enum =
	SOC_ENUM_SINGLE(M98090_REG_25_DAI_IOCFG, M98090_DAI_HIZOFF_SHIFT,
		ARRAY_SIZE(max98090_enableddisabled_inv_text),
		max98090_enableddisabled_inv_text);
	
static const char *max98090_fltr_mode_text[] = { "Voice", "Music" };

static const struct soc_enum max98090_filter_mode_enum =
	SOC_ENUM_SINGLE(M98090_REG_26_DAI_FILTERS, M98090_DAI_FLT_MODE_SHIFT,
		ARRAY_SIZE(max98090_fltr_mode_text), max98090_fltr_mode_text);
	
static const struct soc_enum max98090_filter_dmic34mode_enum =
	SOC_ENUM_SINGLE(M98090_REG_26_DAI_FILTERS,
		M98090_DAI_FLT_DMIC34MODE_SHIFT,
		ARRAY_SIZE(max98090_fltr_mode_text), max98090_fltr_mode_text);

static const char * max98090_dsts_text[] =
	{ "None", "Left", "Right", "Left and Right" };

static const struct soc_enum max98090_dsts_enum =
	SOC_ENUM_SINGLE(M98090_REG_1A_LVL_SIDETONE, M98090_DSTS_SHIFT,
		ARRAY_SIZE(max98090_dsts_text), max98090_dsts_text);
	
static const char * const max98090_alcatk_text[] = {
	"0.125ms", "0.25ms", "1.25ms", "2.5ms",
	"6.25ms",  "12.5ms", "25ms",   "50ms"
};

static const struct soc_enum max98090_alcatk_enum =
	SOC_ENUM_SINGLE(M98090_REG_33_ALC_TIMING, M98090_ALCATK_SHIFT,
		ARRAY_SIZE(max98090_alcatk_text), max98090_alcatk_text);
	
static const char * const max98090_alcrls_text[] = {
	"8s", "4s", "2s", "1s", "0.5s", "0.25s", "0.125s", "0.0625s"
};

static const struct soc_enum max98090_alcrls_enum =
	SOC_ENUM_SINGLE(M98090_REG_33_ALC_TIMING, M98090_ALCRLS_SHIFT,
		ARRAY_SIZE(max98090_alcrls_text), max98090_alcrls_text);

static const char * const max98090_alccmp_text[] = {
	"1:1", "1.5:1", "2:1", "4:1", "INF:1"
};

static const struct soc_enum max98090_alccmp_enum =
	SOC_ENUM_SINGLE(M98090_REG_34_ALC_CMPR, M98090_ALCCMP_SHIFT,
		ARRAY_SIZE(max98090_alccmp_text), max98090_alccmp_text);

static const char * const max98090_alcexp_text[] = { "1:1", "1:2", "1:3" };

static const struct soc_enum max98090_alcexp_enum =
	SOC_ENUM_SINGLE(M98090_REG_35_ALC_EXP, M98090_ALCEXP_SHIFT,
		ARRAY_SIZE(max98090_alcexp_text), max98090_alcexp_text);
	
static const struct soc_enum max98090_zdenn_enum =
	SOC_ENUM_SINGLE(M98090_REG_40_CFG_LVL, M98090_ZDENN_SHIFT,
		ARRAY_SIZE(max98090_enableddisabled_inv_text),
		max98090_enableddisabled_inv_text);

static const struct soc_enum max98090_vs2enn_enum =
	SOC_ENUM_SINGLE(M98090_REG_40_CFG_LVL, M98090_VS2ENN_SHIFT,
		ARRAY_SIZE(max98090_enableddisabled_inv_text),
		max98090_enableddisabled_inv_text);

static const struct soc_enum max98090_vsenn_enum =
	SOC_ENUM_SINGLE(M98090_REG_40_CFG_LVL, M98090_VSENN_SHIFT,
		ARRAY_SIZE(max98090_enableddisabled_inv_text),
		max98090_enableddisabled_inv_text);
	
static const struct snd_kcontrol_new max98090_snd_controls[] = {
	SOC_SINGLE("MIC Bias VCM Bandgap", M98090_REG_42_BIAS_CNTL,
		M98090_VCM_MODE_SHIFT, M98090_VCM_MODE_NUM - 1, 0),
	
	SOC_SINGLE("DMIC MIC Comp Filter Config", M98090_REG_14_MIC_CFG2,
		M98090_DMIC_COMP_SHIFT, M98090_DMIC_COMP_NUM - 1, 0),

	SOC_SINGLE("DMIC MIC Left Enable", M98090_REG_13_MIC_CFG1,
		M98090_DIGMICL_SHIFT, M98090_DIGMICL_NUM - 1, 0),
	SOC_SINGLE("DMIC MIC Right Enable", M98090_REG_13_MIC_CFG1,
		M98090_DIGMICR_SHIFT, M98090_DIGMICR_NUM - 1, 0),

	SOC_SINGLE_EXT_TLV("MIC1 Boost Volume",
		M98090_REG_10_LVL_MIC1, M98090_MIC_PA1EN_SHIFT,
		M98090_MIC_PA1EN_NUM - 1, 0, max98090_get_enab_tlv,
		max98090_put_enab_tlv, max98090_micboost_tlv),

	SOC_SINGLE_EXT_TLV("MIC2 Boost Volume",
		M98090_REG_11_LVL_MIC2, M98090_MIC_PA2EN_SHIFT,
		M98090_MIC_PA2EN_NUM - 1, 0, max98090_get_enab_tlv,
		max98090_put_enab_tlv, max98090_micboost_tlv),

	SOC_SINGLE_TLV("MIC1 Volume", M98090_REG_10_LVL_MIC1,
		M98090_MIC_PGAM1_SHIFT, M98090_MIC_PGAM1_NUM - 1, 1,
		max98090_mic_tlv),

	SOC_SINGLE_TLV("MIC2 Volume", M98090_REG_11_LVL_MIC2,
		M98090_MIC_PGAM2_SHIFT, M98090_MIC_PGAM2_NUM - 1, 1,
		max98090_mic_tlv),

	SOC_ENUM("LINEA Single Ended Reduction", max98090_mixg135_enum),
	SOC_ENUM("LINEB Single Ended Reduction", max98090_mixg246_enum),

	SOC_SINGLE_RANGE_TLV("LINEA Volume", M98090_REG_0E_LVL_LINE,
		M98090_LINAPGA_SHIFT, 0, M98090_LINAPGA_NUM - 1, 1,
		max98090_lin_tlv),

	SOC_SINGLE_RANGE_TLV("LINEB Volume", M98090_REG_0E_LVL_LINE,
		M98090_LINBPGA_SHIFT, 0, M98090_LINBPGA_NUM - 1, 1,
		max98090_lin_tlv),

	SOC_SINGLE("LINEA Ext Resistor Gain Mode", M98090_REG_0F_CFG_LINE,
		M98090_EXTBUFA_SHIFT, M98090_EXTBUFA_NUM - 1, 0),
	SOC_SINGLE("LINEB Ext Resistor Gain Mode", M98090_REG_0F_CFG_LINE,
		M98090_EXTBUFB_SHIFT, M98090_EXTBUFB_NUM - 1, 0),
					   
	SOC_SINGLE_TLV("ADCL Boost Volume", M98090_REG_17_LVL_ADC_L,
		M98090_ADC_AVLG_SHIFT, M98090_ADC_AVLG_NUM - 1, 0,
		max98090_adcboost_tlv),
	SOC_SINGLE_TLV("ADCR Boost Volume", M98090_REG_18_LVL_ADC_R,
		M98090_ADC_AVRG_SHIFT, M98090_ADC_AVLG_NUM - 1, 0,
		max98090_adcboost_tlv),

	SOC_SINGLE_TLV("ADCL Volume", M98090_REG_17_LVL_ADC_L,
		M98090_ADC_AVL_SHIFT, M98090_ADC_AVL_NUM - 1, 1,
		max98090_adc_tlv),
	SOC_SINGLE_TLV("ADCR Volume", M98090_REG_18_LVL_ADC_R,
		M98090_ADC_AVR_SHIFT, M98090_ADC_AVR_NUM - 1, 1,
		max98090_adc_tlv),

	SOC_ENUM("ADC Oversampling Rate", max98090_osr128_enum),
	SOC_SINGLE("ADC Quantizer Dither", M98090_REG_44_ADC_CFG,
		M98090_ADCDITHER_SHIFT, M98090_ADCDITHER_NUM - 1, 0),
	SOC_SINGLE("ADC High Performance Mode", M98090_REG_44_ADC_CFG,
		M98090_ADCHP_SHIFT, M98090_ADCHP_NUM - 1, 0),
				   
	SOC_SINGLE("DAC Mono Mode", M98090_REG_25_DAI_IOCFG,
		M98090_DAI_DMONO_SHIFT, M98090_DAI_DMONO_NUM - 1, 0),
	SOC_ENUM("SDIN Mode", max98090_sdien_enum),
	SOC_ENUM("SDOUT Mode", max98090_sdoen_enum),
	SOC_ENUM("SDOUT Hi-Z Mode", max98090_hizoff_enum),
	SOC_ENUM("Filter Mode", max98090_filter_mode_enum),
	SOC_SINGLE("Record Path DC Blocking", M98090_REG_26_DAI_FILTERS,
		M98090_DAI_FLT_AHPF_SHIFT, M98090_DAI_FLT_AHPF_NUM - 1, 0),
	SOC_SINGLE("Playback Path DC Blocking", M98090_REG_26_DAI_FILTERS,
		M98090_DAI_FLT_DHPF_SHIFT, M98090_DAI_FLT_DHPF_NUM - 1, 0),
	SOC_SINGLE_TLV("Digital BQ Level Volume", M98090_REG_19_LVL_BIQUAD,
		M98090_AVBQ_SHIFT, M98090_AVBQ_NUM - 1, 1, max98090_dv_tlv),
	SOC_ENUM("Digital Sidetone Source", max98090_dsts_enum),
	SOC_SINGLE_EXT_TLV("Digital Sidetone Level Volume",
		M98090_REG_1A_LVL_SIDETONE, M98090_DVST_SHIFT,
		M98090_DVST_NUM - 1, 1, max98090_get_enab_tlv,
		max98090_put_enab_tlv, max98090_micboost_tlv),
	SOC_SINGLE_TLV("Digital Gain Volume", M98090_REG_27_DAI_LVL,
		M98090_DAI_LVL_DVG_SHIFT, M98090_DAI_LVL_DVG_NUM - 1, 0,
		max98090_dvg_tlv),
	SOC_SINGLE_TLV("Digital Level Volume", M98090_REG_27_DAI_LVL,
		M98090_DAI_LVL_DV_SHIFT, M98090_DAI_LVL_DV_NUM - 1, 1,
		max98090_dv_tlv),
	SOC_SINGLE("Digital EQ 3 Band Enable", M98090_REG_41_DSP_EQ_EN,
		M98090_EQ3BANDEN_SHIFT, M98090_EQ3BANDEN_NUM - 1, 0),
	SOC_SINGLE("Digital EQ 5 Band Enable", M98090_REG_41_DSP_EQ_EN,
		M98090_EQ5BANDEN_SHIFT, M98090_EQ5BANDEN_NUM - 1, 0),
	SOC_SINGLE("Digital EQ 7 Band Enable", M98090_REG_41_DSP_EQ_EN,
		M98090_EQ7BANDEN_SHIFT, M98090_EQ7BANDEN_NUM - 1, 0),
	SOC_ENUM("Digital EQ Clipping Detection", max98090_eqclpn_enum),
	SOC_SINGLE_TLV("Digital EQ Level Volume", M98090_REG_28_DAI_LVL_EQ,
		M98090_DAI_LVL_DVEQ_SHIFT, M98090_DAI_LVL_DVEQ_NUM - 1, 1,
		max98090_dv_tlv),

	SOC_SINGLE("ALC Enable", M98090_REG_33_ALC_TIMING,
		M98090_ALCEN_SHIFT, M98090_ALCEN_NUM - 1, 0),
	SOC_ENUM("ALC Attack Time", max98090_alcatk_enum),
	SOC_ENUM("ALC Release Time", max98090_alcrls_enum),
	SOC_SINGLE_TLV("ALC Make Up Gain Volume", M98090_REG_36_LVL_ALC,
		M98090_ALCG_SHIFT, M98090_ALCG_NUM - 1, 0,
		max98090_alcmakeup_tlv),
	SOC_ENUM("ALC Compression Ratio", max98090_alccmp_enum),
	SOC_ENUM("ALC Expansion Ratio", max98090_alcexp_enum),
	SOC_SINGLE_TLV("ALC Compression Threshold Volume",
		M98090_REG_34_ALC_CMPR, M98090_ALCTHC_SHIFT,
		M98090_ALCTHC_NUM - 1, 1, max98090_alccomp_tlv),
	SOC_SINGLE_TLV("ALC Expansion Threshold Volume",
		M98090_REG_35_ALC_EXP, M98090_ALCTHE_SHIFT,
		M98090_ALCTHE_NUM - 1, 1, max98090_alcexp_tlv),

	SOC_SINGLE("DAC HP Playback Performance Mode", M98090_REG_43_DAC_CFG,
		M98090_DAC_PERFMODE_SHIFT, M98090_DAC_PERFMODE_NUM - 1, 0),
	SOC_SINGLE("DAC High Performance Mode", M98090_REG_43_DAC_CFG,
		M98090_DACHP_SHIFT, M98090_DACHP_NUM - 1, 0),
				   
	SOC_SINGLE_TLV("Headphone Left Mixer Gain Volume",
		M98090_REG_2B_MIX_HP_CNTL, M98090_MIXHPLG_SHIFT,
		M98090_MIXHPLG_NUM - 1, 1, max98090_mixout_tlv),
	SOC_SINGLE_TLV("Headphone Right Mixer Gain Volume",
		M98090_REG_2B_MIX_HP_CNTL, M98090_MIXHPRG_SHIFT,
		M98090_MIXHPRG_NUM - 1, 1, max98090_mixout_tlv),

	SOC_SINGLE_TLV("Speaker Left Mixer Gain Volume",
		M98090_REG_30_MIX_SPK_CNTL, M98090_MIXSPLG_SHIFT,
		M98090_MIXSPLG_NUM - 1, 1, max98090_mixout_tlv),
	SOC_SINGLE_TLV("Speaker Right Mixer Gain Volume",
		M98090_REG_30_MIX_SPK_CNTL, M98090_MIXSPRG_SHIFT,
		M98090_MIXSPRG_NUM - 1, 1, max98090_mixout_tlv),

	SOC_SINGLE_TLV("Receiver Left Mixer Gain Volume",
		M98090_REG_38_MIX_RCV_CTRL_LEFT, M98090_MIXRCVLG_SHIFT,
		M98090_MIXRCVLG_NUM - 1, 1, max98090_mixout_tlv),
	SOC_SINGLE_TLV("Receiver Right Mixer Gain Volume",
		M98090_REG_3B_MIX_RCV_CNTL_RIGHT, M98090_MIXRCVRG_SHIFT,
		M98090_MIXRCVRG_NUM - 1, 1, max98090_mixout_tlv),

	SOC_SINGLE_TLV("Headphone Left Volume", M98090_REG_2C_LVL_HP_LEFT,
		M98090_HPVOLL_SHIFT, M98090_HPVOLL_NUM - 1, 0,
		max98090_hp_tlv),
	SOC_SINGLE_TLV("Headphone Right Volume", M98090_REG_2D_LVL_HP_RIGHT,
		M98090_HPVOLR_SHIFT, M98090_HPVOLR_NUM - 1, 0,
		max98090_hp_tlv),

	SOC_SINGLE_RANGE_TLV("Speaker Left Volume",
		M98090_REG_31_LVL_SPK_LEFT, M98090_SPVOLL_SHIFT, 24,
		M98090_SPVOLL_NUM - 1 + 24, 0, max98090_spk_tlv),
	SOC_SINGLE_RANGE_TLV("Speaker Right Volume",
		M98090_REG_32_LVL_SPK_RIGHT, M98090_SPVOLR_SHIFT, 24,
		M98090_SPVOLR_NUM - 1 + 24, 0, max98090_spk_tlv),

	SOC_SINGLE_TLV("Receiver Left Volume", M98090_REG_39_LVL_RCV_LEFT,
		M98090_RCVLVOL_SHIFT, M98090_RCVLVOL_NUM - 1, 0,
		max98090_rcv_lout_tlv),
	SOC_SINGLE_TLV("Receiver Right Volume", M98090_REG_3C_LVL_RCV_RIGHT,
		M98090_RCVRVOL_SHIFT, M98090_RCVRVOL_NUM - 1, 0,
		max98090_rcv_lout_tlv),

	SOC_SINGLE("Headphone Left Switch", M98090_REG_2C_LVL_HP_LEFT,
		M98090_HPLM_SHIFT, 1, 1),
	SOC_SINGLE("Headphone Right Switch", M98090_REG_2D_LVL_HP_RIGHT,
		M98090_HPRM_SHIFT, 1, 1),

	SOC_SINGLE("Speaker Left Switch", M98090_REG_31_LVL_SPK_LEFT,
		M98090_SPLM_SHIFT, 1, 1),
	SOC_SINGLE("Speaker Right Switch", M98090_REG_32_LVL_SPK_RIGHT,
		M98090_SPRM_SHIFT, 1, 1),

	SOC_SINGLE("Receiver Left Switch", M98090_REG_39_LVL_RCV_LEFT,
		M98090_RCVLM_SHIFT, 1, 1),
	SOC_SINGLE("Receiver Right Switch", M98090_REG_3C_LVL_RCV_RIGHT,
		M98090_RCVRM_SHIFT, 1, 1),

	SOC_ENUM("Zero-Crossing Detection", max98090_zdenn_enum),
	SOC_ENUM("Enhanced Volume Smoothing", max98090_vs2enn_enum),
	SOC_ENUM("Volume Adjustment Smoothing", max98090_vsenn_enum),

	SOC_SINGLE("Quick Setup System Clock", M98090_REG_04_QCFG_SYS_CLK,
		M98090_CLK_ALL_SHIFT, M98090_CLK_ALL_NUM - 1, 0),
	SOC_SINGLE("Quick Setup Sample Rate", M98090_REG_05_QCFG_RATE,
		M98090_SR_ALL_SHIFT, M98090_SR_ALL_NUM - 1, 0),
	SOC_SINGLE("Quick Setup DAI Interface", M98090_REG_06_QCFG_DAI,
		M98090_DAI_ALL_SHIFT, M98090_DAI_ALL_NUM - 1, 0),
	SOC_SINGLE("Quick Setup DAC Path", M98090_REG_07_QCFG_DAC,
		M98090_DIG2_ALL_SHIFT, M98090_DIG2_ALL_NUM - 1, 0),
	SOC_SINGLE("Quick Setup MIC/Direct ADC", M98090_REG_08_QCFG_MIC_PATH,
		M98090_MIC_ALL_SHIFT, M98090_MIC_ALL_NUM - 1, 0),
	SOC_SINGLE("Quick Setup Line ADC", M98090_REG_09_QCFG_LINE_PATH,
		M98090_LINE_ALL_SHIFT, M98090_LINE_ALL_NUM - 1, 0),
	SOC_SINGLE("Quick Setup Analog MIC Loop", M98090_REG_0A_QCFG_MIC_LOOP,
		M98090_AMIC_ALL_SHIFT, M98090_AMIC_ALL_NUM - 1, 0),
	SOC_SINGLE("Quick Setup Analog Line Loop",
		M98090_REG_0B_QCFG_LINE_LOOP, M98090_ALIN_ALL_SHIFT,
		M98090_ALIN_ALL_NUM - 1, 0),

	SOC_SINGLE("Biquad Switch", M98090_REG_41_DSP_EQ_EN,
		M98090_ADCBQEN_SHIFT, M98090_ADCBQEN_NUM - 1, 0),
};

static const struct snd_kcontrol_new max98091_snd_controls[] = {
	SOC_SINGLE("DMIC MIC3 Enable", M98090_REG_13_MIC_CFG1,
		M98090_DIGMIC3_SHIFT, M98090_DIGMIC3_NUM - 1, 0),
	SOC_SINGLE("DMIC MIC4 Enable", M98090_REG_13_MIC_CFG1,
		M98090_DIGMIC4_SHIFT, M98090_DIGMIC4_NUM - 1, 0),

	SOC_SINGLE("DMIC34 Zeropad", M98090_REG_C2_SAMPLE_RATE,
		M98090_DMIC34_ZEROPAD_SHIFT,
		M98090_DMIC34_ZEROPAD_NUM - 1, 0),

	SOC_ENUM("Filter DMIC34 Mode", max98090_filter_dmic34mode_enum),
	SOC_SINGLE("DMIC34 DC Blocking", M98090_REG_26_DAI_FILTERS,
		M98090_DAI_FLT_DMIC34HPF_SHIFT,
		M98090_DAI_FLT_DMIC34HPF_NUM - 1, 0),

	SOC_SINGLE_TLV("DMIC3 Boost Volume", M98090_REG_BE_DMIC3_VOLUME,
		M98090_DMIC_AV3G_SHIFT, M98090_DMIC_AV3G_NUM - 1, 0,
		max98090_adcboost_tlv),
	SOC_SINGLE_TLV("DMIC4 Boost Volume", M98090_REG_BF_DMIC4_VOLUME,
		M98090_DMIC_AV4G_SHIFT, M98090_DMIC_AV4G_NUM - 1, 0,
		max98090_adcboost_tlv),

	SOC_SINGLE_TLV("DMIC3 Volume", M98090_REG_BE_DMIC3_VOLUME,
		M98090_DMIC_AV3_SHIFT, M98090_DMIC_AV3_NUM - 1, 1,
		max98090_adc_tlv),
	SOC_SINGLE_TLV("DMIC4 Volume", M98090_REG_BF_DMIC4_VOLUME,
		M98090_DMIC_AV4_SHIFT, M98090_DMIC_AV4_NUM - 1, 1,
		max98090_adc_tlv),

	SOC_SINGLE("DMIC34 Biquad Switch", M98090_REG_41_DSP_EQ_EN,
		M98090_DMIC34BQEN_SHIFT, M98090_DMIC34BQEN_NUM - 1, 0),

	SOC_SINGLE_TLV("DMIC34 BQ PreAttenuation Level Volume",
		M98090_REG_C0_DMIC34_BQ_PREATTEN, M98090_AV34BQ_SHIFT,
		M98090_AV34BQ_NUM - 1, 1, max98090_dv_tlv),

	SOC_SINGLE("Rev ID", M98090_REG_FF_REV_ID,
		M98090_REVID_SHIFT, M98090_REVID_NUM - 1, 0),
};

static int max98090_micinput_event(struct snd_soc_dapm_widget *w,
				 struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;
	struct max98090_priv *max98090 = snd_soc_codec_get_drvdata(codec);

	unsigned int val = snd_soc_read(codec, w->reg);

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		/* If turning on, set to most recently selected volume */
		if (w->reg == M98090_REG_10_LVL_MIC1)
			val = max98090->mic1pre + 1;
		else
			val = max98090->mic2pre + 1;
		break;
	case SND_SOC_DAPM_POST_PMD:
		/* If turning off, turn off */
		val = 0;
		break;
	default:
		return -EINVAL;
	}

	if (w->reg == M98090_REG_10_LVL_MIC1)
		snd_soc_update_bits(codec, w->reg, M98090_MIC_PA1EN_MASK,
			val << M98090_MIC_PA1EN_SHIFT);
	else
		snd_soc_update_bits(codec, w->reg, M98090_MIC_PA2EN_MASK,
			val << M98090_MIC_PA2EN_SHIFT);

	return 0;
}

static int max98090_sidetone_event(struct snd_soc_dapm_widget *w,
				 struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;
	struct max98090_priv *max98090 = snd_soc_codec_get_drvdata(codec);

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		snd_soc_update_bits(codec, w->reg, M98090_DVST_MASK,
			(1 + max98090->sidetone) << M98090_DVST_SHIFT);
		break;
	case SND_SOC_DAPM_POST_PMD:
		snd_soc_update_bits(codec, w->reg, M98090_DVST_MASK, 0);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static const char *mic1_mux_text[] = { "IN12", "IN56" };

static const struct soc_enum mic1_mux_enum =
	SOC_ENUM_SINGLE(M98090_REG_0F_CFG_LINE, M98090_EXTMIC1_SHIFT,
		ARRAY_SIZE(mic1_mux_text), mic1_mux_text);

static const struct snd_kcontrol_new max98090_mic1_mux =
	SOC_DAPM_ENUM("MIC1 Mux", mic1_mux_enum);

static const char *mic2_mux_text[] = { "IN34", "IN56" };

static const struct soc_enum mic2_mux_enum =
	SOC_ENUM_SINGLE(M98090_REG_0F_CFG_LINE, M98090_EXTMIC2_SHIFT,
		ARRAY_SIZE(mic2_mux_text), mic2_mux_text);

static const struct snd_kcontrol_new max98090_mic2_mux =
	SOC_DAPM_ENUM("MIC2 Mux", mic2_mux_enum);

static const char * max98090_micpre_text[] = { "Off", "On" };

static const struct soc_enum max98090_mic1pre_enum =
	SOC_ENUM_SINGLE(M98090_REG_10_LVL_MIC1, M98090_MIC_PA1EN_SHIFT,
		ARRAY_SIZE(max98090_micpre_text), max98090_micpre_text);

static const struct soc_enum max98090_mic2pre_enum =
	SOC_ENUM_SINGLE(M98090_REG_11_LVL_MIC2, M98090_MIC_PA2EN_SHIFT,
		ARRAY_SIZE(max98090_micpre_text), max98090_micpre_text);

/* LINEA mixer switch */
static const struct snd_kcontrol_new max98090_linea_mixer_controls[] = {
	SOC_DAPM_SINGLE("IN1 Switch", M98090_REG_0D_CFG_INPUT,
		M98090_IN1SEEN_SHIFT, 1, 0),
	SOC_DAPM_SINGLE("IN3 Switch", M98090_REG_0D_CFG_INPUT,
		M98090_IN3SEEN_SHIFT, 1, 0),
	SOC_DAPM_SINGLE("IN5 Switch", M98090_REG_0D_CFG_INPUT,
		M98090_IN5SEEN_SHIFT, 1, 0),
	SOC_DAPM_SINGLE("IN34 Switch", M98090_REG_0D_CFG_INPUT,
		M98090_IN34DIFF_SHIFT, 1, 0),
};

/* LINEB mixer switch */
static const struct snd_kcontrol_new max98090_lineb_mixer_controls[] = {
	SOC_DAPM_SINGLE("IN2 Switch", M98090_REG_0D_CFG_INPUT,
		M98090_IN2SEEN_SHIFT, 1, 0),
	SOC_DAPM_SINGLE("IN4 Switch", M98090_REG_0D_CFG_INPUT,
		M98090_IN4SEEN_SHIFT, 1, 0),
	SOC_DAPM_SINGLE("IN6 Switch", M98090_REG_0D_CFG_INPUT,
		M98090_IN6SEEN_SHIFT, 1, 0),
	SOC_DAPM_SINGLE("IN56 Switch", M98090_REG_0D_CFG_INPUT,
		M98090_IN56DIFF_SHIFT, 1, 0),
};

/* Left ADC mixer switch */
static const struct snd_kcontrol_new max98090_left_adc_mixer_controls[] = {
	SOC_DAPM_SINGLE("IN12 Switch", M98090_REG_15_MIX_ADC_L,
		M98090_MIXADL_IN12DIFF_SHIFT, 1, 0),
	SOC_DAPM_SINGLE("IN34 Switch", M98090_REG_15_MIX_ADC_L,
		M98090_MIXADL_IN34DIFF_SHIFT, 1, 0),
	SOC_DAPM_SINGLE("IN56 Switch", M98090_REG_15_MIX_ADC_L,
		M98090_MIXADL_IN65DIFF_SHIFT, 1, 0),
	SOC_DAPM_SINGLE("LINEA Switch", M98090_REG_15_MIX_ADC_L,
		M98090_MIXADL_LINEA_SHIFT, 1, 0),
	SOC_DAPM_SINGLE("LINEB Switch", M98090_REG_15_MIX_ADC_L,
		M98090_MIXADL_LINEB_SHIFT, 1, 0),
	SOC_DAPM_SINGLE("MIC1 Switch", M98090_REG_15_MIX_ADC_L,
		M98090_MIXADL_MIC1_SHIFT, 1, 0),
	SOC_DAPM_SINGLE("MIC2 Switch", M98090_REG_15_MIX_ADC_L,
		M98090_MIXADL_MIC2_SHIFT, 1, 0),
};

/* Right ADC mixer switch */
static const struct snd_kcontrol_new max98090_right_adc_mixer_controls[] = {
	SOC_DAPM_SINGLE("IN12 Switch", M98090_REG_16_MIX_ADC_R,
		M98090_MIXADR_IN12DIFF_SHIFT, 1, 0),
	SOC_DAPM_SINGLE("IN34 Switch", M98090_REG_16_MIX_ADC_R,
		M98090_MIXADR_IN34DIFF_SHIFT, 1, 0),
	SOC_DAPM_SINGLE("IN56 Switch", M98090_REG_16_MIX_ADC_R,
		M98090_MIXADR_IN65DIFF_SHIFT, 1, 0),
	SOC_DAPM_SINGLE("LINEA Switch", M98090_REG_16_MIX_ADC_R,
		M98090_MIXADR_LINEA_SHIFT, 1, 0),
	SOC_DAPM_SINGLE("LINEB Switch", M98090_REG_16_MIX_ADC_R,
		M98090_MIXADR_LINEB_SHIFT, 1, 0),
	SOC_DAPM_SINGLE("MIC1 Switch", M98090_REG_16_MIX_ADC_R,
		M98090_MIXADR_MIC1_SHIFT, 1, 0),
	SOC_DAPM_SINGLE("MIC2 Switch", M98090_REG_16_MIX_ADC_R,
		M98090_MIXADR_MIC2_SHIFT, 1, 0),
};

static const char *lten_mux_text[] = { "Normal", "Loopthrough" };

static const struct soc_enum ltenl_mux_enum =
	SOC_ENUM_SINGLE(M98090_REG_25_DAI_IOCFG, M98090_DAI_LTEN_SHIFT,
		ARRAY_SIZE(lten_mux_text), lten_mux_text);

static const struct soc_enum ltenr_mux_enum =
	SOC_ENUM_SINGLE(M98090_REG_25_DAI_IOCFG, M98090_DAI_LTEN_SHIFT,
		ARRAY_SIZE(lten_mux_text), lten_mux_text);

static const struct snd_kcontrol_new max98090_ltenl_mux =
	SOC_DAPM_ENUM("LTENL Mux", ltenl_mux_enum);

static const struct snd_kcontrol_new max98090_ltenr_mux =
	SOC_DAPM_ENUM("LTENR Mux", ltenr_mux_enum);
	
static const char *lben_mux_text[] = { "Normal", "Loopback" };

static const struct soc_enum lbenl_mux_enum =
	SOC_ENUM_SINGLE(M98090_REG_25_DAI_IOCFG, M98090_DAI_LBEN_SHIFT,
		ARRAY_SIZE(lben_mux_text), lben_mux_text);

static const struct soc_enum lbenr_mux_enum =
	SOC_ENUM_SINGLE(M98090_REG_25_DAI_IOCFG, M98090_DAI_LBEN_SHIFT,
		ARRAY_SIZE(lben_mux_text), lben_mux_text);

static const struct snd_kcontrol_new max98090_lbenl_mux =
	SOC_DAPM_ENUM("LBENL Mux", lbenl_mux_enum);

static const struct snd_kcontrol_new max98090_lbenr_mux =
	SOC_DAPM_ENUM("LBENR Mux", lbenr_mux_enum);
	
/* Left speaker mixer switch */
static const struct
	snd_kcontrol_new max98090_left_speaker_mixer_controls[] = {
	SOC_DAPM_SINGLE("Left DAC Switch", M98090_REG_2E_MIX_SPK_LEFT,
		M98090_MIXSPL_DACL_SHIFT, 1, 0),
	SOC_DAPM_SINGLE("Right DAC Switch", M98090_REG_2E_MIX_SPK_LEFT,
		M98090_MIXSPL_DACR_SHIFT, 1, 0),
	SOC_DAPM_SINGLE("LINEA Switch", M98090_REG_2E_MIX_SPK_LEFT,
		M98090_MIXSPL_LINEA_SHIFT, 1, 0),
	SOC_DAPM_SINGLE("LINEB Switch", M98090_REG_2E_MIX_SPK_LEFT,
		M98090_MIXSPL_LINEB_SHIFT, 1, 0),
	SOC_DAPM_SINGLE("MIC1 Switch", M98090_REG_2E_MIX_SPK_LEFT,
		M98090_MIXSPL_MIC1_SHIFT, 1, 0),
	SOC_DAPM_SINGLE("MIC2 Switch", M98090_REG_2E_MIX_SPK_LEFT,
		M98090_MIXSPL_MIC2_SHIFT, 1, 0),
};

/* Right speaker mixer switch */
static const struct
	snd_kcontrol_new max98090_right_speaker_mixer_controls[] = {
	SOC_DAPM_SINGLE("Left DAC Switch", M98090_REG_2F_MIX_SPK_RIGHT,
		M98090_MIXSPR_DACL_SHIFT, 1, 0),
	SOC_DAPM_SINGLE("Right DAC Switch", M98090_REG_2F_MIX_SPK_RIGHT,
		M98090_MIXSPR_DACR_SHIFT, 1, 0),
	SOC_DAPM_SINGLE("LINEA Switch", M98090_REG_2F_MIX_SPK_RIGHT,
		M98090_MIXSPR_LINEA_SHIFT, 1, 0),
	SOC_DAPM_SINGLE("LINEB Switch", M98090_REG_2F_MIX_SPK_RIGHT,
		M98090_MIXSPR_LINEB_SHIFT, 1, 0),
	SOC_DAPM_SINGLE("MIC1 Switch", M98090_REG_2F_MIX_SPK_RIGHT,
		M98090_MIXSPR_MIC1_SHIFT, 1, 0),
	SOC_DAPM_SINGLE("MIC2 Switch", M98090_REG_2F_MIX_SPK_RIGHT,
		M98090_MIXSPR_MIC2_SHIFT, 1, 0),
};

/* Left headphone mixer switch */
static const struct snd_kcontrol_new max98090_left_hp_mixer_controls[] = {
	SOC_DAPM_SINGLE("Left DAC Switch", M98090_REG_29_MIX_HP_LEFT,
		M98090_MIXHPL_DACL_SHIFT, 1, 0),
	SOC_DAPM_SINGLE("Right DAC Switch", M98090_REG_29_MIX_HP_LEFT,
		M98090_MIXHPL_DACR_SHIFT, 1, 0),
	SOC_DAPM_SINGLE("LINEA Switch", M98090_REG_29_MIX_HP_LEFT,
		M98090_MIXHPL_LINEA_SHIFT, 1, 0),
	SOC_DAPM_SINGLE("LINEB Switch", M98090_REG_29_MIX_HP_LEFT,
		M98090_MIXHPL_LINEB_SHIFT, 1, 0),
	SOC_DAPM_SINGLE("MIC1 Switch", M98090_REG_29_MIX_HP_LEFT,
		M98090_MIXHPL_MIC1_SHIFT, 1, 0),
	SOC_DAPM_SINGLE("MIC2 Switch", M98090_REG_29_MIX_HP_LEFT,
		M98090_MIXHPL_MIC2_SHIFT, 1, 0),
};

/* Right headphone mixer switch */
static const struct snd_kcontrol_new max98090_right_hp_mixer_controls[] = {
	SOC_DAPM_SINGLE("Left DAC Switch", M98090_REG_2A_MIX_HP_RIGHT,
		M98090_MIXHPR_DACL_SHIFT, 1, 0),
	SOC_DAPM_SINGLE("Right DAC Switch", M98090_REG_2A_MIX_HP_RIGHT,
		M98090_MIXHPR_DACR_SHIFT, 1, 0),
	SOC_DAPM_SINGLE("LINEA Switch", M98090_REG_2A_MIX_HP_RIGHT,
		M98090_MIXHPR_LINEA_SHIFT, 1, 0),
	SOC_DAPM_SINGLE("LINEB Switch", M98090_REG_2A_MIX_HP_RIGHT,
		M98090_MIXHPR_LINEB_SHIFT, 1, 0),
	SOC_DAPM_SINGLE("MIC1 Switch", M98090_REG_2A_MIX_HP_RIGHT,
		M98090_MIXHPR_MIC1_SHIFT, 1, 0),
	SOC_DAPM_SINGLE("MIC2 Switch", M98090_REG_2A_MIX_HP_RIGHT,
		M98090_MIXHPR_MIC2_SHIFT, 1, 0),
};

/* Left receiver mixer switch */
static const struct snd_kcontrol_new max98090_left_rcv_mixer_controls[] = {
	SOC_DAPM_SINGLE("Left DAC Switch", M98090_REG_37_MIX_RCV_LEFT,
		M98090_MIXRCVL_DACL_SHIFT, 1, 0),
	SOC_DAPM_SINGLE("Right DAC Switch", M98090_REG_37_MIX_RCV_LEFT,
		M98090_MIXRCVL_DACR_SHIFT, 1, 0),
	SOC_DAPM_SINGLE("LINEA Switch", M98090_REG_37_MIX_RCV_LEFT,
		M98090_MIXRCVL_LINEA_SHIFT, 1, 0),
	SOC_DAPM_SINGLE("LINEB Switch", M98090_REG_37_MIX_RCV_LEFT,
		M98090_MIXRCVL_LINEB_SHIFT, 1, 0),
	SOC_DAPM_SINGLE("MIC1 Switch", M98090_REG_37_MIX_RCV_LEFT,
		M98090_MIXRCVL_MIC1_SHIFT, 1, 0),
	SOC_DAPM_SINGLE("MIC2 Switch", M98090_REG_37_MIX_RCV_LEFT,
		M98090_MIXRCVL_MIC2_SHIFT, 1, 0),
};

/* Right receiver mixer switch */
static const struct snd_kcontrol_new max98090_right_rcv_mixer_controls[] = {
	SOC_DAPM_SINGLE("Left DAC Switch", M98090_REG_3A_MIX_RCV_RIGHT,
		M98090_MIXRCVR_DACL_SHIFT, 1, 0),
	SOC_DAPM_SINGLE("Right DAC Switch", M98090_REG_3A_MIX_RCV_RIGHT,
		M98090_MIXRCVR_DACR_SHIFT, 1, 0),
	SOC_DAPM_SINGLE("LINEA Switch", M98090_REG_3A_MIX_RCV_RIGHT,
		M98090_MIXRCVR_LINEA_SHIFT, 1, 0),
	SOC_DAPM_SINGLE("LINEB Switch", M98090_REG_3A_MIX_RCV_RIGHT,
		M98090_MIXRCVR_LINEB_SHIFT, 1, 0),
	SOC_DAPM_SINGLE("MIC1 Switch", M98090_REG_3A_MIX_RCV_RIGHT,
		M98090_MIXRCVR_MIC1_SHIFT, 1, 0),
	SOC_DAPM_SINGLE("MIC2 Switch", M98090_REG_3A_MIX_RCV_RIGHT,
		M98090_MIXRCVR_MIC2_SHIFT, 1, 0),
};

static const char *linmod_mux_text[] = { "Left Only", "Left and Right" };

static const struct soc_enum linmod_mux_enum =
	SOC_ENUM_SINGLE(M98090_REG_3A_MIX_RCV_RIGHT, M98090_LINMOD_SHIFT,
		ARRAY_SIZE(linmod_mux_text), linmod_mux_text);

static const struct snd_kcontrol_new max98090_linmod_mux =
	SOC_DAPM_ENUM("LINMOD Mux", linmod_mux_enum);

static const char *mixhpsel_mux_text[] = { "DAC Only", "HP Mixer" };

/*
 * This is a mux as it selects the HP output, but to DAPM it is a Mixer enable
 */
static const struct soc_enum mixhplsel_mux_enum =
	SOC_ENUM_SINGLE(M98090_REG_2B_MIX_HP_CNTL, M98090_MIXHPLSEL_SHIFT,
		ARRAY_SIZE(mixhpsel_mux_text), mixhpsel_mux_text);

static const struct snd_kcontrol_new max98090_mixhplsel_mux =
	SOC_DAPM_ENUM("MIXHPLSEL Mux", mixhplsel_mux_enum);

static const struct soc_enum mixhprsel_mux_enum =
	SOC_ENUM_SINGLE(M98090_REG_2B_MIX_HP_CNTL, M98090_MIXHPRSEL_SHIFT,
		ARRAY_SIZE(mixhpsel_mux_text), mixhpsel_mux_text);

static const struct snd_kcontrol_new max98090_mixhprsel_mux =
	SOC_DAPM_ENUM("MIXHPRSEL Mux", mixhprsel_mux_enum);
	
static const struct snd_soc_dapm_widget max98090_dapm_widgets[] = {

	SND_SOC_DAPM_INPUT("MIC1"),
	SND_SOC_DAPM_INPUT("MIC2"),
	SND_SOC_DAPM_INPUT("DMIC1"),
	SND_SOC_DAPM_INPUT("DMIC2"),
	SND_SOC_DAPM_INPUT("IN1"),
	SND_SOC_DAPM_INPUT("IN2"),
	SND_SOC_DAPM_INPUT("IN3"),
	SND_SOC_DAPM_INPUT("IN4"),
	SND_SOC_DAPM_INPUT("IN5"),
	SND_SOC_DAPM_INPUT("IN6"),
	SND_SOC_DAPM_INPUT("IN12"),
	SND_SOC_DAPM_INPUT("IN34"),
	SND_SOC_DAPM_INPUT("IN56"),

	SND_SOC_DAPM_MICBIAS("MICBIAS", M98090_REG_3E_PWR_EN_IN,
		M98090_PWR_MBEN_SHIFT, 0),
	SND_SOC_DAPM_SUPPLY("SHDN", M98090_REG_45_PWR_SYS,
		M98090_PWR_SHDNN_SHIFT, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY("VCM", M98090_REG_42_BIAS_CNTL,
		M98090_VCM_MODE_SHIFT, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY("SDIEN", M98090_REG_25_DAI_IOCFG,
		M98090_DAI_SDIEN_SHIFT, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY("SDOEN", M98090_REG_25_DAI_IOCFG,
		M98090_DAI_SDOEN_SHIFT, 0, NULL, 0),

/*
 * Note: Sysclk and misc power supplies are taken care of by SHDN and VCM
 */

	SND_SOC_DAPM_MUX("MIC1 Mux", SND_SOC_NOPM, 0, 0, &max98090_mic1_mux),
	SND_SOC_DAPM_MUX("MIC2 Mux", SND_SOC_NOPM, 0, 0, &max98090_mic2_mux),

	SND_SOC_DAPM_PGA_E("MIC1 Input", M98090_REG_10_LVL_MIC1,
		M98090_MIC_PA1EN_SHIFT, 0, NULL, 0, max98090_micinput_event,
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_PGA_E("MIC2 Input", M98090_REG_11_LVL_MIC2,
		M98090_MIC_PA2EN_SHIFT, 0, NULL, 0, max98090_micinput_event,
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_MIXER("LINEA Mixer", SND_SOC_NOPM, 0, 0,
		&max98090_linea_mixer_controls[0],
		ARRAY_SIZE(max98090_linea_mixer_controls)),

	SND_SOC_DAPM_MIXER("LINEB Mixer", SND_SOC_NOPM, 0, 0,
		&max98090_lineb_mixer_controls[0],
		ARRAY_SIZE(max98090_lineb_mixer_controls)),

	SND_SOC_DAPM_PGA("LINEA Input", M98090_REG_3E_PWR_EN_IN,
		M98090_PWR_LINEAEN_SHIFT, 0, NULL, 0),
	SND_SOC_DAPM_PGA("LINEB Input", M98090_REG_3E_PWR_EN_IN,
		M98090_PWR_LINEBEN_SHIFT, 0, NULL, 0),

	SND_SOC_DAPM_MIXER("Left ADC Mixer", SND_SOC_NOPM, 0, 0,
		&max98090_left_adc_mixer_controls[0],
		ARRAY_SIZE(max98090_left_adc_mixer_controls)),

	SND_SOC_DAPM_MIXER("Right ADC Mixer", SND_SOC_NOPM, 0, 0,
		&max98090_right_adc_mixer_controls[0],
		ARRAY_SIZE(max98090_right_adc_mixer_controls)),

	SND_SOC_DAPM_ADC("ADCL", NULL, M98090_REG_3E_PWR_EN_IN,
		M98090_PWR_ADLEN_SHIFT, 0),
	SND_SOC_DAPM_ADC("ADCR", NULL, M98090_REG_3E_PWR_EN_IN,
		M98090_PWR_ADREN_SHIFT, 0),
 
	SND_SOC_DAPM_AIF_OUT("AIFOUTL", "HiFi Capture", 0,
		SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_AIF_OUT("AIFOUTR", "HiFi Capture", 1,
		SND_SOC_NOPM, 0, 0),

	SND_SOC_DAPM_MUX("LBENL Mux", SND_SOC_NOPM,
		0, 0, &max98090_lbenl_mux),

	SND_SOC_DAPM_MUX("LBENR Mux", SND_SOC_NOPM,
		0, 0, &max98090_lbenr_mux),

	SND_SOC_DAPM_MUX("LTENL Mux", SND_SOC_NOPM,
		0, 0, &max98090_ltenl_mux),

	SND_SOC_DAPM_MUX("LTENR Mux", SND_SOC_NOPM,
		0, 0, &max98090_ltenr_mux),

	SND_SOC_DAPM_PGA_E("Sidetone", M98090_REG_1A_LVL_SIDETONE,
		M98090_DVST_SHIFT, 0, NULL, 0, max98090_sidetone_event,
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_AIF_IN("AIFINL", "HiFi Playback", 0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_AIF_IN("AIFINR", "HiFi Playback", 1, SND_SOC_NOPM, 0, 0),

	SND_SOC_DAPM_DAC("DACL", NULL, M98090_REG_3F_PWR_EN_OUT,
		M98090_PWR_DALEN_SHIFT, 0),
	SND_SOC_DAPM_DAC("DACR", NULL, M98090_REG_3F_PWR_EN_OUT,
		M98090_PWR_DAREN_SHIFT, 0),

	SND_SOC_DAPM_MIXER("Left Headphone Mixer", SND_SOC_NOPM, 0, 0,
		&max98090_left_hp_mixer_controls[0],
		ARRAY_SIZE(max98090_left_hp_mixer_controls)),

	SND_SOC_DAPM_MIXER("Right Headphone Mixer", SND_SOC_NOPM, 0, 0,
		&max98090_right_hp_mixer_controls[0],
		ARRAY_SIZE(max98090_right_hp_mixer_controls)),

	SND_SOC_DAPM_MIXER("Left Speaker Mixer", SND_SOC_NOPM, 0, 0,
		&max98090_left_speaker_mixer_controls[0],
		ARRAY_SIZE(max98090_left_speaker_mixer_controls)),

	SND_SOC_DAPM_MIXER("Right Speaker Mixer", SND_SOC_NOPM, 0, 0,
		&max98090_right_speaker_mixer_controls[0],
		ARRAY_SIZE(max98090_right_speaker_mixer_controls)),

	SND_SOC_DAPM_MIXER("Left Receiver Mixer", SND_SOC_NOPM, 0, 0,
		&max98090_left_rcv_mixer_controls[0],
		ARRAY_SIZE(max98090_left_rcv_mixer_controls)),

	SND_SOC_DAPM_MIXER("Right Receiver Mixer", SND_SOC_NOPM, 0, 0,
		&max98090_right_rcv_mixer_controls[0],
		ARRAY_SIZE(max98090_right_rcv_mixer_controls)),

	SND_SOC_DAPM_MUX("LINMOD Mux", M98090_REG_3A_MIX_RCV_RIGHT,
		M98090_LINMOD_SHIFT, 0, &max98090_linmod_mux),

	SND_SOC_DAPM_MUX("MIXHPLSEL Mux", M98090_REG_2B_MIX_HP_CNTL,
		M98090_MIXHPLSEL_SHIFT, 0, &max98090_mixhplsel_mux),

	SND_SOC_DAPM_MUX("MIXHPRSEL Mux", M98090_REG_2B_MIX_HP_CNTL,
		M98090_MIXHPRSEL_SHIFT, 0, &max98090_mixhprsel_mux),

	SND_SOC_DAPM_PGA("HP Left Out", M98090_REG_3F_PWR_EN_OUT,
		M98090_PWR_HPLEN_SHIFT, 0, NULL, 0),
	SND_SOC_DAPM_PGA("HP Right Out", M98090_REG_3F_PWR_EN_OUT,
		M98090_PWR_HPREN_SHIFT, 0, NULL, 0),

	SND_SOC_DAPM_PGA("SPK Left Out", M98090_REG_3F_PWR_EN_OUT,
		M98090_PWR_SPLEN_SHIFT, 0, NULL, 0),
	SND_SOC_DAPM_PGA("SPK Right Out", M98090_REG_3F_PWR_EN_OUT,
		M98090_PWR_SPREN_SHIFT, 0, NULL, 0),

	SND_SOC_DAPM_PGA("RCV Left Out", M98090_REG_3F_PWR_EN_OUT,
		M98090_PWR_RCVLEN_SHIFT, 0, NULL, 0),
	SND_SOC_DAPM_PGA("RCV Right Out", M98090_REG_3F_PWR_EN_OUT,
		M98090_PWR_RCVREN_SHIFT, 0, NULL, 0),

	SND_SOC_DAPM_OUTPUT("HPL"),
	SND_SOC_DAPM_OUTPUT("HPR"),
	SND_SOC_DAPM_OUTPUT("SPKL"),
	SND_SOC_DAPM_OUTPUT("SPKR"),
	SND_SOC_DAPM_OUTPUT("RCVL"),
	SND_SOC_DAPM_OUTPUT("RCVR"),
};

static const struct snd_soc_dapm_route max98090_dapm_routes[] = {

	{"MIC1 Input", NULL, "MICBIAS"},
	{"MIC2 Input", NULL, "MICBIAS"},

	/* MIC1 input mux */
	{"MIC1 Mux", "IN12", "IN12"},
	{"MIC1 Mux", "IN56", "IN56"},

	/* MIC2 input mux */
	{"MIC2 Mux", "IN34", "IN34"},
	{"MIC2 Mux", "IN56", "IN56"},

	{"MIC1 Input", NULL, "MIC1 Mux"},
	{"MIC2 Input", NULL, "MIC2 Mux"},

	/* Left ADC input mixer */
	{"Left ADC Mixer", "IN12 Switch", "IN12"},
	{"Left ADC Mixer", "IN34 Switch", "IN34"},
	{"Left ADC Mixer", "IN56 Switch", "IN56"},
	{"Left ADC Mixer", "LINEA Switch", "LINEA Input"},
	{"Left ADC Mixer", "LINEB Switch", "LINEB Input"},
	{"Left ADC Mixer", "MIC1 Switch", "MIC1 Input"},
	{"Left ADC Mixer", "MIC2 Switch", "MIC2 Input"},

	/* Right ADC input mixer */
	{"Right ADC Mixer", "IN12 Switch", "IN12"},
	{"Right ADC Mixer", "IN34 Switch", "IN34"},
	{"Right ADC Mixer", "IN56 Switch", "IN56"},
	{"Right ADC Mixer", "LINEA Switch", "LINEA Input"},
	{"Right ADC Mixer", "LINEB Switch", "LINEB Input"},
	{"Right ADC Mixer", "MIC1 Switch", "MIC1 Input"},
	{"Right ADC Mixer", "MIC2 Switch", "MIC2 Input"},

	/* Line A input mixer */
	{"LINEA Mixer", "IN1 Switch", "IN1"},
	{"LINEA Mixer", "IN3 Switch", "IN3"},
	{"LINEA Mixer", "IN5 Switch", "IN5"},
	{"LINEA Mixer", "IN34 Switch", "IN34"},

	/* Line B input mixer */
	{"LINEB Mixer", "IN2 Switch", "IN2"},
	{"LINEB Mixer", "IN4 Switch", "IN4"},
	{"LINEB Mixer", "IN6 Switch", "IN6"},
	{"LINEB Mixer", "IN56 Switch", "IN56"},

	{"LINEA Input", NULL, "LINEA Mixer"},
	{"LINEB Input", NULL, "LINEB Mixer"},

	/* Inputs */
	{"ADCL", NULL, "Left ADC Mixer"},
	{"ADCR", NULL, "Right ADC Mixer"},
	{"ADCL", NULL, "SHDN"},
	{"ADCR", NULL, "SHDN"},
	{"ADCL", NULL, "VCM"},
	{"ADCR", NULL, "VCM"},

	{"LBENL Mux", "Normal", "ADCL"},
	{"LBENL Mux", "Loopback", "LTENL Mux"},
	{"LBENR Mux", "Normal", "ADCR"},
	{"LBENR Mux", "Loopback", "LTENR Mux"},

	{"AIFOUTL", NULL, "LBENL Mux"},
	{"AIFOUTR", NULL, "LBENR Mux"},
	{"AIFOUTL", NULL, "SHDN"},
	{"AIFOUTR", NULL, "SHDN"},
	{"AIFOUTL", NULL, "VCM"},
	{"AIFOUTR", NULL, "VCM"},
	{"AIFOUTL", NULL, "SDOEN"},
	{"AIFOUTR", NULL, "SDOEN"},

	{"LTENL Mux", "Normal", "AIFINL"},
	{"LTENL Mux", "Loopthrough", "LBENL Mux"},
	{"LTENR Mux", "Normal", "AIFINR"},
	{"LTENR Mux", "Loopthrough", "LBENR Mux"},

	{"DACL", NULL, "LTENL Mux"},
	{"DACR", NULL, "LTENR Mux"},

	{"AIFINL", NULL, "SHDN"},
	{"AIFINR", NULL, "SHDN"},
	{"AIFINL", NULL, "VCM"},
	{"AIFINR", NULL, "VCM"},
	{"AIFINL", NULL, "SDIEN"},
	{"AIFINR", NULL, "SDIEN"},
	{"DACL", NULL, "SHDN"},
	{"DACR", NULL, "SHDN"},
	{"DACL", NULL, "VCM"},
	{"DACR", NULL, "VCM"},

	/* Left headphone output mixer */
	{"Left Headphone Mixer", "Left DAC Switch", "DACL"},
	{"Left Headphone Mixer", "Right DAC Switch", "DACR"},
	{"Left Headphone Mixer", "MIC1 Switch", "MIC1 Input"},
	{"Left Headphone Mixer", "MIC2 Switch", "MIC2 Input"},
	{"Left Headphone Mixer", "LINEA Switch", "LINEA Input"},
	{"Left Headphone Mixer", "LINEB Switch", "LINEB Input"},

	/* Right headphone output mixer */
	{"Right Headphone Mixer", "Left DAC Switch", "DACL"},
	{"Right Headphone Mixer", "Right DAC Switch", "DACR"},
	{"Right Headphone Mixer", "MIC1 Switch", "MIC1 Input"},
	{"Right Headphone Mixer", "MIC2 Switch", "MIC2 Input"},
	{"Right Headphone Mixer", "LINEA Switch", "LINEA Input"},
	{"Right Headphone Mixer", "LINEB Switch", "LINEB Input"},

	/* Left speaker output mixer */
	{"Left Speaker Mixer", "Left DAC Switch", "DACL"},
	{"Left Speaker Mixer", "Right DAC Switch", "DACR"},
	{"Left Speaker Mixer", "MIC1 Switch", "MIC1 Input"},
	{"Left Speaker Mixer", "MIC2 Switch", "MIC2 Input"},
	{"Left Speaker Mixer", "LINEA Switch", "LINEA Input"},
	{"Left Speaker Mixer", "LINEB Switch", "LINEB Input"},

	/* Right speaker output mixer */
	{"Right Speaker Mixer", "Left DAC Switch", "DACL"},
	{"Right Speaker Mixer", "Right DAC Switch", "DACR"},
	{"Right Speaker Mixer", "MIC1 Switch", "MIC1 Input"},
	{"Right Speaker Mixer", "MIC2 Switch", "MIC2 Input"},
	{"Right Speaker Mixer", "LINEA Switch", "LINEA Input"},
	{"Right Speaker Mixer", "LINEB Switch", "LINEB Input"},

	/* Left Receiver output mixer */
	{"Left Receiver Mixer", "Left DAC Switch", "DACL"},
	{"Left Receiver Mixer", "Right DAC Switch", "DACR"},
	{"Left Receiver Mixer", "MIC1 Switch", "MIC1 Input"},
	{"Left Receiver Mixer", "MIC2 Switch", "MIC2 Input"},
	{"Left Receiver Mixer", "LINEA Switch", "LINEA Input"},
	{"Left Receiver Mixer", "LINEB Switch", "LINEB Input"},

	/* Right Receiver output mixer */
	{"Right Receiver Mixer", "Left DAC Switch", "DACL"},
	{"Right Receiver Mixer", "Right DAC Switch", "DACR"},
	{"Right Receiver Mixer", "MIC1 Switch", "MIC1 Input"},
	{"Right Receiver Mixer", "MIC2 Switch", "MIC2 Input"},
	{"Right Receiver Mixer", "LINEA Switch", "LINEA Input"},
	{"Right Receiver Mixer", "LINEB Switch", "LINEB Input"},

	{"MIXHPLSEL Mux", "HP Mixer", "Left Headphone Mixer"},

	/*
	 * Disable this for lowest power if bypassing
	 * the DAC with an analog signal
	 */
	{"HP Left Out", NULL, "DACL"},
	{"HP Left Out", NULL, "MIXHPLSEL Mux"},

	{"MIXHPRSEL Mux", "HP Mixer", "Right Headphone Mixer"},

	/*
	 * Disable this for lowest power if bypassing
	 * the DAC with an analog signal
	 */
	{"HP Right Out", NULL, "DACR"},
	{"HP Right Out", NULL, "MIXHPRSEL Mux"},

	{"SPK Left Out", NULL, "Left Speaker Mixer"},
	{"SPK Right Out", NULL, "Right Speaker Mixer"},
	{"RCV Left Out", NULL, "Left Receiver Mixer"},

	{"LINMOD Mux", "Left and Right", "Right Receiver Mixer"},
	{"LINMOD Mux", "Left Only",  "Left Receiver Mixer"},
	{"RCV Right Out", NULL, "LINMOD Mux"},
	
	{"HPL", NULL, "HP Left Out"},
	{"HPR", NULL, "HP Right Out"},
	{"SPKL", NULL, "SPK Left Out"},
	{"SPKR", NULL, "SPK Right Out"},
	{"RCVL", NULL, "RCV Left Out"},
	{"RCVR", NULL, "RCV Right Out"},

};

static int max98090_add_widgets(struct snd_soc_codec *codec)
{
	struct max98090_priv *max98090 = snd_soc_codec_get_drvdata(codec);
	struct snd_soc_dapm_context *dapm = &codec->dapm;

	snd_soc_add_codec_controls(codec, max98090_snd_controls,
		ARRAY_SIZE(max98090_snd_controls));

	if (max98090->devtype == MAX98091) {
		snd_soc_add_codec_controls(codec, max98091_snd_controls,
			ARRAY_SIZE(max98091_snd_controls));
	}

	snd_soc_dapm_new_controls(dapm, max98090_dapm_widgets,
		ARRAY_SIZE(max98090_dapm_widgets));
	snd_soc_dapm_add_routes(dapm, max98090_dapm_routes,
		ARRAY_SIZE(max98090_dapm_routes));
	snd_soc_dapm_new_widgets(dapm);

	return 0;
}

static const int pclk_rates[] = {
	12000000, 12000000, 13000000, 13000000,
	16000000, 16000000, 19200000, 19200000
};

static const int lrclk_rates[] = {
	8000, 16000, 8000, 16000,
	8000, 16000, 8000, 16000
};

static const int user_pclk_rates[] = {
	13000000, 13000000, 19200000, 19200000
};

static const int user_lrclk_rates[] = {
	44100, 48000, 44100, 48000
};

static const unsigned long long ni_value[] = {
	3528, 768, 147, 160
};

static const unsigned long long mi_value[] = {
	8125, 1625, 500, 500
};

static void max98090_configure_bclk(struct snd_soc_codec *codec)
{
	struct max98090_priv *max98090 = snd_soc_codec_get_drvdata(codec);
	unsigned long long ni;
	int i;

	if (!max98090->sysclk) {
		dev_dbg(codec->dev, "No SYSCLK configured\n");
		return;
	}

	if (!max98090->bclk || !max98090->lrclk) {
		dev_dbg(codec->dev, "No audio clocks configured\n");
		return;
	}

	/* Skip configuration when operating as slave */
	if (!(snd_soc_read(codec, M98090_REG_21_CLOCK_MAS_MODE) &
		M98090_CLK_MAS_MASK)) {
		return;
	}

	/* Check for supported PCLK to LRCLK ratios */
	for (i = 0; i < ARRAY_SIZE(pclk_rates); i++) {
		if ((pclk_rates[i] == max98090->sysclk) &&
			(lrclk_rates[i] == max98090->lrclk)) {
			dev_info(codec->dev,
				"Found supported PCLK to LRCLK rates 0x%x\n",
				i + 0x8);

			snd_soc_update_bits(codec, M98090_REG_1C_CLOCK_MODE,
				M98090_CLK_FREQ1_MASK,
				(i + 0x8) << M98090_CLK_FREQ1_SHIFT);
			snd_soc_update_bits(codec, M98090_REG_1C_CLOCK_MODE,
				M98090_CLK_USE_M1_MASK, 0);
			return;
		}
	}

	/* Check for user calculated MI and NI ratios */
	for (i = 0; i < ARRAY_SIZE(user_pclk_rates); i++) {
		if ((user_pclk_rates[i] == max98090->sysclk) &&
			(user_lrclk_rates[i] == max98090->lrclk)) {
			dev_info(codec->dev,
				"Found user supported PCLK to LRCLK rates\n");
			dev_info(codec->dev, "i %d ni %lld mi %lld\n",
				i, ni_value[i], mi_value[i]);

			snd_soc_update_bits(codec, M98090_REG_1C_CLOCK_MODE,
				M98090_CLK_FREQ1_MASK, 0);
			snd_soc_update_bits(codec, M98090_REG_1C_CLOCK_MODE,
				M98090_CLK_USE_M1_MASK,
					1 << M98090_CLK_USE_M1_SHIFT);

			snd_soc_write(codec, M98090_REG_1D_CLOCK_DAI1_NI_HI,
				(ni_value[i] >> 8) & 0x7F);
			snd_soc_write(codec, M98090_REG_1E_CLOCK_DAI2_NI_LO,
				ni_value[i] & 0xFF);
			snd_soc_write(codec, M98090_REG_1F_CLOCK_DAI3_MI_HI,
				(mi_value[i] >> 8) & 0x7F);
			snd_soc_write(codec, M98090_REG_20_CLOCK_DAI4_MI_LO,
				mi_value[i] & 0xFF);

			return;
		}
	}

	/*
	 * Calculate based on MI = 65536 (not as good as either method above)
	 */
	snd_soc_update_bits(codec, M98090_REG_1C_CLOCK_MODE,
		M98090_CLK_FREQ1_MASK, 0);
	snd_soc_update_bits(codec, M98090_REG_1C_CLOCK_MODE,
		M98090_CLK_USE_M1_MASK, 0);

	/*
	 * Configure NI when operating as master
	 * Note: There is a small, but significant audio quality improvement
	 * by calculating ni and mi.
	 */
	ni = 65536ULL * (max98090->lrclk < 50000 ? 96ULL : 48ULL)
			* (unsigned long long int)max98090->lrclk;
	do_div(ni, (unsigned long long int)max98090->sysclk);
	dev_info(codec->dev, "No better method found\n");
	dev_info(codec->dev, "Calculating ni %lld with mi 65536\n", ni);
	snd_soc_write(codec, M98090_REG_1D_CLOCK_DAI1_NI_HI,
		(ni >> 8) & 0x7F);
	snd_soc_write(codec, M98090_REG_1E_CLOCK_DAI2_NI_LO, ni & 0xFF);
}

static int max98090_dai_set_fmt(struct snd_soc_dai *codec_dai,
				 unsigned int fmt)
{
	struct snd_soc_codec *codec = codec_dai->codec;
	struct max98090_priv *max98090 = snd_soc_codec_get_drvdata(codec);
	struct max98090_cdata *cdata;
	u8 regval;

	max98090->dai_fmt = fmt;
	cdata = &max98090->dai[0];

	if (fmt != cdata->fmt) {
		cdata->fmt = fmt;

		regval = 0;
		switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
		case SND_SOC_DAIFMT_CBS_CFS:
			/* Set to slave mode PLL - MAS mode off */
			snd_soc_write(codec,
				M98090_REG_1D_CLOCK_DAI1_NI_HI, 0x00);
			snd_soc_write(codec,
				M98090_REG_1E_CLOCK_DAI2_NI_LO, 0x00);
			snd_soc_update_bits(codec, M98090_REG_1C_CLOCK_MODE,
				M98090_CLK_USE_M1_MASK, 0);
			break;
		case SND_SOC_DAIFMT_CBM_CFM:
			/* Set to master mode */
			if (max98090->tdm_slots == 4) {
				/* TDM */
				regval |= M98090_CLK_MAS_MASK |
					M98090_CLK_BSEL_64;
			} else if (max98090->tdm_slots == 3) {
				/* TDM */
				regval |= M98090_CLK_MAS_MASK |
					M98090_CLK_BSEL_48;
			} else {
				/* Few TDM slots, or No TDM */
				regval |= M98090_CLK_MAS_MASK |
					M98090_CLK_BSEL_32;
			}
			break;
		case SND_SOC_DAIFMT_CBS_CFM:
		case SND_SOC_DAIFMT_CBM_CFS:
		default:
			dev_err(codec->dev, "DAI clock mode unsupported");
			return -EINVAL;
		}
		snd_soc_write(codec, M98090_REG_21_CLOCK_MAS_MODE, regval);

		regval = 0;
		switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
		case SND_SOC_DAIFMT_I2S:
			regval |= M98090_DAI_DLY_MASK;
			break;
		case SND_SOC_DAIFMT_LEFT_J:
			break;
		case SND_SOC_DAIFMT_RIGHT_J:
			regval |= M98090_DAI_RJ_MASK;
			break;
		case SND_SOC_DAIFMT_DSP_A:
			/* Not supported mode */
		default:
			dev_err(codec->dev, "DAI format unsupported");
			return -EINVAL;
		}

		switch (fmt & SND_SOC_DAIFMT_INV_MASK) {
		case SND_SOC_DAIFMT_NB_NF:
			break;
		case SND_SOC_DAIFMT_NB_IF:
			regval |= M98090_DAI_WCI_MASK;
			break;
		case SND_SOC_DAIFMT_IB_NF:
			regval |= M98090_DAI_BCI_MASK;
			break;
		case SND_SOC_DAIFMT_IB_IF:
			regval |= M98090_DAI_BCI_MASK|M98090_DAI_WCI_MASK;
			break;
		default:
			dev_err(codec->dev, "DAI invert mode unsupported");
			return -EINVAL;
		}

/* 
 * This accommodates an inverted logic in the MAX98090 chip
 * for Bit Clock Invert (BCI). The inverted logic is only seen
 * for the case of TDM mode. The remaining cases have normal logic.
 */
		if (max98090->tdm_slots > 1) {
			regval ^= M98090_DAI_BCI_MASK;
		}

		snd_soc_write(codec,
			M98090_REG_22_DAI_INTERFACE_FORMAT, regval);
	}

	return 0;
}

static int max98090_set_tdm_slot(struct snd_soc_dai *codec_dai,
	unsigned int tx_mask, unsigned int rx_mask, int slots, int slot_width)
{
	struct snd_soc_codec *codec = codec_dai->codec;
	struct max98090_priv *max98090 = snd_soc_codec_get_drvdata(codec);
	struct max98090_cdata *cdata;
	cdata = &max98090->dai[0];

	if (slots < 0 || slots > 4)
		return -EINVAL;

	max98090->tdm_slots = slots;
	max98090->tdm_width = slot_width;

	if (max98090->tdm_slots > 1) {
		/* SLOTL SLOTR SLOTDLY */
		snd_soc_write(codec, M98090_REG_24_DAI_TDM_FORMAT,
			0 << M98090_DAI_TDM_SLOTL_SHIFT |
			1 << M98090_DAI_TDM_SLOTR_SHIFT |
			0 << M98090_DAI_TDM_SLOTDLY_SHIFT);

		/* FSW TDM */
		snd_soc_update_bits(codec, M98090_REG_23_DAI_TDM_CONTROL,
			M98090_DAI_TDM_MASK,
			M98090_DAI_TDM_MASK);
	}

	/*
	 * Normally advisable to set TDM first, but this permits either order
	 */
	cdata->fmt = 0;
	max98090_dai_set_fmt(codec_dai, max98090->dai_fmt);

	return 0;
}

static int max98090_set_bias_level(struct snd_soc_codec *codec,
				   enum snd_soc_bias_level level)
{
	struct max98090_priv *max98090 = snd_soc_codec_get_drvdata(codec);
	int ret;

	switch (level) {
	case SND_SOC_BIAS_ON:
		if (codec->dapm.bias_level == SND_SOC_BIAS_OFF) {
			ret = regcache_sync(max98090->regmap);

			if (ret != 0) {
				dev_err(codec->dev,
					"Failed to sync cache: %d\n", ret);
				return ret;
			}
		}

		if (max98090->jack_state == M98090_JACK_STATE_HEADSET) {
			/*
			 * Set to normal bias level.
			 */
			snd_soc_update_bits(codec, M98090_REG_12_MIC_BIAS,
				M98090_MBVSEL_MASK, M98090_MBVSEL_2V4);

			snd_soc_update_bits(codec, M98090_REG_3E_PWR_EN_IN,
				M98090_PWR_MBEN_MASK, M98090_PWR_MBEN_MASK);
		}
		break;

	case SND_SOC_BIAS_PREPARE:
		break;

	case SND_SOC_BIAS_STANDBY:
	case SND_SOC_BIAS_OFF:
		snd_soc_update_bits(codec, M98090_REG_3E_PWR_EN_IN,
			M98090_PWR_MBEN_MASK, 0);
		/* Set internal pull-up to lowest power mode */
		snd_soc_update_bits(codec, M98090_REG_3D_CFG_JACK,
			M98090_JDWK_MASK, M98090_JDWK_MASK);
		regcache_mark_dirty(max98090->regmap);
		break;
	}
	codec->dapm.bias_level = level;
	return 0;
}

static const int comp_pclk_rates[] = {
	11289600, 12288000, 12000000, 13000000, 19200000
};

static const int dmic_micclk[] = {
	2, 2, 2, 2, 4, 2
};

static const int comp_lrclk_rates[] = {
	8000, 16000, 32000, 44100, 48000, 96000
};

static const int dmic_comp[6][6] = {
	{7, 8, 3, 3, 3, 3},
	{7, 8, 3, 3, 3, 3},
	{7, 8, 3, 3, 3, 3},
	{7, 8, 3, 1, 1, 1},
	{7, 8, 3, 1, 2, 2},
	{7, 8, 3, 3, 3, 3}
};

static int max98090_dai_hw_params(struct snd_pcm_substream *substream,
				   struct snd_pcm_hw_params *params,
				   struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;
	struct max98090_priv *max98090 = snd_soc_codec_get_drvdata(codec);
	struct max98090_cdata *cdata;
	int i, j;

	cdata = &max98090->dai[0];
	max98090->bclk = snd_soc_params_to_bclk(params);
	if (params_channels(params) == 1)
		max98090->bclk *= 2;

	max98090->lrclk = params_rate(params);

	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_S16_LE:
		snd_soc_update_bits(codec, M98090_REG_22_DAI_INTERFACE_FORMAT,
			M98090_DAI_WS_MASK, 0);
		break;
	default:
		return -EINVAL;
	}

	max98090_configure_bclk(codec);

	cdata->rate = max98090->lrclk;

	/* Update filter mode */
	if (max98090->lrclk < 24000) {
		snd_soc_update_bits(codec, M98090_REG_26_DAI_FILTERS,
			M98090_DAI_FLT_MODE_MASK, 0);
	} else {
		snd_soc_update_bits(codec, M98090_REG_26_DAI_FILTERS,
			M98090_DAI_FLT_MODE_MASK, M98090_DAI_FLT_MODE_MASK);
	}

	/* Update sample rate mode */
	if (max98090->lrclk < 50000)
		snd_soc_update_bits(codec, M98090_REG_26_DAI_FILTERS,
			M98090_DAI_FLT_DHF_MASK, 0);
	else
		snd_soc_update_bits(codec, M98090_REG_26_DAI_FILTERS,
			M98090_DAI_FLT_DHF_MASK, M98090_DAI_FLT_DHF_MASK);

	/* Check for supported PCLK to LRCLK ratios */
	for (j = 0; j < ARRAY_SIZE(comp_pclk_rates); j++) {
		if (comp_pclk_rates[j] == max98090->sysclk) {
			break;
		}
	}

	for (i = 0; i < ARRAY_SIZE(comp_lrclk_rates) - 1; i++) {
		if (max98090->lrclk <= (comp_lrclk_rates[i] +
			comp_lrclk_rates[i + 1]) / 2) {
			break;
		}
	}

	snd_soc_update_bits(codec, M98090_REG_13_MIC_CFG1, 
			M98090_MICCLK_MASK,
			dmic_micclk[j] << M98090_MICCLK_SHIFT);

	snd_soc_update_bits(codec, M98090_REG_14_MIC_CFG2, 
			M98090_DMIC_COMP_MASK,
			dmic_comp[j][i] << M98090_DMIC_COMP_SHIFT);

	return 0;
}

/*
 * PLL / Sysclk
 */
static int max98090_dai_set_sysclk(struct snd_soc_dai *dai,
				   int clk_id, unsigned int freq, int dir)
{
	struct snd_soc_codec *codec = dai->codec;
	struct max98090_priv *max98090 = snd_soc_codec_get_drvdata(codec);

	/* Requested clock frequency is already setup */
	if (freq == max98090->sysclk)
		return 0;

	/* Setup clocks for slave mode, and using the PLL
	 * PSCLK = 0x01 (when master clk is 10MHz to 20MHz)
	 *		 0x02 (when master clk is 20MHz to 40MHz)..
	 *		 0x03 (when master clk is 40MHz to 60MHz)..
	 */
	if ((freq >= 10000000) && (freq < 20000000)) {
		snd_soc_write(codec, M98090_REG_1B_SYS_CLOCK,
			M98090_CLK_PSCLK_DIV1);
	} else if ((freq >= 20000000) && (freq < 40000000)) {
		snd_soc_write(codec, M98090_REG_1B_SYS_CLOCK,
			M98090_CLK_PSCLK_DIV2);
	} else if ((freq >= 40000000) && (freq < 60000000)) {
		snd_soc_write(codec, M98090_REG_1B_SYS_CLOCK,
			M98090_CLK_PSCLK_DIV4);
	} else {
		dev_err(codec->dev, "Invalid master clock frequency\n");
		return -EINVAL;
	}

	max98090->sysclk = freq;
	max98090_configure_bclk(codec);

	return 0;
}

static int max98090_dai_digital_mute(struct snd_soc_dai *codec_dai, int mute)
{
	struct snd_soc_codec *codec = codec_dai->codec;
	int regval;

	dev_info(codec->dev, "max98090_dai_digital_mute: mute=%d\n", mute);

	regval = mute ? M98090_DAI_LVL_DVM_MASK : 0;
	snd_soc_update_bits(codec, M98090_REG_27_DAI_LVL,
		M98090_DAI_LVL_DVM_MASK, regval);

	return 0;
}

static void max98090_dmic_switch(struct snd_soc_codec *codec, int state)
{
	struct max98090_priv *max98090 = snd_soc_codec_get_drvdata(codec);
	struct max98090_pdata *pdata = max98090->pdata;
	u8 regval = 0;
	
	dev_info(codec->dev,
		"max98090_dmic_switch state=%d left=%d right=%d\n",
		state, pdata->digmic_left_mode, pdata->digmic_right_mode);

	if (state) {
		/* Configure mic for analog/digital mic mode */
		if (pdata->digmic_left_mode)
			regval |= M98090_DIGMICL_MASK;

		if (pdata->digmic_right_mode)
			regval |= M98090_DIGMICR_MASK;

		if (max98090->devtype == MAX98091) {
			if (pdata->digmic_3_mode)
				regval |= M98090_DIGMIC3_MASK;

			if (pdata->digmic_4_mode)
				regval |= M98090_DIGMIC4_MASK;
		}

		if (regval) {
			/*
			 * Different default setup of DMIC can go here:
			 *  M98090_REG_13_MIC_CFG1
			 *  M98090_REG_14_MIC_CFG2
			 */

			snd_soc_update_bits(codec, M98090_REG_26_DAI_FILTERS,
				M98090_DAI_FLT_AHPF_MASK,
				M98090_DAI_FLT_AHPF_MASK);
		}

	} else {
		snd_soc_update_bits(codec, M98090_REG_26_DAI_FILTERS,
			M98090_DAI_FLT_AHPF_MASK, 0);
	}
	
	snd_soc_update_bits(codec, M98090_REG_13_MIC_CFG1, 
			M98090_DIGMIC4_MASK | M98090_DIGMIC3_MASK |
			M98090_DIGMICR_MASK | M98090_DIGMICL_MASK,
			regval);
}

#ifdef MAX98090_SUPPORTS_JACK_DETECTION
static void max98090_headset_button_event(struct snd_soc_codec *codec)
{
	dev_info(codec->dev, "max98090_headset_button_event\n");
}
	
static void max98090_jack_work(struct work_struct *work)
{
	struct max98090_priv *max98090 = container_of(work,
		struct max98090_priv,
		jack_work.work);
	struct snd_soc_codec *codec = max98090->codec;
	int reg;

	/* Read a second time */
	if (max98090->jack_state == M98090_JACK_STATE_NO_HEADSET) {
		
		snd_soc_update_bits(codec, M98090_REG_3E_PWR_EN_IN,
			M98090_PWR_MBEN_MASK, 0);

		/* Strong pull up allows mic detection */
		snd_soc_update_bits(codec, M98090_REG_3D_CFG_JACK,
			M98090_JDWK_MASK, 0);
		
		msleep(50);

		reg = snd_soc_read(codec, M98090_REG_02_JACK_STATUS);
		
		/* Weak pull up allows only insertion detection */
		snd_soc_update_bits(codec, M98090_REG_3D_CFG_JACK,
			M98090_JDWK_MASK, M98090_JDWK_MASK);
	} else {
		reg = snd_soc_read(codec, M98090_REG_02_JACK_STATUS);
	}
	
	reg = snd_soc_read(codec, M98090_REG_02_JACK_STATUS);
	
	switch (reg & (M98090_LSNS_MASK | M98090_JKSNS_MASK)) {
		case M98090_LSNS_MASK | M98090_JKSNS_MASK:
		{
			dev_info(codec->dev, "No Headset Detected\n");

			max98090->jack_state = M98090_JACK_STATE_NO_HEADSET;

			max98090_set_bias_level(codec, SND_SOC_BIAS_STANDBY);

			snd_soc_dapm_disable_pin(&codec->dapm, "HPL");
			snd_soc_dapm_disable_pin(&codec->dapm, "HPR");
			snd_soc_dapm_force_enable_pin(&codec->dapm, "SPKL");
			snd_soc_dapm_force_enable_pin(&codec->dapm, "SPKR");
			snd_soc_dapm_disable_pin(&codec->dapm, "MIC1");
			snd_soc_dapm_disable_pin(&codec->dapm, "MIC2");
			snd_soc_dapm_force_enable_pin(&codec->dapm, "DMIC1");
			snd_soc_dapm_force_enable_pin(&codec->dapm, "DMIC2");
			max98090_dmic_switch(codec, 1);
			
			break;
		}

		case 0:
		{
			if (max98090->jack_state ==
				M98090_JACK_STATE_HEADSET) {

				dev_info(codec->dev,
					"Headset Button Down Detected\n");

				max98090_headset_button_event(codec);
				
				return;
			}
			
			/* Line is reported as Headphone */
			/* Nokia Headset is reported as Headphone */
			/* Mono Headphone is reported as Headphone */
			dev_info(codec->dev, "Headphone Detected\n");
			
			max98090->jack_state = M98090_JACK_STATE_HEADPHONE;

			max98090_set_bias_level(codec, SND_SOC_BIAS_STANDBY);

			snd_soc_dapm_disable_pin(&codec->dapm, "SPKL");
			snd_soc_dapm_disable_pin(&codec->dapm, "SPKR");
			snd_soc_dapm_force_enable_pin(&codec->dapm, "HPL");
			snd_soc_dapm_force_enable_pin(&codec->dapm, "HPR");
			snd_soc_dapm_disable_pin(&codec->dapm, "MIC1");
			snd_soc_dapm_disable_pin(&codec->dapm, "MIC2");
			snd_soc_dapm_force_enable_pin(&codec->dapm, "DMIC1");
			snd_soc_dapm_force_enable_pin(&codec->dapm, "DMIC2");
			max98090_dmic_switch(codec, 1);

			break;
		}
		
		case M98090_JKSNS_MASK:
		{
			dev_info(codec->dev, "Headset Detected\n");
			
			max98090->jack_state = M98090_JACK_STATE_HEADSET;

			max98090_set_bias_level(codec, SND_SOC_BIAS_ON);

			snd_soc_dapm_disable_pin(&codec->dapm, "SPKL");
			snd_soc_dapm_disable_pin(&codec->dapm, "SPKR");
			snd_soc_dapm_force_enable_pin(&codec->dapm, "HPL");
			snd_soc_dapm_force_enable_pin(&codec->dapm, "HPR");
			snd_soc_dapm_disable_pin(&codec->dapm, "DMIC1");
			snd_soc_dapm_disable_pin(&codec->dapm, "DMIC2");
			snd_soc_dapm_force_enable_pin(&codec->dapm, "MIC1");
			snd_soc_dapm_force_enable_pin(&codec->dapm, "MIC2");
			max98090_dmic_switch(codec, 0);

			break;
		}
		
		default:
		{
			dev_info(codec->dev, "Unrecognized Jack Status\n");
			break;
		}
	}
}

static irqreturn_t max98090_interrupt(int irq, void *data)
{
	struct snd_soc_codec *codec = data;
	struct max98090_priv *max98090 = snd_soc_codec_get_drvdata(codec);
	int ret;
	unsigned int mask;
	unsigned int active;

	dev_info(codec->dev, "***** max98090_interrupt *****\n");

	ret = regmap_read(max98090->regmap, M98090_REG_03_IRQ_ENABLE, &mask);
	
	if (ret != 0) {
		dev_err(codec->dev,
			"failed to read M98090_REG_03_IRQ_ENABLE: %d\n",
			ret);
		return IRQ_NONE;
	}

	ret = regmap_read(max98090->regmap, M98090_REG_01_IRQ_STATUS, &active);

	if (ret != 0) {
		dev_err(codec->dev,
			"failed to read M98090_REG_01_IRQ_STATUS: %d\n",
			ret);
		return IRQ_NONE;
	}

	dev_info(codec->dev, "active=0x%02x mask=0x%02x -> active=0x%02x\n",
		active, mask, active & mask);

	active &= mask;

	if (!active)
		return IRQ_NONE;
	
	/* Send work to be scheduled */
	if (active & M98090_IRQ_CLD_MASK) {
		dev_info(codec->dev, "M98090_IRQ_CLD_MASK\n");
	}

	if (active & M98090_IRQ_SLD_MASK) {
		dev_info(codec->dev, "M98090_IRQ_SLD_MASK\n");
	}

	if (active & M98090_IRQ_ULK_MASK) {
		dev_info(codec->dev, "M98090_IRQ_ULK_MASK\n");
	}

	if (active & M98090_IRQ_JDET_MASK) {
		dev_info(codec->dev, "M98090_IRQ_JDET_MASK\n");

/*		trace_snd_soc_jack_irq(dev_name(dev)); */
	/* All headphone or headset detection will be handled by max97236 amp
		pm_wakeup_event(codec->dev, 100);

		schedule_delayed_work(&max98090->jack_work,
			msecs_to_jiffies(100));
	*/
	}

	if (active & M98090_IRQ_ALCACT_MASK) {
		dev_info(codec->dev, "M98090_IRQ_ALCACT_MASK\n");
	}

	if (active & M98090_IRQ_ALCCLP_MASK) {
		dev_info(codec->dev, "M98090_IRQ_ALCCLP_MASK\n");
	}

	return IRQ_HANDLED;
}
#endif

#define MAX98090_RATES SNDRV_PCM_RATE_8000_96000
#define MAX98090_FORMATS (SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S24_LE)

static struct snd_soc_dai_ops max98090_dai_ops = {
	.set_sysclk = max98090_dai_set_sysclk,
	.set_fmt = max98090_dai_set_fmt,
	.set_tdm_slot = max98090_set_tdm_slot,
	.hw_params = max98090_dai_hw_params,
	.digital_mute = max98090_dai_digital_mute,
};

static struct snd_soc_dai_driver max98090_dai[] = {
{
	.name = "HiFi",
	.playback = {
		.stream_name = "HiFi Playback",
		.channels_min = 2,
		.channels_max = 2,
		.rates = MAX98090_RATES,
		.formats = MAX98090_FORMATS,
	},
	.capture = {
		.stream_name = "HiFi Capture",
		.channels_min = 1,
		.channels_max = 2,
		.rates = MAX98090_RATES,
		.formats = MAX98090_FORMATS,
	},
	 .ops = &max98090_dai_ops,
}
};

/*
 * Equalizer filter coefs generated from the MAXIM MAX98090
 * Evaluation Kit (EVKIT) software tool
 */
static struct max98090_eq_cfg eq_cfg[] = {
	{ /* Flat response */
	.name = "FLAT",
	.rate = 48000,
	.bands = 7,
	.coef = {
		0x00, 0x00, 0x00,
		0x00, 0x00, 0x00,
		0x10, 0x00, 0x00,
		0x00, 0x00, 0x00,
		0x00, 0x00, 0x00,

		0x00, 0x00, 0x00,
		0x00, 0x00, 0x00,
		0x10, 0x00, 0x00,
		0x00, 0x00, 0x00,
		0x00, 0x00, 0x00,

		0x00, 0x00, 0x00,
		0x00, 0x00, 0x00,
		0x10, 0x00, 0x00,
		0x00, 0x00, 0x00,
		0x00, 0x00, 0x00,

		0x00, 0x00, 0x00,
		0x00, 0x00, 0x00,
		0x10, 0x00, 0x00,
		0x00, 0x00, 0x00,
		0x00, 0x00, 0x00,

		0x00, 0x00, 0x00,
		0x00, 0x00, 0x00,
		0x10, 0x00, 0x00,
		0x00, 0x00, 0x00,
		0x00, 0x00, 0x00,

		0x00, 0x00, 0x00,
		0x00, 0x00, 0x00,
		0x10, 0x00, 0x00,
		0x00, 0x00, 0x00,
		0x00, 0x00, 0x00,

		0x00, 0x00, 0x00,
		0x00, 0x00, 0x00,
		0x10, 0x00, 0x00,
		0x00, 0x00, 0x00,
		0x00, 0x00, 0x00,
		},
	},
	{ /* Low pass Fc=4KHz */
	.name = "LOWPASS",
	.rate = 48000,
	.bands = 7,
	.coef = {
		0xC2, 0x51, 0x87,
		0xB2, 0x46, 0xD4,
		0xED, 0x09, 0x65,
		0xC2, 0x51, 0x87,
		0xD5, 0x3D, 0x70,

		0xF3, 0x6A, 0xD6,
		0xE3, 0x62, 0x32,
		0xFF, 0x5B, 0x1E,
		0xF3, 0x6A, 0xD6,
		0xF4, 0x07, 0x14,

		0xC2, 0x51, 0x87,
		0xB2, 0x46, 0xD4,
		0xED, 0x09, 0x65,
		0xC2, 0x51, 0x87,
		0xD5, 0x3D, 0x70,

		0xC2, 0x51, 0x87,
		0xB2, 0x46, 0xD4,
		0xED, 0x09, 0x65,
		0xC2, 0x51, 0x87,
		0xD5, 0x3D, 0x70,

		0xC2, 0x51, 0x87,
		0xB2, 0x46, 0xD4,
		0xED, 0x09, 0x65,
		0xC2, 0x51, 0x87,
		0xD5, 0x3D, 0x70,

		0xC2, 0x51, 0x87,
		0xB2, 0x46, 0xD4,
		0xED, 0x09, 0x65,
		0xC2, 0x51, 0x87,
		0xD5, 0x3D, 0x70,

		0xC2, 0x51, 0x87,
		0xB2, 0x46, 0xD4,
		0xED, 0x09, 0x65,
		0xC2, 0x51, 0x87,
		0xD5, 0x3D, 0x70,
		},
	},
	{ /* Bass and Treble compensation for -30dB volume */
	.name = "SMARTVOL",
	.rate = 48000,
	.bands = 7,
	.coef = {
		0xE0, 0x0D, 0x96,
		0x0F, 0xF2, 0x71,
		0x10, 0x09, 0xFD,
		0xE0, 0x0D, 0x96,
		0x0F, 0xE8, 0x74,

		0xE0, 0x3B, 0x3C,
		0x0F, 0xC4, 0xDD,
		0x0F, 0xF7, 0x00,
		0xE0, 0x3B, 0x3C,
		0x0F, 0xCD, 0xDC,

		0xE0, 0x41, 0x8B,
		0x0F, 0xBE, 0x7D,
		0x10, 0x62, 0x24,
		0xE0, 0x41, 0x8B,
		0x0F, 0x5C, 0x58,

		0xE0, 0x34, 0x76,
		0x0F, 0xCB, 0x91,
		0x10, 0x07, 0x71,
		0xE0, 0x34, 0x76,
		0x0F, 0xC4, 0x1F,

		0xE1, 0x44, 0xA2,
		0x0E, 0xBF, 0x1F,
		0x0F, 0xDA, 0x84,
		0xE1, 0x44, 0xA2,
		0x0E, 0xE4, 0x9B,

		0xF5, 0xCA, 0x0A,
		0xE5, 0xCA, 0x0A,
		0x18, 0x1C, 0x9F,
		0xF5, 0xCA, 0x0A,
		0xDD, 0xAD, 0x6B,

		0xEB, 0x83, 0x70,
		0x08, 0x44, 0x1A,
		0x0F, 0xD8, 0xE1,
		0xEB, 0x83, 0x70,
		0x08, 0x6B, 0x38,
		},
	},
};

static struct max98090_biquad_cfg bq_cfg[] = {
	{
	.name = "FLAT",
	.rate = 48000,
	.coef = {
		0xE0, 0x12, 0x89,
		0x0F, 0xEE, 0x2A,
		0x10, 0x00, 0x00,
		0xE0, 0x12, 0x89,
		0x0F, 0xEE, 0x2A,
		},
	},
	{
	.name = "LOWPASS",
	.rate = 48000,
	.coef = {
		0x06, 0x5B, 0x0E,
		0xF6, 0x5C, 0x2A,
		0x06, 0x6A, 0xE7,
		0x06, 0x5B, 0x0E,
		0xFF, 0xF1, 0x42,
		},
	},
};

static struct max98090_biquad_cfg dmic34bq_cfg[] = {
	{
	.name = "FLAT",
	.rate = 48000,
	.coef = {
		0xE0, 0x12, 0x89,
		0x0F, 0xEE, 0x2A,
		0x10, 0x00, 0x00,
		0xE0, 0x12, 0x89,
		0x0F, 0xEE, 0x2A,
		},
	},
	{
	.name = "LOWPASS",
	.rate = 48000,
	.coef = {
		0x06, 0x5B, 0x0E,
		0xF6, 0x5C, 0x2A,
		0x06, 0x6A, 0xE7,
		0x06, 0x5B, 0x0E,
		0xFF, 0xF1, 0x42,
		},
	},
};

static int max98090_write_eq_data(struct snd_soc_codec *codec, int config)
{
	struct max98090_priv *max98090 = snd_soc_codec_get_drvdata(codec);
	struct max98090_pdata *pdata = max98090->pdata;
	struct max98090_cdata *cdata;
	struct max98090_eq_cfg *coef_set;
	int i;
	int regmask, regsave;
	int num_bands;
	u8 *coefs;
	unsigned int reg;

	cdata = &max98090->dai[0];
	
	coef_set = &pdata->eq_cfg[config];

	regmask = M98090_EQ3BANDEN_MASK |
			  M98090_EQ5BANDEN_MASK |
			  M98090_EQ7BANDEN_MASK;

	num_bands = coef_set->bands;
	if (num_bands > 7)
		return -EINVAL;

	/* Disable filter while configuring, and save current on/off state */
	regsave = snd_soc_read(codec, M98090_REG_41_DSP_EQ_EN);
	snd_soc_update_bits(codec, M98090_REG_41_DSP_EQ_EN, regmask, 0);

	/*
	 * Load equalizer DSP coefficient configurations registers.
	 * Expected sequence is: B0, B1, B2, A1, A2
	 * Each coef is 3 bytes in the register order of [23:16] [15:8] [7:0]
	 */
	coefs = coef_set->coef;
	reg = M98090_REG_46_EQ_BASE;
	for (i = 0; i < M98090_COEFS_BLK_SZ * num_bands; i++) {
		snd_soc_write(codec, reg++, coefs[i]);
	}

	/* Restore the original on/off state */
	snd_soc_update_bits(codec, M98090_REG_41_DSP_EQ_EN, regmask, regsave);
	
	return 0;
}

static int max98090_put_eq_enum(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct max98090_priv *max98090 = snd_soc_codec_get_drvdata(codec);
	struct max98090_pdata *pdata = max98090->pdata;
	struct max98090_cdata *cdata;
	int sel = ucontrol->value.integer.value[0];
	struct max98090_eq_cfg *coef_set;
	int fs, best, best_val, i;
	int num_bands;

	cdata = &max98090->dai[0];

	if (sel >= pdata->eq_cfgcnt)
		return -EINVAL;

	cdata->eq_sel = sel;

	if (!pdata || !max98090->eq_textcnt)
		return 0;

	fs = cdata->rate;

	/* Find the selected configuration with nearest sample rate */
	best = 0;
	best_val = INT_MAX;
	for (i = 0; i < pdata->eq_cfgcnt; i++) {
		if (strcmp(pdata->eq_cfg[i].name,
			max98090->eq_texts[sel]) == 0 &&
			abs(pdata->eq_cfg[i].rate - fs) < best_val) {
			best = i;
			best_val = abs(pdata->eq_cfg[i].rate - fs);
		}
	}

	dev_dbg(codec->dev, "Selected %s/%dHz for %dHz sample rate\n",
		pdata->eq_cfg[best].name,
		pdata->eq_cfg[best].rate, fs);

	coef_set = &pdata->eq_cfg[best];

	num_bands = coef_set->bands;
	if (num_bands > 7)
		return -EINVAL;

	/*
	 * Remember the current number of bands in the current preset
	 * so that the EQ Switch can activate the correct EQ band
	 * operating mode
	 */
	cdata->eq_num_bands = num_bands;

	max98090_write_eq_data(codec, best);
	
	return 0;
}

static int max98090_get_eq_enum(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct max98090_priv *max98090 = snd_soc_codec_get_drvdata(codec);
	struct max98090_cdata *cdata;

	cdata = &max98090->dai[0];
	ucontrol->value.enumerated.item[0] = cdata->eq_sel;

	return 0;
}

static void max98090_handle_eq_pdata(struct snd_soc_codec *codec)
{
	struct max98090_priv *max98090 = snd_soc_codec_get_drvdata(codec);
	struct max98090_pdata *pdata = max98090->pdata;
	struct max98090_eq_cfg *cfg;
	unsigned int cfgcnt;
	int i, j;
	const char **t;
	int ret;

	struct snd_kcontrol_new controls[] = {
		SOC_ENUM_EXT("EQ Mode",
			max98090->eq_enum,
			max98090_get_eq_enum,
			max98090_put_eq_enum),
	};

	cfg = pdata->eq_cfg;
	cfgcnt = pdata->eq_cfgcnt;

	/* Set up an array of texts for the equalizer enum. */
	max98090->eq_textcnt = 0;
	max98090->eq_texts = NULL;
	for (i = 0; i < cfgcnt; i++) {
		for (j = 0; j < max98090->eq_textcnt; j++) {
			if (strcmp(cfg[i].name, max98090->eq_texts[j]) == 0)
				break;
		}

		if (j != max98090->eq_textcnt)
			continue;

		/* Expand the array */
		t = krealloc(max98090->eq_texts,
				 sizeof(char *) * (max98090->eq_textcnt + 1),
				 GFP_KERNEL);
		if (t == NULL)
			continue;

		/* Store the new entry */
		t[max98090->eq_textcnt] = cfg[i].name;
		max98090->eq_textcnt++;
		max98090->eq_texts = t;
	}

	/* Now point the soc_enum to .texts array items */
	max98090->eq_enum.texts = max98090->eq_texts;
	max98090->eq_enum.max = max98090->eq_textcnt;

	ret = snd_soc_add_codec_controls(codec, controls,
		ARRAY_SIZE(controls));
	if (ret != 0)
		dev_err(codec->dev, "Failed to add EQ control: %d\n", ret);
}

static int max98090_write_bq_data(struct snd_soc_codec *codec, int config)
{
	struct max98090_priv *max98090 = snd_soc_codec_get_drvdata(codec);
	struct max98090_pdata *pdata = max98090->pdata;
	struct max98090_biquad_cfg *coef_set;
	int i;
	int regsave;
	u8 *coefs;
	unsigned int reg;
	
	coef_set = &pdata->bq_cfg[config];

	/* Disable filter while configuring, and save current on/off state */
	regsave = snd_soc_read(codec, M98090_REG_41_DSP_EQ_EN);
	snd_soc_update_bits(codec, M98090_REG_41_DSP_EQ_EN,
		M98090_ADCBQEN_MASK, 0);

	/* Load equalizer DSP coefficient configurations registers.
	 * Expected sequence is: B0, B1, B2, A1, A2
	 * Each coef is 3 bytes in the register order of [23:16] [15:8] [7:0]
	 */
	coefs = coef_set->coef;
	reg = M98090_REG_AF_BIQUAD_BASE;
	for (i = 0; i < M98090_COEFS_BLK_SZ; i++) {
		snd_soc_write(codec, reg++, coefs[i]);
	}

	/* Restore the original on/off state */
	snd_soc_update_bits(codec, M98090_REG_41_DSP_EQ_EN,
		M98090_ADCBQEN_MASK, regsave);

	return 0;
}

static int max98090_put_bq_enum(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct max98090_priv *max98090 = snd_soc_codec_get_drvdata(codec);
	struct max98090_pdata *pdata = max98090->pdata;
	struct max98090_cdata *cdata;
	int sel = ucontrol->value.integer.value[0];
	int fs, best, best_val, i;

	cdata = &max98090->dai[0];

	if (sel >= pdata->bq_cfgcnt)
		return -EINVAL;

	cdata->bq_sel = sel;

	if (!pdata || !max98090->bq_textcnt)
		return 0;

	fs = cdata->rate;

	/* Find the selected configuration with nearest sample rate */
	best = 0;
	best_val = INT_MAX;
	for (i = 0; i < pdata->bq_cfgcnt; i++) {
		if (strcmp(pdata->bq_cfg[i].name,
			max98090->bq_texts[sel]) == 0 &&
			abs(pdata->bq_cfg[i].rate - fs) < best_val) {
			best = i;
			best_val = abs(pdata->bq_cfg[i].rate - fs);
		}
	}

	dev_dbg(codec->dev, "Selected %s/%dHz for %dHz sample rate\n",
		pdata->bq_cfg[best].name,
		pdata->bq_cfg[best].rate, fs);

	max98090_write_bq_data(codec, best);

	return 0;
}

static int max98090_get_bq_enum(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct max98090_priv *max98090 = snd_soc_codec_get_drvdata(codec);
	struct max98090_cdata *cdata;

	cdata = &max98090->dai[0];
	ucontrol->value.enumerated.item[0] = cdata->bq_sel;

	return 0;
}

static void max98090_handle_bq_pdata(struct snd_soc_codec *codec)
{
	struct max98090_priv *max98090 = snd_soc_codec_get_drvdata(codec);
	struct max98090_pdata *pdata = max98090->pdata;
	struct max98090_biquad_cfg *cfg;
	unsigned int cfgcnt;
	int i, j;
	const char **t;
	int ret;

	struct snd_kcontrol_new controls[] = {
		SOC_ENUM_EXT("Biquad Mode",
			max98090->bq_enum,
			max98090_get_bq_enum,
			max98090_put_bq_enum),
	};

	cfg = pdata->bq_cfg;
	cfgcnt = pdata->bq_cfgcnt;

	/*
	 * Set up an array of texts for the biquad enum.
	 * This is based on Mark Brown's equalizer driver code.
	 */
	max98090->bq_textcnt = 0;
	max98090->bq_texts = NULL;
	for (i = 0; i < cfgcnt; i++) {
		for (j = 0; j < max98090->bq_textcnt; j++) {
			if (strcmp(cfg[i].name, max98090->bq_texts[j]) == 0)
				break;
		}

		if (j != max98090->bq_textcnt)
			continue;

		/* Expand the array */
		t = krealloc(max98090->bq_texts,
				 sizeof(char *) * (max98090->bq_textcnt + 1),
				 GFP_KERNEL);
		if (t == NULL)
			continue;

		/* Store the new entry */
		t[max98090->bq_textcnt] = cfg[i].name;
		max98090->bq_textcnt++;
		max98090->bq_texts = t;
	}

	/* Now point the soc_enum to .texts array items */
	max98090->bq_enum.texts = max98090->bq_texts;
	max98090->bq_enum.max = max98090->bq_textcnt;

	ret = snd_soc_add_codec_controls(codec, controls,
		ARRAY_SIZE(controls));
	if (ret != 0)
		dev_err(codec->dev,
			"Failed to add Biquad control: %d\n", ret);
}

static int max98090_write_dmic34bq_data(struct snd_soc_codec *codec,
	int config)
{
	struct max98090_priv *max98090 = snd_soc_codec_get_drvdata(codec);
	struct max98090_pdata *pdata = max98090->pdata;
	struct max98090_biquad_cfg *coef_set;
	int i;
	int regsave;
	u8 *coefs;
	unsigned int reg;

	coef_set = &pdata->dmic34bq_cfg[config];

	/* Disable filter while configuring, and save current on/off state */
	regsave = snd_soc_read(codec, M98090_REG_41_DSP_EQ_EN);
	snd_soc_update_bits(codec, M98090_REG_41_DSP_EQ_EN,
		M98090_DMIC34BQEN_MASK, 0);

	/*
	 * Load equalizer DSP coefficient configurations registers.
	 * Expected sequence is: B0, B1, B2, A1, A2
	 * Each coef is 3 bytes in the register order of [23:16] [15:8] [7:0]
	 */
	coefs = coef_set->coef;
	reg = M98090_REG_C3_DMIC34_BIQUAD_BASE;
	for (i = 0; i < M98090_COEFS_BLK_SZ; i++) {
		snd_soc_write(codec, reg++, coefs[i]);
	}

	/* Restore the original on/off state */
	snd_soc_update_bits(codec, M98090_REG_41_DSP_EQ_EN,
		M98090_DMIC34BQEN_MASK, regsave);

	return 0;
}

static int max98090_put_dmic34bq_enum(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct max98090_priv *max98090 = snd_soc_codec_get_drvdata(codec);
	struct max98090_pdata *pdata = max98090->pdata;
	struct max98090_cdata *cdata;
	int sel = ucontrol->value.integer.value[0];
	int fs, best, best_val, i;

	cdata = &max98090->dai[0];

	if (sel >= pdata->dmic34bq_cfgcnt)
		return -EINVAL;

	cdata->dmic34bq_sel = sel;

	if (!pdata || !max98090->dmic34bq_textcnt)
		return 0;

	fs = cdata->rate;

	/* Find the selected configuration with nearest sample rate */
	best = 0;
	best_val = INT_MAX;
	for (i = 0; i < pdata->dmic34bq_cfgcnt; i++) {
		if (strcmp(pdata->dmic34bq_cfg[i].name,
			max98090->dmic34bq_texts[sel]) == 0 &&
			abs(pdata->dmic34bq_cfg[i].rate - fs) < best_val) {
			best = i;
			best_val = abs(pdata->dmic34bq_cfg[i].rate - fs);
		}
	}

	dev_dbg(codec->dev, "Selected %s/%dHz for %dHz sample rate\n",
		pdata->dmic34bq_cfg[best].name,
		pdata->dmic34bq_cfg[best].rate, fs);

	max98090_write_dmic34bq_data(codec, best);

	return 0;
}

static int max98090_get_dmic34bq_enum(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct max98090_priv *max98090 = snd_soc_codec_get_drvdata(codec);
	struct max98090_cdata *cdata;

	cdata = &max98090->dai[0];
	ucontrol->value.enumerated.item[0] = cdata->dmic34bq_sel;

	return 0;
}

static void max98090_handle_dmic34bq_pdata(struct snd_soc_codec *codec)
{
	struct max98090_priv *max98090 = snd_soc_codec_get_drvdata(codec);
	struct max98090_pdata *pdata = max98090->pdata;
	struct max98090_biquad_cfg *cfg;
	unsigned int cfgcnt;
	int i, j;
	const char **t;
	int ret;

	struct snd_kcontrol_new controls[] = {
		SOC_ENUM_EXT("DMIC34 Biquad Mode",
			max98090->dmic34bq_enum,
			max98090_get_dmic34bq_enum,
			max98090_put_dmic34bq_enum),
	};

	cfg = pdata->dmic34bq_cfg;
	cfgcnt = pdata->dmic34bq_cfgcnt;

	/*
	 * Set up an array of texts for the biquad enum.
	 * This is based on Mark Brown's equalizer driver code.
	 */
	max98090->dmic34bq_textcnt = 0;
	max98090->dmic34bq_texts = NULL;
	for (i = 0; i < cfgcnt; i++) {
		for (j = 0; j < max98090->dmic34bq_textcnt; j++) {
			if (strcmp(cfg[i].name,
				max98090->dmic34bq_texts[j]) == 0)
				break;
		}

		if (j != max98090->dmic34bq_textcnt)
			continue;

		/* Expand the array */
		t = krealloc(max98090->dmic34bq_texts,
				sizeof(char *) *
				(max98090->dmic34bq_textcnt + 1),
				GFP_KERNEL);
		if (t == NULL)
			continue;

		/* Store the new entry */
		t[max98090->dmic34bq_textcnt] = cfg[i].name;
		max98090->dmic34bq_textcnt++;
		max98090->dmic34bq_texts = t;
	}

	/* Now point the soc_enum to .texts array items */
	max98090->dmic34bq_enum.texts = max98090->dmic34bq_texts;
	max98090->dmic34bq_enum.max = max98090->dmic34bq_textcnt;

	ret = snd_soc_add_codec_controls(codec, controls,
		ARRAY_SIZE(controls));
	if (ret != 0)
		dev_err(codec->dev,
			"Failed to add DMIC34 Biquad control: %d\n", ret);
}

static void max98090_handle_pdata(struct snd_soc_codec *codec)
{
	struct max98090_priv *max98090 = snd_soc_codec_get_drvdata(codec);
	struct max98090_pdata *pdata = max98090->pdata;

	if (!pdata) {
		dev_dbg(codec->dev, "No platform data\n");
		return;
	}

	max98090_dmic_switch(codec, 1);

	pdata->eq_cfg = eq_cfg;
	pdata->eq_cfgcnt = ARRAY_SIZE(eq_cfg);
	pdata->bq_cfg = bq_cfg;
	pdata->bq_cfgcnt = ARRAY_SIZE(bq_cfg);
	pdata->dmic34bq_cfg = dmic34bq_cfg;
	pdata->dmic34bq_cfgcnt = ARRAY_SIZE(dmic34bq_cfg);
	
	/* Configure equalizer */
	if (pdata->eq_cfgcnt) {
		max98090_handle_eq_pdata(codec);
		max98090_write_eq_data(codec, 0);
	}

	/* Configure bi-quad filters */
	if (pdata->bq_cfgcnt) {
		max98090_handle_bq_pdata(codec);
		max98090_write_bq_data(codec, 0);
	}
	
	/* Configure DMIC34 bi-quad filters */
	if ((max98090->devtype == MAX98091) &&	
		(pdata->dmic34bq_cfgcnt)) {
		max98090_handle_dmic34bq_pdata(codec);
		max98090_write_dmic34bq_data(codec, 0);
	}
}

static int max98090_probe(struct snd_soc_codec *codec)
{
	struct max98090_priv *max98090 = snd_soc_codec_get_drvdata(codec);
	struct max98090_pdata *pdata = max98090->pdata;
	struct max98090_cdata *cdata;
	int ret = 0;

	dev_info(codec->dev, "max98090_probe\n");

	max98090->codec = codec;

	ret = snd_soc_codec_set_cache_io(codec, 8, 8, SND_SOC_I2C);
	codec->control_data = max98090->regmap;

	if (ret != 0) {
		dev_err(codec->dev, "Failed to set cache I/O: %d\n", ret);
		return ret;
	}

	/* Reset the codec, the DSP core, and disable all interrupts */
	max98090_reset(max98090);

	/* Initialize private data */

	max98090->sysclk = (unsigned)-1;
	max98090->eq_textcnt = 0;
	max98090->bq_textcnt = 0;

	cdata = &max98090->dai[0];
	cdata->rate = (unsigned)-1;
	cdata->fmt  = (unsigned)-1;
	cdata->eq_sel = 0;
	cdata->bq_sel = 0;
	cdata->dmic34bq_sel = 0;

	max98090->lin_state = 0;
	max98090->mic1pre = 0;
	max98090->mic2pre = 0;
	max98090->extmic_mux = 0;

	ret = snd_soc_read(codec, M98090_REG_FF_REV_ID);
	if (ret < 0) {
		dev_err(codec->dev, "Failed to read device revision: %d\n",
			ret);
		goto err_access;
	}

	if ((ret >= M98090_REVA) && (ret <= M98090_REVA + 0x0f)) {
		max98090->devtype = MAX98090;
		dev_info(codec->dev, "MAX98090 REVID=0x%02x\n", ret);
	} else if ((ret >= M98091_REVA) && (ret <= M98091_REVA + 0x0f)) {
		max98090->devtype = MAX98091;
		dev_info(codec->dev, "MAX98091 REVID=0x%02x\n", ret);
	} else {
		max98090->devtype = MAX98090;
		dev_err(codec->dev, "Unrecognized revision 0x%02x\n", ret);
	}
	
	max98090_add_widgets(codec);

#ifdef MAX98090_SUPPORTS_JACK_DETECTION
	max98090->jack_state = M98090_JACK_STATE_NO_HEADSET;
	INIT_DELAYED_WORK(&max98090->jack_work, max98090_jack_work);
	
	/* All headphone or headset detection will be handled by max97236 amp */
	snd_soc_write(codec, M98090_REG_3D_CFG_JACK,
		M98090_JDETEN_MASK | M98090_JDEB_25MS);
	dev_info(codec->dev, "irq = %d\n", pdata->irq);
	if ( (request_threaded_irq(pdata->irq, NULL,
		max98090_interrupt, IRQF_TRIGGER_FALLING,
		"max98090_interrupt", codec)) < 0) {
		dev_info(codec->dev, "request_irq failed\n");
	}
	/* 
	 * Clear any old interrupts.
	 * An old interrupt ocurring prior to installing the ISR
	 * can keep a new interrupt from generating a trigger.
	 */
	snd_soc_read(codec, M98090_REG_01_IRQ_STATUS);
#endif

	if (pdata->power_over_performance) {
		/* Low Power */
		snd_soc_update_bits(codec, M98090_REG_43_DAC_CFG,
			M98090_DACHP_MASK,
			0 << M98090_DACHP_SHIFT);
		snd_soc_update_bits(codec, M98090_REG_43_DAC_CFG,
			M98090_DAC_PERFMODE_MASK,
			1 << M98090_DAC_PERFMODE_SHIFT);
		snd_soc_update_bits(codec, M98090_REG_44_ADC_CFG,
			M98090_ADCHP_MASK,
			0 << M98090_ADCHP_SHIFT);
	} else {
		/* High Performance */
		snd_soc_update_bits(codec, M98090_REG_43_DAC_CFG,
			M98090_DACHP_MASK,
			1 << M98090_DACHP_SHIFT);
		snd_soc_update_bits(codec, M98090_REG_43_DAC_CFG,
			M98090_DAC_PERFMODE_MASK,
			0 << M98090_DAC_PERFMODE_SHIFT);
		snd_soc_update_bits(codec, M98090_REG_44_ADC_CFG,
			M98090_ADCHP_MASK,
			1 << M98090_ADCHP_SHIFT);
	}

	/* Turn on VCM bandgap reference */ 
	snd_soc_write(codec, M98090_REG_42_BIAS_CNTL,
		M98090_VCM_MODE_MASK);
	
	max98090_handle_pdata(codec);

err_access:
	return ret;
}

static int max98090_remove(struct snd_soc_codec *codec)
{
	struct max98090_priv *max98090 = snd_soc_codec_get_drvdata(codec);

#ifdef MAX98090_SUPPORTS_JACK_DETECTION
	cancel_delayed_work_sync(&max98090->jack_work);
	free_irq(max98090->irq, codec);
#endif
	kfree(max98090->bq_texts);
	kfree(max98090->eq_texts);

	return 0;
}

static struct snd_soc_codec_driver soc_codec_dev_max98090 = {
	.probe   = max98090_probe,
	.remove  = max98090_remove,
	.set_bias_level = max98090_set_bias_level,
};

static const struct regmap_config max98090_regmap = {
	.reg_bits = 8,
	.val_bits = 8,

	.max_register = MAX98090_MAX_REGISTER,
	.reg_defaults = max98090_reg,
	.num_reg_defaults = ARRAY_SIZE(max98090_reg),
	.volatile_reg = max98090_volatile_register,
	.readable_reg = max98090_readable_register,
	.cache_type = REGCACHE_RBTREE,
};

static int max98090_i2c_probe(struct i2c_client *i2c,
				 const struct i2c_device_id *id)
{
	struct max98090_priv *max98090;
	int ret;

	pr_info("%s\n", __func__);

	max98090 = kzalloc(sizeof(struct max98090_priv), GFP_KERNEL);
	if (max98090 == NULL)
		return -ENOMEM;

	max98090->devtype = id->driver_data;
	i2c_set_clientdata(i2c, max98090);
	max98090->control_data = i2c;
	max98090->pdata = i2c->dev.platform_data;

	max98090->regmap = regmap_init_i2c(i2c, &max98090_regmap);
	if (IS_ERR(max98090->regmap)) {
		ret = PTR_ERR(max98090->regmap);
		dev_err(&i2c->dev, "Failed to allocate regmap: %d\n", ret);
		goto err_enable;
	}

	ret = snd_soc_register_codec(&i2c->dev,
			&soc_codec_dev_max98090, max98090_dai,
			ARRAY_SIZE(max98090_dai));
	if (ret < 0)
		regmap_exit(max98090->regmap);

err_enable:
	return ret;
}

static int __devexit max98090_i2c_remove(struct i2c_client *client)
{
	struct max98090_priv *max98090 = dev_get_drvdata(&client->dev);
	snd_soc_unregister_codec(&client->dev);
	regmap_exit(max98090->regmap);
	kfree(i2c_get_clientdata(client));
	return 0;
}

static int max98090_runtime_resume(struct device *dev)
{
	struct max98090_priv *max98090 = dev_get_drvdata(dev);

	regcache_cache_only(max98090->regmap, false);

	max98090_reset(max98090);

	regcache_sync(max98090->regmap);

	return 0;
}

static int max98090_runtime_suspend(struct device *dev)
{
	struct max98090_priv *max98090 = dev_get_drvdata(dev);

	regcache_cache_only(max98090->regmap, true);

	return 0;
}

static struct dev_pm_ops max98090_pm = {
	SET_RUNTIME_PM_OPS(max98090_runtime_suspend,
		max98090_runtime_resume, NULL)
};

static const struct i2c_device_id max98090_i2c_id[] = {
	{ "max98090", MAX98090 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, max98090_i2c_id);

static struct i2c_driver max98090_i2c_driver = {
	.driver = {
		.name = "max98090",
		.owner = THIS_MODULE,
		.pm = &max98090_pm,
	},
	.probe  = max98090_i2c_probe,
	.remove = __devexit_p(max98090_i2c_remove),
	.id_table = max98090_i2c_id,
};

//module_i2c_driver(max98090_i2c_driver);

static int __init max98090_init(void)
{
       int ret;

       ret = i2c_add_driver(&max98090_i2c_driver);
       if (ret)
               pr_err("Failed to register max98088 I2C driver: %d\n", ret);

       return ret;
}
module_init(max98090_init);

static void __exit max98090_exit(void)
{
	i2c_del_driver(&max98090_i2c_driver);
}
module_exit(max98090_exit);



MODULE_DESCRIPTION("ALSA SoC MAX98090 driver");
MODULE_AUTHOR("Peter Hsiang, Jesse Marroqin, Jerry Wong");
MODULE_LICENSE("GPL");
