/* =============================================================================

    THIS FILE IS PART OF THE ABE/HAL RELEASE FOR TEST PURPOSE
    THIS FILE MUST NOT BE INSERTED IN THE MAKEFILE OF THE FINAL APPLICATION

* =========================================================================== */
/**
 * @file  ABE_API.C
 *
 * All the visible API for the audio drivers
 *
 * @path
 * @rev     01.00
 */
/* ----------------------------------------------------------------------------
*!
*! Revision History
*! ===================================
*! 27-Nov-2008 Original
*! 05-Jun-2009 V05 release
* =========================================================================== */



#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

#include "ABE_MAIN.h"
#include "ABE_DEF.h"

void abe_test_scenario_1 (void);
void abe_test_scenario_2 (void);
void abe_test_scenario_3 (void);
void abe_test_scenario_4 (void);
void abe_test_scenario_5 (void);
void abe_test_scenario_6 (void);
void abe_test_scenario_7 (void);


/* ========================================================================== */
/**
* @fn ABE_TEST_SCENARIO()
*/
/* ========================================================================= */

void abe_test_scenario (abe_int32 scenario_id)
{
    switch (scenario_id)
    {
    case 1 :    abe_test_scenario_1 (); break;
    case 2 :    abe_test_scenario_2 (); break;
    case 3 :    abe_test_scenario_3 (); break;
    case 4 :    abe_test_scenario_4 (); break;
    case 5 :    abe_test_scenario_5 (); break;	// ASRC
    case 6 :    abe_test_scenario_6 (); break;	// APS
    case 7 :    abe_test_scenario_7 (); break;  // RAMP tests
    }
}


/* ========================================================================== */
/**
* @fn abe_test_read_time ()
*/
/* ========================================================================= */
abe_uint32 abe_test_read_time (void)
{
        abe_uint32 time;
        abe_block_copy (COPY_FROM_ABE_TO_HOST, ABE_DMEM, D_slotCounter_ADDR, (abe_uint32*)&time, sizeof (time));
        return (time & 0xFFFF);
}

