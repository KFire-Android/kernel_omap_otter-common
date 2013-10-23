/*
 * max97236.c  --  Audio Amplifier with Jack Detection for max97236
 *
 * Copyright (C) 2013 MM Solutions AD
 *
 * Author: Plamen Valev <pvalev@mm-sol.com>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 */
#define DEBUG

#include <linux/module.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/input.h>
#include <linux/delay.h>
#include <linux/wakelock.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/jack.h>
#include <sound/tlv.h>
#include <sound/max97236.h>
#ifdef CONFIG_AMAZON_METRICS_LOG
#include <linux/metricslog.h>
#endif
#include "max97236.h"

#define SND_JACK_BTN_ALL \
	(SND_JACK_BTN_0 | SND_JACK_BTN_1 | SND_JACK_BTN_2 | \
	 SND_JACK_BTN_3 | SND_JACK_BTN_4 | SND_JACK_BTN_5)

#define EXT_CLK_FREQ 19200000

/* time to wait before reporting begin HW debounce of MIC line state */
#define KEY_DELAY_DEBOUNCE_MS 200

/* Time to debounce MIC line state */
#define KEY_DEBOUNCE_MS 10

/* expect MIC-related spurious KEY event within this
 * time after HS insertion. This happens because the key
 * press/release is detected as specific voltage level across MIC/GND.
 * Pushing the button is shorting MIC through specific resistor.
 * Releasing is detected as voltage crossing some high margin.
 * It is also happening when we enable MIC BIAS for headset, and
 * MIC line is charged initially. */
#define KEY_WAIT_MIC_CHARGED_MS 100

static int detection_mode = MAX97236_ENABLE_2_AUTO_MANUAL;

struct max97236_priv {
	struct regmap *regmap;
	struct snd_soc_codec *codec;
	struct max97236_pdata *pdata;
	struct snd_soc_jack *jack;
	struct delayed_work jack_work;
	struct workqueue_struct *jack_workq;
	struct wake_lock wakelock;
	unsigned long plug_time;
	unsigned int last_key;
	unsigned char jack_status;
	int irq;
	void *control_data;
	bool resuming;
	unsigned char key_state;
	unsigned char status_1;
	unsigned char status_2;
	unsigned char status_3;
	unsigned char irq_mask_1;
	unsigned char irq_mask_2;
};

/* max97236 register cache */
static struct reg_default  max97236_reg[] = {
	{ MAX97236_STATUS_1, 0x00 },
	{ MAX97236_STATUS_2, 0x00 },
	{ MAX97236_STATUS_3, 0x00 },
	{ MAX97236_IRQ_MASK_1, 0x00 },
	{ MAX97236_IRQ_MASK_2, 0x00 },
	{ MAX97236_LEFT_VOLUME, 0x00 },
	{ MAX97236_RIGHT_VOLUME, 0x00 },
	{ MAX97236_MICROPHONE, 0x00 },
	{ MAX97236_VENDOR_ID, 0x00 },
	{ MAX97236_KEYSCAN_CLK_DIV_1, 0x00 },
	{ MAX97236_KEYSCAN_CLK_DIV_2, 0x00 },
	{ MAX97236_KEYSCAN_CLK_DIV_ADC, 0x00 },
	{ MAX97236_KEYSCAN_DEBOUNCE, 0x00 },
	{ MAX97236_KEYSCAN_DELAY, 0x00 },
	{ MAX97236_PASSIVE_MBH_KEYSCAN_DATA, 0x00 },
	{ MAX97236_DC_TEST_SLEW_CONTROL, 0x08 },
	{ MAX97236_STATE_FORCING, 0x01 },
	{ MAX97236_AC_TEST_CONTROL, 0x05 },
	{ MAX97236_ENABLE_1, 0x00 },
	{ MAX97236_ENABLE_2, 0x00 },

	{ MAX97236_PROGRAM_DETECTION, 0x00 },
	{ MAX97236_CUSTOM_DETECT_ENABLE, 0x00 },
	{ MAX97236_DETECTION_RESULTS_1, 0x00 },
	{ MAX97236_DETECTION_RESULTS_2, 0x00 },
	{ MAX97236_RUN_CUSTOM_DETECT, 0x00 },
};

static bool max97236_volatile_register(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case MAX97236_STATUS_1:
	case MAX97236_STATUS_2:
	case MAX97236_STATUS_3:
	case MAX97236_IRQ_MASK_1:
	case MAX97236_IRQ_MASK_2:
	case MAX97236_VENDOR_ID:
	case MAX97236_PASSIVE_MBH_KEYSCAN_DATA:
	case MAX97236_ENABLE_1:
	case MAX97236_ENABLE_2:
	case MAX97236_PROGRAM_DETECTION:
	case MAX97236_CUSTOM_DETECT_ENABLE:
	case MAX97236_DETECTION_RESULTS_1:
	case MAX97236_DETECTION_RESULTS_2:
	case MAX97236_RUN_CUSTOM_DETECT:
		return 1;
	default:
		return 0;
	}

	return 0;
}

static bool max97236_readable_register(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case MAX97236_STATUS_1:
	case MAX97236_STATUS_2:
	case MAX97236_STATUS_3:
	case MAX97236_IRQ_MASK_1:
	case MAX97236_IRQ_MASK_2:
	case MAX97236_LEFT_VOLUME:
	case MAX97236_RIGHT_VOLUME:
	case MAX97236_MICROPHONE:
	case MAX97236_VENDOR_ID:
	case MAX97236_KEYSCAN_CLK_DIV_1:
	case MAX97236_KEYSCAN_CLK_DIV_2:
	case MAX97236_KEYSCAN_CLK_DIV_ADC:
	case MAX97236_KEYSCAN_DEBOUNCE:
	case MAX97236_KEYSCAN_DELAY:
	case MAX97236_PASSIVE_MBH_KEYSCAN_DATA:
	case MAX97236_DC_TEST_SLEW_CONTROL:
	case MAX97236_STATE_FORCING:
	case MAX97236_AC_TEST_CONTROL:
	case MAX97236_ENABLE_1:
	case MAX97236_ENABLE_2:
	case MAX97236_PROGRAM_DETECTION:
	case MAX97236_CUSTOM_DETECT_ENABLE:
	case MAX97236_DETECTION_RESULTS_1:
	case MAX97236_DETECTION_RESULTS_2:
	case MAX97236_RUN_CUSTOM_DETECT:
		return 1;
	default:
		return 0;
	}

	return 0;
}

