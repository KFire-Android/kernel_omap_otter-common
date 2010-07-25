/*
 * ==========================================================================
 *               Texas Instruments OMAP(TM) Platform Firmware
 * (c) Copyright 2009, Texas Instruments Incorporated.  All Rights Reserved.
 *
 *  Use of this firmware is controlled by the terms and conditions found
 *  in the license agreement under which this firmware has been supplied.
 * ==========================================================================
 */

#ifndef _ABE_REF_H_
#define _ABE_REF_H_

/*
 *  'ABE_PRO.H' all non-API prototypes for INI, IRQ, SEQ ...
 */
#ifdef __cplusplus
extern "C" {
#endif

/*
 * HAL EXTERNAL API
 */

/*
 * HAL INTERNAL API
 */
void abe_load_embedded_patterns(void);
void abe_build_scheduler_table(void);
void abe_reset_all_features(void);
void abe_reset_one_port(abe_uint32 x);
void abe_reset_all_ports(void);
void abe_reset_all_fifo(void);
void abe_reset_all_sequence(void);
abe_uint32 abe_dma_port_iteration(abe_data_format_t *format);
void abe_read_sys_clock(abe_micros_t *time);
void abe_enable_dma_request(abe_port_id id);
void abe_disable_dma_request(abe_port_id id);
void abe_enable_atc(abe_port_id id);
void abe_disable_atc(abe_port_id id);
void abe_init_atc(abe_port_id id);
void abe_init_io_tasks(abe_port_id id, abe_data_format_t *format, abe_port_protocol_t *prot);
void abe_init_dma_t(abe_port_id id, abe_port_protocol_t *prot);
abe_uint32 abe_dma_port_iter_factor(abe_data_format_t *f);
abe_uint32 abe_dma_port_copy_subroutine_id(abe_port_id i);
void abe_call_subroutine(abe_uint32 idx, abe_uint32 p1, abe_uint32 p2, abe_uint32 p3, abe_uint32 p4);
void abe_monitoring(void);
void abe_lock_execution(void);
void abe_unlock_execution(void);
void abe_hw_configuration(void);
void abe_add_subroutine(abe_uint32 *id, abe_subroutine2 f,
					abe_uint32 nparam, abe_uint32* params);
void abe_read_next_ping_pong_buffer(abe_port_id port,
					abe_uint32 *p, abe_uint32 *n);
void abe_irq_ping_pong(void);
void abe_irq_check_for_sequences(abe_uint32 seq_info);
void abe_default_irq_pingpong_player(void);
void abe_default_irq_pingping_player_32bits(void);
void abe_default_irq_aps_adaptation(void);
void abe_read_hardware_configuration(abe_use_case_id *u,
					abe_opp_t *o, abe_hw_config_init_t *hw);
void abe_irq_aps(abe_uint32 aps_info);
void abe_clean_temporary_buffers(abe_port_id id);
void abe_reset_atc(abe_uint32 atc_index);
void abe_dbg_log(abe_uint32 x, abe_uint32 y, abe_uint32 z, abe_uint32 t);
void abe_dbg_error_log(abe_uint32 x);


void abe_translate_to_xmem_format(abe_int32 memory_bank,
						float fc, abe_uint32 *c);

/*
 * HAL INTERNAL DATA
 */

extern abe_port_t abe_port[];
extern abe_feature_t feature[];
extern abe_subroutine2 callbacks[];

extern abe_port_t abe_port[];
extern const abe_port_t abe_port_init[];

extern abe_feature_t all_feature[];
extern const abe_feature_t all_feature_init[];

extern abe_seq_t all_sequence[];
extern const abe_seq_t all_sequence_init[];

extern const abe_router_t abe_router_ul_table_preset[NBROUTE_CONFIG][NBROUTE_UL];
extern abe_router_t abe_router_ul_table[NBROUTE_CONFIG_MAX][NBROUTE_UL];

extern abe_uint32 abe_dbg_output;
extern abe_uint32 abe_dbg_mask;
extern abe_uint32 abe_dbg_activity_log [D_DEBUG_HAL_TASK_sizeof];
extern abe_uint32 abe_dbg_activity_log_write_pointer;
extern abe_uint32 abe_dbg_param;

extern abe_uint32 abe_global_mcpdm_control;
extern abe_event_id abe_current_event_id;

extern const abe_sequence_t seq_null;
extern abe_subroutine2 abe_all_subsubroutine[MAXNBSUBROUTINE];	/* table of new subroutines called in the sequence */
extern abe_uint32 abe_all_subsubroutine_nparam[MAXNBSUBROUTINE]; /* number of parameters per calls */
extern abe_uint32 abe_subroutine_id[MAXNBSUBROUTINE];
extern abe_uint32* abe_all_subroutine_params[MAXNBSUBROUTINE];
extern abe_uint32 abe_subroutine_write_pointer;
extern abe_sequence_t abe_all_sequence[MAXNBSEQUENCE]; /* table of all sequences */
extern abe_uint32 abe_sequence_write_pointer;
extern abe_uint32 abe_nb_pending_sequences;	/* current number of pending sequences (avoids to look in the table) */
extern abe_uint32 abe_pending_sequences[MAXNBSEQUENCE]; /* pending sequences due to ressource collision */
extern abe_uint32 abe_global_sequence_mask;	/* mask of unsharable ressources among other sequences */
extern abe_seq_t abe_active_sequence[MAXACTIVESEQUENCE][MAXSEQUENCESTEPS]; /* table of active sequences */
extern abe_uint32 abe_irq_pingpong_player_id;	/* index of the plugged subroutine doing ping-pong cache-flush DMEM accesses */
extern abe_uint32 abe_irq_aps_adaptation_id;
extern abe_uint32 abe_base_address_pingpong[MAX_PINGPONG_BUFFERS]; /* base addresses of the ping pong buffers */
extern abe_uint32 abe_size_pingpong;		/* size of each ping/pong buffers */
extern abe_uint32 abe_nb_pingpong;		/* number of ping/pong buffer being used */

extern abe_uint32 abe_irq_dbg_read_ptr;

extern volatile abe_uint32		just_to_avoid_the_many_warnings;
extern volatile abe_gain_t		just_to_avoid_the_many_warnings_abe_gain_t;
extern volatile abe_ramp_t		just_to_avoid_the_many_warnings_abe_ramp_t;
extern volatile abe_dma_t		just_to_avoid_the_many_warnings_abe_dma_t;
extern volatile abe_port_id		just_to_avoid_the_many_warnings_abe_port_id;
extern volatile abe_millis_t		just_to_avoid_the_many_warnings_abe_millis_t;
extern volatile abe_micros_t		just_to_avoid_the_many_warnings_abe_micros_t;
extern volatile abe_patch_rev		just_to_avoid_the_many_warnings_abe_patch_rev;
extern volatile abe_sequence_t		just_to_avoid_the_many_warnings_abe_sequence_t;
extern volatile abe_ana_port_id		just_to_avoid_the_many_warnings_abe_ana_port_id;
extern volatile abe_time_stamp_t	just_to_avoid_the_many_warnings_abe_time_stamp_t;
extern volatile abe_data_format_t	just_to_avoid_the_many_warnings_abe_data_format_t;
extern volatile abe_port_protocol_t	just_to_avoid_the_many_warnings_abe_port_protocol_t;
extern volatile abe_router_t		just_to_avoid_the_many_warnings_abe_router_t;
extern volatile abe_router_id		just_to_avoid_the_many_warnings_abe_router_id;

extern const abe_int32 abe_dmic_40[C_98_48_LP_Coefs_sizeof];
extern const abe_int32 abe_dmic_32[C_98_48_LP_Coefs_sizeof];
extern const abe_int32 abe_dmic_25[C_98_48_LP_Coefs_sizeof];
extern const abe_int32 abe_dmic_16[C_98_48_LP_Coefs_sizeof];

extern const abe_uint32 abe_db2lin_table [];

#ifdef __cplusplus
}
#endif

#endif	/* _ABE_REF_H_ */
