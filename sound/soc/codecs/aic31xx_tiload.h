
/*
 * linux/sound/soc/codecs/aic31xx_tiload.h
 *
 *
 * Copyright (C) 2011 Texas Instruments Inc.
 *
 *
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
 *
 *
 *
 */

#ifndef _AIC31XX_TILOAD_H
#define _AIC31XX_TILOAD_H

/* typedefs required for the included header files */
typedef char *string;

/* defines */
#define DEVICE_NAME		"tiload_node"
#define aic31xx_IOC_MAGIC	0xE0
#define aic31xx_IOMAGICNUM_GET	_IOR(aic31xx_IOC_MAGIC, 1, int)
#define aic31xx_IOMAGICNUM_SET	_IOW(aic31xx_IOC_MAGIC, 2, int)

extern int aic3xxx_write(struct snd_soc_codec *codec, unsigned int reg,
				unsigned int value);
int aic31xx_driver_init(struct snd_soc_codec *codec);
#endif
