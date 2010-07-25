/*
 * ==========================================================================
 *		 Texas Instruments OMAP(TM) Platform Firmware
 * (c) Copyright 2009, Texas Instruments Incorporated.	All Rights Reserved.
 *
 *  Use of this firmware is controlled by the terms and conditions found
 *  in the license agreement under which this firmware has been supplied.
 * ==========================================================================
 */

#include "abe_main.h"
#include "abe_typedef.h"
#include "abe_initxxx_labels.h"
#include "abe_dbg.h"

static abe_uint32 ABE_FW_PM[ABE_PMEM_SIZE / 4]	= {
#include "C_ABE_FW.PM"
};
static abe_uint32 ABE_FW_CM[ABE_CMEM_SIZE / 4]	= {
#include "C_ABE_FW.CM"
};
static abe_uint32 ABE_FW_DM[ABE_DMEM_SIZE / 4]	= {
#include "C_ABE_FW.lDM"
};
static abe_uint32 ABE_FW_SM[ABE_SMEM_SIZE / 4]	= {
#include "C_ABE_FW.SM32"
};

/**
* @fn abe_reset_hal()
*
*  Operations : reset the HAL by reloading the static variables and default AESS registers.
*	Called after a PRCM cold-start reset of ABE
*
* @see	ABE_API.h
*/
void abe_reset_hal(void)
{
	_lock_enter
	_log(id_reset_hal,0,0,0)

	abe_dbg_output = TERMINAL_OUTPUT;
	abe_irq_dbg_read_ptr = 0; /* IRQ & DBG circular read pointer in DMEM */
	abe_dbg_mask = (abe_dbg_t)(-1); /* set debug mask to "enable all traces" */
	abe_hw_configuration();

	_lock_exit

}

/**
* @fn abe_load_fwl()
*
*  Operations :
*      loads the Audio Engine firmware, generate a single pulse on the Event generator
*      to let execution start, read the version number returned from this execution.
*
* @see	ABE_API.h
*/
void abe_load_fw_param(abe_uint32 *PMEM, abe_uint32 PMEM_SIZE,
			abe_uint32 *CMEM, abe_uint32 CMEM_SIZE,
				abe_uint32 *SMEM, abe_uint32 SMEM_SIZE,
					abe_uint32 *DMEM, abe_uint32 DMEM_SIZE)
{
	static abe_uint32 warm_boot;
	abe_uint32 event_gen;

	_lock_enter
	_log(id_load_fw_param,0,0,0)

#if PC_SIMULATION
	/* the code is loaded from the Checkers */
#else
	/* do not load PMEM */
	if (warm_boot) {
		/* Stop the event Generator */
		event_gen = 0;
		abe_block_copy(COPY_FROM_HOST_TO_ABE, ABE_ATC,
				EVENT_GENERATOR_START, &event_gen, 4);

		/* Now we are sure the firmware is stalled */
		abe_block_copy(COPY_FROM_HOST_TO_ABE, ABE_CMEM, 0, CMEM, CMEM_SIZE);
		abe_block_copy(COPY_FROM_HOST_TO_ABE, ABE_SMEM, 0, SMEM, SMEM_SIZE);
		abe_block_copy(COPY_FROM_HOST_TO_ABE, ABE_DMEM, 0, DMEM, DMEM_SIZE);

		/* Restore the event Generator status */
		event_gen = 1;
		abe_block_copy(COPY_FROM_HOST_TO_ABE, ABE_ATC,
				EVENT_GENERATOR_START, &event_gen, 4);
	} else {
		abe_block_copy(COPY_FROM_HOST_TO_ABE, ABE_PMEM, 0, PMEM, PMEM_SIZE);
		abe_block_copy(COPY_FROM_HOST_TO_ABE, ABE_CMEM, 0, CMEM, CMEM_SIZE);
		abe_block_copy(COPY_FROM_HOST_TO_ABE, ABE_SMEM, 0, SMEM, SMEM_SIZE);
		abe_block_copy(COPY_FROM_HOST_TO_ABE, ABE_DMEM, 0, DMEM, DMEM_SIZE);
	}

	warm_boot = 1;
#endif
	_lock_exit
}

void abe_load_fw(void)
{
	_lock_enter
	_log(id_load_fw,0,0,0)

	abe_load_fw_param(ABE_FW_PM, sizeof (ABE_FW_PM),
			ABE_FW_CM, sizeof(ABE_FW_CM),
			ABE_FW_SM, sizeof(ABE_FW_SM),
			ABE_FW_DM, sizeof(ABE_FW_DM));

	abe_reset_all_ports();
	abe_build_scheduler_table();
	abe_reset_all_sequence();
	abe_select_main_port(PDM_DL_PORT);
	_lock_exit

}

/*
 *  ABE_HARDWARE_CONFIGURATION
 *
 *  Parameter  :
 *  U : use-case description list (pointer)
 *  H : pointer to the output structure
 *
 *  Operations :
 *	return a structure with the HW thresholds compatible with the HAL/FW/AESS_ATC
 *	will be upgraded in FW06
 *
 *  Return value :
 *	None.
 */
void abe_read_hardware_configuration(abe_use_case_id *u, abe_opp_t *o, abe_hw_config_init_t *hw)
{
	_lock_enter
	_log(id_read_hardware_configuration,(abe_uint32)(u),(abe_uint32)u>>8,(abe_uint32)u>>16)

	abe_read_use_case_opp(u, o);

	hw->MCPDM_CTRL__DIV_SEL = 0;		/* 0: 96kHz   1:192kHz */
	hw->MCPDM_CTRL__CMD_INT = 1;		/* 0: no command in the FIFO,  1: 6 data on each lines (with commands) */
	hw->MCPDM_CTRL__PDMOUTFORMAT = 0;	/* 0:MSB aligned  1:LSB aligned */
	hw->MCPDM_CTRL__PDM_DN5_EN = 1;
	hw->MCPDM_CTRL__PDM_DN4_EN = 1;
	hw->MCPDM_CTRL__PDM_DN3_EN = 1;
	hw->MCPDM_CTRL__PDM_DN2_EN = 1;
	hw->MCPDM_CTRL__PDM_DN1_EN = 1;
	hw->MCPDM_CTRL__PDM_UP3_EN = 0;
	hw->MCPDM_CTRL__PDM_UP2_EN = 1;
	hw->MCPDM_CTRL__PDM_UP1_EN = 1;
	hw->MCPDM_FIFO_CTRL_DN__DN_TRESH = MCPDM_DL_ITER/6; /* All the McPDM_DL FIFOs are enabled simultaneously */
	hw->MCPDM_FIFO_CTRL_UP__UP_TRESH = MCPDM_UL_ITER/2; /* number of ATC access upon AMIC DMArequests, 2 the FIFOs channels are enabled */

	hw->DMIC_CTRL__DMIC_CLK_DIV = 0;	/* 0:2.4MHz  1:3.84MHz */
	hw->DMIC_CTRL__DMICOUTFORMAT = 0;	/* 0:MSB aligned  1:LSB aligned */
	hw->DMIC_CTRL__DMIC_UP3_EN = 1;
	hw->DMIC_CTRL__DMIC_UP2_EN = 1;
	hw->DMIC_CTRL__DMIC_UP1_EN = 1;
	hw->DMIC_FIFO_CTRL__DMIC_TRESH = DMIC_ITER/6; /* 1*(DMIC_UP1_EN+ 2+ 3)*2 OCP read access every 96/88.1 KHz. */

	/*   MCBSP SPECIFICATION
	RJUST = 00 Right justify data and zero fill MSBs in DRR[1,2]
	RJUST = 01 Right justify data and sign extend it into the MSBs in DRR[1,2]
	RJUST = 10 Left justify data and zero fill LSBs in DRR[1,2]
	MCBSPLP_RJUST_MODE_RIGHT_ZERO = 0x0,
	MCBSPLP_RJUST_MODE_RIGHT_SIGN = 0x1,
	MCBSPLP_RJUST_MODE_LEFT_ZERO  = 0x2,
	MCBSPLP_RJUST_MODE_MAX = MCBSPLP_RJUST_MODE_LEFT_ZERO
	*/
	hw->MCBSP_SPCR1_REG__RJUST = 2;
	hw->MCBSP_THRSH2_REG_REG__XTHRESHOLD = 1;	/* 1=MONO, 2=STEREO, 3=TDM_3_CHANNELS, 4=TDM_4_CHANNELS, .... */
	hw->MCBSP_THRSH1_REG_REG__RTHRESHOLD = 1;	/* 1=MONO, 2=STEREO, 3=TDM_3_CHANNELS, 4=TDM_4_CHANNELS, .... */

	hw->SLIMBUS_DCT_FIFO_SETUP_REG__SB_THRESHOLD = 1;	/* Slimbus IP FIFO thresholds */

	hw->AESS_EVENT_GENERATOR_COUNTER__COUNTER_VALUE = EVENT_GENERATOR_COUNTER_DEFAULT;	/* 2050 gives about 96kHz */
	hw->AESS_EVENT_SOURCE_SELECTION__SELECTION = 1;		/* 0: DMAreq, 1:Counter */
	hw->AESS_AUDIO_ENGINE_SCHEDULER__DMA_REQ_SELECTION = ABE_ATC_MCPDMDL_DMA_REQ;	 /* 5bits DMAreq selection */

	hw->HAL_EVENT_SELECTION = EVENT_TIMER;	/* THE famous EVENT timer ! */
	_lock_exit

}

