/*
 * ==========================================================================
 *				 Texas Instruments OMAP(TM) Platform Firmware
 * (c) Copyright 2009, Texas Instruments Incorporated.	All Rights Reserved.
 *
 *	Use of this firmware is controlled by the terms and conditions found
 *	in the license agreement under which this firmware has been supplied.
 * ==========================================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "ABE_MAIN.h"
#include "ABE_DEF.h"

void abe_test_scenario_1(void);
void abe_test_scenario_2(void);
void abe_test_scenario_3(void);
void abe_test_scenario_4(void);

/*
* @fn ABE_TEST_SCENARIO()
*/
void abe_test_scenario(abe_int32 scenario_id)
{
	switch (scenario_id) {
	case 1:
		abe_test_scenario_1();
		break;
	case 2:
		abe_test_scenario_2();
		break;
	case 3:
		abe_test_scenario_3();
		break;
	case 4:
		abe_test_scenario_4();
		break;
	}
}

/*
* @fn abe_test_read_time ()
*/
abe_uint32 abe_test_read_time(void)
{
	abe_uint32 time;
	abe_block_copy(COPY_FROM_ABE_TO_HOST, ABE_DMEM, D_slotCounter_ADDR,
						(abe_uint32*)&time, sizeof (time));
	return (time & 0xFFFF);
}

/*
* @fn ABE_TEST_SCENARIO_1 ()
*
*	DMA AUDIO PLAYER + DMA VOICE CALL 16kHz
*/
void abe_test_scenario_1(void)
{
	static abe_int32 time_offset, state;
	abe_data_format_t format;
	abe_dma_t dma_sink;
	abe_uint32 current_time;
	abe_use_case_id UC2[] = {
		ABE_AUDIO_PLAYER_ON_HEADSET_OR_EARPHONE,
		ABE_RINGER_TONES,
		(abe_use_case_id)0
	};
	// abe_use_case_id UC5[] = {ABE_AUDIO_PLAYER_ON_HEADSET_OR_EARPHONE, (abe_use_case_id)0};
	abe_opp_t OPP;
	abe_hw_config_init_t CONFIG;

	/* Scenario 1- 16kHz first */
	switch (state) {
	case 0:
		state ++;
		time_offset = abe_test_read_time();
		abe_reset_hal();
		abe_load_fw();

		/* check HW config and OPP config */
		abe_read_hardware_configuration(UC2, &OPP, &CONFIG);
		/* sets the OPP100 on FW05.xx */
		abe_set_opp_processing(OPP);
		/* "tick" of the audio engine */
		abe_write_event_generator(CONFIG.HAL_EVENT_SELECTION);

		abe_set_router_configuration(UPROUTE, UPROUTE_CONFIG_AMIC,
				(abe_router_t *) abe_router_ul_table_preset[UPROUTE_CONFIG_BT]);

		format.f = 48000;
		format.samp_format = SIX_MSB;
		abe_connect_cbpr_dmareq_port(MM_UL_PORT, &format, ABE_CBPR3_IDX, &dma_sink);
		format.f = 48000;
		format.samp_format = STEREO_MSB;
		abe_connect_cbpr_dmareq_port(MM_UL2_PORT, &format, ABE_CBPR4_IDX, &dma_sink);
		format.f = 8000;
		format.samp_format = MONO_MSB;
		abe_connect_cbpr_dmareq_port(VX_UL_PORT, &format, ABE_CBPR2_IDX, &dma_sink);
		/* enable all DMIC aquisition */
		abe_enable_data_transfer(MM_UL_PORT);
		/* enable large-band DMIC aquisition */
		abe_enable_data_transfer(MM_UL2_PORT);
		abe_enable_data_transfer(VX_UL_PORT);

		format.f = 48000;
		format.samp_format = STEREO_MSB;
		abe_connect_cbpr_dmareq_port(TONES_DL_PORT, &format, ABE_CBPR5_IDX, &dma_sink);
		format.f = 8000;
		format.samp_format = MONO_MSB;
		abe_connect_cbpr_dmareq_port(VX_DL_PORT, &format, ABE_CBPR1_IDX, &dma_sink);
		format.f = 48000;
		format.samp_format = STEREO_MSB;
		abe_connect_cbpr_dmareq_port(MM_DL_PORT, &format, ABE_CBPR0_IDX, &dma_sink);
		format.f = 24000;
		format.samp_format = STEREO_MSB;
		abe_connect_cbpr_dmareq_port(VIB_DL_PORT, &format, ABE_CBPR6_IDX, &dma_sink);
		abe_enable_data_transfer(TONES_DL_PORT);
		abe_enable_data_transfer(VX_DL_PORT);
		/* enable all the data paths */
		abe_enable_data_transfer(MM_DL_PORT);
		abe_enable_data_transfer(VIB_DL_PORT);

		/* SERIAL PORTS TEST */
		format.f = 8000;
		format.samp_format = MONO_MSB;
		abe_connect_serial_port(BT_VX_UL_PORT, &format, MCBSP1_RX);
		format.f = 8000;
		format.samp_format = MONO_RSHIFTED_16;
		abe_connect_serial_port(BT_VX_DL_PORT, &format, MCBSP1_TX);
		format.f = 48000;
		format.samp_format = STEREO_MSB;
		abe_connect_serial_port(MM_EXT_IN_PORT, &format, MCBSP2_RX);
		format.f = 48000;
		format.samp_format = MONO_RSHIFTED_16;
		abe_connect_serial_port(MM_EXT_OUT_PORT, &format, MCBSP2_TX);
		abe_enable_data_transfer(BT_VX_UL_PORT);
		abe_enable_data_transfer(BT_VX_DL_PORT);
		abe_enable_data_transfer(MM_EXT_IN_PORT);
		abe_enable_data_transfer(MM_EXT_OUT_PORT);

		/* DMIC ATC can be enabled even if the DMIC */
		abe_enable_data_transfer(DMIC_PORT);
		abe_enable_data_transfer(PDM_DL_PORT);
		abe_enable_data_transfer(PDM_UL_PORT);

		/* mixers' configuration = voice on earphone + music on hands-free path */
		abe_write_mixer(MIXDL1, GAIN_0dB, RAMP_0MS, MIX_DL1_INPUT_MM_DL);
		abe_write_mixer(MIXDL1, MUTE_GAIN, RAMP_0MS, MIX_DL1_INPUT_MM_UL2);
		abe_write_mixer(MIXDL1, GAIN_0dB, RAMP_0MS, MIX_DL1_INPUT_VX_DL);
		abe_write_mixer(MIXDL1, MUTE_GAIN, RAMP_0MS, MIX_DL1_INPUT_TONES);

		abe_write_mixer(MIXDL2, GAIN_0dB, RAMP_0MS, MIX_DL2_INPUT_TONES);
		abe_write_mixer(MIXDL2, MUTE_GAIN, RAMP_0MS, MIX_DL2_INPUT_VX_DL);
		abe_write_mixer(MIXDL2, GAIN_0dB, RAMP_0MS, MIX_DL2_INPUT_MM_DL);
		abe_write_mixer(MIXDL2, MUTE_GAIN, RAMP_0MS, MIX_DL2_INPUT_MM_UL2);

		abe_write_mixer(MIXSDT, MUTE_GAIN, RAMP_0MS, MIX_SDT_INPUT_UP_MIXER);
		abe_write_mixer(MIXSDT, GAIN_0dB, RAMP_0MS, MIX_SDT_INPUT_DL1_MIXER);

		abe_write_mixer(MIXECHO, GAIN_0dB, RAMP_0MS, GAIN_LEFT_OFFSET);
		abe_write_mixer(MIXECHO, GAIN_0dB, RAMP_0MS, GAIN_RIGHT_OFFSET);

		abe_write_mixer(MIXAUDUL, MUTE_GAIN, RAMP_0MS, MIX_AUDUL_INPUT_MM_DL);
		abe_write_mixer(MIXAUDUL, MUTE_GAIN, RAMP_0MS, MIX_AUDUL_INPUT_TONES);
		abe_write_mixer(MIXAUDUL, GAIN_0dB, RAMP_0MS, MIX_AUDUL_INPUT_UPLINK);
		abe_write_mixer(MIXAUDUL, MUTE_GAIN, RAMP_0MS, MIX_AUDUL_INPUT_VX_DL);

		abe_write_mixer(MIXVXREC, MUTE_GAIN, RAMP_0MS, MIX_VXREC_INPUT_TONES);
		abe_write_mixer(MIXVXREC, GAIN_M6dB, RAMP_0MS, MIX_VXREC_INPUT_VX_DL);
		abe_write_mixer(MIXVXREC, MUTE_GAIN, RAMP_0MS, MIX_VXREC_INPUT_MM_DL);
		abe_write_mixer(MIXVXREC, GAIN_M6dB, RAMP_0MS, MIX_VXREC_INPUT_VX_UL);
		break;
	case 1:
		current_time = abe_test_read_time();
		if ((current_time - time_offset) < 100)
			break;
		else
			state ++;
		break;
	case 2:
		current_time = abe_test_read_time();
		if ((current_time - time_offset) < 100000)
			break;
		else
			state ++; // Internal buffer analysis
		break;
	default:
		state = 0;
		break;
	}
}

