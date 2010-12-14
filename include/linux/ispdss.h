/*
 * drivers/media/video/isp/ispdss.h
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

#ifndef OMAP_ISPDSS_H
#define OMAP_ISPDSS_H

#include <linux/types.h>

typedef void (*ispdss_callback) (void *arg1);

struct isp_node;

/* Contains the status of hardware and channel */
struct ispdss_status {
	__s32 chan_busy;
	__s32 hw_busy;
	__s32 src;
};

int ispdss_begin(struct isp_node *pipe, u32 input_buffer_index,
		 int output_buffer_index, u32 out_off, u32 out_phy_add,
		 u32 in_phy_add, u32 in_off);

int ispdss_configure(struct isp_node *pipe, ispdss_callback callback,
		  u32 num_video_buffers, void *arg1);

void ispdss_put_resource(void);

int ispdss_get_resource(void);

#endif
