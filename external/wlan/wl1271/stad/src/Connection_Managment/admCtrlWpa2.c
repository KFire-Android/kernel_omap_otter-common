/*
 * admCtrlWpa2.c
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

/** \file admCtrlWpa2.c
 *  \brief WPA2 Admission control methods
 *
 *  \see admCtrl.h
 */

/****************************************************************************
 *                                                                          *
 *   MODULE:  Admission Control                                             *
 *   PURPOSE: Admission Control Module API                                  *
 *                                                                          *
 ****************************************************************************/

#define __FILE_ID__  FILE_ID_20
#include "osApi.h"
#include "timer.h"
#include "paramOut.h"
#include "mlmeApi.h"
#include "802_11Defs.h"
#include "DataCtrl_Api.h"
#include "report.h"
#include "rsn.h"
#include "admCtrl.h"
#include "admCtrlWpa2.h"
#include "osDot11.h"
#include "siteMgrApi.h"
#include "smeApi.h"
#include "EvHandler.h"
#include "admCtrl.h"
#ifdef XCC_MODULE_INCLUDED
#include "admCtrlWpa.h"
#include "admCtrlXCC.h"
#include "XCCMngr.h"
#endif
#include "TWDriver.h"


/* Constants */
#define MAX_NETWORK_MODE 2
#define MAX_WPA2_CIPHER_SUITE 6

#define PMKID_CAND_LIST_MEMBUFF_SIZE  (2*sizeof(TI_UINT32) + (sizeof(OS_802_11_PMKID_CANDIDATE) * PMKID_MAX_NUMBER))
#define PMKID_MIN_BUFFER_SIZE    2*sizeof(TI_UINT32) + MAC_ADDR_LEN + PMKID_VALUE_SIZE

#define TI_WLAN_COPY_UINT16_UNALIGNED(addr, val) {\
    *((TI_UINT8 *) &(addr))   = (TI_UINT8)(val & 0x00FF); \
    *((TI_UINT8 *) &(addr) + 1)   = (TI_UINT8)((val & 0xFF00) >> 8);}

/* Enumerations */

/* Typedefs */

/* Structures */

/* External data definitions */

/* Local functions definitions */

/* Global variables */
static TI_UINT8 wpa2IeOuiIe[3] = { 0x00, 0x0f, 0xac};

static TI_BOOL broadcastCipherSuiteValidity[MAX_NETWORK_MODE][MAX_WPA2_CIPHER_SUITE]=
{
    /* RSN_IBSS */  {
/* NONE       */    TI_FALSE,
/* WEP40      */    TI_FALSE,
/* TKIP       */    TI_TRUE,
/* AES_WRAP   */    TI_FALSE,
/* AES_CCMP   */    TI_TRUE,
/* WEP104     */    TI_FALSE},

    /* RSN_INFRASTRUCTURE */  {
/* NONE       */    TI_FALSE,
/* WEP        */    TI_TRUE,
/* TKIP       */    TI_TRUE,
/* AES_WRAP   */    TI_FALSE,
/* AES_CCMP   */    TI_TRUE,
/* WEP104     */    TI_TRUE}
};

/** WPA2 admission table. Used to verify admission parameters to an AP */
/* table parameters:
    Max unicast cipher in the IE
    Max broadcast cipher in the IE
    Encryption status 
*/
typedef struct
{
    TI_STATUS        status;
    ECipherSuite     unicast;
    ECipherSuite     broadcast;
    TI_UINT8            evaluation; 
} admCtrlWpa2_validity_t;

