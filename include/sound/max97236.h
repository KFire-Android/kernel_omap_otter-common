/*
 * Platform data for MAX97236
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

#ifndef __SOUND_MAX97236_PDATA_H__
#define __SOUND_MAX97236_PDATA_H__

#include <sound/soc.h>

/* audio amp platform data */
struct max97236_pdata {
	int irq_gpio;
};

int max97236_set_jack(struct snd_soc_codec *codec, struct snd_soc_jack *jack);
int max97236_set_key_div(struct snd_soc_codec *codec, unsigned long clk_freq);
void max97236_detect_jack(struct snd_soc_codec *codec);

#endif