/* ========================================================================== */
/**
* @fn abe_short_ramps ()
*/
/* ========================================================================= */
void abe_short_ramps (void)
{
	   /* mixers' configuration */
        abe_write_mixer (MIXDL1,   MUTE_GAIN,  RAMP_1MS,  MIX_DL1_INPUT_MM_DL);
        abe_write_mixer (MIXDL1,   MUTE_GAIN,  RAMP_1MS,  MIX_DL1_INPUT_MM_UL2);
        abe_write_mixer (MIXDL1,   MUTE_GAIN,  RAMP_1MS,  MIX_DL1_INPUT_VX_DL);
        abe_write_mixer (MIXDL1,   MUTE_GAIN,  RAMP_1MS,  MIX_DL1_INPUT_TONES);

        abe_write_mixer (MIXDL2,   MUTE_GAIN,  RAMP_1MS,  MIX_DL2_INPUT_TONES);
        abe_write_mixer (MIXDL2,   MUTE_GAIN,  RAMP_1MS,  MIX_DL2_INPUT_VX_DL);
        abe_write_mixer (MIXDL2,   MUTE_GAIN,  RAMP_1MS,  MIX_DL2_INPUT_MM_DL);
        abe_write_mixer (MIXDL2,   MUTE_GAIN,  RAMP_1MS,  MIX_DL2_INPUT_MM_UL2);

        abe_write_mixer (MIXSDT,   MUTE_GAIN,  RAMP_1MS,  MIX_SDT_INPUT_UP_MIXER);
        abe_write_mixer (MIXSDT,   GAIN_0dB ,  RAMP_1MS,  MIX_SDT_INPUT_DL1_MIXER);

        abe_write_mixer (MIXECHO,  GAIN_0dB ,  RAMP_1MS,  GAIN_LEFT_OFFSET);
        abe_write_mixer (MIXECHO,  GAIN_0dB ,  RAMP_1MS,  GAIN_RIGHT_OFFSET);

        abe_write_mixer (MIXAUDUL, MUTE_GAIN,  RAMP_1MS,  MIX_AUDUL_INPUT_MM_DL);
        abe_write_mixer (MIXAUDUL, MUTE_GAIN,  RAMP_1MS,  MIX_AUDUL_INPUT_TONES);
        abe_write_mixer (MIXAUDUL, GAIN_0dB ,  RAMP_1MS,  MIX_AUDUL_INPUT_UPLINK);
        abe_write_mixer (MIXAUDUL, MUTE_GAIN,  RAMP_1MS,  MIX_AUDUL_INPUT_VX_DL);

        abe_write_mixer (MIXVXREC, MUTE_GAIN,  RAMP_1MS,  MIX_VXREC_INPUT_TONES);
        abe_write_mixer (MIXVXREC, MUTE_GAIN,  RAMP_1MS,  MIX_VXREC_INPUT_VX_DL);
        abe_write_mixer (MIXVXREC, MUTE_GAIN,  RAMP_1MS,  MIX_VXREC_INPUT_MM_DL);
        abe_write_mixer (MIXVXREC, MUTE_GAIN,  RAMP_1MS,  MIX_VXREC_INPUT_VX_UL);

        abe_write_gain(GAINS_DMIC1,GAIN_0dB ,  RAMP_1MS,  GAIN_LEFT_OFFSET);
        abe_write_gain(GAINS_DMIC1,GAIN_0dB ,  RAMP_1MS,  GAIN_RIGHT_OFFSET);
        abe_write_gain(GAINS_DMIC2,GAIN_0dB ,  RAMP_1MS,  GAIN_LEFT_OFFSET);
        abe_write_gain(GAINS_DMIC2,GAIN_0dB ,  RAMP_1MS,  GAIN_RIGHT_OFFSET);
        abe_write_gain(GAINS_DMIC3,GAIN_0dB ,  RAMP_1MS,  GAIN_LEFT_OFFSET);
        abe_write_gain(GAINS_DMIC3,GAIN_0dB ,  RAMP_1MS,  GAIN_RIGHT_OFFSET);
        abe_write_gain(GAINS_AMIC ,GAIN_0dB ,  RAMP_1MS,  GAIN_LEFT_OFFSET);
        abe_write_gain(GAINS_AMIC, GAIN_0dB ,  RAMP_1MS,  GAIN_RIGHT_OFFSET);

        abe_write_gain(GAINS_SPLIT ,GAIN_0dB , RAMP_1MS,  GAIN_LEFT_OFFSET);
        abe_write_gain(GAINS_SPLIT, GAIN_0dB , RAMP_1MS,  GAIN_RIGHT_OFFSET);

        abe_write_gain (GAINS_DL1,GAIN_0dB ,   RAMP_1MS,  GAIN_LEFT_OFFSET);
        abe_write_gain (GAINS_DL1,GAIN_0dB ,   RAMP_1MS,  GAIN_RIGHT_OFFSET);
        abe_write_gain (GAINS_DL2,GAIN_0dB ,   RAMP_1MS,  GAIN_LEFT_OFFSET);
        abe_write_gain (GAINS_DL2,GAIN_0dB ,   RAMP_1MS,  GAIN_RIGHT_OFFSET);
}

/* ========================================================================== */
/**
* @fn ABE_TEST_SCENARIO_1 ()
*
*   DMA AUDIO PLAYER + DMA VOICE CALL 16kHz
*/
/* ========================================================================= */

