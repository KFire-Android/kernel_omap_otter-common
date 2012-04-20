/*
 * mlmeParser.h
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

 /** \file mlmeBuilder.h
 *  \brief 802.11 MLME Builder
 *
 *  \see mlmeBuilder.c
 */


/***************************************************************************/
/*																		   */
/*		MODULE:	mlmeBuilder.h											   */
/*    PURPOSE:	802.11 MLME Builder										   */
/*																	 	   */
/***************************************************************************/

#ifndef _MLME_PARSER_H
#define _MLME_PARSER_H

#include "802_11Defs.h"
#include "paramOut.h"
#include "mlmeApi.h"
#include "mlmeSm.h"

/* Constants */

#define FRAME_CTRL_PROTOCOL_VERSION_MASK	0x03
#define FRAME_CTRL_TYPE_MASK				0x0C
#define FRAME_CTRL_SUB_TYPE_MASK			0xF0

/* Enumerations */

/* state machine states */

/* Typedefs */

/* Structures */

/* External data definitions */

/* External functions definitions */

/* Function prototypes */

TI_STATUS mlmeParser_getFrameType(mlme_t *pMlme, 
							   TI_UINT16* pFrameCtrl, 
							   dot11MgmtSubType_e *pType);

TI_STATUS mlmeParser_readRates(mlme_t *pMlme, 
							TI_UINT8 *pData, 
							TI_UINT32 dataLen, 
							TI_UINT32 *pReadLen, 
							dot11_RATES_t *pRates);

TI_STATUS mlmeParser_readERP(mlme_t *pMlme,
                             TI_UINT8 *pData,
                             TI_UINT32 dataLen,
                             TI_UINT32 *pReadLen,
                             TI_BOOL *useProtection,
                             EPreamble *barkerPreambleMode);

TI_STATUS mlmeParser_readSsid(mlme_t *pMlme, 
						   TI_UINT8 *pData, 
						   TI_UINT32 dataLen, 
						   TI_UINT32 *pReadLen, 
						   dot11_SSID_t *pSsid);

TI_STATUS mlmeParser_readFhParams(mlme_t *pMlme, 
							   TI_UINT8 *pData, 
							   TI_UINT32 dataLen, 
							   TI_UINT32 *pReadLen, 
							   dot11_FH_PARAMS_t *pFhParams);

TI_STATUS mlmeParser_readDsParams(mlme_t *pMlme, 
							   TI_UINT8 *pData, 
							   TI_UINT32 dataLen, 
							   TI_UINT32 *pReadLen, 
							   dot11_DS_PARAMS_t *pDsParams);

TI_STATUS mlmeParser_readCfParams(mlme_t *pMlme, 
							   TI_UINT8 *pData, 
							   TI_UINT32 dataLen, 
							   TI_UINT32 *pReadLen, 
							   dot11_CF_PARAMS_t *pCfParams);

TI_STATUS mlmeParser_readIbssParams(mlme_t *pMlme, 
								 TI_UINT8 *pData, 
								 TI_UINT32 dataLen, 
								 TI_UINT32 *pReadLen, 
								 dot11_IBSS_PARAMS_t *pIbssParams);

TI_STATUS mlmeParser_readTim(mlme_t *pMlme, 
						  TI_UINT8 *pData, 
						  TI_UINT32 dataLen, 
						  TI_UINT32 *pReadLen, 
						  dot11_TIM_t *pTim);

TI_STATUS mlmeParser_readCountry(mlme_t *pMlme,
								 TI_UINT8 *pData,
								 TI_UINT32 dataLen,
								 TI_UINT32 *pReadLen,
								 dot11_COUNTRY_t *countryIE);

TI_STATUS mlmeParser_readWMEParams(mlme_t *pMlme,
								   TI_UINT8 *pData,
								   TI_UINT32 dataLen,
								   TI_UINT32 *pReadLen,
								   dot11_WME_PARAM_t *pWMEParamIE, 
								   assocRsp_t *assocRsp);

TI_STATUS mlmeParser_readPowerConstraint(mlme_t *pMlme,
										 TI_UINT8 *pData,
										 TI_UINT32 dataLen,
										 TI_UINT32 *pReadLen,
										 dot11_POWER_CONSTRAINT_t *powerConstraintIE);

TI_STATUS mlmeParser_readChannelSwitch(mlme_t *pMlme,
									   TI_UINT8 *pData,
									   TI_UINT32 dataLen,
									   TI_UINT32 *pReadLen,
									   dot11_CHANNEL_SWITCH_t *channelSwitch,
                                       TI_UINT8 channel);

TI_STATUS mlmeParser_readTPCReport(mlme_t *pMlme,
								   TI_UINT8 *pData,
								   TI_UINT32 dataLen,
								   TI_UINT32 *pReadLen,
								   dot11_TPC_REPORT_t	*TPCReport);

#ifdef XCC_MODULE_INCLUDED
TI_STATUS mlmeParser_readCellTP(mlme_t *pMlme, 
								TI_UINT8 *pData, 
								TI_UINT32 dataLen, 
								TI_UINT32 *pReadLen, 
								dot11_CELL_TP_t *cellTP);
#endif

TI_STATUS mlmeParser_readQuiet(mlme_t *pMlme,
							   TI_UINT8 *pData,
							   TI_UINT32 dataLen,
							   TI_UINT32 *pReadLen,
							   dot11_QUIET_t *quiet);

TI_STATUS mlmeParser_readChallange(mlme_t *pMlme, 
								TI_UINT8 *pData, 
								TI_UINT32 dataLen, 
								TI_UINT32 *pReadLen, 
								dot11_CHALLENGE_t *pChallange);


TI_STATUS mlmeParser_readRsnIe(mlme_t *pMlme, 
                               TI_UINT8 *pData, 
                               TI_UINT32 dataLen, 
                               TI_UINT32 *pReadLen, 
                               dot11_RSN_t *pRsnIe);

TI_STATUS mlmeParser_readQosCapabilityIE(mlme_t *pMlme,
										 TI_UINT8 *pData, 
										 TI_UINT32 dataLen, 
										 TI_UINT32 *pReadLen, 
										 dot11_QOS_CAPABILITY_IE_t *QosCapParams);

TI_STATUS mlmeParser_readHtCapabilitiesIE (mlme_t *pMlme,
										  TI_UINT8 *pData, 
										  TI_UINT32 dataLen, 
										  TI_UINT32 *pReadLen, 
										  Tdot11HtCapabilitiesUnparse *pHtCapabilities);

TI_STATUS mlmeParser_readHtInformationIE (mlme_t *pMlme,
                                          TI_UINT8 *pData, 
                                          TI_UINT32 dataLen, 
                                          TI_UINT32 *pReadLen, 
                                          Tdot11HtInformationUnparse *pHtInformation);


#endif

