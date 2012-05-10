/*
* linux/drivers/video/omap2/dss/cec_util.h
*
* CEC local utility header file
*
* Copyright (C) 2012 Texas Instruments, Inc
* Author: Muralidhar Dixit <murali.dixit@ti.com>
*
* This program is free software; you can redistribute it and/or modify it
* under the terms of the GNU General Public License version 2 as published by
* the Free Software Foundation.
*
* This program is distributed in the hope that it will be useful, but WITHOUT
* ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
* FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
* more details.
*
* You should have received a copy of the GNU General Public License along with
* this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#ifndef _CEC_UTIL_H_
#define _CEC_UTIL_H_

static inline int cec_get_keyboard_handle(struct cec_keyboard_device *dev)
{
	int r = -EINVAL;
#ifdef CONFIG_KEYBOARD_CEC
	/*Get the CEC keyboard driver handle*/
	r = cec_keyboard_get_device(dev);
#endif
	return r;
}
#endif
