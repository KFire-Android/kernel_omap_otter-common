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
#ifndef _ABE_CM_ADDR_H_
#define _ABE_CM_ADDR_H_

#define init_CM_ADDR                                        0 
#define init_CM_ADDR_END                                    303 
#define init_CM_sizeof                                      304 

#define C_Data_LSB_2_ADDR                                   304 
#define C_Data_LSB_2_ADDR_END                               304 
#define C_Data_LSB_2_sizeof                                 1 

#define C_1_Alpha_ADDR                                      305 
#define C_1_Alpha_ADDR_END                                  322 
#define C_1_Alpha_sizeof                                    18 

#define C_Alpha_ADDR                                        323 
#define C_Alpha_ADDR_END                                    340 
#define C_Alpha_sizeof                                      18 

#define C_GainsWRamp_ADDR                                   341 
#define C_GainsWRamp_ADDR_END                               354 
#define C_GainsWRamp_sizeof                                 14 

#define C_Gains_DL1M_ADDR                                   355 
#define C_Gains_DL1M_ADDR_END                               358 
#define C_Gains_DL1M_sizeof                                 4 

#define C_Gains_DL2M_ADDR                                   359 
#define C_Gains_DL2M_ADDR_END                               362 
#define C_Gains_DL2M_sizeof                                 4 

#define C_Gains_EchoM_ADDR                                  363 
#define C_Gains_EchoM_ADDR_END                              364 
#define C_Gains_EchoM_sizeof                                2 

#define C_Gains_SDTM_ADDR                                   365 
#define C_Gains_SDTM_ADDR_END                               366 
#define C_Gains_SDTM_sizeof                                 2 

#define C_Gains_VxRecM_ADDR                                 367 
#define C_Gains_VxRecM_ADDR_END                             370 
#define C_Gains_VxRecM_sizeof                               4 

#define C_Gains_ULM_ADDR                                    371 
#define C_Gains_ULM_ADDR_END                                374 
#define C_Gains_ULM_sizeof                                  4 

#define C_Gains_unused_ADDR                                 375 
#define C_Gains_unused_ADDR_END                             376 
#define C_Gains_unused_sizeof                               2 

#define C_SDT_Coefs_ADDR                                    377 
#define C_SDT_Coefs_ADDR_END                                385 
#define C_SDT_Coefs_sizeof                                  9 

#define C_CoefASRC1_VX_ADDR                                 386 
#define C_CoefASRC1_VX_ADDR_END                             404 
#define C_CoefASRC1_VX_sizeof                               19 

#define C_CoefASRC2_VX_ADDR                                 405 
#define C_CoefASRC2_VX_ADDR_END                             423 
#define C_CoefASRC2_VX_sizeof                               19 

#define C_CoefASRC3_VX_ADDR                                 424 
#define C_CoefASRC3_VX_ADDR_END                             442 
#define C_CoefASRC3_VX_sizeof                               19 

#define C_CoefASRC4_VX_ADDR                                 443 
#define C_CoefASRC4_VX_ADDR_END                             461 
#define C_CoefASRC4_VX_sizeof                               19 

#define C_CoefASRC5_VX_ADDR                                 462 
#define C_CoefASRC5_VX_ADDR_END                             480 
#define C_CoefASRC5_VX_sizeof                               19 

#define C_CoefASRC6_VX_ADDR                                 481 
#define C_CoefASRC6_VX_ADDR_END                             499 
#define C_CoefASRC6_VX_sizeof                               19 

#define C_CoefASRC7_VX_ADDR                                 500 
#define C_CoefASRC7_VX_ADDR_END                             518 
#define C_CoefASRC7_VX_sizeof                               19 

#define C_CoefASRC8_VX_ADDR                                 519 
#define C_CoefASRC8_VX_ADDR_END                             537 
#define C_CoefASRC8_VX_sizeof                               19 

#define C_CoefASRC9_VX_ADDR                                 538 
#define C_CoefASRC9_VX_ADDR_END                             556 
#define C_CoefASRC9_VX_sizeof                               19 

#define C_CoefASRC10_VX_ADDR                                557 
#define C_CoefASRC10_VX_ADDR_END                            575 
#define C_CoefASRC10_VX_sizeof                              19 

#define C_CoefASRC11_VX_ADDR                                576 
#define C_CoefASRC11_VX_ADDR_END                            594 
#define C_CoefASRC11_VX_sizeof                              19 

