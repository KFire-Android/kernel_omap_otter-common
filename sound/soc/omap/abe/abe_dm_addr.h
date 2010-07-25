/*
 * ==========================================================================
 *		 Texas Instruments OMAP(TM) Platform Firmware
 * (c) Copyright 2009, Texas Instruments Incorporated.	All Rights Reserved.
 *
 *  Use of this firmware is controlled by the terms and conditions found
 *  in the license agreement under which this firmware has been supplied.
 * ==========================================================================
 */

#ifndef _ABE_DM_ADDR_H_
#define _ABE_DM_ADDR_H_

#define D_atcDescriptors_ADDR					0
#define D_atcDescriptors_ADDR_END				511
#define D_atcDescriptors_sizeof					512

#define stack_ADDR						512
#define stack_ADDR_END						623
#define stack_sizeof						112

#define D_version_ADDR						624
#define D_version_ADDR_END					627
#define D_version_sizeof					4

#define D_BT_DL_FIFO_ADDR					1024
#define D_BT_DL_FIFO_ADDR_END					1503
#define D_BT_DL_FIFO_sizeof					480

#define D_BT_UL_FIFO_ADDR					1536
#define D_BT_UL_FIFO_ADDR_END					2015
#define D_BT_UL_FIFO_sizeof					480

#define D_MM_EXT_OUT_FIFO_ADDR					2048
#define D_MM_EXT_OUT_FIFO_ADDR_END				2527
#define D_MM_EXT_OUT_FIFO_sizeof				480

#define D_MM_EXT_IN_FIFO_ADDR					2560
#define D_MM_EXT_IN_FIFO_ADDR_END				3039
#define D_MM_EXT_IN_FIFO_sizeof					480

#define D_MM_UL2_FIFO_ADDR					3072
#define D_MM_UL2_FIFO_ADDR_END					3551
#define D_MM_UL2_FIFO_sizeof					480

#define D_VX_UL_FIFO_ADDR					3584
#define D_VX_UL_FIFO_ADDR_END					4063
#define D_VX_UL_FIFO_sizeof					480

#define D_VX_DL_FIFO_ADDR					4096
#define D_VX_DL_FIFO_ADDR_END					4575
#define D_VX_DL_FIFO_sizeof					480

#define D_DMIC_UL_FIFO_ADDR					4608
#define D_DMIC_UL_FIFO_ADDR_END					5087
#define D_DMIC_UL_FIFO_sizeof					480

#define D_MM_UL_FIFO_ADDR					5120
#define D_MM_UL_FIFO_ADDR_END					5599
#define D_MM_UL_FIFO_sizeof					480

#define D_MM_DL_FIFO_ADDR					5632
#define D_MM_DL_FIFO_ADDR_END					6111
#define D_MM_DL_FIFO_sizeof					480

#define D_TONES_DL_FIFO_ADDR					6144
#define D_TONES_DL_FIFO_ADDR_END				6623
#define D_TONES_DL_FIFO_sizeof					480

#define D_VIB_DL_FIFO_ADDR					6656
#define D_VIB_DL_FIFO_ADDR_END					7135
#define D_VIB_DL_FIFO_sizeof					480

#define D_McPDM_DL_FIFO_ADDR					7168
#define D_McPDM_DL_FIFO_ADDR_END				7647
#define D_McPDM_DL_FIFO_sizeof					480

#define D_McPDM_UL_FIFO_ADDR					7680
#define D_McPDM_UL_FIFO_ADDR_END				8159
#define D_McPDM_UL_FIFO_sizeof					480

#define D_DEBUG_FIFO_ADDR					8192
#define D_DEBUG_FIFO_ADDR_END					8319
#define D_DEBUG_FIFO_sizeof					128

#define D_IOdescr_ADDR						8320
#define D_IOdescr_ADDR_END					8879
#define D_IOdescr_sizeof					560

#define d_zero_ADDR						8880
#define d_zero_ADDR_END						8880
#define d_zero_sizeof						1

#define dbg_trace1_ADDR						8881
#define dbg_trace1_ADDR_END					8881
#define dbg_trace1_sizeof					1

#define dbg_trace2_ADDR						8882
#define dbg_trace2_ADDR_END					8882
#define dbg_trace2_sizeof					1