void abe_test_scenario_1 (void)
{
        static abe_int32 time_offset, state;
        abe_data_format_t format;
        abe_dma_t dma_sink;
        abe_uint32 current_time;
        abe_use_case_id UC2[] = {ABE_AUDIO_PLAYER_ON_HEADSET_OR_EARPHONE, ABE_RINGER_TONES, (abe_use_case_id)0};
        // abe_use_case_id UC5[] = {ABE_AUDIO_PLAYER_ON_HEADSET_OR_EARPHONE, (abe_use_case_id)0};
        abe_opp_t OPP;
        abe_hw_config_init_t CONFIG;

        //-----------------------------Scenario 1-
        switch (state)
        {
        case 0:
                {   state ++;

                    time_offset = abe_test_read_time();

                    abe_reset_hal ();
		    abe_load_fw ();
		    abe_short_ramps ();
		    abe_set_debug_trace (-1);
                    abe_read_hardware_configuration (UC2, &OPP,  &CONFIG);          /* check HW config and OPP config */

		    #if 0
		    {   abe_equ_t dl2_eq;
			const abe_int32 DL2_COEF [25] = { -7554223, 708210, -708206, 7554225,    0,0,0,0,   0,0,0,0,   0, 0,0,0,0,
							   0,0,0,0,       0,6802833, -682266, 731554};
			dl2_eq.equ_length = 25;
			memcpy (dl2_eq.coef.type1, DL2_COEF, sizeof(DL2_COEF));

			abe_write_equalizer (EQ2L, &dl2_eq);
			abe_write_equalizer (EQ2R, &dl2_eq);
		    }
		    #endif

		    abe_set_opp_processing (OPP);
                    abe_write_event_generator (CONFIG.HAL_EVENT_SELECTION);         /* "tick" of the audio engine */

                    abe_set_router_configuration (UPROUTE, UPROUTE_CONFIG_AMIC,  (abe_router_t *) abe_router_ul_table_preset[UPROUTE_CONFIG_AMIC]);
                    //abe_set_router_configuration (UPROUTE, UPROUTE_CONFIG_BT,  (abe_router_t *) abe_router_ul_table_preset[UPROUTE_CONFIG_BT]);

                  //format.f =  8000; format.samp_format = STEREO_RSHIFTED_16; abe_connect_cbpr_dmareq_port (VX_UL_PORT,    &format, ABE_CBPR1_IDX, &dma_sink);
                    format.f =  8000; format.samp_format = MONO_RSHIFTED_16; abe_connect_serial_port (VX_UL_PORT, &format, MCBSP2_TX);
                  //format.f =  8000; format.samp_format = STEREO_RSHIFTED_16; abe_connect_cbpr_dmareq_port (VX_DL_PORT,    &format, ABE_CBPR1_IDX, &dma_sink);
                    format.f =  8000; format.samp_format = MONO_RSHIFTED_16; abe_connect_serial_port (VX_DL_PORT, &format, MCBSP2_RX);

                    format.f = 48000; format.samp_format = MONO_MSB;   abe_connect_cbpr_dmareq_port (TONES_DL_PORT, &format, ABE_CBPR5_IDX, &dma_sink);
                    format.f = 48000; format.samp_format = SIX_MSB;    abe_connect_cbpr_dmareq_port (MM_UL_PORT,    &format, ABE_CBPR3_IDX, &dma_sink);
                    format.f = 48000; format.samp_format = STEREO_MSB; abe_connect_cbpr_dmareq_port (MM_UL2_PORT,   &format, ABE_CBPR4_IDX, &dma_sink);
		    format.f = 24000; format.samp_format = STEREO_MSB; abe_connect_cbpr_dmareq_port (VIB_DL_PORT,   &format, ABE_CBPR6_IDX, &dma_sink);

                    format.f = 48000; format.samp_format = STEREO_MSB; abe_connect_cbpr_dmareq_port (MM_DL_PORT,    &format, ABE_CBPR0_IDX, &dma_sink);
		  //format.f = 48000; format.samp_format = STEREO_MSB; abe_connect_serial_port (MM_DL_PORT, &format, MCBSP3_RX);

                    // SERIAL PORTS TEST
		    // abe_select_main_port (MM_DL_PORT);

                    format.f = 16000; format.samp_format = STEREO_RSHIFTED_16; abe_connect_serial_port (BT_VX_UL_PORT, &format, MCBSP1_RX);
                    format.f = 16000; format.samp_format = STEREO_RSHIFTED_16; abe_connect_serial_port (BT_VX_DL_PORT, &format, MCBSP1_TX);
                    //format.f = 48000; format.samp_format = STEREO_RSHIFTED_16; abe_connect_serial_port (MM_EXT_IN_PORT, &format, MCBSP3_RX);
                    //format.f = 48000; format.samp_format = STEREO_RSHIFTED_16; abe_connect_serial_port (MM_EXT_OUT_PORT, &format, MCBSP3_TX);
                    abe_enable_data_transfer (BT_VX_UL_PORT);
                    abe_enable_data_transfer (BT_VX_DL_PORT);
                    //abe_enable_data_transfer (MM_EXT_IN_PORT);
                    //abe_enable_data_transfer (MM_EXT_OUT_PORT);

		    abe_enable_data_transfer (MM_UL_PORT );                            /* enable all DMIC aquisition */
                    abe_enable_data_transfer (MM_UL2_PORT );                          /* enable large-band DMIC aquisition */
                    abe_enable_data_transfer (VX_UL_PORT );
                    abe_enable_data_transfer (TONES_DL_PORT);
                    abe_enable_data_transfer (VX_DL_PORT );
                    abe_enable_data_transfer (MM_DL_PORT );                         /* enable all the data paths */
                    abe_enable_data_transfer (VIB_DL_PORT );
                    abe_enable_data_transfer (DMIC_PORT);                           /* DMIC ATC can be enabled even if the DMIC */
                    abe_enable_data_transfer (PDM_DL_PORT);
                    abe_enable_data_transfer (PDM_UL_PORT);

                    /* mixers' configuration = voice on earphone + music on hands-free path */
                    abe_write_mixer (MIXDL1,    GAIN_M12dB,   RAMP_0MS,  MIX_DL1_INPUT_MM_DL);
                    abe_write_mixer (MIXDL1,    MUTE_GAIN,   RAMP_0MS,  MIX_DL1_INPUT_MM_UL2);
                    abe_write_mixer (MIXDL1,    GAIN_M12dB,   RAMP_0MS,  MIX_DL1_INPUT_VX_DL);
                    abe_write_mixer (MIXDL1,    GAIN_M12dB,   RAMP_0MS,  MIX_DL1_INPUT_TONES);

                    abe_write_mixer (MIXDL2,    GAIN_M12dB,  RAMP_0MS,  MIX_DL2_INPUT_TONES);
                    abe_write_mixer (MIXDL2,    MUTE_GAIN,  RAMP_0MS,   MIX_DL2_INPUT_VX_DL);
                    abe_write_mixer (MIXDL2,    GAIN_M12dB,  RAMP_0MS,  MIX_DL2_INPUT_MM_DL);
                    abe_write_mixer (MIXDL2,    MUTE_GAIN,  RAMP_0MS,   MIX_DL2_INPUT_MM_UL2);

		    //abe_enable_test_pattern (DBG_PATCH_MM_DL_MIXDL1, 1);
		    //abe_enable_test_pattern (DBG_PATCH_MM_DL_MIXDL2, 1);

                }
                break;
        case 1:
                current_time = abe_test_read_time();
                if ((current_time - time_offset) < 1000) break;
                else
                {   state ++;



                }
                break;
        case 2:
                current_time = abe_test_read_time();
                if ((current_time - time_offset) < 1050) break;
                else
                {   state ++;


                }
                break;

        case 3:
                current_time = abe_test_read_time();
                if ((current_time - time_offset) < 200000) break;
                else
                {   state ++;
		}

        default: state = 0; break;
        }
}