/*
 *	ABE_IRQ_PROCESSING
 *
 *	Parameter  :
 *		No parameter
 *
 *	Operations :
 *		This subroutine is call upon reception of "MA_IRQ_99 ABE_MPU_IRQ" ABE interrupt
 *		This subroutine will check the IRQ_FIFO from the AE and act accordingly.
 *		Some IRQ source are originated for the delivery of "end of time sequenced tasks"
 *		notifications, some are originated from the Ping-Pong protocols, some are generated from
 *		the embedded debugger when the firmware stops on programmable break-points, etc …
 *
 *	Return value :
 *		None.
 */
void abe_irq_processing(void)
{
	abe_uint32 clear_abe_irq;
	abe_uint32 abe_irq_dbg_write_ptr, i, cmem_src, sm_cm = 0;
	abe_irq_data_t IRQ_data;
       _lock_enter
        _log(id_irq_processing,0,0,0)

#define IrqFiFoMask ((D_McuIrqFifo_sizeof >> 2) - 1)

	/* extract the write pointer index from CMEM memory (INITPTR format) */
	/* CMEM address of the write pointer in bytes */
	cmem_src = MCU_IRQ_FIFO_ptr_labelID * 4;
	abe_block_copy(COPY_FROM_ABE_TO_HOST, ABE_CMEM, cmem_src,
		(abe_uint32*)&abe_irq_dbg_write_ptr,
			sizeof (abe_irq_dbg_write_ptr));
	abe_irq_dbg_write_ptr = sm_cm >> 16;	/* AESS left-pointer index located on MSBs */
	abe_irq_dbg_write_ptr &= 0xFF;

	/* loop on the IRQ FIFO content */
	for (i = 0; i < D_McuIrqFifo_sizeof; i++) {
		/* stop when the FIFO is empty */
		if (abe_irq_dbg_write_ptr == abe_irq_dbg_read_ptr)
			break;
		/* read the IRQ/DBG FIFO */
		abe_block_copy(COPY_FROM_ABE_TO_HOST, ABE_DMEM,
			D_McuIrqFifo_ADDR + (i << 2), (abe_uint32 *)&IRQ_data,
					sizeof (IRQ_data));
		abe_irq_dbg_read_ptr = (abe_irq_dbg_read_ptr + 1) & IrqFiFoMask;

		/* select the source of the interrupt */
		switch (IRQ_data.tag) {
		case IRQtag_APS:
			_log(id_irq_processing,(abe_uint32)(IRQ_data.data),0,1)
			abe_irq_aps (IRQ_data.data);
			break;
		case IRQtag_PP:
			_log(id_irq_processing,0,0,2)
			abe_irq_ping_pong ();
			break;
		case IRQtag_COUNT:
			_log(id_irq_processing,(abe_uint32)(IRQ_data.data),0,3)
			abe_irq_check_for_sequences (IRQ_data.data);
			break;
		default:
		break;
		}
	}

	abe_monitoring();

	clear_abe_irq = 1;
	abe_block_copy(COPY_FROM_HOST_TO_ABE, ABE_ATC, ABE_MCU_IRQSTATUS,
							     &clear_abe_irq, 4);
	_lock_exit
}

/*
 *  ABE_SELECT_MAIN_PORT
 *
 *  Parameter  :
 *	id : audio port name
 *
 *  Operations :
 *	tells the FW which is the reference stream for adjusting
 *	the processing on 23/24/25 slots
 *
 *  Return value:
 *	None.
 */
void abe_select_main_port (abe_port_id id)
{
	abe_uint32 selection;

	_lock_enter
	_log(id_select_main_port,id,0,0)

	/* flow control */
	selection = D_IOdescr_ADDR + id*sizeof(ABE_SIODescriptor) + flow_counter_;
	abe_block_copy(COPY_FROM_HOST_TO_ABE, ABE_DMEM, D_Slot23_ctrl_ADDR, &selection, 4);

	_lock_exit
}

/*
 *  ABE_WRITE_EVENT_GENERATOR
 *
 *  Parameter  :
 *	e: Event Generation Counter, McPDM, DMIC or default.
 *
 *  Operations :
 *	load the AESS event generator hardware source. Loads the firmware parameters
 *	accordingly. Indicates to the FW which data stream is the most important to preserve
 *	in case all the streams are asynchronous. If the parameter is "default", let the HAL
 *	decide which Event source is the best appropriate based on the opened ports.
 *
 *	When neither the DMIC and the McPDM are activated the AE will have its EVENT generator programmed
 *	with the EVENT_COUNTER. The event counter will be tuned in order to deliver a pulse frequency higher
 *	than 96 kHz. The DPLL output at 100% OPP is MCLK = (32768kHz x6000) = 196.608kHz
 *	The ratio is (MCLK/96000)+(1<<1) = 2050
 *	(1<<1) in order to have the same speed at 50% and 100% OPP (only 15 MSB bits are used at OPP50%)
 *
 *  Return value :
 *	None.
 */
void abe_write_event_generator(abe_event_id e)
{
	abe_uint32 event, selection, counter, start;

	_lock_enter
	_log(id_write_event_generator,e,0,0)

	counter = EVENT_GENERATOR_COUNTER_DEFAULT;
	start = EVENT_GENERATOR_ON;
	abe_current_event_id = e;

	switch (e) {
	case EVENT_MCPDM:
		selection = EVENT_SOURCE_DMA;
		event = ABE_ATC_MCPDMDL_DMA_REQ;
		break;
	case EVENT_DMIC:
		selection = EVENT_SOURCE_DMA;
		event = ABE_ATC_DMIC_DMA_REQ;
		break;
	case EVENT_TIMER:
		selection = EVENT_SOURCE_COUNTER;
		event = 0;
		break;
	case EVENT_McBSP:
		selection = EVENT_SOURCE_COUNTER;
		event = 0;
		break;
	case EVENT_McASP:
		selection = EVENT_SOURCE_COUNTER;
		event = 0;
		break;
	case EVENT_SLIMBUS:
		selection = EVENT_SOURCE_COUNTER;
		event = 0;
		break;
	case EVENT_44100:
		selection = EVENT_SOURCE_COUNTER;
		event = 0;
		counter = EVENT_GENERATOR_COUNTER_44100;
		break;
	case EVENT_DEFAULT:
		selection = EVENT_SOURCE_COUNTER;
		event = 0;
		break;
	default:
		abe_dbg_param |= ERR_API;
		abe_dbg_error_log(ABE_BLOCK_COPY_ERR);
	}

	abe_block_copy(COPY_FROM_HOST_TO_ABE, ABE_ATC, EVENT_GENERATOR_COUNTER, &counter, 4);
	abe_block_copy(COPY_FROM_HOST_TO_ABE, ABE_ATC, EVENT_SOURCE_SELECTION, &selection, 4);
	abe_block_copy(COPY_FROM_HOST_TO_ABE, ABE_ATC, EVENT_GENERATOR_START, &start, 4);
	abe_block_copy(COPY_FROM_HOST_TO_ABE, ABE_ATC, AUDIO_ENGINE_SCHEDULER, &event, 4);

	_lock_exit
}