static admCtrlWpa2_validity_t    admCtrlWpa2_validityTable[MAX_WPA2_CIPHER_SUITE][MAX_WPA2_CIPHER_SUITE][MAX_WPA2_CIPHER_SUITE] =
{
/* AP unicast NONE */ {
        /* AP multicast NONE */ {
            /* STA NONE */  { TI_NOK, TWD_CIPHER_NONE, TWD_CIPHER_NONE ,0},
            /* STA WEP40 */ { TI_NOK, TWD_CIPHER_NONE, TWD_CIPHER_NONE ,0},
            /* STA TKIP */  { TI_NOK, TWD_CIPHER_NONE, TWD_CIPHER_NONE ,0},
            /* STA WRAP */  { TI_NOK, TWD_CIPHER_NONE, TWD_CIPHER_NONE ,0},
            /* STA CCMP */  { TI_NOK, TWD_CIPHER_NONE, TWD_CIPHER_NONE ,0},
            /* STA WEP104 */{ TI_NOK, TWD_CIPHER_NONE, TWD_CIPHER_NONE ,0}},
        /* AP multicast WEP40 */ {
            /* STA NONE */  { TI_NOK, TWD_CIPHER_NONE, TWD_CIPHER_NONE ,0},
            /* STA WEP40 */ { TI_OK,  TWD_CIPHER_NONE, TWD_CIPHER_WEP ,1},
            /* STA TKIP */  { TI_NOK, TWD_CIPHER_NONE, TWD_CIPHER_NONE ,0},
            /* STA WRAP */  { TI_NOK, TWD_CIPHER_NONE, TWD_CIPHER_NONE ,0},
            /* STA CCMP */  { TI_NOK, TWD_CIPHER_NONE, TWD_CIPHER_NONE ,0},
            /* STA WEP104 */{ TI_OK,  TWD_CIPHER_NONE, TWD_CIPHER_WEP104 ,1}},
        /* AP multicast TKIP */ {
            /* STA NONE */  { TI_NOK, TWD_CIPHER_NONE, TWD_CIPHER_NONE ,0},
            /* STA WEP */   { TI_NOK, TWD_CIPHER_NONE, TWD_CIPHER_NONE ,0},
            /* STA TKIP */  { TI_OK,  TWD_CIPHER_NONE, TWD_CIPHER_TKIP ,2},
            /* STA WRAP */  { TI_NOK, TWD_CIPHER_NONE, TWD_CIPHER_NONE ,0},
            /* STA CCMP */  { TI_NOK, TWD_CIPHER_NONE, TWD_CIPHER_NONE ,0},
            /* STA WEP104 */{ TI_NOK, TWD_CIPHER_NONE, TWD_CIPHER_NONE ,0}},
        /* AP multicast WRAP */ {
            /* STA NONE */  { TI_NOK, TWD_CIPHER_NONE, TWD_CIPHER_NONE ,0},
            /* STA WEP */   { TI_NOK, TWD_CIPHER_NONE, TWD_CIPHER_NONE ,0},
            /* STA TKIP */  { TI_NOK, TWD_CIPHER_NONE, TWD_CIPHER_NONE ,0},
            /* STA WRAP */  { TI_OK,  TWD_CIPHER_NONE, TWD_CIPHER_AES_WRAP ,3},
            /* STA CCMP */  { TI_NOK, TWD_CIPHER_NONE, TWD_CIPHER_NONE ,0},
            /* STA WEP104 */{ TI_NOK, TWD_CIPHER_NONE, TWD_CIPHER_NONE ,0}},
        /* AP multicast CCMP */ {
            /* STA NONE */  { TI_NOK, TWD_CIPHER_NONE, TWD_CIPHER_NONE ,0},
            /* STA WEP */   { TI_NOK, TWD_CIPHER_NONE, TWD_CIPHER_NONE ,0},
            /* STA TKIP */  { TI_NOK, TWD_CIPHER_NONE, TWD_CIPHER_NONE ,0},
            /* STA WRAP */  { TI_NOK, TWD_CIPHER_NONE, TWD_CIPHER_NONE ,0},
            /* STA CCMP */  { TI_OK,  TWD_CIPHER_NONE, TWD_CIPHER_AES_CCMP ,3},
            /* STA WEP104 */{ TI_NOK, TWD_CIPHER_NONE, TWD_CIPHER_NONE ,0}},
        /* AP multicast WEP104 */ {
            /* STA NONE */  { TI_NOK, TWD_CIPHER_NONE, TWD_CIPHER_NONE ,0},
            /* STA WEP40 */ { TI_OK,  TWD_CIPHER_NONE, TWD_CIPHER_WEP ,1},
            /* STA TKIP */  { TI_NOK, TWD_CIPHER_NONE, TWD_CIPHER_NONE ,0},
            /* STA WRAP */  { TI_NOK, TWD_CIPHER_NONE, TWD_CIPHER_NONE ,0},
            /* STA CCMP */  { TI_NOK, TWD_CIPHER_NONE, TWD_CIPHER_NONE ,0},
            /* STA WEP104 */{ TI_OK,  TWD_CIPHER_NONE, TWD_CIPHER_WEP104 ,1}}},
/* AP unicast WEP */  {
        /* AP multicast NONE */ {
            /* STA NONE */  { TI_NOK, TWD_CIPHER_NONE, TWD_CIPHER_NONE ,0},
            /* STA WEP */   { TI_NOK, TWD_CIPHER_NONE, TWD_CIPHER_NONE ,0},
            /* STA TKIP */  { TI_NOK, TWD_CIPHER_NONE, TWD_CIPHER_NONE ,0},
            /* STA WRAP */  { TI_NOK, TWD_CIPHER_NONE, TWD_CIPHER_NONE ,0},
            /* STA CCMP */  { TI_NOK, TWD_CIPHER_NONE, TWD_CIPHER_NONE ,0},
            /* STA WEP104 */{ TI_NOK, TWD_CIPHER_NONE, TWD_CIPHER_NONE ,0}},
        /* AP multicast WEP */ {
            /* STA NONE */  { TI_NOK, TWD_CIPHER_NONE, TWD_CIPHER_NONE ,0},
            /* STA WEP */   { TI_NOK, TWD_CIPHER_NONE, TWD_CIPHER_NONE ,0},
            /* STA TKIP */  { TI_NOK, TWD_CIPHER_NONE, TWD_CIPHER_NONE ,0},
            /* STA WRAP */  { TI_NOK, TWD_CIPHER_NONE, TWD_CIPHER_NONE ,0},
            /* STA CCMP */  { TI_NOK, TWD_CIPHER_NONE, TWD_CIPHER_NONE ,0},
            /* STA WEP104 */{ TI_NOK, TWD_CIPHER_NONE, TWD_CIPHER_NONE ,0}},
        /* AP multicast TKIP */ {
            /* STA NONE */  { TI_NOK, TWD_CIPHER_NONE, TWD_CIPHER_NONE ,0},
            /* STA WEP */   { TI_NOK, TWD_CIPHER_NONE, TWD_CIPHER_NONE ,0},
            /* STA TKIP */  { TI_NOK, TWD_CIPHER_NONE, TWD_CIPHER_NONE ,0},
            /* STA AES */   { TI_NOK, TWD_CIPHER_NONE, TWD_CIPHER_NONE ,0},
            /* STA CCMP */  { TI_NOK, TWD_CIPHER_NONE, TWD_CIPHER_NONE ,0},
            /* STA WEP104 */{ TI_NOK, TWD_CIPHER_NONE, TWD_CIPHER_NONE ,0}},
        /* AP multicast WRAP */ {
            /* STA NONE */  { TI_NOK, TWD_CIPHER_NONE, TWD_CIPHER_NONE ,0},
            /* STA WEP */   { TI_NOK, TWD_CIPHER_NONE, TWD_CIPHER_NONE ,0},
            /* STA TKIP */  { TI_NOK, TWD_CIPHER_NONE, TWD_CIPHER_NONE ,0},
            /* STA WRAP */  { TI_NOK, TWD_CIPHER_NONE, TWD_CIPHER_NONE ,0},
            /* STA CCMP */  { TI_NOK, TWD_CIPHER_NONE, TWD_CIPHER_NONE ,0},
            /* STA WEP104 */{ TI_NOK, TWD_CIPHER_NONE, TWD_CIPHER_NONE ,0}},
        /* AP multicast CCMP */ {
            /* STA NONE */  { TI_NOK, TWD_CIPHER_NONE, TWD_CIPHER_NONE ,0},
            /* STA WEP */   { TI_NOK, TWD_CIPHER_NONE, TWD_CIPHER_NONE ,0},
            /* STA TKIP */  { TI_NOK, TWD_CIPHER_NONE, TWD_CIPHER_NONE ,0},
            /* STA AES */   { TI_NOK, TWD_CIPHER_NONE, TWD_CIPHER_NONE ,0},
            /* STA CCMP */  { TI_NOK, TWD_CIPHER_NONE, TWD_CIPHER_NONE ,0},
            /* STA WEP104 */{ TI_NOK, TWD_CIPHER_NONE, TWD_CIPHER_NONE ,0}},
        /* AP multicast WEP104 */ {
            /* STA NONE */  { TI_NOK, TWD_CIPHER_NONE, TWD_CIPHER_NONE ,0},
            /* STA WEP */   { TI_NOK, TWD_CIPHER_NONE, TWD_CIPHER_NONE ,0},
            /* STA TKIP */  { TI_NOK, TWD_CIPHER_NONE, TWD_CIPHER_NONE ,0},
            /* STA AES */   { TI_NOK, TWD_CIPHER_NONE, TWD_CIPHER_NONE ,0},
            /* STA CCMP */  { TI_NOK, TWD_CIPHER_NONE, TWD_CIPHER_NONE ,0},
            /* STA WEP104 */{ TI_NOK, TWD_CIPHER_NONE, TWD_CIPHER_NONE ,0}}},
/* AP unicast TKIP */  {
        /* AP multicast NONE */ {
            /* STA NONE */  { TI_NOK, TWD_CIPHER_NONE, TWD_CIPHER_NONE ,0},
            /* STA WEP */   { TI_NOK, TWD_CIPHER_NONE, TWD_CIPHER_NONE ,0},
            /* STA TKIP */  { TI_NOK, TWD_CIPHER_NONE, TWD_CIPHER_NONE ,0},
            /* STA AES */   { TI_NOK, TWD_CIPHER_NONE, TWD_CIPHER_NONE ,0},
            /* STA CCMP */  { TI_NOK, TWD_CIPHER_NONE, TWD_CIPHER_NONE ,0},
            /* STA WEP104 */{ TI_NOK, TWD_CIPHER_NONE, TWD_CIPHER_NONE ,0}},
        /* AP multicast WEP */ {
            /* STA NONE */  { TI_NOK, TWD_CIPHER_NONE, TWD_CIPHER_NONE ,0},
            /* STA WEP */   { TI_NOK, TWD_CIPHER_NONE, TWD_CIPHER_NONE ,0},
            /* STA TKIP */  { TI_OK,  TWD_CIPHER_TKIP, TWD_CIPHER_WEP  ,4},
            /* STA AES */   { TI_NOK, TWD_CIPHER_NONE, TWD_CIPHER_NONE ,0},
            /* STA CCMP */  { TI_NOK, TWD_CIPHER_NONE, TWD_CIPHER_NONE ,0},
            /* STA WEP104 */{ TI_NOK, TWD_CIPHER_NONE, TWD_CIPHER_NONE ,0}},
        /* AP multicast TKIP */ {
            /* STA NONE */  { TI_NOK, TWD_CIPHER_NONE, TWD_CIPHER_NONE ,0},
            /* STA WEP */   { TI_NOK, TWD_CIPHER_NONE, TWD_CIPHER_NONE ,0},
            /* STA TKIP */  { TI_OK,  TWD_CIPHER_TKIP, TWD_CIPHER_TKIP ,7},
            /* STA AES */   { TI_NOK, TWD_CIPHER_NONE, TWD_CIPHER_NONE ,0},
            /* STA CCMP */  { TI_NOK, TWD_CIPHER_NONE, TWD_CIPHER_NONE ,0},
            /* STA WEP104 */{ TI_NOK, TWD_CIPHER_NONE, TWD_CIPHER_NONE ,0}},
        /* AP multicast WRAP */ {
            /* STA NONE */  { TI_NOK, TWD_CIPHER_NONE, TWD_CIPHER_NONE ,0},
            /* STA WEP */   { TI_NOK, TWD_CIPHER_NONE, TWD_CIPHER_NONE ,0},
            /* STA TKIP */  { TI_NOK, TWD_CIPHER_NONE, TWD_CIPHER_NONE ,0},
            /* STA AES */   { TI_NOK, TWD_CIPHER_NONE, TWD_CIPHER_NONE ,0},
            /* STA CCMP */  { TI_NOK, TWD_CIPHER_NONE, TWD_CIPHER_NONE ,0},
            /* STA WEP104 */{ TI_NOK, TWD_CIPHER_NONE, TWD_CIPHER_NONE ,0}},
        /* AP multicast CCMP */ {
            /* STA NONE */  { TI_NOK, TWD_CIPHER_NONE, TWD_CIPHER_NONE ,0},
            /* STA WEP */   { TI_NOK, TWD_CIPHER_NONE, TWD_CIPHER_NONE ,0},
            /* STA TKIP */  { TI_NOK, TWD_CIPHER_NONE, TWD_CIPHER_NONE ,0},
            /* STA AES */   { TI_NOK, TWD_CIPHER_NONE, TWD_CIPHER_NONE ,0},
            /* STA CCMP */  { TI_NOK, TWD_CIPHER_NONE, TWD_CIPHER_NONE ,0},
            /* STA WEP104 */{ TI_NOK, TWD_CIPHER_NONE, TWD_CIPHER_NONE ,0}},
        /* AP multicast WEP104 */ {
            /* STA NONE */  { TI_NOK, TWD_CIPHER_NONE, TWD_CIPHER_NONE ,0},
            /* STA WEP */   { TI_NOK, TWD_CIPHER_NONE, TWD_CIPHER_NONE ,0},
            /* STA TKIP */  { TI_OK,  TWD_CIPHER_TKIP, TWD_CIPHER_WEP104 ,4},
            /* STA AES */   { TI_NOK, TWD_CIPHER_NONE, TWD_CIPHER_NONE ,0},
            /* STA CCMP */  { TI_NOK, TWD_CIPHER_NONE, TWD_CIPHER_NONE ,0},
            /* STA WEP104 */{ TI_NOK, TWD_CIPHER_NONE, TWD_CIPHER_NONE ,0}}},
/* AP unicast AES_WRAP */ {
        /* AP multicast NONE */ {
            /* STA NONE */  { TI_NOK, TWD_CIPHER_NONE, TWD_CIPHER_NONE ,0},
            /* STA WEP */   { TI_NOK, TWD_CIPHER_NONE, TWD_CIPHER_NONE ,0},
            /* STA TKIP */  { TI_NOK, TWD_CIPHER_NONE, TWD_CIPHER_NONE ,0},
            /* STA AES */   { TI_NOK, TWD_CIPHER_NONE, TWD_CIPHER_NONE ,0},
            /* STA CCMP */  { TI_NOK, TWD_CIPHER_NONE, TWD_CIPHER_NONE ,0},
            /* STA WEP104 */{ TI_NOK, TWD_CIPHER_NONE, TWD_CIPHER_NONE ,0}},
        /* AP multicast WEP40 */ {
            /* STA NONE */  { TI_NOK, TWD_CIPHER_NONE, TWD_CIPHER_NONE ,0},
            /* STA WEP */   { TI_NOK, TWD_CIPHER_NONE, TWD_CIPHER_NONE ,0},
            /* STA TKIP */  { TI_NOK, TWD_CIPHER_NONE, TWD_CIPHER_NONE ,0},
            /* STA AES */   { TI_OK,  TWD_CIPHER_AES_WRAP, TWD_CIPHER_WEP ,5},
            /* STA CCMP */  { TI_NOK, TWD_CIPHER_NONE, TWD_CIPHER_NONE ,0},
            /* STA WEP104 */{ TI_NOK, TWD_CIPHER_NONE, TWD_CIPHER_NONE ,0}},
        /* AP multicast TKIP */ {
            /* STA NONE */  { TI_NOK, TWD_CIPHER_NONE, TWD_CIPHER_NONE ,0},
            /* STA WEP */   { TI_NOK, TWD_CIPHER_NONE, TWD_CIPHER_NONE ,0},
            /* STA TKIP */  { TI_NOK, TWD_CIPHER_NONE, TWD_CIPHER_NONE ,0},
            /* STA AES */   { TI_OK,  TWD_CIPHER_AES_WRAP, TWD_CIPHER_TKIP ,6},
            /* STA CCMP */  { TI_NOK, TWD_CIPHER_NONE, TWD_CIPHER_NONE ,0},
            /* STA WEP104 */{ TI_NOK, TWD_CIPHER_NONE, TWD_CIPHER_NONE ,0}},
        /* AP multicast WRAP */ {
            /* STA NONE */  { TI_NOK, TWD_CIPHER_NONE, TWD_CIPHER_NONE ,0},
            /* STA WEP */   { TI_NOK, TWD_CIPHER_NONE, TWD_CIPHER_NONE ,0},
            /* STA TKIP */  { TI_NOK, TWD_CIPHER_NONE, TWD_CIPHER_NONE ,0},
            /* STA AES */   { TI_OK,  TWD_CIPHER_AES_WRAP, TWD_CIPHER_AES_WRAP ,8},
            /* STA CCMP */  { TI_NOK, TWD_CIPHER_NONE, TWD_CIPHER_NONE ,0},
            /* STA WEP104 */{ TI_NOK, TWD_CIPHER_NONE, TWD_CIPHER_NONE ,0}},
        /* AP multicast CCMP */ {
            /* STA NONE */  { TI_NOK, TWD_CIPHER_NONE, TWD_CIPHER_NONE ,0},
            /* STA WEP */   { TI_NOK, TWD_CIPHER_NONE, TWD_CIPHER_NONE ,0},
            /* STA TKIP */  { TI_NOK, TWD_CIPHER_NONE, TWD_CIPHER_NONE ,0},
            /* STA AES */   { TI_NOK, TWD_CIPHER_NONE, TWD_CIPHER_NONE ,0},
            /* STA CCMP */  { TI_NOK, TWD_CIPHER_NONE, TWD_CIPHER_NONE ,0},
            /* STA WEP104 */{ TI_NOK, TWD_CIPHER_NONE, TWD_CIPHER_NONE ,0}},
        /* AP multicast WEP104 */ {
            /* STA NONE */  { TI_NOK, TWD_CIPHER_NONE, TWD_CIPHER_NONE ,0},
            /* STA WEP */   { TI_NOK, TWD_CIPHER_NONE, TWD_CIPHER_NONE ,0},
            /* STA TKIP */  { TI_NOK, TWD_CIPHER_NONE, TWD_CIPHER_NONE ,0},
            /* STA AES */   { TI_OK,  TWD_CIPHER_AES_WRAP, TWD_CIPHER_WEP104 ,5},
            /* STA CCMP */  { TI_NOK, TWD_CIPHER_NONE, TWD_CIPHER_NONE ,0},
            /* STA WEP104 */{ TI_NOK, TWD_CIPHER_NONE, TWD_CIPHER_NONE ,0}}},
/* AP unicast AES_CCMP */ {
        /* AP multicast NONE */ {
            /* STA NONE */  { TI_NOK, TWD_CIPHER_NONE, TWD_CIPHER_NONE ,0},
            /* STA WEP */   { TI_NOK, TWD_CIPHER_NONE, TWD_CIPHER_NONE ,0},
            /* STA TKIP */  { TI_NOK, TWD_CIPHER_NONE, TWD_CIPHER_NONE ,0},
            /* STA AES */   { TI_NOK, TWD_CIPHER_NONE, TWD_CIPHER_NONE ,0},
            /* STA CCMP */  { TI_NOK, TWD_CIPHER_NONE, TWD_CIPHER_NONE ,0},
            /* STA WEP104 */{ TI_NOK, TWD_CIPHER_NONE, TWD_CIPHER_NONE ,0}},
        /* AP multicast WEP */ {
            /* STA NONE */  { TI_NOK, TWD_CIPHER_NONE, TWD_CIPHER_NONE ,0},
            /* STA WEP */   { TI_NOK, TWD_CIPHER_NONE, TWD_CIPHER_NONE ,0},
            /* STA TKIP */  { TI_NOK, TWD_CIPHER_NONE, TWD_CIPHER_NONE ,0},
            /* STA AES */   { TI_NOK, TWD_CIPHER_NONE, TWD_CIPHER_NONE ,0},
            /* STA CCMP */  { TI_OK,  TWD_CIPHER_AES_CCMP, TWD_CIPHER_WEP ,5},
            /* STA WEP104 */{ TI_NOK, TWD_CIPHER_NONE, TWD_CIPHER_NONE ,0}},
        /* AP multicast TKIP */ {
            /* STA NONE */  { TI_NOK, TWD_CIPHER_NONE, TWD_CIPHER_NONE ,0},
            /* STA WEP */   { TI_NOK, TWD_CIPHER_NONE, TWD_CIPHER_NONE ,0},
            /* STA TKIP */  { TI_OK,  TWD_CIPHER_AES_CCMP, TWD_CIPHER_TKIP ,6},
            /* STA AES */   { TI_NOK, TWD_CIPHER_NONE, TWD_CIPHER_NONE ,0},
            /* STA CCMP */  { TI_OK,  TWD_CIPHER_AES_CCMP, TWD_CIPHER_TKIP ,6},
            /* STA WEP104 */{ TI_NOK, TWD_CIPHER_NONE, TWD_CIPHER_NONE ,0}},
        /* AP multicast WRAP */ {
            /* STA NONE */  { TI_NOK, TWD_CIPHER_NONE, TWD_CIPHER_NONE ,0},
            /* STA WEP */   { TI_NOK, TWD_CIPHER_NONE, TWD_CIPHER_NONE ,0},
            /* STA TKIP */  { TI_NOK, TWD_CIPHER_NONE, TWD_CIPHER_NONE ,0},
            /* STA AES */   { TI_NOK, TWD_CIPHER_NONE, TWD_CIPHER_NONE ,0},
            /* STA CCMP */  { TI_NOK, TWD_CIPHER_NONE, TWD_CIPHER_NONE ,0},
            /* STA WEP104 */{ TI_NOK, TWD_CIPHER_NONE, TWD_CIPHER_NONE ,0}},
        /* AP multicast CCMP */ {
            /* STA NONE */  { TI_NOK, TWD_CIPHER_NONE, TWD_CIPHER_NONE ,0},
            /* STA WEP */   { TI_NOK, TWD_CIPHER_NONE, TWD_CIPHER_NONE ,0},
            /* STA TKIP */  { TI_OK,  TWD_CIPHER_AES_CCMP, TWD_CIPHER_AES_CCMP ,6},
            /* STA AES */   { TI_NOK, TWD_CIPHER_NONE, TWD_CIPHER_NONE ,0},
            /* STA CCMP */  { TI_OK,  TWD_CIPHER_AES_CCMP, TWD_CIPHER_AES_CCMP ,8},
            /* STA WEP104 */{ TI_NOK, TWD_CIPHER_NONE, TWD_CIPHER_NONE ,0}},
        /* AP multicast WEP */ {
            /* STA NONE */  { TI_NOK, TWD_CIPHER_NONE, TWD_CIPHER_NONE ,0},
            /* STA WEP */   { TI_NOK, TWD_CIPHER_NONE, TWD_CIPHER_NONE ,0},
            /* STA TKIP */  { TI_NOK, TWD_CIPHER_NONE, TWD_CIPHER_NONE ,0},
            /* STA AES */   { TI_NOK, TWD_CIPHER_NONE, TWD_CIPHER_NONE ,0},
            /* STA CCMP */  { TI_OK,  TWD_CIPHER_AES_CCMP, TWD_CIPHER_WEP104 ,5},
            /* STA WEP104 */{ TI_NOK, TWD_CIPHER_NONE, TWD_CIPHER_NONE ,0}}},
/* AP unicast WEP104 */  {
        /* AP multicast NONE */ {
            /* STA NONE */  { TI_NOK, TWD_CIPHER_NONE, TWD_CIPHER_NONE ,0},
            /* STA WEP */   { TI_NOK, TWD_CIPHER_NONE, TWD_CIPHER_NONE ,0},
            /* STA TKIP */  { TI_NOK, TWD_CIPHER_NONE, TWD_CIPHER_NONE ,0},
            /* STA AES */   { TI_NOK, TWD_CIPHER_NONE, TWD_CIPHER_NONE ,0},
            /* STA CCMP */  { TI_NOK, TWD_CIPHER_NONE, TWD_CIPHER_NONE ,0},
            /* STA WEP104 */{ TI_NOK, TWD_CIPHER_NONE, TWD_CIPHER_NONE ,0}},
        /* AP multicast WEP */ {
            /* STA NONE */  { TI_NOK, TWD_CIPHER_NONE, TWD_CIPHER_NONE ,0},
            /* STA WEP */   { TI_NOK, TWD_CIPHER_NONE, TWD_CIPHER_NONE ,0},
            /* STA TKIP */  { TI_NOK, TWD_CIPHER_NONE, TWD_CIPHER_NONE ,0},
            /* STA AES */   { TI_NOK, TWD_CIPHER_NONE, TWD_CIPHER_NONE ,0},
            /* STA CCMP */  { TI_NOK, TWD_CIPHER_NONE, TWD_CIPHER_NONE ,0},
            /* STA WEP104 */{ TI_NOK, TWD_CIPHER_NONE, TWD_CIPHER_NONE ,0}},
        /* AP multicast TKIP */ {
            /* STA NONE */  { TI_NOK, TWD_CIPHER_NONE, TWD_CIPHER_NONE ,0},
            /* STA WEP */   { TI_NOK, TWD_CIPHER_NONE, TWD_CIPHER_NONE ,0},
            /* STA TKIP */  { TI_NOK, TWD_CIPHER_NONE, TWD_CIPHER_NONE ,0},
            /* STA AES */   { TI_NOK, TWD_CIPHER_NONE, TWD_CIPHER_NONE ,0},
            /* STA CCMP */  { TI_NOK, TWD_CIPHER_NONE, TWD_CIPHER_NONE ,0},
            /* STA WEP104 */{ TI_NOK, TWD_CIPHER_NONE, TWD_CIPHER_NONE ,0}},
        /* AP multicast WRAP */ {
            /* STA NONE */  { TI_NOK, TWD_CIPHER_NONE, TWD_CIPHER_NONE ,0},
            /* STA WEP */   { TI_NOK, TWD_CIPHER_NONE, TWD_CIPHER_NONE ,0},
            /* STA TKIP */  { TI_NOK, TWD_CIPHER_NONE, TWD_CIPHER_NONE ,0},
            /* STA AES */   { TI_NOK, TWD_CIPHER_NONE, TWD_CIPHER_NONE ,0},
            /* STA CCMP */  { TI_NOK, TWD_CIPHER_NONE, TWD_CIPHER_NONE ,0},
            /* STA WEP104 */{ TI_NOK, TWD_CIPHER_NONE, TWD_CIPHER_NONE ,0}},
        /* AP multicast CCMP */ {
            /* STA NONE */  { TI_NOK, TWD_CIPHER_NONE, TWD_CIPHER_NONE ,0},
            /* STA WEP */   { TI_NOK, TWD_CIPHER_NONE, TWD_CIPHER_NONE ,0},
            /* STA TKIP */  { TI_NOK, TWD_CIPHER_NONE, TWD_CIPHER_NONE ,0},
            /* STA AES */   { TI_NOK, TWD_CIPHER_NONE, TWD_CIPHER_NONE ,0},
            /* STA CCMP */  { TI_NOK, TWD_CIPHER_NONE, TWD_CIPHER_NONE ,0},
            /* STA WEP104 */{ TI_NOK, TWD_CIPHER_NONE, TWD_CIPHER_NONE ,0}},
        /* AP multicast WEP104 */ {
            /* STA NONE */  { TI_NOK, TWD_CIPHER_NONE, TWD_CIPHER_NONE ,0},
            /* STA WEP */   { TI_NOK, TWD_CIPHER_NONE, TWD_CIPHER_NONE ,0},
            /* STA TKIP */  { TI_NOK, TWD_CIPHER_NONE, TWD_CIPHER_NONE ,0},
            /* STA AES */   { TI_NOK, TWD_CIPHER_NONE, TWD_CIPHER_NONE ,0},
            /* STA CCMP */  { TI_NOK, TWD_CIPHER_NONE, TWD_CIPHER_NONE ,0},
            /* STA WEP104 */{ TI_NOK, TWD_CIPHER_NONE, TWD_CIPHER_NONE ,0}}}


};


