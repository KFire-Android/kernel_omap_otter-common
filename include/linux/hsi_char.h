/*
 * hsi_char.h
 *
 * HSI character driver public declaration header file.
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

#ifndef HSI_CHAR_H
#define HSI_CHAR_H

#define HSI_CHAR_BASE		'S'
#define CS_IOW(num, dtype)	_IOW(HSI_CHAR_BASE, num, dtype)
#define CS_IOR(num, dtype)	_IOR(HSI_CHAR_BASE, num, dtype)
#define CS_IOWR(num, dtype)	_IOWR(HSI_CHAR_BASE, num, dtype)
#define CS_IO(num)		_IO(HSI_CHAR_BASE, num)

#define CS_SEND_BREAK		CS_IO(1)
#define CS_FLUSH_RX		CS_IO(2)
#define CS_FLUSH_TX		CS_IO(3)
#define CS_BOOTSTRAP		CS_IO(4)
#define CS_SET_WAKELINE		CS_IOW(5, unsigned int)
#define CS_GET_WAKELINE		CS_IOR(6, unsigned int)
#define CS_SET_RX		CS_IOW(7, struct hsi_rx_config)
#define CS_GET_RX		CS_IOW(8, struct hsi_rx_config)
#define CS_SET_TX		CS_IOW(9, struct hsi_tx_config)
#define CS_GET_TX		CS_IOW(10, struct hsi_tx_config)

#define HSI_MODE_SLEEP		0
#define HSI_MODE_STREAM		1
#define HSI_MODE_FRAME		2

#define HSI_ARBMODE_RR		0
#define HSI_ARBMODE_PRIO	1

#define WAKE_UP			0
#define WAKE_DOWN		1

struct hsi_tx_config {
	u32 mode;
	u32 flow;
	u32 frame_size;
	u32 channels;
	u32 divisor;
	u32 arb_mode;
};

struct hsi_rx_config {
	u32 mode;
	u32 flow;
	u32 frame_size;
	u32 channels;
	u32 divisor;	/* not used for SSI */
};

#endif /* HSI_CHAR_H */
