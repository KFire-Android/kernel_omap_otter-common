/*
 * ==========================================================================
 *               Texas Instruments OMAP(TM) Platform Firmware
 * (c) Copyright 2009, Texas Instruments Incorporated.  All Rights Reserved.
 *
 *  Use of this firmware is controlled by the terms and conditions found
 *  in the license agreement under which this firmware has been supplied.
 * ==========================================================================
 */

#ifndef _ABE_SM_ADDR_H_
#define _ABE_SM_ADDR_H_

#define init_SM_ADDR						0
#define init_SM_ADDR_END					304
#define init_SM_sizeof						305

#define S_Data0_ADDR						305
#define S_Data0_ADDR_END					305
#define S_Data0_sizeof						1

#define S_Temp_ADDR						306
#define S_Temp_ADDR_END						306
#define S_Temp_sizeof						1

#define S_PhoenixOffset_ADDR					307
#define S_PhoenixOffset_ADDR_END				307
#define S_PhoenixOffset_sizeof					1

#define S_GTarget1_ADDR						308
#define S_GTarget1_ADDR_END					314
#define S_GTarget1_sizeof					7

#define S_Gtarget_DL1_ADDR					315
#define S_Gtarget_DL1_ADDR_END					316
#define S_Gtarget_DL1_sizeof					2

#define S_Gtarget_DL2_ADDR					317
#define S_Gtarget_DL2_ADDR_END					318
#define S_Gtarget_DL2_sizeof					2

#define S_Gtarget_Echo_ADDR					319
#define S_Gtarget_Echo_ADDR_END					319
#define S_Gtarget_Echo_sizeof					1

#define S_Gtarget_SDT_ADDR					320
#define S_Gtarget_SDT_ADDR_END					320
#define S_Gtarget_SDT_sizeof					1

#define S_Gtarget_VxRec_ADDR					321
#define S_Gtarget_VxRec_ADDR_END				322
#define S_Gtarget_VxRec_sizeof					2

#define S_Gtarget_UL_ADDR					323
#define S_Gtarget_UL_ADDR_END					324
#define S_Gtarget_UL_sizeof					2

#define S_Gtarget_unused_ADDR					325
#define S_Gtarget_unused_ADDR_END				325
#define S_Gtarget_unused_sizeof					1

#define S_GCurrent_ADDR						326
#define S_GCurrent_ADDR_END					343
#define S_GCurrent_sizeof					18

#define S_GAIN_ONE_ADDR						344
#define S_GAIN_ONE_ADDR_END					344
#define S_GAIN_ONE_sizeof					1

#define S_Tones_ADDR						345
#define S_Tones_ADDR_END					356
#define S_Tones_sizeof						12

#define S_VX_DL_ADDR						357
#define S_VX_DL_ADDR_END					368
#define S_VX_DL_sizeof						12

#define S_MM_UL2_ADDR						369
#define S_MM_UL2_ADDR_END					380
#define S_MM_UL2_sizeof						12

#define S_MM_DL_ADDR						381
#define S_MM_DL_ADDR_END					392
#define S_MM_DL_sizeof						12

#define S_DL1_M_Out_ADDR					393
#define S_DL1_M_Out_ADDR_END					404
#define S_DL1_M_Out_sizeof					12

#define S_DL2_M_Out_ADDR					405
#define S_DL2_M_Out_ADDR_END					416
#define S_DL2_M_Out_sizeof					12

#define S_Echo_M_Out_ADDR					417
#define S_Echo_M_Out_ADDR_END					428
#define S_Echo_M_Out_sizeof					12

#define S_SDT_M_Out_ADDR					429
#define S_SDT_M_Out_ADDR_END					440
#define S_SDT_M_Out_sizeof					12

#define S_VX_UL_ADDR						441
#define S_VX_UL_ADDR_END					452
#define S_VX_UL_sizeof						12

#define S_VX_UL_M_ADDR						453
#define S_VX_UL_M_ADDR_END					464
#define S_VX_UL_M_sizeof					12

#define S_BT_DL_ADDR						465
#define S_BT_DL_ADDR_END					476
#define S_BT_DL_sizeof						12

