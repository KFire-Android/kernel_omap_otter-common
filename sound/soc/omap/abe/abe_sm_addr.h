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
#ifndef _ABE_SM_ADDR_H_
#define _ABE_SM_ADDR_H_

#define init_SM_ADDR                                        0 
#define init_SM_ADDR_END                                    303 
#define init_SM_sizeof                                      304 

#define S_Data0_ADDR                                        304 
#define S_Data0_ADDR_END                                    304 
#define S_Data0_sizeof                                      1 

#define S_Temp_ADDR                                         305 
#define S_Temp_ADDR_END                                     305 
#define S_Temp_sizeof                                       1 

#define S_PhoenixOffset_ADDR                                306 
#define S_PhoenixOffset_ADDR_END                            306 
#define S_PhoenixOffset_sizeof                              1 

#define S_GTarget1_ADDR                                     307 
#define S_GTarget1_ADDR_END                                 313 
#define S_GTarget1_sizeof                                   7 

#define S_Gtarget_DL1_ADDR                                  314 
#define S_Gtarget_DL1_ADDR_END                              315 
#define S_Gtarget_DL1_sizeof                                2 

#define S_Gtarget_DL2_ADDR                                  316 
#define S_Gtarget_DL2_ADDR_END                              317 
#define S_Gtarget_DL2_sizeof                                2 

#define S_Gtarget_Echo_ADDR                                 318 
#define S_Gtarget_Echo_ADDR_END                             318 
#define S_Gtarget_Echo_sizeof                               1 

#define S_Gtarget_SDT_ADDR                                  319 
#define S_Gtarget_SDT_ADDR_END                              319 
#define S_Gtarget_SDT_sizeof                                1 

#define S_Gtarget_VxRec_ADDR                                320 
#define S_Gtarget_VxRec_ADDR_END                            321 
#define S_Gtarget_VxRec_sizeof                              2 

#define S_Gtarget_UL_ADDR                                   322 
#define S_Gtarget_UL_ADDR_END                               323 
#define S_Gtarget_UL_sizeof                                 2 

#define S_Gtarget_unused_ADDR                               324 
#define S_Gtarget_unused_ADDR_END                           324 
#define S_Gtarget_unused_sizeof                             1 

#define S_GCurrent_ADDR                                     325 
#define S_GCurrent_ADDR_END                                 342 
#define S_GCurrent_sizeof                                   18 

#define S_GAIN_ONE_ADDR                                     343 
#define S_GAIN_ONE_ADDR_END                                 343 
#define S_GAIN_ONE_sizeof                                   1 

#define S_Tones_ADDR                                        344 
#define S_Tones_ADDR_END                                    355 
#define S_Tones_sizeof                                      12 

#define S_VX_DL_ADDR                                        356 
#define S_VX_DL_ADDR_END                                    367 
#define S_VX_DL_sizeof                                      12 

#define S_MM_UL2_ADDR                                       368 
#define S_MM_UL2_ADDR_END                                   379 
#define S_MM_UL2_sizeof                                     12 

#define S_MM_DL_ADDR                                        380 
#define S_MM_DL_ADDR_END                                    391 
#define S_MM_DL_sizeof                                      12 

#define S_DL1_M_Out_ADDR                                    392 
#define S_DL1_M_Out_ADDR_END                                403 
#define S_DL1_M_Out_sizeof                                  12 

#define S_DL2_M_Out_ADDR                                    404 
#define S_DL2_M_Out_ADDR_END                                415 
#define S_DL2_M_Out_sizeof                                  12 

#define S_Echo_M_Out_ADDR                                   416 
#define S_Echo_M_Out_ADDR_END                               427 
#define S_Echo_M_Out_sizeof                                 12 

#define S_SDT_M_Out_ADDR                                    428 
#define S_SDT_M_Out_ADDR_END                                439 
#define S_SDT_M_Out_sizeof                                  12 

