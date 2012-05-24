/*
 * soc-private.h  --  ALSA SoC internal declarations
 *
 * Copyright 2005 Wolfson Microelectronics PLC.
 * Copyright 2005 Openedhand Ltd.
 * Copyright (C) 2010 Slimlogic Ltd.
 * Copyright (C) 2012 Texas Instruments Inc.
 *
 * Authors: Liam Girdwood <lrg@ti.com>
 *          Mark Brown <broonie@opensource.wolfsonmicro.com>
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 */
#ifndef __SND_SOC_SOC_PRIVATE_H__
#define __SND_SOC_SOC_PRIVATE_H__

struct snd_soc_pcm_runtime;
struct snd_soc_dapm_widget;

/* soc-pcm.c */
int soc_dpcm_debugfs_add(struct snd_soc_pcm_runtime *rtd);
int soc_dpcm_be_digital_mute(struct snd_soc_pcm_runtime *fe, int mute);
int soc_dpcm_fe_suspend(struct snd_soc_pcm_runtime *fe);
int soc_dpcm_fe_resume(struct snd_soc_pcm_runtime *fe);
int soc_dpcm_be_cpu_dai_suspend(struct snd_soc_pcm_runtime *fe);
int soc_dpcm_be_cpu_dai_resume(struct snd_soc_pcm_runtime *fe);
int soc_dpcm_be_platform_suspend(struct snd_soc_pcm_runtime *fe);
int soc_dpcm_be_platform_resume(struct snd_soc_pcm_runtime *fe);
int soc_dpcm_be_ac97_cpu_dai_suspend(struct snd_soc_pcm_runtime *fe);
int soc_dpcm_be_ac97_cpu_dai_resume(struct snd_soc_pcm_runtime *fe);
int soc_dpcm_runtime_update(struct snd_soc_dapm_widget *widget);

#endif /* __SND_SOC_SOC_PRIVATE_H__ */