#define S_BT_UL_ADDR						477
#define S_BT_UL_ADDR_END					488
#define S_BT_UL_sizeof						12

#define S_BT_DL_8k_ADDR						489
#define S_BT_DL_8k_ADDR_END					490
#define S_BT_DL_8k_sizeof					2

#define S_BT_DL_16k_ADDR					491
#define S_BT_DL_16k_ADDR_END					494
#define S_BT_DL_16k_sizeof					4

#define S_BT_UL_8k_ADDR						495
#define S_BT_UL_8k_ADDR_END					496
#define S_BT_UL_8k_sizeof					2

#define S_BT_UL_16k_ADDR					497
#define S_BT_UL_16k_ADDR_END					500
#define S_BT_UL_16k_sizeof					4

#define S_SDT_F_ADDR						501
#define S_SDT_F_ADDR_END					512
#define S_SDT_F_sizeof						12

#define S_SDT_F_data_ADDR					513
#define S_SDT_F_data_ADDR_END					521
#define S_SDT_F_data_sizeof					9

#define S_MM_DL_OSR_ADDR					522
#define S_MM_DL_OSR_ADDR_END					545
#define S_MM_DL_OSR_sizeof					24

#define S_24_zeros_ADDR						546
#define S_24_zeros_ADDR_END					569
#define S_24_zeros_sizeof					24

#define S_DMIC1_ADDR						570
#define S_DMIC1_ADDR_END					581
#define S_DMIC1_sizeof						12

#define S_DMIC2_ADDR						582
#define S_DMIC2_ADDR_END					593
#define S_DMIC2_sizeof						12

#define S_DMIC3_ADDR						594
#define S_DMIC3_ADDR_END					605
#define S_DMIC3_sizeof						12

#define S_AMIC_ADDR						606
#define S_AMIC_ADDR_END						617
#define S_AMIC_sizeof						12

#define S_EANC_FBK_in_ADDR					618
#define S_EANC_FBK_in_ADDR_END					641
#define S_EANC_FBK_in_sizeof					24

#define S_EANC_FBK_out_ADDR					642
#define S_EANC_FBK_out_ADDR_END					653
#define S_EANC_FBK_out_sizeof					12

#define S_DMIC1_L_ADDR						654
#define S_DMIC1_L_ADDR_END					665
#define S_DMIC1_L_sizeof					12

#define S_DMIC1_R_ADDR						666
#define S_DMIC1_R_ADDR_END					677
#define S_DMIC1_R_sizeof					12

#define S_DMIC2_L_ADDR						678
#define S_DMIC2_L_ADDR_END					689
#define S_DMIC2_L_sizeof					12

#define S_DMIC2_R_ADDR						690
#define S_DMIC2_R_ADDR_END					701
#define S_DMIC2_R_sizeof					12

#define S_DMIC3_L_ADDR						702
#define S_DMIC3_L_ADDR_END					713
#define S_DMIC3_L_sizeof					12

#define S_DMIC3_R_ADDR						714
#define S_DMIC3_R_ADDR_END					725
#define S_DMIC3_R_sizeof					12

#define S_BT_UL_L_ADDR						726
#define S_BT_UL_L_ADDR_END					737
#define S_BT_UL_L_sizeof					12

#define S_BT_UL_R_ADDR						738
#define S_BT_UL_R_ADDR_END					749
#define S_BT_UL_R_sizeof					12

#define S_AMIC_L_ADDR						750
#define S_AMIC_L_ADDR_END					761
#define S_AMIC_L_sizeof						12

#define S_AMIC_R_ADDR						762
#define S_AMIC_R_ADDR_END					773
#define S_AMIC_R_sizeof						12

#define S_EANC_FBK_L_ADDR					774
#define S_EANC_FBK_L_ADDR_END					785
#define S_EANC_FBK_L_sizeof					12

#define S_EANC_FBK_R_ADDR					786
#define S_EANC_FBK_R_ADDR_END					797
#define S_EANC_FBK_R_sizeof					12

#define S_EchoRef_L_ADDR					798
#define S_EchoRef_L_ADDR_END					809
#define S_EchoRef_L_sizeof					12