#define S_VX_UL_ADDR                                        440 
#define S_VX_UL_ADDR_END                                    451 
#define S_VX_UL_sizeof                                      12 

#define S_VX_UL_M_ADDR                                      452 
#define S_VX_UL_M_ADDR_END                                  463 
#define S_VX_UL_M_sizeof                                    12 

#define S_BT_DL_ADDR                                        464 
#define S_BT_DL_ADDR_END                                    475 
#define S_BT_DL_sizeof                                      12 

#define S_BT_UL_ADDR                                        476 
#define S_BT_UL_ADDR_END                                    487 
#define S_BT_UL_sizeof                                      12 

#define S_BT_DL_8k_ADDR                                     488 
#define S_BT_DL_8k_ADDR_END                                 489 
#define S_BT_DL_8k_sizeof                                   2 

#define S_BT_DL_16k_ADDR                                    490 
#define S_BT_DL_16k_ADDR_END                                493 
#define S_BT_DL_16k_sizeof                                  4 

#define S_BT_UL_8k_ADDR                                     494 
#define S_BT_UL_8k_ADDR_END                                 495 
#define S_BT_UL_8k_sizeof                                   2 

#define S_BT_UL_16k_ADDR                                    496 
#define S_BT_UL_16k_ADDR_END                                499 
#define S_BT_UL_16k_sizeof                                  4 

#define S_SDT_F_ADDR                                        500 
#define S_SDT_F_ADDR_END                                    511 
#define S_SDT_F_sizeof                                      12 

#define S_SDT_F_data_ADDR                                   512 
#define S_SDT_F_data_ADDR_END                               520 
#define S_SDT_F_data_sizeof                                 9 

#define S_MM_DL_OSR_ADDR                                    521 
#define S_MM_DL_OSR_ADDR_END                                544 
#define S_MM_DL_OSR_sizeof                                  24 

#define S_24_zeros_ADDR                                     545 
#define S_24_zeros_ADDR_END                                 568 
#define S_24_zeros_sizeof                                   24 

#define S_DMIC1_ADDR                                        569 
#define S_DMIC1_ADDR_END                                    580 
#define S_DMIC1_sizeof                                      12 

#define S_DMIC2_ADDR                                        581 
#define S_DMIC2_ADDR_END                                    592 
#define S_DMIC2_sizeof                                      12 

#define S_DMIC3_ADDR                                        593 
#define S_DMIC3_ADDR_END                                    604 
#define S_DMIC3_sizeof                                      12 

#define S_AMIC_ADDR                                         605 
#define S_AMIC_ADDR_END                                     616 
#define S_AMIC_sizeof                                       12 

#define S_EANC_FBK_in_ADDR                                  617 
#define S_EANC_FBK_in_ADDR_END                              640 
#define S_EANC_FBK_in_sizeof                                24 

#define S_EANC_FBK_out_ADDR                                 641 
#define S_EANC_FBK_out_ADDR_END                             652 
#define S_EANC_FBK_out_sizeof                               12 

#define S_DMIC1_L_ADDR                                      653 
#define S_DMIC1_L_ADDR_END                                  664 
#define S_DMIC1_L_sizeof                                    12 

#define S_DMIC1_R_ADDR                                      665 
#define S_DMIC1_R_ADDR_END                                  676 
#define S_DMIC1_R_sizeof                                    12 

#define S_DMIC2_L_ADDR                                      677 
#define S_DMIC2_L_ADDR_END                                  688 
#define S_DMIC2_L_sizeof                                    12 

#define S_DMIC2_R_ADDR                                      689 
#define S_DMIC2_R_ADDR_END                                  700 
#define S_DMIC2_R_sizeof                                    12 

#define S_DMIC3_L_ADDR                                      701 
#define S_DMIC3_L_ADDR_END                                  712 
#define S_DMIC3_L_sizeof                                    12 

#define S_DMIC3_R_ADDR                                      713 
#define S_DMIC3_R_ADDR_END                                  724 
#define S_DMIC3_R_sizeof                                    12 

