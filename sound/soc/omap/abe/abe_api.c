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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 */

#include "abe_main.h"
#include "abe_typedef.h"
#include "abe_initxxx_labels.h"
#include "abe_dbg.h"

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/platform_device.h>
#include <linux/workqueue.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/pm_runtime.h>

struct abe_gain {
	u32 gain;
	int mute;
};
struct abe_gain *abe_gain_table;

/**
 * abe_reset_hal - reset the ABE/HAL
 * @rdev: regulator source
 * @constraints: constraints to apply
 *
 * Operations : reset the HAL by reloading the static variables and
 * default AESS registers.
 * Called after a PRCM cold-start reset of ABE
 */
abehal_status abe_reset_hal(void)
{
  _log(id_reset_hal, 0, 0, 0)
#ifdef CONFIG_OMAP_ABE_DEBUG
    abe_dbg_output = TERMINAL_OUTPUT;

  abe_dbg_activity_log_write_pointer = 0;

  /* IRQ & DBG circular read pointer in DMEM */
  abe_irq_dbg_read_ptr = 0;

  /* set debug mask to "enable all traces" */
  abe_dbg_mask = (abe_dbg_t) (0);
#endif
  abe_hw_configuration();
	abe_gain_table = kzalloc(sizeof(struct abe_gain)*34, GFP_KERNEL);


  return 0;
}

EXPORT_SYMBOL(abe_reset_hal);

/**
 * abe_load_fw_param - Load ABE Firmware memories
 * @PMEM: Pointer of Program memory data
 * @PMEM_SIZE: Size of PMEM data
 * @CMEM: Pointer of Coeffients memory data
 * @CMEM_SIZE: Size of CMEM data
 * @SMEM: Pointer of Sample memory data
 * @SMEM_SIZE: Size of SMEM data
 * @DMEM: Pointer of Data memory data
 * @DMEM_SIZE: Size of DMEM data
 *
 * loads the Audio Engine firmware, generate a single pulse on the Event
 * generator to let execution start, read the version number returned from
 * this execution.
 */
abehal_status abe_load_fw_param(u32 * ABE_FW)
{
  static u32 warm_boot;
  u32 event_gen;
  u32 pmem_size, dmem_size, smem_size, cmem_size;
  u32 *pmem_ptr, *dmem_ptr, *smem_ptr, *cmem_ptr, *fw_ptr;

  _log(id_load_fw_param, 0, 0, 0)
#if PC_SIMULATION
    /* the code is loaded from the Checkers */
#else
#define ABE_FW_OFFSET 5
    fw_ptr = ABE_FW;
  abe_firmware_version_number = *fw_ptr++;
  pmem_size = *fw_ptr++;
  cmem_size = *fw_ptr++;
  dmem_size = *fw_ptr++;
  smem_size = *fw_ptr++;

  pmem_ptr = fw_ptr;
  cmem_ptr = pmem_ptr + (pmem_size >> 2);
  dmem_ptr = cmem_ptr + (cmem_size >> 2);
  smem_ptr = dmem_ptr + (dmem_size >> 2);

  /* do not load PMEM */
  if (warm_boot) {
    /* Stop the event Generator */
    event_gen = 0;
    abe_block_copy(COPY_FROM_HOST_TO_ABE, ABE_ATC,
		   EVENT_GENERATOR_START, &event_gen, 4);

    /* Now we are sure the firmware is stalled */
    abe_block_copy(COPY_FROM_HOST_TO_ABE, ABE_CMEM, 0, cmem_ptr,
		   cmem_size);
    abe_block_copy(COPY_FROM_HOST_TO_ABE, ABE_SMEM, 0, smem_ptr,
		   smem_size);
    abe_block_copy(COPY_FROM_HOST_TO_ABE, ABE_DMEM, 0, dmem_ptr,
		   dmem_size);

    /* Restore the event Generator status */
    event_gen = 1;
    abe_block_copy(COPY_FROM_HOST_TO_ABE, ABE_ATC,
		   EVENT_GENERATOR_START, &event_gen, 4);
  } else {
    abe_block_copy(COPY_FROM_HOST_TO_ABE, ABE_PMEM, 0, pmem_ptr,
		   pmem_size);
    abe_block_copy(COPY_FROM_HOST_TO_ABE, ABE_CMEM, 0, cmem_ptr,
		   cmem_size);
    abe_block_copy(COPY_FROM_HOST_TO_ABE, ABE_SMEM, 0, smem_ptr,
		   smem_size);
    abe_block_copy(COPY_FROM_HOST_TO_ABE, ABE_DMEM, 0, dmem_ptr,
		   dmem_size);
  }

  warm_boot = 1;
#endif
  return 0;
}

EXPORT_SYMBOL(abe_load_fw_param);

/**
 * abe_load_fw - Load ABE Firmware and initialize memories
 *
 * loads the Audio Engine firmware, generate a single pulse on the Event
 * generator to let execution start, read the version number returned from
 * this execution.
 */
abehal_status abe_load_fw(void)
{

  _log(id_load_fw, 0, 0, 0)

    abe_load_fw_param((u32 *) abe_firmware_array);

  abe_reset_all_ports();
  abe_build_scheduler_table();
  abe_reset_all_sequence();
  abe_select_main_port(PDM_DL_PORT);
  return 0;
}

EXPORT_SYMBOL(abe_load_fw);

/**
 * abe_read_hardware_configuration - Return default HW periferals configuration
 * @u: use-case description list (pointer)
 * @o: opp mode (pointer)
 * @hw: pointer to the output HW structure
 *

 * Parameter :
 * U : use-case description list (pointer)
 * H : pointer to the output structure
 *
 * Operations :
 * return a structure with the HW thresholds compatible with the HAL/FW/AESS_ATC
 * will be upgraded in FW06
 * return a structure with the HW thresholds compatible with the HAL/FW/AESS_ATC
 */
abehal_status
abe_read_hardware_configuration(u32 * u, u32 * o, abe_hw_config_init_t * hw)
{

  _log(id_read_hardware_configuration, (u32) u, (u32) u >> 8,
       (u32) u >> 16)

    abe_read_use_case_opp(u, o);

  /* 0: 96kHz 1:192kHz */
  hw->MCPDM_CTRL__DIV_SEL = 0;

  /* 0: no command in the FIFO, 1: 6 data on each lines (with commands) */
  hw->MCPDM_CTRL__CMD_INT = 1;

  /* 0:MSB aligned 1:LSB aligned */
  hw->MCPDM_CTRL__PDMOUTFORMAT = 0;
  hw->MCPDM_CTRL__PDM_DN5_EN = 1;
  hw->MCPDM_CTRL__PDM_DN4_EN = 1;
  hw->MCPDM_CTRL__PDM_DN3_EN = 1;
  hw->MCPDM_CTRL__PDM_DN2_EN = 1;
  hw->MCPDM_CTRL__PDM_DN1_EN = 1;
  hw->MCPDM_CTRL__PDM_UP3_EN = 0;
  hw->MCPDM_CTRL__PDM_UP2_EN = 1;
  hw->MCPDM_CTRL__PDM_UP1_EN = 1;

  /* All the McPDM_DL FIFOs are enabled simultaneously */
  hw->MCPDM_FIFO_CTRL_DN__DN_TRESH = MCPDM_DL_ITER / 6;

  /* number of ATC access upon AMIC DMArequests, 2 the FIFOs channels
     are enabled */
  hw->MCPDM_FIFO_CTRL_UP__UP_TRESH = MCPDM_UL_ITER / 2;

  /* 0:2.4MHz 1:3.84MHz */
  hw->DMIC_CTRL__DMIC_CLK_DIV = 0;

  /* 0:MSB aligned 1:LSB aligned */
  hw->DMIC_CTRL__DMICOUTFORMAT = 0;
  hw->DMIC_CTRL__DMIC_UP3_EN = 1;
  hw->DMIC_CTRL__DMIC_UP2_EN = 1;
  hw->DMIC_CTRL__DMIC_UP1_EN = 1;

  /* 1*(DMIC_UP1_EN+ 2+ 3)*2 OCP read access every 96/88.1 KHz. */
  hw->DMIC_FIFO_CTRL__DMIC_TRESH = DMIC_ITER / 6;

  /* MCBSP SPECIFICATION
     RJUST = 00 Right justify data and zero fill MSBs in DRR[1,2]
     RJUST = 01 Right justify data and sign extend it into the MSBs
     in DRR[1,2]
     RJUST = 10 Left justify data and zero fill LSBs in DRR[1,2]
     MCBSPLP_RJUST_MODE_RIGHT_ZERO = 0x0,
     MCBSPLP_RJUST_MODE_RIGHT_SIGN = 0x1,
     MCBSPLP_RJUST_MODE_LEFT_ZERO = 0x2,
     MCBSPLP_RJUST_MODE_MAX = MCBSPLP_RJUST_MODE_LEFT_ZERO
  */
  hw->MCBSP_SPCR1_REG__RJUST = 2;

  /* 1=MONO, 2=STEREO, 3=TDM_3_CHANNELS, 4=TDM_4_CHANNELS, .... */
  hw->MCBSP_THRSH2_REG_REG__XTHRESHOLD = 1;

  /* 1=MONO, 2=STEREO, 3=TDM_3_CHANNELS, 4=TDM_4_CHANNELS, .... */
  hw->MCBSP_THRSH1_REG_REG__RTHRESHOLD = 1;

  /* Slimbus IP FIFO thresholds */
  hw->SLIMBUS_DCT_FIFO_SETUP_REG__SB_THRESHOLD = 1;

  /* 2050 gives about 96kHz */
  hw->AESS_EVENT_GENERATOR_COUNTER__COUNTER_VALUE =
    EVENT_GENERATOR_COUNTER_DEFAULT;

  /* 0: DMAreq, 1:Counter */
  hw->AESS_EVENT_SOURCE_SELECTION__SELECTION = 1;

  /* 5bits DMAreq selection */
  hw->AESS_AUDIO_ENGINE_SCHEDULER__DMA_REQ_SELECTION =
    ABE_ATC_MCPDMDL_DMA_REQ;

  /* THE famous EVENT timer ! */
  hw->HAL_EVENT_SELECTION = EVENT_TIMER;
  return 0;
}