#define S_EchoRef_R_ADDR					810
#define S_EchoRef_R_ADDR_END					821
#define S_EchoRef_R_sizeof					12

#define S_MM_DL_L_ADDR						822
#define S_MM_DL_L_ADDR_END					833
#define S_MM_DL_L_sizeof					12

#define S_MM_DL_R_ADDR						834
#define S_MM_DL_R_ADDR_END					845
#define S_MM_DL_R_sizeof					12

#define S_MM_UL_ADDR						846
#define S_MM_UL_ADDR_END					965
#define S_MM_UL_sizeof						120

#define S_AMIC_96k_ADDR						966
#define S_AMIC_96k_ADDR_END					989
#define S_AMIC_96k_sizeof					24

#define S_DMIC0_96k_ADDR					990
#define S_DMIC0_96k_ADDR_END					1013
#define S_DMIC0_96k_sizeof					24

#define S_DMIC1_96k_ADDR					1014
#define S_DMIC1_96k_ADDR_END					1037
#define S_DMIC1_96k_sizeof					24

#define S_DMIC2_96k_ADDR					1038
#define S_DMIC2_96k_ADDR_END					1061
#define S_DMIC2_96k_sizeof					24

#define S_UL_VX_UL_48_8K_ADDR					1062
#define S_UL_VX_UL_48_8K_ADDR_END				1073
#define S_UL_VX_UL_48_8K_sizeof					12

#define S_UL_VX_UL_48_16K_ADDR					1074
#define S_UL_VX_UL_48_16K_ADDR_END				1085
#define S_UL_VX_UL_48_16K_sizeof				12

#define S_UL_MIC_48K_ADDR					1086
#define S_UL_MIC_48K_ADDR_END					1097
#define S_UL_MIC_48K_sizeof					12

#define S_Voice_8k_UL_ADDR					1098
#define S_Voice_8k_UL_ADDR_END					1100
#define S_Voice_8k_UL_sizeof					3

#define S_Voice_8k_DL_ADDR					1101
#define S_Voice_8k_DL_ADDR_END					1102
#define S_Voice_8k_DL_sizeof					2

#define S_McPDM_Out1_ADDR					1103
#define S_McPDM_Out1_ADDR_END					1126
#define S_McPDM_Out1_sizeof					24

#define S_McPDM_Out2_ADDR					1127
#define S_McPDM_Out2_ADDR_END					1150
#define S_McPDM_Out2_sizeof					24

#define S_McPDM_Out3_ADDR					1151
#define S_McPDM_Out3_ADDR_END					1174
#define S_McPDM_Out3_sizeof					24

#define S_Voice_16k_UL_ADDR					1175
#define S_Voice_16k_UL_ADDR_END					1179
#define S_Voice_16k_UL_sizeof					5

#define S_Voice_16k_DL_ADDR					1180
#define S_Voice_16k_DL_ADDR_END					1183
#define S_Voice_16k_DL_sizeof					4

#define S_XinASRC_DL_VX_ADDR					1184
#define S_XinASRC_DL_VX_ADDR_END				1223
#define S_XinASRC_DL_VX_sizeof					40

#define S_XinASRC_UL_VX_ADDR					1224
#define S_XinASRC_UL_VX_ADDR_END				1263
#define S_XinASRC_UL_VX_sizeof					40

#define S_XinASRC_DL_MM_ADDR					1264
#define S_XinASRC_DL_MM_ADDR_END				1303
#define S_XinASRC_DL_MM_sizeof					40

#define S_VX_REC_ADDR						1304
#define S_VX_REC_ADDR_END					1315
#define S_VX_REC_sizeof						12

#define S_VX_REC_L_ADDR						1316
#define S_VX_REC_L_ADDR_END					1327
#define S_VX_REC_L_sizeof					12

#define S_VX_REC_R_ADDR						1328
#define S_VX_REC_R_ADDR_END					1339
#define S_VX_REC_R_sizeof					12

#define S_DL2_M_L_ADDR						1340
#define S_DL2_M_L_ADDR_END					1351
#define S_DL2_M_L_sizeof					12

