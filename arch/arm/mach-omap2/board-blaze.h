/*
 * arch/arm/mach-omap2/board-blaze.h
 *
 * Copyright (C) 2011 Texas Instruments
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

#ifndef _MACH_OMAP_BOARD_44XX_SDP_H
#define _MACH_OMAP_BOARD_44XX_SDP_H

int blaze_touch_init(void);
int blaze_sensor_init(void);
int blaze_panel_init(void);
int blaze_keypad_init(void);
void blaze_modem_init(bool force_mux);
void omap4_create_board_props(void);
void omap4_power_init(void);
void board_serial_init(void);

#define BLAZE_MDM_PWR_EN_GPIO	157

#if defined(CONFIG_USB_EHCI_HCD_OMAP) || defined(CONFIG_USB_OHCI_HCD_OMAP3)
extern struct usbhs_omap_board_data usbhs_bdata;
#endif
#endif