static const char * const max97236_mic_gain_text[] = { "12dB", "24dB" };
static const char * const max97236_mic_bias_vol_text[] = { "2.0V", "2.6V" };
static const char * const max97236_mic_bias_resistor_text[] = { "2.2kOm",
			"2.6kOm", "3.0kOm", "Bypassed", "High impedance" };
static const char * const max97236_enableddisabled_text[] = {
			"Enabled", "Disabled" };

static const struct soc_enum max97236_mic_gain_enum =
	SOC_ENUM_SINGLE(MAX97236_MICROPHONE, MAX97236_MICROPHONE_GAIN_SHIFT,
			ARRAY_SIZE(max97236_mic_gain_text),
			max97236_mic_gain_text);

static const struct soc_enum max97236_mic_bias_resistor_enum =
	SOC_ENUM_SINGLE(MAX97236_MICROPHONE, MAX97236_MICROPHONE_MICR_SHIFT,
			ARRAY_SIZE(max97236_mic_bias_resistor_text),
			max97236_mic_bias_resistor_text);

static const struct soc_enum max97236_mic_bias_vol_enum =
	SOC_ENUM_SINGLE(MAX97236_MICROPHONE, MAX97236_MICROPHONE_BIAS_SHIFT,
			ARRAY_SIZE(max97236_mic_bias_vol_text),
			max97236_mic_bias_vol_text);

static const struct soc_enum max97236_vol_slew_enum =
	SOC_ENUM_SINGLE(MAX97236_ENABLE_2, MAX97236_ENABLE_2_SVEN_SHIFT,
			ARRAY_SIZE(max97236_enableddisabled_text),
			max97236_enableddisabled_text);

static const struct soc_enum max97236_zero_cross_enum =
	SOC_ENUM_SINGLE(MAX97236_ENABLE_2, MAX97236_ENABLE_2_ZDEN_SHIFT,
			ARRAY_SIZE(max97236_enableddisabled_text),
			max97236_enableddisabled_text);

static const struct snd_kcontrol_new max97236_controls[] = {
SOC_SINGLE("Amp HPL Volume", MAX97236_LEFT_VOLUME,
		MAX97236_LEFT_VOLUME_LVOL_SHIFT, 0x3F, 0),
SOC_SINGLE("Amp HPR Volume", MAX97236_RIGHT_VOLUME,
		MAX97236_RIGHT_VOLUME_RVOL_SHIFT, 0x3F, 0),
SOC_SINGLE("Amp HPL Mute Switch", MAX97236_LEFT_VOLUME,
		MAX97236_LEFT_VOLUME_MUTEL_SHIFT, 1, 0),
SOC_SINGLE("Amp HPR Mute Switch", MAX97236_RIGHT_VOLUME,
		MAX97236_RIGHT_VOLUME_MUTER_SHIFT, 1, 0),

SOC_ENUM("Amp Mic gain", max97236_mic_gain_enum),
SOC_ENUM("Amp Mic bias resistor", max97236_mic_bias_resistor_enum),
SOC_ENUM("Amp Mic bias voltage", max97236_mic_bias_vol_enum),

SOC_ENUM("Amp Volume Adjustment Slewing", max97236_vol_slew_enum),
SOC_ENUM("Amp Zero-Crossing Detection", max97236_zero_cross_enum),

/* remove this controls. they are only for debuging */
SOC_SINGLE("Amp SHDN", MAX97236_ENABLE_1,
		MAX97236_ENABLE_1_SHDN_SHIFT, 1, 0),
SOC_SINGLE("Amp KS", MAX97236_ENABLE_1,
		MAX97236_ENABLE_1_KS_SHIFT, 1, 0),
SOC_SINGLE("Amp LFTEN", MAX97236_ENABLE_2,
		MAX97236_ENABLE_2_LFTEN_SHIFT, 1, 0),
SOC_SINGLE("Amp RGHEN", MAX97236_ENABLE_2,
		MAX97236_ENABLE_2_RGHEN_SHIFT, 1, 0),
SOC_SINGLE("Amp AC Repeat", MAX97236_AC_TEST_CONTROL,
		MAX97236_AC_TEST_AC_REPEAT_SHIFT, 3, 0),
SOC_SINGLE("Amp Pulse Width", MAX97236_AC_TEST_CONTROL,
		MAX97236_AC_TEST_PULSE_WIDTH_SHIFT, 3, 0),
SOC_SINGLE("Amp Pulse Amplitude", MAX97236_AC_TEST_CONTROL,
		MAX97236_AC_TEST_PULSE_AMP_SHIFT, 3, 0),

};