#define S_DL2_M_R_ADDR						1352
#define S_DL2_M_R_ADDR_END					1363
#define S_DL2_M_R_sizeof					12

#define S_DL2_M_LR_EQ_data_ADDR					1364
#define S_DL2_M_LR_EQ_data_ADDR_END				1388
#define S_DL2_M_LR_EQ_data_sizeof				25

#define S_DL1_M_EQ_data_ADDR					1389
#define S_DL1_M_EQ_data_ADDR_END				1413
#define S_DL1_M_EQ_data_sizeof					25

#define S_EARP_48_96_LP_data_ADDR				1414
#define S_EARP_48_96_LP_data_ADDR_END				1428
#define S_EARP_48_96_LP_data_sizeof				15

#define S_IHF_48_96_LP_data_ADDR				1429
#define S_IHF_48_96_LP_data_ADDR_END				1443
#define S_IHF_48_96_LP_data_sizeof				15

#define S_VX_UL_8_TEMP_ADDR					1444
#define S_VX_UL_8_TEMP_ADDR_END					1445
#define S_VX_UL_8_TEMP_sizeof					2

#define S_VX_UL_16_TEMP_ADDR					1446
#define S_VX_UL_16_TEMP_ADDR_END				1449
#define S_VX_UL_16_TEMP_sizeof					4

#define S_VX_DL_8_48_LP_data_ADDR				1450
#define S_VX_DL_8_48_LP_data_ADDR_END				1460
#define S_VX_DL_8_48_LP_data_sizeof				11

#define S_VX_DL_8_48_HP_data_ADDR				1461
#define S_VX_DL_8_48_HP_data_ADDR_END				1467
#define S_VX_DL_8_48_HP_data_sizeof				7

#define S_VX_DL_16_48_LP_data_ADDR				1468
#define S_VX_DL_16_48_LP_data_ADDR_END				1478
#define S_VX_DL_16_48_LP_data_sizeof				11

#define S_VX_DL_16_48_HP_data_ADDR				1479
#define S_VX_DL_16_48_HP_data_ADDR_END				1483
#define S_VX_DL_16_48_HP_data_sizeof				5

#define S_VX_UL_48_8_LP_data_ADDR				1484
#define S_VX_UL_48_8_LP_data_ADDR_END				1494
#define S_VX_UL_48_8_LP_data_sizeof				11

#define S_VX_UL_48_8_HP_data_ADDR				1495
#define S_VX_UL_48_8_HP_data_ADDR_END				1501
#define S_VX_UL_48_8_HP_data_sizeof				7

#define S_VX_UL_48_16_LP_data_ADDR				1502
#define S_VX_UL_48_16_LP_data_ADDR_END				1512
#define S_VX_UL_48_16_LP_data_sizeof				11

#define S_VX_UL_48_16_HP_data_ADDR				1513
#define S_VX_UL_48_16_HP_data_ADDR_END				1519
#define S_VX_UL_48_16_HP_data_sizeof				7

#define S_BT_UL_8_48_LP_data_ADDR				1520
#define S_BT_UL_8_48_LP_data_ADDR_END				1530
#define S_BT_UL_8_48_LP_data_sizeof				11

#define S_BT_UL_8_48_HP_data_ADDR				1531
#define S_BT_UL_8_48_HP_data_ADDR_END				1537
#define S_BT_UL_8_48_HP_data_sizeof				7

#define S_BT_UL_16_48_LP_data_ADDR				1538
#define S_BT_UL_16_48_LP_data_ADDR_END				1548
#define S_BT_UL_16_48_LP_data_sizeof				11

#define S_BT_UL_16_48_HP_data_ADDR				1549
#define S_BT_UL_16_48_HP_data_ADDR_END				1553
#define S_BT_UL_16_48_HP_data_sizeof				5

#define S_BT_DL_48_8_LP_data_ADDR				1554
#define S_BT_DL_48_8_LP_data_ADDR_END				1564
#define S_BT_DL_48_8_LP_data_sizeof				11