/*
* @fn ABE_TEST_SCENARIO_2 ()
*
*	DMA AUDIO PLAYER + DMA VOICE CALL 8kHz
*/
void abe_test_scenario_2 (void)
{
	static abe_int32 time_offset, state;
	abe_data_format_t format;
	abe_dma_t dma_sink;
	abe_uint32 current_time;
	abe_use_case_id UC2[] = {ABE_AUDIO_PLAYER_ON_HEADSET_OR_EARPHONE, ABE_RINGER_TONES, (abe_use_case_id)0};
	abe_opp_t OPP;
	abe_hw_config_init_t CONFIG;

	/* Scenario 1- 16kHz first */
	switch (state)
	{
	case 0:
		state ++;

		time_offset = abe_test_read_time();
		abe_reset_hal();
		abe_load_fw();

		/* check HW config and OPP config */
		abe_read_hardware_configuration(UC2, &OPP,  &CONFIG);
		abe_set_opp_processing(OPP); /* sets the OPP100 on FW05.xx */
		/* "tick" of the audio engine */
		abe_write_event_generator(CONFIG.HAL_EVENT_SELECTION);

		abe_set_router_configuration(UPROUTE,
			UPROUTE_CONFIG_AMIC,
			(abe_router_t *) abe_router_ul_table_preset[UPROUTE_CONFIG_AMIC]);

		format.f = 48000;
		format.samp_format = STEREO_MSB;
		abe_connect_cbpr_dmareq_port(MM_UL_PORT, &format, ABE_CBPR3_IDX, &dma_sink);

		format.f = 48000;
		format.samp_format = STEREO_MSB;
		abe_connect_cbpr_dmareq_port(MM_UL2_PORT, &format, ABE_CBPR4_IDX, &dma_sink);
		format.f = 8000;
		format.samp_format = MONO_MSB;
		abe_connect_cbpr_dmareq_port(VX_UL_PORT, &format, ABE_CBPR2_IDX, &dma_sink);
		abe_enable_data_transfer(MM_UL_PORT); /* enable all DMIC aquisition */
		abe_enable_data_transfer(MM_UL2_PORT); /* enable large-band DMIC aquisition */
		abe_enable_data_transfer(VX_UL_PORT);

		format.f = 48000;
		format.samp_format = STEREO_MSB;
		abe_connect_cbpr_dmareq_port(TONES_DL_PORT, &format, ABE_CBPR5_IDX, &dma_sink);
		format.f = 8000;
		format.samp_format = MONO_MSB;
		abe_connect_cbpr_dmareq_port(VX_DL_PORT, &format, ABE_CBPR1_IDX, &dma_sink);
		format.f = 48000;
		format.samp_format = STEREO_MSB;
		abe_connect_cbpr_dmareq_port(MM_DL_PORT, &format, ABE_CBPR0_IDX, &dma_sink);
		format.f = 24000;
		format.samp_format = STEREO_MSB;
		abe_connect_cbpr_dmareq_port(VIB_DL_PORT,&format, ABE_CBPR6_IDX, &dma_sink);
		abe_enable_data_transfer(TONES_DL_PORT);
		abe_enable_data_transfer(VX_DL_PORT);
		abe_enable_data_transfer(MM_DL_PORT); /* enable all the data paths */
		abe_enable_data_transfer(VIB_DL_PORT);

		abe_enable_data_transfer(DMIC_PORT);	/* DMIC ATC can be enabled even if the DMIC */
		abe_enable_data_transfer(PDM_DL_PORT);
		abe_enable_data_transfer(PDM_UL_PORT);

		/* mixers' configuration = voice on earphone + music on hands-free path */
		abe_write_mixer(MIXDL1, MUTE_GAIN, RAMP_0MS, MIX_DL1_INPUT_TONES);
		abe_write_mixer(MIXDL1, GAIN_M6dB, RAMP_0MS, MIX_DL1_INPUT_VX_DL);
		abe_write_mixer(MIXDL1, GAIN_M6dB, RAMP_0MS, MIX_DL1_INPUT_MM_DL);
		abe_write_mixer(MIXDL1, MUTE_GAIN, RAMP_0MS, MIX_DL1_INPUT_MM_UL2);

		abe_write_mixer(MIXDL2, MUTE_GAIN, RAMP_0MS, MIX_DL2_INPUT_TONES);
		abe_write_mixer(MIXDL2, GAIN_M6dB, RAMP_0MS, MIX_DL2_INPUT_VX_DL);
		abe_write_mixer(MIXDL2, GAIN_M6dB, RAMP_0MS, MIX_DL2_INPUT_MM_DL);
		abe_write_mixer(MIXDL2, MUTE_GAIN, RAMP_0MS, MIX_DL2_INPUT_MM_UL2);

		abe_write_mixer(MIXSDT, MUTE_GAIN, RAMP_0MS, MIX_SDT_INPUT_UP_MIXER);
		abe_write_mixer(MIXSDT, GAIN_0dB, RAMP_0MS, MIX_SDT_INPUT_DL1_MIXER);

		abe_write_mixer(MIXECHO, GAIN_0dB, RAMP_0MS, GAIN_LEFT_OFFSET);
		abe_write_mixer(MIXECHO, GAIN_0dB, RAMP_0MS, GAIN_RIGHT_OFFSET);

		abe_write_mixer(MIXAUDUL, MUTE_GAIN, RAMP_0MS, MIX_AUDUL_INPUT_TONES);
		abe_write_mixer(MIXAUDUL, GAIN_0dB, RAMP_0MS, MIX_AUDUL_INPUT_UPLINK);
		abe_write_mixer(MIXAUDUL, MUTE_GAIN, RAMP_0MS, MIX_AUDUL_INPUT_MM_DL);
		abe_write_mixer(MIXAUDUL, MUTE_GAIN, RAMP_0MS, MIX_AUDUL_INPUT_VX_DL);

		abe_write_mixer(MIXVXREC, MUTE_GAIN, RAMP_0MS, MIX_VXREC_INPUT_TONES);
		abe_write_mixer(MIXVXREC, GAIN_M6dB, RAMP_0MS, MIX_VXREC_INPUT_VX_DL);
		abe_write_mixer(MIXVXREC, MUTE_GAIN, RAMP_0MS, MIX_VXREC_INPUT_MM_DL);
		abe_write_mixer(MIXVXREC, GAIN_M6dB, RAMP_0MS, MIX_VXREC_INPUT_VX_UL);
		break;
	case 1:
		current_time = abe_test_read_time();
		if ((current_time - time_offset) < 100)
			break;
		else
			state ++;
		break;
	case 2:
		current_time = abe_test_read_time();
		if ((current_time - time_offset) < 100000)
			break;
		else
			state ++;
		break;
	default:
		state = 0;
		break;
	}
}
/**
*	@fn ABE_TEST_SCENARIO_3 ()
*
*	IRQ AUDIO PLAYER 44100Hz  OPP 25%
*/
void abe_test_scenario_3 (void)
{
	static abe_int32 time_offset, state, gain=0x040000, i;
	abe_data_format_t format;
	abe_uint32 data_sink;
	abe_use_case_id UC2[] = {ABE_AUDIO_PLAYER_ON_HEADSET_OR_EARPHONE, ABE_RINGER_TONES, (abe_use_case_id)0};
	// abe_use_case_id UC5[] = {ABE_AUDIO_PLAYER_ON_HEADSET_OR_EARPHONE, (abe_use_case_id)0};
	abe_opp_t OPP;
	abe_hw_config_init_t CONFIG;

	/* Scenario 1- 16kHz first */
	switch (state) {
	case 0:
		state ++;
		time_offset = abe_test_read_time();
		abe_reset_hal();
		abe_load_fw();

		abe_read_hardware_configuration(UC2, &OPP,  &CONFIG); /* check HW config and OPP config */
		abe_set_opp_processing(ABE_OPP25);	/* sets the OPP25 on FW05.xx */
		abe_write_event_generator(EVENT_44100);	/* "tick" of the audio engine */

		/* connect a Ping-Pong cache-flush protocol to MM_DL port */
#define N_SAMPLES_BYTES (25 *8)				/* half-buffer size in bytes, 32/32 data format */
		format.f = 44100;
		format.samp_format = STEREO_MSB;
		abe_add_subroutine(&abe_irq_pingpong_player_id,
			(abe_subroutine2) abe_default_irq_pingpong_player_32bits,
			SUB_0_PARAM, (abe_uint32*)0);

		abe_connect_irq_ping_pong_port(MM_DL_PORT, &format,
			abe_irq_pingpong_player_id, N_SAMPLES_BYTES,
			&data_sink, PING_PONG_WITH_MCU_IRQ);

		abe_enable_data_transfer(MM_DL_PORT);	/* enable all the data paths */
		abe_enable_data_transfer(PDM_DL_PORT);

		format.f = 48000;
		format.samp_format = STEREO_MSB;
		abe_connect_serial_port(MM_EXT_OUT_PORT, &format, MCBSP2_TX);
		abe_enable_data_transfer(MM_EXT_OUT_PORT);

		/* mixers' configuration */
		abe_write_mixer(MIXDL1, GAIN_0dB, RAMP_0MS, MIX_DL1_INPUT_TONES);
		abe_write_mixer(MIXDL1, GAIN_0dB, RAMP_0MS, MIX_DL1_INPUT_MM_DL);
		break;
	}
}
/**
* @fn ABE_TEST_SCENARIO_4 ()
*
*	DMA AUDIO PLAYER + DMA VOICE CALL 8kHz OPP 50%
*/
void abe_test_scenario_4 (void)
{
	static abe_int32 time_offset, state;
	abe_data_format_t format;
	abe_dma_t dma_sink;
	abe_uint32 current_time;
	abe_use_case_id UC2[] = {
		ABE_AUDIO_PLAYER_ON_HEADSET_OR_EARPHONE,
		ABE_RINGER_TONES,
		(abe_use_case_id)0
	};
	// abe_use_case_id UC5[] = {ABE_AUDIO_PLAYER_ON_HEADSET_OR_EARPHONE, (abe_use_case_id)0};
	abe_opp_t OPP;
	abe_hw_config_init_t CONFIG;

	/* Scenario 1- 16kHz first */
	switch (state) {
	case 0:
		state ++;
		time_offset = abe_test_read_time();

		/* check HW config and OPP config */
		abe_read_hardware_configuration(UC2, &OPP, &CONFIG);
		/* sets the OPP100 on FW05.xx */
		abe_set_opp_processing(OPP);
		/* "tick" of the audio engine */
		abe_write_event_generator(CONFIG.HAL_EVENT_SELECTION);

		abe_set_router_configuration(UPROUTE, UPROUTE_CONFIG_AMIC,
			(abe_router_t *) abe_router_ul_table_preset[UPROUTE_CONFIG_AMIC]);

		format.f = 48000;
		format.samp_format = STEREO_MSB;
		abe_connect_cbpr_dmareq_port(MM_UL_PORT, &format, ABE_CBPR3_IDX, &dma_sink);
		format.f = 48000;
		format.samp_format = STEREO_MSB;
		abe_connect_cbpr_dmareq_port(MM_UL2_PORT, &format, ABE_CBPR4_IDX, &dma_sink);
		format.f = 8000;
		format.samp_format = MONO_MSB;
		abe_connect_cbpr_dmareq_port(VX_UL_PORT, &format, ABE_CBPR2_IDX, &dma_sink);
		/* enable all DMIC aquisition */
		abe_enable_data_transfer(MM_UL_PORT);
		/* enable large-band DMIC aquisition */
		abe_enable_data_transfer(MM_UL2_PORT);
		abe_enable_data_transfer(VX_UL_PORT);

		format.f = 48000;
		format.samp_format = STEREO_MSB;
		abe_connect_cbpr_dmareq_port(TONES_DL_PORT, &format, ABE_CBPR5_IDX, &dma_sink);
		format.f = 8000;
		format.samp_format = MONO_MSB;
		abe_connect_cbpr_dmareq_port(VX_DL_PORT, &format, ABE_CBPR1_IDX, &dma_sink);
		format.f = 48000;
		format.samp_format = STEREO_MSB;
		abe_connect_cbpr_dmareq_port(MM_DL_PORT, &format, ABE_CBPR0_IDX, &dma_sink);
		format.f = 24000;
		format.samp_format = STEREO_MSB;
		abe_connect_cbpr_dmareq_port(VIB_DL_PORT, &format, ABE_CBPR6_IDX, &dma_sink);
		abe_enable_data_transfer(TONES_DL_PORT);
		abe_enable_data_transfer(VX_DL_PORT);
		/* enable all the data paths */
		abe_enable_data_transfer(MM_DL_PORT);
		abe_enable_data_transfer(VIB_DL_PORT);

		/* DMIC ATC can be enabled even if the DMIC */
		abe_enable_data_transfer(DMIC_PORT);
		abe_enable_data_transfer(PDM_DL_PORT);
		abe_enable_data_transfer(PDM_UL_PORT);

		/* mixers' configuration = voice on earphone + music on hands-free path */
		abe_write_mixer(MIXDL1, MUTE_GAIN, RAMP_0MS, MIX_DL1_INPUT_TONES);
		abe_write_mixer(MIXDL1, GAIN_M6dB, RAMP_0MS, MIX_DL1_INPUT_VX_DL);
		abe_write_mixer(MIXDL1, GAIN_M6dB, RAMP_0MS, MIX_DL1_INPUT_MM_DL);
		abe_write_mixer(MIXDL1, MUTE_GAIN, RAMP_0MS, MIX_DL1_INPUT_MM_UL2);

		abe_write_mixer(MIXDL2, MUTE_GAIN, RAMP_0MS, MIX_DL2_INPUT_TONES);
		abe_write_mixer(MIXDL2, GAIN_M6dB, RAMP_0MS, MIX_DL2_INPUT_VX_DL);
		abe_write_mixer(MIXDL2, GAIN_M6dB, RAMP_0MS, MIX_DL2_INPUT_MM_DL);
		abe_write_mixer(MIXDL2, MUTE_GAIN, RAMP_0MS, MIX_DL2_INPUT_MM_UL2);

		abe_write_mixer(MIXSDT, MUTE_GAIN, RAMP_0MS, MIX_SDT_INPUT_UP_MIXER);
		abe_write_mixer(MIXSDT, GAIN_0dB, RAMP_0MS, MIX_SDT_INPUT_DL1_MIXER);

		abe_write_mixer(MIXECHO, GAIN_0dB, RAMP_0MS, GAIN_LEFT_OFFSET);
		abe_write_mixer(MIXECHO, GAIN_0dB, RAMP_0MS, GAIN_RIGHT_OFFSET);

		abe_write_mixer(MIXAUDUL, MUTE_GAIN, RAMP_0MS, MIX_AUDUL_INPUT_TONES);
		abe_write_mixer(MIXAUDUL, GAIN_0dB, RAMP_0MS, MIX_AUDUL_INPUT_UPLINK);
		abe_write_mixer(MIXAUDUL, MUTE_GAIN, RAMP_0MS, MIX_AUDUL_INPUT_MM_DL);
		abe_write_mixer(MIXAUDUL, MUTE_GAIN, RAMP_0MS, MIX_AUDUL_INPUT_VX_DL);

		abe_write_mixer(MIXVXREC, MUTE_GAIN, RAMP_0MS, MIX_VXREC_INPUT_TONES);
		abe_write_mixer(MIXVXREC, GAIN_M6dB, RAMP_0MS, MIX_VXREC_INPUT_VX_DL);
		abe_write_mixer(MIXVXREC, MUTE_GAIN, RAMP_0MS, MIX_VXREC_INPUT_MM_DL);
		abe_write_mixer(MIXVXREC, GAIN_M6dB, RAMP_0MS, MIX_VXREC_INPUT_VX_UL);

		abe_write_gain(GAINS_DMIC1, GAIN_0dB, RAMP_0MS, GAIN_LEFT_OFFSET);
		abe_write_gain(GAINS_DMIC1, GAIN_0dB, RAMP_0MS, GAIN_RIGHT_OFFSET);
		abe_write_gain(GAINS_DMIC2, GAIN_0dB, RAMP_0MS, GAIN_LEFT_OFFSET);
		abe_write_gain(GAINS_DMIC2, GAIN_0dB, RAMP_0MS, GAIN_RIGHT_OFFSET);
		abe_write_gain(GAINS_DMIC3, GAIN_0dB, RAMP_0MS, GAIN_LEFT_OFFSET);
		abe_write_gain(GAINS_DMIC3, GAIN_0dB, RAMP_0MS, GAIN_RIGHT_OFFSET);
		abe_write_gain(GAINS_AMIC, GAIN_0dB, RAMP_0MS, GAIN_LEFT_OFFSET);
		abe_write_gain(GAINS_AMIC, GAIN_0dB, RAMP_0MS, GAIN_RIGHT_OFFSET);

		abe_write_gain(GAINS_SPLIT , GAIN_0dB, RAMP_0MS, GAIN_LEFT_OFFSET);
		abe_write_gain(GAINS_SPLIT,  GAIN_0dB, RAMP_0MS, GAIN_RIGHT_OFFSET);
		//abe_write_gain(GAINS_EANC , GAIN_0dB, RAMP_0MS, GAIN_LEFT_OFFSET);
		//abe_write_gain(GAINS_EANC,  GAIN_0dB, RAMP_0MS, GAIN_RIGHT_OFFSET);
		abe_write_gain(GAINS_DL1, GAIN_0dB, RAMP_0MS, GAIN_LEFT_OFFSET);
		abe_write_gain(GAINS_DL1, GAIN_0dB, RAMP_0MS, GAIN_RIGHT_OFFSET);
		abe_write_gain(GAINS_DL2, GAIN_0dB, RAMP_0MS, GAIN_LEFT_OFFSET);
		abe_write_gain(GAINS_DL2, GAIN_0dB, RAMP_0MS, GAIN_RIGHT_OFFSET);
		break;
		case 1:
			current_time = abe_test_read_time();
			if ((current_time - time_offset) < 100)
				break;
			else
				state ++; /* Gains switch */
			break;
		case 2:
			current_time = abe_test_read_time();
			if ((current_time - time_offset) < 100000)
				break;
			else
				state ++; /* Internal buffer analysis */
			break;
		default:
			state = 0;
			break;
		}
}