/* ========================================================================== */
/**
* @fn ABE_TEST_SCENARIO_2 ()
*
*   DMA AUDIO PLAYER + DMA VOICE CALL 8kHz
*/
/* ========================================================================= */
void abe_test_scenario_2 (void)
{
       static abe_int32 time_offset, state;
        abe_data_format_t format;
        abe_dma_t dma_sink;
        abe_uint32 current_time;
        abe_use_case_id UC2[] = {ABE_AUDIO_PLAYER_ON_HEADSET_OR_EARPHONE, ABE_RINGER_TONES, (abe_use_case_id)0};
        // abe_use_case_id UC5[] = {ABE_AUDIO_PLAYER_ON_HEADSET_OR_EARPHONE, (abe_use_case_id)0};
        abe_opp_t OPP;
        abe_hw_config_init_t CONFIG;

        //-----------------------------Scenario 1- 16kHz first
        switch (state)
        {
        case 0:
                {   state ++;

                    time_offset = abe_test_read_time();
                    abe_reset_hal ();
		    abe_load_fw ();
		    abe_set_debug_trace (-1);
		    abe_short_ramps ();

                    abe_read_hardware_configuration (UC2, &OPP,  &CONFIG);          /* check HW config and OPP config */
                    abe_set_opp_processing (OPP);                                   /* sets the OPP100 on FW05.xx */
                    abe_write_event_generator (CONFIG.HAL_EVENT_SELECTION);         /* "tick" of the audio engine */

                    abe_set_router_configuration (UPROUTE, UPROUTE_CONFIG_AMIC,  (abe_router_t *) abe_router_ul_table_preset[UPROUTE_CONFIG_AMIC]);

                    format.f = 48000; format.samp_format = STEREO_MSB; abe_connect_cbpr_dmareq_port (MM_UL_PORT,    &format, ABE_CBPR3_IDX, &dma_sink);
                    format.f = 48000; format.samp_format = STEREO_MSB; abe_connect_cbpr_dmareq_port (MM_UL2_PORT,   &format, ABE_CBPR4_IDX, &dma_sink);
                    format.f =  8000; format.samp_format = MONO_MSB;   abe_connect_cbpr_dmareq_port (VX_UL_PORT,    &format, ABE_CBPR2_IDX, &dma_sink);
                    abe_enable_data_transfer (MM_UL_PORT );                            /* enable all DMIC aquisition */
                    abe_enable_data_transfer (MM_UL2_PORT );                          /* enable large-band DMIC aquisition */
                    abe_enable_data_transfer (VX_UL_PORT );

                    format.f = 48000; format.samp_format = STEREO_MSB; abe_connect_cbpr_dmareq_port (TONES_DL_PORT, &format, ABE_CBPR5_IDX, &dma_sink);
                    format.f =  8000; format.samp_format = MONO_MSB;   abe_connect_cbpr_dmareq_port (VX_DL_PORT,    &format, ABE_CBPR1_IDX, &dma_sink);
                    format.f = 48000; format.samp_format = STEREO_MSB; abe_connect_cbpr_dmareq_port (MM_DL_PORT,    &format, ABE_CBPR0_IDX, &dma_sink);
                    format.f = 24000; format.samp_format = STEREO_MSB; abe_connect_cbpr_dmareq_port (VIB_DL_PORT,   &format, ABE_CBPR6_IDX, &dma_sink);
                    abe_enable_data_transfer (TONES_DL_PORT);
                    abe_enable_data_transfer (VX_DL_PORT );
                    abe_enable_data_transfer (MM_DL_PORT );                         /* enable all the data paths */
                    abe_enable_data_transfer (VIB_DL_PORT );

                    abe_enable_data_transfer (DMIC_PORT);                           /* DMIC ATC can be enabled even if the DMIC */
                    abe_enable_data_transfer (PDM_DL_PORT);
                    abe_enable_data_transfer (PDM_UL_PORT);


                    /* mixers' configuration = voice on earphone + music on hands-free path */
                    abe_write_mixer (MIXDL1,    MUTE_GAIN,  RAMP_0MS,  MIX_DL1_INPUT_TONES);
                    abe_write_mixer (MIXDL1,    GAIN_M6dB , RAMP_0MS,  MIX_DL1_INPUT_VX_DL);
                    abe_write_mixer (MIXDL1,    GAIN_M6dB,  RAMP_0MS,   MIX_DL1_INPUT_MM_DL);
                    abe_write_mixer (MIXDL1,    MUTE_GAIN, RAMP_0MS,   MIX_DL1_INPUT_MM_UL2);

                    abe_write_mixer (MIXDL2,    MUTE_GAIN, RAMP_0MS,   MIX_DL2_INPUT_TONES);
                    abe_write_mixer (MIXDL2,    GAIN_M6dB, RAMP_0MS,   MIX_DL2_INPUT_VX_DL);
                    abe_write_mixer (MIXDL2,    GAIN_M6dB,  RAMP_0MS,   MIX_DL2_INPUT_MM_DL);
                    abe_write_mixer (MIXDL2,    MUTE_GAIN, RAMP_0MS,   MIX_DL2_INPUT_MM_UL2);

                  }
                break;
        case 1:
                current_time = abe_test_read_time();
                if ((current_time - time_offset) < 100) break;
                else
                {   state ++;
                    // Gains switch
                }
                break;
        case 2:
                current_time = abe_test_read_time();
                if ((current_time - time_offset) < 100000) break;
                else
                {   state ++;
                    // Internal buffer analysis
                }
                break;
        default: state = 0; break;
        }
}
/* ========================================================================== */
/**
*   @fn ABE_TEST_SCENARIO_3 ()
*
*   IRQ AUDIO PLAYER 44100Hz  OPP 25%
*/
/* ========================================================================= */
void abe_test_scenario_3 (void)
{
       static abe_int32 time_offset, state, gain=0x040000, i;
        abe_data_format_t format;
        abe_uint32 data_sink;
        abe_use_case_id UC2[] = {ABE_AUDIO_PLAYER_ON_HEADSET_OR_EARPHONE, (abe_use_case_id)0};
        // abe_use_case_id UC5[] = {ABE_AUDIO_PLAYER_ON_HEADSET_OR_EARPHONE, (abe_use_case_id)0};
        abe_opp_t OPP;
        abe_hw_config_init_t CONFIG;

        //-----------------------------Scenario 1- 16kHz first
        switch (state)
        {
        case 0:
                {   state ++;

                    time_offset = abe_test_read_time();
                    abe_reset_hal ();
		    abe_load_fw ();
		    abe_short_ramps ();

                    abe_read_hardware_configuration (UC2, &OPP,  &CONFIG);          /* check HW config and OPP config */
                    abe_set_opp_processing (OPP);
                    //abe_set_opp_processing (ABE_OPP50);
                    abe_write_event_generator(EVENT_44100);			    /* "tick" of the audio engine */

		    /* connect a Ping-Pong cache-flush protocol to MM_DL port */
		    #define N_SAMPLES_BYTES (25 *8)				/* half-buffer size in bytes, 32/32 data format */
		    format.f = 44100; format.samp_format = STEREO_MSB;
		    abe_add_subroutine (&abe_irq_pingpong_player_id, (abe_subroutine2) abe_default_irq_pingpong_player_32bits, SUB_0_PARAM, (abe_uint32*)0);

//		    #define N_SAMPLES_BYTES (25 *4)				/* half-buffer size in bytes, 16|16 data format */
//		    format.f = 48000; format.samp_format = STEREO_16_16;
//		    abe_add_subroutine (&abe_irq_pingpong_player_id, (abe_subroutine2) abe_default_irq_pingpong_player, SUB_0_PARAM, (abe_uint32*)0);

		    abe_connect_irq_ping_pong_port (MM_DL_PORT, &format, abe_irq_pingpong_player_id, N_SAMPLES_BYTES, &data_sink, PING_PONG_WITH_MCU_IRQ);

                    abe_enable_data_transfer (MM_DL_PORT );                         /* enable all the data paths */
                    abe_enable_data_transfer (PDM_DL_PORT);

                    format.f = 48000; format.samp_format = STEREO_MSB; abe_connect_serial_port (MM_EXT_OUT_PORT, &format, MCBSP2_TX);
                    abe_enable_data_transfer (MM_EXT_OUT_PORT);

                    /* mixers' configuration */
                    abe_write_mixer (MIXDL1,    GAIN_0dB,  RAMP_0MS,   MIX_DL1_INPUT_TONES);
                    abe_write_mixer (MIXDL1,    GAIN_0dB,  RAMP_0MS,   MIX_DL1_INPUT_MM_DL);

                }
                break;
        }
}

