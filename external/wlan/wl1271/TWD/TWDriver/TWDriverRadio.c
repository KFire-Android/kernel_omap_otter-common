/*
 * TWDriverRadio.c
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


/** \file  TWDriverRadio.c 
 *  \brief TI WLAN BIT
 *
 *  \see   TWDriver.h 
 */

#define __FILE_ID__  FILE_ID_119
#include "TWDriver.h"
#include "osApi.h"
#include "TWDriverInternal.h"
#include "CmdBld.h"
/*****************************************************************************
*                                                                            *
*                       Static functions                                     *
*                                                                            *
******************************************************************************/



/*****************************************************************************
*                                                                            *
*                       API functions	                                     *
*                                                                            *
******************************************************************************/
/****************************************************************************************
 *                        TWDriverTestCB                                    
 ****************************************************************************************/
void TWDriverTestCB(TI_HANDLE hTWD, 
					TI_STATUS eStatus, 
					TI_HANDLE pTestCmdParams)
{
	TTwd *pTWD = (TTwd *)hTWD;

	if (pTWD->pRadioCb != NULL)
	{
	os_memoryCopy(NULL, pTWD->pRadioCb, &pTWD->testCmd.testCmd_u, sizeof(pTWD->testCmd.testCmd_u));
	}

	if (pTWD->fRadioCb != NULL)
	{
	pTWD->fRadioCb(pTWD->hRadioCb, eStatus, pTWD->pRadioCb);
}
}

/****************************************************************************************
 *                        TWDriverTest                                    
 ****************************************************************************************/
TI_STATUS TWDriverTest(TI_HANDLE hTWD, 
					   TestCmdID_enum eTestCmd, 
					   void* pTestCmdParams, 
					   TTestCmdCB fCb, 
					   TI_HANDLE hCb)
{
	TTwd *pTWD = (TTwd *)hTWD;

	/* check parameters */
	if (( hTWD == NULL ) ||
		( eTestCmd >= MAX_TEST_CMD_ID ) ||
		( fCb == NULL ) ||
		( hCb == NULL ))
	{
		return (TI_NOK);
	}

	pTWD->testCmd.testCmdId = eTestCmd;

	if (pTestCmdParams != NULL)
	{
	os_memoryCopy(NULL, &pTWD->testCmd.testCmd_u, pTestCmdParams, sizeof(pTWD->testCmd.testCmd_u));
	}

	pTWD->fRadioCb = fCb;
	pTWD->pRadioCb = pTestCmdParams;
	pTWD->hRadioCb = hCb;

	return(cmdBld_CmdTest (pTWD->hCmdBld, 
						   (TI_HANDLE)TWDriverTestCB, 
						   hTWD, 
						   &pTWD->testCmd));
}
