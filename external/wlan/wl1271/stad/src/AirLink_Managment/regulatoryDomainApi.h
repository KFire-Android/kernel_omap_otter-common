/*
 * regulatoryDomainApi.h
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

/** \file regulatoryDomainApi.h
 *  \brief regulatoryDomain module interface header file
 *
 *  \see regulatoryDomain.c & regulatoryDomain.h
 */

/***************************************************************************/
/*                                                                          */
/*    MODULE:   regulatoryDomainApi.h                                       */
/*    PURPOSE:  regulatoryDomain module interface header file               */
/*                                                                          */
/***************************************************************************/
#ifndef __REGULATORY_DOMAIN_API_H__
#define __REGULATORY_DOMAIN_API_H__


#include "802_11Defs.h"
#include "regulatoryDomain.h"
#include "DrvMainModules.h"

TI_HANDLE regulatoryDomain_create(TI_HANDLE hOs);

void      regulatoryDomain_init (TStadHandlesList *pStadHandles);

TI_STATUS regulatoryDomain_SetDefaults (TI_HANDLE  hRegulatoryDomain,
                                        regulatoryDomainInitParams_t *pRegulatoryDomainInitParams);
/**
 * \brief	Set Regulatory Domain Parameter 
 * 
 * \param  hRegulatoryDomain	-	Handle to the regulatory domain object 
 * \param  pParam				-	Pointer to the input parameter
 * \return TI_OK on success, TI_NOK otherwise
 * 
 * \par Description
 * Configure channel validity information to the regulatory domain object.
 * called by the following:
 *	- config mgr in order to set a parameter receiving to the OS abstraction layer.
 * 	- From inside the driver
 * 
 * \sa	
 */ 
TI_STATUS regulatoryDomain_setParam(TI_HANDLE hRegulatoryDomain, paramInfo_t *pParam);
/**
 * \brief	Get Regulatory Domain Parameter 
 * 
 * \param  hRegulatoryDomain	-	Handle to the regulatory domain object 
 * \param  pParam				-	Pointer to the output parameter
 * \return TI_OK on success, TI_NOK otherwise
 * 
 * \par Description
 * Retrieves channel validity information from the regulatory domain object.
 * Called by the following:
 *	- Configuration Manager in order to get a parameter from the OS abstraction layer.
 *	- From inside the driver	 
 * 
 * \sa	
 */ 
TI_STATUS regulatoryDomain_getParam(TI_HANDLE hRegulatoryDomain, paramInfo_t *pParam);

TI_STATUS regulatoryDomain_destroy(TI_HANDLE hRegulatoryDomain);

#endif /* __REGULATORY_DOMAIN_API_H__*/