/**
*  abe_read_use_case_opp() description for void abe_read_use_case_opp().
*
*  Operations : returns the expected min OPP for a given use_case list
*
*  Parameter  : No parameter
* @param
*
* @pre	  no pre-condition
*
* @post
*
* @return	error code
*
* @see
*/
void abe_read_use_case_opp(abe_use_case_id *u, abe_opp_t *o)
{
	abe_uint32 opp, i;
	abe_use_case_id *ptr = u;

	#define MAX_READ_USE_CASE_OPP 10	/* there is no reason to have more use_cases */
	#define OPP_25	1
	#define OPP_50	2
	#define OPP_100 4

	_lock_enter
	_log(id_read_use_case_opp,(abe_uint32)(u),(abe_uint32)u>>8,(abe_uint32)u>>16)

	opp = i = 0;
	do {
		/* check for pointer errors */
		if (i > MAX_READ_USE_CASE_OPP) {
			abe_dbg_param |= ERR_API;
			abe_dbg_error_log(ABE_READ_USE_CASE_OPP_ERR);
			break;
		}

		/* check for end_of_list */
		if (*ptr <= 0)
			break;

		/* OPP selection based on current firmware implementation */
		switch (*ptr) {
		case ABE_AUDIO_PLAYER_ON_HEADSET_OR_EARPHONE:
			opp |= OPP_25;
			break;
		case ABE_DRIFT_MANAGEMENT_FOR_AUDIO_PLAYER:
			opp |= OPP_100;
			break;
		case ABE_DRIFT_MANAGEMENT_FOR_VOICE_CALL:
			opp |= OPP_100;
			break;
		case ABE_VOICE_CALL_ON_HEADSET_OR_EARPHONE_OR_BT:
			opp |= OPP_50;
			break;
		case ABE_MULTIMEDIA_AUDIO_RECORDER:
			opp |= OPP_50;
			break;
		case ABE_VIBRATOR_OR_HAPTICS:
			opp |= OPP_100;
			break;
		case ABE_VOICE_CALL_ON_HANDS_FREE_SPEAKER:
			opp |= OPP_100;
			break;
		case ABE_RINGER_TONES:
			opp |= OPP_100;
			break;
		case ABE_VOICE_CALL_WITH_EARPHONE_ACTIVE_NOISE_CANCELLER:
			opp |= OPP_100;
			break;
		default:
			break;
		}

		i++;
		ptr++;
	} while (*ptr != 0);

	if (opp & OPP_100)
		*o = ABE_OPP100;
	else if (opp & OPP_50)
		*o = ABE_OPP50;
	else
		*o = ABE_OPP25;

	_lock_exit
}



/*
 *  ABE_SET_OPP_PROCESSING
 *
 *  Parameter  :
 *	New processing network and OPP:
 *	  0: Ultra Lowest power consumption audio player (no post-processing, no mixer)
 *	  1: OPP 25% (simple multimedia features, including low-power player)
 *	  2: OPP 50% (multimedia and voice calls)
 *	  3: OPP100% (EANC, multimedia complex use-cases)
 *
 *  Operations :
 *	Rearranges the FW task network to the corresponding OPP list of features.
 *	The corresponding AE ports are supposed to be set/reset accordingly before this switch.
 *
 *  Return value :
 *	error code when the new OPP do not corresponds the list of activated features
 */
void abe_set_opp_processing(abe_opp_t opp)
{
	abe_uint32 dOppMode32, sio_desc_address;
	ABE_SIODescriptor desc;

	_lock_enter
	_log(id_set_opp_processing,opp,0,0)

	switch(opp){
	case ABE_OPP25:
		/* OPP25% */
		dOppMode32 = DOPPMODE32_OPP25;
		break;
	case ABE_OPP50:
		/* OPP50% */
		dOppMode32 = DOPPMODE32_OPP50;
		break;
	default:
		abe_dbg_param |= ERR_API;
		abe_dbg_error_log(ABE_BLOCK_COPY_ERR);
		case ABE_OPP100:
		/* OPP100% */
		dOppMode32 = DOPPMODE32_OPP100;
		break;
	}

	/* Write Multiframe inside DMEM */
	abe_block_copy(COPY_FROM_HOST_TO_ABE, ABE_DMEM,
		D_maxTaskBytesInSlot_ADDR, &dOppMode32, sizeof(abe_uint32));

	sio_desc_address = dmem_port_descriptors + (MM_DL_PORT * sizeof(ABE_SIODescriptor));
	abe_block_copy(COPY_FROM_ABE_TO_HOST, ABE_DMEM, sio_desc_address,
					(abe_uint32*)&desc, sizeof (desc));
	if (dOppMode32 == DOPPMODE32_OPP100)
		desc.smem_addr1 = smem_mm_dl_opp100; /* ASRC input buffer, size 40 */
	else
		desc.smem_addr1 = smem_mm_dl_opp25; /* at OPP 25/50 or without ASRC  */
	abe_block_copy(COPY_FROM_HOST_TO_ABE, ABE_DMEM, sio_desc_address,
					(abe_uint32*)&desc, sizeof (desc));

	_lock_exit

}

/*
 *  ABE_SET_PING_PONG_BUFFER
 *
 *  Parameter  :
 *	Port_ID :
 *	New data
 *
 *  Operations :
 *	Updates the next ping-pong buffer with "size" bytes copied from the
 *	host processor. This API notifies the FW that the data transfer is done.
 */
void abe_set_ping_pong_buffer(abe_port_id port, abe_uint32 n_bytes)
{
	abe_uint32 sio_pp_desc_address, struct_offset, *src, n_samples, datasize, base_and_size;
	ABE_SPingPongDescriptor desc_pp;

	_lock_enter
	_log(id_set_ping_pong_buffer,port,n_bytes,n_bytes>>8)

	/* ping_pong is only supported on MM_DL */
	if (port != MM_DL_PORT) {
	    abe_dbg_param |= ERR_API;
	    abe_dbg_error_log(ABE_PARAMETER_ERROR);
	}
	/* translates the number of bytes in samples */
	/* data size in DMEM words */
	datasize = abe_dma_port_iter_factor(&((abe_port[port]).format));
	/* data size in bytes */
	datasize = datasize << 2;
	n_samples = n_bytes / datasize;

	abe_block_copy(COPY_FROM_ABE_TO_HOST, ABE_DMEM, D_PingPongDesc_ADDR,
					(abe_uint32 *)&desc_pp, sizeof(desc_pp));

	/*
	 * read the port SIO descriptor and extract the current pointer
	 * address  after reading the counter
	 */
	if ((desc_pp.counter & 0x1) == 0) {
		struct_offset = (abe_uint32)&(desc_pp.nextbuff0_BaseAddr) -
							(abe_uint32)&(desc_pp);
		base_and_size = desc_pp.nextbuff0_BaseAddr;
	} else {
		struct_offset = (abe_uint32)&(desc_pp.nextbuff1_BaseAddr) -
							(abe_uint32)&(desc_pp);
		base_and_size = desc_pp.nextbuff1_BaseAddr;
	}

	base_and_size = (base_and_size & 0xFFFFL) + ((abe_uint32)n_samples << 16);

	sio_pp_desc_address = D_PingPongDesc_ADDR + struct_offset;
	src = &base_and_size;
	abe_block_copy(COPY_FROM_HOST_TO_ABE, ABE_DMEM, sio_pp_desc_address,
				(abe_uint32 *)&base_and_size, sizeof(abe_uint32));

	_lock_exit
}

/*
 *  ABE_READ_NEXT_PING_PONG_BUFFER
 *
 *  Parameter  :
 *	Port_ID :
 *	Returned address to the next buffer (byte offset from DMEM start)
 *
 *  Operations :
 *	Tell the next base address of the next ping_pong Buffer and its size
 *
 *
 */
void abe_read_next_ping_pong_buffer(abe_port_id port, abe_uint32 *p, abe_uint32 *n)
{
	abe_uint32 sio_pp_desc_address;
	ABE_SPingPongDescriptor desc_pp;

	_lock_enter
	_log(id_read_next_ping_pong_buffer,port,0,0)

	/* ping_pong is only supported on MM_DL */
	if (port != MM_DL_PORT) {
		abe_dbg_param |= ERR_API;
		abe_dbg_error_log(ABE_PARAMETER_ERROR);
	}

	/* read the port SIO descriptor and extract the current pointer address  after reading the counter */
	sio_pp_desc_address = D_PingPongDesc_ADDR;
	abe_block_copy(COPY_FROM_ABE_TO_HOST, ABE_DMEM, sio_pp_desc_address, (abe_uint32*)&desc_pp, sizeof(ABE_SPingPongDescriptor));

	if ((desc_pp.counter & 0x1) == 0) {
		(*p) = desc_pp.nextbuff0_BaseAddr;
	} else {
		(*p) = desc_pp.nextbuff1_BaseAddr;
	}

	/* translates the number of samples in bytes */
	(*n) = abe_size_pingpong;

	_lock_exit
}

/*
 *  ABE_INIT_PING_PONG_BUFFER
 *
 *  Parameter  :
 *	size of the ping pong
 *	number of buffers (2 = ping/pong)
 *	returned address of the ping-pong list of base address (byte offset from DMEM start)
 *
 *  Operations :
 *	Computes the base address of the ping_pong buffers
 *
 */
void abe_init_ping_pong_buffer(abe_port_id id, abe_uint32 size_bytes, abe_uint32 n_buffers, abe_uint32 *p)
{
	abe_uint32 i, dmem_addr;

	_lock_enter
	_log(id_init_ping_pong_buffer,id,size_bytes,n_buffers)

	/* ping_pong is supported in 2 buffers configuration right now but FW is ready for ping/pong/pung/pang... */
	if (id != MM_DL_PORT || n_buffers > MAX_PINGPONG_BUFFERS) {
		abe_dbg_param |= ERR_API;
		abe_dbg_error_log(ABE_PARAMETER_ERROR);
	}

	for (i = 0; i < n_buffers; i++) {
		dmem_addr = dmem_ping_pong_buffer + (i * size_bytes);
		abe_base_address_pingpong [i] = dmem_addr;	 /* base addresses of the ping pong buffers in U8 unit */
	}

	abe_size_pingpong = size_bytes;	/* global data */
	*p = (abe_uint32)dmem_ping_pong_buffer;
	_lock_exit
}

