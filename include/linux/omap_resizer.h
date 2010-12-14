/*
 * drivers/media/video/isp/omap_resizer.h
 *
 * Include file for Resizer module in TI's OMAP3430 ISP
 *
 * Copyright (C) 2008 Texas Instruments, Inc.
 *
 * This package is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * THIS PACKAGE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#ifndef OMAP_RESIZER_H
#define OMAP_RESIZER_H

#include <linux/types.h>
#include <linux/videodev2.h>
#include <mach/isp_user.h>

/*
 * Contains the status of hardware and channel
 */
struct rsz_stat {
	__u32 ch_busy;		/* 1: channel is busy, 0: channel is ready */
	__u32 hw_busy;		/* 1: hardware is busy, 0: hardware is ready */
};

/*
 * IOCTLS definition
 */
#define RSZ_IOC_BASE	'R'

/*
 * Ioctl options which are to be passed while calling the ioctl
 */
#define RSZ_S_PARAM  _IOWR(RSZ_IOC_BASE, 1, struct isp_node)
#define RSZ_G_PARAM  _IOWR(RSZ_IOC_BASE, 2, struct isp_node)
#define RSZ_G_STATUS _IOWR(RSZ_IOC_BASE, 3, struct rsz_stat)
#define RSZ_REQBUF   _IOWR(RSZ_IOC_BASE, 4, struct v4l2_requestbuffers)
#define RSZ_QUERYBUF _IOWR(RSZ_IOC_BASE, 5, struct v4l2_buffer)
#define RSZ_QUEUEBUF _IOWR(RSZ_IOC_BASE, 6, struct v4l2_buffer)
#define RSZ_RESIZE   _IOWR(RSZ_IOC_BASE, 7, int)

#define RSZ_IOC_MAXNR	7

#endif