EXPORT_SYMBOL(abe_read_hardware_configuration);

/**
 * abe_irq_processing - Process ABE interrupt
 *
 * This subroutine is call upon reception of "MA_IRQ_99 ABE_MPU_IRQ" Audio
 * back-end interrupt. This subroutine will check the ATC Hrdware, the
 * IRQ_FIFO from the AE and act accordingly. Some IRQ source are originated
 * for the delivery of "end of time sequenced tasks" notifications, some are
 * originated from the Ping-Pong protocols, some are generated from
 * the embedded debugger when the firmware stops on programmable break-points,
 * etc …
 */
abehal_status abe_irq_processing(void)
{
  u32 clear_abe_irq;
  u32 abe_irq_dbg_write_ptr, i, cmem_src, sm_cm;
  abe_irq_data_t IRQ_data;
#define IrqFiFoMask ((D_McuIrqFifo_sizeof >> 2) -1)

  _log(id_irq_processing, 0, 0, 0)

    /* extract the write pointer index from CMEM memory (INITPTR format) */
    /* CMEM address of the write pointer in bytes */
    cmem_src = MCU_IRQ_FIFO_ptr_labelID * 4;
  abe_block_copy(COPY_FROM_ABE_TO_HOST, ABE_CMEM, cmem_src,
		 &sm_cm, sizeof(abe_irq_dbg_write_ptr));

  /* AESS left-pointer index located on MSBs */
  abe_irq_dbg_write_ptr = sm_cm >> 16;
  abe_irq_dbg_write_ptr &= 0xFF;

  /* loop on the IRQ FIFO content */
  for (i = 0; i < D_McuIrqFifo_sizeof; i++) {
    /* stop when the FIFO is empty */
    if (abe_irq_dbg_write_ptr == abe_irq_dbg_read_ptr)
      break;
    /* read the IRQ/DBG FIFO */
    abe_block_copy(COPY_FROM_ABE_TO_HOST, ABE_DMEM,
		   (D_McuIrqFifo_ADDR +
		    (abe_irq_dbg_read_ptr << 2)),
		   (u32 *) & IRQ_data, sizeof(IRQ_data));
    abe_irq_dbg_read_ptr = (abe_irq_dbg_read_ptr + 1) & IrqFiFoMask;

    /* select the source of the interrupt */
    switch (IRQ_data.tag) {
    case IRQtag_APS:
      _log(id_irq_processing, IRQ_data.data, 0, 1)
	abe_irq_aps(IRQ_data.data);
      break;
    case IRQtag_PP:
      _log(id_irq_processing, 0, 0, 2)
	abe_irq_ping_pong();
      break;
    case IRQtag_COUNT:
      _log(id_irq_processing, IRQ_data.data, 0, 3)
	abe_irq_check_for_sequences(IRQ_data.data);
      break;
    default:
      break;
    }
  }

  abe_monitoring();

  clear_abe_irq = 1;
  abe_block_copy(COPY_FROM_HOST_TO_ABE, ABE_ATC, ABE_MCU_IRQSTATUS,
		 &clear_abe_irq, 4);
  return 0;
}

EXPORT_SYMBOL(abe_irq_processing);

/**
 * abe_select_main_port - Select stynchronization port for Event generator.
 * @id: audio port name
 *
 * tells the FW which is the reference stream for adjusting
 * the processing on 23/24/25 slots
 */
abehal_status abe_select_main_port(u32 id)
{
  u32 selection;

  _log(id_select_main_port, id, 0, 0)

    /* flow control */
    selection = D_IOdescr_ADDR + id * sizeof(ABE_SIODescriptor) +
    flow_counter_;

  /* when the main port is a sink port from AESS point of view
     the sign the firmware task analysis must be changed  */
  selection &= 0xFFFFL;
  if (abe_port[id].protocol.direction == ABE_ATC_DIRECTION_IN)
    selection |= 0x80000;

  abe_block_copy(COPY_FROM_HOST_TO_ABE, ABE_DMEM, D_Slot23_ctrl_ADDR,
		 &selection, 4);
  return 0;
}

EXPORT_SYMBOL(abe_select_main_port);

/**
 * abe_write_event_generator - Select event generator source
 * @e: Event Generation Counter, McPDM, DMIC or default.
 *
 * load the AESS event generator hardware source. Loads the firmware parameters
 * accordingly. Indicates to the FW which data stream is the most important to preserve
 * in case all the streams are asynchronous. If the parameter is "default", let the HAL
 * decide which Event source is the best appropriate based on the opened ports.
 *
 * When neither the DMIC and the McPDM are activated the AE will have its EVENT generator programmed
 * with the EVENT_COUNTER. The event counter will be tuned in order to deliver a pulse frequency higher
 * than 96 kHz. The DPLL output at 100% OPP is MCLK = (32768kHz x6000) = 196.608kHz
 * The ratio is (MCLK/96000)+(1<<1) = 2050
 * (1<<1) in order to have the same speed at 50% and 100% OPP (only 15 MSB bits are used at OPP50%)
 */
abehal_status abe_write_event_generator(u32 e)
{
  u32 event, selection, counter, start;

  _log(id_write_event_generator, e, 0, 0)

    counter = EVENT_GENERATOR_COUNTER_DEFAULT;
  start = EVENT_GENERATOR_ON;
  abe_current_event_id = e;

  switch (e) {
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
    abe_dbg_param |= ERR_API;
    abe_dbg_error_log(ABE_BLOCK_COPY_ERR);
  }

  abe_block_copy(COPY_FROM_HOST_TO_ABE, ABE_ATC,
		 EVENT_GENERATOR_COUNTER, &counter, 4);
  abe_block_copy(COPY_FROM_HOST_TO_ABE, ABE_ATC,
		 EVENT_SOURCE_SELECTION, &selection, 4);
  abe_block_copy(COPY_FROM_HOST_TO_ABE, ABE_ATC,
		 EVENT_GENERATOR_START, &start, 4);
  abe_block_copy(COPY_FROM_HOST_TO_ABE, ABE_ATC,
		 AUDIO_ENGINE_SCHEDULER, &event, 4);

  return 0;
}