/* PMKID cache */
/* static wpa2_pmkid_cache_t wpa2_pmkid_cache; */

/* Function prototypes */

TI_STATUS admCtrlWpa2_parseIe(admCtrl_t *pAdmCtrl, TI_UINT8 *pWpa2Ie, wpa2IeData_t *pWpa2Data);
TI_UINT16 admCtrlWpa2_buildCapabilities(admCtrl_t *pAdmCtrl);
TI_UINT32  admCtrlWpa2_parseSuiteVal(admCtrl_t *pAdmCtrl, TI_UINT8* suiteVal, TI_UINT32 maxVal, TI_UINT32 unknownVal);
TI_STATUS admCtrlWpa2_checkCipherSuiteValidity(ECipherSuite unicastSuite, ECipherSuite broadcastSuite, ECipherSuite encryptionStatus);
TI_STATUS admCtrlWpa2_getCipherSuiteMetric (admCtrl_t *pAdmCtrl, wpa2IeData_t *pWpa2Data, TI_UINT32 *metric, 
                                            ECipherSuite *uSuite,  ECipherSuite  *bSuite);
TI_STATUS admCtrlWpa2_DynamicConfig(admCtrl_t *pAdmCtrl, TRsnPaeConfig *pPaeConfig);

TI_STATUS admCtrlWpa2_resetPMKIDCache(admCtrl_t *pAdmCtrl);
/*TI_STATUS admCtrlWpa2_sendPMKIDCandListAfterDelay(admCtrl_t * pAdmCtrl, TI_UINT32 delay);*/
TI_STATUS admCtrlWpa2_getPMKIDList(admCtrl_t * pAdmCtrl,OS_802_11_PMKID *pmkidList);
TI_STATUS admCtrlWpa2_setPMKIDList(admCtrl_t * pAdmCtrl, OS_802_11_PMKID *pmkidList);

TI_STATUS admCtrlWpa2_addPMKID(admCtrl_t * pAdmCtrl, TMacAddr * pBSSID, pmkidValue_t pmkID);
TI_STATUS admCtrlWpa2_findPMKID(admCtrl_t * pAdmCtrl, TMacAddr *pBSSID, 
                                pmkidValue_t *pPMKID, TI_UINT8  *cacheIndex);

static TI_BOOL admCtrlWpa2_getPreAuthStatus(admCtrl_t *pAdmCtrl, TMacAddr *givenAP, TI_UINT8  *cacheIndex);

static TI_STATUS admCtrlWpa2_startPreAuth(admCtrl_t *pAdmCtrl, TBssidList4PreAuth *pBssidList);

static void admCtrlWpa2_buildAndSendPMKIDCandList(TI_HANDLE hHandle, TBssidList4PreAuth *apList);

static TI_STATUS admCtrlWpa2_get802_1x_AkmExists (admCtrl_t *pAdmCtrl, TI_BOOL *wpa_802_1x_AkmExists);

/**
*
* admCtrlWpa_config  - Configure XCC admission control.
*
* \b Description: 
*
* Configure XCC admission control.
*
* \b ARGS:
*
*  I   - pAdmCtrl - context \n
*  
* \b RETURNS:
*
*  TI_OK on success, TI_NOK on failure.
*
* \sa 
*/
TI_STATUS admCtrlWpa2_config(admCtrl_t *pAdmCtrl)
{
    TI_STATUS           status;
    TRsnPaeConfig     paeConfig;

    /* check and set admission control default parameters */
    pAdmCtrl->authSuite =   RSN_AUTH_OPEN;
    if (pAdmCtrl->unicastSuite == TWD_CIPHER_NONE)
    {
        pAdmCtrl->unicastSuite = TWD_CIPHER_AES_CCMP;
    }
    if (pAdmCtrl->broadcastSuite == TWD_CIPHER_NONE)
    {
        pAdmCtrl->broadcastSuite = TWD_CIPHER_AES_CCMP;
    }

    /* set callback functions (API) */
    pAdmCtrl->getInfoElement = admCtrlWpa2_getInfoElement;
    pAdmCtrl->setSite  = admCtrlWpa2_setSite;
    pAdmCtrl->evalSite = admCtrlWpa2_evalSite;

    pAdmCtrl->getPmkidList      = admCtrlWpa2_getPMKIDList;
    pAdmCtrl->setPmkidList      = admCtrlWpa2_setPMKIDList;
    pAdmCtrl->resetPmkidList    = admCtrlWpa2_resetPMKIDCache;
    pAdmCtrl->getPreAuthStatus = admCtrlWpa2_getPreAuthStatus;
    pAdmCtrl->startPreAuth = admCtrlWpa2_startPreAuth;
    pAdmCtrl->get802_1x_AkmExists = admCtrlWpa2_get802_1x_AkmExists;

    /* set key management suite (AKMP) */
    switch (pAdmCtrl->externalAuthMode)
    {
    case RSN_EXT_AUTH_MODE_WPA2:
    case RSN_EXT_AUTH_MODE_WPA2PSK:
        pAdmCtrl->keyMngSuite = RSN_KEY_MNG_802_1X;
        break;
    case RSN_EXT_AUTH_MODE_WPANONE:
        pAdmCtrl->keyMngSuite = RSN_KEY_MNG_NONE;
        /* Not supported */
    default:
        return TI_NOK;
    }


    paeConfig.authProtocol = pAdmCtrl->externalAuthMode;
    paeConfig.unicastSuite = pAdmCtrl->unicastSuite;
    paeConfig.broadcastSuite = pAdmCtrl->broadcastSuite;
    paeConfig.keyExchangeProtocol = pAdmCtrl->keyMngSuite;
    /* set default PAE configuration */
    status = pAdmCtrl->pRsn->setPaeConfig(pAdmCtrl->pRsn, &paeConfig);

    return status;
}


/**
*
* admCtrlWpa2_getInfoElement - Get the current information element.
*
* \b Description: 
*
* Get the current information element.
*
* \b ARGS:
*
*  I   - pAdmCtrl - context \n
*  I   - pIe - IE buffer \n
*  I   - pLength - length of IE \n
*  
* \b RETURNS:
*
*  TI_OK on success, TI_NOK on failure.
*
* \sa 
*/