#define S_BT_UL_L_ADDR                                      725 
#define S_BT_UL_L_ADDR_END                                  736 
#define S_BT_UL_L_sizeof                                    12 

#define S_BT_UL_R_ADDR                                      737 
#define S_BT_UL_R_ADDR_END                                  748 
#define S_BT_UL_R_sizeof                                    12 

#define S_AMIC_L_ADDR                                       749 
#define S_AMIC_L_ADDR_END                                   760 
#define S_AMIC_L_sizeof                                     12 

#define S_AMIC_R_ADDR                                       761 
#define S_AMIC_R_ADDR_END                                   772 
#define S_AMIC_R_sizeof                                     12 

#define S_EANC_FBK_L_ADDR                                   773 
#define S_EANC_FBK_L_ADDR_END                               784 
#define S_EANC_FBK_L_sizeof                                 12 

#define S_EANC_FBK_R_ADDR                                   785 
#define S_EANC_FBK_R_ADDR_END                               796 
#define S_EANC_FBK_R_sizeof                                 12 

#define S_EchoRef_L_ADDR                                    797 
#define S_EchoRef_L_ADDR_END                                808 
#define S_EchoRef_L_sizeof                                  12 

#define S_EchoRef_R_ADDR                                    809 
#define S_EchoRef_R_ADDR_END                                820 
#define S_EchoRef_R_sizeof                                  12 

#define S_MM_DL_L_ADDR                                      821 
#define S_MM_DL_L_ADDR_END                                  832 
#define S_MM_DL_L_sizeof                                    12 

#define S_MM_DL_R_ADDR                                      833 
#define S_MM_DL_R_ADDR_END                                  844 
#define S_MM_DL_R_sizeof                                    12 

#define S_MM_UL_ADDR                                        845 
#define S_MM_UL_ADDR_END                                    964 
#define S_MM_UL_sizeof                                      120 

#define S_AMIC_96k_ADDR                                     965 
#define S_AMIC_96k_ADDR_END                                 988 
#define S_AMIC_96k_sizeof                                   24 

#define S_DMIC0_96k_ADDR                                    989 
#define S_DMIC0_96k_ADDR_END                                1012 
#define S_DMIC0_96k_sizeof                                  24 

#define S_DMIC1_96k_ADDR                                    1013 
#define S_DMIC1_96k_ADDR_END                                1036 
#define S_DMIC1_96k_sizeof                                  24 

#define S_DMIC2_96k_ADDR                                    1037 
#define S_DMIC2_96k_ADDR_END                                1060 
#define S_DMIC2_96k_sizeof                                  24 

#define S_UL_VX_UL_48_8K_ADDR                               1061 
#define S_UL_VX_UL_48_8K_ADDR_END                           1072 
#define S_UL_VX_UL_48_8K_sizeof                             12 

#define S_UL_VX_UL_48_16K_ADDR                              1073 
#define S_UL_VX_UL_48_16K_ADDR_END                          1084 
#define S_UL_VX_UL_48_16K_sizeof                            12 

#define S_UL_MIC_48K_ADDR                                   1085 
#define S_UL_MIC_48K_ADDR_END                               1096 
#define S_UL_MIC_48K_sizeof                                 12 

#define S_Voice_8k_UL_ADDR                                  1097 
#define S_Voice_8k_UL_ADDR_END                              1099 
#define S_Voice_8k_UL_sizeof                                3 

#define S_Voice_8k_DL_ADDR                                  1100 
#define S_Voice_8k_DL_ADDR_END                              1101 
#define S_Voice_8k_DL_sizeof                                2 

#define S_McPDM_Out1_ADDR                                   1102 
#define S_McPDM_Out1_ADDR_END                               1125 
#define S_McPDM_Out1_sizeof                                 24 

#define S_McPDM_Out2_ADDR                                   1126 
#define S_McPDM_Out2_ADDR_END                               1149 
#define S_McPDM_Out2_sizeof                                 24 