EXPORT_SYMBOL(abe_write_event_generator);

/**
 * abe_read_use_case_opp() - description for void abe_read_use_case_opp().
 *
 * returns the expected min OPP for a given use_case list
 */
abehal_status abe_read_use_case_opp(u32 * u, u32 * o)
{
  u32 opp, i;
  u32 *ptr = u;
#define MAX_READ_USE_CASE_OPP 10
#define OPP_25 1
#define OPP_50 2
#define OPP_100 4

  _log(id_read_use_case_opp, (u32) u, (u32) u >> 8, (u32) u >> 16)

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

  return 0;
}

EXPORT_SYMBOL(abe_read_use_case_opp);

/**
 * abe_set_opp_processing - Set OPP mode for ABE Firmware
 * @opp: OOPP mode
 *
 * New processing network and OPP:
 * 0: Ultra Lowest power consumption audio player (no post-processing, no mixer)
 * 1: OPP 25% (simple multimedia features, including low-power player)
 * 2: OPP 50% (multimedia and voice calls)
 * 3: OPP100% ( multimedia complex use-cases)
 *
 * Rearranges the FW task network to the corresponding OPP list of features.
 * The corresponding AE ports are supposed to be set/reset accordingly before
 * this switch.
 *
 */
abehal_status abe_set_opp_processing(u32 opp)
{
  u32 dOppMode32, sio_desc_address, sio_desc_address_pp;
  ABE_SIODescriptor desc;
  ABE_SPingPongDescriptor desc_pp;

  _lock_enter;
  _log(id_set_opp_processing, opp, 0, 0)

    switch (opp) {
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
		 D_maxTaskBytesInSlot_ADDR, &dOppMode32, sizeof(u32));

  sio_desc_address = dmem_port_descriptors + (MM_DL_PORT *
					      sizeof(ABE_SIODescriptor));
  abe_block_copy(COPY_FROM_ABE_TO_HOST, ABE_DMEM, sio_desc_address,
		 (u32 *) & desc, sizeof(desc));

  sio_desc_address_pp = D_PingPongDesc_ADDR;
  abe_block_copy(COPY_FROM_HOST_TO_ABE, ABE_DMEM, sio_desc_address_pp,
		 (u32 *) & desc_pp, sizeof(desc_pp));

  if (dOppMode32 == DOPPMODE32_OPP100) {
    /* ASRC input buffer, size 40 */
    desc.smem_addr1 = smem_mm_dl_opp100;
    desc_pp.smem_addr = smem_mm_dl_opp100;
  } else {
    /* at OPP 25/50 or without ASRC */
    desc.smem_addr1 = smem_mm_dl_opp25;
    desc_pp.smem_addr = smem_mm_dl_opp25;
  }
  abe_block_copy(COPY_FROM_HOST_TO_ABE, ABE_DMEM, sio_desc_address,
		 (u32 *) & desc, sizeof(desc));
  abe_block_copy(COPY_FROM_HOST_TO_ABE, ABE_DMEM, sio_desc_address_pp,
		 (u32 *) & desc_pp, sizeof(desc_pp));
  return 0;
}

EXPORT_SYMBOL(abe_set_opp_processing);

/**
 * abe_set_ping_pong_buffer
 * @port: ABE port ID
 * @n_bytes: Size of Ping/Pong buffer
 *
 * Updates the next ping-pong buffer with "size" bytes copied from the
 * host processor. This API notifies the FW that the data transfer is done.
 */
abehal_status abe_set_ping_pong_buffer(u32 port, u32 n_bytes)
{
  u32 sio_pp_desc_address, struct_offset, n_samples, datasize,
    base_and_size, *src;
  ABE_SPingPongDescriptor desc_pp;

  _log(id_set_ping_pong_buffer, port, n_bytes, n_bytes >> 8)

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
		 (u32 *) & desc_pp, sizeof(desc_pp));

  /*
   * read the port SIO descriptor and extract the current pointer
   * address after reading the counter
   */
  if ((desc_pp.counter & 0x1) == 0) {
    struct_offset =
      (u32) & (desc_pp.nextbuff0_BaseAddr) - (u32) & (desc_pp);
    base_and_size = desc_pp.nextbuff0_BaseAddr;
  } else {
    struct_offset =
      (u32) & (desc_pp.nextbuff1_BaseAddr) - (u32) & (desc_pp);
    base_and_size = desc_pp.nextbuff1_BaseAddr;
  }

  base_and_size = (base_and_size & 0xFFFFL) + (n_samples << 16);

  sio_pp_desc_address = D_PingPongDesc_ADDR + struct_offset;
  src = &base_and_size;
  abe_block_copy(COPY_FROM_HOST_TO_ABE, ABE_DMEM, sio_pp_desc_address,
		 (u32 *) & base_and_size, sizeof(u32));

  return 0;
}

EXPORT_SYMBOL(abe_set_ping_pong_buffer);

/**
 * abe_read_next_ping_pong_buffer
 * @port: ABE portID
 * @p: Next buffer address (pointer)
 * @n: Next buffer size (pointer)
 *
 * Tell the next base address of the next ping_pong Buffer and its size
 */
abehal_status abe_read_next_ping_pong_buffer(u32 port, u32 * p, u32 * n)
{
  u32 sio_pp_desc_address;
  ABE_SPingPongDescriptor desc_pp;

  _log(id_read_next_ping_pong_buffer, port, 0, 0)

    /* ping_pong is only supported on MM_DL */
    if (port != MM_DL_PORT) {
      abe_dbg_param |= ERR_API;
      abe_dbg_error_log(ABE_PARAMETER_ERROR);
    }

  /* read the port SIO descriptor and extract the current pointer
     address after reading the counter */
  sio_pp_desc_address = D_PingPongDesc_ADDR;
  abe_block_copy(COPY_FROM_ABE_TO_HOST, ABE_DMEM, sio_pp_desc_address,
		 (u32 *) & desc_pp, sizeof(ABE_SPingPongDescriptor));

  if ((desc_pp.counter & 0x1) == 0) {
    _log(id_read_next_ping_pong_buffer, port, 0, 0)
      * p = desc_pp.nextbuff0_BaseAddr;
  } else {
    _log(id_read_next_ping_pong_buffer, port, 1, 0)
      * p = desc_pp.nextbuff1_BaseAddr;
  }

  /* translates the number of samples in bytes */
  *n = abe_size_pingpong;

  return 0;
}

EXPORT_SYMBOL(abe_read_next_ping_pong_buffer);

/**
 * abe_init_ping_pong_buffer
 * @id: ABE port ID
 * @size_bytes:size of the ping pong
 * @n_buffers:number of buffers (2 = ping/pong)
 * @p:returned address of the ping-pong list of base address (byte offset
 from DMEM start)
 *
 * Computes the base address of the ping_pong buffers
 */
abehal_status
abe_init_ping_pong_buffer(u32 id, u32 size_bytes, u32 n_buffers, u32 * p)
{
  u32 i, dmem_addr;

  _log(id_init_ping_pong_buffer, id, size_bytes, n_buffers)

    /* ping_pong is supported in 2 buffers configuration right now but FW
       is ready for ping/pong/pung/pang... */
    if (id != MM_DL_PORT || n_buffers > MAX_PINGPONG_BUFFERS) {
      abe_dbg_param |= ERR_API;
      abe_dbg_error_log(ABE_PARAMETER_ERROR);
    }

  for (i = 0; i < n_buffers; i++) {
    dmem_addr = dmem_ping_pong_buffer + (i * size_bytes);

    /* base addresses of the ping pong buffers in U8 unit */
    abe_base_address_pingpong[i] = dmem_addr;
  }

  /* global data */
  abe_size_pingpong = size_bytes;
  *p = (u32) dmem_ping_pong_buffer;

  return 0;
}

EXPORT_SYMBOL(abe_init_ping_pong_buffer);