TI_STATUS admCtrlWpa2_getInfoElement(admCtrl_t *pAdmCtrl, TI_UINT8 *pIe, TI_UINT32 *pLength)
{
    wpa2IePacket_t     *pWpa2IePacket;
    TI_UINT8           length = 0;
    TMacAddr           assocBssid;
    TMacAddr           pBssid;
    pmkidValue_t       pmkId;
    TI_STATUS          status;
    TI_UINT8           index;

    if (pIe==NULL)
    {
        *pLength = 0;
        return TI_NOK;
    }

    /* check Group suite validity */
    if (!broadcastCipherSuiteValidity[pAdmCtrl->networkMode][pAdmCtrl->broadcastSuite])
    {
        *pLength = 0;
        return TI_NOK;
    }

    /* Init Wpa2 IE (RSN IE) */
    pWpa2IePacket = (wpa2IePacket_t*)pIe;
    os_memoryZero(pAdmCtrl->hOs, pWpa2IePacket, sizeof(wpa2IePacket_t));
    /* Fill the element ID */
    pWpa2IePacket->elementid = RSN_IE_ID;
    SET_WLAN_WORD(&pWpa2IePacket->version,ENDIAN_HANDLE_WORD(WPA2_OUI_MAX_VERSION));
    length += 2;
    /* build group suite */
    os_memoryCopy(pAdmCtrl->hOs, (void *)pWpa2IePacket->groupSuite, wpa2IeOuiIe, 3);
    pWpa2IePacket->groupSuite[3] = (TI_UINT8)pAdmCtrl->pRsn->paeConfig.broadcastSuite;
    length += 4;
    /* build pairwise suite - we always send only one pairwise suite */
    SET_WLAN_WORD(&pWpa2IePacket->pairwiseSuiteCnt,ENDIAN_HANDLE_WORD(0x0001));
    length += 2;
    os_memoryCopy(pAdmCtrl->hOs, (void *)pWpa2IePacket->pairwiseSuite, wpa2IeOuiIe, 3);
    pWpa2IePacket->pairwiseSuite[3] = (TI_UINT8)pAdmCtrl->pRsn->paeConfig.unicastSuite;
    length += 4;    
    /* build keyMng suite - we always send only one key mgmt  suite*/
    SET_WLAN_WORD(&pWpa2IePacket->authKeyMngSuiteCnt,ENDIAN_HANDLE_WORD(0x0001));
    length += 2;
    os_memoryCopy(pAdmCtrl->hOs, (void *)pWpa2IePacket->authKeyMngSuite, wpa2IeOuiIe, 3);    
    switch (pAdmCtrl->externalAuthMode)
    {
    case RSN_EXT_AUTH_MODE_OPEN:
    case RSN_EXT_AUTH_MODE_SHARED_KEY:
    case RSN_EXT_AUTH_MODE_AUTO_SWITCH:
        pWpa2IePacket->authKeyMngSuite[3] = WPA2_IE_KEY_MNG_NONE;
        break;
    case RSN_EXT_AUTH_MODE_WPA2:
    case RSN_EXT_AUTH_MODE_WPA:   /* for Any-WPA/WPA-Mixed mode */
        {
#ifdef XCC_MODULE_INCLUDED
            TI_UINT8   akmSuite[DOT11_OUI_LEN];

            if (admCtrlXCC_getCckmAkm(pAdmCtrl, akmSuite))
            {
                os_memoryCopy(pAdmCtrl->hOs, (void*)pWpa2IePacket->authKeyMngSuite, akmSuite, DOT11_OUI_LEN);
            }
            else
#endif
            {
                pWpa2IePacket->authKeyMngSuite[3] = WPA2_IE_KEY_MNG_801_1X;
            }
        }
        break;
    case RSN_EXT_AUTH_MODE_WPA2PSK:
    case RSN_EXT_AUTH_MODE_WPAPSK:
        pWpa2IePacket->authKeyMngSuite[3] = WPA2_IE_KEY_MNG_PSK_801_1X;
        break;
    default:
        pWpa2IePacket->authKeyMngSuite[3] = WPA2_IE_KEY_MNG_NONE;
        break;
    }
    length += 4;   
    /* build Capabilities */
    SET_WLAN_WORD(&pWpa2IePacket->capabilities,ENDIAN_HANDLE_WORD(admCtrlWpa2_buildCapabilities(pAdmCtrl)));
    length += 2;
    /* build PMKID list: we support no more than 1 PMKSA per AP, */
    /* so no more than 1 PMKID can be sent in the RSN IE         */
    if(pAdmCtrl->preAuthSupport && 
       (pAdmCtrl->pRsn->paeConfig.authProtocol == RSN_EXT_AUTH_MODE_WPA2))
    {
        /* Init value of PMKID count is 0 */
        SET_WLAN_WORD(&pWpa2IePacket->pmkIdCnt,ENDIAN_HANDLE_WORD(0));
        length += 2;
        status = ctrlData_getParamBssid(pAdmCtrl->pRsn->hCtrlData, CTRL_DATA_CURRENT_BSSID_PARAM, pBssid);
		MAC_COPY(assocBssid, pBssid);
        TRACE0(pAdmCtrl->hReport, REPORT_SEVERITY_INFORMATION, "admCtrlWpa2_getInfoElement - find PMKID \n");
        status = admCtrlWpa2_findPMKID(pAdmCtrl, &assocBssid, &pmkId, &index);
        if(status == TI_OK)
        {
            TRACE0(pAdmCtrl->hReport, REPORT_SEVERITY_INFORMATION, "admCtrlWpa2_getInfoElement - PMKID was found! \n");
            SET_WLAN_WORD(&pWpa2IePacket->pmkIdCnt,ENDIAN_HANDLE_WORD(1));
            os_memoryCopy(pAdmCtrl->hOs, (TI_UINT8 *)pWpa2IePacket->pmkId, 
                      (TI_UINT8 *)pmkId, PMKID_VALUE_SIZE);
            length += PMKID_VALUE_SIZE;
        }
    }   
    pWpa2IePacket->length = length;    /* RSN IE length without IEid and length field */
    *pLength              = length+2;  /* The whole length of the RSN IE */
    TRACE_INFO_HEX(pAdmCtrl->hReport, pIe, *pLength);
    return TI_OK;

}
/**
*
* admCtrlWpa2_setSite  - Set current primary site parameters for registration.
*
* \b Description: 
*
* Set current primary site parameters for registration.
*
* \b ARGS:
*
*  I   - pAdmCtrl - context \n
*  I   - pRsnData - site's RSN data \n
*  O   - pAssocIe - result IE of evaluation \n
*  O   - pAssocIeLen - length of result IE of evaluation \n
*  
* \b RETURNS:
*
*  TI_OK on site is aproved, TI_NOK on site is rejected.
*
* \sa 
*/
TI_STATUS admCtrlWpa2_setSite(admCtrl_t *pAdmCtrl, TRsnData *pRsnData, TI_UINT8 *pAssocIe, TI_UINT8 *pAssocIeLen)
{
    TI_STATUS               status;
    paramInfo_t             *pParam;
    TTwdParamInfo           tTwdParam;
    wpa2IeData_t            wpa2Data;
    TRsnPaeConfig           paeConfig;
    TI_UINT8                *pWpa2Ie;
    ECipherSuite            uSuite, bSuite;

    *pAssocIeLen = 0;

    if (pRsnData==NULL)
    {
        return TI_NOK;
    }

    pParam = (paramInfo_t *)os_memoryAlloc(pAdmCtrl->hOs, sizeof(paramInfo_t));
    if (!pParam)
    {
        return TI_NOK;
    }

    if (pRsnData->pIe==NULL)
    {
        /* configure the MLME module with the 802.11 OPEN authentication suite, 
            THe MLME will configure later the authentication module */
        pParam->paramType = MLME_LEGACY_TYPE_PARAM;
        pParam->content.mlmeLegacyAuthType = AUTH_LEGACY_OPEN_SYSTEM;
        status = mlme_setParam(pAdmCtrl->hMlme, pParam);
        goto adm_ctrl_wpa2_end;
    }

#ifdef XCC_MODULE_INCLUDED
    /* Clean MIC and KP flags in the HAL.                */
    /* It is needed if the previous privacy mode was XCC */
    tTwdParam.paramType = TWD_RSN_XCC_SW_ENC_ENABLE_PARAM_ID; 
    tTwdParam.content.rsnXCCSwEncFlag = TI_FALSE;
    status = TWD_SetParam (pAdmCtrl->pRsn->hTWD, &tTwdParam);

    tTwdParam.paramType = TWD_RSN_XCC_MIC_FIELD_ENABLE_PARAM_ID; 
    tTwdParam.content.rsnXCCMicFieldFlag = TI_FALSE;
    status = TWD_SetParam (pAdmCtrl->pRsn->hTWD, &tTwdParam);

    /* Check if Aironet IE exists */
    admCtrlXCC_setExtendedParams(pAdmCtrl, pRsnData);

#endif /*XCC_MODULE_INCLUDED*/
    
    status = admCtrl_parseIe(pAdmCtrl, pRsnData, &pWpa2Ie, RSN_IE_ID);
    if (status != TI_OK)                                                         
    {
        goto adm_ctrl_wpa2_end;
    }
    TRACE0(pAdmCtrl->hReport, REPORT_SEVERITY_INFORMATION, "admCtrlWpa2_setSite: RSN_IE=\n");
    TRACE_INFO_HEX(pAdmCtrl->hReport, pRsnData->pIe, pRsnData->ieLen);
    status = admCtrlWpa2_parseIe(pAdmCtrl, pWpa2Ie, &wpa2Data);
    if (status != TI_OK)
    {
        goto adm_ctrl_wpa2_end;
    }
    if ((wpa2Data.unicastSuite[0]>=MAX_WPA2_CIPHER_SUITE) ||
        (wpa2Data.broadcastSuite>=MAX_WPA2_CIPHER_SUITE) ||
        (pAdmCtrl->unicastSuite>=MAX_WPA2_CIPHER_SUITE))
    {
        status = TI_NOK;
        goto adm_ctrl_wpa2_end;
    }
    /* Check validity of Group suite */
    if (!broadcastCipherSuiteValidity[pAdmCtrl->networkMode][wpa2Data.broadcastSuite])
    {   /* check Group suite validity */                                          
        status = TI_NOK;
        goto adm_ctrl_wpa2_end;
    }

    status = admCtrlWpa2_getCipherSuiteMetric (pAdmCtrl, &wpa2Data, NULL, &uSuite, &bSuite);
    if (status != TI_OK)
        goto adm_ctrl_wpa2_end;

    /* set replay counter */
    pAdmCtrl->replayCnt = wpa2Data.ptkReplayCounters;

    *pAssocIeLen = pRsnData->ieLen;
    if (pAssocIe != NULL)
    {
        os_memoryCopy(pAdmCtrl->hOs, pAssocIe, &wpa2Data, sizeof(wpa2IeData_t));
    }

    /* re-config PAE with updated unicast and broadcast suite values            */
    /* If STA works in WpaMixed mode/AnyWpa mode, set PAE auth. mode to WPA2    */
    paeConfig.authProtocol = pAdmCtrl->externalAuthMode;

    if(pAdmCtrl->WPAPromoteFlags)
    {
       if(pAdmCtrl->externalAuthMode == RSN_EXT_AUTH_MODE_WPA)
          paeConfig.authProtocol   = RSN_EXT_AUTH_MODE_WPA2;
       if(pAdmCtrl->externalAuthMode == RSN_EXT_AUTH_MODE_WPAPSK)
          paeConfig.authProtocol   = RSN_EXT_AUTH_MODE_WPA2PSK;
    }

#ifdef XCC_MODULE_INCLUDED
    pParam->paramType = XCC_CCKM_EXISTS;
    pParam->content.XCCCckmExists = (wpa2Data.KeyMngSuite[0]==WPA2_IE_KEY_MNG_CCKM) ? TI_TRUE : TI_FALSE;
    XCCMngr_setParam(pAdmCtrl->hXCCMngr, pParam);
#endif

    paeConfig.keyExchangeProtocol = pAdmCtrl->keyMngSuite;
    paeConfig.unicastSuite        = uSuite;    /* Updated value */
    paeConfig.broadcastSuite      = bSuite;    /* Updated value */
    status = admCtrlWpa2_DynamicConfig(pAdmCtrl, &paeConfig);

    if (status != TI_OK)
    {
        goto adm_ctrl_wpa2_end;
    }

    /* Now we configure the MLME module with the 802.11 legacy authentication suite, 
        THe MLME will configure later the authentication module */
    pParam->paramType = MLME_LEGACY_TYPE_PARAM;
#ifdef XCC_MODULE_INCLUDED
    if (pAdmCtrl->networkEapMode!=OS_XCC_NETWORK_EAP_OFF)
    {
        pParam->content.mlmeLegacyAuthType = AUTH_LEGACY_RESERVED1;
    }
    else
#endif
    {
        pParam->content.mlmeLegacyAuthType = AUTH_LEGACY_OPEN_SYSTEM;
    }
    status = mlme_setParam(pAdmCtrl->hMlme, pParam);
    if (status != TI_OK)
    {
        goto adm_ctrl_wpa2_end;
    }

    pParam->paramType = RX_DATA_EAPOL_DESTINATION_PARAM;
    pParam->content.rxDataEapolDestination = OS_ABS_LAYER;
    status = rxData_setParam(pAdmCtrl->hRx, pParam);
    if (status != TI_OK)
    {
        goto adm_ctrl_wpa2_end;
    }

    /* Configure privacy status in HAL so that HW is prepared to recieve keys */
    tTwdParam.paramType = TWD_RSN_SECURITY_MODE_PARAM_ID;
    tTwdParam.content.rsnEncryptionStatus = (ECipherSuite)paeConfig.unicastSuite;
    status = TWD_SetParam(pAdmCtrl->pRsn->hTWD, &tTwdParam);
adm_ctrl_wpa2_end:
    os_memoryFree(pAdmCtrl->hOs, pParam, sizeof(paramInfo_t));
    return status;
}

