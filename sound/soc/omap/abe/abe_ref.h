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
#ifndef _ABE_REF_H_
#define _ABE_REF_H_

/*
 * 'ABE_PRO.H' all non-API prototypes for INI, IRQ, SEQ ...
 */
/*
 * HAL EXTERNAL AP
 */
/*
 * HAL INTERNAL AP
 */
void abe_decide_main_port(void);
void abe_gain_offset(u32 id, u32 *mixer_offset);
void abe_int_2_float16(u32 data, u32 *mantissa, u32 *exp);
void abe_reset_gain_mixer(u32 id, u32 p);
void abe_load_embeddded_patterns(void);
void abe_build_scheduler_table(void);
void abe_reset_one_feature(u32 x);
void abe_reset_all_features(void);
void abe_reset_all_ports(void);
void abe_reset_all_fifo(void);
void abe_reset_all_sequence(void);
int abe_dma_port_iteration(abe_data_format_t *format);
void abe_read_sys_clock(u32 *time);
void abe_enable_dma_request(u32 id);
void abe_disable_dma_request(u32 id);
void abe_enable_atc(u32 id);
void abe_disable_atc(u32 id);
void abe_init_atc(u32 id);
void abe_init_io_tasks(u32 id, abe_data_format_t *format,
			abe_port_protocol_t *prot);
void abe_enable_pp_io_task(u32 id);
void abe_disable_pp_io_task(u32 id);
void abe_init_dma_t(u32 id, abe_port_protocol_t *prot);
int abe_dma_port_iter_factor(abe_data_format_t *f);
int abe_dma_port_copy_subroutine_id(u32 i);
void abe_call_subroutine(u32 idx, u32 p1, u32 p2, u32 p3, u32 p4);
void abe_monitoring(void);
void abe_lock_execution(void);
void abe_unlock_execution(void);
void abe_hw_configuration(void);
void abe_irq_ping_pong(void);
void abe_irq_check_for_sequences(u32 seq_info);
void abe_default_irq_pingpong_player(void);
void abe_default_irq_pingpong_player_32bits(void);
void abe_rshifted16_irq_pingpong_player_32bits(void);
void abe_1616_irq_pingpong_player_1616bits(void);
void abe_default_irq_aps_adaptation(void);
void abe_irq_aps(u32 aps_info);
void abe_clean_temporary_buffers(u32 id);
void abe_dbg_log(u32 x, u32 y, u32 z, u32 t);
void abe_dbg_error_log(u32 x);
void abe_init_asrc_vx_dl(s32 dppm);
void abe_init_asrc_vx_ul(s32 dppm);
void abe_init_asrc_mm_ext_in(s32 dppm);
void abe_init_asrc_bt_ul(s32 dppm);
void abe_init_asrc_bt_dl(s32 dppm);

/*
 * HAL INTERNAL DATA
 */
extern struct omap_abe *abe;

extern const u32 abe_port_priority[LAST_PORT_ID - 1];
extern const u32 abe_firmware_array[ABE_FIRMWARE_MAX_SIZE];
extern const u32 abe_atc_srcid[];
extern const u32 abe_atc_dstid[];
extern const abe_port_t abe_port_init[];
extern const abe_feature_t all_feature_init[];
extern const abe_seq_t all_sequence_init[];
extern const abe_router_t abe_router_ul_table_preset
	[NBROUTE_CONFIG][NBROUTE_UL];
extern const abe_sequence_t seq_null;
extern const u32 abe_db2lin_table[];
extern const u32 abe_alpha_iir[64];
extern const u32 abe_1_alpha_iir[64];

extern abe_port_t abe_port[];
extern abe_feature_t feature[];
extern abe_feature_t all_feature[];
extern abe_seq_t all_sequence[];
extern abe_router_t abe_router_ul_table[NBROUTE_CONFIG_MAX][NBROUTE_UL];
extern u32 abe_current_event_id;
/* table of new subroutines called in the sequence */
extern abe_subroutine2 abe_all_subsubroutine[MAXNBSUBROUTINE];
/* number of parameters per calls */
extern u32 abe_all_subsubroutine_nparam[MAXNBSUBROUTINE];
extern u32 abe_subroutine_id[MAXNBSUBROUTINE];
extern u32 *abe_all_subroutine_params[MAXNBSUBROUTINE];
extern u32 abe_subroutine_write_pointer;
extern abe_sequence_t abe_all_sequence[MAXNBSEQUENCE];
extern u32 abe_sequence_write_pointer;
/* current number of pending sequences (avoids to look in the table) */
extern u32 abe_nb_pending_sequences;
/* pending sequences due to ressource collision */
extern u32 abe_pending_sequences[MAXNBSEQUENCE];
/* mask of unsharable ressources among other sequences */
extern u32 abe_global_sequence_mask;
/* table of active sequences */
extern abe_seq_t abe_active_sequence[MAXACTIVESEQUENCE][MAXSEQUENCESTEPS];
/* index of the plugged subroutine doing ping-pong cache-flush
	DMEM accesses */
extern u32 abe_irq_aps_adaptation_id;
/* base addresses of the ping pong buffers */
extern u32 abe_base_address_pingpong[MAX_PINGPONG_BUFFERS];
/* size of each ping/pong buffers */
extern u32 abe_size_pingpong;
/* number of ping/pong buffer being used */
extern u32 abe_nb_pingpong;


#endif/* _ABE_REF_H_ */
