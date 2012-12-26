/*
 * arch/arm/mach-omap2/board-44xx-serial.c
 *
 * Support for connectivity devices on OMAP44xx platforms.
 *
 * Copyright (C) 2012 Texas Instruments
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/init.h>
#include <plat/omap-serial.h>
#include "mux.h"

#define DEFAULT_UART_AUTOSUSPEND_DELAY	3000	/* Runtime autosuspend (msecs)*/

/*
 *  UART: Tablet platform data
 */

static struct omap_device_pad uart2_pads[] __initdata = {
	{
		.name	= "uart2_cts.uart2_cts",
		.enable	= OMAP_PIN_INPUT_PULLUP | OMAP_MUX_MODE0,
		.flags  = OMAP_DEVICE_PAD_REMUX,
		.idle   = OMAP_WAKEUP_EN | OMAP_PIN_OFF_INPUT_PULLUP |
			  OMAP_MUX_MODE0,
	},
	{
		.name	= "uart2_rts.uart2_rts",
		.flags  = OMAP_DEVICE_PAD_REMUX,
		.enable	= OMAP_PIN_OUTPUT | OMAP_MUX_MODE0,
		.idle   = OMAP_PIN_OFF_INPUT_PULLUP | OMAP_MUX_MODE7,
	},
	{
		.name	= "uart2_tx.uart2_tx",
		.enable	= OMAP_PIN_OUTPUT | OMAP_MUX_MODE0,
	},
	{
		.name	= "uart2_rx.uart2_rx",
		.enable	= OMAP_PIN_INPUT_PULLUP | OMAP_MUX_MODE0,
	},
};

static struct omap_device_pad uart3_pads[] __initdata = {
	{
		.name	= "uart3_cts_rctx.uart3_cts_rctx",
		.enable	= OMAP_PIN_INPUT_PULLUP | OMAP_MUX_MODE0,
	},
	{
		.name	= "uart3_rts_sd.uart3_rts_sd",
		.enable	= OMAP_PIN_OUTPUT | OMAP_MUX_MODE0,
	},
	{
		.name	= "uart3_tx_irtx.uart3_tx_irtx",
		.enable	= OMAP_PIN_OUTPUT | OMAP_MUX_MODE0,
	},
	{
		.name	= "uart3_rx_irrx.uart3_rx_irrx",
		.flags	= OMAP_DEVICE_PAD_REMUX | OMAP_DEVICE_PAD_WAKEUP,
		.enable	= OMAP_PIN_INPUT | OMAP_MUX_MODE0,
		.idle	= OMAP_PIN_INPUT | OMAP_MUX_MODE0,
	},
};

static struct omap_device_pad uart4_pads[] __initdata = {
	{
		.name	= "uart4_tx.uart4_tx",
		.enable	= OMAP_PIN_OUTPUT | OMAP_MUX_MODE0,
	},
	{
		.name	= "uart4_rx.uart4_rx",
		.flags	= OMAP_DEVICE_PAD_REMUX | OMAP_DEVICE_PAD_WAKEUP,
		.enable	= OMAP_PIN_INPUT | OMAP_MUX_MODE0,
		.idle	= OMAP_PIN_INPUT | OMAP_MUX_MODE0,
	},
};

static struct omap_board_data uart2_board_data __initdata = {
	.id = 1,
	.pads = uart2_pads,
	.pads_cnt = ARRAY_SIZE(uart2_pads),
};

static struct omap_board_data uart3_board_data __initdata = {
	.id = 2,
	.pads = uart3_pads,
	.pads_cnt = ARRAY_SIZE(uart3_pads),
};

static struct omap_board_data uart4_board_data __initdata = {
	.id = 3,
	.pads = uart4_pads,
	.pads_cnt = ARRAY_SIZE(uart4_pads),
};

static struct omap_uart_port_info uart2_info __initdata = {
	.dma_enabled = 0,
	.dma_rx_buf_size = 4096,
	.dma_rx_poll_rate = 1,
	.dma_rx_timeout = 3 * HZ,
	.autosuspend_timeout = DEFAULT_UART_AUTOSUSPEND_DELAY,
};

static struct omap_uart_port_info uart3_info __initdata = {
	.dma_enabled = 0,
	.dma_rx_buf_size = 4096,
	.dma_rx_poll_rate = 1,
	.dma_rx_timeout = 3 * HZ,
	.autosuspend_timeout = DEFAULT_UART_AUTOSUSPEND_DELAY,
};

static struct omap_uart_port_info uart4_info __initdata = {
	.dma_enabled = 0,
	.dma_rx_buf_size = 4096,
	.dma_rx_poll_rate = 1,
	.dma_rx_timeout = 3 * HZ,
	.autosuspend_timeout = DEFAULT_UART_AUTOSUSPEND_DELAY,
};

/*
 * UART: Tablet platform initialization
 */

void __init omap4_board_serial_init(void)
{
	omap_serial_init_port(&uart2_board_data, &uart2_info);
	omap_serial_init_port(&uart3_board_data, &uart3_info);
	omap_serial_init_port(&uart4_board_data, &uart4_info);
}
