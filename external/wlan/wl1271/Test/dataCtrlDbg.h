/*
 * dataCtrlDbg.h
 *
 * Copyright(c) 1998 - 2009 Texas Instruments. All rights reserved.      
 * All rights reserved.                                                  
 *                                                                       
 * Redistribution and use in source and binary forms, with or without    
 * modification, are permitted provided that the following conditions    
 * are met:                                                              
 *                                                                       
 *  * Redistributions of source code must retain the above copyright     
 *    notice, this list of conditions and the following disclaimer.      
 *  * Redistributions in binary form must reproduce the above copyright  
 *    notice, this list of conditions and the following disclaimer in    
 *    the documentation and/or other materials provided with the         
 *    distribution.                                                      
 *  * Neither the name Texas Instruments nor the names of its            
 *    contributors may be used to endorse or promote products derived    
 *    from this software without specific prior written permission.      
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
 */

#ifndef __DATA_CTRL_DBG_H__
#define __DATA_CTRL_DBG_H__


/* RX/TX debug functions */
typedef enum
{
/* Tx debug functions */
/*	0	*/	TX_RX_DBG_FUNCTIONS,
/*	1	*/	PRINT_TX_CTRL_INFO,
/*	2	*/	PRINT_TX_CTRL_COUNTERS,
/*	3	*/	PRINT_TX_DATA_QUEUE_INFO,
/*	4	*/	PRINT_TX_DATA_QUEUE_COUNTERS,
/*	5	*/	PRINT_TX_MGMT_QUEUE_INFO,
/*	6	*/	PRINT_TX_MGMT_QUEUE_COUNTERS,
/*	7	*/	PRINT_TX_CTRL_BLK_INFO,
/*	8	*/	PRINT_TX_HW_QUEUE_INFO,
/*	9	*/	PRINT_TX_XFER_INFO,
/*	10	*/	PRINT_TX_RESULT_INFO,
/*	11	*/	PRINT_TX_DATA_CLSFR_TABLE,
/*	20	*/	RESET_TX_CTRL_COUNTERS          = 20,
/*	21	*/	RESET_TX_DATA_QUEUE_COUNTERS,
/*	22	*/	RESET_TX_DATA_CLSFR_TABLE,
/*	23	*/	RESET_TX_MGMT_QUEUE_COUNTERS,
/*	24	*/	RESET_TX_RESULT_COUNTERS,
/*	25	*/	RESET_TX_XFER_COUNTERS,

/* Rx debug functions */
/*	50	*/	PRINT_RX_BLOCK                  = 50,
/*	51	*/	PRINT_RX_COUNTERS,
/*	52	*/	RESET_RX_COUNTERS,
/*	53	*/	PRINT_RX_THROUGHPUT_START,
/*	54	*/	PRINT_RX_THROUGHPUT_STOP

} ERxTxDbgFunc;


/* debg functions */
typedef enum
{
/*	0	*/	CTRL_PRINT_DBG_FUNCTIONS,
/*	1	*/	CTRL_PRINT_CTRL_BLOCK,
/*	2	*/	CTRL_PRINT_TX_PARAMETERS,
/*	3	*/	CTRL_SET_CTS_TO_SELF

} ECtrlDbgFunc;


void rxTxDebugFunction (TI_HANDLE hRxTxHandle, TI_UINT32 funcType, void *pParam);
void ctrlDebugFunction (TI_HANDLE hCtrlData, TI_UINT32 funcType, void *pParam);


#endif /* __DATA_CTRL_DBG_H__*/