#define S_McPDM_Out3_ADDR                                   1150 
#define S_McPDM_Out3_ADDR_END                               1173 
#define S_McPDM_Out3_sizeof                                 24 

#define S_Voice_16k_UL_ADDR                                 1174 
#define S_Voice_16k_UL_ADDR_END                             1178 
#define S_Voice_16k_UL_sizeof                               5 

#define S_Voice_16k_DL_ADDR                                 1179 
#define S_Voice_16k_DL_ADDR_END                             1182 
#define S_Voice_16k_DL_sizeof                               4 

#define S_XinASRC_DL_VX_ADDR                                1183 
#define S_XinASRC_DL_VX_ADDR_END                            1222 
#define S_XinASRC_DL_VX_sizeof                              40 

#define S_XinASRC_UL_VX_ADDR                                1223 
#define S_XinASRC_UL_VX_ADDR_END                            1262 
#define S_XinASRC_UL_VX_sizeof                              40 

#define S_XinASRC_DL_MM_ADDR                                1263 
#define S_XinASRC_DL_MM_ADDR_END                            1302 
#define S_XinASRC_DL_MM_sizeof                              40 

#define S_VX_REC_ADDR                                       1303 
#define S_VX_REC_ADDR_END                                   1314 
#define S_VX_REC_sizeof                                     12 

#define S_VX_REC_L_ADDR                                     1315 
#define S_VX_REC_L_ADDR_END                                 1326 
#define S_VX_REC_L_sizeof                                   12 

#define S_VX_REC_R_ADDR                                     1327 
#define S_VX_REC_R_ADDR_END                                 1338 
#define S_VX_REC_R_sizeof                                   12 

#define S_DL2_M_L_ADDR                                      1339 
#define S_DL2_M_L_ADDR_END                                  1350 
#define S_DL2_M_L_sizeof                                    12 

#define S_DL2_M_R_ADDR                                      1351 
#define S_DL2_M_R_ADDR_END                                  1362 
#define S_DL2_M_R_sizeof                                    12 

#define S_DL2_M_LR_EQ_data_ADDR                             1363 
#define S_DL2_M_LR_EQ_data_ADDR_END                         1387 
#define S_DL2_M_LR_EQ_data_sizeof                           25 

#define S_DL1_M_EQ_data_ADDR                                1388 
#define S_DL1_M_EQ_data_ADDR_END                            1412 
#define S_DL1_M_EQ_data_sizeof                              25 

#define S_EARP_48_96_LP_data_ADDR                           1413 
#define S_EARP_48_96_LP_data_ADDR_END                       1427 
#define S_EARP_48_96_LP_data_sizeof                         15 

#define S_IHF_48_96_LP_data_ADDR                            1428 
#define S_IHF_48_96_LP_data_ADDR_END                        1442 
#define S_IHF_48_96_LP_data_sizeof                          15 

#define S_VX_UL_8_TEMP_ADDR                                 1443 
#define S_VX_UL_8_TEMP_ADDR_END                             1444 
#define S_VX_UL_8_TEMP_sizeof                               2 

#define S_VX_UL_16_TEMP_ADDR                                1445 
#define S_VX_UL_16_TEMP_ADDR_END                            1448 
#define S_VX_UL_16_TEMP_sizeof                              4 

#define S_VX_DL_8_48_LP_data_ADDR                           1449 
#define S_VX_DL_8_48_LP_data_ADDR_END                       1459 
#define S_VX_DL_8_48_LP_data_sizeof                         11 

#define S_VX_DL_8_48_HP_data_ADDR                           1460 
#define S_VX_DL_8_48_HP_data_ADDR_END                       1466 
#define S_VX_DL_8_48_HP_data_sizeof                         7 

#define S_VX_DL_16_48_LP_data_ADDR                          1467 
#define S_VX_DL_16_48_LP_data_ADDR_END                      1477 
#define S_VX_DL_16_48_LP_data_sizeof                        11 

