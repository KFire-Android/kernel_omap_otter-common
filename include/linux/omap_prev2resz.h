/*
 * drivers/media/video/isp/omap_prev2resz.h
 *
 * Header file for Preview, Reszier pipe module in TI's OMAP3430 ISP
 *
 * Copyright (C) 2008 Texas Instruments, Inc.
 *
 * Contributors:
 * 	Atanas Filipov <afilipov@mm-sol.com>
 *
 * This package is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * THIS PACKAGE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#include <mach/isp_user.h>
#include <linux/videodev2.h>

#ifndef OMAP_ISP_PREV2RESZ_H
#define OMAP_ISP_PREV2RESZ_H

enum prev2resz_state {
	PREV2RESZ_FREE,
	PREV2RESZ_BUSY
};

/**
 * struct prev2resz_status - Structure to know status of the hardware
 * @hw_busy: Flag to indicate if hardware is running.
 */
struct prev2resz_status {
	enum prev2resz_state prv_busy;
	enum prev2resz_state rsz_busy;
};

#define PREV2RESZ_IOC_BASE	'M'

#define PREV2RESZ_REQBUF	_IOWR(PREV2RESZ_IOC_BASE, 1,\
						struct v4l2_requestbuffers)
#define PREV2RESZ_QUERYBUF	_IOWR(PREV2RESZ_IOC_BASE, 2, struct v4l2_buffer)

#define PREV2RESZ_QUEUEBUF	_IOWR(PREV2RESZ_IOC_BASE, 3, struct v4l2_buffer)

#define PREV2RESZ_SET_CONFIG	_IOWR(PREV2RESZ_IOC_BASE, 4,\
						struct isp_node)
#define PREV2RESZ_GET_CONFIG	_IOWR(PREV2RESZ_IOC_BASE, 5,\
						struct isp_node)
#define PREV2RESZ_RUN_ENGINE	_IOWR(PREV2RESZ_IOC_BASE, 6, int)

#define PREV2RESZ_GET_STATUS	_IOWR(PREV2RESZ_IOC_BASE, 7,\
						struct prev2resz_status)
#define PREV2RESZ_IOC_MAXNUM	7

#endif
