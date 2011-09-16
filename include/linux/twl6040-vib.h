/* drivers/misc/twl6040-vib.h
 *
 * Copyright (C) 2010 Texas Instruments, Inc.
 * Copyright (C) 2008 Google, Inc.
 * Author: Dan Murphy <dmurphy@ti.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * Derived from: vib-gpio.h
 */

#ifndef _LINUX_TWL6040_VIB_H
#define _LINUX_TWL6040_VIB_H

#ifdef __KERNEL__

#define VIB_NAME "twl6040-vibra"

#endif /* __KERNEL__ */

void vibrator_haptic_fire(int value);

#endif /* _LINUX_TWL6040_VIB_H */
