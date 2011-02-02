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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 */
#include "abe_main.h"
#include "abe_ref.h"

/*
 *  initialize the default values for call-backs to subroutines
 *      - FIFO IRQ call-backs for sequenced tasks
 *      - FIFO IRQ call-backs for audio player/recorders (ping-pong protocols)
 *      - Remote debugger interface
 *      - Error monitoring
 *      - Activity Tracing
 */

/**
 * abe_irq_ping_pong
 *
 * Call the respective subroutine depending on the IRQ FIFO content:
 * APS interrupts : IRQ_FIFO[31:28] = IRQtag_APS,
 *	IRQ_FIFO[27:16] = APS_IRQs, IRQ_FIFO[15:0] = loopCounter
 * SEQ interrupts : IRQ_FIFO[31:28] = IRQtag_COUNT,
 *	IRQ_FIFO[27:16] = Count_IRQs, IRQ_FIFO[15:0] = loopCounter
 * Ping-Pong Interrupts : IRQ_FIFO[31:28] = IRQtag_PP,
 *	IRQ_FIFO[27:16] = PP_MCU_IRQ, IRQ_FIFO[15:0] = loopCounter
 */
void abe_irq_ping_pong(void)
{
	abe_call_subroutine(abe_irq_pingpong_player_id, NOPARAMETER,
			    NOPARAMETER, NOPARAMETER, NOPARAMETER);
}

/**
 * abe_irq_check_for_sequences
* @i: sequence ID
 *
 * check the active sequence list
 *
 */
void abe_irq_check_for_sequences(u32 i)
{
}

/**
 * abe_irq_aps
 *
 * call the application subroutines that updates
 * the acoustics protection filters
 */
void abe_irq_aps(u32 aps_info)
{
	abe_call_subroutine(abe_irq_aps_adaptation_id, NOPARAMETER, NOPARAMETER,
			    NOPARAMETER, NOPARAMETER);
}