/**
 * abe_plug_subroutine
 * @id: returned sequence index after plugging a new subroutine
 * @f: subroutine address to be inserted
 * @n: number of parameters of this subroutine
 * @params: pointer on parameters
 *
 * register a list of subroutines for call-back purpose
 */
abehal_status
abe_plug_subroutine(u32 * id, abe_subroutine2 f, u32 n, u32 * params)
{
  _log(id_plug_subroutine, (u32) (*id), (u32) f, n)
    abe_add_subroutine(id, (abe_subroutine2) f, n, (u32 *) params);
  return 0;
}

EXPORT_SYMBOL(abe_plug_subroutine);

/**
 * abe_set_sequence_time_accuracy
 * @fast: fast counter
 * @slow: slow counter
 *
 */
abehal_status abe_set_sequence_time_accuracy(u32 fast, u32 slow)
{
  u32 data;

  _log(id_set_sequence_time_accuracy, fast, slow, 0)

    data = minimum(MAX_UINT16, fast / FW_SCHED_LOOP_FREQ_DIV1000);
  abe_block_copy(COPY_FROM_HOST_TO_ABE, ABE_DMEM, D_fastCounter_ADDR,
		 &data, sizeof(data));

  data = minimum(MAX_UINT16, slow / FW_SCHED_LOOP_FREQ_DIV1000);
  abe_block_copy(COPY_FROM_HOST_TO_ABE, ABE_DMEM, D_slowCounter_ADDR,
		 &data, sizeof(data));

  return 0;
}

EXPORT_SYMBOL(abe_set_sequence_time_accuracy);

/**
 * abe_reset_port
 * @id: ABE port ID
 *
 * stop the port activity and reload default parameters on the associated
 * processing features.
 * Clears the internal AE buffers.
 */
abehal_status abe_reset_port(u32 id)
{

  _log(id_reset_port, id, 0, 0)
    abe_port[id] = ((abe_port_t *) abe_port_init)[id];
  return 0;
}

EXPORT_SYMBOL(abe_reset_port);

/**
 * abe_read_remaining_data
 * @id:	ABE port_ID
 * @n: size pointer to the remaining number of 32bits words
 *
 * computes the remaining amount of data in the buffer.
 */
abehal_status abe_read_remaining_data(u32 port, u32 * n)
{
  u32 sio_pp_desc_address;
  ABE_SPingPongDescriptor desc_pp;

  _log(id_read_remaining_data, port, 0, 0)

    /*
     * read the port SIO descriptor and extract the
     * current pointer address after reading the counter
     */
    sio_pp_desc_address = D_PingPongDesc_ADDR;
  abe_block_copy(COPY_FROM_ABE_TO_HOST, ABE_DMEM, sio_pp_desc_address,
		 (u32 *) & desc_pp, sizeof(ABE_SPingPongDescriptor));
  *n = desc_pp.workbuff_Samples;

  return 0;
}

EXPORT_SYMBOL(abe_read_remaining_data);

/**
 * abe_disable_data_transfer
 * @id: ABE port id
 *
 * disables the ATC descriptor and stop IO/port activities
 * disable the IO task (@f = 0)
 * clear ATC DMEM buffer, ATC enabled
 */
abehal_status abe_disable_data_transfer(u32 id)
{
  _log(id_disable_data_transfer, id, 0, 0)

    /* local host variable status= "port is running" */
    abe_port[id].status = OMAP_ABE_PORT_ACTIVITY_IDLE;

  /* disable DMA requests */
  abe_disable_dma_request(id);

  /* disable ATC transfers */
  abe_init_atc(id);
  abe_clean_temporary_buffers(id);

  return 0;
}

EXPORT_SYMBOL(abe_disable_data_transfer);

/**
 * abe_enable_data_transfer
 * @ip: ABE port id
 *
 * enables the ATC descriptor
 * reset ATC pointers
 * enable the IO task (@f <> 0)
 */
abehal_status abe_enable_data_transfer(u32 id)
{
  abe_port_protocol_t *protocol;
  abe_data_format_t format;

  _log(id_enable_data_transfer, id, 0, 0)

    /* check firmware status bit for this */
    /*if (abe_port[id].status == OMAP_ABE_PORT_ACTIVITY_RUNNING)
      return 0; */
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
  if (id == VX_UL_PORT) {
    /* Init VX_UL ASRC and enable its adaptation */
    abe_init_asrc_vx_ul(250);
  }
  if (id == VX_DL_PORT) {
    /* Init VX_DL ASRC and enable its adaptation */
    abe_init_asrc_vx_dl(250);
  }

  /* local host variable status= "port is running" */
  abe_port[id].status = OMAP_ABE_PORT_ACTIVITY_RUNNING;
  /* enable DMA requests */
  abe_enable_dma_request(id);

  return 0;
}

EXPORT_SYMBOL(abe_enable_data_transfer);

/**
 * abe_set_dmic_filter
 * @d: DMIC decimation ratio : 16/25/32/40
 *
 * Loads in CMEM a specific list of coefficients depending on the DMIC sampling
 * frequency (2.4MHz or 3.84MHz). This table compensates the DMIC decimator
 * roll-off at 20kHz.
 * The default table is loaded with the DMIC 2.4MHz recommended configuration.
 */
abehal_status abe_set_dmic_filter(u32 d)
{
  s32 *src;

  _log(id_set_dmic_filter, d, 0, 0)

    switch (d) {
    case ABE_DEC16:
      src = (s32 *) abe_dmic_16;
      break;
    case ABE_DEC25:
      src = (s32 *) abe_dmic_25;
      break;
    case ABE_DEC32:
      src = (s32 *) abe_dmic_32;
      break;
    default:
    case ABE_DEC40:
      src = (s32 *) abe_dmic_40;
      break;
    }
  abe_block_copy(COPY_FROM_HOST_TO_ABE, ABE_CMEM,
		 C_98_48_LP_Coefs_ADDR,
		 (u32 *) src, C_98_48_LP_Coefs_sizeof << 2);

  return 0;
}

EXPORT_SYMBOL(abe_set_dmic_filter);

/**
 * abe_connect_cbpr_dmareq_port
 * @id: port name
 * @f: desired data format
 * @d: desired dma_request line (0..7)
 * @a: returned pointer to the base address of the CBPr register and number of
 *	samples to exchange during a DMA_request.
 *
 * enables the data echange between a DMA and the ABE through the
 *	CBPr registers of AESS.
 */

abehal_status
abe_connect_cbpr_dmareq_port(u32 id, abe_data_format_t * f, u32 d,
			     abe_dma_t * returned_dma_t)
{

  _log(id_connect_cbpr_dmareq_port, id, f->f, f->samp_format)

    abe_port[id] = ((abe_port_t *) abe_port_init)[id];
  (abe_port[id]).format = (*f);
  abe_port[id].protocol.protocol_switch = DMAREQ_PORT_PROT;
  abe_port[id].protocol.p.prot_dmareq.iter = abe_dma_port_iteration(f);
  abe_port[id].protocol.p.prot_dmareq.dma_addr = ABE_DMASTATUS_RAW;
  abe_port[id].protocol.p.prot_dmareq.dma_data = (1 << d);

  abe_port[id].status = OMAP_ABE_PORT_ACTIVITY_RUNNING;

  /* load the micro-task parameters */
  abe_init_io_tasks(id, &((abe_port[id]).format),
		    &((abe_port[id]).protocol));

  /* load the dma_t with physical information from AE memory mapping */
  abe_init_dma_t(id, &((abe_port[id]).protocol));

  /* load the ATC descriptors - disabled */
  abe_init_atc(id);

  /* return the dma pointer address */
  abe_read_port_address(id, returned_dma_t);

  return 0;
}

EXPORT_SYMBOL(abe_connect_cbpr_dmareq_port);

/**
 * abe_connect_dmareq_ping_pong_port
 * @id: port name
 * @f: desired data format
 * @d: desired dma_request line (0..7)
 * @s: half-buffer (ping) size
 * @a: returned pointer to the base address of the ping-pong buffer and number
 * of samples to exchange during a DMA_request.
 *
 * enables the data echanges between a DMA and a direct access to
 * the DMEM memory of ABE. On each dma_request activation the DMA will exchange
 * "s" bytes and switch to the "pong" buffer for a new buffer exchange.
 */