static const struct snd_soc_dapm_widget max97236_dapm_widgets[] = {
SND_SOC_DAPM_INPUT("AMP_INL"),
SND_SOC_DAPM_INPUT("AMP_INR"),
SND_SOC_DAPM_INPUT("AMP_MIC"),

SND_SOC_DAPM_SUPPLY("AMP_SHDN", MAX97236_ENABLE_1,
		MAX97236_ENABLE_1_SHDN_SHIFT, 0, NULL, 0),
SND_SOC_DAPM_SUPPLY("AMP_MIC_BIAS", MAX97236_ENABLE_1,
		MAX97236_ENABLE_1_MIC_BIAS_SHIFT, 0, NULL, 0),
SND_SOC_DAPM_SUPPLY("AMP_MIC_AMP", MAX97236_ENABLE_1,
		MAX97236_ENABLE_1_MIC_AMP_SHIFT, 0, NULL, 0),
SND_SOC_DAPM_SUPPLY("AMP_LFTEN", MAX97236_ENABLE_2,
		MAX97236_ENABLE_2_LFTEN_SHIFT, 0, NULL, 0),
SND_SOC_DAPM_SUPPLY("AMP_RGHEN", MAX97236_ENABLE_2,
		MAX97236_ENABLE_2_RGHEN_SHIFT, 0, NULL, 0),

SND_SOC_DAPM_OUTPUT("AMP_MOUT"),
SND_SOC_DAPM_OUTPUT("AMP_HPL"),
SND_SOC_DAPM_OUTPUT("AMP_HPR"),
};

static const struct snd_soc_dapm_route max97236_dapm_routes[] = {
		/* Headphone path */
		{ "AMP_HPL", NULL, "AMP_INL" },
		{ "AMP_HPR", NULL, "AMP_INR" },

		/* Microphone path */
		{ "AMP_MOUT", NULL, "AMP_MIC" },

		/* MIC -> enable chip */
		/* { "AMP_MIC", NULL, "AMP_SHDN" }, */

		/* HP out-> enable chip */
		/* { "AMP_HPL", NULL, "AMP_SHDN" }, */
		/* { "AMP_HPR", NULL, "AMP_SHDN" }, */
};

static int max97236_set_bias_level(struct snd_soc_codec *codec,
				  enum snd_soc_bias_level level)
{
	codec->dapm.bias_level = level;
	return 0;
}

static void max97236_key_report(struct max97236_priv *max97236,
		unsigned int key)
{
	struct snd_soc_codec *codec = max97236->codec;
	if (max97236->last_key == key)
		return;

	dev_info(codec->dev, "Report key pressed: 0x%04X\n", key);
	max97236->last_key = key;
	snd_soc_jack_report(max97236->jack, key, SND_JACK_BTN_ALL);
}

static void max97236_key_detect_report(struct max97236_priv *max97236)
{
	struct snd_soc_codec *codec = max97236->codec;
	int ret, key = 0;
	int adc_data;

	if (!(max97236->status_1 & MAX97236_STATUS_1_MBH_BIT ||
	      max97236->status_1 & MAX97236_STATUS_1_MCSW_BIT ||
	      max97236->status_2 & MAX97236_STATUS_2_KEY_BIT)) {
		dev_dbg(codec->dev,
			"%s: Nothing to do here, just exit\n", __func__);
		return;
	}

	ret = snd_soc_read(codec, MAX97236_PASSIVE_MBH_KEYSCAN_DATA);

	if (ret & MAX97236_PASSIVE_MBH_PRESS_BIT) {

		adc_data = ret & MAX97236_PASSIVE_MBH_KEYDATA_MASK;

		if (max97236->status_1 & MAX97236_STATUS_1_MCSW_BIT) {
			key = SND_JACK_BTN_0;
		} else if (max97236->status_2 & MAX97236_STATUS_2_KEY_BIT) {

			if (adc_data < MAX97236_KEYDATA_THRESH_0)
				key = SND_JACK_BTN_1;
			else if (adc_data < MAX97236_KEYDATA_THRESH_1)
				key = SND_JACK_BTN_2;
			else if (adc_data < MAX97236_KEYDATA_THRESH_2)
				key = SND_JACK_BTN_3;
			else if (adc_data < MAX97236_KEYDATA_THRESH_3)
				key = SND_JACK_BTN_4;
			else
				key = SND_JACK_BTN_5;

		} else if (max97236->status_1 & MAX97236_STATUS_1_MBH_BIT) {
			dev_dbg(codec->dev, "MAX97236_STATUS_1_MBH_BIT\n");
		} else {
			dev_dbg(codec->dev, "Report key: Unknown");
		}
	}
	max97236_key_report(max97236, key);
}

/* make the interrupt management explicit */
static void max97236_key_interrupt_update(struct max97236_priv *max97236,
		bool do_enable)
{
	struct snd_soc_codec *codec = max97236->codec;
	unsigned char enable = 0x00;
	unsigned char key_debounce = 0x00;
	unsigned char key_delay = 0x00;

	if (do_enable) {
		/* delay (in ms) -1 */
		key_debounce = KEY_DEBOUNCE_MS-1;
		/* delay is in units of 4ms; */
		key_delay = KEY_DELAY_DEBOUNCE_MS/4 - 1;
		enable = 0xff;
	}

	snd_soc_write(codec, MAX97236_KEYSCAN_DEBOUNCE, key_debounce);
	snd_soc_write(codec, MAX97236_KEYSCAN_DELAY, key_delay);

	snd_soc_update_bits(codec, MAX97236_IRQ_MASK_1,
			MAX97236_IRQ_MASK_1_IMCSW_MASK |
			MAX97236_IRQ_MASK_1_IMBH_MASK, enable);
	snd_soc_update_bits(codec, MAX97236_IRQ_MASK_2,
			MAX97236_IRQ_MASK_2_IKEY_MASK, enable);
}

