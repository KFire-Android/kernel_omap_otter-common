/*
 * board IDME driver header file
 *
 * Copyright (C) 2011 Amazon Inc., All Rights Reserved.
 *
 * 
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _BOWSER_IDME_INIT_H_
#define _BOWSER_IDME_INIT_H_

#define BOARD_TYPE_BOWSER 0
#define BOARD_TYPE_JEM 1
#define BOARD_TYPE_TATE 2
/*From Jem EVT2 */
#define BOARD_TYPE_JEM_WIFI	801
#define BOARD_TYPE_JEM_WAN	802
/*From Tate EVT2.1*/
#define BOARD_TYPE_TATE_EVT2_1	803
#define BOARD_TYPE_TATE_EVT3	803

#define BOARD_TYPE_RADLEY	804

/* DSN struct: PCDDHHUUYWWD####*/
#define SERIAL_YEAR_OFFSET	8
#define SERIAL_YEAR_LEN		1
#define SERIAL_WEEK_OFFSET	9
#define SERIAL_WEEK_LEN		2

#define IDME_BOARD_VERSION_EVT3 2

enum idme_board_type{
	IDME_BOARD_TYPE_BOWER1=0,
	IDME_BOARD_TYPE_BOWER2,
	IDME_BOARD_TYPE_BOWER3,
	IDME_BOARD_TYPE_BOWER4,
	IDME_BOARD_TYPE_BOWER5,
	IDME_BOARD_TYPE_BOWER6,
	IDME_BOARD_TYPE_BOWER7,
	IDME_BOARD_TYPE_BOWER8,
	IDME_BOARD_TYPE_BOWER9,
	IDME_BOARD_TYPE_JEM_PROTO,
	IDME_BOARD_TYPE_JEM_EVT1,
	IDME_BOARD_TYPE_JEM_EVT1_2_WIFI,
	IDME_BOARD_TYPE_JEM_EVT1_2_WAN,
	IDME_BOARD_TYPE_JEM_EVT2_WIFI,
	IDME_BOARD_TYPE_JEM_EVT2_WAN,
	IDME_BOARD_TYPE_JEM_EVT3_WIFI,
	IDME_BOARD_TYPE_JEM_EVT3_WAN,
	IDME_BOARD_TYPE_JEM_DVT_WIFI,
	IDME_BOARD_TYPE_JEM_DVT_WAN,
	IDME_BOARD_TYPE_JEM_PVT_WIFI,
	IDME_BOARD_TYPE_JEM_PVT_WAN,
	IDME_BOARD_TYPE_TATE_PROTO,
	IDME_BOARD_TYPE_TATE_EVT_PRE,
	IDME_BOARD_TYPE_TATE_EVT1,
	IDME_BOARD_TYPE_TATE_EVT1_0A,
	IDME_BOARD_TYPE_TATE_EVT1_1,
	IDME_BOARD_TYPE_TATE_EVT2,
	IDME_BOARD_TYPE_TATE_EVT2_1,
	IDME_BOARD_TYPE_TATE_EVT3,
	IDME_BOARD_TYPE_RADLEY_EVT1,
	IDME_BOARD_TYPE_RADLEY_EVT2,
};


void bowser_init_idme(void);
int is_bowser_rev4(void);
int idme_query_board_type(const enum idme_board_type bt);
int board_has_wan(void);
#ifdef CONFIG_MACH_OMAP4_BOWSER_SUBTYPE_JEM
int is_wifi_semco(void);
int is_wifi_usi(void);
int idme_is_good_panel(void);
#endif
int is_radley_device(void);
int board_has_usb_host(void);
#endif
