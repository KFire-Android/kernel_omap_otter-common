/*
 *
 * This file is provided under a dual BSD/GPLv2 license.  When using or
 * redistributing this file, you may do so under either license.
 *
 * GPL LICENSE SUMMARY
 *
 * Copyright(c) 2010-2011 Texas Instruments Incorporated,
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 * The full GNU General Public License is included in this distribution
 * in the file called LICENSE.GPL.
 *
 * BSD LICENSE
 *
 * Copyright(c) 2010-2011 Texas Instruments Incorporated,
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *   distribution.
 * * Neither the name of Texas Instruments Incorporated nor the names of
 *   its contributors may be used to endorse or promote products derived
 *   from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
*/
#ifndef _ABE_DM_ADDR_H_
#define _ABE_DM_ADDR_H_
#define D_atcDescriptors_ADDR                               0
#define D_atcDescriptors_ADDR_END                           511
#define D_atcDescriptors_sizeof                             512
#define stack_ADDR                                          512
#define stack_ADDR_END                                      623
#define stack_sizeof                                        112
#define D_version_ADDR                                      624
#define D_version_ADDR_END                                  627
#define D_version_sizeof                                    4
#define D_BT_DL_FIFO_ADDR                                   1024
#define D_BT_DL_FIFO_ADDR_END                               1503
#define D_BT_DL_FIFO_sizeof                                 480
#define D_BT_UL_FIFO_ADDR                                   1536
#define D_BT_UL_FIFO_ADDR_END                               2015
#define D_BT_UL_FIFO_sizeof                                 480
#define D_MM_EXT_OUT_FIFO_ADDR                              2048
#define D_MM_EXT_OUT_FIFO_ADDR_END                          2527
#define D_MM_EXT_OUT_FIFO_sizeof                            480
#define D_MM_EXT_IN_FIFO_ADDR                               2560
#define D_MM_EXT_IN_FIFO_ADDR_END                           3039
#define D_MM_EXT_IN_FIFO_sizeof                             480
#define D_MM_UL2_FIFO_ADDR                                  3072
#define D_MM_UL2_FIFO_ADDR_END                              3551
#define D_MM_UL2_FIFO_sizeof                                480
#define D_VX_UL_FIFO_ADDR                                   3584
#define D_VX_UL_FIFO_ADDR_END                               4063
#define D_VX_UL_FIFO_sizeof                                 480
#define D_VX_DL_FIFO_ADDR                                   4096
#define D_VX_DL_FIFO_ADDR_END                               4575
#define D_VX_DL_FIFO_sizeof                                 480
#define D_DMIC_UL_FIFO_ADDR                                 4608
#define D_DMIC_UL_FIFO_ADDR_END                             5087
#define D_DMIC_UL_FIFO_sizeof                               480
#define D_MM_UL_FIFO_ADDR                                   5120
#define D_MM_UL_FIFO_ADDR_END                               5599
#define D_MM_UL_FIFO_sizeof                                 480
#define D_MM_DL_FIFO_ADDR                                   5632
#define D_MM_DL_FIFO_ADDR_END                               6111
#define D_MM_DL_FIFO_sizeof                                 480
#define D_TONES_DL_FIFO_ADDR                                6144
#define D_TONES_DL_FIFO_ADDR_END                            6623
#define D_TONES_DL_FIFO_sizeof                              480
#define D_VIB_DL_FIFO_ADDR                                  6656
#define D_VIB_DL_FIFO_ADDR_END                              7135
#define D_VIB_DL_FIFO_sizeof                                480
#define D_McPDM_DL_FIFO_ADDR                                7168
#define D_McPDM_DL_FIFO_ADDR_END                            7647
#define D_McPDM_DL_FIFO_sizeof                              480
#define D_McPDM_UL_FIFO_ADDR                                7680
#define D_McPDM_UL_FIFO_ADDR_END                            8159
#define D_McPDM_UL_FIFO_sizeof                              480
#define D_DEBUG_FIFO_ADDR                                   8160
#define D_DEBUG_FIFO_ADDR_END                               8255
#define D_DEBUG_FIFO_sizeof                                 96
#define D_DEBUG_FIFO_HAL_ADDR                               8256
#define D_DEBUG_FIFO_HAL_ADDR_END                           8287
#define D_DEBUG_FIFO_HAL_sizeof                             32
#define D_IOdescr_ADDR                                      8288
#define D_IOdescr_ADDR_END                                  8927
#define D_IOdescr_sizeof                                    640
#define d_zero_ADDR                                         8928
#define d_zero_ADDR_END                                     8931
#define d_zero_sizeof                                       4
#define dbg_trace1_ADDR                                     8932
#define dbg_trace1_ADDR_END                                 8932
#define dbg_trace1_sizeof                                   1
#define dbg_trace2_ADDR                                     8933
#define dbg_trace2_ADDR_END                                 8933
#define dbg_trace2_sizeof                                   1
#define dbg_trace3_ADDR                                     8934
#define dbg_trace3_ADDR_END                                 8934
#define dbg_trace3_sizeof                                   1
#define D_multiFrame_ADDR                                   8936
#define D_multiFrame_ADDR_END                               9335
#define D_multiFrame_sizeof                                 400
#define D_tasksList_ADDR                                    9336
#define D_tasksList_ADDR_END                                11511
#define D_tasksList_sizeof                                  2176
#define D_idleTask_ADDR                                     11512
#define D_idleTask_ADDR_END                                 11513
#define D_idleTask_sizeof                                   2
#define D_typeLengthCheck_ADDR                              11514
#define D_typeLengthCheck_ADDR_END                          11515
#define D_typeLengthCheck_sizeof                            2
#define D_maxTaskBytesInSlot_ADDR                           11516
#define D_maxTaskBytesInSlot_ADDR_END                       11517
#define D_maxTaskBytesInSlot_sizeof                         2
#define D_rewindTaskBytes_ADDR                              11518
#define D_rewindTaskBytes_ADDR_END                          11519
#define D_rewindTaskBytes_sizeof                            2
#define D_pCurrentTask_ADDR                                 11520
#define D_pCurrentTask_ADDR_END                             11521
#define D_pCurrentTask_sizeof                               2
#define D_pFastLoopBack_ADDR                                11522
#define D_pFastLoopBack_ADDR_END                            11523
#define D_pFastLoopBack_sizeof                              2
#define D_pNextFastLoopBack_ADDR                            11524
#define D_pNextFastLoopBack_ADDR_END                        11527
#define D_pNextFastLoopBack_sizeof                          4
#define D_ppCurrentTask_ADDR                                11528
#define D_ppCurrentTask_ADDR_END                            11529
#define D_ppCurrentTask_sizeof                              2
#define D_slotCounter_ADDR                                  11532
#define D_slotCounter_ADDR_END                              11533
#define D_slotCounter_sizeof                                2
#define D_loopCounter_ADDR                                  11536
#define D_loopCounter_ADDR_END                              11539
#define D_loopCounter_sizeof                                4
#define D_RewindFlag_ADDR                                   11540
#define D_RewindFlag_ADDR_END                               11541
#define D_RewindFlag_sizeof                                 2
#define D_Slot23_ctrl_ADDR                                  11544
#define D_Slot23_ctrl_ADDR_END                              11547
#define D_Slot23_ctrl_sizeof                                4
#define D_McuIrqFifo_ADDR                                   11548
#define D_McuIrqFifo_ADDR_END                               11611
#define D_McuIrqFifo_sizeof                                 64
#define D_PingPongDesc_ADDR                                 11612
#define D_PingPongDesc_ADDR_END                             11635
#define D_PingPongDesc_sizeof                               24
#define D_PP_MCU_IRQ_ADDR                                   11636
#define D_PP_MCU_IRQ_ADDR_END                               11637
#define D_PP_MCU_IRQ_sizeof                                 2
#define D_ctrlPortFifo_ADDR                                 11648
#define D_ctrlPortFifo_ADDR_END                             11663
#define D_ctrlPortFifo_sizeof                               16
#define D_Idle_State_ADDR                                   11664
#define D_Idle_State_ADDR_END                               11667
#define D_Idle_State_sizeof                                 4
#define D_Stop_Request_ADDR                                 11668
#define D_Stop_Request_ADDR_END                             11671
#define D_Stop_Request_sizeof                               4
#define D_Ref0_ADDR                                         11672
#define D_Ref0_ADDR_END                                     11673
#define D_Ref0_sizeof                                       2
#define D_DebugRegister_ADDR                                11676
#define D_DebugRegister_ADDR_END                            11815
#define D_DebugRegister_sizeof                              140
#define D_Gcount_ADDR                                       11816
#define D_Gcount_ADDR_END                                   11817
#define D_Gcount_sizeof                                     2
#define D_fastCounter_ADDR                                  11820
#define D_fastCounter_ADDR_END                              11823
#define D_fastCounter_sizeof                                4
#define D_slowCounter_ADDR                                  11824
#define D_slowCounter_ADDR_END                              11827
#define D_slowCounter_sizeof                                4
#define D_aUplinkRouting_ADDR                               11828
#define D_aUplinkRouting_ADDR_END                           11859
#define D_aUplinkRouting_sizeof                             32
#define D_VirtAudioLoop_ADDR                                11860
#define D_VirtAudioLoop_ADDR_END                            11863
#define D_VirtAudioLoop_sizeof                              4
#define D_AsrcVars_DL_VX_ADDR                               11864
#define D_AsrcVars_DL_VX_ADDR_END                           11895
#define D_AsrcVars_DL_VX_sizeof                             32
#define D_AsrcVars_UL_VX_ADDR                               11896
#define D_AsrcVars_UL_VX_ADDR_END                           11927
#define D_AsrcVars_UL_VX_sizeof                             32
#define D_CoefAddresses_VX_ADDR                             11928
#define D_CoefAddresses_VX_ADDR_END                         11959
#define D_CoefAddresses_VX_sizeof                           32
#define D_AsrcVars_MM_EXT_IN_ADDR                           11960
#define D_AsrcVars_MM_EXT_IN_ADDR_END                       11991
#define D_AsrcVars_MM_EXT_IN_sizeof                         32
#define D_CoefAddresses_MM_ADDR                             11992
#define D_CoefAddresses_MM_ADDR_END                         12023
#define D_CoefAddresses_MM_sizeof                           32
#define D_APS_DL1_M_thresholds_ADDR                         12024
#define D_APS_DL1_M_thresholds_ADDR_END                     12031
#define D_APS_DL1_M_thresholds_sizeof                       8
#define D_APS_DL1_M_IRQ_ADDR                                12032
#define D_APS_DL1_M_IRQ_ADDR_END                            12033
#define D_APS_DL1_M_IRQ_sizeof                              2
#define D_APS_DL1_C_IRQ_ADDR                                12034
#define D_APS_DL1_C_IRQ_ADDR_END                            12035
#define D_APS_DL1_C_IRQ_sizeof                              2
#define D_TraceBufAdr_ADDR                                  12036
#define D_TraceBufAdr_ADDR_END                              12037
#define D_TraceBufAdr_sizeof                                2
#define D_TraceBufOffset_ADDR                               12038
#define D_TraceBufOffset_ADDR_END                           12039
#define D_TraceBufOffset_sizeof                             2
#define D_TraceBufLength_ADDR                               12040
#define D_TraceBufLength_ADDR_END                           12041
#define D_TraceBufLength_sizeof                             2
#define D_AsrcVars_ECHO_REF_ADDR                            12044
#define D_AsrcVars_ECHO_REF_ADDR_END                        12075
#define D_AsrcVars_ECHO_REF_sizeof                          32
#define D_Pempty_ADDR                                       12076
#define D_Pempty_ADDR_END                                   12079
#define D_Pempty_sizeof                                     4
#define D_APS_DL2_L_M_IRQ_ADDR                              12080
#define D_APS_DL2_L_M_IRQ_ADDR_END                          12081
#define D_APS_DL2_L_M_IRQ_sizeof                            2
#define D_APS_DL2_L_C_IRQ_ADDR                              12082
#define D_APS_DL2_L_C_IRQ_ADDR_END                          12083
#define D_APS_DL2_L_C_IRQ_sizeof                            2
#define D_APS_DL2_R_M_IRQ_ADDR                              12084
#define D_APS_DL2_R_M_IRQ_ADDR_END                          12085
#define D_APS_DL2_R_M_IRQ_sizeof                            2
#define D_APS_DL2_R_C_IRQ_ADDR                              12086
#define D_APS_DL2_R_C_IRQ_ADDR_END                          12087
#define D_APS_DL2_R_C_IRQ_sizeof                            2
#define D_APS_DL1_C_thresholds_ADDR                         12088
#define D_APS_DL1_C_thresholds_ADDR_END                     12095
#define D_APS_DL1_C_thresholds_sizeof                       8
#define D_APS_DL2_L_M_thresholds_ADDR                       12096
#define D_APS_DL2_L_M_thresholds_ADDR_END                   12103
#define D_APS_DL2_L_M_thresholds_sizeof                     8
#define D_APS_DL2_L_C_thresholds_ADDR                       12104
#define D_APS_DL2_L_C_thresholds_ADDR_END                   12111
#define D_APS_DL2_L_C_thresholds_sizeof                     8
#define D_APS_DL2_R_M_thresholds_ADDR                       12112
#define D_APS_DL2_R_M_thresholds_ADDR_END                   12119
#define D_APS_DL2_R_M_thresholds_sizeof                     8
#define D_APS_DL2_R_C_thresholds_ADDR                       12120
#define D_APS_DL2_R_C_thresholds_ADDR_END                   12127
#define D_APS_DL2_R_C_thresholds_sizeof                     8
#define D_ECHO_REF_48_16_WRAP_ADDR                          12128
#define D_ECHO_REF_48_16_WRAP_ADDR_END                      12135
#define D_ECHO_REF_48_16_WRAP_sizeof                        8
#define D_ECHO_REF_48_8_WRAP_ADDR                           12136
#define D_ECHO_REF_48_8_WRAP_ADDR_END                       12143
#define D_ECHO_REF_48_8_WRAP_sizeof                         8
#define D_BT_UL_16_48_WRAP_ADDR                             12144
#define D_BT_UL_16_48_WRAP_ADDR_END                         12151
#define D_BT_UL_16_48_WRAP_sizeof                           8
#define D_BT_UL_8_48_WRAP_ADDR                              12152
#define D_BT_UL_8_48_WRAP_ADDR_END                          12159
#define D_BT_UL_8_48_WRAP_sizeof                            8
#define D_BT_DL_48_16_WRAP_ADDR                             12160
#define D_BT_DL_48_16_WRAP_ADDR_END                         12167
#define D_BT_DL_48_16_WRAP_sizeof                           8
#define D_BT_DL_48_8_WRAP_ADDR                              12168
#define D_BT_DL_48_8_WRAP_ADDR_END                          12175
#define D_BT_DL_48_8_WRAP_sizeof                            8
#define D_VX_DL_16_48_WRAP_ADDR                             12176
#define D_VX_DL_16_48_WRAP_ADDR_END                         12183
#define D_VX_DL_16_48_WRAP_sizeof                           8
#define D_VX_DL_8_48_WRAP_ADDR                              12184
#define D_VX_DL_8_48_WRAP_ADDR_END                          12191
#define D_VX_DL_8_48_WRAP_sizeof                            8
#define D_VX_UL_48_16_WRAP_ADDR                             12192
#define D_VX_UL_48_16_WRAP_ADDR_END                         12199
#define D_VX_UL_48_16_WRAP_sizeof                           8
#define D_VX_UL_48_8_WRAP_ADDR                              12200
#define D_VX_UL_48_8_WRAP_ADDR_END                          12207
#define D_VX_UL_48_8_WRAP_sizeof                            8
#define D_APS_DL1_IRQs_WRAP_ADDR                            12208
#define D_APS_DL1_IRQs_WRAP_ADDR_END                        12215
#define D_APS_DL1_IRQs_WRAP_sizeof                          8
#define D_APS_DL2_L_IRQs_WRAP_ADDR                          12216
#define D_APS_DL2_L_IRQs_WRAP_ADDR_END                      12223
#define D_APS_DL2_L_IRQs_WRAP_sizeof                        8
#define D_APS_DL2_R_IRQs_WRAP_ADDR                          12224
#define D_APS_DL2_R_IRQs_WRAP_ADDR_END                      12231
#define D_APS_DL2_R_IRQs_WRAP_sizeof                        8
#define D_nextMultiFrame_ADDR                               12232
#define D_nextMultiFrame_ADDR_END                           12239
#define D_nextMultiFrame_sizeof                             8
#define D_HW_TEST_ADDR                                      12240
#define D_HW_TEST_ADDR_END                                  12247
#define D_HW_TEST_sizeof                                    8
#define D_TraceBufAdr_HAL_ADDR                              12248
#define D_TraceBufAdr_HAL_ADDR_END                          12251
#define D_TraceBufAdr_HAL_sizeof                            4
#define D_DEBUG_HAL_TASK_ADDR                               12288
#define D_DEBUG_HAL_TASK_ADDR_END                           14335
#define D_DEBUG_HAL_TASK_sizeof                             2048
#define D_DEBUG_FW_TASK_ADDR                                14336
#define D_DEBUG_FW_TASK_ADDR_END                            14591
#define D_DEBUG_FW_TASK_sizeof                              256
#define D_FwMemInit_ADDR                                    14592
#define D_FwMemInit_ADDR_END                                15551
#define D_FwMemInit_sizeof                                  960
#define D_FwMemInitDescr_ADDR                               15552
#define D_FwMemInitDescr_ADDR_END                           15567
#define D_FwMemInitDescr_sizeof                             16
#define D_AsrcVars_BT_UL_ADDR                               15568
#define D_AsrcVars_BT_UL_ADDR_END                           15599
#define D_AsrcVars_BT_UL_sizeof                             32
#define D_AsrcVars_BT_DL_ADDR                               15600
#define D_AsrcVars_BT_DL_ADDR_END                           15631
#define D_AsrcVars_BT_DL_sizeof                             32
#define D_BT_DL_48_8_OPP100_WRAP_ADDR                       15632
#define D_BT_DL_48_8_OPP100_WRAP_ADDR_END                   15639
#define D_BT_DL_48_8_OPP100_WRAP_sizeof                     8
#define D_BT_DL_48_16_OPP100_WRAP_ADDR                      15640
#define D_BT_DL_48_16_OPP100_WRAP_ADDR_END                  15647
#define D_BT_DL_48_16_OPP100_WRAP_sizeof                    8
#define D_VX_DL_8_48_FIR_WRAP_ADDR                          15648
#define D_VX_DL_8_48_FIR_WRAP_ADDR_END                      15655
#define D_VX_DL_8_48_FIR_WRAP_sizeof                        8
#define D_PING_ADDR                                         16384
#define D_PING_ADDR_END                                     40959
#define D_PING_sizeof                                       24576
#define D_PONG_ADDR                                         40960
#define D_PONG_ADDR_END                                     65535
#define D_PONG_sizeof                                       24576
#endif /* _ABEDM_ADDR_H_ */