static void max97236_jack_report(struct max97236_priv *max97236, int status)
{
	struct snd_soc_codec *codec = max97236->codec;

	if (max97236->jack_status && status &&
		max97236->jack_status != status) {
		/* consider the following scenario:
		 * 1) Headset is inserted
		 * 2) system goes into suspend
		 * 3) headset is removed
		 * 4) headphone is inserted
		 * 5) system goes out of suspend
		 * In this situation, we might have
		 * new valid device, different from previous,
		 * but the jack removal evet is missing.
		 *
		 * Enforce jack removal in such a case */
		max97236_jack_report(max97236, 0);
	}

	if (max97236->jack_status != status) {
#ifdef CONFIG_AMAZON_METRICS_LOG
		char buf[128];
		if (status == 0)
			snprintf(buf, sizeof(buf),
				"%s:jack:unplugged=1;CT;1:NR", __func__);
		else {
			char *string = NULL;
			switch (status) {
			case SND_JACK_HEADSET:
				string = "HEADSET";
				break;
			case SND_JACK_AVOUT:
				string = "AVOUT";
				break;
			case SND_JACK_HEADPHONE:
				string = "HEADPHONES";
				break;
			case SND_JACK_MICROPHONE:
				string = "MICROPHONE";
				break;
			case SND_JACK_LINEOUT:
				string = "LINEOUT";
				break;
			case SND_JACK_MECHANICAL:
				string = "MECHANICAL";
				break;
			case SND_JACK_VIDEOOUT:
				string = "VIDEOOUT";
				break;
			case SND_JACK_LINEIN:
				string = "LINEIN";
				break;
			case SND_JACK_BTN_0:
				string = "BTN_0";
				break;
			case SND_JACK_BTN_1:
				string = "BTN_1";
				break;
			case SND_JACK_BTN_2:
				string = "BTN_2";
				break;
			case SND_JACK_BTN_3:
				string = "BTN_3";
				break;
			case SND_JACK_BTN_4:
				string = "BTN_4";
				break;
			case SND_JACK_BTN_5:
				string = "BTN_5";
				break;
			default:
				string = "NOTHING";
				break;
			}
			snprintf(buf, sizeof(buf),
				"%s:jack:plugged=1;CT;1,state_%s=1;CT;1:NR",
				__func__, string);
		}
		log_to_metrics(ANDROID_LOG_INFO, "AudioJackEvent", buf);
#endif
		if (max97236->jack_status == SND_JACK_HEADSET && !status)
			max97236_key_report(max97236, 0);

		dev_info(codec->dev, "Report jack event: %d\n", status);

		if (max97236->jack)
			snd_soc_jack_report(max97236->jack, status,
					SND_JACK_HEADSET | SND_JACK_LINEOUT |
					SND_JACK_DATA);
		max97236->jack_status = status;
	}
}

static int max97236_get_jack_val(struct max97236_priv *max97236,
					 int val)
{
	struct snd_soc_codec *codec = max97236->codec;
	int jack_value = 0;

	switch (val) {
	case MAX97236_FORCE_DETECTED_HP_LINE:
	case MAX97236_FORCE_DETECTED_HP:
		jack_value = SND_JACK_HEADPHONE;
		dev_dbg(codec->dev, "Headphone detected\n");
		break;
	case MAX97236_FORCE_DETECTED_HS_OMTP:
		jack_value = SND_JACK_HEADSET;
		dev_dbg(codec->dev, "OMTP Headset detected\n");
		break;
	case MAX97236_FORCE_DETECTED_HS_NON_OMTP:
		jack_value = SND_JACK_HEADSET;
		dev_dbg(codec->dev, "Non OMTP Headset detected\n");
		break;
	case MAX97236_FORCE_DETECTED_DIGITAL:
		jack_value = SND_JACK_DATA;
		dev_dbg(codec->dev, "Digital detected\n");
		break;
	case MAX97236_FORCE_DETECTED_LINE:
		jack_value = SND_JACK_LINEOUT;
		dev_dbg(codec->dev, "Line-out detected\n");
		break;
	default:
		jack_value = 0;
		dev_dbg(codec->dev, "Nothing or Unknown detected\n");
		break;
	}

	return jack_value;
}

static int max97236_get_force_val(struct max97236_priv *max97236,
					unsigned int val)
{
	int force_value;

	switch (val) {
	case MAX97236_FORCE_DETECTED_HP:
	case MAX97236_FORCE_DETECTED_HP_LINE:
	case MAX97236_FORCE_DETECTED_LINE:
		force_value = MAX97236_STATE_LRGG_DC;
		break;
	case MAX97236_FORCE_DETECTED_HS_OMTP:
		force_value = MAX97236_STATE_LRMG;
		break;
	case MAX97236_FORCE_DETECTED_DIGITAL:
	case MAX97236_FORCE_DETECTED_HS_NON_OMTP:
		force_value = MAX97236_STATE_LRGM;
		break;
	default:
		force_value = MAX97236_STATE_FLOAT;
		break;
	}

	return force_value;
}

static inline void max97236_set_force_conf(struct max97236_priv *max97236,
					   int val)
{
	struct snd_soc_codec *codec = max97236->codec;

	snd_soc_write(codec, MAX97236_STATE_FORCING, val);
	snd_soc_update_bits(codec, MAX97236_STATE_FORCING,
			MAX97236_STATE_FORCING_FORCE_MASK, 0x00);
}

static inline void max97236_start_detection(struct max97236_priv *max97236,
						int detection_test)
{
	struct snd_soc_codec *codec = max97236->codec;

	snd_soc_write(codec, MAX97236_CUSTOM_DETECT_ENABLE, 0x80);
	snd_soc_write(codec, MAX97236_CUSTOM_DETECT_ENABLE,
			0x80 | ((detection_test >> 8) & 0x7));
	snd_soc_write(codec, MAX97236_PROGRAM_DETECTION, detection_test);
	snd_soc_write(codec, MAX97236_RUN_CUSTOM_DETECT, 0x80);
}

static inline void max97236_stop_detection(struct max97236_priv *max97236)
{
	struct snd_soc_codec *codec = max97236->codec;

	snd_soc_write(codec, MAX97236_CUSTOM_DETECT_ENABLE, 0x00);
	snd_soc_write(codec, MAX97236_RUN_CUSTOM_DETECT, 0x00);
}

static int max97236_wait_ddone(struct max97236_priv *max97236)
{
	struct snd_soc_codec *codec = max97236->codec;
	int reg;
	int count = MAX97236_DDONE_TIMEOUT;

	do {
		msleep(20);
		reg = snd_soc_read(codec, MAX97236_STATUS_1);

		if (!(reg & MAX97236_STATUS_1_JACKSW_BIT))
			return -ENODEV;

		if (!count)
			return -ETIMEDOUT;

		count--;

	} while (!(reg & MAX97236_STATUS_1_DDONE_BIT));

	return 0;
}