#define dbg_trace3_ADDR						8883
#define dbg_trace3_ADDR_END					8883
#define dbg_trace3_sizeof					1

#define D_multiFrame_ADDR					8884
#define D_multiFrame_ADDR_END					9283
#define D_multiFrame_sizeof					400

#define D_tasksList_ADDR					9284
#define D_tasksList_ADDR_END					11331
#define D_tasksList_sizeof					2048

#define D_idleTask_ADDR						11332
#define D_idleTask_ADDR_END					11333
#define D_idleTask_sizeof					2

#define D_typeLengthCheck_ADDR					11334
#define D_typeLengthCheck_ADDR_END				11335
#define D_typeLengthCheck_sizeof				2

#define D_maxTaskBytesInSlot_ADDR				11336
#define D_maxTaskBytesInSlot_ADDR_END				11337
#define D_maxTaskBytesInSlot_sizeof				2

#define D_rewindTaskBytes_ADDR					11338
#define D_rewindTaskBytes_ADDR_END				11339
#define D_rewindTaskBytes_sizeof				2

#define D_pCurrentTask_ADDR					11340
#define D_pCurrentTask_ADDR_END					11341
#define D_pCurrentTask_sizeof					2

#define D_pFastLoopBack_ADDR					11342
#define D_pFastLoopBack_ADDR_END				11343
#define D_pFastLoopBack_sizeof					2

#define D_pNextFastLoopBack_ADDR				11344
#define D_pNextFastLoopBack_ADDR_END				11347
#define D_pNextFastLoopBack_sizeof				4

#define D_ppCurrentTask_ADDR					11348
#define D_ppCurrentTask_ADDR_END				11349
#define D_ppCurrentTask_sizeof					2

#define D_slotCounter_ADDR					11352
#define D_slotCounter_ADDR_END					11353
#define D_slotCounter_sizeof					2

#define D_loopCounter_ADDR					11356
#define D_loopCounter_ADDR_END					11359
#define D_loopCounter_sizeof					4

#define D_RewindFlag_ADDR					11360
#define D_RewindFlag_ADDR_END					11361
#define D_RewindFlag_sizeof					2

#define D_Slot23_ctrl_ADDR					11364
#define D_Slot23_ctrl_ADDR_END					11367
#define D_Slot23_ctrl_sizeof					4

#define D_McuIrqFifo_ADDR					11368
#define D_McuIrqFifo_ADDR_END					11431
#define D_McuIrqFifo_sizeof					64

#define D_PingPongDesc_ADDR					11432
#define D_PingPongDesc_ADDR_END					11479
#define D_PingPongDesc_sizeof					48

#define D_PP_MCU_IRQ_ADDR					11480
#define D_PP_MCU_IRQ_ADDR_END					11481
#define D_PP_MCU_IRQ_sizeof					2

#define D_ctrlPortFifo_ADDR					11488
#define D_ctrlPortFifo_ADDR_END					11503
#define D_ctrlPortFifo_sizeof					16

#define D_Idle_State_ADDR					11504
#define D_Idle_State_ADDR_END					11507
#define D_Idle_State_sizeof					4

#define D_Stop_Request_ADDR					11508
#define D_Stop_Request_ADDR_END					11511
#define D_Stop_Request_sizeof					4

#define D_Ref0_ADDR						11512
#define D_Ref0_ADDR_END						11513
#define D_Ref0_sizeof						2

#define D_DebugRegister_ADDR					11516
#define D_DebugRegister_ADDR_END				11655
#define D_DebugRegister_sizeof					140

#define D_Gcount_ADDR						11656
#define D_Gcount_ADDR_END					11657
#define D_Gcount_sizeof						2

#define D_DCcounter_ADDR					11660
#define D_DCcounter_ADDR_END					11663
#define D_DCcounter_sizeof					4

#define D_DCsum_ADDR						11664
#define D_DCsum_ADDR_END					11671
#define D_DCsum_sizeof						8

#define D_fastCounter_ADDR					11672
#define D_fastCounter_ADDR_END					11675
#define D_fastCounter_sizeof					4

#define D_slowCounter_ADDR					11676
#define D_slowCounter_ADDR_END					11679
#define D_slowCounter_sizeof					4