/**
*
* admCtrlWpa_evalSite  - Evaluate site for registration.
*
* \b Description: 
*
* evaluate site RSN capabilities against the station's cap.
* If the BSS type is infrastructure, the station matches the site only if it's WEP status is same as the site
* In IBSS, it does not matter
*
* \b ARGS:
*
*  I   - pAdmCtrl - Context \n
*  I   - pRsnData - site's RSN data \n
*  O   - pEvaluation - Result of evaluation \n
*  
* \b RETURNS:
*
*  TI_OK 
*
* \sa 
*/
TI_STATUS admCtrlWpa2_evalSite(admCtrl_t *pAdmCtrl, TRsnData *pRsnData, TRsnSiteParams *pRsnSiteParams, TI_UINT32 *pEvaluation)
{
    TI_STATUS               status;
    wpa2IeData_t            wpa2Data;
    TI_UINT8                *pWpa2Ie;
    ECipherSuite            uSuite, bSuite; 
    TI_UINT8                i = 0;
    TIWLN_SIMPLE_CONFIG_MODE  wscMode = TIWLN_SIMPLE_CONFIG_OFF;

    *pEvaluation = 0;

    if (pRsnData==NULL)
    {
        return TI_NOK;
    }
    if (pRsnData->pIe==NULL)
    {
        return TI_NOK;
    }
    
    if (pRsnSiteParams->bssType != BSS_INFRASTRUCTURE)
    {
        return TI_NOK;
    }

    /* Get Simple-Config state */
    siteMgr_getParamWSC(pAdmCtrl->pRsn->hSiteMgr, &wscMode); /* SITE_MGR_SIMPLE_CONFIG_MODE */
    status = admCtrl_parseIe(pAdmCtrl, pRsnData, &pWpa2Ie, RSN_IE_ID);
    if (status != TI_OK)                                                         
    {                                                                                    
        return status;                                                        
    }
    TRACE0(pAdmCtrl->hReport, REPORT_SEVERITY_INFORMATION, "admCtrlWpa2_evalSite, IE=\n");

    TRACE_INFO_HEX(pAdmCtrl->hReport, pRsnData->pIe, pRsnData->ieLen);

    status = admCtrlWpa2_parseIe(pAdmCtrl, pWpa2Ie, &wpa2Data);
    if (status != TI_OK)
    {
        return status;
    }

	/* check keyMngSuite validity */
    status = TI_NOK;
    for(i = 0; 
       (i < wpa2Data.KeyMngSuiteCnt) &&(i<MAX_WPA2_KEY_MNG_SUITES)&& (status != TI_OK);
        i++)
    {
    	switch (wpa2Data.KeyMngSuite[i])
       	{
          	case WPA2_IE_KEY_MNG_NONE:
            	status = (pAdmCtrl->externalAuthMode <= RSN_EXT_AUTH_MODE_AUTO_SWITCH) ? TI_OK : TI_NOK;
              	break;
          	case WPA2_IE_KEY_MNG_801_1X:
#ifdef XCC_MODULE_INCLUDED
			/* CCKM is allowed only in 802.1x auth */
          	case WPA2_IE_KEY_MNG_CCKM:
#endif

            	if(!pAdmCtrl->WPAPromoteFlags)
               		status = (pAdmCtrl->externalAuthMode == RSN_EXT_AUTH_MODE_WPA2) ? TI_OK : TI_NOK;
              	else
                 	/* Any-WPA mode is supported */
                 	status = ((pAdmCtrl->externalAuthMode == RSN_EXT_AUTH_MODE_WPA2) ||
                        (pAdmCtrl->externalAuthMode == RSN_EXT_AUTH_MODE_WPA)) ? TI_OK : TI_NOK;
              	break;
          	case WPA2_IE_KEY_MNG_PSK_801_1X:
				if(!pAdmCtrl->WPAPromoteFlags)
                	status = (pAdmCtrl->externalAuthMode == RSN_EXT_AUTH_MODE_WPA2PSK) ? TI_OK : TI_NOK;
             	else
             		/* Any-WPA mode is supported */
                	status = ((pAdmCtrl->externalAuthMode == RSN_EXT_AUTH_MODE_WPA2PSK) ||
                				(wscMode && (pAdmCtrl->externalAuthMode == RSN_EXT_AUTH_MODE_WPA)) ||
                    			(pAdmCtrl->externalAuthMode == RSN_EXT_AUTH_MODE_WPAPSK)) ? TI_OK : TI_NOK;					

				if ((status == TI_NOK) && (wpa2Data.KeyMngSuiteCnt > 1) && (wpa2Data.KeyMngSuite[1] == WPA2_IE_KEY_MNG_801_1X) && (pAdmCtrl->externalAuthMode == RSN_EXT_AUTH_MODE_WPA2))
                {
                TRACE0(pAdmCtrl->hReport, REPORT_SEVERITY_INFORMATION, "Overriding AKM suite evaluation for simple-config\n");
                    status = TI_OK;
				}
				break;
       		default:
           TRACE1(pAdmCtrl->hReport, REPORT_SEVERITY_ERROR, "admCtrlWpa2_evalSite, default, wpa2Data.KeyMngSuite[i]=%d \n",wpa2Data.KeyMngSuite[i]);
             	status = TI_NOK;
             	break;
       	}
    }

    if (status != TI_OK)
    {
        TRACE3(pAdmCtrl->hReport, REPORT_SEVERITY_ERROR, "admCtrlWpa2_evalSite, status=%d, externalAuthMode=%d, WPAPromoteFlags=%d \n", status, pAdmCtrl->externalAuthMode, pAdmCtrl->WPAPromoteFlags);
        return status;
    }

    /* Check cipher suite validity */
    if(admCtrlWpa2_getCipherSuiteMetric(pAdmCtrl, &wpa2Data, pEvaluation, &uSuite, &bSuite) != TI_OK)
        return TI_NOK;

    /* Check privacy bit if not in mixed mode */
    if (!pAdmCtrl->mixedMode)
    {   /* There's no mixed mode, so make sure that the privacy Bit matches the privacy mode*/
        if (((pRsnData->privacy) && (uSuite == TWD_CIPHER_NONE)) ||
            ((!pRsnData->privacy) && (uSuite > TWD_CIPHER_NONE)))
        {
            *pEvaluation = 0;
            TRACE2(pAdmCtrl->hReport, REPORT_SEVERITY_INFORMATION, "admCtrlWpa2_evalSite, mixedMode is TI_FALSE, privacy=%d, uSuite=%d\n", pRsnData->privacy, uSuite);
            return TI_NOK;
        }
    }

    /* always return TI_OK */
    return TI_OK;
}