#define S_VX_DL_16_48_HP_data_ADDR                          1478 
#define S_VX_DL_16_48_HP_data_ADDR_END                      1482 
#define S_VX_DL_16_48_HP_data_sizeof                        5 

#define S_VX_UL_48_8_LP_data_ADDR                           1483 
#define S_VX_UL_48_8_LP_data_ADDR_END                       1493 
#define S_VX_UL_48_8_LP_data_sizeof                         11 

#define S_VX_UL_48_8_HP_data_ADDR                           1494 
#define S_VX_UL_48_8_HP_data_ADDR_END                       1500 
#define S_VX_UL_48_8_HP_data_sizeof                         7 

#define S_VX_UL_48_16_LP_data_ADDR                          1501 
#define S_VX_UL_48_16_LP_data_ADDR_END                      1511 
#define S_VX_UL_48_16_LP_data_sizeof                        11 

#define S_VX_UL_48_16_HP_data_ADDR                          1512 
#define S_VX_UL_48_16_HP_data_ADDR_END                      1518 
#define S_VX_UL_48_16_HP_data_sizeof                        7 

#define S_BT_UL_8_48_LP_data_ADDR                           1519 
#define S_BT_UL_8_48_LP_data_ADDR_END                       1529 
#define S_BT_UL_8_48_LP_data_sizeof                         11 

#define S_BT_UL_8_48_HP_data_ADDR                           1530 
#define S_BT_UL_8_48_HP_data_ADDR_END                       1536 
#define S_BT_UL_8_48_HP_data_sizeof                         7 

#define S_BT_UL_16_48_LP_data_ADDR                          1537 
#define S_BT_UL_16_48_LP_data_ADDR_END                      1547 
#define S_BT_UL_16_48_LP_data_sizeof                        11 

#define S_BT_UL_16_48_HP_data_ADDR                          1548 
#define S_BT_UL_16_48_HP_data_ADDR_END                      1552 
#define S_BT_UL_16_48_HP_data_sizeof                        5 

#define S_BT_DL_48_8_LP_data_ADDR                           1553 
#define S_BT_DL_48_8_LP_data_ADDR_END                       1563 
#define S_BT_DL_48_8_LP_data_sizeof                         11 

#define S_BT_DL_48_8_HP_data_ADDR                           1564 
#define S_BT_DL_48_8_HP_data_ADDR_END                       1570 
#define S_BT_DL_48_8_HP_data_sizeof                         7 

#define S_BT_DL_48_16_LP_data_ADDR                          1571 
#define S_BT_DL_48_16_LP_data_ADDR_END                      1581 
#define S_BT_DL_48_16_LP_data_sizeof                        11 

#define S_BT_DL_48_16_HP_data_ADDR                          1582 
#define S_BT_DL_48_16_HP_data_ADDR_END                      1586 
#define S_BT_DL_48_16_HP_data_sizeof                        5 

#define S_ECHO_REF_48_8_LP_data_ADDR                        1587 
#define S_ECHO_REF_48_8_LP_data_ADDR_END                    1597 
#define S_ECHO_REF_48_8_LP_data_sizeof                      11 

#define S_ECHO_REF_48_8_HP_data_ADDR                        1598 
#define S_ECHO_REF_48_8_HP_data_ADDR_END                    1604 
#define S_ECHO_REF_48_8_HP_data_sizeof                      7 

#define S_ECHO_REF_48_16_LP_data_ADDR                       1605 
#define S_ECHO_REF_48_16_LP_data_ADDR_END                   1615 
#define S_ECHO_REF_48_16_LP_data_sizeof                     11 

#define S_ECHO_REF_48_16_HP_data_ADDR                       1616 
#define S_ECHO_REF_48_16_HP_data_ADDR_END                   1620 
#define S_ECHO_REF_48_16_HP_data_sizeof                     5 

#define S_EANC_IIR_data_ADDR                                1621 
#define S_EANC_IIR_data_ADDR_END                            1637 
#define S_EANC_IIR_data_sizeof                              17 

