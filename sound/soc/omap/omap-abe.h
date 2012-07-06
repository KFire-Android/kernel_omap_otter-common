/*
 * omap-abe.h
 *
 * Copyright (C) 2010 Texas Instruments
 *
 * Contact: Liam Girdwood <lrg@ti.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 */

#ifndef __OMAP_ABE_H__
#define __OMAP_ABE_H__

#define ABE_FRONTEND_DAI_MEDIA		0
#define ABE_FRONTEND_DAI_MEDIA_CAPTURE	1
#define ABE_FRONTEND_DAI_VOICE		2
#define ABE_FRONTEND_DAI_TONES		3
#define ABE_FRONTEND_DAI_VIBRA		4
#define ABE_FRONTEND_DAI_MODEM		5
#define ABE_FRONTEND_DAI_LP_MEDIA	6
#define ABE_FRONTEND_DAI_NUM		7

/* This must currently match the BE order in DSP */
#define OMAP_ABE_DAI_PDM_UL			0
#define OMAP_ABE_DAI_PDM_DL1			1
#define OMAP_ABE_DAI_PDM_DL2			2
#define OMAP_ABE_DAI_PDM_VIB			3
#define OMAP_ABE_DAI_BT_VX			4
#define OMAP_ABE_DAI_MM_FM			5
#define OMAP_ABE_DAI_MODEM			6
#define OMAP_ABE_DAI_DMIC0			7
#define OMAP_ABE_DAI_DMIC1			8
#define OMAP_ABE_DAI_DMIC2			9
#define OMAP_ABE_DAI_NUM			10

#define OMAP_ABE_BE_PDM_DL1		"PDM-DL1"
#define OMAP_ABE_BE_PDM_UL1		"PDM-UL1"
#define OMAP_ABE_BE_PDM_DL2		"PDM-DL2"
#define OMAP_ABE_BE_PDM_VIB		"PDM-VIB"
#define OMAP_ABE_BE_BT_VX_UL		"BT-VX-UL"
#define OMAP_ABE_BE_BT_VX_DL		"BT-VX-DL"
//#define OMAP_ABE_BE_MM_EXT0		"FM-EXT"
#define OMAP_ABE_BE_MM_EXT0_DL		"FM-EXT-DL"
#define OMAP_ABE_BE_MM_EXT0_UL		"FM-EXT-UL"
#define OMAP_ABE_BE_MM_EXT1		"MODEM-EXT"
#define OMAP_ABE_BE_DMIC0		"DMIC0"
#define OMAP_ABE_BE_DMIC1		"DMIC1"
#define OMAP_ABE_BE_DMIC2		"DMIC2"

#define OMAP_ABE_DL1_NO_PDM		0
#define OMAP_ABE_DL1_HEADSET_LP		1
#define OMAP_ABE_DL1_HEADSET_HP		2
#define OMAP_ABE_DL1_EARPIECE		3
#if !defined(CONFIG_SND_OMAP_SOC_ABE_DL2)
#define OMAP_ABE_DL1_HANDSFREE		4
#endif

int omap_abe_set_dl1_output(int output);

/* Port connection configuration structure Refer abe_type.h for enums */
typedef struct 
{
	int abe_port_id_ul;	/*Which port id to be connected for  up link */
	int serial_id_ul;	/*Which mcbsp id is connected to above mentioned port   */
	int sample_format_ul;	/* ABE format ? */
	int sample_rate_ul;	/*Sample rate ? */
	int bit_reorder_ul;	/* 1: transfer LSB first   0: transfer MSB first */
	int abe_port_id_dl;	/*Which port id to be connected for  down link */
	int serial_id_dl;	/*Which mcbsp id is connected to above mentioned port   */
	int sample_format_dl;	/* ABE format ? */
	int sample_rate_dl;	/* Sample rate ? */
	int bit_reorder_dl;	/* 1: transfer LSB first   0: transfer MSB first */
}t_port_config;
#endif	/* End of __OMAP_MCPDM_H__ */