#define S_BT_DL_48_8_HP_data_ADDR				1565
#define S_BT_DL_48_8_HP_data_ADDR_END				1571
#define S_BT_DL_48_8_HP_data_sizeof				7

#define S_BT_DL_48_16_LP_data_ADDR				1572
#define S_BT_DL_48_16_LP_data_ADDR_END				1582
#define S_BT_DL_48_16_LP_data_sizeof				11

#define S_BT_DL_48_16_HP_data_ADDR				1583
#define S_BT_DL_48_16_HP_data_ADDR_END				1587
#define S_BT_DL_48_16_HP_data_sizeof				5

#define S_ECHO_REF_48_8_LP_data_ADDR				1588
#define S_ECHO_REF_48_8_LP_data_ADDR_END			1598
#define S_ECHO_REF_48_8_LP_data_sizeof				11

#define S_ECHO_REF_48_8_HP_data_ADDR				1599
#define S_ECHO_REF_48_8_HP_data_ADDR_END			1605
#define S_ECHO_REF_48_8_HP_data_sizeof				7

#define S_ECHO_REF_48_16_LP_data_ADDR				1606
#define S_ECHO_REF_48_16_LP_data_ADDR_END			1616
#define S_ECHO_REF_48_16_LP_data_sizeof				11

#define S_ECHO_REF_48_16_HP_data_ADDR				1617
#define S_ECHO_REF_48_16_HP_data_ADDR_END			1621
#define S_ECHO_REF_48_16_HP_data_sizeof				5

#define S_EANC_IIR_data_ADDR					1622
#define S_EANC_IIR_data_ADDR_END				1638
#define S_EANC_IIR_data_sizeof					17

#define S_EANC_SignalTemp_ADDR					1639
#define S_EANC_SignalTemp_ADDR_END				1659
#define S_EANC_SignalTemp_sizeof				21

#define S_EANC_Input_ADDR					1660
#define S_EANC_Input_ADDR_END					1660
#define S_EANC_Input_sizeof					1

#define S_EANC_Output_ADDR					1661
#define S_EANC_Output_ADDR_END					1661
#define S_EANC_Output_sizeof					1

#define S_APS_IIRmem1_ADDR					1662
#define S_APS_IIRmem1_ADDR_END					1670
#define S_APS_IIRmem1_sizeof					9

#define S_APS_M_IIRmem2_ADDR					1671
#define S_APS_M_IIRmem2_ADDR_END				1673
#define S_APS_M_IIRmem2_sizeof					3

#define S_APS_C_IIRmem2_ADDR					1674
#define S_APS_C_IIRmem2_ADDR_END				1676
#define S_APS_C_IIRmem2_sizeof					3

#define S_APS_DL1_OutSamples_ADDR				1677
#define S_APS_DL1_OutSamples_ADDR_END				1678
#define S_APS_DL1_OutSamples_sizeof				2

#define S_APS_DL1_COIL_OutSamples_ADDR				1679
#define S_APS_DL1_COIL_OutSamples_ADDR_END			1680
#define S_APS_DL1_COIL_OutSamples_sizeof			2

#define S_APS_DL2_L_OutSamples_ADDR				1681
#define S_APS_DL2_L_OutSamples_ADDR_END				1682
#define S_APS_DL2_L_OutSamples_sizeof				2

#define S_APS_DL2_L_COIL_OutSamples_ADDR			1683
#define S_APS_DL2_L_COIL_OutSamples_ADDR_END			1684
#define S_APS_DL2_L_COIL_OutSamples_sizeof			2

#define S_APS_DL2_R_OutSamples_ADDR				1685
#define S_APS_DL2_R_OutSamples_ADDR_END				1686
#define S_APS_DL2_R_OutSamples_sizeof				2

#define S_APS_DL2_R_COIL_OutSamples_ADDR			1687
#define S_APS_DL2_R_COIL_OutSamples_ADDR_END			1688
#define S_APS_DL2_R_COIL_OutSamples_sizeof			2

#define S_XinASRC_ECHO_REF_ADDR					1689
#define S_XinASRC_ECHO_REF_ADDR_END				1728
#define S_XinASRC_ECHO_REF_sizeof				40