#define D_aUplinkRouting_ADDR					11680
#define D_aUplinkRouting_ADDR_END				11711
#define D_aUplinkRouting_sizeof					32

#define D_VirtAudioLoop_ADDR					11712
#define D_VirtAudioLoop_ADDR_END				11715
#define D_VirtAudioLoop_sizeof					4

#define D_AsrcVars_DL_VX_ADDR					11716
#define D_AsrcVars_DL_VX_ADDR_END				11747
#define D_AsrcVars_DL_VX_sizeof					32

#define D_AsrcVars_UL_VX_ADDR					11748
#define D_AsrcVars_UL_VX_ADDR_END				11779
#define D_AsrcVars_UL_VX_sizeof					32

#define D_CoefAddresses_VX_ADDR					11780
#define D_CoefAddresses_VX_ADDR_END				11811
#define D_CoefAddresses_VX_sizeof				32

#define D_AsrcVars_DL_MM_ADDR					11812
#define D_AsrcVars_DL_MM_ADDR_END				11843
#define D_AsrcVars_DL_MM_sizeof					32

#define D_CoefAddresses_DL_MM_ADDR				11844
#define D_CoefAddresses_DL_MM_ADDR_END				11875
#define D_CoefAddresses_DL_MM_sizeof				32

#define D_APS_DL1_M_thresholds_ADDR				11876
#define D_APS_DL1_M_thresholds_ADDR_END				11883
#define D_APS_DL1_M_thresholds_sizeof				8

#define D_APS_DL1_M_IRQ_ADDR					11884
#define D_APS_DL1_M_IRQ_ADDR_END				11885
#define D_APS_DL1_M_IRQ_sizeof					2

#define D_APS_DL1_C_IRQ_ADDR					11886
#define D_APS_DL1_C_IRQ_ADDR_END				11887
#define D_APS_DL1_C_IRQ_sizeof					2

#define D_TraceBufAdr_ADDR					11888
#define D_TraceBufAdr_ADDR_END					11889
#define D_TraceBufAdr_sizeof					2

#define D_TraceBufOffset_ADDR					11890
#define D_TraceBufOffset_ADDR_END				11891
#define D_TraceBufOffset_sizeof					2

#define D_TraceBufLength_ADDR					11892
#define D_TraceBufLength_ADDR_END				11893
#define D_TraceBufLength_sizeof					2

#define D_AsrcVars_ECHO_REF_ADDR				11896
#define D_AsrcVars_ECHO_REF_ADDR_END				11927
#define D_AsrcVars_ECHO_REF_sizeof				32

#define D_Pempty_ADDR						11928
#define D_Pempty_ADDR_END					11931
#define D_Pempty_sizeof						4

#define D_APS_DL2_L_M_IRQ_ADDR					11932
#define D_APS_DL2_L_M_IRQ_ADDR_END				11933
#define D_APS_DL2_L_M_IRQ_sizeof				2

#define D_APS_DL2_L_C_IRQ_ADDR					11934
#define D_APS_DL2_L_C_IRQ_ADDR_END				11935
#define D_APS_DL2_L_C_IRQ_sizeof				2

#define D_APS_DL2_R_M_IRQ_ADDR					11936
#define D_APS_DL2_R_M_IRQ_ADDR_END				11937
#define D_APS_DL2_R_M_IRQ_sizeof				2

#define D_APS_DL2_R_C_IRQ_ADDR					11938
#define D_APS_DL2_R_C_IRQ_ADDR_END				11939
#define D_APS_DL2_R_C_IRQ_sizeof				2

#define D_APS_DL1_C_thresholds_ADDR				11940
#define D_APS_DL1_C_thresholds_ADDR_END				11947
#define D_APS_DL1_C_thresholds_sizeof				8

#define D_APS_DL2_L_M_thresholds_ADDR				11948
#define D_APS_DL2_L_M_thresholds_ADDR_END			11955
#define D_APS_DL2_L_M_thresholds_sizeof				8

#define D_APS_DL2_L_C_thresholds_ADDR				11956
#define D_APS_DL2_L_C_thresholds_ADDR_END			11963
#define D_APS_DL2_L_C_thresholds_sizeof				8

