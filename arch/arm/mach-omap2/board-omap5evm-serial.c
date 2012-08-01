/*
 * arch/arm/mach-omap2/board-omap5evm-serial.c
 *
 * Copyright (C) 2012 Texas Instruments
 *
 * Author: Huzefa Kankroliwala <huzefank@ti.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <plat/omap-serial.h>
#include "board-omap5evm.h"
#include "mux.h"

static struct omap_device_pad uart1_pads[] __initdata = {
	{
		.name   = "uart1_cts.uart1_cts",
		.enable = OMAP_PIN_INPUT_PULLUP | OMAP_MUX_MODE0,
	},
	{
		.name   = "uart1_rts.uart1_rts",
		.enable = OMAP_PIN_OUTPUT | OMAP_MUX_MODE0,
	},
	{
		.name   = "uart1_tx.uart1_tx",
		.enable = OMAP_PIN_OUTPUT | OMAP_MUX_MODE0,
	},
	{
		.name   = "uart1_rx.uart1_rx",
		.flags  = OMAP_DEVICE_PAD_REMUX | OMAP_DEVICE_PAD_WAKEUP,
		.enable = OMAP_PIN_INPUT_PULLUP | OMAP_MUX_MODE0,
		.idle   = OMAP_PIN_INPUT_PULLUP | OMAP_MUX_MODE0,
	},
};

static struct omap_device_pad uart3_pads[] __initdata = {
	{
		.name   = "uart3_cts_rctx.uart3_cts_rctx",
		.enable = OMAP_PIN_INPUT_PULLUP | OMAP_MUX_MODE0,
	},
	{
		.name   = "uart3_rts_irsd.uart3_rts_irsd",
		.enable = OMAP_PIN_OUTPUT | OMAP_MUX_MODE0,
	},
	{
		.name   = "uart3_tx_irtx.uart3_tx_irtx",
		.enable = OMAP_PIN_OUTPUT | OMAP_MUX_MODE0,
	},
	{
		.name   = "uart3_rx_irrx.uart3_rx_irrx",
		.flags  = OMAP_DEVICE_PAD_REMUX | OMAP_DEVICE_PAD_WAKEUP,
		.enable = OMAP_PIN_INPUT_PULLUP | OMAP_MUX_MODE0,
		.idle   = OMAP_PIN_INPUT_PULLUP | OMAP_MUX_MODE0,
	},
};

static struct omap_device_pad uart5_pads[] __initdata = {
	{
		.name   = "uart5_cts.uart5_cts",
		.enable = OMAP_PIN_INPUT_PULLUP | OMAP_MUX_MODE0,
	},
	{
		.name   = "uart5_rts.uart5_rts",
		.enable = OMAP_PIN_OUTPUT | OMAP_MUX_MODE0,
	},
	{
		.name   = "uart5_tx.uart5_tx",
		.enable = OMAP_PIN_OUTPUT | OMAP_MUX_MODE0,
	},
	{
		.name   = "uart5_rx.uart5_rx",
		.flags  = OMAP_DEVICE_PAD_REMUX | OMAP_DEVICE_PAD_WAKEUP,
		.enable = OMAP_PIN_INPUT_PULLUP | OMAP_MUX_MODE0,
		.idle   = OMAP_PIN_INPUT_PULLUP | OMAP_MUX_MODE0,
	},
};

static struct omap_board_data uart1_board_data __initdata = {
	.id = 0,
	.pads = uart1_pads,
	.pads_cnt = ARRAY_SIZE(uart1_pads),
};

static struct omap_board_data uart3_board_data __initdata = {
	.id = 2,
	.pads = uart3_pads,
	.pads_cnt = ARRAY_SIZE(uart3_pads),
};

static struct omap_board_data uart5_board_data __initdata = {
	.id = 4,
	.pads = uart5_pads,
	.pads_cnt = ARRAY_SIZE(uart5_pads),
};

static struct omap_uart_port_info uart1_info __initdata = {
	.dma_enabled = 0,
	.dma_rx_buf_size = 4096,
	.dma_rx_poll_rate = 1,
	.dma_rx_timeout = (3 * HZ),
	.autosuspend_timeout = -1,
};

static struct omap_uart_port_info uart3_info __initdata = {
	.dma_enabled = 0,
	.dma_rx_buf_size = 4096,
	.dma_rx_poll_rate = 1,
	.dma_rx_timeout = (3 * HZ),
	.autosuspend_timeout = -1,
};

static struct omap_uart_port_info uart5_info __initdata = {
	.dma_enabled = 0,
	.dma_rx_buf_size = 4096,
	.dma_rx_poll_rate = 1,
	.dma_rx_timeout = (3 * HZ),
	.autosuspend_timeout = -1,
};

void __init omap5_board_serial_init(void)
{
	omap_serial_init_port(&uart1_board_data, &uart1_info);
	omap_serial_init_port(&uart3_board_data, &uart3_info);
	omap_serial_init_port(&uart5_board_data, &uart5_info);
}

