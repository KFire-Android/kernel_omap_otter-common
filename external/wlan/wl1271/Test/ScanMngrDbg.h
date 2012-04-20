/*
 * ScanMngrDbg.h
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

/** \file ScanMngrDbg.h
 *  \brief This file include private definitions for the scan manager debug module.
 *  \
 *  \date 29-March-2005
 */

#ifndef __SCANMNGRDBG_H__
#define __SCANMNGRDBG_H__

/*
 ***********************************************************************
 *	Constant definitions.
 ***********************************************************************
 */

/* debug functions */
#define DBG_SCAN_MNGR_PRINT_HELP		    	0
#define DBG_SCAN_MNGR_START_CONT_SCAN           1
#define DBG_SCAN_MNGR_STOP_CONT_SCAN            2
#define DBG_SCAN_MNGR_START_IMMED_SCAN          3
#define DBG_SCAN_MNGR_STOP_IMMED_SCAN           4
#define DBG_SCAN_MNGR_PRINT_TRACK_LIST          5
#define DBG_SCAN_MNGR_PRINT_STATS               6
#define DBG_SCAN_MNGR_RESET_STATS               7
#define DBG_SCAN_MNGR_PIRNT_NEIGHBOR_APS        8
#define DBG_SCAN_MNGR_PRINT_POLICY              9
#define DBG_SCAN_MNGR_PRINT_OBJECT              10

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
 * \date 29-March-2005\n
 * \brief Main scan manager debug function
 *
 * Function Scope \e Public.\n
 * \param hScanMngr - handle to the scan manager object.\n
 * \param funcType - the specific debug function.\n
 * \param pParam - parameters for the debug function.\n
 * \param hSiteMgr - handle to th esite manager object.\n
 * \param hCtrlData - handle to the data ctrl object.\n
 */
void scanMngrDebugFunction( TI_HANDLE hScanMngr, TI_UINT32 funcType, void *pParam, TI_HANDLE hSiteMgr, TI_HANDLE hCtrlData );

/**
 * \\n
 * \date 29-March-2005\n
 * \brief Prints scan manager debug menu
 *
 * Function Scope \e Public.\n
 */
void printScanMngrDbgFunctions(void);

/**
 * \\n
 * \date 29-March-2005\n
 * \brief Starts continuous scan process.\n
 *
 * Function Scope \e Public.\n
 * \param hScanMngr - handle to the scan manager object.\n
 * \param hSiteMgr - handle to the site manager object.\n\
 * \param hCtrlData - handle to the data ctrl object.\n
 */
void startContScan( TI_HANDLE hScanMngr, TI_HANDLE hSiteMgr, TI_HANDLE hCtrlData );

#endif /* __SCANMNGRDBG_H__ */