#define S_EANC_SignalTemp_ADDR                              1638 
#define S_EANC_SignalTemp_ADDR_END                          1658 
#define S_EANC_SignalTemp_sizeof                            21 

#define S_EANC_Input_ADDR                                   1659 
#define S_EANC_Input_ADDR_END                               1659 
#define S_EANC_Input_sizeof                                 1 

#define S_EANC_Output_ADDR                                  1660 
#define S_EANC_Output_ADDR_END                              1660 
#define S_EANC_Output_sizeof                                1 

#define S_APS_IIRmem1_ADDR                                  1661 
#define S_APS_IIRmem1_ADDR_END                              1669 
#define S_APS_IIRmem1_sizeof                                9 

#define S_APS_M_IIRmem2_ADDR                                1670 
#define S_APS_M_IIRmem2_ADDR_END                            1672 
#define S_APS_M_IIRmem2_sizeof                              3 

#define S_APS_C_IIRmem2_ADDR                                1673 
#define S_APS_C_IIRmem2_ADDR_END                            1675 
#define S_APS_C_IIRmem2_sizeof                              3 

#define S_APS_DL1_OutSamples_ADDR                           1676 
#define S_APS_DL1_OutSamples_ADDR_END                       1677 
#define S_APS_DL1_OutSamples_sizeof                         2 

#define S_APS_DL1_COIL_OutSamples_ADDR                      1678 
#define S_APS_DL1_COIL_OutSamples_ADDR_END                  1679 
#define S_APS_DL1_COIL_OutSamples_sizeof                    2 

#define S_APS_DL2_L_OutSamples_ADDR                         1680 
#define S_APS_DL2_L_OutSamples_ADDR_END                     1681 
#define S_APS_DL2_L_OutSamples_sizeof                       2 

#define S_APS_DL2_L_COIL_OutSamples_ADDR                    1682 
#define S_APS_DL2_L_COIL_OutSamples_ADDR_END                1683 
#define S_APS_DL2_L_COIL_OutSamples_sizeof                  2 

#define S_APS_DL2_R_OutSamples_ADDR                         1684 
#define S_APS_DL2_R_OutSamples_ADDR_END                     1685 
#define S_APS_DL2_R_OutSamples_sizeof                       2 

#define S_APS_DL2_R_COIL_OutSamples_ADDR                    1686 
#define S_APS_DL2_R_COIL_OutSamples_ADDR_END                1687 
#define S_APS_DL2_R_COIL_OutSamples_sizeof                  2 

#define S_XinASRC_ECHO_REF_ADDR                             1688 
#define S_XinASRC_ECHO_REF_ADDR_END                         1727 
#define S_XinASRC_ECHO_REF_sizeof                           40 

#define S_ECHO_REF_16K_ADDR                                 1728 
#define S_ECHO_REF_16K_ADDR_END                             1732 
#define S_ECHO_REF_16K_sizeof                               5 

#define S_ECHO_REF_8K_ADDR                                  1733 
#define S_ECHO_REF_8K_ADDR_END                              1735 
#define S_ECHO_REF_8K_sizeof                                3 

#define S_DL1_EQ_ADDR                                       1736 
#define S_DL1_EQ_ADDR_END                                   1747 
#define S_DL1_EQ_sizeof                                     12 

#define S_DL2_EQ_ADDR                                       1748 
#define S_DL2_EQ_ADDR_END                                   1759 
#define S_DL2_EQ_sizeof                                     12 

#define S_DL1_GAIN_out_ADDR                                 1760 
#define S_DL1_GAIN_out_ADDR_END                             1771 
#define S_DL1_GAIN_out_sizeof                               12 

#define S_DL2_GAIN_out_ADDR                                 1772 
#define S_DL2_GAIN_out_ADDR_END                             1783 
#define S_DL2_GAIN_out_sizeof                               12 

