/*
 * Clock Divider Chip (TCXO) support
 *
 * Copyright (C) 2011 Texas Instruments, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _LINUX_CDC_TCXO_H_
#define _LINUX_CDC_TCXO_H_

#define CDC_TCXO_REG1		0
#define CDC_TCXO_REG2		1
#define CDC_TCXO_REG3		2
#define CDC_TCXO_REG4		3
#define CDC_TCXO_REGNUM		4

/* REGISTER1 bit fields */

#define CDC_TCXO_REQ1POL	0x01
#define CDC_TCXO_REQ2POL	0x02
#define CDC_TCXO_REQ3POL	0x04
#define CDC_TCXO_REQ4POL	0x08
#define CDC_TCXO_REQ1INT	0x10
#define CDC_TCXO_REQ2INT	0x20
#define CDC_TCXO_REQ3INT	0x40
#define CDC_TCXO_REQ4INT	0x80

/* REGISTER2 bit fields */

#define CDC_TCXO_MREQ1		0x10
#define CDC_TCXO_MREQ2		0x20
#define CDC_TCXO_MREQ3		0x40
#define CDC_TCXO_MREQ4		0x80

/* REGISTER3 bit fields */

#define CDC_TCXO_REQ1PRIO	0x01
#define CDC_TCXO_REQ2PRIO	0x02
#define CDC_TCXO_REQ3PRIO	0x04
#define CDC_TCXO_REQ4PRIO	0x08
#define CDC_TCXO_LDOEN0		BIT(4)
#define CDC_TCXO_LDOEN1		BIT(5)
#define CDC_TCXO_MREQCTRL0	BIT(6)
#define CDC_TCXO_MREQCTRL1	BIT(7)

#define CDC_TCXO_CLK1		1
#define CDC_TCXO_CLK2		2
#define CDC_TCXO_CLK3		3
#define CDC_TCXO_CLK4		4

#define CDC_TCXO_PRIO_REQ	0
#define CDC_TCXO_PRIO_REQINT	1

struct cdc_tcxo_platform_data {
	char buf[4];
};

int cdc_tcxo_set_req_int(int clk_id, int enable);
int cdc_tcxo_set_req_prio(int clk_id, int req_prio);

#endif /* _LINUX_CDC_TCXO_H_ */