#define S_ECHO_REF_16K_ADDR					1729
#define S_ECHO_REF_16K_ADDR_END					1733
#define S_ECHO_REF_16K_sizeof					5

#define S_ECHO_REF_8K_ADDR					1734
#define S_ECHO_REF_8K_ADDR_END					1736
#define S_ECHO_REF_8K_sizeof					3

#define S_DL1_EQ_ADDR						1737
#define S_DL1_EQ_ADDR_END					1748
#define S_DL1_EQ_sizeof						12

#define S_DL2_EQ_ADDR						1749
#define S_DL2_EQ_ADDR_END					1760
#define S_DL2_EQ_sizeof						12

#define S_DL1_GAIN_out_ADDR					1761
#define S_DL1_GAIN_out_ADDR_END					1772
#define S_DL1_GAIN_out_sizeof					12

#define S_DL2_GAIN_out_ADDR					1773
#define S_DL2_GAIN_out_ADDR_END					1784
#define S_DL2_GAIN_out_sizeof					12

#define S_APS_DL2_L_IIRmem1_ADDR				1785
#define S_APS_DL2_L_IIRmem1_ADDR_END				1793
#define S_APS_DL2_L_IIRmem1_sizeof				9

#define S_APS_DL2_R_IIRmem1_ADDR				1794
#define S_APS_DL2_R_IIRmem1_ADDR_END				1802
#define S_APS_DL2_R_IIRmem1_sizeof				9

#define S_APS_DL2_L_M_IIRmem2_ADDR				1803
#define S_APS_DL2_L_M_IIRmem2_ADDR_END				1805
#define S_APS_DL2_L_M_IIRmem2_sizeof				3

#define S_APS_DL2_R_M_IIRmem2_ADDR				1806
#define S_APS_DL2_R_M_IIRmem2_ADDR_END				1808
#define S_APS_DL2_R_M_IIRmem2_sizeof				3

#define S_APS_DL2_L_C_IIRmem2_ADDR				1809
#define S_APS_DL2_L_C_IIRmem2_ADDR_END				1811
#define S_APS_DL2_L_C_IIRmem2_sizeof				3

#define S_APS_DL2_R_C_IIRmem2_ADDR				1812
#define S_APS_DL2_R_C_IIRmem2_ADDR_END				1814
#define S_APS_DL2_R_C_IIRmem2_sizeof				3

#define S_DL1_APS_ADDR						1815
#define S_DL1_APS_ADDR_END					1826
#define S_DL1_APS_sizeof					12

#define S_DL2_L_APS_ADDR					1827
#define S_DL2_L_APS_ADDR_END					1838
#define S_DL2_L_APS_sizeof					12

#define S_DL2_R_APS_ADDR					1839
#define S_DL2_R_APS_ADDR_END					1850
#define S_DL2_R_APS_sizeof					12

#define S_APS_DL1_EQ_data_ADDR					1851
#define S_APS_DL1_EQ_data_ADDR_END				1859
#define S_APS_DL1_EQ_data_sizeof				9

#define S_APS_DL2_EQ_data_ADDR					1860
#define S_APS_DL2_EQ_data_ADDR_END				1868
#define S_APS_DL2_EQ_data_sizeof				9

#define S_DC_DCvalue_ADDR					1869
#define S_DC_DCvalue_ADDR_END					1869
#define S_DC_DCvalue_sizeof					1

#define S_VIBRA_ADDR						1870
#define S_VIBRA_ADDR_END					1875
#define S_VIBRA_sizeof						6

#define S_Vibra2_in_ADDR					1876
#define S_Vibra2_in_ADDR_END					1881
#define S_Vibra2_in_sizeof					6

#define S_Vibra2_addr_ADDR					1882
#define S_Vibra2_addr_ADDR_END					1882
#define S_Vibra2_addr_sizeof					1

#define S_VibraCtrl_forRightSM_ADDR				1883
#define S_VibraCtrl_forRightSM_ADDR_END				1906
#define S_VibraCtrl_forRightSM_sizeof				24

#define S_Rnoise_mem_ADDR					1907
#define S_Rnoise_mem_ADDR_END					1907
#define S_Rnoise_mem_sizeof					1

