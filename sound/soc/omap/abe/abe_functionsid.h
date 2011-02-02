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
#ifndef _ABE_FUNCTIONSID_H_
#define _ABE_FUNCTIONSID_H_
/*
 *    TASK function ID definitions
 */
#define C_ABE_FW_FUNCTION_IIR                               0
#define C_ABE_FW_FUNCTION_monoToStereoPack                  1
#define C_ABE_FW_FUNCTION_stereoToMonoSplit                 2
#define C_ABE_FW_FUNCTION_decimator                         3
#define C_ABE_FW_FUNCTION_OS0Fill                           4
#define C_ABE_FW_FUNCTION_mixer2                            5
#define C_ABE_FW_FUNCTION_mixer4                            6
#define C_ABE_FW_FUNCTION_inplaceGain                       7
#define C_ABE_FW_FUNCTION_StreamRouting                     8
#define C_ABE_FW_FUNCTION_gainConverge                      9
#define C_ABE_FW_FUNCTION_dualIir                           10
#define C_ABE_FW_FUNCTION_IO_DL_pp                          11
#define C_ABE_FW_FUNCTION_IO_generic                        12
#define C_ABE_FW_FUNCTION_irq_fifo_debug                    13
#define C_ABE_FW_FUNCTION_synchronize_pointers              14
#define C_ABE_FW_FUNCTION_VIBRA2                            15
#define C_ABE_FW_FUNCTION_VIBRA1                            16
#define C_ABE_FW_FUNCTION_APS_core                          17
#define C_ABE_FW_FUNCTION_IIR_SRC_MIC                       18
#define C_ABE_FW_FUNCTION_wrappers                          19
#define C_ABE_FW_FUNCTION_ASRC_DL_wrapper                   20
#define C_ABE_FW_FUNCTION_ASRC_UL_wrapper                   21
#define C_ABE_FW_FUNCTION_mem_init                          22
#define C_ABE_FW_FUNCTION_debug_vx_asrc                     23
#define C_ABE_FW_FUNCTION_IIR_SRC2                          24
/*
 *    COPY function ID definitions
 */
#define NULL_COPY_CFPID                                     0
#define S2D_STEREO_16_16_CFPID                              1
#define S2D_MONO_MSB_CFPID                                  2
#define S2D_STEREO_MSB_CFPID                                3
#define S2D_STEREO_RSHIFTED_16_CFPID                        4
#define S2D_MONO_RSHIFTED_16_CFPID                          5
#define D2S_STEREO_16_16_CFPID                              6
#define D2S_MONO_MSB_CFPID                                  7
#define D2S_MONO_RSHIFTED_16_CFPID                          8
#define D2S_STEREO_RSHIFTED_16_CFPID                        9
#define D2S_STEREO_MSB_CFPID                                10
#define COPY_DMIC_CFPID                                     11
#define COPY_MCPDM_DL_CFPID                                 12
#define COPY_MM_UL_CFPID                                    13
#define SPLIT_SMEM_CFPID                                    14
#define MERGE_SMEM_CFPID                                    15
#define SPLIT_TDM_CFPID                                     16
#define MERGE_TDM_CFPID                                     17
#define ROUTE_MM_UL_CFPID                                   18
#define IO_IP_CFPID                                         19
#define COPY_UNDERFLOW_CFPID                                20
#endif /* _ABE_FUNCTIONSID_H_ */
