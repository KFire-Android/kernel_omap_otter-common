/*
 * tmp006.h - header for tmp006 platform data
 *
 * Copyright (C) 2011-2012 Texas Instruments, Inc.
 *
 * Author: Radhesh, Fadnis" <radhesh.fadnis@ti.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 */

#ifndef __TMP006_H
#define __TMP006_H

#include <linux/types.h>

struct tmp006_platform_data {
	int update_period;
};

#endif /* End of __TMP006_H */