#define C_CoefASRC12_VX_ADDR                                595 
#define C_CoefASRC12_VX_ADDR_END                            613 
#define C_CoefASRC12_VX_sizeof                              19 

#define C_CoefASRC13_VX_ADDR                                614 
#define C_CoefASRC13_VX_ADDR_END                            632 
#define C_CoefASRC13_VX_sizeof                              19 

#define C_CoefASRC14_VX_ADDR                                633 
#define C_CoefASRC14_VX_ADDR_END                            651 
#define C_CoefASRC14_VX_sizeof                              19 

#define C_CoefASRC15_VX_ADDR                                652 
#define C_CoefASRC15_VX_ADDR_END                            670 
#define C_CoefASRC15_VX_sizeof                              19 

#define C_CoefASRC16_VX_ADDR                                671 
#define C_CoefASRC16_VX_ADDR_END                            689 
#define C_CoefASRC16_VX_sizeof                              19 

#define C_AlphaCurrent_UL_VX_ADDR                           690 
#define C_AlphaCurrent_UL_VX_ADDR_END                       690 
#define C_AlphaCurrent_UL_VX_sizeof                         1 

#define C_BetaCurrent_UL_VX_ADDR                            691 
#define C_BetaCurrent_UL_VX_ADDR_END                        691 
#define C_BetaCurrent_UL_VX_sizeof                          1 

#define C_AlphaCurrent_DL_VX_ADDR                           692 
#define C_AlphaCurrent_DL_VX_ADDR_END                       692 
#define C_AlphaCurrent_DL_VX_sizeof                         1 

#define C_BetaCurrent_DL_VX_ADDR                            693 
#define C_BetaCurrent_DL_VX_ADDR_END                        693 
#define C_BetaCurrent_DL_VX_sizeof                          1 

#define C_CoefASRC1_DL_MM_ADDR                              694 
#define C_CoefASRC1_DL_MM_ADDR_END                          711 
#define C_CoefASRC1_DL_MM_sizeof                            18 

#define C_CoefASRC2_DL_MM_ADDR                              712 
#define C_CoefASRC2_DL_MM_ADDR_END                          729 
#define C_CoefASRC2_DL_MM_sizeof                            18 

#define C_CoefASRC3_DL_MM_ADDR                              730 
#define C_CoefASRC3_DL_MM_ADDR_END                          747 
#define C_CoefASRC3_DL_MM_sizeof                            18 

#define C_CoefASRC4_DL_MM_ADDR                              748 
#define C_CoefASRC4_DL_MM_ADDR_END                          765 
#define C_CoefASRC4_DL_MM_sizeof                            18 

#define C_CoefASRC5_DL_MM_ADDR                              766 
#define C_CoefASRC5_DL_MM_ADDR_END                          783 
#define C_CoefASRC5_DL_MM_sizeof                            18 

#define C_CoefASRC6_DL_MM_ADDR                              784 
#define C_CoefASRC6_DL_MM_ADDR_END                          801 
#define C_CoefASRC6_DL_MM_sizeof                            18 

#define C_CoefASRC7_DL_MM_ADDR                              802 
#define C_CoefASRC7_DL_MM_ADDR_END                          819 
#define C_CoefASRC7_DL_MM_sizeof                            18 

#define C_CoefASRC8_DL_MM_ADDR                              820 
#define C_CoefASRC8_DL_MM_ADDR_END                          837 
#define C_CoefASRC8_DL_MM_sizeof                            18 

#define C_CoefASRC9_DL_MM_ADDR                              838 
#define C_CoefASRC9_DL_MM_ADDR_END                          855 
#define C_CoefASRC9_DL_MM_sizeof                            18 

#define C_CoefASRC10_DL_MM_ADDR                             856 
#define C_CoefASRC10_DL_MM_ADDR_END                         873 
#define C_CoefASRC10_DL_MM_sizeof                           18 

#define C_CoefASRC11_DL_MM_ADDR                             874 
#define C_CoefASRC11_DL_MM_ADDR_END                         891 
#define C_CoefASRC11_DL_MM_sizeof                           18 

#define C_CoefASRC12_DL_MM_ADDR                             892 
#define C_CoefASRC12_DL_MM_ADDR_END                         909 
#define C_CoefASRC12_DL_MM_sizeof                           18 

#define C_CoefASRC13_DL_MM_ADDR                             910 
#define C_CoefASRC13_DL_MM_ADDR_END                         927 
#define C_CoefASRC13_DL_MM_sizeof                           18 