static inline int max97236_read_detected_load(struct snd_soc_codec *codec)
{
	int detected;

	detected = snd_soc_read(codec, MAX97236_DETECTION_RESULTS_1);
	detected |= (snd_soc_read(codec,
			MAX97236_DETECTION_RESULTS_2) << 8);
	detected |= ((snd_soc_read(codec,
			MAX97236_RUN_CUSTOM_DETECT) & 0x1) << 16);

	return detected;
}

/* to improve stability, classify results:
 * sometimes detection oscillates between HP<->LINEOUT, but in fact
 * it's the almost same, so let's treat it as same class */
static int max97236_get_detected_class(struct max97236_priv *max97236,
		int test, int detect_code)
{
	int d_class = CLASS_UNKNOWN;
	if (test == MAX97236_DETECT_DEFAULT) {
		switch (detect_code) {
		case MAX97236_FORCE_DETECTED_HP:
		case MAX97236_FORCE_DETECTED_HP_LINE:
			d_class = CLASS_HP_LINE;
			break;
		case MAX97236_FORCE_DETECTED_DIGITAL:
			d_class = CLASS_DIGITAL;
			break;
		case MAX97236_FORCE_DETECTED_HS_OMTP:
			d_class = CLASS_HEADSET_TYPE_1;
			break;
		case MAX97236_FORCE_DETECTED_HS_NON_OMTP:
			d_class = CLASS_HEADSET_TYPE_2;
			break;
		}
	} else if (test == MAX97236_DETECT_LINEOUT) {
		switch (detect_code) {
		case MAX97236_FORCE_DETECTED_HP_LINE:
			d_class = CLASS_HEADPHONE;
			break;
		case MAX97236_FORCE_DETECTED_LINE:
			d_class = CLASS_LINE_OUT;
			break;
		}
	}
	return d_class;
}

static int max97236_detect_load(struct max97236_priv *max97236,
		int *detected_result , int test, int delay_ms, int stable_max)

{
	struct snd_soc_codec *codec = max97236->codec;
	int retries = MAX97236_DETECT_RETRIES;
	int ret = 0;
	int stable = 0;
	int detected = 0;
	int d_class = CLASS_UNKNOWN;
	int last_d_class = CLASS_INVALID;
	bool done = false;

	/* Turn the state forcing OFF => let detection happen */
	snd_soc_write(codec, MAX97236_STATE_FORCING,
			MAX97236_STATE_FORCING_FORCE_BIT);

	do {
		/* no need to wait if we already running tests for a while */
		if (retries < MAX97236_DETECT_RETRIES/2)
			delay_ms = 0;

		if (delay_ms)
			msleep(delay_ms);

		max97236_start_detection(max97236, test);

		ret = max97236_wait_ddone(max97236);

		if (ret == -ENODEV) {
			dev_dbg(codec->dev, "Jacksw unpluged !\n");
			max97236_stop_detection(max97236);
			d_class = CLASS_NONE;
			detected = -1;
			done = true;
			break;
		}

		if (ret == -ETIMEDOUT) {
			dev_err(codec->dev, "Timeout ddone flag !\n");
			max97236_stop_detection(max97236);
			snd_soc_write(codec, MAX97236_STATE_FORCING,
					MAX97236_STATE_FORCING_FORCE_BIT);
			/* Let's try again */
			continue;
		}

		detected = max97236_read_detected_load(codec);

		max97236_stop_detection(max97236);

		d_class = max97236_get_detected_class(max97236, test, detected);
		if (d_class == CLASS_UNKNOWN) {
			dev_err(codec->dev, "Unknown [test=0x%02X]: 0x%04X\n",
					test, detected);
			snd_soc_write(codec, MAX97236_STATE_FORCING,
					MAX97236_STATE_FORCING_FORCE_BIT);
		}

		if (d_class != last_d_class) {
			last_d_class = d_class;
			stable = 0;
		} else
			stable++;
		if (stable >= stable_max) {
			done = true;
			break;
		}

	} while (--retries);

	if (done && detected_result)
		*detected_result = detected;

	return d_class;
}

static int max97236_jack_detect_report(struct max97236_priv *max97236,
					int *detected_result)
{
	int ret;
	int detected = CLASS_INVALID;

	/* Check for Headphone, Headset and Digital device */
	ret = max97236_detect_load(max97236, &detected,
					MAX97236_DETECT_DEFAULT, 50, 1);

	/* Do not attempt to resolve LineOut vs Headphone. It proved
	 * unreliable and noisy, especially when attached to active load.
	 * Treat them both as headphone.
	 */
#if 0
	/* Let's try it again and check for LINE-OUT device */
	if (ret == CLASS_UNKNOWN ||
		ret == CLASS_HP_LINE)
		ret = max97236_detect_load(max97236, &detected,
					MAX97236_DETECT_LINEOUT, 0, 1);
#endif

	if (detected_result)
		*detected_result = detected;

	return ret;
}

