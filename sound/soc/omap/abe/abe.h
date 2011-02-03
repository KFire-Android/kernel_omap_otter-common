/*

  This file is provided under a dual BSD/GPLv2 license.  When using or
  redistributing this file, you may do so under either license.

  GPL LICENSE SUMMARY

  Copyright(c) 2010-2011 Texas Instruments Incorporated,
  All rights reserved.

  This program is free software; you can redistribute it and/or modify
  it under the terms of version 2 of the GNU General Public License as
  published by the Free Software Foundation.

  This program is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
  The full GNU General Public License is included in this distribution
  in the file called LICENSE.GPL.

  BSD LICENSE

  Copyright(c) 2010-2011 Texas Instruments Incorporated,
  All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions
  are met:

    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in
      the documentation and/or other materials provided with the
      distribution.
    * Neither the name of Texas Instruments Incorporated nor the names of
      its contributors may be used to endorse or promote products derived
      from this software without specific prior written permission.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*/

#ifndef _ABE_H_
#define _ABE_H_

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/io.h>

#include "abe_def.h"
#include "abe_typ.h"
#include "abe_ext.h"
#include "abe_dm_addr.h"


#include <linux/debugfs.h>


/*
 * OS DEPENDENT MMU CONFIGURATION
 */
#define ABE_PMEM_BASE_OFFSET_MPU	0xe0000
#define ABE_CMEM_BASE_OFFSET_MPU	0xa0000
#define ABE_SMEM_BASE_OFFSET_MPU	0xc0000
#define ABE_DMEM_BASE_OFFSET_MPU	0x80000
#define ABE_ATC_BASE_OFFSET_MPU		0xf1000
/* default base address for io_base */
#define ABE_DEFAULT_BASE_ADDRESS_L3 0x49000000L
#define ABE_DEFAULT_BASE_ADDRESS_L4 0x40100000L
#define ABE_DEFAULT_BASE_ADDRESS_DEFAULT ABE_DEFAULT_BASE_ADDRESS_L3

/*
 * TODO: These structures, enums and port ID macros should be moved to the
 * new public ABE API header.
 */

/* Logical PORT IDs - Backend */
#define OMAP_ABE_BE_PORT_DMIC0			0
#define OMAP_ABE_BE_PORT_DMIC1			1
#define OMAP_ABE_BE_PORT_DMIC2			2
#define OMAP_ABE_BE_PORT_PDM_DL1		3
#define OMAP_ABE_BE_PORT_PDM_DL2		4
#define OMAP_ABE_BE_PORT_PDM_VIB		5
#define OMAP_ABE_BE_PORT_PDM_UL1		6
#define OMAP_ABE_BE_PORT_BT_VX_DL		7
#define OMAP_ABE_BE_PORT_BT_VX_UL		8
#define OMAP_ABE_BE_PORT_MM_EXT_UL		9
#define OMAP_ABE_BE_PORT_MM_EXT_DL		10

/* Logical PORT IDs - Frontend */
#define OMAP_ABE_FE_PORT_MM_DL1		11
#define OMAP_ABE_FE_PORT_MM_UL1		12
#define OMAP_ABE_FE_PORT_MM_UL2		13
#define OMAP_ABE_FE_PORT_VX_DL		14
#define OMAP_ABE_FE_PORT_VX_UL		15
#define OMAP_ABE_FE_PORT_VIB		16
#define OMAP_ABE_FE_PORT_TONES		17
#define OMAP_ABE_FE_PORT_MM_DL_LP	18

#define OMAP_ABE_MAX_PORT_ID	OMAP_ABE_FE_PORT_MM_DL_LP

/* ports can either be enabled or disabled */
enum port_state {
	PORT_DISABLED = 0,
	PORT_ENABLED,
};

/* structure used for client port info */
struct omap_abe_port {

	/* logical and physical port IDs that correspond this port */
	int logical_id;
	int physical_id;
	int physical_users;

	/* enabled or disabled */
	enum port_state state;

	/* logical port ref count */
	int users;

	struct list_head list;
	struct omap_aess *abe;

#ifdef CONFIG_DEBUG_FS
	struct dentry *debugfs_lstate;
	struct dentry *debugfs_lphy;
	struct dentry *debugfs_lusers;
#endif
};

struct omap_abe_port *omap_abe_port_open(struct omap_aess *abe, int logical_id);
void omap_abe_port_close(struct omap_aess *abe, struct omap_abe_port *port);
int omap_abe_port_enable(struct omap_aess *abe, struct omap_abe_port *port);
int omap_abe_port_disable(struct omap_aess *abe, struct omap_abe_port *port);
int omap_abe_port_is_enabled(struct omap_aess *abe, struct omap_abe_port *port);
struct omap_aess *omap_abe_port_mgr_get(void);
void omap_abe_port_mgr_put(struct omap_aess *abe);

struct omap_aess_seq {
	u32 write_pointer;
	u32 irq_pingpong_player_id;
};

/* main ABE structure */
struct omap_aess {
	void __iomem *io_base[5];
	u32 firmware_version_number;
	u16 MultiFrame[25][8];
	u32 compensated_mixer_gain;
	u8  muted_gains_indicator[MAX_NBGAIN_CMEM];
	u32 desired_gains_decibel[MAX_NBGAIN_CMEM];
	u32 muted_gains_decibel[MAX_NBGAIN_CMEM];
	u32 desired_gains_linear[MAX_NBGAIN_CMEM];
	u32 desired_ramp_delay_ms[MAX_NBGAIN_CMEM];
	int pp_buf_id;
	int pp_buf_id_next;
	int pp_buf_addr[4];
	int pp_first_irq;
	struct mutex mutex;
	u32 warm_boot;

