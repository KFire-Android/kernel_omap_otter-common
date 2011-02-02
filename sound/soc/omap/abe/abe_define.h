/*
 * ALSA SoC OMAP ABE driver
*
 * Author:          Laurent Le Faucheur <l-le-faucheur@ti.com>
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
 *
 */
#ifndef _ABE_DEFINE_H_
#define _ABE_DEFINE_H_
#define ATC_DESCRIPTOR_NUMBER                               64
#define PROCESSING_SLOTS                                    25
#define TASK_POOL_LENGTH                                    128
#define MCU_IRQ                                            0x24
#define MCU_IRQ_SHIFT2                                     0x90
#define DMA_REQ_SHIFT2                                     0x210
#define DSP_IRQ                                            0x4c
#define IRQtag_APS                                         0x000a
#define IRQtag_COUNT                                       0x000c
#define IRQtag_PP                                          0x000d
#define DMAreq_7                                           0x0080
#define IRQ_FIFO_LENGTH                                     16
#define SDT_EQ_ORDER                                        4
#define DL_EQ_ORDER                                         12
#define MIC_FILTER_ORDER                                    4
#define GAINS_WITH_RAMP1                                    14
#define GAINS_WITH_RAMP2                                    22
#define GAINS_WITH_RAMP_TOTAL                               36
#define ASRC_MEMLENGTH                                      40
#define ASRC_UL_VX_FIR_L                                    19
#define ASRC_DL_VX_FIR_L                                    19
#define ASRC_MM_EXT_IN_FIR_L                                18
#define ASRC_margin                                         2
#define ASRC_N_8k                                           2
#define ASRC_N_16k                                          4
#define ASRC_N_48k                                          12
#define VIBRA_N                                             5
#define VIBRA1_IIR_MEMSIZE                                  11
#define SAMP_LOOP_96K                                       24
#define SAMP_LOOP_48K                                       12
#define SAMP_LOOP_16K                                       4
#define SAMP_LOOP_8K                                        2
#define INPUT_SCALE_SHIFTM2                                 5056
#define OUTPUT_SCALE_SHIFTM2                                5060
#define MUTE_SCALING                                        5064
#define ABE_PMEM                                            1
#define ABE_CMEM                                            2
#define ABE_SMEM                                            3
#define ABE_DMEM                                            4
#define ABE_ATC                                             5
#define ASRC_BT_UL_FIR_L                                    19
#define ASRC_BT_DL_FIR_L                                    19
#endif /* _ABE_DEFINE_H_ */
