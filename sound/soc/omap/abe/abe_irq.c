/*
 * ==========================================================================
 *               Texas Instruments OMAP(TM) Platform Firmware
 * (c) Copyright 2009, Texas Instruments Incorporated.  All Rights Reserved.
 *
 *  Use of this firmware is controlled by the terms and conditions found
 *  in the license agreement under which this firmware has been supplied.
 * ==========================================================================
 */

#include "abe_main.h"

/*
 *  initialize the default values for call-backs to subroutines
 *      - FIFO IRQ call-backs for sequenced tasks
 *      - FIFO IRQ call-backs for audio player/recorders (ping-pong protocols)
 *      - Remote debugger interface
 *      - Error monitoring
 *      - Activity Tracing
 */

/*
 * ABE_IRQ_PING_PONG
 * Parameter  :
 * No parameter
 *
 * Operations :
 * Call the respective subroutine depending on the IRQ FIFO content:
 * APS interrupts : IRQtag_APS to [31:28], APS_IRQs to [27:16], loopCounter to [15:0]
 * SEQ interrupts : IRQtag_COUNT to [31:28], Count_IRQs to [27:16], loopCounter to [15:0]
 * Ping-Pong Interrupts : IRQtag_PP to [31:28], PP_MCU_IRQ to [27:16], loopCounter to [15:0]
 * Check for ping-pong subroutines (low-power players)
 *
 * Return value :
 * None.
 */
void abe_irq_ping_pong(void)
{
	abe_call_subroutine(abe_irq_pingpong_player_id, NOPARAMETER, NOPARAMETER, NOPARAMETER, NOPARAMETER);
}

/*
 * ABE_IRQ_CHECK_FOR_SEQUENCES
 * Parameter  :
 * No parameter
 *
 * Operations :
 * check the active sequence list
 *
 * Return value :
 * None.
 */
void abe_irq_check_for_sequences(abe_uint32 i)
{
}

/*
 * ABE_IRQ_APS
 * Parameter  :
 * No parameter
 *
 * Operations :
 * call the application subroutines that updates the acoustics protection filters
 *
 * Return value :
 * None.
 */
void abe_irq_aps(abe_uint32 aps_info)
{
	abe_call_subroutine(abe_irq_aps_adaptation_id, NOPARAMETER, NOPARAMETER, NOPARAMETER, NOPARAMETER);
}