static void max97236_detection_manual_0(struct max97236_priv *max97236)
{
	struct snd_soc_codec *codec = max97236->codec;
	int ret;

	/*
	 * in case of resume, even if on suspend the state
	 * was != 0, we have to run detection.
	 * At the same time we have to preserve the old state
	 * to check if is it the same, and only report it if
	 * it changed.
	 */
	if (max97236->jack_status && !max97236->resuming) {
		if (time_before(jiffies, max97236->plug_time +
			msecs_to_jiffies(KEY_DELAY_DEBOUNCE_MS +
				KEY_WAIT_MIC_CHARGED_MS))) {
			snd_soc_read(codec, MAX97236_STATUS_1);
			snd_soc_read(codec, MAX97236_STATUS_2);
			snd_soc_read(codec, MAX97236_PASSIVE_MBH_KEYSCAN_DATA);
		} else {
			max97236_key_detect_report(max97236);
		}
	} else {
		int jack_state = 0;
		int detected_res;
		int force_val = MAX97236_STATE_FLOAT;

		ret = max97236_jack_detect_report(max97236 , &detected_res);
		force_val = max97236_get_force_val(max97236, detected_res);

		/* Set forced state */
		max97236_set_force_conf(max97236, force_val);

		if (ret == CLASS_INVALID || ret == CLASS_NONE) {
			snd_soc_update_bits(codec, MAX97236_ENABLE_1,
				MAX97236_ENABLE_1_SHDN_MASK, 0x00);
			max97236_set_force_conf(max97236, MAX97236_STATE_FLOAT);
		}
		else
			/* Enable device */
			snd_soc_update_bits(codec, MAX97236_ENABLE_1,
				MAX97236_ENABLE_1_SHDN_MASK,
				MAX97236_ENABLE_1_SHDN_BIT);

		max97236->resuming = false;
		jack_state = max97236_get_jack_val(max97236, detected_res);

		/* Notify the system what we got */
		max97236_jack_report(max97236, jack_state);

		/* Enable key detection interrupts */
		if (max97236->jack_status == SND_JACK_HEADSET) {
			/* configure MIC BIAS resistor: always 2.2k */
			snd_soc_update_bits(codec, MAX97236_MICROPHONE,
				MAX97236_MICROPHONE_MICR_MASK, 0);
			max97236_key_interrupt_update(max97236, true);
			max97236->plug_time = jiffies;
		} else {
			/* if there is no MIC, disconnect BIAS from MIC IN */
			snd_soc_update_bits(codec, MAX97236_MICROPHONE,
				MAX97236_MICROPHONE_MICR_MASK,
				4 << MAX97236_MICROPHONE_MICR_SHIFT);
			max97236_key_interrupt_update(max97236, false);
		}
	}
}

static void max97236_jack_work(struct work_struct *work)
{
	struct max97236_priv *max97236 = container_of(work,
			struct max97236_priv, jack_work.work);
	struct snd_soc_codec *codec = max97236->codec;

	/* Disable interrupt to avoid any glitches while first jack detection */
	disable_irq(max97236->irq);

	max97236->status_1 = snd_soc_read(codec, MAX97236_STATUS_1);
	max97236->status_2 = snd_soc_read(codec, MAX97236_STATUS_2);

	if (max97236->status_1 & MAX97236_STATUS_1_JACKSW_BIT) {
		wake_lock(&max97236->wakelock);
		max97236_detection_manual_0(max97236);
		wake_unlock(&max97236->wakelock);
		/* get most up-to-date status.
		 * this will also clear any pending irq from max97236 */
		max97236->status_1 = snd_soc_read(codec, MAX97236_STATUS_1);
		max97236->status_2 = snd_soc_read(codec, MAX97236_STATUS_2);
	}

	/* Enable jack switch interrupt */
	snd_soc_update_bits(codec, MAX97236_IRQ_MASK_1,
			MAX97236_IRQ_MASK_1_JACKSW_BIT,
			MAX97236_IRQ_MASK_1_JACKSW_BIT);

	/* Enable interrupt */
	enable_irq(max97236->irq);

	/* we're acting on most up-to-date status. any changes to it
	 * will trigger interrupt, and reschedule this work */
	if (!(max97236->status_1 & MAX97236_STATUS_1_JACKSW_BIT)) {
		max97236_jack_report(max97236, 0);
		max97236_set_force_conf(max97236, MAX97236_STATE_FLOAT);
		max97236_key_interrupt_update(max97236, false);
		snd_soc_update_bits(codec, MAX97236_ENABLE_1,
				MAX97236_ENABLE_1_SHDN_MASK, 0x00);
	}
}

void max97236_detect_jack(struct snd_soc_codec *codec)
{

	struct max97236_priv *max97236 = snd_soc_codec_get_drvdata(codec);

	/* Give some time to system , so "h2w" uevent can rise */
	queue_delayed_work(max97236->jack_workq, &max97236->jack_work,
			msecs_to_jiffies(1000));

}
EXPORT_SYMBOL_GPL(max97236_detect_jack);

/* Interrupt routine for manual mode */
static irqreturn_t max97236_interrupt(int irq, void *data)
{
	struct max97236_priv *max97236 = data;

	/* this is better stay in the interrupt thread, rather than
	 * be converted to hard IRQ routine, because it saves us a lot of
	 * efforts in interrupt management and synchronization.
	 *
	 * Dealing with hard IRQ requires to disable irq here, and reenable
	 * it when we're done, inside the worker thread.
	 * At the same time, adding the ability to cancel
	 * the work on suspend will bring unnecessary complications to such
	 * solution.
	 *
	 * If further speed improvements are necessary, this
	 * approach may still be taken, because I found it lets us save
	 * up to 50 ms per detection cycle. (up to 200 ms worst case total)
	 */
	queue_work(max97236->jack_workq, &max97236->jack_work.work);

	return IRQ_HANDLED;
}

static struct snd_soc_dai_driver max97236_dai[] = {
	{
		.name = "max97236-hifi",
		.playback = {
			.stream_name = "Playback",
			.channels_min = 1,
			.channels_max = 2,
		},
		.capture = {
			.stream_name = "Capture",
			.channels_min = 1,
			.channels_max = 2,
		},
	}
};