/**
*
* admCtrlWpa2_parseIe  - Parse an WPA information element.
*
* \b Description: 
*
* Parse an WPA information element. 
* Builds a structure of the unicast adn broadcast cihper suites,
* the key management suite and the capabilities.
*
* \b ARGS:
*
*  I   - pAdmCtrl - pointer to admCtrl context
*  I   - pWpa2Ie  - pointer to WPA IE (RSN IE) buffer  \n
*  O   - pWpa2Data - WPA2 IE (RSN IE) structure after parsing
*  
*  
* \b RETURNS:
*
* TI_OK on success, TI_NOK on failure. 
*
* \sa 
*/
TI_STATUS admCtrlWpa2_parseIe(admCtrl_t *pAdmCtrl, TI_UINT8 *pWpa2Ie, wpa2IeData_t *pWpa2Data)
{
    dot11_RSN_t      *wpa2Ie       =  (dot11_RSN_t *)pWpa2Ie;
    TI_UINT16            temp2bytes =0, capabilities;
    TI_UINT8             dataOffset = 0, i = 0, j = 0, curKeyMngSuite = 0;
    ECipherSuite     curCipherSuite = TWD_CIPHER_NONE;

    TRACE0(pAdmCtrl->hReport, REPORT_SEVERITY_INFORMATION, "Wpa2_IE: DEBUG: admCtrlWpa2_parseIe\n\n");

    if ((pWpa2Data == NULL) || (pWpa2Ie == NULL))
    {
        return TI_NOK;
    }

    COPY_WLAN_WORD(&temp2bytes, wpa2Ie->rsnIeData);
    dataOffset += 2;

    /* Check the header fields and the version */
    if((wpa2Ie->hdr[0] != RSN_IE_ID) || (wpa2Ie->hdr[1] < WPA2_IE_MIN_LENGTH) ||
       (temp2bytes != WPA2_OUI_MAX_VERSION))
    {
        TRACE3(pAdmCtrl->hReport, REPORT_SEVERITY_ERROR, "Wpa2_ParseIe Error: length=0x%x, elementid=0x%x, version=0x%x\n", wpa2Ie->hdr[1], wpa2Ie->hdr[0], temp2bytes);

        return TI_NOK; 
    }


    /* Set default values */
    os_memoryZero(pAdmCtrl->hOs, pWpa2Data, sizeof(wpa2IeData_t));

    pWpa2Data->broadcastSuite = TWD_CIPHER_AES_CCMP;
    pWpa2Data->unicastSuiteCnt = 1;
    pWpa2Data->unicastSuite[0] = TWD_CIPHER_AES_CCMP;
    pWpa2Data->KeyMngSuiteCnt = 1;
    pWpa2Data->KeyMngSuite[0] = WPA2_IE_KEY_MNG_801_1X;

    /* If we've reached the end of the received RSN IE */
    if(wpa2Ie->hdr[1] < WPA2_IE_GROUP_SUITE_LENGTH)
        return TI_OK;
    
    /* Processing of Group Suite field - 4 bytes*/
    pWpa2Data->broadcastSuite = (ECipherSuite)admCtrlWpa2_parseSuiteVal(pAdmCtrl, (TI_UINT8 *)wpa2Ie->rsnIeData + dataOffset,
                                                          TWD_CIPHER_WEP104, TWD_CIPHER_UNKNOWN);
    dataOffset +=4;
    TRACE1(pAdmCtrl->hReport, REPORT_SEVERITY_INFORMATION, "Wpa2_IE: GroupSuite %x \n", pWpa2Data->broadcastSuite);


    /* Processing of Pairwise (Unicast) Cipher Suite - 2 bytes counter and list of 4-byte entries */
    if(wpa2Ie->hdr[1] < WPA2_IE_MIN_PAIRWISE_SUITE_LENGTH)
        return TI_OK;

    COPY_WLAN_WORD(&pWpa2Data->unicastSuiteCnt, wpa2Ie->rsnIeData + dataOffset);
    dataOffset += 2;

    if(pWpa2Data->unicastSuiteCnt > UNICAST_CIPHER_MAXNO_IN_RSNIE)
    {
        /* something wrong in the RSN IE */
        TRACE1(pAdmCtrl->hReport, REPORT_SEVERITY_ERROR, "Wpa2_ParseIe Error: Pairwise cipher suite count is  %d \n", pWpa2Data->unicastSuiteCnt);
        return TI_NOK;
    }

    /* Get unicast cipher suites */
    for(i = 0; i < pWpa2Data->unicastSuiteCnt; i++)
    {
        curCipherSuite = (ECipherSuite)admCtrlWpa2_parseSuiteVal(pAdmCtrl, (TI_UINT8 *)wpa2Ie->rsnIeData + dataOffset, 
                                                   TWD_CIPHER_WEP104, TWD_CIPHER_UNKNOWN);
        if(curCipherSuite == TWD_CIPHER_NONE)
            curCipherSuite = pWpa2Data->broadcastSuite;

        pWpa2Data->unicastSuite[i] = curCipherSuite;
        dataOffset +=4;

        TRACE1(pAdmCtrl->hReport, REPORT_SEVERITY_INFORMATION, "Wpa_IE: unicast suite %x \n", curCipherSuite);
    }

    /* Sort all the unicast suites supported by the AP in the decreasing order */
    /* (so the best cipher suite will be the first)                            */
    if(pWpa2Data->unicastSuiteCnt > 1)
    {
       for(i = 0; i < (pWpa2Data->unicastSuiteCnt -1); i ++)
       {
           for(j = 0; j < i; j ++)
           {
               if(pWpa2Data->unicastSuite[j] > pWpa2Data->unicastSuite[j + 1])
               {
                   curCipherSuite               = pWpa2Data->unicastSuite[j];
                   pWpa2Data->unicastSuite[j]   = pWpa2Data->unicastSuite[j+1];
                   pWpa2Data->unicastSuite[j+1] = curCipherSuite;
               }
           }
       }
    }

    /* If we've reached the end of the received RSN IE */
    if (wpa2Ie->hdr[1] == dataOffset)
        return TI_OK;

     /* KeyMng Suite */
    COPY_WLAN_WORD(&(pWpa2Data->KeyMngSuiteCnt), wpa2Ie->rsnIeData + dataOffset);

     dataOffset += 2;
     pAdmCtrl->wpaAkmExists = TI_FALSE;
     for(i = 0; i < pWpa2Data->KeyMngSuiteCnt; i++)
     {
#ifdef XCC_MODULE_INCLUDED
            curKeyMngSuite = admCtrlXCC_parseCckmSuiteVal4Wpa2(pAdmCtrl, (TI_UINT8 *)(wpa2Ie->rsnIeData + dataOffset));
            if (curKeyMngSuite == WPA2_IE_KEY_MNG_CCKM)
            {  /* CCKM is the maximum AKM */
                pWpa2Data->KeyMngSuite[i] = curKeyMngSuite;
            }
            else
#endif
            {
                curKeyMngSuite = admCtrlWpa2_parseSuiteVal(pAdmCtrl, (TI_UINT8 *)wpa2Ie->rsnIeData + dataOffset, 
                            WPA2_IE_KEY_MNG_PSK_801_1X, WPA2_IE_KEY_MNG_NA);
            }


        TRACE1(pAdmCtrl->hReport, REPORT_SEVERITY_INFORMATION, "Wpa2_IE: authKeyMng %x  \n", curKeyMngSuite);

         if ((curKeyMngSuite != WPA2_IE_KEY_MNG_NA) && 
             (curKeyMngSuite != WPA2_IE_KEY_MNG_CCKM))
         {
             pWpa2Data->KeyMngSuite[i] = curKeyMngSuite;
         }

         if (curKeyMngSuite==WPA2_IE_KEY_MNG_801_1X)
         {   /* If 2 AKM exist, save also the second priority */
             pAdmCtrl->wpaAkmExists = TI_TRUE;
         }

         dataOffset += 4;

		 /* Include all AP key management supported suites in the wpaData structure */
            pWpa2Data->KeyMngSuite[i+1] = curKeyMngSuite;
     }

    /* If we've reached the end of the received RSN IE */
    if (wpa2Ie->hdr[1] == dataOffset)
        return TI_OK;

    /* Parse capabilities */
    COPY_WLAN_WORD(&capabilities, wpa2Ie->rsnIeData + dataOffset);
    pWpa2Data->bcastForUnicatst  = (TI_UINT8)(capabilities & WPA2_GROUP_4_UNICAST_CAPABILITY_MASK)>> 
                                           WPA2_GROUP_4_UNICAST_CAPABILITY_SHIFT;
    pWpa2Data->ptkReplayCounters = (TI_UINT8)(capabilities &  WPA2_PTK_REPLAY_COUNTERS_CAPABILITY_MASK)>> 
                                           WPA2_PTK_REPLAY_COUNTERS_CAPABILITY_SHIFT;

    switch (pWpa2Data->ptkReplayCounters)
    {
    case 0: pWpa2Data->ptkReplayCounters=1;
            break;
    case 1: pWpa2Data->ptkReplayCounters=2;
            break;
    case 2: pWpa2Data->ptkReplayCounters=4;
            break;
    case 3: pWpa2Data->ptkReplayCounters=16;
            break;
    default: pWpa2Data->ptkReplayCounters=1;
            break;
   }
   pWpa2Data->gtkReplayCounters = (TI_UINT8)(capabilities & 
                                        WPA2_GTK_REPLAY_COUNTERS_CAPABILITY_MASK) >> 
                                        WPA2_GTK_REPLAY_COUNTERS_CAPABILITY_SHIFT;
   switch (pWpa2Data->gtkReplayCounters)
   {
   case 0: pWpa2Data->gtkReplayCounters=1;
            break;
   case 1: pWpa2Data->gtkReplayCounters=2;
            break;
   case 2: pWpa2Data->gtkReplayCounters=4;
            break;
   case 3: pWpa2Data->gtkReplayCounters=16;
            break;
   default: pWpa2Data->gtkReplayCounters=1;
            break;
   }

   pWpa2Data->preAuthentication = (TI_UINT8)(capabilities & WPA2_PRE_AUTH_CAPABILITY_MASK);

   TRACE5(pAdmCtrl->hReport, REPORT_SEVERITY_INFORMATION, "Wpa2_IE: capabilities %x, preAuthentication = %x, bcastForUnicatst %x, ptk = %x, gtk = %x\n", capabilities, pWpa2Data->preAuthentication, pWpa2Data->bcastForUnicatst, pWpa2Data->ptkReplayCounters, pWpa2Data->gtkReplayCounters);

    return TI_OK;

}


TI_UINT16 admCtrlWpa2_buildCapabilities(admCtrl_t *pAdmCtrl)
{
   TI_UINT16 capabilities = 0;
   TI_UINT16 replayCnt;


   /* Bit 0 - Pre-authentication is set to 0             */
   /* when RSN IE is sent from a STA (in assoc request)  */

   /* Bit1: group key for unicast is set to 0*/

   /* Bits 2&3: PTKSA Replay counter; bits 4&5 GTKSA replay Counters */
   switch (pAdmCtrl->replayCnt)
   {
   case 1:  replayCnt=0;
       break;
   case 2:  replayCnt=1;
       break;
   case 4:  replayCnt=2;
       break;
   case 16: replayCnt=3;
       break;
   default: replayCnt=0;
       break;
   }

   capabilities |= replayCnt << WPA2_PTK_REPLAY_COUNTERS_CAPABILITY_SHIFT;
   capabilities |= replayCnt << WPA2_GTK_REPLAY_COUNTERS_CAPABILITY_SHIFT;

   return   capabilities;

}


TI_UINT32  admCtrlWpa2_parseSuiteVal(admCtrl_t *pAdmCtrl, TI_UINT8* suiteVal, TI_UINT32 maxVal, TI_UINT32 unknownVal)
{
    TI_UINT32  suite;

    if ((pAdmCtrl==NULL) || (suiteVal==NULL))
    {
        return TWD_CIPHER_UNKNOWN;
    }
    if (!os_memoryCompare(pAdmCtrl->hOs, suiteVal, wpa2IeOuiIe, 3))
    {
        suite =  (ECipherSuite)((suiteVal[3]<=maxVal) ? suiteVal[3] : unknownVal); 
    } else
    {
        suite = unknownVal;
    }
    return  suite;

}


TI_STATUS admCtrlWpa2_checkCipherSuiteValidity (ECipherSuite unicastSuite, ECipherSuite broadcastSuite, ECipherSuite encryptionStatus)
{
    ECipherSuite maxCipher;

    maxCipher = (unicastSuite>=broadcastSuite) ? unicastSuite : broadcastSuite ;
    if (maxCipher != encryptionStatus)
    {
        return TI_NOK;
    }
    if ((unicastSuite != TWD_CIPHER_NONE) && (broadcastSuite>unicastSuite))
    {
        return TI_NOK;
    }
    return TI_OK;
}

TI_STATUS admCtrlWpa2_getCipherSuiteMetric (admCtrl_t *pAdmCtrl, wpa2IeData_t *pWpa2Data, TI_UINT32 *metric, 
                                            ECipherSuite *uSuite, ECipherSuite  *bSuite)
{
   ECipherSuite   encryption   = TWD_CIPHER_NONE;
   ECipherSuite   unicastSuite = TWD_CIPHER_NONE, brdcstSuite = TWD_CIPHER_NONE;
   admCtrlWpa2_validity_t  admCtrlWpa2_validity;
   TI_UINT32     maxMetric = 0, index = 0;
   TI_STATUS  status = TI_NOK;

   /* Set admCtrlWpa2_validity initial values */
   admCtrlWpa2_validity = admCtrlWpa2_validityTable[TWD_CIPHER_NONE][TWD_CIPHER_NONE][TWD_CIPHER_NONE];

   /* Check validity of configured encryption (cipher) and validity of */
   /* promoted cipher (in case of AnyWPA (WPAmixed mode))              */
   pAdmCtrl->getCipherSuite(pAdmCtrl, &encryption);
   TRACE1(pAdmCtrl->hReport, REPORT_SEVERITY_INFORMATION , "admCtrlWpa2_getCipherSuiteMetric, encryption=%d\n", encryption);

   while(encryption != TWD_CIPHER_NONE) 
   {
      for (index=0; index<pWpa2Data->unicastSuiteCnt; index++)
      {
          admCtrlWpa2_validity = 
          admCtrlWpa2_validityTable[pWpa2Data->unicastSuite[index]][pWpa2Data->broadcastSuite][encryption];
          if (admCtrlWpa2_validity.status == TI_OK)
          {
              TRACE1(pAdmCtrl->hReport, REPORT_SEVERITY_INFORMATION , "admCtrlWpa2_getCipherSuiteMetric, break: validity.evaluation=%d\n", admCtrlWpa2_validity.evaluation);
              break;
          }
      }

      if ((admCtrlWpa2_validity.status == TI_OK) && (admCtrlWpa2_validity.evaluation > maxMetric))
      {
          TRACE2(pAdmCtrl->hReport, REPORT_SEVERITY_INFORMATION , "admCtrlWpa2_getCipherSuiteMetric, validity.evaluation=%d, maxMetric=%d\n", admCtrlWpa2_validity.evaluation, maxMetric);

          maxMetric       = admCtrlWpa2_validity.evaluation;
          status          = admCtrlWpa2_validity.status;
          unicastSuite    = admCtrlWpa2_validity.unicast;
          brdcstSuite     = admCtrlWpa2_validity.broadcast;
      }

      if((pAdmCtrl->WPAPromoteFlags & ADMCTRL_WPA_OPTION_ENABLE_PROMOTE_CIPHER) &&
         (encryption != TWD_CIPHER_AES_CCMP))
         encryption = TWD_CIPHER_AES_CCMP;
      else
         encryption = TWD_CIPHER_NONE;

    }  /* End of "while encryption" stmt */

   if(metric)
      *metric = maxMetric;

   if(uSuite)
      *uSuite = unicastSuite;

   if(bSuite)
      *bSuite = brdcstSuite;

    return status;
}


/**
*
* admCtrlWpa2_DynamicConfig  - Dynamic setting of WPA2 config parameters.
*
* \b Description: 
*
*   Sets  WPA2 callback procedures and PAE configuration parameters.
*   This procedure is similar to admCtrlWpa2_Config procedure.
*   The main difference is that admCtrlWpa2_Config sets the DEFAULT VALUES
*   of the configuration parameters and so it should be called during
*   initialization of the driver code or when Auth mode or Encryption status
*   parameters are beeing set.
*   admCtrlWpa2_DynamicConfig set the updated values of WPA2 configuration  
*   parameters which gets after negotiation with an AP. So the procedure 
*   should be called during setSite stage.
*   
* \b ARGS:
*
*  I   - pAdmCtrl    - pointer to admCtrl context
*  I   - pPaeConfig  - pointer to PAE structure
*
* \b RETURNS:
*
* TI_OK on success, TI_NOK on failure. 
*
* \sa 
*/

TI_STATUS admCtrlWpa2_DynamicConfig(admCtrl_t *pAdmCtrl, TRsnPaeConfig *pPaeConfig)
{
    TI_STATUS status = TI_OK;

    /* Set those WPA2 params and callback procedures used after setSite stage */
    pAdmCtrl->getInfoElement = admCtrlWpa2_getInfoElement;

    pAdmCtrl->getPmkidList      = admCtrlWpa2_getPMKIDList;
    pAdmCtrl->setPmkidList      = admCtrlWpa2_setPMKIDList;
    pAdmCtrl->resetPmkidList    = admCtrlWpa2_resetPMKIDCache;
    pAdmCtrl->getPreAuthStatus = admCtrlWpa2_getPreAuthStatus;
    pAdmCtrl->startPreAuth = admCtrlWpa2_startPreAuth;

    /* set key management suite */
    switch (pAdmCtrl->externalAuthMode)
    {
    case RSN_EXT_AUTH_MODE_WPA2:
    case RSN_EXT_AUTH_MODE_WPA2PSK:
        pAdmCtrl->keyMngSuite = RSN_KEY_MNG_802_1X;
        break;
    case RSN_EXT_AUTH_MODE_WPA:  /* It is any-WPA (WPA-mixed mode ) */
    case RSN_EXT_AUTH_MODE_WPAPSK:
        pAdmCtrl->keyMngSuite = RSN_KEY_MNG_802_1X;
        break;
    case RSN_EXT_AUTH_MODE_WPANONE:
        pAdmCtrl->keyMngSuite = RSN_KEY_MNG_NONE;
        /* Not supported */
    default:
        return TI_NOK;
    }

    /* Config PAE (if needed) */
    if(pPaeConfig)
       status = pAdmCtrl->pRsn->setPaeConfig(pAdmCtrl->pRsn, pPaeConfig);

    return status;
}




