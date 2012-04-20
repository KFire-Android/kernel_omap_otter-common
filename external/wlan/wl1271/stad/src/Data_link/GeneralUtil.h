/*
 * GeneralUtil.h
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


/***************************************************************************/
/*																		   */
/*	  MODULE:	TrafficMonitor.h												       */
/*    PURPOSE:	TrafficMonitor module Header file		 							   */
/*																		   */
/***************************************************************************/
#ifndef _GENERALUTIL_H_
#define _GENERALUTIL_H_


#include "tidef.h"
#include "GeneralUtilApi.h"




/*******************************************************************************************/
typedef struct
{
	TI_HANDLE			Container;
    TI_BOOL                Inuse; 
}ListElement_t;


typedef struct
{
    int                 CurrentIndex; 
    int                 MaxNumOfElements;      
    int                 ContainerSize;    
    ListElement_t	    *ElementList;
    TI_HANDLE           hOs; 
}List_t;


/*******************************************************************************************/


/*******************************************************************************************/
typedef struct{
    List_t *DistributionList;
    TI_HANDLE hOs; 
}DistributorMgr_t;   


typedef struct{
    TI_UINT16              Mask;
    TI_BOOL                HaltReq;
    GeneralEventCall_t  CallBack;
    TI_HANDLE           Context;
    TI_UINT32              Cookie;
}NotifReqElment_t;

/*******************************************************************************************/


#endif