/*
 *  ABE_PLUG_SUBROUTINE
 *
 *  Parameter :
 *  id: returned sequence index after plugging a new subroutine
 *  f : subroutine address to be inserted
 *  n : number of parameters of this subroutine
 *
 *  Returned value : error code
 *
 *  Operations : register a list of subroutines for call-back purpose
 *
 */
void abe_plug_subroutine(abe_uint32 *id, abe_subroutine2 f, abe_uint32 n, abe_uint32* params)
{
	_lock_enter
	_log(id_plug_subroutine,(abe_uint32)(*id),(abe_uint32)f,n)
	abe_add_subroutine((abe_uint32 *)id, (abe_subroutine2)f,
					(abe_uint32)n, (abe_uint32*)params);
	_lock_exit
}

/*
 *  ABE_SET_SEQUENCE_TIME_ACCURACY
 *
 *  Parameter  :
 *	patch bit field used to guarantee the code compatibility without conditionnal compilation
 *	Sequence index
 *
 *  Operations : two counters are implemented in the firmware:
 *  -	one "fast" counter, generating an IRQ to the HAL for sequences scheduling, the rate is in the range 1ms .. 100ms
 *  -	one "slow" counter, generating an IRQ to the HAL for the management of ASRC drift, the rate is in the range 1s .. 100s
 *
 *  Return value :
 *	None.
 */
void abe_set_sequence_time_accuracy(abe_micros_t fast, abe_micros_t slow)
{
	abe_uint32 data;

	_lock_enter
	_log(id_set_sequence_time_accuracy,fast,slow,0)

	data = minimum(MAX_UINT16, (abe_uint32) fast / FW_SCHED_LOOP_FREQ_DIV1000);
	abe_block_copy(COPY_FROM_HOST_TO_ABE, ABE_DMEM, D_fastCounter_ADDR,
					(abe_uint32 *)&data, sizeof (data));

	data = minimum(MAX_UINT16, (abe_uint32) slow / FW_SCHED_LOOP_FREQ_DIV1000);
	abe_block_copy(COPY_FROM_HOST_TO_ABE, ABE_DMEM, D_slowCounter_ADDR,
				(abe_uint32 *)&data, sizeof (data));

	_lock_exit
}

/*
 *  ABE_RESET_PORT
 *
 *  Parameters :
 *  id: port name
 *
 *  Returned value : error code
 *
 *  Operations : stop the port activity and reload default parameters on the associated processing features.
 *	 Clears the internal AE buffers.
 *
 */
void abe_reset_port(abe_port_id id)
{
	_lock_enter
	_log(id_reset_port,id,0,0)
	abe_port[id] = ((abe_port_t *) abe_port_init) [id];
	_lock_exit
}

/*
 *  ABE_READ_REMAINING_DATA
 *
 *  Parameter  :
 *	Port_ID :
 *	size : pointer to the remaining number of 32bits words
 *
 *  Operations :
 *	computes the remaining amount of data in the buffer.
 *
 *  Return value :
 *	error code
 */
void abe_read_remaining_data(abe_port_id port, abe_uint32 *n)
{
	abe_uint32 sio_pp_desc_address;
	ABE_SPingPongDescriptor desc_pp;

	_lock_enter
	_log(id_read_remaining_data,port,0,0)

	/*
	 * read the port SIO descriptor and extract the
	 * current pointer address  after reading the counter
	 */
	sio_pp_desc_address = D_PingPongDesc_ADDR;
	abe_block_copy (COPY_FROM_ABE_TO_HOST, ABE_DMEM, sio_pp_desc_address,
		(abe_uint32*)&desc_pp, sizeof(ABE_SPingPongDescriptor));
	(*n) = desc_pp.workbuff_Samples;

	_lock_exit
}

/*
 *  ABE_DISABLE_DATA_TRANSFER
 *
 *  Parameter  :
 *	p: port indentifier
 *
 *  Operations :
 *	disables the ATC descriptor and stop IO/port activities
 *	disable the IO task (@f = 0)
 *	clear ATC DMEM buffer, ATC enabled
 *
 *  Return value :
 *	None.
 */
void abe_disable_data_transfer(abe_port_id id)
{
	_lock_enter
	_log(id_disable_data_transfer,id,0,0)

	/* local host variable status= "port is running" */
	abe_port[id].status = IDLE_P;
	/* disable DMA requests */
	abe_disable_dma_request(id);
	/* disable ATC transfers */
	abe_init_atc(id);
	abe_clean_temporary_buffers(id);

	_lock_exit

}

/*
 *  ABE_ENABLE_DATA_TRANSFER
 *
 *  Parameter  :
 *	p: port indentifier
 *
 *  Operations :
 *	enables the ATC descriptor
 *	reset ATC pointers
 *	enable the IO task (@f <> 0)
 *
 *  Return value :
 *	None.
 */
void abe_enable_data_transfer(abe_port_id id)
{
	abe_port_protocol_t *protocol;
	abe_data_format_t format;

	_lock_enter
	_log(id_enable_data_transfer,id,0,0)

	abe_clean_temporary_buffers(id);

	if (id == PDM_UL_PORT) {
		/* initializes the ABE ATC descriptors in DMEM - MCPDM_UL */
		protocol = &(abe_port[PDM_UL_PORT].protocol);
		format = abe_port[PDM_UL_PORT].format;
		abe_init_atc(PDM_UL_PORT);
		abe_init_io_tasks(PDM_UL_PORT, &format, protocol);
	}
	if (id == PDM_DL_PORT) {
		/* initializes the ABE ATC descriptors in DMEM - MCPDM_DL */
		protocol = &(abe_port[PDM_DL_PORT].protocol);
		format = abe_port[PDM_DL_PORT].format;
		abe_init_atc(PDM_DL_PORT);
		abe_init_io_tasks(PDM_DL_PORT, &format, protocol);
	}
	if (id == DMIC_PORT) {
		/* one DMIC port enabled = all DMICs enabled,
		 * since there is a single DMIC path for all DMICs */
		protocol = &(abe_port[DMIC_PORT].protocol);
		format = abe_port[DMIC_PORT].format;
		abe_init_atc(DMIC_PORT);
		abe_init_io_tasks(DMIC_PORT, &format, protocol);
	}

	/* local host variable status= "port is running" */
	abe_port[id].status = RUN_P;
	/* enable DMA requests */
	abe_enable_dma_request(id);
	_lock_exit
}

/*
 *  ABE_SET_DMIC_FILTER
 *
 *  Parameter  :
 *	DMIC decimation ratio : 16/25/32/40
 *
 *  Operations :
 *	Loads in CMEM a specific list of coefficients depending on the DMIC sampling
 *	frequency (2.4MHz or 3.84MHz). This table compensates the DMIC decimator roll-off at 20kHz.
 *	The default table is loaded with the DMIC 2.4MHz recommended configuration.
 *
 *  Return value :
 *	None.
 */
void abe_set_dmic_filter(abe_dmic_ratio_t d)
{
	abe_int32 *src;

	_lock_enter
	_log(id_set_dmic_filter,d,0,0)

	switch(d) {
	case ABE_DEC16:
		src = (abe_int32 *)abe_dmic_16;
		break;
	case ABE_DEC25:
		src = (abe_int32 *) abe_dmic_25;
		break;
	case ABE_DEC32:
		src = (abe_int32 *) abe_dmic_32;
		break;
	default:
	case ABE_DEC40:
		src = (abe_int32 *) abe_dmic_40;
		break;
	}
	abe_block_copy(COPY_FROM_HOST_TO_ABE, ABE_CMEM,
		C_98_48_LP_Coefs_ADDR,
		(abe_uint32 *)src, C_98_48_LP_Coefs_sizeof << 2);
	_lock_exit
}

/**
* @fn abe_connect_cbpr_dmareq_port()
*
*  Operations : enables the data echange between a DMA and the ABE through the
*	CBPr registers of AESS.
*
*   Parameters :
*   id: port name
*   f : desired data format
*   d : desired dma_request line (0..7)
*   a : returned pointer to the base address of the CBPr register and number of
*	samples to exchange during a DMA_request.
*
* @see	ABE_API.h
*/
void abe_connect_cbpr_dmareq_port(abe_port_id id, abe_data_format_t *f, abe_uint32 d, abe_dma_t *returned_dma_t)
{
	_lock_enter
	_log(id_connect_cbpr_dmareq_port,id,f->f,f->samp_format)

	abe_port[id] = ((abe_port_t *)abe_port_init)[id];

	abe_port[id].format = *f;
	abe_port[id].protocol.protocol_switch = DMAREQ_PORT_PROT;
	abe_port[id].protocol.p.prot_dmareq.iter = abe_dma_port_iteration(f);
	abe_port[id].protocol.p.prot_dmareq.dma_addr = ABE_DMASTATUS_RAW;
	abe_port[id].protocol.p.prot_dmareq.dma_data = (1 << d);

	abe_port[id].status = RUN_P;

	/* load the micro-task parameters */
	abe_init_io_tasks(id, &((abe_port [id]).format), &((abe_port [id]).protocol));

	/* load the dma_t with physical information from AE memory mapping */
	abe_init_dma_t(id, &((abe_port [id]).protocol));

	/* load the ATC descriptors - disabled */
	abe_init_atc(id);

	/* return the dma pointer address */
	abe_read_port_address(id, returned_dma_t);

	_lock_exit
}