#define S_APS_DL2_L_IIRmem1_ADDR                            1784 
#define S_APS_DL2_L_IIRmem1_ADDR_END                        1792 
#define S_APS_DL2_L_IIRmem1_sizeof                          9 

#define S_APS_DL2_R_IIRmem1_ADDR                            1793 
#define S_APS_DL2_R_IIRmem1_ADDR_END                        1801 
#define S_APS_DL2_R_IIRmem1_sizeof                          9 

#define S_APS_DL2_L_M_IIRmem2_ADDR                          1802 
#define S_APS_DL2_L_M_IIRmem2_ADDR_END                      1804 
#define S_APS_DL2_L_M_IIRmem2_sizeof                        3 

#define S_APS_DL2_R_M_IIRmem2_ADDR                          1805 
#define S_APS_DL2_R_M_IIRmem2_ADDR_END                      1807 
#define S_APS_DL2_R_M_IIRmem2_sizeof                        3 

#define S_APS_DL2_L_C_IIRmem2_ADDR                          1808 
#define S_APS_DL2_L_C_IIRmem2_ADDR_END                      1810 
#define S_APS_DL2_L_C_IIRmem2_sizeof                        3 

#define S_APS_DL2_R_C_IIRmem2_ADDR                          1811 
#define S_APS_DL2_R_C_IIRmem2_ADDR_END                      1813 
#define S_APS_DL2_R_C_IIRmem2_sizeof                        3 

#define S_DL1_APS_ADDR                                      1814 
#define S_DL1_APS_ADDR_END                                  1825 
#define S_DL1_APS_sizeof                                    12 

#define S_DL2_L_APS_ADDR                                    1826 
#define S_DL2_L_APS_ADDR_END                                1837 
#define S_DL2_L_APS_sizeof                                  12 

#define S_DL2_R_APS_ADDR                                    1838 
#define S_DL2_R_APS_ADDR_END                                1849 
#define S_DL2_R_APS_sizeof                                  12 

#define S_APS_DL1_EQ_data_ADDR                              1850 
#define S_APS_DL1_EQ_data_ADDR_END                          1858 
#define S_APS_DL1_EQ_data_sizeof                            9 

#define S_APS_DL2_EQ_data_ADDR                              1859 
#define S_APS_DL2_EQ_data_ADDR_END                          1867 
#define S_APS_DL2_EQ_data_sizeof                            9 

#define S_DC_DCvalue_ADDR                                   1868 
#define S_DC_DCvalue_ADDR_END                               1868 
#define S_DC_DCvalue_sizeof                                 1 

#define S_VIBRA_ADDR                                        1869 
#define S_VIBRA_ADDR_END                                    1874 
#define S_VIBRA_sizeof                                      6 

#define S_Vibra2_in_ADDR                                    1875 
#define S_Vibra2_in_ADDR_END                                1880 
#define S_Vibra2_in_sizeof                                  6 

#define S_Vibra2_addr_ADDR                                  1881 
#define S_Vibra2_addr_ADDR_END                              1881 
#define S_Vibra2_addr_sizeof                                1 

#define S_VibraCtrl_forRightSM_ADDR                         1882 
#define S_VibraCtrl_forRightSM_ADDR_END                     1905 
#define S_VibraCtrl_forRightSM_sizeof                       24 

#define S_Rnoise_mem_ADDR                                   1906 
#define S_Rnoise_mem_ADDR_END                               1906 
#define S_Rnoise_mem_sizeof                                 1 

#define S_Ctrl_ADDR                                         1907 
#define S_Ctrl_ADDR_END                                     1924 
#define S_Ctrl_sizeof                                       18 

#define S_Vibra1_in_ADDR                                    1925 
#define S_Vibra1_in_ADDR_END                                1930 
#define S_Vibra1_in_sizeof                                  6 

#define S_Vibra1_temp_ADDR                                  1931 
#define S_Vibra1_temp_ADDR_END                              1954 
#define S_Vibra1_temp_sizeof                                24 

