/*
 *  FM Driver for Connectivity chip of Texas Instruments.
 *  FM RX module header.
 *
 *  Copyright (C) 2010 Texas Instruments
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#ifndef _FMDRV_RX_H
#define _FMDRV_RX_H

int fm_rx_set_frequency(struct fmdrv_ops*, unsigned int);
int fm_rx_set_mute_mode(struct fmdrv_ops*, unsigned char);
int fm_rx_set_stereo_mono(struct fmdrv_ops*, unsigned short);
int fm_rx_set_rds_mode(struct fmdrv_ops*, unsigned char);
int fm_rx_set_rds_system(struct fmdrv_ops *, unsigned char);
int fm_rx_set_volume(struct fmdrv_ops*, unsigned short);
int fm_rx_set_rssi_threshold(struct fmdrv_ops*, short);
int fm_rx_set_region(struct fmdrv_ops*, unsigned char);
int fm_rx_set_rfdepend_softmute(struct fmdrv_ops *, unsigned char);
int fm_rx_set_deemphasis_mode(struct fmdrv_ops *, unsigned short);
int fm_rx_set_af_switch(struct fmdrv_ops *, unsigned char);

void fm_rx_reset_rds_cache(struct fmdrv_ops *);
void fm_rx_reset_curr_station_info(struct fmdrv_ops *);

int fm_rx_seek(struct fmdrv_ops*, unsigned int, unsigned int);

int fm_rx_get_rds_mode(struct fmdrv_ops*, unsigned char*);
int fm_rx_get_rds_system(struct fmdrv_ops *, unsigned char*);
int fm_rx_get_mute_mode(struct fmdrv_ops*, unsigned char*);
int fm_rx_get_volume(struct fmdrv_ops*, unsigned short*);
int fm_rx_get_currband_lowhigh_freq(struct fmdrv_ops*,
					unsigned int*, unsigned int*);
int fm_rx_get_stereo_mono(struct fmdrv_ops *, unsigned short*);
int fm_rx_get_rssi_level(struct fmdrv_ops *, unsigned short*);
int fm_rx_get_rssi_threshold(struct fmdrv_ops *, short*);
int fm_rx_get_rfdepend_softmute(struct fmdrv_ops *, unsigned char*);
int fm_rx_get_deemphasis_mode(struct fmdrv_ops *, unsigned short*);
int fm_rx_get_af_switch(struct fmdrv_ops *, unsigned char *);

#endif