#define C_CoefASRC14_DL_MM_ADDR                             928 
#define C_CoefASRC14_DL_MM_ADDR_END                         945 
#define C_CoefASRC14_DL_MM_sizeof                           18 

#define C_CoefASRC15_DL_MM_ADDR                             946 
#define C_CoefASRC15_DL_MM_ADDR_END                         963 
#define C_CoefASRC15_DL_MM_sizeof                           18 

#define C_CoefASRC16_DL_MM_ADDR                             964 
#define C_CoefASRC16_DL_MM_ADDR_END                         981 
#define C_CoefASRC16_DL_MM_sizeof                           18 

#define C_AlphaCurrent_DL_MM_ADDR                           982 
#define C_AlphaCurrent_DL_MM_ADDR_END                       982 
#define C_AlphaCurrent_DL_MM_sizeof                         1 

#define C_BetaCurrent_DL_MM_ADDR                            983 
#define C_BetaCurrent_DL_MM_ADDR_END                        983 
#define C_BetaCurrent_DL_MM_sizeof                          1 

#define C_DL2_L_Coefs_ADDR                                  984 
#define C_DL2_L_Coefs_ADDR_END                              1008 
#define C_DL2_L_Coefs_sizeof                                25 

#define C_DL2_R_Coefs_ADDR                                  1009 
#define C_DL2_R_Coefs_ADDR_END                              1033 
#define C_DL2_R_Coefs_sizeof                                25 

#define C_DL1_Coefs_ADDR                                    1034 
#define C_DL1_Coefs_ADDR_END                                1058 
#define C_DL1_Coefs_sizeof                                  25 

#define C_SRC_3_LP_Coefs_ADDR                               1059 
#define C_SRC_3_LP_Coefs_ADDR_END                           1069 
#define C_SRC_3_LP_Coefs_sizeof                             11 

#define C_SRC_3_LP_GAIN_Coefs_ADDR                          1070 
#define C_SRC_3_LP_GAIN_Coefs_ADDR_END                      1080 
#define C_SRC_3_LP_GAIN_Coefs_sizeof                        11 

#define C_SRC_3_HP_Coefs_ADDR                               1081 
#define C_SRC_3_HP_Coefs_ADDR_END                           1085 
#define C_SRC_3_HP_Coefs_sizeof                             5 

#define C_SRC_6_LP_Coefs_ADDR                               1086 
#define C_SRC_6_LP_Coefs_ADDR_END                           1096 
#define C_SRC_6_LP_Coefs_sizeof                             11 

#define C_SRC_6_LP_GAIN_Coefs_ADDR                          1097 
#define C_SRC_6_LP_GAIN_Coefs_ADDR_END                      1107 
#define C_SRC_6_LP_GAIN_Coefs_sizeof                        11 

#define C_SRC_6_HP_Coefs_ADDR                               1108 
#define C_SRC_6_HP_Coefs_ADDR_END                           1114 
#define C_SRC_6_HP_Coefs_sizeof                             7 

#define C_EANC_WarpCoeffs_ADDR                              1115 
#define C_EANC_WarpCoeffs_ADDR_END                          1116 
#define C_EANC_WarpCoeffs_sizeof                            2 

#define C_EANC_FIRcoeffs_ADDR                               1117 
#define C_EANC_FIRcoeffs_ADDR_END                           1137 
#define C_EANC_FIRcoeffs_sizeof                             21 

#define C_EANC_IIRcoeffs_ADDR                               1138 
#define C_EANC_IIRcoeffs_ADDR_END                           1154 
#define C_EANC_IIRcoeffs_sizeof                             17 

#define C_EANC_FIRcoeffs_2nd_ADDR                           1155 
#define C_EANC_FIRcoeffs_2nd_ADDR_END                       1175 
#define C_EANC_FIRcoeffs_2nd_sizeof                         21 

#define C_EANC_IIRcoeffs_2nd_ADDR                           1176 
#define C_EANC_IIRcoeffs_2nd_ADDR_END                       1192 
#define C_EANC_IIRcoeffs_2nd_sizeof                         17 

#define C_APS_DL1_coeffs1_ADDR                              1193 
#define C_APS_DL1_coeffs1_ADDR_END                          1201 
#define C_APS_DL1_coeffs1_sizeof                            9 

