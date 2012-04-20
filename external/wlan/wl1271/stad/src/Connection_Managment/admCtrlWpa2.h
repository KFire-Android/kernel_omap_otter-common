/*
 * admCtrlWpa2.h
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

/** \file admCtrlWpa2.h
 *  \brief Admission control header file for WPA2
 *
 *  \see admCtrl.c and admCtrlWpa2.c
 */

/****************************************************************************
 *                                                                          *
 *   MODULE:  Admission Control                                             *
 *   PURPOSE: Admission Control Header file for WPA2                        *
 *                                                                          *
 ****************************************************************************/

#ifndef _ADM_CTRL_WPA2_H_
#define _ADM_CTRL_WPA2_H_


/* Constants */

/* Enumerations */

/* Typedefs */

/* WPA2 configuration parameters:                                       */
/* defined here only for debugging purposes; should be moved from here  */

#define WPA2_PRE_AUTHENTICATION_SUPPORT 1
#define WPA2_PMKID_CACHE_SIZE           32
#define WPA2_CANDIDATE_LIST_MAX_SIZE    16

/* RSN admission control prototypes */


/* Structures */



#define MAX_WPA2_UNICAST_SUITES     (TWD_CIPHER_WEP104+1)
#define MAX_WPA2_KEY_MNG_SUITES     (RSN_KEY_MNG_XCC+1)

/* Cipher suites for group key sent in RSN IE are: WEP40, WEP104, TKIP, CCCMP */
#define GRP_CIPHER_MAXNO_IN_RSNIE         4

/* Cipher suites for unicast key sent in RSN IE are TKIP, CCMP, "use Group key"*/
#define UNICAST_CIPHER_MAXNO_IN_RSNIE     3

/* OUIs for cipher suites and appropriated values of cipherSuite_e (paramout.h file)
 *
 *   00-0F-AC-0   Use group cipher suite     RSN_CIPHER_NONE
 *   00-0F-AC-1   WEP-40                     RSN_CIPHER_WEP
 *   00-0F-AC-2   TKIP                       RSN_CIPHER_TKIP
 *   00-0F-AC-3   Reserved                   RSN_CIPHER_WRAP   not used for WPA2
 *   00-0F-AC-4   4                          RSN_CIPHER_CCMP
 *   00-0F-AC-5   WEP-104                    RSN_CIPHER_WEP104
 *   00-0F-AC 6   reserved 6 to 255          RSN_CIPHER_CKIP  - not used for WPA2
 *
 */

/* Key management suites (Authentication and Key Management Protocol - AKMP)  */
/* received in RSN IE                                                         */
#define KEY_MGMT_SUITE_MAXNO_IN_RSN_IE  2

/* OUIs for key management  
*
*   00-0F-AC-00  Reserved
*   00-0F-AC-01  802.1X
*   00-0F-AC-02  PSK
*   00-0F-AC-03   reserved from 3 to 255
*/

/* WPA2 key management suites */
#define WPA2_IE_KEY_MNG_NONE             0
#define WPA2_IE_KEY_MNG_801_1X           1
#define WPA2_IE_KEY_MNG_PSK_801_1X       2
#define WPA2_IE_KEY_MNG_CCKM			 3
#define WPA2_IE_KEY_MNG_NA               4


#define WPA2_OUI_MAX_VERSION           0x1
#define WPA2_OUI_DEF_TYPE              0x1
#define WPA2_OUI_MAX_TYPE              0x2

#define WPA2_PRE_AUTH_CAPABILITY_MASK               0x0001   /* bit 0 */
#define WPA2_PRE_AUTH_CAPABILITY_SHIFT              0
#define WPA2_GROUP_4_UNICAST_CAPABILITY_MASK        0x0002   /* bit 1 No Pairwise */
#define WPA2_GROUP_4_UNICAST_CAPABILITY_SHIFT        1
#define WPA2_PTK_REPLAY_COUNTERS_CAPABILITY_MASK    0x000c   /* bit 2 and 3 */
#define WPA2_PTK_REPLAY_COUNTERS_CAPABILITY_SHIFT   2
#define WPA2_GTK_REPLAY_COUNTERS_CAPABILITY_MASK    0x0030   /* bit 4 and 5 */
#define WPA2_GTK_REPLAY_COUNTERS_CAPABILITY_SHIFT   4
                                                             /* bit 6 - 15 - reserved */
#define WPA2_IE_MIN_LENGTH                  4
#define WPA2_IE_GROUP_SUITE_LENGTH          8
#define WPA2_IE_MIN_PAIRWISE_SUITE_LENGTH   14
#define WPA2_IE_MIN_DEFAULT_LENGTH          24
#define WPA2_IE_MIN_KEY_MNG_SUITE_LENGTH(pairwiseCnt) (10+4*pairwiseCnt)




/* WPA2 IE (RSN IE) packet structure                                          */
/* This structure is used for outgoing packets, i.e. for association request  */
/* For incoming packets (Beacon and Probe response from an AP) stucture of    */
/* dot11_RSN_t type is used as more common stucture                           */
typedef struct
{

    TI_UINT8               elementid;           /* WPA2 IE (RSN IE) id is 0x30 */
    TI_UINT8               length;
    TI_UINT16              version;
    TI_UINT8               groupSuite[4];       /* OUI for broadcast suite */
    TI_UINT16              pairwiseSuiteCnt;
    TI_UINT8               pairwiseSuite[4];    /* OUI for 1 unicast suite */ 
    TI_UINT16              authKeyMngSuiteCnt;
    TI_UINT8               authKeyMngSuite[4];  /* OUI for 1 key mgmt suite */
    TI_UINT16              capabilities;
    TI_UINT16              pmkIdCnt;            /* only one PMKID is supported per AP */
    TI_UINT8               pmkId[PMKID_VALUE_SIZE];
} wpa2IePacket_t;


/* WPA2 data parsed from RSN info element */
typedef struct
{

    ECipherSuite        broadcastSuite;
    TI_UINT16              unicastSuiteCnt;
    ECipherSuite        unicastSuite[MAX_WPA2_UNICAST_SUITES];
    TI_UINT16              KeyMngSuiteCnt;
    TI_UINT8               KeyMngSuite[MAX_WPA2_KEY_MNG_SUITES];
    TI_UINT8               preAuthentication;
    TI_UINT8               bcastForUnicatst;
    TI_UINT8               ptkReplayCounters;
    TI_UINT8               gtkReplayCounters;
    TI_UINT16              pmkIdCnt;
    TI_UINT8               pmkId[PMKID_VALUE_SIZE];
} wpa2IeData_t;



/* External data definitions */

/* External functions definitions */

/* Function prototypes */

TI_STATUS admCtrlWpa2_config(admCtrl_t *pAdmCtrl);

TI_STATUS admCtrlWpa2_getInfoElement(admCtrl_t *pAdmCtrl, TI_UINT8 *pIe, TI_UINT32 *pLength);

TI_STATUS admCtrlWpa2_setSite(admCtrl_t *pAdmCtrl, TRsnData *pRsnData, TI_UINT8 *pAssocIe, TI_UINT8 *pAssocIeLen);

TI_STATUS admCtrlWpa2_evalSite(admCtrl_t *pAdmCtrl, TRsnData *pRsnData, TRsnSiteParams *pRsnSiteParams, TI_UINT32 *pEvaluation);

void admCtrlWpa2_preAuthTimerExpire(TI_HANDLE hadmCtrl, TI_BOOL bTwdInitOccured);

#endif /*  _ADM_CTRL_WPA_H_*/
