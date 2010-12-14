/*
 * drivers/media/video/isp/omap_previewer.h
 *
 * Header file for Preview module wrapper in TI's OMAP3430 ISP
 *
 * Copyright (C) 2008 Texas Instruments, Inc.
 *
 * Contributors:
 * 	Leonides Martinez <leonides.martinez@ti.com>
 * 	Sergio Aguirre <saaguirre@ti.com>
 *
 * This package is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * THIS PACKAGE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#include <linux/videodev2.h>
#include <mach/isp_user.h>

#ifndef OMAP_ISP_PREVIEW_WRAP_H
#define OMAP_ISP_PREVIEW_WRAP_H

#define PREV_IOC_BASE			'P'
#define PREV_REQBUF			_IOWR(PREV_IOC_BASE, 1,\
						struct v4l2_requestbuffers)
#define PREV_QUERYBUF			_IOWR(PREV_IOC_BASE, 2,\
							struct v4l2_buffer)
#define PREV_SET_PARAM			_IOW(PREV_IOC_BASE, 3,\
							struct prev_params)
#define PREV_GET_PARAM			_IOWR(PREV_IOC_BASE, 4,\
							struct prev_params)
#define PREV_PREVIEW			_IOR(PREV_IOC_BASE, 5, int)
#define PREV_GET_STATUS			_IOR(PREV_IOC_BASE, 6, char)
#define PREV_GET_CROPSIZE		_IOR(PREV_IOC_BASE, 7,\
							struct v4l2_rect)
#define PREV_QUEUEBUF			_IOWR(PREV_IOC_BASE, 8,\
							struct v4l2_buffer)
#define PREV_IOC_MAXNR			8

#endif