/* ========================================================================== */
/**
* @fn ABE_TEST_SCENARIO_4 ()
*
*   DMA AUDIO PLAYER + DMA VOICE CALL 8kHz OPP 25%
*/
/* ========================================================================= */
void abe_test_scenario_4 (void)
{
		    abe_set_debug_trace (-1);
		    abe_short_ramps ();

}

/* ========================================================================== */
/**
* @fn ABE_TEST_SCENARIO_5 ()
*
*   ASRC TESTS
*/
/* ========================================================================= */
void abe_test_scenario_5 (void)
{
       static abe_int32 time_offset, state;
        abe_data_format_t format;
        abe_dma_t dma_sink;
        abe_uint32 current_time;
        abe_use_case_id UC2[] = {ABE_AUDIO_PLAYER_ON_HEADSET_OR_EARPHONE, ABE_RINGER_TONES, (abe_use_case_id)0};
        // abe_use_case_id UC5[] = {ABE_AUDIO_PLAYER_ON_HEADSET_OR_EARPHONE, (abe_use_case_id)0};
        abe_opp_t OPP;
        abe_hw_config_init_t CONFIG;

        //-----------------------------Scenario 1- 16kHz first
        switch (state)
        {
        case 0:
                {   state ++;
                    abe_reset_hal ();
		    abe_load_fw ();
		    abe_set_debug_trace (-1);
		    abe_short_ramps ();

                    time_offset = abe_test_read_time();

                    abe_read_hardware_configuration (UC2, &OPP,  &CONFIG);          /* check HW config and OPP config */
                    abe_set_opp_processing (OPP);                                   /* sets the OPP100 on FW05.xx */
                    abe_write_event_generator (CONFIG.HAL_EVENT_SELECTION);         /* "tick" of the audio engine */

                    abe_set_router_configuration (UPROUTE, UPROUTE_CONFIG_AMIC,  (abe_router_t *) abe_router_ul_table_preset[UPROUTE_CONFIG_AMIC]);

                    format.f =  8000; format.samp_format = STEREO_RSHIFTED_16; abe_connect_serial_port (VX_UL_PORT, &format, MCBSP2_TX);
                    format.f =  8000; format.samp_format = STEREO_RSHIFTED_16; abe_connect_serial_port (VX_DL_PORT, &format, MCBSP2_RX);
                    format.f = 48000; format.samp_format = STEREO_MSB; abe_connect_serial_port (MM_DL_PORT, &format, MCBSP3_RX);
		    
                    abe_enable_data_transfer (VX_UL_PORT );
                    abe_enable_data_transfer (VX_DL_PORT );
                    abe_enable_data_transfer (MM_DL_PORT );                         /* enable all the data paths */

                    // SERIAL PORTS TEST
		    // abe_select_main_port (MM_DL_PORT);

                    abe_enable_data_transfer (PDM_DL_PORT);
                    abe_enable_data_transfer (PDM_UL_PORT);

                    /* mixers' configuration = voice on earphone + music on hands-free path */
                    abe_write_mixer (MIXDL1,    MUTE_GAIN,   RAMP_0MS,  MIX_DL1_INPUT_MM_DL);
                    abe_write_mixer (MIXDL1,    MUTE_GAIN, RAMP_0MS,   MIX_DL1_INPUT_MM_UL2);
                    abe_write_mixer (MIXDL1,    GAIN_M6dB,   RAMP_0MS,  MIX_DL1_INPUT_VX_DL);
                    abe_write_mixer (MIXDL1,    MUTE_GAIN,   RAMP_0MS,  MIX_DL1_INPUT_TONES);

                    abe_write_mixer (MIXDL2,    MUTE_GAIN, RAMP_0MS,   MIX_DL2_INPUT_TONES);
                    abe_write_mixer (MIXDL2,    MUTE_GAIN,  RAMP_0MS,   MIX_DL2_INPUT_VX_DL);
                    abe_write_mixer (MIXDL2,    GAIN_M6dB,  RAMP_0MS,   MIX_DL2_INPUT_MM_DL);
                    abe_write_mixer (MIXDL2,    MUTE_GAIN, RAMP_0MS,   MIX_DL2_INPUT_MM_UL2);
                }
                break;
        case 1:
                current_time = abe_test_read_time();
                if ((current_time - time_offset) < 34) break;
                else
                {
		    state ++;
		  // Init ASRC(s)
		    abe_init_asrc_vx_dl (0);
		    abe_init_asrc_vx_ul (0);
		    abe_init_asrc_mm_dl (0);
		 //   abe_init_asrc_mm_dl (1250);
		}
                break;
        case 2:
                current_time = abe_test_read_time();
                if ((current_time - time_offset) < 10000) break;
                else
                {
		    state ++;
	//	    abe_write_asrc             (VX_UL_PORT, -1250);
	//	    abe_write_asrc             (VX_DL_PORT, -1250);
	//	    abe_write_asrc             (MM_DL_PORT, -1250);
		}
                break;
        case 3:
                current_time = abe_test_read_time();
                if ((current_time - time_offset) < 100000) break;
                else
                {   state ++;
	//	    abe_set_asrc_drift_control (VX_UL_PORT, FORCED_DRIFT_CONTROL);
		    abe_write_asrc             (VX_UL_PORT, -100);
	//	    abe_set_asrc_drift_control (VX_DL_PORT, FORCED_DRIFT_CONTROL);
		    abe_write_asrc             (VX_DL_PORT, -200);
	//	    abe_set_asrc_drift_control (MM_DL_PORT, FORCED_DRIFT_CONTROL);
		    abe_write_asrc             (MM_DL_PORT, -300);

                }
                break;
        default: state = 0; break;
        }
}