abehal_status
abe_connect_dmareq_ping_pong_port(u32 id, abe_data_format_t * f,
				  u32 d, u32 s, abe_dma_t * returned_dma_t)
{
  abe_dma_t dma1;

  _log(id_connect_dmareq_ping_pong_port, id, f->f, f->samp_format)

    /* ping_pong is only supported on MM_DL */
    if (id != MM_DL_PORT) {
      abe_dbg_param |= ERR_API;
      abe_dbg_error_log(ABE_PARAMETER_ERROR);
    }

  /* declare PP buffer and prepare the returned dma_t */
  abe_init_ping_pong_buffer(MM_DL_PORT, s, 2,
			    (u32 *) & (returned_dma_t->data));

  abe_port[id] = ((abe_port_t *) abe_port_init)[id];

  (abe_port[id]).format = (*f);
  (abe_port[id]).protocol.protocol_switch = PINGPONG_PORT_PROT;
  (abe_port[id]).protocol.p.prot_pingpong.buf_addr =
    dmem_ping_pong_buffer;
  (abe_port[id]).protocol.p.prot_pingpong.buf_size = s;
  (abe_port[id]).protocol.p.prot_pingpong.irq_addr = ABE_DMASTATUS_RAW;
  (abe_port[id]).protocol.p.prot_pingpong.irq_data = (1 << d);

  abe_port[id].status = OMAP_ABE_PORT_ACTIVITY_RUNNING;

  /* load the micro-task parameters DESC_IO_PP */
  abe_init_io_tasks(id, &((abe_port[id]).format),
		    &((abe_port[id]).protocol));

  /* load the dma_t with physical information from AE memory mapping */
  abe_init_dma_t(id, &((abe_port[id]).protocol));

  dma1.data = (u32 *) (abe_port[id].dma.data + ABE_DMEM_BASE_ADDRESS_L3);
  dma1.iter = abe_port[id].dma.iter;
  *returned_dma_t = dma1;

  return 0;
}

EXPORT_SYMBOL(abe_connect_dmareq_ping_pong_port);

/**
 * abe_connect_irq_ping_pong_port
 * @id: port name
 * @f: desired data format
 * @I: index of the call-back subroutine to call
 * @s: half-buffer (ping) size
 * @p: returned base address of the first (ping) buffer)
 *
 * enables the data echanges between a direct access to the DMEM
 * memory of ABE using cache flush. On each IRQ activation a subroutine
 * registered with "abe_plug_subroutine" will be called. This subroutine
 * will generate an amount of samples, send them to DMEM memory and call
 * "abe_set_ping_pong_buffer" to notify the new amount of samples in the
 * pong buffer.
 */
abehal_status
abe_connect_irq_ping_pong_port(u32 id, abe_data_format_t * f,
			       u32 subroutine_id, u32 size,
			       u32 * sink, u32 dsp_mcu_flag)
{

  _log(id_connect_irq_ping_pong_port, id, f->f, f->samp_format)

    /* ping_pong is only supported on MM_DL */
    if (id != MM_DL_PORT) {
      abe_dbg_param |= ERR_API;
      abe_dbg_error_log(ABE_PARAMETER_ERROR);
    }

  abe_port[id] = ((abe_port_t *) abe_port_init)[id];
  (abe_port[id]).format = (*f);
  (abe_port[id]).protocol.protocol_switch = PINGPONG_PORT_PROT;
  (abe_port[id]).protocol.p.prot_pingpong.buf_addr =
    dmem_ping_pong_buffer;
  (abe_port[id]).protocol.p.prot_pingpong.buf_size = size;
  (abe_port[id]).protocol.p.prot_pingpong.irq_data = (1);

  abe_init_ping_pong_buffer(MM_DL_PORT, size, 2, sink);

  if (dsp_mcu_flag == PING_PONG_WITH_MCU_IRQ)
    (abe_port[id]).protocol.p.prot_pingpong.irq_addr =
      ABE_MCU_IRQSTATUS_RAW;

  if (dsp_mcu_flag == PING_PONG_WITH_DSP_IRQ)
    (abe_port[id]).protocol.p.prot_pingpong.irq_addr =
      ABE_DSP_IRQSTATUS_RAW;

  abe_port[id].status = OMAP_ABE_PORT_ACTIVITY_RUNNING;

  /* load the micro-task parameters */
  abe_init_io_tasks(id, &((abe_port[id]).format),
		    &((abe_port[id]).protocol));

  /* load the ATC descriptors - disabled */
  abe_init_atc(id);

  *sink = (abe_port[id]).protocol.p.prot_pingpong.buf_addr;

  return 0;
}

EXPORT_SYMBOL(abe_connect_irq_ping_pong_port);

/**
 * abe_connect_serial_port()
 * @id: port name
 * @f: data format
 * @i: peripheral ID (McBSP #1, #2, #3)
 *
 * Operations : enables the data echanges between a McBSP and an ATC buffer in
 * DMEM. This API is used connect 48kHz McBSP streams to MM_DL and 8/16kHz
 * voice streams to VX_UL, VX_DL, BT_VX_UL, BT_VX_DL. It abstracts the
 * abe_write_port API.
 */
abehal_status
abe_connect_serial_port(u32 id, abe_data_format_t * f, u32 mcbsp_id)
{
  u32 UC_NULL[] = { 0 };
  u32 OPP;
  abe_hw_config_init_t CONFIG;

  _log(id_connect_serial_port, id, f->samp_format, mcbsp_id)

    abe_port[id] = ((abe_port_t *) abe_port_init)[id];
  (abe_port[id]).format = (*f);

  (abe_port[id]).protocol.protocol_switch = SERIAL_PORT_PROT;

  /* McBSP peripheral connected to ATC */
  (abe_port[id]).protocol.p.prot_serial.desc_addr = mcbsp_id * ATC_SIZE;

  /* check the iteration of ATC */
  abe_read_hardware_configuration(UC_NULL, &OPP, &CONFIG);

  (abe_port[id]).protocol.p.prot_serial.iter =
    abe_dma_port_iter_factor(f);

  abe_port[id].status = OMAP_ABE_PORT_ACTIVITY_RUNNING;

  /* load the micro-task parameters */
  abe_init_io_tasks(id, &((abe_port[id]).format),
		    &((abe_port[id]).protocol));

  /* load the ATC descriptors - disabled */
  abe_init_atc(id);
  return 0;
}

EXPORT_SYMBOL(abe_connect_serial_port);

/**
 * abe_connect_slimbus_port
 * @id: port name
 * @f: data format
 * @i: peripheral ID (McBSP #1, #2, #3)
 * @j: peripheral ID (McBSP #1, #2, #3)
 *
 * enables the data echanges between 1/2 SB and an ATC buffers in
 * DMEM.
 */
abehal_status
abe_connect_slimbus_port(u32 id, abe_data_format_t * f,
			 u32 sb_port1, u32 sb_port2)
{
  u32 UC_NULL[] = { 0 };
  u32 OPP;
  abe_hw_config_init_t CONFIG;
  u32 iter;

  _log(id_connect_slimbus_port, id, f->samp_format, sb_port2)

    abe_port[id] = ((abe_port_t *) abe_port_init)[id];
  (abe_port[id]).format = (*f);
  (abe_port[id]).protocol.protocol_switch = SLIMBUS_PORT_PROT;

  /* SB1 peripheral connected to ATC */
  (abe_port[id]).protocol.p.prot_slimbus.desc_addr1 = sb_port1 * ATC_SIZE;

  /* SB2 peripheral connected to ATC */
  (abe_port[id]).protocol.p.prot_slimbus.desc_addr2 = sb_port2 * ATC_SIZE;

  /* check the iteration of ATC */
  abe_read_hardware_configuration(UC_NULL, &OPP, &CONFIG);
  iter = CONFIG.SLIMBUS_DCT_FIFO_SETUP_REG__SB_THRESHOLD;

  /* SLIMBUS iter should be 1 */
  (abe_port[id]).protocol.p.prot_serial.iter = iter;

  abe_port[id].status = OMAP_ABE_PORT_ACTIVITY_RUNNING;

  /* load the micro-task parameters */
  abe_init_io_tasks(id, &((abe_port[id]).format),
		    &((abe_port[id]).protocol));

  /* load the ATC descriptors - disabled */
  abe_init_atc(id);

  return 0;
}