#define D_APS_DL2_R_M_thresholds_ADDR				11964
#define D_APS_DL2_R_M_thresholds_ADDR_END			11971
#define D_APS_DL2_R_M_thresholds_sizeof				8

#define D_APS_DL2_R_C_thresholds_ADDR				11972
#define D_APS_DL2_R_C_thresholds_ADDR_END			11979
#define D_APS_DL2_R_C_thresholds_sizeof				8

#define D_ECHO_REF_48_16_WRAP_ADDR				11980
#define D_ECHO_REF_48_16_WRAP_ADDR_END				11987
#define D_ECHO_REF_48_16_WRAP_sizeof				8

#define D_ECHO_REF_48_8_WRAP_ADDR				11988
#define D_ECHO_REF_48_8_WRAP_ADDR_END				11995
#define D_ECHO_REF_48_8_WRAP_sizeof				8

#define D_BT_UL_16_48_WRAP_ADDR					11996
#define D_BT_UL_16_48_WRAP_ADDR_END				12003
#define D_BT_UL_16_48_WRAP_sizeof				8

#define D_BT_UL_8_48_WRAP_ADDR					12004
#define D_BT_UL_8_48_WRAP_ADDR_END				12011
#define D_BT_UL_8_48_WRAP_sizeof				8

#define D_BT_DL_48_16_WRAP_ADDR					12012
#define D_BT_DL_48_16_WRAP_ADDR_END				12019
#define D_BT_DL_48_16_WRAP_sizeof				8

#define D_BT_DL_48_8_WRAP_ADDR					12020
#define D_BT_DL_48_8_WRAP_ADDR_END				12027
#define D_BT_DL_48_8_WRAP_sizeof				8

#define D_VX_DL_16_48_WRAP_ADDR					12028
#define D_VX_DL_16_48_WRAP_ADDR_END				12035
#define D_VX_DL_16_48_WRAP_sizeof				8

#define D_VX_DL_8_48_WRAP_ADDR					12036
#define D_VX_DL_8_48_WRAP_ADDR_END				12043
#define D_VX_DL_8_48_WRAP_sizeof				8

#define D_VX_UL_48_16_WRAP_ADDR					12044
#define D_VX_UL_48_16_WRAP_ADDR_END				12051
#define D_VX_UL_48_16_WRAP_sizeof				8

#define D_VX_UL_48_8_WRAP_ADDR					12052
#define D_VX_UL_48_8_WRAP_ADDR_END				12059
#define D_VX_UL_48_8_WRAP_sizeof				8

#define D_APS_DL1_IRQs_WRAP_ADDR				12060
#define D_APS_DL1_IRQs_WRAP_ADDR_END				12067
#define D_APS_DL1_IRQs_WRAP_sizeof				8

#define D_APS_DL2_L_IRQs_WRAP_ADDR				12068
#define D_APS_DL2_L_IRQs_WRAP_ADDR_END				12075
#define D_APS_DL2_L_IRQs_WRAP_sizeof				8

#define D_APS_DL2_R_IRQs_WRAP_ADDR				12076
#define D_APS_DL2_R_IRQs_WRAP_ADDR_END				12083
#define D_APS_DL2_R_IRQs_WRAP_sizeof				8

#define D_nextMultiFrame_ADDR					12084
#define D_nextMultiFrame_ADDR_END				12091
#define D_nextMultiFrame_sizeof					8

#define D_HW_TEST_ADDR						12092
#define D_HW_TEST_ADDR_END					12099
#define D_HW_TEST_sizeof					8

#define D_DEBUG_HAL_TASK_ADDR					12288
#define D_DEBUG_HAL_TASK_ADDR_END				14335
#define D_DEBUG_HAL_TASK_sizeof					2048

#define D_DEBUG_FW_TASK_ADDR					14336
#define D_DEBUG_FW_TASK_ADDR_END				14591
#define D_DEBUG_FW_TASK_sizeof					256

#define D_PING_ADDR						16384
#define D_PING_ADDR_END						40959
#define D_PING_sizeof						24576

#define D_PONG_ADDR						40960
#define D_PONG_ADDR_END						65535
#define D_PONG_sizeof						24576

#endif	/* _ABE_DM_ADDR_H_ */