/**
* @fn abe_connect_dmareq_ping_pong_port()
*
*  Operations : enables the data echanges between a DMA and a direct access to
*   the DMEM memory of ABE. On each dma_request activation the DMA will exchange
*   "s" bytes and switch to the "pong" buffer for a new buffer exchange.
*
*    Parameters :
*    id: port name
*    f : desired data format
*    d : desired dma_request line (0..7)
*    s : half-buffer (ping) size
*
*    a : returned pointer to the base address of the ping-pong buffer and number of samples to exchange during a DMA_request.
*
* @see	ABE_API.h
*/
void abe_connect_dmareq_ping_pong_port(abe_port_id id, abe_data_format_t *f, abe_uint32 d, abe_uint32 s, abe_dma_t *returned_dma_t)
{
	abe_dma_t dma1;

	_lock_enter
	_log(id_connect_dmareq_ping_pong_port,id,f->f,f->samp_format)

	/* ping_pong is only supported on MM_DL */
	if (id != MM_DL_PORT)
	{
	    abe_dbg_param |= ERR_API;
	    abe_dbg_error_log(ABE_PARAMETER_ERROR);
	}

	/* declare PP buffer and prepare the returned dma_t */
	abe_init_ping_pong_buffer(MM_DL_PORT, s, 2, (abe_uint32 *)&(returned_dma_t->data));

	abe_port[id] = ((abe_port_t *) abe_port_init) [id];

	(abe_port[id]).format = (*f);
	(abe_port[id]).protocol.protocol_switch = PINGPONG_PORT_PROT;
	(abe_port[id]).protocol.p.prot_pingpong.buf_addr = dmem_ping_pong_buffer;
	(abe_port[id]).protocol.p.prot_pingpong.buf_size = s;
	(abe_port[id]).protocol.p.prot_pingpong.irq_addr = ABE_DMASTATUS_RAW;
	(abe_port[id]).protocol.p.prot_pingpong.irq_data = (1 << d);

	abe_port [id].status = RUN_P;

	/* load the micro-task parameters DESC_IO_PP */
	abe_init_io_tasks(id, &((abe_port [id]).format), &((abe_port [id]).protocol));

	/* load the dma_t with physical information from AE memory mapping */
	abe_init_dma_t(id, &((abe_port [id]).protocol));

	dma1.data = (abe_uint32 *)(abe_port [id].dma.data + ABE_DMEM_BASE_ADDRESS_L3);
	dma1.iter = abe_port [id].dma.iter;
	(*returned_dma_t) = dma1;

	_lock_exit
}

/**
* @fn abe_connect_irq_ping_pong_port()
*
*  Operations : enables the data echanges between a direct access to the DMEM
*	memory of ABE using cache flush. On each IRQ activation a subroutine
*	registered with "abe_plug_subroutine" will be called. This subroutine
*	will generate an amount of samples, send them to DMEM memory and call
*	"abe_set_ping_pong_buffer" to notify the new amount of samples in the
*	pong buffer.
*
*    Parameters :
*	id: port name
*	f : desired data format
*	I : index of the call-back subroutine to call
*	s : half-buffer (ping) size
*
*	p: returned base address of the first (ping) buffer)
*
* @see	ABE_API.h
*/
void abe_connect_irq_ping_pong_port(abe_port_id id, abe_data_format_t *f,
			abe_uint32 subroutine_id, abe_uint32 size,
					abe_uint32 *sink, abe_uint32 dsp_mcu_flag)
{
	_lock_enter
	_log(id_connect_irq_ping_pong_port,id,f->f,f->samp_format)

	/* ping_pong is only supported on MM_DL */
	if (id != MM_DL_PORT) {
		abe_dbg_param |= ERR_API;
		abe_dbg_error_log(ABE_PARAMETER_ERROR);
	}

	abe_port[id] = ((abe_port_t *) abe_port_init)[id];
	(abe_port[id]).format = (*f);
	(abe_port[id]).protocol.protocol_switch = PINGPONG_PORT_PROT;
	(abe_port[id]).protocol.p.prot_pingpong.buf_addr = dmem_ping_pong_buffer;
	(abe_port[id]).protocol.p.prot_pingpong.buf_size = size;
	(abe_port[id]).protocol.p.prot_pingpong.irq_data = (1);

	abe_init_ping_pong_buffer(MM_DL_PORT, size, 2, sink);

	if (dsp_mcu_flag == PING_PONG_WITH_MCU_IRQ)
		(abe_port [id]).protocol.p.prot_pingpong.irq_addr = ABE_MCU_IRQSTATUS_RAW;

	if (dsp_mcu_flag == PING_PONG_WITH_DSP_IRQ)
		(abe_port [id]).protocol.p.prot_pingpong.irq_addr = ABE_DSP_IRQSTATUS_RAW;

	abe_port[id].status = RUN_P;

	/* load the micro-task parameters */
	abe_init_io_tasks(id, &((abe_port [id]).format), &((abe_port [id]).protocol));

	/* load the ATC descriptors - disabled */
	abe_init_atc(id);

	(*sink)= (abe_port [id]).protocol.p.prot_pingpong.buf_addr;
	_lock_exit
}

/**
* @fn abe_connect_serial_port()
*
*  Operations : enables the data echanges between a McBSP and an ATC buffer in
*   DMEM. This API is used connect 48kHz McBSP streams to MM_DL and 8/16kHz
*   voice streams to VX_UL, VX_DL, BT_VX_UL, BT_VX_DL. It abstracts the
*   abe_write_port API.
*
*   Parameters :
*    id: port name
*    f : data format
*    i : peripheral ID (McBSP #1, #2, #3)
*
* @see	ABE_API.h
*/
void abe_connect_serial_port(abe_port_id id, abe_data_format_t *f, abe_mcbsp_id mcbsp_id)
{
	abe_use_case_id UC_NULL[] = {(abe_use_case_id)0};
	abe_opp_t OPP;
	abe_hw_config_init_t CONFIG;

	_lock_enter
	_log(id_connect_serial_port,id,f->samp_format,mcbsp_id)

	abe_port [id] = ((abe_port_t *) abe_port_init) [id];
	(abe_port [id]).format = (*f);
	(abe_port [id]).protocol.protocol_switch = SERIAL_PORT_PROT;
	/* McBSP peripheral connected to ATC */
	(abe_port [id]).protocol.p.prot_serial.desc_addr = mcbsp_id*ATC_SIZE;

	abe_read_hardware_configuration(UC_NULL, &OPP, &CONFIG);	/* check the iteration of ATC */

	(abe_port[id]).protocol.p.prot_serial.iter = abe_dma_port_iter_factor(f);

	abe_port[id].status = RUN_P;

	/* load the micro-task parameters */
	abe_init_io_tasks(id, &((abe_port[id]).format), &((abe_port[id]).protocol));
	abe_init_atc(id); /* load the ATC descriptors - disabled */

	_lock_exit
}

/*
 *  ABE_READ_PORT_DESCRIPTOR
 *
 *  Parameter  :
 *	id: port name
 *	f : input pointer to the data format
 *	p : input pointer to the protocol description
 *	dma : output pointer to the DMA iteration and data destination pointer :
 *
 *  Operations :
 *	returns the port parameters from the HAL internal buffer.
 *
 *  Return value :
 *	error code in case the Port_id is not compatible with the current OPP value
 */
void abe_read_port_descriptor(abe_port_id port, abe_data_format_t *f, abe_port_protocol_t *p)
{
	(*f)	= (abe_port[port]).format;
	(*p)	= (abe_port[port]).protocol;
}

/*
 *  ABE_READ_APS_ENERGY
 *
 *  Parameter  :
 *	Port_ID : port ID supporting APS
 *	APS data struct pointer
 *
 *  Operations :
 *	Returns the estimated amount of energy
 *
 *  Return value :
 *	error code when the Port is not activated.
 */
void abe_read_aps_energy(abe_port_id *p, abe_gain_t *a)
{
	just_to_avoid_the_many_warnings_abe_port_id = *p;
	just_to_avoid_the_many_warnings_abe_gain_t = *a;
}