	/* base addresses of the ping pong buffers in bytes addresses */
	u32 base_address_pingpong[MAX_PINGPONG_BUFFERS];
	/* size of each ping/pong buffers */
	u32 size_pingpong;
	/* number of ping/pong buffer being used */
	u32 nb_pingpong;

	u32 irq_dbg_read_ptr;
	u32 dbg_param;
	struct omap_aess_dbg *dbg;
	struct omap_aess_seq seq;

	/* List of open ABE logical ports */
	struct list_head ports;

	/* spinlock */
	spinlock_t lock;

#ifdef CONFIG_DEBUG_FS
	struct dentry *debugfs_root;
#endif
};

#include "abe_gain.h"

struct omap_aess_equ {
	/* type of filter */
	u32 equ_type;
	/* filter length */
	u32 equ_length;
	union {
		/* parameters are the direct and recursive coefficients in */
		/* Q6.26 integer fixed-point format. */
		s32 type1[NBEQ1];
		struct {
			/* center frequency of the band [Hz] */
			s32 freq[NBEQ2];
			/* gain of each band. [dB] */
			s32 gain[NBEQ2];
			/* Q factor of this band [dB] */
			s32 q[NBEQ2];
		} type2;
	} coef;
	s32 equ_param3;
};


struct omap_aess_dma {
	/* OCP L3 pointer to the first address of the */
	void *data;
	/* destination buffer (either DMA or Ping-Pong read/write pointers). */
	/* address L3 when addressing the DMEM buffer instead of CBPr */
	void *l3_dmem;
	/* address L3 translated to L4 the ARM memory space */
	void *l4_dmem;
	/* number of iterations for the DMA data moves. */
	u32 iter;
};



int omap_aess_set_opp_processing(struct omap_aess *abe, u32 opp);
int omap_aess_connect_debug_trace(struct omap_aess *abe,
				 struct omap_aess_dma *dma2);

/* gain */
int omap_aess_use_compensated_gain(struct omap_aess *abe, int on_off);
int omap_aess_write_equalizer(struct omap_aess *abe,
			     u32 id, struct omap_aess_equ *param);

int omap_aess_disable_gain(struct omap_aess *abe, u32 id);
int omap_aess_enable_gain(struct omap_aess *abe, u32 id);
int omap_aess_mute_gain(struct omap_aess *abe, u32 id);
int omap_aess_unmute_gain(struct omap_aess *abe, u32 id);

int omap_aess_write_gain(struct omap_aess *abe,	u32 id, s32 f_g);
int omap_aess_write_mixer(struct omap_aess *abe, u32 id, s32 f_g);
int omap_aess_read_gain(struct omap_aess *abe, u32 id, u32 *f_g);
int omap_aess_read_mixer(struct omap_aess *abe, u32 id, u32 *f_g);

u32 *omap_aess_get_default_fw(void);

int omap_aess_init_mem(struct omap_aess *abe, void __iomem **_io_base);
int omap_aess_reset_hal(struct omap_aess *abe);
int omap_aess_load_fw_param(struct omap_aess *abe, u32 *data);
int omap_aess_load_fw(struct omap_aess *abe, u32 *firmware);
int omap_aess_reload_fw(struct omap_aess *abe, u32 *firmware);
u32 omap_abe_get_supported_fw_version(void);

/* port */
int omap_aess_mono_mixer(struct omap_aess *abe, u32 id, u32 on_off);
int omap_aess_connect_serial_port(struct omap_aess *abe,
				 u32 id, struct omap_aess_data_format *f,
				 u32 mcbsp_id);
int omap_aess_connect_cbpr_dmareq_port(struct omap_aess *abe,
						u32 id, struct omap_aess_data_format *f,
						u32 d,
						struct omap_aess_dma *returned_dma_t);
int omap_aess_read_port_address(struct omap_aess *abe,
					 u32 port, struct omap_aess_dma *dma2);
int omap_aess_connect_irq_ping_pong_port(struct omap_aess *abe,
					u32 id, struct omap_aess_data_format *f,
					u32 subroutine_id, u32 size,
					u32 *sink, u32 dsp_mcu_flag);
void omap_aess_write_pdmdl_offset(struct omap_aess *abe, u32 path, u32 offset_left, u32 offset_right);
int omap_aess_enable_data_transfer(struct omap_aess *abe, u32 id);
int omap_aess_disable_data_transfer(struct omap_aess *abe, u32 id);

/* core */
int omap_aess_check_activity(struct omap_aess *abe);
int omap_aess_wakeup(struct omap_aess *abe);
int omap_aess_set_router_configuration(struct omap_aess *abe,
				      u32 id, u32 k, u32 *param);
int omap_abe_read_next_ping_pong_buffer(struct omap_aess *abe, u32 port, u32 *p, u32 *n);
int omap_aess_read_next_ping_pong_buffer(struct omap_aess *abe, u32 port, u32 *p, u32 *n);
int omap_aess_irq_processing(struct omap_aess *abe);
int omap_aess_set_ping_pong_buffer(struct omap_aess *abe, u32 port, u32 n_bytes);
int omap_aess_read_offset_from_ping_buffer(struct omap_aess *abe,
					  u32 id, u32 *n);
/* seq */
int omap_aess_plug_subroutine(struct omap_aess *abe, u32 *id, abe_subroutine2 f, u32 n,
				  u32 *params);

#endif/* _ABE_H_ */
