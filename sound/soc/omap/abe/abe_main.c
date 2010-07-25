/* =============================================================================
*	     Texas Instruments OMAP(TM) Platform Software
*  (c) Copyright 2009 Texas Instruments Incorporated.  All Rights Reserved.
*
*  Use of this software is controlled by the terms and conditions found
*  in the license agreement under which this software has been supplied.
* =========================================================================== */
/**
 * @file  ABE_MAIN.C
 *
 * 'ABEMAIN.C' dummy main of the HAL
 *
 * @path
 * @rev     01.00
 */
/* ----------------------------------------------------------------------------
*!
*! Revision History
*! ===================================
*! 27-Nov-2008 Original (LLF)
*! 05-Jun-2009 V05 release
* =========================================================================== */


#if !PC_SIMULATION


#include "abe_main.h"
#include "abe_test.h"

void main (void)
{

	abe_dma_t dma_sink, dma_source;
	abe_data_format_t format;
	abe_uint32 base_address;

	abe_auto_check_data_format_translation();
	abe_reset_hal();
	abe_check_opp();
	abe_check_dma();

	/*
		To be added here :
		Device driver initialization:
		McPDM_DL : threshold=1, 6 slots activated
		DMIC : threshold=1, 6 microphones activated
		McPDM_UL : threshold=1, two microphones activated
	*/


	/*  MM_DL INIT
		connect a DMA channel to MM_DL port (ATC FIFO)
		format.f = 48000;
		format.samp_format = STEREO_MSB;
		abe_connect_cbpr_dmareq_port (MM_DL_PORT, &format, ABE_CBPR0_IDX, &dma_sink);

		connect a Ping-Pong SDMA protocol to MM_DL port with Ping-Pong 576 stereo samples
		format.f = 48000;
		format.samp_format = STEREO_MSB;
		abe_connect_dmareq_ping_pong_port (MM_DL_PORT, &format, ABE_CBPR0_IDX, (576 * 4), &dma_sink);

		connect a Ping-Pong cache-flush protocol to MM_DL port with 50Hz (20ms) rate
	*/
	abe_add_subroutine(&abe_irq_pingpong_player_id,
		(abe_subroutine2) abe_default_irq_pingpong_player, SUB_0_PARAM, (abe_uint32*)0 );
	format.f = 48000;
	format.samp_format = STEREO_MSB;
/* ping-pong access to MM_DL at 48kHz Mono with 20ms packet sizes */
#define N_SAMPLES ((int)(48000 * 0.020))
	abe_connect_irq_ping_pong_port(MM_DL_PORT, &format, abe_irq_pingpong_player_id,
				N_SAMPLES, &base_address, PING_PONG_WITH_MCU_IRQ);

	/*  VX_DL INIT
		connect a DMA channel to VX_DL port (ATC FIFO)
	*/
	format.f = 8000;
	format.samp_format = MONO_MSB;
	abe_connect_cbpr_dmareq_port(VX_DL_PORT, &format, ABE_CBPR1_IDX, &dma_sink);

	/*  VX_UL INIT
	    connect a DMA channel to VX_UL port (ATC FIFO)
	*/
	format.f = 8000;
	format.samp_format = MONO_MSB;
	abe_connect_cbpr_dmareq_port(VX_UL_PORT, &format, ABE_CBPR2_IDX, &dma_source);

	/* make the AE waking event to be the McPDM DMA requests */
	abe_write_event_generator(EVENT_MCPDM);

	abe_enable_data_transfer(MM_DL_PORT );
	abe_enable_data_transfer(VX_DL_PORT );
	abe_enable_data_transfer(VX_UL_PORT );
	abe_enable_data_transfer(PDM_UL_PORT);
	abe_enable_data_transfer(PDM_DL1_PORT);

}

#endif
