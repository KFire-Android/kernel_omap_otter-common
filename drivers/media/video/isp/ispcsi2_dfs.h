/*
 * ispcsi2_dfs.c
 *
 * Copyright (C) 2009 Texas Instruments.
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

#ifndef OMAP_ISP_CSI2_DFS_API_H
#define OMAP_ISP_CSI2_DFS_API_H

struct isp_device;

void ispcsi2_dfs_dump(struct isp_device *isp);

void ispcsi2_dfs_setup(struct isp_device *isp);

void ispcsi2_dfs_shutdown(struct isp_device *isp);

#endif
