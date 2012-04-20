/*
 * admCtrlWpa.h
 *
 * Copyright(c) 1998 - 2010 Texas Instruments. All rights reserved.      
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

/** \file admCtrlWpa.h
 *  \brief Admission control API
 *
 *  \see admCtrl.c
 */

/****************************************************************************
 *                                                                          *
 *   MODULE:  Admission Control	    		                                *
 *   PURPOSE: Admission Control Module API                              	*
 *                                                                          *
 ****************************************************************************/

#ifndef _ADM_CTRL_WPA_H_
#define _ADM_CTRL_WPA_H_

/* Constants */

/* Enumerations */

/* Typedefs */


/* RSN admission control prototypes */


/* Structures */



#define MAX_WPA_UNICAST_SUITES        (TWD_CIPHER_CKIP+1)

#define WPA_OUI_MAX_VERSION           0x1
#define WPA_OUI_DEF_TYPE              0x1
#define WPA_OUI_MAX_TYPE			  0x2

#define WPA_GROUP_4_UNICAST_CAPABILITY_MASK  	0x0002
#define WPA_REPLAY_COUNTERS_CAPABILITY_MASK 	0x000c
#define WPA_REPLAY_GROUP4UNI_CAPABILITY_SHIFT 	1
#define WPA_REPLAY_COUNTERS_CAPABILITY_SHIFT 	2

#define WPA_IE_MIN_LENGTH 				 	6
#define WPA_IE_GROUP_SUITE_LENGTH 		 	10
#define WPA_IE_MIN_PAIRWISE_SUITE_LENGTH 	16
#define WPA_IE_MIN_DEFAULT_LENGTH 			24
#define WPA_IE_MIN_KEY_MNG_SUITE_LENGTH(pairwiseCnt) (18+4*pairwiseCnt)

typedef enum 
{
	WPA_IE_KEY_MNG_NONE				= 0,		/**< no key management available */
	WPA_IE_KEY_MNG_801_1X			= 1,		/**< "802.1X" key management - WPA default*/
	WPA_IE_KEY_MNG_PSK_801_1X		= 2,		/**< "WPA PSK */
	WPA_IE_KEY_MNG_CCKM			    = 3,		/**< WPA CCKM */
	WPA_IE_KEY_MNG_NA			    = 4			/**< NA */
} keyMngSuite_e;


#define	MAX_WPA_KEY_MNG_SUITES   	(WPA_IE_KEY_MNG_CCKM+1)


typedef struct
{

	TI_UINT8  				elementid;	   /* WPA information element id is 0xDD */	   
	TI_UINT8  				length;			   
    TI_UINT8  				oui[DOT11_OUI_LEN - 1];
	TI_UINT8  				ouiType;
    TI_UINT16 				version;
    TI_UINT8 				groupSuite[DOT11_OUI_LEN];
    TI_UINT16 				pairwiseSuiteCnt;
	TI_UINT8				pairwiseSuite[DOT11_OUI_LEN];
	TI_UINT16 				authKeyMngSuiteCnt;
	TI_UINT8				authKeyMngSuite[DOT11_OUI_LEN];
    TI_UINT16				capabilities;
} wpaIePacket_t;


/* WPA capabilities structure */
typedef struct
{
    ECipherSuite 		broadcastSuite;
    TI_UINT16 			unicastSuiteCnt;
	ECipherSuite		unicastSuite[MAX_WPA_UNICAST_SUITES];
	TI_UINT16 			KeyMngSuiteCnt;
	ERsnKeyMngSuite	    KeyMngSuite[MAX_WPA_KEY_MNG_SUITES];
    TI_UINT8			bcastForUnicatst;
	TI_UINT8			replayCounters;
    TI_BOOL             XCCKp;
    TI_BOOL             XCCMic;

} wpaIeData_t;


/* External data definitions */

/* External functions definitions */

/* Function prototypes */

TI_STATUS admCtrlWpa_getInfoElement(admCtrl_t *pAdmCtrl, TI_UINT8 *pIe, TI_UINT32 *pLength);

TI_STATUS admCtrlWpa_setSite(admCtrl_t *pAdmCtrl, TRsnData *pRsnData, TI_UINT8 *pAssocIe, TI_UINT8 *pAssocIeLen);

TI_STATUS admCtrlWpa_evalSite(admCtrl_t *pAdmCtrl, TRsnData *pRsnData, TRsnSiteParams *pRsnSiteParams, TI_UINT32 *pEvaluation);

#endif /*  _ADM_CTRL_WPA_H_*/