/**
*
* admCtrlWpa2_findPMKID 
*
* \b Description: 
*
* Retrieve an AP's PMKID (if exist)

* \b ARGS:
*
*  I   - pAdmCtrl - pointer to admCtrl context
*  I   - pBSSID   - pointer to AP's BSSID address 
*  O   - pmkID    - pointer to AP's PMKID (if it is NULL ptr, only
*                   cache index will be returned to the caller)
*  O   - cacheIndex  - index of the cache table entry containing the 
                       bssid
*  
* \b RETURNS:
*
* TI_OK on success, TI_NOK on failure. 
*
* \sa 
*/
TI_STATUS admCtrlWpa2_findPMKID (admCtrl_t * pAdmCtrl, TMacAddr *pBSSID, 
                                 pmkidValue_t *pPMKID, TI_UINT8  *cacheIndex)
{

    TI_UINT8           i     = 0;
    TI_BOOL            found = TI_FALSE;
    TMacAddr    entryMac;
    TI_STATUS       status = TI_NOK;

    while(!found && (i < ADMCTRL_PMKID_CACHE_SIZE) && 
                    (i <= pAdmCtrl->pmkid_cache.entriesNumber))
    {
		MAC_COPY (entryMac, pAdmCtrl->pmkid_cache.pmkidTbl[i].bssId);
        if (MAC_EQUAL (entryMac, *pBSSID))
        {
            found       = TI_TRUE;
            *cacheIndex = i;
            if(pPMKID)
            {
               os_memoryCopy(pAdmCtrl->hOs, (void*)pPMKID,
                             pAdmCtrl->pmkid_cache.pmkidTbl[i].pmkId, 
                             PMKID_VALUE_SIZE);
            }
        }
        i++;
    }

    if(found)
        status = TI_OK;

    return status;

}


/**
*
* admCtrlWpa2_getPMKIDList 
*
* \b Description: 
*
* Returns content of the PMKID cache 
*
* \b ARGS:
*
*  I   - pAdmCtrl        - pointer to admCtrl context
*  O   - pmkidList       - memory buffer where the procedure writes the PMKIDs
*                          Supplied by the caller procedure. .
*  
* \b RETURNS:
*
* TI_OK on success, TI_NOK on failure. 
*
* \sa 
*/
TI_STATUS admCtrlWpa2_getPMKIDList (admCtrl_t * pAdmCtrl,OS_802_11_PMKID *pmkidList)
{

    TI_UINT8   neededLength, i = 0;
    TI_UINT8   NumOfEntries = pAdmCtrl->pmkid_cache.entriesNumber;
    TI_UINT8   *bssid, *pmkid;

    if(!pAdmCtrl->preAuthSupport)
        return PARAM_NOT_SUPPORTED;

    /* Check the buffer length */
    if(NumOfEntries > 1)
       neededLength = 30 + ((NumOfEntries - 1) * (MAC_ADDR_LEN + PMKID_VALUE_SIZE));
    else
       neededLength = 30;

    if(neededLength > pmkidList->Length)
    {
        /* The buffer length is not enough */
        pmkidList->Length = neededLength;
        return TI_NOK;
    }

    /* The buffer is big enough. Fill the info */
    pmkidList->Length         = neededLength;
    pmkidList->BSSIDInfoCount = NumOfEntries;

    TRACE1(pAdmCtrl->hReport, REPORT_SEVERITY_INFORMATION, "RSN:  Get PMKID cache.  Number of entries  = %d \n", NumOfEntries);

    for (i = 0; i < NumOfEntries; i++ )
    {
        bssid = (TI_UINT8 *) pAdmCtrl->pmkid_cache.pmkidTbl[i].bssId;
        pmkid = (TI_UINT8 *)pAdmCtrl->pmkid_cache.pmkidTbl[i].pmkId;

        MAC_COPY(pmkidList->osBSSIDInfo[i].BSSID, bssid);

        os_memoryCopy(pAdmCtrl->hOs,
                      (void *)pmkidList->osBSSIDInfo[i].PMKID,
                      &pmkid, 
                      PMKID_VALUE_SIZE);

        TRACE22(pAdmCtrl->hReport, REPORT_SEVERITY_INFORMATION, "RSN:  BSSID:  %.2X-%.2X-%.2X-%.2X-%.2X-%.2X   PMKID: %.2X %.2X %.2X %.2X %.2X %.2X %.2X %.2X %.2X %.2X %.2X %.2X %.2X %.2X %.2X %.2X  \n", bssid[0], bssid[1], bssid[2], bssid[3], bssid[4], bssid[5], pmkid[0], pmkid[1], pmkid[2], pmkid[3], pmkid[4], pmkid[5], pmkid[6], pmkid[7], pmkid[8], pmkid[9], pmkid[10],pmkid[11], pmkid[12],pmkid[13],pmkid[14],pmkid[15]);
    }

    return TI_OK;

}

/**
*
* admCtrlWpa2_addPMKID 
*
* \b Description: 
*
* Add/Set an AP's PMKID received from the Supplicant 
*
* \b ARGS:
*
*  I   - pAdmCtrl - pointer to admCtrl context
*  I   - pBSSID   - pointer to AP's BSSID address 
*  I   - pmkID    - AP's PMKID
*  
* \b RETURNS:
*
* TI_OK on success, TI_NOK on failure. 
*
* \sa 
*/
TI_STATUS admCtrlWpa2_addPMKID (admCtrl_t * pAdmCtrl, TMacAddr *pBSSID, pmkidValue_t pmkID)
{
   TI_UINT8         cacheIndex;
   TI_STATUS     status = TI_NOK;

   /* Try to find the pBSSId in the PMKID cache */
   status = admCtrlWpa2_findPMKID (pAdmCtrl, pBSSID, NULL, &cacheIndex);

   if(status == TI_OK)
   {
       /* Entry for the bssid has been found; Update PMKID */
       os_memoryCopy(pAdmCtrl->hOs, 
                    (void*)&pAdmCtrl->pmkid_cache.pmkidTbl[cacheIndex].pmkId,
                    pmkID, PMKID_VALUE_SIZE);
       /*pAdmCtrl->pmkid_cache.pmkidTbl[cacheIndex].generationTs = os_timeStampMs(pAdmCtrl->hOs); */
   }
   else
   {
       /* The new entry is added to the next free entry. */
       /* Copy the new entry to the next free place.     */
       cacheIndex = pAdmCtrl->pmkid_cache.nextFreeEntry;
       MAC_COPY (pAdmCtrl->pmkid_cache.pmkidTbl[cacheIndex].bssId, *pBSSID);
       os_memoryCopy(pAdmCtrl->hOs, 
                     (void*)&pAdmCtrl->pmkid_cache.pmkidTbl[cacheIndex].pmkId,
                     (void*)pmkID, 
                     PMKID_VALUE_SIZE);

       /* Update the next free entry index. (If the table is full, a new entry */
       /* will override the oldest entries from the beginning of the table)    */
       /* Update the number of entries. (it cannot be more than max cach size) */
       pAdmCtrl->pmkid_cache.nextFreeEntry  = (cacheIndex + 1) % ADMCTRL_PMKID_CACHE_SIZE;

       if(pAdmCtrl->pmkid_cache.entriesNumber < ADMCTRL_PMKID_CACHE_SIZE)
          pAdmCtrl->pmkid_cache.entriesNumber ++;
   }

        TRACE1(pAdmCtrl->hReport, REPORT_SEVERITY_INFORMATION, "RSN   Add PMKID   Entry index is %d \n", cacheIndex);
        TRACE22(pAdmCtrl->hReport, REPORT_SEVERITY_INFORMATION, "RSN:  BSSID: %.2X-%.2X-%.2X-%.2X-%.2X-%.2X  PMKID: %.2X %.2X %.2X %.2X %.2X %.2X %.2X %.2X %.2X %.2X %.2X %.2X %.2X %.2X %.2X %.2X  \n", (*pBSSID)[0], (*pBSSID)[1], (*pBSSID)[2], (*pBSSID)[3], (*pBSSID)[4], (*pBSSID)[5], pmkID[0], pmkID[1], pmkID[2], pmkID[3], pmkID[4], pmkID[5], pmkID[6], pmkID[7], pmkID[8], pmkID[9], pmkID[10],pmkID[11], pmkID[12],pmkID[13],pmkID[14],pmkID[15]);



   return TI_OK;
}

/**
*
* admCtrlWpa2_setPMKIDList 
*
* \b Description: 
*
* Set PMKID cache 
*
* \b ARGS:
*
*  I   - pAdmCtrl        - pointer to admCtrl context
*  O   - pmkidList       - memory buffer where the procedure reads the PMKIDs from
*                          Supplied by the caller procedure. 
* \b RETURNS:
*
* TI_OK on success, TI_NOK on failure. 
*
* \sa 
*/
TI_STATUS admCtrlWpa2_setPMKIDList (admCtrl_t * pAdmCtrl, OS_802_11_PMKID *pmkidList)
{
    TI_UINT8          neededLength, i = 0;
    TI_UINT8          NumOfEntries;
    TMacAddr   macAddr;

    /* Check the minimal buffer length */
    if (pmkidList->Length < 2*sizeof(TI_UINT32))
    {
        TRACE1(pAdmCtrl->hReport, REPORT_SEVERITY_INFORMATION, "RSN: Set PMKID list - Buffer size < min length (8 bytes). Supplied length is %d .\n", pmkidList->Length);
        return TI_NOK;
    }

    /* Check the num of entries in the buffer: if 0 it means that */
    /* PMKID cache has to be cleaned                              */
    if(pmkidList->BSSIDInfoCount == 0)
    {
        admCtrlWpa2_resetPMKIDCache(pAdmCtrl);
        return TI_OK;
    }

    /* Check the buffer length */
    NumOfEntries = (TI_UINT8)pmkidList->BSSIDInfoCount;
    neededLength =  2*sizeof(TI_UINT32) + (NumOfEntries  *(MAC_ADDR_LEN + PMKID_VALUE_SIZE));

    if(pmkidList->Length < neededLength)
    {
        /* Something wrong goes with the buffer */
        TRACE3(pAdmCtrl->hReport, REPORT_SEVERITY_INFORMATION, "RSN: Set PMKID list - no enough room for %d entries Needed length is %d. Supplied length is %d .\n", NumOfEntries, neededLength,pmkidList->Length);
        return TI_NOK;
    }

    /*  Write  the PMKID to the PMKID cashe */
    pmkidList->BSSIDInfoCount = NumOfEntries;
    for (i = 0; i < NumOfEntries; i++ )
    {
         MAC_COPY (macAddr, pmkidList->osBSSIDInfo[i].BSSID);

         TRACE0(pAdmCtrl->hReport, REPORT_SEVERITY_INFORMATION , "admCtrlWpa2_setPMKIDList: Received new pre-auth AP\n");
         if (pAdmCtrl->numberOfPreAuthCandidates)
         {
            pAdmCtrl->numberOfPreAuthCandidates--;
            if (pAdmCtrl->numberOfPreAuthCandidates == 0)
            {
               TRACE0(pAdmCtrl->hReport, REPORT_SEVERITY_INFORMATION , "Stopping the Pre-Auth timer since Pre-auth is finished\n");
               tmr_StopTimer (pAdmCtrl->hPreAuthTimerWpa2); 
               /* Send PRE-AUTH end event to External Application */
               admCtrl_notifyPreAuthStatus (pAdmCtrl, RSN_PRE_AUTH_END);
            }

            TRACE1(pAdmCtrl->hReport, REPORT_SEVERITY_INFORMATION , "admCtrlWpa2_setPMKIDList: %d APs left in candidate list\n",pAdmCtrl->numberOfPreAuthCandidates);

         }
        else
        {
           TRACE0(pAdmCtrl->hReport, REPORT_SEVERITY_WARNING , "admCtrlWpa2_setPMKIDList: number of candidates was already zero...\n");
        }
        admCtrlWpa2_addPMKID(pAdmCtrl,&macAddr, (TI_UINT8 *)pmkidList->osBSSIDInfo[i].PMKID);
    }

    return TI_OK;

}