/* ========================================================================== */
/**
* @fn ABE_TEST_SCENARIO_6 ()
*
*   APS TESTS
*/
/* ========================================================================= */
void abe_test_scenario_6 (void)
{
       static abe_int32 time_offset, state;
        abe_data_format_t format;
        abe_uint32 data_sink;
        abe_uint32 current_time;
        abe_use_case_id UC2[] = {ABE_AUDIO_PLAYER_ON_HEADSET_OR_EARPHONE, ABE_RINGER_TONES, (abe_use_case_id)0};
        // abe_use_case_id UC5[] = {ABE_AUDIO_PLAYER_ON_HEADSET_OR_EARPHONE, (abe_use_case_id)0};
        abe_opp_t OPP;
        abe_hw_config_init_t CONFIG;

        //-----------------------------Scenario 1- 16kHz first
        switch (state)
        {
        case 0:

                {   state ++;
                    abe_reset_hal ();
		    abe_load_fw ();
		    abe_set_debug_trace (-1);
		    abe_short_ramps ();

                    time_offset = abe_test_read_time();

		    abe_read_hardware_configuration (UC2, &OPP,  &CONFIG);           /* check HW config and OPP config */
		    abe_set_opp_processing (OPP);                                   /* sets the OPP100 on FW05.xx */
		    abe_write_event_generator (CONFIG.HAL_EVENT_SELECTION);         /* "tick" of the audio engine */

		    /* connect a Ping-Pong cache-flush protocol to MM_DL port */
		    #define N_SAMPLES_BYTES (25 *8)				/* half-buffer size in bytes, 32/32 data format */
		    format.f = 44100; format.samp_format = STEREO_MSB;
		    abe_add_subroutine (&abe_irq_pingpong_player_id, (abe_subroutine2) abe_default_irq_pingpong_player_32bits, SUB_0_PARAM, (abe_uint32*)0);

//		    #define N_SAMPLES_BYTES (25 *4)				/* half-buffer size in bytes, 16|16 data format */
//		    format.f = 48000; format.samp_format = STEREO_16_16;
//		    abe_add_subroutine (&abe_irq_pingpong_player_id, (abe_subroutine2) abe_default_irq_pingpong_player, SUB_0_PARAM, (abe_uint32*)0);

		    abe_connect_irq_ping_pong_port (MM_DL_PORT, &format, abe_irq_pingpong_player_id, N_SAMPLES_BYTES, &data_sink, PING_PONG_WITH_MCU_IRQ);

		    /* mixers' configuration = voice on earphone + music on hands-free path */
		    abe_write_mixer (MIXDL1, GAIN_0dB,      RAMP_2MS,   MIX_DL1_INPUT_MM_DL);
		    abe_write_mixer (MIXDL2, GAIN_0dB,      RAMP_50MS,  MIX_DL2_INPUT_MM_DL);

		    abe_enable_data_transfer (MM_DL_PORT );                         /* enable all the data paths */
		    abe_enable_data_transfer (PDM_DL_PORT);

		    /* connect a Ping-Pong cache-flush protocol to MM_DL port with 50Hz (20ms) rate */
		    abe_add_subroutine (&abe_irq_aps_adaptation_id, (abe_subroutine2) abe_default_irq_aps_adaptation, SUB_0_PARAM, (abe_uint32*)0);
                }
                break;

        case 1:
                current_time = abe_test_read_time();
                if ((current_time - time_offset) < 100000) break;
                else
                {   state ++;
                }
                break;
        default: state = 0; break;
        }
}