/**
* @fn abe_connect_slimbus_port()
*
*  Operations : enables the data echanges between 1/2 SB and an ATC buffers in
*   DMEM.
*
*   Parameters :
*    id: port name
*    f : data format
*    i : peripheral ID (McBSP #1, #2, #3)
*    j : peripheral ID (McBSP #1, #2, #3)
*
* @see	      ABE_API.h
*/
void abe_connect_slimbus_port(abe_port_id id, abe_data_format_t *f,
			abe_slimbus_id sb_port1, abe_slimbus_id sb_port2)
{
	abe_use_case_id UC_NULL[] = {(abe_use_case_id)0};
	abe_opp_t OPP;
	abe_hw_config_init_t CONFIG;
	abe_uint32 iter;

	_lock_enter
	_log(id_connect_slimbus_port,id,f->samp_format,sb_port2)

	abe_port[id] = ((abe_port_t *) abe_port_init) [id];
	(abe_port[id]).format = (*f);
	(abe_port[id]).protocol.protocol_switch = SLIMBUS_PORT_PROT;
	/* SB1 peripheral connected to ATC */
	(abe_port[id]).protocol.p.prot_slimbus.desc_addr1= sb_port1*ATC_SIZE;
	/* SB2 peripheral connected to ATC */
	(abe_port[id]).protocol.p.prot_slimbus.desc_addr2= sb_port2*ATC_SIZE;

	/* check the iteration of ATC */
	abe_read_hardware_configuration(UC_NULL, &OPP, &CONFIG);
	iter = CONFIG.SLIMBUS_DCT_FIFO_SETUP_REG__SB_THRESHOLD;
	/* SLIMBUS iter should be 1 */
	(abe_port[id]).protocol.p.prot_serial.iter = iter;

	abe_port[id].status = RUN_P;
	/* load the micro-task parameters */
	abe_init_io_tasks(id, &((abe_port [id]).format), &((abe_port [id]).protocol));
	/* load the ATC descriptors - disabled */
	abe_init_atc(id);

	_lock_exit
}


/**
* @fn abe_connect_tdm_port()
*
*  Operations : enables the data echanges between TDM McBSP ATC buffers in
*   DMEM and 1/2 SMEM buffers
*
*   Parameters :
*    id: port name
*    f : data format
*    i : peripheral ID (McBSP #1, #2, #3)
*    j : peripheral ID (McBSP #1, #2, #3)
*
* @see	      ABE_API.h
*/

void abe_connect_tdm_port(abe_port_id id, abe_data_format_t *f, abe_mcbsp_id mcbsp_id)
{
	abe_use_case_id UC_NULL[] = {(abe_use_case_id)0};
	abe_opp_t OPP;
	abe_hw_config_init_t CONFIG;
	abe_uint32 iter;

	_lock_enter
	_log(id_connect_tdm_port,id,f->samp_format,mcbsp_id)

	abe_port [id] = ((abe_port_t *) abe_port_init) [id];
	(abe_port [id]).format = (*f);
	(abe_port [id]).protocol.protocol_switch = TDM_SERIAL_PORT_PROT;
	/* McBSP peripheral connected to ATC */
	(abe_port [id]).protocol.p.prot_serial.desc_addr = mcbsp_id*ATC_SIZE;

	/* check the iteration of ATC */
	abe_read_hardware_configuration (UC_NULL, &OPP,  &CONFIG);
	if (abe_port[id].protocol.direction == ABE_ATC_DIRECTION_IN)
		iter = CONFIG.MCBSP_THRSH1_REG_REG__RTHRESHOLD;
	else
		iter = CONFIG.MCBSP_THRSH2_REG_REG__XTHRESHOLD;
	/* McBSP iter should be 1 */
	(abe_port[id]).protocol.p.prot_serial.iter = iter;

	abe_port[id].status = RUN_P;
	/* load the micro-task parameters */
	abe_init_io_tasks(id, &((abe_port [id]).format), &((abe_port [id]).protocol));
	/* load the ATC descriptors - disabled */
	abe_init_atc(id);
	_lock_exit
}

/*
 *  ABE_READ_PORT_ADDRESS
 *
 *  Parameter  :
 *	dma : output pointer to the DMA iteration and data destination pointer
 *
 *  Operations :
 *	This API returns the address of the DMA register used on this audio port.
 *	Depending on the protocol being used, adds the base address offset L3 (DMA) or MPU (ARM)
 *
 *  Return value :
 */
void abe_read_port_address(abe_port_id port, abe_dma_t *dma2)
{
	abe_dma_t_offset dma1;
	abe_uint32 protocol_switch;

	_lock_enter
	_log(id_read_port_address,port,0,0)

	dma1 = (abe_port[port]).dma;
	protocol_switch = abe_port[port].protocol.protocol_switch;

	switch (protocol_switch) {
	case PINGPONG_PORT_PROT:
		/* return the base address of the ping buffer in L3 and L4 spaces */
		(*dma2).data = (void *)(dma1.data + ABE_DMEM_BASE_ADDRESS_L3);
		(*dma2).l3_dmem = (void *)(dma1.data + ABE_DMEM_BASE_ADDRESS_L3);
		(*dma2).l4_dmem = (void *)(dma1.data + ABE_DMEM_BASE_ADDRESS_L4);
		break;
	case DMAREQ_PORT_PROT:
		/* return the CBPr(L3), DMEM(L3), DMEM(L4) address */
		(*dma2).data = (void *)(dma1.data + ABE_ATC_BASE_ADDRESS_L3);
		(*dma2).l3_dmem =
			(void *)((abe_port[port]).protocol.p.prot_dmareq.buf_addr +
					ABE_DMEM_BASE_ADDRESS_L3);
		(*dma2).l4_dmem = (void *)((abe_port[port]).protocol.p.prot_dmareq.buf_addr + ABE_DMEM_BASE_ADDRESS_L4);
		break;
	default:
		break;
	}

	(*dma2).iter  = (dma1.iter);

	_lock_exit
}

/*
 *  ABE_WRITE_EQUALIZER
 *
 *  Parameter  :
 *	Id  : name of the equalizer
 *	Param : equalizer coefficients
 *
 *  Operations :
 *	Load the coefficients in CMEM. This API can be called when the corresponding equalizer
 *	is not activated. After reloading the firmware the default coefficients corresponds to
 *	"no equalizer feature". Loading all coefficients with zeroes disables the feature.
 *
 *  Return value :
 *	None.
 */
void abe_write_equalizer(abe_equ_id id, abe_equ_t *param)
{
	abe_uint32 eq_offset, length, *src;

	_lock_enter
	_log(id_write_equalizer,id,0,0)

	switch(id) {
	default:
	case EQ1:
		eq_offset = C_DL1_Coefs_ADDR;
		break;
	case EQ2L:
		eq_offset = C_DL2_L_Coefs_ADDR;
		break;
	case EQ2R:
		eq_offset = C_DL2_R_Coefs_ADDR;
		break;
	case EQSDT:
		eq_offset = C_SDT_Coefs_ADDR;
		break;
	case EQMIC:
		eq_offset = C_98_48_LP_Coefs_ADDR;
		break;
	case APS1:
		eq_offset = C_APS_DL1_coeffs1_ADDR;
		break;
	case APS2L:
		eq_offset = C_APS_DL2_L_coeffs1_ADDR;
		break;
	case APS2R:
		eq_offset = C_APS_DL2_R_coeffs1_ADDR;
		break;
	}

	length = param->equ_length;
	src = (abe_uint32 *)((param->coef).type1);

	eq_offset <<=2;	/* translate in bytes */
	length <<=2;	/* translate in bytes */
	abe_block_copy(COPY_FROM_HOST_TO_ABE, ABE_CMEM, eq_offset, src, length);
	_lock_exit
}

/*
 * ABE_SET_ASRC_DRIFT_CONTROL
 *
 *  Parameter  :
 *	Id  : name of the asrc
 *	f: flag which enables (1) the automatic computation of drift parameters
 *
 *  Operations :
 *	When an audio port is connected to an hardware peripheral (MM_DL connected to a McBSP for
 *	example), the drift compensation can be managed in "forced mode" (f=0) or "adaptive mode"
 *	(f=1). In the first case the drift is managed with the usage of the API "abe_write_asrc".
 *	In the second case the firmware will generate on periodic basis an information about the
 *	observed drift, the HAL will reload the drift parameter based on those observations.
 *
 *  Return value :
 *	None.
 */
void abe_set_asrc_drift_control(abe_asrc_id id, abe_uint32 f)
{
	_lock_enter
	_log(id_set_asrc_drift_control,id,f,f>>8)
	_lock_exit
}

/*
 * ABE_WRITE_ASRC
 *
 *  Parameter  :
 *	Id  : name of the asrc
 *	param : drift value t compensate
 *
 *  Operations :
 *	Load the drift coefficients in FW memory. This API can be called when the corresponding
 *	ASRC is not activated. After reloading the firmware the default coefficients corresponds
 *	to "no ASRC activated". Loading the drift value with zero disables the feature.
 *
 *  Return value :
 *	None.
 */
