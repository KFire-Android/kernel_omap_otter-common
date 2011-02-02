/*
 * ALSA SoC OMAP ABE driver
 *
 * Author:	Laurent Le Faucheur <l-le-faucheur@ti.com>
 *		Liam Girdwood <lrg@slimlogic.co.uk>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 */
#include "abe_main.h"
/*
 * DEFINE
 */
#define NO_OUTPUT 0
#define TERMINAL_OUTPUT 1
#define LINE_OUTPUT 2
#define DEBUG_TRACE_OUTPUT 3
/*
 *	Debug trace format
 *	TIME 2 bytes from ABE : 4kHz period of the FW scheduler
 *	SUBID 1 byte : HAL API index
 * From 0 to 16 bytes : parameters of the subroutine
 * on every 32 dumps a tag is pushed on the debug trace : 0x55555555
 */
#define dbg_bitfield_offset 8
#define dbg_api_calls 0
#define dbg_mapi (1L << (dbg_api_calls + dbg_bitfield_offset))
#define dbg_external_data_access 1
#define dbg_mdata (1L << (dbg_external_data_access + dbg_bitfield_offset))
#define dbg_err_codes 2
#define dbg_merr (1L << (dbg_api_calls + dbg_bitfield_offset))
#define ABE_DBG_MAGIC_NUMBER 0x55555555
/*
 * IDs used for traces
 */
#define id_reset_hal (1 + dbg_mapi)
#define id_load_fw (2 + dbg_mapi)
#define id_default_configuration (3 + dbg_mapi)
#define id_irq_processing (4 + dbg_mapi)
#define id_event_generator_switch (5 + dbg_mapi)
#define id_read_hardware_configuration (6 + dbg_mapi)
#define id_read_lowest_opp (7 + dbg_mapi)
#define id_write_gain (8 + dbg_mapi)
#define id_set_asrc_drift_control (9 + dbg_mapi)
#define id_plug_subroutine (10 + dbg_mapi)
#define id_unplug_subroutine (11 + dbg_mapi)
#define id_plug_sequence (12 + dbg_mapi)
#define id_launch_sequence (13 + dbg_mapi)
#define id_launch_sequence_param (14 + dbg_mapi)
#define id_connect_irq_ping_pong_port (15 + dbg_mapi)
#define id_read_analog_gain_dl (16 + dbg_mapi)
#define id_read_analog_gain_ul (17 + dbg_mapi)
#define id_enable_dyn_ul_gain (18 + dbg_mapi)
#define id_disable_dyn_ul_gain (19 + dbg_mapi)
#define id_enable_dyn_extension (20 + dbg_mapi)
#define id_disable_dyn_extension (21 + dbg_mapi)
#define id_notify_analog_gain_changed (22 + dbg_mapi)
#define id_reset_port (23 + dbg_mapi)
#define id_read_remaining_data (24 + dbg_mapi)
#define id_disable_data_transfer (25 + dbg_mapi)
#define id_enable_data_transfer (26 + dbg_mapi)
#define id_read_global_counter (27 + dbg_mapi)
#define id_set_dmic_filter (28 + dbg_mapi)
#define id_set_opp_processing (29 + dbg_mapi)
#define id_set_ping_pong_buffer (30 + dbg_mapi)
#define id_read_port_address (31 + dbg_mapi)
#define id_load_fw_param (32 + dbg_mapi)
#define id_write_headset_offset (33 + dbg_mapi)
#define id_read_gain_ranges (34 + dbg_mapi)
#define id_write_equalizer (35 + dbg_mapi)
#define id_write_asrc (36 + dbg_mapi)
#define id_write_aps (37 + dbg_mapi)
#define id_write_mixer (38 + dbg_mapi)
#define id_write_eanc (39 + dbg_mapi)
#define id_write_router (40 + dbg_mapi)
#define id_read_port_gain (41 + dbg_mapi)
#define id_read_asrc (42 + dbg_mapi)
#define id_read_aps (43 + dbg_mapi)
#define id_read_aps_energy (44 + dbg_mapi)
#define id_read_mixer (45 + dbg_mapi)
#define id_read_eanc (46 + dbg_mapi)
#define id_read_router (47 + dbg_mapi)
#define id_read_debug_trace (48 + dbg_mapi)
#define id_set_sequence_time_accuracy (49 + dbg_mapi)
#define id_set_debug_pins (50 + dbg_mapi)
#define id_select_main_port (51 + dbg_mapi)
#define id_write_event_generator (52 + dbg_mapi)
#define id_read_use_case_opp (53 + dbg_mapi)
#define id_select_data_source (54 + dbg_mapi)
#define id_read_next_ping_pong_buffer (55 + dbg_mapi)
#define id_init_ping_pong_buffer (56 + dbg_mapi)
#define id_connect_cbpr_dmareq_port (57 + dbg_mapi)
#define id_connect_dmareq_port (58 + dbg_mapi)
#define id_connect_dmareq_ping_pong_port (59 + dbg_mapi)
#define id_connect_serial_port (60 + dbg_mapi)
#define id_connect_slimbus_port (61 + dbg_mapi)
#define id_read_gain (62 + dbg_mapi)
#define id_set_router_configuration (63 + dbg_mapi)
#define id_connect_debug_trace (64 + dbg_mapi)
#define id_set_debug_trace (65 + dbg_mapi)
#define id_remote_debugger_interface (66 + dbg_mapi)
#define id_enable_test_pattern (67 + dbg_mapi)
#define id_connect_tdm_port (68 + dbg_mapi)
/*
 * IDs used for error codes
 */
#define NOERR 0
#define ABE_SET_MEMORY_CONFIG_ERR (1 + dbg_merr)
#define ABE_BLOCK_COPY_ERR (2 + dbg_merr)
#define ABE_SEQTOOLONG (3 + dbg_merr)
#define ABE_BADSAMPFORMAT (4 + dbg_merr)
#define ABE_SET_ATC_MEMORY_CONFIG_ERR (5 + dbg_merr)
#define ABE_PROTOCOL_ERROR (6 + dbg_merr)
#define ABE_PARAMETER_ERROR (7 + dbg_merr)
/*  port programmed while still running */
#define ABE_PORT_REPROGRAMMING (8 + dbg_merr)
#define ABE_READ_USE_CASE_OPP_ERR (9 + dbg_merr)
#define ABE_PARAMETER_OVERFLOW (10 + dbg_merr)
#define ABE_FW_FIFO_WRITE_PTR_ERR (11 + dbg_merr)
/*
 * IDs used for error codes
 */
/* error in the LIB.C file */
#define ERR_LIB (1 << 1)
/* error in the API.C file */
#define ERR_API (1 << 2)
/* error in the INI.C file */
#define ERR_INI (1 << 3)
/* error in the SEQ.C file */
#define ERR_SEQ (1 << 4)
/* error in the DBG.C file */
#define ERR_DBG (1 << 5)
/* error in the DBG.C file */
#define ERR_EXT (1 << 6)
/*
 * MACROS
 */
#define _log(x, y, z, t) { if (x & abe->dbg_mask) abe_dbg_log(x, y, z, t); }

