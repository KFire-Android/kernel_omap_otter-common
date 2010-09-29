/*
 * ALSA SoC OMAP ABE driver
 *
 * Author:	Laurent Le Faucheur <l-le-faucheur@ti.com>
 * 		Liam Girdwood <lrg@slimlogic.co.uk>
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
 * APS interrupts : IRQtag_APS to [31:28], APS_IRQs to [27:16], loopCounter to [15:0]
 * SEQ interrupts : IRQtag_COUNT to [31:28], Count_IRQs to [27:16], loopCounter to [15:0]
 * Ping-Pong Interrupts : IRQtag_PP to [31:28], PP_MCU_IRQ to [27:16], loopCounter to [15:0]
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
 * call the application subroutines that updates the acoustics protection filters
 */
void abe_irq_aps(u32 aps_info)
{
	abe_call_subroutine(abe_irq_aps_adaptation_id, NOPARAMETER, NOPARAMETER,
			    NOPARAMETER, NOPARAMETER);
}