#define S_VibraCtrl_forLeftSM_ADDR                          1955 
#define S_VibraCtrl_forLeftSM_ADDR_END                      1978 
#define S_VibraCtrl_forLeftSM_sizeof                        24 

#define S_Vibra1_mem_ADDR                                   1979 
#define S_Vibra1_mem_ADDR_END                               1989 
#define S_Vibra1_mem_sizeof                                 11 

#define S_VibraCtrl_Stereo_ADDR                             1990 
#define S_VibraCtrl_Stereo_ADDR_END                         2013 
#define S_VibraCtrl_Stereo_sizeof                           24 

#define S_AMIC_96_48_data_ADDR                              2014 
#define S_AMIC_96_48_data_ADDR_END                          2032 
#define S_AMIC_96_48_data_sizeof                            19 

#define S_DMIC0_96_48_data_ADDR                             2033 
#define S_DMIC0_96_48_data_ADDR_END                         2051 
#define S_DMIC0_96_48_data_sizeof                           19 

#define S_DMIC1_96_48_data_ADDR                             2052 
#define S_DMIC1_96_48_data_ADDR_END                         2070 
#define S_DMIC1_96_48_data_sizeof                           19 

#define S_DMIC2_96_48_data_ADDR                             2071 
#define S_DMIC2_96_48_data_ADDR_END                         2089 
#define S_DMIC2_96_48_data_sizeof                           19 

#define S_EANC_FBK_96_48_data_ADDR                          2090 
#define S_EANC_FBK_96_48_data_ADDR_END                      2108 
#define S_EANC_FBK_96_48_data_sizeof                        19 

#define S_DBG_8K_PATTERN_ADDR                               2109 
#define S_DBG_8K_PATTERN_ADDR_END                           2110 
#define S_DBG_8K_PATTERN_sizeof                             2 

#define S_DBG_16K_PATTERN_ADDR                              2111 
#define S_DBG_16K_PATTERN_ADDR_END                          2114 
#define S_DBG_16K_PATTERN_sizeof                            4 

#define S_DBG_24K_PATTERN_ADDR                              2115 
#define S_DBG_24K_PATTERN_ADDR_END                          2120 
#define S_DBG_24K_PATTERN_sizeof                            6 

#define S_DBG_48K_PATTERN_ADDR                              2121 
#define S_DBG_48K_PATTERN_ADDR_END                          2132 
#define S_DBG_48K_PATTERN_sizeof                            12 

#define S_DBG_96K_PATTERN_ADDR                              2133 
#define S_DBG_96K_PATTERN_ADDR_END                          2156 
#define S_DBG_96K_PATTERN_sizeof                            24 

#define S_MM_EXT_IN_ADDR                                    2157 
#define S_MM_EXT_IN_ADDR_END                                2168 
#define S_MM_EXT_IN_sizeof                                  12 

#define S_MM_EXT_IN_L_ADDR                                  2169 
#define S_MM_EXT_IN_L_ADDR_END                              2180 
#define S_MM_EXT_IN_L_sizeof                                12 

#define S_MM_EXT_IN_R_ADDR                                  2181 
#define S_MM_EXT_IN_R_ADDR_END                              2192 
#define S_MM_EXT_IN_R_sizeof                                12 

#define S_MIC4_ADDR                                         2193 
#define S_MIC4_ADDR_END                                     2204 
#define S_MIC4_sizeof                                       12 

#define S_MIC4_L_ADDR                                       2205 
#define S_MIC4_L_ADDR_END                                   2216 
#define S_MIC4_L_sizeof                                     12 

#define S_MIC4_R_ADDR                                       2217 
#define S_MIC4_R_ADDR_END                                   2228 
#define S_MIC4_R_sizeof                                     12 

#define S_HW_TEST_ADDR                                      2229 
#define S_HW_TEST_ADDR_END                                  2229 
#define S_HW_TEST_sizeof                                    1 


#endif /* _ABESM_ADDR_H_ */