EXPORT_SYMBOL(abe_connect_slimbus_port);

/**
 * abe_connect_tdm_port
 * @id: port name
 * @f: data format
 * @i: peripheral ID (McBSP #1, #2, #3)
 * @j: peripheral ID (McBSP #1, #2, #3)
 *
 * enables the data echanges between TDM McBSP ATC buffers in
 * DMEM and 1/2 SMEM buffers
 */
abehal_status abe_connect_tdm_port(u32 id, abe_data_format_t * f, u32 mcbsp_id)
{
  u32 UC_NULL[] = { 0 };
  u32 OPP;
  abe_hw_config_init_t CONFIG;
  u32 iter;

  _log(id_connect_tdm_port, id, f->samp_format, mcbsp_id)

    abe_port[id] = ((abe_port_t *) abe_port_init)[id];
  (abe_port[id]).format = (*f);
  (abe_port[id]).protocol.protocol_switch = TDM_SERIAL_PORT_PROT;
  /* McBSP peripheral connected to ATC */
  (abe_port[id]).protocol.p.prot_serial.desc_addr = mcbsp_id * ATC_SIZE;

  /* check the iteration of ATC */
  abe_read_hardware_configuration(UC_NULL, &OPP, &CONFIG);
  if (abe_port[id].protocol.direction == ABE_ATC_DIRECTION_IN)
    iter = CONFIG.MCBSP_THRSH1_REG_REG__RTHRESHOLD;
  else
    iter = CONFIG.MCBSP_THRSH2_REG_REG__XTHRESHOLD;
  /* McBSP iter should be 1 */
  (abe_port[id]).protocol.p.prot_serial.iter = iter;

  abe_port[id].status = OMAP_ABE_PORT_ACTIVITY_RUNNING;

  /* load the micro-task parameters */
  abe_init_io_tasks(id, &((abe_port[id]).format),
		    &((abe_port[id]).protocol));

  /* load the ATC descriptors - disabled */
  abe_init_atc(id);

  return 0;
}

EXPORT_SYMBOL(abe_connect_tdm_port);

/**
 * abe_read_port_address
 * @dma: output pointer to the DMA iteration and data destination pointer
 *
 * This API returns the address of the DMA register used on this audio port.
 * Depending on the protocol being used, adds the base address offset L3
 * (DMA) or MPU (ARM)
 */
abehal_status abe_read_port_address(u32 port, abe_dma_t * dma2)
{
  abe_dma_t_offset dma1;
  u32 protocol_switch;

  _log(id_read_port_address, port, 0, 0)

    dma1 = (abe_port[port]).dma;
  protocol_switch = abe_port[port].protocol.protocol_switch;

  switch (protocol_switch) {
  case PINGPONG_PORT_PROT:
    /* return the base address of the buffer in L3 and L4 spaces */
    (*dma2).data = (void *)(dma1.data + ABE_DMEM_BASE_ADDRESS_L3);
    (*dma2).l3_dmem =
      (void *)(dma1.data + ABE_DMEM_BASE_ADDRESS_L3);
    (*dma2).l4_dmem =
      (void *)(dma1.data + ABE_DMEM_BASE_ADDRESS_L4);
    break;
  case DMAREQ_PORT_PROT:
    /* return the CBPr(L3), DMEM(L3), DMEM(L4) address */
    (*dma2).data = (void *)(dma1.data + ABE_ATC_BASE_ADDRESS_L3);
    (*dma2).l3_dmem =
      (void *)((abe_port[port]).protocol.p.prot_dmareq.buf_addr +
	       ABE_DMEM_BASE_ADDRESS_L3);
    (*dma2).l4_dmem =
      (void *)((abe_port[port]).protocol.p.prot_dmareq.buf_addr +
	       ABE_DMEM_BASE_ADDRESS_L4);
    break;
  default:
    break;
  }
  (*dma2).iter = (dma1.iter);
  return 0;
}

EXPORT_SYMBOL(abe_read_port_address);

/**
 * abe_write_equalizer
 * @id: name of the equalizer
 * @param : equalizer coefficients
 *
 * Load the coefficients in CMEM.
 */
