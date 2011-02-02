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

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/err.h>
#include <linux/slab.h>

#include "abe_main.h"
#include "abe_ref.h"
#include "abe_typedef.h"
#include "abe_initxxx_labels.h"
#include "abe_dbg.h"
#include "abe_mem.h"

/**
 * abe_clear_irq - clear ABE interrupt
 *
 * This subroutine is call to clear MCU Irq
 */
int abe_clear_irq(void)
{
	omap_abe_reg_writel(abe, OMAP_ABE_AESS_OFFSET + ABE_MCU_IRQSTATUS, 1);
	return 0;
}
EXPORT_SYMBOL(abe_clear_irq);

/**
 * abe_write_event_generator - Selects event generator source
 * @e: Event Generation Counter, McPDM, DMIC or default.
 *
 * Loads the AESS event generator hardware source.
 * Loads the firmware parameters accordingly.
 * Indicates to the FW which data stream is the most important to preserve
 * in case all the streams are asynchronous.
 * If the parameter is "default", then HAL decides which Event source
 * is the best appropriate based on the opened ports.
 *
 * When neither the DMIC and the McPDM are activated, the AE will have
 * its EVENT generator programmed with the EVENT_COUNTER.
 * The event counter will be tuned in order to deliver a pulse frequency higher
 * than 96 kHz.
 * The DPLL output at 100% OPP is MCLK = (32768kHz x6000) = 196.608kHz
 * The ratio is (MCLK/96000)+(1<<1) = 2050
 * (1<<1) in order to have the same speed at 50% and 100% OPP
 * (only 15 MSB bits are used at OPP50%)
 */
int abe_write_event_generator(u32 source)
{
	u32 event, selection;
	u32 counter = EVENT_GENERATOR_COUNTER_DEFAULT;
	u32 start = EVENT_GENERATOR_ON;

	_log(id_write_event_generator, source, 0, 0);
	switch (source) {
	case EVENT_TIMER:
		selection = EVENT_SOURCE_COUNTER;
		event = 0;
		break;
	case EVENT_44100:
		selection = EVENT_SOURCE_COUNTER;
		event = 0;
		counter = EVENT_GENERATOR_COUNTER_44100;
		break;
	default:
		abe->dbg_param |= ERR_API;
		abe_dbg_error_log(ABE_BLOCK_COPY_ERR);
		return -EINVAL;
	}
	abe_current_event_id = source;

	omap_abe_reg_writel(abe, OMAP_ABE_AESS_OFFSET +
			    EVENT_GENERATOR_COUNTER, counter);
	omap_abe_reg_writel(abe, OMAP_ABE_AESS_OFFSET +
			    EVENT_SOURCE_SELECTION, selection);
	omap_abe_reg_writel(abe, OMAP_ABE_AESS_OFFSET +
			    EVENT_GENERATOR_START, start);
	omap_abe_reg_writel(abe, OMAP_ABE_AESS_OFFSET +
			    AUDIO_ENGINE_SCHEDULER, event);

	return 0;
}
EXPORT_SYMBOL(abe_write_event_generator);

/**
 * abe_start_event_generator - Starts event generator source
 *
 * Start the event genrator of AESS. No more event will be send to AESS engine.
 * Upper layer must wait 1/96kHz to be sure that engine reaches
 * the IDLE instruction.
 */
int abe_start_event_generator(void)
{
	omap_abe_reg_writel(abe, OMAP_ABE_AESS_OFFSET +
			    EVENT_GENERATOR_START, 1);
	return 0;
}
EXPORT_SYMBOL(abe_start_event_generator);

/**
 * abe_stop_event_generator - Stops event generator source
 *
 * Stop the event genrator of AESS. No more event will be send to AESS engine.
 * Upper layer must wait 1/96kHz to be sure that engine reaches
 * the IDLE instruction.
 */
int abe_stop_event_generator(void)
{
	omap_abe_reg_writel(abe, OMAP_ABE_AESS_OFFSET +
			    EVENT_GENERATOR_START, 0);
	return 0;
}
EXPORT_SYMBOL(abe_stop_event_generator);

/**
 * abe_hw_configuration
 *
 */
void abe_hw_configuration()
{
	/* enables the DMAreq from AESS AESS_DMAENABLE_SET = 255 */
	omap_abe_reg_writel(abe, OMAP_ABE_AESS_OFFSET + 0x60, 0xFF);
	/* enables the MCU IRQ from AESS to Cortex A9 */
	omap_abe_reg_writel(abe, OMAP_ABE_AESS_OFFSET + 0x3C, 0x01);
}

/**
 * abe_disable_irq - disable MCU/DSP ABE interrupt
 *
 * This subroutine is disabling ABE MCU/DSP Irq
 */
int abe_disable_irq(void)
{
	/*
	 * disables the DMAreq from AESS AESS_DMAENABLE_CLR = 127
	 * DMA_Req7 will still be enabled as it is used for ABE trace
	 */
	omap_abe_reg_writel(abe, OMAP_ABE_AESS_OFFSET + 0x64, 0x7F);
	/* disables the MCU IRQ from AESS to Cortex A9 */
	omap_abe_reg_writel(abe, OMAP_ABE_AESS_OFFSET + 0x40, 0x01);
	return 0;
}
EXPORT_SYMBOL(abe_disable_irq);