void abe_write_asrc(abe_asrc_id id, abe_drift_t dppm)
{
	_lock_enter
	_log(id_write_asrc,id,dppm,dppm>>8)

#if 0
	abe_int32 dtempvalue, adppm, alpha_current, beta_current, asrc_params;
	abe_int32 atempvalue32[8];
	/*
	 * x = ppm
	 *	 - 1000000/x must be multiple of 16
	 *	 - deltaalpha = round(2^20*x*16/1000000)=round(2^18/5^6*x) on 22 bits. then shifted by 2bits
	 *	 - minusdeltaalpha
	 *	 - oneminusepsilon = 1-deltaalpha/2.
	 *	 ppm = 250
	 *	 - 1000000/250=4000
	 *	 - deltaalpha = 4194.3 ~ 4195 => 0x00418c
	 */
	 /* examples for -6250 ppm */
	 // atempvalue32[0] = 4;		  /* d_constalmost0 */
	 // atempvalue32[1] = -1;		   /* d_driftsign */
	 // atempvalue32[2] = 15;		  /* d_subblock */
	 // atempvalue32[3] = 0x00066668; /* d_deltaalpha */
	 // atempvalue32[4] = 0xfff99998; /* d_minusdeltaalpha */
	 // atempvalue32[5] = 0x003ccccc; /* d_oneminusepsilon */
	 // atempvalue32[6] = 0x00000000; /* d_alphazero */
	 // atempvalue32[7] = 0x00400000; /* d_betaone */

	/* compute new value for the ppm */
	if (dppm > 0){
		atempvalue32[1] = 1;		   /* d_driftsign */
		adppm = dppm;
	} else {
		atempvalue32[1] = -1;		   /* d_driftsign */
		adppm = (-1*dppm);
	}

	dtempvalue =  (adppm << 4) +  adppm - ((adppm * 3481L)/15625L);
	atempvalue32[3] = dtempvalue<<2;
	atempvalue32[4] = (-dtempvalue)<<2;
	atempvalue32[5] = (0x00100000-(dtempvalue/2))<<2;

	switch (id) {
	case ASRC2:		/* asynchronous sample-rate-converter for the uplink voice path */
		alpha_current = C_AlphaCurrent_UL_VX_ADDR;
		beta_current  = C_BetaCurrent_UL_VX_ADDR;
		asrc_params   = D_AsrcVars_UL_VX_ADDR;
		break;
	case ASRC1:		/* asynchronous sample-rate-converter for the downlink voice path */
		alpha_current = C_AlphaCurrent_DL_VX_ADDR;
		beta_current  = C_BetaCurrent_DL_VX_ADDR;
		asrc_params   = D_AsrcVars_DL_VX_ADDR;
		break;
	default:
	case ASRC3:		/* asynchronous sample-rate-converter for the multimedia player */
		alpha_current = C_AlphaCurrent_DL_MM_ADDR;
		beta_current  = C_BetaCurrent_DL_MM_ADDR;
		asrc_params   = D_AsrcVars_DL_MM_ADDR;
		break;
	}

	dtempvalue = 0x00000000;
	abe_block_copy (COPY_FROM_HOST_TO_ABE, ABE_CMEM, alpha_current,(abe_uint32 *)&dtempvalue, 4);
	dtempvalue = 0x00400000;
	abe_block_copy (COPY_FROM_HOST_TO_ABE, ABE_CMEM, beta_current, (abe_uint32 *)&dtempvalue, 4);
	abe_block_copy (COPY_FROM_HOST_TO_ABE, ABE_CMEM, asrc_params , (abe_uint32 *)&atempvalue32, sizeof(atempvalue32));
#endif
	_lock_exit
}

/*
 *  ABE_WRITE_APS
 *
 *  Parameter  :
 *	Id  : name of the aps filter
 *	param : table of filter coefficients
 *
 *  Operations :
 *	Load the filters and thresholds coefficients in FW memory. This API can be called when
 *	the corresponding APS is not activated. After reloading the firmware the default coefficients
 *	corresponds to "no APS activated". Loading all the coefficients value with zero disables
 *	the feature.
 *
 *  Return value :
 *	None.
 */
void abe_write_aps(abe_aps_id id, abe_aps_t *param)
{
	_lock_enter
	_log(id_write_aps,id,0,0)
	_lock_exit
}

/*
 *  ABE_WRITE_MIXER
 *
 *  Parameter  :
 *  Id	: name of the mixer
 *  param : list of input gains of the mixer
 *  p : list of port corresponding to the above gains
 *
 *  Operations :
 *	Load the gain coefficients in FW memory. This API can be called when the corresponding
 *	MIXER is not activated. After reloading the firmware the default coefficients corresponds
 *	to "all input and output mixer's gain in mute state". A mixer is disabled with a network
 *	reconfiguration corresponding to an OPP value.
 *
 *  Return value :
 *	None.
 */
void abe_write_gain(abe_gain_id id, abe_gain_t f_g, abe_ramp_t f_ramp, abe_port_id p)
{
	abe_uint32 lin_g, mixer_target, mixer_offset;
	abe_int32 gain_index;

	_lock_enter
	_log(id_write_gain,id,f_g,p)

	gain_index = ((f_g - min_mdb) / 100);
	gain_index = maximum(gain_index, 0);
	gain_index = minimum(gain_index, sizeof_db2lin_table);

	lin_g = abe_db2lin_table [gain_index];

	switch(id) {
	default:
	case GAINS_DMIC1:
		mixer_offset = dmic1_gains_offset;
		break;
	case GAINS_DMIC2:
		mixer_offset = dmic2_gains_offset;
		break;
	case GAINS_DMIC3:
		mixer_offset = dmic3_gains_offset;
		break;
	case GAINS_AMIC:
		mixer_offset = amic_gains_offset;
		break;
	case GAINS_DL1:
		mixer_offset = dl1_gains_offset;
		break;
	case GAINS_DL2:
		mixer_offset = dl2_gains_offset;
		break;
	case GAINS_SPLIT:
		mixer_offset = splitters_gains_offset;
		break;
	case MIXDL1:
		mixer_offset = mixer_dl1_offset;
		break;
	case MIXDL2:
		mixer_offset = mixer_dl2_offset;
		break;
	case MIXECHO:
		mixer_offset = mixer_echo_offset;
		break;
	case MIXSDT:
		mixer_offset = mixer_sdt_offset;
		break;
	case MIXVXREC:
		mixer_offset = mixer_vxrec_offset;
		break;
	case MIXAUDUL:
		mixer_offset = mixer_audul_offset;
		break;
	}

	mixer_target = (smem_target_gain_base << 1);/* SMEM word32 address */
	mixer_target += mixer_offset;
	mixer_target += p;
	mixer_target <<= 2; /* translate coef address in Bytes */

	/* load the S_G_Target SMEM table */
	abe_block_copy(COPY_FROM_HOST_TO_ABE, ABE_SMEM, mixer_target,
				(abe_uint32*)&lin_g, sizeof(lin_g));
	_lock_exit
}

void abe_write_mixer(abe_mixer_id id, abe_gain_t f_g, abe_ramp_t f_ramp, abe_port_id p)
{
	_lock_enter
	_log(id_write_mixer,id,f_ramp,p)

	abe_write_gain  ((abe_gain_id)id, f_g, f_ramp, p);

	_lock_exit
}

/*
 *  ABE_SET_ROUTER_CONFIGURATION
 *
 *  Parameter  :
 *	Id  : name of the router
 *	Conf : id of the configuration
 *	param : list of output index of the route
 *
 *  Operations :
 *	The uplink router takes its input from DMIC (6 samples), AMIC (2 samples) and
 *	PORT1/2 (2 stereo ports). Each sample will be individually stored in an intermediate
 *	table of 10 elements. The intermediate table is used to route the samples to
 *	three directions : REC1 mixer, 2 EANC DMIC source of filtering and MM recording audio path.
 *	For example, a use case consisting in AMIC used for uplink voice communication, DMIC 0,1,2,3
 *	used for multimedia recording, , DMIC 5 used for EANC filter, DMIC 4 used for the feedback channel,
 *	will be implemented with the following routing table index list :
 *	[3, 2 , 1, 0, 0, 0 (two dummy indexes to data that will not be on MM_UL), 4, 5, 7, 6]
 *  example
 *	abe_set_router_configuration (UPROUTE, UPROUTE_CONFIG_AMIC, abe_router_ul_table_preset[UPROUTE_CONFIG_AMIC]);
 *  Return value :
 *	None.
 */
void abe_set_router_configuration(abe_router_id id, abe_uint32 unused, abe_router_t *param)
{
	_lock_enter
	_log(id_set_router_configuration,id,(abe_uint32)param,(abe_uint32)param>>8)

	abe_block_copy(COPY_FROM_HOST_TO_ABE, ABE_DMEM, D_aUplinkRouting_ADDR,
		(abe_uint32 *)param, D_aUplinkRouting_ADDR_END - D_aUplinkRouting_ADDR + 1);

	_lock_exit
}