abehal_status abe_write_equalizer(u32 id, abe_equ_t * param)
{
  u32 eq_offset, length, *src;

  _log(id_write_equalizer, id, 0, 0)

    switch (id) {
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
  src = (u32 *) ((param->coef).type1);

  /* translate in bytes */
  eq_offset <<= 2;

  /* translate in bytes */
  length <<= 2;
  abe_block_copy(COPY_FROM_HOST_TO_ABE, ABE_CMEM, eq_offset, src, length);

  return 0;
}

EXPORT_SYMBOL(abe_write_equalizer);

/**
 * abe_write_asrc
 * @id: name of the port
 * @param: drift value to compensate [ppm]
 *
 * Load the drift variables to the FW memory. This API can be called only
 * when the corresponding port has been already opened and the ASRC has
 * been correctly initialized with API abe_init_asrc_... If this API is
 * used such that the drift has been changed from positive to negative drift
 * or vice versa, there will be click in the output signal. Loading the drift
 * value with zero disables the feature.
 */
abehal_status abe_write_asrc(u32 port, s32 dppm)
{
  s32 dtempvalue, adppm, drift_sign, drift_sign_addr, alpha_params_addr;
  s32 alpha_params[3];

  _log(id_write_asrc, port, dppm, dppm >> 8)

    /*
     * x = ppm
     *
     * - 1000000/x must be multiple of 16
     * - deltaalpha = round(2^20*x*16/1000000)=round(2^18/5^6*x) on 22 bits.
     *      then shifted by 2bits
     * - minusdeltaalpha
     * - oneminusepsilon = 1-deltaalpha/2.
     *
     * ppm = 250
     * - 1000000/250=4000
     * - deltaalpha = 4194.3 ~ 4195 => 0x00418c
     */
    /* examples for -6250 ppm */
    /* atempvalue32[1] = -1;  d_driftsign */
    /* atempvalue32[3] = 0x00066668;  d_deltaalpha */
    /* atempvalue32[4] = 0xfff99998;  d_minusdeltaalpha */
    /* atempvalue32[5] = 0x003ccccc;  d_oneminusepsilon */
    /* example for 100 ppm */
    /* atempvalue32[1] = 1;* d_driftsign */
    /* atempvalue32[3] = 0x00001a38;  d_deltaalpha */
    /* atempvalue32[4] = 0xffffe5c8;  d_minusdeltaalpha */
    /* atempvalue32[5] = 0x003ccccc;  d_oneminusepsilon */
    /* compute new value for the ppm */
    if (dppm >= 0) {
      /* d_driftsign */
      drift_sign = 1;
      adppm = dppm;
    } else {
      /* d_driftsign */
      drift_sign = -1;
      adppm = (-1 * dppm);
    }
  if (dppm == 0) {
    /* delta_alpha */
    alpha_params[0] = 0;

    /* minusdelta_alpha */
    alpha_params[1] = 0;

    /* one_minusepsilon */
    alpha_params[2] = 0x003ffff0;
  } else {
    dtempvalue = (adppm << 4) + adppm - ((adppm * 3481L) / 15625L);

    /* delta_alpha */
    alpha_params[0] = dtempvalue << 2;

    /* minusdelta_alpha */
    alpha_params[1] = (-dtempvalue) << 2;

    /* one_minusepsilon */
    alpha_params[2] = (0x00100000 - (dtempvalue / 2)) << 2;
  }

  switch (port) {

    /* asynchronous sample-rate-converter for the uplink voice path */
  case VX_DL_PORT:
    drift_sign_addr = D_AsrcVars_DL_VX_ADDR + 1 * sizeof(s32);
    alpha_params_addr = D_AsrcVars_DL_VX_ADDR + 3 * sizeof(s32);
    break;

    /* asynchronous sample-rate-converter for the downlink voice path */
  case VX_UL_PORT:
    drift_sign_addr = D_AsrcVars_UL_VX_ADDR + 1 * sizeof(s32);
    alpha_params_addr = D_AsrcVars_UL_VX_ADDR + 3 * sizeof(s32);
    break;

  default:

    /* asynchronous sample-rate-converter for the multimedia player */
  case MM_DL_PORT:
    drift_sign_addr = D_AsrcVars_DL_MM_ADDR + 1 * sizeof(s32);
    alpha_params_addr = D_AsrcVars_DL_MM_ADDR + 3 * sizeof(s32);
    break;
  }

  abe_block_copy(COPY_FROM_HOST_TO_ABE, ABE_DMEM, drift_sign_addr,
		 (u32 *) & drift_sign, 4);
  abe_block_copy(COPY_FROM_HOST_TO_ABE, ABE_DMEM, alpha_params_addr,
		 (u32 *) & alpha_params[0], 12);

  return 0;
}

EXPORT_SYMBOL(abe_write_asrc);

/**
 * abe_write_aps
 * @id: name of the aps filter
 * @param: table of filter coefficients
 *
 * Load the filters and thresholds coefficients in FW memory. This API
 * can be called when the corresponding APS is not activated. After
 * reloading the firmware the default coefficients corresponds to "no APS
 * activated".
 * Loading all the coefficients value with zero disables the feature.
 */
abehal_status abe_write_aps(u32 id, abe_aps_t * param)
{
  _log(id_write_aps, id, 0, 0)
    return 0;
}
EXPORT_SYMBOL(abe_write_aps);

int abe_get_gain_index(u32 id, u32 p)
{
	int index;

	switch (id) {
	default:
	case GAINS_DMIC1:
		index = 0;
		break;
	case GAINS_DMIC2:
		index = 2;
		break;
	case GAINS_DMIC3:
		index = 4;
		break;
	case GAINS_AMIC:
		index = 6;
		break;
	case GAINS_DL1:
		index = 8;
		break;
	case GAINS_DL2:
		index = 10;
		break;
	case GAINS_SPLIT:
		index = 12;
		break;
	case MIXDL1:
		index = 14;
		break;
	case MIXDL2:
		index = 18;
		break;
	case MIXECHO:
		index = 22;
		break;
	case MIXSDT:
		index = 24;
		break;
	case MIXVXREC:
		index = 26;
		break;
	case MIXAUDUL:
		index = 30;
		break;
	}
	index += p;

	return(index);

}

/**
 * abe_mute_gain
 * @id: name of the mixer
 * @p: port corresponding to the above gains
 *
 * Mute ABE gain
 */
abehal_status abe_mute_gain(u32 id, u32 p)
{
	int index;

	index = abe_get_gain_index(id, p);

	if (abe_gain_table[index].mute == 0) {
		abe_gain_table[index].mute = 1;
		abe_read_gain(id, &abe_gain_table[index].gain, p);
		abe_update_gain(id, MUTE_GAIN, RAMP_5MS, p);
	} else
		abe_gain_table[index].mute = 1;
	return 0;

}
EXPORT_SYMBOL(abe_mute_gain);


/**
 * abe_unmute_gain
 * @id: name of the mixer
 * @p: port corresponding to the above gains
 *
 * UnMute ABE gain
 */
abehal_status abe_unmute_gain(u32 id, u32 p)
{
	int index;

	index = abe_get_gain_index(id, p);

	abe_gain_table[index].mute = 0;
	abe_update_gain(id, abe_gain_table[index].gain, RAMP_5MS, p);

	return 0;
}
EXPORT_SYMBOL(abe_unmute_gain);

/**
 * abe_update_gain
 * @id: name of the mixer
 * @param: list of input gains of the mixer
 * @p: list of port corresponding to the above gains
 *
 * Load the gain coefficients in FW memory. This API can be called when
 * the corresponding MIXER is not activated. After reloading the firmware
 * the default coefficients corresponds to "all input and output mixer's gain
 * in mute state". A mixer is disabled with a network reconfiguration
 * corresponding to an OPP value.
 */
abehal_status abe_update_gain(u32 id, u32 f_g, u32 ramp, u32 p)
{
  u32 lin_g, mixer_target, mixer_offset;
  s32 gain_index;
  u32 alpha, beta;
  u32 ramp_index;

  _log(id_write_gain, id, f_g, p)

    gain_index = ((f_g - min_mdb) / 100);
  gain_index = maximum(gain_index, 0);
  gain_index = minimum(gain_index, sizeof_db2lin_table);

  lin_g = abe_db2lin_table[gain_index];

  switch (id) {
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

  /* SMEM word32 address */
  mixer_target = (S_GTarget1_ADDR << 1);
  mixer_target += mixer_offset;
  mixer_target += p;

  /* translate coef address in Bytes */
  mixer_target <<= 2;

  /* load the S_G_Target SMEM table */
  abe_block_copy(COPY_FROM_HOST_TO_ABE, ABE_SMEM, mixer_target,
		 (u32 *) & lin_g, sizeof(lin_g));

  ramp = maximum(minimum(RAMP_MAXLENGTH, ramp), RAMP_MINLENGTH);

  /* ramp data should be interpolated in the table instead */
  if (ramp < RAMP_5MS)
    ramp_index = 8;
  if ((RAMP_5MS <= ramp) && (ramp < RAMP_50MS))
    ramp_index = 24;
  if ((RAMP_50MS <= ramp) && (ramp < RAMP_500MS))
    ramp_index = 36;
  if (ramp > RAMP_500MS)
    ramp_index = 48;

  beta = abe_alpha_iir[ramp_index];
  alpha = abe_1_alpha_iir[ramp_index];

  /* CMEM word32 address */
  mixer_target = C_1_Alpha_ADDR;

  /* a pair of gains is updated once in the firmware */
  mixer_target += (p + mixer_offset) >> 1;

  /* translate coef address in Bytes */
  mixer_target <<= 2;

  /* load the ramp delay data */
  abe_block_copy(COPY_FROM_HOST_TO_ABE, ABE_CMEM, mixer_target,
		 (u32 *) & alpha, sizeof(alpha));

  /* CMEM word32 address */
  mixer_target = C_Alpha_ADDR;

  /* a pair of gains is updated once in the firmware */
  mixer_target += (p + mixer_offset) >> 1;

  /* translate coef address in Bytes */
  mixer_target <<= 2;
  abe_block_copy(COPY_FROM_HOST_TO_ABE, ABE_CMEM, mixer_target,
		 (u32 *) & beta, sizeof(beta));

  return 0;
}
EXPORT_SYMBOL(abe_update_gain);

abehal_status abe_write_gain (u32 id, u32 f_g, u32 ramp, u32 p)
{
	int index;

	index = abe_get_gain_index(id, p);

	if (abe_gain_table[index].mute == 0) {
		abe_gain_table[index].gain = f_g;
		abe_update_gain(id, f_g,ramp, p);
	} else
		abe_gain_table[index].gain = f_g;

	return 0;
}
EXPORT_SYMBOL(abe_write_gain);

/**
 * abe_write_mixer
 * @id: name of the mixer
 * @param: input gains and delay ramp of the mixer
 * @p: port corresponding to the above gains
 *
 * Load the gain coefficients in FW memory. This API can be called when
 * the corresponding MIXER is not activated. After reloading the firmware
 * the default coefficients corresponds to "all input and output mixer's
 * gain in mute state". A mixer is disabled with a network reconfiguration
 * corresponding to an OPP value.
 */
abehal_status abe_write_mixer(u32 id, u32 f_g, u32 f_ramp, u32 p)
{
  _log(id_write_mixer, id, f_ramp, p)

    abe_write_gain(id, f_g, f_ramp, p);
  return 0;
}

EXPORT_SYMBOL(abe_write_mixer);

/**
 * abe_read_gain
 * @id: name of the mixer
 * @param: list of input gains of the mixer
 * @p: list of port corresponding to the above gains
 *
 */
abehal_status abe_read_gain(u32 id, u32 * f_g, u32 p)
{
  u32 mixer_target, mixer_offset;
       int i;

  _log(id_read_gain, id, (u32) f_g, p)

    switch (id) {
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

  /* SMEM word32 address */
  mixer_target = (S_GTarget1_ADDR << 1);
  mixer_target += mixer_offset;
  mixer_target += p;

  /* translate coef address in Bytes */
  mixer_target <<= 2;

	/* load the S_G_Target SMEM table */
 	abe_block_copy(COPY_FROM_ABE_TO_HOST, ABE_SMEM, mixer_target,
		 (u32 *) f_g, sizeof(*f_g));

       for (i = 0; i < sizeof_db2lin_table; i++) {
               if (abe_db2lin_table[i] == *f_g)
                       goto found;
       }
       *f_g = 0;
       return -1;

found:
       *f_g = (i * 100) + min_mdb;

       return 0;
}
EXPORT_SYMBOL(abe_read_gain);

/**
 * abe_read_mixer
 * @id: name of the mixer
 * @param: gains of the mixer
 * @p: port corresponding to the above gains
 *
 * Load the gain coefficients in FW memory. This API can be called when
 * the corresponding MIXER is not activated. After reloading the firmware
 * the default coefficients corresponds to "all input and output mixer's
 * gain in mute state". A mixer is disabled with a network reconfiguration
 * corresponding to an OPP value.
 */
abehal_status abe_read_mixer(u32 id, u32 * f_g, u32 p)
{
  _log(id_read_mixer, id, 0, p)

    abe_read_gain(id, f_g, p);
  return 0;
}

EXPORT_SYMBOL(abe_read_mixer);

/**
 * abe_set_router_configuration
 * @Id: name of the router
 * @Conf: id of the configuration
 * @param: list of output index of the route
 *
 * The uplink router takes its input from DMIC (6 samples), AMIC (2 samples)
 * and PORT1/2 (2 stereo ports). Each sample will be individually stored in
 * an intermediate table of 10 elements.
 *
 * Example of router table parameter for voice uplink with phoenix microphones 
 * 
 * indexes 0 .. 9 = MM_UL description (digital MICs and MMEXTIN)
 *	DMIC1_L_labelID, DMIC1_R_labelID, DMIC2_L_labelID, DMIC2_R_labelID,
 *	MM_EXT_IN_L_labelID, MM_EXT_IN_R_labelID, ZERO_labelID, ZERO_labelID,
 *	ZERO_labelID, ZERO_labelID,
 * indexes 10 .. 11 = MM_UL2 description (recording on DMIC3)
 *	DMIC3_L_labelID, DMIC3_R_labelID,
 * indexes 12 .. 13 = VX_UL description (VXUL based on PDMUL data)
 *	AMIC_L_labelID, AMIC_R_labelID,
 * indexes 14 .. 15 = RESERVED (NULL)
 *	ZERO_labelID, ZERO_labelID,
 */
abehal_status abe_set_router_configuration(u32 id, u32 k, u16 * param)
{

  _log(id_set_router_configuration, id, (u32) param, (u32) param >> 8)

    abe_block_copy(COPY_FROM_HOST_TO_ABE, ABE_DMEM,
		   D_aUplinkRouting_ADDR, param,
		   D_aUplinkRouting_sizeof);

  return 0;
}

EXPORT_SYMBOL(abe_set_router_configuration);

/**
 * abe_select_data_source
 * @@@
 */
abehal_status abe_select_data_source(u32 port_id, u32 smem_source)
{
  ABE_SIODescriptor desc;
  u32 sio_desc_address;

  _log(id_select_data_source, port_id, smem_source, smem_source >> 8)

    sio_desc_address = dmem_port_descriptors + (port_id *
						sizeof
						(ABE_SIODescriptor));
  abe_block_copy(COPY_FROM_ABE_TO_HOST, ABE_DMEM, sio_desc_address,
		 (u32 *) & desc, sizeof(desc));
  desc.smem_addr1 = (u16) smem_source;
  abe_block_copy(COPY_FROM_HOST_TO_ABE, ABE_DMEM, sio_desc_address,
		 (u32 *) & desc, sizeof(desc));

  return 0;
}

EXPORT_SYMBOL(abe_select_data_source);

/**
 * ABE_READ_DEBUG_TRACE
 *
 * Parameter :
 *	data destination pointer
 *	max number of data read
 *
 * Operations :
 *	reads the AE circular data pointer holding pairs of debug data+
 * 	timestamps, and store the pairs in linear addressing to the parameter
 * 	pointer. Stops the copy when the max parameter is reached or when the
 * 	FIFO is empty.
 *
 * Return value :
 *	None.
 */
abehal_status abe_read_debug_trace(u32 * data, u32 * n)
{
  _log(id_select_data_source, 0, 0, 0)
    return 0;
}

EXPORT_SYMBOL(abe_read_debug_trace);

/**
 * abe_connect_debug_trace
 * @dma2:pointer to the DMEM trace buffer
 *
 * returns the address and size of the real-time debug trace buffer,
 * the content of which will vary from one firmware release to an other
 */
abehal_status abe_connect_debug_trace(abe_dma_t * dma2)
{

  _log(id_connect_debug_trace, 0, 0, 0)

    /* return the base address of the ping buffer in L3 and L4 spaces */
    (*dma2).data =
    (void *)(D_DEBUG_FIFO_ADDR + ABE_DMEM_BASE_ADDRESS_L3);
  (*dma2).l3_dmem =
    (void *)(D_DEBUG_FIFO_ADDR + ABE_DMEM_BASE_ADDRESS_L3);
  (*dma2).l4_dmem =
    (void *)(D_DEBUG_FIFO_ADDR + ABE_DMEM_BASE_ADDRESS_L4);
  (*dma2).iter = D_DEBUG_FIFO_sizeof;

  return 0;
}

EXPORT_SYMBOL(abe_connect_debug_trace);

/**
 * abe_set_debug_trace
 * @debug: debug ID from a list to be defined
 *
 * load a mask which filters the debug trace to dedicated types of data
 */
abehal_status abe_set_debug_trace(abe_dbg_t debug)
{

  _log(id_set_debug_trace, 0, 0, 0)

    abe_dbg_mask = debug;
  return 0;
}

EXPORT_SYMBOL(abe_set_debug_trace);

/**
 * abe_remote_debugger_interface
 *
 * interpretation of the UART stream from the remote debugger commands.
 * The commands consist in setting break points, loading parameter
 */
abehal_status abe_remote_debugger_interface(u32 n, u8 * p)
{

  _log(id_remote_debugger_interface, n, 0, 0)
    return 0;
}

EXPORT_SYMBOL(abe_remote_debugger_interface);

/**
 * abe_enable_test_pattern
 *
 */

abehal_status abe_enable_test_pattern(u32 smem_id, u32 on_off)
{
  u16 dbg_on, dbg_off, idx_patch, task_patch, addr_patch;
  u32 patch, task32;

  _log(id_enable_test_pattern, on_off, smem_id, smem_id >> 8)

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
      return 0;
    }
  patch = (on_off != 0) ? dbg_on : dbg_off;

  /* address is on 16bits boundary */
  addr_patch = D_tasksList_ADDR + 16 * task_patch + 2 * idx_patch;

  /* read on 32bits words' boundary */
  abe_block_copy(COPY_FROM_ABE_TO_HOST, ABE_DMEM, addr_patch & (~0x03),
		 &task32, 4);

  if (addr_patch & 0x03)
    task32 = (0x0000FFFFL & task32) | (patch << 16);
  else
    task32 = (0xFFFF0000L & task32) | (0x0000FFFF & patch);

  abe_block_copy(COPY_FROM_HOST_TO_ABE, ABE_DMEM, addr_patch & (~0x03),
		 &task32, 4);

  return 0;
}

EXPORT_SYMBOL(abe_enable_test_pattern);