#ifdef CONFIG_PM
static int max97236_suspend(struct snd_soc_codec *codec)
{
	struct max97236_priv *max97236 = snd_soc_codec_get_drvdata(codec);

	cancel_delayed_work_sync(&max97236->jack_work);

	/* Mute Left and Right channels */
	snd_soc_update_bits(codec, MAX97236_LEFT_VOLUME,
			MAX97236_LEFT_VOLUME_MUTEL_SHIFT,
			MAX97236_LEFT_VOLUME_MUTEL_BIT);
	snd_soc_update_bits(codec, MAX97236_RIGHT_VOLUME,
			MAX97236_RIGHT_VOLUME_MUTER_SHIFT,
			MAX97236_RIGHT_VOLUME_MUTER_BIT);

	max97236_set_force_conf(max97236, MAX97236_STATE_LRGG_DC);

	/* Set device in standby mode */
	snd_soc_update_bits(codec, MAX97236_ENABLE_1,
			MAX97236_ENABLE_1_SHDN_MASK, 0x00);

	if (!max97236->jack_status) {
		/* power cycle codec to apply forced config */
		snd_soc_update_bits(codec, MAX97236_ENABLE_1,
				MAX97236_ENABLE_1_SHDN_MASK,
				MAX97236_ENABLE_1_SHDN_BIT);
		snd_soc_update_bits(codec, MAX97236_ENABLE_1,
				MAX97236_ENABLE_1_SHDN_MASK, 0x00);
	}

	return 0;
}

static int max97236_resume(struct snd_soc_codec *codec)
{
	struct max97236_priv *max97236 = snd_soc_codec_get_drvdata(codec);

	/* Set detection mode / chip controls fuction blocks */
	snd_soc_update_bits(codec, MAX97236_ENABLE_2,
			MAX97236_ENABLE_2_FAST_MASK |
			MAX97236_ENABLE_2_AUTO_MASK,
			MAX97236_ENABLE_2_FAST_BIT |
			detection_mode);

	if (max97236->jack_status)
		snd_soc_update_bits(codec, MAX97236_ENABLE_1,
				MAX97236_ENABLE_1_SHDN_MASK,
				MAX97236_ENABLE_1_SHDN_BIT);

	/* re-trigger detection on resume */
	max97236->resuming = true;
	queue_work(max97236->jack_workq, &max97236->jack_work.work);

	return 0;
}
#else
#define max97236_suspend NULL
#define max97236_resume NULL
#endif

int max97236_set_jack(struct snd_soc_codec *codec, struct snd_soc_jack *jack)
{
	struct max97236_priv *max97236 = snd_soc_codec_get_drvdata(codec);

	if (jack)
		max97236->jack = jack;

	return (jack != NULL);
}
EXPORT_SYMBOL_GPL(max97236_set_jack);

int max97236_set_key_div(struct snd_soc_codec *codec, unsigned long clk_freq)
{

	dev_dbg(codec->dev, "External clk rate: %lu\n", clk_freq);

	snd_soc_write(codec, MAX97236_KEYSCAN_CLK_DIV_1,
			(clk_freq / 2000) >> 8);
	snd_soc_write(codec, MAX97236_KEYSCAN_CLK_DIV_2,
			(clk_freq / 2000) & 0xFF);
	snd_soc_write(codec, MAX97236_KEYSCAN_CLK_DIV_ADC,
			(clk_freq / 200000));

	return 0;
}
EXPORT_SYMBOL_GPL(max97236_set_key_div);

static int max97236_probe(struct snd_soc_codec *codec)
{
	struct max97236_priv *max97236 = snd_soc_codec_get_drvdata(codec);
	int ret;

	ret = snd_soc_codec_set_cache_io(codec, 8, 8, SND_SOC_I2C);
	codec->control_data = max97236->regmap;
	max97236->codec = codec;

	ret = snd_soc_read(codec, MAX97236_VENDOR_ID);
	if (ret < 0) {
		dev_err(codec->dev, "Failed to read device rev: %d\n", ret);
		return -ENODEV;
	}

	dev_info(codec->dev,
		"Found MAX97236 HP Amp device revision: 0x%x\n", ret);

	/* Disable all interrupts */
	snd_soc_write(codec, MAX97236_IRQ_MASK_1, 0x00);
	snd_soc_write(codec, MAX97236_IRQ_MASK_2, 0x00);

	/* Clear any interrupts */
	snd_soc_read(codec, MAX97236_STATUS_1);
	snd_soc_read(codec, MAX97236_STATUS_2);
	snd_soc_read(codec, MAX97236_PASSIVE_MBH_KEYSCAN_DATA);

	/* Set key debouce and delay time */
	snd_soc_write(codec, MAX97236_KEYSCAN_DEBOUNCE, 0x00);
	snd_soc_write(codec, MAX97236_KEYSCAN_DELAY, 0x00);

	snd_soc_write(codec, MAX97236_DC_TEST_SLEW_CONTROL, 0x06);

	/* Pulse Width = 100uS ; Pulse Amplitude = 50 mV */
	snd_soc_write(codec, MAX97236_AC_TEST_CONTROL, 0x05);

	/* Set mic bias voltage to 2.6V */
	snd_soc_update_bits(codec, MAX97236_MICROPHONE,
				MAX97236_MICROPHONE_BIAS_MASK,
				MAX97236_MICROPHONE_BIAS_BIT);

	/* Set detection mode / chip controls fuction blocks */
	snd_soc_update_bits(codec, MAX97236_ENABLE_2,
				MAX97236_ENABLE_2_SVEN_MASK |
				MAX97236_ENABLE_2_FAST_MASK |
				MAX97236_ENABLE_2_AUTO_MASK,
				MAX97236_ENABLE_2_SVEN_BIT |
				MAX97236_ENABLE_2_FAST_BIT |
				detection_mode);

	max97236->jack_workq = create_singlethread_workqueue("max97236_wq");
	if (!max97236->jack_workq) {
		dev_err(codec->dev, "Failed to create a workqueue\n");
		return -ENOMEM;
	}

	INIT_DELAYED_WORK(&max97236->jack_work, max97236_jack_work);

	wake_lock_init(&max97236->wakelock, WAKE_LOCK_SUSPEND,
				"max97236-wakelock");

	ret = request_threaded_irq(max97236->irq, NULL, max97236_interrupt,
				   IRQF_TRIGGER_LOW | IRQF_ONESHOT,
				   "max97236_irq", max97236);
	if (ret < 0) {
		dev_err(codec->dev, "Failed to register irq\n");
		wake_lock_destroy(&max97236->wakelock);
		destroy_workqueue(max97236->jack_workq);
		return ret;
	}

	return 0;
}