/*
 *  ABE_SELECT_DATA_SOURCE
 *
 *  Parameter  :
 *
 *  Operations :
 *
 *  Return value :
 *      None.
 */
void abe_select_data_source (abe_port_id port_id, abe_dl_src_id smem_source)
{
	ABE_SIODescriptor desc;
	abe_uint32 sio_desc_address;

	_lock_enter
	_log(id_select_data_source,port_id,smem_source,smem_source>>8)

	sio_desc_address = dmem_port_descriptors + (port_id * sizeof(ABE_SIODescriptor));
	abe_block_copy(COPY_FROM_ABE_TO_HOST, ABE_DMEM, sio_desc_address,
					(abe_uint32*)&desc, sizeof (desc));
	desc.smem_addr1 = (abe_uint16) smem_source;
	abe_block_copy(COPY_FROM_HOST_TO_ABE, ABE_DMEM, sio_desc_address,
					(abe_uint32*)&desc, sizeof (desc));

	_lock_exit
}


/*
 *  ABE_READ_DEBUG_TRACE
 *
 *  Parameter  :
 *	data destination pointer
 *	max number of data read
 *
 *  Operations :
 *	reads the AE circular data pointer holding pairs of debug data+timestamps, and store
 *	the pairs in linear addressing to the parameter pointer. Stops the copy when the max
 *	parameter is reached or when the FIFO is empty.
 *
 *  Return value :
 *	None.
 */
void abe_read_debug_trace(abe_uint32 *data, abe_uint32 *n)
{
	_lock_enter
	_log(id_select_data_source,0,0,0)

	_lock_exit
}

/*
 *  ABE_CONNECT_DEBUG_TRACE
 *
 *  Parameter  :
 *      pointer to the DMEM trace buffer
 *
 *  Operations :
 *      returns the address and size of the real-time debug trace buffer,
 *	the content of which will vary from one firmware release to an other
 *
 *  Return value :
 *      None.
 */

void abe_connect_debug_trace (abe_dma_t *dma2)
{
	_lock_enter
	_log(id_connect_debug_trace,0,0,0)

	/* return the base address of the ping buffer in L3 and L4 spaces */
	(*dma2).data =    (void *)(D_DEBUG_FIFO_ADDR + ABE_DMEM_BASE_ADDRESS_L3);
	(*dma2).l3_dmem = (void *)(D_DEBUG_FIFO_ADDR + ABE_DMEM_BASE_ADDRESS_L3);
	(*dma2).l4_dmem = (void *)(D_DEBUG_FIFO_ADDR + ABE_DMEM_BASE_ADDRESS_L4);
	(*dma2).iter = D_DEBUG_FIFO_ADDR_END - D_DEBUG_FIFO_ADDR;

	_lock_exit
}


/*
 *  ABE_SET_DEBUG_TRACE
 *
 *  Parameter  :
 *	debug ID from a list to be defined
 *
 *  Operations :
 *	load a mask which filters the debug trace to dedicated types of data
 *
 *  Return value :
 *	None.
 */
void abe_set_debug_trace(abe_dbg_t debug)
{
	_lock_enter
	_log(id_set_debug_trace,0,0,0)

	abe_dbg_mask = debug;

	_lock_exit
}

/*
 *  ABE_REMOTE_DEBUGGER_INTERFACE
 *
 *  Parameter  :
 *
 *  Operations :
 *	interpretation of the UART stream from the remote debugger commands.
 *	The commands consist in setting break points, loading parameter
 *
 *  Return value :
 *	None.
 */
void abe_remote_debugger_interface(abe_uint32 n, abe_uint8 *p)
{
	_lock_enter
	_log(id_remote_debugger_interface,n,0,0)

	_lock_exit

}

/*
 *  ABE_ENABLE_TEST_PATTERN
 *
 *  Parameter:
 *
 *  Operations :
 *
 *  Return value :
 *      None.
 */

void abe_enable_test_pattern (abe_patched_pattern_id smem_id, abe_uint32 on_off)
{
	abe_uint16 dbg_on, dbg_off, idx_patch, task_patch, addr_patch;
	abe_uint32 patch, task32;

	_lock_enter
	_log(id_enable_test_pattern,on_off,smem_id,smem_id>>8)

	switch (smem_id) {
	case DBG_PATCH_AMIC:
		dbg_on = DBG_48K_PATTERN_labelID;
		dbg_off = AMIC_labelID;
		task_patch = C_ABE_FW_TASK_AMIC_SPLIT;
		idx_patch = 1;
		break;
	case DBG_PATCH_DMIC1:
		dbg_on = DBG_48K_PATTERN_labelID;
		dbg_off = DMIC1_labelID;
		task_patch = C_ABE_FW_TASK_DMIC1_SPLIT;
		idx_patch = 1;
		break;
	case DBG_PATCH_DMIC2:
		dbg_on = DBG_48K_PATTERN_labelID;
		dbg_off = DMIC2_labelID;
		task_patch = C_ABE_FW_TASK_DMIC2_SPLIT;
		idx_patch = 1;
		break;
	case DBG_PATCH_DMIC3:
		dbg_on = DBG_48K_PATTERN_labelID;
		dbg_off = DMIC3_labelID;
		task_patch = C_ABE_FW_TASK_DMIC3_SPLIT;
		idx_patch = 1;
		break;
	case DBG_PATCH_VX_REC:
		dbg_on = DBG_48K_PATTERN_labelID;
		dbg_off = VX_REC_labelID;
		task_patch = C_ABE_FW_TASK_VXREC_SPLIT;
		idx_patch = 1;
		break;
	case DBG_PATCH_BT_UL:
		dbg_on = DBG_48K_PATTERN_labelID;
		dbg_off = BT_UL_labelID;
		task_patch = C_ABE_FW_TASK_BT_UL_SPLIT;
		idx_patch = 1;
		break;
	case DBG_PATCH_MM_DL:
		dbg_on = DBG_48K_PATTERN_labelID;
		dbg_off = MM_DL_labelID;
		task_patch = C_ABE_FW_TASK_MM_SPLIT;
		idx_patch = 1;
		break;
	case DBG_PATCH_DL2_EQ:
		dbg_on = DBG_48K_PATTERN_labelID;
		dbg_off = DL2_EQ_labelID;
		task_patch = C_ABE_FW_TASK_DL2_APS_SPLIT;
		idx_patch = 1;
		break;
	case DBG_PATCH_VIBRA:
		dbg_on = DBG_48K_PATTERN_labelID;
		dbg_off = VIBRA_labelID;
		task_patch = C_ABE_FW_TASK_VIBRA_SPLIT;
		idx_patch = 1;
		break;
	case DBG_PATCH_MM_EXT_IN:
		dbg_on = DBG_48K_PATTERN_labelID;
		dbg_off = MM_EXT_IN_labelID;
		task_patch = C_ABE_FW_TASK_MM_EXT_IN_SPLIT;
		idx_patch = 1;
		break;
	case DBG_PATCH_EANC_FBK_Out:
		dbg_on = DBG_48K_PATTERN_labelID;
		dbg_off = EANC_FBK_Out_labelID;
		task_patch = C_ABE_FW_TASK_EANC_FBK_SPLIT;
		idx_patch = 1;
		break;
	case DBG_PATCH_MIC4:
		dbg_on = DBG_48K_PATTERN_labelID;
		dbg_off = MIC4_labelID;
		task_patch = C_ABE_FW_TASK_MIC4_SPLIT;
		idx_patch = 1;
		break;
	case DBG_PATCH_MM_DL_MIXDL1:
		dbg_on = DBG_48K_PATTERN_labelID;
		dbg_off = AMIC_labelID;
		task_patch = C_ABE_FW_TASK_DL1Mixer;
		idx_patch = 1;
		break;
	case DBG_PATCH_MM_DL_MIXDL2:
		dbg_on = DBG_48K_PATTERN_labelID;
		dbg_off = AMIC_labelID;
		task_patch = C_ABE_FW_TASK_DL2Mixer;
		idx_patch = 1;
		break;
	default:
		return;
	}

	patch = (on_off != 0)? dbg_on: dbg_off;
	/* address is on 16bits boundary */
	addr_patch = D_tasksList_ADDR + 16 * task_patch + 2 * idx_patch;
	/* read on 32bits words' boundary */
	abe_block_copy (COPY_FROM_ABE_TO_HOST, ABE_DMEM, addr_patch & (~0x03), &task32, 4);

	if (addr_patch & 0x03)
		task32 = (0x0000FFFFL & task32) | (patch << 16);
	else
		task32 = (0xFFFF0000L & task32) | (0x0000FFFF & patch);

	abe_block_copy (COPY_FROM_HOST_TO_ABE, ABE_DMEM, addr_patch & (~0x03), &task32, 4);

	_lock_exit
}
