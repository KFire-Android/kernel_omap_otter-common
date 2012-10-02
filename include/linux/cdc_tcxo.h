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

#define CDC_TCXO_REQ1POL	BIT(0)
#define CDC_TCXO_REQ2POL	BIT(1)
#define CDC_TCXO_REQ3POL	BIT(2)
#define CDC_TCXO_REQ4POL	BIT(3)
#define CDC_TCXO_REQ1INT	BIT(4)
#define CDC_TCXO_REQ2INT	BIT(5)
#define CDC_TCXO_REQ3INT	BIT(6)
#define CDC_TCXO_REQ4INT	BIT(7)

/* REGISTER2 bit fields */

#define CDC_TCXO_MREQ1		BIT(4)
#define CDC_TCXO_MREQ2		BIT(5)
#define CDC_TCXO_MREQ3		BIT(6)
#define CDC_TCXO_MREQ4		BIT(7)

/* REGISTER3 bit fields */

#define CDC_TCXO_REQ1PRIO	BIT(0)
#define CDC_TCXO_REQ2PRIO	BIT(1)
#define CDC_TCXO_REQ3PRIO	BIT(2)
#define CDC_TCXO_REQ4PRIO	BIT(3)
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

#ifdef CONFIG_CDC_TCXO
int cdc_tcxo_set_req_int(int clk_id, int enable);
int cdc_tcxo_set_req_prio(int clk_id, int req_prio);
#else
static inline int cdc_tcxo_set_req_int(int clk_id, int enable) { return 0; }
static inline int cdc_tcxo_set_req_prio(int clk_id, int req_prio) { return 0; }
#endif

#endif /* _LINUX_CDC_TCXO_H_ */