#define S_Ctrl_ADDR						1908
#define S_Ctrl_ADDR_END						1925
#define S_Ctrl_sizeof						18

#define S_Vibra1_in_ADDR					1926
#define S_Vibra1_in_ADDR_END					1931
#define S_Vibra1_in_sizeof					6

#define S_Vibra1_temp_ADDR					1932
#define S_Vibra1_temp_ADDR_END					1955
#define S_Vibra1_temp_sizeof					24

#define S_VibraCtrl_forLeftSM_ADDR				1956
#define S_VibraCtrl_forLeftSM_ADDR_END				1979
#define S_VibraCtrl_forLeftSM_sizeof				24

#define S_Vibra1_mem_ADDR					1980
#define S_Vibra1_mem_ADDR_END					1990
#define S_Vibra1_mem_sizeof					11

#define S_VibraCtrl_Stereo_ADDR					1991
#define S_VibraCtrl_Stereo_ADDR_END				2014
#define S_VibraCtrl_Stereo_sizeof				24

#define S_AMIC_96_48_data_ADDR					2015
#define S_AMIC_96_48_data_ADDR_END				2033
#define S_AMIC_96_48_data_sizeof				19

#define S_DMIC0_96_48_data_ADDR					2034
#define S_DMIC0_96_48_data_ADDR_END				2052
#define S_DMIC0_96_48_data_sizeof				19

#define S_DMIC1_96_48_data_ADDR					2053
#define S_DMIC1_96_48_data_ADDR_END				2071
#define S_DMIC1_96_48_data_sizeof				19

#define S_DMIC2_96_48_data_ADDR					2072
#define S_DMIC2_96_48_data_ADDR_END				2090
#define S_DMIC2_96_48_data_sizeof				19

#define S_EANC_FBK_96_48_data_ADDR				2091
#define S_EANC_FBK_96_48_data_ADDR_END				2109
#define S_EANC_FBK_96_48_data_sizeof				19

#define S_DBG_8K_PATTERN_ADDR					2110
#define S_DBG_8K_PATTERN_ADDR_END				2111
#define S_DBG_8K_PATTERN_sizeof					2

#define S_DBG_16K_PATTERN_ADDR					2112
#define S_DBG_16K_PATTERN_ADDR_END				2115
#define S_DBG_16K_PATTERN_sizeof				4

#define S_DBG_24K_PATTERN_ADDR					2116
#define S_DBG_24K_PATTERN_ADDR_END				2121
#define S_DBG_24K_PATTERN_sizeof				6

#define S_DBG_48K_PATTERN_ADDR					2122
#define S_DBG_48K_PATTERN_ADDR_END				2133
#define S_DBG_48K_PATTERN_sizeof				12

#define S_DBG_96K_PATTERN_ADDR					2134
#define S_DBG_96K_PATTERN_ADDR_END				2157
#define S_DBG_96K_PATTERN_sizeof				24

#define S_MM_EXT_IN_ADDR					2158
#define S_MM_EXT_IN_ADDR_END					2169
#define S_MM_EXT_IN_sizeof					12

#define S_MM_EXT_IN_L_ADDR					2170
#define S_MM_EXT_IN_L_ADDR_END					2181
#define S_MM_EXT_IN_L_sizeof					12

#define S_MM_EXT_IN_R_ADDR					2182
#define S_MM_EXT_IN_R_ADDR_END					2193
#define S_MM_EXT_IN_R_sizeof					12

#define S_MIC4_ADDR						2194
#define S_MIC4_ADDR_END						2205
#define S_MIC4_sizeof						12

#define S_MIC4_L_ADDR						2206
#define S_MIC4_L_ADDR_END					2217
#define S_MIC4_L_sizeof						12

#define S_MIC4_R_ADDR						2218
#define S_MIC4_R_ADDR_END					2229
#define S_MIC4_R_sizeof						12

#define S_HW_TEST_ADDR						2230
#define S_HW_TEST_ADDR_END					2230
#define S_HW_TEST_sizeof					1

#endif /* _ABESM_ADDR_H_ */