#define C_APS_DL1_M_coeffs2_ADDR                            1202 
#define C_APS_DL1_M_coeffs2_ADDR_END                        1204 
#define C_APS_DL1_M_coeffs2_sizeof                          3 

#define C_APS_DL1_C_coeffs2_ADDR                            1205 
#define C_APS_DL1_C_coeffs2_ADDR_END                        1207 
#define C_APS_DL1_C_coeffs2_sizeof                          3 

#define C_APS_DL2_L_coeffs1_ADDR                            1208 
#define C_APS_DL2_L_coeffs1_ADDR_END                        1216 
#define C_APS_DL2_L_coeffs1_sizeof                          9 

#define C_APS_DL2_R_coeffs1_ADDR                            1217 
#define C_APS_DL2_R_coeffs1_ADDR_END                        1225 
#define C_APS_DL2_R_coeffs1_sizeof                          9 

#define C_APS_DL2_L_M_coeffs2_ADDR                          1226 
#define C_APS_DL2_L_M_coeffs2_ADDR_END                      1228 
#define C_APS_DL2_L_M_coeffs2_sizeof                        3 

#define C_APS_DL2_R_M_coeffs2_ADDR                          1229 
#define C_APS_DL2_R_M_coeffs2_ADDR_END                      1231 
#define C_APS_DL2_R_M_coeffs2_sizeof                        3 

#define C_APS_DL2_L_C_coeffs2_ADDR                          1232 
#define C_APS_DL2_L_C_coeffs2_ADDR_END                      1234 
#define C_APS_DL2_L_C_coeffs2_sizeof                        3 

#define C_APS_DL2_R_C_coeffs2_ADDR                          1235 
#define C_APS_DL2_R_C_coeffs2_ADDR_END                      1237 
#define C_APS_DL2_R_C_coeffs2_sizeof                        3 

#define C_AlphaCurrent_ECHO_REF_ADDR                        1238 
#define C_AlphaCurrent_ECHO_REF_ADDR_END                    1238 
#define C_AlphaCurrent_ECHO_REF_sizeof                      1 

#define C_BetaCurrent_ECHO_REF_ADDR                         1239 
#define C_BetaCurrent_ECHO_REF_ADDR_END                     1239 
#define C_BetaCurrent_ECHO_REF_sizeof                       1 

#define C_APS_DL1_EQ_ADDR                                   1240 
#define C_APS_DL1_EQ_ADDR_END                               1248 
#define C_APS_DL1_EQ_sizeof                                 9 

#define C_APS_DL2_L_EQ_ADDR                                 1249 
#define C_APS_DL2_L_EQ_ADDR_END                             1257 
#define C_APS_DL2_L_EQ_sizeof                               9 

#define C_APS_DL2_R_EQ_ADDR                                 1258 
#define C_APS_DL2_R_EQ_ADDR_END                             1266 
#define C_APS_DL2_R_EQ_sizeof                               9 

#define C_Vibra2_consts_ADDR                                1267 
#define C_Vibra2_consts_ADDR_END                            1270 
#define C_Vibra2_consts_sizeof                              4 

#define C_Vibra1_coeffs_ADDR                                1271 
#define C_Vibra1_coeffs_ADDR_END                            1281 
#define C_Vibra1_coeffs_sizeof                              11 

#define C_48_96_LP_Coefs_ADDR                               1282 
#define C_48_96_LP_Coefs_ADDR_END                           1296 
#define C_48_96_LP_Coefs_sizeof                             15 

#define C_98_48_LP_Coefs_ADDR                               1297 
#define C_98_48_LP_Coefs_ADDR_END                           1315 
#define C_98_48_LP_Coefs_sizeof                             19 

#define C_INPUT_SCALE_ADDR                                  1316 
#define C_INPUT_SCALE_ADDR_END                              1316 
#define C_INPUT_SCALE_sizeof                                1 

#define C_OUTPUT_SCALE_ADDR                                 1317 
#define C_OUTPUT_SCALE_ADDR_END                             1317 
#define C_OUTPUT_SCALE_sizeof                               1 

#define C_MUTE_SCALING_ADDR                                 1318 
#define C_MUTE_SCALING_ADDR_END                             1318 
#define C_MUTE_SCALING_sizeof                               1 

#define C_GAINS_0DB_ADDR                                    1319 
#define C_GAINS_0DB_ADDR_END                                1320 
#define C_GAINS_0DB_sizeof                                  2 


#endif /* _ABECM_ADDR_H_ */