/* ========================================================================== */
/**
* @fn ABE_TEST_SCENARIO_7 ()
*
*   GAIN RAMP TESTS
*/
/* ========================================================================= */
void abe_test_scenario_7 (void)
{
       static abe_int32 time_offset, state, current_time;
        abe_data_format_t format;
        abe_dma_t dma_sink;
        abe_use_case_id UC2[] = {ABE_AUDIO_PLAYER_ON_HEADSET_OR_EARPHONE, (abe_use_case_id)0};
        abe_opp_t OPP;
        abe_hw_config_init_t CONFIG;

        switch (state)
        {
        case 0:
                {   state ++;
                    abe_reset_hal ();
		    abe_load_fw ();
		    abe_set_debug_trace (-1);

                    time_offset = abe_test_read_time();

		    abe_read_hardware_configuration (UC2, &OPP,  &CONFIG);           /* check HW config and OPP config */
		    abe_set_opp_processing (OPP);
		    abe_write_event_generator (CONFIG.HAL_EVENT_SELECTION);         /* "tick" of the audio engine */

                    format.f = 48000; format.samp_format = STEREO_MSB; abe_connect_cbpr_dmareq_port (MM_DL_PORT,    &format, ABE_CBPR0_IDX, &dma_sink);
                    abe_enable_data_transfer (MM_DL_PORT );                         /* enable all the data paths */
                    abe_enable_data_transfer (PDM_DL_PORT);

                    /* mixers' configuration = voice on earphone + music on hands-free path */
                    abe_write_mixer (MIXDL1,    GAIN_0dB,   RAMP_5MS,  MIX_DL1_INPUT_MM_DL);
                }
                break;
        case 1:
                current_time = abe_test_read_time();
                if ((current_time - time_offset) < 10000) break;
                else
                {
		    state ++;
                    abe_write_mixer (MIXDL1,    GAIN_MUTE,   RAMP_5MS,  MIX_DL1_INPUT_MM_DL);
		}
                break;
        case 2:
                current_time = abe_test_read_time();
                if ((current_time - time_offset) < 20000) break;
                else
                {
		    state ++;
                    abe_write_mixer (MIXDL1,    GAIN_0dB,    RAMP_10MS,  MIX_DL1_INPUT_MM_DL);
                }
                break;
        case 3:
                current_time = abe_test_read_time();
                if ((current_time - time_offset) < 30000) break;
                else  state ++;
                break;
        default: state = 0; break;
        }
}