#if 0

	/*
	 *		build the default uplink router configurations
	 */
	abe_set_router_configuration(UPROUTE, UPROUTE_CONFIG_AMIC,
		(abe_router_t *) abe_router_ul_table_preset[UPROUTE_CONFIG_AMIC]);
#if 0
	abe_set_router_configuration(UPROUTE, UPROUTE_CONFIG_DMIC1,
		(abe_router_t *) abe_router_ul_table_preset[UPROUTE_CONFIG_DMIC1]);
	abe_set_router_configuration(UPROUTE, UPROUTE_CONFIG_DMIC2,
		(abe_router_t *) abe_router_ul_table_preset[UPROUTE_CONFIG_DMIC2]);
	abe_set_router_configuration(UPROUTE, UPROUTE_CONFIG_DMIC3,
		(abe_router_t *) abe_router_ul_table_preset[UPROUTE_CONFIG_DMIC3]);
#endif
	/* meaningful other microphone configuration can be added here */
	/* init hardware components */
	abe_hw_configuration();

	/* enable the VX_UL path with Analog microphones from Phoenix */
	/*	MM_DL INIT
		connect a DMA channel to MM_DL port (ATC FIFO) */
	format.f = 48000;
	format.samp_format = STEREO_MSB;
	abe_connect_cbpr_dmareq_port(MM_DL_PORT, &format, ABE_CBPR0_IDX, &dma_sink);

	/*	VX_DL INIT
		connect a DMA channel to VX_DL port (ATC FIFO) */
	format.f = 16000;
	format.samp_format = STEREO_MSB;
	abe_connect_cbpr_dmareq_port(VX_DL_PORT, &format, ABE_CBPR1_IDX, &dma_sink);

	/*	VX_UL INIT
		connect a DMA channel to VX_UL port (ATC FIFO) */
	format.f = 16000;
	format.samp_format = STEREO_MSB;
	abe_connect_cbpr_dmareq_port(VX_UL_PORT, &format, ABE_CBPR2_IDX, &dma_sink);

	/*	MM_UL2 INIT
		connect a DMA channel to MM_UL2 port (ATC FIFO) */
	format.f = 48000;
	format.samp_format = STEREO_MSB;
	abe_connect_cbpr_dmareq_port(MM_UL2_PORT, &format, ABE_CBPR4_IDX, &dma_sink);

	/*	MM_UL INIT
		connect a DMA channel to MM_UL port (ATC FIFO) */
	format.f = 48000;
	format.samp_format = STEREO_MSB;
	abe_connect_cbpr_dmareq_port(MM_UL_PORT, &format, ABE_CBPR3_IDX, &dma_sink);

	/*	TONES INIT
		connect a DMA channel to TONES port (ATC FIFO) */
	format.f = 48000;
	format.samp_format = STEREO_MSB;
	abe_connect_cbpr_dmareq_port(TONES_DL_PORT, &format, ABE_CBPR5_IDX, &dma_sink);

	/*	VIBRA/HAPTICS INIT
		connect a DMA channel to VIBRA/HAPTICS port (ATC FIFO) */
	format.f = 24000;
	format.samp_format = STEREO_MSB;
	abe_connect_cbpr_dmareq_port(VIB_DL_PORT, &format, ABE_CBPR6_IDX, &dma_sink);

	/* mixers' default configuration = voice on earphone + music on hands-free path */
	case 2:
	/* Scenario 2-	8kHz first */
		switch (time10us) {
		case 1:
			/* check HW config and OPP config */
			abe_read_hardware_configuration(UC2, &OPP,	&CONFIG);
			/* sets the OPP100 on FW05.xx */
			abe_set_opp_processing(OPP);
			/* "tick" of the audio engine */
			abe_write_event_generator(CONFIG.HAL_EVENT_SELECTION);
			// enables VOICECALL-MMDL-MMUL-8/16kHz-ROUTING
			abe_reset_hal();
			format.f = 8000;
			format.samp_format = MONO_MSB;
			abe_connect_cbpr_dmareq_port(VX_DL_PORT, &format, ABE_CBPR1_IDX, &dma_sink);
			format.f = 8000;
			format.samp_format = MONO_MSB;
			abe_connect_cbpr_dmareq_port(VX_UL_PORT, &format, ABE_CBPR2_IDX, &dma_sink);

			abe_write_mixer(MIXDL1, GAIN_0dB, RAMP_1MS, MIX_DL1_INPUT_VX_DL);
			abe_write_mixer(MIXDL2, GAIN_0dB, RAMP_50MS, MIX_DL2_INPUT_MM_DL);

			/* enable large-band DMIC aquisition */
			abe_enable_data_transfer(MM_UL2_PORT);
			/* enable all DMIC aquisition */
			abe_enable_data_transfer(MM_UL_PORT);
			/* enable all the data paths */
			abe_enable_data_transfer(MM_DL_PORT);
			abe_enable_data_transfer(VX_DL_PORT);
			abe_enable_data_transfer(VX_UL_PORT);
			abe_enable_data_transfer(PDM_UL_PORT);
			/* DMIC ATC can be enabled even if the DMIC */
			abe_enable_data_transfer(DMIC_PORT);
			abe_enable_data_transfer(PDM_DL_PORT);
			abe_enable_data_transfer(TONES_DL_PORT);
			break;
		case 100:
			abe_write_mixer(MIXDL1, GAIN_M6dB, RAMP_0MS, MIX_DL1_INPUT_TONES);
			abe_write_mixer(MIXDL1, GAIN_M6dB, RAMP_1MS, MIX_DL1_INPUT_VX_DL);
			abe_write_mixer(MIXDL1, GAIN_M6dB, RAMP_2MS, MIX_DL1_INPUT_MM_DL);
			abe_write_mixer(MIXDL1, GAIN_M6dB, RAMP_5MS, MIX_DL1_INPUT_MM_UL2);
			break;
		case 1200:
			abe_set_router_configuration(UPROUTE, UPROUTE_CONFIG_DMIC1,
				(abe_router_t *) abe_router_ul_table_preset[UPROUTE_CONFIG_DMIC1]);
			break;
		case 8000:		// end
			fcloseall();
			exit(-2);
		}
	 /* case scenario_id ==2 */
	break;
	case 3:
		/* Scenario 3 PING-PONG DMAreq */
		switch (time10us) {
		case 1:
			/* Ping-Pong access through MM_DL using Left/Right 16bits/16bits data format */
			/* To be added here :  Device driver initialization following
					abe_read_hardware_configuration() returned data
					McPDM_DL : 6 slots activated (5 + Commands)
					DMIC : 6 microphones activated
					McPDM_UL : 2 microphones activated (No status)
			*/
			abe_read_hardware_configuration(UC5, &OPP, &CONFIG);
			abe_set_opp_processing(OPP);
			abe_write_event_generator(CONFIG.HAL_EVENT_SELECTION);

			/* MM_DL INIT (overwrite the previous default initialization made above  */
			format.f = 48000;
			format.samp_format = MONO_MSB;

			/* connect a Ping-Pong SDMA protocol to MM_DL port with Ping-Pong 12 mono
			 * samples (12x4 bytes for each ping & pong size)*/
			abe_connect_dmareq_ping_pong_port(MM_DL_PORT, &format, ABE_CBPR0_IDX, (12 * 4), &dma_sink);

			/* mixers' configuration = voice on earphone + music on hands-free path */
			abe_write_mixer(MIXDL1, GAIN_0dB, RAMP_2MS, MIX_DL1_INPUT_MM_DL);
			abe_write_mixer(MIXDL2, GAIN_0dB, RAMP_50MS, MIX_DL2_INPUT_MM_DL);

			/* Here : connect the sDMA to "dma_sink" content */
			/* enable all the data paths */
			abe_enable_data_transfer(MM_DL_PORT);
			abe_enable_data_transfer(PDM_DL_PORT);
			break;
		case 8000:		// end
			fcloseall();
			exit(-3);
		}
	/* case scenario_id ==3 */
	break;
	case 40:
		 /* Scenario 4.0	PING_PONG+ IRQ TO MCU */
		switch (time10us) {
		case 1:
			/* check HW config and OPP config */
			abe_read_hardware_configuration(UC5, &OPP,	&CONFIG);
			/* sets the OPP100 on FW05.xx */
			abe_set_opp_processing(OPP);
			/* "tick" of the audio engine */
			abe_write_event_generator(CONFIG.HAL_EVENT_SELECTION);

			/* MM_DL INIT (overwrite the previous default initialization made above  */
			format.f = 48000;
			format.samp_format = STEREO_16_16;

			/* connect a Ping-Pong cache-flush protocol to MM_DL port with 50Hz (20ms) rate */
			abe_add_subroutine(&abe_irq_pingpong_player_id,
				(abe_subroutine2) abe_default_irq_pingpong_player, SUB_0_PARAM, (abe_uint32*)0);

			#define N_SAMPLES_BYTES (24 *4)		  // @@@@ to be tuned
			abe_connect_irq_ping_pong_port(MM_DL_PORT, &format,
				abe_irq_pingpong_player_id, N_SAMPLES_BYTES, &data_sink, PING_PONG_WITH_MCU_IRQ);

			abe_write_mixer(MIXDL1,	GAIN_0dB, RAMP_0MS, MIX_DL1_INPUT_TONES);
			abe_write_mixer(MIXDL1,	GAIN_0dB, RAMP_0MS, MIX_DL1_INPUT_VX_DL);
			abe_write_mixer(MIXDL1,	GAIN_0dB,  RAMP_0MS, MIX_DL1_INPUT_MM_DL);
			abe_write_mixer(MIXDL1,	GAIN_0dB, RAMP_0MS, MIX_DL1_INPUT_MM_UL2);
			abe_write_mixer(MIXDL2,	GAIN_0dB, RAMP_0MS, MIX_DL2_INPUT_TONES);
			abe_write_mixer(MIXDL2,	GAIN_0dB, RAMP_0MS, MIX_DL2_INPUT_VX_DL);
			abe_write_mixer(MIXDL2,	GAIN_0dB,  RAMP_0MS, MIX_DL2_INPUT_MM_DL);
			abe_write_mixer(MIXDL2,	GAIN_0dB, RAMP_0MS, MIX_DL2_INPUT_MM_UL2);

			abe_write_mixer(MIXSDT,	GAIN_0dB,  RAMP_0MS, MIX_SDT_INPUT_UP_MIXER);
			abe_write_mixer(MIXSDT,	MUTE_GAIN, RAMP_0MS, MIX_SDT_INPUT_DL1_MIXER);

			/* enable all the data paths */
			abe_enable_data_transfer(MM_DL_PORT);
			abe_enable_data_transfer(PDM_DL_PORT);
			break;
		case 1200:
			abe_set_router_configuration(UPROUTE, UPROUTE_CONFIG_DMIC1,
				(abe_router_t *) abe_router_ul_table_preset[UPROUTE_CONFIG_DMIC1]);
			break;
		case 2400:		// end
			fcloseall();
			exit(-4);
		}
	 /* case scenario_id ==4 */
	break;
	case 41:
		/* Scenario 4.1   PING_PONG+ IRQ TO MCU 32BITS */
		switch (time10us) {
		case 1:
			/* check HW config and OPP config */
			abe_read_hardware_configuration(UC5, &OPP, &CONFIG);
			/* sets the OPP100 on FW05.xx */
			abe_set_opp_processing(OPP);
			/* "tick" of the audio engine */
			abe_write_event_generator(CONFIG.HAL_EVENT_SELECTION);

			/* MM_DL INIT(overwrite the previous default initialization made above	*/
			format.f = 48000;
			format.samp_format = STEREO_MSB;

			/* connect a Ping-Pong cache-flush protocol to MM_DL port with 50Hz (20ms) rate */
			abe_add_subroutine(&abe_irq_pingpong_player_id,
				(abe_subroutine2) abe_default_irq_pingpong_player_32bits, SUB_0_PARAM, (abe_uint32*)0);

			#define N_SAMPLES_BYTES (24 * 4)	   // @@@@ to be tuned
			abe_connect_irq_ping_pong_port(MM_DL_PORT, &format,
				abe_irq_pingpong_player_id, N_SAMPLES_BYTES, &data_sink, PING_PONG_WITH_MCU_IRQ);

			abe_write_mixer(MIXDL1,	MUTE_GAIN, RAMP_0MS, MIX_DL1_INPUT_TONES);
			abe_write_mixer(MIXDL1,	MUTE_GAIN, RAMP_0MS, MIX_DL1_INPUT_VX_DL);
			abe_write_mixer(MIXDL1,	GAIN_0dB,  RAMP_0MS, MIX_DL1_INPUT_MM_DL);
			abe_write_mixer(MIXDL1,	MUTE_GAIN, RAMP_0MS, MIX_DL1_INPUT_MM_UL2);

			abe_write_mixer(MIXDL2,	MUTE_GAIN, RAMP_0MS, MIX_DL2_INPUT_TONES);
			abe_write_mixer(MIXDL2,	MUTE_GAIN, RAMP_0MS, MIX_DL2_INPUT_VX_DL);
			abe_write_mixer(MIXDL2,	GAIN_0dB,  RAMP_0MS, MIX_DL2_INPUT_MM_DL);
			abe_write_mixer(MIXDL2,	MUTE_GAIN, RAMP_0MS, MIX_DL2_INPUT_MM_UL2);

			abe_write_mixer(MIXSDT,	GAIN_0dB,  RAMP_0MS, MIX_SDT_INPUT_UP_MIXER);
			abe_write_mixer(MIXSDT,	MUTE_GAIN, RAMP_0MS, MIX_SDT_INPUT_DL1_MIXER);

			/* enable all the data paths */
			abe_enable_data_transfer(MM_DL_PORT );
			abe_enable_data_transfer(PDM_DL_PORT);

			break;
		case 1200:
			abe_set_router_configuration(UPROUTE, UPROUTE_CONFIG_DMIC1,
				(abe_router_t *) abe_router_ul_table_preset[UPROUTE_CONFIG_DMIC1]);
			break;

		case 2400:		// end
			fcloseall();
			exit(-4);
		}
	 /* case scenario_id ==4 */
	break;

	case 5:
		/* Scenario 5	  CHECK APS ADAPTATION ALGO */
		switch (time10us) {
		case 1:
			/* check HW config and OPP config */
			abe_read_hardware_configuration(UC5, &OPP,	&CONFIG);
			/* sets the OPP100 on FW05.xx */
			abe_set_opp_processing(OPP);
			/* "tick" of the audio engine */
			abe_write_event_generator(CONFIG.HAL_EVENT_SELECTION);

			/* MM_DL INIT(overwrite the previous default initialization made above	*/
			format.f = 48000;
			format.samp_format = STEREO_16_16;

			/* connect a Ping-Pong cache-flush protocol to MM_DL port with 50Hz (20ms) rate */
			abe_add_subroutine(&abe_irq_pingpong_player_id,
				(abe_subroutine2) abe_default_irq_pingpong_player, SUB_0_PARAM, (abe_uint32*)0);

			#define N_SAMPLES_BYTES (24 *4)		  // @@@@ to be tuned
			abe_connect_irq_ping_pong_port(MM_DL_PORT,
				&format, abe_irq_pingpong_player_id, N_SAMPLES_BYTES,
							&data_sink, PING_PONG_WITH_MCU_IRQ);

			/* mixers' configuration = voice on earphone + music on hands-free path */
			abe_write_mixer(MIXDL1, GAIN_0dB, RAMP_2MS, MIX_DL1_INPUT_MM_DL);
			abe_write_mixer(MIXDL2, GAIN_0dB, RAMP_50MS, MIX_DL2_INPUT_MM_DL);

			/* enable all the data paths */
			abe_enable_data_transfer(MM_DL_PORT);
			abe_enable_data_transfer(PDM_DL_PORT);

			/* connect a Ping-Pong cache-flush protocol to MM_DL port with 50Hz (20ms) rate */
			abe_add_subroutine(&abe_irq_aps_adaptation_id,
			(abe_subroutine2) abe_default_irq_aps_adaptation, SUB_0_PARAM, (abe_uint32*)0);
			break;
		} /* case scenario_id ==5 */

	abe_set_router_configuration(UPROUTE, UPROUTE_CONFIG_AMIC,
			(abe_router_t *) abe_router_ul_table_preset[UPROUTE_CONFIG_AMIC]);
	abe_set_router_configuration(UPROUTE, UPROUTE_CONFIG_DMIC1,
			(abe_router_t *) abe_router_ul_table_preset[UPROUTE_CONFIG_DMIC1]);
	abe_set_router_configuration(UPROUTE, UPROUTE_CONFIG_DMIC2,
			(abe_router_t *) abe_router_ul_table_preset[UPROUTE_CONFIG_DMIC2]);
	abe_set_router_configuration(UPROUTE, UPROUTE_CONFIG_DMIC3,
			(abe_router_t *) abe_router_ul_table_preset[UPROUTE_CONFIG_DMIC3]);
	case UC31_VOICE_CALL_8KMONO:
		abe_disable_data_transfer(VX_DL_PORT);
		abe_disable_data_transfer(VX_UL_PORT);
		format.f = 8000;
		format.samp_format = MONO_MSB;
		abe_connect_cbpr_dmareq_port(VX_DL_PORT, &format, ABE_CBPR1_IDX, &dma_sink);
		format.f = 8000;
		format.samp_format = MONO_MSB;
		abe_connect_cbpr_dmareq_port(VX_UL_PORT, &format, ABE_CBPR2_IDX, &dma_sink);
		abe_enable_data_transfer(VX_DL_PORT);
		abe_enable_data_transfer(VX_UL_PORT);
	case UC32_VOICE_CALL_8KSTEREO:
		format.f = 8000;
		format.samp_format = STEREO_MSB;
		abe_connect_cbpr_dmareq_port(VX_DL_PORT, &format, ABE_CBPR1_IDX, &dma_sink);
		format.f = 8000;
		format.samp_format = STEREO_MSB;
		abe_connect_cbpr_dmareq_port(VX_UL_PORT, &format, ABE_CBPR2_IDX, &dma_sink);
	case UC33_VOICE_CALL_16KMONO:
		format.f = 16000;
		format.samp_format = MONO_MSB;
		abe_connect_cbpr_dmareq_port(VX_DL_PORT, &format, ABE_CBPR1_IDX, &dma_sink);
		format.f = 16000;
		format.samp_format = MONO_MSB;
		abe_connect_cbpr_dmareq_port(VX_UL_PORT, &format, ABE_CBPR2_IDX, &dma_sink);
	case UC34_VOICE_CALL_16KSTEREO:
		format.f = 16000;
		format.samp_format = STEREO_MSB;
		abe_connect_cbpr_dmareq_port(VX_DL_PORT, &format, ABE_CBPR1_IDX, &dma_sink);
		format.f = 16000;
		format.samp_format = STEREO_MSB;
		abe_connect_cbpr_dmareq_port(VX_UL_PORT, &format, ABE_CBPR2_IDX, &dma_sink);
	case UC35_MMDL_MONO:
		format.f = 48000;
		format.samp_format = MONO_MSB;
		abe_connect_cbpr_dmareq_port(MM_DL_PORT, &format, ABE_CBPR0_IDX, &dma_sink);
	case UC36_MMDL_STEREO:
		format.f = 48000;
		format.samp_format = STEREO_MSB;
		abe_connect_cbpr_dmareq_port(MM_DL_PORT, &format, ABE_CBPR0_IDX, &dma_sink);
	case UC37_MMUL2_MONO:
		format.f = 48000;
		format.samp_format = MONO_MSB;
		abe_connect_cbpr_dmareq_port(MM_UL2_PORT, &format, ABE_CBPR4_IDX, &dma_sink);
	case UC38_MMUL2_STEREO:
		format.f = 48000;
		format.samp_format = MONO_MSB;
		abe_connect_cbpr_dmareq_port(MM_UL2_PORT, &format, ABE_CBPR4_IDX, &dma_sink);
	case UC91_ASRC_DRIFT1:
		abe_set_asrc_drift_control(VX_UL_PORT, FORCED_DRIFT_CONTROL);
		abe_write_asrc(VX_UL_PORT, 100);
		abe_set_asrc_drift_control(VX_DL_PORT, FORCED_DRIFT_CONTROL);
		abe_write_asrc(VX_DL_PORT, 200);
		abe_set_asrc_drift_control(MM_DL_PORT, FORCED_DRIFT_CONTROL);
		abe_write_asrc(MM_DL_PORT, 300);
	case UC92_ASRC_DRIFT2:
		abe_set_asrc_drift_control(VX_UL_PORT, FORCED_DRIFT_CONTROL);
		abe_write_asrc(VX_UL_PORT, -100);
		abe_set_asrc_drift_control(VX_DL_PORT, FORCED_DRIFT_CONTROL);
		abe_write_asrc(VX_DL_PORT, -200);
		abe_set_asrc_drift_control(MM_DL_PORT, FORCED_DRIFT_CONTROL);
		abe_write_asrc(MM_DL_PORT, -300);
#endif