static int max97236_remove(struct snd_soc_codec *codec)
{
	struct max97236_priv *max97236 = snd_soc_codec_get_drvdata(codec);

	if (!max97236)
		return -ENODEV;

	cancel_delayed_work_sync(&max97236->jack_work);

	destroy_workqueue(max97236->jack_workq);

	/* Disable all interrupts */
	snd_soc_write(codec, MAX97236_IRQ_MASK_1, 0x00);
	snd_soc_write(codec, MAX97236_IRQ_MASK_2, 0x00);

	max97236_set_force_conf(max97236, MAX97236_STATE_LRGG_DC);

	/* Set device in standby */
	snd_soc_update_bits(codec, MAX97236_ENABLE_1,
			MAX97236_ENABLE_1_SHDN_MASK, 0x00);

	/* apply new configuration. */
	if (!max97236->jack_status) {
		snd_soc_update_bits(codec, MAX97236_ENABLE_1,
			MAX97236_ENABLE_1_SHDN_MASK,
			MAX97236_ENABLE_1_SHDN_BIT);
		snd_soc_update_bits(codec, MAX97236_ENABLE_1,
			MAX97236_ENABLE_1_SHDN_MASK, 0);
	}

	if (max97236->irq) {
		/* Disable OMAP interrupt */
		disable_irq(max97236->irq);

		free_irq(max97236->irq, codec);
	}

	wake_lock_destroy(&max97236->wakelock);

	return 0;
}


static struct snd_soc_codec_driver soc_codec_dev_max97236 = {
	.probe =	max97236_probe,
	.remove =	max97236_remove,
	.suspend =	max97236_suspend,
	.resume =	max97236_resume,
	.set_bias_level = max97236_set_bias_level,

	.controls = max97236_controls,
	.num_controls = ARRAY_SIZE(max97236_controls),
	.dapm_widgets = max97236_dapm_widgets,
	.num_dapm_widgets = ARRAY_SIZE(max97236_dapm_widgets),
	.dapm_routes = max97236_dapm_routes,
	.num_dapm_routes = ARRAY_SIZE(max97236_dapm_routes),
};

static const struct regmap_config max97236_regmap = {
	.reg_bits = 8,
	.val_bits = 8,
	.max_register = MAX97236_MAX_REG,
	.reg_defaults = max97236_reg,
	.num_reg_defaults = ARRAY_SIZE(max97236_reg),
	.volatile_reg = max97236_volatile_register,
	.readable_reg = max97236_readable_register,
	.cache_type = REGCACHE_RBTREE,
};

static int __devinit max97236_i2c_probe(struct i2c_client *i2c,
		const struct i2c_device_id *id)
{
	struct max97236_priv *max97236;
	int ret;

	if (!i2c->dev.platform_data) {
		dev_err(&i2c->dev, "Missing platform data\n");
		return -ENODEV;
	}

	max97236 = devm_kzalloc(&i2c->dev, sizeof(struct max97236_priv),
				GFP_KERNEL);
	if (max97236 == NULL) {
		dev_err(&i2c->dev, "Failed to allocate private data for device\n");
		return -ENOMEM;
	}

	i2c_set_clientdata(i2c, max97236);

	max97236->control_data = i2c;
	max97236->pdata = i2c->dev.platform_data;

	ret = gpio_request_one(max97236->pdata->irq_gpio,
				GPIOF_IN, "max97236_irq");
	if (ret < 0) {
		dev_err(&i2c->dev, "Failed to request interrupt gpio\n");
		goto err_gpio;
	}
	max97236->irq = gpio_to_irq(max97236->pdata->irq_gpio);

	max97236->regmap = regmap_init_i2c(i2c, &max97236_regmap);
	if (IS_ERR(max97236->regmap)) {
		ret = PTR_ERR(max97236->regmap);
		dev_err(&i2c->dev, "Failed to allocate regmap\n");
		goto err_regmap;
	}

	ret = snd_soc_register_codec(&i2c->dev, &soc_codec_dev_max97236,
				max97236_dai, ARRAY_SIZE(max97236_dai));
	if (ret < 0) {
		dev_err(&i2c->dev, "Failed to register codec\n");
		goto err_codec;
	}

	return 0;

err_codec:
	regmap_exit(max97236->regmap);
err_regmap:
	gpio_free(max97236->pdata->irq_gpio);
err_gpio:

	return ret;
}

static __devexit int max97236_i2c_remove(struct i2c_client *client)
{
	struct max97236_priv *max97236 = dev_get_drvdata(&client->dev);

	snd_soc_unregister_codec(&client->dev);
	regmap_exit(max97236->regmap);
	gpio_free(max97236->pdata->irq_gpio);

	return 0;
}

static const struct i2c_device_id max97236_i2c_id[] = {
	{ "max97236", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, max97236_i2c_id);

static struct i2c_driver max97236_i2c_driver = {
	.driver = {
		.name = "max97236",
		.owner = THIS_MODULE,
	},
	.probe = max97236_i2c_probe,
	.remove = __devexit_p(max97236_i2c_remove),
	.id_table = max97236_i2c_id,
};

static int __init max97236_init(void)
{
	return i2c_add_driver(&max97236_i2c_driver);
}
module_init(max97236_init);

static void __exit max97236_exit(void)
{
	i2c_del_driver(&max97236_i2c_driver);
}
module_exit(max97236_exit);

MODULE_AUTHOR("Plamen Valev <pvalev@mm-sol.com>");
MODULE_DESCRIPTION("ASoC MAX97236 Amp driver");
MODULE_LICENSE("GPL");
