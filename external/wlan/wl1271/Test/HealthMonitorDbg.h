/*
 * HealthMonitorDbg.h
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

/** \file SoftGeminiDbg.h
 *  \brief This file include private definitions for the soft Gemini debug module.
 *  \
 *  \date 30-Dec-2004
 */

#ifndef __HEALTH_MONITOR_DBG_H__
#define __HEALTH_MONITOR_DBG_H__

/*
 ***********************************************************************
 *	Constant definitions.
 ***********************************************************************
 */

/* debug functions */
#define DBG_HM_PRINT_HELP					  0 	
#define DBG_HM_RECOVERY_NO_SCAN_COMPLETE	  1   
#define DBG_HM_RECOVERY_MBOX_FAILURE		  2   
#define DBG_HM_RECOVERY_HW_AWAKE_FAILURE	  3   
#define DBG_HM_RECOVERY_TX_STUCK			  4   
#define DBG_HM_DISCONNECT_TIMEOUT			  5   
#define DBG_HM_RECOVERY_POWER_SAVE_FAILURE	  6   
#define DBG_HM_RECOVERY_MEASUREMENT_FAILURE	  7   
#define DBG_HM_RECOVERY_BUS_FAILURE	          8   
#define DBG_HM_RECOVERY_FROM_CLI              9
#define DBG_HM_RECOVERY_FROM_HW_WD_EXPIRE     10
#define DBG_HM_RECOVERY_RX_XFER_FAILURE       11
                                               
/*                                            
 ***********************************************************************
 *	Enums.
 ***********************************************************************
 */

/*
 ***********************************************************************
 *	Typedefs.
 ***********************************************************************
 */

/*
 ***********************************************************************
 *	Structure definitions.
 ***********************************************************************
 */

/*
 ***********************************************************************
 *	External data definitions.
 ***********************************************************************
 */

/*
 ***********************************************************************
 *	External functions definitions
 ***********************************************************************
 */

/**
 * \\n
 * \date 14-Feb-2005\n
 * \brief Main soft Geminirdebug function
 *
 * Function Scope \e Public.\n
 * \param pStadHandles - modules handles list.\n
 * \param funcType - the specific debug function.\n
 * \param pParam - parameters for the debug function.\n
 */
void healthMonitorDebugFunction (TStadHandlesList *pStadHandles, TI_UINT32 funcType, void *pParam);

/**
 * \\n
 * \date 14-Feb-2005\n
 * \brief Prints scan concentrator debug menu
 *
 * Function Scope \e Public.\n
 */
void printHealthMonitorDbgFunctions(void);



#endif /* __HEALTH_MONITOR_DBG_H__ */