/**
*
* admCtrlWpa2_resetPMKIDCache 
*
* \b Description: 
*
* Reset PMKID Table 
*
* \b ARGS:
*
*  I   - pAdmCtrl - pointer to admCtrl context
*  
* \b RETURNS:
*
* TI_OK on success, TI_NOK on failure. 
*
* \sa 
*/
TI_STATUS admCtrlWpa2_resetPMKIDCache (admCtrl_t *pAdmCtrl)
{

    TRACE1(pAdmCtrl->hReport, REPORT_SEVERITY_INFORMATION, "RSN:  Reset PMKID cache.  %d entries are deleted. \n", pAdmCtrl->pmkid_cache.entriesNumber);

   os_memoryZero(pAdmCtrl->hOs, (void*)&pAdmCtrl->pmkid_cache, sizeof(pmkid_cache_t));

   return TI_OK;
}


/**
*
* admCtrlWpa2_sendPMKIDCandidateListAfterDelay 
*
* \b Description: 
*
* New Candidate List of APs with the same SSID as the STA is connected to 
* is generated and sent after the delay to the supplicant 
* in order to retrieve the new PMKIDs for the APs.
*
* \b ARGS:
*  I   - pAdmCtrl - pointer to admCtrl context
*  
* \b RETURNS:
*
* TI_OK on success, TI_NOK on failure. 
*
* \sa 
*/

static void admCtrlWpa2_buildAndSendPMKIDCandList (TI_HANDLE hHandle, TBssidList4PreAuth *apList)
{

    admCtrl_t         *pAdmCtrl = (admCtrl_t *)hHandle;
    TI_UINT8          candIndex =0, apIndex = 0, size =0;
    paramInfo_t       *pParam;
    OS_802_11_PMKID_CANDIDATELIST  *pCandList;
    TI_UINT8           memBuff[PMKID_CAND_LIST_MEMBUFF_SIZE + sizeof(TI_UINT32)];
    dot11_RSN_t       *rsnIE = 0;
    wpa2IeData_t      wpa2Data;
    TI_STATUS         status = TI_NOK;

    pParam = (paramInfo_t *)os_memoryAlloc(pAdmCtrl->hOs, sizeof(paramInfo_t));
    if (!pParam)
    {
        return;
    }

    /* Get SSID that the STA is accociated with    */
    pParam->paramType = SME_DESIRED_SSID_ACT_PARAM;
    status          = sme_GetParam (pAdmCtrl->pRsn->hSmeSm, pParam);
    if(status != TI_OK) {
        os_memoryFree(pAdmCtrl->hOs, pParam, sizeof(paramInfo_t));
        return;
    }

    /* If the existing PMKID cache contains information for not relevant */
    /* ssid (i.e. ssid was changed), clean up the PMKID cache and update */
    /* the ssid in the PMKID cache */
    if ((pAdmCtrl->pmkid_cache.ssid.len != pParam->content.smeDesiredSSID.len) || 
         (os_memoryCompare(pAdmCtrl->hOs, (TI_UINT8 *)pAdmCtrl->pmkid_cache.ssid.str,
          (TI_UINT8 *)pParam->content.smeDesiredSSID.str,
                          pAdmCtrl->pmkid_cache.ssid.len) != 0))
    {
        admCtrlWpa2_resetPMKIDCache(pAdmCtrl);

        os_memoryCopy(pAdmCtrl->hOs, (void *)pAdmCtrl->pmkid_cache.ssid.str, 
                      (void *)pParam->content.smeDesiredSSID.str,
                      pParam->content.siteMgrCurrentSSID.len);
        pAdmCtrl->pmkid_cache.ssid.len = pParam->content.smeDesiredSSID.len;
    }

    /* Get list of APs of the SSID that the STA is associated with*/
    /*os_memoryZero(pAdmCtrl->hOs, (void*)&apList, sizeof(bssidListBySsid_t));
    status = siteMgr_GetApListBySsid (pAdmCtrl->pRsn->hSiteMgr, 
                                      &param.content.siteMgrCurrentSSID,
                                      &apList);
    */
    os_memoryFree(pAdmCtrl->hOs, pParam, sizeof(paramInfo_t));
    if((apList == NULL) || (apList->NumOfItems == 0))
        return;
        
    TRACE0(pAdmCtrl->hReport, REPORT_SEVERITY_INFORMATION, "admCtrlWpa2_buildAndSendPMKIDCandList - Entry \n");

    /* fill the PMKID candidate list */
    pCandList = (OS_802_11_PMKID_CANDIDATELIST *)(memBuff + sizeof(TI_UINT32));
    pCandList->Version = 1;
    for (apIndex=0; apIndex<pAdmCtrl->pmkid_cache.entriesNumber; apIndex++)
    {
        pAdmCtrl->pmkid_cache.pmkidTbl[apIndex].preAuthenticate = TI_FALSE;
    }

    /* Go over AP list and find APs supporting pre-authentication */
    for(apIndex = 0; apIndex < apList->NumOfItems; apIndex++)
    {
        TI_UINT8 *bssidMac, i = 0;

        status = TI_NOK;

        if (apList->bssidList[apIndex].pRsnIEs==NULL)
        {
            continue;
        }
        /* Check is there RSN IE in this site */
        rsnIE = 0;      
        while( !rsnIE && (i < MAX_RSN_IE))
        {
            if(apList->bssidList[apIndex].pRsnIEs[i].hdr[0] == RSN_IE_ID)
            {
                rsnIE  = &apList->bssidList[apIndex].pRsnIEs[i];
                status = TI_OK;
            }
            i ++;
        }
		if (rsnIE)
		{
			TRACE1(pAdmCtrl->hReport, REPORT_SEVERITY_INFORMATION, "admCtrlWpa2_buildAndSendPMKIDCandList - rsnIE-hdr.eleId = %x \n", rsnIE->hdr[0]);
		}

        if(status == TI_OK)
           status = admCtrlWpa2_parseIe(pAdmCtrl, (TI_UINT8 *)rsnIE, &wpa2Data);

        TRACE1(pAdmCtrl->hReport, REPORT_SEVERITY_INFORMATION, "admCtrlWpa2_buildAndSendPMKIDCandList - parseIe status = %d \n", status);
        if(status == TI_OK)
        {   
            TI_BOOL        preAuthStatus;
            TI_UINT8               cacheIndex;

            preAuthStatus = admCtrlWpa2_getPreAuthStatus(pAdmCtrl, &apList->bssidList[apIndex].bssId, &cacheIndex);

            TRACE1(pAdmCtrl->hReport, REPORT_SEVERITY_INFORMATION, "admCtrlWpa2_buildAndSendPMKIDCandList, preAuthStatus=%d \n", preAuthStatus);

            if (preAuthStatus)
            {
                pAdmCtrl->pmkid_cache.pmkidTbl[cacheIndex].preAuthenticate = TI_TRUE;
            }

            bssidMac = (TI_UINT8 *)apList->bssidList[apIndex].bssId;
            MAC_COPY (pCandList->CandidateList[candIndex].BSSID, bssidMac);
 
            if(pAdmCtrl->preAuthSupport && (wpa2Data.preAuthentication))
            {
               pCandList->CandidateList[candIndex].Flags = 
                                 OS_802_11_PMKID_CANDIDATE_PREAUTH_ENABLE;
            }
            else
            {
                pCandList->CandidateList[candIndex].Flags = 0; 

            }
 
            TRACE8(pAdmCtrl->hReport, REPORT_SEVERITY_INFORMATION, "RSN:  Candidate [%d] is   %.2X-%.2X-%.2X-%.2X-%.2X-%.2X , Flags=0x%x\n", candIndex, bssidMac[0], bssidMac[1], bssidMac[2], bssidMac[3], bssidMac[4], bssidMac[5], pCandList->CandidateList[candIndex].Flags);
 
            candIndex ++;
        }
        
    }
    /* Add candidates that have valid PMKID, but were not in the list */
    for (apIndex=0; apIndex<pAdmCtrl->pmkid_cache.entriesNumber; apIndex++)
    {
        if (!pAdmCtrl->pmkid_cache.pmkidTbl[apIndex].preAuthenticate)
        {
            MAC_COPY (pCandList->CandidateList[candIndex].BSSID,
                      pAdmCtrl->pmkid_cache.pmkidTbl[apIndex].bssId);
            pCandList->CandidateList[apIndex].Flags = 
                OS_802_11_PMKID_CANDIDATE_PREAUTH_ENABLE;
            candIndex++;
        }
    }


    pCandList->NumCandidates = candIndex;

    
    /* Send Status Media specific indication to OS */
    size = sizeof(OS_802_11_PMKID_CANDIDATELIST) + 
           (candIndex - 1) * sizeof(OS_802_11_PMKID_CANDIDATE) + sizeof(TI_UINT32);

     /* Fill type of indication */
    *(TI_UINT32*)memBuff = os802_11StatusType_PMKID_CandidateList;

    pCandList->NumCandidates = candIndex;

    /* Store the number of candidates sent - needed for pre-auth finish event */
    pAdmCtrl->numberOfPreAuthCandidates = candIndex;
    /* Start the pre-authentication finish event timer */
    /* If the pre-authentication process is not over by the time it expires - we send an event */
    TRACE1(pAdmCtrl->hReport, REPORT_SEVERITY_INFORMATION , "Starting PREAUTH timer (%d mSec)\n",pAdmCtrl->preAuthTimeout*candIndex);
    tmr_StartTimer (pAdmCtrl->hPreAuthTimerWpa2,
                    admCtrlWpa2_preAuthTimerExpire,
                    (TI_HANDLE)pAdmCtrl,
                    pAdmCtrl->preAuthTimeout * candIndex,
                    TI_FALSE);

    EvHandlerSendEvent(pAdmCtrl->hEvHandler, IPC_EVENT_MEDIA_SPECIFIC,
                        memBuff, size);

    /* Send PRE-AUTH start event to External Application */
    admCtrl_notifyPreAuthStatus (pAdmCtrl, RSN_PRE_AUTH_START);
    TRACE1(pAdmCtrl->hReport, REPORT_SEVERITY_INFORMATION, "RSN:  PMKID Candidate List with %d entries has been built and sent for ssid  \n", candIndex);
    return;
}

/**
*
* admCtrlWpa2_getPreAuthStatus 
*
* \b Description: 
*
* Returns the status of the Pre Auth for the BSSID. If the authentictaion mode
 * is not WPA2, then TI_FALSE will be returned.
 * For WPA2 mode, if PMKID exists fro the BSSID and its liftime is valid 
 * TI_TRUE will be returned.
 * Otherwise TI_FALSE.
* 
* 
*
* \b ARGS:
*  I   - pAdmCtrl - pointer to admCtrl context
 * I   - givenAP  - required BSSID
*  
* \b RETURNS:
*
* TI_OK on success, TI_NOK on failure. 
*
* \sa 
*/
static TI_BOOL admCtrlWpa2_getPreAuthStatus(admCtrl_t *pAdmCtrl, TMacAddr *givenAP, TI_UINT8  *cacheIndex)
{
    pmkidValue_t    PMKID;
    
    if (admCtrlWpa2_findPMKID (pAdmCtrl, givenAP, 
                                 &PMKID, cacheIndex)!=TI_OK)
    {
        return TI_FALSE;
    }
    return TI_TRUE;

}

static TI_STATUS admCtrlWpa2_startPreAuth(admCtrl_t *pAdmCtrl, TBssidList4PreAuth *pBssidList)
{

    admCtrlWpa2_buildAndSendPMKIDCandList (pAdmCtrl, pBssidList);
    return TI_OK;
}

static TI_STATUS admCtrlWpa2_get802_1x_AkmExists (admCtrl_t *pAdmCtrl, TI_BOOL *wpa_802_1x_AkmExists)
{
    *wpa_802_1x_AkmExists = pAdmCtrl->wpaAkmExists;
    return TI_OK;
}



/*-----------------------------------------------------------------------------
Routine Name: admCtrlWpa2_preAuthTimerExpire
Routine Description: updates the preAuthStatus
Arguments:
Return Value:
-----------------------------------------------------------------------------*/
void admCtrlWpa2_preAuthTimerExpire(TI_HANDLE hAdmCtrl, TI_BOOL bTwdInitOccured)
{
    admCtrl_t         *pAdmCtrl = (admCtrl_t *)hAdmCtrl;
    TRACE0(pAdmCtrl->hReport, REPORT_SEVERITY_WARNING , "admCtrlWpa2_preAuthTimerExpire: PREAUTH EXPIRED !!!!!!!!");
    /* Send PRE-AUTH end event to External Application */
    admCtrl_notifyPreAuthStatus (pAdmCtrl, RSN_PRE_AUTH_END);
    pAdmCtrl->numberOfPreAuthCandidates = 0;
   return;
}

