/*
 * hsi-char.h
 *
 * HSI character driver private declaration header file.
 *
 * Copyright (C) 2009 Nokia Corporation. All rights reserved.
 * Copyright (C) 2009 Texas Instruments, Inc.
 *
 * Author: Andras Domokos <andras.domokos@nokia.com>
 * Author: Sebastien JAN <s-jan@ti.com>
 *
 * This package is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * THIS PACKAGE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#ifndef _HSI_CHAR_H
#define _HSI_CHAR_H

#include <linux/hsi_char.h>
#include "hsi-if.h"

/* Max HSI channel id allowed to be handled as char device. */
/* Current range [1, 16] */
#define HSI_MAX_CHAR_DEV_ID	HSI_MAX_CHAR_DEVS

void if_hsi_notify(int ch, struct hsi_event *ev);

#endif /* _HSI_CHAR_H */
